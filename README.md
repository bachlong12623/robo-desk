# RoboDesk

Robot bàn làm việc (desk companion): **ESP32-S3** tự chạy (Solo) hoặc kết nối **Raspberry Pi 5** (Linked) để có AI, thời tiết, tin tức.

## Linh kiện (BOM)

Danh sách đầy đủ: **`BOM.txt`** — 5 khối chính (não, nguồn 2S, nghe/nhìn, truyền động, cảm biến).

| Khối | Thành phần chính |
|---|---|
| 1. Xử lý | ESP32-S3 DevKit N16R8 |
| 2. Nguồn | 2×18650 2S, sạc Type-C BM1, MP1584EN 5V, công tắc gạt |
| 3. Nghe/Nhìn | GC9A01, INMP441, MAX98357A, loa 3W, WS2812 |
| 4. Truyền động | N20 300RPM ×2, DRV8833, bánh xe + omni |
| 5. Cảm biến | TCRT5000 ×2, VL53L0X, Rd-04, MPU6050, DHT20 |

**Bắt đầu đấu dây:** đọc [`docs/WIRING.md`](docs/WIRING.md) — hướng dẫn chi tiết cho người mới, có sơ đồ.

## Cấu trúc repo

```
BOM.txt                     # Danh sách linh kiện
docs/
  WIRING.md                 # ★ Hướng dẫn đấu dây (người mới)
  PINMAP.md                 # Bảng chân GPIO
  POWER.md                  # Sơ đồ nguồn 2S
  BRINGUP_CHECKLIST.md      # Checklist test từng bước
  PROTOCOL.md               # MQTT / WebSocket / BLE
  FACE_PIXEL.md             # Biểu cảm & màn hình GC9A01
firmware/esp32/             # Firmware ESP-IDF
server/pi5/                 # Pi 5: Mosquitto + orchestrator (Linked mode)
tools/                      # Preview biểu cảm/màn hình trên máy tính
```

## Xem biểu cảm (chưa cần lắp phần cứng)

```bash
python tools/preview_faces.py --html tools/faces.html
python tools/preview_screens.py --html tools/screens.html
python tools/preview_faces.py --mood happy
```

## Firmware ESP32-S3

```bash
cd firmware/esp32
idf.py set-target esp32s3
idf.py build flash monitor
```

Yêu cầu: ESP-IDF ≥ 5.1, board N16R8 (PSRAM octal).

Tính năng firmware:
- Đọc cảm biến thật (DHT20, MPU6050, VL53L0X) với cờ `valid/stale`
- Biểu cảm GC9A01 + 13 màn hình thông tin
- An toàn: cliff TCRT, shock MPU, pin yếu — chặn motor trên ESP
- Solo ↔ Linked tự chuyển khi mất Pi

## Pi 5 server (Linked mode — tùy chọn)

```bash
cd server/pi5
docker compose up -d --build
curl http://localhost:8080/health
```

- MQTT `:1883`, HTTP `:8080`, audio WS `:8081`
- Weather, AQI, RSS news, calendar, desk tracker, morning greeting

## Kiểm tra phần mềm

```bash
python -m unittest tools.tests.test_previews
python -m unittest server.pi5.orchestrator.tests.test_main
```

## An toàn

- Mạch sạc **không bảo vệ xả** → luôn dùng công tắc gạt, không để pin cạn
- Motor chỉ test duty thấp khi chưa hoàn thiện khung
- Mất Pi → robot tự về chế độ **Solo**
