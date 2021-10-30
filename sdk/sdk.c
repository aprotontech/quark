

#include "env.h"
#include "rc_timer.h"

const char* rc_sdk_version()
{
    return QUARKSDK_VERSION;
}

rc_settings_t* rc_settings_init(rc_settings_t* setting)
{
    memset(setting, 0, sizeof(rc_settings_t));
    setting->time_notify_min_diff = 60;
    setting->max_device_retry_times = 1;
    setting->production = "";
    setting->property_change_report = 1;
    setting->porperty_retry_interval = 30 * 1000;
    return setting;
}
