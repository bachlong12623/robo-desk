#include "screen_feed.h"
#include "cJSON.h"
#include <string.h>

static screen_info_data_t s_remote;

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
}

void screen_feed_update_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    cJSON *n = NULL;
    if ((n = cJSON_GetObjectItem(root, "forecast_temp_c")) && cJSON_IsNumber(n)) s_remote.forecast_temp_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_min_c")) && cJSON_IsNumber(n)) s_remote.forecast_min_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_max_c")) && cJSON_IsNumber(n)) s_remote.forecast_max_c = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_rain_pct")) && cJSON_IsNumber(n)) s_remote.forecast_rain_pct = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "forecast_weather_code")) && cJSON_IsNumber(n)) s_remote.forecast_weather_code = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "aqi_us")) && cJSON_IsNumber(n)) s_remote.aqi_us = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "aqi_pm25")) && cJSON_IsNumber(n)) s_remote.aqi_pm25 = (float)n->valuedouble;

    if ((n = cJSON_GetObjectItem(root, "desk_today_min")) && cJSON_IsNumber(n)) s_remote.desk_today_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "desk_session_min")) && cJSON_IsNumber(n)) s_remote.desk_session_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "desk_break_due")) && cJSON_IsBool(n)) s_remote.desk_break_due = cJSON_IsTrue(n);

    if ((n = cJSON_GetObjectItem(root, "calendar_hh")) && cJSON_IsNumber(n)) s_remote.calendar_hh = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_mm")) && cJSON_IsNumber(n)) s_remote.calendar_mm = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_in_min")) && cJSON_IsNumber(n)) s_remote.calendar_in_min = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "calendar_has")) && cJSON_IsBool(n)) s_remote.calendar_has = cJSON_IsTrue(n);

    if ((n = cJSON_GetObjectItem(root, "moon_phase_pct")) && cJSON_IsNumber(n)) s_remote.moon_phase_pct = (int)n->valuedouble;
    if ((n = cJSON_GetObjectItem(root, "moon_illum_pct")) && cJSON_IsNumber(n)) s_remote.moon_illum_pct = (int)n->valuedouble;

    if ((n = cJSON_GetObjectItem(root, "morning_pending")) && cJSON_IsBool(n)) s_remote.morning_pending = cJSON_IsTrue(n);

    copy_json_string(root, "forecast_label", s_remote.forecast_label, sizeof(s_remote.forecast_label));
    copy_json_string(root, "aqi_label", s_remote.aqi_label, sizeof(s_remote.aqi_label));
    copy_json_string(root, "news_title", s_remote.news_title, sizeof(s_remote.news_title));
    copy_json_string(root, "calendar_title", s_remote.calendar_title, sizeof(s_remote.calendar_title));
    copy_json_string(root, "morning_line1", s_remote.morning_line1, sizeof(s_remote.morning_line1));
    copy_json_string(root, "morning_line2", s_remote.morning_line2, sizeof(s_remote.morning_line2));
    copy_json_string(root, "moon_label", s_remote.moon_label, sizeof(s_remote.moon_label));

    cJSON_Delete(root);
}

void screen_feed_merge(screen_info_data_t *d)
{
    if (!d) {
        return;
    }
    d->forecast_temp_c = s_remote.forecast_temp_c;
    d->forecast_min_c = s_remote.forecast_min_c;
    d->forecast_max_c = s_remote.forecast_max_c;
    d->forecast_rain_pct = s_remote.forecast_rain_pct;
    d->forecast_weather_code = s_remote.forecast_weather_code;
    strncpy(d->forecast_label, s_remote.forecast_label, sizeof(d->forecast_label));
    strncpy(d->aqi_label, s_remote.aqi_label, sizeof(d->aqi_label));
    d->aqi_us = s_remote.aqi_us;
    d->aqi_pm25 = s_remote.aqi_pm25;
    strncpy(d->news_title, s_remote.news_title, sizeof(d->news_title));
    d->calendar_has = s_remote.calendar_has;
    d->calendar_hh = s_remote.calendar_hh;
    d->calendar_mm = s_remote.calendar_mm;
    d->calendar_in_min = s_remote.calendar_in_min;
    strncpy(d->calendar_title, s_remote.calendar_title, sizeof(d->calendar_title));
    d->desk_today_min = s_remote.desk_today_min;
    d->desk_session_min = s_remote.desk_session_min;
    d->desk_break_due = s_remote.desk_break_due;
    d->morning_pending = s_remote.morning_pending;
    strncpy(d->morning_line1, s_remote.morning_line1, sizeof(d->morning_line1));
    strncpy(d->morning_line2, s_remote.morning_line2, sizeof(d->morning_line2));
    d->moon_phase_pct = s_remote.moon_phase_pct;
    d->moon_illum_pct = s_remote.moon_illum_pct;
    strncpy(d->moon_label, s_remote.moon_label, sizeof(d->moon_label));
}
