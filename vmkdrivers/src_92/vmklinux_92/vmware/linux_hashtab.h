/* ****************************************************************
 * Portions Copyright 2008 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#ifndef _LINUX_HASHTAB_H_
#define _LINUX_HASHTAB_H_

#include <linux/list.h>

struct vmklnx_hashtab_item {
   struct hlist_node head;
   unsigned long key;
};

struct vmklnx_hashtab {
   unsigned int size;
   unsigned int count;
   unsigned int order;
   struct hlist_head *table;
};

int vmklnx_hashtab_create(struct vmklnx_hashtab *ht,
                          unsigned int order);
void vmklnx_hashtab_dump(struct vmklnx_hashtab *ht,
                         unsigned long key);
int vmklnx_hashtab_find_item(struct vmklnx_hashtab *ht, 
                             unsigned long key,
                             struct vmklnx_hashtab_item **item);
int vmklnx_hashtab_insert_item(struct vmklnx_hashtab *ht,
                               struct vmklnx_hashtab_item *item);
int vmklnx_hashtab_remove_key(struct vmklnx_hashtab *ht,
                              unsigned long key,
                              struct vmklnx_hashtab_item **item);
int vmklnx_hashtab_remove_item(struct vmklnx_hashtab *ht,
                               struct vmklnx_hashtab_item *item);
void vmklnx_hashtab_remove(struct vmklnx_hashtab *ht);

#endif /* _LINUX_HASHTAB_H_ */
