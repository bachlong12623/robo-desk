# Firmware ESP32-S3 — RoboDesk

## Module chính

| File | Vai trò |
|---|---|
| `main.c` | Khởi động task |
| `board_pins.h` | **Bảng chân GPIO** — khớp `docs/PINMAP.md` |
| `sensors.c` | MPU6050, DHT20, VL53L0X, TCRT, Rd-04 |
| `display_gc9a01.c` + `face_*.c` | Màn GC9A01 + biểu cảm |
| `drive.c` + `safety.c` | Motor DRV8833 + chặn cliff/shock |
| `audio_path.c` + `voice_solo.c` | I2S mic/amp |
| `led_ring.c` | WS2812 NeoPixel |
| `net_mqtt.c` | MQTT với Pi 5 |
| `mode_manager.c` | Solo ↔ Linked |
| `behavior_brain.c` | Hành vi tự động |
| `power_monitor.c` | Đo pin qua GPIO10 (chia áp) |

## Build

```bash
cd firmware/esp32
idf.py set-target esp32s3
idf.py build flash monitor
```

Board: **ESP32-S3-DevKitC-1 N16R8** (16MB flash, 8MB PSRAM octal).

## Chân GPIO

Xem `board_pins.h` và `docs/PINMAP.md`. Không đổi chân khi đấu dây theo `docs/WIRING.md`.

## ESP-SR (tích hợp sau)

1. Thêm component `esp-sr` (WakeNet + MultiNet).
2. INMP441 @ 16 kHz qua AFE.
3. Map lệnh giọng → `voice_solo_inject()`.
4. TTS qua `esp-tts` thay stub trong `voice_solo_say()`.

## Core pinning

- Core0: `drive`, GPIO an toàn
- Core1: voice, UI, MQTT, brain
