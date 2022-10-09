/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     backoff.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-26 08:44:27
 *
 */

#include "backoff.h"
#include "rc_error.h"

int rc_backoff_algorithm_init(backoff_algorithm_t* alg, int* fail_intervals,
                              int count, int suc_retry_interval) {
    if (alg == NULL || fail_intervals == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    alg->status = 0;
    alg->index = 0;
    alg->length = count;
    alg->fail_retry_intervals = fail_intervals;
    alg->last_retry_time = 0;
    alg->suc_retry_interval = suc_retry_interval;

    return 0;
}

int rc_backoff_algorithm_set_result(backoff_algorithm_t* alg, int is_success) {
    if (alg == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    alg->last_retry_time = time(NULL);
    alg->status = is_success;
    if (is_success) {
        alg->index = 0;
    } else if (alg->index < alg->length - 1) {
        ++alg->index;
    }

    return 0;
}

int rc_backoff_algorithm_can_retry(backoff_algorithm_t* alg) {
    if (alg == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    time_t t = alg->last_retry_time;
    if (alg->status) {  // success
        if (alg->suc_retry_interval < 0) {
            return 0;
        }
        t += alg->suc_retry_interval;
    } else {
        t += alg->fail_retry_intervals[alg->index];
    }

    return t <= time(NULL);
}

int rc_backoff_algorithm_restart(backoff_algorithm_t* alg, int skip_success) {
    if (alg == NULL) {
        return RC_ERROR_INVALIDATE_INPUT;
    }

    alg->index = 0;
    if (skip_success && alg->status == 1) {
        alg->status = 0;
    }

    return 0;
}