
#include "mock.h"

#include "rc_thread.h"

extern int httpget_other_service_url();
extern int set_next_thread_params(const char* thread_name, int stack_size,
                                  int thread_priority);

int watch_wifi_status_change(int connected);

int mock_virtual_device(char* env, char* app_id, char* app_secret,
                        int test_time_sec) {
    int i = 0;
    rc_settings_t settings;

    LOGI(VD_TAG, "quark sdk version: %s", rc_sdk_version());

    rc_settings_init(&settings);
    settings.wifi_status_callback = watch_wifi_status_change;
    settings.app_id = app_id;
    settings.app_secret = app_secret;
    settings.uuid = NULL;
    settings.enable_keepalive = 1;
    settings.iot_platform = RC_IOT_QUARK;
    settings.service_url = "http://192.168.3.24:8080/api";

    // init sdk
    RC_EXCEPT_SUCCESS(rc_sdk_init("test", 1, &settings));

    // get wifi status, if is not connected, do connect
    for (i = 5; i >= 0; i--) {
        LOGI(VD_TAG, "Start to connect to wifi in %d seconds...", i);

        rc_sleep(1000);
    }

    RC_EXCEPT_SUCCESS(rc_set_wifi("kog_2.4G", "huxiaolong@2018"));

    // entry working thread
    for (i = 10; i < test_time_sec; ++i) {
        if (i % 60 == 0) {
            LOGI(VD_TAG, "tick in %d seconds...", i);
        }
        rc_sleep(1000);
    }

    RC_EXCEPT_SUCCESS(rc_sdk_uninit());
    return 0;
}

void* test_query_thread(void* arg) {
    LOGI(VD_TAG, "before httpget_other_service_url");
    rc_sleep(1000);  // sleep 1s
    httpget_other_service_url();
    return NULL;
}

int watch_wifi_status_change(int connected) {
    static int access = 0;
    if (connected && !access) {  // wifi status connected
        access = 1;

        char ip[16] = {0};
        rc_get_wifi_local_ip(ip);
        LOGI(VD_TAG, "Local Ip: %s", ip);

        // create new thread because memory
        rc_thread_create(test_query_thread, NULL, NULL);
    }

    return 0;
}