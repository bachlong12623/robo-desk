#include "face_ui.h"
#include "display_gc9a01.h"
#include "face_pixel.h"
#include "screen_info.h"
#include "screen_feed.h"
#include "sensors.h"
#include "power_monitor.h"
#include "mode_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include <math.h>
#include <string.h>
#include <time.h>

static const char *TAG = "face";

typedef enum {
    FACE_UI_CMD_MOOD = 0,
    FACE_UI_CMD_SCREEN,
    FACE_UI_CMD_FACE,
} face_ui_cmd_type_t;

typedef struct {
    face_ui_cmd_type_t type;
    face_ui_priority_t prio;
    face_mood_t mood;
    info_screen_t screen;
    uint32_t hold_s;
} face_ui_cmd_t;

typedef struct {
    bool show_screen;
    face_ui_priority_t prio;
    face_mood_t mood;
    info_screen_t screen;
    int64_t until_us;
    uint32_t frame0;
} face_ui_state_t;

static QueueHandle_t s_cmd_q;
static face_ui_state_t s_state = {
    .show_screen = false,
    .prio = FACE_UI_PRIO_IDLE,
    .mood = FACE_IDLE,
    .screen = SCREEN_CLIMATE,
    .until_us = 0,
    .frame0 = 0,
};

static void state_reset(face_ui_state_t *st, uint32_t frame_now)
{
    st->show_screen = false;
    st->prio = FACE_UI_PRIO_IDLE;
    st->mood = FACE_IDLE;
    st->until_us = 0;
    st->frame0 = frame_now;
}

static void state_apply(face_ui_state_t *st, const face_ui_cmd_t *cmd, uint32_t frame_now)
{
    if (!st || !cmd) {
        return;
    }
    if (cmd->prio < st->prio && cmd->prio != FACE_UI_PRIO_SAFETY) {
        return;
    }
    if (cmd->type == FACE_UI_CMD_FACE) {
        state_reset(st, frame_now);
        return;
    }
    st->prio = cmd->prio;
    st->until_us = cmd->hold_s ? esp_timer_get_time() + ((int64_t)cmd->hold_s * 1000000LL) : 0;
    st->frame0 = frame_now;
    if (cmd->type == FACE_UI_CMD_MOOD) {
        st->show_screen = false;
        st->mood = cmd->mood;
    } else if (cmd->type == FACE_UI_CMD_SCREEN) {
        st->show_screen = true;
        st->screen = cmd->screen;
    }
}

static void push_cmd(face_ui_cmd_t cmd)
{
    if (!s_cmd_q) {
        return;
    }
    xQueueSend(s_cmd_q, &cmd, 0);
}

void face_ui_init(void)
{
    s_cmd_q = xQueueCreate(16, sizeof(face_ui_cmd_t));
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
    face_ui_set_mood_prio(mood, FACE_UI_PRIO_IDLE, 0);
}

void face_ui_set_mood_prio(face_mood_t mood, face_ui_priority_t prio, uint32_t hold_s)
{
    face_ui_cmd_t cmd = {
        .type = FACE_UI_CMD_MOOD,
        .prio = prio,
        .mood = mood,
        .hold_s = hold_s,
    };
    push_cmd(cmd);
}

face_mood_t face_ui_get_mood(void)
{
    return s_state.mood;
}

void face_ui_show_screen(info_screen_t screen, uint32_t hold_s)
{
    face_ui_show_screen_prio(screen, hold_s, FACE_UI_PRIO_REMOTE);
}

void face_ui_show_screen_prio(info_screen_t screen, uint32_t hold_s, face_ui_priority_t prio)
{
    if (screen >= SCREEN_COUNT) {
        return;
    }
    face_ui_cmd_t cmd = {
        .type = FACE_UI_CMD_SCREEN,
        .prio = prio,
        .screen = screen,
        .hold_s = hold_s,
    };
    push_cmd(cmd);
}

void face_ui_show_face(void)
{
    face_ui_cmd_t cmd = { .type = FACE_UI_CMD_FACE, .prio = FACE_UI_PRIO_SAFETY };
    push_cmd(cmd);
}

static void collect_screen_data(screen_info_data_t *d)
{
    memset(d, 0, sizeof(*d));
    const sensor_snapshot_t *sn = sensors_get();
    const power_status_t *pw = power_monitor_get();

    d->temp_c = sn->temp_c;
    d->rh = sn->rh;
    d->climate_valid = sn->climate_valid && !sn->climate_stale;
    d->tof_mm = sn->tof_mm;
    d->tof_valid = sn->tof_valid && !sn->tof_stale;
    d->presence = sn->presence;

    d->batt_v = pw->pack_v;
    d->batt_pct = pw->pct;
    d->batt_low = (pw->low || pw->critical) && pw->valid;
    d->batt_valid = pw->valid;

    /* tilt from accelerometer */
    float az = (fabsf(sn->az) < 0.05f) ? 0.05f : sn->az;
    d->pitch_deg = atan2f(-sn->ax, sqrtf(sn->ay * sn->ay + az * az)) * 180.f / 3.14159265f;
    d->roll_deg = atan2f(sn->ay, az) * 180.f / 3.14159265f;
    d->imu_valid = sn->imu_valid && !sn->imu_stale;

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
    uint32_t frame_now = 0;
    if (!fb) {
        ESP_LOGE(TAG, "no framebuffer");
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        face_ui_cmd_t cmd;
        while (s_cmd_q && xQueueReceive(s_cmd_q, &cmd, 0) == pdTRUE) {
            state_apply(&s_state, &cmd, frame_now);
        }
        if (s_state.until_us && esp_timer_get_time() > s_state.until_us) {
            state_reset(&s_state, frame_now);
        }
        if (s_state.show_screen) {
            screen_info_data_t d;
            collect_screen_data(&d);
            screen_info_render(s_state.screen, &d, frame_now - s_state.frame0, fb,
                               DISPLAY_W, DISPLAY_H);
        } else {
            face_pixel_render(s_state.mood, frame_now - s_state.frame0, fb, DISPLAY_W, DISPLAY_H);
        }
        display_gc9a01_blit(fb);
        frame_now++;
        /* ~12 fps — smooth enough for blink/speak anim */
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

void face_ui_task_start(void)
{
    xTaskCreatePinnedToCore(face_task, "face", 6144, NULL, 4, NULL, 1);
}
