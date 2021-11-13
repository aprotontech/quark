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

#include "env.h"
#include "logs.h"
#include "hashmap.h"
#include "cJSON.h"
#include "rc_json.h"
#include "rc_mutex.h"
#include "rc_mqtt.h"

typedef struct _rc_net_dispatch_t {
    void* callback;
    int type;
    char key[4];
} rc_net_dispatch_t;

typedef struct _rc_netmsg_mgr_t {
    map_t items;
    rc_mutex mobject;
} rc_netmsg_mgr_t;

const char* get_message_key(cJSON* input, cJSON** data);
int regist_net_dispatch(int type, const char* key, void* callback);
const char* env_sdk_device_token(const char* client_id);
int get_mqtt_host_port(rc_runtime_t* env, const char* service, char* ip, int len, int *port);
extern int free_hash_item(any_t n, const char* key, any_t val);

int rc_net_publish(const char* topic, const char* message, int len)
{
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->mqtt != NULL) {
        return rc_mqtt_publish(env->mqtt, topic, message, len);
    }

    return RC_ERROR_MQTT_PUBLISH;
}

rc_net_dispatch_mgr_t net_dispatch_init()
{
    rc_netmsg_mgr_t* mgr = (rc_netmsg_mgr_t*)rc_malloc(sizeof(rc_netmsg_mgr_t));
    mgr->mobject = rc_mutex_create(NULL);
    mgr->items = hashmap_new();
    return mgr;
}

int net_dispatch_uninit(rc_net_dispatch_mgr_t dismgr)
{
    DECLEAR_REAL_VALUE(rc_netmsg_mgr_t, mgr, dismgr);
    hashmap_iterate(mgr->items, free_hash_item, NULL);
    hashmap_free(mgr->items);
    rc_mutex_destroy(mgr->mobject);
    rc_free(mgr);
}

int rc_regist_netcmd(const char* key, rc_netcmd_dispatch_callback callback)
{
    return regist_net_dispatch(0, key, callback);
}

int rc_regist_netrpc(const char* key, rc_netrpc_dispatch_callback callback)
{
    return regist_net_dispatch(1, key, callback);
}

int regist_net_dispatch(int type, const char* key, void* callback)
{
    any_t netdis;
    rc_net_dispatch_t* pnet = NULL;
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || key == NULL || callback == NULL) {
        LOGW("[NETDIS]", "input params is invalidate");
        return -1;
    }

    DECLEAR_REAL_VALUE(rc_netmsg_mgr_t, mgr, env->net_dispatch);

    if (mgr->items) {
        rc_mutex_lock(mgr->mobject);
        if (hashmap_get(mgr->items, (char*)key, &netdis) == MAP_OK) {
            pnet = (rc_net_dispatch_t*)netdis;
            pnet->callback = callback;
        } else {
            pnet = (rc_net_dispatch_t*)rc_malloc(sizeof(rc_net_dispatch_t) + strlen(key));
            strcpy(pnet->key, key);
            pnet->type = type;
            pnet->callback = callback;
            hashmap_put(mgr->items, pnet->key, pnet);
        }

        rc_mutex_unlock(mgr->mobject);
        LOGI("[NETDIS]", "regist type(%d), key(%s), callback(%p)", type, key, callback);
    }
    
    return 0;
}

int dispatch_net_message(int type, const char* message, rc_buf_t* response)
{
    cJSON* input = NULL;
    cJSON* data = NULL;
    const char* key = NULL;
    any_t netdis;
    int rc = RC_ERROR_NET_NOT_ROUTER;
    rc_net_dispatch_t* pnet = NULL;
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || message == NULL) {
        LOGW("[NETDIS]", "input params is invalidate");
        return RC_ERROR_NET_FORMAT;
    }

    DECLEAR_REAL_VALUE(rc_netmsg_mgr_t, mgr, env->net_dispatch);

    input = cJSON_Parse(message);
    if (input != NULL) {
        key = get_message_key(input, &data);
        if (mgr->items && key != NULL) {
            rc_mutex_lock(mgr->mobject);
            if (hashmap_get(mgr->items, (char*)key, &netdis) == MAP_OK) {
                pnet = (rc_net_dispatch_t*)netdis;
                rc_mutex_unlock(mgr->mobject);
                if (pnet != NULL && pnet->callback != NULL) {
                    if (pnet->type == 0) {
                        rc = ((rc_netcmd_dispatch_callback)pnet->callback)(key, data);
                    } else {
                        rc = ((rc_netrpc_dispatch_callback)pnet->callback)(key, data, response);
                    }
                }
            } else {
                rc_mutex_unlock(mgr->mobject);
            }
        }
        cJSON_Delete(input);
    }

    return rc;
}

int kl_json_decode_message_kv(cJSON* input, char** key, cJSON** data)
{
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

const char* get_message_key(cJSON* input, cJSON** data)
{
    char* key = NULL;
    kl_json_decode_message_kv(input, &key, data);

    return key;
}

char* format_send_event_message(const char* action, cJSON* body);
int rc_send_netevt(const char* dest, const char* action, cJSON* body)
{
    int rcode = 0;
    char* message = NULL;
    char topic[100] = {0};
    if (dest == NULL) { // to server
        snprintf(topic, 100, "/%s/%s/evt", rc_get_app_id(), rc_get_client_id());
        message = format_send_event_message(action, body);
        rcode = rc_net_publish(topic, message, strlen(message));
        free(message);
    } else if (rc_get_client_id() != NULL && strcmp(dest, rc_get_client_id()) == 0) {
        snprintf(topic, 100, "/%s/%s/cmd", rc_get_app_id(), rc_get_client_id());
        message = format_send_event_message(action, body);
        rcode = rc_net_publish(topic, message, strlen(message));
        free(message);
    } else {
        LOGW("[NETDIS]", "not supported dest(%s) now", dest);
    }
    return rcode;
}

int rc_call_netrpc(const char* dest, const char* action, cJSON* body, rc_buf_t* response)
{
    return 0;
}

char* format_send_event_message(const char* action, cJSON* body)
{
    char* message = NULL;
    BEGIN_JSON_OBJECT(root)
        JSON_OBJECT_ADD_STRING(root, action, action)
        cJSON_AddItemToObject(JSON(root), "data", body != NULL ? body : cJSON_CreateObject());
        message = JSON_TO_STRING(root);
        if (body != NULL) {
            cJSON_DetachItemFromObject(JSON(root), "data");
        }
    END_JSON_OBJECT(root);
    return message;
}

int sdk_mqtt_status_callback(rc_mqtt_client client, int status, const char* cause)
{
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->mqtt == client) {
        if (env->kl_change != NULL) {
            env->kl_change(status, cause);
        }
    }

    return 0;
}

int mqtt_client_init(rc_runtime_t* env, const char* app_id, const char* client_id)
{
    ///////////////////////////////////////////////////
    // MQTT    
    rc_settings_t* settings = &env->settings;

    env->net_dispatch = net_dispatch_init();
    if (env->net_dispatch == NULL) {
        LOGI(SDK_TAG, "sdk init failed, net dispatch init failed");
        return RC_ERROR_SDK_INIT;
    }

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
        rc_buf_append(buff, settings->client_id, strlen(settings->client_id) + 1);
        client_id = rc_buf_head_ptr(buff);
        username = rc_buf_tail_ptr(buff);

        snprintf(rc_buf_tail_ptr(buff), RC_BUF_LEFT_SIZE(buff), "%s%s;%s;%06d;%ld", settings->app_id, client_id, "12010126",
                 rand(), time(NULL) + 600);
    }

    env->mqtt = rc_mqtt_create(host, port, 
            app_id, client_id, username, env_sdk_device_token);
    rc_buf_free(buff);
    if (env->mqtt == NULL) {
        LOGI(SDK_TAG, "sdk init failed, mqtt client init failed");
        return RC_ERROR_SDK_INIT;
    }

    rc = rc_mqtt_enable_auto_connect(env->mqtt, env->timermgr, sdk_mqtt_status_callback, 1);
    if (rc != 0) {
        LOGI(SDK_TAG, "sdk init failed, mqtt client enable auto connect failed");
        return RC_ERROR_SDK_INIT;
    }

    return RC_SUCCESS;
}

const char* env_sdk_device_token(const char* client_id)
{
    rc_runtime_t* env = get_env_instance();
    const char* cid, *token;
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

int get_mqtt_host_port(rc_runtime_t* env, const char* service, char* ip, int len, int *port)
{
    if (env->settings.iot_platform == RC_IOT_TENCENT) {
        if (strlen(env->settings.app_id) + strlen(".iotcloud.tencentdevices.com") + 1 >= len) {
            LOGI(SDK_TAG, "input buffer is too small");
            return -1;
        }
        strcpy(ip, env->settings.app_id);
        strcat(ip, ".iotcloud.tencentdevices.com");
        *port = 1883;

        return 0;
    }

    return 0;
}