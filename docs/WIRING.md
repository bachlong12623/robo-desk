# Breadboard / wiring guide

**Kho:** đang có **A + D** thôi. Mục **B/C chưa có** → làm theo Phase 0 trong `POWER.md`.

Follow `docs/PINMAP.md` and `docs/POWER.md`. Star-ground near the buck module.

## Mechanical layout (top view)

```
              [GC9A01 + WS2812 ring]
                     |
              [ESP32-S3 DevKit]
     VL53L0X (front)     Rd-04 (up/front)
   [N20 L]========chassis========[N20 R]
        TCRT-L   TCRT-R (underside)
              omni caster (rear)
         DHT20 (side)  MPU6050 (center)
         2S pack + charger + bucks
```

## Block wiring

### Power
- PACK+ → Switch → Fuse → MP1584 VIN+
- PACK- → all GND star
- MP1584 OUT+ → 5V rail bus
- 5V → AMS1117/MP1584#2 IN → 3.3V rail bus
- 5V → ESP VIN, WS2812, MAX98357A, DRV8833 VM, Rd-04(if 5V)
- 3.3V → I2C sensors, GC9A01, INMP441, DRV8833 VCC

### I2C bus
ESP 8/9 → MPU + DHT20 + VL53L0X (and INA219 if fitted)

### SPI LCD
ESP 12/11/48/13/14/21 → GC9A01

### I2S
Mic: 5/4/6 → INMP441; Amp: 17/18/16 → MAX98357A → speaker

### Motors
GPIO 1/2 → DRV AIN1/2 → N20 L  
GPIO 41/42 → DRV BIN1/2 → N20 R

### Safety / presence
TCRT L/R DO → GPIO 15/7  
Rd-04 → GPIO 40  
BTN → GPIO 47  
WS2812 DIN ← level shift ← GPIO 38

## Bring-up order

1. Power rails only  
2. ESP serial hello  
3. LCD face smoke test  
4. WS2812  
5. I2C scan (expect 0x68, 0x38, 0x29)  
6. Audio loopback beep  
7. Motors @ low duty with wheels off desk  
8. Cliff sensors over edge  
9. Full stack
