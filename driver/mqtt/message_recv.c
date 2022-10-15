/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     message_recv.c
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

typedef struct _mqtt_topic_info_t {
    short type;  // 0-unknown, 1-cmd, 2-rpc
    char* key;
    char* from;
    char* msgid;
} mqtt_topic_info_t;

int is_topic_match(rc_mqtt_client* mqtt, const char* topic,
                   const char* func_prefix) {
    return strncmp(topic + mqtt->topic_prefix_length, func_prefix,
                   strlen(func_prefix)) == 0 &&
           topic[mqtt->topic_prefix_length + strlen(func_prefix)] ==
               MQTT_TOPIC_SEP;
}

int _mqtt_regist_callback(rc_mqtt_client* mqtt, const char* key, short type,
                          void* callback, void* args) {
    int ret;
    any_t val;
    mqtt_subscribe_t* sub = NULL;
    if (strchr(key, MQTT_TOPIC_SEP) != NULL) {
        LOGI(MQ_TAG, "mqtt(%p), invalidate key(%s)", mqtt, key);
        return RC_ERROR_MQTT_SUBSCRIBE;
    }

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)key, &val) == MAP_MISSING) {
        sub = (mqtt_subscribe_t*)rc_malloc(sizeof(mqtt_subscribe_t) +
                                           strlen(key) + 1);
        memset(sub, 0, sizeof(mqtt_subscribe_t));
        sub->type = type;
        sub->args = args;
        sub->callback = callback;
        strcpy(sub->key, key);
        ret = hashmap_put(mqtt->sub_map, sub->key, sub);
        rc_mutex_unlock(mqtt->mobject);

        if (ret != MAP_OK) {
            LOGW(MQ_TAG, "mqtt(%p) bind key(%s) failed with(%d)", mqtt, key,
                 ret);
            rc_free(sub);
            return RC_ERROR_MQTT_SUBSCRIBE;
        }
    } else {
        ret = MAP_OK;
        sub = (mqtt_subscribe_t*)val;
        sub->callback = callback;  // update callback
        sub->type = type;
        rc_mutex_unlock(mqtt->mobject);
    }

    LOGI(MQ_TAG, "mqtt(%p) %d map (%s) -> %p", mqtt, type, key, callback);
    return RC_SUCCESS;
}

int mqtt_client_cmd_regist(mqtt_client client, const char* key,
                           mqtt_cmd_event_callback callback, void* args) {
    return _mqtt_regist_callback(client, key, MQTT_TOPIC_TYPE_CMD, callback,
                                 args);
}

int mqtt_client_rpc_regist(mqtt_client client, const char* key,
                           mqtt_rpc_event_callback callback, void* args) {
    return _mqtt_regist_callback(client, key, MQTT_TOPIC_TYPE_RPC, callback,
                                 args);
}

int on_rpc_message(rc_mqtt_client* mqtt, const mqtt_topic_info_t* info,
                   struct mqtt_response_publish* message,
                   mqtt_rpc_event_callback callback, void* args) {
    rc_buf_t response = rc_buf_stack();

    int ret = callback(mqtt, info->from, message->application_message,
                       message->application_message_size, args, &response);

    if (info->msgid != NULL) {
        char acktopic[100] = {0};
        ret = snprintf(acktopic, sizeof(acktopic), "%sack/%s/%s",
                       mqtt->topic_prefix, info->msgid,
                       ret == 0 ? "succ" : "fail");
        if (ret <= 0 || ret >= sizeof(acktopic)) {  // buffer is too small
            LOGW(MQ_TAG, "msgid(%s), acktopic buffer is too small",
                 info->msgid);
        } else {
            mqtt_publish_data_t* data =
                (mqtt_publish_data_t*)rc_malloc(sizeof(mqtt_publish_data_t));
            data->buff = response;
            strcpy(data->topic, acktopic);

            LOGI(MQ_TAG, "mqtt rpc(%s), content(%s)", acktopic,
                 rc_buf_head_ptr(&response));

            rc_mutex_lock(mqtt->mobject);
            LL_insert(&data->link, mqtt->msg_to_publish.prev);
            rc_mutex_unlock(mqtt->mobject);
            return 0;
        }
    } else {
        LOGI(MQ_TAG, "message has no msgid, skip response");
    }

    rc_buf_free(&response);

    return 0;
}

// topic: /aproton/{appId}/{clientId}/(rpc|cmd)/{type}/{msgId}/{from}
int _get_from_type_id(rc_mqtt_client* mqtt, char* topic,
                      mqtt_topic_info_t* info) {
    char *t = NULL, *m = NULL;
    char* p = topic + mqtt->topic_prefix_length;

    memset(info, 0, sizeof(mqtt_topic_info_t));
    if (strncmp(topic, mqtt->topic_prefix, mqtt->topic_prefix_length) == 0) {
        if (strncmp(p, MQTT_TOPIC_CMD, sizeof(MQTT_TOPIC_CMD) - 1) == 0) {
            p += sizeof(MQTT_TOPIC_CMD) - 1;
            info->type = p[0] == MQTT_TOPIC_SEP ? MQTT_TOPIC_TYPE_CMD : 0;
        } else if (strncmp(p, MQTT_TOPIC_RPC, sizeof(MQTT_TOPIC_RPC) - 1) ==
                   0) {
            p += sizeof(MQTT_TOPIC_RPC) - 1;
            info->type = p[0] == MQTT_TOPIC_SEP ? MQTT_TOPIC_TYPE_RPC : 0;
        } else {
            LOGI(MQ_TAG, "input topic(%s) is not cmd/rpc", topic);
        }
    } else {
        LOGI(MQ_TAG, "input topic(%s) is not match prefix", topic);
    }

    if (info->type == 0) {
        return -1;
    }

    info->key = p + 1;

    // key = [info->key, t)
    if ((t = strchr(info->key, '/')) != NULL) {
        *t = '\0';

        // msgId = [t + 1, m)
        if ((m = strchr(t + 1, '/')) != NULL) {
            *m = '\0';  // info->from: [m+1, eof)
        }

        // exists msgid, so check msgid is validate
        p = t + 1;
        while (*p != '\0' && isalnum((int)(*p))) {
            ++p;
        }

        if (*p == '\0') {
            info->msgid = t + 1;
        } else {  // invalidate msgid
            LOGI(MQ_TAG, "input topic(%s) is not match msgid", topic);
            return -1;
        }

        if (m != NULL) {
            info->from = m + 1;
        }
    }

    LOGI(MQ_TAG, "topic(%s) split done, key(%s), msgId(%s), from(%s)", topic,
         info->key, PRINTSTR(info->msgid), PRINTSTR(info->from));

    return 0;
}

int _mqtt_handle_message(void* context, char* topic,
                         struct mqtt_response_publish* message) {
    any_t p;
    void* callback = NULL;
    void* args = NULL;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, context);

    mqtt_topic_info_t info;
    if (_get_from_type_id(mqtt, topic, &info) != 0) {
        LOGW(MQ_TAG, "mqtt(%p), topic(%s) is invalidate, so skip", mqtt, topic);
        return -1;
    }

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, info.key, &p) == MAP_OK) {
        mqtt_subscribe_t* sub = (mqtt_subscribe_t*)p;
        if (sub != NULL) {
            callback = sub->callback;
            args = sub->args;
        }
    }
    rc_mutex_unlock(mqtt->mobject);

    if (callback == NULL) {
        LOGI(MQ_TAG, "not found callback of topic(%s)", topic);
        return -1;
    }
    switch (info.type) {
    case MQTT_TOPIC_TYPE_CMD:
        ((mqtt_cmd_event_callback)callback)(
            mqtt, info.from, message->application_message,
            message->application_message_size, args);
        break;
    case MQTT_TOPIC_TYPE_RPC:
        on_rpc_message(mqtt, &info, message, (mqtt_rpc_event_callback)callback,
                       args);
        break;
    }

    return 0;
}

void publish_callback(void** context, struct mqtt_response_publish* message) {
    char* topic = (char*)rc_malloc(message->topic_name_size + 1);

    memcpy(topic, message->topic_name, message->topic_name_size);
    topic[message->topic_name_size] = '\0';

    LOGI(MQ_TAG, "mqtt(%p) msgarrvd, topic(%s), message-len(%d)", *context,
         topic, (int)message->application_message_size);

    _mqtt_handle_message(*context, topic, message);

    rc_free(topic);
}