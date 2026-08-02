/* terminal — um emulador de terminal escrito aqui, do pty ao pixel.
 *
 *   gcc -O2 -Wall -o terminal terminal.c -lX11 -lXft $(pkg-config --cflags xft)
 *   DISPLAY=:10 ./terminal [-e comando ...]
 *
 * POR QUE ELE EXISTE
 *
 * Por gosto de ter o proprio. Ele NAO substitui o xterm da aba 0 da bancada —
 * aquilo esta resolvido e funcionando, e mexer nele so traria de volta o
 * trabalho das invariantes de janela adotada. Aqui e um binario a parte, que
 * abre a propria janela e roda o proprio shell.
 *
 * Isso muda o alvo, e o alvo e que define quanto parser e preciso escrever.
 * Nao ha compromisso de aguentar a TUI do Claude Code (saida sincronizada,
 * redesenho de tela inteira a cada tecla). O compromisso e com o que se usa num
 * terminal: o shell, `ls`, `git`, `vim`, `htop`, `less`, `ranger`.
 *
 * AS QUATRO PECAS
 *
 *   1. o pty      — posix_openpt + fork; o filho vira lider de sessao e o pty
 *                   vira o terminal de controle dele. ~40 linhas.
 *   2. a grade    — matriz de celulas (codepoint, cor de frente, de fundo,
 *                   atributos) + o historico de rolagem.
 *   3. o parser   — a maquina de estados que le os escapes do pty e mexe na
 *                   grade. E aqui que mora 80% do trabalho.
 *   4. o desenho  — Xft por trecho de mesmo atributo, e o caminho inverso:
 *                   tecla -> bytes para o pty.
 *
 * A GRADE E DE PONTEIROS, NAO DE BYTES
 *
 * `tela` e um vetor de ponteiros para linha, nao um bloco unico. Rolar a tela
 * um passo entao nao copia texto nenhum: gira o vetor de ponteiros, manda a
 * linha que saiu por cima direto para o historico e traz uma limpa por baixo.
 * O historico e um anel dos MESMOS ponteiros — uma linha que rolou para fora
 * nao e copiada, e adotada. Com `less` num arquivo grande isso e a diferenca
 * entre memmove de megabytes e trocar dois enderecos.
 *
 * O CUSTO DO HISTORICO, DITO EM NUMEROS
 *
 * Cada celula sao 16 bytes. O historico padrao de 1000 linhas numa janela de
 * 200 colunas custa 1000 * 200 * 16 = 3,2 MB, e ele so cresce ate o teto. E o
 * maior gasto do programa e e por escolha; `-sl 0` desliga.
 *
 * O CURSOR NAO PISCA, DE PROPOSITO
 *
 * A sessao inteira e RDP. Um cursor piscando e uma atualizacao de tela duas
 * vezes por segundo, para sempre, mesmo com ninguem digitando — o codec vai
 * mandar esse retangulo pela rede a noite toda. Um cursor solido e visivel do
 * mesmo jeito e nao gera trafego. Pelo mesmo motivo nao ha timer nenhum no
 * laco: sem dado no pty e sem evento do X, o processo dorme no select().
 *
 * O QUE ELE NAO TEM, DE PROPOSITO
 *
 *   - abas e divisao de painel: para isso existe o tmux, que ja faz melhor.
 *   - transparencia e compositor: nao ha compositor nesta sessao.
 *   - utmp: exigiria a libutempter so para o `who` enxergar a janela.
 *   - reflow ao mudar de largura: as linhas do historico sao recortadas, nao
 *     redobradas. O xterm tambem nao redobra; o st tambem nao.
 *
 * ARMADILHAS QUE CUSTARAM TEMPO (ver README)
 *
 *   - so a janela pede KeyPressMask, e o XIM e obrigatorio: sem XIC +
 *     XFilterEvent o acento morto do ABNT2 nao compoe.
 *   - o filho precisa de setsid() ANTES do TIOCSCTTY, senao o pty nao vira
 *     terminal de controle e o Ctrl+C nao chega em ninguem.
 *   - read() do pty devolve EIO (nao 0) quando o filho morre, no Linux.
 */

#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

/* ------------------------------------------------------------------ grade */

#define ATR_NEGRITO   0x001
#define ATR_FRACO     0x002
#define ATR_ITALICO   0x004
#define ATR_SUBLINHA  0x008
#define ATR_INVERSO   0x010
#define ATR_INVISIVEL 0x020
#define ATR_RISCADO   0x040
#define ATR_LARGA     0x080  /* a celula da esquerda de um glifo de 2 colunas */
#define ATR_CONT      0x100  /* a celula da direita: nao se desenha nada nela */

/* Cor: ou indice 0..255 da paleta, ou RGB direto com o bit 31 aceso. Guardar
 * as duas coisas no mesmo inteiro evita um campo de tipo em cada celula — que,
 * multiplicado pelo historico inteiro, seria megabyte de flag. */
#define COR_RGB(r,g,b) (0x80000000u | ((unsigned)(r)<<16) | ((unsigned)(g)<<8) | (unsigned)(b))
#define COR_E_RGB(c)   ((c) & 0x80000000u)

typedef struct {
    FcChar32     cp;
    unsigned int fg, bg;
    unsigned int atr;
} Celula;

typedef struct { long linha; int col; } Ponto;

/* ------------------------------------------------------------------ estado */

static Display  *dpy;
static int       tela_num;
static Window    janela;
static Visual   *visual;
static Colormap  cmap;
static GC        gc;
static Atom      A_WM_DELETE, A_UTF8, A_NET_NAME, A_CLIPBOARD, A_TARGETS, A_INCR;
static XIM       xim;
static XIC       xic;

static XftFont  *fonte[4];              /* normal, negrito, italico, os dois */
static FcPattern *pedido[4];            /* o pedido CRU de cada uma; ver fonte_para */
static XftDraw  *desenho;
static int       larg_cel, alt_cel, base_cel;
static int       margem = 2;

static int      colunas = 80, linhas = 24;
static Celula **tela;                   /* vetor de ponteiros de linha */
static Celula **tela_guardada;          /* a principal, enquanto a alt esta no ar */
static char    *sujo;                   /* uma marca de "redesenhar" por linha */

static Celula **hist;                   /* anel do historico */
static int      hist_max = 1000, hist_n, hist_i;
static long     desloc;                 /* total de linhas que ja sairam por cima */
static int      rolagem;                /* quantas linhas o usuario subiu */

static int      cx, cy;                 /* cursor */
static int      cx_sv, cy_sv;
static unsigned atr_sv;
static unsigned fg_sv, bg_sv;
static int      topo_rol, base_rol;     /* regiao de rolagem, DECSTBM */
static int      alt_ativa;
static int      cursor_visivel = 1;
static int      envolver = 1;           /* DECAWM */
static int      pendente_envolver;      /* o cursor "gruda" na ultima coluna */
static int      modo_insercao;          /* IRM */
static int      teclas_app;             /* DECCKM */
static int      colar_marcado;          /* 2004 */
static int      mouse_modo;             /* 0, 1000, 1002, 1003 */
static int      mouse_sgr;              /* 1006 */
static int      foco_nosso = 1;

static unsigned atr_atual;
static unsigned fg_atual, bg_atual;
static unsigned padrao_fg = COR_RGB(0xe5, 0xe5, 0xe5);
static unsigned padrao_bg = COR_RGB(0x00, 0x00, 0x00);

static char    *paradas;                /* tabulacao: uma marca por coluna */
static int      g0_grafico;             /* ESC ( 0 — os tracos de moldura */

static int      mestre = -1;            /* fd do pty */
static pid_t    filho = -1;
static volatile sig_atomic_t filho_morreu;

static Ponto    sel_a, sel_b;           /* em linha ABSOLUTA (desloc + y) */
static int      sel_ativa, sel_arrastando;
static char    *sel_texto;
static Time     ultimo_clique;
static int      cliques;

/* ------------------------------------------------------------------- cores */

static XftColor  pal[256];
static struct { unsigned rgb; XftColor cor; } cache_rgb[256];
static int       n_cache;

static const unsigned base16[16] = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x1e90ff, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x4d4d4d, 0xff0000, 0x00ff00, 0xffff00, 0x4682b4, 0xff00ff, 0x00ffff, 0xffffff
};

static void morrer(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void aloca_rgb(XftColor *c, unsigned rgb)
{
    XRenderColor r;
    r.red   = ((rgb >> 16) & 0xff) * 257;
    r.green = ((rgb >>  8) & 0xff) * 257;
    r.blue  = ( rgb        & 0xff) * 257;
    r.alpha = 0xffff;
    XftColorAllocValue(dpy, visual, cmap, &r, c);
}

static void cores_iniciar(void)
{
    int i, r, g, b, n = 0;
    for (i = 0; i < 16; i++) aloca_rgb(&pal[n++], base16[i]);
    for (r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++)
                aloca_rgb(&pal[n++], ((r ? r * 40 + 55 : 0) << 16) |
                                     ((g ? g * 40 + 55 : 0) <<  8) |
                                      (b ? b * 40 + 55 : 0));
    for (i = 0; i < 24; i++) {
        unsigned v = 8 + i * 10;
        aloca_rgb(&pal[n++], (v << 16) | (v << 8) | v);
    }
}

/* Truecolor chega de programa que se acha bonito; sao poucos valores
 * distintos numa sessao inteira. Busca linear num cache pequeno custa menos
 * que manter tabela de hash — e quando o cache lota, cai na paleta em vez de
 * vazar XftColor a cada quadro. */
static XftColor *cor_de(unsigned c)
{
    int i;
    if (!COR_E_RGB(c)) return &pal[c & 0xff];
    c &= 0xffffff;
    for (i = 0; i < n_cache; i++)
        if (cache_rgb[i].rgb == c) return &cache_rgb[i].cor;
    if (n_cache == 256) return &pal[7];
    cache_rgb[n_cache].rgb = c;
    aloca_rgb(&cache_rgb[n_cache].cor, c);
    return &cache_rgb[n_cache++].cor;
}

/* ------------------------------------------------------------------ fontes */

/* Reserva de fontes para o que a DejaVu nao tem (emoji, CJK, simbolo solto).
 * Sem isto o caractere vira retangulo vazio e parece bug do parser. */
#define RESERVA_MAX 16
static struct { XftFont *f; int estilo; FcCharSet *cs; } reserva[RESERVA_MAX];
static int n_reserva;

static XftFont *fonte_para(FcChar32 cp, int estilo)
{
    FcPattern *p, *casado;
    FcResult   res;
    FcCharSet *cs;
    int i;

    if (XftCharExists(dpy, fonte[estilo], cp)) return fonte[estilo];

    for (i = 0; i < n_reserva; i++)
        if (reserva[i].estilo == estilo && FcCharSetHasChar(reserva[i].cs, cp))
            return reserva[i].f;
    if (n_reserva == RESERVA_MAX) return fonte[estilo];

    cs = FcCharSetCreate();
    FcCharSetAddChar(cs, cp);
    /* Parte-se do pedido CRU ("DejaVu Sans Mono:size=10"), nunca do
     * fonte[estilo]->pattern. O pattern de uma fonte ja aberta e o RESOLVIDO:
     * carrega FC_FILE e FC_FONTVERSION da DejaVu, e os dois sao criterios de
     * prioridade MAIOR que o charset dentro do fontconfig. Duplicando-o, o
     * FcFontMatch devolve a propria DejaVu por mais que se peca um charset que
     * ela nao tem — medido em 01/08/2026: japones e emoji continuavam caixa
     * vazia com as fontes Noto instaladas e funcionando. Do pedido cru sai a
     * Noto Sans Mono CJK, ja na variante monoespacada. */
    p = FcPatternDuplicate(pedido[estilo]);
    FcPatternAddCharSet(p, FC_CHARSET, cs);
    FcConfigSubstitute(NULL, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    casado = FcFontMatch(NULL, p, &res);
    FcPatternDestroy(p);
    if (!casado) { FcCharSetDestroy(cs); return fonte[estilo]; }

    reserva[n_reserva].f = XftFontOpenPattern(dpy, casado);
    if (!reserva[n_reserva].f) { FcPatternDestroy(casado); FcCharSetDestroy(cs); return fonte[estilo]; }
    reserva[n_reserva].estilo = estilo;
    reserva[n_reserva].cs = cs;
    return reserva[n_reserva++].f;
}

static void fontes_abrir(const char *nome, double tam)
{
    char buf[256];
    XGlyphInfo g;
    static const char *sufixo[4] = { "", ":bold", ":italic", ":bold:italic" };
    int i;

    for (i = 0; i < 4; i++) {
        snprintf(buf, sizeof buf, "%s:size=%g%s", nome, tam, sufixo[i]);
        fonte[i] = XftFontOpenName(dpy, tela_num, buf);
        if (!fonte[i]) morrer("nao consegui abrir a fonte '%s'", buf);
        pedido[i] = FcNameParse((const FcChar8 *) buf);
        if (!pedido[i]) morrer("nao entendi o nome de fonte '%s'", buf);
    }
    /* A largura da celula vem do avanco de um caractere real, nao do
     * max_advance_width: em fonte com glifo largo perdido no meio (uma seta,
     * um emoji) o maximo estica a grade inteira e sobra buraco entre colunas. */
    XftTextExtentsUtf8(dpy, fonte[0], (const FcChar8 *) "M", 1, &g);
    larg_cel  = g.xOff ? g.xOff : fonte[0]->max_advance_width;
    alt_cel   = fonte[0]->ascent + fonte[0]->descent;
    base_cel  = fonte[0]->ascent;
}

/* ------------------------------------------------------- linhas e historico */

static Celula *linha_nova(void)
{
    Celula *l = malloc(sizeof(Celula) * colunas);
    int x;
    if (!l) morrer("sem memoria");
    for (x = 0; x < colunas; x++) {
        l[x].cp = ' '; l[x].fg = padrao_fg; l[x].bg = padrao_bg; l[x].atr = 0;
    }
    return l;
}

static void linha_limpar(Celula *l, int de, int ate)
{
    int x;
    if (de < 0) de = 0;
    for (x = de; x <= ate && x < colunas; x++) {
        l[x].cp = ' '; l[x].fg = fg_atual; l[x].bg = bg_atual; l[x].atr = 0;
    }
}

static void sujar_tudo(void) { memset(sujo, 1, linhas); }

static void hist_empurrar(Celula *l)
{
    if (hist_max == 0) { free(l); desloc++; return; }
    if (hist_n == hist_max) free(hist[hist_i]);
    else hist_n++;
    hist[hist_i] = l;
    hist_i = (hist_i + 1) % hist_max;
    desloc++;
}

/* Linha ABSOLUTA: 0 e a primeira que ja existiu. As do historico ainda vivo
 * ocupam [desloc-hist_n, desloc), e a tela ocupa [desloc, desloc+linhas). */
static Celula *linha_abs(long a)
{
    if (a >= desloc) {
        if (a >= desloc + linhas) return NULL;
        return tela[a - desloc];
    }
    if (a < desloc - hist_n) return NULL;
    return hist[(hist_i - (int)(desloc - a) + hist_max * 2) % hist_max];
}

/* A linha mostrada na altura y da janela, ja contando a rolagem do usuario. */
static Celula *linha_vista(int y) { return linha_abs(desloc - rolagem + y); }

/* Rolar N passos para cima dentro da regiao. So o que sai do TOPO DA TELA
 * (regiao comecando em 0, e a tela principal) vira historico — o que sai de
 * uma regiao no meio, como a que o vim monta, e texto descartado mesmo. */
static void rolar_cima(int n)
{
    int i;
    while (n-- > 0) {
        Celula *saiu = tela[topo_rol];
        for (i = topo_rol; i < base_rol; i++) tela[i] = tela[i + 1];
        if (topo_rol == 0 && !alt_ativa) {
            hist_empurrar(saiu);
            saiu = linha_nova();
        } else {
            linha_limpar(saiu, 0, colunas - 1);
        }
        tela[base_rol] = saiu;
    }
    sujar_tudo();
}

static void rolar_baixo(int n)
{
    int i;
    while (n-- > 0) {
        Celula *saiu = tela[base_rol];
        for (i = base_rol; i > topo_rol; i--) tela[i] = tela[i - 1];
        linha_limpar(saiu, 0, colunas - 1);
        tela[topo_rol] = saiu;
    }
    sujar_tudo();
}

/* ------------------------------------------------------------ escrita no pty */

static void enviar(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t k = write(mestre, s, n);
        if (k < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return;
        }
        s += k; n -= k;
    }
}

static void enviar_str(const char *s) { enviar(s, strlen(s)); }

/* ------------------------------------------------------------- parser: base */

static void novalinha(int com_retorno)
{
    if (com_retorno) cx = 0;
    if (cy == base_rol) rolar_cima(1);
    else if (cy < linhas - 1) cy++;
    pendente_envolver = 0;
}

static void por_caractere(FcChar32 cp, int largura)
{
    Celula *l;
    int i;

    if (pendente_envolver && envolver) { novalinha(1); pendente_envolver = 0; }
    if (cx + largura > colunas) {
        if (!envolver) { cx = colunas - largura; if (cx < 0) return; }
        else novalinha(1);
    }

    l = tela[cy];
    if (modo_insercao) {
        for (i = colunas - 1; i >= cx + largura; i--) l[i] = l[i - largura];
    }
    /* Sobrescrever a metade de um glifo largo deixaria a outra metade orfa,
     * desenhada sem par; apaga-se o par inteiro antes de escrever. */
    if (l[cx].atr & ATR_CONT) { if (cx > 0) l[cx-1].cp = ' ', l[cx-1].atr &= ~ATR_LARGA; }
    if ((l[cx].atr & ATR_LARGA) && cx + 1 < colunas) { l[cx+1].cp = ' '; l[cx+1].atr &= ~ATR_CONT; }

    l[cx].cp = cp;
    l[cx].fg = fg_atual;
    l[cx].bg = bg_atual;
    l[cx].atr = atr_atual | (largura == 2 ? ATR_LARGA : 0);
    if (largura == 2 && cx + 1 < colunas) {
        l[cx+1].cp = ' ';
        l[cx+1].fg = fg_atual; l[cx+1].bg = bg_atual;
        l[cx+1].atr = atr_atual | ATR_CONT;
    }
    sujo[cy] = 1;

    if (cx + largura >= colunas) { cx = colunas - 1; pendente_envolver = 1; }
    else cx += largura;
}

/* A tabela de "desenho de linha" do DEC: o ncurses monta moldura mandando
 * ESC ( 0 e depois letras minusculas. Sem isto, `dialog`, `mc` e as bordas do
 * vim viram um amontoado de "lqqqk". */
static FcChar32 grafico_dec(FcChar32 c)
{
    static const FcChar32 t[] = {
        0x25c6,0x2592,0x2409,0x240c,0x240d,0x240a,0x00b0,0x00b1, /* ` a b c d e f g */
        0x2424,0x240b,0x2518,0x2510,0x250c,0x2514,0x253c,0x23ba, /* h i j k l m n o */
        0x23bb,0x2500,0x23bc,0x23bd,0x251c,0x2524,0x2534,0x252c, /* p q r s t u v w */
        0x2502,0x2264,0x2265,0x03c0,0x2260,0x00a3,0x00b7         /* x y z { | } ~ */
    };
    if (c >= 0x60 && c <= 0x7e) return t[c - 0x60];
    return c;
}

/* ------------------------------------------------------------ parser: estado */

enum { S_CHAO, S_ESC, S_CSI, S_OSC, S_CHARSET, S_IGNORA };
static int estado;
static int par[16], n_par;
static int priv;                       /* o '?' do CSI */
static char inter;                     /* o '>' / '$' / ' ' intermediario */
static char osc[512];
static int n_osc;
static int viu_esc;                    /* o ESC de um ST partido em dois bytes */

static void titulo_por(const char *t)
{
    XStoreName(dpy, janela, t);
    XChangeProperty(dpy, janela, A_NET_NAME, A_UTF8, 8, PropModeReplace,
                    (const unsigned char *) t, strlen(t));
}

static void sgr(void)
{
    int i;
    if (n_par == 0) { par[0] = 0; n_par = 1; }
    for (i = 0; i < n_par; i++) {
        int p = par[i];
        switch (p) {
        case 0:  atr_atual = 0; fg_atual = padrao_fg; bg_atual = padrao_bg; break;
        case 1:  atr_atual |= ATR_NEGRITO; break;
        case 2:  atr_atual |= ATR_FRACO; break;
        case 3:  atr_atual |= ATR_ITALICO; break;
        case 4:  atr_atual |= ATR_SUBLINHA; break;
        case 7:  atr_atual |= ATR_INVERSO; break;
        case 8:  atr_atual |= ATR_INVISIVEL; break;
        case 9:  atr_atual |= ATR_RISCADO; break;
        case 21: case 22: atr_atual &= ~(ATR_NEGRITO | ATR_FRACO); break;
        case 23: atr_atual &= ~ATR_ITALICO; break;
        case 24: atr_atual &= ~ATR_SUBLINHA; break;
        case 27: atr_atual &= ~ATR_INVERSO; break;
        case 28: atr_atual &= ~ATR_INVISIVEL; break;
        case 29: atr_atual &= ~ATR_RISCADO; break;
        case 39: fg_atual = padrao_fg; break;
        case 49: bg_atual = padrao_bg; break;
        case 38: case 48: {
            unsigned c = 0;
            int ok = 0;
            if (i + 1 < n_par && par[i+1] == 5 && i + 2 < n_par) {
                c = par[i+2] & 0xff; ok = 1; i += 2;
            } else if (i + 1 < n_par && par[i+1] == 2 && i + 4 < n_par) {
                c = COR_RGB(par[i+2], par[i+3], par[i+4]); ok = 1; i += 4;
            }
            if (ok) { if (p == 38) fg_atual = c; else bg_atual = c; }
            break;
        }
        default:
            if (p >= 30 && p <= 37)        fg_atual = p - 30;
            else if (p >= 40 && p <= 47)   bg_atual = p - 40;
            else if (p >= 90 && p <= 97)   fg_atual = p - 90 + 8;
            else if (p >= 100 && p <= 107) bg_atual = p - 100 + 8;
            break;
        }
    }
}

static void trocar_tela(int para_alt)
{
    if (para_alt == alt_ativa) return;
    if (para_alt) {
        int y;
        tela_guardada = tela;
        tela = malloc(sizeof(Celula *) * linhas);
        for (y = 0; y < linhas; y++) tela[y] = linha_nova();
        alt_ativa = 1;
        rolagem = 0;
    } else {
        int y;
        for (y = 0; y < linhas; y++) free(tela[y]);
        free(tela);
        tela = tela_guardada;
        tela_guardada = NULL;
        alt_ativa = 0;
    }
    sujar_tudo();
}

static void modo_dec(int ligar)
{
    int i;
    for (i = 0; i < n_par; i++) {
        switch (par[i]) {
        case 1:    teclas_app = ligar; break;
        case 7:    envolver = ligar; break;
        case 12:   break;                       /* piscar cursor: ignorado */
        case 25:   cursor_visivel = ligar; sujo[cy] = 1; break;
        case 1000: mouse_modo = ligar ? 1000 : 0; break;
        case 1002: mouse_modo = ligar ? 1002 : 0; break;
        case 1003: mouse_modo = ligar ? 1003 : 0; break;
        case 1005: break;
        case 1006: mouse_sgr = ligar; break;
        case 2004: colar_marcado = ligar; break;
        case 47: case 1047: trocar_tela(ligar); break;
        case 1048:
            if (ligar) { cx_sv = cx; cy_sv = cy; atr_sv = atr_atual; fg_sv = fg_atual; bg_sv = bg_atual; }
            else { cx = cx_sv; cy = cy_sv; atr_atual = atr_sv; fg_atual = fg_sv; bg_atual = bg_sv; }
            break;
        case 1049:
            if (ligar) {
                cx_sv = cx; cy_sv = cy; atr_sv = atr_atual; fg_sv = fg_atual; bg_sv = bg_atual;
                trocar_tela(1);
            } else {
                trocar_tela(0);
                cx = cx_sv; cy = cy_sv; atr_atual = atr_sv; fg_atual = fg_sv; bg_atual = bg_sv;
            }
            break;
        }
    }
}

static void fim_csi(char c)
{
    int p0 = n_par > 0 ? par[0] : 0;
    int p1 = n_par > 1 ? par[1] : 0;
    int n  = p0 ? p0 : 1;
    int i;
    char buf[64];

    switch (c) {
    case '@': { /* ICH */
        Celula *l = tela[cy];
        for (i = colunas - 1; i >= cx + n; i--) l[i] = l[i - n];
        linha_limpar(l, cx, cx + n - 1);
        sujo[cy] = 1; break;
    }
    case 'A': cy -= n; if (cy < 0) cy = 0; pendente_envolver = 0; break;
    case 'B': case 'e': cy += n; if (cy >= linhas) cy = linhas - 1; pendente_envolver = 0; break;
    case 'C': case 'a': cx += n; if (cx >= colunas) cx = colunas - 1; pendente_envolver = 0; break;
    case 'D': cx -= n; if (cx < 0) cx = 0; pendente_envolver = 0; break;
    case 'E': cx = 0; cy += n; if (cy >= linhas) cy = linhas - 1; break;
    case 'F': cx = 0; cy -= n; if (cy < 0) cy = 0; break;
    case 'G': case '`': cx = n - 1; if (cx >= colunas) cx = colunas - 1; pendente_envolver = 0; break;
    case 'd': cy = n - 1; if (cy >= linhas) cy = linhas - 1; break;
    case 'H': case 'f':
        cy = (p0 ? p0 : 1) - 1; cx = (p1 ? p1 : 1) - 1;
        if (cy >= linhas) cy = linhas - 1;
        if (cx >= colunas) cx = colunas - 1;
        pendente_envolver = 0;
        break;
    case 'I': /* CHT */
        while (n-- > 0 && cx < colunas - 1) { do cx++; while (cx < colunas - 1 && !paradas[cx]); }
        if (cx >= colunas) cx = colunas - 1;
        break;
    case 'Z': /* CBT */
        while (n-- > 0 && cx > 0) { do cx--; while (cx > 0 && !paradas[cx]); }
        if (cx < 0) cx = 0;
        break;
    case 'J':
        if (p0 == 0) { linha_limpar(tela[cy], cx, colunas - 1);
                       for (i = cy + 1; i < linhas; i++) linha_limpar(tela[i], 0, colunas - 1); }
        else if (p0 == 1) { linha_limpar(tela[cy], 0, cx);
                            for (i = 0; i < cy; i++) linha_limpar(tela[i], 0, colunas - 1); }
        else if (p0 == 2 || p0 == 3) {
            /* `clear` manda ED 2. Jogar a tela cheia para o historico em vez de
             * apaga-la e o que faz o texto anterior continuar existindo na
             * rolagem, como no xterm — apagar de verdade some com ele. */
            if (!alt_ativa && p0 == 2) {
                int y, ultima = -1;
                for (y = 0; y < linhas; y++) {
                    int x;
                    for (x = 0; x < colunas; x++)
                        if (tela[y][x].cp != ' ') { ultima = y; break; }
                }
                for (y = 0; y <= ultima; y++) {
                    hist_empurrar(tela[y]);
                    tela[y] = linha_nova();
                }
                for (; y < linhas; y++) linha_limpar(tela[y], 0, colunas - 1);
            } else {
                for (i = 0; i < linhas; i++) linha_limpar(tela[i], 0, colunas - 1);
            }
        }
        sujar_tudo(); break;
    case 'K':
        if (p0 == 0)      linha_limpar(tela[cy], cx, colunas - 1);
        else if (p0 == 1) linha_limpar(tela[cy], 0, cx);
        else              linha_limpar(tela[cy], 0, colunas - 1);
        sujo[cy] = 1; break;
    case 'L': /* IL */
        if (cy >= topo_rol && cy <= base_rol) {
            int guarda = topo_rol; topo_rol = cy; rolar_baixo(n); topo_rol = guarda;
        }
        break;
    case 'M': /* DL */
        if (cy >= topo_rol && cy <= base_rol) {
            int guarda = topo_rol; topo_rol = cy; rolar_cima(n); topo_rol = guarda;
        }
        break;
    case 'P': { /* DCH */
        Celula *l = tela[cy];
        for (i = cx; i < colunas - n; i++) l[i] = l[i + n];
        linha_limpar(l, colunas - n, colunas - 1);
        sujo[cy] = 1; break;
    }
    case 'X': linha_limpar(tela[cy], cx, cx + n - 1); sujo[cy] = 1; break;
    case 'S': rolar_cima(n); break;
    case 'T': rolar_baixo(n); break;
    case 'g': if (p0 == 3) memset(paradas, 0, colunas); else paradas[cx] = 0; break;
    case 'h': if (priv) modo_dec(1); else if (p0 == 4) modo_insercao = 1; break;
    case 'l': if (priv) modo_dec(0); else if (p0 == 4) modo_insercao = 0; break;
    case 'm': if (!priv) sgr(); break;
    case 'n':
        if (p0 == 5) enviar_str("\033[0n");
        else if (p0 == 6) { snprintf(buf, sizeof buf, "\033[%d;%dR", cy + 1, cx + 1); enviar_str(buf); }
        break;
    case 'c': if (!priv) enviar_str("\033[?6c"); break;   /* diz que e um VT102 */
    case 'r':
        topo_rol = (p0 ? p0 : 1) - 1;
        base_rol = (p1 ? p1 : linhas) - 1;
        if (topo_rol < 0) topo_rol = 0;
        if (base_rol >= linhas) base_rol = linhas - 1;
        if (topo_rol >= base_rol) { topo_rol = 0; base_rol = linhas - 1; }
        cx = 0; cy = topo_rol;
        break;
    case 's': cx_sv = cx; cy_sv = cy; break;
    case 'u': cx = cx_sv; cy = cy_sv; break;
    case 't': break;                                      /* geometria: ignorado */
    }
}

static void byte_controle(unsigned char c)
{
    switch (c) {
    case '\a': XBell(dpy, 0); break;
    case '\b': if (pendente_envolver) pendente_envolver = 0; else if (cx > 0) cx--; break;
    case '\t': { int x = cx;
                 do x++; while (x < colunas - 1 && !paradas[x]);
                 cx = x < colunas ? x : colunas - 1; break; }
    case '\n': case '\v': case '\f': novalinha(0); break;
    case '\r': cx = 0; pendente_envolver = 0; break;
    case 0x0e: g0_grafico = 1; break;   /* SO — usado com o charset G1 */
    case 0x0f: g0_grafico = 0; break;   /* SI */
    }
}

/* Decodificador de UTF-8 que guarda estado entre leituras: uma sequencia pode
 * ser cortada no meio pela fronteira do read(), e tratar isso como byte
 * invalido colocaria "??" no meio de palavra acentuada de vez em quando. */
static FcChar32 u_acum;
static int u_faltam;

static void um_byte(unsigned char c)
{
    if (estado == S_CHAO && u_faltam > 0) {
        if ((c & 0xc0) == 0x80) {
            u_acum = (u_acum << 6) | (c & 0x3f);
            if (--u_faltam == 0) {
                int w = wcwidth((wchar_t) u_acum);
                if (w < 0) w = 1;
                if (w == 0) return;             /* combinante: nao ha celula para ele */
                por_caractere(u_acum, w > 2 ? 2 : w);
            }
            return;
        }
        u_faltam = 0;   /* sequencia truncada; o byte de agora recomeca */
    }

    switch (estado) {
    case S_CHAO:
        if (c == 0x1b) { estado = S_ESC; inter = 0; return; }
        if (c < 0x20 || c == 0x7f) { byte_controle(c); return; }
        if (c < 0x80) {
            FcChar32 cp = g0_grafico ? grafico_dec(c) : c;
            por_caractere(cp, 1);
            return;
        }
        if ((c & 0xe0) == 0xc0) { u_acum = c & 0x1f; u_faltam = 1; }
        else if ((c & 0xf0) == 0xe0) { u_acum = c & 0x0f; u_faltam = 2; }
        else if ((c & 0xf8) == 0xf0) { u_acum = c & 0x07; u_faltam = 3; }
        else por_caractere(0xfffd, 1);
        return;

    case S_ESC:
        switch (c) {
        case '[': estado = S_CSI; n_par = 0; par[0] = 0; priv = 0; inter = 0; return;
        case ']': estado = S_OSC; n_osc = 0; return;
        case '(': case ')': case '*': case '+': estado = S_CHARSET; inter = c; return;
        case 'P': case '^': case '_': estado = S_IGNORA; return;  /* DCS/PM/APC */
        case '7': cx_sv = cx; cy_sv = cy; atr_sv = atr_atual; fg_sv = fg_atual; bg_sv = bg_atual; break;
        case '8': cx = cx_sv; cy = cy_sv; atr_atual = atr_sv; fg_atual = fg_sv; bg_atual = bg_sv; break;
        case 'D': novalinha(0); break;
        case 'E': novalinha(1); break;
        case 'M': if (cy == topo_rol) rolar_baixo(1); else if (cy > 0) cy--; break;
        case 'H': paradas[cx] = 1; break;
        case '=': case '>': break;        /* modo do teclado numerico */
        case 'c': {
            int y;
            atr_atual = 0; fg_atual = padrao_fg; bg_atual = padrao_bg;
            cx = cy = 0; topo_rol = 0; base_rol = linhas - 1;
            trocar_tela(0);
            for (y = 0; y < linhas; y++) linha_limpar(tela[y], 0, colunas - 1);
            sujar_tudo();
            break;
        }
        }
        estado = S_CHAO;
        return;

    case S_CHARSET:
        /* So o G0 importa aqui: quem usa moldura manda ESC ( 0 e volta com
         * ESC ( B. Trocar G1..G3 sem SO/SI nao muda nada na tela. */
        if (inter == '(') g0_grafico = (c == '0');
        estado = S_CHAO;
        return;

    case S_CSI:
        if (c >= '0' && c <= '9') {
            if (n_par == 0) n_par = 1;
            par[n_par - 1] = par[n_par - 1] * 10 + (c - '0');
            if (par[n_par - 1] > 65535) par[n_par - 1] = 65535;
            return;
        }
        if (c == ';' || c == ':') {
            if (n_par == 0) n_par = 1;
            if (n_par < 16) par[n_par++] = 0;
            return;
        }
        if (c == '?' || c == '>' || c == '<' || c == '!') { priv = 1; return; }
        if (c == ' ' || c == '$' || c == '"' || c == '\'') { inter = c; return; }
        if (c >= 0x40 && c <= 0x7e) { fim_csi(c); estado = S_CHAO; return; }
        if (c < 0x20) { byte_controle(c); return; }
        estado = S_CHAO;
        return;

    case S_OSC:
        /* O terminador e BEL ou ST, e o ST sao DOIS bytes (ESC \\). Fechar no
         * ESC sozinho perderia o titulo de quem usa a forma correta — que e a
         * que o bash manda no PROMPT_COMMAND de varias distribuicoes. */
        if (c == 0x1b) { viu_esc = 1; return; }
        if (viu_esc) {
            viu_esc = 0;
            if (c != '\\') return;
        } else if (c != 0x07 && c != 0x9c) {
            if (n_osc < (int) sizeof osc - 1) osc[n_osc++] = (char) c;
            return;
        }
        osc[n_osc] = 0;
        if (!strncmp(osc, "0;", 2) || !strncmp(osc, "2;", 2)) titulo_por(osc + 2);
        estado = S_CHAO;
        return;

    case S_IGNORA:
        /* Consome ate o ST ou o BEL. O conteudo de DCS/APC nao interessa aqui,
         * mas ENGOLI-LO interessa muito: sem isto ele cairia no chao e viraria
         * lixo desenhado na tela. */
        if (c == 0x1b) { viu_esc = 1; return; }
        if (viu_esc) { viu_esc = 0; if (c == '\\') estado = S_CHAO; return; }
        if (c == 0x07 || c == 0x9c) estado = S_CHAO;
        return;
    }
}

static void alimentar(const char *b, int n)
{
    int i;
    for (i = 0; i < n; i++) um_byte((unsigned char) b[i]);
}

/* ---------------------------------------------------------------- desenho */

static void efetiva(const Celula *c, unsigned *fg, unsigned *bg, int selecionada)
{
    *fg = c->fg; *bg = c->bg;
    /* Fraco em RGB e metade de cada componente; na paleta e a versao normal da
     * cor, ja que 0..7 ja sao as escuras e nao ha degrau abaixo delas. */
    if (c->atr & ATR_FRACO) {
        if (COR_E_RGB(*fg))
            *fg = COR_RGB(((*fg >> 17) & 0x7f), ((*fg >> 9) & 0x7f), ((*fg >> 1) & 0x7f));
        else if (*fg >= 8 && *fg < 16) *fg -= 8;
    }
    if (c->atr & ATR_INVERSO) { unsigned t = *fg; *fg = *bg; *bg = t; }
    if (c->atr & ATR_INVISIVEL) *fg = *bg;
    if (selecionada) { unsigned t = *fg; *fg = *bg; *bg = t; }
}

static int celula_selecionada(long abs, int x)
{
    Ponto a = sel_a, b = sel_b;
    long ia, ib;
    if (!sel_ativa) return 0;
    if (a.linha > b.linha || (a.linha == b.linha && a.col > b.col)) { Ponto t = a; a = b; b = t; }
    ia = a.linha; ib = b.linha;
    if (abs < ia || abs > ib) return 0;
    if (abs == ia && x < a.col) return 0;
    if (abs == ib && x > b.col) return 0;
    return 1;
}

static void desenhar_linha(int y)
{
    Celula *l = linha_vista(y);
    long abs = desloc - rolagem + y;
    int py = y * alt_cel + margem;
    int x = 0;
    FcChar32 buf[512];

    if (!l) {
        XftDrawRect(desenho, cor_de(padrao_bg), margem, py, colunas * larg_cel, alt_cel);
        return;
    }

    while (x < colunas) {
        unsigned fg, bg;
        int estilo, sel = celula_selecionada(abs, x);
        int inicio = x, n = 0;
        XftFont *f;

        if (l[x].atr & ATR_CONT) { x++; continue; }

        efetiva(&l[x], &fg, &bg, sel);
        estilo = ((l[x].atr & ATR_NEGRITO) ? 1 : 0) | ((l[x].atr & ATR_ITALICO) ? 2 : 0);
        f = fonte_para(l[x].cp, estilo);

        /* Um trecho e a maior sequencia com a MESMA cor, o mesmo estilo, a
         * mesma fonte e sem glifo largo. O glifo largo sai do trecho porque o
         * Xft avanca pela largura real dele e nao pela celula — dentro de um
         * trecho isso desalinharia tudo o que vem depois. */
        while (x < colunas && n < (int) (sizeof buf / sizeof buf[0])) {
            unsigned f2, b2;
            int e2, s2 = celula_selecionada(abs, x);
            if (l[x].atr & ATR_CONT) break;
            efetiva(&l[x], &f2, &b2, s2);
            e2 = ((l[x].atr & ATR_NEGRITO) ? 1 : 0) | ((l[x].atr & ATR_ITALICO) ? 2 : 0);
            if (f2 != fg || b2 != bg || e2 != estilo) break;
            if ((l[x].atr ^ l[inicio].atr) & (ATR_SUBLINHA | ATR_RISCADO)) break;
            if (fonte_para(l[x].cp, e2) != f) break;
            buf[n++] = l[x].cp;
            if (l[x].atr & ATR_LARGA) { x++; if (x < colunas) x++; break; }
            x++;
        }
        if (n == 0) { x++; continue; }

        XftDrawRect(desenho, cor_de(bg), margem + inicio * larg_cel, py,
                    (x - inicio) * larg_cel, alt_cel);
        XftDrawString32(desenho, cor_de(fg), f,
                        margem + inicio * larg_cel, py + base_cel, buf, n);
        if (l[inicio].atr & ATR_SUBLINHA)
            XftDrawRect(desenho, cor_de(fg), margem + inicio * larg_cel,
                        py + base_cel + 1, (x - inicio) * larg_cel, 1);
        if (l[inicio].atr & ATR_RISCADO)
            XftDrawRect(desenho, cor_de(fg), margem + inicio * larg_cel,
                        py + base_cel - alt_cel / 3, (x - inicio) * larg_cel, 1);
    }
}

static void desenhar_cursor(void)
{
    int y = cy + rolagem;
    Celula *l;
    unsigned fg, bg;
    FcChar32 cp;
    int estilo;

    if (!cursor_visivel || y < 0 || y >= linhas) return;
    l = linha_vista(y);
    if (!l) return;

    efetiva(&l[cx], &fg, &bg, 0);
    if (foco_nosso) {
        unsigned t = fg; fg = bg; bg = t;
        XftDrawRect(desenho, cor_de(bg), margem + cx * larg_cel, y * alt_cel + margem,
                    larg_cel, alt_cel);
        cp = l[cx].cp;
        estilo = ((l[cx].atr & ATR_NEGRITO) ? 1 : 0) | ((l[cx].atr & ATR_ITALICO) ? 2 : 0);
        XftDrawString32(desenho, cor_de(fg), fonte_para(cp, estilo),
                        margem + cx * larg_cel, y * alt_cel + margem + base_cel, &cp, 1);
    } else {
        /* Sem foco o cursor vira moldura: o usuario ve onde ele esta sem
         * achar que a janela vai receber o que ele digitar. */
        XSetForeground(dpy, gc, cor_de(fg)->pixel);
        XDrawRectangle(dpy, janela, gc, margem + cx * larg_cel, y * alt_cel + margem,
                       larg_cel - 1, alt_cel - 1);
    }
}

static void redesenhar(int tudo)
{
    int y;
    for (y = 0; y < linhas; y++)
        if (tudo || sujo[y] || rolagem) desenhar_linha(y);
    memset(sujo, 0, linhas);
    desenhar_cursor();
    /* As sobras a direita e embaixo, quando a janela nao e multipla exata da
     * celula. Sem pintar, ficam com lixo do que havia antes do resize. */
    {
        XWindowAttributes a;
        XGetWindowAttributes(dpy, janela, &a);
        if (a.width > margem + colunas * larg_cel)
            XftDrawRect(desenho, cor_de(padrao_bg), margem + colunas * larg_cel, 0,
                        a.width - margem - colunas * larg_cel, a.height);
        if (a.height > margem + linhas * alt_cel)
            XftDrawRect(desenho, cor_de(padrao_bg), 0, margem + linhas * alt_cel,
                        a.width, a.height - margem - linhas * alt_cel);
        if (margem) {
            XftDrawRect(desenho, cor_de(padrao_bg), 0, 0, a.width, margem);
            XftDrawRect(desenho, cor_de(padrao_bg), 0, 0, margem, a.height);
        }
    }
}

/* ------------------------------------------------------------ instrumento */

/* Um SIGUSR1 despeja a grade em texto. Existe porque a regra deste projeto e
 * medir, e porque screenshot MENTE: sem compositor, `import -window` de uma
 * janela coberta devolve os pixels de quem esta por cima, e ja se perseguiu
 * aqui um bug que nao existia por causa disso. O estado real e este arquivo. */
static volatile sig_atomic_t pedido_despejo;
static void ao_pedir_despejo(int s) { (void) s; pedido_despejo = 1; }

static void despejar(void)
{
    const char *caminho = getenv("TERMINAL_DESPEJO");
    FILE *f = caminho ? fopen(caminho, "w") : stderr;
    int y, x;

    if (!f) return;
    fprintf(f, "grade %dx%d cursor=%d,%d alt=%d rolagem=%d hist=%d desloc=%ld\n",
            colunas, linhas, cx, cy, alt_ativa, rolagem, hist_n, desloc);
    fprintf(f, "regiao=%d..%d envolver=%d insercao=%d teclas_app=%d mouse=%d sgr=%d colar=%d\n",
            topo_rol, base_rol, envolver, modo_insercao, teclas_app, mouse_modo,
            mouse_sgr, colar_marcado);

    for (y = 0; y < linhas; y++) {
        Celula *l = tela[y];
        fprintf(f, "%2d |", y);
        for (x = 0; x < colunas; x++) {
            FcChar32 c = l[x].cp;
            if (l[x].atr & ATR_CONT) continue;
            if (c < 0x80) fputc((int) c, f);
            else if (c < 0x800) { fputc(0xc0 | (c >> 6), f); fputc(0x80 | (c & 0x3f), f); }
            else if (c < 0x10000) { fputc(0xe0 | (c >> 12), f); fputc(0x80 | ((c >> 6) & 0x3f), f); fputc(0x80 | (c & 0x3f), f); }
            else { fputc(0xf0 | (c >> 18), f); fputc(0x80 | ((c >> 12) & 0x3f), f); fputc(0x80 | ((c >> 6) & 0x3f), f); fputc(0x80 | (c & 0x3f), f); }
        }
        fputs("|\n", f);
    }

    /* Os trechos de atributo, que o texto acima nao mostra: e onde se ve se o
     * SGR pegou a celula certa ou uma a mais. */
    for (y = 0; y < linhas; y++) {
        Celula *l = tela[y];
        for (x = 0; x < colunas; x++) {
            int i = x;
            while (i + 1 < colunas && l[i+1].fg == l[x].fg && l[i+1].bg == l[x].bg
                   && l[i+1].atr == l[x].atr) i++;
            if (l[x].fg != padrao_fg || l[x].bg != padrao_bg || l[x].atr)
                fprintf(f, "atr %d %d-%d fg=%08x bg=%08x a=%03x\n",
                        y, x, i, l[x].fg, l[x].bg, l[x].atr);
            x = i;
        }
    }

    for (y = 0; y < hist_n; y++) {
        Celula *l = hist[(hist_i - hist_n + y + hist_max * 2) % hist_max];
        int ultimo = -1;
        for (x = 0; x < colunas; x++) if (l[x].cp != ' ') ultimo = x;
        fprintf(f, "hist %ld |", desloc - hist_n + y);
        for (x = 0; x <= ultimo; x++) fputc(l[x].cp < 0x80 ? (int) l[x].cp : '?', f);
        fputs("|\n", f);
    }

    fflush(f);
    if (caminho) fclose(f);
    pedido_despejo = 0;
}

/* --------------------------------------------------------------- tamanho */

static void avisar_pty(void)
{
    struct winsize w;
    w.ws_col = colunas; w.ws_row = linhas;
    w.ws_xpixel = colunas * larg_cel; w.ws_ypixel = linhas * alt_cel;
    ioctl(mestre, TIOCSWINSZ, &w);
}

/* Estica ou corta UMA linha para a largura nova, preenchendo com branco o que
 * apareceu a direita. Devolve o ponteiro novo; o antigo morre aqui. */
static Celula *linha_relargar(Celula *l, int velho_c, int nc)
{
    int x;
    l = realloc(l, sizeof(Celula) * nc);
    if (!l) morrer("sem memoria");
    for (x = velho_c; x < nc; x++) {
        l[x].cp = ' '; l[x].fg = padrao_fg; l[x].bg = padrao_bg; l[x].atr = 0;
    }
    return l;
}

static void redimensionar(int lp, int ap)
{
    int nc = (lp - 2 * margem) / larg_cel;
    int nl = (ap - 2 * margem) / alt_cel;
    int velho_c = colunas, velho_l = linhas;
    int mantidas, y, x;

    if (nc < 1) nc = 1;
    if (nl < 1) nl = 1;
    if (nc == colunas && nl == linhas) return;

    /* Encolhendo em altura, o que sai vai pelo TOPO — e so o bastante para o
     * cursor caber. O prompt costuma estar embaixo; jogar linhas fora pelo
     * rodape esconderia justamente aquela em que a pessoa esta digitando. */
    if (nl < linhas) {
        int quanto = cy - nl + 1;
        if (quanto < 0) quanto = 0;
        if (quanto > linhas - nl) quanto = linhas - nl;
        for (y = 0; y < quanto; y++) {
            if (!alt_ativa) hist_empurrar(tela[y]); else free(tela[y]);
        }
        if (quanto) {
            memmove(tela, tela + quanto, sizeof(Celula *) * (linhas - quanto));
            cy -= quanto;
            linhas -= quanto;   /* so restam linhas-quanto ponteiros validos */
        }
        for (y = nl; y < linhas; y++) free(tela[y]);
    }

    mantidas = nl < linhas ? nl : linhas;

    /* Sem redobrar: as linhas mantem o conteudo e sao cortadas ou preenchidas
     * a direita. Redobrar exigiria saber onde cada linha logica termina, e o
     * pty nao conta isso — a informacao se perde na hora em que o texto entra. */
    for (y = 0; y < hist_n; y++) {
        int k = (hist_i - hist_n + y + hist_max * 2) % hist_max;
        hist[k] = linha_relargar(hist[k], velho_c, nc);
    }
    for (y = 0; y < mantidas; y++) tela[y] = linha_relargar(tela[y], velho_c, nc);

    tela = realloc(tela, sizeof(Celula *) * nl);
    if (!tela) morrer("sem memoria");

    colunas = nc;                      /* linha_nova() ja usa a largura nova */
    for (y = mantidas; y < nl; y++) tela[y] = linha_nova();
    linhas = nl;

    /* A tela principal guardada tambem tem de acompanhar. Ela esta parada
     * enquanto o vim ocupa a alternativa, mas volta ao primeiro plano quando
     * ele sair — se ficasse com a largura velha, a primeira leitura dela
     * passaria do fim do malloc, e o estouro so apareceria ao fechar o vim. */
    if (tela_guardada) {
        int m = nl < velho_l ? nl : velho_l;
        for (y = 0; y < velho_l; y++) {
            if (y < m) tela_guardada[y] = linha_relargar(tela_guardada[y], velho_c, nc);
            else free(tela_guardada[y]);
        }
        tela_guardada = realloc(tela_guardada, sizeof(Celula *) * nl);
        if (!tela_guardada) morrer("sem memoria");
        for (y = m; y < nl; y++) tela_guardada[y] = linha_nova();
    }

    paradas = realloc(paradas, colunas);
    for (x = 0; x < colunas; x++) paradas[x] = (x % 8) == 0;

    sujo = realloc(sujo, linhas);
    topo_rol = 0; base_rol = linhas - 1;
    if (cy >= linhas) cy = linhas - 1;
    if (cx >= colunas) cx = colunas - 1;
    if (rolagem > hist_n) rolagem = hist_n;

    sujar_tudo();
    avisar_pty();
}

static void dicas_tamanho(void)
{
    XSizeHints *h = XAllocSizeHints();
    h->flags = PResizeInc | PBaseSize | PMinSize;
    h->width_inc  = larg_cel;
    h->height_inc = alt_cel;
    h->base_width  = 2 * margem;
    h->base_height = 2 * margem;
    h->min_width  = 2 * margem + larg_cel * 2;
    h->min_height = 2 * margem + alt_cel * 2;
    XSetWMNormalHints(dpy, janela, h);
    XFree(h);
}

/* -------------------------------------------------------------- selecao */

static int e_palavra(FcChar32 c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || c == '_' || c == '-' || c == '.' || c == '/' || c > 127;
}

static char *selecao_texto(void)
{
    Ponto a = sel_a, b = sel_b;
    size_t cap = 1024, n = 0;
    char *s;
    long ln;

    if (!sel_ativa) return NULL;
    if (a.linha > b.linha || (a.linha == b.linha && a.col > b.col)) { Ponto t = a; a = b; b = t; }
    s = malloc(cap);
    if (!s) return NULL;

    for (ln = a.linha; ln <= b.linha; ln++) {
        Celula *l = linha_abs(ln);
        int de = (ln == a.linha) ? a.col : 0;
        int ate = (ln == b.linha) ? b.col : colunas - 1;
        int x, ultimo = de - 1;
        if (!l) continue;
        for (x = de; x <= ate && x < colunas; x++)
            if (l[x].cp != ' ' || (l[x].atr & ATR_CONT)) ultimo = x;
        /* A cauda de espacos e enchimento da grade, nao texto: colar um
         * comando com 60 espacos no fim e o jeito mais rapido de estragar
         * um `git commit -m`. */
        for (x = de; x <= ultimo; x++) {
            char u[6];
            int k = 0;
            FcChar32 c = l[x].cp;
            if (l[x].atr & ATR_CONT) continue;
            if (c < 0x80) u[k++] = (char) c;
            else if (c < 0x800) { u[k++] = 0xc0 | (c >> 6); u[k++] = 0x80 | (c & 0x3f); }
            else if (c < 0x10000) { u[k++] = 0xe0 | (c >> 12); u[k++] = 0x80 | ((c >> 6) & 0x3f); u[k++] = 0x80 | (c & 0x3f); }
            else { u[k++] = 0xf0 | (c >> 18); u[k++] = 0x80 | ((c >> 12) & 0x3f); u[k++] = 0x80 | ((c >> 6) & 0x3f); u[k++] = 0x80 | (c & 0x3f); }
            if (n + k + 2 > cap) { cap *= 2; s = realloc(s, cap); if (!s) return NULL; }
            memcpy(s + n, u, k); n += k;
        }
        if (ln < b.linha) s[n++] = '\n';
    }
    s[n] = 0;
    return s;
}

static void tomar_selecao(Atom qual)
{
    free(sel_texto);
    sel_texto = selecao_texto();
    if (!sel_texto) return;
    XSetSelectionOwner(dpy, qual, janela, CurrentTime);
}

static void pedir_colagem(Atom qual)
{
    XConvertSelection(dpy, qual, A_UTF8, A_UTF8, janela, CurrentTime);
}

static void colar(const char *s, int n)
{
    if (colar_marcado) enviar_str("\033[200~");
    /* Um \n colado num shell EXECUTA a linha. Trocar por \r e o que o Enter
     * de verdade manda, e e o que os outros terminais fazem. */
    {
        int i, ini = 0;
        for (i = 0; i < n; i++) {
            if (s[i] == '\n') {
                enviar(s + ini, i - ini);
                enviar("\r", 1);
                ini = i + 1;
            }
        }
        enviar(s + ini, n - ini);
    }
    if (colar_marcado) enviar_str("\033[201~");
}

/* ---------------------------------------------------------------- teclado */

static int modificadores(unsigned est)
{
    int m = 1;
    if (est & ShiftMask)   m += 1;
    if (est & Mod1Mask)    m += 2;
    if (est & ControlMask) m += 4;
    return m;
}

static void seta(char letra, int mod)
{
    char buf[32];
    if (mod > 1) snprintf(buf, sizeof buf, "\033[1;%d%c", mod, letra);
    else snprintf(buf, sizeof buf, "\033%c%c", teclas_app ? 'O' : '[', letra);
    enviar_str(buf);
}

static void til(int n, int mod)
{
    char buf[32];
    if (mod > 1) snprintf(buf, sizeof buf, "\033[%d;%d~", n, mod);
    else snprintf(buf, sizeof buf, "\033[%d~", n);
    enviar_str(buf);
}

static void rolar_vista(int n)
{
    rolagem += n;
    if (rolagem > hist_n) rolagem = hist_n;
    if (rolagem < 0) rolagem = 0;
    sujar_tudo();
}

static void tecla(XKeyEvent *ev)
{
    char buf[64];
    KeySym ks;
    Status st;
    int n, mod = modificadores(ev->state);

    n = Xutf8LookupString(xic, ev, buf, sizeof buf - 1, &ks, &st);
    if (st == XBufferOverflow) return;

    /* Atalhos da janela. Ctrl+Shift porque Ctrl+C sozinho e do programa que
     * esta rodando, e roubar isso de um terminal e imperdoavel. */
    if ((ev->state & ControlMask) && (ev->state & ShiftMask)) {
        switch (ks) {
        case XK_C: case XK_c: tomar_selecao(A_CLIPBOARD); return;
        case XK_V: case XK_v: pedir_colagem(A_CLIPBOARD); return;
        }
    }
    if ((ev->state & ShiftMask) && ks == XK_Insert) { pedir_colagem(A_CLIPBOARD); return; }
    if ((ev->state & ShiftMask) && ks == XK_Prior)  { rolar_vista(linhas / 2); return; }
    if ((ev->state & ShiftMask) && ks == XK_Next)   { rolar_vista(-linhas / 2); return; }

    /* Qualquer tecla que gere dado traz a vista de volta para o fim: e onde a
     * resposta do que se digitou vai aparecer. */
    if (rolagem) { rolagem = 0; sujar_tudo(); }

    switch (ks) {
    case XK_Up:    seta('A', mod); return;
    case XK_Down:  seta('B', mod); return;
    case XK_Right: seta('C', mod); return;
    case XK_Left:  seta('D', mod); return;
    case XK_Home:  case XK_KP_Home: seta('H', mod); return;
    case XK_End:   case XK_KP_End:  seta('F', mod); return;
    case XK_Insert:   til(2, mod); return;
    case XK_Delete: case XK_KP_Delete: til(3, mod); return;
    case XK_Prior:    til(5, mod); return;
    case XK_Next:     til(6, mod); return;
    case XK_BackSpace: enviar((ev->state & Mod1Mask) ? "\033\177" : "\177",
                              (ev->state & Mod1Mask) ? 2 : 1); return;
    case XK_ISO_Left_Tab: enviar_str("\033[Z"); return;
    case XK_F1: case XK_F2: case XK_F3: case XK_F4: {
        char b[16];
        if (mod > 1) snprintf(b, sizeof b, "\033[1;%d%c", mod, "PQRS"[ks - XK_F1]);
        else snprintf(b, sizeof b, "\033O%c", "PQRS"[ks - XK_F1]);
        enviar_str(b); return;
    }
    case XK_F5:  til(15, mod); return;
    case XK_F6:  til(17, mod); return;
    case XK_F7:  til(18, mod); return;
    case XK_F8:  til(19, mod); return;
    case XK_F9:  til(20, mod); return;
    case XK_F10: til(21, mod); return;
    case XK_F11: til(23, mod); return;
    case XK_F12: til(24, mod); return;
    case XK_Return: case XK_KP_Enter: enviar_str("\r"); return;
    }

    if (n > 0) {
        /* Alt+tecla vira ESC+tecla: e como o readline e o vim esperam o Meta. */
        if ((ev->state & Mod1Mask) && n == 1) {
            char b[2] = { 27, buf[0] };
            enviar(b, 2);
        } else {
            enviar(buf, n);
        }
    }
}

/* ------------------------------------------------------------------ mouse */

static void relatar_mouse(int botao, int x, int y, int solto, unsigned est)
{
    char b[64];
    int cb = botao;
    if (est & ShiftMask)   cb |= 4;
    if (est & Mod1Mask)    cb |= 8;
    if (est & ControlMask) cb |= 16;
    if (mouse_sgr) {
        snprintf(b, sizeof b, "\033[<%d;%d;%d%c", cb, x + 1, y + 1, solto ? 'm' : 'M');
    } else {
        if (solto) cb = 3;
        if (x > 222 || y > 222) return;
        snprintf(b, sizeof b, "\033[M%c%c%c", 32 + cb, 32 + x + 1, 32 + y + 1);
    }
    enviar_str(b);
}

static void ponto_da_tela(int px, int py, long *ln, int *col)
{
    int y = (py - margem) / alt_cel;
    int x = (px - margem) / larg_cel;
    if (y < 0) y = 0;
    if (y >= linhas) y = linhas - 1;
    if (x < 0) x = 0;
    if (x >= colunas) x = colunas - 1;
    *ln = desloc - rolagem + y;
    *col = x;
}

static void expandir_selecao(void)
{
    Celula *l;
    if (cliques == 2) {
        l = linha_abs(sel_a.linha);
        if (!l) return;
        while (sel_a.col > 0 && e_palavra(l[sel_a.col - 1].cp)) sel_a.col--;
        while (sel_b.col < colunas - 1 && e_palavra(l[sel_b.col + 1].cp)) sel_b.col++;
    } else if (cliques >= 3) {
        sel_a.col = 0;
        sel_b.col = colunas - 1;
    }
}

/* ------------------------------------------------------------------- pty */

static void ao_morrer_filho(int s) { (void) s; filho_morreu = 1; }

static void subir_filho(char **cmd)
{
    int escravo;
    char *nome;
    struct termios t;

    mestre = posix_openpt(O_RDWR | O_NOCTTY);
    if (mestre < 0 || grantpt(mestre) < 0 || unlockpt(mestre) < 0)
        morrer("nao consegui abrir um pty: %s", strerror(errno));
    nome = ptsname(mestre);
    if (!nome) morrer("ptsname falhou");

    filho = fork();
    if (filho < 0) morrer("fork falhou");

    if (filho == 0) {
        close(mestre);
        /* setsid ANTES de abrir o escravo: e o que faz este pty virar o
         * terminal de CONTROLE do novo grupo. Sem isso o Ctrl+C nao gera
         * SIGINT em ninguem e o shell fica surdo ao sinal. */
        if (setsid() < 0) _exit(1);
        escravo = open(nome, O_RDWR);
        if (escravo < 0) _exit(1);
#ifdef TIOCSCTTY
        ioctl(escravo, TIOCSCTTY, 0);
#endif
        if (tcgetattr(escravo, &t) == 0) {
            t.c_cc[VERASE] = 0177;   /* casa com o \177 que o BackSpace manda */
            tcsetattr(escravo, TCSANOW, &t);
        }
        dup2(escravo, 0); dup2(escravo, 1); dup2(escravo, 2);
        if (escravo > 2) close(escravo);

        /* xterm-256color e uma promessa: quem le esse terminfo vai mandar
         * tela alternativa, regiao de rolagem, 256 cores e as sequencias de
         * funcao — tudo o que este parser implementa. Prometer mais do que se
         * cumpre e como se quebra um terminal caseiro. */
        setenv("TERM", "xterm-256color", 1);
        unsetenv("COLUMNS"); unsetenv("LINES");

        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);

        if (cmd && cmd[0]) execvp(cmd[0], cmd);
        else {
            const char *sh = getenv("SHELL");
            if (!sh) { struct passwd *p = getpwuid(getuid()); sh = p && p->pw_shell ? p->pw_shell : "/bin/sh"; }
            execl(sh, sh, "-i", (char *) NULL);
        }
        _exit(127);
    }

    fcntl(mestre, F_SETFL, O_NONBLOCK);
    signal(SIGCHLD, ao_morrer_filho);
}

/* ------------------------------------------------------------------- main */

static void selecao_pedida(XSelectionRequestEvent *r)
{
    XSelectionEvent resp;
    resp.type = SelectionNotify;
    resp.requestor = r->requestor;
    resp.selection = r->selection;
    resp.target = r->target;
    resp.time = r->time;
    resp.property = None;

    if (r->target == A_TARGETS) {
        Atom alvos[3] = { A_TARGETS, A_UTF8, XA_STRING };
        XChangeProperty(dpy, r->requestor, r->property, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *) alvos, 3);
        resp.property = r->property;
    } else if ((r->target == A_UTF8 || r->target == XA_STRING) && sel_texto) {
        XChangeProperty(dpy, r->requestor, r->property, r->target, 8,
                        PropModeReplace, (unsigned char *) sel_texto, strlen(sel_texto));
        resp.property = r->property;
    }
    XSendEvent(dpy, r->requestor, False, 0, (XEvent *) &resp);
}

static void selecao_chegou(XSelectionEvent *e)
{
    Atom tipo;
    int fmt;
    unsigned long n, resto;
    unsigned char *dados = NULL;

    if (e->property == None) return;
    if (XGetWindowProperty(dpy, janela, e->property, 0, (1 << 22), True, AnyPropertyType,
                           &tipo, &fmt, &n, &resto, &dados) != Success) return;
    if (tipo == A_INCR) { XFree(dados); return; }   /* colagem gigante: ignorada */
    if (dados) { colar((char *) dados, (int) n); XFree(dados); }
}

static void evento(XEvent *ev)
{
    switch (ev->type) {
    case Expose:
        if (ev->xexpose.count == 0) redesenhar(1);
        break;
    case ConfigureNotify:
        redimensionar(ev->xconfigure.width, ev->xconfigure.height);
        XftDrawChange(desenho, janela);
        break;
    case KeyPress:
        tecla(&ev->xkey);
        break;
    case FocusIn:  foco_nosso = 1; XSetICFocus(xic); sujo[cy] = 1; break;
    case FocusOut: foco_nosso = 0; XUnsetICFocus(xic); sujo[cy] = 1; break;
    case ButtonPress: {
        XButtonEvent *b = &ev->xbutton;
        long ln; int col;

        if (b->button == 4 || b->button == 5) {
            if (mouse_modo && !(b->state & ShiftMask)) {
                long l2; int c2;
                ponto_da_tela(b->x, b->y, &l2, &c2);
                relatar_mouse(b->button == 4 ? 64 : 65, c2,
                              (int) (l2 - desloc + rolagem), 0, b->state);
            } else if (alt_ativa) {
                /* Na tela alternativa nao ha historico para rolar: a roda vira
                 * seta, que e o que o less e o man entendem sem pedir nada. */
                int i;
                for (i = 0; i < 3; i++) seta(b->button == 4 ? 'A' : 'B', 1);
            } else {
                rolar_vista(b->button == 4 ? 3 : -3);
                redesenhar(1);
            }
            break;
        }
        ponto_da_tela(b->x, b->y, &ln, &col);
        if (mouse_modo && !(b->state & ShiftMask)) {
            relatar_mouse(b->button - 1, col, (int)(ln - desloc + rolagem), 0, b->state);
            break;
        }
        if (b->button == 2) { pedir_colagem(XA_PRIMARY); break; }
        if (b->button == 1) {
            cliques = (b->time - ultimo_clique < 400) ? cliques + 1 : 1;
            ultimo_clique = b->time;
            sel_a.linha = ln; sel_a.col = col;
            sel_b = sel_a;
            sel_ativa = 1;
            sel_arrastando = 1;
            expandir_selecao();
            redesenhar(1);
        }
        break;
    }
    case MotionNotify: {
        XMotionEvent *m = &ev->xmotion;
        long ln; int col;
        if (mouse_modo == 1002 || mouse_modo == 1003) {
            ponto_da_tela(m->x, m->y, &ln, &col);
            if (m->state & (Button1Mask | Button2Mask | Button3Mask) || mouse_modo == 1003)
                relatar_mouse(32, col, (int)(ln - desloc + rolagem), 0, m->state);
            break;
        }
        if (!sel_arrastando) break;
        ponto_da_tela(m->x, m->y, &ln, &col);
        if (ln != sel_b.linha || col != sel_b.col) {
            sel_b.linha = ln; sel_b.col = col;
            expandir_selecao();
            redesenhar(1);
        }
        break;
    }
    case ButtonRelease: {
        XButtonEvent *b = &ev->xbutton;
        long ln; int col;
        if (mouse_modo && !(b->state & ShiftMask) && b->button < 4) {
            ponto_da_tela(b->x, b->y, &ln, &col);
            relatar_mouse(b->button - 1, col, (int)(ln - desloc + rolagem), 1, b->state);
            break;
        }
        if (sel_arrastando) {
            sel_arrastando = 0;
            /* PRIMARY na hora de soltar: e o "selecionou, ja da para colar com
             * o botao do meio" que todo mundo espera de um terminal X. */
            if (sel_a.linha != sel_b.linha || sel_a.col != sel_b.col) tomar_selecao(XA_PRIMARY);
            else { sel_ativa = 0; redesenhar(1); }
        }
        break;
    }
    case SelectionRequest: selecao_pedida(&ev->xselectionrequest); break;
    case SelectionNotify:  selecao_chegou(&ev->xselection); break;
    case SelectionClear:   sel_ativa = 0; redesenhar(1); break;
    case ClientMessage:
        if ((Atom) ev->xclient.data.l[0] == A_WM_DELETE) exit(0);
        break;
    }
}

static void uso(void)
{
    fprintf(stderr,
        "uso: terminal [-fa fonte] [-fs tamanho] [-g COLSxLINS] [-T titulo]\n"
        "              [-sl linhas_de_historico] [-e comando ...]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *nome_fonte = "DejaVu Sans Mono";
    const char *titulo = "terminal";
    double tam = 10;
    char **cmd = NULL;
    int i, y;
    XSetWindowAttributes at;
    XClassHint ch;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-e")) { cmd = &argv[i + 1]; break; }
        else if (!strcmp(argv[i], "-fa") && i + 1 < argc) nome_fonte = argv[++i];
        else if (!strcmp(argv[i], "-fs") && i + 1 < argc) tam = atof(argv[++i]);
        else if (!strcmp(argv[i], "-T") && i + 1 < argc) titulo = argv[++i];
        else if (!strcmp(argv[i], "-sl") && i + 1 < argc) hist_max = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-g") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &colunas, &linhas) != 2) uso();
        } else uso();
    }
    if (hist_max < 0) hist_max = 0;

    /* O wcwidth() so sabe a largura de verdade com o locale montado; sem isto
     * todo caractere fora do ASCII responde -1 e o CJK sai por cima do vizinho. */
    setlocale(LC_CTYPE, "");
    if (!XSupportsLocale()) fprintf(stderr, "aviso: locale sem suporte no X\n");
    XSetLocaleModifiers("");

    dpy = XOpenDisplay(NULL);
    if (!dpy) morrer("nao abri o display %s", getenv("DISPLAY") ? getenv("DISPLAY") : "(vazio)");
    tela_num = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, tela_num);
    cmap = DefaultColormap(dpy, tela_num);

    cores_iniciar();
    fontes_abrir(nome_fonte, tam);

    fg_atual = padrao_fg; bg_atual = padrao_bg;

    at.background_pixel = cor_de(padrao_bg)->pixel;
    at.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask |
                    FocusChangeMask | ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask;
    janela = XCreateWindow(dpy, RootWindow(dpy, tela_num), 0, 0,
                           colunas * larg_cel + 2 * margem,
                           linhas * alt_cel + 2 * margem, 0,
                           DefaultDepth(dpy, tela_num), InputOutput, visual,
                           CWBackPixel | CWEventMask, &at);

    A_WM_DELETE = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    A_UTF8      = XInternAtom(dpy, "UTF8_STRING", False);
    A_NET_NAME  = XInternAtom(dpy, "_NET_WM_NAME", False);
    A_CLIPBOARD = XInternAtom(dpy, "CLIPBOARD", False);
    A_TARGETS   = XInternAtom(dpy, "TARGETS", False);
    A_INCR      = XInternAtom(dpy, "INCR", False);
    XSetWMProtocols(dpy, janela, &A_WM_DELETE, 1);

    ch.res_name = "terminal"; ch.res_class = "Terminal";
    XSetClassHint(dpy, janela, &ch);
    titulo_por(titulo);
    dicas_tamanho();

    gc = XCreateGC(dpy, janela, 0, NULL);
    desenho = XftDrawCreate(dpy, janela, visual, cmap);

    /* Sem XIM nao ha acento morto: no ABNT2 o ' seguido de a tem de virar a
     * com acento, e quem compoe isso e o metodo de entrada, nao o Xlib cru. */
    xim = XOpenIM(dpy, NULL, NULL, NULL);
    if (!xim) { XSetLocaleModifiers("@im=none"); xim = XOpenIM(dpy, NULL, NULL, NULL); }
    if (!xim) morrer("sem metodo de entrada (XOpenIM falhou)");
    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, janela, XNFocusWindow, janela, NULL);
    if (!xic) morrer("sem contexto de entrada (XCreateIC falhou)");

    tela = malloc(sizeof(Celula *) * linhas);
    sujo = malloc(linhas);
    paradas = malloc(colunas);
    hist = hist_max ? calloc(hist_max, sizeof(Celula *)) : NULL;
    if (!tela || !sujo || !paradas || (hist_max && !hist)) morrer("sem memoria");
    for (y = 0; y < linhas; y++) tela[y] = linha_nova();
    for (i = 0; i < colunas; i++) paradas[i] = (i % 8) == 0;
    topo_rol = 0; base_rol = linhas - 1;
    sujar_tudo();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, ao_pedir_despejo);
    subir_filho(cmd);
    avisar_pty();

    XMapWindow(dpy, janela);

    for (;;) {
        fd_set r;
        int nx = ConnectionNumber(dpy), maxf;
        int houve = 0;

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            /* XFilterEvent primeiro, SEMPRE: e ele quem engole a tecla do
             * acento morto enquanto a composicao nao terminou. */
            if (XFilterEvent(&ev, None)) continue;
            evento(&ev);
            houve = 1;
        }
        if (houve) { redesenhar(0); XFlush(dpy); }
        if (pedido_despejo) despejar();

        FD_ZERO(&r);
        FD_SET(nx, &r);
        if (mestre >= 0) FD_SET(mestre, &r);
        maxf = mestre > nx ? mestre : nx;

        if (select(maxf + 1, &r, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                if (filho_morreu) break;
                if (pedido_despejo) despejar();
                continue;
            }
            break;
        }

        if (mestre >= 0 && FD_ISSET(mestre, &r)) {
            char buf[65536];
            ssize_t n;
            int leu = 0;
            /* Ler ate esvaziar antes de desenhar: com `cat` de arquivo grande,
             * redesenhar a cada 64 KB gastaria a sessao RDP inteira mostrando
             * texto que ja rolou para fora. Uma leitura, um quadro. */
            while ((n = read(mestre, buf, sizeof buf)) > 0) {
                alimentar(buf, (int) n);
                leu = 1;
                if (n < (ssize_t) sizeof buf) break;
            }
            if (n <= 0 && (n == 0 || (errno != EAGAIN && errno != EINTR))) break;
            if (leu) {
                if (rolagem) { rolagem = 0; sujar_tudo(); }
                redesenhar(0);
                XFlush(dpy);
            }
        }
        if (filho_morreu) {
            int st;
            if (waitpid(filho, &st, WNOHANG) == filho) break;
        }
    }

    if (filho > 0) kill(filho, SIGHUP);
    XCloseDisplay(dpy);
    return 0;
}
