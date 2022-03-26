/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     location.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-26 09:06:08
 *
 */

#ifndef _QUARK_LOCATION_H_
#define _QUARK_LOCATION_H_

#include "env.h"

typedef struct _rc_location_manager_t {
    rc_mutex mobject;
    rc_location_t current_location;
    rc_timer location_timer;
    int is_reporting_location;
    backoff_algorithm_t location_report_backoff;
} rc_location_manager_t;

typedef void* location_manager;

location_manager location_manager_init(rc_runtime_t* env);

int location_manager_uninit(location_manager mgr);

#endif