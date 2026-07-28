#include "audio_path.h"
#include "board_pins.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "audio";
static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;

void audio_path_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_AMP_BCLK,
            .ws = PIN_I2S_AMP_LRCLK,
            .dout = PIN_I2S_AMP_DOUT,
            .din = PIN_I2S_MIC_DIN,
            .invert_flags = { false, false, false },
        },
    };
    /* Mic often needs its own clocks; stage-1 shares amp clocks — rebind mic GPIOs in full duplex bring-up */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_LOGI(TAG, "I2S TX ready (mic RX secondary channel in voice_solo)");
}

void audio_path_play_beep(uint16_t hz, uint16_t ms)
{
    (void)hz;
    (void)ms;
    /* Tone synthesis placeholder — fills silence frame to keep path warm */
    int16_t silence[256] = { 0 };
    size_t written = 0;
    if (s_tx) {
        i2s_channel_write(s_tx, silence, sizeof(silence), &written, pdMS_TO_TICKS(50));
    }
}

void audio_path_speak_text_stub(const char *text)
{
    ESP_LOGI(TAG, "TTS(stub): %s", text ? text : "");
    audio_path_play_beep(880, 120);
}

int audio_path_read_mic(int16_t *dst, size_t samples)
{
    if (!dst || !s_rx) {
        memset(dst, 0, samples * sizeof(int16_t));
        return 0;
    }
    size_t n = 0;
    i2s_channel_read(s_rx, dst, samples * sizeof(int16_t), &n, pdMS_TO_TICKS(20));
    return (int)(n / sizeof(int16_t));
}

void audio_path_write_pcm(const int16_t *src, size_t samples)
{
    if (!s_tx || !src) {
        return;
    }
    size_t written = 0;
    i2s_channel_write(s_tx, src, samples * sizeof(int16_t), &written, pdMS_TO_TICKS(100));
}
