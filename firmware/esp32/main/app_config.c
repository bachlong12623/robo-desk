#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static app_config_t s_cfg;

void app_config_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    strncpy(s_cfg.robot_id, ROBODESK_ID_DEFAULT, sizeof(s_cfg.robot_id) - 1);
    strncpy(s_cfg.wifi_ssid, "YOUR_WIFI", sizeof(s_cfg.wifi_ssid) - 1);
    strncpy(s_cfg.wifi_pass, "YOUR_PASSWORD", sizeof(s_cfg.wifi_pass) - 1);
    strncpy(s_cfg.mqtt_uri, "mqtt://deskbuddy-ai.local", sizeof(s_cfg.mqtt_uri) - 1);
    strncpy(s_cfg.pi_host, ROBODESK_MDNS_PI, sizeof(s_cfg.pi_host) - 1);
    s_cfg.comfort_temp_c = 30.0f;
    s_cfg.comfort_rh_min = 35.0f;
    s_cfg.patrol_enabled = true;
    s_cfg.voice_enabled = true;

    nvs_handle_t h;
    if (nvs_open("robodesk", NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_cfg.wifi_ssid);
        nvs_get_str(h, "ssid", s_cfg.wifi_ssid, &len);
        len = sizeof(s_cfg.wifi_pass);
        nvs_get_str(h, "pass", s_cfg.wifi_pass, &len);
        len = sizeof(s_cfg.robot_id);
        nvs_get_str(h, "id", s_cfg.robot_id, &len);
        nvs_close(h);
    }
}

const app_config_t *app_config_get(void)
{
    return &s_cfg;
}

void app_config_save(const app_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    s_cfg = *cfg;
    nvs_handle_t h;
    if (nvs_open("robodesk", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_str(h, "ssid", s_cfg.wifi_ssid);
    nvs_set_str(h, "pass", s_cfg.wifi_pass);
    nvs_set_str(h, "id", s_cfg.robot_id);
    nvs_commit(h);
    nvs_close(h);
}
