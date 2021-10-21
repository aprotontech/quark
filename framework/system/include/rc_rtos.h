

#ifndef _AIE_RTOS_H_
#define _AIE_RTOS_H_

#ifdef __QUARK_RTTHREAD__

#include <stdbool.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>

#include <rthw.h>

#include <wlan_dev.h>
#include <wlan_mgnt.h>


#define RT_THREAD_STACK_SIZE 2048
#define RT_THREAD_PROIORITY 8

#endif

#endif
