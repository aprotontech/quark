/*
 * **************************************************************************
 * 
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 * 
 * **************************************************************************
 * 
 *  @file     rc_buf_queue.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-11-14 11:20:21
 * 
 */

#ifndef __QUARK_BUF_QUEQU_H__
#define __QUARK_BUF_QUEQU_H__


typedef void* rc_buf_queue;

rc_buf_queue rc_buf_queue_init(int swap_size, int swaps, int skip_header_bytes);

int rc_buf_queue_push(rc_buf_queue queue, const char* data, int len, int timeout_ms);

int rc_buf_queue_pop(rc_buf_queue queue, char* data, int len, int timeout_ms);

int rc_buf_queue_clean(rc_buf_queue queue);

int rc_buf_queue_is_empty(rc_buf_queue queue);

int rc_buf_queue_is_full(rc_buf_queue queue);

int rc_buf_queue_get_size(rc_buf_queue queue);

int rc_buf_queue_uninit(rc_buf_queue queue);

#endif