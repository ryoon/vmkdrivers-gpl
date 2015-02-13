#ifndef _LINUX_CONFIG_H
#define _LINUX_CONFIG_H
/* This file is no longer in use and kept only for backward compatibility.
 * autoconf.h is now included via -imacros on the commandline
 */
#warning Including config.h is deprecated.
#include <linux/autoconf.h>
#if !defined (__KERNEL__) && !defined(__KERNGLUE__)
#error including kernel header in userspace; use the glibc headers instead!
#endif
#endif
