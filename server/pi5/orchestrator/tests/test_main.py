import pathlib
import sys
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

if "websockets" not in sys.modules:
    ws = types.ModuleType("websockets")
    ws.ConnectionClosed = Exception
    ws.serve = lambda *args, **kwargs: None
    sys.modules["websockets"] = ws

if "aiohttp" not in sys.modules:
    aiohttp = types.ModuleType("aiohttp")
    web = types.SimpleNamespace(
        Application=lambda: types.SimpleNamespace(router=types.SimpleNamespace(add_get=lambda *a, **k: None, add_post=lambda *a, **k: None)),
        json_response=lambda payload: payload,
        FileResponse=lambda path: path,
        HTTPNotFound=Exception,
        AppRunner=lambda app: None,
        TCPSite=lambda *args, **kwargs: None,
    )
    aiohttp.web = web
    sys.modules["aiohttp"] = aiohttp

if "paho" not in sys.modules:
    paho = types.ModuleType("paho")
    mqtt_pkg = types.ModuleType("paho.mqtt")
    client_mod = types.ModuleType("paho.mqtt.client")
    client_mod.CallbackAPIVersion = types.SimpleNamespace(VERSION2=2)
    client_mod.Client = lambda *args, **kwargs: types.SimpleNamespace(
        reconnect_delay_set=lambda **kw: None,
        publish=lambda *a, **k: None,
        subscribe=lambda *a, **k: None,
    )
    mqtt_pkg.client = client_mod
    paho.mqtt = mqtt_pkg
    sys.modules["paho"] = paho
    sys.modules["paho.mqtt"] = mqtt_pkg
    sys.modules["paho.mqtt.client"] = client_mod

from app.main import Orchestrator  # noqa: E402


class OrchestratorTests(unittest.TestCase):
    def test_sanitize_payload_truncates_strings(self):
        orch = Orchestrator()
        payload = orch._sanitize_screen_payload(
            {
                "news_title": "x" * 200,
                "calendar_title": "y" * 200,
                "forecast_label": "rain-and-thunder-showers-all-day",
            }
        )
        self.assertLessEqual(len(payload["news_title"]), 95)
        self.assertLessEqual(len(payload["calendar_title"]), 63)
        self.assertLessEqual(len(payload["forecast_label"]), 23)

    def test_cache_fallback(self):
        orch = Orchestrator()
        orch._cache_put("news", {"news_title": "cached headline"})
        self.assertEqual(orch._cache_get("news")["news_title"], "cached headline")

    def test_publish_screen_data_adds_version_and_timestamp(self):
        orch = Orchestrator()
        published = {}

        class DummyMqtt:
            def publish(self, topic, payload, qos=0):
                published["topic"] = topic
                published["payload"] = payload
                published["qos"] = qos

        orch.mqtt = DummyMqtt()
        orch.publish_screen_data({"news_title": "hello"})
        self.assertEqual(orch.screen_payload["version"], 1)
        self.assertIn("ts", orch.screen_payload)
        self.assertIn("/screen_data", published["topic"])


if __name__ == "__main__":
    unittest.main()
