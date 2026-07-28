"""RoboDesk Pi5 orchestrator — Linked AI upgrade over MQTT + WebSocket audio.

Adds linked-mode content feeds for GC9A01:
- weather forecast
- AQI
- RSS news
- local calendar
- desk time tracker
- morning greeting
- moon phase
"""

from __future__ import annotations

import asyncio
import datetime as dt
import json
import logging
import os
import sqlite3
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

import websockets
from aiohttp import web
from paho.mqtt import client as mqtt

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
log = logging.getLogger("orch")

MQTT_HOST = os.getenv("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
ROBOT_ID = os.getenv("ROBOT_ID", "deskbuddy-01")
DATA_DIR = Path(os.getenv("DATA_DIR", "./data"))
ASSETS_DIR = Path(os.getenv("ASSETS_DIR", "./assets"))
MODEL_PROFILE = os.getenv("MODEL_PROFILE", "small")
LATITUDE = os.getenv("LATITUDE", "11.106117")
LONGITUDE = os.getenv("LONGITUDE", "106.630860")
TIMEZONE = os.getenv("TIMEZONE", "Asia/Bangkok")
NEWS_RSS_URL = os.getenv("NEWS_RSS_URL", "https://vnexpress.net/rss/tin-moi-nhat.rss")
CALENDAR_FILE = Path(os.getenv("CALENDAR_FILE", str(DATA_DIR / "calendar.json")))
SCREEN_SCHEMA_VERSION = 2
SCREEN_STALE_S = 900

DATA_DIR.mkdir(parents=True, exist_ok=True)
ASSETS_DIR.mkdir(parents=True, exist_ok=True)


class TelemetryStore:
    def __init__(self, path: Path):
        self.path = path
        self.conn = sqlite3.connect(path, check_same_thread=False)
        self.conn.execute(
            """CREATE TABLE IF NOT EXISTS telemetry (
                ts REAL, batt_pct REAL, temp_c REAL, rh REAL, presence INT, cliff INT
            )"""
        )
        self.conn.execute(
            """CREATE TABLE IF NOT EXISTS presence_sessions (
                start_ts REAL, end_ts REAL
            )"""
        )
        self.conn.commit()
        self._presence_on = False
        self._presence_start = 0.0

    def insert(self, msg: dict):
        cliff = 1 if msg.get("cliff_l") or msg.get("cliff_r") else 0
        self.conn.execute(
            "INSERT INTO telemetry VALUES (?,?,?,?,?,?)",
            (
                time.time(),
                float(msg.get("batt_pct") or 0),
                float(msg.get("temp_c") or 0),
                float(msg.get("rh") or 0),
                1 if msg.get("presence") else 0,
                cliff,
            ),
        )
        presence = bool(msg.get("presence"))
        if presence and not self._presence_on:
            self._presence_on = True
            self._presence_start = time.time()
        elif not presence and self._presence_on:
            self.conn.execute(
                "INSERT INTO presence_sessions VALUES (?,?)",
                (self._presence_start, time.time()),
            )
            self._presence_on = False
        self.conn.commit()

    def sitting_minutes(self) -> float:
        if self._presence_on:
            return (time.time() - self._presence_start) / 60.0
        return 0.0

    def today_minutes(self) -> float:
        now = time.time()
        start = dt.datetime.now().replace(hour=0, minute=0, second=0, microsecond=0).timestamp()
        cur = self.sitting_minutes()
        row = self.conn.execute(
            "SELECT COALESCE(SUM(end_ts - start_ts), 0) FROM presence_sessions WHERE end_ts >= ?",
            (start,),
        ).fetchone()
        hist = float(row[0] or 0.0) / 60.0
        return hist + cur


class AIPipeline:
    """STT → LLM → TTS stubs sized for microSD 64GB (small profile)."""

    def __init__(self, profile: str):
        self.profile = profile
        log.info("AI profile=%s (wire Whisper/Ollama/Piper here)", profile)

    def transcribe(self, pcm: bytes) -> str:
        if not pcm:
            return ""
        # Placeholder until faster-whisper installed on device
        return ""

    def chat(self, text: str, telemetry: dict | None = None) -> tuple[str, dict | None]:
        t = (text or "").strip().lower()
        if not t:
            return "", None
        if "patrol" in t or "tuần tra" in t:
            return "Okay, I will patrol the desk.", {"type": "goal", "goal": "patrol", "ttl_ms": 30000}
        if "stop" in t or "dừng" in t:
            return "Stopping.", {"type": "goal", "goal": "stop"}
        if "come" in t or "lại đây" in t:
            return "On my way.", {"type": "goal", "goal": "approach_user", "ttl_ms": 10000}
        if "temperature" in t or "nhiệt" in t:
            temp = (telemetry or {}).get("temp_c")
            return f"It is about {temp} degrees.", None
        return f"I heard: {text}", {"type": "emote_hint", "mood": "happy"}

    def synthesize(self, text: str) -> bytes:
        # Return empty PCM; Piper integration fills this
        log.info("TTS: %s", text)
        return b""


class Orchestrator:
    def __init__(self):
        self.store = TelemetryStore(DATA_DIR / "telemetry.db")
        self.ai = AIPipeline(MODEL_PROFILE)
        self.last_telemetry: dict = {}
        self.screen_payload: dict = {}
        self._screen_version = 0
        self._cache: dict[str, dict] = {}
        self._last_presence = False
        self._morning_greeted_date = ""
        self.mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="robodesk-orch")
        self.mqtt.on_connect = self._on_connect
        self.mqtt.on_message = self._on_message
        self.mqtt.reconnect_delay_set(min_delay=1, max_delay=30)

    def _topic(self, leaf: str) -> str:
        return f"deskbuddy/{ROBOT_ID}/{leaf}"

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        log.info("MQTT connected rc=%s", reason_code)
        client.subscribe(self._topic("telemetry"))
        client.subscribe(self._topic("hello"))
        client.subscribe(self._topic("event"))
        client.publish("deskbuddy/server/ready", json.dumps({"ts": time.time(), "profile": MODEL_PROFILE}), qos=1)

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except Exception:
            return
        if msg.topic.endswith("/telemetry"):
            self.last_telemetry = payload
            self.store.insert(payload)
            self._maybe_greet_morning(bool(payload.get("presence")))
            # Comfort insight
            if payload.get("presence") and float(payload.get("temp_c") or 0) >= 30:
                if self.store.sitting_minutes() > 45:
                    self.publish_cmd({"type": "goal", "goal": "stop"})
                    self.publish_emote("alert")
        elif msg.topic.endswith("/hello"):
            log.info("Robot hello: %s", payload)

    def publish_cmd(self, cmd: dict):
        self.mqtt.publish(self._topic("cmd"), json.dumps(cmd), qos=1)

    def publish_emote(self, mood: str):
        self.mqtt.publish(self._topic("emote"), json.dumps({"mood": mood}), qos=0)

    def publish_screen(self, screen: str, hold_s: int = 15):
        self.mqtt.publish(self._topic("emote"), json.dumps({"screen": screen, "hold_s": hold_s}), qos=0)

    def publish_screen_data(self, payload: dict):
        self._screen_version += 1
        payload = self._sanitize_screen_payload(payload)
        payload["ts"] = time.time()
        payload["version"] = self._screen_version
        self.screen_payload = payload
        self.mqtt.publish(self._topic("screen_data"), json.dumps(payload, ensure_ascii=True), qos=0)

    def _maybe_greet_morning(self, presence: bool):
        today = dt.date.today().isoformat()
        now = dt.datetime.now()
        if presence and not self._last_presence and 5 <= now.hour < 11 and self._morning_greeted_date != today:
            self.publish_screen_data({**self.screen_payload, "morning_pending": True})
            self.publish_emote("happy")
            self.publish_screen("morning", 20)
            self._morning_greeted_date = today
        self._last_presence = presence

    def heartbeat_loop(self):
        while True:
            self.mqtt.publish(self._topic("heartbeat"), json.dumps({"ts": time.time()}), qos=0)
            time.sleep(2)

    @staticmethod
    def _fetch_json(url: str) -> dict:
        last_exc = None
        for delay in (0, 1, 2):
            if delay:
                time.sleep(delay)
            try:
                with urllib.request.urlopen(url, timeout=10) as resp:
                    return json.loads(resp.read().decode("utf-8"))
            except Exception as exc:
                last_exc = exc
        raise last_exc

    @staticmethod
    def _fetch_text(url: str) -> str:
        last_exc = None
        for delay in (0, 1, 2):
            if delay:
                time.sleep(delay)
            try:
                with urllib.request.urlopen(url, timeout=10) as resp:
                    return resp.read().decode("utf-8", errors="replace")
            except Exception as exc:
                last_exc = exc
        raise last_exc

    @staticmethod
    def _clip(text: str, limit: int) -> str:
        text = " ".join((text or "").split())
        return text[:limit]

    def _cache_get(self, key: str, fresh_for_s: int = SCREEN_STALE_S) -> dict:
        item = self._cache.get(key) or {}
        ts = float(item.get("_ts", 0) or 0)
        if ts and (time.time() - ts) <= fresh_for_s:
            return {k: v for k, v in item.items() if k != "_ts"}
        return {}

    def _cache_put(self, key: str, payload: dict) -> dict:
        clean = dict(payload)
        clean["_ts"] = time.time()
        self._cache[key] = clean
        return payload

    def _sanitize_screen_payload(self, payload: dict) -> dict:
        out = dict(payload)
        for key, limit in {
            "forecast_label": 23,
            "aqi_label": 15,
            "news_title": 95,
            "calendar_title": 63,
            "morning_line1": 47,
            "morning_line2": 47,
            "moon_label": 23,
        }.items():
            if key in out:
                out[key] = self._clip(str(out.get(key, "")), limit)
        return out

    def _load_calendar(self) -> dict:
        if not CALENDAR_FILE.exists():
            CALENDAR_FILE.write_text(
                json.dumps(
                    [
                        {"title": "Sprint review", "start": "2026-07-28T15:30:00+07:00"},
                        {"title": "Deep work", "start": "2026-07-28T20:00:00+07:00"},
                    ],
                    indent=2,
                ),
                encoding="utf-8",
            )
        try:
            items = json.loads(CALENDAR_FILE.read_text(encoding="utf-8"))
        except Exception:
            return self._cache_get("calendar") or {
                "calendar_has": False, "calendar_title": "", "calendar_hh": 0, "calendar_mm": 0, "calendar_in_min": 0
            }
        now = dt.datetime.now().astimezone()
        best = None
        for item in items:
            try:
                start = dt.datetime.fromisoformat(item["start"])
            except Exception:
                continue
            if start >= now and (best is None or start < best[0]):
                best = (start, str(item.get("title", "Event")))
        if not best:
            return self._cache_put("calendar", {
                "calendar_has": False, "calendar_title": "", "calendar_hh": 0, "calendar_mm": 0, "calendar_in_min": 0
            })
        start, title = best
        return self._cache_put("calendar", {
            "calendar_has": True,
            "calendar_title": self._clip(title, 63),
            "calendar_hh": start.hour,
            "calendar_mm": start.minute,
            "calendar_in_min": max(0, int((start - now).total_seconds() // 60)),
        })

    def _fetch_news(self) -> dict:
        try:
            xml_text = self._fetch_text(NEWS_RSS_URL)
            root = ET.fromstring(xml_text)
            item = root.find(".//item/title")
            title = item.text.strip() if item is not None and item.text else "No headline"
        except Exception as e:
            log.warning("news fetch failed: %s", e)
            cached = self._cache_get("news")
            if cached:
                return cached
            title = "No headline"
        return self._cache_put("news", {"news_title": self._clip(title, 95)})

    def _moon_payload(self) -> dict:
        today = dt.date.today()
        # simple synodic month approximation
        ref = dt.date(2000, 1, 6)
        age = ((today - ref).days % 29.53058867)
        phase_pct = int((age / 29.53058867) * 100) % 100
        illum = int((1 - abs(1 - 2 * (age / 29.53058867))) * 100)
        if age < 1.8:
            label = "New moon"
        elif age < 7.4:
            label = "Waxing crescent"
        elif age < 9.2:
            label = "First quarter"
        elif age < 14.8:
            label = "Waxing gibbous"
        elif age < 16.6:
            label = "Full moon"
        elif age < 22.1:
            label = "Waning gibbous"
        elif age < 24.0:
            label = "Last quarter"
        else:
            label = "Waning crescent"
        return {"moon_phase_pct": phase_pct, "moon_illum_pct": illum, "moon_label": label}

    def _fetch_weather(self) -> dict:
        q = urllib.parse.urlencode(
            {
                "latitude": LATITUDE,
                "longitude": LONGITUDE,
                "current": "temperature_2m,weather_code",
                "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max",
                "timezone": TIMEZONE,
            }
        )
        q_aqi = urllib.parse.urlencode(
            {
                "latitude": LATITUDE,
                "longitude": LONGITUDE,
                "current": "us_aqi,pm2_5",
                "timezone": TIMEZONE,
            }
        )
        payload: dict = {}
        try:
            w = self._fetch_json(f"https://api.open-meteo.com/v1/forecast?{q}")
            cur = w.get("current", {})
            daily = w.get("daily", {})
            code = int(cur.get("weather_code", 0) or 0)
            label = "Clear" if code == 0 else ("Rain" if code >= 60 else "Cloudy")
            payload.update(
                {
                    "forecast_temp_c": float(cur.get("temperature_2m") or 0.0),
                    "forecast_min_c": float((daily.get("temperature_2m_min") or [0])[0]),
                    "forecast_max_c": float((daily.get("temperature_2m_max") or [0])[0]),
                    "forecast_rain_pct": int((daily.get("precipitation_probability_max") or [0])[0] or 0),
                    "forecast_weather_code": code,
                    "forecast_label": label,
                }
            )
        except Exception as e:
            log.warning("weather fetch failed: %s", e)
            payload.update(self._cache_get("weather"))
        try:
            a = self._fetch_json(f"https://air-quality-api.open-meteo.com/v1/air-quality?{q_aqi}")
            cur = a.get("current", {})
            us = int(cur.get("us_aqi") or 0)
            if us <= 50:
                label = "GOOD"
            elif us <= 100:
                label = "MODERATE"
            elif us <= 150:
                label = "UNHEALTHY S"
            else:
                label = "UNHEALTHY"
            payload.update({"aqi_us": us, "aqi_pm25": float(cur.get("pm2_5") or 0.0), "aqi_label": label})
        except Exception as e:
            log.warning("aqi fetch failed: %s", e)
            payload.update(self._cache_get("aqi"))
        if any(k.startswith("forecast_") for k in payload):
            self._cache_put("weather", {k: v for k, v in payload.items() if k.startswith("forecast_")})
        if "aqi_us" in payload:
            self._cache_put("aqi", {k: v for k, v in payload.items() if k.startswith("aqi_")})
        return payload

    def build_screen_payload(self) -> dict:
        payload: dict = {}
        payload.update(self._fetch_weather())
        payload.update(self._fetch_news())
        payload.update(self._load_calendar())
        payload.update(self._moon_payload())
        session_min = int(self.store.sitting_minutes())
        today_min = int(self.store.today_minutes())
        payload.update(
            {
                "desk_today_min": today_min,
                "desk_session_min": session_min,
                "desk_break_due": session_min >= 50,
                "morning_pending": False,
            }
        )
        t = payload.get("forecast_temp_c") or self.last_telemetry.get("temp_c") or 0
        aqi = int(payload.get("aqi_us") or 0)
        payload["morning_line1"] = "Good morning"
        payload["morning_line2"] = f"{t:.0f}C AQI {aqi}" if aqi else f"{t:.0f}C Focus mode"
        return self._sanitize_screen_payload(payload)

    async def content_loop(self):
        while True:
            payload = await asyncio.to_thread(self.build_screen_payload)
            self.publish_screen_data(payload)
            await asyncio.sleep(300)

    def start_mqtt(self):
        self.mqtt.connect(MQTT_HOST, MQTT_PORT, 60)
        self.mqtt.loop_start()
        import threading

        threading.Thread(target=self.heartbeat_loop, daemon=True).start()

    async def audio_handler(self, websocket):
        log.info("Audio WS client connected")
        buf = bytearray()
        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    buf.extend(message)
                    # ~1s @ 16k mono s16
                    if len(buf) >= 32000:
                        text = self.ai.transcribe(bytes(buf))
                        buf.clear()
                        reply, cmd = self.ai.chat(text, self.last_telemetry)
                        if cmd:
                            if cmd.get("type") == "emote_hint":
                                self.publish_emote(cmd.get("mood", "think"))
                            else:
                                self.publish_cmd(cmd)
                        if reply:
                            self.publish_emote("speak")
                            pcm = self.ai.synthesize(reply)
                            if pcm:
                                await websocket.send(pcm)
                else:
                    log.debug("WS text: %s", message)
        except websockets.ConnectionClosed:
            log.info("Audio WS closed")

    async def start_ws(self):
        async with websockets.serve(self.audio_handler, "0.0.0.0", 8081, max_size=8_000_000):
            log.info("WebSocket audio on :8081")
            await asyncio.Future()

    def build_http(self) -> web.Application:
        app = web.Application()

        async def health(_):
            return web.json_response(
                {
                    "ok": True,
                    "robot": ROBOT_ID,
                    "sitting_min": self.store.sitting_minutes(),
                    "today_min": self.store.today_minutes(),
                    "telemetry": self.last_telemetry,
                    "screens": self.screen_payload,
                    "screen_feed_fresh": (time.time() - float(self.screen_payload.get("ts", 0) or 0)) < SCREEN_STALE_S,
                    "sd_profile": MODEL_PROFILE,
                }
            )

        async def assets(request):
            name = request.match_info["name"]
            path = ASSETS_DIR / name
            if not path.exists():
                raise web.HTTPNotFound()
            return web.FileResponse(path)

        async def say(request):
            body = await request.json()
            text = body.get("text", "")
            reply, cmd = self.ai.chat(text, self.last_telemetry)
            if cmd and cmd.get("type") != "emote_hint":
                self.publish_cmd(cmd)
            return web.json_response({"reply": reply, "cmd": cmd})

        async def cmd(request):
            body = await request.json()
            self.publish_cmd(body)
            return web.json_response({"ok": True})

        async def screens(_):
            return web.json_response(self.screen_payload)

        app.router.add_get("/health", health)
        app.router.add_get("/api/screens", screens)
        app.router.add_get("/assets/{name}", assets)
        app.router.add_post("/api/say", say)
        app.router.add_post("/api/cmd", cmd)
        return app


async def main_async():
    orch = Orchestrator()
    orch.start_mqtt()
    asyncio.create_task(orch.content_loop())
    app = orch.build_http()
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", 8080)
    await site.start()
    log.info("HTTP dashboard API on :8080")
    await orch.start_ws()


def main():
    asyncio.run(main_async())


if __name__ == "__main__":
    main()
