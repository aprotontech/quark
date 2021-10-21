/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_mqtt.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-17 00:25:18
 * @version  0
 * @brief
 *
 **/

#ifndef _AIE_MQTT_H_
#define _AIE_MQTT_H_

#include "rc_buf.h"
#include "rc_timer.h"

enum {
    MQTT_STATUS_DISCONNECT = 0,
    MQTT_STATUS_CONNECTED = 1,
};

typedef void* mqtt_client;
typedef int (*mqtt_connect_callback)(mqtt_client client, int status, const char* cause);
typedef int (*mqtt_subscribe_callback)(mqtt_client client, const char* message, int message_length);
typedef int (*mqtt_rpc_event_callback)(mqtt_client client, const char* message, int message_length, rc_buf_t* response);

typedef const char* (*mqtt_session_token_callback)(const char* client_id);

mqtt_client rc_mqtt_create(const char* host, int port, 
        const char* app_id, const char* client_id, mqtt_session_token_callback callback);

int rc_mqtt_connect(mqtt_client client);
int rc_mqtt_enable_auto_connect(mqtt_client client, rc_timer_manager mgr, mqtt_connect_callback callback);

int rc_mqtt_subscribe(mqtt_client client, const char* topic, mqtt_subscribe_callback callback);

int rc_mqtt_publish(mqtt_client client, const char* topic, const char* body, int len);

int rc_mqtt_rpc_send(mqtt_client client, const char* topic, const char* body, int len, int timeout, rc_buf_t* response);

int rc_mqtt_rpc_event(mqtt_client client, const char* topic, mqtt_rpc_event_callback callback);

int rc_mqtt_close(mqtt_client client);

#endif
