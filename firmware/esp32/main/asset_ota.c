#include "asset_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "asset";
static char s_url[192];

void asset_ota_on_manifest(const char *url)
{
    if (!url) {
        return;
    }
    strncpy(s_url, url, sizeof(s_url) - 1);
    ESP_LOGI(TAG, "asset queued: %s", s_url);
}

static void asset_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (s_url[0]) {
            ESP_LOGI(TAG, "download asset pack from %s (SPIFFS/LittleFS write next)", s_url);
            s_url[0] = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void asset_ota_start(void)
{
    xTaskCreate(asset_task, "asset", 4096, NULL, 3, NULL);
}
