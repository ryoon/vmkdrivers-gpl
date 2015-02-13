#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */

extern unsigned long loops_per_jiffy;

#include <asm/delay.h>
#include <linux/hardirq.h>
/*
 * Using udelay() for intervals greater than a few milliseconds can
 * risk overflow for high loops_per_jiffy (high bogomips) machines. The
 * mdelay() provides a wrapper to prevent this.  For delays greater
 * than MAX_UDELAY_MS milliseconds, the wrapper is used.  Architecture
 * specific values can be defined in asm-???/delay.h as an override.
 * The 2nd mdelay() definition ensures GCC will optimize away the 
 * while loop for the common cases where n <= MAX_UDELAY_MS  --  Paul G.
 */

#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_MS	5
#endif

#ifndef mdelay
#define mdelay(n) (				\
{						\
	static int warned=0;			\
	unsigned long __ms=(n);			\
	WARN_ON(in_irq() && !(warned++));	\
	while (__ms--) udelay(1000);		\
})
#endif

#ifndef ndelay
#define ndelay(x)	udelay(((x)+999)/1000)
#endif

void calibrate_delay(void);
void msleep(unsigned int msecs);
unsigned long msleep_interruptible(unsigned int msecs);

/**                                          
 *  ssleep - sleep for @seconds number of seconds
 *  @seconds: number of seconds to sleep
 *                                           
 *  Sleep for @seconds number of seconds
 */                                          
/* _VMKLNX_CODECHECK_: ssleep */
static inline void ssleep(unsigned int seconds)
{
	msleep(seconds * 1000);
}

#endif /* defined(_LINUX_DELAY_H) */
