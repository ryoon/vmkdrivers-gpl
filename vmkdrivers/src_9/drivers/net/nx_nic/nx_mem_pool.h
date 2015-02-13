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
#ifndef _NX_MEM_POOL_H
#define _NX_MEM_POOL_H

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#define	NX_MEM_POOL_ALIGNMENT	8

typedef struct {
	struct list_head	free_list;
	int			max_entries;
	int			entry_size;
	int			allocated;
	int			offset;
	spinlock_t		lock;
} nx_mem_pool_t;

typedef struct {
	struct list_head	list;
	nx_mem_pool_t		*pool;
} nx_mem_pool_node_t;

int nx_mem_pool_create(nx_mem_pool_t *pool, int max_entries, int entry_size);
void nx_mem_pool_destroy(nx_mem_pool_t *pool);
void *nx_mem_pool_alloc(nx_mem_pool_t *pool);
void nx_mem_pool_free(nx_mem_pool_t *pool, void *ptr);

#endif /* _NX_MEM_POOL_H */
