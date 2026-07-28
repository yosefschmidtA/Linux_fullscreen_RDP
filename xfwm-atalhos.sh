#!/bin/bash
# Atalhos estilo GNOME/Windows para o xfwm4, + o helper de terminal do exo.
#
#   bash ~/linux-fullscreen/xfwm-atalhos.sh
#
# Rode DE DENTRO da sessao (um terminal da propria sessao RDP). Ele fala com o
# xfconfd pelo barramento dbus da sessao; fora dela nao ha com quem falar.
#
# Idempotente: pode rodar quantas vezes quiser.
#
# Por que isto e necessario: o padrao do XFCE poe o tiling no teclado NUMERICO
# (<Super>KP_Left, KP_Right...), nao nas setas. Quem vem do GNOME ou do Windows
# aperta Super+seta e nada acontece - a tecla simplesmente nao esta mapeada.
set -u

if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    echo "ERRO: sem DBUS_SESSION_BUS_ADDRESS." >&2
    echo "Rode isto num terminal aberto DENTRO da sessao RDP." >&2
    exit 1
fi

CH=xfce4-keyboard-shortcuts

bind() {   # bind <atalho> <acao>
    # O -r antes de gravar NAO e redundancia. Gravar por cima de um valor que
    # ja existe atualiza o arquivo, mas nem sempre faz o xfwm4 refazer o grab
    # da tecla no servidor X - e sem grab a tecla nunca chega nele. Foi
    # exatamente o que aconteceu aqui com o <Super>Right: a configuracao
    # estava correta, a tecla chegava ao X, e o xfwm4 nao reagia. Remover e
    # recriar forca o registro novo e resolve.
    xfconf-query -c "$CH" -p "/xfwm4/custom/$1" -r 2>/dev/null
    xfconf-query -c "$CH" -p "/xfwm4/custom/$1" -n -t string -s "$2"
    echo "  $1  ->  $2"
}

echo "== atalhos de janela =="
# Meia tela nas laterais, como no GNOME e no Windows.
bind '<Super>Left'  tile_left_key
bind '<Super>Right' tile_right_key
# Super+Cima maximiza (aperte de novo para restaurar - o xfwm4 alterna).
bind '<Super>Up'    maximize_window_key
# Super+Baixo minimiza. Difere um pouco do Windows, que restaura antes de
# minimizar; o xfwm4 nao tem essa acao composta.
#
# Para trazer a janela de volta: Alt+Tab. Nao ha barra de tarefas nesta sessao
# (nao ha painel nenhum), entao o Alt+Tab e o UNICO caminho de volta - e ele so
# lista janelas minimizadas por causa do cycle_hidden=true no xfwm4.xml. Se
# alguem desmarcar "incluir janelas ocultas" nas preferencias do xfwm4,
# minimizar vira um buraco negro. Ver README, "Minimizar sem barra de tarefas".
bind '<Super>Down'  hide_window_key
# Mostrar area de trabalho.
bind '<Super>d'     show_desktop_key

# Mandar a janela para o monitor vizinho, como o Win+Shift+seta do Windows.
#
# O xfwm4 4.18 TEM acoes de monitor - versoes antigas deste README afirmavam o
# contrario ("o xfwm4 nao tem nenhuma acao de monitor") e mandavam arrastar com
# o mouse. Estava errado; elas existem desde o 4.16 e aparecem nas preferencias
# como "Move to Another Monitor":
#
#     strings /usr/bin/xfwm4 | grep move_window_to_monitor
#
# Ficam no Shift, e nao no Super+seta puro, de proposito: o xfwm4 nao tem uma
# acao composta "encaixa; se ja estiver na borda, pula de monitor" como a do
# Windows. Para imitar isso seria preciso trocar o Super+seta por um COMANDO
# (script lendo xrandr + wmctrl), o que significaria reimplementar o tiling e,
# pior, devolver a tecla ao xfsettingsd - exatamente o arranjo que causou o bug
# do grab orfao. Nao compensa: com o Shift, uma tecla so resolve a travessia.
bind '<Super><Shift>Left'  move_window_to_monitor_left_key
bind '<Super><Shift>Right' move_window_to_monitor_right_key
bind '<Super><Shift>Up'    move_window_to_monitor_up_key
bind '<Super><Shift>Down'  move_window_to_monitor_down_key

echo
echo "== atalhos de comando =="
# ATENCAO: nunca use aqui uma combinacao que ja exista em /xfwm4/custom/ acima.
# Os dois namespaces disputam o mesmo passive grab no X, e grab so tem um dono:
# enquanto a tecla for comando, o xfsettingsd fica dono dela e NAO devolve o
# grab nem quando a propriedade e apagada - o xfwm4 leva BadAccess e a acao de
# janela para de funcionar de vez. Foi assim que o <Super>Right ficou morto por
# sessoes inteiras. Se precisar migrar uma tecla de comando para acao de
# janela, reinicie o xfsettingsd no meio (ver README).
# Numa maquina recem-formatada estes podem nao existir. Definir aqui torna o
# script suficiente sozinho, sem depender de configuracao XFCE anterior.
cmd() {   # mesmo motivo do -r explicado no bind()
    xfconf-query -c "$CH" -p "/commands/custom/$1" -r 2>/dev/null
    xfconf-query -c "$CH" -p "/commands/custom/$1" -n -t string -s "$2"
    echo "  $1  ->  $2"
}
cmd '<Primary><Alt>t' 'exo-open --launch TerminalEmulator'
cmd '<Alt>F3'         'xfce4-appfinder'
cmd '<Super>r'        'xfce4-appfinder -c'

echo
echo "== helper de terminal do exo =="
# O Ctrl+Alt+T ja vem mapeado para "exo-open --launch TerminalEmulator", mas
# sem este arquivo o exo nao sabe QUAL terminal e a tecla nao faz nada.
HELP="$HOME/.config/xfce4/helpers.rc"
mkdir -p "$(dirname "$HELP")"
if grep -q '^TerminalEmulator=' "$HELP" 2>/dev/null; then
    sed -i 's/^TerminalEmulator=.*/TerminalEmulator=xfce4-terminal/' "$HELP"
else
    echo 'TerminalEmulator=xfce4-terminal' >> "$HELP"
fi
echo "  $HELP -> xfce4-terminal"

echo
echo "Pronto. Os atalhos valem na hora, sem reiniciar a sessao."
echo "Os do teclado numerico (Super+KP_*) continuam funcionando em paralelo."
