#ifndef PNG_RENDER_H
#define PNG_RENDER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_image/stb_truetype.h"

#include "utils.h"
#include "args.h"
#include "ascii_art.h"  /* must precede us — provides stbi_write_png + get_rgb / get_intensity */

#define PNG_DEFAULT_PIXEL_HEIGHT 16
#define PNG_HQ_PIXEL_HEIGHT      24

/*
 * Adaptive PNG glyph size for "HQ" mode.
 *
 * 100% 1080p terminal baseline is approximated as:
 *   240 cols x 67 rows (about 8x16 px cell metrics).
 *
 * If the ASCII grid fits within that, render with larger glyphs for sharper PNGs.
 * As grids exceed that size, smoothly fade back to the default quality.
 */
static int png_pick_pixel_height(int width, int height, int hq)
{
    if (!hq) return PNG_DEFAULT_PIXEL_HEIGHT;

    if (width <= 0 || height <= 0) return PNG_DEFAULT_PIXEL_HEIGHT;

    const float target_cols = 240.0f;
    const float target_rows = 67.0f;
    float fx = (float)width / target_cols;
    float fy = (float)height / target_rows;
    float overflow = (fx > fy) ? fx : fy;

    if (overflow <= 1.0f) return PNG_HQ_PIXEL_HEIGHT;
    if (overflow >= 2.0f) return PNG_DEFAULT_PIXEL_HEIGHT;

    float t = overflow - 1.0f; /* 0..1 */
    float ph = (float)PNG_HQ_PIXEL_HEIGHT
             - t * (float)(PNG_HQ_PIXEL_HEIGHT - PNG_DEFAULT_PIXEL_HEIGHT);
    return (int)(ph + 0.5f);
}

static const char *find_default_font(void)
{
    static const char *candidates[] = {
        "C:/Windows/Fonts/CascadiaMono.ttf", /* Windows Terminal / PS 7 default */
        "C:/Windows/Fonts/consola.ttf",      /* legacy powershell.exe default */
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) { fclose(f); return candidates[i]; }
    }
    return NULL;
}

static unsigned char *png_read_file(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

/* Replace a trailing ".txt" with ".png", or append ".png" if no .txt suffix. */
static void txt_path_to_png(const char *src, char *dst, size_t cap)
{
    size_t n = strlen(src);
    if (n >= 4 && (strcmp(src + n - 4, ".txt") == 0
                || strcmp(src + n - 4, ".TXT") == 0)) {
        snprintf(dst, cap, "%.*s.png", (int)(n - 4), src);
    } else {
        snprintf(dst, cap, "%s.png", src);
    }
}

/*
 * Render the same character-per-cell mapping used by the TXT output, but as a
 * PNG image. Black background; colored glyphs (or white in grayscale mode).
 */
typedef void (*png_progress_cb)(int done, int total, void *userdata);

static int render_to_png(
    const char *out_path,
    uint8_t *image, int width, int height,
    const char *characters_in,
    uint8_t flags,
    const char *font_path,
    int pixel_height,
    png_progress_cb cb,
    void *cb_data)
{
    if (!font_path)  return 0;
    if (!out_path)   return 0;
    if (width <= 0 || height <= 0) return 0;

    long font_len = 0;
    unsigned char *font_bytes = png_read_file(font_path, &font_len);
    if (!font_bytes) return 0;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_bytes,
                        stbtt_GetFontOffsetForIndex(font_bytes, 0))) {
        free(font_bytes);
        return 0;
    }

    if (pixel_height <= 0) pixel_height = PNG_DEFAULT_PIXEL_HEIGHT;
    float scale = stbtt_ScaleForPixelHeight(&font, (float)pixel_height);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    int baseline = (int)(ascent * scale + 0.5f);
    int cell_h   = (int)((ascent - descent + line_gap) * scale + 0.5f);
    if (cell_h < 1) cell_h = 1;

    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&font, 'M', &adv, &lsb);
    int cell_w = (int)(adv * scale + 0.5f);
    if (cell_w < 1) cell_w = 1;

    /* Pre-rasterize printable ASCII into per-cell alpha bitmaps. */
    const int FIRST = 32, LAST = 126;
    int N = LAST - FIRST + 1;
    unsigned char **glyphs = (unsigned char **)calloc((size_t)N, sizeof(unsigned char *));
    if (!glyphs) { free(font_bytes); return 0; }

    for (int i = 0; i < N; i++) {
        int cp = FIRST + i;
        int gw = 0, gh = 0, gx0 = 0, gy0 = 0;
        unsigned char *bmp = stbtt_GetCodepointBitmap(
            &font, 0.0f, scale, cp, &gw, &gh, &gx0, &gy0);

        unsigned char *cell = (unsigned char *)calloc((size_t)cell_w * (size_t)cell_h, 1);
        if (!cell) { if (bmp) stbtt_FreeBitmap(bmp, NULL); continue; }

        if (bmp) {
            int dst_x = gx0; if (dst_x < 0) dst_x = 0;
            int dst_y = baseline + gy0;
            for (int yy = 0; yy < gh; yy++) {
                int dy = dst_y + yy;
                if (dy < 0 || dy >= cell_h) continue;
                for (int xx = 0; xx < gw; xx++) {
                    int dx = dst_x + xx;
                    if (dx < 0 || dx >= cell_w) continue;
                    cell[dy * cell_w + dx] = bmp[yy * gw + xx];
                }
            }
            stbtt_FreeBitmap(bmp, NULL);
        }
        glyphs[i] = cell;
    }

    long img_w = (long)width  * cell_w;
    long img_h = (long)height * cell_h;
    if (img_w > 32767 || img_h > 32767) {
        /* stb_image_write hard-limits PNG to 16-bit dimensions. */
        for (int i = 0; i < N; i++) free(glyphs[i]);
        free(glyphs); free(font_bytes);
        return -1; /* signals "too big" */
    }
    uint8_t *out = (uint8_t *)calloc((size_t)img_w * (size_t)img_h * 3, 1);
    if (!out) {
        for (int i = 0; i < N; i++) free(glyphs[i]);
        free(glyphs); free(font_bytes);
        return 0;
    }

    char *characters = strdup(characters_in);
    if (flags & REVERSE_FLAG) reverse_string(characters);
    int characters_count = (int)strlen(characters);
    if (characters_count < 1) characters_count = 1;

    int gray = (flags & GRAYSCALE_FLAG) != 0;

    if (cb) cb(0, height, cb_data);
    int last_row_reported = -1;

    for (int i = 0; i < width * height; i++) {
        int intensity = get_intensity(image, i);
        int char_index = char_index_from_intensity(intensity, characters_count);
        unsigned char ch = (unsigned char)characters[char_index];
        if (ch < FIRST || ch > LAST) ch = '?';
        unsigned char *alpha = glyphs[ch - FIRST];
        if (!alpha) continue;

        int cx = i % width;
        int cy = i / width;

        if (cb && cy != last_row_reported) {
            cb(cy, height, cb_data);
            last_row_reported = cy;
        }

        uint8_t r, g, b;
        if (gray) {
            r = g = b = 255;
        } else {
            get_rgb(&r, &g, &b, image, i);
        }

        /* Use size_t for the offset math — on Windows `long` is 32 bits,
         * which overflows on PNGs around ~25000x12000 (max-res of large
         * images) and writes past the buffer → access violation. */
        size_t ox = (size_t)cx * (size_t)cell_w;
        size_t oy = (size_t)cy * (size_t)cell_h;
        size_t pitch = (size_t)img_w * 3;
        for (int yy = 0; yy < cell_h; yy++) {
            uint8_t *out_row = out + (oy + (size_t)yy) * pitch + ox * 3;
            unsigned char *a_row = alpha + (size_t)yy * (size_t)cell_w;
            for (int xx = 0; xx < cell_w; xx++) {
                unsigned char a = a_row[xx];
                if (!a) continue;
                out_row[xx * 3 + 0] = (uint8_t)((int)r * a / 255);
                out_row[xx * 3 + 1] = (uint8_t)((int)g * a / 255);
                out_row[xx * 3 + 2] = (uint8_t)((int)b * a / 255);
            }
        }
    }

    if (cb) cb(height, height, cb_data);

    int ok = stbi_write_png(out_path, (int)img_w, (int)img_h,
                            3, out, (int)(img_w * 3));

    free(out);
    free(characters);
    for (int i = 0; i < N; i++) free(glyphs[i]);
    free(glyphs);
    free(font_bytes);
    return ok ? 1 : 0;
}

#endif /* PNG_RENDER_H */
