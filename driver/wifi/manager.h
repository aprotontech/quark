
#ifndef _QUARK_WIFI_MANAGER_
#define _QUARK_WIFI_MANAGER_

#include "wifi.h"
#include "rc_system.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_event.h"
#include "esp_wifi.h"
#endif

#define WIFI_TAG "[WIFI]"

typedef struct _wifi_manager_t {
    char ip[16];
    char status;
    on_wifi_status_changed on_changed;
    rc_wifi_scan_result_t* scan_result;

#if defined(__QUARK_FREERTOS__)
    EventGroupHandle_t wifi_event_group;
    wifi_config_t wifi_config;
    wifi_scan_config_t scan_config;
    wifi_ap_record_t* ap_cache;

#endif
} wifi_manager_t;

#endif