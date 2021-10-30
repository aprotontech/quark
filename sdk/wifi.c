#include "env.h"

int quark_on_wifi_status_changed(wifi_manager mgr, int wifi_status)
{
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->settings.wifi_status_callback) { // callback
        env->settings.wifi_status_callback(wifi_status == RC_WIFI_CONNECTED);
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