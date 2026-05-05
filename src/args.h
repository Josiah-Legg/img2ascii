#ifndef ARGS_H
#define ARGS_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

#define MAX_INPUTS 1024

enum {
    GRAYSCALE_FLAG = 1 << 0,
    REVERSE_FLAG   = 1 << 1,
    PRINT_FLAG     = 1 << 2,
    DEBUG_FLAG     = 1 << 3,
    MAX_RES_FLAG   = 1 << 4,
    PNG_FLAG       = 1 << 5
};

void process_arguments(
    int argc,
    char **argv,
    const char **inputs,
    int *input_count,
    char **output_filepath,
    char **characters,
    int *desired_width,
    uint8_t *flags,
    bool *resize_image,
    const char **font_path
) {
    if (argc == 1) {
        printf("No input file\n");
        show_usage();
        exit(EXIT_FAILURE);
    }

    bool parsing_options = true;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (parsing_options && strcmp(arg, "--") == 0) {
            parsing_options = false;
            continue;
        }

        if (parsing_options && arg[0] == '-' && arg[1] != '\0') {
            const char *value = NULL;

            if (arg[1] == '-') {
                const char *name = arg + 2;
                const char *eq = strchr(name, '=');
                size_t name_len = eq ? (size_t)(eq - name) : strlen(name);

                if (strcmp(name, "help") == 0) {
                    show_usage();
                    exit(EXIT_SUCCESS);
                } else if (strncmp(name, "input", name_len) == 0 && name_len == 5) {
                    if (eq && eq[1]) value = eq + 1;
                    else if (i + 1 < argc) value = argv[++i];
                    else value = NULL;
                    if (value && *input_count < MAX_INPUTS) inputs[(*input_count)++] = value;
                } else if (strncmp(name, "output", name_len) == 0 && name_len == 6) {
                    if (eq && eq[1]) value = eq + 1;
                    else if (i + 1 < argc) value = argv[++i];
                    if (value) *output_filepath = (char *)value;
                } else if (strncmp(name, "width", name_len) == 0 && name_len == 5) {
                    if (eq && eq[1]) value = eq + 1;
                    else if (i + 1 < argc) value = argv[++i];
                    if (value) {
                        *desired_width = atoi(value);
                        *resize_image = true;
                    }
                } else if (strncmp(name, "chars", name_len) == 0 && name_len == 5) {
                    if (eq && eq[1]) value = eq + 1;
                    else if (i + 1 < argc) value = argv[++i];
                    if (value && strlen(value) != 0) {
                        char *copy = strdup(value);
                        if (copy) {
                            free(*characters);
                            *characters = copy;
                        }
                    }
                } else if (strncmp(name, "grayscale", name_len) == 0 && name_len == 9) {
                    *flags |= GRAYSCALE_FLAG;
                } else if (strncmp(name, "print", name_len) == 0 && name_len == 5) {
                    *flags |= PRINT_FLAG;
                } else if (strncmp(name, "reverse", name_len) == 0 && name_len == 7) {
                    *flags |= REVERSE_FLAG;
                } else if (strncmp(name, "debug", name_len) == 0 && name_len == 5) {
                    *flags |= DEBUG_FLAG;
                } else if (strncmp(name, "max", name_len) == 0 && name_len == 3) {
                    *flags |= MAX_RES_FLAG;
                } else if (strncmp(name, "tui", name_len) == 0 && name_len == 3) {
                    /* handled in main */
                } else if (strncmp(name, "png", name_len) == 0 && name_len == 3) {
                    *flags |= PNG_FLAG;
                } else if (strncmp(name, "font", name_len) == 0 && name_len == 4) {
                    if (eq && eq[1]) value = eq + 1;
                    else if (i + 1 < argc) value = argv[++i];
                    if (value) *font_path = value;
                } else {
                    printf("\nHint: Use the \x1b[1m--help\x1b[0m option to get help about the usage \n\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                char opt = arg[1];
                const char *inline_value = (arg[2] != '\0') ? arg + 2 : NULL;

                switch (opt) {
                case 'h':
                    show_usage();
                    exit(EXIT_SUCCESS);
                case 'i':
                    value = inline_value ? inline_value : ((i + 1 < argc) ? argv[++i] : NULL);
                    if (value && *input_count < MAX_INPUTS) inputs[(*input_count)++] = value;
                    break;
                case 'o':
                    value = inline_value ? inline_value : ((i + 1 < argc) ? argv[++i] : NULL);
                    if (value) *output_filepath = (char *)value;
                    break;
                case 'w':
                    value = inline_value ? inline_value : ((i + 1 < argc) ? argv[++i] : NULL);
                    if (value) {
                        *desired_width = atoi(value);
                        *resize_image = true;
                    }
                    break;
                case 'c':
                    value = inline_value ? inline_value : ((i + 1 < argc) ? argv[++i] : NULL);
                    if (value && strlen(value) != 0) {
                        char *copy = strdup(value);
                        if (copy) {
                            free(*characters);
                            *characters = copy;
                        }
                    }
                    break;
                case 'g': *flags |= GRAYSCALE_FLAG; break;
                case 'p': *flags |= PRINT_FLAG;     break;
                case 'r': *flags |= REVERSE_FLAG;   break;
                case 'd': *flags |= DEBUG_FLAG;     break;
                case 'm': *flags |= MAX_RES_FLAG;   break;
                case 'P': *flags |= PNG_FLAG;       break;
                case 't': /* handled in main */     break;
                default:
                    printf("\nHint: Use the \x1b[1m--help\x1b[0m option to get help about the usage \n\n");
                    exit(EXIT_FAILURE);
                }
            }

            continue;
        }

        if (*input_count < MAX_INPUTS) {
            inputs[(*input_count)++] = arg;
        }
    }

    if (*input_count == 0) {
        printf("No input file\n");
        show_usage();
        exit(EXIT_FAILURE);
    }

    /* --max overrides any -w and disables explicit resize. */
    if (*flags & MAX_RES_FLAG) {
        *resize_image = false;
        *desired_width = 0;
    }

    if (*output_filepath == NULL && *input_count == 1) {
        *flags |= PRINT_FLAG;
    }
}

#endif // ARGS_H
