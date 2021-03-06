#include "mock.h"
#include "rc_http_manager.h"
#include "rc_http_request.h"

#define TEST_QUERY_URL "http://ip.me"

int httpget_other_service_url() {  // query ip.me
    // use rc_http_get/rc_http_post to query service resource
    // here is just demo for access other service
    http_manager mgr = http_manager_init();
    if (mgr == NULL) {
        LOGW(VD_TAG, "init http manager failed");
        return -1;
    }
    LOGI(VD_TAG, "try to query url %s", TEST_QUERY_URL);

    rc_buf_t response = rc_buf_stack();
    const char* headers[] = {"User-Agent: curl/7.68.0"};
    int rc =
        http_get(mgr, TEST_QUERY_URL, NULL, headers,
                 sizeof(headers) / sizeof(const char*), 3 * 1000, &response);
    LOGI(VD_TAG, "query %s status code %d", TEST_QUERY_URL, rc);
    if (rc >= 200 && rc < 300) {
        LOGI(VD_TAG, "My Public Ip: %s", rc_buf_head_ptr(&response));
    }

    rc_buf_free(&response);

    http_manager_uninit(mgr);
    return 0;
}