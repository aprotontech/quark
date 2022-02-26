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

// is network available
int is_network_available(rc_network_manager mgr, int level);

int network_set_available(rc_network_manager mgr, int available);

rc_network_manager network_manager_init(int current_status);

int network_manager_uninit(rc_network_manager mgr);

#endif