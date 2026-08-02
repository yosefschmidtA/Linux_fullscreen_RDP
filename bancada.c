/* bancada — a "oficina": arvore de arquivos, editor e Claude Code numa janela
 * so, sem terminal por baixo e sem toolkit.
 *
 *   gcc -O2 -Wall -o bancada bancada.c -lX11 -lXft $(pkg-config --cflags xft)
 *   DISPLAY=:10 ./bancada [pasta]
 *
 * POR QUE ELA EXISTE
 *
 * O VS Code custava 845 MB de PSS (medido em 01/08/2026, 11 processos) para
 * servir de arvore de arquivos e de editor ocasional — o resto do trabalho ja
 * era no terminal. Isto aqui e a mesma tela, sem as partes que nao se usavam:
 *
 *   +--------------------------------------------------------------+
 *   | [Projeto] [Salvar] [Claude]        caminho/do/arquivo.c   *   |
 *   +-------------+------------------------------------------------+
 *   |  arvore     | [Claude] [bancada.c * x] [README.md  x]        |
 *   |  de         +------------------------------------------------+
 *   |  arquivos   |  o painel da aba escolhida, ocupando tudo:     |
 *   |             |  ou o editor de um arquivo, ou o xterm com o   |
 *   |             |  Claude Code                                   |
 *   +-------------+------------------------------------------------+
 *
 * XFT, E NAO A FONTE CORE DA BARRA
 *
 * A barra-tarefas.c usa fonte CORE em iso8859-1, e isso e certo la: sao rotulos
 * ASCII de dez letras e o visual arcaico depende de nao haver antialiasing.
 * Aqui seria um desastre. Um editor com fonte iso8859-1 CORROMPE, ao salvar,
 * todo caractere fora do latin-1 — e o README deste projeto e cheio de "—",
 * "→" e acento. Editor que estraga arquivo e pior do que nenhum editor. Por
 * isso aqui entra o Xft: UTF-8 de verdade, e o custo e uma biblioteca a mais
 * NESTE binario, nao na barra.
 *
 * O TERMINAL E DE VERDADE, E EMBUTIDO
 *
 * O Claude Code e um programa de terminal; nao ha como desenha-lo com Xlib. Mas
 * o X11 deixa uma janela de outro cliente virar filha da nossa: o xterm aceita
 * "-into <id>" e passa a viver dentro do painel de baixo. Nao ha emulador de
 * terminal escrito aqui — seria um projeto inteiro — e mesmo assim a bancada
 * nao depende de terminal nenhum para existir.
 *
 * ABAS, E O CLAUDE COMO UMA DELAS
 *
 * O Claude morava num painel fixo embaixo, sempre roubando altura do editor
 * mesmo quando ninguem olhava para ele. Virou aba: o painel de baixo sumiu e o
 * conteudo escolhido ocupa a area inteira. A aba do Claude e a primeira, nao
 * fecha e nao tem arquivo; as outras sao arquivos abertos.
 *
 * A aba existe desde o inicio, mas o PROCESSO nao: a bancada sobe sem xterm e
 * sem Claude nenhum. Quem o levanta e o botao "Claude" da barra, ou um clique
 * no painel vazio da aba - que ate 02/08/2026 anunciava isso por escrito e
 * hoje e so preto, a pedido de quem usa. A bancada abre em segundos e sem os
 * 490 MB do Claude para quem so ia olhar um arquivo; e quando ele nasce, nasce
 * no projeto que estiver aberto na hora.
 *
 * Uma aba guarda o ESTADO INTEIRO de um arquivo (buffer, cursor, rolagem,
 * desfazer). O editor todo continua mexendo nos mesmos globais de sempre - a
 * troca de aba salva os globais numa struct e carrega os da outra. Sao
 * ponteiros e inteiros, dezenas de bytes: trocar de aba nao copia texto. Foi de
 * proposito, para nao ter de reescrever cada funcao de edicao em cima de um
 * ponteiro de contexto, que e onde se erra.
 *
 * A SESSAO
 *
 * As abas abertas voltam na proxima vez, por projeto, de
 * ~/.config/bancada/sessoes/. Grava-se so o que da para reconstruir do disco:
 * caminho, cursor e rolagem — nunca conteudo nao salvo. A conversa do Claude
 * nao entra; a aba dele sobe limpa, e quem quiser a anterior usa
 * `claude --continue` dentro dela.
 *
 * O QUE ESTE EDITOR NAO TEM, DE PROPOSITO
 *
 * Sem realce de sintaxe, sem busca, sem auto-completar. Para editar a serio,
 * nvim e vim estao no disco. Isto aqui e para abrir, olhar, corrigir uma linha
 * e salvar.
 * O que ele TEM de ter, e tem: UTF-8 correto, acento morto do teclado ABNT2,
 * desfazer, e aviso antes de perder alteracao.
 *
 * "Sem selecao de mouse, sem area de transferencia" estava nesta lista ate
 * 02/08/2026, e saiu dela. Hoje ele tem o que se espera de um editor:
 *
 *   mouse ....... arrastar marca, duplo clique pega a palavra, triplo a linha
 *   teclado ..... Shift+setas/Home/End estende a marca, Ctrl+A pega tudo
 *   Ctrl+C ...... copia (sem nada marcado, copia a linha do cursor)
 *   Ctrl+X ...... recorta
 *   Ctrl+V ...... cola do CLIPBOARD e, se nao houver dono, do PRIMARY
 *   digitar ..... sobre a marca, substitui; Backspace/Delete apagam a marca
 *   Alt+Cima/Baixo  move a linha do cursor
 *
 * Ver os blocos "Selecao de texto e area de transferencia" e "Colar".
 *
 * O QUE NAO E TEXTO: HEX E IMAGEM
 *
 * Ate aqui a bancada RECUSAVA o resto ("isso parece um arquivo binario") — e um
 * projeto tem binario compilado e imagem no meio dos fontes; clicar neles na
 * arvore so dava um dialogo de erro. Agora eles abrem em SOMENTE LEITURA: o
 * binario vira hex dump, a imagem aparece desenhada. Nenhum dos dois tem cursor,
 * nenhum dos dois salva — e o salvar recusa os dois na porta, porque gravar um
 * buffer de texto vazio por cima de um .png seria destruir o arquivo.
 *
 * A IMAGEM NAO TROUXE BIBLIOTECA DE IMAGEM NENHUMA. Quem decodifica e o
 * `convert` do imagemagick — ja dependencia declarada do projeto, pelo
 * barra-apps — chamado uma vez por abertura e devolvendo PPM cru pelo pipe: 16
 * MB de pico e 0,03 s para o Untitled.png (medido em 01/08/2026), e some. E a
 * mesma divisao de trabalho da barra-tarefas.c: pixels crus aqui dentro,
 * formato de imagem la fora. A libpng ate ja esta no processo (a freetype, que
 * a Xft usa, a carrega), entao linka-la nao custaria RSS — custaria o decodifi-
 * cador escrito a mao, e so resolveria PNG. Pelo pipe vem PNG, JPEG, GIF, BMP,
 * WEBP e TIFF por um caminho de codigo so.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/select.h>

/* ---- medidas ------------------------------------------------------------- */
#define BARRA_H    28          /* barra de ferramentas do topo */
#define ABAS_H     24          /* faixa das abas, logo abaixo dela */
#define ARVORE_W  240          /* largura da coluna da esquerda */
#define PAD         2
#define GAP         4
#define MARGEM      6          /* respiro interno do texto */
#define FECHAR_W   14          /* caixinha do "x" dentro da aba */
#define ABA_W_MAX 200
#define ABA_W_MIN  60

#define MAX_ARQ  (8 * 1024 * 1024)   /* teto para trazer um arquivo para a RAM */
/* Imagem tem teto proprio, e maior: dela nao entra byte nenhum aqui. Quem le o
 * arquivo e o convert, e o que volta pelo pipe ja vem reduzido ao painel. */
#define MAX_IMG  (64 * 1024 * 1024)
#define HEX_COLS   16                /* bytes por linha do hex dump */
#define MAX_ABAS   16                /* incluindo a do Claude */
#define UNDO_N     32                /* instantaneos guardados, POR ABA */
/* Teto de memoria do desfazer, POR ABA. Era 8 MB quando havia um buffer so;
 * com 16 abas o mesmo numero viraria 128 MB de teto, o que briga com a premissa
 * de RAM baixa do projeto. Para arquivo de codigo normal 2 MB ainda dao os 32
 * passos inteiros. */
#define UNDO_BYTES (2 * 1024 * 1024)

/* ---- estado do X --------------------------------------------------------- */
static Display  *dpy;
static int       tela;
static Window    raiz, win, w_arv, w_ed, w_term;
static GC        gc;
static XftFont  *fonte;
static XftDraw  *dr_barra, *dr_arv, *dr_ed, *dr_term;
static XftColor  c_ink, c_fraco, c_sel, c_aberto;
static XIM       xim;
static XIC       xic;
static Atom      A_DELETE;
static Atom      A_AREA, A_UTF8, A_ALVOS, A_TEXTO;   /* selecao/area de transf. */
static Atom      A_COLA;                             /* onde a cola aterrissa */

static unsigned long FACE, FACE_ESC, HI, SH, INK, PAPEL, SEL_FUNDO;

static int larg = 1100, alt = 700;   /* tamanho da janela */
static int avanco, altura_lin, base_lin;   /* metrica da fonte monoespacada */

/* ---- projeto e arvore ---------------------------------------------------- */
typedef struct { char nome[256]; int dir; } Entrada;

static char     projeto[PATH_MAX];   /* raiz escolhida no botao "Projeto" */
static char     pasta[PATH_MAX];     /* pasta que a arvore mostra agora */
static Entrada *entradas;
static int      n_entradas, cap_entradas;
static int      arv_topo;            /* primeira linha visivel da arvore */

/* ---- o que a aba mostra ---------------------------------------------------
 * MODO_TEXTO e o unico que edita. Os outros dois sao janelas de leitura: nao tem
 * cursor, nao tem desfazer e nao salvam. Vale ZERO de proposito — assim toda aba
 * nascida de um memset() ja e de texto, como sempre foi. */
enum { MODO_TEXTO, MODO_HEX, MODO_IMG };

/* ---- buffer de texto ----------------------------------------------------- */
static char   **linhas;
static int      n_linhas, cap_linhas;
static char     arquivo[PATH_MAX];   /* vazio = nenhum arquivo aberto */
static int      sujo;                /* alterado desde o ultimo salvar */
static int      sem_nl_final;        /* o arquivo original nao terminava em \n */
static int      crlf;                /* o arquivo original usava \r\n */
static int      cur_l, cur_c;        /* cursor: linha, e COLUNA EM BYTES */
static int      topo, col0;          /* rolagem vertical e horizontal */

/* ---- arquivo que nao e texto ----------------------------------------------
 * Convivem com os de cima em vez de substitui-los: "topo" continua sendo a
 * rolagem, tambem no hex, e por isso a roda do mouse e o PageDown nao precisaram
 * de um segundo caminho. */
static int             modo;         /* MODO_TEXTO, MODO_HEX ou MODO_IMG */
static unsigned char  *bruto;        /* MODO_HEX: o arquivo inteiro, cru */
static size_t          bruto_n;
static Pixmap          img_pm;       /* MODO_IMG: os pixels, ja no servidor X */
static int             img_w, img_h;          /* tamanho em que ela foi desenhada */
static int             img_nat_w, img_nat_h;  /* tamanho de verdade, para a legenda */
static long            img_bytes;             /* tamanho do arquivo em disco */
static int             img_para_w, img_para_h;/* painel para o qual ela foi gerada */

/* ---- desfazer ------------------------------------------------------------ */
typedef struct { char **l; int n; int cur_l, cur_c; size_t bytes; } Instante;
static Instante undo[UNDO_N];
static int      undo_n, undo_pos;

/* ---- terminal embutido --------------------------------------------------- */
static pid_t    pid_term;

/* ---- sessao -------------------------------------------------------------- */
static int      restaurando;         /* lendo a sessao gravada agora */

static void desenhar(void);
static void redimensionar(void);
static void abrir_arquivo(const char *caminho);
static void carregar_pasta(const char *p);
static void seguir_cursor(void);
static void esticar_terminal(void);
static void mostrar_painel(void);
static int  claude_vivo(void);
static void ativar(int i);
static void fechar_aba(int i);

static void morrer(const char *m) { fprintf(stderr, "bancada: %s\n", m); exit(1); }

/* =========================================================================
 * UTF-8
 *
 * O editor guarda BYTES e conta CARACTERES. Toda a aritmetica de cursor passa
 * por aqui: mover uma casa nao e somar 1, e pular a sequencia inteira. Errar
 * isto e o jeito classico de um editor cortar um caractere ao meio e gravar
 * lixo no arquivo.
 * ========================================================================= */
static int cont_utf8(unsigned char c) { return (c & 0xC0) == 0x80; }

static int prox_car(const char *s, int i)
{
    if (!s[i]) return i;
    i++;
    while (s[i] && cont_utf8((unsigned char) s[i])) i++;
    return i;
}

static int car_anterior(const char *s, int i)
{
    if (i <= 0) return 0;
    i--;
    while (i > 0 && cont_utf8((unsigned char) s[i])) i--;
    return i;
}

/* Quantos caracteres ha nos primeiros n bytes - e a COLUNA na tela, porque a
 * fonte e monoespacada e todo caractere ocupa um avanco. */
static int colunas_ate(const char *s, int n)
{
    int i, k = 0;
    for (i = 0; i < n && s[i]; i++)
        if (!cont_utf8((unsigned char) s[i])) k++;
    return k;
}

static int byte_da_coluna(const char *s, int col)
{
    int i = 0, k = 0;
    while (s[i] && k < col) { i = prox_car(s, i); k++; }
    return i;
}

/* =========================================================================
 * Buffer de linhas
 * ========================================================================= */
static void linhas_cabe(int n)
{
    if (n <= cap_linhas) return;
    cap_linhas = n + n / 2 + 64;
    linhas = realloc(linhas, (size_t) cap_linhas * sizeof *linhas);
    if (!linhas) morrer("sem memoria");
}

static void limpar_buffer(void)
{
    int i;
    for (i = 0; i < n_linhas; i++) free(linhas[i]);
    n_linhas = 0;
}

static char *dup_str(const char *s) { char *p = strdup(s); if (!p) morrer("sem memoria"); return p; }

static void inserir_linha(int em, const char *txt)
{
    linhas_cabe(n_linhas + 1);
    memmove(&linhas[em + 1], &linhas[em], (size_t) (n_linhas - em) * sizeof *linhas);
    linhas[em] = dup_str(txt);
    n_linhas++;
}

static void remover_linha(int em)
{
    free(linhas[em]);
    memmove(&linhas[em], &linhas[em + 1], (size_t) (n_linhas - em - 1) * sizeof *linhas);
    n_linhas--;
}

/* =========================================================================
 * Desfazer
 *
 * Instantaneo do buffer inteiro, nao registro de operacoes. E a implementacao
 * mais simples que esta CERTA, e o custo esta limitado nos dois eixos: no
 * maximo UNDO_N instantaneos e UNDO_BYTES de memoria somada; o mais antigo sai
 * quando qualquer um dos dois estoura. Para arquivo de codigo normal isso da
 * dezenas de passos por alguns megabytes.
 * ========================================================================= */
static size_t peso(char **l, int n)
{
    size_t t = 0; int i;
    for (i = 0; i < n; i++) t += strlen(l[i]) + 1 + sizeof(char *);
    return t;
}

static void soltar_instante(Instante *in)
{
    int i;
    if (!in->l) return;
    for (i = 0; i < in->n; i++) free(in->l[i]);
    free(in->l);
    in->l = NULL; in->n = 0; in->bytes = 0;
}

static size_t undo_total(void)
{
    size_t t = 0; int i;
    for (i = 0; i < UNDO_N; i++) t += undo[i].bytes;
    return t;
}

static void guardar_instante(void)
{
    Instante *in;
    int i;

    /* Grava por cima do mais antigo quando o anel da a volta. */
    in = &undo[undo_pos];
    soltar_instante(in);

    in->l = malloc((size_t) (n_linhas + 1) * sizeof *in->l);
    if (!in->l) return;                     /* sem memoria: segue sem desfazer */
    for (i = 0; i < n_linhas; i++) in->l[i] = dup_str(linhas[i]);
    in->n = n_linhas;
    in->cur_l = cur_l; in->cur_c = cur_c;
    in->bytes = peso(in->l, in->n);

    undo_pos = (undo_pos + 1) % UNDO_N;
    if (undo_n < UNDO_N) undo_n++;

    while (undo_total() > UNDO_BYTES && undo_n > 1) {
        int velho = (undo_pos - undo_n + UNDO_N) % UNDO_N;
        soltar_instante(&undo[velho]);
        undo_n--;
    }
}

static void desfazer(void)
{
    Instante *in;
    int i, idx;

    if (undo_n == 0) return;
    idx = (undo_pos - 1 + UNDO_N) % UNDO_N;
    in = &undo[idx];
    if (!in->l) return;

    limpar_buffer();
    linhas_cabe(in->n + 1);
    for (i = 0; i < in->n; i++) linhas[i] = dup_str(in->l[i]);
    n_linhas = in->n;
    cur_l = in->cur_l; cur_c = in->cur_c;
    if (cur_l >= n_linhas) cur_l = n_linhas ? n_linhas - 1 : 0;
    if (n_linhas == 0) { inserir_linha(0, ""); cur_l = cur_c = 0; }

    soltar_instante(in);
    undo_pos = idx; undo_n--;
    sujo = 1;
}

/* AGRUPAMENTO. Sem isto, cada tecla vira um instantaneo e o Ctrl+Z anda letra
 * por letra - com 32 instantaneos, o desfazer nao alcancaria nem uma frase.
 * Aqui uma sequencia de digitacao vira UM passo: so se guarda instantaneo
 * quando o tipo de acao muda (digitar / apagar / estrutural) ou quando o cursor
 * foi movido por fora. Navegar zera o grupo, e por isso mover o cursor fecha o
 * passo atual - que e como um editor de verdade se comporta. */
enum { G_NENHUM, G_DIGITANDO, G_APAGANDO };
static int grupo;

static void talvez_guardar(int tipo)
{
    if (tipo == G_NENHUM || tipo != grupo) guardar_instante();
    grupo = tipo;
}

/* =========================================================================
 * Abas
 *
 * A aba 0 e sempre a do Claude: nao tem buffer, nao fecha, e o painel dela e o
 * xterm. As demais sao arquivos.
 *
 * O TRUQUE, e o motivo de isto caber em poucas linhas: a aba nao e um contexto
 * que as funcoes de edicao recebem, e um LUGAR ONDE OS GLOBAIS DORMEM. Trocar
 * de aba e guardar os globais na struct da aba que sai e carregar os da que
 * entra. Nada de texto se move - "linhas" e um ponteiro, e o anel de desfazer
 * sao 32 structs. Por isso inserir_texto(), desfazer() e seguir_cursor()
 * continuam exatamente como eram, sem uma linha a mais.
 *
 * "carregada" e a aba cujo buffer esta nos globais neste momento, ou -1 quando
 * nenhuma esta (no inicio, e quando a ultima aba de arquivo fecha). Ela NAO e
 * a mesma coisa que "atual": com a aba do Claude a frente, o buffer da ultima
 * aba de arquivo continua carregado, so nao aparece.
 * ========================================================================= */
typedef struct {
    int      claude;                  /* a aba do terminal */
    char     arquivo[PATH_MAX];
    char   **linhas;
    int      n_linhas, cap_linhas;
    int      sujo, sem_nl_final, crlf;
    int      cur_l, cur_c, topo, col0;
    Instante undo[UNDO_N];
    int      undo_n, undo_pos, grupo;
    int      modo;                    /* hex e imagem dormem aqui do mesmo jeito */
    unsigned char *bruto;
    size_t   bruto_n;
    Pixmap   img_pm;
    int      img_w, img_h, img_nat_w, img_nat_h, img_para_w, img_para_h;
    long     img_bytes;
    int      x, w;                    /* onde a aba ficou desenhada */
} Aba;

static Aba abas[MAX_ABAS];
static int n_abas;
static int atual;                     /* aba a frente */
static int carregada = -1;            /* aba cujo buffer esta nos globais */

static void globais_para_aba(Aba *a)
{
    snprintf(a->arquivo, sizeof a->arquivo, "%s", arquivo);
    a->linhas = linhas; a->n_linhas = n_linhas; a->cap_linhas = cap_linhas;
    a->sujo = sujo; a->sem_nl_final = sem_nl_final; a->crlf = crlf;
    a->cur_l = cur_l; a->cur_c = cur_c; a->topo = topo; a->col0 = col0;
    memcpy(a->undo, undo, sizeof undo);
    a->undo_n = undo_n; a->undo_pos = undo_pos; a->grupo = grupo;
    a->modo = modo; a->bruto = bruto; a->bruto_n = bruto_n;
    a->img_pm = img_pm; a->img_w = img_w; a->img_h = img_h;
    a->img_nat_w = img_nat_w; a->img_nat_h = img_nat_h; a->img_bytes = img_bytes;
    a->img_para_w = img_para_w; a->img_para_h = img_para_h;
}

static void aba_para_globais(Aba *a)
{
    snprintf(arquivo, sizeof arquivo, "%s", a->arquivo);
    linhas = a->linhas; n_linhas = a->n_linhas; cap_linhas = a->cap_linhas;
    sujo = a->sujo; sem_nl_final = a->sem_nl_final; crlf = a->crlf;
    cur_l = a->cur_l; cur_c = a->cur_c; topo = a->topo; col0 = a->col0;
    memcpy(undo, a->undo, sizeof undo);
    undo_n = a->undo_n; undo_pos = a->undo_pos; grupo = a->grupo;
    modo = a->modo; bruto = a->bruto; bruto_n = a->bruto_n;
    img_pm = a->img_pm; img_w = a->img_w; img_h = a->img_h;
    img_nat_w = a->img_nat_w; img_nat_h = a->img_nat_h; img_bytes = a->img_bytes;
    img_para_w = a->img_para_w; img_para_h = a->img_para_h;
}

/* Larga os globais SEM liberar nada - o que eles apontavam ja pertence a struct
 * de outra aba. Chamar limpar_buffer() ou esquecer_undo() aqui liberaria o
 * texto da aba vizinha, e o estrago so apareceria ao voltar para ela. Vale
 * igual para o "bruto" do hex e para o Pixmap da imagem: soltar aqui apagaria a
 * imagem que a aba do lado esta mostrando. */
static void globais_zerar(void)
{
    linhas = NULL; n_linhas = cap_linhas = 0;
    arquivo[0] = '\0';
    sujo = sem_nl_final = crlf = 0;
    cur_l = cur_c = topo = col0 = 0;
    memset(undo, 0, sizeof undo);
    undo_n = undo_pos = 0; grupo = G_NENHUM;
    modo = MODO_TEXTO; bruto = NULL; bruto_n = 0;
    img_pm = 0; img_w = img_h = img_nat_w = img_nat_h = 0;
    img_bytes = 0; img_para_w = img_para_h = 0;
}

/* Antes de olhar qualquer campo das structs, os globais tem de descer para a
 * aba carregada - la e que estao "sujo" e o cursor de verdade. */
static void sincronizar(void)
{
    if (carregada >= 0) globais_para_aba(&abas[carregada]);
}

static const char *nome_base(const char *p)
{
    const char *b = strrchr(p, '/');
    return b ? b + 1 : p;
}

static int aba_do_arquivo(const char *caminho)
{
    int i;
    for (i = 0; i < n_abas; i++)
        if (!abas[i].claude && !strcmp(abas[i].arquivo, caminho)) return i;
    return -1;
}

/* Devolve um buffer estatico: quem chamar tem de usar antes de chamar de novo. */
static const char *rotulo_aba(int i)
{
    static char r[300];
    if (abas[i].claude) snprintf(r, sizeof r, "Claude");
    else snprintf(r, sizeof r, "%s%s", nome_base(abas[i].arquivo),
                  abas[i].sujo ? " *" : "");
    return r;
}

/* =========================================================================
 * Arquivos
 * ========================================================================= */
static int e_binario(const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) if (b[i] == '\0') return 1;
    return 0;
}

/* Imagem pelo NUMERO MAGICO, nunca pela extensao: um print salvo sem extensao
 * nenhuma abre igual, e um ".png" que por dentro e JPEG nao vira hex dump. Quem
 * vai ler os seis formatos e o convert; aqui so se decide para onde o arquivo
 * vai. Basta o comeco do arquivo - por isso a decisao acontece antes de trazer
 * qualquer coisa para a RAM. */
static int e_imagem(const unsigned char *b, size_t n)
{
    if (n >= 8  && !memcmp(b, "\x89PNG\r\n\x1a\n", 8))                    return 1;
    if (n >= 3  && !memcmp(b, "\xff\xd8\xff", 3))                         return 1; /* JPEG */
    if (n >= 6  && (!memcmp(b, "GIF87a", 6) || !memcmp(b, "GIF89a", 6)))  return 1;
    if (n >= 2  && !memcmp(b, "BM", 2))                                   return 1; /* BMP */
    if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4))    return 1;
    if (n >= 4  && (!memcmp(b, "II*\0", 4) || !memcmp(b, "MM\0*", 4)))     return 1; /* TIFF */
    return 0;
}

/* Quantas linhas o hex dump tem. Nao existe array de linhas por tras disto: o
 * dump e formatado na hora, so a parte visivel. Guardar o dump do binario da
 * bancada (51 KB) como texto custaria uns 240 KB de linhas para nada. */
static int linhas_hex(void)
{
    return (int) ((bruto_n + HEX_COLS - 1) / HEX_COLS);
}

/* zenity, como no resto do projeto: e a escada de dialogo ja usada pelo
 * abrir-windows e pelo barra-apps, e escrever um seletor de arquivos em Xlib
 * seria mais codigo do que o editor inteiro.
 *
 * O texto vai pelo AMBIENTE, nao interpolado no comando: assim um caminho com
 * aspas ou cifrao nao vira comando. Aqui isso nao e teoria - os nomes vem de
 * nomes de arquivo, que sao seus e podem ter qualquer coisa. */
static int zenity(const char *tipo, const char *texto)
{
    char cmd[256];
    setenv("BANCADA_TXT", texto, 1);
    snprintf(cmd, sizeof cmd,
             "zenity --%s --title=bancada --width=380 --text=\"$BANCADA_TXT\" 2>/dev/null",
             tipo);
    return system(cmd) == 0;
}

static int perguntar(const char *t) { return zenity("question", t); }

/* Calado enquanto restaura a sessao: um arquivo que virou binario ou sumiu
 * desde a ultima vez daria uma fila de dialogos na abertura, antes mesmo de a
 * janela aparecer. Ele so nao volta como aba. */
static void avisar(const char *t)
{
    if (restaurando) return;
    (void) zenity("error", t);
}

/* Le o arquivo inteiro sem tocar em estado nenhum: devolve o conteudo (o
 * chamador libera) ou NULL. Ficou separado do resto porque, com abas, uma falha
 * no meio do caminho nao pode mais deixar uma aba vazia e quebrada para tras -
 * a aba so nasce depois que o arquivo passou por aqui.
 *
 * Serve texto E hex: quem decide o que fazer com os bytes e o chamador, que ja
 * cheirou o comeco do arquivo. O terminador vai um byte depois do fim para o
 * texto poder ser varrido com strchr(); o hex usa o tamanho e ignora isso. */
static char *ler_todo(const char *caminho, size_t *fora_n)
{
    FILE *f;
    struct stat st;
    char *todo;
    size_t n;

    if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    if (st.st_size > MAX_ARQ) { avisar("Arquivo grande demais para a bancada."); return NULL; }

    f = fopen(caminho, "rb");
    if (!f) return NULL;
    todo = malloc((size_t) st.st_size + 1);
    if (!todo) { fclose(f); return NULL; }
    n = fread(todo, 1, (size_t) st.st_size, f);
    fclose(f);
    todo[n] = '\0';

    *fora_n = n;
    return todo;
}

/* Consome o texto lido e o quebra em linhas nos globais, que ja tem de estar
 * vazios (aba nova) ou serem os desta mesma aba. */
static void texto_para_buffer(char *todo, size_t n)
{
    char *p, *q;

    /* Arquivo VAZIO nao pode ganhar uma linha em branco ao salvar: com n==0 o
     * teste antigo dava "termina em \n" e o salvar acrescentava um. */
    sem_nl_final = (n == 0) || (todo[n - 1] != '\n');
    crlf = (memchr(todo, '\r', n) != NULL);

    p = todo;
    while (1) {
        q = strchr(p, '\n');
        if (q) {
            *q = '\0';
            /* O \r sai da linha para nao virar caractere de tela, mas o
             * arquivo LEMBRA que era CRLF e volta CRLF no salvar. Converter
             * final de linha sem avisar e das piores coisas que um editor faz:
             * o diff seguinte mostra o arquivo inteiro mudado. */
            { size_t l = strlen(p); if (l && p[l - 1] == '\r') p[l - 1] = '\0'; }
            inserir_linha(n_linhas, p);
            p = q + 1;
        } else {
            if (*p || n_linhas == 0) inserir_linha(n_linhas, p);
            break;
        }
    }
    if (n_linhas == 0) inserir_linha(0, "");
    free(todo);
}

/* Abrir agora e SEMPRE abrir uma aba. O arquivo que ja esta aberto so vem para
 * a frente - e por isso sumiu daqui o "descartar alteracoes?": nada mais e
 * sobrescrito ao abrir. A pergunta migrou para fechar_aba(), que e onde o
 * trabalho passa a correr risco de verdade. */
static void abrir_arquivo(const char *caminho)
{
    unsigned char cheiro[512];
    struct stat st;
    char *todo = NULL;
    size_t n = 0, nc;
    FILE *f;
    int i, novo_modo;

    sincronizar();

    i = aba_do_arquivo(caminho);
    if (i >= 0) { ativar(i); return; }

    if (n_abas >= MAX_ABAS) { avisar("Abas demais abertas; feche alguma."); return; }

    /* O CHEIRO PRIMEIRO, e so uns bytes: e ele que diz se este arquivo tem de
     * caber na RAM (texto, hex) ou se nem precisa entrar aqui (imagem, que o
     * convert le do disco). Sem isto, uma foto de 30 MB seria lida inteira para
     * ser jogada fora em seguida. */
    if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode)) return;
    f = fopen(caminho, "rb");
    if (!f) { avisar("Nao consegui ler o arquivo."); return; }
    nc = fread(cheiro, 1, sizeof cheiro, f);
    fclose(f);

    if (e_imagem(cheiro, nc))                     novo_modo = MODO_IMG;
    else if (e_binario((const char *) cheiro, nc)) novo_modo = MODO_HEX;
    else                                          novo_modo = MODO_TEXTO;

    if (novo_modo == MODO_IMG) {
        if (st.st_size > MAX_IMG) { avisar("Imagem grande demais para a bancada."); return; }
    } else {
        todo = ler_todo(caminho, &n);
        if (!todo) return;
        /* O cheiro viu so o comeco. Um NUL mais adiante ainda pararia o
         * texto_para_buffer() no meio - ele varre com strchr() - e o Ctrl+S
         * gravaria o arquivo TRUNCADO. Por isso a palavra final e do arquivo
         * INTEIRO: qualquer NUL manda para o hex, que nao salva. E a mesma
         * garantia da recusa antiga, so que agora dando para ver o arquivo. */
        if (novo_modo == MODO_TEXTO && e_binario(todo, n)) novo_modo = MODO_HEX;
    }

    /* Daqui em diante nada pode falhar: os globais passam a ser da aba nova. */
    globais_zerar();
    carregada = n_abas;
    memset(&abas[n_abas], 0, sizeof abas[0]);
    n_abas++;

    modo = novo_modo;
    if (modo == MODO_TEXTO) {
        texto_para_buffer(todo, n);
    } else if (modo == MODO_HEX) {
        bruto = (unsigned char *) todo;    /* a aba passa a ser a dona destes bytes */
        bruto_n = n;
    } else {
        img_bytes = (long) st.st_size;     /* os pixels so nascem no primeiro desenho */
    }
    snprintf(arquivo, sizeof arquivo, "%s", caminho);
    cur_l = cur_c = topo = col0 = 0;
    sujo = 0;
    globais_para_aba(&abas[carregada]);

    ativar(carregada);
}

/* =========================================================================
 * Sessao — quais abas estavam abertas, por projeto
 *
 * Grava so o que da para reconstruir do disco: caminho, cursor e rolagem. NAO
 * grava conteudo nao salvo. Um editor que ressuscita texto que voce nao mandou
 * gravar precisa acertar isso SEMPRE, e errar uma vez custa o arquivo; aqui,
 * fechar com alteracao pendente continua perguntando, que e a garantia simples
 * e que ja funciona.
 *
 * A conversa do Claude nao entra: a aba dele sobe limpa. Quem quiser a anterior
 * usa `claude --continue` dentro da propria aba.
 * ========================================================================= */
static void caminho_sessao(char *fora, size_t n)
{
    const char *casa = getenv("HOME");
    char cod[PATH_MAX];
    size_t i;

    fora[0] = '\0';
    if (!casa) return;

    /* O caminho do projeto vira o nome do arquivo com "/" -> "%": legivel, sem
     * hash e sem colisao. Fundo demais nao ganha sessao gravada - melhor nao
     * gravar do que gravar num nome truncado, que colidiria com o do vizinho. */
    for (i = 0; projeto[i] && i < sizeof cod - 1; i++)
        cod[i] = (projeto[i] == '/') ? '%' : projeto[i];
    cod[i] = '\0';
    if (i > 200) return;

    snprintf(fora, n, "%s/.config/bancada/sessoes/%s", casa, cod);
}

static void salvar_sessao(void)
{
    char caminho[PATH_MAX], dir[PATH_MAX];
    const char *casa = getenv("HOME");
    size_t np;
    FILE *f;
    int i;

    if (restaurando || !casa) return;
    caminho_sessao(caminho, sizeof caminho);
    if (!caminho[0]) return;

    snprintf(dir, sizeof dir, "%s/.config", casa);                 mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/.config/bancada", casa);         mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/.config/bancada/sessoes", casa); mkdir(dir, 0755);

    sincronizar();
    f = fopen(caminho, "w");
    if (!f) return;

    np = strlen(projeto);
    fprintf(f, "# bancada: sessao de %s\n", projeto);
    fprintf(f, "atual %d\n", atual);
    for (i = 0; i < n_abas; i++) {
        if (abas[i].claude) continue;
        /* So o que esta DENTRO do projeto. Sem isto, trocar de projeto pelo
         * botao levaria os arquivos do projeto velho para a sessao do novo, e
         * a proxima abertura viria com arquivos que nao sao dali. */
        if (strncmp(abas[i].arquivo, projeto, np) != 0) continue;
        fprintf(f, "aba %d %d %d %d %s\n", abas[i].cur_l, abas[i].cur_c,
                abas[i].topo, abas[i].col0, abas[i].arquivo);
    }
    fclose(f);
}

static void restaurar_sessao(void)
{
    char caminho[PATH_MAX], linha[4200];
    FILE *f;
    int quer_atual = 0;

    caminho_sessao(caminho, sizeof caminho);
    if (!caminho[0]) return;
    f = fopen(caminho, "r");
    if (!f) return;

    restaurando = 1;
    while (fgets(linha, sizeof linha, f)) {
        int l, c, t, c0;
        char arq[4096];

        if (sscanf(linha, "atual %d", &quer_atual) == 1) continue;
        if (sscanf(linha, "aba %d %d %d %d %4095[^\n]", &l, &c, &t, &c0, arq) != 5)
            continue;

        abrir_arquivo(arq);
        /* So mexe no cursor se o arquivo REALMENTE abriu: ele pode ter sumido,
         * virado binario ou crescido demais desde a ultima vez. */
        if (carregada < 0 || strcmp(arquivo, arq) != 0) continue;

        /* Cursor so existe no texto; rolagem existe tambem no hex, e ali o
         * limite e o numero de linhas do DUMP. A imagem nao rola: fica no 0. */
        if (modo == MODO_TEXTO && l >= 0 && l < n_linhas) {
            cur_l = l;
            cur_c = (c > 0 && c <= (int) strlen(linhas[l])) ? c : 0;
        }
        {
            int lim = (modo == MODO_HEX)   ? linhas_hex()
                    : (modo == MODO_TEXTO) ? n_linhas : 0;
            topo = (t >= 0 && t < lim) ? t : 0;
            col0 = (modo == MODO_TEXTO && c0 > 0) ? c0 : 0;
        }
        globais_para_aba(&abas[carregada]);
    }
    fclose(f);

    restaurando = 0;
    if (quer_atual >= n_abas) quer_atual = n_abas - 1;
    if (quer_atual < 0) quer_atual = 0;
    ativar(quer_atual);
}

static int salvar(void)
{
    FILE *f;
    int i;

    if (!arquivo[0]) return 0;
    /* Hex e imagem nao tem buffer de texto: sem esta guarda, o Ctrl+S numa aba
     * dessas percorreria zero linhas e o fopen("wb") ja teria TRUNCADO o
     * arquivo — um .png de 27 KB viraria 0 byte, calado. */
    if (modo != MODO_TEXTO) return 0;
    f = fopen(arquivo, "wb");
    if (!f) { avisar("Nao consegui gravar o arquivo."); return 0; }
    for (i = 0; i < n_linhas; i++) {
        fputs(linhas[i], f);
        if (i < n_linhas - 1 || !sem_nl_final) {
            if (crlf) fputc('\r', f);
            fputc('\n', f);
        }
    }
    fclose(f);
    sujo = 0;
    return 1;
}

/* =========================================================================
 * Arvore de arquivos
 * ========================================================================= */
static int cmp_entrada(const void *a, const void *b)
{
    const Entrada *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;      /* pastas primeiro */
    return strcmp(x->nome, y->nome);
}

static void carregar_pasta(const char *p)
{
    DIR *d;
    struct dirent *e;
    char cheio[PATH_MAX];
    struct stat st;

    d = opendir(p);
    if (!d) return;
    snprintf(pasta, sizeof pasta, "%s", p);
    n_entradas = 0;
    arv_topo = 0;

    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".")) continue;
        if (e->d_name[0] == '.' && strcmp(e->d_name, "..")) continue;  /* ocultos fora */
        if (n_entradas >= cap_entradas) {
            cap_entradas = cap_entradas ? cap_entradas * 2 : 128;
            entradas = realloc(entradas, (size_t) cap_entradas * sizeof *entradas);
            if (!entradas) morrer("sem memoria");
        }
        snprintf(cheio, sizeof cheio, "%s/%s", p, e->d_name);
        snprintf(entradas[n_entradas].nome, sizeof entradas[0].nome, "%s", e->d_name);
        entradas[n_entradas].dir = (stat(cheio, &st) == 0 && S_ISDIR(st.st_mode));
        n_entradas++;
    }
    closedir(d);
    qsort(entradas, (size_t) n_entradas, sizeof *entradas, cmp_entrada);
}

/* =========================================================================
 * Geometria do painel
 *
 * O painel e a area a direita da arvore e abaixo das abas. O editor e o xterm
 * disputam esse MESMO retangulo - so um deles fica mapeado por vez. Enquanto
 * o Claude era um painel fixo embaixo, havia dois retangulos e o TERM_H
 * roubava altura do editor mesmo com ninguem olhando.
 * ========================================================================= */
#define CONT_X ARVORE_W
#define CONT_Y (BARRA_H + ABAS_H)

static int cont_w(void) { int w = larg - ARVORE_W;          return w < 80 ? 80 : w; }
static int cont_h(void) { int h = alt - BARRA_H - ABAS_H;   return h < altura_lin ? altura_lin : h; }

/* =========================================================================
 * Desenho — o mesmo bisel Motif da barra-tarefas.c, para as duas ferramentas
 * parecerem a mesma coisa.
 * ========================================================================= */
static void linha_px(Drawable d, int x0, int y0, int x1, int y1, unsigned long c)
{
    XSetForeground(dpy, gc, c);
    XDrawLine(dpy, d, gc, x0, y0, x1, y1);
}

static void bisel(Drawable d, int x, int y, int w, int h, int fundo, int afundado)
{
    unsigned long a = afundado ? SH : HI, b = afundado ? HI : SH;

    XSetForeground(dpy, gc, (unsigned long) fundo);
    XFillRectangle(dpy, d, gc, x, y, (unsigned) w, (unsigned) h);
    linha_px(d, x, y, x + w - 2, y, a);
    linha_px(d, x, y, x, y + h - 2, a);
    linha_px(d, x, y + h - 1, x + w - 1, y + h - 1, INK);
    linha_px(d, x + w - 1, y, x + w - 1, y + h - 1, INK);
    linha_px(d, x + 1, y + h - 2, x + w - 2, y + h - 2, b);
    linha_px(d, x + w - 2, y + 1, x + w - 2, y + h - 2, b);
}

static void texto(XftDraw *dr, int x, int y, const char *s, XftColor *cor)
{
    XftDrawStringUtf8(dr, cor, fonte, x, y, (const FcChar8 *) s, (int) strlen(s));
}

/* --- botoes da barra de ferramentas --------------------------------------- */
typedef struct { const char *rotulo; int x, w; } Botao;
static Botao botoes[] = { { "Projeto", 0, 74 }, { "Salvar", 0, 66 }, { "Claude", 0, 66 } };
#define N_BOTOES ((int) (sizeof botoes / sizeof botoes[0]))

static void desenhar_barra(void)
{
    int i, x = PAD;
    char titulo[PATH_MAX + 64];

    XSetForeground(dpy, gc, FACE);
    XFillRectangle(dpy, win, gc, 0, 0, (unsigned) larg, BARRA_H);

    for (i = 0; i < N_BOTOES; i++) {
        botoes[i].x = x;
        bisel(win, x, PAD, botoes[i].w, BARRA_H - 2 * PAD, (int) FACE, 0);
        texto(dr_barra, x + 10, PAD + base_lin + 3, botoes[i].rotulo, &c_ink);
        x += botoes[i].w + GAP;
    }

    /* O aviso de leitura fica no titulo, e nao no rotulo da aba: a aba ja e
     * curta e o nome do arquivo e o que se procura la. */
    snprintf(titulo, sizeof titulo, "%s%s",
             arquivo[0] ? arquivo : "(nenhum arquivo aberto)",
             modo == MODO_HEX ? "   [binario, somente leitura]" :
             modo == MODO_IMG ? "   [imagem, somente leitura]"  :
             sujo             ? "  *" : "");
    texto(dr_barra, x + 12, PAD + base_lin + 3, titulo, &c_fraco);
}

/* --- a faixa das abas ------------------------------------------------------
 * Desenha em duas passadas porque a largura de cada aba depende de quantas ha:
 * a primeira mede o que cada uma gostaria de ocupar, a segunda desenha ja
 * sabendo se coube. Nao ha rolagem de abas - quando nao cabe, todas encolhem
 * junto e o nome e cortado. E o comportamento que nao esconde aba nenhuma. */
static void desenhar_x(int cx, int cy)
{
    linha_px(win, cx,     cy,     cx + 6, cy + 6, INK);
    linha_px(win, cx + 6, cy,     cx,     cy + 6, INK);
}

static void desenhar_abas(void)
{
    int i, x = ARVORE_W + PAD;
    int disp = larg - ARVORE_W - 2 * PAD;
    int larguras[MAX_ABAS], soma = 0;
    int y = BARRA_H + PAD, h = ABAS_H - 2 * PAD;
    int linha_base = BARRA_H + (ABAS_H + base_lin) / 2 - 2;

    XSetForeground(dpy, gc, FACE);
    XFillRectangle(dpy, win, gc, ARVORE_W, BARRA_H, (unsigned) (larg - ARVORE_W), ABAS_H);

    for (i = 0; i < n_abas; i++) {
        const char *r = rotulo_aba(i);
        int w = MARGEM + colunas_ate(r, (int) strlen(r)) * avanco + MARGEM;
        if (!abas[i].claude) w += FECHAR_W;
        if (w > ABA_W_MAX) w = ABA_W_MAX;
        larguras[i] = w;
        soma += w + PAD;
    }
    if (soma > disp && n_abas > 0) {
        int w = (disp - n_abas * PAD) / n_abas;
        if (w < ABA_W_MIN) w = ABA_W_MIN;
        for (i = 0; i < n_abas; i++) larguras[i] = w;
    }

    for (i = 0; i < n_abas; i++) {
        const char *r = rotulo_aba(i);
        int w = larguras[i], ativa = (i == atual);
        int reservado = MARGEM + (abas[i].claude ? MARGEM : FECHAR_W);
        int cabe = (w - reservado) / avanco;
        char corte[300];

        abas[i].x = x;
        abas[i].w = w;

        /* a aba da frente sobe (bisel em relevo, face clara); as outras afundam */
        bisel(win, x, y, w, h, (int) (ativa ? FACE : FACE_ESC), !ativa);

        if (cabe < 0) cabe = 0;
        snprintf(corte, sizeof corte, "%.*s", byte_da_coluna(r, cabe), r);
        texto(dr_barra, x + MARGEM, linha_base, corte, ativa ? &c_ink : &c_fraco);

        if (!abas[i].claude)
            desenhar_x(x + w - FECHAR_W + 2, BARRA_H + ABAS_H / 2 - 3);

        x += w + PAD;
    }
}

static void desenhar_arvore(void)
{
    int i, y, vis;

    XSetForeground(dpy, gc, PAPEL);
    XFillRectangle(dpy, w_arv, gc, 0, 0, (unsigned) ARVORE_W, (unsigned) (alt - BARRA_H));

    /* cabecalho: a pasta atual, cortada pela esquerda quando nao cabe */
    {
        const char *p = pasta;
        int cabe = (ARVORE_W - 2 * MARGEM) / avanco;
        int n = (int) strlen(p);
        if (colunas_ate(p, n) > cabe) p = p + byte_da_coluna(p, colunas_ate(p, n) - cabe + 1);
        XSetForeground(dpy, gc, FACE);
        XFillRectangle(dpy, w_arv, gc, 0, 0, (unsigned) ARVORE_W, (unsigned) altura_lin + 2);
        texto(dr_arv, MARGEM, base_lin + 1, p, &c_fraco);
    }

    vis = (alt - BARRA_H - altura_lin - 2) / altura_lin;
    for (i = 0; i < vis && arv_topo + i < n_entradas; i++) {
        Entrada *e = &entradas[arv_topo + i];
        char rot[300];
        int em_aba = -1, frente = 0;

        y = altura_lin + 2 + i * altura_lin;
        snprintf(rot, sizeof rot, "%s%s", e->dir ? "[+] " : "    ", e->nome);

        if (!e->dir) {
            /* caminho inteiro, nao so o nome: dois arquivos com o mesmo
             * nome em pastas diferentes acenderiam os dois */
            char cheio[PATH_MAX + 300];
            snprintf(cheio, sizeof cheio, "%s/%s", pasta, e->nome);
            em_aba = aba_do_arquivo(cheio);
            frente = (em_aba >= 0 && em_aba == atual);
        }
        /* tres estados, agora que ha mais de um arquivo aberto por vez: o da
         * aba da frente acende; os que estao em outra aba ficam numa cor
         * propria, para se saber o que ja esta carregado sem ler as abas */
        if (frente) {
            XSetForeground(dpy, gc, SH);
            XFillRectangle(dpy, w_arv, gc, 0, y, (unsigned) ARVORE_W, (unsigned) altura_lin);
        }
        texto(dr_arv, MARGEM, y + base_lin, rot,
              frente ? &c_sel : (em_aba >= 0 ? &c_aberto : &c_ink));
    }
}

/* =========================================================================
 * Selecao de texto e area de transferencia
 *
 * Ate 02/08/2026 este editor nao tinha nem uma coisa nem outra, e o cabecalho
 * dizia isso com todas as letras. Passou a ter a pedido de quem usa - "nao
 * consigo ver o que estou selecionando e dar copiar". O resto da lista de
 * ausencias continua valendo: sem realce de sintaxe, sem busca, sem completar.
 *
 * A selecao mora nos globais do editor, como o cursor - e por isso e APAGADA em
 * toda troca de aba. Depois de trocar, os globais apontam para o buffer de outra
 * aba, e um intervalo guardado do arquivo anterior nao quer dizer nada no novo.
 * E o mesmo cuidado que ja governa o `globais_zerar()`.
 *
 * NAO EXISTE "area de transferencia" no X. O que existe e um DONO vivo: quem
 * copiou fica com o texto e responde ao SelectionRequest de quem colar. Por isso
 * o texto copiado tem de ficar guardado aqui, e por isso fechar a bancada leva o
 * que foi copiado junto - igual a qualquer app X sem gerenciador de area de
 * transferencia. Sao duas selecoes, e viramos donos das duas:
 *
 *   PRIMARY    a do X de sempre: marcou, ja esta la; cola com o botao do meio.
 *   CLIPBOARD  a do Ctrl+C / Ctrl+V, que e o que os apps de hoje esperam.
 * ========================================================================= */
static int   sel_on;                 /* ha intervalo marcado */
static int   sel_la, sel_ca;         /* ancora: onde o arraste comecou */
static int   sel_lb, sel_cb;         /* ponta: onde o mouse esta agora */
static int   arrastando;
static char *txt_pri, *txt_cli;      /* o texto que servimos em cada selecao */
static Time  t_clique;               /* para separar clique de duplo e triplo */
static int   l_clique, n_cliques;

static void sel_limpar(void) { sel_on = 0; arrastando = 0; }

/* A ancora pode estar DEPOIS da ponta (arraste para tras), entao ninguem le
 * sel_la/sel_lb direto: esta funcao devolve o par em ordem de leitura e ja preso
 * aos limites do buffer de AGORA. O preso nao e zelo excessivo - desfazer encurta
 * linhas debaixo de um intervalo ja marcado, e a alternativa e ler fora do
 * malloc. Devolve 0 quando nao ha nada selecionado de verdade. */
static int sel_ordem(int *la, int *ca, int *lb, int *cb)
{
    int n;

    if (!sel_on || carregada < 0 || modo != MODO_TEXTO || n_linhas == 0) return 0;

    if (sel_la < sel_lb || (sel_la == sel_lb && sel_ca <= sel_cb)) {
        *la = sel_la; *ca = sel_ca; *lb = sel_lb; *cb = sel_cb;
    } else {
        *la = sel_lb; *ca = sel_cb; *lb = sel_la; *cb = sel_ca;
    }
    if (*la < 0) { *la = 0; *ca = 0; }
    if (*lb >= n_linhas) { *lb = n_linhas - 1; *cb = (int) strlen(linhas[*lb]); }
    if (*la > *lb) return 0;
    n = (int) strlen(linhas[*la]); if (*ca > n) *ca = n;
    n = (int) strlen(linhas[*lb]); if (*cb > n) *cb = n;
    return !(*la == *lb && *ca == *cb);
}

/* O texto do intervalo, com \n entre as linhas. Devolve malloc ou NULL. */
static char *sel_texto(void)
{
    int la, ca, lb, cb, l;
    size_t n = 0;
    char *r, *p;

    if (!sel_ordem(&la, &ca, &lb, &cb)) return NULL;

    for (l = la; l <= lb; l++) {
        int i0 = (l == la) ? ca : 0;
        int i1 = (l == lb) ? cb : (int) strlen(linhas[l]);
        n += (size_t) (i1 - i0) + (l < lb ? 1u : 0u);
    }
    r = malloc(n + 1);
    if (!r) return NULL;
    p = r;
    for (l = la; l <= lb; l++) {
        int i0 = (l == la) ? ca : 0;
        int i1 = (l == lb) ? cb : (int) strlen(linhas[l]);
        memcpy(p, linhas[l] + i0, (size_t) (i1 - i0));
        p += i1 - i0;
        if (l < lb) *p++ = '\n';
    }
    *p = '\0';
    return r;
}

/* Tira o intervalo marcado do buffer e deixa o cursor onde ele comecava.
 *
 * NAO guarda instantaneo de desfazer, de proposito: quem chama e que sabe se
 * isto e um passo sozinho (Delete) ou a primeira metade de um par (recortar,
 * digitar por cima). Guardar aqui faria o Ctrl+Z andar meio passo de cada vez.
 *
 * A conta e uma so: a linha que sobra e o COMECO da primeira linha marcada
 * colado com o FIM da ultima. Tudo entre as duas some. Vale para marca dentro
 * de uma linha e para marca de vinte linhas - o caso de uma linha e o mesmo com
 * la == lb. */
static int apagar_selecao(void)
{
    int la, ca, lb, cb, l;
    char *nova;
    size_t n1, n2;

    if (!sel_ordem(&la, &ca, &lb, &cb)) return 0;

    n1 = (size_t) ca;                              /* comeco da primeira */
    n2 = strlen(linhas[lb]) - (size_t) cb;         /* fim da ultima */
    nova = malloc(n1 + n2 + 1);
    if (!nova) return 0;
    memcpy(nova, linhas[la], n1);
    memcpy(nova + n1, linhas[lb] + cb, n2 + 1);    /* leva o \0 junto */

    for (l = lb; l > la; l--) remover_linha(l);    /* de tras para frente */
    free(linhas[la]);
    linhas[la] = nova;

    cur_l = la; cur_c = ca;
    sel_limpar();
    sujo = 1;
    return 1;
}

/* Ctrl+A. */
static void sel_tudo(void)
{
    if (carregada < 0 || modo != MODO_TEXTO || n_linhas == 0) return;
    sel_la = 0; sel_ca = 0;
    sel_lb = n_linhas - 1;
    sel_cb = (int) strlen(linhas[sel_lb]);
    sel_on = !(sel_la == sel_lb && sel_ca == sel_cb);
}

/* Vira dono de uma selecao. CONSOME o texto: quem chama entrega e esquece. */
static void virar_dono(Atom qual, char *texto)
{
    char **guardado = (qual == A_AREA) ? &txt_cli : &txt_pri;

    if (!texto) return;
    free(*guardado);
    *guardado = texto;
    XSetSelectionOwner(dpy, qual, win, CurrentTime);
}

/* Ctrl+C. Sem intervalo marcado, copia a LINHA do cursor inteira, com o \n - e
 * o gesto mais comum aqui (pegar uma linha para levar embora) e o que o VS Code
 * tambem faz. Vai para as DUAS selecoes: o Ctrl+V de outro app le CLIPBOARD, o
 * botao do meio le PRIMARY. */
static void copiar(void)
{
    char *t = sel_texto();

    if (!t) {
        size_t n;
        if (carregada < 0 || modo != MODO_TEXTO || n_linhas == 0) return;
        n = strlen(linhas[cur_l]);
        t = malloc(n + 2);
        if (!t) return;
        memcpy(t, linhas[cur_l], n);
        t[n] = '\n';
        t[n + 1] = '\0';
    }
    virar_dono(A_AREA, t);
    virar_dono(XA_PRIMARY, strdup(t));   /* cada selecao guarda a SUA copia */
}

/* Alguem esta colando o que copiamos aqui. */
static void responder_selecao(XSelectionRequestEvent *r)
{
    XSelectionEvent aviso;
    const char *t = (r->selection == A_AREA) ? txt_cli : txt_pri;

    memset(&aviso, 0, sizeof aviso);
    aviso.type      = SelectionNotify;
    aviso.display   = r->display;
    aviso.requestor = r->requestor;
    aviso.selection = r->selection;
    aviso.target    = r->target;
    aviso.time      = r->time;
    aviso.property  = None;               /* None = recusado, e o padrao */

    if (t && r->property != None) {
        if (r->target == A_ALVOS) {
            Atom lista[] = { A_ALVOS, A_UTF8, XA_STRING, A_TEXTO };
            XChangeProperty(dpy, r->requestor, r->property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *) lista,
                            (int) (sizeof lista / sizeof lista[0]));
            aviso.property = r->property;
        } else if (r->target == A_UTF8 || r->target == XA_STRING ||
                   r->target == A_TEXTO) {
            /* STRING e, pela ICCCM, latin-1 - e mesmo assim devolvemos UTF-8,
             * porque e o que todo mundo faz e o que todo mundo espera. Quem se
             * importa pede UTF8_STRING, que e o caminho certo e o primeiro da
             * lista de alvos acima. */
            XChangeProperty(dpy, r->requestor, r->property, r->target, 8,
                            PropModeReplace, (const unsigned char *) t,
                            (int) strlen(t));
            aviso.property = r->property;
        }
    }
    XSendEvent(dpy, r->requestor, False, 0, (XEvent *) &aviso);
}

/* Onde um clique caiu, em linha e byte. O byte vem do byte_da_coluna(), que e
 * quem sabe de UTF-8: clicar no meio de um "a" com acento nao pode devolver o
 * segundo byte dele. */
static void pos_do_clique(int px, int py, int *l, int *c)
{
    int ln = (py < MARGEM) ? topo : topo + (py - MARGEM) / altura_lin;
    int cl = col0 + (px - MARGEM) / avanco;

    if (ln < 0) ln = 0;
    if (ln >= n_linhas) ln = n_linhas - 1;
    if (cl < 0) cl = 0;
    *l = ln;
    *c = byte_da_coluna(linhas[ln], cl);
}

/* Byte >= 0x80 conta como letra: assim o duplo clique pega "funcao" inteiro, e
 * nao "fun" + "cao". Nada de <ctype.h> aqui - isalnum() depende do locale, e
 * este teste nao pode mudar de comportamento com a variavel de ambiente. */
static int e_palavra(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

/* Duplo clique: a palavra sob o ponteiro. */
static void sel_palavra(int l, int c)
{
    const char *s = linhas[l];
    int n = (int) strlen(s), i = c, f = c;

    if (n == 0) return;
    if (i >= n) i = f = n - 1;
    if (!e_palavra((unsigned char) s[i])) return;    /* clicou no espaco: nada */
    while (i > 0 && e_palavra((unsigned char) s[i - 1])) i--;
    while (f < n && e_palavra((unsigned char) s[f]))   f++;
    sel_la = sel_lb = l;
    sel_ca = i; sel_cb = f;
    sel_on = 1;
}

/* Triplo clique: a linha inteira, com o \n - por isso a ponta cai na coluna 0
 * da linha seguinte, quando ela existe. */
static void sel_linha(int l)
{
    sel_la = l; sel_ca = 0;
    if (l + 1 < n_linhas) { sel_lb = l + 1; sel_cb = 0; }
    else                  { sel_lb = l; sel_cb = (int) strlen(linhas[l]); }
    sel_on = 1;
}

/* O realce, desenhado DEPOIS do texto: preenche o retangulo por cima do preto e
 * redesenha so o trecho selecionado em branco. Sai mais barato do que quebrar o
 * laco de baixo em tres pedacos por linha, e o cursor, que vem depois, continua
 * aparecendo por cima de tudo. */
static void desenhar_selecao(void)
{
    int la, ca, lb, cb, i;
    int vis = (cont_h() - 2 * MARGEM) / altura_lin;

    if (!sel_ordem(&la, &ca, &lb, &cb)) return;

    for (i = 0; i < vis && topo + i < n_linhas; i++) {
        int l = topo + i, corte, b0, b1, x0, x1, y;
        char *s, guardado;

        if (l < la || l > lb) continue;
        s     = linhas[l];
        corte = byte_da_coluna(s, col0);          /* primeiro byte visivel */
        b0    = (l == la) ? ca : 0;
        b1    = (l == lb) ? cb : (int) strlen(s);
        if (b0 < corte) b0 = corte;               /* rolado para fora, a esquerda */
        if (b1 < b0) continue;

        x0 = MARGEM + (colunas_ate(s, b0) - col0) * avanco;
        x1 = MARGEM + (colunas_ate(s, b1) - col0) * avanco;
        /* meia coluna a mais quando a selecao segue para a linha de baixo: e
         * assim que se ve que o \n entrou junto */
        if (l < lb) x1 += avanco / 2;
        y = MARGEM + i * altura_lin;

        XSetForeground(dpy, gc, SEL_FUNDO);
        XFillRectangle(dpy, w_ed, gc, x0, y,
                       (unsigned) (x1 - x0 > 0 ? x1 - x0 : 1), (unsigned) altura_lin);

        guardado = s[b1];                          /* corta, desenha, devolve */
        s[b1] = '\0';
        texto(dr_ed, x0, y + base_lin, s + b0, &c_sel);
        s[b1] = guardado;
    }
}

static void desenhar_editor(void)
{
    int w = cont_w(), h = cont_h();
    int vis = (h - 2 * MARGEM) / altura_lin, i;

    XSetForeground(dpy, gc, PAPEL);
    XFillRectangle(dpy, w_ed, gc, 0, 0, (unsigned) w, (unsigned) h);

    for (i = 0; i < vis && topo + i < n_linhas; i++) {
        const char *s = linhas[topo + i];
        int corte = byte_da_coluna(s, col0);
        int y = MARGEM + i * altura_lin + base_lin;
        texto(dr_ed, MARGEM, y, s + corte, &c_ink);
    }

    desenhar_selecao();

    /* cursor: barra vertical de 2px, sem piscar (piscar exigiria acordar o
     * processo duas vezes por segundo so para isso) */
    if (cur_l >= topo && cur_l < topo + vis) {
        int cx = MARGEM + (colunas_ate(linhas[cur_l], cur_c) - col0) * avanco;
        int cy = MARGEM + (cur_l - topo) * altura_lin;
        if (cx >= MARGEM - 1) {
            XSetForeground(dpy, gc, INK);
            XFillRectangle(dpy, w_ed, gc, cx, cy, 2, (unsigned) altura_lin);
        }
    }
}

/* =========================================================================
 * Os dois paineis de LEITURA: hex e imagem
 * ========================================================================= */

/* O hex dump classico: deslocamento, 16 bytes e as mesmas 16 posicoes em ASCII.
 * Nao ha buffer de linhas por tras - so as linhas VISIVEIS sao formadas, na
 * hora, num buffer de pilha. Tres colunas, tres cores: o deslocamento e o ASCII
 * em cinza, os bytes em preto, que e onde o olho vai. */
static void desenhar_hex(void)
{
    int w = cont_w(), h = cont_h();
    int vis = (h - 2 * MARGEM) / altura_lin;
    int total = linhas_hex(), i, j;

    XSetForeground(dpy, gc, PAPEL);
    XFillRectangle(dpy, w_ed, gc, 0, 0, (unsigned) w, (unsigned) h);

    for (i = 0; i < vis && topo + i < total; i++) {
        size_t base = (size_t) (topo + i) * HEX_COLS;
        char desl[24], hexa[4 * HEX_COLS], asc[HEX_COLS + 4];
        int y = MARGEM + i * altura_lin + base_lin;
        int p = 0, q = 0;

        snprintf(desl, sizeof desl, "%08lx", (unsigned long) base);

        for (j = 0; j < HEX_COLS; j++) {
            if (base + j < bruto_n)
                p += snprintf(hexa + p, sizeof hexa - (size_t) p, "%02x ",
                              bruto[base + j]);
            else
                p += snprintf(hexa + p, sizeof hexa - (size_t) p, "   ");
            /* o respiro no meio dos 16 e o que deixa contar byte com o dedo */
            if (j == HEX_COLS / 2 - 1) hexa[p++] = ' ';
        }
        hexa[p] = '\0';

        asc[q++] = '|';
        for (j = 0; j < HEX_COLS && base + j < bruto_n; j++) {
            unsigned char c = bruto[base + j];
            asc[q++] = (c >= 32 && c < 127) ? (char) c : '.';
        }
        asc[q++] = '|';
        asc[q] = '\0';

        texto(dr_ed, MARGEM,                     y, desl, &c_fraco);
        texto(dr_ed, MARGEM + 10 * avanco,       y, hexa, &c_ink);
        texto(dr_ed, MARGEM + (10 + p) * avanco, y, asc,  &c_fraco);
    }
}

/* Poe um componente 0-255 na posicao que ele ocupa na mascara do visual - a
 * mesma funcao do barra-tarefas.c, pelo mesmo motivo: chumbar "0xRRGGBB" faria a
 * imagem sair com as cores trocadas num servidor de outro arranjo, sem erro
 * nenhum para avisar. */
static unsigned long componente(unsigned long mascara, unsigned v)
{
    int desl = 0, bits = 0;
    unsigned long m = mascara;

    if (!mascara) return 0;
    while (!(m & 1)) { m >>= 1; desl++; }
    while (m & 1)    { m >>= 1; bits++; }
    if (bits < 8) v >>= (8 - bits);
    return ((unsigned long) v << desl) & mascara;
}

/* Chama o convert UMA vez e recebe PPM cru pelo pipe: "P6", largura, altura,
 * 255, e os pixels. O alfa e achatado sobre o #DEDEDE da face ANTES de sair de
 * la, como o barra-apps ja faz com os icones - assim nao chega canal alfa aqui e
 * nao ha nada para compor.
 *
 * O identify na frente, no mesmo pipe, e so pela legenda: e o tamanho DE VERDADE
 * da imagem, que o PPM ja reduzido nao conta mais. Sao dois processos em
 * sequencia, nunca ao mesmo tempo - o pico e o de um convert (16 MB e 0,03 s
 * para o Untitled.png, medido em 01/08/2026), e ele morre antes do desenho.
 *
 * O caminho vai pelo AMBIENTE, como no zenity(): nome de arquivo com aspas ou
 * cifrao nao pode virar comando, e aqui os nomes sao os do seu disco.
 *
 * O resultado vira Pixmap, do lado do servidor: redesenhar depois e um
 * XCopyArea, sem trafego e sem nada guardado deste lado. */
static void gerar_imagem(void)
{
    Visual *vis = DefaultVisual(dpy, tela);
    int prof = DefaultDepth(dpy, tela);
    char cmd[PATH_MAX + 512];
    int cx, cy, w = 0, h = 0, maxv = 0, x, y;
    unsigned char *pix;
    XImage *im;
    FILE *p;

    if (img_pm) { XFreePixmap(dpy, img_pm); img_pm = 0; }
    img_w = img_h = 0;
    img_para_w = cont_w();
    img_para_h = cont_h();

    cx = img_para_w - 2 * MARGEM;
    cy = img_para_h - 3 * MARGEM - altura_lin;      /* o resto e da legenda */
    if (cx < 16 || cy < 16) return;

    setenv("BANCADA_IMG", arquivo, 1);
    snprintf(cmd, sizeof cmd,
             "identify -format '%%w %%h\\n' \"$BANCADA_IMG\"'[0]' 2>/dev/null; "
             "convert \"$BANCADA_IMG\"'[0]' -background '#DEDEDE' -alpha remove "
             "-alpha off -resize '%dx%d>' -depth 8 ppm:- 2>/dev/null", cx, cy);
    p = popen(cmd, "r");
    if (!p) return;

    /* Sem identify (ou com ele falhando) a legenda perde o tamanho natural e o
     * resto continua: o que importa e a imagem. */
    if (fscanf(p, "%d %d", &img_nat_w, &img_nat_h) != 2) img_nat_w = img_nat_h = 0;

    /* O cabecalho do PPM: o scanf pula o branco entre os campos, inclusive o
     * "\n" que sobrou da linha do identify. Depois dele vem UM unico byte de
     * branco - e a partir do proximo ja e pixel. */
    if (fscanf(p, " P6 %d %d %d", &w, &h, &maxv) != 3 ||
        w <= 0 || h <= 0 || w > 10000 || h > 10000 || maxv != 255) {
        pclose(p);
        return;
    }
    fgetc(p);

    pix = malloc((size_t) w * (size_t) h * 3);
    if (!pix) { pclose(p); return; }
    if (fread(pix, 1, (size_t) w * (size_t) h * 3, p) != (size_t) w * (size_t) h * 3) {
        free(pix); pclose(p); return;                /* truncado: melhor nada */
    }
    pclose(p);

    im = XCreateImage(dpy, vis, prof, ZPixmap, 0, NULL, w, h, 32, 0);
    if (!im) { free(pix); return; }
    im->data = calloc((size_t) im->bytes_per_line, (size_t) h);
    if (!im->data) { XDestroyImage(im); free(pix); return; }

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const unsigned char *s = pix + ((size_t) y * w + x) * 3;
            XPutPixel(im, x, y,
                      componente(vis->red_mask,   s[0]) |
                      componente(vis->green_mask, s[1]) |
                      componente(vis->blue_mask,  s[2]));
        }

    img_pm = XCreatePixmap(dpy, raiz, (unsigned) w, (unsigned) h, (unsigned) prof);
    XPutImage(dpy, img_pm, gc, im, 0, 0, 0, 0, (unsigned) w, (unsigned) h);
    XDestroyImage(im);            /* leva o im->data junto */
    free(pix);
    img_w = w;
    img_h = h;
}

/* Fundo da FACE, e nao do PAPEL branco: e sobre o #DEDEDE que o convert achatou
 * o alfa, entao a moldura tem de ser a mesma cor, senao a borda de um PNG
 * transparente aparece como um retangulo cinza dentro do branco. */
static void desenhar_imagem(void)
{
    int w = cont_w(), h = cont_h();
    char legenda[300];
    int lw;

    XSetForeground(dpy, gc, FACE);
    XFillRectangle(dpy, w_ed, gc, 0, 0, (unsigned) w, (unsigned) h);

    /* So gera quando nao ha imagem ou quando o painel mudou de tamanho - o
     * convert nao pode rodar a cada Expose. */
    if (!img_pm || img_para_w != w || img_para_h != h) gerar_imagem();

    if (img_pm) {
        int ix = (w - img_w) / 2;
        int iy = (h - MARGEM - altura_lin - img_h) / 2;
        if (ix < 0) ix = 0;
        if (iy < MARGEM) iy = MARGEM;
        XCopyArea(dpy, img_pm, w_ed, gc, 0, 0,
                  (unsigned) img_w, (unsigned) img_h, ix, iy);

        if (img_nat_w > 0 && (img_nat_w != img_w || img_nat_h != img_h))
            snprintf(legenda, sizeof legenda, "%d x %d  ·  %ld KB  ·  reduzida para caber",
                     img_nat_w, img_nat_h, (img_bytes + 512) / 1024);
        else
            snprintf(legenda, sizeof legenda, "%d x %d  ·  %ld KB",
                     img_nat_w > 0 ? img_nat_w : img_w,
                     img_nat_w > 0 ? img_nat_h : img_h, (img_bytes + 512) / 1024);
    } else {
        snprintf(legenda, sizeof legenda,
                 "Nao consegui converter esta imagem (falta o imagemagick?)");
    }

    lw = colunas_ate(legenda, (int) strlen(legenda)) * avanco;
    texto(dr_ed, (w - lw) / 2 > 0 ? (w - lw) / 2 : MARGEM,
          h - MARGEM - altura_lin + base_lin, legenda, &c_fraco);
}

/* O painel da aba do Claude, enquanto ele nao subiu, e um retangulo preto e
 * mudo — e isso e o pedido, nao um esquecimento.
 *
 * Ate 02/08/2026 ele trazia um convite em duas linhas ("O Claude nao sobe
 * sozinho." / "Clique aqui, ou no botao Claude da barra, para abrir."), pela
 * razao de que quem caisse na aba pelo Ctrl+Tab nao teria como adivinhar o
 * clique. Saiu a pedido de quem usa: quem mora aqui ja sabe, e a frase so
 * ocupava a tela. As duas portas de entrada continuam iguais — o botao
 * [Claude] da barra e o clique no painel.
 *
 * Nada nosso desenha mais nesta janela, e por isso o w_term deixou de pedir
 * ExposureMask: o preto vem do background_pixel dela, que o proprio servidor X
 * repinta na area exposta, sem cliente nenhum no caminho. */

static void desenhar(void)
{
    sincronizar();          /* "sujo" e o cursor vivem nos globais; as abas leem deles */
    desenhar_barra();
    desenhar_abas();
    desenhar_arvore();
    /* Na aba do Claude quem desenha o painel e o xterm, e o w_ed nem esta
     * mapeado. Sem esta guarda, a aba do Claude sem arquivo nenhum carregado
     * cairia num linhas[cur_l] com linhas == NULL. */
    if (!abas[atual].claude && carregada >= 0) {
        if (modo == MODO_HEX)      desenhar_hex();
        else if (modo == MODO_IMG) desenhar_imagem();
        else                       desenhar_editor();
    }
    XFlush(dpy);
}

/* =========================================================================
 * Rolagem — o cursor manda; a tela segue.
 * ========================================================================= */
static void seguir_cursor(void)
{
    int vis, cabe, col;

    if (carregada < 0 || n_linhas == 0) return;   /* nenhuma aba de arquivo carregada */
    if (modo != MODO_TEXTO) return;               /* hex e imagem nao tem cursor */

    vis  = (cont_h() - 2 * MARGEM) / altura_lin;
    cabe = (cont_w() - 2 * MARGEM) / avanco;
    col  = colunas_ate(linhas[cur_l], cur_c);

    if (vis < 1) vis = 1;
    if (cur_l < topo) topo = cur_l;
    if (cur_l >= topo + vis) topo = cur_l - vis + 1;
    if (col < col0) col0 = col;
    if (col >= col0 + cabe) col0 = col - cabe + 1;
    if (col0 < 0) col0 = 0;
}

/* =========================================================================
 * Edicao
 * ========================================================================= */
static void inserir_texto(const char *t, int n)
{
    char *velha = linhas[cur_l];
    size_t lv = strlen(velha);
    char *nova;
    int i;

    for (i = 0; i < n; i++) {
        if (t[i] == '\n' || t[i] == '\r') {          /* Enter: parte a linha */
            char *resto = dup_str(velha + cur_c);
            velha[cur_c] = '\0';
            linhas[cur_l] = realloc(velha, (size_t) cur_c + 1);
            inserir_linha(cur_l + 1, resto);
            free(resto);
            cur_l++; cur_c = 0;
            velha = linhas[cur_l]; lv = strlen(velha);
            continue;
        }
        nova = malloc(lv + 2);
        if (!nova) return;
        memcpy(nova, velha, (size_t) cur_c);
        nova[cur_c] = t[i];
        memcpy(nova + cur_c + 1, velha + cur_c, lv - (size_t) cur_c + 1);
        free(velha);
        linhas[cur_l] = velha = nova;
        lv++; cur_c++;
    }
    sujo = 1;
}

static void apagar_atras(void)
{
    char *l = linhas[cur_l];

    if (cur_c > 0) {
        int ini = car_anterior(l, cur_c);
        memmove(l + ini, l + cur_c, strlen(l) - (size_t) cur_c + 1);
        cur_c = ini;
    } else if (cur_l > 0) {
        char *acima = linhas[cur_l - 1];
        size_t la = strlen(acima);
        char *junto = malloc(la + strlen(l) + 1);
        if (!junto) return;
        strcpy(junto, acima); strcat(junto, l);
        free(linhas[cur_l - 1]);
        linhas[cur_l - 1] = junto;
        remover_linha(cur_l);
        cur_l--; cur_c = (int) la;
    } else return;
    sujo = 1;
}

static void apagar_frente(void)
{
    char *l = linhas[cur_l];
    size_t n = strlen(l);

    if (cur_c < (int) n) {
        int fim = prox_car(l, cur_c);
        memmove(l + cur_c, l + fim, n - (size_t) fim + 1);
    } else if (cur_l < n_linhas - 1) {
        char *abaixo = linhas[cur_l + 1];
        char *junto = malloc(n + strlen(abaixo) + 1);
        if (!junto) return;
        strcpy(junto, l); strcat(junto, abaixo);
        free(linhas[cur_l]);
        linhas[cur_l] = junto;
        remover_linha(cur_l + 1);
    } else return;
    sujo = 1;
}

/* Alt+Cima / Alt+Baixo: leva a linha do cursor uma casa para cima ou para
 * baixo. Nao ha texto novo nem apagado - so duas linhas trocando de lugar -,
 * entao o buffer se resolve com uma troca de ponteiros. */
static void mover_linha(int d)
{
    int destino = cur_l + d;
    char *tmp;

    if (carregada < 0 || modo != MODO_TEXTO) return;
    if (destino < 0 || destino >= n_linhas) return;

    guardar_instante();
    grupo = G_NENHUM;
    tmp = linhas[cur_l];
    linhas[cur_l] = linhas[destino];
    linhas[destino] = tmp;
    cur_l = destino;
    if (cur_c > (int) strlen(linhas[cur_l])) cur_c = (int) strlen(linhas[cur_l]);
    sel_limpar();
    sujo = 1;
}

/* =========================================================================
 * Colar
 *
 * Pedir uma selecao no X e ASSINCRONO, e essa e a unica coisa dificil aqui:
 * o XConvertSelection() nao devolve texto nenhum, so avisa ao dono que alguem
 * quer. O texto chega depois, num SelectionNotify, e e o laco de eventos que
 * termina o servico. Por isso colar sao duas funcoes e nao uma.
 *
 * Pede-se primeiro o CLIPBOARD (o Ctrl+C dos aplicativos de hoje) e, se nao
 * houver dono, o PRIMARY (o "marcou, ja esta la" do X de sempre). Assim tanto
 * faz de onde veio o texto: do Ctrl+C de um navegador ou de um arraste no
 * xterm. Sem esse plano B, colar do terminal - que so povoa o PRIMARY - nao
 * funcionaria e pareceria bug.
 * ========================================================================= */
static void pedir_cola(Atom de)
{
    if (carregada < 0 || modo != MODO_TEXTO) return;
    XConvertSelection(dpy, de, A_UTF8, A_COLA, win, CurrentTime);
}

/* O texto chegou. Ele vem do mundo la fora, entao passa por um filtro antes de
 * entrar no arquivo:
 *
 *   \r sai   - texto do Windows vem com \r\n, e o inserir_texto() trata os dois
 *              como quebra de linha: uma linha viraria duas. Quem decide se o
 *              arquivo grava \r\n e a flag "crlf", na hora de salvar, nao a cola.
 *   controle sai - byte abaixo de 32 que nao seja \n ou \t nao tem como ser
 *              visto nem salvo de forma util; deixaria lixo invisivel no meio
 *              do codigo.
 *
 * O UTF-8 passa inteiro: byte >= 0x80 nao e tocado. */
static void receber_cola(const unsigned char *bruto_t, size_t n)
{
    char *limpo;
    size_t i, j = 0;

    if (n == 0 || carregada < 0 || modo != MODO_TEXTO) return;
    limpo = malloc(n + 1);
    if (!limpo) return;

    for (i = 0; i < n; i++) {
        unsigned char c = bruto_t[i];
        if (c == '\r') continue;
        if (c < 32 && c != '\n' && c != '\t') continue;
        limpo[j++] = (char) c;
    }
    limpo[j] = '\0';

    if (j > 0) {
        /* UM instantaneo para o par apagar+inserir: colar por cima de uma marca
         * tem de voltar de uma vez so no Ctrl+Z. */
        talvez_guardar(G_NENHUM);
        if (sel_on) apagar_selecao();
        inserir_texto(limpo, (int) j);
        grupo = G_NENHUM;
        seguir_cursor();
    }
    free(limpo);
}

/* =========================================================================
 * O Claude, embutido
 *
 * O binario vem do `trabalho onde`, que ja sabe procura-lo (nesta maquina ele
 * mora dentro da extensao do VS Code, num caminho com a versao). Assim ha um
 * lugar so no projeto que sabe disso.
 * ========================================================================= */
static void caminho_claude(char *fora, size_t n)
{
    FILE *f = popen("trabalho onde 2>/dev/null", "r");
    char linha[PATH_MAX] = "";

    if (f) { if (fgets(linha, sizeof linha, f)) { char *nl = strchr(linha, '\n'); if (nl) *nl = '\0'; } pclose(f); }
    snprintf(fora, n, "%s", linha[0] ? linha : "claude");
}

static void abrir_claude(void)
{
    char id[32], cmd[PATH_MAX * 2 + 64], cla[PATH_MAX];

    if (pid_term > 0) { kill(pid_term, SIGTERM); pid_term = 0; }

    caminho_claude(cla, sizeof cla);
    snprintf(id, sizeof id, "%lu", (unsigned long) w_term);
    snprintf(cmd, sizeof cmd, "cd '%s' && exec '%s'", projeto, cla);

    pid_term = fork();
    if (pid_term == 0) {
        setsid();
        execlp("xterm", "xterm", "-into", id,
               "-fa", "DejaVu Sans Mono", "-fs", "10",
               "-bg", "black", "-fg", "gray90", "-b", "2",
               "-e", "sh", "-c", cmd, (char *) NULL);
        _exit(127);
    }
}

/* Como o SIGCHLD e SIG_IGN, o filho morto some sozinho: nao ha wait() para
 * colher status, e o kill(pid,0) e o teste que sobra. Serve tanto para o
 * Claude que nunca subiu (pid_term == 0) quanto para o que saiu. */
static int claude_vivo(void)
{
    return pid_term > 0 && kill(pid_term, 0) == 0;
}

/* O botao "Claude" da barra traz a aba para a frente e, se o processo nao
 * estiver de pe, e ELE quem o levanta - na primeira vez como em qualquer
 * outra. E o unico caminho junto com o clique no painel vazio: nada de subir
 * Claude por conta propria. */
static void ir_para_claude(void)
{
    if (!claude_vivo()) abrir_claude();
    ativar(0);
}

/* Depois de um resize, o xterm nao se estica sozinho: com "-into" ele e apenas
 * filho da nossa janela e mantem a geometria com que nasceu. Quem o acerta e
 * este laco, que redimensiona os filhos do painel. */
static void esticar_terminal(void)
{
    Window r, pai, *filhos = NULL;
    unsigned n = 0, i;
    int w = cont_w(), h = cont_h();

    if (XQueryTree(dpy, w_term, &r, &pai, &filhos, &n) && filhos) {
        for (i = 0; i < n; i++)
            XMoveResizeWindow(dpy, filhos[i], 0, 0, (unsigned) w, (unsigned) h);
        XFree(filhos);
    }
}

/* =========================================================================
 * Geometria
 * ========================================================================= */
/* O editor e o xterm ocupam o MESMO retangulo; so um fica mapeado. */
static void redimensionar(void)
{
    unsigned w = (unsigned) cont_w(), h = (unsigned) cont_h();

    XMoveResizeWindow(dpy, w_arv, 0, BARRA_H, ARVORE_W, (unsigned) (alt - BARRA_H));
    XMoveResizeWindow(dpy, w_ed,   CONT_X, CONT_Y, w, h);
    XMoveResizeWindow(dpy, w_term, CONT_X, CONT_Y, w, h);
    esticar_terminal();
    seguir_cursor();
}

/* =========================================================================
 * Troca de aba
 * ========================================================================= */
static void focar_terminal(void)
{
    Window r, pai, *f = NULL;
    unsigned n = 0;

    if (XQueryTree(dpy, w_term, &r, &pai, &f, &n) && f && n)
        XSetInputFocus(dpy, f[0], RevertToParent, CurrentTime);
    if (f) XFree(f);
}

static void mostrar_painel(void)
{
    if (abas[atual].claude) {
        XUnmapWindow(dpy, w_ed);
        XMapWindow(dpy, w_term);
        /* Enquanto esteve escondido o xterm nao soube dos resizes: com "-into"
         * ele so obedece a quem o redimensiona de fora, e isso somos nos. */
        esticar_terminal();
        /* Sem processo nao ha xterm para receber foco, e deixa-lo onde estava
         * daria uma aba que come as teclas sem ninguem para trata-las. O foco
         * volta para a mae, que e quem tem o XIC e o teclado. */
        if (claude_vivo()) focar_terminal();
        else               XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    } else {
        XUnmapWindow(dpy, w_term);
        XMapWindow(dpy, w_ed);
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    }
}

static void ativar(int i)
{
    if (i < 0 || i >= n_abas) return;

    sincronizar();
    /* A selecao vive nos globais e vale para o buffer que estava neles - depois
     * da troca ela apontaria para dentro do texto de outro arquivo. */
    sel_limpar();
    if (!abas[i].claude) { aba_para_globais(&abas[i]); carregada = i; }
    atual = i;
    mostrar_painel();

    /* SEM seguir_cursor() aqui, e isso NAO e esquecimento. Ele existe para
     * arrastar a tela ATE o cursor, e o cursor so anda por acao do teclado.
     * Chamado na troca de aba, ele desfazia a rolagem de quem tinha descido o
     * arquivo com a roda do mouse sem mexer no cursor: rolar ate a linha 36,
     * ir para outra aba e voltar devolvia a aba na linha 1. Medido em
     * 01/08/2026. O topo e o col0 guardados na aba ja sao validos - foram
     * salvos num estado coerente e nenhum resize os invalida. */

    /* Toda mudanca de aba passa por aqui - abrir, fechar e trocar chamam
     * ativar(). Gravar neste ponto faz a sessao sobreviver tambem a um kill,
     * nao so a uma saida limpa. Sao poucas linhas num arquivo minusculo, no
     * ritmo de um clique humano. */
    salvar_sessao();
}

static void fechar_aba(int i)
{
    int k;

    if (i < 0 || i >= n_abas || abas[i].claude) return;   /* a do Claude nao fecha */

    sincronizar();
    if (abas[i].sujo) {
        char m[320];
        snprintf(m, sizeof m, "%s tem alteracoes nao salvas. Fechar assim mesmo?",
                 nome_base(abas[i].arquivo));
        if (!perguntar(m)) return;
    }

    /* Libera o que e DESTA aba. Nao da para usar limpar_buffer()/esquecer_undo(),
     * que mexem nos globais: a aba fechada pode nao ser a carregada. */
    for (k = 0; k < abas[i].n_linhas; k++) free(abas[i].linhas[k]);
    free(abas[i].linhas);
    for (k = 0; k < UNDO_N; k++) soltar_instante(&abas[i].undo[k]);
    free(abas[i].bruto);                                     /* hex */
    if (abas[i].img_pm) XFreePixmap(dpy, abas[i].img_pm);    /* imagem */

    if (carregada == i) { carregada = -1; globais_zerar(); }
    else if (carregada > i) carregada--;

    memmove(&abas[i], &abas[i + 1], (size_t) (n_abas - i - 1) * sizeof *abas);
    n_abas--;

    if (atual > i || atual >= n_abas) atual--;
    if (atual < 0) atual = 0;
    ativar(atual);
}

/* =========================================================================
 * Eventos
 * ========================================================================= */
static void clique_arvore(int y)
{
    int i = (y - altura_lin - 2) / altura_lin + arv_topo;
    char cheio[PATH_MAX + 300];

    if (i < 0 || i >= n_entradas) return;
    snprintf(cheio, sizeof cheio, "%s/%s", pasta, entradas[i].nome);

    if (entradas[i].dir) {
        char real[PATH_MAX];
        if (realpath(cheio, real)) carregar_pasta(real);
    } else {
        /* realpath antes de abrir: o caminho volta canonico e cabe em PATH_MAX,
         * e e o mesmo que a arvore compara para acender a linha do arquivo
         * aberto. Sem isso, "a/./b" e "a/b" seriam dois arquivos diferentes. */
        char real[PATH_MAX];
        if (realpath(cheio, real)) abrir_arquivo(real);
    }
}

/* Sair passou a ser mais perigoso: pode haver alteracao pendente numa aba que
 * nem esta na tela. */
static int algum_sujo(void)
{
    int i;
    sincronizar();
    for (i = 0; i < n_abas; i++) if (abas[i].sujo) return 1;
    return 0;
}

static void clique_abas(int x)
{
    int i;

    for (i = 0; i < n_abas; i++)
        if (x >= abas[i].x && x < abas[i].x + abas[i].w) {
            if (!abas[i].claude && x >= abas[i].x + abas[i].w - FECHAR_W) fechar_aba(i);
            else ativar(i);
            return;
        }
}

static void escolher_projeto(void)
{
    FILE *f;
    char linha[PATH_MAX] = "", cmd[PATH_MAX + 160];

    snprintf(cmd, sizeof cmd,
             "zenity --file-selection --directory --title='Projeto' "
             "--filename='%s/' 2>/dev/null", projeto);
    f = popen(cmd, "r");
    if (!f) return;
    if (fgets(linha, sizeof linha, f)) { char *nl = strchr(linha, '\n'); if (nl) *nl = '\0'; }
    pclose(f);
    if (!linha[0]) return;

    salvar_sessao();                      /* fecha a sessao do projeto que sai */
    snprintf(projeto, sizeof projeto, "%s", linha);
    carregar_pasta(projeto);
    /* O Claude segue o projeto - mas so o que JA estava de pe. Trocar de
     * projeto nao e pedir Claude: se ele nao subiu, continua nao subindo, e a
     * primeira abertura ja vai nascer na pasta nova, que e a de agora. */
    if (claude_vivo()) abrir_claude();
    salvar_sessao();                      /* e abre a do que entra */
}

static void tecla(XKeyEvent *ev)
{
    char buf[64];
    KeySym ks = NoSymbol;
    Status st;
    int n = 0;
    int ctrl = (ev->state & ControlMask) != 0;
    int estendendo = 0;             /* Shift+movimento: a marca acompanha */

    if (xic) n = Xutf8LookupString(xic, ev, buf, sizeof buf - 1, &ks, &st);
    else     n = XLookupString(ev, buf, sizeof buf - 1, &ks, NULL);
    buf[n > 0 ? n : 0] = '\0';

    /* As teclas de ABA valem em qualquer aba, inclusive na do Claude - por isso
     * vem antes da guarda logo abaixo. */
    if (ctrl) {
        switch (ks) {
        case XK_Tab:          ativar((atual + 1) % n_abas); desenhar(); return;
        case XK_ISO_Left_Tab: ativar((atual - 1 + n_abas) % n_abas); desenhar(); return;
        case XK_w: case XK_W: fechar_aba(atual); desenhar(); return;
        }
    }

    /* Na aba do Claude o teclado e do xterm. As unicas teclas que chegam aqui
     * sao as capturadas por XGrabKey, ja tratadas acima; qualquer outra coisa
     * seria editar as escondidas um arquivo que nem esta na tela. */
    if (abas[atual].claude) return;

    if (ctrl) {
        switch (ks) {
        case XK_s: case XK_S: salvar(); desenhar(); return;
        /* Ctrl+C NAO passa por XGrabKey de proposito: com grab, ele deixaria de
         * chegar ao Claude da aba 0 - onde e o "interrompe isso" - e a bancada
         * ficaria roubando a tecla mais importante do terminal. Sem grab, ele so
         * chega quando a janela da bancada tem o foco, que e quando faz sentido. */
        case XK_c: case XK_C: copiar(); return;
        case XK_v: case XK_V: pedir_cola(A_AREA); return;   /* o resto e no laco */
        case XK_x: case XK_X:
            if (carregada < 0 || modo != MODO_TEXTO) return;
            copiar();                       /* sem marca, copia a linha... */
            if (sel_on) {                   /* ...e ai nao ha o que recortar */
                talvez_guardar(G_NENHUM);
                apagar_selecao();
                seguir_cursor();
            }
            desenhar();
            return;
        case XK_a: case XK_A:
            sel_tudo();
            if (sel_on) virar_dono(XA_PRIMARY, sel_texto());
            desenhar();
            return;
        case XK_z: case XK_Z: grupo = G_NENHUM; desfazer(); sel_limpar(); seguir_cursor(); desenhar(); return;
        case XK_q: case XK_Q:
            if (!algum_sujo() || perguntar("Ha alteracoes nao salvas. Sair mesmo assim?")) {
                salvar_sessao();
                if (pid_term > 0) kill(pid_term, SIGTERM);
                exit(0);
            }
            return;
        }
        return;                            /* outros Ctrl+ nao fazem nada */
    }

    if (carregada < 0) return;             /* nenhuma aba de arquivo carregada */

    /* SOMENTE LEITURA, e a guarda vem antes de tudo: dali para baixo cada ramo
     * mexe em linhas[cur_l], e no hex e na imagem "linhas" e NULL. Uma tecla
     * qualquer numa aba dessas nao pode virar segmentation fault. */
    if (modo != MODO_TEXTO) {
        int vis = (cont_h() - 2 * MARGEM) / altura_lin;
        int total = (modo == MODO_HEX) ? linhas_hex() : 0;

        switch (ks) {
        case XK_Up:    topo--;        break;
        case XK_Down:  topo++;        break;
        case XK_Prior: topo -= vis;   break;
        case XK_Next:  topo += vis;   break;
        case XK_Home:  topo = 0;      break;
        case XK_End:   topo = total - vis; break;
        default: return;
        }
        if (topo > total - 1) topo = total - 1;
        if (topo < 0) topo = 0;
        desenhar();
        return;
    }

    /* Alt+Cima / Alt+Baixo movem a linha. Vem antes de tudo porque nao e nem
     * movimento de cursor nem digitacao: as duas setas ja tem outro dono logo
     * abaixo, e sem esta saida antecipada elas so andariam com o cursor. */
    if ((ev->state & Mod1Mask) && (ks == XK_Up || ks == XK_Down)) {
        mover_linha(ks == XK_Up ? -1 : 1);
        seguir_cursor();
        desenhar();
        return;
    }

    /* O QUE A TECLA FAZ COM A MARCA. Tres caminhos, e a ordem importa:
     *
     *  1. Shift + movimento ESTENDE. Sem ancora ainda, o cursor de agora vira a
     *     ancora - e por isso a marca do teclado comeca de onde se estava, nao
     *     do inicio do arquivo.
     *  2. Uma tecla que ESCREVE, com marca de pe, apaga o marcado primeiro.
     *     Ate 02/08/2026 este editor nao fazia isso, e o comentario aqui dizia
     *     que era de proposito; virou o comportamento normal a pedido de quem
     *     usa, que e tambem o de qualquer editor.
     *  3. Qualquer outra tecla so apaga a marca.
     *
     * O par apagar+escrever guarda UM instantaneo: o `grupo` e forcado logo
     * depois para o `talvez_guardar()` do switch nao gravar o segundo. */
    {
        int shift = (ev->state & ShiftMask) != 0;
        int movimento = (ks == XK_Left || ks == XK_Right || ks == XK_Up ||
                         ks == XK_Down || ks == XK_Home  || ks == XK_End ||
                         ks == XK_Prior || ks == XK_Next);
        int escreve = (ks == XK_BackSpace || ks == XK_Delete ||
                       ks == XK_Return || ks == XK_KP_Enter || ks == XK_Tab ||
                       (n > 0 && (unsigned char) buf[0] >= 32));

        if (shift && movimento) {
            estendendo = 1;
            if (!sel_on) { sel_la = cur_l; sel_ca = cur_c; }
        } else if (sel_on && escreve) {
            talvez_guardar(G_NENHUM);
            apagar_selecao();
            grupo = G_DIGITANDO;
            if (ks == XK_BackSpace || ks == XK_Delete) {   /* apagar ja acabou */
                seguir_cursor();
                desenhar();
                return;
            }
        } else {
            sel_limpar();
        }
    }

    switch (ks) {
    case XK_Left: case XK_Right: case XK_Up: case XK_Down:
    case XK_Home: case XK_End: case XK_Prior: case XK_Next:
        grupo = G_NENHUM;                 /* mover o cursor fecha o passo */
        break;
    }

    switch (ks) {
    case XK_Left:  if (cur_c > 0) cur_c = car_anterior(linhas[cur_l], cur_c);
                   else if (cur_l > 0) { cur_l--; cur_c = (int) strlen(linhas[cur_l]); }
                   break;
    case XK_Right: if (cur_c < (int) strlen(linhas[cur_l])) cur_c = prox_car(linhas[cur_l], cur_c);
                   else if (cur_l < n_linhas - 1) { cur_l++; cur_c = 0; }
                   break;
    case XK_Up:
    case XK_Down: {
        int col = colunas_ate(linhas[cur_l], cur_c);
        if (ks == XK_Up   && cur_l > 0)            cur_l--;
        if (ks == XK_Down && cur_l < n_linhas - 1) cur_l++;
        cur_c = byte_da_coluna(linhas[cur_l], col);
        break;
    }
    case XK_Home:  cur_c = 0; break;
    case XK_End:   cur_c = (int) strlen(linhas[cur_l]); break;
    case XK_Prior:
    case XK_Next: {
        int vis = (cont_h() - 2 * MARGEM) / altura_lin;
        cur_l += (ks == XK_Next ? vis : -vis);
        if (cur_l < 0) cur_l = 0;
        if (cur_l >= n_linhas) cur_l = n_linhas - 1;
        cur_c = 0;
        break;
    }
    case XK_BackSpace: talvez_guardar(G_APAGANDO);  apagar_atras();  break;
    case XK_Delete:    talvez_guardar(G_APAGANDO);  apagar_frente(); break;
    case XK_Return:
    case XK_KP_Enter:  talvez_guardar(G_NENHUM); inserir_texto("\n", 1); break;
    case XK_Tab:       talvez_guardar(G_NENHUM); inserir_texto("    ", 4); break;
    case XK_Escape:    return;
    default:
        if (n > 0 && (unsigned char) buf[0] >= 32) {
            talvez_guardar(G_DIGITANDO);
            inserir_texto(buf, n);
        }
        break;
    }

    /* A ponta segue o cursor que a tecla acabou de mover. O PRIMARY se atualiza
     * a cada tecla de proposito: e o "marcou, ja esta la" do X - quem estende
     * com Shift e cola com o botao do meio nao aperta mais nada. */
    if (estendendo) {
        sel_lb = cur_l; sel_cb = cur_c;
        sel_on = !(sel_la == sel_lb && sel_ca == sel_cb);
        if (sel_on) virar_dono(XA_PRIMARY, sel_texto());
    }

    seguir_cursor();
    desenhar();
}

/* Ctrl+Tab tem de funcionar TAMBEM com o foco dentro do xterm, senao a aba do
 * Claude vira uma armadilha: entra-se nela pelo teclado e so se sai pelo mouse.
 * XGrabKey na janela-mae alcanca a subarvore inteira - o xterm e neto dela - e
 * com owner_events=False o evento vem para ca em vez de ir para o xterm.
 * As quatro combinacoes sao por causa do CapsLock e do NumLock, que entram no
 * "state" e fariam o grab nao casar. Grava-se so o Tab: cada tecla capturada
 * aqui e uma tecla a menos para o Claude Code, que usa o Shift+Tab. */
static void pegar_teclas(void)
{
    KeyCode k = XKeysymToKeycode(dpy, XK_Tab);
    unsigned trava[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    unsigned i;

    if (!k) return;
    for (i = 0; i < sizeof trava / sizeof trava[0]; i++) {
        XGrabKey(dpy, k, ControlMask | trava[i], win, False,
                 GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, k, ControlMask | ShiftMask | trava[i], win, False,
                 GrabModeAsync, GrabModeAsync);
    }
}

/* Com abas, XSetInputFocus passa a mirar janela que acabou de ser desmapeada -
 * o que devolve BadMatch, e o tratador padrao do Xlib MATA o processo. Erro de
 * foco e de janela ja morta se ignora; o resto continua aparecendo no stderr. */
static int erro_x(Display *d, XErrorEvent *e)
{
    char m[160];

    if (e->error_code == BadMatch || e->error_code == BadWindow) return 0;
    XGetErrorText(d, e->error_code, m, sizeof m);
    fprintf(stderr, "bancada: erro do X: %s\n", m);
    return 0;
}

int main(int argc, char **argv)
{
    XSetWindowAttributes at;
    XColor c;
    Colormap cm;
    XGlyphInfo gi;
    XRenderColor rc;
    char inicial[PATH_MAX];

    setlocale(LC_ALL, "");
    if (!XSupportsLocale()) fprintf(stderr, "bancada: locale sem suporte no X\n");
    XSetLocaleModifiers("");
    signal(SIGCHLD, SIG_IGN);

    dpy = XOpenDisplay(NULL);
    if (!dpy) morrer("nao abriu o display");
    XSetErrorHandler(erro_x);
    tela = DefaultScreen(dpy);
    raiz = RootWindow(dpy, tela);
    cm   = DefaultColormap(dpy, tela);

#define COR(s) (XParseColor(dpy, cm, s, &c), XAllocColor(dpy, cm, &c), c.pixel)
    FACE  = COR("#DEDEDE");
    FACE_ESC = COR("#C4C4C4");     /* face das abas de tras */
    HI    = COR("#FFFFFF");
    SH    = COR("#808080");
    INK   = COR("#000000");
    PAPEL = COR("#FFFFFF");
    SEL_FUNDO = COR("#000080");        /* o azul de selecao dos anos 90 */
#undef COR

    /* A pasta inicial: o argumento, ou de onde a bancada foi chamada. */
    if (argc > 1 && realpath(argv[1], inicial)) ;
    else if (!getcwd(inicial, sizeof inicial)) snprintf(inicial, sizeof inicial, "%s", ".");
    snprintf(projeto, sizeof projeto, "%s", inicial);

    at.background_pixel = FACE;
    at.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask |
                    ButtonPressMask | FocusChangeMask;
    win = XCreateWindow(dpy, raiz, 0, 0, (unsigned) larg, (unsigned) alt, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWBackPixel | CWEventMask, &at);
    XStoreName(dpy, win, "bancada");
    { XClassHint ch = { "bancada", "Bancada" }; XSetClassHint(dpy, win, &ch); }
    A_DELETE = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    /* PRIMARY nao precisa de XInternAtom: e um atomo fixo do protocolo
     * (XA_PRIMARY, do Xatom.h). Os outros tres sao convencao, nao protocolo. */
    A_AREA   = XInternAtom(dpy, "CLIPBOARD",   False);
    A_UTF8   = XInternAtom(dpy, "UTF8_STRING", False);
    A_ALVOS  = XInternAtom(dpy, "TARGETS",     False);
    A_TEXTO  = XInternAtom(dpy, "TEXT",        False);
    A_COLA   = XInternAtom(dpy, "BANCADA_COLA", False);
    XSetWMProtocols(dpy, win, &A_DELETE, 1);

    at.event_mask = ExposureMask | ButtonPressMask;
    w_arv = XCreateWindow(dpy, win, 0, BARRA_H, ARVORE_W, (unsigned) (alt - BARRA_H), 0,
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWBackPixel | CWEventMask, &at);
    /* SEM KeyPressMask aqui, e isso NAO e esquecimento. O X entrega a tecla a
     * MENOR janela sob o ponteiro que a tenha pedido, subindo dali - e nao
     * simplesmente a janela com foco. Com KeyPressMask no editor, as teclas
     * chegavam com window=w_ed enquanto o contexto de entrada (XIC) apontava
     * para a janela-mae; o XFilterEvent nao reconhecia o evento como dele e o
     * acento morto do ABNT2 nunca compunha: ' + a saia "a" em vez de "a" com
     * acento. Medido em 01/08/2026 contra o xterm, que no mesmo ambiente
     * compunha certo. Deixando so a mae pedir teclado, o evento sobe ate ela e
     * o XIM funciona. */
    /* ButtonRelease e Button1Motion sao a selecao com o mouse: o arraste so
     * existe se os tres eventos chegarem. Continua SEM KeyPressMask, pelo motivo
     * do paragrafo acima - o acento morto depende disso. */
    at.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                    Button1MotionMask;
    w_ed = XCreateWindow(dpy, win, CONT_X, CONT_Y,
                         (unsigned) (larg - ARVORE_W), (unsigned) (alt - BARRA_H - ABAS_H), 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWBackPixel | CWEventMask, &at);
    at.background_pixel = INK;
    /* SubstructureNotifyMask: o xterm nasce depois, de forma assincrona, e com
     * "-into" ele mantem a geometria de 80x24 com que veio ao mundo. E este
     * aviso que diz a hora certa de estica-lo para o painel inteiro. */
    /* SEM ExposureMask, desde que o convite ao clique saiu (02/08/2026): nao ha
     * mais nada nosso para desenhar aqui, e o preto do painel vazio vem do
     * background_pixel abaixo - o servidor X repinta a area exposta sozinho.
     * ButtonPressMask fica: o clique no painel vazio e uma das duas portas de
     * entrada do Claude. */
    at.event_mask = ButtonPressMask | SubstructureNotifyMask;
    w_term = XCreateWindow(dpy, win, CONT_X, CONT_Y,
                           (unsigned) (larg - ARVORE_W), (unsigned) (alt - BARRA_H - ABAS_H), 0,
                           CopyFromParent, InputOutput, CopyFromParent,
                           CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, win, 0, NULL);

    fonte = XftFontOpenName(dpy, tela, "DejaVu Sans Mono:size=10");
    if (!fonte) fonte = XftFontOpenName(dpy, tela, "monospace:size=10");
    if (!fonte) morrer("nao achei fonte monoespacada");

    XftTextExtentsUtf8(dpy, fonte, (const FcChar8 *) "M", 1, &gi);
    avanco     = gi.xOff ? gi.xOff : 8;
    altura_lin = fonte->ascent + fonte->descent + 2;
    base_lin   = fonte->ascent;

    dr_barra = XftDrawCreate(dpy, win,   DefaultVisual(dpy, tela), cm);
    dr_arv   = XftDrawCreate(dpy, w_arv, DefaultVisual(dpy, tela), cm);
    dr_ed    = XftDrawCreate(dpy, w_ed,  DefaultVisual(dpy, tela), cm);
    dr_term  = XftDrawCreate(dpy, w_term, DefaultVisual(dpy, tela), cm);

#define XCOR(dst, r, g, b) do { rc.red=(r); rc.green=(g); rc.blue=(b); rc.alpha=0xffff; \
        XftColorAllocValue(dpy, DefaultVisual(dpy, tela), cm, &rc, (dst)); } while (0)
    XCOR(&c_ink,    0x0000, 0x0000, 0x0000);
    XCOR(&c_fraco,  0x5000, 0x5000, 0x5000);
    XCOR(&c_sel,    0xffff, 0xffff, 0xffff);
    XCOR(&c_aberto, 0x1000, 0x3000, 0x9000);   /* na arvore: aberto em outra aba */
#undef XCOR

    xim = XOpenIM(dpy, NULL, NULL, NULL);
    if (xim)
        xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, win, XNFocusWindow, win, (char *) NULL);
    if (!xic)
        fprintf(stderr, "bancada: sem XIM; acento morto do ABNT2 nao vai compor\n");

    /* A aba 0 e a do Claude, e ela existe desde o inicio. Sem nenhum arquivo
     * aberto nao ha buffer nenhum: "carregada" fica em -1 e os globais do
     * editor ficam zerados ate a primeira aba de arquivo nascer. */
    abas[0].claude = 1;
    n_abas = 1;
    atual = 0;
    carregada = -1;
    globais_zerar();

    carregar_pasta(projeto);

    /* Nada de XMapSubwindows: quem decide qual dos dois paineis fica visivel e
     * o mostrar_painel(), e ele precisa da janela-mae ja mapeada para o
     * XSetInputFocus valer. */
    XMapWindow(dpy, w_arv);
    XMapWindow(dpy, win);
    XSync(dpy, False);
    pegar_teclas();
    /* SEM abrir_claude() aqui, e isso NAO e esquecimento. A bancada nao levanta
     * o Claude por conta propria: a aba dele sobe com o painel vazio, esperando
     * o clique. Ela serve de arvore e de editor sem custar os 490 MB do Claude
     * Code para quem abriu so para olhar um arquivo. */
    mostrar_painel();
    /* Depois de mapear: restaurar abre abas, e abrir aba mapeia e move o foco. */
    restaurar_sessao();

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (XFilterEvent(&ev, None)) continue;     /* o XIM come as teclas mortas */

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0) desenhar();
            break;
        case ConfigureNotify:
            if (ev.xconfigure.window == win &&
                (ev.xconfigure.width != larg || ev.xconfigure.height != alt)) {
                larg = ev.xconfigure.width;
                alt  = ev.xconfigure.height;
                redimensionar();
                desenhar();
            }
            break;
        case MapNotify:
            esticar_terminal();
            /* O xterm nasce assincrono: quando o clique o pediu, ele ainda nao
             * existia para receber o foco. Este e o momento em que ele existe. */
            if (abas[atual].claude) focar_terminal();
            break;
        case KeyPress:
            tecla(&ev.xkey);
            break;
        /* O arraste. Redesenhar a cada pixel de movimento seria caro numa sessao
         * remota - cada quadro e comprimido em CPU e vai por TCP -, entao so se
         * redesenha quando a PONTA muda de lugar de verdade. */
        case MotionNotify:
            if (arrastando && ev.xmotion.window == w_ed &&
                carregada >= 0 && modo == MODO_TEXTO && n_linhas > 0) {
                int l, c, antes_l = sel_lb, antes_c = sel_cb, antes_topo = topo;

                /* arrastar para fora da janela rola, como em qualquer editor */
                if (ev.xmotion.y < MARGEM && topo > 0) topo--;
                else if (ev.xmotion.y > cont_h() - MARGEM && topo < n_linhas - 1) topo++;

                pos_do_clique(ev.xmotion.x, ev.xmotion.y, &l, &c);
                sel_lb = l; sel_cb = c;
                cur_l  = l; cur_c  = c;
                sel_on = !(sel_la == sel_lb && sel_ca == sel_cb);
                if (l != antes_l || c != antes_c || topo != antes_topo) desenhar();
            }
            break;
        case ButtonRelease:
            if (arrastando && ev.xbutton.window == w_ed) {
                arrastando = 0;
                /* PRIMARY e "marcou, ja esta la": quem quiser colar com o botao
                 * do meio nao aperta mais nada. O CLIPBOARD e do Ctrl+C. */
                if (sel_on) virar_dono(XA_PRIMARY, sel_texto());
            }
            break;
        /* Alguem esta colando o que copiamos daqui. */
        case SelectionRequest:
            responder_selecao(&ev.xselectionrequest);
            break;
        /* A outra ponta do colar: o dono respondeu ao nosso pedido. */
        case SelectionNotify:
            if (ev.xselection.property == None) {
                /* Ninguem tem o CLIPBOARD - tenta o PRIMARY, que e onde mora a
                 * selecao de um xterm ou de um arraste em outro programa. */
                if (ev.xselection.selection == A_AREA) pedir_cola(XA_PRIMARY);
            } else {
                Atom tipo; int fmt; unsigned long n_itens, resto;
                unsigned char *dados = NULL;
                if (XGetWindowProperty(dpy, win, A_COLA, 0, 1 << 20, True,
                                       AnyPropertyType, &tipo, &fmt, &n_itens,
                                       &resto, &dados) == Success && dados) {
                    /* fmt != 8 seria uma resposta que nao e texto (lista de
                     * atomos, por exemplo); nesse caso nao ha o que colar. */
                    if (fmt == 8) receber_cola(dados, (size_t) n_itens);
                    XFree(dados);
                    desenhar();
                }
            }
            break;
        /* Outro programa virou dono. Perder o PRIMARY apaga o realce, que e o
         * que todo mundo faz: duas selecoes visiveis ao mesmo tempo, em janelas
         * diferentes, mentiriam sobre qual delas o botao do meio vai colar. */
        case SelectionClear:
            if (ev.xselectionclear.selection == A_AREA) {
                free(txt_cli); txt_cli = NULL;
            } else {
                free(txt_pri); txt_pri = NULL;
                if (sel_on) { sel_limpar(); desenhar(); }
            }
            break;
        case FocusIn:
            if (xic) XSetICFocus(xic);
            break;
        case FocusOut:
            if (xic) XUnsetICFocus(xic);
            break;
        case ButtonPress:
            if (ev.xbutton.window == w_arv) {
                if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5) {
                    arv_topo += (ev.xbutton.button == Button5 ? 3 : -3);
                    if (arv_topo < 0) arv_topo = 0;
                    if (arv_topo > n_entradas - 1) arv_topo = n_entradas - 1;
                } else {
                    clique_arvore(ev.xbutton.y);
                }
                desenhar();
            } else if (ev.xbutton.window == w_ed && carregada >= 0 &&
                       modo != MODO_TEXTO) {
                /* Leitura: a roda rola o hex, o resto do painel so devolve o
                 * foco. Nao ha cursor para posicionar, e nem "linhas" para o
                 * clique cair dentro. */
                if (modo == MODO_HEX &&
                    (ev.xbutton.button == Button4 || ev.xbutton.button == Button5)) {
                    int total = linhas_hex();
                    topo += (ev.xbutton.button == Button5 ? 3 : -3);
                    if (topo > total - 1) topo = total - 1;
                    if (topo < 0) topo = 0;
                } else if (ev.xbutton.button == Button1) {
                    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                }
                desenhar();
            } else if (ev.xbutton.window == w_ed && carregada >= 0) {
                if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5) {
                    topo += (ev.xbutton.button == Button5 ? 3 : -3);
                    if (topo < 0) topo = 0;
                    if (topo > n_linhas - 1) topo = n_linhas - 1;
                } else if (ev.xbutton.button == Button1) {
                    int l, c;
                    pos_do_clique(ev.xbutton.x, ev.xbutton.y, &l, &c);
                    cur_l = l; cur_c = c;

                    /* Um, dois ou tres cliques. O tempo vem do proprio evento,
                     * que e o relogio do servidor - nada de gettimeofday aqui,
                     * que mediria outra coisa. 400 ms e o intervalo classico. */
                    if (ev.xbutton.time - t_clique < 400 && l == l_clique) n_cliques++;
                    else                                                   n_cliques = 1;
                    t_clique = ev.xbutton.time;
                    l_clique = l;

                    if (n_cliques == 2)      { sel_palavra(l, c); arrastando = 0; }
                    else if (n_cliques >= 3) { sel_linha(l);      arrastando = 0; }
                    else {
                        sel_la = sel_lb = l;      /* ancora aqui; a ponta segue */
                        sel_ca = sel_cb = c;      /* o mouse no MotionNotify */
                        sel_on = 0;
                        arrastando = 1;
                    }
                    if (sel_on) virar_dono(XA_PRIMARY, sel_texto());
                    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                } else {
                    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                }
                desenhar();
            } else if (ev.xbutton.window == w_term) {
                /* Chega clique aqui de duas maneiras: no painel vazio, antes de
                 * o Claude existir - e ai o clique E o pedido de abrir; ou na
                 * borda de 2px que o xterm deixa de fora, onde so cabe devolver
                 * o foco a ele. */
                if (!claude_vivo()) { abrir_claude(); desenhar(); }
                else                focar_terminal();
            } else if (ev.xbutton.window == win && ev.xbutton.y < BARRA_H) {
                int i;
                for (i = 0; i < N_BOTOES; i++)
                    if (ev.xbutton.x >= botoes[i].x &&
                        ev.xbutton.x < botoes[i].x + botoes[i].w) {
                        if (i == 0) escolher_projeto();
                        else if (i == 1) salvar();
                        else ir_para_claude();
                        desenhar();
                        break;
                    }
            } else if (ev.xbutton.window == win &&
                       ev.xbutton.y < BARRA_H + ABAS_H && ev.xbutton.x >= ARVORE_W) {
                clique_abas(ev.xbutton.x);
                desenhar();
            }
            break;
        case ClientMessage:
            if ((Atom) ev.xclient.data.l[0] == A_DELETE) {
                if (!algum_sujo() || perguntar("Ha alteracoes nao salvas. Sair mesmo assim?")) {
                    salvar_sessao();
                    if (pid_term > 0) kill(pid_term, SIGTERM);
                    return 0;
                }
            }
            break;
        }
    }
    return 0;
}
