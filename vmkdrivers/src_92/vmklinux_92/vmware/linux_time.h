
/* ****************************************************************
 * Portions Copyright 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_time.h --
 *
 *      Linux kernel time compatibility.
 */

#ifndef _LINUX_TIME_H_
#define _LINUX_TIME_H_

extern vmk_TimerQueue vmklnxTimerQueue;
extern void LinuxTime_Init(void);
extern void LinuxTime_Cleanup(void);

#endif // _LINUX_TIME_H_
