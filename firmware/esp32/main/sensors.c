#include "sensors.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "sensors";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev_dht20;
static i2c_master_dev_handle_t s_dev_tof;
static i2c_master_dev_handle_t s_dev_mpu;
static sensor_snapshot_t s_snap;
static float s_last_az = 1.f;
static bool s_dht_ok;
static bool s_tof_ok;
static bool s_mpu_ok;
static int64_t s_last_dht_us;
static int64_t s_last_tof_us;
static int64_t s_last_mpu_us;

#define STALE_US 5000000LL

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

static i2c_master_dev_handle_t dev_for_addr(uint8_t addr)
{
    switch (addr) {
    case I2C_ADDR_DHT20:
        return s_dev_dht20;
    case I2C_ADDR_MPU6050:
        return s_dev_mpu;
    case I2C_ADDR_VL53L0X:
        return s_dev_tof;
    default:
        return NULL;
    }
}

static esp_err_t i2c_write_reg8(uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_master_dev_handle_t dev = dev_for_addr(addr);
    uint8_t buf[2] = { reg, val };
    return dev ? i2c_master_transmit(dev, buf, sizeof(buf), 50) : ESP_ERR_INVALID_ARG;
}

static esp_err_t i2c_write_bytes(uint8_t addr, const uint8_t *buf, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for_addr(addr);
    return dev ? i2c_master_transmit(dev, buf, len, 50) : ESP_ERR_INVALID_ARG;
}

static esp_err_t i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for_addr(addr);
    return dev ? i2c_master_transmit_receive(dev, &reg, 1, buf, len, 50) : ESP_ERR_INVALID_ARG;
}

static void add_device(uint8_t addr, i2c_master_dev_handle_t *dev)
{
    if (!s_bus || !dev) {
        return;
    }
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(s_bus, &cfg, dev) != ESP_OK) {
        *dev = NULL;
    }
}

static bool dht20_init(void)
{
    uint8_t st = 0;
    if (i2c_read_reg(I2C_ADDR_DHT20, 0x71, &st, 1) != ESP_OK) {
        return false;
    }
    if ((st & 0x18) != 0x18) {
        uint8_t init_cmd[] = { 0xBE, 0x08, 0x00 };
        if (i2c_write_bytes(I2C_ADDR_DHT20, init_cmd, sizeof(init_cmd)) != ESP_OK) {
            return false;
        }
    }
    return true;
}

static bool dht20_read(float *temp_c, float *rh)
{
    uint8_t trig[] = { 0xAC, 0x33, 0x00 };
    uint8_t buf[7] = { 0 };
    if (i2c_write_bytes(I2C_ADDR_DHT20, trig, sizeof(trig)) != ESP_OK) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(90));
    if (i2c_read_reg(I2C_ADDR_DHT20, 0x00, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    if (buf[0] & 0x80) {
        return false;
    }
    uint32_t raw_rh = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    uint32_t raw_t = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    *rh = (raw_rh * 100.0f) / 1048576.0f;
    *temp_c = (raw_t * 200.0f) / 1048576.0f - 50.0f;
    return true;
}

static bool mpu6050_init(void)
{
    if (i2c_write_reg8(I2C_ADDR_MPU6050, 0x6B, 0x00) != ESP_OK) { /* wake */
        return false;
    }
    i2c_write_reg8(I2C_ADDR_MPU6050, 0x1B, 0x00); /* gyro +-250 */
    i2c_write_reg8(I2C_ADDR_MPU6050, 0x1C, 0x00); /* accel +-2g */
    i2c_write_reg8(I2C_ADDR_MPU6050, 0x1A, 0x03); /* DLPF */
    return true;
}

static bool mpu6050_read(sensor_snapshot_t *s)
{
    uint8_t buf[14];
    if (i2c_read_reg(I2C_ADDR_MPU6050, 0x3B, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t gx = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz = (int16_t)((buf[12] << 8) | buf[13]);
    s->ax = ax / 16384.0f;
    s->ay = ay / 16384.0f;
    s->az = az / 16384.0f;
    s->gx = gx / 131.0f;
    s->gy = gy / 131.0f;
    s->gz = gz / 131.0f;
    return true;
}

static bool vl53l0x_init(void)
{
    /* Minimal identity probe + start continuous ranging.
     * Not a full ST reference init, but enough for bring-up on common breakout configs.
     */
    uint8_t model = 0;
    if (i2c_read_reg(I2C_ADDR_VL53L0X, 0xC0, &model, 1) != ESP_OK) {
        return false;
    }
    i2c_write_reg8(I2C_ADDR_VL53L0X, 0x00, 0x01); /* SYSRANGE_START single-shot */
    return model != 0;
}

static bool vl53l0x_read_mm(uint16_t *mm)
{
    uint8_t st = 0;
    uint8_t range[2] = { 0 };
    /* RESULT_INTERRUPT_STATUS / RESULT_RANGE */
    if (i2c_read_reg(I2C_ADDR_VL53L0X, 0x13, &st, 1) != ESP_OK) {
        return false;
    }
    if ((st & 0x07) == 0) {
        return false;
    }
    if (i2c_read_reg(I2C_ADDR_VL53L0X, 0x1E, range, 2) != ESP_OK) {
        return false;
    }
    *mm = ((uint16_t)range[0] << 8) | range[1];
    i2c_write_reg8(I2C_ADDR_VL53L0X, 0x0B, 0x01); /* clear interrupt */
    i2c_write_reg8(I2C_ADDR_VL53L0X, 0x00, 0x01); /* next single-shot */
    return *mm > 0 && *mm < 8190;
}

void sensors_init(void)
{
    memset(&s_snap, 0, sizeof(s_snap));
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
        add_device(I2C_ADDR_DHT20, &s_dev_dht20);
        add_device(I2C_ADDR_MPU6050, &s_dev_mpu);
        add_device(I2C_ADDR_VL53L0X, &s_dev_tof);
    }

    s_snap.tof_mm = 1000;
    s_snap.temp_c = 25.f;
    s_snap.rh = 50.f;
    s_snap.az = 1.f;
    s_snap.sample_ms = (uint32_t)(esp_timer_get_time() / 1000);

    if (s_bus) {
        s_dht_ok = dht20_init();
        s_mpu_ok = mpu6050_init();
        s_tof_ok = vl53l0x_init();
    }
    ESP_LOGI(TAG, "devices dht20=%d mpu6050=%d vl53l0x=%d", s_dht_ok, s_mpu_ok, s_tof_ok);
}

void sensors_poll(void)
{
    s_snap.cliff_l = gpio_get_level(PIN_TCRT_L) == 0;
    s_snap.cliff_r = gpio_get_level(PIN_TCRT_R) == 0;
    s_snap.presence = gpio_get_level(PIN_RD04) == 1;
    s_snap.btn = gpio_get_level(PIN_USER_BTN) == 0;
    int64_t now = esp_timer_get_time();

    if (s_dht_ok) {
        float t = 0.f, rh = 0.f;
        if (dht20_read(&t, &rh)) {
            s_snap.temp_c = t;
            s_snap.rh = rh;
            s_snap.climate_valid = true;
            s_last_dht_us = now;
        } else {
            s_snap.climate_valid = false;
        }
    }
    s_snap.climate_stale = (now - s_last_dht_us) > STALE_US;

    if (s_mpu_ok) {
        if (mpu6050_read(&s_snap)) {
            s_snap.imu_valid = true;
            s_last_mpu_us = now;
        } else {
            s_snap.imu_valid = false;
        }
    }
    s_snap.imu_stale = (now - s_last_mpu_us) > STALE_US;

    if (s_tof_ok) {
        uint16_t mm = 0;
        if (vl53l0x_read_mm(&mm)) {
            s_snap.tof_mm = mm;
            s_snap.tof_valid = true;
            s_last_tof_us = now;
        } else {
            s_snap.tof_valid = false;
        }
    }
    s_snap.tof_stale = (now - s_last_tof_us) > STALE_US;

    float daz = fabsf(s_snap.az - s_last_az);
    s_snap.shock = s_snap.imu_valid && daz > 0.45f;
    s_snap.tipped = s_snap.imu_valid && fabsf(s_snap.az) < 0.35f;
    s_last_az = s_snap.az;
    s_snap.sample_ms = (uint32_t)(now / 1000);
    ESP_LOGD(TAG, "poll cliff=%d/%d presence=%d temp=%.1f tof=%u imu=%d",
             s_snap.cliff_l, s_snap.cliff_r, s_snap.presence, s_snap.temp_c, s_snap.tof_mm, s_snap.imu_valid);
}

const sensor_snapshot_t *sensors_get(void)
{
    return &s_snap;
}

i2c_master_bus_handle_t sensors_i2c_bus(void)
{
    return s_bus;
}
