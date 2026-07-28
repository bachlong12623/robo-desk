#include "power_monitor.h"
#include "board_pins.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "power";
static adc_oneshot_unit_handle_t s_adc;
static power_status_t s_st;
static float s_filt_v;

void power_monitor_init(void)
{
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &s_adc));
    adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_9, &ccfg)); /* GPIO10 */
    s_st.pack_v = 7.4f;
    s_st.pct = 50.f;
    s_filt_v = s_st.pack_v;
}

void power_monitor_update(void)
{
    int raw = 0;
    if (adc_oneshot_read(s_adc, ADC_CHANNEL_9, &raw) != ESP_OK) {
        s_st.valid = false;
        return;
    }
    /* Basic filtered estimate; calibrate with NVS later. */
    float vadc = (raw / 4095.0f) * 3.3f;
    float ratio = BAT_DIV_RBOT_OHM / (BAT_DIV_RTOP_OHM + BAT_DIV_RBOT_OHM);
    float pack_v = vadc / ratio;
    s_filt_v = (s_filt_v * 0.85f) + (pack_v * 0.15f);
    s_st.pack_v = s_filt_v;
    float pct = (s_st.pack_v - 6.0f) / (8.4f - 6.0f) * 100.f;
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }
    s_st.pct = pct;
    s_st.low = pct < 25.f;
    s_st.critical = pct < 10.f;
    s_st.valid = true;
    ESP_LOGD(TAG, "pack=%.2fV pct=%.0f", s_st.pack_v, s_st.pct);
}

const power_status_t *power_monitor_get(void)
{
    return &s_st;
}
