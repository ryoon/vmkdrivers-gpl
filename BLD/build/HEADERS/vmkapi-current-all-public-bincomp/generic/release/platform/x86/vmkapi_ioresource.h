/***************************************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * IOResource                                                     */ /**
 * \defgroup IOResource Generic IO resource interface.
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_IORESOURCE_H_
#define _VMKAPI_IORESOURCE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief IOResource type */
typedef enum {
   VMK_IORESOURCE_NONE    = 0,
   VMK_IORESOURCE_UNKNOWN = 1,
   VMK_IORESOURCE_MEM     = 2,
   VMK_IORESOURCE_PORT    = 3,
} vmk_IOResourceType;

/** \brief IOResource address */
typedef struct vmk_IOResourceAddress {
   /** Resource type */
   vmk_IOResourceType type;
   /** Physical address of resource */
   union {
      vmk_MA memory;
      vmk_IOPortAddr port;
   } address;
} vmk_IOResourceAddress;

/** \brief IOResource properties 
 *
 * Properties of an io resource as defined at registration.
 * These define what an io resource is capable of.
 * Attributes specified for io reservation and/or mapping 
 * requests are checked for conflict against these. 
 */
typedef enum {
   /** Resource can be accessed by multiple users */
   VMK_IORESOURCE_SHAREABLE=0x1,
   /** Resource accesses can be safely cached */
   VMK_IORESOURCE_CACHEABLE=0x2,
   /** Resource is a prefetchable region */
   VMK_IORESOURCE_PREFETCHABLE=0x4,
} vmk_IOResourceAttrs;

/** \brief IOResource reservation flags */
typedef enum {
   /** Request exclusive use of resource */
   VMK_IORESOURCE_RESERVE_EXCLUSIVE=0x1,
} vmk_IOReservationAttrs;

/** \brief IOResource description */
typedef struct vmk_IOResourceInfo {
   /** Module registering the resource */
   vmk_ModuleID moduleID;
   /** Resource begins at this physical address */
   vmk_IOResourceAddress start;
   /** Number of bytes from the start */
   vmk_ByteCount len;
   /** Special attributes of this resource */
   vmk_IOResourceAttrs attrs; 
} vmk_IOResourceInfo;

/** \brief IOResource handle */
typedef struct vmkIOResource* vmk_IOResource;

/** \brief IOResource reservation handle */
typedef struct vmkIOReservation* vmk_IOReservation;


/*
 ***********************************************************************
 * vmk_IOResourceRegister --                                     */ /**
 *
 * \brief Register an IO resource with the vmkernel.
 *        
 * \note This function will not block.
 *
 * \param[in]   resourceInfo    IOResource description data.
 * \param[out]  handle          Handle to registered resource.
 *
 * \retval VMK_OK          Success.
 * \retval VMK_BAD_PARAM   IOResource information incomplete.
 * \retval VMK_NO_MEMORY   Couldn't allocate memory for resource.
 * \retval VMK_BAD_ADDR_RANGE IOResource overlaps with an existing 
 *                            resource.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceRegister(vmk_IOResourceInfo *resourceInfo,
                       vmk_IOResource *handle);


/*
 ***********************************************************************
 * vmk_IOResourceUnregister --                                   */ /**
 *
 * \brief Unregister a resource from the vmkernel.
 *
 * \note This function will not block.
 *
 * \param[in]   handle IOResource to unregister.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        Invalid handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceUnregister(vmk_IOResource handle);


/*
 ***********************************************************************
 * vmk_IOResourceReserve --                                      */ /**
 *
 * \brief Reserve a resource.
 *
 * \note This function will not block.
 *
 * \param[in]   resourceInfo    Requested resource information.
 * \param[out]  reservation     Reservation handle.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_NO_MODULE_HEAP   Caller has no heap to allocate memory.
 * \retval VMK_NO_MEMORY        Couldn't allocate reservation handle.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match existing resource.
 * \retval VMK_NOT_SHARED       Reservation conflict.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceReserve(vmk_IOResourceInfo *resourceInfo,
                      vmk_IOReservation *reservation);


/*
 ***********************************************************************
 * vmk_IOResourceRelease --                                      */ /**
 *
 * \brief Release a resource reservation.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     Reservation handle.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        Invalid handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceRelease(vmk_IOReservation reservation);


/*
 ***********************************************************************
 * vmk_IOResourceBindToDevice --                                  */ /**
 *
 * \brief Associate a resource with a device.
 *
 * This call is used to attach an IO resource to a device registered 
 * with the vmkernel. A device may not always be known when registering
 * an IO resource if that device is discovered by probing a known 
 * IO resource.
 *
 * \note Resource must be removed from the device using
 *       vmk_IOResourceUnbindFromDevice 
 *        
 * \note This function may block.
 *
 * \param[in]   resource        IOResource handle.
 * \param[in]   device          Device handle.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        Resource or device handle is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceBindToDevice(vmk_IOResource resource,
                           vmk_Device device);


/*
 ***********************************************************************
 * vmk_IOResourceUnbindFromDevice --                              */ /**
 *
 * \brief Remove a resource from a device.
 *       
 * \note This function may block.
 *
 * \param[in]   resource        IOResource handle.
 * \param[in]   device          Device handle.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        Resource or device handle is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOResourceUnbindFromDevice(vmk_IOResource resource,
                               vmk_Device device);

#endif
/** @} */
