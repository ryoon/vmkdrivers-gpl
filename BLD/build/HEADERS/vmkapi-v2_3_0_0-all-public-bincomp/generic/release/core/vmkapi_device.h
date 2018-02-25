/***************************************************************************
 * Copyright 2007 - 2013  VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Devices                                                        */ /**
 * \defgroup Device Device interface
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_DEVICE_H_
#define _VMKAPI_DEVICE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Device identification.
 */
typedef struct {
   /** Type of bus device is on */
   vmk_BusType busType;
   /** Bus-specific address for device */
   char *busAddress;
   /**
    * String length of bus-specific address (excluding terminating NUL).
    * The maximum length supported is 511.
    */
   vmk_uint32 busAddressLen;
   /** Bus-specific identifier for device */
   char *busIdentifier;
   /**
    * String length of bus-specific identifier (excluding terminating NUL).
    * The maximum length supported is 63.
    */
   vmk_uint32 busIdentifierLen;
} vmk_DeviceID;

/*
 ***********************************************************************
 * vmk_DeviceRemove --                                           */ /**
 *
 * \ingroup Device 
 * \brief Remove a device. 
 *        
 * This callback is invoked only on devices in an unclaimed state.
 * A device may be removed for operations such as system shutdown,
 * driver unload, or explicit device removal.
 *
 * Driver should carry out any operations required for the physical
 * removal of the device, and the device object must be unregistered 
 * by calling vmk_DeviceUnregister(). Driver's private data for the
 * device, including registeringDriverData, must be freed only after 
 * successful unregistration of the device. 
 * 
 * If the device is not unregistered, this callback may be invoked 
 * again later. 
 *
 * \param[in]  device   Handle to device to be removed.
 *
 * \retval VMK_OK          Device unregistered successfully.
 * \retval VMK_FAILURE     Driver could not unregister device. 
 *                         All other error codes are treated as 
 *                         VMK_FAILURE.
 *         
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_DeviceRemove)(vmk_Device device);

/**
 * \brief Device operations.
 */
typedef struct {
   /** \brief Remove a device from the system. */
   vmk_DeviceRemove removeDevice;
} vmk_DeviceOps;

/**
 * \brief Device registration data.
 */
typedef struct {
   /** Driver registering this device */
   vmk_Driver registeringDriver;
   /** Device identification */
   vmk_DeviceID *deviceID;
   /** Device operations */
   vmk_DeviceOps *deviceOps;
   /** Opaque data set by registering driver for its private use */
   vmk_AddrCookie registeringDriverData;
   /** Opaque data set by registering driver for attaching driver */
   vmk_AddrCookie registrationData;
} vmk_DeviceProps;

/*
 ***********************************************************************
 * vmk_DeviceRegister --                                          */ /**
 *
 * \ingroup Device
 * \brief Register a device with the device database and get a device 
 *        handle back. 
 *
 * A device can be physical (e.g. a PCI device) or logical 
 * (e.g. an uplink, or a similar software construct).
 *
 * Devices can be registered only from a driver's scan callback.
 *
 * The '#' character is not permitted in the busAddress of the device, 
 * except if it is a logical device whose busAddress is generated using 
 * vmk_LogicalCreateBusAddress.
 *
 * The registering driver's module heap is used for temporary scratch
 * purposes by this service.  No memory allocated from this heap will
 * persist after vmk_DeviceRegister returns.
 *
 * \note This function will not block.
 *
 * \param[in]  deviceProps     Device information. 
 * \param[in]  parent          Parent device handle. 
 * \param[out] newDevice       New device handle.
 *
 * \retval VMK_OK              Successfully registered device.
 * \retval VMK_NOT_SUPPORTED   Registration not from a driver's scan 
 *                             callback.
 * \retval VMK_BAD_PARAM       Device ID is NULL or incomplete. 
 * \retval VMK_BAD_PARAM       The deviceID's busAddress is improperly
 *                             formatted for the specified bus type.
 * \retval VMK_EXISTS          Device with this data already registered. 
 * \retval VMK_NO_MEMORY       Unable to allocate memory for device
 *                             handle.
 * \retval VMK_NO_MODULE_HEAP  The registeringDriver's module
 *                             (specified in deviceProps) has no heap.
 * \retval VMK_NAME_TOO_LONG   The deviceID's busAddressLen is greater
 *                             then the system defined maximum.
 * \retval VMK_NAME_TOO_LONG   The deviceID's busIdentifierLen is
 *                             greater then the system defined maximum.
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_DeviceRegister(vmk_DeviceProps *deviceProps,
                   vmk_Device parent,
                   vmk_Device *newDevice);

/*
 ***********************************************************************
 * vmk_DeviceUnregister --                                        */ /**
 *
 * \ingroup Device
 * \brief Unregister a device from the device database.
 *
 * A device must be unregistered only from its device remove callback
 * provided by the registering driver. 
 *
 * \note This function will not block.
 *
 * \param[in] device       Device handle.
 *
 * \retval VMK_OK          Successfully unregistered device.
 * \retval VMK_NOT_SUPPORTED Unregistration was not from the device's 
 *                           remove callback.
 * \retval VMK_BAD_PARAM   Device handle is invalid.
 * \retval VMK_BUSY        Device has references or resources allocated. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceUnregister(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DeviceGetDeviceID --                                       */ /**
 *
 * \ingroup Device
 * \brief Return a handle to device identification data.
 *
 * \note This function will not block.
 *
 * \param[in]  heap        Heap from which device ID should be allocated.
 * \param[in]  device      Device handle.
 * \param[out] devID       Device identification data.
 *
 * \retval VMK_OK          Successfully returned device id.
 * \retval VMK_BAD_PARAM   Device handle is invalid.
 * \retval VMK_NO_MEMORY   Unable to allocate memory for device ID.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetDeviceID(vmk_HeapID heap,
                      vmk_Device device, 
                      vmk_DeviceID **devID);

/*
 ***********************************************************************
 * vmk_DevicePutDeviceID --                                       */ /**
 *
 * \ingroup Device
 * \brief Release device identification data.
 *
 * \note This function will not block.
 *
 * \param[in] heap         Heap from which device ID was allocated. 
 * \param[in] devID        Device identification data.
 *
 * \retval VMK_OK          Successfully released device id data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DevicePutDeviceID(vmk_HeapID heap,
                      vmk_DeviceID *devID);

/*
 ***********************************************************************
 * vmk_DeviceGetRegisteringDriverData --                   */ /**
 *
 * \ingroup Device
 * \brief Get registering driver's private data for device.
 *
 * \note This function will not block.
 *
 * \param[in]  device      Device handle.
 * \param[out] data        Registering driver's private data for device.
 *
 * \retval VMK_OK          Successfully returned device data.
 * \retval VMK_BAD_PARAM   Invalid device handle.
 * \retval VMK_BAD_PARAM   data argument is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetRegisteringDriverData(vmk_Device device, 
                                   vmk_AddrCookie *data);


/*
 ***********************************************************************
 * vmk_DeviceGetRegistrationData --                               */ /**
 *
 * \ingroup Device
 * \brief Get device registration data for attaching driver. 
 *
 * \note This function will not block.
 *
 * \param[in]  device      Device handle.
 * \param[out] data        Device registration data.
 *
 * \retval VMK_OK          Successfully returned device data.
 * \retval VMK_BAD_PARAM   Invalid device handle.
 * \retval VMK_BAD_PARAM   data argument is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetRegistrationData(vmk_Device device, 
                              vmk_AddrCookie *data);

/*
 ***********************************************************************
 * vmk_DeviceGetAttachedDriverData --                      */ /**
 *
 * \ingroup Device
 * \brief Get attached driver's private data for device.
 *
 * \note This function will not block.
 *
 * \param[in]  device      Device handle.
 * \param[out] data        Attached driver's private data for device.
 *
 * \retval VMK_OK          Successfully returned private data.
 * \retval VMK_BAD_PARAM   Invalid device handle.
 * \retval VMK_BAD_PARAM   data argument is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetAttachedDriverData(vmk_Device device, 
                                vmk_AddrCookie *data);

/*
 ***********************************************************************
 * vmk_DeviceSetAttachedDriverData --                     */ /**
 *
 * \ingroup Device
 * \brief Set attached driver's private data for device.
 *
 * \note This function will not block.
 *
 * \param[in] device       Device handle.
 * \param[in] data         Attached driver's private data for device.
 *
 * \retval VMK_OK          Successfully set private data.
 * \retval VMK_BAD_PARAM   Invalid device handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceSetAttachedDriverData(vmk_Device device, 
                                vmk_AddrCookie data);

/*
 ***********************************************************************
 * vmk_DeviceRelease --                                     */ /**
 *
 * \ingroup Device
 * \brief Release the device reference obtained using any of the Find
 *        functions.
 *
 * \note This function will not block.
 *
 * \param[in] device       Device to be released.
 *
 * \retval VMK_OK          Successfully released reference on device.
 * \retval VMK_BAD_PARAM   Device handle is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceRelease(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DeviceRequestRescan --                                     */ /**
 *
 * \ingroup Device 
 * \brief Request a rescan of the device to register new children.
 *        
 * This call submits a request to the device layer to schedule an 
 * invocation of the device driver's vmk_DriverScanDevice() callback.
 *
 * \param[in]   driver  Requesting driver 
 * \param[in]   device  Device to be scanned
 *
 * \retval VMK_OK            Request accepted. 
 * \retval VMK_BAD_PARAM     Invalid device handle. 
 * \retval VMK_NO_PERMISSION Driver cannot submit this request. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceRequestRescan(vmk_Driver driver, 
                        vmk_Device device);

#endif /* _VMKAPI_DEVICE_H_ */
/** @} */
