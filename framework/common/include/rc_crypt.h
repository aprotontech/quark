/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_crypt.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-16 21:22:49
 * @version  0
 * @brief
 *
 **/

#ifndef _AIE_CRYPT_H_
#define _AIE_CRYPT_H_

#include "rc_buf.h"

typedef void* rsa_crypt;

rsa_crypt rc_rsa_crypt_init(const char* public_key);

int rc_rsa_encrypt(rsa_crypt rsa, const char* decrypted, int len, rc_buf_t* encrypted);

int rc_rsa_decrypt(rsa_crypt rsa, const char* encrypted, int len, rc_buf_t* decrypted);

int rc_rsa_crypt_uninit(rsa_crypt rsa);

#endif
