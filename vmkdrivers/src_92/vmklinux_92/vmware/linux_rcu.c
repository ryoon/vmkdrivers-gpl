/* ****************************************************************
 * Portions Copyright 1998, 2010, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************
 *
 * Adapted from Linux rcupdate.c for vmklinux.
 */

/*
 * Read-Copy Update mechanism for mutual exclusion
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2001
 *
 * Authors: Dipankar Sarma <dipankar@in.ibm.com>
 *	    Manfred Spraul <manfred@colorfullife.com>
 * 
 * Based on the original work by Paul McKenney <paulmck@us.ibm.com>
 * and inputs from Rusty Russell, Andrea Arcangeli and Andi Kleen.
 * Papers:
 * http://www.rdrop.com/users/paulmck/paper/rclockpdcsproof.pdf
 * http://lse.sourceforge.net/locking/rclock_OLS.2001.05.01c.sc.pdf (OLS2001)
 *
 * For detailed explanation of Read-Copy Update mechanism see -
 * 		http://lse.sourceforge.net/locking/rcupdate.html
 *
 */

#include <linux/module.h>
#include "vmklinux_dist.h"
#include "linux_task.h"
#include "vmklinux_log.h"
#include "interrupt.h"
#include "spinlock.h"
#include "rcupdate.h"

#define VMKLNX_RCU_DELAY        1000     // delay for RCU retry

static void vmklnx_rcu_callback(unsigned long data);
static void vmklnx_rcu_delay_timer(unsigned long data);

/**
 * call_rcu - Queue an RCU callback for invocation after a grace period.
 * @head: structure to be used for queueing the RCU updates.
 * @func: actual update function to be invoked after the grace period
 *
 * The update function will be invoked some time after a full grace
 * period elapses, in other words after all currently executing RCU
 * read-side critical sections have completed.  RCU read-side critical
 * sections are delimited by rcu_read_lock() and rcu_read_unlock(),
 * and may be nested.
 */
void vmklnx_call_rcu(struct vmklnx_rcu_data *rdp, struct rcu_head *head,
                     void (*func)(struct rcu_head *rcu))
{
        unsigned long flags;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        head->func = func;
        head->next = NULL;
        spin_lock_irqsave(&rdp->lock, flags);
        head->generation = atomic64_read(&rdp->generation);
        *rdp->nxttail = head;
        rdp->nxttail = &head->next;
        ++rdp->qLen;
        if (atomic64_read(&rdp->nestingLevel) == 0) {
           atomic64_inc(&rdp->generation);
           spin_unlock_irqrestore(&rdp->lock, flags);
           tasklet_schedule(rdp->callback);
        } else {
           // must wait for the quiescent state
           spin_unlock_irqrestore(&rdp->lock, flags);
           mod_timer(rdp->delayTimer, jiffies + VMKLNX_RCU_DELAY);
        }
}
EXPORT_SYMBOL(vmklnx_call_rcu);

/*
 * Return the number of RCU batches processed thus far.  Useful
 * for debug and statistics.
 */
long vmklnx_rcu_batches_completed(struct vmklnx_rcu_data *rdp)
{
        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        return rdp->completed;
}
EXPORT_SYMBOL(vmklnx_rcu_batches_completed);

static void
vmklnx_rcu_delay_timer(unsigned long data)
{
        struct vmklnx_rcu_data *rdp = (struct vmklnx_rcu_data *) data;
        unsigned long flags;

        spin_lock_irqsave(&rdp->lock, flags);
        if (atomic64_read(&rdp->nestingLevel) == 0) {
           atomic64_inc(&rdp->generation);
           spin_unlock_irqrestore(&rdp->lock, flags);
           vmklnx_rcu_callback((unsigned long) rdp);
        } else {
           // need to wait some more for the quiescent state
           spin_unlock_irqrestore(&rdp->lock, flags);
           mod_timer(rdp->delayTimer, jiffies + VMKLNX_RCU_DELAY);
        }
}

static void
vmklnx_rcu_callback(unsigned long data)
{
   struct vmklnx_rcu_data *rdp = (struct vmklnx_rcu_data *) data;
   unsigned long flags;
   struct rcu_head *first;

   for(;;) {
      spin_lock_irqsave(&rdp->lock, flags);
      first = rdp->nxtlist;
      if (first == NULL) {
         VMK_ASSERT(rdp->nxttail == &rdp->nxtlist);
         break;
      }

      /*
       * The list isn't strictly ordered by generation because we
       * didn't have a "lock" on rdp->generation when we sampled it
       * in call_rcu().  However, assuming ordering is a good enough
       * approximation to work for us.
       */
      if (rcu_batch_before(first->generation, atomic64_read(&rdp->generation))) {
         rdp->nxtlist = first->next;
         if (rdp->nxttail == &first->next) {
            rdp->nxttail = &rdp->nxtlist;
         }
         --rdp->qLen;
         spin_unlock_irqrestore(&rdp->lock, flags);

         /*
          * Invoke rcu  callback func.
          *
          * We don't need a VMKAPI_MODULE_CALL wrapper here because RCU
          * cannot cross module boundaries.
          */
         first->func(first);
      } else {
         /*
          * "first" hasn't aged enough yet, so just abandon the list at this
          * time.  We're sure to get another chance later, because even if
          * rcu_read_unlock() is racing with us, it had to increment
          * rdp->generation after we acquired the rdp->lock.  Thus, another
          * tasklet will soon fire.
          */
         break;
      }
   }
   ++rdp->completed;
   spin_unlock_irqrestore(&rdp->lock, flags);
   return;
}

void
vmklnx_rcu_init(struct vmklnx_rcu_data *rdp, struct tasklet_struct *tlet,
                struct timer_list *timer)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   memset(rdp, 0, sizeof(*rdp));
   rdp->nxttail = &rdp->nxtlist;
   spin_lock_init(&rdp->lock);
   rdp->callback = tlet;
   tasklet_init(rdp->callback, vmklnx_rcu_callback, (unsigned long) rdp);
   rdp->delayTimer = timer;
   setup_timer(timer, vmklnx_rcu_delay_timer, (unsigned long) rdp);
}
EXPORT_SYMBOL(vmklnx_rcu_init);

void
vmklnx_rcu_cleanup(struct vmklnx_rcu_data *rdp)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * We're in single threaded operation during module unload.
    * So just push the generation forward, and dispose of all
    * pending callbacks (if there still are any).
    */
   atomic64_inc(&rdp->generation);
   smp_wmb();
   if (rdp->qLen > 0) {
      vmklnx_rcu_callback((unsigned long)rdp);
   }
   tasklet_kill(rdp->callback);
   del_timer_sync(rdp->delayTimer);
}
EXPORT_SYMBOL(vmklnx_rcu_cleanup);

struct rcu_synchronize {
        struct rcu_head head;
        struct completion completion;
};

/* Because of FASTCALL declaration of complete, we use this wrapper */
static void wakeme_after_rcu(struct rcu_head  *head)
{
        struct rcu_synchronize *rcu;

        rcu = container_of(head, struct rcu_synchronize, head);
        complete(&rcu->completion);
}

/**
 * synchronize_rcu - wait until a grace period has elapsed.
 *
 * Control will return to the caller some time after a full grace
 * period has elapsed, in other words after all currently executing RCU
 * read-side critical sections have completed.  RCU read-side critical
 * sections are delimited by rcu_read_lock() and rcu_read_unlock(),
 * and may be nested.
 *
 * If your read-side code is not protected by rcu_read_lock(), do -not-
 * use synchronize_rcu().
 */
void vmklnx_synchronize_rcu(struct vmklnx_rcu_data *rdp)
{
        struct rcu_synchronize rcu;

        VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
        init_completion(&rcu.completion);
        /* Will wake me after RCU finished */
        vmklnx_call_rcu(rdp, &rcu.head, wakeme_after_rcu);


        /* Wait for it */
        wait_for_completion(&rcu.completion);
}
EXPORT_SYMBOL(vmklnx_synchronize_rcu);
