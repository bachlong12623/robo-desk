#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float temp_c;
    float rh;
    uint16_t tof_mm;
    bool presence;
    bool cliff_l;
    bool cliff_r;
    float ax, ay, az;
    float gx, gy, gz;
    bool shock;
    bool tipped;
    bool btn;
} sensor_snapshot_t;

void sensors_init(void);
void sensors_poll(void);
const sensor_snapshot_t *sensors_get(void);
