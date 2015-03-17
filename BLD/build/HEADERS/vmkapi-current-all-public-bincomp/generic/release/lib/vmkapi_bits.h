/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Bits                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Bits Bit Manipulation
 *
 * Utility interfaces for bit shifting, flipping, etc. 
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_BITS_H_
#define _VMKAPI_BITS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_CPUToBE64 --                                               */ /**
 *
 * \brief Convert an unsigned 64 bit unsigned integer in the
 *        CPU native endian format into a big endian 64 bit
 *        unsigned integer.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_BE64ToCPU 
 *
 ***********************************************************************
 */
static inline vmk_uint64
vmk_CPUToBE64(vmk_uint64 x)
{
#if defined(__x86_64__) && defined(__GNUC__)
   if (!__builtin_constant_p(x)) {
      __asm__ ("bswapq %0"
               : "=r" (x)
               : "0" (x));
      return x;
   }
#endif
   return ((x >> 56) & VMK_CONST64U(0x00000000000000FF)) |
          ((x >> 40) & VMK_CONST64U(0x000000000000FF00)) |
          ((x >> 24) & VMK_CONST64U(0x0000000000FF0000)) |
          ((x >> 8)  & VMK_CONST64U(0x00000000FF000000)) | 
          ((x << 8)  & VMK_CONST64U(0x000000FF00000000)) |
          ((x << 24) & VMK_CONST64U(0x0000FF0000000000)) |
          ((x << 40) & VMK_CONST64U(0x00FF000000000000)) |
          ((x << 56) & VMK_CONST64U(0xFF00000000000000));
}

/*
 ***********************************************************************
 * vmk_BE64ToCPU --                                               */ /**
 *
 * \brief Convert an unsigned 64 bit unsigned integer in the
 *        big endian format into a 64 bit unsigned integer in
 *        the native CPU endian format.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_CPUToBE64 
 *
 ***********************************************************************
 */
static inline vmk_uint64
vmk_BE64ToCPU(vmk_uint64 x)
{
   return vmk_CPUToBE64(x);
}

/*
 ***********************************************************************
 * vmk_CPUToBE32 --                                               */ /**
 *
 * \brief Convert an unsigned 32 bit unsigned integer in the
 *        CPU native endian format into a big endian 32 bit
 *        unsigned integer.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_BE32ToCPU 
 *
 ***********************************************************************
 */
static inline vmk_uint32
vmk_CPUToBE32(vmk_uint32 x)
{
#if  defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   if (!__builtin_constant_p(x)) {
      __asm__ ("bswap %0"
               : "=r" (x)
               : "0" (x));
      return x;
   }
#endif
   return ((x >> 24) & 0x000000ff) | ((x >> 8)  & 0x0000ff00) | 
          ((x << 8)  & 0x00ff0000) | ((x << 24) & 0xff000000);
}

/*
 ***********************************************************************
 * vmk_BE32ToCPU --                                               */ /**
 *
 * \brief Convert an unsigned 32 bit unsigned integer in the
 *        big endian format into a 32 bit unsigned integer in
 *        the native CPU endian format.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_CPUToBE32 
 *
 ***********************************************************************
 */
static inline vmk_uint32
vmk_BE32ToCPU(vmk_uint32 x)
{
   return vmk_CPUToBE32(x);
}

/*
 ***********************************************************************
 * vmk_CPUToBE16 --                                               */ /**
 *
 * \brief Convert an unsigned 16 bit unsigned integer in the
 *        CPU native endian format into a big endian 16 bit
 *        unsigned integer.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_BE16ToCPU 
 *
 ***********************************************************************
 */
static inline vmk_uint16
vmk_CPUToBE16(vmk_uint16 x)
{
#if  defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   if (!__builtin_constant_p(x)) {
      int bits = 8;
      __asm__ ("rolw %%cl, %0"
               : "=r" (x)
               : "0" (x), "c" (bits));
      return x;
   }
#endif
   return ((x >> 8) & 0x00ff) | ((x << 8) & 0xff00);
}

/*
 ***********************************************************************
 * vmk_BE16ToCPU --                                               */ /**
 *
 * \brief Convert an unsigned 16 bit unsigned integer in the
 *        big endian format into a 16 bit unsigned integer in
 *        the native CPU endian format.
 *
 * \param[in] x Value to swap
 *
 * \return Swapped value.
 *
 * \sa vmk_CPUToBE16 
 *
 ***********************************************************************
 */
static inline vmk_uint16
vmk_BE16ToCPU(vmk_uint16 x)
{
   return vmk_CPUToBE16(x);
}

#endif /* _VMKAPI_BITS_H_ */
/** @} */
/** @} */
