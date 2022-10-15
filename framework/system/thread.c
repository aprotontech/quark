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

#include <stdlib.h>

#include "logs.h"
#include "rc_event.h"
#include "rc_mutex.h"
#include "rc_system.h"
#include "rc_thread.h"

#define RC_THREAD_NAME_MAX_LENGTH 20
#define RC_DEFAULT_THREAD_NAME "quark"

typedef struct _rc_thread_t {
    rc_event* join_event;
    rc_thread_function func;
    void* arg;
#if defined(__QUARK_RTTHREAD__)
    rt_thread_t pthread;
#elif defined(__QUARK_FREERTOS__)
    TaskHandle_t handle;
#elif defined(__QUARK_LINUX__)
    pthread_t pthread;
#endif
    char name[RC_THREAD_NAME_MAX_LENGTH];
} rc_thread_t;

#if defined(__QUARK_RTTHREAD__)

#define RC_DEFAULT_THREAD_STACK_SIZE RT_THREAD_STACK_SIZE
#define RC_DEFAULT_THREAD_PRIORITY 0

void rtos_thread_adatpor(void* args) {
    rc_thread_t* p = (rc_thread_t*)args;
    LOGI("THREAD", "rtos_thread_adatpor");
    p->func(p->arg);
}

#elif defined(__QUARK_FREERTOS__)

#define RC_DEFAULT_THREAD_STACK_SIZE 4096
#define RC_DEFAULT_THREAD_PRIORITY (tskIDLE_PRIORITY + 12)

static void freertos_thread_adaptor(void* args) {
    rc_thread_t* p = (rc_thread_t*)args;
    if (p != NULL) {
        LOGI("THREAD", "thread(%s) started", p->name);
        p->func(p->arg);

        LOGI("THREAD", "thread(%s) finished", p->name);

        vTaskDelete(p->handle);
        p->handle = NULL;
        if (p->join_event == NULL) {
            free(p);
        } else {  // notify thread stoped
            rc_event_signal(p->join_event);
        }
    }
}

#elif defined(__QUARK_LINUX__)
#define RC_DEFAULT_THREAD_STACK_SIZE 0
#define RC_DEFAULT_THREAD_PRIORITY 0

static void* linux_thread_adaptor(void* args) {
    rc_thread_t* p = (rc_thread_t*)args;
    if (p != NULL) {
        prctl(PR_SET_NAME, p->name);
        LOGI("THREAD", "thread(%s) started", p->name);
        p->func(p->arg);

        LOGI("THREAD", "thread(%s) finished", p->name);
    }

    return NULL;
}

#endif

rc_thread rc_thread_create(rc_thread_function func, void* arg, void* ctx) {
    rc_thread_t* thread = (rc_thread_t*)malloc(sizeof(rc_thread_t));
    memset(thread, 0, sizeof(rc_thread_t));
    rc_thread_context_t* thread_ctx = (rc_thread_context_t*)ctx;
    if (thread_ctx != NULL && thread_ctx->name != NULL) {
        if (strlen(thread_ctx->name) >= RC_THREAD_NAME_MAX_LENGTH) {
            LOGW("TREAD", "invalidate thread name(%s), length is too long",
                 thread_ctx->name);
        }
        strncpy(thread->name, thread_ctx->name, sizeof(thread->name) - 1);
    } else {
        strcpy(thread->name, RC_DEFAULT_THREAD_NAME);
    }

    int thread_stack_size = RC_DEFAULT_THREAD_STACK_SIZE;
    if (thread_ctx != NULL && thread_ctx->stack_size > 0) {
        thread_stack_size = thread_ctx->stack_size;
    }

    int thread_priority = RC_DEFAULT_THREAD_PRIORITY;
    if (thread_ctx != NULL && thread_ctx->priority > 0) {
        thread_priority = thread_ctx->priority;
    }

    thread->func = func;
    thread->arg = arg;

#if defined(__QUARK_RTTHREAD__)

    thread->pthread =
        rt_thread_create(thread->name, rtos_thread_adatpor, (void*)thread,
                         thread_stack_size, thread_priority, 8);
    if (thread->pthread == RT_NULL) {
        LOGE("THREAD", "***************create new thread failed************");
        free(thread);
        return NULL;
    }

    rt_thread_startup(thread->pthread);
#elif defined(__QUARK_FREERTOS__)

    if (thread_ctx != NULL && thread_ctx->joinable) {
        thread->join_event = rc_event_init();
        if (thread->join_event == NULL) {
            LOGE("THREAD", "create thread join event failed");
            free(thread);
            return NULL;
        }
    }

    BaseType_t btt =
        xTaskCreate(freertos_thread_adaptor, thread->name, thread_stack_size,
                    thread, thread_priority, &thread->handle);
    if (btt != pdPASS) {
        LOGE("THREAD", "***************create new thread failed************");
        if (thread->join_event != NULL) {
            rc_event_uninit(thread->join_event);
        }
        free(thread);
        return NULL;
    }

    LOGI("THREAD", "create freertos task success")
#elif defined(__QUARK_LINUX__)
    pthread_create(&thread->pthread, NULL, linux_thread_adaptor, thread);
#endif

    return thread;
}

int rc_thread_join(rc_thread th) {
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_thread_t* thread = (rc_thread_t*)th;
    if (thread != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rt_thread_delete(thread->pthread);
#elif defined(__QUARK_FREERTOS__)
        if (thread->join_event != NULL) {
            if (rc_event_wait(thread->join_event, INT_MAX) == 0) {  // got event
                rc_event_uninit(thread->join_event);
            }
        }
#elif defined(__QUARK_LINUX__)
        rc = pthread_join(thread->pthread, NULL);
#endif
        free(thread);
    }

    return rc;
}

int rc_sleep(int ms) {
#if defined(__QUARK_RTTHREAD__)
    rt_thread_mdelay(ms);
#elif defined(__QUARK_FREERTOS__)
    vTaskDelay(ms / portTICK_PERIOD_MS);
#elif defined(__QUARK_LINUX__)
    sleep(ms / 1000);  // sleep ms later
#endif
    return 0;
}