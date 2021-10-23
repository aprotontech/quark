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

#ifndef _AIE_MQTT_ADATOR_H_
#define _AIE_MQTT_ADATOR_H_

#include "rc_mqtt.h"
#include "hashmap.h"
#include "rc_error.h"
#include "rc_event.h"
#include "rc_mutex.h"

#ifdef __QUARK_RTTHREAD__
#include "paho_mqtt.h"
#else
#include "MQTTClient.h"
#endif

#ifndef MQTT_TOPIC_PREFIX_LENGTH
#define MQTT_TOPIC_PREFIX_LENGTH 60
#endif

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

#ifndef MQTT_PUB_SUB_BUF_SIZE
#define MQTT_PUB_SUB_BUF_SIZE 1024
#endif

#define MQTT_TOPIC_SEP '/'

#define MQ_TAG "[MQTT]"

typedef struct _mqtt_subscribe_t {
    void* callback;
    short error; // 1-error to subscribe, 0-success
    short type; // 0-normal, 1-rpc, 2-ack
    char topic[4];
} mqtt_subscribe_t;

#ifdef __QUARK_RTTHREAD__
typedef MQTTClient* inner_mqtt_ptr;
typedef int MQTTClient_deliveryToken;
typedef MQTTMessage MQTTClient_message;
typedef MQTTPacket_connectData MQTTClient_connectOptions;
typedef void MQTTClient_connectionLost(void *context, char *cause);
typedef int MQTTClient_messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
typedef void MQTTClient_deliveryComplete(void *context, MQTTClient_deliveryToken dt);

int MQTTClient_create(void** handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context);
int MQTTClient_connect(void* handle, MQTTClient_connectOptions* options);
int MQTTClient_isConnected(void* handle);        
int MQTTClient_disconnect(void* handle, int timeout);
void MQTTClient_free(void* ptr);
void MQTTClient_destroy(void** handle);
void MQTTClient_freeMessage(MQTTClient_message** msg);
int MQTTClient_subscribe(void* handle, const char* topic, int qos);
int MQTTClient_publish(void* handle, const char* topicName, int payloadlen, void* payload, int qos, int retained,
																 MQTTClient_deliveryToken* dt);
int MQTTClient_setCallbacks(void* handle, void *context, MQTTClient_connectionLost *cl, 
    MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);
#define MQTTClient_connectOptions_initializer {}
#define MQTTCLIENT_SUCCESS 0
#define MQTTCLIENT_PERSISTENCE_NONE 0
#endif


typedef struct _rc_mqtt_client_t {
    void* client;
    MQTTClient_connectOptions conn_opts;

    int connectResult;

    char* client_id;
    mqtt_session_token_callback get_session;
    mqtt_connect_callback on_connect;

    char topic_prefix[MQTT_TOPIC_PREFIX_LENGTH];

    rc_mutex mobject;

    // auto reconnection
    rc_timer reconn_timer;
    short cur_reconnect_interval_index;
    int last_reconnect_time;

    short force_re_sub;
    short has_sub_failed;
    map_t sub_map;
} rc_mqtt_client;

#endif
