#pragma once

/* Single source of truth — ESP32-S3 DevKit N16R8 RoboDesk */

#define PIN_I2C_SDA           8
#define PIN_I2C_SCL           9

#define PIN_LCD_SCLK          12
#define PIN_LCD_MOSI          11
#define PIN_LCD_CS            48
#define PIN_LCD_DC            13
#define PIN_LCD_RST           14
#define PIN_LCD_BL            21

#define PIN_I2S_MIC_BCLK      5
#define PIN_I2S_MIC_WS        4
#define PIN_I2S_MIC_DIN       6

#define PIN_I2S_AMP_BCLK      17
#define PIN_I2S_AMP_LRCLK     18
#define PIN_I2S_AMP_DOUT      16

#define PIN_WS2812            38
#define WS2812_COUNT          12

#define PIN_MOTOR_AIN1        1
#define PIN_MOTOR_AIN2        2
#define PIN_MOTOR_BIN1        41
#define PIN_MOTOR_BIN2        42

#define PIN_TCRT_L            15
#define PIN_TCRT_R            7

#define PIN_RD04              40
#define PIN_RD04_TX           39

#define PIN_USER_BTN          47
#define PIN_BAT_ADC           10

#define I2C_ADDR_MPU6050      0x68
#define I2C_ADDR_DHT20        0x38
#define I2C_ADDR_VL53L0X      0x29
#define I2C_ADDR_INA219       0x40

#define BAT_DIV_RTOP_OHM      100000.0f
#define BAT_DIV_RBOT_OHM      33000.0f
