
#include "logs.h"
#include "rc_http_client.h"
#include "rc_http_manager.h"
#include "rc_http_request.h"
#include "rc_thread.h"
#include "rc_system.h"

#define HC_TAG "[HttpClient]"

typedef struct _rc_http_client_t {
#if defined(WITH_SSL)
    SSL_CTX *ctx;
    SSL *ssl;
#endif
    int socket;

    http_manager manager;

//    short timeout;
    struct sockaddr_in remote_ip;

    http_request cur_request;

    rc_thread worker_thread;

} rc_http_client_t;

extern int http_request_finish(http_request request, int status_code, const char* body);

struct timeval* get_tm_diff(const struct timeval* timeout, struct timeval* result)
{
    gettimeofday(result, NULL);
    result->tv_sec = timeout->tv_sec - result->tv_sec;
    if (timeout->tv_usec < result->tv_usec) {
        -- result->tv_sec;
        result->tv_usec = timeout->tv_usec - result->tv_usec + 1000000;
    }
    else {
        result->tv_usec = timeout->tv_usec - result->tv_usec;
    }
    return result;
}

int connect_to_remote(rc_http_client_t* client, struct timeval* timeout)
{
    int ret;
    char ip[20] = {0};
    client->socket = socket(PF_INET, SOCK_STREAM, 0);
    if (client->socket < 0) {
        LOGI(HC_TAG, "create socket failed with(%d)", client->socket);
        return RC_ERROR_CREATE_SOCKET;
    }

    LOGI(HC_TAG, "timeout: %d.%06d", (int)timeout->tv_sec, (int)timeout->tv_usec);
    setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(struct timeval));
    setsockopt(client->socket, SOL_SOCKET, SO_SNDTIMEO, timeout, sizeof(struct timeval));

#ifdef __QUARK_RTTHREAD__
    strcpy(ip, inet_ntoa(client->remote_ip.sin_addr));
#else
    inet_ntop(AF_INET, &client->remote_ip.sin_addr, ip, sizeof(ip));
#endif    
    LOGI(HC_TAG, "[sock=%d] connect host(%s), port(%d)...", client->socket, ip, (int)ntohs(client->remote_ip.sin_port));

    ret = connect(client->socket, (struct sockaddr *)(&client->remote_ip), sizeof(struct sockaddr));
    if (ret != 0) {
        LOGI(HC_TAG, "[sock=%d] connect failed with(%d)", client->socket, ret);
        close(client->socket);
        client->socket = 0;
    }

    return ret;
}

int http_client_init_remote_info(rc_http_client_t* client, const char* domain, const char *ipaddr, int port)
{
    int result = 0;
    client->remote_ip.sin_family = AF_INET;
    client->remote_ip.sin_port = htons(port);

    if(ipaddr != NULL) {
        client->remote_ip.sin_addr.s_addr = inet_addr(ipaddr);
    } else if (inet_aton(domain, &client->remote_ip.sin_addr) == 0) {
        if ((result = rc_resolve_dns(client->manager, domain, &client->remote_ip.sin_addr)) != 0) {
            LOGE(HC_TAG, "resolve_dns failed %d %s", errno, strerror(errno));
            if (client->cur_request != NULL) {
                http_request_finish(client->cur_request, result, "resolve_dns failed");
                client->cur_request = NULL;
            }
            return result;
        }
    }

    return RC_SUCCESS;
}

http_client http_client_init(http_manager mgr, const char* domain, const char *ipaddr, int port)
{ //
    rc_http_client_t* client = (rc_http_client_t*)rc_malloc(sizeof(rc_http_client_t));
    memset(client, 0, sizeof(rc_http_client_t));
    client->manager = mgr;

    if (http_client_init_remote_info(client, domain, ipaddr, port) != 0) {
        rc_free(client);
        return NULL;
    }

    //LOGI(HC_TAG, "new http client(%p)", client);
//    client->timeout = 5;
    return client;
}

int http_client_connect(http_client _client, int schema, struct timeval* timeout)
{
    struct timeval tv;
    rc_http_client_t* client = (rc_http_client_t*)_client;
    if (client == NULL) return -1;

    return connect_to_remote(client, get_tm_diff(timeout, &tv));
}

int http_client_set_opt(http_client _client, int key, void* value)
{
    rc_http_client_t* client = (rc_http_client_t*)_client;
    if (client == NULL || value == NULL) {
        return RC_ERROR_INVALIDATE_HTTPCLIENT;
    }

    switch (key) {
    case HTTP_CLIENT_OPT_TIMEOUT:
        break;
    default:
        break;
    }

    return RC_SUCCESS;
}

int http_client_recv(http_client _client, char* buf, int buf_len, struct timeval* timeout)
{
    rc_http_client_t* client = (rc_http_client_t*)_client;
    struct timeval tv;
    if (client == NULL) return RC_ERROR_INVALIDATE_HTTPCLIENT;

    if (timeout != NULL) {
        setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)get_tm_diff(timeout, &tv), sizeof(struct timeval));
    }
    return recv(client->socket, buf, buf_len, 0);
}

int http_client_send(http_client _client, const char* buf, int buf_len, struct timeval* timeout)
{
    rc_http_client_t* client = (rc_http_client_t*)_client;
    struct timeval tv;
    if (client == NULL) return RC_ERROR_INVALIDATE_HTTPCLIENT;

    if (timeout != NULL) {
        setsockopt(client->socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)get_tm_diff(timeout, &tv), sizeof(struct timeval));
    }
    return send(client->socket, buf, buf_len, 0) == buf_len ? 0 : RC_ERROR_HTTP_SEND;
}

int http_client_uninit(http_client _client)
{
    rc_http_client_t* client = (rc_http_client_t*)_client;
    if (client == NULL) return RC_ERROR_INVALIDATE_HTTPCLIENT;

    if (client->cur_request != NULL) { // finish current request
        http_request_finish(client->cur_request, 1000, "http client uninit");
        client->cur_request = NULL;
    }

    if (client->worker_thread != NULL) {
        // stop worker thread
        client->worker_thread = NULL;
    }
    
    if (client->socket != 0) {
        close(client->socket);
        client->socket = 0;
    }

    rc_free(client);
    LOGI(HC_TAG, "free http client(%p)", client);
    return RC_SUCCESS;
}
