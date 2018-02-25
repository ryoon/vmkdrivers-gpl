/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * SCSI I/O Device Management(IODM) Interfaces                    */ /**
 *
 * \addtogroup SCSI
 * @{
 *
 * \defgroup IODM SCSI I/O Device Management(IODM) interfaces
 *
 * IODM interfaces allow SCSI drivers to notify VMkernel of any events
 * happening on the HBA(Host Bus Adapter).
 *
 * For example any Fiber Channel driver can use IODM interfaces to
 * notify VMkernel of FC link related events, target port state
 * change notifications, FC dropped frame instances etc.
 *
 * These interfaces are to be used only by Native SCSI drivers and
 * vmklinux module, but not by vmklinux SCSI drivers.
 *
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SCSI_IODM_H_
#define _VMKAPI_SCSI_IODM_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Types of IODM events.
 */
typedef enum {
   /** \brief Event ID for an I/O Error.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to the address of the failed 'vmk_ScsiCommand' and
    * 'data' parameter set to 0.
    */
   VMK_IODM_IOERROR,
   /** \brief Event ID for a Fibre Channel(FC) RSCN event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to NULL and
    * 'data' parameter set to the FCID of the port that the FC driver needs to act on.
    */
   VMK_IODM_RSCN,
   /** \brief Event ID for an FC link up event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to NULL and
    * 'data' parameter set to 0.
    */
   VMK_IODM_LINKUP,
   /** \brief Event ID for an FC link down event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to NULL and
    * 'data' parameter set to 0.
    */
   VMK_IODM_LINKDOWN,
   /** \brief Event ID for an FC frame drop event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to the address of the 'vmk_ScsiCommand' and
    * 'data' parameter set to the FC residual data bytes count(FCP_RESID).
    */
   VMK_IODM_FRAMEDROP,
   /** \brief Event ID for a SCSI LUN reset event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to NULL and 'data' parameter set to
    * the value encoded by (C << 48 | T << 32 | L), where C, T, L
    * represent Channel, Target and LUN IDs of the SCSI device respectively.
    */
   VMK_IODM_LUNRESET,
   /** \brief Event ID for an FCoE Clear Virutal Link(CVL) event.
    * \note To notify this event, vmk_IodmEvent() should be called with
    * 'dataPtr.ptr' parameter set to NULL and
    * 'data' parameter set to the FCID of the vport that received the CVL.
    */
   VMK_IODM_FCOE_CVL,
   /*
    * Whenever a new IODM event is added to the 'vmk_IodmEventType'
    * enum, make sure its corresponding string is added to the
    * 'VMK_IODM_EVENT_STRINGS' macro below in the same exact order.
    */
   VMK_IODM_EVENT_LAST,
} vmk_IodmEventType;

/** \cond nodoc */
#define VMK_IODM_EVENT_STRINGS                            \
   VMK_IODM_EVENT_STR(VMK_IODM_IOERROR, "IO_ERROR")       \
   VMK_IODM_EVENT_STR(VMK_IODM_RSCN, "RSCN")              \
   VMK_IODM_EVENT_STR(VMK_IODM_LINKUP, "LINK_UP")         \
   VMK_IODM_EVENT_STR(VMK_IODM_LINKDOWN, "LINK_DOWN")     \
   VMK_IODM_EVENT_STR(VMK_IODM_FRAMEDROP, "FRAME_DROP")   \
   VMK_IODM_EVENT_STR(VMK_IODM_LUNRESET, "LUN_RESET")     \
   VMK_IODM_EVENT_STR(VMK_IODM_FCOE_CVL, "FCOE_CVL")
/** \endcond */

/*
 ***********************************************************************
 * vmk_IodmEnableEvents --                                        */ /**
 *
 * \brief Enable IODM event notification for a SCSI Adapter.
 *
 * This function enables IODM event collection for a SCSI Adapter.
 * SCSI drivers call this function during HBA registration time.
 * This function will allocate memory to store a reasonable number of
 * IODM events for the SCSI Adapter.
 *
 * \note This function may block.
 *
 * \param[in]  vmkAdapter  SCSI Adapter on which IODM event collection
 *                         is to be enabled.
 *
 * \retval VMK_OK          IODM Event notification is enabled.
 * \retval VMK_NO_MEMORY   Not enough memory to allocate resources.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IodmEnableEvents(vmk_ScsiAdapter *vmkAdapter);

/*
 ***********************************************************************
 * vmk_IodmDisableEvents --                                       */ /**
 *
 * \brief Disable IODM event notification for a SCSI Adapter.
 *
 * This function disables IODM event collection for a SCSI Adapter.
 * SCSI drivers call this function during HBA unregistration time, if
 * IODM event collection was enabled for the HBA during registration.
 * This function will free up the IODM resources allocated for the
 * SCSI Adapter.
 *
 * \note This function may block.
 *
 * \param[in]  vmkAdapter  SCSI Adapter on which IODM is to be disabled.
 *
 * \retval VMK_OK          IODM Event notification is disabled.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IodmDisableEvents(vmk_ScsiAdapter *vmkAdapter);

/*
 ***********************************************************************
 * vmk_IodmEvent --                                               */ /**
 *
 * \brief Notify VMkernel of an IODM event of a SCSI Adapter
 *
 * SCSI drivers call this function to notify VMkernel when
 * an IODM event has occured on the HBA device.
 *
 * \pre IODM event notification should have been enabled.
 *
 * \see  vmk_IodmEnableEvents()
 *
 * \note This is a non-blocking function.
 *
 * \note Spin locks can be held while calling into this function.
 *
 * \note Given an 'eventId', either 'dataPtr' or 'data' or both
 *       parameters will be used to convey IODM event's payload.
 *       Refer to 'vmk_IodmEventType' enum for different 'eventId'
 *       types and their associated 'dataPtr' and 'data' parameters.
 *
 * \param[in]  vmkAdapter SCSI Adapter on which the event occured.
 * \param[in]  eventId    Event type
 * \param[in]  dataPtr    Pointer to payload associated with the event.
 * \param[in]  data       Data associated with the event.
 *
 * \retval VMK_OK         Event successfully added to the pool.
 * \retval VMK_NOT_FOUND  IODM event notification is not enabled for
 *                        the adapter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IodmEvent(vmk_ScsiAdapter *vmkAdapter,
              vmk_IodmEventType eventId,
              vmk_AddrCookie dataPtr,
              vmk_uint64 data);

#endif //_VMKAPI_SCSI_IODM_H_
/** @} */
/** @} */
