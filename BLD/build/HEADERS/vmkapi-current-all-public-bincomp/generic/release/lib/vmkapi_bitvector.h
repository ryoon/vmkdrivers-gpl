/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * BitVector                                                      */ /**
 *
 * \addtogroup Lib
 * @{
 * \defgroup vmk_BitVector Bit Vector Manipulation
 *
 * Utility interfaces for managing a Bit Vector.

 * \par Example - Using VMK_BITVECTOR_ITERATE or VMK_BITVECTOR_ITERATE_AND_CLEAR
 *      and VMK_BITVECTOR_ENDITERATE
 * 
 * \code
 * {
 *    vmk_uint32 n;
 *    VMK_BITVECTOR_ITERATE(_bv,n) {
 *       printf("%d is in set\n",n);
 *    }
 *    VMK_BITVECTOR_ENDITERATE()
 * }
 * \endcode
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_BITVECTOR_H_
#define _VMKAPI_BITVECTOR_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief Incomplete Abstract Data type allocated to be larger than
 *        sizeof(vmk_BitVector). Should only be allocated by
 *        vmk_BitVectorAlloc.
 *        Clients should only use the returned pointer.
 */
typedef struct vmk_BitVector {
   vmk_uint32   n;
   vmk_uint32   nwords;
   vmk_uint32   vector[1];
} vmk_BitVector;


/*
 *******************************************************************************
 * vmk_BitVectorSize --                                                   */ /**
 *
 * \brief    Size of vmk_BitVector for a given number of bits
 *
 * \param[in]  n    Number of bits to size for
 *
 * \retval size calculation of a vmk_BitVector
 *
 *******************************************************************************
 */
#define vmk_BitVectorSize(n) \
   (sizeof(vmk_uint32)                       /* n */ +                        \
    sizeof(vmk_uint32)                       /* nwords */ +                   \
    ((n + 31) / 32) * sizeof(vmk_uint32))    /* n bits rounded up to words */


/*
 *******************************************************************************
 * vmk_BitVectorAllocWithTimeout --                                       */ /**
 *
 * \brief    Allocate a bitvector with timeout
 *
 * \param[in]  heap        vmk_HeapID to allocate the vmk_BitVector from
 * \param[in]  n           Total number of bits to allocate for
 * \param[in]  timeoutMs   vmk_BitVector to set a bit
 *
 * \note If timeoutMS is not 0 then the allocation may block if the
 *       heap needs to expand to accomodate the request.
 *
 * \retval vmk_BitVector pointer or NULL.
 *
 *******************************************************************************
 */
vmk_BitVector* vmk_BitVectorAllocWithTimeout(vmk_HeapID heap,
                                             vmk_uint32 n,
                                             vmk_uint32 timeoutMs);


/*
 *******************************************************************************
 * vmk_BitVectorAlloc --                                                  */ /**
 *
 * \brief    Allocate a bitvector
 *
 * \param[in]  heap        vmk_HeapID to allocate the vmk_BitVector from
 * \param[in]  n           Total number of bits to allocate for
 *
 * \retval vmk_BitVector pointer or NULL.
 *
 *******************************************************************************
 */
vmk_BitVector* vmk_BitVectorAlloc(vmk_HeapID heap,
                                  vmk_uint32 n);

/*
 *******************************************************************************
 * vmk_BitVectorDuplicate --                                              */ /**
 *
 * \brief    Allocate a vmk_BitVector and initialise it from another vmk_BitVector
 *
 * \param[in]  heap     vmk_HeapID to allocate the vmk_BitVector from
 * \param[in]  src      Source vmk_BitVector to copy into the new vmk_BitVector
 *
 * \retval new vmk_BitVector pointer
 *
 *******************************************************************************
 */
vmk_BitVector* vmk_BitVectorDuplicate(vmk_HeapID heap,
                                      const vmk_BitVector *src);


/*
 *******************************************************************************
 * vmk_BitVectorFree --                                                   */ /**
 *
 * \brief    Free a vmk_BitVector
 *
 * \param[in]  heap   vmk_HeapID to free the vmk_BitVector to
 * \param[in]  bv     vmk_BitVector to free
 *
 * \retval If the previous value of this bit was zero then zero is returned
 *         otherwise non-zero is returned.
 *
 *******************************************************************************
 */
void vmk_BitVectorFree(vmk_HeapID heap,
                       vmk_BitVector *bv);


/*
 *******************************************************************************
 * vmk_BitVectorSet --                                                    */ /**
 *
 * \brief    Set a bit in a vmk_BitVector
 *
 * \param[in]  bv     vmk_BitVector to set a bit
 * \param[in]  n      Bit to set
 *
 *******************************************************************************
 */
static inline void
vmk_BitVectorSet(vmk_BitVector *bv,
                 vmk_uint32 n)
{
   VMK_ASSERT(n < bv->n);
   asm volatile("btsl %1, (%0)"
                :: "r" (bv->vector), "r" (n)
                : "cc", "memory");
}


/*
 *******************************************************************************
 * vmk_BitVectorClear --                                                  */ /**
 *
 * \brief    Clear a bit in a vmk_BitVector
 *
 * \param[in]  bv     vmk_BitVector to clear a bit
 * \param[in]  n      Bit to set
 *
 *******************************************************************************
 */
static inline void
vmk_BitVectorClear(vmk_BitVector *bv,
                   vmk_uint32 n)
{
   VMK_ASSERT(n < bv->n);
   asm volatile("btrl %1, (%0)"
                :: "r" (bv->vector), "r" (n)
                : "cc", "memory");
}


/*
 *******************************************************************************
 * vmk_BitVectorTest --                                                   */ /**
 *
 * \brief    Test if a bit in a vmk_BitVector is set
 *
 * \param[in]  bv     vmk_BitVector to check
 * \param[in]  n      Bit to check
 *
 * \retval If this bit is set, non-zero is returned.
 *
 *******************************************************************************
 */
static inline int
vmk_BitVectorTest(const vmk_BitVector *bv,
                  vmk_uint32 n)
{
   VMK_ASSERT(n < bv->n);
   {
      vmk_uint32 tmp;

      asm("btl  %2, (%1); "
          "sbbl %0, %0"
          : "=r" (tmp)
          : "r" (bv->vector), "r" (n)
          : "cc");
      return tmp;
   }
}


/*
 *******************************************************************************
 * vmk_BitVectorAtomicTestAndSet --                                       */ /**
 *
 * \brief    Atomically read bit n and set it
 *
 * \param[in]  bv     vmk_BitVector to set a bit
 * \param[in]  n      Bit to set
 *
 * \retval If the previous value of this bit was zero then zero is returned
 *         otherwise non-zero is returned.
 *
 *******************************************************************************
 */
static inline int
vmk_BitVectorAtomicTestAndSet(const vmk_BitVector *bv,
                              vmk_uint32 n)
{
   VMK_ASSERT(n < bv->n);
   {
      vmk_uint32 tmp;

      asm volatile("lock; btsl %2, (%1); "
                   "sbbl %0, %0"
                   : "=r" (tmp)
                   : "r" (bv->vector), "r" (n)
                   : "cc", "memory");

      return tmp;
   }
}


/*
 *******************************************************************************
 * vmk_BitVectorAtomicTestAndClear --                                     */ /**
 *
 * \brief    Atomically read bit n and clear it
 *
 * \param[in]  bv     vmk_BitVector to operate on
 * \param[in]  n      Bit to clear
 *
 * \retval If the previous value of this bit was zero then zero is returned
 *         otherwise non-zero is returned.
 *
 *******************************************************************************
 */
static inline int
vmk_BitVectorAtomicTestAndClear(const vmk_BitVector *bv,
                                vmk_uint32 n)
{
   VMK_ASSERT(n < bv->n);
   {
      vmk_uint32 tmp;

      asm volatile("lock; btrl %2, (%1); "
                   "sbbl %0, %0"
                   : "=r" (tmp)
                   : "r" (bv->vector), "r" (n)
                   : "cc", "memory");

      return tmp;
   }
}


/*
 *******************************************************************************
 * vmk_BitVectorZap --                                                    */ /**
 *
 * \brief    Removes all entries from the set
 *
 * \param[in]  bv     vmk_BitVector to zap
 *
 *******************************************************************************
 */
static inline void
vmk_BitVectorZap(vmk_BitVector *bv)
{
   vmk_Memset(bv->vector, 0, bv->nwords * sizeof(bv->vector[0]));
}


/*
 *******************************************************************************
 * vmk_BitVectorFill --                                                   */ /**
 *
 * \brief    Set all bits in a vmk_BitVector
 *
 * \param[in]  bv     vmk_BitVector to operate on
 *
 *******************************************************************************
 */
static inline void
vmk_BitVectorFill(vmk_BitVector *bv)
{
   vmk_Memset(bv->vector, 0xff, bv->nwords * sizeof(bv->vector[0]));
}


/*
 *******************************************************************************
 * vmk_BitVectorGetRef --                                                 */ /**
 *
 * \brief    Get a pointer to a particular byte in vmk_BitVector
 *
 * \param[in]  bv       vmk_BitVector to look inside
 * \param[in]  start    Starting byte return
 * \param[in]  nbytes   Number of bytes expected to be used
 *
 * \note nbytes is only checked in debug builds
 *
 * \retval Pointer to a vmk_unit8.
 *
 *******************************************************************************
 */
static inline vmk_uint8 *
vmk_BitVectorGetRef(const vmk_BitVector *bv,
                    vmk_uint32 start,
                    vmk_uint32 nbytes)
{
   vmk_uint8 *ptr = (vmk_uint8 *)bv->vector;
   (void)nbytes;
   VMK_ASSERT((start + nbytes) <= bv->nwords * sizeof(bv->vector[0]));
   return &ptr[start];
}


/*
 *******************************************************************************
 * vmk_BitVectorNumBitsSet --                                             */ /**
 *
 * \brief     Return the number of set bits in this vmk_BitVector
 *
 * \param[in]  bv     vmk_BitVector to count
 *
 * \retval number of bits set in this vmk_BitVector
 *
 *******************************************************************************
 */
vmk_uint32 vmk_BitVectorNumBitsSet(const vmk_BitVector *bv);


/*
 *******************************************************************************
 * vmk_BitVectorIsZero --                                                 */ /**
 *
 * \brief    Check if a vmk_BitVector has no bits set
 *
 * \param[in]  bv     vmk_BitVector to check
 *
 * \retval non-zero if any bit is set in the vmk_BitVector
 *
 *******************************************************************************
 */
vmk_uint32 vmk_BitVectorIsZero(const vmk_BitVector *bv);


/*
 *******************************************************************************
 * vmk_BitVectorNextBit --                                                */ /**
 *
 * \brief    Find next bit set in a vmk_BitVector
 *
 * \param[in]  bv      vmk_BitVector to check
 * \param[in]  start   Bit to start searching from
 * \param[in]  state   VMK_TRUE if looking for set bits
 * \param[out] pos     Filled in with index of the next bit
 *
 * \retval VMK_TRUE if a bit was found
 *
 * \note The value of "pos" must not be used if VMK_FALSE is returned.
 *
 *******************************************************************************
 */
vmk_Bool vmk_BitVectorNextBit(const vmk_BitVector *bv,
                              vmk_uint32 start,
                              vmk_Bool state,
                              vmk_uint32 *pos);


/*
 *******************************************************************************
 * vmk_BitVectorPrevBit --                                                */ /**
 *
 * \brief    Find previous bit set in a vmk_BitVector, searching backwards
 *
 * \param[in]  bv      vmk_BitVector to check
 * \param[in]  start   Bit to start searching from
 * \param[in]  state   VMK_TRUE if looking for set bits
 * \param[out] pos     Filled in with index of the next bit
 *
 * \retval VMK_TRUE if a bit was found
 *
 * \note BitVectors are implemented as an array of vmk_uint32 which affects
 *       byte ordering on little endian architectures, such as x86.
 *
 * \note The value of "pos" must not be used if VMK_FALSE is returned.
 *
 *******************************************************************************
 */
vmk_Bool vmk_BitVectorPrevBit(const vmk_BitVector *bv,
                              vmk_uint32 start,
                              vmk_Bool state,
                              vmk_uint32 *pos);


/*
 *******************************************************************************
 * vmk_BitVectorGetExtent --                                              */ /**
 *
 * \brief    Find sequence of bits in a vmk_BitVector
 *
 * \param[in]  bv       vmk_BitVector to check
 * \param[in]  start    Bit to start searching from
 * \param[out] set      Is start bit set or not
 * \param[out] length   Filled in with index of the next bit
 *
 * \note BitVectors are implemented as an array of vmk_uint32 which affects
 *       byte ordering on little endian architectures, such as x86.
 *
 *******************************************************************************
 */
void vmk_BitVectorGetExtent(const vmk_BitVector *bv,
                            vmk_uint32 start,
                            vmk_Bool *set,
                            vmk_uint32 *length);

/*
 *******************************************************************************
 * vmk_BitVectorNextExtent --                                             */ /**
 *
 * \brief    Find next sequence of bits in a vmk_BitVector
 *
 * \param[in]  bv            vmk_BitVector to check
 * \param[in]  startSearch   Bit to start searching from
 * \param[in]  set           VMK_TRUE if looking for set bits
 * \param[out] startRun      Start index of extend found
 * \param[out] length        Length of extent found
 *
 * \retval VMK_TRUE if a bit was found
 *
 * \note BitVectors are implemented as an array of vmk_uint32 which affects
 *       byte ordering on little endian architectures, such as x86.
 *
 * \note The values of "startRun" and "length" must not be used if VMK_FALSE
 *       is returned.
 *
 *******************************************************************************
 */
vmk_Bool vmk_BitVectorNextExtent(const vmk_BitVector *bv,
                                 vmk_uint32 startSearch,
                                 vmk_Bool set,
                                 vmk_uint32 *startRun,
                                 vmk_uint32 *length);


/*
 *******************************************************************************
 * vmk_BitVectorSetExtent --                                              */ /**
 *
 * \brief    Set an extent of a bitvector to a particular state
 *
 * \param[in]  bv         vmk_BitVector to set a bit
 * \param[in]  startRun   Bit to start setting from
 * \param[in]  length     Number of bits to set
 * \param[in]  state      VMK_TRUE if setting to 1
 *
 * \retval If setting to 1, returns a positive count number of bits set,
 *         otherwise returns a negative count of the number of bits set.
 *
 *******************************************************************************
 */
int vmk_BitVectorSetExtent(vmk_BitVector *bv,
                           vmk_uint32 startRun,
                           vmk_uint32 length,
                           vmk_Bool state);


/*
 *******************************************************************************
 * vmk_BitVectorSetExtentFast --                                          */ /**
 *
 * \brief    Set an extent of a bitvector to 1
 *
 * \param[in]  bv         vmk_BitVector to set a bit
 * \param[in]  startRun   Bit to start setting from
 * \param[in]  length     Number of bits to set
 *
 *******************************************************************************
 */
void vmk_BitVectorSetExtentFast(vmk_BitVector *bv,
                                vmk_uint32 startRun,
                                vmk_uint32 length);


/*
 *******************************************************************************
 * vmk_BitVectorMerge --                                                  */ /**
 *
 * \brief    Merge two sets of vmk_BitVector
 *
 * \param[in]      src     vmk_BitVector merged into dest
 * \param[in,out]  dest    vmk_BitVector modified by src
 *
 * \retval number of bits merged from src to dest that were not previously set.
 *
 *******************************************************************************
 */
vmk_uint32 vmk_BitVectorMerge(vmk_BitVector *src,
                              vmk_BitVector *dest);


/*
 *******************************************************************************
 * vmk_BitVectorMergeAtOffset --                                          */ /**
 *
 * \brief    Merge two sets of vmk_BitVector from specific offset
 *
 * This function merges bits from the start of one vmk_BitVector into another,
 * starting the merge at a specific offset in the destination vector.  Bits set
 * in the source vector that would go beyond the size of dest are ignored.
 *
 * \param[in]      src     vmk_BitVector merged into dest
 * \param[in,out]  dest    vmk_BitVector modified by src
 * \param[in]      offset  Bit position to write bits from src into
 *
 *******************************************************************************
 */
void vmk_BitVectorMergeAtOffset(vmk_BitVector *src,
                                vmk_BitVector *dest,
                                vmk_uint32 offset);


/*
 *******************************************************************************
 * vmk_BitVectorMaxSize --                                          */ /**
 *
 * \brief  Returns the maximum number of bits that can be held in a vmk_BitVector
 *
 * \retval maximum number of bits that can be held in a vmk_BitVector
 *
 *******************************************************************************
 */
vmk_uint32 vmk_BitVectorMaxSize(void);


/*
 *******************************************************************************
 * VMK_BITVECTOR_ITERATE --                                               */ /**
 *
 * \brief    Macro to iterate a vmk_BitVector
 *
 * \param[in]  _bv     vmk_BitVector to iterate across
 * \param[in]  _n      Variable set to each valid bit each iteration
 *
 * \retval If the previous value of this bit was zero then zero is returned
 *         otherwise non-zero is returned.
 *
 *******************************************************************************
 */

#define VMK_BITVECTOR_ITERATE(_bv,_n) {            \
   vmk_uint32 _index;                              \
   for (_index=0;_index <(_bv)->nwords;_index++) { \
      vmk_uint32 _off,_vals;                       \
      _vals = (_bv)->vector[_index];               \
      while(_vals) {                               \
          __asm ("bsfl %1,%0\n\t"                  \
                 "btrl %0,%1"                      \
                 : "=r" (_off), "+g" (_vals)       \
                 : : "cc" );                       \
         _n = (_index * 32) + _off;                \
         if (_n >= (_bv)->n) {                     \
            break;                                 \
         }                                         \


/*
 *******************************************************************************
 * VMK_BITVECTOR_ITERATE_AND_CLEAR --                                     */ /**
 *
 * \brief    Macro to iterate a vmk_BitVector, clearing it as well
 *
 * \param[in]  _bv     vmk_BitVector to iterate across
 * \param[in]  _n      Variable set to each valid bit each iteration
 *
 * \retval If the previous value of this bit was zero then zero is returned
 *         otherwise non-zero is returned.
 *
 * \note The state of the vmk_BitVector during the iteration is undefined
 *
 *******************************************************************************
 */

#define VMK_BITVECTOR_ITERATE_AND_CLEAR(_bv,_n) {  \
   vmk_uint32 _index;                              \
   for (_index=0;_index <(_bv)->nwords;_index++) { \
      vmk_uint32 _off,_vals;                       \
      _vals = (_bv)->vector[_index];               \
      (_bv)->vector[_index] = 0;                   \
      while(_vals) {                               \
          __asm ("bsfl %1,%0\n\t"                  \
                 "btrl %0,%1"                      \
                 : "=r" (_off), "+g" (_vals)       \
                 : : "cc");                        \
         _n = (_index * 32) + _off;                \
         if (_n >= (_bv)->n) {                     \
            break;                                 \
         }                                         \


/*
 *******************************************************************************
 * VMK_BITVECTOR_ENDITERATE --                                     */ /**
 *
 * \brief    Macro to end a iteration zone
 *
 * Can be used with either VMK_BITVECTOR_ITERATE() or
 * VMK_BITVECTOR_ITERATE_AND_CLEAR()
 *
 *******************************************************************************
 */

#define VMK_BITVECTOR_ENDITERATE() }}}

#endif /* _VMKAPI_BITVECTOR_H_ */
/** @} */
/** @} */
