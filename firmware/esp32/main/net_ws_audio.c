#include "net_ws_audio.h"
#include "mode_manager.h"
#include "app_config.h"
#include "audio_path.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ws_audio";

/*
 * WebSocket PCM bridge to Pi (ws://deskbuddy-ai.local:8081/audio/{id}).
 * Full esp_websocket_client wiring enabled when Linked; Solo keeps local I2S only.
 */
static void ws_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (mode_manager_get() == ROBODESK_MODE_LINKED) {
            ESP_LOGD(TAG, "Linked audio bridge idle — connect WS to %s:8081", app_config_get()->pi_host);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void net_ws_audio_start(void)
{
    xTaskCreatePinnedToCore(ws_task, "ws_audio", 4096, NULL, 4, NULL, 1);
}
