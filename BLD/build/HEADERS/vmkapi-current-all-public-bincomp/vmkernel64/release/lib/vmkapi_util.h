/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
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

#endif /* _VMKAPI_UTIL_H_ */
/** @} */
/** @} */
