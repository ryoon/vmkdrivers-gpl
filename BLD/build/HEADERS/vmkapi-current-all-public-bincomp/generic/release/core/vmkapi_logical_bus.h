/***************************************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Logical                                                            */ /**
 * \addtogroup Device 
 * @{
 * \defgroup Logical Logical bus interface
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_LOGICAL_BUS_H_
#define _VMKAPI_LOGICAL_BUS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Name of the logical bus type. */
#define VMK_LOGICAL_BUS_NAME "logical"

/*
 ***********************************************************************
 * vmk_LogicalCreateBusAddress --                                 */ /**
 *
 * \brief Return a global address for a device on the logical bus.
 *        
 * A driver registering a device on the logical bustype must use this 
 * call to obtain a globally unique and persistent busAddress for the 
 * device. Driver must provide a logical port component satisfying the
 * following conditions. 
 *
 * Locally unique: This means the logical port distinguishes this 
 * logical device from other logical devices created by the same driver 
 * under the same parent device. 
 *
 * Locally persistent: This means the same logical port is used for 
 * a logical device every time that device is created by the driver.
 * This is important if the logical device directly represents a 
 * physical hardware component in the system.
 *
 * Example : A network driver creates uplinks (logical devices) for a
 * NIC addressable by a single PCI function (physical parent device).
 *
 * Case 1: Single uplink for a single-port NIC. 
 *         Driver can use '0' as unique and persistent logical port. 
 *
 * Case 2: Multiplexing two uplinks for a single-port NIC.
 *         Driver may use '0' and '1' as unique logical ports. As long
 *         as the driver manages multiplexing internally, and logical 
 *         ports do not represent physical ports directly, shifting of
 *         logical port numbers is permitted. E.g. if driver is 
 *         configured to register only one uplink at next driver load, 
 *         logical port '0' may be used, whether it was '0' or '1' 
 *         during the previous load.
 *
 * Case 3: Two uplinks for a two-port NIC, one per port.
 *         Driver may choose '0' and '1' as unique logical ports for the 
 *         uplinks on the first and second port, respectively. Since the 
 *         logical devices directly represent physical ports, the driver 
 *         must also ensure that the logical ports are never shifted or 
 *         mixed up. That is, '0' is always used as the logical port for 
 *         the uplink for the first port, and '1' is always used as the 
 *         logical port for the uplink for the second port, even if one 
 *         of the physical ports fails or is disabled, or if the driver 
 *         is configured to use only one port at next driver load. 
 *
 * \note Memory is allocated for \em globalAddress. 
 *       It must be freed using vmk_LogicalFreeBusAddress().
 *
 * \param[in] driver Driver creating the device.
 * \param[in] parent Parent of device being created.
 * \param[in] uniqueLogicalPort Unique and persistent logical port number.
 * \param[out] globalAddress Globally unique and persistent address.
 * \param[out] globalAddressLen Length of globally unique address.
 *
 * \retval VMK_OK          Success.
 * \retval VMK_NO_MODULE_HEAP Driver module has no heap to allocate memory.
 * \retval VMK_NO_MEMORY   Could not allocate memory to create address. 
 * \retval VMK_BAD_PARAM   Invalid parameter. 
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_LogicalCreateBusAddress(vmk_Driver driver,
                            vmk_Device parent,
                            vmk_uint32 uniqueLogicalPort,
                            char* *globalAddress,
                            vmk_uint32 *globalAddressLen);


/*
 ***********************************************************************
 * vmk_LogicalFreeBusAddress --                                   */ /**
 *
 * \brief Free memory for logical bus address created using
 *        vmk_LogicalCreateBusAddress().
 * 
 * \param[in] driver Driver that allocated the bus address.
 * \param[in] globalAddress Allocated global address.
 *
 * \retval VMK_OK          Success 
 * \retval VMK_NO_MODULE_HEAP Driver module has no heap to free memory.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_LogicalFreeBusAddress(vmk_Driver driver,
                          char* globalAddress);

#endif /* _VMKAPI_LOGICAL_BUS_H_ */
/** @} */
/** @} */
