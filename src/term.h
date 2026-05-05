#ifndef TERM_H
#define TERM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <conio.h>
  #include <io.h>
  #include <direct.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <sys/select.h>
#endif

enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_ENTER, KEY_TAB, KEY_ESC, KEY_BACKSPACE, KEY_SPACE
};

#ifdef _WIN32
static DWORD g_orig_in_mode  = 0;
static DWORD g_orig_out_mode = 0;
static UINT  g_orig_out_cp   = 0;
static int   g_term_inited   = 0;
#else
static struct termios g_orig_termios;
static int   g_term_inited   = 0;
#endif

static void term_restore(void)
{
    if (!g_term_inited) return;
    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    fflush(stdout);
#ifdef _WIN32
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),  g_orig_in_mode);
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), g_orig_out_mode);
    if (g_orig_out_cp) SetConsoleOutputCP(g_orig_out_cp);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
#endif
    g_term_inited = 0;
}

static void term_init(void)
{
#ifdef _WIN32
    HANDLE hin  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hin,  &g_orig_in_mode);
    GetConsoleMode(hout, &g_orig_out_mode);
    g_orig_out_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    DWORD outmode = g_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hout, outmode);
    /* Leave input mode alone — we read with _getch(). */
#else
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); /* keep ISIG so Ctrl+C still works */
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
    g_term_inited = 1;
    atexit(term_restore);
    fputs("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H", stdout);
    fflush(stdout);
}

static void term_size(int *w, int *h)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        *w = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        *h = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
        return;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *w = ws.ws_col;
        *h = ws.ws_row;
        return;
    }
#endif
    *w = 80; *h = 24;
}

static int term_read_key(int *out_ch)
{
    *out_ch = 0;
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        switch (c2) {
        case 72: return KEY_UP;
        case 80: return KEY_DOWN;
        case 75: return KEY_LEFT;
        case 77: return KEY_RIGHT;
        }
        return KEY_NONE;
    }
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == '\t')              return KEY_TAB;
    if (c == 27)                return KEY_ESC;
    if (c == 8 || c == 127)     return KEY_BACKSPACE;
    if (c == ' ')               return KEY_SPACE;
    if (c >= 32 && c < 127)     { *out_ch = c; return KEY_CHAR; }
    return KEY_NONE;
#else
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_NONE;
    if (c == 27) {
        struct timeval tv = {0, 50000};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return KEY_ESC;
        unsigned char s0, s1;
        if (read(STDIN_FILENO, &s0, 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &s1, 1) != 1) return KEY_ESC;
        if (s0 == '[') {
            switch (s1) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            }
        }
        return KEY_ESC;
    }
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == '\t')              return KEY_TAB;
    if (c == 8 || c == 127)     return KEY_BACKSPACE;
    if (c == ' ')               return KEY_SPACE;
    if (c >= 32 && c < 127)     { *out_ch = c; return KEY_CHAR; }
    return KEY_NONE;
#endif
}

/* Discard any keystrokes buffered while we were busy. */
static void term_flush_input(void)
{
#ifdef _WIN32
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
#else
    tcflush(STDIN_FILENO, TCIFLUSH);
#endif
}

static void term_get_cwd(char *out, size_t size)
{
#ifdef _WIN32
    if (!_getcwd(out, (int)size)) { strncpy(out, ".", size); out[size-1] = 0; }
#else
    if (!getcwd(out, size))       { strncpy(out, ".", size); out[size-1] = 0; }
#endif
}

#endif /* TERM_H */
