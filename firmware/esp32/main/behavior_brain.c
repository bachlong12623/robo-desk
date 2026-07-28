#include "behavior_brain.h"
#include "sensors.h"
#include "safety.h"
#include "drive.h"
#include "face_ui.h"
#include "led_ring.h"
#include "voice_solo.h"
#include "power_monitor.h"
#include "app_config.h"
#include "mode_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "brain";
static char s_goal[32];
static int64_t s_last_presence_us;
static int64_t s_last_remind_us;

void behavior_brain_init(void)
{
    memset(s_goal, 0, sizeof(s_goal));
    s_last_presence_us = esp_timer_get_time();
}

void behavior_brain_on_goal(const char *goal)
{
    if (!goal) {
        return;
    }
    strncpy(s_goal, goal, sizeof(s_goal) - 1);
    ESP_LOGI(TAG, "goal=%s", s_goal);
}

static void handle_voice(voice_cmd_t c)
{
    switch (c) {
    case VOICE_CMD_COME:
        behavior_brain_on_goal("approach_user");
        voice_solo_say("Coming");
        break;
    case VOICE_CMD_STOP:
        drive_stop();
        memset(s_goal, 0, sizeof(s_goal));
        voice_solo_say("Stopped");
        break;
    case VOICE_CMD_SLEEP:
        face_ui_set_mood(FACE_SLEEPY);
        drive_stop();
        voice_solo_say("Good night");
        break;
    case VOICE_CMD_PATROL:
        behavior_brain_on_goal("patrol");
        break;
    case VOICE_CMD_SPIN:
        behavior_brain_on_goal("spin");
        break;
    case VOICE_CMD_TEMP: {
        const sensor_snapshot_t *s = sensors_get();
        voice_solo_say("Temperature ready");
        ESP_LOGI(TAG, "temp=%.1f rh=%.1f", s->temp_c, s->rh);
        break;
    }
    case VOICE_CMD_BATTERY: {
        const power_status_t *p = power_monitor_get();
        ESP_LOGI(TAG, "batt=%.0f%%", p->pct);
        voice_solo_say("Battery status");
        break;
    }
    default:
        break;
    }
}

static void run_goal(void)
{
    if (!safety_allows_motion()) {
        drive_stop();
        face_ui_set_mood(FACE_ALERT);
        led_ring_set_mood(FACE_ALERT);
        return;
    }
    if (strcmp(s_goal, "approach_user") == 0) {
        face_ui_set_mood(FACE_HAPPY);
        drive_set(DRIVE_FORWARD, 40);
    } else if (strcmp(s_goal, "patrol") == 0) {
        face_ui_set_mood(FACE_IDLE);
        drive_set(DRIVE_FORWARD, 30);
    } else if (strcmp(s_goal, "spin") == 0) {
        drive_set(DRIVE_SPIN, 35);
    } else if (strcmp(s_goal, "stop") == 0) {
        drive_stop();
        s_goal[0] = 0;
    }
}

static void brain_task(void *arg)
{
    (void)arg;
    for (;;) {
        sensors_poll();
        power_monitor_update();
        safety_update();

        const sensor_snapshot_t *s = sensors_get();
        const safety_state_t *safe = safety_get();
        int64_t now = esp_timer_get_time();

        if (safe->cliff || safe->tip) {
            drive_stop();
            face_ui_set_mood(FACE_PANIC);
            led_ring_set_mood(FACE_PANIC);
            if (safe->cliff) {
                drive_set(DRIVE_BACKWARD, 35);
                vTaskDelay(pdMS_TO_TICKS(400));
                drive_stop();
            }
        }

        if (s->presence) {
            if (now - s_last_presence_us > 3000000 && mode_manager_get() == ROBODESK_MODE_SOLO) {
                face_ui_set_mood(FACE_HAPPY);
                led_ring_set_mood(FACE_HAPPY);
                voice_solo_say("Hello");
            }
            s_last_presence_us = now;
        } else if (now - s_last_presence_us > 120000000) {
            face_ui_set_mood(FACE_SLEEPY);
            led_ring_set_mood(FACE_SLEEPY);
            drive_stop();
        }

        const app_config_t *cfg = app_config_get();
        if (s->presence && s->temp_c >= cfg->comfort_temp_c &&
            (now - s_last_remind_us) > 900000000) {
            voice_solo_say("Please take a break");
            s_last_remind_us = now;
        }

        if (s->btn) {
            voice_solo_say("Muted");
            face_ui_set_mood(FACE_IDLE);
        }

        handle_voice(voice_solo_pop_cmd());
        run_goal();

        /* ToF follow soft limit */
        if (strcmp(s_goal, "approach_user") == 0 && s->tof_mm > 0 && s->tof_mm < 120) {
            drive_stop();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void behavior_brain_task_start(void)
{
    xTaskCreatePinnedToCore(brain_task, "brain", 6144, NULL, 6, NULL, 1);
    ESP_LOGI(TAG, "behavior brain started");
}
