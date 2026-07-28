# ESP32-S3 DevKit N16R8 — Pin Map (chốt)

Tránh strapping: GPIO0, GPIO3, GPIO45, GPIO46.
Battery ADC dùng ADC1 (GPIO1–10) — không dùng ADC2 khi WiFi bật.

| Function | GPIO | Notes |
|---|---:|---|
| I2C SDA | 8 | MPU6050 + DHT20 + VL53L0X (+ optional INA219) |
| I2C SCL | 9 | Pull-up 4.7k to 3.3V if modules lack them |
| LCD SCLK | 12 | GC9A01 |
| LCD MOSI | 11 | GC9A01 |
| LCD CS | 48 | GC9A01 |
| LCD DC | 13 | |
| LCD RST | 14 | |
| LCD BL | 21 | PWM backlight |
| I2S MIC BCLK | 5 | INMP441 SCK |
| I2S MIC WS | 4 | INMP441 WS |
| I2S MIC DIN | 6 | INMP441 SD |
| I2S AMP BCLK | 17 | MAX98357A BCLK |
| I2S AMP LRCLK | 18 | MAX98357A LRC |
| I2S AMP DOUT | 16 | MAX98357A DIN |
| WS2812 DIN | 38 | Via 3.3→5V level shift |
| DRV AIN1 | 1 | Motor Left |
| DRV AIN2 | 2 | Motor Left |
| DRV BIN1 | 41 | Motor Right |
| DRV BIN2 | 42 | Motor Right |
| TCRT Left DO | 15 | Cliff |
| TCRT Right DO | 7 | Cliff |
| Rd-04 | 40 | Digital OUT or UART RX (module variant) |
| Rd-04 TX (UART) | 39 | Only if UART variant |
| USER BTN | 47 | INPUT_PULLUP, mute/snooze |
| BAT ADC | 10 | Divider from PACK+ (ADC1_CH9) |

## I2C addresses

| Device | Addr |
|---|---|
| MPU6050 | 0x68 |
| DHT20 | 0x38 |
| VL53L0X | 0x29 |
| INA219 (optional) | 0x40 |

## Firmware header

See `firmware/esp32/main/board_pins.h` — single source of truth.
