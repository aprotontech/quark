/***************************************************************************
 *
 * Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     wifi.h
 * @author   kuper - kuper@aproton.tech
 * @data     2021-10-23 18:00:00
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_DRIVER_WIFI_H_
#define _QUARK_DRIVER_WIFI_H_

#include <stdint.h>

enum {
    RC_WIFI_CONNECTED = 1,
    RC_WIFI_DISCONNECTED = 0,
};

typedef struct _wifi_ap_t {
    uint8_t mac[6];
    uint8_t rssi;
    char ssid[33];
} rc_wifi_ap_t;

typedef struct _wifi_scan_result_t {
    int count;
    rc_wifi_ap_t* aps;
} rc_wifi_scan_result_t;

typedef void* wifi_manager;
typedef int (*on_wifi_status_changed)(wifi_manager mgr, int wifi_status);

wifi_manager wifi_manager_init(on_wifi_status_changed on_changed);

int wifi_manager_connect(wifi_manager mgr, const char* ssid,
                         const char* password);

int wifi_manager_uninit(wifi_manager mgr);

int wifi_get_local_ip(wifi_manager mgr, char* ip, int* wifi_status);

int wifi_manager_scan_ap(wifi_manager mgr, rc_wifi_scan_result_t* result);

#endif