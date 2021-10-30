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

rc_runtime_t* _env;
void env_free(rc_runtime_t* env);

int sync_server_time(rc_timer timer, void* dev);

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

    env = (rc_runtime_t*)rc_malloc(sizeof(rc_runtime_t));
    memset(env, 0, sizeof(rc_runtime_t));
    _env = env;

    memcpy(&env->settings, settings, sizeof(env->settings));
    env->time_update = settings->time_update;

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

void env_free(rc_runtime_t* env)
{
    if (env->timermgr != NULL) { // stop all timer first
        rc_timer_manager_stop_world(env->timermgr);
    }

    if (env->device != NULL) {
        rc_device_uninit(env->device);
        env->device = NULL;
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

