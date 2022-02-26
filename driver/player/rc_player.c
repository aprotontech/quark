/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     rc_player.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-01-01 10:50:47
 *
 */

#include "rc_player.h"

#include "logs.h"
#include "rc_error.h"

void* _quark_player_thread(void* param);

rc_player rc_player_init(rc_buf_queue queue, player_write_data writer) {
    if (queue == NULL) {
        LOGW(PY_TAG, "input buffer queue is empty");
        return NULL;
    }
    rc_player_t* player = (rc_player_t*)rc_malloc(sizeof(rc_player_t));
    memset(player, 0, sizeof(rc_player_t));
    player->stop = 0;
    player->queue = queue;
    player->writer = writer;

    if (player->writer == NULL) {
#if defined(__QUARK_FREERTOS__)
        extern int esp32_write_wav_data(char* buff, int len);
        player->writer = esp32_write_wav_data;
#endif
    }

    rc_thread_context_t ctx = {
        .joinable = 1, .name = "player", .priority = 0, .stack_size = 2048};

    player->play_thread = rc_thread_create(_quark_player_thread, player, &ctx);
    return player;
}

int rc_player_uninit(rc_player plyer) {
    DECLEAR_REAL_VALUE(rc_player_t, player, plyer);
    if (player->play_thread != NULL) {
        player->stop = 1;
        rc_thread_join(player->play_thread);
    }

    rc_free(player);
    return 0;
}

int rc_player_restart(rc_player plyer, int code, int nbits_per_sample,
                      int sample, int channels) {
    DECLEAR_REAL_VALUE(rc_player_t, player, plyer);
#if defined(__QUARK_FREERTOS__)
    extern int esp32_setup_wav_pin(int sample, int channels);
    esp32_setup_wav_pin(sample, channels);
#endif
    rc_buf_queue_clean(player->queue);
    return 0;
}

void* _quark_player_thread(void* param) {
    rc_player_t* player = (rc_player_t*)param;
    char buf[512] = {0};
    while (!player->stop) {
        int len = rc_buf_queue_pop(player->queue, buf, sizeof(buf), 1000);
        if (len > 0) {
            int bytes_written = player->writer(player, buf, len);

            LOGI(PY_TAG, "write to speaker %d bytes", bytes_written);
        }
    }
    return NULL;
}