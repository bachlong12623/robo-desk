# Prompt cho Gemini / AI vẽ sơ đồ đấu dây RoboDesk

Copy nguyên khối prompt bên dưới vào Gemini (hoặc ChatGPT, Claude) kèm yêu cầu **vẽ sơ đồ kỹ thuật**.

---

## Prompt 1 — Sơ đồ tổng thể (khuyên dùng)

```
Vẽ sơ đồ đấu dây kỹ thuật (wiring diagram) cho robot bàn RoboDesk, phong cách infographic
dễ hiểu cho người mới, có màu phân loại từng bus, nhãn tiếng Việt + tiếng Anh trên chân module.

TRUNG TÂM: ESP32-S3 DevKitC-1 N16R8 (hình chữ nhật xanh, 2 hàng header J1 trái / J3 phải).

KHỐI NGUỒN (góc dưới trái, màu đỏ):
- 2 pin 18650 nối tiếp 2S → mạch sạc Type-C BM1 → công tắc gạt → MP1584 buck 5V
- Bus 5V cấp: ESP VIN, WS2812 VCC, DRV8833 VM, MAX98357 Vin
- ESP 3V3 cấp: MPU6050, DHT20, VL53L0X, GC9A01, INMP441, DRV8833 VCC
- Chia áp 100k/33k từ PACK+ → GPIO10

I2C (màu xanh lá, GPIO8 SDA + GPIO9 SCL, bus chung):
- MPU6050 (0x68): VCC,GND,SDA,SCL
- DHT20 (0x38): VCC,GND,SDA,SCL
- VL53L0X (0x29): VCC,GND,SDA,SCL, XSHUT→3V3

SPI LCD GC9A01 1.28" tròn (màu tím):
- VCC→3V3, GND, MOSI→GPIO11, SCK→GPIO12, CS→GPIO48, DC→GPIO13, RST→GPIO14, BL→GPIO21

I2S MIC INMP441 (màu cyan):
- VDD→3V3, GND, SCK→GPIO5, WS→GPIO4, SD→GPIO6, L/R→GND

I2S AMP MAX98357A (màu cyan):
- Vin→5V, GND, BCLK→GPIO17, LRC→GPIO18, DIN→GPIO16, GAIN→GND, SD→3V3, loa 3W

WS2812 NeoPixel ring 12 LED (màu vàng):
- VCC→5V, GND, DIN→GPIO38 qua level shifter 3.3V→5V

MOTOR DRV8833 + N20 300RPM×2 (màu cam):
- VM→5V, VCC→3V3, GND, nSLEEP→3V3
- AIN1→GPIO1, AIN2→GPIO2 → motor TRÁI
- BIN1→GPIO41, BIN2→GPIO42 → motor PHẢI
- Bánh omni phía sau

CẢM BIẾN DIGITAL (màu xám):
- TCRT5000 trái: VCC,GND, DO→GPIO15 (đáy robot)
- TCRT5000 phải: VCC,GND, DO→GPIO7
- Rd-04 radar: VCC,GND, OUT→GPIO40

BỐ CỤC ROBOT nhìn từ trên: màn GC9A01 + vòng LED phía trước, ESP giữa, VL53L0X mũi,
Rd-04 trước, 2 motor N20 2 bên, TCRT đáy, omni sau, pin+buck dưới đáy.

Style: sạch, nền tối hoặc trắng, mũi tên có hướng, ghi rõ từng nhãn in trên breakout board
(VCC, SDA, AIN1...). Tỷ lệ 16:9, độ phân giải cao, không chữ quá nhỏ.
```

---

## Prompt 2 — Chỉ sơ đồ nguồn 2S

```
Vẽ sơ đồ khối nguồn điện robot 2S lithium, infographic tiếng Việt:

[USB Type-C] → [Mạch sạc 2S BM1 cân bằng, KHÔNG bảo vệ xả]
      ↓
[2×18650 nối tiếp] PACK+ / PACK-
      ↓
[Công tắc gạt] — (khuyến nghị cầu chì 2-3A)
      ↓
[MP1584EN buck] OUT = 5.00V
      ├─→ ESP32 VIN
      ├─→ WS2812 VCC (+ tụ 470µF)
      ├─→ DRV8833 VM (+ tụ 100µF)
      └─→ MAX98357A Vin
      │
PACK+ ──[100kΩ]──┬── GPIO10 ESP (đo pin)
                 └──[33kΩ]── GND

ESP 3V3 → cảm biến I2C, LCD, mic, DRV VCC logic

Cảnh báo nổi bật: "Luôn tắt công tắc khi không dùng — mạch sạc không chống xả cạn"
Style kỹ thuật, màu đỏ cho đường nguồn, đen GND chung star-ground.
```

---

## Prompt 3 — Sơ đồ ESP32 header (chân vật lý)

```
Vẽ hình ESP32-S3-DevKitC-1 nhìn từ trên, USB ở dưới, 2 header:

HEADER J1 (trái, 25 chân từ trên xuống):
3V3, 3V3, RST, GPIO4(INMP441 WS), GPIO5(INMP441 SCK), GPIO6(INMP441 SD),
GPIO7(TCRT phải), GPIO15(TCRT trái), GPIO16(MAX98357 DIN), GPIO17(MAX98357 BCLK),
GPIO18(MAX98357 LRC), GPIO8(I2C SDA), GPIO19, GPIO20, GPIO3, GPIO46,
GPIO9(I2C SCL), GPIO10(ADC pin), GPIO11(GC9A01 MOSI), GPIO12(GC9A01 SCK),
GPIO13(GC9A01 DC), GPIO14(GC9A01 RST), 5V, GND, GND

HEADER J3 (phải):
GPIO43, GPIO44, GPIO1(DRV AIN1), GPIO2(DRV AIN2), GPIO42(DRV BIN2), GPIO41(DRV BIN1),
GPIO40(Rd-04 OUT), GPIO39, GPIO38(WS2812 DIN), GPIO37-35, GPIO0, GPIO45,
GPIO48(GC9A01 CS), GPIO47, GPIO21(GC9A01 BL), GND, GND

Tô màu chân đang dùng (xanh=I2C, tím=SPI, cyan=I2S, cam=motor, đỏ=nguồn).
Chân strapping GPIO0,3,45,46 gạch chéo đỏ "KHÔNG DÙNG".
```

---

## Prompt 4 — Poster một trang cho người mới

```
Tạo poster hướng dẫn lắp ráp robot RoboDesk 1 trang A3, chia 4 vùng:

1) BẢNG LINH KIỆN (5 khối): não ESP32-S3, nguồn 2S, nghe/nhìn, motor N20 300RPM, cảm biến
2) SƠ ĐỒ NGUỒN (như prompt 2)
3) SƠ ĐỒ TÍN HIỆU ESP ↔ module (như prompt 1, thu nhỏ)
4) THỨ TỰ LẮP 9 BƯỚC: nguồn→ESP→LCD→I2C→LED→audio→motor→cliff→khung

Phong cách: friendly, icon từng module, số bước lớn, cảnh báo an toàn pin lithium nổi bật.
Ngôn ngữ: tiếng Việt.
```

---

## Giao diện HTML có sẵn trong repo

Mở file trong trình duyệt (không cần cài gì):

```bash
# Linux / Mac
xdg-open tools/wiring_diagram.html

# Hoặc kéo thả file vào Chrome / Edge
```

File: `tools/wiring_diagram.html` — sơ đồ tương tác, bấm module xem bảng chân.
