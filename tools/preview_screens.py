#!/usr/bin/env python3
"""RoboDesk info-screen renderer & previewer (GC9A01 240x240 round).

Screens:
  climate, battery, level, radar, clock, status
  weather, aqi, news, calendar, desk, morning, moon

Mirrors firmware screen_info.c — keep both in sync.
"""

from __future__ import annotations

import argparse
import base64
import math
import sys

from preview_faces import (
    Canvas, W, H, CX, CY, png_bytes, lerp, BG, WHITE, FPS,
)

SCREENS = [
    "climate", "battery", "level", "radar", "clock", "status",
    "weather", "aqi", "news", "calendar", "desk", "morning", "moon",
]
LOOP = 96

ACCENT = (90, 220, 255)
LABEL = (95, 130, 180)
TEXT = (230, 240, 255)
GOOD = (80, 230, 120)
WARN = (255, 200, 60)
BAD = (255, 80, 90)
RING_BG = (28, 38, 66)
HUM_C = (80, 170, 255)
MOON_C = (248, 226, 155)
MOON_SH = (66, 80, 125)
NEWS_C = (255, 178, 90)

# ------------------------------------------------------------------ font 5x7
FONT = {
    "0": [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],
    "1": [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "2": [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],
    "3": [0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110],
    "4": [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],
    "5": [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
    "6": [0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
    "7": [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
    "8": [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
    "9": [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100],
    "A": [0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "B": [0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110],
    "C": [0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110],
    "D": [0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100],
    "E": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],
    "F": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000],
    "G": [0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111],
    "H": [0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "I": [0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "J": [0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100],
    "K": [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
    "L": [0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111],
    "M": [0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001],
    "N": [0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001],
    "O": [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "P": [0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000],
    "Q": [0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101],
    "R": [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],
    "S": [0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110],
    "T": [0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100],
    "U": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "V": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100],
    "W": [0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010],
    "X": [0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001],
    "Y": [0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100],
    "Z": [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111],
    "\u00b0": [0b00110, 0b01001, 0b01001, 0b00110, 0, 0, 0],
    "%": [0b11000, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b00011],
    ":": [0, 0b00110, 0b00110, 0, 0b00110, 0b00110, 0],
    ".": [0, 0, 0, 0, 0, 0b00110, 0b00110],
    "-": [0, 0, 0, 0b01110, 0, 0, 0],
    "+": [0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0],
    "/": [0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000],
    "!": [0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100],
    " ": [0, 0, 0, 0, 0, 0, 0],
}


def text_w(s: str, scale: int) -> int:
    return len(s) * 6 * scale - scale


def draw_text(c: Canvas, x: int, y: int, s: str, scale: int,
              color: tuple) -> None:
    for ch in s:
        rows = FONT.get(ch.upper(), FONT[" "])
        for r in range(7):
            bits = rows[r]
            for col in range(5):
                if bits & (1 << (4 - col)):
                    c.rect(x + col * scale, y + r * scale,
                           x + (col + 1) * scale, y + (r + 1) * scale, color)
        x += 6 * scale


def draw_text_center(c: Canvas, y: int, s: str, scale: int,
                     color: tuple) -> None:
    draw_text(c, CX - text_w(s, scale) // 2, y, s, scale, color)


# --------------------------------------------------------------- primitives
def ring_arc(c: Canvas, cx: float, cy: float, r_in: float, r_out: float,
             a0: float, a1: float, color) -> None:
    """Angle-based ring segment. a0<a1 in radians, 0=+x, CCW-positive on
    screen means visually clockwise (y down). color: tuple or fn(t)->tuple
    with t in [0,1] along the arc."""
    span = a1 - a0
    x0, x1 = int(cx - r_out), int(cx + r_out) + 1
    y0, y1 = int(cy - r_out), int(cy + r_out) + 1
    for y in range(max(0, y0), min(H, y1)):
        dy = y - cy
        for x in range(max(0, x0), min(W, x1)):
            dx = x - cx
            d = math.sqrt(dx * dx + dy * dy)
            if d < r_in or d > r_out:
                continue
            ang = math.atan2(dy, dx)
            rel = (ang - a0) % (2 * math.pi)
            if rel <= span:
                col = color(rel / span) if callable(color) else color
                c.set(x, y, col)


def circle_outline(c: Canvas, cx: float, cy: float, r: float, t: float,
                   color: tuple) -> None:
    ring_arc(c, cx, cy, r - t, r, 0, 2 * math.pi - 1e-6, color)


def screen_bg(c: Canvas, title: str) -> None:
    c.fill(BG)
    draw_text_center(c, 26, title, 2, LABEL)


def text_center_clamped(c: Canvas, y: int, s: str, max_chars: int, scale: int,
                        color: tuple) -> None:
    s2 = "".join(ch.upper() if ch.isascii() else " " for ch in s)
    if len(s2) > max_chars:
        s2 = s2[: max_chars - 3] + "..."
    draw_text_center(c, y, s2, scale, color)


def text_line_clamped(c: Canvas, x: int, y: int, s: str, max_chars: int, scale: int,
                      color: tuple) -> None:
    s2 = "".join(ch.upper() if ch.isascii() else " " for ch in s)
    if len(s2) > max_chars:
        s2 = s2[: max_chars - 3] + "..."
    draw_text(c, x, y, s2, scale, color)


def aqi_color(us: int) -> tuple:
    if us <= 50:
        return GOOD
    if us <= 100:
        return WARN
    if us <= 150:
        return (255, 154, 56)
    return BAD


def weather_icon(c: Canvas, cx: float, cy: float, code: int) -> None:
    if code == 0:
        c.ellipse(cx, cy, 20, 20, (255, 212, 90))
        c.ellipse(cx - 6, cy - 6, 4, 4, WHITE)
        return
    if code >= 60:
        c.ellipse(cx - 10, cy, 18, 11, WHITE)
        c.ellipse(cx + 8, cy - 4, 15, 10, WHITE)
        c.ellipse(cx + 22, cy + 2, 13, 9, WHITE)
        for i in range(-14, 17, 10):
            c.line(cx + i, cy + 16, cx + i - 4, cy + 26, 1.6, HUM_C)
        return
    c.ellipse(cx - 10, cy, 18, 11, WHITE)
    c.ellipse(cx + 8, cy - 4, 15, 10, WHITE)
    c.ellipse(cx + 22, cy + 2, 13, 9, WHITE)
    c.ellipse(cx - 26, cy - 12, 11, 11, (255, 212, 90))


def moon_icon(c: Canvas, cx: float, cy: float, phase_pct: int) -> None:
    c.ellipse(cx, cy, 36, 36, MOON_C)
    shift = (1.0 - phase_pct / 50.0) * 36 if phase_pct <= 50 else ((phase_pct - 50) / 50.0) * 36
    if phase_pct <= 50:
        c.ellipse(cx + shift, cy, 36, 36, MOON_SH)
    else:
        c.ellipse(cx - shift, cy, 36, 36, MOON_SH)


def sample_data(f: int) -> dict:
    return {
        "temp_c": 26.0 + 2.0 * math.sin(f * 2 * math.pi / LOOP),
        "rh": 55.0 + 12.0 * math.sin(f * 2 * math.pi / LOOP + 2.0),
        "batt_pct": 76.0,
        "batt_v": 7.91,
        "batt_low": False,
        "pitch_deg": 8.0 * math.sin(f * 2 * math.pi / LOOP),
        "roll_deg": 12.0 * math.cos(f * 2 * math.pi / LOOP * 1.3),
        "tof_mm": int(350 + 250 * math.sin(f * 2 * math.pi / LOOP)),
        "presence": (f % LOOP) < LOOP // 2,
        "hh": 14, "mm": 23, "ss": (f // FPS) % 60, "day": 28, "mon": 7, "dow": 2,
        "linked": True, "rssi": -58, "uptime_s": 2 * 3600 + 15 * 60,
        "forecast_temp_c": 29.0, "forecast_min_c": 27.0, "forecast_max_c": 33.0,
        "forecast_rain_pct": 68, "forecast_weather_code": 61, "forecast_label": "Light rain",
        "aqi_us": 72, "aqi_pm25": 24.6, "aqi_label": "MODERATE",
        "news_title": "Robot ban doc len ke hoach hoc va lam viec thong minh hon moi sang",
        "calendar_has": True, "calendar_hh": 15, "calendar_mm": 30, "calendar_in_min": 67,
        "calendar_title": "Sprint review",
        "desk_today_min": 183, "desk_session_min": 54, "desk_break_due": True,
        "morning_pending": True, "morning_line1": "Good morning", "morning_line2": "Warm, bring water",
        "moon_phase_pct": 63, "moon_illum_pct": 71, "moon_label": "Waning gibbous",
    }


# ------------------------------------------------------------------ screens
def temp_color(t: float) -> tuple:
    if t < 18:
        return (90, 170, 255)
    if t < 27:
        return GOOD
    if t < 32:
        return WARN
    return BAD


def render_climate(c: Canvas, f: int) -> None:
    d = sample_data(f)
    temp = d["temp_c"]
    rh = d["rh"]
    screen_bg(c, "CLIMATE")
    # temp gauge: 270deg sweep, 135deg..405deg, scale 0..40C
    ga0 = math.radians(135)
    frac = max(0.0, min(1.0, temp / 40.0))
    ring_arc(c, CX, CY, 104, 114, ga0, ga0 + math.radians(270), RING_BG)
    ring_arc(c, CX, CY, 104, 114, ga0, ga0 + math.radians(270) * frac,
             lambda t: lerp((90, 170, 255), (255, 120, 80), t))
    tcol = temp_color(temp)
    ts = f"{temp:.1f}"
    tw = text_w(ts, 5) + 10 + text_w("\u00b0C", 2)
    draw_text(c, CX - tw // 2, 62, ts, 5, tcol)
    draw_text(c, CX - tw // 2 + text_w(ts, 5) + 10, 62, "\u00b0C", 2, tcol)
    # humidity: droplet + %
    hs = f"{rh:.0f}%"
    hw = 18 + text_w(hs, 4)
    hx = CX - hw // 2
    c.ellipse(hx + 6, 138, 7, 9, HUM_C)
    for i in range(10):
        half = i * 0.62
        c.rect(int(hx + 6 - half), 122 + i, int(hx + 6 + half) + 1,
               123 + i, HUM_C)
    draw_text(c, hx + 18, 122, hs, 4, HUM_C)
    # comfort mini-face
    comf_ok = 20 <= temp <= 28 and 35 <= rh <= 65
    fc = GOOD if comf_ok else WARN
    fy = 186
    circle_outline(c, CX, fy, 14, 2.4, fc)
    c.ellipse(CX - 5, fy - 3, 1.8, 2.4, fc)
    c.ellipse(CX + 5, fy - 3, 1.8, 2.4, fc)
    if comf_ok:
        c.arc_band(CX, fy + 2, 5.5, 7.8, fc, lower=True, span=0.8)
    else:
        c.line(CX - 5, fy + 6, CX + 5, fy + 6, 1.2, fc)


def render_battery(c: Canvas, f: int) -> None:
    d = sample_data(f)
    pct = d["batt_pct"]
    volts = d["batt_v"]
    c.fill(BG)
    ga0 = math.radians(135)
    ring_arc(c, CX, CY, 96, 112, ga0, ga0 + math.radians(270), RING_BG)
    draw_text_center(c, 34, "BATTERY", 2, LABEL)
    if pct > 40:
        col = GOOD
    elif pct > 15:
        col = WARN
    else:
        col = BAD
    ring_arc(c, CX, CY, 96, 112, ga0,
             ga0 + math.radians(270) * pct / 100.0, col)
    ps = f"{pct:.0f}%"
    draw_text_center(c, 92, ps, 6, TEXT)
    draw_text_center(c, 150, f"{volts:.2f}V", 3, LABEL)
    if pct <= 15 and (f // 6) % 2 == 0:
        draw_text_center(c, 190, "LOW!", 3, BAD)


def render_level(c: Canvas, f: int) -> None:
    d = sample_data(f)
    pitch = d["pitch_deg"]
    roll = d["roll_deg"]
    screen_bg(c, "LEVEL")
    cy = 118
    for r in (22, 46, 70):
        circle_outline(c, CX, cy, r, 1.6, RING_BG)
    c.line(CX - 74, cy, CX + 74, cy, 0.8, RING_BG)
    c.line(CX, cy - 74, CX, cy + 74, 0.8, RING_BG)
    flat = abs(pitch) < 2.0 and abs(roll) < 2.0
    bx = CX + max(-64.0, min(64.0, roll * 3.2))
    by = cy + max(-64.0, min(64.0, pitch * 3.2))
    bcol = GOOD if flat else ACCENT
    c.ellipse(bx, by, 9, 9, bcol)
    c.ellipse(bx - 3, by - 3, 2.6, 2.6, WHITE)
    ps = f"P{pitch:+03.0f} R{roll:+03.0f}"
    draw_text_center(c, 204, ps, 2, GOOD if flat else LABEL)


def render_radar(c: Canvas, f: int) -> None:
    d = sample_data(f)
    dist_mm = d["tof_mm"]
    presence = d["presence"]
    screen_bg(c, "RADAR")
    cy, R = 108, 62
    for r in (R // 3, 2 * R // 3, R):
        circle_outline(c, CX, cy, r, 1.4, RING_BG)
    # rotating sweep + fading trail
    rot = f * 0.22
    for i in range(5, 0, -1):
        a = rot - i * 0.13
        col = lerp((80, 230, 140), BG, i / 5.0)
        c.line(CX, cy, CX + R * math.cos(a), cy + R * math.sin(a),
               1.6 - i * 0.2, col)
    c.line(CX, cy, CX + R * math.cos(rot), cy + R * math.sin(rot),
           2.0, (170, 255, 200))
    # blip at measured distance (fixed bearing, pulse)
    br = 10 + min(1.0, dist_mm / 1200.0) * (R - 14)
    ba = math.radians(-55)
    pul = 1.0 + 0.4 * math.sin(f * 0.9)
    c.ellipse(CX + br * math.cos(ba), cy + br * math.sin(ba),
              3.2 * pul, 3.2 * pul, (255, 120, 90))
    cm = f"{dist_mm / 10:.0f} CM"
    draw_text_center(c, 186, cm, 3, TEXT)
    if presence and (f // 5) % 2 == 0:
        draw_text_center(c, 214, "MOTION", 2, BAD)


def render_clock(c: Canvas, f: int) -> None:
    d = sample_data(f)
    hh, mm, sec = d["hh"], d["mm"], d["ss"]
    c.fill(BG)
    colon = ":" if (f // 6) % 2 == 0 else " "
    draw_text_center(c, 88, f"{hh:02d}{colon}{mm:02d}", 6, TEXT)
    draw_text_center(c, 146, "TUE 28/07", 2, LABEL)
    draw_text_center(c, 176, f"{d['temp_c']:.0f}\u00b0C {d['rh']:.0f}%", 2, (70, 100, 150))
    # seconds dots around the rim
    for i in range(60):
        a = math.radians(i * 6 - 90)
        big = i % 5 == 0
        r = 112
        col = ACCENT if i <= sec else RING_BG
        sz = 2.2 if big else 1.2
        c.ellipse(CX + r * math.cos(a), CY + r * math.sin(a), sz, sz, col)


def render_status(c: Canvas, f: int) -> None:
    d = sample_data(f)
    linked = d["linked"]
    rssi = d["rssi"] + int(4 * math.sin(f * 0.2))
    screen_bg(c, "STATUS")
    mode = "LINKED" if linked else "SOLO"
    draw_text_center(c, 66, mode, 4, GOOD if linked else ACCENT)
    # wifi icon: 3 arcs + dot
    wy = 132
    wx = CX - 40
    c.ellipse(wx, wy, 2.6, 2.6, ACCENT)
    bars = 3 if rssi > -60 else (2 if rssi > -75 else 1)
    for i in range(3):
        col = ACCENT if i < bars else RING_BG
        ring_arc(c, wx, wy, 6 + i * 7, 9 + i * 7,
                 math.radians(215), math.radians(325), col)
    draw_text(c, wx + 32, wy - 10, f"{rssi}", 2, TEXT)
    mq = "MQTT OK" if linked else "MQTT --"
    draw_text_center(c, 162, mq, 2, GOOD if linked else LABEL)
    up_min = d["uptime_s"] // 60
    draw_text_center(c, 190, f"UP {up_min // 60:02d}:{up_min % 60:02d}", 2,
                     LABEL)


def render_weather(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "WEATHER")
    weather_icon(c, 56, 94, d["forecast_weather_code"])
    text_center_clamped(c, 56, d["forecast_label"], 12, 2, LABEL)
    draw_text(c, 102, 78, f"{d['forecast_temp_c']:.0f}\u00b0C", 4, TEXT)
    draw_text(c, 104, 118, f"{d['forecast_min_c']:.0f}\u00b0 {d['forecast_max_c']:.0f}\u00b0", 2, LABEL)
    ring_arc(c, CX, 176, 30, 40, math.radians(180), math.radians(360), RING_BG)
    ring_arc(c, CX, 176, 30, 40, math.radians(180),
             math.radians(180) + math.radians(180) * d["forecast_rain_pct"] / 100.0, HUM_C)
    draw_text_center(c, 166, "RAIN", 2, LABEL)
    draw_text_center(c, 182, f"{d['forecast_rain_pct']}%", 3, HUM_C)


def render_aqi(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "AQI")
    col = aqi_color(d["aqi_us"])
    ring_arc(c, CX, CY, 98, 112, math.radians(135), math.radians(405), RING_BG)
    clamped = max(0, min(200, d["aqi_us"]))
    ring_arc(c, CX, CY, 98, 112, math.radians(135),
             math.radians(135) + math.radians(270) * clamped / 200.0, col)
    draw_text_center(c, 84, f"{d['aqi_us']}", 6, col)
    text_center_clamped(c, 144, d["aqi_label"], 14, 2, LABEL)
    draw_text_center(c, 174, f"{d['aqi_pm25']:.1f}", 3, TEXT)
    draw_text_center(c, 200, "UG M3", 2, LABEL)


def render_news(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "NEWS")
    c.ellipse(38, 64, 10, 10, NEWS_C)
    c.rect(48, 56, 92, 72, NEWS_C)
    draw_text(c, 58, 58, "NEWS", 2, BG)
    text_line_clamped(c, 20, 92, d["news_title"], 18, 2, TEXT)
    tail = d["news_title"][18:] if len(d["news_title"]) > 18 else ""
    text_line_clamped(c, 20, 122, tail, 18, 2, TEXT)
    c.line(40, 168, 200, 168, 1.2, RING_BG)
    draw_text_center(c, 186, "RSS VIA PI5", 2, LABEL)


def render_calendar(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "CALENDAR")
    c.rect(52, 52, 188, 164, (17, 25, 53))
    c.rect(52, 52, 188, 76, (224, 92, 108))
    c.ellipse(76, 52, 6, 6, BG)
    c.ellipse(164, 52, 6, 6, BG)
    if d["calendar_has"]:
        draw_text_center(c, 88, f"{d['calendar_hh']:02d}:{d['calendar_mm']:02d}", 4, TEXT)
        text_center_clamped(c, 126, d["calendar_title"], 18, 2, TEXT)
        draw_text_center(c, 188, f"IN {d['calendar_in_min']} MIN", 2, WARN if d["calendar_in_min"] <= 30 else LABEL)
    else:
        draw_text_center(c, 96, "FREE", 4, GOOD)
        draw_text_center(c, 136, "NO EVENT", 2, LABEL)


def render_desk(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "DESK")
    ring_arc(c, CX, CY, 94, 108, math.radians(135), math.radians(405), RING_BG)
    today = max(0, min(480, d["desk_today_min"]))
    ring_arc(c, CX, CY, 94, 108, math.radians(135),
             math.radians(135) + math.radians(270) * today / 480.0, ACCENT)
    draw_text_center(c, 84, f"{d['desk_today_min']}M", 5, TEXT)
    draw_text_center(c, 146, f"NOW {d['desk_session_min']}M", 2, LABEL)
    draw_text_center(c, 176, "TAKE BREAK" if d["desk_break_due"] else "ON TRACK", 2,
                     WARN if d["desk_break_due"] else GOOD)


def render_morning(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "MORNING")
    c.ellipse(58, 70, 18, 18, (255, 212, 90))
    for i in range(8):
        a = math.radians(i * 45)
        c.line(58 + 24 * math.cos(a), 70 + 24 * math.sin(a),
               58 + 34 * math.cos(a), 70 + 34 * math.sin(a), 1.5, (255, 212, 90))
    text_center_clamped(c, 70, d["morning_line1"], 18, 2, TEXT)
    text_center_clamped(c, 100, d["morning_line2"], 18, 2, LABEL)
    draw_text_center(c, 150, f"{d['forecast_temp_c']:.0f}\u00b0 {d['aqi_us']}AQI", 2, ACCENT)
    draw_text_center(c, 180, f"{d['calendar_hh']:02d}:{d['calendar_mm']:02d}" if d["calendar_has"] else "NO EVENT", 3,
                     WARN if d["calendar_has"] else GOOD)


def render_moon(c: Canvas, f: int) -> None:
    d = sample_data(f)
    screen_bg(c, "MOON")
    moon_icon(c, 120, 102, d["moon_phase_pct"])
    text_center_clamped(c, 154, d["moon_label"], 14, 2, TEXT)
    draw_text_center(c, 182, f"{d['moon_illum_pct']}% LIT", 2, LABEL)
    for i in range(5):
        x = 38 + i * 40 + (6 if i & 1 else 0)
        y = 58 + (8 if i & 1 else 0)
        b = 1.6 + (1 if i == (f // 6) % 5 else 0)
        c.ellipse(x, y, b, b, WHITE)


RENDERERS = {
    "climate": render_climate,
    "battery": render_battery,
    "level": render_level,
    "radar": render_radar,
    "clock": render_clock,
    "status": render_status,
    "weather": render_weather,
    "aqi": render_aqi,
    "news": render_news,
    "calendar": render_calendar,
    "desk": render_desk,
    "morning": render_morning,
    "moon": render_moon,
}


def render_screen(name: str, frame: int) -> Canvas:
    c = Canvas()
    RENDERERS[name](c, frame)
    c.clip_round()
    return c


# ------------------------------------------------------------------ outputs
def write_sheet(path: str, frame: int) -> None:
    cols, rows = 4, math.ceil(len(SCREENS) / 4)
    sw, sh = cols * W, rows * H
    sheet = bytearray(sw * sh * 3)
    for idx, name in enumerate(SCREENS):
        c = render_screen(name, frame)
        gx, gy = (idx % cols) * W, (idx // cols) * H
        for y in range(H):
            src = c.px[y * W * 3:(y + 1) * W * 3]
            dst = ((gy + y) * sw + gx) * 3
            sheet[dst:dst + W * 3] = src
    with open(path, "wb") as fh:
        fh.write(png_bytes(sw, sh, sheet))
    print(f"wrote {path}")


def write_html(path: str, frames: int = LOOP) -> None:
    cards = []
    for name in SCREENS:
        b64 = []
        for f in range(frames):
            c = render_screen(name, f)
            b64.append(base64.b64encode(png_bytes(W, H, c.px)).decode())
        frames_js = ",".join(f'"{s}"' for s in b64)
        cards.append(
            f'<div class="card"><img id="img_{name}" width="240" height="240">'
            f'<div class="label">{name.upper()}</div></div>'
            f'<script>animate("img_{name}", [{frames_js}]);</script>'
        )
    html = f"""<!DOCTYPE html><html><head><meta charset="utf-8">
<title>RoboDesk info screens</title><style>
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
<h1>RoboDesk &mdash; GC9A01 info screens (240&times;240, {FPS} fps)</h1>
<div class="grid">{"".join(cards)}</div>
</body></html>"""
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(html)
    print(f"wrote {path} ({len(SCREENS)} screens x {frames} frames)")


def term_preview(name: str, frame: int) -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    c = render_screen(name, frame)
    print(f"\n=== {name.upper()} ===")
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
    ap = argparse.ArgumentParser(description="RoboDesk info screen preview")
    ap.add_argument("--html", metavar="FILE")
    ap.add_argument("--sheet", metavar="FILE")
    ap.add_argument("--screen", metavar="NAME")
    ap.add_argument("--frame", type=int, default=0)
    ap.add_argument("--frames", type=int, default=LOOP)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list:
        print("Screens:", ", ".join(SCREENS))
        return
    if args.sheet:
        write_sheet(args.sheet, args.frame)
        return
    if args.html:
        write_html(args.html, args.frames)
        return
    if args.screen:
        if args.screen not in RENDERERS:
            print("Unknown screen. Use --list")
            sys.exit(1)
        term_preview(args.screen, args.frame)
        return
    for name in SCREENS:
        term_preview(name, args.frame)


if __name__ == "__main__":
    main()
