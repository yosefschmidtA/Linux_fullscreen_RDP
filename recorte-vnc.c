/*
 * recorte-vnc.c — mostra UM retangulo de um servidor VNC numa janela normal.
 *
 * Existe por causa de uma medicao (03/08/2026): o UltraVNC do Windows serve a
 * area de trabalho INTEIRA, 6400x1080, e nao o monitor virtual sozinho. A
 * opcao "Default screen: Secondary" da propria GUI dele nao recortou nada. A
 * selecao de monitor do UltraVNC e extensao proprietaria do viewer dele, e o
 * TigerVNC nao a fala.
 *
 * Entao o recorte acontece aqui, no X, e nao no protocolo:
 *
 *     +-- a nossa janela, 1920x1080, gerenciada pelo xfwm4 --+
 *     |                                                     |
 *  [==|== a janela do vncviewer, 6400x1080, em x = -4480 ===]|
 *     |                                                     |
 *     +-----------------------------------------------------+
 *
 * O filho e maior que a mae e comeca fora dela, a esquerda. O X corta o que
 * passa da borda de graca -- e o que sobra visivel e exatamente o monitor
 * virtual. Clique e teclado chegam com as coordenadas certas porque quem os
 * traduz e o proprio viewer, que continua achando que tem 6400 px.
 *
 * O custo e o VNC ainda trafegar os 6400 px de largura. Na pratica so retangulo
 * sujo anda no fio, os outros dois monitores ficam parados, e o fio e a vNIC da
 * WSL. Se um dia isso doer, o caminho e trocar o transporte por Sunshine, que
 * escolhe display por nome sem discussao.
 *
 *   gcc -O2 -Wall -o recorte-vnc recorte-vnc.c -lX11
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* O retangulo do framebuffer remoto que queremos ver, e o framebuffer todo.
 * Os padroes sao a medicao de 03/08/2026: monitor virtual do VDD em X=4480,
 * area de trabalho do Windows somando 6400x1080. */
static int rx = 4480, ry = 0, rw = 1920, rh = 1080;
static int fbw = 6400, fbh = 1080;

static char servidor[256] = "172.22.32.1:5900";
static char senha[512]    = "";          /* vazio => ~/.vnc/passwd */

static Display *dpy;
static Window   raiz;
static Window   win;                     /* a moldura: a nossa janela */
static Window   cliente = None;          /* a janela do vncviewer, adotada */
static pid_t    pid_viewer = 0;
static Atom     a_fechar, a_protos, a_lista, a_pid;

/* O Xlib mata o processo no erro de protocolo, e aqui erro de protocolo e
 * rotina: adotar uma janela corre contra o xfwm4 largando a moldura dela, e o
 * viewer pode morrer no meio de qualquer chamada nossa. Mesma razao da
 * bancada. */
static int engolir_erro(Display *d, XErrorEvent *e)
{
    (void) d; (void) e;
    return 0;
}

static void uso(const char *prog)
{
    fprintf(stderr,
        "uso: %s [-r X,Y,LARG,ALT] [-f LARGxALT] [-s host:porta] [-p arquivo]\n"
        "  -r  retangulo a mostrar, no framebuffer remoto (padrao %d,%d,%d,%d)\n"
        "  -f  tamanho do framebuffer remoto inteiro      (padrao %dx%d)\n"
        "  -s  servidor VNC                               (padrao %s)\n"
        "  -p  arquivo de senha do vncviewer              (padrao ~/.vnc/passwd)\n",
        prog, rx, ry, rw, rh, fbw, fbh, servidor);
    exit(2);
}

static void argumentos(int argc, char **argv)
{
    int c;

    while ((c = getopt(argc, argv, "r:f:s:p:h")) != -1) {
        switch (c) {
        case 'r':
            if (sscanf(optarg, "%d,%d,%d,%d", &rx, &ry, &rw, &rh) != 4) uso(argv[0]);
            break;
        case 'f':
            if (sscanf(optarg, "%dx%d", &fbw, &fbh) != 2) uso(argv[0]);
            break;
        case 's': snprintf(servidor, sizeof servidor, "%s", optarg); break;
        case 'p': snprintf(senha, sizeof senha, "%s", optarg);       break;
        default:  uso(argv[0]);
        }
    }
    if (rw <= 0 || rh <= 0 || fbw <= 0 || fbh <= 0) uso(argv[0]);
}

/* ==========================================================================
 * O viewer
 * ========================================================================== */
/* O "-geometry" e o que faz o viewer nascer do tamanho do framebuffer inteiro:
 * assim ele nao poe barra de rolagem propria e desenha os 6400 px de uma vez.
 * O xfwm4 provavelmente vai encolher a janela na hora de mapear, porque ela
 * nao cabe na tela -- nao importa, o XResizeWindow de depois da adocao desfaz
 * isso, e o viewer se re-arruma no ConfigureNotify.
 *
 * "RemoteResize=0" nao e enfeite: sem ele o viewer manda o SERVIDOR mudar a
 * resolucao para caber na janela. Como a janela vai ser redimensionada por nos
 * o tempo todo, isso mexeria na area de trabalho real do Windows. */
static void subir_viewer(void)
{
    char geo[64], caminho[512];

    snprintf(geo, sizeof geo, "%dx%d+0+0", fbw, fbh);
    if (senha[0]) snprintf(caminho, sizeof caminho, "%s", senha);
    else          snprintf(caminho, sizeof caminho, "%s/.vnc/passwd", getenv("HOME") ? getenv("HOME") : "");

    pid_viewer = fork();
    if (pid_viewer == 0) {
        setsid();
        execlp("vncviewer", "vncviewer",
               "-passwd", caminho,
               "-geometry", geo,
               "-RemoteResize=0",
               "-Shared=1",
               "-AlertOnFatalError=0",
               "-ReconnectOnError=0",
               servidor, (char *) NULL);
        _exit(127);
    }
}

/* ==========================================================================
 * Achar a janela do viewer
 * ========================================================================== */
/* Nao da para varrer os filhos da raiz procurando: o xfwm4 ja reparentou o
 * viewer para dentro de uma moldura, entao o filho da raiz e a MOLDURA, nao o
 * cliente. O _NET_CLIENT_LIST entrega os clientes direto, que e o que
 * queremos adotar -- adotar a moldura traria a decoracao do xfwm4 junto. */
static int e_o_nosso(Window w)
{
    XClassHint ch;
    Atom tipo;
    int fmt;
    unsigned long n = 0, resto;
    unsigned char *dados = NULL;
    int achou = 0;

    /* Preferimos casar por PID: se houver outro vncviewer aberto na sessao,
     * casar so pela classe adotaria a janela do usuario. */
    if (XGetWindowProperty(dpy, w, a_pid, 0, 1, False, XA_CARDINAL,
                           &tipo, &fmt, &n, &resto, &dados) == Success && dados) {
        pid_t p = (pid_t) *(unsigned long *) dados;
        XFree(dados);
        if (n == 1) return p == pid_viewer;
        /* n == 0: sem _NET_WM_PID, cai na classe abaixo */
    }

    if (XGetClassHint(dpy, w, &ch)) {
        if ((ch.res_name  && strcasecmp(ch.res_name,  "vncviewer") == 0) ||
            (ch.res_class && strcasecmp(ch.res_class, "vncviewer") == 0))
            achou = 1;
        if (ch.res_name)  XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
    }
    return achou;
}

static Window achar_cliente(void)
{
    Atom tipo;
    int fmt;
    unsigned long n = 0, resto, i;
    unsigned char *dados = NULL;
    Window achada = None;

    if (XGetWindowProperty(dpy, raiz, a_lista, 0, 1024, False, XA_WINDOW,
                           &tipo, &fmt, &n, &resto, &dados) != Success || !dados)
        return None;

    for (i = 0; i < n && achada == None; i++) {
        Window w = ((Window *) dados)[i];
        if (w != win && e_o_nosso(w)) achada = w;
    }
    XFree(dados);
    return achada;
}

/* Espera a janela aparecer. Enquanto ela nao vem, o viewer pode ter morrido --
 * senha errada, servidor fora do ar -- e ai nao adianta esperar os 20 s. */
static Window esperar_cliente(void)
{
    struct timespec pausa = { 0, 100 * 1000 * 1000 };   /* 100 ms */
    int tentativas;

    for (tentativas = 0; tentativas < 200; tentativas++) {
        Window w = achar_cliente();
        if (w != None) return w;
        if (pid_viewer > 0 && waitpid(pid_viewer, NULL, WNOHANG) == pid_viewer) {
            fprintf(stderr, "recorte-vnc: o vncviewer saiu antes de abrir janela.\n"
                            "             senha errada, ou servidor fora do ar?\n");
            return None;
        }
        nanosleep(&pausa, NULL);
    }
    fprintf(stderr, "recorte-vnc: nao achei a janela do vncviewer em 20 s.\n");
    return None;
}

/* ==========================================================================
 * A adocao
 * ========================================================================== */
/* Encaixa o filho: sempre do tamanho do framebuffer inteiro, sempre ancorado
 * de modo que o pixel (rx,ry) do remoto caia no canto (0,0) da nossa janela.
 * Chamado na adocao e a cada resize nosso -- o filho nao muda de tamanho com a
 * moldura, so a fatia visivel dele e que muda. */
static void encaixar(void)
{
    if (cliente == None) return;
    XMoveResizeWindow(dpy, cliente, -rx, -ry, (unsigned) fbw, (unsigned) fbh);
}

static void adotar(Window w)
{
    cliente = w;

    /* Tirar o cliente da moldura do xfwm4: o gerenciador so cuida de janelas
     * que descendem da raiz, entao reparentar para dentro da nossa ja o
     * desgarra. O xfwm4 recolhe a moldura vazia sozinho. */
    XReparentWindow(dpy, cliente, win, -rx, -ry);
    encaixar();
    XMapWindow(dpy, cliente);

    /* Para saber quando o viewer morrer: sem isto a janela sumiria e a moldura
     * ficaria de pe, vazia, sem ninguem entender por que. */
    XSelectInput(dpy, cliente, StructureNotifyMask);
    XSync(dpy, False);
}

/* Janela desgarrada nao recebe foco do gerenciador: quem tem de repassar somos
 * nos, quando a moldura ganha o foco. Mesmo arranjo do xterm da bancada. */
static void repassar_foco(void)
{
    if (cliente != None) XSetInputFocus(dpy, cliente, RevertToParent, CurrentTime);
}

/* ==========================================================================
 * A moldura
 * ========================================================================== */
static void criar_janela(void)
{
    XSizeHints dicas;
    int larg = rw, alt = rh;
    int tela_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int tela_h = DisplayHeight(dpy, DefaultScreen(dpy));
    XClassHint classe;

    /* Se o recorte nao couber na tela, a janela nasce do tamanho da tela e o
     * usuario ve um pedaco -- redimensionar mostra o resto. Nasce menor de
     * proposito: janela maior que a tela nasce com o canto de baixo perdido. */
    if (larg > tela_w) larg = tela_w;
    if (alt  > tela_h) alt  = tela_h;

    win = XCreateSimpleWindow(dpy, raiz, 0, 0, (unsigned) larg, (unsigned) alt,
                              0, BlackPixel(dpy, DefaultScreen(dpy)),
                              BlackPixel(dpy, DefaultScreen(dpy)));

    XStoreName(dpy, win, "Windows");
    classe.res_name  = (char *) "recorte-vnc";
    classe.res_class = (char *) "Recorte-vnc";
    XSetClassHint(dpy, win, &classe);

    /* Sem max: o recorte tem tamanho natural, mas esticar a janela so revela
     * mais do framebuffer, o que e util para depurar o alinhamento. */
    dicas.flags      = PMinSize | PBaseSize;
    dicas.min_width  = 160;
    dicas.min_height = 120;
    dicas.base_width = rw;
    dicas.base_height = rh;
    XSetWMNormalHints(dpy, win, &dicas);

    XSetWMProtocols(dpy, win, &a_fechar, 1);
    XSelectInput(dpy, win, StructureNotifyMask | FocusChangeMask |
                           SubstructureNotifyMask | ButtonPressMask);
    XMapWindow(dpy, win);
}

static void encerrar(int codigo)
{
    if (pid_viewer > 0) kill(-pid_viewer, SIGTERM);
    if (dpy) XCloseDisplay(dpy);
    exit(codigo);
}

int main(int argc, char **argv)
{
    XEvent ev;

    argumentos(argc, argv);

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "recorte-vnc: sem display\n"); return 1; }
    XSetErrorHandler(engolir_erro);

    raiz     = DefaultRootWindow(dpy);
    a_fechar = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    a_protos = XInternAtom(dpy, "WM_PROTOCOLS", False);
    a_lista  = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    a_pid    = XInternAtom(dpy, "_NET_WM_PID", False);
    (void) a_protos;

    criar_janela();
    subir_viewer();

    {
        Window w = esperar_cliente();
        if (w == None) encerrar(1);
        adotar(w);
        repassar_foco();
    }

    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case ConfigureNotify:
            /* So o resize da MOLDURA interessa. O do filho e eco do nosso
             * proprio XMoveResizeWindow -- reagir a ele daria laco. */
            if (ev.xconfigure.window == win) encaixar();
            break;

        case FocusIn:
            repassar_foco();
            break;

        case ButtonPress:
            /* Clique que pegou a moldura em vez do filho: devolve o foco.
             * Acontece na borda, quando o recorte e menor que a janela. */
            repassar_foco();
            break;

        case DestroyNotify:
            if (ev.xdestroywindow.window == cliente) {
                cliente = None;
                encerrar(0);
            }
            break;

        case UnmapNotify:
            /* O viewer desmapeia ao cair a conexao, antes de destruir. */
            if (ev.xunmap.window == cliente) encerrar(0);
            break;

        case ClientMessage:
            if ((Atom) ev.xclient.data.l[0] == a_fechar) encerrar(0);
            break;
        }
    }
}
