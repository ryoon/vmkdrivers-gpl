/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef _X86_64_SEMAPHORE_H
#define _X86_64_SEMAPHORE_H

#include <linux/linkage.h>

#ifdef __KERNEL__

/*
 * SMP- and interrupt-safe semaphores..
 *
 * (C) Copyright 1996 Linus Torvalds
 *
 * Modified 1996-12-23 by Dave Grothe <dave@gcom.com> to fix bugs in
 *                     the original code and to make semaphore waits
 *                     interruptible so that processes waiting on
 *                     semaphores can be killed.
 * Modified 1999-02-14 by Andrea Arcangeli, split the sched.c helper
 *		       functions in asm/sempahore-helper.h while fixing a
 *		       potential and subtle race discovered by Ulrich Schmid
 *		       in down_interruptible(). Since I started to play here I
 *		       also implemented the `trylock' semaphore operation.
 *          1999-07-02 Artur Skawina <skawina@geocities.com>
 *                     Optimized "0(ecx)" -> "(ecx)" (the assembler does not
 *                     do this). Changed calling sequences from push/jmp to
 *                     traditional call/ret.
 * Modified 2001-01-01 Andreas Franck <afranck@gmx.de>
 *		       Some hacks to ensure compatibility with recent
 *		       GCC snapshots, to avoid stack corruption when compiling
 *		       with -fomit-frame-pointer. It's not sure if this will
 *		       be fixed in GCC, as our previous implementation was a
 *		       bit dubious.
 *
 * If you would like to see an analysis of this implementation, please
 * ftp to gcom.com and download the file
 * /pub/linux/src/semaphore/semaphore-2.0.24.tar.gz.
 *
 */

#include <asm/system.h>
#include <asm/atomic.h>
#include <asm/rwlock.h>
#include <linux/wait.h>
#include <linux/rwsem.h>
#include <linux/stringify.h>

#if defined(__VMKLNX__)
#include "vmkapi.h"
#endif /* defined(__VMKLNX__) */

struct semaphore {
	atomic_t count;
	int sleepers;
	wait_queue_head_t wait;
};

/**
 *  __SEMAPHORE_INITIALIZER - initialize the semaphore
 *  @name: name of the semaphore
 *  @n: count for the semaphore
 *
 *  Initialize the semaphore and set its count to @n, the number of sleepers to
 *  0 and initialize its wait queue.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 */
/* _VMKLNX_CODECHECK_: __SEMAPHORE_INITIALIZER */
#define __SEMAPHORE_INITIALIZER(name, n)				\
{									\
	.count		= ATOMIC_INIT(n),				\
	.sleepers	= 0,						\
	.wait		= __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
}

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

/**                                          
 *  sema_init - Initializes semaphore value
 *  @sem: semaphore
 *  @val: value 
 *                                           
 *  Initilizes semaphore value 
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: sema_init */
static inline void sema_init (struct semaphore *sem, int val)
{
/*
 *	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER((*sem),val);
 *
 * i'd rather use the more flexible initialization above, but sadly
 * GCC 2.7.2.3 emits a bogus warning. EGCS doesn't. Oh well.
 */
	atomic_set(&sem->count, val);
	sem->sleepers = 0;
	init_waitqueue_head(&sem->wait);
}

/**                                          
 *  init_MUTEX - Initializes the semaphore as free resource with exclusive access.
 *  @sem: Pointer to semaphore
 *                                           
 *  The given semaphore is initialized as a free resource with exclusive
 *  access.
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: init_MUTEX */
static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

/**                                          
 *  init_MUTEX_LOCKED - Initializes the semaphore as busy resource with exclusive access.
 *  @sem: Pointer to semaphore.
 *                                           
 *  The given semaphore is initialized as a busy resource with exclusive access
 *  given to the caller of the initialization function.
 *                                           
 *  RETURN VALUE:
 *  NONE 
 */                                          
/* _VMKLNX_CODECHECK_: init_MUTEX_LOCKED */
static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

asmlinkage void __down_failed(void /* special register calling convention */);
asmlinkage int  __down_failed_interruptible(void  /* params in registers */);
asmlinkage int  __down_failed_trylock(void  /* params in registers */);
asmlinkage void __up_wakeup(void /* special register calling convention */);

asmlinkage void __down(struct semaphore * sem);
asmlinkage int  __down_interruptible(struct semaphore * sem);
asmlinkage int  __down_trylock(struct semaphore * sem);
asmlinkage void __up(struct semaphore * sem);

/*
 * This is ugly, but we want the default case to fall through.
 * "__down_failed" is a special asm handler that calls the C
 * routine that actually waits. See arch/x86_64/kernel/semaphore.c
 */
/**                                          
 *  down - acquire a semaphore
 *  @sem: Pointer to the semaphore to be acquired
 *                                           
 *  Acquires the specified semaphore.  If the semaphore's count of available
 *  acquisitions is exhausted, then the calling function will block.
 *
 *  RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: down */
static inline void down(struct semaphore * sem)
{
	might_sleep();

#if defined(__VMKLNX__)
	vmk_AtomicPrologue();
	__asm__ __volatile__(
		"# atomic down operation\n\t"
		LOCK_PREFIX "decl %0\n\t"     /* --sem->count */
		"js 2f\n"
                "cmpb $0, vmk_AtomicUseFence(%%rip)\n"
                "jne 4f\n"
		"1:\n"
		LOCK_SECTION_START("")
                "2:\tcmpb $0, vmk_AtomicUseFence(%%rip)\n"
                "jne 3f\n"
		"call __down_failed\n\t"
                "jmp 1b\n"
                "3:\tlfence\n"
		"call __down_failed\n\t"
                "jmp 1b\n"
                "4:\tlfence\n"
		"jmp 1b\n"
                LOCK_SECTION_END
		:"=m" (sem->count)
		:"D" (sem)
		:"memory");
#else /* !defined(__VMKLNX__) */
	__asm__ __volatile__(
		"# atomic down operation\n\t"
		LOCK_PREFIX "decl %0\n\t"     /* --sem->count */
		"js 2f\n"
		"1:\n"
		LOCK_SECTION_START("")
		"2:\tcall __down_failed\n\t"
		"jmp 1b\n"
		LOCK_SECTION_END
		:"=m" (sem->count)
		:"D" (sem)
		:"memory");
#endif /* !defined(__VMKLNX__) */
}

/*
 * Interruptible try to acquire a semaphore.  If we obtained
 * it, return zero.  If we were interrupted, returns -EINTR
 */
/**                                          
 *  down_interruptible - Acquire a semaphore with an interruptible wait
 *  @sem: Pointer to the semaphore to be acquired
 *                                           
 *  Attempt to acquire the specified semaphore.  If the semaphore's count of available
 *  acquisitions is exhausted, then the calling function will block.  If a
 *  signal is received by the blocking thread, then this function aborts.
 *
 *  RETURN VALUE:
 *  0 on successful acquisition, or -EINTR if interrupted by a signal
 */                                          
/* _VMKLNX_CODECHECK_: down_interruptible */
static inline int down_interruptible(struct semaphore * sem)
{
	int result;

	might_sleep();

#if defined(__VMKLNX__)
	vmk_AtomicPrologue();
	__asm__ __volatile__(
		"# atomic interruptible down operation\n\t"
		LOCK_PREFIX "decl %1\n\t"     /* --sem->count */
		"js 2f\n\t"
		"xorl %0,%0\n"
                "cmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 4f\n"
		"1:\n"
		LOCK_SECTION_START("")
                "2:\tcmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 3f\n"
		"call __down_failed_interruptible\n\t"
                "jmp 1b\n"
                "3:\tlfence\n"
		"call __down_failed_interruptible\n\t"
                "jmp 1b\n"
                "4:\tlfence\n"
		"jmp 1b\n"
                LOCK_SECTION_END
		:"=a" (result), "=m" (sem->count)
		:"D" (sem)
		:"memory");
#else /* !defined(__VMKLNX__) */
	__asm__ __volatile__(
		"# atomic interruptible down operation\n\t"
		LOCK_PREFIX "decl %1\n\t"     /* --sem->count */
		"js 2f\n\t"
		"xorl %0,%0\n"
		"1:\n"
		LOCK_SECTION_START("")
		"2:\tcall __down_failed_interruptible\n\t"
		"jmp 1b\n"
		LOCK_SECTION_END
		:"=a" (result), "=m" (sem->count)
		:"D" (sem)
		:"memory");
#endif /* !defined(__VMKLNX__) */
	return result;
}

/*
 * Non-blockingly attempt to down() a semaphore.
 * Returns zero if we acquired it
 */
/**                                          
 *  down_trylock - attempt to acquire a semaphore
 *  @sem: Pointer to the semaphore to be acquired
 *                                           
 *  Attempt to acquire the specified semaphore.  If the semaphore's count of available
 *  acquisitions is exhausted, then return immediately.
 *
 *  RETURN VALUE:
 *  0 on successful acquisition, or 1 if the semaphore is not immediately available
 */                                          
/* _VMKLNX_CODECHECK_: down_trylock */
static inline int down_trylock(struct semaphore * sem)
{
	int result;

#if defined(__VMKLNX__)
	vmk_AtomicPrologue();
	__asm__ __volatile__(
		"# atomic interruptible down operation\n\t"
		LOCK_PREFIX "decl %1\n\t"     /* --sem->count */
		"js 2f\n\t"
		"xorl %0,%0\n"
                "cmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 4f\n"
		"1:\n"
		LOCK_SECTION_START("")
                "2:\tcmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 3f\n"
		"call __down_failed_trylock\n\t"
                "jmp 1b\n"
                "3:\tlfence\n"
		"call __down_failed_trylock\n\t"
                "jmp 1b\n"
                "4:\tlfence\n"
		"jmp 1b\n"
                LOCK_SECTION_END
		:"=a" (result), "=m" (sem->count)
		:"D" (sem)
		:"memory","cc");
#else /* !defined(__VMKLNX__) */
	__asm__ __volatile__(
		"# atomic interruptible down operation\n\t"
		LOCK_PREFIX "decl %1\n\t"     /* --sem->count */
		"js 2f\n\t"
		"xorl %0,%0\n"
		"1:\n"
		LOCK_SECTION_START("")
		"2:\tcall __down_failed_trylock\n\t"
		"jmp 1b\n"
		LOCK_SECTION_END
		:"=a" (result), "=m" (sem->count)
		:"D" (sem)
		:"memory","cc");
#endif /* !defined(__VMKLNX__) */
	return result;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
/**
 *  up - release semaphore
 *  @sem: pointer to initialized and acquired semaphore
 *
 *  RETURN VALUE:
 *  none
 */
 /* _VMKLNX_CODECHECK_: up               */
static inline void up(struct semaphore * sem)
{
#if defined(__VMKLNX__)
	vmk_AtomicPrologue();
	__asm__ __volatile__(
		"# atomic up operation\n\t"
		LOCK_PREFIX "incl %0\n\t"     /* ++sem->count */
		"jle 2f\n"
                "cmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 4f\n"
		"1:\n"
		LOCK_SECTION_START("")
                "2:\tcmpb $0, vmk_AtomicUseFence(%%rip)\n\t"
                "jne 3f\n"
		"call __up_wakeup\n\t"
                "jmp 1b\n"
                "3:\tlfence\n"
		"call __up_wakeup\n\t"
                "jmp 1b\n"
                "4:\tlfence\n"
		"jmp 1b\n"
                LOCK_SECTION_END
		:"=m" (sem->count)
		:"D" (sem)
		:"memory");
#else /* !defined(__VMKLNX__) */
	__asm__ __volatile__(
		"# atomic up operation\n\t"
		LOCK_PREFIX "incl %0\n\t"     /* ++sem->count */
		"jle 2f\n"
		"1:\n"
		LOCK_SECTION_START("")
		"2:\tcall __up_wakeup\n\t"
		"jmp 1b\n"
		LOCK_SECTION_END
		:"=m" (sem->count)
		:"D" (sem)
		:"memory");
#endif /* !defined(__VMKLNX__) */
}
#endif /* __KERNEL__ */
#endif
