/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_timer.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-20 18:08:37
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_TIMER_H_
#define _QUARK_TIMER_H_

#include "rc_system.h"

typedef void* rc_timer;
typedef void* rc_timer_manager;
typedef int (*rc_on_time)(rc_timer timer, void* usr_data);

typedef long long mstime_t;

mstime_t rc_get_mstick();

rc_timer_manager rc_timer_manager_init();

rc_timer rc_timer_create(rc_timer_manager mgr, int tick, int repeat, rc_on_time on_time, void* usr_data);

int rc_timer_ahead_once(rc_timer timer, int next_tick);

int rc_timer_stop(rc_timer timer);

int rc_timer_manager_stop_world(rc_timer_manager mgr);

int rc_timer_manager_uninit(rc_timer_manager mgr);

#endif
