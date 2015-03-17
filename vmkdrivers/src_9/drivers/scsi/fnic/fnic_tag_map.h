/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 *
 * This file is derived from block/blk-tag.c, scsi/scsi_tcq.h, bitops.h and
 * others from Linux release 2.6.27-rc5, and shares copyright with the
 * original authors (Jens Axboe and others).
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * [Insert appropriate license here when releasing outside of Cisco]
 * $Id: fnic_tag_map.h 101907 2012-04-25 08:16:26Z sebaddel $
 */

#ifndef _FNIC_TAG_MAP_H_
#define _FNIC_TAG_MAP_H_

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <asm/atomic.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_tcq.h>
#include <scsi/libfc.h>
#include <scsi/fc_frame.h>
#include "fnic.h"

#define scsi_init_shared_tag_map         fnic_init_shared_tag_map
#define scsi_host_start_tag(shost, sc)   fnic_host_start_tag(shost, sc)
#define scsi_host_find_tag               fnic_host_find_tag
#define scsi_host_end_tag(shost, sc)     fnic_host_end_tag(shost, sc)
#define scsi_free_shared_tag_map(shost)  fnic_free_shared_tag_map(shost)
#define scsi_cmd_get_tag(sc)             (CMD_TAG(sc))
#define	scsi_set_tagged_support(sdev)
#define test_and_set_bit_lock            test_and_set_bit
#define clear_bit_unlock(nr, addr) \
do { \
	barrier(); \
	clear_bit(nr, addr); \
} while (0); \

struct fnic_host_tag {
	struct scsi_cmnd **tag_index;	/* map of busy tags */
	unsigned long *tag_map;		/* bit map of free/busy tags */
	int max_depth;			/* what we will send to device */
	int next_tag;			/* Most recently allocated tag */
};

int fnic_init_shared_tag_map(struct Scsi_Host *shost, int depth);
void fnic_free_shared_tag_map(struct Scsi_Host *shost);
int fnic_host_start_tag(struct Scsi_Host *shost, struct scsi_cmnd *sc);
void fnic_host_end_tag(struct Scsi_Host *shost, struct scsi_cmnd *sc);
struct scsi_cmnd *fnic_host_find_tag(struct Scsi_Host *shost, int tag);

#endif /* _FNIC_TAG_MAP_H_ */
