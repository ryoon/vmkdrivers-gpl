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
 * $Id: fnic_tag_map.c 122966 2013-02-07 20:43:04Z hiralpat $
 */

#include "fnic_tag_map.h"

static int fnic_init_tag_map(struct fnic_host_tag *fht, int depth)
{
	struct scsi_cmnd **tag_index = NULL;
	unsigned long *tag_map = NULL;
	int nr_ulongs;

	tag_index = kzalloc(depth * sizeof(struct scsi_cmnd *), GFP_ATOMIC);
	if (!tag_index)
		return -ENOMEM;
		/* goto fail; */

	nr_ulongs = ALIGN(depth, BITS_PER_LONG) / BITS_PER_LONG;
	tag_map = kzalloc(nr_ulongs * sizeof(unsigned long), GFP_ATOMIC);
	if (!tag_map)
		goto fail;

	fht->max_depth = depth;
	fht->tag_index = tag_index;
	fht->tag_map = tag_map;
	fht->next_tag = 1;

	return 0;
fail:
	if (tag_index) {
		kfree(tag_index);
	}
	if (tag_map) {
		kfree(tag_map);
	}
	return -ENOMEM;
}

int fnic_init_shared_tag_map(struct Scsi_Host *shost, int depth)
{
	struct fnic_host_tag *fht;
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);

	fht = kmalloc(sizeof(struct fnic_host_tag), GFP_ATOMIC);
	if (!fht)
		goto fail;

	if (fnic_init_tag_map(fht, depth))
		goto fail;

	fnic->tags = fht;

	return 0;
fail:
	kfree(fht);
	return 1;
}

static inline void fnic_free_tags(struct fnic_host_tag *fht)
{

	BUG_ON(find_first_bit(fht->tag_map, fht->max_depth) <
	       fht->max_depth);

	kfree(fht->tag_index);
	fht->tag_index = NULL;

	kfree(fht->tag_map);
	fht->tag_map = NULL;

	kfree(fht);

}

void fnic_free_shared_tag_map(struct Scsi_Host *shost)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);

	fnic_free_tags(fnic->tags);
}

int fnic_host_start_tag(struct Scsi_Host *shost, struct scsi_cmnd *sc)
{

	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);
	struct fnic_host_tag *fht = fnic->tags;
	struct fnic_stats fnic_stats = fnic->fnic_stats;
	int tag;
	unsigned long flags;

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	tag = find_next_zero_bit(fht->tag_map, fht->max_depth, fht->next_tag);
	if (((tag < 1) || (tag >= fht->max_depth)) && (fht->next_tag > 1)) {
		tag = find_next_zero_bit(fht->tag_map, fht->max_depth, 1);
	}
	if ((tag < 1) || (tag >= fht->max_depth)) {
		atomic64_inc(&fnic_stats.io_stats.tag_alloc_failures);
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		printk(KERN_ERR "Tag allocation failure next_tag (0x%x)\n",
		       fht->next_tag);
		return SCSI_NO_TAG;
	}
	fht->next_tag = (tag < (fht->max_depth - 1)) ? tag + 1 : 1;
	set_bit(tag, fht->tag_map);
	fht->tag_index[tag] = sc;
	CMD_TAG(sc) = tag;
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);

	return tag;
}

void fnic_host_end_tag(struct Scsi_Host *shost, struct scsi_cmnd *sc)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);
	struct fnic_host_tag *fht = fnic->tags;
	int tag = CMD_TAG(sc);
	unsigned long flags;

	BUG_ON(tag == SCSI_NO_TAG);

	if (unlikely((tag < 1) || (tag >= fht->max_depth))) {
		printk(KERN_ERR "Tag 0x%x is out of range for sc 0x%p\n",
		       tag, sc);
		return;
	}

	spin_lock_irqsave(&fnic->fnic_lock, flags);
	if (unlikely(fht->tag_index[tag] == NULL)) {
		printk(KERN_ERR "Tag 0x%x is missing sc 0x%p\n",
		       tag, sc);
	}
	if (unlikely(fht->tag_index[tag] != sc)) {
		printk(KERN_ERR "Tag 0x%x does not match sc 0x%p  0x%p\n",
		      tag, sc, fht->tag_index[tag]);
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		return;
	}

	fht->tag_index[tag] = NULL;
	CMD_TAG(sc) = SCSI_NO_TAG;

	if (unlikely(!test_bit(tag, fht->tag_map))) {
		printk(KERN_ERR "Attempt to clear non-busy tag (0x%x)\n", tag);
		spin_unlock_irqrestore(&fnic->fnic_lock, flags);
		return;
	}

	clear_bit(tag, fht->tag_map);
	spin_unlock_irqrestore(&fnic->fnic_lock, flags);
}

static inline struct scsi_cmnd *fnic_find_tag(struct fnic_host_tag *fht,
					      int tag)
{
	/* Called with fnic_lock held */
	if (unlikely(fht == NULL || tag >= fht->max_depth || tag < 1))
		return NULL;
	return fht->tag_index[tag];
}

struct scsi_cmnd *fnic_host_find_tag(struct Scsi_Host *shost, int tag)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);

	/* Called with fnic_lock held */
	if (tag != SCSI_NO_TAG)
		return fnic_find_tag(fnic->tags, tag);
	return NULL;
}

