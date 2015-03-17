/***************************************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Port                                                           */ /**
 *
 * \addtogroup Network
 * @{
 * \addtogroup Portset Interface
 * @{
 * \defgroup Deprecated Portset Interface APIs
 * @{
 ***********************************************************************
 */


#ifndef _VMKAPI_NET_PORTSET_DEPRECATED_H_
#define _VMKAPI_NET_PORTSET_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_PortsetSchedSamplingActive --                              */ /**
 *
 * \brief Get whether system service accounting is active.
 *
 * \deprecated This function will be removed in a future release. It is
 *             no longer necessary for the virtual switch implementation
 *             to cooperate with the platform to manage port billing.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \retval     VMK_TRUE       system service accounting is active
 * \retval     VMK_FALSE      system service accouting is inactive
 ***********************************************************************
 */

vmk_Bool vmk_PortsetSchedSamplingActive(void);

/*
 ***********************************************************************
 * vmk_PortsetSchedBillToPort --                                  */ /**
 *
 * \brief Set the current port that is billed for Rx processing.
 *
 * \deprecated This function will be removed in a future release. It is
 *             no longer necessary for the virtual switch implementation
 *             to cooperate with the platform to manage port billing.
 *
 * \nativedriversdisallowed
 *
 * In order for Rx packet processing to be accounted properly, the
 * portset implementation must use this API to notify the scheduling
 * subsystem which port to bill for all the Rx processing.
 *
 * \note This function must be called inside vmk_PortsetOps's dispatch
 *       callback. After dispatching all packets, dispatch callback can
 *       use a random algorithm to select a port from all destination
 *       ports of packets, and provide the selected portID to this
 *       function.
 *
 * \note Before calling this function, vmk_PortsetSchedSamplingActive()
 *       must be called to check whether system account is active. If
 *       sampling is not active, there is no need to call
 *       vmk_PortsetSchedBillToPort().
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  totalNumPkts   Number of packets currently being
 *                            processed
 *
 * \retval     VMK_OK         Successfully billed packet count to port.
 * \retval     VMK_NOT_FOUND  Port was not found.
 * \retval     VMK_BAD_PARAM  The totalNumPkts argument is 0.
 * \retval     Other status   Failed to bill packet count to port.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetSchedBillToPort(vmk_SwitchPortID portID,
                                            vmk_uint64 totalNumPkts);

#endif /* _VMKAPI_NET_PORTSET_DEPRECATED_H_ */
/** @} */
/** @} */
/** @} */
