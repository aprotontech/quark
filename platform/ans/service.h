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

#ifndef _AIE_SERVICE_H_
#define _AIE_SERVICE_H_

#include "ans.h"
#include "hashmap.h"
#include "rc_crypt.h"
#include "rc_mutex.h"

#define SC_TAG "[Service]"


#define MAX_ANS_SERVICES 5

typedef char* local_config_string_t[MAX_ANS_SERVICES][4];

typedef struct _rc_service_t {
    int validtm;
    int ip_count;
    char* service;
    char* uri;
    char* host;
    char* ips[1];
} rc_service_t;

typedef struct _rcservice_mgr_t {
    char* url;
    char* json;
    int json_len;
    char* encrypt_key;
    int encrypt_type;
    rsa_crypt rsa;
    http_manager manager;

    map_t smap;

    map_t ipmap;

    rc_mutex mobject;
    
} rcservice_mgr_t;

#endif
