# Solo AI stack (ESP32-S3)

## Components

| Module | Role |
|---|---|
| `voice_solo.c` | Wake/command API — integrate **esp-sr** (WakeNet + MultiNet) + **esp-tts** |
| `behavior_brain.c` | Utility/behavior tree: presence, comfort, goals, safety interrupts |
| `audio_path.c` | I2S mic/amp |
| `ble_remote.c` | Solo phone remote (GATTS bring-up) |
| `mode_manager.c` | Solo ↔ Linked heartbeat |

## ESP-SR integration steps

1. Add Espressif `esp-sr` component to `firmware/esp32` (IDF component manager or git submodule).
2. Load WakeNet model into PSRAM; feed INMP441 @ 16 kHz via AFE.
3. Map MultiNet phrases → `voice_cmd_t` and `voice_solo_inject()`.
4. Hook `esp-tts` into `voice_solo_say()` replacing `audio_path_speak_text_stub`.

## Core pinning

- Core0: `drive`, safety-sensitive GPIO
- Core1: voice, UI, MQTT, brain
