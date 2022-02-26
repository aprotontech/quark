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

#include "logs.h"
#include "rc_error.h"
#include "service.h"

typedef struct _local_service_config_t {
    char* config;
    char* env_name;
} local_service_config_t;

const local_service_config_t _local_configs[] = {
    {.env_name = "online",
     .config = "{\"host\":\"api.aproton.tech\",\"ip\":[\"82.157.138.167\"],"
               "\"prefix\":"
               "\"\\/\",\"protcol\":{\"http\":80,\"mqtt\":1883}}"}};

extern int parse_json_config(const char* json, map_t* smap);

int merge_new_services(rcservice_mgr_t* mgr, map_t new_services, int overwrite);

int rc_service_local_config(ans_service ans, const char* env_name) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);

    rc_mutex_lock(service_mgr->mobject);

    for (int i = 0; i < sizeof(_local_configs) / sizeof(local_service_config_t);
         ++i) {
        if (strcmp(_local_configs[i].env_name, env_name) == 0) {
            map_t services = NULL;
            if (parse_json_config(_local_configs[i].config, &services) == 0) {
                merge_new_services(service_mgr, services, 0);
                hashmap_free(service_mgr);
            }
            break;
        }
    }

    rc_mutex_unlock(service_mgr->mobject);

    return 0;
}
