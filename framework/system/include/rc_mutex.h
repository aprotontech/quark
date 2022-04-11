/***************************************************************************
 *
 * Copyright (c) 2019 aproton.tech, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file     rc_mutex.h
 * @author   kuper - kuper@aproton.tech
 * @data     2019-12-26 23:18:07
 * @version  0
 * @brief
 *
 **/

#ifndef _QUARK_MUTEX_H_
#define _QUARK_MUTEX_H_

typedef void* rc_mutex;

rc_mutex rc_mutex_create(void* attr);

int rc_mutex_lock(rc_mutex mutex);
int rc_mutex_unlock(rc_mutex mutex);
int rc_mutex_destroy(rc_mutex mutex);


#endif
