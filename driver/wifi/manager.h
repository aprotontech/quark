
#ifndef _QUARK_WIFI_MANAGER_
#define _QUARK_WIFI_MANAGER_

#include "wifi.h"
#include "rc_system.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_event.h"
#endif

#define WIFI_TAG "[WIFI]"

typedef struct _wifi_manager_t {
    char ip[16];
    on_wifi_status_changed on_changed;

#if defined(__QUARK_FREERTOS__)
    EventGroupHandle_t wifi_event_group;
#endif
} wifi_manager_t;

#endif