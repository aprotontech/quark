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

enum {
    RC_WIFI_CONNECTED = 1,
    RC_WIFI_DISCONNECTED = 0,
};

typedef void* wifi_manager;
typedef int (*on_wifi_status_changed)(wifi_manager mgr, int wifi_status);

wifi_manager wifi_manager_init(on_wifi_status_changed on_changed);

int wifi_manager_connect(wifi_manager mgr, const char* ssid, const char* password);

int wifi_manager_uninit(wifi_manager mgr);

#endif