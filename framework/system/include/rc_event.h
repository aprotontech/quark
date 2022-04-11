/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     event.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-20 00:04:21
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_EVENT_H_
#define _QUARK_EVENT_H_

typedef void* rc_event;

rc_event rc_event_init();

int rc_event_wait(rc_event event, int timeout_ms);

int rc_event_signal(rc_event event);

int rc_event_uninit(rc_event event);

#endif
