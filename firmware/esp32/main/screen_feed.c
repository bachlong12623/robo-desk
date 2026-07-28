#include "screen_feed.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <string.h>

static screen_info_data_t s_remote;
static screen_info_data_t s_last_good;
static int64_t s_last_update_us;
static int s_version;

#define SCREEN_FEED_STALE_US (15LL * 60LL * 1000000LL)

static void copy_json_string(cJSON *root, const char *key, char *dst, size_t dst_len)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dst_len - 1);
        dst[dst_len - 1] = '\0';
    }
}

void screen_feed_reset(void)
{
    memset(&s_remote, 0, sizeof(s_remote));
    memset(&s_last_good, 0, sizeof(s_last_good));
    s_last_update_us = 0;
    s_version = 0;
}

void screen_feed_update_json(const char *json)
{
    screen_info_data_t next = s_last_good;
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    cJSON *n = NULL;
    cJSON *ver = cJSON_GetObjectItem(root, "version");
    if (cJSON_IsNumber(ver) && (int)ver->valuedouble < s_version) {
        cJSON_Delete(root);
        return;
    }
    if ((n = cJSON_GetObjectItem(root, "forecast_temp_c")) && cJSON_IsNumber(n)) next.forecast_temp_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_min_c")) && cJSON_IsNumber(n)) next.forecast_min_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_max_c")) && cJSON_IsNumber(n)) next.forecast_max_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_rain_pct")) && cJSON_IsNumber(n)) next.forecast_rain_pct = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_weather_code")) && cJSON_IsNumber(n)) next.forecast_weather_code = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "aqi_us")) && cJSON_IsNumber(n)) next.aqi_us = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "aqi_pm25")) && cJSON_IsNumber(n)) next.aqi_pm25 = (float)n->valuedouble;

    if ((n = cJSON_GetObjectItem(root, "desk_today_min")) && cJSON_IsNumber(n)) next.desk_today_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "desk_session_min")) && cJSON_IsNumber(n)) next.desk_session_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "desk_break_due")) && cJSON_IsBool(n)) next.desk_break_due = cJSON_IsTrue(n);

    if ((n = cJSON_GetObjectItem(root, "calendar_hh")) && cJSON_IsNumber(n)) next.calendar_hh = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_mm")) && cJSON_IsNumber(n)) next.calendar_mm = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_in_min")) && cJSON_IsNumber(n)) next.calendar_in_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_has")) && cJSON_IsBool(n)) next.calendar_has = cJSON_IsTrue(n);

    if ((n = cJSON_GetObjectItem(root, "moon_phase_pct")) && cJSON_IsNumber(n)) next.moon_phase_pct = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "moon_illum_pct")) && cJSON_IsNumber(n)) next.moon_illum_pct = (int)n->valuedouble;

    if ((n = cJSON_GetObjectItem(root, "morning_pending")) && cJSON_IsBool(n)) next.morning_pending = cJSON_IsTrue(n);

    copy_json_string(root, "forecast_label", next.forecast_label, sizeof(next.forecast_label));
    copy_json_string(root, "aqi_label", next.aqi_label, sizeof(next.aqi_label));
    copy_json_string(root, "news_title", next.news_title, sizeof(next.news_title));
    copy_json_string(root, "calendar_title", next.calendar_title, sizeof(next.calendar_title));
    copy_json_string(root, "morning_line1", next.morning_line1, sizeof(next.morning_line1));
    copy_json_string(root, "morning_line2", next.morning_line2, sizeof(next.morning_line2));
    copy_json_string(root, "moon_label", next.moon_label, sizeof(next.moon_label));

    s_remote = next;
    s_last_good = next;
    s_last_update_us = esp_timer_get_time();
    s_version = cJSON_IsNumber(ver) ? (int)ver->valuedouble : (s_version + 1);

    cJSON_Delete(root);
}

void screen_feed_merge(screen_info_data_t *d)
{
    if (!d) {
        return;
    }
    const screen_info_data_t *src = s_last_good.news_title[0] || s_last_update_us ? &s_last_good : &s_remote;
    d->forecast_temp_c = src->forecast_temp_c;
    d->forecast_min_c = src->forecast_min_c;
    d->forecast_max_c = src->forecast_max_c;
    d->forecast_rain_pct = src->forecast_rain_pct;
    d->forecast_weather_code = src->forecast_weather_code;
    strncpy(d->forecast_label, src->forecast_label, sizeof(d->forecast_label) - 1);
    d->forecast_label[sizeof(d->forecast_label) - 1] = '\0';
    strncpy(d->aqi_label, src->aqi_label, sizeof(d->aqi_label) - 1);
    d->aqi_label[sizeof(d->aqi_label) - 1] = '\0';
    d->aqi_us = src->aqi_us;
    d->aqi_pm25 = src->aqi_pm25;
    strncpy(d->news_title, src->news_title, sizeof(d->news_title) - 1);
    d->news_title[sizeof(d->news_title) - 1] = '\0';
    d->calendar_has = src->calendar_has;
    d->calendar_hh = src->calendar_hh;
    d->calendar_mm = src->calendar_mm;
    d->calendar_in_min = src->calendar_in_min;
    strncpy(d->calendar_title, src->calendar_title, sizeof(d->calendar_title) - 1);
    d->calendar_title[sizeof(d->calendar_title) - 1] = '\0';
    d->desk_today_min = src->desk_today_min;
    d->desk_session_min = src->desk_session_min;
    d->desk_break_due = src->desk_break_due;
    d->morning_pending = src->morning_pending;
    strncpy(d->morning_line1, src->morning_line1, sizeof(d->morning_line1) - 1);
    d->morning_line1[sizeof(d->morning_line1) - 1] = '\0';
    strncpy(d->morning_line2, src->morning_line2, sizeof(d->morning_line2) - 1);
    d->morning_line2[sizeof(d->morning_line2) - 1] = '\0';
    d->moon_phase_pct = src->moon_phase_pct;
    d->moon_illum_pct = src->moon_illum_pct;
    strncpy(d->moon_label, src->moon_label, sizeof(d->moon_label) - 1);
    d->moon_label[sizeof(d->moon_label) - 1] = '\0';
    d->feed_fresh = screen_feed_is_fresh();
    d->feed_age_s = s_last_update_us ? (uint32_t)((esp_timer_get_time() - s_last_update_us) / 1000000LL) : 0;
}

bool screen_feed_is_fresh(void)
{
    return s_last_update_us > 0 && (esp_timer_get_time() - s_last_update_us) < SCREEN_FEED_STALE_US;
}
