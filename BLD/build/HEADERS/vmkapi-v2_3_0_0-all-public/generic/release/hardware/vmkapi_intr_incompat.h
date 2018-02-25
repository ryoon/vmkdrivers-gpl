/* **********************************************************
 * Copyright 2008 - 2009, 2012-2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Please consult with the VMKernel hardware and core teams before making any
 * binary incompatible changes to this file!
 */

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Interrupt                                                      */ /**
 * \addtogroup Device
 * @{
 * \defgroup Interrupt Interrupt Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_INTR_INCOMPAT_H_
#define _VMKAPI_INTR_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** old style interrupt handlers have no acknowledge handler */
#define VMK_INTR_ATTRS_INCOMPAT_ACK  (1UL << 63)

/*
 ***********************************************************************
 * vmk_IntrTrackerAddSample --                                    */ /**
 *
 * \ingroup Interrupt
 * \brief Adds "count" samples to the affinity tracker of "intrCookie".
 *
 *  The interrupt tracker keeps track of the number of samples
 *  associated with each scheduling context for each interrupt.
 *  It may automatically let "intrCookie" follow the context
 *  with dominating number of samples.
 *
 * \note This function will not block.
 *
 * \param[in] intrCookie   The interrupt cookie being tracked.
 * \param[in] count        The number of samples to be added.
 * \param[in] now          Current timestamp.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        "intrCookie" is not valid.
 * \retval VMK_NOT_SUPPORTED    The function is not called from a world
 *                              or worldlet context.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrTrackerAddSample(vmk_IntrCookie intrCookie,
                                          vmk_uint32 count,
                                          vmk_TimerCycles now);

#endif /* _VMKAPI_INTR_INCOMPAT_H_ */
/** @} */
/** @} */
