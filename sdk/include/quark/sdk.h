/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     sdk.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-19 19:17:15
 * @version  0
 * @brief
 *
 **/

#ifndef _APROTON_TECH_SDK_H_
#define _APROTON_TECH_SDK_H_

#include "rc_buf.h"

#ifndef QUARKSDK_VERSION
#define QUARKSDK_VERSION "1.0"
#endif

typedef int (*rc_kl_status_change)(int online, const char* cause);
typedef int (*rc_session_change)(const char* session, int timeout);
typedef int (*rc_push_callback)(const char* message, int len, rc_buf_t* response);
typedef int (*rc_instant_callback)(const char* message, int len);

typedef struct cJSON cJSON;
typedef int (*rc_netcmd_dispatch_callback)(const char* key, cJSON* data);
typedef int (*rc_netrpc_dispatch_callback)(const char* key, cJSON* data, rc_buf_t* response);

typedef int (*rc_time_callback)(int sec, int usec);

typedef int (*rc_property_change)(const char* name, int type, void* value);

typedef int (*rc_wifi_status_callback)(int is_connected);

typedef enum _rc_property_type {
    RC_PROPERTY_INT_VALUE = 0,
    RC_PROPERTY_STRING_VALUE = 1,
    RC_PROPERTY_DOUBLE_VALUE = 2,
    RC_PROPERTY_BOOL_VALUE = 3,
} rc_property_type;

typedef void* rc_hardware_t;

typedef struct _rc_settings_t {
    // input settings
    char* app_id;
    char* client_id;
    char* uuid;
    char* public_key;
    char* app_secret;
    char* production;
    rc_hardware_t* hardware;

    rc_kl_status_change kl_change;
    rc_session_change session_chanage;
    rc_push_callback push_callback;
    rc_instant_callback instant_callback;

    rc_wifi_status_callback wifi_status_callback;

    rc_time_callback time_update;
    int time_notify_min_diff;

    int property_change_report;
    int porperty_retry_interval;

    char enable_ntp_time_sync;
    char new_thread_init;
    char enable_keepalive; 
    char max_device_retry_times;
    char auto_watch_wifi_status;
} rc_settings_t;

const char* rc_sdk_version();

rc_settings_t* rc_settings_init(rc_settings_t* setting);

int rc_sdk_init(const char* env_name, int enable_debug_client_info, rc_settings_t* settings);

int rc_sdk_uninit();


#endif