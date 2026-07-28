# Pi 5 Linked services

- **Mosquitto** — MQTT broker
- **orchestrator** — heartbeat, telemetry SQLite, cmd/emote, HTTP API, WebSocket PCM audio
- **assets/** — face/SFX packs for ESP OTA
- **data/** — SQLite on microSD (log rotation recommended)

## Model profile (`MODEL_PROFILE=small`)

Aimed at **64GB microSD**: Whisper tiny/base, LLM 3–7B Q4 via host Ollama, Piper voice — install models on the Pi host and point env vars when extending `AIPipeline`.

## Quick test

```bash
docker compose up -d --build
curl -s localhost:8080/health | jq
curl -s -X POST localhost:8080/api/say -H 'content-type: application/json' -d '{"text":"come here"}'
mosquitto_sub -h localhost -t 'deskbuddy/#' -v
```
