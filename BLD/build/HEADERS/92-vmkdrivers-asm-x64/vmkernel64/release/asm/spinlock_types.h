/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

#if defined(__VMKLNX__)
#define SPINLOCK_VMKERNEL_CPU_INVALID 0xbadc0ded
#define SPINLOCK_VMKERNEL_INIT , SPINLOCK_VMKERNEL_CPU_INVALID, 0, 0
                                                                                                          
typedef struct {
        volatile unsigned int slock;
        unsigned int          cpu;
        unsigned long         ra;
        unsigned long         flags;
} raw_spinlock_t;
                                                                                                          
typedef struct {
        volatile unsigned int lock;
        unsigned int          cpu;
        unsigned long         ra;
        unsigned long         flags;
} raw_rwlock_t;
                                                                                                          
#define __RAW_SPIN_LOCK_UNLOCKED { 1 SPINLOCK_VMKERNEL_INIT }
#define __RAW_RW_LOCK_UNLOCKED   { RW_LOCK_BIAS SPINLOCK_VMKERNEL_INIT }
                                                                                                          
#else /* !defined(__VMKLNX__) */


typedef struct {
	volatile unsigned int slock;
} raw_spinlock_t;

#define __RAW_SPIN_LOCK_UNLOCKED	{ 1 }

typedef struct {
	volatile unsigned int lock;
} raw_rwlock_t;

#define __RAW_RW_LOCK_UNLOCKED		{ RW_LOCK_BIAS }

#endif /* defined(__VMKLNX__) */

#endif
