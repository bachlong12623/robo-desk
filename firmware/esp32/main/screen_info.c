/* Info screens for GC9A01 — mirrors tools/preview_screens.py. */
#include "screen_info.h"
#include "face_draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI_F 3.14159265f
#define DEG(x) ((x) * PI_F / 180.f)

#define C_BG      0x0A0C1Au
#define C_ACCENT  0x5ADCFFu
#define C_LABEL   0x5F82B4u
#define C_TEXT    0xE6F0FFu
#define C_GOOD    0x50E678u
#define C_WARN    0xFFC83Cu
#define C_BAD     0xFF505Au
#define C_RING_BG 0x1C2642u
#define C_HUM     0x50AAFFu
#define C_WHITE   0xFFFFFFu
#define C_COLD    0x5AAAFFu
#define C_HOT     0xFF7850u
#define C_MOON    0xF8E29Bu
#define C_MOON_SH 0x42507Du
#define C_NEWS    0xFFB25Au

static const char *SCREEN_NAMES[SCREEN_COUNT] = {
    "climate", "battery", "level", "radar", "clock", "status",
    "weather", "aqi", "news", "calendar", "desk", "morning", "moon",
};

info_screen_t screen_info_by_name(const char *name)
{
    for (int i = 0; i < SCREEN_COUNT; ++i) {
        if (strcmp(name, SCREEN_NAMES[i]) == 0) {
            return (info_screen_t)i;
        }
    }
    return SCREEN_COUNT;
}

static void screen_bg(const char *title)
{
    fd_fill_screen(C_BG);
    fd_text_center(26, title, 2, C_LABEL);
}

static void text_line_clamped(int x, int y, const char *src, int max_chars, int scale, uint32_t c)
{
    char buf[128];
    int n = 0;
    while (src && src[n] && n < max_chars && n < (int)sizeof(buf) - 4) {
        char ch = src[n];
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == ' ' || ch == '%' || ch == ':' || ch == '.' || ch == '-' ||
            ch == '+' || ch == '/' || ch == '!') {
            buf[n] = ch;
        } else {
            buf[n] = ' ';
        }
        n++;
    }
    if (src && src[n]) {
        if (n > 3) {
            buf[n - 3] = '.';
            buf[n - 2] = '.';
            buf[n - 1] = '.';
        }
    }
    buf[n] = '\0';
    fd_text(x, y, buf, scale, c);
}

static void text_center_clamped(int y, const char *src, int max_chars, int scale, uint32_t c)
{
    char buf[128];
    int n = 0;
    while (src && src[n] && n < max_chars && n < (int)sizeof(buf) - 4) {
        char ch = src[n];
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == ' ' || ch == '%' || ch == ':' || ch == '.' || ch == '-' ||
            ch == '+' || ch == '/' || ch == '!') {
            buf[n] = ch;
        } else {
            buf[n] = ' ';
        }
        n++;
    }
    if (src && src[n]) {
        if (n > 3) {
            buf[n - 3] = '.';
            buf[n - 2] = '.';
            buf[n - 1] = '.';
        }
    }
    buf[n] = '\0';
    fd_text_center(y, buf, scale, c);
}

static uint32_t aqi_color(int us)
{
    if (us <= 50) return C_GOOD;
    if (us <= 100) return C_WARN;
    if (us <= 150) return 0xFF9A38u;
    return C_BAD;
}

static void weather_icon(float cx, float cy, int code)
{
    if (code == 0) {
        fd_fill_ellipse(cx, cy, 20, 20, 0xFFD45Au);
        fd_fill_ellipse(cx - 6, cy - 6, 4, 4, C_WHITE);
        return;
    }
    if (code >= 60) {
        fd_fill_ellipse(cx - 10, cy, 18, 11, C_WHITE);
        fd_fill_ellipse(cx + 8, cy - 4, 15, 10, C_WHITE);
        fd_fill_ellipse(cx + 22, cy + 2, 13, 9, C_WHITE);
        for (int i = -14; i <= 16; i += 10) {
            fd_thick_line(cx + i, cy + 16, cx + i - 4, cy + 26, 1.6f, C_HUM);
        }
        return;
    }
    fd_fill_ellipse(cx - 10, cy, 18, 11, C_WHITE);
    fd_fill_ellipse(cx + 8, cy - 4, 15, 10, C_WHITE);
    fd_fill_ellipse(cx + 22, cy + 2, 13, 9, C_WHITE);
    fd_fill_ellipse(cx - 26, cy - 12, 11, 11, 0xFFD45Au);
}

static void moon_icon(float cx, float cy, int phase_pct)
{
    fd_fill_ellipse(cx, cy, 36, 36, C_MOON);
    float shift = (phase_pct <= 50)
        ? (1.f - phase_pct / 50.f) * 36.f
        : ((phase_pct - 50.f) / 50.f) * 36.f;
    if (phase_pct <= 50) {
        fd_fill_ellipse(cx + shift, cy, 36, 36, C_MOON_SH);
    } else {
        fd_fill_ellipse(cx - shift, cy, 36, 36, C_MOON_SH);
    }
}

/* ------------------------------------------------------------- climate */
static uint32_t temp_color(float t)
{
    if (t < 18.f) return C_COLD;
    if (t < 27.f) return C_GOOD;
    if (t < 32.f) return C_WARN;
    return C_BAD;
}

static void render_climate(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("CLIMATE");
    float ga0 = DEG(135);
    float frac = d->temp_c / 40.f;
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    fd_ring_arc(FD_CX, FD_CY, 104, 114, ga0, ga0 + DEG(270), C_RING_BG, C_RING_BG);
    fd_ring_arc(FD_CX, FD_CY, 104, 114, ga0, ga0 + DEG(270) * frac, C_COLD, C_HOT);
    uint32_t tcol = temp_color(d->temp_c);
    char ts[16];
    snprintf(ts, sizeof(ts), "%.1f", (double)d->temp_c);
    int tw = fd_text_w(ts, 5) + 10 + fd_text_w("*C", 2);
    fd_text(FD_CX - tw / 2, 62, ts, 5, tcol);
    fd_text(FD_CX - tw / 2 + fd_text_w(ts, 5) + 10, 62, "*C", 2, tcol);
    char hs[8];
    snprintf(hs, sizeof(hs), "%.0f%%", (double)d->rh);
    int hw = 18 + fd_text_w(hs, 4);
    int hx = FD_CX - hw / 2;
    fd_fill_ellipse(hx + 6, 138, 7, 9, C_HUM);
    for (int i = 0; i < 10; ++i) {
        float half = i * 0.62f;
        fd_fill_rect((int)(hx + 6 - half), 122 + i, (int)(hx + 6 + half) + 1,
                     123 + i, C_HUM);
    }
    fd_text(hx + 18, 122, hs, 4, C_HUM);
    int comf = d->temp_c >= 20.f && d->temp_c <= 28.f &&
               d->rh >= 35.f && d->rh <= 65.f;
    uint32_t fc = comf ? C_GOOD : C_WARN;
    int fy = 186;
    fd_circle_outline(FD_CX, fy, 14, 2.4f, fc);
    fd_fill_ellipse(FD_CX - 5, fy - 3, 1.8f, 2.4f, fc);
    fd_fill_ellipse(FD_CX + 5, fy - 3, 1.8f, 2.4f, fc);
    if (comf) {
        fd_arc_band(FD_CX, fy + 2, 5.5f, 7.8f, fc, 1, 0.8f);
    } else {
        fd_thick_line(FD_CX - 5, fy + 6, FD_CX + 5, fy + 6, 1.2f, fc);
    }
}

/* ------------------------------------------------------------- battery */
static void render_battery(const screen_info_data_t *d, uint32_t f)
{
    fd_fill_screen(C_BG);
    float ga0 = DEG(135);
    fd_ring_arc(FD_CX, FD_CY, 96, 112, ga0, ga0 + DEG(270), C_RING_BG, C_RING_BG);
    uint32_t col = d->batt_pct > 40.f ? C_GOOD : (d->batt_pct > 15.f ? C_WARN : C_BAD);
    fd_ring_arc(FD_CX, FD_CY, 96, 112, ga0,
                ga0 + DEG(270) * d->batt_pct / 100.f, col, col);
    fd_text_center(34, "BATTERY", 2, C_LABEL);
    char s[8];
    snprintf(s, sizeof(s), "%.0f%%", (double)d->batt_pct);
    fd_text_center(92, s, 6, C_TEXT);
    snprintf(s, sizeof(s), "%.2fV", (double)d->batt_v);
    fd_text_center(150, s, 3, C_LABEL);
    if (d->batt_low && (f / 6) % 2 == 0) {
        fd_text_center(190, "LOW!", 3, C_BAD);
    }
}

/* ------------------------------------------------------------- level */
static void render_level(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("LEVEL");
    int cy = 118;
    fd_circle_outline(FD_CX, cy, 22, 1.6f, C_RING_BG);
    fd_circle_outline(FD_CX, cy, 46, 1.6f, C_RING_BG);
    fd_circle_outline(FD_CX, cy, 70, 1.6f, C_RING_BG);
    fd_thick_line(FD_CX - 74, cy, FD_CX + 74, cy, 0.8f, C_RING_BG);
    fd_thick_line(FD_CX, cy - 74, FD_CX, cy + 74, 0.8f, C_RING_BG);
    int flat = fabsf(d->pitch_deg) < 2.f && fabsf(d->roll_deg) < 2.f;
    float bx = d->roll_deg * 3.2f;
    float by = d->pitch_deg * 3.2f;
    if (bx > 64.f) bx = 64.f;
    if (bx < -64.f) bx = -64.f;
    if (by > 64.f) by = 64.f;
    if (by < -64.f) by = -64.f;
    uint32_t bcol = flat ? C_GOOD : C_ACCENT;
    fd_fill_ellipse(FD_CX + bx, cy + by, 9, 9, bcol);
    fd_fill_ellipse(FD_CX + bx - 3, cy + by - 3, 2.6f, 2.6f, C_WHITE);
    char s[20];
    snprintf(s, sizeof(s), "P%+03d R%+03d",
             (int)lroundf(d->pitch_deg), (int)lroundf(d->roll_deg));
    fd_text_center(204, s, 2, flat ? C_GOOD : C_LABEL);
}

/* ------------------------------------------------------------- radar */
static void render_radar(const screen_info_data_t *d, uint32_t f)
{
    screen_bg("RADAR");
    int cy = 108;
    const float R = 62.f;
    fd_circle_outline(FD_CX, cy, R / 3, 1.4f, C_RING_BG);
    fd_circle_outline(FD_CX, cy, 2 * R / 3, 1.4f, C_RING_BG);
    fd_circle_outline(FD_CX, cy, R, 1.4f, C_RING_BG);
    float rot = f * 0.22f;
    for (int i = 5; i >= 1; --i) {
        float a = rot - i * 0.13f;
        uint32_t col = fd_lerp888(0x50E68C, C_BG, i / 5.f);
        fd_thick_line(FD_CX, cy, FD_CX + R * cosf(a), cy + R * sinf(a),
                      1.6f - i * 0.2f, col);
    }
    fd_thick_line(FD_CX, cy, FD_CX + R * cosf(rot), cy + R * sinf(rot),
                  2.f, 0xAAFFC8);
    float dn = d->tof_mm / 1200.f;
    if (dn > 1.f) dn = 1.f;
    float br = 10 + dn * (R - 14);
    float ba = DEG(-55);
    float pul = 1.f + 0.4f * sinf(f * 0.9f);
    fd_fill_ellipse(FD_CX + br * cosf(ba), cy + br * sinf(ba),
                    3.2f * pul, 3.2f * pul, 0xFF785A);
    char s[12];
    snprintf(s, sizeof(s), "%u CM", (unsigned)(d->tof_mm / 10));
    fd_text_center(186, s, 3, C_TEXT);
    if (d->presence && (f / 5) % 2 == 0) {
        fd_text_center(214, "MOTION", 2, C_BAD);
    }
}

/* ------------------------------------------------------------- clock */
static void render_clock(const screen_info_data_t *d, uint32_t f)
{
    fd_fill_screen(C_BG);
    char s[16];
    snprintf(s, sizeof(s), "%02d%c%02d", d->hh, ((f / 6) % 2 == 0) ? ':' : ' ',
             d->mm);
    fd_text_center(88, s, 6, C_TEXT);
    static const char *DOW[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    snprintf(s, sizeof(s), "%s %02d/%02d", DOW[d->dow % 7], d->day, d->mon);
    fd_text_center(146, s, 2, C_LABEL);
    snprintf(s, sizeof(s), "%.0f*C %.0f%%", (double)d->temp_c, (double)d->rh);
    fd_text_center(176, s, 2, 0x466496);
    for (int i = 0; i < 60; ++i) {
        float a = DEG(i * 6 - 90);
        float sz = (i % 5 == 0) ? 2.2f : 1.2f;
        uint32_t col = (i <= d->ss) ? C_ACCENT : C_RING_BG;
        fd_fill_ellipse(FD_CX + 112 * cosf(a), FD_CY + 112 * sinf(a), sz, sz, col);
    }
}

/* ------------------------------------------------------------- status */
static void render_status(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("STATUS");
    fd_text_center(66, d->linked ? "LINKED" : "SOLO", 4,
                   d->linked ? C_GOOD : C_ACCENT);
    int wy = 132, wx = FD_CX - 40;
    fd_fill_ellipse(wx, wy, 2.6f, 2.6f, C_ACCENT);
    int bars = d->rssi == 0 ? 0 : (d->rssi > -60 ? 3 : (d->rssi > -75 ? 2 : 1));
    for (int i = 0; i < 3; ++i) {
        uint32_t col = (i < bars) ? C_ACCENT : C_RING_BG;
        fd_ring_arc(wx, wy, 6 + i * 7, 9 + i * 7, DEG(215), DEG(325), col, col);
    }
    char s[16];
    snprintf(s, sizeof(s), "%d", d->rssi);
    fd_text(wx + 32, wy - 10, s, 2, C_TEXT);
    fd_text_center(162, d->linked ? "MQTT OK" : "MQTT --", 2,
                   d->linked ? C_GOOD : C_LABEL);
    unsigned up_min = d->uptime_s / 60;
    snprintf(s, sizeof(s), "UP %02u:%02u", up_min / 60, up_min % 60);
    fd_text_center(190, s, 2, C_LABEL);
}

/* ------------------------------------------------------------- weather */
static void render_weather(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("WEATHER");
    weather_icon(56, 94, d->forecast_weather_code);
    text_center_clamped(56, d->forecast_label[0] ? d->forecast_label : "TODAY", 12, 2, C_LABEL);
    char s[24];
    snprintf(s, sizeof(s), "%.0f*C", (double)d->forecast_temp_c);
    fd_text(102, 78, s, 4, C_TEXT);
    snprintf(s, sizeof(s), "%.0f* %.0f*", (double)d->forecast_min_c, (double)d->forecast_max_c);
    fd_text(104, 118, s, 2, C_LABEL);
    fd_ring_arc(FD_CX, 176, 30, 40, DEG(180), DEG(180) + DEG(180) * d->forecast_rain_pct / 100.f, C_HUM, C_HUM);
    fd_ring_arc(FD_CX, 176, 30, 40, DEG(180), DEG(360), C_RING_BG, C_RING_BG);
    snprintf(s, sizeof(s), "%d%%", d->forecast_rain_pct);
    fd_text_center(166, "RAIN", 2, C_LABEL);
    fd_text_center(182, s, 3, C_HUM);
}

/* ------------------------------------------------------------- aqi */
static void render_aqi(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("AQI");
    uint32_t col = aqi_color(d->aqi_us);
    fd_ring_arc(FD_CX, FD_CY, 98, 112, DEG(135), DEG(405), C_RING_BG, C_RING_BG);
    int clamped = d->aqi_us;
    if (clamped < 0) clamped = 0;
    if (clamped > 200) clamped = 200;
    fd_ring_arc(FD_CX, FD_CY, 98, 112, DEG(135), DEG(135) + DEG(270) * clamped / 200.f, col, col);
    char s[16];
    snprintf(s, sizeof(s), "%d", d->aqi_us);
    fd_text_center(84, s, 6, col);
    text_center_clamped(144, d->aqi_label[0] ? d->aqi_label : "PM25", 14, 2, C_LABEL);
    snprintf(s, sizeof(s), "%.1f", (double)d->aqi_pm25);
    fd_text_center(174, s, 3, C_TEXT);
    fd_text_center(200, "UG M3", 2, C_LABEL);
}

/* ------------------------------------------------------------- news */
static void render_news(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("NEWS");
    fd_fill_ellipse(38, 64, 10, 10, C_NEWS);
    fd_fill_rect(48, 56, 92, 72, C_NEWS);
    text_line_clamped(58, 58, "NEWS", 8, 2, C_BG);
    text_line_clamped(20, 92, d->news_title[0] ? d->news_title : "NO HEADLINE", 18, 2, C_TEXT);
    text_line_clamped(20, 122, d->news_title[0] ? d->news_title + ((strlen(d->news_title) > 18) ? 18 : strlen(d->news_title)) : "", 18, 2, C_TEXT);
    fd_thick_line(40, 168, 200, 168, 1.2f, C_RING_BG);
    fd_text_center(186, "RSS VIA PI5", 2, C_LABEL);
}

/* ------------------------------------------------------------- calendar */
static void render_calendar(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("CALENDAR");
    fd_fill_rect(52, 52, 188, 164, 0x111935u);
    fd_fill_rect(52, 52, 188, 76, 0xE05C6Cu);
    fd_fill_ellipse(76, 52, 6, 6, C_BG);
    fd_fill_ellipse(164, 52, 6, 6, C_BG);
    if (d->calendar_has) {
        char s[16];
        snprintf(s, sizeof(s), "%02d:%02d", d->calendar_hh, d->calendar_mm);
        fd_text_center(88, s, 4, C_TEXT);
        text_center_clamped(126, d->calendar_title, 18, 2, C_TEXT);
        snprintf(s, sizeof(s), "IN %d MIN", d->calendar_in_min);
        fd_text_center(188, s, 2, d->calendar_in_min <= 30 ? C_WARN : C_LABEL);
    } else {
        fd_text_center(96, "FREE", 4, C_GOOD);
        fd_text_center(136, "NO EVENT", 2, C_LABEL);
    }
}

/* ------------------------------------------------------------- desk */
static void render_desk(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("DESK");
    fd_ring_arc(FD_CX, FD_CY, 94, 108, DEG(135), DEG(405), C_RING_BG, C_RING_BG);
    int today = d->desk_today_min;
    if (today < 0) today = 0;
    if (today > 480) today = 480;
    fd_ring_arc(FD_CX, FD_CY, 94, 108, DEG(135), DEG(135) + DEG(270) * today / 480.f, C_ACCENT, C_GOOD);
    char s[16];
    snprintf(s, sizeof(s), "%dM", d->desk_today_min);
    fd_text_center(84, s, 5, C_TEXT);
    snprintf(s, sizeof(s), "NOW %dM", d->desk_session_min);
    fd_text_center(146, s, 2, C_LABEL);
    fd_text_center(176, d->desk_break_due ? "TAKE BREAK" : "ON TRACK", 2, d->desk_break_due ? C_WARN : C_GOOD);
}

/* ------------------------------------------------------------- morning */
static void render_morning(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("MORNING");
    fd_fill_ellipse(58, 70, 18, 18, 0xFFD45Au);
    for (int i = 0; i < 8; ++i) {
        float a = DEG(i * 45);
        fd_thick_line(58 + 24 * cosf(a), 70 + 24 * sinf(a), 58 + 34 * cosf(a), 70 + 34 * sinf(a), 1.5f, 0xFFD45Au);
    }
    text_center_clamped(70, d->morning_line1[0] ? d->morning_line1 : "GOOD MORNING", 18, 2, C_TEXT);
    text_center_clamped(100, d->morning_line2[0] ? d->morning_line2 : "READY TO WORK", 18, 2, C_LABEL);
    char s[24];
    snprintf(s, sizeof(s), "%.0f* %dAQI", (double)d->forecast_temp_c, d->aqi_us);
    fd_text_center(150, s, 2, C_ACCENT);
    snprintf(s, sizeof(s), "%02d:%02d", d->calendar_hh, d->calendar_mm);
    fd_text_center(180, d->calendar_has ? s : "NO EVENT", 3, d->calendar_has ? C_WARN : C_GOOD);
}

/* ------------------------------------------------------------- moon */
static void render_moon(const screen_info_data_t *d, uint32_t f)
{
    (void)f;
    screen_bg("MOON");
    moon_icon(120, 102, d->moon_phase_pct);
    text_center_clamped(154, d->moon_label[0] ? d->moon_label : "PHASE", 14, 2, C_TEXT);
    char s[20];
    snprintf(s, sizeof(s), "%d%% LIT", d->moon_illum_pct);
    fd_text_center(182, s, 2, C_LABEL);
    for (int i = 0; i < 5; ++i) {
        float x = 38 + i * 40 + ((i & 1) ? 6 : 0);
        float y = 58 + ((i & 1) ? 8 : 0);
        fd_fill_ellipse(x, y, 1.6f + (i == ((int)f / 6) % 5), 1.6f + (i == ((int)f / 6) % 5), C_WHITE);
    }
}

void screen_info_render(info_screen_t screen, const screen_info_data_t *d,
                        uint32_t frame, uint16_t *fb, int w, int h)
{
    if (!fb || !d || w < FD_W || h < FD_H) {
        return;
    }
    fd_begin(fb, w);
    switch (screen) {
    case SCREEN_BATTERY: render_battery(d, frame); break;
    case SCREEN_LEVEL:   render_level(d, frame); break;
    case SCREEN_RADAR:   render_radar(d, frame); break;
    case SCREEN_CLOCK:   render_clock(d, frame); break;
    case SCREEN_STATUS:  render_status(d, frame); break;
    case SCREEN_WEATHER: render_weather(d, frame); break;
    case SCREEN_AQI:     render_aqi(d, frame); break;
    case SCREEN_NEWS:    render_news(d, frame); break;
    case SCREEN_CALENDAR: render_calendar(d, frame); break;
    case SCREEN_DESK:    render_desk(d, frame); break;
    case SCREEN_MORNING: render_morning(d, frame); break;
    case SCREEN_MOON:    render_moon(d, frame); break;
    case SCREEN_CLIMATE:
    default:             render_climate(d, frame); break;
    }
    fd_clip_round();
}
