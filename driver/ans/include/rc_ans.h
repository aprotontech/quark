/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     ans.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-20 15:12:59
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_ANS_H_
#define _QUARK_ANS_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include "hashmap.h"
#include "rc_error.h"
#include "rc_http_manager.h"
#include "rc_network.h"
#include "rc_timer.h"

typedef void* ans_service;

typedef struct _rc_service_t {
    int validtm;
    char* service;
    char* host;
    char* prefix;

    // item is rc_service_protocol_t
    map_t protocols;

    short ip_count;
    char* ips[1];
} rc_service_t;

typedef struct _rc_service_protocol_info_t {
    char* host;
    char* prefix;
    char* ip;
    int port;
} rc_service_protocol_info_t;

ans_service rc_service_init(const char* app_id, const char* device_id,
                            const char* url, http_manager mgr,
                            rc_timer_manager tmgr, rc_network_manager* nmgr);

// load local configs. if service exists, skip it
int rc_service_local_config(ans_service ans, const char* env_config);

int rc_service_dns_resolve(ans_service ans, const char* host,
                           struct in_addr* ip);

int rc_service_sync(ans_service ans, int force);

int rc_service_is_synced(ans_service ans);

int rc_service_query(ans_service ans, const char* name, const char* protocol,
                     rc_service_protocol_info_t* info);

int rc_service_uninit(ans_service ans);

#endif
