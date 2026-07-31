# linux-fullscreen — mapa do projeto

Sessão Linux em ecrã cheio sobre WSL2, cobrindo o Windows, via xrdp + xfwm4.
**Sem desktop environment**: só o gerenciador de janelas, o `xfsettingsd`, um
terminal e uma barra própria. Premissa permanente: **RAM baixa**.

```
Windows                              WSL2 (Ubuntu-24.04)
mstsc.exe /multimon /f  <--RDP-->    xrdp :3390
                                       └─ Xorg :10 (driver xrdpdev, virtual)
                                            └─ xfwm4 + xfce4-terminal + barra
```

## Antes de ler o README

O `README.md` tem ~2500 linhas e é o **caderno de laboratório** do projeto: cada
decisão vem com a medição que a justifica e com correções datadas de conclusões
anteriores erradas. Ele é a fonte da verdade sobre o *porquê*.

**Não leia inteiro.** Use `grep -n '^## \|^### ' README.md` para o índice e leia
só a seção necessária. Ler tudo custa ~35 mil tokens e quase nunca é preciso.

## Arquivos

| Arquivo | O que é |
|---|---|
| `install.sh` | instalador principal, idempotente. Compila a barra no passo 2 |
| `startwm.sh` | → `/etc/xrdp/startwm.sh`. Sobe a sessão inteira; limpa vars do WSLg |
| `barra-tarefas.c` | **único componente compilado**. C + Xlib cru, 2,6 MB de RSS |
| `transferir-usb` | passa headset/webcam entre Windows e Linux (usbipd) |
| `camera-rede` | **a webcam de verdade**: ponte de vídeo por rede, sem tirar do Windows |
| `compilar-v4l2loopback` | refaz o módulo quando a WSL troca de kernel (senão a câmera some) |
| `v4l2loopback.service` | carrega o módulo no boot; **não** use `/lib/modules`, a WSL apaga |
| `audio-dispositivos` | lista/escolhe saída e entrada, juntando Linux e Windows |
| `jogo-windows` | encolhe a sessão para um monitor e cede outro ao jogo |
| `linux-desktop-down` | "botão de desligar": encerra sessão ou a VM |
| `xfwm-atalhos.sh` | atalhos de janela; reaplicado a cada login pelo `startwm.sh` |
| `instalar-som.sh` | compila o `pulseaudio-module-xrdp` + ajustes de buffer |
| `instalar-xrdp010.sh` | troca o xrdp da distro pelo 0.10 compilado (o mais arriscado) |
| `windows/audio-padrao.ps1` | troca o dispositivo padrão do Windows (COM `IPolicyConfig`) |
| `windows/assinar-rdp.ps1` | assina o `.rdp`; roda **fora** da sessão, uma vez |
| `Untitled.png` | o diálogo de login do xrdp — **referência visual da barra** |

## O grafo (graphify)

Há um grafo de conhecimento do código em `graphify-out/` — derivado, está no
`.gitignore`. Hoje: 121 nós, 205 arestas, 15 comunidades nomeadas.

```bash
graphify update .                      # apos mexer no codigo. Zero tokens
graphify . --code-only                 # construir do zero
graphify explain "aplicar_volume()"    # o que e, quem chama, o que chama
graphify query "como o volume e aplicado"
firefox graphify-out/graph.html        # o mapa visual
```

**`--code-only` é obrigatório ao construir.** Sem ele o graphify classifica o
`README.md` como conteúdo semântico e o manda para um LLM. Isso é gasto sem
retorno: o valor do README é a prosa curada, não um resumo automático dela.

O binário vive em `~/.local/bin/graphify` (instalado com `pip install --user`) e
depende do PATH — `command not found` num terminal significa shell antigo, não
falta de conda. O shebang aponta para o Python do miniconda por caminho
absoluto, então **não é preciso `conda activate`**.

**Quem nomeia as comunidades é o assistente**, não uma API: é o passo 5 do
`~/.claude/skills/graphify/SKILL.md`. Os nomes ficam em
`graphify-out/.graphify_labels.json`; `--no-label` pula esse passo e deixa
"Community N".

Invocação pela skill, depois de um `/clear`:

| O que digitar | O que acontece |
|---|---|
| `/graphify <pergunta>` | usa o grafo pronto, só consulta — **o caso normal** |
| `/graphify . --update` | reprocessa só o que mudou |
| `/graphify .` sozinho | pipeline completo — **inclui ler o README inteiro** |

### O que o grafo NÃO mostra

Ele mapeia funções por arquivo, mas reporta *"all connections are within the
same source files"* — verdade e enganoso ao mesmo tempo: **os componentes se
chamam por nome de comando**, que o AST não resolve. `graphify path` entre dois
scripts responde "No path found" mesmo quando um executa o outro. A cadeia real:

```
barra-tarefas.c  ──exec──>  transferir-usb  ──exec──>  audio-padrao.ps1 (interop)
       │                          │
       └────exec────>  audio-dispositivos  ──exec──>  audio-padrao.ps1
       └────exec────>  linux-desktop-down  ──exec──>  transferir-usb
```

Com 15 arquivos, `grep` ainda acha qualquer coisa mais rápido que o grafo. Ele
passa a ganhar quando houver muitos arquivos numa linguagem só, onde as chamadas
entre eles são visíveis ao AST.

## Invariantes — quebrar qualquer uma destas quebra o ambiente

| Regra | Por quê |
|---|---|
| `max_bpp=32` no `xrdp.ini` | com 24 o servidor **recusa** o GFX/H.264 e cai no NSCodec |
| `Policy=Default` no `sesman.ini` | é o que faz a sessão sobreviver a fechar o mstsc. Com `UBD`, reconectar abre desktop vazio e **destrói trabalho** |
| `cycle_hidden=true` no xfwm4 | a barra **não** tem lista de janelas; `Alt+Tab` é o único caminho de volta ao minimizar |
| uma tecla por ação do xfwm4 | duas teclas na mesma ação = só uma pega o grab, e a vencedora é a última gravada |
| rótulos da barra em ASCII | a fonte core está em `iso8859-1`; acento sai corrompido no `XDrawString` |
| barra **não** é `override_redirect` | janela não gerenciada não tem strut: o xfwm4 ignora o `_NET_WM_STRUT_PARTIAL` e as janelas maximizadas voltam a passar por baixo dela, sem erro nenhum |
| `guiApplications=false` | com o WSLg ligado, apps GTK/Qt fogem para o desktop do Windows |
| `allowed_users=anybody` | volta para `console` a cada upgrade do `xserver-xorg-legacy` e a sessão para de subir |
| vídeo **nunca** por USB/IP | o `vhci_hcd` satura em 0,25 MB/s e o navegador pede 18,4 — dá chuvisco. Áudio cabe (0,18), vídeo não |

## Armadilhas do ambiente (custaram horas)

- **`.rdp` da Área de Trabalho é UTF-16LE.** `sed`/`grep` ASCII não casam e
  **não avisam**. Editar por bytes. Mexer nele invalida a assinatura → re-assinar
  com `rdpsign`.
- **Saída do PowerShell não é UTF-8** (CP-850/1252). `grep` declara
  `binary file matches` e engole tudo. Use `grep -a` + `tr -cd '[:print:]\t\n'`.
- **Lançar `.exe` do Windows:** `setsid --fork`, nunca `( cmd & )`. O proxy de
  interop segura o descritor de saída original e quem estiver **lendo** trava
  até a janela do Windows fechar.
- **`pgrep -c` conta zumbi.** Filtre com `grep -v defunct` antes de concluir que
  há duas instâncias.
- **`install.sh` derruba a sessão** (reinicia o xrdp). Para trocar só o
  `startwm.sh`, copie o arquivo com `sudo cp` em vez de rodar o instalador.
- **O xrdp às vezes não marca monitor primário** (`xrandr` sem o `*`). Desempate
  por posição: o monitor que contém a origem `(0,0)`.

## Comandos frequentes

```bash
# recompilar e reinstalar a barra
gcc -O2 -Wall -o barra-tarefas barra-tarefas.c -lX11 -lXrandr
sudo install -m 755 barra-tarefas /usr/local/bin/barra-tarefas
pkill -x barra-tarefas; setsid --fork /usr/local/bin/barra-tarefas </dev/null >/dev/null 2>&1

# ambiente da sessão gráfica, visto de um shell qualquer
P=$(pgrep -x xfwm4 | head -1); tr '\0' '\n' < /proc/$P/environ | grep -E 'DISPLAY|XAUTH|DBUS'

# estado do áudio/câmera
transferir-usb estado
audio-dispositivos listar saida
```

`DISPLAY=:10.0`, `XAUTHORITY=$HOME/.Xauthority`,
`PULSE_RUNTIME_PATH=/run/user/1000/pulse`.

## Como trabalhar aqui

- **Medir antes de concluir.** Este projeto já registrou várias conclusões
  erradas por não medir; as correções datadas no README existem para isso. Se
  uma explicação não foi verificada, diga que não foi.
- **Editar no repositório, nunca no destino instalado.** O `install.sh` espalha
  cópias; editar em `/etc` ou `/usr/local/bin` se perde na próxima instalação.
- **Nada de comando manual por sessão de uso.** Se uma solução exige rodar algo
  toda vez, ou tira hardware do Windows, ela é recusada — mesmo funcionando.
- **Documentar no README** o que foi medido, com data, incluindo o que se
  provou falso.
