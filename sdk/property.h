/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     property.h
 * @author   kuper - kuper@aproton.tech
 * @data     2020-02-02 14:47:03
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_PROPERTY_H_
#define _QUARK_PROPERTY_H_

#include "hashmap.h"
#include "rc_mutex.h"
#include "env.h"

#define MAX_PROPERTY_NAME 30

typedef struct _rc_property_t {
    rc_property_change callback;
    char name[MAX_PROPERTY_NAME];
    long long change_timestamp;
    char notset:2;
    char status:2;
    char changed:2;
    char type; // rc_property_type
    union {
        int int_value;
        double double_value;
        char* string_value;
        int bool_value;
    };
} rc_property_t;

typedef struct _rc_property_manager {
    map_t values;
    int last_report_timestamp;
    int property_change_report;
    int porperty_retry_interval;
    rc_timer property_timer;
    rc_mutex mgr_mutex;
    int findchangeditem;
} rc_property_manager;

typedef void* property_manager;

property_manager property_manager_init(rc_runtime_t* env, int property_change_report, int porperty_retry_interval);
int property_manager_uninit(property_manager manager);

#endif

