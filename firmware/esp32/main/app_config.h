#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ROBODESK_ID_DEFAULT "deskbuddy-01"
#define ROBODESK_MDNS_PI    "deskbuddy-ai.local"

typedef enum {
    ROBODESK_MODE_SOLO = 0,
    ROBODESK_MODE_LINKED = 1,
} robodesk_mode_t;

typedef struct {
    char robot_id[32];
    char wifi_ssid[32];
    char wifi_pass[64];
    char mqtt_uri[96];
    char pi_host[64];
    float comfort_temp_c;
    float comfort_rh_min;
    bool patrol_enabled;
    bool voice_enabled;
} app_config_t;

void app_config_init(void);
const app_config_t *app_config_get(void);
void app_config_save(const app_config_t *cfg);
