/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     url.c
 * @author   kuper - kuper@aproton.tech
 * @data     2020-02-15 15:58:13
 * @version  0
 * @brief
 *
 **/

#include "rc_url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_symbol(char c)
{
    if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("-_.!~*'()", c) ) {
        return 1;
    }

    return 0;
}

char* rc_url_encode(const char *input, int end)
{
    int final_size = (end * 3) + 1;
    char* working = (char*)malloc(final_size * sizeof(char));
    char* output = working;
    char encoded[4] = {0};

    while (*input) {
        const char c = *input;
        if (is_symbol(c)) {
            *(working ++) = *(input ++);
        } else {
            snprintf(encoded, 4, "%%%02x", (unsigned char)c);

            *(working ++) = encoded[0];
            *(working ++) = encoded[1];
            *(working ++) = encoded[2];
            input ++;
        }
    }

    *working = 0;
    return output;
}

