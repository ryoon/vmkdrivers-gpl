/*
 * Portions Copyright 2012 VMware, Inc.
 */
#include <linux/module.h>
#include <asm/types.h>

/**
 * hweightN - returns the hamming weight of a N-bit word
 * @w: the word to weigh
 *
 * The Hamming Weight of a number is the total number of bits set in it.
 */

unsigned int hweight32(unsigned int w)
{
	unsigned int res = w - ((w >> 1) & 0x55555555);
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
	res = (res + (res >> 4)) & 0x0F0F0F0F;
	res = res + (res >> 8);
	return (res + (res >> 16)) & 0x000000FF;
}
EXPORT_SYMBOL(hweight32);

unsigned int hweight16(unsigned int w)
{
	unsigned int res = w - ((w >> 1) & 0x5555);
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	res = (res & 0x3333) + ((res >> 2) & 0x3333);
	res = (res + (res >> 4)) & 0x0F0F;
	return (res + (res >> 8)) & 0x00FF;
}
EXPORT_SYMBOL(hweight16);

/**
 * hweight8 - Find number of bits set (hamming weight) in the 8-bit parameter
 * @w : Number to weigh 
 *
 * Find number of bits set (hamming weight) in the 8-bit parameter
 *
 * RETURN VALUE
 *    Number of bits set 
 */
/* _VMKLNX_CODECHECK_: hweight8 */

unsigned int hweight8(unsigned int w)
{
	unsigned int res = w - ((w >> 1) & 0x55);
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	res = (res & 0x33) + ((res >> 2) & 0x33);
	return (res + (res >> 4)) & 0x0F;
}
EXPORT_SYMBOL(hweight8);

unsigned long hweight64(__u64 w)
{
#if BITS_PER_LONG == 32
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return hweight32((unsigned int)(w >> 32)) + hweight32((unsigned int)w);
#elif BITS_PER_LONG == 64
	__u64 res = w - ((w >> 1) & 0x5555555555555555ul);
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	res = (res & 0x3333333333333333ul) + ((res >> 2) & 0x3333333333333333ul);
	res = (res + (res >> 4)) & 0x0F0F0F0F0F0F0F0Ful;
	res = res + (res >> 8);
	res = res + (res >> 16);
	return (res + (res >> 32)) & 0x00000000000000FFul;
#else
#error BITS_PER_LONG not defined
#endif
}
EXPORT_SYMBOL(hweight64);
