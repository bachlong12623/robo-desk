#include "sensors.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "sensors";
static i2c_master_bus_handle_t s_bus;
static sensor_snapshot_t s_snap;
static float s_last_az = 1.f;

static void gpio_inputs_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_TCRT_L) | (1ULL << PIN_TCRT_R) | (1ULL << PIN_RD04) | (1ULL << PIN_USER_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

void sensors_init(void)
{
    gpio_inputs_init();
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_master_new_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "I2C ready SDA=%d SCL=%d", PIN_I2C_SDA, PIN_I2C_SCL);
    }
    s_snap.tof_mm = 1000;
    s_snap.temp_c = 25.f;
    s_snap.rh = 50.f;
    s_snap.az = 1.f;
}

/* Lightweight stubs until full driver bring-up; GPIO paths are live. */
void sensors_poll(void)
{
    s_snap.cliff_l = gpio_get_level(PIN_TCRT_L) == 0;
    s_snap.cliff_r = gpio_get_level(PIN_TCRT_R) == 0;
    s_snap.presence = gpio_get_level(PIN_RD04) == 1;
    s_snap.btn = gpio_get_level(PIN_USER_BTN) == 0;

    /* Placeholder IMU fusion until MPU6050 register driver is filled */
    float daz = fabsf(s_snap.az - s_last_az);
    s_snap.shock = daz > 0.45f;
    s_snap.tipped = fabsf(s_snap.az) < 0.35f;
    s_last_az = s_snap.az;

    /* Soft demo drift for temp when no ACK yet — overwritten by real DHT read path */
    static int64_t last_demo;
    int64_t now = esp_timer_get_time();
    if (now - last_demo > 5000000) {
        last_demo = now;
        ESP_LOGD(TAG, "poll cliff=%d/%d presence=%d", s_snap.cliff_l, s_snap.cliff_r, s_snap.presence);
    }
}

const sensor_snapshot_t *sensors_get(void)
{
    return &s_snap;
}

/* Expose bus for future device drivers */
i2c_master_bus_handle_t sensors_i2c_bus(void)
{
    return s_bus;
}
