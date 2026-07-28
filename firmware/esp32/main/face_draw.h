#pragma once
#include <stdint.h>

/* Shared procedural drawing primitives for the GC9A01 framebuffer.
 * Used by face_pixel.c (anime face) and screen_info.c (info screens).
 * Colors are packed RGB888 (0xRRGGBB); converted to RGB565 on write.
 */

#define FD_W 240
#define FD_H 240
#define FD_CX 120
#define FD_CY 120
#define FD_SCREEN_R 120

void fd_begin(uint16_t *fb, int stride);
void fd_fill_screen(uint32_t c);
void fd_set_px(int x, int y, uint32_t c);
uint32_t fd_lerp888(uint32_t a, uint32_t b, float t);

void fd_fill_rect(int x0, int y0, int x1, int y1, uint32_t c);
void fd_fill_ellipse(float cx, float cy, float rx, float ry, uint32_t c);
void fd_crescent(float cx, float cy, float rx, float ry, float shift, uint32_t c);
/* half-plane arc: lower!=0 keeps y>=cy; span limits |dx| to r_out*span */
void fd_arc_band(float cx, float cy, float r_in, float r_out, uint32_t c,
                 int lower, float span);
/* angle-based ring segment (radians, from a0 sweeping to a1, y-down screen);
 * gradient c0 -> c1 along the sweep (pass c0==c1 for solid) */
void fd_ring_arc(float cx, float cy, float r_in, float r_out,
                 float a0, float a1, uint32_t c0, uint32_t c1);
void fd_circle_outline(float cx, float cy, float r, float t, uint32_t c);
void fd_thick_line(float x0, float y0, float x1, float y1, float t, uint32_t c);
void fd_half_disc(float cx, float my, float rx, float ry, uint32_t c);
void fd_clip_round(void);

/* 5x7 bitmap font, scaled. Supports 0-9 A-Z and " %:.-+/!".
 * '*' renders as the degree symbol. */
int fd_text_w(const char *s, int scale);
void fd_text(int x, int y, const char *s, int scale, uint32_t c);
void fd_text_center(int y, const char *s, int scale, uint32_t c);
