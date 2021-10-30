#include "manager.h"
#include "rc_error.h"

wifi_manager wifi_manager_init(on_wifi_status_changed on_changed)
{
    wifi_manager_t* mgr = (wifi_manager_t*)rc_malloc(sizeof(wifi_manager_t));
    memset(mgr, 0, sizeof(wifi_manager_t));
    mgr->on_changed = on_changed;
    return mgr;
}

int wifi_manager_uninit(wifi_manager mgr)
{
    if (mgr != NULL) {
        rc_free(mgr);
    }
    return RC_SUCCESS;
}