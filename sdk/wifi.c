#include "env.h"

int quark_on_wifi_status_changed(wifi_manager mgr, int wifi_status)
{
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->settings.wifi_status_callback) { // callback
        env->settings.wifi_status_callback(wifi_status == RC_WIFI_CONNECTED);
    }

    if (wifi_status == RC_WIFI_CONNECTED) {
        const char* token = get_device_session_token(env->device);
        if (token == NULL || token[0] == '\0') { // device is not registed, so must 
            rc_device_refresh_atonce(env->device, 1);
        }
    }
    return 0;
}

int rc_set_wifi(const char* ssid, const char* password)
{
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_manager_connect(env->wifimgr, ssid, password);
}

int rc_get_wifi_local_ip(char ip[16])
{
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_get_local_ip(env->wifimgr, ip, NULL);
}

int rc_get_wifi_status(int* wifi_status)
{
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_get_local_ip(env->wifimgr, NULL, wifi_status);
}