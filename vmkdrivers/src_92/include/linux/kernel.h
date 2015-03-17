/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

/*
 * 'kernel.h' contains some often-used function prototypes etc
 */

#ifdef __KERNEL__

#include <stdarg.h>
#include <linux/linkage.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/bitops.h>
#include <asm/byteorder.h>
#include <asm/bug.h>

#if defined (__VMKLNX__)
#include "vmkapi.h"
#endif /* defined(__VMKLNX__) */

extern const char linux_banner[];

#define INT_MAX		((int)(~0U>>1))
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX	(~0U)
#define LONG_MAX	((long)(~0UL>>1))
#define LONG_MIN	(-LONG_MAX - 1)
#define ULONG_MAX	(~0UL)
#define LLONG_MAX	((long long)(~0ULL>>1))
#define LLONG_MIN	(-LLONG_MAX - 1)
#define ULLONG_MAX	(~0ULL)

#define STACK_MAGIC	0xdeadbeef

/**
 *  ARRAY_SIZE - return the size of an array
 *  @x: The array variable (not pointer)
 *
 *  Return the size of an array (the number of elements in an array).
 *
 *  SYNOPSIS:
 *  #define ARRAY_SIZE(x)
 *
 *  RETURN VALUE:
 *  The size of the array.
 *
 **/
/* _VMKLNX_CODECHECK_: ARRAY_SIZE */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/**
 *  ALIGN - Aligns @x to next multiple of @a
 *  @x:  address to be aligned 
 *  @a: alignment constraint (power of 2)
 *
 *  Aligns @x to next multiple of @a
 *
 *  SYNOPSIS:
 *  #define ALIGN(x,a)
 *
 *  RETURN VALUE:
 *  Returns aligned result.
 *
 **/
/* _VMKLNX_CODECHECK_: ALIGN */
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

/**
 *  DIV_ROUND_UP - Round up a number and divide
 *  @n: dividend
 *  @d: divisor
 *
 *  Get the quotient from dividing the dividend rounded to
 *  the next higher multiple of the divisor.
 *
 *  SYNOPSIS:
 *  #define DIV_ROUND_UP(n,d)
 *
 *  RETURN VALUE:
 *  NONE
 *
 **/
/* _VMKLNX_CODECHECK_: DIV_ROUND_UP */
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/**
 *  roundup - round a number
 *  @x: the number to be rounded
 *  @y: the number by which it should be rounded
 *
 *  A macro that rounds @x to the nearest higher multiple of @y.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 */
/* _VMKLNX_CODECHECK_: roundup */
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))

#if defined(__VMKLNX__)
/* 2010: update from linux source */

/**
 * upper_32_bits - return bits 32-63 of a number
 * @n: the number we're accessing
 *
 * A basic shift-right of a 64- or 32-bit quantity.  Use this to suppress
 * the "right shift count >= width of type" warning when that quantity is
 * 32-bits.
 */
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) ((u32)(n))
#endif /* defined(__VMKLNX__) */

#define	KERN_EMERG	"<0>"	/* system is unusable			*/
#define	KERN_ALERT	"<1>"	/* action must be taken immediately	*/
#define	KERN_CRIT	"<2>"	/* critical conditions			*/
#define	KERN_ERR	"<3>"	/* error conditions			*/
#define	KERN_WARNING	"<4>"	/* warning conditions			*/
#define	KERN_NOTICE	"<5>"	/* normal but significant condition	*/
#define	KERN_INFO	"<6>"	/* informational			*/
#define	KERN_DEBUG	"<7>"	/* debug-level messages			*/
#if defined(__VMKLNX__)
#define	VMKLNX_KERN_ALERT	"<alert>"       /* alert messages to be printed to ESXi console */
#endif

/*
 * Annotation for a "continued" line of log printout (only done after a
 * line that had no enclosing \n). Only to be used by core/arch code
 * during early bootup (a continued line is not SMP-safe otherwise).
 */
#define KERN_CONT       ""

extern int console_printk[];

#define console_loglevel (console_printk[0])
#define default_message_loglevel (console_printk[1])
#define minimum_console_loglevel (console_printk[2])
#define default_console_loglevel (console_printk[3])

#if defined(__VMKLNX__)
//Throttled logging for vmklinux modules
#define _VMKLNX_PRINTK_THROTTLED_BASE_PROLOGUE			\
	    do {						\
		static unsigned _count=0;			\
		static unsigned _limit=1;
#define _VMKLNX_PRINTK_THROTTLED_PROLOGUE			\
		_VMKLNX_PRINTK_THROTTLED_BASE_PROLOGUE		\
		if (unlikely(++_count == _limit)) {		\
		    _limit <<= 1;				\
		    if (!_limit) {				\
			_limit = _count;			\
		    }
#define _VMKLNX_PRINTK_DELAY_THROTTLED_PROLOGUE			\
		_VMKLNX_PRINTK_THROTTLED_BASE_PROLOGUE		\
		static unsigned _delay=100;			\
		static unsigned _max_throttle=1024;		\
		++_count;					\
		if (likely(_delay || _count == _limit)) {	\
		    if (likely(_delay)) {			\
			--_delay;				\
		    } else if (_limit < _max_throttle) {	\
			 _limit <<= 1;				\
		    }
#define _VMKLNX_PRINTK_THROTTLED_EPILOGUE			\
		    _count = 0;					\
		}						\
	    } while (0)
#endif /* __VMKLNX__ */
										
struct completion;
struct pt_regs;
struct user;

/**
 * might_sleep - annotation for functions that can sleep
 *
 * this macro will print a stack trace if it is executed in an atomic
 * context (spinlock, irq-handler, ...).
 *
 * This is a useful debugging help to be able to catch problems early and not
 * be biten later when the calling function happens to sleep when it is not
 * supposed to.
 */
#ifdef CONFIG_PREEMPT_VOLUNTARY
extern int cond_resched(void);
# define might_resched() cond_resched()
#else
# define might_resched() do { } while (0)
#endif

#ifdef CONFIG_DEBUG_SPINLOCK_SLEEP
  void __might_sleep(char *file, int line);
# define might_sleep() \
	do { __might_sleep(__FILE__, __LINE__); might_resched(); } while (0)
#else
# define might_sleep() do { might_resched(); } while (0)
#endif

#define might_sleep_if(cond) do { if (cond) might_sleep(); } while (0)

/**
 *  abs - Returns absolute value for given integer 
 *  @x: integer  
 *  
 *  Returns absolute value for given integer
 *
 *  SYNOPSIS:
 *      #define abs(x)
 *
 *  RETURN VALUE:
 *  Returns absolute value for given integer
 *
 */
/* _VMKLNX_CODECHECK_: abs */
#define abs(x) ({				\
		int __x = (x);			\
		(__x < 0) ? -__x : __x;		\
	})

#define labs(x) ({				\
		long __x = (x);			\
		(__x < 0) ? -__x : __x;		\
	})

extern struct atomic_notifier_head panic_notifier_list;
extern long (*panic_blink)(long time);
#if defined(__VMKLNX__)
/**
 *  panic - halt the system
 *  @fmt: The text string to print
 *
 *  Display a message, then perform cleanups
 *
 *  SYNOPSIS:
 *  #define panic(fmt, args...)
 *
 *  RETURN VALUE:
 *  This function never returns
 */
/* _VMKLNX_CODECHECK_: panic */
#define panic(fmt, args...) vmk_PanicWithModuleID(vmk_ModuleCurrentID, fmt, ##args)
#else /* !defined(__VMKLNX__) */
NORET_TYPE void panic(const char * fmt, ...)
	__attribute__ ((NORET_AND format (printf, 1, 2)));
#endif /* defined(__VMKLNX__) */
extern void oops_enter(void);
extern void oops_exit(void);
extern int oops_may_print(void);
fastcall NORET_TYPE void do_exit(long error_code)
	ATTRIB_NORET;
#if defined(__VMKERNEL__)
void complete_and_exit(struct completion *, long);
#else /* !defined(__VMKERNEL__) */
NORET_TYPE void complete_and_exit(struct completion *, long)
	ATTRIB_NORET;
#endif /* defined(__VMKERNEL__) */
extern unsigned long simple_strtoul(const char *,char **,unsigned int);
extern long simple_strtol(const char *,char **,unsigned int);
extern unsigned long long simple_strtoull(const char *,char **,unsigned int);
extern long long simple_strtoll(const char *,char **,unsigned int);
#if defined(__VMKLNX__)
/**                                          
 *  sprintf - Output a formatted string to a character buffer       
 *
 *  sprintf takes a format string and arguments and produces a null-terminated
 *  character string corresponding to the provided format specification and 
 *  supplementary data arguments.  The interface to sprintf in ESX is nearly 
 *  identical to the C Library standard (see deviation notes), and the ESX
 *  version supports the same formatting strings.
 *
 *  ARGUMENTS:
 *     buf - Output buffer     
 *     fmt - Format string for output data
 *
 *  SYNOPSIS:
 *     int sprintf(char * buf, const char * fmt, ...)
 *
 *  RETURN VALUE:
 *  The number of characters written to buf, not including the terminating NULL
 *
 *  ESX Deviation Notes:
 *  sprintf on ESX always returns the number of characters written, but the 
 *  standard C Library returns a negative value on an error.  If an error
 *  is encountered on ESX, 0 will be returned. 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: sprintf */
#define sprintf vmk_Sprintf
#else /* !defined(__VMKLNX__) */
extern int sprintf(char * buf, const char * fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
#endif /* defined(__VMKLNX__) */
#if defined(__VMKLNX__)
/**                                          
 *  vsprintf - format a string and place it in a buffer
 *  @buf: the buffer to place the result into
 *  @format: the format string to use
 *  @ap: the argument for the format string
 *                                           
 *  Format a string and put it into the given buffer. 
 *                                           
 *  RETURN VALUE:                     
 *  The number of characters is written into the buffer. 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: vsprintf */
static inline int vsprintf(char *buf, const char *format, va_list ap)
{
   return vmk_Vsprintf(buf, format, ap);
}
#else /* !defined(__VMKLNX__) */
extern int vsprintf(char *buf, const char *, va_list)
	__attribute__ ((format (printf, 2, 0)));
#endif /* defined(__VMKLNX__) */

/**                                          
 *  snprintf - Print the message to the buffer
 *  @buf: buffer to print the message to
 *  @size: number of bytes for the message
 *  @fmt: the format of the message
 *                                           
 *  Print the message to the buffer in the format specified.
 *                                           
 *  RETURN VALUE:
 *  0 if success
 *  non-zero otherwise
 */                                          
/* _VMKLNX_CODECHECK_: snprintf */

extern int snprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
#if defined(__VMKLNX__)
/**                                          
 *  vsnprintf - print the message to the buffer
 *  @buf: the buffer to print the message to
 *  @size: the number of bytes for the message
 *  @fmt: the format of the message
 *  @args: the arguments for the format string 
 *                                           
 *  Print the message to the buffer in the format specified.
 *                                           
 *  RETURN VALUE:
 *  It returns the number of bytes required by the format string.
 */                                          
/* _VMKLNX_CODECHECK_: vsnprintf */
static inline int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
   return vmk_Vsnprintf(buf, size, fmt, args);
}
#else /* !defined(__VMKLNX__) */
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
#endif
extern int scnprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
#if defined(__VMKLNX__)
/**
 *  vscnprintf - Format a string and place it in a buffer
 *  @buf: The buffer to place the result into
 *  @size: The size of the buffer, including the trailing null space
 *  @fmt: The format string to use
 *  @args: Arguments for the format string
 *
 *  Format a string and place it in a buffer.
 *
 *  RETURN VALUE:
 *  The number of characters written into the buf not including the trailing '\0'. 
 */
/* _VMKLNX_CODECHECK_: vscnprintf */

static inline int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
   int i = vmk_Vsnprintf(buf, size, fmt, args);
   return (i >= size) ? (size - 1) : i;
}
#else  /* !defined(__VMKLNX__) */
extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
#endif /* defined(__VMKLNX__) */
extern char *kasprintf(gfp_t gfp, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));

#if defined(__VMKLNX__)
#define vsscanf(str, fmt, va_list)\
   vmk_Vsscanf(str, vmt, va_list)
#define sscanf(str, fmt, args...) \
   vmk_Sscanf(str, fmt, ##args)
#else /* !defined(__VMKLNX__) */
extern int sscanf(const char *, const char *, ...)
	__attribute__ ((format (scanf, 2, 3)));
extern int vsscanf(const char *, const char *, va_list)
	__attribute__ ((format (scanf, 2, 0)));
#endif /* defined(__VMKLNX__) */

extern int get_option(char **str, int *pint);
extern char *get_options(const char *str, int nints, int *ints);
extern unsigned long long memparse(char *ptr, char **retptr);

extern int core_kernel_text(unsigned long addr);
extern int __kernel_text_address(unsigned long addr);
extern int kernel_text_address(unsigned long addr);
extern int session_of_pgrp(int pgrp);

extern void dump_thread(struct pt_regs *regs, struct user *dump);

#ifdef CONFIG_PRINTK
asmlinkage int vprintk(const char *fmt, va_list args)
	__attribute__ ((format (printf, 1, 0)));
asmlinkage int printk(const char * fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
#else
static inline int vprintk(const char *s, va_list args)
	__attribute__ ((format (printf, 1, 0)));
static inline int vprintk(const char *s, va_list args) { return 0; }
static inline int printk(const char *s, ...)
	__attribute__ ((format (printf, 1, 2)));
static inline int printk(const char *s, ...) { return 0; }
#endif

unsigned long int_sqrt(unsigned long);

static inline int __attribute_pure__ long_log2(unsigned long x)
{
	int r = 0;
	for (x >>= 1; x > 0; x >>= 1)
		r++;
	return r;
}

static inline unsigned long
__attribute_const__ roundup_pow_of_two(unsigned long x)
{
	return 1UL << fls_long(x - 1);
}

extern int printk_ratelimit(void);
extern int __printk_ratelimit(int ratelimit_jiffies, int ratelimit_burst);

static inline void console_silent(void)
{
	console_loglevel = 0;
}

static inline void console_verbose(void)
{
	if (console_loglevel)
		console_loglevel = 15;
}

extern void bust_spinlocks(int yes);
extern int oops_in_progress;		/* If set, an oops, panic(), BUG() or die() is in progress */
extern int panic_timeout;
extern int panic_on_oops;
extern int panic_on_unrecovered_nmi;
extern int tainted;

#if !defined(__VMKLNX__)
extern const char *print_tainted(void);
#endif

extern void add_taint(unsigned);

/* Values used for system_state */
extern enum system_states {
	SYSTEM_BOOTING,
	SYSTEM_RUNNING,
	SYSTEM_HALT,
	SYSTEM_POWER_OFF,
	SYSTEM_RESTART,
	SYSTEM_SUSPEND_DISK,
} system_state;

#define TAINT_PROPRIETARY_MODULE	(1<<0)
#define TAINT_FORCED_MODULE		(1<<1)
#define TAINT_UNSAFE_SMP		(1<<2)
#define TAINT_FORCED_RMMOD		(1<<3)
#define TAINT_MACHINE_CHECK		(1<<4)
#define TAINT_BAD_PAGE			(1<<5)

extern void dump_stack(void);

#ifdef DEBUG
/* If you are writing a driver, please use dev_dbg instead */
/**
 *  pr_debug - print messages to the vmkernel log
 *  @fmt: format string
 *  @arg: values for format string
 *
 *  Print a formatted message to the vmkernel log.  This function
 *  is a no-op if DEBUG is not defined.
 *
 *  SYNOPSIS:
 *  #define pr_debug(fmt, arg...)
 *
 *  ESX Deviation Notes:
 *  The message is also written to serial port. 
 *
 *  SEE ALSO:
 *  printk pr_info dev_printk dev_dbg dev_err dev_warn dev_info
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: pr_debug */
#define pr_debug(fmt,arg...) \
	printk(KERN_DEBUG fmt,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

/**
 *  pr_info - print messages to the vmkernel log
 *  @fmt: format string
 *  @arg: values for format string
 *
 *  Print a formatted message to the vmkernel log.
 *
 *  SYNOPSIS:
 *  #define pr_debug(fmt, arg...)
 *
 *  ESX Deviation Notes:
 *  The message is also written to serial port.
 *
 *  SEE ALSO:
 *  printk pr_debug dev_printk dev_dbg dev_err dev_warn dev_info
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: pr_info */
#define pr_info(fmt,arg...) \
	printk(KERN_INFO fmt,##arg)

/*
 *      Display an IP address in readable format.
 */

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
#define NIPQUAD_FMT "%u.%u.%u.%u"

#define NIP6(addr) \
	ntohs((addr).s6_addr16[0]), \
	ntohs((addr).s6_addr16[1]), \
	ntohs((addr).s6_addr16[2]), \
	ntohs((addr).s6_addr16[3]), \
	ntohs((addr).s6_addr16[4]), \
	ntohs((addr).s6_addr16[5]), \
	ntohs((addr).s6_addr16[6]), \
	ntohs((addr).s6_addr16[7])
#define NIP6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#define NIP6_SEQFMT "%04x%04x%04x%04x%04x%04x%04x%04x"

#if defined(__LITTLE_ENDIAN)
#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#elif defined(__BIG_ENDIAN)
#define HIPQUAD	NIPQUAD
#else
#error "Please fix asm/byteorder.h"
#endif /* __LITTLE_ENDIAN */

/*
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

/**
 *  max - Find the max value of two given values
 *  @x: first value
 *  @y: second value
 *
 *  Find the max of the two values. 
 *
 *  SYNOPSIS:
 *  #define max(x,y)
 *
 *  RETURN VALUE:
 *  Max value
 *
 */
/* _VMKLNX_CODECHECK_: max */
#define max(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max at all, of course.
 */
/**
 *  min_t - Find the min value
 *  @type: Type of variable to use
 *  @x: first value
 *  @y: second value
 *
 *  Find the min of the two values. The macros allows for a @type
 *  to be passed. It assumes that the two values @x and @y are of
 *  type 'type' and uses temporary variables of type 'type' to 
 *  make all comparisons
 *
 *  SYNOPSIS:
 *      #define min_t(type,x,y)
 *
 *  RETURN VALUE:
 *  Min value
 *
 */
/* _VMKLNX_CODECHECK_: min_t */
#define min_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
/**
 *  max_t - Find the max value
 *  @type: Type of variable to use
 *  @x: first value
 *  @y: second value
 *
 *  Find the max of the two values. The macros allows for a @type
 *  to be passed. It assumes that the two values @x and @y are of
 *  type 'type' and uses temporary variables of type 'type' to 
 *  make all comparisons
 *
 *  SYNOPSIS:
 *      #define max_t(type,x,y)
 *
 *  RETURN VALUE:
 *  Max value
 *
 */
/* _VMKLNX_CODECHECK_: max_t */
#define max_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

/**
 * clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does strict typechecking of min/max to make sure they are of the
 * same type as val.  See the unnecessary pointer comparisons.
 *
 *  SYNOPSIS:
 *      #define clamp(val,min,max)
 *
 *  RETURN VALUE:
 *  Value clamped with the given range
 */
#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	(void) (&__val == &__min);		\
	(void) (&__val == &__max);		\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })

/**
 * clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * 'type' to make all the comparisons.
 *
 * SYNOPSIS:
 *     #define clamp_t(val,min,max)
 *
 * RETURN VALUE:
 * Value clamped with the given range
 *
 */
#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })

/**
 * clamp_val - return a value clamped to a given range using val's type
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of whatever
 * type the input argument 'val' is.  This is useful when val is an unsigned
 * type and min and max are literals that will otherwise be assigned a signed
 * integer type.
 *
 * SYNOPSIS:
 *     #define clamp_val(val,min,max)
 *
 * RETURN VALUE:
 * Value clamped with the given range
 *
 */
#define clamp_val(val, min, max) ({		\
	typeof(val) __val = (val);		\
	typeof(val) __min = (min);		\
	typeof(val) __max = (max);		\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })


/**
 *  container_of - cast member of a structure out to the containing structure
 *  @ptr:    pointer to the member.
 *  @type:   type of the container struct this is embedded in.
 *  @member: name of the member within the struct.
 *
 *  Cast a member of a structure out to the containing structure.
 *
 *  SYNOPSIS:
 *     #define container_of(ptr, type, member)
 *  
 *  RETURN VALUE:
 *  the location of the structure of type 'type' in memory.
 */
/* _VMKLNX_CODECHECK_: container_of */
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */
#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

/*
 * Check at compile time that 'function' is a certain type, or is a pointer
 * to that type (needs to use typedef for the function type.)
 */
#define typecheck_fn(type,function) \
({	typeof(type) __tmp = function; \
	(void)__tmp; \
})

#endif /* __KERNEL__ */

#define SI_LOAD_SHIFT	16
struct sysinfo {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned short pad;		/* explicit padding for m68k */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};

/* Force a compilation error if condition is true */
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)

/* Trap pasters of __FUNCTION__ at compile-time */
#define __FUNCTION__ (__func__)

#if defined(__VMKERNEL__)
#include "vmkapi.h"

/* 
 * Redefine some of the API to be their counterparts in vmkapi
 */
#define snprintf vmk_Snprintf

/**
 *  simple_strtol - Convert a string to a signed long integer
 *
 *  Convert a string to a signed long integer.
 *
 *  ARGUMENTS:
 *  str - beginning of the string
 *  end - end of the string
 *  base - base of the number system the string should be interpreted in
 *
 *  SYNOPSIS:
 *     #define simple_strtol(str,end,base)
 *
 *  ESX Deviation Notes:
 *   This is a macro.
 *
 *  RETURN VALUE:
 *   long integer value
 *
 */
 /* _VMKLNX_CODECHECK_: simple_strtol */

#define simple_strtol vmk_Strtol

/**
 *  simple_strtoul - Convert a string to an unsigned long integer
 *
 *  Convert a string to an unsigned long integer.
 *
 *  ARGUMENTS:
 *  str - beginning of the string
 *  end - end of the string
 *  base - base of the number system the string should be interpreted in
 *
 *  SYNOPSIS:
 *     #define simple_strtoul(str,end,base)
 *
 *  ESX Deviation Notes:
 *   This is a macro.
 *
 *  RETURN VALUE:
 *   unsigned long integer value
 *
 */
 /* _VMKLNX_CODECHECK_: simple_strtoul */

#define simple_strtoul vmk_Strtoul
#endif /* defined(__VMKERNEL__) */
#endif
