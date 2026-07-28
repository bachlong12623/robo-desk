#pragma once
#include <stdbool.h>

typedef struct {
    float pack_v;
    float pct;
    bool low;
    bool critical;
    bool valid;
} power_status_t;

void power_monitor_init(void);
void power_monitor_update(void);
const power_status_t *power_monitor_get(void);
