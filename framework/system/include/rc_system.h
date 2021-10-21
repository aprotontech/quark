

#ifndef _AIE_SYSTEM_H_
#define _AIE_SYSTEM_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "rc_rtos.h"

#ifndef __QUARK_RTTHREAD__
#include <pthread.h>
#include <arpa/inet.h>
#endif

#ifndef RC_ERROR_INVALIDATE_INPUT
#define RC_ERROR_INVALIDATE_INPUT 1004
#endif

#endif
