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
| `barra-tarefas.c` | C + Xlib cru, 3,0 MB de RSS. Fonte **core**, iso8859-1 |
| `bancada.c` | o "VS Code" próprio: árvore + **abas** + editor + xterm embutido com o Claude (que é a aba 0, **aberta sob demanda**). C + Xlib + **Xft**, 2,4 MB de PSS sem o Claude. Binário abre em **hex** e imagem em **preview**, ambos só-leitura. Sessão por projeto em `~/.config/bancada/sessoes/` |
| `perfil.sh` | o pedaço do projeto que entra no shell: a função `bancada`, que abre na pasta atual e devolve o prompt, como o `code .` |
| `barra-apps` | atalhos de aplicativo da barra: lê `.desktop`, converte o ícone e mantém o `apps.conf` |
| `trabalho` | substitui o VS Code: ranger + Claude Code lado a lado num tmux. `trabalho instalar` cria os `.desktop` |
| `transferir-usb` | passa headset/webcam entre Windows e Linux (usbipd) |
| `camera-rede` | **a webcam de verdade**: ponte de vídeo por rede, sem tirar do Windows |
| `compilar-v4l2loopback` | refaz o módulo quando a WSL troca de kernel (senão a câmera some) |
| `v4l2loopback.service` | carrega o módulo no boot; **não** use `/lib/modules`, a WSL apaga |
| `audio-dispositivos` | lista/escolhe saída e entrada, juntando Linux e Windows |
| `jogo-windows` | encolhe a sessão para um monitor e cede outro ao jogo. A lista de jogos é a pasta `Jogos` da área de trabalho do Windows |
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
       └────exec────>  jogo-windows        ──exec──>  mstsc.exe / cmd.exe
       └────exec────>  barra-apps          ──exec──>  zenity / convert / o app
                                  └──(.desktop)──>  trabalho  ──exec──>  tmux + ranger + claude
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
| `bancada.c` usa **Xft**, a barra usa fonte **core** | não unifique: na barra o iso8859-1 é o visual arcaico; num editor ele **corromperia** todo caractere fora do latin-1 ao salvar |
| só a janela-mãe pede `KeyPressMask` na bancada | o X entrega a tecla à menor janela sob o ponteiro que a pediu, não à que tem foco. Com o filho pedindo, o `XFilterEvent` não reconhece o evento e o **acento morto do ABNT2 para de compor** |
| a bancada precisa de `XSetErrorHandler` | trocar de aba desmapeia uma janela e mapeia outra; `XSetInputFocus` na recém-desmapeada dá `BadMatch`, e o tratador padrão do Xlib **mata o processo** |
| `globais_zerar()` e `fechar_aba()` **não** usam `limpar_buffer`/`esquecer_undo` | eles mexem nos globais, que apontam para o buffer de outra aba. Liberar ali destrói o texto da aba vizinha, e só aparece ao voltar para ela |
| nada de `seguir_cursor()` ao ativar aba | ele arrasta a tela até o cursor, que só anda pelo teclado — desfaz a rolagem de quem desceu o arquivo com a roda do mouse. O `topo`/`col0` guardados na aba já são válidos |
| a bancada **não** levanta o Claude sozinha | subir só com a janela custava 490 MB a quem abriu para olhar um arquivo. Só o botão `[Claude]` e o clique no painel vazio o abrem; abrir a bancada, restaurar sessão e `Ctrl+Tab` não. Trocar de projeto reabre **só** o que já estava de pé |
| `salvar()` recusa `modo != MODO_TEXTO` | hex e imagem não têm buffer de linhas: o `Ctrl+S` percorreria zero linhas, e o `fopen(…,"wb")` **já truncou** antes disso. Um `.png` de 27 KB viraria 0 byte, calado |
| qualquer `NUL` no arquivo **inteiro** manda para o hex | o cheiro olha só 512 bytes, mas o `texto_para_buffer()` varre com `strchr()`: um `NUL` no meio faria o resto do arquivo sumir do buffer e o salvar gravá-lo truncado |
| nenhuma lib de imagem no `bancada.c` também | quem decodifica é o `convert` (16 MB de pico, 0,03 s, e morre), devolvendo PPM cru pelo pipe. A `libpng` até já está no processo pela freetype, mas resolveria só PNG — pelo pipe vêm seis formatos num caminho só |
| a sessão da bancada **não** guarda texto não salvo | só caminho, cursor e rolagem. Ressuscitar texto que o usuário não mandou gravar tem de acertar sempre; errar uma vez custa o arquivo |
| o `perfil.sh` é instalado **fora** do repositório | se o `.bashrc` apontasse para a pasta do repositório, mover o repositório quebraria o shell. E não é `/etc/profile.d`: terminal interativo não-login lê o `.bashrc` e ignora o `profile.d` |
| nenhuma lib de imagem no `barra-tarefas.c` | o ícone é convertido pelo `barra-apps` para pixels crus (1200 bytes) uma vez, ao fixar. A barra só faz `fread` + `XPutImage`. Linkar libpng aqui é o fim da premissa do arquivo |
| dicas de tamanho **antes** do `XResizeWindow` | elas dizem `min=max`; sem atualizar primeiro, o xfwm4 recusa a largura nova e o conteúdo transborda |
| barra **não** é `override_redirect` | janela não gerenciada não tem strut: o xfwm4 ignora o `_NET_WM_STRUT_PARTIAL` e as janelas maximizadas voltam a passar por baixo dela, sem erro nenhum |
| `guiApplications=false` | com o WSLg ligado, apps GTK/Qt fogem para o desktop do Windows |
| `allowed_users=anybody` | volta para `console` a cada upgrade do `xserver-xorg-legacy` e a sessão para de subir |
| vídeo **nunca** por USB/IP | o `vhci_hcd` satura em 0,25 MB/s e o navegador pede 18,4 — dá chuvisco. Áudio cabe (0,18), vídeo não |
| lançar `.exe` com argumentos separados | a interop do WSL **escapa aspas embutidas**: `start "" $linha` com aspas dentro da string vira `"\"C:\Riot" Games\...` no Windows. Cada pedaço tem de ser um argumento do bash |
| nada de interop no caminho do menu | a barra chama `jogo-windows --listar-jogos` a cada abertura; um `powershell.exe` ali custaria ~1 s por clique |

## Armadilhas do ambiente (custaram horas)

- **`.rdp` da Área de Trabalho é UTF-16LE.** `sed`/`grep` ASCII não casam e
  **não avisam**. Editar por bytes. Mexer nele invalida a assinatura → re-assinar
  com `rdpsign`. **Quem o converte é o próprio `rdpsign`**: nasce ASCII, volta
  UTF-16LE assinado. Passe por `ler_rdp()` (`jogo-windows`) antes de filtrar —
  sem isso o `grep` responde `binary file matches` no stderr e nada no stdout, e
  o resultado é um `.rdp` de 53 bytes que ninguém percebe.
- **Nada de caminho chumbado na Área de Trabalho.** Arrumar o desktop numa
  subpasta é normal e já quebrou o `jogo-windows` (erro visível) e o `.vbs`
  (queda silenciosa para `mstsc /f` sem os ajustes). Os dois procuram um nível
  de subpasta abaixo.
- **Saída do PowerShell não é UTF-8** (CP-850/1252). `grep` declara
  `binary file matches` e engole tudo. Use `grep -a` + `tr -cd '[:print:]\t\n'`.
- **Lançar `.exe` do Windows:** `setsid --fork`, nunca `( cmd & )`. O proxy de
  interop segura o descritor de saída original e quem estiver **lendo** trava
  até a janela do Windows fechar.
- **O binário do `claude` mora dentro da extensão do VS Code** (`~/.vscode/extensions/anthropic.claude-code-<versao>/resources/native-binary/claude`). O caminho tem a versão e muda a cada atualização; desinstalar o VS Code leva o Claude junto. O `trabalho` procura em vez de chumbar.
- **Opções de janela do `xfce4-terminal` vêm antes das de aba.** `--maximize` depois de `--title` é ignorado **sem erro** — a janela abre no tamanho padrão.
- **`pgrep -c` conta zumbi.** Filtre com `grep -v defunct` antes de concluir que
  há duas instâncias.
- **Não tire screenshot no meio de um gesto com menu aberto.** O `import` quebra
  o `XGrabPointer` do menu da barra; o clique seguinte vai para a janela da barra
  com coordenadas relativas a ela, o menu fecha sem escolher, e parece bug de
  código. Capture antes ou depois — para conferir o gesto, instrumente.
- **`import -window <id>` mente quando a janela está coberta.** Sem compositor, o
  conteúdo obscurecido é indefinido: o `import` devolve os pixels de quem está
  por cima. Testando a bancada com uma segunda instância que nasceu em `+1280+29`,
  quase em cima da que estava em uso (`+1285+29`), a captura mostrou arquivos
  abrindo rolados no meio — um bug que **não existia**: a instrumentação dizia
  `topo=0`. Um navegador aberto num monitor deixou outra captura inteiramente
  preta. Antes de acreditar num print de teste, confira que a janela está
  visível (`xwininfo -root -children` dá o empilhamento) — ou, melhor, meça o
  estado por `fprintf` em vez de olhar a tela.
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
