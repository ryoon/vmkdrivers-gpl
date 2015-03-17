#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H
/* Simple interface for creating and stopping kernel threads without mess. */
#include <linux/err.h>
#include <linux/sched.h>

#if defined(__VMKLNX__)
/**
 *  kthread_create - create a kernel thread
 *  @fn: pointer to the thread entry function
 *  @data: argument to be passed to @fn
 *  @fmt: printf style format specification string
 *  @arg...: variable arguments
 *                                           
 *  Create a new kernel thread using @fn as the thread starting entry
 *  point. @data is passed as argument to @fn when it is invoked.
 *
 *  The thread is named according the format specification provided by 
 *  @fmt.  Additional input required by @fmt should be provided 
 *  in the list of arguments that go after @fmt.
 *
 *  SYNOPSIS:
 *  struct task_struct *kthread_create(int (*fn)(void *data),
 *				   void *data,
 *				   const char fmt[], ...);
 *
 *  ESX Deviation Notes:
 *  On ESX, there are no threads, or light weight processes. Blockable
 *  execution units are implemented as VMKernel Worlds.
 *
 *  The resulting World created by the function is named using the
 *  convention "<module name>:<function name>".  The @fmt argument
 *  is ignored.
 *
 *  RETURN VALUE:
 *  On success, return a pointer to the task_struct of the new thread;
 *  on failure, return -errno (negative errno) casted as a pointer.
 *
 *  SEE ALSO:
 *  kthread_stop() and kthread_should_stop()
 */                                          
/* _VMKLNX_CODECHECK_: kthread_create */
#define kthread_create(fn, data, fmt, arg...) \
   vmklnx_kthread_create(vmklnx_this_module_id, fn, data, #fn)

#else /* !defined(__VMKLNX__) */
struct task_struct *kthread_create(int (*fn)(void *data),
				   void *data,
				   const char fmt[], ...);
#endif /* defined(__VMKLNX__) */

/**
 * kthread_run - create and wake a thread.
 * @threadfn: the function to run until signal_pending(current).
 * @data: data ptr for @threadfn.
 * @namefmt: printf-style name for the thread.
 *
 * Description: Convenient wrapper for kthread_create() followed by
 * wake_up_process().  Returns the kthread or ERR_PTR(-ENOMEM).
 */
#define kthread_run(threadfn, data, namefmt, ...)			   \
({									   \
	struct task_struct *__k						   \
		= kthread_create(threadfn, data, namefmt, ## __VA_ARGS__); \
	if (!IS_ERR(__k))						   \
		wake_up_process(__k);					   \
	__k;								   \
})

void kthread_bind(struct task_struct *k, unsigned int cpu);
int kthread_stop(struct task_struct *k);
int kthread_should_stop(void);

#endif /* _LINUX_KTHREAD_H */
