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
# OpenGL na GPU dedicada (RTX 4060) em vez do rasterizador por software.
#
# Sem isto, o Mesa cai no llvmpipe - que executa os shaders na CPU. Funciona,
# e para cena leve chega a ser MAIS RAPIDO (medido: glxgears a 1084 FPS contra
# 103 do d3d12 numa janela 300x300), porque o d3d12 paga um pedagio fixo de
# ~10 ms por quadro copiando o resultado da GPU de volta para a memoria do
# sistema - o xorgxrdp so le pixels da RAM.
#
# A escolha aqui e deliberada: prioridade para a GPU, mesmo custando FPS em
# app leve. O pedagio e LATENCIA, nao largura de banda - 16x mais pixels
# custam so 25% a mais - entao ele nao piora com telas grandes, e os 77 FPS
# medidos a 1600x900 ficam acima do teto de entrega da sessao (62 fps).
#
# Sem o ADAPTER_NAME o d3d12 escolhe a Intel integrada, nao a NVIDIA.
#
# Para rodar UM aplicativo na CPU (util se algo renderizar errado):
#     GALLIUM_DRIVER=llvmpipe <o-app>
# ---------------------------------------------------------------------------
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA

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
# Para lancar programas sem painel: Ctrl+Alt+T (o terminal deste repositorio,
# /usr/local/bin/terminal), Alt+F3 ou Super+R (xfce4-appfinder).
#
# A sessao nasce SEM terminal aberto. Ate 02/08/2026 havia um "xfce4-terminal &"
# aqui, o que custava ~9 MB de RSS e uma janela para fechar a cada login mesmo
# quando o login era so para abrir a bancada ou um jogo. Quem quiser um terminal
# aperta Ctrl+Alt+T - e o que abre e o terminal.c, nao o do XFCE.
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

    # Barra de tarefas (relogio + desligar), no visual do dialogo do xrdp.
    # Compilada de barra-tarefas.c pelo install.sh; se nao estiver instalada, a
    # sessao segue sem ela. Ela mesma acha o monitor primario pelo XRandR e se
    # reposiciona quando o layout muda (o abrir-windows encolhe a sessao), entao
    # nao precisa de sleep aqui nem de coordenada chumbada.
    [ -x /usr/local/bin/barra-tarefas ] && /usr/local/bin/barra-tarefas &

    # Panorama: toca a tecla Win e aparece a grade das janelas abertas, cada uma
    # com a propria miniatura - o caminho de volta para o que se minimizou. Sem
    # ele, so o Alt+Tab (que depende do cycle_hidden=true no xfwm4.xml).
    #
    # Sobe DEPOIS do xfsettingsd e ANTES do xfwm4, e a ordem nao importa: ele
    # nao pega grab de tecla nenhum. Quem detecta a tecla Win e o XInput2 com
    # eventos crus, que chegam sem tirar a tecla de ninguem - por isso o
    # Super+seta do tiling continua inteiro no xfwm4. Ver README, "O panorama".
    #
    # Parado ele custa 0,6 MB de PSS e nao acorda a CPU; a fonte so e carregada
    # na primeira vez que o painel abre.
    [ -x /usr/local/bin/panorama ] && /usr/local/bin/panorama &

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

    # NECESSARIO, e nao mais um contorno. Ate 29/07/2026 este bloco era marcado
    # como CONTORNO, com a suspeita de uma "corrida na largada" entre o setxkbmap
    # e o registro dos grabs pelo xfwm4, e a nota de que seria a primeira linha a
    # sair quando a causa real aparecesse.
    #
    # A causa apareceu, e inverteu a conclusao: o xfwm4 guarda UMA tecla por
    # acao, e o padrao do XFCE ja aponta <Super>KP_Left/KP_Right para as mesmas
    # acoes que as setas querem (tile_left_key/tile_right_key). Duas teclas na
    # mesma acao = so uma consegue o grab no X, e ganha a ULTIMA gravada. Era
    # isso que fazia o <Super>Right nascer morto de forma intermitente, sem
    # nenhuma corrida envolvida. Ver README, "A regra que faltava".
    #
    # Este bloco e o que remove a duplicata e grava as setas por ultimo - ou
    # seja, e ele que garante o grab. Tirar daqui devolve o bug.
    #
    # O caminho instalado vem primeiro, e o repositorio e o plano B. Chumbar
    # "$HOME/linux-fullscreen" aqui significava que renomear ou mover a pasta do
    # repositorio derrubava os atalhos de janela no proximo login, em silencio -
    # a sessao sobe igual, so o Super+seta e que morre. E o mesmo motivo pelo
    # qual o perfil.sh mora fora do repositorio.
    for atalhos in /usr/local/share/linux-fullscreen/xfwm-atalhos.sh \
                   "$HOME/linux-fullscreen/xfwm-atalhos.sh"; do
        [ -r "$atalhos" ] || continue
        ( sleep 4; bash "$atalhos" >/dev/null 2>&1 ) &
        break
    done

    exec xfwm4
'
