

#include "env.h"
#include "rc_device.h"
#include "property.h"

#define DEVICE_SESSION_PATH "/api/devices"

int mqtt_client_init(rc_runtime_t* env, const char* app_id, const char* client_id);

void sdk_device_token_callback(aidevice dev, const char* token, int timeout)
{
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->device == dev) {
        if (env->session_chanage != NULL) {
            env->session_chanage(token, timeout);
        }

        // regist mqtt 
        if (env->mqtt == NULL && env->settings.enable_keepalive) { // mqtt is not created
            const char* app_id = get_device_app_id(env->device);
            const char* client_id = get_device_client_id(env->device);
            if (mqtt_client_init(env, app_id, client_id) != 0) {
                LOGI(SDK_TAG, "create mqtt client failed");
                return;
            }
        }
    }
}

int device_regist(rc_runtime_t* env)
{
    ///////////////////////////////////////////////////
    // DEVICE    
    // get device url from ans service
    const char* url = QUARK_API_URL;
    if (url == NULL) {
        LOGI(SDK_TAG, "sdk init failed, ans get device service url failed");
        return RC_ERROR_SDK_INIT;
    }
    
    rc_buf_t* tmp = rc_buf_init(strlen(url) + strlen(DEVICE_SESSION_PATH) + 1);
    strcpy(rc_buf_head_ptr(tmp), url);
    strcat(rc_buf_head_ptr(tmp), DEVICE_SESSION_PATH);

    rc_settings_t* settings = &env->settings;

    env->device = rc_device_init(env->httpmgr, rc_buf_head_ptr(tmp), (rc_hardware_info*)settings->hardware);
    rc_buf_free(tmp);
    if (env->device == NULL) {
        LOGI(SDK_TAG, "sdk init failed, device manager init failed");
        return RC_ERROR_SDK_INIT;
    }

    int wifi_connected;
    if (rc_get_wifi_status(&wifi_connected) != RC_SUCCESS) {
        LOGI(SDK_TAG, "sdk init failed, get wifi status failed");
        return RC_ERROR_SDK_INIT;
    }

    int rc = rc_device_regist(env->device, settings->app_id, settings->uuid, settings->app_secret, wifi_connected);
    LOGI(SDK_TAG, "device regist app(%s), uuid(%s), secret(%s). response=%d", settings->app_id, settings->uuid, settings->app_secret, rc);
    
    rc_device_enable_auto_refresh(env->device, env->timermgr, sdk_device_token_callback);

    ///////////////////////////////////////////////////
    // PROPERTY
    env->properties = property_manager_init(env, settings->property_change_report, settings->porperty_retry_interval);
    if (env->properties == NULL) {
        LOGE(SDK_TAG, "property_manager_init failed");
        return RC_ERROR_SDK_INIT;
    }

    // define localip property
    rc_property_define("localIp", RC_PROPERTY_STRING_VALUE, NULL, NULL);

    return RC_SUCCESS;
}