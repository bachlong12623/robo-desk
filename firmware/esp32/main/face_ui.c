#include "face_ui.h"
#include "display_gc9a01.h"
#include "face_pixel.h"
#include "screen_info.h"
#include "screen_feed.h"
#include "sensors.h"
#include "power_monitor.h"
#include "mode_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include <math.h>
#include <time.h>

static const char *TAG = "face";
static volatile face_mood_t s_mood = FACE_IDLE;
static volatile int s_screen = -1;              /* -1 = face mode */
static volatile int64_t s_screen_until_us = 0;  /* 0 = no timeout */
static uint32_t s_frame = 0;

void face_ui_init(void)
{
    if (!display_gc9a01_init()) {
        ESP_LOGE(TAG, "display init failed");
        return;
    }
    uint16_t *fb = display_gc9a01_framebuffer();
    if (fb) {
        face_pixel_render(FACE_IDLE, 0, fb, DISPLAY_W, DISPLAY_H);
        display_gc9a01_blit(fb);
    }
    ESP_LOGI(TAG, "anime face UI ready");
}

void face_ui_set_mood(face_mood_t mood)
{
    s_mood = mood;
}

face_mood_t face_ui_get_mood(void)
{
    return s_mood;
}

void face_ui_show_screen(info_screen_t screen, uint32_t hold_s)
{
    if (screen >= SCREEN_COUNT) {
        return;
    }
    s_screen = (int)screen;
    s_screen_until_us = hold_s ? esp_timer_get_time() + (int64_t)hold_s * 1000000 : 0;
    s_frame = 0;
}

void face_ui_show_face(void)
{
    s_screen = -1;
}

static void collect_screen_data(screen_info_data_t *d)
{
    const sensor_snapshot_t *sn = sensors_get();
    const power_status_t *pw = power_monitor_get();

    d->temp_c = sn->temp_c;
    d->rh = sn->rh;
    d->tof_mm = sn->tof_mm;
    d->presence = sn->presence;

    d->batt_v = pw->pack_v;
    d->batt_pct = pw->pct;
    d->batt_low = pw->low || pw->critical;

    /* tilt from accelerometer */
    float az = (fabsf(sn->az) < 0.05f) ? 0.05f : sn->az;
    d->pitch_deg = atan2f(-sn->ax, sqrtf(sn->ay * sn->ay + az * az)) * 180.f / 3.14159265f;
    d->roll_deg = atan2f(sn->ay, az) * 180.f / 3.14159265f;

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    d->hh = tm_now.tm_hour;
    d->mm = tm_now.tm_min;
    d->ss = tm_now.tm_sec;
    d->day = tm_now.tm_mday;
    d->mon = tm_now.tm_mon + 1;
    d->dow = tm_now.tm_wday;

    d->linked = mode_manager_get() == ROBODESK_MODE_LINKED;
    wifi_ap_record_t ap;
    d->rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
    d->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    screen_feed_merge(d);
}

static void face_task(void *arg)
{
    (void)arg;
    uint16_t *fb = display_gc9a01_framebuffer();
    if (!fb) {
        ESP_LOGE(TAG, "no framebuffer");
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        int screen = s_screen;
        if (screen >= 0 && s_screen_until_us != 0 &&
            esp_timer_get_time() > s_screen_until_us) {
            s_screen = -1;
            screen = -1;
        }
        if (screen >= 0) {
            screen_info_data_t d;
            collect_screen_data(&d);
            screen_info_render((info_screen_t)screen, &d, s_frame, fb,
                               DISPLAY_W, DISPLAY_H);
        } else {
            face_pixel_render(s_mood, s_frame, fb, DISPLAY_W, DISPLAY_H);
        }
        display_gc9a01_blit(fb);
        s_frame++;
        /* ~12 fps — smooth enough for blink/speak anim */
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

void face_ui_task_start(void)
{
    xTaskCreatePinnedToCore(face_task, "face", 6144, NULL, 4, NULL, 1);
}
