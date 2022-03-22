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
#include "rc_location.h"
#include "quark/quark.h"

#define LOC_TAG "[Location]"

rc_location_t _current_location;

rc_location_t* rc_get_current_location() { return &_current_location; }

int parse_location_result(cJSON* input, rc_location_t* location);

int rc_report_location(const rc_wifi_info_t* wifis, int wifi_count) {
    int i;
    char* wifistr = NULL;
    cJSON* input = NULL;
    rc_buf_t response = rc_buf_stack();
    LOGI(LOC_TAG, "rc_report_location");

    BEGIN_JSON_OBJECT(root)
        JSON_OBJECT_ADD_ARRAY(root, wifiList)
        for (i = 0; i < wifi_count; ++i) {
            BEGIN_JSON_ARRAY_ADD_OBJECT_ITEM(wifiList, itm)
            JSON_OBJECT_ADD_STRING(itm, mac, wifis[i].mac)
            JSON_OBJECT_ADD_STRING(itm, ssid, wifis[i].ssid)
            JSON_OBJECT_ADD_NUMBER(itm, signal, wifis[i].signal)
            END_JSON_ARRAY_ADD_OBJECT_ITEM(wifiList, itm)
        }
        wifistr = JSON_TO_STRING(root);
    END_JSON_OBJECT(root);

    int rc =
        rc_http_quark_post("api", "/location/report", wifistr, 5000, &response);
    free(wifistr);  // free string
    if (rc == 200) {
        LOGI(LOC_TAG, "report location success(%s)",
             rc_buf_head_ptr(&response));
        input = cJSON_Parse(rc_buf_head_ptr(&response));
        if (input != NULL) {
            parse_location_result(input, &_current_location);
            cJSON_Delete(input);
        }
    }
    rc_buf_free(&response);

    return 0;
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
        }
    END_MAPPING_JSON(root);

    return 0;
}
