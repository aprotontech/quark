/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     env.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-19 18:20:38
 * @version  0
 * @brief
 *
 **/

#include "env.h"
#include "property.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_system.h"
#endif

#define ENV_BUFF_SIZE 64

rc_runtime_t* _env;
void env_free(rc_runtime_t* env);

int sync_server_time(rc_timer timer, void* dev);
int rc_enable_ntp_sync_time();
int device_regist(rc_runtime_t* env);
int net_dispatch_uninit(rc_net_dispatch_mgr_t dismgr);
int append_hardware_info(rc_runtime_t* env);

int quark_on_wifi_status_changed(wifi_manager mgr, int wifi_status);

rc_runtime_t* get_env_instance()
{
    return _env;
}

int rc_sdk_init(const char* env_name, int enable_debug_client_info, rc_settings_t* settings)
{
    int rc = 0, port, i;
    rc_runtime_t* env = NULL;
    const char* url;
    rc_buf_t* tmp;
    char *host = NULL;

    if (_env != NULL) {
        LOGW(SDK_TAG, "quark sdk had init");
        return RC_ERROR_SDK_INIT;
    }

    LOGW(SDK_TAG, "quark sdk start to init");
    if (settings->enable_ntp_time_sync) {
        rc_enable_ntp_sync_time();
    }

    env = (rc_runtime_t*)rc_malloc(sizeof(rc_runtime_t) + ENV_BUFF_SIZE);
    memset(env, 0, sizeof(rc_runtime_t));
    _env = env;

    env->buff = rc_buf_stack();
    env->buff.total = ENV_BUFF_SIZE; // expand buffer size

    memcpy(&env->settings, settings, sizeof(env->settings));
    //env->time_update = settings->time_update;

    // hardware info
    append_hardware_info(env);

    env->httpmgr = http_manager_init();
    if (env->httpmgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, http manager init failed");
        return RC_ERROR_SDK_INIT;
    }

    env->timermgr = rc_timer_manager_init();
    if (env->timermgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, timer manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    if (env->time_update != NULL) {
        int randtm = (rand() % 1800) + 7200;
        env->sync_timer = rc_timer_create(env->timermgr, 1000, randtm * 1000, sync_server_time, env);
    }

    env->wifimgr = wifi_manager_init(quark_on_wifi_status_changed);
    if (env->wifimgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, wifi manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    if (device_regist(env) != 0) {
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    return 0;
}

int rc_sdk_uninit()
{
    if (_env == NULL) {
        LOGW(SDK_TAG, "sdk uninit failed, had not inited");
        return RC_ERROR_SDK_UNINIT;
    }
    env_free(_env);
    _env = NULL;
    return 0;
}

int append_hardware_info(rc_runtime_t* env)
{
    if (env->settings.hardware == NULL) {
        env->settings.hardware = rc_malloc(sizeof(rc_hardware_info));
        memset(env->settings.hardware, 0, sizeof(rc_hardware_info));
    }

    // get mac address
    char* mac_str = rc_buf_tail_ptr(&env->buff);
#if defined(__QUARK_FREERTOS__)
    {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(mac_str, RC_BUF_LEFT_SIZE(&env->buff), "%x:%x:%x:%x:%x:%x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        env->buff.length += 18; // skip mac
    }
#endif

    rc_hardware_info* hi = (rc_hardware_info*)env->settings.hardware;
    if (hi->mac == NULL) hi->mac = mac_str;
    if (hi->bsn == NULL) hi->bsn = mac_str;
    if (hi->cpu == NULL) hi->cpu = mac_str;

    if (env->settings.uuid == NULL) {
        env->settings.uuid = mac_str;
    }

    return 0;
}

void env_free(rc_runtime_t* env)
{
    if (env->timermgr != NULL) { // stop all timer first
        rc_timer_manager_stop_world(env->timermgr);
    }

    if (env->net_dispatch != NULL) {
        net_dispatch_uninit(env->net_dispatch);
        env->net_dispatch = NULL;
    }

    if (env->mqtt != NULL) {
        rc_mqtt_close(env->mqtt);
        env->mqtt = NULL;
    }

    if (env->device != NULL) {
        rc_device_uninit(env->device);
        env->device = NULL;
    }

    if (env->properties != NULL) {
        property_manager_uninit(env->properties);
        env->properties = NULL;
    }

    if (env->httpmgr != NULL) {
        http_manager_uninit(env->httpmgr);
        env->httpmgr = NULL;
    }

    if (env->timermgr != NULL) {
        rc_timer_manager_uninit(env->timermgr);
        env->timermgr = NULL;
    }

    if (env->wifimgr != NULL) {
        wifi_manager_uninit(env->wifimgr);
        env->wifimgr = NULL;
    }

    if (env == _env) {
        _env = NULL;
    }
    
    rc_free(env);
}

