/* ****************************************************************
 * Portions Copyright 2005, 2010, 2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_task.h --
 *
 *      Linux kernel task compatibility.
 */

#ifndef _LINUX_TASK_H_
#define _LINUX_TASK_H_

#include <sched.h>
#include "vmklinux_debug.h"
#include "vmkapi.h"

/*
 * Bit definitions for the event field in LinuxTaskExt.
 */
#define LTE_RESUME              0x0001       /* Tell task to resume execution */

/* 
 * Bit definitions for the flags field in LinuxTaskExt.
 *
 *   Note: flags must be set in the context of the task, but can be safely
 *         read out of context.
 */
#define LT_KTHREAD              0x01     /* Task created by kthread_create() */
#define LT_KERNEL_THREAD        0x02     /* Task created by kernel_thread()  */
#define LT_SUSPENDED            0x04     /* Task suspended                   */
#define LT_HELPER               0x08     /* Helper world for workqueues      */

#define INVALID_PID VMK_INVALID_WORLD_ID
#define TASK_NAME_LENGTH        64

typedef struct LinuxTaskExt {
   struct task_struct task;
   int                (*func)(void *);
   void               *arg;
   int                (*kthreadFunc)(void *);
   void               *kthreadArg;
   int                retval;
   vmk_ModuleID       modID;
   u32                flags;
   u32                events;
#if defined(VMX86_DEBUG)
   /*
    * If you make any change in this debug area of the struct, make sure
    * vmkdrivers/src_92/vmklinux_92/vmware/linux_task.gdb is also updated
    * in accordance with your change.
    */

   /*
    * No code path should be nesting more than 5 mutexes.
    */
   #define TASK_MAX_MUTEX_RECORDS 5

   /*
    * For efficiency, set aside a fix number of records to track locks
    * instead of allocating the memory dynamically.
    */
   struct MutexRecord {
      struct list_head list;
      void *mutex;
   } mutexRecords[TASK_MAX_MUTEX_RECORDS];

   /*
    * List of unused lock records.
    */
   struct list_head freeMutexRecords;

   /*
    * List of records of locks acquired.
    */
   struct list_head mutexesAcquired;

   /*
    * The list here is to link all the LinuxTaskExt structs together
    * so that it makes coding easier when writing a gdb macro to
    * search for tasks that hold any vmklinux locks. Technically,
    * you can find the LinuxTaskExt by digging into the per world
    * storage area via the World handle, but doing it this way will
    * make the gdb macro more complicated and less future proof since
    * World handle is more likely to change from release to release
    * than LinuxTaskExt.
    */
   struct list_head   list;
#endif /* defined(VMX86_DEBUG) */
} LinuxTaskExt;

extern vmk_SpinlockIRQ taskLock;

extern void LinuxTask_Init(void);
extern void LinuxTask_Cleanup(void);
extern void LinuxTask_WaitEvent(u32 eventID);
extern void LinuxTask_SendEvent(LinuxTaskExt *te, u32 eventID);
extern LinuxTaskExt *LinuxTask_Find(pid_t pid);

extern VMK_ReturnStatus LinuxTask_Create(int (*fn)(void *),
	                                 void *arg,
		                         vmk_ModuleID module_id,
                                         u32 flags,
                                         char *name,
			                 pid_t *pid);


extern void LinuxTask_Exit(LinuxTaskExt *te);
#if defined(VMX86_DEBUG)
extern vmk_Bool LinuxTask_ValidTask(LinuxTaskExt *te);
extern void LinuxTask_AssertLocked(LinuxTaskExt *te);
extern void LinuxTask_AddMutexRecord(struct mutex *m);
extern void LinuxTask_DeleteMutexRecord(struct mutex *m);
#else
#define LinuxTask_ValidTask(te)
#define LinuxTask_AssertLocked(te)
#endif /* defined(VMX86_DEBUG) */


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_IsVmklinuxTask --
 *      Check to see if the task identified by te is a
 *      task created by vmklinux.
 *
 * Results:
 *      1 if current task is created by vmklinux; 0 if not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline int
LinuxTask_IsVmklinuxTask(LinuxTaskExt *te)
{
   return te->flags & (LT_KTHREAD|LT_KERNEL_THREAD);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Suspend --
 *      Suspend the execution of the current running task.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTask_Suspend(void)
{
   LinuxTask_WaitEvent(LTE_RESUME);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskResume --
 *      Resume the execution of the task identified by te.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTask_Resume(LinuxTaskExt *te)
{
   LinuxTask_SendEvent(te, LTE_RESUME);
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Lock --
 *      Acquire exclusive access to the extended task descriptor
 *      identified by te.
 *
 * Results:
 *      Return the interrupt level
 *
 * Side effects:
 *	Disable interrupt.
 *
 *----------------------------------------------------------------------
 */

static inline unsigned long 
LinuxTask_Lock(LinuxTaskExt *te)
{
   /*
    * We may need per LinuxTaskExt lock. But for now, we
    * go with a global lock.
    */
   return vmk_SPLockIRQ(&taskLock);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Unlock --
 *      Open up access to the extended task descriptor identified by
 *      te to other process, and restore the interrupt level to prevIRQL.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTask_Unlock(LinuxTaskExt *te, unsigned long prevIRQL)
{
   vmk_SPUnlockIRQ(&taskLock, prevIRQL);
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Release --
 *      Release a task returned from LinuxTask_Find().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTask_Release(LinuxTaskExt *te)
{
   vmk_WorldStorageRelease(te->task.pid, (void **) &te);
}

#endif /* _LINUX_TASK_H_ */
