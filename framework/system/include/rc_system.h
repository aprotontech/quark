

#ifndef _AIE_SYSTEM_H_
#define _AIE_SYSTEM_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if QUARK_SYSTEM == 1
#define __QUARK_FREERTOS__ 1
#endif

#ifdef __QUARK_RTTHREAD__
#include "rc_rtos.h"
#endif

#ifdef __QUARK_LINUX__
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

#ifdef __QUARK_FREERTOS__
#endif

#ifndef RC_ERROR_INVALIDATE_INPUT
#define RC_ERROR_INVALIDATE_INPUT 1004
#endif

#endif
