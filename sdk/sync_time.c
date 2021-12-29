#include "env.h"
#include "rc_system.h"

#if defined(__QUARK_FREERTOS__)
#include "esp_sntp.h"

int rc_enable_ntp_sync_time() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp1.aliyun.com");
    sntp_setservername(1, "pool.ntp.org");
    sntp_init();
    setenv("TZ", "CST-8", 1);
    tzset();
}

#elif defined(__QUARK_LINUX__)

int rc_enable_ntp_sync_time() { return 0; }

#endif

int parse_server_time(cJSON* input, double* now) {
    char* str_rc;
    BEGIN_MAPPING_JSON(input, root)
    JSON_OBJECT_EXTRACT_STRING_TO_VALUE(root, rc, str_rc)
    if (strcmp(str_rc, "0") == 0) {
        JSON_OBJECT_EXTRACT_INT_TO_DOUBLE(root, now, *now);
    }
    END_MAPPING_JSON(root)

    return 0;
}

int sync_server_time(rc_timer timer, void* dev) {
    rc_buf_t response = rc_buf_stack();
    double now = 0;
    LOGI(SDK_TAG, "sync_server_timer");
    int rc = rc_http_quark_post("DEVICE", "/time", "{\"microSecond\":true}",
                                500, &response);
    if (rc == 200) {
        BEGIN_EXTRACT_JSON(rc_buf_head_ptr(&response), root)
        parse_server_time(JSON(root), &now);
        END_EXTRACT_JSON(root)
    }

    rc_buf_free(&response);

    rc_runtime_t* env = get_env_instance();
    if (now != 0 && env != NULL && env->time_update) {
        int sec = (int)now;
        int usec = (int)((now - sec) * 1000000);
        int diff = time(NULL) - sec;
        if (diff >= NOTIFY_TIME_DIFF || diff <= -NOTIFY_TIME_DIFF) {
            env->time_update(sec, usec);
        }
    }

    return 0;
}

rc_timer rc_set_interval(int tick, int repeat, rc_on_time on_time,
                         void* usr_data) {
    rc_runtime_t* env = get_env_instance();
    if (env != NULL && env->timermgr != NULL) {
        return rc_timer_create(env->timermgr, tick, repeat, on_time, usr_data);
    }
    return NULL;
}

int rc_ahead_interval(rc_timer timer, int next_tick) {
    return rc_timer_ahead_once(timer, next_tick);
}

int rc_clear_interval(rc_timer timer) { return rc_timer_stop(timer); }