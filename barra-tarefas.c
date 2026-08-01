/* Barra de tarefas arcaica para a sessao fullscreen, no visual do dialogo de
 * login do xrdp (veja Untitled.png).
 *
 *   gcc -O2 -Wall -o barra-tarefas barra-tarefas.c -lX11 -lXrandr
 *   DISPLAY=:10 ./barra-tarefas &
 *
 * POR QUE Xlib CRU, E NAO UM TOOLKIT
 *
 * A primeira versao era Python + Tk e pesava 21 MB de RSS. O visual exige
 * desenhar cada pixel a mao - o "relief raised" do Tk calcula os tons sozinho
 * e nao bate com o que foi amostrado do PNG -, entao aquela versao ja usava um
 * Canvas e ignorava os widgets. Estavamos pagando um toolkit inteiro para nao
 * usar nenhum widget dele. Aqui as mesmas primitivas (linha, retangulo, texto)
 * vem direto do servidor X.
 *
 * A FONTE
 *
 * Fonte CORE do X (-adobe-helvetica-*-11), nao TrueType. O proprio dialogo do
 * xrdp e desenhado com fonte core, entao o texto fica identico e nao apenas
 * parecido - bitmap, sem antialiasing nenhum. Rotulos em ASCII: a fonte esta em
 * iso8859-1 e este arquivo em UTF-8, entao um rotulo acentuado sairia errado no
 * XDrawString. Se precisar de acento, converta para latin-1 antes.
 *
 * O BISEL
 *
 * Motif, assimetrico, amostrado pixel a pixel do Untitled.png:
 *   topo/esquerda  1px #FFFFFF
 *   baixo/direita  1px #000000 por fora, 1px #808080 por dentro
 * Sao cinco cores no total e nenhum meio-tom. Nao acrescente tom novo aqui: o
 * ar arcaico vem justamente de nao haver gradiente nem antialiasing.
 *
 * PARA ACRESCENTAR UM ITEM
 *
 * Escreva a funcao de acao e ponha uma linha em "fixos_antes" ou "fixos_depois".
 * A largura da barra e calculada a partir delas; nada mais precisa mudar.
 *
 * Entre as duas tabelas entram, em tempo de execucao, os ATALHOS DE APLICATIVO
 * que o barra-apps reporta - por isso a tabela e dupla. Esses sao os unicos
 * itens com icone, e o unico lugar do arquivo onde a barra desenha pixels que
 * nao calculou ela mesma; veja "atalhos de aplicativo" mais abaixo para o porque
 * de isso nao ter trazido libpng junto.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>

#define ALTURA 28
#define PAD     2          /* respiro entre a moldura da barra e os itens */
#define GAP     4          /* espaco entre itens */

/* Onde a barra fica no monitor primario. Troque para mudar o alinhamento:
 * CENTRO, ESQUERDA ou DIREITA. */
#define ALINHAMENTO CENTRO
enum { ESQUERDA, CENTRO, DIREITA };
#define MARGEM_LATERAL 8   /* usado so quando o alinhamento nao e CENTRO */

static Display     *dpy;
static Window       win;
static GC           gc;
static XFontStruct *fonte, *mono;
static unsigned long FACE, HI, SH, INK;
static int          LARG;
static time_t       mt_ref;      /* mtime do cache no instante do clique */
static int          cam_ref;     /* estado da ponte de video no clique */

enum { BOTAO, RELOGIO, BOTAO_USB, VOLUME, BOTAO_JOGOS, BOTAO_APP, BOTAO_MAIS };

/* Os quatro ultimos campos so existem para BOTAO_APP, que nasce em tempo de
 * execucao e por isso nao pode apontar para literal de string como os fixos.
 * Ficam no fim para que os inicializadores das tabelas fixas continuem valendo
 * sem mudanca (o C zera o que sobra). */
typedef struct {
    int         tipo;
    const char *rotulo;
    int         larg;
    void      (*acao)(void);
    const char *dev;        /* so BOTAO_USB: "audio" ou "camera" */
    int         pendente;   /* clicado, esperando o transferir-usb responder */
    int         x;          /* preenchido pelo layout */
    char        id[288];    /* BOTAO_APP: caminho do .desktop */
    char        txt[64];    /* BOTAO_APP: nome, plano B quando nao ha icone */
    Pixmap      icone;      /* BOTAO_APP: 0 = desenhar o nome */
    int         ic_lado;
} Item;

static void acao_desligar(void);
static void tick(void);
static void aplicar_volume(Item *it, int mx);
static void desenhar(void);
static void levantado(int x, int y, int w, int h);
static void gravado(int x, int y, int w, int h);
static void primario(int *px, int *py, int *pw, int *ph);
static void anunciar_dock(void);
static void aplicar_strut(int bx, int my, int mh);
static void fechar_popup(void);
static void escolher_popup(int i);
static void desenhar_popup(void);
static void abrir_popup(Item *it);
static void montar_itens(void);
static void recarregar_apps(void);
static void dicas_de_tamanho(void);
static void reposicionar(void);
static time_t apps_mtime(void);
static Pixmap carregar_icone(const char *caminho, int lado);
static time_t apps_ref;        /* mtime do apps.conf na ultima montagem */
static int  arrastando = -1;   /* item de volume sob arrasto, ou -1 */

/* Partes clicaveis de um controle de volume, da esquerda para a direita:
 *   [rotulo]  alterna o mudo
 *   [calha]   ajusta o nivel
 *   [seta]    abre o menu de dispositivos
 */
#define VOL_LARG   116
#define VOL_ROTULO  26
#define VOL_SETA    12
#define VOL_CALHA  (VOL_LARG - VOL_ROTULO - GAP - VOL_SETA - 2)

/* A barra, da esquerda para a direita. Sem lista de janelas de proposito - o
 * caminho de volta para janela minimizada continua sendo o Alt+Tab
 * (cycle_hidden=true no xfwm4.xml).
 *
 * Os dois BOTAO_USB sao separados de proposito: bundle-los obrigaria a
 * tudo-ou-nada.
 *
 * ATENCAO - os dois botoes NAO fazem mais a mesma coisa por baixo (31/07/2026):
 *
 *   "Audio"  -> transferir-usb: move mesmo o headset por USB/IP.
 *   "Camera" -> camera-rede: liga/desliga uma ponte de VIDEO POR REDE, e o
 *               dispositivo USB nunca sai do Windows.
 *
 * O motivo esta medido no README: por USB/IP o navegador pede YUYV 640x480 a
 * 30 fps (18,4 MB/s) e o vhci_hcd satura em 0,25 MB/s - dava chuvisco. O audio
 * cabe nesse mesmo teto (0,18 MB/s) e por isso continua no USB/IP.
 *
 * O rotulo da camera diz "Rede" e nao "Linux" justamente para nao sugerir que o
 * dispositivo mudou de lado. Ele nao muda: enquanto a ponte esta ligada, o
 * ffmpeg.exe segura a camera no Windows e nenhum outro app de la a abre.
 * A largura cabe o rotulo mais longo ("Camera: Rede"). */
/* A tabela virou DUAS, com os atalhos de aplicativo montados no meio em tempo
 * de execucao (veja carregar_apps). O "[+]" fecha o grupo dos lancadores: e o
 * botao que abre a lista de apps instalados para fixar ou tirar.
 *
 * Continua nao havendo lista de janelas, e isso e deliberado: estes botoes
 * LANCAM, nao alternam. Uma janela minimizada volta pelo Alt+Tab, como sempre
 * (cycle_hidden=true no xfwm4.xml). */
static Item fixos_antes[] = {
    { VOLUME,      "Vol",     VOL_LARG, NULL,       "sink",   0, 0 },
    { VOLUME,      "Mic",     VOL_LARG, NULL,       "source", 0, 0 },
    { BOTAO_USB,   "Audio",    92, NULL,          "audio",  0, 0 },
    { BOTAO_USB,   "Camera",   92, NULL,          "camera", 0, 0 },
    { BOTAO_JOGOS, "Jogos",    52, NULL,          NULL,     0, 0 },
};
static Item fixos_depois[] = {
    { BOTAO_MAIS,  "+",        20, NULL,          NULL,     0, 0 },
    { RELOGIO,     NULL,       52, NULL,          NULL,     0, 0 },
    { BOTAO,       "Desligar", 66, acao_desligar, NULL,     0, 0 },
};

#define N_ANTES  ((int) (sizeof fixos_antes  / sizeof fixos_antes[0]))
#define N_DEPOIS ((int) (sizeof fixos_depois / sizeof fixos_depois[0]))
#define MAX_APPS 14
#define APP_LADO 20                        /* casa com o LADO do barra-apps */
#define APP_LARG (APP_LADO + 8)

static Item itens[N_ANTES + MAX_APPS + N_DEPOIS];
static int  n_itens;

/* ---- menu suspenso -------------------------------------------------------
 * Serve a dois botoes, e a mecanica e a mesma nos dois: um comando externo
 * imprime as linhas, o menu mostra e o clique devolve a escolha ao mesmo
 * comando. A barra nao sabe nada sobre audio nem sobre jogos.
 *
 * DISPOSITIVOS DE AUDIO. O Linux so tem dois dispositivos de cada lado (xrdp e,
 * quando anexada, a placa USB). Um menu feito so com o pactl mostraria
 * "xrdp-sink", que nao significa nada, e esconderia a caixa do notebook e o
 * monitor - que sao do Windows e so existem atras do canal RDP. Por isso a
 * lista vem do audio-dispositivos, que junta os dois mundos.
 *
 * JOGOS. A lista vem do jogo-windows --listar-jogos, que le uma PASTA de
 * atalhos na area de trabalho do Windows. Por que ler no clique, e nao guardar:
 * assim um atalho novo aparece sem reiniciar a barra - que e o ponto todo de
 * usar uma pasta - e nao ha nada a vigiar enquanto o menu esta fechado. Custa
 * 48 ms (medido em 31/07/2026, com o cache do caminho ja quente).
 *
 * O FORMATO E UM SO, e por isso os dois cabem no mesmo parser:
 *
 *     <id>\t<em uso: 0 ou 1>\t<rotulo ASCII>
 *
 * O id nunca e desenhado - e o que volta para o comando -, e por isso ele pode
 * ter acento enquanto o rotulo, que passa pelo XDrawString, nao pode.
 */
#define MAX_POP 24        /* teto de linhas do menu; 24 jogos e folgado */
#define POP_LINHA 18

/* Onde as primitivas de desenho pintam. Existe porque levantado/gravado/linha/
 * texto tinham a janela da BARRA chumbada: a moldura e o realce do menu eram
 * pintados sobre a barra, nas coordenadas do menu - aparecia um retangulo
 * fantasma algumas linhas abaixo do ponteiro, e o realce de verdade nunca
 * saia. Quem desenha ajusta este alvo antes. */
static Drawable alvo;

static Window pop = 0;              /* 0 = fechado */
static int    pop_n, pop_sob = -1;  /* itens; linha sob o mouse */
static int    pop_w, pop_h;
static char   pop_id[MAX_POP][288];   /* cabe o caminho de um .desktop */
static char   pop_rot[MAX_POP][120];
static int    pop_atual[MAX_POP];
static char   pop_lado[8];          /* so o menu de audio: "saida"/"entrada" */
static int    pop_tipo;             /* qual botao abriu: decide o que o clique faz */

/* Estado lido do cache que o transferir-usb escreve. A barra NAO chama o
 * usbipd.exe: uma chamada de interop leva quase um segundo e travaria o
 * desenho. */
static char est_audio[24]  = "?";
/* Nao ha est_camera: desde 31/07/2026 a camera nao passa pelo transferir-usb.
 * O estado dela e a ponte de rede estar viva, lido em camera_ligada(). */

/* Volume, 0-100, e mudo. Aqui, ao contrario do estado USB, NAO ha cache: o
 * pactl responde em 7-9 ms (medido em 31/07/2026) porque fala por socket unix
 * local, entao da para perguntar a cada tick sem travar o desenho. E o interop
 * do Windows que e caro, nao um processo local.
 *
 * Sempre @DEFAULT_SINK@ / @DEFAULT_SOURCE@, nunca um nome fixo: assim o mesmo
 * controle serve para o headset USB nativo e para o xrdp-sink, e continua certo
 * depois de um clique em "Audio: Win". */
static int vol_sink = -1, vol_source = -1;
static int mudo_sink = 0, mudo_source = 0;

/* Le a primeira porcentagem da saida do pactl. Retorna -1 se nao houver. */
static int pactl_num(const char *cmd)
{
    FILE *f = popen(cmd, "r");
    char buf[512], *p;
    int v = -1;

    if (!f)
        return -1;
    if (fgets(buf, sizeof buf, f)) {
        p = strchr(buf, '%');
        if (p) {
            while (p > buf && (p[-1] == ' ' || (p[-1] >= '0' && p[-1] <= '9')))
                p--;
            v = atoi(p);
        }
    }
    pclose(f);
    return v;
}

static int pactl_mudo(const char *cmd)
{
    FILE *f = popen(cmd, "r");
    char buf[128];
    int m = 0;

    if (!f)
        return 0;
    if (fgets(buf, sizeof buf, f))
        m = strstr(buf, "yes") != NULL;
    pclose(f);
    return m;
}

static void ler_volumes(void)
{
    vol_sink   = pactl_num("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null");
    vol_source = pactl_num("pactl get-source-volume @DEFAULT_SOURCE@ 2>/dev/null");
    mudo_sink   = pactl_mudo("pactl get-sink-mute @DEFAULT_SINK@ 2>/dev/null");
    mudo_source = pactl_mudo("pactl get-source-mute @DEFAULT_SOURCE@ 2>/dev/null");
}

static void ler_cache(void)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    char caminho[256], linha[128];
    FILE *f;

    snprintf(caminho, sizeof caminho, "%s/transferir-usb.estado",
             rt && *rt ? rt : "/run/user/1000");
    f = fopen(caminho, "r");
    if (!f)
        return;
    while (fgets(linha, sizeof linha, f)) {
        char *nl = strchr(linha, '\n');
        if (nl) *nl = '\0';
        if (!strncmp(linha, "audio=", 6))
            snprintf(est_audio, sizeof est_audio, "%.20s", linha + 6);
    }
    fclose(f);
}

/* A ponte de video esta de pe? Le o pidfile que o camera-rede escreve e
 * confere se o processo vive. E so um open + kill(0), sem fork: chamar
 * "camera-rede status" a cada tick custaria um shell por segundo. */
static int camera_ligada(void)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    char caminho[256];
    FILE *f;
    int pid = 0;

    snprintf(caminho, sizeof caminho, "%s/camera-rede-local.pid",
             rt && *rt ? rt : "/run/user/1000");
    f = fopen(caminho, "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    return pid > 0 && kill(pid, 0) == 0;
}

/* "Audio: Linux" quando esta aqui, "Audio: Win" quando esta no Windows,
 * "Audio: --" quando falta o 'usbipd bind' ou o aparelho esta desligado.
 *
 * A camera nao usa esse vocabulario: ela nunca "esta aqui". "Camera: Rede"
 * significa que a ponte esta transmitindo; "Camera: Win", que esta desligada e
 * o Windows tem a camera livre. */
static void rotulo_usb(const Item *it, char *fora, size_t n)
{
    const char *lado;

    if (it->pendente) {
        lado = "...";
    } else if (!strcmp(it->dev, "camera")) {
        lado = camera_ligada() ? "Rede" : "Win";
    } else {
        if (!strcmp(est_audio, "linux"))        lado = "Linux";
        else if (!strcmp(est_audio, "windows")) lado = "Win";
        else                                    lado = "--";
    }

    snprintf(fora, n, "%s: %s", it->rotulo, lado);
}

/* ---- utilidades ------------------------------------------------------- */

static unsigned long cor(const char *spec)
{
    Colormap cm = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor c;

    if (!XParseColor(dpy, cm, spec, &c) || !XAllocColor(dpy, cm, &c)) {
        fprintf(stderr, "barra: nao alocou a cor %s\n", spec);
        exit(1);
    }
    return c.pixel;
}

/* Roda um comando desacoplado: a barra pode morrer sem levar o filho junto. */
static void solta(const char *cmd)
{
    if (fork() == 0) {
        setsid();
        execlp("sh", "sh", "-c", cmd, (char *) NULL);
        _exit(127);
    }
}

/* Cita um texto para o shell, com a regra do apostrofo: fecha a aspa, escapa,
 * reabre. Existe porque o nome do jogo e um NOME DE ARQUIVO escolhido por voce -
 * "Assassin's Creed.lnk" e um nome perfeitamente normal, e sem isto o comando
 * sairia partido no meio. */
static void cita(char *fora, size_t n, const char *s)
{
    size_t o = 0;

    if (n < 3) { if (n) fora[0] = '\0'; return; }
    fora[o++] = '\'';
    for (; *s && o + 5 < n; s++) {
        if (*s == '\'') { memcpy(fora + o, "'\\''", 4); o += 4; }
        else            { fora[o++] = *s; }
    }
    fora[o++] = '\'';
    fora[o]   = '\0';
}

static void acao_desligar(void)
{
    /* o proprio linux-desktop-down pergunta antes, via zenity */
    solta("linux-desktop-down");
}

/* Clique ou arrasto num controle de volume. No rotulo alterna o mudo; na calha
 * ajusta o nivel.
 *
 * O valor local e atualizado na hora e o pactl e chamado solto: assim o cursor
 * acompanha o mouse sem esperar processo nenhum. O tick corrige depois se algo
 * mudou o volume por fora. */
static void aplicar_volume(Item *it, int mx)
{
    int e_sink = !strcmp(it->dev, "sink");
    int calha_x = it->x + VOL_ROTULO + GAP;
    int util = VOL_CALHA - 8;
    char cmd[160];
    int v;

    if (mx >= it->x + it->larg - VOL_SETA) {  /* seta: menu de dispositivos */
        abrir_popup(it);
        return;
    }

    if (mx < it->x + VOL_ROTULO) {          /* rotulo: alterna o mudo */
        int novo = e_sink ? !mudo_sink : !mudo_source;
        if (e_sink) mudo_sink = novo; else mudo_source = novo;
        snprintf(cmd, sizeof cmd, "pactl set-%s-mute @DEFAULT_%s@ %d",
                 e_sink ? "sink" : "source", e_sink ? "SINK" : "SOURCE", novo);
        solta(cmd);
        desenhar();
        return;
    }

    v = ((mx - calha_x - 4) * 100 + util / 2) / (util > 0 ? util : 1);
    if (v < 0)   v = 0;
    if (v > 100) v = 100;

    if (e_sink) vol_sink = v; else vol_source = v;
    snprintf(cmd, sizeof cmd, "pactl set-%s-volume @DEFAULT_%s@ %d%%",
             e_sink ? "sink" : "source", e_sink ? "SINK" : "SOURCE", v);
    solta(cmd);
    desenhar();
}

/* ---- o menu de dispositivos --------------------------------------------- */

static void desenhar_popup(void)
{
    int i;

    if (!pop)
        return;
    alvo = pop;
    levantado(0, 0, pop_w, pop_h);

    for (i = 0; i < pop_n; i++) {
        int y = 2 + i * POP_LINHA;
        int w = XTextWidth(fonte, pop_rot[i], strlen(pop_rot[i]));

        if (i == pop_sob)
            gravado(2, y, pop_w - 4, POP_LINHA);

        /* O que esta em uso leva um marcador de texto, nao cor nova: a paleta
         * tem cinco cores e nenhuma sobra para "estado". */
        XSetFont(dpy, gc, fonte->fid);
        XSetForeground(dpy, gc, INK);
        XDrawString(dpy, alvo, gc, 8, y + POP_LINHA / 2 + 4,
                    pop_atual[i] ? ">" : " ", 1);
        XDrawString(dpy, alvo, gc, 20, y + POP_LINHA / 2 + 4,
                    pop_rot[i], strlen(pop_rot[i]));
        (void) w;
    }
    XFlush(dpy);
}

static void fechar_popup(void)
{
    if (!pop)
        return;
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, pop);
    pop = 0;
    pop_sob = -1;
}

static void abrir_popup(Item *it)
{
    char cmd[160], linha[512];
    XSetWindowAttributes at;
    FILE *f;
    int larg = 120, bx, by, mx, my, mw, mh;

    fechar_popup();
    pop_tipo = it->tipo;

    /* Menu do botao direito num atalho de aplicativo. E o unico menu que a barra
     * monta sozinha, sem perguntar a ninguem: e uma linha so.
     *
     * POR QUE UM MENU, E NAO REMOVER DIRETO. A barra mora na borda de baixo da
     * tela, que e onde o ponteiro passa o tempo todo; um botao direito perdido
     * apagaria um atalho sem aviso. Com o menu e preciso clicar de novo, e
     * clicar fora cancela - a mesma mecanica dos outros dois menus, entao nao ha
     * gesto novo para aprender. */
    if (it->tipo == BOTAO_APP) {
        snprintf(pop_id[0],  sizeof pop_id[0],  "%s", it->id);
        snprintf(pop_rot[0], sizeof pop_rot[0], "Tirar %.90s da barra", it->txt);
        pop_atual[0] = 0;
        pop_n = 1;
        larg = XTextWidth(fonte, pop_rot[0], strlen(pop_rot[0])) + 32;
        goto montar;
    }

    if (it->tipo == BOTAO_JOGOS) {
        snprintf(cmd, sizeof cmd, "jogo-windows --listar-jogos 2>/dev/null");
    } else {
        snprintf(pop_lado, sizeof pop_lado, "%s",
                 !strcmp(it->dev, "sink") ? "saida" : "entrada");
        snprintf(cmd, sizeof cmd, "audio-dispositivos listar %s 2>/dev/null",
                 pop_lado);
    }

    pop_n = 0;
    f = popen(cmd, "r");
    if (!f)
        return;
    while (pop_n < MAX_POP && fgets(linha, sizeof linha, f)) {
        char *t1 = strchr(linha, '\t'), *t2;
        int w;

        if (!t1) continue;
        *t1 = '\0';
        t2 = strchr(t1 + 1, '\t');
        if (!t2) continue;
        *t2 = '\0';
        { char *nl = strchr(t2 + 1, '\n'); if (nl) *nl = '\0'; }

        snprintf(pop_id[pop_n],  sizeof pop_id[0],  "%.286s", linha);
        snprintf(pop_rot[pop_n], sizeof pop_rot[0], "%.118s", t2 + 1);
        pop_atual[pop_n] = atoi(t1 + 1);

        w = XTextWidth(fonte, pop_rot[pop_n], strlen(pop_rot[pop_n])) + 32;
        if (w > larg) larg = w;
        pop_n++;
    }
    pclose(f);

    /* Pasta vazia (ou nao encontrada) abriria um menu de zero linha, e o clique
     * pareceria um botao morto. Uma linha de aviso, com id vazio para o
     * escolher_popup a ignorar, diz o que fazer. */
    if (pop_n == 0 && it->tipo == BOTAO_JOGOS) {
        pop_id[0][0] = '\0';
        snprintf(pop_rot[0], sizeof pop_rot[0],
                 "ponha um atalho na pasta Jogos");
        pop_atual[0] = 0;
        larg = XTextWidth(fonte, pop_rot[0], strlen(pop_rot[0])) + 32;
        pop_n = 1;
    }
    if (pop_n == 0)
        return;

montar:
    pop_w = larg;
    pop_h = pop_n * POP_LINHA + 4;

    /* Abre ACIMA da barra: ela vive na base da tela, entao para baixo nao ha
     * espaco. E prende no monitor para o menu nao vazar pela lateral. */
    primario(&mx, &my, &mw, &mh);
    bx = itens[0].x;                       /* posicao da barra na tela */
    bx = (LARG >= mw) ? mx : mx + (mw - LARG) / 2;
    bx += it->x;
    if (bx + pop_w > mx + mw) bx = mx + mw - pop_w;
    if (bx < mx) bx = mx;
    by = my + mh - ALTURA - pop_h;

    at.override_redirect = True;
    at.background_pixel  = FACE;
    at.event_mask        = ExposureMask | ButtonPressMask | PointerMotionMask;
    pop = XCreateWindow(dpy, DefaultRootWindow(dpy), bx, by, pop_w, pop_h, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &at);
    XMapRaised(dpy, pop);

    /* owner_events False: assim TODO clique vem para ca, inclusive fora do
     * menu - e e assim que ele fecha ao clicar em qualquer outro lugar. */
    XGrabPointer(dpy, pop, False,
                 ButtonPressMask | PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    desenhar_popup();
}

static void escolher_popup(int i)
{
    char cmd[512], q[400];

    if (i >= 0 && i < pop_n && pop_id[i][0]) {
        cita(q, sizeof q, pop_id[i]);
        if (pop_tipo == BOTAO_APP)
            /* o apps.conf muda; o tick ve o mtime e remonta a barra */
            snprintf(cmd, sizeof cmd, "barra-apps remover %s", q);
        else if (pop_tipo == BOTAO_JOGOS)
            /* O jogo-windows faz o resto sozinho: pergunta o monitor, encolhe a
             * sessao, lanca e devolve o multimonitor quando voce fechar. A barra
             * so passa o nome. Solto, porque ele vive enquanto durar o jogo. */
            snprintf(cmd, sizeof cmd, "jogo-windows %s", q);
        else
            snprintf(cmd, sizeof cmd, "audio-dispositivos usar %s %s",
                     pop_lado, q);
        solta(cmd);
    }
    fechar_popup();
}

/* mtime do cache: e assim que sabemos que o transferir-usb terminou, sem
 * precisar esperar por ele (o attach leva segundos e travaria a barra) */
static time_t cache_mtime(void)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    char caminho[256];
    struct stat st;

    snprintf(caminho, sizeof caminho, "%s/transferir-usb.estado",
             rt && *rt ? rt : "/run/user/1000");
    return stat(caminho, &st) == 0 ? st.st_mtime : 0;
}

/* ---- desenho ---------------------------------------------------------- */

static void linha(int x0, int y0, int x1, int y1, unsigned long c)
{
    XSetForeground(dpy, gc, c);
    XDrawLine(dpy, alvo, gc, x0, y0, x1, y1);
}

static void levantado(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y1 = y + h - 1;

    XSetForeground(dpy, gc, FACE);
    XFillRectangle(dpy, alvo, gc, x, y, w, h);
    linha(x, y, x1, y, HI);                      /* topo */
    linha(x, y, x, y1, HI);                      /* esquerda */
    linha(x, y1, x1, y1, INK);                   /* baixo, externo */
    linha(x1, y, x1, y1, INK);                   /* direita, externo */
    linha(x + 1, y1 - 1, x1 - 1, y1 - 1, SH);    /* baixo, interno */
    linha(x1 - 1, y + 1, x1 - 1, y1 - 1, SH);    /* direita, interno */
}

static void gravado(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y1 = y + h - 1;

    XSetForeground(dpy, gc, HI);
    XFillRectangle(dpy, alvo, gc, x, y, w, h);
    linha(x, y, x1, y, SH);
    linha(x, y, x, y1, SH);
    linha(x + 1, y + 1, x1 - 1, y + 1, INK);
    linha(x + 1, y + 1, x + 1, y1 - 1, INK);
    linha(x, y1, x1, y1, HI);
    linha(x1, y, x1, y1, HI);
}

static void texto(int cx, int cy, const char *s, XFontStruct *f)
{
    int w = XTextWidth(f, s, strlen(s));

    XSetFont(dpy, gc, f->fid);
    XSetForeground(dpy, gc, INK);
    XDrawString(dpy, alvo, gc, cx - w / 2,
                cy + (f->ascent - f->descent) / 2, s, strlen(s));
}

static void desenhar(void)
{
    alvo = win;
    char hm[8];
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    int i;

    /* a barra flutua: moldura levantada nos quatro lados */
    levantado(0, 0, LARG, ALTURA);

    strftime(hm, sizeof hm, "%H:%M", t);

    for (i = 0; i < n_itens; i++) {
        Item *it = &itens[i];
        int cx = it->x + it->larg / 2;

        if (it->tipo == RELOGIO) {
            gravado(it->x, PAD + 1, it->larg, ALTURA - 2 * (PAD + 1));
            texto(cx, ALTURA / 2, hm, mono);
        } else if (it->tipo == BOTAO_USB) {
            char r[48];
            rotulo_usb(it, r, sizeof r);
            levantado(it->x, PAD, it->larg, ALTURA - 2 * PAD);
            texto(cx, ALTURA / 2, r, fonte);
        } else if (it->tipo == VOLUME) {
            int e_sink = !strcmp(it->dev, "sink");
            int v     = e_sink ? vol_sink  : vol_source;
            int mudo  = e_sink ? mudo_sink : mudo_source;
            int cx_r  = it->x + VOL_ROTULO;          /* fim do rotulo */
            int calha_x = cx_r + GAP;
            int calha_y = ALTURA / 2 - 5;
            int util, pos;

            /* O rotulo e o botao de mudo: afundado = mudo. Nao inventa cor
             * nova para o estado - usa o bisel, como o resto da barra. */
            if (mudo)
                gravado(it->x, PAD, VOL_ROTULO, ALTURA - 2 * PAD);
            else
                levantado(it->x, PAD, VOL_ROTULO, ALTURA - 2 * PAD);
            texto(it->x + VOL_ROTULO / 2, ALTURA / 2, it->rotulo, fonte);

            /* calha gravada */
            gravado(calha_x, calha_y, VOL_CALHA, 10);

            if (v < 0)
                continue;                 /* sem PulseAudio: calha vazia */

            /* cursor levantado, transbordando a calha como num Motif */
            util = VOL_CALHA - 8;
            pos  = calha_x + 1 + (util * v) / 100;
            levantado(pos, calha_y - 3, 7, 16);

            /* seta do menu de dispositivos, no canto direito */
            {
                int sx = it->x + it->larg - VOL_SETA;
                int cxs = sx + VOL_SETA / 2, cys = ALTURA / 2;
                XPoint tri[3];

                levantado(sx, PAD, VOL_SETA, ALTURA - 2 * PAD);
                tri[0].x = cxs - 3; tri[0].y = cys - 1;
                tri[1].x = cxs + 3; tri[1].y = cys - 1;
                tri[2].x = cxs;     tri[2].y = cys + 3;
                XSetForeground(dpy, gc, INK);
                XFillPolygon(dpy, alvo, gc, tri, 3, Convex, CoordModeOrigin);
            }
        } else if (it->tipo == BOTAO_APP) {
            levantado(it->x, PAD, it->larg, ALTURA - 2 * PAD);
            if (it->icone)
                /* o Pixmap ja esta na profundidade da tela: copiar e trabalho
                 * do servidor, nao ha pixel passando pelo socket aqui */
                XCopyArea(dpy, it->icone, alvo, gc, 0, 0,
                          it->ic_lado, it->ic_lado,
                          it->x + (it->larg - it->ic_lado) / 2,
                          (ALTURA - it->ic_lado) / 2);
            else
                texto(cx, ALTURA / 2, it->rotulo, fonte);
        } else {
            levantado(it->x, PAD, it->larg, ALTURA - 2 * PAD);
            texto(cx, ALTURA / 2, it->rotulo, fonte);
        }
    }
    XFlush(dpy);
}

/* Redesenha so quando o que aparece na tela muda. Sem isto, redesenhar a cada
 * 2 s faria a barra piscar de leve (nao ha duplo buffer aqui). */
static void tick(void)
{
    static char antes[128] = "";
    static int  ciclos = 0;
    char agora[128], hm[8];
    time_t t = time(NULL);
    int i, algum_pendente = 0, cam_agora;

    /* Voce fixou ou tirou um app pelo "[+]". Um stat num arquivo local nao
     * custa nada, e e o que faz a barra mudar sozinha em ate 2 s - sem
     * reiniciar e sem a barra precisar falar com o zenity. */
    if (apps_mtime() != apps_ref) {
        recarregar_apps();
        return;                  /* ja redesenhou tudo */
    }

    /* Revalida o estado de verdade a cada ~30 s. O cache so era reescrito
     * quando NOS mexiamos, e por isso ficava mentindo quando o estado mudava
     * por fora: um detach feito do lado Windows, o aparelho desplugado, ou um
     * usbipd que nao confirmou. Visto em 30/07/2026, com o cache dizendo
     * "camera=windows" enquanto o usbipd a mostrava como Attached.
     *
     * Solto, nunca em linha: a chamada de interop leva ~1 s e travaria a barra.
     * Nao mexemos em nada aqui - so pedimos que o cache seja reescrito, e o
     * proximo tick le o valor novo. */
    if (++ciclos >= 15) {
        ciclos = 0;
        solta("transferir-usb estado");
    }

    ler_cache();

    /* Nao perguntamos o volume no meio de um arrasto: o valor local e o que o
     * mouse esta ditando, e o pactl ainda pode estar respondendo o antigo -
     * o cursor pularia para tras. */
    if (arrastando < 0)
        ler_volumes();

    for (i = 0; i < n_itens; i++)
        if (itens[i].pendente)
            algum_pendente = 1;

    /* o transferir-usb reescreve o cache ao terminar; e o nosso sinal de fim */
    if (algum_pendente && cache_mtime() != mt_ref)
        for (i = 0; i < n_itens; i++)
            if (strcmp(itens[i].dev ? itens[i].dev : "", "camera") != 0)
                itens[i].pendente = 0;

    /* A camera nao passa pelo cache do transferir-usb, entao precisa do proprio
     * sinal de fim: o estado da ponte ter mudado em relacao ao do clique. */
    cam_agora = camera_ligada();
    for (i = 0; i < n_itens; i++)
        if (itens[i].dev && !strcmp(itens[i].dev, "camera") &&
            itens[i].pendente && cam_agora != cam_ref)
            itens[i].pendente = 0;

    strftime(hm, sizeof hm, "%H:%M", localtime(&t));
    snprintf(agora, sizeof agora, "%s|%s|%d|%d|%d%d|%d%d",
             hm, est_audio, cam_agora, algum_pendente,
             vol_sink, mudo_sink, vol_source, mudo_source);

    if (strcmp(agora, antes) != 0) {
        desenhar();
        snprintf(antes, sizeof antes, "%s", agora);
    }
}

/* ---- arranque --------------------------------------------------------- */

/* Geometria do monitor onde a barra vive. Sem coordenada chumbada: se voce
 * trocar de ambiente, ela se acha sozinha.
 *
 * ATENCAO AO FALLBACK - e a parte que importa aqui. O xrandr desta sessao
 * frequentemente NAO marca monitor primario nenhum:
 *
 *   $ xrandr --listmonitors
 *   0: +rdp0 1920/344x1080/194+2560+0      <- sem o '*' em nenhum dos dois
 *   1: +rdp1 2560/798x1080/334+0+0
 *
 * Medido em 30/07/2026: numa sessao havia '+*rdp1' e na seguinte, apos "Sair da
 * sessao" e reconectar, nenhum dos dois tinha a marca. A ordem em que o xrdp
 * cria as saidas tambem nao e estavel, entao cair no "indice 0" punha a barra no
 * monitor errado de forma imprevisivel.
 *
 * A regra de desempate e por POSICAO: o monitor que contem a origem (0,0). E o
 * mesmo critério que o jogo-windows usa para numerar monitores (esquerda para a
 * direita), e nao depende de flag que o xrdp pode nao setar. */
static void primario(int *px, int *py, int *pw, int *ph)
{
    XRRMonitorInfo *m;
    int n, i, esc = -1;

    *px = 0; *py = 0;
    *pw = DisplayWidth(dpy, DefaultScreen(dpy));
    *ph = DisplayHeight(dpy, DefaultScreen(dpy));

    m = XRRGetMonitors(dpy, DefaultRootWindow(dpy), True, &n);
    if (!m || n <= 0)
        return;

    for (i = 0; i < n; i++)                      /* 1o: a flag, se existir */
        if (m[i].primary) { esc = i; break; }

    if (esc < 0)
        for (i = 0; i < n; i++)                  /* 2o: o que contem a origem */
            if (m[i].x == 0 && m[i].y == 0) { esc = i; break; }

    if (esc < 0) {                               /* 3o: o mais a esquerda */
        esc = 0;
        for (i = 1; i < n; i++)
            if (m[i].x < m[esc].x ||
                (m[i].x == m[esc].x && m[i].y < m[esc].y))
                esc = i;
    }

    *px = m[esc].x; *py = m[esc].y;
    *pw = m[esc].width; *ph = m[esc].height;
    XRRFreeMonitors(m);
}

/* ---- reserva de espaco (EWMH) ------------------------------------------
 *
 * O PROBLEMA. Ate 31/07/2026 a barra era override_redirect: uma janela que o
 * xfwm4 nao gerencia, nao move e nao enxerga. Isso resolvia o posicionamento
 * exato e a ausencia de moldura, mas tinha o custo de TAPAR o que estivesse
 * embaixo - uma janela maximizada ia ate o fim da tela e os 28px de baixo dela
 * ficavam escondidos atras da barra.
 *
 * A SOLUCAO. Quem encolhe as janelas maximizadas e o gerenciador de janelas, e
 * ele so leva em conta janelas que gerencia. Entao a barra deixou de ser
 * override_redirect e virou uma janela COMUM com dois anuncios EWMH:
 *
 *   _NET_WM_WINDOW_TYPE = _NET_WM_WINDOW_TYPE_DOCK
 *       o xfwm4 nao desenha moldura, nao poe na lista de janelas, nao dá foco
 *       e mantem acima das janelas normais - ou seja, devolve de graca tudo o
 *       que o override_redirect dava.
 *
 *   _NET_WM_STRUT_PARTIAL
 *       "reserve N pixels na borda de baixo da tela". O xfwm4 subtrai isso do
 *       _NET_WORKAREA, e maximizar/tile passam a parar no topo da barra
 *       sozinhos. Nada mais no codigo precisa saber disto.
 *
 * O STRUT E MEDIDO DA BORDA DA TELA INTEIRA, nao do monitor. Com dois monitores
 * lado a lado a tela X e a caixa que envolve os dois, entao o valor certo e
 *
 *     bottom = altura_da_tela - (monitor_y + monitor_altura) + ALTURA
 *
 * que dá exatamente ALTURA quando o monitor da barra encosta no fundo da tela,
 * e compensa a diferenca quando ele e mais baixo que o vizinho. Os campos
 * start_x/end_x limitam o strut ao trecho horizontal onde a barra esta, e e o
 * que impede o outro monitor de perder 28px por nada.
 *
 * NAO volte a override_redirect sem tirar isto junto: janela nao gerenciada nao
 * tem strut nenhum, o xfwm4 ignora a propriedade e o efeito some sem erro. */
static void anunciar_dock(void)
{
    Atom tipo, dock, estado[4], cardeal[1];
    XWMHints    wmh;
    XClassHint  ch;

    tipo = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, win, tipo, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&dock, 1);

    estado[0] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    estado[1] = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    estado[2] = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    estado[3] = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_STATE", False),
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)estado, 4);

    /* 0xFFFFFFFF = "todos os espacos de trabalho" */
    cardeal[0] = 0xFFFFFFFF;
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_DESKTOP", False),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)cardeal, 1);

    /* Tamanho fixo e posicao escolhida por nos: sem isto o xfwm4 se acha no
     * direito de reposicionar a janela na primeira aparicao. */
    dicas_de_tamanho();

    /* input = False: a barra nunca aceita foco de teclado. O tipo DOCK ja
     * bastaria no xfwm4, mas a dica e barata e vale para qualquer WM. */
    memset(&wmh, 0, sizeof wmh);
    wmh.flags = InputHint | StateHint;
    wmh.input = False;
    wmh.initial_state = NormalState;
    XSetWMHints(dpy, win, &wmh);

    ch.res_name = "barra-tarefas"; ch.res_class = "Barra-tarefas";
    XSetClassHint(dpy, win, &ch);
    XStoreName(dpy, win, "barra-tarefas");
}

/* Ha algum monitor ABAIXO do da barra, no mesmo trecho horizontal dela?
 *
 * Esta pergunta existe porque o strut e um conceito de BORDA DA TELA: ele so
 * sabe dizer "reserve N pixels contados do fundo", e nao "reserve uma faixa no
 * meio". Com os monitores lado a lado (o caso normal aqui) isso nao aparece,
 * porque o fundo do monitor e o fundo da tela e N vale 28.
 *
 * Com monitores EMPILHADOS e a barra no de cima, o mesmo N passa a valer a
 * altura do monitor de baixo inteiro, e os campos start_x/end_x nao salvam:
 * o monitor de baixo esta no MESMO trecho horizontal. Medido em 31/07/2026, com
 * dois monitores 2560x540 empilhados e a barra no de cima:
 *
 *   _NET_WM_STRUT_PARTIAL = 0,0,0,568 ...        <- 1080-540+28
 *   maximizar no monitor DE BAIXO  ->  x=0 y=24 2560x488
 *
 * ou seja, a janela foi parar no monitor DE CIMA e o de baixo virou terra
 * morta. Nao ha valor de strut que descreva "barra no meio da tela", entao o
 * certo e nao publicar nenhum: a barra volta a so flutuar por cima naquele
 * monitor - o comportamento antigo, ruim mas nao quebrado. E o que os paineis
 * de verdade fazem quando nao estao numa borda da tela. */
static int engoliria_vizinho(int bx, int my, int mh)
{
    XRRMonitorInfo *m;
    int n, i, achou = 0;

    m = XRRGetMonitors(dpy, DefaultRootWindow(dpy), True, &n);
    if (!m)
        return 0;

    for (i = 0; i < n && !achou; i++) {
        if (m[i].y + m[i].height <= my + mh)         /* nao esta abaixo (o
                                                      * proprio monitor da barra
                                                      * cai aqui) */
            continue;
        if (m[i].x >= bx + LARG || m[i].x + m[i].width <= bx)
            continue;                                /* fora do trecho da barra */
        achou = 1;
    }
    XRRFreeMonitors(m);
    return achou;
}

/* bx = x da barra na tela; my/mh = topo e altura do monitor onde ela esta. */
static void aplicar_strut(int bx, int my, int mh)
{
    long p[12] = {0}, s[4] = {0};
    int  tela_h = DisplayHeight(dpy, DefaultScreen(dpy));
    long fundo  = tela_h - (my + mh) + ALTURA;

    if (fundo < 0 || engoliria_vizinho(bx, my, mh))
        fundo = 0;                                 /* zero = "nao reserve nada" */

    s[3] = fundo;                                  /* left, right, top, bottom */
    p[3] = fundo;
    p[10] = bx;                                    /* bottom_start_x */
    p[11] = bx + LARG - 1;                         /* bottom_end_x   */

    XChangeProperty(dpy, win,
                    XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)p, 12);
    /* _NET_WM_STRUT e o antecessor, sem os start/end. Fica para o caso de um WM
     * que so entenda o formato antigo; quem entende os dois usa o PARTIAL. */
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_STRUT", False),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)s, 4);
}

static void layout(void)
{
    int i, x = PAD;

    LARG = 2 * PAD;
    for (i = 0; i < n_itens; i++)
        LARG += itens[i].larg + (i ? GAP : 0);

    for (i = 0; i < n_itens; i++) {
        itens[i].x = x;
        x += itens[i].larg + GAP;
    }
}

/* Tamanho fixo e posicao escolhida por nos. Repetida no resize porque as dicas
 * dizem min=max: sem atualiza-las ANTES do XResizeWindow, o xfwm4 recusa a
 * largura nova e a barra fica do tamanho velho com o conteudo transbordando. */
static void dicas_de_tamanho(void)
{
    XSizeHints sh;

    memset(&sh, 0, sizeof sh);
    sh.flags      = USPosition | USSize | PMinSize | PMaxSize;
    sh.min_width  = sh.max_width  = LARG;
    sh.min_height = sh.max_height = ALTURA;
    XSetWMNormalHints(dpy, win, &sh);
}

/* Monta a barra inteira: fixos da esquerda, atalhos de aplicativo, fixos da
 * direita. Chamada no arranque e a cada mudanca do apps.conf. */
static void montar_itens(void)
{
    char linha[512];
    FILE *f;
    int i, n = 0;

    for (i = 0; i < n_itens; i++)          /* os Pixmaps antigos nao servem */
        if (itens[i].icone)
            XFreePixmap(dpy, itens[i].icone);

    memset(itens, 0, sizeof itens);
    for (i = 0; i < N_ANTES; i++)
        itens[n++] = fixos_antes[i];

    /* Mesmo formato tabulado dos menus: <id>\t<nome>\t<arquivo de pixels>. */
    f = popen("barra-apps listar 2>/dev/null", "r");
    if (f) {
        while (n < N_ANTES + MAX_APPS && fgets(linha, sizeof linha, f)) {
            char *t1 = strchr(linha, '\t'), *t2;
            Item *it = &itens[n];

            if (!t1) continue;
            *t1 = '\0';
            t2 = strchr(t1 + 1, '\t');
            if (!t2) continue;
            *t2 = '\0';
            { char *nl = strchr(t2 + 1, '\n'); if (nl) *nl = '\0'; }

            it->tipo = BOTAO_APP;
            snprintf(it->id,  sizeof it->id,  "%.286s", linha);
            snprintf(it->txt, sizeof it->txt, "%.62s",  t1 + 1);
            it->rotulo  = it->txt;         /* aponta para dentro do proprio item */
            it->icone   = carregar_icone(t2 + 1, APP_LADO);
            it->ic_lado = APP_LADO;
            /* Sem icone o botao nao some: vira um botao de texto com o nome do
             * app. Um icone que nao converteu nao pode custar o atalho. */
            it->larg    = it->icone ? APP_LARG
                        : XTextWidth(fonte, it->txt, strlen(it->txt)) + 12;
            n++;
        }
        pclose(f);
    }

    for (i = 0; i < N_DEPOIS; i++)
        itens[n++] = fixos_depois[i];

    n_itens  = n;
    apps_ref = apps_mtime();
    layout();
}

/* O apps.conf mudou (voce fixou ou tirou um app pelo "[+]"). A barra muda de
 * largura, entao nao basta redesenhar: e preciso reavisar o tamanho, redimen-
 * sionar, reposicionar - e o reposicionar refaz o strut, que depende da
 * largura. */
static void recarregar_apps(void)
{
    montar_itens();
    dicas_de_tamanho();
    XResizeWindow(dpy, win, LARG, ALTURA);
    reposicionar();
    desenhar();
}

/* ---- atalhos de aplicativo -----------------------------------------------
 *
 * A DIVISAO. A barra nao sabe o que e um aplicativo: ela pede a lista ao
 * barra-apps, desenha os icones e devolve o id que voce clicou. .desktop, tema
 * de icones e linha de comando sao problema de la. Mesma divisao do
 * audio-dispositivos e do jogo-windows.
 *
 * POR QUE NAO HA libpng AQUI. Estes sao os primeiros pixels de verdade que a
 * barra desenha, e seria natural linkar uma biblioteca de imagem — seria
 * tambem o fim da premissa do arquivo (Xlib cru, 2,6 MB de RSS). Em vez disso o
 * barra-apps converte o icone UMA vez, na hora em que voce o fixa, e deixa no
 * cache um arquivo de PIXELS CRUS: lado*lado*3 bytes, RGB de 8 bits, sem
 * cabecalho, sem compressao e sem alfa (o alfa foi achatado sobre o #DEDEDE da
 * face, que e chapado, entao o resultado e identico ao de compor de verdade).
 * Ler isso e um fread. Nenhuma dependencia nova entrou.
 */

/* Poe um componente 0-255 na posicao que ele ocupa na mascara do visual. Sem
 * isto seria preciso chumbar "0xRRGGBB", e num servidor com outro arranjo o
 * icone sairia com as cores trocadas — falha feia e silenciosa. */
static unsigned long componente(unsigned long mascara, unsigned v)
{
    int desl = 0, bits = 0;
    unsigned long m = mascara;

    if (!mascara)
        return 0;
    while (!(m & 1)) { m >>= 1; desl++; }
    while (m & 1)    { m >>= 1; bits++; }
    if (bits < 8)
        v >>= (8 - bits);
    return ((unsigned long) v << desl) & mascara;
}

/* O Pixmap e montado UMA vez, no carregamento. Cada redesenho depois e so um
 * XCopyArea, que roda inteiro dentro do servidor X. */
static Pixmap carregar_icone(const char *caminho, int lado)
{
    int tela = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, tela);
    int prof = DefaultDepth(dpy, tela);
    size_t n = (size_t) lado * lado * 3;
    unsigned char *cru;
    XImage *img;
    Pixmap pm;
    FILE *f;
    int x, y;

    if (!caminho || !*caminho)
        return 0;
    f = fopen(caminho, "rb");
    if (!f)
        return 0;

    cru = malloc(n);
    /* Tamanho errado = arquivo truncado ou de outro lado; melhor cair no rotulo
     * de texto do que desenhar lixo. */
    if (!cru || fread(cru, 1, n, f) != n) { free(cru); fclose(f); return 0; }
    fclose(f);

    img = XCreateImage(dpy, vis, prof, ZPixmap, 0, NULL, lado, lado, 32, 0);
    if (!img) { free(cru); return 0; }
    img->data = calloc((size_t) img->bytes_per_line, lado);
    if (!img->data) { XDestroyImage(img); free(cru); return 0; }

    for (y = 0; y < lado; y++)
        for (x = 0; x < lado; x++) {
            const unsigned char *p = cru + ((size_t) y * lado + x) * 3;
            XPutPixel(img, x, y,
                      componente(vis->red_mask,   p[0]) |
                      componente(vis->green_mask, p[1]) |
                      componente(vis->blue_mask,  p[2]));
        }

    pm = XCreatePixmap(dpy, DefaultRootWindow(dpy), lado, lado, prof);
    XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, lado, lado);
    XDestroyImage(img);          /* leva o img->data junto */
    free(cru);
    return pm;
}

static const char *caminho_apps(void)
{
    static char c[256];
    const char *h = getenv("HOME");

    if (!c[0])
        snprintf(c, sizeof c, "%s/.config/linux-fullscreen/apps.conf",
                 h ? h : "");
    return c;
}

static time_t apps_mtime(void)
{
    struct stat st;
    return stat(caminho_apps(), &st) == 0 ? st.st_mtime : 0;
}

/* Poe a barra no lugar certo do monitor primario. Chamada no arranque e a cada
 * mudanca de monitor - o jogo-windows encolhe a sessao para um monitor so
 * enquanto o jogo roda, e sem isto a barra ficaria fora da tela ate o proximo
 * login. Tambem cobre monitor plugado/desplugado e reconexao com layout novo. */
static void reposicionar(void)
{
    int mx, my, mw, mh, bx, by;

    primario(&mx, &my, &mw, &mh);
    by = my + mh - ALTURA;
    if (ALINHAMENTO == CENTRO)        bx = mx + (mw - LARG) / 2;
    else if (ALINHAMENTO == DIREITA)  bx = mx + mw - LARG - MARGEM_LATERAL;
    else                              bx = mx + MARGEM_LATERAL;

    XMoveWindow(dpy, win, bx, by);
    XRaiseWindow(dpy, win);
    aplicar_strut(bx, my, mh);   /* a reserva muda junto: o strut e medido da
                                  * borda da TELA, e a tela muda de tamanho
                                  * quando o jogo-windows encolhe a sessao */
}

int main(void)
{
    XSetWindowAttributes at;
    int rr_base, rr_err;

    signal(SIGCHLD, SIG_IGN);       /* nao deixar zumbi dos comandos soltos */

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "barra: nao abriu o display\n");
        return 1;
    }

    FACE = cor("#DEDEDE");
    HI   = cor("#FFFFFF");
    SH   = cor("#808080");
    INK  = cor("#000000");

    fonte = XLoadQueryFont(dpy,
        "-*-helvetica-medium-r-normal--11-*-*-*-*-*-iso8859-1");
    mono  = XLoadQueryFont(dpy,
        "-*-fixed-medium-r-normal--10-*-*-*-*-*-iso8859-1");
    if (!fonte || !mono) {
        fprintf(stderr, "barra: fonte core do X nao encontrada\n");
        return 1;
    }

    /* SEM override_redirect de proposito - veja "reserva de espaco (EWMH)"
     * acima. A barra precisa ser gerenciada pelo xfwm4 para que o strut valha;
     * o tipo DOCK e quem devolve o "sem moldura, sempre acima, sem foco". */
    at.background_pixel  = FACE;
    at.event_mask        = ExposureMask | ButtonPressMask |
                           ButtonReleaseMask | Button1MotionMask;

    /* Nasce 1x1 e so depois recebe o tamanho certo, porque a ordem aqui e
     * circular: montar_itens() precisa do gc para converter os icones em
     * Pixmap, o gc precisa de uma janela, e a largura da janela so e conhecida
     * DEPOIS de saber quantos atalhos existem. Redimensionar antes de mapear
     * nao custa nada e nao pisca. */
    win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, ALTURA, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, win, 0, NULL);

    montar_itens();
    XResizeWindow(dpy, win, LARG, ALTURA);

    /* avisos de mudanca de monitor; sem a extensao, so nao reposiciona */
    if (XRRQueryExtension(dpy, &rr_base, &rr_err))
        XRRSelectInput(dpy, DefaultRootWindow(dpy), RRScreenChangeNotifyMask);
    else
        rr_base = -1000;

    /* Tudo isto ANTES de mapear: o WM le tipo, hints e strut no instante em que
     * adota a janela, e o mapa e o que dispara a adocao. A barra sobe antes do
     * xfwm4 no startwm.sh, entao quem adota e a varredura de janelas ja mapeadas
     * que ele faz no arranque - que le as mesmas propriedades. */
    anunciar_dock();
    reposicionar();
    XMapWindow(dpy, win);

    /* O cache vive no XDG_RUNTIME_DIR e morre com a sessao - o que e correto,
     * porque um 'wsl --shutdown' devolve tudo ao Windows. Mas entao no primeiro
     * desenho nao ha estado, e os botoes sairiam "--" mesmo com o aparelho
     * disponivel. Pedimos um refresh aqui, solto: a chamada de interop leva
     * quase um segundo e nao pode bloquear o arranque da barra. */
    solta("transferir-usb estado");

    for (;;) {
        fd_set fds;
        struct timeval tv;
        int fd = ConnectionNumber(dpy);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            /* Com o menu aberto o ponteiro esta capturado (owner_events
             * False), entao TODO evento de mouse chega aqui - inclusive os de
             * fora do menu, e e assim que ele fecha ao clicar em outro lugar. */
            if (pop) {
                if (ev.type == Expose && ev.xany.window == pop) {
                    desenhar_popup();
                    continue;
                }
                if (ev.type == MotionNotify) {
                    int novo = -1;
                    if (ev.xmotion.x >= 0 && ev.xmotion.x < pop_w &&
                        ev.xmotion.y >= 2 &&
                        ev.xmotion.y < 2 + pop_n * POP_LINHA)
                        novo = (ev.xmotion.y - 2) / POP_LINHA;
                    if (novo < 0 || novo >= pop_n) novo = -1;
                    if (novo != pop_sob) { pop_sob = novo; desenhar_popup(); }
                    continue;
                }
                if (ev.type == ButtonPress) {
                    /* A linha vem das COORDENADAS do clique, nao de pop_sob:
                     * pop_sob so e preenchido por MotionNotify, e um clique
                     * direto (sem passar por cima antes) nao gera movimento -
                     * o menu fechava sem escolher nada.
                     *
                     * E o teste e por coordenada, nao por janela: com o
                     * ponteiro capturado em owner_events False, um clique FORA
                     * tambem chega com window == pop, so que com x/y fora do
                     * retangulo. */
                    int lin = -1;
                    if (ev.xbutton.x >= 0 && ev.xbutton.x < pop_w &&
                        ev.xbutton.y >= 2 && ev.xbutton.y < 2 + pop_n * POP_LINHA)
                        lin = (ev.xbutton.y - 2) / POP_LINHA;

                    if (lin >= 0 && lin < pop_n)
                        escolher_popup(lin);
                    else
                        fechar_popup();
                    continue;
                }
                if (ev.type == ButtonRelease)
                    continue;
            }

            if (ev.type == rr_base + RRScreenChangeNotify) {
                /* obrigatorio: sem isto o Xlib segue com a tela antiga em cache
                 * e o reposicionamento usa geometria velha */
                XRRUpdateConfiguration(&ev);
                reposicionar();
                desenhar();
            } else if (ev.type == Expose) {
                desenhar();
            } else if (ev.type == MotionNotify && arrastando >= 0) {
                /* Comprime o rastro: o X entrega dezenas de MotionNotify por
                 * segundo, e cada um viraria um pactl. Fica so o ultimo. */
                while (XCheckTypedWindowEvent(dpy, win, MotionNotify, &ev))
                    ;
                aplicar_volume(&itens[arrastando], ev.xmotion.x);
            } else if (ev.type == ButtonRelease) {
                arrastando = -1;
            } else if (ev.type == ButtonPress) {
                int i;

                /* ATE 31/07/2026 NAO SE OLHAVA QUAL BOTAO ERA, e todos faziam a
                 * acao do esquerdo: botao direito no icone do Brave abria o
                 * Brave, botao do meio na calha de volume mexia no volume. Nunca
                 * incomodou porque nao havia nada ligado ao direito - passou a
                 * incomodar no instante em que ele virou "tirar da barra". */
                if (ev.xbutton.button != Button1 &&
                    ev.xbutton.button != Button3)
                    continue;

                for (i = 0; i < n_itens; i++) {
                    Item *it = &itens[i];

                    if (ev.xbutton.x < it->x ||
                        ev.xbutton.x >= it->x + it->larg)
                        continue;

                    /* O direito so tem sentido nos atalhos de aplicativo; nos
                     * outros itens ele nao faz nada, em vez de fazer a acao do
                     * esquerdo. */
                    if (ev.xbutton.button == Button3) {
                        if (it->tipo == BOTAO_APP)
                            abrir_popup(it);
                        break;
                    }

                    if (it->tipo == VOLUME) {
                        aplicar_volume(it, ev.xbutton.x);
                        arrastando = i;
                    } else if (it->tipo == BOTAO_JOGOS) {
                        abrir_popup(it);
                    } else if (it->tipo == BOTAO_APP) {
                        char cmd[400], q[320];
                        cita(q, sizeof q, it->id);
                        snprintf(cmd, sizeof cmd, "barra-apps executar %s", q);
                        solta(cmd);
                    } else if (it->tipo == BOTAO_MAIS) {
                        /* o barra-apps abre a lista pelo zenity e mexe no
                         * apps.conf; o tick ve o mtime mudar e remonta */
                        solta("barra-apps escolher");
                    } else if (it->tipo == BOTAO_USB) {
                        char cmd[128];
                        /* a camera nao e mais transferencia USB: e a ponte de
                         * rede. Ver o comentario da tabela 'itens'. */
                        if (!strcmp(it->dev, "camera")) {
                            snprintf(cmd, sizeof cmd, "camera-rede alternar");
                            cam_ref = camera_ligada();
                        } else {
                            snprintf(cmd, sizeof cmd,
                                     "transferir-usb %s alternar", it->dev);
                            mt_ref = cache_mtime();
                        }
                        it->pendente = 1;
                        solta(cmd);
                        desenhar();          /* mostra o "..." na hora */
                    } else if (it->acao) {
                        it->acao();
                    }
                    break;
                }
            }
        }

        /* Acorda a cada 2 s, mas so redesenha se algo mudou de verdade - o
         * relogio, o estado de um dispositivo, ou um "..." que terminou. Ler o
         * cache e um fopen de 30 bytes, nao ha custo em olhar com frequencia. */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (select(fd + 1, &fds, NULL, NULL, &tv) == 0)
            tick();
    }
    return 0;
}
