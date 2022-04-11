/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     backoff.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-26 08:39:57
 *
 */

#ifndef _PROTON_COMMON_BACKOFF_H_
#define _PROTON_COMMON_BACKOFF_H_

#include <time.h>

typedef struct _backoff_algorithm_t {
    time_t last_retry_time;
    int status;  // 1-success, 0-failed
    int index;
    int length;
    int suc_retry_interval;
    int* fail_retry_intervals;
} backoff_algorithm_t;

int rc_backoff_algorithm_init(backoff_algorithm_t* alg, int* fail_intervals,
                              int count, int suc_retry_interval);

int rc_backoff_algorithm_set_result(backoff_algorithm_t* alg, int is_success);

int rc_backoff_algorithm_can_retry(backoff_algorithm_t* alg);

#endif