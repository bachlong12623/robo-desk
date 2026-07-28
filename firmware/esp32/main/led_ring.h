#pragma once
#include "face_ui.h"
#include <stdint.h>

void led_ring_init(void);
void led_ring_set_mood(face_mood_t mood);
void led_ring_set_amplitude(uint8_t amp);
void led_ring_task_start(void);
