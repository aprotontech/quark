
#include "manager.h"
#include "rc_system.h"

#if defined(__QUARK_FREERTOS__)

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "logs.h"
#include "rc_error.h"

#include "nvs_flash.h"

const int CONNECTED_BIT = BIT0;
const int SCAN_DONE_BIT = BIT1;

#define MAX_AP_CACHE_SIZE 20

int _get_wifi_connect_bit(EventGroupHandle_t wifi_event_group) {
    return (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) ==
                   CONNECTED_BIT
               ? 1
               : 0;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    wifi_manager_t* mgr = (wifi_manager_t*)arg;
    if (mgr == NULL) {
        LOGW(WIFI_TAG, "input mgr==null");
        return;
    }
    LOGI(WIFI_TAG, "type(%s), event(%d)", event_base, event_id);
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case SYSTEM_EVENT_STA_START:
            LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_CONNECTED");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
            auto-reassociate. */
            LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            xEventGroupClearBits(mgr->wifi_event_group, CONNECTED_BIT);
            mgr->status = RC_WIFI_DISCONNECTED;
            if (mgr->on_changed != NULL) {
                mgr->on_changed(mgr, RC_WIFI_DISCONNECTED);
            }
            break;
        case SYSTEM_EVENT_SCAN_DONE: {
            uint16_t i, ap_count = 0;
            wifi_ap_record_t tmp;
            rc_wifi_ap_t* aps = NULL;

            esp_wifi_scan_get_ap_num(&ap_count);
            if (ap_count != 0 && mgr->scan_result != NULL) {
                if (mgr->ap_cache == NULL) {
                    mgr->ap_cache = (wifi_ap_record_t*)malloc(
                        sizeof(wifi_ap_record_t) * MAX_AP_CACHE_SIZE);
                }
                wifi_ap_record_t* list = mgr->ap_cache;
                if (ap_count >= MAX_AP_CACHE_SIZE) {
                    ap_count = MAX_AP_CACHE_SIZE;
                }

                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, list));

                // because sizeof(wifi_ap_record_t) > sizeof(rc_wifi_ap_t)
                aps = (rc_wifi_ap_t*)list;
                for (i = 0; i < ap_count; i++) {
                    tmp = list[i];

                    aps[i].rssi = list[i].rssi;

                    memcpy(aps[i].mac, tmp.bssid, sizeof(tmp.bssid));
                    memcpy(aps[i].ssid, tmp.ssid, sizeof(tmp.ssid));

                    LOGI(WIFI_TAG, "(%d,\"%s\",%d,\"" MACSTR "\", %d)",
                         tmp.authmode, aps[i].ssid, aps[i].rssi,
                         MAC2STR(aps[i].mac), tmp.primary);
                }

                mgr->scan_result->aps = aps;
                mgr->scan_result->count = ap_count;
            }

            xEventGroupSetBits(mgr->wifi_event_group, SCAN_DONE_BIT);

            break;
        }
        default:
            break;
        }

    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            snprintf(mgr->ip, sizeof(mgr->ip), "%d.%d.%d.%d",
                     IP2STR(&event->ip_info.ip));
            LOGI(WIFI_TAG, "ip event: SYSTEM_EVENT_STA_GOT_IP, ip=%s", mgr->ip);
            xEventGroupSetBits(mgr->wifi_event_group, CONNECTED_BIT);
            mgr->status = RC_WIFI_CONNECTED;

            if (mgr->on_changed != NULL) {
                mgr->on_changed(mgr, RC_WIFI_CONNECTED);
            }
        }
    }
}

int wifi_manager_connect(wifi_manager wm, const char* ssid,
                         const char* password) {
    wifi_manager_t* mgr = (wifi_manager_t*)wm;
    if (mgr == NULL) {
        LOGW(WIFI_TAG, "input wifi manager is null");
        return RC_ERROR_INVALIDATE_INPUT;
    }

    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {.capable = true, .required = false},
            },
    };

    int ssid_len = strlen(ssid);
    if (ssid_len >= sizeof(wifi_config.sta.ssid)) {
        LOGE(WIFI_TAG, "wifi ssid(%s) is too long. max length=%d", ssid,
             sizeof(wifi_config.sta.ssid));
        return -1;
    }
    int pass_len = strlen(password);
    if (pass_len >= sizeof(wifi_config.sta.password)) {
        LOGE(WIFI_TAG, "wifi password(%s) is too long. max length=%d", password,
             sizeof(wifi_config.sta.password));
        return -1;
    }

    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    LOGI(WIFI_TAG, "wifi_init_sta finished.");

    return 0;
}

int wifi_manager_scan_ap(wifi_manager wm, rc_wifi_scan_result_t* result) {
    DECLEAR_REAL_VALUE(wifi_manager_t, mgr, wm);
    wifi_scan_config_t scan_config = {
        .bssid = NULL,
        .channel = 0,
        .show_hidden = 1,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .ssid = NULL,
    };

    if (result == NULL) {
        LOGW(WIFI_TAG, "input result is empty");
        return RC_ERROR_INVALIDATE_INPUT;
    }

    if (mgr->scan_result != NULL) {
        LOGI(WIFI_TAG, "current is scaning");
        return RC_ERROR_REPEAT_CALL;
    }

    mgr->scan_result = result;

    xEventGroupClearBits(mgr->wifi_event_group, SCAN_DONE_BIT);

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, 1));

    xEventGroupWaitBits(mgr->wifi_event_group, SCAN_DONE_BIT, 1, 1, 10 * 1000);

    mgr->scan_result = NULL;
    return 0;
}

int _wifi_manager_init(wifi_manager_t* mgr) {
    mgr->ap_cache = NULL;
    mgr->wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, mgr, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, mgr, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    return 0;
}

#endif