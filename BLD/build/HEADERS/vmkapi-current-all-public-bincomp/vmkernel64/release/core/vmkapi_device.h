/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
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
 * \brief Opaque device type 
 */
typedef struct vmkDevice* vmk_Device;

/** \brief A null device handle. */
#define VMK_DEVICE_NONE ((vmk_Device )0)

/**
 * \brief Opaque driver type.
 */
typedef struct vmkDriver* vmk_Driver; 

/**
 * \brief Device identification.
 */
typedef struct {
   /** Type of bus device is on */
   vmk_BusType busType;
   /** Bus-specific address for device */
   char *busAddress;
   /** Length of bus-specific address */
   vmk_uint32 busAddressLen;
   /** Bus-specific identifier for device */
   char *busIdentifier;
   /** Length of bus-specific identifier */
   vmk_uint32 busIdentifierLen;
} vmk_DeviceID;

/*
 ***********************************************************************
 * vmk_DeviceRemove --                                           */ /**
 *
 * \ingroup Device 
 * \brief Remove a device. 
 *        
 * Driver should carry out any operations required for the physical 
 * removal of a device, and unregister the device object using
 * vmk_DeviceUnregister().
 *
 * \param[in]  device   Handle to device to be removed.
 *
 * \retval VMK_OK          Success 
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
   /** Module registering this device */
   vmk_ModuleID moduleID;
   /** Device identification */
   vmk_DeviceID *deviceID;
   /** Device operations */
   vmk_DeviceOps *deviceOps;
   /** Opaque bus-specific data for this device */
   vmk_AddrCookie busDriverData;
} vmk_DeviceProps;

/*
 ***********************************************************************
 * vmk_DeviceRegister --                                          */ /**
 *
 * \ingroup Device
 * \brief Register a device with the device database and get a 
 *        a device handle back. 
 *
 * This function should be used only by a driver that discovers 
 * new physical devices on any buses spawned from the device 
 * that it is the driver for, e.g. a PCI bridge driver.
 *
 * \note This function will not block.
 *
 * \param[in]  deviceProps     Device information. 
 * \param[in]  parent          Parent device handle. 
 * \param[out] newDevice       New device handle.
 *
 * \retval VMK_BAD_PARAM   Device ID is NULL or incomplete. 
 * \retval VMK_EXISTS      Device with this data already registered. 
 * \retval VMK_NO_MEMORY   Unable to allocate memory for device handle.
 * \retval VMK_OK          Successfully registered device.
 *
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
 * \brief Remove a device from the device database.
 *
 * Devices should typically be unregistered only from the device 
 * remove callback. 
 *
 * \note This function will not block.
 *
 * \param[in] device       Device handle.
 *
 * \retval VMK_OK          Successfully unregistered device.
 * \retval VMK_BAD_PARAM   Device handle is invalid.
 * \retval VMK_BUSY        Device has references. Will be freed after 
 *                         last reference is released.
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
 * \param[in]  device      Device handle.
 * \param[out] devID       Device identification data.
 *
 * \retval VMK_OK          Successfully returned device id.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetDeviceID(vmk_Device device, 
                      vmk_DeviceID **devID);

/*
 ***********************************************************************
 * vmk_DevicePutDeviceID --                                       */ /**
 *
 * \ingroup Device
 * \brief Return a handle to device identification data.
 *
 * \note This function will not block.
 *
 * \param[in] devID        Device identification data.
 *
 * \retval VMK_OK          Successfully released device id data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DevicePutDeviceID(vmk_DeviceID *devID);

/*
 ***********************************************************************
 * vmk_DeviceGetBusDriverData --                                  */ /**
 *
 * \ingroup Device
 * \brief Get bus-driver data for device.
 *
 * \note This function will not block.
 *
 * \param[in]  device      Device handle.
 * \param[out] data        Bus-specific data for device.
 *
 * \retval VMK_OK          Successfully returned device data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetBusDriverData(vmk_Device device, 
                           vmk_AddrCookie *data);

/*
 ***********************************************************************
 * vmk_DeviceSetFunctionDriverData --                             */ /**
 *
 * \ingroup Device
 * \brief Set function-driver data for device.
 *
 * \note This function will not block.
 *
 * \param[in] device       Device handle.
 * \param[in] data         Device data.
 *
 * \retval VMK_OK          Successfully set private data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceSetFunctionDriverData(vmk_Device device, 
                                vmk_AddrCookie data);

/*
 ***********************************************************************
 * vmk_DeviceGetFunctionDriverData --                             */ /**
 *
 * \ingroup Device
 * \brief Get function-driver data for device.
 *
 * \note This function will not block.
 *
 * \param[in]  device      Device handle.
 * \param[out] data        Device data.
 *
 * \retval VMK_OK          Successfully returned private data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetFunctionDriverData(vmk_Device device, 
                                vmk_AddrCookie *data);

/*
 ***********************************************************************
 * vmk_DeviceFindDeviceByIdentifier --                            */ /**
 *
 * \ingroup Device
 * \brief Return a reference to the device that matches the bus type & 
 *        identifier in the given identification data. 
 *
 * If device identifiers are not unique, this function will return 
 * the first device found matching the given identifier.
 *
 * \note Reference must be released using vmk_DeviceRelease.
 * \note This function may block.
 *
 * \param[in]  deviceID    Device identification.
 * \param[out] device      Reference to device matching requested 
 *                         identification.
 *
 * \retval VMK_OK          Successfully returned reference to 
 *                         matching device.
 * \retval VMK_NOT_FOUND   No device with given identification.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_DeviceFindDeviceByIdentifier(vmk_DeviceID *deviceID, 
                                 vmk_Device *device);

/*
 ***********************************************************************
 * vmk_DeviceFindDeviceByAddress --                               */ /**
 *
 * \ingroup Device
 * \brief Return a handle to the device that matches the bus & address 
 *        in the given identification data.
 *
 * \note Reference must be released using vmk_DeviceRelease.
 * \note This function may block.
 *
 * \param[in]  deviceID    Device identification.
 * \param[out] device      Reference to device matching requested 
 *                         identification.
 *
 * \retval VMK_OK          Successfully returned reference to 
 *                         matching device.
 * \retval VMK_NOT_FOUND   No device with given identification.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_DeviceFindDeviceByAddress(vmk_DeviceID *deviceID, 
                              vmk_Device *device);

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
 * vmk_DeviceGetParent--                                          */ /**
 *
 * \ingroup Device 
 * \brief Obtain a handle to the parent device of the given device.
 *       
 * \note Parent device reference must be released using vmk_DeviceRelease.
 * \note This function may block.
 *
 * \param[in]   device  Device handle
 * \param[out]  parent  Parent handle 
 *
 * \retval VMK_OK          Success 
 * \retval VMK_BAD_PARAM   Invalid device handle. 
 * \retval VMK_NOT_FOUND   Device has no parent.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetParent(vmk_Device device,
                    vmk_Device *parent);

/*
 ***********************************************************************
 * vmk_DeviceGetSibling --                                        */ /**
 *
 * \ingroup Device 
 * \brief Obtain a handle to the immediate sibling device of the given 
 *        device.
 *        
 * This always returns a handle to the immediate sibling of a device 
 * even if the given device has multiple sibling devices.
 *
 * \note Sibling device reference must be released using vmk_DeviceRelease.
 * \note This function may block.
 *
 * \param[in]   device  Device handle
 * \param[out]  sibling Sibling handle 
 *
 * \retval VMK_OK          Success 
 * \retval VMK_BAD_PARAM   Invalid device handle. 
 * \retval VMK_NOT_FOUND   Device has no siblings. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetSibling(vmk_Device device,
                     vmk_Device *sibling);

/*
 ***********************************************************************
 * vmk_DeviceGetChild --                                          */ /**
 *
 * \ingroup Device 
 * \brief Obtain a handle to the first child device of the given device.
 *        
 * This always returns a handle to the first child of a device even if
 * the given device has multiple child devices.
 *
 * \note Child device reference must be released using vmk_DeviceRelease.
 * \note This function may block.
 *
 * \param[in]   device  Device handle
 * \param[out]  child   Child handle 
 *
 * \retval VMK_OK          Success 
 * \retval VMK_BAD_PARAM   Invalid device handle. 
 * \retval VMK_NOT_FOUND   Device has no children. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DeviceGetChild(vmk_Device device,
                   vmk_Device *child);

#endif /* _VMKAPI_DEVICE_H_ */
/** @} */
