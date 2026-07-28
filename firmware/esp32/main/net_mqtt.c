#include "net_mqtt.h"
#include "app_config.h"
#include "mode_manager.h"
#include "sensors.h"
#include "power_monitor.h"
#include "behavior_brain.h"
#include "face_ui.h"
#include "screen_feed.h"
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
#define WIFI_FAIL BIT1

static int s_wifi_retry;

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_eg, WIFI_OK);
        xEventGroupSetBits(s_wifi_eg, WIFI_FAIL);
        s_wifi_retry++;
        ESP_LOGW(TAG, "WiFi disconnected, retry=%d", s_wifi_retry);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry = 0;
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
    cJSON *ttl = cJSON_GetObjectItem(root, "ttl_ms");
    if (cJSON_IsString(goal)) {
        uint32_t ttl_ms = cJSON_IsNumber(ttl) ? (uint32_t)ttl->valuedouble : 30000;
        behavior_brain_on_goal_ttl(goal->valuestring, ttl_ms);
    } else if (cJSON_IsString(type) && strcmp(type->valuestring, "stop") == 0) {
        behavior_brain_on_goal_ttl("stop", 0);
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
        static const struct { const char *name; face_mood_t m; } MOODS[] = {
            { "idle", FACE_IDLE },       { "happy", FACE_HAPPY },
            { "sleepy", FACE_SLEEPY },   { "listen", FACE_LISTEN },
            { "speak", FACE_SPEAK },     { "think", FACE_THINK },
            { "alert", FACE_ALERT },     { "panic", FACE_PANIC },
            { "sad", FACE_SAD },         { "angry", FACE_ANGRY },
            { "love", FACE_LOVE },       { "excited", FACE_EXCITED },
            { "dizzy", FACE_DIZZY },     { "plead", FACE_PLEAD },
        };
        for (unsigned i = 0; i < sizeof(MOODS) / sizeof(MOODS[0]); ++i) {
            if (strcmp(mood->valuestring, MOODS[i].name) == 0) {
                face_ui_show_face();
                face_ui_set_mood_prio(MOODS[i].m, FACE_UI_PRIO_REMOTE, 10);
                led_ring_set_mood(MOODS[i].m);
                break;
            }
        }
    }
    /* {"screen":"climate","hold_s":10} — show an info screen */
    cJSON *screen = cJSON_GetObjectItem(root, "screen");
    if (cJSON_IsString(screen)) {
        if (strcmp(screen->valuestring, "face") == 0) {
            face_ui_show_face();
        } else {
            info_screen_t s = screen_info_by_name(screen->valuestring);
            if (s < SCREEN_COUNT) {
                cJSON *hold = cJSON_GetObjectItem(root, "hold_s");
                uint32_t hold_s = cJSON_IsNumber(hold) ? (uint32_t)hold->valuedouble : 15;
                face_ui_show_screen_prio(s, hold_s, FACE_UI_PRIO_REMOTE);
            }
        }
    }
    cJSON_Delete(root);
}

static void handle_screen_data(const char *json)
{
    screen_feed_update_json(json);
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
        snprintf(topic, sizeof(topic), "deskbuddy/%s/screen_data", ac->robot_id);
        esp_mqtt_client_subscribe(s_client, topic, 0);
        esp_mqtt_client_subscribe(s_client, "deskbuddy/server/ready", 0);
        snprintf(topic, sizeof(topic), "deskbuddy/%s/hello", ac->robot_id);
        esp_mqtt_client_publish(s_client, topic,
                                "{\"fw\":\"robodesk\",\"caps\":[\"drive\",\"face\",\"voice_solo\",\"audio\"]}", 0, 1, 0);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        int tl = ev->topic_len < 255 ? ev->topic_len : 255;
        int pl = ev->data_len < 2047 ? ev->data_len : 2047;
        char *t = calloc((size_t)tl + 1, 1);
        char *p = calloc((size_t)pl + 1, 1);
        if (!t || !p) {
            free(t);
            free(p);
            break;
        }
        memcpy(t, ev->topic, (size_t)tl);
        memcpy(p, ev->data, (size_t)pl);
        if (strstr(t, "/heartbeat") || strstr(t, "server/ready")) {
            mode_manager_on_pi_heartbeat();
        } else if (strstr(t, "/cmd")) {
            handle_cmd(p);
        } else if (strstr(t, "/emote")) {
            handle_emote(p);
        } else if (strstr(t, "/screen_data")) {
            handle_screen_data(p);
        }
        free(t);
        free(p);
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
        .network.reconnect_timeout_ms = 5000,
        .buffer.size = 4096,
        .buffer.out_size = 2048,
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
    cJSON_AddBoolToObject(root, "climate_valid", s->climate_valid && !s->climate_stale);
    cJSON_AddBoolToObject(root, "tof_valid", s->tof_valid && !s->tof_stale);
    cJSON_AddBoolToObject(root, "imu_valid", s->imu_valid && !s->imu_stale);
    cJSON_AddBoolToObject(root, "power_valid", p->valid);
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
