# perfil.sh — o pedaco deste projeto que entra no shell do usuario.
#
# Instalado em /usr/local/share/linux-fullscreen/perfil.sh e carregado por uma
# linha no ~/.bashrc. Fica FORA do repositorio depois de instalado de proposito:
# se o .bashrc apontasse para a pasta do repositorio, mover o repositorio
# quebraria o shell de quem so queria abrir um terminal.

# bancada — como o "code ." do VS Code: abre na pasta em que voce esta e DEVOLVE
# o prompt. Sem isto, `bancada .` prende o terminal e a bancada morre junto com
# ele; com setsid --fork ela sobrevive ao terminal que a abriu.
#
# Sem argumento, o binario ja usa o diretorio atual (getcwd), entao "bancada" e
# "bancada ." dao no mesmo.
#
# Para ver as mensagens de erro dela (fonte que faltou, display que nao abriu),
# fuja da funcao com `command bancada` — isso roda o binario em primeiro plano.
bancada() {
    if [ -x /usr/local/bin/bancada ]; then
        setsid --fork /usr/local/bin/bancada "$@" </dev/null >/dev/null 2>&1
    else
        echo "bancada nao instalada; rode o install.sh do repositorio" >&2
        return 127
    fi
}
