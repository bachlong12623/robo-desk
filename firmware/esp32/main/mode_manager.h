#pragma once
#include "app_config.h"

void mode_manager_start(void);
robodesk_mode_t mode_manager_get(void);
void mode_manager_on_pi_heartbeat(void);
void mode_manager_force_solo(void);
