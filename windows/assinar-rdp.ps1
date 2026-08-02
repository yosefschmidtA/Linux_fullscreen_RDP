# assinar-rdp.ps1 - tira o aviso "fornecedor desconhecido" do mstsc.
#
# O aviso aparece porque o .rdp nao tem assinatura digital e mesmo assim pede
# acesso a recursos locais (area de transferencia, WebAuthn). Assinado por um
# publicador confiavel, o mstsc conecta sem perguntar.
#
# Escopo de USUARIO de proposito: o certificado vai para os armazenamentos do
# seu perfil, nao os da maquina. Sem UAC, e a confianca nao vaza para outras
# contas do Windows. Se o aviso persistir, rode com -Maquina (como admin).
#
#   powershell -ExecutionPolicy Bypass -File assinar-rdp.ps1
#   powershell -ExecutionPolicy Bypass -File assinar-rdp.ps1 -Maquina
#
# RODE ISTO NUMA JANELA DO POWERSHELL DO WINDOWS. Chamado pelo interop da WSL
# (`powershell.exe` a partir de um terminal Linux) ele falha com
#
#   CertEnroll::CX509Enrollment::_CreateRequest: Tipo de provedor nao definido
#   0x80090017 (NTE_PROV_TYPE_NOT_DEF)
#
# porque o processo nasce sem o hive de criptografia do perfil - medido em
# 29/07/2026: o COM instancia e o RSA do .NET funciona, mas
# `Test-Path HKCU:\Software\Microsoft\Cryptography` responde False. Nada a ver
# com a maquina; e so o contexto. Ctrl+Alt+Break, abra o PowerShell e rode la.

param([switch]$Maquina, [string]$Perfil)

$ErrorActionPreference = 'Stop'

# Onde esta o "Linux Fullscreen.rdp".
#
# Ate 02/08/2026 era chumbado em "%USERPROFILE%\Desktop\Linux Fullscreen.rdp" -
# a mesma armadilha que quebrou o jogo-windows, o .vbs e o transferir-usb quando
# a Area de Trabalho foi arrumada numa subpasta. Aqui a falha era so barulhenta
# ("nao achei o perfil"), mas ela morde justo na hora ruim: assinar e o que se
# faz DEPOIS de editar o .rdp, porque editar invalida a assinatura.
#
# A ordem comeca pelo lado do proprio script ($PSScriptRoot): quem move o .rdp
# para uma pasta costuma levar este .ps1 junto.
function AcharPerfil {
    $nome = 'Linux Fullscreen.rdp'
    $raizes = @(
        (Join-Path $env:USERPROFILE 'Desktop'),
        (Join-Path $env:USERPROFILE 'OneDrive\Desktop'),
        (Join-Path $env:USERPROFILE 'OneDrive\Área de Trabalho')
    )
    foreach ($d in @($PSScriptRoot) + $raizes) {
        if ($d -and (Test-Path -LiteralPath (Join-Path $d $nome))) {
            return (Join-Path $d $nome)
        }
    }
    # um nivel de subpasta, como fazem os outros lancadores deste projeto
    foreach ($d in $raizes) {
        if (-not (Test-Path -LiteralPath $d)) { continue }
        $achado = Get-ChildItem -LiteralPath $d -Directory -ErrorAction SilentlyContinue |
                  ForEach-Object { Join-Path $_.FullName $nome } |
                  Where-Object { Test-Path -LiteralPath $_ } |
                  Select-Object -First 1
        if ($achado) { return $achado }
    }
    return $null
}

$assunto = 'CN=Linux Fullscreen (assinatura de .rdp)'
$rdp     = if ($Perfil) { $Perfil } else { AcharPerfil }
$dados   = Join-Path $env:LOCALAPPDATA 'linux-fullscreen'
$escopo  = if ($Maquina) { 'LocalMachine' } else { 'CurrentUser' }

if (-not $rdp -or -not (Test-Path -LiteralPath $rdp)) {
    throw "nao achei o ""Linux Fullscreen.rdp"". Procurei ao lado deste script, na Area de Trabalho e um nivel de subpasta abaixo. Use -Perfil <caminho> para dizer onde ele esta."
}
Write-Host "perfil               : $rdp"
New-Item -ItemType Directory -Force -Path $dados | Out-Null

# --- 1. o certificado (reaproveita se ja existir) --------------------------
# A chave privada fica sempre no SEU perfil, mesmo com -Maquina: o rdpsign
# precisa dela, e ele roda como voce.
$cert = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq $assunto } |
        Sort-Object NotAfter -Descending | Select-Object -First 1

if (-not $cert) {
    $cert = New-SelfSignedCertificate -Type Custom -Subject $assunto `
        -KeyUsage DigitalSignature -CertStoreLocation Cert:\CurrentUser\My `
        -KeyAlgorithm RSA -KeyLength 2048 `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3') `
        -NotAfter (Get-Date).AddYears(10)
    Write-Host "certificado criado   : $($cert.Thumbprint)"
} else {
    Write-Host "certificado ja existia: $($cert.Thumbprint)"
}

# --- 2. confiar nele ------------------------------------------------------
# Root: para a cadeia fechar (é autoassinado, entao ele é a propria raiz).
# TrustedPublisher: é o que o mstsc consulta para decidir se pergunta.
# So a parte PUBLICA vai para os armazenamentos - a chave privada nao sai
# do Cert:\CurrentUser\My.
$publico = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 `
             (,$cert.Export('Cert'))

foreach ($nome in 'Root', 'TrustedPublisher') {
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store($nome, $escopo)
    $store.Open('ReadWrite')
    if ($store.Certificates | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }) {
        Write-Host "ja confiavel em      : $escopo\$nome"
    } else {
        $store.Add($publico)
        Write-Host "instalado em         : $escopo\$nome"
    }
    $store.Close()
}

# --- 3. declarar o certificado como publicador confiavel de .rdp ----------
# Assinar so troca o aviso vermelho ("fornecedor desconhecido") pelo amarelo
# ("verificar o distribuidor"), agora COM a caixa "Lembrar minhas opcoes".
# A caixa grava em HKCU\...\Terminal Server Client\LocalDevices, com nome de
# GUID por identidade de conexao - e como o jogo-windows gera um perfil por
# partida, cada um vira uma identidade nova. Esta politica e o mecanismo
# desenhado para o caso: thumbprint na lista = nenhum aviso, sempre.
#
# Precisa de ELEVACAO: HKCU\Software\Policies tem ACL que nega escrita ao
# token comum do usuario. Sem admin o script segue e apenas avisa - o resto
# (assinatura) continua valendo.
$elevado = (New-Object Security.Principal.WindowsPrincipal(
              [Security.Principal.WindowsIdentity]::GetCurrent())
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($elevado) {
    $aplicou = $false
    foreach ($raiz in 'HKCU:', 'HKLM:') {
        $chave = "$raiz\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services"
        try {
            New-Item -Path $chave -Force -ErrorAction Stop | Out-Null
            # a politica aceita varios thumbprints separados por virgula
            $atual = (Get-ItemProperty -Path $chave -Name TrustedCertThumbprints `
                        -ErrorAction SilentlyContinue).TrustedCertThumbprints
            $lista = @($cert.Thumbprint)
            if ($atual) {
                $lista = @($atual -split ',' | Where-Object { $_ -and $_ -ne $cert.Thumbprint }) + $lista
            }
            Set-ItemProperty -Path $chave -Name TrustedCertThumbprints `
                -Value ($lista -join ',') -Type String -ErrorAction Stop
            Set-ItemProperty -Path $chave -Name AllowSignedFiles `
                -Value 1 -Type DWord -ErrorAction Stop
            Write-Host "politica aplicada em : $chave"
            $aplicou = $true
            break
        } catch {
            Write-Host "politica falhou em   : $chave ($($_.Exception.Message))"
        }
    }
    if (-not $aplicou) { Write-Warning 'nao consegui aplicar a politica de publicador confiavel' }
} else {
    Write-Host ''
    Write-Warning 'SEM ELEVACAO: o certificado foi instalado e o perfil sera assinado,'
    Write-Warning 'mas o aviso amarelo vai continuar aparecendo. Para calar de vez,'
    Write-Warning 'rode este script de novo num PowerShell como administrador.'
}

# --- 4. assinar o perfil --------------------------------------------------
& "$env:windir\System32\rdpsign.exe" /sha256 $cert.Thumbprint $rdp
if ($LASTEXITCODE -ne 0) { throw "rdpsign falhou (codigo $LASTEXITCODE)" }

# O jogo-windows gera um .rdp novo a cada partida (as coordenadas do monitor
# mudam), e isso invalida a assinatura. Ele le este arquivo para re-assinar.
Set-Content -Path (Join-Path $dados 'thumbprint.txt') -Value $cert.Thumbprint -NoNewline

Write-Host ''
Write-Host 'pronto. feche o mstsc e reconecte pelo .vbs para conferir.'
Write-Host "thumbprint guardado em: $dados\thumbprint.txt"
