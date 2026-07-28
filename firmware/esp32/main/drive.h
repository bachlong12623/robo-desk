#pragma once
#include <stdint.h>

typedef enum {
    DRIVE_STOP = 0,
    DRIVE_FORWARD,
    DRIVE_BACKWARD,
    DRIVE_LEFT,
    DRIVE_RIGHT,
    DRIVE_SPIN,
} drive_cmd_t;

void drive_init(void);
void drive_set(drive_cmd_t cmd, int speed_pct);
void drive_stop(void);
void drive_task_start(void);
