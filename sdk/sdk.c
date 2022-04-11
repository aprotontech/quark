

#include "env.h"
#include "rc_timer.h"

const char* rc_sdk_version() { return QUARKSDK_VERSION; }

rc_settings_t* rc_settings_init(rc_settings_t* setting) {
    memset(setting, 0, sizeof(rc_settings_t));
    setting->time_notify_min_diff = 60;
    setting->max_device_retry_times = 1;
    setting->production = "";
    setting->property_change_report = 1;
    setting->porperty_retry_interval = 30 * 1000;
    setting->enable_ntp_time_sync = 1;
    setting->auto_report_location = 1;
    setting->location_report_interval = 3600 * 12;
    return setting;
}

const char* rc_get_app_id() {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device != NULL) {
        return get_device_app_id(env->device);
    }

    return NULL;
}

const char* rc_get_client_id() {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device != NULL) {
        return get_device_client_id(env->device);
    }

    return NULL;
}

const char* rc_get_session_token() {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device != NULL) {
        return get_device_session_token(env->device);
    }

    return NULL;
}
