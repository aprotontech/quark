/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     env.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-19 18:25:03
 * @version  0
 * @brief
 *
 **/

#ifndef _APROTON_TECH_ENV_H_
#define _APROTON_TECH_ENV_H_

#include "include/quark/quark.h"

#include "rc_error.h"
#include "rc_device.h"
#include "rc_ans.h"
#include "rc_http_manager.h"
#include "rc_http_request.h"
#include "rc_json.h"
#include "rc_mqtt.h"
#include "rc_network.h"
#include "rc_timer.h"
#include "wifi.h"
#include "backoff.h"

#define SDK_TAG "[QUARK]"
#define NOTIFY_TIME_DIFF 60

typedef void* rc_mqtt_client;
typedef void* protontech;

typedef void* property_manager;
typedef void* location_manager;

typedef struct _rc_runtime_t {
    rc_settings_t settings;

    aidevice device;
    rc_mqtt_client mqtt;
    http_manager httpmgr;
    rc_timer_manager timermgr;
    rc_network_manager netmgr;

    ans_service ansmgr;

    property_manager properties;

    location_manager locmgr;

    rc_session_change session_chanage;
    rc_sync_time_callback time_update;

    rc_kl_status_change kl_change;
    rc_push_callback push_callback;
    rc_instant_callback instant_callback;

    // sync time with server
    rc_timer sync_timer;

    wifi_manager wifimgr;

    int device_registed;

    // buffer, must be last property
    rc_buf_t buff;

} rc_runtime_t;

rc_runtime_t* get_env_instance();

#endif
