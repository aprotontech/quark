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
#include "logs.h"
#include "rc_error.h"
#include "rc_http_request.h"
#include "cJSON.h"
#include "rc_json.h"
#include "rc_system.h"

#define ANS_SERVICE_SIZE (((sizeof(rcservice_mgr_t) + 1023) / 1024) * 1024)

extern char* ans_build_request_json_body(rc_ans_config_t* query);
extern int parse_json_config(rcservice_mgr_t* service_mgr, const char* json, int encrypt, map_t* smap);

int free_hash_item(any_t n, const char* key, any_t val)
{
    rc_free(val);
    return 0;
}

int dns_map(any_t ipm, const char* key, any_t value)
{
    rc_service_t* s = (rc_service_t*)value;
    map_t* dnsmap = (map_t*)ipm;
    any_t p = NULL;
    if (MAP_MISSING == hashmap_get(dnsmap, s->host, &p) && s->ip_count > 0) {
        struct in_addr* addr = (struct in_addr*)rc_malloc(sizeof(struct in_addr));
        if (inet_aton(s->ips[0], addr) != 0) {
            hashmap_put(dnsmap, s->host, addr);
            LOGI(SC_TAG, "bind %s to %s", s->host, s->ips[0]);
        }
        else {
            rc_free(addr);
        }
    }
    return 0;
}

map_t build_dns_map(map_t smap)
{
    map_t dnsmap = hashmap_new();
    hashmap_iterate(smap, dns_map, dnsmap);
    return dnsmap;
}

char* safe_copy_string_to_buffer(rc_buf_t* buf, char* s)
{
    if (s == NULL) return NULL;

    int len = strlen(s);
    if (rc_buf_append(buf, s, len + 1) == NULL) {
        return NULL;
    }

    return RC_BUF_PTR(buf);
}

ans_service rc_service_init(rc_ans_config_t* config, http_manager hmgr)
{
    int len = 0;
    rcservice_mgr_t* mgr = (rcservice_mgr_t*)rc_malloc(ANS_SERVICE_SIZE);
    if (mgr == NULL) {
        LOGI(SC_TAG, "init service mgr failed with null");
        return NULL;
    }

    mgr->buff = rc_buf_stack();
    mgr->buff.total = ANS_SERVICE_SIZE - sizeof(rcservice_mgr_t) + sizeof(mgr->buff.buf);
    
    // copy input config info
    mgr->config = *config;
    mgr->config.input_service_count = 0;
    mgr->config.input_services = NULL;
    mgr->config.app_id = safe_copy_string_to_buffer(&mgr->buff, config->app_id);
    mgr->config.client_id = safe_copy_string_to_buffer(&mgr->buff, config->client_id);
    mgr->config.url = safe_copy_string_to_buffer(&mgr->buff, config->url);
    mgr->config.host = safe_copy_string_to_buffer(&mgr->buff, config->host);
    mgr->config.encrypt_key = safe_copy_string_to_buffer(&mgr->buff, config->encrypt_key);

    mgr->smap = hashmap_new();
    mgr->ipmap = hashmap_new();
    mgr->json = safe_copy_string_to_buffer(&mgr->buff, ans_build_request_json_body(config));
    mgr->json_len = strlen(mgr->json);
    mgr->httpmgr = hmgr;
    if (mgr->config.encrypt_type != ANS_ENCRYPT_TYPE_NONE) {
        mgr->rsa = rc_rsa_crypt_init(mgr->config.encrypt_key);
    }
    mgr->mobject = rc_mutex_create(NULL);
    
    LOGI(SC_TAG, "new ans service(%p)", mgr);
    return mgr;
}

int service_set_config(ans_service ans, const char* config, int force_none_encrypt)
{
    int rc = 0;
    map_t new_smap = NULL, new_dnsmap = NULL;
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    rc_mutex_lock(mgr->mobject);
    rc = parse_json_config(mgr, config, force_none_encrypt ? ANS_ENCRYPT_TYPE_NONE : mgr->config.encrypt_type, &new_smap);
    if (rc == 0 && new_smap != NULL) {
        if (mgr->smap != NULL) {
            hashmap_iterate(mgr->smap, free_hash_item, NULL);
            hashmap_free(mgr->smap);
        }

        if (mgr->ipmap != NULL) {
            hashmap_iterate(mgr->ipmap, free_hash_item, NULL);
            hashmap_free(mgr->ipmap);
        }

        mgr->smap = new_smap;
        mgr->ipmap = build_dns_map(mgr->smap);
    }

    rc_mutex_unlock(mgr->mobject);
    
    return rc;
}

int rc_service_reload_config(ans_service ans)
{
    int rc = 0;
    rc_buf_t response = rc_buf_stack();
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    const char* headers[1] = {mgr->config.host};

    rc = http_post(mgr->httpmgr, mgr->config.url, NULL, headers, headers[0] != NULL ? 1 : 0,
            mgr->json, mgr->json_len, 3000, &response);
    LOGI(SC_TAG, "url(%s), request(%s), status(%d), response(%s)",
            mgr->config.url, mgr->json, rc, rc == 200 ? get_buf_ptr(&response) : "");

    if (rc != 200) {
        return RC_ERROR_SVRMGR_RELOAD;
    }

    rc = service_set_config(ans, get_buf_ptr(&response), 0);
    rc_buf_free(&response);

    return rc;
}

int rc_service_uninit(ans_service ans)
{
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    LOGI(SC_TAG, "free ans service(%p)", mgr);

    if (mgr->smap != NULL) {
        hashmap_iterate(mgr->smap, free_hash_item, NULL);
        hashmap_free(mgr->smap);
        mgr->smap = NULL;
    }

    if (mgr->ipmap != NULL) {
        hashmap_iterate(mgr->ipmap, free_hash_item, NULL);
        hashmap_free(mgr->ipmap);
        mgr->ipmap = NULL;
    }

    mgr->httpmgr = NULL;
    rc_rsa_crypt_uninit(mgr->rsa);
    rc_mutex_destroy(mgr->mobject);
    rc_free(mgr);
    mgr = NULL;

    return RC_SUCCESS;
}

const char* rc_service_get_url(ans_service ans, const char* service)
{
    any_t val = NULL;
    const char* url = "";
    rcservice_mgr_t* mgr = (rcservice_mgr_t*)ans;
    if (mgr == NULL) {
        return "";
    }
    
    rc_mutex_lock(mgr->mobject);
    if (MAP_OK == hashmap_get(mgr->smap, (char*)service, &val) && val != NULL) {
        url = ((rc_service_t*)val)->uri;
    }
    rc_mutex_unlock(mgr->mobject);

    return url;
}

int rc_service_get_info(ans_service ans, const char* name, char domain[30], char uri[50], char ip[20])
{
    int rc = -1;
    any_t val = NULL;
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    
    rc_mutex_lock(mgr->mobject);
    if (MAP_OK == hashmap_get(mgr->smap, (char*)name, &val) && val != NULL) {
        strcpy(uri, ((rc_service_t*)val)->uri);
        strcpy(domain, ((rc_service_t*)val)->host);
        strcpy(ip, ((rc_service_t*)val)->ips[0]);
        rc = 0;
    }
    rc_mutex_unlock(mgr->mobject);

    return rc;
    
}

const char* rc_service_get_ip(ans_service ans, const char* service)
{
    any_t val = NULL;
    const char* ip = "";
    rcservice_mgr_t* mgr = (rcservice_mgr_t*)ans;
    if (mgr == NULL) {
        return "";
    }
    
    rc_mutex_lock(mgr->mobject);
    if (MAP_OK == hashmap_get(mgr->smap, (char*)service, &val) && val != NULL) {
        if (((rc_service_t*)val)->ip_count) {
            ip = ((rc_service_t*)val)->ips[0];
            LOGI(SC_TAG, "service(%s) ---> ip(%s)", service, ip);
        }
        else { // TODO: by host
            ip = "";
        }
    }
    rc_mutex_unlock(mgr->mobject);

    return ip;
}

int rc_service_dns_resolve(ans_service ans, const char* host, struct in_addr* ip)
{
    DECLEAR_REAL_VALUE(rcservice_mgr_t, mgr, ans);
    if (host != NULL && ip != NULL && mgr->ipmap != NULL) {
        any_t val = NULL;
        rc_mutex_lock(mgr->mobject);
        if (MAP_OK == hashmap_get(mgr->ipmap, (char*)host, &val) && val != NULL) {
            *ip = *((struct in_addr*)val);
            rc_mutex_unlock(mgr->mobject);
            return RC_SUCCESS;
        }
        rc_mutex_unlock(mgr->mobject);
    }

    return RC_ERROR_SVRMGR_NODNS;
}

