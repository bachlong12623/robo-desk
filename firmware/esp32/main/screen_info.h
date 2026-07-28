#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Info screens on GC9A01 driven by BOM section-A sensors.
 * Rendering mirrors tools/preview_screens.py — keep both in sync. */

typedef enum {
    SCREEN_CLIMATE = 0, /* DHT20: temp + humidity + comfort */
    SCREEN_BATTERY,     /* pack ADC: SoC gauge + volts */
    SCREEN_LEVEL,       /* MPU6050: bubble level */
    SCREEN_RADAR,       /* VL53L0X distance + Rd-04 motion */
    SCREEN_CLOCK,       /* NTP time + date */
    SCREEN_STATUS,      /* mode / WiFi / MQTT / uptime */
    SCREEN_WEATHER,     /* forecast from Open-Meteo */
    SCREEN_AQI,         /* air quality from Open-Meteo AQI */
    SCREEN_NEWS,        /* headline from RSS */
    SCREEN_CALENDAR,    /* next calendar item */
    SCREEN_DESK,        /* desk time tracker */
    SCREEN_MORNING,     /* morning greeting summary */
    SCREEN_MOON,        /* moon phase */
    SCREEN_COUNT,
} info_screen_t;

typedef struct {
    float temp_c;
    float rh;
    bool climate_valid;
    float batt_v;
    float batt_pct;
    bool batt_low;
    bool batt_valid;
    float pitch_deg;
    float roll_deg;
    bool imu_valid;
    uint16_t tof_mm;
    bool tof_valid;
    bool presence;
    int hh, mm, ss;
    int day, mon, dow;    /* dow: 0=Sun */
    bool linked;
    int rssi;             /* dBm, 0 = unknown */
    uint32_t uptime_s;
    bool feed_fresh;
    uint32_t feed_age_s;

    float forecast_temp_c;
    float forecast_min_c;
    float forecast_max_c;
    int forecast_rain_pct;
    int forecast_weather_code;
    char forecast_label[24];

    int aqi_us;
    float aqi_pm25;
    char aqi_label[16];

    char news_title[96];

    bool calendar_has;
    int calendar_hh, calendar_mm;
    int calendar_in_min;
    char calendar_title[64];

    int desk_today_min;
    int desk_session_min;
    bool desk_break_due;

    bool morning_pending;
    char morning_line1[48];
    char morning_line2[48];

    int moon_phase_pct;  /* 0=new, 50=full, 100=new again */
    int moon_illum_pct;
    char moon_label[24];
} screen_info_data_t;

/* Map name ("climate", "battery", ...) to enum; returns SCREEN_COUNT if unknown. */
info_screen_t screen_info_by_name(const char *name);

void screen_info_render(info_screen_t screen, const screen_info_data_t *d,
                        uint32_t frame, uint16_t *fb, int w, int h);
