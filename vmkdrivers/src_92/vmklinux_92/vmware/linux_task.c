/* ****************************************************************
 * Portions Copyright 1998, 2007-2010, 2012, 2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <asm/semaphore.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "vmkapi.h"
#include "vmklinux_debug.h"
#include "linux_stubs.h"
#include "linux_task.h"
#include "vmklinux_dist.h"

#define VMKLNX_LOG_HANDLE LinTask
#include "vmklinux_log.h"

/*
 * Struct used to pass essential arguments to the
 * startup function of a new task
 */
typedef struct LinuxTaskStartFuncArg {
   int                (*func)(void *);
   void               *arg;
   vmk_ModuleID       modID;
   vmk_HeapID         heapID;
   u32                flags;
} LinuxTaskStartFuncArg;

vmk_SpinlockIRQ               taskLock;
static vmk_WorldStorageHandle taskStorageHandle;


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskGetCurrent --
 *
 *      Get the extended task_struct pointer associated with the
 *      current running world.
 *
 * Results:
 *      Return the extended task_struct of the current world.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static inline LinuxTaskExt *
LinuxTaskGetCurrent(void)
{
   LinuxTaskExt *te;
   VMK_ReturnStatus status;

   status = vmk_WorldStorageLookUpLocal(taskStorageHandle, (void **)&te);
   VMK_ASSERT(status == VMK_OK);

   return te;
}


#if defined(VMX86_DEBUG)
/*
 * Global task list to facilitate scanning all the task structures during debugging.
 */
static LIST_HEAD(taskList);

/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_ValidTask --
 *      Verify if the pointer te is indeed pointing to a valid
 *      extended task descriptor.
 *
 * Results:
 *      TRUE if te is a valid pointer; otherwise FALSE.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

vmk_Bool
LinuxTask_ValidTask(LinuxTaskExt *te)
{
   LinuxTaskExt *taskFound;
   vmk_Bool ret;
   extern LinuxTaskExt * LinuxTask_Find(pid_t pid);

   taskFound = LinuxTask_Find(te->task.pid);
   if (taskFound == NULL) {
      return VMK_FALSE;
   }

   ret = (taskFound == te) ? VMK_TRUE : VMK_FALSE;

   LinuxTask_Release(taskFound);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_IsLocked --
 *      Verify that the current world has exclusive access to the
 *      specified extended task struct.
 *
 * Results:
 *      TRUE if the current world has exclusive access; otherwise
 *      FALSE;
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_AssertLocked(LinuxTaskExt *te)
{
   vmk_SPAssertIsLockedIRQ(&taskLock);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskInitMutexRecordList --
 *      Initialize the mutex record list that associates with the specified
 *      task.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Add the LinuxTaskExt to the global "taskList"
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTaskInitMutexRecordList(LinuxTaskExt *te)
{
   int i;
   unsigned long prevIRQL;

   INIT_LIST_HEAD(&te->freeMutexRecords);
   INIT_LIST_HEAD(&te->mutexesAcquired);

   /*
    * Connect together all the lockRecord elements into a free list.
    */
   for(i=0; i<TASK_MAX_MUTEX_RECORDS; i++) {
      list_add(&te->mutexRecords[i].list, &te->freeMutexRecords);
   }

   prevIRQL = LinuxTask_Lock(te);

   list_add(&te->list, &taskList);

   LinuxTask_Unlock(te, prevIRQL);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskCleanupMutexRecordList --
 *      Clean up the mutex record list that associates with the specified
 *      task.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Remove the LinuxTaskExt off the global "taskList"
 *
 *----------------------------------------------------------------------
 */

static inline void
LinuxTaskCleanupMutexRecordList(LinuxTaskExt *te)
{
   struct list_head *lr;
   unsigned long prevIRQL;

   list_for_each(lr, &te->mutexesAcquired) {
      struct MutexRecord *r = list_entry(lr, struct MutexRecord, list);
      VMKLNX_WARN("task %d exited while holding mutex %p.",
                  te->task.pid,
                  r->mutex);
      VMK_ASSERT(0);
   }

   prevIRQL = LinuxTask_Lock(te);

   list_del(&te->list);

   LinuxTask_Unlock(te, prevIRQL);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_AddMutexRecord --
 *      Record the mutex the current task has acquired.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_AddMutexRecord(struct mutex *m)
{
   struct list_head *lr;
   struct MutexRecord *r;
   LinuxTaskExt *te = LinuxTaskGetCurrent();

   if (list_empty(&te->freeMutexRecords)) {
      VMKLNX_WARN("Too many nested mutexes");
      VMK_ASSERT(0);
   }

   lr = (struct list_head *) te->freeMutexRecords.next;
   list_del(lr);
   r = list_entry(lr, struct MutexRecord, list);
   r->mutex = m;
   list_add(lr, &te->mutexesAcquired);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_DeleteMutexRecord --
 *      Delete the record of the mutex the current task has just released.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_DeleteMutexRecord(struct mutex *m)
{
   struct list_head *lr;
   struct MutexRecord *r;
   LinuxTaskExt *te = LinuxTaskGetCurrent();


   list_for_each(lr, &te->mutexesAcquired) {
      if (list_entry(lr, struct MutexRecord, list)->mutex == m) {
         break;
      }
   }

   if (lr == &te->mutexesAcquired) {
      VMKLNX_WARN("Task %d does not own mutex %p", te->task.pid, m);
      VMK_ASSERT(0);
   }

   list_del(lr);
   r = list_entry(lr, struct MutexRecord, list);
   r->mutex = NULL;
   list_add(lr, &te->freeMutexRecords);
}
EXPORT_SYMBOL(vmklnx_delete_lock_record);
#endif /* defined(VMX86_DEBUG) */


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskConstruct --
 *      Constructor used by vmk_WorldStorageCreate() to
 *      initialize the per workd LinuxTaskExt struct.
 *
 * Results:
 *      Always return VMK_OK.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxTaskConstruct(vmk_WorldID wid,
                   void *object,
                   vmk_ByteCountSmall size,
                   vmk_AddrCookie arg)
{
   LinuxTaskExt *te = object;

   VMK_ASSERT(size == sizeof(LinuxTaskExt));

   memset(te, 0, size);

   VMKLNX_DEBUG_ONLY( LinuxTaskInitMutexRecordList(te) );

   te->task.pid   = (pid_t) wid;
   te->task.state = TASK_RUNNING;
   te->task.cpu   = VMKLNX_INVALID_TASK_CPU;

   VMKLNX_DEBUG(1, "te=%p (pid=%d)", te, te->task.pid);

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTaskDestruct --
 *      Destructor used by vmk_WorldStorageCreate() to
 *      clean up and release resources binded with
 *      LinuxTaskExt struct when the associating world is
 *      being destroyed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
LinuxTaskDestruct(vmk_WorldID wid,
                   void *object,
                   vmk_ByteCountSmall size,
                   vmk_AddrCookie arg)
{
   LinuxTaskExt       *te = object;
   struct task_struct *tp = &te->task;

   VMK_ASSERT(size == sizeof(LinuxTaskExt));

   VMKLNX_DEBUG(1, "te=%p (pid=%d)", te, te->task.pid);

   VMKLNX_DEBUG_ONLY( LinuxTaskCleanupMutexRecordList(te) );

   if (tp->umem) {
      kfree(tp->umem);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * vmklnx_GetCurrent --
 *
 *      Gets current task pointer for the current world.
 *
 * Results:
 *      task struct of current world.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

struct task_struct *
vmklnx_GetCurrent(void)
{
   LinuxTaskExt *te;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   te = LinuxTaskGetCurrent();
   return &te->task;;
}
EXPORT_SYMBOL(vmklnx_GetCurrent);


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_WaitEvent --
 *      Block the current world until the event identified by eventID
 *      is delivered to the world.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_WaitEvent(u32 eventID)
{
   LinuxTaskExt *te = LinuxTaskGetCurrent();
   struct task_struct *task = &te->task;
   unsigned long prevIRQL;

   prevIRQL = LinuxTask_Lock(te);

   VMKLNX_DEBUG(1, "eventID=%d task=%p te=%p te->events=%d",
                 eventID, task, te, te->events);

   while ((te->events & eventID) == 0) {
      te->flags |= LT_SUSPENDED;

      VMKLNX_DEBUG(1, "task=%p (te=%p) suspending...", task, te);

      vmk_WorldWaitIRQLegacy((vmk_WorldEventID)te, &taskLock, 0, prevIRQL);

      VMKLNX_DEBUG(1, "task=%p (te=%p) resumed.", task, te);

      prevIRQL = LinuxTask_Lock(te);

      te->flags &= ~LT_SUSPENDED;

      if (te->events & eventID) {
         break;
      }

      VMKLNX_DEBUG(1, "Spurious wakeup: "
                       "pid=%d te->events=0x%x eventID=0x%x .",
                       task->pid, te->events, eventID);
   }

   te->events &= ~eventID;
   task->state = TASK_RUNNING;
   LinuxTask_Unlock(te, prevIRQL);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_SendEvent --
 *      Delivered the event identified by eventID to the world identified
 *      the task descriptor te. If world te is suspended, it will be
 *      resumed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_SendEvent(LinuxTaskExt *te, u32 eventID)
{
   unsigned long prevIRQL;

   VMKLNX_DEBUG(1, "eventID=%d task=%p te=%p te->events=%d",
                 eventID, &te->task, te, te->events);

   prevIRQL = LinuxTask_Lock(te);

   te->events |= eventID;

   vmk_WorldWakeup((vmk_WorldEventID)te);

   LinuxTask_Unlock(te, prevIRQL);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Exit --
 *      Terminate the current world, removing the descriptor from the
 *      world private data area (which will free it via the destructor).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
LinuxTask_Exit(LinuxTaskExt *te)
{
   VMKLNX_DEBUG(1, "Task te=%p (pid=%d) func=%p exited. Return value=%d",
                te, te->task.pid, te->func, te->retval);

   /*
    * If this is a kthread, we need to call LinuxTask_Release()
    * to reduce a reference to 'te' that was created by
    * kthread_create().
    */
   if (te->flags & LT_KTHREAD) {
      LinuxTask_Release(te);
   }
   vmk_WorldExit(VMK_OK);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Find --
 *      Get the extended task struct associated to the world whose
 *      process ID is pid.
 *
 * Results:
 *      Return the extended task struct of the world.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

LinuxTaskExt *
LinuxTask_Find(pid_t pid)
{
   LinuxTaskExt *taskFound = NULL;
   VMK_ReturnStatus status;

   status = vmk_WorldStorageLookUp((vmk_WorldID) pid,
                                   taskStorageHandle,
                                   (void **)&taskFound);
   if (status == VMK_OK) {
      return taskFound;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxStartFunc --
 *      This function starts up a vmklinux task. It sets up the
 *      runtime for the task and release the task resources on exit.
 *      data is the LinuxTaskStartFuncArg which provides the essential
 *      arguments to start up the new task.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
LinuxStartFunc(void *data)
{
   LinuxTaskStartFuncArg *arg = data;
   LinuxTaskExt          *te  = LinuxTaskGetCurrent();

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT_CPU_HAS_INTS_ENABLED();

   te->func       = arg->func;
   te->arg        = arg->arg;
   te->modID      = arg->modID;
   te->flags      = arg->flags;
   te->task.state = TASK_RUNNING;

   VMKLNX_DEBUG(1, "task=%p te=%p flags=%d func=%p arg=%p",
                 &te->task, te, te->flags, te->func, te->arg);

   vmk_HeapFree(arg->heapID, arg);

   if (te->modID != vmklinuxModID) {
      VMKAPI_MODULE_CALL(te->modID, te->retval, te->func, te->arg);
   } else {
      te->retval = te->func(te->arg);
   }

   VMKLNX_DEBUG(1, "Task func=%p exited.", te->func);

   LinuxTask_Exit(te);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxTask_Create --
 *      Create a vmklinux world to execute function fn, passing arg as
 *      argument to fn. fn is executed in the context of the module
 *      specified by module_id.
 *
 * Results:
 *      VMK_OK if success.
 *
 * Side effects:
 *      Return the world ID via the output argument pid.
 *
 *----------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxTask_Create(int (*fn)(void *),
                 void * arg,
                 vmk_ModuleID module_id,
                 u32 flags,
                 char *fn_name,
                 pid_t *pid)
{
   vmk_ByteCount         byte_left = TASK_NAME_LENGTH;
   VMK_ReturnStatus      status;
   vmk_WorldID           worldID;
   vmk_HeapID            heapID;
   char                  name[TASK_NAME_LENGTH];
   vmk_ByteCount         module_name_len;
   LinuxTaskStartFuncArg *start_func_arg;
   vmk_WorldProps        worldProps;

   /*
    * Create task name as "module:fn_name"
    */
   vmk_ModuleGetName(module_id, name, TASK_NAME_LENGTH);
   module_name_len = vmk_Strnlen(name, TASK_NAME_LENGTH);
   byte_left -= module_name_len + 1;
   vmk_Snprintf(name + module_name_len, byte_left, ":%s", fn_name);

   VMKLNX_DEBUG(1, "name=%s fn=%p arg=%p module_id=%d",
                name, fn, arg, module_id);

   heapID = vmk_ModuleGetHeapID(module_id);
   VMK_ASSERT(heapID != VMK_INVALID_HEAP_ID);

   start_func_arg = vmk_HeapAlloc(heapID, sizeof(*start_func_arg));
   if (start_func_arg == NULL) {
      status = VMK_NO_MEMORY;
      goto done;
   }

   start_func_arg->func   = fn;
   start_func_arg->arg    = arg;
   start_func_arg->modID  = module_id;
   start_func_arg->heapID = heapID;
   start_func_arg->flags  = flags;

   worldProps.name = name;
   worldProps.moduleID = vmklinuxModID;
   worldProps.startFunction = (vmk_WorldStartFunc)LinuxStartFunc;
   worldProps.data = start_func_arg;
   worldProps.heapID = VMK_MODULE_HEAP_ID;
   worldProps.schedClass = VMK_WORLD_SCHED_CLASS_DEFAULT;
   status = vmk_WorldCreate(&worldProps, &worldID);
   if (status == VMK_OK) {
      *pid = worldID;
   } else {
      vmk_HeapFree(heapID, start_func_arg);
   }

done:
   // XXX worldIDs are not valid in the same range as linux PIDs.
   // Probably okay in the kernel, though.
   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * vmklnx_kernel_thread --
 *      Create a vmklinux world to execute function fn, passing arg as
 *      argument to fn. fn is executed in the context of the module
 *      specified by module_id. The new task is named fn_name.
 *
 * Results:
 *      The world ID of the new world on success; otherwise -ECHILD.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
vmklnx_kernel_thread(vmk_ModuleID module_id,
                     int (*fn)(void *),
                     void * arg,
                     char *fn_name)
{
   VMK_ReturnStatus status;
   pid_t            pid;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   status = LinuxTask_Create(fn,
                             arg,
                             module_id,
                             LT_KERNEL_THREAD,
                             fn_name,
                             &pid);
   if (status != VMK_OK) {
      return -ECHILD;
   }

   return pid;
}
EXPORT_SYMBOL(vmklnx_kernel_thread);


/**
 *  wake_up_process - wake up a given task
 *  @task: a given task to be resumed
 *
 *  Wake up and resume a task.
 *
 *  RETURN VALUE:
 *  Always return 1.
 */
/* _VMKLNX_CODECHECK_: wake_up_process */
fastcall
int wake_up_process(struct task_struct *task)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(task && task->pid);

   LinuxTaskExt *te = container_of(task, LinuxTaskExt, task);

   LinuxTask_Resume(te);
   return 1;
}
EXPORT_SYMBOL(wake_up_process);

/**
 *  schedule - select a new process to be executed
 *
 *  Selects a new process to be executed
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: schedule */
void
schedule(void)
{
   struct task_struct *task = get_current();

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (unlikely((signal_pending(task)
       && (task->state == TASK_INTERRUPTIBLE
           || task->state == TASK_RUNNING)))) {
      return;
   }

   if (unlikely(task->state == TASK_RUNNING)) {
      yield();
      return;
   }

   vmk_WorldAssertIsSafeToBlock();
   LinuxTask_Suspend();
}
EXPORT_SYMBOL(schedule);

static void
__wait_timeout(unsigned long data)
{
   struct task_struct *task = (struct task_struct *) data;

   VMK_ASSERT(task && task->pid);

   LinuxTaskExt *te = container_of(task, LinuxTaskExt, task);

   LinuxTask_Resume(te);
}

static signed long
__schedule_timeout(struct task_struct *current_task, signed long wait_time)
{
   unsigned long timeout = wait_time;
   signed long diff;
   long state;
   struct timer_list wait_timer;
   int ret;

   if (unlikely(wait_time == MAX_SCHEDULE_TIMEOUT)) {
      schedule();
      return wait_time;
   }

   if (unlikely(wait_time < 0)) {
      VMKLNX_DEBUG(1, "negative timeout value %lx from %p",
                     wait_time, __builtin_return_address(0));
      return 0;
   }

   timeout = wait_time + jiffies;
   state = current_task->state;

   setup_timer(&wait_timer, __wait_timeout, (unsigned long) current_task);

   /*
    * We might need to set a timer ***twice*** because jiffies may not have
    * advanced fully for the first timer.  That's because jiffies is
    * maintained on CPU 0, whereas the local CPU clock may not be fully
    * synchronized to it.
    *
    * See PR 330905.
    */
   for (;;) {
      __mod_timer(&wait_timer, timeout);
      schedule();
      ret = del_timer_sync(&wait_timer);
      diff = timeout - jiffies;
      if (likely(diff <= 0 || ret)) {
         break;
      }
      // set the state back in preparation for blocking again
      current_task->state = state;
   }

   return diff < 0 ? 0 : diff;
}

/**
 *  schedule_timeout - sleep until timeout
 *  @wait_time: timeout value in jiffies
 *
 *  Make the current task sleep until specified number of jiffies have elapsed.
 *  The routine may return early if a signal is delivered to the current task.
 *  Specifying a timeout value of MAX_SCHEDULE_TIMEOUT will schedule
 *  the CPU away without a bound on the timeout.
 *
 *  RETURN VALUE:
 *  0 if the timer expired in time,
 *  remaining time in jiffies if function returned early, or
 *  MAX_SCHEDULE_TIMEOUT if timeout value of MAX_SCHEDULE_TIMEOUT was specified
 */
/* _VMKLNX_CODECHECK_: schedule_timeout */
fastcall signed long
schedule_timeout(signed long wait_time)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return __schedule_timeout(get_current(), wait_time);
}
EXPORT_SYMBOL(schedule_timeout);

/**
 *  schedule_timeout_interruptible - sleep until timeout or event
 *  @wait_time: the timeout in jiffies
 *
 *  Make the current task sleep until @wait_time jiffies have elapsed or if a
 *  signal is delivered to the current task.
 *
 *  RETURN VALUE:
 *  Returns the remaining time in jiffies.
 *
 */
/* _VMKLNX_CODECHECK_: schedule_timeout_interruptible */
signed long
schedule_timeout_interruptible(signed long wait_time)
{
   struct task_struct *current_task = get_current();

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   current_task->state = TASK_INTERRUPTIBLE;
   return __schedule_timeout(current_task, wait_time);
}
EXPORT_SYMBOL(schedule_timeout_interruptible);

/**
 *  schedule_timeout_uninterruptible - sleep until at least specified timeout
 *                                     has elapsed
 *  @wait_time: timeout value in jiffies
 *
 *  Make the current task sleep until at least specified number of jiffies
 *  have elapsed. Specifying a timeout value of MAX_SCHEDULE_TIMEOUT will
 *  schedule the CPU away without a bound on the timeout.
 *
 *  RETURN VALUE:
 *  0 or MAX_SCHEDULE_TIMEOUT if timeout value of MAX_SCHEDULE_TIMEOUT
 *  was specified
 */
/* _VMKLNX_CODECHECK_: schedule_timeout_uninterruptible */
signed long
schedule_timeout_uninterruptible(signed long wait_time)
{
   struct task_struct *current_task = get_current();

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   current_task->state = TASK_UNINTERRUPTIBLE;
   return __schedule_timeout(current_task, wait_time);
}
EXPORT_SYMBOL(schedule_timeout_uninterruptible);

/**
 *  cond_resched - latency reduction via explicit rescheduling
 *
 *  Performs 'possible' rescheduling of this task by invoking builtin
 *  throttling yield mechanism. The task may or may not yield the CPU
 *  depending on the decision.
 *
 *  RETURN VALUE:
 *  0
 *
 *  ESX DEVIATION NOTES: Always returns 0, unlike Linux where the
 *  return value indicates whether a reschedule was done in fact.
 */
/* _VMKLNX_CODECHECK_: cond_resched */
int
cond_resched(void)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Let vmk_WorldYield builtin throttling yield mechanism to
    * decide whether this world should yield the CPU.
    */
   vmk_WorldYield();

   /*
    * Always assume we have not yielded.
    */
   return 0;
}
EXPORT_SYMBOL(cond_resched);

/**
 *  yield - yield the CPU
 *
 *  Yield the CPU to other tasks.
 *
 *  ESX Deviation Notes:
 *  The calling task will be descheduled for at least 1 millisecond.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 */
/* _VMKLNX_CODECHECK_: yield */
void
yield(void)
{
   /*
    * vmk_WorldYield doesn't always yield currently since the yield
    * is throttled. In order to force the caller to block we can call
    * vmk_WorldSleep() but if the delay is too short, the call may
    * return without yielding. So sleep for at least a 1 millisecond
    * to ensure that CPU actually is given up.
    */
   vmk_WorldSleep(1000);
}
EXPORT_SYMBOL(yield);

/**
 *  msleep - Deschedules the current task for a duration
 *  @msecs: time, in milliseconds, that the current task needs to be descheduled
 *
 *  Deschedules the current task for a duration
 *
 *  See Also:
 *  msleep_interruptible
 */
/* _VMKLNX_CODECHECK_: msleep */
void
msleep(unsigned int msecs)
{
   signed long sleep_time = msecs_to_jiffies(msecs) + 1;

   while (sleep_time) {
      sleep_time = schedule_timeout_uninterruptible(sleep_time);
   }
}
EXPORT_SYMBOL(msleep);

/**
 *  msleep_interruptible - deschedules the current task for a duration
 *  @msecs: time, in milliseconds, that the current task needs to be descheduled.
 *
 *  Deschedules the current task for a duration. The function returns when
 *  the task has been descheduled for the whole intended duration,
 *  or returns when the task is interrupted by an external event
 *  while it is descheduled. In this case, the amount of time unslept is
 *  returned.
 *
 *  Return Value:
 *  Amount of time unslept
 *
 *  See Also:
 *  msleep
 *
 */
/* _VMKLNX_CODECHECK_: msleep_interruptible */
unsigned long
msleep_interruptible(unsigned int msecs)
{
   signed long sleep_time = msecs_to_jiffies(msecs) + 1;
   struct task_struct *current_task = get_current();

   while(sleep_time && !signal_pending(current_task)) {
      current_task->state = TASK_INTERRUPTIBLE;
      sleep_time = __schedule_timeout(current_task, sleep_time);
   }

   return jiffies_to_msecs(sleep_time);
}
EXPORT_SYMBOL(msleep_interruptible);


/*
 *----------------------------------------------------------------------------
 *
 * LinuxTask(Init|Cleanup)Tasks --
 *
 *      Set up the world storage property for the
 *      memory allocation of the per world extended
 *      task struct.
 *
 * Results:
 *      Init: VMK_OK on success.
 *
 * Side effects:
 *      Will vmk_Panic() on failure.
 *
 *----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxTaskInitTasks(void)
{
   VMK_ReturnStatus status = VMK_OK;
   vmk_WorldStorageProps props;

   props.type       = VMK_WORLD_STORAGE_TYPE_SIMPLE;
   props.moduleID   = vmklinuxModID;
   props.size       = sizeof(LinuxTaskExt);
   props.align      = sizeof(long);
   props.constructor= LinuxTaskConstruct;
   props.destructor = LinuxTaskDestruct;

   status = vmk_WorldStorageCreate(&props, &taskStorageHandle);
   if (status != VMK_OK) {
      VMKLNX_WARN("Failed to create world storage (%s)", vmk_StatusToString(status));
      VMK_ASSERT(0);
   }

   return status;
}

void
LinuxTaskCleanupTasks(void)
{
   VMK_ReturnStatus status;

   status = vmk_WorldStorageDestroy(taskStorageHandle);
   VMK_ASSERT(status == VMK_OK);
}


/*
 *----------------------------------------------------------------------------
 *
 * LinuxTask_(Init|Cleanup) --
 *
 *      Initialization and cleanup of LinuxTask.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
LinuxTask_Init(void)
{
   VMK_ReturnStatus status;

   VMKLNX_CREATE_LOG();

   status = vmk_SPCreateIRQ_LEGACY(&taskLock, vmklinuxModID,
                            "taskLock", NULL, VMK_SP_RANK_IRQ_BLOCK_LEGACY);
   VMK_ASSERT(status == VMK_OK);

   status = LinuxTaskInitTasks();
   VMK_ASSERT(status == VMK_OK);
}

void
LinuxTask_Cleanup(void)
{
   LinuxTaskCleanupTasks();
   vmk_SPDestroyIRQ(&taskLock);
   VMKLNX_DESTROY_LOG();
}
