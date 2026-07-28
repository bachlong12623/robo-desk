#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float temp_c;
    float rh;
    bool climate_valid;
    bool climate_stale;
    uint16_t tof_mm;
    bool tof_valid;
    bool tof_stale;
    bool presence;
    bool cliff_l;
    bool cliff_r;
    float ax, ay, az;
    float gx, gy, gz;
    bool imu_valid;
    bool imu_stale;
    bool shock;
    bool tipped;
    bool btn;
    uint32_t sample_ms;
} sensor_snapshot_t;

void sensors_init(void);
void sensors_poll(void);
const sensor_snapshot_t *sensors_get(void);
