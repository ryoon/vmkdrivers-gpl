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


#ifndef _MLNX_HASH_H
#define _MLNX_HASH_H


/************************************************************************/
typedef struct hash_node hash_node_t;

struct hash_table {
	hash_node_t **bucket;		// the hash array
	unsigned long size;		// size of the array
	unsigned long entries;		// number of used entries in hash
	unsigned long mask;		// used to select bits for hashing
};

typedef struct hash_table * hdl_hash_t;


/************************************************************************/
void hash_print(hdl_hash_t p_table);
int hash_init(hdl_hash_t p_table, unsigned long table_size);
void hash_destroy(hdl_hash_t p_table);
void * hash_lookup(hdl_hash_t p_table, unsigned long key);
int hash_insert(hdl_hash_t p_table, unsigned long key, void *p_data);
void * hash_delete(hdl_hash_t p_table, unsigned long key);


#endif	/* _MLNX_HASH_H */
