/* **********************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NetProtoMisc                                                   */ /**
 * \addtogroup Network
 *@{
 * \defgroup NetProtoMisc Network protocol miscellaneous APIs
 *@{
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_PROTO_MISC_H_
#define _VMKAPI_NET_PROTO_MISC_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** Definition of maximum VXLAN UDP port array size */
#define VMK_VXLAN_MAX_CONFIGURED_UDP_PORTS 4

/*
 ************************************************************************
 * vmk_VXLANConfiguredPortGetAll --                                */ /**
 *
 * \brief  Get all UDP port numbers configured as VXLAN UDP ports.
 *
 * \note   Since IANA has not assigned a specific UDP port for VXLAN use,
 *         it is possible for deployed systems to have more than one
 *         UDP port used for VXLAN. To handle this case, we need to
 *         support at least two UDP port numbers.
 *
 * \note   nPorts should be set to at least VMK_VXLAN_MAX_CONFIGURED_UDP_PORTS
 *         to get all the configured ports, otherwise it is possible that
 *         only a partial list of UDP ports is returned.
 *
 * \param[out]     portList     List of ports configured for VXLAN use.
 * \param[in,out]  nPorts       Number of ports memory allocated for (in)
 *                              actual number of ports returned(out).
 *
 * \retval      VMK_OK          On success.
 * \retval      VMK_BAD_PARAM   if bad arguments are passed.
 ************************************************************************
 */
VMK_ReturnStatus vmk_VXLANConfiguredPortGetAll(vmk_uint16 *portList,
                                               vmk_uint8 *nPorts);

/*
 ***********************************************************************
 * vmk_VXLANIsConfiguredPort --                                   */ /**
 *
 * \brief  Checks to see if portNBO is one of the configured VXLAN ports.
 *
 * \param[in]   portNBO           Port number (in network byte order).
 *
 * \retval      VMK_TRUE          If portNBO is one of the configured
 *                                VXLAN ports.
 *
 * \retval      VMK_FALSE         Otherwise.
 *
 ***********************************************************************
 */
vmk_Bool vmk_VXLANIsConfiguredPort(vmk_uint16 portNBO);

/*
 ***********************************************************************
 * vmk_VXLANConfiguredPortAdd --                                  */ /**
 *
 * \brief  Add a port to the list of configured ports for VXLAN
 *
 * \param[in]   portNBO             Port number in network byte order.
 *
 * \retval      VMK_OK              On success.
 * \retval      VMK_EXISTS          If the port number already exists.
 * \retval      VMK_LIMIT_EXCEEDED  If the number of ports in the list
 *                                  already reached the maximum number
 *                                  of configurable ports.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VXLANConfiguredPortAdd(vmk_uint16 portNBO);

/*
 ***********************************************************************
 * vmk_VXLANConfiguredPortRemove --                               */ /**
 *
 * \brief  Remove a port from the list of configured ports for VXLAN
 *
 * \param[in]   portNBO             Port number in network byte order.
 *
 * \retval      VMK_OK              On success.
 * \retval      VMK_NOT_FOUND       If the port number is not found.
 * \retval      VMK_NOT_SUPPORTED   If the removal of the port is not
 *                                  supported. This can happen if it
 *                                  is the last port in the list.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VXLANConfiguredPortRemove(vmk_uint16 portNBO);

/*
 ***********************************************************************
 * vmk_VXLANConfiguredPortGetPrimary --                           */ /**
 *
 * \brief  Get the primary UDP port number configured for VXLAN.
 *
 * \retval Primary VXLAN UDP port number.
 ***********************************************************************
 */
vmk_uint16 vmk_VXLANConfiguredPortGetPrimary(void);

/*
 ***********************************************************************
 * vmk_VXLANConfiguredPortSetPrimary --                           */ /**
 *
 * \brief  Sets the primary configured VXLAN UDP port.
 *
 * \param[in]   portNBO           Port number in network byte order.
 *
 ***********************************************************************
 */
void vmk_VXLANConfiguredPortSetPrimary(vmk_uint16 portNBO);


/*
 ***********************************************************************
 * vmk_GenevePortGet --                                           */ /**
 *
 * \brief  Get current Geneve UDP port number configured in VMkernel.
 *
 * \return      Geneve port number in network byte order.
 *
 ***********************************************************************
 */
vmk_uint16 vmk_GenevePortGet(void);


/*
 ***********************************************************************
 * vmk_GenevePortSet --                                           */ /**
 *
 * \nativedriversdisallowed
 *
 * \brief  Set current Geneve UDP port number used in VMkernel.
 *
 * \param[in]  portNBO  Geneve UDP port number in network byte order.
 *
 * \retval  VMK_OK         Set operation completes successfully.
 * \retval  VMK_BAD_PARAM  Invalid port number.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_GenevePortSet(vmk_uint16 portNBO);


#endif /* _VMKAPI_NET_PROTO_MISC_H_ */
/** @} */
/** @} */
