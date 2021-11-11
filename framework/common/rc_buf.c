/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_buf.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 13:49:54
 * @version  0
 * @brief
 *
 **/

#include "rc_buf.h"
#include <stdarg.h>

char* rc_compress_string(int pad, char* input[], int input_len, ...)
{
//    va_list arg_ptr;
//    char* ret = NULL, *p = NULL;
//    int total = pad;
//    int i = 0;
//
//    for (i = 0; i < input_len; ++ i) {
//        total += strlen(input[i]);
//    }
//
//    va_start(arg_ptr, input_len);
//    do {
//    } while(p != NULL);
    return NULL;
    
}

char* rc_copy_string(const char* input)
{
    if (input != NULL) {
        int len = strlen(input);
        char* output = (char*)rc_malloc(len + 1);
        if (output != NULL) {
            memcpy(output, input, len);
            output[len] = '\0';
            return output;
        }
    }
    return NULL;
}

char* get_buf_ptr(rc_buf_t* buf)
{
    return buf == NULL ? NULL :
        (buf->usr_buf ? buf->usr_buf : buf->buf);
}

rc_buf_t* rc_buf_init(int size)
{
    int total = size + sizeof(rc_buf_t) - 4;
    rc_buf_t* p = (rc_buf_t*)rc_malloc(total + 1);
    memset(p, 0, total + 1); // make sure eof is \0

    p->free = 1;
    p->total = size;
    LL_init(&p->link);
    
    return p;
}

rc_buf_t rc_buf_stack()
{
    rc_buf_t buf;
    memset(&buf, 0, sizeof(buf));
    buf.total = sizeof(buf.buf);
    return buf;
}

void rc_buf_free(rc_buf_t* buf)
{
    if (buf != NULL && buf->free) {
        if (buf->usr_buf != NULL) {
            rc_free(buf->usr_buf);
        }
        else {
            rc_free(buf);
        }
    }
}

rc_buf_t rc_buf_usrdata(char* usrbuf, int len)
{
    rc_buf_t buf;
    memset(&buf, 0, sizeof(buf));
    buf.usr_buf = usrbuf;
    buf.total = len;
    return buf;
}

rc_buf_t* rc_buf_append(rc_buf_t* buf, const char* data, int len)
{
    if (buf == NULL || RC_BUF_LEFT_SIZE(buf) <= len) {
        return NULL;
    }

    memcpy(get_buf_ptr(buf) + buf->length, data, len);
    buf->length += len;
    return buf;
}