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

int _mqtt_reconnect_interval[] = {5, 10, 10, 20, 30, 40, 60};

static void* mqtt_main_thread(void* arg);

mqtt_client mqtt_client_init(const char* app_id,
                             mqtt_session_callback sessioncb) {
    if (sessioncb == NULL) {
        LOGI(MQ_TAG, "input sessioncb(%p) callback is invalidate", sessioncb);
        return NULL;
    }

    if (app_id == NULL) {
        LOGI(MQ_TAG, "input app_id is invalidate");
        return NULL;
    }

    rc_mqtt_client* mqtt = (rc_mqtt_client*)rc_malloc(sizeof(rc_mqtt_client));
    memset(mqtt, 0, sizeof(rc_mqtt_client));

    mqtt->sync_thread = NULL;
    mqtt->mobject = rc_mutex_create(NULL);
    mqtt->mevent = rc_event_init();

    mqtt->get_session = sessioncb;

    mqtt->buff = rc_buf_stack();
    mqtt->buff.total = MQTT_CLIENT_PAD_SIZE;
    // topic prefix buffer
    mqtt->topic_prefix = rc_buf_tail_ptr(&mqtt->buff);
    mqtt->buff.length = MQTT_TOPIC_PREFIX_LENGTH;

    char* ptr = rc_buf_tail_ptr(&mqtt->buff);
    mqtt_client_session_t session = {
        .client_id = &ptr[0],
        .password = &ptr[MQTT_MAX_CLIENTID_LENGTH],
        .username = &ptr[MQTT_MAX_CLIENTID_LENGTH + MQTT_MAX_PASSWD_LENGTH]};
    mqtt->session = session;
    mqtt->buff.length += MQTT_MAX_CLIENTID_LENGTH + MQTT_MAX_PASSWD_LENGTH +
                         MQTT_MAX_USERNAME_LENGTH;

    // appid
    mqtt->app_id = rc_buf_tail_ptr(&mqtt->buff);
    rc_buf_append(&mqtt->buff, app_id, strlen(app_id) + 1);

    mqtt->on_connect = NULL;

    mqtt->force_re_sub = 0;
    mqtt->has_sub_failed = 0;
    mqtt->sub_map = hashmap_new();

    mqtt->is_inited = 0;
    mqtt->is_connected = 0;
    mqtt->is_exit = 0;

    mqtt->recv_buf = rc_buf_init(960);
    mqtt->send_buf = rc_buf_init(960);

    rc_backoff_algorithm_init(
        &mqtt->reconnect_backoff, _mqtt_reconnect_interval,
        sizeof(_mqtt_reconnect_interval) / sizeof(int), -1);

    LOGI(MQ_TAG, "mqtt(%p) client inited", mqtt);

    return mqtt;
}

int mqtt_client_start(mqtt_client client, const char* host, int port,
                      int is_ssl, mqtt_connect_callback callback) {
    DECLEAR_REAL_VALUE(rc_mqtt_client, mqtt, client);

    LOGI(MQ_TAG, "mqtt client remote(%s:%d), is_ssl(%d)", PRINTSTR(host), port,
         is_ssl);

    if (host == NULL || port <= 0) {
        LOGI(MQ_TAG, "input port is invalidate");
        return RC_ERROR_INVALIDATE_INPUT;
    }

    mqtt->on_connect = callback;
    if (mqtt->sync_thread != NULL) {
        LOGI(MQ_TAG, "mqtt client had started");
        // mqtt client is not connected, so connect to remote again
        if (!mqtt->is_connected) {
            rc_event_signal(mqtt->mevent);
        }
    } else {
        mqtt->remote_host.sin_family = AF_INET;
        mqtt->remote_host.sin_port = htons(port);
        mqtt->remote_host.sin_addr.s_addr = inet_addr(host);

        rc_thread_context_t tc = {
            .joinable = 1, .name = "mqtt", .priority = 0, .stack_size = 4096};
        mqtt->sync_thread = rc_thread_create(mqtt_main_thread, mqtt, &tc);
    }

    return RC_SUCCESS;
}

static int _client_subscribe(rc_mqtt_client* mqtt) {
    char topic[100] = {0};
    snprintf(topic, 100, "/aproton/%s/%s/cmd/#", mqtt->app_id,
             mqtt->session.client_id);
    int ret =
        mqtt_subscribe(&mqtt->client, topic, INT_QOS_TO_ENUM(MQTT_CMD_QOS));
    LOGI(MQ_TAG, "mqtt(%p) subscribe topic(%s) ret(%d), success(%s)", mqtt,
         topic, ret, ret == MQTT_OK ? "yes" : "no");
    if (ret == MQTT_OK) {
        snprintf(topic, 100, "/proton/%s/%s/rpc/#", mqtt->app_id,
                 mqtt->session.client_id);
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

int mqtt_update_session(rc_mqtt_client* mqtt) {
    int ret;
    mqtt_client_session_t session;
    memset(&session, 0, sizeof(session));
    if ((ret = mqtt->get_session(mqtt, &session)) != 0) {
        LOGW(MQ_TAG, "mqtt(%p) get session failed with %d", mqtt, ret);
        return RC_ERROR_MQTT_CONNECT;
    }

    LOGI(MQ_TAG, "mqtt(%p) connect clientId(%s), token(%s), username(%s)", mqtt,
         PRINTSTR(session.client_id), PRINTSTR(session.password),
         PRINTSTR(session.username));

    if (session.username == NULL || session.password == NULL ||
        session.client_id == NULL ||
        strlen(session.client_id) >= MQTT_MAX_CLIENTID_LENGTH ||
        strlen(session.password) >= MQTT_MAX_PASSWD_LENGTH ||
        strlen(session.username) >= MQTT_MAX_USERNAME_LENGTH) {
        LOGW(MQ_TAG, "result session content is invalidate");
        return RC_ERROR_MQTT_CONNECT;
    }

    strcpy((char*)mqtt->session.client_id, session.client_id);
    strcpy((char*)mqtt->session.password, session.password);
    strcpy((char*)mqtt->session.username, session.username);

    return RC_SUCCESS;
}

int mqtt_connect_to_remote(rc_mqtt_client* mqtt) {
    int ret, socket;

    if (mqtt_update_session(mqtt) != 0) {
        LOGW(MQ_TAG, "mqtt update session failed");
        return RC_ERROR_MQTT_CONNECT;
    }

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

    ret = mqtt_connect(&mqtt->client, mqtt->session.client_id, NULL, NULL, 0,
                       mqtt->session.username, mqtt->session.password,
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

    while (!mqtt->is_exit) {
        while (!mqtt->is_exit) {
            rc_event_wait(mqtt->mevent, 500);

            if (rc_backoff_algorithm_can_retry(&mqtt->reconnect_backoff)) {
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

        rc_backoff_algorithm_set_result(&mqtt->reconnect_backoff, ret == 0);

        if (ret != 0) {
            continue;
        }

        // SUBSCRIBE
        if (_client_subscribe(mqtt) != 0) {
            mqtt_client_disconnect(mqtt);
            continue;
        }

        rc_mutex_lock(mqtt->mobject);
        mqtt->is_connected = 1;
        rc_mutex_unlock(mqtt->mobject);

        while (!mqtt->is_exit && mqtt_sync(&mqtt->client) == MQTT_OK) {
            // nope
        }

        mqtt_client_disconnect(mqtt);
        rc_backoff_algorithm_set_result(&mqtt->reconnect_backoff, 0);

        LOGI(MQ_TAG, "mqtt(%p) client closed", mqtt);
    }

    mqtt_client_disconnect(mqtt);

    return NULL;
}

int sub_item_free(any_t ipm, const char* key, any_t value) {
    rc_free(value);
    return RC_SUCCESS;
}

int mqtt_client_close(mqtt_client client) {
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
