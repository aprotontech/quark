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

const char* env_sdk_device_token(const char* client_id);
int get_mqtt_host_port(rc_runtime_t* env, const char* service, char* ip,
                       int len, int* port);
extern int free_hash_item(any_t n, const char* key, any_t val);

int rc_net_publish(const char* topic, const char* message, int len) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->mqtt != NULL) {
        return rc_mqtt_publish(env->mqtt, topic, message, len);
    }

    return RC_ERROR_MQTT_PUBLISH;
}

int kl_json_decode_message_kv(cJSON* input, char** key, cJSON** data) {
    char* tmp = NULL;
    BEGIN_MAPPING_JSON(input, root)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, action, tmp)
        *data = cJSON_GetObjectItem(JSON(root), "data");
    END_MAPPING_JSON(root);

    *key = tmp;
    if (*data == NULL) {
        *data = input;
    }
    return 0;
}

const char* get_message_key(cJSON* input, cJSON** data) {
    char* key = NULL;
    kl_json_decode_message_kv(input, &key, data);

    return key;
}

int rc_call_netrpc(const char* dest, const char* action, cJSON* body,
                   rc_buf_t* response) {
    return 0;
}

int sdk_mqtt_status_callback(rc_mqtt_client client, int status,
                             const char* cause) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->mqtt == client) {
        if (env->kl_change != NULL) {
            env->kl_change(status, cause);
        }
    }

    return 0;
}

int mqtt_client_init(rc_runtime_t* env, const char* app_id,
                     const char* client_id) {
    ///////////////////////////////////////////////////
    // MQTT
    rc_settings_t* settings = &env->settings;

    char host[64] = {0};
    int port = 1883;
    int rc = get_mqtt_host_port(env, "mqtt", host, sizeof(host), &port);
    if (rc != 0) {
        LOGI(SDK_TAG, "sdk init failed, get mqtt host, port failed");
        return RC_ERROR_SDK_INIT;
    }

    const char* username = NULL;
    rc_buf_t* buff = NULL;
    if (settings->iot_platform == RC_IOT_TENCENT) {
        buff = rc_buf_init(128);
        rc_buf_append(buff, settings->app_id, strlen(settings->app_id));
        rc_buf_append(buff, settings->client_id,
                      strlen(settings->client_id) + 1);
        client_id = rc_buf_head_ptr(buff);
        username = rc_buf_tail_ptr(buff);

        snprintf(rc_buf_tail_ptr(buff), RC_BUF_LEFT_SIZE(buff),
                 "%s%s;%s;%06d;%ld", settings->app_id, client_id, "12010126",
                 rand(), time(NULL) + 600);
    }

    env->mqtt = rc_mqtt_create(host, port, app_id, client_id, username,
                               env_sdk_device_token);
    rc_buf_free(buff);
    if (env->mqtt == NULL) {
        LOGI(SDK_TAG, "sdk init failed, mqtt client init failed");
        return RC_ERROR_SDK_INIT;
    }

    rc = rc_mqtt_start(env->mqtt, sdk_mqtt_status_callback);
    if (rc != 0) {
        LOGI(SDK_TAG,
             "sdk init failed, mqtt client enable auto connect failed");
        return RC_ERROR_SDK_INIT;
    }

    return RC_SUCCESS;
}

const char* env_sdk_device_token(const char* client_id) {
    rc_runtime_t* env = get_env_instance();
    const char *cid, *token;
    if (env != NULL && env->device != NULL) {
        switch (env->settings.iot_platform) {
        case RC_IOT_TENCENT:
            break;
        case RC_IOT_QUARK:
        default:
            cid = get_device_client_id(env->device);
            if (cid != NULL && strcmp(cid, client_id) == 0) {
                token = get_device_session_token(env->device);
            }
            break;
        }
    }

    return token != NULL ? token : "";
}

int get_mqtt_host_port(rc_runtime_t* env, const char* service, char* ip,
                       int len, int* port) {
    if (env->settings.iot_platform == RC_IOT_TENCENT) {
        if (strlen(env->settings.app_id) +
                strlen(".iotcloud.tencentdevices.com") + 1 >=
            len) {
            LOGI(SDK_TAG, "input buffer is too small");
            return -1;
        }
        strcpy(ip, env->settings.app_id);
        strcat(ip, ".iotcloud.tencentdevices.com");
        *port = 1883;

        return 0;
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