#include "face_draw.h"
#include <math.h>
#include <ctype.h>

#define PI_F 3.14159265f

static uint16_t *s_fb;
static int s_w;

static inline uint16_t to565(uint32_t c)
{
    uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void fd_begin(uint16_t *fb, int stride)
{
    s_fb = fb;
    s_w = stride;
}

void fd_set_px(int x, int y, uint32_t c)
{
    if (x >= 0 && x < FD_W && y >= 0 && y < FD_H) {
        s_fb[y * s_w + x] = to565(c);
    }
}

uint32_t fd_lerp888(uint32_t a, uint32_t b, float t)
{
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    uint8_t r = (uint8_t)(((a >> 16) & 0xFF) + t * (((b >> 16) & 0xFF) - (float)((a >> 16) & 0xFF)));
    uint8_t g = (uint8_t)(((a >> 8) & 0xFF) + t * (((b >> 8) & 0xFF) - (float)((a >> 8) & 0xFF)));
    uint8_t bl = (uint8_t)((a & 0xFF) + t * ((b & 0xFF) - (float)(a & 0xFF)));
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

void fd_fill_screen(uint32_t c)
{
    uint16_t v = to565(c);
    for (int i = 0; i < s_w * FD_H; ++i) {
        s_fb[i] = v;
    }
}

void fd_fill_rect(int x0, int y0, int x1, int y1, uint32_t c)
{
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            fd_set_px(x, y, c);
        }
    }
}

void fd_fill_ellipse(float cx, float cy, float rx, float ry, uint32_t c)
{
    for (int y = (int)(cy - ry); y <= (int)(cy + ry) + 1; ++y) {
        float ny = (y - cy) / ry;
        for (int x = (int)(cx - rx); x <= (int)(cx + rx) + 1; ++x) {
            float nx = (x - cx) / rx;
            if (nx * nx + ny * ny <= 1.f) {
                fd_set_px(x, y, c);
            }
        }
    }
}

void fd_crescent(float cx, float cy, float rx, float ry, float shift, uint32_t c)
{
    for (int y = (int)(cy - ry); y <= (int)(cy + ry) + 1; ++y) {
        for (int x = (int)(cx - rx); x <= (int)(cx + rx) + 1; ++x) {
            float nx = (x - cx) / rx;
            float ny = (y - cy) / ry;
            float ny2 = (y - (cy + shift)) / ry;
            if (nx * nx + ny * ny <= 1.f && nx * nx + ny2 * ny2 > 1.f) {
                fd_set_px(x, y, c);
            }
        }
    }
}

void fd_arc_band(float cx, float cy, float r_in, float r_out, uint32_t c,
                 int lower, float span)
{
    for (int y = (int)(cy - r_out); y <= (int)(cy + r_out) + 1; ++y) {
        float dy = y - cy;
        if ((lower && dy < 0) || (!lower && dy > 0)) {
            continue;
        }
        for (int x = (int)(cx - r_out); x <= (int)(cx + r_out) + 1; ++x) {
            float dx = x - cx;
            if (fabsf(dx) > r_out * span) {
                continue;
            }
            float d = sqrtf(dx * dx + dy * dy);
            if (d >= r_in && d <= r_out) {
                fd_set_px(x, y, c);
            }
        }
    }
}

void fd_ring_arc(float cx, float cy, float r_in, float r_out,
                 float a0, float a1, uint32_t c0, uint32_t c1)
{
    float span = a1 - a0;
    if (span <= 0.f) {
        return;
    }
    for (int y = (int)(cy - r_out); y <= (int)(cy + r_out) + 1; ++y) {
        float dy = y - cy;
        for (int x = (int)(cx - r_out); x <= (int)(cx + r_out) + 1; ++x) {
            float dx = x - cx;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < r_in || d > r_out) {
                continue;
            }
            float rel = fmodf(atan2f(dy, dx) - a0, 2 * PI_F);
            if (rel < 0.f) {
                rel += 2 * PI_F;
            }
            if (rel <= span) {
                uint32_t c = (c0 == c1) ? c0 : fd_lerp888(c0, c1, rel / span);
                fd_set_px(x, y, c);
            }
        }
    }
}

void fd_circle_outline(float cx, float cy, float r, float t, uint32_t c)
{
    fd_ring_arc(cx, cy, r - t, r, 0.f, 2 * PI_F - 1e-6f, c, c);
}

void fd_thick_line(float x0, float y0, float x1, float y1, float t, uint32_t c)
{
    float dx = fabsf(x1 - x0), dy = fabsf(y1 - y0);
    int steps = (int)(dx > dy ? dx : dy) + 1;
    for (int i = 0; i <= steps; ++i) {
        float f = (float)i / steps;
        fd_fill_ellipse(x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, t, t, c);
    }
}

void fd_half_disc(float cx, float my, float rx, float ry, uint32_t c)
{
    for (int y = (int)my; y <= (int)(my + ry) + 1; ++y) {
        float ny = (y - my) / ry;
        for (int x = (int)(cx - rx); x <= (int)(cx + rx) + 1; ++x) {
            float nx = (x - cx) / rx;
            if (nx * nx + ny * ny <= 1.f) {
                fd_set_px(x, y, c);
            }
        }
    }
}

void fd_clip_round(void)
{
    for (int y = 0; y < FD_H; ++y) {
        int dy = y - FD_CY;
        for (int x = 0; x < FD_W; ++x) {
            int dx = x - FD_CX;
            if (dx * dx + dy * dy > FD_SCREEN_R * FD_SCREEN_R) {
                s_fb[y * s_w + x] = 0;
            }
        }
    }
}

/* ------------------------------------------------------------- 5x7 font */
typedef struct {
    char ch;
    uint8_t rows[7];
} glyph_t;

static const glyph_t FONT[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F } },
    { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
    { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
    { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
    { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } },
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } },
    { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
    { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
    { '*', { 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00 } }, /* degree */
    { '%', { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } },
    { ':', { 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00 } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 } },
    { '-', { 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00 } },
    { '+', { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } },
    { '/', { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 } },
    { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } },
};

static const uint8_t *find_glyph(char ch)
{
    ch = (char)toupper((unsigned char)ch);
    for (unsigned i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].ch == ch) {
            return FONT[i].rows;
        }
    }
    return NULL; /* space / unknown */
}

int fd_text_w(const char *s, int scale)
{
    int n = 0;
    while (s[n]) {
        n++;
    }
    return n > 0 ? n * 6 * scale - scale : 0;
}

void fd_text(int x, int y, const char *s, int scale, uint32_t c)
{
    for (; *s; ++s) {
        const uint8_t *rows = find_glyph(*s);
        if (rows) {
            for (int r = 0; r < 7; ++r) {
                uint8_t bits = rows[r];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (1 << (4 - col))) {
                        fd_fill_rect(x + col * scale, y + r * scale,
                                     x + (col + 1) * scale, y + (r + 1) * scale, c);
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

void fd_text_center(int y, const char *s, int scale, uint32_t c)
{
    fd_text(FD_CX - fd_text_w(s, scale) / 2, y, s, scale, c);
}
