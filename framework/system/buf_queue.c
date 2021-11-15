/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     buf_queue.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-11-14 11:20:12
 *
 */

#include "./include/rc_buf_queue.h"
#include "logs.h"
#include "rc_buf.h"
#include "rc_event.h"
#include "rc_mutex.h"
#include "rc_thread.h"
#include "rc_timer.h"

typedef struct _rc_buf_queue_t {
    rc_mutex buf_lock;
    rc_event push_event;
    rc_event pop_event;
    int skip_header_bytes;
    int total_bytes;

    list_link_t free_queue;  // queue header
    list_link_t inuse_queue;

    int dequeue_offset;

    int swap_count;
    rc_buf_t swaps[1];
} rc_buf_queue_t;

rc_buf_queue rc_buf_queue_init(int swap_size, int swaps,
                               int skip_header_bytes) {
    if (swap_size <= 0 || swaps <= 1) {
        LOGI("[QUEUE]", "invalidate input swap_size(%d), swap_count(%d)",
             swap_size, swaps);
        return NULL;
    }

    int queue_bytes = sizeof(rc_buf_queue_t) + (swaps - 1) * sizeof(rc_buf_t) +
                      swap_size * swaps + swaps;

    rc_buf_queue_t* queue = (rc_buf_queue_t*)rc_malloc(queue_bytes);
    if (queue == NULL) {
        LOGW("[QUEUE]", "malloc buffer queue failed. length=%d", queue_bytes);
        return NULL;
    }
    memset(queue, 0, queue_bytes);

    queue->push_event = rc_event_init();
    queue->pop_event = rc_event_init();
    queue->buf_lock = rc_mutex_create(NULL);

    queue->skip_header_bytes = skip_header_bytes;
    queue->swap_count = swaps;

    char* p = (char*)&queue->swaps[swaps];
    for (int i = 0; i < queue->swap_count; ++i) {
        queue->swaps[i] = rc_buf_stack();
        queue->swaps[i].total = swap_size;
        queue->swaps[i].usr_buf = p;
        p += swap_size + 1;  // skip '\0' for debug printer
    }

    rc_buf_queue_clean(queue);

    return queue;
}

int rc_buf_queue_clean(rc_buf_queue q) {
    rc_buf_queue_t* queue = (rc_buf_queue_t*)q;

    rc_mutex_lock(queue->buf_lock);
    LL_init(&queue->free_queue);
    LL_init(&queue->inuse_queue);

    queue->dequeue_offset = 0;

    for (int i = 0; i < queue->swap_count; ++i) {
        LL_insert(&queue->swaps[i].link, queue->free_queue.prev);
    }

    rc_mutex_unlock(queue->buf_lock);

    return 0;
}

int rc_buf_queue_is_empty(rc_buf_queue q) {
    int is_empty = 0;
    rc_buf_queue_t* queue = (rc_buf_queue_t*)q;

    rc_mutex_lock(queue->buf_lock);
    if (!LL_isspin(&queue->inuse_queue)) {
        is_empty = 1;
    }

    rc_mutex_unlock(queue->buf_lock);
    return is_empty;
}

int _enqueue_data(rc_buf_queue_t* queue, const char* data, int len) {
    rc_buf_t* buf = NULL;

    int offset = 0;
    while (offset < len) {
        if (!LL_isspin(&queue->inuse_queue) &&
            RC_BUF_LEFT_SIZE((rc_buf_t*)queue->inuse_queue.prev) > 0) {
            buf = (rc_buf_t*)queue->inuse_queue.prev;
            int left_size = len - offset;
            if (left_size > RC_BUF_LEFT_SIZE(buf)) {
                left_size = RC_BUF_LEFT_SIZE(buf);
            }

            rc_buf_append(buf, data + offset, left_size);
            offset += left_size;
        } else if (!LL_isspin(&queue->free_queue)) {  // has free queue
            list_link_t* lt = queue->free_queue.next;
            LL_remove(lt);
            LL_insert(lt, queue->inuse_queue.prev);

            // notify consumer data
            rc_event_signal(queue->pop_event);
        } else {
            break;
        }
    }

    return offset;
}

int rc_buf_queue_push(rc_buf_queue q, const char* data, int len,
                      int timeout_ms) {
    mstime_t stm = rc_get_mstick() + timeout_ms;
    rc_buf_queue_t* queue = (rc_buf_queue_t*)q;

    int offset = 0;
    while (offset < len) {
        rc_mutex_lock(queue->buf_lock);

        offset += _enqueue_data(queue, data + offset, len - offset);

        rc_mutex_unlock(queue->buf_lock);

        if (offset < len) {  // wait for new buffer
            if (timeout_ms == 0) break;
            mstime_t cur = rc_get_mstick();
            if (cur < stm) {
                rc_event_wait(queue->push_event, stm - cur);
            } else {  // wait timeout
                break;
            }
        }
    }

    return offset;
}

int _dequeue_data(rc_buf_queue_t* queue, char* data, int len) {
    rc_buf_t* buf = NULL;
    char* p = data;

    int offset = 0;
    while (offset < len && !LL_isspin(&queue->inuse_queue)) {
        rc_buf_t* buf = (rc_buf_t*)queue->inuse_queue.next;
        if (queue->dequeue_offset < buf->length) {  // still have data
            int left_size = len - offset;
            if (queue->dequeue_offset + left_size > buf->length) {
                left_size = buf->length - queue->dequeue_offset;
            }
            memcpy(data + offset, rc_buf_head_ptr(buf) + queue->dequeue_offset,
                   left_size);
            queue->dequeue_offset += left_size;
            offset += left_size;
        }

        if (queue->dequeue_offset >= buf->length) {
            // current buffer data had dequeue
            LL_remove(&buf->link);  // remove from in-use queue
            LL_insert(&buf->link, queue->free_queue.prev);
            buf->length = 0;
            queue->dequeue_offset = 0;

            // notify push data
            rc_event_signal(queue->push_event);
        }
    }

    return offset;
}

int rc_buf_queue_pop(rc_buf_queue q, char* data, int len, int timeout_ms) {
    mstime_t stm = rc_get_mstick() + timeout_ms;
    rc_buf_queue_t* queue = (rc_buf_queue_t*)q;

    int offset = 0;
    while (offset < len) {
        rc_mutex_lock(queue->buf_lock);

        offset += _dequeue_data(queue, data + offset, len - offset);

        rc_mutex_unlock(queue->buf_lock);

        if (offset < len) {  // wait for new buffer
            if (timeout_ms == 0) break;
            mstime_t cur = rc_get_mstick();
            if (cur < stm) {
                rc_event_wait(queue->pop_event, stm - cur);
            } else {  // wait timeout
                break;
            }
        }
    }

    return offset;
}

int rc_buf_queue_uninit(rc_buf_queue q) {
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_buf_queue_t* queue = (rc_buf_queue_t*)q;
    if (queue != NULL) {
        rc_event_uninit(queue->push_event);
        rc_event_uninit(queue->pop_event);
        rc_mutex_destroy(queue->buf_lock);
        rc_free(queue);
        rc = RC_SUCCESS;
    }
    return rc;
}
