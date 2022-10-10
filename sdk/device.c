

#include "env.h"
#include "property.h"
#include "rc_device.h"
#include "location.h"

#define DEVICE_SESSION_PATH "/device/session"

extern int mqtt_auto_connect(rc_runtime_t* env);

void sdk_device_token_callback(aidevice dev, const char* token, int timeout) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device == dev) {
        network_set_available(env->netmgr, NETWORK_MASK_SESSION, 1);
        if (env->session_chanage != NULL) {
            env->session_chanage(token, timeout);
        }

        // regist mqtt
        if (env->settings.enable_keepalive) {  // mqtt is not created
            mqtt_auto_connect(env);
        }

        if (env->settings.auto_report_location) {
            location_manager_retry_report_atonce(env->locmgr);
        }
    }
}

int device_regist(rc_runtime_t* env) {
    // get service from remote
    http_request_url_info_t url;
    rc_service_protocol_info_t info;
    rc_settings_t* settings = &env->settings;

    memset(&url, 0, sizeof(url));

    LOGI(SDK_TAG, "device try to regist....");

    int waitsec = settings->max_ans_wait_time_sec;
    if (waitsec != 0) {
        rc_service_sync(env->ansmgr, 1);

        time_t s = time(NULL);
        unsigned int logtimes = 0;
        while ((waitsec < 0 || (time(NULL) - s) < waitsec) &&
               rc_service_is_synced(env->ansmgr) != 1) {
            if (logtimes++ % 20 == 0) {
                LOGD(SDK_TAG, "sdk init ans service is not synced, wait...");
            }
            rc_sleep(100);
        }
    }

    // get device url from ans service
    if (rc_service_query(env->ansmgr, "DEVICE", "http", &info) != 0) {
        LOGW(SDK_TAG, "sdk init get device service failed");
        return RC_ERROR_REGIST_DEVICE;
    }

    url.host = rc_copy_string(info.host);
    url.ip = rc_copy_string(info.ip);
    url.port = info.port;
    url.schema = 0;
    url.path =
        (char*)rc_malloc(strlen(info.prefix) + strlen(DEVICE_SESSION_PATH) + 1);
    strcpy(url.path, info.prefix);
    strcat(url.path, DEVICE_SESSION_PATH);

    int netok = network_is_available(env->netmgr, NETWORK_MASK_LOCAL);

    int rc = rc_device_regist(env->device, &url, settings->app_id,
                              settings->uuid, settings->app_secret, netok);
    LOGI(SDK_TAG, "device regist app(%s), uuid(%s), secret(%s). response=%d",
         settings->app_id, settings->uuid, settings->app_secret, rc);

    if (netok != 0) {
        network_set_available(env->netmgr, NETWORK_MASK_SESSION,
                              get_device_session_token(env->device) != NULL);
    }
    rc_device_enable_auto_refresh(env->device, env->timermgr,
                                  sdk_device_token_callback);

    // setup mqtt client
    if (env->settings.enable_keepalive) {
        mqtt_auto_connect(env);
    }

    env->device_registed = 1;

    return RC_SUCCESS;
}