#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *portable_strdup(const char *src)
{
    size_t len = strlen(src) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) return NULL;
    memcpy(copy, src, len);
    return copy;
}

#define strdup portable_strdup

#ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

static int path_is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

static void path_basename_no_ext(const char *path, char *out, size_t cap)
{
    const char *fwd  = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *base = (back > fwd) ? back + 1 : (fwd ? fwd + 1 : path);
    strncpy(out, base, cap - 1);
    out[cap - 1] = 0;
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

void reverse_string(char *str)
{
    int len = strlen(str);

    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

void show_usage(void)
{
    printf(
        "\nUsage: \x1b[1mimg2ascii [options] -i <FILE> [-o <FILE>]\x1b[0m \n"
        "       \x1b[1mimg2ascii [options] -o <DIR> <FILE1> <FILE2> ...\x1b[0m   (batch) \n"
        "       \x1b[1mimg2ascii\x1b[0m   (no args - launches the interactive TUI) \n\n"

        "A command-line tool for converting images to ASCII art \n\n"

        "Options: \n"
        "   -i, --input  <FILE>     Path of an input image file (repeatable) \n"
        "   -o, --output <PATH>     Output file (single input) or directory (batch) \n"
        "   -w, --width  <NUMBER>   Width of the output \n"
        "   -m, --max               Use the image's native resolution (overrides -w) \n"
        "   -c, --chars  <STRING>   Characters to be used for the ASCII image \n"
        "   -g, --grayscale         Use grayscale output (no color) \n"
        "   -p, --print             Print the output to the console \n"
        "   -r, --reverse           Reverse the string of characters \n"
        "   -d, --debug             Print some useful information \n"
        "   -P, --png               Also write a .png alongside each .txt \n"
        "       --font <FILE>       TTF font for PNG output (default: Cascadia Mono) \n"
        "   -t, --tui               Launch the interactive TUI \n\n"

        "Batch mode is triggered when more than one input is given. The output \n"
        "argument must then be a directory; each result is saved as \n"
        "<DIR>/<stem>_ascii.txt and a view_ascii.ps1 viewer script is generated. \n\n"
    );
}

#endif // UTILS_H