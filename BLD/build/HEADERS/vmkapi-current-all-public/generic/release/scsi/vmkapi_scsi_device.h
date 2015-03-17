/***************************************************************************
 * Copyright 2008 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */


/*
 * vmkapi_scsi_device.h --
 *
 *	SCSI device related API.
 *  
 */
#ifndef _VMKAPI_SCSI_DEVICE_H_
#define _VMKAPI_SCSI_DEVICE_H_

#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif

struct vmk_ScsiDevice;

/*
 ***********************************************************************
 * vmk_ScsiDeviceGetIdFromPage83 --                               */ /**
 *
 * \ingroup SCSI
 *
 * \brief Get the device UID of given type from page 83 inquiry buffer
 *
 * Return device uid (max upto 'idLength' bytes) of the requested
 * device uid type/association in caller-provided buffer 'id'. If desc
 * value is non NULL return the identifier descriptor header.
 *
 * \note The returned descriptor is not NULL terminated (many ID types
 *       can contain NULL as valid data). The length of the returned
 *       descriptor is in the desc->idLen field.
 *
 * \note This function will not block.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in]  vmkDevice  SCSI device whose id is requested.
 * \param[in]  idType     SCSI identifier type
 * \param[in]  assoc      SCSI association type
 * \param[in]  idLength   Size of identifier buffer
 * \param[out] id         Identifier buffer
 * \param[out] desc       Buffer of type vmk_ScsiInquiryVPD83IdDesc for
 *                        returning identifier descriptor 
 *
 * \retval VMK_OK   Requested id was found and returned in'id' buffer.
 * \retval Other    An error occured.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_ScsiDeviceGetIdFromPage83(
   struct vmk_ScsiDevice *vmkDevice,
   vmk_ScsiIdentifierType idType,
   vmk_ScsiAssociationType assoc,
   vmk_ByteCountSmall idLength,
   vmk_uint8 *id,
   vmk_ScsiInquiryVPD83IdDesc *desc);

/* 
 *********************************************************************** 
 * vmk_ScsiRegisterDeviceEvent --                                 */ /** 
 * 
 * \ingroup SCSI   
 * 
 * \brief Add an Event Handler for the event type on the device.
 *
 * This function registers a handler that will be called when a
 * certain event on the device occurs.
 * 
 * \note This function may block.
 * 
 * \param[in]  deviceName  Scsi Device name to register a callback for.
 * \param[in]  eventType   Event to be notified of.
 * \param[in]  handlerCbk  Callback to be called when the event occurs.
 * \param[in]  parameter   Parameter to the callback handler.
 * 
 * \retval VMK_OK              Event handler successfully registered.
 * \retval VMK_NO_MEMORY       Event handler registration failed.
 * \retval VMK_NOT_FOUND       Device was not found.
 * 
 *********************************************************************** 
 */ 
VMK_ReturnStatus
vmk_ScsiRegisterDeviceEvent(const char *deviceName,
                            vmk_ScsiDeviceEventType eventType,
                            vmk_ScsiEventHandlerCbk handlerCbk,
                            vmk_AddrCookie parameter);

/* 
 *********************************************************************** 
 * vmk_ScsiUnregisterDeviceEvent --                               */ /** 
 * 
 * \ingroup SCSI   
 * 
 * \brief Remove the specified event handler for the event type.
 * 
 * This function unregisters an event handler earlier registered by
 * vmk_ScsiRegisterDeviceEvent. All arguments need to match those that
 * were used to register the handler in order for the call to succeed.
 *
 * \note This function may block.
 * 
 * \param[in]  deviceName  Scsi Device name to unregister a callback for.
 * \param[in]  eventType   Event to unregister from.
 * \param[in]  handlerCbk  Callback to be unregistered.
 * \param[in]  parameter   Parameter of the callback handler.
 * 
 * \retval VMK_OK              Event handler unregistered successfully.
 * \retval VMK_NOT_FOUND       Device was not found.
 * 
 *********************************************************************** 
 */ 
VMK_ReturnStatus
vmk_ScsiUnregisterDeviceEvent(const char *deviceName,
                              vmk_ScsiDeviceEventType eventType,
                              vmk_ScsiEventHandlerCbk handlerCbk,
                              vmk_AddrCookie parameter);

#endif //_VMKAPI_SCSI_DEVICE_H_
