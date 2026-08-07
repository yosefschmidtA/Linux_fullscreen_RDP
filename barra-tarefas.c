/* Barra de tarefas arcaica para a sessao fullscreen, no visual do dialogo de
 * login do xrdp (veja Untitled.png).
 *
 *   gcc -O2 -Wall -o barra-tarefas barra-tarefas.c -lX11 -lXrandr
 *   DISPLAY=:10 ./barra-tarefas &
 *
 * O QUE SOBROU AQUI, E POR QUE (07/08/2026)
 *
 * Relogio e desligar. Mais nada.
 *
 * Ate 06/08/2026 esta barra tinha tambem os dois controles de volume, os botoes
 * de audio e camera, o menu da pasta Coisas, os atalhos de aplicativo e o "[+]"
 * — cerca de 700 linhas, tres menus suspensos e uma janela override_redirect
 * propria para desenha-los. Tudo isso mudou para a DOCA do panorama, a tela que
 * a tecla Win abre (veja "a doca" no panorama.c).
 *
 * A razao e de espaco e de uso, nao de codigo. Aqui ha 28px de altura e uma
 * faixa colada na borda de baixo, onde o ponteiro passa o tempo todo; la ha uma
 * tela inteira que abre no meio, ja com as miniaturas das janelas. Um icone que
 * precisava caber em 20px agora tem 32, e o gesto ficou um so: aperta Win, ve
 * tudo, escolhe. O que continua tendo de ser visto SEM abrir nada — as horas —
 * e o que continua tendo de ser alcancavel sem depender de gesto nenhum — o
 * desligar — ficaram.
 *
 * O efeito colateral que importa: esta barra nao pergunta mais nada a ninguem.
 * Ela chamava o pactl quatro vezes a cada 2 s e o `transferir-usb estado` a cada
 * 30 s, para sempre. Agora o unico motivo de ela acordar e o minuto virar.
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
 * Escreva a funcao de acao e ponha uma linha em "fixos". A largura da barra e
 * calculada a partir dela; nada mais precisa mudar.
 *
 * Mas pense duas vezes antes: o lugar de um controle novo provavelmente e a
 * doca do panorama, que tem espaco, Xft (logo, acento) e um menu pronto. Esta
 * barra e o que precisa ser visto sem gesto nenhum.
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
static unsigned long FACE, LUZ, SOMBRA, BORDA, TINTA, POCO;
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
static void tick(void);
static void desenhar(void);
static void levantado(int x, int y, int w, int h);
static void gravado(int x, int y, int w, int h);
static void primario(int *px, int *py, int *pw, int *ph);
static void anunciar_dock(void);
static void aplicar_strut(int bx, int my, int mh);
static void dicas_de_tamanho(void);
static void reposicionar(void);

/* A barra, da esquerda para a direita. Sem lista de janelas de proposito - o
 * caminho de volta para janela minimizada continua sendo o Alt+Tab
 * (cycle_hidden=true no xfwm4.xml), e agora tambem o painel da tecla Win, que
 * mostra todas elas com miniatura. */
static Item itens[] = {
    { RELOGIO, NULL,       58, NULL,          0 },
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
    linha(x, y, x1, y, LUZ);                       /* topo */
    linha(x, y, x, y1, LUZ);                       /* esquerda */
    linha(x, y1, x1, y1, BORDA);                   /* baixo, externo */
    linha(x1, y, x1, y1, BORDA);                   /* direita, externo */
    linha(x + 1, y1 - 1, x1 - 1, y1 - 1, SOMBRA);  /* baixo, interno */
    linha(x1 - 1, y + 1, x1 - 1, y1 - 1, SOMBRA);  /* direita, interno */
}

static void gravado(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y1 = y + h - 1;

    /* O preenchimento e POCO, nao LUZ. Sao papeis diferentes que na paleta
     * clara calhavam de ser a mesma cor (#FFFFFF): "bisel claro" e "fundo de
     * campo afundado". No escuro eles se separam - um campo afundado tem de
     * ficar MAIS ESCURO que a face, e nao mais claro. Era o unico lugar onde a
     * troca de tema nao era so trocar constante. */
    XSetForeground(dpy, gc, POCO);
    XFillRectangle(dpy, win, gc, x, y, w, h);
    linha(x, y, x1, y, SOMBRA);
    linha(x, y, x, y1, SOMBRA);
    linha(x + 1, y + 1, x1 - 1, y + 1, BORDA);
    linha(x + 1, y + 1, x + 1, y1 - 1, BORDA);
    linha(x, y1, x1, y1, LUZ);
    linha(x1, y, x1, y1, LUZ);
}

static void texto(int cx, int cy, const char *s, XFontStruct *f)
{
    int w = XTextWidth(f, s, strlen(s));

    XSetFont(dpy, gc, f->fid);
    XSetForeground(dpy, gc, TINTA);
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

/* Redesenha so quando o que aparece na tela muda - ou seja, quando o minuto
 * vira. Sem isto, redesenhar a cada acordada faria a barra piscar de leve (nao
 * ha duplo buffer aqui). */
static void tick(void)
{
    static char antes[8] = "";
    char hm[8];
    time_t t = time(NULL);

    strftime(hm, sizeof hm, "%H:%M", localtime(&t));
    if (strcmp(hm, antes) != 0) {
        desenhar();
        snprintf(antes, sizeof antes, "%s", hm);
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
 * mesmo critério que o abrir-windows usa para numerar monitores (esquerda para a
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
    for (i = 0; i < N_ITENS; i++)
        LARG += itens[i].larg + (i ? GAP : 0);

    for (i = 0; i < N_ITENS; i++) {
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

/* Poe a barra no lugar certo do monitor primario. Chamada no arranque e a cada
 * mudanca de monitor - o abrir-windows encolhe a sessao para um monitor so
 * enquanto a coisa esta aberta, e sem isto a barra ficaria fora da tela ate o
 * proximo login. Tambem cobre monitor plugado/desplugado e reconexao com layout
 * novo. */
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
                                  * quando o abrir-windows encolhe a sessao */
}

int main(void)
{
    XSetWindowAttributes at;
    int rr_base, rr_err;

    signal(SIGCHLD, SIG_IGN);       /* nao deixar zumbi do linux-desktop-down */

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "barra: nao abriu o display\n");
        return 1;
    }

    /* DUAS PALETAS, seis papeis. O escuro entrou em 03/08/2026, a pedido de quem
     * usa, depois que o painel do panorama chegou nesse tom: a barra era a
     * ultima coisa clara numa sessao que ficou escura inteira.
     *
     * A paleta clara NAO foi apagada de proposito. Ela nao e gosto: saiu pixel a
     * pixel do Untitled.png, o dialogo de login do xrdp, para a barra parecer
     * parte da mesma sessao (ver README, "A paleta e o bisel, amostrados").
     * Trocar BARRA_ESCURA para 0 devolve aquele visual inteiro.
     *
     * O bisel Motif continua o mesmo, e e ele que carrega o "arcaico": borda
     * assimetrica, sem meio-tom, sem cantos redondos. So os tons mudaram. */
#define BARRA_ESCURA 1
#if BARRA_ESCURA
    FACE   = cor("#333333");   /* face dos botoes; o mesmo tom do cartao do panorama */
    LUZ    = cor("#4E4E4E");   /* bisel claro */
    SOMBRA = cor("#1A1A1A");   /* sombra interna */
    BORDA  = cor("#101010");   /* borda externa */
    TINTA  = cor("#E8E8E8");   /* texto */
    POCO   = cor("#1E1E1E");   /* fundo de campo afundado: o relogio */
#else
    FACE   = cor("#DEDEDE");
    LUZ    = cor("#FFFFFF");
    SOMBRA = cor("#808080");
    BORDA  = cor("#000000");
    TINTA  = cor("#000000");
    POCO   = cor("#FFFFFF");
#endif

    fonte = XLoadQueryFont(dpy,
        "-*-helvetica-medium-r-normal--11-*-*-*-*-*-iso8859-1");

    /* O relogio em NEGRITO e maior (13 em vez de 10), a pedido de quem usa em
     * 03/08/2026: e a unica coisa da barra que se le de longe e de relance, e no
     * fundo escuro o traco fino da fixed-medium sumia.
     *
     * A -misc-fixed-bold-13 e monoespacada (o "c" no nome), que e o que importa
     * aqui: com fonte proporcional os digitos mudam de largura e o relogio
     * DANCA de posicao a cada minuto, porque o texto e centralizado. O plano B
     * e a fonte antiga - o negrito e um luxo, e uma barra sem relogio nao e. */
    mono  = XLoadQueryFont(dpy,
        "-*-fixed-bold-r-normal--13-*-*-*-*-c-*-iso8859-1");
    if (!mono)
        mono = XLoadQueryFont(dpy,
            "-*-fixed-medium-r-normal--10-*-*-*-*-*-iso8859-1");
    if (!fonte || !mono) {
        fprintf(stderr, "barra: fonte core do X nao encontrada\n");
        return 1;
    }

    /* SEM override_redirect de proposito - veja "reserva de espaco (EWMH)"
     * acima. A barra precisa ser gerenciada pelo xfwm4 para que o strut valha;
     * o tipo DOCK e quem devolve o "sem moldura, sempre acima, sem foco". */
    at.background_pixel  = FACE;
    at.event_mask        = ExposureMask | ButtonPressMask;

    layout();
    win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, LARG, ALTURA, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, win, 0, NULL);

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

                if (ev.xbutton.button != Button1)
                    continue;

                for (i = 0; i < N_ITENS; i++) {
                    Item *it = &itens[i];

                    if (ev.xbutton.x < it->x ||
                        ev.xbutton.x >= it->x + it->larg)
                        continue;
                    if (it->acao)
                        it->acao();
                    break;
                }
            }
        }

        /* Acorda a cada 2 s e so redesenha quando o minuto vira. Nao ha mais
         * nada a vigiar aqui: volume, USB e atalhos foram para a doca do
         * panorama, e o relogio e a unica coisa que muda sozinha. */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (select(fd + 1, &fds, NULL, NULL, &tv) == 0)
            tick();
    }
    return 0;
}
