/* ****************************************************************
 * Portions Copyright 2008 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_hashtab.c
 *
 * From linux-2.6.24-7/drivers/char/drm/drm_hashtab.c:
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND. USA.
 * All Rights Reserved.
 *
 ******************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "linux_hashtab.h"

int
vmklnx_hashtab_create(struct vmklnx_hashtab *ht,
                      unsigned int order)
{
   unsigned int i;

   ht->size = 1 << order;
   ht->count = 0;
   ht->order = order;
   ht->table = kmalloc(ht->size * sizeof(*ht->table), GFP_ATOMIC);
   
   if (!ht->table) {
      printk("out of memory for hash table\n");
      return -ENOMEM;
   }
   for (i = 0; i< ht->size; ++i) {
      INIT_HLIST_HEAD(&ht->table[i]);
   }
   return 0;
}

void
vmklnx_hashtab_dump(struct vmklnx_hashtab *ht,
                    unsigned long key)
{
   struct vmklnx_hashtab_item *entry;
   struct hlist_head *h_list;
   struct hlist_node *list;
   unsigned long hashed_key;
   int count;
   
   count = 0;
   hashed_key = hash_long(key, ht->order);
   printk("key is 0x%08lx, hashed key is 0x%08lx\n", key, hashed_key);
   h_list = &ht->table[hashed_key];
   hlist_for_each(list, h_list) {
      entry = hlist_entry(list, struct vmklnx_hashtab_item, head);
      printk("count %d, key: 0x%08lx\n", count++, entry->key);
   }
}

static struct hlist_node *
vmklnx_hashtab_find_key(struct vmklnx_hashtab *ht, 
                        unsigned long key)
{
   struct vmklnx_hashtab_item *entry;
   struct hlist_head *h_list;
   struct hlist_node *list;
   unsigned int hashed_key;
   
   hashed_key = hash_long(key, ht->order);
   h_list = &ht->table[hashed_key];
   hlist_for_each(list, h_list) {
      entry = hlist_entry(list, struct vmklnx_hashtab_item, head);
      if (entry->key == key)
         return list;
      if (entry->key > key)
         break;
   }
   return NULL;
}

int
vmklnx_hashtab_find_item(struct vmklnx_hashtab *ht, 
                         unsigned long key,
                         struct vmklnx_hashtab_item **item)
{
   struct hlist_node *list;
   
   list = vmklnx_hashtab_find_key(ht, key);
   if (!list)
      return -EINVAL;
   
   *item = hlist_entry(list, struct vmklnx_hashtab_item, head);
   return 0;
}

int
vmklnx_hashtab_insert_item(struct vmklnx_hashtab *ht,
                           struct vmklnx_hashtab_item *item)
{
   struct vmklnx_hashtab_item *entry;
   struct hlist_head *h_list;
   struct hlist_node *list, *parent;
   unsigned int hashed_key;
   unsigned long key = item->key;
   
   hashed_key = hash_long(key, ht->order);
   h_list = &ht->table[hashed_key];
   parent = NULL;
   hlist_for_each(list, h_list) {
      entry = hlist_entry(list, struct vmklnx_hashtab_item, head);
      if (entry->key == key)
         return -EINVAL;
      if (entry->key > key)
         break;
      parent = list;
   }
   if (parent) {
      hlist_add_after(parent, &item->head);
   } else {
      hlist_add_head(&item->head, h_list);
   }
   ht->count++;
   return 0;
}

int
vmklnx_hashtab_remove_key(struct vmklnx_hashtab *ht,
                          unsigned long key,
                          struct vmklnx_hashtab_item **item)
{
   struct hlist_node *list;
   
   list = vmklnx_hashtab_find_key(ht, key);
   if (list) {
      hlist_del_init(list);
      ht->count--;
      if (item) {
         *item = hlist_entry(list, struct vmklnx_hashtab_item, head);
      }
      return 0;
   }
   return -EINVAL;
}

int
vmklnx_hashtab_remove_item(struct vmklnx_hashtab *ht,
                           struct vmklnx_hashtab_item *item)
{
   return vmklnx_hashtab_remove_key(ht, item->key, NULL);
}

void
vmklnx_hashtab_remove(struct vmklnx_hashtab *ht)
{
   if (ht->table) {
      kfree(ht->table);
      ht->table = NULL;
      ht->count = 0;
   }
}
