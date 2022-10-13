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

#define API_URL_LENGTH 120

char* newReqId(char buf[]);
int need_refresh_token(const char* body);

int _get_http_service_url(rc_runtime_t* env, char url[API_URL_LENGTH],
                          char ip[20], const char* service_name,
                          const char* path) {
    rc_service_protocol_info_t info;

    if (rc_service_query(env->ansmgr, service_name, "http", &info) == 0) {
        strcpy(ip, info.ip);
        strcat(url, info.host);
        if (info.port != 80) {
            snprintf(url, API_URL_LENGTH, "http://%s:%d%s%s", info.host,
                     info.port, info.prefix, path);
        } else {
            snprintf(url, API_URL_LENGTH, "http://%s%s%s", info.host,
                     info.prefix, path);
        }

        return 0;
    }

    return -1;
}

int rc_http_quark_post(const char* service_name, const char* path,
                       const char* body, int timeout, rc_buf_t* response) {
    int rc, offset;
    char reqId[17] = {0};
    char header[60] = {0};
    char url[API_URL_LENGTH] = {0};
    char ip[20] = {0};
    char const* p[1] = {header};

    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->httpmgr == NULL) {
        LOGI(SDK_TAG, "env is not inited");
        return RC_ERROR_SDK_INIT;
    }

    LOGI(SDK_TAG, "rc_http_quark_post(%s)(%s)", service_name, path);

    if (_get_http_service_url(env, url, ip, service_name, path) != 0) {
        LOGW(SDK_TAG, "query service(%s) info failed", service_name);
        return -1;
    }

    LOGI(SDK_TAG, "url(%s) --> ip(%s)", url, ip);

    snprintf(header, sizeof(header), "IOT-DEVICE-SESSION: %s",
             get_device_session_token(env->device));

    offset = strlen(url);
    snprintf(&url[offset], sizeof(url) - offset, "%csdkVersion=%s&reqId=%s",
             strchr(path, '?') != NULL ? '&' : '?', rc_sdk_version(),
             newReqId(reqId));

    LOGI(SDK_TAG, "http request body(%s)", body);
    rc = http_post(env->httpmgr, url, ip, p, 1, body, strlen(body), timeout,
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
             env->settings.service_url, path,
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
    int i, rc;
    int maybe = 0, refresh = 0;
    for (i = 0; i < n; ++i) {
        if (strstr(body, errors[i]) != NULL) {
            maybe = 1;
            break;
        }
    }

    if (!maybe) {
        return 0;
    }

    BEGIN_EXTRACT_JSON(body, root)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, rc)

        for (i = 0; i < n; ++i) {
            if (rc == atoi(errors[i])) {
                refresh = 1;
                break;
            }
        }
    END_EXTRACT_JSON(root)

    return refresh;
}
