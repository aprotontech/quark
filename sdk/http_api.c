/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     httpapi.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-25 01:44:28
 * @version  0
 * @brief
 *
 **/

#include "env.h"
#include "logs.h"
#include "rc_http_request.h"

char* newReqId(char buf[]);
int need_refresh_token(const char* body);

int rc_http_quark_post(const char* service_name, const char* path,
                       const char* body, int timeout, rc_buf_t* response) {
    int rc;
    char reqId[17] = {0};
    char header[60] = {0};
    char url[120] = {0};
    char const* p[1] = {header};

    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->httpmgr == NULL) {
        LOGI(SDK_TAG, "env is not inited");
        return RC_ERROR_SDK_INIT;
    }

    snprintf(header, sizeof(header), "IOT-DEVICE-SESSION: %s",
             get_device_session_token(env->device));
    snprintf(url, sizeof(url), "%s%s%csdkVersion=%s&reqId=%s",
             env->local.default_service_url, path,
             strchr(path, '?') != NULL ? '&' : '?', rc_sdk_version(),
             newReqId(reqId));

    LOGI("SDK_TAG", "http request body(%s)", body);
    rc = http_post(env->httpmgr, url, NULL, p, 1, body, strlen(body), timeout,
                   response);
    if (rc == 200) {
        if (need_refresh_token(rc_buf_head_ptr(response))) {
            rc_device_refresh_atonce(env->device, 1);
        }
    }

    return rc;
}

http_request rc_http_quark_request(const char* service_name, const char* path,
                                   int timeout, http_body_callback callback,
                                   int type) {
    int rc;
    char reqId[17] = {0};
    char url[120] = {0};
    http_request request = NULL;
    rc_buf_t* head_buf = NULL;

    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->httpmgr == NULL) {
        LOGI(SDK_TAG, "env is not inited");
        return NULL;
    }

    snprintf(url, sizeof(url), "%s%s%csdkVersion=%s&reqId=%s",
             env->local.default_service_url, path,
             strchr(path, '?') != NULL ? '&' : '?', rc_sdk_version(),
             newReqId(reqId));

    request = http_request_init(env->httpmgr, url, NULL, HTTP_REQUEST_POST);
    if (request == NULL) {
        return NULL;
    }

    head_buf = rc_buf_init(60);
    snprintf(rc_buf_head_ptr(head_buf), head_buf->total,
             "IOT-DEVICE-SESSION: %s", get_device_session_token(env->device));

    http_request_set_opt(request, HTTP_REQUEST_OPT_TIMEOUT, &timeout);
    http_request_set_opt(request, HTTP_REQUEST_OPT_HEADER, head_buf);
    rc = http_request_async_execute(request, callback, type);
    if (rc != 0) {
        http_request_uninit(request);
        return NULL;
    }

    return request;
}

int rc_http_quark_post_body(http_request request, const char* body, int len) {
    return http_request_async_send(request, body, len);
}

int rc_http_quark_close(http_request request) {
    return http_request_uninit(request);
}

void* rc_http_request_get_data(http_request request) {
    return http_request_get_data(request);
}

int rc_http_request_set_data(http_request request, void* data) {
    return http_request_set_opt(request, HTTP_REQUEST_OPT_USER_DATA, data);
}

char* newReqId(char buf[]) {
    int i;
    char t[] =
        "ABCDEFEGHIJKLMNOPQRSTUVWXYZAbcdefeghijklmnopqrstuvwxyz0123456789";
    int n = sizeof(t) - 1;
    for (i = 0; i < 16; ++i) {
        buf[i] = t[rand() % n];
    }
    buf[16] = '\0';
    return buf;
}

int get_rcode(cJSON* input, char** str_rc) {
    BEGIN_MAPPING_JSON(input, root)
    JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, rc, *str_rc)
    END_MAPPING_JSON(str_rc)
    return RC_SUCCESS;
}

int need_refresh_token(const char* body) {
    char* errors[] = {"1414", "1415"};
    int n = sizeof(errors) / sizeof(char*);
    int i;
    int maybe = 0, refresh = 0;
    for (i = 0; i < n; ++i) {
        if (strstr(body, errors[i]) != NULL) {
            maybe = 1;
            break;
        }
    }

    if (maybe) {
        char* str_rc = NULL;

        BEGIN_EXTRACT_JSON(body, root)
        if (get_rcode(JSON(root), &str_rc) == RC_SUCCESS) {
            for (i = 0; i < n; ++i) {
                if (strcmp(str_rc, errors[i]) == 0) {
                    refresh = 1;
                    break;
                }
            }
        }
        END_EXTRACT_JSON(root)
    }

    return refresh;
}
