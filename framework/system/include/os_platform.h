/*
 * **************************************************************************
 * 
 *  Copyright (c) 2021 aproton.tech, Inc. All Rights Reserved
 * 
 * **************************************************************************
 * 
 *  @file     os_platform.h
 *  @author   kuper - <kuper@aproton.tech>
 *  @data     2021-12-26 09:19:29
 * 
 */

#ifndef _QUARK_OS_PLATFORM_H_
#define _QUARK_OS_PLATFORM_H_


#ifndef QUARK_SYSTEM
#define QUARK_SYSTEM 1
#endif

#if QUARK_SYSTEM == 1
#define __QUARK_LINUX__ 1
#endif

#if QUARK_SYSTEM == 2
#define __QUARK_FREERTOS__ 1
#endif

#endif