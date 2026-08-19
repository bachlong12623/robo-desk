# Anime face (GC9A01)

Khuôn mặt **phong cách anime**, vẽ thủ tục (ellipse/arc/line) ở độ phân giải đầy đủ
**240×240**, bo tròn theo màn GC9A01. Mắt bóng gradient xanh + highlight trắng,
mí mắt chớp được, lông mày, má hồng. Mọi mood đều có animation (~12 fps).

## Moods

| Mood | Look | Animation |
|---|---|---|
| Idle | Mắt tròn bóng, cười nhẹ | Chớp mắt, liếc qua lại, "thở" |
| Happy | Mắt cong `^`, miệng cười mở + lưỡi, má hồng | Nhún nhảy |
| Sleepy | Mí sập 60%+, miệng chúm | Zzz trôi lên |
| Listen | Mắt mở to, miệng O | Sóng âm 2 bên lan ra |
| Speak | Mắt thường | Miệng đóng/mở liên tục + lưỡi |
| Think | Mắt liếc lên phải, miệng thẳng | 3 chấm suy nghĩ nhấp nháy |
| Alert | Mắt phồng to, miệng O | Dấu `!` vàng nhấp nháy |
| Panic | Mắt X đỏ, miệng zigzag | Rung lắc + `!!` hai bên |
| Sad | Mí xệ, mày cụp, miệng mếu | Giọt lệ rơi lặp |
| Angry | Mí xếch, mày đỏ, miệng mím | Dấu tức giận 井 nhấp nháy |
| Love | Mắt trái tim hồng | Tim đập, tim nhỏ bay lên |
| Excited | Mắt sao vàng ✦ | Sao phồng, sparkle nhấp nháy |
| Dizzy | Mắt xoáy cam | Xoáy quay ngược chiều nhau |
| Plead | Kiểu 🥺: mắt to long lanh, highlight lớn, mày cụp vào trong, miệng chúm | Mắt shimmer nhẹ |

Set qua `face_ui_set_mood()` — cùng hook với LED ring (`behavior_brain`,
`voice_solo`, MQTT emote). Enum mới: `FACE_SAD/ANGRY/LOVE/EXCITED/DIZZY/PLEAD`
(thêm cuối enum, code cũ không đổi).

## Files

- `display_gc9a01.c` — SPI + GC9A01 driver, PSRAM framebuffer
- `face_pixel.c` — renderer (C) — **giữ đồng bộ với `tools/preview_faces.py`**
- `face_ui.c` — render loop ~12 fps
- `face_ui.c` — command/priority state machine nhỏ cho mood/screen timeout
- `tools/preview_faces.py` — renderer Python y hệt, để xem trước không cần phần cứng

## Xem trước (chưa cần ESP/màn)

```bash
# HTML có animation đầy đủ 14 mood — khuyên dùng, mở trong browser
python tools/preview_faces.py --html tools/faces.html

# PNG tĩnh: contact sheet tất cả mood tại 1 frame
python tools/preview_faces.py --sheet sheet.png --frame 12

# Xem nhanh 1 mood trên terminal (ANSI true color)
python tools/preview_faces.py --mood happy
python tools/preview_faces.py --list
```

Nếu màu trên màn thật bị đảo, chỉnh `esp_lcd_panel_invert_color` /
`rgb_ele_order` trong `display_gc9a01.c`.

---

# Info screens (màn hình thông tin)

Ngoài khuôn mặt, màn GC9A01 hiện có **13** màn hình. Trong đó 6 màn local lấy từ cảm biến onboard (BOM §5),
và 7 màn nội dung linked-mode lấy từ Pi 5 qua MQTT:

| Screen | Cảm biến | Nội dung |
|---|---|---|
| `climate` | DHT20 | Nhiệt độ lớn + gauge vòng cung 0–40°C, độ ẩm + icon giọt nước, mặt cười "độ thoải mái" |
| `battery` | ADC pin 2S | Gauge vòng % pin (xanh/vàng/đỏ), % lớn, volt, cảnh báo LOW nhấp nháy |
| `level` | MPU6050 | Thước thủy bong bóng (bubble level), pitch/roll độ, xanh khi cân bằng |
| `radar` | VL53L0X + Rd-04 | Màn quét radar xoay, chấm blip theo khoảng cách, số CM, chữ MOTION khi có người |
| `clock` | NTP | Giờ:phút lớn (chấm hai nhấp nháy), thứ + ngày, 60 chấm giây quanh viền |
| `status` | WiFi/MQTT | SOLO/LINKED, icon WiFi + RSSI, MQTT, uptime |
| `weather` | Open-Meteo (Pi 5) | Forecast hôm nay: icon trời, nhiệt độ hiện tại, min/max, gauge % mưa |
| `aqi` | Open-Meteo AQI (Pi 5) | US AQI, PM2.5, màu chất lượng không khí |
| `news` | RSS (Pi 5) | Headline tin tức mới nhất, 2 dòng |
| `calendar` | `data/calendar.json` (Pi 5) | Sự kiện kế tiếp, giờ bắt đầu, còn bao nhiêu phút |
| `desk` | Radar session + DB (Pi 5) | Tổng phút hôm nay, phiên hiện tại, nhắc nghỉ |
| `morning` | Tổng hợp Pi 5 | Màn chào buổi sáng: thời tiết, AQI, lịch kế tiếp |
| `moon` | Tính offline trên Pi 5 | Pha trăng + % sáng |

## API firmware

```c
face_ui_show_screen(SCREEN_CLIMATE, 15);  // hiện 15s rồi tự về mặt
face_ui_show_screen(SCREEN_CLOCK, 0);     // giữ đến khi gọi face_ui_show_face()
face_ui_show_face();
```

Qua MQTT (topic `deskbuddy/<id>/emote`):

```json
{"screen": "climate", "hold_s": 10}
{"screen": "weather", "hold_s": 20}
{"screen": "morning", "hold_s": 20}
{"screen": "face"}
{"mood": "love"}
```

## Files

- `face_draw.c/.h` — primitives vẽ dùng chung (ellipse, arc, ring gauge, font 5×7)
- `screen_info.c/.h` — 13 màn hình thông tin
- `screen_feed.c/.h` — cache nội dung linked-mode nhận qua MQTT
- `tools/preview_screens.py` — bản Python y hệt để xem trước

## Xem trước

```bash
python tools/preview_screens.py --html tools/screens.html   # 13 màn hình, có animation
python tools/preview_screens.py --sheet screens.png
python tools/preview_screens.py --screen weather            # terminal
```

Lưu ý: font chỉ có A–Z 0–9 và `% : . - + / !`; ký tự `*` vẽ thành dấu độ (°).
Đồng hồ cần SNTP (thêm `esp_sntp` khi WiFi lên, chưa bật sẵn).

## Pi 5 content feed

Pi 5 publish topic `deskbuddy/<id>/screen_data` với các field như:

- `forecast_temp_c`, `forecast_min_c`, `forecast_max_c`, `forecast_rain_pct`, `forecast_weather_code`, `forecast_label`
- `aqi_us`, `aqi_pm25`, `aqi_label`
- `news_title`
- `calendar_has`, `calendar_title`, `calendar_hh`, `calendar_mm`, `calendar_in_min`
- `desk_today_min`, `desk_session_min`, `desk_break_due`
- `morning_line1`, `morning_line2`
- `moon_phase_pct`, `moon_illum_pct`, `moon_label`
- `version`, `ts` để ESP phát hiện feed cũ/stale

Firmware local sensor snapshot cũng đã có cờ validity/staleness cho `climate`, `imu`, `tof`,
để các màn `climate/level/radar` và safety không tiêu thụ dữ liệu giả khi cảm biến mất ACK.

Biến môi trường Pi 5 quan trọng:

- `LATITUDE`, `LONGITUDE`, `TIMEZONE` — cho weather/AQI
- `NEWS_RSS_URL` — nguồn tin
- `CALENDAR_FILE` — file JSON lịch local
