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

extern char* ans_json_body(rc_ans_query_t* query);
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

ans_service rc_service_init(http_manager mgr, const char* url, rc_ans_query_t* query)
{
    int len = 0;
    rcservice_mgr_t* service_mgr = (rcservice_mgr_t*)rc_malloc(sizeof(rcservice_mgr_t));
    if (service_mgr == NULL) {
        LOGI(SC_TAG, "init service mgr failed with null");
        return NULL;
    }

    service_mgr->url = rc_copy_string(url);
    service_mgr->smap = hashmap_new();
    service_mgr->ipmap = hashmap_new();
    service_mgr->json = ans_json_body(query);
    service_mgr->json_len = strlen(service_mgr->json);
    service_mgr->manager = mgr;
    service_mgr->encrypt_type = query->encrypt_type;
    service_mgr->encrypt_key = service_mgr->encrypt_type != ANS_ENCRYPT_TYPE_NONE ? 
        rc_copy_string(query->encrypt_key) : NULL;
    service_mgr->rsa = rc_rsa_crypt_init(service_mgr->encrypt_key);
    service_mgr->mobject = rc_mutex_create(NULL);
    
    LOGI(SC_TAG, "new ans service(%p)", service_mgr);
    return service_mgr;
}

int service_set_config(ans_service ans, const char* config, int force_none_encrypt)
{
    int rc = 0;
    map_t new_smap = NULL, new_dnsmap = NULL;
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);
    rc_mutex_lock(service_mgr->mobject);
    rc = parse_json_config(service_mgr, config, force_none_encrypt ? ANS_ENCRYPT_TYPE_NONE : service_mgr->encrypt_type, &new_smap);
    if (rc == 0 && new_smap != NULL) {
        if (service_mgr->smap != NULL) {
            hashmap_iterate(service_mgr->smap, free_hash_item, NULL);
            hashmap_free(service_mgr->smap);
        }

        if (service_mgr->ipmap != NULL) {
            hashmap_iterate(service_mgr->ipmap, free_hash_item, NULL);
            hashmap_free(service_mgr->ipmap);
        }

        service_mgr->smap = new_smap;
        service_mgr->ipmap = build_dns_map(service_mgr->smap);
    }

    rc_mutex_unlock(service_mgr->mobject);
    
    return rc;
}

int rc_service_reload_config(ans_service ans)
{
    int rc = 0;
    rc_buf_t response = rc_buf_stack();
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);

    rc = http_post(service_mgr->manager, service_mgr->url, NULL, NULL, 0,
            service_mgr->json, service_mgr->json_len, 3000, &response);
    LOGI(SC_TAG, "url(%s), request(%s), status(%d), response(%s)",
            service_mgr->url, service_mgr->json, rc, rc == 200 ? get_buf_ptr(&response) : "");

    if (rc != 200) {
        return RC_ERROR_SVRMGR_RELOAD;
    }

    rc = service_set_config(ans, get_buf_ptr(&response), 0);
    rc_buf_free(&response);

    return rc;
}

int rc_service_uninit(ans_service ans)
{
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);
    LOGI(SC_TAG, "free ans service(%p)", service_mgr);

    if (service_mgr->smap != NULL) {
        hashmap_iterate(service_mgr->smap, free_hash_item, NULL);
        hashmap_free(service_mgr->smap);
        service_mgr->smap = NULL;
    }

    if (service_mgr->ipmap != NULL) {
        hashmap_iterate(service_mgr->ipmap, free_hash_item, NULL);
        hashmap_free(service_mgr->ipmap);
        service_mgr->ipmap = NULL;
    }

    if (service_mgr->encrypt_key != NULL) {
        rc_free(service_mgr->encrypt_key);
        service_mgr->encrypt_key = NULL;
    }

    service_mgr->manager = NULL;
    rc_rsa_crypt_uninit(service_mgr->rsa);
    rc_free(service_mgr->url);
    free(service_mgr->json);
    rc_mutex_destroy(service_mgr->mobject);
    rc_free(service_mgr);
    service_mgr = NULL;

    return RC_SUCCESS;
}

const char* rc_service_get_url(ans_service ans, const char* service)
{
    any_t val = NULL;
    const char* url = "";
    rcservice_mgr_t* service_mgr = (rcservice_mgr_t*)ans;
    if (service_mgr == NULL) {
        return "";
    }
    
    rc_mutex_lock(service_mgr->mobject);
    if (MAP_OK == hashmap_get(service_mgr->smap, (char*)service, &val) && val != NULL) {
        url = ((rc_service_t*)val)->uri;
    }
    rc_mutex_unlock(service_mgr->mobject);

    return url;
}

int rc_service_get_info(ans_service ans, const char* name, char domain[30], char uri[50], char ip[20])
{
    int rc = -1;
    any_t val = NULL;
    rcservice_mgr_t* service_mgr = (rcservice_mgr_t*)ans;
    if (service_mgr == NULL) {
        return -1;
    }
    
    rc_mutex_lock(service_mgr->mobject);
    if (MAP_OK == hashmap_get(service_mgr->smap, (char*)name, &val) && val != NULL) {
        strcpy(uri, ((rc_service_t*)val)->uri);
        strcpy(domain, ((rc_service_t*)val)->host);
        strcpy(ip, ((rc_service_t*)val)->ips[0]);
        rc = 0;
    }
    rc_mutex_unlock(service_mgr->mobject);

    return rc;
    
}

const char* rc_service_get_ip(ans_service ans, const char* service)
{
    any_t val = NULL;
    const char* ip = "";
    rcservice_mgr_t* service_mgr = (rcservice_mgr_t*)ans;
    if (service_mgr == NULL) {
        return "";
    }
    
    rc_mutex_lock(service_mgr->mobject);
    if (MAP_OK == hashmap_get(service_mgr->smap, (char*)service, &val) && val != NULL) {
        if (((rc_service_t*)val)->ip_count) {
            ip = ((rc_service_t*)val)->ips[0];
            LOGI(SC_TAG, "service(%s) ---> ip(%s)", service, ip);
        }
        else { // TODO: by host
            ip = "";
        }
    }
    rc_mutex_unlock(service_mgr->mobject);

    return ip;
}

int rc_service_dns_resolve(ans_service ans, const char* host, struct in_addr* ip)
{
    DECLEAR_REAL_VALUE(rcservice_mgr_t, service_mgr, ans);
    if (host != NULL && ip != NULL && service_mgr->ipmap != NULL) {
        any_t val = NULL;
        rc_mutex_lock(service_mgr->mobject);
        if (MAP_OK == hashmap_get(service_mgr->ipmap, (char*)host, &val) && val != NULL) {
            *ip = *((struct in_addr*)val);
            rc_mutex_unlock(service_mgr->mobject);
            return RC_SUCCESS;
        }
        rc_mutex_unlock(service_mgr->mobject);
    }

    return RC_ERROR_SVRMGR_NODNS;
}

