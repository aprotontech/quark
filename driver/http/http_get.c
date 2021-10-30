

#include "rc_http_request.h"
#include "rc_http_client.h"

// timeout: ms
int http_get(http_manager mgr, const char* url, const char *ipaddr, 
        const char* headers[], int head_count, int timeout, rc_buf_t* response)
{
    int rc = 0, i;
    http_request request = NULL;

    if (response == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    request = http_request_init(mgr, url, ipaddr, HTTP_REQUEST_GET);
    if (request == NULL) {
        return RC_ERROR_CREATE_REQUEST_FAILED;
    }

    http_request_set_opt(request, HTTP_REQUEST_OPT_TIMEOUT, &timeout);

    for (i = 0; i < head_count; ++ i) {
        if (headers[i] != NULL) {
            rc_buf_t* head = rc_buf_init(strlen(headers[i]));
            strcpy(get_buf_ptr(head), headers[i]);
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
        char* p = (char*)rc_malloc(response->length);
        memcpy(p, response->usr_buf, response->length);
        response->usr_buf = p;
        response->free = 1;
    }

    http_request_uninit(request);
    return rc;
}

