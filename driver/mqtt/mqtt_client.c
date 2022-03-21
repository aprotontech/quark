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

#include "mqtt_client.h"
#include "logs.h"
#include "rc_json.h"
#include "rc_mqtt.h"

#define INTERVAL_COUNT (sizeof(_mqtt_reconnect_interval) / sizeof(short))

#define MQ_TIMER_INTERVAL 5

short _mqtt_reconnect_interval[] = {5, 10, 10, 20, 30, 40, 60};

static void* mqtt_main_thread(void* arg);
void publish_callback(void** unused, struct mqtt_response_publish* published) {}

mqtt_client rc_mqtt_create(const char* host, int port, const char* app_id,
                           const char* client_id, const char* username,
                           mqtt_session_token_callback callback) {
    rc_mqtt_client* mqtt = NULL;

    LOGI(MQ_TAG,
         "mqtt client remote(%s:%d), app_id(%s), client_id(%s), username(%s)",
         PRINTSTR(host), port, app_id, client_id, PRINTSTR(username));

    if (callback == NULL) {
        LOGI(MQ_TAG, "session callback is null");
        return NULL;
    }

    if (app_id == NULL || client_id == NULL) {
        LOGI(MQ_TAG, "input appId(%s), clientId(%s) is invalidate", app_id,
             client_id);
        return NULL;
    }

    if (host == NULL || port <= 0) {
        LOGI(MQ_TAG, "input remote addr is invalidate");
        return NULL;
    }

    mqtt = (rc_mqtt_client*)rc_malloc(sizeof(rc_mqtt_client));
    memset(mqtt, 0, sizeof(rc_mqtt_client));
    mqtt->buff = rc_buf_stack();
    mqtt->buff.total = MQTT_CLIENT_PAD_SIZE;

    mqtt->sync_thread = NULL;
    mqtt->mobject = rc_mutex_create(NULL);
    mqtt->mevent = rc_event_init();

    // topic prefix buffer
    mqtt->topic_prefix = rc_buf_tail_ptr(&mqtt->buff);
    mqtt->buff.length = MQTT_TOPIC_PREFIX_LENGTH;

    // append client id
    mqtt->client_id = rc_buf_tail_ptr(&mqtt->buff);
    rc_buf_append(&mqtt->buff, client_id, strlen(client_id) + 1);

    if (username == NULL || username == client_id) {
        mqtt->user_name = mqtt->client_id;
    } else {
        mqtt->user_name = rc_buf_tail_ptr(&mqtt->buff);
        rc_buf_append(&mqtt->buff, username, strlen(username) + 1);
    }

    // passwd
    mqtt->passwd = rc_buf_tail_ptr(&mqtt->buff);
    mqtt->buff.length += MQTT_PASSWD_MAX_LENGTH;

    if (RC_BUF_LEFT_SIZE(&mqtt->buff) <= 0) {
        LOGI(MQ_TAG, "buffer size is out-of range");
        rc_free(mqtt);
        return NULL;
    }

    mqtt->get_session = callback;
    mqtt->on_connect = NULL;

    mqtt->force_re_sub = 0;
    mqtt->has_sub_failed = 0;
    mqtt->sub_map = hashmap_new();

    mqtt->is_inited = 0;
    mqtt->is_connected = 0;
    mqtt->is_exit = 0;

    mqtt->recv_buf = rc_buf_init(960);
    mqtt->send_buf = rc_buf_init(960);

    mqtt->remote_host.sin_family = AF_INET;
    mqtt->remote_host.sin_port = htons(port);
    mqtt->remote_host.sin_addr.s_addr = inet_addr(host);

    LOGI(MQ_TAG, "mqtt(%p), client_id(%s) created", mqtt, mqtt->client_id);

    return mqtt;
}

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

int do_rpc_callback(rc_mqtt_client* mqtt, mqtt_rpc_event_callback callback,
                    char* msgId, char* acktopic, char* body) {
    any_t p;
    int ret;
    char str_rc[10] = {0};
    char* output = NULL;
    rc_buf_t response = rc_buf_stack();

    ret = callback(mqtt, body, strlen(body), &response);

    mqtt_publish(&mqtt->client, acktopic, rc_buf_head_ptr(&response),
                 response.length, INT_QOS_TO_ENUM(MQTT_RPC_QOS));
    if (mqtt->client.error != MQTT_OK) {
        LOGW(MQ_TAG, "package rpc response message failed");
        return RC_ERROR_MQTT_RPC;
    }

    mqtt_sync(&mqtt->client);

    LOGI(MQ_TAG,
         "mqtt client rpc ack(%s), ret(%d), msgId(%s), rc(%d), content(%s)",
         acktopic, ret, msgId, mqtt->client.error, rc_buf_head_ptr(&response));
    rc_buf_free(&response);

    if (mqtt->client.error != MQTT_OK) {
        LOGW(MQ_TAG, "ack rpc failed");
        return RC_ERROR_MQTT_RPC;
    }

    return RC_SUCCESS;
}

int on_rpc_message(rc_mqtt_client* mqtt, const char* topic,
                   MQTTClient_message* message) {
    int ret = 0;
    char* msgId = NULL;
    char* acktopic = NULL;
    char *body = NULL, *payload = NULL;
    cJSON* root = NULL;
    any_t p;
    mqtt_subscribe_t* sub = NULL;
    mqtt_rpc_event_callback callback = NULL;

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)topic, &p) == MAP_OK) {
        sub = (mqtt_subscribe_t*)p;
        if (sub != NULL) {
            callback = (mqtt_rpc_event_callback)sub->callback;
        }
    }
    rc_mutex_unlock(mqtt->mobject);

    if (callback == NULL) {
        LOGI(MQ_TAG, "not found rpc event callback of topic(%s)", topic);
        return -1;
    }

    payload = (char*)rc_malloc(message->application_message_size + 1);
    if (payload == NULL) {
        LOGW("[MQTT]", "message playload malloc failed");
        return RC_ERROR_NET_FORMAT;
    }
    memcpy(payload, message->application_message,
           message->application_message_size);
    payload[message->application_message_size] = '\0';
    LOGI("[MQTT]", "topic(%s), message(%s)", topic, payload);

    ret = do_rpc_callback(mqtt, callback, msgId, acktopic, body);
    rc_free(payload);

    return ret;
}

int on_normal_message(rc_mqtt_client* mqtt, const char* topic,
                      MQTTClient_message* message) {
    int ret;
    any_t p;
    mqtt_subscribe_t* sub = NULL;
    mqtt_subscribe_callback callback = NULL;

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)topic, &p) == MAP_OK) {
        sub = (mqtt_subscribe_t*)p;
        if (sub != NULL) {
            callback = (mqtt_subscribe_callback)sub->callback;
        }
    }
    rc_mutex_unlock(mqtt->mobject);

    if (callback == NULL) {
        LOGI(MQ_TAG, "not found comand callback of topic(%s)", topic);
        return -1;
    }

    return 0;
    // return callback(mqtt, message->payload, message->payloadlen);
}

int re_subscribe(any_t client, const char* topic, any_t val) {
    mqtt_subscribe_t* sub = (mqtt_subscribe_t*)val;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (sub->error != 0 || mqtt->force_re_sub != 0) {
        mqtt_subscribe(&mqtt->client, topic, INT_QOS_TO_ENUM(MQTT_RPC_QOS));
        LOGI(MQ_TAG, "mqtt(%p) re-subscribe(%s), ret(%d)", mqtt, topic,
             mqtt->client.error);
        if (mqtt->client.error != MQTT_OK) {
            mqtt->has_sub_failed = 1;
            LOGI(MQ_TAG, "subscribe(%s) failed, ret(%d)", topic,
                 mqtt->client.error);
        } else {
            sub->error = 0;
        }
    }

    return MAP_OK;
}

int mqtt_tcp_connect(rc_mqtt_client* mqtt) {
    int ret;
    char ip[20] = {0};
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        LOGW(MQ_TAG, "create socket failed with(%d)", sfd);
        return sfd;
    }

    struct timeval timeout = {
        .tv_sec = 3,
        .tv_usec = 0,
    };

    // setsockopt(sfd, SOL_SOCKET, SO_CONTIMEO, &timeout, sizeof(struct
    // timeval));
    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
    setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));

#ifdef __QUARK_RTTHREAD__
    strcpy(ip, inet_ntoa(client->remote_ip.sin_addr));
#else
    inet_ntop(AF_INET, &mqtt->remote_host.sin_addr, ip, sizeof(ip));
#endif
    LOGI(MQ_TAG, "[sock=%d] connect host(%s), port(%d)...", sfd, ip,
         (int)ntohs(mqtt->remote_host.sin_port));

    ret = connect(sfd, (struct sockaddr*)(&mqtt->remote_host),
                  sizeof(struct sockaddr));
    if (ret != 0) {
        LOGI(MQ_TAG, "[sock=%d] connect failed with(%d)", sfd, ret);
        close(sfd);
        return -1;
    }

    return sfd;
}

int mqtt_connect_to_remote(rc_mqtt_client* mqtt) {
    int ret, socket;
    char* passwd = (char*)mqtt->get_session(mqtt->client_id);

    LOGI(MQ_TAG, "mqtt(%p) connect clientId(%s), token(%s)", mqtt,
         mqtt->client_id, passwd);
    if (passwd == NULL || strlen(passwd) >= MQTT_PASSWD_MAX_LENGTH) {
        LOGI(MQ_TAG, "input mqtt password is invalidate");
        return RC_ERROR_MQTT_CONNECT;
    }

    strcpy(mqtt->passwd, passwd);

    socket = mqtt_tcp_connect(mqtt);
    if (socket < 0) {
        LOGI(MQ_TAG, "mqtt(%p) socket create failed", mqtt);
        return RC_ERROR_CREATE_SOCKET;
    }

    if (mqtt->is_inited) {
        mqtt_reinit(&mqtt->client, socket, rc_buf_head_ptr(mqtt->send_buf),
                    mqtt->send_buf->total, rc_buf_head_ptr(mqtt->recv_buf),
                    mqtt->recv_buf->total);
    } else {
        ret = mqtt_init(&mqtt->client, socket, rc_buf_head_ptr(mqtt->send_buf),
                        mqtt->send_buf->total, rc_buf_head_ptr(mqtt->recv_buf),
                        mqtt->recv_buf->total, publish_callback);

        if (ret != MQTT_OK) {
            LOGI(MQ_TAG, "init mqtt client failed with %d", ret);
            close(socket);
            return RC_ERROR_MQTT_CONNECT;
        }

        mqtt->is_inited = 1;
    }

    ret = mqtt_connect(&mqtt->client, mqtt->client_id, NULL, NULL, 0,
                       mqtt->user_name, mqtt->passwd,
                       MQTT_CONNECT_CLEAN_SESSION, 60);

    if (mqtt_sync(&mqtt->client) == MQTT_OK) {  // connect success
        LOGI(MQ_TAG, "mqtt(%p) connect success", mqtt);
    } else {
        LOGW(MQ_TAG, "package mqtt connect message failed");
        close(socket);
        return RC_ERROR_MQTT_CONNECT;
    }

    return RC_SUCCESS;
}

int rc_mqtt_start(mqtt_client client, mqtt_connect_callback callback) {
    int rc;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    if (mqtt->sync_thread != NULL) {
        LOGW(MQ_TAG, "mqtt client had starte");
        return -1;
    }

    mqtt->on_connect = callback;

    rc_thread_context_t tc = {
        .joinable = 1, .name = "mqtt", .priority = 0, .stack_size = 2048};
    mqtt->sync_thread = rc_thread_create(mqtt_main_thread, mqtt, &tc);

    return RC_SUCCESS;
}

int rc_mqtt_subscribe(mqtt_client client, const char* topic,
                      mqtt_subscribe_callback callback) {
    int ret;
    any_t val;
    mqtt_subscribe_t* sub = NULL;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_CMD) == 0) {
        LOGI(MQ_TAG, "mqtt(%p), invalidate mqtt cmd topic(%s)", mqtt, topic);
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

    LOGI(MQ_TAG, "mqtt(%p) cmd map (%s) -> %p", mqtt, topic, callback);
    return RC_SUCCESS;
}

int rc_mqtt_publish(mqtt_client client, const char* topic, const char* body,
                    int len) {
    int ret;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_EVT) == 0 &&
        is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_CMD) ==
            0) {  // for test to send cmd
        LOGI(MQ_TAG, "invalidate mqtt event topic(%s)", topic);
        return RC_ERROR_MQTT_PUBLISH;
    }

    ret = mqtt_publish(&mqtt->client, (char*)topic, (char*)body, len,
                       MQTT_CMD_QOS);

    LOGI(MQ_TAG, "mqtt(%p) publish(%s), content-length(%d), ret(%d), token(%d)",
         mqtt, topic, len, ret, 0)
    return ret == 0 ? RC_SUCCESS : RC_ERROR_MQTT_PUBLISH;
}

int rc_mqtt_rpc_send(mqtt_client client, const char* topic, const char* body,
                     int len, int timeout, rc_buf_t* response) {
    return 0;
}

int rc_mqtt_rpc_event(mqtt_client client, const char* topic,
                      mqtt_rpc_event_callback callback) {
    int ret, i;
    any_t val = NULL;
    mqtt_subscribe_t* sub = NULL;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    LOGI(MQ_TAG, "mqtt(%p) topic(%s), callback(%p)", mqtt, topic, callback);
    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_RPC) == 0) {
        LOGI(MQ_TAG, "invalidate mqtt rpc topic(%s)", topic);
        return RC_ERROR_MQTT_SUBSCRIBE;
    }

    rc_mutex_lock(mqtt->mobject);
    if (hashmap_get(mqtt->sub_map, (char*)topic, &val) == MAP_MISSING) {
        sub = (mqtt_subscribe_t*)rc_malloc(sizeof(mqtt_subscribe_t) +
                                           strlen(topic));
        memset(sub, 0, sizeof(mqtt_subscribe_t));
        sub->type = 1;
        sub->callback = callback;
        strcpy(sub->topic, topic);
        ret = hashmap_put(mqtt->sub_map, sub->topic, sub);
        rc_mutex_unlock(mqtt->mobject);

        if (ret != MAP_OK) {
            LOGW(MQ_TAG, "mqtt(%p) bind topic(%s) failed with(%d)", mqtt, topic,
                 ret);
            rc_free(sub);
            return RC_ERROR_MQTT_SUBSCRIBE;
        }

        LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s)", mqtt, topic);

        mqtt_subscribe(&mqtt->client, topic, INT_QOS_TO_ENUM(MQTT_RPC_QOS));

        if (mqtt_sync(&mqtt->client) != MQTT_OK) {
            LOGI(MQ_TAG, "subscribe(%s) failed, ret(%d)", topic, ret);
            sub->error = 1;
            mqtt->has_sub_failed = 1;
        }
    } else {
        ret = MAP_OK;
        ((mqtt_subscribe_t*)val)->callback = callback;  // update callback
        rc_mutex_unlock(mqtt->mobject);
    }

    LOGI(MQ_TAG, "mqtt(%p) rpc map (%s) -> %p", mqtt, topic, callback);
    return 0;
}

static void mqtt_client_disconnect(rc_mqtt_client* mqtt) {
    if (mqtt->client.socketfd <= 0) {
        return;
    }

    if (mqtt->client.error == MQTT_OK) {
        mqtt_disconnect(&mqtt->client);
        if (mqtt->client.error != MQTT_OK) {
            LOGI(MQ_TAG, "package mqtt disconnect message failed");
            return;
        }

        __mqtt_send(&mqtt->client);

        if (mqtt->client.error != MQTT_OK) {
            LOGW(MQ_TAG, "send mqtt disconnect message failed");
        }
    }

    close(mqtt->client.socketfd);
    mqtt->client.socketfd = -1;
}

static void* mqtt_main_thread(void* arg) {
    rc_mqtt_client* mqtt = (rc_mqtt_client*)arg;

    mqtt->cur_reconnect_interval_index = 0;
    mqtt->last_reconnect_time = 0;

    while (!mqtt->is_exit) {
        while (!mqtt->is_exit) {
            rc_event_wait(mqtt->mevent, 500);

            int now = time(NULL);
            int interval =
                _mqtt_reconnect_interval[mqtt->cur_reconnect_interval_index];
            if (mqtt->last_reconnect_time + interval <= now) {
                // need reconnect to the remote mqtt server
                break;
            }
        }

        if (mqtt->is_exit) break;

        // CONNECT TO REMOTE
        int ret = mqtt_connect_to_remote(mqtt);

        // CONNECT-CALLBACK
        if (mqtt->on_connect != NULL) {
            char error[40] = {0};
            snprintf(error, sizeof(error), "mqtt connect result: %d", ret);
            mqtt->on_connect(
                mqtt,
                ret != MQTT_OK ? MQTT_STATUS_DISCONNECT : MQTT_STATUS_CONNECTED,
                error);
        }

        if (ret != 0) {
            if (mqtt->cur_reconnect_interval_index != INTERVAL_COUNT - 1) {
                mqtt->cur_reconnect_interval_index++;
                LOGI(MQ_TAG, "reconn interval adjust to %d sec",
                     _mqtt_reconnect_interval
                         [mqtt->cur_reconnect_interval_index]);
            }
            continue;
        }

        // UPDATE INFO
        rc_mutex_lock(mqtt->mobject);
        mqtt->is_connected = 1;
        mqtt->last_reconnect_time = (int)time(NULL);
        mqtt->cur_reconnect_interval_index = 0;
        mqtt->has_sub_failed = 1;
        rc_mutex_unlock(mqtt->mobject);

        while (!mqtt->is_exit && mqtt_sync(&mqtt->client) == MQTT_OK) {
            rc_mutex_lock(mqtt->mobject);
            if (mqtt->has_sub_failed) {
                mqtt->has_sub_failed = 0;
                mqtt->force_re_sub = 1;
                hashmap_iterate(mqtt->sub_map, re_subscribe, mqtt);
                mqtt->force_re_sub = 0;
            }
            rc_mutex_unlock(mqtt->mobject);
        }

        mqtt_client_disconnect(mqtt);
    }

    mqtt_client_disconnect(mqtt);
}

int sub_item_free(any_t ipm, const char* key, any_t value) {
    rc_free(value);
    return RC_SUCCESS;
}

int rc_mqtt_close(mqtt_client client) {
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    mqtt->is_exit = 1;
    rc_event_signal(mqtt->mevent);

    if (mqtt->client.socketfd > 0) {
        close(mqtt->client.socketfd);
    }

    if (mqtt->sync_thread != NULL) {
        rc_thread_join(mqtt->sync_thread);
    }

    if (mqtt->client.mutex != NULL) {
        rc_mutex_destroy(mqtt->client.mutex);
        mqtt->client.mutex = NULL;
    }

    if (mqtt->mevent != NULL) {
        rc_event_uninit(mqtt->mevent);
    }

    hashmap_iterate(mqtt->sub_map, sub_item_free, NULL);
    hashmap_free(mqtt->sub_map);
    rc_mutex_destroy(mqtt->mobject);
    rc_buf_free(mqtt->send_buf);
    rc_buf_free(mqtt->recv_buf);

    rc_free(mqtt);
    LOGI(MQ_TAG, "mqtt(%p) free", mqtt);
    return 0;
}
