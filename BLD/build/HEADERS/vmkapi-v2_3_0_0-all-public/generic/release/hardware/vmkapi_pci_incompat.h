/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * Please consult with the VMKernel hardware and core teams before making any
 * binary incompatible changes to this file!
 */

/*
 ***********************************************************************
 * PCI                                                            */ /**
 * \addtogroup Device
 * @{
 * \defgroup PCI PCI
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PCI_INCOMPAT_H_
#define _VMKAPI_PCI_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Device callback reason
 */
typedef enum vmk_PCIDeviceCallbackReason {
      VMK_PCI_DEVICE_INSERTED             = 0,
      VMK_PCI_DEVICE_REMOVED              = 1,
      VMK_PCI_DEVICE_CHANGED_OWNER        = 2,
} vmk_PCIDeviceCallbackReason;

/*
 ***********************************************************************
 * vmk_PCIGetDevice --                                            */ /**
 *
 * \ingroup PCI
 * \brief Retrieves vmk_PCIDevice that corresponds to the generic
 *        device genDevice.
 *
 * \param[in]  genDevice   Generic device handle.
 * \param[out] pciDevice   Pointer to PCI device handle.
 *
 * \retval VMK_BAD_PARAM genDevice is invalid
 * \retval VMK_OK        PCI device handle is successfully returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetDevice(vmk_Device genDevice,
                                  vmk_PCIDevice *pciDevice);

/*
 ***********************************************************************
 * vmk_PCIGetGenDevice --                                         */ /**
 *
 * \ingroup PCI
 * \brief Retrieves generic vmk_Device that corresponds to the PCI
 *        device pciDevice.
 *
 * \param[in]  pciDevice   PCI device handle.
 * \param[out] genDevice   Pointer to generic device handle.
 *
 * \retval VMK_BAD_PARAM   pciDevice is invalid
 * \retval VMK_OK          Generic device handle is successfully
 *                         returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetGenDevice(vmk_PCIDevice pciDevice,
                                     vmk_Device *genDevice);

/*
 ***********************************************************************
 * vmk_PCIGetPCIDevice --                                            */ /**
 *
 * \ingroup PCI
 * \brief Retrieves vmk_PCIDevice handle that corresponds to segment,
 *        bus, device and function
 *
 * \param[in]  segment   PCI segment
 * \param[in]  bus       Bus number
 * \param[in]  dev       Device number
 * \param[in]  func      Function number
 * \param[out] device    PCI device handle
 *
 * \retval VMK_NOT_FOUND Device with bus, device, number does not exist
 * \retval VMK_BAD_PARAM device argument is NULL
 * \retval VMK_OK        device handle is successfully returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetPCIDevice(const vmk_uint16 segment,
                                     const vmk_uint8 bus,
                                     const vmk_uint8 dev,
                                     const vmk_uint8 func,
                                     vmk_PCIDevice *device);

/*
 ***********************************************************************
 * vmk_PCIGetSegmentBusDevFunc --                                 */ /**
 *
 * \ingroup PCI
 * \brief Return device's bus, device, func and function
 *
 * \param[in]  device     PCI device handle
 * \param[out] segment    Pointer to Segment number
 * \param[out] bus        Pointer to Bus number
 * \param[out] dev        Pointer to Device number
 * \param[out] func       Pointer to Function number
 *
 * \retval VMK_BAD_PARAM   One or more of bus, dev, func is NULL.
 * \retval VMK_BAD_PARAM   Device handle is invalid or device does
 *                         not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetSegmentBusDevFunc(vmk_PCIDevice device,
                                             vmk_uint16 *segment,
                                             vmk_uint8 *bus,
                                             vmk_uint8 *dev,
                                             vmk_uint8 *func);

/*
 ***********************************************************************
 * vmk_PCIGetIDs --                                               */ /**
 *
 * \ingroup PCI
 * \brief Return device's vendor ID, device ID, sub vendor ID and
 *        sub device iD
 *
 * \param[in] device       PCI device handle
 * \param[out] vendorID    Pointer to vendor ID.
 * \param[out] deviceID    Pointer to device ID.
 * \param[out] subVendorID Pointer to sub vendor ID.
 * \param[out] subDeviceID Pointer to sub device ID.
 *
 * \retval VMK_BAD_PARAM   One or more of vendorID, deviceID
 *                         subVendorID and subDeviceID is NULL.
 * \retval VMK_BAD_PARAM   Device handle is invalid or device does
 *                         not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetIDs(vmk_PCIDevice device,
                               vmk_uint16 *vendorID,
                               vmk_uint16 *deviceID,
                               vmk_uint16 *subVendorID,
                               vmk_uint16 *subDeviceID);

/*
 ***********************************************************************
 * vmk_PCISetDeviceName --                                        */ /**
 *
 * \ingroup PCI
 * \brief Sets the device name
 *
 * \param[in] device    PCI device handle
 * \param[in] name      Name to be set
 *
 * \retval VMK_BAD_PARAM Device handle is invalid/non existent or name
 *                       is NULL
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCISetDeviceName(vmk_PCIDevice device,
                                      const char *name);

/*
 ***********************************************************************
 * vmk_PCIGetDeviceName --                                        */ /**
 *
 * \ingroup PCI
 * \brief Gets the device name
 *
 * \param[in]  device      PCI device handle
 * \param[out] name        Pointer to char array to return the name in
 * \param[in]  nameLen     Length of the char array.
 *
 * \retval VMK_BAD_PARAM         Device is invalid or non existent.
 * \retval VMK_BAD_PARAM         Name is NUL or nameLen is greater than
 *                               VMK_PCI_DEVICE_NAME_LENGTH.
 * \retval VMK_DEVICE_NOT_NAMED  Device is not named
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetDeviceName(vmk_PCIDevice device,
                                      char *name,
                                      vmk_ByteCount nameLen);

/*
 ***********************************************************************
 * vmk_PCIGetAlternateName --                                     */ /**
 *
 * \ingroup PCI
 * \brief Gets the alternate name of the device
 *
 * \param[in] device       PCI device handle
 * \param[out] name        Pointer to char array to return the name in
 * \param[in] nameLen      Length of the char array.
 *
 * \retval VMK_BAD_PARAM         Device is invalid or non existent.
 * \retval VMK_BAD_PARAM         Name is NULL or nameLen is greater than
 *                               VMK_PCI_DEVICE_NAME_LENGTH.
 * \retval VMK_DEVICE_NOT_NAMED  Device has no alternate name
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetAlternateName(vmk_PCIDevice device,
                                               char *name,
                                               vmk_ByteCount nameLen);

/*
 ***********************************************************************
 * vmk_PCIDoPreRemove --                                          */ /**
 *
 * \ingroup PCI
 * \brief Invoke a module-specific function before a device is removed.
 *
 * This function should only be called by drivers, not by vmkernel
 * modules.
 *
 * \param[in] moduleID ID of the module whose pre-remove func is to be
 *                     called.
 * \param[in] device   Device handle to be passed to the pre-remove
 *                     function.
 *
 * \retval VMK_BAD_PARAM   Device is invalid/NULL.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDoPreRemove(vmk_ModuleID moduleID,
                   vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIDoPostInsert --                                         */ /**
 *
 * \ingroup PCI
 * \brief Invoke a module-specific function after a device has been
 *        inserted.
 *
 * \param[in] moduleID  ID of the module whose post-insert func is to
 *                      be called.
 * \param[in] device    Device handle to be passed to the post-insert
 *                      function.
 *
 * This function should only be called by drivers, not by vmkernel
 * modules.
 *
 * \retval VMK_BAD_PARAM device is invalid/NULL
 * \retval VMK_OK        success.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDoPostInsert(vmk_ModuleID moduleID,
                    vmk_PCIDevice device);

/*
 *************************************************************************
 * vmk_PCIRegisterPTOps --                                          */ /**
 *
 * \ingroup PCI
 * \brief Register SR-IOV passthrough operations with all VFs
 *
 * Stores a pointer to the hardware vendor defined passthrough operations
 * in private data associated with virtual functions(VF). The passthru ops
 * are stored with all VFs instantiated by a particular physical
 * function(PF). The PF driver should call this function after
 * instantiating virtual functions.
 *
 * \param[in] pf            PCI device handle of the PF.
 * \param[in] ptOps         Address cookie with pointer to passthu ops
 *
 * \retval VMK_BAD_PARAM PF device handle is invalid.
 * \retval VMK_BAD_PARAM privateData pointer is NULL.
 *
 ************************************************************************
 */
VMK_ReturnStatus
vmk_PCIRegisterPTOps(vmk_PCIDevice pf,
                     vmk_AddrCookie ptOps);

#endif /* _VMKAPI_PCI_INCOMPAT_H_ */
/** @} */
/** @} */
