/*
 * This code is copied from kernel/sched.c in Linux source.
 *
 *  Kernel scheduler and related syscalls
 *
 *  Copyright (C) 1991-2002  Linus Torvalds
 *
 *  1996-12-23  Modified by Dave Grothe to fix bugs in semaphores and
 *              make semaphores SMP safe
 *  1998-11-19  Implemented schedule_timeout() and related stuff
 *              by Andrea Arcangeli
 *  2002-01-04  New ultra-scalable O(1) scheduler by Ingo Molnar:
 *              hybrid priority-list and round-robin design with
 *              an array-switch method of distributing timeslices
 *              and per-CPU runqueues.  Cleanups and useful suggestions
 *              by Davide Libenzi, preemptible kernel bits by Robert Love.
 *  2003-09-03  Interactivity tuning by Con Kolivas.
 *  2004-04-02  Scheduler domains code by Nick Piggin
 */

/* ****************************************************************
 * Portions Copyright 2008-2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/
#include <linux/module.h>
#include <linux/wait.h>
#include "linux_task.h"
#include "linux_time.h"
#include "linux_stubs.h"
#include "vmklinux_log.h"

/**                                          
 *  init_waitqueue_head - initialize a waitqueue_head      
 *  @q: waitqueue head to initialize     
 *                                           
 *  Initialize the waitqueue head so it is valid for use.
 *  
 *  RETURN VALUE:
 *       None.                                        
 */                                          
/* _VMKLNX_CODECHECK_: init_waitqueue_head */
void init_waitqueue_head(wait_queue_head_t *q)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   spin_lock_init(&q->lock);
   INIT_LIST_HEAD(&q->task_list);
   q->wakeupType = WAIT_QUEUE_REGULAR;
}
EXPORT_SYMBOL(init_waitqueue_head);

int
default_wake_function(wait_queue_t *curr, unsigned mode, int sync,
                          void *key)
{
   struct task_struct *task;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   task = (struct task_struct *) curr->private;

   if ((task->state & mode) == 0)
      return 1;

   wake_up_process(task);
   return 1;
}
EXPORT_SYMBOL(default_wake_function);

/*
 * The core wakeup function.  Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up.  If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 * 
 * There are circumstances in which we can try to wake a task which has already
 * started to run but is not in state TASK_RUNNING.  try_to_wake_up() returns
 * zero in this (rare) case, and we handle it by continuing to scan the queue.
 */
static void
__wake_up_common(wait_queue_head_t *q, unsigned int mode,
                             int nr_exclusive, int sync, void *key)
{
   struct list_head *tmp, *next;

   list_for_each_safe(tmp, next, &q->task_list) {
      wait_queue_t *curr = list_entry(tmp, wait_queue_t, task_list);
      unsigned flags = curr->flags;

      if (curr->func(curr, mode, sync, key) &&
          (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
         break;
   }
}

/**
 *  __wake_up - wake up threads blocked on a waitqueue.
 *  @q: the waitqueue
 *  @mode: which threads
 *  @nr_exclusive: how many wake-one or wake-many threads to wake up
 *  @key: is directly passed to the wakeup function
 */
/* _VMKLNX_CODECHECK_: __wake_up */
void fastcall
__wake_up(wait_queue_head_t *q, unsigned int mode,
                        int nr_exclusive, void *key)
{
   unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   spin_lock_irqsave(&q->lock, flags);
   if (q->wakeupType == WAIT_QUEUE_POLL) {
      vmk_Timer timer;
      /*
       * __wake_up is called directly by the driver that may have
       * some private locks held. The VMKernel acquires its own
       * locks and may also sleep. Schedule a bh to keep the VMKernel
       * from having to know driver constraints.
       */
      extern void Linux_PollBH(void *data);

      if (vmk_TimerSchedule(vmklnxTimerQueue,
                            Linux_PollBH,
                            (void *)q,
                            0,
                            VMK_TIMER_DEFAULT_TOLERANCE,
                            VMK_TIMER_ATTR_NONE,
                            VMK_LOCKDOMAIN_INVALID,
                            VMK_SPINLOCK_UNRANKED,
                            &timer) != VMK_OK) {
         vmk_Panic("Failed to schedule timer");
      }
   } else {
      __wake_up_common(q, mode, nr_exclusive, 0, key);
   }
   spin_unlock_irqrestore(&q->lock, flags);
}
EXPORT_SYMBOL(__wake_up);

/*
 * Same as __wake_up but called with the spinlock in wait_queue_head_t held.
 */
void fastcall
__wake_up_locked(wait_queue_head_t *q, unsigned int mode)
{
	__wake_up_common(q, mode, 1, 0, NULL);
}

/**                                          
 *  complete - wake up task on a completion waitqueue
 *  @completion: a completion with a waitqueue
 *                                           
 *  This will wake up exactly one waiting process on @completion waitqueue.
 *                                           
 *  RETURN VALUE:
 *  None
 *
 *  SEE ALSO:
 *  complete_and_exit
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: complete */
fastcall void
complete(struct completion* completion)
{
   unsigned long flags;
								
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   spin_lock_irqsave(&completion->wait.lock, flags);
   completion->done++;
   __wake_up_common(&completion->wait, 
                    TASK_UNINTERRUPTIBLE | TASK_INTERRUPTIBLE,
	            1, 
                    0, 
                    NULL);
   spin_unlock_irqrestore(&completion->wait.lock, flags);
}
EXPORT_SYMBOL(complete);

/*
 * 2011/12/01
 * The implementation of the following various wait_for_completion related
 * functions are imported from Linux kernel/sched.c at version 2.6.31.14.
 */
static inline long
do_wait_for_common(struct completion *completion, long timeout, int state)
{
   if (!completion->done) {
      DECLARE_WAITQUEUE(wait, current);

      wait.flags |= WQ_FLAG_EXCLUSIVE;
      __add_wait_queue_tail(&completion->wait, &wait);
      do {
            if ((state & TASK_INTERRUPTIBLE) &&
               signal_pending(current)) {
                  timeout = -ERESTARTSYS;
                  break;
         }
         __set_current_state(state);
         spin_unlock_irq(&completion->wait.lock);
         timeout = schedule_timeout(timeout);
         spin_lock_irq(&completion->wait.lock);
      } while (!completion->done && timeout);
      __remove_wait_queue(&completion->wait, &wait);
      if (!completion->done)
         return timeout;
   }
   completion->done--;

   /*
    * At this point, the completion event is delivered.
    * If timeout happens to be 0, that means the task
    * has been re-scheduled to run at a time exceeds
    * the timeout.  In this case, we want to return 1 
    * to avoid the caller erraneously thinking that 
    * the wait was timed out instead of being woken 
    * up by the completion event.
    */
   return timeout ?: 1;
}

static long
wait_for_common(struct completion *completion, long timeout, int state)
{
        spin_lock_irq(&completion->wait.lock);
        timeout = do_wait_for_common(completion, timeout, state);
        spin_unlock_irq(&completion->wait.lock);
        return timeout;
}

/**                                          
 *  wait_for_completion - sleep until completion is done. 
 *  @completion: a completion with a waitqueue to wait on 
 *                                           
 *  The process is blocked to sleep (TASK_UNINTERRUPTIBLE) until completion is done.  
 *                                           
 *  RETURN VALUE:
 *  This function does not return a value.
 */                                          
/* _VMKLNX_CODECHECK_: wait_for_completion */
void fastcall __sched
wait_for_completion(struct completion *completion)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   wait_for_common(completion, MAX_SCHEDULE_TIMEOUT, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion);

/**                                          
 *  wait_for_completion_timeout - sleep until completion is done or timeout elapses 
 *  @completion: a completion with a waitqueue to wait on 
 *  @timeout: timeout value in jiffies
 *                                           
 *  This process is blocked (TASK_UNINTERRUPTIBLE) until it is completed or timed out.
 *                                           
 *  RETURN VALUE:                                           
 *  This function returns 0 if timeout elapses and returns remaining jiffies if
 *  it completes before the timeout expires. 
 */                                          
/* _VMKLNX_CODECHECK_: wait_for_completion_timeout */
unsigned long fastcall __sched
wait_for_completion_timeout(struct completion *completion, unsigned long timeout)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return wait_for_common(completion, timeout, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_timeout);

/**                                          
 *  wait_for_completion_interruptible_timeout - sleep until completion is done, timeout elapses, or a signal is received.
 *  @completion: a completion with a waitqueue to wait on 
 *  @timeout: timeout in jiffies   
 *                                           
 *  This process is blocked (TASK_INTERRUPTIBLE) until completion is done or a signal is received.                       
 *                                           
 *  RETURN VALUE:                                           
 *  0 if timeout elapses.
 *  -ERESTARTSYS if a signal is received.  
 *  positive number as remaining jiffies if it completes before timeout.
 */                                          
/* _VMKLNX_CODECHECK_: wait_for_completion_interruptible_timeout */
unsigned long fastcall __sched
wait_for_completion_interruptible_timeout(struct completion *completion,
					  unsigned long timeout)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return wait_for_common(completion, timeout, TASK_INTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_interruptible_timeout);

/**                                          
 *  complete_and_exit - wake up the waiting task and exit
 *  @completion: a completion with a waitqueue
 *  @code: code to return on task exit
 *                                           
 *  This will wake up the waiting task on @completion waitqueue and
 *  then exits the calling process.
 *                                           
 *  ESX Deviation Notes:                     
 *  @code is ignored.
 *  
 *  RETURN VALUE:
 *  None
 *
 *  SEE ALSO:
 *  complete
 *                                         
 */                                          
/* _VMKLNX_CODECHECK_: complete_and_exit */
void 
complete_and_exit(struct completion* completion, long code)
{
   struct task_struct *task;
   LinuxTaskExt *te;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (completion) {
      complete(completion);
   }

   task = get_current();
   te = container_of(task, LinuxTaskExt, task);

   if (!LinuxTask_IsVmklinuxTask(te)) {
      VMKLNX_PANIC("complete_and_exit() called by non-vmklinux task.");
   }

   LinuxTask_Exit(te);
}
EXPORT_SYMBOL(complete_and_exit);

