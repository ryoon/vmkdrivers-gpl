/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
/*
 * NetXen:
 *
 * Provides a cache for destination lookup.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include "unm_nic.h"
#include "unm_inc.h"
#include "nx_hash_table.h"
#include "nx_types.h"


/*
 *
 */
int nx_hash_tbl_create(nx_hash_tbl_t *tbl, int bucket_cnt,
		       nx_hash_tbl_ops_t *ops)
{
	int	alloc_size;
	int	i;

	nx_nic_print6(NULL, "Creating Hash table: Buckets[%u]\n", bucket_cnt);

	memset(tbl, 0, sizeof(nx_hash_tbl_t));

	tbl->bucket_cnt = bucket_cnt;

	/* Allocate and initialize the table buckets. */
	alloc_size = sizeof (nx_hbucket_head_t) * bucket_cnt;
	tbl->buckets = (nx_hbucket_head_t *)vmalloc(alloc_size);
	if (tbl->buckets == NULL) {
		nx_nic_print3(NULL, "Hash table alloc failed\n");
		return (-ENOMEM);
	}

	for (i = 0; i < bucket_cnt; i++) {
		INIT_NX_HBUCKET_HEAD(&tbl->buckets[i]);
	}

	tbl->ops = ops;

	return (0);
}

/*
 *
 */
void nx_hash_tbl_destroy(nx_hash_tbl_t *tbl)
{
	int			i;
	nx_hash_tbl_node_t	*node;
	struct list_head 	*node1;
	struct list_head	*node2;

	for (i = 0; i < tbl->bucket_cnt; i++) {
		spin_lock_bh(&tbl->buckets[i].lock);

		list_for_each_safe(node1, node2, &tbl->buckets[i].head) {
			node = list_entry(node1, nx_hash_tbl_node_t, list);
			list_del_init(&node->list);
			tbl->ops->destroy_cb(node);
		}

		spin_unlock_bh(&tbl->buckets[i].lock);
	}
	vfree(tbl->buckets);
}

/*
 *
 */
static inline nx_hbucket_head_t *get_bucket(nx_hash_tbl_t *tbl, void *key)
{
	uint32_t	hash;

	hash = tbl->ops->hash(key) & (tbl->bucket_cnt - 1);
	return (&tbl->buckets[hash]);
}

/*
 *
 */
static inline void list_insert_before(struct list_head *new,
				      struct list_head *before)
{
	new->next = before;
	new->prev = before->prev;
	new->prev->next = new;
	before->prev = new;
}

/*
 *
 */
int nx_hash_tbl_insert(nx_hash_tbl_t *tbl, nx_hash_tbl_node_t *node)
{
	nx_hash_tbl_node_t	*entry = NULL;
	nx_hbucket_head_t	*head;
	struct list_head 	*node1;
	struct list_head	*node2;
	int			rv;


	head = get_bucket(tbl, node->key);

	spin_lock_bh(&head->lock);

	list_for_each_safe(node1, node2, &head->head) {
		entry = list_entry(node1, nx_hash_tbl_node_t, list);
		rv = tbl->ops->compare_keys(node->key, entry->key);
		if (rv == 0) {
			rv = NX_HASH_TBL_NODE_EXISTS;
			goto done;
		}
		if (rv < 0) {
			entry = NULL;
		}
	}

	if (entry) {
		list_insert_before(&node->list, &entry->list);
	} else {
		list_add_tail(&node->list, &head->head);
	}
	rv = NX_HASH_TBL_NODE_INSERTED;

  done:
	spin_unlock_bh(&head->lock);
	return (rv);
}

/*
 *
 */
nx_hash_tbl_node_t *nx_hash_tbl_get(nx_hash_tbl_t *tbl, void *key)
{
	nx_hash_tbl_node_t	*entry = NULL;
	nx_hbucket_head_t	*head;
	struct list_head 	*node1;
	struct list_head	*node2;
	int			rv;

	head = get_bucket(tbl, key);

	spin_lock_bh(&head->lock);

	list_for_each_safe(node1, node2, &head->head) {
		entry = list_entry(node1, nx_hash_tbl_node_t, list);
		rv = tbl->ops->compare_keys(key, entry->key);
		if (rv == 0) {
			goto done;
		}
		entry = NULL;
	}

  done:
	spin_unlock_bh(&head->lock);

	return (entry);
}

/*
 *
 */
nx_hash_tbl_node_t *nx_hash_tbl_delete(nx_hash_tbl_t *tbl, void *key)
{
	nx_hash_tbl_node_t	*entry = NULL;
	nx_hbucket_head_t	*head;
	struct list_head 	*node1;
	struct list_head	*node2;
	int			rv;

	head = get_bucket(tbl, key);

	spin_lock_bh(&head->lock);

	list_for_each_safe(node1, node2, &head->head) {
		entry = list_entry(node1, nx_hash_tbl_node_t, list);
		rv = tbl->ops->compare_keys(key, entry->key);
		if (rv == 0) {
			list_del_init(&entry->list);
			goto done;
		}
		entry = NULL;
	}

  done:
	spin_unlock_bh(&head->lock);
	return (entry);
}

/*
 *
 */
int nx_cmp_ip_key(void *a1, void *a2)
{
        int     rv = 0;
        int     i;

	nx_host_key_t *key1 = (nx_host_key_t *)a1;
	nx_host_key_t *key2 = (nx_host_key_t *)a2;
        rv = key1->ip_version - key2->ip_version;
        if (rv) {
                return (rv);
        }
        if (key1->ip_version == NX_IP_VERSION_V4) {
                if (key1->daddr.v4 != key2->daddr.v4) {
                        return ((int)key1->daddr.v4 - (int)key2->daddr.v4);
                } 
		if (key1->sport != key2->sport) {
			return (key1->sport - key2->sport);
		}
		if (key1->dport != key2->dport) {
			return (key1->dport - key2->dport);
		}
                return 0;
        } else {
        	for (i = 0; i < 4; i++) {
                	rv = key1->daddr.v6[i] - key2->daddr.v6[i];
                	if (rv) {
                    		return (rv);
                	}
                	rv = key1->saddr.v6[i] - key2->saddr.v6[i];
                	if (rv) {
                    		return (rv);
                	}
            	}
	   	if (key1->sport != key2->sport) {
			return (key1->sport - key2->sport);
	  	}
		if (key1->dport != key2->dport) {
			return (key1->dport - key2->dport);
		}
                return 0;
        }
        return rv;
}

