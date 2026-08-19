# Pi 5 — Linked mode (tùy chọn)

Robot chạy được **hoàn toàn không cần Pi**. Pi 5 bổ sung AI, thời tiết, tin tức qua MQTT.

## Thành phần

- **Mosquitto** — MQTT broker (`:1883`)
- **orchestrator** — heartbeat, telemetry SQLite, HTTP API (`:8080`), WebSocket audio (`:8081`)
- **assets/** — gói biểu cảm/SFX OTA cho ESP

## Chạy nhanh

```bash
cd server/pi5
docker compose up -d --build
curl -s localhost:8080/health
```

## Test MQTT

```bash
mosquitto_sub -h localhost -t 'deskbuddy/#' -v
curl -s -X POST localhost:8080/api/say \
  -H 'content-type: application/json' \
  -d '{"text":"come here"}'
```

## Cấu hình ESP

Trỏ `mqtt_uri` trong NVS / `app_config.c` tới `mqtt://<pi-ip>`.

Giao thức chi tiết: `docs/PROTOCOL.md`.

## Model AI (tuỳ chọn)

`MODEL_PROFILE=small` — Whisper tiny, LLM Q4 qua Ollama trên Pi, Piper TTS. Cài model trên host Pi, không nằm trong repo.
