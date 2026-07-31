# Troca o dispositivo de audio padrao do Windows, por linha de comando.
#
#   powershell -ExecutionPolicy Bypass -File audio-padrao.ps1 -Listar
#   powershell -ExecutionPolicy Bypass -File audio-padrao.ps1 -Evitar "G435"
#   powershell -ExecutionPolicy Bypass -File audio-padrao.ps1 -Restaurar
#
# POR QUE ISTO EXISTE
#
# O usbipd nao consegue anexar um dispositivo que o Windows esta usando ("Device
# busy (exported)"), e um headset esta sempre em uso enquanto for o dispositivo
# PADRAO do Windows - o servico de audio mantem o endpoint aberto. Medido em
# 30/07/2026: trocar a saida padrao a mao liberou o G435 e o attach passou.
#
# Este script faz essa troca sozinho, para o botao da barra nao depender de
# ninguem abrir as Configuracoes do Windows. Ver README, "Transferir audio".
#
# NAO PRECISA DE ADMINISTRADOR e NAO INSTALA NADA. O Windows nao tem comando
# nativo para isto, e as receitas correntes mandam instalar nircmd,
# SoundVolumeView ou o modulo AudioDeviceCmdlets. Todas elas chamam por baixo a
# mesma interface COM nao documentada, a IPolicyConfig - que da para instanciar
# direto, sem dependencia nenhuma. E o que este script faz.
#
# A lista de dispositivos vem do registro (HKLM, so leitura) em vez do
# IMMDeviceEnumerator: sao menos linhas de C# e o nome amigavel ja esta la.
[CmdletBinding()]
param(
    [switch] $Listar,
    [switch] $ListarUsb,
    [string] $Evitar,
    [switch] $Restaurar,
    [string] $Definir,
    [string] $DefinirId
)

$ErrorActionPreference = 'Stop'

$dirEstado  = Join-Path $env:LOCALAPPDATA 'linux-fullscreen'
$arqEvitado = Join-Path $dirEstado 'audio-evitado.txt'

# --- a interface COM ------------------------------------------------------
# A ORDEM DOS METODOS E O QUE IMPORTA: sao slots de vtable, nao nomes. Trocar
# duas linhas de lugar faz o SetDefaultEndpoint chamar outra funcao. Os nove
# primeiros existem so para empurrar o slot certo para a posicao 10.
if (-not ('IPolicyConfig' -as [type])) {
    Add-Type -Language CSharp @'
using System;
using System.Runtime.InteropServices;

[ComImport, Guid("f8679f50-850a-41cf-9c72-430f290290c8"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPolicyConfig
{
    [PreserveSig] int GetMixFormat(string id, IntPtr a);
    [PreserveSig] int GetDeviceFormat(string id, bool b, IntPtr a);
    [PreserveSig] int ResetDeviceFormat(string id);
    [PreserveSig] int SetDeviceFormat(string id, IntPtr a, IntPtr b);
    [PreserveSig] int GetProcessingPeriod(string id, bool b, IntPtr a, IntPtr c);
    [PreserveSig] int SetProcessingPeriod(string id, IntPtr a);
    [PreserveSig] int GetShareMode(string id, IntPtr a);
    [PreserveSig] int SetShareMode(string id, IntPtr a);
    [PreserveSig] int GetPropertyValue(string id, bool b, IntPtr k, IntPtr v);
    [PreserveSig] int SetPropertyValue(string id, bool b, IntPtr k, IntPtr v);
    [PreserveSig] int SetDefaultEndpoint(string id, int role);
    [PreserveSig] int SetEndpointVisibility(string id, bool visible);
}

[ComImport, Guid("870af99c-171d-4f9e-af0d-e63df40c2bc9")]
public class PolicyConfigClient { }

// Para LER qual e o padrao atual. O IPolicyConfig so escreve; quem responde
// "quem e o padrao agora" e o IMMDeviceEnumerator. Sem isto nao da para marcar
// o item ativo no menu de dispositivos da barra.
//
// Mesma regra da vtable: a ordem dos metodos e que importa. No IMMDevice o
// GetId e o TERCEIRO - os dois antes existem so para ocupar o slot.
[ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMMDevice
{
    [PreserveSig] int Activate(ref Guid iid, int ctx, IntPtr p, out IntPtr obj);
    [PreserveSig] int OpenPropertyStore(int acesso, out IntPtr loja);
    [PreserveSig] int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
    [PreserveSig] int GetState(out int estado);
}

[ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMMDeviceEnumerator
{
    [PreserveSig] int EnumAudioEndpoints(int fluxo, int mascara, out IntPtr col);
    [PreserveSig] int GetDefaultAudioEndpoint(int fluxo, int papel, out IMMDevice dev);
}

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
public class MMDeviceEnumerator { }

// A conversao para a interface fica AQUI, no C#, e nao no PowerShell.
// "[IPolicyConfig] (New-Object PolicyConfigClient)" falha com
// "Nao e possivel converter o valor PolicyConfigClient no tipo IPolicyConfig":
// o PowerShell nao faz QueryInterface em classe ComImport. Em C# o cast e um
// QueryInterface de verdade e funciona.
public static class AudioPadrao
{
    // fluxo: 0 = render (saida), 1 = capture (entrada). papel 1 = eMultimedia.
    public static string Atual(int fluxo)
    {
        try
        {
            IMMDeviceEnumerator en = (IMMDeviceEnumerator)(new MMDeviceEnumerator());
            IMMDevice dev;
            if (en.GetDefaultAudioEndpoint(fluxo, 1, out dev) != 0 || dev == null)
                return "";
            string id;
            if (dev.GetId(out id) != 0)
                return "";
            return id;
        }
        catch { return ""; }
    }

    public static void Definir(string id)
    {
        IPolicyConfig pc = (IPolicyConfig)(new PolicyConfigClient());
        // eConsole, eMultimedia, eCommunications - os tres, senao o Windows
        // continua usando o aparelho para alguma finalidade e ele nao se solta
        for (int papel = 0; papel < 3; papel++)
        {
            int hr = pc.SetDefaultEndpoint(id, papel);
            if (hr != 0)
                throw new Exception("SetDefaultEndpoint papel=" + papel + " hr=0x" + hr.ToString("X8"));
        }
    }
}
'@
}

# --- listar dispositivos do registro -------------------------------------
$RAIZ = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio'

# O nome que as Configuracoes do Windows mostram e a juncao de DUAS chaves:
#   "Alto-falantes" + " (" + "Realtek(R) Audio" + ")"
# A primeira e o nome do ENDPOINT, a segunda a do ADAPTADOR. Nenhuma sozinha
# basta - ha duas "Alto-falantes" distintas nesta maquina, e so o adaptador as
# diferencia. Cuidado com o GUID do adaptador: termina em bfc, nao bf8.
$K_ENDPOINT  = '{a45c254e-df1c-4efd-8020-67d146a850e0},2'
$K_ADAPTADOR = '{b3f8fa53-0004-438e-9003-51a46e139bfc},6'

# O vinculo com o HARDWARE, e a chave deste script. Guarda o caminho USB do
# aparelho, ex.: "{1}.USB\VID_046D&PID_0ADF&MI_01\6&1A27FB7A&0&0001".
#
# Por que nao casar por nome: o Windows NAO chama o headset de "G435". Ele
# aparece como "Fone de ouvido do headset (Tecnologia Intel Smart Sound para
# audio USB)" - generico e traduzido. Casar por nome falhou nas duas pontas
# (medido em 30/07/2026: o -Evitar nao excluia nada e o -Restaurar nao achava
# nada), alem de quebrar em Windows de outro idioma. O VID:PID e o mesmo
# identificador que o usbipd usa, entao as duas metades falam a mesma lingua.
$K_HARDWARE  = '{b3f8fa53-0004-438e-9003-51a46e139bfc},39'

# "046d:0adf" -> "VID_046D&PID_0ADF"
function ConvertTo-Padrao-Hw {
    param([string] $VidPid)
    $p = $VidPid.Split(':')
    if ($p.Count -ne 2) { throw "esperado VID:PID, recebi '$VidPid'" }
    return ('VID_' + $p[0] + '&PID_' + $p[1]).ToUpper()
}

function Get-Dispositivos {
    param([ValidateSet('Render','Capture')] [string] $Tipo)

    # o prefixo do id difere entre saida e entrada, e o SetDefaultEndpoint
    # espera o id COMPLETO nesse formato
    $prefixo = if ($Tipo -eq 'Render') { '{0.0.0.00000000}' } else { '{0.0.1.00000000}' }

    Get-ChildItem "$RAIZ\$Tipo" -ErrorAction SilentlyContinue | ForEach-Object {
        $guid = $_.PSChildName

        # DeviceState e BITFIELD, nao enum: aqui aparecem 4, 8, 0x20000004...
        # O bit 1 (ACTIVE) e o unico que interessa. Comparar com -eq 1 nao
        # casa com nada e a lista sai vazia.
        $estado = (Get-ItemProperty $_.PSPath -Name DeviceState `
                    -ErrorAction SilentlyContinue).DeviceState
        if (-not $estado -or ($estado -band 1) -ne 1) { return }

        $props = Get-ItemProperty (Join-Path $_.PSPath 'Properties') `
                    -ErrorAction SilentlyContinue
        if (-not $props) { return }

        $ep = $props.$K_ENDPOINT
        $ad = $props.$K_ADAPTADOR
        if (-not $ep -and -not $ad) { return }

        $nome = if ($ep -and $ad) { "$ep ($ad)" } elseif ($ep) { $ep } else { $ad }

        [pscustomobject]@{
            Tipo = $Tipo
            Nome = $nome
            Id   = "$prefixo.$guid"
            Hw   = "$($props.$K_HARDWARE)".ToUpper()
        }
    }
}

function Set-Padrao {
    param([string] $Id, [string] $Nome)

    [AudioPadrao]::Definir($Id)
    Write-Output "padrao -> $Nome"
}

# --- acoes ----------------------------------------------------------------
if ($Listar) {
    $padRender  = [AudioPadrao]::Atual(0)
    $padCapture = [AudioPadrao]::Atual(1)
    # "SAIDA|1|<id>|<nome>"  - o 1 marca o padrao atual. Formato com barras para
    # o lado Linux poder cortar sem depender de espacos no nome.
    Get-Dispositivos -Tipo Render | ForEach-Object {
        "SAIDA|{0}|{1}|{2}" -f $(if ($_.Id -eq $padRender) {1} else {0}), $_.Id, $_.Nome
    }
    Get-Dispositivos -Tipo Capture | ForEach-Object {
        "ENTRADA|{0}|{1}|{2}" -f $(if ($_.Id -eq $padCapture) {1} else {0}), $_.Id, $_.Nome
    }
    exit 0
}

# Só os dispositivos de audio que estao atras de um USB, um por VID:PID.
#
# E assim que o transferir-usb descobre sozinho QUAL headset esta nesta maquina:
# casa e trabalho tem headsets diferentes, e chumbar um VID:PID quebraria num
# deles. Um headset recem-ligado aparece primeiro no WINDOWS, que e exatamente
# o lado que esta varredura enxerga.
#
# Saida: "VID:PID<TAB>Nome", uma linha por aparelho.
if ($ListarUsb) {
    $vistos = @{}
    foreach ($t in 'Render', 'Capture') {
        foreach ($d in Get-Dispositivos -Tipo $t) {
            if ($d.Hw -match 'VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') {
                $id = ($Matches[1] + ':' + $Matches[2]).ToLower()
                if (-not $vistos.ContainsKey($id)) {
                    $vistos[$id] = $true
                    "$id`t$($d.Nome)"
                }
            }
        }
    }
    exit 0
}

# Pelo ID exato, e nao por pedaco de nome: o menu da barra ja sabe o ID, e ha
# duas "Alto-falantes (Realtek(R) Audio)" nesta maquina - por nome seria ambiguo.
if ($DefinirId) {
    $d = @(Get-Dispositivos -Tipo Render) + @(Get-Dispositivos -Tipo Capture) |
         Where-Object { $_.Id -eq $DefinirId } | Select-Object -First 1
    if (-not $d) { Write-Output "id nao encontrado: $DefinirId"; exit 1 }
    Set-Padrao -Id $d.Id -Nome $d.Nome
    exit 0
}

if ($Definir) {
    foreach ($t in 'Render', 'Capture') {
        $d = Get-Dispositivos -Tipo $t | Where-Object { $_.Nome -like "*$Definir*" } |
             Select-Object -First 1
        if ($d) { Set-Padrao -Id $d.Id -Nome "$($d.Nome) ($t)" }
    }
    exit 0
}

if ($Evitar) {
    # Escolhe, para saida E entrada, o primeiro dispositivo ativo que NAO seja o
    # aparelho a liberar. Faz os dois porque um headset ocupa os dois endpoints e
    # qualquer um deles sozinho mantem o aparelho preso pelo servico de audio.
    $hw = ConvertTo-Padrao-Hw $Evitar
    New-Item -ItemType Directory -Force -Path $dirEstado | Out-Null
    Set-Content -Path $arqEvitado -Value $Evitar -Encoding ASCII

    $trocou = $false
    foreach ($t in 'Render', 'Capture') {
        $todos = @(Get-Dispositivos -Tipo $t)
        $nosso = @($todos | Where-Object { $_.Hw -like "*$hw*" })
        if ($nosso.Count -eq 0) { continue }   # nao esta no Windows: nada a fazer

        $outro = $todos | Where-Object { $_.Hw -notlike "*$hw*" } | Select-Object -First 1
        if (-not $outro) {
            Write-Output "aviso: nao ha outro dispositivo de $t para assumir o padrao"
            continue
        }
        Set-Padrao -Id $outro.Id -Nome "$($outro.Nome) [$t]"
        $trocou = $true
    }
    if (-not $trocou) { Write-Output "nada a trocar (o aparelho nao era o padrao)" }
    exit 0
}

if ($Restaurar) {
    if (-not (Test-Path $arqEvitado)) {
        Write-Output "nada para restaurar"
        exit 0
    }
    $alvo = (Get-Content $arqEvitado -Raw).Trim()
    if (-not $alvo) { exit 0 }

    # Espera o aparelho reaparecer. Depois do 'usbipd detach' o Windows leva
    # alguns segundos para reenumerar o USB e marcar o endpoint como ativo -
    # medido em 30/07/2026: 2 s nao bastavam e o restore saia sem achar nada,
    # deixando o Windows com o padrao errado. A espera fica aqui, e nao no bash,
    # porque consultar o registro e barato e cada chamada de interop custa ~1 s.
    $hw = ConvertTo-Padrao-Hw $alvo
    $achou = $false
    foreach ($tentativa in 1..12) {
        foreach ($t in 'Render', 'Capture') {
            $d = Get-Dispositivos -Tipo $t | Where-Object { $_.Hw -like "*$hw*" } |
                 Select-Object -First 1
            if ($d) { Set-Padrao -Id $d.Id -Nome "$($d.Nome) [$t]"; $achou = $true }
        }
        if ($achou) { break }
        Start-Sleep -Milliseconds 900
    }
    if (-not $achou) {
        Write-Output "aviso: '$alvo' nao reapareceu no Windows em ~11s; padrao mantido"
    }
    exit 0
}

Write-Output "uso: -Listar | -Evitar <nome> | -Restaurar | -Definir <nome>"
exit 2
