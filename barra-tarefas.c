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
 * Escreva a funcao de acao e ponha uma linha na tabela "itens" abaixo. A
 * largura da barra e calculada a partir dela; nada mais precisa mudar.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

enum { BOTAO, RELOGIO, BOTAO_USB, VOLUME };

typedef struct {
    int         tipo;
    const char *rotulo;
    int         larg;
    void      (*acao)(void);
    const char *dev;        /* so BOTAO_USB: "audio" ou "camera" */
    int         pendente;   /* clicado, esperando o transferir-usb responder */
    int         x;          /* preenchido pelo layout */
} Item;

static void acao_desligar(void);
static void tick(void);
static void aplicar_volume(Item *it, int mx);
static void desenhar(void);
static void levantado(int x, int y, int w, int h);
static void gravado(int x, int y, int w, int h);
static void primario(int *px, int *py, int *pw, int *ph);
static void fechar_popup(void);
static void escolher_popup(int i);
static void desenhar_popup(void);
static void abrir_popup(Item *it);
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
static Item itens[] = {
    { VOLUME,    "Vol",     VOL_LARG, NULL,       "sink",   0, 0 },
    { VOLUME,    "Mic",     VOL_LARG, NULL,       "source", 0, 0 },
    { BOTAO_USB, "Audio",    92, NULL,          "audio",  0, 0 },
    { BOTAO_USB, "Camera",   92, NULL,          "camera", 0, 0 },
    { RELOGIO,   NULL,       52, NULL,          NULL,     0, 0 },
    { BOTAO,     "Desligar", 66, acao_desligar, NULL,     0, 0 },
};

/* ---- menu de dispositivos ------------------------------------------------
 * O Linux so tem dois dispositivos de cada lado (xrdp e, quando anexada, a
 * placa USB). Um menu feito so com o pactl mostraria "xrdp-sink", que nao
 * significa nada, e esconderia a caixa do notebook e o monitor - que sao do
 * Windows e so existem atras do canal RDP. Por isso a lista vem do
 * audio-dispositivos, que junta os dois mundos.
 */
#define MAX_DEV 16
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
static char   pop_id[MAX_DEV][192];
static char   pop_rot[MAX_DEV][120];
static int    pop_atual[MAX_DEV];
static char   pop_lado[8];
#define N_ITENS ((int) (sizeof itens / sizeof itens[0]))

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
    snprintf(pop_lado, sizeof pop_lado, "%s",
             !strcmp(it->dev, "sink") ? "saida" : "entrada");
    snprintf(cmd, sizeof cmd, "audio-dispositivos listar %s 2>/dev/null", pop_lado);

    pop_n = 0;
    f = popen(cmd, "r");
    if (!f)
        return;
    while (pop_n < MAX_DEV && fgets(linha, sizeof linha, f)) {
        char *t1 = strchr(linha, '\t'), *t2;
        int w;

        if (!t1) continue;
        *t1 = '\0';
        t2 = strchr(t1 + 1, '\t');
        if (!t2) continue;
        *t2 = '\0';
        { char *nl = strchr(t2 + 1, '\n'); if (nl) *nl = '\0'; }

        snprintf(pop_id[pop_n],  sizeof pop_id[0],  "%.190s", linha);
        snprintf(pop_rot[pop_n], sizeof pop_rot[0], "%.118s", t2 + 1);
        pop_atual[pop_n] = atoi(t1 + 1);

        w = XTextWidth(fonte, pop_rot[pop_n], strlen(pop_rot[pop_n])) + 32;
        if (w > larg) larg = w;
        pop_n++;
    }
    pclose(f);
    if (pop_n == 0)
        return;

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
    char cmd[256];

    if (i >= 0 && i < pop_n) {
        snprintf(cmd, sizeof cmd, "audio-dispositivos usar %s '%s'",
                 pop_lado, pop_id[i]);
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

    for (i = 0; i < N_ITENS; i++) {
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

    for (i = 0; i < N_ITENS; i++)
        if (itens[i].pendente)
            algum_pendente = 1;

    /* o transferir-usb reescreve o cache ao terminar; e o nosso sinal de fim */
    if (algum_pendente && cache_mtime() != mt_ref)
        for (i = 0; i < N_ITENS; i++)
            if (strcmp(itens[i].dev ? itens[i].dev : "", "camera") != 0)
                itens[i].pendente = 0;

    /* A camera nao passa pelo cache do transferir-usb, entao precisa do proprio
     * sinal de fim: o estado da ponte ter mudado em relacao ao do clique. */
    cam_agora = camera_ligada();
    for (i = 0; i < N_ITENS; i++)
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

static void layout(void)
{
    int i, x = PAD;

    LARG = 2 * PAD;
    for (i = 0; i < N_ITENS; i++)
        LARG += itens[i].larg + (i ? GAP : 0);

    for (i = 0; i < N_ITENS; i++) {
        itens[i].x = x;
        x += itens[i].larg + GAP;
    }
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

    layout();

    at.override_redirect = True;      /* sem moldura do xfwm4, posicao exata */
    at.background_pixel  = FACE;
    at.event_mask        = ExposureMask | ButtonPressMask |
                           ButtonReleaseMask | Button1MotionMask;

    win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, LARG, ALTURA, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, win, 0, NULL);

    /* avisos de mudanca de monitor; sem a extensao, so nao reposiciona */
    if (XRRQueryExtension(dpy, &rr_base, &rr_err))
        XRRSelectInput(dpy, DefaultRootWindow(dpy), RRScreenChangeNotifyMask);
    else
        rr_base = -1000;

    reposicionar();
    XMapRaised(dpy, win);

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
                for (i = 0; i < N_ITENS; i++) {
                    Item *it = &itens[i];

                    if (ev.xbutton.x < it->x ||
                        ev.xbutton.x >= it->x + it->larg)
                        continue;

                    if (it->tipo == VOLUME) {
                        aplicar_volume(it, ev.xbutton.x);
                        arrastando = i;
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
