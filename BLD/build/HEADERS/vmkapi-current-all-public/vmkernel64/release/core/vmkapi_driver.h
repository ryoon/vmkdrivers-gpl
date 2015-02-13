
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
 * vmk_DriverAttachDevice --                                         */ /**
 *
 * \brief Attach a device to a driver.
 *
 * The driver should start driving this device. If the driver is not 
 * capable of driving the given device, an appropriate error should be 
 * returned, and the device must be restored to its original state at 
 * entry. 
 *
 * \param[in]  device 	Handle to device to be added to the driver.
 *
 * \retval VMK_OK          	Success 
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverAttachDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverDetachDevice --                                      */ /**
 *
 * \brief Detach a device from its driver.
 *
 * The driver should stop driving this device, and release its resources. 
 *        
 * \param[in]  device	Handle to device to be removed from the driver.
 *
 * \retval VMK_OK          Success 
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverDetachDevice)(vmk_Device device);


/*
 ***********************************************************************
 * vmk_DriverQuiesceDevice --                                    */ /**
 *
 * \brief Quiesce a device. 
 *       
 * This callback is invoked in preparation for device removal or 
 * system shutdown. The driver should complete any IO on the device 
 * and flush any device caches as necessary to put the device in a 
 * quiescent state.
 *
 * \param[in]  device	Handle to device to be quiesced.
 *
 * \retval VMK_OK          Success 
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverQuiesceDevice)(vmk_Device device);

/*
 ***********************************************************************
 * vmk_DriverScanDevice --                                        */ /**
 *
 * \brief Scan a device for new children and register them. 
 *        
 * Only bus drivers will typically need to implement this entry.
 * This function is called at least once after a device has been 
 * successfully attached to a driver. It may be called at other 
 * device hotplug events as appropriate.
 *
 * \param[in]   device  Handle to device to scan. 
 *
 * \retval VMK_OK          Success 
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_DriverScanDevice)(vmk_Device device);

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
   /** \brief Quiesce a device for system shutdown */
   vmk_DriverQuiesceDevice quiesceDevice;
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
 *        a driver handle back.
 *
 * \note This function will not block.
 *
 * \param[in]  driverProps Driver registration data
 * \param[out] driver      New driver handle.
 *
 * \retval VMK_BAD_PARAM   Name or ops argument is NULL.
 * \retval VMK_NO_MEMORY   Unable to allocate memory for device handle.
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
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DriverGetPrivateData(vmk_Driver driver, 
                         vmk_AddrCookie *data);

#endif /* _VMKAPI_DRIVER_H_ */
/** @} */
/** @} */

