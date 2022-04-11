/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     location.c
 * @author   kuper - kuper@aproton.tech
 * @data     2020-01-22 15:10:46
 * @version  0
 * @brief
 *
 **/

#include "logs.h"
#include "rc_buf.h"
#include "rc_json.h"
#include "wifi.h"
#include "env.h"
#include "location.h"

#define LOC_TAG "[Location]"

int _location_report_retry_intervals[] = {5, 10, 10, 20, 30, 40, 60};

rc_location_t* rc_get_current_location() {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->locmgr != NULL) {
        return &(((rc_location_manager_t*)env->locmgr)->current_location);
    }
    return NULL;
}

int parse_location_result(cJSON* input, rc_location_t* location);

int rc_report_location_wifi(const rc_wifi_ap_t* wifis, int wifi_count,
                            rc_location_t* location) {
    int i;
    char* wifistr = NULL;
    cJSON* input = NULL;
    rc_buf_t response = rc_buf_stack();
    char mac[20] = {0};

    LOGI(LOC_TAG, "rc_report_location");

    BEGIN_JSON_OBJECT(root)
        JSON_OBJECT_ADD_ARRAY(root, wifiList)
        for (i = 0; i < wifi_count; ++i) {
            BEGIN_JSON_ARRAY_ADD_OBJECT_ITEM(wifiList, itm)
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     wifis[i].mac[0], wifis[i].mac[1], wifis[i].mac[2],
                     wifis[i].mac[3], wifis[i].mac[4], wifis[i].mac[5]);
            JSON_OBJECT_ADD_STRING(itm, mac, mac)
            JSON_OBJECT_ADD_STRING(itm, ssid, wifis[i].ssid)
            JSON_OBJECT_ADD_NUMBER(itm, signal, wifis[i].rssi)
            END_JSON_ARRAY_ADD_OBJECT_ITEM(wifiList, itm)
        }
        wifistr = JSON_TO_STRING(root);
    END_JSON_OBJECT(root);

    int rc = rc_http_quark_post("device", "/location/report", wifistr, 5000,
                                &response);
    free(wifistr);  // free string
    if (rc == 200) {
        LOGI(LOC_TAG, "report location success(%s)",
             rc_buf_head_ptr(&response));
        input = cJSON_Parse(rc_buf_head_ptr(&response));
        if (input != NULL) {
            parse_location_result(input, location);
            cJSON_Delete(input);
        }
    }
    rc_buf_free(&response);

    return 0;
}

int rc_report_location() {
    rc_runtime_t* env = get_env_instance();
    rc_wifi_scan_result_t scan_result = {
        .aps = NULL,
        .count = 0,
    };
    rc_location_t location;

    if (env == NULL) {
        return RC_ERROR_SDK_INIT;
    }

    DECLEAR_REAL_VALUE(rc_location_manager_t, mgr, env->locmgr);

    rc_mutex_lock(mgr->mobject);
    if (mgr->is_reporting_location || rc_get_session_token() == NULL) {
        rc_mutex_unlock(mgr->mobject);
        return RC_ERROR_REPEAT_CALL;
    }

    mgr->is_reporting_location = 1;
    rc_mutex_unlock(mgr->mobject);

    int rc = -1;
    if (wifi_manager_scan_ap(env->wifimgr, &scan_result) == 0) {
        rc = rc_report_location_wifi(scan_result.aps, scan_result.count,
                                     &location);
    } else {
        LOGI(LOC_TAG, "scan ap failed");
    }

    rc_mutex_lock(mgr->mobject);
    mgr->is_reporting_location = 0;
    if (rc == 0) {
        mgr->current_location = location;
    }
    rc_mutex_unlock(mgr->mobject);

    return rc;
}

int parse_location_result(cJSON* input, rc_location_t* location) {
    int rc = 0;
    BEGIN_MAPPING_JSON(input, root)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, rc);
        if (rc == 0) {
            JSON_OBJECT_EXTRACT_INT_TO_DOUBLE(root, latitude,
                                              location->latitude)
            JSON_OBJECT_EXTRACT_INT_TO_DOUBLE(root, longitude,
                                              location->longitude)
            location->update_time = time(NULL);
        }
    END_MAPPING_JSON(root);

    return 0;
}

int auto_report_location_time(rc_timer timer, void* dev) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL) {
        DECLEAR_REAL_VALUE(rc_location_manager_t, mgr, env->locmgr);
        if (env->settings.auto_report_location &&
            rc_backoff_algorithm_can_retry(&mgr->location_report_backoff) &&
            network_is_available(env->netmgr, NETWORK_SESSION)) {
            int rc = rc_report_location();
            rc_backoff_algorithm_set_result(&mgr->location_report_backoff,
                                            rc == 0);
        }
    }
    return 0;
}

location_manager location_manager_init(rc_runtime_t* env) {
    rc_location_manager_t* mgr =
        (rc_location_manager_t*)malloc(sizeof(rc_location_manager_t));
    memset(mgr, 0, sizeof(rc_location_manager_t));
    mgr->mobject = rc_mutex_create(NULL);
    rc_backoff_algorithm_init(
        &mgr->location_report_backoff, _location_report_retry_intervals,
        sizeof(_location_report_retry_intervals) / sizeof(int),
        env->settings.location_report_interval);

    if (env->settings.auto_report_location) {
        mgr->location_timer = rc_timer_create(env->timermgr, 3 * 1000, 3 * 1000,
                                              auto_report_location_time, NULL);
    }

    return mgr;
}

int location_manager_uninit(location_manager lm) {
    DECLEAR_REAL_VALUE(rc_location_manager_t, mgr, lm);

    if (mgr->location_timer != NULL) {
        rc_timer_stop(mgr->location_timer);
        mgr->location_timer = NULL;
    }

    if (mgr->mobject != NULL) {
        rc_mutex_destroy(mgr->mobject);
    }

    free(mgr);

    return 0;
}