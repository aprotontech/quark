#include "manager.h"
#include "rc_error.h"

extern int _wifi_manager_init(wifi_manager_t* mgr);

wifi_manager wifi_manager_init(on_wifi_status_changed on_changed) {
    wifi_manager_t* mgr = (wifi_manager_t*)rc_malloc(sizeof(wifi_manager_t));
    memset(mgr, 0, sizeof(wifi_manager_t));
    mgr->on_changed = on_changed;
    mgr->scan_result = NULL;
    mgr->status = RC_WIFI_DISCONNECTED;

    _wifi_manager_init(mgr);
    return mgr;
}

int wifi_manager_uninit(wifi_manager mgr) {
    if (mgr != NULL) {
#if defined(__QUARK_FREERTOS__)
        if (((wifi_manager_t*)mgr)->ap_cache != NULL) {
            free(((wifi_manager_t*)mgr)->ap_cache);
        }
#endif

        rc_free(mgr);
    }

    return RC_SUCCESS;
}

int wifi_get_local_ip(wifi_manager wm, char* ip, int* wifi_status) {
    DECLEAR_REAL_VALUE(wifi_manager_t, mgr, wm);

    if (ip != NULL) {
        memcpy(ip, mgr->ip, sizeof(mgr->ip));
    }
    if (wifi_status != NULL) {
        *wifi_status = mgr->status;
    }

    return ip == NULL && wifi_status == NULL ? RC_ERROR_INVALIDATE_INPUT
                                             : RC_SUCCESS;
}