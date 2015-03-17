#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H

#include <linux/compiler.h>

#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif

#ifdef __KERNEL__

#if defined(__VMKLNX__)
/* 2010: update from linux source */
enum {
	false	= 0,
	true	= 1
};
#endif /* defined(__VMKLNX__) */

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
/**
 *  offsetof - Calculates the offset of a member element within a structure
 *  @TYPE: structure type
 *  @MEMBER: member of structure
 *
 *  Calculates the offset of a member element within a structure
 *
 *  SYNOPSIS:
 *      #define offsetof(TYPE, MEMBER)
 *
 *  RETURN VALUE:
 *  Returns the offset of the member field 
 *
 */
/* _VMKLNX_CODECHECK_: offsetof */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif /* __KERNEL__ */

#endif
