/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     kldispatch.c
 * @author   kuper - kuper@aproton.tech
 * @data     2020-02-04 17:30:56
 * @version  0
 * @brief
 *
 **/

#include <stddef.h>

#include "cJSON.h"
#include "env.h"
#include "hashmap.h"
#include "logs.h"
#include "rc_json.h"
#include "rc_mqtt.h"
#include "rc_mutex.h"
#include "rc_url.h"

int get_mqtt_ip_port(rc_runtime_t* env, const char* service, char* ip,
                     int* port);

int rc_call_netrpc(const char* dest, const char* action, cJSON* body,
                   rc_buf_t* response) {
    return 0;
}

int _mqtt_remote_cmd_callback(mqtt_client client, const char* from,
                              const char* type, const char* message,
                              int message_length, void* args) {
    return ((rc_remote_cmd_callback)args)(message, message_length);
}

int rc_regist_cmd_handle(const char* topic, rc_remote_cmd_callback callback) {
    rc_runtime_t* env = get_env_instance();
    if (env == NULL && env->mqtt == NULL) {
        LOGW(SDK_TAG, "mqtt client is not inited");
        return RC_ERROR_SDK_INIT;
    }

    if (callback == NULL || topic == NULL) {
        LOGW(SDK_TAG, "invalidate input topic(%s), callback(%p)",
             PRINTSTR(topic), callback);
        return RC_ERROR_INVALIDATE_INPUT;
    }

    return mqtt_client_cmd_subscribe(env->mqtt, topic,
                                     _mqtt_remote_cmd_callback, callback);
}

int sdk_mqtt_status_callback(rc_mqtt_client client, int status,
                             const char* cause) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->mqtt == client) {
        network_set_available(env->netmgr, NETWORK_MASK_KEEPALIVE,
                              status == MQTT_STATUS_CONNECTED);

        if (env->kl_change != NULL) {
            env->kl_change(status, cause);
        }
    }

    return 0;
}

int mqtt_auto_connect(rc_runtime_t* env) {
    int port = 1883;
    char ip[32] = {0};

    if (network_is_available(env->netmgr, NETWORK_MASK_SESSION) == 0) {
        LOGW(SDK_TAG, "device session is not registed, so skip connect mqtt");
        return RC_ERROR_SDK_INIT;
    }

    int rc = get_mqtt_ip_port(env, "mqtt", ip, &port);
    if (rc != 0) {
        LOGI(SDK_TAG, "sdk init failed, get mqtt host, port failed");
        return RC_ERROR_SDK_INIT;
    }

    rc = mqtt_client_start(env->mqtt, ip, port, 0, sdk_mqtt_status_callback);
    if (rc != 0) {
        LOGI(SDK_TAG,
             "sdk init failed, mqtt client enable auto connect failed");
        return RC_ERROR_SDK_INIT;
    }

    return RC_SUCCESS;
}

int env_sdk_device_session(mqtt_client client, mqtt_client_session_t* session) {
    rc_runtime_t* env = get_env_instance();
    memset(session, 0, sizeof(mqtt_client_session_t));
    if (env != NULL && env->device != NULL) {
        switch (env->settings.iot_platform) {
        case RC_IOT_TENCENT:
            break;
        case RC_IOT_QUARK:
        default:
            session->client_id = get_device_client_id(env->device);
            session->password = get_device_session_token(env->device);
            session->username = get_device_client_id(env->device);
            break;
        }
    }

    return (session->client_id != NULL && session->password != NULL &&
            session->username != NULL)
               ? RC_SUCCESS
               : RC_ERROR_MQTT_SESSION;
}

int get_mqtt_ip_port(rc_runtime_t* env, const char* service, char* ip,
                     int* port) {
    if (env->settings.iot_platform == RC_IOT_TENCENT) {
        char host[64] = {0};
        if (strlen(env->settings.app_id) +
                strlen(".iotcloud.tencentdevices.com") + 1 >=
            sizeof(host)) {
            LOGI(SDK_TAG, "input host buffer is too small");
            return -1;
        }
        strcpy(host, env->settings.app_id);
        strcat(host, ".iotcloud.tencentdevices.com");

        *port = 1883;

        // convert host to ip
        return rc_dns_resolve(host, ip, 0);
    } else {
        rc_service_protocol_info_t info;
        if (rc_service_query(env->ansmgr, "keepalive", "mqtt", &info) == 0) {
            strcpy(ip, info.ip);
            *port = info.port;

            return 0;
        }
    }

    return -1;
}