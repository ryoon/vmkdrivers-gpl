/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/*
 * workqueue.h --- work queue handling for Linux.
 */

#ifndef _LINUX_WORKQUEUE_H
#define _LINUX_WORKQUEUE_H

#include <linux/timer.h>
#include <linux/linkage.h>
#include <linux/bitops.h>
#if defined(__VMKLNX__)
#include "vmklinux_dist.h"
extern int LinuxWorkQueue_Init(void);
extern void LinuxWorkQueue_Cleanup(void);
#endif /* defined(__VMKLNX__) */

struct workqueue_struct;

#if defined(__VMKLNX__)

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	unsigned long pending;
	struct list_head entry;
	work_func_t func;
	volatile void *wq_data;
        vmk_ModuleID module_id;
};

struct delayed_work{
	struct work_struct work;
        struct timer_list timer;
};

// define pending bits
#define __WORK_PENDING                  0

// define pending bits by numeric value
#define __WORK_PENDING_BIT      (1 << __WORK_PENDING)

#else /* !defined(__VMKLNX__) */
struct work_struct {
	unsigned long pending;
	struct list_head entry;
	void (*func)(void *);
        void *data;
	volatile void *wq_data;
	struct timer_list timer;
};
#endif /* defined(__VMKLNX__) */

struct execute_work {
	struct work_struct work;
};

#if defined(__VMKLNX__)
#define __WORK_INITIALIZER(n, f) {                              \
        .entry      = { &(n).entry, &(n).entry },               \
	.func       = (f),					\
        .wq_data    = (NULL),                                   \
        .module_id  = (VMK_INVALID_MODULE_ID),                  \
        }

#define __DELAYED_WORK_INITIALIZER(n, f) {                      \
        .work = __WORK_INITIALIZER((n).work, (f)),              \
        .timer      = TIMER_INITIALIZER(NULL, 0, 0),		\
        }
#else /* !defined(__VMKLNX__) */
#define __WORK_INITIALIZER(n, f, d) {				\
	.entry  = { &(n).entry, &(n).entry },			\
	.func = (f),						\
	.data = (d),						\
	.timer = TIMER_INITIALIZER(NULL, 0, 0),			\
	}
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__) 
/**
 *  DECLARE_WORK - Declare and initialize a work_struct
 *  @n: name of the work_struct to declare
 *  @f: function to be called from the work queue
 *
 *  Declares and initializes a work_struct with the given name, using the 
 *  specified callback function.  The callback function 
 *  executes in the context of a work queue thread.
 *
 *  SYNOPSIS:
 *
 *  #define DECLARE_WORK(n, f)
 *
 *  RETURN VALUE:
 *  NONE
 */
/* _VMKLNX_CODECHECK_: DECLARE_WORK */
#define DECLARE_WORK(n, f)					\
	struct work_struct n = __WORK_INITIALIZER(n, f)

/**
 *  DECLARE_DELAYED_WORK - Declare and initialize a delayed_work
 *  @n: name of the delayed_work to declare
 *  @f: function to be called from the work queue
 *
 *  Declares and initializes a delayed_work with the given name, using the 
 *  specified callback function.  The callback function 
 *  executes in the context of a work queue thread.
 *
 *  SYNOPSIS:
 *
 *  #define DECLARE_DELAYED_WORK(n, f)
 *
 *  RETURN VALUE:
 *  NONE
 */
/* _VMKLNX_CODECHECK_: DECLARE_DELAYED_WORK */
#define DECLARE_DELAYED_WORK(n, f)                              \
	struct delayed_work n = __DELAYED_WORK_INITIALIZER(n, f)

#else /* !defined(__VMKLNX__) */

#define DECLARE_WORK(n, f, d)					\
	struct work_struct n = __WORK_INITIALIZER(n, f, d)
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__)
/*
 * initialize a work-struct's func and data pointers:
 */

/**
 *  PREPARE_WORK - Initialize a work_struct
 *  @_work: name of the work_struct to prepare
 *  @_func: function to be called from the work queue
 *
 *  Initializes the given work_struct (@_work), using the 
 *  specified callback function (@_func).  The callback function
 *  executes in the context of a work queue thread.
 *
 *  SYNOPSIS:
 *
 *  #define PREPARE_WORK(_work, _func)
 *
 *  RETURN VALUE:
 *  NONE
 */
/* _VMKLNX_CODECHECK_: PREPARE_WORK */
#define PREPARE_WORK(_work, _func)			        \
	do {							\
		(_work)->func = _func;				\
	} while (0)

/**
 *  PREPARE_DELAYED_WORK - Initialize a delayed_work
 *  @_dwork: name of the delayed_work to prepare
 *  @_func: function to be called from the work queue
 *
 *  Initializes the given delayed_work (@_dwork), using the 
 *  specified callback function (@_func).  The callback function
 *  executes in the context of a work queue thread.
 *
 *  SYNOPSIS:
 *
 *  #define PREPARE_DELAYED_WORK(_dwork, _func)
 *
 *  RETURN VALUE:
 *  NONE
 */
/* _VMKLNX_CODECHECK_: PREPARE_WORK */
#define PREPARE_DELAYED_WORK(_dwork, _func)			\
	PREPARE_WORK(&(_dwork)->work, (void*)(_func))

/*
 * Initialize all of a work-struct.
 */

/**
 *  INIT_WORK - Initialize all of a work_struct
 *  @_work: name of the work_struct to prepare
 *  @_func: function to be called from the work queue
 *
 *  Initializes the given work_struct (@_work)
 *
 *  SYNOPSIS:
 *
 *  #define INIT_WORK(_work, _func)
 *
 *  RETURN VALUE:
 *  	None
 */
/* _VMKLNX_CODECHECK_: INIT_WORK */
#define INIT_WORK(_work, _func)	    			        \
	do {							\
		INIT_LIST_HEAD(&(_work)->entry);		\
		(_work)->pending = 0;	                        \
		PREPARE_WORK((_work), (_func));	                \
		(_work)->module_id = VMK_INVALID_MODULE_ID;	\
	} while (0)

/**
 *  INIT_DELAYED_WORK - Initialize all of a delayed_work
 *  @_dwork: name of the delayed_work to prepare
 *  @_func: function to be called from the work queue
 *
 *  Initializes the given delayed_work (@_dwork)
 *
 *  SYNOPSIS:
 *
 *  #define INIT_DELAYED_WORK(_dwork, _func)
 *
 *  RETURN VALUE:
 *  	None
 */
/* _VMKLNX_CODECHECK_: INIT_DELAYED_WORK */
#define INIT_DELAYED_WORK(_dwork, _func)                        \
        do {                                                    \
                INIT_WORK(&(_dwork)->work, (_func));            \
                init_timer(&(_dwork)->timer);                   \
        } while (0)
#else /* !defined(__VMKLNX__) */
/*
 * initialize a work-struct's func and data pointers:
 */
#define PREPARE_WORK(_work, _func, _data)			\
	do {							\
		(_dwork)->func = _func;				\
		(_dwork)->data = _data;				\
	} while (0)

/*
 * initialize all of a work-struct:
 */
#define INIT_WORK(_work, _func, _data)				\
	do {							\
		INIT_LIST_HEAD(&(_work)->entry);		\
		(_work)->pending = 0;				\
		PREPARE_WORK((_work), (_func), (_data));	\
		init_timer(&(_work)->timer);			\
	} while (0)
#endif /* defined(__VMKLNX__) */

extern struct workqueue_struct *__create_workqueue(const char *name,
						    int singlethread);
/**                                          
 *  create_workqueue - create a multithreaded workqueue
 *  @name: pointer to given wq name
 *                                                     
 *  Create and initialize a workqueue struct
 *  
 *  SYNOPSIS:
 *     #define create_workqueue(name)
 *  
 *  RETURN VALUE:
 *  If successful: pointer to the workqueue struct
 *  NULL otherwise
 *  
 *  ESX Deviation Notes:
 *  For ESX, the callback function is not bound to a physical CPU.
 *  For multi-threaded workqueues under Linux, each callback
 *  function stays bound to a single CPU once it starts executing.
 */
/* _VMKLNX_CODECHECK_: create_workqueue */
#define create_workqueue(name) __create_workqueue((name), 0)

/**                                          
 *  create_singlethread_workqueue - create a singlethreaded workqueue
 *  @name: pointer to given wq name
 *                                                     
 *  Create and initialize a singlethreaded workqueue struct
 *  
 *  SYNOPSIS:
 *     #define create_singlethread_workqueue(name)
 *  
 *  RETURN VALUE:
 *  If successful: pointer to the workqueue struct
 *  NULL otherwise
 *  
 *  ESX Deviation Notes:
 *  For ESX, the callback function is not bound to a physical CPU.
 *  For single threaded workqueues, Linux binds all callback functions to
 *  a single CPU.  
 */
/* _VMKLNX_CODECHECK_: create_singlethread_workqueue */
#define create_singlethread_workqueue(name) __create_workqueue((name), 1)

extern void destroy_workqueue(struct workqueue_struct *wq);

extern int FASTCALL(queue_work(struct workqueue_struct *wq, struct work_struct *work));
extern int FASTCALL(queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay));
extern int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
	struct work_struct *work, unsigned long delay);
extern void FASTCALL(flush_workqueue(struct workqueue_struct *wq));

extern int FASTCALL(schedule_work(struct work_struct *work));
extern int FASTCALL(schedule_delayed_work(struct delayed_work *dwork, unsigned long delay));

extern int schedule_delayed_work_on(int cpu, struct delayed_work *dwork, unsigned long delay);
extern int schedule_on_each_cpu(void (*func)(void *info), void *info);

extern void flush_scheduled_work(void);
extern int current_is_keventd(void);
extern int keventd_up(void);

extern void init_workqueues(void);
void cancel_rearming_delayed_work(struct work_struct *work);
void cancel_rearming_delayed_workqueue(struct workqueue_struct *,
				       struct work_struct *);
int execute_in_process_context(void (*fn)(void *), void *,
			       struct execute_work *);


#if defined(__VMKLNX__)
extern int vmklnx_cancel_work_sync(struct work_struct *work, struct timer_list *timer);

/**                                          
 *  cancel_work_sync - block until a work_struct's callback has terminated
 *  @work: the work to be cancelled
 *                                           
 * cancel_work_sync() will cancel the work if it is queued.  If one or more
 * invocations of the work's callback function are running, cancel_work_sync()
 * will block until all invocations have completed.
 *
 * It is possible to use this function if the work re-queues itself. However,
 * if work jumps from one queue to another, then completion of the callback
 * is only guaranteed from the last workqueue.
 *
 * cancel_delayed_work_sync() should be used for cases where the
 * work structure was queued by queue_delayed_work().
 *
 * The caller must ensure that workqueue_struct on which this work was last
 * queued can't be destroyed before this function returns.
 *
 * RETURN VALUE:
 * 1 if the work structure was queued on a workqueue and 0 otherwise.
 */                                          
/* _VMKLNX_CODECHECK_: cancel_work_sync */
static inline int cancel_work_sync(struct work_struct *work)
{
   return vmklnx_cancel_work_sync(work, NULL);
}

/**                                          
 *  cancel_delayed_work_sync - block until a work_struct's callback has terminated
 *  @dwork: the work to be cancelled
 *                                           
 * cancel_delayed_work_sync() will cancel the work if it is queued, or if the delay
 * timer is pending.  If one or more invocations of the work's callback function
 * are running, cancel_delayed_work_sync() will block until all invocations have
 * completed.
 *
 * It is possible to use this function if the work re-queues itself. However,
 * if work jumps from one queue to another, then completion of the callback
 * is only guaranteed from the last workqueue.
 *
 * The caller must ensure that workqueue_struct on which this work was last
 * queued can't be destroyed before this function returns.
 *
 * RETURN VALUE:
 * 1 if the work structure was waiting on a timer or was queued on a workqueue,
 * and 0 otherwise.
 */                                          
/* _VMKLNX_CODECHECK_: cancel_delayed_work_sync */
static inline int cancel_delayed_work_sync(struct delayed_work *dwork)
{
   return vmklnx_cancel_work_sync(&dwork->work, &dwork->timer);
}

/**                                          
 *  cancel_delayed_work - Cancel the timer associated with a delayed work structure
 *  @dwork: The work structure.
 *                                           
 *  The timer associated with the specified work structure, if previously
 *  passed to schedule_delayed_work, is cancelled.  However, if the timer
 *  has already fired this function has no effect.  Use function
 *  flush_workqueue() or cancel_delayed_work_sync() if it is necessary to
 *  wait for the callout function to terminate.
 *                                           
 *  RETURN VALUE:
 *  1 if the timer was pending at the time when cancel_delayed_work() was called,
 *  and 0 otherwise.
 */                                          
/* _VMKLNX_CODECHECK_: cancel_delayed_work */
static inline int cancel_delayed_work(struct delayed_work *dwork)
{
   int ret;

   if(NULL == dwork) {
      return 0;
   }

   ret = del_timer_sync(&dwork->timer);
   if (ret) {
      /*
       * Timer was still pending
       */
      clear_bit(__WORK_PENDING, &dwork->work.pending);
   }
   return ret;
}
#else /* !defined(__VMKLNX__) */
static inline int cancel_delayed_work(struct delayed_work *work)
{
	int ret;

	ret = del_timer_sync(&work->timer);
	if (ret)
		clear_bit(__WORK_PENDING, &work->pending);
	return ret;
}
#endif /* defined(__VMKLNX__) */

#endif
