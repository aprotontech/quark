/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     heap.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-21 13:45:33
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_TIME_HEAD_H_
#define _QUARK_TIME_HEAD_H_

#include "rc_event.h"
#include "rc_timer.h"
#include "rc_thread.h"
#include "rc_mutex.h"
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
//#include <memory.h>
#include <errno.h>

#define TIMER_STOP_FLAG 2144937600
#define TM_TAG "[TIMER]"

typedef struct _rc_timer_t {
    void* usr_data;
    struct timeval next_tick;

    rc_on_time callback;
    int tick;
    int repeat;

    int idx;
    short calling;
    short stoped;
    rc_timer_manager manager;

} rc_timer_t;

typedef struct _rc_timer_manager_t {
    rc_timer_t** heap;
    int heap_total;
    volatile int heap_length;

    rc_thread timer_thread;
    rc_mutex mobject;
    rc_event event;
    short exit_thread;
} rc_timer_manager_t;

void swap_timer_node(rc_timer_t** heap, int i, int j);
int adjust_node(rc_timer_t** heap, int total, int i);


#endif

