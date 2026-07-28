#pragma once
#include <stdbool.h>

typedef struct {
    bool halt_motors;
    bool cliff;
    bool tip;
    bool batt_critical;
    bool tof_emergency;
} safety_state_t;

void safety_init(void);
void safety_update(void);
const safety_state_t *safety_get(void);
bool safety_allows_motion(void);
