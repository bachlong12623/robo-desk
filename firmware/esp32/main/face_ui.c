#include "face_ui.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "face";
static face_mood_t s_mood = FACE_IDLE;

void face_ui_init(void)
{
    gpio_reset_pin(PIN_LCD_BL);
    gpio_set_direction(PIN_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_BL, 1);
    gpio_reset_pin(PIN_LCD_RST);
    gpio_set_direction(PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_LCD_RST, 1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI init: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "GC9A01 SPI bus ready (LVGL bind in bring-up)");
}

void face_ui_set_mood(face_mood_t mood)
{
    s_mood = mood;
}

static void face_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* Placeholder: mood drives LED sync via led_ring; LVGL sprites later */
        ESP_LOGD(TAG, "mood=%d", (int)s_mood);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void face_ui_task_start(void)
{
    xTaskCreatePinnedToCore(face_task, "face", 4096, NULL, 4, NULL, 1);
}
