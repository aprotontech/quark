/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     crypt.c
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-16 21:22:00
 * @version  0
 * @brief
 *
 **/

#include "rc_crypt.h"
#include "rc_error.h"
#include "logs.h"

#ifdef RC_ENABLE_RSA

#include "base64.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"

#define CR_TAG "[CRYPT]"

typedef struct _rc_rsa_crypt_t {
    RSA *rsa;
    BIO *keybio;
} rc_rsa_crypt_t;

rsa_crypt rc_rsa_crypt_init(const char* public_key)
{
    rc_rsa_crypt_t* crypt;
    if (public_key == NULL) return NULL;
    crypt = (rc_rsa_crypt_t*)rc_malloc(sizeof(rc_rsa_crypt_t));
    memset(crypt, 0, sizeof(rc_rsa_crypt_t));
    
    crypt->keybio = BIO_new_mem_buf((unsigned char *)public_key, -1);
    if (crypt->keybio == NULL) {
        rc_free(crypt);
        LOGI(CR_TAG, "BIO_new_mem_buf failed");
        return NULL;
    }

    crypt->rsa = PEM_read_bio_RSA_PUBKEY(crypt->keybio, &crypt->rsa, NULL, NULL);
    if (crypt->rsa == NULL) {
        BIO_free_all(crypt->keybio);
        rc_free(crypt);
        LOGI(CR_TAG, "PEM_read_bio_RSA_PUBKEY return null");
        return NULL;
    }

    return crypt;
}

int rc_rsa_encrypt(rsa_crypt rsa, const char* decrypted, int len, rc_buf_t* encrypted)
{
    int ret;
    int buflen = 0;
    rc_buf_t* tmp = NULL;
    rc_rsa_crypt_t* crypt = (rc_rsa_crypt_t*)rsa;

    if (crypt == NULL || encrypted == NULL) return RC_ERROR_INVALIDATE_INPUT;

    tmp = rc_buf_init(RSA_size(crypt->rsa) + 1);
    ret = RSA_public_encrypt(len, decrypted, rc_buf_head_ptr(tmp), crypt->rsa, RSA_PKCS1_PADDING);
    if (ret < 0) {
        LOGI(CR_TAG, "rsa encrypt(%s) failed", decrypted);
        rc_buf_free(tmp);
        return RC_ERROR_ENCRYPT;
    }

    buflen = BASE64_ENCODE_OUT_SIZE(ret);
    *encrypted = rc_buf_usrdata((char*)rc_malloc(buflen + 1), buflen + 1);
    base64_encode_internal(rc_buf_head_ptr(tmp), ret, rc_buf_head_ptr(encrypted));
    rc_buf_free(tmp);
    encrypted->length = buflen;

    return RC_SUCCESS;
}

int rc_rsa_decrypt(rsa_crypt rsa, const char* encrypted, int len, rc_buf_t* decrypted)
{
    int ret;
    int buflen = BASE64_DECODE_OUT_SIZE(len);
    rc_buf_t* tmp = NULL;
    rc_rsa_crypt_t* crypt = (rc_rsa_crypt_t*)rsa;
    if (crypt == NULL || decrypted == NULL) return RC_ERROR_INVALIDATE_INPUT;
    tmp = rc_buf_init(buflen + 1);

    ret = base64_decode_internal(encrypted, len, rc_buf_head_ptr(tmp));
    if (ret != 0) {
        rc_buf_free(tmp);
        LOGI(CR_TAG, "base64 decode(%s) failed", encrypted);
        return RC_ERROR_DECRYPT;
    }
    
    decrypted->total = RSA_size(crypt->rsa) + 1;
    decrypted->length = 0;
    decrypted->free = 1;
    decrypted->usr_buf = (char*)rc_malloc(decrypted->total);
    ret = RSA_public_decrypt(buflen - 1, rc_buf_head_ptr(tmp), rc_buf_head_ptr(decrypted), crypt->rsa, RSA_PKCS1_PADDING);
    rc_buf_free(tmp); // free tmp cache
    if (ret < 0) {
        LOGI(CR_TAG, "RSA_public_decrypt failed with ret=%d", ret);
        rc_buf_free(decrypted);
        return RC_ERROR_DECRYPT;
    }
    
    decrypted->length = ret;
    return RC_SUCCESS;
}

int rc_rsa_crypt_uninit(rsa_crypt rsa)
{
    rc_rsa_crypt_t* crypt = (rc_rsa_crypt_t*)rsa;
    if (crypt != NULL) {
        if (crypt->keybio != NULL) {
            BIO_free_all(crypt->keybio);
        }

        if (crypt->rsa != NULL) {
            RSA_free(crypt->rsa);
        }

        rc_free(crypt);
    }
    
    return RC_SUCCESS;
}

#else

rsa_crypt rc_rsa_crypt_init(const char* public_key)
{
    return NULL;
}

int rc_rsa_encrypt(rsa_crypt rsa, const char* decrypted, int len, rc_buf_t* encrypted)
{
    return RC_ERROR_NOT_IMPLEMENT;
}

int rc_rsa_decrypt(rsa_crypt rsa, const char* encrypted, int len, rc_buf_t* decrypted)
{
    return RC_ERROR_NOT_IMPLEMENT;
}

int rc_rsa_crypt_uninit(rsa_crypt rsa)
{
    return RC_SUCCESS;
}

#endif
