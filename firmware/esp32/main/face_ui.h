#pragma once

typedef enum {
    FACE_IDLE = 0,
    FACE_HAPPY,
    FACE_SLEEPY,
    FACE_LISTEN,
    FACE_SPEAK,
    FACE_THINK,
    FACE_ALERT,
    FACE_PANIC,
    FACE_SAD,
    FACE_ANGRY,
    FACE_LOVE,
    FACE_EXCITED,
    FACE_DIZZY,
    FACE_PLEAD,
    FACE_MOOD_COUNT,
} face_mood_t;

#include "screen_info.h"
#include <stdint.h>

typedef enum {
    FACE_UI_PRIO_IDLE = 0,
    FACE_UI_PRIO_REMOTE = 1,
    FACE_UI_PRIO_LOCAL = 2,
    FACE_UI_PRIO_SAFETY = 3,
} face_ui_priority_t;

void face_ui_init(void);
void face_ui_set_mood(face_mood_t mood);
void face_ui_set_mood_prio(face_mood_t mood, face_ui_priority_t prio, uint32_t hold_s);
face_mood_t face_ui_get_mood(void);
void face_ui_task_start(void);

/* Show an info screen (climate/battery/level/radar/clock/status).
 * hold_s: seconds before auto-returning to the face; 0 = stay until
 * face_ui_show_face() is called. */
void face_ui_show_screen(info_screen_t screen, uint32_t hold_s);
void face_ui_show_screen_prio(info_screen_t screen, uint32_t hold_s, face_ui_priority_t prio);
void face_ui_show_face(void);
