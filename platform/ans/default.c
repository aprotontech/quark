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

#define COPY_BYTES(property, dest, source) \
    strcpy(dest, source); \
    property = dest; \
    dest += strlen(source) + 1;
    

rc_service_t* create_service_item(char* configs[4])
{
    char* p = NULL;
    rc_service_t* s = NULL;
    int i = 0;
    int size = sizeof(rc_service_t);
    for (i = 0; i < 4; ++ i) {
        size += strlen(configs[i]) + 1;
    }
    s = (rc_service_t*)rc_malloc(size);
    if (s == NULL) {
        LOGI("ANS", "rc_malloc ans service failed");
        return NULL;
    }

    p = (char*)s + sizeof(rc_service_t);
    
    s->validtm = 2147183259;
    s->ip_count = 0;
    COPY_BYTES(s->service, p, configs[0]);
    COPY_BYTES(s->uri, p, configs[1]);
    COPY_BYTES(s->host, p, configs[2]);
    COPY_BYTES(s->ips[0], p, configs[3]);

    LOGI("[ANS]", "default service(%s), uri(%s), host(%s), ip(%s)", 
            s->service, s->uri, s->host, s->ips[0]);

    return s;
}

extern int free_hash_item(any_t n, const char* key, any_t val);
extern map_t build_dns_map(map_t smap);

int load_ans_local_config(ans_service ans, local_config_string_t* local_configs, int services)
{
    int i;
    rc_service_t* s;
    void* t;
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);

    rc_mutex_lock(service_mgr->mobject);
    for (i = 0; i < services; ++ i) {
        s = create_service_item(local_configs[0][i]);
        if (s != NULL) {
            if (hashmap_get(service_mgr->smap, s->service, &t) == MAP_MISSING) {
                hashmap_put(service_mgr->smap, s->service, s);
            }
            else {
                LOGW("[ANS]", "exist service(%s)", s->service);
            }
        }
    }

    if (service_mgr->ipmap != NULL) {
        hashmap_iterate(service_mgr->ipmap, free_hash_item, NULL);
        hashmap_free(service_mgr->ipmap);
    }

    service_mgr->ipmap = build_dns_map(service_mgr->smap);
    rc_mutex_unlock(service_mgr->mobject);

    return 0;
}
