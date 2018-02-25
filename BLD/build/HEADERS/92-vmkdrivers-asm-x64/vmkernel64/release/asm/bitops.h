/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
#ifndef _X86_64_BITOPS_H
#define _X86_64_BITOPS_H

#if defined(__VMKLNX__)
#include "vmkapi.h"
#endif /* defined(__VMKLNX__) */

/*
 * Copyright 1992, Linus Torvalds.
 */

#include <asm/alternative.h>

#define ADDR (*(volatile long *) addr)

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 *
 * RETURN VALUE: 
 * NONE
 *
 */
/* _VMKLNX_CODECHECK_: set_bit */
static __inline__ void set_bit(int nr, volatile void * addr)
{
	__asm__ __volatile__( LOCK_PREFIX
		"btsl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr) : "memory");
}

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __set_bit(int nr, volatile void * addr)
{
	__asm__ volatile(
		"btsl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr) : "memory");
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * Clears a bit in memory.
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 *
 * RETURN VALUE:
 * None
 *
 */
/* _VMKLNX_CODECHECK_: clear_bit */
static __inline__ void clear_bit(int nr, volatile void * addr)
{
	__asm__ __volatile__( LOCK_PREFIX
		"btrl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr));
}

static __inline__ void __clear_bit(int nr, volatile void * addr)
{
	__asm__ __volatile__(
		"btrl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr));
}

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __change_bit(int nr, volatile void * addr)
{
	__asm__ __volatile__(
		"btcl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr));
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void change_bit(int nr, volatile void * addr)
{
	__asm__ __volatile__( LOCK_PREFIX
		"btcl %1,%0"
		:"+m" (ADDR)
		:"dIr" (nr));
}

/**
 * test_and_set_bit - Set a bit and return its old state
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 * It tests if the bit at position nr in *addr is 0 or not and sets it to 1.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: test_and_set_bit */
static __inline__ int test_and_set_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr) : "memory");
	return oldbit;
}

/**
 * __test_and_set_bit - Set a bit and return its old state
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 * It tests if the  bit at position nr in *addr is 0 or not and sets it to 1.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 *
 * SEE ALSO:
 * test_and_set_bit
 */
static __inline__ int __test_and_set_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__(
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr));
	return oldbit;
}

/**
 * test_and_clear_bit - Clear a bit and return its old state
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 * It tests if the  bit at position nr in *addr is 0 or not and sets it to 0.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: test_and_clear_bit */
static __inline__ int test_and_clear_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr) : "memory");
	return oldbit;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old state
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 * It tests if the  bit at position nr in *addr is 0 or not and sets it to 0.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 *
 * SEE ALSO:
 * test_and_clear_bit
 */
static __inline__ int __test_and_clear_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__(
		"btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr));
	return oldbit;
}

/**
 * __test_and_change_bit - Toggle a bit and return its old state
 * @nr: Bit to toggle
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * It also implies a memory barrier.
 * It tests if the  bit at position nr in *addr is 0 or not and toggles it.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 *
 * SEE ALSO:
 * test_and_change_bit
 */
static __inline__ int __test_and_change_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__(
		"btcl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr) : "memory");
	return oldbit;
}

/**
 * test_and_change_bit - Toggle a bit and return its old state
 * @nr: Bit to toggle
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 * It tests if the  bit at position nr in *addr is 0 or not and toggles it.
 * Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 * RETURN VALUE:
 * 0 if original bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: test_and_change_bit */
static __inline__ int test_and_change_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btcl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"+m" (ADDR)
		:"dIr" (nr) : "memory");
	return oldbit;
}

/**                                          
 *  constant_test_bit - determine whether a bit is set
 *  @nr: bit number to test
 *  @addr: addr to test
 *                                           
 *  Determines the state of the specified bit.
 *  This is used when @nr is known to be constant at compile-time.
 *  Use test_bit() instead of using this directly.
 *                                           
 *  RETURN VALUE:
 *  0 if the bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: constant_test_bit */
static __inline__ int constant_test_bit(int nr, const volatile void * addr)
{
	return ((1UL << (nr & 31)) & (((const volatile unsigned int *) addr)[nr >> 5])) != 0;
}

/**                                          
 *  variable_test_bit - determine whether a bit is set
 *  @nr: bit number to test
 *  @addr: addr to test
 *                                           
 *  Determines the state of the specified bit.
 *  This is used when @nr is a variable.
 *  Use test_bit() instead of using this directly.
 * 
 *  RETURN VALUE:
 *  0 if the bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: variable_test_bit */
static __inline__ int variable_test_bit(int nr, volatile const void * addr)
{
	int oldbit;

	__asm__ __volatile__(
		"btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (ADDR),"dIr" (nr));
	return oldbit;
}
/**
 *  test_bit - Determine if bit at given position is set
 *  @nr: number of bit to be tested
 *  @addr: pointer to byte to test
 *
 *  It tests if the bit at position nr in *addr is 0 or not.
 *  If the bit number is a constant an optimized bit extract is done.
 *  Note that the return value need not be 1 (just non-zero) if the bit was 1.
 *
 *  SYNOPSIS:
 *  #define test_bit(nr,addr)
 *
 *  RETURN VALUE:
 *  0 if the bit was 0 and NON-ZERO otherwise
 */
/* _VMKLNX_CODECHECK_: test_bit */
#define test_bit(nr,addr) \
(__builtin_constant_p(nr) ? \
 constant_test_bit((nr),(addr)) : \
 variable_test_bit((nr),(addr)))

#undef ADDR
#if defined(__VMKLNX__)
/**
 * find_first_zero_bit - find the first zero bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum bitnumber to search
 *
 * Finds the first zero bit in a specified memory region
 *
 * RETURN VALUE:
 * Returns the bit-number of the first zero bit, not the number of the byte
 * containing a bit.
 * If result is equal to or greater than size means no zero bit is found
 */
/* _VMKLNX_CODECHECK_: find_first_zero_bit */
static __inline__ long 
find_first_zero_bit(const unsigned long * addr, unsigned long size)
{
        long d0, d1, d2;
        long res;

        /*
         * We must test the size in words, not in bits, because
         * otherwise incoming sizes in the range -63..-1 will not run
         * any scasq instructions, and then the flags used by the je
         * instruction will have whatever random value was in place
         * before.  Nobody should call us like that, but
         * find_next_zero_bit() does when offset and size are at the
         * same word and it fails to find a zero itself.
         */
        size += 63;
        size >>= 6;
        if (!size)
                return 0;

        asm volatile(
                "  cld\n"
                "  repe; scasq\n"
                "  je 1f\n"
                "  xorq -8(%%rdi),%%rax\n"
                "  subq $8,%%rdi\n"
                "  bsfq %%rax,%%rdx\n"
                "1:  subq %[addr],%%rdi\n"
                "  shlq $3,%%rdi\n"
                "  addq %%rdi,%%rdx"
                :"=d" (res), "=&c" (d0), "=&D" (d1), "=&a" (d2)
                :"0" (0ULL), "1" (size), "2" (addr), "3" (-1ULL),
                 [addr] "S" (addr) : "memory");
        /*
         * Any register would do for [addr] above, but GCC tends to
         * prefer rbx over rsi, even though rsi is readily available
         * and doesn't have to be saved.
         */

        return res;
}

/**
 * find_next_zero_bit - find the first zero bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 *
 * Finds the first zero bit in a specified memory region
 *
 * RETURN VALUE:
 * Returns the bit-number of the first zero bit in a memory region after the 
 * specified offset
 * If result is equal to or greater than size means no zero bit is found
 */
/* _VMKLNX_CODECHECK_: find_next_zero_bit */
static __inline__ long 
find_next_zero_bit (const unsigned long * addr, long size, long offset)
{
        const unsigned long * p = addr + (offset >> 6);
        unsigned long set = 0;
        unsigned long res, bit = offset&63;

        if (bit) {

                /*
                 * Look for zero in first word
                 */
                asm("bsfq %1,%0\n\t"
                    "cmoveq %2,%0"
                    : "=r" (set)
                    : "r" (~(*p >> bit)), "r"(64L));
                if (set < (64 - bit))
                        return set + offset;
                set = 64 - bit;
                p++;
        }

        /*
         * No zero yet, search remaining full words for a zero
         */
        res = find_first_zero_bit (p, size - 64 * (p - addr));

        return (offset + set + res);
}

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Finds the first set bit in a specified memory region
 *
 * RETURN VALUE:
 * Returns the bit-number of the first set bit, not the number of the byte
 * containing a bit.
 *
 */
/* _VMKLNX_CODECHECK_: find_first_bit */
static __inline__ long find_first_bit(const unsigned long * addr, unsigned long size)
{
	long d0, d1;
	long res;

	/*
	 * We must test the size in words, not in bits, because
	 * otherwise incoming sizes in the range -63..-1 will not run
	 * any scasq instructions, and then the flags used by the jz
	 * instruction will have whatever random value was in place
	 * before.  Nobody should call us like that, but
	 * find_next_bit() does when offset and size are at the same
	 * word and it fails to find a one itself.
	 */
	size += 63;
	size >>= 6;
	if (!size)
		return 0;

	asm volatile(
                "   cld\n"
                "   repe; scasq\n"
		"   jz 1f\n"
		"   subq $8,%%rdi\n"
		"   bsfq (%%rdi),%%rax\n"
		"1: subq %[addr],%%rdi\n"
		"   shlq $3,%%rdi\n"
		"   addq %%rdi,%%rax"
		:"=a" (res), "=&c" (d0), "=&D" (d1)
		:"0" (0ULL), "1" (size), "2" (addr),
		 [addr] "r" (addr) : "memory");
	return res;
}

/**
 * find_next_bit - find the first set bit in a memory region
 * @addr: The address to base the search on
 * @size: The maximum size to search
 * @offset: The bitnumber to start searching at
 *
 * Finds the first set bit in a specified memory region
 *
 * RETURN VALUE:
 * Position of the first set bit in the specified memory, starting from offset.
 * If none is found, the full word, starting from addr, is searched.
 */
/* _VMKLNX_CODECHECK_: find_next_bit */
static __inline__ long find_next_bit(const unsigned long * addr, long size, long offset)
{
	const unsigned long * p = addr + (offset >> 6);
	unsigned long set = 0, bit = offset & 63, res;

	if (bit) {
		/*
		 * Look for nonzero in the first 64 bits:
		 */
		asm("bsfq %1,%0\n\t"
		    "cmoveq %2,%0\n\t"
		    : "=r" (set)
		    : "r" (*p >> bit), "r" (64L));
		if (set < (64 - bit))
			return set + offset;
		set = 64 - bit;
		p++;
	}
	/*
	 * No set bit yet, search remaining full words for a bit
	 */
	res = find_first_bit (p, size - 64 * (p - addr));
	return (offset + set + res);
}


#else /* !defined(__VMKLNX__) */
extern long find_first_zero_bit(const unsigned long * addr, unsigned long size);
extern long find_next_zero_bit (const unsigned long * addr, long size, long offset);
extern long find_first_bit(const unsigned long * addr, unsigned long size);
extern long find_next_bit(const unsigned long * addr, long size, long offset);
#endif /* defined(__VMKLNX__) */

/**                                          
 * __scanbit - searches unsigned long value for the least significant set bit 
 * @val: The unsigned long value to scan for the set bit
 * @max: The value to return if no set bit found
 *
 * Finds the least significant set bit in specified unsigned long value
 *
 * RETURN VALUE:
 * Index of first bit set in val or max when no bit is set
 */                                          
/* _VMKLNX_CODECHECK_: __scanbit */
static inline unsigned long __scanbit(unsigned long val, unsigned long max)
{
	asm("bsfq %1,%0 ; cmovz %2,%0" : "=&r" (val) : "r" (val), "r" (max));
	return val;
}

#define find_first_bit(addr,size) \
((__builtin_constant_p(size) && (size) <= BITS_PER_LONG ? \
  (__scanbit(*(unsigned long *)addr,(size))) : \
  find_first_bit(addr,size)))

#define find_next_bit(addr,size,off) \
((__builtin_constant_p(size) && (size) <= BITS_PER_LONG ? 	  \
  ((off) + (__scanbit((*(unsigned long *)addr) >> (off),(size)-(off)))) : \
	find_next_bit(addr,size,off)))

#define find_first_zero_bit(addr,size) \
((__builtin_constant_p(size) && (size) <= BITS_PER_LONG ? \
  (__scanbit(~*(unsigned long *)addr,(size))) : \
  	find_first_zero_bit(addr,size)))
	
#define find_next_zero_bit(addr,size,off) \
((__builtin_constant_p(size) && (size) <= BITS_PER_LONG ? 	  \
  ((off)+(__scanbit(~(((*(unsigned long *)addr)) >> (off)),(size)-(off)))) : \
	find_next_zero_bit(addr,size,off)))

/* 
 * Find string of zero bits in a bitmap. -1 when not found.
 */ 
extern unsigned long 
find_next_zero_string(unsigned long *bitmap, long start, long nbits, int len);

static inline void set_bit_string(unsigned long *bitmap, unsigned long i, 
				  int len) 
{ 
	unsigned long end = i + len; 
	while (i < end) {
		__set_bit(i, bitmap); 
		i++;
	}
} 

static inline void __clear_bit_string(unsigned long *bitmap, unsigned long i, 
				    int len) 
{ 
	unsigned long end = i + len; 
	while (i < end) {
		__clear_bit(i, bitmap); 
		i++;
	}
} 

/**
 * ffz - find first zero in word.
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 *
 * RETURN VALUE:
 * Position of the first zero in the word 
 */
/* _VMKLNX_CODECHECK_: ffz */
static __inline__ unsigned long ffz(unsigned long word)
{
	__asm__("bsfq %1,%0"
		:"=r" (word)
		:"r" (~word));
	return word;
}

/**
 * __ffs - find first set bit in word
 * @word: The word to search
 *
 * Undefined if no bit is set, so code should check against 0 first.
 *
 * RETURN VALUE:
 * Position of the first set bit in the word 
 */
/* _VMKLNX_CODECHECK_: __ffs */
static __inline__ unsigned long __ffs(unsigned long word)
{
	__asm__("bsfq %1,%0"
		:"=r" (word)
		:"rm" (word));
	return word;
}

/*
 * __fls: find last bit set.
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
static __inline__ unsigned long __fls(unsigned long word)
{
	__asm__("bsrq %1,%0"
		:"=r" (word)
		:"rm" (word));
	return word;
}

#ifdef __KERNEL__

#include <asm-generic/bitops/sched.h>

/**
 * ffs - find first bit set
 * @x: the word to search
 *
 * Finds the first bit set in the referenced word
 *
 * RETURN VALUE:
 * Position of the first set bit
 */
/* _VMKLNX_CODECHECK_: ffs */
static __inline__ int ffs(int x)
{
	int r;

	__asm__("bsfl %1,%0\n\t"
		"cmovzl %2,%0" 
		: "=r" (r) : "rm" (x), "r" (-1));
	return r+1;
}

/**
 * fls64 - find last bit set in 64 bit word
 * @x: the word to search
 *
 * This is defined the same way as fls.
 */
static __inline__ int fls64(__u64 x)
{
	if (x == 0)
		return 0;
	return __fls(x) + 1;
}

/**
 * fls - find last bit set
 * @x: the word to search
 *
 * Finds last bit set in the specified word
 * 
 * RETURN VALUE:
 * The last set bit in specified word
 */
/* _VMKLNX_CODECHECK_: fls */
static __inline__ int fls(int x)
{
	int r;

	__asm__("bsrl %1,%0\n\t"
		"cmovzl %2,%0"
		: "=&r" (r) : "rm" (x), "rm" (-1));
	return r+1;
}

#include <asm-generic/bitops/hweight.h>

#endif /* __KERNEL__ */

#ifdef __KERNEL__

#include <asm-generic/bitops/ext2-non-atomic.h>

#define ext2_set_bit_atomic(lock,nr,addr) \
	        test_and_set_bit((nr),(unsigned long*)addr)
#define ext2_clear_bit_atomic(lock,nr,addr) \
	        test_and_clear_bit((nr),(unsigned long*)addr)

#include <asm-generic/bitops/minix.h>

#endif /* __KERNEL__ */

#endif /* _X86_64_BITOPS_H */
