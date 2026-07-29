#!/usr/bin/env bash
# Substitui o xrdp/xorgxrdp da distribuicao por uma compilacao 0.10, com
# GFX/H.264.
#
#   sudo bash instalar-xrdp010.sh
#
# LEIA "O caminho de volta" NO README ANTES. Este e o experimento mais
# arriscado deste projeto: mexe nas duas pecas que sustentam a sessao, e as
# versoes das duas precisam casar. Se der errado, a sessao grafica nao sobe -
# e voce desfaz por um 'wsl -d Ubuntu-24.04' ou pelo VS Code do Windows, que
# nao dependem do xrdp:
#
#     sudo apt install --reinstall xrdp xorgxrdp
#     sudo bash ~/linux-fullscreen/install.sh
#
# POR QUE COMPILAR
# O Ubuntu 24.04 so tem o xrdp 0.9.24, que nao entende o pipeline GFX (RDP 8+).
# O mstsc oferece GFX, o 0.9.24 loga "unknown codec id 5" e a negociacao cai
# para o NSCodec, o codec mais fraco - e o caminho de captura "normal", travado
# em 40 ms (25 quadros/s). Com o GFX vem o H.264 e o intervalo de 16 ms
# (~62 quadros/s). Nao ha PPA: o unico que existe publica 0.9.4 para o xenial.
#
# NAO espere GPU. As notas do 0.10 dizem que "hardware-accelerated encoding are
# not supported in this version yet" - o encode continua em CPU. O ganho vem de
# o H.264 ser muito mais eficiente que o NSCodec, e do teto de quadros subir.
set -euo pipefail

XRDP_VER=0.10.6.1
XORGXRDP_VER=0.10.5

if [ "$(id -u)" -ne 0 ]; then
    echo "Precisa de root. Rode: sudo bash $0" >&2
    exit 1
fi

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REAL_USER="${SUDO_USER:-$(logname 2>/dev/null || echo yosef)}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
chown "$REAL_USER" "$WORK"

echo "==> [1/6] Pacotes de build"
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -qq build-essential autoconf libtool pkg-config \
    libssl-dev libpam0g-dev libx11-dev libxfixes-dev libxrandr-dev \
    libxext-dev libjpeg-dev libopus-dev libfuse3-dev libpixman-1-dev \
    libx264-dev libopenh264-dev nasm xsltproc flex bison \
    xserver-xorg-dev libxfont-dev >/dev/null

# ATENCAO A ORDEM: o passo acima atualiza o xserver-xorg-legacy, e essa
# atualizacao REGENERA o /etc/X11/Xwrapper.config com allowed_users=console -
# que impede o Xorg de subir numa sessao xrdp. Se o conserto viesse antes, ele
# seria desfeito aqui. Foi assim que a sessao quebrou em 29/07/2026.
echo "    corrigindo o Xwrapper.config (a atualizacao do X acabou de reverte-lo)"
XWRAP=/etc/X11/Xwrapper.config
if [ -f "$XWRAP" ]; then
    sed -i 's/^allowed_users=.*$/allowed_users=anybody/' "$XWRAP"
else
    echo "allowed_users=anybody" > "$XWRAP"
fi
grep -q "^allowed_users=anybody" "$XWRAP" || echo "allowed_users=anybody" >> "$XWRAP"

echo
echo "==> [2/6] Backup do que vai ser substituido"
BK="/root/xrdp-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$BK"
cp -a /etc/xrdp "$BK/etc-xrdp"
cp -a /usr/sbin/xrdp /usr/sbin/xrdp-sesman /usr/sbin/xrdp-chansrv "$BK/" 2>/dev/null || true
cp -a /usr/lib/xorg/modules/drivers/xrdp*_drv.so "$BK/" 2>/dev/null || true
echo "    $BK"

echo
echo "==> [3/6] Baixando as fontes"
# Os tarballs de release ja trazem os submodulos (librfxcodec, painter); um
# 'git clone' simples nao traz, e a compilacao falha.
cd "$WORK"
for u in "https://github.com/neutrinolabs/xrdp/releases/download/v$XRDP_VER/xrdp-$XRDP_VER.tar.gz" \
         "https://github.com/neutrinolabs/xorgxrdp/releases/download/v$XORGXRDP_VER/xorgxrdp-$XORGXRDP_VER.tar.gz"; do
    curl -sSL -O "$u"
done
tar xzf "xrdp-$XRDP_VER.tar.gz"
tar xzf "xorgxrdp-$XORGXRDP_VER.tar.gz"

echo
echo "==> [4/6] Compilando o xrdp $XRDP_VER"
# --with-socketdir precisa bater com o que o Ubuntu usava: o modulo de som do
# PulseAudio escreve nesse diretorio (XRDP_SOCKET_PATH). Mudar isso quebra o
# som silenciosamente. As demais opcoes reproduzem a compilacao do Ubuntu, para
# nao perder recurso; --enable-x264 e --enable-openh264 sao a novidade.
cd "$WORK/xrdp-$XRDP_VER"
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    --with-socketdir=/run/xrdp/sockdir \
    --with-systemdsystemunitdir=/lib/systemd/system \
    --enable-fuse --enable-ipv6 --enable-jpeg --enable-opus --enable-vsock \
    --enable-x264 --enable-openh264 --enable-pam-config=debian >/dev/null
make -j"$(nproc)" >/dev/null
make install >/dev/null

echo
echo "==> [5/6] Compilando o xorgxrdp $XORGXRDP_VER"
# Precisa dos cabecalhos do xrdp recem-compilado, apontados a mao.
cd "$WORK/xorgxrdp-$XORGXRDP_VER"
./configure --prefix=/usr \
    XRDP_CFLAGS="-I$WORK/xrdp-$XRDP_VER/common" \
    XRDP_LIBS="-L$WORK/xrdp-$XRDP_VER/common" >/dev/null
make -j"$(nproc)" >/dev/null
make install >/dev/null

echo
echo "==> [6/6] Reaplicando a configuracao deste projeto"
# O 'make install' do xrdp SOBRESCREVE /etc/xrdp/xrdp.ini e startwm.sh com os
# padroes dele. Tudo que este projeto ajusta tem que voltar.
bash "$SRC/install.sh" >/dev/null
systemctl daemon-reload

cat <<FIM

Pronto. Instalado:

    xrdp      $(xrdp --version 2>/dev/null | head -1)
    xorgxrdp  $XORGXRDP_VER
    backup    $BK

Falta reiniciar o servico - isto DERRUBA a sessao atual (o daemon antigo ainda
esta em memoria; sair da sessao pelo Alt+F3 nao bastaria):

    sudo systemctl restart xrdp-sesman xrdp

Reconecte pelo .vbs e confirme que o GFX pegou:

    sudo grep -iE "gfx|h264|codec" /var/log/xrdp.log | tail -8

Esperado: "egfx created" e "using x264 for software encoder".
Se aparecer "insufficient color depth", o max_bpp ou o "session bpp" do .rdp
esta abaixo de 32 - o GFX exige 32 bits.

Para desfazer tudo:

    sudo apt install --reinstall xrdp xorgxrdp
    sudo bash "$SRC/install.sh"
FIM
