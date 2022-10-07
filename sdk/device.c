

#include "env.h"
#include "property.h"
#include "rc_device.h"

#define DEVICE_SESSION_PATH "/device/session"

int mqtt_client_init(rc_runtime_t* env, const char* app_id,
                     const char* client_id);

void sdk_device_token_callback(aidevice dev, const char* token, int timeout) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device == dev) {
        network_set_available(env->netmgr, NETWORK_MASK_SESSION, 1);
        if (env->session_chanage != NULL) {
            env->session_chanage(token, timeout);
        }

        // regist mqtt
        if (env->mqtt == NULL &&
            env->settings.enable_keepalive) {  // mqtt is not created
            const char* app_id = get_device_app_id(env->device);
            const char* client_id = get_device_client_id(env->device);
            if (mqtt_client_init(env, app_id, client_id) != 0) {
                LOGI(SDK_TAG, "create mqtt client failed");
                return;
            }
        }

        if (env->settings.auto_report_location && env->time_update == 0) {
            rc_report_location();
        }
    }
}

int device_regist(rc_runtime_t* env) {
    // get service from remote
    http_request_url_info_t url;
    rc_service_protocol_info_t info;
    rc_settings_t* settings = &env->settings;

    memset(&url, 0, sizeof(url));

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
    if (rc_service_query(env->ansmgr, "DEVICE", "HTTP", &info) != 0) {
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

    rc_device_enable_auto_refresh(env->device, env->timermgr,
                                  sdk_device_token_callback);

    return RC_SUCCESS;
}