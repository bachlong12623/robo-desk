#include "voice_solo.h"
#include "audio_path.h"
#include "face_ui.h"
#include "led_ring.h"
#include "mode_manager.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "voice";
static QueueHandle_t s_q;

/*
 * Solo voice stack: ESP-SR WakeNet + MultiNet hooks.
 * Until esp-sr component is linked, we expose the command API and a
 * demo path via button long-press / MQTT test inject.
 */
void voice_solo_init(void)
{
    s_q = xQueueCreate(8, sizeof(voice_cmd_t));
    ESP_LOGI(TAG, "Solo voice ready (ESP-SR integration point)");
}

void voice_solo_say(const char *text)
{
    face_ui_set_mood(FACE_SPEAK);
    led_ring_set_mood(FACE_SPEAK);
    audio_path_speak_text_stub(text);
}

voice_cmd_t voice_solo_pop_cmd(void)
{
    voice_cmd_t c = VOICE_CMD_NONE;
    if (s_q) {
        xQueueReceive(s_q, &c, 0);
    }
    return c;
}

void voice_solo_inject(voice_cmd_t c)
{
    if (s_q) {
        xQueueSend(s_q, &c, 0);
    }
}

static void voice_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (mode_manager_get() == ROBODESK_MODE_SOLO && app_config_get()->voice_enabled) {
            face_ui_set_mood(FACE_LISTEN);
            /* ESP-SR would block on AFE here; keep listen mood alive */
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void voice_solo_task_start(void)
{
    xTaskCreatePinnedToCore(voice_task, "voice", 6144, NULL, 5, NULL, 1);
}
