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

desbind() {   # desbind <atalho>   - tira a tecla da acao, liberando o slot
    xfconf-query -c "$CH" -p "/xfwm4/custom/$1" -r 2>/dev/null \
        && echo "  $1  ->  (removido)"
}

# UMA TECLA POR ACAO. Medido em 29/07/2026, e e a causa do <Super>Right morto
# que este projeto perseguiu como "corrida na largada" durante dias.
#
# O xfwm4 guarda UM atalho por acao interna. Se duas teclas apontam para a mesma
# acao, so uma consegue o passive grab no servidor X - e a outra fica muda. Quem
# ganha e a ULTIMA a ser gravada, o que faz o vencedor depender da ordem do XML
# e mudar de sessao para sessao. Era isso que imitava uma falha de arranque.
#
# O padrao do XFCE poe tile_left_key/tile_right_key no teclado numerico
# (<Super>KP_Left / <Super>KP_Right). Como as setas abaixo querem as MESMAS duas
# acoes, os dois pares colidem, e por isso as duplicatas saem antes.
#
# Como conferir (grab de tecla tem um dono; tentar registrar em cima da
# BadAccess). Sem duplicata, os quatro tem que dar BadAccess:
#
#   xfconf-query -c xfce4-keyboard-shortcuts -p /xfwm4/custom -l -v \
#       | awk 'NF==2{print $2}' | sort | uniq -d   # nao deve sair nada
#
# KP_Up e KP_Down NAO colidem e por isso ficam: apontam para tile_up_key e
# tile_down_key, enquanto as setas de cima e de baixo vao para maximize e
# hide - acoes diferentes, slots diferentes. Idem KP_Home/End/Next/Page_Up.
desbind '<Super>KP_Left'
desbind '<Super>KP_Right'

# Os tres abaixo perdiam a disputa para as teclas Super e por isso JA ESTAVAM
# mudos - removidos para a configuracao parar de anunciar tecla que nao funciona.
# A escolha aqui e Super, decidida em 29/07/2026; para inverter qualquer uma,
# troque o desbind pelo bind correspondente e tire a linha do Super.
#
#   <Alt>F10        vs <Super>Up    -> maximize_window_key
#   <Alt>F9         vs <Super>Down  -> hide_window_key
#   <Primary><Alt>d vs <Super>d     -> show_desktop_key
desbind '<Alt>F10'
desbind '<Alt>F9'
desbind '<Primary><Alt>d'

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
# Ctrl+Alt+T abre o terminal DESTE repositorio (terminal.c), nao o do XFCE.
# Trocado em 02/08/2026, junto com a saida do "xfce4-terminal &" do startwm.sh:
# a sessao passou a nascer sem terminal nenhum, entao esta tecla virou o unico
# caminho normal para abrir um.
#
# O teste esta AQUI, na hora de gravar, e nao dentro do comando gravado. O
# terminal.c e opcional no install.sh (ele avisa e segue quando o libxft-dev
# falta), e sem terminal no login uma tecla morta deixaria a sessao sem caminho
# obvio para um shell - so pelo appfinder. Um `sh -c "if ..."` no valor tambem
# resolveria, mas quem executa esses comandos e o xfsettingsd, que passa a string
# pelo g_shell_parse_argv antes de rodar: aspas dentro do valor sao mais uma
# camada de citacao para errar, e o sintoma de erro e a tecla nao fazer NADA,
# calada. Com o valor sem aspa nenhuma nao ha o que dar errado.
#
# Se o binario aparecer depois, o startwm.sh roda este script a cada login e a
# tecla se corrige sozinha no proximo.
cmd '<Primary><Alt>t' 'exo-open --launch TerminalEmulator'
cmd '<Alt>F3'         'xfce4-appfinder'
cmd '<Super>r'        'xfce4-appfinder -c'

# Print de tela (flameshot). DUAS teclas para a MESMA acao aqui e permitido e
# proposital - a regra de "uma tecla por acao" vale para as acoes internas do
# xfwm4, que guardam um unico atalho cada. No namespace /commands cada tecla e
# uma entrada independente com sua propria string, entao nao ha colisao.
#
# Sao duas porque este teclado NAO tem tecla Print, e porque o Win+Shift+S pode
# ser engolido pelo Windows (a Ferramenta de Captura usa a mesma combinacao) -
# se o mstsc nao repassar, o Ctrl+Alt+S continua valendo. Descubra qual pega e
# apague a outra se quiser.
#
# 'flameshot gui' abre a selecao de regiao com a barra de anotacao (seta, texto,
# retangulo, borrao). O v12 nao precisa de daemon nem de icone de bandeja: sobe,
# captura e sai, o que casa com a premissa de RAM baixa.
cmd '<Super><Shift>s' 'flameshot gui'
cmd '<Primary><Alt>s' 'flameshot gui'

# Estas tres ja vinham do padrao do XFCE apontando para o xfce4-screenshooter,
# que nunca foi instalado - eram atalhos orfaos. Repontadas para o flameshot;
# so servem em teclado que tenha a tecla, mas nao custam nada deixar corretas.
#
# A pasta sai de $HOME, expandido AQUI, na hora de gravar. Ate 02/08/2026 era
# "/home/yosef/prints" chumbado - errado para qualquer outro usuario, e o
# flameshot nao avisa: ele so nao salva. E o $HOME tem de ser expandido no
# momento da gravacao mesmo, porque quem executa esses comandos e o
# xfsettingsd, que nao passa a string por shell nenhum - um "$HOME" literal
# no valor chegaria ao flameshot como o nome de uma pasta chamada "$HOME".
mkdir -p "$HOME/prints"
cmd 'Print'           'flameshot gui'
cmd '<Shift>Print'    "flameshot full -p $HOME/prints"
cmd '<Alt>Print'      "flameshot screen -p $HOME/prints"

echo
echo "== helper de terminal do exo =="
# O Ctrl+Alt+T NAO passa mais por aqui (ver o bloco acima), mas este arquivo
# continua valendo: e por ele que o appfinder, o Thunar e qualquer app que chame
# "exo-open --launch TerminalEmulator" descobrem QUAL terminal abrir. Sem ele,
# esses caminhos nao fazem nada. Fica no xfce4-terminal porque o exo so aceita
# helper REGISTRADO (um .desktop em share/xfce4/helpers, com X-XFCE-Binaries), e
# registrar o terminal.c ali seria mais um arquivo para manter em troca de um
# caminho que quase nunca e usado nesta sessao.
HELP="$HOME/.config/xfce4/helpers.rc"
mkdir -p "$(dirname "$HELP")"
if grep -q '^TerminalEmulator=' "$HELP" 2>/dev/null; then
    sed -i 's/^TerminalEmulator=.*/TerminalEmulator=xfce4-terminal/' "$HELP"
else
    echo 'TerminalEmulator=xfce4-terminal' >> "$HELP"
fi
echo "  $HELP -> xfce4-terminal"

echo
echo "== fluidez da sessao remota =="
# Esta sessao nao tem GPU nenhuma desenhando: o Xorg roda com o driver virtual
# xrdpdev, que desenha por CPU e entrega os pixels ao xrdp, que comprime (em
# CPU) e manda por TCP. Sao 4480x1080 a cada quadro. Tudo que evite trabalho
# nesse caminho vale mais do que parece.
perf() {   # perf <propriedade> <tipo> <valor>
    xfconf-query -c xfwm4 -p "$1" -n -t "$2" -s "$3" 2>/dev/null \
        || xfconf-query -c xfwm4 -p "$1" -s "$3"
    echo "  $1 -> $3"
}

# O compositor do xfwm4 recompoe regioes grandes a cada movimento, e aqui isso
# sai caro porque nao ha GPU para absorver. Medido em 28/07/2026 com glxgears
# numa janela 1600x900: 197 FPS com compositing, 225 sem - +14%. O ganho no
# ARRASTO e maior que isso; o glxgears mede uma janela parada se redesenhando,
# nao uma janela se movendo. O preco e perder sombra e transparencia.
perf /general/use_compositing bool false

# Arrastar/redimensionar mostrando so o contorno. Com o conteudo visivel, cada
# pixel de movimento do mouse vira um quadro cheio para comprimir e transmitir.
perf /general/box_move   bool true
perf /general/box_resize bool true

echo
echo "== apps 2D fora do canal da GPU =="
# A sessao roda com GALLIUM_DRIVER=d3d12 (veja startwm.sh): todo cliente
# OpenGL renderiza na RTX 4060. Mas o resultado precisa voltar para a RAM,
# porque o xorgxrdp so le pixels de la - e esse canal, pelo /dev/dxg,
# SERIALIZA entre clientes.
#
# Medido em 29/07/2026, glxgears a 1600x900:
#   um cliente sozinho no canal ......... 77 FPS
#   dividindo o canal com o VS Code ..... 37 FPS
#
# VS Code e Firefox sao interfaces 2D: nao ganham nada com a GPU e tomam
# metade da vazao de quem ganha. Tira-los dali devolve o canal ao 3D.
#
# Nao da para fazer isso com variavel de ambiente por app (o Mesa nao tem
# selecao de driver no drirc - conferido). Entao usa-se a configuracao NATIVA
# de cada um, que vale independente de como o app foi aberto.

# VS Code: JSONC (aceita comentario), lido no arranque do Electron.
CODEARGV="$HOME/.config/Code/User/argv.json"
if [ ! -f "$CODEARGV" ]; then
    mkdir -p "$(dirname "$CODEARGV")"
    printf '{\n    "disable-hardware-acceleration": true\n}\n' > "$CODEARGV"
    echo "  $CODEARGV -> aceleracao desligada"
elif grep -q '"disable-hardware-acceleration"' "$CODEARGV"; then
    echo "  $CODEARGV -> ja configurado"
else
    echo "  ATENCAO: $CODEARGV existe e nao tem disable-hardware-acceleration."
    echo "           Nao vou mexer num arquivo seu; acrescente a mao:"
    echo '             "disable-hardware-acceleration": true'
fi

# Firefox: user.js no perfil. O nome do perfil e aleatorio por maquina, dai o
# glob. Snap e deb guardam em lugares diferentes.
for base in "$HOME/snap/firefox/common/.mozilla/firefox" "$HOME/.mozilla/firefox"; do
    for prof in "$base"/*.default*; do
        [ -d "$prof" ] || continue
        if ! grep -q "gfx.webrender.software" "$prof/user.js" 2>/dev/null; then
            echo 'user_pref("gfx.webrender.software", true);' >> "$prof/user.js"
        fi
        echo "  $prof/user.js -> renderizacao por software"
    done
done

echo
echo "Pronto. Os atalhos valem na hora, sem reiniciar a sessao."
echo "O VS Code e o Firefox precisam ser FECHADOS e reabertos para a"
echo "configuracao de GPU valer - reiniciar a sessao nao e necessario."
echo "Do teclado numerico, KP_Up/KP_Down/KP_Home/KP_End/KP_Next/KP_Page_Up"
echo "continuam valendo; KP_Left e KP_Right foram REMOVIDOS de proposito -"
echo "colidiam com Super+Left/Right na mesma acao e uma das duas ficava muda."
echo "Para ter sombra/transparencia de volta:"
echo "  xfconf-query -c xfwm4 -p /general/use_compositing -s true"
