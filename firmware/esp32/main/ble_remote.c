#include "ble_remote.h"
#include "behavior_brain.h"
#include "face_ui.h"
#include "led_ring.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ble";

/*
 * BLE GATT remote for Solo. Bluedroid init is heavy; this task documents the
 * service surface and can be expanded with gatts table matching docs/PROTOCOL.md.
 * Commands accepted conceptually: move, emote, status notify.
 */
static void ble_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "BLE remote placeholder active (enable full GATTS in bring-up)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ble_remote_start(void)
{
    xTaskCreate(ble_task, "ble", 4096, NULL, 3, NULL);
}
