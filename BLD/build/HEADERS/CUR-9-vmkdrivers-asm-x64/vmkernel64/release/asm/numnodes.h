#ifndef _ASM_X8664_NUMNODES_H
#define _ASM_X8664_NUMNODES_H 1

#include <linux/config.h>

/* Implement this change only for large-SMP configuration */
#if NR_CPUS > 8
#define NODES_SHIFT	6
#else
#define NODES_SHIFT	3
#endif

#endif
