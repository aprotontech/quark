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

rc_thread rc_thread_create(rc_thread_function func, void* arg);

int rc_thread_join(rc_thread thread);

#endif
