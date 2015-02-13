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
 * Memory Pool implemented using a link list.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include "unm_nic.h"
#include "unm_inc.h"
#include "nx_mem_pool.h"
#include "nx_types.h"


/*
 *
 */
int nx_mem_pool_create(nx_mem_pool_t *pool, int max_entries, int entry_size)
{
	int			i;
	nx_mem_pool_node_t	*node;

	nx_nic_print6(NULL, "Creating Memory Pool: Max[%u], EntrySize[%u]\n",
		 max_entries, entry_size);

	memset(pool, 0, sizeof(nx_mem_pool_t));
        /*Adjustment done to make 8 byte alligned*/
	pool->offset = ((sizeof(nx_mem_pool_node_t) +
			(NX_MEM_POOL_ALIGNMENT - 1)) / NX_MEM_POOL_ALIGNMENT) * NX_MEM_POOL_ALIGNMENT;

	nx_nic_print6(NULL, "Entry Size[%u], offset[%u]\n",
		      entry_size, pool->offset);

	entry_size += pool->offset;

	pool->max_entries = max_entries;
	pool->entry_size = entry_size;
	INIT_LIST_HEAD(&pool->free_list);
	spin_lock_init(&pool->lock);

	/* Allocate the Free pool */
	for (i = 0; i < max_entries; i++) {
		node = (nx_mem_pool_node_t *)kmalloc(entry_size, GFP_KERNEL);
		if (node == NULL) {
			nx_nic_print3(NULL, "Memory allocation for entries "
				      "failed\n");
			nx_mem_pool_destroy(pool);
			return (-ENOMEM);
		}
		list_add(&node->list, &pool->free_list);
		node->pool = pool;
	}

	return (0);
}

/*
 *
 */
void nx_mem_pool_destroy(nx_mem_pool_t *pool)
{
	nx_mem_pool_node_t	*node;
	struct list_head 	*node1;
	struct list_head	*node2;

        spin_lock_bh(&pool->lock);

	if (pool->allocated) {
		nx_nic_print3(NULL, "Destroying a memory pool with "
			      "outstanding frees\n");
		BUG();
	}

	list_for_each_safe(node1, node2, &pool->free_list) {
		node = list_entry(node1, nx_mem_pool_node_t, list);
		list_del_init(&node->list);
		kfree(node);
	}

	spin_unlock_bh(&pool->lock);
}

/*
 *
 */
void *nx_mem_pool_alloc(nx_mem_pool_t *pool)
{
	nx_mem_pool_node_t	*node;
	void			*ptr = NULL;


        spin_lock_bh(&pool->lock);

	if (!list_empty(&pool->free_list)) {
		node = list_entry(pool->free_list.next, nx_mem_pool_node_t,
				  list);
		list_del_init(&node->list);
		pool->allocated++;
		ptr = ((void *)node) + pool->offset;
	}

        spin_unlock_bh(&pool->lock);

	return (ptr);
}

/*
 *
 */
void nx_mem_pool_free(nx_mem_pool_t *pool, void *ptr)
{
	nx_mem_pool_node_t	*node;

	node = (nx_mem_pool_node_t *)(ptr - pool->offset);
	if (node->pool != pool) {
		nx_nic_print3(NULL, "Freeing a buffer to the wrong pool\n");
		BUG();
		return;
	}

        spin_lock_bh(&pool->lock);

	list_add_tail(&node->list, &pool->free_list);
	pool->allocated--;
	if (pool->allocated < 0) {
		nx_nic_print3(NULL, "Too many frees\n");
		BUG();
	}

        spin_unlock_bh(&pool->lock);
}
