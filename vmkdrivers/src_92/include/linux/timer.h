/*
 * Portions Copyright 2008, 2010-2011 VMware, Inc.
 */
#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>

#if defined(__VMKLNX__)
#include "vmkapi.h"
#endif

struct tvec_t_base_s;

struct timer_list {
#if !defined(__VMKLNX__)
	struct list_head entry;
#endif
	unsigned long expires;

	void (*function)(unsigned long);
	unsigned long data;

#if defined(__VMKLNX__)
	vmk_Timer vmk_handle;
#else
	struct tvec_t_base_s *base;
#endif
};

extern struct tvec_t_base_s boot_tvec_bases;

#if defined(__VMKLNX__)
#define TIMER_INITIALIZER(_function, _expires, _data) {		\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
		.vmk_handle = VMK_INVALID_TIMER,                \
	}
#else
#define TIMER_INITIALIZER(_function, _expires, _data) {		\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
		.base = &boot_tvec_bases,			\
	}
#endif

#define DEFINE_TIMER(_name, _function, _expires, _data)		\
	struct timer_list _name =				\
		TIMER_INITIALIZER(_function, _expires, _data)

void fastcall init_timer(struct timer_list * timer);

/**                                          
 *  setup_timer - Initialize timer fields
 *  @timer: timer to be initialized
 *  @function: timer function 
 *  @data: timer data 
 *                                           
 *  This is a helper function that initializes funtion and data fields of 
 *  timer.
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: setup_timer */
static inline void setup_timer(struct timer_list * timer,
				void (*function)(unsigned long),
				unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer(timer);
}

/**
 * timer_pending - is a timer pending?
 * @timer: the timer in question
 *
 * timer_pending will tell whether a given timer is currently pending,
 * or not. Callers must ensure serialization wrt. other operations done
 * to this timer, eg. interrupt contexts, or other CPUs on SMP.
 *
 * return value: 1 if the timer is pending, 0 if not.
 */
/* _VMKLNX_CODECHECK_: timer_pending */
static inline int timer_pending(const struct timer_list * timer)
{
#if defined(__VMKLNX__)
	extern int __timer_pending(const struct timer_list *timer);
	return __timer_pending(timer);
#else
	return timer->entry.next != NULL;
#endif
}

extern void add_timer_on(struct timer_list *timer, int cpu);
extern int del_timer(struct timer_list * timer);
extern int __mod_timer(struct timer_list *timer, unsigned long expires);
#if defined(__VMKLNX__)
/**                                          
 *  mod_timer - Modify expiration time of a timer
 *  @timer: the timer in question
 *  @expires: the new expiration time
 *                                           
 *  Modify the expiration time of a timer
 *
 *  Return Value:
 *  0 means the timer was not pending when being modified;
 *  1 means the timer was pending
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: mod_timer */
static inline int mod_timer(struct timer_list *timer, unsigned long expires)
{
	return __mod_timer(timer, expires);
}
#else
extern int mod_timer(struct timer_list *timer, unsigned long expires);
#endif

extern unsigned long next_timer_interrupt(void);

#if defined(__VMKLNX__)
extern void __add_timer(vmk_ModuleID modID, struct timer_list *timer);
#endif

/**
 *  add_timer - start a timer
 *  @timer: the timer to be added
 *
 *  The kernel will do a ->function(->data) callback from the
 *  timer interrupt at the ->expires point in the future. The
 *  current time is 'jiffies'.
 *
 *  The timer's ->expires, ->function (and if the handler uses it, ->data)
 *  fields must be set prior calling this function.
 *
 *  Timers with an ->expires field in the past will be executed in the next
 *  timer tick.
 *
 *  RETURN VALUE:
 *  NONE
 */
/* _VMKLNX_CODECHECK_: add_timer */
static inline void add_timer(struct timer_list *timer)
{
#if defined(__VMKLNX__)
	BUG_ON(timer_pending(timer));
	__add_timer(vmk_ModuleCurrentID, timer);
#else
	BUG_ON(timer_pending(timer));
	__mod_timer(timer, timer->expires);
#endif
}

#ifdef CONFIG_SMP
  extern int try_to_del_timer_sync(struct timer_list *timer);
  extern int del_timer_sync(struct timer_list *timer);
#else

#  if defined(__VMKLNX__)
#  error vmkdrivers must be compiled SMP
#  endif /* defined(__VMKLNX__) */

# define try_to_del_timer_sync(t)	del_timer(t)
# define del_timer_sync(t)		del_timer(t)
#endif

/**
 *  del_singleshot_timer_sync - Disarms and removes a timer by waiting for the
 *  timer handler to complete
 *  @t: timer to remove
 *
 *  Disarms and removes a timer by waiting for the timer handler to complete.
 *
 *  SYNOPSIS:
 *  #define del_singleshot_timer_sync(t)
 *
 *  SEE ALSO:
 *  del_timer_sync
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: del_singleshot_timer_sync */
#define del_singleshot_timer_sync(t) del_timer_sync(t)

extern void init_timers(void);
extern void run_local_timers(void);
struct hrtimer;
extern int it_real_fn(struct hrtimer *);

#if defined(__VMKLNX__)
/* 2010: update from linux source */
extern unsigned long round_jiffies(unsigned long j);
extern unsigned long round_jiffies_relative(unsigned long j);

/* 2011: from Linux 2.6.34.1, commit 3db48f5c1a68e801146ca58ff94f3898c6fbf90e */
extern unsigned long round_jiffies_up(unsigned long j);
extern unsigned long round_jiffies_up_relative(unsigned long j);
#endif /* defined(__VMKLNX__) */

#endif
