/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     net.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-10-10 00:01:57
 *
 */

#include "rc_url.h"
#include "rc_error.h"

#include <arpa/inet.h>
#include <netdb.h>

int rc_dns_resolve(const char* host, char* ip, int idx) {
    struct in_addr** addr_list;
    struct hostent* he = gethostbyname(host);

    if (he != NULL) {
        addr_list = (struct in_addr**)he->h_addr_list;
        if (addr_list[0] != NULL && inet_ntoa(**addr_list) != NULL) {
            strcpy(ip, inet_ntoa(**addr_list));

            return 0;
        }
    }

    return RC_ERROR_DNS;
}