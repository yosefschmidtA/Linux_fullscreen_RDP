' Sessao Linux fullscreen multi-monitor (xrdp + xfwm4 dentro da WSL).
'
' Sobe o servidor RDP dentro da WSL e abre a sessao ocupando TODOS os
' monitores, cobrindo o Windows por completo - sem barra de tarefas.
'
' Ctrl+Alt+Break ....... alterna fullscreen <-> janela (volta pro Windows)
' Fechar a janela ...... a sessao Linux continua viva; reabrir retoma tudo
'                        exatamente onde estava.
'
' Copie ESTE arquivo E o "Linux Fullscreen.rdp" juntos para a Area de
' Trabalho. Sem o .rdp o script ainda conecta, so que com as opcoes padrao do
' mstsc (mais lentas).
'
' O .rdp NAO precisa mais ficar exatamente ao lado deste arquivo: desde
' 31/07/2026 o script tambem olha um nivel de subpasta. Arrumar a Area de
' Trabalho jogando os atalhos numa pasta "Tudo" e uma coisa normal de se fazer,
' e antes disso bastava fazer isso para o lancador cair no plano B em silencio
' - a sessao subia, so que sem os ajustes de fluidez, e nada avisava.

Set WshShell = CreateObject("WScript.Shell")
Set fso      = CreateObject("Scripting.FileSystemObject")

' 1) acorda a WSL e garante o xrdp no ar (oculto, espera terminar)
WshShell.Run "wsl.exe -d Ubuntu-24.04 -u root /usr/local/bin/linux-desktop-up", 0, True

' 2) abre a sessao. O .rdp ao lado deste script leva as opcoes afinadas para
'    uma conexao local: LAN, sem deteccao automatica de banda e sem compressao
'    do lado do cliente. Num loopback essas heuristicas so custam CPU - e CPU
'    e exatamente o gargalo aqui, porque nao ha GPU no caminho: o xrdp
'    comprime 4480x1080 por software a cada quadro.
'
'    O fullscreen multimonitor agora vem de dentro do .rdp
'    ("screen mode id:i:2" + "use multimon:i:1"), nao mais dos parametros
'    /f e /multimon da linha de comando.
'
'    Em fullscreen o mstsc entrega a tecla Windows para a sessao remota - e o
'    Super dos atalhos do xfwm4 (Super+setas, Super+D, Super+R). Isso foi
'    medido: tanto Win+Direita quanto Win+Esquerda chegam ao Linux, entao NAO
'    e preciso "keyboardhook:i:1" no .rdp.
'    Onde o .rdp e procurado: ao lado deste script e, se nao estiver la, em
'    cada subpasta um nivel abaixo. So um nivel - varrer arvore de Area de
'    Trabalho seria lento e nao resolveria mais caso nenhum.
base   = fso.GetParentFolderName(WScript.ScriptFullName)
perfil = base & "\Linux Fullscreen.rdp"

If Not fso.FileExists(perfil) Then
    perfil = ""
    On Error Resume Next
    For Each pasta In fso.GetFolder(base).SubFolders
        If perfil = "" And fso.FileExists(pasta.Path & "\Linux Fullscreen.rdp") Then
            perfil = pasta.Path & "\Linux Fullscreen.rdp"
        End If
    Next
    On Error GoTo 0
End If

If perfil <> "" Then
    WshShell.Run "mstsc.exe """ & perfil & """", 1, False
Else
    ' Plano B: sem o .rdp em lugar nenhum, conecta do jeito antigo.
    WshShell.Run "mstsc.exe /v:localhost:3390 /multimon /f", 1, False
End If
