#!/usr/bin/env bash
# Som na sessao RDP: compila e instala o pulseaudio-module-xrdp.
#
#   sudo bash instalar-som.sh
#
# Separado do install.sh de proposito: isto baixa as fontes do PulseAudio e
# compila, o que leva alguns minutos e puxa dezenas de pacotes de build. O
# install.sh precisa continuar rapido.
#
# POR QUE PRECISA COMPILAR
# Nao existe pacote pulseaudio-module-xrdp no Ubuntu 24.04 (procure com
# 'apt-cache policy pulseaudio-module-xrdp' - vem vazio). E os modulos usam a
# API interna do PulseAudio, que o libpulse-dev nao expoe: e obrigatorio ter a
# arvore de fontes do PA configurada, na mesma versao instalada.
#
# COMO O SOM FUNCIONA AQUI
# Nao ha placa de som nesta VM ('aplay -l' diz "no soundcards found"). O
# modulo cria um sink falso que escreve no canal de audio do xrdp; o mstsc
# recebe e toca do lado do Windows. Por isso o .rdp precisa de audiomode:i:0
# ("tocar no computador local") - com :2 o cliente recusa o canal e nao sai som
# por mais que o lado Linux esteja correto.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Precisa de root. Rode: sudo bash $0" >&2
    exit 1
fi

REAL_USER="${SUDO_USER:-$(logname 2>/dev/null || echo yosef)}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> [1/5] Pacotes de build"
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -qq build-essential autoconf libtool pkg-config \
                       libpulse-dev meson ninja-build git >/dev/null

echo
echo "==> [2/5] Habilitando deb-src e baixando as fontes do PulseAudio"
# O Ubuntu 24.04 usa o formato deb822; o deb-src vem desligado de fabrica.
SRCLIST=/etc/apt/sources.list.d/ubuntu.sources
if ! grep -q "^Types: deb deb-src" "$SRCLIST"; then
    [ -f "$SRCLIST.orig" ] || cp "$SRCLIST" "$SRCLIST.orig"
    sed -i 's/^Types: deb$/Types: deb deb-src/' "$SRCLIST"
fi
apt-get update -qq
apt-get build-dep -y -qq pulseaudio >/dev/null

# apt-get source recusa rodar como root sem avisar do risco; roda como o dono.
chown "$REAL_USER" "$WORK"
sudo -u "$REAL_USER" sh -c "cd '$WORK' && apt-get source pulseaudio" >/dev/null 2>&1
PA_SRC="$(find "$WORK" -maxdepth 1 -type d -name 'pulseaudio-*' | head -1)"
[ -n "$PA_SRC" ] || { echo "ERRO: nao achei as fontes do PulseAudio" >&2; exit 1; }
echo "    $PA_SRC"

echo
echo "==> [3/5] Configurando a arvore do PulseAudio (gera o config.h)"
# So o 'meson setup' interessa: o modulo precisa dos cabecalhos e do config.h,
# nao do PulseAudio compilado. O PA 16 usa meson, nao autotools.
sudo -u "$REAL_USER" sh -c "cd '$PA_SRC' && meson setup build" >/dev/null

echo
echo "==> [4/5] Compilando o pulseaudio-module-xrdp"
sudo -u "$REAL_USER" git clone -q --depth 1 \
    https://github.com/neutrinolabs/pulseaudio-module-xrdp.git "$WORK/mod"
sudo -u "$REAL_USER" sh -c "cd '$WORK/mod' && ./bootstrap && ./configure PULSE_DIR='$PA_SRC' && make -j\$(nproc)" >/dev/null
make -C "$WORK/mod" install >/dev/null

MODDIR="$(pkg-config --variable=modlibexecdir libpulse)"
if ! ls "$MODDIR" | grep -q module-xrdp-sink.so; then
    echo "ERRO: module-xrdp-sink.so nao apareceu em $MODDIR" >&2
    exit 1
fi
echo "    modulos em $MODDIR"

echo
echo "==> [5/5] Tirando o PipeWire do caminho"
# O Ubuntu 24.04 traz pipewire-pulse, que disputa o mesmo socket do PulseAudio.
# Os modulos do xrdp sao do PulseAudio e nao carregam no PipeWire, entao aqui
# quem tem que atender e o PulseAudio. Os units vem habilitados em escopo
# GLOBAL - 'disable' no escopo do usuario nao segura, so 'mask' segura.
sudo -u "$REAL_USER" XDG_RUNTIME_DIR="/run/user/$(id -u "$REAL_USER")" \
    systemctl --user mask pipewire.socket pipewire-pulse.socket \
                          wireplumber.service pipewire.service \
                          pipewire-pulse.service >/dev/null 2>&1 || true

cat <<'FIM'

Pronto. O startwm.sh sobe o PulseAudio e carrega os modulos a cada login.

Falta encerrar a sessao atual para valer (reconectar NAO basta - o sesman
devolve a mesma sessao):

    Alt+F3 -> "Sair da sessao"       ou    linux-desktop-down -s -y

Depois de reconectar, teste com:

    paplay /usr/share/sounds/alsa/Front_Center.wav

Se nao sair som, confira nesta ordem:
    pactl get-default-sink                 # tem que dizer xrdp-sink
    pgrep -ax pulseaudio                   # tem que estar rodando
    grep audiomode "Linux Fullscreen.rdp"  # tem que ser audiomode:i:0

Para devolver o PipeWire e desfazer isto:
    systemctl --user unmask pipewire.socket pipewire-pulse.socket \
                            wireplumber.service pipewire.service \
                            pipewire-pulse.service
FIM
