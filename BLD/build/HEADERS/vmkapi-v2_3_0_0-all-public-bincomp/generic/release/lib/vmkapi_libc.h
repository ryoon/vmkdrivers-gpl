/* **********************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * LibC                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup LibC C-Library-Style Interfaces
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_LIBC_H_
#define _VMKAPI_LIBC_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include <stdarg.h>

/*
 ***********************************************************************
 * vmk_Strnlen --                                                 */ /**
 *
 * \brief Determine the length of a string up to a maximum number
 *        of bytes.
 *
 * \note  This function will not block.
 *
 * \param[in] src    String whose length is to be determined.
 * \param[in] maxLen Maximum number of bytes to check.
 *
 * \return Length of the string in bytes.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_Strnlen (
   const char *src,
   vmk_ByteCount maxLen);

/*
 ***********************************************************************
 * vmk_Strcpy --                                                 */ /**
 *
 * \brief Copy a string.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringCopy() instead.
 *
 * \param[out] dst   String to copy to.
 * \param[in]  src   String to copy from.
 *
 * \return Pointer to src.
 *
 ***********************************************************************
 */
char *vmk_Strcpy(
   char *dst,
   const char *src);

/*
 ***********************************************************************
 * vmk_Strncpy --                                                 */ /**
 *
 * \brief Copy a string up to a maximum number of bytes.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringCopy() instead.
 *
 * \param[out] dst      String to copy to.
 * \param[in]  src      String to copy from.
 * \param[in]  maxLen   Maximum number of bytes to copy into dst.
 *
 * \warning A copied string is not automatically nul-terminated.
 *          Users should take care to ensure that the destination
 *          is large enough to hold the string and the nul-terminator.
 *
 * \return Pointer src.
 *
 ***********************************************************************
 */
char *vmk_Strncpy(
   char *dst,
   const char *src,
   vmk_ByteCount maxLen);

/*
 ***********************************************************************
 * vmk_Strlcpy --                                                  */ /**
 *
 * \brief Copy a string and ensure nul termination.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringLCopy() instead.
 *
 * \param[out] dst   String to copy to.
 * \param[in]  src   String to copy from.
 * \param[in]  size  Maximum number of bytes to store a string in dst.
 *
 * \note At most size-1 bytes will be copied. All copies will
 *       be terminated with a nul unless size is set to zero.
 *
 * \return The length of src. If the return value is greater than
 *         or equal to size, then truncation has occured during
 *         the copy.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_Strlcpy(
   char *dst,
   const char *src,
   vmk_ByteCount size);

/*
 ***********************************************************************
 * vmk_Strcmp --                                                  */ /**
 *
 * \brief Compare two strings.
 *
 * \note  This function will not block.
 *
 * \param[in] src1      First string to compare.
 * \param[in] src2      Second string to compare.
 *
 * \return A value greater than, less than or equal to 0 depending on
 *         the lexicographical difference between the strings.
 *
 ***********************************************************************
 */
int vmk_Strcmp(
   const char *src1,
   const char *src2);

/*
 ***********************************************************************
 * vmk_Strncmp --                                                 */ /**
 *
 * \brief Compare two strings up to a maximum number of bytes.
 *
 * \note  This function will not block.
 *
 * \param[in] src1      First string to compare.
 * \param[in] src2      Second string to compare.
 * \param[in] maxLen    Maximum number of bytes to compare.
 *
 * \return A value greater than, less than or equal to 0 depending on
 *         the lexicographical difference between the strings.
 *
 ***********************************************************************
 */
int vmk_Strncmp(
   const char *src1,
   const char *src2,
   vmk_ByteCount maxLen);

/*
 ***********************************************************************
 * vmk_Strncasecmp --                                             */ /**
 *
 * \brief Compare two strings while disregarding case.
 *
 * \note  This function will not block.
 *
 * \param[in] src1      First string to compare.
 * \param[in] src2      Second string to compare.
 * \param[in] maxLen    Maximum number of bytes to compare.
 *
 * \return A value greater than, less than or equal to 0 depending on
 *         the lexicographical difference between the strings as if
 *         all characters in the string are lower-case.
 *
 ***********************************************************************
 */
int vmk_Strncasecmp(
   const char *src1,
   const char *src2,
   vmk_ByteCount maxLen);

/*
 ***********************************************************************
 * vmk_Strchr --                                                  */ /**
 *
 * \brief Forward search through a string for a character.
 *
 * \note  This function will not block.
 *
 * \param[in] src  String to search forward.
 * \param[in] c    Character to search for.
 *
 * \return Pointer to the offset in src where c was found or NULL
 *         if c was not found in src.
 *
 ***********************************************************************
 */
char *vmk_Strchr(
   const void *src,
   int c);

/*
 ***********************************************************************
 * vmk_Strrchr --                                                 */ /**
 *
 * \brief Reverse search through a string for a character.
 *
 * \note  This function will not block.
 *
 * \param[in] src  String to search backward.
 * \param[in] c    Character to search for.
 *
 * \return Pointer to the offset in src where c was found or NULL
 *         if c was not found in src.
 *
 ***********************************************************************
 */
char *vmk_Strrchr(
   const void *src,
   int c);

/*
 ***********************************************************************
 * vmk_Strstr --                                                  */ /**
 *
 * \brief Search for a substring in a string.
 *
 * \note  This function will not block.
 *
 * \param[in] s1  String to search.
 * \param[in] s2  String to search for.
 *
 * \return Pointer to the offset in s1 where s2 was found or NULL
 *         if s2 was not found in s1.
 *
 ***********************************************************************
 */
char *vmk_Strstr(
   const void *s1,
   const void *s2);

/*
 ***********************************************************************
 * vmk_Strtol --                                                  */ /**
 *
 * \brief Convert a string to a signed long integer.
 *
 * \note  This function will not block.
 *
 * \param[in]  str   String to convert
 * \param[out] end   If non-NULL, the address of the first invalid
 *                   character or the value of str if there are no
 *                   digits in the string.
 * \param[in]  base  Base of the number being converted which may be
 *                   between 2 and 36, or 0 may be supplied such
 *                   that strtol will attempt to detect the base
 *                   of the number in the string.
 *
 * \return Numeric value of the string.
 *
 ***********************************************************************
 */
long vmk_Strtol(
   const char *str,
   char **end,
   int base);

/*
 ***********************************************************************
 * vmk_Strtoul --                                                 */ /**
 *
 * \brief Convert a string to an unsigned long integer.
 *
 * \note  This function will not block.
 *
 * \param[in]  str   String to convert
 * \param[out] end   If non-NULL, the address of the first invalid
 *                   character or the value of str if there are no
 *                   digits in the string.
 * \param[in]  base  Base of the number being converted which may be
 *                   between 2 and 36, or 0 may be supplied such
 *                   that strtoul will attempt to detect the base
 *                   of the number in the string.
 *
 * \return Numeric value of the string.
 *
 ***********************************************************************
 */
unsigned long vmk_Strtoul(
   const char *str,
   char **end,
   int base);

/*
 ***********************************************************************
 * vmk_Sprintf --                                                 */ /**
 *
 * \brief Formatted print to a string.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringFormat() instead.
 *
 * \printformatstringdoc
 *
 * \param[in] str       Buffer in which to place output.
 * \param[in] format    Printf-style format string.
 *
 * \return Number of bytes in the output string, not including the
 *         terminating nul character.
 *
 ***********************************************************************
 */
int vmk_Sprintf(
   char *str,
   const char *format,
   ...)
VMK_ATTRIBUTE_PRINTF(2,3);

/*
 ***********************************************************************
 * vmk_Snprintf --                                                */ /**
 *
 * \brief Formatted print to a string with a maximum bound.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringFormat() instead.
 *
 * \printformatstringdoc
 *
 * \param[in] str       Buffer in which to place output.
 * \param[in] size      Maximum number of bytes to output.
 * \param[in] format    Printf-style format string.
 *
 * \return Number of bytes required to format the output string, not
 *         including the terminating nul character.
 *
 * \note If the return value is less than the specified size, the
 *       string has been formatted completely.  If the return value is
 *       equal to or greater than the specified size, the output has
 *       been truncated.  The output will always be nul terminated
 *       unless the specified size is zero.
 *
 ***********************************************************************
 */
int vmk_Snprintf(
   char *str,
   vmk_ByteCount size,
   const char *format,
   ...)
VMK_ATTRIBUTE_PRINTF(3,4);


/*
 ***********************************************************************
 * vmk_Vsprintf --                                                */ /**
 *
 * \brief Formatted print to a string using varargs.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringVFormat() instead.
 *
 * \printformatstringdoc
 *
 * \param[in] str       Buffer in which to place output.
 * \param[in] format    Printf-style format string.
 * \param[in] ap        Varargs for format.
 *
 * \return Number of bytes in the output string, not including the
 *         terminating nul character.
 *
 ***********************************************************************
 */
int
vmk_Vsprintf(
   char *str,
   const char *format,
   va_list ap);

/*
 ************************************************************************
 * vmk_Vsnprintf --                                                */ /**
 *
 * \brief Formatted print to a string with a maximum bound using varargs.
 *
 * \note  This function will not block.
 *
 * \deprecated Use vmk_StringVFormat() instead.
 *
 * \printformatstringdoc
 *
 * \param[in] str       Buffer in which to place output.
 * \param[in] size      Maximum number of bytes to output.
 * \param[in] format    Printf-style format string.
 * \param[in] ap        Varargs for format.
 *
 * \return Number of bytes required to format the output string, not
 *         including the terminating nul character.
 *
 * \note If the return value is less than the specified size, the
 *       string has been formatted completely.  If the return value is
 *       equal to or greater than the specified size, the output has
 *       been truncated.  The output will always be nul terminated
 *       unless the specified size is zero.
 *
 ***********************************************************************
 */
int
vmk_Vsnprintf(
   char *str,
   vmk_ByteCount size,
   const char *format,
   va_list ap);

/*
 ***********************************************************************
 * vmk_Vsscanf  --                                                */ /**
 *
 * \brief Formatted scan of a string using varargs.
 *
 * \note  This function will not block.
 *
 * \scanformatstringdoc
 *
 * \param[in] inp    Buffer to scan.
 * \param[in] fmt0   Scanf-style format string.
 * \param[in] ap     Varargs that hold input values for format.
 *
 * \return Number of input values assigned.
 *
 ***********************************************************************
 */
int
vmk_Vsscanf(
    const char *inp,
    char const *fmt0,
    va_list ap);

/*
 ***********************************************************************
 * vmk_Sscanf   --                                                */ /**
 *
 * \brief Formatted scan of a string.
 *
 * \note  This function will not block.
 *
 * \scanformatstringdoc
 *
 * \param[in] ibuf   Buffer to scan.
 * \param[in] fmt    Scanf-style format string.
 *
 * \return Number of input values assigned.
 *
 ***********************************************************************
 */
int
vmk_Sscanf(
    const char *ibuf,
    const char *fmt,
    ...)
VMK_ATTRIBUTE_SCANF(2, 3);

/*
 ***********************************************************************
 * vmk_Memset --                                                  */ /**
 *
 * \brief Set bytes in a buffer to a particular value.
 *
 * \note  This function will not block.
 *
 * \param[in] dst    Destination buffer to set.
 * \param[in] byte   Value to set each byte to.
 * \param[in] len    Number of bytes to set.
 *
 * \return Original value of dst.
 *
 ***********************************************************************
 */
void *vmk_Memset(
   void *dst,
   int byte,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_Memcpy --                                                  */ /**
 *
 * \brief Copy bytes from one buffer to another.
 *
 * \note  This function will not block.
 * \note  Memory areas may not overlap.
 *
 * \param[in] dst    Destination buffer to copy to.
 * \param[in] src    Source buffer to copy from.
 * \param[in] len    Number of bytes to copy.
 *
 * \return Original value of dst.
 *
 ***********************************************************************
 */
void *vmk_Memcpy(
   void *dst,
   const void *src,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_Memmove --                                                 */ /**
 *
 * \brief Copy bytes between two potentially overlapping buffers.
 *
 * \note  This function will not block.
 * \note  Memory areas may overlap.
 *
 * \param[in] dst    Destination buffer to copy to.
 * \param[in] src    Source buffer to copy from.
 * \param[in] len    Number of bytes to copy.
 *
 * \return Original value of dst.
 *
 ***********************************************************************
 */
void *vmk_Memmove(
   void *dst,
   void *src,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_Memcmp --                                                  */ /**
 *
 * \brief Compare bytes between two buffers.
 *
 * \note  This function will not block.
 *
 * \param[in] src1   Buffer to compare.
 * \param[in] src2   Other buffer to compare.
 * \param[in] len    Number of bytes to compare.
 *
 * \return Difference between the first two differing bytes or
 *         zero if the buffers are identical over the specified length.
 *
 ***********************************************************************
 */
int vmk_Memcmp(
   const void *src1,
   const void *src2,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_Rand --                                                    */ /**
 *
 * \brief Generate the next number in a pseudorandom sequence.
 *
 * \note  This function will not block.
 *
 * \warning This function's pseudorandom numbers do not have high
 *          enough quality for cryptographic purposes.
 *
 * \note    The values returned are in the range 0 < n <= 0x7fffffff,
 *          and the seed supplied must also be in this range.
 *
 * \param[in] seed   The previous number in the sequence, if any.  To
 *                   start a new sequence at a random point, call
 *                   vmk_GetRandSeed to get the initial seed.  To
 *                   start a new sequence at a deterministic point,
 *                   choose any fixed value in range for the initial
 *                   seed.
 *
 * \return A pseudorandom number.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_Rand(
   vmk_uint32 seed);

/*
 ***********************************************************************
 * vmk_GetRandSeed --                                             */ /**
 *
 * \brief Generate a random initial seed for vmk_Rand()
 *
 * \note  This function will not block.
 *
 * \warning This function should be called only occasionally, to start
 *          a sequence of pseudorandom numbers generated by
 *          vmk_Rand().  Attempting to use this function frequently to
 *          generate individual random numbers will result in values
 *          with very poor randomness.  Even when this function is
 *          called only occasionally, its random numbers do not have
 *          high enough quality for cryptographic purposes.
 *
 * \return A random seed value suitable to start a new pseudorandom
 *         sequence.
 *
 ***********************************************************************
 */
vmk_uint32
vmk_GetRandSeed(
   void);

/*
 ***********************************************************************
 * vmk_IsPrint --                                                 */ /**
 *
 * \brief Determine if a character is printable.
 *
 * \note  This function will not block.
 *
 * \param[in] c  Character to check.
 *
 * \retval VMK_TRUE  Supplied character is printable.
 * \retval VMK_FALSE Supplied character is not printable.
 *
 ***********************************************************************
 */
vmk_Bool
vmk_IsPrint(
   int c);

#endif /* _VMKAPI_LIBC_H_ */
/** @} */
/** @} */
