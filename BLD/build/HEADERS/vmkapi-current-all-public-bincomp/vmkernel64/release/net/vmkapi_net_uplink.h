/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink                                                         */ /**
 * \addtogroup Network
 *@{
 * \defgroup Uplink Uplink management
 *@{
 *
 * In VMkernel, uplinks are physical NICs, also known as `pNics'. They
 * provide external connectivity.
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_UPLINK_H_
#define _VMKAPI_NET_UPLINK_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */



/**
 * \brief Uplink handle
 *
 * vmk_Uplink is used as a handle to perform operations on uplink devices.
 *
 */
typedef struct UplinkDev * vmk_Uplink;

/*
 ***********************************************************************
 * vmk_UplinkIoctl --                                             */ /**
 *
 * \brief Do an ioctl call against the uplink.
 *
 * This function will call down to device driver to perform an ioctl.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the ioctl callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of ioctl calls with the provider of
 * the driver.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   cmd               Ioctl command
 * \param[in]   args              Ioctl arguments
 * \param[out]  result            Ioctl result
 *
 * \retval      VMK_OK            If the ioctl call succeeds
 * \retval      VMK_NOT_SUPPORTED If the uplink doesn't support ioctl
 * \retval      Other status      If the device ioctl call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkIoctl(vmk_Uplink  uplink,
                                 vmk_uint32  cmd,
                                 void       *args,
                                 vmk_uint32 *result);


/*
 ***********************************************************************
 * vmk_UplinkReset --                                             */ /**
 *
 * \brief Reset the uplink device underneath.
 *
 * This function will call down to device driver, close and re-open the
 * device. The link state will consequently go down and up.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the reset callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of the reset call with the provider of
 * the driver.
 *
 * \note This call is asynchronous, the function might return before
 * the driver call completed.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            if the reset call succeeds
 * \retval      VMK_NOT_SUPPORTED if the uplink doesn't support reset
 * \retval      Other status      if the device reset call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkReset(vmk_Uplink uplink);


#endif /* _VMKAPI_NET_UPLINK_H_ */
/** @} */
/** @} */
