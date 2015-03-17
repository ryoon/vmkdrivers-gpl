/************************************************************
 * Copyright 2007-2008, 2010 VMware, Inc.  All rights reserved.
 ************************************************************/

/*
 *
 * iscsi_linux.c --
 *
 *  Annex iSCSI Transport Code: 3rd party media driver Linux Interface
 *
 *
 */

#include "linux/types.h"
#include "vmkapi.h"
#include "scsi_host.h"
#include "scsi_device.h"
#include "scsi_transport_iscsi.h"
#include "scsi_transport.h"

/*
 * for iscsi_register_host and iscsi_unregister_host
 */
#include "scsi_cmnd.h"
#include "vmklinux_9/vmklinux_scsi.h"

/* iscsi_linux revision */
#define ISCSILINUX_REVISION_MAJOR        1
#define ISCSILINUX_REVISION_MINOR        0
#define ISCSILINUX_REVISION_UPDATE       0
#define ISCSILINUX_REVISION_PATCH_LEVEL  0

VMK_VERSION_INFO("Version " VMK_REVISION_STRING(ISCSILINUX)
                                     ", Built on: " __DATE__);

VMK_LICENSE_INFO(VMK_MODULE_LICENSE_GPLV2);

#define ISCSILINUX_MODULE_NAME "iscsi_linux"

static int iscsilinux_loglevel = 1;
static vmk_ModuleID iscsilinuxModuleID;

#define VMK_ADAPTER(hptr)                                                  \
   (((struct vmklnx_ScsiAdapter *)((hptr)->adapter))->vmkAdapter)

#define VMK_ADAPTER_NAME(hptr)                                              \
   (hptr)->adapter ?                                                        \
      vmk_NameToString(                                                     \
      &((struct vmklnx_ScsiAdapter *)((hptr)->adapter))->vmkAdapter->name): \
      "unregistered"

#define VMK_ADAPTER_VMK_NAME(hptr)                                           \
   (hptr)->adapter ?                                                         \
      (&((struct vmklnx_ScsiAdapter *)((hptr)->adapter))->vmkAdapter->name): \
      NULL

#define ISCSILINUX_LOG(level, fmt, arg...)               \
   do {                                                  \
      if (level <= iscsilinux_loglevel) {                \
         vmk_LogMessage("iscsi_linux: [%s:%d] " fmt,     \
            __FUNCTION__, __LINE__, ##arg);              \
      }                                                  \
   } while(0)

#define ISCSILINUX_LOG_SESS_ARGS(sess, fmt, args...)     \
   do {                                                  \
      vmk_LogMessage("iscsi_linux: "                     \
            "[%s: H:%d C:%d T:%d] "                      \
            fmt,                                         \
            VMK_ADAPTER_NAME(sess->host),                \
            sess->host->host_no, sess->channelID,        \
            sess->targetID, ##args);                     \
   } while(0)

#define ISCSILINUX_LOG_ALLOC_SESS(sess, str)                   \
   do {                                                  \
      vmk_LogMessage("iscsi_linux: "                     \
            "[%s: H:%d C:? T:?] "                      \
            str,                                         \
            VMK_ADAPTER_NAME(sess->host),                \
            sess->host->host_no);                        \
   } while(0)

#define ISCSILINUX_LOG_SESS(sess, str)                   \
   do {                                                  \
      vmk_LogMessage("iscsi_linux: "                     \
            "[%s: H:%d C:%d T:%d] "                      \
            str,                                         \
            VMK_ADAPTER_NAME(sess->host),                \
            sess->host->host_no, sess->channelID,        \
            sess->targetID);                             \
   } while(0)

#define ISCSILINUX_LOG2(host, channelID, targetID, str)  \
   do {                                                  \
      vmk_LogMessage("iscsi_linux: "                     \
            "[%s: H:%d C:%d T:%d] "                      \
            str,                                         \
            VMK_ADAPTER_NAME(host),                      \
            host->host_no, channelID, targetID);         \
   } while(0)

static vmk_int32 iscsilinux_VmkReturnStatusToErrno(VMK_ReturnStatus status);
static VMK_ReturnStatus iscsilinux_ErrnoToVmkReturnStatus(vmk_int32 errno);

/**********************************************************************
 *[  Middle Layer Support Functions
 **********************************************************************/
    
/*
 * iscsi_release_device_host
 * Description:  Release a Device. Currently does a scsi_host_put and
 *               device_put on shost_gendev
 * Arguments:    dptr - pointer to the device whose host should be released
 * Returns:      nothing
 * Side effects: XXXSEFD
 */
void
iscsi_release_device_host(
   struct device *dptr
   )
{
   struct Scsi_Host *hptr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dptr != NULL);
   hptr = dev_to_shost(dptr);
   VMK_ASSERT(hptr != NULL);

   put_device(&hptr->shost_gendev);
   scsi_host_put(hptr);
   kfree(dptr);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_release_device_host);

/*
 * iscsi_alloc_session
 * Description:  Initialize the ML specific class object that represents a
 *               session. Note that the size of iscsi_cls_session will vary
 *               depending on the ML implementation. (IE: Open iSCSI version
 *               vs VMWare proprietary version) The new session is not added
 *               to the ML list of sessions for this Media Driver. Media
 *               Drivers should use iscsi_create_session.
 * Arguments:    hptr - Pointer to the scsi_host that will own this target
 *               tptr - Pointer to The transport template passed in as part of 
 *                      iscsi_register_transport
 * Returns:      pointer to the newly allocated session.
 * Side effects: XXXSEFD
 */
struct iscsi_cls_session *
iscsi_alloc_session(
   struct Scsi_Host        *hptr,
   struct iscsi_transport  *tptr
   )
{
   vmk_IscsiTransSession   *transSession = NULL;
   struct iscsi_cls_session *sptr;
   int reg;
   struct device *dptr;
   VMK_ASSERT(hptr != NULL);
   VMK_ASSERT(tptr != NULL);

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   ISCSILINUX_LOG(10, "hptr=%p tptr=%p\n", hptr, tptr);

   if (vmk_IscsiTransportAllocSession((void *)hptr,
                               (vmk_IscsiTransTransport *)tptr->private,
                               &transSession) != VMK_OK) {
      goto sessionAllocFailed;
   }

   sptr = (struct iscsi_cls_session *) transSession->ddData;
   if (tptr->sessiondata_size > 0) {
      sptr->dd_data = (void *)(sptr + 1);
   } else {
      sptr->dd_data = NULL;
   }

   sptr->recovery_tmo = 0;
   transSession->recoveryTimeout = &sptr->recovery_tmo;

   sptr->transport = tptr;
   sptr->host = hptr;
   sptr->private = (void *)transSession;
   
   sptr->device = kmalloc(sizeof(struct device), GFP_KERNEL);
   if (sptr->device == NULL) {
      goto allocateDeviceFailed;
   }

   vmk_Memset(sptr->device, 0, sizeof(struct device));

   dptr = (struct device*)sptr->device;
   VMK_ASSERT(dptr != NULL);
   dptr->parent  = get_device(&hptr->shost_gendev);
   dptr->release = iscsi_release_device_host;

   /**
     * FIXME: XXX Format Proper Name Here
     *        It is not clear what the proper value for this field is, or
     *        how it is used.
     *  Filed under PR#276535
     */
   sprintf(dptr->bus_id, "iscsi.%d", hptr->host_no);
   sptr->hostNo = hptr->host_no;

   if (scsi_host_get(hptr) == NULL) {
      goto hostGetFailed;
   }

   /*
    * After device_register succeeds we need to remember
    * iscsi_release_device_host code path will start to
    * take effect.
    */

   /*
    * After device_register succeeds we need to remember
    * iscsi_release_device_host code path will start to
    * take effect.
    */
   if ((reg = device_register(dptr)) != 0) {
      goto deviceRegisterFailed;
   }

   ISCSILINUX_LOG_ALLOC_SESS(sptr, "session allocated");

   goto ok;

deviceRegisterFailed:
   scsi_host_put(hptr);
hostGetFailed:
   put_device(&hptr->shost_gendev);
   kfree(sptr->device);
allocateDeviceFailed:
   (void)vmk_IscsiTransportFreeSession(transSession);
sessionAllocFailed:                    
   sptr = NULL;
ok:
   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);
   return sptr;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_alloc_session);

/*
 * iscsi_add_session
 * Description:  Adds the session object to the ML thus exposing the target
 *               to the Operating System as a SCSI target.
 * Arguments:    sptr      - A session previously created with
 *                           iscsi_alloc_session
 *               target_id - The target ID for this session
 * Returns:      0 on success, otherwise an OS ERROR.
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_add_session(
   struct iscsi_cls_session   *sptr,
   vmk_uint32                 target_id,
   vmk_uint32                 channel
   )
{
   VMK_ReturnStatus status;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);
   VMK_ASSERT(sptr->device != NULL);
   
   ISCSILINUX_LOG(10, "sptr=%p session=%p device=%p target_id=%d channel=%d\n",
      sptr, sptr->private, sptr->device, target_id, channel);

   sptr->channelID = channel;
   sptr->targetID  = target_id;
   
   if ((status = vmk_IscsiTransportAddSession((vmk_IscsiTransSession *)sptr->private,
                                        sptr->hostNo,
                                        target_id,
                                        channel)) != VMK_OK) {
      goto addSession_failed;
   }

   if (!get_device((struct device*)sptr->device)) {
      goto get_device_failed;
   }

   if (vmklnx_scsi_alloc_target((struct device *)sptr->device,
                                channel,
                                target_id) == NULL) {
      goto scsiAllocTargetFailed;
   }
         
   ISCSILINUX_LOG_SESS(sptr, "session added");
   goto ok;
      
scsiAllocTargetFailed:
   put_device((struct device*)sptr->device);
get_device_failed:
   vmk_IscsiTransportRemoveSession((vmk_IscsiTransSession *)sptr->private);
addSession_failed:
   status = VMK_BAD_PARAM;
ok:
   ISCSILINUX_LOG(10, "status=%x\n", status);
   return iscsilinux_VmkReturnStatusToErrno(status);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_add_session);

/*
 * iscsi_create_session
 * Description:  This function allocates a new iscsi_cls_session object and
 *               adds it to the Media Drivers list of sessions. This is done
 *               by calling iscsi_alloc_session followed by
 *               iscsi_add_session.  Most Media Drivers should use this
 *               interface.
 * Arguments:    hptr      - The host that will be used to create the session
 *               tptr      - Pointer to the iscsi_transport template that was
 *                           previously registered for this host (Media Driver)
 *               target_id - Target ID for the session
 *               channel   - Channel for the session
 * Returns:      pointer to the session created
 * Side effects: XXXSEFD
 */
struct iscsi_cls_session *
iscsi_create_session(struct Scsi_Host *hptr, struct iscsi_transport *tptr,
                     vmk_uint32 target_id, vmk_uint32 channel)
{
   struct iscsi_cls_session *sptr;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(hptr != NULL);
   VMK_ASSERT(tptr != NULL);
   if ((sptr = iscsi_alloc_session(hptr, tptr)) != NULL) {
      if ((iscsi_add_session(sptr, target_id, channel)) != 0) {
         iscsi_free_session(sptr);
         sptr = NULL;
      }
   }

   if (sptr) {
      ISCSILINUX_LOG_SESS(sptr, "session created");
   }

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);
   return sptr;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_create_session);

/*
 * iscsi_offline_session
 * Description:  Prevent additional I/O from being sent to the Media Driver
 *               through queue command.  In addition update scsi_device
 *               states to mark this device SDEV_OFFLINE.  Then notify
 *               the upper layer to rescan the path.
 * Arguments:    sptr - This session to offline
 * Returns:      nothing, function must always succeed.
 * Side effects: XXXSEFD
 */
void
iscsi_offline_session(struct iscsi_cls_session *sptr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);
   VMK_ASSERT(sptr->device != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   vmklnx_scsi_target_offline((struct device*)sptr->device);
   ISCSILINUX_LOG_SESS(sptr, "session offlined");
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_offline_session);

/*
 * iscsi_lost_session
 * Description:  Report that a session was lost and the target has
 *               authoritatively states the session is not coming
 *               back.  This marks the path associated with this
 *               session lost, as a step to marking Permanent Device
 *               Loss (PDL).
 * Arguments:    sptr - This session to offline
 * Returns:      nothing, function must always succeed.
 * Side effects: XXXSEFD
 */
void
iscsi_lost_session(struct iscsi_cls_session *sptr)
{
   VMK_ReturnStatus vmkStat;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);
   VMK_ASSERT(sptr->host != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   vmkStat = vmk_ScsiSetPathLostByDevice(VMK_ADAPTER_VMK_NAME(sptr->host),
                                         sptr->channelID, sptr->targetID, -1);

   if (vmkStat != VMK_OK) {
      vmk_WarningMessage("Failed marking path lost 0x%x.", vmkStat);
   }
      
   ISCSILINUX_LOG_SESS(sptr, "session lost");
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_lost_session);

/*
 * iscsi_remove_session
 * Description:  Remove the specified session from the Mid Layer. The Mid
 *               Layer is responsible for removing the scsi target from the
 *               SCSI layer as well as ensuring any recovery work is cleaned
 *               up.
 * Arguments:    sptr - The session to remove from the ML
 * Returns:      nothing
 * Side effects: XXXSEFD
 */
void
iscsi_remove_session(struct iscsi_cls_session *sptr)
{
   struct Scsi_Host *hptr;
   struct device *dptr;
   vmk_uint32 channel, target;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   dptr = (struct device *)sptr->device;

   /* record host,channel,target for logging since sptr could be freed */
   hptr   = sptr->host;
   channel = sptr->channelID;
   target  = sptr->targetID;

   vmk_IscsiTransportRemoveSession((vmk_IscsiTransSession *)sptr->private);
   scsi_remove_target(dptr);
   put_device(dptr);

   ISCSILINUX_LOG2(hptr, channel, target, "session removed");
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_remove_session);

/*
 * iscsi_free_session
 * Description:  Unregister the scsi device associated with this session, this
 *               should have the effect of iscsi_release_device_host() being called
 *               to release the device resources.  Then free the iscsi session data.
 * Arguments:    sptr - Pointer to the session class object freeing it's memory
 * Returns:      nothing
 * Side effects: XXXSEFD
 */
void
iscsi_free_session(struct iscsi_cls_session *sptr)
{
   struct Scsi_Host *hptr;
   vmk_uint32 channel, target;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   hptr = dev_to_shost((struct device *)sptr->device);
   VMK_ASSERT(hptr != NULL);

   /* record host,channel,target for logging since sptr could be freed */
   channel = sptr->channelID;
   target  = sptr->targetID;

   device_unregister((struct device *)sptr->device);
   vmk_IscsiTransportFreeSession((vmk_IscsiTransSession *)sptr->private);

   ISCSILINUX_LOG2(hptr, channel, target, "session freed");
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_free_session);

/*
 * iscsi_destroy_session
 * Description:  Reverse of iscsi_create_session.  Removes the session from
 *               the ML, dereferences any scsi devices. Frees all resources.
 * Arguments:    sptr - Session to shutdown/destroy.
 * Returns:      XXXRVFD
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_destroy_session(struct iscsi_cls_session *sptr)
{
   struct Scsi_Host *hptr;
   vmk_uint32 channel, target;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   hptr   = sptr->host;
   channel = sptr->channelID;
   target  = sptr->targetID;

   iscsi_remove_session(sptr);
   iscsi_free_session(sptr);

   ISCSILINUX_LOG2(hptr, channel, target, "session destroyed");
   return 0;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_destroy_session);

/*
 * iscsi_create_conn
 * Description:  Create a new connection to the target on the specified session.
 * Arguments:    sptr         - session on which to create a new connection
 *               connectionID - The connection ID/number
 * Returns:      pointer to the newly created connection.
 * Side effects: XXXSEFD
 */
struct iscsi_cls_conn *
iscsi_create_conn(struct iscsi_cls_session *sptr, vmk_uint32 connectionID)
{
   vmk_IscsiTransConnection   *transConn = NULL;
   struct iscsi_cls_conn      *cptr = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   if (vmk_IscsiTransportCreateConnection(sptr->private, connectionID, &transConn) == VMK_OK) {
      if (!get_device((struct device*)sptr->device)) {
         (void)vmk_IscsiTransportDestroyConnection(transConn);
      } else {
         cptr = (struct iscsi_cls_conn *) transConn->ddData;
         cptr->private = (void *)transConn;
         cptr->session = sptr;
         cptr->last_rx_time_jiffies = 0;
         transConn->lastRxTimeJiffies = &cptr->last_rx_time_jiffies;
         if (sptr->transport->conndata_size > 0) {
            cptr->dd_data = (void *)(cptr + 1);
         } else {
            cptr->dd_data = NULL;
         }
      }
   }
   return cptr;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_create_conn);

/*
 * iscsi_destroy_conn
 * Description:  Close the specified connection and remove it from the sessions
 *               list of connections. Leave the session in place.
 * Arguments:    cptr - The connection to shutdown
 * Returns:      0 on success otherwise an OS ERROR
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_destroy_conn(struct iscsi_cls_conn *cptr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(cptr != NULL);
   VMK_ASSERT(cptr->session != NULL);
   VMK_ASSERT(cptr->session->device != NULL);

   ISCSILINUX_LOG(10, "cptr=%p\n", cptr);

   put_device((struct device*)cptr->session->device);
   (void)vmk_IscsiTransportDestroyConnection((vmk_IscsiTransConnection *)cptr->private);
   return 0;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_destroy_conn);

/*
 * iscsi_unblock_session
 * Description:  Allow additional I/O work to be sent to the driver through
 *               it's queuecommand function. It may be necessary to block
 *               the Operating System IO queue.
 * Arguments:    sptr - The session to unblock
 * Returns:      nothing, function must always succeed.
 * Side effects: XXXSEFD
 */
void
iscsi_unblock_session(struct iscsi_cls_session *sptr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   scsi_target_unblock((struct device *)sptr->device);
   vmk_IscsiTransportStopSessionRecoveryTimer((vmk_IscsiTransSession *)sptr->private);

   ISCSILINUX_LOG_SESS(sptr, "session unblocked");
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_unblock_session);

/*
 * iscsi_block_session
 * Description:  Prevent additional I/O from being sent to the Media Driver
 *               through queue command. Any session recovery work should
 *               still continue.
 * Arguments:    sptr - The session to block
 * Returns:      nothing, function must always succeed.
 * Side effects: XXXSEFD
 */
void
iscsi_block_session(struct iscsi_cls_session *sptr)
{
   VMK_ReturnStatus vmkStat;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sptr != NULL);

   ISCSILINUX_LOG(10, "sptr=%p\n", sptr);

   scsi_target_block((struct device *)sptr->device);

   vmkStat = vmk_IscsiTransportStartSessionRecoveryTimer(
                  (vmk_IscsiTransSession *)sptr->private);

   if (vmkStat != VMK_OK) {
      ISCSILINUX_LOG_SESS_ARGS(sptr,
                          "session blocked, however could not "
                          "start the recovery timer, error %x",
                          vmkStat);
   } else {
      ISCSILINUX_LOG_SESS(sptr, "session blocked");
   }
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_block_session);

/*
 *
 * iscsi_conn_error --
 *   Description:  This up call must be made to the ML when a connection
 *                 fails. IE: a target disconnect occurs, etc. This function
 *                 must then inform the user space process of a connection
 *                 error by sending a ISCSI_KEVENT_CONN_ERROR event packet.
 *   arguments:    struct iscsi_cls_conn  *connection   The connection that we
 *                                                      had an error on
 *                 iSCSIErrCode_t          error        The iSCSI error code
 *                                                      that was encountered:
 *                                                      ISCSI_ERR_CONN_FAILED
 *   returns:      NONE
 *   side effects: XXX TBD
 */
void iscsi_conn_error(
   struct iscsi_cls_conn *cptr,
   iSCSIErrCode_t         error
   )
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   ISCSILINUX_LOG(10, "cptr=%p\n", cptr);

   (void)vmk_IscsiTransportReportConnectionError((vmk_IscsiTransConnection *)cptr->private, error);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_conn_error);

/*
 *
 * iscsi_recv_pdu --
 *   Description:  Called to have the ML management code handle a specific PDU
 *                 for us. The required PDU's that must be sent to the ML are:
 *                    ISCSI_OP_NOOP_IN,
 *                    ISCSI_OP_ASYNC_EVENT,
 *                    ISCSI_OP_TEXT_RSP
 *                 The mid layer is then responsible for sending this PDU up
 *                 to the user space daemon for processing using an
 *                 ISCSI_KEVENT_RECV_PDU event packet. Recv pdu will also need
 *                 to handle asynchronous events and not just replies to
 *                 previous send_pdu packets.
 *   arguments:    struct iscsi_cls_conn  *connection   The connection the PDU
 *                                                      arrived on
 *                 struct iscsi_hdr       *PDU          The iSCSI PDU Header
 *                 char                   *data         Any additional data
 *                                                      that arrived with this
 *                                                      PDU
 *                 vmk_uint32             nByteOfData   The size of the
 *                                                      additional data
 *   returns:      OS Error. ( ENOMEM, EINVAL, etc )
 *   side effects: XXX TBD
 */
int
iscsi_recv_pdu(
   struct iscsi_cls_conn   *cptr,
   void                    *pduHdr,
   char                    *pduData,
   vmk_uint32              nByteOfData
   )
{
   VMK_ReturnStatus vmkStat;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   ISCSILINUX_LOG(10, "cptr=%p\n", cptr);

   vmkStat = vmk_IscsiTransportSendReceivedPdu((vmk_IscsiTransConnection *)cptr->private,
                            pduHdr,
                            pduData,
                            nByteOfData);

   return iscsilinux_VmkReturnStatusToErrno(vmkStat);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_recv_pdu);

/*
 * iscsi_scan_target
 * Description:  Perform a scsi_scan_target on the specified target
 * Arguments:    dptr - Device Pointer
 *               chan - Channel
 *               id   - SCSI ID
 *               lun  - SCSI LUN
 * Returns:      Status Code
 * Side effects: XXXSEFD
 * Note: This function is called by iscsi_trans to perform the appropriate
 *       target scan.
 */
void
iscsi_scan_target(struct device *dptr, uint chan, uint id, uint lun, int rescan)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dptr != NULL);
   scsi_scan_target(dptr, chan, id, lun, rescan);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_scan_target);
                                                                      
/*
 * iscsi_user_scan
 * Description:  Performs a scan of all targets registered to this transport.
 * Arguments:    hptr              - Device Host Pointer
 *               chan              - Channel
 *               id                - SCSI ID
 *               lun               - SCSI LUN
 *               iscsi_scan_target - target scanning function
 * Returns:      Status Code
 * Side effects: XXXSEFD
 */
static int 
iscsi_user_scan(struct Scsi_Host *hptr, uint chan, uint id, uint lun)
{
   int res = -EINVAL;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(hptr != NULL);
   return res;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_user_scan);

/*
 *
 * iscsi_if_create_session_done --
 *   Description:  This is a callback from a Media Driver when it has
 *                 completed creating a session or an existing session has
 *                 gone into the "LOGGED" in state.  It may be used by a
 *                 Hardware Media Driver (such as qlogic) that brings it's
 *                 sessions up on boot. (Currently Not Used)
 *   arguments:    struct iscsi_cls_conn  *connection   The connection that is
 *                                                      carrying the session
 *   returns:      0 on success, otherwise an OS error.
 *   side effects: XXX TBD
 */
int
iscsi_if_create_session_done(
   struct iscsi_cls_conn *cptr
   )
{
   VMK_ReturnStatus  status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   status = vmk_IscsiTransportCreateSessionDone((vmk_IscsiTransConnection *)cptr->private);
   return iscsilinux_VmkReturnStatusToErrno(status);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_if_create_session_done);

/*
 *
 * iscsi_if_destroy_session_done --
 *   Description:  This is a callback from the Media Driver to inform the ML
 *                 that a particular session was removed by the Media Driver.
 *                 (Currently Not Used)
 *   arguments:    struct iscsi_cls_conn  *connection   The connection that is
 *                                                      carrying the session
 *   returns:      0 on success, otherwise an OS error.
 *   side effects: XXX TBD
 */
int
iscsi_if_destroy_session_done(
   struct iscsi_cls_conn *cptr
   )
{
  VMK_ReturnStatus  status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   status = vmk_IscsiTransportDestroySessionDone((vmk_IscsiTransConnection *)cptr->private);
   return iscsilinux_VmkReturnStatusToErrno(status);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_if_destroy_session_done);

/*
 *
 * iscsi_if_connection_start_done
 *   Description:  This is a callback from a Media Driver when it has
 *                 completed the connection start operation
 *
 *   arguments:    struct iscsi_cls_conn  *connection   The connection that is
 *                                                      carrying the session
 *                 int error                             Error code for the operation
 *   returns:      0 on success, otherwise an OS error.
 *   side effects: XXX TBD
 */
int
iscsi_if_connection_start_done(
   struct iscsi_cls_conn   *cptr,
   int                     error
   )
{
  VMK_ReturnStatus status, vmkStatus;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkStatus = iscsilinux_ErrnoToVmkReturnStatus(error);
   status = vmk_IscsiTransportConnectionStartDone((vmk_IscsiTransConnection *)cptr->private,
                                               vmkStatus);
   return iscsilinux_VmkReturnStatusToErrno(status);
}

VMK_MODULE_EXPORT_SYMBOL(iscsi_if_connection_start_done);

/*
 *
 * iscsi_if_connection_stop_done
 *   Description:  This is a callback from a Media Driver when it has
 *                 completed the connection stop operation
 *
 *   arguments:    struct iscsi_cls_conn  *connection   The connection that is
 *                                                      carrying the session
 *                 int error                            Error code for the
 *                                                      operation
 *   returns:      0 on success, otherwise an OS error.
 *   side effects: XXX TBD
 */
int
iscsi_if_connection_stop_done(
   struct iscsi_cls_conn   *cptr,
   int                     error
   )
{
   VMK_ReturnStatus status, vmkStatus;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkStatus = iscsilinux_ErrnoToVmkReturnStatus(error);
   status = vmk_IscsiTransportConnectionStopDone((vmk_IscsiTransConnection *)cptr->private,
                                              vmkStatus);

   return iscsilinux_VmkReturnStatusToErrno(status);
}

VMK_MODULE_EXPORT_SYMBOL(iscsi_if_connection_stop_done);

/*
 * Create and initialize a session class object. Do not
 * start any connections.
 */
static VMK_ReturnStatus
iscsilinux_create_session_persistent(
   vmk_IscsiTransTransport  *transTransport,
   vmk_uint16               maxCmds,
   vmk_uint16               qDepth,
   vmk_uint32               initialCmdSeqNum,
   vmk_uint32               targetID,
   vmk_uint32               channelID,
   vmk_uint32               *hostNumber,
   vmk_IscsiTransSession    **outSession
   )
{
   VMK_ReturnStatus           vmkStat = VMK_FAILURE;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   
   tptr = (struct iscsi_transport *)transTransport->ddData;

   sptr = tptr->create_session_persistent(tptr,
                                          tptr->scsi_template,
                                          maxCmds,
                                          qDepth,
                                          initialCmdSeqNum,
                                          targetID,
                                          channelID,
                                          hostNumber);
   if (sptr) {
      VMK_ASSERT(sptr->private != NULL);
      *outSession =  (vmk_IscsiTransSession *)sptr->private;
      vmkStat = VMK_OK;
   }

   return vmkStat;
}

/*
 * Destroy a session class object and tear down a session
 */
static VMK_ReturnStatus
iscsilinux_destroy_session(
   vmk_IscsiTransSession    *transSession
   )
{
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;

   sptr = (struct iscsi_cls_session *)transSession->ddData;

   tptr = (struct iscsi_transport *)sptr->transport;

   tptr->destroy_session(sptr);

   return VMK_OK;
}

/*
 * Create a connection class object
 */
static VMK_ReturnStatus
iscsilinux_create_connection(
   vmk_IscsiTransSession    *transSession,
   vmk_uint32               connId,
   vmk_IscsiTransConnection **outConnection
   )
{
   VMK_ReturnStatus           vmkStat = VMK_FAILURE;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   sptr = (struct iscsi_cls_session *)transSession->ddData;

   tptr = (struct iscsi_transport *)sptr->transport;

   cptr =  tptr->create_conn(sptr, connId);

   if (cptr) {
      VMK_ASSERT(outConnection != NULL);
      VMK_ASSERT(cptr->private != NULL);
      *outConnection = (vmk_IscsiTransConnection *)cptr->private;
      vmkStat = VMK_OK;
   }

   return vmkStat;
}

/*
 * Bind a socket created in user space to a session.
 */
static VMK_ReturnStatus
iscsilinux_bind_connection(
   vmk_IscsiTransSession    *transSession,
   vmk_IscsiTransConnection *transConnection,
   vmk_uint64               socket,
   vmk_Bool                isLeading
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   sptr = (struct iscsi_cls_session *)transSession->ddData;

   tptr = (struct iscsi_transport *)sptr->transport;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   res = tptr->bind_conn(sptr,
                         cptr,
                         socket,
                         isLeading == VMK_TRUE ? 1 : 0);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Enable connection for IO ( Connect )
 */
static VMK_ReturnStatus
iscsilinux_start_connection(
   vmk_IscsiTransConnection *transConnection
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->start_conn(cptr);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Stop IO on the connection ( Disconnect )
 */
static VMK_ReturnStatus
iscsilinux_stop_connection(
   vmk_IscsiTransConnection *transConnection,
   vmk_IscsiStopConnMode    mode
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->stop_conn(cptr, (vmk_int32)mode);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Destroy a connection
 */
static VMK_ReturnStatus
iscsilinux_destroy_connection(
   vmk_IscsiTransConnection *transConnection
   )
{
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   tptr->destroy_conn(cptr);

   return VMK_OK;
}

/*
 * Retrieve session parameters from Media Driver
 */
static VMK_ReturnStatus
iscsilinux_get_session_param(
   vmk_IscsiTransSession      *transSession,
   vmk_IscsiTransIscsiParam   param,
   vmk_int8                   *dataBuf,
   vmk_int32                  *dataLen
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;

   sptr = (struct iscsi_cls_session *)transSession->ddData;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->get_session_param(sptr, param, dataBuf);

   if (res >= 0) {
      *dataLen = res;
      return VMK_OK;
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Set connection/session parameters from Media Driver
 */
static VMK_ReturnStatus
iscsilinux_set_param(
   vmk_IscsiTransConnection *transConnection,
   vmk_IscsiTransIscsiParam param,
   vmk_int8                 *dataBuf,
   vmk_int32                bufLen
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->set_param(cptr, param, dataBuf, bufLen);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Retrieve connection parameters from Media Driver
 */
static VMK_ReturnStatus
iscsilinux_get_connection_param(
   vmk_IscsiTransConnection   *transConnection,
   vmk_IscsiTransIscsiParam   param,
   vmk_int8                   *dataBuf,
   vmk_int32                  *dataLen
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->get_conn_param(cptr, param, dataBuf);

   if (res >= 0) {
      *dataLen = res;
      return VMK_OK;
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Retrieve host configuration parameters from Media Driver
 */
static VMK_ReturnStatus
iscsilinux_get_host_param(
   vmk_IscsiTransTransport *transTransport,
   void                    *hostPrivate,
   vmk_IscsiTransHostParam param,
   vmk_int8                *dataBuf,
   vmk_int32               *dataLen
   )
{
   vmk_int32                  res=-EINVAL;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   if (tptr->get_host_param != NULL) {
      res =  tptr->get_host_param(hostPrivate, param, dataBuf);
   }

   if (res >= 0) {
      *dataLen = res;
      return VMK_OK;
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Set a host configuration parameters for a Media Driver
 */
static VMK_ReturnStatus
iscsilinux_set_host_param(
   vmk_IscsiTransTransport *transTransport,
   void                    *hostPrivate,
   vmk_IscsiTransHostParam  param,
   vmk_int8                 *dataBuf,
   vmk_int32                bufLen
   )
{
   vmk_int32                  res=-EINVAL;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   if (tptr->set_host_param !=NULL) {
      res = tptr->set_host_param(hostPrivate, param, dataBuf, bufLen);
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Send a data PDU to a target. This is a redirect from an
 * external source, like the iscsid daemon.
 */
static VMK_ReturnStatus
iscsilinux_send_pdu(
   vmk_IscsiTransConnection *transConnection,
   void                     *pduHdr,
   vmk_int8                 *pduData,
   vmk_ByteCountSmall       dataLen
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   res = tptr->send_pdu(cptr, pduHdr, pduData, dataLen);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Retrieve per connection statistics from the Media Driver
 */
static VMK_ReturnStatus
iscsilinux_get_stats(
   vmk_IscsiTransConnection *transConnection,
   vmk_IscsiTransIscsiStats *stats
   )
{
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;
   struct iscsi_cls_conn      *cptr;

   cptr = (struct iscsi_cls_conn *)transConnection->ddData;

   sptr = cptr->session;

   tptr = (struct iscsi_transport *)sptr->transport;

   tptr->get_stats(cptr, (struct iscsi_stats *)stats);

   return VMK_OK;
}

/*
 * This will be called by the ML when the session recovery
 * timer has expired and the driver will be opened back up
 * for IO.
 */
static void
iscsilinux_session_recovery_timedout(
   vmk_IscsiTransSession    *transSession
   )
{
   struct iscsi_transport     *tptr;
   struct iscsi_cls_session   *sptr;

   sptr = (struct iscsi_cls_session *)transSession->ddData;

   tptr = (struct iscsi_transport *)sptr->transport;

   ISCSILINUX_LOG_SESS(sptr, "session recovery timeout");

   tptr->session_recovery_timedout(sptr);
   return;
}

/*
 * Connect to the specified destination through a Media Driver.
 * This can be used to get a handle on a socket like handle of
 * the Media Driver to allow user space to drive recovery
 * through iscsid. This extended version provides a handle
 * to the vmknic to use.
 */
static VMK_ReturnStatus
iscsilinux_ep_connect_extended(
   vmk_IscsiTransTransport *transTransport,
   void                    *destAddr,
   vmk_Bool                nonBlocking,
   vmk_uint64              *ep,
   vmk_IscsiNetHandle      iscsiNetHandle
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   res = tptr->ep_connect_extended((struct sockaddr *)destAddr,
                                    nonBlocking == VMK_TRUE ? 1 : 0,
                                    ep,
                                    iscsiNetHandle);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Poll for an event from the media driver
 */
static VMK_ReturnStatus
iscsilinux_ep_poll(
   vmk_IscsiTransTransport *transTransport,
   vmk_uint64                 ep,
   vmk_int32                  timeoutMS,
   vmk_IscsiTransPollStatus   *eventReply
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   res = tptr->ep_poll(ep, timeoutMS);

   if (res >= 0) {
      *eventReply = res == 0 ? 
         VMK_ISCSI_TRANSPORT_POLL_STATUS_NO_DATA :
         VMK_ISCSI_TRANSPORT_POLL_STATUS_DATA_AVAILABLE;
      return VMK_OK;
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Disconnect the socket channel though the Media Driver
 */
static VMK_ReturnStatus
iscsilinux_ep_disconnect(
   vmk_IscsiTransTransport *transTransport,
   vmk_uint64               ep
   )
{
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   tptr->ep_disconnect(ep);

   return VMK_OK;
}

/*
 * Perform a target discovery on a specific ip address
 * Not currently used by any Media Driver
 */
static VMK_ReturnStatus
iscsilinux_target_discover(
   vmk_IscsiTransTransport  *transTransport,
   void                     *hostPrivate,
   vmk_IscsiTransTgtDiscvr  discType,
   vmk_Bool                 enable,
   void                     *discAddress
   )
{
   vmk_int32                  res;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   res = tptr->tgt_dscvr(hostPrivate,
                          discType,
                          enable == VMK_TRUE ? 1 : 0,
                          (struct sockaddr *)discAddress);

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}

/*
 * Get driver limits on various params
 */
static VMK_ReturnStatus
iscsilinux_get_transport_limits(
   vmk_IscsiTransTransport              *transTransport,
   vmk_IscsiTransIscsiParam             param,
   vmk_IscsiTransTransportParamLimits   *paramList,
   vmk_int32                            listMaxLen
   )
{
   vmk_int32                  res = -EINVAL;
   struct iscsi_transport     *tptr;

   tptr = (struct iscsi_transport *)transTransport->ddData;

   if (tptr->get_transport_limit != NULL) {
      res = tptr->get_transport_limit(tptr,
                                      (enum iscsi_param)param,
                                      paramList,
                                      listMaxLen);
   }

   return iscsilinux_ErrnoToVmkReturnStatus(-res);
}
               
/*
 * Translate driver's IscsiParam bit mask to property object
 */
static VMK_ReturnStatus
iscsilinux_TranslateIscsiParamBitMaskToProperty(
   vmk_uint64                       bitMask,
   vmk_IscsiTransIscsiParamProperty *outProp
   )
{
   VMK_ReturnStatus                 vmkStat;
   vmk_uint32                       bitPos;
   vmk_IscsiTransIscsiParamProperty prop;

   vmkStat = vmk_IscsiTransportCreateIscsiParamProperty(&prop);

   if (vmkStat != VMK_OK) {
      return vmkStat;
   }

   while ((bitPos = (vmk_uint32)ffs(bitMask)) != 0) {
      vmk_IscsiTransIscsiParam param=bitPos-1;

      vmkStat = vmk_IscsiTransportAddIscsiParamProperty(prop, param);
      if (vmkStat != VMK_OK) {
         break;
      }

      bitMask &= ~(1 << (bitPos - 1));
   }

   if (vmkStat != VMK_OK) {
      vmk_IscsiTransportDestroyIscsiParamProperty(prop);
   } else {
      *outProp = prop;
   }

   return vmkStat;
}

/*
 * Translate driver's HostParam bit mask to property object
 */
static VMK_ReturnStatus
iscsilinux_TranslateHostParamBitMaskToProperty(
   vmk_uint64                       bitMask,
   vmk_IscsiTransHostParamProperty  *outProp
   )
{
   VMK_ReturnStatus                 vmkStat;
   vmk_uint32                       bitPos;
   vmk_IscsiTransHostParamProperty  prop;

   vmkStat = vmk_IscsiTransportCreateHostParamProperty(&prop);
   if (vmkStat != VMK_OK) {
      return vmkStat;
   }

   while ((bitPos = (vmk_uint32)ffs(bitMask)) != 0) {
      vmk_IscsiTransIscsiParam param=bitPos-1;

      vmkStat = vmk_IscsiTransportAddHostParamProperty(prop, param);
      if (vmkStat != VMK_OK) {
         break;
      }

      bitMask &= ~(1 << (bitPos - 1));
   }

   if (vmkStat != VMK_OK) {
      vmk_IscsiTransportDestroyHostParamProperty(prop);
   } else {
      *outProp = prop;
   }

   return vmkStat;
}

/*
 * Translate driver's capabilities bit mask to property object
 */
static VMK_ReturnStatus
iscsilinux_TranslateCapabilityBitMaskToProperty(
   vmk_uint64                          bitMask,
   vmk_IscsiTransCapabilitiesProperty  *outProp
   )
{
   VMK_ReturnStatus                 vmkStat;
   vmk_uint32                       bitPos;
   vmk_IscsiTransCapabilitiesProperty  prop;

   vmkStat = vmk_IscsiTransportCreateCapabilitiesProperty(&prop);
   if (vmkStat != VMK_OK) {
      return vmkStat;
   }

   while ((bitPos = (vmk_uint32)ffs(bitMask)) != 0) {
      vmk_IscsiTransCapabilities cap=bitPos-1;

      vmkStat = vmk_IscsiTransportAddCapabilitiesProperty(prop, cap);
      if (vmkStat != VMK_OK) {
         break;
      }

      bitMask &= ~(1 << (bitPos - 1));
   }

   if (vmkStat != VMK_OK) {
      vmk_IscsiTransportDestroyCapabilitiesProperty(prop);
   } else {
      *outProp = prop;
   }

   return vmkStat;
}
/*
 * iscsi_register_transport
 * Description:  Register this Media Driver with the transport Mid-Layer.
 *               This is the first step required in enabling the Media
 *               Driver to function in the Open iSCSI framework.  The
 *               Mid-Layer will then query the driver for various
 *               information as well as signal when to start operation.
 * Arguments:    tptr - identifies this Media Driver
 *               size - size of the template
 * Returns:      The transport template
 * Side effects: XXXSEFD
 */
struct scsi_transport_template *
iscsi_register_transport(struct iscsi_transport *tptr)
{
   struct scsi_transport_template   *res = NULL;

   vmk_IscsiTransIscsiParamProperty    iscsiParamProperty = NULL;
   vmk_IscsiTransHostParamProperty     hostParamProperty = NULL;
   vmk_IscsiTransCapabilitiesProperty  capsProperty = NULL;
   vmk_IscsiTransTransport             *transTransport = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(tptr != NULL);

   do {

      transTransport = kmalloc(sizeof(vmk_IscsiTransTransport), GFP_KERNEL);
      if (transTransport == NULL) {
         break;
      }

      vmk_Memset(transTransport, 0, sizeof(*transTransport));

      if (tptr->iscsiParamProperty == NULL) {
         if (iscsilinux_TranslateIscsiParamBitMaskToProperty(
                  tptr->param_mask,
                  &iscsiParamProperty) != VMK_OK) {
            break;
         }
         transTransport->iscsiParamProperty = iscsiParamProperty;
      } else {
         if(tptr->iscsiParamProperty->propertyType !=
            VMK_ISCSI_TRANSPORT_PROPERTY_ISCSI_PARAM) {
            VMK_ASSERT(0);
            break;
         }
         transTransport->iscsiParamProperty = tptr->iscsiParamProperty;
      }

      if (tptr->hostParamProperty == NULL) {
         if (iscsilinux_TranslateHostParamBitMaskToProperty(
                  tptr->host_param_mask,
                  &hostParamProperty) != VMK_OK) {
            break;
         }
         transTransport->hostParamProperty = hostParamProperty;
      } else {
         if(tptr->hostParamProperty->propertyType !=
            VMK_ISCSI_TRANSPORT_PROPERTY_HOST_PARAM) {
            VMK_ASSERT(0);
            break;
         }
         transTransport->hostParamProperty = tptr->hostParamProperty;
      }

      if (tptr->capsProperty == NULL) {
         if (iscsilinux_TranslateCapabilityBitMaskToProperty(
                  tptr->caps,
                  &capsProperty) != VMK_OK) {
            break;
         }
         transTransport->capsProperty = capsProperty;
      } else {
         if(tptr->capsProperty->propertyType !=
            VMK_ISCSI_TRANSPORT_PROPERTY_CAPABILITIES) {
            VMK_ASSERT(0);
            break;
         }
         transTransport->capsProperty = tptr->capsProperty;
      }

      vmk_Strncpy(transTransport->name,
                  tptr->name,
                  VMK_ISCSI_MAX_TRANSPORT_NAME_SZ);

      vmk_Strncpy(transTransport->description,
                  tptr->description,
                  VMK_ISCSI_MAX_TRANSPORT_DESC_SZ);  

      /* check required functions for non-hw iscsi */
      if (!(tptr->caps & CAP_FW_DB)) {
         if (tptr->create_session_persistent &&
             tptr->destroy_session &&
             tptr->create_conn &&
             tptr->destroy_conn &&
             tptr->start_conn &&
             tptr->stop_conn &&
             tptr->bind_conn &&
             tptr->send_pdu) {
         } else {
            vmk_WarningMessage("Failed registering iscsi transport: \n"
               "Missing required function entry points, non-hw iscsi adapter.\n");
            break; 
         }
      }

      /* check requried functions for offload iscsi */
      if ((tptr->caps & CAP_KERNEL_POLL)) {
         if (tptr->ep_connect_extended && 
             tptr->ep_poll &&
             tptr->ep_disconnect) {
         } else {
            vmk_WarningMessage("Failed registering iscsi transport: \n"
               "Missing required function entry points, offload iscsi adapter.\n");
            break;
         }
      }

      transTransport->ddData                    = (void *)tptr;
      transTransport->revision                  = VMK_ISCSI_TRANSPORT_REVISION;


      transTransport->connDataSize              = tptr->conndata_size +
                                                  sizeof(struct iscsi_cls_conn);
      transTransport->sessionDataSize           = tptr->sessiondata_size +
                                                  sizeof(struct iscsi_cls_session);
      transTransport->maxLun                    = tptr->max_lun;
      transTransport->maxConn                   = tptr->max_conn;
      transTransport->maxCmdLen                 = tptr->max_cmd_len;

      transTransport->createSessionPersistent   = iscsilinux_create_session_persistent;
      transTransport->destroySession            = iscsilinux_destroy_session;
      transTransport->createConnection          = iscsilinux_create_connection;
      transTransport->bindConnection            = iscsilinux_bind_connection;
      transTransport->startConnection           = iscsilinux_start_connection;
      transTransport->stopConnection            = iscsilinux_stop_connection;
      transTransport->destroyConnection         = iscsilinux_destroy_connection;
      transTransport->getSessionParam           = iscsilinux_get_session_param;
      transTransport->setParam                  = iscsilinux_set_param;
      transTransport->getConnectionParam        = iscsilinux_get_connection_param;
      transTransport->getHostParam              = iscsilinux_get_host_param;
      transTransport->setHostParam              = iscsilinux_set_host_param;
      transTransport->sendPdu                   = iscsilinux_send_pdu;
      transTransport->getStats                  = iscsilinux_get_stats;
      transTransport->sessionRecoveryTimedout   = iscsilinux_session_recovery_timedout;
      transTransport->EPConnectExtended         = iscsilinux_ep_connect_extended;
      transTransport->EPPoll                    = iscsilinux_ep_poll;
      transTransport->EPDisconnect              = iscsilinux_ep_disconnect;
      transTransport->targetDiscover            = iscsilinux_target_discover;
      transTransport->getTransportLimits        = iscsilinux_get_transport_limits;

      if (vmk_IscsiTransportRegisterTransport(transTransport) != VMK_OK) {
         break;
      }

      res = kmalloc(sizeof(struct scsi_transport_template), GFP_KERNEL);
      if (res == NULL) {
         vmk_IscsiTransportUnregisterTransport(transTransport);
         break;
      }

      vmk_Memset(res, 0, sizeof(*res));

      res->user_scan = iscsi_user_scan;
      tptr->scsi_template = res;
      tptr->private = (void *)transTransport;
   } while (0);

   if (res == NULL) {
      if (transTransport) {
         kfree(transTransport);
      }

      if (capsProperty) {
         vmk_IscsiTransportDestroyCapabilitiesProperty(capsProperty);
      }

      if (hostParamProperty) {
         vmk_IscsiTransportDestroyHostParamProperty(hostParamProperty);
      }

      if (iscsiParamProperty) {
         vmk_IscsiTransportDestroyIscsiParamProperty(iscsiParamProperty);
      }
   }

   return res;   
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_register_transport);

/*
 * iscsi_unregister_transport
 * Description:  This function is called in the clean up path of the Media
 *               Driver when we are trying to unload it from the kernel.
 * Arguments:    tptr - The iSCSI transport template we previously registered 
 *                      using the iscsi_register_transport call.
 * Returns:      Always zero. I suppose it should return an ERROR if the
 *               driver is still in use.
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_unregister_transport(struct iscsi_transport *tptr)
{
   VMK_ReturnStatus  status = VMK_OK;
   vmk_IscsiTransTransport *transTransport;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(tptr != NULL);

   transTransport = (vmk_IscsiTransTransport *)tptr->private;

   if (status == VMK_OK &&
       tptr->iscsiParamProperty == NULL &&
       transTransport->iscsiParamProperty) {
      status = vmk_IscsiTransportDestroyIscsiParamProperty(
                  transTransport->iscsiParamProperty);
      if (status == VMK_OK) {
         transTransport->iscsiParamProperty = NULL;
      }
   }

   if (status == VMK_OK &&
       tptr->hostParamProperty == NULL &&
       transTransport->hostParamProperty) {
      status = vmk_IscsiTransportDestroyHostParamProperty(
                  transTransport->hostParamProperty);
      if (status == VMK_OK) {
         transTransport->hostParamProperty = NULL;
      }
   }

   if (status == VMK_OK &&
       tptr->capsProperty == NULL &&
       transTransport->capsProperty) {
      status = vmk_IscsiTransportDestroyCapabilitiesProperty(
                  transTransport->capsProperty);
      if (status == VMK_OK) {
         transTransport->capsProperty = NULL;
      }
   }

   if (status == VMK_OK) {
      status = vmk_IscsiTransportUnregisterTransport(transTransport);
      if (status == VMK_OK) {
         kfree(tptr->scsi_template);
         tptr->scsi_template = NULL;
         kfree(transTransport);
         tptr->private = NULL;
      }
   }

   if (status == VMK_OK) {
      return 0;
   } else {
      return 1;
   }
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_unregister_transport);

/*
 * iscsi_register_host
 * Description:  Register a management adapter on the hptr specified. Management
 *               adapters are registered as a member of vmk_ScsiAdapter so this 
 *               call would actualy just retreive the underlying vmk_ScsiAdapter
 *               from the struct Scsi_Host and call the vmk version of the API
 * Arguments:    hptr - Pointer to the SCSI host to be registered.
 *               tptr - The iSCSI transport template we previously registered 
 *                      using the iscsi_register_transport call.
 * Returns:      Status Code
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_register_host(struct Scsi_Host *hptr, struct iscsi_transport *tptr)
{
   vmk_int32 res=0;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(hptr != NULL);
   VMK_ASSERT(tptr != NULL);
   VMK_ASSERT(hptr->adapter != NULL);
   VMK_ASSERT(VMK_ADAPTER(hptr) != NULL);

   /* set transport type on behalf of hwiscsi */
   hptr->xportFlags |= VMKLNX_SCSI_TRANSPORT_TYPE_ISCSI;

   res = vmk_IscsiTransportRegisterAdapter(VMK_ADAPTER(hptr),
                                    (vmk_IscsiTransTransport *)tptr->private);
   return res;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_register_host);

/*
 * iscsi_unregister_host
 * Description:  Unregister a management adapter on the hptr specified. 
 *               Management adapters are registered as a member of 
 *               vmk_ScsiAdapter so this call would actualy just retreive the 
 *               underlying vmk_ScsiAdapter from the struct Scsi_Host and call 
 *               the vmk version of the API.
 * Arguments:    hptr - Pointer to the SCSI host to be unregistered.
 *               tptr - The iSCSI transport template we previously registered 
 *                      using the iscsi_register_transport call.
 * Returns:      Status Code
 * Side effects: XXXSEFD
 */
vmk_int32
iscsi_unregister_host(struct Scsi_Host *hptr, struct iscsi_transport *tptr)
{
   /*
    * Release a management adapter on the hptr specified. Management adapters
    * are registered as a member of vmk_ScsiAdapter so this call would actualy
    * just retreive the underlying vmk_ScsiAdapter from the struct Scsi_Host and
    * call the vmk version of the API
    */
   vmk_int32 res=0;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(hptr != NULL);
   VMK_ASSERT(tptr != NULL);
   VMK_ASSERT(hptr->adapter != NULL);
   VMK_ASSERT(VMK_ADAPTER(hptr) != NULL);
   res = vmk_IscsiTransportUnregisterAdapter(VMK_ADAPTER(hptr));
   return res;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_unregister_host);

/*
 * iscsi_sdevice_to_session
 * Description:  return the session associated with a scsi device
 * Arguments:    sdev - pointer to the scsi_device
 * Returns:      pointer to the session
 * Side effects: none
 */
struct iscsi_cls_session *
iscsi_sdevice_to_session(struct scsi_device *sdev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sdev != NULL);
   VMK_ASSERT(sdev->host != NULL);

   return iscsi_lookup_session(sdev->host->host_no,
                            sdev->channel,
                            sdev->id);
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_sdevice_to_session);

struct iscsi_cls_session *
iscsi_lookup_session(int host_no, int channel, int id)
{
   struct iscsi_cls_session   *sptr = NULL;
   vmk_IscsiTransSession      *transSession = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (vmk_IscsiTransportLookupSession(host_no, channel, id, &transSession) == VMK_OK) {
      sptr = (struct iscsi_cls_session *)transSession->ddData;
   }

   return sptr;
}
VMK_MODULE_EXPORT_SYMBOL(iscsi_lookup_session);

#define OK 0
#define DEFINE_VMK_ERR(_err, _str, _lerrno) _lerrno,
#define DEFINE_VMK_ERR_AT(_err, _str, _val, _lerrno) _lerrno,
int returnStatusErrnos[] = {
   VMK_ERROR_CODES
};
#undef DEFINE_VMK_ERR
#undef DEFINE_VMK_ERR_AT

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof (a) / sizeof *(a))
#endif

/*
 *----------------------------------------------------------------------
 *
 * iscsilinux_VmkReturnStatusToErrno --
 *
 *    Returns linux -errno corresponding to given "status".  Results
 *    undefined for invalid status.
 *
 * Results:
 *    Returns linux errno corresponding to given "status".
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static vmk_int32
iscsilinux_VmkReturnStatusToErrno(VMK_ReturnStatus status)
{
   if (!(VMK_GENERIC_LINUX_ERROR > VMK_FAILURE)) {
      return -EINVAL;
   }

   if (status >= VMK_GENERIC_LINUX_ERROR) {
      return (int)-(status - VMK_GENERIC_LINUX_ERROR);
   }

   if (status == VMK_OK) {
      return 0;
   }

   if (status >= VMK_FAILURE) {
      unsigned int index = (status - VMK_FAILURE) + 1;
      if (index < ARRAYSIZE(returnStatusErrnos)) {
         return -returnStatusErrnos[index];
      }
   }

   return -EINVAL;
}

#define   LINUX_EPERM	      (1)
#define   LINUX_ENOENT	      (2)
#define   LINUX_ESRCH	      (3)
#define   LINUX_EINTR	      (4)
#define   LINUX_EIO           (5)
#define   LINUX_ENXIO	      (6)
#define   LINUX_EAGAIN	      (11)
#define   LINUX_ENOMEM	      (12)
#define   LINUX_EACCES	      (13)
#define   LINUX_EFAULT	      (14)
#define   LINUX_EBUSY	      (16)
#define   LINUX_EEXIST	      (17)
#define   LINUX_EXDEV	      (18)
#define   LINUX_ENODEV	      (19)
#define   LINUX_EINVAL	      (22)
#define   LINUX_ENOSPC	      (28)
#define   LINUX_EPIPE	      (32)
#define   LINUX_ERANGE	      (34)
#define   LINUX_ENOSYS	      (38)
#define   LINUX_EOPNOTSUPP	   (95)
#define   LINUX_ENETDOWN	   (100)
#define   LINUX_ENETUNREACH	(101)
#define   LINUX_ENETRESET	   (102)
#define   LINUX_ECONNRESET	   (104)
#define   LINUX_ENOBUFS	      (105)
#define   LINUX_ENOTCONN	   (107)
#define   LINUX_ESHUTDOWN	   (108)
#define   LINUX_ETIMEDOUT	   (110)
#define   LINUX_ECONNREFUSED	(111)
#define   LINUX_EHOSTDOWN	   (112)
#define   LINUX_EHOSTUNREACH	(113)
#define   LINUX_EALREADY	   (114)
#define   LINUX_EINPROGRESS	(115)

static struct {
   vmk_int32 errno;
   VMK_ReturnStatus vmkStatus;
} linuxErrnoToVmkStatusMap[] = {
        {0,                   VMK_OK},
        {LINUX_EPERM,         VMK_NO_PERMISSION},
        {LINUX_ENOENT,        VMK_NOT_FOUND},
        {LINUX_EINTR,         VMK_WAIT_INTERRUPTED},
        {LINUX_EIO,           VMK_IO_ERROR},
        {LINUX_EAGAIN,        VMK_STATUS_PENDING},
        {LINUX_ENOMEM,        VMK_NO_MEMORY},
        {LINUX_EACCES,        VMK_NO_ACCESS},
        {LINUX_EFAULT,        VMK_INVALID_ADDRESS},
        {LINUX_EBUSY,         VMK_BUSY},
        {LINUX_EEXIST,        VMK_EXISTS},
        {LINUX_ENODEV,        VMK_INVALID_TARGET},
        {LINUX_EINVAL,        VMK_BAD_PARAM},
        {LINUX_ENOSPC,        VMK_NO_SPACE},
        {LINUX_EPIPE,         VMK_BROKEN_PIPE},
        {LINUX_ERANGE,        VMK_RESULT_TOO_LARGE},
        {LINUX_ENOSYS,        VMK_NOT_SUPPORTED},
        {LINUX_EOPNOTSUPP,    VMK_EOPNOTSUPP},
        {LINUX_ENETDOWN,      VMK_ENETDOWN},
        {LINUX_ENETUNREACH,   VMK_ENETUNREACH},
        {LINUX_ENETRESET,     VMK_ENETRESET},
        {LINUX_ECONNRESET,    VMK_ECONNRESET},
        {LINUX_ENOBUFS,       VMK_NO_BUFFERSPACE},
        {LINUX_ENOTCONN,      VMK_IS_DISCONNECTED},
        {LINUX_ESHUTDOWN,     VMK_ESHUTDOWN},
        {LINUX_ETIMEDOUT,     VMK_TIMEOUT},
        {LINUX_ECONNREFUSED,  VMK_ECONNREFUSED},
        {LINUX_EHOSTDOWN,     VMK_EHOSTDOWN},
        {LINUX_EHOSTUNREACH,  VMK_EHOSTUNREACH},
        {LINUX_EALREADY,      VMK_EALREADY},
        {LINUX_EINPROGRESS,   VMK_WORK_RUNNING}
};
#define LINUX_ERRNO_TO_VMK_STATUS_MAP_TABLE_SIZE \
   (sizeof(linuxErrnoToVmkStatusMap)/sizeof(linuxErrnoToVmkStatusMap[0]))

/*
 *----------------------------------------------------------------------
 *
 * iscsilinux_ErrnoToVmkReturnStatus --
 *
 *    Returns vmkernel error code corresponding to given "linux errno".
 *
 * Results:
 *    Returns VMK_ReturnStatus corresponding to given "errno".
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
iscsilinux_ErrnoToVmkReturnStatus(vmk_int32 errno)
{
   vmk_int32 index = 0;

   while (index < LINUX_ERRNO_TO_VMK_STATUS_MAP_TABLE_SIZE) {
      if (linuxErrnoToVmkStatusMap[index].errno == errno) {
         return linuxErrnoToVmkStatusMap[index].vmkStatus;
      }
      index++;
   }

   ISCSILINUX_LOG(1, "Returning default error, VMK_ERROR, for %d\n", errno);

   return VMK_FAILURE;
}

/*
 ***********************************************************************
 * iscsilinux_init --                                                 */ /**
 *
 *  iscsilinux
 *
 * This function is called automatically when the module is loaded.
 *
 * \return Returns 0 if the module initialization succeeded.
 *
 ***********************************************************************
 */
int __init
iscsilinux_init(void)
{
   VMK_ReturnStatus vmkStat;

   /* Register module */
   vmkStat = vmk_ModuleRegister(&iscsilinuxModuleID, VMKAPI_REVISION);

   if (vmkStat != VMK_OK) {
      vmk_WarningMessage("vmk_ModuleRegister failed: %s",
                         vmk_StatusToString(vmkStat));
      vmk_WarningMessage("Could not load %s module",
                         ISCSILINUX_MODULE_NAME);
      return 1;
   }

   return 0;
}

/*
 ***********************************************************************
 * cleanup_module --                                              */ /**
 *
 *  iscsilinux
 *
 * This function cleans up all resources allocated to the modules.
 *
 * It is invoked when a user tries to unload the modules and the
 * module's reference count is nul.
 *
 ***********************************************************************
 */
static void __exit
iscsilinux_exit(void)
{
   /* Unregister module */
   vmk_ModuleUnregister(iscsilinuxModuleID);
}

module_init(iscsilinux_init);
module_exit(iscsilinux_exit);
