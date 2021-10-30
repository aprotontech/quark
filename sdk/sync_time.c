#include "env.h"
#include "rc_system.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_sntp.h"

int rc_enable_ntp_sync_time()
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp1.aliyun.com");
    sntp_init();
}

#endif


int sync_server_time(rc_timer timer, void* dev)
{
    LOGI(SDK_TAG, "sync_server_time");
    return 0;
}