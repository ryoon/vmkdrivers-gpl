/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2004-2007 Adaptec, Inc
 *
 * Copyright (c) 2004-2007 Adaptec, Inc. (aacraid@adaptec.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *   csmi.c
 *
 * Abstract: All CSMI IOCTL processing is handled here.
 */

/*
 * Include Files
 */

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <asm/uaccess.h> /* For copy_from_user()/copy_to_user() definitions */
#include <linux/slab.h> /* For kmalloc()/kfree() definitions */
#include "aacraid.h"
#include "fwdebug.h"
# include <scsi/scsi.h>
# include <scsi/scsi_host.h>
# include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
#include <linux/syscalls.h>
#endif

