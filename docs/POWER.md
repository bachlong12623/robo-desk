# Hệ thống nguồn 2S

## Tổng quan

Robot dùng **2 pin 18650 mắc nối tiếp (2S)** ≈ 7.4V danh định (6.0–8.4V).

| Thành phần | Vai trò |
|---|---|
| Mạch sạc 2S Type-C | Sạc + cân bằng BM1 |
| Công tắc gạt | Ngắt nguồn khi không dùng — **bắt buộc** vì mạch sạc **không có bảo vệ xả** |
| MP1584EN | Hạ 2S → **5.0V** cho ESP, LED, motor, loa |
| 3.3V | Lấy từ chân **3V3** trên ESP32 DevKit cho cảm biến, LCD, mic |

> **Cảnh báo pin:** Không để robot chạy đến khi pin cạn. Luôn tắt bằng công tắc gạt khi cất. Firmware có cảnh báo % pin (cần mạch chia áp GPIO10).

## Sơ đồ khối nguồn

```
[USB Type-C] ──► [Mạch sạc 2S BM1] ──► [2×18650 nối tiếp]
                                              │
                                         PACK+ / PACK-
                                              │
                                    [Công tắc gạt]──(khuyến nghị: cầu chì 2–3A)
                                              │
                         ┌────────────────────┼────────────────────┐
                         │                    │                    │
                  [Chia áp 100k/33k]    [MP1584EN → 5.0V]         GND chung
                         │                    │
                    GPIO 10 (ADC)      ┌──────┴──────┐
                                       │             │
                                  ESP32 VIN      WS2812 VCC
                                  MAX98357 Vin   DRV8833 VM
                                                 (Rd-04 nếu module 5V)
                                       │
                                  ESP32 3V3 pin ──► I2C sensors, GC9A01, INMP441, DRV8833 VCC
```

## Rail điện áp

| Rail | Điện áp | Cấp cho |
|---|---|---|
| PACK (2S) | 6.0 – 8.4 V | Đầu vào buck |
| 5V | 5.00 ± 0.05 V | ESP VIN, WS2812, DRV8833 VM, MAX98357 Vin |
| 3.3V | 3.30 V | Cảm biến I2C, LCD, mic, logic DRV8833 |

## Chỉnh MP1584EN

1. **Chưa** cắm pin / ESP — chỉ cấp PACK qua công tắc.
2. Xoay chiết áp đến đồng hồ đo **5.00V** ở OUT (không tải).
3. Gắn tụ 470–1000µF sát chân OUT+ và GND của buck.
4. Mới cắm ESP và các module.

## Mạch chia áp đo pin (GPIO10)

```
PACK+ ────[100kΩ]────┬──── GPIO10 (ESP32)
                     │
                    [33kΩ]
                     │
                    GND
```

Ở pin đầy (8.4V): GPIO10 ≈ 2.08V — nằm trong vùng ADC an toàn.

Nếu chưa có điện trở: firmware vẫn chạy nhưng màn `% pin` không chính xác.

## Khuyến nghị an toàn

- Gắn **cầu chì 2–3A** giữa công tắc và buck.
- Tụ **1000µF** gần WS2812 và **100µF** gần DRV8833 VM.
- Tụ **100nF** mỗi module 3.3V (VCC–GND sát chân).
- **Không** bật WS2812 full trắng + 2 motor kẹt cùng lúc khi chưa có tụ lọc.
- Sạc pin: chỉ dùng mạch sạc 2S đi kèm; không sạc từng cell rời.

## Kiểm tra nguồn (trước khi nối ESP)

| Bước | Kỳ vọng |
|---|---|
| Công tắc OFF, đo PACK | 6.0–8.4 V |
| Công tắc ON, đo buck OUT (không tải) | 5.00 V |
| Cắm ESP qua VIN, đo 3V3 DevKit | ~3.3 V |
| Cắm tải nhẹ (1 module I2C) | 5V không tụt > 0.1 V |
