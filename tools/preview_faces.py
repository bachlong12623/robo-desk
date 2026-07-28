#!/usr/bin/env python3
"""RoboDesk anime face renderer & previewer.

Renders 240x240 anime-style faces (same logic as firmware face_pixel.c).
Export:
  python tools/preview_faces.py --sheet sheet.png        # contact sheet PNG
  python tools/preview_faces.py --html faces.html        # animated HTML preview
  python tools/preview_faces.py --png out_dir            # per-frame PNGs
  python tools/preview_faces.py --mood happy             # quick terminal view
"""

from __future__ import annotations

import argparse
import base64
import math
import os
import struct
import sys
import zlib

W, H = 240, 240
CX, CY = W // 2, H // 2
SCREEN_R = 120  # round GC9A01

# ---------------------------------------------------------------- palette
BG = (10, 12, 26)
EYE_TOP = (150, 240, 255)
EYE_BOT = (35, 115, 235)
EYE_RIM = (18, 60, 140)
PUPIL = (10, 40, 105)
WHITE = (255, 255, 255)
MOUTH_LINE = (235, 245, 255)
MOUTH_DARK = (105, 26, 48)
MOUTH_RIM = (225, 120, 140)
TONGUE = (240, 110, 130)
BLUSH = (255, 120, 150)
WARN = (255, 205, 60)
DANGER = (255, 70, 80)
HEART = (255, 90, 130)
HEART_HI = (255, 170, 200)
STAR = (255, 220, 90)
ZZZ = (150, 205, 255)
BROW = (30, 80, 170)

MOODS = [
    "idle", "happy", "sleepy", "listen", "speak", "think",
    "alert", "panic", "sad", "angry", "love", "excited", "dizzy", "plead",
]

FPS = 12
LOOP = 48  # frames per loop


def lerp(a: tuple, b: tuple, t: float) -> tuple:
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Canvas:
    def __init__(self) -> None:
        self.px = bytearray(W * H * 3)
        self.fill(BG)

    def fill(self, c: tuple) -> None:
        r, g, b = c
        self.px[0::3] = bytes([r]) * (W * H)
        self.px[1::3] = bytes([g]) * (W * H)
        self.px[2::3] = bytes([b]) * (W * H)

    def set(self, x: int, y: int, c: tuple) -> None:
        if 0 <= x < W and 0 <= y < H:
            i = (y * W + x) * 3
            self.px[i:i + 3] = bytes(c)

    # ---- shape primitives ----
    def ellipse(self, cx: float, cy: float, rx: float, ry: float, color,
                rim: tuple | None = None, rim_at: float = 0.82) -> None:
        """color: tuple or fn(nx, ny)->tuple where nx,ny in [-1,1]."""
        x0, x1 = int(cx - rx), int(cx + rx) + 1
        y0, y1 = int(cy - ry), int(cy + ry) + 1
        for y in range(max(0, y0), min(H, y1)):
            ny = (y - cy) / ry
            for x in range(max(0, x0), min(W, x1)):
                nx = (x - cx) / rx
                d = nx * nx + ny * ny
                if d <= 1.0:
                    if rim is not None and d >= rim_at:
                        self.set(x, y, rim)
                    else:
                        c = color(nx, ny) if callable(color) else color
                        self.set(x, y, c)

    def crescent(self, cx: float, cy: float, rx: float, ry: float,
                 shift: float, color: tuple) -> None:
        """Upper crescent: ellipse minus same ellipse shifted down."""
        x0, x1 = int(cx - rx), int(cx + rx) + 1
        y0, y1 = int(cy - ry), int(cy + ry) + 1
        for y in range(max(0, y0), min(H, y1)):
            for x in range(max(0, x0), min(W, x1)):
                nx = (x - cx) / rx
                ny = (y - cy) / ry
                ny2 = (y - (cy + shift)) / ry
                if nx * nx + ny * ny <= 1.0 and nx * nx + ny2 * ny2 > 1.0:
                    self.set(x, y, color)

    def arc_band(self, cx: float, cy: float, r_in: float, r_out: float,
                 color: tuple, lower: bool = True, span: float = 0.85) -> None:
        """Smile/frown arc: ring segment. span limits x extent (0..1)."""
        x0, x1 = int(cx - r_out), int(cx + r_out) + 1
        y0, y1 = int(cy - r_out), int(cy + r_out) + 1
        for y in range(max(0, y0), min(H, y1)):
            dy = y - cy
            if lower and dy < 0:
                continue
            if not lower and dy > 0:
                continue
            for x in range(max(0, x0), min(W, x1)):
                dx = x - cx
                if abs(dx) > r_out * span:
                    continue
                d = math.sqrt(dx * dx + dy * dy)
                if r_in <= d <= r_out:
                    self.set(x, y, color)

    def rect(self, x0: int, y0: int, x1: int, y1: int, c: tuple) -> None:
        for y in range(max(0, y0), min(H, y1)):
            for x in range(max(0, x0), min(W, x1)):
                self.set(x, y, c)

    def line(self, x0: float, y0: float, x1: float, y1: float,
             t: float, c: tuple) -> None:
        steps = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(steps + 1):
            f = i / steps
            x = x0 + (x1 - x0) * f
            y = y0 + (y1 - y0) * f
            self.ellipse(x, y, t, t, c)

    def clip_round(self) -> None:
        for y in range(H):
            dy = y - CY
            for x in range(W):
                dx = x - CX
                if dx * dx + dy * dy > SCREEN_R * SCREEN_R:
                    i = (y * W + x) * 3
                    self.px[i:i + 3] = b"\x00\x00\x00"


# ---------------------------------------------------------------- eye parts
EYE_LX, EYE_RX_ = 76, 164
EYE_Y = 104
EYE_RX = 27
EYE_RY = 38


def eye_gradient(nx: float, ny: float) -> tuple:
    t = (ny + 1.0) / 2.0
    return lerp(EYE_TOP, EYE_BOT, t)


def draw_open_eye(c: Canvas, cx: float, cy: float, scale: float = 1.0,
                  look: tuple = (0, 0), blink: float = 0.0,
                  lid_slant: float = 0.0, color_fn=eye_gradient,
                  rim: tuple = EYE_RIM) -> None:
    """Anime glossy eye. blink 0=open 1=closed. lid_slant >0 angry, <0 sad."""
    rx = EYE_RX * scale
    ry = EYE_RY * scale
    if blink >= 0.97:
        c.line(cx - rx, cy, cx + rx, cy, 3.4, rim)
        return
    c.ellipse(cx, cy, rx, ry, color_fn, rim=rim)
    lx, ly = look
    c.ellipse(cx + lx, cy + 6 + ly, rx * 0.52, ry * 0.42, PUPIL)
    c.ellipse(cx + lx * 0.6 - rx * 0.32, cy + ly * 0.6 - ry * 0.38,
              rx * 0.30, ry * 0.20, WHITE)
    c.ellipse(cx + lx * 0.6 + rx * 0.28, cy + ly * 0.6 + ry * 0.30,
              rx * 0.14, ry * 0.09, WHITE)
    # eyelid: cover from top, optionally slanted
    if blink > 0.0 or lid_slant != 0.0:
        base = cy - ry + blink * 2.0 * ry
        x0, x1 = int(cx - rx) - 2, int(cx + rx) + 3
        for x in range(max(0, x0), min(W, x1)):
            lid_y = base + lid_slant * (x - cx)
            for y in range(max(0, int(cy - ry) - 2), min(H, int(lid_y) + 1)):
                dx = (x - cx) / (rx + 2)
                dy = (y - cy) / (ry + 2)
                if dx * dx + dy * dy <= 1.0:
                    c.set(x, y, BG)
        # lid edge line
        c.line(max(cx - rx, 0), base - lid_slant * rx,
               cx + rx, base + lid_slant * rx, 2.2, rim)


def draw_happy_eye(c: Canvas, cx: float, cy: float) -> None:
    c.crescent(cx, cy, EYE_RX * 0.95, EYE_RY * 0.55, 11, eye_avg())


_eye_avg_cache: tuple | None = None


def eye_avg() -> tuple:
    global _eye_avg_cache
    if _eye_avg_cache is None:
        _eye_avg_cache = lerp(EYE_TOP, EYE_BOT, 0.35)
    return _eye_avg_cache


def draw_heart(c: Canvas, cx: float, cy: float, s: float, color: tuple) -> None:
    r = 11 * s
    c.ellipse(cx - r * 0.85, cy - r * 0.5, r, r, color)
    c.ellipse(cx + r * 0.85, cy - r * 0.5, r, r, color)
    # triangle bottom
    h = 2.1 * r
    for i in range(int(h)):
        f = i / h
        half = (1.0 - f) * 1.8 * r
        y = cy - r * 0.1 + i
        c.rect(int(cx - half), int(y), int(cx + half) + 1, int(y) + 1, color)
    c.ellipse(cx - r * 0.9, cy - r * 0.7, r * 0.32, r * 0.32, HEART_HI)


def draw_star(c: Canvas, cx: float, cy: float, s: float, color: tuple) -> None:
    a, b = 21 * s, 21 * s
    for y in range(int(cy - b), int(cy + b) + 1):
        for x in range(int(cx - a), int(cx + a) + 1):
            nx = abs(x - cx) / a
            ny = abs(y - cy) / b
            # 4-point star shape
            if math.sqrt(nx) + math.sqrt(ny) <= 1.0:
                c.set(x, y, color)
    c.ellipse(cx - a * 0.25, cy - b * 0.25, 2.6 * s, 2.6 * s, WHITE)


def draw_x_eye(c: Canvas, cx: float, cy: float, s: float, color: tuple) -> None:
    r = 16 * s
    c.line(cx - r, cy - r, cx + r, cy + r, 4.2, color)
    c.line(cx - r, cy + r, cx + r, cy - r, 4.2, color)


def draw_spiral_eye(c: Canvas, cx: float, cy: float, rot: float,
                    color: tuple) -> None:
    rmax = 19.0
    x0, x1 = int(cx - rmax), int(cx + rmax) + 1
    y0, y1 = int(cy - rmax), int(cy + rmax) + 1
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            dx, dy = x - cx, y - cy
            d = math.sqrt(dx * dx + dy * dy)
            if d > rmax or d < 2.0:
                continue
            ang = math.atan2(dy, dx) + rot
            arm = (ang / (2 * math.pi) * 2.4 + d / rmax * 2.6) % 1.0
            if arm < 0.42:
                c.set(x, y, color)


def draw_brow(c: Canvas, cx: float, cy: float, slant: float,
              color: tuple = BROW) -> None:
    y0 = cy - EYE_RY - 12
    c.line(cx - EYE_RX * 0.9, y0 - slant * 9,
           cx + EYE_RX * 0.9, y0 + slant * 9, 3.6, color)


def draw_blush(c: Canvas, strong: bool = False) -> None:
    ry = 7 if strong else 5
    c.ellipse(50, 148, 15, ry, BLUSH)
    c.ellipse(190, 148, 15, ry, BLUSH)


def draw_zzz(c: Canvas, frame: int) -> None:
    ph = (frame % 36) / 36.0
    for i, s in enumerate((1.0, 0.75, 0.5)):
        yy = 62 - i * 20 - ph * 8
        xx = 168 + i * 14 + math.sin(ph * 6.28 + i) * 3
        r = 8 * s
        col = ZZZ
        c.line(xx - r, yy - r, xx + r, yy - r, 1.8 * s + 0.6, col)
        c.line(xx + r, yy - r, xx - r, yy + r, 1.8 * s + 0.6, col)
        c.line(xx - r, yy + r, xx + r, yy + r, 1.8 * s + 0.6, col)


def draw_excl(c: Canvas, cx: float, cy: float, color: tuple) -> None:
    c.rect(int(cx - 3), int(cy - 16), int(cx + 4), int(cy + 4), color)
    c.ellipse(cx, cy + 12, 3.6, 3.6, color)


def draw_tear(c: Canvas, cx: float, cy: float, frame: int) -> None:
    drop = (frame % 30) / 30.0
    ty = cy + drop * 34
    r = 4.5 + drop * 2.0
    c.ellipse(cx, ty, r * 0.75, r, (120, 200, 255))
    c.ellipse(cx - r * 0.25, ty - r * 0.3, r * 0.22, r * 0.3, WHITE)


# ---------------------------------------------------------------- mouths
def mouth_smile(c: Canvas, dy: int = 0, size: float = 1.0) -> None:
    r = 20 * size
    c.arc_band(CX, 152 + dy, r - 3.2, r, MOUTH_LINE, lower=True, span=0.8)


def mouth_frown(c: Canvas, dy: int = 0) -> None:
    c.arc_band(CX, 178 + dy, 15, 18.2, MOUTH_LINE, lower=False, span=0.8)


def mouth_pout(c: Canvas, dy: int = 0, size: float = 1.0) -> None:
    c.arc_band(CX, 176 + dy, 7.0 * size, 9.4 * size, MOUTH_LINE,
               lower=False, span=0.68)


def _half_disc(c: Canvas, cx: float, my: float, rx: float, ry: float,
               color: tuple) -> None:
    x0, x1 = int(cx - rx), int(cx + rx) + 1
    for y in range(int(my), int(my + ry) + 1):
        ny = (y - my) / ry
        for x in range(max(0, x0), min(W, x1)):
            nx = (x - cx) / rx
            if nx * nx + ny * ny <= 1.0:
                c.set(x, y, color)


def mouth_open_smile(c: Canvas, dy: int = 0, size: float = 1.0) -> None:
    rx, ry = 26 * size, 20 * size
    my = 158 + dy
    _half_disc(c, CX, my - 2, rx + 2.4, ry + 2.4, MOUTH_RIM)
    c.rect(int(CX - rx - 3), int(my - 2), int(CX + rx + 4), int(my), MOUTH_RIM)
    _half_disc(c, CX, my, rx, ry, MOUTH_DARK)
    c.ellipse(CX, my + ry * 0.78, rx * 0.55, ry * 0.42, TONGUE)


def mouth_o(c: Canvas, dy: int = 0, size: float = 1.0) -> None:
    c.ellipse(CX, 162 + dy, 9 * size + 2.2, 11 * size + 2.2, MOUTH_RIM)
    c.ellipse(CX, 162 + dy, 9 * size, 11 * size, MOUTH_DARK)


def mouth_flat(c: Canvas, dy: int = 0, hw: int = 15) -> None:
    c.line(CX - hw, 164 + dy, CX + hw, 164 + dy, 2.6, MOUTH_LINE)


def mouth_wavy(c: Canvas, dy: int = 0, color: tuple = MOUTH_LINE) -> None:
    prev = None
    for i in range(-18, 19, 2):
        x = CX + i
        y = 164 + dy + math.sin(i * 0.55) * 4
        if prev:
            c.line(prev[0], prev[1], x, y, 2.2, color)
        prev = (x, y)


# ---------------------------------------------------------------- moods
def blink_amount(frame: int, period: int = 56) -> float:
    ph = frame % period
    if ph >= period - 6:
        k = ph - (period - 6)  # 0..5
        return 1.0 - abs(k - 2.5) / 2.5 * 0.999
    return 0.0


def breathe(frame: int) -> int:
    return round(2.0 * math.sin(frame * 2 * math.pi / LOOP))


def render_idle(c: Canvas, f: int) -> None:
    dy = breathe(f)
    bl = blink_amount(f)
    lx = round(3.5 * math.sin(f * 0.18))
    ly = round(1.5 * math.sin(f * 0.11 + 1.2))
    draw_open_eye(c, EYE_LX, EYE_Y + dy, look=(lx, ly), blink=bl)
    draw_open_eye(c, EYE_RX_, EYE_Y + dy, look=(lx, ly), blink=bl)
    mouth_smile(c, dy=dy + 8)


def render_happy(c: Canvas, f: int) -> None:
    dy = round(2.5 * math.sin(f * 2 * math.pi / 12))  # bounce
    draw_happy_eye(c, EYE_LX, EYE_Y + dy)
    draw_happy_eye(c, EYE_RX_, EYE_Y + dy)
    mouth_open_smile(c, dy=dy, size=1.0 + 0.05 * math.sin(f * 0.5))
    draw_blush(c, strong=True)


def render_sleepy(c: Canvas, f: int) -> None:
    dy = breathe(f // 2)
    droop = 0.62 + 0.05 * math.sin(f * 0.15)
    draw_open_eye(c, EYE_LX, EYE_Y + dy, blink=droop)
    draw_open_eye(c, EYE_RX_, EYE_Y + dy, blink=droop)
    mouth_o(c, dy=dy + 4, size=0.7)
    draw_zzz(c, f)


def render_listen(c: Canvas, f: int) -> None:
    bl = blink_amount(f, 70)
    puls = 1.06 + 0.03 * math.sin(f * 0.6)
    draw_open_eye(c, EYE_LX, EYE_Y, scale=puls, blink=bl)
    draw_open_eye(c, EYE_RX_, EYE_Y, scale=puls, blink=bl)
    mouth_o(c, size=0.85)
    # sound waves near edges
    k = (f % 12) / 12.0
    for i in range(2):
        r = 8 + i * 7 + k * 7
        col = lerp((80, 200, 255), BG, (i + k) / 3.0)
        c.arc_band(26, 118, r - 2, r, col, lower=False, span=0.6)
        c.arc_band(214, 118, r - 2, r, col, lower=True, span=0.6)


def render_speak(c: Canvas, f: int) -> None:
    bl = blink_amount(f, 64)
    draw_open_eye(c, EYE_LX, EYE_Y, blink=bl)
    draw_open_eye(c, EYE_RX_, EYE_Y, blink=bl)
    k = abs(math.sin(f * 0.9))
    rx, ry = 13 + 6 * k, 5 + 12 * k
    c.ellipse(CX, 164, rx + 2.2, ry + 2.2, MOUTH_RIM)
    c.ellipse(CX, 164, rx, ry, MOUTH_DARK)
    if k > 0.55:
        c.ellipse(CX, 168 + 4 * k, 8, 4.5, TONGUE)


def render_think(c: Canvas, f: int) -> None:
    draw_open_eye(c, EYE_LX, EYE_Y, look=(6, -8))
    draw_open_eye(c, EYE_RX_, EYE_Y, look=(6, -8))
    mouth_flat(c, hw=11)
    step = (f // 6) % 4
    for i in range(3):
        col = (90, 220, 255) if i < step else (40, 70, 120)
        c.ellipse(158 + i * 17, 52 - i * 9, 4.5 + i * 0.8, 4.5 + i * 0.8, col)


def render_alert(c: Canvas, f: int) -> None:
    puls = 1.1 + 0.04 * math.sin(f * 1.2)
    draw_open_eye(c, EYE_LX, EYE_Y + 2, scale=puls)
    draw_open_eye(c, EYE_RX_, EYE_Y + 2, scale=puls)
    mouth_o(c, size=1.0)
    if (f // 4) % 2 == 0:
        draw_excl(c, CX, 40, WARN)


def render_panic(c: Canvas, f: int) -> None:
    sh = 2 if (f % 4) < 2 else -2
    draw_x_eye(c, EYE_LX + sh, EYE_Y, 1.0 + 0.06 * math.sin(f), DANGER)
    draw_x_eye(c, EYE_RX_ + sh, EYE_Y, 1.0 + 0.06 * math.cos(f), DANGER)
    mouth_wavy(c, dy=2, color=DANGER)
    if (f // 3) % 2 == 0:
        draw_excl(c, 52, 52, DANGER)
        draw_excl(c, 188, 52, DANGER)


def render_sad(c: Canvas, f: int) -> None:
    dy = breathe(f // 2)
    draw_open_eye(c, EYE_LX, EYE_Y + dy, blink=0.28, lid_slant=-0.35,
                  look=(0, 5))
    draw_open_eye(c, EYE_RX_, EYE_Y + dy, blink=0.28, lid_slant=0.35,
                  look=(0, 5))
    draw_brow(c, EYE_LX, EYE_Y + dy, -0.5)
    draw_brow(c, EYE_RX_, EYE_Y + dy, 0.5)
    mouth_frown(c, dy=dy)
    draw_tear(c, EYE_LX + EYE_RX - 4, EYE_Y + 18 + dy, f)


def render_angry(c: Canvas, f: int) -> None:
    sh = 1 if (f // 6) % 2 == 0 else 0
    draw_open_eye(c, EYE_LX, EYE_Y + sh, blink=0.22, lid_slant=0.42)
    draw_open_eye(c, EYE_RX_, EYE_Y + sh, blink=0.22, lid_slant=-0.42)
    draw_brow(c, EYE_LX, EYE_Y, 0.55, (200, 60, 70))
    draw_brow(c, EYE_RX_, EYE_Y, -0.55, (200, 60, 70))
    mouth_flat(c, dy=4, hw=17)
    # anger mark top-right (above brows)
    if (f // 5) % 2 == 0:
        m = 176, 30
        c.line(m[0] - 8, m[1] - 8, m[0] - 2, m[1] - 2, 2.2, DANGER)
        c.line(m[0] + 8, m[1] - 8, m[0] + 2, m[1] - 2, 2.2, DANGER)
        c.line(m[0] - 8, m[1] + 8, m[0] - 2, m[1] + 2, 2.2, DANGER)
        c.line(m[0] + 8, m[1] + 8, m[0] + 2, m[1] + 2, 2.2, DANGER)


def render_love(c: Canvas, f: int) -> None:
    s = 1.0 + 0.12 * math.sin(f * 0.7)
    draw_heart(c, EYE_LX, EYE_Y, s, HEART)
    draw_heart(c, EYE_RX_, EYE_Y, s, HEART)
    mouth_open_smile(c, size=0.8)
    draw_blush(c, strong=True)
    # floating mini hearts
    ph = (f % 24) / 24.0
    draw_heart(c, 200, 70 - ph * 16, 0.35, lerp(HEART, BG, ph))
    draw_heart(c, 40, 80 - ((ph + 0.5) % 1.0) * 16, 0.3,
               lerp(HEART, BG, (ph + 0.5) % 1.0))


def render_excited(c: Canvas, f: int) -> None:
    s = 1.0 + 0.15 * abs(math.sin(f * 0.8))
    draw_star(c, EYE_LX, EYE_Y, s, STAR)
    draw_star(c, EYE_RX_, EYE_Y, s, STAR)
    mouth_open_smile(c, size=1.05)
    draw_blush(c)
    for i, (sx, sy) in enumerate(((44, 60), (196, 56), (120, 34))):
        if ((f // 4) + i) % 3 != 0:
            c.line(sx - 5, sy, sx + 5, sy, 1.6, STAR)
            c.line(sx, sy - 5, sx, sy + 5, 1.6, STAR)


def render_dizzy(c: Canvas, f: int) -> None:
    rot = f * 0.55
    draw_spiral_eye(c, EYE_LX, EYE_Y, rot, (255, 190, 90))
    draw_spiral_eye(c, EYE_RX_, EYE_Y, -rot, (255, 190, 90))
    mouth_wavy(c, dy=4)
    draw_blush(c)


def render_plead(c: Canvas, f: int) -> None:
    dy = breathe(f // 2)
    shimmer = 1.02 + 0.03 * math.sin(f * 0.6)
    draw_open_eye(c, EYE_LX, EYE_Y + dy, scale=1.08 * shimmer, look=(0, 8), blink=0.08)
    draw_open_eye(c, EYE_RX_, EYE_Y + dy, scale=1.08 * shimmer, look=(0, 8), blink=0.08)
    draw_brow(c, EYE_LX, EYE_Y + dy - 2, -0.42)
    draw_brow(c, EYE_RX_, EYE_Y + dy - 2, 0.42)
    c.ellipse(EYE_LX - 8, EYE_Y - 8 + dy, 10, 6, WHITE)
    c.ellipse(EYE_RX_ - 8, EYE_Y - 8 + dy, 10, 6, WHITE)
    c.ellipse(EYE_LX + 14, EYE_Y + 18 + dy, 3.2, 5.0, (170, 225, 255))
    c.ellipse(EYE_RX_ - 14, EYE_Y + 18 + dy, 3.2, 5.0, (170, 225, 255))
    mouth_pout(c, dy=7, size=0.95 + 0.03 * math.sin(f * 0.5))
    draw_blush(c, strong=True)


RENDERERS = {
    "idle": render_idle,
    "happy": render_happy,
    "sleepy": render_sleepy,
    "listen": render_listen,
    "speak": render_speak,
    "think": render_think,
    "alert": render_alert,
    "panic": render_panic,
    "sad": render_sad,
    "angry": render_angry,
    "love": render_love,
    "excited": render_excited,
    "dizzy": render_dizzy,
    "plead": render_plead,
}


def render_mood(name: str, frame: int) -> Canvas:
    c = Canvas()
    RENDERERS[name](c, frame)
    c.clip_round()
    return c


# ---------------------------------------------------------------- PNG out
def png_bytes(w: int, h: int, px: bytearray) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        out = struct.pack(">I", len(data)) + tag + data
        return out + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(
        b"\x00" + bytes(px[y * w * 3:(y + 1) * w * 3]) for y in range(h)
    )
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def write_sheet(path: str, frame: int) -> None:
    cols, rows = 4, 4
    sw, sh = cols * W, rows * H
    sheet = bytearray(sw * sh * 3)
    for idx, name in enumerate(MOODS):
        c = render_mood(name, frame)
        gx, gy = (idx % cols) * W, (idx // cols) * H
        for y in range(H):
            src = c.px[y * W * 3:(y + 1) * W * 3]
            dst = ((gy + y) * sw + gx) * 3
            sheet[dst:dst + W * 3] = src
    with open(path, "wb") as fh:
        fh.write(png_bytes(sw, sh, sheet))
    print(f"wrote {path}")


def write_pngs(out_dir: str, frames: int) -> None:
    os.makedirs(out_dir, exist_ok=True)
    for name in MOODS:
        for f in range(frames):
            c = render_mood(name, f)
            p = os.path.join(out_dir, f"{name}_{f:02d}.png")
            with open(p, "wb") as fh:
                fh.write(png_bytes(W, H, c.px))
    print(f"wrote {len(MOODS) * frames} PNGs to {out_dir}")


def write_html(path: str, frames: int = LOOP) -> None:
    cards = []
    for name in MOODS:
        b64 = []
        for f in range(frames):
            c = render_mood(name, f)
            b64.append(base64.b64encode(png_bytes(W, H, c.px)).decode())
        frames_js = ",".join(f'"{s}"' for s in b64)
        cards.append(
            f'<div class="card"><img id="img_{name}" width="240" height="240">'
            f'<div class="label">{name.upper()}</div></div>'
            f'<script>animate("img_{name}", [{frames_js}]);</script>'
        )
    html = f"""<!DOCTYPE html><html><head><meta charset="utf-8">
<title>RoboDesk anime faces</title><style>
body{{background:#0b0d1a;color:#dde;font-family:sans-serif;text-align:center}}
h1{{font-weight:300;letter-spacing:2px}}
.grid{{display:flex;flex-wrap:wrap;justify-content:center;gap:24px;padding:16px}}
.card{{display:flex;flex-direction:column;align-items:center;gap:8px}}
img{{border-radius:50%;box-shadow:0 0 30px #123,0 0 4px #245 inset;background:#000}}
.label{{font-size:13px;letter-spacing:3px;color:#8ac}}
</style>
<script>
function animate(id, frames) {{
  let i = 0;
  setInterval(() => {{
    const el = document.getElementById(id);
    if (el) {{ el.src = "data:image/png;base64," + frames[i]; i = (i+1)%frames.length; }}
  }}, {int(1000 / FPS)});
}}
</script></head><body>
<h1>RoboDesk &mdash; GC9A01 anime faces (240&times;240, {FPS} fps)</h1>
<div class="grid">{"".join(cards)}</div>
</body></html>"""
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(html)
    print(f"wrote {path} ({len(MOODS)} moods x {frames} frames)")


# ---------------------------------------------------------------- terminal
def term_preview(name: str, frame: int) -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    c = render_mood(name, frame)
    print(f"\n=== {name.upper()} ===")
    # downsample 240x240 -> 60x30 (2 px per char via half-block)
    for gy in range(0, H, 8):
        row = ""
        for gx in range(0, W, 4):
            i_top = ((gy + 2) * W + gx + 2) * 3
            i_bot = ((gy + 6) * W + gx + 2) * 3
            rt, gt, bt = c.px[i_top:i_top + 3]
            rb, gb, bb = c.px[i_bot:i_bot + 3]
            row += (f"\033[38;2;{rt};{gt};{bt}m"
                    f"\033[48;2;{rb};{gb};{bb}m\u2580")
        print(row + "\033[0m")


def main() -> None:
    ap = argparse.ArgumentParser(description="RoboDesk anime face preview")
    ap.add_argument("--html", metavar="FILE", help="animated HTML preview")
    ap.add_argument("--sheet", metavar="FILE", help="contact-sheet PNG")
    ap.add_argument("--png", metavar="DIR", help="dump per-frame PNGs")
    ap.add_argument("--mood", metavar="NAME", help="terminal preview one mood")
    ap.add_argument("--frame", type=int, default=0)
    ap.add_argument("--frames", type=int, default=LOOP)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list:
        print("Moods:", ", ".join(MOODS))
        return
    if args.sheet:
        write_sheet(args.sheet, args.frame)
        return
    if args.png:
        write_pngs(args.png, args.frames)
        return
    if args.html:
        write_html(args.html, args.frames)
        return
    if args.mood:
        if args.mood not in RENDERERS:
            print("Unknown mood. Use --list")
            sys.exit(1)
        term_preview(args.mood, args.frame)
        return
    for name in MOODS:
        term_preview(name, args.frame)


if __name__ == "__main__":
    main()
