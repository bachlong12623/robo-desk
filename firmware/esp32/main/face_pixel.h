#pragma once
#include "face_ui.h"
#include <stdint.h>

/* Render chunky pixel-art face into RGB565 buffer (240x240). */
void face_pixel_render(face_mood_t mood, uint32_t anim_frame, uint16_t *fb, int w, int h);
