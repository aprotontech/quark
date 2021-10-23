
#include "wifi.h"
#include "rc_system.h"

#define WIFI_TAG "[WIFI]"

#if defined(__QUARK_FREERTOS__)

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "logs.h"

#include "nvs_flash.h"

const int CONNECTED_BIT = BIT0;
static EventGroupHandle_t wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case SYSTEM_EVENT_STA_START:
        LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_START");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_GOT_IP");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        LOGI(WIFI_TAG, "wifi event: SYSTEM_EVENT_STA_DISCONNECTED");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
}

int rc_set_wifi(char* ssid, char* password)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    int ssid_len = strlen(ssid);
    if (ssid_len >= sizeof(wifi_config.sta.ssid)) {
        LOGE(WIFI_TAG, "wifi ssid(%s) is too long. max length=%d", ssid, sizeof(wifi_config.sta.ssid));
        return -1;
    }
    int pass_len = strlen(password);
    if (pass_len >= sizeof(wifi_config.sta.password)) {
        LOGE(WIFI_TAG, "wifi password(%s) is too long. max length=%d", password, sizeof(wifi_config.sta.password));
        return -1;
    }

    strcpy(wifi_config.sta.ssid, ssid);
    strcpy(wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    LOGI(WIFI_TAG, "wifi_init_sta finished.");

    return 0;
}

#endif