/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
#ifndef _ASM_GENERIC_BUG_H
#define _ASM_GENERIC_BUG_H

#include <linux/compiler.h>

#if defined(__VMKLNX__)
#include <linux/stringify.h>

/**
 *  print_tainted - return "inside vmklinux" for use with BUG() and WARN_ON()
 *
 *  Used by vmklinux to provide some information if a BUG() or WARN_ON() is
 *  invoked in vmklinux context.
 *  
 *  ESX Deviation Notes:                     
 *  Always returns "inside vmklinux"
 *                                           
 *  RETURN VALUE:
 *  Always returns "inside vmklinux"
 */                                          
/* _VMKLNX_CODECHECK_: print_tainted */
static inline const char *print_tainted(void)
{
   return "inside vmklinux";
} 

/**
 *  dump_stack - log the current stack backtrace to the vmkernel log
 *
 *  Log the current stack backtrace to the vmkernel log.
 *
 *  RETURN VALUE:
 *  None.
 */                                          
/* _VMKLNX_CODECHECK_: dump_stack */
static inline void dump_stack(void)
{
   vmk_LogBacktraceMessage();
}

#else
#ifndef __ASSEMBLY__
extern const char *print_tainted(void);
#endif
#endif //__VMKLNX__

#ifdef CONFIG_BUG
#ifndef HAVE_ARCH_BUG
#if defined(__VMKLNX__)
/**
 * BUG - Prints a message
 *
 * Prints a log message
 *
 * SYNOPSIS:
 * #define BUG()
 *
 * ESX Deviation Notes:
 * Generates a system stop (panic).
 *
 * RETURN VALUE:
 * None
 *
 */
/* _VMKLNX_CODECHECK_: BUG */
#define BUG() do {                                                      \
	printk("BUG: failure at %s:%d/%s()! (%s)\n", __FILE__, __LINE__, __FUNCTION__, print_tainted()); \
	panic("BUG: " __FILE__ ":" __stringify(__LINE__));              \
} while (0)
#else /* !defined(__VMKLNX__) */
#define BUG() do { \
	printk("BUG: failure at %s:%d/%s()! (%s)\n", __FILE__, __LINE__, __FUNCTION__, print_tainted()); \
	panic("BUG!"); \
} while (0)
#endif /* defined(__VMKLNX__) */
#endif

#ifndef HAVE_ARCH_BUG_ON
/**                                          
 *  BUG_ON - Report bug if condition happens
 *  @condition: Condition to check
 *                                           
 *  It reports a bug for a condition which is never
 *  supposed to happen. Also provides the location in
 *  the code where the bug was hit.
 *                                           
 *  SYNOPSIS:
 *  #define BUG_ON(condition)
 *
 *  ESX Deviation Notes:
 *  As in x86_64 most variants of Linux, generates a 
 *  system stop (panic).
 *
 *  RETURN VALUE:
 *  None
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: BUG_ON */
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif

#ifndef HAVE_ARCH_WARN_ON
/**
 *  WARN_ON - print a warning message
 *  @condition: a condition
 *
 *  Generate a warning message if the given condition is TRUE.
 *
 *  SYNOPSIS:
 *  #define WARN_ON(condition) 
 *
 *  RETURN VALUE:
 *  1 if the given condition is TRUE, 0 otherwise.
 */
/* _VMKLNX_CODECHECK_: WARN_ON */
#define WARN_ON(condition) ({				\
	int __ret_warn_on = !!(condition);		\
	if (unlikely(__ret_warn_on)) {			\
		printk("WARNING: at %s:%d/%s() (%s)\n", __FILE__, __LINE__, __FUNCTION__, print_tainted()); \
		dump_stack(); 				\
	}						\
	unlikely(__ret_warn_on);			\
})
#endif

#else /* !CONFIG_BUG */
#ifndef HAVE_ARCH_BUG
#define BUG()
#endif

#ifndef HAVE_ARCH_BUG_ON
#define BUG_ON(condition) do { if (condition) ; } while(0)
#endif

#ifndef HAVE_ARCH_WARN_ON
#define WARN_ON(condition) do { if (condition) ; } while(0)
#endif
#endif

#define WARN_ON_ONCE(condition)				\
({							\
	static int __warn_once = 1;			\
	int __ret = 0;					\
							\
	if (unlikely((condition) && __warn_once)) {	\
		__warn_once = 0;			\
		WARN_ON(1);				\
		__ret = 1;				\
	}						\
	__ret;						\
})

#ifdef CONFIG_SMP
# define WARN_ON_SMP(x)			WARN_ON(x)
#else
# define WARN_ON_SMP(x)			do { } while (0)
#endif

#endif
