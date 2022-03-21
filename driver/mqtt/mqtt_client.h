/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     mqtt_client.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-20 08:45:19
 *
 */

#ifndef _QUARK_MQTT_ADATOR_H_
#define _QUARK_MQTT_ADATOR_H_

#include "rc_mqtt.h"
#include "hashmap.h"
#include "rc_error.h"
#include "rc_event.h"
#include "rc_mutex.h"
#include "rc_thread.h"

#include "mqtt.h"

#include <sys/socket.h>
#include <netinet/in.h>

#define MQTT_CLIENT_PAD_SIZE 384

#define MQTT_TOPIC_PREFIX_LENGTH 60
#define MQTT_PASSWD_MAX_LENGTH 100

#ifndef MQTT_TOPIC_RPC
#define MQTT_TOPIC_RPC "rpc"
#endif

#ifndef MQTT_TOPIC_CMD
#define MQTT_TOPIC_CMD "cmd"
#endif

#ifndef MQTT_TOPIC_EVT
#define MQTT_TOPIC_EVT "evt"
#endif

#ifndef MQTT_TOPIC_ACK
#define MQTT_TOPIC_ACK "ack"
#endif

#ifndef MQTT_RPC_QOS
#define MQTT_RPC_QOS 0
#endif

#ifndef MQTT_CMD_QOS
#define MQTT_CMD_QOS 1
#endif

#define MQTT_TOPIC_SEP '/'

#define MQ_TAG "[MQTT]"

typedef struct _mqtt_subscribe_t {
    void* callback;
    short error;  // 1-error to subscribe, 0-success
    short type;   // 0-normal, 1-rpc, 2-ack
    char topic[4];
} mqtt_subscribe_t;

typedef struct _rc_mqtt_client_t {
    struct mqtt_client client;

    /// REMOTE INFO
    struct sockaddr_in remote_host;

    short is_inited;
    short is_connected;
    short is_exit;

    char* app_id;
    char* client_id;
    char* user_name;
    char* passwd;
    char* topic_prefix;

    mqtt_session_token_callback get_session;
    mqtt_connect_callback on_connect;

    // auto reconnection
    short cur_reconnect_interval_index;
    int last_reconnect_time;

    short force_re_sub;
    short has_sub_failed;
    map_t sub_map;

    rc_mutex mobject;
    rc_event mevent;
    rc_thread sync_thread;

    rc_buf_t* send_buf;
    rc_buf_t* recv_buf;

    rc_buf_t buff;
    char pad[MQTT_CLIENT_PAD_SIZE];
} rc_mqtt_client;

void publish_callback(void** unused, struct mqtt_response_publish* published);

#endif
