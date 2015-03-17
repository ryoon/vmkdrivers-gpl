/*
 * Portions Copyright 2009, 2010 VMware, Inc.
 */
#ifndef _LINUX_STRING_H_
#define _LINUX_STRING_H_

/* We don't want strings.h stuff being user by user stuff by accident */

#ifdef __KERNEL__

#include <linux/compiler.h>	/* for inline */
#include <linux/types.h>	/* for size_t */
#include <linux/stddef.h>	/* for NULL */

#if defined(__VMKLNX__)
#include "vmklinux_dist.h"
#include "vmkapi.h"
#endif /* defined(__VMKLNX__) */

#ifdef __cplusplus
extern "C" {
#endif

extern char *strndup_user(const char __user *, long);

/*
 * Include machine specific inline routines
 */
#include <asm/string.h>

#ifndef __HAVE_ARCH_STRCPY
#if defined(__VMKLNX__)
/**
 * strcpy - Copy %NUL terminated @src to @dest
 * @src: Source string to be copied
 * @dest: Destination where @src should be copied to
 *
 * This functions copies @src to @dest. This is essentially a wrapper for the
 * VMK API vmk_Strcpy().
 * Assumptions :
 *    - @src is NUL terminated
 *    - Enough memory is allocated at @dest
 */
/* _VMKLNX_CODECHECK_: strcpy */
static inline char *strcpy(char * dest, const char * src) 
{
   return vmk_Strcpy(dest, src);
}
#else /*!defined(__VMKLNX__) */
extern char * strcpy(char *,const char *);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRNCPY
#if defined(__VMKLNX__)
/**
 * strncpy - Copy no more than @maxlen bytes from %NUL terminated @src to @dest
 * @src: %NUL terminated Source string to copy from
 * @dest: Destination to copy to
 * @maxlen: Max # of bytes to copy from @src
 *
 * This functions copies no more than @maxlen bytes from %NUL terminated string
 * @src. Copy stops if a %NUL is hit before that.
 * This function is a wrapper for the VMK API vmk_Strncpy().
 */
/* _VMKLNX_CODECHECK_: strncpy */
static inline char * strncpy(char * dest, const char * src, vmk_ByteCount maxlen)
{
   return vmk_Strncpy(dest, src, maxlen);
}
#else /* !defined(__VMKLNX__) */
extern char * strncpy(char *,const char *, __kernel_size_t);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRLCPY
#if defined(__VMKLNX__)
static inline size_t strlcpy(char * dest, const char * src, size_t siz)
{
   return vmk_Strlcpy(dest, src, siz);
}
#else /* !defined(__VMKLNX__) */
size_t strlcpy(char *, const char *, size_t);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRCAT
extern char * strcat(char *, const char *);
#endif
#ifndef __HAVE_ARCH_STRNCAT
extern char * strncat(char *, const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRLCAT
extern size_t strlcat(char *, const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRCMP
#if defined(__VMKLNX__)
/**
 * strcmp - Compare %NUL terminated strings @s1 and @s2
 * @s1: First string
 * @s2: Second string
 *
 * This functions compares strings @s1 and @s2 to determine if they are the same
 * or different.
 * Return values:
 *    - 0 indicates same strings
 *    - > 0 indicates @s1[x] > @s2[x]
 *    - < 0 indicates @s1[x] < @s2[x]
 * This function is essentially a wrapper for the VMK API vmk_Strcmp().
 */
/* _VMKLNX_CODECHECK_: strcmp */
static inline int strcmp(const char * s1, const char * s2)
{
   return vmk_Strcmp(s1, s2);
}
#else /* !defined(__VMKLNX__) */
extern int strcmp(const char *,const char *);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRNCMP
#if defined(__VMKLNX__)
/**
 * strncmp - Compare a maximum of @maxlen bytes of strings @s1 and @s2
 * @s1: First string
 * @s2: Second string
 * @maxlen : Max # of bytes to compare
 *
 * This functions compares a max of @maxlen bytes of strings @s1 and @s2 to
 * determine if they are the same or different. Comparison stops if a %NUL is
 * encountered before @maxlen bytes.
 * This function is a wrapper for the VMK API vmk_Strncmp().
 * Return values are similar to that of strcmp().
 */
/* _VMKLNX_CODECHECK_: strncmp */
static inline int strncmp(const char * s1, const char * s2, vmk_ByteCount maxlen)
{
   return vmk_Strncmp(s1, s2, maxlen);
}
#else /* !defined(__VMKLNX__) */
extern int strncmp(const char *,const char *,__kernel_size_t);
#endif 
#endif /* defined(__VMKLNX__) */
#ifndef __HAVE_ARCH_STRNICMP
#if defined(__VMKLNX__)
/**
 * strnicmp - Compare a maximum of @n bytes of strings @s1 and @s2
 * @s1: First string
 * @s2: Second string
 * @n : Max # of bytes to compare
 *
 * This functions compares a max of @maxlen bytes of strings @s1 and @s2 to
 * determine if they are the same or different ignoring case.
 * Comparison stops if a %NUL is
 * encountered before @maxlen bytes.
 * return value :
 *    0 indicates strings match, 
 *    positive value indicates @s1[x] > @s2[x],
 *    negative value  indicates @s1[x] < @s2[x]
 */
/* _VMKLNX_CODECHECK_: strnicmp */
static inline int strnicmp(const char *s1, const char *s2, size_t n)
{
  return vmk_Strncasecmp(s1, s2, n);
}
#else /* !defined(__VMKLNX__)  */
extern int strnicmp(const char *, const char *, __kernel_size_t);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRCHR
#if defined(__VMKLNX__)
/**
 *  strchr - find character @c in string @s
 *  @s : input string
 *  @c : search character
 *
 *  return pointer to first instance of c within s.
 *
 *  RETURN VALUE:
 *  pointer to c or null
 *
 */
/* _VMKLNX_CODECHECK_: strchr */
static inline char * strchr(const char * s, int c)
{
   return vmk_Strchr(s, c);
}
#else /* !defined(__VMKLNX__) */
extern char * strchr(const char *,int);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRNCHR
extern char * strnchr(const char *, size_t, int);
#endif
#ifndef __HAVE_ARCH_STRRCHR
#if defined(__VMKLNX__)
static inline char * strrchr(const char *s, int c)
{
   return vmk_Strrchr(s, c);
}
#else /* !defined(__VMKLNX__) */
extern char * strrchr(const char *,int);
#endif /* defined(__VMKLNX__) */
#endif
extern char * strstrip(char *);
#ifndef __HAVE_ARCH_STRSTR
#if defined(__VMKLNX__)
/**
 *  strstr - search for @needle in @haystack
 *  @haystack: string to search in
 *  @needle: string to search for
 *
 *  returns pointer to instance of @needle within @haystack or
 *  null if none is found.
 *
 *  RETURN VALUE:
 *  pointer within @haystack or NULL
 *
 */
  /* _VMKLNX_CODECHECK_: strstr */
static inline char * strstr(const char * haystack, const char * needle)
{
   return vmk_Strstr(haystack, needle);
}
#else /* !defined(__VMKLNX__) */
extern char * strstr(const char *,const char *);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRLEN
extern __kernel_size_t strlen(const char *);
#endif
#ifndef __HAVE_ARCH_STRNLEN
#if defined(__VMKLNX__)
/**
 *  strnlen - get length of string
 *  @s: pointer to string
 *  @maxlen: limit of length
 *
 *  find length of string by searching for null terminator.
 *  stop search after maxlen characters
 *
 *  RETURN VALUE:
 *  length of string or maxlen
 *
 */
  /* _VMKLNX_CODECHECK_: strnlen */
static inline vmk_ByteCount strnlen(const char * s, vmk_ByteCount maxlen)
{
   return vmk_Strnlen(s, maxlen);
}
#else /* !defined(__VMKLNX__) */
extern __kernel_size_t strnlen(const char *,__kernel_size_t);
#endif /* defined(__VMKLNX__) */
#endif
#ifndef __HAVE_ARCH_STRPBRK
extern char * strpbrk(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRSEP
extern char * strsep(char **,const char *);
#endif
#ifndef __HAVE_ARCH_STRSPN
extern __kernel_size_t strspn(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRCSPN
extern __kernel_size_t strcspn(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_MEMSET
#if defined(__VMKLNX__)
static inline void * memset(void * dest, int byte, vmk_ByteCount len)
{
   return vmk_Memset(dest, byte, len);
}
#else /* !defined(__VMKLNX__) */
extern void * memset(void *,int,__kernel_size_t);
#endif
#endif /* defined(__VMKLNX__) */
#ifndef __HAVE_ARCH_MEMCPY
#if defined(__VMKLNX__)
/**                                          
 *  memcpy - copy bytes between two memory areas
 *  @dest: pointer to the destination area in memory
 *  @src: pointer to the source area in memory
 *  @len: the number of bytes to copy
 *                                           
 *  Copy @len number of bytes from @src to @dest.
 *                                           
 *  RETURN VALUE:
 *  @src
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: memcpy */
static inline void * memcpy(void * dest, const void * src, vmk_ByteCount len)
{
   return vmk_Memcpy(dest, src, len);
}
#else /* !defined(__VMKLNX__) */
extern void * memcpy(void *,const void *,__kernel_size_t);
#endif
#endif /* defined(__VMKLNX__) */
#ifndef __HAVE_ARCH_MEMMOVE
extern void * memmove(void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMSCAN
extern void * memscan(void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCMP
#if defined(__VMKLNX__)
/**
 * memcmp - Compare two areas of memory
 * @s1: pointer to one area of memory
 * @s2: pointer to another area of memory
 * @count: The size of the area.
 *
 * Compare the first @count bytes of the two memory regions pointed to by
 * @s1 and @s2.
 *
 * RETURN VALUE:
 * 0 if the content of the two memory regions are the same;
 * less than 0 if @s1 is less than @s2;
 * greater than 0 if @s1 is greater than @s2.
 */
/* _VMKLNX_CODECHECK_: memcmp */
static inline int memcmp(const void * s1, const void * s2, vmk_ByteCount count)
{
   return vmk_Memcmp(s1, s2, count);
}
#else
extern int memcmp(const void *,const void *,__kernel_size_t);
#endif 
#endif /* defined(__VMKLNX__) */
#ifndef __HAVE_ARCH_MEMCHR
extern void * memchr(const void *,int,__kernel_size_t);
#endif

#if defined(__VMKLNX__)
#define kstrdup(s, gfp) vmklnx_kstrdup(VMK_MODULE_HEAP_ID, s, 0)
#else
extern char *kstrdup(const char *s, gfp_t gfp);
#endif

#ifdef __cplusplus
}
#endif

#endif
#endif /* _LINUX_STRING_H_ */
