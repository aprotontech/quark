#include "manager.h"
#include "rc_error.h"

wifi_manager wifi_manager_init(on_wifi_status_changed on_changed)
{
    wifi_manager_t* mgr = (wifi_manager_t*)rc_malloc(sizeof(wifi_manager_t));
    memset(mgr, 0, sizeof(wifi_manager_t));
    mgr->on_changed = on_changed;
    mgr->status = RC_WIFI_DISCONNECTED;
    return mgr;
}

int wifi_manager_uninit(wifi_manager mgr)
{
    if (mgr != NULL) {
        rc_free(mgr);
    }
    return RC_SUCCESS;
}

int wifi_get_local_ip(wifi_manager wm, char* ip, int* wifi_status)
{
    wifi_manager_t* mgr = (wifi_manager_t*)wm;
    if (mgr != NULL) {
        if (ip != NULL) {
            memcpy(ip, mgr->ip, sizeof(mgr->ip));
        }
        if (wifi_status != NULL) {
            *wifi_status = mgr->status;
        }

        return ip == NULL && wifi_status == NULL ? RC_ERROR_INVALIDATE_INPUT : RC_SUCCESS;
    }
    

    return RC_ERROR_INVALIDATE_INPUT;
}