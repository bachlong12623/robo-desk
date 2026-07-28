#pragma once
#include "app_config.h"

typedef enum {
    ROBODESK_LIFE_SOLO = 0,
    ROBODESK_LIFE_LINKED,
    ROBODESK_LIFE_LINKED_STALE,
    ROBODESK_LIFE_LOW_POWER,
    ROBODESK_LIFE_MUTED,
} robodesk_lifecycle_t;

void mode_manager_start(void);
robodesk_mode_t mode_manager_get(void);
void mode_manager_on_pi_heartbeat(void);
void mode_manager_force_solo(void);
void mode_manager_set_low_power(bool on);
void mode_manager_set_muted(bool on);
robodesk_lifecycle_t mode_manager_lifecycle_get(void);
