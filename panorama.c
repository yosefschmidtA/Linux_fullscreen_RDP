/* panorama - aperta Win e ve tudo que esta aberto, com a cara de cada janela.
 *
 *   gcc -O2 -Wall -o panorama panorama.c $(pkg-config --cflags xft) \
 *       -lX11 -lXft -lXi -lXrandr -lXcomposite -lXrender
 *   DISPLAY=:10 ./panorama &
 *
 * POR QUE ELE EXISTE
 *
 * Esta sessao nao tem lista de janelas em lugar nenhum - a barra-tarefas.c nao
 * tem uma de proposito, para nao virar painel. Ate 03/08/2026 a UNICA forma de
 * trazer de volta uma janela minimizada era o Alt+Tab (e so por causa do
 * cycle_hidden=true no xfwm4.xml; ver README, "Minimizar sem barra de
 * tarefas"). Alt+Tab e cego: mostra uma janela por vez e nao diz o que existe.
 *
 * Aqui e o gesto do GNOME: toca a tecla Win e aparece a grade do que esta
 * aberto, minimizado ou nao, cada um com a propria imagem; escolhe e a janela
 * volta. A barra continua sem lista de janelas - a lista so existe enquanto se
 * olha para ela.
 *
 * A TECLA WIN SOZINHA: POR QUE NAO E UM ATALHO DO XFWM4
 *
 * Atalho do xfce4-keyboard-shortcuts precisa de uma COMBINACAO. Uma tecla
 * modificadora sozinha nao serve: o passive grab do X em Super_L sem
 * modificador captura a tecla no instante em que ela desce, e ai o Super+Left do
 * tiling nunca mais chegaria ao xfwm4 - o grab tem um dono so, e essa disputa e
 * exatamente o bug do "<Super>Right morto" que este projeto perseguiu por dias
 * (README, "A regra que faltava: uma tecla por acao").
 *
 * A saida e nao pegar grab nenhum: XInput2 entrega eventos CRUS de teclado
 * (XI_RawKeyPress/XI_RawKeyRelease) a quem pedir, sem tirar a tecla de ninguem.
 * Medido em 03/08/2026 nesta sessao: XI 2.4 no servidor, xrdpKeyboard como
 * dispositivo escravo, e o keycode 115 (Super_L) chega intacto - o mstsc
 * REPASSA a tecla Win em vez de engoli-la no Windows.
 *
 * A regra do gesto: abre no RELEASE do Super, e so se nenhuma outra tecla nem
 * botao do mouse tiver descido no meio, e se o toque durou menos de 800 ms.
 * Assim Super+Left continua sendo tiling, Super+D continua mostrando a area de
 * trabalho, e segurar a tecla nao dispara nada.
 *
 * O auto-repeat pede cuidado: medido no mesmo dia, o xrdp entrega repeticao
 * como pares RELEASE+PRESS, e nao como PRESS seguido. Segurar o Super poderia,
 * entao, parecer um toque. Por isso um PRESS do proprio Super estando ja armado
 * DESARMA (e repeticao, nao toque), alem do limite de tempo.
 *
 * A MINIATURA, E POR QUE ELA PRECISA DO COMPOSITE
 *
 * Medido em 03/08/2026, com duas janelas nossas controladas (uma coberta 100%
 * pela outra) e depois com as janelas reais da sessao:
 *
 *   XGetImage numa janela COBERTA devolve ZEROS - nem o conteudo dela, nem o de
 *   quem esta por cima. Um terminal meio encoberto pela bancada saiu com a
 *   metade de cima certa e o resto preto. (E a mesma armadilha do "import
 *   -window mente quando a janela esta coberta" ja registrada no README; agora
 *   se sabe o que exatamente ela devolve.)
 *
 * A saida e o Composite: XCompositeRedirectSubwindows(root, Automatic) faz cada
 * janela de primeiro nivel passar a ter o proprio pixmap fora da tela, e o
 * servidor continua compondo a tela sozinho - nao viramos compositor, nao
 * mexemos no xfwm4, e o use_compositing dele continua false. Com isso a captura
 * sai inteira mesmo coberta. Medido:
 *
 *   desenho de 2000 retangulos, 4 alternancias: sem redirect 26-48 ms,
 *   com redirect 18-29 ms. Nunca ficou MAIS lento - o desenho de uma janela
 *   parcialmente coberta deixa de precisar do recorte contra as vizinhas.
 *   RAM do Xorg: +16 MB com a sessao inteira redirecionada.
 *
 * IMPORTANTE: esse benchmark mede o caminho cliente->servidor. O caminho
 * servidor->xrdp->rede NAO foi medido isoladamente; o que se sabe e que a
 * sessao continua fluida em uso normal.
 *
 * A escala e feita pelo XRender, DENTRO do servidor: 0,22 ms por miniatura,
 * contra ~15 ms do caminho "XGetImage + reamostrar na CPU" (que ainda arrastaria
 * 11 MB de pixel pelo socket a cada janela). E por isso que da para gerar todas
 * as miniaturas no instante em que o painel abre.
 *
 * QUEM E A JANELA: O FRAME, NAO O CLIENTE
 *
 * O _NET_CLIENT_LIST da a janela do CLIENTE, que o xfwm4 reparenta para dentro
 * de um frame com a barra de titulo. Quem e filho do root - e portanto quem o
 * redirect alcanca - e o FRAME. Nomear o pixmap da janela do cliente da
 * BadMatch. Por isso o ate_o_root(): sobe a arvore ate o filho do root. De
 * brinde, a miniatura sai com a barra de titulo, que e como a janela se parece.
 *
 * O [x] DE FECHAR
 *
 * Cada cartao tem um [x] no canto, e so o cartao sob o mouse o mostra - um [x]
 * em cada cartao o tempo todo vira campo minado num painel feito para ser
 * clicado depressa. Ele manda um _NET_CLOSE_WINDOW, que e o pedido EDUCADO: o
 * gerenciador entrega um WM_DELETE_WINDOW ao programa, e quem tem trabalho por
 * salvar pergunta antes. Nada de XKillClient, que derruba a conexao e perde o
 * que estiver aberto sem dizer nada.
 *
 * O cartao NAO some no clique: quem manda tira-lo e o PropertyNotify do
 * _NET_CLIENT_LIST, quando o gerenciador confirma que a janela morreu. O
 * programa pode demorar, pode perguntar e pode ate recusar. Do mesmo evento vem
 * de graca o caso oposto: janela que nasce com o painel aberto entra na grade
 * sozinha, e as outras deslizam para abrir espaco.
 *
 * MINIMIZADA NAO TEM PIXMAP
 *
 * Ao desmapear, o servidor libera o pixmap - e minimizada e justamente o caso
 * que mais importa aqui. Por isso existe o cache: a miniatura de cada janela
 * fica guardada, e a da janela ATIVA e refrescada 2 s depois da ultima atividade
 * de teclado ou mouse (que este programa ja recebe de graca pelos eventos crus).
 * Parado, nao acorda: sem atividade, o select nao tem timeout nenhum. Assim, o
 * que se ve de uma janela minimizada e como ela estava pouco antes de sumir.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_JAN      64
#define LADO         32            /* icone grande, no lugar da miniatura */
#define ICONE_LEG    16            /* icone pequeno, na legenda do cartao */
#define MINI_W       360           /* largura da miniatura guardada no cache */
#define ESPACO       14
#define LEGENDA_H    30
#define CABECALHO_H  30
#define RODAPE_H     22
#define MARGEM       10
#define CARTAO_MAX   340
#define CARTAO_MIN   150
#define TOQUE_MS     800           /* acima disso foi segurar, nao tocar */
#define ANIM_MS      190           /* duracao de uma reorganizacao */
#define ATRASO_MS    22            /* escalonamento entre cartoes, na entrada */
#define QUADRO_MS    22
#define REFRESCO_MS  2000          /* silencio antes de refrescar a ativa */
#define FECHAR_W     20            /* lado do botao [x] no canto do cartao */

static Display *dpy;
static int      tela;
static Window   raiz, pop;         /* pop = 0 quando fechado */
static GC       gc;
static Visual  *vis;
static Colormap cm;
static int      prof;
static int      xi_op;
static KeyCode  kc_super_l, kc_super_r;
static XRenderPictFormat *fmt_tela;

static XftFont *fonte, *fonte_p;
static XftDraw *dr;
static XftColor c_tinta, c_fraco, c_sel_tinta, c_sel_fraco;
static unsigned long P_FUNDO, P_CAB, P_SEL, P_BORDA, P_CARTAO, P_VAZIO,
                     P_FECHAR;
static unsigned long P_LETRA[8];

static const unsigned char RGB_CARTAO[3] = { 0x33, 0x33, 0x33 };
static const unsigned char RGB_SEL[3]    = { 0x26, 0x4F, 0x78 };

static Atom at_lista, at_lista_pilha, at_estado, at_oculta, at_pular_barra,
            at_tipo, at_tipo_dock, at_tipo_desktop, at_nome, at_utf8,
            at_icone, at_ativa, at_desktop, at_desktop_atual,
            at_selecao, at_mostrar, at_fechar_jan;

/* A miniatura sobrevive ao fechamento do painel e a minimizacao da janela: e
 * justamente isso que a torna util. Vive no servidor, como Pixmap. */
typedef struct {
    Window w;
    Pixmap mini;
    int    mw, mh;
} Cache;
static Cache cache[MAX_JAN];
static int   n_cache;

typedef struct {
    Window         w, frame;
    char           titulo[256];
    char           classe[64];
    int            oculta, ativa, outro_desktop;
    unsigned char *px;             /* icone RGBA, ou NULL */
    Pixmap         mini;
    int            mw, mh;
    /* animacao: onde o cartao esta e para onde vai */
    double         x, y, w_at, h_at;
    double         ax, ay, aw, ah;
    double         t0;             /* quando este cartao comeca a se mover */
    int            entrando;
} Jan;

static Jan  jan[MAX_JAN];
static int  n_jan;
static int  vis_idx[MAX_JAN];
static int  n_vis, sel, colunas;
static int  pt_x = -1, pt_y = -1;   /* onde o ponteiro esta, para o [x] */
static char termo[48];
static int  pop_w, pop_h;
static Pixmap buf;                 /* fundo duplo: nada e desenhado na tela */

static double anim_ate;            /* enquanto agora() < isto, esta animando */
static double atividade, capturado;

static volatile sig_atomic_t pedido_sinal;

static double agora(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000.0 + t.tv_usec / 1000.0;
}

/* Mexemos em janelas de outros programas, que podem morrer entre a listagem e a
 * consulta; e o Composite responde BadMatch para janela que saiu de cena. Com o
 * tratador padrao do Xlib qualquer um desses MATA o processo - e este e um
 * daemon que precisa atravessar o dia. */
static int erro_x(Display *d, XErrorEvent *e)
{
    (void) d; (void) e;
    return 0;
}

static void ao_sinal(int s)
{
    (void) s;
    pedido_sinal = 1;
}

static unsigned long cor(const char *spec)
{
    XColor c;
    XParseColor(dpy, cm, spec, &c);
    XAllocColor(dpy, cm, &c);
    return c.pixel;
}

static unsigned char *prop(Window w, Atom p, Atom tipo, unsigned long *n)
{
    Atom          real;
    int           formato;
    unsigned long resto;
    unsigned char *dados = NULL;

    *n = 0;
    if (XGetWindowProperty(dpy, w, p, 0, 1L << 22, False, tipo,
                           &real, &formato, n, &resto, &dados) != Success)
        return NULL;
    if (real == None || !dados) {
        if (dados) XFree(dados);
        *n = 0;
        return NULL;
    }
    return dados;
}

static int tem_estado(Window w, Atom estado)
{
    unsigned long n, i;
    Atom *e = (Atom *) prop(w, at_estado, XA_ATOM, &n);
    int achou = 0;

    for (i = 0; e && i < n; i++)
        if (e[i] == estado) { achou = 1; break; }
    if (e) XFree(e);
    return achou;
}

/* ---------------------------------------------------------------- titulos */

static int utf8_valido(const char *s)
{
    const unsigned char *p = (const unsigned char *) s;

    while (*p) {
        int n;
        if (*p < 0x80)                n = 0;
        else if ((*p & 0xE0) == 0xC0) n = 1;
        else if ((*p & 0xF0) == 0xE0) n = 2;
        else if ((*p & 0xF8) == 0xF0) n = 3;
        else                          return 0;
        p++;
        while (n--) {
            if ((*p & 0xC0) != 0x80) return 0;
            p++;
        }
    }
    return 1;
}

static void latin1_para_utf8(const char *ent, char *fora, size_t n)
{
    const unsigned char *p = (const unsigned char *) ent;
    size_t o = 0;

    while (*p && o + 3 < n) {
        if (*p < 0x80) {
            fora[o++] = (char) *p;
        } else {
            fora[o++] = (char) (0xC0 | (*p >> 6));
            fora[o++] = (char) (0x80 | (*p & 0x3F));
        }
        p++;
    }
    fora[o] = '\0';
}

/* Tira do fim um caractere UTF-8 que ficou pela metade: titulo de pagina passa
 * dos 255 bytes com facilidade e o corte cai no meio de um "—". */
static void podar_utf8(char *s)
{
    size_t k = strlen(s);
    int n;

    while (k > 0 && ((unsigned char) s[k - 1] & 0xC0) == 0x80)
        k--;
    if (k == 0)
        return;
    {
        unsigned char c = (unsigned char) s[k - 1];
        if      (c < 0x80)           n = 1;
        else if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else                         n = 1;
    }
    if (k - 1 + (size_t) n > strlen(s))
        s[k - 1] = '\0';
}

/* O UTF-8 do Xft nao perdoa byte invalido. Nem todo mundo publica _NET_WM_NAME
 * (a bancada e o terminal deste projeto so tem WM_NAME, do tipo STRING, que
 * pelo padrao e latin-1 mas quase sempre vem em UTF-8): se ja for UTF-8 valido
 * passa direto, se nao for e tratado como latin-1 e convertido. */
static void titulo_de(Window w, char *fora, size_t n)
{
    unsigned long qtd;
    unsigned char *t;

    fora[0] = '\0';
    t = prop(w, at_nome, at_utf8, &qtd);
    if (!t)
        t = prop(w, XA_WM_NAME, AnyPropertyType, &qtd);
    if (t) {
        char bruto[512];
        snprintf(bruto, sizeof bruto, "%.500s", (char *) t);
        XFree(t);
        if (utf8_valido(bruto))
            snprintf(fora, n, "%.*s", (int) n - 1, bruto);
        else
            latin1_para_utf8(bruto, fora, n);
    }
    if (!fora[0])
        snprintf(fora, n, "(sem titulo)");
    podar_utf8(fora);
}

static void classe_de(Window w, char *fora, size_t n)
{
    XClassHint ch = { NULL, NULL };

    fora[0] = '\0';
    if (XGetClassHint(dpy, w, &ch)) {
        snprintf(fora, n, "%s", ch.res_class ? ch.res_class :
                                (ch.res_name ? ch.res_name : ""));
        if (ch.res_name)  XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
    }
    if (!fora[0])
        snprintf(fora, n, "?");
}

/* ------------------------------------------------------------------ icone */

/* _NET_WM_ICON e uma lista de imagens ARGB, uma atras da outra: largura,
 * altura, largura*altura pixels, repete. Pegamos a menor que ainda seja >= LADO.
 * Cuidado com o formato 32 do X: o XGetWindowProperty devolve array de `long`,
 * que aqui tem 8 bytes - ler como uint32_t desalinha tudo. */
static unsigned char *icone_de(Window w)
{
    unsigned long n, i;
    unsigned long *d = (unsigned long *) prop(w, at_icone, XA_CARDINAL, &n);
    unsigned long melhor = 0, mw = 0, mh = 0;
    unsigned char *px;
    int x, y;

    if (!d)
        return NULL;

    for (i = 0; i + 2 <= n; ) {
        unsigned long iw = d[i], ih = d[i + 1];

        if (!iw || !ih || iw > 1024 || ih > 1024) break;
        if (i + 2 + iw * ih > n) break;
        if (!mw || (mw < LADO && iw > mw) ||
            (iw >= LADO && (mw < LADO || iw < mw))) {
            melhor = i + 2; mw = iw; mh = ih;
        }
        i += 2 + iw * ih;
    }
    if (!mw) { XFree(d); return NULL; }

    px = malloc((size_t) LADO * LADO * 4);
    if (!px) { XFree(d); return NULL; }

    for (y = 0; y < LADO; y++)
        for (x = 0; x < LADO; x++) {
            unsigned long v = d[melhor + (y * mh / LADO) * mw + (x * mw / LADO)];
            unsigned char *q = px + ((size_t) y * LADO + x) * 4;
            q[0] = (v >> 16) & 0xFF;
            q[1] = (v >> 8)  & 0xFF;
            q[2] = v & 0xFF;
            q[3] = (v >> 24) & 0xFF;
        }
    XFree(d);
    return px;
}

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

/* Sem compositor proprio nao ha alpha na tela: a transparencia do icone e
 * misturada AQUI contra a cor de fundo, na CPU. Sao 32x32 pixels. */
static void por_icone(Drawable alvo, int x0, int y0, int lado,
                      const unsigned char *px, const unsigned char *fundo)
{
    XImage *img;
    int x, y;

    img = XCreateImage(dpy, vis, prof, ZPixmap, 0, NULL, lado, lado, 32, 0);
    if (!img)
        return;
    img->data = calloc((size_t) img->bytes_per_line, lado);
    if (!img->data) { XDestroyImage(img); return; }

    for (y = 0; y < lado; y++)
        for (x = 0; x < lado; x++) {
            const unsigned char *q =
                px + ((size_t) (y * LADO / lado) * LADO + x * LADO / lado) * 4;
            unsigned a = q[3];
            unsigned r = (q[0] * a + fundo[0] * (255 - a)) / 255;
            unsigned g = (q[1] * a + fundo[1] * (255 - a)) / 255;
            unsigned b = (q[2] * a + fundo[2] * (255 - a)) / 255;
            XPutPixel(img, x, y, componente(vis->red_mask,   r) |
                                 componente(vis->green_mask, g) |
                                 componente(vis->blue_mask,  b));
        }
    XPutImage(dpy, alvo, gc, img, 0, 0, x0, y0, lado, lado);
    XDestroyImage(img);
}

/* Quem nao publica icone ganha um quadrado com a inicial, colorido por hash da
 * classe: e sempre a mesma cor para o mesmo programa. */
static void quadro_de_letra(Drawable alvo, int x0, int y0, int lado,
                            const char *classe)
{
    unsigned h = 5381;
    const char *p;
    char inicial[2];
    XGlyphInfo g;
    XftFont *f = lado >= LADO ? fonte : fonte_p;

    for (p = classe; *p; p++)
        h = h * 33 + (unsigned char) *p;

    XSetForeground(dpy, gc, P_LETRA[h % (sizeof P_LETRA / sizeof *P_LETRA)]);
    XFillRectangle(dpy, alvo, gc, x0, y0, lado, lado);

    inicial[0] = (char) (classe[0] >= 'a' && classe[0] <= 'z'
                         ? classe[0] - 32 : classe[0]);
    inicial[1] = '\0';
    XftTextExtentsUtf8(dpy, f, (FcChar8 *) inicial, 1, &g);
    XftDrawStringUtf8(dr, &c_sel_tinta, f, x0 + (lado - g.xOff) / 2,
                      y0 + (lado + f->ascent - f->descent) / 2,
                      (FcChar8 *) inicial, 1);
}

/* -------------------------------------------------------------- miniatura */

/* Sobe a arvore ate o filho do root: e o frame do xfwm4, e e ele que o
 * redirect do Composite alcanca. */
static Window ate_o_root(Window w)
{
    Window r, pai, *filhos;
    unsigned n;
    int voltas = 0;

    while (voltas++ < 16 && XQueryTree(dpy, w, &r, &pai, &filhos, &n)) {
        if (filhos) XFree(filhos);
        if (!pai || pai == r)
            return w;
        w = pai;
    }
    return w;
}

static Cache *achar_cache(Window w)
{
    int i;
    for (i = 0; i < n_cache; i++)
        if (cache[i].w == w)
            return &cache[i];
    return NULL;
}

/* Escala o pixmap da janela para o cache, inteirinho dentro do servidor: 0,22 ms
 * por miniatura, sem trazer um pixel sequer para este processo. */
static void atualizar_mini(Window cliente)
{
    Window frame = ate_o_root(cliente);
    XWindowAttributes a;
    Pixmap origem;
    Picture po, pd;
    XTransform t = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
    XRenderPictFormat *pf;
    Cache *c;
    int mw, mh;

    if (!XGetWindowAttributes(dpy, frame, &a) || a.map_state != IsViewable)
        return;                       /* minimizada: fica a do cache */
    if (a.width < 8 || a.height < 8)
        return;

    origem = XCompositeNameWindowPixmap(dpy, frame);
    if (!origem)
        return;

    mw = MINI_W;
    mh = mw * a.height / a.width;
    if (mh < 1) mh = 1;
    if (mh > MINI_W * 2) { mh = MINI_W * 2; }

    c = achar_cache(cliente);
    if (!c && n_cache < MAX_JAN) {
        c = &cache[n_cache++];
        c->w = cliente; c->mini = 0;
    }
    if (!c) { XFreePixmap(dpy, origem); return; }

    if (c->mini && (c->mw != mw || c->mh != mh)) {
        XFreePixmap(dpy, c->mini);
        c->mini = 0;
    }
    if (!c->mini) {
        c->mini = XCreatePixmap(dpy, raiz, mw, mh, prof);
        c->mw = mw; c->mh = mh;
    }

    pf = XRenderFindVisualFormat(dpy, a.visual);
    if (!pf) pf = fmt_tela;
    po = XRenderCreatePicture(dpy, origem, pf, 0, NULL);
    pd = XRenderCreatePicture(dpy, c->mini, fmt_tela, 0, NULL);
    t.matrix[0][0] = XDoubleToFixed((double) a.width  / mw);
    t.matrix[1][1] = XDoubleToFixed((double) a.height / mh);
    t.matrix[2][2] = XDoubleToFixed(1.0);
    XRenderSetPictureTransform(dpy, po, &t);
    XRenderSetPictureFilter(dpy, po, "good", NULL, 0);
    XRenderComposite(dpy, PictOpSrc, po, None, pd, 0, 0, 0, 0, 0, 0, mw, mh);
    XRenderFreePicture(dpy, po);
    XRenderFreePicture(dpy, pd);
    XFreePixmap(dpy, origem);
}

/* Desenha a miniatura do cache no tamanho pedido, mantendo a proporcao. E aqui
 * que a animacao acontece: o tamanho muda a cada quadro e o XRender reescala. */
static void por_mini(Drawable alvo, Pixmap mini, int mw, int mh,
                     int x, int y, int w, int h)
{
    Picture po, pd;
    XTransform t = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};

    if (w < 1 || h < 1)
        return;
    po = XRenderCreatePicture(dpy, mini, fmt_tela, 0, NULL);
    pd = XRenderCreatePicture(dpy, alvo, fmt_tela, 0, NULL);
    t.matrix[0][0] = XDoubleToFixed((double) mw / w);
    t.matrix[1][1] = XDoubleToFixed((double) mh / h);
    t.matrix[2][2] = XDoubleToFixed(1.0);
    XRenderSetPictureTransform(dpy, po, &t);
    XRenderSetPictureFilter(dpy, po, "good", NULL, 0);
    XRenderComposite(dpy, PictOpSrc, po, None, pd, 0, 0, 0, 0, x, y, w, h);
    XRenderFreePicture(dpy, po);
    XRenderFreePicture(dpy, pd);
}

/* ---------------------------------------------------------------- listagem */

static void soltar_lista(void)
{
    int i;
    for (i = 0; i < n_jan; i++) {
        free(jan[i].px);
        jan[i].px = NULL;
    }
    n_jan = 0;
}

/* Janela que fechou leva a miniatura junto - senao o cache seguraria pixmaps de
 * janelas mortas ate o fim da sessao. */
static void podar_cache(const Window *vivas, unsigned long n)
{
    int i;
    unsigned long k;

    for (i = 0; i < n_cache; ) {
        int viva = 0;
        for (k = 0; k < n; k++)
            if (vivas[k] == cache[i].w) { viva = 1; break; }
        if (viva) { i++; continue; }
        if (cache[i].mini) XFreePixmap(dpy, cache[i].mini);
        cache[i] = cache[--n_cache];
    }
}

static int casa(const Jan *j)
{
    char alvo[320], t[48];
    unsigned i;

    if (!termo[0])
        return 1;
    snprintf(alvo, sizeof alvo, "%s %s", j->classe, j->titulo);
    for (i = 0; alvo[i]; i++)
        if (alvo[i] >= 'A' && alvo[i] <= 'Z') alvo[i] += 32;
    for (i = 0; termo[i] && i < sizeof t - 1; i++)
        t[i] = (termo[i] >= 'A' && termo[i] <= 'Z') ? termo[i] + 32 : termo[i];
    t[i] = '\0';
    return strstr(alvo, t) != NULL;
}

static void listar(void)
{
    unsigned long n, i;
    Window *w;
    unsigned long *dsk_atual;
    long atual_desktop = 0;
    Window ativa = 0;
    unsigned long qtd;
    Window *a;

    soltar_lista();

    w = (Window *) prop(raiz, at_lista_pilha, XA_WINDOW, &n);
    if (!w)
        w = (Window *) prop(raiz, at_lista, XA_WINDOW, &n);
    if (!w)
        return;

    podar_cache(w, n);

    a = (Window *) prop(raiz, at_ativa, XA_WINDOW, &qtd);
    if (a) { if (qtd) ativa = a[0]; XFree(a); }

    dsk_atual = (unsigned long *) prop(raiz, at_desktop_atual, XA_CARDINAL, &qtd);
    if (dsk_atual) { if (qtd) atual_desktop = (long) dsk_atual[0]; XFree(dsk_atual); }

    /* De tras para a frente na pilha: o topo primeiro. Nao e o historico de uso
     * de verdade (o EWMH nao guarda isso), mas na pratica e quase. */
    for (i = n; i-- > 0 && n_jan < MAX_JAN; ) {
        Window u = w[i];
        unsigned long qn;
        Atom *tipo;
        int pular = 0;
        unsigned long *d;
        Cache *c;

        tipo = (Atom *) prop(u, at_tipo, XA_ATOM, &qn);
        if (tipo) {
            unsigned long k;
            for (k = 0; k < qn; k++)
                if (tipo[k] == at_tipo_dock || tipo[k] == at_tipo_desktop)
                    pular = 1;
            XFree(tipo);
        }
        if (pular || tem_estado(u, at_pular_barra))
            continue;

        memset(&jan[n_jan], 0, sizeof jan[n_jan]);
        jan[n_jan].w = u;
        jan[n_jan].frame = ate_o_root(u);
        titulo_de(u, jan[n_jan].titulo, sizeof jan[n_jan].titulo);
        classe_de(u, jan[n_jan].classe, sizeof jan[n_jan].classe);
        jan[n_jan].oculta = tem_estado(u, at_oculta);
        jan[n_jan].ativa  = (u == ativa);
        jan[n_jan].px     = icone_de(u);

        d = (unsigned long *) prop(u, at_desktop, XA_CARDINAL, &qn);
        if (d) {
            if (qn && (long) d[0] != atual_desktop && (long) d[0] != -1)
                jan[n_jan].outro_desktop = 1;
            XFree(d);
        }

        atualizar_mini(u);           /* 0,22 ms; de graca em cima da listagem */
        c = achar_cache(u);
        if (c && c->mini) {
            jan[n_jan].mini = c->mini;
            jan[n_jan].mw = c->mw;
            jan[n_jan].mh = c->mh;
        }
        n_jan++;
    }
    XFree(w);
}

/* ------------------------------------------------------------------ layout */

/* Quantas colunas? A grade quer ficar parecida com a tela: quadrada demais
 * desperdicia altura, larga demais deixa os cartoes minusculos. */
static void dispor(int animar)
{
    double t = agora();
    int i, linhas, cw, ch, largura_grade, altura_grade, x0, y0;

    if (n_vis <= 0)
        return;

    colunas = (int) ceil(sqrt((double) n_vis));
    if (colunas < 1) colunas = 1;
    linhas = (n_vis + colunas - 1) / colunas;

    cw = (pop_w - MARGEM * 2 - ESPACO * (colunas - 1)) / colunas;
    ch = (pop_h - CABECALHO_H - RODAPE_H - MARGEM * 2 - ESPACO * (linhas - 1))
         / linhas;
    if (cw > CARTAO_MAX) cw = CARTAO_MAX;
    if (ch > CARTAO_MAX) ch = CARTAO_MAX;

    largura_grade = colunas * cw + (colunas - 1) * ESPACO;
    altura_grade  = linhas  * ch + (linhas  - 1) * ESPACO;
    x0 = (pop_w - largura_grade) / 2;
    y0 = CABECALHO_H + (pop_h - CABECALHO_H - RODAPE_H - altura_grade) / 2;

    for (i = 0; i < n_vis; i++) {
        Jan *j = &jan[vis_idx[i]];
        int lin = i / colunas, col = i % colunas;
        int sobra = (i / colunas == linhas - 1)
                    ? (largura_grade - ((n_vis - 1) % colunas + 1) * cw
                       - ((n_vis - 1) % colunas) * ESPACO) / 2
                    : 0;

        j->ax = x0 + col * (cw + ESPACO) + (lin == linhas - 1 ? sobra : 0);
        j->ay = y0 + lin * (ch + ESPACO);
        j->aw = cw;
        j->ah = ch;
        j->t0 = t + (animar ? i * ATRASO_MS : 0);

        if (!animar) {
            j->x = j->ax; j->y = j->ay; j->w_at = j->aw; j->h_at = j->ah;
        }
    }
    if (animar)
        anim_ate = t + ANIM_MS + (double) n_vis * ATRASO_MS;
}

static void refiltrar(int animar)
{
    int i;
    Window antes = (sel >= 0 && sel < n_vis) ? jan[vis_idx[sel]].w : 0;

    n_vis = 0;
    for (i = 0; i < n_jan; i++)
        if (casa(&jan[i]))
            vis_idx[n_vis++] = i;

    /* Manter escolhida a MESMA janela quando ela continua na lista: sem isto,
     * apagar uma letra do filtro pularia a selecao para outra janela. */
    sel = 0;
    for (i = 0; i < n_vis; i++)
        if (jan[vis_idx[i]].w == antes) { sel = i; break; }
    if (sel >= n_vis) sel = n_vis - 1;
    if (sel < 0)      sel = 0;

    dispor(animar);
}

/* --------------------------------------------------------------- desenho */

static int cortar(const char *s, int max_px, char *fora, size_t n, XftFont *f)
{
    XGlyphInfo g;
    size_t i = 0, ultimo_ok = 0;

    XftTextExtentsUtf8(dpy, f, (FcChar8 *) s, strlen(s), &g);
    if (g.xOff <= max_px) {
        snprintf(fora, n, "%s", s);
        return g.xOff;
    }
    while (s[i] && i < n - 4) {
        do { i++; } while ((s[i] & 0xC0) == 0x80);
        XftTextExtentsUtf8(dpy, f, (FcChar8 *) s, i, &g);
        if ((int) g.xOff > max_px - 12) break;
        ultimo_ok = i;
    }
    memcpy(fora, s, ultimo_ok);
    fora[ultimo_ok] = '\0';
    strncat(fora, "...", n - strlen(fora) - 1);
    XftTextExtentsUtf8(dpy, f, (FcChar8 *) fora, strlen(fora), &g);
    return g.xOff;
}

static void texto(int x, int y, const char *s, XftColor *c, XftFont *f)
{
    XftDrawStringUtf8(dr, c, f, x, y, (FcChar8 *) s, strlen(s));
}

/* Suaviza a chegada: rapido no comeco, devagar no fim. E o que faz a
 * reorganizacao parecer um movimento e nao um salto. */
static double suave(double p)
{
    if (p <= 0) return 0;
    if (p >= 1) return 1;
    return 1 - pow(1 - p, 3);
}

static void desenhar(void)
{
    char linha[400], cab[96];
    double t = agora();
    int i;

    if (!pop)
        return;

    XSetForeground(dpy, gc, P_FUNDO);
    XFillRectangle(dpy, buf, gc, 0, 0, pop_w, pop_h);
    XSetForeground(dpy, gc, P_CAB);
    XFillRectangle(dpy, buf, gc, 0, 0, pop_w, CABECALHO_H);

    if (termo[0]) snprintf(cab, sizeof cab, "Janelas: %s", termo);
    else          snprintf(cab, sizeof cab, "Janelas abertas");
    texto(MARGEM, (CABECALHO_H + fonte->ascent - fonte->descent) / 2,
          cab, &c_tinta, fonte);
    snprintf(cab, sizeof cab, "%d", n_vis);
    {
        XGlyphInfo g;
        XftTextExtentsUtf8(dpy, fonte, (FcChar8 *) cab, strlen(cab), &g);
        texto(pop_w - MARGEM - g.xOff,
              (CABECALHO_H + fonte->ascent - fonte->descent) / 2,
              cab, &c_fraco, fonte);
    }

    for (i = 0; i < n_vis; i++) {
        Jan *j = &jan[vis_idx[i]];
        double p = suave((t - j->t0) / ANIM_MS);
        int x, y, w, h, mini_h, ix, iy, iw, ih;
        const unsigned char *fundo = (i == sel) ? RGB_SEL : RGB_CARTAO;

        /* Entrando: nasce menor e cresce no lugar. Reorganizando: caminha da
         * posicao velha para a nova. As duas coisas saem da mesma conta. */
        if (j->entrando) {
            double e = 0.90 + 0.10 * p;
            w = (int) (j->aw * e);
            h = (int) (j->ah * e);
            x = (int) (j->ax + (j->aw - w) / 2);
            y = (int) (j->ay + (j->ah - h) / 2);
            if (p >= 1) j->entrando = 0;
        } else {
            x = (int) (j->x + (j->ax - j->x) * p);
            y = (int) (j->y + (j->ay - j->y) * p);
            w = (int) (j->w_at + (j->aw - j->w_at) * p);
            h = (int) (j->h_at + (j->ah - j->h_at) * p);
            if (p >= 1) {
                j->x = j->ax; j->y = j->ay;
                j->w_at = j->aw; j->h_at = j->ah;
            }
        }
        if (w < 2 || h < 2)
            continue;

        XSetForeground(dpy, gc, i == sel ? P_SEL : P_CARTAO);
        XFillRectangle(dpy, buf, gc, x, y, w, h);

        mini_h = h - LEGENDA_H;
        if (mini_h > 4) {
            XSetForeground(dpy, gc, P_VAZIO);
            XFillRectangle(dpy, buf, gc, x + 3, y + 3, w - 6, mini_h - 3);

            if (j->mini) {
                /* Letterbox: a miniatura guarda a proporcao da janela real, que
                 * quase nunca e a do cartao. Esticar deformaria a imagem e
                 * tiraria justamente o que faz reconhecer a janela de relance. */
                iw = w - 6; ih = iw * j->mh / j->mw;
                if (ih > mini_h - 6) { ih = mini_h - 6; iw = ih * j->mw / j->mh; }
                ix = x + (w - iw) / 2;
                iy = y + 3 + (mini_h - 3 - ih) / 2;
                por_mini(buf, j->mini, j->mw, j->mh, ix, iy, iw, ih);
            } else if (mini_h > LADO + 12) {
                /* Sem miniatura ainda (janela que nasceu minimizada, ou que
                 * nunca esteve visivel desde que este daemon subiu). */
                if (j->px)
                    por_icone(buf, x + (w - LADO) / 2,
                              y + 3 + (mini_h - 3 - LADO) / 2, LADO, j->px,
                              (const unsigned char *) "\x1e\x1e\x1e");
                else
                    quadro_de_letra(buf, x + (w - LADO) / 2,
                                    y + 3 + (mini_h - 3 - LADO) / 2, LADO,
                                    j->classe);
            }
        }

        /* Legenda: icone pequeno + titulo. */
        if (h >= LEGENDA_H) {
            int tx = x + 6, ty = y + h - LEGENDA_H;
            int cabe = w - 12;

            if (w > 60) {
                if (j->px)
                    por_icone(buf, tx, ty + (LEGENDA_H - ICONE_LEG) / 2,
                              ICONE_LEG, j->px, fundo);
                else
                    quadro_de_letra(buf, tx, ty + (LEGENDA_H - ICONE_LEG) / 2,
                                    ICONE_LEG, j->classe);
                tx += ICONE_LEG + 6;
                cabe -= ICONE_LEG + 6;
            }
            cortar(j->titulo, cabe, linha, sizeof linha, fonte_p);
            texto(tx, ty + (LEGENDA_H + fonte_p->ascent - fonte_p->descent) / 2,
                  linha, i == sel ? &c_sel_tinta : &c_tinta, fonte_p);
        }

        if (j->oculta || j->outro_desktop) {
            const char *m = j->oculta ? "minimizada" : "outra area";
            XGlyphInfo g;
            XftTextExtentsUtf8(dpy, fonte_p, (FcChar8 *) m, strlen(m), &g);
            if (g.xOff + 12 < w) {
                XSetForeground(dpy, gc, P_CAB);
                XFillRectangle(dpy, buf, gc, x + w - g.xOff - 12, y + 4,
                               g.xOff + 8, fonte_p->height + 4);
                texto(x + w - g.xOff - 8, y + 6 + fonte_p->ascent, m,
                      &c_sel_fraco, fonte_p);
            }
        }

        /* O [x] so aparece no cartao sob o mouse, como no GNOME: um [x] em cada
         * cartao o tempo todo vira um campo minado num painel que existe para
         * ser clicado depressa. */
        if (i == sel && w > 90 && h > 70) {
            int bx = x + w - FECHAR_W - 6, by = y + 6;
            int quente = (pt_x >= bx && pt_x < bx + FECHAR_W &&
                          pt_y >= by && pt_y < by + FECHAR_W);
            int m = 5;

            XSetForeground(dpy, gc, quente ? P_FECHAR : P_CAB);
            XFillRectangle(dpy, buf, gc, bx, by, FECHAR_W, FECHAR_W);
            XSetForeground(dpy, gc, P_BORDA);
            XDrawRectangle(dpy, buf, gc, bx, by, FECHAR_W - 1, FECHAR_W - 1);
            XSetForeground(dpy, gc, cor("#F0F0F0"));
            XDrawLine(dpy, buf, gc, bx + m, by + m,
                      bx + FECHAR_W - 1 - m, by + FECHAR_W - 1 - m);
            XDrawLine(dpy, buf, gc, bx + FECHAR_W - 1 - m, by + m,
                      bx + m, by + FECHAR_W - 1 - m);
        }

        XSetForeground(dpy, gc, i == sel ? P_SEL : P_BORDA);
        XDrawRectangle(dpy, buf, gc, x, y, w - 1, h - 1);
        if (i == sel)
            XDrawRectangle(dpy, buf, gc, x + 1, y + 1, w - 3, h - 3);
    }

    if (n_vis == 0)
        texto(MARGEM, pop_h / 2,
              n_jan ? "nada casa com o que foi digitado"
                    : "nenhuma janela aberta", &c_fraco, fonte);

    XSetForeground(dpy, gc, P_BORDA);
    XDrawRectangle(dpy, buf, gc, 0, 0, pop_w - 1, pop_h - 1);
    XDrawLine(dpy, buf, gc, 1, pop_h - RODAPE_H, pop_w - 2, pop_h - RODAPE_H);
    texto(MARGEM, pop_h - RODAPE_H + (RODAPE_H + fonte_p->ascent) / 2 - 2,
          "Enter abre  ·  setas escolhem  ·  Del ou [x] fecha a janela  ·  digite para filtrar  ·  Esc sai",
          &c_fraco, fonte_p);

    /* Um XCopyArea so: a tela nunca ve um quadro pela metade. */
    XCopyArea(dpy, buf, pop, gc, 0, 0, pop_w, pop_h, 0, 0);
    XFlush(dpy);
}

/* ------------------------------------------------------------ abrir/fechar */

static void monitor_do_ponteiro(int *mx, int *my, int *mw, int *mh)
{
    Window r1, r2;
    int px, py, wx, wy, i, n = 0;
    unsigned mask;
    XRRMonitorInfo *mons;

    *mx = 0; *my = 0;
    *mw = DisplayWidth(dpy, tela);
    *mh = DisplayHeight(dpy, tela);

    if (!XQueryPointer(dpy, raiz, &r1, &r2, &px, &py, &wx, &wy, &mask))
        return;
    mons = XRRGetMonitors(dpy, raiz, True, &n);
    if (!mons)
        return;
    for (i = 0; i < n; i++)
        if (px >= mons[i].x && px < mons[i].x + mons[i].width &&
            py >= mons[i].y && py < mons[i].y + mons[i].height) {
            *mx = mons[i].x; *my = mons[i].y;
            *mw = mons[i].width; *mh = mons[i].height;
            break;
        }
    XRRFreeMonitors(mons);
}

static void fechar(void)
{
    if (!pop)
        return;
    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XSelectInput(dpy, raiz, NoEventMask);
    pt_x = pt_y = -1;
    if (dr)  { XftDrawDestroy(dr); dr = NULL; }
    if (buf) { XFreePixmap(dpy, buf); buf = 0; }
    XDestroyWindow(dpy, pop);
    pop = 0;
    anim_ate = 0;
    soltar_lista();
    termo[0] = '\0';
    XFlush(dpy);
}

/* A fonte so e carregada na PRIMEIRA abertura. Enquanto ninguem tocar a tecla,
 * este processo e um Xlib com um socket e mais nada - o fontconfig, que e o
 * pedaco caro, nem foi tocado. */
static int preparar_visual(void)
{
    XRenderColor rc;

    if (fonte)
        return 1;

    fonte   = XftFontOpenName(dpy, tela, "DejaVu Sans:size=10");
    fonte_p = XftFontOpenName(dpy, tela, "DejaVu Sans:size=8");
    if (!fonte)   fonte   = XftFontOpenName(dpy, tela, "sans:size=10");
    if (!fonte_p) fonte_p = fonte;
    if (!fonte) {
        fprintf(stderr, "panorama: nenhuma fonte Xft\n");
        return 0;
    }

    P_FUNDO  = cor("#242424");
    P_CAB    = cor("#3A3A3A");
    P_SEL    = cor("#264F78");
    P_BORDA  = cor("#101010");
    P_CARTAO = cor("#333333");
    P_VAZIO  = cor("#1E1E1E");
    P_FECHAR = cor("#C0392B");   /* o [x] sob o mouse */

    {
        static const char *paleta[] = { "#7E57C2", "#26A69A", "#EF6C00",
                                        "#42A5F5", "#AB47BC", "#66BB6A",
                                        "#EC407A", "#8D6E63" };
        unsigned i;
        for (i = 0; i < sizeof P_LETRA / sizeof *P_LETRA; i++)
            P_LETRA[i] = cor(paleta[i]);
    }

#define ALOC(hex, dst) do { \
        XColor c; XParseColor(dpy, cm, hex, &c); \
        rc.red = c.red; rc.green = c.green; rc.blue = c.blue; rc.alpha = 0xFFFF; \
        XftColorAllocValue(dpy, vis, cm, &rc, (dst)); } while (0)
    ALOC("#E5E5E5", &c_tinta);
    ALOC("#9A9A9A", &c_fraco);
    ALOC("#FFFFFF", &c_sel_tinta);
    ALOC("#B9CBDF", &c_sel_fraco);
#undef ALOC
    return 1;
}

static void abrir(void)
{
    XSetWindowAttributes at;
    int mx, my, mw, mh, i, linhas;

    if (!preparar_visual())
        return;

    listar();
    termo[0] = '\0';
    monitor_do_ponteiro(&mx, &my, &mw, &mh);

    n_vis = 0;
    for (i = 0; i < n_jan; i++)
        vis_idx[n_vis++] = i;

    /* Comeca na SEGUNDA janela: a primeira e aquela em que se esta agora, e
     * escolher a janela em que ja se esta nao serve para nada. Assim "Win,
     * Enter" alterna para a anterior, como o Alt+Tab. */
    sel = (n_vis > 1) ? 1 : 0;

    /* O painel tem tamanho FIXO enquanto estiver aberto, calculado para caber
     * todas as janelas. Filtrar nao encolhe a janela: os cartoes e que se
     * reorganizam e crescem dentro dela - que e de onde vem o movimento. */
    colunas = n_vis ? (int) ceil(sqrt((double) n_vis)) : 1;
    if (colunas < 1) colunas = 1;
    linhas = n_vis ? (n_vis + colunas - 1) / colunas : 1;

    pop_w = MARGEM * 2 + colunas * CARTAO_MAX + (colunas - 1) * ESPACO;
    pop_h = CABECALHO_H + RODAPE_H + MARGEM * 2 +
            linhas * (CARTAO_MAX * 3 / 4) + (linhas - 1) * ESPACO;
    if (pop_w > mw * 88 / 100) pop_w = mw * 88 / 100;
    if (pop_h > mh * 88 / 100) pop_h = mh * 88 / 100;
    if (pop_w < 420) pop_w = 420;
    if (pop_h < 240) pop_h = 240;

    dispor(0);
    for (i = 0; i < n_vis; i++)
        jan[vis_idx[i]].entrando = 1;
    anim_ate = agora() + ANIM_MS + (double) n_vis * ATRASO_MS;

    at.override_redirect = True;
    at.background_pixel  = P_FUNDO;
    at.event_mask        = ExposureMask | ButtonPressMask | PointerMotionMask |
                           KeyPressMask;
    pop = XCreateWindow(dpy, raiz,
                        mx + (mw - pop_w) / 2, my + (mh - pop_h) / 2,
                        pop_w, pop_h, 0, CopyFromParent, InputOutput,
                        CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &at);
    buf = XCreatePixmap(dpy, raiz, pop_w, pop_h, prof);
    dr  = XftDrawCreate(dpy, buf, vis, cm);
    XMapRaised(dpy, pop);

    /* owner_events False no ponteiro: TODO clique vem para ca, inclusive o de
     * fora do painel - e e assim que ele fecha ao clicar em qualquer lugar. O
     * teclado precisa de grab porque janela override_redirect nao recebe foco
     * do xfwm4; se o grab falhar, o painel ainda anda no mouse. */
    XGrabPointer(dpy, pop, False,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XGrabKeyboard(dpy, pop, False, GrabModeAsync, GrabModeAsync, CurrentTime);

    /* Enquanto o painel estiver aberto, queremos saber quando uma janela nasce
     * ou morre - e e assim que o cartao some depois do [x]. Fora daqui a
     * mascara e desligada: o root muda de propriedade o tempo todo (foco, area
     * de trabalho, lista de janelas) e um daemon parado nao pode acordar a cada
     * uma delas. */
    XSelectInput(dpy, raiz, PropertyChangeMask);
    desenhar();
}

/* Pedir ao gerenciador de janelas, em vez de mexer na janela: o
 * _NET_ACTIVE_WINDOW faz o xfwm4 desminimizar, trocar de area de trabalho se for
 * o caso, levantar e dar foco - o que um XMapWindow cru nao faria. */
static void ativar(Window w)
{
    XEvent e;

    memset(&e, 0, sizeof e);
    e.xclient.type         = ClientMessage;
    e.xclient.window       = w;
    e.xclient.message_type = at_ativa;
    e.xclient.format       = 32;
    e.xclient.data.l[0]    = 2;              /* origem: pager */
    e.xclient.data.l[1]    = CurrentTime;
    XSendEvent(dpy, raiz, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
    XFlush(dpy);
}

static void escolher(int i)
{
    Window w;

    if (i < 0 || i >= n_vis) { fechar(); return; }
    w = jan[vis_idx[i]].w;
    fechar();
    ativar(w);
}

/* Pedido EDUCADO de fechamento: o _NET_CLOSE_WINDOW passa pelo gerenciador de
 * janelas, que manda o WM_DELETE_WINDOW ao programa - quem tem trabalho por
 * salvar pergunta antes, como faria no botao [x] da barra de titulo. Nada de
 * XKillClient aqui: aquilo derruba a conexao do cliente e perde o que estiver
 * aberto sem dizer nada.
 *
 * O dialogo de "salvar?" que aparecer fica ATRAS do painel, que esta com o
 * ponteiro e o teclado capturados - some com o Esc, e o dialogo esta la. */
static void fechar_janela(int i)
{
    XEvent e;

    if (i < 0 || i >= n_vis)
        return;

    memset(&e, 0, sizeof e);
    e.xclient.type         = ClientMessage;
    e.xclient.window       = jan[vis_idx[i]].w;
    e.xclient.message_type = at_fechar_jan;
    e.xclient.format       = 32;
    e.xclient.data.l[0]    = CurrentTime;
    e.xclient.data.l[1]    = 2;              /* origem: pager */
    XSendEvent(dpy, raiz, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
    XFlush(dpy);
    /* Nao se mexe na lista aqui: quem manda e o PropertyNotify do
     * _NET_CLIENT_LIST. O programa pode demorar, pode perguntar antes, e pode
     * ate recusar - tirar o cartao na hora do clique seria mentir sobre isso. */
}

/* Relista com o painel ABERTO, guardando onde cada cartao esta para que os
 * sobreviventes DESLIZEM para o lugar novo em vez de saltar. E o que se ve
 * quando uma janela e fechada pelo [x]: as outras se reorganizam. */
static void relistar_vivo(void)
{
    struct { Window w; double x, y, cw, ch; } antes[MAX_JAN];
    int n_antes = 0, i, k;
    Window escolhida = (sel >= 0 && sel < n_vis) ? jan[vis_idx[sel]].w : 0;

    for (i = 0; i < n_vis && n_antes < MAX_JAN; i++) {
        Jan *j = &jan[vis_idx[i]];
        antes[n_antes].w  = j->w;
        antes[n_antes].x  = j->entrando ? j->ax : j->x;
        antes[n_antes].y  = j->entrando ? j->ay : j->y;
        antes[n_antes].cw = j->entrando ? j->aw : j->w_at;
        antes[n_antes].ch = j->entrando ? j->ah : j->h_at;
        n_antes++;
    }

    listar();
    if (n_jan == 0) {                 /* fechou a ultima: nao ha o que mostrar */
        fechar();
        return;
    }

    n_vis = 0;
    for (i = 0; i < n_jan; i++)
        if (casa(&jan[i]))
            vis_idx[n_vis++] = i;
    sel = 0;
    for (i = 0; i < n_vis; i++)
        if (jan[vis_idx[i]].w == escolhida) { sel = i; break; }
    if (sel >= n_vis) sel = n_vis - 1;
    if (sel < 0)      sel = 0;

    dispor(1);
    for (i = 0; i < n_vis; i++) {
        Jan *j = &jan[vis_idx[i]];
        j->entrando = 1;              /* janela nova nasce crescendo */
        for (k = 0; k < n_antes; k++)
            if (antes[k].w == j->w) {
                j->x = antes[k].x; j->y = antes[k].y;
                j->w_at = antes[k].cw; j->h_at = antes[k].ch;
                j->entrando = 0;      /* ja existia: desliza de onde estava */
                break;
            }
    }
    desenhar();
}

static void alternar(void)
{
    if (pop) fechar();
    else     abrir();
}

/* --------------------------------------------------------------- eventos */

static void mover(int d)
{
    if (!n_vis)
        return;
    sel += d;
    if (sel < 0)      sel = n_vis - 1;
    if (sel >= n_vis) sel = 0;
    desenhar();
}

static void tecla(XKeyEvent *ev)
{
    char buf_t[16];
    KeySym ks;
    int n;

    n = XLookupString(ev, buf_t, sizeof buf_t - 1, &ks, NULL);

    switch (ks) {
    case XK_Escape:                    fechar();               return;
    case XK_Return: case XK_KP_Enter:  escolher(sel);          return;
    case XK_Left:                      mover(-1);              return;
    case XK_Right: case XK_Tab:        mover(1);               return;
    case XK_Up:                        mover(-colunas);        return;
    case XK_Down:                      mover(colunas);         return;
    case XK_ISO_Left_Tab:              mover(-1);              return;
    case XK_Delete:                    fechar_janela(sel);     return;
    case XK_Home: sel = 0;             desenhar();             return;
    case XK_End:  sel = n_vis ? n_vis - 1 : 0; desenhar();     return;
    case XK_BackSpace:
        if (termo[0]) {
            size_t k = strlen(termo);
            do { k--; } while (k > 0 && (termo[k] & 0xC0) == 0x80);
            termo[k] = '\0';
            refiltrar(1);
            desenhar();
        }
        return;
    }

    /* Filtrar digitando, como no GNOME. O que nao e imprimivel cai fora aqui -
     * inclusive o proprio Super, que chega como KeyPress quando e apertado de
     * novo para fechar o painel. */
    if (n > 0 && (unsigned char) buf_t[0] >= 32 &&
        strlen(termo) + n < sizeof termo - 1) {
        buf_t[n] = '\0';
        strcat(termo, buf_t);
        refiltrar(1);
        desenhar();
    }
}

/* Qual cartao esta em (x,y)? Percorre ao contrario para que, durante a
 * animacao, o de cima na pilha de desenho ganhe. */
static int cartao_em(int x, int y)
{
    int i;

    for (i = n_vis - 1; i >= 0; i--) {
        Jan *j = &jan[vis_idx[i]];
        if (x >= j->ax && x < j->ax + j->aw && y >= j->ay && y < j->ay + j->ah)
            return i;
    }
    return -1;
}

/* A area do [x] sai das coordenadas de DESTINO do cartao, nao das animadas: no
 * meio de uma reorganizacao o alvo e onde o cartao vai parar, e clicar num alvo
 * parado e mais previsivel do que perseguir o desenho. */
static int no_fechar(int i, int x, int y)
{
    Jan *j;
    int bx, by;

    if (i < 0 || i >= n_vis)
        return 0;
    j = &jan[vis_idx[i]];
    if (j->aw <= 90 || j->ah <= 70)
        return 0;
    bx = (int) (j->ax + j->aw) - FECHAR_W - 6;
    by = (int) j->ay + 6;
    return x >= bx && x < bx + FECHAR_W && y >= by && y < by + FECHAR_W;
}

static void clique(XButtonEvent *ev)
{
    int i;

    if (ev->x < 0 || ev->y < 0 || ev->x >= pop_w || ev->y >= pop_h) {
        fechar();
        return;
    }
    if (ev->button == 4 || ev->button == 5) {
        mover(ev->button == 4 ? -1 : 1);
        return;
    }
    i = cartao_em(ev->x, ev->y);
    if (i < 0)
        return;
    /* O [x] fica DENTRO do cartao, entao ele tem de ser testado antes - senao o
     * clique nele abriria a janela em vez de fecha-la. */
    if (i == sel && no_fechar(i, ev->x, ev->y)) {
        fechar_janela(i);
        return;
    }
    escolher(i);
}

static void movimento(XMotionEvent *ev)
{
    int i = cartao_em(ev->x, ev->y);
    int antes = no_fechar(sel, pt_x, pt_y);

    pt_x = ev->x;
    pt_y = ev->y;

    if (i >= 0 && i != sel) {
        sel = i;
        desenhar();
        return;
    }
    /* Redesenho so quando o ponteiro cruza a borda do [x]: mover o mouse dentro
     * do mesmo cartao nao pode repintar o painel a cada pixel - cada quadro
     * daqui e CPU comprimindo e TCP transmitindo. */
    if (antes != no_fechar(sel, pt_x, pt_y))
        desenhar();
}

/* A maquina de estados do "Win sozinho". Ver o cabecalho do arquivo. */
static void raw(XIRawEvent *r, int tipo)
{
    static int  armado;
    static Time t_armado;
    KeyCode kc = (KeyCode) r->detail;
    int e_super = (kc == kc_super_l || kc == kc_super_r);

    atividade = agora();

    if (tipo == XI_RawButtonPress) { armado = 0; return; }

    if (tipo == XI_RawKeyPress) {
        /* PRESS do proprio Super estando ja armado e REPETICAO (o xrdp entrega
         * auto-repeat como RELEASE+PRESS): quem segura a tecla nao esta dando
         * um toque nela. */
        if (e_super && !armado) { armado = 1; t_armado = r->time; }
        else                      armado = 0;
        return;
    }

    if (tipo == XI_RawKeyRelease && armado && e_super) {
        armado = 0;
        if (r->time - t_armado <= TOQUE_MS)
            alternar();
    }
}

/* Refresca a miniatura da janela ativa depois de um tempo de silencio. E o que
 * faz uma janela minimizada aparecer como estava pouco antes de sumir. */
static void refrescar_ativa(void)
{
    unsigned long qtd;
    Window *a = (Window *) prop(raiz, at_ativa, XA_WINDOW, &qtd);

    capturado = agora();
    if (!a)
        return;
    if (qtd && a[0])
        atualizar_mini(a[0]);
    XFree(a);
}

int main(int argc, char **argv)
{
    int ev_base, err_base, maj = 2, min = 2;
    unsigned char mascara[XIMaskLen(XI_LASTEVENT)];
    XIEventMask em;
    Window dono;

    (void) argc; (void) argv;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "panorama: nao abriu o display\n");
        return 1;
    }
    XSetErrorHandler(erro_x);

    tela = DefaultScreen(dpy);
    raiz = RootWindow(dpy, tela);
    vis  = DefaultVisual(dpy, tela);
    cm   = DefaultColormap(dpy, tela);
    prof = DefaultDepth(dpy, tela);
    gc   = XCreateGC(dpy, raiz, 0, NULL);
    fmt_tela = XRenderFindVisualFormat(dpy, vis);

    at_lista        = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    at_lista_pilha  = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    at_estado       = XInternAtom(dpy, "_NET_WM_STATE", False);
    at_oculta       = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    at_pular_barra  = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    at_tipo         = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    at_tipo_dock    = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    at_tipo_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    at_nome         = XInternAtom(dpy, "_NET_WM_NAME", False);
    at_utf8         = XInternAtom(dpy, "UTF8_STRING", False);
    at_icone        = XInternAtom(dpy, "_NET_WM_ICON", False);
    at_ativa        = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    at_desktop      = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    at_desktop_atual= XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    at_selecao      = XInternAtom(dpy, "_PANORAMA_S0", False);
    at_mostrar      = XInternAtom(dpy, "_PANORAMA_MOSTRAR", False);
    at_fechar_jan   = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);

    /* Instancia unica, com uma selecao do X em vez de arquivo de pid: dois
     * daemons escutando a mesma tecla abririam DOIS paineis, um por cima do
     * outro, e o de baixo ficaria com o grab preso. Se ja houver dono, esta
     * chamada vira "mostre agora" e sai - e assim `panorama` na linha de comando
     * (ou num .desktop, ou num botao da barra) tambem serve de gatilho. */
    dono = XGetSelectionOwner(dpy, at_selecao);
    if (dono != None) {
        XEvent e;
        memset(&e, 0, sizeof e);
        e.xclient.type         = ClientMessage;
        e.xclient.window       = dono;
        e.xclient.message_type = at_mostrar;
        e.xclient.format       = 32;
        XSendEvent(dpy, dono, False, NoEventMask, &e);
        XFlush(dpy);
        XCloseDisplay(dpy);
        return 0;
    }
    {
        XSetWindowAttributes at;
        Window marca;
        at.override_redirect = True;
        at.event_mask = StructureNotifyMask;
        marca = XCreateWindow(dpy, raiz, -10, -10, 1, 1, 0, CopyFromParent,
                              InputOnly, CopyFromParent,
                              CWOverrideRedirect | CWEventMask, &at);
        XSetSelectionOwner(dpy, at_selecao, marca, CurrentTime);
    }

    if (!XQueryExtension(dpy, "XInputExtension", &xi_op, &ev_base, &err_base) ||
        XIQueryVersion(dpy, &maj, &min) != Success) {
        fprintf(stderr, "panorama: sem XInput2; a tecla Win nao vai funcionar\n");
        return 1;
    }

    /* O redirecionamento e o que da miniatura de janela coberta. Automatic: o
     * servidor continua compondo a tela sozinho - nao viramos compositor e o
     * use_compositing do xfwm4 continua false. Se outro cliente ja tiver
     * redirecionado (um compositor de verdade), isto da BadAccess, o tratador
     * engole, e o painel segue funcionando com icone no lugar da miniatura. */
    {
        int cev, cerr;
        if (XCompositeQueryExtension(dpy, &cev, &cerr))
            XCompositeRedirectSubwindows(dpy, raiz, CompositeRedirectAutomatic);
        else
            fprintf(stderr, "panorama: sem Composite; sem miniatura\n");
    }

    kc_super_l = XKeysymToKeycode(dpy, XK_Super_L);
    kc_super_r = XKeysymToKeycode(dpy, XK_Super_R);

    /* Eventos CRUS na raiz: chegam sem grab nenhum, entao o Super continua
     * inteiro para o xfwm4 (Super+Left, Super+D) e para o xfsettingsd
     * (Super+R). Este programa observa, nao intercepta. */
    memset(mascara, 0, sizeof mascara);
    XISetMask(mascara, XI_RawKeyPress);
    XISetMask(mascara, XI_RawKeyRelease);
    XISetMask(mascara, XI_RawButtonPress);
    em.deviceid = XIAllMasterDevices;
    em.mask_len = sizeof mascara;
    em.mask     = mascara;
    XISelectEvents(dpy, raiz, &em, 1);

    XkbSetDetectableAutoRepeat(dpy, True, NULL);
    XFlush(dpy);

    /* Plano B de gatilho: pkill -USR1 -x panorama
     *
     * Com sigaction SEM SA_RESTART de proposito. O signal() da glibc liga o
     * SA_RESTART sozinho, e ai o read() de dentro do Xlib seria REINICIADO
     * depois do sinal: o select abaixo nunca voltaria e o SIGUSR1 so teria
     * efeito no proximo evento do X - tecla morta ate alguem mexer o mouse. */
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = ao_sinal;
        sigaction(SIGUSR1, &sa, NULL);
    }

    for (;;) {
        fd_set fds;
        struct timeval tv, *ptv = NULL;
        int fd = ConnectionNumber(dpy);
        double t;

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            if (ev.xcookie.type == GenericEvent &&
                ev.xcookie.extension == xi_op) {
                if (XGetEventData(dpy, &ev.xcookie)) {
                    if (ev.xcookie.evtype == XI_RawKeyPress ||
                        ev.xcookie.evtype == XI_RawKeyRelease ||
                        ev.xcookie.evtype == XI_RawButtonPress)
                        raw((XIRawEvent *) ev.xcookie.data, ev.xcookie.evtype);
                    XFreeEventData(dpy, &ev.xcookie);
                }
                continue;
            }

            switch (ev.type) {
            case ClientMessage:
                if (ev.xclient.message_type == at_mostrar) alternar();
                break;
            case Expose:
                if (pop && ev.xany.window == pop) desenhar();
                break;
            case KeyPress:
                if (pop) tecla(&ev.xkey);
                break;
            case ButtonPress:
                if (pop) clique(&ev.xbutton);
                break;
            case MotionNotify:
                if (pop) movimento(&ev.xmotion);
                break;
            case PropertyNotify:
                /* Janela fechou (ou nasceu) com o painel aberto: quem avisa e o
                 * gerenciador de janelas, mexendo na lista. Nao se tira o cartao
                 * no clique do [x] - o programa pode perguntar antes de sair, e
                 * pode ate recusar. */
                if (pop && ev.xproperty.window == raiz &&
                    (ev.xproperty.atom == at_lista ||
                     ev.xproperty.atom == at_lista_pilha))
                    relistar_vivo();
                break;
            }
        }

        if (pedido_sinal) {
            pedido_sinal = 0;
            alternar();
            continue;
        }

        /* Tres motivos para acordar, e so tres: um quadro de animacao, o
         * refresco da miniatura da janela ativa depois de um tempo de silencio,
         * ou nada - e "nada" quer dizer select sem timeout, sem gastar CPU. */
        t = agora();
        if (pop && anim_ate > t) {
            tv.tv_sec = 0; tv.tv_usec = QUADRO_MS * 1000;
            ptv = &tv;
        } else if (!pop && atividade > capturado) {
            double falta = REFRESCO_MS - (t - atividade);
            if (falta <= 0) {
                refrescar_ativa();
                continue;
            }
            tv.tv_sec = (long) (falta / 1000);
            tv.tv_usec = (long) ((falta - tv.tv_sec * 1000) * 1000);
            ptv = &tv;
        }

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        XFlush(dpy);
        select(fd + 1, &fds, NULL, NULL, ptv);

        if (pop && anim_ate > agora())
            desenhar();
        else if (pop && anim_ate) {
            anim_ate = 0;
            desenhar();               /* quadro final, ja nos alvos exatos */
        }
    }
    return 0;
}
