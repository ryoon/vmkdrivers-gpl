/* ****************************************************************
 * Portions Copyright 2007-2010, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_kthread.c
 *
 *      Emulation for Linux kthreads.
 *
 * From linux-2.6.18-8/kernel/kthread.c:
 *
 * Copyright (C) 2004 IBM Corporation, Rusty Russell.
 *
 ******************************************************************/

#include <asm/semaphore.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/delay.h>

#include "linux_stubs.h"
#include "linux_task.h"
#include "vmklinux_log.h"

struct kthread_create_info
{
   int (*threadfn)(void *data);
   void *data;
   vmk_ModuleID module_id;
   struct completion done;
};

struct kthread_stop_info
{
   pid_t k;
   int err;
   struct completion done;
};

static struct kthread_stop_info kthread_stop_info;
static struct mutex kthread_stop_lock;

/**                                          
 *  kthread_should_stop - check if the current kernel thread needs to exit
 *                                           
 *  Check if the calling kthread has been notified to stop.
 *  The notification can only be delivered via an invocation of
 *  kthread_stop() by a different thread. 
 *  
 *                                           
 *  RETURN VALUE:
 *  Non-zero if a stop notification has been delivered; otherwise 0.
 *
 *  SEE ALSO:
 *  kthread_create() and kthread_stop()
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: kthread_should_stop */
int 
kthread_should_stop(void) 
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return (kthread_stop_info.k == get_current()->pid);
}
EXPORT_SYMBOL(kthread_should_stop);

static int 
kthread(void *_create) 
{
   struct kthread_create_info *create = _create;
   int (*threadfn)(void *data);
   void *data;
   vmk_ModuleID module_id;
   struct task_struct *task;
   LinuxTaskExt *te;
   VMK_ReturnStatus status;
   vmk_AffinityMask affinity;
   int ret = -EINTR;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT_CPU_HAS_INTS_ENABLED();

   threadfn  = create->threadfn;
   data      = create->data;
   module_id = create->module_id;

   complete(&create->done);

    __set_current_state(TASK_INTERRUPTIBLE);
   schedule();

   task = get_current();

   /*
    * Fix up the func and arg pointer in the extended task struct.
    * LinuxStartFunc() has saved 'kthread' and '_create' as func 
    * and arg. This needs to be corrected.
    */
   te = container_of(task, LinuxTaskExt, task);
   te->func = threadfn;
   te->arg = data;
   te->modID = module_id;

   if (task->cpu != VMKLNX_INVALID_TASK_CPU) {
      affinity = vmk_AffinityMaskCreate(vmklinuxModID);
      if (VMK_INVALID_AFFINITY_MASK == affinity) {
         VMKLNX_WARN("Affinity mask creation failed for "
                     "task->pid %d, task->cpu %d",
                     task->pid, task->cpu);
      } else {
         vmk_AffinityMaskAdd(task->cpu, affinity);

         status = vmk_WorldSetAffinity(task->pid, affinity);
         if (VMK_OK != status) {
            VMKLNX_WARN("Setting world affinity (task->pid %d) failed: %s",
                        task->pid, vmk_StatusToString(status));
         }

         vmk_AffinityMaskDestroy(affinity);

         /*
          * The affinity mask is only guaranteed to take effect upon
          * World wakeup.  Because short sleeps may not necessarily
          * result in the World blocking, attempt the sleep multiple
          * times.
          */
#define VMKLNX_KTHREAD_BIND_ATTEMPTS 2
        
         if ((VMK_OK == status) && (smp_processor_id() != task->cpu)) {
            int attempts;
            for (attempts = 0;
                 ((attempts < VMKLNX_KTHREAD_BIND_ATTEMPTS) &&
                  (smp_processor_id() != task->cpu));
                 attempts++) {
               msleep(100);
            }
         }

         if (smp_processor_id() != task->cpu) {
            VMKLNX_WARN("Failed to awake on CPU %d\n", task->cpu);
         }
      }
   }

   if (!kthread_should_stop()) {
      VMKAPI_MODULE_CALL(module_id,
                         ret,
			 threadfn,
			 data);
   }

   if (kthread_should_stop()) {
      kthread_stop_info.err = ret;
      complete(&kthread_stop_info.done);
   }

   return ret;
}

/*
 * ----------------------------------------------------------------------------- 
 *
 * LinuxKthread_Cleanup --
 *
 *    Called when vmklinux is cleaning up.
 *
 *
 * Results:
 *   void
 *
 * Side effects:
 *   kthread locking primitives are destroyed
 *
 * -----------------------------------------------------------------------------
 */
void 
LinuxKthread_Cleanup(void)
{
   mutex_destroy(&kthread_stop_lock);
}


/*
 * ----------------------------------------------------------------------------- 
 *
 * LinuxKthread_Init --
 *
 *    Initializes kthread data structures
 *
 *
 * Results:
 *    void
 *
 * Side effects:
 *    kthread locking primitives are initialized
 *
 * -----------------------------------------------------------------------------
 */
void LinuxKthread_Init(void) 
{
   mutex_init(&kthread_stop_lock);
}


/*
 * ----------------------------------------------------------------------------- 
 *
 * vmklnx_kthread_create --
 *
 *    Implement kthread_create()
 *
 *
 * Results:
 *    Return the task struct of the newly created kthread on success; 
 *    otherwise -errno as pointer.
 *
 * Side effects:
 *    None.
 *
 * -----------------------------------------------------------------------------
 */

struct task_struct *
vmklnx_kthread_create(vmk_ModuleID module_id,
                        int (*threadfn)(void *),
                        void *data,
			char *threadfn_name)
{
   struct kthread_create_info create;
   LinuxTaskExt *te;
   pid_t pid;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   create.threadfn  = threadfn;
   create.data      = data;
   create.module_id = module_id;

   init_completion(&create.done);

   status = LinuxTask_Create(kthread, 
			     &create, 
			     vmklinuxModID, 
			     LT_KTHREAD, 
			     threadfn_name, 
			     &pid);

   if (status != VMK_OK) {
      return ERR_PTR(-ECHILD);
   }

   wait_for_completion(&create.done); 

   te = LinuxTask_Find(pid);

   return &te->task;
}
EXPORT_SYMBOL(vmklnx_kthread_create);


/**                                          
 *  kthread_stop - deliver a stop notification to a kernel thread 
 *  @k: a pointer to a task_struct 
 *                                           
 *  Deliver a stop notification to the kthread identified by the arugment 
 *  @k. This function will block until the targeted kthread has exited.
 *                                           
 *  RETURN VALUE:
 *  The exit value returned by @k.
 *                                           
 *  SEE ALSO:
 *  kthread_create() and kthread_should_stop()
 */                                          
/* _VMKLNX_CODECHECK_: kthread_stop */
int 
kthread_stop(struct task_struct *k) 
{
   int ret;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!k)
      return -EINVAL;
   
   mutex_lock(&kthread_stop_lock);

   init_completion(&kthread_stop_info.done);
   smp_wmb();

   kthread_stop_info.k = k->pid;

   wake_up_process(k);
   
   wait_for_completion(&kthread_stop_info.done);

   kthread_stop_info.k = 0;
   ret = kthread_stop_info.err;

   mutex_unlock(&kthread_stop_lock);

   return ret;
}
EXPORT_SYMBOL(kthread_stop);

/**
 *  kthread_bind - bind a kthread's execution to a specific physical CPU
 *  @task: task_struct object for the thread being modified
 *  @cpu: physical cpu, 0-indexed, to which the thread will be bound
 *
 *  The given task must be stopped (for example, just returned
 *  from kthread_create()).
 *
 *  SEE ALSO:
 *  kthread_create()
 */
/* _VMKLNX_CODECHECK_: kthread_bind */
void 
kthread_bind(struct task_struct *task, unsigned int cpu)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(!(container_of(task, LinuxTaskExt, task)->flags & LT_HELPER));
   task->cpu = cpu;
}
EXPORT_SYMBOL(kthread_bind);
