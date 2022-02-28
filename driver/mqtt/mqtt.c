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

#include "logs.h"
#include "mqtt_adaptor.h"
#include "rc_json.h"
#include "rc_mqtt.h"

#define INTERVAL_COUNT (sizeof(_mqtt_reconnect_interval) / sizeof(short))

#define MQ_TIMER_INTERVAL 5

short _mqtt_reconnect_interval[] = {5, 10, 10, 20, 30, 40, 60};

int msgarrvd(void* context, char* topicName, int topicLen,
             MQTTClient_message* message);
void delivered(void* context, MQTTClient_deliveryToken dt);
void connlost(void* context, char* cause);
extern int _consume_message(rc_mqtt_client* mqtt, const char* topic,
                            MQTTClient_message* message);

mqtt_client rc_mqtt_create(const char* host, int port, const char* app_id,
                           const char* client_id, const char* username,
                           mqtt_session_token_callback callback) {
    char addr[100] = {0};
    rc_mqtt_client* mqtt = NULL;

    int ret = snprintf(addr, sizeof(addr), "tcp://%s:%d", host, port);
    if (ret <= 0 || ret >= sizeof(addr)) {
        LOGI(MQ_TAG, "input address(%s:%d) is too long", host, port);
        return NULL;
    }

    LOGI(MQ_TAG,
         "mqtt client remote(%s), app_id(%s), client_id(%s), username(%s)",
         addr, app_id, client_id, username != NULL ? username : "null");

    if (callback == NULL) {
        LOGI(MQ_TAG, "session callback is null");
        return NULL;
    }

    if (app_id == NULL || client_id == NULL) {
        LOGI(MQ_TAG, "input appId(%s), clientId(%s) is invalidate", app_id,
             client_id);
        return NULL;
    }

    mqtt = (rc_mqtt_client*)rc_malloc(sizeof(rc_mqtt_client));
    memset(mqtt, 0, sizeof(rc_mqtt_client));
    mqtt->buff = rc_buf_stack();
    mqtt->buff.total = MQTT_CLIENT_PAD_SIZE;

    mqtt->mobject = rc_mutex_create(NULL);

    // topic prefix buffer
    mqtt->topic_prefix = rc_buf_tail_ptr(&mqtt->buff);
    mqtt->buff.length = MQTT_TOPIC_PREFIX_LENGTH;

    // append client id
    mqtt->app_id = rc_buf_tail_ptr(&mqtt->buff);
    rc_buf_append(&mqtt->buff, app_id, strlen(app_id) + 1);
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
    mqtt->reconn_timer = NULL;
    mqtt->sub_map = hashmap_new();
#if defined(__QUARK_FREERTOS__) || defined(__QUARK_RTTHREAD__)
    mqtt->client = &mqtt->wrap;
#endif
    ret = MQTTClient_create(&mqtt->client, addr, mqtt->client_id,
                            MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != 0) {
        LOGI(MQ_TAG, "create mqtt client failed with %d", ret);
        rc_free(mqtt);
        return NULL;
    }
    MQTTClient_setCallbacks(mqtt->client, mqtt, connlost, msgarrvd, delivered);

    LOGI(MQ_TAG, "mqtt(%p), client_id(%s) created", mqtt, mqtt->client_id);

    return mqtt;
}

int msgarrvd(void* context, char* topicName, int topicLen,
             MQTTClient_message* message) {
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, context);
    LOGI(MQ_TAG, "mqtt(%p) msgarrvd, topic(%s), message-len(%d)", mqtt,
         topicName, message->payloadlen);
    _consume_message(mqtt, topicName, message);
    MQTTClient_free(topicName);
    MQTTClient_freeMessage(&message);
    return 1;
}

void connlost(void* context, char* cause) {
    rc_mqtt_client* mqtt = (rc_mqtt_client*)context;
    LOGI(MQ_TAG, "mqtt(%p) connect lost, cause(%s)", mqtt, cause);
    if (mqtt->on_connect != NULL) {
        mqtt->on_connect(mqtt, MQTT_STATUS_DISCONNECT, cause);
    }
}

void delivered(void* context, MQTTClient_deliveryToken dt) {
    rc_mqtt_client* mqtt = (rc_mqtt_client*)context;
    LOGI(MQ_TAG, "mqtt(%p) delivered, token(%d)", mqtt, dt);
}

int re_subscribe(any_t client, const char* topic, any_t val) {
    int ret;
    mqtt_subscribe_t* sub = (mqtt_subscribe_t*)val;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (sub->error != 0 || mqtt->force_re_sub != 0) {
        ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_RPC_QOS);
        LOGI(MQ_TAG, "mqtt(%p) re-subscribe(%s), ret(%d)", mqtt, topic, ret);
        if (ret != MQTTCLIENT_SUCCESS) {
            mqtt->has_sub_failed = 1;
            LOGI(MQ_TAG, "subscribe(%s) failed, ret(%d)", topic, ret);
        } else
            sub->error = 0;
    }

    return MAP_OK;
}

static int _client_subscribe(rc_mqtt_client* mqtt) {
    char topic[100] = {0};
    snprintf(topic, 100, "/proton/%s/%s/cmd/#", mqtt->app_id, mqtt->client_id);
    int ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_CMD_QOS);
    LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d)", mqtt, topic, ret);
    if (ret == MQTTCLIENT_SUCCESS) {
        snprintf(topic, 100, "/proton/%s/%s/rpc/#", mqtt->app_id,
                 mqtt->client_id);
        ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_RPC_QOS);
        LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d)", mqtt, topic, ret);
    }

    return ret;
}

int mqtt_connect_to_remote(rc_mqtt_client* mqtt) {
    int ret;
    char* passwd = (char*)mqtt->get_session(mqtt->client_id);

    LOGI(MQ_TAG, "mqtt(%p) connect clientId(%s), token(%s)", mqtt,
         mqtt->client_id, passwd);
    if (passwd == NULL || strlen(passwd) >= MQTT_PASSWD_MAX_LENGTH) {
        LOGI(MQ_TAG, "input mqtt password is invalidate");
        return -1;
    }

    strcpy(mqtt->passwd, passwd);

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
#if defined(__QUARK_RTTHREAD__) || defined(__QUARK_FREERTOS__)
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.willFlag = 0;
    conn_opts.MQTTVersion = 3;
    conn_opts.clientID.cstring = mqtt->client_id;
    conn_opts.username.cstring = mqtt->user_name;
    conn_opts.password.cstring = mqtt->passwd;
    ret = MQTTClient_connect(mqtt->client, &conn_opts);
#elif defined(__QUARK_LINUX__)
    conn_opts = conn_opts;
    conn_opts.keepAliveInterval = 60;
    conn_opts.reliable = 0;
    conn_opts.connectTimeout = 3;
    conn_opts.retryInterval = 10;
    conn_opts.cleansession = 1;
    // conn_opts.clientID = mqtt->client_id;
    conn_opts.username = mqtt->user_name;
    conn_opts.password = mqtt->passwd;
    ret = MQTTClient_connect(mqtt->client, &conn_opts);
#endif

    LOGI(MQ_TAG, "mqtt(%p) connect, ret(%d)-%s", mqtt, ret,
         ret == 0 ? "success" : "failed");

    if (ret == MQTTCLIENT_SUCCESS) {  // connect success
        ret = _client_subscribe(mqtt);
        if (ret != MQTTCLIENT_SUCCESS) {
            LOGW(MQ_TAG, "subscribe cmd/rpc topic failed, so close the socket");
            MQTTClient_disconnect(mqtt->client, 10000);
            return ret;
        }
    }

    mqtt->last_reconnect_time = (int)time(NULL);
    mqtt->connectResult = ret;

#if defined(__QUARK_RTTHREAD__) || defined(__QUARK_FREERTOS__)
    if (mqtt->on_connect != NULL) {
        char error[40] = {0};
        snprintf(error, sizeof(error), "mqtt connect result: %d", ret);
        mqtt->on_connect(
            mqtt, ret ? MQTT_STATUS_DISCONNECT : MQTT_STATUS_CONNECTED, error);
    }
#endif

    return ret;
}

int mqtt_on_timer(rc_timer timer, void* client) {
    int rc = 0;
    int now = time(NULL);
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (mqtt->reconn_timer == NULL) {
        LOGI(MQ_TAG, "reconnect timer is null, had closed");
        return -1;
    }

    if (MQTTClient_isConnected(mqtt->client)) {
        if (mqtt->has_sub_failed != 0) {  // has subscribe failed topic
            mqtt->has_sub_failed = 0;
            hashmap_iterate(mqtt->sub_map, re_subscribe, mqtt);
        }
    }
#ifndef __QUARK_RTTHREAD__
    else if (mqtt->last_reconnect_time +
                 _mqtt_reconnect_interval[mqtt->cur_reconnect_interval_index] <=
             now) {
        // need reconnect to the remote mqtt server
        rc = mqtt_connect_to_remote(mqtt);

        if (rc != 0) {
            if (mqtt->cur_reconnect_interval_index != INTERVAL_COUNT - 1) {
                mqtt->cur_reconnect_interval_index++;
                LOGI(MQ_TAG, "reconn interval adjust to %d sec",
                     _mqtt_reconnect_interval
                         [mqtt->cur_reconnect_interval_index]);
            }
        } else
            mqtt->cur_reconnect_interval_index = 0;
    }
#endif

    return RC_ERROR_MQTT_SKIP_CONN;
}

int rc_mqtt_connect(mqtt_client client) {
    int ret;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    if (mqtt->reconn_timer != NULL) {
        LOGI(MQ_TAG, "mqtt(%p) had enabled auto connect, so wait for it", mqtt);
        return RC_ERROR_MQTT_CONNECT;
    }

    return mqtt_connect_to_remote(mqtt);
}

int rc_mqtt_enable_auto_connect(mqtt_client client, rc_timer_manager mgr,
                                mqtt_connect_callback callback, int at_once) {
    int rc;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (mqtt->reconn_timer != NULL) {
        LOGI(MQ_TAG, "mqtt(%p) had enabled auto connect", mqtt);
        return RC_ERROR_MQTT_CONNECT;
    }

    mqtt->on_connect = callback;
#ifdef __QUARK_RTTHREAD__
#elif defined(__QUARK_LINUX__) || defined(__QUARK_FREERTOS__)
    mqtt->cur_reconnect_interval_index = 0;
    mqtt->last_reconnect_time = 0;
    mqtt->reconn_timer =
        rc_timer_create(mgr, MQ_TIMER_INTERVAL * 1000, MQ_TIMER_INTERVAL * 1000,
                        mqtt_on_timer, mqtt);
#endif

    if (at_once && mqtt->reconn_timer != NULL) {
        rc_timer_ahead_once(mqtt->reconn_timer, 100);
    }
    return RC_SUCCESS;
}

int sub_item_free(any_t ipm, const char* key, any_t value) {
    rc_free(value);
    return RC_SUCCESS;
}

int rc_mqtt_close(mqtt_client client) {
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    if (mqtt->reconn_timer != NULL) {
        rc_timer_stop(mqtt->reconn_timer);
        mqtt->reconn_timer = NULL;
    }

    MQTTClient_disconnect(mqtt->client, 10000);
    MQTTClient_destroy(&mqtt->client);
    hashmap_iterate(mqtt->sub_map, sub_item_free, NULL);
    hashmap_free(mqtt->sub_map);
    rc_mutex_destroy(mqtt->mobject);
    rc_free(mqtt->client_id);
    rc_free(mqtt);
    LOGI(MQ_TAG, "mqtt(%p) free", mqtt);
    return 0;
}
