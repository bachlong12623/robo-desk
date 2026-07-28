#include "drive.h"
#include "board_pins.h"
#include "safety.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "drive";
static drive_cmd_t s_cmd = DRIVE_STOP;
static int s_speed = 0;

static void motor_pins_init(void)
{
    int pins[] = { PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, PIN_MOTOR_BIN1, PIN_MOTOR_BIN2 };
    for (int i = 0; i < 4; ++i) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }
    /* Soft-PWM via LEDC for speed */
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);
}

static void set_pair(int a, int b, int forward, int duty)
{
    if (duty <= 0) {
        gpio_set_level(a, 0);
        gpio_set_level(b, 0);
        return;
    }
    if (forward) {
        gpio_set_level(a, 1);
        gpio_set_level(b, 0);
    } else {
        gpio_set_level(a, 0);
        gpio_set_level(b, 1);
    }
    (void)duty; /* GPIO on/off stage-1; LEDC channel map in bring-up */
}

void drive_init(void)
{
    motor_pins_init();
    drive_stop();
}

void drive_stop(void)
{
    s_cmd = DRIVE_STOP;
    s_speed = 0;
    set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 1, 0);
    set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 1, 0);
}

void drive_set(drive_cmd_t cmd, int speed_pct)
{
    if (speed_pct < 0) {
        speed_pct = 0;
    }
    if (speed_pct > 100) {
        speed_pct = 100;
    }
    s_cmd = cmd;
    s_speed = speed_pct;
}

static void apply(void)
{
    if (!safety_allows_motion()) {
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 1, 0);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 1, 0);
        return;
    }
    int d = s_speed;
    switch (s_cmd) {
    case DRIVE_FORWARD:
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 1, d);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 1, d);
        break;
    case DRIVE_BACKWARD:
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 0, d);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 0, d);
        break;
    case DRIVE_LEFT:
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 0, d);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 1, d);
        break;
    case DRIVE_RIGHT:
    case DRIVE_SPIN:
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 1, d);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 0, d);
        break;
    default:
        set_pair(PIN_MOTOR_AIN1, PIN_MOTOR_AIN2, 1, 0);
        set_pair(PIN_MOTOR_BIN1, PIN_MOTOR_BIN2, 1, 0);
        break;
    }
}

static void drive_task(void *arg)
{
    (void)arg;
    for (;;) {
        apply();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void drive_task_start(void)
{
    xTaskCreatePinnedToCore(drive_task, "drive", 3072, NULL, 8, NULL, 0);
    ESP_LOGI(TAG, "drive on core0");
}
