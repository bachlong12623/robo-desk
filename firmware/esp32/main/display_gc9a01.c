#include "display_gc9a01.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "lcd";
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_fb;

bool display_gc9a01_init(void)
{
    gpio_reset_pin(PIN_LCD_BL);
    gpio_set_direction(PIN_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_BL, 1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_W * DISPLAY_H * sizeof(uint16_t),
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 4,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = { .dc_low_on_data = 0, .octal_mode = 0, .lsb_first = 0 },
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel io: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_gc9a01(io, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel gc9a01: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_mirror(s_panel, false, false);
    esp_lcd_panel_disp_on_off(s_panel, true);

    s_fb = heap_caps_malloc(DISPLAY_W * DISPLAY_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_fb) {
        s_fb = heap_caps_malloc(DISPLAY_W * DISPLAY_H * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!s_fb) {
        ESP_LOGE(TAG, "framebuffer alloc failed");
        return false;
    }
    memset(s_fb, 0, DISPLAY_W * DISPLAY_H * sizeof(uint16_t));
    ESP_LOGI(TAG, "GC9A01 ready %dx%d", DISPLAY_W, DISPLAY_H);
    return true;
}

uint16_t *display_gc9a01_framebuffer(void)
{
    return s_fb;
}

void display_gc9a01_blit(const uint16_t *rgb565)
{
    if (!s_panel || !rgb565) {
        return;
    }
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, DISPLAY_W, DISPLAY_H, rgb565);
}
