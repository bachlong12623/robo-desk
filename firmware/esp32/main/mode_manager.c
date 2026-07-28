#include "mode_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mode";
static robodesk_mode_t s_mode = ROBODESK_MODE_SOLO;
static int64_t s_last_hb_us;

void mode_manager_on_pi_heartbeat(void)
{
    s_last_hb_us = esp_timer_get_time();
    if (s_mode != ROBODESK_MODE_LINKED) {
        s_mode = ROBODESK_MODE_LINKED;
        ESP_LOGI(TAG, "Linked: Pi heartbeat OK");
    }
}

void mode_manager_force_solo(void)
{
    s_mode = ROBODESK_MODE_SOLO;
}

robodesk_mode_t mode_manager_get(void)
{
    return s_mode;
}

static void mode_task(void *arg)
{
    (void)arg;
    s_last_hb_us = 0;
    for (;;) {
        int64_t now = esp_timer_get_time();
        if (s_mode == ROBODESK_MODE_LINKED && s_last_hb_us > 0) {
            if ((now - s_last_hb_us) > 6000000) { /* 6s = 3 missed @2s */
                s_mode = ROBODESK_MODE_SOLO;
                ESP_LOGW(TAG, "Pi heartbeat lost → Solo");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void mode_manager_start(void)
{
    xTaskCreatePinnedToCore(mode_task, "mode", 3072, NULL, 5, NULL, 1);
}
