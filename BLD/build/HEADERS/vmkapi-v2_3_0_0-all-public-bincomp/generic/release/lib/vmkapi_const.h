/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Constants                                                      */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Constants Constants
 *
 * Useful constants
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_CONST_H_
#define _VMKAPI_CONST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#if !defined(NULL)
# ifndef __cplusplus
#  define NULL (void *)0
# else
#  define NULL __null
# endif
#endif

#if defined(VMX86_DEBUG)
#  define vmkApiDebug 1
#else
#  define vmkApiDebug 0
#endif

/**
 *  \brief Wrapper for 64 bit signed and unsigned constants
 */
#if defined(__ia64__) || defined(__x86_64__)
#define VMK_CONST64(c)     c##L
#define VMK_CONST64U(c)    c##UL
#else
#define VMK_CONST64(c)     c##LL
#define VMK_CONST64U(c)    c##ULL
#endif

/** Max length of a device name such as 'scsi0' */
#define VMK_DEVICE_NAME_MAX_LENGTH	32

/** Printf format for a 64 bit wide value */
#if defined(__x86_64__)
#define VMK_FMT64 "l"
#else
#define VMK_FMT64 "L"
#endif

/** \brief vmk_Bool FALSE value */
#define VMK_FALSE 0

/** \brief vmk_Bool TRUE value */
#define VMK_TRUE  1

#define VMK_KILOBYTE (1024)
#define VMK_MEGABYTE (1024 * VMK_KILOBYTE)
#define VMK_GIGABYTE (1024 * VMK_MEGABYTE)
#define VMK_TERABYTE (VMK_CONST64U(1024) * VMK_GIGABYTE)
#define VMK_PETABYTE (VMK_CONST64U(1024) * VMK_TERABYTE)
#define VMK_EXABYTE (VMK_CONST64U(1024) * VMK_PETABYTE)

#define VMK_INT8_MIN   ((vmk_int8)0x80)
#define VMK_INT8_MAX   ((vmk_int8)0x7f)

#define VMK_UINT8_MIN  ((vmk_uint8)0)
#define VMK_UINT8_MAX  ((vmk_uint8)0xff)

#define VMK_INT16_MIN  ((vmk_int16)0x8000)
#define VMK_INT16_MAX  ((vmk_int16)0x7fff)

#define VMK_UINT16_MIN ((vmk_uint16)0)
#define VMK_UINT16_MAX ((vmk_uint16)0xffff)

#define VMK_INT32_MIN  ((vmk_int32)0x80000000)
#define VMK_INT32_MAX  ((vmk_int32)0x7fffffff)

#define VMK_UINT32_MIN ((vmk_uint32)0)
#define VMK_UINT32_MAX ((vmk_uint32)0xffffffff)

#define VMK_INT64_MIN  ((vmk_int64)VMK_CONST64(0x8000000000000000))
#define VMK_INT64_MAX  ((vmk_int64)VMK_CONST64(0x7fffffffffffffff))

#define VMK_UINT64_MIN ((vmk_uint64)VMK_CONST64U(0))
#define VMK_UINT64_MAX ((vmk_uint64)VMK_CONST64U(0xffffffffffffffff))


/**
 * \brief Bits per byte.
 */
#ifndef VMK_BITS_PER_BYTE
#define VMK_BITS_PER_BYTE (8)
#endif


#endif /* _VMKAPI_CONST_H_ */
/** @} */
/** @} */
