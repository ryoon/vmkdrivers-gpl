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
 */
#include "fnic_tag_map.h"

static int fnic_init_tag_map(struct fnic_host_tag *fht, int depth)
{
	struct scsi_cmnd **tag_index;
	unsigned long *tag_map;
	int nr_ulongs;

	tag_index = kzalloc(depth * sizeof(struct scsi_cmnd *), GFP_ATOMIC);
	if (!tag_index)
		goto fail;

	nr_ulongs = ALIGN(depth, BITS_PER_LONG) / BITS_PER_LONG;
	tag_map = kzalloc(nr_ulongs * sizeof(unsigned long), GFP_ATOMIC);
	if (!tag_map)
		goto fail;

	fht->max_depth = depth;
	fht->tag_index = tag_index;
	fht->tag_map = tag_map;

	return 0;
fail:
	kfree(tag_index);
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
	int tag;

	do {
		tag = find_first_zero_bit(fht->tag_map, fht->max_depth);
		if (tag >= fht->max_depth)
			return SCSI_NO_TAG;

	} while (test_and_set_bit_lock(tag, fht->tag_map));

	fht->tag_index[tag] = sc;
	CMD_TAG(sc) = tag;

	return tag;
}

void fnic_host_end_tag(struct Scsi_Host *shost, struct scsi_cmnd *sc)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);
	struct fnic_host_tag *fht = fnic->tags;
	int tag = CMD_TAG(sc);

	BUG_ON(tag == SCSI_NO_TAG);

	if (unlikely(tag >= fht->max_depth))
		return;

	if (unlikely(fht->tag_index[tag] == NULL))
		printk(KERN_ERR "%s: tag %d is missing\n",
		       __func__, tag);

	fht->tag_index[tag] = NULL;
	CMD_TAG(sc) = SCSI_NO_TAG;

	if (unlikely(!test_bit(tag, fht->tag_map))) {
		printk(KERN_ERR "%s: attempt to clear non-busy tag (%d)\n",
		       __func__, tag);
		return;
	}

	clear_bit_unlock(tag, fht->tag_map);
}

static inline struct scsi_cmnd *fnic_find_tag(struct fnic_host_tag *fht,
					      int tag)
{
	if (unlikely(fht == NULL || tag >= fht->max_depth))
		return NULL;
	return fht->tag_index[tag];
}

struct scsi_cmnd *fnic_host_find_tag(struct Scsi_Host *shost, int tag)
{
	struct fc_lport *lp = shost_priv(shost);
	struct fnic *fnic = lport_priv(lp);

	if (tag != SCSI_NO_TAG)
		return fnic_find_tag(fnic->tags, tag);
	return NULL;
}

