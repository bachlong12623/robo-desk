# Checklist bring-up phần cứng

In ra và tick từng ô khi test trong phòng lab.

**Trước khi bắt đầu:** đọc `docs/WIRING.md` (hướng dẫn đấu dây) và `docs/POWER.md` (nguồn).

## Giai đoạn 1 — Nguồn & ESP

| Bước | Kỳ vọng | Đo / quan sát | OK |
|---|---|---|---|
| Pin 2S đầy | 7.0–8.4 V ở PACK | | ☐ |
| Công tắc OFF = mất nguồn tải | 0 V sau switch | | ☐ |
| Buck no-load | 5.00 ± 0.05 V | | ☐ |
| ESP boot qua VIN | Serial log "RoboDesk boot" | | ☐ |
| ESP 3V3 pin | ~3.3 V | | ☐ |

## Giai đoạn 2 — Màn hình & I2C

| Bước | Kỳ vọng | Đo / quan sát | OK |
|---|---|---|---|
| GC9A01 backlight | Sáng | | ☐ |
| Face smoke test | Hiện biểu cảm | | ☐ |
| I2C 0x68 | MPU6050 | | ☐ |
| I2C 0x38 | DHT20 | | ☐ |
| I2C 0x29 | VL53L0X | | ☐ |

## Giai đoạn 3 — LED, âm thanh, motor

| Bước | Kỳ vọng | Đo / quan sát | OK |
|---|---|---|---|
| WS2812 ring | Đổi màu, độ sáng thấp trước | | ☐ |
| Mic + amp beep | Nghe thấy tiếng | | ☐ |
| Motor trái (bánh tháo) | Quay duty thấp | | ☐ |
| Motor phải (bánh tháo) | Quay duty thấp | | ☐ |
| 5V không sụt khi motor | < 0.2 V sag | | ☐ |

## Giai đoạn 4 — An toàn & cảm biến

| Bước | Kỳ vọng | Đo / quan sát | OK |
|---|---|---|---|
| TCRT trái (GPIO15) | Đổi mức ở mép bàn | | ☐ |
| TCRT phải (GPIO7) | Đổi mức ở mép bàn | | ☐ |
| Rd-04 (GPIO40) | Đổi khi có người | | ☐ |
| Chia áp GPIO10 | ~2.0 V khi pin đầy | | ☐ |
| Cliff → motor dừng | Firmware safety | | ☐ |

## Giai đoạn 5 — Tích hợp (tuỳ chọn Pi 5)

| Bước | Kỳ vọng | Đo / quan sát | OK |
|---|---|---|---|
| Pi `docker compose up` | Container running | | ☐ |
| `curl localhost:8080/health` | `{"ok":true}` | | ☐ |
| ESP Linked mode | MQTT heartbeat OK | | ☐ |
| Mất Pi → Solo | ESP tự fallback | | ☐ |

## Ghi chú lab

```
Ngày:
Điện áp buck đo được:
Firmware version / commit:
Vấn đề gặp phải:
```
