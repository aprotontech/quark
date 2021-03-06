/*
 * **************************************************************************
 *
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     main.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-12-26 08:44:08
 *
 */

extern int mock_virtual_device(char* env, char* app_id, char* app_secret,
                               int test_time_sec);

int main(int argc, const char* argv[]) {
    mock_virtual_device("test", "test", "7045298f456cea6d7a4737c62dd3b89e", 60 * 60);
    return 0;
}
