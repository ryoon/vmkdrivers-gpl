/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Platform                                                       */ /**
 * \defgroup Platform Platform
 *
 * Interfaces relating to the underlying platform.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PLATFORM_H_
#define _VMKAPI_PLATFORM_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Size of an L1 cacheline */
#define VMK_L1_CACHELINE_SIZE       64

/** \brief Type that is large enough to store CPU flags */
typedef vmk_uintptr_t vmk_CPUFlags;

/*
 ***********************************************************************
 * VMK_ATTRIBUTE_L1_ALIGNED --                                    */ /**
 * \ingroup Compiler
 *
 * \brief Indicate to the compiler that a data structure should be
 *        aligned on an L1 cacheline boundary.
 *
 ***********************************************************************
 */
#define VMK_ATTRIBUTE_L1_ALIGNED VMK_ATTRIBUTE_ALIGN(VMK_L1_CACHELINE_SIZE)

/*
 ***********************************************************************
 * vmk_CPUDisableInterrupts --                                    */ /**
 *
 * \ingroup Platform
 * \brief Disable interrupts on the current CPU.
 *
 * \note  This function will not block.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
void vmk_CPUDisableInterrupts(void);

/*
 ***********************************************************************
 * vmk_CPUEnableInterrupts --                                    */ /**
 *
 * \ingroup Platform
 * \brief Enable interrupts on the current CPU.
 *
 * \note  This function will not block.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
void vmk_CPUEnableInterrupts(void);

/*
 ***********************************************************************
 * vmk_CPUHasIntsEnabled --                                       */ /**
 *
 * \ingroup Platform
 * \brief Check whether interrupts are enabled on the current CPU.
 *
 * \note  This function will not block.
 *
 * \retval VMK_TRUE  Interrupts are enabled on the current CPU.
 * \retval VMK_FALSE Interrupts are disabled on the current CPU.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
vmk_Bool vmk_CPUHasIntsEnabled(void);

/*
 ***********************************************************************
 * VMK_ASSERT_CPU_HAS_INTS_ENABLED --                             */ /**
 *
 * \ingroup Platform
 * \brief Assert that interrupts are enabled on the current CPU.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
#define VMK_ASSERT_CPU_HAS_INTS_ENABLED() \
      VMK_ASSERT(vmk_CPUHasIntsEnabled())

/*
 ***********************************************************************
 * VMK_ASSERT_CPU_HAS_INTS_DISABLED --                            */ /**
 *
 * \ingroup Platform
 * \brief Assert that interrupts are disabled on the current CPU.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
#define VMK_ASSERT_CPU_HAS_INTS_DISABLED() \
      VMK_ASSERT(!vmk_CPUHasIntsEnabled())

/*
 ***********************************************************************
 * vmk_CPUGetFlags --                                             */ /**
 *
 * \ingroup Platform
 * \brief Get the current CPU's interrupt flags.
 *
 * \note  This function will not block.
 *
 * \return The current CPU's interrupt flags.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
vmk_CPUFlags vmk_CPUGetFlags(void);

/*
 ***********************************************************************
 * vmk_CPUSetFlags --                                             */ /**
 *
 * \ingroup Platform
 * \brief Restore the current CPU's interrupt flags
 *
 * \note  This function will not block.
 *
 * \nativedriversdisallowed
 *
 ***********************************************************************
 */
void vmk_CPUSetFlags(
   vmk_CPUFlags flags);


/*
 ***********************************************************************
 * vmk_CPUEnsureClearDF --                                        */ /**
 *
 * \ingroup Platform
 * \brief Ensures that the DF bit is clear.
 *
 * This is useful for instructions like outs, ins, scas, movs, stos,
 * cmps, lods which look at DF.
 *
 * \deprecated Should never be needed in pure C code. If cld is needed
 *             in an assembly code block, write it explicitly along with
 *             the other assembly instructions.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_CPUEnsureClearDF(void)
{
   __asm__ __volatile__ ("cld\n\t");
}

/*
 ***********************************************************************
 * vmk_CPUMemFenceRead --                                         */ /**
 *
 * \ingroup Platform
 * \brief Ensure that all prior loads have completed before loads
 *        following the fence.
 *
 * \note This fence does not have any implication on the ordering of
 *       store instructions.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_CPUMemFenceRead(void)
{
   asm volatile ("lfence" ::: "memory");
}

/*
 ***********************************************************************
 * vmk_CPUMemFenceWrite --                                        */ /**
 *
 * \ingroup Platform
 * \brief Ensure that all prior stores have completed and are globally
 *        visible before stores following the fence.
 *
 * \note This fence does not have any implication on the ordering of
 *       load instructions.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_CPUMemFenceWrite(void)
{
   asm volatile ("sfence" ::: "memory");
}

/*
 ***********************************************************************
 * vmk_CPUMemFenceReadWrite --                                    */ /**
 *
 * \ingroup Platform
 * \brief Ensure that all prior loads and stores have completed and are
 *        globally visible before loads and stores following the fence.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_CPUMemFenceReadWrite(void)
{
   asm volatile ("mfence" ::: "memory");
}

/*
 ***********************************************************************
 * vmk_CPUCacheFlush --                                           */ /**
 *
 * \ingroup Platform
 * \brief Flushes the cache lines on the local PCPU for the memory
 *        region specified.
 *
 * \note If a specific operation required flushing the cache line of
 *       the local PCPU, both that operation and this call should
 *       come within the same VMK_WITH_PCPU_DO region.
 *
 * \param[in] pcpu        PCPU ID obtained from VMK_WITH_PCPU_DO.
 * \param[in] va          Starting virtual address of memory region.
 * \param[in] size        Size of memory region.
 *
 * \retval VMK_OK         Cache lines successfully flushed.
 * \retval VMK_BAD_PARAM  An invalid parameter was specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CPUCacheFlush(
   vmk_PCPUID pcpu,
   vmk_VA va,
   vmk_ByteCount size);

/*
 ***********************************************************************
 * vmk_CPUGlobalCacheFlush --                                     */ /**
 *
 * \ingroup Platform
 * \brief Flushes all modified cache lines on all PCPUs.
 *
 * \note This function will block.
 *
 * \note This is an expensive operation that should be used rarely and
 *       not at all on fast paths.
 *
 * \retval VMK_OK       Cachelines successfully flushed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CPUGlobalCacheFlush(void);

#endif /* _VMKAPI_PLATFORM_H_ */
/* @} */
