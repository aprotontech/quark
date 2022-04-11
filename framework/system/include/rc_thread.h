/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_thread.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-26 23:06:25
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_THREAD_H_
#define _QUARK_THREAD_H_

typedef void* rc_thread;

typedef void* (*rc_thread_function)(void* arg);

typedef struct _rc_thread_context_t {
    short priority;
    short stack_size;
    char joinable;
    const char* name;
} rc_thread_context_t;

rc_thread rc_thread_create(rc_thread_function func, void* arg, void* ctx);

int rc_thread_join(rc_thread thread);

int rc_sleep(int ms);

#endif
