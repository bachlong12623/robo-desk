#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "mode_manager.h"
#include "power_monitor.h"
#include "sensors.h"
#include "safety.h"
#include "drive.h"
#include "face_ui.h"
#include "led_ring.h"
#include "audio_path.h"
#include "voice_solo.h"
#include "behavior_brain.h"
#include "net_mqtt.h"
#include "net_ws_audio.h"
#include "ble_remote.h"
#include "asset_ota.h"

static const char *TAG = "main";

static void telemetry_task(void *arg)
{
    (void)arg;
    for (;;) {
        net_mqtt_publish_telemetry();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "RoboDesk boot — Solo-first");
    ESP_ERROR_CHECK(nvs_flash_init());

    app_config_init();
    power_monitor_init();
    sensors_init();
    safety_init();
    drive_init();
    face_ui_init();
    led_ring_init();
    audio_path_init();
    voice_solo_init();
    behavior_brain_init();

    mode_manager_start();
    drive_task_start();
    face_ui_task_start();
    led_ring_task_start();
    voice_solo_task_start();
    behavior_brain_task_start();

    net_wifi_start();
    net_mqtt_start();
    net_ws_audio_start();
    ble_remote_start();
    asset_ota_start();

    xTaskCreate(telemetry_task, "telem", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "all tasks launched");
}
