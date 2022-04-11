/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     download.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-11-20 04:45:51
 *
 */

#include "env.h"
#include "rc_buf_queue.h"
#include "rc_thread.h"

typedef struct _rc_http_downloader {
    rc_download_type_t type;
    union {
        on_download_callback calllback;
        rc_buf_queue* queue;
        void* data;
    };

    http_request request;
    rc_thread thread;
    int recved_bytes;
    int total_bytes;

    mstime_t timeout_at_ms;

    char download_finished;

} rc_http_downloader;

int on_download_content(http_request request, int status_code, const char* body,
                        int len) {
    LOGI(SDK_TAG, "on_download_content: %d", len);
    rc_http_downloader* downloader =
        (rc_http_downloader*)http_request_get_data(request);

    switch (downloader->type) {
    case DOWNLOAD_TO_BUF_QUEUE: {
        int offset = 0;
        while (offset < len) {
            int rc = rc_buf_queue_push(downloader->queue, body + offset,
                                       len - offset, 1000);
            LOGI(SDK_TAG, "downloader(%p) push %d to queue", downloader, rc);
            if (rc > 0) {
                offset += rc;
            }

            if (rc_get_mstick() >= downloader->timeout_at_ms) {
                LOGI(SDK_TAG,
                     "downloader(%p) timeout, left(%d) bytes not pushed",
                     downloader, len - offset);
                break;
            }
        }
        break;
    }
    case DOWNLOAD_TO_CALLBACK:
        downloader->calllback(downloader, body, len);
        break;
    default: break;
    }

    downloader->recved_bytes += len;
    // LOGI(SDK_TAG, "downloader(%p) recved %d bytes", downloader,
    //     downloader->recved_bytes);

    return 0;
}

rc_downloader rc_downloader_init(const char* url, const char* headers[],
                                 int header_count, int timeout_ms,
                                 rc_download_type_t type, void* data) {
    rc_runtime_t* env = get_env_instance();
    if (env == NULL || env->httpmgr == NULL) {
        LOGI(SDK_TAG, "env is not inited");
        return NULL;
    }
    rc_http_downloader* downloader =
        (rc_http_downloader*)rc_malloc(sizeof(rc_http_downloader));
    memset(downloader, 0, sizeof(rc_http_downloader));
    downloader->type = type;
    downloader->data = data;

    http_request request =
        http_request_init(env->httpmgr, url, NULL, HTTP_REQUEST_GET);
    if (request == NULL) {
        LOGI(SDK_TAG, "downloader create http request failed");
        rc_free(downloader);
        return NULL;
    }

    if (header_count != 0 && headers != NULL) {
        for (int i = 0; i < header_count; ++i) {
            if (headers[i] != NULL) {
                rc_buf_t* head = rc_buf_init(strlen(headers[i]));
                strcpy(rc_buf_head_ptr(head), headers[i]);
                http_request_set_opt(request, HTTP_REQUEST_OPT_HEADER, head);
            }
        }
    }
    http_request_set_opt(request, HTTP_REQUEST_OPT_TIMEOUT, &timeout_ms);
    http_request_set_opt(request, HTTP_REQUEST_OPT_RES_CHUNK_CALLBACK,
                         on_download_content);
    http_request_set_opt(request, HTTP_REQUEST_OPT_USER_DATA, downloader);

    downloader->request = request;
    downloader->timeout_at_ms = timeout_ms + rc_get_mstick();

    return downloader;
}

static void* downloader_download_thread(void* param) {
    rc_http_downloader* downloader = (rc_http_downloader*)param;

    http_request_execute(downloader->request);

    // download finished
    downloader->download_finished = 1;

    LOGI(SDK_TAG, "download_thread stoped");
    return NULL;
}

int rc_downloader_start(rc_downloader dwner, int new_thread) {
    DECLEAR_REAL_VALUE(rc_http_downloader, downloader, dwner);
    if (downloader->request == NULL) {
        LOGI(SDK_TAG, "downloader(%p) request is invalidate", downloader);
        return RC_ERROR_INVALIDATE_INPUT;
    }
    if (new_thread) {
        rc_thread_context_t ctx = {.joinable = 0,
                                   .name = "downloader",
                                   .priority = -1,
                                   .stack_size = 2048};
        downloader->thread =
            rc_thread_create(downloader_download_thread, downloader, &ctx);
        if (downloader->thread == NULL) {
            LOGI(SDK_TAG, "downloader(%p) create thread failed", downloader);
            return RC_ERROR_DOWNLOAD_FAILED;
        }

        return RC_SUCCESS;
    }

    return http_request_execute(downloader->request);
}

int rc_downloader_get_status(rc_downloader dwner, int* total, int* current) {
    DECLEAR_REAL_VALUE(rc_http_downloader, downloader, dwner);

    if (total != NULL) {
        *total = downloader->total_bytes;
    }

    if (current != NULL) {
        *current = downloader->recved_bytes;
    }

    return 0;
}

int rc_downloader_uninit(rc_downloader dwner) {
    DECLEAR_REAL_VALUE(rc_http_downloader, downloader, dwner);

    if (downloader->thread != NULL) {
        while (!downloader->download_finished) {
            rc_sleep(100);  // wait for download thread finished
            // rc_thread_join(downloader->thread);
        }
    }

    if (downloader->request != NULL) {
        http_request_uninit(downloader->request);
    }

    rc_free(downloader);

    return RC_SUCCESS;
}