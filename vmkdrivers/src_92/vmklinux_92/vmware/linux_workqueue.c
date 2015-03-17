/********************************************************************************
 * Portions Copyright 2008 - 2012 VMware, Inc.  All rights reserved.
 *******************************************************************************/

/******************************************************************
 *
 *  linux_workqueue.c
 *
 * From linux-2.6.18.8/kernel/workqueue.c:
 *
 * Started by Ingo Molnar, Copyright (C) 2002
 *
 ******************************************************************/

#include <asm/semaphore.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include "vmkapi.h"
#include "linux_task.h"
#include "linux_stubs.h"

#define VMKLNX_LOG_HANDLE LinWQ
#include "vmklinux_log.h"

static struct workqueue_struct *keventd_wq = NULL;

/*
 * This implementation of workqueues layers linux queues on top of
 * vmkapi Helper worlds.  Both singly threaded and multi-threaded queues
 * are supported.  All linux workqueues are multiplexed onto a single
 * set of vmk_Helper worlds (thus limiting the number of worlds that
 * can be created with an absolute cap).
 */

struct workqueue_struct {
   const char *name;
#if defined(__VMKLNX__)
   struct list_head     head;           // queued requests
   struct timer_list    delayTimer;     // for use when vmk_HelperSubmitRequest fails
   int                  qLen;           // size of the list attached to "head"
   int                  curPosted;      // # of requests posted to the vmk_Helper
   int                  curDispatch;    // # of requests executing
   int                  maxDispatch;    // max concurrent requests permitted
   int                  waiting;        // # of worlds waiting for cancel/flush
   vmk_SpinlockIRQ      wqLock;         // mutexes all of the above fields
   struct list_head     trackList;      // tracking for cancel
   struct list_head     trackFlushList; // tracking for cancel and queue flush
   struct list_head     trackFree;      // free tracking structures
   struct mutex         flushMutex;     // for single threading flush
   unsigned long        setExitFlag;
#else
   struct list_head entry;  /* Empty if single thread */
#endif
};

#if defined(__VMKLNX__)
/*
 * The work_tracker list elements are used to track in-progress work
 * for the benefit of cancel_work_sync().
 */
struct work_tracker {
   struct list_head          links;          // list links
   struct work_struct   *executing;     // work_struct in progress
};

static vmk_Helper vmklnx_Helper;            // VMKAPI handle to the helper worlds
static int vmklnx_Helper_worlds;            // the number of helper worlds created

#define VMKLNX_MIN_QHELPERS         16      // min worlds servicing the workqueues
#define VMKLNX_MAX_QHELPERS         32      // max worlds servicing the workqueues
#define VMKLNX_MAX_REQ_PER_WORLD    3       // max requests per world
#define VMKLNX_MAX_QBLOCK_TIME      100     // max block time (ms) before the
                                            // vmk_Helper creates a new world
#define VMKLNX_MAX_QIDLE_TIME  (60*1000)    // max idle time (ms) before a
                                            // vmk_Helper thread exit
#define VMKLNX_CANCEL_WAIT_TIME  (100*1000) // time (us) to wait for pending bit
                                            // to clear in cancel

#endif /* defined(__VMKLNX__) */

static void vmklnx_workqueue_timeout(unsigned long);
static void vmklnx_workqueue_callout(vmk_AddrCookie data);
static inline void __queue_work(struct workqueue_struct *wqPtr,
                                struct work_struct *work);

#if defined(VMX86_DEBUG)

static inline int
__qlen(struct list_head *head)
{
   struct list_head *pos;
   int i = 0;

   list_for_each(pos, head) {
      ++i;
   }

   return i;
}

static inline int
__on_q(struct workqueue_struct *wqPtr, struct work_struct *work)
{
   struct list_head *head = &wqPtr->head;
   struct list_head *pos;

   list_for_each(pos, head) {
      if (pos == &work->entry) {
         return 1;
      }
   }

   return 0;
}

struct {
   atomic64_t   created;
   atomic64_t   destroyed;
   atomic64_t   queued;
   atomic64_t   dispatched;
   atomic64_t   callouts;
   atomic64_t   compat_callouts;
   atomic64_t   flushes;
   atomic64_t   cancel_entered;
   atomic64_t   cancelled;
   atomic64_t   removed;
   atomic64_t   waited;
   atomic64_t   busy_waited;
   atomic64_t   grabbed;
   atomic64_t   executing;
   atomic64_t   timer_cancelled;
} vmklnx_qstats;

#define qstat_incr(field)       atomic64_inc(&vmklnx_qstats.field);
#define qstat_read(field)       atomic64_read(&vmklnx_qstats.field)
#define debug_set(lval, val)    ((lval) = (val))

#else

#define qstat_incr(field)
#define qstat_read(field)
#define debug_set(lval, val)

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxWorkQueue_Init --
 *    initialize workqueue 
 *
 * Results:
 *     0  successful completion;
 *    -1  otherwise
 *
 *-----------------------------------------------------------------------------
 */
int LinuxWorkQueue_Init()
{
   VMK_ReturnStatus status;
   vmk_HelperProps vmklnx_Helper_props = {
      .name = { "vmklnx Helper" },
      .heap = VMK_MODULE_HEAP_ID,
      .useIrqSpinlock = VMK_TRUE,
      .preallocRequests = VMK_FALSE,
      .blockingSubmit = VMK_TRUE,    // XXX: required for
                                     // ASSERT(i < MAX)
                                     // in vmkernel/main/helper.c 
      .mutables = {
            .minWorlds = 1,
            .maxIdleTime = VMKLNX_MAX_QIDLE_TIME,
            .maxRequestBlockTime = VMKLNX_MAX_QBLOCK_TIME,
         },
      .tagCompare = NULL,
      .constructor = NULL,
      .constructorArg = (vmk_AddrCookie) NULL,
   };

   VMKLNX_CREATE_LOG();

   vmklnx_Helper_worlds = vmk_NumPCPUs();
   if (vmklnx_Helper_worlds < VMKLNX_MIN_QHELPERS) {
      vmklnx_Helper_worlds = VMKLNX_MIN_QHELPERS;
   } else if (vmklnx_Helper_worlds > VMKLNX_MAX_QHELPERS) {
      vmklnx_Helper_worlds = VMKLNX_MAX_QHELPERS;
   }
   vmklnx_Helper_props.mutables.maxWorlds = vmklnx_Helper_worlds;
   vmklnx_Helper_props.maxRequests = VMKLNX_MAX_REQ_PER_WORLD * vmklnx_Helper_worlds;

   status = vmk_HelperCreate(&vmklnx_Helper_props, &vmklnx_Helper);
   VMK_ASSERT(status == VMK_OK);

   keventd_wq = create_workqueue("keventd_wq"); 
   if (NULL == keventd_wq) {
      vmk_HelperDestroy(vmklnx_Helper);
      return -1;
   }
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxWorkQueue_Cleanup --
 *
 *    cleanup work queue during unloading 
 *
 * Results
 *    None
 *
 *-----------------------------------------------------------------------------
 */
void LinuxWorkQueue_Cleanup()
{
   destroy_workqueue(keventd_wq);
   vmk_HelperDestroy(vmklnx_Helper);

   VMKLNX_DESTROY_LOG();

   return;
}

/**                                          
 *  __create_workqueue - construct a workqueue struct
 *  @name: a const char * to the given wq name
 *  @singlethread: a flag to indicate singlethread processing or not
 *  
 *  Construct and initialize a workqueue struct
 *
 *  RETURN VALUE:
 *  If successful: pointer to the workqueue struct
 *  NULL otherwise.
 *                                           
 *  ESX Deviation Notes:                     
 *  For ESX, the callback function is not bound to a physical CPU.
 *  For single threaded workqueues, Linux binds all callback functions to
 *  a single CPU.  For multi-threaded workqueues under Linux, each callback
 *  function stays bound to a single CPU once it starts executing.
 */                                          
/* _VMKLNX_CODECHECK_: __create_workqueue */
struct workqueue_struct *
__create_workqueue(const char *name, int singlethread)
{
   VMK_ReturnStatus status;
   struct workqueue_struct *wqPtr; 
   int maxDispatch, i;
   struct work_tracker *wtPtr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT (name);
   if (NULL == name) {
      VMKLNX_WARN("Couldn't create workqueue due to a NULL name");
      return NULL;
   }

   // don't occupy all the helpers (see PR 428424)
   maxDispatch = singlethread ? 1 : vmklnx_Helper_worlds / 2;

   wqPtr = (struct workqueue_struct *) kmalloc(sizeof(struct workqueue_struct) +
                  (maxDispatch * sizeof(struct work_tracker)), GFP_KERNEL);
   if(NULL == wqPtr) {
      VMKLNX_WARN("Couldn't allocate memory for workqueue struct");
      return NULL;
   }

   status = vmk_SPCreateIRQ_LEGACY(&wqPtr->wqLock, vmklinuxModID, "workQueueLock",
			 NULL, VMK_SP_RANK_IRQ_BLOCK_LEGACY);

   if (status != VMK_OK) {
      VMKLNX_WARN("LinuxWorkQueue_Init: vmk_SPCreate_LEGACY for workListLock failed: %s", vmk_StatusToString(status));
      kfree(wqPtr);
      return NULL;
   }

   mutex_init(&wqPtr->flushMutex);

   wqPtr->name = name;
   INIT_LIST_HEAD(&wqPtr->head);
   setup_timer(&wqPtr->delayTimer, vmklnx_workqueue_timeout, (unsigned long) wqPtr);
   wqPtr->qLen = 0;
   wqPtr->curPosted = 0;
   wqPtr->curDispatch = 0;
   wqPtr->maxDispatch = maxDispatch;
   wqPtr->waiting = 0;
   wqPtr->setExitFlag = 0;

   INIT_LIST_HEAD(&wqPtr->trackList);
   INIT_LIST_HEAD(&wqPtr->trackFlushList);
   INIT_LIST_HEAD(&wqPtr->trackFree);
   wtPtr = (struct work_tracker *)
                        (((char *) wqPtr) + sizeof(struct workqueue_struct));
   for (i = 0; i < maxDispatch; ++i) {
      list_add(&wtPtr->links, &wqPtr->trackFree);
      debug_set(wtPtr->executing, NULL);
      ++wtPtr;
   }
   qstat_incr(created);

   return wqPtr;
}
EXPORT_SYMBOL(__create_workqueue);

struct vmklnx_flush_plunger {
   struct work_struct work;
   struct workqueue_struct * wqPtr;
   struct completion completion;
};

static void
vmklnx_plunger_complete(struct work_struct *work)
{
   struct vmklnx_flush_plunger *plunger;
   struct workqueue_struct * wqPtr;
   unsigned vmkFlag;

   plunger = container_of(work, struct vmklnx_flush_plunger, work);

   // move the trackList over to the trackFlushList
   wqPtr = plunger->wqPtr;
   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   list_splice_init(&wqPtr->trackList, &wqPtr->trackFlushList);
   VMK_ASSERT(list_empty(&wqPtr->trackList));
   VMK_ASSERT(!list_empty(&wqPtr->trackFlushList));
   vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);

   complete(&plunger->completion);
}

/**                                          
 *  flush_workqueue - drain pending works in the workqueue
 *  @wqPtr : a pointer to a given workqueue
 *                                           
 *  Drains pending works in the workqueue, plus it guarantees
 *  that all callbacks associated with the drained work items have
 *  run to completion.
 *
 *  ESX Deviation Notes:
 *  If an associated work_struct is self re-initiating, then a
 *  then flush_workqueue() is not sufficient to free the work
 *  struct.  Instead, either cancel_work_sync() or
 *  cancel_delayed_work_sync() should be used, as appropriate.
 *
 *  RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: flush_workqueue */
void fastcall flush_workqueue(struct workqueue_struct *wqPtr)
{
   struct vmklnx_flush_plunger plunger;
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(wqPtr);

   if (NULL == wqPtr) {
     return;
   }

   vmk_WorldAssertIsSafeToBlock();

   /*
    * Single thread flushes on this queue so that we have
    * exclusive access to the trackFlushList.
    *
    * If we didn't do this, then multiple concurrent flush_workqueue()
    * operations might interfere with each other.  The problem is
    * that one flush_workqueue() might dumps additional items into the
    * trackFlushList that will be seen by a previous in progress
    * flush_workqueue().  While the probability of a livelock is
    * extremely small, we don't want to take chances.
    */
   mutex_lock(&wqPtr->flushMutex);

   /*
    * Throw a dummy work item into the queue (called the plunger).
    * When the plunger comes out the bottom, we know the pending
    * portion of the queue has been flushed, and that the in-progress
    * trackList has been moved to the trackFlushList.
    */
   init_completion(&plunger.completion);
   plunger.work.pending = 0;
   INIT_LIST_HEAD(&plunger.work.entry);
   plunger.work.func = vmklnx_plunger_complete;
   plunger.work.wq_data = NULL;
   plunger.work.module_id = vmk_ModuleStackTop();
   plunger.wqPtr = wqPtr;
   set_bit(__WORK_PENDING, &plunger.work.pending);
   __queue_work(wqPtr, &plunger.work);
   wait_for_completion(&plunger.completion);

   /*
    * Wait for the trackFlushList to drain out, guaranteeing
    * that all in progress workqueue callouts have completed.
    */
   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   while (!list_empty(&wqPtr->trackFlushList)) {
      ++wqPtr->waiting;
      vmk_WorldWaitIRQLegacy((vmk_WorldEventID) &wqPtr->waiting, &wqPtr->wqLock,
                         VMK_WORLD_WAIT_MISC, vmkFlag);
      vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
      --wqPtr->waiting;
   }
   vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
   mutex_unlock(&wqPtr->flushMutex);

   qstat_incr(flushes);
}
EXPORT_SYMBOL(flush_workqueue);

/**                                          
 *  flush_scheduled_work - a wrapper for flush_workqueue 
 *                                           
 *  A wrapper for flush_workqueue 
 *                                           
 *  RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: flush_scheduled_work */
void flush_scheduled_work(void)
{
    VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
    return(flush_workqueue(keventd_wq));
}
EXPORT_SYMBOL(flush_scheduled_work);

/**                                          
 *  destroy_workqueue - delete pending works and clean up a workqueue
 *  @wqPtr: pointer to the workqueue struct to be deleted    
 *                                           
 *  This function deletes pending works, cleans up and destroys the
 *  workqueue. 
 *                                           
 *  RETURN VALUE:                     
 *  None.
 *                                           
 *  ESX Deviation Notes:
 *  If any work items for this queue are self re-initiating, cancel_work_sync()
 *  or cancel_delayed_work_sync() should first be called for those items.
 *  The caller must ensure that all related cancel_work_sync() and
 *  cancel_delayed_work_sync() calls have completed execution before
 *  calling destroy_workqueue().
 */                                          
/* _VMKLNX_CODECHECK_: destroy_workqueue */
void destroy_workqueue(struct workqueue_struct *wqPtr)
{
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(wqPtr);

   if (NULL == wqPtr) {
      return;
   }

   vmk_WorldAssertIsSafeToBlock();

   if (test_and_set_bit(0, &wqPtr->setExitFlag) > 0) {
      VMKLNX_WARN("WQ Destroy already in process");
      return;
   }

   /* flush the queue */
   flush_workqueue(wqPtr);

   /*
    * Wait for all helping Q postings to drain out.
    */
   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   while (wqPtr->curPosted > 0) {
      vmk_WorldWaitIRQLegacy((vmk_WorldEventID) &wqPtr->curPosted, &wqPtr->wqLock,
                         VMK_WORLD_WAIT_MISC, vmkFlag);
      vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   }
   vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);

   VMK_ASSERT(wqPtr->qLen == 0);
   VMK_ASSERT(list_empty(&wqPtr->head));

   /* kill a possibly pending timer now */
   del_timer_sync(&wqPtr->delayTimer);

   VMK_ASSERT(wqPtr->curDispatch == 0);
   VMK_ASSERT(wqPtr->curPosted == 0);
   VMK_ASSERT(wqPtr->waiting == 0);
   VMK_ASSERT(__qlen(&wqPtr->trackList) == 0);
   VMK_ASSERT(__qlen(&wqPtr->trackFlushList) == 0);
   VMK_ASSERT(__qlen(&wqPtr->trackFree) == wqPtr->maxDispatch);

   vmk_SPDestroyIRQ(&wqPtr->wqLock);
   kfree(wqPtr);

   qstat_incr(destroyed);

   return;
}
EXPORT_SYMBOL(destroy_workqueue);

/*
 * vmklnx_workqueue_dispatch(struct workqueue_struct *wqPtr, unsigned vmkFlag)
 *
 *      Dispatch a request to the vmk_Helper if the conditions reuqire it.
 *
 *      If the vmk_HelperSubmitRequest() fails, then we fire off a timer
 *      to try later.
 *
 *      This function is called with the wqPtr->wqLock held.  It returns
 *      with the lock dropped.
 */
static inline void
vmklnx_workqueue_dispatch(struct workqueue_struct *wqPtr, unsigned vmkFlag)
{
   int postDeficit;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmk_SPAssertIsLockedIRQ(&wqPtr->wqLock);
   VMK_ASSERT(__qlen(&wqPtr->head) == wqPtr->qLen);
   VMK_ASSERT(wqPtr->curPosted >= 0);
   VMK_ASSERT(wqPtr->curDispatch >= 0);
   VMK_ASSERT(wqPtr->curPosted + wqPtr->curDispatch <= wqPtr->maxDispatch);

   postDeficit = min(wqPtr->maxDispatch - (wqPtr->curDispatch + wqPtr->curPosted),
                     wqPtr->qLen - wqPtr->curPosted);

   /*
    * Since queue work structs can be cancelled, it is impossible to
    * for wqPtr->qLen - wqPtr->curPosted to be negative.
    */
   if (postDeficit <= 0) {
      vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
      return;
   }

   /*
    * Pre-increment curPosted to prevent too many dispatches (i.e. helper
    * callbacks for this work queue).  Such would be a performance bug.
    *
    * In addition, curPosted posted is actually holding the queue in
    * existance if a concurrent destroy is executing.  So this increment
    * is necessary to make all further reference to wqPtr safe after the
    * lock is dropped.
    */
   wqPtr->curPosted += postDeficit;
   vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);

   while (postDeficit > 0) {

      VMK_ReturnStatus status;
      vmk_HelperRequestProps props = {
         .requestMayBlock = VMK_FALSE,
         .tag = (vmk_AddrCookie) NULL,
         .cancelFunc = NULL,
         .worldToBill = VMK_INVALID_WORLD_ID,
      };
      status = vmk_HelperSubmitRequest(vmklnx_Helper, vmklnx_workqueue_callout,
                                       wqPtr, &props);
      qstat_incr(dispatched);
      if (status != VMK_OK) {
         /*
          * Oops, the vmk_Helper is no longer accepting requests.
          * We'll compensate by firing off a timer to try later.
          */
         mod_timer(&wqPtr->delayTimer, jiffies + VMKLNX_MAX_QBLOCK_TIME);

         /*
          * Restore curPosted.  There is a chance (however small) that
          * the queue is being destroyed.
          */
         vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
         if ((wqPtr->curPosted -= postDeficit) == 0 && wqPtr->setExitFlag) {
            vmk_WorldWakeup((vmk_WorldEventID)&wqPtr->curPosted);
         }
         vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
         return;
      }

      --postDeficit;
   }
}

/*
 * void
 * vmklnx_workqueue_timeout(unsigned long data)
 */
static void
vmklnx_workqueue_timeout(unsigned long data)
{
   struct workqueue_struct *wqPtr = (struct workqueue_struct *) data;
   unsigned vmkFlag;

   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);

   // fire off requests to the Helper (if necessary)
   vmklnx_workqueue_dispatch(wqPtr, vmkFlag);

   // vmklnx_workqueue_dispatch drops the wqPtr->wqLock
}


/*
 * void
 * vmklnx_workqueue_callout(vmk_AddrCookie data)
 *
 *      Callback from the vmk_Helper.  Runs in the helper world context.
 *
 *      This function pulls the top item off the work queue, calls the
 *      client'function, and if necessary, sends another request to the
 *      vmk_Helper.
 */
static void
vmklnx_workqueue_callout(vmk_AddrCookie data)
{
   struct workqueue_struct *wqPtr = data.ptr;
   struct work_struct *wsPtr;
   struct work_tracker *wtPtr;
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#ifdef VMX86_DEBUG
   {
        struct task_struct *cur = get_current();

        container_of(cur, LinuxTaskExt, task)->flags |= LT_HELPER;
   }
#endif /* VMX86_DEBUG */

   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   VMK_ASSERT(__qlen(&wqPtr->head) == wqPtr->qLen);
   VMK_ASSERT(wqPtr->curPosted > 0);
   --wqPtr->curPosted;

   // no work items to dispatch?
   if (list_empty(&wqPtr->head)) {
      /*
       * if destroying the queue and if we are done servicing callouts
       * then wakeup the world waiting in destroy_workqueue()
       */
      if (wqPtr->setExitFlag && wqPtr->curPosted == 0) {
         vmk_WorldWakeup((vmk_WorldEventID)&wqPtr->curPosted);
      }
      vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
      return;
   }

   // max work items already dispatched?
   if (wqPtr->curDispatch >= wqPtr->maxDispatch) {
      vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
      return;
   }

   // Get the top work struct off the head of the queue
   wsPtr = list_entry(wqPtr->head.next, struct work_struct, entry);
   VMK_ASSERT(test_bit(__WORK_PENDING, &wsPtr->pending));
   VMK_ASSERT(wqPtr->qLen > 0);
   list_del_init(&wsPtr->entry);
   ++wqPtr->curDispatch;
   --wqPtr->qLen;
   VMK_ASSERT(__qlen(&wqPtr->head) == wqPtr->qLen);

   /*
    * Set up a tracking element to prepare for the possiblity of
    * the cancel_work_sync().
    */
   VMK_ASSERT(!list_empty(&wqPtr->trackFree));
   wtPtr = list_entry(wqPtr->trackFree.next, struct work_tracker, links);
   list_del_init(&wtPtr->links);
   VMK_ASSERT(wtPtr->executing == NULL);
   wtPtr->executing = wsPtr;
   list_add(&wtPtr->links, &wqPtr->trackList);
   VMK_ASSERT(__qlen(&wqPtr->trackList) + __qlen(&wqPtr->trackFlushList) ==
              wqPtr->curDispatch);
   VMK_ASSERT(__qlen(&wqPtr->trackFree) == wqPtr->maxDispatch - wqPtr->curDispatch);

   vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);

   {
      /*
       * Need to save info out of the work_struct, because as soon as
       * we clear the pending bit, this structure could be reused.
       */
      work_func_t g = NULL;
      int module_id = wsPtr->module_id;

      g = wsPtr->func;

      /*
       * Clear the pending bit, as technically, we're now executing.
       */
      clear_bit(__WORK_PENDING, &wsPtr->pending);

      /*
       * Call the module's function
       */
      VMK_ASSERT(qstat_read(callouts) < qstat_read(queued));

      VMKAPI_MODULE_CALL_VOID(module_id, g, wsPtr);
      if (irqs_disabled()) {
         VMKLNX_WARN("%p in module %d left interrupts disabled", g, module_id);
         VMK_ASSERT(VMK_FALSE);
      }
      qstat_incr(callouts);
   }

   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   --wqPtr->curDispatch;

   // is a cancel or flush waiting?
   if (wqPtr->waiting > 0) {
      vmk_WorldWakeup((vmk_WorldEventID)&wqPtr->waiting);
   }

   // done with the cancel tracking structure
   list_del_init(&wtPtr->links);
   debug_set(wtPtr->executing, NULL);
   list_add(&wtPtr->links, &wqPtr->trackFree);
   // fire off another request to the Helper (if necessary)
   vmklnx_workqueue_dispatch(wqPtr, vmkFlag);
   // vmklnx_workqueue_dispatch drops the wqPtr->wqLock
}

/*
 * static inline void
 * __queue_work(struct workqueue_struct *wqPtr, struct work_struct *work)
 *
 *      Internal function to queue up work (common to several paths).
 */
static inline void
__queue_work(struct workqueue_struct *wqPtr, struct work_struct *work)
{
   unsigned vmkFlag;

   VMK_ASSERT(list_empty(&work->entry));
   VMK_ASSERT(work->module_id != VMK_INVALID_MODULE_ID);
   VMK_ASSERT(test_bit(__WORK_PENDING, &work->pending));

   vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
   VMK_ASSERT(__qlen(&wqPtr->head) == wqPtr->qLen);
   VMK_ASSERT(!__on_q(wqPtr, work));
   list_add_tail(&work->entry, &wqPtr->head);
   ++wqPtr->qLen;
   work->wq_data = wqPtr;
   qstat_incr(queued);
   VMK_ASSERT(wqPtr->qLen > 0);

   // fire off a request to the Helper (if necessary)
   vmklnx_workqueue_dispatch(wqPtr, vmkFlag);

   // vmklnx_workqueue_dispatch drops the wqPtr->wqLock
}

/*
 *-----------------------------------------------------------------------------
 *
 * queue_work --
 *       Add a work to a given work queue 
 *
 * Results:
 *       non-zero for SUCCESS; 0 otherwise 
 *
 *-----------------------------------------------------------------------------
 */
/**                                          
 *  queue_work - queue a work struct on the given work queue.
 *  @wqPtr: Workqueue on which to queue work
 *  @work: Workstruct to queue
 *  
 *  ESX Deviation Notes:                     
 *  For ESX, the callback function is not bound to a physical CPU.
 *  For single threaded workqueues, Linux binds all callback functions to
 *  a single CPU.  For multi-threaded workqueues under Linux, each callback
 *  function stays bound to a single CPU once it starts executing.
 */                                          
/* _VMKLNX_CODECHECK_: queue_work */
int fastcall queue_work(struct workqueue_struct *wqPtr, struct work_struct *work)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (NULL == wqPtr) {
      VMKLNX_WARN("Couldn't queue_work because of a NULL work queue");
      return 0;
   }
   if (NULL == work) {
      VMKLNX_WARN("Couldn't queue_work because of a NULL work");
      return 0;
   }
   if (wqPtr->setExitFlag > 0) {
      VMKLNX_WARN("The WQ is in the process of being destroyed");
      return 0;
   }
   if (test_and_set_bit(__WORK_PENDING, &work->pending)) {
      VMKLNX_DEBUG(1, "queue_work: Couldn't queue_work because it is pending");
      return 0;
   }
   work->module_id = vmk_ModuleStackTop();

   __queue_work(wqPtr, work);

   return 1;
}
EXPORT_SYMBOL(queue_work);

/**
 *  schedule_work - put work task in global workqueue
 *  @work: job to be done
 *
 *  This puts a job in the kernel-global workqueue.
 *
 *  RETURN VALUE:
 *  0 on success, non-zero on error 
 */                                          
/* _VMKLNX_CODECHECK_: schedule_work */
int fastcall schedule_work(struct work_struct *work)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return(queue_work(keventd_wq, work));
} 
EXPORT_SYMBOL(schedule_work);

/*
 *-----------------------------------------------------------------------------
 *
 * delayed_work_timer_fn --
 *     Add work to workqueue when timer expires 
 *
 * Results
 *     None
 *
 *-----------------------------------------------------------------------------
 */
static void delayed_work_timer_fn(unsigned long data)
{
   struct work_struct *work;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   work = (struct work_struct *) data;

   VMK_ASSERT(work);
   VMK_ASSERT(work->module_id != VMK_INVALID_MODULE_ID);

   __queue_work((struct workqueue_struct *)work->wq_data, work);
}

/**                                          
 *  queue_delayed_work - queue work to given work queue and process after the 
 *  specified delay at least.
 *  @wq: workqueue to schedule work on
 *  @dwork: the work item to schedule
 *  @delay: the delay in jiffies of the minimum time until execution
 *                                           
 *  Queue a given work item to a workqueue and process this item after AT LEAST
 *  delay (in jiffies)
 *                                           
 *  ESX Deviation Notes:                     
 *  For ESX, the callback function is not bound to a physical CPU.
 *  For single threaded workqueues, Linux binds all callback functions to
 *  a single CPU.  For multi-threaded workqueues, each callback function
 *  stays bound to a single CPU once it starts executing.
 */                                          
/* _VMKLNX_CODECHECK_: queue_delayed_work */
int fastcall
queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay)
{
   struct timer_list *timer;
   struct work_struct *work;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if((NULL == wq) || (NULL == dwork)) {
      return 0;
   }

   work = &dwork->work;
   
   if (delay == 0)
      return queue_work(wq, work);

   /*
    * Bit 0 is marked to see if timer is pending. If so dont queue it
    */ 
   if (!test_and_set_bit(__WORK_PENDING, &work->pending)) {
      timer = &dwork->timer;
     
      VMK_ASSERT_BUG(!timer_pending(timer));
      VMK_ASSERT_BUG(list_empty(&work->entry));

      work->wq_data = wq;
      work->module_id = vmk_ModuleStackTop();
      VMK_ASSERT(work->module_id != VMK_INVALID_MODULE_ID);
      timer->expires = jiffies + delay;
      timer->data = (unsigned long)work;
      timer->function = delayed_work_timer_fn;
      __add_timer(work->module_id, timer);

      return 1;
   }

   return 0; 
}
EXPORT_SYMBOL(queue_delayed_work);

/**                                          
 *  schedule_delayed_work - put work task in global workqueue after delay
 *  @dwork: job to be done
 *  @delay: number of jiffies to wait
 *
 *  After waiting for a given time this puts a job in the kernel-global
 *  workqueue.
 *                                          
 *  RETURN VALUE:
 *  0 on success, non-zero on error
 */
/* _VMKLNX_CODECHECK_: schedule_delayed_work */
int fastcall
schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return(queue_delayed_work(keventd_wq, dwork, delay));
}
EXPORT_SYMBOL(schedule_delayed_work);

int
vmklnx_cancel_work_sync(struct work_struct *work, struct timer_list *timer)
{
   vmk_Bool didGrabPending = VMK_FALSE;
   vmk_Bool didWait;
   struct workqueue_struct *wqPtr; 
   struct work_tracker *wtPtr; 
   int ret = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   qstat_incr(cancel_entered);

   do {
      // try to grab the pending bit directly
      if (!test_and_set_bit(__WORK_PENDING, &work->pending)) {
         didGrabPending = VMK_TRUE;
         qstat_incr(grabbed);
      } else if (timer != NULL && del_timer_sync(timer)) {
         // try to grab pending from the timer

         VMK_ASSERT(test_bit(__WORK_PENDING, &work->pending));
         didGrabPending = VMK_TRUE;
         ret = 1;
         qstat_incr(timer_cancelled);
      }

      didWait = VMK_FALSE;

      // now look into the doings in the work queue
      wqPtr = (struct workqueue_struct *) work->wq_data;
      if (wqPtr != NULL) {
         unsigned vmkFlag;

         vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);

         // verify that the work queue is still valid
         if (wqPtr == (struct workqueue_struct *) work->wq_data) {
            struct list_head *pos;
            vmk_Bool running;

            // try to grab pending from the work queue
            if (!list_empty(&work->entry) && test_bit(__WORK_PENDING, &work->pending)) {
               list_del_init(&work->entry);
               --wqPtr->qLen;
               VMK_ASSERT(!didGrabPending);
               VMK_ASSERT(__qlen(&wqPtr->head) == wqPtr->qLen);
               qstat_incr(removed);
               didGrabPending = VMK_TRUE;
               ret = 1;
            }

            // scan the trackList for our work structure
            do {
               running = VMK_FALSE;

               list_for_each(pos, &wqPtr->trackList) {
                  wtPtr = list_entry(pos, struct work_tracker, links);
                  if (wtPtr->executing == work) {
                     running = VMK_TRUE;
                     break;
                  }
               }

               // similarly, scan the trackFlushList
               if (!running) {
                  list_for_each(pos, &wqPtr->trackFlushList) {
                     wtPtr = list_entry(pos, struct work_tracker, links);
                     if (wtPtr->executing == work) {
                        running = VMK_TRUE;
                        break;
                     }
                  }
               }

               if (running) {
                  qstat_incr(executing);
                  ++wqPtr->waiting;
                  vmk_WorldWaitIRQLegacy((vmk_WorldEventID) &wqPtr->waiting, &wqPtr->wqLock,
                                     VMK_WORLD_WAIT_MISC, vmkFlag);
                  qstat_incr(waited);
                  didWait = VMK_TRUE;
                  vmkFlag = vmk_SPLockIRQ(&wqPtr->wqLock);
                  --wqPtr->waiting;
               }
            } while (running);
         }
         vmk_SPUnlockIRQ(&wqPtr->wqLock, vmkFlag);
      }

      if (!didGrabPending && !didWait) {
         /*
          * We need to set a timer here because wakeups are not generated
          * when the pending bit is cleared.
          */
         vmk_WorldSleep(VMKLNX_CANCEL_WAIT_TIME);
         qstat_incr(busy_waited);
      }
   } while (!didGrabPending);

   qstat_incr(cancelled);
   clear_bit(__WORK_PENDING, &work->pending);

   return ret;
}
EXPORT_SYMBOL(vmklnx_cancel_work_sync);
