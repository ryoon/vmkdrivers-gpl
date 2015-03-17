/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * String                                                         */ /**
 * \addtogroup Lib
 * @{
 * \defgroup String Interfaces
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_STRING_H_
#define _VMKAPI_STRING_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include <stdarg.h>


/*
 ***********************************************************************
 * vmk_StringCat --                                               */ /**
 *
 * \brief Appends the source string to the end of the destination string. 
 *
 * \note  This function will not block.
 *
 * \param[in] dst    String to copy to.
 * \param[in] src    String to append from.
 * \param[in] dstLen Length of destination buffer.
 *
 * \note dst will always be nul terminated so long as dstLen is greater
 *       than zero.
 *
 * \retval VMK_OK             The string was successfully concatenated.
 * \retval VMK_LIMIT_EXCEEDED The source string was too long to be
 *                            copied fully into the destination.  As
 *                            much of the source string as possible was
 *                            concatenated.
 * \retval VMK_BAD_PARAM      The source string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringCat(
   char *dst,
   const char *src,
   vmk_ByteCount dstLen);

/*
 ***********************************************************************
 * vmk_StringCopy --                                              */ /**
 *
 * \brief Copy a string up to a maximum number of bytes.
 *
 * \param[out] dst      String to copy to.
 * \param[in]  src      String to copy from.
 * \param[in]  dstLen   Maximum number of bytes to copy into dst.
 *
 * \note dst will always be be nul terminated so long as dstLen is
 *       greater than zero.
 * \note  This function will not block.
 *
 * \retval VMK_OK             The string was successfully copied.
 * \retval VMK_LIMIT_EXCEEDED The source string was too long to be
 *                            copied fully into the destination.  As
 *                            much of the source string as possible was
 *                            copied.
 * \retval VMK_BAD_PARAM      The source string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringCopy(
   char *dst,
   const char *src,
   vmk_ByteCount dstLen);

/*
 ***********************************************************************
 * vmk_StringLCopy --                                             */ /**
 *
 * \brief Copy a string and ensure nul termination.
 *
 * \param[out] dst    String to copy to.
 * \param[in]  src    String to copy from.
 * \param[in]  dstLen Maximum number of bytes to store a string in dst.
 * \param[out] outLen Number of bytes necessary.  Optional: may pass
 *                    NULL.
 *
 * \note dst will always be be nul terminated so long as dstLen is
 *       greater than zero.
 * \note outLen will always be set to the number of bytes needed to
 *       successfully copy the src string, even if that copy is not
 *       successful, not including the nul terminator.
 * \note  This function will not block.
 *
 * \retval VMK_OK             The string was successfully copied.
 * \retval VMK_LIMIT_EXCEEDED The source string was too long to be
 *                            copied fully into the destination.  As
 *                            much of the source string as possible was
 *                            copied.
 * \retval VMK_BAD_PARAM      The source string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringLCopy(
   char *dst,
   const char *src,
   vmk_ByteCount dstLen,
   vmk_ByteCount *outLen);

/*
 ***********************************************************************
 * vmk_StringFormat --                                            */ /**
 *
 * \brief Formats the provided format string and arguments into the
 *        destination buffer, up to a maximum bound.
 *
 * \param[in]  str      Buffer in which to place output.
 * \param[in]  strLen   Maximum number of bytes to output.
 * \param[out] outLen   Number of bytes necessary to format this string,
 *                      not including the terminating nul character.
 *                      Optional: may pass NULL.
 * \param[in]  format   Printf-style format string.
 *
 * \note str will always be be nul terminated so long as strLen is
 *       greater than zero.
 * \note outLen will always be set to the number of bytes needed to
 *       successfully format the src string, even if that formatting
 *       is not successful, not including the nul terminator.
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \retval VMK_OK             The string was successfully formatted.
 * \retval VMK_LIMIT_EXCEEDED The source string was too long to be
 *                            formatted fully into the destination.  As
 *                            much of the specified format string as
 *                            possible was formatted.
 * \retval VMK_BAD_PARAM      The format string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringFormat(
   char *str,
   vmk_ByteCount strLen,
   vmk_ByteCount *outLen,
   const char *format,
   ...)
VMK_ATTRIBUTE_PRINTF(4,5);

/*
 ***********************************************************************
 * vmk_StringVFormat --                                            */ /**
 *
 * \brief Formats the provided format string and varargs into the
 *        destination buffer, up to a maximum bound.
 *
 * \param[in]  str      Buffer in which to place output.
 * \param[in]  strLen   Maximum number of bytes to output.
 * \param[out] outLen   Number of bytes necessary to format this string,
 *                      not including the terminating nul character.
 *                      Optional: may pass NULL.
 * \param[in]  format   Printf-style format string.
 * \param[in]  ap       Varargs for format.
 *
 * \note str will always be be nul terminated so long as strLen is
 *       greater than zero.
 * \note outLen will always be set to the number of bytes needed to
 *       successfully format the src string, even if that formatting
 *       is not successful, not including the nul terminator.
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \retval VMK_OK             The string was successfully formatted.
 * \retval VMK_LIMIT_EXCEEDED The source string was too long to be
 *                            formatted fully into the destination.  As
 *                            much of the specified format string as
 *                            possible was formatted.
 * \retval VMK_BAD_PARAM      The format string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringVFormat(
   char *str,
   vmk_ByteCount strLen,
   vmk_ByteCount *outLen,
   const char *format,
   va_list ap);

/*
 ***********************************************************************
 * vmk_StringDup                                                  */ /**
 *
 * \brief Duplicate a string given a heap ID.
 *
 * \param[in]  heapID   Heap to allocate from.
 * \param[in]  src      String to copy from.
 * \param[in]  maxLen   Maximum length src could be.
 * \param[out] dst      Destination for allocated string.
 *
 * \note *dst is meant to be freed with vmk_HeapFree.
 * \note *dst will always be nul terminated
 * \note This function will not block.
 * \note Because it is unknown whether vmk_StringDup would be used
 *       internally or to validate some external buffers, the maxLen
 *       ensures that we do not keep scanning an unterminated string.
 *
 * \retval VMK_OK             The string was successfully copied.
 * \retval VMK_NO_MEMORY      Memory allocation failure.
 * \retval VMK_BAD_PARAM      The source string or destination buffer
 *                            is not valid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StringDup(
   vmk_HeapID heapID,
   const char *src,
   vmk_ByteCount maxLen,
   char **dst);

#endif /* _VMKAPI_STRING_H_ */
/** @} */
/** @} */
