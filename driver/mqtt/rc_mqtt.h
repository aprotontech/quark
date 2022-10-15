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

#ifndef _QUARK_MQTT_H_
#define _QUARK_MQTT_H_

#include "rc_buf.h"
#include "rc_timer.h"

enum {
    MQTT_STATUS_DISCONNECT = 0,
    MQTT_STATUS_CONNECTED = 1,
};

typedef struct _mqtt_client_session_t {
    const char* username;
    const char* client_id;
    const char* password;
} mqtt_client_session_t;

typedef void* mqtt_client;
typedef int (*mqtt_connect_callback)(mqtt_client client, int status,
                                     const char* cause);
typedef int (*mqtt_cmd_event_callback)(mqtt_client client, const char* from,
                                       const char* message, int message_length,
                                       void* args);
typedef int (*mqtt_rpc_event_callback)(mqtt_client client, const char* from,
                                       const char* message, int message_length,
                                       void* args, rc_buf_t* response);

typedef int (*mqtt_session_callback)(mqtt_client client,
                                     mqtt_client_session_t* session);

mqtt_client mqtt_client_init(const char* app_id,
                             mqtt_session_callback sessioncb);

int mqtt_client_start(mqtt_client client, const char* host, int port, int ssl,
                      mqtt_connect_callback conncb);

int mqtt_client_publish(mqtt_client client, const char* topic, const char* body,
                        int len);

int mqtt_client_rpc_send(mqtt_client client, const char* topic,
                         const char* body, int len, int timeout,
                         rc_buf_t* response);

int mqtt_client_cmd_regist(mqtt_client client, const char* topic,
                           mqtt_cmd_event_callback callback, void* args);

int mqtt_client_rpc_regist(mqtt_client client, const char* type,
                           mqtt_rpc_event_callback callback, void* args);

int mqtt_client_close(mqtt_client client);

#endif
