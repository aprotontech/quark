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

typedef enum _rc_network_mask {
    NETWORK_MASK_LOCAL = 0x01,      // local network, eg: wifi
    NETWORK_MASK_SERVICE = 0x02,    // got all services
    NETWORK_MASK_SESSION = 0x04,    // device had registed 
    NETWORK_MASK_KEEPALIVE = 0x08,  // mqtt had connected
    NETWORK_MASK_PING_BAIDU = 0x10, // ping www.baidu.com
    NETWORK_MASK_PING_API = 0x20,   // ping api service
} rc_network_mask;

// is network available
int network_is_available(rc_network_manager mgr, int mask);

int network_set_available(rc_network_manager mgr, int mask, int available);

rc_network_manager network_manager_init(int current_status);

int network_manager_uninit(rc_network_manager mgr);

#endif