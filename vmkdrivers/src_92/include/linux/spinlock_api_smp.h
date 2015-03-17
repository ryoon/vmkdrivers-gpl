/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef __LINUX_SPINLOCK_API_SMP_H
#define __LINUX_SPINLOCK_API_SMP_H

#ifndef __LINUX_SPINLOCK_H
# error "please don't include this file directly"
#endif

/*
 * include/linux/spinlock_api_smp.h
 *
 * spinlock API declarations on SMP (and debug)
 * (implemented in kernel/spinlock.c)
 *
 * portions Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 */
#if defined(__VMKLNX__)
/*
 * The xxx_irq functions are not safe to use in the vmkernel, since they blindly
 * clear and set IF. We convert the spinlock ones into spinlock_irq{save,restore}
 * internally and disable the read/write lock ones.
 *
 * The xxx_bh functions don't need to do anything with bottom halves, since all the
 * call sites will be protected from bottom halves running by one of the following:
 *
 *     - caller is an interrupt handler
 *     - caller is a bottom half itself
 *     - caller is a dev function like hard_start_xmit, in which
 *       case SCHED_CURRENT_WORLD->inVMKernel is TRUE and bottom
 *       halves won't run.
 */

/**
 *  assert_spin_locked - Assert if the spin lock is not locked
 *  @x: the pointer to spin lock
 *
 *  Assert if the spin lock is not locked.
 *
 *  SYNOPSIS:
 *  #define assert_spin_locked(x)
 *
 *  RETURN VALUE:
 *  Zero if spin lock is locked, otherwise panic.
 *
 */
 /* _VMKLNX_CODECHECK_: assert_spin_locked */
#define assert_spin_locked(x)	 BUG_ON(!spin_is_locked(x))

#define _spin_trylock(lock)      __raw_spin_trylock(&(lock)->raw_lock)
#define _read_trylock(lock)      __raw_read_trylock(&(lock)->raw_lock)
#define _write_trylock(lock)     __raw_write_trylock(&(lock)->raw_lock)

#define _spin_lock(lock)         __raw_spin_lock(&(lock)->raw_lock)
#define _read_lock(lock)         __raw_read_lock(&(lock)->raw_lock)
#define _write_lock(lock)        __raw_write_lock(&(lock)->raw_lock)

#define _spin_unlock(lock)       __raw_spin_unlock(&(lock)->raw_lock)
#define _read_unlock(lock)       __raw_read_unlock(&(lock)->raw_lock)
#define _write_unlock(lock)      __raw_write_unlock(&(lock)->raw_lock)

#define _spin_lock_irq(lock)     _spin_lock_irqsave(lock)
#define _spin_lock_bh(lock)      do { __raw_spin_lock(&(lock)->raw_lock); } while (0)
#define _read_lock_irq(lock)     _read_lock_irqsave(lock)
#define _read_lock_bh(lock)      do { __raw_read_lock(&(lock)->raw_lock); } while (0);
#define _write_lock_irq(lock)    _write_lock_irqsave(lock)
#define _write_lock_bh(lock)     do { __raw_write_lock(&(lock)->raw_lock); } while (0)

#define _spin_unlock_irqrestore(lock, flags)                                                     \
                                 do {                                                            \
                                    unsigned long rflags = (flags);                              \
                                    __raw_spin_unlock(&(lock)->raw_lock);                        \
                                    raw_local_irq_restore(rflags);                               \
				 } while (0)
#define _spin_unlock_irq(lock)   do {                                                            \
                                    unsigned long rflags = (lock)->raw_lock.flags;               \
                                    __raw_spin_unlock(&(lock)->raw_lock);                        \
				    raw_local_irq_restore(rflags);                               \
                                 } while (0)
#define _spin_unlock_bh(lock)    do { __raw_spin_unlock(&(lock)->raw_lock); } while (0)
#define _read_unlock_irqrestore(lock, flags)                                                     \
                                 do {                                                            \
                                    unsigned long rflags = (flags);                              \
				    __raw_read_unlock(&(lock)->raw_lock);                        \
				    raw_local_irq_restore(rflags);                               \
				 } while (0)
#define _read_unlock_irq(lock)   do {                                                            \
                                    unsigned long flags = (lock)->raw_lock.flags;                \
                                    _read_unlock_irqrestore(lock, flags);                        \
                                 } while (0)
#define _read_unlock_bh(lock)    do { __raw_read_unlock(&(lock)->raw_lock); } while (0)
#define _write_unlock_irqrestore(lock, flags)                                                    \
                                 do {                                                            \
                                    unsigned long rflags = (flags);                              \
				    __raw_write_unlock(&(lock)->raw_lock);                       \
				    raw_local_irq_restore(rflags);                               \
			         } while (0)
#define _write_unlock_irq(lock)  do {                                                            \
                                    unsigned long flags = (lock)->raw_lock.flags;                \
                                    _write_unlock_irqrestore(lock, flags);                       \
                                 } while (0)
#define _write_unlock_bh(lock)   do { __raw_write_unlock(&(lock)->raw_lock); } while (0)

/**                                          
 *  _spin_lock_irqsave - Grab the spin lock, @lock
 *  @lock: Pointer to the spin lock to grab
 *                                           
 *  Grab the passed in spin lock and save the current flags
 *
 *  RETURN VALUE:
 *    The value of flags that was saved
 */                                          
/* _VMKLNX_CODECHECK_: _spin_lock_irqsave */
static inline unsigned long
_spin_lock_irqsave(spinlock_t *lock)
{
        unsigned long flags;
        flags = __raw_local_irq_save();
	__raw_spin_lock_flags(&(lock)->raw_lock, flags);
        (lock)->raw_lock.flags = flags;
        return flags;
}

static inline unsigned long
_read_lock_irqsave(rwlock_t *lock)
{
        unsigned long flags;
        flags = __raw_local_irq_save();
	__raw_read_lock(&(lock)->raw_lock); 
        (lock)->raw_lock.flags = flags;
        return flags;
}

static inline unsigned long
_write_lock_irqsave(rwlock_t *lock)
{
        unsigned long flags;
	flags = __raw_local_irq_save();
	__raw_write_lock(&(lock)->raw_lock);
        (lock)->raw_lock.flags = flags;
        return flags;
}


#else // !defined(__VMKLNX__)

int in_lock_functions(unsigned long addr);

#define assert_spin_locked(x)	BUG_ON(!spin_is_locked(x))

void __lockfunc _spin_lock(spinlock_t *lock)		__acquires(spinlock_t);
void __lockfunc _spin_lock_nested(spinlock_t *lock, int subclass)
							__acquires(spinlock_t);
void __lockfunc _read_lock(rwlock_t *lock)		__acquires(rwlock_t);
void __lockfunc _write_lock(rwlock_t *lock)		__acquires(rwlock_t);
void __lockfunc _spin_lock_bh(spinlock_t *lock)		__acquires(spinlock_t);
void __lockfunc _read_lock_bh(rwlock_t *lock)		__acquires(rwlock_t);
void __lockfunc _write_lock_bh(rwlock_t *lock)		__acquires(rwlock_t);
void __lockfunc _spin_lock_irq(spinlock_t *lock)	__acquires(spinlock_t);
void __lockfunc _read_lock_irq(rwlock_t *lock)		__acquires(rwlock_t);
void __lockfunc _write_lock_irq(rwlock_t *lock)		__acquires(rwlock_t);
unsigned long __lockfunc _spin_lock_irqsave(spinlock_t *lock)
							__acquires(spinlock_t);
unsigned long __lockfunc _read_lock_irqsave(rwlock_t *lock)
							__acquires(rwlock_t);
unsigned long __lockfunc _write_lock_irqsave(rwlock_t *lock)
							__acquires(rwlock_t);
int __lockfunc _spin_trylock(spinlock_t *lock);
int __lockfunc _read_trylock(rwlock_t *lock);
int __lockfunc _write_trylock(rwlock_t *lock);
int __lockfunc _spin_trylock_bh(spinlock_t *lock);
void __lockfunc _spin_unlock(spinlock_t *lock)		__releases(spinlock_t);
void __lockfunc _read_unlock(rwlock_t *lock)		__releases(rwlock_t);
void __lockfunc _write_unlock(rwlock_t *lock)		__releases(rwlock_t);
void __lockfunc _spin_unlock_bh(spinlock_t *lock)	__releases(spinlock_t);
void __lockfunc _read_unlock_bh(rwlock_t *lock)		__releases(rwlock_t);
void __lockfunc _write_unlock_bh(rwlock_t *lock)	__releases(rwlock_t);
void __lockfunc _spin_unlock_irq(spinlock_t *lock)	__releases(spinlock_t);
void __lockfunc _read_unlock_irq(rwlock_t *lock)	__releases(rwlock_t);
void __lockfunc _write_unlock_irq(rwlock_t *lock)	__releases(rwlock_t);
void __lockfunc _spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
							__releases(spinlock_t);
void __lockfunc _read_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
							__releases(rwlock_t);
void __lockfunc _write_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
							__releases(rwlock_t);
#endif /* __VMKLNX__ */

#endif /* __LINUX_SPINLOCK_API_SMP_H */
