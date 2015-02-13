#ifndef _LINUX_ERR_H
#define _LINUX_ERR_H

#include <linux/compiler.h>

#include <asm/errno.h>

/*
 * Kernel pointers have redundant information, so we can use a
 * scheme where we can return either an error code or a dentry
 * pointer with the same return value.
 *
 * This should be a per-architecture thing, to allow different
 * error and pointer decisions.
 */
#define MAX_ERRNO	4095

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

/**                                          
 *  ERR_PTR - returns the given error long as a pointer
 *  @error: error number
 *                                           
 *  Returns the given error long as a pointer
 *                                           
 *  RETURN VALUE:
 *  The given error cast as a void pointer
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: ERR_PTR */
static inline void *ERR_PTR(long error)
{
	return (void *) error;
}

/**
 *  PTR_ERR - Return error code given a pointer
 *  @ptr: the pointer
 *
 *  Kernel pointers have redundant information, so we can use a scheme where we
 *  can return either an error code or a struct dentry pointer with the same return
 *  value.
 *
 *  This should be a per-architecture thing, to allow different error and
 *  pointer decisions. [From linux/err.h]
 *
 *  RETURN VALUE:
 *  Returns the error code corresponding to the pointer.
 *
 */
/* _VMKLNX_CODECHECK_: PTR_ERR */
static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

/**                                          
 *  IS_ERR - Check if pointer is valid or if it has an error encoded in it       
 *  @ptr: Pointer to check    
 *                                           
 *  Checks if pointer is valid or if it has an error encoded in it.                      
 *                                           
 *  RETURN VALUE:
 *  Non-zero if the given pointer is an error value.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: IS_ERR */
static inline long IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

#endif /* _LINUX_ERR_H */
