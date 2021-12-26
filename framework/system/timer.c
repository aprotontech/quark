/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     timer.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-20 18:08:51
 * @version  0
 * @brief
 *
 **/

#include "heap.h"
#include "logs.h"
#include "rc_system.h"

#if defined(__QUARK_RTTHREAD__)
#define TIMER_HEAP_INIT_SIZE 16
#else
#define TIMER_HEAP_INIT_SIZE 1024
#endif
#define DEFAULT_WAIT_TIMEOUT 60000
#define FIRST_TICK_INTERVAL 100

extern int compare_timer(rc_timer_t* t1, rc_timer_t* t2);

mstime_t rc_get_mstick() {
    struct timeval now;
    mstime_t t = 0;
    gettimeofday(&now, NULL);
    t = now.tv_sec;
    t = t * 1000 + now.tv_usec / 1000;
    return t;
}

void* timer_tick_thread(void* mgr) {
    int ret;
    int timeout = DEFAULT_WAIT_TIMEOUT;
    struct timeval now;
    struct timeval* tick;
    rc_timer_t* tc;
    rc_timer_manager_t* manager = (rc_timer_manager_t*)mgr;
    if (mgr == NULL) {
        LOGI(TM_TAG, "timer_tick_thread invalidate input");
        return NULL;
    }

    LOGI(TM_TAG, "timer(%p) timer_tick_thread start", manager);

    while (!manager->exit_thread) {
        ret = rc_event_wait(manager->event, timeout);
        if (manager->exit_thread) break;
#if TIMER_BUFFER_LOGGER
        LOGI(TM_TAG, "timer-manager(%p) loop, ret=%d", manager, ret);
#endif

        tc = NULL;
        rc_mutex_lock(manager->mobject);
        if (manager->heap_length > 0) {
            gettimeofday(&now, NULL);
            tick = &manager->heap[0]->next_tick;
            if (tick->tv_sec < now.tv_sec ||
                (tick->tv_sec == now.tv_sec && tick->tv_usec <= now.tv_usec)) {
                tc = manager->heap[0];
                tc->calling = 1;

                if (tc->repeat > 0) {
                    int usec =
                        now.tv_usec +
                        (tc->repeat % 1000) *
                            1000;  // 存在时钟漂移问题，因为使用了当前时间，理论上应该是用上一次的时间
                    tc->next_tick.tv_sec =
                        now.tv_sec + (tc->repeat / 1000) + (usec / 1000000);
                    tc->next_tick.tv_usec = usec % 1000000;
                    adjust_node(manager->heap, manager->heap_length, 0);
                } else {
                    tc->next_tick.tv_sec = TIMER_STOP_FLAG;
                    swap_timer_node(manager->heap, tc->idx,
                                    manager->heap_length - 1);
                    --manager->heap_length;
                    adjust_node(manager->heap, manager->heap_length, 0);
                    LOGI(TM_TAG,
                         "timer-manager(%p) remove non-repeat timer(%p), "
                         "callback(%p)",
                         manager, tc, tc->callback);
                }
            }
        }
        rc_mutex_unlock(manager->mobject);

        if (tc != NULL && tc->callback != NULL) {
#if TIMER_BUFFER_LOGGER
            LOGI(TM_TAG, "timer to call(%p)", tc->callback);
#endif
            tc->callback(tc, tc->usr_data);
        }

        rc_mutex_lock(manager->mobject);
        if (tc != NULL) {
            tc->calling = 0;
            if (manager->heap[tc->idx] !=
                tc) {  // this timer had stoped and the position had reused, so
                       // release it
                free(tc);
            }
        }

        if (manager->heap_length > 0) {
            gettimeofday(&now, NULL);
            timeout =
                (manager->heap[0]->next_tick.tv_sec - now.tv_sec) * 1000 +
                (manager->heap[0]->next_tick.tv_usec - now.tv_usec + 999) /
                    1000;
#if TIMER_BUFFER_LOGGER
            LOGI(TM_TAG, "timer manager(%p) next tick(%d)ms", manager, timeout);
#endif
            if (timeout < 0) {
                timeout = 0;
                LOGI(TM_TAG, "timer[0](%p) is delayed, now=%d.%06d",
                     manager->heap[0]->callback, (int)now.tv_sec,
                     (int)now.tv_usec);
                for (ret = 0; ret < manager->heap_length; ++ret) {
                    tick = &manager->heap[ret]->next_tick;
                    LOGI(TM_TAG, "timer[%d](%p)=%d.%06d", ret,
                         manager->heap[ret]->callback, (int)tick->tv_sec,
                         (int)tick->tv_usec);
                }
            } else if (timeout > DEFAULT_WAIT_TIMEOUT)
                timeout = DEFAULT_WAIT_TIMEOUT;
        } else {
#if TIMER_BUFFER_LOGGER
            LOGI(TM_TAG, "timer manager no timer");
#endif
            timeout = DEFAULT_WAIT_TIMEOUT;
        }
        rc_mutex_unlock(manager->mobject);
    }

    LOGI(TM_TAG, "timer(%p) timer_tick_thread stop", manager);
    return NULL;
}

rc_timer_manager rc_timer_manager_init() {
    int ret;
    rc_timer_manager_t* mgr =
        (rc_timer_manager_t*)malloc(sizeof(rc_timer_manager_t));
    if (mgr == NULL) {
        LOGI(TM_TAG, "create rc_timer_manager_t failed");
        return NULL;
    }

    memset(mgr, 0, sizeof(rc_timer_manager_t));
    mgr->mobject = rc_mutex_create(NULL);
    mgr->event = rc_event_init();
    mgr->heap = malloc(sizeof(rc_timer_t*) * TIMER_HEAP_INIT_SIZE);
    mgr->heap_total = TIMER_HEAP_INIT_SIZE;
    mgr->heap_length = 0;
    memset(mgr->heap, 0, sizeof(rc_timer_t*) * TIMER_HEAP_INIT_SIZE);
    rc_thread_context_t ctx = {
        .joinable = 1, .name = "timer", .priority = -1, .stack_size = 4096};
    mgr->timer_thread = rc_thread_create(timer_tick_thread, mgr, &ctx);
    LOGI(TM_TAG, "timer manager(%p) new", mgr);
    return mgr;
}

rc_timer rc_timer_create(rc_timer_manager mgr, int tick, int repeat,
                         rc_on_time on_time, void* usr_data) {
    rc_timer_t* tm;
    rc_timer_t** heap;
    rc_timer_manager_t* manager = (rc_timer_manager_t*)mgr;
    if (manager == NULL) {
        return NULL;
    }

    LOGI(TM_TAG, "tick=%d, repeat=%d, callback=%p", tick, repeat, on_time);
    rc_mutex_lock(manager->mobject);
    if (manager->heap_length + 1 >= manager->heap_total) {  // need new buffer
        heap =
            (rc_timer_t**)malloc(sizeof(rc_timer_t*) * manager->heap_total * 2);
        memcpy(heap, manager->heap, sizeof(rc_timer_t*) * manager->heap_total);
        memset(heap + manager->heap_total, 0,
               sizeof(rc_timer_t*) * manager->heap_total);
        manager->heap_total = manager->heap_total * 2;
        manager->heap = heap;
    }

    tm = manager->heap[manager->heap_length];
    if (tm == NULL || tm->calling) {
        tm = (rc_timer_t*)malloc(sizeof(rc_timer_t));
        memset(tm, 0, sizeof(rc_timer_t));
    }

    tm->tick = tick;
    tm->repeat = repeat;
    tm->callback = on_time;
    tm->usr_data = usr_data;
    tm->calling = 0;
    tm->stoped = 0;
    tm->idx = manager->heap_length;
    tm->manager = manager;
    gettimeofday(&tm->next_tick, NULL);
    tm->next_tick.tv_usec += (tick % 1000) * 1000;
    tm->next_tick.tv_sec += (tick / 1000) + (tm->next_tick.tv_usec / 1000000);
    tm->next_tick.tv_usec = tm->next_tick.tv_usec % 1000000;

    manager->heap[manager->heap_length] = tm;
    ++manager->heap_length;  // new item
    adjust_node(manager->heap, manager->heap_length, manager->heap_length - 1);
    rc_mutex_unlock(manager->mobject);

    LOGI(TM_TAG, "tick=%d, repeat=%d, callback=%p", tick, repeat, on_time);
    LOGI(TM_TAG, "new timer(%p), tick(%d), callback(%p)", tm, tm->tick,
         tm->callback);
    if (manager->event) {
        rc_event_signal(manager->event);
    }

    return tm;
}

int rc_timer_stop(rc_timer timer) {
    int i;
    rc_timer_t* tm;
    rc_timer_manager_t* manager;
    if (timer == NULL) {
        return -1;
    }

    tm = (rc_timer_t*)timer;
    manager = (rc_timer_manager_t*)tm->manager;

    rc_mutex_lock(manager->mobject);
    if (manager->heap[tm->idx] == tm) {
        tm->stoped = 1;
        tm->next_tick.tv_sec = TIMER_STOP_FLAG;
        i = tm->idx;
        swap_timer_node(manager->heap, tm->idx,
                        manager->heap_length - 1);  // swap it with last node
        --manager->heap_length;
        adjust_node(manager->heap, manager->heap_length, i);
    }
    rc_mutex_unlock(manager->mobject);
    if (manager->event) {
        rc_event_signal(manager->event);
    }

    return 0;
}

int rc_timer_ahead_once(rc_timer timer, int tick) {
    rc_timer_t* tm;
    rc_timer_t tmp;
    rc_timer_manager_t* manager;
    if (timer == NULL) {
        return -1;
    }

    tm = (rc_timer_t*)timer;
    manager = (rc_timer_manager_t*)tm->manager;

    rc_mutex_lock(manager->mobject);
    if (manager->heap[tm->idx] == tm) {
        LOGI(TM_TAG, "ahead timer[%d](%p) once tick(%dms)", tm->idx,
             tm->callback, tick);

        gettimeofday(&tmp.next_tick, NULL);
        tmp.next_tick.tv_usec += (tick % 1000) * 1000;
        tmp.next_tick.tv_sec +=
            (tick / 1000) + (tmp.next_tick.tv_usec / 1000000);
        tmp.next_tick.tv_usec = tmp.next_tick.tv_usec % 1000000;
        if (compare_timer(&tmp, tm) < 0) {  // less than current tick
            tm->next_tick = tmp.next_tick;
            adjust_node(manager->heap, manager->heap_length, tm->idx);
        }
    }

    rc_mutex_unlock(manager->mobject);
    if (manager->event) {
        rc_event_signal(manager->event);
    }

    return 0;
}

int rc_timer_manager_stop_world(rc_timer_manager mgr) {
    rc_timer_manager_t* manager = (rc_timer_manager_t*)mgr;
    if (manager != NULL && manager->exit_thread == 0) {
        LOGI(TM_TAG, "timer stop the world");
        manager->exit_thread = 1;
        if (manager->event) {
            rc_event_signal(manager->event);
        }

        if (manager->timer_thread != NULL) {
            rc_thread_join(manager->timer_thread);
            manager->timer_thread = NULL;
        }
        LOGI(TM_TAG, "timer all stoped");
    }

    return 0;
}

int rc_timer_manager_uninit(rc_timer_manager mgr) {
    int i;
    rc_timer_manager_t* manager = (rc_timer_manager_t*)mgr;
    if (manager != NULL) {
        rc_timer_manager_stop_world(mgr);

        if (manager->event) {
            rc_event_uninit(manager->event);
            manager->event = NULL;
        }
        for (i = 0; i < manager->heap_length; ++i) {
            if (manager->heap[i]) {
                free(manager->heap[i]);
            }
        }
        free(manager->heap);
        manager->heap = NULL;
        if (manager->mobject != NULL) {
            rc_mutex_destroy(manager->mobject);
            manager->mobject = NULL;
        }
        free(manager);

        LOGI(TM_TAG, "timer manager(%p) free", mgr);
    }
    return 0;
}
