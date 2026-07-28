#include "led_ring.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "led";
static face_mood_t s_mood = FACE_IDLE;
static uint8_t s_amp;
static uint8_t s_rgb[WS2812_COUNT][3];

void led_ring_init(void)
{
    gpio_reset_pin(PIN_WS2812);
    gpio_set_direction(PIN_WS2812, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_WS2812, 0);
    memset(s_rgb, 0, sizeof(s_rgb));
    ESP_LOGI(TAG, "WS2812 pin %d (RMT encoder fill in bring-up)", PIN_WS2812);
}

void led_ring_set_mood(face_mood_t mood)
{
    s_mood = mood;
}

void led_ring_set_amplitude(uint8_t amp)
{
    s_amp = amp;
}

static void palette(uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (s_mood) {
    case FACE_HAPPY: *r = 40; *g = 200; *b = 40; break;
    case FACE_ALERT: *r = 220; *g = 80; *b = 0; break;
    case FACE_PANIC: *r = 255; *g = 0; *b = 0; break;
    case FACE_LISTEN: *r = 40; *g = 120; *b = 255; break;
    case FACE_SPEAK: *r = 180; *g = 80; *b = 255; break;
    case FACE_THINK: *r = 80; *g = 80; *b = 200; break;
    case FACE_SLEEPY: *r = 10; *g = 10; *b = 40; break;
    default: *r = 20; *g = 40; *b = 30; break;
    }
    uint8_t scale = 40 + s_amp;
    *r = (*r * scale) / 255;
    *g = (*g * scale) / 255;
    *b = (*b * scale) / 255;
}

static void led_task(void *arg)
{
    (void)arg;
    uint8_t phase = 0;
    for (;;) {
        uint8_t r, g, b;
        palette(&r, &g, &b);
        for (int i = 0; i < WS2812_COUNT; ++i) {
            int idx = (i + phase) % WS2812_COUNT;
            s_rgb[idx][0] = r;
            s_rgb[idx][1] = g;
            s_rgb[idx][2] = b;
        }
        phase++;
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

void led_ring_task_start(void)
{
    xTaskCreatePinnedToCore(led_task, "led", 3072, NULL, 3, NULL, 1);
}
