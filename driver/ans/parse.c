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

#include "service.h"
#include "logs.h"
#include "rc_error.h"
#include "rc_http_request.h"
#include "hashmap.h"
#include "cJSON.h"
#include "rc_json.h"
#include "rc_crypt.h"

char* ans_build_request_json_body(rc_ans_config_t* config)
{
    char* entype = "none";
    char* output = NULL;

    BEGIN_JSON_OBJECT(body)
        if (config->app_id != NULL) {
            JSON_OBJECT_ADD_STRING(body, appId, config->app_id)
        }
        if (config->client_id != NULL) {
            JSON_OBJECT_ADD_STRING(body, clientId, config->client_id)
        }

        JSON_OBJECT_ADD_ITEM(body, services,
                cJSON_CreateStringArray((const char**)config->input_services, config->input_service_count));
        JSON_OBJECT_ADD_STRING(body, encryptType, entype)

        output = JSON_TO_STRING(body);
    END_JSON_OBJECT(body);

    return output;
}

int decrypt_ip_string(rcservice_mgr_t* service_mgr, const char* enip, char* output)
{
    rc_buf_t decrypted = rc_buf_stack();
    rsa_crypt rsa = service_mgr->rsa;
    int ret = rc_rsa_decrypt(rsa, enip, strlen(enip), &decrypted);
    if (ret != RC_SUCCESS) {
        rc_buf_free(&decrypted);
        LOGI(SC_TAG, "decrypt enip failed");
        return -1;
    }

    if (decrypted.length <= 0 || decrypted.length > 15) {
        rc_buf_free(&decrypted);
        LOGI(SC_TAG, "decrypt enip failed, result(%s) is not ip", rc_buf_head_ptr(&decrypted));
        return -1;
    }

    memcpy(output, rc_buf_head_ptr(&decrypted), decrypted.length);
    output[decrypted.length] = '\0';
    rc_buf_free(&decrypted);
    return decrypted.length;
}

int json_to_service(rcservice_mgr_t* service_mgr, cJSON* v, int encrypt_type, rc_service_t** output)
{
    int nips = 0, size, i, len;
    char* p;
    rc_service_t tmp;
    rc_service_t* s;

    BEGIN_MAPPING_JSON(v, root)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, service, tmp.service)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, uri, tmp.uri)
        JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, host, tmp.host)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, port, tmp.port)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, validtm, tmp.validtm)
        JSON_OBJECT_EXTRACT_ARRAY(root, ips)
        nips = cJSON_GetArraySize(JSON(ips));
    END_MAPPING_JSON(root)
    
    if (nips <= 0) nips = 1;

    size = strlen(tmp.service) + 1 + 
        strlen(tmp.uri) + 1 +
        strlen(tmp.host) + 1 +
        (16 + sizeof(char*)) * nips;

    s = (rc_service_t*)rc_malloc(size + sizeof(rc_service_t));

    s->validtm = tmp.validtm;
    s->service = (char*)s + sizeof(rc_service_t) + (nips - 1) * sizeof(char*);
    s->host = s->service + strlen(tmp.service) + 1;
    s->uri = s->host + strlen(tmp.host) + 1;

    strcpy(s->service, tmp.service);
    strcpy(s->host, tmp.host);
    strcpy(s->uri, tmp.uri);

    
    p = s->uri + strlen(tmp.uri) + 1;
    s->ip_count = 0;
    BEGIN_MAPPING_JSON(v, root)
        JSON_OBJECT_EXTRACT_ARRAY(root, ips)
        JSON_ARRAY_FOREACH(ips, ip)
            if (JSON_IS_STRING(JSON(ip))) {
                if (encrypt_type == ANS_ENCRYPT_TYPE_NONE) {
                    s->ips[s->ip_count ++] = p;
                    strcpy(p, JSON(ip)->valuestring);
                    p += strlen(JSON(ip)->valuestring) + 1;
                } else {
                    len = decrypt_ip_string(service_mgr, JSON(ip)->valuestring, p);
                    if (len > 0) {
                        s->ips[s->ip_count ++] = p;
                        p += len + 1;
                    }
                }
            }
        JSON_ARRAY_END_FOREACH(ips, itm)
    END_MAPPING_JSON(root)

    LOGI(SC_TAG, "item={service(%s),uri(%s),host(%s),port(%d),validtm(%d)}",
            s->service, s->uri, s->host, s->port, s->validtm);
    for (i = 0; i < s->ip_count; ++ i) {
        LOGI(SC_TAG, "ip[%d]=%s", i, s->ips[i]);
    }

    *output = s;
    return RC_SUCCESS;
}

int parse_json_config(rcservice_mgr_t* service_mgr, const char* json, int encrypt, map_t* smap)
{
    int rc;
    BEGIN_EXTRACT_JSON(json, root)
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, rc)
        if (rc == 0) {
            JSON_OBJECT_EXTRACT_ARRAY(root, data)
            *smap = hashmap_new();
            JSON_ARRAY_FOREACH(data, itm)
                rc_service_t* s = NULL;
                if (json_to_service(service_mgr, JSON(itm), encrypt, &s) == RC_SUCCESS && s != NULL) {
                    hashmap_put(*smap, s->service, s);
                }
            JSON_ARRAY_END_FOREACH(data, itm)
        }
    END_EXTRACT_JSON(root);

    return RC_SUCCESS;
}

