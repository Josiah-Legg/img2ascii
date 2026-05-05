#ifndef TUI_H
#define TUI_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include <sys/stat.h>

#include "term.h"
#include "args.h"
#include "ascii_art.h"
#include "png_render.h"

#ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#define TUI_PATH_LEN  512
#define TUI_CHARS_LEN 256
#define TUI_MAX_BATCH 256
#define TUI_DEFAULT_CHARS "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "

/* ------------------------------------------------------------------ */
/* Color palette (256-color)                                          */
/* ------------------------------------------------------------------ */

#define C_RESET     "\x1b[0m"
#define C_TITLE     "\x1b[1;38;5;81m"
#define C_SUBTITLE  "\x1b[38;5;245m"
#define C_BORDER    "\x1b[38;5;240m"
#define C_LABEL     "\x1b[1;38;5;252m"
#define C_VALUE     "\x1b[38;5;255m"
#define C_HINT      "\x1b[38;5;244m"
#define C_EDITING   "\x1b[38;5;220m"
#define C_CHECKED   "\x1b[38;5;120m"
#define C_FOCUS_BG  "\x1b[48;5;238m\x1b[38;5;231m"
#define C_BTN       "\x1b[38;5;252m"
#define C_BTN_FOCUS "\x1b[48;5;81m\x1b[38;5;232m\x1b[1m"
#define C_ERROR     "\x1b[38;5;203m"
#define C_SUCCESS   "\x1b[38;5;120m"

/* ------------------------------------------------------------------ */
/* Frame buffer — accumulate the whole frame, write once              */
/* ------------------------------------------------------------------ */

static char   g_fb[1u << 20]; /* 1 MiB */
static size_t g_fb_len = 0;

static void fb_reset(void) { g_fb_len = 0; }

static void fb_putc(char c)
{
    if (g_fb_len < sizeof g_fb) g_fb[g_fb_len++] = c;
}

static void fb_puts(const char *s)
{
    while (*s && g_fb_len < sizeof g_fb) g_fb[g_fb_len++] = *s++;
}

static void fb_printf(const char *fmt, ...)
{
    if (g_fb_len >= sizeof g_fb) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_fb + g_fb_len, sizeof g_fb - g_fb_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_fb_len += (size_t)n;
        if (g_fb_len > sizeof g_fb) g_fb_len = sizeof g_fb;
    }
}

static void fb_flush(void)
{
    fwrite(g_fb, 1, g_fb_len, stdout);
    fflush(stdout);
    g_fb_len = 0;
}

static void fb_move(int row, int col) { fb_printf("\x1b[%d;%dH", row, col); }
static void fb_clear(void)            { fb_puts("\x1b[2J\x1b[H"); }
static void fb_show_cursor(int on)    { fb_puts(on ? "\x1b[?25h" : "\x1b[?25l"); }

/* Repeat a UTF-8 string `n` times. */
static void fb_repeat(const char *s, int n)
{
    for (int i = 0; i < n; i++) fb_puts(s);
}

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char input_path [TUI_PATH_LEN];                    /* single-file mode */
    char batch_paths[TUI_MAX_BATCH][TUI_PATH_LEN];     /* batch mode */
    int  batch_count;
    char output_path[TUI_PATH_LEN];
    char width_str[16];
    char chars[TUI_CHARS_LEN];
    bool grayscale;
    bool reverse;
    bool print_to_console;
    bool debug;
    bool batch;
    int  preset;       /* 0 = Custom, 1 = Max, 2 = Half, 3 = Quarter */
    bool txt_out;
    bool png_out;
    bool png_hq;
    char status[256];
    int  status_kind; /* 0 = info, 1 = error, 2 = success */
} TuiState;

enum {
    F_INPUT = 0,
    F_INPUT_BROWSE,
    F_OUTPUT,
    F_OUTPUT_BROWSE,
    F_WIDTH,
    F_PRESET,
    F_CHARS,
    F_GRAYSCALE,
    F_REVERSE,
    F_PRINT,
    F_DEBUG,
    F_TXT,
    F_PNG,
    F_PNG_HQ,
    F_GENERATE,
    F_QUIT,
    N_FIELDS
};

/* ------------------------------------------------------------------ */
/* Form-element drawing                                               */
/* ------------------------------------------------------------------ */

/* Draws a labelled text field. Returns the cursor column to use if editing. */
static int draw_textfield(int row, int col, int width,
                          const char *label, const char *value,
                          int focused, int editing)
{
    fb_move(row, col);
    fb_puts(C_LABEL);
    fb_printf("%-7s ", label);
    fb_puts(C_RESET);

    const char *bar_color = editing ? C_EDITING : (focused ? C_TITLE : C_BORDER);

    fb_puts(bar_color);
    fb_puts("│ ");
    fb_puts(C_RESET);

    int innerw = width - 4;
    if (innerw < 1) innerw = 1;

    int vlen = (int)strlen(value);
    const char *vshow = value;
    int scrolled = 0;
    if (vlen > innerw) {
        vshow = value + (vlen - innerw);
        scrolled = vlen - innerw;
    }
    int shown = vlen - scrolled;

    if (focused && !editing) fb_puts(C_FOCUS_BG);
    else if (editing)        fb_puts(C_EDITING);
    else                     fb_puts(C_VALUE);

    fb_printf("%-*.*s", innerw, innerw, vshow);
    fb_puts(C_RESET);

    fb_puts(bar_color);
    fb_puts(" │");
    fb_puts(C_RESET);

    /* cursor column = col + label_w + " │ " + shown */
    return col + 7 + 1 + 2 + shown;
}

static void draw_button(int row, int col, const char *label, int focused)
{
    fb_move(row, col);
    fb_puts(focused ? C_BTN_FOCUS : C_BTN);
    fb_printf(" %s ", label);
    fb_puts(C_RESET);
}

static void draw_minibutton(int row, int col, const char *label, int focused)
{
    fb_move(row, col);
    if (focused) {
        fb_puts(C_BTN_FOCUS);
        fb_printf(" %s ", label);
    } else {
        fb_puts(C_HINT);
        fb_printf(" %s ", label);
    }
    fb_puts(C_RESET);
}

static void draw_check(int row, int col, const char *label, int checked, int focused, int disabled)
{
    fb_move(row, col);
    if (focused) fb_puts(C_FOCUS_BG);
    if (checked) {
        fb_puts(disabled ? C_HINT : C_CHECKED);
        fb_puts("[x]");
        fb_puts(C_RESET);
        if (focused) fb_puts(C_FOCUS_BG);
    } else {
        fb_puts(disabled ? C_HINT : C_BORDER);
        fb_puts("[ ]");
        fb_puts(C_RESET);
        if (focused) fb_puts(C_FOCUS_BG);
    }
    fb_puts(disabled ? C_HINT : C_LABEL);
    fb_printf(" %-10s", label);
    fb_puts(C_RESET);
}

static void draw_hrule(int row, int col, int width)
{
    fb_move(row, col);
    fb_puts(C_BORDER);
    fb_repeat("─", width);
    fb_puts(C_RESET);
}

/* ------------------------------------------------------------------ */
/* Form view                                                          */
/* ------------------------------------------------------------------ */

static void tui_draw_form(const TuiState *s, int focus, int editing)
{
    fb_reset();
    fb_show_cursor(0);
    fb_clear();

    int tw, th; term_size(&tw, &th);
    int rule_w = tw - 4;
    if (rule_w > 70) rule_w = 70;
    if (rule_w < 20) rule_w = 20;

    /* header */
    fb_move(2, 3);
    fb_puts(C_TITLE);   fb_puts("img2ascii");        fb_puts(C_RESET);
    fb_puts(C_SUBTITLE);fb_puts("  ·  Convert images to ASCII art"); fb_puts(C_RESET);
    draw_hrule(3, 3, rule_w);

    int field_w   = 50;
    int browse_x  = 3 + 7 + 1 + field_w + 3; /* label + sp + bar+val+bar + gap */

    int cur_col = 0;
    int cur_row = 0;

    int c;

    /* Input row: in batch mode the value field shows a count summary; otherwise the path. */
    char input_display[TUI_PATH_LEN];
    if (s->batch) {
        if (s->batch_count == 0)
            snprintf(input_display, sizeof input_display, "(no files selected)");
        else if (s->batch_count == 1)
            snprintf(input_display, sizeof input_display, "1 file: %.480s", s->batch_paths[0]);
        else
            snprintf(input_display, sizeof input_display, "%d files selected", s->batch_count);
    } else {
        strncpy(input_display, s->input_path, sizeof input_display - 1);
        input_display[sizeof input_display - 1] = 0;
    }

    c = draw_textfield(5,  3, field_w,
                       s->batch ? "Inputs" : "Input",
                       input_display,
                       focus == F_INPUT,
                       editing && focus == F_INPUT && !s->batch);
    if (editing && focus == F_INPUT && !s->batch) { cur_row = 5;  cur_col = c; }
    draw_minibutton(5, browse_x, "Pick...",
                    focus == F_INPUT_BROWSE);

    c = draw_textfield(7,  3, field_w, s->batch ? "Out dir" : "Output", s->output_path,
                       focus == F_OUTPUT, editing && focus == F_OUTPUT);
    if (editing && focus == F_OUTPUT) { cur_row = 7;  cur_col = c; }
    draw_minibutton(7, browse_x, "Browse...", focus == F_OUTPUT_BROWSE);

    /* Width field: dimmed unless preset == Custom. */
    static const char *preset_names[] = { "Custom", "Max-res", "Half-res", "Quarter-res" };
    static const char *preset_hint [] = {
        "blank → fit terminal width",
        "use full image native width",
        "use 1/2 of image native width",
        "use 1/4 of image native width",
    };
    int p = s->preset;
    if (p < 0 || p > 3) p = 0;

    if (p != 0) {
        fb_move(9, 3);
        fb_puts(C_LABEL);
        fb_printf("%-7s ", "Width");
        fb_puts(C_RESET);
        fb_puts(C_BORDER);
        fb_printf("│ %-*.*s │", 18 - 4, 18 - 4, "(preset)");
        fb_puts(C_RESET);
    } else {
        c = draw_textfield(9,  3, 18,      "Width",  s->width_str,
                           focus == F_WIDTH,  editing && focus == F_WIDTH);
        if (editing && focus == F_WIDTH)  { cur_row = 9;  cur_col = c; }
    }

    /* Preset cycle button right after the width field. */
    {
        char preset_label[40];
        snprintf(preset_label, sizeof preset_label, "[ Preset: %-11s ]", preset_names[p]);
        draw_button(9, 3 + 7 + 1 + 18 + 2, preset_label, focus == F_PRESET);
    }

    /* Hint to the right of the preset button. */
    fb_move(9, 3 + 7 + 1 + 18 + 2 + (int)strlen("[ Preset: Quarter-res ]") + 2);
    fb_puts(C_HINT);
    fb_puts(preset_hint[p]);
    fb_puts(C_RESET);

    c = draw_textfield(11, 3, field_w, "Chars",  s->chars,
                       focus == F_CHARS,  editing && focus == F_CHARS);
    if (editing && focus == F_CHARS)  { cur_row = 11; cur_col = c; }

    /* checkboxes */
    draw_check(14, 3,  "Grayscale", s->grayscale,        focus == F_GRAYSCALE, 0);
    draw_check(14, 21, "Reverse",   s->reverse,          focus == F_REVERSE,   0);
    draw_check(14, 39, "Print",     s->print_to_console, focus == F_PRINT,     0);
    draw_check(14, 55, "Debug",     s->debug,            focus == F_DEBUG,     0);
    draw_check(15, 3,  "TXT",       s->txt_out,          focus == F_TXT,       0);
    draw_check(15, 21, "PNG",       s->png_out,          focus == F_PNG,       0);
    draw_check(15, 39, "PNG HQ",    s->png_hq,           focus == F_PNG_HQ,    !s->png_out);

    draw_button(18, 3,  "[ Generate ]", focus == F_GENERATE);
    draw_button(18, 22, "[ Quit ]",     focus == F_QUIT);

    /* footer */
    draw_hrule(th - 3, 3, rule_w);
    fb_move(th - 2, 3);
    if (s->status[0]) {
        const char *col = (s->status_kind == 1) ? C_ERROR
                        : (s->status_kind == 2) ? C_SUCCESS
                        : C_HINT;
        fb_puts(col);
        fb_puts(s->status);
        fb_puts(C_RESET);
    } else {
        fb_puts(C_HINT);
        fb_puts("Tab/arrows: navigate  ·  Enter: edit/select  ·  Space: toggle  ·  Esc: quit");
        fb_puts(C_RESET);
    }

    if (editing && cur_row > 0) {
        fb_move(cur_row, cur_col);
        fb_show_cursor(1);
    }

    fb_flush();
}

/* ------------------------------------------------------------------ */
/* File browser                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[TUI_PATH_LEN];
    int  is_dir;   /* 0 = file, 1 = directory, 2 = "use this folder" sentinel */
} TuiEntry;

typedef enum { BROWSE_INPUT, BROWSE_OUTPUT } BrowseMode;

static int tui_is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int tui_entry_cmp(const void *a, const void *b)
{
    const TuiEntry *ea = (const TuiEntry*)a, *eb = (const TuiEntry*)b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcmp(ea->name, eb->name);
}

static int tui_has_image_ext(const char *name)
{
    static const char *exts[] = {
        ".png",".jpg",".jpeg",".bmp",".tga",".gif",".psd",
        ".hdr",".pic",".ppm",".pgm",NULL
    };
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    for (int i = 0; exts[i]; i++) {
        const char *a = dot, *b = exts[i];
        int ok = 1;
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
            if (ca != cb) { ok = 0; break; }
            a++; b++;
        }
        if (ok && *a == 0 && *b == 0) return 1;
    }
    return 0;
}

static int tui_browser_load(const char *dir, TuiEntry *out, int max, BrowseMode mode)
{
    int n = 0;
    if (mode == BROWSE_OUTPUT && n < max) {
        strncpy(out[n].name, "[ Use this folder (auto-name) ]", TUI_PATH_LEN - 1);
        out[n].name[TUI_PATH_LEN - 1] = '\0';
        out[n].is_dir = 2;
        n++;
    }

    if (n < max) {
        strncpy(out[n].name, "..", TUI_PATH_LEN - 1);
        out[n].name[TUI_PATH_LEN - 1] = '\0';
        out[n].is_dir = 1;
        n++;
    }

#ifdef _WIN32
    {
        char pattern[TUI_PATH_LEN * 2];
        snprintf(pattern, sizeof pattern, "%s\\*", dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            return n;
        }
        do {
            if (strcmp(fd.cFileName, ".") == 0) continue;
            if (strcmp(fd.cFileName, "..") == 0) continue;
            if (fd.cFileName[0] == '.') continue;
            if (n >= max) break;
            strncpy(out[n].name, fd.cFileName, TUI_PATH_LEN - 1);
            out[n].name[TUI_PATH_LEN - 1] = '\0';
            out[n].is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
            n++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        if (!d) return n;
        struct dirent *de;
        while ((de = readdir(d)) != NULL && n < max) {
            if (strcmp(de->d_name, ".")  == 0) continue;
            if (strcmp(de->d_name, "..") == 0) continue;
            if (de->d_name[0] == '.')          continue; /* skip hidden */
            char full[TUI_PATH_LEN * 2];
            snprintf(full, sizeof full, "%s/%s", dir, de->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            strncpy(out[n].name, de->d_name, TUI_PATH_LEN - 1);
            out[n].name[TUI_PATH_LEN - 1] = '\0';
            out[n].is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            n++;
        }
        closedir(d);
    }
#endif
    /* Don't sort sentinel/".." prefix. */
    int prefix = (mode == BROWSE_OUTPUT) ? 2 : 1;
    if (n > prefix) qsort(out + prefix, n - prefix, sizeof(TuiEntry), tui_entry_cmp);
    return n;
}

static void tui_path_up(char *path)
{
    size_t len = strlen(path);
    if (len == 0) { strcpy(path, "."); return; }
    char *fwd  = strrchr(path, '/');
    char *back = strrchr(path, '\\');
    char *slash = (back > fwd) ? back : fwd;
    if (!slash) { strcpy(path, "."); return; }
    if (slash == path) { path[1] = '\0'; return; }
    if (slash - path == 2 && path[1] == ':') { slash[1] = '\0'; return; }
    *slash = '\0';
}

static void tui_basename_no_ext(const char *path, char *out, size_t cap)
{
    const char *fwd  = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *base = (back > fwd) ? back + 1 : (fwd ? fwd + 1 : path);
    strncpy(out, base, cap - 1);
    out[cap - 1] = 0;
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

/*
 * File browser.
 *  - Enter on a directory: descend (or select folder sentinel in output mode)
 *  - Enter on a file: returns file path (input mode only)
 */
static int tui_file_browser(BrowseMode mode,
                            const char *initial_filename,
                            char *cwd,
                            char *out_path, int out_size)
{
    static TuiEntry entries[2048];
    int n = tui_browser_load(cwd, entries, 2048, mode);
    int sel = 0, scroll = 0;

    (void)initial_filename; /* output picker is directory-only */

    while (1) {
        fb_reset();
        fb_show_cursor(0);
        fb_clear();

        int tw, th; term_size(&tw, &th);
        int rule_w = tw - 4; if (rule_w > 90) rule_w = 90; if (rule_w < 20) rule_w = 20;

        fb_move(2, 3);
        fb_puts(C_TITLE);
        fb_puts(mode == BROWSE_INPUT ? "Pick an image" : "Choose output location");
        fb_puts(C_RESET);

        fb_move(3, 3);
        fb_puts(C_HINT);
        int max_path_show = rule_w;
        int cl = (int)strlen(cwd);
        if (cl > max_path_show && max_path_show > 4)
            fb_printf("...%s", cwd + cl - max_path_show + 3);
        else
            fb_puts(cwd);
        fb_puts(C_RESET);

        draw_hrule(4, 3, rule_w);

        int viewh = th - 9; if (viewh < 1) viewh = 1;
        if (viewh < 1) viewh = 1;
        if (sel < scroll)             scroll = sel;
        if (sel >= scroll + viewh)    scroll = sel - viewh + 1;

        for (int i = 0; i < viewh && (scroll + i) < n; i++) {
            int idx = scroll + i;
            TuiEntry *e = &entries[idx];
            int active_list = (idx == sel);
            fb_move(5 + i, 3);
            if (active_list) fb_puts(C_FOCUS_BG);
            if (e->is_dir == 2) {
                if (!active_list) fb_puts(C_SUCCESS);
                fb_printf(" %-58s", e->name);
                if (!active_list) fb_puts(C_RESET);
            } else if (e->is_dir) {
                if (!active_list) fb_puts(C_TITLE);
                fb_printf(" %-50s", e->name[0] ? e->name : "/");
                if (!active_list) fb_puts(C_RESET);
                if (active_list) fb_puts(C_FOCUS_BG);
                fb_puts(C_HINT);
                fb_puts("  <dir>");
                fb_puts(C_RESET);
                if (active_list) fb_puts(C_FOCUS_BG);
            } else {
                int img = tui_has_image_ext(e->name);
                if (mode == BROWSE_OUTPUT) {
                    if (!active_list) fb_puts(C_HINT);
                    fb_printf("   %-50s", e->name);
                    if (!active_list) fb_puts(C_RESET);
                    if (active_list) fb_puts(C_FOCUS_BG);
                    fb_puts(C_HINT);
                    fb_puts("  <file>");
                    fb_puts(C_RESET);
                    if (active_list) fb_puts(C_FOCUS_BG);
                } else {
                    if (!active_list) fb_puts(img ? C_VALUE : C_HINT);
                    fb_printf("   %-50s", e->name);
                    if (!active_list) fb_puts(C_RESET);
                }
            }
            if (active_list) fb_puts(C_RESET);
        }

        draw_hrule(th - 2, 3, rule_w);
        fb_move(th - 1, 3);
        fb_puts(C_HINT);
        if (mode == BROWSE_INPUT)
            fb_puts("↑↓: move  ·  Enter: open/select  ·  Backspace: up  ·  Esc: cancel");
        else
            fb_puts("↑↓: move  ·  Enter: open/select folder  ·  Backspace: up  ·  Esc: cancel");
        fb_puts(C_RESET);
        fb_flush();

        int ch;
        int k = term_read_key(&ch);

        if (k == KEY_ESC) return 0;
        if (k == KEY_UP   && sel > 0)     sel--;
        if (k == KEY_DOWN && sel < n - 1) sel++;
        if (k == KEY_BACKSPACE) {
            tui_path_up(cwd);
            n = tui_browser_load(cwd, entries, 2048, mode);
            sel = 0; scroll = 0;
        }
        if (k == KEY_ENTER && n > 0) {
            TuiEntry *e = &entries[sel];
            if (e->is_dir == 2) {
                /* "Use this folder" sentinel — return cwd; generate will auto-name. */
                strncpy(out_path, cwd, out_size - 1);
                out_path[out_size - 1] = '\0';
                return 1;
            } else if (e->is_dir) {
                if (strcmp(e->name, "..") == 0) {
                    tui_path_up(cwd);
                } else {
                    char tmp[TUI_PATH_LEN * 2];
                    snprintf(tmp, sizeof tmp, "%s/%s", cwd, e->name);
                    strncpy(cwd, tmp, TUI_PATH_LEN - 1);
                    cwd[TUI_PATH_LEN - 1] = '\0';
                }
                n = tui_browser_load(cwd, entries, 2048, mode);
                sel = 0; scroll = 0;
            } else if (mode == BROWSE_INPUT) {
                snprintf(out_path, out_size, "%s/%s", cwd, e->name);
                return 1;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Multi-select file browser (for Batch mode)                         */
/* ------------------------------------------------------------------ */

/* Returns the number of files selected (0 = cancelled / nothing picked).
 * Output paths are written to `out_paths[i]` (each TUI_PATH_LEN bytes). */
static int tui_multi_browser(char *cwd,
                             char (*out_paths)[TUI_PATH_LEN], int out_cap)
{
    static TuiEntry entries[2048];
    static int      selected[2048]; /* parallel to entries; 1 = ticked */
    int n = tui_browser_load(cwd, entries, 2048, BROWSE_INPUT);
    for (int i = 0; i < n; i++) selected[i] = 0;

    /* Picked paths persist across directory changes. */
    static char picked[TUI_MAX_BATCH][TUI_PATH_LEN];
    int picked_count = 0;
    int sel = 0, scroll = 0;

    while (1) {
        fb_reset();
        fb_show_cursor(0);
        fb_clear();

        int tw, th; term_size(&tw, &th);
        int rule_w = tw - 4; if (rule_w > 90) rule_w = 90; if (rule_w < 20) rule_w = 20;

        fb_move(2, 3);
        fb_puts(C_TITLE); fb_puts("Pick images (multi-select)"); fb_puts(C_RESET);

        fb_move(3, 3);
        fb_puts(C_HINT);
        int max_path_show = rule_w;
        int cl = (int)strlen(cwd);
        if (cl > max_path_show && max_path_show > 4)
            fb_printf("...%s", cwd + cl - max_path_show + 3);
        else
            fb_puts(cwd);
        fb_puts(C_RESET);

        draw_hrule(4, 3, rule_w);

        /* "Done" row at index -1 (drawn as the first list entry, sel == -1). */
        int viewh = th - 8; if (viewh < 1) viewh = 1;

        int total_rows = n + 1; /* +1 for Done row */
        if (sel < scroll)              scroll = sel;
        if (sel >= scroll + viewh)     scroll = sel - viewh + 1;
        if (scroll < 0)                scroll = 0;

        for (int i = 0; i < viewh && (scroll + i) < total_rows; i++) {
            int idx = scroll + i; /* 0 = Done row, 1..n = entries[idx-1] */
            int active = (idx == sel);
            fb_move(5 + i, 3);
            if (active) fb_puts(C_FOCUS_BG);

            if (idx == 0) {
                if (!active) fb_puts(C_SUCCESS);
                fb_printf(" [ Done — %d selected ]%*s", picked_count,
                          40, "");
                if (!active) fb_puts(C_RESET);
            } else {
                TuiEntry *e = &entries[idx - 1];
                if (e->is_dir) {
                    if (!active) fb_puts(C_TITLE);
                    fb_printf("     %-46s", e->name[0] ? e->name : "/");
                    if (!active) fb_puts(C_RESET);
                    if (active) fb_puts(C_FOCUS_BG);
                    fb_puts(C_HINT); fb_puts("  <dir>"); fb_puts(C_RESET);
                    if (active) fb_puts(C_FOCUS_BG);
                } else {
                    int img = tui_has_image_ext(e->name);
                    const char *mark = selected[idx - 1] ? "[x]" : "[ ]";
                    if (!active) fb_puts(selected[idx - 1] ? C_CHECKED
                                         : (img ? C_VALUE : C_HINT));
                    fb_printf(" %s %-46s", mark, e->name);
                    if (!active) fb_puts(C_RESET);
                }
            }
            if (active) fb_puts(C_RESET);
        }

        draw_hrule(th - 2, 3, rule_w);
        fb_move(th - 1, 3);
        fb_puts(C_HINT);
        fb_puts("Space: toggle  ·  Enter: open dir / Done  ·  Backspace: up  ·  Esc: cancel");
        fb_puts(C_RESET);

        fb_flush();

        int ch;
        int k = term_read_key(&ch);

        if (k == KEY_ESC) return 0;
        if (k == KEY_UP   && sel > 0)               sel--;
        if (k == KEY_DOWN && sel < total_rows - 1)  sel++;
        if (k == KEY_BACKSPACE) {
            tui_path_up(cwd);
            n = tui_browser_load(cwd, entries, 2048, BROWSE_INPUT);
            for (int i = 0; i < n; i++) selected[i] = 0;
            sel = 0; scroll = 0;
        }
        if (k == KEY_SPACE && sel > 0) {
            int idx = sel - 1;
            TuiEntry *e = &entries[idx];
            if (!e->is_dir) {
                if (selected[idx]) {
                    /* untick: also remove from picked list */
                    char full[TUI_PATH_LEN * 2];
                    snprintf(full, sizeof full, "%s/%s", cwd, e->name);
                    for (int i = 0; i < picked_count; i++) {
                        if (strcmp(picked[i], full) == 0) {
                            for (int j = i; j < picked_count - 1; j++)
                                strcpy(picked[j], picked[j + 1]);
                            picked_count--;
                            break;
                        }
                    }
                    selected[idx] = 0;
                } else if (picked_count < TUI_MAX_BATCH) {
                    int written = snprintf(picked[picked_count], TUI_PATH_LEN,
                                           "%s/%s", cwd, e->name);
                    if (written >= TUI_PATH_LEN) {
                        /* Path too long, skip this entry */
                        continue;
                    }
                    picked_count++;
                    selected[idx] = 1;
                }
            }
        }
        if (k == KEY_ENTER) {
            if (sel == 0) {
                /* Done */
                int n_out = picked_count < out_cap ? picked_count : out_cap;
                for (int i = 0; i < n_out; i++)
                    strncpy(out_paths[i], picked[i], TUI_PATH_LEN - 1);
                return n_out;
            }
            int idx = sel - 1;
            TuiEntry *e = &entries[idx];
            if (e->is_dir) {
                if (strcmp(e->name, "..") == 0) {
                    tui_path_up(cwd);
                } else {
                    char tmp[TUI_PATH_LEN * 2];
                    snprintf(tmp, sizeof tmp, "%s/%s", cwd, e->name);
                    strncpy(cwd, tmp, TUI_PATH_LEN - 1);
                    cwd[TUI_PATH_LEN - 1] = 0;
                }
                n = tui_browser_load(cwd, entries, 2048, BROWSE_INPUT);
                /* Re-mark entries that are in the picked list. */
                for (int i = 0; i < n; i++) {
                    selected[i] = 0;
                    if (entries[i].is_dir) continue;
                    char full[TUI_PATH_LEN * 2];
                    snprintf(full, sizeof full, "%s/%s", cwd, entries[i].name);
                    for (int j = 0; j < picked_count; j++) {
                        if (strcmp(picked[j], full) == 0) { selected[i] = 1; break; }
                    }
                }
                sel = 0; scroll = 0;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Preview — ANSI-aware line clipping so wide images don't wrap        */
/* ------------------------------------------------------------------ */

/* Append `output` to the frame buffer, but clip each visual line to
 * `max_w` visible columns and stop after `max_lines`. ANSI CSI sequences
 * are emitted verbatim and don't count toward the visual column. */
static void render_output_clipped(const char *output, int max_w, int max_lines)
{
    int line = 0, col = 0;
    const unsigned char *p = (const unsigned char *)output;
    while (*p && line < max_lines) {
        if (*p == 0x1b) {
            fb_putc((char)*p++);
            if (*p == '[') {
                fb_putc((char)*p++);
                while (*p && !(*p >= '@' && *p <= '~')) fb_putc((char)*p++);
                if (*p) fb_putc((char)*p++);
            }
            continue;
        }
        if (*p == '\n') {
            fb_putc('\n'); line++; col = 0; p++;
            continue;
        }
        if (col < max_w) {
            fb_putc((char)*p);
            col++;
        }
        p++;
    }
}

static void tui_preview(const TuiState *s, const char *output,
                        int width, int height,
                        const char *save_path, int saved_initially)
{
    (void)s;
    int saved = saved_initially;
    while (1) {
        int tw, th; term_size(&tw, &th);
        fb_reset();
        fb_show_cursor(0);
        fb_clear();

        render_output_clipped(output, tw, th - 2);
        fb_puts(C_RESET);

        fb_move(th - 1, 2);
        fb_puts(C_SUBTITLE);
        if (save_path && save_path[0]) {
            if (saved)
                fb_printf("Preview · %dx%d · saved → %.180s",
                          width, height, save_path);
            else
                fb_printf("Preview · %dx%d · press 's' to save → %.180s",
                          width, height, save_path);
        } else {
            fb_printf("Preview · %dx%d · no output path set", width, height);
        }
        fb_puts(C_RESET);

        fb_move(th, 2);
        fb_puts(C_HINT);
        fb_puts("s: save  ·  q/Esc: back to form");
        fb_puts(C_RESET);

        fb_flush();

        int ch;
        int k = term_read_key(&ch);
        if (k == KEY_ESC) return;
        if (k == KEY_CHAR) {
            if (ch == 'q' || ch == 'Q') return;
            if ((ch == 's' || ch == 'S') && save_path && save_path[0]) {
                FILE *f = fopen(save_path, "w");
                if (f) { fputs(output, f); fclose(f); saved = 1; }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Generate                                                           */
/* ------------------------------------------------------------------ */

static int tui_save_output(const char *path, const char *output)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fputs(output, f);
    fclose(f);
    return 1;
}

static void set_status(TuiState *s, int kind, const char *msg)
{
    s->status_kind = kind;
    strncpy(s->status, msg, sizeof s->status - 1);
    s->status[sizeof s->status - 1] = 0;
}

/* If output_path points at a directory (or is empty), build "<dir>/<input-stem>_ascii.txt".
 * Otherwise, return output_path unchanged. */
static void resolve_output_path(const char *output_path, const char *input_path,
                                char *out, size_t cap)
{
    if (!output_path || !output_path[0]) { out[0] = 0; return; }
    if (tui_is_directory(output_path)) {
        char stem[TUI_PATH_LEN];
        tui_basename_no_ext(input_path, stem, sizeof stem);
        snprintf(out, cap, "%.300s/%.180s_ascii.txt",
                 output_path, stem[0] ? stem : "output");
    } else {
        strncpy(out, output_path, cap - 1);
        out[cap - 1] = 0;
    }
}

/* Build "<dir>/<input-stem>_ascii.png" for PNG output. */
static void resolve_png_output_path(const char *output_path, const char *input_path,
                                    char *out, size_t cap)
{
    if (!output_path || !output_path[0] || !tui_is_directory(output_path)) {
        out[0] = 0;
        return;
    }
    char stem[TUI_PATH_LEN];
    tui_basename_no_ext(input_path, stem, sizeof stem);
    snprintf(out, cap, "%.300s/%.180s_ascii.png",
             output_path, stem[0] ? stem : "output");
}

/* Aspect-corrected resize: terminal cells are ~2x taller than wide, so
 * scale the height down so the rendered image looks correct on screen. */
#define TUI_CHAR_ASPECT 0.5

static uint8_t *tui_load_corrected(const char *path, int desired_w,
                                   int *out_w, int *out_h)
{
    int img_w = 0, img_h = 0;
    uint8_t *raw = stbi_load(path, &img_w, &img_h, NULL, STBI_rgb);
    if (!raw) return NULL;

    /* desired_w == 0 means "use the image's native width" (max-res). */
    if (desired_w <= 0)       desired_w = img_w;
    if (desired_w > img_w)    desired_w = img_w;

    int desired_h = (int)(((double)img_h / (double)img_w)
                          * (double)desired_w * TUI_CHAR_ASPECT + 0.5);
    if (desired_h < 1)        desired_h = 1;
    if (desired_h > img_h)    desired_h = img_h;

    if (desired_w == img_w && desired_h == img_h) {
        *out_w = img_w; *out_h = img_h;
        return raw;
    }

    uint8_t *resized = (uint8_t *)malloc((size_t)desired_w * desired_h * 3);
    if (!resized) { stbi_image_free(raw); return NULL; }
    stbir_resize_uint8(raw,     img_w,     img_h,     img_w     * 3,
                       resized, desired_w, desired_h, desired_w * 3, 3);
    stbi_image_free(raw);
    *out_w = desired_w; *out_h = desired_h;
    return resized;
}

/* Build an ASCII output for one input. Returns malloc'd string or NULL.
 * On success, out_w and out_h receive the rendered dimensions. */
/* Renders one input. Returns the malloc'd output text, or NULL on error.
 * If out_image is non-NULL, the resized RGB image buffer is also returned
 * (caller must free); otherwise it's freed internally.
 * preset: 0 = Custom (uses requested_width + terminal clamp),
 *         1 = Max, 2 = Half, 3 = Quarter (of native width). */
static char *tui_render_one(const char *input_path, const char *chars_src,
                            uint8_t flags, int requested_width,
                            int preset, int *out_w, int *out_h,
                            uint8_t **out_image)
{
    int img_w = 0, img_h = 0, channels;
    if (!stbi_info(input_path, &img_w, &img_h, &channels)) return NULL;

    int width;
    if (preset == 1) {
        width = img_w;
    } else if (preset == 2) {
        width = img_w / 2; if (width < 1) width = 1;
    } else if (preset == 3) {
        width = img_w / 4; if (width < 1) width = 1;
    } else {
        int tw, th; term_size(&tw, &th); (void)th;
        int max_w = tw - 1; if (max_w < 10) max_w = 10;
        width = requested_width;
        if (width <= 0)    width = (img_w < max_w) ? img_w : max_w;
        if (width > img_w) width = img_w;
        if (width > max_w) width = max_w;
        if (width < 1)     width = 1;
    }

    int height = 0;
    uint8_t *image = tui_load_corrected(input_path, width, &width, &height);
    if (!image) return NULL;

    char *characters = strdup(chars_src);
    char *output = (flags & GRAYSCALE_FLAG)
        ? get_output_grayscale(image, width, height, characters, flags)
        : get_output_rgb     (image, width, height, characters, flags);

    if (out_image) *out_image = image;
    else           free(image);
    free(characters);
    *out_w = width; *out_h = height;
    return output;
}

/* Centered progress bar overlay. Pass total <= 0 to show an indeterminate
 * label only. Always re-flushes the frame buffer immediately. */
static void tui_draw_progress(const char *title, const char *detail,
                              int done, int total)
{
    fb_reset();
    fb_show_cursor(0);
    fb_clear();

    int tw, th; term_size(&tw, &th);
    int row = th / 2 - 1;
    int bar_w = tw - 14; if (bar_w > 60) bar_w = 60; if (bar_w < 10) bar_w = 10;

    int title_len = (int)strlen(title);
    fb_move(row, (tw - title_len) / 2 + 1);
    fb_puts(C_TITLE); fb_puts(title); fb_puts(C_RESET);

    if (detail && detail[0]) {
        int dlen = (int)strlen(detail);
        if (dlen > tw - 4) dlen = tw - 4;
        fb_move(row + 1, (tw - dlen) / 2 + 1);
        fb_puts(C_HINT);
        fb_printf("%.*s", dlen, detail);
        fb_puts(C_RESET);
    }

    if (total > 0) {
        if (done < 0) done = 0;
        if (done > total) done = total;
        int filled = (int)((long)done * bar_w / total);
        int bar_col = (tw - bar_w - 2) / 2 + 1;
        fb_move(row + 3, bar_col);
        fb_puts(C_BORDER); fb_puts("["); fb_puts(C_RESET);
        fb_puts(C_CHECKED);
        for (int i = 0; i < filled; i++) fb_puts("\xe2\x96\x88"); /* █ */
        fb_puts(C_RESET);
        fb_puts(C_BORDER);
        for (int i = filled; i < bar_w; i++) fb_puts("\xe2\x96\x91"); /* ░ */
        fb_puts("]");
        fb_puts(C_RESET);

        char count[40];
        snprintf(count, sizeof count, "%d / %d  (%d%%)",
                 done, total, (int)((long)done * 100 / total));
        fb_move(row + 5, (tw - (int)strlen(count)) / 2 + 1);
        fb_puts(C_HINT); fb_puts(count); fb_puts(C_RESET);
    }

    fb_flush();
}

/* Throttled progress callback for render_to_png. */
typedef struct {
    const char *title;
    const char *detail;
    int last_pct;
} TuiProgressCtx;

static void tui_png_progress(int done, int total, void *userdata)
{
    TuiProgressCtx *ctx = (TuiProgressCtx *)userdata;
    if (total <= 0) return;
    int pct = (int)((long)done * 100 / total);
    if (pct == ctx->last_pct && done != total) return;
    ctx->last_pct = pct;
    tui_draw_progress(ctx->title, ctx->detail, done, total);
}

static void tui_write_view_script(const char *out_dir,
                                  char (*paths)[TUI_PATH_LEN], int count)
{
    char script_path[TUI_PATH_LEN + 32];
    snprintf(script_path, sizeof script_path, "%s/view_ascii.ps1", out_dir);
    FILE *f = fopen(script_path, "w");
    if (!f) return;
    fputs("# Auto-generated by img2ascii. Run with:  powershell -File view_ascii.ps1\n", f);
    fputs("$ErrorActionPreference = 'Stop'\n", f);
    fputs("[Console]::OutputEncoding = [System.Text.Encoding]::UTF8\n", f);
    fputs("$here = Split-Path -Parent $MyInvocation.MyCommand.Path\n", f);
    fputs("$files = @(\n", f);
    for (int i = 0; i < count; i++) {
        char stem[TUI_PATH_LEN];
        tui_basename_no_ext(paths[i], stem, sizeof stem);
        fprintf(f, "    \"%s_ascii.txt\"%s\n", stem,
                (i + 1 < count) ? "," : "");
    }
    fputs(")\n", f);
    fputs("foreach ($f in $files) {\n", f);
    fputs("    Clear-Host\n", f);
    fputs("    [Console]::Write((Get-Content -Raw -Path (Join-Path $here $f)))\n", f);
    fputs("    Write-Host \"\"\n", f);
    fputs("    Write-Host \"--- $f --- press Enter for next, Ctrl+C to quit ---\"\n", f);
    fputs("    [void](Read-Host)\n", f);
    fputs("}\n", f);
    fclose(f);
}

static void tui_generate(TuiState *s)
{
    uint8_t flags = 0;
    if (s->grayscale) flags |= GRAYSCALE_FLAG;
    if (s->reverse)   flags |= REVERSE_FLAG;

    const char *chars = s->chars[0] ? s->chars : TUI_DEFAULT_CHARS;
    int requested_width = atoi(s->width_str);
    const char *font_path = s->png_out ? find_default_font() : NULL;

    if (s->png_out && !font_path) {
        set_status(s, 1, "Error: PNG output needs a font (Cascadia Mono / Consolas not found).");
        return;
    }

    /* ---------- Batch ---------- */
    if (s->batch) {
        if (s->batch_count == 0) {
            set_status(s, 1, "Error: no input files selected (open Pick... in Inputs).");
            return;
        }
        if (s->output_path[0] == '\0' || !tui_is_directory(s->output_path)) {
            set_status(s, 1, "Error: batch mode requires an existing output directory.");
            return;
        }

        int ok = 0, fail = 0, png_ok = 0, png_fail = 0;
        for (int i = 0; i < s->batch_count; i++) {
            char detail[TUI_PATH_LEN + 32];
            snprintf(detail, sizeof detail, "%d / %d  ·  %s",
                     i + 1, s->batch_count, s->batch_paths[i]);
            tui_draw_progress("Generating ASCII...", detail, i, s->batch_count);

            int w = 0, h = 0;
            uint8_t *image = NULL;
            char *output = tui_render_one(s->batch_paths[i], chars, flags,
                                          requested_width, s->preset, &w, &h,
                                          s->png_out ? &image : NULL);
            if (!output) { fail++; continue; }

            char stem[TUI_PATH_LEN];
            tui_basename_no_ext(s->batch_paths[i], stem, sizeof stem);
            if (!stem[0]) snprintf(stem, sizeof stem, "image_%d", i);

            if (s->txt_out) {
                char saved_to[2048];
                snprintf(saved_to, sizeof saved_to, "%s/%s_ascii.txt",
                         s->output_path, stem);

                if (tui_save_output(saved_to, output)) ok++; else fail++;
            }

            if (s->png_out && image) {
                char png_path[2048];
                snprintf(png_path, sizeof png_path, "%s/%s_ascii.png",
                         s->output_path, stem);
                char png_detail[TUI_PATH_LEN + 64];
                snprintf(png_detail, sizeof png_detail,
                         "PNG %d / %d  ·  %s_ascii.png",
                         i + 1, s->batch_count, stem);
                TuiProgressCtx ctx = { "Rendering PNG...", png_detail, -1 };
                int pixel_height = png_pick_pixel_height(w, h, s->png_hq ? 1 : 0);
                int r = render_to_png(png_path, image, w, h, chars, flags,
                                      font_path, pixel_height, tui_png_progress, &ctx);
                if (r == 1) png_ok++; else png_fail++;
            }
            if (image) free(image);
            free(output);
        }
        tui_draw_progress("Generating ASCII...", "Done.", s->batch_count, s->batch_count);

        tui_write_view_script(s->output_path, s->batch_paths, s->batch_count);

        char m[sizeof s->status];
        if (s->png_out)
            snprintf(m, sizeof m,
                     "Batch: %d txt ok / %d failed; %d png ok / %d failed.",
                     ok, fail, png_ok, png_fail);
        else
            snprintf(m, sizeof m,
                     "Batch done: %d ok, %d failed. Viewer: %.180s/view_ascii.ps1",
                     ok, fail, s->output_path);
        set_status(s, (fail || png_fail) ? 1 : 2, m);
        return;
    }

    /* ---------- Single ---------- */
    if (s->input_path[0] == '\0') {
        set_status(s, 1, "Error: no input file selected.");
        return;
    }
    FILE *f = fopen(s->input_path, "rb");
    if (!f) { set_status(s, 1, "Error: cannot open the input file."); return; }
    fclose(f);

    int width = 0, height = 0;
    uint8_t *image = NULL;
    char *output = tui_render_one(s->input_path, chars, flags,
                                  requested_width, s->preset, &width, &height,
                                  s->png_out ? &image : NULL);
    if (!output) {
        set_status(s, 1, "Error: failed to load or resize the image.");
        return;
    }

    char saved_to[TUI_PATH_LEN] = {0};
    if (s->txt_out) {
        resolve_output_path(s->output_path, s->input_path, saved_to, sizeof saved_to);
    }

    int saved = 0;
    if (saved_to[0]) saved = tui_save_output(saved_to, output);

    int png_saved = 0;
    char png_path[TUI_PATH_LEN + 16] = {0};
    if (s->png_out && image) {
        resolve_png_output_path(s->output_path, s->input_path,
                                png_path, sizeof png_path);
    }
    if (s->png_out && image && png_path[0]) {
        char detail[TUI_PATH_LEN + 32];
        snprintf(detail, sizeof detail, "%s", png_path);
        TuiProgressCtx ctx = { "Rendering PNG...", detail, -1 };
        int pixel_height = png_pick_pixel_height(width, height, s->png_hq ? 1 : 0);
        png_saved = (render_to_png(png_path, image, width, height,
                                   chars, flags, font_path, pixel_height,
                                   tui_png_progress, &ctx) == 1);
    }

    tui_preview(s, output, width, height, saved_to, saved);

    char m[sizeof s->status];
    if (saved_to[0] && saved) {
        if (s->png_out)
            snprintf(m, sizeof m, "Generated %dx%d → %.140s (png: %s).",
                     width, height, saved_to, png_saved ? "ok" : "failed");
        else
            snprintf(m, sizeof m, "Generated %dx%d, saved to %.180s.",
                     width, height, saved_to);
        set_status(s, (s->png_out && !png_saved) ? 1 : 2, m);
    } else if (saved_to[0]) {
        snprintf(m, sizeof m, "Generated %dx%d, but failed to save to %.180s.",
                 width, height, saved_to);
        set_status(s, 1, m);
    } else {
        snprintf(m, sizeof m, "Generated %dx%d (set an output path to save).",
                 width, height);
        set_status(s, 0, m);
    }
    if (image) free(image);
    free(output);
}

/* ------------------------------------------------------------------ */
/* Main loop                                                          */
/* ------------------------------------------------------------------ */

static void tui_init_state(TuiState *s)
{
    memset(s, 0, sizeof *s);
    strncpy(s->chars, TUI_DEFAULT_CHARS, sizeof(s->chars) - 1);
    s->txt_out = true;  /* Generate txt files by default */
    s->png_out = false; /* PNG generation is opt-in */
    s->png_hq = false;  /* HQ PNG is opt-in */
}

static void run_tui(void)
{
    term_init();

    TuiState s;
    tui_init_state(&s);

    int focus   = F_INPUT;
    int editing = 0;

    while (1) {
        tui_draw_form(&s, focus, editing);

        int ch;
        int k = term_read_key(&ch);

        if (editing) {
            char *buf = NULL;
            int   cap = 0;
            switch (focus) {
            case F_INPUT:
                if (s.batch) { editing = 0; continue; } /* batch input is read-only */
                buf = s.input_path;  cap = (int)sizeof s.input_path;  break;
            case F_OUTPUT: buf = s.output_path; cap = (int)sizeof s.output_path; break;
            case F_WIDTH:
                if (s.preset != 0) { editing = 0; continue; }
                buf = s.width_str;   cap = (int)sizeof s.width_str;   break;
            case F_CHARS:  buf = s.chars;       cap = (int)sizeof s.chars;       break;
            default: editing = 0; continue;
            }
            int len = (int)strlen(buf);

            if (k == KEY_ENTER || k == KEY_ESC || k == KEY_TAB) {
                editing = 0;
            } else if (k == KEY_BACKSPACE) {
                if (len > 0) buf[len - 1] = '\0';
            } else if (k == KEY_SPACE && len < cap - 1 && focus != F_WIDTH) {
                buf[len] = ' '; buf[len + 1] = '\0';
            } else if (k == KEY_CHAR && len < cap - 1) {
                if (focus == F_WIDTH && (ch < '0' || ch > '9')) continue;
                buf[len] = (char)ch; buf[len + 1] = '\0';
            }
            continue;
        }

        if (k != KEY_NONE) s.status[0] = '\0';

        if (k == KEY_TAB || k == KEY_DOWN) {
            focus = (focus + 1) % N_FIELDS;
            /* Skip width when a preset hides it. */
            if (s.preset != 0 && focus == F_WIDTH) focus = (focus + 1) % N_FIELDS;
            if (!s.png_out && focus == F_PNG_HQ) focus = F_GENERATE;
        } else if (k == KEY_UP) {
            focus = (focus - 1 + N_FIELDS) % N_FIELDS;
            if (s.preset != 0 && focus == F_WIDTH) focus = (focus - 1 + N_FIELDS) % N_FIELDS;
            if (!s.png_out && focus == F_PNG_HQ) focus = F_PNG;
        } else if (k == KEY_LEFT) {
            if (focus == F_REVERSE)        focus = F_GRAYSCALE;
            else if (focus == F_PRINT)     focus = F_REVERSE;
            else if (focus == F_DEBUG)     focus = F_PRINT;
            else if (focus == F_PNG)       focus = F_TXT;
            else if (focus == F_PNG_HQ)    focus = F_PNG;
            else if (focus == F_PRESET)    { if (s.preset == 0) focus = F_WIDTH; }
            else if (focus == F_QUIT)      focus = F_GENERATE;
            else if (focus == F_INPUT_BROWSE)  focus = F_INPUT;
            else if (focus == F_OUTPUT_BROWSE) focus = F_OUTPUT;
        } else if (k == KEY_RIGHT) {
            if (focus == F_GRAYSCALE)      focus = F_REVERSE;
            else if (focus == F_REVERSE)   focus = F_PRINT;
            else if (focus == F_PRINT)     focus = F_DEBUG;
            else if (focus == F_TXT)       focus = F_PNG;
            else if (focus == F_PNG)       focus = s.png_out ? F_PNG_HQ : F_GENERATE;
            else if (focus == F_WIDTH)     focus = F_PRESET;
            else if (focus == F_GENERATE)  focus = F_QUIT;
            else if (focus == F_INPUT)     focus = F_INPUT_BROWSE;
            else if (focus == F_OUTPUT)    focus = F_OUTPUT_BROWSE;
        } else if (k == KEY_SPACE) {
            switch (focus) {
            case F_GRAYSCALE: s.grayscale        = !s.grayscale;        break;
            case F_REVERSE:   s.reverse          = !s.reverse;          break;
            case F_PRINT:     s.print_to_console = !s.print_to_console; break;
            case F_DEBUG:     s.debug            = !s.debug;            break;
            case F_TXT:       s.txt_out          = !s.txt_out;          break;
            case F_PNG:       s.png_out          = !s.png_out;
                              if (!s.png_out) s.png_hq = false;
                              break;
            case F_PNG_HQ:    if (s.png_out) s.png_hq = !s.png_hq;      break;
            case F_PRESET:    s.preset           = (s.preset + 1) % 4;  break;
            }
        } else if (k == KEY_ENTER) {
            switch (focus) {
            case F_INPUT:
                if (!s.batch) editing = 1;
                break;
            case F_OUTPUT: case F_CHARS:
                editing = 1; break;
            case F_WIDTH:
                if (s.preset == 0) editing = 1;
                break;
            case F_INPUT_BROWSE: {
                char cwd[TUI_PATH_LEN];
                term_get_cwd(cwd, sizeof cwd);
                int n = tui_multi_browser(cwd, s.batch_paths, TUI_MAX_BATCH);
                if (n > 0) {
                    s.batch_count = n;
                    /* Automatically enable batch mode if multiple files selected */
                    if (n > 1) {
                        s.batch = true;
                        char m[sizeof s.status];
                        snprintf(m, sizeof m, "Selected %d files (batch mode enabled).",
                                 n);
                        set_status(&s, 0, m);
                    } else {
                        /* Single file: disable batch mode and use single-file path */
                        s.batch = false;
                        strncpy(s.input_path, s.batch_paths[0], sizeof s.input_path - 1);
                        s.input_path[sizeof s.input_path - 1] = '\0';
                        char m[sizeof s.status];
                        snprintf(m, sizeof m, "Selected file: %.150s",
                                 s.batch_paths[0]);
                        set_status(&s, 0, m);
                    }
                }
                break;
            }
            case F_OUTPUT_BROWSE: {
                char cwd[TUI_PATH_LEN];
                term_get_cwd(cwd, sizeof cwd);
                char default_name[TUI_PATH_LEN];
                if (s.input_path[0] && !s.batch) {
                    char stem[TUI_PATH_LEN];
                    tui_basename_no_ext(s.input_path, stem, sizeof stem);
                    snprintf(default_name, sizeof default_name, "%.500s.txt", stem[0] ? stem : "output");
                } else {
                    strcpy(default_name, "output.txt");
                }
                char picked[TUI_PATH_LEN];
                if (tui_file_browser(BROWSE_OUTPUT, default_name, cwd, picked, sizeof picked)) {
                    strncpy(s.output_path, picked, sizeof s.output_path - 1);
                    s.output_path[sizeof s.output_path - 1] = '\0';
                }
                break;
            }
            case F_GRAYSCALE: s.grayscale        = !s.grayscale;        break;
            case F_REVERSE:   s.reverse          = !s.reverse;          break;
            case F_PRINT:     s.print_to_console = !s.print_to_console; break;
            case F_DEBUG:     s.debug            = !s.debug;            break;
            case F_TXT:       s.txt_out          = !s.txt_out;          break;
            case F_PNG:       s.png_out          = !s.png_out;
                              if (!s.png_out) s.png_hq = false;
                              break;
            case F_PNG_HQ:    if (s.png_out) s.png_hq = !s.png_hq;      break;
            case F_PRESET:    s.preset           = (s.preset + 1) % 4;  break;
            case F_GENERATE:  tui_generate(&s); term_flush_input();     break;
            case F_QUIT:      term_restore(); return;
            }
        } else if (k == KEY_ESC) {
            term_restore(); return;
        }
    }
}

#endif /* TUI_H */
