/* **********************************************************
 * Copyright 2008 - 2009, 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Utilities                                                      */ /**
 *
 * \addtogroup Lib
 * @{
 * \defgroup Util Utilities
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_UTIL_H_
#define _VMKAPI_UTIL_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * VMK_STRINGIFY --                                               */ /**
 *
 * \brief Turn a preprocessor variable into a string
 *
 * \param[in] v      A preprocessor variable to be converted to a
 *                   string.
 *
 ***********************************************************************
 */
/** \cond never */
#define __VMK_STRINGIFY(v) #v
/** \endcond never */
#define VMK_STRINGIFY(v) __VMK_STRINGIFY(v)

/*
 ***********************************************************************
 * VMK_UTIL_ROUNDUP --                                            */ /**
 *
 * \brief Round up a value X to the next multiple of Y.
 *
 * \param[in] x    Value to round up.
 * \param[in] y    Value to round up to the next multiple of.
 *
 * \returns Rounded up value.
 *
 ***********************************************************************
 */
#define VMK_UTIL_ROUNDUP(x, y)   ((((x)+(y)-1) / (y)) * (y))

/**
 * \brief A series of macros used to count parameters in a varargs list
 */

/*
 ***********************************************************************
 * __VMK_UTIL_MASK_ARGS_INT__ --                                  */ /**
 *
 * \brief Internal macro to ignore the first 128 varargs parameters and
 *        evaluate as 129th.
 *
 * This is used as part of VMK_UTIL_NUM_ARGS().  VMKAPI clients should
 * not call this macro directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
#define __VMK_UTIL_MASK_ARGS_INT__( \
   _ARG1, _ARG2, _ARG3, _ARG4, _ARG5, \
   _ARG6, _ARG7, _ARG8, _ARG9, _ARG10, \
   _ARG11, _ARG12, _ARG13, _ARG14, _ARG15, \
   _ARG16, _ARG17, _ARG18, _ARG19, _ARG20, \
   _ARG21, _ARG22, _ARG23, _ARG24, _ARG25, \
   _ARG26, _ARG27, _ARG28, _ARG29, _ARG30, \
   _ARG31, _ARG32, _ARG33, _ARG34, _ARG35, \
   _ARG36, _ARG37, _ARG38, _ARG39, _ARG40, \
   _ARG41, _ARG42, _ARG43, _ARG44, _ARG45, \
   _ARG46, _ARG47, _ARG48, _ARG49, _ARG50, \
   _ARG51, _ARG52, _ARG53, _ARG54, _ARG55, \
   _ARG56, _ARG57, _ARG58, _ARG59, _ARG60, \
   _ARG61, _ARG62, _ARG63, _ARG64, _ARG65, \
   _ARG66, _ARG67, _ARG68, _ARG69, _ARG70, \
   _ARG71, _ARG72, _ARG73, _ARG74, _ARG75, \
   _ARG76, _ARG77, _ARG78, _ARG79, _ARG80, \
   _ARG81, _ARG82, _ARG83, _ARG84, _ARG85, \
   _ARG86, _ARG87, _ARG88, _ARG89, _ARG90, \
   _ARG91, _ARG92, _ARG93, _ARG94, _ARG95, \
   _ARG96, _ARG97, _ARG98, _ARG99, _ARG100, \
   _ARG101, _ARG102, _ARG103, _ARG104, _ARG105, \
   _ARG106, _ARG107, _ARG108, _ARG109, _ARG110, \
   _ARG111, _ARG112, _ARG113, _ARG114, _ARG115, \
   _ARG116, _ARG117, _ARG118, _ARG119, _ARG120, \
   _ARG121, _ARG122, _ARG123, _ARG124, _ARG125, \
   _ARG126, _ARG127, _ARG128, _ARG129, ...) _ARG129
/** \endcond */

/*
 ***********************************************************************
 * __VMK_UTIL_ARG_COUNTS__ --                                     */ /**
 *
 * \brief A series of argument counts, used with VMK_UTIL_NUM_ARGS.
 *
 * This is used as part of VMK_UTIL_NUM_ARGS().  VMKAPI clients should
 * no call this macro directly.
 *
 ***********************************************************************
 */
/** \cond nodoc */
#define __VMK_UTIL_ARG_COUNTS__() \
   128, 127, 126, 125, 124, 123, 122, 121, 120, \
   119, 118, 117, 116, 115, 114, 113, 112, 111, 110, \
   109, 108, 107, 106, 105, 104, 103, 102, 101, 100, \
   99, 98, 97, 96, 95, 94, 93, 92, 91, 90, \
   89, 88, 87, 86, 85, 84, 83, 82, 81, 80, \
   79, 78, 77, 76, 75, 74, 73, 72, 71, 70, \
   69, 68, 67, 66, 65, 64, 63, 62, 61, 60, \
   59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
   49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
   39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
   29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
   19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
   9, 8, 7, 6, 5, 4, 3, 2, 1, 0
/** \endcond */


/*
 ***********************************************************************
 * __VMK_UTIL_MASK_ARGS__ --                                      */ /**
 *
 * \brief Variadic macro that masks the first 128 arguments, evaluating
 *        the 129th.
 *
 * This is used as part of VMK_UTIL_NUM_ARGS().  VMKAPI clients should
 * no call this macro directly.
 *
 ***********************************************************************
 */
/** \cond nodoc */
#define __VMK_UTIL_MASK_ARGS__(...) \
   __VMK_UTIL_MASK_ARGS_INT__(__VA_ARGS__)
/** \endcond */

/*
 ***********************************************************************
 * VMK_UTIL_NUM_ARGS --                                           */ /**
 *
 * \brief Macro to count the number of varargs parameters.
 *
 * \param[in] ...      varargs parameters.  There can be 0 to 127
 *                     parameters.  More than 127 produces an
 *                     undefined result.
 *
 * \note This works by creating a larger varargs series of parameters
 *       around the passed series and using internal macros.  The
 *       larger series is a dummy parameter (to support if the passed
 *       series has no parameters), the passed series, and then a
 *       decreasing series of numbers representing the parameter count.
 *       The internal macros effectively ignore the first 128
 *       parameters of this larger series and evaluates as the 129th,
 *       thus effectively evaluating as one plus the count of passed
 *       parameters.  (The one is offset by subtraction in this
 *       macro).
 * \note This macro relies on the special behavior of ##__VA_ARGS__,
 *       as described here:
 *       http://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
 *       In the case that the passed varargs series has no elements,
 *       ##__VA_ARGS__ will also consume the comma preceding it,
 *       allowing this macro (VMK_UTIL_NUM_ARGS) to compile correctly
 *       and evaluate as 0.
 *
 ***********************************************************************
 */
/** \cond nodoc */
#define VMK_UTIL_NUM_ARGS(...) \
   (__VMK_UTIL_MASK_ARGS__(_DUMMY, ##__VA_ARGS__, __VMK_UTIL_ARG_COUNTS__()) - 1)
/** \endcond */

#endif /* _VMKAPI_UTIL_H_ */
/** @} */
/** @} */
