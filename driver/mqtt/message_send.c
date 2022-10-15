/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     message_send.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-02-27 10:59:23
 *
 */

#include "logs.h"
#include "mqtt_client.h"
#include "rc_json.h"
#include "rc_mqtt.h"

#define FROM_LENGTH 64
#define TYPE_LENGTH 32
#define MSGID_LENGTH 32

int mqtt_client_publish(mqtt_client client, const char* topic, const char* body,
                        int len) {
    int ret;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    /*
    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_EVT) == 0 &&
        is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_CMD) == 0) {
        // check cmd topic for test to send myself cmd
        LOGI(MQ_TAG, "invalidate mqtt event topic(%s)", topic);
        return RC_ERROR_MQTT_PUBLISH;
    }

    ret = mqtt_publish(&mqtt->client, (char*)topic, (char*)body, len,
                       INT_QOS_TO_ENUM(MQTT_CMD_QOS));

    LOGI(MQ_TAG, "mqtt(%p) publish(%s), content-length(%d), ret(%d)", mqtt,
         topic, len, ret)
    return ret == 0 ? RC_SUCCESS : RC_ERROR_MQTT_PUBLISH;*/
}

int mqtt_client_rpc_send(mqtt_client client, const char* topic,
                         const char* body, int len, int timeout,
                         rc_buf_t* response) {
    return RC_ERROR_NOT_IMPLEMENT;
}
