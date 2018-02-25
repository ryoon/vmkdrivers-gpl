/* **********************************************************
 * Copyright 1998 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Types                                                          */ /**
 * LibC                                                           */ /**
 * \addtogroup Lib
 * \defgroup Types Basic Types
 * 
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_TYPES_H_
#define _VMKAPI_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Boolean value */
typedef char vmk_Bool;

typedef signed char        vmk_int8;
typedef unsigned char      vmk_uint8;
typedef short              vmk_int16;
typedef unsigned short     vmk_uint16;
typedef int                vmk_int32;
typedef unsigned int       vmk_uint32;

#if defined(__ia64__) || defined(__x86_64__)
typedef long               vmk_int64;
typedef unsigned long      vmk_uint64;
typedef vmk_uint64         vmk_uintptr_t;
#else
typedef long long          vmk_int64;
typedef unsigned long long vmk_uint64;
typedef vmk_uint32         vmk_uintptr_t;
#endif

typedef vmk_uint64         vmk_ByteCount;
typedef vmk_int64          vmk_ByteCountSigned;
typedef vmk_uint32         vmk_ByteCountSmall;
typedef vmk_int32          vmk_ByteCountSmallSigned;
typedef long long          vmk_loff_t;

#define VMK_BYTE_COUNT_MAX         VMK_UINT64_MAX
#define VMK_BYTE_COUNT_S_MAX       VMK_INT64_MAX
#define VMK_BYTE_COUNT_SMALL_MAX   VMK_UINT32_MAX
#define VMK_BYTE_COUNT_SMALL_S_MAX VMK_UINT32_MAX

/**
 * \brief Structure containing information about a generic string
 */
typedef struct {
   vmk_ByteCountSmall bufferSize;
   vmk_ByteCountSmall stringLength;
   vmk_uint8  *buffer;
} vmk_String;

#define VMK_STRING_CHECK_CONSISTENCY(string) VMK_ASSERT((string) && ((string)->bufferSize > (string)->stringLength))

#define VMK_STRING_SET(str,ptr,size,len) { \
         (str)->buffer = (ptr); \
         (str)->bufferSize = (size); \
         (str)->stringLength = (len); \
         }

/**
 * \brief Address space size of ioctl caller.
 */ 
typedef enum {
   /** \brief Caller has 64-bit address space. */
   VMK_IOCTL_CALLER_64 = 0,
   
   /** \brief Caller has 32-bit address space. */
   VMK_IOCTL_CALLER_32 = 1
} vmk_IoctlCallerSize;

#endif /* _VMKAPI_TYPES_H_ */
/** @} */
/** @} */
