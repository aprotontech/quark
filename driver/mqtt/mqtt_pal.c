/*
 * **************************************************************************
 *
 *  Copyright (c) 2022 aproton.tech, Inc. All Rights Reserved
 *
 * **************************************************************************
 *
 *  @file     mqtt_pal.c
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2022-03-20 08:46:42
 *
 */

#include "mqtt.h"
#include "mqtt_pal.h"
#include "rc_system.h"

#include <errno.h>
#include <memory.h>
#include <sys/socket.h>

void _mqtt_pal_mutex_init(rc_mutex* pm) { *pm = rc_mutex_create(NULL); }

void _mqtt_pal_mutex_lock(rc_mutex* pm) {
    if (pm != NULL) {
        rc_mutex_lock(*pm);
    }
}

void _mqtt_pal_mutex_unlock(rc_mutex* pm) {
    if (pm != NULL) {
        rc_mutex_unlock(*pm);
    }
}

ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void* buf, size_t len,
                         int flags) {
    enum MQTTErrors error = 0;
    size_t sent = 0;
    while (sent < len) {
        ssize_t rv = send(fd, buf + sent, len - sent, flags);
        if (rv < 0) {
            if (errno == EAGAIN) {
                /* should call send later again */
                break;
            }
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        if (rv == 0) {
            /* is this possible? maybe OS bug. */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        sent += (size_t)rv;
    }
    if (sent == 0) {
        return error;
    }
    return sent;
}

ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void* buf, size_t bufsz,
                         int flags) {
    const void* const start = buf;
    enum MQTTErrors error = 0;
    ssize_t rv;
    mstime_t start_time = rc_get_mstick();
    do {
        rv = recv(fd, buf, bufsz, flags);
        if (rv == 0) {
            /*
             * recv returns 0 when the socket is (half) closed by the peer.
             *
             * Raise an error to trigger a reconnect.
             */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        if (rv < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* should call recv later again */
                break;
            }
            /* an error occurred that wasn't "nothing to read". */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        buf = (char*)buf + rv;
        bufsz -= rv;
    } while (bufsz > 0 &&
             (rc_get_mstick() - start_time) >= MQTT_PAL_RECV_TIMEOUT_MS);
    if (buf == start) {
        return error;
    }
    return buf - start;
}