#pragma once
#include "screen_info.h"

/* Remote content feed (Pi 5 -> MQTT -> ESP32).
 * Carries non-local content like weather forecast, AQI, news, calendar,
 * desk tracker summaries, morning greeting, and moon phase.
 */

void screen_feed_reset(void);
void screen_feed_update_json(const char *json);
void screen_feed_merge(screen_info_data_t *d);
bool screen_feed_is_fresh(void);
