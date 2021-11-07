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

#include "rc_error.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "rc_http_manager.h"

typedef void* ans_service;

typedef struct _rc_ans_config_t {
    char* app_id;
    char* client_id;
    // ans query service url and host(can be null)
    char* url;
    char* host;

    char** input_services;
    int input_service_count;

    int encrypt_type;
    char* encrypt_key;
} rc_ans_config_t;

enum {
    ANS_ENCRYPT_TYPE_NONE = 0,
};

ans_service rc_service_init(rc_ans_config_t* query, http_manager mgr);

int rc_service_dns_resolve(ans_service ans, const char* host, struct in_addr* ip);

int rc_service_get_info(ans_service ans, const char* name, char domain[30], char uri[50], char ip[20]);

const char* rc_service_get_ip(ans_service ans, const char* service);
const char* rc_service_get_url(ans_service ans, const char* service);

int rc_service_uninit(ans_service ans);

#endif
