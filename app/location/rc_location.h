/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_location.h
 * @author   kuper - kuper@aproton.tech
 * @data     2020-01-22 15:12:24
 * @version  0
 * @brief
 *
 **/

#ifndef _PROTON_RC_LOCATION_
#define _PROTON_RC_LOCATION_

typedef struct _rc_wifi_info_t {
    char* ssid;
    char* mac;
    int signal;
} rc_wifi_info_t;

typedef struct _rc_location_t {
    double latitude;
    double longitude;
} rc_location_t;

int rc_report_location(const rc_wifi_info_t* wifis, int wifi_count);

rc_location_t* rc_get_current_location();

#endif
