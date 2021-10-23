/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     event.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-20 00:06:09
 * @version  0
 * @brief
 *
 **/

#include "rc_event.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "logs.h"
#include "rc_system.h"
#define RC_DEFAULT_EVENT_SET 0x00000400

typedef struct _rc_pthread_event_t {
#if defined(__QUARK_RTTHREAD__)
	rt_event_t mevent;
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
    pthread_mutex_t mevent;
    pthread_cond_t mcond;
#endif
} rc_pthread_event_t;

rc_event rc_event_init()
{
    rc_pthread_event_t* event = (rc_pthread_event_t*)malloc(sizeof(rc_pthread_event_t));
#if defined(__QUARK_RTTHREAD__)
	event->mevent = rt_event_create ("rc_event", RT_IPC_FLAG_FIFO);
    if (event->mevent == RT_NULL) {
		free(event);
        return NULL;
    }
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
    pthread_mutex_init(&event->mevent, NULL);
    pthread_cond_init(&event->mcond, NULL);
#endif
    return event;
}

int rc_event_wait(rc_event evt, int timeout)
{
    int rc;
    int msec;
    rc_pthread_event_t* event = (rc_pthread_event_t*)evt;
    if (event != NULL) {
#if defined(__QUARK_RTTHREAD__)
        //rt_thread_mdelay(timeout / 2);
        rt_uint32_t e;

        rc = rt_event_recv(event->mevent, RC_DEFAULT_EVENT_SET,
                                RT_EVENT_FLAG_CLEAR | RT_EVENT_FLAG_AND,
                                timeout, &e);
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        msec = (ts.tv_nsec / 1000000) + (timeout % 1000);
        ts.tv_sec += (timeout / 1000) + (msec / 1000);
        ts.tv_nsec = (ts.tv_nsec % 1000000) + (msec % 1000) * 1000000;
        pthread_mutex_lock(&event->mevent);
        rc = pthread_cond_timedwait(&event->mcond, &event->mevent, &ts);
        pthread_mutex_unlock(&event->mevent);
#endif
        return rc;
    }
    
    return EINVAL;
}

int rc_event_signal(rc_event evt)
{
    int rc;
    rc_pthread_event_t* event = (rc_pthread_event_t*)evt;
    if (event != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rc = rt_event_send(event->mevent, RC_DEFAULT_EVENT_SET);
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
        pthread_mutex_lock(&event->mevent);
        rc = pthread_cond_signal(&event->mcond);
        pthread_mutex_unlock(&event->mevent);
#endif
        return rc;
    }
    
    return EINVAL;
}

int rc_event_uninit(rc_event evt)
{
    rc_pthread_event_t* event = (rc_pthread_event_t*)evt;
    if (event != NULL) {
#if defined(__QUARK_RTTHREAD__)
        rt_event_delete(event->mevent);
#elif defined(__QUARK_FREERTOS__)

#elif defined(__QUARK_LINUX__)
        pthread_mutex_destroy(&event->mevent);
        pthread_cond_destroy(&event->mcond);
#endif
        free(event);
    }

    return 0;
}
