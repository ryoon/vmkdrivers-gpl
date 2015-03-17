#ifndef _X86_64_STRING_H_
#define _X86_64_STRING_H_

#ifdef __KERNEL__

#if defined(__VMKLNX__)
extern inline char *
#else /* !defined(__VMKLNX__) */
#define __HAVE_ARCH_STRCPY
static inline char *
#endif /* defined(__VMKLNX__) */
strcpy(char *dst, __const char *src)
{
   char *retval = dst;
   while((*dst++ = *src++)) {
      ;
   }
   return retval;
}

#if !defined(__VMKLNX__)
#define __HAVE_ARCH_STRNCMP
static inline int
strncmp(const char *s1, const char *s2, size_t count)
{
   int res = 0;

   while (count > 0) {
      res = *s1 - *s2;
      if (res != 0 || *s1 == 0) {
         break;
      }
      count--;
      s1++;
      s2++;
   }

   return res;
}

#define __HAVE_ARCH_STRCAT
static inline char *
strcat(char *dst, __const char *src)
{
   char *ptr=dst;
   
   for (ptr=dst;*ptr != '\0'; ptr++) {
      continue;
   }
   for ( ; *src!='\0'; ptr++,src++) {
      *ptr = *src;
   }
   *ptr = '\0';
   return dst;
}

#define __HAVE_ARCH_STRLEN
static inline size_t strlen(const char * s)
{
int d0;
register int __res;
__asm__ __volatile__(
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res), "=&D" (d0) :"1" (s),"a" (0), "0" (0xffffffffffffffff));
return __res;
}

#define __HAVE_ARCH_STRSTR
extern char *strstr(const char *cs, const char *ct);

/* Written 2002 by Andi Kleen */ 

/* Only used for special circumstances. Stolen from i386/string.h */ 
static __always_inline void *
__inline_memcpy(void * to, const void * from, size_t n)
{
unsigned long d0, d1, d2;
__asm__ __volatile__(
	"rep ; movsl\n\t"
	"testb $2,%b4\n\t"
	"je 1f\n\t"
	"movsw\n"
	"1:\ttestb $1,%b4\n\t"
	"je 2f\n\t"
	"movsb\n"
	"2:"
	: "=&c" (d0), "=&D" (d1), "=&S" (d2)
	:"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
	: "memory");
return (to);
}

/* Even with __builtin_ the compiler may decide to use the out of line
   function. */

#define __HAVE_ARCH_MEMCPY 1
extern void *__memcpy(void *to, const void *from, size_t len); 
#define memcpy(dst,src,len) \
	({ size_t __len = (len);				\
	   void *__ret;						\
	   if (__builtin_constant_p(len) && __len >= 64)	\
		 __ret = __memcpy((dst),(src),__len);		\
	   else							\
		 __ret = __builtin_memcpy((dst),(src),__len);	\
	   __ret; }) 


#define __HAVE_ARCH_MEMSET
void *memset(void *s, int c, size_t n);
#endif /* !defined(__VMKLNX__) */

#define __HAVE_ARCH_MEMMOVE
void * memmove(void * dest,const void *src,size_t count);

#if !defined(__VMKLNX__)
int memcmp(const void * cs,const void * ct,size_t count);
#endif /* !defined(__VMKLNX__) */
size_t strlen(const char * s);
char *strcpy(char * dest,const char *src);
char *strcat(char * dest, const char * src);
#if !defined(__VMKLNX__)
int strcmp(const char * cs,const char * ct);
#endif /* !defined(__VMKLNX__) */
#endif /* __KERNEL__ */

#endif
