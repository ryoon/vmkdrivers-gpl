/* ****************************************************************
 * Portions Copyright 2008, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_notifier.c
 *
 * From linux-2.6.18.8/kernel/sys.c:
 *
 * Copyright (C) 1991, 1992  Linus Torvalds
 *
 ******************************************************************/

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/rcupdate.h>
#include <linux/rwsem.h>

static int notifier_chain_register(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	n->modID = vmk_ModuleStackTop();
	rcu_assign_pointer(*nl, n);
	return 0;
}

static int notifier_chain_unregister(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			rcu_assign_pointer(*nl, n->next);
			return 0;
		}
		nl = &((*nl)->next);
	}
	return -ENOENT;
}

static int notifier_call_chain(struct notifier_block **nl,
		unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb, *next_nb;

	nb = rcu_dereference(*nl);
	while (nb) {
		next_nb = rcu_dereference(nb->next);
                VMKAPI_MODULE_CALL(nb->modID, ret, nb->notifier_call, nb, 
                                   val, v);
		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;
		nb = next_nb;
	}
	return ret;
}

/**
 * atomic_notifier_chain_register - Add notifier to an atomic notifier chain
 * @nh: Pointer to head of the atomic notifier chain
 * @n: New entry in notifier chain
 *
 * Adds a notifier to an atomic notifier chain.
 *
 * RETURN VALUE:
 * Currently always returns zero.
 */
/* _VMKLNX_CODECHECK_: atomic_notifier_chain_register */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                struct notifier_block *n)
{
        unsigned long flags;
        int ret;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        spin_lock_irqsave(&nh->lock, flags);
        ret = notifier_chain_register(&nh->head, n);
        spin_unlock_irqrestore(&nh->lock, flags);
        return ret;
}
EXPORT_SYMBOL(atomic_notifier_chain_register);

/**
 * atomic_notifier_chain_unregister - Remove entry from atomic notifier chain.
 * @nh: Pointer to head of the atomic notifier chain
 * @n: Entry to be removed from the notifier chain
 *
 * Removes a previously registered notifier in an atomic notifier chain.
 *
 * RETURN VALUE:
 * Zero on success, non-zero otherwise.
 */
/* _VMKLNX_CODECHECK_: atomic_notifier_chain_unregister */
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
                struct notifier_block *n)
{
        unsigned long flags;
        int ret;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        spin_lock_irqsave(&nh->lock, flags);
        ret = notifier_chain_unregister(&nh->head, n);
        spin_unlock_irqrestore(&nh->lock, flags);
        return ret;
}
EXPORT_SYMBOL(atomic_notifier_chain_unregister);

int __atomic_notifier_call_chain(struct atomic_notifier_head *nh,
                                        unsigned long val, void *v,
                                        int nr_to_call, int *nr_calls)
{
        int ret;

        rcu_read_lock();
        ret = notifier_call_chain(&nh->head, val, v);
        rcu_read_unlock();
        return ret;
}

/**
 * atomic_notifier_call_chain - Call functions in an atomic notifier chain
 * @nh: Pointer to head of the atomic notifier chain
 * @val: Value passed unmodified to notifier function
 * @v: Pointer passed unmodified to notifier function
 *
 * Calls each function in a notifier chain in turn.  The functions
 * run in an atomic context, so they must not block.
 * This routine uses RCU to synchronize with changes to the chain.
 *
 * RETURN VALUE:
 * If the return value of the notifier can be and'ed
 * with %NOTIFY_STOP_MASK then atomic_notifier_call_chain()
 * will return immediately, with the return value of
 * the notifier function which halted execution.
 * Otherwise the return value is the return value
 * of the last notifier function called.
 */
/* _VMKLNX_CODECHECK_: atomic_notifier_call_chain */
int atomic_notifier_call_chain(struct atomic_notifier_head *nh,
                unsigned long val, void *v)
{
        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        return __atomic_notifier_call_chain(nh, val, v, -1, NULL);
}
EXPORT_SYMBOL(atomic_notifier_call_chain);

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	down_write(&nh->rwsem);
	ret = notifier_chain_register(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL(blocking_notifier_chain_register);

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	down_write(&nh->rwsem);
	ret = notifier_chain_unregister(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL(blocking_notifier_chain_unregister);

int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
		unsigned long val, void *v)
{
	int ret;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	down_read(&nh->rwsem);
	ret = notifier_call_chain(&nh->head, val, v);
	up_read(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL(blocking_notifier_call_chain);
