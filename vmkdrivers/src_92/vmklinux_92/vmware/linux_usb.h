/* ****************************************************************
 * Portions Copyright 2008, 2010-2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_usb.h --
 *
 *      vmklinux USB utility functions
 */

#ifndef _LINUX_USB_H_
#define _LINUX_USB_H_

#include "vmkapi.h"

#include <linux/compiler.h>
#include <vmklinux_92/vmklinux_usb.h>

/* 
 * Stress option handles
 */
extern vmk_StressOptionHandle stressUSBBulkDelayProcessURB;
extern vmk_StressOptionHandle stressUSBBulkURBFakeTransientError;
extern vmk_StressOptionHandle stressUSBDelayProcessTD;
extern vmk_StressOptionHandle stressUSBFailGPHeapAlloc;
extern vmk_StressOptionHandle stressUSBStorageDelaySCSIDataPhase;
extern vmk_StressOptionHandle stressUSBStorageDelaySCSITransfer;

#endif /*_LINUX_USB_H_ */
