/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_error.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 22:12:02
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_RC_ERROR_H_
#define _QUARK_RC_ERROR_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define rc_malloc malloc
#define rc_free free

#define DECLEAR_REAL_VALUE(type, name, ptr) \
    type* name = (type*)(ptr);              \
    if (name == NULL) return RC_ERROR_INVALIDATE_INPUT;

#define PRINTSTR(s) ((s) != NULL ? (s) : "null")

#define RC_SUCCESS 0
#define RC_ERROR_JSON_FORMAT 100
#define RC_ERROR_UNKOWN_HOST 1000
#define RC_ERROR_CREATE_SOCKET 1001  // create socket error
#define RC_ERROR_INVALIDATE_HTTPCLIENT 1002
#define RC_ERROR_CREATE_REQUEST_FAILED 1003
#define RC_ERROR_INVALIDATE_INPUT 1004
#define RC_ERROR_EXECUTE_REQUEST_FAILED 1005

#define RC_ERROR_SVRMGR_INIT 1006
#define RC_ERROR_SVRMGR_REINIT 1007
#define RC_ERROR_SVRMGR_RELOAD 1008
#define RC_ERROR_SVRMGR_NODNS 1009

#define RC_ERROR_REPEAT_CALL 1050

#define RC_ERROR_NOT_IMPLEMENT 1100
#define RC_ERROR_REGIST_DEVICE 1101
#define RC_ERROR_CRYPT_INIT 1102
#define RC_ERROR_DECRYPT 1103
#define RC_ERROR_ENCRYPT 1104
#define RC_ERROR_REPEAT_REFRESH 1105

#define RC_ERROR_HTTP_SEND 1020
#define RC_ERROR_HTTP_RECV 1021
#define RC_ERROR_DOWNLOAD_FAILED 1050

#define RC_ERROR_MQTT_SUBSCRIBE 1200
#define RC_ERROR_MQTT_PUBLISH 1201
#define RC_ERROR_MQTT_SKIP_CONN 1202
#define RC_ERROR_MQTT_CONNECT 1203
#define RC_ERROR_MQTT_RPC 1204

#define RC_ERROR_SDK_INIT 1300
#define RC_ERROR_SDK_UNINIT 1301

#define RC_ERROR_ASR_REQUEST 1400
#define RC_ERROR_ASTRO_REQUEST 1401
#define RC_ERROR_TTS_REQUEST 1402

#define RC_ERROR_NET_FORMAT 11000
#define RC_ERROR_NET_NOT_ROUTER 11001

#endif
