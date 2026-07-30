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
apt-get install -y --no-install-recommends \
    xrdp xorgxrdp xfwm4 xfce4-settings exo-utils xfce4-appfinder \
    i3 xfce4-terminal dbus-x11 x11-xserver-utils fonts-dejavu-core \
    zenity x11-utils gcc libx11-dev libxrandr-dev xfonts-base

echo
echo "==> [2/7] Scripts auxiliares, servico do X11-unix e itens do menu"
install -m 755 "$SRC/fix-x11-unix"       /usr/local/bin/fix-x11-unix
install -m 755 "$SRC/linux-desktop-up"   /usr/local/bin/linux-desktop-up
install -m 755 "$SRC/linux-desktop-down" /usr/local/bin/linux-desktop-down
install -m 755 "$SRC/jogo-windows"       /usr/local/bin/jogo-windows
install -m 755 "$SRC/transferir-usb"     /usr/local/bin/transferir-usb
install -m 644 "$SRC/x11-unix-writable.service" /etc/systemd/system/

# A barra de tarefas e o unico componente compilado deste repositorio. E C com
# Xlib cru, nao um painel de desktop environment: 2,6 MB de RSS contra os 21 MB
# da primeira versao em Python+Tk, e o visual exige desenhar cada pixel a mao de
# qualquer jeito - um toolkit seria peso sem uso. O startwm.sh a sobe a cada
# login. Ver README, "A barra de tarefas".
if gcc -O2 -Wall -o "$SRC/barra-tarefas" "$SRC/barra-tarefas.c" \
        -lX11 -lXrandr 2>/dev/null; then
    install -m 755 "$SRC/barra-tarefas" /usr/local/bin/barra-tarefas
    echo "    barra-tarefas compilada e instalada"
else
    echo "    AVISO: barra-tarefas nao compilou; a sessao sobe sem ela"
fi

# Os .desktop sao a "interface": sem painel, quem lista aplicativos e o
# xfce4-appfinder (Alt+F3 ou Super+R). Tudo que virar item de menu nesta
# sessao entra por aqui - e o lugar de crescer a UI sem subir um DE inteiro.
install -d -m 755 /usr/share/applications
install -m 644 "$SRC"/desktop/*.desktop /usr/share/applications/
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
