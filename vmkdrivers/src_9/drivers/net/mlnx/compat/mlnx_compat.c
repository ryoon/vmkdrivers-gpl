/*
 * Copyright (c) 2011-2012 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "mlnx_compat.h"


/************************************************************************/
#include "hash.c"
#include "linux/lib/bitmap.c"
#include "linux/lib/rbtree.c"


/************************************************************************/
/* Ethtool */
int __ethtool_get_settings(struct net_device *dev, struct ethtool_cmd *p_cmd)
{
	int err;

	if (!dev->ethtool_ops)
		return -EOPNOTSUPP;
	if (!dev->ethtool_ops->get_settings)
		return -EOPNOTSUPP;

	err = dev->ethtool_ops->get_settings(dev, p_cmd);
	return err;
}

void ethtool_cmd_speed_set(struct ethtool_cmd *ep,
		__u32 speed)
{
	ep->speed = (__u16)speed;
#ifndef __VMKERNEL_MODULE__
	ep->speed_hi = (__u16)(speed >> 16);
#endif	/* NOT __VMKERNEL_MODULE__ */
}

__u32 ethtool_cmd_speed(const struct ethtool_cmd *ep)
{
#ifdef __VMKERNEL_MODULE__
	return ep->speed;
#else
	return (ep->speed_hi << 16) | ep->speed;
#endif	/* __VMKERNEL_MODULE__ */
}


/************************************************************************/
/* Hex dump */
int hex_to_bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return -1;
}


/************************************************************************/
/* Pages */
unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return __get_free_pages(gfp_mask | __GFP_ZERO, 0);
}


/************************************************************************/
/* Work Queues */
int __cancel_delayed_work(struct delayed_work *dwork)
{
	int ret;

	if(NULL == dwork) {
		return 0;
	}

	ret = del_timer(&dwork->timer);
	if (ret) {
		/*
		 * Timer was still pending
		 */
		clear_bit(__WORK_PENDING, &dwork->work.pending);
	}
	return ret;
}


/************************************************************************/
/* Mem */
void *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p;

	p = kmalloc(len, gfp);
	if (p)
		memcpy(p, src, len);
	return p;
}


/************************************************************************/
/* File */
/*
 * This is used by subsystems that don't want seekable
 * file descriptors. The function is not supposed to ever fail, the only
 * reason it returns an 'int' and not 'void' is so that it can be plugged
 * directly into file_operations structure.
 */
int nonseekable_open(struct inode *inode, struct file *filp)
{
	filp->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}

