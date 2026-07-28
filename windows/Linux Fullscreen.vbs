' Sessao Linux fullscreen multi-monitor (xrdp + xfwm4 dentro da WSL).
'
' Sobe o servidor RDP dentro da WSL e abre a sessao ocupando TODOS os
' monitores, cobrindo o Windows por completo - sem barra de tarefas.
'
' Ctrl+Alt+Break ....... alterna fullscreen <-> janela (volta pro Windows)
' Fechar a janela ...... a sessao Linux continua viva; reabrir retoma tudo
'                        exatamente onde estava.

Set WshShell = CreateObject("WScript.Shell")

' 1) acorda a WSL e garante o xrdp no ar (oculto, espera terminar)
WshShell.Run "wsl.exe -d Ubuntu-24.04 -u root /usr/local/bin/linux-desktop-up", 0, True

' 2) abre a sessao: /multimon espalha por todos os monitores, /f entra em
'    fullscreen. Em fullscreen o mstsc entrega a tecla Windows para a sessao
'    remota - e o Super dos atalhos do xfwm4 (Super+setas, Super+D, Super+R).
'    Isso foi medido: tanto Win+Direita quanto Win+Esquerda chegam ao Linux,
'    entao NAO e preciso um .rdp com keyboardhook:i:1 aqui.
WshShell.Run "mstsc.exe /v:localhost:3390 /multimon /f", 1, False
