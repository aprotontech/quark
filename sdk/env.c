/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     env.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-19 18:20:38
 * @version  0
 * @brief
 *
 **/

#include "env.h"

#include "property.h"
#include "location.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_system.h"
#elif defined(__QUARK_LINUX__)
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#define ENV_BUFF_SIZE 64
#define ANS_QUERY_PATH "/device/dns"

rc_runtime_t* _env;
void env_free(rc_runtime_t* env);

int sync_server_time(rc_timer timer, void* dev);
int rc_enable_ntp_sync_time();
int device_regist(rc_runtime_t* env);
int append_hardware_info(rc_runtime_t* env);
int auto_report_location_time(rc_timer timer, void* dev);

int quark_on_wifi_status_changed(wifi_manager mgr, int wifi_status);

rc_runtime_t* get_env_instance() { return _env; }

int rc_sdk_init(rc_settings_t* settings) {
    rc_runtime_t* env = NULL;

    if (_env != NULL) {
        LOGW(SDK_TAG, "quark sdk had init");
        return RC_ERROR_SDK_INIT;
    }

    LOGW(SDK_TAG, "quark sdk start to init");

    env = (rc_runtime_t*)rc_malloc(sizeof(rc_runtime_t) + ENV_BUFF_SIZE);
    memset(env, 0, sizeof(rc_runtime_t));
    _env = env;

    env->buff = rc_buf_stack();
    env->buff.total = ENV_BUFF_SIZE;  // expand buffer size

    memcpy(&env->settings, settings, sizeof(env->settings));
    // env->time_update = settings->time_update;

    if (env->settings.service_url != NULL) {  // use input url
        env->local.default_service_url = env->settings.service_url;
    } else {
        env->local.default_service_url = QUARK_API_URL;
    }

    if (env->settings.enable_ntp_time_sync) {
        rc_enable_ntp_sync_time();
    }

    // hardware info
    append_hardware_info(env);

    env->httpmgr = http_manager_init();
    if (env->httpmgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, http manager init failed");
        return RC_ERROR_SDK_INIT;
    }

    env->timermgr = rc_timer_manager_init();
    if (env->timermgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, timer manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    if (env->time_update != NULL) {
        int randtm = (rand() % 1800) + 7200;
        env->sync_timer = rc_timer_create(env->timermgr, 1000, randtm * 1000,
                                          sync_server_time, env);
    }

    env->netmgr = network_manager_init(0);
    if (env->netmgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, net manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    env->wifimgr = wifi_manager_init(quark_on_wifi_status_changed);
    if (env->wifimgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, wifi manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    {  // init ans service
        char url[100] = {0};
        snprintf(url, sizeof(url), "%s%s", env->local.default_service_url,
                 ANS_QUERY_PATH);
        env->ansmgr =
            rc_service_init(env->settings.app_id, env->settings.uuid, url,
                            env->httpmgr, env->timermgr, env->netmgr);
    }

    if (env->ansmgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, ans manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    // load local config
    if (env->settings.default_svr_config != NULL) {
        rc_service_local_config(env->ansmgr, env->settings.default_svr_config);
    }

    ///////////////////////////////////////////////////
    // DEVICE
    env->device =
        rc_device_init(env->httpmgr, (rc_hardware_info*)env->settings.hardware);

    if (env->device == NULL) {
        LOGI(SDK_TAG, "sdk init failed, device manager init failed");
        return RC_ERROR_SDK_INIT;
    }

    ///////////////////////////////////////////////////
    // PROPERTY
    env->properties =
        property_manager_init(env, settings->property_change_report,
                              settings->porperty_retry_interval);
    if (env->properties == NULL) {
        LOGE(SDK_TAG, "property_manager_init failed");
        return RC_ERROR_SDK_INIT;
    }

    // define localip property
    rc_property_define("localIp", RC_PROPERTY_STRING_VALUE, NULL, NULL);

    env->locmgr = location_manager_init(env);
    if (env->locmgr == NULL) {
        LOGI(SDK_TAG, "sdk init failed, location manager init failed");
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    if (device_regist(env) != 0) {
        env_free(env);
        return RC_ERROR_SDK_INIT;
    }

    return 0;
}

int rc_sdk_uninit() {
    if (_env == NULL) {
        LOGW(SDK_TAG, "sdk uninit failed, had not inited");
        return RC_ERROR_SDK_UNINIT;
    }
    env_free(_env);
    _env = NULL;
    return 0;
}

int append_hardware_info(rc_runtime_t* env) {
    if (env->settings.hardware == NULL) {
        env->settings.hardware = rc_malloc(sizeof(rc_hardware_info));
        memset(env->settings.hardware, 0, sizeof(rc_hardware_info));
    }

    // get mac address
    char* mac_str = rc_buf_tail_ptr(&env->buff);
#if defined(__QUARK_FREERTOS__)
    {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(mac_str, RC_BUF_LEFT_SIZE(&env->buff), "%x:%x:%x:%x:%x:%x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        env->buff.length += 20;  // skip mac
    }
#elif defined(__QUARK_LINUX__)
    {
        int fd, interface;
        struct ifreq buf[3];
        struct ifconf ifc;

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
            int i = 0;
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = (caddr_t)buf;
            if (!ioctl(fd, SIOCGIFCONF, (char*)&ifc)) {
                interface = ifc.ifc_len / sizeof(struct ifreq);
                while (i < interface) {
                    if (!(ioctl(fd, SIOCGIFHWADDR, (char*)&buf[i]))) {
                        snprintf(mac_str, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[0],
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[1],
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[2],
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[3],
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[4],
                                 (unsigned char)buf[i].ifr_hwaddr.sa_data[5]);
                        if (strcmp(mac_str, "00:00:00:00:00:00") != 0) {
                            env->buff.length += 20;  // skip mac
                            LOGI(SDK_TAG, "mac=%s", mac_str);
                            break;
                        }
                    }
                    i++;
                }
            }
        }
    }
#endif

    rc_hardware_info* hi = (rc_hardware_info*)env->settings.hardware;
    if (hi->mac == NULL) hi->mac = mac_str;
    if (hi->bid == NULL) hi->bid = mac_str;
    if (hi->cpu == NULL) hi->cpu = mac_str;

    if (env->settings.uuid == NULL) {
        env->settings.uuid = mac_str;
    }

    return 0;
}

void env_free(rc_runtime_t* env) {
    if (env->timermgr != NULL) {  // stop all timer first
        rc_timer_manager_stop_world(env->timermgr);
    }

    if (env->mqtt != NULL) {
        rc_mqtt_close(env->mqtt);
        env->mqtt = NULL;
    }

    if (env->device != NULL) {
        rc_device_uninit(env->device);
        env->device = NULL;
    }

    if (env->properties != NULL) {
        property_manager_uninit(env->properties);
        env->properties = NULL;
    }

    if (env->netmgr != NULL) {
        network_manager_uninit(env->netmgr);
        env->netmgr = NULL;
    }

    if (env->ansmgr != NULL) {
        rc_service_uninit(env->ansmgr);
        env->ansmgr = NULL;
    }

    if (env->httpmgr != NULL) {
        http_manager_uninit(env->httpmgr);
        env->httpmgr = NULL;
    }

    if (env->timermgr != NULL) {
        rc_timer_manager_uninit(env->timermgr);
        env->timermgr = NULL;
    }

    if (env->wifimgr != NULL) {
        wifi_manager_uninit(env->wifimgr);
        env->wifimgr = NULL;
    }

    if (env->locmgr != NULL) {
        location_manager_uninit(env->locmgr);
        env->locmgr = NULL;
    }

    if (env == _env) {
        _env = NULL;
    }

    rc_free(env);
}
