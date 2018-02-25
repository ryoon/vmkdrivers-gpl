/*
 * ****************************************************************
 * Portions Copyright 2009-2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/


/*
 * linux_dcb.c --
 *
 *      vmklinux DCB utility functions
 */
#include <linux/netdevice.h>
#include <linux/dcbnl.h>
#include <net/dcbnl.h>

#include "vmkapi.h"
#include "cna_ioctl.h"
#include "linux_dcb.h"
#include "linux_net.h"

#define VMKLNX_LOG_HANDLE LinDCB
#include "vmklinux_log.h"

/*
 *----------------------------------------------------------------------
 *
 * LinuxDCB_Init
 *
 *      Initialize DCB subsystem.  Logging?
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Initializes logging functionalities.
 *
 *----------------------------------------------------------------------
 */
void LinuxDCB_Init(void)
{
   VMKLNX_CREATE_LOG();
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxDCB_Cleanup
 *
 *      Clean up DCB subsystem.  
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Unregister DCB logging functionalities.
 *
 *----------------------------------------------------------------------
 */
void LinuxDCB_Cleanup(void)
{
   VMKLNX_DESTROY_LOG();
}

/*
 *----------------------------------------------------------------------
 *
 * DCBLinuxGetNetDevice
 *
 *      Helper function to get the net_dev for ioctl's
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Increments the net_dev reference count
 *
 *----------------------------------------------------------------------
 */
static struct net_device *
DCBLinuxGetNetDevice(struct fcoe_ioctl_pkt *dcb_ioctl)
{
   struct net_device *netdev = NULL;

   /*
    * Initialize the status to success
    */
   dcb_ioctl->status = DCB_IOCTL_SUCCESS;

   netdev = dev_get_by_name(dcb_ioctl->data);
   if (!netdev) {
      dcb_ioctl->status = DCB_IOCTL_NO_CONTROLLER_FAILURE;
      VMKLNX_DEBUG(2, "IOCTL 0x%x error: DCB_IOCTL_NO_CONTROLLER_FAILURE",
                   dcb_ioctl->cmd);
   }

   return netdev;
}

/*
 *----------------------------------------------------------------------
 *
 * CNAProcessDCBRequests
 *
 *      Main entry point for ioctl-driven DCB control path.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Dispatch given ioctl to ioctl handler; driver down-calls made
 *      accordingly.
 *
 * Note:
 *	ioctl for DCB has been replaced by VSI as of today. This code
 *	path is kept in case we need to add any ioctl for DCB operations
 *	in the future (e.g. due to VSI limitations)
 *
 *----------------------------------------------------------------------
 */
void CNAProcessDCBRequests(struct fcoe_ioctl_pkt *dcb_ioctl)
{
   struct net_device *netdev;

   VMK_ASSERT(dcb_ioctl);

   if (!(netdev = DCBLinuxGetNetDevice(dcb_ioctl))) {
      VMKLNX_DEBUG(0, "Invalid controller %s", dcb_ioctl->data);
      return;
   }

   rtnl_lock();

   switch (dcb_ioctl->cmd) {
      default:
         VMKLNX_DEBUG(2, "Invalid DCB request %d", dcb_ioctl->cmd);
         VMK_ASSERT(0);
   }

   rtnl_unlock();

   dcb_ioctl->cmd = DCB_IOCTL_RESPONSE_CODE | dcb_ioctl->cmd;
   dev_put(netdev);

   return;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBIsEnabled --
 *
 *      Get the DCB state of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBIsEnabled(void *device, vmk_Bool *state,  vmk_DCBVersion *version)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   u8 mode = 0;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getstate: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "getstate: %s", netdev->name);
   VMKAPI_MODULE_CALL(netdev->module_id, *((vmk_uint8 *)state),
                      netdev->dcbnl_ops->getstate, netdev);
   rtnl_unlock();


   if (linDev->flags & NET_VMKLNX_IEEE_DCB) {
      if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getdcbx) {
         VMKLNX_DEBUG(9, "getdcbx not defined on %s", netdev->name);
         return VMK_NOT_SUPPORTED;
      }

      rtnl_lock();

      VMKLNX_DEBUG(9, "getdcbx: %s", netdev->name);
      VMKAPI_MODULE_CALL(netdev->module_id, mode,
                         netdev->dcbnl_ops->getdcbx, netdev);
      rtnl_unlock();
   }

   if (mode & DCB_CAP_DCBX_VER_IEEE) {
      version->majorVersion = 2;
      version->minorVersion = 0;
   } else if (mode & DCB_CAP_DCBX_VER_CEE) {
      version->majorVersion = 1;
      version->minorVersion = 2;
   } else {
      version->majorVersion = 1;
      version->minorVersion = 0;
   }

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBEnable --
 *
 *      Enable DCB support on the device.
 *
 * Results:
 *      VMK_OK if DCB support enable succeeds.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      DCB support enabled on the device if succeeds.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBEnable(void *device)
{
   vmk_uint8 ret;
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setstate: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "setstate: %s, state: 1", netdev->name);
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->setstate, netdev, 1);

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBDisable --
 *
 *      Set the DCB state of the device.
 *
 * Results:
 *      VMK_OK if DCB support disable succeeds.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      DCB support disabled on the device if succeeds.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBDisable(void *device)
{
   vmk_uint8 ret;
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setstate: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "setstate: %s, state: 0", netdev->name);
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->setstate, netdev, 0);

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetNumTCs --
 *
 *      Get traffic class information for a NIC.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetNumTCs(void *device, vmk_DCBNumTCs *tcsInfo)
{
   struct net_device *netdev = (struct net_device *) device;
   int i, ret = 0;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getnumtcs) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getnumtcs: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   for (i = DCB_NUMTCS_ATTR_ALL + 1; i <= DCB_NUMTCS_ATTR_MAX; i++) {
      switch (i) {
         case DCB_NUMTCS_ATTR_PG:
            VMKLNX_DEBUG(9, "getnumtcs: %s, PG", netdev->name);
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getnumtcs, netdev, i,
                               &(tcsInfo->pgTcs));
            break;
         case DCB_NUMTCS_ATTR_PFC:
            VMKLNX_DEBUG(9, "getnumtcs: %s, PFC", netdev->name);
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getnumtcs, netdev, i,
                               &(tcsInfo->pfcTcs));
            break;
         default:
            VMK_ASSERT(0);
      }
      if (ret) {
         VMKLNX_DEBUG(0, "getnumtcs: failed 0x%x", i);
         break;
      }
   }

   rtnl_unlock();

   return (ret ? VMK_FAILURE : VMK_OK);
}

/** Maximum number of Traffic Classes */
#define VMK_DCB_MAX_TC_COUNT                8

typedef struct {

   /* Number of TCs supported for the PG. */
   vmk_uint8 num_tcs;

   /* Maps PGID : Link Bandwidth %. */
   vmk_uint8 pg_bw_percent[VMK_DCB_MAX_PG_COUNT];

   /**
    * Maps TC : UP bitmap.
    * A set bit X in tc_to_up_map[Y] indicates that UP X belongs to TC Y.
    */
   vmk_uint8 tc_to_up_map[VMK_DCB_MAX_TC_COUNT];

   /**
    * Maps TC : PGID.
    * tc_to_pgid_map[X] = Y indicates that PGID Y maps to TC X.
    */
   vmk_uint8 tc_to_pgid_map[VMK_DCB_MAX_TC_COUNT];

   /**
    * Maps TC : Sub-PG Bandwidth %.
    * This param is not used on wire.
    */
   vmk_uint8 tc_sub_pg_bw_percent[VMK_DCB_MAX_TC_COUNT];

   /**
    * Maps TC : Link priority type.
    * This param is not used on wire.
    */
   vmk_uint8 tc_to_link_prio_type[VMK_DCB_MAX_TC_COUNT];

} vmklnx_dcb_pg;

/*
 *-----------------------------------------------------------------------------
 *
 * linpg_to_vmkpg --
 *
 *      Helper to convert vmklinux PG to vmkernel PG.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static void
linpg_to_vmkpg(vmklnx_dcb_pg *linpg, vmk_DCBPriorityGroup *vmkpg)
{
   int i, j;

   /*
    * copy pg configuration to vmkpg
    */
   for (i = 0; i < VMK_DCB_MAX_PG_COUNT; i++) {
      vmkpg->pgBwPercent[i] = linpg->pg_bw_percent[i];
      VMK_ASSERT(vmkpg->pgBwPercent[i] <= 100);
   }

   /*
    * Setup up_to_pgid_map based on tc_to_up_map and tc_to_pgid_map
    */
   for (i = 0; i < VMK_DCB_MAX_TC_COUNT; i++) {
      if (linpg->tc_to_up_map[i] != 0) {
         for (j = 0; j < VMK_DCB_MAX_TC_COUNT; j++) {
            if (linpg->tc_to_up_map[i] & (1 << j))
               /* UP j is mapped to TC i */
               vmkpg->upToPgIDMap[j] = linpg->tc_to_pgid_map[i];
         }
      }
   }

   return;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetPriorityGroup --
 *
 *      Get the PG configuration of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetPriorityGroup(void *device, vmk_DCBPriorityGroup *vmkpg)
{
   struct net_device *netdev = (struct net_device *) device;
   vmklnx_dcb_pg linpg, *pg = &linpg;
   vmk_DCBNumTCs num_tcs;
   int tc;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops ||
       !netdev->dcbnl_ops->getpgtccfgtx ||
       !netdev->dcbnl_ops->getpgbwgcfgtx ||
       !netdev->dcbnl_ops->getpgtccfgrx ||
       !netdev->dcbnl_ops->getpgbwgcfgrx) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getpg: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   memset(pg, 0, sizeof(vmklnx_dcb_pg));

   /* get number of traffic classes supported first */
   if (NICDCBGetNumTCs(device, (void *)(&num_tcs)) != VMK_OK) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getpg: %s failed to get num_tcs",
                   netdev->name);
      return VMK_FAILURE;
   }

   pg->num_tcs = num_tcs.pgTcs;

   rtnl_lock();

   for (tc = 0; tc < pg->num_tcs; tc++) {

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->getpgtccfgtx,
                              netdev,
                              tc,
                              &(pg->tc_to_link_prio_type[tc]),
                              &(pg->tc_to_pgid_map[tc]),
                              &(pg->tc_sub_pg_bw_percent[tc]),
                              &(pg->tc_to_up_map[tc]));

      VMKLNX_DEBUG(9, "getpgtccfgtx: %s, tc: %d, priotype: %d, bwg_id: %d, "
                      "bw_pct: %d up_map 0x%x",
                      netdev->name,
                      tc,
                      pg->tc_to_link_prio_type[tc],
                      pg->tc_to_pgid_map[tc],
                      pg->tc_sub_pg_bw_percent[tc],
                      pg->tc_to_up_map[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->getpgtccfgrx,
                              netdev,
                              tc,
                              &(pg->tc_to_link_prio_type[tc]),
                              &(pg->tc_to_pgid_map[tc]),
                              &(pg->tc_sub_pg_bw_percent[tc]),
                              &(pg->tc_to_up_map[tc]));

      VMKLNX_DEBUG(9, "getpgtccfgrx: %s, tc: %d, priotype: %d, bwg_id: %d, "
                      "bw_pct: %d up_map 0x%x",
                      netdev->name,
                      tc,
                      pg->tc_to_link_prio_type[tc],
                      pg->tc_to_pgid_map[tc],
                      pg->tc_sub_pg_bw_percent[tc],
                      pg->tc_to_up_map[tc]);
   }

   /*
    * Get Priority Group bandwidth allotment.
    */
   for (tc = 0; tc < pg->num_tcs; tc++) {

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->getpgbwgcfgtx,
                              netdev, tc, &(pg->pg_bw_percent[tc]));

      VMKLNX_DEBUG(9, "getpgbwgcfgtx: %s, pg: %d, pct: %d",
                      netdev->name, tc, pg->pg_bw_percent[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->getpgbwgcfgrx,
                              netdev, tc, &(pg->pg_bw_percent[tc]));

      VMKLNX_DEBUG(9, "getpgbwgcfgrx: %s, pg: %d, pct: %d",
                      netdev->name, tc, pg->pg_bw_percent[tc]);
   }

   rtnl_unlock();

   /* Store the PG data to vmkernel PG */
   linpg_to_vmkpg(pg, vmkpg);

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmkpg_to_linpg --
 *
 *      Helper to convert vmkernel PG to vmklinux PG.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
vmkpg_to_linpg(vmk_DCBPriorityGroup *vmkpg, vmklnx_dcb_pg *linpg)
{
   vmk_uint8 pgid;
   int i, up, tc;

   for (i = 0; i < VMK_DCB_MAX_PG_COUNT; i++) {
      VMK_ASSERT(vmkpg->pgBwPercent[i] <= 100);
      linpg->pg_bw_percent[i] = vmkpg->pgBwPercent[i];
   }

   /*
    * Setup tc_to_pgid_map.
    * Assume 1:1 map (pg0:tc0, pg1:tc1, ..., pg7:tc7)
    */
   for (tc = 0; tc < VMK_DCB_MAX_TC_COUNT; tc++) {
      linpg->tc_to_pgid_map[tc] = tc;
   }

   /*
    * Setup tc_to_up_map based on up_to_pgid_map
    */
   for (up = 0; up < VMK_DCB_MAX_PRIORITY_COUNT; up++) {
      /* Only tc(0) to tc(linpg->num_tcs - 1) can have UP mapped */
      if(vmkpg->upToPgIDMap[up] >= linpg->num_tcs) {
         /*
	  * More PGs configured than the number of TCs driver reported
	  * that it supports. Just map the UP in this PG to TC0 and let
	  * driver to handle it.
	  */
         VMKLNX_DEBUG(9, "linux_dcb.c: setpg: up_to_pgid_map incorrect- "
	             "UP: %x PGID: %x Num. of TCs supported: %x "
		     "This UP will be ignored",
		     up, vmkpg->upToPgIDMap[up], linpg->num_tcs);
         linpg->tc_to_up_map[0] |= 1 << up;
      } else {
         pgid = vmkpg->upToPgIDMap[up];
         linpg->tc_to_up_map[pgid] |= 1 << up;
      }
   }

   /*
    * Setup tc_to_link_prio_type. Since it's not used on wire,
    * set it to DCB_ATTR_VALUE_UNDEFINED
    */
   for (tc = 0; tc < VMK_DCB_MAX_TC_COUNT; tc++) {
      linpg->tc_to_link_prio_type[tc] = DCB_ATTR_VALUE_UNDEFINED;
   }

   /*
    * Setup tc_sub_pg_bw_percent
    * Since TC:PG map is 1:1, sub_pg_bw will always be 100%
    */
   for (tc = 0; tc < VMK_DCB_MAX_TC_COUNT; tc++) {
         linpg->tc_sub_pg_bw_percent[tc] = 100;
   }

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetPriorityGroup --
 *
 *      Set the PG configuration of the device.
 *
 * Results:
 *      VMK_OK if PG configuration set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      Driver down-calls for traffic class and bandwidth group
 *      configuration are invoked, resulting in configuring the
 *      Priority Group DCBX feature.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetPriorityGroup(void *device, vmk_DCBPriorityGroup *vmkpg)
{
   struct net_device *netdev = (struct net_device *) device;
   vmklnx_dcb_pg linpg, *pg = &linpg;
   vmk_DCBNumTCs num_tcs;
   int tc;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops ||
       !netdev->dcbnl_ops->setpgtccfgtx ||
       !netdev->dcbnl_ops->setpgbwgcfgtx ||
       !netdev->dcbnl_ops->setpgtccfgrx ||
       !netdev->dcbnl_ops->setpgbwgcfgrx) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setpg: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   memset(pg, 0, sizeof(vmklnx_dcb_pg));

   /* get number of traffic classes supported first */
   if (NICDCBGetNumTCs(device, (void *)(&num_tcs)) != VMK_OK) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setpg: %s failed to get num_tcs",
                   netdev->name);
      return VMK_FAILURE;
   }

   pg->num_tcs = num_tcs.pgTcs;

   /* Convert vmkernel PG to vmklinux PG */
   if (vmkpg_to_linpg(vmkpg, pg) != VMK_OK) {
      return VMK_FAILURE;
   }

   rtnl_lock();

   /*
    * For each Traffic Class, push down:
    *
    *    Link priority type (group/link-strict, or none),
    *    PGID for the PG to which the Traffic Class belongs,
    *    Sub-PG TC bandwidth allotment,
    *    UP bitmap (maps what user priorities belong to the TC).
    *
    * Note that notions of priority types (link-strict / group-strict)
    * and sub-PG bandwidth divvy-ing are gone in DCBX 1.01.  But we
    * use interfaces which still expect this information.
    *
    * PG settings for Rx and Tx are forced to be the same
    */
   for (tc = 0; tc < pg->num_tcs; tc++) {

      VMKLNX_DEBUG(9, "setpgtccfg: %s, tc: %d, priotype: %d, bwg_id: %d, "
                      "bw_pct: %d up_map 0x%x",
                      netdev->name,
                      tc,
                      pg->tc_to_link_prio_type[tc],
                      pg->tc_to_pgid_map[tc],
                      pg->tc_sub_pg_bw_percent[tc],
                      pg->tc_to_up_map[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->setpgtccfgtx,
                              netdev,
                              tc,
                              pg->tc_to_link_prio_type[tc],
                              pg->tc_to_pgid_map[tc],
                              pg->tc_sub_pg_bw_percent[tc],
                              pg->tc_to_up_map[tc]);

      VMKLNX_DEBUG(9, "setpgtccfgrx: %s, tc: %d, priotype: %d, bwg_id: %d, "
                      "bw_pct: %d up_map 0x%x",
                      netdev->name,
                      tc,
                      pg->tc_to_link_prio_type[tc],
                      pg->tc_to_pgid_map[tc],
                      pg->tc_sub_pg_bw_percent[tc],
                      pg->tc_to_up_map[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->setpgtccfgrx,
                              netdev,
                              tc,
                              pg->tc_to_link_prio_type[tc],
                              pg->tc_to_pgid_map[tc],
                              pg->tc_sub_pg_bw_percent[tc],
                              pg->tc_to_up_map[tc]);
   }

   /*
    * Push down Priority Group bandwidth allotment.
    */
   for (tc = 0; tc < pg->num_tcs; tc++) {

      VMKLNX_DEBUG(9, "setpgbwgcfgtx: %s, pg: %d, pct: %d",
                      netdev->name, tc, pg->pg_bw_percent[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->setpgbwgcfgtx,
                              netdev, tc, pg->pg_bw_percent[tc]);

      VMKLNX_DEBUG(9, "setpgbwgcfgrx: %s, pg: %d, pct: %d",
                      netdev->name, tc, pg->pg_bw_percent[tc]);

      VMKAPI_MODULE_CALL_VOID(netdev->module_id,
                              netdev->dcbnl_ops->setpgbwgcfgrx,
                              netdev, tc, pg->pg_bw_percent[tc]);
   }

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetPFCCfg --
 *
 *      Get the PFC configuration of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetPFCCfg(void *device, vmk_DCBPriorityFlowControlCfg *cfg)
{
   struct net_device *netdev = (struct net_device *) device;
   int i;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getpfccfg) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getpfccfg: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   /*
    * Toggle whether PFC is enabled for each priority class.
    */
   for (i = 0; i < VMK_DCB_MAX_PRIORITY_COUNT; i++) {
      VMKLNX_DEBUG(9, "getpfcfg: %s, priority: %d", netdev->name, i);
      VMKAPI_MODULE_CALL_VOID(netdev->module_id, netdev->dcbnl_ops->getpfccfg,
                              netdev, i, &(cfg->pfcEnable[i]));
   }

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetPFCCfg --
 *
 *      Set the PFC configuration of the device.
 *
 * Results:
 *      VMK_OK if PFC configuration set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      PFC configuration of the device is set.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetPFCCfg(void *device, vmk_DCBPriorityFlowControlCfg *cfg)
{
   struct net_device *netdev = (struct net_device *) device;
   int i;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setpfccfg) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setpfccfg: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   /*
    * Toggle whether PFC is enabled for each priority class.
    */
   for (i = 0; i < VMK_DCB_MAX_PRIORITY_COUNT; i++) {
      VMKLNX_DEBUG(9, "setpfcfg: %s, priority: %d, enable: %d",
                      netdev->name, i, cfg->pfcEnable[i]);
      VMKAPI_MODULE_CALL_VOID(netdev->module_id, netdev->dcbnl_ops->setpfccfg,
                              netdev, i, cfg->pfcEnable[i]);
   }

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBIsPFCEnabled --
 *
 *      Get the PFC state of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBIsPFCEnabled(void *device, vmk_Bool *state)
{
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getpfcstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getpfcstate: %s not defined",
                   netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "getpfcstate: %s", netdev->name);
   VMKAPI_MODULE_CALL(netdev->module_id, *((vmk_uint8 *)state),
                      netdev->dcbnl_ops->getpfcstate, netdev);

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBEnablePFC --
 *
 *      Enable PFC support on the device.
 *
 * Results:
 *      VMK_OK if PFC support enable succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      PFC support enabled on the device.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBEnablePFC(void *device)
{
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setpfcstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setpfcstate: %s not defined",
                   netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "setpfcstate: %s, state: 1", netdev->name);
   VMKAPI_MODULE_CALL_VOID(netdev->module_id, netdev->dcbnl_ops->setpfcstate,
                           netdev, 1);

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBDisablePFC --
 *
 *      Disable PFC on the device.
 *
 * Results:
 *      VMK_OK if PFC support disable succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      PFC support disabled on the device.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBDisablePFC(void *device)
{
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setpfcstate) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setpfcstate: %s not defined",
                   netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "setpfcstate: %s, state: 0", netdev->name);
   VMKAPI_MODULE_CALL_VOID(netdev->module_id, netdev->dcbnl_ops->setpfcstate,
                           netdev, 0);

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetApplications --
 *
 *      Get all available DCB Application setting of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetApplications(void *device, vmk_DCBApplications *apps)
{
   struct net_device *netdev = (struct net_device *) device;
   int i;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getapp) {
      VMKLNX_DEBUG(9, "linux_dcb.c: app_getall: %s not defined",
                   netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   for (i = 0; i < VMK_DCB_MAX_APP_COUNT; i++) {
      if (!apps->app[i].enabled)
         continue;

      VMKLNX_DEBUG(9, "getapp: %s index %d sf %x protoID %x",
                   netdev->name, i, apps->app[i].sf, apps->app[i].protoID);

      VMKAPI_MODULE_CALL(netdev->module_id, apps->app[i].priority,
                         netdev->dcbnl_ops->getapp, netdev,
                         (vmk_uint8)(apps->app[i].sf), apps->app[i].protoID);
   }

   rtnl_unlock();

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetApplication --
 *
 *      Set the DCB Application of the device.
 *
 * Results:
 *      VMK_OK if DCB Application set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      DCB Application configurations of the device is set.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetApplication(void *device, vmk_DCBApplication *app)
{
   struct net_device *netdev = (struct net_device *) device;
   u8 ret;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setapp) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setapp: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKLNX_DEBUG(9, "setapp: %p app sf %d protoID %x priority %d ",
                netdev->name, app->sf, app->protoID, app->priority);
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->setapp, netdev,
                      (vmk_uint8)app->sf, app->protoID, app->priority);

   rtnl_unlock();

   if (ret == 0) {
      vmklnx_cna_update_fcoe_priority(netdev->name, app->priority);
      app->enabled = 1;
      return VMK_OK;
   } else {
      app->enabled = 0;
      return VMK_FAILURE;
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetCapabilities --
 *
 *      Get DCB capabilities for a NIC.
 *
 * Results:
 *      VMK_OK if DCB Application set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetCapabilities(void *device, vmk_DCBCapabilities *caps)
{
   struct net_device *netdev = (struct net_device *) device;
   int i, ret = 0;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getcap) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getcap: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   for (i = DCB_CAP_ATTR_ALL + 1; i <= DCB_CAP_ATTR_MAX; i++) {
      switch (i) {
         case DCB_CAP_ATTR_PG:
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getcap, netdev, i,
                               (vmk_uint8 *)(&caps->priorityGroup));
            break;
         case DCB_CAP_ATTR_PFC:
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getcap, netdev, i,
                               (vmk_uint8 *)(&caps->priorityFlowControl));
            break;
         case DCB_CAP_ATTR_UP2TC:
            VMKLNX_DEBUG(9, "linux_dcb.c: getcap: up2tc map not supported");
            break;
         case DCB_CAP_ATTR_PG_TCS:
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getcap, netdev, i,
                               &caps->pgTrafficClasses);
            break;
         case DCB_CAP_ATTR_PFC_TCS:
            VMKAPI_MODULE_CALL(netdev->module_id, ret,
                               netdev->dcbnl_ops->getcap, netdev, i,
                               &caps->pfcTrafficClasses);
            break;
         /*
          * PR485630 Should remove all GSP & BCN Hooks (old DCBX stuff)
          */
         case DCB_CAP_ATTR_GSP:
            VMKLNX_DEBUG(9, "linux_dcb.c: getcap: gsp not supported");
            break;
         case DCB_CAP_ATTR_BCN:
            VMKLNX_DEBUG(9, "linux_dcb.c: getcap: bcn not supported");
            break;
	 case DCB_CAP_ATTR_DCBX:
	    VMKLNX_DEBUG(9, "linux_dcb.c: getdcbx: dcbx not supported");
            break;
         default:
            VMK_ASSERT(0);
      }
      if (ret) {
         VMKLNX_DEBUG(0, "getcap: failed: 0x%x", i);
         break;
      }
   }

   rtnl_unlock();

   VMKLNX_DEBUG(9, "get capabilities: %s\n", netdev->name);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBApplySettings --
 *
 *      Flush out all pending hardware configuration changes to the
 *      driver.
 *
 * Results:
 *      VMK_OK if flush succeeded and required device reset.
 *      VMK_IS_ENABLED if flush succeeded and no device reset was required.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      All pending HW configuration changes flushed out.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBApplySettings(void *device)
{
   vmk_int32 hwState = 0;
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setall) {
      VMKLNX_DEBUG(9, "linux_dcb.c: setall: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }
   /* 
    * PR 593650	
    * Update trans_start here so that netdev_watchdog will not mistake
    * a stopped tx_queue as a sign of pNIc hang during DCB config configuration.
    * The same is done in the SetMTU case for the same reason
    */ 
   rtnl_lock();
   netdev->trans_start = jiffies;

   VMKLNX_DEBUG(9, "setall: %s", netdev->name);

   VMKAPI_MODULE_CALL(netdev->module_id, hwState,
                      netdev->dcbnl_ops->setall, netdev);

   VMKLNX_DEBUG(9, "setall: %s; hwState: %d", netdev->name, hwState);

   rtnl_unlock();

   /*
    * A return value of non-zero for dcbnl_ops->setall
    * means that no device reset was necessary.
    */
   return (hwState ? VMK_IS_ENABLED : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetSettings --
 *
 *      Get All DCB settings for a NIC.
 *
 * Results:
 *      VMK_OK if operation succeeds.
 *      VMK_NOT_SUPPORTED if any callback is not defined by the driver.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetSettings(void *device, vmk_DCBConfig *dcb)
{
   struct net_device *netdev = (struct net_device *) device;

   VMK_ASSERT(netdev);

   if (!netdev->dcbnl_ops                  ||
       !netdev->dcbnl_ops->getstate        ||
       !netdev->dcbnl_ops->getnumtcs       ||
       !netdev->dcbnl_ops->getpgtccfgtx    ||
       !netdev->dcbnl_ops->getpgbwgcfgtx   ||
       !netdev->dcbnl_ops->getpgtccfgrx    ||
       !netdev->dcbnl_ops->getpgbwgcfgrx   ||
       !netdev->dcbnl_ops->getpfccfg       ||
       !netdev->dcbnl_ops->getpfcstate     ||
       !netdev->dcbnl_ops->getapp          ||
       !netdev->dcbnl_ops->getcap
       ) {
      VMKLNX_DEBUG(9, "linux_dcb.c: getsettings: %s not defined", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (NICDCBIsEnabled(device, &dcb->dcbEnabled, &dcb->version) != VMK_OK) {
      VMKLNX_DEBUG(0, "getsettings: failed");
      return VMK_FAILURE;
   }
   if (!dcb->dcbEnabled) {
      /* No need to retrieve other DCB settings if DCB is not enabled */
      return VMK_OK;
   }

   if ((NICDCBGetNumTCs(device, &dcb->numTCs) != VMK_OK)                    ||
       (NICDCBGetPriorityGroup(device, &dcb->priorityGroup) != VMK_OK)      ||
       (NICDCBGetPFCCfg(device, &dcb->pfcCfg) != VMK_OK)                    ||
       (NICDCBIsPFCEnabled(device, &dcb->pfcEnabled) != VMK_OK)             ||
       (NICDCBGetApplications(device, &dcb->apps) != VMK_OK)                ||
       (NICDCBGetCapabilities(device, &dcb->caps) != VMK_OK)){
      VMKLNX_DEBUG(0, "getsettings: failed");
      return VMK_FAILURE;
   }

   VMKLNX_DEBUG(9, "getsettings: %s\n", netdev->name);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * NICDCBGetDcbxMode --
 *
 *	Get DCBx mode fom the NIC.
 *
 * Results:
 *	VMK_OK on success.
 *	VMK_NOT_SUPPORTED if any callback is not defined by the driver
 *	VMK_FAILURE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetDcbxMode(void *device, vmk_DCBSubType *dcbx_mode)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   u8 mode;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->getdcbx) {
      VMKLNX_DEBUG(9, "getdcbx not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();

   VMKAPI_MODULE_CALL(netdev->module_id, mode,
                           netdev->dcbnl_ops->getdcbx, netdev);
   rtnl_unlock();

   /*
    * DCB negotiation should always use latest version
    * that is available. So always return the latest version
    * available in current device capability.
    */
   if (mode & DCB_CAP_DCBX_VER_IEEE) {
      *dcbx_mode = VMK_DCB_CAP_DCBX_SUBTYPE_IEEE;
   } else if (mode & DCB_CAP_DCBX_VER_CEE) {
      *dcbx_mode = VMK_DCB_CAP_DCBX_SUBTYPE_CEE;
   } else {
      *dcbx_mode = VMK_DCB_CAP_DCBX_SUBTYPE_PRE_CEE;
   }

   VMKLNX_DEBUG(9, "got dcbx cap:%d from:%s\n", mode, netdev->name);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * NICDCBSetDcbxMode --
 *
 *	Set DCBx mode on the NIC.
 *
 * Results:
 *	VMK_OK on success.
 *	VMK_NOT_SUPPORTED if any callback is not defined by the driver
 *	VMK_FAILURE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetDcbxMode(void *device, vmk_DCBSubType dcbx_mode)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   int ret = 0;
   u8 mode = DCB_CAP_DCBX_HOST;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->setdcbx) {
      VMKLNX_DEBUG(9, "setdcbx not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (dcbx_mode == VMK_DCB_CAP_DCBX_SUBTYPE_IEEE) {
      mode |= DCB_CAP_DCBX_VER_IEEE;
   } else {
      mode |= DCB_CAP_DCBX_VER_CEE;
   }

   rtnl_lock();

   VMKAPI_MODULE_CALL(netdev->module_id, ret,
			   netdev->dcbnl_ops->setdcbx, netdev, mode);
   rtnl_unlock();

   VMKLNX_DEBUG(9, "set dcbx dcbx_mode:%d on %s mode:%d ret: %d\n",
                dcbx_mode, netdev->name, mode, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetIEEEEtsCfg --
 *
 *      Get the IEEE DCB ETS configuration of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetIEEEEtsCfg(void *device, vmk_DCBIEEEEtsCfg *etscfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct ieee_ets ets = {0};
   int ret = 0;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_getets) {
      VMKLNX_DEBUG(9, "ieee_getets not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                         netdev->dcbnl_ops->ieee_getets, netdev, &ets);
   rtnl_unlock();

   etscfg->etsWilling = ets.willing;
   etscfg->etsCap = ets.ets_cap;
   etscfg->etsCbs = ets.cbs;
   memcpy(etscfg->etsTcTxBw, ets.tc_tx_bw, sizeof(etscfg->etsTcTxBw));
   memcpy(etscfg->etsTcRxBw, ets.tc_rx_bw, sizeof(etscfg->etsTcRxBw));
   memcpy(etscfg->etsTcTsa, ets.tc_tsa, sizeof(etscfg->etsTcTsa));
   memcpy(etscfg->etsPrioTc, ets.prio_tc, sizeof(etscfg->etsPrioTc));
   memcpy(etscfg->etsTcRecoBw, ets.tc_reco_bw, sizeof(etscfg->etsTcRecoBw));
   memcpy(etscfg->etsTcRecoTsa, ets.tc_reco_tsa, sizeof(etscfg->etsTcRecoTsa));
   memcpy(etscfg->etsRecoPrioTc, ets.reco_prio_tc, sizeof(etscfg->etsRecoPrioTc));

   VMKLNX_DEBUG(9, "get IEEE ETS config:%s ret:%d \n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetIEEEEtsCfg --
 *
 *      Set the IEEE DCB ETS configuration on the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetIEEEEtsCfg(void *device, vmk_DCBIEEEEtsCfg *etscfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct ieee_ets ets = {0};
   int ret = 0;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_setets) {
      VMKLNX_DEBUG(9, "ieee_setets not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   ets.willing = etscfg->etsWilling;
   ets.ets_cap = etscfg->etsCap;
   ets.cbs = etscfg->etsCbs;
   memcpy(ets.tc_tx_bw, etscfg->etsTcTxBw, sizeof(ets.tc_tx_bw));
   memcpy(ets.tc_rx_bw, etscfg->etsTcRxBw, sizeof(ets.tc_rx_bw));
   memcpy(ets.tc_tsa, etscfg->etsTcTsa, sizeof(ets.tc_tsa));
   memcpy(ets.prio_tc, etscfg->etsPrioTc, sizeof(ets.prio_tc));
   memcpy(ets.tc_reco_bw, etscfg->etsTcRecoBw, sizeof(ets.tc_reco_bw));
   memcpy(ets.tc_reco_tsa, etscfg->etsTcRecoTsa, sizeof(ets.tc_reco_tsa));
   memcpy(ets.reco_prio_tc, etscfg->etsRecoPrioTc, sizeof(ets.reco_prio_tc));

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                         netdev->dcbnl_ops->ieee_setets, netdev, &ets);
   rtnl_unlock();

   VMKLNX_DEBUG(9, "set IEEE Ets config: %s ret: %d\n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetIEEEPfcCfg --
 *
 *      Get the IEEE DCB PFC configuration of the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetIEEEPfcCfg(void *device, vmk_DCBIEEEPfcCfg *pfccfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct ieee_pfc pfc = {0};
   int ret = 0, i;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_getpfc) {
      VMKLNX_DEBUG(9, "ieee_getpfc not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                         netdev->dcbnl_ops->ieee_getpfc, netdev, &pfc);
   rtnl_unlock();

   pfccfg->pfcCap = pfc.pfc_cap;
   pfccfg->pfcEnabled = pfc.pfc_en;
   pfccfg->pfcMbc = pfc.mbc;
   pfccfg->pfcDelay = pfc.delay;
   for (i = 0; i < VMK_DCB_IEEE_MAX_TCS; i++) {
      pfccfg->pfcReq[i] = pfc.requests[i];
      pfccfg->pfcInd[i] = pfc.indications[i];
   }

   VMKLNX_DEBUG(9, "get IEEE PFC config:%s ret:%d\n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetIEEEPfcCfg --
 *
 *      Set the IEEE DCB PFC configuration on the device.
 *
 * Results:
 *      VMK_OK if success.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetIEEEPfcCfg(void *device, vmk_DCBIEEEPfcCfg *pfccfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct ieee_pfc pfc = {0};
   int ret = 0, i;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_setpfc) {
      VMKLNX_DEBUG(9, "ieee_setpfc not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   pfc.pfc_cap = pfccfg->pfcCap;
   pfc.pfc_en = pfccfg->pfcEnabled;
   pfc.mbc = pfccfg->pfcMbc;
   pfc.delay = pfccfg->pfcDelay;
   for (i = 0; i < VMK_DCB_IEEE_MAX_TCS; i++) {
      pfc.requests[i] = pfccfg->pfcReq[i];
      pfc.indications[i] = pfccfg->pfcInd[i];
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                         netdev->dcbnl_ops->ieee_setpfc, netdev, &pfc);
   rtnl_unlock();

   VMKLNX_DEBUG(9, "set IEEE PFC config:%s ret:%d\n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBGetIEEEAppCfg --
 *
 *      Get the DCB IEEE mode Application on the device
 *
 * Results:
 *      VMK_OK if DCB Application get succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBGetIEEEAppCfg(void *device, vmk_DCBApplication *appcfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct dcb_app app = {0};
   int ret = 0;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_getapp) {
      VMKLNX_DEBUG(9, "ieee_getapp not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->ieee_getapp, netdev, &app);
   rtnl_unlock();

   appcfg->sf = app.selector;
   appcfg->protoID = app.protocol;
   appcfg->priority = app.priority;

   VMKLNX_DEBUG(9, "get IEEE APP config:%s ret:%d \n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBSetIEEEAppCfg --
 *
 *      Set the DCB IEEE mode Application on the device
 *
 * Results:
 *      VMK_OK if DCB Application set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      DCB Application configurations of the device is set.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBSetIEEEAppCfg(void *device, vmk_DCBApplication *appcfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct dcb_app app = {0};
   int ret = 0;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_setapp) {
      VMKLNX_DEBUG(9, "ieee_setapp not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   app.selector = appcfg->sf;
   app.protocol = appcfg->protoID;
   app.priority = appcfg->priority;

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->ieee_setapp, netdev, &app);
   rtnl_unlock();

   VMKLNX_DEBUG(9, "set IEEE APP config:%s ret:%d \n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICDCBDelIEEEAppCfg --
 *
 *      Delete the IEEE mode DCB Application of the device
 *
 * Results:
 *      VMK_OK if DCB Application set succeeded.
 *      VMK_NOT_SUPPORTED if callback is not defined by the driver.
 *
 * Side effects:
 *      DCB Application configurations of the device is set.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
NICDCBDelIEEEAppCfg(void *device, vmk_DCBApplication *appcfg)
{
   struct net_device *netdev = (struct net_device *) device;
   LinNetDev *linDev = get_LinNetDev(netdev);
   struct dcb_app app = {0};
   int ret = 0;

   VMK_ASSERT(netdev);

   if (!(linDev->flags & NET_VMKLNX_IEEE_DCB)) {
      VMKLNX_DEBUG(9, "IEEE DCB is not supported on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   if (!netdev->dcbnl_ops || !netdev->dcbnl_ops->ieee_delapp) {
      VMKLNX_DEBUG(9, "ieee_delapp not defined on %s", netdev->name);
      return VMK_NOT_SUPPORTED;
   }

   app.selector = appcfg->sf;
   app.priority = appcfg->priority;
   app.protocol = appcfg->protoID;

   rtnl_lock();
   VMKAPI_MODULE_CALL(netdev->module_id, ret,
                      netdev->dcbnl_ops->ieee_delapp, netdev, &app);
   rtnl_unlock();

   VMKLNX_DEBUG(9, "delete IEEE app config:%s ret:%d\n", netdev->name, ret);

   return (ret ? VMK_FAILURE : VMK_OK);
}
