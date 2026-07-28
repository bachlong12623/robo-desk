#include "safety.h"
#include "sensors.h"
#include "power_monitor.h"
#include "esp_log.h"

static const char *TAG = "safety";
static safety_state_t s_st;

void safety_init(void)
{
    s_st.halt_motors = false;
}

void safety_update(void)
{
    const sensor_snapshot_t *s = sensors_get();
    const power_status_t *p = power_monitor_get();
    s_st.cliff = s->cliff_l || s->cliff_r;
    s_st.tip = s->tipped || s->shock;
    s_st.batt_critical = p->critical;
    s_st.tof_emergency = s->tof_valid && !s->tof_stale && s->tof_mm > 0 && s->tof_mm < 40;
    s_st.halt_motors = s_st.cliff || s_st.tip || s_st.batt_critical || s_st.tof_emergency;
    if (s_st.halt_motors) {
        ESP_LOGW(TAG, "HALT cliff=%d tip=%d batt=%d tof=%d",
                 s_st.cliff, s_st.tip, s_st.batt_critical, s_st.tof_emergency);
    }
}

const safety_state_t *safety_get(void)
{
    return &s_st;
}

bool safety_allows_motion(void)
{
    return !s_st.halt_motors;
}
