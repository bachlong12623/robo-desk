# RoboDesk link protocol (ESP ↔ Pi 5)

mDNS: `deskbuddy-ai.local`  
Robot id default: `deskbuddy-01`

## Modes

| Mode | When |
|---|---|
| `solo` | No Pi heartbeat for > 5 failed intervals |
| `linked` | Heartbeat OK + `server/ready` |

Heartbeat: Pi publishes `deskbuddy/{id}/heartbeat` every 2s. ESP misses 3 → solo.

## MQTT topics

Broker on Pi (Mosquitto), LAN only.

| Topic | Dir | Payload |
|---|---|---|
| `deskbuddy/{id}/hello` | ESP→ | JSON capabilities on boot / reconnect |
| `deskbuddy/{id}/mode` | ESP→ | `{"mode":"solo\|linked"}` |
| `deskbuddy/{id}/telemetry` | ESP→ | sensors @ 1–2 Hz |
| `deskbuddy/{id}/event` | ESP→ | cliff, shock, wake, cmd_local |
| `deskbuddy/{id}/heartbeat` | Pi→ | `{"ts":...}` |
| `deskbuddy/{id}/cmd` | Pi→ | behavior command (safety-gated on ESP) |
| `deskbuddy/{id}/emote` | Pi→ | face/LED mood |
| `deskbuddy/{id}/asset` | Pi→ | manifest URL for OTA assets |
| `deskbuddy/server/ready` | Pi→ | announce AI stack up |

### Telemetry JSON

```json
{
  "ts": 1710000000,
  "mode": "linked",
  "batt_v": 7.6,
  "batt_pct": 72,
  "temp_c": 28.5,
  "rh": 55.0,
  "tof_mm": 420,
  "presence": true,
  "cliff_l": false,
  "cliff_r": false,
  "imu": {"ax":0,"ay":0,"az":1,"shock":false},
  "wifi_rssi": -55
}
```

### Command JSON

```json
{
  "id": "uuid",
  "type": "goal",
  "goal": "approach_user|patrol|stop|spin|emote_motion",
  "params": {},
  "ttl_ms": 5000
}
```

Motion commands always pass ESP safety gate (cliff / tip / tof emergency).

## WebSocket audio

URL: `ws://deskbuddy-ai.local:8081/audio/{id}`

- Client ESP binary frames: PCM s16le mono 16 kHz (mic up)
- Server → ESP: PCM s16le mono 16 kHz (TTS / media down)
- Control text frames: `{"type":"vad","active":true}` etc.

## HTTP asset hub

`http://deskbuddy-ai.local:8080/assets/{pack}.tar`  
Manifest MQTT points to path; ESP downloads when idle + charging preferred.

## BLE GATT (Solo remote)

Service UUID `7d5b0001-...` (see firmware). Characteristics: status (notify), cmd (write), emote (write).
