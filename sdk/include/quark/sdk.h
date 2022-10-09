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
typedef int (*rc_push_callback)(const char* message, int len,
                                rc_buf_t* response);
typedef int (*rc_instant_callback)(const char* message, int len);

typedef int (*rc_sync_time_callback)(int sec, int usec);

typedef int (*rc_property_change)(const char* name, int type, void* value);

typedef int (*rc_wifi_status_callback)(int is_connected);

typedef enum _rc_property_type {
    RC_PROPERTY_INT_VALUE = 0,
    RC_PROPERTY_STRING_VALUE = 1,
    RC_PROPERTY_DOUBLE_VALUE = 2,
    RC_PROPERTY_BOOL_VALUE = 3,
} rc_property_type;

typedef enum _rc_iot_platform_type {
    RC_IOT_QUARK = 0,
    RC_IOT_TENCENT = 1,
} rc_iot_platform_type;

typedef void* rc_hardware_t;

typedef struct _rc_settings_t {
    char* service_url;
    char* default_svr_config;  // default service config
    // input settings
    char* app_id;
    char* client_id;
    char* uuid;
    char* public_key;
    char* app_secret;
    char* production;
    rc_hardware_t* hardware;

    // keepalive(mqtt) status changed
    rc_kl_status_change kl_change;
    // device session changed
    rc_session_change session_chanage;
    // recv push message
    rc_push_callback push_callback;
    // recv instant message
    rc_instant_callback instant_callback;
    // wifi status changed
    rc_wifi_status_callback wifi_status_callback;

    //    rc_sync_time_callback time_update;
    int time_notify_min_diff;

    int property_change_report;
    int porperty_retry_interval;

    int location_report_interval;  // second

    // enable ntp time
    char enable_ntp_time_sync;
    char enable_keepalive;
    char iot_platform;  // rc_iot_platform_type
    char max_device_retry_times;
    char auto_watch_wifi_status;
    char auto_report_location;
    char max_ans_wait_time_sec;  // max wait time, -1: forever

} rc_settings_t;

const char* rc_sdk_version();

rc_settings_t* rc_settings_init(rc_settings_t* setting);

int rc_sdk_init(rc_settings_t* settings, int regist_sync);

int rc_sdk_uninit();

///////////////////////
// DEVICE
const char* rc_get_app_id();
const char* rc_get_client_id();
const char* rc_get_session_token();

///////////////////////
// PROPERTY
int rc_property_define(const char* name, int type, void* init_value,
                       rc_property_change on_change);
int rc_property_set(const char* name, void* value);
int rc_property_flush();
int rc_property_get_values(const char* keys, ...);

///////////////////////
// TIMER
typedef void* rc_timer;
typedef int (*rc_timer_callback)(rc_timer timer, void* usr_data);
rc_timer rc_set_interval(int tick_ms, int repeat_ms, rc_timer_callback on_time,
                         void* usr_data);
int rc_ahead_interval(rc_timer timer, int next_tick_ms);
int rc_clear_interval(rc_timer timer);

///////////////////////
// HTTP
int rc_http_quark_post(const char* service_name, const char* path,
                       const char* body, int timeout, rc_buf_t* response);

///////////////////////
// Downloader

typedef void* rc_downloader;
typedef int (*on_download_callback)(rc_downloader downloader, const char* data,
                                    int len);

typedef enum _rc_download_type_t {
    DOWNLOAD_TO_CALLBACK,
    DOWNLOAD_TO_BUF_QUEUE,
    DOWNLOAD_TO_FILE,
} rc_download_type_t;

rc_downloader rc_downloader_init(const char* url, const char* headers[],
                                 int header_count, int timeout_ms,
                                 rc_download_type_t type, void* data);

int rc_downloader_start(rc_downloader downloader, int new_thread);

int rc_downloader_get_status(rc_downloader downloader, int* total,
                             int* current);

int rc_downloader_uninit(rc_downloader downloader);

////////////////////////
// Location
typedef struct _rc_location_t {
    time_t update_time;
    double latitude;
    double longitude;
} rc_location_t;

int rc_report_location();

rc_location_t* rc_get_current_location();

#endif
