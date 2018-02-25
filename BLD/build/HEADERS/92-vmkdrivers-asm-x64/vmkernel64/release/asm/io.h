/*
 * Portions Copyright 2008, 2009 and 2011 VMware, Inc.
 */
#ifndef _ASM_IO_H
#define _ASM_IO_H


/*
 * This file contains the definitions for the x86 IO instructions
 * inb/inw/inl/outb/outw/outl and the "string versions" of the same
 * (insb/insw/insl/outsb/outsw/outsl). You can also use "pausing"
 * versions of the single-IO instructions (inb_p/inw_p/..).
 *
 * This file is not meant to be obfuscating: it's just complicated
 * to (a) handle it all in a way that makes gcc able to optimize it
 * as well as possible and (b) trying to avoid writing the same thing
 * over and over again with slight variations and possibly making a
 * mistake somewhere.
 */

/*
 * Thanks to James van Artsdalen for a better timing-fix than
 * the two short jumps: using outb's to a nonexistent port seems
 * to guarantee better timings even on fast machines.
 *
 * On the other hand, I'd like to be sure of a non-existent port:
 * I feel a bit unsafe about using 0x80 (should be safe, though)
 *
 *		Linus
 */

 /*
  *  Bit simplified and optimized by Jan Hubicka
  *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999.
  *
  *  isa_memset_io, isa_memcpy_fromio, isa_memcpy_toio added,
  *  isa_read[wl] and isa_write[wl] fixed
  *  - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  */

#define __SLOW_DOWN_IO "\noutb %%al,$0x80"

#ifdef REALLY_SLOW_IO
#define __FULL_SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO __SLOW_DOWN_IO
#else
#define __FULL_SLOW_DOWN_IO __SLOW_DOWN_IO
#endif

/*
 * In case of any code change below, make sure you
 * go and update the corresponding documentation.
 * The documentation file can be found at
 * vmkdrivers/src_92/doc/dummyDefs.doc
 *
 * outb
 * outb_p
 * outw
 * outl
 * outsw
 */

/*
 * Talk about misusing macros..
 */
#define __OUT1(s,x) \
static inline void out##s(unsigned x value, unsigned short port) {

#if defined(__VMKLNX__)
#define __OUT2(s,s1,s2) \
__asm__ __volatile__ ("cld\n" "out" #s " %" s1 "0,%" s2 "1"
#else /* !defined(__VMKLNX__) */
#define __OUT2(s,s1,s2) \
__asm__ __volatile__ ("out" #s " %" s1 "0,%" s2 "1"
#endif /* defined(__VMKLNX__) */

#define __OUT(s,s1,x) \
__OUT1(s,x) __OUT2(s,s1,"w") : : "a" (value), "Nd" (port)); } \
__OUT1(s##_p,x) __OUT2(s,s1,"w") __FULL_SLOW_DOWN_IO : : "a" (value), "Nd" (port));} \

#define __IN1(s) \
static inline RETURN_TYPE in##s(unsigned short port) { RETURN_TYPE _v;

#if defined(__VMKLNX__)
#define __IN2(s,s1,s2) \
__asm__ __volatile__ ("cld\n" "in" #s " %" s2 "1,%" s1 "0"
#else /* !defined(__VMKLNX__) */
#define __IN2(s,s1,s2) \
__asm__ __volatile__ ("in" #s " %" s2 "1,%" s1 "0"
#endif /* defined(__VMKLNX__) */

#define __IN(s,s1,i...) \
__IN1(s) __IN2(s,s1,"w") : "=a" (_v) : "Nd" (port) ,##i ); return _v; } \
__IN1(s##_p) __IN2(s,s1,"w") __FULL_SLOW_DOWN_IO : "=a" (_v) : "Nd" (port) ,##i ); return _v; } \

#if defined(__VMKLNX__)
#define __INS(s) \
static inline void ins##s(unsigned short port, void * addr, unsigned long count) \
{ \
 __asm__ __volatile__ ("cld\n" "rep ; ins" #s \
: "=D" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }
#else /* !defined(__VMKLNX__) */
#define __INS(s) \
static inline void ins##s(unsigned short port, void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; ins" #s \
: "=D" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__)
#define __OUTS(s) \
static inline void outs##s(unsigned short port, const void * addr, unsigned long count) \
{ \
 __asm__ __volatile__ ("cld\n" "rep ; outs" #s \
: "=S" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }
#else /* !defined(__VMKLNX__) */
#define __OUTS(s) \
static inline void outs##s(unsigned short port, const void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; outs" #s \
: "=S" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }
#endif /* defined(__VMKLNX__) */

#define RETURN_TYPE unsigned char
__IN(b,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned short
__IN(w,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned int
__IN(l,"")
#undef RETURN_TYPE

__OUT(b,"b",char)
__OUT(w,"w",short)
__OUT(l,,int)

__INS(b)
__INS(w)
__INS(l)

__OUTS(b)
__OUTS(w)
__OUTS(l)

#define IO_SPACE_LIMIT 0xffff

#if defined(__KERNEL__) && __x86_64__

#include <linux/vmalloc.h>

#ifndef __i386__
/*
 * Change virtual addresses to physical addresses and vv.
 * These are pretty trivial
 */
static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}
#endif

/*
 * Change "struct page" to page number.
 */
#if defined(__VMKLNX__)
/**
 *  phys_to_page - machine address to page handle
 *  @maddr : machine address
 *
 *  ESX Deviation Notes:
 *  The resulting page handle cannot be derefenced. The returned value
 *  doesn't correspond to an address of page structure but to the actual page
 *  number. This page handle needs to be handled through the page api only.
 *
 */
/* _VMKLNX_CODECHECK_: phys_to_page */
#define phys_to_page(maddr) ((struct page *)pfn_to_page(maddr >> PAGE_SHIFT))

/**
 *  page_to_phys - page handle to machine address
 *  @page : page handle
 *
 *  Gets the machine address that corresponds to the page parameter
 *
 *  ESX Deviation Notes:
 *  None.
 *
 *  RETURN VALUE:
 *  A machine address
 *
 */
/* _VMKLNX_CODECHECK_: page_to_phys */
#define page_to_phys(page)  ((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#else /* !defined(__VMKLNX__) */
#define page_to_phys(page)    ((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#endif /* defined(__VMKLNX__) */

#include <asm-generic/iomap.h>

extern void __iomem *__ioremap(unsigned long offset, unsigned long size, unsigned long flags);

/**                                          
 *  ioremap - perform a cachable mapping of a physically contiguous range
 *  @offset: physical address to map   
 *  @size: number of bytes to map
 *                                           
 *  Map in a physically contiguous range into kernel virtual memory and
 *  get a pointer to the mapped region. The region is mapped cacheable.      
 *                                           
 *  RETURN VALUE:
 *     None.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: ioremap */
static inline void __iomem * ioremap (unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

extern void *early_ioremap(unsigned long addr, unsigned long size);
extern void early_iounmap(void *addr, unsigned long size);

/*
 * This one maps high address device memory and turns off caching for that area.
 * it's useful if some control registers are in such an area and write combining
 * or read caching is not desirable:
 */
extern void __iomem * ioremap_nocache (unsigned long offset, unsigned long size);
extern void iounmap(volatile void __iomem *addr);

/*
 * ISA I/O bus memory addresses are 1:1 with the physical address.
 */
#define isa_virt_to_bus virt_to_phys
#define isa_page_to_bus page_to_phys
#define isa_bus_to_virt phys_to_virt

/*
 * However PCI ones are not necessarily 1:1 and therefore these interfaces
 * are forbidden in portable PCI drivers.
 *
 * Allow them on x86 for legacy drivers, though.
 */
#if !defined(__VMKLNX__)
#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#endif

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the x86 architecture, we just read/write the
 * memory location directly.
 */

/**                                          
 *  __readb - Read a byte from specified address
 *  @addr: memory address to read from
 *                                           
 *  Reads a byte from a memory location that is mapped to a 
 *  device.
 *
 *  RETURN VALUE:
 *  An 8-bit unsigned byte value
 *
 *  SEE ALSO:
 *  readb
 */                                          
/* _VMKLNX_CODECHECK_: __readb */
static inline __u8 __readb(const volatile void __iomem *addr)
{
	return *(__force volatile __u8 *)addr;
}

/**                                          
 *  __readw - Read a word (16 bits) from specified address
 *  @addr: memory address to read from
 *
 *  Reads a word (16 bits) from a memory location
 *  that is mapped to a device
 *
 *  RETURN VALUE:
 *  A 16-bit unsigned short word value
 *
 *  SEE ALSO:
 *  readw
 */                                          
/* _VMKLNX_CODECHECK_: __readw */
static inline __u16 __readw(const volatile void __iomem *addr)
{
	return *(__force volatile __u16 *)addr;
}

/**                                          
 *  __readl - Read a long (32 bits) from specified address
 *  @addr: memory address to read from
 *
 *  Reads a long (32 bits) from a memory location that is
 *  mapped to a device
 *
 *  RETURN VALUE:
 *  A 32-bit unsigned long word value
 *
 *  SEE ALSO:
 *  readl
 */
/* _VMKLNX_CODECHECK_: __readl */
static __always_inline __u32 __readl(const volatile void __iomem *addr)
{
	return *(__force volatile __u32 *)addr;
}

/**                                          
 *  __readq - Read a quad word (64 bits) from specified address
 *  @addr: memory address to read from
 *
 *  Reads a quad word (64 bits) from a memory location
 *  that is mapped to a device
 *
 *  RETURN VALUE:
 *  A 64-bit unsigned quard word value
 *
 *  SEE ALSO:
 *  readq
 */
/* _VMKLNX_CODECHECK_: __readq */
static inline __u64 __readq(const volatile void __iomem *addr)
{
	return *(__force volatile __u64 *)addr;
}

/**                                          
 *  readb - Read a byte from specified address
 *  @x: memory address to read from
 *                                           
 *  Reads a byte from a memory location that is mapped to a device.
 *  readb is an alias to __readb.
 *                                           
 *  RETURN VALUE:
 *  An 8-bit unsigned byte value
 *
 *  SEE ALSO:
 *  __readb
 */                                          
/* _VMKLNX_CODECHECK_: readb */
#define readb(x) __readb(x)

/**                                          
 *  readw - Read a word (16 bits) from specified address
 *  @x: memory address to read from
 *
 *  Reads a word (16 bits) from a memory location that is mapped to a device.
 *  readw is an alias to __readw.
 *
 *  RETURN VALUE:
 *  A 16-bit unsigned short word value
 *
 *  SEE ALSO:
 *  __readw
 */                                          
/* _VMKLNX_CODECHECK_: readw */
#define readw(x) __readw(x)

/**                                          
 *  readl - Read a long (32 bits) from specified address
 *  @x: memory address to read from
 *
 *  Reads a long (32 bits) from a memory location that is mapped to a device.
 *  readl is an alias to __readl.
 *
 *  RETURN VALUE:
 *  A 32-bit unsigned long word value
 *
 *  SEE ALSO:
 *  __readl
 */
/* _VMKLNX_CODECHECK_: readl */
#define readl(x) __readl(x)

/**                                          
 *  readq - Read a quad word (64 bits) from specified address
 *  @x: memory address to read from
 *
 *  Reads a quad word (64 bits) from a memory location that is mapped to a device.
 *  readq is an alias to __readq.
 *
 *  RETURN VALUE:
 *  A 64-bit unsigned quard word value
 *
 *  SEE ALSO:
 *  __readq
 */
/* _VMKLNX_CODECHECK_: readq */
#define readq(x) __readq(x)

#define readb_relaxed(a) readb(a)

/**                                          
 *  readw_relaxed - Read a word (16 bits) from specified address
 *  @a: memory address to read from
 *
 *  Reads a word (16 bits) from a memory location that is mapped to a device.
 *  readw_relaxed is an alias to __readw.
 *
 *  RETURN VALUE:
 *  A 16-bit unsigned short word value
 *
 *  SEE ALSO:
 *  __readw readw
 */                                          
/* _VMKLNX_CODECHECK_: readw_relaxed */
#define readw_relaxed(a) readw(a)

/**                                          
 *  readl_relaxed - Read a long (32 bits) from specified address
 *  @a: memory address to read from
 *
 *  Reads a long (32 bits) from a memory location that is mapped to a device.
 *  readw_relaxed is an alias to __readl.
 *
 *  RETURN VALUE:
 *  A 32-bit unsigned long word value
 *
 *  SEE ALSO:
 *  __readl readl
 */
/* _VMKLNX_CODECHECK_: readl_relaxed */
#define readl_relaxed(a) readl(a)
#define readq_relaxed(a) readq(a)
#define __raw_readb readb
#define __raw_readw readw
#define __raw_readl readl
#define __raw_readq readq

#define mmiowb()

/**
 *  __writel - write an u32 value to I/O device memory
 *  @b: the u32 value to be written
 *  @addr: the iomapped memory address that is obtained from ioremap() or from ioremap_nocache()
 *
 *  This is an internal function. Please call writel instead. 
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: __writel */
static inline void __writel(__u32 b, volatile void __iomem *addr)
{
	*(__force volatile __u32 *)addr = b;
}

/**
 *__writeq - write a u64 value to I/O device memory
 * @b: the u64 value to be written
 * @addr: the iomapped memory address that is obtained from ioremap() 
 * or from ioremap_nocache()
 * 
 * This is an internal function. Please call writeq instead. 
 * 
 * RETURN VALUE:
 * This function does not return a value.
 * 
 * SEE ALSO:
 * ioremap(), ioremap_nocache()
 * 
 */
/* _VMKLNX_CODECHECK_: __writeq */
static inline void __writeq(__u64 b, volatile void __iomem *addr)
{
	*(__force volatile __u64 *)addr = b;
}

/**
 *  __writeb - write an u8 value to I/O device memory
 *  @b: the u8 value to be written
 *  @addr: the iomapped memory address that is obtained from ioremap() or from ioremap_nocache()
 *
 *  This is an internal function. Please call writeb instead.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: __writeb */
static inline void __writeb(__u8 b, volatile void __iomem *addr)
{
	*(__force volatile __u8 *)addr = b;
}

/**
 *  __writew - write an u16 value to I/O device memory
 *  @b: the u16 value to be written
 *  @addr: the iomapped memory address that is obtained from ioremap() or from ioremap_nocache()
 *
 *  This is an internal function. Please call writew instead.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: __writew */
static inline void __writew(__u16 b, volatile void __iomem *addr)
{
	*(__force volatile __u16 *)addr = b;
}
#define writeq(val,addr) __writeq((val),(addr))
/**
 *  writel - write an u32 value to I/O device memory
 *  @val: the u32 value to be written
 *  @addr: the iomapped memory address that is generated by ioremap() or ioremap_nocache()
 *
 *  Write an u32 value to I/O device memory.
 *
 *  SYNOPSIS:
 *  #define writel(val,addr) 
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: writel */
#define writel(val,addr) __writel((val),(addr))

/**
 *  writew - write an u16 value to I/O device memory
 *  @val: the u16 value to be written
 *  @addr: the iomapped memory address that is obtained from ioremap() or from ioremap_nocache()
 *
 *  Write an u16 value to I/O device memory.
 *
 *  SYNOPSIS:
 *  #define writew(val,addr)
 * 
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: writew */
#define writew(val,addr) __writew((val),(addr))

/**
 *  writeb - write an u8 value to I/O device memory
 *  @val: the u8 value to be written
 *  @addr: the iomapped memory address that is generated by ioremap() or ioremap_nocache()
 *
 *  Write an u8 value to I/O device memory.
 *
 *  SYNOPSIS:
 *  #define writeb(val,addr)
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 *  SEE ALSO:
 *      ioremap(), ioremap_nocache()
 *
 */
/* _VMKLNX_CODECHECK_: writeb */
#define writeb(val,addr) __writeb((val),(addr))
#define __raw_writeb writeb
#define __raw_writew writew
#define __raw_writel writel
#define __raw_writeq writeq

#if defined(__VMKLNX__)
/**                                          
 *  memcpy_fromio - copy from an IO device space to main memory space       
 *  @from: the address for the IO Device in PCI space
 *  @to: the address to where the copy is to happen
 *  @len: the number of bytes to be copied
 *
 *  Copy @len bytes from @from address of an IO device in the PCI space to 
 *  @to address in main memory space
 *                                           
 *  RETURN VALUE:
 *  None                                         
 */                                          
/* _VMKLNX_CODECHECK_: memcpy_fromio */
static inline void memcpy_fromio(void *to, const volatile void __iomem *from, unsigned len)
{
        vmk_Memcpy(to, (void *)from, len);
}

/**
  *  memcpy_toio - copy from main memory space to IO device space
  *  @to: the address for the IO Device in PCI space
  *  @from: the address from where the copy is to happen
  *  @len: the number of bytes to be copied
  *
  *  Copy @len bytes from @from address to @to address of a IO Device in the PCI
  *  space.
  * 
  *  RETURN VALUE
  *  None
  */
/* _VMKLNX_CODECHECK_: memcpy_toio */
static inline void memcpy_toio(volatile void __iomem *to, const void *from, unsigned len)
{
        vmk_Memcpy((void *)to, from, len);
}
#else /* !defined(__VMKLNX__) */
void __memcpy_fromio(void*,unsigned long,unsigned);
void __memcpy_toio(unsigned long,const void*,unsigned);

static inline void memcpy_fromio(void *to, const volatile void __iomem *from, unsigned len)
{
	__memcpy_fromio(to,(unsigned long)from,len);
}
static inline void memcpy_toio(volatile void __iomem *to, const void *from, unsigned len)
{
	__memcpy_toio((unsigned long)to,from,len);
}
#endif /* defined(__VMKLNX__) */

void memset_io(volatile void __iomem *a, int b, size_t c);

/*
 * ISA space is 'always mapped' on a typical x86 system, no need to
 * explicitly ioremap() it. The fact that the ISA IO space is mapped
 * to PAGE_OFFSET is pure coincidence - it does not mean ISA values
 * are physical addresses. The following constant pointer can be
 * used as the IO-area pointer (it can be iounmapped as well, so the
 * analogy with PCI is quite large):
 */
#define __ISA_IO_base ((char __iomem *)(PAGE_OFFSET))

/*
 * Again, x86-64 does not require mem IO specific function.
 */

#define eth_io_copy_and_sum(a,b,c,d)		eth_copy_and_sum((a),(void *)(b),(c),(d))

/**
 *	check_signature		-	find BIOS signatures
 *	@io_addr: mmio address to check 
 *	@signature:  signature block
 *	@length: length of signature
 *
 *	Perform a signature comparison with the mmio address io_addr. This
 *	address should have been obtained by ioremap.
 *	Returns 1 on a match.
 */
 
static inline int check_signature(void __iomem *io_addr,
	const unsigned char *signature, int length)
{
	int retval = 0;
	do {
		if (readb(io_addr) != *signature)
			goto out;
		io_addr++;
		signature++;
		length--;
	} while (length);
	retval = 1;
out:
	return retval;
}

/* Nothing to do */

#define dma_cache_inv(_start,_size)		do { } while (0)
#define dma_cache_wback(_start,_size)		do { } while (0)
#define dma_cache_wback_inv(_start,_size)	do { } while (0)

#define flush_write_buffers() 

extern int iommu_bio_merge;
#define BIO_VMERGE_BOUNDARY iommu_bio_merge

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

#endif /* __KERNEL__ */

#endif
