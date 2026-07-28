#include "net_mqtt.h"
#include "app_config.h"
#include "mode_manager.h"
#include "sensors.h"
#include "power_monitor.h"
#include "behavior_brain.h"
#include "face_ui.h"
#include "led_ring.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "mqtt";
static EventGroupHandle_t s_wifi_eg;
static esp_mqtt_client_handle_t s_client;
#define WIFI_OK BIT0

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_eg, WIFI_OK);
    }
}

void net_wifi_start(void)
{
    s_wifi_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));

    wifi_config_t wcfg = { 0 };
    const app_config_t *ac = app_config_get();
    strncpy((char *)wcfg.sta.ssid, ac->wifi_ssid, sizeof(wcfg.sta.ssid));
    strncpy((char *)wcfg.sta.password, ac->wifi_pass, sizeof(wcfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi connecting to %s", ac->wifi_ssid);
}

static void handle_cmd(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }
    cJSON *goal = cJSON_GetObjectItem(root, "goal");
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(goal)) {
        behavior_brain_on_goal(goal->valuestring);
    } else if (cJSON_IsString(type) && strcmp(type->valuestring, "stop") == 0) {
        behavior_brain_on_goal("stop");
    }
    cJSON_Delete(root);
}

static void handle_emote(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }
    cJSON *mood = cJSON_GetObjectItem(root, "mood");
    if (cJSON_IsString(mood)) {
        if (strcmp(mood->valuestring, "happy") == 0) {
            face_ui_set_mood(FACE_HAPPY);
            led_ring_set_mood(FACE_HAPPY);
        } else if (strcmp(mood->valuestring, "think") == 0) {
            face_ui_set_mood(FACE_THINK);
            led_ring_set_mood(FACE_THINK);
        } else if (strcmp(mood->valuestring, "alert") == 0) {
            face_ui_set_mood(FACE_ALERT);
            led_ring_set_mood(FACE_ALERT);
        }
    }
    cJSON_Delete(root);
}

static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t ev = data;
    const app_config_t *ac = app_config_get();
    char topic[96];
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        snprintf(topic, sizeof(topic), "deskbuddy/%s/cmd", ac->robot_id);
        esp_mqtt_client_subscribe(s_client, topic, 1);
        snprintf(topic, sizeof(topic), "deskbuddy/%s/emote", ac->robot_id);
        esp_mqtt_client_subscribe(s_client, topic, 1);
        snprintf(topic, sizeof(topic), "deskbuddy/%s/heartbeat", ac->robot_id);
        esp_mqtt_client_subscribe(s_client, topic, 0);
        esp_mqtt_client_subscribe(s_client, "deskbuddy/server/ready", 0);
        snprintf(topic, sizeof(topic), "deskbuddy/%s/hello", ac->robot_id);
        esp_mqtt_client_publish(s_client, topic,
                                "{\"fw\":\"robodesk\",\"caps\":[\"drive\",\"face\",\"voice_solo\",\"audio\"]}", 0, 1, 0);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DATA: {
        char t[128] = { 0 };
        char p[512] = { 0 };
        int tl = ev->topic_len < 127 ? ev->topic_len : 127;
        int pl = ev->data_len < 511 ? ev->data_len : 511;
        memcpy(t, ev->topic, tl);
        memcpy(p, ev->data, pl);
        if (strstr(t, "/heartbeat") || strstr(t, "server/ready")) {
            mode_manager_on_pi_heartbeat();
        } else if (strstr(t, "/cmd")) {
            handle_cmd(p);
        } else if (strstr(t, "/emote")) {
            handle_emote(p);
        }
        break;
    }
    default:
        break;
    }
}

void net_mqtt_start(void)
{
    xEventGroupWaitBits(s_wifi_eg, WIFI_OK, false, true, portMAX_DELAY);
    const app_config_t *ac = app_config_get();
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = ac->mqtt_uri,
    };
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    esp_mqtt_client_start(s_client);
}

void net_mqtt_publish_telemetry(void)
{
    if (!s_client) {
        return;
    }
    const app_config_t *ac = app_config_get();
    const sensor_snapshot_t *s = sensors_get();
    const power_status_t *p = power_monitor_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", mode_manager_get() == ROBODESK_MODE_LINKED ? "linked" : "solo");
    cJSON_AddNumberToObject(root, "batt_v", p->pack_v);
    cJSON_AddNumberToObject(root, "batt_pct", p->pct);
    cJSON_AddNumberToObject(root, "temp_c", s->temp_c);
    cJSON_AddNumberToObject(root, "rh", s->rh);
    cJSON_AddNumberToObject(root, "tof_mm", s->tof_mm);
    cJSON_AddBoolToObject(root, "presence", s->presence);
    cJSON_AddBoolToObject(root, "cliff_l", s->cliff_l);
    cJSON_AddBoolToObject(root, "cliff_r", s->cliff_r);
    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        return;
    }
    char topic[96];
    snprintf(topic, sizeof(topic), "deskbuddy/%s/telemetry", ac->robot_id);
    esp_mqtt_client_publish(s_client, topic, printed, 0, 0, 0);
    free(printed);
}
