#!/usr/bin/env bash
# Sessao Linux fullscreen multi-monitor sobre WSL2 (xrdp + xfwm4, sem DE).
#
#   sudo bash install.sh
#
# Idempotente: pode rodar de novo sem estragar nada. Todo arquivo do sistema
# que e substituido ganha uma copia .orig antes.
#
# Atencao: o passo 7 escreve do lado WINDOWS (%USERPROFILE%\.wslconfig) para
# desligar o WSLg, e por isso exige um 'wsl --shutdown' depois. E o unico passo
# que sai da WSL; se o interop estiver desligado o script avisa e continua.
set -euo pipefail

RDP_PORT=3390   # 3389 e do RDP nativo do Windows - nao dá pra usar

# Profundidade de cor negociada com o mstsc.
#
# NAO baixe para 24. Ja esteve em 24 aqui, para tirar 25% dos bytes que o
# NSCodec tinha que comprimir - fazia sentido enquanto o servidor era o xrdp
# 0.9.24. Com o 0.10 e o GFX, 24 bits fazem o servidor RECUSAR o pipeline:
#
#   [WARN] client requested gfx protocol with insufficient color depth
#
# ...e a sessao cai de volta no NSCodec, que e justamente o que se queria
# abandonar. O H.264 comprime muito melhor que os 25% economizados ali.
# O "session bpp" do Linux Fullscreen.rdp tem que casar com este valor.
MAX_BPP=32

if [ "$(id -u)" -ne 0 ]; then
    echo "Precisa de root. Rode: sudo bash $0" >&2
    exit 1
fi

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REAL_USER="${SUDO_USER:-$(logname 2>/dev/null || echo yosef)}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"

echo "==> Usuario: $REAL_USER  ($REAL_HOME)"

echo
echo "==> [1/7] Instalando pacotes"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
# xfwm4 e o gerenciador de janelas; xfce4-settings traz o xfsettingsd, que e
# quem executa os atalhos de COMANDO (Ctrl+Alt+T); exo-utils traz o exo-open,
# usado por esse atalho; xfce4-appfinder e como se abre programa sem painel.
# O i3 fica instalado de proposito: e o plano B documentado no startwm.sh.
# zenity: dialogo de confirmacao do linux-desktop-down (o "botao de desligar").
# Sem ele o script cai para xmessage (x11-utils) e, sem os dois, so consegue
# perguntar num terminal - o que nao serve para um item lancado pelo appfinder.
# gcc/libx11-dev/libxrandr-dev sao para compilar a barra-tarefas (passo 2).
# xfonts-base traz as fontes CORE do X: a barra usa -adobe-helvetica-11, bitmap
# e sem antialiasing, que e o que faz o texto ficar identico ao do dialogo do
# xrdp em vez de so parecido.
#
# imagemagick e do barra-apps, NAO da barra: ele converte o icone do app UMA vez
# (na hora em que voce o fixa) para pixels crus. E o que evita linkar libpng no
# barra-tarefas.c e manter a barra sem dependencia de imagem nenhuma.
apt-get install -y --no-install-recommends \
    xrdp xorgxrdp xfwm4 xfce4-settings exo-utils xfce4-appfinder \
    i3 xfce4-terminal dbus-x11 x11-xserver-utils fonts-dejavu-core \
    zenity x11-utils gcc libx11-dev libxrandr-dev xfonts-base imagemagick \
    libxft-dev fonts-dejavu-core xterm ranger tmux

echo
echo "==> [2/7] Scripts auxiliares, servico do X11-unix e itens do menu"
install -m 755 "$SRC/fix-x11-unix"       /usr/local/bin/fix-x11-unix
install -m 755 "$SRC/linux-desktop-up"   /usr/local/bin/linux-desktop-up
install -m 755 "$SRC/linux-desktop-down" /usr/local/bin/linux-desktop-down
install -m 755 "$SRC/abrir-windows"      /usr/local/bin/abrir-windows
install -m 755 "$SRC/transferir-usb"     /usr/local/bin/transferir-usb
install -m 755 "$SRC/audio-dispositivos" /usr/local/bin/audio-dispositivos
install -m 755 "$SRC/camera-rede"        /usr/local/bin/camera-rede
install -m 755 "$SRC/barra-apps"          /usr/local/bin/barra-apps
install -m 644 "$SRC/x11-unix-writable.service" /etc/systemd/system/

# A ponte de video precisa do v4l2loopback, que NAO vem no kernel da WSL e nao
# tem pacote: e compilado por fora (ver README, "Webcam por rede").
#
# O .ko fica em /usr/local/lib/v4l2loopback/ e NAO em /lib/modules. Medido em
# 31/07/2026: a WSL monta um overlay sobre /usr/lib/modules/<kver> e recria a
# camada de escrita a cada boot, entao modulo instalado la some no proximo
# arranque - com o kernel intacto, o que torna o sintoma confuso. Por isso o
# carregamento e por servico, com insmod de caminho absoluto (o modprobe nao
# acharia, ja que so olha em /lib/modules/$(uname -r)).
rm -f /etc/modules-load.d/v4l2loopback.conf /etc/modprobe.d/v4l2loopback.conf
install -m 644 "$SRC/v4l2loopback.service" /etc/systemd/system/
if [ -f "$REAL_HOME/v4l2loopback-src/v4l2loopback.ko" ]; then
    install -D -m 644 "$REAL_HOME/v4l2loopback-src/v4l2loopback.ko" \
        /usr/local/lib/v4l2loopback/v4l2loopback.ko
    systemctl enable --quiet v4l2loopback.service
    echo "    v4l2loopback instalado; sobe pelo servico no boot"
elif [ -f /usr/local/lib/v4l2loopback/v4l2loopback.ko ]; then
    systemctl enable --quiet v4l2loopback.service
    echo "    v4l2loopback ja instalado; servico habilitado"
else
    echo "    AVISO: v4l2loopback nao compilado; a camera por rede nao vai funcionar"
    echo "           rode: bash $SRC/compilar-v4l2loopback"
fi

# Os TRES componentes compilados deste repositorio, todos C com Xlib cru e sem
# toolkit. O startwm.sh sobe a barra a cada login; a bancada e o terminal sao
# por demanda. Ver README, "A barra de tarefas", "Trocar o VS Code" e "Um
# emulador de terminal escrito aqui".
#
# Nenhum dos tres e obrigatorio para a sessao subir, e por isso cada um falha
# sozinho, com aviso, em vez de derrubar a instalacao inteira.
if gcc -O2 -Wall -o "$SRC/barra-tarefas" "$SRC/barra-tarefas.c" \
        -lX11 -lXrandr 2>/dev/null; then
    install -m 755 "$SRC/barra-tarefas" /usr/local/bin/barra-tarefas
    echo "    barra-tarefas compilada e instalada"
else
    echo "    AVISO: barra-tarefas nao compilou; a sessao sobe sem ela"
fi

# A bancada nao entra na sessao sozinha e nao e essencial: se as bibliotecas de
# Xft faltarem, o resto do ambiente sobe igual.
if gcc -O2 -Wall -o "$SRC/bancada" "$SRC/bancada.c" \
        $(pkg-config --cflags xft 2>/dev/null) -lX11 -lXft 2>/dev/null; then
    install -m 755 "$SRC/bancada" /usr/local/bin/bancada
    echo "    bancada compilada e instalada"
else
    echo "    AVISO: bancada nao compilou (falta libxft-dev?); segue sem ela"
fi

# O emulador de terminal proprio. E INDEPENDENTE: a bancada continua usando o
# xterm na aba do Claude, e nada nesta sessao passa a depender deste binario.
# Instala-se porque ele so serve para ser aberto, e nao para ser compilado a
# mao toda vez.
#
# O -lfontconfig e a mais em relacao a bancada: quem escolhe a fonte de reserva
# para o caractere que a DejaVu nao tem (japones, emoji) e o FcFontMatch,
# chamado direto. Nao e pacote novo - o libxft-dev ja depende do
# libfontconfig1-dev, que instala tanto o cabecalho quanto o .so do link.
#
# Sem as fontes Noto instaladas, a reserva nao acha nada e esses caracteres
# saem como caixa vazia. Nao se instala fonte aqui de proposito: seriam dezenas
# de MB de disco para um caso que quase nao aparece, e quem precisar resolve com
# um `apt install fonts-noto-core fonts-noto-color-emoji` - a reserva passa a
# achar sozinha, sem recompilar nada.
if gcc -O2 -Wall -o "$SRC/terminal" "$SRC/terminal.c" \
        $(pkg-config --cflags xft 2>/dev/null) -lX11 -lXft -lfontconfig 2>/dev/null; then
    install -m 755 "$SRC/terminal" /usr/local/bin/terminal
    echo "    terminal compilado e instalado"
else
    echo "    AVISO: terminal nao compilou (falta libxft-dev?); segue sem ele"
fi

# O trecho que entra no shell: a funcao `bancada`, que abre na pasta atual e
# devolve o prompt, como o `code .` do VS Code. Instalado fora do repositorio
# porque o .bashrc nao pode depender de onde o repositorio esta.
#
# A linha no .bashrc e marcada e idempotente: reinstalar nao duplica. Nao se
# usa /etc/profile.d aqui porque terminal interativo NAO-login (o caso normal:
# xfce4-terminal, xterm) le o .bashrc e ignora o profile.d.
install -D -m 644 "$SRC/perfil.sh" /usr/local/share/linux-fullscreen/perfil.sh

# Estas duas vao para o mesmo lugar e pelo mesmo motivo do perfil.sh: quem as usa
# nao pode depender de o repositorio estar em ~/linux-fullscreen.
#
#   audio-padrao.ps1   e a ponte de audio do lado Windows. O transferir-usb e o
#                      audio-dispositivos o copiam para o LOCALAPPDATA; sem
#                      achar a fonte, trocar o padrao do Windows para de
#                      funcionar com um aviso vago.
#   xfwm-atalhos.sh    e chamado pelo startwm.sh a cada login. Sem achar, a
#                      sessao sobe igual e so os Super+seta morrem - falha
#                      silenciosa, do tipo que este projeto mais paga caro.
#
# Os dois mantem o repositorio como plano B, entao rodar de la sem instalar
# continua valendo.
install -m 644 "$SRC/windows/audio-padrao.ps1" /usr/local/share/linux-fullscreen/audio-padrao.ps1
install -m 755 "$SRC/xfwm-atalhos.sh"          /usr/local/share/linux-fullscreen/xfwm-atalhos.sh
MARCA='# linux-fullscreen'
if ! grep -qF "$MARCA" "$REAL_HOME/.bashrc" 2>/dev/null; then
    cp -a "$REAL_HOME/.bashrc" "$REAL_HOME/.bashrc.bak.$(date +%Y%m%d%H%M%S)" 2>/dev/null || true
    printf '\n%s\n. /usr/local/share/linux-fullscreen/perfil.sh\n' "$MARCA" \
        >> "$REAL_HOME/.bashrc"
    chown "$REAL_USER:$REAL_USER" "$REAL_HOME/.bashrc"
    echo "    funcao 'bancada' ligada no .bashrc"
else
    echo "    .bashrc ja carrega o perfil.sh"
fi

# Os .desktop sao a "interface": sem painel, quem lista aplicativos e o
# xfce4-appfinder (Alt+F3 ou Super+R). Tudo que virar item de menu nesta
# sessao entra por aqui - e o lugar de crescer a UI sem subir um DE inteiro.
install -d -m 755 /usr/share/applications
install -m 644 "$SRC"/desktop/*.desktop /usr/share/applications/

# Nomes que este projeto ja usou e nao usa mais. O install e idempotente por
# copia, o que NAO apaga o que saiu de circulacao: sem esta linha, quem instalou
# antes de 02/08/2026 ficaria com o jogo-windows antigo em /usr/local/bin e o
# item "Jogos (Windows)" no appfinder para sempre, apontando para um script que
# nao recebe mais correcao. Vira lixo silencioso - o pior tipo.
rm -f /usr/local/bin/jogo-windows /usr/share/applications/jogos-windows.desktop

update-desktop-database /usr/share/applications 2>/dev/null || true
systemctl daemon-reload
systemctl enable --quiet x11-unix-writable.service
/usr/local/bin/fix-x11-unix

echo
echo "==> [3/7] Configurando xrdp na porta $RDP_PORT (max_bpp=$MAX_BPP)"
[ -f /etc/xrdp/xrdp.ini.orig ] || cp /etc/xrdp/xrdp.ini /etc/xrdp/xrdp.ini.orig
sed -i "s/^port=3389$/port=$RDP_PORT/"          /etc/xrdp/xrdp.ini
sed -i "s/^new_cursors=true$/new_cursors=false/" /etc/xrdp/xrdp.ini
# ^max_bpp=.*$ em vez de ^max_bpp=32$ para o script continuar idempotente
# depois que este valor ja tiver sido trocado uma vez.
sed -i "s/^max_bpp=.*$/max_bpp=$MAX_BPP/"        /etc/xrdp/xrdp.ini
# xrdp precisa ler o certificado TLS gerado na instalacao
adduser xrdp ssl-cert >/dev/null 2>&1 || true

# Xwrapper: quem pode iniciar o Xorg.
#
# O padrao do Debian/Ubuntu e "console", que so deixa iniciar o Xorg a partir
# de um console fisico - e a sessao do xrdp nao e um. Com "console" a sessao
# nao sobe, e o sintoma nao acusa a causa.
#
# Isto NAO e configurado uma vez e esquecido: o arquivo e regenerado a cada
# atualizacao do pacote xserver-xorg-legacy, voltando para "console" (o proprio
# cabecalho do arquivo avisa). Aconteceu aqui em 29/07/2026, disparado por um
# 'apt install xserver-xorg-dev'. Se um dia a sessao parar de subir depois de
# um 'apt upgrade', olhe este arquivo primeiro.
XWRAP=/etc/X11/Xwrapper.config
if [ -f "$XWRAP" ]; then
    [ -f "$XWRAP.orig" ] || cp "$XWRAP" "$XWRAP.orig"
    sed -i 's/^allowed_users=.*$/allowed_users=anybody/' "$XWRAP"
else
    echo "allowed_users=anybody" > "$XWRAP"
fi
grep -q "^allowed_users=anybody" "$XWRAP" || echo "allowed_users=anybody" >> "$XWRAP"

# GFX/H.264: threads do x264, se este xrdp for 0.10 ou mais novo.
#
# O gfx.toml so existe a partir do 0.10 - no 0.9.24 do Ubuntu nao ha arquivo
# nenhum e este bloco e ignorado, de proposito. Veja "Fluidez" no README.
#
# O padrao e 1 thread POR TELA. Com dois monitores isso da 2 threads numa
# maquina de 12 nucleos, o que e conservador demais para um unico usuario. O
# manual avisa que threads demais prejudicam a qualidade, entao 2 (= 4 no
# total aqui) fica longe do limite. Como 'tune = "zerolatency"' faz o x264
# usar threads fatiadas dentro do quadro, isso nao acrescenta latencia.
GFXCFG=/etc/xrdp/gfx.toml
if [ -f "$GFXCFG" ]; then
    [ -f "$GFXCFG.orig" ] || cp "$GFXCFG" "$GFXCFG.orig"
    sed -i 's/^threads = 1\b/threads = 2/' "$GFXCFG"
    echo "    gfx.toml: threads do x264 = 2 por tela"
fi

echo
echo "==> [4/7] Instalando startwm.sh (limpa vars do WSLg, sobe o xfwm4)"
[ -f /etc/xrdp/startwm.sh.orig ] || cp /etc/xrdp/startwm.sh /etc/xrdp/startwm.sh.orig
install -m 755 "$SRC/startwm.sh" /etc/xrdp/startwm.sh

echo
echo "==> [5/7] Instalando config do i3 para $REAL_USER (plano B, nao usada)"
install -d -o "$REAL_USER" -g "$REAL_USER" -m 755 "$REAL_HOME/.config/i3"
if [ -f "$REAL_HOME/.config/i3/config" ]; then
    cp -a "$REAL_HOME/.config/i3/config" "$REAL_HOME/.config/i3/config.bak.$(date +%Y%m%d%H%M%S)"
    echo "    (config antiga do i3 salva como config.bak.*)"
fi
install -m 644 -o "$REAL_USER" -g "$REAL_USER" "$SRC/i3.config" "$REAL_HOME/.config/i3/config"

echo
echo "==> [6/7] Subindo o xrdp"
systemctl enable --quiet xrdp xrdp-sesman
systemctl restart xrdp-sesman
systemctl restart xrdp
sleep 2

echo
echo "==> [7/7] Desligando o WSLg no Windows (.wslconfig)"
# Sem isto o setup funciona pela metade: o WSLg deixa em toda sessao WSL o
# symlink /run/user/<uid>/wayland-0 -> /mnt/wslg/runtime-dir/wayland-0, e
# qualquer app GTK/Qt/Electron que o encontre renderiza no desktop do Windows
# em vez de entrar na sessao RDP. Ver README, "O WSLg esta desligado".
#
# E o unico passo fora da WSL, entao depende do interop com o Windows estar
# ligado. Se nao der, o script avisa e segue - a instalacao Linux fica valida.
WSLCFG=""
WIN_PROFILE="$(cd /mnt/c 2>/dev/null && cmd.exe /c 'echo %USERPROFILE%' 2>/dev/null | tr -d '\r' || true)"
if [ -n "$WIN_PROFILE" ]; then
    WSLCFG="$(wslpath -u "$WIN_PROFILE" 2>/dev/null || true)"
    [ -n "$WSLCFG" ] && WSLCFG="$WSLCFG/.wslconfig"
fi

if [ -n "$WSLCFG" ] && [ -d "$(dirname "$WSLCFG")" ]; then
    if [ -f "$WSLCFG" ]; then
        [ -f "$WSLCFG.orig" ] || cp "$WSLCFG" "$WSLCFG.orig"
        if grep -q '^[[:space:]]*guiApplications[[:space:]]*=' "$WSLCFG"; then
            sed -i 's/^[[:space:]]*guiApplications[[:space:]]*=.*/guiApplications=false/' "$WSLCFG"
        elif grep -q '^[[:space:]]*\[wsl2\]' "$WSLCFG"; then
            sed -i '0,/^[[:space:]]*\[wsl2\]/s//[wsl2]\nguiApplications=false/' "$WSLCFG"
        else
            printf '\n[wsl2]\nguiApplications=false\n' >> "$WSLCFG"
        fi
    else
        printf '[wsl2]\nguiApplications=false\n' > "$WSLCFG"
    fi
    echo "    ajustado: $WSLCFG"
    echo "    (original preservado em .wslconfig.orig, se ja existia)"
    WSLG_OK=1
else
    echo "    NAO consegui achar o .wslconfig - faca a mao no Windows:"
    echo "      C:\\Users\\<voce>\\.wslconfig"
    echo "      [wsl2]"
    echo "      guiApplications=false"
    WSLG_OK=0
fi

echo
echo "======================================================================"
if ss -tln 2>/dev/null | grep -q ":$RDP_PORT "; then
    echo "OK - xrdp escutando na porta $RDP_PORT"
    echo
    echo "Proximos passos, no Windows (nesta ordem):"
    if [ "${WSLG_OK:-0}" = "1" ]; then
        echo "  1. wsl --shutdown          <- OBRIGATORIO: aplica o .wslconfig"
    else
        echo "  1. ajuste o .wslconfig como indicado acima, depois wsl --shutdown"
    fi
    echo "  2. copie 'Linux Fullscreen.vbs' para a Area de Trabalho e clique nele."
    echo "     Login: usuario '$REAL_USER' + sua senha do Linux."
    echo "  3. JA DENTRO da sessao, num terminal dela:"
    echo "       bash ~/linux-fullscreen/xfwm-atalhos.sh"
    echo "     (poe o snap de meia tela no Super+setas; o padrao do XFCE usa o"
    echo "      teclado numerico e parece que nada funciona)"
    echo
    echo "  3. Para ter SOM (opcional, demora alguns minutos):"
    echo "       sudo bash ~/linux-fullscreen/instalar-som.sh"
    echo "     (compila o pulseaudio-module-xrdp; nao ha pacote pronto no 24.04)"
    echo
    echo "  O passo 1 nao e opcional. Sem ele o WSLg continua no ar e as janelas"
    echo "  abrem soltas no desktop do Windows em vez de entrar na sessao."
else
    echo "FALHOU - nada escutando em $RDP_PORT."
    echo "Diagnostico:"
    echo "  systemctl status xrdp --no-pager"
    echo "  journalctl -u xrdp -u xrdp-sesman -n 40 --no-pager"
fi
echo "======================================================================"
