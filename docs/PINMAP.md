# Bản đồ chân ESP32-S3 (RoboDesk)

Nguồn chân trong firmware: `firmware/esp32/main/board_pins.h`.

**Không dùng** các chân strapping: GPIO0, GPIO3, GPIO45, GPIO46.

Đo pin dùng **ADC1** (GPIO1–10) — không dùng ADC2 khi WiFi bật.

## Bảng chân

| Chức năng | GPIO | Module | Ghi chú |
|---|---:|---|---|
| I2C SDA | 8 | MPU6050, DHT20, VL53L0X | Bus chung |
| I2C SCL | 9 | MPU6050, DHT20, VL53L0X | Pull-up 4.7k nếu module thiếu |
| LCD SCLK | 12 | GC9A01 | SPI |
| LCD MOSI | 11 | GC9A01 | SPI |
| LCD CS | 48 | GC9A01 | |
| LCD DC | 13 | GC9A01 | |
| LCD RST | 14 | GC9A01 | |
| LCD BL | 21 | GC9A01 | PWM đèn nền |
| I2S mic BCLK | 5 | INMP441 | SCK |
| I2S mic WS | 4 | INMP441 | WS/LRCLK |
| I2S mic DIN | 6 | INMP441 | SD (data out) |
| I2S amp BCLK | 17 | MAX98357A | |
| I2S amp LRCLK | 18 | MAX98357A | LRC |
| I2S amp DOUT | 16 | MAX98357A | DIN |
| WS2812 DIN | 38 | NeoPixel ring | Qua level shifter 3.3→5V |
| Motor trái AIN1 | 1 | DRV8833 | PWM |
| Motor trái AIN2 | 2 | DRV8833 | PWM |
| Motor phải BIN1 | 41 | DRV8833 | PWM |
| Motor phải BIN2 | 42 | DRV8833 | PWM |
| Cliff trái | 15 | TCRT5000 | DO (digital) |
| Cliff phải | 7 | TCRT5000 | DO (digital) |
| Presence | 40 | Rd-04 | OUT digital (pull-up) |
| Rd-04 UART TX | 39 | Rd-04 | Chỉ nếu module dùng UART |
| Đo pin ADC | 10 | Chia áp 100k/33k | Từ PACK+ |
| Nút user (tùy chọn) | 47 | Nút tactile | Không có trong BOM — có thể bỏ qua |

## Địa chỉ I2C

| Thiết bị | Địa chỉ |
|---|---|
| MPU6050 | 0x68 |
| DHT20 | 0x38 |
| VL53L0X | 0x29 |

Khi scan I2C thành công bạn sẽ thấy đủ 3 địa chỉ trên.
