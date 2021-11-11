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

#define RC_THREAD_NAME_MAX_LENGTH 20
#define RC_DEFAULT_THREAD_NAME "quark"

typedef struct _rc_thread_t {
#if defined(__QUARK_RTTHREAD__)
    rt_thread_t pthread;
    rc_thread_function func;
    void* arg;
#elif defined(__QUARK_FREERTOS__)
    rc_thread_function func;
    void* arg;
    TaskHandle_t handle;
#elif defined(__QUARK_LINUX__)
    pthread_t pthread;
#endif
    char name[RC_THREAD_NAME_MAX_LENGTH];
} rc_thread_t;

#if defined(__QUARK_RTTHREAD__)

#define RC_DEFAULT_THREAD_STACK_SIZE RT_THREAD_STACK_SIZE
#define RC_DEFAULT_THREAD_PRIORITY 0

void rtos_thread_adatpor(void* args)
{
    rc_thread_t* p = (rc_thread_t*)args;
    LOGI("THREAD", "rtos_thread_adatpor");
    p->func(p->arg);
}

#elif defined(__QUARK_FREERTOS__)

#define RC_DEFAULT_THREAD_STACK_SIZE 4096
#define RC_DEFAULT_THREAD_PRIORITY (tskIDLE_PRIORITY + 12)

static void freertos_thread_adaptor(void* args)
{
    rc_thread_t* p = (rc_thread_t*)args;
    if (p != NULL) {
        LOGI("THREAD", "thread(%s) started", p->name);
        p->func(p->arg);

        LOGI("THREAD", "thread(%s) finished", p->name);

        vTaskDelete(p->handle);
        free(p);
    }
}

#endif

#if defined(__QUARK_FREERTOS__) || defined(__QUARK_RTTHREAD__)

int _thread_stack_size = RC_DEFAULT_THREAD_STACK_SIZE;
int _thread_priority = RC_DEFAULT_THREAD_PRIORITY;
const char* _thread_name = RC_DEFAULT_THREAD_NAME;

int set_next_thread_params(const char* thread_name, int stack_size, int thread_priority)
{
    if (thread_name != NULL) {
        if (strlen(thread_name) >= RC_THREAD_NAME_MAX_LENGTH) {
            LOGE("TREAD", "invalidate thread name(%s), length is too long", thread_name);
            thread_name = RC_DEFAULT_THREAD_NAME;
            return RC_ERROR_INVALIDATE_INPUT;
        }
    } else {
        thread_name = RC_DEFAULT_THREAD_NAME;
    }

    if (stack_size <= 0) {
       stack_size = RC_DEFAULT_THREAD_STACK_SIZE;
    }

    if (thread_priority < 0) {
        _thread_priority = RC_DEFAULT_THREAD_PRIORITY;
    }
    
    _thread_stack_size = stack_size;
    _thread_priority = thread_priority;
    _thread_name = thread_name;
    return 0;
}

#endif

rc_thread rc_thread_create(rc_thread_function func, void* arg)
{
    rc_thread_t* thread = (rc_thread_t*)malloc(sizeof(rc_thread_t));
    memset(thread, 0, sizeof(rc_thread_t));
    strcpy(thread->name, _thread_name);
    
#if defined(__QUARK_RTTHREAD__)
    
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
    thread->func = func;
    thread->arg = arg;
    BaseType_t btt = xTaskCreate(freertos_thread_adaptor, thread->name, _thread_stack_size, 
        thread, _thread_priority, &thread->handle);
    if (btt != pdPASS) {
         LOGE("THREAD", "***************create new thread failed************");
         free(thread);
         return NULL;
    }
    LOGI("WIFI", "create freertos task success")
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
        vTaskDelete(thread->handle);
#elif defined(__QUARK_LINUX__)
        rc = pthread_join(thread->pthread, NULL);
#endif
        free(thread);
    }

    return rc;
}

int rc_sleep(int ms)
{
#if defined(__QUARK_RTTHREAD__)
    rt_thread_mdelay(ms);
#elif defined(__QUARK_FREERTOS__)
    vTaskDelay(ms / portTICK_PERIOD_MS);
#elif defined(__QUARK_LINUX__)
    sleep(ms / 1000); // sleep ms later
#endif
    return 0;
}