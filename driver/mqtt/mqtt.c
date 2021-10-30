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
#include "rc_json.h"
#include "mqtt_adaptor.h"

#define INTERVAL_COUNT (sizeof(_mqtt_reconnect_interval) / sizeof(short))

#ifdef __QUARK_RTTHREAD__
#define MQ_TIMER_INTERVAL 60
#else
#define MQ_TIMER_INTERVAL 5
#endif

short _mqtt_reconnect_interval[] = {
    5, 10, 10, 20, 30, 40, 60
};

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void delivered(void *context, MQTTClient_deliveryToken dt);
void connlost(void *context, char *cause);

mqtt_client rc_mqtt_create(const char* host, int port, 
        const char* app_id, const char* client_id, mqtt_session_token_callback callback)
{
    char addr[40] = {0};
    rc_mqtt_client* mqtt = NULL;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    int ret = snprintf(addr, sizeof(addr), "tcp://%s:%d", host, port);

    LOGI(MQ_TAG, "mqtt client host(%s), port(%d), app_ id(%s), client_id(%s)", 
            host, port, app_id, client_id);
    if (ret <= 0 || ret >= sizeof(addr)) {
        LOGI(MQ_TAG, "input address(%s:%d) is too long", host, port);
        return NULL;
    }
    if (callback == NULL) {
        LOGI(MQ_TAG, "session callback is null");
        return NULL;
    }

    mqtt = (rc_mqtt_client*)rc_malloc(sizeof(rc_mqtt_client));
    memset(mqtt, 0, sizeof(rc_mqtt_client));
    mqtt->mobject = rc_mutex_create(NULL);

    mqtt->client_id = rc_copy_string(client_id);
    mqtt->get_session = callback;
    mqtt->on_connect = NULL;
    snprintf(mqtt->topic_prefix, sizeof(mqtt->topic_prefix) - 1,
            "/%s/%s", app_id, client_id);

    mqtt->force_re_sub = 0;
    mqtt->has_sub_failed = 0;
    mqtt->reconn_timer = NULL;
    mqtt->sub_map = hashmap_new();

#ifdef __QUARK_RTTHREAD__
    mqtt->conn_opts.clientID.cstring = mqtt->client_id;
    mqtt->conn_opts.keepAliveInterval = 60;
    mqtt->conn_opts.cleansession = 1;
    mqtt->conn_opts.username.cstring = mqtt->client_id;
    mqtt->conn_opts.password.cstring = "";
    mqtt->conn_opts.willFlag = 0;
    mqtt->conn_opts.MQTTVersion = 4;
#elif defined(__QUARK_FREERTOS__)
#elif defined(__QUARK_LINUX__)
    mqtt->conn_opts = conn_opts;
    mqtt->conn_opts.keepAliveInterval = 60;
    mqtt->conn_opts.reliable = 0;
    mqtt->conn_opts.connectTimeout = 3;
    mqtt->conn_opts.retryInterval = 10;
    mqtt->conn_opts.cleansession = 1;
    mqtt->conn_opts.username = mqtt->client_id;
    mqtt->conn_opts.password = "";

#endif

    MQTTClient_create(&mqtt->client, addr, mqtt->client_id,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(mqtt->client, mqtt, connlost, msgarrvd, delivered);
    
    LOGI(MQ_TAG, "mqtt(%p), topic-prefix(%s) created", mqtt, mqtt->topic_prefix);

    return mqtt;
}

int is_topic_match(const char* topic, const char* client_prefix, const char* func_prefix)
{
    const char* org = topic;

    while (*client_prefix != '\0') {
        if (*topic == '\0' || *topic != *client_prefix) return 0;
        ++ client_prefix;
        ++ topic;
    }
    
    if (*topic == '\0' || *topic != MQTT_TOPIC_SEP) return 0;
    ++ topic;

    while (*func_prefix != '\0') {
        if (*topic == '\0' || *topic != *func_prefix) return 0;
        ++ func_prefix;
        ++ topic;
    }

    return topic - org;
}

int do_rpc_callback(rc_mqtt_client* mqtt, mqtt_rpc_event_callback callback, 
        char* msgId, char* acktopic, char* body)
{
    any_t p;
    int ret;
    char str_rc[10] = {0};
    char* output = NULL;
    MQTTClient_deliveryToken token;
    rc_buf_t response = rc_buf_stack();

    ret = callback(mqtt, body, strlen(body), &response);
    snprintf(str_rc, sizeof(str_rc)-1, "%d", ret);
    BEGIN_JSON_OBJECT(body)
        JSON_OBJECT_ADD_STRING(body, i, msgId);
        JSON_OBJECT_ADD_OBJECT(body, c)
            JSON_OBJECT_ADD_STRING(c, rc, str_rc);
            if (ret == RC_SUCCESS && get_buf_ptr(&response) != NULL) {
                JSON_OBJECT_ADD_STRING(c, body, get_buf_ptr(&response));
            }
        END_JSON_OBJECT_ITEM(c)
        output = JSON_TO_STRING(body);
    END_JSON_OBJECT(body);
#ifdef __QUARK_RTTHREAD__
    ret = MQTTClient_publish(mqtt->client, acktopic, strlen(output), output, MQTT_RPC_QOS, 0, &token);
#elif defined(__QUARK_FREERTOS__)
    ret = RC_ERROR_NOT_IMPLEMENT;
#elif defined(__QUARK_LINUX__)
    ret = MQTTClient_publish(mqtt->client, acktopic, strlen(output), output, MQTT_RPC_QOS, 0, &token);
#endif
    if (output != NULL) {
        free(output);
        output = NULL;
    }
    
    LOGI(MQ_TAG, "mqtt client rpc ack(%s), ret(%d), msgId(%s), rc(%s), content(%s)", 
            acktopic, ret, msgId, str_rc, get_buf_ptr(&response));
    rc_buf_free(&response);
    return RC_SUCCESS;
}

int parse_rpc_msg(cJSON* input, char** msgId, char** acktopic, char** body)
{
    BEGIN_MAPPING_JSON(input, root)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, i, *msgId)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, a, *acktopic)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, c, *body)
    END_MAPPING_JSON(root);
    return 0;
}

int on_rpc_message(rc_mqtt_client* mqtt, const char *topic, MQTTClient_message *message)
{
    int ret = 0;
    char* msgId = NULL;
    char* acktopic = NULL;
    char* body = NULL, *payload = NULL;
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

    payload = (char*)rc_malloc(message->payloadlen + 1);
    if (payload == NULL) {
        LOGW("[MQTT]", "message playload malloc failed");
        return RC_ERROR_NET_FORMAT;
    }
    memcpy(payload, message->payload, message->payloadlen);
    payload[message->payloadlen] = '\0';
    LOGI("[MQTT]", "topic(%s), message(%s)", topic, payload);
    root = cJSON_Parse(payload);
    rc_free(payload);
    payload = NULL;
    if (root != NULL) {
        if (parse_rpc_msg(root, &msgId, &acktopic, &body) == 0) {
            ret = do_rpc_callback(mqtt, callback, msgId, acktopic, body);
        }
        cJSON_Delete(root);
    }
    else {
        LOGW("[MQTT]", "parse mqtt rpc message content failed");
    }

    return ret;
}

int on_normal_message(rc_mqtt_client* mqtt, const char *topic, MQTTClient_message *message)
{
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

    return callback(mqtt, message->payload, message->payloadlen);
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, context);
    LOGI(MQ_TAG, "mqtt(%p) msgarrvd, topic(%s), message-len(%d)", 
            mqtt, topicName, message->payloadlen);
    if (is_topic_match(topicName, mqtt->topic_prefix, MQTT_TOPIC_RPC)) {
        on_rpc_message(mqtt, topicName, message);
    }
    else if (is_topic_match(topicName, mqtt->topic_prefix, MQTT_TOPIC_CMD)) {
        on_normal_message(mqtt, topicName, message);
    }
    MQTTClient_free(topicName);
    MQTTClient_freeMessage(&message);
    return 1;
}

void connlost(void *context, char *cause)
{
    rc_mqtt_client* mqtt = (rc_mqtt_client*)context;
    LOGI(MQ_TAG, "mqtt(%p) connect lost, cause(%s)", mqtt, cause);
    if (mqtt->on_connect != NULL) {
        mqtt->on_connect(mqtt, MQTT_STATUS_DISCONNECT, cause);
    }
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    rc_mqtt_client* mqtt = (rc_mqtt_client*)context;
    LOGI(MQ_TAG, "mqtt(%p) delivered, token(%d)", mqtt, dt);
}

int re_subscribe(any_t client, const char* topic, any_t val)
{
    int ret;
    mqtt_subscribe_t* sub = (mqtt_subscribe_t*)val;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (sub->error != 0 || mqtt->force_re_sub != 0) {
        ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_RPC_QOS);
        LOGI(MQ_TAG, "mqtt(%p) re-subscribe(%s), ret(%d)", mqtt, topic, ret);
        if (ret != MQTTCLIENT_SUCCESS) {
            mqtt->has_sub_failed = 1;
            LOGI(MQ_TAG, "subscribe(%s) failed, ret(%d)", topic, ret);
        }
        else sub->error = 0;
    }

    return MAP_OK;
}

int mqtt_connect_to_remote(rc_mqtt_client* mqtt)
{
    int ret;
    char* passwd = (char*)mqtt->get_session(mqtt->client_id);
    
    LOGI(MQ_TAG, "mqtt(%p) connect clientId(%s), token(%s)", 
            mqtt, mqtt->client_id, passwd);

#ifdef __QUARK_RTTHREAD__
    mqtt->conn_opts.password.cstring = passwd;
    ret = MQTTClient_connect(mqtt->client, &mqtt->conn_opts);
#elif defined(__QUARK_FREERTOS__)
    ret = RC_ERROR_NOT_IMPLEMENT;
#elif defined(__QUARK_LINUX__)
    mqtt->conn_opts.password = passwd;
    ret = MQTTClient_connect(mqtt->client, &mqtt->conn_opts);
    mqtt->conn_opts.password = "";
#endif
    LOGI(MQ_TAG, "mqtt(%p) connect, ret(%d)-%s", mqtt, ret, ret==0?"success":"failed");

    if (ret == MQTTCLIENT_SUCCESS) { // connect success    
        rc_mutex_lock(mqtt->mobject);
        mqtt->force_re_sub = 1;
        mqtt->has_sub_failed = 0;
        hashmap_iterate(mqtt->sub_map, re_subscribe, mqtt);
        mqtt->force_re_sub = 0;
        rc_mutex_unlock(mqtt->mobject);
    }
    
    mqtt->last_reconnect_time = (int)time(NULL);

    mqtt->connectResult = ret;
#ifndef __QUARK_RTTHREAD__    
    if (mqtt->on_connect != NULL) {
        char error[40] = {0};
        snprintf(error, sizeof(error), "mqtt connect result: %d", ret);
        mqtt->on_connect(mqtt, ret ? MQTT_STATUS_DISCONNECT : MQTT_STATUS_CONNECTED, error);
    }
#endif
    return ret;
}

int mqtt_on_timer(rc_timer timer, void* client)
{
    int rc = 0;
    int now = time(NULL);
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (mqtt->reconn_timer == NULL) {
        LOGI(MQ_TAG, "reconnect timer is null, had closed");
        return -1;
    }

    if (MQTTClient_isConnected(mqtt->client)) {
        if (mqtt->has_sub_failed != 0) { // has subscribe failed topic
            mqtt->has_sub_failed = 0;
            hashmap_iterate(mqtt->sub_map, re_subscribe, mqtt);
        }
    }
#ifndef __QUARK_RTTHREAD__
    else if (mqtt->last_reconnect_time + _mqtt_reconnect_interval[mqtt->cur_reconnect_interval_index] <= now) { 
        // need reconnect to the remote mqtt server
        rc = mqtt_connect_to_remote(mqtt);

        if (rc != 0) {
            if (mqtt->cur_reconnect_interval_index != INTERVAL_COUNT - 1) {
                mqtt->cur_reconnect_interval_index ++;
                LOGI(MQ_TAG, "reconn interval adjust to %d sec", 
                        _mqtt_reconnect_interval[mqtt->cur_reconnect_interval_index]);
            }
        }
        else mqtt->cur_reconnect_interval_index = 0;
    }
#endif    

    return RC_ERROR_MQTT_SKIP_CONN;
}

int rc_mqtt_connect(mqtt_client client)
{
    int ret;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    if (mqtt->reconn_timer != NULL) {
        LOGI(MQ_TAG, "mqtt(%p) had enabled auto connect, so wait for it", mqtt);
        return RC_ERROR_MQTT_CONNECT;
    }

    return mqtt_connect_to_remote(mqtt);
}

int rc_mqtt_enable_auto_connect(mqtt_client client, rc_timer_manager mgr, mqtt_connect_callback callback)
{
    int rc;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (mqtt->reconn_timer != NULL) {
        LOGI(MQ_TAG, "mqtt(%p) had enabled auto connect", mqtt);
        return RC_ERROR_MQTT_CONNECT;
    }

    mqtt->on_connect = callback;
#ifdef __QUARK_RTTHREAD__
#elif defined(__QUARK_LINUX__)
    mqtt->cur_reconnect_interval_index = 0;
    mqtt->last_reconnect_time = 0;
    mqtt->reconn_timer = rc_timer_create(mgr, MQ_TIMER_INTERVAL * 1000, MQ_TIMER_INTERVAL * 1000, mqtt_on_timer, mqtt);
#endif
    
    return RC_SUCCESS;
}

int rc_mqtt_subscribe(mqtt_client client, const char* topic, mqtt_subscribe_callback callback)
{
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
        sub = (mqtt_subscribe_t*)rc_malloc(sizeof(mqtt_subscribe_t) + strlen(topic));
        memset(sub, 0, sizeof(mqtt_subscribe_t));
        sub->type = 0;
        sub->callback = callback;
        strcpy(sub->topic, topic);
        ret = hashmap_put(mqtt->sub_map, sub->topic, sub);
        rc_mutex_unlock(mqtt->mobject);

        if (ret != MAP_OK) {
            LOGW(MQ_TAG, "mqtt(%p) bind topic(%s) failed with(%d)", mqtt, topic, ret);
            rc_free(sub);
            return RC_ERROR_MQTT_SUBSCRIBE;
        }

        ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_CMD_QOS);

        LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d)", mqtt, topic, ret);
        if (ret != MQTTCLIENT_SUCCESS) {
            sub->error = 1;
            mqtt->has_sub_failed = 1;
        }
    }
    else {
        ret = MAP_OK;
        ((mqtt_subscribe_t*)val)->callback = callback; // update callback
        rc_mutex_unlock(mqtt->mobject);
    }

    LOGI(MQ_TAG, "mqtt(%p) cmd map (%s) -> %p", mqtt, topic, callback);
    return RC_SUCCESS;
}

int rc_mqtt_publish(mqtt_client client, const char* topic, const char* body, int len)
{
    int ret;
    MQTTClient_deliveryToken token;
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);
    if (is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_EVT) == 0 && 
        is_topic_match(topic, mqtt->topic_prefix, MQTT_TOPIC_CMD) == 0) { // for test to send cmd
        LOGI(MQ_TAG, "invalidate mqtt event topic(%s)", topic);
        return RC_ERROR_MQTT_PUBLISH;
    }

    ret = MQTTClient_publish(mqtt->client, (char*)topic, len, (char*)body, MQTT_CMD_QOS, 0, &token);

    LOGI(MQ_TAG, "mqtt(%p) publish(%s), content-length(%d), ret(%d), token(%d)", 
            mqtt, topic, len, ret, token)
    return ret == 0 ? RC_SUCCESS : RC_ERROR_MQTT_PUBLISH;
}

int rc_mqtt_rpc_send(mqtt_client client, const char* topic, const char* body, int len, int timeout, rc_buf_t* response)
{
    return 0;
}

int rc_mqtt_rpc_event(mqtt_client client, const char* topic, mqtt_rpc_event_callback callback)
{
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
        sub = (mqtt_subscribe_t*)rc_malloc(sizeof(mqtt_subscribe_t) + strlen(topic));
        memset(sub, 0, sizeof(mqtt_subscribe_t));
        sub->type = 1;
        sub->callback = callback;
        strcpy(sub->topic, topic);
        ret = hashmap_put(mqtt->sub_map, sub->topic, sub);
        rc_mutex_unlock(mqtt->mobject);

        if (ret != MAP_OK) {
            LOGW(MQ_TAG, "mqtt(%p) bind topic(%s) failed with(%d)", mqtt, topic, ret);
            rc_free(sub);
            return RC_ERROR_MQTT_SUBSCRIBE;
        }

        LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s)", mqtt, topic);

        ret = MQTTClient_subscribe(mqtt->client, topic, MQTT_RPC_QOS);

        if (ret != MQTTCLIENT_SUCCESS) {
            LOGI(MQ_TAG, "subscribe(%s) failed, ret(%d)", topic, ret);
            sub->error = 1;
            mqtt->has_sub_failed = 1;
        }
    }
    else {
        ret = MAP_OK;
        ((mqtt_subscribe_t*)val)->callback = callback; // update callback
        rc_mutex_unlock(mqtt->mobject);
    }

    LOGI(MQ_TAG, "mqtt(%p) rpc map (%s) -> %p", mqtt, topic, callback);
    return 0;
}

int sub_item_free(any_t ipm, const char* key, any_t value)
{
    rc_free(value);
    return RC_SUCCESS;
}

int rc_mqtt_close(mqtt_client client)
{
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

