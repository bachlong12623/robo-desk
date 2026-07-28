/* Anime-style face renderer for GC9A01 (240x240 round display).
 *
 * Mirrors tools/preview_faces.py — keep both in sync when editing.
 * All drawing is procedural (ellipses/arcs/lines) via face_draw primitives.
 */
#include "face_pixel.h"
#include "face_draw.h"
#include <math.h>

#define LOOP 48
#define PI_F 3.14159265f

/* ---- palette (RGB888) ---- */
#define C_BG      0x0A0C1Au
#define C_EYE_TOP 0x96F0FFu
#define C_EYE_BOT 0x2373EBu
#define C_EYE_RIM 0x123C8Cu
#define C_PUPIL   0x0A2869u
#define C_WHITE   0xFFFFFFu
#define C_MOUTH_L 0xEBF5FFu
#define C_MOUTH_D 0x691A30u
#define C_MOUTH_R 0xE1788Cu
#define C_TONGUE  0xF06E82u
#define C_BLUSH   0xFF7896u
#define C_WARN    0xFFCD3Cu
#define C_DANGER  0xFF4650u
#define C_HEART   0xFF5A82u
#define C_HEARTHI 0xFFAAC8u
#define C_STAR    0xFFDC5Au
#define C_ZZZ     0x96CDFFu
#define C_BROW    0x1E50AAu
#define C_TEAR    0x78C8FFu
#define C_SPIRAL  0xFFBE5Au
#define C_BROW_A  0xC83C46u
#define C_DOT_ON  0x5ADCFFu
#define C_DOT_OFF 0x284678u

/* ---------------------------------------------------- face parts */
#define EYE_LX 76.f
#define EYE_RXC 164.f
#define EYE_Y 104.f
#define EYE_RX 27.f
#define EYE_RY 38.f

/* eye ellipse: vertical gradient + darker rim */
static void fill_eye_ellipse(float cx, float cy, float rx, float ry)
{
    for (int y = (int)(cy - ry); y <= (int)(cy + ry) + 1; ++y) {
        float ny = (y - cy) / ry;
        for (int x = (int)(cx - rx); x <= (int)(cx + rx) + 1; ++x) {
            float nx = (x - cx) / rx;
            float d = nx * nx + ny * ny;
            if (d <= 1.f) {
                if (d >= 0.82f) {
                    fd_set_px(x, y, C_EYE_RIM);
                } else {
                    fd_set_px(x, y, fd_lerp888(C_EYE_TOP, C_EYE_BOT, (ny + 1.f) / 2.f));
                }
            }
        }
    }
}

/* blink: 0=open 1=closed; lid_slant >0 angry-side, <0 sad-side */
static void draw_open_eye(float cx, float cy, float scale, float look_x,
                          float look_y, float blink, float lid_slant)
{
    float rx = EYE_RX * scale, ry = EYE_RY * scale;
    if (blink >= 0.97f) {
        fd_thick_line(cx - rx, cy, cx + rx, cy, 3.4f, C_EYE_RIM);
        return;
    }
    fill_eye_ellipse(cx, cy, rx, ry);
    fd_fill_ellipse(cx + look_x, cy + 6 + look_y, rx * 0.52f, ry * 0.42f, C_PUPIL);
    fd_fill_ellipse(cx + look_x * 0.6f - rx * 0.32f, cy + look_y * 0.6f - ry * 0.38f,
                    rx * 0.30f, ry * 0.20f, C_WHITE);
    fd_fill_ellipse(cx + look_x * 0.6f + rx * 0.28f, cy + look_y * 0.6f + ry * 0.30f,
                    rx * 0.14f, ry * 0.09f, C_WHITE);
    if (blink > 0.f || lid_slant != 0.f) {
        float base = cy - ry + blink * 2.f * ry;
        for (int x = (int)(cx - rx) - 2; x <= (int)(cx + rx) + 2; ++x) {
            float lid_y = base + lid_slant * (x - cx);
            for (int y = (int)(cy - ry) - 2; y <= (int)lid_y; ++y) {
                float dx = (x - cx) / (rx + 2);
                float dy = (y - cy) / (ry + 2);
                if (dx * dx + dy * dy <= 1.f) {
                    fd_set_px(x, y, C_BG);
                }
            }
        }
        fd_thick_line(cx - rx, base - lid_slant * rx,
                      cx + rx, base + lid_slant * rx, 2.2f, C_EYE_RIM);
    }
}

static void draw_happy_eye(float cx, float cy)
{
    fd_crescent(cx, cy, EYE_RX * 0.95f, EYE_RY * 0.55f, 11.f,
                fd_lerp888(C_EYE_TOP, C_EYE_BOT, 0.35f));
}

static void draw_heart(float cx, float cy, float s, uint32_t c)
{
    float r = 11.f * s;
    fd_fill_ellipse(cx - r * 0.85f, cy - r * 0.5f, r, r, c);
    fd_fill_ellipse(cx + r * 0.85f, cy - r * 0.5f, r, r, c);
    float h = 2.1f * r;
    for (int i = 0; i < (int)h; ++i) {
        float f = i / h;
        float half = (1.f - f) * 1.8f * r;
        int y = (int)(cy - r * 0.1f) + i;
        fd_fill_rect((int)(cx - half), y, (int)(cx + half) + 1, y + 1, c);
    }
    fd_fill_ellipse(cx - r * 0.9f, cy - r * 0.7f, r * 0.32f, r * 0.32f, C_HEARTHI);
}

static void draw_star(float cx, float cy, float s, uint32_t c)
{
    float a = 21.f * s;
    for (int y = (int)(cy - a); y <= (int)(cy + a); ++y) {
        for (int x = (int)(cx - a); x <= (int)(cx + a); ++x) {
            float nx = fabsf(x - cx) / a;
            float ny = fabsf(y - cy) / a;
            if (sqrtf(nx) + sqrtf(ny) <= 1.f) {
                fd_set_px(x, y, c);
            }
        }
    }
    fd_fill_ellipse(cx - a * 0.25f, cy - a * 0.25f, 2.6f * s, 2.6f * s, C_WHITE);
}

static void draw_x_eye(float cx, float cy, float s, uint32_t c)
{
    float r = 16.f * s;
    fd_thick_line(cx - r, cy - r, cx + r, cy + r, 4.2f, c);
    fd_thick_line(cx - r, cy + r, cx + r, cy - r, 4.2f, c);
}

static void draw_spiral_eye(float cx, float cy, float rot, uint32_t c)
{
    const float rmax = 19.f;
    for (int y = (int)(cy - rmax); y <= (int)(cy + rmax); ++y) {
        for (int x = (int)(cx - rmax); x <= (int)(cx + rmax); ++x) {
            float dx = x - cx, dy = y - cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > rmax || d < 2.f) {
                continue;
            }
            float ang = atan2f(dy, dx) + rot;
            float arm = fmodf(ang / (2 * PI_F) * 2.4f + d / rmax * 2.6f, 1.f);
            if (arm < 0.f) arm += 1.f;
            if (arm < 0.42f) {
                fd_set_px(x, y, c);
            }
        }
    }
}

static void draw_brow(float cx, float cy, float slant, uint32_t c)
{
    float y0 = cy - EYE_RY - 12;
    fd_thick_line(cx - EYE_RX * 0.9f, y0 - slant * 9,
                  cx + EYE_RX * 0.9f, y0 + slant * 9, 3.6f, c);
}

static void draw_blush(int strong)
{
    float ry = strong ? 7.f : 5.f;
    fd_fill_ellipse(50, 148, 15, ry, C_BLUSH);
    fd_fill_ellipse(190, 148, 15, ry, C_BLUSH);
}

static void draw_zzz(uint32_t f)
{
    float ph = (f % 36) / 36.f;
    for (int i = 0; i < 3; ++i) {
        float s = 1.f - i * 0.25f;
        float yy = 62 - i * 20 - ph * 8;
        float xx = 168 + i * 14 + sinf(ph * 6.28f + i) * 3;
        float r = 8 * s;
        float t = 1.8f * s + 0.6f;
        fd_thick_line(xx - r, yy - r, xx + r, yy - r, t, C_ZZZ);
        fd_thick_line(xx + r, yy - r, xx - r, yy + r, t, C_ZZZ);
        fd_thick_line(xx - r, yy + r, xx + r, yy + r, t, C_ZZZ);
    }
}

static void draw_excl(float cx, float cy, uint32_t c)
{
    fd_fill_rect((int)cx - 3, (int)cy - 16, (int)cx + 4, (int)cy + 4, c);
    fd_fill_ellipse(cx, cy + 12, 3.6f, 3.6f, c);
}

static void draw_tear(float cx, float cy, uint32_t f)
{
    float drop = (f % 30) / 30.f;
    float ty = cy + drop * 34;
    float r = 4.5f + drop * 2.f;
    fd_fill_ellipse(cx, ty, r * 0.75f, r, C_TEAR);
    fd_fill_ellipse(cx - r * 0.25f, ty - r * 0.3f, r * 0.22f, r * 0.3f, C_WHITE);
}

/* ---------------------------------------------------- mouths */
static void mouth_smile(int dy, float size)
{
    float r = 20.f * size;
    fd_arc_band(FD_CX, 152 + dy, r - 3.2f, r, C_MOUTH_L, 1, 0.8f);
}

static void mouth_frown(int dy)
{
    fd_arc_band(FD_CX, 178 + dy, 15, 18.2f, C_MOUTH_L, 0, 0.8f);
}

static void mouth_pout(int dy, float size)
{
    fd_arc_band(FD_CX, 176 + dy, 7.f * size, 9.4f * size, C_MOUTH_L, 0, 0.68f);
}

static void mouth_open_smile(int dy, float size)
{
    float rx = 26.f * size, ry = 20.f * size;
    float my = 158 + dy;
    fd_half_disc(FD_CX, my - 2, rx + 2.4f, ry + 2.4f, C_MOUTH_R);
    fd_fill_rect((int)(FD_CX - rx) - 3, (int)my - 2, (int)(FD_CX + rx) + 4, (int)my, C_MOUTH_R);
    fd_half_disc(FD_CX, my, rx, ry, C_MOUTH_D);
    fd_fill_ellipse(FD_CX, my + ry * 0.78f, rx * 0.55f, ry * 0.42f, C_TONGUE);
}

static void mouth_o(int dy, float size)
{
    fd_fill_ellipse(FD_CX, 162 + dy, 9 * size + 2.2f, 11 * size + 2.2f, C_MOUTH_R);
    fd_fill_ellipse(FD_CX, 162 + dy, 9 * size, 11 * size, C_MOUTH_D);
}

static void mouth_flat(int dy, int hw)
{
    fd_thick_line(FD_CX - hw, 164 + dy, FD_CX + hw, 164 + dy, 2.6f, C_MOUTH_L);
}

static void mouth_wavy(int dy, uint32_t c)
{
    float px = 0, py = 0;
    int first = 1;
    for (int i = -18; i <= 18; i += 2) {
        float x = FD_CX + i;
        float y = 164 + dy + sinf(i * 0.55f) * 4;
        if (!first) {
            fd_thick_line(px, py, x, y, 2.2f, c);
        }
        px = x;
        py = y;
        first = 0;
    }
}

/* ---------------------------------------------------- animation helpers */
static float blink_amount(uint32_t f, int period)
{
    int ph = f % period;
    if (ph >= period - 6) {
        int k = ph - (period - 6);
        return 1.f - fabsf(k - 2.5f) / 2.5f * 0.999f;
    }
    return 0.f;
}

static int breathe(uint32_t f)
{
    return (int)lroundf(2.f * sinf(f * 2 * PI_F / LOOP));
}

/* ---------------------------------------------------- moods */
static void render_idle(uint32_t f)
{
    int dy = breathe(f);
    float bl = blink_amount(f, 56);
    float lx = roundf(3.5f * sinf(f * 0.18f));
    float ly = roundf(1.5f * sinf(f * 0.11f + 1.2f));
    draw_open_eye(EYE_LX, EYE_Y + dy, 1.f, lx, ly, bl, 0);
    draw_open_eye(EYE_RXC, EYE_Y + dy, 1.f, lx, ly, bl, 0);
    mouth_smile(dy + 8, 1.f);
}

static void render_happy(uint32_t f)
{
    int dy = (int)lroundf(2.5f * sinf(f * 2 * PI_F / 12));
    draw_happy_eye(EYE_LX, EYE_Y + dy);
    draw_happy_eye(EYE_RXC, EYE_Y + dy);
    mouth_open_smile(dy, 1.f + 0.05f * sinf(f * 0.5f));
    draw_blush(1);
}

static void render_sleepy(uint32_t f)
{
    int dy = breathe(f / 2);
    float droop = 0.62f + 0.05f * sinf(f * 0.15f);
    draw_open_eye(EYE_LX, EYE_Y + dy, 1.f, 0, 0, droop, 0);
    draw_open_eye(EYE_RXC, EYE_Y + dy, 1.f, 0, 0, droop, 0);
    mouth_o(dy + 4, 0.7f);
    draw_zzz(f);
}

static void render_listen(uint32_t f)
{
    float bl = blink_amount(f, 70);
    float puls = 1.06f + 0.03f * sinf(f * 0.6f);
    draw_open_eye(EYE_LX, EYE_Y, puls, 0, 0, bl, 0);
    draw_open_eye(EYE_RXC, EYE_Y, puls, 0, 0, bl, 0);
    mouth_o(0, 0.85f);
    float k = (f % 12) / 12.f;
    for (int i = 0; i < 2; ++i) {
        float r = 8 + i * 7 + k * 7;
        uint32_t col = fd_lerp888(0x50C8FF, C_BG, (i + k) / 3.f);
        fd_arc_band(26, 118, r - 2, r, col, 0, 0.6f);
        fd_arc_band(214, 118, r - 2, r, col, 1, 0.6f);
    }
}

static void render_speak(uint32_t f)
{
    float bl = blink_amount(f, 64);
    draw_open_eye(EYE_LX, EYE_Y, 1.f, 0, 0, bl, 0);
    draw_open_eye(EYE_RXC, EYE_Y, 1.f, 0, 0, bl, 0);
    float k = fabsf(sinf(f * 0.9f));
    float rx = 13 + 6 * k, ry = 5 + 12 * k;
    fd_fill_ellipse(FD_CX, 164, rx + 2.2f, ry + 2.2f, C_MOUTH_R);
    fd_fill_ellipse(FD_CX, 164, rx, ry, C_MOUTH_D);
    if (k > 0.55f) {
        fd_fill_ellipse(FD_CX, 168 + 4 * k, 8, 4.5f, C_TONGUE);
    }
}

static void render_think(uint32_t f)
{
    draw_open_eye(EYE_LX, EYE_Y, 1.f, 6, -8, 0, 0);
    draw_open_eye(EYE_RXC, EYE_Y, 1.f, 6, -8, 0, 0);
    mouth_flat(0, 11);
    int step = (f / 6) % 4;
    for (int i = 0; i < 3; ++i) {
        uint32_t col = (i < step) ? C_DOT_ON : C_DOT_OFF;
        fd_fill_ellipse(158 + i * 17, 52 - i * 9, 4.5f + i * 0.8f, 4.5f + i * 0.8f, col);
    }
}

static void render_alert(uint32_t f)
{
    float puls = 1.1f + 0.04f * sinf(f * 1.2f);
    draw_open_eye(EYE_LX, EYE_Y + 2, puls, 0, 0, 0, 0);
    draw_open_eye(EYE_RXC, EYE_Y + 2, puls, 0, 0, 0, 0);
    mouth_o(0, 1.f);
    if ((f / 4) % 2 == 0) {
        draw_excl(FD_CX, 40, C_WARN);
    }
}

static void render_panic(uint32_t f)
{
    int sh = ((f % 4) < 2) ? 2 : -2;
    draw_x_eye(EYE_LX + sh, EYE_Y, 1.f + 0.06f * sinf((float)f), C_DANGER);
    draw_x_eye(EYE_RXC + sh, EYE_Y, 1.f + 0.06f * cosf((float)f), C_DANGER);
    mouth_wavy(2, C_DANGER);
    if ((f / 3) % 2 == 0) {
        draw_excl(52, 52, C_DANGER);
        draw_excl(188, 52, C_DANGER);
    }
}

static void render_sad(uint32_t f)
{
    int dy = breathe(f / 2);
    draw_open_eye(EYE_LX, EYE_Y + dy, 1.f, 0, 5, 0.28f, -0.35f);
    draw_open_eye(EYE_RXC, EYE_Y + dy, 1.f, 0, 5, 0.28f, 0.35f);
    draw_brow(EYE_LX, EYE_Y + dy, -0.5f, C_BROW);
    draw_brow(EYE_RXC, EYE_Y + dy, 0.5f, C_BROW);
    mouth_frown(dy);
    draw_tear(EYE_LX + EYE_RX - 4, EYE_Y + 18 + dy, f);
}

static void render_angry(uint32_t f)
{
    int sh = ((f / 6) % 2 == 0) ? 1 : 0;
    draw_open_eye(EYE_LX, EYE_Y + sh, 1.f, 0, 0, 0.22f, 0.42f);
    draw_open_eye(EYE_RXC, EYE_Y + sh, 1.f, 0, 0, 0.22f, -0.42f);
    draw_brow(EYE_LX, EYE_Y, 0.55f, C_BROW_A);
    draw_brow(EYE_RXC, EYE_Y, -0.55f, C_BROW_A);
    mouth_flat(4, 17);
    if ((f / 5) % 2 == 0) {
        float mx = 176, my = 30;
        fd_thick_line(mx - 8, my - 8, mx - 2, my - 2, 2.2f, C_DANGER);
        fd_thick_line(mx + 8, my - 8, mx + 2, my - 2, 2.2f, C_DANGER);
        fd_thick_line(mx - 8, my + 8, mx - 2, my + 2, 2.2f, C_DANGER);
        fd_thick_line(mx + 8, my + 8, mx + 2, my + 2, 2.2f, C_DANGER);
    }
}

static void render_love(uint32_t f)
{
    float s = 1.f + 0.12f * sinf(f * 0.7f);
    draw_heart(EYE_LX, EYE_Y, s, C_HEART);
    draw_heart(EYE_RXC, EYE_Y, s, C_HEART);
    mouth_open_smile(0, 0.8f);
    draw_blush(1);
    float ph = (f % 24) / 24.f;
    float ph2 = fmodf(ph + 0.5f, 1.f);
    draw_heart(200, 70 - ph * 16, 0.35f, fd_lerp888(C_HEART, C_BG, ph));
    draw_heart(40, 80 - ph2 * 16, 0.3f, fd_lerp888(C_HEART, C_BG, ph2));
}

static void render_excited(uint32_t f)
{
    float s = 1.f + 0.15f * fabsf(sinf(f * 0.8f));
    draw_star(EYE_LX, EYE_Y, s, C_STAR);
    draw_star(EYE_RXC, EYE_Y, s, C_STAR);
    mouth_open_smile(0, 1.05f);
    draw_blush(0);
    static const int spark[3][2] = { {44, 60}, {196, 56}, {120, 34} };
    for (int i = 0; i < 3; ++i) {
        if (((f / 4) + i) % 3 != 0) {
            fd_thick_line(spark[i][0] - 5, spark[i][1], spark[i][0] + 5, spark[i][1], 1.6f, C_STAR);
            fd_thick_line(spark[i][0], spark[i][1] - 5, spark[i][0], spark[i][1] + 5, 1.6f, C_STAR);
        }
    }
}

static void render_dizzy(uint32_t f)
{
    float rot = f * 0.55f;
    draw_spiral_eye(EYE_LX, EYE_Y, rot, C_SPIRAL);
    draw_spiral_eye(EYE_RXC, EYE_Y, -rot, C_SPIRAL);
    mouth_wavy(4, C_MOUTH_L);
    draw_blush(0);
}

static void render_plead(uint32_t f)
{
    int dy = breathe(f / 2);
    float shimmer = 1.02f + 0.03f * sinf(f * 0.6f);
    draw_open_eye(EYE_LX, EYE_Y + dy, 1.08f * shimmer, 0, 8, 0.08f, 0);
    draw_open_eye(EYE_RXC, EYE_Y + dy, 1.08f * shimmer, 0, 8, 0.08f, 0);
    draw_brow(EYE_LX, EYE_Y + dy - 2, -0.42f, C_BROW);
    draw_brow(EYE_RXC, EYE_Y + dy - 2, 0.42f, C_BROW);
    fd_fill_ellipse(EYE_LX - 8, EYE_Y - 8 + dy, 10, 6, C_WHITE);
    fd_fill_ellipse(EYE_RXC - 8, EYE_Y - 8 + dy, 10, 6, C_WHITE);
    fd_fill_ellipse(EYE_LX + 14, EYE_Y + 18 + dy, 3.2f, 5.f, 0xAAE1FF);
    fd_fill_ellipse(EYE_RXC - 14, EYE_Y + 18 + dy, 3.2f, 5.f, 0xAAE1FF);
    mouth_pout(7, 0.95f + 0.03f * sinf(f * 0.5f));
    draw_blush(1);
}

void face_pixel_render(face_mood_t mood, uint32_t anim_frame, uint16_t *fb, int w, int h)
{
    if (!fb || w < FD_W || h < FD_H) {
        return;
    }
    fd_begin(fb, w);
    fd_fill_screen(C_BG);
    switch (mood) {
    case FACE_HAPPY:   render_happy(anim_frame); break;
    case FACE_SLEEPY:  render_sleepy(anim_frame); break;
    case FACE_LISTEN:  render_listen(anim_frame); break;
    case FACE_SPEAK:   render_speak(anim_frame); break;
    case FACE_THINK:   render_think(anim_frame); break;
    case FACE_ALERT:   render_alert(anim_frame); break;
    case FACE_PANIC:   render_panic(anim_frame); break;
    case FACE_SAD:     render_sad(anim_frame); break;
    case FACE_ANGRY:   render_angry(anim_frame); break;
    case FACE_LOVE:    render_love(anim_frame); break;
    case FACE_EXCITED: render_excited(anim_frame); break;
    case FACE_DIZZY:   render_dizzy(anim_frame); break;
    case FACE_PLEAD:   render_plead(anim_frame); break;
    case FACE_IDLE:
    default:           render_idle(anim_frame); break;
    }
    fd_clip_round();
}
