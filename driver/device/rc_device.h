/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     device.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-15 17:40:41
 * @version  0
 * @brief
 *
 **/

#ifndef _AIE_DEVICE_H_
#define _AIE_DEVICE_H_

#include "rc_http_manager.h"
#include "rc_timer.h"

typedef void* aidevice;

typedef struct _rc_hardware_info_t {
    char* cpu;
    char* mac;
    char* bsn;
} rc_hardware_info;

typedef void (*device_token_change)(aidevice dev, const char* token, int timeout);

aidevice rc_device_init(http_manager mgr, const char* url, const rc_hardware_info* hardware);

int rc_device_enable_auto_refresh(aidevice device, rc_timer_manager mgr, device_token_change callback);

int rc_device_android_regist(aidevice device, const char* client_id, const char* public_key);

int rc_device_storybox_regist(aidevice device, const char* app_id, const char* uuid, const char* app_secret);

int rc_device_stp_regist(aidevice device, const char* client_id, const char* app_secret);

int rc_device_refresh_atonce(aidevice device, int async);

int rc_device_uninit(aidevice device);

const char* get_device_app_id(aidevice device);
const char* get_device_client_id(aidevice device);
const char* get_device_session_token(aidevice device);

#endif
