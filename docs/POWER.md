# Power rail — 2S → 5V / 3.3V

## Inventory note

Hiện **chỉ có BOM mục A + D**. Mục B (công tắc, fuse, tụ, chia áp, level shift, LDO 3.3V riêng) **chưa có**.

Dùng **Phase 0 (A-only)** để smoke-test từng khối; mua B trước khi chạy motor+LED full đồng thời hoặc mang robot ra bàn thật.

## Tree (đầy đủ — cần B)

```
Type-C charger (2S balance + protect)
        │
2×18650 series ──► PACK+ / PACK-
        │
   [Switch]──[Fuse 2–3A]          ← B: chưa có → nối tạm, luôn giám sát
        │
        ├──► Voltage divider 100k/33k ──► ESP GPIO10   ← B: chưa có → bỏ ADC pin tạm
        │
        └──► MP1584EN ──► 5.0V rail ──┬── ESP32 VIN/5V
                                     ├── WS2812 VCC (+1000µF)  ← B: tụ chưa có
                                     ├── MAX98357A Vin
                                     ├── DRV8833 VM (+100µF)
                                     ├── Rd-04 (if 5V module)
                                     └── LDO 3.3V 1A ──► sensors…  ← B: chưa có
```

## Phase 0 — chỉ dùng A (workaround)

| Thiếu (B) | Workaround tạm | Rủi ro |
|---|---|---|
| Công tắc + fuse | Rút dây PACK / rút USB khi không dùng; không để chạy không người | Cháy ngắn mạch |
| Tụ 1000µF / 100nF | Thử LED dim + motor duty thấp | LED nháy / reset ESP khi motor kick |
| LDO 3.3V riêng | Lấy **3V3 từ DevKit** cho I2C/LCD/mic; 5V buck chỉ cho ESP VIN + amp + (LED/motor khi test) | Quá tải 3V3 onboard nếu bật hết |
| Level shift WS2812 | Thử DATA 3.3V trực tiếp ở độ sáng thấp | LED không nhận / màu sai |
| Chia áp ADC | Bỏ `power_monitor` / hardcode %; xem điện áp bằng DMM | Không có fuel gauge trên UI |
| Ốc / trục omni | Keo nóng + băng dính trên breadboard chassis tạm | Rơi linh kiện |

**Thứ tự Phase 0 an toàn:** (1) buck → 5.00V no-load (2) ESP only (3) LCD (4) I2C sensors trên 3V3 DevKit (5) mic/amp (6) LED dim (7) motor từng bánh, bánh treo — **chưa** cliff-drive trên mép bàn.

## Setpoints

| Rail | Target | Max load (design) |
|---|---|---|
| PACK (2S) | 6.0–8.4 V | — |
| 5V buck | 5.00 ± 0.05 V | ≤ 2.5 A peak (motors+LED) — chỉ sau khi có B |
| 3.3V | 3.30 ± 0.05 V | ≤ 800 mA — Phase 0: DevKit 3V3, tải nhẹ |

## Commissioning checklist

### Phase 0 (A only)
1. MP1584 no-load **5.00 V**.
2. ESP từ 5V VIN — idle WiFi.
3. Từng khối: LCD → I2C scan → audio → LED dim → motor no-load.
4. Không để WS2812 full white + 2 motor stall cùng lúc.

### Phase 1 (sau khi mua B)
1. Thêm switch + fuse + tụ rail.
2. LDO/Buck 3.3V riêng; chuyển sensor/LCD/mic sang rail này.
3. Level shift WS2812; tụ sát vòng LED.
4. Chia áp → GPIO10; calibrate ADC.
5. Full load test rồi mới tuần tra bàn.
