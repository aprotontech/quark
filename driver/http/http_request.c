/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_request.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-13 00:28:47
 * @version  0
 * @brief
 *
 **/

#include "http_parser.h"
#include "logs.h"
#include "rc_event.h"
#include "rc_http_client.h"
#include "rc_http_manager.h"
#include "rc_http_request.h"
#include "rc_mutex.h"
#include "rc_thread.h"

#define RC_TAG "[Request]"
#define MAX_RESPONSE_SIZE 409600
#ifdef __QUARK_RTTHREAD__
#define RC_HTTP_RECV_INIT_BUF_SIZE 510
#else
#define RC_HTTP_RECV_INIT_BUF_SIZE 4090
#endif

#define RC_DEFAULT_USER_AGENT "RC-CLIENT(1.0)"

typedef struct _async_request_thread {
    rc_thread reqthread;

    int exit_thread;
    int can_send;
    rc_event send_event;
    rc_mutex mobject;
} async_request_thread;

typedef struct _rc_http_request_t {
    http_client client;
    http_manager manager;

    http_parser parser;
    http_parser_settings settings;

    struct timeval timeout;

    rc_buf_t* buf;
    int buf_size;

    void* user_data;

    // response data
    int status_code;
    list_link_t res_body;
    int total_res_size;

    http_body_callback callback;

    async_request_thread* thread;

    char stype;  // response type: normal, chunk

    // request data
    char finished;

    char rtype;  // request type: normal, chunk
    // has custome user agent header
    char custom_user_agent_header;

    int cur_chunck_size;

    int method;

    int schema;

    list_link_t headers;
    list_link_t body;
    list_link_t* response_headers;

    char* host;
    char* path;
} rc_http_request_t;

int init_http_parser(rc_http_request_t* request);
int http_request_build_header(rc_http_request_t* request);
extern struct timeval* get_tm_diff(const struct timeval* timeout,
                                   struct timeval* result);

http_request http_request_init(http_manager mgr, const char* raw_url,
                               const char* ipaddr, int method) {
    const char* tmp;
    char buf[10] = {0};
    int port;
    int ret = 0;
    int len = 0;
    struct http_parser_url url;
    ret = http_parser_parse_url(raw_url, strlen(raw_url), 0, &url);
    if (ret != 0) {
        LOGI(RC_TAG, "http_parser_parse_url(%s) failed,ret(%d)", raw_url, ret);
        return NULL;
    }

    if (!(url.field_set & (1 << UF_HOST))) {
        LOGI(RC_TAG, "http_parser_parse_url(%s) failed, not found host",
             raw_url);
        return NULL;
    }

    if (url.field_set & (1 << UF_PORT)) {
        if (url.field_data[UF_PORT].len >= sizeof(buf) - 1) {
            LOGI(RC_TAG, "http_parser_parse_url(%s) failed, port is invalidate",
                 raw_url);
            return NULL;
        }
        memcpy(buf, raw_url + url.field_data[UF_PORT].off,
               url.field_data[UF_PORT].len);
        port = atoi(buf);
    } else if (url.field_set & (1 << UF_SCHEMA)) {
        if (url.field_data[UF_SCHEMA].len >= sizeof(buf) - 1) {
            LOGI(RC_TAG,
                 "http_parser_parse_url(%s) failed, schema is invalidate",
                 raw_url);
            return NULL;
        }
        memcpy(buf, raw_url + url.field_data[UF_SCHEMA].off,
               url.field_data[UF_SCHEMA].len);
        for (ret = 0; buf[ret] != '\0'; ++ret) {
            if (buf[ret] >= 'A' && buf[ret] <= 'Z') {
                buf[ret] += 32;  // to lower
            }
        }

        port = strcmp(buf, "https") == 0 ? 443 : 80;
    }

    tmp = strchr(raw_url + url.field_data[UF_HOST].off, '/');
    len = sizeof(rc_http_request_t) + url.field_data[UF_HOST].len + 1 +
          (tmp == NULL ? 1 : strlen(tmp)) + 1;
    rc_http_request_t* request = (rc_http_request_t*)rc_malloc(len);
    memset(request, 0, len);
    request->host = ((char*)request) + sizeof(rc_http_request_t);
    request->path = request->host + url.field_data[UF_HOST].len + 1;

    memcpy(request->host, raw_url + url.field_data[UF_HOST].off,
           url.field_data[UF_HOST].len);
    if (tmp != NULL) {
        memcpy(request->path, tmp, strlen(tmp));
    } else {
        request->path[0] = '/';
    }

    request->buf_size =
        RC_HTTP_RECV_INIT_BUF_SIZE - sizeof(rc_buf_t);  // default buf size
    request->manager = mgr;
    request->stype = HTTP_RESPONSE_TYPE_NORMAL;
    request->client = http_manager_get_client(mgr, request->host, ipaddr, port);
    if (request->client == NULL) {
        rc_free(request);
        return NULL;
    }

    LOGI(RC_TAG, "request to (%s:%d%s)", request->host, port, request->path);

    request->method = method;
    gettimeofday(&request->timeout, NULL);
    request->timeout.tv_sec += 5;  // default timeout
    LL_init(&request->body);
    LL_init(&request->headers);
    LL_init(&request->res_body);
    init_http_parser(request);
    // LOGI(RC_TAG, "new http request(%p)", request);
    return request;
}

int http_request_finish(http_request _request, int status_code,
                        const char* body) {
    rc_http_request_t* request = (rc_http_request_t*)_request;
    if (request == NULL)
        return 0;
    else if (request->finished)
        return 0;

    return 0;
}

int str_prefix_case_compare(const char* prefix, const char* content) {
    while (*prefix != '\0' && tolower(*prefix) == tolower(*content)) {
        ++prefix;
        ++content;
    }

    if (*prefix == '\0')
        return 0;
    else if (*content == '\0')
        return -1;
    return 1;
}

int http_request_set_opt(http_request req, int type, void* opt) {
    DECLEAR_REAL_VALUE(rc_http_request_t, request, req);

    switch (type) {
    case HTTP_REQUEST_OPT_TIMEOUT:
        if (opt != NULL) {
            int t = *(int*)opt;
            struct timeval* tv = &request->timeout;
            gettimeofday(tv, NULL);
            t = t * 1000 + tv->tv_usec;
            tv->tv_usec = t % 1000000;
            tv->tv_sec += t / 1000000;
        }
        break;
    case HTTP_REQUEST_OPT_USER_DATA: request->user_data = opt; break;
    case HTTP_REQUEST_OPT_HEADER:
        if (opt != NULL) {
            LL_insert(&((rc_buf_t*)opt)->link, request->headers.prev);
            if (str_prefix_case_compare("User-Agent:",
                                        rc_buf_head_ptr((rc_buf_t*)opt)) == 0) {
                request->custom_user_agent_header = 1;
            }
        }
        break;
    case HTTP_REQUEST_OPT_METHOD:
        if (opt != NULL) {
            request->method = *(int*)opt;
        }
        break;
    case HTTP_REQUEST_OPT_REQUEST_BODY:
        if (opt != NULL) {
            LL_insert(&((rc_buf_t*)opt)->link, request->body.prev);
        }
        break;
    case HTTP_REQUEST_OPT_RES_CHUNK_CALLBACK:
        request->stype = HTTP_RESPONSE_TYPE_CHUNK;
        request->callback = (http_body_callback)opt;
        break;
    case HTTP_REQUEST_OPT_RES_BODY_CALLBACK:
        request->stype = HTTP_RESPONSE_TYPE_NORMAL;
        request->callback = (http_body_callback)opt;
        break;
    case HTTP_REQUEST_OPT_RES_BUFFER_SIZE:
        if (opt != NULL) {
            if (*(int*)opt > 0 && *(int*)opt <= 1024 * 1024) {
                request->buf_size = *(int*)opt;
            } else {
                LOGI(RC_TAG,
                     "input buf size invalidate, must 0 < bufsize <= 1MB");
                return -1;
            }
        }
        break;
    case HTTP_REQUEST_OPT_RES_HEADERS:
        request->response_headers = (list_link_t*)opt;
        break;
    }

    return 0;
}

int http_request_on_recv(http_request _request, char* buf, size_t len) {
    size_t r = 0;
    rc_http_request_t* request = (rc_http_request_t*)_request;
    if (isprint(buf[0])) {  // skip un-print body
        LOGI(RC_TAG, "response: %s", buf);
    }

    r = http_parser_execute(&request->parser, &request->settings, buf, len);

    if (request->parser.http_errno) {
        LOGI(RC_TAG, "http_parser_execute failed with(%s:%s)",
             http_errno_name(request->parser.http_errno),
             http_errno_description(request->parser.http_errno));
        return -1;
    }

    return 0;
}

int http_request_conn_send_header(rc_http_request_t* request) {
    if (request->buf == NULL) {
        request->buf = rc_buf_init(request->buf_size);
        if (request->buf == NULL) {
            LOGI(RC_TAG, "http request alloc buf failed");
            return -1;
        }
    }

    if (http_client_connect(request->client, request->schema,
                            &request->timeout) != 0) {
        LOGI(RC_TAG, "http client connect failed");
        return -1;
    }

    if (http_request_build_header(request) != 0) {
        LOGI(RC_TAG, "build http headers failed");
        return -1;
    }

    LOGI(RC_TAG, "header: %s", rc_buf_head_ptr(request->buf));

    if (http_client_send(request->client, rc_buf_head_ptr(request->buf),
                         request->buf->length, &request->timeout) != 0) {
        LOGI(RC_TAG, "send headers to http client failed");
        return -1;
    }

    LOGI(RC_TAG, "request(%p) all head data had sended", request);
    return RC_SUCCESS;
}

int http_request_recv_response(rc_http_request_t* request) {
    int len;
    do {
        RC_BUF_CLEAN(request->buf);
        memset(rc_buf_head_ptr(request->buf), 0,
               RC_BUF_LEFT_SIZE(request->buf));
        len =
            http_client_recv(request->client, rc_buf_head_ptr(request->buf),
                             RC_BUF_LEFT_SIZE(request->buf), &request->timeout);
        if (len <= 0) {  // recv eof or timeout
            LOGI(RC_TAG, "request(%p) recv eof or timeout, ret(%d)", request,
                 len);
            return RC_ERROR_HTTP_RECV;
        }

        if (RC_BUF_LEFT_SIZE(request->buf) > 0) {
            // this is just for debuger printer, real response is not need
            // ending with '\0' the if sentence can be removed
            rc_buf_head_ptr(request->buf)[len] = '\0';
        }
        if (http_request_on_recv(request, rc_buf_head_ptr(request->buf), len) !=
            0) {
            return RC_ERROR_HTTP_RECV;
        }

    } while (!request->finished &&
             (request->thread == NULL || !request->thread->exit_thread));

    return RC_SUCCESS;
}

int http_request_body_callback(rc_http_request_t* request) {
    if (request->callback != NULL &&
        request->stype == HTTP_RESPONSE_TYPE_NORMAL) {
        rc_buf_t buf = rc_buf_stack();
        http_request_get_response(request, NULL, &buf);
        request->callback(request, request->status_code, rc_buf_head_ptr(&buf),
                          buf.length);
        rc_buf_free(&buf);
    }
    return RC_SUCCESS;
}

int http_request_execute(http_request req) {
    int rc;
    list_link_t* p = NULL;
    rc_buf_t* buf = NULL;
    DECLEAR_REAL_VALUE(rc_http_request_t, request, req);
    if (request->thread != NULL) {
        LOGW(RC_TAG, "request(%p) had init async thread", request);
        return RC_ERROR_EXECUTE_REQUEST_FAILED;
    }

    request->rtype = HTTP_REQUEST_TYPE_NORMAL;
    // request->stype = HTTP_RESPONSE_TYPE_NORMAL;

    rc = http_request_conn_send_header(request);
    if (rc != 0) {
        return rc;
    }

    p = request->body.next;
    while (p != &request->body) {
        buf = (rc_buf_t*)p;
        LOGI(RC_TAG, "http(%p) send body-length(%d)", request, buf->length);
        if (http_client_send(request->client, rc_buf_head_ptr(buf), buf->length,
                             &request->timeout) != 0) {
            LOGW(RC_TAG, "send body to http client failed");
            return -1;
        }

        p = p->next;
    }

    rc = http_request_recv_response(request);
    LOGI(RC_TAG, "request(%p) recv response ret=%d", request, rc);

    return http_request_body_callback(request);
}

void* async_request_thread_func(void* req) {
    int rc;
    rc_http_request_t* request = (rc_http_request_t*)req;
    if (request == NULL || request->thread == NULL) return NULL;
    LOGI(RC_TAG, "request(%p), async_request_thread start", request);

    rc = http_request_conn_send_header(request);

    rc_mutex_lock(request->thread->mobject);
    request->thread->can_send = 1;
    rc_mutex_unlock(request->thread->mobject);
    rc_event_signal(request->thread->send_event);

    if (rc == 0) {
        rc = http_request_recv_response(request);
        http_request_body_callback(request);
    }

    LOGI(RC_TAG, "request(%p), async_request_thread stop", request);
    return NULL;
}

int http_request_async_execute(http_request req, http_body_callback callback,
                               int type) {
    DECLEAR_REAL_VALUE(rc_http_request_t, request, req);
    if (request->thread != NULL) {
        LOGW(RC_TAG, "request(%p) had init async thread", request);
        return RC_ERROR_EXECUTE_REQUEST_FAILED;
    }

    request->rtype = HTTP_REQUEST_TYPE_CHUNK;
    request->stype = type;
    request->callback = callback;

    request->thread =
        (async_request_thread*)rc_malloc(sizeof(async_request_thread));

    request->thread->can_send = 0;
    request->thread->exit_thread = 0;
    request->thread->mobject = rc_mutex_create(NULL);
    request->thread->send_event = rc_event_init();

    rc_thread_context_t ctx = {.joinable = 1,
                               .name = "http-executor",
                               .priority = -1,
                               .stack_size = 4096};

    request->thread->reqthread =
        rc_thread_create(async_request_thread_func, request, &ctx);

    return RC_SUCCESS;
}

int http_request_async_send(http_request req, const char* body, int len) {
    int rc, i;
    int can_send, timeout;
    struct timeval tm;
    char hex[20] = {0};
    char* p = &hex[1];
    DECLEAR_REAL_VALUE(rc_http_request_t, request, req);
    if (request->thread == NULL) {
        LOGW(RC_TAG, "request(%p) aync thread is not start", request);
        return RC_ERROR_EXECUTE_REQUEST_FAILED;
    }

    if (!request->thread->can_send) {
        get_tm_diff(&request->timeout, &tm);
        timeout = tm.tv_sec * 1000 + tm.tv_usec / 1000;
        rc_event_wait(request->thread->send_event, timeout);
        if (!request->thread->can_send) {
            LOGW(RC_TAG, "request(%p) wait send timeout", request);
            return RC_ERROR_EXECUTE_REQUEST_FAILED;
        }
    }

    if (snprintf(p, sizeof(hex) - 1, "%x\r\n", len) % 2 != 0) {
        p = hex;
        *p = '0';
    }
    rc = http_client_send(request->client, p, strlen(p), &request->timeout);
    if (rc != 0) {
        LOGW(RC_TAG, "request(%p) send chunk hex failed", request);
        return rc;
    }
    rc = http_client_send(request->client, body, len, &request->timeout);
    if (rc != 0) {
        LOGW(RC_TAG, "request(%p) send real body failed", request);
        return rc;
    }
    rc = http_client_send(request->client, "\r\n", 2, &request->timeout);
    if (rc != 0) {
        LOGW(RC_TAG, "request(%p) send \\r\\n failed", request);
        return rc;
    }

    LOGD(RC_TAG, "request(%p) send chunk body size=%d done", request, len);
    return rc;
}

int http_request_uninit(http_request _request) {
    rc_http_request_t* request = (rc_http_request_t*)_request;
    list_link_t* p = NULL;
    if (request == NULL) return 0;

    if (request->buf) {
        rc_free(request->buf);
        request->buf = NULL;
    }

    if (request->client) {
        http_manager_free_client(request->manager, request->client, 1);
        request->client = NULL;
    }

    p = request->body.next;
    while (p != &request->body) {
        rc_buf_t* buf = (rc_buf_t*)p;
        p = p->next;
        rc_buf_free(buf);
    }

    LL_init(&request->body);

    p = request->headers.next;
    while (p != &request->headers) {
        rc_buf_t* buf = (rc_buf_t*)p;
        p = p->next;
        rc_buf_free(buf);
    }
    LL_init(&request->headers);

    if (request->thread) {
        request->thread->exit_thread = 1;
        rc_thread_join(request->thread->reqthread);

        rc_mutex_destroy(request->thread->mobject);
        rc_event_uninit(request->thread->send_event);
    }

    // release resbody
    p = &request->res_body;
    while (p != &request->res_body) {
        rc_buf_t* buf = (rc_buf_t*)p;
        p = p->next;
        rc_buf_free(buf);
    }

    LL_init(&request->res_body);

    rc_free(request);
    LOGI(RC_TAG, "free http request(%p)", request);
    return 0;
}

void* http_request_get_data(http_request req) {
    rc_http_request_t* request = (rc_http_request_t*)req;
    if (request != NULL) return request->user_data;
    return NULL;
}

int on_chunk_header(http_parser* p) { return 0; }

int on_chunk_complete(http_parser* p) { return 0; }

int on_headers_complete(http_parser* p) {
    rc_http_request_t* request = (rc_http_request_t*)p->data;
    request->status_code = p->status_code;

    return 0;
}

int on_message_begin(http_parser* p) { return 0; }

int on_message_complete(http_parser* p) {
    rc_http_request_t* request = (rc_http_request_t*)p->data;

    request->finished = 1;
    return 0;
}

int on_url_cb(http_parser* p, const char* at, size_t len) {}

int header_field_cb(http_parser* p, const char* at, size_t len) { return 0; }

int header_value_cb(http_parser* p, const char* at, size_t len) { return 0; }

int on_body_cb(http_parser* p, const char* at, size_t len) {
    rc_http_request_t* request = (rc_http_request_t*)p->data;
    if (request->stype == HTTP_RESPONSE_TYPE_NORMAL) {
        if (request->total_res_size + len > MAX_RESPONSE_SIZE) {
            len = MAX_RESPONSE_SIZE - request->total_res_size;
            LOGW(RC_TAG, "request(%p) response body max than max_size(%d)",
                 request, MAX_RESPONSE_SIZE);
        }
        if (len > 0) {
            request->total_res_size += len;
            rc_buf_t* buf = rc_buf_init(len);
            if (buf == NULL) {
                LOGW(RC_TAG, "request(%p) malloc buffer(%d) failed", request,
                     (int)len);
            } else {
                LL_insert(&buf->link, request->res_body.prev);
                memcpy(rc_buf_head_ptr(buf), at, len);
                buf->length = len;
            }
        }
    } else if (request->stype == HTTP_RESPONSE_TYPE_CHUNK) {  // got chunk body
        if (request->callback != NULL) {
            request->callback(request, request->status_code, (char*)at, len);
        }
    }

    return 0;
}

int init_http_parser(rc_http_request_t* request) {
    http_parser_init(&request->parser, HTTP_RESPONSE);
    request->parser.data = request;
    request->parser.http_errno = 0;

    /* setup callback */
    request->settings.on_message_begin = on_message_begin;
    request->settings.on_header_field = header_field_cb;
    request->settings.on_header_value = header_value_cb;
    request->settings.on_url = on_url_cb;
    request->settings.on_body = on_body_cb;
    request->settings.on_headers_complete = on_headers_complete;
    request->settings.on_message_complete = on_message_complete;
    return 0;
}

#define HTTP_WRITE_BUF(format, arg...)     \
    r = snprintf(buf, len, format, ##arg); \
    if (r < 0) return -1;                  \
    len -= r;                              \
    buf += r;

int http_request_build_header(rc_http_request_t* request) {
    char* buf = rc_buf_head_ptr(request->buf);
    size_t len = request->buf->total - request->buf->length;
    int r = 0;
    list_link_t* p = NULL;
    rc_buf_t* rbuf = NULL;
    HTTP_WRITE_BUF("%s %s HTTP/1.1\r\n", http_method_str(request->method),
                   request->path);
    HTTP_WRITE_BUF("Host: %s\r\n", request->host);
    if (!request->custom_user_agent_header) {
        HTTP_WRITE_BUF("User-Agent: %s\r\n", RC_DEFAULT_USER_AGENT);
    }
    HTTP_WRITE_BUF("Accept: */*\r\n");

    p = request->headers.next;
    while (p != &request->headers) {
        rbuf = (rc_buf_t*)p;
        HTTP_WRITE_BUF("%s\r\n", rc_buf_head_ptr(rbuf));
        p = p->next;
    }

    if (request->method != HTTP_REQUEST_GET) {  // get not send Content-Length
        HTTP_WRITE_BUF("Connection: keep-alive\r\n");
        if (request->rtype == HTTP_REQUEST_TYPE_NORMAL) {
            int total = 0;
            p = request->body.next;
            rbuf = NULL;
            while (p != &request->body) {
                rbuf = (rc_buf_t*)p;
                total += rbuf->length;
                p = p->next;
            }
            HTTP_WRITE_BUF("Content-Length: %d\r\n", total);
        } else if (request->rtype == HTTP_REQUEST_TYPE_CHUNK) {
            HTTP_WRITE_BUF("Transfer-Encoding: chunked\r\n");
        }
    }
    HTTP_WRITE_BUF("\r\n");

    request->buf->length += buf - rc_buf_head_ptr(request->buf);

    return 0;
}

int http_request_get_raw_response(http_request _request, int* status_code,
                                  list_link_t** body_head_buf) {
    rc_http_request_t* request = (rc_http_request_t*)_request;
    if (status_code != NULL) {
        *status_code = request->status_code;
    }

    if (body_head_buf != NULL) {
        *body_head_buf = &request->res_body;
    }

    return 0;
}

int http_request_get_response(http_request _request, int* status_code,
                              rc_buf_t* obuf) {
    rc_http_request_t* request = (rc_http_request_t*)_request;
    if (status_code != NULL) {
        *status_code = request->status_code;
    }

    if (obuf != NULL) {
        if (LL_isspin(&request->res_body)) {  // not found any response
            obuf->usr_buf = NULL;
            obuf->free = 0;
            obuf->total = sizeof(obuf->buf);
        } else if (request->res_body.next->next ==
                   &request->res_body) {  // only has one buffer
            rc_buf_t* buf = (rc_buf_t*)request->res_body.next;
            obuf->total = buf->total;
            obuf->length = buf->length;
            obuf->free = 0;
            obuf->usr_buf = rc_buf_head_ptr(buf);
        } else {  // response is too long, has more than two buffer
            int total = 0;
            list_link_t* p = request->res_body.next;
            while (p != &request->res_body) {
                rc_buf_t* rbuf = (rc_buf_t*)p;
                total += rbuf->length;
                p = p->next;
            }

            obuf->length = total;
            obuf->total = total + 1;  // +1 is for '\0'(real response is not
                                      // need it, just for debuger print)
            obuf->free = 1;
            obuf->usr_buf = rc_malloc(obuf->total);
            LOGI("[REQUEST]", "total memory(%d)", total);
            if (obuf->usr_buf == NULL) {
                LOGW("[REQUEST]", "alloc output buffer failed");
                *obuf = rc_buf_stack();
                return -1;
            }

            char* ptr = obuf->usr_buf;
            p = request->res_body.next;
            while (p != &request->res_body) {
                rc_buf_t* rbuf = (rc_buf_t*)p;
                memcpy(ptr, rc_buf_head_ptr(rbuf), rbuf->length);
                LOGI("[REQUEST]", "copy memory(%d)", rbuf->length);

                ptr += rbuf->length;
                p = p->next;
            }
        }
    }

    if (RC_BUF_LEFT_SIZE(obuf) > 0) {
        // this is just for debuger printer, real response is not need ending
        // with '\0' the if sentence can be removed
        rc_buf_tail_ptr(obuf)[0] = '\0';
    }
    return 0;
}
