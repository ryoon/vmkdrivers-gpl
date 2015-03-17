/* ****************************************************************
 * Portions Copyright 2009-2014 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <vmklinux_92/vmklinux_cna.h>

#include "linux_stubs.h"
#include "linux_cna.h"
#include "linux_scsi.h"
#include "linux_net.h"
#include "linux_pci.h"

#define VMKLNX_LOG_HANDLE LinCNA
#include "vmklinux_log.h"

static struct vmklnx_fcoe_template *default_fcoe_owner = NULL;
static vmk_ModuleID cnaModID = VMK_INVALID_MODULE_ID;
static vmk_CharDev cnaCharHandle = VMK_INVALID_CHARDEV;
static struct mutex cna_ioctl_mutex;

/*
 * FCoE Transport Clients
 */
struct list_head        linuxFCoEClientList;
vmk_SpinlockIRQ         linuxFCoEClientLock;

/*
 * List of converged adapters
 */
vmk_uint32		totalCNA = 0;
struct list_head        linuxCNAAdapterList;
vmk_SpinlockIRQ         linuxCNAAdapterLock;

static VMK_ReturnStatus CNACharUnregCleanup(vmk_AddrCookie devicePrivate);
static VMK_ReturnStatus CNACharOpsOpen(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus CNACharOpsClose(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus CNACharOpsIoctl(vmk_CharDevFdAttr *attr,
                                        unsigned int cmd,
                                        vmk_uintptr_t userData,
                                        vmk_IoctlCallerSize callerSize,
                                        vmk_int32 *result);

static void CNAListVmnics(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoECreateController(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEGetController(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEGetCNAInfo(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEDiscoverFCF(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEInitiateFlogout(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEDestroyController(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEDCBWakeup(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEDCBHWAssist(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoESetCnaVN2VNMode(struct fcoe_ioctl_pkt *fcoe_ioctl);
static void FCoEUnsetCnaVN2VNMode(struct fcoe_ioctl_pkt *fcoe_ioctl);

static VMK_ReturnStatus CNACheckQueues(struct net_device *netdev);
static VMK_ReturnStatus CNACheckQueueFeatures(struct net_device *netdev);
static VMK_ReturnStatus CNACheckQueueCount(struct net_device *netdev);
static VMK_ReturnStatus CNACheckFilterCount(struct net_device *netdev);
static VMK_ReturnStatus CNASetProperties(struct vmklnx_cna *cna,
                                         struct fcoe_cntlr *fcoeCntlr);
static VMK_ReturnStatus CNAInitQueues(struct vmklnx_cna *cna,
                                      vmk_uint8 *mac_address, int max_queues);
static VMK_ReturnStatus CNAAllocTXRXQueue(struct vmklnx_cna *cna, int count);
static VMK_ReturnStatus CNAFreeTXRXQueue(struct vmklnx_cna *cna,
                                         vmk_Bool is_reset);
static VMK_ReturnStatus CNASetRxFilter(struct vmklnx_cna *cna,
                                       unsigned char *mac_addr);
static VMK_ReturnStatus CNAClearRxFilter(struct vmklnx_cna *cna,
                                         vmk_Bool is_reset);
static VMK_ReturnStatus CNAClearProperties(struct vmklnx_cna *cna,
                                           vmk_Bool is_reset);
static VMK_ReturnStatus CNAGetFreeRxIndices(struct vmklnx_cna *cna, int *qid,
                                            int *fid);
static VMK_ReturnStatus CNARemoveRxFilter(struct vmklnx_cna *cna,
                                          int qindex, int findex);

static struct vmklnx_cna *__cna_lookup_by_name(char * vmnic_name);
static struct vmklnx_fcoe_template *fcoe_lookup_by_pcitbl(struct pci_dev *pdev);
static int cna_netdev_process_rx(struct sk_buff *skb, void *handle);

static vmk_CharDevOps cnaCharOps = {
   CNACharOpsOpen,
   CNACharOpsClose,
   CNACharOpsIoctl,
   NULL,
   NULL
};


/*
 *----------------------------------------------------------------------
 *
 * LinuxCNA_Init
 *
 *      Main entry point from vmklinux init
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      none.
 *----------------------------------------------------------------------
 */
void
LinuxCNA_Init(void)
{
   VMK_ReturnStatus status;

   status = vmk_SPCreateIRQ_LEGACY(&linuxFCoEClientLock, vmklinuxModID,
                            "FcoeClientListLck", NULL, VMK_SP_RANK_IRQ_LEAF_LEGACY);

   if (status != VMK_OK) {
      vmk_AlertMessage("Failed to initialize FCoE Template Spinlock");
      VMK_ASSERT(status == VMK_OK);
   }

   status = vmk_SPCreateIRQ_LEGACY(&linuxCNAAdapterLock, vmklinuxModID,
                            "FcoeCNAListLck", NULL, VMK_SP_RANK_IRQ_LEAF_LEGACY);

   if (status != VMK_OK) {
      vmk_AlertMessage("Failed to initialize CNA List Lock");
      VMK_ASSERT(status == VMK_OK);
   }

   cnaModID = vmklinuxModID;

   mutex_init(&cna_ioctl_mutex);

   INIT_LIST_HEAD(&linuxFCoEClientList);
   INIT_LIST_HEAD(&linuxCNAAdapterList);

   VMKLNX_CREATE_LOG();

   if (vmk_CharDevRegister(vmklinuxModID, CNA_NAME, 
                           &cnaCharOps, CNACharUnregCleanup, 0, 
                           &cnaCharHandle) != VMK_OK) {
      VMKLNX_WARN("failed to register cna char dev");
   }

   LinuxDCB_Init();

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxCNA_Cleanup
 *
 *	Clean up entry point
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      none.
 *----------------------------------------------------------------------
 */
void
LinuxCNA_Cleanup(void)
{
   LinuxDCB_Cleanup();

   vmk_CharDevUnregister(cnaCharHandle);

   vmk_SPDestroyIRQ(&linuxFCoEClientLock);
   vmk_SPDestroyIRQ(&linuxCNAAdapterLock);

   mutex_destroy(&cna_ioctl_mutex);

   VMKLNX_DESTROY_LOG();
}

/*
 *----------------------------------------------------------------------
 *
 * CNACharUnregCleanup
 *
 *	Clean up any device-private data registered to the CNA char
 *      device.  (Currently, the device does not use device-private
 *      data).
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACharUnregCleanup(vmk_AddrCookie devicePrivate)
{
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACharOpsOpen
 *
 *	Open the char dev for management
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Takes reference on vmklinux
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACharOpsOpen(vmk_CharDevFdAttr *attr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmk_WorldAssertIsSafeToBlock();

   return vmk_ModuleIncUseCount(cnaModID) == VMK_OK ? 0 : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACharOpsClose
 *
 *	Closes the char dev for management
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Removes the reference taken on vmklinux
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACharOpsClose(vmk_CharDevFdAttr *attr)
{
   VMK_ReturnStatus vmkRet = vmk_ModuleDecUseCount(cnaModID);
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmkRet == VMK_OK);
   return vmkRet;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACharOpsIoctl
 *
 *      Handles communication with UW daemon, which parses for
 *      packet content.
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      none.
 *
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACharOpsIoctl(vmk_CharDevFdAttr *attr,
                unsigned int cmd,
                vmk_uintptr_t userData,
                vmk_IoctlCallerSize callerSize,
                vmk_int32 *result)
{
   VMK_ReturnStatus status = VMK_OK;
   struct fcoe_ioctl_pkt *fcoe_ioctl;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmk_WorldAssertIsSafeToBlock();

   fcoe_ioctl = (struct fcoe_ioctl_pkt *)kzalloc(sizeof(*fcoe_ioctl),
                                                 GFP_KERNEL);
   if (!fcoe_ioctl) {
      return VMK_NO_MEMORY;
   }

   VMKLNX_DEBUG(2, "got ioctl 0x%x", cmd);

   status = vmk_CopyFromUser((vmk_VA)fcoe_ioctl,
                             (vmk_VA)userData,
                             sizeof (*fcoe_ioctl));

   if (status != VMK_OK) {
      VMKLNX_WARN("Failed to copy fcoe ioctl msg");
      goto fcoe_ioctl_err;
   }

   if (cmd != fcoe_ioctl->cmd) {
      VMKLNX_WARN("received invalid ioctl 0x%x", fcoe_ioctl->cmd);
      goto fcoe_ioctl_err;
   }

   /*
    * Process DCB requests
    */
   if (cmd & CNA_IOCTL_DCB_MASK) {
      CNAProcessDCBRequests(fcoe_ioctl);
      goto processed_cna_ioctl;
   }

   /*
    * Mgmt code will not be reentrant
    */
   mutex_lock(&cna_ioctl_mutex);

   switch (fcoe_ioctl->cmd) {
      case FCOE_IOCTL_GET_CNA_VMNICS:
         CNAListVmnics(fcoe_ioctl);
         break;
      case FCOE_IOCTL_CREATE_FC_CONTROLLER:
         FCoECreateController(fcoe_ioctl);
         break;
      case FCOE_IOCTL_GET_CNA_INFO:
         FCoEGetCNAInfo(fcoe_ioctl);
         break;
      case FCOE_IOCTL_GET_FC_CONTROLLER:
         FCoEGetController(fcoe_ioctl);
         break;
      case FCOE_IOCTL_DISCOVER_FCF:
         FCoEDiscoverFCF(fcoe_ioctl);
         break;
      case FCOE_IOCTL_FCF_LOGOUT:
         FCoEInitiateFlogout(fcoe_ioctl);
         break;
      case FCOE_IOCTL_DESTROY_FC_CONTROLLER:
         FCoEDestroyController(fcoe_ioctl);
         break;
      case FCOE_IOCTL_DCB_WAKEUP:
         FCoEDCBWakeup(fcoe_ioctl);
         break;
      case FCOE_IOCTL_DCB_HWASSIST:
         FCoEDCBHWAssist(fcoe_ioctl);
         break;
      case FCOE_IOCTL_SET_CNA_VN2VN_MODE:
         FCoESetCnaVN2VNMode(fcoe_ioctl);
         break;
      case FCOE_IOCTL_UNSET_CNA_VN2VN_MODE:
         FCoEUnsetCnaVN2VNMode(fcoe_ioctl);
         break;
      default:
         VMKLNX_DEBUG(2, "Invalid ioctl 0x%x", fcoe_ioctl->cmd);
         status = VMK_BAD_PARAM;
         VMK_ASSERT(0);
   }

   mutex_unlock(&cna_ioctl_mutex);

   if (status == VMK_BAD_PARAM) {
      goto fcoe_ioctl_err;
   }

processed_cna_ioctl:
   /*
    * This is a 2 way street. Each of the underlying functions
    * provide the response in the fcoe_ioctl structure. We are
    * copying the response here
    */
   status = vmk_CopyToUser((vmk_VA)userData,
                           (vmk_VA)fcoe_ioctl,
                           sizeof (*fcoe_ioctl));

   if (status != VMK_OK) {
      VMKLNX_WARN("Failed to copy fcoe ioctl response");
      VMK_ASSERT(FALSE);
   }

fcoe_ioctl_err:
   kfree(fcoe_ioctl);
   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * CNAListVmnics
 *
 *	List all CNA based vmnics
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static void
CNAListVmnics(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   unsigned vmkFlag;
   struct vmklnx_cna *cna;
   int count = 0;
   struct fcoe_cna_list *fcoeCna = /* Response data */
      (struct fcoe_cna_list *) fcoe_ioctl->data;

   vmkFlag = vmk_SPLockIRQ(&linuxCNAAdapterLock);
   list_for_each_entry(cna, &linuxCNAAdapterList, entry) {
      strncpy(fcoeCna->vmnic_name[count], cna->netdev->name, CNA_DRIVER_NAME);
      ++count;
   }
   vmk_SPUnlockIRQ(&linuxCNAAdapterLock, vmkFlag);

   fcoeCna->cna_count = count;

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_GET_CNA_VMNICS;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoECreateController
 *
 *	User interface to create an FCoE controller
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Resources are allocated for FCoE controller
 *----------------------------------------------------------------------
 */
static void
FCoECreateController(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct vmklnx_fcoe_contlr *contlr;
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_template *fcoe = NULL;
   unsigned vmkFlag;
   struct fcoe_cntlr *fcoeCntlr = (struct fcoe_cntlr *) fcoe_ioctl->data;
   VMK_ReturnStatus vmkRet = VMK_OK;

   VMK_ASSERT(fcoeCntlr->vmnic_name);

   /*
    * Allocate memory for fcoe controller
    */
   contlr = kzalloc(sizeof(*contlr), GFP_KERNEL);
   if (!contlr) {
      VMKLNX_DEBUG(2, "Failed to alloc mem for fcoe controller");
      goto fcoe_return_failure;
   }

   cna = vmklnx_cna_lookup_by_name(fcoeCntlr->vmnic_name);

   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA struct for fcoe controller");
      goto fcoe_free_contlr;
   }

   /*
    * There can only be one FCoE Controller per CNA instance
    * If this is already registerred, then fail the request
    */
   if (cna->handle) {
      VMKLNX_DEBUG(2, "Trying to re-register controller on a given CNA");
      goto fcoe_free_contlr;
   }

   /*
    * Now try to lookup any FCoE template that has registerred
    * for the matching pci id.
    */
   vmkFlag = vmk_SPLockIRQ(&linuxFCoEClientLock);
   fcoe = fcoe_lookup_by_pcitbl(cna->netdev->pdev);

   if ((!fcoe) && (default_fcoe_owner)) {
      fcoe = default_fcoe_owner;
   }
   vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);

   if (!fcoe) {
      VMKLNX_DEBUG(2, "FCoE template lookup failed failed for %s",
                   fcoeCntlr->vmnic_name);
      goto fcoe_free_contlr;
   }

   vmkRet = vmk_ModuleIncUseCount(cna->netdev->module_id);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Failed to increment reference on net driver: %s",
                  vmk_StatusToString(vmkRet));
      goto fcoe_free_contlr;
   }

   vmkRet = vmk_ModuleIncUseCount(fcoe->module_id);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Failed to increment reference on fcoe transport: %s",
                  vmk_StatusToString(vmkRet));
      goto fcoe_dec_cna_count;
   }

   /*
    * If we are here, it means everything went through smoothly
    * Populate FCoE Controller fields and send up the FCoE Controller
    * handle
    */
   contlr->cna = cna;
   contlr->fcoe = fcoe;

   /*
    * Check to see if there are queues available for the data transfer
    */
   if (CNACheckQueues(cna->netdev) != VMK_OK) {
      VMKLNX_ALERT("Failed to validate queue");
      goto fcoe_dec_fcoe_count;
   }

   if (CNASetProperties(cna, fcoeCntlr) != VMK_OK) {
      VMKLNX_DEBUG(2, "CNA Set properties failed for %s",
                   fcoeCntlr->vmnic_name);
      goto fcoe_dec_fcoe_count;
   }

   /*
    * Set the receive function now
    */
   cna->handle = (void *) contlr;
   cna->rx_handler = cna_netdev_process_rx;
   atomic_set(&contlr->vnports, 0);

   /*
    * Prepare an ioctl response now
    */
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_CREATE_FC_CONTROLLER;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;

fcoe_dec_fcoe_count:
   vmkRet = vmk_ModuleDecUseCount(fcoe->module_id);
   VMK_ASSERT(vmkRet == VMK_OK);
fcoe_dec_cna_count:
   vmkRet = vmk_ModuleDecUseCount(cna->netdev->module_id);
   VMK_ASSERT(vmkRet == VMK_OK);
fcoe_free_contlr:
   kfree(contlr);

fcoe_return_failure:
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_CREATE_FC_CONTROLLER;
   fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEGetCNAInfo
 *
 *    User interface to get CNA information (default settings and
 *    settability of FCoE parameters).
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Populates a fcoe_cna_info structure inside the given fcoe_ioctl
 *      packet upon success.
 *----------------------------------------------------------------------
 */
static void
FCoEGetCNAInfo(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct fcoe_cna_info *info = (struct fcoe_cna_info *) fcoe_ioctl->data;

   VMK_ASSERT(info->vmnic_name);

   struct vmklnx_cna *cna;
   cna = vmklnx_cna_lookup_by_name(info->vmnic_name);
   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA struct for fcoe info");
      fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_GET_CNA_INFO;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   vmk_int32 defaultVlan = 0;

   info->default_priority = VMK_VLAN_PRIORITY_CRITICAL_APPS; /* Priority 3 */
   info->default_vlan[defaultVlan / 32] |= (0x00000001 << (defaultVlan % 32));
   memcpy(info->default_mac_address, cna->netdev->dev_addr, 6);

   info->priority_settable = VMK_FALSE;
   info->vlan_settable = VMK_FALSE;
   info->mac_address_settable = VMK_FALSE;

   // In case of VN2VN mode let the user select VLAN ID
   if (cna->flags & CNA_FCOE_VN2VN) {
      info->vlan_settable = VMK_TRUE;
   }

   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;

   // Fill out the response fields
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_GET_CNA_INFO;
   fcoe_ioctl->cmd_len = sizeof(*info);

   VMK_ASSERT(fcoe_ioctl->cmd_len < MAX_FCOE_IOCTL_CMD_SIZE);

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEGetController
 *
 *    User interface to get the run-time FCoE Controller for a given
 *    CNA.  The input payload is the vmnic_name field of an fcoe_cntlr
 *    struct.  Upon success, that struct's other fields will be
 *    populated with current values.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Populates a fcoe_cntlr structure inside the given fcoe_ioctl
 *      packet upon success; the FCOE_IOCTL_NO_CONTROLLER bit will be
 *      set in the returned cmd value if there is no FCoE Controller
 *      to be found.
 *----------------------------------------------------------------------
 */
static void
FCoEGetController(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct fcoe_cntlr *out = (struct fcoe_cntlr *) fcoe_ioctl->data;

   struct vmklnx_cna *cna;
   struct fcoe_cntlr *contlr;

   VMK_ASSERT(out->vmnic_name);
   cna = vmklnx_cna_lookup_by_name(out->vmnic_name);

   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA struct for fcoe controller");
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   if (!cna->handle) {
      fcoe_ioctl->cmd =
         FCOE_IOCTL_NO_CONTROLLER | FCOE_IOCTL_GET_FC_CONTROLLER;
   } else {
      vmk_uint32 vlan = cna->fcoe_vlan_id;
      out->priority = cna->fcoe_app_prio;
      out->vlan[vlan / 32] |= (0x00000001 << (vlan % 32));
      memcpy(out->mac_address, cna->netdev->dev_addr, 6);

      fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_GET_FC_CONTROLLER;
      fcoe_ioctl->cmd_len = sizeof(*contlr);
   }

   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   VMK_ASSERT(fcoe_ioctl->cmd_len < MAX_FCOE_IOCTL_CMD_SIZE);

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEGetVmklnxContlr
 *
 *    Lookup a vmklnx_fcoe_contlr by the given vmnic_name.
 *
 * Results:
 *    Pointer to vmklnx_fcoe_contlr upon success; NULL if none found.
 *
 * Side Effects:
 *    None.
 *----------------------------------------------------------------------
 */
static struct vmklnx_fcoe_contlr *
FCoEGetVmklnxContlr(char *vmnic_name)
{
   struct vmklnx_fcoe_contlr *result = NULL;
   struct vmklnx_cna *cna;

   VMK_ASSERT(vmnic_name);
   cna = vmklnx_cna_lookup_by_name(vmnic_name);

   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA struct for fcoe controller");
   } else if (!cna->handle) {
      VMKLNX_DEBUG(2, "No FCOE controller handle for CNA \"%s\"", vmnic_name);
   } else {
      result = cna->handle;
      VMKLNX_DEBUG(2, "Returning FCOE controller handle 0x%p", cna->handle);
   }

   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEDCBWakeup
 *
 *    Wakeup FCoE interface
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    None.
 *----------------------------------------------------------------------
 */
static void
FCoEDCBWakeup(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   int status;
   struct vmklnx_fcoe_contlr *contlr;
   struct fcoe_cntlr *ioctlContlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   contlr = FCoEGetVmklnxContlr(ioctlContlr->vmnic_name);
   if (!contlr) {
      fcoe_ioctl->cmd = FCOE_IOCTL_NO_CONTROLLER | FCOE_IOCTL_DCB_WAKEUP;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   VMKLNX_DEBUG(2, "FCoEDCBWakeup for %s", contlr->fcoe->name);

   VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                      status,
                      contlr->fcoe->fcoe_wakeup,
                      contlr->cna->netdev);
   if (status) {
      VMKLNX_DEBUG(0, "fcoe wakeup failed for %s status (%d)",
                   contlr->fcoe->name, status);
      fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DCB_WAKEUP;
      if (status == -EAGAIN) {
         fcoe_ioctl->status = FCOE_IOCTL_WAKEUP_LINK_DOWN;
      } else {
         fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      }

      return;
   }

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DCB_WAKEUP;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * FCoEDCBHWAssist
 *
 *  Checked if the DCB negotiation is done in
 *  the adapter firmware.
 *
 * Results:
 *  VMK_TRUE if DCB negotiation is done by the adapter firmware
 *  VMK_FALSE otherwise
 *
 * Side Effects:
 *    Updates the dcb_hw_assist filed in dcb_mode
 *----------------------------------------------------------------------
 */
static void
FCoEDCBHWAssist(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct fcoe_dcb_mode *dcb_mode = (struct fcoe_dcb_mode *) fcoe_ioctl->data;

   dcb_mode->dcb_hw_assist = vmklnx_cna_is_dcb_hw_assist(dcb_mode->vmnic_name);

   VMKLNX_DEBUG(2, "%s: dcb hw assist %d",
                dcb_mode->vmnic_name,
                dcb_mode->dcb_hw_assist);

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DCB_HWASSIST;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * FCoESetCnaVN2VNMode
 *
 *	User interface to set VN2VN mode on NetDevice so FCoE will be
 *	started in FIP_MODE_VN2VN.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      VN2VN flag is enabled on the net_device
 *----------------------------------------------------------------------
 */
static void
FCoESetCnaVN2VNMode(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct vmklnx_cna *cna;
   struct net_device *netdev;
   struct fcoe_cntlr *fcoeCntlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   cna = vmklnx_cna_lookup_by_name(fcoeCntlr->vmnic_name);

   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA by name for fcoe ");
      fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_SET_CNA_VN2VN_MODE;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   VMKLNX_DEBUG(1, "Setting vn2vn mode on: %s", fcoeCntlr->vmnic_name);

   netdev = cna->netdev;
   cna->flags |= CNA_FCOE_VN2VN;
   netdev->features |= NETIF_F_CNA_VN2VN;

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_SET_CNA_VN2VN_MODE;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * FCoEUnsetCnaVN2VNMode
 *
 *	User interface to re-set VN2VN mode on NetDevice so FCoE will
 *	not be started in FIP_MODE_VN2VN.
 *
 *	Requires a reboot to make this change effective.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      VN2VN flag is removed on the net_device
 *----------------------------------------------------------------------
 */
static void
FCoEUnsetCnaVN2VNMode(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct vmklnx_cna *cna;
   struct net_device *netdev;
   struct fcoe_cntlr *fcoeCntlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   cna = vmklnx_cna_lookup_by_name(fcoeCntlr->vmnic_name);

   if (!cna) {
      VMKLNX_DEBUG(2, "Failed to lookup CNA by name for fcoe ");
      fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_UNSET_CNA_VN2VN_MODE;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   VMKLNX_DEBUG(1, "Unsetting vn2vn mode on: %s", fcoeCntlr->vmnic_name);

   netdev = cna->netdev;
   cna->flags &= ~CNA_FCOE_VN2VN;
   netdev->features &= ~NETIF_F_CNA_VN2VN;

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_UNSET_CNA_VN2VN_MODE;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * FCoEDiscoverFCF
 *
 *	User interface to trigger FCOE discovery
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      VNPorts are registerred by this action
 *----------------------------------------------------------------------
 */
static void
FCoEDiscoverFCF(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   int status;
   struct vmklnx_fcoe_contlr *contlr;
   struct fcoe_cntlr *ioctlContlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   contlr = FCoEGetVmklnxContlr(ioctlContlr->vmnic_name);
   if (!contlr) {
      fcoe_ioctl->cmd = FCOE_IOCTL_NO_CONTROLLER | FCOE_IOCTL_DISCOVER_FCF;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                      status,
                      contlr->fcoe->fcoe_create,
                      contlr->cna->netdev);
   if (status) {
      VMKLNX_DEBUG(2, "fcoe discover fcf failed for %s status %d",
                      contlr->fcoe->name, status);
      goto fail_discover_interface;
   }

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DISCOVER_FCF;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;

fail_discover_interface:
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DISCOVER_FCF;
   if (status == -EEXIST) {
      fcoe_ioctl->status = FCOE_IOCTL_ERROR_DEVICE_EXIST;
   } else {
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
   }

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEInitiateFlogout
 *
 *	This initiates logout from a given FCF
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static void
FCoEInitiateFlogout(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   int status;
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_contlr *contlr;
   struct fcoe_cntlr *ioctlContlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   contlr = FCoEGetVmklnxContlr(ioctlContlr->vmnic_name);
   if (!contlr) {
      fcoe_ioctl->cmd = FCOE_IOCTL_NO_CONTROLLER | FCOE_IOCTL_FCF_LOGOUT;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   cna = contlr->cna;

   VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                      status,
                      contlr->fcoe->fcoe_destroy,
                      cna->netdev);
   if (status) {
      VMKLNX_WARN("Fcoe Destroy failed: %d", status);
      goto flogout_failure;
   }

   status = LinNet_RemoveVlanGroupDevice(cna->netdev, VMK_TRUE, NULL);
   if (VMK_OK != status) {
      VMKLNX_WARN("Removing FCOE VLAN group failed: %s",
                  vmk_StatusToString(status));
      goto flogout_failure;
   }

   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_FCF_LOGOUT;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;

flogout_failure:
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_FCF_LOGOUT;
   fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FCoEDestroyController
 *
 *	This destroys FCoE controller
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Resources allocated for FCoE controller are freed
 *----------------------------------------------------------------------
 */
static void
FCoEDestroyController(struct fcoe_ioctl_pkt *fcoe_ioctl)
{
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_contlr *contlr;
   atomic_t num_vnports = { 0 };
   VMK_ReturnStatus vmkRet = VMK_OK;
   struct fcoe_cntlr *ioctlContlr = (struct fcoe_cntlr *) fcoe_ioctl->data;

   contlr = FCoEGetVmklnxContlr(ioctlContlr->vmnic_name);
   if (!contlr) {
      fcoe_ioctl->cmd = FCOE_IOCTL_NO_CONTROLLER | FCOE_IOCTL_DESTROY_FC_CONTROLLER;
      fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
      return;
   }

   num_vnports.counter = atomic_read(&contlr->vnports);
   if (num_vnports.counter != 0) {
      VMKLNX_WARN("Contlr has non-zero vnports: %d", num_vnports.counter);
      goto fcoe_destroy_failure;
   }

   cna = contlr->cna;
   cna->handle = NULL;
   cna->rx_handler = NULL;

   /*
    * Clear the CNA Properties
    */
   if (CNAClearProperties(cna, VMK_FALSE) != VMK_OK) {
      VMKLNX_DEBUG(2, "CNA Clear properties failed for %s",
                   cna->netdev->name);
      goto fcoe_destroy_failure;
   }

   /*
    * Decrement the reference count taken
    */
   vmkRet = vmk_ModuleDecUseCount(contlr->fcoe->module_id);
   VMK_ASSERT(vmkRet == VMK_OK);
   vmkRet = vmk_ModuleDecUseCount(cna->netdev->module_id);
   VMK_ASSERT(vmkRet == VMK_OK);

   kfree(contlr);

   /*
    * Complete the ioctl
    */
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DESTROY_FC_CONTROLLER;
   fcoe_ioctl->status = FCOE_IOCTL_SUCCESS;
   return;

fcoe_destroy_failure:
   fcoe_ioctl->cmd = FCOE_IOCTL_RESPONSE_CODE | FCOE_IOCTL_DESTROY_FC_CONTROLLER;
   fcoe_ioctl->status = FCOE_IOCTL_GENERIC_FAILURE;
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxCNA_RegisterNetDev
 *
 *	Register CNA netdev structure
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Adds CNA to the list of avaialable CNA vmnics
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
LinuxCNA_RegisterNetDev(struct net_device *netdev)
{
   struct vmklnx_cna *cna;
   unsigned vmkFlag;

   VMK_ASSERT(netdev);

   cna = kzalloc(sizeof(*cna), GFP_KERNEL);

   if (!cna) {
      VMKLNX_WARN("Failed to allocate memory for cna netdev");
      return VMK_FAILURE;
   }

   VMK_ASSERT(netdev->features & NETIF_F_CNA);

   cna->netdev = netdev;

   vmkFlag = vmk_SPLockIRQ(&linuxCNAAdapterLock);
   if (totalCNA < VMKLNX_MAX_CNA) {
      ++totalCNA;
   } else {
      vmk_SPUnlockIRQ(&linuxCNAAdapterLock, vmkFlag);
      VMKLNX_WARN("Can't support more than %d CNA net devices", VMKLNX_MAX_CNA);
      goto free_cna_memory;
   }
   list_add_tail(&cna->entry, &linuxCNAAdapterList);
   vmk_SPUnlockIRQ(&linuxCNAAdapterLock, vmkFlag);

   spin_lock_init(&cna->lock);
   /*
    * Update the cna_ops
    */
   netdev->cna_ops = (void *)cna;

   return VMK_OK;

free_cna_memory:
   kfree(cna);
   return VMK_FAILURE;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACheckQueues -
 *
 *	Check if the CNA net device has backing Queues
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Removes CNA from the list of avaialable CNA vmnics
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACheckQueues(struct net_device *netdev)
{
   /*
    * Check if there is a valid netqueue ops
    */
   if (!netdev->netqueue_ops) {
      VMKLNX_WARN("No Netqueue ops found for CNA");
      return VMK_FAILURE;
   }

   /*
    * Check queue features
    */
   if (CNACheckQueueFeatures(netdev) != VMK_OK) {
      VMKLNX_WARN("CNA does not support the required features");
      return VMK_FAILURE;
   }

   if (CNACheckQueueCount(netdev) != VMK_OK) {
      VMKLNX_WARN("CNA does not have required queue count");
      return VMK_FAILURE;
   }

   if (CNACheckFilterCount(netdev) != VMK_OK) {
      VMKLNX_WARN("CNA does not support required Filters");
      return VMK_FAILURE;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACheckQueueFeatures
 *
 *	Check if the Queue supports required features
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACheckQueueFeatures(struct net_device *netdev)
{
   vmk_NetqueueOpGetFeaturesArgs args;

   if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                   VMK_NETQUEUE_OP_GET_FEATURES, (void *) &args)) {
      VMKLNX_DEBUG(2, "Get Features for netqueue failed on CNA");
      return VMK_FAILURE;
   } else {
      if ((args.features & VMK_NETQUEUE_FEATURE_RXQUEUES) &&
          (args.features & VMK_NETQUEUE_FEATURE_TXQUEUES)) {
         return VMK_OK;
      } else {
         VMKLNX_DEBUG(2, "CNA adapter does not support TX and RX queues");
         return VMK_FAILURE;
      }
   }
}

/*
 *----------------------------------------------------------------------
 *
 * CNACheckQueueCount
 *
 *	Check the Queue Count
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACheckQueueCount(struct net_device *netdev)
{
   vmk_NetqueueOpGetQueueCountArgs  args;

   args.qtype = VMK_NETQUEUE_QUEUE_TYPE_TX;

   /*
    * Get the TX queue count
    */
   if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                   VMK_NETQUEUE_OP_QUEUE_COUNT, (void *) &args)) {
      VMKLNX_DEBUG(2, "Get TX queue count failed for CNA");
      return VMK_FAILURE;
   } else if (args.count < CNA_MAX_QUEUE) {
      VMKLNX_DEBUG(2, "TX queue count is less than expected for CNA - %d",
                   args.count);
      return VMK_FAILURE;
   }

   /*
    * Get the RX queue count
    */
   args.count = 0;
   args.qtype = VMK_NETQUEUE_QUEUE_TYPE_RX;

   if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                   VMK_NETQUEUE_OP_QUEUE_COUNT, (void *) &args)) {
      VMKLNX_DEBUG(2, "Get RX queue count failed for CNA");
      return VMK_FAILURE;
   } else if (args.count < CNA_MAX_QUEUE) {
      VMKLNX_DEBUG(2, "RX queue count is less than expected for CNA - %d",
                   args.count);
      return VMK_FAILURE;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CNACheckFilterCount
 *
 *	Check the number of filters
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNACheckFilterCount(struct net_device *netdev)
{
   vmk_NetqueueOpGetFilterCountArgs args;

   args.qtype = VMK_NETQUEUE_QUEUE_TYPE_RX;

   if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                   VMK_NETQUEUE_OP_FILTER_COUNT, (void *) &args)) {
      VMKLNX_DEBUG(2, "Get RX filter count failed for CNA");
      return VMK_FAILURE;
   } else if (args.count < CNA_MAX_FILTER_PER_QUEUE) {
      VMKLNX_DEBUG(2, "RX filter count is less than expected for CNA - %d",
                   args.count);
      return VMK_FAILURE;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 * LinuxCNA_UnRegisterNetDev -
 *
 *	Unregister the CNA. This should only be called during module
 * unload time
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
LinuxCNA_UnRegisterNetDev(struct net_device *netdev)
{
   struct vmklnx_cna *cna;
   unsigned vmkFlag;

   VMK_ASSERT(netdev);

   cna = (struct vmklnx_cna *) netdev->cna_ops;
   if (!cna) {
      VMKLNX_ALERT("Invalid CNA being unregisterred");
      return VMK_FAILURE;
   }

   vmkFlag = vmk_SPLockIRQ(&linuxCNAAdapterLock);
   --totalCNA;
   list_del(&cna->entry);
   vmk_SPUnlockIRQ(&linuxCNAAdapterLock, vmkFlag);

   /*
    * Time to free allocated memory
    */
   kfree(cna);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * cna_netdev_process_rx
 *
 *	Receive handler
 *
 * Results:
 *      int - 0 on success
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
static int
cna_netdev_process_rx(struct sk_buff *skb, void *handle)
{
   __be16 proto_type;
   int status;
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_contlr *contlr;

   VMKLNX_DEBUG(5, "CNA - RX entered");

   if (ETH_P_8021Q  == htons(eth_hdr(skb)->h_proto)) {
      /* VLAN tag present. Look beyond for frame type */
      proto_type = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;
   } else {
      proto_type = htons(eth_hdr(skb)->h_proto);
   }

   cna = (struct vmklnx_cna *) skb->dev->cna_ops;

   if (!cna) {
      VMK_ASSERT(cna);
      VMKLNX_WARN("CNA is not valid for RX");
      return -1;
   }

   contlr = (struct vmklnx_fcoe_contlr *)handle;

   if (!contlr) {
      VMKLNX_WARN("Controller is not valid for RX");
      return -1;
   }

   skb_pull(skb, eth_header_len((struct ethhdr *)skb->data));

   /*
    * We need to process FCoE and FIP frames only.
    */
   switch (proto_type) {
      case ETH_P_FCOE:
         VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                            status,
                            contlr->fcoe->fcoe_recv,
                            skb);
         break;
      case ETH_P_FIP:
         VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                            status,
                            contlr->fcoe->fip_recv,
                            skb);
         break;
      default:
         VMKLNX_WARN("Non-FCOE or FIP type (%d) received on CNA \"%s\"",
                     proto_type, cna->netdev->name);
         VMK_ASSERT(FALSE);
         status = -1;
   }

   if (status) {
      VMKLNX_DEBUG(2, "FCoE Driver failed receive request (status: %d, "
                      "proto_type: %d)", status, proto_type);
      return -1;
   }
   VMKLNX_DEBUG(5, "CNA - RX complete");
   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxCNA_RxProcess
 *
 *	Receive handler called by vmklinux network layer
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
void
LinuxCNA_RxProcess(vmk_PktList pktList, struct net_device *dev)
{
   vmk_PktHandle *pkt;
   struct sk_buff *skb;
   struct vmklnx_cna *cna;

   cna = (struct vmklnx_cna *) dev->cna_ops;

   while (!vmk_PktListIsEmpty(pktList)) {
      pkt = vmk_PktListPopFirstPkt(pktList);

      vmk_PktGetCompletionData(pkt, (vmk_PktCompletionData *)&skb);
      vmk_PktClearCompletionData(pkt);

      if (cna->rx_handler) {
         /*
          * Client RX handlers are responsible for free'ing the skb.
          */
         cna->rx_handler(skb, cna->handle);
      } else {
         /*
          * No client RX handler, we are responsible for free'ing skb.
          */
         VMKLNX_DEBUG(5, "CNA skb received, but no RX handler (free'ing %p)",
                         skb);
         kfree_skb(skb);
      }
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxCNA_Poll --
 *
 *    Callback registered with the napi handler for CNA device.
 *
 *  Results:
 *    None
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
LinuxCNA_Poll(vmk_PktList rxPktList, vmk_AddrCookie cookie) 
{
   struct napi_struct *napi = (struct napi_struct *)cookie.ptr;
   LinuxCNA_RxProcess(rxPktList, napi->dev);
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxCNADev_Poll --
 *
 *    Callback registered with the backup net device handler for CNA device
 *
 *  Results:
 *    None
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
LinuxCNADev_Poll(vmk_PktList rxPktList, vmk_AddrCookie cookie) 
{
   struct net_device *dev= (struct net_device *)cookie.ptr;
   LinuxCNA_RxProcess(rxPktList, dev);
}

/**
 *  vmklnx_fcoe_attach_transport -
 *  @this_module: Module structure of template client
 *  @fcoe: Pointer to fcoe template
 *
 *  Adds the given fcoe template to the list of FCOE handlers.
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux. This should be called during module
 *  load time.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_fcoe_attach_transport */
int
vmklnx_fcoe_attach_transport (struct module *this_module,
                              struct vmklnx_fcoe_template *fcoe)
{
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(fcoe);

   if ((!fcoe->name) || (!fcoe->fcoe_create) || (!fcoe->fcoe_destroy) ||
       (!fcoe->fcoe_recv) || (!fcoe->fip_recv)) {
      VMKLNX_DEBUG(2, "Insufficient parameters for fcoe transport attach");
      return -1;
   }

   /*
    * Always initialize module_id regardless of whether "default" template.
    */
   fcoe->module_id = this_module->moduleID;
   VMKLNX_DEBUG(0, "FCOE transport \"%s\" using module ID %d",
                   fcoe->name, fcoe->module_id);

   if (!strncmp(fcoe->name, "fcoe_sw_esx", VMKLNX_FCOE_TEMPLATE_NAME)) {
      /*
       * The template claims to be the default. Check if we have any
       * registerred so far
       */
      vmkFlag = vmk_SPLockIRQ(&linuxFCoEClientLock);
      if (default_fcoe_owner) {
         vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);
         VMKLNX_DEBUG(2, "Default owner already been set");
         return -1;
      } else {
         default_fcoe_owner = fcoe;
         vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);
         VMKLNX_DEBUG(2, "Set %s as default fcoe owner", fcoe->name);
         return 0;
      }
   }

   /*
    * All other FCOE templates need to provide the pci_id they want to control.
    * If not fail them
    */
   if (!fcoe->id_table) {
      VMKLNX_DEBUG(2, "Missing PCI table for fcoe transport attach");
      return -1;
   }

   INIT_LIST_HEAD(&fcoe->entry);
   vmkFlag = vmk_SPLockIRQ(&linuxFCoEClientLock);
   list_add_tail(&fcoe->entry, &linuxFCoEClientList);
   vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);

   VMKLNX_DEBUG(2, "Added %s to fcoe client list", fcoe->name);

   return 0;
}
EXPORT_SYMBOL(vmklnx_fcoe_attach_transport);

/**
 * vmklnx_fcoe_release_transport -
 * @fcoe: Pointer to fcoe template
 *
 * Removes the given fcoe template from the list of FCOE handlers.
 *
 * ESX Deviation Notes:
 * This API is not present in Linux. This should be called during module
 * unload time.
 */
/* _VMKLNX_CODECHECK_: vmklnx_fcoe_release_transport */
int
vmklnx_fcoe_release_transport (struct vmklnx_fcoe_template *fcoe)
{
   unsigned vmkFlag;
   int ret = -1;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Check if this is the default FCoE template
    */
   if (!strncmp(fcoe->name, "fcoe_sw_esx", VMKLNX_FCOE_TEMPLATE_NAME)) {
      vmkFlag = vmk_SPLockIRQ(&linuxFCoEClientLock);

      if (default_fcoe_owner == fcoe) {
         default_fcoe_owner = NULL;
         ret = 0;
      }
      vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);
      return ret;
   }

   vmkFlag = vmk_SPLockIRQ(&linuxFCoEClientLock);
   list_del(&fcoe->entry);
   vmk_SPUnlockIRQ(&linuxFCoEClientLock, vmkFlag);

   return 0;
}
EXPORT_SYMBOL(vmklnx_fcoe_release_transport);

/*
 *----------------------------------------------------------------------
 *
 * CNASetProperties -
 *
 *	Setup CNA features
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      Sets MTU, allocates TX and RX queue and applies RX filter
 *      Enable HW vlan and add new vlan id
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNASetProperties(struct vmklnx_cna *cna, struct fcoe_cntlr *fcoeCntlr)
{
   struct net_device *netdev = cna->netdev;
   unsigned int i, j, bitNum;
   VMK_ReturnStatus vmkRet = VMK_OK;
   vmk_VlanID newVid = 0;

   /*
    * ToDo:
    * Check on the Priority and allocate queues from this priority
    */

   /*
    * For now we only support a single VLAN ID.
    * The first set bit will win; fallback to zero.
    */
   cna->fcoe_vlan_id = (vmk_VlanID) 0;
   for (i = 0; i < 128; i++) {
      if (fcoeCntlr->vlan[i] != 0) {
         for (j = 0; j < 32; j++) {
            bitNum = ((i * 32) + j);
            if (fcoeCntlr->vlan[i] & (0x00000001 << j)) {

               VMKLNX_DEBUG(2, "Calculated user-requested VLAN %d",
                               bitNum);

               newVid = (vmk_VlanID) bitNum;
               i = 128;
               j = 32;
            }
         }
      }
   }

   /*
    * enable HW vlan and add new vlan id
    */
   if (vmklnx_cna_set_vlan_tag(netdev, newVid)) {
         return VMK_FAILURE;
   }

   VMKLNX_DEBUG(2, "Using VLAN ID %d for FCOE traffic",
                   cna->fcoe_vlan_id);

   VMK_ASSERT((vmk_VlanPriority) fcoeCntlr->priority < 8);
   cna->fcoe_app_prio = (vmk_VlanPriority) fcoeCntlr->priority;
   VMKLNX_DEBUG(2, "Using priority %d for FCOE traffic",
                   fcoeCntlr->priority);

   /*
    * update mac address to the netdev
    */
   memcpy(netdev->dev_addr, fcoeCntlr->mac_address, 6);

   /*
    * Initialize the TX/RX queues
    */
   vmkRet = CNAInitQueues(cna, fcoeCntlr->mac_address, CNA_MAX_QUEUE);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Initializing of TX/RX queues failed: %s",
                  vmk_StatusToString(vmkRet));
      return VMK_FAILURE;
   }
   VMKLNX_DEBUG(1, "CNA allocated TX and RX queues");

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * CNAInitQueues -
 *
 *      Initializes the TX & RX queues and set the MAC RX filter
 *
 * Results:
 *      VMK_OK on success
 *
 * Side Effects:
 *      This holds the cna->lock.
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAInitQueues(struct vmklnx_cna *cna, vmk_uint8 *mac_address, int max_queues)
{
   VMK_ReturnStatus vmkRet = VMK_OK;
   unsigned long flags;

   VMKLNX_DEBUG(1, "Initializing TX/RX queues for cna device");

   /*
    * Allocate Queue
    */
   vmkRet = CNAAllocTXRXQueue(cna, max_queues);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Allocation of TX and RX Queue failed: %s",
                  vmk_StatusToString(vmkRet));
      return VMK_FAILURE;
   }
   VMKLNX_DEBUG(1, "CNA allocated TX and RX queues");

   spin_lock_irqsave(&cna->lock, flags);

   /*
    * Set RX filter
    */
   vmkRet = CNASetRxFilter(cna, mac_address);
   spin_unlock_irqrestore(&cna->lock, flags);

   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Failed to set rx filter: %s",
                  vmk_StatusToString(vmkRet));
      vmkRet = CNAFreeTXRXQueue(cna, VMK_FALSE);
      if (vmkRet != VMK_OK) {
         VMK_ASSERT(FALSE);
         VMKLNX_WARN("Failed to release the allocated queues: %s",
                     vmk_StatusToString(vmkRet));
         return VMK_FAILURE;
      }
   }

   if (vmkRet == VMK_OK) {
      VMKLNX_DEBUG(1, "CNA RX filter set");
   }

   return vmkRet;
}


/*
 *----------------------------------------------------------------------
 *
 * CNAAllocTXRXQueue -
 *
 *	Allocate TX and RX queue. Today, max queue allocated is 1, but
 * we may want to be ready for the future
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      This holds the cna->lock
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAAllocTXRXQueue(struct vmklnx_cna *cna, int count)
{
   vmk_NetqueueOpAllocQueueArgs  args;
   struct net_device *netdev = cna->netdev;
   int qindex;
   unsigned long flags;

   /*
    * Get required TX queues
    */
   for (qindex = 0; qindex < count; ++qindex) {
      /*
       * Zero out args each time.
       */
      memset(&args, 0, sizeof(args));
      args.qtype = VMK_NETQUEUE_QUEUE_TYPE_TX;
      vmk_NetqueueMkTxQueueID(&args.qid, 0, qindex);

      VMKLNX_DEBUG(2, "CNA TX Queue pre-alloc qid: 0x%llx",
                   (uint64_t) args.qid);

      if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                      VMK_NETQUEUE_OP_ALLOC_QUEUE, (void *) &args)) {
         VMKLNX_WARN("Failed to allocate TX queue for CNA");
         CNAFreeTXRXQueue(cna, VMK_FALSE);
         return VMK_FAILURE;
      }
      /*
       * Update the TX queue information
       */
      spin_lock_irqsave(&cna->lock, flags);
      cna->tqueue[qindex].tx_queue_id = args.qid;
      cna->tqueue[qindex].active = VMK_TRUE;
      spin_unlock_irqrestore(&cna->lock, flags);

      VMKLNX_DEBUG(2, "CNA TX Queue post-alloc qid: 0x%llx",
                   (uint64_t) args.qid);
   }

   /*
    * Get RX queues
    */
   for (qindex = 0; qindex < count; ++qindex) {
      /*
       * Zero out args each time.
       */
      memset(&args, 0, sizeof(args));
      args.qtype = VMK_NETQUEUE_QUEUE_TYPE_RX;
      vmk_NetqueueMkRxQueueID(&args.qid, 0, qindex);

      VMKLNX_DEBUG(2, "CNA RX Queue pre-alloc qid: 0x%llx",
                   (uint64_t) args.qid);

      if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                      VMK_NETQUEUE_OP_ALLOC_QUEUE, (void *) &args)) {
         VMKLNX_WARN("Failed to allocate RX queue for CNA");
         CNAFreeTXRXQueue(cna, VMK_FALSE);
         return VMK_FAILURE;
      }
      /*
       * Update the RX queue information
       */
      spin_lock_irqsave(&cna->lock, flags);
      cna->rqueue[qindex].rx_queue_id = args.qid;
      cna->rqueue[qindex].active = VMK_TRUE;
      spin_unlock_irqrestore(&cna->lock, flags);

      VMKLNX_DEBUG(2, "CNA RX Queue post-alloc qid: 0x%llx",
                   (uint64_t) args.qid);
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CNAFreeTXRXQueue -
 *
 *      Free TX and RX queue
 *
 * @is_reset: whether this function is called for hw reset handling.
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      This holds the cna->lock. If there are any filters applied
 * they should be removed before this
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAFreeTXRXQueue(struct vmklnx_cna *cna, vmk_Bool is_reset)
{
   VMK_ReturnStatus result = VMK_OK;
   vmk_NetqueueOpFreeQueueArgs args;
   struct net_device *netdev = cna->netdev;
   int i;
   unsigned long flags;

   spin_lock_irqsave(&cna->lock, flags);
   for (i = 0; i < CNA_MAX_QUEUE; ++i) {

      /* free up the tx queue resources */
      if (cna->tqueue[i].active == VMK_TRUE) {
         args.qid = cna->tqueue[i].tx_queue_id;
         if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                         VMK_NETQUEUE_OP_FREE_QUEUE, (void *) &args)) {
            VMKLNX_WARN("Failed to free allocated TX queue for CNA");
            result = VMK_FAILURE;

            /*
	     * Even if the tx/rx queue removal failed we reset the "active" flag as
             * the low level driver has reset the queue information.
             */
            if (is_reset) {
               cna->rqueue[i].active = VMK_FALSE;
            }
         } else {
            cna->tqueue[i].active = VMK_FALSE;
         }
      }

      /* free up the rx queue resources */
      if (cna->rqueue[i].active == VMK_TRUE) {
         args.qid = cna->rqueue[i].rx_queue_id;
         if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                         VMK_NETQUEUE_OP_FREE_QUEUE, (void *) &args)) {
            VMKLNX_WARN("Failed to free allocated RX queue for CNA");
            result = VMK_FAILURE;
            if (is_reset) {
               cna->rqueue[i].active = VMK_FALSE;
            }
         } else {
            cna->rqueue[i].active = VMK_FALSE;
         }
      }
   }
   spin_unlock_irqrestore(&cna->lock, flags);

   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CNASetRxFilter -
 *
 *	Set RX filter on this device
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      The caller should hold the cna->lock before calling this function
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNASetRxFilter(struct vmklnx_cna *cna, unsigned char *mac_address)
{
   VMK_ReturnStatus result = VMK_OK;
   struct net_device *netdev = cna->netdev;
   vmk_NetqueueOpApplyRxFilterArgs args;
   vmk_UplinkQueueVXLANFilterInfo filterInfo;
   int qindex, findex;

   args.filter.filterInfo.ptr = &filterInfo;

   /*
    * Refuse to set "zero" MAC address filter.
    */
   if (is_zero_ether_addr(mac_address)) {
      VMKLNX_WARN("Can not set zero MAC address CNA RX filter");
      VMK_ASSERT(0);
      result = VMK_FAILURE;
      goto rx_filter_err;
   }

   result = CNAGetFreeRxIndices(cna, &qindex, &findex);
   if (result != VMK_OK ) {
      VMKLNX_DEBUG(0, "Failed to get free RX index for MAC address "
                      "%02x:%02x:%02x:%02x:%02x:%02x : %s",
                      mac_address[0], mac_address[1], mac_address[2],
                      mac_address[3], mac_address[4], mac_address[5],
                      vmk_StatusToString(result));
      result = VMK_FAILURE;
      goto rx_filter_err;
   }

   args.qid = cna->rqueue[qindex].rx_queue_id;
   args.filter.class = VMK_UPLINK_QUEUE_FILTER_CLASS_MAC_ONLY;
   memcpy(((vmk_UplinkQueueMACFilterInfo*)args.filter.filterInfo.ptr)->mac,
          mac_address, 6);

   VMKLNX_DEBUG(2, "Set up MAC address %02x:%02x:%02x:%02x:%02x:%02x",
                mac_address[0], mac_address[1], mac_address[2],
                mac_address[3], mac_address[4], mac_address[5]);

   result = LinNet_NetqueueOp((void *) netdev,
                              VMK_NETQUEUE_OP_APPLY_RX_FILTER, (void *) &args);
   if (result != VMK_OK) {
      VMKLNX_DEBUG(0, "Failed to apply CNA RX filter: %s",
                      vmk_StatusToString(result));
      result = VMK_FAILURE;
      goto rx_filter_err;
   } else {
      cna->rqueue[qindex].filter[findex].id = args.fid;
      ++cna->rqueue[qindex].filter[findex].ref;
      memcpy(cna->rqueue[qindex].filter[findex].mac_addr, mac_address, 6);
   }

rx_filter_err:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * CNAGetFreeRxIndices -
 *
 *	Get free index for applying the rx filter
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      Callers must hold the cna->lock prior to calling this function
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAGetFreeRxIndices(struct vmklnx_cna *cna, int *qid, int *fid)
{
   int qindex, findex;

   for (qindex = 0; qindex < CNA_MAX_QUEUE; ++qindex) {

      if (cna->rqueue[qindex].active == VMK_TRUE) {
         for (findex = 0; findex < CNA_MAX_FILTER_PER_QUEUE; ++findex) {
            if (cna->rqueue[qindex].filter[findex].ref == 0) {
               *qid = qindex;
               *fid = findex;
               return VMK_OK;
            }
         }
      }
   }
   return VMK_FAILURE;
}

/*
 *----------------------------------------------------------------------
 *
 * CNAClearRxFilter -
 *
 *	Free up all the filters associated with a CNA netdev
 *
 * @is_reset: whether this function is called for hw reset handling.
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      Holds the cna->lock
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAClearRxFilter(struct vmklnx_cna *cna, vmk_Bool is_reset)
{
   VMK_ReturnStatus result = VMK_OK;
   unsigned long flags;
   int qindex, findex;

   spin_lock_irqsave(&cna->lock, flags);
   for (qindex = 0; qindex < CNA_MAX_QUEUE; ++qindex) {
      if (cna->rqueue[qindex].active == VMK_TRUE) {

         for (findex = 0; findex < CNA_MAX_FILTER_PER_QUEUE; ++findex) {
            if (cna->rqueue[qindex].filter[findex].ref) {
               /* If this is a reset, then just init the fields */
               if (is_reset) {
                  cna->rqueue[qindex].filter[findex].ref = 0;
                  memset(cna->rqueue[qindex].filter[findex].mac_addr, 0, 6);
	       } else {
                  if (VMK_OK != CNARemoveRxFilter(cna, qindex, findex)) {
                     VMKLNX_WARN("Failed to remove CNA RX filter");
                     result = VMK_FAILURE;
                  } else {
                     /*
                      * If we removed filter, ref count should have been only 1.
                      */
                     --cna->rqueue[qindex].filter[findex].ref;
                     VMK_ASSERT(cna->rqueue[qindex].filter[findex].ref == 0);

                     /*
                      * Clear out the mac address field so this entry is not
                      * erroneously reused via vmklnx_cna_set_macaddr.
                      */
                     memset(cna->rqueue[qindex].filter[findex].mac_addr, 0, 6);
                  }
	       } // end if (!is_reset)
            }
         }
      }
   }
   spin_unlock_irqrestore(&cna->lock, flags);
   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CNARemoveRxFilter -
 *
 *	Remove the rx filter
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      This function should be called with cna->lock
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNARemoveRxFilter(struct vmklnx_cna *cna, int qindex, int findex)
{
   vmk_NetqueueOpRemoveRxFilterArgs args;
   struct net_device *netdev = cna->netdev;

   args.qid = cna->rqueue[qindex].rx_queue_id;
   args.fid = cna->rqueue[qindex].filter[findex].id;

   if (VMK_OK != LinNet_NetqueueOp((void *) netdev,
                                   VMK_NETQUEUE_OP_REMOVE_RX_FILTER, (void *) &args)) {
      VMKLNX_WARN("Failed to remove CNA RX filter on %s", netdev->name);
      return  VMK_FAILURE;
   }
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CNAClearProperties -
 *
 *	Free up the resources
 *
 * @is_reset: whether this function is called for hw reset handling.
 *
 * Results:
 *      VMK_OK on success
 *
 * Side Effects:
 *      Resources are freed back to the pool
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
CNAClearProperties(struct vmklnx_cna *cna, vmk_Bool is_reset)
{
   VMKLNX_DEBUG(1, "Clearing TX/RX queues: Reset=%d", is_reset);

   if (CNAClearRxFilter(cna, is_reset) != VMK_OK) {

      /* If this is a reset case then proceed with freeing of tx/rx queues. */
      if (!is_reset) {
         VMKLNX_WARN("RX filter removal failed");
         return VMK_FAILURE;
      }
   }

   if (CNAFreeTXRXQueue(cna, is_reset) != VMK_OK) {
      VMKLNX_WARN("Failed to free TX/RX Queue");
      return VMK_FAILURE;
   }

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_cleanup_queues -
 *
 *	Clear filter and TX/RX queues for the adapter.
 *
 * Results:
 *      0 on success
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
int
vmklnx_cna_cleanup_queues(struct net_device *netdev)
{
   VMK_ReturnStatus vmkRet = VMK_OK;
   struct vmklnx_cna *cna;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(netdev);
   VMK_ASSERT(netdev->cna_ops);

   cna = (struct vmklnx_cna *)netdev->cna_ops;

   /*
    * Clear the MAC RX filters and free up TX/RX resources.
    */
   vmkRet = CNAClearProperties(cna, VMK_TRUE);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("CNA Clear properties failed for %s", netdev->name);
   } else {
      VMKLNX_DEBUG(1, "Successfully cleaned up the TX and RX Queues for %s",
                  netdev->name);
   }

   return (vmkRet == VMK_OK) ? 0 : -1;
}
EXPORT_SYMBOL(vmklnx_cna_cleanup_queues);


/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_reinit_queues -
 *
 *	Re-allocate the TX/RX queues and re-apply netdev mac filters
 *	for the adapter. This should be called upon the link up event
 *	after the underying hw reset is complete.
 *
 *	vmklnx_cna_cleanup_queues should have been called first. If not,
 *	driver should NOT call this API after reset is complete.
 *
 * Results:
 *      0 on success
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
int
vmklnx_cna_reinit_queues(struct net_device *netdev)
{
   unsigned long flags;
   VMK_ReturnStatus vmkRet = VMK_OK;
   struct vmklnx_cna *cna;
   unsigned char *mac_address;
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(netdev);
   VMK_ASSERT(netdev->cna_ops);

   cna = (struct vmklnx_cna *)netdev->cna_ops;
   mac_address = netdev->dev_addr;

   spin_lock_irqsave(&cna->lock, flags);
   for (i = 0; i < CNA_MAX_QUEUE; ++i) {
      /* check if queues have already been allocated */
      if (cna->tqueue[i].active == VMK_TRUE) {
            VMKLNX_WARN("TX queue already allocated for %s", netdev->name);
	    VMK_ASSERT(0);
      }
      if (cna->rqueue[i].active == VMK_TRUE) {
	    VMKLNX_WARN("RX queue already allocated for %s", netdev->name);
	    VMK_ASSERT(0);
      }
   }
   spin_unlock_irqrestore(&cna->lock, flags);

   /*
    * Initialize the TX/RX Queues for the CNA adapter
    */
   vmkRet = CNAInitQueues(cna, mac_address, CNA_MAX_QUEUE);
   if (vmkRet != VMK_OK) {
      VMKLNX_WARN("Re-initializing of TX and RX Queue failed for %s: %s",
                  netdev->name, vmk_StatusToString(vmkRet));
   } else {
      VMKLNX_DEBUG(1, "Successfully Re-allocated the TX and RX Queues for %s",
                  netdev->name);
   }

   return (vmkRet == VMK_OK) ? 0 : -1;
}
EXPORT_SYMBOL(vmklnx_cna_reinit_queues);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_queue_xmit -
 *
 *	Transmits a single frame
 *
 * Results:
 *      0 on success
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
int
vmklnx_cna_queue_xmit(struct sk_buff *skb)
{
   struct net_device *netdev = skb->dev;
   int xmit_status = -1;
   VMK_ReturnStatus vmkRet = VMK_OK;
   struct vmklnx_cna *cna;
   vmk_VlanID vlanID;
   vmk_VlanPriority priority;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   cna = (struct vmklnx_cna *)netdev->cna_ops;

   VMKLNX_DEBUG(5, "CNA - TX entered");

   if (!cna) {
      VMKLNX_DEBUG(0, "Cannot lookup backing cna netdev");
      return xmit_status;
   }

   /*
    * Callers must have marked frags as owned by Vmkernel.
    */
   VMK_ASSERT((skb_shinfo(skb)->nr_frags == 0) ||
              (vmklnx_is_skb_frags_owner(skb) == 0));

   /*
    * Tag skb according to FCOE VLAN and priority.
    */
   vlanID = cna->fcoe_vlan_id;
   priority = cna->fcoe_app_prio;
   if ((priority != 0) || (vlanID != 0)) {
      vlan_put_tag(skb, vlanID | (priority << VLAN_1PTAG_SHIFT));
   }
   skb->priority = priority;

   /*
    * For now tx data on first queue
    */
   VMK_ASSERT(cna->tqueue[0].active == VMK_TRUE);
   vmkRet = LinNet_NetqueueSkbXmit(netdev, cna->tqueue[0].tx_queue_id, skb);
   switch (vmkRet) {
      static uint32_t txFailThrottle = 0;
      case VMK_OK:
         xmit_status = 0;
         VMKLNX_DEBUG(5, "CNA - TX completed");
         break;
      case VMK_FAILURE:
         VMKLNX_THROTTLED_WARN(txFailThrottle,
                               "TX Failed: LinNet_NetqueueSkbXmit "
                               "returned failure");
         break;
      case VMK_BUSY:
         VMKLNX_DEBUG(9, "TX Failed: LinNet_NetqueueSkbXmit "
                               "returned busy");
         break;
      default:
         VMKLNX_WARN("Unhandled error: 0x%0x - '%s'",
                     vmkRet, vmk_StatusToString(vmkRet));
         break;
   }

   return xmit_status;
}
EXPORT_SYMBOL(vmklnx_cna_queue_xmit);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_get_vlan_tag -
 *
 *      Retrieve the VLAN tag to be used for the CNA associated with
 *      the given net_device.
 *
 * Results:
 *      802.1q VLAN tag.
 *
 *      The VLAN ID can be computed as: (result & VLAN_VID_MASK).
 *
 *      The 802.1p priority tag can be computed as:
 *      ((result & VLAN_1PTAG_MASK) >> VLAN_1PTAG_SHIFT).
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
unsigned short vmklnx_cna_get_vlan_tag(struct net_device *netdev) {
   struct vmklnx_cna *cna;
   unsigned short result = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(netdev);

   cna = (struct vmklnx_cna *) netdev->cna_ops;
   if (!cna) {
      VMKLNX_ALERT("No CNA for netdev %s, can't compute VLAN tag",
                   netdev->name);
      VMK_ASSERT(cna);
      return result;
   }

   result = (cna->fcoe_vlan_id | (cna->fcoe_app_prio << VLAN_1PTAG_SHIFT));

   return result;
}
EXPORT_SYMBOL(vmklnx_cna_get_vlan_tag);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_set_vlan_tag -
 *
 *      Enable HW vlan and add new vlan id.
 *      Remove the old vlan id.
 *
 * Results:
 *      Return 0 if there is VLan HW tx/rx acceleration support;
 *      Return -1 otherwise.
 *
 * Side Effects:
 *      hw vlan register is updated.
 *      cna's fcoe_vlan_id is updated.
 *      802.1p bits are not touched with this function
 *
 *-----------------------------------------------------------------------------
 */
int
vmklnx_cna_set_vlan_tag(struct net_device *dev, unsigned short vid)
{
   struct vmklnx_cna *cna;
   vmk_VlanID old_vid;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dev);
   if (!dev) {
      VMKLNX_DEBUG(0, "net_device handle is NULL\n");
      return -1;
   }
   cna = (struct vmklnx_cna *)dev->cna_ops;

   VMK_ASSERT(cna);
   if (!cna) {
      VMKLNX_DEBUG(0, "vmklnx_cna handle is NULL\n");
      return -1;
   }
   old_vid = cna->fcoe_vlan_id;

   /* If the new vid is same as old_vid return success */
   if ((int)old_vid == (int)vid) {
      VMKLNX_DEBUG(0, "%s: new vid:%d is same as old vid:%d",
                       dev->name, (int)vid, (int)old_vid);
      return 0;
   }

   if (LinNet_EnableHwVlan(dev) != VMK_OK) {
      goto error;
   }

   /* if hw doesn't support rx vlan filter, bail out here */
   if (!(dev->features & NETIF_F_HW_VLAN_FILTER)) {
      VMKLNX_DEBUG(0, "%s: HW doesn't support rx vlan filter", dev->name);
      goto error;
   }

   if (old_vid) {
      VMK_ASSERT(dev->vlan_rx_kill_vid);
      if (!dev->vlan_rx_kill_vid) {
         VMKLNX_DEBUG(0, "%s: no vlan_rx_kill_vid handler", dev->name);
         goto error;
      }

      VMKLNX_DEBUG(1, "%s: deleting vlan id %d", dev->name, (int)old_vid);
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_kill_vid, dev,
			old_vid);
   }

   if (vid) {
      VMK_ASSERT(dev->vlan_rx_add_vid);
      if (!dev->vlan_rx_add_vid) {
         VMKLNX_DEBUG(0, "%s: no vlan_rx_add_vid handler", dev->name);
         goto error;
      }

      VMKLNX_DEBUG(1, "%s: adding vlan id %d", dev->name, (int)vid);
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_add_vid, dev, vid);
   }

   cna->fcoe_vlan_id = vid;

   /*
    * Valid VLAN ID ranges from 1 to 4095.
    * If a non-zero VLAN ID has been obtained,
    * which indicates VLAN discovery completeion,
    * call the L2 driver provided callback.
    */
   if (vid != 0) {
      int status;
      struct vmklnx_fcoe_contlr *contlr;

      contlr = FCoEGetVmklnxContlr(dev->name);
      if (contlr && contlr->fcoe->fcoe_vlan_disc_cmpl) {
         VMKAPI_MODULE_CALL(contlr->fcoe->module_id,
                      status,
                      contlr->fcoe->fcoe_vlan_disc_cmpl,
                      dev);
         if (status) {
            VMKLNX_DEBUG(0, "%s: fcoe_vlan_disc_cmpl callback failure",
                         dev->name);
         }
      }
   }

   return 0;

error:
   return -1;
}
EXPORT_SYMBOL(vmklnx_cna_set_vlan_tag);

/**
 *  vmklnx_scsi_attach_cna -
 *  @sh: Pointer to shost structure used by scsi_add_host
 *  @netdev: Pointer to netdev which is the parent for this adapter
 *
 *  The API is to be used by pseudo drivers to set PAE capability.
 *  This is also used to add list of vn ports in case of FCoE
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_attach_cna */
void
vmklnx_scsi_attach_cna(struct Scsi_Host *sh, struct net_device *netdev)
{
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_contlr *contlr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);
   VMK_ASSERT(netdev);

   ((struct vmklnx_ScsiAdapter *)sh->adapter)->vmkAdapter->paeCapable =
      LinuxPCI_DeviceIsPAECapable(netdev->pdev);

   /*
    * Check if this is a FCoE transport. If iscsi or so return
    */
   if (sh->xportFlags != VMKLNX_SCSI_TRANSPORT_TYPE_FCOE) {
      return;
   }

   cna = (struct vmklnx_cna *)netdev->cna_ops;

   if (!cna) {
      return;
   }
   contlr = (struct vmklnx_fcoe_contlr *)cna->handle;

   atomic_inc(&contlr->vnports);

   return;
}
EXPORT_SYMBOL(vmklnx_scsi_attach_cna);

/**
 *  vmklnx_scsi_remove_cna -
 *  @sh: Pointer to shost structure used by scsi_add_host
 *  @netdev: Pointer to netdev which is the parent for this adapter
 *
 *  Removes any association between CNA and scsi_host
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux. Should be called before
 * scsi_remove_host
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_remove_cna */
void
vmklnx_scsi_remove_cna(struct Scsi_Host *sh, struct net_device *netdev)
{
   struct vmklnx_cna *cna;
   struct vmklnx_fcoe_contlr *contlr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);
   VMK_ASSERT(netdev);

   if (sh->xportFlags != VMKLNX_SCSI_TRANSPORT_TYPE_FCOE) {
      return;
   }

   cna = (struct vmklnx_cna *)netdev->cna_ops;

   if (!cna) {
      return;
   }
   contlr = (struct vmklnx_fcoe_contlr *)cna->handle;

   atomic_dec(&contlr->vnports);

   return;
}
EXPORT_SYMBOL(vmklnx_scsi_remove_cna);

/**
 *  vmklnx_cna_set_macaddr -
 *  @netdev: Pointer to netdev for which mac address needs to be set
 *  @mac_addr: Pointer to mac address
 *
 *  Sets given MAC address
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux. This would be called to set
 * appropriate MAC address for a given CNA
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_cna_set_macaddr */
int
vmklnx_cna_set_macaddr(struct net_device *netdev, unsigned char *mac_addr)
{
   unsigned long flags;
   struct vmklnx_cna *cna;
   int findex, qindex, ret = -1;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(netdev);
   cna = (struct vmklnx_cna *)netdev->cna_ops;

   if (!cna) {
      VMK_ASSERT(cna);
      return -1;
   }

   spin_lock_irqsave(&cna->lock, flags);
   for (qindex = 0; qindex < CNA_MAX_QUEUE; ++qindex) {
      for (findex = 0; findex < CNA_MAX_FILTER_PER_QUEUE; ++findex) {
         if (memcmp(mac_addr, cna->rqueue[qindex].filter[findex].mac_addr,
                    6) == 0) {
            /*
             * We found a match. Increment the usage count on this
             */
            ++cna->rqueue[qindex].filter[findex].ref;

            /*
             * If we brought ref count to 1, something is wrong.
             */
            VMK_ASSERT(cna->rqueue[qindex].filter[findex].ref > 1);
            
            ret = 0;
         }
      }
   }

   if (ret) {
      /*
       * No match was found. So add a new filter
       */
      if (CNASetRxFilter(cna, mac_addr) == VMK_OK) {
         ret = 0;
      }
   }
   spin_unlock_irqrestore(&cna->lock, flags);

   return ret;
}
EXPORT_SYMBOL(vmklnx_cna_set_macaddr);

/**
 *  vmklnx_cna_remove_macaddr -
 *  @netdev: Pointer to netdev for which mac address needs to be removed
 *  @mac_address: Pointer to mac address
 *
 *  Removes given MAC address
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux. This would be called to remove
 * appropriate MAC address for a given CNA
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_cna_remove_macaddr */
int
vmklnx_cna_remove_macaddr(struct net_device *netdev, unsigned char *mac_address)
{
   unsigned long flags;
   struct vmklnx_cna *cna;
   int findex, qindex, ret = -1;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(netdev);
   cna = (struct vmklnx_cna *)netdev->cna_ops;

   if (!cna) {
      VMK_ASSERT(cna);
      return -1;
   }

   spin_lock_irqsave(&cna->lock, flags);
   for (qindex = 0; qindex < CNA_MAX_QUEUE; ++qindex) {
      for (findex = 0; findex < CNA_MAX_FILTER_PER_QUEUE; ++findex) {
         if (memcmp(mac_address, cna->rqueue[qindex].filter[findex].mac_addr,
                    6) == 0) {
            /*
             * We found a match.
             */
            ret = 0;
            if (--cna->rqueue[qindex].filter[findex].ref == 0) {
               /*
                * We decremented our reference. So lets break out from this
                */
               CNARemoveRxFilter(cna, qindex, findex);

               /*
                * Clear out the mac address field so this entry is not
                * erroneously reused via vmklnx_cna_set_macaddr.
                */
               memset(cna->rqueue[qindex].filter[findex].mac_addr, 0, 6);
            }
            goto remove_mac_addr_end;
         }
      }
   }
remove_mac_addr_end:
   spin_unlock_irqrestore(&cna->lock, flags);
   return ret;
}
EXPORT_SYMBOL(vmklnx_cna_remove_macaddr);

/**
 *  vmklnx_cna_lookup_by_name -
 *  @vmnic_name: Name of the NIC.
 *
 *  Lookup the CNA structure for a given vmnic name.
 *
 *  Results:
 *  	 Return the pointer the vmklinux CNA structure.
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_cna_lookup_by_name */
struct vmklnx_cna *
vmklnx_cna_lookup_by_name(char *vmnic_name)
{
   struct vmklnx_cna *cna;
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkFlag = vmk_SPLockIRQ(&linuxCNAAdapterLock);
   cna = __cna_lookup_by_name(vmnic_name);
   vmk_SPUnlockIRQ(&linuxCNAAdapterLock, vmkFlag);

   return cna;
}
EXPORT_SYMBOL(vmklnx_cna_lookup_by_name);

/**
 *  vmklnx_cna_is_dcb_hw_assist -
 *  @vmnic_name: Name of the NIC.
 *
 *  Checked if the DCB negotiation is done in
 *  the adapter firmware.
 *
 *  Results:
 *  	 VMK_TRUE if DCB negotiation is done by the adapter firmware
 *  	 VMK_FALSE otherwise
 *
 *  ESX Deviation Notes:
 *  This API is not present in Linux.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_cna_is_dcb_hw_assist */
vmk_Bool
vmklnx_cna_is_dcb_hw_assist(char *vmnic_name)
{
   struct vmklnx_cna *cna;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   cna = vmklnx_cna_lookup_by_name(vmnic_name);

   if (cna && (cna->netdev->features & NETIF_F_HWDCB)) {
      return VMK_TRUE;
   }

   return VMK_FALSE;
}
EXPORT_SYMBOL(vmklnx_cna_is_dcb_hw_assist);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_cna_update_fcoe_priority -
 *
 *	Update the fcoe priority in the CNA layer based on the DCBX
 *      negotiated value.
 *
 * Results:
 *      0 on success
 *
 * Side Effects:
 *      None
 *----------------------------------------------------------------------
 */
int
vmklnx_cna_update_fcoe_priority(char *vmnic_name,
                                vmk_uint8 priority_bitmap)
{
   struct vmklnx_cna *cna;
   vmk_VlanPriority priority = 0;

   /*
    * Return if no FCOE priority has been set.
    */
   if (priority_bitmap == 0) {
      return 0;
   }

   cna = vmklnx_cna_lookup_by_name(vmnic_name);
   if (!cna) {
      VMKLNX_DEBUG(0, "Cannot lookup backing cna netdev for %s",
                   vmnic_name);
      return -1;
   }

   /*
    * The FCOE priority passed in is in the form of bitmap.
    * Convert it to the decimal value.
    */
   while (priority < 8) {
      if (priority_bitmap & (0x1 << priority)) {
          break;
      }
      priority++;
   }
   VMK_ASSERT(priority < 8);
 
   if (cna->fcoe_app_prio != priority) {
      VMKLNX_DEBUG(2, "Updated %s FCOE priority in CNA %u->%u",
         vmnic_name, cna->fcoe_app_prio, priority);
      cna->fcoe_app_prio = priority;
   }

   return 0;
}

/*
 * Helper to lookup cna - Callers need to hold lock
 */
static struct vmklnx_cna *
__cna_lookup_by_name(char * vmnic_name)
{
   struct vmklnx_cna *cna;

   list_for_each_entry(cna, &linuxCNAAdapterList, entry) {
      if (!strncmp(vmnic_name, cna->netdev->name, CNA_DRIVER_NAME)) {
         VMK_ASSERT(cna->netdev->features & NETIF_F_CNA);
         return cna;
      }
   }
   return NULL;
}

/*
 * Helper function to lookup fcoe template given pcidev
 * Callers must hold linuxFCoEClientLock before calling this fn
 */
static struct vmklnx_fcoe_template *
fcoe_lookup_by_pcitbl(struct pci_dev *pdev)
{
   struct vmklnx_fcoe_template *fcoe;

   list_for_each_entry(fcoe, &linuxFCoEClientList, entry) {
      if (fcoe->id_table && (pci_match_device(fcoe->id_table, pdev))) {
         return fcoe;
      }
   }
   return NULL;
}
