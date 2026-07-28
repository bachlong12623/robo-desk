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
} face_mood_t;

void face_ui_init(void);
void face_ui_set_mood(face_mood_t mood);
void face_ui_task_start(void);
