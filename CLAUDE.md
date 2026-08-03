# linux-fullscreen — mapa do projeto

Sessão Linux em ecrã cheio sobre WSL2, cobrindo o Windows, via xrdp + xfwm4.
**Sem desktop environment**: só o gerenciador de janelas, o `xfsettingsd`, um
terminal e uma barra própria. Premissa permanente: **RAM baixa**.

```
Windows                              WSL2 (Ubuntu-24.04)
mstsc.exe /multimon /f  <--RDP-->    xrdp :3390
                                       └─ Xorg :10 (driver xrdpdev, virtual)
                                            └─ xfwm4 + barra (sem terminal)
```

## Antes de ler o README

O `README.md` tem ~4400 linhas e é o **caderno de laboratório** do projeto: cada
decisão vem com a medição que a justifica e com correções datadas de conclusões
anteriores erradas. Ele é a fonte da verdade sobre o *porquê*.

**Não leia inteiro.** Use `grep -n '^## \|^### ' README.md` para o índice e leia
só a seção necessária. Ler tudo custa ~35 mil tokens e quase nunca é preciso.

## Arquivos

| Arquivo | O que é |
|---|---|
| `install.sh` | instalador principal, idempotente. Compila a barra no passo 2 |
| `startwm.sh` | → `/etc/xrdp/startwm.sh`. Sobe a sessão inteira; limpa vars do WSLg |
| `barra-tarefas.c` | C + Xlib cru, 2,9 MB de RSS (387 kB de PSS). Fonte **core**, iso8859-1. **Escura** desde 03/08/2026 (`#define BARRA_ESCURA`; a paleta clara amostrada do `Untitled.png` continua no código, atrás do `#else`), relógio em `fixed-bold-13` |
| `bancada.c` | o "VS Code" próprio: árvore + **abas** + editor + xterm embutido com o Claude (que é a aba 0, **aberta sob demanda**). C + Xlib + **Xft**, 2,4 MB de PSS sem o Claude. Binário abre em **hex** e imagem em **preview**, ambos só-leitura. Sessão por projeto em `~/.config/bancada/sessoes/`. Desde 02/08/2026 é um editor de texto normal: seleção com mouse e `Shift`+setas, `Ctrl+C/X/V/A`, digitar substitui a marca, `Alt+↑/↓` move a linha, `Ctrl+F` procura (barrinha no lugar do título, `Enter`/`F3`/`Ctrl+G` andam), `Ctrl+Shift+Z`/`Ctrl+Y` refazem, e o painel tem **coluna de números** à esquerda e **barra de rolagem** arrastável à direita. Dona de PRIMARY e CLIPBOARD |
| `terminal.c` | emulador de terminal próprio, do pty ao pixel. C + Xlib + Xft, 2,66 MB de PSS contra 8,12 do xterm. É o terminal **da sessão** desde 02/08/2026 (`Ctrl+Alt+T`), mas **não** o da bancada: a aba 0 segue no xterm. `SIGUSR1` despeja a grade em texto |
| `panorama.c` | **aperta Win e vê tudo**: grade das janelas abertas, cada uma com a miniatura viva, em 0,6 MB de PSS parado. Daemon, sobe no `startwm.sh`. Detecta a tecla Win sozinha com **XInput2 cru** (sem grab, senão roubaria o Super do xfwm4); a miniatura vem do **Composite** redirecionado, escalada pelo **XRender** dentro do servidor. Desde 03/08/2026 |
| `perfil.sh` | o pedaço do projeto que entra no shell: a função `bancada`, que abre na pasta atual e devolve o prompt, como o `code .` |
| `barra-apps` | atalhos de aplicativo da barra: lê `.desktop`, converte o ícone e mantém o `apps.conf`. Programa nosso só entra na lista do `[+]` se tiver um `.desktop` em `desktop/` |
| `trabalho` | substitui o VS Code: ranger + Claude Code lado a lado num tmux. `trabalho instalar` cria os `.desktop` |
| `transferir-usb` | passa headset/webcam entre Windows e Linux (usbipd) |
| `camera-rede` | **a webcam de verdade**: ponte de vídeo por rede, sem tirar do Windows |
| `compilar-v4l2loopback` | refaz o módulo quando a WSL troca de kernel (senão a câmera some) |
| `v4l2loopback.service` | carrega o módulo no boot; **não** use `/lib/modules`, a WSL apaga |
| `audio-dispositivos` | lista/escolhe saída e entrada, juntando Linux e Windows |
| `abrir-windows` | encolhe a sessão para um monitor e cede outro ao Windows. A lista é a pasta `Coisas` da área de trabalho do Windows, e aceita **qualquer arquivo**: um `.docx` ali abre no Word de verdade. Chamava-se `jogo-windows`/`Jogos` até 02/08/2026 |
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
       └────exec────>  abrir-windows       ──exec──>  mstsc.exe / cmd.exe
       └────exec────>  barra-apps          ──exec──>  zenity / convert / o app
                                  └──(.desktop)──>  trabalho  ──exec──>  tmux + ranger + claude
```

Com 15 arquivos, `grep` ainda acha qualquer coisa mais rápido que o grafo. Ele
passa a ganhar quando houver muitos arquivos numa linguagem só, onde as chamadas
entre eles são visíveis ao AST.

## Invariantes — quebrar qualquer uma destas quebra o ambiente

| Regra | Por quê |
|---|---|
| a barra tem **seis** cores com papel nomeado, não quatro | na paleta clara `LUZ`/`POCO` eram os dois `#FFFFFF` e `BORDA`/`TINTA` os dois `#000000`. No escuro cada par se separa: o `gravado()` preenche com `POCO`, porque campo afundado tem de ficar **mais escuro** que a face, não mais claro |
| a cor do `-flatten` do `barra-apps` casa com a `FACE` da barra, e vai no **nome** do cache | a barra não sabe o que é alfa: o ícone chega achatado. Achatado sobre claro e desenhado sobre escuro põe um quadrado claro em volta de cada ícone. Com a cor no nome (`brave-browser-333333.rgb`), trocar a paleta invalida o cache sozinho — sem passo manual |
| a fonte do relógio é **monoespaçada** (o `c` no nome XLFD) | o texto é centralizado; com fonte proporcional os dígitos mudam de largura e o relógio dança de posição a cada minuto |
| o cartão do panorama **não** some no clique do `×` | quem manda tirá-lo é o `PropertyNotify` do `_NET_CLIENT_LIST`. O `_NET_CLOSE_WINDOW` é um pedido educado: o programa pode perguntar antes de sair e pode recusar. Sumir na hora seria mentir sobre isso — e nada de `XKillClient`, que perde o trabalho aberto sem avisar |
| a tecla Win do panorama **não** entra em `XGrabKey` nem no `xfce4-keyboard-shortcuts` | atalho do xfsettingsd exige combinação, e um passive grab em `Super_L` sem modificador captura a tecla **na descida**: o `Super+←` do tiling nunca mais chegaria ao xfwm4. É a mesma disputa de grab do `<Super>Right` morto. Quem detecta é o XInput2 com eventos **crus**, que chegam sem tirar a tecla de ninguém — o panorama observa, não intercepta |
| no panorama, um `PRESS` do Super **estando já armado** desarma o gesto | o xrdp entrega auto-repeat como pares `RELEASE`+`PRESS`, não como `PRESS` seguido (medido 03/08/2026). Sem essa regra, segurar a tecla Win pareceria um toque e o painel abriria sozinho |
| a miniatura sai do **frame**, não da janela do cliente | o `_NET_CLIENT_LIST` dá a janela do cliente, que o xfwm4 reparenta; quem é filho do root — e quem o redirect do Composite alcança — é o frame. Nomear o pixmap do cliente dá `BadMatch`. É o que o `ate_o_root()` resolve |
| o redirect do Composite é `Automatic`, nunca `Manual` | com `Automatic` o servidor continua compondo a tela sozinho: não viramos compositor, o xfwm4 não sabe de nada e o `use_compositing` dele continua `false`. Com `Manual` a sessão inteira passaria a depender deste processo para aparecer na tela |
| `sigaction` sem `SA_RESTART` no `SIGUSR1` do panorama | o `signal()` da glibc liga o `SA_RESTART` sozinho, e aí o `read()` de dentro do Xlib é reiniciado após o sinal: o `select` nunca volta e o sinal só tem efeito no próximo evento do X — tecla morta até alguém mexer o mouse |
| `max_bpp=32` no `xrdp.ini` | com 24 o servidor **recusa** o GFX/H.264 e cai no NSCodec |
| `Policy=Default` no `sesman.ini` | é o que faz a sessão sobreviver a fechar o mstsc. Com `UBD`, reconectar abre desktop vazio e **destrói trabalho** |
| `cycle_hidden=true` no xfwm4 | a barra **não** tem lista de janelas; `Alt+Tab` é o único caminho de volta ao minimizar |
| uma tecla por ação do xfwm4 | duas teclas na mesma ação = só uma pega o grab, e a vencedora é a última gravada |
| o valor do `Ctrl+Alt+T` sem aspas | a sessão nasce sem terminal, então essa tecla é o único caminho normal para um shell. Quem a executa é o `xfsettingsd`, que passa a string pelo `g_shell_parse_argv`: aspas no valor são uma camada a mais para errar, e o erro é a tecla **não fazer nada, calada**. O `if` de fallback fica no `xfwm-atalhos.sh`, na hora de gravar |
| rótulos da barra em ASCII | a fonte core está em `iso8859-1`; acento sai corrompido no `XDrawString` |
| `bancada.c` usa **Xft**, a barra usa fonte **core** | não unifique: na barra o iso8859-1 é o visual arcaico; num editor ele **corromperia** todo caractere fora do latin-1 ao salvar |
| `Ctrl+C` da bancada **não** entra no `XGrabKey` | com grab ele deixaria de chegar ao Claude da aba 0, onde é o "interrompe isso". Só o `Ctrl+Tab` é capturado; o resto depende do foco, que é o certo aqui |
| modificador sozinho sai cedo do `tecla()` | apertar `Ctrl` gera um `KeyPress` próprio, e o `state` de um evento X é o de **antes** dele: ali o `ControlMask` ainda não está ligado. Sem a saída, esse evento desce até a decisão da marca como "qualquer outra tecla" — nem movimento nem escrita, `n=0` — e cai no `sel_limpar()`. A seleção do mouse morria no `Ctrl`, antes do `C`, e o `Ctrl+C` copiava a linha do cursor. O intervalo `ISO_*` vai escrito à mão ao lado do `IsModifierKey()` porque essa parte do macro exige `<keysym.h>` **antes** do `<Xutil.h>`, e aqui é o contrário — sem ela o `AltGr` do ABNT2 fica de fora |
| `apagar_selecao()` **não** guarda instantâneo de desfazer | quem chama é que sabe se aquilo é um passo sozinho (Delete) ou a primeira metade de um par (recortar, colar/digitar por cima). Guardar ali faria o `Ctrl+Z` andar meio passo — desfaria o colado sem devolver o apagado |
| o texto colado passa por filtro | `\r` sai (senão `\r\n` do Windows vira **duas** quebras no `inserir_texto()`), e byte de controle abaixo de 32 que não seja `\n`/`\t` também. Quem decide `\r\n` no arquivo é a flag `crlf`, ao salvar |
| a seleção da bancada morre na troca de aba | ela vive nos globais do editor, como o cursor — depois da troca eles apontam para o buffer de outro arquivo, e o intervalo não quer dizer nada lá. A barrinha de procurar cai junto, pelo mesmo motivo: a âncora dela é uma posição no arquivo que saiu de cena. O **termo** fica, e o `F3` o reaproveita no arquivo novo |
| na bancada, resposta de filho vem **pelo pipe**, nunca do código de saída | o `main()` ignora `SIGCHLD` (senão xterm/`convert`/zenity viram zumbi), e com isso o `system()` devolve `-1`/`ECHILD` sempre. Ver a armadilha logo abaixo: era o que fazia toda pergunta responder "não" sozinha |
| ninguém escreve `MARGEM` como origem do texto do editor: é `texto_x0()` | a coluna dos números come a esquerda e a barra de rolagem come a direita, e a origem aparecia em **cinco** lugares (`desenhar_editor`, `desenhar_selecao`, o cursor, `pos_do_clique`, `seguir_cursor`). Um esquecido não dá erro: só põe o cursor uns pixels fora do texto, ou deixa a última coluna debaixo da barra. A largura útil é o `cols_cabem()`, a altura útil é o `linhas_vis()` |
| todo lugar que rola passa pelo `topo_max()`/`prender_topo()` | o teto é a última **tela**, não a última linha. Enquanto nada mostrava a posição, rolar até a última linha sozinha no alto era só feio; com a barra de rolagem desenhada vira mentira visível — o cursor dela desceria até o fim do trilho com o arquivo inteiro já na tela |
| os números e a barra de rolagem são desenhados **depois** do texto | linha comprida com o texto rolado passa por baixo da barra; pintá-la por cima é de graça. Cortar cada `XftDrawStringUtf8` na largura útil exigiria medir a linha inteira em caracteres a cada quadro |
| a saída da barra de rolagem vem antes dos outros ramos do clique no painel | aquele x também cai dentro do texto: sem ela o clique poria o cursor na última coluna da linha em vez de rolar |
| a barrinha de procurar fica na barra de ferramentas, não numa faixa no pé | uma faixa embaixo teria de roubar altura do texto, e a altura do texto é a conta que o `desenhar_editor()`, o `desenhar_selecao()`, o `seguir_cursor()`, o `pos_do_clique()`, o `PageUp/PageDown` e o arraste usam — **seis** lugares. Um esquecido não dá erro: só põe o cursor debaixo da faixa |
| o achado da busca **é** a seleção | de graça vêm o desenho, o `Ctrl+C` do achado e o digitar por cima. Um realce próprio poria dois intervalos marcados na tela, mentindo sobre qual deles o botão do meio cola |
| a busca incremental parte da **âncora**, não do achado anterior | senão cada letra a mais do termo avança na lista e o texto vai embora rolando enquanto se digita |
| `Ctrl+Shift+Z` sai do `ShiftMask`, nunca do keysym | com `CapsLock` ligado o `Ctrl+Z` chega como `XK_Z` **sem** Shift apertado; um `case XK_Z: refazer()` faria o desfazer virar refazer, calado, para quem estiver de CapsLock |
| `esquecer_refazer()` mora dentro do `guardar_instante()` | é o único ponto chamado *antes de cada edição*, e é aí que o futuro tem de morrer. Fora dali, o `Ctrl+Y` reconstruiria um texto que nunca foi escrito. O par desfazer/refazer não dobra a memória: cada passo **tira de um anel e põe no outro** |
| só a janela-mãe pede `KeyPressMask` na bancada | o X entrega a tecla à menor janela sob o ponteiro que a pediu, não à que tem foco. Com o filho pedindo, o `XFilterEvent` não reconhece o evento e o **acento morto do ABNT2 para de compor** |
| a bancada precisa de `XSetErrorHandler` | trocar de aba desmapeia uma janela e mapeia outra; `XSetInputFocus` na recém-desmapeada dá `BadMatch`, e o tratador padrão do Xlib **mata o processo** |
| o `terminal.c` **não** entra na bancada | ele existe por gosto de ter o próprio, não para trocar o xterm da aba 0 — que funciona. Mexer nisso traz de volta todo o trabalho das invariantes de janela adotada, para economizar 14 MB num par que gasta 195 |
| a reserva de fonte parte do pedido **cru**, nunca de `fonte->pattern` | o pattern de uma fonte já aberta é o resolvido: traz `FC_FILE` e `FC_FONTVERSION`, ambos de prioridade **maior** que o charset. O `FcFontMatch` devolve a mesma fonte, e CJK/emoji ficam caixa vazia com as Noto instaladas e funcionando |
| `setsid()` **antes** do `TIOCSCTTY` no filho do pty | sem virar líder de sessão primeiro, o pty não vira terminal de *controle*: tudo funciona e só o `Ctrl+C` não faz nada |
| `globais_zerar()` e `fechar_aba()` **não** usam `limpar_buffer`/`esquecer_undo` | eles mexem nos globais, que apontam para o buffer de outra aba. Liberar ali destrói o texto da aba vizinha, e só aparece ao voltar para ela |
| nada de `seguir_cursor()` ao ativar aba | ele arrasta a tela até o cursor, que só anda pelo teclado — desfaz a rolagem de quem desceu o arquivo com a roda do mouse. O `topo`/`col0` guardados na aba já são válidos |
| a bancada **não** levanta o Claude sozinha | subir só com a janela custava 490 MB a quem abriu para olhar um arquivo. Só o botão `[Claude]` e o clique no painel vazio o abrem; abrir a bancada, restaurar sessão e `Ctrl+Tab` não. Trocar de projeto reabre **só** o que já estava de pé |
| `salvar()` recusa `modo != MODO_TEXTO` | hex e imagem não têm buffer de linhas: o `Ctrl+S` percorreria zero linhas, e o `fopen(…,"wb")` **já truncou** antes disso. Um `.png` de 27 KB viraria 0 byte, calado |
| qualquer `NUL` no arquivo **inteiro** manda para o hex | o cheiro olha só 512 bytes, mas o `texto_para_buffer()` varre com `strchr()`: um `NUL` no meio faria o resto do arquivo sumir do buffer e o salvar gravá-lo truncado |
| nenhuma lib de imagem no `bancada.c` também | quem decodifica é o `convert` (16 MB de pico, 0,03 s, e morre), devolvendo PPM cru pelo pipe. A `libpng` até já está no processo pela freetype, mas resolveria só PNG — pelo pipe vêm seis formatos num caminho só |
| a sessão da bancada **não** guarda texto não salvo | só caminho, cursor e rolagem. Ressuscitar texto que o usuário não mandou gravar tem de acertar sempre; errar uma vez custa o arquivo |
| o `perfil.sh`, o `audio-padrao.ps1` e o `xfwm-atalhos.sh` são instalados **fora** do repositório (`/usr/local/share/linux-fullscreen/`) | se o `.bashrc` apontasse para a pasta do repositório, mover o repositório quebraria o shell. Vale igual para os outros dois: mover a pasta derrubava a troca de áudio do Windows e, calado, os atalhos de janela no login. Quem os usa procura no instalado primeiro, com o repositório como plano B. E não é `/etc/profile.d`: terminal interativo não-login lê o `.bashrc` e ignora o `profile.d` |
| nenhuma lib de imagem no `barra-tarefas.c` | o ícone é convertido pelo `barra-apps` para pixels crus (1200 bytes) uma vez, ao fixar. A barra só faz `fread` + `XPutImage`. Linkar libpng aqui é o fim da premissa do arquivo |
| dicas de tamanho **antes** do `XResizeWindow` | elas dizem `min=max`; sem atualizar primeiro, o xfwm4 recusa a largura nova e o conteúdo transborda |
| barra **não** é `override_redirect` | janela não gerenciada não tem strut: o xfwm4 ignora o `_NET_WM_STRUT_PARTIAL` e as janelas maximizadas voltam a passar por baixo dela, sem erro nenhum |
| `guiApplications=false` | com o WSLg ligado, apps GTK/Qt fogem para o desktop do Windows |
| `allowed_users=anybody` | volta para `console` a cada upgrade do `xserver-xorg-legacy` e a sessão para de subir |
| vídeo **nunca** por USB/IP | o `vhci_hcd` satura em 0,25 MB/s e o navegador pede 18,4 — dá chuvisco. Áudio cabe (0,18), vídeo não |
| lançar `.exe` com argumentos separados | a interop do WSL **escapa aspas embutidas**: `start "" $linha` com aspas dentro da string vira `"\"C:\Riot" Games\...` no Windows. Cada pedaço tem de ser um argumento do bash |
| nada de interop no caminho do menu | a barra chama `abrir-windows --listar-coisas` a cada abertura; um `powershell.exe` ali custaria ~1 s por clique |
| o filtro da pasta `Coisas` é lista **negra**, não branca | numa pasta que aceita qualquer arquivo, uma lista de extensões aceitas decide por você que `.docx` não é coisa — e o arquivo largado lá **some do menu sem dizer nada**. Só o lixo do Windows (`desktop.ini`, `Thumbs.db`, ocultos) fica de fora |

## Armadilhas do ambiente (custaram horas)

- **`.rdp` da Área de Trabalho é UTF-16LE.** `sed`/`grep` ASCII não casam e
  **não avisam**. Editar por bytes. Mexer nele invalida a assinatura → re-assinar
  com `rdpsign`. **Quem o converte é o próprio `rdpsign`**: nasce ASCII, volta
  UTF-16LE assinado. Passe por `ler_rdp()` (`abrir-windows`) antes de filtrar —
  sem isso o `grep` responde `binary file matches` no stderr e nada no stdout, e
  o resultado é um `.rdp` de 53 bytes que ninguém percebe.
- **Nada de caminho chumbado na Área de Trabalho.** Arrumar o desktop numa
  subpasta é normal e já quebrou **três** coisas: o `abrir-windows` (erro
  visível), o `.vbs` (queda silenciosa para `mstsc /f` sem os ajustes) e, dois
  dias depois, o `transferir-usb` — que matava o mstsc para trocar o áudio e
  **não conseguia reabri-lo**, deixando a sessão desconectada. Os três procuram
  um nível de subpasta abaixo, nas três variantes de área de trabalho. Quando um
  caminho chumbado quebrar, **procure os outros no mesmo dia**.
- **Saída do PowerShell não é UTF-8** (CP-850/1252). `grep` declara
  `binary file matches` e engole tudo. Use `grep -a` + `tr -cd '[:print:]\t\n'`.
- **Lançar `.exe` do Windows:** `setsid --fork`, nunca `( cmd & )`. O proxy de
  interop segura o descritor de saída original e quem estiver **lendo** trava
  até a janela do Windows fechar.
- ~~**O binário do `claude` mora dentro da extensão do VS Code.**~~ **Não vale mais** (medido em 02/08/2026): o CLI está instalado sozinho em `~/.local/share/claude/versions/`, e `trabalho onde` responde `~/.local/bin/claude`. Desinstalar o VS Code **não** leva o Claude junto. O `onde_claude()` tenta `command -v claude` primeiro e só cai na extensão como plano B — continue usando ele em vez de chumbar caminho.
- **Opções de janela do `xfce4-terminal` vêm antes das de aba.** `--maximize` depois de `--title` é ignorado **sem erro** — a janela abre no tamanho padrão.
- **`gcc ... | head` mata o compilador no meio, e o binário ANTIGO fica.** Medido
  em 03/08/2026: `gcc -O2 -Wall -Wextra -o barra-tarefas barra-tarefas.c 2>&1 |
  head -10` — o `-Wextra` passou de 10 linhas de aviso, o `head` fechou o pipe, o
  gcc levou **SIGPIPE** e morreu antes de gravar o binário. O `; echo "compilou"`
  logo depois imprimiu do mesmo jeito (o `echo` não vê o código de saída de quem
  veio antes do `;`), então a barra antiga foi instalada e a paleta nova "não
  teve efeito". O sintoma manda procurar no lugar errado — no código, na
  instalação, no processo. **Nunca truncar a saída de um compilador**: para
  encurtar, `2>&1 | tail -20` (o tail lê tudo antes de imprimir) ou `; echo
  "saida: $?"` para ver o código de verdade. Confirmar pelo artefato também vale:
  `strings binario | grep '#333333'`.
- **`pgrep -c` conta zumbi.** Filtre com `grep -v defunct` antes de concluir que
  há duas instâncias.
- **`signal(SIGCHLD, SIG_IGN)` faz o `system()` devolver sempre `-1`** (medido em
  02/08/2026). Com essa disposição o kernel recolhe o filho sozinho e o `wait()`
  de dentro do `system()` não acha ninguém: volta `-1`/`ECHILD` tenha o comando
  dado certo ou errado. O `bancada.c` ignora `SIGCHLD` para o xterm/`convert`/
  zenity não virarem zumbi, e por isso o `return system(cmd) == 0` do `zenity()`
  era **falso o tempo todo**: toda pergunta respondia "não" por baixo do clique —
  aba suja não fechava ao clicar "Sim", e a janela com aba suja não saía. Quem
  precisa da resposta de um filho **lê pelo pipe** (`popen` + `&& echo marca`),
  nunca pelo código de saída. O `popen` sempre funcionou aqui; o `pclose`
  devolvendo `-1` não atrapalha ninguém.
- **Numa sessão xrdp o `CLIPBOARD` tem dono o tempo todo** (medido em
  03/08/2026): é o `xrdp-chansrv`, espelhando o Windows — `xclip -o -selection
  clipboard` devolvia um caminho em `thinclient_drives/.clipboard/`. Todo plano B
  do tipo "se ninguém for dono do CLIPBOARD, tenta o PRIMARY" **nunca dispara**
  aqui. Era o que fazia o `Ctrl+V` da bancada colar um arquivo do Windows no
  lugar do que se marcou no terminal.
- **O Claude Code liga o relato de mouse** (`\033[?1000h`/`\033[?1006h` no
  binário): arrastar dentro da aba 0 manda o gesto ao programa e o xterm nem
  marca. Segure `Shift` para o xterm interceptar; `Ctrl+Shift+C` copia para o
  CLIPBOARD desde 03/08/2026 (não vem de fábrica no xterm 390).
- **Para testar `-xrm`/janela no display vivo, `xterm -iconic … -e true`**: roda
  o caminho inteiro sem janela piscando na tela de quem está usando a sessão. E
  teste com **controle** — silêncio só prova algo se o erro for possível.
- **Não tire screenshot no meio de um gesto com menu aberto.** O `import` quebra
  o `XGrabPointer` do menu da barra; o clique seguinte vai para a janela da barra
  com coordenadas relativas a ela, o menu fecha sem escolher, e parece bug de
  código. Capture antes ou depois — para conferir o gesto, instrumente.
- **Capturar uma janela coberta não devolve o conteúdo dela.** Sem compositor, o
  conteúdo obscurecido é indefinido — e medido em 03/08/2026, com duas janelas
  controladas (uma 100% em cima da outra), o que volta são **zeros**: nem o
  conteúdo dela, nem o de quem está por cima, como este arquivo dizia antes. Testando a bancada com uma segunda instância que nasceu em `+1280+29`,
  quase em cima da que estava em uso (`+1285+29`), a captura mostrou arquivos
  abrindo rolados no meio — um bug que **não existia**: a instrumentação dizia
  `topo=0`. Um navegador aberto num monitor deixou outra captura inteiramente
  preta. Antes de acreditar num print de teste, confira que a janela está
  visível (`xwininfo -root -children` dá o empilhamento) — ou, melhor, meça o
  estado por `fprintf` em vez de olhar a tela.
- **Para testar programa X sem tocar na sessão viva** (02/08/2026): `#define main
  <prog>_main` + `#include "<prog>.c"` num arquivo de teste alcança os `static`
  de dentro. Monte o display de verdade e **não mapeie janela nenhuma**: o
  desenho roda inteiro e não vira pixel. Para *ver* um widget, desenhe-o num
  **Pixmap** e leia de lá — Pixmap não pode ser encoberto. Nada de `XTest` no
  display vivo: ele injeta no servidor e a tecla vai para a janela com foco, que
  pode ser um arquivo aberto do usuário.
- ~~**`install.sh` derruba a sessão**~~ — **ele a duplica** (medido em
  02/08/2026). Reiniciar o xrdp apaga a tabela de sessões, que vive na memória do
  sesman; no reconnect ele abre um display novo e o antigo fica **órfão**, vivo e
  sem `sesexec` que o recolha. Ficou de pé 11 h segurando **1,66 GB**. O
  `Policy=Default` não cobre este caso: ele protege contra o mstsc fechar, não
  contra o sesman reiniciar. Sintoma: dois `Xorg :` no `ps`. Limpeza sem
  reiniciar nada: `kill -TERM` no `xfwm4` e no `Xorg` do display velho (os
  clientes X caem junto) e depois no `xrdp-chansrv`, que não cai. Processo com
  `PPID 1` + sessão própria + `tty ?` sobrevive — foi como um `gmx mdrun` de 11 h
  atravessou a limpeza intacto. Ver README, "Mas o `Policy=Default` não protege
  contra o *sesman* reiniciar". Para trocar só o `startwm.sh`, copie o arquivo
  com `sudo cp` em vez de rodar o instalador.
- **O xrdp às vezes não marca monitor primário** (`xrandr` sem o `*`). Desempate
  por posição: o monitor que contém a origem `(0,0)`.

## Comandos frequentes

```bash
# recompilar e reinstalar a barra
gcc -O2 -Wall -o barra-tarefas barra-tarefas.c -lX11 -lXrandr
sudo install -m 755 barra-tarefas /usr/local/bin/barra-tarefas
pkill -x barra-tarefas; setsid --fork /usr/local/bin/barra-tarefas </dev/null >/dev/null 2>&1

# recompilar e reinstalar o panorama
gcc -O2 -Wall -o panorama panorama.c $(pkg-config --cflags xft) \
    -lX11 -lXft -lXi -lXrandr -lXcomposite -lXrender -lm
sudo install -m 755 panorama /usr/local/bin/panorama
pkill -x panorama; setsid --fork /usr/local/bin/panorama </dev/null >/dev/null 2>&1

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
