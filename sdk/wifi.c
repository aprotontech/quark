#include "env.h"
#include "rc_thread.h"

extern int device_regist(rc_runtime_t* env);

void* _regist_thread(void* env) {
    device_regist((rc_runtime_t*)env);
    return NULL;
}

int quark_on_wifi_status_changed(wifi_manager mgr, int wifi_status) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->settings.wifi_status_callback) {  // callback
        env->settings.wifi_status_callback(wifi_status == RC_WIFI_CONNECTED);
    }

    network_set_available(env->netmgr, NETWORK_MASK_LOCAL,
                          wifi_status == RC_WIFI_CONNECTED);
    if (wifi_status == RC_WIFI_CONNECTED) {
        char ip[16] = {0};
        wifi_get_local_ip(env->wifimgr, ip, NULL);
        rc_property_set("localIp", ip);

        LOGI(SDK_TAG, "wifi connected, ip=%s, %d", ip, env->device_registed);

        if (!env->device_registed) {  // has not registed device
            rc_thread_context_t ctx = {.joinable = 1,
                                       .name = "device-regist",
                                       .priority = -1,
                                       .stack_size = 4096};
            rc_thread_create(_regist_thread, env, &ctx);
        } else {
            rc_service_sync(env->ansmgr, 0);

            const char* token = get_device_session_token(env->device);
            if (token == NULL || token[0] == '\0') {
                // device is not registed, so do it
                rc_device_refresh_atonce(env->device, 100);
            }
        }

        if (time(NULL) < 1000000000 &&
            env->sync_timer != NULL) {                 // time is not synced
            rc_timer_ahead_once(env->sync_timer, 10);  // sync time atonce
        }
    }
    return 0;
}

int rc_set_wifi(const char* ssid, const char* password) {
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_manager_connect(env->wifimgr, ssid, password);
}

int rc_get_wifi_local_ip(char ip[16]) {
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_get_local_ip(env->wifimgr, ip, NULL);
}

int rc_get_wifi_status(int* wifi_status) {
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->wifimgr == NULL) {
        LOGW(SDK_TAG, "sdk is not inited");
        return RC_ERROR_SDK_INIT;
    }

    return wifi_get_local_ip(env->wifimgr, NULL, wifi_status);
}