/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     parse.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-22 22:02:29
 * @version  0
 * @brief
 *
 **/

#include "hashmap.h"
#include "logs.h"
#include "rc_crypt.h"
#include "rc_error.h"
#include "rc_http_request.h"
#include "rc_json.h"
#include "service.h"

char* ans_build_request_json_body(const char* app_id, const char* device_id) {
    const char* services[] = {DEFAULT_SERVICE};
    char* output = NULL;

    BEGIN_JSON_OBJECT(body)
        if (app_id != NULL) {
            JSON_OBJECT_ADD_STRING(body, appId, app_id)
        }
        if (device_id != NULL) {
            JSON_OBJECT_ADD_STRING(body, deviceId, device_id)
        }

        JSON_OBJECT_ADD_NUMBER(body, timestamp, (int)time(NULL))

        JSON_OBJECT_ADD_ITEM(
            body, services,
            cJSON_CreateStringArray(services,
                                    sizeof(services) / sizeof(const char*)));

        output = JSON_TO_STRING(body);
    END_JSON_OBJECT(body);

    return output;
}

int json_to_service(cJSON* v, const char* service_name, rc_service_t** output) {
    int nips = 0, nprotocols = 0, size, i, len;
    char* p;
    rc_service_t tmp;
    rc_service_t* s;

    rc_service_protocol_t* protocol_ptr = NULL;

    tmp.service = (char*)service_name;
    tmp.validtm = 0;

    BEGIN_MAPPING_JSON(v, root)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, prefix, tmp.prefix)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, host, tmp.host)
        JSON_OBJECT_EXTRACT_ARRAY(root, ips)
        nips = cJSON_GetArraySize(JSON(ips));

        JSON_OBJECT_EXTRACT_OBJECT_BEGIN(root, protocols)
        nprotocols = cJSON_GetArraySize(JSON(protocols));
        JSON_OBJECT_EXTRACT_OBJECT_END(root)
    END_MAPPING_JSON(root)

    if (nips <= 0) nips = 1;

    size = strlen(tmp.service) + 1 + strlen(tmp.prefix) + 1 + strlen(tmp.host) +
           1 + (16 + sizeof(char*)) * nips +
           sizeof(rc_service_protocol_t) * nprotocols;
    size = ((size + 1) / 2) * 2;

    s = (rc_service_t*)rc_malloc(size + sizeof(rc_service_t));

    s->protocols = hashmap_new();

    protocol_ptr = (rc_service_protocol_t*)(&s[1]);

    char* next_pos =
        (char*)protocol_ptr + sizeof(rc_service_protocol_t) * nprotocols;

    s->validtm = tmp.validtm;
    s->service = next_pos + (nips - 1) * sizeof(char*);
    s->host = s->service + strlen(tmp.service) + 1;
    s->prefix = s->host + strlen(tmp.host) + 1;

    strcpy(s->service, tmp.service);
    strcpy(s->host, tmp.host);
    strcpy(s->prefix, tmp.prefix);

    p = s->prefix + strlen(tmp.prefix) + 1;
    s->ip_count = 0;
    BEGIN_MAPPING_JSON(v, root)
        JSON_OBJECT_EXTRACT_ARRAY(root, ips)
        JSON_ARRAY_FOREACH(ips, ip)
            if (JSON_IS_STRING(JSON(ip))) {
                s->ips[s->ip_count++] = p;
                strcpy(p, JSON(ip)->valuestring);
                p += strlen(JSON(ip)->valuestring) + 1;
            }
        JSON_ARRAY_END_FOREACH(ips, ip)

        JSON_OBJECT_EXTRACT_OBJECT_BEGIN(root, protocols)
        JSON_ARRAY_FOREACH(protocols, protocol)
            if (JSON(protocol)->string != NULL &&
                strlen(JSON(protocol)->string) < 10 &&
                JSON(protocol)->type == cJSON_Number) {
                strcpy(protocol_ptr->protocol, JSON(protocol)->string);
                protocol_ptr->port = JSON(protocol)->valueint;

                hashmap_put(s->protocols, protocol_ptr->protocol, protocol_ptr);

                ++protocol_ptr;
            }
        JSON_ARRAY_END_FOREACH(protocols, protocol)
        JSON_OBJECT_EXTRACT_OBJECT_END(root)

    END_MAPPING_JSON(root)

    LOGI(SC_TAG, "item={service(%s),prefix(%s),host(%s),validtm(%d)}",
         s->service, s->prefix, s->host, s->validtm);
    for (i = 0; i < s->ip_count; ++i) {
        LOGI(SC_TAG, "ip[%d]=%s", i, s->ips[i]);
    }

    *output = s;
    return RC_SUCCESS;
}

int parse_json_config(const char* str, map_t* smap) {
    int rc;
    BEGIN_EXTRACT_JSON(str, root)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, rc)
        if (rc == 0) {
            *smap = hashmap_new();
            JSON_OBJECT_EXTRACT_OBJECT_BEGIN(root, default)

            rc_service_t* s = NULL;
            if (json_to_service(JSON(default), DEFAULT_SERVICE, &s) ==
                    RC_SUCCESS &&
                s != NULL) {
                hashmap_put(*smap, s->service, s);
            }

            JSON_OBJECT_EXTRACT_OBJECT_END(root)
        }
    END_EXTRACT_JSON(root);

    return RC_SUCCESS;
}
