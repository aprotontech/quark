/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     service.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 23:09:41
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_SERVICE_H_
#define _QUARK_SERVICE_H_


#include "hashmap.h"
#include "rc_crypt.h"
#include "rc_mutex.h"
#include "rc_ans.h"

#define SC_TAG "[Service]"

#define DEFAULT_SERVICE "default"

typedef struct _rc_service_protocol_t {
    char protocol[10];
    short port;
} rc_service_protocol_t;

typedef struct _rcservice_mgr_t {
    char* url;

    char* app_id;
    char* device_id;

    rsa_crypt rsa;
    http_manager httpmgr;
    rc_network_manager netmgr;

    rc_timer timer;

    int sync_status;  // 0: idle, 1: syncing, 2: finished

    map_t smap;

    map_t ipmap;

    rc_mutex mobject;

} rcservice_mgr_t;

#endif
