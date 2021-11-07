/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_json.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-15 23:39:31
 * @version  0
 * @brief
 *
 **/

#ifndef _RC_JSON_H_
#define _RC_JSON_H_

#include "cJSON.h"

#define JSON(key) _json_##key

#define JSON_IS_STRING(ip) \
    (ip != NULL && ip->type == cJSON_String && ip->valuestring != NULL)

#define BEGIN_EXTRACT_JSON(str, root) \
    { \
        cJSON *_json_##root = cJSON_Parse(str); \
        if (_json_##root == NULL) { \
            LOGI("JSON", "parse json failed, input(%s) is not json", str); \
            return RC_ERROR_JSON_FORMAT; \
        }

#define END_EXTRACT_JSON(root) \
        cJSON_Delete(_json_##root); \
    }
        
#define BEGIN_MAPPING_JSON(input, root) \
    { \
        cJSON* JSON(root) = input;

#define END_MAPPING_JSON(root) \
    }

#define JSON_OBJECT_EXTRACT_ITEM(root, key, value_type) \
    cJSON* _json_##key = cJSON_GetObjectItem(_json_##root, #key); \
    if (_json_##key == NULL || _json_##key->type != cJSON_##value_type) { \
        LOGI("JSON", "%s is not found or is not %s", #key, #value_type); \
        return RC_ERROR_JSON_FORMAT; \
    } \

#define JSON_OBJECT_EXTRACT_OBJECT_BEGIN(root, key) \
    JSON_OBJECT_EXTRACT_ITEM(root, key, Object)
    
#define JSON_OBJECT_EXTRACT_OBJECT_END(root) 

#define JSON_OBJECT_EXTRACT_STRING(root, key) \
    JSON_OBJECT_EXTRACT_ITEM(root, key, String) \
    if (_json_##key->valuestring == NULL) { \
        LOGI("JSON", "%s valuestring == null", #key); \
        return RC_ERROR_JSON_FORMAT; \
    }

#define JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, key, value) \
    JSON_OBJECT_EXTRACT_STRING(root, key); \
    value = _json_##key->valuestring;

#define JSON_OBJECT_EXTRACT_INT(root, key) \
    JSON_OBJECT_EXTRACT_ITEM(root, key, Number)

#define JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, key, value) \
    JSON_OBJECT_EXTRACT_INT(root, key); \
    value = _json_##key->valueint;

#define JSON_OBJECT_EXTRACT_INT_TO_DOUBLE(root, key, value) \
    JSON_OBJECT_EXTRACT_INT(root, key); \
    value = _json_##key->valuedouble;

#define JSON_OBJECT_EXTRACT_BOOL(root, key) \
    cJSON* _json_##key = cJSON_GetObjectItem(_json_##root, #key); \
    if (_json_##key == NULL || (_json_##key->type != cJSON_True && _json_##key->type != cJSON_False)) { \
        LOGI("JSON", "%s is not found or is not bool", #key); \
        return RC_ERROR_JSON_FORMAT; \
    } \

#define JSON_OBJECT_EXTRACT_BOOL_TO_VALUE(root, key, value) \
    JSON_OBJECT_EXTRACT_BOOL(root, key); \
    value = _json_##key->type == cJSON_True;

#define JSON_OBJECT_EXTRACT_ARRAY(root, key) \
    JSON_OBJECT_EXTRACT_ITEM(root, key, Array);

#define JSON_ARRAY_FOREACH(arr, itm) \
    { \
        int _iter_##arr = 0; \
        int _count##arr = cJSON_GetArraySize(JSON(arr)); \
        for (_iter_##arr = 0; _iter_##arr < _count##arr; ++ _iter_##arr) { \
            cJSON* JSON(itm) = cJSON_GetArrayItem(JSON(arr), _iter_##arr);

#define JSON_ARRAY_END_FOREACH(arr, itm) \
        } \
    }

#define BEGIN_JSON_OBJECT(root) \
    { \
        cJSON* _json_##root = cJSON_CreateObject();

#define END_JSON_OBJECT(root) \
        cJSON_Delete(_json_##root); \
    }


#define END_JSON_OBJECT_ITEM(root) \
    }

#define JSON_OBJECT_ADD_STRING(root, key, value) \
    cJSON_AddStringToObject(_json_##root, #key, value); 

#define JSON_OBJECT_ADD_NUMBER(root, key, value) \
    cJSON_AddNumberToObject(_json_##root, #key, value); 

#define JSON_OBJECT_ADD_OBJECT(root, key) \
    BEGIN_JSON_OBJECT(key) \
    cJSON_AddItemToObject(_json_##root, #key, _json_##key);

#define FREE_JSON_OBJECT(root) \
    cJSON_Delete(_json_##root);

#define JSON_TO_STRING(root) \
    cJSON_PrintUnformatted(_json_##root)

#define JSON_OBJECT_ADD_ITEM(root, key, value) \
    cJSON_AddItemToObject(JSON(root), #key, value); 

#define JSON_OBJECT_ADD_ARRAY(root, arr) \
    cJSON* JSON(arr) = cJSON_CreateArray(); \
    cJSON_AddItemToObject(JSON(root), #arr, JSON(arr)); 

#define BEGIN_JSON_ARRAY_ADD_OBJECT_ITEM(arr, itm) \
    { \
        cJSON* JSON(itm) = cJSON_CreateObject(); \
        cJSON_AddItemToArray(JSON(arr), JSON(itm));

#define END_JSON_ARRAY_ADD_OBJECT_ITEM(arr, itm) \
    }

#define QUARK_API_RESPONSE_BEGIN(root, result_rc) \
    { \
        JSON_OBJECT_EXTRACT_INT_TO_VALUE(root, rc, result_rc); \
        if (result_rc == 200) {

#define QUARK_API_RESPONSE_END(root) \
        } \
    }
#endif
