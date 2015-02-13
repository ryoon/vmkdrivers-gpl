/*
 * kref.c - library routines for handling generic reference counted objects
 *
 * Copyright (C) 2004 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2004 IBM Corp.
 *
 * based on lib/kobject.c which was:
 * Copyright (C) 2002-2003 Patrick Mochel <mochel@osdl.org>
 *
 * This file is released under the GPLv2.
 *
 */

#include <linux/kref.h>
#include <linux/module.h>

/**
 * kref_init - initialize object.
 * @kref: pointer to a kref object.
 *
 * Initialize the refcount of @kref.
 */
/* _VMKLNX_CODECHECK_: kref_init */
void kref_init(struct kref *kref)
{
	atomic_set(&kref->refcount,1);
	smp_mb();
}

/**
 * kref_get - increment refcount for object.
 * @kref: pointer to a kref object.
 *
 * Increment the refcount in @kref.
 */
/* _VMKLNX_CODECHECK_: kref_get */
void kref_get(struct kref *kref)
{
	WARN_ON(!atomic_read(&kref->refcount));
	atomic_inc(&kref->refcount);
	smp_mb__after_atomic_inc();
}

/**
 * kref_put - decrement refcount for object.
 * @kref: pointer to a kref object.
 * @release: pointer to the function that will clean up the object when the
 *	     last reference to the object is released.
 *	     This pointer is required, and it is not acceptable to pass kfree
 *	     in as this function.
 *
 * Decrement the refcount in @kref, and if refcount has reached 0, call release().
 *
 * RETURN VALUE:
 * Returns 1 if the object was removed; otherwise returns 0.  
 * Even if the function returns 0, this does not guarantee that the kref 
 * remains in memory. Only use the return value to determine if
 * the kref is gone from memory. Do not rely on this function to 
 * determine if it is present.
 */
/* _VMKLNX_CODECHECK_: kref_put */
int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
	WARN_ON(release == NULL);
/* TODO:dilpreet issues with kfree, hopefully changes to slab.h will fix this */
#ifdef NOT_NEEDED
	WARN_ON(release == (void (*)(struct kref *))kfree);
#endif

	if (atomic_dec_and_test(&kref->refcount)) {
		release(kref);
		return 1;
	}
	return 0;
}

EXPORT_SYMBOL(kref_init);
EXPORT_SYMBOL(kref_get);
EXPORT_SYMBOL(kref_put);
