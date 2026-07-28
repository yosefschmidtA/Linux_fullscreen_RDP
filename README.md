# Linux fullscreen sobre WSL2 (xrdp + xfwm4)

Sessão Linux ocupando todos os monitores, cobrindo o Windows. Tela limpa: sem
barra de tarefas, sem relógio, sem ícones de área de trabalho — e sem desktop
environment.

As janelas se comportam como no GNOME ou no Windows: barra de título com os
botões de fechar/minimizar/maximizar, arrastar com o mouse, snap ao encostar na
borda, `Alt+Tab`, `Super+setas` para meia tela.

Isso vem do **xfwm4**, o gerenciador de janelas do XFCE, rodando sozinho — sem
`xfce4-session`, sem `xfdesktop`, sem painel. Só ele, o `xfsettingsd` (que
executa os atalhos de comando) e um terminal.

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
| `Ctrl+Alt+T` | novo terminal |
| `Alt+F3` ou `Super+R` | xfce4-appfinder (é assim que se abre programa) |
| `Alt+F4` | fecha a janela |
| `Alt+F10` | maximiza |
| `Alt+Tab` | alterna janelas (inclui as minimizadas) |
| `Super+←` / `Super+→` | meia tela na lateral |
| `Super+↑` | maximiza (de novo, restaura) |
| `Super+↓` | minimiza |
| `Super+Shift+←` / `Super+Shift+→` | manda a janela para o monitor vizinho |
| `Super+Shift+↑` / `Super+Shift+↓` | idem, para monitor acima / abaixo |
| `Super+D` | mostra a área de trabalho |
| `Super+KP_*` | tiling pelo teclado numérico (padrão do XFCE, mantido) |
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

## Desligar (e a interface)

Fechar a janela do mstsc **não encerra nada** — nem a sessão, nem a VM. Sem um
botão, desligar era fechar o RDP e depois ir ao PowerShell rodar
`wsl --shutdown`. O `linux-desktop-down` resolve isso de dentro:

| Como chamar | O que faz |
|---|---|
| `Alt+F3` → "Desligar" | encerra a sessão **e** desliga a VM (devolve a RAM ao Windows) |
| `Alt+F3` → "Sair da sessão" | encerra só a sessão gráfica; a VM continua, e o atalho do Windows reabre em segundos |
| `linux-desktop-down -y` | o mesmo, sem perguntar (para script) |

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

Sem painel, quem lista aplicativos é o `xfce4-appfinder` (`Alt+F3` ou
`Super+R`). Então **um botão novo é um `.desktop` novo** em `desktop/`, que o
`install.sh` copia para `/usr/share/applications/`. Não é preciso subir um
desktop environment para ter menu — é o que permite manter a premissa de RAM
baixa deste projeto.

Para os diálogos, a sessão tem `zenity` (GTK, bonitinho) e `xmessage` (feio,
mas quase sem dependência). O `linux-desktop-down` usa o primeiro que achar e
cai para o terminal se não houver tela — vale como modelo para os próximos.

## Arquivos instalados

| Origem | Destino |
|---|---|
| `startwm.sh` | `/etc/xrdp/startwm.sh` |
| `fix-x11-unix` | `/usr/local/bin/fix-x11-unix` |
| `linux-desktop-up` | `/usr/local/bin/linux-desktop-up` |
| `linux-desktop-down` | `/usr/local/bin/linux-desktop-down` |
| `desktop/*.desktop` | `/usr/share/applications/` (itens do appfinder) |
| `x11-unix-writable.service` | `/etc/systemd/system/` |
| `i3.config` | `~/.config/i3/config` (só serve se voltar ao i3) |

Dois scripts **não** são instalados — rodam de dentro da sessão, quando você
quiser:

| Script | Para quê |
|---|---|
| `xfwm-atalhos.sh` | põe os `Super+setas` e conserta o `Ctrl+Alt+T`. Rodar uma vez após instalar. |
| `diag-super-direita.sh` | descobre se um atalho com a tecla Windows está sendo engolido pelo Windows antes de chegar na sessão. |

Há ainda uma alteração do lado Windows, feita pelo passo 7 do `install.sh`:
`guiApplications=false` em `C:\Users\<você>\.wslconfig` (veja acima). É o único
arquivo que o instalador escreve fora da WSL, e o motivo do `wsl --shutdown`
logo depois. Se o interop com o Windows estiver desligado, ele avisa e segue —
aí é à mão.

xrdp roda na porta **3390** (3389 é do RDP nativo do Windows).
Os originais viram `.orig` ao lado (`/etc/xrdp/xrdp.ini.orig`, etc.).

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

**Sobre a "corrida na largada".** A versão anterior deste README suspeitava do
`setxkbmap` trocando o mapa do teclado enquanto o xfwm4 registrava os grabs.
Isso continua **não provado**, e agora se sabe que boa parte do sintoma era o
grab órfão descrito acima — que sobrevive a qualquer reaplicação de atalho e
por isso imitava perfeitamente uma falha de arranque.

Um agravante real foi removido: o `~/.bashrc` rodava `setxkbmap` **a cada shell
novo**, não só no arranque. Cada terminal aberto trocava o mapa de teclado da
sessão inteira, e toda troca de mapa invalida os grabs já registrados. A linha
está comentada — quem aplica o layout é o `startwm.sh`, uma vez por sessão, e o
xrdp já carrega `br(abnt2)` a cada conexão (vindo do `keylayout 0x00000416` que
o mstsc envia).

**Placar de sessões novas** (para decidir quando remover o contorno):

| Data | `Super+→` no arranque | Observação |
|---|---|---|
| 28/07/2026 | **funcionou** | 1ª sessão após corrigir o grab órfão e o `.bashrc`; nenhuma intervenção manual |

O `startwm.sh` ainda reaplica os atalhos 4 segundos depois que a sessão sobe.
Mantido por ora: com o `.bashrc` corrigido, é possível que não seja mais
necessário, mas uma sessão só não decide — o bug antigo era **intermitente**, e
é exatamente assim que um contorno desnecessário se disfarça de indispensável.
Anote as próximas aqui; com uma sequência de acertos, apague o bloco marcado
como CONTORNO no `startwm.sh` e confirme que continua nascendo funcionando.
Se ficar comprovado que o `Super+→` nasce funcionando, aquele bloco é o
primeiro a sair. O efeito colateral a conhecer é que qualquer atalho que você
mudar à mão nesses quatro (`Super+setas`) é sobrescrito a cada login — para
mudar de verdade, edite o `xfwm-atalhos.sh`.

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
padrões do XFCE, e o `xfwm-atalhos.sh` recria o que importa (os `Super+setas` e
o `Ctrl+Alt+T`). Se você tiver ajustado tema, fontes ou o comportamento do
`Alt+Tab` à mão, copie `~/.config/xfce4/` junto.

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
