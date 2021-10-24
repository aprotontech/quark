/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_buf.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-14 13:50:44
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_RC_BUF_H_
#define _QUARK_RC_BUF_H_

#include "clist.h"
#include "rc_error.h"

typedef struct _rc_buf_t {
    list_link_t link;
    int length;
    int total;
    char free; // buf need to free
    char immutable; // buf data can't modify
    char* usr_buf;
    char buf[4];
} rc_buf_t;

char* get_buf_ptr(rc_buf_t* buf);
rc_buf_t* rc_buf_init(int size);
rc_buf_t rc_buf_usrdata(char* usrbuf, int len);
rc_buf_t rc_buf_stack();
void rc_buf_free(rc_buf_t* buf);

char* rc_copy_string(const char* input);
char* rc_compress_string(int pad, char* input[], int input_len, ...);

#define RC_BUF_PTR(p) (((p)->usr_buf != NULL ? (p)->usr_buf : (char*)(p)->buf) + (p)->length)
#define RC_BUF_CLEAN(p) ((p)->length = 0)
#define RC_BUF_LEFT_SIZE(p) ((p)->total - (p)->length)


#endif

