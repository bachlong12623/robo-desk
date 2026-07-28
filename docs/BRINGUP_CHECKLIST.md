# Hardware bring-up log (fill during lab)

**Kho hiện tại: chỉ A + D.** B/C chưa mua → làm **Phase 0** trước (xem `POWER.md`).

## Phase 0 — A only

| Step | Expected | Measured | OK |
|---|---|---|---|
| MP1584 no-load 5.00V | 5.00±0.05 | | |
| ESP+WiFi idle @5V | boot OK | | |
| I2C scan 0x68/0x38/0x29 (3V3 DevKit) | 3 addr | | |
| GC9A01 backlight | on | | |
| WS2812 dim (no level shift) | 1–few LEDs | | |
| Mic/amp beep | audible | | |
| Motor L then R (wheels free, low duty) | spin | | |
| TCRT DO change over edge (hand test) | toggle | | |
| Rd-04 presence | toggle | | |
| Pi5 `docker compose` /health | ok | | |

## Phase 1 — sau khi có B

| Step | Expected | Measured | OK |
|---|---|---|---|
| Switch + fuse in path | | | |
| 3.3V rail riêng | 3.30±0.05 | | |
| Tụ 5V gần LED/DRV | | | |
| Level shift WS2812 full ring | | | |
| Divider @8.4V → GPIO10 | ~2.08V | | |
| Motors+LED cùng lúc, 5V sag | <100mV | | |
| Cliff drive trên bàn (có safety) | | | |

