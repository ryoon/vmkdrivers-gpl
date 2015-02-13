/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
	/*
	 * Save registers before calling assembly functions. This avoids
	 * disturbance of register allocation in some inline assembly constructs.
	 * Copyright 2001,2002 by Andi Kleen, SuSE Labs.
	 * Subject to the GNU public license, v.2. No warranty of any kind.
	 * $Id: thunk.S,v 1.2 2002/03/13 20:06:58 ak Exp $
	 */


	#if defined(__VMKLNX__)
	/* Define this directive to quiet a warning from dwarf2.h */
	#define __ASSEMBLY__
	#endif /* defined(__VMKLNX__) */

	#if !defined(__VMKLNX__)
	/* 
	 * Skip this header file since config.h is no longer used and
	 * including it will cause gcc to warn.
	 */
	#include <linux/config.h>
	#endif /* !defined(__VMKLNX__) */
	#include <linux/linkage.h>
	#include <asm/dwarf2.h>
	#include <asm/calling.h>			
	#include <asm/rwlock.h>

	/* rdi:	arg1 ... normal C conventions. rax is saved/restored. */ 	
	.macro thunk name,func
	.globl \name
#if defined(__VMKLNX__)
	/*
	 * This ensures that the symbol is marked as a function and
         * not accidently thrown away by our symbol export logic.
	 */
	.type \name STT_FUNC
#endif
\name:	
	CFI_STARTPROC
	SAVE_ARGS
	call \func
	jmp  restore
	CFI_ENDPROC
	.endm

	/* rdi:	arg1 ... normal C conventions. rax is passed from C. */ 	
	.macro thunk_retrax name,func
	.globl \name
#if defined(__VMKLNX__)
	/* as for above ... */
	.type \name STT_FUNC
#endif	
\name:	
	CFI_STARTPROC
	SAVE_ARGS
	call \func
	jmp  restore_norax
	CFI_ENDPROC
	.endm
	

#if defined(__VMKLNX__)
	.section .text
#else /* !defined(__VMKLNX__) */
	.section .sched.text
#ifdef CONFIG_RWSEM_XCHGADD_ALGORITHM
	thunk rwsem_down_read_failed_thunk,rwsem_down_read_failed
	thunk rwsem_down_write_failed_thunk,rwsem_down_write_failed
	thunk rwsem_wake_thunk,rwsem_wake
	thunk rwsem_downgrade_thunk,rwsem_downgrade_wake
#endif	
#endif /* !defined(__VMKLNX__) */
	
	thunk __down_failed,__down
	thunk_retrax __down_failed_interruptible,__down_interruptible
	thunk_retrax __down_failed_trylock,__down_trylock
	thunk __up_wakeup,__up

#if !defined(__VMKLNX__)
#ifdef CONFIG_TRACE_IRQFLAGS
	thunk trace_hardirqs_on_thunk,trace_hardirqs_on
	thunk trace_hardirqs_off_thunk,trace_hardirqs_off
#endif
#endif /* !defined(__VMKLNX__) */
	
	/* SAVE_ARGS below is used only for the .cfi directives it contains. */
	CFI_STARTPROC
	SAVE_ARGS
restore:
	RESTORE_ARGS
	ret	
	CFI_ENDPROC
	
	CFI_STARTPROC
	SAVE_ARGS
restore_norax:	
	RESTORE_ARGS 1
	ret
	CFI_ENDPROC

#if defined(__VMKLNX__)
        /* Using ordinary C calling convention: pointer to rwlock in %rdi */
        .section .text
        .globl __write_lock_failed
	.type __write_lock_failed STT_FUNC
__write_lock_failed:
	lock
	addl $RW_LOCK_BIAS,(%rdi)
        cmpb $0, vmk_AtomicUseFence(%rip)
        je 1f
        lfence
1:	rep
	nop
	cmpl $RW_LOCK_BIAS,(%rdi)
	jne 1b
	lock 
	subl $RW_LOCK_BIAS,(%rdi)
	jnz  3f
        cmpb $0, vmk_AtomicUseFence(%rip)
        je 2f
        lfence
2:      ret
3:      cmpb $0, vmk_AtomicUseFence(%rip)
        je __write_lock_failed
        lfence
        jmp __write_lock_failed

        /* Using ordinary C calling convention: pointer to rwlock in %rdi */
        .section .text
        .globl __read_lock_failed
	.type __read_lock_failed STT_FUNC
__read_lock_failed:
	lock
	incl (%rdi)
        cmpb $0, vmk_AtomicUseFence(%rip)
        je 1f
        lfence
1:	rep
	nop
	cmpl $1,(%rdi)
	js 1b
	lock
	decl (%rdi)
	js 3f
        cmpb $0, vmk_AtomicUseFence(%rip)
        je 2f
        lfence
2:      ret
3:      cmpb $0, vmk_AtomicUseFence(%rip)
        je __read_lock_failed
        lfence
        jmp __read_lock_failed

#else /* !defined(__VMKLNX__) */
#ifdef CONFIG_SMP
/* Support for read/write spinlocks. */
	.text
/* rax:	pointer to rwlock_t */	
ENTRY(__write_lock_failed)
	lock
	addl $RW_LOCK_BIAS,(%rax)
1:	rep
	nop
	cmpl $RW_LOCK_BIAS,(%rax)
	jne 1b
	lock 
	subl $RW_LOCK_BIAS,(%rax)
	jnz  __write_lock_failed
	ret

/* rax:	pointer to rwlock_t */	
ENTRY(__read_lock_failed)
	lock
	incl (%rax)
1:	rep
	nop
	cmpl $1,(%rax)
	js 1b
	lock
	decl (%rax)
	js __read_lock_failed
	ret
#endif
#endif /* !defined(__VMKLNX__) */

#if defined(__VMKLNX__)
        /* Using ordinary C calling convention: pointer to raw_spin_lock in %rdi */
        .section .text
        .globl __raw_spin_failed
	.type __raw_spin_failed STT_FUNC
ENTRY(__raw_spin_failed)
1:      rep
        nop
	cmpl $0, (%rdi)
	jle 1b
	lock
        decl (%rdi)
	js 1b
	ret
#endif /* defined(__VMKLNX__) */
