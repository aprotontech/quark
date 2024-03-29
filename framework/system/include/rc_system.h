

#ifndef _QUARK_SYSTEM_H_
#define _QUARK_SYSTEM_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "os_platform.h"

#ifdef __QUARK_RTTHREAD__
#include "rc_rtos.h"
#endif

#ifdef __QUARK_LINUX__
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>

#endif

#ifdef __QUARK_FREERTOS__
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_mac.h"

#endif

#ifndef RC_ERROR_INVALIDATE_INPUT
#define RC_ERROR_INVALIDATE_INPUT 1004
#endif

#endif
