/* Copyright 2002,2003 Andi Kleen, SuSE Labs.
 * Subject to the GNU Public License v.2
 * 
 * Wrappers of assembly checksum functions for x86-64.
 */

#include <asm/checksum.h>
#include <linux/module.h>

unsigned short csum_ipv6_magic(struct in6_addr *saddr, struct in6_addr *daddr,
                               __u32 len, unsigned short proto, unsigned int sum) 
{
        __u64 rest, sum64;

        rest = (__u64)htonl(len) + (__u64)htons(proto) + (__u64)sum;
        asm("  addq (%[saddr]),%[sum]\n"
            "  adcq 8(%[saddr]),%[sum]\n"
            "  adcq (%[daddr]),%[sum]\n" 
            "  adcq 8(%[daddr]),%[sum]\n"
            "  adcq $0,%[sum]\n"
            : [sum] "=r" (sum64) 
            : "[sum]" (rest),[saddr] "r" (saddr), [daddr] "r" (daddr));
        return csum_fold(add32_with_carry(sum64 & 0xffffffff, sum64>>32));
}

EXPORT_SYMBOL(csum_ipv6_magic);
