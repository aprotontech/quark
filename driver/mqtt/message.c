/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     message.c
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

int is_topic_match(const char* topic, const char* client_prefix,
                   const char* func_prefix) {
    const char* org = topic;

    while (*client_prefix != '\0') {
        if (*topic == '\0' || *topic != *client_prefix) return 0;
        ++client_prefix;
        ++topic;
    }

    if (*topic == '\0' || *topic != MQTT_TOPIC_SEP) return 0;
    ++topic;

    while (*func_prefix != '\0') {
        if (*topic == '\0' || *topic != *func_prefix) return 0;
        ++func_prefix;
        ++topic;
    }

    return topic - org;
}

static int _mqtt_subscribe(mqtt_client client, const char* topic,
                           const char* topic_type, void* callback) {
    int ret;
    any_t val;
    mqtt_subscribe_t* sub = NULL;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (is_topic_match(topic, mqtt->topic_prefix, topic_type) == 0) {
        LOGI(MQ_TAG, "mqtt(%p), invalidate mqtt %s topic(%s)", mqtt, topic_type,
             topic);
        return RC_ERROR_MQTT_SUBSCRIBE;
    }

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)topic, &val) == MAP_MISSING) {
        sub = (mqtt_subscribe_t*)rc_malloc(sizeof(mqtt_subscribe_t) +
                                           strlen(topic));
        memset(sub, 0, sizeof(mqtt_subscribe_t));
        sub->type = 0;
        sub->callback = callback;
        strcpy(sub->topic, topic);
        ret = hashmap_put(mqtt->sub_map, sub->topic, sub);
        mqtt->has_sub_failed = 1;
        rc_mutex_unlock(mqtt->mobject);

        if (ret != MAP_OK) {
            LOGW(MQ_TAG, "mqtt(%p) bind topic(%s) failed with(%d)", mqtt, topic,
                 ret);
            rc_free(sub);
            return RC_ERROR_MQTT_SUBSCRIBE;
        }
    } else {
        ret = MAP_OK;
        ((mqtt_subscribe_t*)val)->callback = callback;  // update callback
        rc_mutex_unlock(mqtt->mobject);
    }

    LOGI(MQ_TAG, "mqtt(%p) %s map (%s) -> %p", mqtt, topic_type, topic,
         callback);
    return RC_SUCCESS;
}

int rc_mqtt_cmd_subscribe(mqtt_client client, const char* topic,
                          mqtt_subscribe_callback callback) {
    return _mqtt_subscribe(client, topic, MQTT_TOPIC_CMD, callback);
}

int rc_mqtt_rpc_subscribe(mqtt_client client, const char* topic,
                          mqtt_rpc_event_callback callback) {
    return _mqtt_subscribe(client, topic, MQTT_TOPIC_RPC, callback);
}

int rc_mqtt_publish(mqtt_client client, const char* topic, const char* body,
                    int len) {
    int ret;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
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
    return ret == 0 ? RC_SUCCESS : RC_ERROR_MQTT_PUBLISH;
}

int rc_mqtt_rpc_send(mqtt_client client, const char* topic, const char* body,
                     int len, int timeout, rc_buf_t* response) {
    return RC_ERROR_NOT_IMPLEMENT;
}

int _get_from_type_id(const char* topic, char from[FROM_LENGTH],
                      char type[TYPE_LENGTH], char msgid[MSGID_LENGTH]) {
    int i = 0;
    const char *f = NULL, *t = NULL, *m = NULL;
    if (topic[0] == '/') {
        f = topic;
        if ((t = strchr(f + 1, '/')) != NULL) {
            m = strchr(t + 1, '/');
        }
    }

    if (f == NULL || t == NULL || m == NULL) {
        return -1;
    }

    if (t - f - 1 >= FROM_LENGTH) {
        return -1;
    }
    memcpy(from, f + 1, t - f - 1);

    if (m - t - 1 >= TYPE_LENGTH) {
        return -1;
    }

    memcpy(type, t + 1, m - t - 1);

    if (strlen(m + 1) >= MSGID_LENGTH || strlen(m + 1) <= 0) {
        return -1;
    }

    for (i = strlen(m + 1); i > 0; --i) {
        if (!isalnum(m[i])) return -1;
    }
    return 0;
}

int on_rpc_message(rc_mqtt_client* mqtt, const char* topic,
                   MQTTClient_message* message,
                   mqtt_rpc_event_callback callback) {
    // 1: '/'
    int offset = strlen(mqtt->topic_prefix) + strlen(MQTT_TOPIC_RPC) + 1;
    char from[FROM_LENGTH] = {0};
    char type[TYPE_LENGTH] = {0};
    char msgid[MSGID_LENGTH] = {0};
    char acktopic[100] = {0};
    rc_buf_t response = rc_buf_stack();
    int ret = 0;

    if (_get_from_type_id(topic + offset, from, type, msgid) != 0) {
        LOGW(MQ_TAG, "extract topic info failed");
        return -1;
    }

    ret = callback(mqtt, from, type, message->application_message,
                   message->application_message_size, &response);

    snprintf(acktopic, sizeof(acktopic), "/proton/%s/%s/ack/%s/%s",
             mqtt->app_id, mqtt->client_id, ret ? "succ" : "fail", msgid);

    ret = mqtt_publish(&mqtt->client, acktopic, rc_buf_head_ptr(&response),
                       response.length, INT_QOS_TO_ENUM(MQTT_RPC_QOS));

    if (mqtt->client.error != MQTT_OK) {
        LOGW(MQ_TAG, "package rpc response message failed");
        return RC_ERROR_MQTT_RPC;
    }

    mqtt_sync(&mqtt->client);

    LOGI(MQ_TAG,
         "mqtt client rpc ack(%s), ret(%d), msgId(%s), rc(%d), content(%s)",
         acktopic, ret, msgid, mqtt->client.error, rc_buf_head_ptr(&response));
    rc_buf_free(&response);

    if (mqtt->client.error != MQTT_OK) {
        LOGW(MQ_TAG, "ack rpc failed");
        return RC_ERROR_MQTT_RPC;
    }

    return 0;
}

int on_normal_message(rc_mqtt_client* mqtt, const char* topic,
                      MQTTClient_message* message,
                      mqtt_subscribe_callback callback) {
    // 1: '/'
    int offset = strlen(mqtt->topic_prefix) + strlen(MQTT_TOPIC_CMD) + 1;
    char from[FROM_LENGTH] = {0};
    char type[TYPE_LENGTH] = {0};
    char msgid[MSGID_LENGTH] = {0};

    if (_get_from_type_id(topic + offset, from, type, msgid) != 0) {
        LOGW(MQ_TAG, "extract topic info failed");
        return -1;
    }

    return callback(mqtt, from, type, message->application_message,
                    message->application_message_size);
}

int _consume_message(rc_mqtt_client* mqtt, const char* topic,
                     MQTTClient_message* message) {
    any_t p;
    mqtt_subscribe_t* sub = NULL;
    void* callback = NULL;

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)topic, &p) == MAP_OK) {
        sub = (mqtt_subscribe_t*)p;
        if (sub != NULL) {
            callback = sub->callback;
        }
    }
    rc_mutex_unlock(mqtt->mobject);

    if (callback == NULL) {
        LOGI(MQ_TAG, "not found callback of topic(%s)", topic);
        return -1;
    }

    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_CMD) != 0) {
        on_normal_message(mqtt, topic, message,
                          (mqtt_subscribe_callback)callback);
    } else if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_RPC) != 0) {
        on_rpc_message(mqtt, topic, message, (mqtt_rpc_event_callback)callback);
    }

    return 0;
}

void publish_callback(void** unused, struct mqtt_response_publish* published) {
    /*DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, context);
    LOGI(MQ_TAG, "mqtt(%p) msgarrvd, topic(%s), message-len(%d)", mqtt,
         topicName, message->payloadlen);
    _consume_message(mqtt, topicName, message);*/
}