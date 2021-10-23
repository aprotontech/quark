/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     thread.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-26 23:08:56
 * @version  0
 * @brief
 *
 **/

#include "rc_thread.h"
#include <stdlib.h>
#include "rc_system.h"
#include "logs.h"

typedef struct _rc_thread_t {
#if defined(__QUARK_RTTHREAD__)
    rt_thread_t pthread;
    rc_thread_function func;
    void* arg;
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
    pthread_t pthread;
#endif
    char name[20];
} rc_thread_t;

#if defined(__QUARK_RTTHREAD__)

int _thread_stack_size = RT_THREAD_STACK_SIZE;

void rtos_thread_adatpor(void* args)
{
    rc_thread_t* p = (rc_thread_t*)args;
    LOGI("THREAD", "rtos_thread_adatpor");
    p->func(p->arg);
}


void set_next_rt_thread_stack_size(int size)
{
    _thread_stack_size = size;
}

#endif

rc_thread rc_thread_create(rc_thread_function func, void* arg)
{
    rc_thread_t* thread = (rc_thread_t*)malloc(sizeof(rc_thread_t));
#if defined(__QUARK_RTTHREAD__)
    memset(thread, 0, sizeof(rc_thread_t));
    strcpy(thread->name, "rc_thread");
    thread->pthread = rt_thread_create(thread->name, rtos_thread_adatpor, (void *)thread, _thread_stack_size, RT_THREAD_PROIORITY, 8);
    _thread_stack_size = RT_THREAD_STACK_SIZE;
    if (thread->pthread == RT_NULL) {
        LOGE("THREAD", "***************create new thread failed************");
        free(thread);
        return NULL;
    }

    thread->func = func;
    thread->arg = arg;
    rt_thread_startup(thread->pthread);    
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
    pthread_create(&thread->pthread, NULL, func, arg);
#endif

    return thread;
}

int rc_thread_join(rc_thread th)
{
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_thread_t* thread = (rc_thread_t*)th;
    if (thread != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rt_thread_delete(thread->pthread);
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
        rc = pthread_join(thread->pthread, NULL);
#endif
        free(thread);
    }

    return rc;
}

