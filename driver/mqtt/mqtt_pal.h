/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     mqtt_pal.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-12 06:13:04
 *
 */

#ifndef _QUARK_MQTT_PAL_H_
#define _QUARK_MQTT_PAL_H_

/*
MIT License

Copyright(c) 2018 Liam Bindle

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <arpa/inet.h>
#include <limits.h>
#include <stdarg.h>

#include "rc_buf.h"
#include "rc_timer.h"
#include "rc_mutex.h"

#define MQTT_PAL_HTONS(s) htons(s)
#define MQTT_PAL_NTOHS(s) ntohs(s)

#define MQTT_PAL_TIME() time(NULL)

typedef time_t mqtt_pal_time_t;
typedef rc_mutex mqtt_pal_mutex_t;

#define MQTT_PAL_MUTEX_INIT(mtx_ptr) _mqtt_pal_mutex_init(mtx_ptr)
#define MQTT_PAL_MUTEX_LOCK(mtx_ptr) _mqtt_pal_mutex_lock(mtx_ptr)
#define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) _mqtt_pal_mutex_unlock(mtx_ptr)

#define MQTT_PAL_RECV_TIMEOUT_MS 300

typedef int mqtt_pal_socket_handle;

#define INT_QOS_TO_ENUM(qos)         \
    ((qos) == 0 ? MQTT_PUBLISH_QOS_0 \
                : ((qos) == 1 ? MQTT_PUBLISH_QOS_1 : MQTT_PUBLISH_QOS_2))

void _mqtt_pal_mutex_init(rc_mutex *pm);
void _mqtt_pal_mutex_lock(rc_mutex *pm);
void _mqtt_pal_mutex_unlock(rc_mutex *pm);

/**
 * @brief Sends all the bytes in a buffer.
 * @ingroup pal
 *
 * @param[in] fd The file-descriptor (or handle) of the socket.
 * @param[in] buf A pointer to the first byte in the buffer to send.
 * @param[in] len The number of bytes to send (starting at \p buf).
 * @param[in] flags Flags which are passed to the underlying socket.
 *
 * @returns The number of bytes sent if successful, an \ref MQTTErrors
 * otherwise.
 */
ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void *buf, size_t len,
                         int flags);

/**
 * @brief Non-blocking receive all the byte available.
 * @ingroup pal
 *
 * @param[in] fd The file-descriptor (or handle) of the socket.
 * @param[in] buf A pointer to the receive buffer.
 * @param[in] bufsz The max number of bytes that can be put into \p buf.
 * @param[in] flags Flags which are passed to the underlying socket.
 *
 * @returns The number of bytes received if successful, an \ref MQTTErrors
 * otherwise.
 */
ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void *buf, size_t bufsz,
                         int flags);

#endif
