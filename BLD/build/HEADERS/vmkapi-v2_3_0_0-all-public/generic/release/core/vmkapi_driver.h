
/***************************************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Driver                                                        */ /**
 * \addtogroup Device 
 * @{
 * \defgroup Driver Driver Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_DRIVER_H_
#define _VMKAPI_DRIVER_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_DriverAttachDevice --                                      */ /**
 *
 * \brief Attach a device to a driver.
 *
 * This callback is invoked to offer an unclaimed device to a driver.
 * 
 * The driver should check whether it is capable of driving the given 
 * device, and do initial device set up, e.g. allocate device resources. 
 * If the driver is not capable of driving the given device, the device 
 * must be restored to its original state at callback entry, and an 
 * error should be returned. 
 *
 * \param[in]  device 	Handle to device to be added to the driver.
 *
 * \retval VMK_OK          Driver has claimed this device.	
 * \retval VMK_FAILURE     Driver did not claim this device. 
 *                         All other error codes are treated as 
 *                         VMK_FAILURE.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverAttachDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverStartDevice--                                        */ /**
 *
 * \brief  Prepare a device to accept IO. 
 * 
 * This callback is invoked to place the device in an IO-able state. 
 * This can be when the device is implicitly in a quiescent state after 
 * a successful driver attach operation, or at any other time when the
 * device has been explicitly put in a quiescent state by the callback
 * vmk_DriverQuiesceDevice().
 *
 * The driver should prepare the device for IO.
 * 
 * \param[in]   device  Handle to device to prepare for IO.
 *
 * \retval VMK_OK        Driver will accept IO for this device.
 * \retval VMK_FAILURE    Driver could not prepare device for IO. 
 *                        All other error codes are treated as 
 *                        VMK_FAILURE.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverStartDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverScanDevice --                                        */ /**
 *
 * \brief Register any child devices. 
 *       
 * This callback is invoked only on devices in IO-able state. It is 
 * invoked at least once after a device has been successfully attached 
 * to a driver and started. It may be invoked for other device hotplug 
 * events as appropriate. 
 *
 * The driver may register new devices by calling vmk_DeviceRegister()
 * from this callback. New devices may be registered only from this 
 * callback.
 *
 * \param[in]  device  Handle to device whose children may be registered. 
 *
 * \retval VMK_OK          Devices registered, or nothing to register. 
 * \retval VMK_FAILURE     Driver could not register a child device. 
 *                         All other error codes are treated as 
 *                         VMK_FAILURE.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverScanDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverQuiesceDevice --                                    */ /**
 *
 * \brief Place a device in quiescent state. 
 *       
 * This callback may be invoked any time after a device is in an IO-able
 * state, in preparation for operations such as system shutdown, driver 
 * unload, or device removal. 
 *
 * The driver should complete any IO on the device and flush any device 
 * caches as necessary to place the device in a quiescent state. When a 
 * device is in quiescent state, the driver must not report any IO to,
 * and will not receive any IO from, any kernel subsystem.
 *
 * \param[in]  device	Handle to device to be quiesced.
 *
 * \retval VMK_OK          Device has been quiesced. 
 * \retval VMK_FAILURE     Driver could not quiesce IO on device. 
 *                         All other error codes are treated as 
 *                         VMK_FAILURE.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverQuiesceDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverDetachDevice --                                      */ /**
 *
 * \brief Detach a device from its driver.
 *
 * This callback is invoked only on devices in a quiescent state.
 * A device may be detached for operations such as system shutdown, 
 * driver unload, or device removal. 
 *
 * The driver should stop driving this device, and undo all device setup
 * performed in vmk_DriverAttachDevice. E.g. release device resources. 
 *        
 * \param[in]  device	Handle to device to detach from the driver.
 *
 * \retval VMK_OK          Device has been released.
 * \retval VMK_FAILURE     Driver could not detach itself from device. 
 *                         All other error codes are treated as 
 *                         VMK_FAILURE.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverDetachDevice)(vmk_Device device);


/*
 ***********************************************************************
 * vmk_DriverForgetDevice--                                       */ /**
 *
 * \brief  Mark a device as inaccessible. 
 *        
 * This callback is a notification. It may be invoked at any time to 
 * notify the driver that a device is inaccessible, so that the driver 
 * does not wait indefinitely for any subsequent device operations. 
 * 
 * The driver must note that the device is inaccessible. The driver must 
 * return successfully, in deterministic time, on any subsequent device 
 * callbacks, e.g. vmk_DriverQuiesceDevice, vmk_DriverDetachDevice.
 *
 * \param[in]   device  Handle to device that is inaccessible.
 *
 ***********************************************************************
 */
typedef void (*vmk_DriverForgetDevice)(vmk_Device device);

/**
 * \brief Driver operations.
 */
typedef struct {
   /** \brief Attach a device to a driver */
   vmk_DriverAttachDevice attachDevice;
   /** \brief Scan a device for new child devices */
   vmk_DriverScanDevice scanDevice;
   /** \brief Detach a device from its driver */
   vmk_DriverDetachDevice detachDevice;
   /** \brief Quiesce a device */
   vmk_DriverQuiesceDevice quiesceDevice;
   /** \brief Prepare device for IO */
   vmk_DriverStartDevice startDevice;
   /** \brief Notify driver of a lost device */
   vmk_DriverForgetDevice forgetDevice;
} vmk_DriverOps;

/**
 * \brief Driver registration data.
 */
typedef struct {
   /** Module registering this driver */
   vmk_ModuleID moduleID;
   /** Identifying name for the driver */
   vmk_Name name;
   /** Driver operations */
   vmk_DriverOps *ops;
   /** Driver private data */
   vmk_AddrCookie privateData;
} vmk_DriverProps;

/*
 ***********************************************************************
 * vmk_DriverRegister --                                          */ /**
 *
 * \brief Register a driver with the driver database and get a 
 *        a driver handle back. This must be called only within
 *        the module's initialization routine.
 *
 * \note This function will not block.
 *
 * \param[in]  driverProps Driver registration data
 * \param[out] driver      New driver handle.
 *
 * \retval VMK_BAD_PARAM   Name or ops argument is NULL.
 * \retval VMK_EXISTS      A driver by this name is already registered.
 * \retval VMK_NO_MEMORY   Unable to allocate memory for device handle.
 * \retval VMK_NOT_FOUND   Unable to find module registering this driver.
 * \retval VMK_OK          Successfully registered driver.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DriverRegister(vmk_DriverProps *driverProps,
                   vmk_Driver *driver);

/*
 ***********************************************************************
 * vmk_DriverUnregister --                                        */ /**
 *
 * \brief Unregister a driver from the driver database. 
 *
 * \note This function will not block.
 *
 * \param[in]  driver      Driver handle 
 *
 * \retval VMK_OK          Successfully unregistered driver.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DriverUnregister(vmk_Driver driver);

/*
 ***********************************************************************
 * vmk_DriverGetPrivateData --                                    */ /**
 *
 * \brief Get private data for driver. 
 *
 * \note This function will not block.
 *
 * \param[in]  driver      Driver handle 
 * \param[out] data        Driver data.
 *
 * \retval VMK_OK          Successfully returned driver private data.
 * \retval VMK_BAD_PARAM   Invalid driver handle.
 * \retval VMK_BAD_PARAM   data argument is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DriverGetPrivateData(vmk_Driver driver, 
                         vmk_AddrCookie *data);

#endif /* _VMKAPI_DRIVER_H_ */
/** @} */
/** @} */



