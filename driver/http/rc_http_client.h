/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_client.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-12 23:24:58
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_HTTP_CLIENT_H_
#define _QUARK_HTTP_CLIENT_H_

#include "rc_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef void* http_client;
typedef void* http_manager;

enum {
    HTTP_CLIENT_OPT_TIMEOUT = 0
};

http_client http_client_init(http_manager mgr, const char* domain, const char *ipaddr, int port);

int http_client_uninit(http_client client);

int http_client_connect(http_client client, int schema, struct timeval* timeout);

int http_client_recv(http_client client, char* buf, int buf_len, struct timeval* timeout);

int http_client_send(http_client client, const char* buf, int buf_len, struct timeval* timeout);

#endif

