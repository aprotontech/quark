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
#include "http_parser.h"
#include "rc_url.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ANS_QUERY_PATH "/device/dns"
#define ANS_SERVICE_SIZE (((sizeof(rcservice_mgr_t) + 1023) / 1024) * 1024)
#define GETMIN(a, b) ((a) < (b) ? (a) : (b))

static int __ans_fail_intervals[] = {2, 8, 32, 128, 512};

extern char* ans_build_request_json_body(const char* app_id,
                                         const char* device_id);
extern int parse_json_config(const char* json, map_t* smap);
int ans_service_do_sync(rcservice_mgr_t* mgr);

int free_hash_item(any_t n, const char* key, any_t val) {
    rc_free(val);
    return 0;
}

int service_cleanup(any_t n, const char* key, any_t val) {
    DECLEAR_REAL_VALUE(rc_service_t, service, val);
    // hashmap_iterate(service->protocols, free_hash_item, NULL);
    if (service->protocols != NULL) {
        hashmap_free(service->protocols);
    }

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
    LOGI(SC_TAG, "update service(%s), host(%s)", key, service->host);

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

int rc_service_is_synced(ans_service ans) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    int status = 0;
    rc_mutex_lock(mgr->mobject);
    status = mgr->sync_status;
    rc_mutex_unlock(mgr->mobject);
    return status == 2;
}

int rc_service_sync(ans_service ans, int force) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);

    if (mgr->timer != NULL && !force) {
        rc_timer_ahead_once(mgr->timer, 10);
    } else {
        ans_service_do_sync(mgr);
        if (mgr->sync_status != 2) {
            LOGW(SC_TAG, "ans service is not synced");
            return -1;
        }
    }

    return 0;
}

int ans_service_do_sync(rcservice_mgr_t* mgr) {
    rc_mutex_lock(mgr->mobject);
    if (mgr->sync_status) {
        rc_mutex_unlock(mgr->mobject);
        LOGI(SC_TAG, "ans service is syncing or synced, so skip");
        return -1;
    }

    mgr->sync_status = 1;  // mark is syncing
    rc_mutex_unlock(mgr->mobject);

    char* reqbody = ans_build_request_json_body(mgr->app_id, mgr->device_id);
    rc_buf_t response = rc_buf_stack();
    if (200 == http_post(mgr->httpmgr, mgr->url, NULL, NULL, 0, reqbody,
                         strlen(reqbody), 3000, &response)) {
        map_t new_services = NULL;
        if (parse_json_config(rc_buf_head_ptr(&response), &new_services) == 0) {
            rc_mutex_lock(mgr->mobject);

            mgr->sync_status = 2;
            merge_new_services(mgr, new_services, 1);
            hashmap_free(new_services);

            rc_mutex_unlock(mgr->mobject);
        }
    }

    free(reqbody);
    rc_buf_free(&response);

    rc_mutex_lock(mgr->mobject);
    if (mgr->sync_status != 2) {
        mgr->sync_status = 0;  // mark is idle
    }
    rc_mutex_unlock(mgr->mobject);

    return 0;
}

//////////////////////////////////////////////
// SERVICE INIT/UNINIT
static int ans_check_timer(rc_timer timer, void* usr_data) {
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, usr_data);
    if (network_is_available(mgr->netmgr, NETWORK_MASK_LOCAL) == 0) {
        return 0;
    }

    // network is ok now
    if (!rc_backoff_algorithm_can_retry(&mgr->bkg)) {
        LOGD(SC_TAG, "ans service backoff try later");
        return 0;
    }

    ans_service_do_sync(mgr);

    rc_backoff_algorithm_set_result(&mgr->bkg, mgr->sync_status == 2);

    return 0;
}

int ans_init_default_service(rcservice_mgr_t* mgr, const char* raw_url) {
    int is_https = 0;
    rc_service_protocol_t* protocol = NULL;
    struct sockaddr_in addr;
    struct http_parser_url url;
    int ret = http_url_parse(raw_url, &url, &is_https);
    if (ret != 0) {
        LOGI(SC_TAG, "http_parser_parse_url(%s) failed, ret(%d)", raw_url, ret);
        return ret;
    }

    int host_len = url.field_data[UF_HOST].len;
    int path_len = url.field_data[UF_PATH].len;

    rc_service_t* dftsvr = (rc_service_t*)rc_malloc(
        sizeof(rc_service_t) + sizeof(rc_service_protocol_t) + host_len +
        path_len + 2 + 14);

    protocol = (rc_service_protocol_t*)(dftsvr + 1);
    dftsvr->host = (char*)(protocol + 1);
    dftsvr->prefix = dftsvr->host + host_len + 1;
    dftsvr->ip_count = 1;
    dftsvr->ips[0] = dftsvr->prefix + path_len + 1;
    dftsvr->service = "default";
    dftsvr->validtm = 0;

    memcpy(dftsvr->host, raw_url + url.field_data[UF_HOST].off, host_len);
    memcpy(dftsvr->prefix, raw_url + url.field_data[UF_PATH].off, path_len);
    dftsvr->host[host_len] = '\0';
    dftsvr->prefix[path_len] = '\0';

    strcpy(protocol->protocol, is_https ? "https" : "http");

    dftsvr->protocols = hashmap_new();
    hashmap_put(dftsvr->protocols, protocol->protocol, protocol);
    protocol->port = url.port;

    dftsvr->ips[0][0] = '\0';
    if (inet_pton(AF_INET, dftsvr->host, &addr) != 0) {
        memcpy(dftsvr->ips[0], dftsvr->host, host_len);
        dftsvr->ips[0][host_len] = '\0';
    } else {  // try to dns
        rc_dns_resolve(dftsvr->host, dftsvr->ips[0], 0);
    }

    if (dftsvr->ips[0][0] == '\0') {
        service_cleanup(NULL, NULL, dftsvr);
        return RC_ERROR_SVRMGR_SERVICE;
    }

    hashmap_put(mgr->smap, dftsvr->service, dftsvr);
    return 0;
}

ans_service rc_service_init(const char* app_id, const char* device_id,
                            const char* url, http_manager hmgr,
                            rc_timer_manager tmgr, rc_network_manager* nmgr) {
    rcservice_mgr_t* mgr = (rcservice_mgr_t*)rc_malloc(
        sizeof(rcservice_mgr_t) + strlen(app_id) + strlen(device_id) +
        strlen(url) + strlen(ANS_QUERY_PATH) + 3);
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
    strcat(mgr->url, ANS_QUERY_PATH);

    mgr->sync_status = 0;
    mgr->httpmgr = hmgr;
    mgr->netmgr = nmgr;

    rc_backoff_algorithm_init(&mgr->bkg, __ans_fail_intervals,
                              sizeof(__ans_fail_intervals) / sizeof(int),
                              12 * 3600);

    mgr->smap = hashmap_new();
    // init default service
    if (ans_init_default_service(mgr, mgr->url) != 0) {
        LOGW(SC_TAG, "init default service mgr failed");
        mgr->ipmap = hashmap_new();
    } else {
        mgr->ipmap = build_dns_map(mgr->smap);
    }

    mgr->mobject = rc_mutex_create(NULL);
    mgr->timer = rc_timer_create(tmgr, 1000, 1000, ans_check_timer, mgr);

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
            fill_protocol_info(service, (rc_service_protocol_t*)val, info);
            rc_mutex_unlock(mgr->mobject);
            return RC_SUCCESS;
        }
    } else if (MAP_OK == hashmap_get(mgr->smap, DEFAULT_SERVICE, &val) &&
               val != NULL) {
        rc_service_t* service = (rc_service_t*)val;
        if (MAP_OK == hashmap_get(service->protocols, (char*)protocol, &val)) {
            fill_protocol_info(service, (rc_service_protocol_t*)val, info);
            rc_mutex_unlock(mgr->mobject);
            return RC_SUCCESS;
        }
    }
    rc_mutex_unlock(mgr->mobject);
    LOGI(SC_TAG, "not found information of service(%s), protocol(%s)", name,
         protocol);

    return RC_ERROR_SVRMGR_NODNS;
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

    return http_manager_resolve_dns(mgr->httpmgr, host, ip);
}