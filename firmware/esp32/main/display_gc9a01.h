#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_W 240
#define DISPLAY_H 240

bool display_gc9a01_init(void);
void display_gc9a01_blit(const uint16_t *rgb565);
uint16_t *display_gc9a01_framebuffer(void);
