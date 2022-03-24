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

    mqtt->app_id = rc_buf_tail_ptr(&mqtt->buff);
    rc_buf_append(&mqtt->buff, app_id, strlen(app_id) + 1);
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

static int _client_subscribe(rc_mqtt_client* mqtt) {
    char topic[100] = {0};
    snprintf(topic, 100, "/proton/%s/%s/cmd/#", mqtt->app_id, mqtt->client_id);
    int ret =
        mqtt_subscribe(&mqtt->client, topic, INT_QOS_TO_ENUM(MQTT_CMD_QOS));
    LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d)", mqtt, topic, ret);
    if (ret == MQTT_OK) {
        snprintf(topic, 100, "/proton/%s/%s/rpc/#", mqtt->app_id,
                 mqtt->client_id);
        ret =
            mqtt_subscribe(&mqtt->client, topic, INT_QOS_TO_ENUM(MQTT_RPC_QOS));
        LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d)", mqtt, topic, ret);
    }

    return ret == MQTT_OK ? 0 : RC_ERROR_MQTT_SUBSCRIBE;
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
        mqtt_reinit(
            &mqtt->client, socket, (uint8_t*)rc_buf_head_ptr(mqtt->send_buf),
            mqtt->send_buf->total, (uint8_t*)rc_buf_head_ptr(mqtt->recv_buf),
            mqtt->recv_buf->total);
    } else {
        ret = mqtt_init(
            &mqtt->client, socket, (uint8_t*)rc_buf_head_ptr(mqtt->send_buf),
            mqtt->send_buf->total, (uint8_t*)rc_buf_head_ptr(mqtt->recv_buf),
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
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    if (mqtt->sync_thread != NULL) {
        LOGW(MQ_TAG, "mqtt client had starte");
        return -1;
    }

    mqtt->on_connect = callback;

    rc_thread_context_t tc = {
        .joinable = 1, .name = "mqtt", .priority = 0, .stack_size = 4096};
    mqtt->sync_thread = rc_thread_create(mqtt_main_thread, mqtt, &tc);

    return RC_SUCCESS;
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

        // SUBSCRIBE
        if (_client_subscribe(mqtt) != 0) {
            mqtt_client_disconnect(mqtt);
            break;
        }

        rc_mutex_lock(mqtt->mobject);
        mqtt->is_connected = 1;
        mqtt->last_reconnect_time = (int)time(NULL);
        mqtt->cur_reconnect_interval_index = 0;
        rc_mutex_unlock(mqtt->mobject);

        while (!mqtt->is_exit && mqtt_sync(&mqtt->client) == MQTT_OK) {
            // nope
        }

        mqtt_client_disconnect(mqtt);

        LOGI(MQ_TAG, "mqtt(%p) client closed", mqtt);
    }

    mqtt_client_disconnect(mqtt);

    return NULL;
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
