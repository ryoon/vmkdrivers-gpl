/*
 * Portions Copyright 2008, 2011 VMware, Inc.
 */
#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

/*
 * include/linux/spinlock.h - generic spinlock/rwlock declarations
 *
 * here's the role of the various spinlock/rwlock related include files:
 *
 * on SMP builds:
 *
 *  asm/spinlock_types.h: contains the raw_spinlock_t/raw_rwlock_t and the
 *                        initializers
 *
 *  linux/spinlock_types.h:
 *                        defines the generic type and initializers
 *
 *  asm/spinlock.h:       contains the __raw_spin_*()/etc. lowlevel
 *                        implementations, mostly inline assembly code
 *
 *   (also included on UP-debug builds:)
 *
 *  linux/spinlock_api_smp.h:
 *                        contains the prototypes for the _spin_*() APIs.
 *
 *  linux/spinlock.h:     builds the final spin_*() APIs.
 *
 * on UP builds:
 *
 *  linux/spinlock_type_up.h:
 *                        contains the generic, simplified UP spinlock type.
 *                        (which is an empty structure on non-debug builds)
 *
 *  linux/spinlock_types.h:
 *                        defines the generic type and initializers
 *
 *  linux/spinlock_up.h:
 *                        contains the __raw_spin_*()/etc. version of UP
 *                        builds. (which are NOPs on non-debug, non-preempt
 *                        builds)
 *
 *   (included on UP-non-debug builds:)
 *
 *  linux/spinlock_api_up.h:
 *                        builds the _spin_*() APIs.
 *
 *  linux/spinlock.h:     builds the final spin_*() APIs.
 */

#include <linux/preempt.h>
#include <linux/linkage.h>
#include <linux/compiler.h>
#include <linux/thread_info.h>
#include <linux/kernel.h>
#include <linux/stringify.h>

#include <asm/system.h>

/*
 * Must define these before including other files, inline functions need them
 */
#define LOCK_SECTION_NAME ".text.lock."KBUILD_BASENAME

#define LOCK_SECTION_START(extra)               \
        ".subsection 1\n\t"                     \
        extra                                   \
        ".ifndef " LOCK_SECTION_NAME "\n\t"     \
        LOCK_SECTION_NAME ":\n\t"               \
        ".endif\n"

#define LOCK_SECTION_END                        \
        ".previous\n\t"

#if defined(__VMKLNX__)
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#endif /* defined(__VMKLNX__) */

#define __lockfunc fastcall __attribute__((section(".spinlock.text")))

/*
 * Pull the raw_spinlock_t and raw_rwlock_t definitions:
 */
#include <linux/spinlock_types.h>

extern int __lockfunc generic__raw_read_trylock(raw_rwlock_t *lock);

/*
 * Pull the __raw*() functions/declarations (UP-nondebug doesnt need them):
 */
#ifdef CONFIG_SMP
# include <asm/spinlock.h>
#else
# include <linux/spinlock_up.h>
#endif

#ifdef CONFIG_DEBUG_SPINLOCK
extern void __spin_lock_init(spinlock_t *lock, const char *name,
                             struct lock_class_key *key);
#endif

/**
 *  spin_lock_init - Initialize a spin lock
 *  @lock: lock to initialize
 *
 *  Initializes a spin lock to an unlocked state
 *
 *  SYNOPSIS:
 *     #define spin_lock_init(lock)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_lock_init */
#ifdef CONFIG_DEBUG_SPINLOCK
#define spin_lock_init(lock)					\
do {								\
	static struct lock_class_key __key;			\
								\
	__spin_lock_init((lock), #lock, &__key);		\
} while (0)
#else
#define spin_lock_init(lock)					\
	do { *(lock) = SPIN_LOCK_UNLOCKED; } while (0)
#endif /* CONFIG_DEBUG_SPINLOCK */

#ifdef CONFIG_DEBUG_SPINLOCK
  extern void __rwlock_init(rwlock_t *lock, const char *name,
			    struct lock_class_key *key);
#define rwlock_init(lock)					\
do {								\
	static struct lock_class_key __key;			\
								\
	__rwlock_init((lock), #lock, &__key);			\
} while (0)
#else
#define rwlock_init(lock)					\
	do { *(lock) = RW_LOCK_UNLOCKED; } while (0)
#endif

/**
 *  spin_is_locked - Test to see if a lock is already held
 *  @lock: lock to test
 *
 *  Checks to see if the lock is held, but does not change the lock's state
 *
 *  SYNOPSIS:
 *     #define spin_is_locked(lock)
 *
 *  RETURN VALUE:
 *  TRUE if lock is already locked, FALSE otherwise
 *
 */
/* _VMKLNX_CODECHECK_: spin_is_locked */
#define spin_is_locked(lock)	__raw_spin_is_locked(&(lock)->raw_lock)

/**
 *  spin_unlock_wait - Wait until the spinlock gets unlocked
 *  @lock: lock to wait upon
 *
 *  Waits for a spinlock to become unlocked, but does not acquire the lock
 *
 *  SYNOPSIS:
 *     #define spin_unlock_wait(lock)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_unlock_wait */
#define spin_unlock_wait(lock)	__raw_spin_unlock_wait(&(lock)->raw_lock)

/*
 * Pull the _spin_*()/_read_*()/_write_*() functions/declarations:
 */
#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
# include <linux/spinlock_api_smp.h>
#else
# include <linux/spinlock_api_up.h>
#endif

#ifdef CONFIG_DEBUG_SPINLOCK
 extern void _raw_spin_lock(spinlock_t *lock);
#define _raw_spin_lock_flags(lock, flags) _raw_spin_lock(lock)
 extern int _raw_spin_trylock(spinlock_t *lock);
 extern void _raw_spin_unlock(spinlock_t *lock);
 extern void _raw_read_lock(rwlock_t *lock);
 extern int _raw_read_trylock(rwlock_t *lock);
 extern void _raw_read_unlock(rwlock_t *lock);
 extern void _raw_write_lock(rwlock_t *lock);
 extern int _raw_write_trylock(rwlock_t *lock);
 extern void _raw_write_unlock(rwlock_t *lock);
#else
# define _raw_spin_lock(lock)		__raw_spin_lock(&(lock)->raw_lock)
# define _raw_spin_lock_flags(lock, flags) \
		__raw_spin_lock_flags(&(lock)->raw_lock, *(flags))
# define _raw_spin_trylock(lock)	__raw_spin_trylock(&(lock)->raw_lock)
# define _raw_spin_unlock(lock)		__raw_spin_unlock(&(lock)->raw_lock)
# define _raw_read_lock(rwlock)		__raw_read_lock(&(rwlock)->raw_lock)
# define _raw_read_trylock(rwlock)	__raw_read_trylock(&(rwlock)->raw_lock)
# define _raw_read_unlock(rwlock)	__raw_read_unlock(&(rwlock)->raw_lock)
# define _raw_write_lock(rwlock)	__raw_write_lock(&(rwlock)->raw_lock)
# define _raw_write_trylock(rwlock)	__raw_write_trylock(&(rwlock)->raw_lock)
# define _raw_write_unlock(rwlock)	__raw_write_unlock(&(rwlock)->raw_lock)
#endif

#define read_can_lock(rwlock)		__raw_read_can_lock(&(rwlock)->raw_lock)
#define write_can_lock(rwlock)		__raw_write_can_lock(&(rwlock)->raw_lock)

/*
 * Define the various spin_lock and rw_lock methods.  Note we define these
 * regardless of whether CONFIG_SMP or CONFIG_PREEMPT are set. The various
 * methods are defined as nops in the case they are not required.
 */

/**
 *  spin_trylock - Attempt to acquire a spinlock without spinning
 *  @lock: lock to acquire
 *
 *  Attempts to acquire a spinlock that is otherwise acquired and released using 
 *  spin_lock and spin_unlock.  spin_trylock does not spin if the lock is
 *  already held.  Do not use this variant if the lock could be used in a 
 *  bottom-half or interrupt context.
 *
 *  SYNOPSIS:
 *     #define spin_trylock(lock)
 *
 *  SEE ALSO:
 *  spin_unlock, spin_trylock_irq, spin_trylock_irqsave
 *
 *  RETURN VALUE:
 *  TRUE if lock acquisition succeeded, FALSE if the lock was already held
 *
 */
/* _VMKLNX_CODECHECK_: spin_trylock */
#define spin_trylock(lock)		__cond_lock(_spin_trylock(lock))
#define read_trylock(lock)		__cond_lock(_read_trylock(lock))
#define write_trylock(lock)		__cond_lock(_write_trylock(lock))

/**
 *  spin_lock - Acquire a spinlock
 *  @lock: lock to acquire
 *
 *  Acquires a lock, spinning if necessary until the previous owner of the
 *  lock relinquishes it.  Do not use spin_lock / spin_unlock on locks that 
 *  can be held by either bottom-half or interrupt contexts.
 *
 *  SYNOPSIS:
 *     #define spin_lock(lock)
 *
 *  SEE ALSO:
 *  spin_unlock, spin_lock_bh, spin_lock_irq, spin_lock_irqsave
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_lock */
#define spin_lock(lock)			_spin_lock(lock)

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define spin_lock_nested(lock, subclass) _spin_lock_nested(lock, subclass)
#else
# define spin_lock_nested(lock, subclass) _spin_lock(lock)
#endif

#define write_lock(lock)		_write_lock(lock)
#define read_lock(lock)			_read_lock(lock)

#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
/**
 *  spin_lock_irqsave - Acquire a spinlock, save the current interrupt state and disable interrupts
 *  @lock: lock to acquire
 *  @flags: saved interrupt state
 *
 *  Acquires a spinlock, spinning if necessary until the previous owner of the
 *  lock relinquishes it.  During acquisition, spin_lock_irqsave saves the
 *  current interrupt state to "flags" and disables interrupts on the local
 *  CPU.
 *
 *  SYNOPSIS:
 *     #define spin_lock_irqsave(lock, flags)
 *
 *  SEE ALSO:
 *  spin_unlock_irqrestore, spin_trylock_irqsave
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_lock_irqsave */
#define spin_lock_irqsave(lock, flags)	flags = _spin_lock_irqsave(lock)
#define read_lock_irqsave(lock, flags)	flags = _read_lock_irqsave(lock)
#define write_lock_irqsave(lock, flags)	flags = _write_lock_irqsave(lock)
#else
#define spin_lock_irqsave(lock, flags)	_spin_lock_irqsave(lock, flags)
#define read_lock_irqsave(lock, flags)	_read_lock_irqsave(lock, flags)
#define write_lock_irqsave(lock, flags)	_write_lock_irqsave(lock, flags)
#endif

/**
 *  spin_lock_irq - Acquire a spinlock and disable interrupts
 *
 *  @lock: lock to acquire
 *
 *  Acquires a spinlock, spinning if necessary until the previous owner of the
 *  lock relinquishes it.  During acquisition, spin_lock_irq disables interrupts
 *  on the local CPU but does not save the interrupt state.
 *
 *  SYNOPSIS:
 *     #define spin_lock_irq(lock)
 *
 *  SEE ALSO:
 *  spin_unlock_irq, spin_trylock_irq, spin_lock_irqsave
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_lock_irq */
#define spin_lock_irq(lock)		_spin_lock_irq(lock)

/**
 *  spin_lock_bh - Acquire a spinlock and disable bottom-half execution
 *  @lock: lock to acquire
 *
 *  Acquires a spinlock, spinning if necessary until the previous owner of the
 *  lock relinquishes it.  During acquisition, spin_lock_bh disables bottom
 *  half contexts (such as timers and tasklets) from running in the future
 *  on the local CPU.  Usually bottom-halves are re-enabled using 
 *  spin_unlock_bh.  Do not use spin_lock_bh if the lock could be used by an
 *  interrupt context.
 *
 *  SYNOPSIS:
 *     #define spin_lock_bh(lock)
 *
 *  SEE ALSO:
 *  spin_unlock_bh, spin_lock_irqsave
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_lock_bh */
#define spin_lock_bh(lock)		_spin_lock_bh(lock)

#define read_lock_irq(lock)		_read_lock_irq(lock)
#define read_lock_bh(lock)		_read_lock_bh(lock)

#define write_lock_irq(lock)		_write_lock_irq(lock)
#define write_lock_bh(lock)		_write_lock_bh(lock)


/*
 * We inline the unlock functions in the nondebug case:
 */

#if defined(__VMKLNX__)

/**
 *  spin_unlock - Release a spinlock
 *  @lock: lock to release
 *
 *  Releases a lock that was acquired with spin_lock.  Do not use spin_lock / 
 *  spin_unlock on locks that can be held by either bottom-half (e.g., 
 *  timer, tasklet, etc) or interrupt contexts.
 *
 *  SYNOPSIS:
 *     #define spin_unlock(lock)
 *
 *  SEE ALSO:
 *  spin_lock, spin_unlock_bh, spin_unlock_irq, spin_unlock_irqrestore
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_unlock */
#define spin_unlock(lock)		_spin_unlock(lock)
#define read_unlock(lock)		_read_unlock(lock)
#define write_unlock(lock)		_write_unlock(lock)

/**
 *  spin_unlock_irq - Release a spinlock and enable local interrupts
 *  @lock: lock to release
 *
 *  Releases a lock that was acquired with spin_lock_irq and enables the
 *  local CPU's interrupts but does not restore any interrupt state.
 *
 *  SYNOPSIS:
 *     #define spin_unlock_irq(lock)
 *
 *  SEE ALSO:
 *  spin_lock_irq, spin_unlock_irqrestore
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_unlock_irq */
#define spin_unlock_irq(lock)		_spin_unlock_irq(lock)
#define read_unlock_irq(lock)		_read_unlock_irq(lock)
#define write_unlock_irq(lock)		_write_unlock_irq(lock)

#else /* !defined(__VMKLNX__) */

#if defined(CONFIG_DEBUG_SPINLOCK) || defined(CONFIG_PREEMPT) || \
	!defined(CONFIG_SMP)

#define spin_unlock(lock)		_spin_unlock(lock)
#define read_unlock(lock)		_read_unlock(lock)
#define write_unlock(lock)		_write_unlock(lock)

#define spin_unlock_irq(lock)		_spin_unlock_irq(lock)
#define read_unlock_irq(lock)		_read_unlock_irq(lock)
#define write_unlock_irq(lock)		_write_unlock_irq(lock)
#else

#define spin_unlock(lock)		__raw_spin_unlock(&(lock)->raw_lock)
#define read_unlock(lock)		__raw_read_unlock(&(lock)->raw_lock)
#define write_unlock(lock)		__raw_write_unlock(&(lock)->raw_lock)

#define spin_unlock_irq(lock) \
    do { __raw_spin_unlock(&(lock)->raw_lock); local_irq_enable(); } while (0)
#define read_unlock_irq(lock) \
    do { __raw_read_unlock(&(lock)->raw_lock); local_irq_enable(); } while (0)
#define write_unlock_irq(lock) \
    do { __raw_write_unlock(&(lock)->raw_lock); local_irq_enable(); } while (0)
#endif

#endif /* defined(__VMKLNX__) */

/**
 *  spin_unlock_irqrestore - Release a spinlock, enable local interrupts, and restore interrupt state
 *  @lock: lock to release
 *  @flags: interrupt state to restore
 *
 *  Releases a lock that was acquired with spin_lock_irqsave, restores the
 *  local CPU's interrupt-processing state, and enables the CPU's interrupts
 *
 *  SYNOPSIS:
 *     #define spin_unlock_irqrestore(lock, flags)
 *
 *  SEE ALSO:
 *  spin_lock_irqsave
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_unlock_irqrestore */
#define spin_unlock_irqrestore(lock, flags) \
					_spin_unlock_irqrestore(lock, flags)

/**
 *  spin_unlock_bh - Release a spinlock and enable bottom-half execution
 *  @lock: lock to release
 *
 *  Releases a lock that was acquired with spin_lock_bh and re-enables 
 *  bottom-half execution.  Do not use spin_lock_bh / spin_unlock_bh if the
 *  lock could be held by an interrupt context.
 *
 *  SYNOPSIS:
 *     #define spin_unlock_bh(lock)
 *
 *  SEE ALSO:
 *  spin_lock_bh, spin_unlock_irq, spin_unlock_irqrestore
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spin_unlock_bh */
#define spin_unlock_bh(lock)		_spin_unlock_bh(lock)

#define read_unlock_irqrestore(lock, flags) \
					_read_unlock_irqrestore(lock, flags)
#define read_unlock_bh(lock)		_read_unlock_bh(lock)

#define write_unlock_irqrestore(lock, flags) \
					_write_unlock_irqrestore(lock, flags)
#define write_unlock_bh(lock)		_write_unlock_bh(lock)

/**
 *  spin_trylock_irq - Attempt to acquire an interrupt-context spinlock without spinning
 *  @lock: lock to acquire
 *
 *  Attempts to acquire a spinlock that is otherwise acquired and released using 
 *  spin_lock_irq and spin_unlock_irq.  spin_trylock_irq does not spin if the
 *  lock is already held.  If the lock-acquire is successful, spin_trylock_irq
 *  disables interrupts on the local CPU.
 *
 *  SYNOPSIS:
 *     #define spin_trylock_irq(lock)
 *
 *  SEE ALSO:
 *  spin_unlock_irq, spin_trylock_irqsave
 *
 *  RETURN VALUE:
 *  TRUE if lock acquisition succeeded, FALSE if the lock was already held
 *
 */
/* _VMKLNX_CODECHECK_: spin_trylock_irq */
#define spin_trylock_irq(lock) \
({ \
	local_irq_disable(); \
	_spin_trylock(lock) ? \
	1 : ({ local_irq_enable(); 0;  }); \
})

/**
 *  spin_trylock_irqsave - Attempt to acquire an interrupt-context spinlock without spinning and save interrupt state if successful
 *  @lock: lock to acquire
 *  @flags: saved interrupt state
 *
 *  Attempts to acquire a spinlock that is otherwise acquired and released using 
 *  spin_lock_irqsave and spin_unlock_irqrestore.  spin_trylock_irqsave does not
 *  spin if the lock is already held.  If the lock-acquire is successful, 
 *  spin_trylock_irqsave stores the current interrupt state to flags and 
 *  disables interrupts on the local CPU.
 *
 *  SYNOPSIS:
 *     #define spin_trylock_irqsave(lock, flags)
 *
 *  SEE ALSO:
 *  spin_unlock_irqrestore
 *
 *  RETURN VALUE:
 *  TRUE if lock acquisition succeeded, FALSE if the lock was already held
 *
 */
/* _VMKLNX_CODECHECK_: spin_trylock_irqsave */
#define spin_trylock_irqsave(lock, flags)       \
({                                              \
        unsigned long rflags;                   \
	local_irq_save(rflags);                 \
	_spin_trylock(lock) ?                   \
	   ({(flags) = rflags; 1; }) :          \
	   ({ local_irq_restore(rflags); 0; }); \
})

/*
 * Pull the atomic_t declaration:
 * (asm-mips/atomic.h needs above definitions)
 */
#include <asm/atomic.h>
/**
 * atomic_dec_and_lock - lock on reaching reference count zero
 * @atomic: the atomic counter
 * @lock: the spinlock in question
 */
extern int _atomic_dec_and_lock(atomic_t *atomic, spinlock_t *lock);
#define atomic_dec_and_lock(atomic, lock) \
		__cond_lock(_atomic_dec_and_lock(atomic, lock))

/**
 * spin_can_lock - would spin_trylock() succeed?
 * @lock: the spinlock in question.
 */
#define spin_can_lock(lock)	(!spin_is_locked(lock))

#if defined(__VMKLNX__)
static inline int
vmklnx_spin_is_locked_by_my_cpu(spinlock_t *lock)
{
   return spin_is_locked(lock) && (lock->raw_lock.cpu == raw_smp_processor_id());
}
#endif /* defined(__VMKLNX__) */

#endif /* __LINUX_SPINLOCK_H */
