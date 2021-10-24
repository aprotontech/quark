

#ifndef _QUARK_RTOS_H_
#define _QUARK_RTOS_H_

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

#endif //__QUARK_RTTHREAD__

#ifdef __QUARK_FREERTOS__
#endif // __QUARK_FREERTOS__


#endif
