# Linux fullscreen sobre WSL2 (xrdp + xfwm4)

Sessão Linux ocupando todos os monitores, cobrindo o Windows. Tela limpa: sem
ícones de área de trabalho e **sem desktop environment**.

As janelas se comportam como no GNOME ou no Windows: barra de título com os
botões de fechar/minimizar/maximizar, arrastar com o mouse, snap ao encostar na
borda, `Alt+Tab`, `Super+setas` para meia tela.

Isso vem do **xfwm4**, o gerenciador de janelas do XFCE, rodando sozinho — sem
`xfce4-session`, sem `xfdesktop`, sem painel do XFCE. Só ele, o `xfsettingsd`
(que executa os atalhos de comando), um terminal e uma barra própria de 2,6 MB.

> **Correção da premissa (30/07/2026).** Estas linhas diziam *"sem barra de
> tarefas, sem relógio"*, e isso era descrição de projeto, não só de tela: a
> ausência de painel era a premissa. Hoje existe uma **barra própria** com
> relógio, desligar e os botões de transferir áudio/câmera — 2,6 MB de RSS, em C
> com Xlib cru, sem toolkit. O que **não** mudou é o que a premissa protegia: não
> há desktop environment, não há painel do XFCE, e a RAM continua baixa. Veja "A
> barra de tarefas".

> **Nota histórica.** Este projeto começou com o **i3** e um ambiente só-terminal
> em *tiling*. O i3 foi trocado porque ele não desenha botão de janela nenhum —
> não é configuração, é ausência de recurso — e minimizar sequer existe nele.
> O `i3.config` continua no repositório e o `startwm.sh` documenta como voltar,
> caso você prefira tiling puro.

## Instalar

Do zero, numa máquina recém-formatada, na ordem:

**1. Windows — WSL2 e o Ubuntu**

```powershell
wsl --install -d Ubuntu-24.04
```

Reinicie se ele pedir. Na primeira abertura o Ubuntu cria seu usuário e pede
uma senha — **guarde essa senha**: é com ela que você entra na sessão RDP, não
com a do Windows. Se algum dia ela se perder:

```bash
sudo passwd $USER
```

**2. Leve esta pasta para dentro da WSL**

Ela precisa estar no sistema de arquivos do Linux (`~/linux-fullscreen`), não em
`/mnt/c`, senão o bit de execução dos scripts se perde.

**3. Linux — o instalador**

```bash
sudo bash ~/linux-fullscreen/install.sh
```

Instala `xrdp`, `xorgxrdp`, `xfwm4`, `xfce4-settings`, `exo-utils`,
`xfce4-appfinder`, `xfce4-terminal`, `dbus-x11`, `x11-xserver-utils`,
`fonts-dejavu-core` e o `i3` (plano B); põe o xrdp na porta 3390; e ajusta o
`.wslconfig` do Windows. É idempotente — pode rodar de novo à vontade, todo
arquivo substituído vira `.orig` antes.

**4. Windows — reiniciar a WSL**

```powershell
wsl --shutdown
```

**Não pule.** O `.wslconfig` só é lido quando a WSL sobe do zero, e é ele que
desliga o WSLg. Sem isso as janelas abrem soltas no desktop do Windows.

**5. Windows — o atalho**

Copie **os dois** arquivos de `windows/` — `Linux Fullscreen.vbs` e
`Linux Fullscreen.rdp` — para a Área de Trabalho, na mesma pasta, e clique no
`.vbs`. Login = seu usuário Linux + a senha do passo 1.

O `.rdp` ao lado é que carrega o fullscreen multimonitor e as opções afinadas
de desempenho (veja "Fluidez"). Se ele faltar, o `.vbs` ainda conecta, mas cai
nas opções padrão do mstsc.

`Ctrl+Alt+Break` sai do fullscreen e devolve o Windows.

## Como funciona

```
Windows                          WSL2 (Ubuntu-24.04)
--------                         -------------------
mstsc.exe /multimon /f  <--RDP-->  xrdp :3390
                                     |
                                   Xorg (:10)  <- monitores viram saídas RandR
                                     |
                                   xfwm4 + xfce4-terminal
```

### O WSLg está desligado — de propósito

Este setup **desativa o WSLg** em `C:\Users\<você>\.wslconfig`:

```ini
[wsl2]
guiApplications=false
```

Não é detalhe de gosto: o WSLg era a origem de praticamente todo problema
listado em "Problemas conhecidos". Ele deixa em toda sessão WSL um symlink

```
/run/user/1000/wayland-0  ->  /mnt/wslg/runtime-dir/wayland-0
```

que funciona como um ralo — qualquer app GTK/Qt/Electron que encontre esse
socket renderiza no desktop do Windows em vez de entrar na sessão RDP, mesmo
com `DISPLAY=:10` correto. Desligado o WSLg, o socket não existe e não há para
onde a janela fugir.

O preço: atalhos `.vbs` que abriam apps Linux direto no Windows (`nautilus.vbs`
e afins) param de funcionar. A troca só faz sentido se você for viver dentro da
sessão fullscreen. Para reverter, `guiApplications=true` e `wsl --shutdown`.

## Atalhos

`Super` = tecla **Windows**.

| Atalho | Ação |
|---|---|
| `Ctrl+Alt+T` | novo terminal — o `terminal.c` deste repositório |
| `Alt+F3` ou `Super+R` | xfce4-appfinder (é assim que se abre programa) |
| `Alt+F4` | fecha a janela |
| `Alt+Tab` | alterna janelas (inclui as minimizadas) |
| `Super+←` / `Super+→` | meia tela na lateral |
| `Super+↑` | maximiza (de novo, restaura) |
| `Super+↓` | minimiza |
| `Super+Shift+←` / `Super+Shift+→` | manda a janela para o monitor vizinho |
| `Super+Shift+↑` / `Super+Shift+↓` | idem, para monitor acima / abaixo |
| `Super+D` | mostra a área de trabalho |
| `Super+KP_8` / `KP_2` / `KP_7` / `KP_9` / `KP_1` / `KP_3` | tiling pelo teclado numérico (padrão do XFCE) |
| `Alt+F7` / `Alt+F8` | mover / redimensionar pelo teclado |
| arrastar até a borda | snap, como no Windows |
| **`Ctrl+Alt+Break`** | (do RDP) sai do fullscreen, volta pro Windows |

Os `Super+setas` **não vêm de fábrica** — o padrão do XFCE põe o tiling no
teclado numérico (`Super+KP_Left` etc.), e quem vem do GNOME aperta `Super+←`
e conclui que nada funciona. Quem cria esses mapeamentos é o
`xfwm-atalhos.sh`, que precisa ser rodado uma vez, de dentro da sessão.

Uma ausência real, verificada na lista de ações do xfwm4:

- **`Super+↓` minimiza direto**, sem restaurar antes como faz o Windows — o
  xfwm4 não tem essa ação composta. Para trazer de volta, `Alt+Tab`.

### A regra que faltava: uma tecla por ação

**Medido em 29/07/2026.** É a causa real do `Super+→` morto que este projeto
perseguiu por dias como "corrida na largada" — e ela explica de uma vez três
coisas que pareciam desconexas.

**O xfwm4 guarda um único atalho por ação interna.** Se duas teclas apontam para
a mesma ação, só uma consegue o *passive grab* no servidor X; a outra fica muda.
No X um grab de tecla tem **um** dono, e a segunda tentativa leva `BadAccess`.

Quem ganha é a **última a ser gravada** — então o vencedor depende da ordem em
que as propriedades são lidas, e muda de sessão para sessão. Era exatamente isso
que imitava uma falha de arranque: configuração correta, tecla chegando ao X,
xfwm4 sem reagir, e um `Super+←` que funcionava ao lado de um `Super+→` que não.
Não havia corrida com o `setxkbmap`; havia colisão.

A medição que fecha o caso — o padrão é perfeito:

| Par de teclas | Ações | Resultado |
|---|---|---|
| `Super+←` / `Super+KP_4` | **as duas** `tile_left_key` | uma viva, outra muda |
| `Super+→` / `Super+KP_6` | **as duas** `tile_right_key` | uma viva, outra muda |
| `Super+↑` / `Super+KP_8` | `maximize_window` vs `tile_up` | **as duas vivas** |
| `Super+↓` / `Super+KP_2` | `hide_window` vs `tile_down` | **as duas vivas** |

Só colidem os pares que compartilham a ação. E o teste que **falsifica** a
hipótese, se alguém duvidar: regravar o `<Super>KP_Right` mata o `Super+→` na
hora, e regravar o `<Super>Right` o traz de volta matando o outro. Previsto e
confirmado.

**O `xfwm-atalhos.sh` agora remove as duplicatas** (`<Super>KP_Left` e
`<Super>KP_Right`) antes de gravar as setas, o que torna o resultado
determinístico em vez de depender de ordem. **É o preço da regra:** essas duas
teclas do numérico deixaram de encaixar às laterais. As outras seis
(`KP_8`, `KP_2`, `KP_7`, `KP_9`, `KP_1`, `KP_3`) apontam para ações exclusivas e
continuam valendo.

Três vítimas antigas da mesma regra, achadas junto — as três já estavam mudas,
sem que ninguém tivesse notado:

| Tecla muda | Disputava | Contra |
|---|---|---|
| `Alt+F10` | `maximize_window_key` | `Super+↑` |
| `Alt+F9` | `hide_window_key` | `Super+↓` |
| `Ctrl+Alt+D` | `show_desktop_key` | `Super+D` |

O `Alt+F10` estava anunciado na tabela de atalhos acima como "maximiza" desde que
ela existe, sem nunca ter funcionado. Saiu.

**A escolha deste projeto é o `Super`** (decidida em 29/07/2026), e as três
perdedoras foram **removidas** da configuração pelo `xfwm-atalhos.sh` — não para
mudar comportamento, que já era esse, mas para a configuração parar de anunciar
tecla que não funciona. Foi assim que o `Alt+F10` enganou por tanto tempo.

Não há como ter os dois lados de nenhum desses pares. Para inverter qualquer um,
troque o `desbind` pelo `bind` no `xfwm-atalhos.sh` e tire a linha do `Super`.

Para auditar a qualquer momento — não deve sair nada:

```bash
xfconf-query -c xfce4-keyboard-shortcuts -p /xfwm4/custom -l -v \
  | awk 'NF==2{print $2}' | sort | uniq -d
```

E para saber se uma tecla específica tem dono, sem depender de apertá-la, o
método é tentar registrar o grab em cima: `BadAccess` significa que alguém é
dono (o atalho está vivo), e sucesso significa que **ninguém** registrou — a
tecla está morta. Foi assim que este diagnóstico saiu do palpite.

> **Correção (28/07/2026).** Este README afirmava também que *"não existe
> atalho para mandar janela ao outro monitor; o xfwm4 não tem nenhuma ação de
> monitor"*, e mandava arrastar com o mouse. **É falso.** O xfwm4 4.18 tem
> quatro ações de monitor, expostas nas preferências como *"Move to Another
> Monitor"*:
>
> ```bash
> strings /usr/bin/xfwm4 | grep move_window_to_monitor
> ```
>
> Elas estão mapeadas em `Super+Shift+setas` pelo `xfwm-atalhos.sh`. Não é
> preciso script, `wmctrl`, `xdotool` nem `sudo` — só mapear.

**Por que no `Shift` e não no `Super+seta` puro.** O Windows faz uma coisa
composta: `Win+→` encaixa à direita e, se a janela já estiver encaixada na
borda, joga para o monitor seguinte. O xfwm4 não tem ação composta — `tile` e
`move to monitor` são ações separadas, e uma ação de tiling numa janela já
encaixada daquele lado simplesmente não faz nada.

Imitar o Windows exigiria trocar o `Super+seta` por um **comando** (um script
lendo `xrandr` e movendo a janela), o que significa reimplementar o tiling e —
pior — devolver a tecla ao `xfsettingsd`, exatamente o arranjo que causou o bug
do grab órfão descrito em "Problemas conhecidos". Não compensa: com o `Shift`,
uma tecla só resolve a travessia e o tiling nativo continua intacto.

### Minimizar sem barra de tarefas

Esta sessão não tem painel, então uma janela minimizada não tem onde ser
clicada de volta. Isso só é seguro por causa de um ajuste em `xfwm4.xml`:

```xml
<property name="cycle_hidden" type="bool" value="true"/>
```

...que faz o `Alt+Tab` listar também as janelas ocultas. **Se você desmarcar
"incluir janelas ocultas" nas configurações do xfwm4, minimizar vira um buraco
negro.** Nesse caso, ou devolva o painel (`xfce4-panel &` no `startwm.sh`), ou
remova o atalho de minimizar.

> **A barra de tarefas não muda isto.** Ela existe desde 30/07/2026, mas **não
> tem lista de janelas** — de propósito. Então o `cycle_hidden` continua sendo a
> única rede de segurança do minimizar, com o mesmo peso de antes.

## Desligar (e a interface)

Fechar a janela do mstsc **não encerra nada** — nem a sessão, nem a VM. Sem um
botão, desligar era fechar o RDP e depois ir ao PowerShell rodar
`wsl --shutdown`. O `linux-desktop-down` resolve isso de dentro:

| Como chamar | O que faz |
|---|---|
| botão **Desligar** na barra | o mesmo que o `Alt+F3` → "Desligar", a um clique |
| `Alt+F3` → "Desligar" | encerra a sessão **e** desliga a VM (devolve a RAM ao Windows) |
| `Alt+F3` → "Sair da sessão" | encerra só a sessão gráfica; a VM continua, e o atalho do Windows reabre em segundos |
| `linux-desktop-down -y` | o mesmo, sem perguntar (para script) |

Os dois modos **devolvem o headset e a webcam ao Windows** antes de encerrar —
veja "Sair da sessão devolve tudo", que explica por que isso é necessário até no
modo VM, onde o `wsl --shutdown` parece resolver sozinho e resolve metade.

É o par do `linux-desktop-up`, que o atalho do Windows já chamava para **subir**
tudo. Os dois modos pedem confirmação, porque ambos matam aplicativos sem
oferecer salvar — e um item de menu não pode desligar a máquina com um `Enter`
distraído.

O script encapsula o que se aprendeu na marra e não é óbvio:

- **`pkill -x xfsettingsd` antes do `xfwm4`.** Se o xfsettingsd tiver sido
  reiniciado à mão durante a sessão (o conserto do grab órfão faz isso), ele foi
  desacoplado com `setsid` e pode sobreviver ao fim da sessão — e aí a sessão
  seguinte sobe um segundo, os dois disputam os mesmos grabs e o `Super+→` morre
  de novo.
- **`pkill -x xfwm4`, não `xfwm4 --quit`** — essa opção não existe no 4.18.
- **Caminho completo do `wsl.exe`.** O PATH do Windows não está no PATH desta
  sessão (`cmd.exe: command not found`) mesmo com o interop ligado.
- **`setsid` no `wsl.exe --shutdown`.** O comando mata a VM que hospeda o próprio
  script; sem sair do grupo de processos, ele morreria no meio da chamada.
- **`sudo -n` no plano B.** Lançado pelo appfinder não há terminal: um `sudo`
  comum esperaria uma senha que não tem onde ser digitada e o botão pareceria
  não fazer nada.

### Log de depuração da sessão

O `startwm.sh` grava a saída inteira da sessão (`dbus-launch`, `xfsettingsd`,
`xfwm4`) em `~/startwm-debug.log`, via `exec > "$HOME/startwm-debug.log" 2>&1`
antes do `exec dbus-launch`. Ele é truncado a cada novo login, então nunca
cresce sem limite — só reflete a sessão mais recente.

Existe porque o `xrdp-sesman.log` só registra o **código de saída** do
gerenciador de janelas (`exited with non-zero exit code 1`), nunca a mensagem
de erro em si. Foi assim que se descobriu, em 28/07/2026, que a sessão abria e
fechava sozinha em segundos: o `xfwm4`/`dbus-launch` morria sem deixar rastro
em nenhum log padrão do xrdp, e só o redirecionamento revelou o motivo. Nesse
caso específico a causa acabou sendo um `/etc/xrdp/startwm.sh` corrompido por
uma instalação anterior — reinstalar (`sudo bash install.sh`) resolveu.

O `linux-desktop-down` apaga esse arquivo ao sair da sessão ou desligar a VM
(por arrumação — como o arquivo é truncado no próximo login de qualquer jeito,
deixá-lo parado no `$HOME` entre sessões não preserva nada útil).

### Onde crescer a interface

Quem lista aplicativos é o `xfce4-appfinder` (`Alt+F3` ou `Super+R`). Então
**um item novo de menu é um `.desktop` novo** em `desktop/`, que o `install.sh`
copia para `/usr/share/applications/`. Não é preciso subir um desktop environment
para ter menu — é o que permite manter a premissa de RAM baixa deste projeto.

Desde 30/07/2026 há um segundo lugar onde crescer: a **barra de tarefas**, para o
que precisa estar sempre visível ou a um clique. Acrescentar um botão ali é uma
linha na tabela `itens[]` do `barra-tarefas.c`. A regra prática que separa os
dois: se é "abrir um programa", vai no `.desktop`; se é "ver ou alternar um
estado", vai na barra.

Para os diálogos, a sessão tem `zenity` (GTK, bonitinho) e `xmessage` (feio,
mas quase sem dependência). O `linux-desktop-down` usa o primeiro que achar e
cai para o terminal se não houver tela — vale como modelo para os próximos.

## A barra de tarefas (30/07/2026)

Há uma barra flutuante na base do monitor primário, no visual do diálogo de
login do xrdp:

```
  ┌────────────────────────────────────────────────────────┐
  │  Audio: Linux  │  Camera: Win  │ 01:53 │   Desligar    │
  └────────────────────────────────────────────────────────┘
```

126–318 px de largura, 28 px de altura — ela se dimensiona pelo conteúdo, não
ocupa a tela toda. **Não tem lista de janelas**, de propósito: o caminho de
volta para uma janela minimizada continua sendo o `Alt+Tab`, por causa do
`cycle_hidden` (veja "Minimizar sem barra de tarefas").

O código é [`barra-tarefas.c`](barra-tarefas.c), o **único componente compilado**
deste repositório. O `install.sh` o compila e instala em `/usr/local/bin`; o
`startwm.sh` o sobe a cada login. Se a compilação falhar, o instalador avisa e a
sessão sobe sem ela.

### Por que Xlib cru, e não um toolkit

A primeira versão era Python + Tk e pesava **21.440 kB de RSS**. A atual pesa
**2.740 kB** — 7,8× menos, num binário de 21 KB.

Mas o número não foi o argumento decisivo. O visual exige desenhar cada pixel à
mão (o `relief raised` do Tk calcula os tons sozinho e não bate com o que foi
amostrado), então **aquela versão já usava um Canvas e ignorava os widgets**.
Estávamos pagando um toolkit inteiro para não usar widget nenhum dele. Em Xlib
as mesmas três primitivas — linha, retângulo, texto — vêm direto do servidor X.

### A paleta e o bisel, amostrados

Tudo veio do `Untitled.png` pixel a pixel, não de gosto. São **cinco cores e
nenhum meio-tom**, e a barra de título do diálogo é o *mesmo teal do fundo*, não
um azul separado:

| Cor | Onde |
|---|---|
| `#009CB5` | fundo da sessão, barra de título |
| `#DEDEDE` | face dos botões |
| `#FFFFFF` | bisel claro, fundo de campo |
| `#808080` | sombra interna |
| `#000000` | texto, borda externa |

O bisel Motif é **assimétrico**, e é esse detalhe que separa "arcaico" de só
"cinza":

| Borda | Pixels, de fora para dentro |
|---|---|
| topo, esquerda | `#FFFFFF` ×1 |
| baixo, direita | `#000000` ×1, depois `#808080` ×1 |

Repare que o cinza é `#DEDEDE`, mais claro que o `#C0C0C0` do Windows 95 — não é
aquele visual, é Motif/CDE.

**A fonte é core do X** (`-adobe-helvetica-medium-r-normal--11`), bitmap, não
TrueType. O próprio diálogo do xrdp é desenhado com fonte core, então o texto
fica *idêntico* e não apenas parecido — sem antialiasing nenhum. É por isso que
o `install.sh` instala `xfonts-base`: sem as fontes core a barra não sobe.

Consequência a lembrar: os rótulos têm que ser **ASCII**. A fonte está em
`iso8859-1` e o `.c` em UTF-8, então um rótulo acentuado sai corrompido no
`XDrawString`.

### Volume e escolha de dispositivo (31/07/2026)

Os dois primeiros controles da barra são `Vol` e `Mic`. Cada um tem **três
zonas**:

```
   ┌────┬──────────────┬──┐
   │Vol │  ▓▓▓▓█       │ ▾│
   └────┴──────────────┴──┘
     │          │        └── abre a lista de dispositivos
     │          └─────────── arrasta ou clica para ajustar
     └────────────────────── alterna o mudo (afundado = mudo)
```

**Sem cache, ao contrário do estado USB.** O `pactl` responde em 7–9 ms porque
fala por socket unix local — o que é caro é o interop do Windows, não um
processo local. Então a barra pergunta a cada tick e reflete mudanças feitas por
fora. Durante um arrasto ela **não** relê: o `pactl` ainda pode estar
respondendo o valor antigo e o cursor pularia para trás.

Sempre `@DEFAULT_SINK@` / `@DEFAULT_SOURCE@`, nunca um nome fixo. Assim o mesmo
controle serve para o headset USB nativo e para o `xrdp-sink`, e continua certo
depois de um clique em "Audio: Win".

#### A lista de dispositivos é unificada — e precisa ser

O Linux só tem **dois** dispositivos de cada lado: o `xrdp-sink` e, quando o
headset está anexado, a placa dele. Um menu feito só com o `pactl` mostraria
`xrdp-sink`, que não significa nada — e esconderia que atrás dele estão a caixa
do notebook e o monitor, que são do Windows.

Por isso o [`audio-dispositivos`](audio-dispositivos) junta os dois mundos:

```
> G435 Wireless Gaming Headset (aqui, USB)          ← placa nativa, sem estalo
  Alto-falantes (Realtek(R) Audio) (pelo Windows)   ← canal RDP
  LG HDR WFHD (NVIDIA) (pelo Windows)
```

Escolher um "pelo Windows" faz **duas** coisas: aponta o PulseAudio para o
`xrdp-sink` **e** troca o dispositivo padrão do Windows. É o único caminho até a
**caixa do notebook**, que não é USB (é Intel HDA/Realtek soldada na placa) e por
isso nunca pode ser anexada aqui. O `usbipd` só passa USB.

O `>` marca o que está em uso — marcador de texto, não cor: a paleta tem cinco
cores e nenhuma sobra para "estado".

Para saber qual é o padrão **atual do Windows** foi preciso o
`IMMDeviceEnumerator`; o `IPolicyConfig` só escreve, não lê. Mesma regra de
vtable dos outros: no `IMMDevice` o `GetId` é o **terceiro** método, e os dois
antes existem só para ocupar o slot.

> **Armadilha: as primitivas de desenho não podem ter a janela chumbada.**
> `levantado`, `gravado`, `linha` e `texto` pintavam sempre na janela da barra.
> Com o menu aberto, a moldura e o realce iam para a **barra**, nas coordenadas
> do menu — como o menu abre acima dela, aparecia um retângulo fantasma algumas
> linhas abaixo do ponteiro, e o realce de verdade nunca saía. Hoje existe um
> `static Drawable alvo` que quem desenha ajusta antes.
>
> O sintoma enganava: o clique **funcionava** (ele sempre usou coordenadas),
> só o retorno visual estava errado. Ao medir — ponteiro no centro exato de uma
> linha — **nenhuma** linha acendia, o que descartou "está um pixel fora" e
> apontou para o desenho estar indo para outro lugar.

> **Duas armadilhas de menu com ponteiro capturado.** Com
> `XGrabPointer(..., owner_events = False, ...)`:
>
> - **um clique fora também chega com `window == pop`**, só que com `x/y` fora do
>   retângulo. Testar a janela não distingue dentro de fora; tem que ser por
>   coordenada.
> - **a linha tem que vir das coordenadas do clique**, não da linha "sob o
>   mouse". Esta última só é preenchida por `MotionNotify`, e um clique direto
>   não gera movimento — o menu fechava sem escolher nada.

### Como acrescentar um item

Escreva a função de ação e ponha **uma linha** na tabela `itens[]`:

```c
static Item itens[] = {
    { BOTAO_USB,   "Audio",    92, NULL,          "audio",  0, 0 },
    { BOTAO_USB,   "Camera",   92, NULL,          "camera", 0, 0 },
    { BOTAO_COISAS,"Coisas",   56, NULL,          NULL,     0, 0 },
    { RELOGIO,     NULL,       52, NULL,          NULL,     0, 0 },
    { BOTAO,       "Desligar", 66, acao_desligar, NULL,     0, 0 },
};
```

A largura da barra é calculada a partir dela; nada mais precisa mudar.

### O botão "Coisas" (31/07/2026, renomeado em 02/08/2026)

Abre a lista e lança o escolhido — o `abrir-windows` faz o resto sozinho
(pergunta o monitor, encolhe a sessão, abre e devolve o multimonitor quando
você fecha). A lista vem da **pasta** descrita em "Abrir coisas do Windows
cedendo um monitor": pôs o item lá, ele está na barra.

> **Chamava-se "Jogos" até 02/08/2026**, quando a pasta passou a aceitar
> qualquer arquivo e não só lançador. A largura do botão saiu de 52 para 56 px:
> medido no mesmo dia com a própria fonte da barra, `"Coisas"` ocupa 34 px na
> `-*-helvetica-medium-r-normal--11-*` contra 30 px de `"Jogos"`, e 56 mantém a
> mesma folga de 22 px que o botão já tinha. Não é chute — a fonte é
> proporcional, e somar "mais um caractere" erraria.

**Um botão que abre menu, e não um botão por item.** A barra tem largura fixa
calculada da tabela `itens[]`; um botão por item faria ela crescer sem limite a
cada arquivo novo, e nomes longos precisariam ser encurtados. O menu já existia
para a escolha de dispositivo de áudio, e escala para qualquer quantidade.

**A lista é lida no clique, não guardada.** É o que faz um item novo aparecer
sem reiniciar a barra — que é o ponto todo de usar uma pasta — e não deixa nada
para vigiar enquanto o menu está fechado. Custa 48 ms com o cache do caminho
quente (31/07/2026), e 34 ms na remedição de 02/08/2026 depois da generalização
— aceitar qualquer arquivo não custou tempo, porque o gasto é a travessia de
diretório no drvfs e não o filtro.

**Os dois menus compartilham o parser** porque compartilham o formato:

```
<id>\t<em uso: 0 ou 1>\t<rótulo ASCII>
```

O `id` nunca é desenhado — é o que volta para o comando —, e por isso ele pode
ter acento enquanto o rótulo, que passa pelo `XDrawString`, não pode. Quem
transliera é o `abrir-windows --listar-coisas`, com
`iconv -t ASCII//TRANSLIT`; a barra continua sem saber o que é um jogo — nem o
que é um documento.

> Isso **deixou de ser um caso raro em 02/08/2026**. Enquanto a pasta só tinha
> jogo, nome acentuado era exceção; com documento na lista virou regra. Medido
> no mesmo dia: um arquivo de teste `Relatório Final.docx` sai como
> `Relatório Final` no campo `id` e `Relatorio Final` no rótulo, que é
> exatamente o que se quer — o acento sobrevive onde importa (o nome que volta
> para o comando) e desaparece onde quebraria (o `XDrawString`).

> O nome é um **nome de arquivo escolhido por você**, então "Assassin's
> Creed.lnk" é normal e partiria o comando ao meio. A `cita()` aplica a regra do
> apóstrofo (fecha a aspa, escapa, reabre) antes de montar a linha.

Pasta vazia mostra uma linha de aviso em vez de um menu de zero linha — sem ela
o clique parecia um botão morto.

### Atalhos de aplicativo, com ícone (31/07/2026)

Entre o botão "Coisas" e o relógio ficam os seus apps, e depois deles um `[+]`:

```
[Vol][Mic][Audio: Linux][Camera: Rede][Jogos][@][>_][+] 16:28 [Desligar]
```

Clicar no `[+]` abre a lista dos apps instalados (168 aqui), lida dos `.desktop`.
Clicar num deles põe ou tira da barra — **o mesmo botão faz as duas coisas**, e
os que já estão lá aparecem com `[*]`. Não há arquivo para editar nem comando
para decorar.

**Botão direito no ícone** abre um menu de uma linha, "Tirar &lt;app&gt; da barra".
É o caminho curto; o `[+]` continua servindo para os dois sentidos.

> **Por que um menu e não remover direto.** A barra mora na borda de baixo da
> tela, que é por onde o ponteiro passa o tempo todo. Um botão direito perdido
> apagaria um atalho sem aviso. Com o menu é preciso clicar de novo, e clicar
> fora cancela — a mesma mecânica dos outros dois menus, então não há gesto novo
> para aprender.

> **Isso destapou um bug que estava dormindo.** Até 31/07/2026 o `ButtonPress`
> **não olhava qual botão era**, e todos faziam a ação do esquerdo: botão direito
> no ícone do Brave abria o Brave, botão do meio na calha de volume mexia no
> volume. Nunca incomodou porque nada estava ligado ao direito — passou a
> incomodar no instante em que ele virou "tirar da barra". Agora o direito só faz
> sentido nos atalhos de aplicativo; nos outros itens ele não faz nada, em vez de
> fazer a ação do esquerdo.

Estes botões **lançam, não alternam**. Continua não havendo lista de janelas, e
isso é deliberado: minimizou, volta pelo `Alt+Tab` (veja "Minimizar sem barra de
tarefas"). Um `[+]` que fixasse janelas seria outra coisa, e não é esta.

**Pôr um programa deste projeto na barra (02/08/2026).** A lista do `[+]` é a
dos `.desktop` instalados, então um binário nosso só aparece lá se tiver um. Foi
o caso do `terminal`: ganhou o `desktop/terminal.desktop` do repositório, que o
`install.sh` já leva para `/usr/share/applications` junto com os outros. Depois
disso é o caminho normal — `[+]`, clicar em "Terminal", pronto.

O `bancada.desktop` foi junto, no mesmo dia e pelo mesmo motivo. Ele existia,
mas escrito à mão em `~/.local/share/applications/` — **fora do repositório**,
ou seja, fora do git e fora do `install.sh`: sobrevivia a tudo, menos a formatar
a máquina, que é justamente o caso em que ele faz falta. Estar no git guarda o
arquivo; é o `install.sh` que o *põe no lugar onde o `[+]` procura*. De passagem,
o `Exec=bancada` virou `Exec=/usr/local/bin/bancada`, para não depender do PATH
de quem lança.

> **Mover um `.desktop` já fixado tira o ícone da barra, calado.** O `apps.conf`
> guarda o **caminho** do `.desktop`, e o `fixados()` pula silenciosamente as
> linhas cujo arquivo não existe mais — de propósito, para um app desinstalado
> não deixar um botão quebrado. Mudar o arquivo de `~/.local/share/applications`
> para `/usr/share/applications` é exatamente esse caso: some o botão, sem erro
> nenhum. Ao mover, corrija a linha do `apps.conf` junto (foi o que se fez aqui,
> e a barra continuou com os mesmos 734 px, na mesma ordem).

O `Icon=utilities-terminal` é um **nome de tema**, não um caminho: o `barra-apps`
o resolve em `/usr/share/icons`, converte para os 1200 bytes crus e guarda. Sai
exato (conferido: `1200`), o que quer dizer que a barra desenha o ícone e não cai
no plano B do rótulo de texto.

Medido no mesmo dia, e serve de teste de que a barra **relê o `apps.conf`
sozinha**: a janela dela tinha 702 px sem o Terminal e 734 px com ele — 32 px a
mais, um botão de app, sem reiniciar nada. A barra compara o `mtime` do
`apps.conf` a cada tique e remonta quando ele muda.

**Por que `.desktop` e não uma lista de comandos.** O `.desktop` já traz o nome
bonito, o comando certo com caminho absoluto e o nome do ícone. Fixar um app é
guardar **um caminho de `.desktop`** em `apps.conf`; se o pacote for atualizado
e o binário mudar de lugar, o atalho continua certo sozinho. O `Exec=` passa por
uma limpeza dos *field codes* (`%U %f %F %i %c %k`) — são lugares onde um
lançador poria arquivos arrastados, e sem removê-los o app receberia um
argumento literal `%U`.

#### O ícone entrou sem trazer libpng junto

Este é o primeiro pixel de verdade que a barra desenha, e o caminho óbvio seria
linkar uma biblioteca de imagem — que seria também o fim da premissa do arquivo
(Xlib cru, RAM baixa). O desvio:

**O `barra-apps` converte o ícone UMA vez, na hora em que você o fixa**, e deixa
no cache um arquivo de pixels crus:

```bash
convert "$origem" -background '#DEDEDE' -alpha remove -alpha off \
        -resize 20x20! -depth 8 "rgb:$destino"      # 20*20*3 = 1200 bytes exatos
```

Sem cabeçalho, sem compressão, sem alfa. A barra lê isso com um `fread` e joga
num `XImage`. **Nenhuma dependência nova entrou no `barra-tarefas.c`** — o
`imagemagick` é dependência do `barra-apps`, que roda uma vez por app fixado.

A transparência morre na conversão: o `-alpha remove` achata o alfa sobre o
`#DEDEDE`, que é a cor da face da barra. Como esse fundo é chapado, o resultado é
idêntico ao de compor de verdade — e a barra não precisa saber o que é alfa.

> **Não chumbe `0xRRGGBB`.** A cor vai para o pixel pelas máscaras do visual
> (`componente()`), não por deslocamento fixo. Num servidor com outro arranjo de
> bytes o ícone sairia com as cores trocadas — falha feia e silenciosa. Aqui o
> visual é TrueColor de 24 bits, mas isso não é garantia de nada.

O `Pixmap` é montado uma vez, no carregamento; cada redesenho depois é um
`XCopyArea`, que roda inteiro dentro do servidor X — nenhum pixel passa pelo
socket a cada tick.

**Custo medido**, com três ícones carregados: RSS de **2,6 MB → 3,0 MB**.

#### A barra muda de tamanho sozinha

`apps.conf` mudou → o `tick` vê o `mtime` (um `stat` num arquivo local, a cada
2 s) → remonta, e aí **não basta redesenhar**: a barra mudou de largura. A ordem
importa e as três coisas são obrigatórias:

1. `dicas_de_tamanho()` **antes** do resize — as dicas dizem `min=max`, e sem
   atualizá-las o xfwm4 recusa a largura nova e a barra fica do tamanho velho
   com o conteúdo transbordando;
2. `XResizeWindow`;
3. `reposicionar()`, que recentra **e refaz o strut** — o `bottom_end_x` depende
   da largura.

Medido: fixar um app levou a barra de 702 para 734px, recentrando de x=929 para
x=913, e uma janela maximizada continuou parando em y=1052.

> **O `_NET_WORKAREA` fica para trás.** Depois de um resize da barra ele continua
> mostrando `4480x1080` mesmo com o strut publicado e funcionando. Não é bug
> nosso: o xfwm4 mantém as margens internas certas (a janela maximizada **para**
> na barra, medido) e só republica essa dica global em certos eventos. Diagnostique
> pelo comportamento, não por esse `xprop` — foi o que quase me fez "consertar"
> algo que não estava quebrado.

#### Armadilha de teste: o `import` quebra o grab do ponteiro

Anotado porque custou meia hora e vai custar de novo. Testando o menu do botão
direito por `XTest`, a sequência

```
clique direito no ícone  ->  menu abre         (ok)
import -window root ...  ->  screenshot        (para conferir)
clique na linha do menu  ->  menu fecha, NADA acontece
```

fazia parecer que o `escolher_popup` estava quebrado. Não estava — a versão
instrumentada mostrou o caminho inteiro correto. **É o `import` que interfere no
grab do ponteiro** que o menu mantém (`XGrabPointer` com `owner_events` False).
Sem o grab, o clique seguinte chega à janela da **barra** em vez da do menu, e
as coordenadas passam a ser relativas a ela: o `y` fica negativo (o menu está
*acima* da barra), a linha não casa com nenhuma, e o código faz exatamente o que
deve — fecha sem escolher.

O sintoma engana porque o menu **fecha**, ou seja, o clique claramente chegou a
algum lugar. Regra: **não tire screenshot no meio de um gesto com menu aberto.**
Capture antes ou depois; para conferir o gesto, instrumente.

#### A ordem no `main()` é circular, e por isso a janela nasce 1x1

`montar_itens()` precisa do `gc` para converter ícones em `Pixmap`; o `gc`
precisa de uma janela; e a largura da janela só é conhecida **depois** de saber
quantos atalhos existem. A saída é criar a janela com 1px de largura e
redimensioná-la antes de mapear — não custa nada e não pisca, porque o
gerenciador de janelas ainda nem viu a janela.

### Ela se reposiciona sozinha — e por que isso é obrigatório

A barra escuta `RRScreenChangeNotify` e se move quando o layout de monitores
muda. Sem isso, o `abrir-windows` (que encolhe a sessão para um monitor) a
deixaria fora da tela até o próximo login.

No tratamento do evento, o `XRRUpdateConfiguration` **não é enfeite**: sem ele o
Xlib continua com a tela antiga em cache e o reposicionamento usa geometria
velha.

> **Armadilha: o xrdp não marca monitor primário.** Medido em 30/07/2026 — numa
> sessão o `xrandr --listmonitors` mostrava `+*rdp1`, e na seguinte, após "Sair
> da sessão" e reconectar, **nenhum dos dois tinha a marca**:
>
> ```
> 0: +rdp0 1920/344x1080/194+2560+0      <- sem o '*'
> 1: +rdp1 2560/798x1080/334+0+0
> ```
>
> Como a ordem em que o xrdp cria as saídas também não é estável, cair no
> "índice 0" punha a barra no monitor errado de forma imprevisível. O desempate
> é por **posição**: o monitor que contém a origem `(0,0)` — o mesmo critério
> que o `abrir-windows` usa para numerar monitores.

### As janelas param em cima dela: `_NET_WM_STRUT_PARTIAL` (31/07/2026)

Até aqui a barra **tapava** o que estivesse embaixo: uma janela maximizada ia
até o fim da tela e perdia os 28px de baixo atrás da barra. O conserto não é
mexer nas janelas — é fazer o xfwm4 saber que aquela faixa não existe.

**A troca que destravou tudo: deixar de ser `override_redirect`.** Essa flag diz
ao servidor X "nenhum gerenciador de janelas toca nisto", e era o que dava
posição exata e ausência de moldura. O preço escondido é que o xfwm4 **não
enxergava a barra de forma alguma**, e quem calcula o tamanho de uma janela
maximizada é ele. Janela não gerenciada não tem *strut*: a propriedade fica lá,
o WM nunca lê, e o efeito some sem nenhum erro.

Então a barra virou uma janela comum, com dois anúncios EWMH postos **antes do
`XMapWindow`** (o WM lê tudo no instante em que adota a janela):

| Propriedade | O que ela compra |
|---|---|
| `_NET_WM_WINDOW_TYPE_DOCK` | sem moldura, sem foco de teclado, sempre acima, fora do `Alt+Tab` — devolve de graça tudo o que o `override_redirect` dava |
| `_NET_WM_STRUT_PARTIAL` | "reserve 28px na borda de baixo". O xfwm4 subtrai do `_NET_WORKAREA` e a maximização passa a respeitar sozinha |

**O strut é medido da borda da TELA, não do monitor.** Com dois monitores lado a
lado a tela X é a caixa que envolve os dois, então o valor certo é
`altura_da_tela - (monitor_y + monitor_altura) + ALTURA`. Dá exatamente `ALTURA`
quando o monitor da barra encosta no fundo, e compensa a diferença quando ele é
mais baixo que o vizinho. Como o strut é recalculado dentro de `reposicionar()`,
ele acompanha o `abrir-windows` encolhendo a sessão.

Medido em 31/07/2026, tela `4480x1080`, barra no monitor da esquerda
(`rdp1 2560x1080+0+0`):

```
antes:  _NET_WORKAREA = 0, 0, 4480, 1080
depois: _NET_WORKAREA = 0, 0, 4480, 1052        <- 28px reservados

maximizar no monitor DA barra:    y=24  h=1028  -> termina em 1052, encosta nela
maximizar no monitor DE FORA:     y=0   h=1080  -> intacto
```

O segundo monitor ficar intacto é o efeito dos campos `bottom_start_x`/
`bottom_end_x`, que limitam o strut ao trecho horizontal onde a barra está. Sem
eles o monitor da direita perderia 28px por nada.

> Conferido no mesmo dia que a barra **não rouba o foco**: `XGetInputFocus`
> continua apontando para o navegador depois que ela sobe. O `input = False` do
> `XSetWMHints` mais o tipo `DOCK` bastam. O `_NET_WM_STATE_FOCUSED` que aparece
> no `xprop` da barra é resíduo do momento do mapa — o `_NET_ACTIVE_WINDOW` da
> raiz nunca aponta para ela.

> **Não medido:** o `startwm.sh` sobe a barra **antes** do `xfwm4`, então quem a
> adota é a varredura de janelas já mapeadas que o xfwm4 faz no arranque, e não
> o caminho normal de `MapRequest`. O teste acima foi feito com o xfwm4 já
> rodando. A varredura lê as mesmas propriedades, mas isso só se confirma num
> login completo.

### O limite do strut: monitores empilhados (31/07/2026)

O strut adapta-se sozinho a **qualquer** arranjo lado a lado — número de
monitores, resoluções diferentes, alturas diferentes. Ele é recalculado dentro
de `reposicionar()`, que já roda a cada `RRScreenChangeNotify`. Mas há **um**
arranjo que ele não sabe descrever, e a descoberta veio de medir, não de supor.

`_NET_WM_STRUT_PARTIAL` só sabe dizer *"reserve N pixels contados da borda da
tela"*. Não existe forma de dizer *"reserve uma faixa no meio"*. Os campos
`start_x`/`end_x` recortam o strut **na horizontal**, e por isso protegem o
monitor ao lado — mas não protegem nada que esteja **embaixo**, no mesmo trecho
horizontal.

Medido com dois monitores 2560x540 empilhados e a barra no de **cima**:

```
_NET_WM_STRUT_PARTIAL = 0, 0, 0, 568 ...          <- 1080 - 540 + 28

maximizar no monitor DE BAIXO  ->  x=0 y=24 2560x488
```

A janela foi parar no monitor **de cima**, e o de baixo virou terra morta: os
568px engoliram-no inteiro.

**O conserto é não publicar strut nenhum nesse caso.** A função
`engoliria_vizinho()` procura um monitor que fique abaixo do monitor da barra e
no mesmo trecho horizontal dela; se achar, o strut sai zerado. A barra volta a
só flutuar por cima naquele monitor — o comportamento antigo, ruim mas não
quebrado. É o que os painéis de verdade fazem quando não estão numa borda da
tela. Depois do conserto, os três arranjos medidos:

| Arranjo | `_NET_WORKAREA` | Veredito |
|---|---|---|
| lado a lado, 2560 + 1920 (o real) | `4480x1052` | reserva os 28px, vizinho intacto |
| empilhado, barra em **cima** | `4480x1080` | não reserva — monitor de baixo salvo |
| empilhado, barra em **baixo** | `4480x1052` | reserva; o monitor de cima não perde nada |

> **Como testar sem trocar de hardware.** `xrandr --setmonitor <nome> <geom>
> <saída>` inventa um monitor só nos metadados do RandR, sem tocar nas saídas de
> verdade, e `--delmonitor` desfaz. Foi assim que os três arranjos acima foram
> medidos dentro da sessão em uso. **Armadilha medida no mesmo dia:** essa troca
> **não** gera `RRScreenChangeNotify` — a barra não percebe e é preciso
> reiniciá-la à mão para ver o efeito. As trocas de verdade (reconectar o mstsc
> com outro `.rdp`, que é o que o `abrir-windows` faz) mudam o tamanho da tela e
> aí o evento sai.

## Fluidez

O caminho até a sua tela tem **dois** gargalos independentes, e eles pedem
correções opostas. Confundir os dois foi o erro que atrasou este trabalho.

| Gargalo | Quem limita | Como se mede |
|---|---|---|
| **Desenhar** o quadro | o renderizador OpenGL | `glxgears` |
| **Entregar** o quadro | codec + intervalo de captura do xrdp | seus olhos |

> **Sobre os números desta seção.** Todos foram medidos com `llvmpipe`, que era
> o renderizador padrão na época. Hoje a sessão usa a GPU por escolha (veja
> "A GPU"), e o mesmo `glxgears` marca ~77 FPS em vez de ~331 — sem que a
> fluidez piore, porque os dois valores estão acima do teto de entrega de
> 62 fps. Para comparar com o que está registrado aqui, force a CPU:
> `GALLIUM_DRIVER=llvmpipe glxgears ...`

### O trabalho de 28/07/2026: atacando o desenho

Medido com `glxgears` numa janela 1600x900 dentro da sessão:

| Configuração | FPS |
|---|---|
| Original (compositing ligado, 32 bpp, `.vbs` sem perfil) | 197 |
| Só o compositing do xfwm4 desligado | 225 |
| Sem compositing + 24 bpp + `.rdp` afinado | **497** |

2,5×. Ressalva de método: o 24 bpp e o `.rdp` entraram juntos com uma sessão
nova, então não dá para separar quanto foi de cada um.

### O trabalho de 29/07/2026: atacando a entrega — e o paradoxo

Trocado o xrdp 0.9.24 pelo **0.10.6.1 compilado da fonte**, com GFX/H.264. A
mesma medição depois:

| | glxgears | Teto de entrega | Fluidez percebida |
|---|---|---|---|
| xrdp 0.9.24 (NSCodec) | **497 FPS** | 25 fps | pior |
| xrdp 0.10.6.1 (GFX/H.264) | **331 FPS** | 62 fps | **muito melhor** |

**O benchmark caiu 40% e a experiência melhorou.** Não é contradição: o x264
consome CPU (medido: `xrdp` sai de 30% em repouso para 73% com a tela em
movimento), e essa CPU sai do mesmo bolo que o `llvmpipe` usa para desenhar. O
app desenha menos — mas o que ele desenhava a mais nunca chegava na sua tela.

**A lição, para quem for afinar isto no futuro:** o `glxgears` só media o
gargalo do *desenho*. Depois que a entrega passou a ser o limite, ele virou
enganoso. Não otimize por ele sem olhar a tela.

> **Como medir sem se enganar.** O `glxgears` imprime uma amostra a cada 5
> segundos. Descarte **a primeira** (aquecimento) e **qualquer janela que não
> tenha exatamente `5.0 seconds`** — quando o `timeout` mata o processo no
> meio, a última amostra sai truncada e com FPS artificialmente baixo. Incluí-la
> na média já produziu aqui uma conclusão errada (parecia que um ajuste não
> tinha surtido efeito, quando tinha dado 11%).
>
> ```bash
> DISPLAY=:10 timeout 32 glxgears -geometry 1600x900+100+100 > g.txt 2>&1
> grep "frames in" g.txt | awk '$4=="5.0"' | tail -n +2 \
>   | awk '{s+=$(NF-1);n++} END{printf "%.1f FPS (%d amostras)\n", s/n, n}'
> ```
>
> E olhe a **dispersão**, não só a média: foi ela que revelou o efeito real do
> `threads` do x264.

### Por que este caminho é lento

```
app renderiza (CPU)
   ↓
xorgxrdp captura  →  xrdp comprime (CPU)  →  TCP
   ↓
mstsc descomprime  →  Windows compõe  →  a GPU do Windows varre o monitor
```

Não há GPU em nenhum ponto do lado Linux. O Xorg desta sessão roda com o
driver **`xrdpdev`**, que é virtual: desenha em memória e entrega pixels. São
4480x1080 por quadro, comprimidos por CPU.

### O codec: como saber o que está valendo

```bash
sudo grep -iE "gfx|h264|codec" /var/log/xrdp.log | tail -10
```

**Saudável** (0.10 com GFX):

```
xrdp_mm_egfx_caps_advertise: ... monitorCount 2
xrdp_mm_egfx_create_surfaces: map surface_id 0 ... width 1920 height 1080
xrdp_encoder_create: starting h264 codec session gfx
xrdp_encoder_create: using x264 for software encoder
xrdp_mm_egfx_caps_advertise: egfx created.
```

**Degradado** (era assim no 0.9.24):

```
xrdp_caps_process_codecs: nscodec, codec id 1
xrdp_caps_process_codecs: unknown codec id 5
```

O `unknown codec id 5` era o mstsc oferecendo GFX e o 0.9.24 não sabendo o que
era. O 0.10 identifica: `Image RemoteFX(2744CCD4-9D8A-4E74-803C-0ECBEEA19C54)`.

### O intervalo de captura — o teto real

Só existe no 0.10 (`/etc/xrdp/xrdp.ini`):

```ini
h264_frame_interval=16      # ~62 quadros/s
rfx_frame_interval=32       # ~31
normal_frame_interval=40    # 25   <- o caminho do 0.9.24
```

É por isso que o 0.9.24 não passava de 25 fps na tela por mais que o
`glxgears` marcasse 497. O 0.9.24 **não tem esse ajuste**: nem no binário, nem
no `.ini`, nem no manual.

### Armadilha: `max_bpp=24` quebra o GFX

Estivemos com `max_bpp=24` para aliviar o NSCodec. Com o 0.10 isso faz o
servidor **recusar** o pipeline:

```
[WARN] client requested gfx protocol with insufficient color depth
```

...e a sessão cai de volta no NSCodec — justamente o que se queria abandonar.
**Precisa ser 32 nos dois lados**: `max_bpp` no `xrdp.ini` e `session bpp` no
`Linux Fullscreen.rdp`. O H.264 comprime muito mais do que os 25% que o 24 bits
economizava.

### O que foi aplicado

- `use_compositing=false` — o compositor recompõe regiões grandes a cada
  movimento, e aqui não há GPU para absorver. Custa sombra e transparência.
- `box_move=true` e `box_resize=true` — arrasta o contorno, não o conteúdo.
- `max_bpp=32` no `xrdp.ini` — **não baixe**, veja a armadilha acima.
- `windows/Linux Fullscreen.rdp` — conexão tipo LAN, sem detecção automática
  de banda e sem compressão do cliente. Num loopback essas heurísticas só
  custam CPU, que é justamente o recurso escasso aqui.
- `gfx.toml` — `threads = 2` por tela (o padrão é 1). São 4 threads numa
  máquina de 12 núcleos; o manual avisa que threads demais prejudicam a
  qualidade, e `tune = "zerolatency"` faz o x264 usar threads fatiadas dentro
  do quadro, que não acrescentam latência.

  **Medido, e não era o que se esperava.** O palpite era que não faria
  diferença, já que o encoder usava só 73% de *um* núcleo — sobrava CPU. Mas:

  | | `threads = 1` | `threads = 2` |
  |---|---|---|
  | glxgears, média | 297 FPS | **331 FPS** |
  | amplitude entre amostras | 255→328 = **73 FPS** | 327→334 = **7 FPS** |

  O ganho de 11% na média é secundário; o que importa é a **dispersão caindo
  dez vezes**. O problema nunca foi falta de capacidade total, era
  irregularidade: com uma thread, codificar um quadro de 2560x1080 toma um
  bloco longo de CPU, e cada bloco desses tira o `llvmpipe` do ar em rajadas.
  Com duas, cada codificação termina mais rápido e mais uniforme.

  Para a sensação de fluidez, regularidade vale mais que média.

Os três primeiros vivem no `xfwm-atalhos.sh`, que o `startwm.sh` reaplica 4
segundos após cada login — então voltam sozinhos a cada sessão.

Para ter sombra e transparência de volta:

```bash
xfconf-query -c xfwm4 -p /general/use_compositing -s true
```

O `gfx.toml` é relido **a cada conexão nova** — para testar um ajuste ali,
basta fechar o mstsc e reabrir pelo `.vbs`. Não precisa derrubar a sessão.

### Armadilha: `Xwrapper.config` volta sozinho

O Xorg só pode ser iniciado por quem o `/etc/X11/Xwrapper.config` permitir, e
o padrão do Debian/Ubuntu é `allowed_users=console` — que **não** inclui uma
sessão xrdp. Com ele, a sessão não sobe.

O problema é que isso não fica resolvido: o arquivo é **regenerado a cada
atualização do pacote `xserver-xorg-legacy`**, voltando para `console` (o
próprio cabeçalho do arquivo avisa). Aconteceu aqui em 29/07/2026, disparado
por um `apt install xserver-xorg-dev` durante a compilação do xorgxrdp.

```bash
grep allowed_users /etc/X11/Xwrapper.config    # tem que ser "anybody"
```

O `install.sh` força `anybody`, mas se a sessão parar de subir logo depois de
um `apt upgrade`, este é o primeiro arquivo a conferir.

## A GPU: o que dá e o que não dá

Separe duas coisas que não têm nada a ver uma com a outra.

**Computação (CUDA) funciona plenamente.** Medido nesta máquina com um saxpy de
16,7 milhões de elementos: **137 GB/s**, resultado correto, numa RTX 4060
Laptop. Não há penalidade relevante de virtualização, e nada disso passa pelo
xrdp — as simulações falam direto com `/dev/dxg`.

**Renderização é outra história**, e o motivo é estrutural:

```bash
ls /dev/dri/          # nao existe
ls /sys/class/drm/    # so "version", nenhum card0
```

Na WSL2 **o Linux não tem controlador de display nenhum**. A GPU é alcançada só
pelo `/dev/dxg`, que serve para computação e renderização fora-de-tela — nunca
para varrer um monitor. No Windows a placa é dona do painel; aqui o lado Linux
nunca chega nele. Não é configuração faltando.

### OpenGL na placa dedicada

**Esta sessão usa a 4060 por padrão.** O `startwm.sh` exporta:

```bash
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
```

Sem a segunda variável o d3d12 escolhe a **Intel integrada**, não a NVIDIA. Sem
a primeira, o Mesa cai no `llvmpipe`, que executa os shaders na CPU.

Para rodar **um** aplicativo na CPU, se algo renderizar errado:

```bash
GALLIUM_DRIVER=llvmpipe <o-app>
```

### O pedágio, medido

Sob o xrdp 0.10, `glxgears`, mesmas condições:

| Janela | `llvmpipe` (CPU) | `d3d12` (RTX 4060) | ms por quadro |
|---|---|---|---|
| 300x300 | **1084,6 FPS** | 102,8 FPS | 0,9 vs 9,7 |
| 1600x900 | **262,5 FPS** | 77,0 FPS | 3,8 vs 13,0 |

A CPU ganha do jeito que está — e não é pouco. O `d3d12` paga um **pedágio fixo
de ~9,5 ms por quadro**: o `xrdpdev` só lê pixels da RAM, então todo quadro
renderizado na placa precisa ser copiado de volta atravessando o `/dev/dxg`.

O detalhe que importa: esse pedágio é **latência de sincronização, não largura
de banda**. Repare que 16 vezes mais pixels custam só 25% a mais ao `d3d12`
(9,7 → 13,0 ms), enquanto o `llvmpipe` fica 4 vezes mais lento (0,9 → 3,8 ms).
O custo da GPU quase não cresce com o tamanho da tela; o da CPU cresce.

**A escolha deste projeto é manter a GPU mesmo assim**, porque o pedágio é fixo
e o `llvmpipe` degrada com a carga: numa cena pesada de verdade (malha grande,
shader complexo) a conta inverte, e é para isso que a máquina existe.

A regra, com o número certo: a GPU compensa quando a cena faria o `llvmpipe`
gastar **mais de ~10 ms por quadro**. Para o `glxgears` (0,9 ms) é absurdo;
para visualização científica, quase sempre vale.

### O canal de volta serializa — e é isso que decide a configuração

Este é o achado que fecha a seção. O `/dev/dxg` não é só lento por quadro: ele
**divide vazão entre clientes**. Medido com `glxgears` a 800x600:

| | FPS por cliente | Vazão total |
|---|---|---|
| 1 cliente na GPU | **76,0** | 76 |
| 2 clientes na GPU | 51,4 e 51,8 | 103 |

Não é serialização pura — a vazão total sobe — mas **cada cliente perde cerca
de um terço**. E com um cliente pesado junto é pior: a 1600x900, um app sozinho
faz 77 FPS; dividindo o canal com o VS Code, cai para **37**.

A consequência é contraintuitiva: **deixar a GPU ligada para todo mundo
prejudica justamente o app que você quer acelerar.** O VS Code e o Firefox são
interfaces 2D — não ganham nada com a placa e tomam metade do canal.

Por isso a configuração deste projeto é:

- **GPU por padrão na sessão** (`startwm.sh`), para que qualquer app 3D novo já
  nasça acelerado, sem prefixo nem lembrete;
- **VS Code e Firefox fora do canal**, pela configuração nativa de cada um, que
  vale independente de como o app é aberto:

| App | Onde | O quê |
|---|---|---|
| VS Code | `~/.config/Code/User/argv.json` | `"disable-hardware-acceleration": true` |
| Firefox | `user.js` do perfil | `user_pref("gfx.webrender.software", true);` |

O `xfwm-atalhos.sh` cria os dois. **Não** dá para resolver isso com variável de
ambiente por aplicativo: o Mesa não expõe seleção de driver no `drirc`
(conferido em `/usr/share/drirc.d/00-mesa-defaults.conf` — não existe
`gallium_driver`), e os `.desktop` dos snaps usam caminho absoluto, então um
wrapper em `/usr/local/bin` não intercepta o lançamento pela interface.

Os dois apps precisam ser **fechados e reabertos** para valer — não é preciso
reiniciar a sessão.

### O resultado

Com VS Code e Firefox fora do canal, `glxgears` a 1600x900, 45 s:

```
68,9   26,9   64,0   62,1   62,9   62,7   62,3   62,4   25,5   63,0  FPS
```

| | |
|---|---|
| Mediana | **62,4 FPS** |
| Teto teórico (`h264_frame_interval=16`) | **62,5 FPS** |
| Amostras ≥ 60 FPS | 7 de 9 |
| Antes do conserto | 37,2 FPS |

**O app encostou no teto de entrega da sessão.** Não é mais a GPU que limita —
é o próprio RDP, que não transmite mais que 62,5 quadros por segundo. Esse é o
melhor resultado possível nesta arquitetura: a placa renderiza mais rápido do
que a sessão consegue mostrar.

Repare que a **média seria 54,6** por causa de duas quedas isoladas a ~26 FPS
(atividade de fundo). Aqui a mediana descreve melhor o comportamento que a
média — mesmo cuidado de método da seção "Fluidez".

Como confirmar que os dois apps saíram mesmo do canal:

```bash
ps -eo cmd | grep -c "type=gpu-process"   # VS Code: 0 = fora
pgrep -af "firefox.*gpu"                  # Firefox: vazio = fora
```

> **Correção (29/07/2026).** Este README afirmava que o `d3d12` era "16 a 24
> vezes mais lento", com "teto de ~30 FPS fixo, que hardware melhor não
> remove", a partir de uma medição de 21–31 FPS. **Estava contaminado:** aquilo
> foi medido sob o xrdp 0.9.24, cujo NSCodec disputava CPU pesadamente. Sob o
> 0.10 a mesma janela dá 103 FPS. O pedágio real é ~10 ms, não ~35 ms. Um
> gargalo estava mascarando o outro — o mesmo erro que a seção "Fluidez"
> descreve.

### Jogos: não

Dois obstáculos, e o primeiro sozinho já basta:

1. **Não há ICD Vulkan para a NVIDIA.** Os instalados são todos do Mesa
   (`nouveau`, `radeon`, `intel`, `lvp`…), nenhum serve, e não há lib Vulkan da
   NVIDIA em `/usr/lib/wsl/lib`. Jogo moderno, Steam/Proton e DXVK são Vulkan.
2. O encode do RDP em CPU e o teto de 62 fps na entrega.

O lugar do jogo é o Windows, onde a 4060 é dona do display. `Ctrl+Alt+Break`
devolve o Windows com a sessão Linux intacta rodando por trás — e o
`abrir-windows` (veja "Jogar no Windows cedendo um monitor") automatiza isso
sem precisar sair do ecrã cheio inteiro.

## Som

Não há placa de som nesta VM (`aplay -l` responde *"no soundcards found"*), e
nenhum servidor de som sobe sozinho — sem desktop environment, ninguém inicia
pipewire nem pulseaudio. O áudio sai por um **sink falso** do PulseAudio que
escreve no canal de áudio do xrdp; quem toca é o mstsc, do lado do Windows.

```bash
sudo bash ~/linux-fullscreen/instalar-som.sh
```

Depois, **encerre a sessão** (`Alt+F3` → "Sair da sessão") — reconectar não
basta, o sesman devolve a mesma sessão. E teste:

```bash
paplay /usr/share/sounds/alsa/Front_Center.wav
```

Está separado do `install.sh` porque **precisa compilar**: não existe pacote
`pulseaudio-module-xrdp` no Ubuntu 24.04, e os módulos usam a API interna do
PulseAudio, que o `libpulse-dev` não expõe. É obrigatório ter a árvore de
fontes do PA na mesma versão instalada, configurada com `meson setup`.

Três detalhes deste setup que enganam:

- **O autostart do módulo não funciona aqui.** O instalador põe um `.desktop`
  em `/etc/xdg/autostart`, e autostart XDG depende de um desktop environment.
  Esta sessão não tem nenhum, então o `startwm.sh` chama o
  `load_pa_modules.sh` na mão.
- **O PipeWire precisa ser mascarado, não só desabilitado.** Os units vêm
  habilitados em escopo *global*; um `systemctl --user disable` não segura, e
  ele volta a disputar o socket do PulseAudio. Os módulos do xrdp são do
  PulseAudio e não carregam no PipeWire.
- **`audiomode:i:0` no `.rdp`.** Com `:2` ("não reproduzir") o cliente recusa o
  canal e não sai som, por mais correto que esteja o lado Linux.

Se não sair som, confira nesta ordem:

```bash
pactl get-default-sink    # tem que dizer xrdp-sink
pgrep -ax pulseaudio      # tem que estar rodando
grep audiomode "Linux Fullscreen.rdp"
```

### Os estalinhos: era falta de prioridade, não driver

**Medido em 30/07/2026.** Som picado, com estalos que apareciam **quando um vídeo
estava tocando**. A tentação é trocar de driver; não era driver nenhum.

```
Max realtime priority     0        <- em /proc/<pulseaudio>/limits
SCHED_OTHER, nice 0                <- chrt -p e /proc/<pid>/stat
default-fragments = 4 x 25 ms      <- 100 ms de buffer, o padrão
```

O `daemon.conf` do PulseAudio pede `realtime-scheduling = yes` e
`nice-level = -11` por padrão, e **as duas coisas falhavam em silêncio**:
`RLIMIT_RTPRIO` valia 0, então ele rodava como processo comum, em nice 0, com
100 ms de folga — disputando CPU com o x264 do xrdp, que a seção "Fluidez" mediu
subindo a **73%** quando a tela se move.

Aí está o encaixe com o sintoma: **vídeo tocando é exatamente o pior momento.**
O encoder trabalha mais justo quando há mais áudio a entregar; o PulseAudio
perde o prazo, e o *underrun* sai como estalo. Não é ruído analógico — não há
placa de som nesta VM para ter ruído. É buraco no fluxo.

Duas correções, e as duas são necessárias:

| Onde | O quê | Por quê |
|---|---|---|
| `/etc/pulse/daemon.conf` | `default-fragments = 4`<br>`default-fragment-size-msec = 50` | 200 ms de buffer em vez de 100 |
| `/etc/security/limits.d/95-pulseaudio-xrdp.conf` | `@audio - rtprio 9`<br>`@audio - nice -11` | faz o `realtime-scheduling` do PulseAudio parar de falhar calado |

O detalhe do buffer que vale entender: são **fragmentos maiores**, não mais
fragmentos. O dobro de folga contra atraso de escalonamento pela **metade** dos
despertares — ganha nos dois eixos ao mesmo tempo, e num ambiente onde CPU é o
recurso escasso isso importa mais que o número final.

O `limits.d` é aplicado pelo **PAM, no login**, então só vale a partir da
próxima sessão. Confirme que pegou:

```bash
grep 'realtime priority' /proc/$(pgrep -x pulseaudio)/limits   # tem que ser 9
chrt -p $(pgrep -x pulseaudio)
```

> **Isto reduz, mas não elimina — e a causa completa é outra.** Medido em
> 30/07/2026, algumas horas depois: o som ficava bom e **voltava a estalar depois
> de uns 20 segundos**. Tocando um tom de 440 Hz e amostrando a latência do sink:
>
> ```
> t=2s  20ms    t=14s   3ms  ←    t=26s   6ms
> t=6s  29ms    t=18s  18ms       t=30s  19ms
> t=10s 48ms    t=22s  31ms       t=34s  35ms
> ```
>
> **Dente de serra**, entre ~3 ms e ~48 ms, ciclo de 12–16 s — e nunca chegando
> aos 200 ms configurados. Cada queda a 3 ms é um estalo, e o primeiro ciclo se
> fecha em ~20 s: exatamente o sintoma.
>
> A causa é **deriva de relógio**. O sink do xrdp não tem relógio de hardware,
> ele simula um; o mstsc consome no cristal real da placa do Windows. A diferença
> acumula, o buffer infla e periodicamente há um resync. Buffer e prioridade
> tratam *jitter de CPU*, que é problema real mas não este — por isso melhoram e
> não resolvem.
>
> **A eliminação é estrutural: áudio USB nativo.** Veja "Transferir áudio,
> microfone e câmera". Com o headset anexado por USB/IP a latência fica em
> **208–217 ms** (amplitude de 9 ms contra 45 ms), em 48 kHz nativo, sem
> reamostrar. Mantenha os ajustes desta seção de qualquer forma: eles valem
> sempre que o áudio estiver no caminho do RDP.

Se ainda estalar com a prioridade valendo **e** o áudio no caminho do RDP, o
próximo passo é aceitar mais latência (`default-fragment-size-msec = 80`), não
trocar de driver — a taxa de 44100 Hz e o reamostrar de 48 kHz do navegador são
limpos e não fazem estalo.

### Microfone

**Já funciona, e não precisou de nada no lado Linux.** O
`pulseaudio-module-xrdp` instala os **dois** módulos, e o `xrdp-source` já estava
carregado e como source padrão:

```bash
pactl list sources short     # xrdp-source tem que aparecer
```

O que faltava era **uma linha no `.rdp`**: o cliente não estava oferecendo o
canal de entrada.

```ini
audiocapturemode:i:1      # era 0 — "não gravar"
```

Mesmo par de armadilhas do `audiomode`: por mais correto que esteja o lado
Linux, com `:0` o mstsc não abre o canal e não existe microfone nenhum para o
Firefox enxergar.

> **Mexer no `.rdp` invalida a assinatura.** Depois de editar, re-assine — é o
> mesmo caminho que o `abrir-windows` usa a cada partida, e roda de dentro da
> sessão:
>
> ```bash
> TP=$(tr -d ' \r\n' < /mnt/c/Users/<você>/AppData/Local/linux-fullscreen/thumbprint.txt)
> cd /tmp && /mnt/c/Windows/System32/rdpsign.exe /sha256 "$TP" \
>     "C:\Users\<você>\Desktop\Linux Fullscreen.rdp"
> ```

> **O `.rdp` da Área de Trabalho é UTF-16LE, não ASCII.** O do repositório é
> texto comum, mas o mstsc reescreve o dele em UTF-16 ao salvar — e aí `sed` e
> `grep` com padrão ASCII **não casam e não avisam**: o `sed -i` roda, devolve
> sucesso e não muda nada. Editar aquele arquivo se faz por bytes:
>
> ```python
> b = open(f, "rb").read()
> b = b.replace("audiocapturemode:i:0".encode("utf-16-le"),
>               "audiocapturemode:i:1".encode("utf-16-le"))
> ```

**Dois microfones, e o do RDP é o pior dos dois.** Depois que a passagem USB
existiu (veja "Transferir áudio, microfone e câmera"), há dois caminhos: o canal
do RDP, que compartilha o microfone padrão do Windows, e o headset anexado
nativamente, que traz o microfone dele junto. O nativo é melhor — sem
compressão, sem o canal.

O canal do RDP continua valendo porque cobre o caso em que o headset está no
Windows e você quer falar aqui. Mas ele **tem um custo escondido**: é justamente
o `audiocapturemode:i:1` que faz o mstsc abrir um stream de captura no
dispositivo padrão do Windows, e esse stream é um dos dois detentores que travam
o `attach`. As duas funcionalidades disputam o mesmo aparelho físico. Está
resolvido (o `transferir-usb` reconecta o mstsc), mas é a explicação de por que
mexer numa mexe na outra.

### Webcam: não pelo RDP — mas há um caminho

Duas coisas separadas, e confundi-las leva a procurar configuração que não
existe.

**1. Pelo RDP, não dá.** O redirecionamento de câmera do Windows é o
MS-RDPECAM, e **o xrdp não implementa esse canal.** Não é opção desligada; o
código não está lá. O que o chansrv implementa:

```bash
strings /usr/sbin/xrdp-chansrv | grep -oE '^(rdpsnd|cliprdr|rdpdr|rail)$' | sort -u
#   cliprdr   rdpdr   rail   rdpsnd      <- e nada de camera
```

Por isso `camerastoredirect:s:*` no `.rdp` não produz efeito nenhum aqui,
diferente do que produziria contra um Windows do outro lado.

**2. Por USB, dá — e o kernel desta WSL já tem tudo.** Verificado em
30/07/2026, no kernel `6.18.33.2-microsoft-standard-WSL2`:

```bash
ls /lib/modules/$(uname -r)/kernel/drivers/usb/usbip/   # usbip-core, usbip-host, vhci-hcd
ls /lib/modules/$(uname -r)/kernel/drivers/media/usb/uvc/   # uvcvideo.ko
```

Isso é novidade que vale registrar: kernels antigos da WSL **não** traziam
`uvcvideo`, e a resposta corrente na internet ainda é "precisa compilar kernel
próprio". Aqui não precisa.

A metade Windows é o `usbipd-win`. **Feito em 30/07/2026** — veja "Transferir
áudio, microfone e câmera", que é a seção que descreve como isso virou um botão.
Do lado Linux, `/dev/video0` e `/dev/video1` aparecem e o Firefox vê a câmera.

**O preço, que decide se vale:** enquanto anexada, a câmera é **exclusiva da
WSL** — o Windows perde o acesso.

> **Resolvido em 31/07/2026 — e a conclusão foi abandonar o USB/IP para vídeo.**
> A pendência acima ("o Meet reconhece a câmera mas não mostra imagem") era
> **banda isócrona**, não negociação de formato. O caminho que funciona é a
> ponte por rede: veja **"Webcam por rede"**, logo abaixo. Esta seção fica como
> registro de por que o USB/IP não serve — a medição está lá.

## Webcam por rede (31/07/2026)

O sintoma era "o Meet detecta a câmera e não mostra imagem". A causa não era o
Meet, nem o formato, nem permissão. Era **banda isócrona no USB/IP**.

### A medição que fechou o diagnóstico

O primeiro dado veio de ler o formato negociado **enquanto o navegador segurava
o dispositivo** — o `VIDIOC_G_FMT` é só leitura e não precisa tomar a câmera:

```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
#   Width/Height : 640/480
#   Pixel Format : 'YUYV' (YUYV 4:2:2)
#   Size Image   : 614400
#   Frames per second: 30.000
```

614.400 bytes por quadro × 30 = **18,4 MB/s**, sem compressão. O mesmo teste com
MJPG dava 21.930 bytes por quadro — **0,66 MB/s, 28 vezes menos**. E o teto real
do canal, medido capturando direto com o `v4l2-ctl`:

| modo | fps | taxa | veredito |
|---|---|---|---|
| MJPG 640x360 | 13,2 | 0,25 MB/s | limítrofe |
| MJPG 640x480 | 10,4 | 0,20 MB/s | ruim |
| MJPG 1280x720 | **0** | — | sem vídeo |
| YUYV 640x480 | **0** | — | sem vídeo |

**O canal satura em ~0,25 MB/s.** Não é limite de rede — a rede WSL↔Windows é
virtual. É o agendamento isócrono degradado, e o `dmesg` diz o porquê, umas
2.800 vezes por segundo:

```
usb usb1: Not yet implemented          <- vhci_get_frame_number
vhci_hcd: urb->status -104             <- ECONNRESET, URBs cancelados
```

O `vhci_hcd` **não implementa** `get_frame_number`. Não há ajuste, opção de
módulo ou versão de `usbipd` que corrija isso.

> **O chuvisco não era ruído de imagem, era o diagnóstico.** No
> `webcamtests.com` a área de preview mostrava estática, não preto. Preto seria
> "nenhum dado"; estática é **dado parcial** — a minoria de pacotes isócronos
> que sobrevive, preenchendo um quadro que o `uvcvideo` entrega pela metade.

### Por que forçar MJPG não resolveria

Brave e Firefox escolhem **os dois** `YUYV 640x480@30`. Não é peculiaridade de
um navegador: é a preferência normal do backend V4L2, que evita o custo de
decodificar JPEG quando a câmera é local. Sensata no notebook, péssima aqui.

E mesmo forçando MJPG o teto seria 13 fps a 360p. Não vale.

### O desenho que ficou

```
Windows                                       WSL2
camera --dshow--> ffmpeg.exe --TCP/NUT--> ffmpeg --> /dev/video10 --> navegador
```

Três decisões que não são óbvias e custaram tempo:

**1. Quem escuta é o Linux; quem conecta é o Windows, em `127.0.0.1`.** Isso
funciona porque o `.wslconfig` tem `localhostForwarding=true`. O sentido inverso
exigiria o IP da WSL — que muda a cada boot — e ainda passaria pelo firewall.
Verificado: `Test-NetConnection 127.0.0.1 -Port 5004` → `True`.

**2. O contêiner é NUT, não MPEG-TS.** O TS não tem tipo de stream para MJPEG.
O ffmpeg empacota como dado privado e avisa em letra miúda; o receptor lê
`Data: bin_data` e morre com `Output file does not contain any stream`. Custou
uma rodada inteira de depuração achando que era rede.

**3. É liga/desliga, não permanente.** A câmera no Windows é **exclusiva**:

```
primeira captura:  frame=  18
segunda captura:   I/O error       frame=   0
```

Se a ponte ficasse de pé sempre, o Windows perderia a câmera do mesmo jeito que
perdia com o USB/IP — só que sem botão para trazê-la de volta.

### O resultado

```
/dev/video10:  1280/720  'YU12'      frames=90   fps=31.6
```

E há um efeito colateral que resolve o problema pela raiz: o `v4l2loopback` só
expõe **o que o ffmpeg escreve nele**. Não existe YUYV para o navegador
escolher. O impasse de "os dois navegadores pedem o formato que não cabe"
deixou de existir por construção, não por configuração.

### Áudio continua no USB/IP, e não é incoerência

| | câmera | áudio |
|---|---|---|
| problema | **banda** — 18,4 MB/s num canal de 0,25 | **relógio** — sink do xrdp oscilando 3–48 ms |
| USB/IP | não aguenta | aguenta, e dá relógio de hardware (208–217 ms estável) |
| rede | resolve | reintroduziria o problema |

O headset pede **0,18 MB/s** (48 kHz × 2 canais × 2 bytes) contra o teto medido
de 0,25 MB/s. **Ele passa por 28% de margem.** É a mesma limitação; só que o
áudio cabe nela e o vídeo não. Cada dispositivo pelo caminho que a física dele
pede.

*(A previsão de que uma ponte de áudio por rede traria os estalos de volta é
inferência a partir do que já foi medido sobre o sink do xrdp, não medição
nova.)*

### O v4l2loopback e a manutenção que ele cria

O módulo não vem no kernel da WSL e não tem pacote. Compilar exigiu a fonte na
tag exata (`linux-msft-wsl-6.18.33.2`) e **o build completo do kernel** — o
`Module.symvers`, que carrega os CRCs de cada símbolo exigidos pelo
`CONFIG_MODVERSIONS=y`, só nasce da passada global do `modpost`.

> **A armadilha que custou uma hora:** sem o `pahole` instalado, o
> `CONFIG_DEBUG_INFO_BTF_MODULES` cai silenciosamente da config no
> `olddefconfig` — e esse símbolo **acrescenta campos ao `struct module`**. O
> erro resultante não menciona pahole nem BTF:
>
> ```
> .gnu.linkonce.this_module section size must match the kernel's
> built struct module size at run time
> ```
>
> Confira sempre com `diff` entre `/proc/config.gz` e o `.config` gerado.

Detalhe elegante que explica outra confusão: o `vermagic` do módulo ficava com
um `+` a mais (`...WSL2+`) e mesmo assim carregava. O kernel faz isto:

```c
static int same_magic(const char *amagic, const char *bmagic, bool has_crcs)
{
    if (has_crcs) {          /* modulo traz CRCs: pula a string de versao */
        amagic += strcspn(amagic, " ");
        bmagic += strcspn(bmagic, " ");
    }
    return strcmp(amagic, bmagic) == 0;
}
```

Com CRCs presentes o nome da versão deixa de ser conferido — os CRCs são a
verificação de verdade, e são mais rigorosos.

**A manutenção:** quando a WSL trocar de kernel, o módulo para de carregar e a
câmera some. O DKMS não serve aqui, porque precisa dos headers do kernel novo e
a WSL não os publica por `apt`. Por isso existe
[`compilar-v4l2loopback`](compilar-v4l2loopback), que refaz tudo (~15 min com
`-j10`; a árvore em `~/kernel-src` é reaproveitada).

> **`/lib/modules` é volátil na WSL — descoberto no primeiro reboot,
> 31/07/2026.** O módulo instalado ali funcionou a sessão inteira e
> **desapareceu no arranque seguinte**, com o kernel exatamente o mesmo. A causa:
>
> ```
> none on /usr/lib/modules/6.18.33.2-microsoft-standard-WSL2 type overlay
>   lowerdir=/modules
>   upperdir=/lib/modules/6.18.33.2-microsoft-standard-WSL2/rw/upper
> ```
>
> A WSL monta um overlay e **recria a camada `upper` a cada boot**. O sintoma
> engana: `modprobe` diz "not found" sem nada ter mudado, o que manda investigar
> compilação e versão de kernel — os lugares errados.
>
> Por isso o `.ko` vive em `/usr/local/lib/v4l2loopback/` e entra por
> [`v4l2loopback.service`](v4l2loopback.service), com `insmod` de caminho
> absoluto. O `modprobe` não serviria: ele só procura em
> `/lib/modules/$(uname -r)`, que é justamente o diretório volátil.

Duas armadilhas menores que o mesmo reboot revelou, ambas resolvidas no
`.service`:

**`insmod` não resolve dependência.** O carregamento morria com
`Unknown symbol v4l2_device_unregister (err -2)`. Falta o `videodev`, que antes
vinha de graça porque o `uvcvideo` o puxava junto com a câmera por USB/IP — com
a câmera no Windows, ninguém mais o carrega. Daí o
`ExecStartPre=/sbin/modprobe videodev`.

**A permissão do `/dev/video10` é uma corrida.** No boot ele nasceu
`crw------- root:root` e o navegador não conseguiria abri-lo; dentro de uma
sessão já quente o udev chegava a tempo e punha `group=video`. O `ExecStartPost`
grava `chgrp video` e `0660` explicitamente, em vez de torcer.

## Transferir áudio, microfone e câmera (30/07/2026)

Dois botões na barra passam o **headset USB** e a **webcam** entre o Windows e
esta sessão, com um clique:

```
  │ Audio: Linux │   │ Camera: Win │
```

`Audio: Linux` significa "está aqui"; `Audio: Win`, "está no Windows";
`Audio: --`, que falta o `usbipd bind` ou o aparelho está desligado. O `...` é o
estado pendente, para o clique não parecer inerte.

> **Correção (31/07/2026).** Os dois botões **não fazem mais a mesma coisa por
> baixo.** O de áudio continua sendo transferência USB, como descrito aqui. O de
> câmera passou a ligar e desligar a **ponte de rede** (veja "Webcam por rede"),
> e o dispositivo USB nunca sai do Windows. Por isso o rótulo dele diz
> **`Camera: Rede`** e não `Camera: Linux`: dizer "Linux" sugeriria que a câmera
> mudou de lado, e ela não muda — enquanto a ponte está ligada, o `ffmpeg.exe`
> a segura no Windows e nenhum outro app de lá consegue abri-la.
>
> Tudo que esta seção diz sobre a câmera daqui para baixo (o `usbipd bind`, o
> `transferir-usb camera`, a exclusividade do passthrough) **continua verdade
> sobre o mecanismo antigo**, que ainda funciona por linha de comando — mas
> deixou de ser o caminho usado, porque entregava chuvisco.

Quem faz o trabalho é [`transferir-usb`](transferir-usb), que também serve por
linha de comando:

```bash
transferir-usb audio  linux      # traz o headset para cá
transferir-usb camera windows    # devolve a webcam
transferir-usb audio  status     # linux | windows | nao-compartilhado | ausente
```

**Por que existe:** é a eliminação estrutural dos estalos (veja "Os
estalinhos"). Com o headset anexado, o áudio vira ALSA nativo com relógio de
hardware de verdade — latência de **208–217 ms** estável, contra o dente de serra
de 3–48 ms do sink do xrdp. E o microfone vem junto, sem depender do canal RDP.

**O preço, que é a razão de haver dois botões e não um:** o dispositivo é
**exclusivo**. Enquanto está aqui, o Windows não o tem. Por isso áudio e câmera
são separados — bundlá-los obrigaria a tudo-ou-nada, e o caso útil é justamente
áudio nativo aqui *com a câmera ainda no Windows* para o Meet.

### Casa e trabalho: ele acha o headset sozinho (31/07/2026)

O headset do trabalho é outro, com outro VID:PID. Chumbar um só quebraria em um
dos dois ambientes, então a resolução é automática e a regra é
**"está presente?"**:

| Situação | O que acontece |
|---|---|
| o ID gravado está presente (qualquer lado) | não mexe em nada, custo zero |
| não está, e há **um** áudio USB no Windows | adota e grava, sem perguntar |
| não está, e há **vários** | pergunta uma vez (zenity) e grava |
| não está, e não há nenhum | mantém o que estava — pode estar só desligado |

A precedência, do mais fraco ao mais forte — o último lido vence:

```
valores no proprio script  ->  audio-detectado.conf  ->  dispositivos.conf
   (chute inicial)              (automatico)             (sua escolha, ganha)
```

**Não há assinatura de ambiente** como o `abrir-windows` faz com monitores, e foi
decisão deliberada: aqui a *presença* do aparelho já é o discriminador, e se
corrige sozinha ao trocar de mesa — sem precisar reconhecer "que ambiente é
este".

A detecção pergunta ao Windows quais endpoints de áudio estão atrás de um USB
(`audio-padrao.ps1 -ListarUsb`), porque um headset recém-ligado aparece primeiro
lá. Forçar a redetecção:

```bash
transferir-usb detectar
```

> **Armadilha: `grep` engole a saída do PowerShell.** Ele devolve os acentos na
> codificação do console do Windows (CP-850/1252), não em UTF-8. O `grep` vê
> bytes inválidos, declara `binary file matches` e **descarta todas as linhas** —
> a detecção falhava dizendo "nenhum áudio USB encontrado". É preciso
> `grep -a` mais um `tr -cd '[:print:]\t\n'`. O VID:PID é ASCII, então só o nome
> exibido perde os acentos.

### Pré-requisito, uma vez por máquina

Em **PowerShell administrador**:

```powershell
winget install --id dorssel.usbipd-win
```

Feche e reabra o PowerShell (o `usbipd` só entra no `PATH` numa sessão nova),
descubra os BUSID e compartilhe:

```powershell
usbipd list
usbipd bind --busid 1-3      # o headset, no exemplo desta máquina
usbipd bind --busid 1-6      # a webcam
```

O `bind` **não tira nada do Windows** — só marca como compartilhável, e é
persistente. O `attach` e o `detach` que o botão usa **não** precisam de
administrador; foi isso que tornou o botão possível, já que o token do interop
da WSL vem filtrado e nada elevado roda de dentro da sessão.

> Os comandos acima são um **modelo**: `<BUSID>` é para substituir pelo número
> real. O PowerShell reserva o `<` e devolve *"Operador '<' reservado para uso
> futuro"* se você colar literalmente. Rode uma linha por vez.

Os dispositivos são identificados por **VID:PID**, não por BUSID — o busid muda
se você trocar a porta USB, e este projeto não tem calibração em lugar nenhum.
Sobrescreva em `~/.config/linux-fullscreen/dispositivos.conf`:

```bash
ID_AUDIO="046d:0adf"      # G435 Wireless Gaming Headset
ID_CAMERA="0408:403a"     # ACER HD User Facing
```

### O que trava o `attach`, e as duas coisas que resolvem

O sintoma é sempre este, e a mensagem do `usbipd` engana ao culpar "software":

```
WSL usbip: error: Attach Request for 1-3 failed - Device busy (exported)
usbipd: warning: The device appears to be used by Windows
```

**Não há `--force` no `attach`** (só no `bind`, e ele significa *"the host cannot
use the device"* — permanente, o que quebraria o vaivém). Então o Windows tem
que soltar o aparelho de verdade. São **dois** detentores, e é preciso tratar os
dois:

**1. O serviço de áudio, enquanto o headset for o dispositivo padrão.** Resolvido
pelo [`windows/audio-padrao.ps1`](windows/audio-padrao.ps1), que troca o padrão
do Windows por script. **Não instala nada e não precisa de administrador**: o
Windows não tem comando nativo para isso, e as receitas correntes mandam
instalar `nircmd`, `SoundVolumeView` ou o módulo `AudioDeviceCmdlets` — todas
chamam por baixo a mesma interface COM não documentada, a `IPolicyConfig`, que dá
para instanciar direto.

**2. O `mstsc`.** Este é o que custou mais para achar. Ele abre stream de áudio
nos dispositivos padrão do Windows **no momento em que conecta** — um de
reprodução (para tocar o áudio desta sessão) e um de captura (por causa do
`audiocapturemode:i:1`) — e **não os larga quando o padrão muda depois**. Trocar
o padrão e esperar 12 s não resolvia. Quem apontou o culpado:

```powershell
Get-Process | Where-Object { $_.Modules.ModuleName -contains "AUDIOSES.DLL" }
#   brave  brave  explorer  ipf_helper  mstsc  RtkAudUService64  ...
```

A correção é **fechar e reabrir o mstsc** na troca, o que o `transferir-usb` faz
sozinho: ao subir de novo ele abre o dispositivo que for padrão *naquele
momento*. Custa uns 3–4 s de tela piscando; o ciclo completo leva ~6 s.

Reconecta nas **duas** direções, e a de volta não é simetria gratuita: o mstsc em
execução tinha aberto o alto-falante, então sem reconectar o áudio da sessão
continuaria saindo por ele com o headset ocioso ao lado.

> **Isso só é seguro por causa do `Policy=Default`** no `/etc/xrdp/sesman.ini` —
> veja "Por que a sessão sobrevive à troca". Fechar o mstsc apenas desconecta. Se
> alguém puser `Policy=UBD`, este botão passa a destruir trabalho.

**Trava de segurança:** se o `attach` falhar, o mstsc reabre de qualquer forma.
Sem isso você perderia a tela numa falha — e a mensagem de erro seria desenhada
numa sessão que ninguém está vendo.

### Três armadilhas do lado Windows

**O Windows não chama o headset de "G435".** Para ele é *"Fone de ouvido do
headset (Tecnologia Intel Smart Sound para áudio USB)"* — genérico e traduzido. O
nome "G435" só existe no descritor USB que o `usbipd` mostra. Casar por nome
falhou nas duas pontas: o `-Evitar` não excluía nada (acertava por sorte, pegando
o primeiro da lista) e o `-Restaurar` não achava nada. O vínculo correto está na
propriedade **39** do endpoint, no registro:

```
{b3f8fa53-0004-438e-9003-51a46e139bfc},39 = {1}.USB\VID_046D&PID_0ADF&MI_01\...
```

É o mesmo VID:PID que o `usbipd` usa, então as duas metades falam a mesma língua
— e não quebra num Windows de outro idioma.

**O `DeviceState` do registro é bitfield, não enum.** Valem `4`, `8`,
`0x20000004`, `0x22000004`… O filtro tem que ser `-band 1` (o bit ACTIVE);
comparar com `-eq 1` devolve lista vazia e o script sai calado.

**O PowerShell não converte objeto COM para interface.**
`[IPolicyConfig] (New-Object PolicyConfigClient)` falha com *"Não é possível
converter o valor PolicyConfigClient no tipo IPolicyConfig"* — ele não faz
`QueryInterface` em classe `ComImport`. A conversão tem que ficar **dentro do
C#**, onde é um `QueryInterface` de verdade. E na declaração da interface a
**ordem dos métodos é o que importa**: são slots de vtable, não nomes. Os nove
primeiros existem só para empurrar o `SetDefaultEndpoint` para o slot 10.

### Duas armadilhas do lado Linux

**`setsid --fork`, não `( cmd & )`.** Lançar o mstsc com
`( setsid "$MSTSC" "$perfil" & )` fazia a chamada do `transferir-usb` nunca
retornar. Medido em 31/07/2026, com um `PING.EXE` de 20 s no lugar do mstsc:

| | com a saída lida por um pipe | sem pipe |
|---|---|---|
| `( setsid cmd & )` | **21 s** | 1 s |
| `setsid --fork cmd </dev/null` | 1 s | 1 s |

**Não é o bash esperando um filho** — sem pipe o script retorna na hora. O que
prende é o **descritor de saída**: o proxy de interop do WSL fica com a saída
*original* do script, apesar do `>/dev/null 2>&1`. Dá para ver nos descritores
dele enquanto roda:

```bash
ls -l /proc/<pid-do-proxy>/fd
#  fd1 -> .../a-saida-de-fora     <- e nao /dev/null, como pedimos
```

Então quem trava é **quem estiver lendo** a saída — um pipe, uma substituição de
comando, ou o processo que chamou o script. Como o proxy só morre quando você
fecha a janela do Windows, a espera é indefinida.

> **Correção (31/07/2026).** A primeira versão desta nota dizia que "o bash
> ficava preso em `do_wait` esperando o processo de interop". O `do_wait` foi
> observado de verdade, mas a medição acima mostra que a causa é o descritor
> herdado, não a espera por um filho. Conselho diferente: não adianta trocar
> como o processo é bifurcado se a saída continuar sendo herdada.

> **O mesmo padrão está no `abrir-windows`** (`abrir_mstsc`, linha 254). Lá não
> incomoda **porque ninguém lê a saída dele**: é chamado pelo appfinder ou por um
> terminal interativo. Passa a incomodar no dia em que alguém o chamar de um
> script com `$( )` ou com pipe — aí trava até a janela do jogo fechar.

**O cache da barra precisa ser revalidado.** A barra **não** chama o
`usbipd.exe` a cada redesenho — uma chamada de interop leva quase um segundo e
travaria o desenho. Ela lê um cache em `$XDG_RUNTIME_DIR/transferir-usb.estado`.
Mas esse cache só era reescrito quando *nós* agíamos, e por isso mentia quando o
estado mudava por fora: visto dizendo `camera=windows` enquanto o `usbipd` a
mostrava como `Attached`. A barra agora pede um refresh a cada ~30 s, solto.

O cache vive no `XDG_RUNTIME_DIR`, então morre com a sessão — o que é correto,
porque um `wsl --shutdown` devolve tudo ao Windows de qualquer jeito. O padrão
volta a ser "Windows dono", que é o seguro para quando você abre um jogo.

### Sair da sessão devolve tudo

O `linux-desktop-down` chama o `transferir-usb` antes de encerrar, **nos dois
modos**, e o motivo é diferente em cada um:

| Modo | Por que precisa |
|---|---|
| "Sair da sessão" | a VM continua ligada, então o aparelho fica **preso na WSL** |
| "Desligar" (VM) | o `wsl --shutdown` devolve o dispositivo sozinho, **mas não desfaz a troca de dispositivo padrão** no Windows |

O segundo é o que engana: parece que o `wsl --shutdown` resolve tudo, e resolve
metade — sem passar por aqui, o Windows volta com o som no alto-falante e o
headset ocioso.

Roda **antes** dos `pkill`, de propósito: depois que o xfwm4 morre a sessão cai e
não haveria mais quem chamasse o `usbipd`. Leva alguns segundos (o Windows
precisa reenumerar o USB), por isso a barra de progresso do zenity. Ali o vaivém
do mstsc é desligado com `TRANSFERIR_SEM_RECONECTAR=1` — a sessão está acabando,
e reabrir o cliente para matá-lo dois segundos depois só atrasaria.

### Diagnóstico

```bash
transferir-usb estado                              # reescreve o cache e mostra
cat /run/user/1000/transferir-usb.log              # a saida crua do usbipd
cd /tmp && '/mnt/c/Program Files/usbipd-win/usbipd.exe' list
```

Estados do `usbipd list`: `Not shared` (falta o `bind`), `Shared` (no Windows,
pronto), `Attached` (aqui). Do lado Linux, `aplay -l` e `ls /dev/video*`.

E para ver o que o Windows está oferecendo de áudio:

```bash
cd /tmp && powershell.exe -ExecutionPolicy Bypass \
  -File "$LOCALAPPDATA\linux-fullscreen\audio-padrao.ps1" -Listar
```

O `.ps1` se copia sozinho para o `%LOCALAPPDATA%` quando está mais novo que a
cópia de lá — editar no repositório basta, não há passo de instalação no Windows.

## Abrir coisas do Windows cedendo um monitor

> **Chamava-se "Jogar no Windows cedendo um monitor" e o script, `jogo-windows`,
> até 02/08/2026.** Generalizou para qualquer coisa do Windows — jogo, Word,
> PDF. O porquê está em "Por que não VNC, e por que não custou código",
> logo abaixo. Se você chegou aqui procurando `jogo-windows`, é este arquivo.

A coisa continua sendo do Windows — a seção "Jogos: não" não mudou. O que muda é
que não é mais preciso abandonar a sessão inteira com `Ctrl+Alt+Break` para
usá-la. O `abrir-windows` (`Alt+F3` → "Abrir no Windows") **encolhe a sessão
para um monitor só** enquanto ela está aberta, e devolve o multimonitor quando
você fecha.

O ganho é estrutural, não de conforto: **nenhum pixel do jogo passa pelo x264 do
xrdp**. Ele é desenhado pela 4060 direto no painel, como sempre foi. Não há
pedágio de codificação, não há o teto de 62 fps da seção "Fluidez", e a latência
do mouse é a nativa do Windows. Qualquer solução que trouxesse a imagem do jogo
*para dentro* da sessão pagaria dois encodes em série — é por isso que ceder o
monitor ganha de transmitir a tela, e não é perto.

**E não é preciso `Ctrl+Alt+Break`.** Verificado em 29/07/2026: com o mstsc em
ecrã cheio de **um** monitor, o cursor atravessa livremente para o monitor
vizinho, onde está o jogo. Ele só fica preso quando o mstsc está em multimonitor
cobrindo tudo — aí não há para onde sair. Passar de um lado para o outro é
arrastar o mouse, como entre duas janelas quaisquer.

### Por que não VNC, e por que não custou código (02/08/2026)

A pergunta que abriu esta mudança foi outra: *editar `.docx` no Word de verdade,
e o VNC não seria o caminho?* No começo do projeto o VNC tinha sido cogitado
para trazer ferramentas do Windows para cá, e como protocolo ele é de fato
melhor escolha que o Sunshine+Moonlight de "Moonlight, adiado": manda retângulo
sujo em vez de vídeo, e a tela do Word fica parada quase o tempo todo.

**Foi recusado assim mesmo**, por três motivos que não se resolvem trocando de
software:

- **A recursão é a mesma que adiou o Moonlight.** O servidor VNC no Windows
  captura a área de trabalho, que neste momento é o mstsc em ecrã cheio
  mostrando a sessão Linux. A janela mostraria a si mesma. A saída seria um
  display virtual no Windows — o mesmo trabalho de antes, só que agora com um
  serviço VNC a mais rodando como SYSTEM.
- **Word é aplicativo de digitar, e é o pior caso para caminho aninhado.** O eco
  de cada tecla atravessaria Linux → VNC → Windows → renderiza → captura → VNC →
  Linux → x264 do xrdp → mstsc. A análise de 29/07/2026 mediu ~60–100 ms para
  dois encodes em série e concluiu *"inviável para jogo; aceitável para app
  comum"*. Word não é app comum nesse critério: ler um `.docx` assim é
  tranquilo, escrever um é cansativo.
- **Não economiza RAM, que é a premissa permanente.** A WSL2 não tem memória
  própria — é recortada da mesma RAM física do Windows. Rodar o Word "no
  Windows" não move o custo para fora: soma o Word, mais o servidor VNC, mais o
  cliente segurando um framebuffer da área de trabalho remota inteira.

**E o mecanismo já existia.** Lendo o então `jogo-windows` atrás do que faltava,
não faltava nada: o `lancar()` entrega o caminho ao `start ""` do Windows, e
quem resolve executável, atalho e associação de arquivo é o shell de lá. Medido
com `ftype` em 02/08/2026:

```
.docx=Word.Document.12
Word.Document.12="C:\Program Files\Microsoft Office\Root\Office16\WINWORD.EXE" /n "%1" /o "%u"
```

Ou seja, um `.docx` largado na pasta abre no Word nativo, no monitor cedido, com
fidelidade total, latência nativa e zero pedágio de transporte — que é
exatamente o que se queria do VNC, e melhor. O trabalho todo foi **mudar nome e
filtro**: a pasta virou `Coisas`, o filtro de extensões virou filtro de lixo, e
o script virou `abrir-windows`.

> **O filtro passou de lista branca para lista negra, e isso é deliberado.** Até
> aqui ele só aceitava `.lnk/.url/.exe/.bat/.cmd`, o que fazia sentido numa
> pasta chamada `Jogos` — tudo ali era lançador. Numa pasta chamada `Coisas` a
> lista branca decide por você que `.docx` não é coisa, e o arquivo largado lá
> **some do menu sem dizer nada**, que é o pior modo de falhar. Agora passa
> qualquer arquivo e só o lixo do próprio Windows fica de fora: `desktop.ini`,
> `Thumbs.db` e ocultos. Verificado em 02/08/2026 — um `desktop.ini` posto na
> pasta de propósito não apareceu na listagem.

### Como ele escolhe o monitor — e por que não usa `selectedmonitors`

O caminho óbvio seria `selectedmonitors:s:<id>` no `.rdp`. **Foi descartado**, e
vale saber por quê antes de alguém "simplificar" isso de volta:

- os IDs do mstsc só aparecem numa **caixa de diálogo** (`mstsc /l`), sem
  nenhuma forma de ler por script;
- a numeração **não segue** a ordem do `[System.Windows.Forms.Screen]::AllScreens`
  do PowerShell. Medido em 29/07/2026: o `AllScreens` lista o primário de
  2560x1080 em primeiro, mas para o mstsc ele é o **ID 1**;
- monitores idênticos ficam **indistinguíveis** — não há como saber qual é qual.

O `winposstr` resolve os três de uma vez, porque trabalha em **coordenadas do
desktop do Windows**, que o PowerShell entrega exatas:

```ini
use multimon:i:0
winposstr:s:0,1,<esq>,<topo>,<dir>,<base>
```

Um retângulo dentro do monitor desejado, e o mstsc entra em ecrã cheio ali.
Testado e confirmado em 29/07/2026. É o que torna o script portátil: chegou
noutro ambiente com três monitores, ele calcula as coordenadas novas sozinho —
**não existe calibração**.

### Escolher o monitor, e o botão "Identificar"

O script **pergunta em qual monitor o jogo abre**, toda vez. Não é preguiça de
adivinhar: é a única resposta que não pode errar, e corrigir custa um clique.
A escolha anterior daquele ambiente aparece marcada com `[ultima vez]`.

Como saber qual é qual num ambiente estranho, a lista tem um item
**"Identificar monitores"**, que pisca `MONITOR 1`, `MONITOR 2`… na tela — o
mesmo que o botão "Identificar" das Configurações do Windows. Sozinho:

```bash
abrir-windows --identificar
```

O que faz isso funcionar é a **numeração por posição**: os monitores são
contados da esquerda para a direita (e depois de cima para baixo), critério
aplicado **igual dos dois lados** — na lista, que vem do PowerShell, e no
overlay, que vem do `xrandr`. Por isso o número que pisca é o número da lista,
sem depender de nenhuma correspondência entre os IDs do mstsc e as saídas do
xrdp — que, como a seção anterior mostra, não são a mesma coisa.

Os números desenhados vivem **dentro da sessão Linux**, que neste momento cobre
todos os monitores físicos. Nada é desenhado do lado Windows.

O ambiente é reconhecido sozinho: o script guarda as escolhas em
`~/.config/linux-fullscreen/monitores.conf`, indexadas por uma **assinatura do
conjunto de monitores** (todas as geometrias, ordenadas). Casa e trabalho geram
assinaturas diferentes, então cada um tem a sua memória.

```bash
abrir-windows --listar       # o que o Windows está reportando agora
abrir-windows --perfil       # gera o .rdp e mostra, sem mexer na sessão
```

### A lista é uma pasta (31/07/2026, generalizada em 02/08/2026)

A fonte da verdade é uma pasta `Coisas` na área de trabalho do Windows. **Arrasta
para lá o que você quer abrir** — atalho de jogo ou o próprio documento — e ele
aparece no menu do `abrir-windows` e no botão "Coisas" da barra, sem editar
arquivo nenhum e sem reiniciar nada.

```
C:\Users\<você>\Desktop\Coisas\
    Hollow Knight Silksong.url      <- atalho de Steam
    League of Legends.lnk           <- atalho do instalador
    TCC.docx                        <- o arquivo mesmo; abre no Word
    Contrato.pdf                    <- abre no leitor de PDF padrão
```

> **A pasta chamava-se `Jogos` até 02/08/2026.** O nome antigo continua sendo
> procurado logo depois do novo, para a renomeação no Windows não ser obrigatória
> no mesmo minuto em que o script muda; se as duas existirem, `Coisas` ganha por
> vir primeiro na busca. Isso **dobrou o custo do glob** — medido no mesmo dia:
> 580 ms com um nome só, 1150 ms com os dois, pelo mesmo motivo de sempre
> (dobrou a travessia de diretório no drvfs). É preço pago só quando o cache
> falha; o caminho normal continua sendo um teste de diretório, medido em 5 ms.

**Por que atalho serve, e não só a linha de comando.** Vários jogos só sobem pelo
atalho que o instalador criou: ele carrega o diretório de trabalho e os argumentos
certos, e reproduzir isso à mão é um jeito de errar. Entregando o `.lnk` ao shell
do Windows (`start "" "...lnk"`), quem resolve tudo é o próprio Windows. Por isso
**não há parser de `.lnk` aqui** — o formato é binário e não precisamos dele. O
`.url` do Steam passa pelo mesmo caminho, e o `.docx` também: é a mesma chamada.

O nome é o nome do arquivo sem a última extensão — inclusive para arquivo de
verdade, para que `TCC.docx` e um atalho chamado `TCC` apareçam igual na lista.
Renomear o arquivo renomeia o item, e nada quebra: o `monitores.conf` é indexado
por ambiente, não por item.

**Como a pasta é encontrada, sem interop.** A barra chama `abrir-windows
--listar-coisas` toda vez que o menu abre, e uma chamada de PowerShell custa quase
um segundo — então essa rota não fala com o Windows. Ela procura em
`Desktop/Coisas` e nas duas variantes do OneDrive (`Desktop` e `Área de
Trabalho`), pulando os perfis de sistema. Medido em 31/07/2026:

| | custo |
|---|---|
| glob em `/mnt/c/Users/*/...` | **540 ms** — o drvfs cobra caro por travessia |
| listar a pasta já conhecida | **38 ms** |

Meio segundo para abrir um menu é inaceitável, então o caminho resolvido fica em
`~/.config/linux-fullscreen/pasta-coisas.cache` e o normal passa a ser um único
teste de diretório: o `--listar-coisas` inteiro caiu de **481 ms para 48 ms**. O
cache se invalida sozinho — se a pasta sumiu (outra máquina, outro perfil, o
OneDrive assumiu a área de trabalho), o teste falha e o glob roda de novo. Para
manter a pasta noutro lugar, escreva o caminho em `pasta-coisas.conf`.

O `jogos.conf` antigo (`Nome|linha de comando`) **continua valendo** e é o que
manda quando não há pasta. Com pasta no lugar, ele nem é semeado — apareceriam
jogos que você nunca pôs ali. **O nome desse arquivo não mudou junto com o do
script, de propósito**: é arquivo do usuário, escrito por ele, e renomear dado de
usuário calado é um jeito de fazer trabalho sumir.

### Armadilha: as aspas não sobrevivem à interop (31/07/2026)

Achada ao medir o caminho novo, mas o bug era **antigo** e atingia o `jogos.conf`
desde sempre. O `lancar()` recebia a linha inteira numa string e a expandia sem
aspas, apostando que as aspas escritas no arquivo chegariam ao Windows:

```bash
setsid "$CMD" /c start "" $comando      # <- $comando sem aspas, de propósito
```

Elas não chegam. A interop do WSL monta a linha de comando do `CreateProcess`
**escapando as aspas que encontra dentro de cada argumento**. Verificado com
`cmd.exe /c echo`, que mostra exatamente o que o Windows recebe:

```
bash quebra em  :  ["C:\Riot]  [Games\Riot]  [Client\Riot...exe"]
Windows recebe  :  start "" "\"C:\Riot" Games\Riot "Client\Riot...exe\""
```

Ou seja: **todo caminho com espaço estava quebrado**, inclusive a linha do League
of Legends que o próprio script semeava. O conserto é entregar cada pedaço como
**um argumento do bash** e deixar a interop citar sozinha — aí o Windows recebe

```
start "" "C:\Riot Games\Riot Client\RiotClientServices.exe" --launch-product=...
```

Por isso `lancar()` passou a receber `"$@"` em vez de uma string, e
`comando_do_jogo` devolve a linha **citada para o shell** (`printf %q`), que quem
chama transforma em array com `eval "comando=($linha)"`. As duas fontes — pasta e
`jogos.conf` — saem no mesmo formato.

> Se algum jogo do seu `jogos.conf` "nunca funcionou e você não sabia por quê",
> era isto. Não precisa mexer no arquivo: o conserto está do lado de quem lê.

### Arrumar a Área de Trabalho quebrava tudo — duas vezes (31/07/2026)

Mover os atalhos do desktop para uma subpasta (`Tudo/`) é uma coisa banal de se
fazer. Levou o `.rdp` base junto, e **duas coisas independentes quebraram**:

**1. O `abrir-windows` parava com "não achei o perfil base".** O caminho era
chumbado em `$WINHOME/Desktop/Linux Fullscreen.rdp`. Agora ele procura: ao lado,
e um nível de subpasta abaixo, nas três variantes de área de trabalho (`Desktop`,
OneDrive `Desktop`, OneDrive `Área de Trabalho`). Um nível basta — é assim que se
arruma desktop, e varrer árvore no drvfs é caro. O achado fica em
`perfil-base.cache`; para pôr o `.rdp` em outro lugar qualquer, escreva o caminho
em `perfil-base.conf`.

**2. O `.vbs` caía no plano B em silêncio.** Ele procurava o `.rdp`
`fso.GetParentFolderName(WScript.ScriptFullName) & "\Linux Fullscreen.rdp"` — ao
lado de si mesmo. Sem achar, conectava com `mstsc /v:localhost:3390 /multimon /f`:
a sessão **subia normalmente**, só que sem nenhum dos ajustes de fluidez, e nada
avisava. Esse é o pior tipo de falha deste projeto — a que parece sucesso. Agora
o `.vbs` também olha um nível de subpasta.

**3. E, em 02/08/2026, uma terceira: o `transferir-usb`.** A mesma armadilha,
no mesmo `.rdp`, escondida por dois dias — ela não apareceu em 31/07 porque este
arquivo não foi olhado junto com os outros dois. O sintoma: **trocar o áudio
para o Linux matou o mstsc e não conseguiu reabri-lo**, deixando a sessão
desconectada e o usuário no desktop do Windows, tendo que achar o `.vbs` na mão.

O `perfil_rdp_windows()` montava `%USERPROFILE%\Desktop\Linux Fullscreen.rdp` com
um `printf` e só falhava se o `%USERPROFILE%` viesse vazio — **nunca conferia se
o arquivo existia**. Devolvia um caminho inválido com código de sucesso, e o
`mstsc` abria um diálogo de erro em vez da sessão. Fechar o mstsc é obrigatório
ali (ver "O vaivém do mstsc"), então falhar em reabrir é a falha mais cara das
três: as outras duas degradavam, esta deixa sem tela.

Agora ele procura nos mesmos seis lugares do `abrir-windows`, guarda o achado em
`~/.config/linux-fullscreen/perfil-rdp.cache` (o teste é I/O no drvfs e isto roda
a cada clique no botão de áudio da barra) e, se mesmo assim não achar, cai no
**plano B do `.vbs`** — `mstsc /v:localhost:3390 /multimon /f`, com aviso. Volta
sem os ajustes de fluidez, mas volta; e continua casando a sessão existente,
porque o `Policy=Default` casa por usuário e bpp, e o padrão do mstsc é 32 bits
como o `session bpp:i:32` do perfil.

Conferido em 02/08/2026, com o `.rdp` em `Desktop\Tudo\`: a função devolveu
`C:\Users\Yosef\Desktop\Tudo\Linux Fullscreen.rdp`. A lição que se repete —
**quando um caminho chumbado quebra, procure os outros no mesmo dia**; três
arquivos diferentes tinham a mesma linha, e consertar dois deixou o terceiro
esperando dois dias para morder.

**Teste ao vivo, 02/08/2026.** O ciclo completo, com o `.rdp` na subpasta:
`transferir-usb audio windows` (7 s) e `audio linux` (8 s), cada um matando e
reabrindo o mstsc. Nas duas vezes o cliente voltou sozinho, e a linha de comando
dele — lida com `Get-CimInstance Win32_Process` — mostrou
`mstsc.exe "C:\Users\Yosef\Desktop\Tudo\Linux Fullscreen.rdp"`: o perfil certo,
não o plano B. O headset terminou onde estava (`audio=linux`).

### A varredura: o que mais dependia de caminho chumbado (02/08/2026)

Se a mesma linha estava em três arquivos, valia procurar o resto. O que a
varredura achou, e o que virou:

**`abrir-windows`, a pasta `Jogos`.** Procurava em `Desktop/Jogos` e nas duas
variantes do OneDrive, sem subpasta — a `Jogos` está hoje na raiz da Área de
Trabalho, ao lado da `Tudo/`, e teria quebrado no dia em que fosse arrumada
junto. Agora olha um nível abaixo também. **O preço, medido:** o glob passou de
473 ms para 971 ms, porque dobrou o número de padrões a expandir no drvfs. Ele
só roda quando o cache falha; com cache, listar a pasta conhecida custa 14 ms, e
foi o que o `--listar-coisas` gastou (53 ms com cache, 1,03 s sem).

> Ainda no mesmo dia essa pasta foi renomeada para `Coisas` e o script para
> `abrir-windows` — veja "A lista é uma pasta". O glob dobrou **de novo** pelo
> mesmo motivo, agora por procurar os dois nomes.

**`windows/assinar-rdp.ps1`.** `Join-Path $env:USERPROFILE 'Desktop\Linux
Fullscreen.rdp'`, chumbado. Falha barulhenta ("não achei o perfil"), mas na hora
errada: assinar é justamente o que se faz **depois** de editar o `.rdp`, porque
editar invalida a assinatura. Agora procura ao lado do próprio script
(`$PSScriptRoot` — quem move o `.rdp` para uma pasta costuma levar o `.ps1`
junto), depois na Área de Trabalho e um nível de subpasta, nas três variantes; e
aceita `-Perfil <caminho>` como escape. Conferido: devolveu
`C:\Users\Yosef\Desktop\Tudo\Linux Fullscreen.rdp`.

**Três caminhos chumbados do lado Linux, e um deles era falha silenciosa.** O
`transferir-usb` e o `audio-dispositivos` liam o `audio-padrao.ps1` de
`$HOME/linux-fullscreen/windows/`, e o `startwm.sh` chamava o `xfwm-atalhos.sh`
de `$HOME/linux-fullscreen/`. Renomear ou mover a pasta do repositório —
exatamente o que a invariante do `perfil.sh` já previa — derrubaria a troca de
áudio do lado Windows (com um aviso vago) e, pior, **os atalhos de janela no
login sem aviso nenhum**: a sessão sobe igual, só o `Super+seta` é que morre.
Os dois arquivos passaram a ser instalados em `/usr/local/share/linux-fullscreen/`
como o `perfil.sh`, e os três consumidores procuram lá primeiro, mantendo o
repositório como plano B para quem quiser rodar sem instalar.

**`/home/yosef/prints` nos atalhos de print.** Chumbado no `xfwm-atalhos.sh` —
errado para qualquer outro usuário, e o flameshot não avisa: só não salva. Agora
sai de `$HOME`, expandido **na hora de gravar** e não no valor do atalho: quem
executa esses comandos é o `xfsettingsd`, que não passa a string por shell
nenhum, então um `$HOME` literal chegaria ao flameshot como o nome de uma pasta
chamada `$HOME`. A pasta também passou a ser criada (`mkdir -p`).

**O que ficou como estava, de propósito:** o `Documentation=file:/home/yosef/...`
dos dois `.service` (é texto de documentação, não caminho executado); o
`trabalho`, que já tem o escape `TRABALHO_DIR`; e tudo que pergunta o caminho ao
Windows em vez de chumbá-lo (`%LOCALAPPDATA%`, `%USERPROFILE%` via `cmd.exe`),
que é o padrão certo e já estava sendo seguido nesses lugares.

### Assinar o `.rdp` o converte para UTF-16 — e isso quebrava o perfil do jogo

Achado ao investigar o erro acima, e é **independente dele**. O
`preparar_perfis` monta o perfil do jogo filtrando o perfil base:

```bash
grep -viE '^(use multimon|selectedmonitors|winposstr|signscope|signature):' \
    "$BASE_U" > "$dir_u/jogo.rdp"
```

Um `.rdp` escrito à mão é ASCII e isso funciona. **Depois de assinado pelo
`assinar-rdp.ps1`, ele volta em UTF-16LE com BOM** — e aí o `grep` responde
`binary file matches` na saída de **erro** e nada na saída padrão:

```
$ ls -l jogo.rdp
-rwxrwxrwx 53 jogo.rdp        <- 53 bytes!

$ cat jogo.rdp
use multimon:i:0
winposstr:s:0,1,2944,216,3712,648
```

Só as duas linhas que o `printf` acrescenta depois. **Sem endereço, sem
credencial, sem `session bpp:i:32`, sem nada.** E falhava calado: o arquivo
existia, o script seguia feliz, e quem reclamava era o mstsc muito depois.

O conserto é um `ler_rdp()` que detecta o BOM e converte antes de filtrar:

```bash
ler_rdp() {
    case "$(head -c2 "$1" | od -An -tx1 | tr -d ' \n')" in
        fffe|feff) iconv -f UTF-16 -t UTF-8 "$1" ;;
        *)         cat "$1" ;;
    esac
}
```

O `-f UTF-16` **sem** o `LE`/`BE` é de propósito: ele usa o BOM para descobrir a
ordem dos bytes *e o remove da saída*. Com `-f UTF-16LE` o BOM sobreviveria como
um U+FEFF invisível na primeira linha — outro jeito de quebrar calado.

Medido depois do conserto, o mesmo comando que gerava 53 bytes:

```
5184 bytes, UTF-16 (o rdpsign reescreve), e agora COM assinatura:
  full address:s:localhost:3390
  session bpp:i:32
  use multimon:i:0
  winposstr:s:0,1,2944,216,3712,648
```

O `aviso: nao consegui assinar o perfil do jogo` também sumiu — o `rdpsign`
falhava porque o arquivo que recebia não era um `.rdp`.

> Se você assinou o `.rdp` em algum momento, **todo jogo lançado depois disso
> usou um perfil vazio**. A data da assinatura está no seu
> `%LOCALAPPDATA%\linux-fullscreen\thumbprint.txt`.

### Com três monitores: o Linux fica em um só (por ora)

Se você cede um monitor ao jogo e sobram dois, o natural seria o Linux ocupar os
dois. **Ainda não faz isso**, e o motivo é o `winposstr`: ele posiciona uma
janela, logo entrega **um** monitor. Dois exigiriam voltar ao
`selectedmonitors:s:<id>,<id>` e, com ele, ao problema dos IDs opacos.

Há uma pista que pode tornar isso barato, mas está com **uma medição só** e por
isso não virou código: o `selectedmonitors:s:1` trouxe o monitor que o `xrandr`
chama de `rdp1`. Se **ID do mstsc = índice da saída do xrdp** se confirmar, o
mapa inteiro sai de um `xrandr --listmonitors` na sessão normal, sem reconexão
nenhuma. Falta testar a outra ponta (o ID 0 tem que trazer o `rdp0`).

E há um problema que nenhum ID resolve: **jogo no monitor do meio deixa o Linux
com um vão**. Não se sabe se o mstsc aceita seleção não contígua, e se aceitar,
o xrdp provavelmente aloca o retângulo envolvente — o vão viraria uma **zona
morta dentro da sessão X**, onde uma janela se perde sem estar em tela nenhuma
(o `Alt+Tab` ainda a traria de volta, por causa do `cycle_hidden`).

O desvio que dispensa tudo isso: **ponha o jogo num monitor da ponta**. Aí os
monitores do Linux ficam adjacentes e não há vão. A restrição real não é "qual
monitor", é **não partir a sessão ao meio**.

> **E por que não deixar o Linux nos três, com o jogo por cima?** Porque em
> multimonitor o mstsc é **uma janela só** cobrindo todos os monitores. No
> instante em que você clica na sessão Linux, ela sobe no z-order e engole o
> jogo junto. É exatamente por isso que dividir os monitores precisa existir.

### As duas telas que aparecem a cada troca — e como calar as duas

Cada partida reconecta o mstsc duas vezes, e sem preparo cada reconexão pede
duas coisas. **São problemas diferentes, de lados diferentes**, e confundi-los
leva a conselho errado.

> **Resolvido e verificado em 29/07/2026.** A troca de perfil hoje é silenciosa:
> nenhuma senha, nenhum aviso. A receita completa são três peças, e as três são
> necessárias:
>
> 1. `cmdkey /generic:TERMSRV/localhost` — mata a tela de login do xrdp;
> 2. `assinar-rdp.ps1` — assina o `.rdp` (e o `abrir-windows` re-assina o perfil
>    que gera);
> 3. a **política `TrustedCertThumbprints`**, que só o mesmo script rodando
>    **como administrador** grava — é ela que cala o aviso de distribuidor.
>
> Confirmado no registro depois de funcionar: a política está aplicada e o
> contador de `LocalDevices` **não subiu**, prova de que o aviso é suprimido
> antes de existir, em vez de ser consentido a cada vez.

**1. A tela de login com o logo do xrdp** (`Login to <máquina>`, com o campo
`Session: Xorg`) **não é do Windows.** Ela é desenhada *dentro* da sessão
remota, pelo próprio xrdp. O Windows só está exibindo pixels — nunca vê aquele
formulário, e por isso **não existe "lembrar-me" para ela**. Procurar essa opção
no mstsc é procurar o que não pode existir.

Ela aparece porque o `.rdp` não manda credencial nenhuma, então o xrdp pergunta.
A correção é fazer o mstsc enviar usuário e senha na conexão; aí o xrdp
autentica sozinho e nunca desenha a tela:

```bash
cd /tmp && /mnt/c/Windows/System32/cmdkey.exe /generic:TERMSRV/localhost /user:$USER /pass
```

Omitir o valor depois de `/pass` faz o `cmdkey` **perguntar** a senha (a do
Linux, a mesma daquela tela), sem deixá-la na linha de comando nem no histórico.
O `username:s:` no `.rdp` completa o par. Conferir com
`cmdkey /list | grep -i termsrv`.

> **Correção (29/07/2026).** Este README mandava marcar "Lembrar-me" quando o
> mstsc perguntasse a senha. **O mstsc nunca pergunta** neste setup — sem NLA,
> ele conecta calado e quem pergunta é o xrdp, lá dentro. O conselho não tinha
> como funcionar.

**2. O aviso de "fornecedor desconhecido"** é do Windows, e sai porque o `.rdp`
**não tem assinatura digital** enquanto pede acesso a recursos locais (área de
transferência, WebAuthn). Sem publicador confiável o mstsc nem oferece a caixa
"não perguntar novamente". A correção é assinar:

```powershell
powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Desktop\assinar-rdp.ps1"
```

O `windows/assinar-rdp.ps1` cria um certificado autoassinado, instala nos
armazenamentos `Root` e `TrustedPublisher` **do seu usuário** (não da máquina —
menos invasivo e dispensa UAC; há `-Maquina` se não bastar) e assina o perfil.
Ele guarda o thumbprint em `%LOCALAPPDATA%\linux-fullscreen\thumbprint.txt`,
porque o `abrir-windows` **re-assina o perfil que gera** a cada partida — as
coordenadas do monitor mudam, e qualquer mudança invalida a assinatura.

**Assinar não cala o aviso — troca ele por um que pode ser calado.** O vermelho
("Cuidado: conexão remota desconhecida", sem opção nenhuma) vira o amarelo
("Verificar o distribuidor", com o nome do certificado) e, o que importa,
com a caixa **"Lembrar minhas opções de conexões remotas deste editor"**.

Marcar essa caixa **funciona, e cola** — mas por *identidade de conexão*, não por
publicador. A memória vive em `HKCU\Software\Microsoft\Terminal Server
Client\LocalDevices`, com nome de GUID por identidade. Como neste setup existem
**dois** perfis (o da Área de Trabalho e o gerado pelo `abrir-windows`), são
**duas** marcações — uma em cada aviso, na ida e na volta. Depois disso silencia.

Medido em 29/07/2026, e é o que garante que colar: **o `rdpsign` é
determinístico.** Assinar duas vezes o mesmo conteúdo dá arquivos byte a byte
idênticos, então o perfil do jogo mantém a mesma identidade entre partidas. O
que gera identidade nova é **mudar de monitor** (as coordenadas entram no
arquivo) — ou seja, uma marcação por escolha de monitor, e nada mais.

**O caminho que ficou valendo aqui é o outro**, porque a marcação cobra uma por
escolha de monitor e isso incomoda ao trocar de ambiente: rodar o
`assinar-rdp.ps1` **como administrador**, para ele também gravar a política de
publicador confiável:

```
HKCU\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services
    TrustedCertThumbprints = <thumbprint SHA1 do certificado>
    AllowSignedFiles       = 1
```

Com o thumbprint nessa lista o mstsc não pergunta mais nada, para qualquer
perfil assinado por ele. **Exige elevação mesmo sendo `HKCU`**: a ACL de
`HKCU\Software\Policies` nega escrita ao token comum do usuário. E não se
consegue de dentro da sessão — o token do interop da WSL vem filtrado, sem o SID
`S-1-5-32-544` nem como deny-only, mesmo com o usuário no grupo de
administradores. Sem elevação o script avisa e segue; a assinatura continua
valendo.

Para desfazer a política:

```powershell
Remove-ItemProperty "HKCU:\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services" TrustedCertThumbprints
```

> **Este é o único passo que não roda de dentro da sessão.** Chamado pelo
> interop da WSL, o `New-SelfSignedCertificate` falha com
> `NTE_PROV_TYPE_NOT_DEF`: o processo nasce sem o hive de criptografia do
> perfil (medido em 29/07/2026 — o COM instancia e o RSA do .NET funciona, mas
> `Test-Path HKCU:\Software\Microsoft\Cryptography` responde `False`). Não é
> defeito da máquina. `Ctrl+Alt+Break`, abra o PowerShell do Windows e rode lá,
> uma vez só.

### Por que a sessão sobrevive à troca — e o que a quebraria

Trocar de perfil é matar o mstsc e reabrir. Isso só é seguro por causa de uma
linha do `/etc/xrdp/sesman.ini`:

```ini
[Sessions]
Policy=Default
```

A política `Default` casa a sessão existente por **usuário e bpp**. A checagem
de dimensões (`Dimensions don't match for 'D' policy`, no binário do sesman) só
roda nas políticas que incluem `D`. **Se alguém puser `Policy=UBD`, reconectar
com um monitor a menos abre um desktop novo em branco** e todo o seu trabalho
fica preso numa sessão órfã. É o pré-requisito silencioso deste script.

O outro pré-requisito é o **xrdp 0.10**: o redimensionamento da sessão viva
(`process_display_control_monitor_layout_data`) não existe no 0.9.24. Voltar
para o xrdp da distribuição, como descreve "O caminho de volta", desliga este
recurso junto.

#### Mas o `Policy=Default` não protege contra o *sesman* reiniciar (02/08/2026)

O que está escrito acima continua valendo, e é fácil ler mais do que ele diz. O
`Policy=Default` preserva a sessão contra **o mstsc fechar**. Ele não preserva
nada contra o **sesman reiniciar** — e é exatamente isso que o `install.sh` faz.

A tabela de sessões do sesman vive na memória do processo. Reiniciar o serviço a
apaga; os processos da sessão antiga, que não são filhos dele, continuam de pé.
No reconnect o sesman até enxerga o X que sobrou, e mesmo assim abre outro:

```
[13:05:11] sesman_main_loop: sesman asked to terminate
[13:05:12] starting xrdp-sesman with pid 225079
[13:05:29] Found X server running at /tmp/.X11-unix/X10
[13:05:30] Starting X server on display 11
```

Ele não está errado: sem a tabela, aquele `X10` é um servidor X qualquer, não uma
sessão dele. O resultado é uma sessão **órfã** — viva, sem `sesexec` que a
recolha (o único vivo era o do `:11`) e sem ninguém conectado. Medido no dia: a
sessão de 01:49 ficou de pé até as 13:32 segurando **1,66 GB** (`used` caiu de
3316 MB para 1655 MB ao encerrá-la). Numa VM de 7,9 GB com `available` em 4,6 GB,
isso é a diferença entre caber e não caber.

Nada avisa. A sessão nova sobe bonita, com o código recém-instalado, e a antiga
só aparece se você for procurar — `ps -eo pid,lstart,cmd | grep Xorg` mostrando
dois displays é o sintoma.

**Como limpar, sem reiniciar nada:** derrube o `xfwm4` e o `Xorg` do display
antigo e pare por aí. Os clientes X morrem sozinhos ao perder a conexão — foi o
caso de xfsettingsd, barra, bancada, Brave, três terminais e o xclip, nenhum
precisou de sinal. Só o `xrdp-chansrv` sobrou, porque não é cliente X nesse
sentido:

```bash
ps -eo pid,lstart,cmd | grep -E 'Xorg :|xfwm4'   # achar o display velho
kill -TERM <pid-do-xfwm4> <pid-do-Xorg>          # o resto cai junto
kill -TERM <pid-do-xrdp-chansrv>                 # este precisa de empurrão
```

**O que sobrevive a isso** — e a razão de dar para limpar com um trabalho pesado
rodando: processo já desligado da sessão não sente. Um `gmx mdrun` lançado por
`conda run` estava com `PPID 1`, sessão própria (`SID` igual ao próprio PID) e
`tty ?`; o watcher dele tinha sido `nohup` + `disown`. Os dois atravessaram a
limpeza sem piscar — o `mdrun` seguiu a 407% de CPU e avançou 410 mil passos
durante ela. Confira o trio `PPID`/`SID`/`tty` antes, com
`ps -o pid,ppid,pgid,sid,tty,cmd -p <pid>`: é ele que diz se algo depende da
sessão gráfica ou não. `DISPLAY` no ambiente **não** quer dizer dependência —
o `mdrun` tinha `DISPLAY=:10` herdado e nunca abriu o display.

### Três coisas que enganam

- **Os números não batem com as Configurações do Windows.** O `--listar` mostra
  o monitor de 1920x1080 como **1536x864**: são coordenadas *lógicas*, já
  divididas pela escala de 125%. Está certo, e é obrigatório — o `winposstr`
  também fala nesse espaço. Não "corrija" para a resolução física.
- **Escolher o monitor não obriga o jogo a obedecer.** O script escolhe onde a
  *sessão Linux* fica; o jogo abre onde o Windows mandar, que costuma ser o
  **primário**. Por isso a lista marca qual é o primário. Se um jogo teimar em
  abrir no lugar errado, o ajuste é do lado Windows (o primário nas
  Configurações, ou a opção de monitor do próprio jogo) — e o script se adapta
  sozinho na próxima vez.

Um efeito colateral sem conserto barato: ao encolher, o xfwm4 empilha as janelas
no monitor que sobrou, e elas **não voltam sozinhas** ao lugar quando a sessão
retoma o multimonitor.

## Arquivos instalados

| Origem | Destino |
|---|---|
| `startwm.sh` | `/etc/xrdp/startwm.sh` |
| `fix-x11-unix` | `/usr/local/bin/fix-x11-unix` |
| `linux-desktop-up` | `/usr/local/bin/linux-desktop-up` |
| `linux-desktop-down` | `/usr/local/bin/linux-desktop-down` |
| `abrir-windows` | `/usr/local/bin/abrir-windows` |
| `transferir-usb` | `/usr/local/bin/transferir-usb` |
| `audio-dispositivos` | `/usr/local/bin/audio-dispositivos` |
| `camera-rede` | `/usr/local/bin/camera-rede` |
| `barra-apps` | `/usr/local/bin/barra-apps` |
| `barra-tarefas.c` | **compilado** para `/usr/local/bin/barra-tarefas` |
| `bancada.c` | **compilado** para `/usr/local/bin/bancada` |
| `terminal.c` | **compilado** para `/usr/local/bin/terminal` |
| `perfil.sh` | `/usr/local/share/linux-fullscreen/perfil.sh` (+ uma linha no `.bashrc`) |
| `windows/audio-padrao.ps1` | `/usr/local/share/linux-fullscreen/audio-padrao.ps1` |
| `xfwm-atalhos.sh` | `/usr/local/share/linux-fullscreen/xfwm-atalhos.sh` |
| `desktop/*.desktop` | `/usr/share/applications/` (itens do appfinder) |
| `x11-unix-writable.service` | `/etc/systemd/system/` |
| `v4l2loopback.service` | `/etc/systemd/system/` |
| `i3.config` | `~/.config/i3/config` (só serve se voltar ao i3) |

São **três itens compilados**, todos no passo 2 e todos C com Xlib cru:

```bash
gcc -O2 -Wall -o barra-tarefas barra-tarefas.c -lX11 -lXrandr
gcc -O2 -Wall -o bancada  bancada.c  $(pkg-config --cflags xft) -lX11 -lXft
gcc -O2 -Wall -o terminal terminal.c $(pkg-config --cflags xft) -lX11 -lXft -lfontconfig
```

Daí `gcc`, `libx11-dev`, `libxrandr-dev`, `libxft-dev` e `xfonts-base` na lista
de pacotes — o `-lfontconfig` do terminal não pede pacote novo, porque o
`libxft-dev` já depende do `libfontconfig1-dev`.

O que se versiona é o `.c`: binário é derivado e vai para o `.gitignore`.
**`barra-tarefas` e `terminal` estão lá; o `bancada` não** — ele foi commitado
junto com o `bancada.c` e continua rastreado, o que faz cada recompilação
aparecer como alteração no `git status`. Acertar isso é um
`git rm --cached bancada` mais a linha no `.gitignore`.

Nenhum dos três é necessário para a sessão subir, então cada um falha sozinho e
com aviso em vez de derrubar a instalação. Só a barra entra na sessão pelo
`startwm.sh`; a bancada e o terminal são por demanda. O `terminal` é instalado
por ser um programa que se abre, **não** porque algo passe a depender dele: a
bancada continua usando o xterm na aba do Claude.

Do lado Windows, `Linux Fullscreen.vbs` e `Linux Fullscreen.rdp` (de `windows/`)
vão **juntos** para a Área de Trabalho: o `.vbs` é o que se clica, e o `.rdp` ao
lado é o perfil de conexão que ele usa (fullscreen multimonitor + os ajustes de
fluidez). Sem o `.rdp` o `.vbs` ainda conecta, mas cai nas opções padrão do
mstsc.

O `windows/audio-padrao.ps1` **não** vai para a Área de Trabalho: o
`transferir-usb` o copia sozinho para `%LOCALAPPDATA%\linux-fullscreen\` sempre
que a cópia do repositório for mais nova. Editar aqui basta.

O terceiro arquivo de `windows/`, o `assinar-rdp.ps1`, também vai para lá, mas é
de execução única: **um PowerShell do Windows como administrador**, uma vez por
máquina (veja "As duas telas que aparecem a cada troca"). Não roda de dentro da
sessão. Ele refaz tudo o que precisa do lado Windows — certificado, confiança e
política —, então nada disso precisa ser salvo antes de formatar.

Quatro scripts **não** são instalados — rodam quando você quiser:

| Script | Para quê |
|---|---|
| `xfwm-atalhos.sh` | põe os `Super+setas`, conserta o `Ctrl+Alt+T` e aplica os ajustes de fluidez. Rodar uma vez após instalar — mas o `startwm.sh` já o reaplica a cada login. |
| `instalar-som.sh` | compila o `pulseaudio-module-xrdp` (veja "Som"). Precisa de `sudo` e leva alguns minutos. |
| `instalar-xrdp010.sh` | troca o xrdp da distribuição pelo **0.10 compilado**, com GFX/H.264 (veja "Fluidez"). O passo mais arriscado — leia "O caminho de volta" antes. |
| `diag-super-direita.sh` | descobre se um atalho com a tecla Windows está sendo engolido pelo Windows antes de chegar na sessão. |

> **O `install.sh` sozinho instala o xrdp 0.9.24 do Ubuntu**, não o 0.10. Numa
> máquina nova, a ordem é `install.sh` → `instalar-som.sh` → `instalar-xrdp010.sh`.
> O último chama o `install.sh` de novo por dentro, porque o `make install` do
> xrdp sobrescreve o `xrdp.ini` e o `startwm.sh`.

Há ainda uma alteração do lado Windows, feita pelo passo 7 do `install.sh`:
`guiApplications=false` em `C:\Users\<você>\.wslconfig` (veja acima). É o único
arquivo que o instalador escreve fora da WSL, e o motivo do `wsl --shutdown`
logo depois. Se o interop com o Windows estiver desligado, ele avisa e segue —
aí é à mão.

xrdp roda na porta **3390** (3389 é do RDP nativo do Windows).
Os originais viram `.orig` ao lado (`/etc/xrdp/xrdp.ini.orig`, etc.).

## O caminho de volta: quando a sessão RDP não sobe

**Leia isto antes de mexer no xrdp, no `startwm.sh` ou no Xorg.**

A sessão gráfica não é a única porta de entrada desta máquina, e isso é o que
torna seguro experimentar nela. A WSL responde por dois caminhos que **não
passam pelo xrdp nem pelo Xorg**:

```powershell
wsl -d Ubuntu-24.04            # terminal comum, direto do Windows
```

E o **VS Code do Windows** com a extensão WSL, que sobe o `vscode-server`
dentro da distro pelo interop — também sem tocar em xrdp, Xorg ou sessão
gráfica. Vale a pena deixar um preparado antes de qualquer experimento: com
ele você continua editando, compilando e rodando comandos mesmo com a sessão
fullscreen completamente quebrada.

Ou seja: **nada que você faça no xrdp tranca você fora da máquina.** O pior
caso é ficar sem a sessão gráfica até desfazer — e desfazer se faz de fora.

### Desfazer, do mais brando ao mais radical

```bash
# 1. Configuração quebrada (startwm.sh, xrdp.ini, atalhos):
sudo bash ~/linux-fullscreen/install.sh

# 2. Binários do xrdp quebrados (uma compilação da fonte que deu errado):
sudo apt install --reinstall xrdp xorgxrdp
sudo bash ~/linux-fullscreen/install.sh

# 3. Voltar o repositório a um estado que funcionava:
cd ~/linux-fullscreen && git log --oneline && git checkout <commit>
```

Depois de qualquer um dos três, reconecte pelo `.vbs`.

Se nem isso resolver, os originais intocados do sistema estão em
`/etc/xrdp/*.orig`, e a distro inteira pode ser exportada antes de um
experimento grande:

```powershell
wsl --export Ubuntu-24.04 D:\ubuntu-antes.tar
```

### Onde olhar para saber o que quebrou

```bash
cat ~/startwm-debug.log                    # a sessao inteira; e o mais util
sudo journalctl -u xrdp -u xrdp-sesman -n 40 --no-pager
cat ~/.xorgxrdp.10.log
systemctl is-active xrdp xrdp-sesman
```

O `startwm-debug.log` existe exatamente por isso: o `xrdp-sesman.log` só
registra o código de saída do gerenciador de janelas, nunca a mensagem de erro.
Veja "Log de depuração da sessão".

### Desfazer o xrdp 0.10 compilado (29/07/2026)

Esta máquina teve o `xrdp` e o `xorgxrdp` da distribuição **substituídos por uma
compilação da fonte** (0.10.6.1 e 0.10.5), para ter o GFX/H.264 — veja
"Fluidez". Se a sessão parar de subir por causa disso, o caminho de volta é:

```bash
sudo apt install --reinstall xrdp xorgxrdp
sudo bash ~/linux-fullscreen/install.sh
```

Isso devolve os binários 0.9.24/0.9.19 do Ubuntu e reaplica a configuração
(porta 3390, `max_bpp=24`, nosso `startwm.sh`). Há também uma cópia intacta de
tudo que foi substituído, feita antes da instalação:

```bash
sudo ls /root/xrdp-backup-*        # etc-xrdp/, binarios e modulos do Xorg
```

O que muda com o 0.10, para saber o que conferir depois:

| Arquivo | O que aconteceu |
|---|---|
| `/etc/xrdp/xrdp.ini` | sobrescrito pelo padrão do 0.10; porta e `max_bpp` reaplicados à mão |
| `/etc/xrdp/startwm.sh` | sobrescrito; reinstalado a partir deste repositório |
| `/etc/xrdp/gfx.toml` | **novo** — configura o GFX (`order = ["H.264", "RFX"]`, x264 `ultrafast`/`zerolatency`) |
| `/lib/systemd/system/xrdp*.service` | units novas; exigem `systemctl daemon-reload` |

> **Risco específico de trocar o xrdp por uma versão compilada.** Substituir o
> `xrdp` e o `xorgxrdp` da distribuição por uma compilação da fonte (para ter o
> GFX do 0.10 — veja "Fluidez") mexe nas duas peças que sustentam a sessão, e
> as versões dos dois precisam casar. É o experimento mais arriscado deste
> projeto. Faça só com o repositório empurrado para o GitHub e com um terminal
> WSL ou VS Code de fora já aberto e testado — não abra os dois pela primeira
> vez *depois* de quebrar.

## Problemas conhecidos

**Rodei o `install.sh` de dentro da sessão RDP e ela fechou na hora.**

Não é falha, é esperado. O passo `[6/7]` do instalador faz

```bash
systemctl restart xrdp-sesman
systemctl restart xrdp
```

e, se você está rodando o script *de dentro* da própria sessão RDP, isso
reinicia o serviço que está servindo essa conexão — a queda é imediata. A VM
continua de pé (o `wsl.exe`/Gerenciador de Tarefas mostra ela rodando
normalmente) e o xrdp volta no ar em segundos; é só reconectar pelo atalho do
Windows de novo. Confirmado em 28/07/2026: `systemctl is-active xrdp
xrdp-sesman` voltava `active` logo depois da queda.

**Editei o `startwm.sh` e nada mudou ao reconectar.**

Duas armadilhas se somam aqui.

A primeira: `sudo ./startwm.sh` no terminal **não testa nada**. O script é
chamado pelo xrdp *depois* que o Xorg já subiu e exportou `DISPLAY=:10`; solto
no shell ele erra com `Cannot open display` mesmo numa instalação perfeita.
Não é sintoma de problema nenhum.

A segunda, e a que realmente engana: **fechar a janela do mstsc não encerra a
sessão**. O `xrdp-sesman` a mantém viva e devolve a mesma ao reconectar — mesmo
Xorg, mesmo xfwm4, mesmo ambiente. E o `startwm.sh` só é lido quando uma sessão
*nova* nasce. Para valer, a antiga tem que morrer.

O jeito curto é `Alt+F3` → **"Sair da sessão"** (ou `linux-desktop-down -s -y`),
que já faz os dois `pkill` na ordem certa. À mão, é isto:

```bash
pkill -x xfsettingsd     # ver nota abaixo
pkill -x xfwm4
```

Sem `sudo` — o gerenciador de janelas roda como você. Como o `startwm.sh`
termina em `exec xfwm4` dentro do `dbus-launch --exit-with-session`, matar o
xfwm4 derruba o `sh`, que derruba o `dbus-launch`, que encerra a sessão inteira.
Só então reconecte. (No i3 o equivalente era `i3-msg exit`.)

> **Não existe `xfwm4 --quit`.** Versões anteriores deste README mandavam rodar
> `xfwm4 --quit`; o xfwm4 4.18 responde `Unknown option --quit`. As únicas
> opções que ele aceita são `--replace`, `--compositor`, `--vblank`,
> `--version` e `--display`. E **não use `--replace` para isso**: ele substitui
> o gerenciador por um novo em vez de encerrar, e o processo que morre é o do
> `exec` da sessão — você derruba a sessão de um jeito mais confuso e ainda
> deixa um xfwm4 avulso. `pkill -x xfwm4` é o caminho direto.

O `pkill -x xfsettingsd` antes só é necessário se você tiver reiniciado o
xfsettingsd à mão durante a sessão (ver o problema do grab órfão adiante).
Nesse caso ele foi desacoplado com `setsid` e pode sobreviver ao fim da sessão;
se sobreviver, a sessão seguinte sobe um segundo, os dois disputam os mesmos
grabs de teclado e o bug do `Super+→` reaparece.

Ao reconectar, confira que há **exatamente um** — e cuidado, `pgrep -c` conta
processo zumbi, que não vale:

```bash
pgrep -ax xfsettingsd | grep -v defunct
```

Confirme que pegou comparando o horário do processo com o da sua edição:

```bash
ps -eo pid,lstart,cmd | grep xfwm4
```

**Snap não abre: `Error: cannot open display: :10.0`.**

Sintoma idêntico ao da armadilha do Wayland abaixo, causa diferente — e engana
mais ainda, porque `DISPLAY`, o socket `/tmp/.X11-unix/X10` e o cookie estão
todos corretos. Acontecia com o Firefox enquanto o VS Code abria normalmente.

Duas causas somadas:

1. **`XAUTHORITY` não é exportado pelo xrdp.** Para um app comum tanto faz: sem
   a variável, a libX11 usa o padrão `$HOME/.Xauthority`. Mas o snapd reescreve
   o `HOME` dos snaps **estritos** para `~/snap/<app>/common`, e o padrão passa
   a apontar para um arquivo inexistente. Snaps `classic` (`code`, `nvim`,
   JetBrains) não têm o `HOME` mexido — por isso funcionavam.

2. **O lançador do snap sobrescreve o `GDK_BACKEND`.** Todo snap desktop passa
   pelo `command-chain` do `gnome-platform`, que faz:

   ```bash
   if [[ -n "$XDG_RUNTIME_DIR" && -z "$DISABLE_WAYLAND" ]]; then
       [ -S "$XDG_RUNTIME_DIR/../wayland-0" ] && wayland_available=true
   ...
   [ "$wayland_available" = true ] && export GDK_BACKEND="wayland"
   ```

   Ele testa **o socket, não a variável `WAYLAND_DISPLAY`** — então apagar a
   variável não adianta. Com o WSLg ligado o socket estava lá, o launcher
   trocava nosso `GDK_BACKEND=x11` por `wayland`, e o `MOZ_ENABLE_WAYLAND=0`
   mandava o Firefox pedir uma tela X11 que o GDK, restrito ao backend Wayland,
   recusava a abrir.

Ambas já corrigidas no `startwm.sh` (`XAUTHORITY` e `DISABLE_WAYLAND=1`), e a
segunda deixou de existir com o WSLg desligado. `DISABLE_WAYLAND` é a válvula
de escape prevista pelo próprio script do snap.

Como confirmar que é isto, e não o socket ou o cookie — o binário cru, sem o
wrapper, ignora o launcher e abre:

```bash
/snap/firefox/current/usr/lib/firefox/firefox
```

**Snap não abre: `cannot preserve mount namespace ... Invalid argument`.**

Sintoma (visto em 28/07/2026, com o Firefox):

```
update.go:193: cannot change mount namespace according to change mount
  (/var/lib/snapd/hostfs/usr/share/gimp/2.0/help ...): cannot write to
  "/var/lib/snapd/hostfs/..." because it would affect the host in "/var/lib/snapd"
  [... mais quatro linhas parecidas ...]
cannot preserve mount namespace of process 159695 as firefox.mnt: Invalid argument
unexpected eof from helper process
```

**Não tem nada a ver com o RDP.** A sessão RDP e um shell comum estão no mesmo
mount namespace (compare `readlink /proc/$(pgrep -x xfwm4)/ns/mnt` com o do seu
shell), e nenhuma linha fala de display — o Firefox nem chega a tentar abrir
janela. É o snapd, e é **intermitente**.

As linhas `update.go:193` são **ruído**, não a falha. O perfil que o snapd gera
pede bind-mounts de diretórios de documentação do host que nesta máquina não
existem (gimp, libreoffice, xubuntu-docs…). Sem a origem, o `snap-update-ns`
teria que criá-la dentro de `/var/lib/snapd/hostfs`, o que mexeria no host — e
ele recusa, corretamente. Pula essas entradas e segue. Numa execução
bem-sucedida elas nem aparecem.

A falha real são as duas últimas linhas. O `snap-confine` monta o namespace do
snap e depois o *preserva*, com um bind-mount de `/proc/<pid>/ns/mnt` em
`/run/snapd/ns/<app>.mnt` — é isso que faz a segunda abertura ser rápida. Esse
bind falhou, o processo auxiliar morreu, e o app não subiu.

Como reconhecer que é isto: o `.mnt` fica como **arquivo vazio de 0 byte**, em
vez do mount `nsfs` que deveria ser.

```bash
ls -la /run/snapd/ns/firefox.mnt          # 0 byte e dono errado = quebrado
grep firefox.mnt /proc/self/mountinfo     # saudavel mostra uma linha "nsfs"
```

**Correção** — apague o arquivo-fantasma e abra de novo:

```bash
sudo umount /run/snapd/ns/firefox.mnt 2>/dev/null
sudo rm -f /run/snapd/ns/firefox.mnt
firefox
```

Ou `sudo systemctl restart snapd`. Aconteceu na primeira abertura de snap
depois que a VM subiu — mesmo padrão da seção sobre a VM desligando sozinha
("a primeira tentativa logo após ligar o PC costumava falhar").

Um agravante estrutural que vale conhecer: **`/` nesta WSL está com propagação
privada**. Num Ubuntu normal o systemd faz `mount --make-rshared /` no arranque;
aqui só `/run`, `/dev`, `/sys` e `/proc` aparecem como `shared:`.

```bash
awk '$5=="/"{print}' /proc/self/mountinfo   # sem "shared:" = privado
```

É o tipo de coisa que deixa a preservação de namespace do `snap-confine`
frágil e intermitente.

> **O teste do binário cru não serve aqui.** O
> `/snap/firefox/current/usr/lib/firefox/firefox` da seção anterior pula o
> `snap-confine` inteiro, então abre normalmente mesmo com o namespace
> quebrado. Ele diagnostica o problema de *display*, não este.

**Cliquei no `.rdp` e não conectou.**

Esperado. O `.rdp` sozinho só sabe conectar — quem acorda a WSL e garante o
xrdp no ar é o `linux-desktop-up`, chamado pelo `.vbs`. Se a VM estiver
dormindo, não há o que conectar.

**Clique sempre no `.vbs`**; ele chama o `.rdp` sozinho. O ícone novo do RDP na
Área de Trabalho é só o perfil de configuração, não um atalho de entrada.

**A sessão abre e fecha na hora.** É o `/tmp/.X11-unix`: o Xorg precisa criar o
socket `X10` ali dentro e não consegue. Confira:

```bash
ls -ld /tmp/.X11-unix                  # tem que existir e ser 1777
grep " /tmp/.X11-unix " /proc/mounts   # se aparecer, não pode conter "ro,"
sudo /usr/local/bin/fix-x11-unix
cat ~/.xorgxrdp.10.log
```

Com o WSLg desligado esse `grep` **não retorna nada** — e está certo: sem WSLg
ninguém monta nada ali, e `/tmp/.X11-unix` é um diretório comum criado pelo
`fix-x11-unix`. O caso clássico do mount read-only era coisa do WSLg; o que
sobra hoje é o diretório simplesmente não existir, e é por isso que o
`fix-x11-unix` começa com um `mkdir -p`.

**Abro algo e a janela aparece solta no Windows, não na sessão.**

> Com `guiApplications=false` isto não acontece mais — não existe compositor
> para onde fugir. Fica registrado porque volta a valer se você reativar o
> WSLg, e porque explica por que o `startwm.sh` tem tantos `export`.

Esta era *a* armadilha desse setup, e o sintoma engana: o processo aparece com
`DISPLAY=:10` certinho, e mesmo assim a janela vai pro Windows.

Causa: apagar `WAYLAND_DISPLAY` não é suficiente. Quando essa variável não
existe, a libwayland assume o socket chamado `wayland-0` dentro do
`XDG_RUNTIME_DIR` — e:

```
/run/user/1000/wayland-0  ->  /mnt/wslg/runtime-dir/wayland-0
```

...que é o compositor do WSLg. Então todo app GTK/Qt tenta Wayland primeiro,
encontra o WSLg e renderiza no desktop do Windows. Apps X11 puros (`xterm`)
não passam por esse caminho — por isso um funciona e o outro não.

Solução (já aplicada no `/etc/xrdp/startwm.sh`): forçar X11 em todos os
toolkits — `GDK_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`,
`MOZ_ENABLE_WAYLAND=0`, `ELECTRON_OZONE_PLATFORM_HINT=x11`.

Diagnóstico — se o processo está em `:10` mas a janela não aparece na sessão,
é isso. Liste o que o servidor X realmente tem:

```bash
DISPLAY=:10 XAUTHORITY=~/.Xauthority xwininfo -root -children | grep '"'
```

Teste de um caso isolado, sem reconectar:

```bash
GDK_BACKEND=x11 <o-app>
```

**A janela abriu, mas eu não vejo.** Pode estar em outro monitor ou em outro
workspace. Use `Alt+Tab` (que lista até as janelas minimizadas, por causa do
`cycle_hidden`) ou `Ctrl+Alt+←/→` para trocar de workspace.

O `$mod+Ctrl+setas` que este trecho recomendava era sintaxe do i3 e não vale
aqui. Para trazer uma janela do outro monitor, use `Super+Shift+←/→` (ver a
seção de atalhos).

**Um atalho existe na configuração mas simplesmente não faz nada.**

Aconteceu com o `Super+→` enquanto o `Super+←` funcionava — mesmo script, mesma
seção, um funcionava e o outro não.

**Causa (encontrada em 28/07/2026): um *grab* órfão do `xfsettingsd`.**

No X, um *passive grab* de tecla só tem **um dono**. Enquanto `<Super>Right`
existir como atalho de **comando** (`/commands/custom/…`), quem registra o grab
é o `xfsettingsd`. Ao apagar essa propriedade ele atualiza a configuração mas
**não devolve o grab** — segue dono da tecla até morrer. A partir daí o xfwm4
leva `BadAccess` ao tentar registrar o dele, e a ação de janela nunca roda.

O que torna isso difícil de enxergar é que **tudo o mais parece certo**: a
configuração está correta, a tecla chega ao servidor X, e a ação funciona — dá
para provar ligando `tile_right_key` a outra tecla (`<Super>F9` encaixa a
janela à direita normalmente). Só aquela combinação está morta.

A ironia: quem envenenava a tecla era o próprio `diag-super-direita.sh`, que
grava `/commands/custom/<Super>Right` para testar. Cada rodada de diagnóstico
matava o `Super+→` pelo resto da sessão — justamente a tecla que investigava. E
como o estado sobrevivia a qualquer remove/recria no xfconf, parecia falha
insolúvel do xfwm4 no arranque. O script já foi corrigido para reiniciar o
`xfsettingsd` ao sair.

**Correção:** recriar a propriedade **não basta** — é preciso soltar o grab
antes, e só o `xfsettingsd` morrendo faz isso.

```bash
pkill -x xfsettingsd; sleep 2; setsid xfsettingsd >/dev/null 2>&1 &
sleep 2
xfconf-query -c xfce4-keyboard-shortcuts -p '/xfwm4/custom/<Super>Right' -r
xfconf-query -c xfce4-keyboard-shortcuts -p '/xfwm4/custom/<Super>Right' -n -t string -s tile_right_key
```

Reiniciar o `xfsettingsd` é seguro: no `startwm.sh` ele sobe com `&`, não é o
`exec` final, então derrubá-lo não encerra a sessão. Os atalhos de comando
(`Ctrl+Alt+T`) voltam junto com ele.

**A regra prática que sai daí:** nunca deixe a mesma combinação de teclas em
`/commands/custom/` e em `/xfwm4/custom/`. Se precisar migrar uma tecla de
comando para ação de janela, reinicie o `xfsettingsd` no meio.

**E o Windows não tem nada a ver com isso.** Ficava a suspeita de que o Aero
Snap do Windows engolia o `Win+→` antes de chegar na sessão. **Foi medido e é
falso:** ligando a combinação a um comando que só cria um arquivo, tanto o
`Win+→` quanto o `Win+←` carimbam. As duas teclas chegam. Não há motivo para
mexer em `keyboardhook:i:1` nem para trocar o `.vbs` por um `.rdp`.

Para refazer essa medição a qualquer momento:

```bash
bash ~/linux-fullscreen/diag-super-direita.sh
```

> **Encerrado em 29/07/2026: não era corrida na largada.** A causa real é que o
> xfwm4 **guarda uma tecla por ação**, e havia duas apontando para
> `tile_right_key` (`<Super>Right` e o `<Super>KP_Right` do padrão do XFCE). Só
> uma consegue o grab, e a vencedora é a última gravada — daí a intermitência.
> Medido, previsto e confirmado por falsificação; veja **"A regra que faltava:
> uma tecla por ação"**, na seção "Atalhos". O `xfwm-atalhos.sh` já remove a
> duplicata, e o sintoma não tem mais como voltar por esse caminho.
>
> As duas explicações abaixo ficam registradas porque descrevem mecanismos
> **reais** — só não eram *esta* falha. O grab órfão do `xfsettingsd` continua
> valendo se você puser a mesma tecla em `/commands/custom/` e `/xfwm4/custom/`.

**Sobre a "corrida na largada".** A versão anterior deste README suspeitava do
`setxkbmap` trocando o mapa do teclado enquanto o xfwm4 registrava os grabs.
Isso nunca foi provado, e hoje se sabe que era desnecessário: a colisão de ação
explica o sintoma inteiro, inclusive a assimetria entre `Super+←` e `Super+→`,
que nenhuma corrida explicaria bem.

Um agravante real foi removido: o `~/.bashrc` rodava `setxkbmap` **a cada shell
novo**, não só no arranque. Cada terminal aberto trocava o mapa de teclado da
sessão inteira, e toda troca de mapa invalida os grabs já registrados. A linha
está comentada — quem aplica o layout é o `startwm.sh`, uma vez por sessão, e o
xrdp já carrega `br(abnt2)` a cada conexão (vindo do `keylayout 0x00000416` que
o mstsc envia).

**Placar de sessões novas:**

| Data | `Super+→` no arranque | Observação |
|---|---|---|
| 28/07/2026 | **funcionou** | 1ª sessão após corrigir o grab órfão e o `.bashrc`; nenhuma intervenção manual |
| 29/07/2026 | **falhou** | e foi essa falha que levou à causa real — a colisão de ação |

O `startwm.sh` reaplica os atalhos 4 segundos depois que a sessão sobe. Esse
bloco era marcado como CONTORNO e como "o primeiro a sair quando a causa real
aparecer". **A causa apareceu, e a conclusão se inverteu: agora ele deve ficar** —
não como contorno, mas porque é ele que remove a duplicata do teclado numérico e
grava as setas **por último**, que é o que garante o grab. Sem ele, o vencedor
volta a depender da ordem do XML.

O efeito colateral a conhecer é que qualquer atalho que você mudar à mão nesses
quatro (`Super+setas`) é sobrescrito a cada login — para mudar de verdade, edite
o `xfwm-atalhos.sh`.

**Só um monitor aparece.** O `/multimon` só vale na hora de conectar —
monitor plugado depois não entra. Reconecte.

**Os acentos não funcionam: `~` + `a` + `o` sai como `"o"`.**

A tecla morta engole a vogal e não devolve nada. É fácil confundir com layout
errado, mas **não é** — e vale checar antes de mexer em teclado:

```bash
setxkbmap -query                       # espera-se layout br, variant abnt2
grep -c '' /usr/share/X11/locale/pt_BR.UTF-8/Compose
locale -a | grep pt_BR                 # pt_BR.utf8 tem que estar gerado
```

Se tudo isso estiver certo (e costuma estar — o mstsc manda
`keylayout 0x00000416` e o xrdp carrega `br(abnt2)` sozinho), o culpado é o
input method:

```bash
echo "$GTK_IM_MODULE"
```

Se responder `xim`, é isso. Esse módulo manda o GTK falar com um **servidor XIM
externo** (ibus, fcitx, scim) — e não há nenhum rodando nesta sessão, que não
tem desktop environment para subir um. Sem servidor, a tecla morta é consumida
e a composição nunca acontece. Sem a variável, o GTK usa o módulo `simple`
embutido, que lê a tabela `Compose` e compõe certo.

A linha estava no `~/.bashrc` e já foi removida. Dois detalhes que enganam:

- **Só afeta o que nasce de um shell.** O `xfce4-terminal` que o `startwm.sh`
  abre não herda a variável e digita acentos normalmente; o VS Code aberto a
  partir de um terminal herda, e falha. Como quase tudo é digitado no editor, o
  sintoma parece universal quando não é.
- **Processo em execução não relê o `.bashrc`.** Depois de corrigir, feche e
  reabra o aplicativo — reiniciar o shell não basta.

Para confirmar quem está contaminado:

```bash
for p in $(pgrep -u "$USER" .); do
    tr '\0' '\n' < /proc/$p/environ 2>/dev/null | grep -q '^GTK_IM_MODULE=xim$' \
      && echo "$p $(tr '\0' ' ' < /proc/$p/cmdline | cut -c1-60)"
done
```

**Teclado errado.** `setxkbmap -model abnt2 -layout br` está no `startwm.sh`,
dentro do bloco final que sobe a sessão; ajuste ali se o seu não for ABNT2.
(Ficava no `i3.config` na versão antiga.)

## O disco não encolhe: limpar por dentro não devolve espaço ao Windows (31/07/2026)

Sintoma: você roda `docker prune`, libera dezenas de GB dentro do Linux, e o
`ext4.vhdx` no Windows continua do mesmo tamanho. **Isso é por design** — o
VHDX só cresce. São duas operações separadas, e só a primeira é óbvia.

### Não use `--set-sparse`

A resposta que a internet dá é converter o disco para esparso:

```powershell
wsl --manage Ubuntu-24.04 --set-sparse true
#   O suporte a VHD esparso esta atualmente desativado devido a
#   POSSIVEL CORRUPCAO DE DADOS.
#   Para forcar: ... --set-sparse true --allow-unsafe
```

**A Microsoft desativou o recurso.** O `--allow-unsafe` existe para forçar, e
foi forçado aqui uma vez — sem incidente, mas revertido em seguida. Não vale o
risco numa máquina com dados de pesquisa.

E não resolveu: o esparso só encolhe **daí em diante**, e mesmo assim depende do
`fstrim`. Depois de forçado, o arquivo continuava ocupando 207 GB para 173 GB de
uso real.

### O caminho que funciona

```bash
sudo fstrim -av          # dentro da WSL: marca os blocos livres
```
```powershell
wsl --shutdown
wsl --manage Ubuntu-24.04 --set-sparse false    # se estiver esparso
```
```
diskpart
select vdisk file="C:\Users\<voce>\AppData\Local\Packages\CanonicalGroupLimited.Ubuntu24.04LTS_79rhkp1fndgsc\LocalState\ext4.vhdx"
attach vdisk readonly
compact vdisk
detach vdisk
```

> **A ordem importa, e é contraintuitiva.** O `diskpart` **recusa** anexar um
> VHDX esparso: *"Os arquivos de disco rígido virtual devem ser descompactados e
> descriptografados. Eles não devem ser analisados."* Então é reverter o esparso
> **primeiro** e compactar depois — mesmo parecendo que reverter vai inflar o
> arquivo. Não infla muito: preencher os buracos custou 8 GB aqui, não os 215
> do tamanho lógico.

O `attach readonly` não é decoração: garante que nada escreva enquanto o disco é
reorganizado.

### O resultado medido

```
VHDX:      215,1 GB  ->  181,2 GB
Livre C::   30,8 GB  ->   57,3 GB
```

E o critério para saber se vale repetir: compare o VHDX com o `df` de dentro.
Aqui ficou **181,2 contra 173 GB usados** — 8 GB de folga, que são metadados do
ext4 e granularidade de bloco. Não há mais o que extrair; se a diferença estiver
nessa ordem, o disco já está compacto e o espaço tem que sair de dentro.

## A bancada: o "VS Code" próprio, em 2,1 MB (01/08/2026)

O `trabalho` (abaixo) resolveu o layout, mas dependia de terminal. A **bancada**
é o segundo programa compilado deste repositório — C com Xlib cru, como a barra —
e é uma aplicação de verdade:

```
+--------------------------------------------------------------+
| [Projeto] [Salvar] [Claude]        caminho/do/arquivo.c   *   |
+-------------+------------------------------------------------+
|  árvore     | [Claude] [bancada.c * x] [README.md  x]        |
|  de         +------------------------------------------------+
|  arquivos   |  o painel da aba escolhida, ocupando tudo:     |
|             |  ou o editor de um arquivo, ou o xterm com o   |
|             |  Claude Code                                   |
+-------------+------------------------------------------------+
```

| | PSS |
|---|---|
| VS Code | 845 MB |
| **bancada** | **2,1 MB** |
| + o xterm embutido | 7,2 MB |

### O terminal é de verdade, e embutido

O Claude Code é um programa de terminal; não há como desenhá-lo com Xlib, e
escrever um emulador de terminal seria um projeto inteiro. Mas o X11 deixa a
janela de **outro cliente** virar filha da nossa: o `xterm` aceita `-into <id>` e
passa a viver dentro do painel. O binário do Claude vem do `trabalho onde`,
então há um só lugar no projeto que sabe onde ele mora. Esse xterm só nasce
quando alguém pede — veja *[O Claude não sobe sozinho](#o-claude-não-sobe-sozinho-01082026)*.

> Com `-into` o xterm **não se estica sozinho** — ele nasce com 80x24 e fica
> assim. Quem o acerta é um `XQueryTree` no painel, redimensionando os filhos.
> E é preciso `SubstructureNotifyMask` no painel para saber a hora: o xterm
> aparece de forma assíncrona, bem depois do `fork`.

### Xft, e não a fonte core da barra

A `barra-tarefas.c` usa fonte **core** em `iso8859-1`, e isso está certo lá: são
rótulos ASCII de dez letras e o visual arcaico depende de não haver
antialiasing. Aqui seria um desastre. Um editor com fonte `iso8859-1`
**corrompe, ao salvar, todo caractere fora do latin-1** — e este README é cheio
de `—`, `→` e acento. Editor que estraga arquivo é pior que nenhum editor.

O custo é uma biblioteca a mais **neste** binário, não na barra.

### A armadilha que quase matou o acento do ABNT2

Digitar `´` + `a` saía `a`, não `á`. O mesmo `xterm`, no mesmo ambiente, compunha
certo — então não era a sessão. A instrumentação mostrou a causa:

```
EV tipo=2 win=2a00003 filtrou=0        <- a tecla chegou na janela do EDITOR
IM: xic criado com XNFocusWindow = 2a00001   <- mas o contexto aponta para a MÃE
```

**O X entrega a tecla à menor janela sob o ponteiro que a tenha pedido**, subindo
dali — não simplesmente à janela com foco. Como o editor tinha pedido
`KeyPressMask` sem precisar, os eventos chegavam com `window=w_ed`, o
`XFilterEvent` não os reconhecia como do contexto de entrada, e o XIM nunca via a
tecla morta. Tirando o `KeyPressMask` do filho, o evento sobe até a mãe e compõe.

Verificado depois do conserto: `´` + `a` grava `c3 a1` no disco.

### O que foi medido no arquivo, não só na tela

O risco real de um editor é o arquivo, então os testes foram sobre os bytes,
com um arquivo de tortura (`é á ç ã`, `—`, `→`, emoji):

| Teste | Resultado |
|---|---|
| abrir e salvar **sem editar** | byte a byte idêntico |
| `Backspace` sobre `ã` | os **2 bytes** saíram juntos, sem byte órfão |
| `´` + `a` no fim de uma linha | `c3 a1` gravado; as outras linhas intactas |
| `Ctrl+Z` | volta ao estado anterior |

Dois casos corrigidos ao reler, antes de testar: arquivo **vazio** ganhava uma
linha em branco ao salvar, e arquivo **CRLF** era convertido para LF sem avisar
(o diff seguinte mostraria o arquivo inteiro mudado). Agora o final de linha é
lembrado e devolvido como estava.

### O que ela não tem, de propósito

Sem realce de sintaxe, busca ou auto-completar. Para editar a sério, `nvim` e
`vim` estão no disco. Isto é para abrir, olhar, corrigir uma linha e salvar. O
desfazer agrupa a digitação: uma sequência de teclas é **um** passo, senão os 32
instantâneos não alcançariam nem uma frase.

> **"Seleção de mouse" e "área de transferência" estavam nesta lista até
> 02/08/2026** — e saíram dela: hoje há seleção com mouse e teclado, copiar,
> recortar, colar, selecionar tudo e mover linha. Ver *[Selecionar e copiar
> (02/08/2026)](#selecionar-e-copiar-02082026)* e *[O resto do editor: colar,
> recortar, mover (02/08/2026)](#o-resto-do-editor-colar-recortar-mover-02082026)*.

### Selecionar e copiar (02/08/2026)

O pedido veio assim: *"não consigo ver o que estou selecionando no bancada e dar
Copiar"*. Não era regressão — a seleção nunca existiu, e a ausência estava
escrita no cabeçalho do `bancada.c`. Passou a existir:

| Gesto | O que faz |
|---|---|
| arrastar com o botão esquerdo | marca, com realce azul e texto branco |
| duplo clique | a palavra sob o ponteiro |
| triplo clique | a linha inteira, **com** o `\n` |
| `Ctrl+C` | copia; **sem nada marcado, copia a linha do cursor** |
| arrastar para fora da janela | rola enquanto marca |

**No X não existe "área de transferência".** Existe um *dono vivo*: quem copiou
fica com o texto e responde ao `SelectionRequest` de quem for colar. Por isso o
texto copiado mora dentro do processo, e **fechar a bancada leva junto o que foi
copiado** — igual a qualquer aplicativo X sem gerenciador de área de
transferência. A bancada vira dona das **duas** seleções, porque elas são coisas
diferentes e as duas são usadas aqui:

- `PRIMARY` — a do X de sempre: marcou, já está lá; cola com o **botão do meio**;
- `CLIPBOARD` — a do `Ctrl+C` / `Ctrl+V`, que é o que os aplicativos de hoje
  esperam.

Três decisões que não são óbvias:

- **`Ctrl+C` não entra no `XGrabKey`.** Só o `Ctrl+Tab` é capturado por grab.
  Com grab, o `Ctrl+C` deixaria de chegar ao Claude da aba 0 — onde ele é o
  "interrompe isso" — e a bancada estaria roubando a tecla mais importante do
  terminal. Sem grab, ele só chega quando a janela da bancada tem o foco.
- **Digitar apaga a marca, não o texto.** Não há "digitar por cima da seleção":
  o realce some e o caractere entra onde o cursor está. Num editor que existe
  para corrigir *uma* linha, um apagar que ninguém pediu é pior do que um gesto
  a menos.
- **A seleção morre na troca de aba.** Ela vive nos globais do editor, como o
  cursor; depois da troca esses globais apontam para o buffer de outro arquivo,
  e um intervalo guardado não quer dizer nada lá. É o mesmo cuidado que já
  governa o `globais_zerar()`.

O `w_ed` passou a pedir `ButtonReleaseMask` e `Button1MotionMask` — e **continua
sem `KeyPressMask`**, que é a invariante do acento morto do ABNT2.

#### Medido com eventos sintéticos, e o que o teste ensinou

Não há `xdotool` nesta máquina, então o teste é um cliente X de 130 linhas que
manda `ButtonPress`/`MotionNotify`/`ButtonRelease` para o painel do editor e
**pede a seleção de volta** — que é exatamente o que um "colar" de verdade faz.
Resultados, com o arquivo `linha um com acentuacao: funcao / linha dois / linha
tres`:

| Gesto | O que voltou pela seleção |
|---|---|
| arraste de (0,0) a (1,10) | `ha um com acentuacao: funcao⏎linha dois` |
| duplo clique na coluna 26 | `funcao` |
| duplo clique na coluna 10 da linha 1 | `dois` |
| triplo clique na linha 1 | `linha dois⏎` |
| `Ctrl+C` depois do duplo clique | `funcao`, **no CLIPBOARD** |

Duas armadilhas, e as duas eram do teste, não do editor:

- **`CurrentTime` (0) nos eventos sintéticos quebra o duplo clique.** A bancada
  separa clique de duplo por `time - t_clique < 400`; com tudo em zero, *todo*
  clique parecia o segundo do anterior, e um arraste virava seleção de linha. O
  teste passou a carregar um relógio próprio. Evento de verdade sempre traz o
  relógio do servidor — o sintético tem de imitar.
- **A sessão gravada guarda a rolagem**, e o teste começou com `topo=2` de uma
  rodada anterior. As três primeiras medições disseram `linha tres` para tudo, e
  pareciam bug de coordenada. Não eram: a tela estava rolada, e a instrumentação
  (`fprintf` com `l`, `c` e `topo`) mostrou isso na primeira linha de log. Vale a
  regra da casa — **meça o estado, não olhe a tela**.

E uma confirmação que veio de graça: no meio do teste um arraste **de verdade**,
com a mão, apareceu no log e devolveu `ha um com acentuacao: funcao⏎linha dois`.
Evento sintético prova o caminho; o de verdade prova o gesto.

### O resto do editor: colar, recortar, mover (02/08/2026)

No mesmo dia, o pedido seguinte: *"temos que ter as funções de um editor de
texto, copiar, colar, mover"*. O conjunto ficou assim:

| Tecla | O que faz |
|---|---|
| `Ctrl+C` | copia (sem marca, a linha do cursor) |
| `Ctrl+X` | recorta |
| `Ctrl+V` | cola |
| `Ctrl+A` | seleciona tudo |
| `Shift`+setas/`Home`/`End` | estende a marca pelo teclado |
| digitar sobre a marca | substitui |
| `Backspace` / `Delete` sobre a marca | apaga o marcado |
| `Alt+↑` / `Alt+↓` | move a linha do cursor |

Quatro decisões que valem registro:

**Colar são duas funções, e é o X que obriga.** `XConvertSelection()` não
devolve texto nenhum — ele apenas avisa ao dono que alguém quer. O texto chega
depois, num `SelectionNotify`, e é o laço de eventos que termina o serviço. Por
isso `Ctrl+V` só dispara o pedido, e a inserção mora no laço.

**Pede o `CLIPBOARD` e, se não houver dono, o `PRIMARY`.** Sem esse plano B,
colar o que foi selecionado num xterm — que só povoa o `PRIMARY` — não
funcionaria, e pareceria bug em vez de decisão.

**O texto que chega passa por um filtro, e o `\r` é o motivo.** Texto vindo do
Windows chega como `\r\n`, e o `inserir_texto()` trata os dois como quebra de
linha: uma linha viraria duas. O `\r` sai na cola; quem decide se o arquivo
grava `\r\n` é a flag `crlf`, na hora de salvar. Byte de controle abaixo de 32
que não seja `\n` ou `\t` também sai — deixaria lixo invisível no meio do
código. UTF-8 passa inteiro.

**Apagar-e-escrever é UM passo do desfazer.** Colar ou digitar por cima de uma
marca são duas operações no buffer, e um `Ctrl+Z` que desfizesse só metade
seria pior do que não ter. O `apagar_selecao()` de propósito **não** guarda
instantâneo — quem chama é que sabe se aquilo é um passo sozinho ou a primeira
metade de um par.

#### Medido, tudo por evento sintético

Mesmo arnês da seleção, agora mandando teclas. Arquivo de partida: `linha um` /
`linha dois` / `linha tres`. Cada gesto termina em `Ctrl+S`, e o teste lê o
**arquivo em disco** — não o que a tela mostra:

| Gesto | Resultado |
|---|---|
| `Shift+End`, `Ctrl+X` | linha 0 vazia; CLIPBOARD = `linha um` |
| `Ctrl+V` com `AAA\r\nBBB` na área | virou `AAA` + `BBBlinha um` — **uma** quebra, não duas |
| `Shift+End`, digitar `Z` | linha 0 virou `Z` |
| `↓`, `Alt+↑` | `linha dois` e `linha um` trocaram de lugar |
| `Ctrl+A` → PRIMARY | as três linhas, com os `\n` |
| `Shift+→` ×5 → PRIMARY | `linha` |
| marcar sem editar | arquivo intacto |
| marcar, colar por cima, **um** `Ctrl+Z` | arquivo de volta ao original |

O último é o que prova a decisão do desfazer: um único `Ctrl+Z` devolveu o
texto apagado *e* removeu o colado.

## Abas, e o Claude como uma delas (01/08/2026)

A bancada nasceu com **um** arquivo por vez e o Claude num painel fixo embaixo.
Os dois problemas eram o mesmo: o painel de baixo roubava altura do editor
mesmo quando ninguém olhava para ele, e abrir outro arquivo perdia o primeiro.
Agora há uma faixa de abas, a aba `[0]` é o Claude, e a aba escolhida ocupa o
painel inteiro.

Medido: **2,2 MB** de PSS parado, **2,6 MB** com três arquivos abertos (um deles
este README, de 3410 linhas). O custo do recurso é o texto em si.

### As abas guardam os globais, não recebem um contexto

O editor inteiro (`inserir_texto`, `desfazer`, `seguir_cursor`, o anel de
desfazer) trabalha sobre variáveis globais. O caminho "certo" de livro seria
trocar tudo por um ponteiro de contexto — uns cem pontos de edição, cada um uma
chance de errar. Foi feito o contrário: a aba é uma struct **onde os globais
dormem**. Trocar de aba é `globais_para_aba()` na que sai e `aba_para_globais()`
na que entra.

Como `linhas` é um ponteiro e o anel são 32 structs, **trocar de aba não copia
texto nenhum**, e nenhuma função de edição precisou de uma linha a mais.

Duas armadilhas que isso cria, e as duas apareceram:

- **`globais_zerar()` não pode liberar nada.** O que os globais apontam já
  pertence à struct da aba que acabou de ser salva; um `limpar_buffer()` ali
  liberaria o texto da aba vizinha, e o estrago só apareceria ao voltar para
  ela. A função larga os ponteiros sem tocar neles.
- **`fechar_aba()` não pode usar `limpar_buffer()`/`esquecer_undo()`**, que
  mexem nos globais: a aba fechada pode não ser a carregada. Ela libera direto
  os campos da struct.

Rodado sob AddressSanitizer o ciclo inteiro — abrir três, trocar, fechar a de
baixo, fechar a ativa, fechar a última, reabrir do zero: **nenhum erro**.

### Correção: `seguir_cursor()` na troca de aba desfazia a rolagem

Primeira versão chamava `seguir_cursor()` ao ativar uma aba, por simetria com o
`redimensionar()`. Errado. Essa função existe para arrastar a tela **até o
cursor**, e o cursor só anda pelo teclado. Quem tinha descido o arquivo com a
roda do mouse, sem tocar no cursor, voltava para a linha 1:

| | `topo` |
|---|---|
| rolei o `install.sh` até | 36 |
| troquei de aba e voltei | **0** |
| depois de tirar o `seguir_cursor()` | **36** |

O `topo` e o `col0` guardados na aba já são válidos — foram salvos num estado
coerente, e nenhum resize os invalida.

### O Ctrl+Tab tem de furar o xterm

Com o Claude virando aba, entrar nela pelo teclado e não conseguir sair seria
uma armadilha: o foco vai para o xterm, e daí em diante toda tecla é dele. A
saída é `XGrabKey` **na janela-mãe** — um grab alcança a subárvore inteira, e o
xterm é neto dela.

Verificado com tecla de verdade (extensão XTest; evento sintético não passa por
grab): com o foco confirmado no xterm embutido, `Ctrl+Tab` trocou a aba.

Grava-se só o `Tab`, com as quatro combinações de `CapsLock`/`NumLock` — cada
tecla capturada aqui é uma tecla a menos para o Claude Code, que usa o
`Shift+Tab`.

### Um tratador de erro do X, agora obrigatório

Trocar de aba desmapeia uma janela e mapeia outra, e `XSetInputFocus` numa
janela recém-desmapeada devolve `BadMatch` — cujo tratador padrão do Xlib
**mata o processo**. Com um painel fixo isso era hipótese; com abas passa a
acontecer na operação normal. `BadMatch` e `BadWindow` agora são ignorados; o
resto continua indo para o `stderr`.

### O que mudou de comportamento

- Abrir um arquivo **nunca mais sobrescreve** o buffer, então sumiu o
  "descartar alterações?" do abrir. A pergunta migrou para o **fechar**, que é
  onde o trabalho corre risco de verdade. Sair pergunta se **qualquer** aba
  estiver suja, inclusive uma que não está na tela.
- Clicar num arquivo já aberto **traz a aba para a frente** em vez de reler o
  disco.
- Na árvore há três estados agora: aceso (aba da frente), azul (aberto em outra
  aba) e normal.
- O teto do desfazer caiu de 8 MB para **2 MB por aba**: com 16 abas o número
  antigo viraria 128 MB de teto, o que briga com a premissa de RAM baixa. Para
  arquivo de código normal, 2 MB ainda dão os 32 passos inteiros.

### A sessão volta; o texto não salvo, não

As abas abertas voltam na próxima vez, **por projeto**, de
`~/.config/bancada/sessoes/`. O nome do arquivo é o caminho do projeto com `/`
virando `%` — legível, sem hash e sem colisão:

```
# bancada: sessao de /home/yosef/linux-fullscreen
atual 3
aba 0 0 0 0 /home/yosef/linux-fullscreen/bancada.c
aba 0 0 45 0 /home/yosef/linux-fullscreen/README.md
```

Grava-se só o que dá para reconstruir do disco: caminho, cursor e rolagem.
**Conteúdo não salvo não entra.** Um editor que ressuscita texto que você não
mandou gravar precisa acertar isso *sempre*, e errar uma vez custa o arquivo;
fechar com alteração pendente continua perguntando, que é a garantia simples e
que já funciona.

Grava a cada mudança de aba, não só ao sair — abrir, fechar e trocar passam
todos pelo `ativar()`. Por isso a sessão sobrevive a um `kill -9`, verificado.
São poucas linhas num arquivo minúsculo, no ritmo de um clique humano.

Só entram na sessão os arquivos **dentro** do projeto. Sem essa regra, trocar
de projeto pelo botão levaria os arquivos do projeto velho para a sessão do
novo, e a abertura seguinte viria com arquivos que não são dali.

Testado com a sessão envenenada de propósito — arquivo apagado, um `.png` no
lugar de código, cursor em `999999`, linha truncada, lixo, e um caminho de 6000
bytes:

| | |
|---|---|
| processo | sobreviveu |
| diálogos do zenity na abertura | nenhum (o `avisar()` é mudo enquanto restaura) |
| arquivo regravado | só as três abas válidas |
| AddressSanitizer | nenhum erro |

O arquivo repetido virou "traz a aba que já existe", que é o mesmo caminho de
clicar duas vezes no mesmo arquivo na árvore.

### A conversa do Claude fica de fora, e isso é escolha

A aba do Claude sobe limpa. O Claude Code sabe retomar sozinho —
`claude --continue` pega a conversa mais recente **daquela pasta**, e
`claude --resume` abre um seletor —, mas subir sempre com `--continue` faria
toda abertura cair na conversa velha, arrastando o contexto anterior junto e
exigindo `/clear` para começar do zero. Quem quer a conversa anterior pede por
ela, dentro da própria aba.

### O Claude não sobe sozinho (01/08/2026)

A aba do Claude existe desde o início — ela é a aba `[0]`, não fecha e não tem
arquivo. O **processo**, não: a bancada abre sem xterm nenhum. Quem levanta o
Claude é o botão `[Claude]` da barra ou um clique no painel vazio da aba.

> **O convite escrito saiu em 02/08/2026.** Até então o painel vazio trazia duas
> linhas centralizadas — *"O Claude nao sobe sozinho."* e *"Clique aqui, ou no
> botao Claude da barra, para abrir."* — pelo argumento de que quem caísse na aba
> pelo `Ctrl+Tab` não teria como adivinhar o clique. Removido a pedido de quem
> usa: o argumento vale para quem chega de fora, e aqui não chega ninguém de
> fora. As duas portas de entrada continuam as mesmas; o painel é só preto agora.
>
> Com isso o `w_term` **deixou de pedir `ExposureMask`** — não há mais nada nosso
> para desenhar ali, e o preto vem do `background_pixel` da janela, que o próprio
> servidor X repinta na área exposta sem passar pelo cliente. O `ButtonPressMask`
> fica, que é o clique.
>
> **Não economizou memória, e foi medido para não ficar a suspeita.** Os dois
> binários abertos no mesmo projeto, sem Claude: 1.929 kB de PSS antes, 1.934 kB
> depois (`smaps_rollup`) — diferença dentro do ruído. Era esperado: sai texto
> desenhado, não estrutura alocada. A mudança é de gosto visual, e o ganho é a
> tela limpa.

Antes ele subia junto com a janela. Abrir a bancada para olhar um arquivo
custava os **490 MB** do Claude Code sem ninguém ter pedido — e numa premissa de
RAM baixa isso é o oposto do que se quer. Medido hoje, a bancada aberta com a
árvore carregada e sem Claude: **2,4 MB de PSS** (`smaps_rollup`, `Pss:
2417 kB`).

O que **não** levanta o Claude, de propósito: a abertura, a restauração da
sessão, e o `Ctrl+Tab` que passa pela aba — cair nela pelo teclado dá o painel
preto, e só. Trocar de projeto pelo botão `[Projeto]` também não: se o Claude **já** estava
de pé, ele reabre na pasta nova, como antes; se não estava, continua não
estando, e a primeira abertura já nasce na pasta certa, que é a de agora.

Quatro detalhes de X11 que isso exigiu:

- ~~o `w_term` passou a pedir `ExposureMask`. Enquanto não há xterm, aquele
  painel é **nosso** e precisa saber quando repintar.~~ **Desfeito em
  02/08/2026**, junto com o convite: sem nada nosso para desenhar ali, o
  `background_pixel` da janela dá conta, e o repintar vira trabalho do servidor.
- ~~de brinde, o convite **volta sozinho** quando o Claude sai: destruir a janela
  filha gera um Expose no pai, o `kill(pid, 0)` falha e o painel se redesenha.~~
  Sem convite, o que volta é o preto — e volta pelo mesmo caminho, só que sem
  cliente nenhum envolvido.
- sem processo não há xterm para receber foco, então o `mostrar_painel()` devolve
  o foco à janela-mãe — que é quem tem o XIC e o teclado. Sem isso a aba comeria
  as teclas sem ninguém para tratá-las.
- o xterm nasce **assíncrono**: no instante do clique ele ainda não existe para
  receber foco. Quem o foca é o `MapNotify`, quando ele passa a existir.

Medido nas duas portas de entrada, com evento sintético de `ButtonPress` (não há
`xdotool` nesta máquina): abertura → `0` filhos; clique no painel → o `xterm
-into` aparece; reiniciada, clique no botão da barra → idem.

### `bancada` no shell, como o `code .`

O `perfil.sh` do repositório instala uma função em
`/usr/local/share/linux-fullscreen/perfil.sh`, carregada por uma linha marcada
no `~/.bashrc` (idempotente, com backup). Ela abre na pasta atual e **devolve o
prompt**: 5 ms medidos, com `setsid --fork` para a bancada sobreviver ao
terminal que a abriu.

Fica fora do repositório depois de instalada de propósito — se o `.bashrc`
apontasse para a pasta do repositório, mover o repositório quebraria o shell de
quem só queria abrir um terminal. E não é `/etc/profile.d`: terminal interativo
**não-login** (o caso normal aqui) lê o `.bashrc` e ignora o `profile.d`.

Para ver as mensagens de erro dela, `command bancada` foge da função e roda o
binário em primeiro plano.

**Ela já vale em qualquer pasta, e isso foi conferido em 02/08/2026**: de
`~/Documents`, `bancada .` devolveu o prompt e o processo nasceu com
`/proc/<pid>/cwd -> /home/yosef/Documents`, ou seja, a árvore abriu no projeto
certo. Se num terminal específico o comando não existir, o caso é sempre o
mesmo e não é a instalação: **aquele shell é anterior ao `install.sh`**. O
`.bashrc` é lido no arranque do shell, então terminal já aberto não enxerga a
função — abra um novo (ou `. ~/.bashrc`). Vale em qualquer shell interativo
desta sessão, incluindo o do `terminal.c`, que roda `$SHELL -i`.

### Binário e imagem: a bancada deixou de recusar (01/08/2026)

Clicar no `Untitled.png` ou no `bancada` compilado, na árvore, dava só um diálogo
de erro: *"isso parece um arquivo binário; a bancada não abre"*. Um projeto tem
binário compilado e imagem no meio dos fontes, e o VS Code mostrava os dois — era
a lacuna mais visível de quem trocou um pelo outro.

Agora abrem, os dois em **somente leitura**, cada um no seu modo:

| Modo | Quando | O que aparece |
|---|---|---|
| `MODO_TEXTO` | o de sempre | o editor |
| `MODO_HEX` | qualquer byte `NUL` no arquivo | deslocamento, 16 bytes, ASCII |
| `MODO_IMG` | número mágico de PNG, JPEG, GIF, BMP, WEBP ou TIFF | a imagem desenhada, com legenda |

Modo é mais um campo que **dorme na aba** junto com os globais, como o buffer e o
cursor — nenhuma função de edição precisou de contexto novo. Vale zero de
propósito: toda aba nascida de um `memset()` continua sendo de texto.

**O `salvar()` recusa na porta o que não é texto.** É a linha mais importante das
mudanças: hex e imagem não têm buffer de linhas, então um `Ctrl+S` numa aba
dessas percorreria zero linhas — e o `fopen(…, "wb")` **já teria truncado** o
arquivo antes disso. Um `.png` de 27 KB viraria 0 byte, calado.

**Detecção pelo número mágico, nunca pela extensão**, e sobre os primeiros 512
bytes, antes de trazer o arquivo para a RAM: uma foto de 30 MB não é lida aqui
para ser jogada fora em seguida — da imagem não entra byte nenhum neste processo.

**Mas o `NUL` é decidido sobre o arquivo INTEIRO**, e isso é uma correção de um
erro que eu já tinha escrito: com o cheiro olhando só o começo, um arquivo de
texto com um `NUL` no meio abriria como texto, e o `texto_para_buffer()` — que
varre com `strchr()` — pararia no `NUL`, jogando fora tudo o que vem depois. O
`Ctrl+S` seguinte gravaria o arquivo truncado. Conferido com um arquivo de 53
bytes e um `NUL` na terceira linha: abre em hex, e as quatro linhas estão lá.

#### A imagem não trouxe biblioteca de imagem nenhuma

Quem decodifica é o `convert` do imagemagick — que já é dependência declarada do
projeto, pelo `barra-apps` — chamado **uma vez por abertura** e devolvendo PPM
cru pelo pipe (`P6`, largura, altura, `255`, pixels). O alfa é achatado sobre o
`#DEDEDE` da face antes de sair de lá, como nos ícones da barra, então não chega
canal alfa aqui e não há nada para compor. Medido em 01/08/2026 com o
`Untitled.png`: **16 MB de pico e 0,03 s**, e o processo morre antes do desenho.

É a mesma divisão de trabalho do `barra-tarefas.c`: pixels crus aqui dentro,
formato de imagem lá fora. E aqui a comparação é honesta nos dois sentidos — a
`libpng` **já está no processo** (a freetype, que a Xft usa, a carrega), então
linká-la não custaria RSS. Custaria o decodificador escrito à mão, e resolveria
só PNG. Pelo pipe vêm seis formatos por um caminho de código só.

O `identify` vai no mesmo pipe, antes do `convert`, só pela legenda: é o tamanho
**de verdade** da imagem, que o PPM já reduzido ao painel não conta mais. São
dois processos em sequência, nunca ao mesmo tempo.

O resultado vira `Pixmap`, do lado do servidor: redesenhar depois é um
`XCopyArea`. O `convert` só roda de novo se o painel mudar de tamanho — nunca a
cada `Expose`.

#### O que foi medido, e o que não

Com uma aba de imagem e uma de hex abertas: **PSS 2486 kB**, contra os 2,4 MB de
antes — a mudança não aparece na conta. Ela não é grátis, só não é deste lado: os
pixels ficam no servidor X, `largura × altura × 4` bytes (1,8 MB para este PNG),
e são liberados ao fechar a aba. **O crescimento do Xorg não foi medido.**

Verificado por captura de tela numa segunda instância, com sessão semeada num
projeto de teste (`~/.config/bancada/sessoes/`) para não encostar na sessão real
— não há `xdotool` nesta máquina, então abrir a aba pelo clique não era uma
opção. A instância de teste foi posta numa faixa da tela sem nenhuma janela por
cima, pelo motivo já registrado aqui: `import` mente quando a janela está coberta.
A rolagem do hex foi conferida pelo mesmo caminho — sessão com `topo 100`, e a
tela abriu no deslocamento `0x640`, que é 100 × 16.

## Trocar o VS Code por 31 MB (01/08/2026)

O VS Code servia de **árvore de arquivos** — o resto do trabalho já era no
terminal. Medido com PSS (não RSS: o `ps` mostrava 1946 MB porque conta a mesma
página várias vezes entre os 20 processos do Electron):

| | PSS |
|---|---|
| **VS Code** | **845–877 MB** em 11 processos |
| GROMACS | 708 MB |
| Brave | 614 MB |
| **Claude Code** | **490 MB** |
| xfwm4 | 27 MB |
| xfce4-terminal (4 janelas) | 16 MB |
| barra-tarefas | < 1 MB |
| **tmux + ranger** | **31 MB** |
| thunar (GUI, com arrastar e soltar) | 23 MB |

**Não há editor próprio aqui e não vai haver.** Escrever um seria reconstruir
justamente a parte que não se usa; para editar, `nvim` e `vim` já estão no disco.
O que faltava era só o *layout*, e ele virou o `trabalho`.

### O que o `trabalho` faz

```bash
trabalho            # ranger e Claude Code lado a lado, ou reconecta ao que existe
trabalho claude     # só o Claude, num terminal, na pasta do projeto
trabalho arquivos   # só o ranger
trabalho onde       # qual binário do Claude foi encontrado
trabalho instalar   # cria os .desktop e o ícone (uma vez por máquina)
```

**Por que tmux e não dois terminais lado a lado.** Pelo mesmo motivo que a sessão
RDP sobrevive a fechar o mstsc: fechar a janela não pode custar o trabalho. O
tmux guarda o ranger e o Claude vivos, e reabrir reconecta no ponto exato —
testado fechando a janela e clicando de novo, mesma sessão, mesmos processos.

### Armadilha: o Claude Code vinha dentro do VS Code — **resolvido em 02/08/2026**

> **Esta seção descreve um estado que não vale mais.** Conferido em 02/08/2026:
> `trabalho onde` responde `/home/yosef/.local/bin/claude`, um link para
> `~/.local/share/claude/versions/2.1.220` (263 MB) — o CLI **está** instalado
> sozinho agora. O `command -v claude` acha, então o bloco de busca na extensão
> nem chega a rodar, e **desinstalar o VS Code não leva mais o Claude junto**.
> Fica registrado porque o `onde_claude()` continua com o plano B da extensão,
> que é o que salva numa máquina onde só ela exista.

```
~/.vscode/extensions/anthropic.claude-code-2.1.220-linux-x64/resources/
    native-binary/claude
```

Era esse o único lugar do binário até então. Ele roda perfeitamente fora do VS
Code (`claude --version` responde `2.1.220`), mas:

- o caminho carrega a **versão**, então muda a cada atualização da extensão — por
  isso o `onde_claude()` procura e pega o maior por ordem de versão, em vez de
  chumbar;
- **desinstalar o VS Code levaria o Claude junto.** Era a razão de instalar o CLI
  pelo instalador oficial antes de mexer no VS Code — que é exatamente o que
  aconteceu aqui, e o que desarmou a armadilha.

E há a perda de integração, que é real: rodando como extensão do VS Code, o
Claude enxerga a **seleção** do editor, mostra **diff** das edições e transforma
`arquivo.c:42` em link. No terminal puro é o mesmo modelo e as mesmas
ferramentas, sem essas três coisas.

### Duas armadilhas de layout, ambas silenciosas

**1. A ordem das opções do `xfce4-terminal`.** As de JANELA (`--maximize`,
`--hide-menubar`) têm de vir **antes** das de ABA (`--title`, `--command`), e ele
não reclama quando está errado: com `--maximize` depois do `--title` a janela
abria com 815x458 sem nenhum aviso; com ele antes, 1920x1056.

**2. A largura do painel do ranger.** Porcentagem não sobrevive a redimensionar:
com `split-window -l 62%` numa sessão criada em 200 colunas, ligar um terminal de
80 deu **15 colunas para o ranger contra 64 do Claude** — o tmux encolhe tirando
de um lado só, não proporcionalmente. A solução é `main-vertical` com
`main-pane-width` em colunas absolutas, mais **dois hooks**:

```bash
tmux set-option -w -t "$S" main-pane-width 34
tmux set-hook   -t "$S" client-attached 'select-layout main-vertical'
tmux set-hook   -t "$S" window-resized  'select-layout main-vertical'
```

O segundo é o que importa, e tem de ser `window-resized`, **não**
`client-resized`. Só com `client-attached` a largura escapava de novo (89/100
numa janela de 190 colunas): o hook dispara quando o cliente conecta, com a
janela ainda no tamanho padrão, e o `--maximize` vem logo depois e reparte tudo
outra vez. Com `client-resized` o painel voltava a escorregar (7 e 192 colunas).
Com `window-resized`, o ranger fica em 34 colunas em qualquer largura — testado
com a janela em 120, 260 e 180 colunas, sempre 34.

### O ícone do Claude na barra

Os dois atalhos são `.desktop` de usuário em `~/.local/share/applications/`, e é
só isso: **a barra não tem caso especial para nenhum dos dois**, são apps como
qualquer outro no menu `[+]`.

O logo oficial vem junto da extensão do VS Code
(`resources/claude-logo.png`, 266x266 RGBA). O `trabalho instalar` o copia para
`~/.local/share/icons/hicolor/256x256/apps/claude-code.png` — assim o atalho
continua com ícone depois de a extensão atualizar ou sumir, que é o mesmo
problema de caminho versionado do binário.

## O Gerenciador de Tarefas mostra muito mais RAM que o htop

Não é contradição — medem coisas diferentes. O `htop`/`free` mostram memória de
**processos**; o `vmmemWSL` no Windows mostra tudo que a VM tomou: processos +
*page cache* do Linux + estruturas do kernel + folga.

O problema real é que **a WSL2 pega memória e não devolve**: ela infla até o
pico de uso e fica segurando, mesmo depois que o Linux liberou.

Existe um `autoMemoryReclaim=gradual` em `[experimental]` no `.wslconfig` que
resolveria isso — **mas não use.** Nesta máquina ele fez a VM ser desligada
sozinha a cada 5–10 minutos, derrubando a sessão RDP junto:

```
Exception:
unknown: Operation canceled @p9io.cpp:258 (AcceptAsync)
systemd-logind: The system will power off now!
```

Boots de mais de 30 minutos passaram a durar menos de dez, e voltaram ao normal
ao remover a linha. É correlação forte, não causa provada — mas é um recurso
experimental trocando estabilidade por um número mais bonito no Gerenciador de
Tarefas. Não compensa.

O jeito seguro de recuperar memória é reduzir o teto (`memory=`) ou desligar o
que não se usa — veja abaixo.

> **Comentei a linha e o problema continuou.** Comentar/apagar
> `autoMemoryReclaim` no `.wslconfig` **não muda nada sozinho** - esse arquivo só
> é lido num boot **totalmente frio da plataforma WSL**, o que é diferente de
> reconectar o RDP ou de a sessão gráfica cair e subir de novo (isso reinicia só
> a *distro*, não a VM utilitária do WSL). Se a VM já estava de pé com o recurso
> ativo quando você editou o arquivo, ela **continua rodando com a configuração
> antiga em memória** até alguém derrubar a plataforma inteira.
>
> Sintoma: a sessão RDP fecha sozinha poucos segundos depois de abrir, mesmo com
> a linha comentada. No `xrdp-sesman.log` aparece só "window manager exited with
> non-zero exit code" - parece o xfwm4 travando, mas é a VM inteira caindo por
> baixo dele (o mesmo `Exception: unknown: Operation canceled @p9io.cpp:258` +
> `systemd-logind: The system will power off now!` de sempre). Confirma-se pelo
> Visualizador de Eventos do **Windows**: `Get-WinEvent` no log `System` mostra a
> VM do Hyper-V (`Microsoft-Windows-Hyper-V-VmSwitch`) sendo destruída e recriada
> repetidas vezes em poucos minutos - prova de que a queda é da VM, não da sessão
> grafica.
>
> **Correção:** `wsl --shutdown` no PowerShell (derruba a plataforma inteira, não
> só o Ubuntu), *depois* reconectar. Confirmado em 28/07/2026: rodar isso parou
> os ciclos de queda na hora, sem nenhuma outra mudança. Vale sempre que você
> editar `.wslconfig` e a mudança não parecer ter feito diferença - não é só o
> `autoMemoryReclaim`, é qualquer opção desse arquivo.
>
> O `linux-desktop-down` (modo VM, "Desligar") já faz exatamente esse
> `wsl --shutdown` por dentro (só que via interop, chamando o `wsl.exe` do
> Windows a partir do Linux) - é o jeito certo de aplicar uma mudança de
> `.wslconfig` sem sair da sessão. O problema é só quando a VM está travando
> tão rápido (poucos segundos) que não dá tempo de abrir o appfinder e clicar
> nele antes da sessão cair sozinha; nesse caso extremo, o `wsl --shutdown` de
> fora (PowerShell) é o único caminho que sobra.

**A VM desliga sozinha logo após ligar o PC - mas só nas primeiras tentativas.**

Investigação longa (28/07/2026). Depois de reiniciar o Windows e rodar
`wsl --shutdown`, a VM continuava desligando sozinha em poucos segundos - o
que descartou o `.wslconfig` desatualizado como causa única. Sempre a mesma
assinatura no log do convidado:

```
Exception: unknown: Operation canceled @p9io.cpp:258 (AcceptAsync)
systemd-logind: The system will power off now!
```

Confirmado pelo Visualizador de Eventos do **Windows** que é a VM inteira
morrendo (o Hyper-V recria a VM do zero repetidas vezes em minutos), não a
sessão gráfica - o `xrdp-sesman.log` só mostra "window manager exited with
non-zero exit code" como sintoma colateral.

Duas pistas foram testadas e descartadas ou mantidas como mitigação sem prova
definitiva:

- **`vmIdleTimeout`** (detector de ociosidade do WSL 2.7.11.0, que parece olhar
  só para processos `wsl.exe` anexados - uma sessão RDP conectando direto na
  porta 3390 não conta como atividade pra ele). Foi setado como
  `vmIdleTimeout=-1` no `.wslconfig`. Manteve a VM viva mesmo sem ninguém
  conectado, mas **não impediu** a queda durante uma sessão ativa - descartado
  como causa principal, mas deixado no arquivo por via das dúvidas (inofensivo).

- **Windows Defender interrompendo I/O do disco.** No exato segundo de uma das
  quedas, o Visualizador de Eventos (log `Windows Defender/Operational`, ID
  5007) mostrou o serviço do Defender sendo reconfigurado/reiniciado. Antivírus
  reiniciando o driver de proteção em tempo real (`WdFilter`) pode travar I/O
  de disco por um instante - inclusive o `.vhdx` que sustenta a VM inteira.
  Excluído da varredura via PowerShell **administrador**:

  ```powershell
  Add-MpPreference -ExclusionPath "C:\Users\<voce>\AppData\Local\Packages\CanonicalGroupLimited.Ubuntu24.04LTS_<hash>\LocalState\ext4.vhdx"
  Add-MpPreference -ExclusionProcess "$env:windir\System32\vmwp.exe"
  Add-MpPreference -ExclusionProcess "$env:windir\System32\wsl.exe"
  ```

  (ache o caminho exato do `.vhdx` procurando por `*.vhdx` dentro de
  `%LOCALAPPDATA%\Packages\CanonicalGroupLimited.Ubuntu*`). É persistente -
  não precisa repetir a cada boot.

**O padrão observado antes da causa real aparecer:** mesmo com a exclusão do
Defender, a primeira tentativa de conectar logo após ligar o PC costumava
falhar (às vezes a segunda também), estabilizando só da 2ª/3ª em diante. Na
hora pareceu "bagunça de inicialização do Windows", mas era só o timing de um
bug mais específico - ver abaixo.

**A causa real (encontrada em 28/07/2026, via issues do próprio
`microsoft/WSL` no GitHub): faltava `instanceIdleTimeout`, que é DIFERENTE de
`vmIdleTimeout`.**

O WSL2 tem **dois** relógios de ociosidade independentes, e só tínhamos mexido
em um:

- `vmIdleTimeout` (seção `[wsl2]`) - desliga a **VM utilitária inteira**.
- `instanceIdleTimeout` (seção `[general]`) - desliga só a **distro**
  (Ubuntu-24.04), por baixo, mesmo com a VM de pé.

Zerar só o primeiro (`vmIdleTimeout=-1`) não bastava: a VM continuava viva, mas
a instância era derrubada por baixo do xrdp mesmo assim - exatamente o
`systemd-logind: The system will power off now!` que perseguiu essa
investigação inteira. É um problema documentado e ainda sem correção definitiva
do lado do WSL: [Issue #13291](https://github.com/microsoft/WSL/issues/13291)
("WSL Services (e.g., Ollama, SSHD) Are Being Suspended Despite
`vmIdleTimeout=-1`") e a
[Discussion #8659](https://github.com/microsoft/WSL/discussions/8659)
("`vmIdleTimeout` has no effect") - o mesmo padrão de outros serviços de fundo
(Ollama, SSHD) sendo derrubados mesmo com a VM configurada para nunca dormir.

**Correção:** os dois juntos no `.wslconfig`:

```ini
[general]
instanceIdleTimeout=-1

[wsl2]
vmIdleTimeout=-1
```

`wsl --shutdown` depois, sempre - nenhuma mudança de `.wslconfig` vale sem
isso. Ainda em teste (28/07/2026) - se voltar a fechar sozinho mesmo com os
dois setados, essa também não era a causa completa, e a pista da exclusão do
Defender (acima) continua valendo como mitigação.

### A conta, medida (28/07/2026)

Com só VS Code e um terminal abertos, o Gerenciador de Tarefas marcava ~8 GB.
De onde vem:

```
MemTotal      11,7 GB     teto do memory= vigente na hora da medição
used           2,6 GB     processos de verdade  <- é isto que o htop mostra
buff/cache     7,8 GB     page cache + buffers + slab reclaimável
MemAvailable   9,1 GB     o que o Linux entregaria na hora se pedissem
```

Os 7,8 GB são **cache de arquivos**, não vazamento. Todo arquivo lido ou escrito
(apt, git, npm, imagens Docker, a árvore do projeto no editor) fica em cache, e
o kernel só o descarta quando alguém precisa. Do lado do Linux está tudo certo:
`MemAvailable` de 9,1 GB diz que quase tudo é recuperável na hora.

**O problema é só na fronteira com o Windows.** O Linux devolve o cache para si
mesmo, mas a VM não devolve a página para o host: o `vmmemWSL` infla até o pico
e fica lá. Por isso o htop mostra 2,6 GB e o Gerenciador de Tarefas 8 GB — os
dois estão certos, medindo lados diferentes da mesma fronteira.

### Liberar RAM para o Windows (antes de um jogo, por exemplo)

| O que fazer | Devolve RAM ao Windows? |
|---|---|
| `wsl --shutdown` | **Sim, tudo, na hora.** É o único caminho confiável |
| Baixar `memory=` no `.wslconfig` | Sim, limita o pico — exige `wsl --shutdown` para valer |
| Fechar apps / parar serviços | Em parte: reduz o pico futuro, não encolhe o que já inflou |
| `drop_caches` | **Não.** Ver abaixo |
| `autoMemoryReclaim=gradual` | Sim, mas **não use** — ver acima |

O `drop_caches` é a armadilha, porque *parece* resolver:

```bash
sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
```

O `free` passa a mostrar buff/cache perto de zero e dá a sensação de vitória.
Mas o `vmmemWSL` no Windows **não encolhe** — a VM continua com as páginas
alocadas do host, agora vazias. Você jogou fora um cache útil e não ganhou um
megabyte no Windows. Sem `autoMemoryReclaim` (que está proibido nesta máquina),
não existe caminho de volta que não passe por desligar a VM.

Ou seja: para jogar, `wsl --shutdown` e pronto. Se isso incomodar, baixe o
`memory=` para um teto que o Windows possa perder sem sentir.

### O `memory=` é um teto rígido — não um valor inicial

Vale insistir porque é contraintuitivo: **o Linux não cresce além do
`memory=`**. Não existe "ele aumenta se precisar". O que se ajusta sozinho é a
divisão *dentro* do teto — o kernel descarta cache para atender um processo que
peça memória. A fatia se reorganiza; o bolo não cresce.

Estourar o teto não dá erro claro: primeiro vem swap (lento, e o arquivo fica no
disco do Windows), depois o *OOM killer*. E aqui isso é pior que o normal — se o
OOM escolher o `Xorg` ou o `xfwm4`, **a sessão RDP inteira cai**, pelo mesmo
encadeamento que faz `pkill -x xfwm4` encerrar tudo.

Por isso o teto tem que ficar acima do seu uso real de *processos*, com folga.
Nesta máquina (host de 16 GB) ficou em **8 GB**, escolhido assim:

```
2,6 GB   processos medidos com VS Code + terminal abertos
+ ~5 GB  folga para cache, kernel e picos
= 8 GB
```

`4GB` foi cogitado e **descartado**: deixaria ~1,4 GB de folga, e o piso de
2,6 GB já inclui VS Code, um language server Java, Xorg/xfwm4 e
Docker/containerd/Postgres. Uma segunda janela do editor ou um build estouraria.

Depois de mudar o `memory=`, é obrigatório `wsl --shutdown` — o `.wslconfig` só
é lido quando a VM sobe do zero.

Para ver quem está consumindo de verdade, por serviço:

```bash
for s in $(systemctl list-units --type=service --state=running --no-legend --plain | awk '{print $1}'); do
    m=$(systemctl show "$s" -p MemoryCurrent --value)
    case "$m" in ''|'[not set]'|18446744073709551615) continue;; esac
    LC_ALL=C awk -v m="$m" -v s="$s" 'BEGIN{printf "%10.1f MB  %s\n", m/1048576, s}'
done | sort -rn | head
```

> A versão anterior deste laço usava `printf … "$(echo "$m/1048576" | bc -l)"` e
> **estava quebrada num sistema em português**: o `bc` emite `117.7`, mas o
> `printf` sob `LC_NUMERIC=pt_BR.UTF-8` espera vírgula e recusa o valor com
> `invalid number` — imprimindo `117,0`, ou seja, truncando a casa decimal e
> ainda poluindo a saída de erros. O `awk` acima faz a divisão e a formatação de
> uma vez, e o `LC_ALL=C` garante o ponto decimal.

Nesta máquina o `ollama.service` sozinho segurava **928 MB** em idle, mais
2,1 GB de disco em `/usr/local/lib/ollama` (os runners de GPU), sem nenhum
modelo baixado. Foi removido. Vale rodar esse laço antes de culpar a WSL.

Lembre também que a WSL sobe vários serviços sem sentido numa VM — `bluetooth`,
`cups`, `ModemManager`, `nvidia-suspend`, `tlp`, `thermald`, `cloud-init`.
Somam pouco (~100 MB), mas atrasam o boot; `systemctl disable` neles é seguro.

## Um emulador de terminal escrito aqui (01/08/2026)

O `terminal.c` é um emulador de terminal completo em C + Xlib + Xft: 1.740
linhas, do pty ao pixel. Ele **não substitui nada**. A aba 0 da bancada
continua sendo o xterm com `-into`, que funciona e está resolvido; este é um
binário à parte, que abre a própria janela e roda o próprio shell. Existe por
gosto de ter o próprio.

Isso não é detalhe de gosto, é o que define o tamanho do trabalho. Não havendo
o compromisso de aguentar a TUI do Claude Code, o alvo passa a ser o que se
usa num terminal — shell, `ls`, `git`, `vim`, `htop`, `less` — e o parser cabe
num fim de semana em vez de num mês.

### O custo, medido

Os dois rodando `bash --norc -i` parado, numa janela 80×24, mesma fonte
(DejaVu Sans Mono 10), medido em 01/08/2026:

| | RSS | PSS | bibliotecas ligadas |
|---|---|---|---|
| `terminal` | 9,0 MB | **2,66 MB** | 20 |
| `xterm` | 16,0 MB | 8,12 MB | 31 |

Um terço do PSS do xterm. A diferença não é mérito de esperteza nenhuma: é o
xterm carregando Xaw, Xmu, Xt e Xpm — um toolkit inteiro dos anos 90 — para
desenhar retângulos e texto.

O maior gasto do programa é o histórico de rolagem, e é por escolha: cada
célula são 16 bytes, então as 1000 linhas padrão numa janela de 200 colunas
custam 1000 × 200 × 16 = 3,2 MB, que ele só paga conforme rola. `-sl 0`
desliga.

A reserva de fonte cobra à parte e só quando aparece o caractere: uma tela com
japonês subiu o PSS de 3,1 para 6,0 MB, porque a `NotoSansCJK-Regular.ttc`
entrou no processo. Sessão que nunca vê CJK nunca paga.

### O que o parser cobre

Medido pelo despejo da grade (ver abaixo), não por olhar a tela: CUP/CUU/CUD/
CUF/CUB/CHA/VPA, ED/EL, IL/DL, ICH/DCH/ECH, SU/SD, DECSTBM, IRM, DECAWM,
DECCKM, tela alternativa (`?1049`, `?47`, `?1047`), colagem marcada (`?2004`),
relato de mouse (`?1000/1002/1003` e SGR `?1006`), DSR, tabulações, ESC 7/8,
IND/NEL/RI, RIS, OSC 0/2 para o título, SGR inteiro — inclusive `38;5;n` e
`38;2;r;g;b` — e o conjunto gráfico DEC (`ESC ( 0`), sem o qual as molduras do
ncurses saem como `lqqqk`.

Rodando de verdade: `vim` com realce de sintaxe, `htop` com medidores e barra
de teclas, `less`, `ls --color`. O acento morto do ABNT2 compõe (`´`+`a` → `á`,
`~`+`a` → `ã`, `^`+`e` → `ê`), o que exige XIM com XIC e `XFilterEvent` antes
de qualquer tratamento — testado injetando teclas reais pelo XTEST.

### Três armadilhas que custaram tempo

**A reserva de fonte não pode partir de uma fonte já aberta.** O jeito óbvio de
achar uma fonte que tenha o caractere que falta é duplicar o pattern da fonte
atual e acrescentar um `FC_CHARSET` com ele. Não funciona, e não avisa: o
pattern de uma fonte **já aberta** é o *resolvido*, e traz `FC_FILE` e
`FC_FONTVERSION` dentro. Os dois são critérios de prioridade **maior** que o
charset dentro do fontconfig, então o `FcFontMatch` devolve a própria DejaVu
por mais que se peça um charset que ela não tem. O sintoma é japonês e emoji
saindo como caixa vazia *com as fontes Noto instaladas e funcionando* — o que
manda investigar o sistema, que está certo. A correção é guardar o pedido
**cru** (`FcNameParse("DejaVu Sans Mono:size=10")`) na abertura e partir dele.
Feito isso, o fontconfig ainda escolhe sozinho a variante monoespaçada:
`Noto Sans Mono CJK JP`.

**`setsid()` antes do `TIOCSCTTY`, sempre.** Sem virar líder de sessão primeiro,
o pty não vira o terminal de *controle* do filho. Tudo parece funcionar — o
shell roda, o texto aparece — e só o `Ctrl+C` não faz nada, porque não há
terminal de controle para gerar o SIGINT.

**Encolher a janela é onde mora o `free` duplo.** A primeira versão liberava as
linhas que sobravam depois de já ter movido o vetor de ponteiros, e liberava
de novo o intervalo antigo. E enquanto o vim está na tela alternativa, a tela
principal fica guardada **parada**: esquecer de reajustar a largura dela
também faz o estouro aparecer só na hora de fechar o vim, longe da causa.
As duas coisas foram achadas com `-fsanitize=address,undefined`, que passa
limpo por 60 linhas de rolagem, região de rolagem, cinco redimensionamentos
(inclusive 180×90 pixels) e entrada de teclado.

### Medir sem acreditar em screenshot

Um `SIGUSR1` despeja a grade inteira em texto — tamanho, cursor, tela ativa,
região de rolagem, modos, o conteúdo linha a linha, os trechos de atributo com
as cores em hexadecimal e o histórico:

```bash
TERMINAL_DESPEJO=/tmp/grade.txt ./terminal -e vim arquivo.c &
kill -USR1 $!
cat /tmp/grade.txt
```

Isso existe porque, como já está registrado neste README, `import -window` de
uma janela coberta devolve os pixels de quem está por cima — e neste projeto
já se perseguiu um bug que não existia por causa disso. O despejo é o estado
real; o print serve só para o que ele não alcança, que é o desenho.

### Compilar e usar

```bash
gcc -O2 -Wall -o terminal terminal.c -lX11 -lXft -lfontconfig $(pkg-config --cflags xft)
./terminal                       # o $SHELL, 80x24
./terminal -g 100x30 -fs 12      # geometria e corpo da fonte
./terminal -sl 5000 -e htop      # historico maior, comando direto
```

`-fa` troca a fonte, `-T` o título. Dentro: `Ctrl+Shift+C` / `Ctrl+Shift+V`
para a área de transferência (`Ctrl+C` continua sendo do programa que roda,
como tem de ser), seleção com o botão esquerdo — duplo clique pega a palavra,
triplo a linha —, botão do meio cola a seleção, roda do mouse e
`Shift+PgUp/PgDn` rolam o histórico.

### O que ele não tem, de propósito

Abas e divisão de painel, que é o que o tmux já faz melhor; transparência, que
pediria um compositor que esta sessão não tem; `utmp`, que traria a
libutempter só para o `who` enxergar a janela; e **reflow** — mudar a largura
recorta ou preenche as linhas, não as redobra. O xterm e o `st` também não
redobram: para redobrar seria preciso saber onde cada linha lógica termina, e
essa informação se perde no instante em que o texto entra pelo pty.

### Ctrl+Alt+T abre este terminal — e a sessão nasce sem nenhum (02/08/2026)

Duas mudanças que só fazem sentido juntas:

- saiu o `xfce4-terminal &` do `startwm.sh`. A sessão passou a nascer **sem
  janela nenhuma**: quem entra só para abrir a bancada, ou para jogar, não
  ganha mais um terminal para fechar. São ~9 MB de RSS e uma janela a menos por
  login;
- `Ctrl+Alt+T` deixou de ser `exo-open --launch TerminalEmulator` e passou a
  apontar direto para `/usr/local/bin/terminal` — o `terminal.c` deste
  repositório. A tecla continua sendo do `xfsettingsd`, no *namespace*
  `/commands`, então **não** há a migração de grab entre namespaces que já
  matou o `Super+→` (ver "A regra que faltava").

Com a sessão nascendo vazia, essa tecla virou o único caminho normal para um
shell — o que muda o peso de ela falhar. Por isso o `xfwm-atalhos.sh` **testa o
binário na hora de gravar** e cai no `exo-open` se ele não existir, em vez de
gravar um `sh -c "if [ -x ... ]"` como valor do atalho. O motivo é o executor:
o `xfsettingsd` passa a string pelo `g_shell_parse_argv` antes de rodar, então
aspas dentro do valor são mais uma camada de citação para errar — e o sintoma
de erro é a tecla **não fazer nada, calada**. Valor sem aspa nenhuma não tem
como quebrar. Se o binário aparecer depois, o `startwm.sh` roda o script a cada
login e a tecla se corrige sozinha.

O helper do exo (`~/.config/xfce4/helpers.rc`) continua apontando para o
`xfce4-terminal` de propósito: ele é quem atende o appfinder e qualquer app que
peça "abrir um terminal", e o exo só aceita helper **registrado** (um `.desktop`
em `share/xfce4/helpers`, com `X-XFCE-Binaries`). Registrar o `terminal.c` ali
seria mais um arquivo para manter por um caminho que quase não se usa aqui.

Nada disso muda a aba 0 da bancada, que segue no xterm com `-into` — a
invariante continua de pé. O `terminal.c` virou o terminal **da sessão**, não o
terminal **da bancada**.

## O grafo do projeto (graphify, 31/07/2026)

O código tem um grafo de conhecimento gerado pelo
[graphify](https://github.com/Graphify-Labs/graphify), em `graphify-out/` — que
está no `.gitignore`, porque é **derivado**, como o binário da barra. Regenerar
custa um comando e zero tokens.

```bash
pip3 install --user graphifyy      # o pacote tem dois "y"; o comando tem um so
graphify install --platform claude # registra a skill /graphify
graphify . --code-only             # constroi
graphify update .                  # apos mexer no codigo
```

Hoje: **121 nós, 205 arestas, 15 comunidades**, extraídos por AST local
(tree-sitter), sem chamada de API nenhuma.

### `--code-only` não é opcional

Sem ele, o graphify classifica o `README.md` como conteúdo semântico e o manda
para um LLM. Na primeira tentativa aqui ele detectou uma `GEMINI_API_KEY` no
ambiente e tentou justamente isso — só não gastou porque faltava um pacote.

É gasto sem retorno: **o valor deste README é a prosa curada**, as medições e as
correções datadas. Um resumo automático dela não acrescenta nada e custa tokens
toda vez.

### Para que ele serve, e para que não

Serve bem para navegar código que você não lembra:

```bash
graphify explain "aplicar_volume()"   # onde esta, quem chama, o que chama
graphify query "como o volume e aplicado"
firefox graphify-out/graph.html       # o mapa visual, comunidades nomeadas
```

**Não serve** para entender como os componentes conversam. O próprio relatório
diz *"all connections are within the same source files"*, e um
`graphify path "transferir-usb script" "audio-dispositivos script"` responde
**"No path found"** — mesmo que o primeiro execute o segundo. A razão é
estrutural: aqui os componentes se chamam **por nome de comando**, e AST não
resolve isso. O grafo enxerga 15 ilhas, uma por arquivo.

Por isso a cadeia real está desenhada à mão no `CLAUDE.md`. E por isso, com 15
arquivos, o `grep` continua achando qualquer coisa mais rápido — o grafo passa a
compensar quando houver muitos arquivos numa linguagem só.

### Nomear as comunidades não custa API

Elas nascem como "Community 0", "Community 1"… O passo que dá nome a elas é
feito pelo **assistente**, lendo `graphify-out/.graphify_analysis.json` — não por
uma API. Os nomes ficam em `graphify-out/.graphify_labels.json` e sobrevivem a um
`graphify update`. A flag `--no-label` pula esse passo.

### Duas armadilhas de instalação

- **`command not found` não é falta de conda.** O binário vai para
  `~/.local/bin`, que precisa estar no `PATH` — há uma linha para isso no
  `~/.bashrc`, e ela só vale em shell **novo** (`source ~/.bashrc` resolve no
  atual). O shebang aponta para o Python do miniconda por caminho absoluto,
  então `conda activate` é desnecessário.
- **O shebang amarra ao Python do miniconda.** Se um dia o miniconda sair ou
  mudar de versão, o comando quebra com `bad interpreter`. A correção é
  `pip3 install --user --force-reinstall graphifyy`, ou instalar com `uv`, que
  isola num ambiente próprio.

## Antes de formatar a máquina

O único diretório que precisa sobreviver é **este** (`~/linux-fullscreen`) —
ele contém os originais de tudo que o `install.sh` espalha pelo sistema. Copie a
pasta inteira para fora da WSL (`/mnt/c/...`, um pendrive, um repositório
Git) e a reinstalação é o `install.sh` de novo.

Não vale a pena salvar os arquivos instalados (`/etc/xrdp/startwm.sh`,
`/usr/local/bin/fix-x11-unix`, etc.): são cópias do que já está aqui. Se você
editou algum deles *no lugar* em vez de editar aqui, essa alteração se perde —
o hábito certo é editar nesta pasta e reinstalar.

Uma exceção que vale conhecer: as preferências do xfwm4 vivem em
`~/.config/xfce4/` e **não** estão neste repositório. Numa máquina nova valem os
padrões do XFCE, e o `xfwm-atalhos.sh` recria o que importa (os `Super+setas`,
o `Ctrl+Alt+T` e os ajustes de fluidez). Se você tiver ajustado tema, fontes ou
o comportamento do `Alt+Tab` à mão, copie `~/.config/xfce4/` junto.

O mesmo vale para o som: o `instalar-som.sh` refaz tudo — inclusive as máscaras
do PipeWire em `~/.config/systemd/user/`, que também não estão aqui.

E há `~/.config/linux-fullscreen/`. Só parte importa: o `monitores.conf` é
descartável (o `abrir-windows` repergunta uma vez por ambiente e refaz), e o
`dispositivos.conf` só existe se você tiver sobrescrito os VID:PID do
`transferir-usb` — sem ele valem os padrões do script. O `pasta-coisas.cache` é
puro cache (se apagar, o próximo `--listar-coisas` o refaz em cerca de um
segundo).

Desde 31/07/2026 o **`jogos.conf` deixou de ser insubstituível**: a lista mora na
pasta `Coisas` da área de trabalho do Windows, que já vai junto no backup do
Windows. Só copie o `jogos.conf` se você tiver linhas curadas à mão que não
viraram atalho.

Do lado Windows, o `usbipd bind` é persistente e **se perde na formatação**:
numa máquina nova é preciso reinstalar o `usbipd-win` e refazer os `bind` (veja
"Transferir áudio, microfone e câmera"). O `audio-evitado.txt` em
`%LOCALAPPDATA%\linux-fullscreen\` é descartável — o script o reescreve.

O `graphify` também se perde: é `pip3 install --user graphifyy` mais
`graphify install --platform claude` de novo (veja "O grafo do projeto"). O
`graphify-out/` **não** precisa ser salvo — reconstruir custa um comando e zero
tokens.

Lembre que a WSL inteira some ao formatar: `~`, pacotes apt, snaps, tudo.
`wsl --export Ubuntu-24.04 D:\ubuntu.tar` salva a distro completa, se preferir
não reconstruir do zero.

## Desinstalar

```bash
sudo systemctl disable --now xrdp xrdp-sesman x11-unix-writable
sudo cp /etc/xrdp/xrdp.ini.orig /etc/xrdp/xrdp.ini
sudo cp /etc/xrdp/startwm.sh.orig /etc/xrdp/startwm.sh
```

E, no Windows, devolver o WSLg em `C:\Users\<você>\.wslconfig`:

```ini
guiApplications=true
```

seguido de `wsl --shutdown`. Sem esse passo você fica sem os dois ambientes
gráficos.
