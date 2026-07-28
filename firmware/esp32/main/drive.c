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

typedef struct {
    int pin;
    ledc_channel_t chan;
} motor_pin_t;

static const motor_pin_t s_motor_pins[] = {
    { PIN_MOTOR_AIN1, LEDC_CHANNEL_0 },
    { PIN_MOTOR_AIN2, LEDC_CHANNEL_1 },
    { PIN_MOTOR_BIN1, LEDC_CHANNEL_2 },
    { PIN_MOTOR_BIN2, LEDC_CHANNEL_3 },
};

static void set_pwm(const motor_pin_t *p, int duty)
{
    if (!p) {
        return;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, p->chan, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, p->chan);
}

static void motor_pins_init(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);

    for (size_t i = 0; i < sizeof(s_motor_pins) / sizeof(s_motor_pins[0]); ++i) {
        gpio_reset_pin(s_motor_pins[i].pin);
        ledc_channel_config_t ccfg = {
            .gpio_num = s_motor_pins[i].pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = s_motor_pins[i].chan,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ledc_channel_config(&ccfg);
    }
}

static void set_pair(const motor_pin_t *a, const motor_pin_t *b, int forward, int duty)
{
    if (duty <= 0) {
        set_pwm(a, 0);
        set_pwm(b, 0);
        return;
    }
    if (forward) {
        set_pwm(a, duty);
        set_pwm(b, 0);
    } else {
        set_pwm(a, 0);
        set_pwm(b, duty);
    }
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
    set_pair(&s_motor_pins[0], &s_motor_pins[1], 1, 0);
    set_pair(&s_motor_pins[2], &s_motor_pins[3], 1, 0);
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
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 1, 0);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 1, 0);
        return;
    }
    int d = (s_speed * 1023) / 100;
    switch (s_cmd) {
    case DRIVE_FORWARD:
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 1, d);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 1, d);
        break;
    case DRIVE_BACKWARD:
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 0, d);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 0, d);
        break;
    case DRIVE_LEFT:
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 0, d);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 1, d);
        break;
    case DRIVE_RIGHT:
    case DRIVE_SPIN:
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 1, d);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 0, d);
        break;
    default:
        set_pair(&s_motor_pins[0], &s_motor_pins[1], 1, 0);
        set_pair(&s_motor_pins[2], &s_motor_pins[3], 1, 0);
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
