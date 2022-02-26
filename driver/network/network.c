/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     network.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-02-26 12:03:57
 *
 */

#include "rc_network.h"

typedef struct _rc_network_manager_t {
    int status;
} rc_network_manager_t;

int is_network_available(rc_network_manager nm, int level) {
    DECLEAR_REAL_VALUE(rc_network_manager_t, mgr, nm);
    return mgr->status;
}

int network_set_available(rc_network_manager nm, int available) {
    DECLEAR_REAL_VALUE(rc_network_manager_t, mgr, nm);
    mgr->status = available;
    return 0;
}

rc_network_manager network_manager_init(int current_status) {
    rc_network_manager_t* mgr =
        (rc_network_manager_t*)rc_malloc(sizeof(rc_network_manager_t));

    mgr->status = current_status;

    return mgr;
}

int network_manager_uninit(rc_network_manager mgr) {
    rc_free(mgr);
    return 0;
}