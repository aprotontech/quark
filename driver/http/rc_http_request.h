/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_request.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-12 23:27:02
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_HTTP_REQUEST_H_
#define _QUARK_HTTP_REQUEST_H_

#include "rc_buf.h"

typedef void* http_request;
typedef void* http_manager;

/////// HTTP REQUEST
enum {
    HTTP_REQUEST_OPT_TIMEOUT = 0,
    HTTP_REQUEST_OPT_USER_DATA,
    HTTP_REQUEST_OPT_HEADER,
    HTTP_REQUEST_OPT_METHOD,
    HTTP_REQUEST_OPT_TYPE,
    HTTP_REQUEST_OPT_REQUEST_BODY,
    HTTP_REQUEST_OPT_RES_BODY_CALLBACK,
    HTTP_REQUEST_OPT_RES_CHUNK_CALLBACK,
    HTTP_REQUEST_OPT_RES_BUFFER_SIZE,
    HTTP_REQUEST_OPT_RES_HEADERS,
};

enum {
    HTTP_REQUEST_TYPE_NORMAL = 0,
    HTTP_REQUEST_TYPE_CHUNK,

    HTTP_RESPONSE_TYPE_NORMAL,
    HTTP_RESPONSE_TYPE_CHUNK,
};

enum {
    HTTP_REQUEST_GET = 1,
    HTTP_REQUEST_POST = 3,
};

typedef struct _http_request_url_info_t {
    char* host;
    char* path;
    char* ip;
    int schema;  // 0-http, 1-https
    int port;
    // struct sockaddr_in addr;
} http_request_url_info_t;

struct http_parser_url;

typedef int (*http_body_callback)(http_request request, int status_code,
                                  const char* body, int len);

///////////////// open api
http_request http_request_init(http_manager mgr, const char* url,
                               const char* ipaddr, int method);

http_request http_request_init_url(http_manager mgr,
                                   http_request_url_info_t* info, int method);

http_request http_request_init_raw(http_manager mgr, int is_https,
                                   const char* host, int host_len, int port,
                                   const char* path, int path_len, int method,
                                   const char* ipaddr);

int http_url_parse(const char* raw_url, struct http_parser_url* url,
                   int* schema);

int http_request_set_opt(http_request request, int type, void* opt);

int http_request_execute(http_request request);
int http_request_async_execute(http_request request,
                               http_body_callback callback, int type);
int http_request_async_send(http_request request, const char* body, int len);

void* http_request_get_data(http_request request);

int http_request_get_response(http_request request, int* status_code,
                              rc_buf_t* buf);

// get raw response
int http_request_get_raw_response(http_request request, int* status_code,
                                  list_link_t** body_head_buf);

int http_request_uninit(http_request request);

// result is http status code
int http_post(http_manager mgr, const char* url, const char* ipaddr,
              char const* headers[], int head_count, const char* body,
              int length, int timeout, rc_buf_t* response);

int http_get(http_manager mgr, const char* url, const char* ipaddr,
             const char* headers[], int head_count, int timeout,
             rc_buf_t* response);

#endif
