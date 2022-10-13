/***************************************************************************
 *
 * Copyright (c) 2019 restartcloud.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     http_manager.c
 * @author   kuper - kuper@restartcloud.com
 * @data     2019-12-12 23:21:30
 * @version  0
 * @brief
 *
 **/

#include "logs.h"
#include <assert.h>
#include "rc_http_manager.h"
#include "rc_http_client.h"
#include "rc_error.h"
#include "rc_url.h"
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>

#define HM_TAG "[HTTP]"

typedef struct _rc_http_manager_t {
    dns_resolver dns;

} rc_http_manager_t;

http_manager http_manager_init() {
    rc_http_manager_t* http_mgr =
        (rc_http_manager_t*)rc_malloc(sizeof(rc_http_manager_t));
    http_mgr->dns = NULL;
    LOGI(HM_TAG, "http manager(%p) new", http_mgr);
    return http_mgr;
}

http_client http_manager_get_client(http_manager mgr, const char* domain,
                                    const char* ipaddr, int port) {
    http_client client = http_client_init(mgr, domain, ipaddr, port);
    return client;
}

int http_manager_free_client(http_manager mgr, http_client client, int close) {
    return http_client_uninit(client);
}

int http_manager_set_keepalive(http_manager mgr, const char* domain, int port,
                               int max_clients, int keep_alive_sec) {
    return 0;
}

int http_manager_uninit(http_manager mgr) {
    rc_http_manager_t* http_mgr = (rc_http_manager_t*)mgr;
    LOGI(HM_TAG, "http manager(%p) free", http_mgr);
    if (http_mgr != NULL) {
        rc_free(http_mgr);
        http_mgr = NULL;
    }

    return 0;
}

int http_manager_set_dns_resolver(http_manager mgr, dns_resolver dns) {
    rc_http_manager_t* http_mgr = (rc_http_manager_t*)mgr;
    if (http_mgr != NULL) {
        http_mgr->dns = dns;
    }

    return RC_SUCCESS;
}

int http_manager_resolve_dns(http_manager mgr, const char* host,
                             struct in_addr* ip) {
    struct hostent* he = NULL;
    struct in_addr** addr_list;
    rc_http_manager_t* http_mgr = (rc_http_manager_t*)mgr;

    if (http_mgr != NULL && http_mgr->dns != NULL) {
        if (http_mgr->dns(host, ip) == RC_SUCCESS) {
            return RC_SUCCESS;
        }
    }

    he = gethostbyname(host);

    if (he == NULL) {
        LOGI(HM_TAG, "gethostbyname(%s) failed", host);
        return RC_ERROR_UNKOWN_HOST;
    }

    addr_list = (struct in_addr**)he->h_addr_list;
    if (addr_list[0] == NULL) {
        return errno;
    }

    *ip = *(addr_list[0]);
    return RC_SUCCESS;
}
