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
 * Hash table implemented on top of a link list.
 */
#ifndef _NX_HASH_TABLE_H
#define _NX_HASH_TABLE_H

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "nx_types.h"

#define	NX_HASH_TBL_NODE_INSERTED	1
#define	NX_HASH_TBL_NODE_EXISTS		2

typedef struct {
        uint8_t         ip_version;
        ip_addr_t       daddr;
        ip_addr_t       saddr;
	uint16_t 	sport;
	uint16_t 	dport;
} nx_host_key_t;


typedef struct {
	spinlock_t		lock;
	struct list_head	head;
} nx_hbucket_head_t;

typedef struct {
	struct list_head	list;
	void			*key;
	void			*data;
} nx_hash_tbl_node_t;

typedef struct {
	int (*hash)(void *key);
        int (*compare_keys)(void *key1, void *key2);
        void (*destroy_cb)(nx_hash_tbl_node_t *node);
} nx_hash_tbl_ops_t;

typedef struct {
	nx_hbucket_head_t	*buckets;
	int			bucket_cnt;
	nx_hash_tbl_ops_t	*ops;
} nx_hash_tbl_t;

#define INIT_NX_HBUCKET_HEAD(PTR)		\
	do {					\
		spin_lock_init(&(PTR)->lock);	\
		INIT_LIST_HEAD(&(PTR)->head);	\
	} while (0)

/*
 *
 */
static inline void nx_hbucket_add_tail(struct list_head *new,
				       nx_hbucket_head_t *head)
{
	spin_lock_bh(&head->lock);
	list_add_tail(new, &head->head);
	spin_unlock_bh(&head->lock);
}

/*
 *
 */
static inline void nx_hbucket_del_init(struct list_head *node,
				  nx_hbucket_head_t *head)
{
	spin_lock_bh(&head->lock);
	list_del_init(node);
	spin_unlock_bh(&head->lock);
}

int nx_hash_tbl_create(nx_hash_tbl_t *tbl, int bucket_cnt,
		       nx_hash_tbl_ops_t *ops);
void nx_hash_tbl_destroy(nx_hash_tbl_t *tbl);
int nx_hash_tbl_insert(nx_hash_tbl_t *tbl, nx_hash_tbl_node_t *node);
nx_hash_tbl_node_t *nx_hash_tbl_get(nx_hash_tbl_t *tbl, void *key);
nx_hash_tbl_node_t *nx_hash_tbl_delete(nx_hash_tbl_t *tbl, void *key);
int nx_cmp_ip_key(void *a1, void *a2);

#endif /* _NX_HASH_TABLE_H */
