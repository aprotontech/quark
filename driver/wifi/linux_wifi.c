/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     linux_wifi.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-12-28 10:52:55
 *
 */

#include "logs.h"
#include "manager.h"
#include "rc_error.h"
#include "rc_system.h"

#if defined(__QUARK_LINUX__)

void fill_local_ip(char ip[16]) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        LOGI(WIFI_TAG, "getifaddrs failed");
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET && strcmp(ifa->ifa_name, "lo") != 0) {
            strcpy(ip,
                   inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr));
            LOGI(WIFI_TAG, "interfac: %s, ip: %s", ifa->ifa_name, ip);
            break;
        }
    }

    freeifaddrs(ifaddr);
}

int wifi_manager_connect(wifi_manager wm, const char* ssid,
                         const char* password) {
    DECLEAR_REAL_VALUE(wifi_manager_t, wifimgr, wm);
    wifimgr->status = RC_WIFI_CONNECTED;
    fill_local_ip(wifimgr->ip);
    if (wifimgr->on_changed != NULL) {
        wifimgr->on_changed(wm, RC_WIFI_CONNECTED);
    }
    return 0;
}

#endif