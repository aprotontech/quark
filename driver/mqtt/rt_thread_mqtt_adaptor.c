/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     mqtt.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-17 00:25:10
 * @version  0
 * @brief
 *
 **/

#include "rc_mqtt.h"
#include "logs.h"
#include "mqtt_adaptor.h"

#ifdef __QUARK_RTTHREAD__

#include "paho_mqtt.h"

void rtos_mqtt_sub_callback(MQTTClient *c, MessageData *msg_data);
void rtos_mqtt_connect_callback(MQTTClient *c);
void rtos_mqtt_online_callback(MQTTClient *c);
void rtos_mqtt_offline_callback(MQTTClient *c);


int MQTTClient_create(void** handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context)
{
    MQTTClient* client = (MQTTClient*)rc_malloc(sizeof(MQTTClient));
    memset(client, 0, sizeof(MQTTClient));
    client->buf_size = client->readbuf_size = MQTT_PUB_SUB_BUF_SIZE;
    client->buf = rc_malloc(client->buf_size);
    client->readbuf = rc_malloc(client->readbuf_size);
    if (!client->buf || !client->readbuf) {
        LOGI(MQ_TAG, "no memory for MQTT client buffer!");
        rc_free(client);
        return -1;
    }
    /* set event callback function */
    client->connect_callback = rtos_mqtt_connect_callback;
    client->online_callback = rtos_mqtt_online_callback;
    client->offline_callback = rtos_mqtt_offline_callback;
    client->uri = rc_copy_string(serverURI);
    client->isconnected = 0;

    /* set default subscribe event callback */
    client->defaultMessageHandler = rtos_mqtt_sub_callback;
    *handle = client;
    return 0;
}

int MQTTClient_connect(void* handle, MQTTClient_connectOptions* options)
{
    MQTTClient* client = (MQTTClient*)handle;
    client->condata = *options;
    return paho_mqtt_start(client);
}

int MQTTClient_isConnected(void* handle)
{
    MQTTClient* client = (MQTTClient*)handle;
    return client != NULL ? client->isconnected : 0;
}

void MQTTClient_freeMessage(MQTTClient_message** msg)
{

}

int MQTTClient_disconnect(void* handle, int timeout)
{
    return 0;
}

void MQTTClient_free(void* ptr)
{
}

void MQTTClient_destroy(void** handle)
{
    if (handle != NULL) {
        MQTTClient* client = *(MQTTClient**)handle;
        //MQTTDisconnect(client); // send disconnect package

        if (client->buf != NULL) {
            rc_free(client->buf);
            client->buf = NULL;
        }
        if (client->readbuf != NULL) {
            rc_free(client->readbuf);
            client->readbuf = NULL;
        }

        if (client->uri != NULL) {
            rc_free((char*)client->uri);
            client->uri = NULL;
        }
        
        if (client->messageHandlers[0].topicFilter != NULL) {
            rc_free((char*)client->messageHandlers[0].topicFilter);
            client->messageHandlers[0].topicFilter = NULL;
        }

        if (client != NULL) {
            rc_free(client);
            *(MQTTClient**)handle = NULL;
        }
    }
}

int MQTTClient_subscribe(void* handle, const char* topic, int qos)
{
/*    int i;
    MQTTClient* client = (MQTTClient*)handle;
    for (i = 0 ; i < MAX_MESSAGE_HANDLERS; ++ i) {
        if (client->messageHandlers[i].topicFilter != NULL &&
            strcmp(client->messageHandlers[i].topicFilter, topic) == 0) {
                LOGI("[MQTT]", "topic(%s) had subscribed", topic);
                return 0;
            }
    }

    for (i = 0 ; i < MAX_MESSAGE_HANDLERS; ++ i) {
        if (client->messageHandlers[i].topicFilter == NULL) {
            client->messageHandlers[i].topicFilter = topic;
            client->messageHandlers[i].callback = rtos_mqtt_sub_callback;
            client->messageHandlers[i].qos = qos;
            break;
        }
    }

    return i < MAX_MESSAGE_HANDLERS ? 0 : -1;*/
    return 0;
}

int MQTTClient_publish(void* handle, const char* topicName, int payloadlen, void* payload, int qos, int retained,
																 MQTTClient_deliveryToken* dt)
{
    int ret = 0;
    MQTTClient* client = (MQTTClient*)handle;
    MQTTMessage message;
    message.qos = qos;
    message.retained = retained;
    message.payload = payload;
    message.payloadlen = payloadlen;

    ret = MQTTPublish(client, topicName, &message);
    
    if (dt != NULL) {
        *dt = message.id;
    }
    
    return ret;
}

int MQTTClient_setCallbacks (void* handle, void *context, MQTTClient_connectionLost *cl, 
    MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc)
{
    MQTTClient* client = (MQTTClient*)handle;
    rc_mqtt_client* mqtt = (rc_mqtt_client*)context;

    char* topic = (char*)rc_malloc(strlen(mqtt->topic_prefix) + 4);
    strcpy(topic, mqtt->topic_prefix);
    strcat(topic, "/#");
    client->messageHandlers[0].topicFilter = topic;
    client->messageHandlers[0].callback = rtos_mqtt_sub_callback;
    client->messageHandlers[0].qos = 1;

    client->context = context;
    return 0;
}

extern int on_rpc_message(rc_mqtt_client* mqtt, const char *topic, MQTTClient_message *message);
void rtos_mqtt_sub_callback(MQTTClient* handle, MessageData *msg_data)
{
    char *topic = NULL;
    
    MQTTClient* client = (MQTTClient*)handle;
    rc_mqtt_client* mqtt = client != NULL ? (rc_mqtt_client*)client->context : NULL;
    LOGI("[MQTT]", "rtos_mqtt_sub_callback");

    // *((char *)msg_data->message->payload + msg_data->message->payloadlen) = '\0';
    // rt_kprintf("mqtt sub default callback: %.*s %.*s\n",
    //            msg_data->topicName->lenstring.len,
    //            msg_data->topicName->lenstring.data,
    //            msg_data->message->payloadlen,
    //            (char *)msg_data->message->payload);

    if (mqtt != NULL) {
        topic = (char*)rc_malloc(msg_data->topicName->lenstring.len + 1);
        if (topic == NULL) {
            LOGW("[MQTT]", "malloc topic buf failed");
            return;
        }
        memcpy(topic, msg_data->topicName->lenstring.data, msg_data->topicName->lenstring.len);
        topic[msg_data->topicName->lenstring.len] = '\0';
        on_rpc_message(mqtt, topic, msg_data->message);
        rc_free(topic);
    }
    return;
}

void rtos_mqtt_connect_callback(MQTTClient *handle)
{
    LOGI("[MQTT]", "rtos_mqtt_connect_callback, start to connect");
}

extern int mqtt_on_timer(rc_timer timer, void* client);
void rtos_mqtt_online_callback(MQTTClient *handle)
{
    MQTTClient* client = (MQTTClient*)handle;
    rc_mqtt_client* mqtt = client != NULL ? (rc_mqtt_client*)client->context : NULL;
    LOGI("[MQTT]", "rtos_mqtt_online_callback, mqtt is online now");
    if (mqtt != NULL) {
        mqtt_on_timer(mqtt->reconn_timer, mqtt);
    }

    if (mqtt != NULL && mqtt->on_connect != NULL) {
        mqtt->on_connect(mqtt, MQTT_STATUS_CONNECTED, "success");
    }
}

void rtos_mqtt_offline_callback(MQTTClient *handle)
{
    MQTTClient* client = (MQTTClient*)handle;
    rc_mqtt_client* mqtt = client != NULL ? (rc_mqtt_client*)client->context : NULL;
    LOGI("[MQTT]", "rtos_mqtt_offline_callback, mqtt is offline now");
    if (mqtt != NULL && mqtt->on_connect != NULL) {
        mqtt->on_connect(mqtt, MQTT_STATUS_DISCONNECT, "disconnect");
    }
}

#endif
