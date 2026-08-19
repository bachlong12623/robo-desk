# Hướng dẫn hàn dây + mô phỏng Wokwi (RoboDesk)

Mục tiêu: **cùng một sơ đồ GPIO** cho cả breadboard thật và Wokwi — firmware đọc `board_pins.h`, không đổi chân khi chuyển từ sim sang phần cứng.

```
Nguồn thật (2S → buck → 5V/3.3V)     Wokwi (bỏ qua nguồn pin)
         │                                    │
         └──────────► ESP32-S3 ◄──────────────┘
                         │
      I2C 8/9 ── MPU6050, DHT20, VL53L0X
      SPI 11/12/48/13/14/21 ── GC9A01
      I2S 4/5/6 + 16/17/18 ── mic/amp (chỉ thật)
      GPIO 38 ── WS2812
      GPIO 1/2/41/42 ── DRV8833 → N20
      GPIO 7/15/40/47/10 ── cliff / radar / nút / ADC pin
```

## 1. Quy tắc chung

| Quy tắc | Thực tế | Wokwi |
|---|---|---|
| Chân GPIO | Theo `firmware/esp32/main/board_pins.h` | **Giữ nguyên** |
| Nguồn logic | 3.3V rail (Phase 0: lấy từ DevKit 3V3) | `esp:3V3` |
| Mass | Star-ground gần buck | `esp:GND.*` + breadboard `bn.*` |
| I2C | SDA=8, SCL=9, pull-up 4.7k nếu module không có | Nối thẳng, Wokwi tự pull-up |
| Strapping | **Tránh** GPIO0, 3, 45, 46 | Không dùng các chân này |

**Thứ tự bring-up (cả thật lẫn sim):** ESP → LCD → I2C scan → LED ring → cliff/radar → motor → audio.

---

## 2. Bảng nối dây chi tiết

### 2.1 Bus I2C chung (GPIO 8 / 9)

| Linh kiện | Chân module | → ESP32-S3 | Hàn / nối thật | Wokwi |
|---|---:|---|---|---|
| MPU6050 | VCC | 3.3V | Dây đỏ, ngắn | `mpu:VCC` ← `esp:3V3` |
| MPU6050 | GND | GND | Dây đen, chung mass | `mpu:GND` ← `esp:GND.1` |
| MPU6050 | SDA | **GPIO 8** | Bus I2C chung | `mpu:SDA` ← `esp:8` |
| MPU6050 | SCL | **GPIO 9** | Bus I2C chung | `mpu:SCL` ← `esp:9` |
| DHT20 | VCC / GND / SDA / SCL | 3.3V / GND / 8 / 9 | Cùng bus, addr `0x38` | *Chưa có part chuẩn* — xem §4 |
| VL53L0X | VCC / GND / SDA / SCL | 3.3V / GND / 8 / 9 | Cùng bus, addr `0x29` | *Custom chip* hoặc bỏ qua ở sim cơ bản |

### 2.2 Màn hình GC9A01 (SPI)

| Chân GC9A01 | GPIO | Màu gợi ý | Wokwi (`lcd` = GC9A01Breakout) |
|---|---:|---|---|
| VCC | 3.3V | Đỏ | `lcd:VCC` ← `esp:3V3` |
| GND | GND | Đen | `lcd:GND` ← `esp:GND.1` |
| SCLK | **12** | Vàng | `lcd:SCK` ← `esp:12` |
| MOSI (SDA) | **11** | Xanh lá | `lcd:MOSI` ← `esp:11` |
| CS | **48** | Cam | `lcd:CS` ← `esp:48` |
| DC | **13** | Tím | `lcd:DC` ← `esp:13` |
| RST | **14** | Trắng | `lcd:RST` ← `esp:14` |
| BL (backlight) | **21** | Xám | `lcd:BL` ← `esp:21` (nếu breakout có chân BL) |

> Wokwi: fork project mẫu [ESP32 GC9A01 round TFT](https://wokwi.com/projects/460019066136725505), đổi chân SPI cho khớp bảng trên.

### 2.3 Âm thanh I2S (chỉ phần cứng thật)

| Module | Chân | GPIO | Ghi chú |
|---|---|---:|---|
| INMP441 | SCK / WS / SD | **5 / 4 / 6** | LDO 3.3V, GND chung |
| INMP441 | L/R | GND | Mono left |
| MAX98357A | BCLK / LRC / DIN | **17 / 18 / 16** | Vin 5V, GAIN → GND (9 dB) |
| MAX98357A | SD | 3.3V hoặc float | Enable |
| Loa 3W | ± | OUT amp | Không nối trực tiếp ESP |

Wokwi **không mô phỏng** INMP441/MAX98357 trong diagram cơ bản — test audio trên breadboard thật.

### 2.4 Vòng LED WS2812 (GPIO 38)

| Chân | Nối tới | Thực tế | Wokwi |
|---|---|---|---|
| VCC | 5V rail | 5V (sau buck) | `ring:VCC` ← `esp:5V` hoặc `esp:3V3` khi test dim |
| GND | GND | Mass chung | `ring:GND` ← `esp:GND.1` |
| DIN | **GPIO 38** | Qua level shifter 3.3→5V (khuyến nghị) | `ring:DIN` ← `esp:38` |

Wokwi: `wokwi-led-ring`, `attrs: { "pixels": "12" }`.

### 2.5 Motor DRV8833 + N20 (GPIO 1, 2, 41, 42)

| DRV8833 | → | ESP / Motor |
|---|---|---|
| VM | 5V + tụ 100µF | Rail motor |
| VCC | 3.3V | Logic |
| GND | GND | Mass |
| AIN1 / AIN2 | **GPIO 1 / 2** | Motor trái |
| BIN1 / BIN2 | **GPIO 41 / 42** | Motor phải |
| AOUT1/2 | Motor L | |
| BOUT1/2 | Motor R | |

**Hàn thật:** motor không gắn bánh khi test đầu; duty thấp 20–30%.

**Wokwi:** không có DRV8833 — thay bằng 4 LED (hoặc `wokwi-led`) trên GPIO 1, 2, 41, 42 để thấy PWM/safety gate.

### 2.6 Cảm biến digital

| Cảm biến | Chân | GPIO | Logic | Wokwi thay thế |
|---|---|---:|---|---|
| TCRT5000 trái | DO | **15** | `0` = có vật / mép | `wokwi-pushbutton` → GND (nhấn = cliff) |
| TCRT5000 phải | DO | **7** | như trên | pushbutton |
| Rd-04 | OUT | **40** | `1` = presence (pull-up) | pushbutton hoặc slide switch |
| Nút user | → GND | **47** | INPUT_PULLUP | `wokwi-pushbutton` |
| Pin ADC | chia áp PACK+ | **10** | 100k/33k → ~2.1V max | `wokwi-potentiometer` giữa 3V3–GND |

Rd-04 UART (nếu module dùng serial): TX module → **GPIO 39** — sim: bỏ qua, dùng chế độ digital OUT.

### 2.7 Nguồn 2S (chỉ breadboard thật — không cần trên Wokwi)

```
[2×18650 2S] → [Công tắc gạt] → [MP1584 IN+]
      │                              │
     GND ─────────────────────── IN- / GND buck
                                     └── OUT 5.0V → ESP VIN, WS2812, DRV VM, MAX98357 Vin
```

| Điểm | Nối |
|---|---|
| PACK+ | Qua công tắc → MP1584 VIN+ |
| PACK- | Star GND |
| MP1584 OUT | 5V bus |
| Chia áp 100k/33k | PACK+ → junction → GPIO **10**; 33k → GND |
| ESP32 | 5V vào **VIN** (hoặc USB khi dev) |
| Cảm biến / LCD / mic | **3.3V** từ DevKit (Phase 0) hoặc LDO riêng (Phase 1) |

---

## 3. Sơ đồ breadboard (nhìn từ trên)

```
                    [GC9A01 + WS2812]
                           |
                    [ESP32-S3 DevKit]
        VL53L0X (trước)          Rd-04
      [N20 L]══════chassis══════[N20 R]
           TCRT-L    TCRT-R (đáy)
                 omni (sau)
           DHT20 (cạnh)  MPU6050 (giữa)
              2S + sạc + buck + công tắc
```

**Mẹo hàn:**
1. Dựng **bus 3V3 / 5V / GND** trên breadboard trước.
2. I2C: một nhánh SDA/SCL, cắm từng module, chạy I2C scan (`0x68`, `0x38`, `0x29`).
3. SPI LCD: dây ngắn, tách khỏi dây motor.
4. Motor/LED: nguồn 5V riêng, mass về **một điểm** gần buck.
5. Cliff TCRT: hướng xuống đáy, chỉnh potentiometer trên module đến khi DO đổi trạng thái ở mép bàn.

---

## 4. Đưa lên Wokwi

### Cách nhanh

1. Mở [wokwi.com](https://wokwi.com) → **New Project** → **ESP32-S3 DevKitC-1**.
2. Copy nội dung `tools/wokwi/diagram.json` vào tab **diagram.json** của project.
3. (Tuỳ chọn) Thêm GC9A01: **+** → tìm **GC9A01** hoặc import chip từ [project mẫu](https://wokwi.com/projects/460019066136725505).
4. Bấm **Play** — kiểm tra Serial Monitor.

### Phạm vi mô phỏng

| Khối | Wokwi | Ghi chú |
|---|---|---|
| ESP32-S3 | ✅ `board-esp32-s3-devkitc-1` | |
| GC9A01 | ✅ custom chip | Cần thêm file chip từ project mẫu |
| MPU6050 | ✅ `wokwi-mpu6050` | |
| WS2812 ring | ✅ `wokwi-led-ring` | 12 LED |
| TCRT / Rd-04 / nút | ✅ pushbutton | Tương tác tay khi sim |
| ADC pin | ✅ potentiometer | Giả lập % pin |
| Motor | ⚠️ LED thay DRV8833 | Đủ test GPIO + safety |
| DHT20 | ⚠️ custom stub | [DHT20 test project](https://wokwi.com/projects/396250059645007873) |
| VL53L0X | ⚠️ custom / stub | Hoặc test trên thật |
| INMP441 / MAX98357 | ❌ | Test trên breadboard |
| WiFi / MQTT / Pi | ⚠️ Wokwi có WiFi giả lập | Cần cấu hình riêng cho Linked mode |

### Firmware ESP-IDF trên Wokwi

- ESP-IDF ≥ 6.0: có thể dùng `idf.py wokwi` (diagram tự sinh).
- Với repo này (ESP-IDF 5.x): dùng diagram thủ công + build local, hoặc viết **sketch Arduino smoke test** cùng GPIO để validate dây trước khi flash firmware đầy đủ.

---

## 5. Checklist sau khi nối dây

### Trên Wokwi
- [ ] Serial boot không lỗi GPIO strapping
- [ ] I2C scan thấy `0x68` (MPU6050)
- [ ] LED ring GPIO 38 đổi màu
- [ ] Nhấn nút cliff → GPIO 7/15 đổi mức
- [ ] Potentiometer → GPIO 10 đọc ADC thay đổi

### Trên breadboard thật
- [ ] Buck no-load = **5.00 V**
- [ ] I2C: `0x68`, `0x38`, `0x29`
- [ ] LCD hiện face smoke test
- [ ] Motor quay duty thấp, bánh **không** chạm mặt bàn
- [ ] Cliff: đưa qua mép → firmware dừng motor
- [ ] Công tắc gạt ngắt hoàn toàn tải khi không dùng

---

## 6. Tham chiếu nhanh GPIO

| GPIO | Chức năng |
|---:|---|
| 1, 2 | Motor trái AIN1/AIN2 |
| 4, 5, 6 | I2S mic WS/BCLK/DIN |
| 7, 15 | TCRT phải / trái |
| 8, 9 | I2C SDA / SCL |
| 10 | ADC pin 2S |
| 11, 12 | LCD MOSI / SCLK |
| 13, 14, 21, 48 | LCD DC / RST / BL / CS |
| 16, 17, 18 | I2S amp DOUT / BCLK / LRCLK |
| 38 | WS2812 DIN |
| 40 | Rd-04 presence |
| 41, 42 | Motor phải BIN1/BIN2 |
| 47 | Nút user |

Nguồn chân: `docs/PINMAP.md`, `firmware/esp32/main/board_pins.h`.
