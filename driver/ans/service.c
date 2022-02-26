/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     service.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 23:11:25
 * @version  0
 * @brief
 *
 **/

#include "service.h"

#include "cJSON.h"
#include "logs.h"
#include "rc_error.h"
#include "rc_http_request.h"
#include "rc_json.h"
#include "rc_system.h"

#define ANS_SERVICE_SIZE (((sizeof(rcservice_mgr_t) + 1023) / 1024) * 1024)

extern char* ans_build_request_json_body(const char* app_id,
                                         const char* device_id);
extern int parse_json_config(rcservice_mgr_t* service_mgr, const char* json,
                             map_t* smap);

int free_hash_item(any_t n, const char* key, any_t val) {
    rc_free(val);
    return 0;
}

int service_cleanup(any_t n, const char* key, any_t val) {
    DECLEAR_REAL_VALUE(rc_service_t, service, val);
    hashmap_iterate(service->protocols, free_hash_item, NULL);
    hashmap_free(service->protocols);
    rc_free(service);
    return 0;
}

//////////////////////////////////////////////
// BUILD DNS MAP
int dns_map(any_t ipm, const char* key, any_t value) {
    rc_service_t* s = (rc_service_t*)value;
    map_t* dnsmap = (map_t*)ipm;
    any_t p = NULL;
    if (MAP_MISSING == hashmap_get(dnsmap, s->host, &p) && s->ip_count > 0) {
        struct in_addr* addr =
            (struct in_addr*)rc_malloc(sizeof(struct in_addr));
        if (inet_aton(s->ips[0], addr) != 0) {
            hashmap_put(dnsmap, s->host, addr);
            LOGI(SC_TAG, "bind %s to %s", s->host, s->ips[0]);
        } else {
            rc_free(addr);
        }
    }
    return 0;
}

map_t build_dns_map(map_t smap) {
    map_t dnsmap = hashmap_new();
    hashmap_iterate(smap, dns_map, dnsmap);
    return dnsmap;
}

//////////////////////////////////////////////
// MERGE SERVICES
typedef struct _iterator_context_t {
    rcservice_mgr_t* mgr;
    int overwrite;
} iterator_context_t;

static int update_mgr_services(any_t n, const char* key, any_t val) {
    DECLEAR_REAL_VALUE(rc_service_t, service, val);
    DECLEAR_REAL_VALUE(iterator_context_t, context, n);
    any_t result = NULL;
    if (hashmap_get(context->mgr->smap, service->service, &result) == MAP_OK) {
        if (context->overwrite) {
            service_cleanup(NULL, NULL, result);
        } else {  // exist same service skip
            service_cleanup(NULL, NULL, val);
            return 0;
        }
    }

    hashmap_put(context->mgr->smap, service->service, service);

    return 0;
}

int merge_new_services(rcservice_mgr_t* mgr, map_t new_services,
                       int overwrite) {
    iterator_context_t context = {.mgr = mgr, .overwrite = overwrite};

    LOGI(SC_TAG, "merge new services");

    hashmap_iterate(new_services, update_mgr_services, &context);

    if (mgr->ipmap != NULL) {
        hashmap_iterate(mgr->ipmap, free_hash_item, NULL);
        hashmap_free(mgr->ipmap);
    }

    mgr->ipmap = build_dns_map(mgr->smap);

    return 0;
}

int rc_service_sync(ans_service ans) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);

    rc_mutex_lock(&mgr->mobject);
    if (mgr->sync_status) {
        rc_mutex_unlock(&mgr->mobject);
        LOGI(SC_TAG, "ans service is syncing or synced, so skip");
        return -1;
    }

    mgr->sync_status = 1;  // mark is syncing
    rc_mutex_unlock(&mgr->mobject);

    char* reqbody = ans_build_request_json_body(mgr->app_id, mgr->device_id);
    rc_buf_t response = rc_buf_stack();
    if (200 == http_post(mgr->httpmgr, mgr->url, NULL, NULL, 0, reqbody,
                         strlen(reqbody), 3000, &response)) {
        map_t new_service_map = NULL;
        if (0 == parse_json_config(mgr, rc_buf_head_ptr(&response),
                                   &new_service_map)) {
            rc_mutex_lock(&mgr->mobject);

            mgr->sync_status = 2;
            merge_new_services(mgr, new_service_map, 1);
            hashmap_free(new_service_map);

            rc_mutex_unlock(&mgr->mobject);
        }
    }

    free(reqbody);

    rc_mutex_lock(&mgr->mobject);
    if (mgr->sync_status != 2) {
        mgr->sync_status = 0;  // mark is idle
    }
    rc_mutex_unlock(&mgr->mobject);

    return 0;
}

//////////////////////////////////////////////
// SERVICE INIT/UNINIT
static int ans_check_timer(rc_timer timer, void* usr_data) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, usr_data);
    if (is_network_available(mgr->netmgr, 0) == 0) {
        return 0;
    }

    // network is ok now

    rc_service_sync(mgr);

    if (mgr->sync_status == 2) {
        LOGI(SC_TAG, "finish query services, so close the timer");
        rc_timer_stop(mgr->timer);
        mgr->timer = NULL;
    }

    return 0;
}

ans_service rc_service_init(const char* app_id, const char* device_id,
                            const char* url, http_manager hmgr,
                            rc_timer_manager tmgr, rc_network_manager* nmgr) {
    int len = 0;
    rcservice_mgr_t* mgr =
        (rcservice_mgr_t*)rc_malloc(sizeof(rcservice_mgr_t) + strlen(app_id) +
                                    strlen(device_id) + strlen(url) + 3);
    if (mgr == NULL) {
        LOGI(SC_TAG, "init service mgr failed with null");
        return NULL;
    }

    // init configs
    mgr->app_id = (char*)&(mgr[1]);
    mgr->device_id = mgr->app_id + strlen(app_id) + 1;
    mgr->url = mgr->device_id + strlen(device_id) + 1;
    strcpy(mgr->app_id, app_id);
    strcpy(mgr->device_id, device_id);
    strcpy(mgr->url, url);

    mgr->smap = hashmap_new();
    mgr->ipmap = hashmap_new();
    mgr->sync_status = 0;
    mgr->httpmgr = hmgr;
    mgr->netmgr = nmgr;

    mgr->mobject = rc_mutex_create(NULL);
    mgr->timer = rc_timer_create(tmgr, 1000, 1000, ans_check_timer, mgr);
    rc_timer_ahead_once(mgr->timer, 10);

    LOGI(SC_TAG, "new ans service(%p)", mgr);
    return mgr;
}

int rc_service_uninit(ans_service ans) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    LOGI(SC_TAG, "free ans service(%p)", mgr);

    if (mgr->smap != NULL) {
        hashmap_iterate(mgr->smap, service_cleanup, NULL);
        hashmap_free(mgr->smap);
        mgr->smap = NULL;
    }

    if (mgr->ipmap != NULL) {
        hashmap_iterate(mgr->ipmap, free_hash_item, NULL);
        hashmap_free(mgr->ipmap);
        mgr->ipmap = NULL;
    }

    if (mgr->timer != NULL) {
        rc_timer_stop(mgr->timer);
        mgr->timer = NULL;
    }

    mgr->httpmgr = NULL;
    // rc_rsa_crypt_uninit(mgr->rsa);
    rc_mutex_destroy(mgr->mobject);
    rc_free(mgr);
    mgr = NULL;

    return RC_SUCCESS;
}

//////////////////////////////////////////////
// SERVICE QUERY
static int fill_protocol_info(rc_service_t* service,
                              rc_service_protocol_t* protocol,
                              rc_service_protocol_info_t* output) {
    output->host = service->host;
    output->port = protocol->port;
    output->prefix = service->prefix;
    output->ip = service->ips[rand() % service->ip_count];
    return 0;
}

int rc_service_query(ans_service ans, const char* name, const char* protocol,
                     rc_service_protocol_info_t* info) {
    any_t val = NULL;
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);

    rc_mutex_lock(mgr->mobject);
    if (MAP_OK == hashmap_get(mgr->smap, (char*)name, &val) && val != NULL) {
        rc_service_t* service = (rc_service_t*)val;
        // find the protocol
        if (MAP_OK == hashmap_get(service->protocols, (char*)protocol, &val)) {
            return fill_protocol_info(service, (rc_service_protocol_t*)val,
                                      info);
        }
    } else if (MAP_OK == hashmap_get(mgr->smap, DEFAULT_SERVICE, &val) &&
               val != NULL) {
        rc_service_t* service = (rc_service_t*)val;
        // find the protocol
        if (MAP_OK == hashmap_get(service->protocols, (char*)protocol, &val)) {
            return fill_protocol_info(service, (rc_service_protocol_t*)val,
                                      info);
        }
    }
    rc_mutex_unlock(mgr->mobject);

    return 0;
}

int rc_service_dns_resolve(ans_service ans, const char* host,
                           struct in_addr* ip) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    if (host != NULL && ip != NULL && mgr->ipmap != NULL) {
        any_t val = NULL;
        rc_mutex_lock(mgr->mobject);
        if (MAP_OK == hashmap_get(mgr->ipmap, (char*)host, &val) &&
            val != NULL) {
            *ip = *((struct in_addr*)val);
            rc_mutex_unlock(mgr->mobject);
            return RC_SUCCESS;
        }
        rc_mutex_unlock(mgr->mobject);
    }

    return rc_resolve_dns(mgr->httpmgr, host, ip);
}