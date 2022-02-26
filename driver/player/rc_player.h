/*
 * **************************************************************************
 * 
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 * 
 * **************************************************************************
 * 
 *  @file     rc_player.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-12-31 11:00:40
 * 
 */

#ifndef _QUARK_PLAYER_H_
#define _QUARK_PLAYER_H_

#include "rc_thread.h"
#include "rc_buf_queue.h"

#define PY_TAG "[Player]"

typedef void* rc_player;

typedef int (*player_write_data)(rc_player player, char* data, int len);


rc_player rc_player_init(rc_buf_queue queue, player_write_data writer);

int rc_player_restart(rc_player player, int code, int nbits_per_sample, int samples, int channels);

int rc_player_uninit(rc_player player);



typedef struct _rc_player_t {
    rc_buf_queue queue;
    rc_thread play_thread;

    player_write_data writer;
    
    short stop;
} rc_player_t;

#endif