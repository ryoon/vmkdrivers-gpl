/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Atomics                                                        */ /**
 * \defgroup Atomics Atomic Operations
 *
 * Interfaces to CPU atomic operations.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_ATOMIC_H_
#define _VMKAPI_ATOMIC_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/***
 * \brief Atomic unsigned 64 bit integer
 *
 * \warning Operations on unaligned vmk_atomic64's are not guaranteed to
 *          be atomic.
 *
 * \note Be aware that some compilers will not obey a type's alignment
 *       if it is embedded in a packed structure.
 */
typedef volatile vmk_uint64 vmk_atomic64
   VMK_ATTRIBUTE_ALIGN(sizeof(vmk_uint64));

/**
 * \brief Read Fence Indicator for Atomic Memory Access
 *
 * If this global is set to true, then CPU atomic operations should
 * be followed by a read fence (i.e. "lfence" instruction for the
 * x64-64 architecture).
 *
 * \note This variable should only be used in assembly language routines
 *       or fragments. C routines should utilize vmk_AtomicPrologue()
 *       and vmk_AtomicEpilogue().
 */
extern vmk_Bool vmk_AtomicUseFence;

/*
 ***********************************************************************
 * vmk_AtomicPrologue --                                          */ /**
 *
 * \ingroup Atomics
 * \brief Setup for atomic operations
 *
 * This routine should be invoked before atomic operations to allow
 * for errata work-arounds.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_AtomicPrologue(
   void)
{
   /* Nothing for now */
}

/*
 ***********************************************************************
 * vmk_AtomicEpilogue --                                          */ /**
 *
 * \ingroup Atomics
 * \brief Finish atomic operations
 *
 * This routine should be invoked after atomic operations to allow
 * for errata work-arounds.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE void vmk_AtomicEpilogue(
   void)
{
   /* Nothing for now (and we hope to keep it this way). */
}

/*
 ***********************************************************************
 * vmk_AtomicInc64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically increment 
 *
 * \param[out] var   Atomic to increment.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicInc64(
   vmk_atomic64 *var)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; incq %0"
      : "+m" (*var)
      :
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicDec64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically decrement
 *
 * \param[out] var   Atomic to decrement.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicDec64(
   vmk_atomic64 *var)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; decq %0"
      : "+m" (*var)
      :
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicAdd64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically add
 *
 * \param[out] var   Atomic to add to.
 * \param[in]  val   Value to add to atomic.
 * \return None.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicAdd64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; addq %1, %0"
      : "+m" (*var)
      : "re" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicSub64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically subtract
 *
 * \param[out] var   Atomic to subtract from.
 * \param[in]  val   Value to subtract from atomic.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicSub64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; subq %1, %0"
      : "+m" (*var)
      : "re" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicOr64 --                                              */ /**
 *
 * \ingroup Atomics
 * \brief Atomically bitwise OR
 *
 * \param[out] var   Atomic to OR
 * \param[in] val    Value to OR
 * \return None.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicOr64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; orq %1, %0"
      : "+m" (*var)
      : "re" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicAnd64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically bitwise AND
 *
 * \param[out] var   Atomic to AND.
 * \param[in]  val   Value to AND.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicAnd64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; andq %1, %0"
      : "+m" (*var)
      : "re" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicXor64 --                                             */ /**
 *
 * \ingroup Atomics
 * \brief Atomically bitwise XOR
 *
 * \param[out] var   Atomic to XOR.
 * \param[in] val    Value to XOR.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicXor64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; xorq %1, %0"
      : "+m" (*var)
      : "re" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();
}

/*
 ***********************************************************************
 * vmk_AtomicRead64 --                                            */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read
 *
 * \param[out] var   Atomic to read.
 *
 * \return Value of the atomic.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicRead64(
   vmk_atomic64 const *var)
{
   /* Ensure alignment otherwise this isn't atomic. */
   VMK_ASSERT(((vmk_uintptr_t)var & (vmk_uintptr_t)(sizeof(*var) - 1)) == 0);
   return *var;
}

/*
 ***********************************************************************
 * vmk_AtomicWrite64 --                                           */ /**
 *
 * \ingroup Atomics
 * \brief Atomically write
 *
 * \param[out] var   Atomic to write.
 * \param[in]  val   Value to write.
 *
 ***********************************************************************
 */
static inline void vmk_AtomicWrite64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   /* Ensure alignment otherwise this isn't atomic. */
   VMK_ASSERT(((vmk_uintptr_t)var & (vmk_uintptr_t)(sizeof(*var) - 1)) == 0);

   /*
    * Ensure that we do a single movq. Without this, the compiler
    * may do write with a constant as two movl operations.
    */
   __asm__ __volatile__(
      "movq %1, %0"
      : "=m" (*var)
      : "r" (val)
   );
}

/*
 ***********************************************************************
 * vmk_AtomicReadWrite64 --                                       */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read and write
 *
 * \param[out] var   Atomic to read.
 * \param[in]  val   Value to write.
 *
 * \return Value of the atomic before written.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadWrite64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_AtomicPrologue();
   __asm__ __volatile__(
      /* The lock prefix is not strictly necesessary but it is imformative. */
      "lock; xchgq %0, %1"
      : "=r" (val),
	"+m" (*var)
      : "0" (val)
   );
   vmk_AtomicEpilogue();

   return val;
}

/*
 ***********************************************************************
 * vmk_AtomicReadIfEqualWrite64 --                                */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read, compare, and conditionally write
 *
 * Atomically writes new to var if its current value is old.
 * The value returned is old if new was written, else
 * the current unmodified value of the atomic is returned
 *
 * \param[out] var   Atomic to read.
 * \param[in]  old   Value to compare.
 * \param[in]  new   Value to write.
 *
 * \return Value of the atomic before written.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadIfEqualWrite64(
   vmk_atomic64 *var,
   vmk_uint64 old,
   vmk_uint64 new)
{
   vmk_uint64 val;

   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; cmpxchgq %2, %1"
      : "=a" (val),
	"+m" (*var)
      : "r" (new),
	"0" (old)
      : "cc"
   );
   vmk_AtomicEpilogue();

   return val;
}

/*
 ***********************************************************************
 * vmk_AtomicReadAdd64 --                                         */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read and add
 *
 * \param[out] var   Atomic to read.
 * \param[in]  val   Value to add.
 *
 * \return Value of the atomic before addition.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadAdd64(
  vmk_atomic64 *var,
  vmk_uint64 val)
{

   vmk_AtomicPrologue();
   __asm__ __volatile__(
      "lock; xaddq %0, %1"
      : "=r" (val),
	"+m" (*var)
      : "0" (val)
      : "cc"
   );
   vmk_AtomicEpilogue();

   return val;
}

/*
 ***********************************************************************
 * vmk_AtomicReadInc64 --                                         */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read and increment
 *
 * \param[out] var   Atomic to read.
 *
 * \return Value of the atomic before incremented.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadInc64(
   vmk_atomic64 *var)
{
   return vmk_AtomicReadAdd64(var, 1);
}

/*
 ***********************************************************************
 * vmk_AtomicReadDec64 --                                         */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read and decrement
 *
 * \param[out] var   Atomic to read.
 *
 * \return Value of the atomic before decremented.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadDec64(
   vmk_atomic64 *var)
{
   return vmk_AtomicReadAdd64(var, VMK_CONST64U(-1));
}

/*
 ***********************************************************************
 * vmk_AtomicReadOr64 --                                          */ /**
 *
 * \ingroup Atomics
 * \brief Atomically read and bitwise OR with a value
 *
 * \param[out] var   Atomic to read/OR.
 * \param[in]  val   Value to OR atomically.
 *
 * \return Value of the atomic before the operation.
 *
 ***********************************************************************
 */
static inline vmk_uint64 vmk_AtomicReadOr64(
   vmk_atomic64 *var,
   vmk_uint64 val)
{
   vmk_uint64 res;

   do {
      res = vmk_AtomicRead64(var);
   } while (res != vmk_AtomicReadIfEqualWrite64(var, res, res | val));

   return res;
}

#endif /* _VMKAPI_ATOMIC_H_ */
/** @} */
