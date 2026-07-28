# RoboDesk

Desk companion robot: **ESP32-S3 Solo-first** + optional **Raspberry Pi 5 Linked** AI (microSD 64GB).

## Layout

```
BOM.txt                 # A+D đang có; B+C chưa mua
docs/
  PINMAP.md             # GPIO map
  POWER.md              # 2S → 5V/3.3V (+ Phase 0 A-only)
  WIRING.md             # Breadboard bring-up
  PROTOCOL.md           # MQTT / WS / BLE
  BRINGUP_CHECKLIST.md  # Phase 0 rồi Phase 1
firmware/esp32/         # ESP-IDF project
server/pi5/             # Docker Mosquitto + orchestrator
```

**Kho phần cứng:** mục A (robot) + D (Pi 5) = có. Mục B (bảo vệ nguồn/cơ khí) và C (nice-to-have) = chưa có — mua B trước khi full-load / chạy mép bàn.
## Firmware (ESP32-S3)

```bash
cd firmware/esp32
idf.py set-target esp32s3
# Edit WiFi in NVS or app_config.c defaults
idf.py build flash monitor
```

Requires ESP-IDF ≥ 5.1, PSRAM octal (N16R8).

## Pi 5 server

```bash
cd server/pi5
docker compose up -d --build
curl http://localhost:8080/health
```

MQTT `:1883`, HTTP `:8080`, audio WS `:8081`.

On-device without Docker:

```bash
cd server/pi5/orchestrator
python -m venv .venv && source .venv/bin/activate  # or Windows venv
pip install -r requirements.txt
MQTT_HOST=127.0.0.1 python -m app.main
```

Run Mosquitto locally and point ESP `mqtt_uri` to `mqtt://<pi-ip>`.

## Safety

All motion commands are gated on the ESP (cliff TCRT, tip/shock MPU, battery critical). Losing Pi falls back to **Solo** automatically.
