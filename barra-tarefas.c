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

enum { BOTAO, RELOGIO };

typedef struct {
    int         tipo;
    const char *rotulo;
    int         larg;
    void      (*acao)(void);
    int         x;          /* preenchido pelo layout */
} Item;

static void acao_desligar(void);

/* A barra, da esquerda para a direita. Sem lista de janelas de proposito - o
 * caminho de volta para janela minimizada continua sendo o Alt+Tab
 * (cycle_hidden=true no xfwm4.xml). */
static Item itens[] = {
    { RELOGIO, NULL,       52, NULL,          0 },
    { BOTAO,   "Desligar", 66, acao_desligar, 0 },
};
#define N_ITENS ((int) (sizeof itens / sizeof itens[0]))

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

/* ---- desenho ---------------------------------------------------------- */

static void linha(int x0, int y0, int x1, int y1, unsigned long c)
{
    XSetForeground(dpy, gc, c);
    XDrawLine(dpy, win, gc, x0, y0, x1, y1);
}

static void levantado(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y1 = y + h - 1;

    XSetForeground(dpy, gc, FACE);
    XFillRectangle(dpy, win, gc, x, y, w, h);
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
    XFillRectangle(dpy, win, gc, x, y, w, h);
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
    XDrawString(dpy, win, gc, cx - w / 2,
                cy + (f->ascent - f->descent) / 2, s, strlen(s));
}

static void desenhar(void)
{
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
        } else {
            levantado(it->x, PAD, it->larg, ALTURA - 2 * PAD);
            texto(cx, ALTURA / 2, it->rotulo, fonte);
        }
    }
    XFlush(dpy);
}

/* ---- arranque --------------------------------------------------------- */

/* Geometria do monitor primario. Sem coordenada chumbada: se voce trocar de
 * ambiente, ela se acha sozinha. */
static void primario(int *px, int *py, int *pw, int *ph)
{
    XRRMonitorInfo *m;
    int n, i, achou = 0;

    *px = 0; *py = 0;
    *pw = DisplayWidth(dpy, DefaultScreen(dpy));
    *ph = DisplayHeight(dpy, DefaultScreen(dpy));

    m = XRRGetMonitors(dpy, DefaultRootWindow(dpy), True, &n);
    if (!m)
        return;
    for (i = 0; i < n; i++) {
        if (m[i].primary || (!achou && i == 0)) {
            *px = m[i].x; *py = m[i].y;
            *pw = m[i].width; *ph = m[i].height;
            if (m[i].primary) achou = 1;
        }
    }
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

int main(void)
{
    XSetWindowAttributes at;
    int mx, my, mw, mh, bx, by;

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
    primario(&mx, &my, &mw, &mh);

    by = my + mh - ALTURA;
    if (ALINHAMENTO == CENTRO)        bx = mx + (mw - LARG) / 2;
    else if (ALINHAMENTO == DIREITA)  bx = mx + mw - LARG - MARGEM_LATERAL;
    else                              bx = mx + MARGEM_LATERAL;

    at.override_redirect = True;      /* sem moldura do xfwm4, posicao exata */
    at.background_pixel  = FACE;
    at.event_mask        = ExposureMask | ButtonPressMask;

    win = XCreateWindow(dpy, DefaultRootWindow(dpy), bx, by, LARG, ALTURA, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, win, 0, NULL);

    XMapRaised(dpy, win);

    for (;;) {
        fd_set fds;
        struct timeval tv;
        int fd = ConnectionNumber(dpy);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.type == Expose) {
                desenhar();
            } else if (ev.type == ButtonPress) {
                int i;
                for (i = 0; i < N_ITENS; i++) {
                    Item *it = &itens[i];
                    if (it->acao && ev.xbutton.x >= it->x &&
                        ev.xbutton.x < it->x + it->larg) {
                        it->acao();
                        break;
                    }
                }
            }
        }

        /* acorda a cada 10 s so para o relogio; fora disso fica bloqueado */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        if (select(fd + 1, &fds, NULL, NULL, &tv) == 0)
            desenhar();
    }
    return 0;
}
