/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     rc_network.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-02-26 12:02:21
 *
 */

#ifndef _QUARK_NETWORK_H_
#define _QUARK_NETWORK_H_

#include "rc_error.h"

typedef void* rc_network_manager;

typedef enum _rc_network_level {
    NETWORK_LOCAL = 0,  // local network, eg: wifi
    NETWORK_SESSION = 1,
    NETWORK_KEEPALIVE = 2,
    _NETWORK_MAX_LEVEL,
} rc_network_level;

// is network available
int network_is_available(rc_network_manager mgr, int level);

int network_set_available(rc_network_manager mgr, int level, int available);

rc_network_manager network_manager_init(int current_status);

int network_manager_uninit(rc_network_manager mgr);

#endif