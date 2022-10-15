/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     device.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-15 17:40:35
 * @version  0
 * @brief
 *
 **/

#include "logs.h"
#include "md5.h"
#include "rc_buf.h"
#include "rc_crypt.h"
#include "rc_device.h"
#include "rc_error.h"
#include "rc_http_request.h"
#include "rc_json.h"
#include "rc_mutex.h"
#include "backoff.h"

#define DM_TAG "[DEVICE]"

extern void md5tostr(const char* message, long len, char* output);

#define DM_TOKEN_REFRESH_INTERVAL 10

int _device_regist_retry_intervals[] = {10, 10, 30, 60, 120, 180};

typedef struct _rc_device_t {
    char* app_id;
    char* client_id;
    char* uuid;
    char* public_key;
    char* app_secret;
    char session_token[40];
    http_request_url_info_t url;
    http_manager manager;
    rc_hardware_info* hardware;
    int expire;
    int timeout;

    int regist_type;  // 0-android, 1-storybox
    rc_timer refresh_timer;
    device_token_change callback;
    backoff_algorithm_t sbackoff;

    int is_refreshing;

    rc_mutex mobject;
} rc_device_t;

#define UPDATE_DEVICE_PROPERTY(device, property, value)                 \
    LOGI(DM_TAG, "before udpate %s=%s", #property, value);              \
    if (device->property != NULL) {                                     \
        LOGI(DM_TAG, "device propertiy org %s is not null", #property); \
        rc_free(device->property);                                      \
    }                                                                   \
    device->property = rc_copy_string(value);

char* build_hardware_info(const rc_hardware_info* hardware) {
    LOGI(DM_TAG, "build_hardware_info");
    if (hardware == NULL || (hardware->bid == NULL && hardware->cpu == NULL &&
                             hardware->mac == NULL)) {
        char* hstr = (char*)rc_malloc(4);
        memset(hstr, 0, 4);
        return hstr;
    }

    int lb = hardware->bid != NULL ? (strlen(hardware->bid) + 5) : 0;
    int lc = hardware->cpu != NULL ? (strlen(hardware->cpu) + 5) : 0;
    int lm = hardware->mac != NULL ? (strlen(hardware->mac) + 5) : 0;
    char* hstr = (char*)rc_malloc(lb + lc + lm + 1);
    hstr[0] = '\0';

    if (hardware->bid != NULL) {
        strcat(hstr, ",bid=");
        strcat(hstr, hardware->bid);
    }

    if (hardware->cpu != NULL) {
        strcat(hstr, ",cpu=");
        strcat(hstr, hardware->cpu);
    }

    if (hardware != NULL && hardware->mac != NULL) {
        strcat(hstr, ",mac=");
        strcat(hstr, hardware->mac);
    }

    return hstr;
}

aidevice rc_device_init(http_manager mgr, const rc_hardware_info* hardware) {
    rc_device_t* device = (rc_device_t*)rc_malloc(sizeof(rc_device_t));
    memset(device, 0, sizeof(rc_device_t));

    LOGI(DM_TAG, "rc_device_init: mac(%s), bid(%s), cpu(%s)",
         hardware && hardware->mac ? hardware->mac : "",
         hardware && hardware->bid ? hardware->bid : "",
         hardware && hardware->cpu ? hardware->cpu : "");

    device->manager = mgr;
    device->hardware = (rc_hardware_info*)rc_malloc(sizeof(rc_hardware_info));
    device->hardware->cpu =
        hardware && hardware->cpu ? rc_copy_string(hardware->cpu) : NULL;
    device->hardware->mac =
        hardware && hardware->mac ? rc_copy_string(hardware->mac) : NULL;
    device->hardware->bid =
        hardware && hardware->bid ? rc_copy_string(hardware->bid) : NULL;
    device->is_refreshing = 0;
    device->client_id = NULL;
    device->mobject = rc_mutex_create(NULL);

    rc_backoff_algorithm_init(
        &device->sbackoff, _device_regist_retry_intervals,
        sizeof(_device_regist_retry_intervals) / sizeof(int), 3600);

    LOGI(DM_TAG, "device(%p) new", device);

    return device;
}

int real_update_device_info(cJSON* input, rc_device_t* device) {
    int rc = RC_ERROR_REGIST_DEVICE;
    char *session_token = NULL, *client_id = NULL, *app_id = NULL;
    LOGI(DM_TAG, "start to BEGIN_MAPPING_JSON");
    BEGIN_MAPPING_JSON(input, root)
        int int_rc = 0;
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, int_rc)

        if (int_rc == 0) {
            rc = RC_SUCCESS;
            JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, session, session_token)
            JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, clientId, client_id)
            JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, appId, app_id)
            JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, expire, device->expire)
            JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, timeout, device->timeout)

            if (strlen(session_token) < sizeof(device->session_token) - 1) {
                strcpy(device->session_token, session_token);
            }

            if (device->client_id != NULL &&
                strcmp(device->client_id, client_id) != 0) {
                LOGE(DM_TAG, "clientId is not match, org(%s), result(%s)",
                     device->client_id, client_id);
            }

            UPDATE_DEVICE_PROPERTY(device, client_id, client_id);

            if (device->app_id != NULL && strcmp(device->app_id, app_id) != 0) {
                LOGE(DM_TAG, "appId is not match, org(%s), result(%s)",
                     device->app_id, app_id);
            }
            UPDATE_DEVICE_PROPERTY(device, app_id, app_id);
        }
    END_MAPPING_JSON(root);

    return rc;
}

int fill_device_info(char* res, rc_device_t* device) {
    int rc = 0;
    LOGI(DM_TAG, "fill_device_info to parse json response");
    BEGIN_EXTRACT_JSON(res, root)
        rc = real_update_device_info(JSON(root), device);
    END_EXTRACT_JSON(root)
    return rc;
}

char* build_regist_json(rc_device_t* device, int now, const char* signature) {
    char* json_req = NULL;
    char now_str[20] = {0};
    snprintf(now_str, sizeof(now_str) - 1, "%d", now);
    BEGIN_JSON_OBJECT(root)
        if (device->app_id != NULL) {
            JSON_OBJECT_ADD_STRING(root, appId, device->app_id);
        }
        if (device->uuid != NULL) {
            JSON_OBJECT_ADD_STRING(root, deviceId, device->uuid);
        }
        if (device->client_id != NULL) {
            JSON_OBJECT_ADD_STRING(root, clientId, device->client_id);
        }
        JSON_OBJECT_ADD_STRING(root, signature, signature)
        JSON_OBJECT_ADD_STRING(root, time, now_str)
        if (device->hardware != NULL) {
            JSON_OBJECT_ADD_OBJECT(root, hardwareInfo)
            if (device->hardware->mac != NULL) {
                JSON_OBJECT_ADD_STRING(hardwareInfo, mac,
                                       device->hardware->mac);
            }
            if (device->hardware->cpu != NULL) {
                JSON_OBJECT_ADD_STRING(hardwareInfo, cpu,
                                       device->hardware->cpu);
            }
            if (device->hardware->bid != NULL) {
                JSON_OBJECT_ADD_STRING(hardwareInfo, bid,
                                       device->hardware->bid);
            }
            END_JSON_OBJECT_ITEM(hardwareInfo);
        }
        json_req = JSON_TO_STRING(root);
    END_JSON_OBJECT(root);
    return json_req;
}

int regist_device(rc_device_t* device, int now, const char* signature) {
    int rc;
    char* json_req;
    rc_buf_t response = rc_buf_stack();
    rc_mutex_lock(device->mobject);
    json_req = build_regist_json(device, now, signature);
    rc_mutex_unlock(device->mobject);

    LOGI(DM_TAG, "json request: %s", json_req);

    // init request body
    // will free buffer after request done
    rc_buf_t rbody = rc_buf_usrdata(json_req, strlen(json_req), 1);
    rbody.length = rbody.total;
    rbody.immutable = 1;

    http_request request =
        http_request_init_url(device->manager, &device->url, HTTP_REQUEST_POST);
    if (request == NULL) {
        LOGW(DM_TAG, "create http request failed");
        return RC_ERROR_CREATE_REQUEST_FAILED;
    }

    int timeout = 3000;
    http_request_set_opt(request, HTTP_REQUEST_OPT_REQUEST_BODY, &rbody);
    http_request_set_opt(request, HTTP_REQUEST_OPT_TIMEOUT, &timeout);

    rc = http_request_execute(request);
    LOGI(DM_TAG, "http post regist device finish with %d", rc);
    if (rc == 0) {
        http_request_get_response(request, &rc, &response);

        // is just for debuger print
        rc_buf_tail_ptr(&response)[0] = '\0';
        LOGI(DM_TAG, "json rsponse: %s", rc_buf_head_ptr(&response));

        rc_mutex_lock(device->mobject);
        LOGI(DM_TAG, "before fill_device_info");
        rc = fill_device_info(rc_buf_head_ptr(&response), device);
        device->is_refreshing = 0;
        rc_mutex_unlock(device->mobject);
    } else {
        rc_mutex_lock(device->mobject);
        device->is_refreshing = 0;
        rc_mutex_unlock(device->mobject);
    }

    http_request_uninit(request);

    rc_backoff_algorithm_set_result(&device->sbackoff, rc == RC_SUCCESS);

    return rc;
}

int calc_signature_use_secret(rc_device_t* device, int now, char* signature) {
    char* hstr = build_hardware_info(device->hardware);
    {  // calc signature
        char tmp[60];
        int len = strlen(device->app_secret) + strlen(device->uuid) +
                  strlen(hstr) + 20;
        char* buf = (char*)rc_malloc(len + 1);
        snprintf(buf, len, "%s%s%s%d", device->app_secret, device->uuid, hstr,
                 now / 60);
        md5tostr(buf, strlen(buf), tmp);
        LOGI(DM_TAG, "md51=%s", tmp);

        memcpy(tmp + 32, "aproton.tech", sizeof("aproton.tech") - 1);

        md5tostr(tmp, 32 + sizeof("aproton.tech") - 1, signature);
        rc_free(buf);
    }

    if (hstr != NULL) {
        rc_free(hstr);
    }
    return 0;
}

int calc_signature_use_publickey(rc_device_t* device, int now,
                                 char* signature) {
    char* hstr = build_hardware_info(device->hardware);
    {
        char tmp[40] = {0};
        int len = strlen(device->client_id) + strlen(hstr) + 20;
        char* buf = (char*)rc_malloc(len + 1);
        int ret = 0;
        rc_buf_t encrypt = rc_buf_stack();
        rsa_crypt rsa = rc_rsa_crypt_init(device->public_key);

        snprintf(buf, len, "%s%s%d", device->client_id, hstr, now / 60);
        md5tostr(buf, strlen(buf), tmp);
        rc_free(buf);
        LOGI(DM_TAG, "md5(X+time)=%s", tmp)

        ret = rc_rsa_encrypt(rsa, tmp, 32, &encrypt);
        LOGI(DM_TAG, "%s", rc_buf_head_ptr(&encrypt));
        if (ret == 0) {
            memcpy(signature, rc_buf_head_ptr(&encrypt), encrypt.length);
        }

        rc_rsa_crypt_uninit(rsa);
    }

    if (hstr != NULL) {
        rc_free(hstr);
    }

    return 0;
}

int query_device_session_token(rc_device_t* device) {
    int rc;
    int now = time(NULL);
    LOGI(DM_TAG, "device(%p) query_device_session_token", device);
    if (device->regist_type == 0) {  // public key
        char* signature = (char*)rc_malloc(2048);
        // char signature[2048] = {0};
        rc_mutex_lock(device->mobject);
        if (device->is_refreshing) {
            LOGI(DM_TAG, "device(%p) is refreshing token", device);
            rc_free(signature);
            rc_mutex_unlock(device->mobject);
            return RC_ERROR_REGIST_DEVICE;
        }
        device->is_refreshing = 1;
        calc_signature_use_publickey(device, now, signature);
        rc_mutex_unlock(device->mobject);

        LOGI(DM_TAG, "signature=%s", signature);

        rc = regist_device(device, now, signature);
        rc_free(signature);
    } else {  // app secret
        char signature[40] = {0};
        rc_mutex_lock(device->mobject);
        if (device->is_refreshing) {
            LOGI(DM_TAG, "device(%p) is refreshing token", device);
            rc_mutex_unlock(device->mobject);
            return RC_ERROR_REGIST_DEVICE;
        }
        device->is_refreshing = 1;
        // LOGI(DM_TAG, "before calc_signature_use_secret");
        calc_signature_use_secret(device, now, signature);
        rc_mutex_unlock(device->mobject);

        LOGI(DM_TAG, "signature=%s", signature);

        rc = regist_device(device, now, signature);
    }

    return rc;
}

int rc_device_regist(aidevice dev, const http_request_url_info_t* url,
                     const char* app_id, const char* uuid,
                     const char* app_secret, int at_once) {
    DECLEAR_REAL_VALUE(rc_device_t, device, dev);
    LOGI(DM_TAG,
         "regist: device(%p), url(%s), app_id(%s), uuid(%s), app_secret(%s)",
         dev, url->host, app_id, uuid, app_secret);

    if (app_id == NULL || uuid == NULL || app_secret == NULL) {
        LOGI(DM_TAG, "regist_device_storybox input params has null");
        return RC_ERROR_INVALIDATE_INPUT;
    }

    device->url = *url;
    UPDATE_DEVICE_PROPERTY(device, app_id, app_id);
    UPDATE_DEVICE_PROPERTY(device, uuid, uuid);
    UPDATE_DEVICE_PROPERTY(device, app_secret, app_secret);

    device->regist_type = 1;
    if (at_once) {
        return query_device_session_token(device);
    }

    return RC_SUCCESS;
}

int rc_device_uninit(aidevice dev) {
    DECLEAR_REAL_VALUE(rc_device_t, device, dev);
    LOGI(DM_TAG, "device(%p) free", device);

    if (device->refresh_timer) {
        rc_timer_stop(device->refresh_timer);
    }

    if (device->app_id) rc_free(device->app_id);
    if (device->client_id) rc_free(device->client_id);
    if (device->uuid) rc_free(device->uuid);
    if (device->public_key) rc_free(device->public_key);
    if (device->url.host) rc_free(device->url.host);
    if (device->url.ip) rc_free(device->url.ip);
    if (device->url.path) rc_free(device->url.path);

    if (device->hardware) {
        rc_hardware_info* hardware = device->hardware;
        if (hardware->cpu) rc_free(hardware->cpu);
        if (hardware->mac) rc_free(hardware->mac);
        if (hardware->bid) rc_free(hardware->bid);
        rc_free(device->hardware);
    }

    rc_mutex_destroy(device->mobject);
    rc_free(device);
    return RC_SUCCESS;
}

int device_to_refresh_token(rc_timer timer, void* dev) {
    rc_device_t* device = (rc_device_t*)dev;
    if (device != NULL) {
        LOGD(DM_TAG, "device_to_refresh_token");
        if (rc_backoff_algorithm_can_retry(&device->sbackoff)) {
            LOGI(DM_TAG, "token timeout, so to reget token");
            if (query_device_session_token(device) == 0) {
                device->sbackoff.suc_retry_interval = device->timeout / 2;
                if (device->callback != NULL) {
                    device->callback(device, device->session_token,
                                     device->timeout);
                }
            }
        }
    }
    return 0;
}

int rc_device_enable_auto_refresh(aidevice dev, rc_timer_manager mgr,
                                  device_token_change callback) {
    DECLEAR_REAL_VALUE(rc_device_t, device, dev);

    if (mgr == NULL || callback == NULL) {
        LOGE(DM_TAG, "timer manager(%p) or callback(%p) is null", mgr,
             callback);
        return RC_ERROR_INVALIDATE_INPUT;
    }

    if (device->refresh_timer != NULL) {
        LOGE(DM_TAG, "had enabled auto refresh");
        return RC_ERROR_REPEAT_REFRESH;
    }

    rc_mutex_lock(device->mobject);
    device->refresh_timer = rc_timer_create(
        mgr, DM_TOKEN_REFRESH_INTERVAL * 1000, DM_TOKEN_REFRESH_INTERVAL * 1000,
        device_to_refresh_token, device);
    device->callback = callback;
    rc_mutex_unlock(device->mobject);

    return RC_SUCCESS;
}

int rc_device_refresh_atonce(aidevice dev, int async) {
    DECLEAR_REAL_VALUE(rc_device_t, device, dev);

    if (!async) {
        return query_device_session_token(device);
    }

    if (device->refresh_timer == NULL) {
        LOGE(DM_TAG, "device(%p) not enable auto refresh", device);
        return RC_ERROR_REGIST_DEVICE;
    }

    device->sbackoff.status = 0;
    device->sbackoff.index = 0;

    // refresh in next 100ms
    return rc_timer_ahead_once(device->refresh_timer, 100);
}

const char* get_device_app_id(aidevice dev) {
    return dev == NULL ? NULL : ((rc_device_t*)dev)->app_id;
}

const char* get_device_client_id(aidevice dev) {
    return dev == NULL ? NULL : ((rc_device_t*)dev)->client_id;
}

const char* get_device_session_token(aidevice dev) {
    return dev == NULL ? NULL : ((rc_device_t*)dev)->session_token;
}
