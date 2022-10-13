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

extern int parse_json_config(const char* json, map_t* smap);

int merge_new_services(rcservice_mgr_t* mgr, map_t new_services, int overwrite);

int rc_service_local_config(ans_service ans, const char* env_config) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);

    rc_mutex_lock(service_mgr->mobject);

    LOGI(SC_TAG, "load default settings(%s)", env_config);
    map_t services = NULL;
    if (parse_json_config(env_config, &services) == 0) {
        merge_new_services(service_mgr, services, 1);
        hashmap_free(services);
    }

    rc_mutex_unlock(service_mgr->mobject);

    return 0;
}
