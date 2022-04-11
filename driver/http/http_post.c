/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_post.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 13:48:48
 * @version  0
 * @brief
 *
 **/

#include "rc_http_request.h"
#include "rc_http_client.h"

// timeout: ms
int http_post(http_manager mgr, const char* url, const char *ipaddr, const char* headers[], int head_count, 
        const char* body, int length, int timeout, rc_buf_t* response)
{
    int rc = 0, i;
    http_request request = NULL;
    rc_buf_t rbody = rc_buf_stack();

    if (response == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    request = http_request_init(mgr, url, ipaddr, HTTP_REQUEST_POST);
    if (request == NULL) {
        return RC_ERROR_CREATE_REQUEST_FAILED;
    }

    // init request body
    rbody.length = length;
    rbody.total = length;
    rbody.usr_buf = (char*)body;
    rbody.free = 0;
    rbody.immutable = 1;

    http_request_set_opt(request, HTTP_REQUEST_OPT_REQUEST_BODY, &rbody);
    http_request_set_opt(request, HTTP_REQUEST_OPT_TIMEOUT, &timeout);

    for (i = 0; i < head_count; ++ i) {
        if (headers[i] != NULL) {
            rc_buf_t* head = rc_buf_init(strlen(headers[i]));
            strcpy(rc_buf_head_ptr(head), headers[i]);
            http_request_set_opt(request, HTTP_REQUEST_OPT_HEADER, head);
        }
    }
    rc = http_request_execute(request);
    if (rc != 0) {
        http_request_uninit(request);
        return RC_ERROR_EXECUTE_REQUEST_FAILED;
    }

    http_request_get_response(request, &rc, response);
    if (!response->free && response->length) {
        char* p = (char*)rc_malloc(response->length + 1);
        memcpy(p, response->usr_buf, response->length);
        response->usr_buf = p;
        response->free = 1;
        rc_buf_tail_ptr(response)[0] = '\0'; // is just for debuger print
    }

    http_request_uninit(request);
    return rc;
}

