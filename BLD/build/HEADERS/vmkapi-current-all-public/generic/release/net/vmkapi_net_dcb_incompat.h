/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

#ifndef _VMKAPI_NET_DCB_INCOMPAT_H_
#define _VMKAPI_NET_DCB_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_UplinkCapDCBIsEnabled --                                   */ /**
 *
 * \ingroup DCB
 * \brief Indicates if uplink DCB capability is enabled.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_TRUE          if capability DCB is enabled
 * \retval      VMK_FALSE         Otherwise
 *
 ***********************************************************************
 */
vmk_Bool vmk_UplinkCapDCBIsEnabled(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkCapDCBEnable --                                      */ /**
 *
 * \ingroup DCB
 * \brief Enable DCB capability on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkCapDCBEnable(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkCapDCBDisable --                                     */ /**
 *
 * \ingroup DCB
 * \brief Disable DCB capability on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkCapDCBDisable(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBNumTCsGet --                                      */ /**
 *
 * \ingroup DCB
 * \brief Get number of DCB traffic class on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      refer to the definition of enum vmk_DCBNumTCs
 *
 ***********************************************************************
 */
vmk_DCBNumTCs vmk_UplinkDCBNumTCsGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityGroupGet --                               */ /**
 *
 * \ingroup DCB
 * \brief Get DCB priority group on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      refer to the definition of enum vmk_DCBPriorityGroup
 *
 ***********************************************************************
 */
vmk_DCBPriorityGroup vmk_UplinkDCBPriorityGroupGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityGroupSet --                               */ /**
 *
 * \ingroup DCB
 * \brief Set DCB priority group on uplink.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   prioGroup         Priority group
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBPriorityGroupSet(vmk_Uplink uplink,
                                               vmk_DCBPriorityGroup *prioGroup);


/*
 ***********************************************************************
 * vmk_UplinkDCBIsPriorityFCEnabled --                            */ /**
 *
 * \ingroup DCB
 * \brief Indicates if DCB priority flow control is enabled on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_TRUE          if uplink DCB is priority FC enabled
 * \retval      VMK_FALSE         Otherwise
 *
 ***********************************************************************
 */
vmk_Bool vmk_UplinkDCBIsPriorityFCEnabled(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCEnable --                               */ /**
 *
 * \ingroup DCB
 * \brief Enable DCB priority flow control on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBPriorityFCEnable(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCDisable --                              */ /**
 *
 * \ingroup DCB
 * \brief Disable DCB priority flow control on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBPriorityFCDisable(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCGet --                                  */ /**
 *
 * \ingroup DCB
 * \brief Get DCB priority flow control on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      refer to the definition of enum vmk_DCBPriorityFlowControlCfg
 *
 ***********************************************************************
 */
vmk_DCBPriorityFlowControlCfg vmk_UplinkDCBPriorityFCGet(
                              vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCSet --                                  */ /**
 *
 * \ingroup DCB
 * \brief Set DCB priority flow control on uplink.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   prioFC            priority FC
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBPriorityFCSet(vmk_Uplink uplink,
                                            vmk_DCBPriorityFlowControlCfg *prioFC);


/*
 ***********************************************************************
 * vmk_UplinkDCBApplicationsGet --                                */ /**
 *
 * \ingroup DCB
 * \brief Get DCB applications on uplink.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      refer to the definition of enum vmk_DCBApplications
 *
 ***********************************************************************
 */
vmk_DCBApplications vmk_UplinkDCBApplicationsGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkDCBApplicationSet --                                 */ /**
 *
 * \ingroup DCB
 * \brief Set DCB application on uplink.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   appl              DCB application
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBApplicationSet(vmk_Uplink uplink,
                                             vmk_DCBApplication *appl);


/*
 ***********************************************************************
 * vmk_UplinkDCBCapabilitiesGet --                                */ /**
 *
 * \ingroup DCB
 * \brief Retrieve DCB capabilities information.
 *
 * \param[in]   uplink            Uplink handle
 * \param[out]  caps              DCB capabilities
 *
 * \retval      VMK_OK            If the call completes successfully.
 * \retval      VMK_BAD_PARAM     Either uplink or caps is invalid.
 * \retval      VMK_FAILURE       Otherwise.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkDCBCapabilitiesGet(vmk_Uplink uplink,
                                              vmk_DCBCapabilities *caps);


#endif /* _VMKAPI_NET_DCB_INCOMPAT_H_ */
