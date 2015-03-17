/* cnic_register.c: QLogic CNIC Registration Agent
 *
 * Copyright (c) 2010-2014 QLogic Corporation
 *
 * Portions Copyright (c) VMware, Inc. 2010-2013, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: 
 */

#include <linux/version.h>
#if (LINUX_VERSION_CODE < 0x020612)
#include <linux/config.h>
#endif

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include "cnic_register.h"
#include "vmkapi.h"

#define DRV_MODULE_NAME         "cnic_register"
#define PFX DRV_MODULE_NAME ": "

static char version[] __devinitdata =
        "QLogic NetXtreme II CNIC Registration Agent " DRV_MODULE_NAME " v" CNIC_REGISTER_MODULE_VERSION " (" CNIC_REGISTER_MODULE_RELDATE ")\n";

MODULE_DESCRIPTION("QLogic NetXtreme II CNIC Registration Agent");
MODULE_LICENSE("GPL");
MODULE_VERSION(CNIC_REGISTER_MODULE_VERSION);

#define MAX_ADAPTER_NAME 128
#define TABLE_SIZE 16

struct cnic_registration {
   char name[MAX_ADAPTER_NAME];
   vmk_ModuleID modID;
   int refcnt_increased;
   void *callback;
};

/* static module data */
static struct mutex cnic_register_mutex;
static struct cnic_registration registration_table[TABLE_SIZE];


static int
__init cnic_register_init(void)
{
        int rc = 0;
        int i;

        mutex_init(&cnic_register_mutex);

        memset(registration_table, 0, sizeof(registration_table));
        for (i = 0; i < TABLE_SIZE; ++i) {
		registration_table[i].modID = VMK_INVALID_MODULE_ID;
        }

        printk(KERN_INFO "%s", version);

        return rc;
}


int
cnic_register_adapter(const char * name, void *callback)
{
        struct cnic_registration *entry, *new = NULL;
        int rc = 0;
        int i;

        if (name == NULL || name[0] == '\0' || callback == NULL) {
                printk(KERN_ERR PFX "cnic_register_adapter called with NULL parameters\n");
                return -EINVAL;
        }

        mutex_lock(&cnic_register_mutex);

        /* search table for duplicate entry, first empty slot */
        for (i = 0; i < TABLE_SIZE; ++i) {
                entry = &registration_table[i];
                if (entry->callback == NULL) {
                        if (new == NULL)
                                new = entry;
                } else if (strncmp(name, entry->name, MAX_ADAPTER_NAME) == 0) {
                         rc = -EEXIST;
                         goto done;
                }
        }

        /* table full ? */
        if (new == NULL) {
                rc = -E2BIG;
                goto done;
        }

        /* record entry */
        new->callback = callback;
        new->modID = vmk_ModuleStackTop();
        snprintf(new->name, MAX_ADAPTER_NAME, "%s", name);

done:
        mutex_unlock(&cnic_register_mutex);

        return rc;
}
EXPORT_SYMBOL(cnic_register_adapter);


void *
cnic_register_get_callback(const char * name)
{
        struct cnic_registration *entry;
        void *ret = NULL;
        int i;

        if (name == NULL || name[0] == '\0') {
                printk(KERN_ERR PFX "cnic_register_get_callback called with NULL parameters\n");
                return NULL;
        }

        mutex_lock(&cnic_register_mutex);
        for (i = 0; i < TABLE_SIZE; ++i) {
                entry = &registration_table[i];
                if (strncmp(name, entry->name, MAX_ADAPTER_NAME) == 0) {
                         ret = entry->callback;
			 if (entry->refcnt_increased == 0) {
				BUG_ON(entry->modID == VMK_INVALID_MODULE_ID);
				vmk_ModuleIncUseCount(entry->modID);
				entry->refcnt_increased = 1;
                         }
                         goto done;
                }
        }
done:
        mutex_unlock(&cnic_register_mutex);
        return ret;
}
EXPORT_SYMBOL(cnic_register_get_callback);


int
cnic_register_get_table_size(void)
{
        return TABLE_SIZE;
}
EXPORT_SYMBOL(cnic_register_get_table_size);


int
cnic_register_get_callback_by_index(int index, char * name, void **callback)
{
        struct cnic_registration *entry;
        int rc = 0;

        if (index < 0 || index >= TABLE_SIZE) {
                printk(KERN_ERR PFX "cnic_register_get_callback_by_index called with invalid index\n");
                return -EINVAL;
        }

        entry = &registration_table[index];
        mutex_lock(&cnic_register_mutex);
        if (entry->name[0] != '\0') {
                snprintf(name, MAX_ADAPTER_NAME, "%s", entry->name);
                if (entry->refcnt_increased == 0) {
			BUG_ON(entry->modID == VMK_INVALID_MODULE_ID);
			vmk_ModuleIncUseCount(entry->modID);
			entry->refcnt_increased = 1;
                }
                *callback = entry->callback;
        } else {
                rc = -ENOENT;
        }
        mutex_unlock(&cnic_register_mutex);

        return rc;
}
EXPORT_SYMBOL(cnic_register_get_callback_by_index);


void
cnic_register_cancel(const char * name)
{
        struct cnic_registration *entry;
        int i;

        if (name == NULL || name[0] == '\0') {
                printk(KERN_ERR PFX "cnic_register_cancel called with NULL parameters\n");
                return;
        }

        mutex_lock(&cnic_register_mutex);
        for (i = 0; i < TABLE_SIZE; ++i) {
                entry = &registration_table[i];
                if (strncmp(name, entry->name, MAX_ADAPTER_NAME) == 0) {
			/*
                         * To safely remove callback info provided by bnx2/bnx2x,
                         * cnic must have been unloaded, and refcnt on bnx2/bnx2x
                         * has been released.
                         *
                         * The following BUG_ON assertion strictly limits
                         * registration_table to be lookup'ed by cnic module
                         * only, which is the current expected situation.
                         */
                         BUG_ON(entry->refcnt_increased != 0);
                         entry->callback = NULL;
                         entry->name[0] = '\0';
                         entry->modID = VMK_INVALID_MODULE_ID;
                }
        }
        mutex_unlock(&cnic_register_mutex);
}
EXPORT_SYMBOL(cnic_register_cancel);

void
cnic_register_release_all_callbacks()
{
	struct cnic_registration *entry;
	int i;

	mutex_lock(&cnic_register_mutex);
	for (i = 0; i < TABLE_SIZE; ++i) {
		entry = &registration_table[i];
		if (entry->callback) {
			BUG_ON(entry->refcnt_increased != 1);
			vmk_ModuleDecUseCount(entry->modID);
			entry->refcnt_increased = 0;
		}
	}
	mutex_unlock(&cnic_register_mutex);
}
EXPORT_SYMBOL(cnic_register_release_all_callbacks);

module_init(cnic_register_init);
