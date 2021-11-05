

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
    setting->enable_ntp_time_sync = 1;
    setting->hardware = rc_malloc(sizeof(rc_hardware_info));
    memset(setting->hardware, 0, sizeof(rc_hardware_info));

    rc_hardware_info* hi = (rc_hardware_info*)setting->hardware;
#if defined(__QUARK_FREERTOS__)
    hi->bsn = "testdev";
    hi->cpu = "abcdef";
    hi->mac = "123456";
#endif
    return setting;
}
