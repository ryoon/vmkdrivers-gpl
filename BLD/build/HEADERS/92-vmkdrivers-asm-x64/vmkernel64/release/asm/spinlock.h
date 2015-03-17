/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/atomic.h>
#include <asm/rwlock.h>
#include <asm/page.h>

#if defined(__VMKLNX__)
#include "vmkapi.h"
#endif /* defined(__VMKLNX__) */

/*
 * XXX: Somebody managed to create a big header mess which ends up in asm/smp.h
 * not being includable in spinlock.h.
 * Define raw_smp_processor_id in here for now to avoid that mess.
 */
#if defined(__VMKLNX__)
extern uint32_t raw_smp_processor_id(void);
#endif

/*
 * Your basic SMP spinlocks, allowing only a single CPU anywhere
 *
 * Simple spin lock operations.  There are two variants, one clears IRQ's
 * on the local processor, one does not.
 *
 * We make no fairness assumptions. They have a cost.
 *
 * (the type definitions are in asm/spinlock_types.h)
 */

#define __raw_spin_is_locked(x) \
		(*(volatile signed int *)(&(x)->slock) <= 0)

#if !defined(__VMKLNX__)
#define __raw_spin_lock_string \
	"\n1:\t" \
	LOCK_PREFIX " ; decl %0\n\t" \
	"js 2f\n" \
	LOCK_SECTION_START("") \
	"2:\t" \
	"rep;nop\n\t" \
	"cmpl $0,%0\n\t" \
	"jle 2b\n\t" \
	"jmp 1b\n" \
	LOCK_SECTION_END
#else /* defined(__VMKLNX__) */

asmlinkage void __raw_spin_failed(void /* special register calling convention */);

#define __raw_spin_lock_string \
	LOCK_PREFIX " ; decl %0\n\t" \
	"js 2f\n" \
	"1:\n" \
	LOCK_SECTION_START("") \
	"2:\t" \
        "pushq %%rdi\n\t" \
        "lea %0,%%rdi\n\t" \
	"call __raw_spin_failed\n\t" \
        "popq %%rdi\n\t" \
	"jmp 1b\n" \
	LOCK_SECTION_END

#endif /* defined(__VMKLNX__) */

#define __raw_spin_lock_string_up \
	"\n\tdecl %0"

#define __raw_spin_unlock_string \
	"movl $1,%0" \
		:"=m" (lock->slock) : : "memory"

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
#if defined(__VMKLNX__)
        vmk_AtomicPrologue();
#endif /* defined(__VMKLNX__) */
	asm volatile(__raw_spin_lock_string : "=m" (lock->slock) : : "memory");
#if defined(__VMKLNX__)
        vmk_AtomicEpilogue();
        lock->cpu = raw_smp_processor_id();
        lock->ra = (unsigned long) __builtin_return_address(0);
#endif /* defined(__VMKLNX__) */
}

#define __raw_spin_lock_flags(lock, flags) __raw_spin_lock(lock)

static inline int __raw_spin_trylock(raw_spinlock_t *lock)
{
	int oldval;

#if defined(__VMKLNX__)
        vmk_AtomicPrologue();
#endif /* defined(__VMKLNX__) */
	__asm__ __volatile__(
		"xchgl %0,%1"
		:"=q" (oldval), "=m" (lock->slock)
		:"0" (0) : "memory");
#if defined(__VMKLNX__)
        vmk_AtomicEpilogue();
        if (oldval > 0) {
                lock->cpu = raw_smp_processor_id();
                lock->ra = (unsigned long) __builtin_return_address(0);
        }
#endif /* defined(__VMKLNX__) */

	return oldval > 0;
}

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
#if defined(__VMKLNX__)
        lock->cpu = SPINLOCK_VMKERNEL_CPU_INVALID;
#endif /* defined(__VMKLNX__) */
	__asm__ __volatile__(
		__raw_spin_unlock_string
	);
}

#define __raw_spin_unlock_wait(lock) \
	do { while (__raw_spin_is_locked(lock)) cpu_relax(); } while (0)

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 *
 * On x86, we implement read-write locks as a 32-bit counter
 * with the high bit (sign) being the "contended" bit.
 */

#define __raw_read_can_lock(x)		((int)(x)->lock > 0)
#define __raw_write_can_lock(x)		((x)->lock == RW_LOCK_BIAS)

static inline void __raw_read_lock(raw_rwlock_t *rw)
{
	__build_read_lock(rw);
}

static inline void __raw_write_lock(raw_rwlock_t *rw)
{
	__build_write_lock(rw);
}

static inline int __raw_read_trylock(raw_rwlock_t *lock)
{
	atomic_t *count = (atomic_t *)lock;
	atomic_dec(count);
	if (atomic_read(count) >= 0)
		return 1;
	atomic_inc(count);
	return 0;
}

static inline int __raw_write_trylock(raw_rwlock_t *lock)
{
	atomic_t *count = (atomic_t *)lock;
	if (atomic_sub_and_test(RW_LOCK_BIAS, count))
		return 1;
	atomic_add(RW_LOCK_BIAS, count);
	return 0;
}

static inline void __raw_read_unlock(raw_rwlock_t *rw)
{
#if defined(__VMKLNX__)
        vmk_AtomicPrologue();
#endif /* defined(__VMKLNX__) */
	asm volatile(LOCK_PREFIX " ; incl %0" :"=m" (rw->lock) : : "memory");
#if defined(__VMKLNX__)
        vmk_AtomicEpilogue();
#endif /* defined(__VMKLNX__) */
}

static inline void __raw_write_unlock(raw_rwlock_t *rw)
{ 
#if defined(__VMKLNX__)
        vmk_AtomicPrologue();
#endif /* defined(__VMKLNX__) */
	asm volatile(LOCK_PREFIX " ; addl $" RW_LOCK_BIAS_STR ",%0"
				: "=m" (rw->lock) : : "memory");
#if defined(__VMKLNX__)
        vmk_AtomicEpilogue();
#endif /* defined(__VMKLNX__) */
}

#endif /* __ASM_SPINLOCK_H */
