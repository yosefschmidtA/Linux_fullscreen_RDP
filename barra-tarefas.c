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

enum { BOTAO, RELOGIO, BOTAO_USB };

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

/* A barra, da esquerda para a direita. Sem lista de janelas de proposito - o
 * caminho de volta para janela minimizada continua sendo o Alt+Tab
 * (cycle_hidden=true no xfwm4.xml).
 *
 * Os dois BOTAO_USB passam o headset e a webcam entre o Windows e esta sessao.
 * Sao separados de proposito: bundle-los obrigaria a tudo-ou-nada, e o caso util
 * e justamente audio nativo aqui (mata os estalos do sink do xrdp) com a camera
 * ainda no Windows para o Meet. A largura cabe o rotulo mais longo
 * ("Camera: Linux") para a barra nao mudar de tamanho ao alternar. */
static Item itens[] = {
    { BOTAO_USB, "Audio",    92, NULL,          "audio",  0, 0 },
    { BOTAO_USB, "Camera",   92, NULL,          "camera", 0, 0 },
    { RELOGIO,   NULL,       52, NULL,          NULL,     0, 0 },
    { BOTAO,     "Desligar", 66, acao_desligar, NULL,     0, 0 },
};
#define N_ITENS ((int) (sizeof itens / sizeof itens[0]))

/* Estado lido do cache que o transferir-usb escreve. A barra NAO chama o
 * usbipd.exe: uma chamada de interop leva quase um segundo e travaria o
 * desenho. */
static char est_audio[24]  = "?";
static char est_camera[24] = "?";

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
        else if (!strncmp(linha, "camera=", 7))
            snprintf(est_camera, sizeof est_camera, "%.20s", linha + 7);
    }
    fclose(f);
}

/* "Audio: Linux" quando esta aqui, "Audio: Win" quando esta no Windows,
 * "Audio: --" quando falta o 'usbipd bind' ou o aparelho esta desligado. */
static void rotulo_usb(const Item *it, char *fora, size_t n)
{
    const char *e = !strcmp(it->dev, "audio") ? est_audio : est_camera;
    const char *lado;

    if (it->pendente)              lado = "...";
    else if (!strcmp(e, "linux"))   lado = "Linux";
    else if (!strcmp(e, "windows")) lado = "Win";
    else                            lado = "--";

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
        } else if (it->tipo == BOTAO_USB) {
            char r[48];
            rotulo_usb(it, r, sizeof r);
            levantado(it->x, PAD, it->larg, ALTURA - 2 * PAD);
            texto(cx, ALTURA / 2, r, fonte);
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
    int i, algum_pendente = 0;

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

    for (i = 0; i < N_ITENS; i++)
        if (itens[i].pendente)
            algum_pendente = 1;

    /* o transferir-usb reescreve o cache ao terminar; e o nosso sinal de fim */
    if (algum_pendente && cache_mtime() != mt_ref)
        for (i = 0; i < N_ITENS; i++)
            itens[i].pendente = 0;

    strftime(hm, sizeof hm, "%H:%M", localtime(&t));
    snprintf(agora, sizeof agora, "%s|%s|%s|%d",
             hm, est_audio, est_camera, algum_pendente);

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
    at.event_mask        = ExposureMask | ButtonPressMask;

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

            if (ev.type == rr_base + RRScreenChangeNotify) {
                /* obrigatorio: sem isto o Xlib segue com a tela antiga em cache
                 * e o reposicionamento usa geometria velha */
                XRRUpdateConfiguration(&ev);
                reposicionar();
                desenhar();
            } else if (ev.type == Expose) {
                desenhar();
            } else if (ev.type == ButtonPress) {
                int i;
                for (i = 0; i < N_ITENS; i++) {
                    Item *it = &itens[i];

                    if (ev.xbutton.x < it->x ||
                        ev.xbutton.x >= it->x + it->larg)
                        continue;

                    if (it->tipo == BOTAO_USB) {
                        char cmd[128];
                        snprintf(cmd, sizeof cmd,
                                 "transferir-usb %s alternar", it->dev);
                        mt_ref = cache_mtime();
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
