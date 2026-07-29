#!/bin/sh
# /etc/xrdp/startwm.sh - inicia a sessao RDP com xfwm4 (sem desktop environment).
#
# O i3 foi o gerenciador original deste projeto e continua instalado como plano
# B; como voltar a ele esta documentado no bloco final deste arquivo.

# ---------------------------------------------------------------------------
# Limpeza das variaveis vazadas do WSLg.
#
# A WSL injeta DISPLAY=:0, WAYLAND_DISPLAY, XDG_RUNTIME_DIR e PULSE_SERVER em
# todo processo. Se elas sobreviverem ate aqui, tudo que for aberto dentro da
# sessao RDP vai parar na sessao do WSLg (aparecendo como janela solta no
# Windows) em vez de entrar nesta sessao. DISPLAY e reposto pelo proprio xrdp
# logo abaixo.
# ---------------------------------------------------------------------------
unset WAYLAND_DISPLAY
unset PULSE_SERVER
unset DBUS_SESSION_BUS_ADDRESS
unset XDG_SESSION_TYPE
unset WSL2_GUI_APPS_ENABLED

export XDG_RUNTIME_DIR="/run/user/$(id -u)"
[ -d "$XDG_RUNTIME_DIR" ] || { mkdir -p "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"; }

# ---------------------------------------------------------------------------
# Forcar X11 em todos os toolkits.
#
# Apagar WAYLAND_DISPLAY acima NAO basta: quando a variavel nao existe, a
# libwayland assume o socket chamado "wayland-0" dentro do XDG_RUNTIME_DIR. E
# /run/user/1000/wayland-0 e um symlink para /mnt/wslg/runtime-dir/wayland-0,
# ou seja, o compositor do WSLg.
#
# Sem isto, todo app GTK/Qt tenta Wayland primeiro, encontra o WSLg e desenha
# a janela no desktop do Windows - mesmo com DISPLAY=:10 no processo. Apps X11
# puros (xterm) nao passam por isso, o que torna o sintoma bem confuso.
# ---------------------------------------------------------------------------
export GDK_BACKEND=x11                   # GTK 3/4
export QT_QPA_PLATFORM=xcb               # Qt 5/6
export CLUTTER_BACKEND=x11
export SDL_VIDEODRIVER=x11
export MOZ_ENABLE_WAYLAND=0              # Firefox
export ELECTRON_OZONE_PLATFORM_HINT=x11  # VS Code e afins

# ---------------------------------------------------------------------------
# XAUTHORITY explicito (necessario para snaps de confinamento estrito).
#
# O xrdp poe DISPLAY no ambiente mas nao XAUTHORITY. Para um app comum isso
# nao faz falta: sem a variavel, a libX11 usa o padrao $HOME/.Xauthority e
# acha o cookie. Mas o snapd reescreve o HOME dos snaps ESTRITOS para
# ~/snap/<app>/common - e o padrao passaria a apontar para um arquivo que nao
# existe. Snaps "classic" (code, nvim, JetBrains) nao tem o HOME reescrito.
# ---------------------------------------------------------------------------
export XAUTHORITY="$HOME/.Xauthority"

# ---------------------------------------------------------------------------
# DISABLE_WAYLAND - impede o lancador do snap de sabotar o GDK_BACKEND.
#
# Os exports de X11 acima NAO bastam para snaps desktop (Firefox, Chromium,
# Thunderbird). Todo snap desses passa pelo command-chain do snap gnome-platform
# (snap/command-chain/desktop-launch), que faz:
#
#     if [[ -n "$XDG_RUNTIME_DIR" && -z "$DISABLE_WAYLAND" ]]; then
#         [ -S "$XDG_RUNTIME_DIR/../wayland-0" ] && wayland_available=true
#     ...
#     [ "$wayland_available" = true ] && export GDK_BACKEND="wayland"
#
# Ou seja: ele testa o socket, nao a variavel WAYLAND_DISPLAY. E o socket esta
# la mesmo com WAYLAND_DISPLAY apagado, porque a WSL deixa o symlink
# /run/user/1000/wayland-0 -> /mnt/wslg/runtime-dir/wayland-0. O snapd ainda
# reescreve XDG_RUNTIME_DIR para /run/user/1000/snap.<app>, entao "../wayland-0"
# cai exatamente nesse symlink do WSLg.
#
# Resultado: o lancador sobrescreve nosso GDK_BACKEND=x11 por "wayland", e o
# MOZ_ENABLE_WAYLAND=0 manda o Firefox pedir uma tela X11 que o GDK, restrito
# ao backend wayland, recusa a abrir -> "Error: cannot open display: :10.0",
# com o socket e o cookie intactos. O binario cru (sem o wrapper) funciona.
#
# DISABLE_WAYLAND e a valvula de escape prevista pelo proprio script.
# ---------------------------------------------------------------------------
export DISABLE_WAYLAND=1

if [ -r /etc/default/locale ]; then
    . /etc/default/locale
    export LANG LANGUAGE
fi

# ---------------------------------------------------------------------------
# A sessao: xfwm4 + painel.
#
# Nao e o XFCE inteiro - nao ha xfce4-session nem xfdesktop. So as tres pecas
# que fazem falta:
#
#   xfwm4        decoracao das janelas (os botoes fechar/minimizar/maximizar),
#                snap ao arrastar para a borda, Alt+Tab e os atalhos de tiling.
#                O i3 nao desenha botao nenhum - foi por isso que trocamos.
#   xfsettingsd  executa os atalhos de COMANDO (Ctrl+Alt+T e afins). O xfwm4
#                cuida so dos atalhos de janela; sem este daemon, nenhuma tecla
#                abre aplicativo.
# NAO ha painel (xfce4-panel). O pedido era uma tela limpa, sem barra de
# tarefas nem relogio - a mesma premissa da versao i3.
#
# Isso so e seguro por causa de uma opcao do seu xfwm4.xml:
#
#     cycle_hidden = true
#
# ...que faz o Alt+Tab listar tambem as janelas minimizadas. Sem ela, apertar
# Super+Baixo esconderia a janela para sempre: sem barra de tarefas nao haveria
# onde clicar para traze-la de volta. Se um dia mudar esse ajuste para false,
# ou volte o painel, ou tire o atalho de minimizar.
#
# Para lancar programas sem painel: Ctrl+Alt+T (terminal), Alt+F3 ou Super+R
# (xfce4-appfinder).
#
# Tudo no mesmo barramento dbus, com o xfwm4 no exec final - quando ele morre,
# o sh morre, o dbus-launch morre, e a sessao cai inteira. E o que faz o
# "i3-msg exit" ter equivalente aqui (pkill -x xfwm4) derrubar tudo.
# NAO existe "xfwm4 --quit" - o 4.18 responde "Unknown option --quit".
#
# Para voltar ao i3, troque este bloco inteiro por:
#     exec dbus-launch --exit-with-session /usr/bin/i3
# (o i3.config continua no repositorio, intocado).
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Debug: guarda a saida da sessao inteira (dbus-launch, xfsettingsd, xfwm4)
# em texto plano. "exec >" trunca a cada novo login, entao o arquivo sempre
# reflete so a sessao mais recente - nao cresce sem limite. Serve para
# diagnosticar o xfwm4/dbus-launch morrendo sem aviso logo apos o login (o
# xrdp-sesman.log so mostra o codigo de saida, nunca a mensagem de erro).
# ---------------------------------------------------------------------------
exec > "$HOME/startwm-debug.log" 2>&1
set -x
exec dbus-launch --exit-with-session sh -c '
    set -x
    setxkbmap -model abnt2 -layout br
    xfsettingsd &
    xfce4-terminal &

    # Som. Nao ha placa de som nesta VM (aplay -l diz "no soundcards found"):
    # o audio sai por um sink falso do PulseAudio que escreve no canal de audio
    # do xrdp, e o mstsc toca do lado do Windows. Quem faz isso e o
    # pulseaudio-module-xrdp, compilado contra as fontes do PA 16.1 (nao ha
    # pacote pronto no Ubuntu 24.04) - veja "Som" no README.
    #
    # O instalador do modulo poe um .desktop em /etc/xdg/autostart, que aqui
    # NAO adianta: autostart XDG depende de um desktop environment, e esta
    # sessao nao tem nenhum. Por isso o carregador e chamado na mao.
    #
    # O --exit-idle-time=-1 impede o PulseAudio de se encerrar sozinho quando
    # ninguem esta tocando nada - se ele morrer, o sink some junto e o som so
    # volta no proximo login.
    if [ -x /usr/libexec/pulseaudio-module-xrdp/load_pa_modules.sh ]; then
        pulseaudio --start --exit-idle-time=-1
        ( sleep 2; /usr/libexec/pulseaudio-module-xrdp/load_pa_modules.sh ) &
    fi

    # CONTORNO, nao correcao. Alguns atalhos (aqui, o <Super>Right) nao sao
    # capturados quando o xfwm4 os le no arranque: a configuracao fica certa,
    # a tecla chega ao servidor X - da para provar com o diag-super-direita.sh
    # - e o xfwm4 nao reage. Definir o mesmo atalho com a sessao ja rodando
    # funciona sempre. Ou seja, e uma corrida na largada; a suspeita e o
    # setxkbmap trocando o mapa do teclado no mesmo instante em que o xfwm4
    # registra os grabs, mas isso nao foi provado.
    #
    # Reaplicar 4 segundos depois resolve na pratica. Se um dia a causa real
    # aparecer, esta e a primeira linha a sair daqui.
    if [ -r "$HOME/linux-fullscreen/xfwm-atalhos.sh" ]; then
        ( sleep 4; bash "$HOME/linux-fullscreen/xfwm-atalhos.sh" >/dev/null 2>&1 ) &
    fi

    exec xfwm4
'
