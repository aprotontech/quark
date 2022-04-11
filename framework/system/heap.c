/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     heap.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-21 13:44:50
 * @version  0
 * @brief
 *
 **/

#include "heap.h"
#include "logs.h"
#include <stdio.h>

void print_time_nodes(rc_timer_t** heap, int total);

int compare_timer(rc_timer_t* t1, rc_timer_t* t2) {
    if (t1->next_tick.tv_sec < t2->next_tick.tv_sec)
        return -1;
    else if (t1->next_tick.tv_sec > t2->next_tick.tv_sec)
        return 1;
    else if (t1->next_tick.tv_usec < t2->next_tick.tv_usec)
        return -1;
    else if (t1->next_tick.tv_usec > t2->next_tick.tv_usec)
        return 1;
    return 0;
}

void swap_timer_node(rc_timer_t** heap, int i, int j) {
    rc_timer_t* p = heap[i];
    heap[i] = heap[j];
    heap[j] = p;
    heap[i]->idx = i;
    heap[j]->idx = j;
#if TIMER_BUFFER_LOGGER
    LOGI("TIME", "swap timer[%d](%p)<-->[%d](%p)", i, heap[i]->callback, j,
         heap[j]->callback);
#endif
}

int adjust_node(rc_timer_t** heap, int total, int i) {
    int c;
    int j;
    int b;
    while (i != 0) {
        j = (i - 1) / 2;
        c = compare_timer(heap[i], heap[j]);
        if (c >= 0) break;

        swap_timer_node(heap, i, j);
        i = j;
    }

    while (i * 2 + 1 < total) {
        j = i * 2 + 1;
        c = compare_timer(heap[i], heap[j]);
        if (j + 1 < total) {
            b = compare_timer(heap[i], heap[j + 1]);
            if (c <= 0 && b <= 0) {  // Ti <= Tj && Ti <= Tj+1
                break;
            } else if (c > 0) {  // Ti > Tj
                if (b <= 0) {    // Tj < Ti <= Tj+1
                    swap_timer_node(heap, i, j);
                    i = j;
                } else {  // Ti > Tj && Ti > Tj+1
                    if (compare_timer(heap[j], heap[j + 1]) <
                        0) {  // Ti > Tj+1 > Tj
                        swap_timer_node(heap, i, j);
                        i = j;
                    } else {  // Ti>Tj>=Tj+1
                        swap_timer_node(heap, i, j + 1);
                        i = j + 1;
                    }
                }
            } else if (b > 0) {  // Tj+1 < Ti <= Tj
                swap_timer_node(heap, i, j + 1);
                i = j + 1;
            }
        } else if (c <= 0) {  // no j + 1 and Ti <= Tj
            break;
        } else {  // no j + 1 and Ti > Tj
            swap_timer_node(heap, i, j);
            i = j;
        }
    }

    return 0;
}

void print_time_nodes(rc_timer_t** heap, int total) {
    int i;
    printf("time nodes: ");
    for (i = 0; i < total; ++i) {
        if (i != 0) printf(" ");
        printf("%p", heap[i]);
    }
    printf("\n");
}
