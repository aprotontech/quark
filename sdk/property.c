/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     property.c
 * @author   kuper - kuper@aproton.tech
 * @data     2020-02-02 14:46:06
 * @version  0
 * @brief
 *
 **/

#include "property.h"

#include "env.h"
#include "logs.h"

#define PROPERTY_TAG "[PROPERTY]"
#define GETTIMESTAMP(a)                           \
    do {                                          \
        struct timeval tv;                        \
        gettimeofday(&tv, NULL);                  \
        a = tv.tv_sec * 1000 + tv.tv_usec / 1000; \
    } while (0)

int on_property_timer(rc_timer timer, void* usr_data);
int find_change_item(any_t attrs, const char* key, any_t value);
int clear_change_state(any_t params, const char* key, any_t value);
char* format_query_post(rc_property_manager* mgr, const char* source,
                        cJSON** names);
int on_remote_set_property(const char* key, cJSON* data, rc_buf_t* response);
extern int free_hash_item(any_t n, const char* key, any_t val);

int rc_property_value_check(int type, void* value, rc_property_t* ppty) {
    int r = 0;
    switch (type) {
    case RC_PROPERTY_INT_VALUE:
        if (ppty->int_value != *(int*)value) {
            ppty->int_value = *(int*)value;
            r = 1;
        }
        break;
    case RC_PROPERTY_STRING_VALUE:
        if (ppty->string_value != NULL &&
            strcmp(ppty->string_value, value) == 0) {
            r = 0;
            break;
        }

        if (ppty->string_value != NULL) {
            rc_free(ppty->string_value);
        }

        ppty->string_value = (char*)rc_malloc(strlen(value) + 1);
        if (ppty->string_value == NULL) {
            LOGW(PROPERTY_TAG, "ppty->string_value rc_malloc failed");
            return -1;
        }

        strcpy(ppty->string_value, value);
        r = 1;
        break;
    case RC_PROPERTY_DOUBLE_VALUE:
        if (ppty->double_value != *(double*)value) {
            ppty->double_value = *(double*)value;
            r = 1;
        }
        break;
    case RC_PROPERTY_BOOL_VALUE:
        if (ppty->bool_value != *(int*)value) {
            ppty->bool_value = *(int*)value;
            r = 1;
        }
        break;
    default:
        break;
    }

    return r;
}

int rc_property_define(const char* name, int type, void* init_value,
                       rc_property_change on_change) {
    any_t propertydis;
    rc_property_t* ppty = NULL;
    rc_runtime_t* env = get_env_instance();

    if (env == NULL || name == NULL) {
        LOGW(PROPERTY_TAG, "input params is invalidate");
        return -1;
    }

    if ((strlen(name) + 1) > MAX_PROPERTY_NAME) {
        LOGW(PROPERTY_TAG, "the property name is too long");
        return -1;
    }

    DECLEAR_REAL_VALUE(rc_property_manager, mgr, env->properties);
    rc_mutex_lock(mgr->mgr_mutex);
    if (mgr->values) {
        if (hashmap_get(mgr->values, (char*)name, &propertydis) == MAP_OK) {
            LOGI(PROPERTY_TAG, "this %s key is defined", name);
        } else {
            ppty = (rc_property_t*)rc_malloc(sizeof(rc_property_t));
            if (ppty == NULL) {
                LOGW(PROPERTY_TAG, "rc_property_define rc_malloc failed");
                rc_mutex_unlock(mgr->mgr_mutex);
                return -1;
            }
            memset(ppty, 0, sizeof(rc_property_t));
            if (init_value != NULL) {
                if (rc_property_value_check(type, init_value, ppty) == -1) {
                    rc_free(ppty);
                    ppty = NULL;
                    rc_mutex_unlock(mgr->mgr_mutex);
                    return -1;
                }
                ppty->changed = 1;
                GETTIMESTAMP(ppty->change_timestamp);
            } else {
                ppty->notset = 1;
            }
            strcpy(ppty->name, name);
            ppty->type = type;
            ppty->callback = on_change;
            hashmap_put(mgr->values, ppty->name, ppty);
        }
    }
    rc_mutex_unlock(mgr->mgr_mutex);
    return 0;
}

int rc_property_set(const char* name, void* value) {
    int ischanged = 0;
    any_t propertydis;
    rc_property_t* ppty = NULL;
    rc_runtime_t* env = get_env_instance();

    if (env == NULL || name == NULL || value == NULL) {
        LOGW(PROPERTY_TAG, "input params is invalidate");
        return -1;
    }

    DECLEAR_REAL_VALUE(rc_property_manager, mgr, env->properties);
    rc_mutex_lock(mgr->mgr_mutex);
    if (mgr->values) {
        if (hashmap_get(mgr->values, (char*)name, &propertydis) != MAP_OK) {
            rc_mutex_unlock(mgr->mgr_mutex);
            LOGI(PROPERTY_TAG, "this %s key is not defined", name);
            return -1;
        }
        ppty = (rc_property_t*)propertydis;
        ischanged = rc_property_value_check(ppty->type, value, ppty);
        if (ischanged == -1) {
            rc_mutex_unlock(mgr->mgr_mutex);
            return -1;
        }
        if (ppty->notset == 1) {
            ppty->changed = 1;
            ppty->notset = 0;
        } else if (ppty->changed == 0) {
            ppty->changed = ischanged;
        }
        if (ppty->changed == 1) {
            mgr->findchangeditem = 1;
            GETTIMESTAMP(ppty->change_timestamp);
        }
    }

    rc_mutex_unlock(mgr->mgr_mutex);

    if (mgr->findchangeditem && mgr->property_change_report) {
        LOGI(SDK_TAG, "found property change to report");
        rc_timer_ahead_once(mgr->property_timer, 100);
    }

    return 0;
}

int rc_property_flush() {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL) {
        DECLEAR_REAL_VALUE(rc_property_manager, mgr, env->properties);
        rc_timer_ahead_once(mgr->property_timer, 10);
    }
    return 0;
}

property_manager property_manager_init(rc_runtime_t* env,
                                       int property_change_report,
                                       int porperty_retry_interval) {
    rc_property_manager* mgr =
        (rc_property_manager*)rc_malloc(sizeof(rc_property_manager));
    mgr->findchangeditem = 0;
    mgr->property_change_report = property_change_report;
    mgr->porperty_retry_interval = porperty_retry_interval;
    mgr->mgr_mutex = rc_mutex_create(NULL);
    mgr->values = hashmap_new();
    mgr->property_timer =
        rc_timer_create(env->timermgr, porperty_retry_interval,
                        porperty_retry_interval, on_property_timer, mgr);

    return mgr;
}

int property_manager_uninit(property_manager manager) {
    rc_property_manager* mgr = (rc_property_manager*)manager;
    if (mgr->values) {
        hashmap_iterate(mgr->values, free_hash_item, NULL);
        hashmap_free(mgr->values);
    }
    rc_mutex_destroy(mgr->mgr_mutex);
    rc_timer_stop(mgr->property_timer);
    rc_free(manager);
    mgr = NULL;

    return 0;
}

int on_property_timer(rc_timer timer, void* usr_data) {
    long long now = 0;
    char isreportsuccess = 0;
    any_t propertydis;
    char* str = NULL;
    rc_buf_t response = rc_buf_stack();
    void* params[2] = {&now, &isreportsuccess};

    DECLEAR_REAL_VALUE(rc_property_manager, mgr, usr_data);

    if (rc_get_client_id() == NULL || rc_get_session_token() == NULL) {
        // client or session is empty, device is not registed, so retry later
        LOGD(SDK_TAG, "device is not registed, so skip report attr");
        rc_timer_ahead_once(mgr->property_timer, 1000);
        return 0;
    }

    if (mgr->findchangeditem != 0 && mgr->values != NULL) {
        rc_mutex_lock(mgr->mgr_mutex);
        GETTIMESTAMP(now);
        BEGIN_JSON_OBJECT(root)
            JSON_OBJECT_ADD_STRING(root, clientId, rc_get_client_id())
            JSON_OBJECT_ADD_OBJECT(root, attrs)
            hashmap_iterate(mgr->values, find_change_item, JSON(attrs));
            END_JSON_OBJECT_ITEM(attrs)
            JSON_OBJECT_ADD_NUMBER(root, timestamp, now);
            str = JSON_TO_STRING(root);
        END_JSON_OBJECT(root);
        rc_mutex_unlock(mgr->mgr_mutex);
        LOGI(PROPERTY_TAG, "ppty json string %s", str);

        int rc =
            rc_http_quark_post("device", "/device/attrs", str, 5000, &response);
        free(str);
        if (rc == 200) {
            LOGI(PROPERTY_TAG, "property report success(%s)",
                 rc_buf_head_ptr(&response));
            isreportsuccess = 1;
            mgr->findchangeditem = 0;
        }
        rc_buf_free(&response);

        rc_mutex_lock(mgr->mgr_mutex);
        hashmap_iterate(mgr->values, clear_change_state, (any_t)params);
        rc_mutex_unlock(mgr->mgr_mutex);
    }

    return 0;
}

int find_change_item(any_t attrs, const char* key, any_t value) {
    rc_property_t* ppty = (rc_property_t*)value;

    if (ppty->changed == 0) {
        return 0;
    }

    ppty->status = 1;
    switch (ppty->type) {
    case RC_PROPERTY_INT_VALUE:
        cJSON_AddItemToObject((cJSON*)attrs, ppty->name,
                              cJSON_CreateNumber(ppty->int_value));
        break;
    case RC_PROPERTY_STRING_VALUE:
        if (ppty->string_value != NULL) {
            cJSON_AddItemToObject((cJSON*)attrs, ppty->name,
                                  cJSON_CreateString(ppty->string_value));
        }
        break;
    case RC_PROPERTY_DOUBLE_VALUE:
        cJSON_AddItemToObject((cJSON*)attrs, ppty->name,
                              cJSON_CreateNumber(ppty->double_value));
        break;
    case RC_PROPERTY_BOOL_VALUE:
        cJSON_AddItemToObject((cJSON*)attrs, ppty->name,
                              cJSON_CreateNumber(ppty->bool_value));
        break;
    default:
        break;
    }

    return 0;
}

int clear_change_state(any_t p, const char* key, any_t value) {
    rc_property_t* ppty = (rc_property_t*)value;
    void** params = (void**)p;
    int reporttime = *(int*)params[0];
    char reportresult = *(char*)params[1];

    if (ppty->status == 1) {
        ppty->status = 0;
        if (reportresult != 0 && ppty->changed == 1 &&
            ppty->change_timestamp <= reporttime) {
            ppty->changed = 0;
        }
    }

    return 0;
}

int on_remote_set_property(const char* key, cJSON* JSON(data),
                           rc_buf_t* response) {
    any_t propertydis;
    rc_property_t* ppty = NULL;
    rc_runtime_t* env = get_env_instance();

    if (env == NULL || JSON(data) == NULL) {
        LOGW(PROPERTY_TAG, "input params is invalidate");
        return -1;
    }

    DECLEAR_REAL_VALUE(rc_property_manager, mgr, env->properties);

    rc_mutex_lock(mgr->mgr_mutex);
    JSON_ARRAY_FOREACH(data, itm)
        if (JSON(itm)->string != NULL &&
            hashmap_get(mgr->values, JSON(itm)->string, &propertydis) ==
                MAP_OK &&
            propertydis != NULL) {
            ppty = (rc_property_t*)propertydis;
            if (ppty->callback != NULL) {
                switch (ppty->type) {
                case RC_PROPERTY_INT_VALUE:
                case RC_PROPERTY_DOUBLE_VALUE:
                    if (JSON(itm)->type != cJSON_Number) {
                        LOGI(PROPERTY_TAG,
                             "key(%s), server push type is not match local "
                             "define(int)",
                             ppty->name);
                        continue;
                    }
                    rc_mutex_unlock(mgr->mgr_mutex);
                    ppty->callback(ppty->name, ppty->type,
                                   ppty->type == RC_PROPERTY_INT_VALUE
                                       ? (void*)&JSON(itm)->valueint
                                       : (void*)&JSON(itm)->valuedouble);
                    rc_mutex_lock(mgr->mgr_mutex);
                    break;
                case RC_PROPERTY_BOOL_VALUE:
                    if (JSON(itm)->type != cJSON_True &&
                        JSON(itm)->type != cJSON_False) {
                        LOGI(PROPERTY_TAG,
                             "key(%s), server push type is not match local "
                             "define(bool)",
                             ppty->name);
                        continue;
                    }
                    rc_mutex_unlock(mgr->mgr_mutex);
                    ppty->callback(ppty->name, ppty->type,
                                   &JSON(itm)->valueint);
                    rc_mutex_lock(mgr->mgr_mutex);
                    break;
                case RC_PROPERTY_STRING_VALUE:
                    if (JSON(itm)->type != cJSON_String) {
                        LOGI(PROPERTY_TAG,
                             "key(%s), server push type is not match local "
                             "define(string)",
                             ppty->name);
                        continue;
                    }
                    rc_mutex_unlock(mgr->mgr_mutex);
                    ppty->callback(ppty->name, ppty->type,
                                   &JSON(itm)->valuestring);
                    rc_mutex_lock(mgr->mgr_mutex);
                    break;
                }
            }
        }
    JSON_ARRAY_END_FOREACH(data, itm)
    rc_mutex_unlock(mgr->mgr_mutex);
    return 0;
}

int parse_attrs_get_result(rc_property_manager* mgr, cJSON* paramresult,
                           va_list ap, cJSON* JSON(names)) {
    int i = 0;
    int apirc = 0;
    cJSON* json = NULL;
    char* key = NULL;
    any_t propertydis;
    rc_property_t* ppty = NULL;

    BEGIN_MAPPING_JSON(paramresult, root)
        QUARK_API_RESPONSE_BEGIN(root, apirc);
        JSON_OBJECT_EXTRACT_OBJECT_BEGIN(root, attrs);
        JSON_ARRAY_FOREACH(names, key)
            json = NULL;
            key = JSON(key)->valuestring;
            if (hashmap_get(mgr->values, key, &propertydis) != MAP_OK ||
                propertydis != NULL) {
                LOGE(PROPERTY_TAG,
                     "key(%s) is not defined, must not reach here", key);
                return -1;
            }

            ppty = (rc_property_t*)propertydis;
            json = cJSON_GetObjectItem(JSON(attrs), key);

            if (json == NULL) {
                LOGI(PROPERTY_TAG, "The key(%s) not found in server", key);
                return -1;
            }

            switch (ppty->type) {
            case RC_PROPERTY_INT_VALUE:
                if (json->type != cJSON_Number) {
                    LOGI(PROPERTY_TAG,
                         "key(%s), server result type is not match local "
                         "define(int)",
                         key);
                    return -1;
                }
                *(va_arg(ap, int*)) = json->valueint;
                break;
            case RC_PROPERTY_STRING_VALUE:
                if (json->type != cJSON_String) {
                    LOGI(PROPERTY_TAG,
                         "key(%s), server result type is not match local "
                         "define(string)",
                         key);
                    return -1;
                }
                *(va_arg(ap, char**)) = rc_copy_string(json->valuestring);
                break;
            case RC_PROPERTY_DOUBLE_VALUE:
                if (json->type != cJSON_Number) {
                    LOGI(PROPERTY_TAG,
                         "key(%s), server result type is not match local "
                         "define(double)",
                         key);
                    return -1;
                }
                *(va_arg(ap, double*)) = json->valuedouble;
                break;
            case RC_PROPERTY_BOOL_VALUE:
                if (json->type != cJSON_True && json->type != cJSON_False) {
                    LOGI(PROPERTY_TAG,
                         "key(%s), server result type is not match local "
                         "define(bool)",
                         key);
                    return -1;
                }
                *(va_arg(ap, int*)) = json->valueint;
                break;
            default:
                break;
            }
        JSON_ARRAY_END_FOREACH(names, key);
        QUARK_API_RESPONSE_END(root)
    END_MAPPING_JSON(root);

    return 0;
}

int rc_property_get_values(const char* keys, ...) {
    int r = -1;
    va_list ap;
    char* sendstr = NULL;
    cJSON* arr = NULL;
    rc_buf_t response = rc_buf_stack();
    rc_runtime_t* env = get_env_instance();

    if (env == NULL || keys == NULL || strlen(keys) == 0) {
        LOGW(PROPERTY_TAG, "input params is invalidate");
        return -1;
    }

    DECLEAR_REAL_VALUE(rc_property_manager, mgr, env->properties);

    rc_mutex_lock(mgr->mgr_mutex);
    sendstr = format_query_post(mgr, keys, &arr);
    rc_mutex_unlock(mgr->mgr_mutex);

    LOGI(PROPERTY_TAG, "get json is %s", sendstr);
    if (sendstr == NULL || arr == NULL) {
        if (arr != NULL) cJSON_Delete(arr);
        return -1;
    }

    int post_rc =
        rc_http_quark_post("DEVICE", "/attrs/get", sendstr, 5000, &response);
    free(sendstr);
    if (post_rc == 200) {
        LOGI(PROPERTY_TAG, "property  get report success(%s)",
             rc_buf_head_ptr(&response));
        cJSON* paramresult = cJSON_Parse(rc_buf_head_ptr(&response));
        if (paramresult != NULL) {
            va_start(ap, keys);

            rc_mutex_lock(mgr->mgr_mutex);
            r = parse_attrs_get_result(mgr, paramresult, ap, arr);
            rc_mutex_unlock(mgr->mgr_mutex);

            va_end(ap);
            cJSON_Delete(paramresult);
        }
    }

    cJSON_Delete(arr);
    rc_buf_free(&response);
    return r;
}

cJSON* get_param_keyArray(rc_property_manager* mgr, const char* keys) {
    char *strforcpy = NULL, *p = NULL;
    const char** names = NULL;
    int i = 0;
    int count = 0;
    cJSON* arr = NULL;
    any_t propertydis;

    strforcpy = (char*)rc_malloc(strlen(keys) + 1);
    if (strforcpy == NULL) {
        return NULL;
    }

    strcpy(strforcpy, keys);
    count = 1;
    p = strforcpy;
    while (*p) {
        if (*p == ';') {
            ++count;
            *p = '\0';
        }
        ++p;
    }

    if (count <= 0) {
        LOGI(PROPERTY_TAG, "not found any key in input params");
        rc_free(strforcpy);
        return NULL;
    }

    names = (const char**)rc_malloc(sizeof(char*) * count);
    if (names == NULL) {
        rc_free(strforcpy);
        LOGI(PROPERTY_TAG, "malloc array failed");
        return NULL;
    }

    p = strforcpy;
    for (i = 0; i < count; ++i) {
        if (*p != 0 && hashmap_get(mgr->values, p, &propertydis) == MAP_OK) {
            names[i] = p;
        } else {
            LOGI(PROPERTY_TAG, "invalidate key(%s)", p);
            rc_free(strforcpy);
            rc_free(names);
            return NULL;
        }

        p += strlen(p) + 1;  // to next string
    }

    arr = cJSON_CreateStringArray(names, count);

    rc_free(strforcpy);
    rc_free(names);
    return arr;
}

char* format_query_post(rc_property_manager* mgr, const char* keys,
                        cJSON** names) {
    char* sendstr = NULL;
    cJSON* arr = NULL;
    arr = get_param_keyArray(mgr, keys);
    if (arr == NULL) {
        LOGI(PROPERTY_TAG, "create name array failed");
        return NULL;
    }
    BEGIN_JSON_OBJECT(root)
        JSON_OBJECT_ADD_ITEM(root, attrs, arr);
        sendstr = JSON_TO_STRING(root);
        cJSON_DetachItemFromObject(JSON(root), "attrs");
    END_JSON_OBJECT(root);

    *names = arr;  // use it later

    return sendstr;
}
