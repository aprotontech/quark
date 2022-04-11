/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_manager.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-12 23:23:24
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_HTTP_MANAGER_H_
#define _QUARK_HTTP_MANAGER_H_

#include "rc_http_client.h"

typedef void* http_manager;
typedef int (*dns_resolver)(const char* host, struct in_addr* ip);

http_manager http_manager_init();

http_client http_manager_get_client(http_manager mgr, const char* domain, const char *ipaddr, int port);

int http_manager_free_client(http_manager mgr, http_client client, int close);

int http_manager_set_keepalive(http_manager mgr, const char* domain, int port, int max_clients, int keep_alive_sec);

int http_manager_uninit(http_manager mgr);

int rc_resolve_dns(http_manager mgr, const char* host, struct in_addr* ip);

int http_manager_set_dns_resolver(http_manager mgr, dns_resolver dns);

#endif
