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
static int64_t s_goal_deadline_us;

void behavior_brain_init(void)
{
    memset(s_goal, 0, sizeof(s_goal));
    s_last_presence_us = esp_timer_get_time();
    s_goal_deadline_us = 0;
}

void behavior_brain_on_goal(const char *goal)
{
    behavior_brain_on_goal_ttl(goal, 30000);
}

void behavior_brain_on_goal_ttl(const char *goal, uint32_t ttl_ms)
{
    if (!goal) {
        return;
    }
    strncpy(s_goal, goal, sizeof(s_goal) - 1);
    s_goal_deadline_us = ttl_ms ? (esp_timer_get_time() + ((int64_t)ttl_ms * 1000LL)) : 0;
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
        s_goal_deadline_us = 0;
        voice_solo_say("Stopped");
        break;
    case VOICE_CMD_SLEEP:
        face_ui_set_mood_prio(FACE_SLEEPY, FACE_UI_PRIO_LOCAL, 2);
        drive_stop();
        s_goal_deadline_us = 0;
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
        voice_solo_say(s->climate_valid ? "Temperature ready" : "Climate sensor offline");
        ESP_LOGI(TAG, "temp=%.1f rh=%.1f valid=%d", s->temp_c, s->rh, s->climate_valid);
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
        face_ui_set_mood_prio(FACE_ALERT, FACE_UI_PRIO_SAFETY, 2);
        led_ring_set_mood(FACE_ALERT);
        return;
    }
    if (s_goal_deadline_us && esp_timer_get_time() > s_goal_deadline_us) {
        drive_stop();
        s_goal[0] = 0;
        s_goal_deadline_us = 0;
        return;
    }
    if (strcmp(s_goal, "approach_user") == 0) {
        face_ui_set_mood_prio(FACE_HAPPY, FACE_UI_PRIO_LOCAL, 1);
        if (sensors_get()->tof_valid && !sensors_get()->tof_stale) {
            drive_set(DRIVE_FORWARD, 40);
        } else {
            drive_stop();
        }
    } else if (strcmp(s_goal, "patrol") == 0) {
        face_ui_set_mood_prio(FACE_IDLE, FACE_UI_PRIO_LOCAL, 1);
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
        const power_status_t *p = power_monitor_get();
        int64_t now = esp_timer_get_time();

        mode_manager_set_low_power(p->valid && p->low);

        if (safe->cliff || safe->tip) {
            drive_stop();
            face_ui_set_mood_prio(FACE_PANIC, FACE_UI_PRIO_SAFETY, 3);
            led_ring_set_mood(FACE_PANIC);
            if (safe->cliff) {
                drive_set(DRIVE_BACKWARD, 35);
                vTaskDelay(pdMS_TO_TICKS(400));
                drive_stop();
            }
        }

        if (s->presence) {
            if (now - s_last_presence_us > 3000000 && mode_manager_get() == ROBODESK_MODE_SOLO) {
                face_ui_set_mood_prio(FACE_HAPPY, FACE_UI_PRIO_LOCAL, 2);
                led_ring_set_mood(FACE_HAPPY);
                voice_solo_say("Hello");
            }
            s_last_presence_us = now;
        } else if (now - s_last_presence_us > 120000000) {
            face_ui_set_mood_prio(FACE_SLEEPY, FACE_UI_PRIO_IDLE, 0);
            led_ring_set_mood(FACE_SLEEPY);
            drive_stop();
        }

        const app_config_t *cfg = app_config_get();
        if (s->presence && s->climate_valid && s->temp_c >= cfg->comfort_temp_c &&
            (now - s_last_remind_us) > 900000000) {
            voice_solo_say("Please take a break");
            s_last_remind_us = now;
        }

        if (s->btn) {
            mode_manager_set_muted(true);
            voice_solo_say("Muted");
            face_ui_set_mood_prio(FACE_IDLE, FACE_UI_PRIO_LOCAL, 1);
        } else {
            mode_manager_set_muted(false);
        }

        handle_voice(voice_solo_pop_cmd());
        run_goal();

        /* ToF follow soft limit */
        if (strcmp(s_goal, "approach_user") == 0 && s->tof_valid && !s->tof_stale &&
            s->tof_mm > 0 && s->tof_mm < 120) {
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
