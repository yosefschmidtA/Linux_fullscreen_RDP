#!/bin/bash
# Descobre POR QUE o Super+Direita nao funciona (o Super+Esquerda funciona).
#
#   bash ~/linux-fullscreen/diag-super-direita.sh
#
# Rode DE DENTRO da sessao. Ele troca o atalho por um "carimbo" temporario,
# espera voce apertar a tecla, e restaura tudo sozinho no fim.
#
# A pergunta que ele responde: a tecla chega ao Linux?
#   - se chega  -> o problema esta na acao tile_right_key do xfwm4
#   - se nao    -> o Windows/mstsc esta engolindo o Win+Direita antes
set -u

if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    echo "ERRO: rode num terminal DENTRO da sessao RDP." >&2
    exit 1
fi

CH=xfce4-keyboard-shortcuts
MARCA=/tmp/super-direita-chegou

restaura() {
    xfconf-query -c "$CH" -p '/commands/custom/<Super>Right' -r 2>/dev/null

    # Remover a propriedade NAO basta, e esta era a armadilha deste script.
    #
    # Enquanto <Super>Right for um atalho de COMANDO, quem registra o grab da
    # tecla no servidor X e o xfsettingsd. Ao apagar a propriedade o xfsettingsd
    # atualiza a configuracao mas NAO devolve o grab - ele continua dono da
    # tecla ate morrer. E um passive grab so tem um dono: a partir dai o xfwm4
    # leva BadAccess ao tentar registrar o dele, e a acao de janela nunca roda.
    #
    # Resultado: rodar este diagnostico DEIXAVA o Super+Direita morto pelo resto
    # da sessao - exatamente a tecla que ele se propunha a investigar. Como o
    # sintoma sobrevivia a qualquer remove/recria no xfconf, ele parecia uma
    # falha insoluvel do xfwm4 no arranque.
    #
    # Reiniciar o xfsettingsd e o que efetivamente devolve a tecla. E seguro:
    # no startwm.sh ele sobe com "&", nao e o exec final, entao derruba-lo nao
    # encerra a sessao. Os atalhos de comando (Ctrl+Alt+T) voltam com ele.
    pkill -x xfsettingsd 2>/dev/null
    sleep 2
    pkill -9 -x xfsettingsd 2>/dev/null   # caso o SIGTERM nao tenha bastado
    setsid xfsettingsd >/dev/null 2>&1 < /dev/null &
    sleep 2

    # So agora vale recriar o atalho de janela: com o grab livre, o xfwm4
    # consegue registra-lo.
    xfconf-query -c "$CH" -p '/xfwm4/custom/<Super>Right' -r 2>/dev/null
    xfconf-query -c "$CH" -p '/xfwm4/custom/<Super>Right' -n -t string -s tile_right_key 2>/dev/null \
      || xfconf-query -c "$CH" -p '/xfwm4/custom/<Super>Right' -t string -s tile_right_key
    echo "  (atalho original restaurado, xfsettingsd reiniciado)"
}
trap restaura EXIT

rm -f "$MARCA"

# Tira o atalho de janela e poe um comando que so cria um arquivo.
xfconf-query -c "$CH" -p '/xfwm4/custom/<Super>Right' -r 2>/dev/null
xfconf-query -c "$CH" -p '/commands/custom/<Super>Right' -n -t string -s "touch $MARCA" 2>/dev/null \
  || xfconf-query -c "$CH" -p '/commands/custom/<Super>Right' -t string -s "touch $MARCA"

echo
echo "  >>> APERTE  Super+Direita  algumas vezes agora <<<"
echo "      (15 segundos)"
sleep 15

echo
if [ -f "$MARCA" ]; then
    echo "RESULTADO: a tecla CHEGA ao Linux."
    echo
    echo "Nem o Windows nem o mstsc estao interceptando - a tecla e entregue a"
    echo "sessao. NAO adianta mexer em keyboardhook nem trocar a acao: a acao"
    echo "tile_right_key funciona (da para provar ligando-a a outra tecla, tipo"
    echo "<Super>F9 - ela encaixa a janela a direita normalmente)."
    echo
    echo "A causa e um grab orfao: enquanto <Super>Right for atalho de COMANDO,"
    echo "o dono do grab no X e o xfsettingsd, e ele nao devolve a tecla quando"
    echo "a propriedade e apagada. Como passive grab so tem um dono, o xfwm4"
    echo "leva BadAccess e a acao de janela nunca roda."
    echo
    echo "A saida deste script ja reinicia o xfsettingsd, que e o que devolve a"
    echo "tecla. Se algum dia o Super+Direita morrer de novo, e isto:"
    echo "    pkill -x xfsettingsd; sleep 2; setsid xfsettingsd >/dev/null 2>&1 &"
    echo "    xfconf-query -c xfce4-keyboard-shortcuts \\"
    echo "      -p '/xfwm4/custom/<Super>Right' -r"
    echo "    xfconf-query -c xfce4-keyboard-shortcuts \\"
    echo "      -p '/xfwm4/custom/<Super>Right' -n -t string -s tile_right_key"
else
    echo "RESULTADO: a tecla NAO chega ao Linux."
    echo
    echo "O Windows esta engolindo o Win+Direita (Aero Snap) antes de mandar"
    echo "para a sessao. O Super+Esquerda passa porque, com a janela do mstsc"
    echo "no monitor da esquerda, o atalho local nao tem para onde encaixar."
    echo
    echo "Correcao: force o mstsc a mandar TODAS as combinacoes com a tecla"
    echo "Windows para a sessao remota. Crie um arquivo .rdp com:"
    echo "    keyboardhook:i:1"
    echo "e abra por ele em vez do /v: direto no .vbs."
fi
rm -f "$MARCA"
