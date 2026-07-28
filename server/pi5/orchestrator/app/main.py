"""RoboDesk Pi5 orchestrator — Linked AI upgrade over MQTT + WebSocket audio."""

from __future__ import annotations

import asyncio
import json
import logging
import os
import sqlite3
import time
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
        self.mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="robodesk-orch")
        self.mqtt.on_connect = self._on_connect
        self.mqtt.on_message = self._on_message

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

    def heartbeat_loop(self):
        while True:
            self.mqtt.publish(self._topic("heartbeat"), json.dumps({"ts": time.time()}), qos=0)
            time.sleep(2)

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
                    "telemetry": self.last_telemetry,
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

        app.router.add_get("/health", health)
        app.router.add_get("/assets/{name}", assets)
        app.router.add_post("/api/say", say)
        app.router.add_post("/api/cmd", cmd)
        return app


async def main_async():
    orch = Orchestrator()
    orch.start_mqtt()
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
