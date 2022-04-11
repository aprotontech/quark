/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     esp32_speaker.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-12-31 11:01:08
 *
 */

#include "rc_player.h"

#if defined(__QUARK_FREERTOS__)

#include "driver/i2s.h"

int esp32_setup_wav_pin(int sample, int channels) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = 16000,  // 44100,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // 2-channels
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .intr_alloc_flags = 0,      // Default interrupt priority
        .tx_desc_auto_clear = true  // Auto clear tx descriptor on underflow
    };

    i2s_driver_install(0, &i2s_config, 0, NULL);
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_EXAMPLE_I2S_BCK_PIN,
        .ws_io_num = CONFIG_EXAMPLE_I2S_LRCK_PIN,
        .data_out_num = CONFIG_EXAMPLE_I2S_DATA_PIN,
        .data_in_num = -1  // Not used
    };

    i2s_set_pin(0, &pin_config);
    return 0;
}

int esp32_write_wav_data(char* buff, int len) {
    int bytes_written = 0;
    i2s_write(0, buff, len, &bytes_written, portMAX_DELAY);
    return bytes_written;
}

#endif