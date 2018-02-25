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


#include <linux/slab.h>
#include <linux/errno.h>
#include "hash.h"


/************************************************************************/
struct hash_node {
	unsigned long key;		// key for lookup
	void *p_data;			// pointer to stored data
	struct hash_node *p_next;	// pointer to next node
};


/************************************************************************/
#define MINIMUM_HASH_TABLE_SIZE		512
/* TODO: Need to fix expand latter */
#ifdef __VMKERNEL_HASH_EXPAND_ENABLE__
#define EXPAND_HASH_TABLE_LIMIT_ORDER	1
#endif	/* __VMKERNEL_HASH_EXPAND_ENABLE__ */


/************************************************************************/
static unsigned long hash(hdl_hash_t p_table, unsigned long key)
{
	return (key & p_table->mask);
}

/************/
void hash_print(hdl_hash_t p_table)
{
	hash_node_t *p_node;
	unsigned long i;

	printk("HASH ID=%p\n", p_table);
	// if empty
	if (p_table->size == 0) {
		printk("HASH is EMPTY!\n");
		return;
	}

	printk("HASH size=%lu\n", p_table->size);
	printk("HASH mask=%lu\n", p_table->mask);
	printk("HASH entries=%lu\n", p_table->entries);
	printk("HASH bucket:\n");
	for (i = 0; i < p_table->size; ++i) {
		p_node = p_table->bucket[i];
		printk("[%lu]    ", i);
		while (p_node) {
			printk("Key=0x%lx --> ", p_node->key);
			p_node = p_node->p_next;
		}
		printk("\n");
	}
}

/************/
int hash_init(hdl_hash_t p_table, unsigned long table_size)
{
	p_table->entries = 0;
	p_table->size = 2;
	p_table->mask = p_table->size - 1;

	// round to power of 2
	while (p_table->size < table_size)
		p_table->size <<= 1;
	p_table->mask = p_table->size - 1;

	// allocate
	p_table->bucket = (hash_node_t **)kcalloc(p_table->size,
			sizeof(hash_node_t *), GFP_KERNEL);
	if (!p_table->bucket)
		return -ENOMEM;
	return 0;
}

/************/
void hash_destroy(hdl_hash_t p_table)
{
	hash_node_t *p_node, *p_prev_node;
	unsigned long i;

	for (i = 0; i < p_table->size; ++i) {
		p_node = p_table->bucket[i];
		while (p_node) {
			p_prev_node = p_node;
			p_node = p_node->p_next;
			kfree(p_prev_node);
		}
	}
	kfree(p_table->bucket);
	memset(p_table, 0, sizeof(struct hash_table));
}

/************/
#ifdef __VMKERNEL_HASH_EXPAND_ENABLE__
static int hash_expand(hdl_hash_t p_table)
{
	hash_node_t **prev_bucket;
	hash_node_t *p_prev_node, *p_tmp_node;
	unsigned long prev_size, hash_entry, i;
	int rc = 0;


	prev_bucket = p_table->bucket;
	prev_size = p_table->size;

	if ((rc = hash_init(p_table, (prev_size << 1))) != 0);
		return rc;
	for (i = 0; i < prev_size; ++i) {
		p_prev_node = prev_bucket[i];
		// we have several nodes in this hash entry
		while (p_prev_node) {
			p_tmp_node = p_prev_node;
			p_prev_node = p_prev_node->p_next;
			hash_entry = hash(p_table, p_tmp_node->key);
			p_tmp_node->p_next = p_table->bucket[hash_entry];
			p_table->bucket[hash_entry] = p_tmp_node;
			++p_table->entries;
		}
	}

	// free only the previous bucket array, not the nodes
	// because we will use the nodes pointers in new large hash
	kfree(prev_bucket);
	return 0;
}
#endif	/* __VMKERNEL_HASH_EXPAND_ENABLE__ */

/************/
void * hash_lookup(hdl_hash_t p_table, unsigned long key)
{
	hash_node_t *p_node;
	unsigned long hash_entry;

	// if empty
	if (p_table->size == 0)
		return NULL;

	hash_entry = hash(p_table, key);
	for (p_node = p_table->bucket[hash_entry]; p_node;
			p_node = p_node->p_next) {
		if (key == p_node->key)
			break;
	}
	return (p_node ? p_node->p_data : NULL);
}

/************/
int hash_insert(hdl_hash_t p_table, unsigned long key, void *p_data)
{
	void *p_tmp_data;
	hash_node_t *p_node;
	unsigned long hash_entry;
	int rc = 0;

	// if empty
	if (!p_table->size) {
		rc = hash_init(p_table, MINIMUM_HASH_TABLE_SIZE);
		if (rc)
			return rc;

	}

	// if data not valid
	if (!p_data)
		return -EINVAL;

	// maybe already exists
	p_tmp_data = hash_lookup(p_table, key);
	if (p_tmp_data)
		return -EEXIST;

#ifdef __VMKERNEL_HASH_EXPAND_ENABLE__
	// maybe we need to expand
	while (p_table->entries >= (p_table->size >> EXPAND_HASH_TABLE_LIMIT_ORDER)) {
		if ((rc = hash_expand(p_table)) != 0)
			return rc;
	}
#endif	/* __VMKERNEL_HASH_EXPAND_ENABLE__ */

	// allocate memory for new entry
	p_node = (hash_node_t *)kmalloc(sizeof(hash_node_t), GFP_KERNEL);
	if (!p_node)
		return -ENOMEM;

	// insert the new entry
	p_node->p_data = p_data;
	p_node->key = key;
	hash_entry = hash(p_table, key);
	p_node->p_next = p_table->bucket[hash_entry];
	p_table->bucket[hash_entry] = p_node;
	++p_table->entries;
	return 0;
}

/************/
void * hash_delete(hdl_hash_t p_table, unsigned long key)
{
	hash_node_t *p_node, *p_prev_node;
	void *p_data;
	unsigned long hash_entry;

	// if empty
	if (!p_table->size) {
		return NULL;
	}

	// find what to remove
	hash_entry = hash(p_table, key);
	for (p_node = p_table->bucket[hash_entry]; p_node;
			p_node = p_node->p_next) {
		if (key == p_node->key)
			break;
	}
	if (!p_node)
		return NULL;	// nothing to remove

	// remove the selected node
	if (p_node == p_table->bucket[hash_entry]) {
		p_table->bucket[hash_entry] = p_node->p_next;
		goto free_node;
	}
	for (p_prev_node = p_table->bucket[hash_entry];
			p_prev_node && p_prev_node->p_next;
			p_prev_node = p_prev_node->p_next) {
		if (p_prev_node->p_next == p_node)
			break;
	}
	p_prev_node->p_next = p_node->p_next;

free_node:
	 p_data = p_node->p_data;
	 kfree(p_node);

	 /*
	  * TODO: maybe we will want to implement shrink of the hash
	  */
	 // if empty
	 if (--p_table->entries == 0)
		 hash_destroy(p_table);

	 return p_data;
}
