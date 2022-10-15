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
#include "backoff.h"

#include "mqtt.h"
#include "mqtt_defines.h"

#include <sys/socket.h>
#include <netinet/in.h>

#define MQ_TAG "[MQTT]"

typedef struct _mqtt_subscribe_t {
    void* callback;
    void* args;
    short error;  // 1-error to subscribe, 0-success
    short type;   // 0-unknown, 1-cmd,2-rpc,3-ack
    char key[4];
} mqtt_subscribe_t;

typedef struct _mqtt_publish_data_t {
    list_link_t link;
    char topic[100];
    rc_buf_t buff;
} mqtt_publish_data_t;

typedef struct _rc_mqtt_client_t {
    struct mqtt_client client;

    /// REMOTE INFO
    struct sockaddr_in remote_host;

    short is_inited;
    short is_connected;
    short is_exit;

    char* app_id;
    char* topic_prefix;
    int topic_prefix_length;
    mqtt_client_session_t session;

    mqtt_session_callback get_session;
    mqtt_connect_callback on_connect;

    // auto reconnection
    backoff_algorithm_t reconnect_backoff;

    map_t sub_map;

    rc_mutex mobject;
    rc_event mevent;
    rc_thread sync_thread;

    list_link_t msg_to_publish;

    rc_buf_t* send_buf;
    rc_buf_t* recv_buf;

    rc_buf_t buff;
    char pad[MQTT_CLIENT_PAD_SIZE];
} rc_mqtt_client;

void publish_callback(void** unused, struct mqtt_response_publish* published);

#endif
