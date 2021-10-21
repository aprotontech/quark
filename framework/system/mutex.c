/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     mutex.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-26 23:17:55
 * @version  0
 * @brief
 *
 **/

#include "rc_mutex.h"
#include <stdlib.h>
#include "rc_system.h"

typedef struct _rc_mutex_t {
#if defined(__QUARK_RTTHREAD__)
	rt_mutex_t pm;
#else
    pthread_mutex_t pm;
#endif
} rc_mutex_t;


rc_mutex rc_mutex_create(void* attr)
{
    rc_mutex_t* mutex = (rc_mutex_t*)malloc(sizeof(rc_mutex_t));
#if defined(__QUARK_RTTHREAD__)
	mutex->pm = rt_mutex_create("rc_mutex", RT_IPC_FLAG_FIFO);
    if (mutex->pm == NULL) {
        free(mutex);
        return NULL;
    }
#else
    pthread_mutex_init(&mutex->pm, NULL);
#endif

    return mutex;
}

int rc_mutex_lock(rc_mutex mt)
{
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_mutex_t* mutex = (rc_mutex_t*)mt;
    if (mutex != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rc = rt_mutex_take(mutex->pm, RT_WAITING_FOREVER);
#else        
        rc = pthread_mutex_lock(&mutex->pm);
#endif
    }

    return rc;
}

int rc_mutex_unlock(rc_mutex mt)
{
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_mutex_t* mutex = (rc_mutex_t*)mt;
    if (mutex != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rc = rt_mutex_release(mutex->pm);
#else      
        rc = pthread_mutex_unlock(&mutex->pm);
#endif        
    }

    return rc;
}

int rc_mutex_destroy(rc_mutex mt)
{
    int rc = RC_ERROR_INVALIDATE_INPUT;
    rc_mutex_t* mutex = (rc_mutex_t*)mt;
    if (mutex != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rc = rt_mutex_delete(mutex->pm);
#else
        rc = pthread_mutex_destroy(&mutex->pm);
#endif        
        free(mutex);
    }

    return rc;
}

