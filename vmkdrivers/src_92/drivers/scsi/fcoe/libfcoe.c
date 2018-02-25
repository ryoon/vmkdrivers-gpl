/*
 * Copyright (c) 2008-2009 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2009 Intel Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/string.h>
#if !defined(__VMKLNX__)
#include <net/rtnetlink.h>
#endif /* !defined(__VMKLNX__) */

#include <scsi/fc/fc_els.h>
#include <scsi/fc/fc_fs.h>
#include <scsi/fc/fc_fip.h>
#include <scsi/fc/fc_encaps.h>
#include <scsi/fc/fc_fcoe.h>
#include <scsi/fc/fc_fcp.h>

#include <scsi/libfc.h>
#include <scsi/libfcoe.h>

#if defined(__VMKLNX__)
#include "fcoe.h"
#include <vmklinux_92/vmklinux_cna.h>
#endif /* defined(__VMKLNX__) */

MODULE_AUTHOR("Open-FCoE.org");
MODULE_DESCRIPTION("FIP discovery protocol support for FCoE HBAs");
MODULE_LICENSE("GPLv2");
#if defined(__VMKLNX__)
/*
 * Match the major version number with libfcoe.sc scons file
 */
MODULE_VERSION("1.0.24.9.4-0vmw")
#endif /* defined(__VMKLNX__) */

#define	FCOE_CTLR_MIN_FKA	500		/* min keep alive (mS) */
#define	FCOE_CTLR_DEF_FKA	FIP_DEF_FKA	/* default keep alive (mS) */

#if defined(__VMKLNX__)
/* libfcoe log level*/
#define	LIBFCOE_MODULE_NAME	"libfcoe"
vmk_LogComponent libfcoeLog;

/* module_param for turning on/off VN2VN */
static char *vn2vn_ports = "All ports are set to fabric by default. User specifices vn2vn ports";
module_param(vn2vn_ports, charp,  S_IRUGO);
MODULE_PARM_DESC(vn2vn_ports,
   "'Turn on VN2VN  feature on select ports (or all)\n"
   "    (default == 0)");

#endif /* #if defined(__VMKLNX__) */

static void fcoe_ctlr_timeout(unsigned long);
static void fcoe_ctlr_timer_work(struct work_struct *);
static void fcoe_ctlr_recv_work(struct work_struct *);

static void fcoe_ctlr_select(struct fcoe_ctlr *fip);

u8 fcoe_all_fcfs[ETH_ALEN] = FIP_ALL_FCF_MACS;
u8 fcoe_all_enode[ETH_ALEN] = FIP_ALL_ENODE_MACS;
u8 fcoe_all_vn2vn[ETH_ALEN] = FIP_ALL_VN2VN_MACS;
u8 fcoe_all_p2p[ETH_ALEN] = FIP_ALL_P2P_MACS;

unsigned int libfcoe_debug_logging = 0x2;
module_param_named(debug_logging, libfcoe_debug_logging, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug_logging, "a bit mask of logging levels");

#if defined(__VMKLNX__)
static int fcoe_ctlr_vlan_request(struct fcoe_ctlr *fip);
static int fcoe_ctlr_recv_vlan_notification(struct fcoe_ctlr *fip, struct fip_header *fh);

/*
 * This is defined to check the compatibility of fcoe_ctlr structural changes
 * with previous released drivers which uses libfcoe.
 */
#define SAME_OFFSET(st1, st2, field) (offsetof(st1, field) == offsetof(st2, field))
VMK_ASSERT_LIST(struct_fcoe_ctlr_offset_check,
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, state));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, mode));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, lp));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, sel_fcf));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, fcfs));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, fcf_count));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, sol_time));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, sel_time));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, port_ka_time));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, ctlr_ka_time));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, timer));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, timer_work));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, recv_work));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, fip_recv_list));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, user_mfs));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, flogi_oxid));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, flogi_count));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, reset_req));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, map_dest));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, spma));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, send_ctlr_ka));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, send_port_ka));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, dest_addr));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, ctl_src_addr));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, vlan_id));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, send));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, update_mac));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct fcoe_ctlr, struct fcoe_ctlr_9_2_2_x, get_src_addr));
)

VMK_ASSERT_LIST(struct_fcoe_ctlr,
   VMK_ASSERT_ON_COMPILE(sizeof(struct fcoe_ctlr) ==
                         sizeof(struct fcoe_ctlr_9_2_2_x));
)
#endif	/* defined(__VMKLNX__) */

/**
 * fcoe_ctlr_mtu_valid() - Check if a FCF's MTU is valid
 * @fcf: The FCF to check
 *
 * Return non-zero if FCF fcoe_size has been validated.
 */
static inline int fcoe_ctlr_mtu_valid(const struct fcoe_fcf *fcf)
{
	return (fcf->flags & FIP_FL_SOL) != 0;
}

/**
 * fcoe_ctlr_fcf_usable() - Check if a FCF is usable
 * @fcf: The FCF to check
 *
 * Return non-zero if the FCF is usable.
 */
static inline int fcoe_ctlr_fcf_usable(struct fcoe_fcf *fcf)
{
	u16 flags = FIP_FL_SOL | FIP_FL_AVAIL;

	return (fcf->flags & flags) == flags;
}

 /**
 * fcoe_ctlr_map_dest() - Set flag and OUI for mapping destination addresses
 * @fip: The FCoE controller
 */
void fcoe_ctlr_map_dest(struct fcoe_ctlr *fip)
{
	u8 *ptr;
	if (fip->mode == FIP_MODE_VN2VN)
		hton24(fip->dest_addr, FIP_VN_FC_MAP);
	else
		hton24(fip->dest_addr, FIP_DEF_FC_MAP);
        ptr = &fip->dest_addr[3];
	hton24(ptr, 0);
	fip->map_dest = 1;
}

/**
 * fcoe_ctlr_init() - Initialize the FCoE Controller instance
 * @fip: The FCoE controller to initialize
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_init */
void fcoe_ctlr_init(struct fcoe_ctlr *fip)
{
	fip->state = FIP_ST_LINK_WAIT;
	fip->mode = FIP_MODE_AUTO;
	INIT_LIST_HEAD(&fip->fcfs);
	INIT_LIST_HEAD(&fip->remote_vn_ports);
#if defined(__VMKLNX__)
	fip->ctlr_lock = kmalloc(sizeof(spinlock_t), GFP_ATOMIC);
	spin_lock_init(fip->ctlr_lock);
# else
	mutex_init(&fip->ctlr_mutex);
#endif /* defined(__VMKLNX__) */
	fip->flogi_oxid = FC_XID_UNKNOWN;
	setup_timer(&fip->timer, fcoe_ctlr_timeout, (unsigned long)fip);
	INIT_WORK(&fip->timer_work, fcoe_ctlr_timer_work);
	INIT_WORK(&fip->recv_work, fcoe_ctlr_recv_work);
	skb_queue_head_init(&fip->fip_recv_list);
}
EXPORT_SYMBOL(fcoe_ctlr_init);

#if defined(__VMKLNX__)
static int __init libfcoe_init(void)
{
	VMK_ReturnStatus vmkStat;
	vmk_LogProperties logProps;

	vmkStat = vmk_NameInitialize(&logProps.name, LIBFCOE_MODULE_NAME);
 	VMK_ASSERT(vmkStat == VMK_OK);
 	logProps.module = vmklnx_this_module_id;
 	logProps.heap = vmk_ModuleGetHeapID(vmklnx_this_module_id);
 	logProps.defaultLevel = 0;
 	logProps.throttle = NULL;
 	vmkStat = vmk_LogRegister(&logProps, &libfcoeLog);
 	if (vmkStat != VMK_OK) {
 		printk(KERN_ERR "LIBFCOE vmk_LogRegister failed: %s.\n",
 			vmk_StatusToString(vmkStat));
 		return 1;
 	}
	vmk_LogSetCurrentLogLevel(libfcoeLog, libfcoe_debug_logging);

	return 0;
}
module_init(libfcoe_init);

static void __exit libfcoe_exit(void)
{
	vmk_LogUnregister(libfcoeLog);
}
module_exit(libfcoe_exit);
#endif /* defined(__VMKLNX__) */

/**
 * fcoe_ctlr_reset_fcfs() - Reset and free all FCFs for a controller
 * @fip: The FCoE controller whose FCFs are to be reset
 *
 * Called with &fcoe_ctlr lock held.
 */
static void fcoe_ctlr_reset_fcfs(struct fcoe_ctlr *fip)
{
	struct fcoe_fcf *fcf;
	struct fcoe_fcf *next;

	fip->sel_fcf = NULL;
	list_for_each_entry_safe(fcf, next, &fip->fcfs, list) {
		list_del(&fcf->list);
		kfree(fcf);
	}
	fip->fcf_count = 0;
	fip->sel_time = 0;
}

/**
 * fcoe_ctlr_destroy() - Disable and tear down a FCoE controller
 * @fip: The FCoE controller to tear down
 *
 * This is called by FCoE drivers before freeing the &fcoe_ctlr.
 *
 * The receive handler will have been deleted before this to guarantee
 * that no more recv_work will be scheduled.
 *
 * The timer routine will simply return once we set FIP_ST_DISABLED.
 * This guarantees that no further timeouts or work will be scheduled.
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_destroy */
void fcoe_ctlr_destroy(struct fcoe_ctlr *fip)
{
	cancel_work_sync(&fip->recv_work);
	skb_queue_purge(&fip->fip_recv_list);
#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	fip->state = FIP_ST_DISABLED;
	fcoe_ctlr_reset_fcfs(fip);
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
	kfree(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	del_timer_sync(&fip->timer);
	cancel_work_sync(&fip->timer_work);
}
EXPORT_SYMBOL(fcoe_ctlr_destroy);

/**
 * fcoe_ctlr_fcoe_size() - Return the maximum FCoE size required for VN_Port
 * @fip: The FCoE controller to get the maximum FCoE size from
 *
 * Returns the maximum packet size including the FCoE header and trailer,
 * but not including any Ethernet or VLAN headers.
 */
u32 fcoe_ctlr_fcoe_size(struct fcoe_ctlr *fip)
{
	/*
	 * Determine the max FCoE frame size allowed, including
	 * FCoE header and trailer.
	 * Note:  lp->mfs is currently the payload size, not the frame size.
	 */
	return fip->lp->mfs + sizeof(struct fc_frame_header) +
		sizeof(struct fcoe_hdr) + sizeof(struct fcoe_crc_eof);
}

/**
 * fcoe_ctlr_solicit() - Send a FIP solicitation
 * @fip: The FCoE controller to send the solicitation on
 * @fcf: The destination FCF (if NULL, a multicast solicitation is sent)
 */
static void fcoe_ctlr_solicit(struct fcoe_ctlr *fip, struct fcoe_fcf *fcf)
{
	struct sk_buff *skb;
	struct fip_sol {
		struct ethhdr eth;
		struct fip_header fip;
		struct {
			struct fip_mac_desc mac;
			struct fip_wwn_desc wwnn;
			struct fip_size_desc size;
		} __attribute__((packed)) desc;
	}  __attribute__((packed)) *sol;
	u32 fcoe_size;

	if (0 == fip->vlan_id)
		return;

	skb = dev_alloc_skb(sizeof(*sol));
	if (!skb)
		return;

	sol = (struct fip_sol *)skb->data;

	memset(sol, 0, sizeof(*sol));
	memcpy(sol->eth.h_dest, fcf ? fcf->fcf_mac : fcoe_all_fcfs, ETH_ALEN);
	memcpy(sol->eth.h_source, fip->ctl_src_addr, ETH_ALEN);
	sol->eth.h_proto = htons(ETH_P_FIP);

	sol->fip.fip_ver = FIP_VER_ENCAPS(FIP_VER);
	sol->fip.fip_op = htons(FIP_OP_DISC);
	sol->fip.fip_subcode = FIP_SC_SOL;
	sol->fip.fip_dl_len = htons(sizeof(sol->desc) / FIP_BPW);
	sol->fip.fip_flags = htons(FIP_FL_FPMA);
	if (fip->spma)
		sol->fip.fip_flags |= htons(FIP_FL_SPMA);

	sol->desc.mac.fd_desc.fip_dtype = FIP_DT_MAC;
	sol->desc.mac.fd_desc.fip_dlen = sizeof(sol->desc.mac) / FIP_BPW;
	memcpy(sol->desc.mac.fd_mac, fip->ctl_src_addr, ETH_ALEN);

	sol->desc.wwnn.fd_desc.fip_dtype = FIP_DT_NAME;
	sol->desc.wwnn.fd_desc.fip_dlen = sizeof(sol->desc.wwnn) / FIP_BPW;
	put_unaligned_be64(fip->lp->wwnn, &sol->desc.wwnn.fd_wwn);

	fcoe_size = fcoe_ctlr_fcoe_size(fip);
	sol->desc.size.fd_desc.fip_dtype = FIP_DT_FCOE_SIZE;
	sol->desc.size.fd_desc.fip_dlen = sizeof(sol->desc.size) / FIP_BPW;
	sol->desc.size.fd_size = htons(fcoe_size);

	skb_put(skb, sizeof(*sol));
	skb->protocol = htons(ETH_P_FIP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	fip->send(fip, skb);

	if (!fcf)
		fip->sol_time = jiffies;
}

/**
 * fcoe_ctlr_link_up() - Start FCoE controller
 * @fip: The FCoE controller to start
 *
 * Called from the LLD when the network link is ready.
 *
 * ESX Deviation Notes:
 * FIP VLAN discovery is only invoked for port that depends on ESX to do it.
 * For device that does FIP VLAN discovery in firmare or the driver, it should
 * set fip->vlan_id to FCOE_FIP_NO_VLAN_DISCOVERY.
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_link_up */
void fcoe_ctlr_link_up(struct fcoe_ctlr *fip)
{
	struct net_device *netdev;
	char *port_ptr = NULL;
	u16 vlan_id;

#if defined(__VMKLNX__)
        spin_lock_bh(fip->ctlr_lock);
#else
        mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	if (fip->state == FIP_ST_NON_FIP || fip->state == FIP_ST_AUTO) {
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		fc_linkup(fip->lp);
	} else if (fip->state == FIP_ST_LINK_WAIT) {
#if defined(__VMKLNX__)
		if (fip->vlan_id !=  FCOE_FIP_NO_VLAN_DISCOVERY) {
#endif /* defined(__VMKLNX__) */
			netdev = (fip->lp)->tt.get_cna_netdev(fip->lp);
			VMK_ASSERT(netdev);
			port_ptr = strstr(vn2vn_ports, netdev->name);
#if defined(__VMKLNX__)
			if (port_ptr || (netdev->features & NETIF_F_CNA_VN2VN))
#else
			if (port_ptr)
#endif /* defined(__VMKLNX__) */
			{
				fip->mode = FIP_MODE_VN2VN;
				printk("fcoe_ctlr_link_up: setting vn2vn mode for %s\n",
	                               netdev->name);
			} else {
				fip->mode = FIP_MODE_FABRIC;
				printk("fcoe_ctlr_link_up: setting fabric mode for %s\n",
	                               netdev->name);
			}
#if defined(__VMKLNX__)
		}
#endif /* defined(__VMKLNX__) */

		fip->state = fip->mode;

                switch (fip->mode) {
                default:
                        LIBFCOE_FIP_DBG(fip, "invalid mode %d\n", fip->mode);
                        /* fall-through */
                case FIP_MODE_AUTO:
			LIBFCOE_FIP_DBG(fip, "%s", "setting AUTO mode.\n");
                case FIP_MODE_FABRIC:
#if defined(__VMKLNX__)
			spin_unlock_bh(fip->ctlr_lock);
#else
			mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
                        fc_linkup(fip->lp);
#if defined(__VMKLNX__)
			if (fip->vlan_id ==  FCOE_FIP_NO_VLAN_DISCOVERY) {
				fcoe_ctlr_solicit(fip, NULL);
			} else {
				struct net_device *netdev;

				netdev = (fip->lp)->tt.get_cna_netdev(fip->lp);
				VMK_ASSERT(netdev);
				vlan_id = vmklnx_cna_get_vlan_tag(netdev) & VLAN_VID_MASK;
				if (fip->vlan_id == 0 && vlan_id != 0 &&
				    vlan_id < FCOE_FIP_NO_VLAN_DISCOVERY) {
					fip->vlan_id = FCOE_FIP_NO_VLAN_DISCOVERY;
					LIBFCOE_FIP_DBG(fip, "vlan id already known(%d), "
							"no FIP VLAN discovery needed\n",
							vlan_id);
					fcoe_ctlr_solicit(fip, NULL);
				} else {
					fcoe_ctlr_vlan_request(fip);
				}
			}
#else /* !defined(__VMKLNX__) */
			fcoe_ctlr_solicit(fip, NULL);
#endif /* defined(__VMKLNX__) */
			break;
                case FIP_MODE_VN2VN:
                        fip->lp->point_to_multipoint = 1;
                        fip->lp->tt.disc_recv_req = fcoe_ctlr_disc_recv;
                        fip->lp->tt.disc_start = fcoe_ctlr_disc_start;
                        fip->lp->tt.disc_stop = fcoe_ctlr_disc_stop;
                        fip->lp->tt.disc_stop_final = fcoe_ctlr_disc_stop_final;
                        fip->lp->disc.priv = fip;

                        fcoe_ctlr_vn_start(fip);
#if defined(__VMKLNX__)
			spin_unlock_bh(fip->ctlr_lock);
#else
			mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
                        fc_linkup(fip->lp);

			/*
			 * Current VN2VN specification does not have any info on
			 * vlan setting, but network l2 switches needs VLAN to
			 * send traffic across. According to the T11 clarification
			 * "http://www.t11.org/ftp/t11/pub/fc/bb-6/12-218v2.pdf"
			 * document user can configure static VLAN-ID or choose not
			 * to. If there is no vlan-id set by the user just message
			 * in the log and move on.
			 */
			netdev = (fip->lp)->tt.get_cna_netdev(fip->lp);
			VMK_ASSERT(netdev);
			vlan_id = vmklnx_cna_get_vlan_tag(netdev) & VLAN_VID_MASK;
			if (!vlan_id) {
				printk("host%d: fip: static vlan id (%d) is NOT"
					"set, no FIP VLAN discovery \n",
					(fip)->lp->host->host_no, vlan_id);
                        }

                        break;
                }

	} else {
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	}
}
EXPORT_SYMBOL(fcoe_ctlr_link_up);

/**
 * fcoe_ctlr_reset() - Reset a FCoE controller
 * @fip:       The FCoE controller to reset
 */
static void fcoe_ctlr_reset(struct fcoe_ctlr *fip)
{
	fcoe_ctlr_reset_fcfs(fip);
#if !defined(__VMKLNX__)
	del_timer(&fip->timer);
#else /* !defined(__VMKLNX__) */
	/*
	 * Remove the timer only if FIP VLAN discovery has succeeded.
	 */
	if (fip->vlan_id != 0) {
		del_timer(&fip->timer);
	}
#endif /* !defined(__VMKLNX__) */
	fip->ctlr_ka_time = 0;
	fip->port_ka_time = 0;
	fip->sol_time = 0;
	fip->flogi_oxid = FC_XID_UNKNOWN;
	fip->map_dest = 0;
}

/**
 * fcoe_ctlr_link_down() - Stop a FCoE controller
 * @fip: The FCoE controller to be stopped
 *
 * Returns non-zero if the link was up and now isn't.
 *
 * Called from the LLD when the network link is not ready.
 * There may be multiple calls while the link is down.
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_link_down */
int fcoe_ctlr_link_down(struct fcoe_ctlr *fip)
{
	int link_dropped;

	LIBFCOE_FIP_DBG(fip, "link down.\n");
#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
        fcoe_ctlr_reset(fip);
	link_dropped = fip->state != FIP_ST_LINK_WAIT;
	fip->state = FIP_ST_LINK_WAIT;
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	if (link_dropped)
		fc_linkdown(fip->lp);
	return link_dropped;
}
EXPORT_SYMBOL(fcoe_ctlr_link_down);

/**
 * fcoe_ctlr_send_keep_alive() - Send a keep-alive to the selected FCF
 * @fip:   The FCoE controller to send the FKA on
 * @lport: libfc fc_lport to send from
 * @ports: 0 for controller keep-alive, 1 for port keep-alive
 * @sa:	   The source MAC address
 *
 * A controller keep-alive is sent every fka_period (typically 8 seconds).
 * The source MAC is the native MAC address.
 *
 * A port keep-alive is sent every 90 seconds while logged in.
 * The source MAC is the assigned mapped source address.
 * The destination is the FCF's F-port.
 */
static void fcoe_ctlr_send_keep_alive(struct fcoe_ctlr *fip,
				      struct fc_lport *lport,
				      int ports, u8 *sa)
{
	struct sk_buff *skb;
	struct fip_kal {
		struct ethhdr eth;
		struct fip_header fip;
		struct fip_mac_desc mac;
	} __attribute__((packed)) *kal;
	struct fip_vn_desc *vn;
	u32 len;
	struct fc_lport *lp;
	struct fcoe_fcf *fcf;

	fcf = fip->sel_fcf;
	lp = fip->lp;
	if (!fcf || !fc_host_port_id(lp->host))
		return;

	len = sizeof(*kal) + ports * sizeof(*vn);
	skb = dev_alloc_skb(len);
	if (!skb) {
		LIBFCOE_FIP_DBG(fip, "skb alloc failed- not sending FIP KA\n");
		return;
	}

	kal = (struct fip_kal *)skb->data;
	memset(kal, 0, len);
	memcpy(kal->eth.h_dest, fcf->fcf_mac, ETH_ALEN);
	memcpy(kal->eth.h_source, sa, ETH_ALEN);
	kal->eth.h_proto = htons(ETH_P_FIP);

	kal->fip.fip_ver = FIP_VER_ENCAPS(FIP_VER);
	kal->fip.fip_op = htons(FIP_OP_CTRL);
	kal->fip.fip_subcode = FIP_SC_KEEP_ALIVE;
	kal->fip.fip_dl_len = htons((sizeof(kal->mac) +
				     ports * sizeof(*vn)) / FIP_BPW);
	kal->fip.fip_flags = htons(FIP_FL_FPMA);
	if (fip->spma)
		kal->fip.fip_flags |= htons(FIP_FL_SPMA);

	kal->mac.fd_desc.fip_dtype = FIP_DT_MAC;
	kal->mac.fd_desc.fip_dlen = sizeof(kal->mac) / FIP_BPW;
	memcpy(kal->mac.fd_mac, fip->ctl_src_addr, ETH_ALEN);
	if (ports) {
		vn = (struct fip_vn_desc *)(kal + 1);
		vn->fd_desc.fip_dtype = FIP_DT_VN_ID;
		vn->fd_desc.fip_dlen = sizeof(*vn) / FIP_BPW;
		memcpy(vn->fd_mac, fip->get_src_addr(lport), ETH_ALEN);
		hton24(vn->fd_fc_id, fc_host_port_id(lp->host));
		put_unaligned_be64(lp->wwpn, &vn->fd_wwpn);
	}
	skb_put(skb, len);
	skb->protocol = htons(ETH_P_FIP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	fip->send(fip, skb);
}

/**
 * fcoe_ctlr_encaps() - Encapsulate an ELS frame for FIP, without sending it
 * @fip:   The FCoE controller for the ELS frame
 * @lport: local port
 * @dtype: The FIP descriptor type for the frame
 * @skb:   The FCoE ELS frame including FC header but no FCoE headers
 * @d_id:  The destination port ID.
 * Returns non-zero error code on failure.
 *
 * The caller must check that the length is a multiple of 4.
 *
 * The @skb must have enough headroom (28 bytes) and tailroom (8 bytes).
 * Headroom includes the FIP encapsulation description, FIP header, and
 * Ethernet header.  The tailroom is for the FIP MAC descriptor.
 */
static int fcoe_ctlr_encaps(struct fcoe_ctlr *fip, struct fc_lport *lport,
			    u8 dtype, struct sk_buff *skb, u32 d_id)
{
	struct fip_encaps_head {
		struct ethhdr eth;
		struct fip_header fip;
		struct fip_encaps encaps;
	} __attribute__((packed)) *cap;
	struct fc_frame_header *fh;
	struct fip_mac_desc *mac;
	struct fcoe_fcf *fcf;
	size_t dlen;
	u16 fip_flags;
        u8 op;
	u8 *mac_ptr;

	fh = (struct fc_frame_header *)skb->data;
	op = *(u8 *)(fh + 1);
	dlen = sizeof(struct fip_encaps) + skb->len;	/* len before push */
	cap = (struct fip_encaps_head *)skb_push(skb, sizeof(*cap));

	memset(cap, 0, sizeof(*cap));

	if (lport->point_to_multipoint) {
		if (fcoe_ctlr_vn_lookup(fip, d_id, cap->eth.h_dest))
			return -ENODEV;
                fip_flags = 0;
	} else {
		fcf = fip->sel_fcf;
		if (!fcf)
			return -ENODEV;
		fip_flags = fcf->flags;
		fip_flags &= fip->spma ? FIP_FL_SPMA | FIP_FL_FPMA :
					 FIP_FL_FPMA;
		if (!fip_flags)
			return -ENODEV;
		memcpy(cap->eth.h_dest, fcf->fcf_mac, ETH_ALEN);
	}
	memcpy(cap->eth.h_source, fip->ctl_src_addr, ETH_ALEN);
	cap->eth.h_proto = htons(ETH_P_FIP);

	cap->fip.fip_ver = FIP_VER_ENCAPS(FIP_VER);
	cap->fip.fip_op = htons(FIP_OP_LS);
	if (op == ELS_LS_ACC || op == ELS_LS_RJT)
		cap->fip.fip_subcode = FIP_SC_REP;
	else
		cap->fip.fip_subcode = FIP_SC_REQ;
        cap->fip.fip_flags = htons(fip_flags);

	cap->encaps.fd_desc.fip_dtype = dtype;
	cap->encaps.fd_desc.fip_dlen = dlen / FIP_BPW;
	if (op != ELS_LS_RJT) {
		dlen += sizeof(*mac);
		mac = (struct fip_mac_desc *)skb_put(skb, sizeof(*mac));
		memset(mac, 0, sizeof(*mac));
		mac->fd_desc.fip_dtype = FIP_DT_MAC;
		mac->fd_desc.fip_dlen = sizeof(*mac) / FIP_BPW;
		if (dtype != FIP_DT_FLOGI && dtype != FIP_DT_FDISC) {
			memcpy(mac->fd_mac, fip->get_src_addr(lport), ETH_ALEN);
		} else if (fip->mode == FIP_MODE_VN2VN) {
			hton24(mac->fd_mac, FIP_VN_FC_MAP);
			mac_ptr = &mac->fd_mac[3];
			hton24(mac_ptr, fip->port_id);
		} else if (fip_flags & FIP_FL_SPMA) {
			LIBFCOE_FIP_DBG(fip, "FLOGI/FDISC sent with SPMA\n");
			memcpy(mac->fd_mac, fip->ctl_src_addr, ETH_ALEN);
		} else {
			LIBFCOE_FIP_DBG(fip, "FLOGI/FDISC sent with FPMA\n");
			/* FPMA only FLOGI.  Must leave the MAC desc zeroed. */
		}
	}
	cap->fip.fip_dl_len = htons(dlen / FIP_BPW);
	skb->protocol = htons(ETH_P_FIP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	return 0;
}

/**
 * fcoe_ctlr_els_send() - Send an ELS frame encapsulated by FIP if appropriate.
 * @fip:	FCoE controller.
 * @lport:	libfc fc_lport to send from
 * @skb:	FCoE ELS frame including FC header but no FCoE headers.
 *
 * Returns a non-zero error code if the frame should not be sent.
 * Returns zero if the caller should send the frame with FCoE encapsulation.
 *
 * The caller must check that the length is a multiple of 4.
 * The SKB must have enough headroom (28 bytes) and tailroom (8 bytes).
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_els_send */
int fcoe_ctlr_els_send(struct fcoe_ctlr *fip, struct fc_lport *lport,
		       struct sk_buff *skb)
{
	struct fc_frame *fp;
	struct fc_frame_header *fh;
	u16 old_xid;
	u8 op;
	u8 mac[ETH_ALEN];

	fp = container_of(skb, struct fc_frame, skb);
	fh = (struct fc_frame_header *)skb->data;
	op = *(u8 *)(fh + 1);

	if (op == ELS_FLOGI && fip->mode != FIP_MODE_VN2VN) {
		old_xid = fip->flogi_oxid;
		fip->flogi_oxid = ntohs(fh->fh_ox_id);
		if (fip->state == FIP_ST_AUTO) {
			if (old_xid == FC_XID_UNKNOWN)
				fip->flogi_count = 0;
			fip->flogi_count++;
			if (fip->flogi_count < 3)
				goto drop;
			fcoe_ctlr_map_dest(fip);
			return 0;
		}
		if (fip->state == FIP_ST_NON_FIP)
			fip->map_dest = 1;
	}

	if (fip->state == FIP_ST_NON_FIP)
		return 0;
	if (!fip->sel_fcf && fip->mode != FIP_MODE_VN2VN)
		goto drop;

	switch (op) {
	case ELS_FLOGI:
		op = FIP_DT_FLOGI;
		break;
	case ELS_FDISC:
		if (ntoh24(fh->fh_s_id))
			return 0;
		op = FIP_DT_FDISC;
		break;
	case ELS_LOGO:
		if (fip->mode == FIP_MODE_VN2VN) {
			if (fip->state != FIP_ST_VNMP_UP)
				return -EINVAL;
			if (ntoh24(fh->fh_d_id) == FC_FID_FLOGI)
				return -EINVAL;
		} else {
			if (fip->state != FIP_ST_ENABLED)
				return 0;
			if (ntoh24(fh->fh_d_id) != FC_FID_FLOGI)
				return 0;
		}
		op = FIP_DT_LOGO;
		break;
	case ELS_LS_ACC:
		/*
		 * Here we must've gotten an SID by accepting an FLOGI
		 * If non-FIP, we may have gotten an SID by accepting an FLOGI
		 * from a point-to-point connection.  Switch to using
		 * the source mac based on the SID.  The destination
		 * MAC in this case would have been set by receving the
		 * FLOGI.
		 */
		if (fip->state == FIP_ST_NON_FIP) {
			if (fip->flogi_oxid == FC_XID_UNKNOWN)
				return 0;
			fip->flogi_oxid = FC_XID_UNKNOWN;
			fc_fcoe_set_mac(mac, fh->fh_d_id);
			fip->update_mac(lport, mac);
		}
		/* fall through */
	case ELS_LS_RJT:
		op = fr_encaps(fp);
		if (op)
			break;
		return 0;
	default:
		if (fip->state != FIP_ST_ENABLED &&
		    fip->state != FIP_ST_VNMP_UP)
			goto drop;
		return 0;
	}
	LIBFCOE_FIP_DBG(fip, "els_send op %u d_id %x\n",
			op, ntoh24(fh->fh_d_id));
	if (fcoe_ctlr_encaps(fip, lport, op, skb, ntoh24(fh->fh_d_id)))
		goto drop;
	fip->send(fip, skb);
	return -EINPROGRESS;
drop:
	kfree_skb(skb);
	return -EINVAL;
}
EXPORT_SYMBOL(fcoe_ctlr_els_send);

/**
 * fcoe_ctlr_age_fcfs() - Reset and free all old FCFs for a controller
 * @fip: The FCoE controller to free FCFs on
 *
 * Called with lock held and preemption disabled.
 *
 * An FCF is considered old if we have missed three advertisements.
 * That is, there have been no valid advertisement from it for three
 * times its keep-alive period including fuzz.
 *
 * In addition, determine the time when an FCF selection can occur.
 *
 * Also, increment the MissDiscAdvCount when no advertisement is received
 * for the corresponding FCF for 1.5 * FKA_ADV_PERIOD (FC-BB-5 LESB).
 */
static void fcoe_ctlr_age_fcfs(struct fcoe_ctlr *fip)
{
	struct fcoe_fcf *fcf;
	struct fcoe_fcf *next;
	unsigned long sel_time = 0;
	unsigned long mda_time = 0;
	struct fcoe_dev_stats *stats;

	list_for_each_entry_safe(fcf, next, &fip->fcfs, list) {
		mda_time = fcf->fka_period + (fcf->fka_period >> 1);
		if ((fip->sel_fcf == fcf) &&
		    (time_after(jiffies, fcf->time + mda_time))) {
			mod_timer(&fip->timer, jiffies + mda_time);
#if defined(__VMKLNX__)
			stats = FCOE_PER_CPU_PTR(fip->lp->dev_stats,
			                         smp_processor_id(),
			                         sizeof(struct fcoe_dev_stats));
#else
			stats = per_cpu_ptr(fip->lp->dev_stats,
			                    smp_processor_id());
#endif
			stats->MissDiscAdvCount++;
			printk(KERN_INFO "libfcoe: host%d: Missing Discovery "
			       "Advertisement for fab %16.16llx count %lld\n",
			       fip->lp->host->host_no, fcf->fabric_name,
			       stats->MissDiscAdvCount);
		}
		if (time_after_eq(jiffies, fcf->time + fcf->fka_period * 3 +
			          msecs_to_jiffies(FIP_FCF_FUZZ * 3))) {
			if (fip->sel_fcf == fcf)
				fip->sel_fcf = NULL;
			list_del(&fcf->list);
			WARN_ON(!fip->fcf_count);
			fip->fcf_count--;
			kfree(fcf);
#if defined(__VMKLNX__)
			stats = FCOE_PER_CPU_PTR(fip->lp->dev_stats,
			                         smp_processor_id(),
			                         sizeof(struct fcoe_dev_stats));
#else
			stats = per_cpu_ptr(fip->lp->dev_stats,
			                    smp_processor_id());
#endif
			stats->VLinkFailureCount++;
		} else if (fcoe_ctlr_mtu_valid(fcf) &&
			   (!sel_time || time_before(sel_time, fcf->time))) {
			sel_time = fcf->time;
		}
	}
	if (sel_time && !fip->sel_fcf && !fip->sel_time) {
		sel_time += msecs_to_jiffies(FCOE_CTLR_START_DELAY);
		fip->sel_time = sel_time;
		if (time_before(sel_time, fip->timer.expires))
			mod_timer(&fip->timer, sel_time);
	}
}

/**
 * fcoe_ctlr_parse_adv() - Decode a FIP advertisement into a new FCF entry
 * @fip: The FCoE controller receiving the advertisement
 * @skb: The received FIP advertisement frame
 * @fcf: The resulting FCF entry
 *
 * Returns zero on a valid parsed advertisement,
 * otherwise returns non zero value.
 */
static int fcoe_ctlr_parse_adv(struct fcoe_ctlr *fip,
			       struct sk_buff *skb, struct fcoe_fcf *fcf)
{
	struct fip_header *fiph;
	struct fip_desc *desc = NULL;
	struct fip_wwn_desc *wwn;
	struct fip_fab_desc *fab;
	struct fip_fka_desc *fka;
	unsigned long t;
	size_t rlen;
	size_t dlen;

	memset(fcf, 0, sizeof(*fcf));
	fcf->fka_period = msecs_to_jiffies(FCOE_CTLR_DEF_FKA);

	fiph = (struct fip_header *)skb->data;
	fcf->flags = ntohs(fiph->fip_flags);

	rlen = ntohs(fiph->fip_dl_len) * 4;
	if (rlen + sizeof(*fiph) > skb->len)
		return -EINVAL;

	desc = (struct fip_desc *)(fiph + 1);
	while (rlen > 0) {
		dlen = desc->fip_dlen * FIP_BPW;
		if (dlen < sizeof(*desc) || dlen > rlen)
			return -EINVAL;
		switch (desc->fip_dtype) {
		case FIP_DT_PRI:
			if (dlen != sizeof(struct fip_pri_desc))
				goto len_err;
			fcf->pri = ((struct fip_pri_desc *)desc)->fd_pri;
			break;
		case FIP_DT_MAC:
			if (dlen != sizeof(struct fip_mac_desc))
				goto len_err;
			memcpy(fcf->fcf_mac,
			       ((struct fip_mac_desc *)desc)->fd_mac,
			       ETH_ALEN);
			if (!is_valid_ether_addr(fcf->fcf_mac)) {
				LIBFCOE_FIP_DBG(fip,
					"Invalid MAC addr %pM in FIP adv\n",
					fcf->fcf_mac);
				return -EINVAL;
			}
			break;
		case FIP_DT_NAME:
			if (dlen != sizeof(struct fip_wwn_desc))
				goto len_err;
			wwn = (struct fip_wwn_desc *)desc;
			fcf->switch_name = get_unaligned_be64(&wwn->fd_wwn);
			break;
		case FIP_DT_FAB:
			if (dlen != sizeof(struct fip_fab_desc))
				goto len_err;
			fab = (struct fip_fab_desc *)desc;
			fcf->fabric_name = get_unaligned_be64(&fab->fd_wwn);
			fcf->vfid = ntohs(fab->fd_vfid);
			fcf->fc_map = ntoh24(fab->fd_map);
			break;
		case FIP_DT_FKA:
			if (dlen != sizeof(struct fip_fka_desc))
				goto len_err;
			fka = (struct fip_fka_desc *)desc;
			if (fka->fd_flags & FIP_FKA_ADV_D)
				fcf->fd_flags = 1;
			t = ntohl(fka->fd_fka_period);
			if (t >= FCOE_CTLR_MIN_FKA)
				fcf->fka_period = msecs_to_jiffies(t);
			break;
		case FIP_DT_MAP_OUI:
		case FIP_DT_FCOE_SIZE:
		case FIP_DT_FLOGI:
		case FIP_DT_FDISC:
		case FIP_DT_LOGO:
		case FIP_DT_ELP:
		default:
			LIBFCOE_FIP_DBG(fip, "unexpected descriptor type %x "
					"in FIP adv\n", desc->fip_dtype);
			/* standard says ignore unknown descriptors >= 128 */
			if (desc->fip_dtype < FIP_DT_VENDOR_BASE)
				return -EINVAL;
			continue;
		}
		desc = (struct fip_desc *)((char *)desc + dlen);
		rlen -= dlen;
	}
	if (!fcf->fc_map || (fcf->fc_map & 0x10000))
		return -EINVAL;
	if (!fcf->switch_name)
		return -EINVAL;
	return 0;

len_err:
	LIBFCOE_FIP_DBG(fip, "FIP length error in descriptor type %x len %zu\n",
			desc->fip_dtype, dlen);
	return -EINVAL;
}

/**
 * fcoe_ctlr_recv_adv() - Handle an incoming advertisement
 * @fip: The FCoE controller receiving the advertisement
 * @skb: The received FIP packet
 */
static void fcoe_ctlr_recv_adv(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	struct fcoe_fcf *fcf;
	struct fcoe_fcf new;
	unsigned long sol_tov = msecs_to_jiffies(FCOE_CTRL_SOL_TOV);
	int first = 0;
	int mtu_valid;
	int found = 0;

	if (fcoe_ctlr_parse_adv(fip, skb, &new))
		return;

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	first = list_empty(&fip->fcfs);
	list_for_each_entry(fcf, &fip->fcfs, list) {
		if (fcf->switch_name == new.switch_name &&
		    fcf->fabric_name == new.fabric_name &&
		    fcf->fc_map == new.fc_map &&
		    compare_ether_addr(fcf->fcf_mac, new.fcf_mac) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) {
		if (fip->fcf_count >= FCOE_CTLR_FCF_LIMIT)
			goto out;

		fcf = kmalloc(sizeof(*fcf), GFP_ATOMIC);
		if (!fcf)
			goto out;

		fip->fcf_count++;
		memcpy(fcf, &new, sizeof(new));
		list_add(&fcf->list, &fip->fcfs);
	} else {
		/*
		 * Flags in advertisements are ignored once the FCF is
		 * selected.  Flags in unsolicited advertisements are
		 * ignored after a usable solicited advertisement
		 * has been received.
		 */
		fcf->fd_flags = new.fd_flags;
		if (!fcoe_ctlr_fcf_usable(fcf))
			fcf->flags = new.flags;

		if (fcf == fip->sel_fcf && !fcf->fd_flags) {
			fip->ctlr_ka_time -= fcf->fka_period;
			fip->ctlr_ka_time += new.fka_period;
			if (time_before(fip->ctlr_ka_time, fip->timer.expires))
				mod_timer(&fip->timer, fip->ctlr_ka_time);
		}
		fcf->fka_period = new.fka_period;
		memcpy(fcf->fcf_mac, new.fcf_mac, ETH_ALEN);
	}

	mtu_valid = fcoe_ctlr_mtu_valid(fcf);
	fcf->time = jiffies;
	if (!found) {
		LIBFCOE_FIP_DBG(fip, "New FCF for fab %16.16llx "
				"map %x val %d mac %02x:%02x:%02x:%02x:%02x:%02x\n",
				fcf->fabric_name, fcf->fc_map, mtu_valid,
				fcf->fcf_mac[0], fcf->fcf_mac[1],
				fcf->fcf_mac[2], fcf->fcf_mac[3],
				fcf->fcf_mac[4], fcf->fcf_mac[5]);
	}

	/*
	 * If this advertisement is not solicited and our max receive size
	 * hasn't been verified, send a solicited advertisement.
	 */
	if (!mtu_valid)
		fcoe_ctlr_solicit(fip, fcf);

	/*
	 * If its been a while since we did a solicit, and this is
	 * the first advertisement we've received, do a multicast
	 * solicitation to gather as many advertisements as we can
	 * before selection occurs.
	 */
	if (first && time_after(jiffies, fip->sol_time + sol_tov))
		fcoe_ctlr_solicit(fip, NULL);

	/*
	 * Put this FCF at the head of the list for priority among equals.
	 * This helps in the case of an NPV switch which insists we use
	 * the FCF that answers multicast solicitations, not the others that
	 * are sending periodic multicast advertisements.
	 */
	if (mtu_valid) {
		list_move(&fcf->list, &fip->fcfs);
	}

	/*
	 * If this is the first validated FCF, note the time and
	 * set a timer to trigger selection.
	 *
	 * Update FCF sel_time on new FCF aka FCF that is not in list
	 * and FCF selection has not happened yet.
	 */
	if (mtu_valid && !fip->sel_fcf && fcoe_ctlr_fcf_usable(fcf)) {
		if (!found) {
			fip->sel_time = jiffies +
				msecs_to_jiffies(FCOE_CTLR_START_DELAY);
		}

		if (!timer_pending(&fip->timer) ||
		    time_before(fip->sel_time, fip->timer.expires))
			mod_timer(&fip->timer, fip->sel_time);
	}
out:
#if defined(__VMKLNX__)
        spin_unlock_bh(fip->ctlr_lock);
#else
        mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
}

/**
 * fcoe_ctlr_recv_els() - Handle an incoming FIP encapsulated ELS frame
 * @fip: The FCoE controller which received the packet
 * @skb: The received FIP packet
 */
static void fcoe_ctlr_recv_els(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	struct fc_lport *lport = fip->lp;
	struct fip_header *fiph;
	struct fc_frame *fp = (struct fc_frame *)skb;
	struct fc_frame_header *fh = NULL;
	struct fip_desc *desc;
	struct fip_encaps *els;
	struct fcoe_dev_stats *stats;
	enum fip_desc_type els_dtype = 0;
	u8 els_op;
	u8 sub;
	u8 granted_mac[ETH_ALEN] = { 0 };
	size_t els_len = 0;
	size_t rlen;
	size_t dlen;

	fiph = (struct fip_header *)skb->data;
	sub = fiph->fip_subcode;
	if (sub != FIP_SC_REQ && sub != FIP_SC_REP)
		goto drop;

	rlen = ntohs(fiph->fip_dl_len) * 4;
	if (rlen + sizeof(*fiph) > skb->len)
		goto drop;

	desc = (struct fip_desc *)(fiph + 1);
	while (rlen > 0) {
		dlen = desc->fip_dlen * FIP_BPW;
		if (dlen < sizeof(*desc) || dlen > rlen)
			goto drop;
		switch (desc->fip_dtype) {
		case FIP_DT_MAC:
			if (dlen != sizeof(struct fip_mac_desc))
				goto len_err;
			memcpy(granted_mac,
			       ((struct fip_mac_desc *)desc)->fd_mac,
			       ETH_ALEN);
			if (!is_valid_ether_addr(granted_mac)) {
				LIBFCOE_FIP_DBG(fip, "Invalid MAC address "
						"in FIP ELS\n");
				goto drop;
			}
			memcpy(fr_cb(fp)->granted_mac, granted_mac, ETH_ALEN);
			break;
		case FIP_DT_FLOGI:
		case FIP_DT_FDISC:
		case FIP_DT_LOGO:
		case FIP_DT_ELP:
			if (fh)
				goto drop;
			if (dlen < sizeof(*els) + sizeof(*fh) + 1)
				goto len_err;
			els_len = dlen - sizeof(*els);
			els = (struct fip_encaps *)desc;
			fh = (struct fc_frame_header *)(els + 1);
			els_dtype = desc->fip_dtype;
			break;
		default:
			LIBFCOE_FIP_DBG(fip, "unexpected descriptor type %x "
					"in FIP adv\n", desc->fip_dtype);
			/* standard says ignore unknown descriptors >= 128 */
			if (desc->fip_dtype < FIP_DT_VENDOR_BASE)
				goto drop;
			continue;
		}
		desc = (struct fip_desc *)((char *)desc + dlen);
		rlen -= dlen;
	}

	if (!fh)
		goto drop;
	els_op = *(u8 *)(fh + 1);

#if defined(__VMKLNX__)
	/*
	 * retry FLOGI on next available FCF if FLOGI fails.
	 * This is for supporting NPV mode
	 */
	if (els_dtype == FIP_DT_FLOGI && sub == FIP_SC_REP &&
	    fip->flogi_oxid == ntohs(fh->fh_ox_id) && fip->mode != FIP_MODE_VN2VN ) {
		if (els_op == ELS_LS_ACC && is_valid_ether_addr(granted_mac)) {
			fip->flogi_oxid = FC_XID_UNKNOWN;
		} else if(fip->sel_fcf != NULL) {
			/* FLOGI failed, select next FCF */
			LIBFCOE_FIP_DBG(fip, "FLOGI failed -Try next FCF\n");
			fip->sel_fcf->flags &= ~FIP_FL_AVAIL;
			fcoe_ctlr_select(fip);

			if( fip->sel_fcf  != NULL ) {
				memcpy(fip->dest_addr, fip->sel_fcf->fcf_mac,
				       ETH_ALEN);
				fip->port_ka_time = jiffies +
					msecs_to_jiffies(FIP_VN_KA_PERIOD);
				fip->ctlr_ka_time = jiffies +
					fip->sel_fcf->fka_period;
				if (!timer_pending(&fip->timer) ||
				    time_before(fip->ctlr_ka_time,
						fip->timer.expires)) {
					mod_timer(&fip->timer,
						  fip->ctlr_ka_time);
				}
			}
		}
	}

#else /* !defined(__VMKLNX__) */
	if ((els_dtype == FIP_DT_FLOGI || els_dtype == FIP_DT_FDISC) &&
	    sub == FIP_SC_REP && els_op == ELS_LS_ACC &&
	    fip->mode != FIP_MODE_VN2VN) {
		if (!is_valid_ether_addr(granted_mac)) {
			LIBFCOE_FIP_DBG(fip,
				"Invalid MAC address %pM in FIP ELS\n",
				granted_mac);
			goto drop;
		}
		memcpy(fr_cb(fp)->granted_mac, granted_mac, ETH_ALEN);

		if (fip->flogi_oxid == ntohs(fh->fh_ox_id))
			fip->flogi_oxid = FC_XID_UNKNOWN;
	}
#endif /* !defined(__VMKLNX__) */

	/*
	 * Convert skb into an fc_frame containing only the ELS.
	 */
	skb_pull(skb, (u8 *)fh - skb->data);
	skb_trim(skb, els_len);
	fp = (struct fc_frame *)skb;
	fc_frame_init(fp);
	fr_sof(fp) = FC_SOF_I3;
	fr_eof(fp) = FC_EOF_T;
	fr_dev(fp) = lport;
        fr_encaps(fp) = els_dtype;
#if defined(__VMKLNX__)
	stats = FCOE_PER_CPU_PTR(lport->dev_stats,
	                         get_cpu(),
	                         sizeof(struct fcoe_dev_stats));
#else
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
#endif
	stats->RxFrames++;
	stats->RxWords += skb->len / FIP_BPW;
	put_cpu();

	fc_exch_recv(lport, fp);
	return;

len_err:
	LIBFCOE_FIP_DBG(fip, "FIP length error in descriptor type %x len %zu\n",
			desc->fip_dtype, dlen);
drop:
	kfree_skb(skb);
}

/**
 * fcoe_ctlr_recv_els() - Handle an incoming link reset frame
 * @fip: The FCoE controller that received the frame
 * @fh:	 The received FIP header
 *
 * There may be multiple VN_Port descriptors.
 * The overall length has already been checked.
 */
static void fcoe_ctlr_recv_clr_vlink(struct fcoe_ctlr *fip,
				     struct fip_header *fh)
{
	struct fip_desc *desc;
	struct fip_mac_desc *mp;
	struct fip_wwn_desc *wp;
	struct fip_vn_desc *vp;
	size_t rlen;
	size_t dlen;
	struct fcoe_fcf *fcf = fip->sel_fcf;
	struct fc_lport *lport = fip->lp;
	u32	desc_mask;

	LIBFCOE_FIP_DBG(fip, "Clear Virtual Link received\n");
	if (!fcf || !fc_host_port_id(lport->host))
		return;

	/*
	 * mask of required descriptors.  Validating each one clears its bit.
	 */
	desc_mask = BIT(FIP_DT_MAC) | BIT(FIP_DT_NAME) | BIT(FIP_DT_VN_ID);

	rlen = ntohs(fh->fip_dl_len) * FIP_BPW;
	desc = (struct fip_desc *)(fh + 1);
	while (rlen >= sizeof(*desc)) {
		dlen = desc->fip_dlen * FIP_BPW;
		if (dlen > rlen)
			return;
		switch (desc->fip_dtype) {
		case FIP_DT_MAC:
			mp = (struct fip_mac_desc *)desc;
			if (dlen < sizeof(*mp))
				return;
			if (compare_ether_addr(mp->fd_mac, fcf->fcf_mac))
				return;
			desc_mask &= ~BIT(FIP_DT_MAC);
			break;
		case FIP_DT_NAME:
			wp = (struct fip_wwn_desc *)desc;
			if (dlen < sizeof(*wp))
				return;
			if (get_unaligned_be64(&wp->fd_wwn) != fcf->switch_name)
				return;
			desc_mask &= ~BIT(FIP_DT_NAME);
			break;
		case FIP_DT_VN_ID:
			vp = (struct fip_vn_desc *)desc;
			if (dlen < sizeof(*vp))
				return;
			if (compare_ether_addr(vp->fd_mac,
					       fip->get_src_addr(lport)) != 0 ||
			get_unaligned_be64(&vp->fd_wwpn) != lport->wwpn ||
			ntoh24(vp->fd_fc_id) !=
			fc_host_port_id(lport->host)) {
				LIBFCOE_FIP_DBG(fip, "incorrect Vx_Port Identification descriptor: "
					"fd_mac=%02x:%02x:%02x:%02x:%02x:%02x\n, fd_wwpn=%016llx, fd_fc_id=%02x%02x%02x\n", 
					vp->fd_mac[0], vp->fd_mac[1], vp->fd_mac[2], vp->fd_mac[3], vp->fd_mac[4], vp->fd_mac[5], 
					vp->fd_wwpn, vp->fd_fc_id[0], vp->fd_fc_id[1], vp->fd_fc_id[2]);
				return;
			}
			desc_mask &= ~BIT(FIP_DT_VN_ID);
			break;
		default:
			/* standard says ignore unknown descriptors >= 128 */
			if (desc->fip_dtype < FIP_DT_VENDOR_BASE)
				return;
			break;
		}
		desc = (struct fip_desc *)((char *)desc + dlen);
		rlen -= dlen;
	}

	/*
	 * reset only if all required descriptors were present and valid.
	 */
	if (desc_mask) {
		LIBFCOE_FIP_DBG(fip, "missing descriptors mask %x\n",
				desc_mask);
	} else {
		LIBFCOE_FIP_DBG(fip, "performing Clear Virtual Link\n");

#if defined(__VMKLNX__)
		spin_lock_bh(fip->ctlr_lock);
		FCOE_PER_CPU_PTR(lport->dev_stats,
		                 smp_processor_id(),
		                 sizeof(struct fcoe_dev_stats))->VLinkFailureCount++;
#else
		mutex_lock(&fip->ctlr_mutex);
		per_cpu_ptr(lport->dev_stats,
			    smp_processor_id())->VLinkFailureCount++;
#endif
		fcoe_ctlr_reset(fip);
#if defined(__VMKLNX__)
	        spin_unlock_bh(fip->ctlr_lock);
#else
	        mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		fc_lport_reset(fip->lp);

#if defined(__VMKLNX__)
		if (fip->vlan_id == FCOE_FIP_NO_VLAN_DISCOVERY) {
			fcoe_ctlr_solicit(fip, NULL);
		} else {
			fcoe_ctlr_vlan_request(fip);
		}
#else /* !defined(__VMKLNX__) */	
		fcoe_ctlr_solicit(fip, NULL);
#endif /* defined(__VMKLNX__) */
	}
}

/**
 * fcoe_ctlr_recv() - Receive a FIP packet
 * @fip: The FCoE controller that received the packet
 * @skb: The received FIP packet
 *
 * This may be called from either NET_RX_SOFTIRQ or IRQ.
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_recv */
void fcoe_ctlr_recv(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	skb_queue_tail(&fip->fip_recv_list, skb);
	schedule_work(&fip->recv_work);
}
EXPORT_SYMBOL(fcoe_ctlr_recv);

/**
 * fcoe_ctlr_recv_handler() - Receive a FIP frame
 * @fip: The FCoE controller that received the frame
 * @skb: The received FIP frame
 *
 * Returns non-zero if the frame is dropped.
 */
static int fcoe_ctlr_recv_handler(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	struct fip_header *fiph;
	struct ethhdr *eh;
	enum fip_state state;
	u16 op;
	u8 sub;

	if (skb_linearize(skb))
		goto drop;
	if (skb->len < sizeof(*fiph))
		goto drop;
	eh = eth_hdr(skb);
	if (fip->mode == FIP_MODE_VN2VN) {
		if (compare_ether_addr(eh->h_dest, fip->ctl_src_addr) &&
		    compare_ether_addr(eh->h_dest, fcoe_all_vn2vn) &&
		    compare_ether_addr(eh->h_dest, fcoe_all_p2p))
			goto drop;
	} else if (compare_ether_addr(eh->h_dest, fip->ctl_src_addr) &&
		   compare_ether_addr(eh->h_dest, fcoe_all_enode))
		goto drop;
	fiph = (struct fip_header *)skb->data;
	op = ntohs(fiph->fip_op);
	sub = fiph->fip_subcode;

	if (FIP_VER_DECAPS(fiph->fip_ver) != FIP_VER)
		goto drop;
	if (ntohs(fiph->fip_dl_len) * FIP_BPW + sizeof(*fiph) > skb->len)
		goto drop;

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	state = fip->state;
	if (state == FIP_ST_AUTO) {
		fip->map_dest = 0;
		fip->state = FIP_ST_ENABLED;
		state = FIP_ST_ENABLED;
		LIBFCOE_FIP_DBG(fip, "Using FIP mode\n");
	}
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)

	if (fip->mode == FIP_MODE_VN2VN && op == FIP_OP_VN2VN)
		return fcoe_ctlr_vn_recv(fip, skb);

	if (state != FIP_ST_ENABLED && state != FIP_ST_VNMP_UP &&
	    state != FIP_ST_VNMP_CLAIM)
		goto drop;
	if (op == FIP_OP_LS) {
		fcoe_ctlr_recv_els(fip, skb);	/* consumes skb */
		return 0;
	}

	if (state != FIP_ST_ENABLED)
		goto drop;

	if (op == FIP_OP_DISC && sub == FIP_SC_ADV)
		fcoe_ctlr_recv_adv(fip, skb);
	else if (op == FIP_OP_CTRL && sub == FIP_SC_CLR_VLINK)
		fcoe_ctlr_recv_clr_vlink(fip, fiph);
#if defined(__VMKLNX__)
	else if (op == FIP_OP_VLAN && sub == FIP_SC_VL_REP) {
		if (fip->vlan_id != FCOE_FIP_NO_VLAN_DISCOVERY) {
			fcoe_ctlr_recv_vlan_notification(fip, fiph);
		}
	}
#endif /* defined(__VMKLNX__) */	    
	kfree_skb(skb);
	return 0;
drop:
	kfree_skb(skb);
	return -1;
}

/**
 * fcoe_ctlr_select() - Select the best FCF (if possible)
 * @fip: The FCoE controller
 *
 * If there are conflicting advertisements, no FCF can be chosen.
 *
 * Called with lock held.
 */
static void fcoe_ctlr_select(struct fcoe_ctlr *fip)
{
	struct fcoe_fcf *fcf;
	struct fcoe_fcf *best = NULL;

	list_for_each_entry(fcf, &fip->fcfs, list) {
		LIBFCOE_FIP_DBG(fip, "consider FCF for fab %16.16llx "
				"VFID %d map %x val %d\n",
				fcf->fabric_name, fcf->vfid,
				fcf->fc_map, fcoe_ctlr_mtu_valid(fcf));
		if (!fcoe_ctlr_fcf_usable(fcf)) {
			LIBFCOE_FIP_DBG(fip, "FCF for fab %16.16llx "
					"map %x %svalid %savailable\n",
					fcf->fabric_name, fcf->fc_map,
					(fcf->flags & FIP_FL_SOL) ? "" : "in",
					(fcf->flags & FIP_FL_AVAIL) ?
					"" : "un");
			continue;
		}
		if (!best) {
			best = fcf;
			continue;
		}
		if (fcf->fabric_name != best->fabric_name ||
		    fcf->vfid != best->vfid ||
		    fcf->fc_map != best->fc_map) {
			LIBFCOE_FIP_DBG(fip, "Conflicting fabric, VFID, "
					"or FC-MAP\n");
			return;
		}
		if (fcf->pri < best->pri)
			best = fcf;
	}
	fip->sel_fcf = best;
}

/**
 * fcoe_ctlr_timeout() - FIP timeout handler
 * @arg: The FCoE controller that timed out
 *
 * Ages FCFs.  Triggers FCF selection if possible.  Sends keep-alives.
 */
static void fcoe_ctlr_timeout(unsigned long arg)
{
	struct fcoe_ctlr *fip = (struct fcoe_ctlr *)arg;
	struct fcoe_fcf *sel;
	struct fcoe_fcf *fcf;
	unsigned long next_timer = jiffies + msecs_to_jiffies(FIP_VN_KA_PERIOD);

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	if (fip->state == FIP_ST_DISABLED) {
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		return;
	}

#if defined(__VMKLNX__)
	/* handle FIP VLAN request timeout */
	if (0 == fip->vlan_id) {
		schedule_work(&fip->timer_work);
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		return;
	}
#endif /* defined(__VMKLNX__) */

	fcf = fip->sel_fcf;
	fcoe_ctlr_age_fcfs(fip);

	sel = fip->sel_fcf;
	if (!sel && fip->sel_time && time_after_eq(jiffies, fip->sel_time)) {
		fcoe_ctlr_select(fip);
		sel = fip->sel_fcf;
		fip->sel_time = 0;
	}

	if (sel != fcf) {
		fcf = sel;		/* the old FCF may have been freed */
		if (sel) {
#if defined(__VMKLNX__)
			printk(KERN_INFO "libfcoe: host%d: FIP selected "
			                 "Fibre-Channel Forwarder MAC "
			                 "%02x:%02x:%02x:%02x:%02x:%02x\n",
			                 fip->lp->host->host_no,
			                 sel->fcf_mac[0], sel->fcf_mac[1],
			                 sel->fcf_mac[2], sel->fcf_mac[3],
			                 sel->fcf_mac[4], sel->fcf_mac[5]);
#else /* !defined(__VMKLNX__) */
			printk(KERN_INFO "libfcoe: host%d: FIP selected "
			       "Fibre-Channel Forwarder MAC %pM\n",
			       fip->lp->host->host_no, sel->fcf_mac);
#endif /* defined(__VMKLNX__) */
			memcpy(fip->dest_addr, sel->fcf_mac, ETH_ALEN);
			fip->port_ka_time = jiffies +
				msecs_to_jiffies(FIP_VN_KA_PERIOD);
			fip->ctlr_ka_time = jiffies + sel->fka_period;
			if (time_after(next_timer, fip->ctlr_ka_time))
				next_timer = fip->ctlr_ka_time;
		} else {
			printk(KERN_NOTICE "libfcoe: host%d: "
			       "FIP Fibre-Channel Forwarder timed out.	"
			       "Starting FCF discovery.\n",
			       fip->lp->host->host_no);
			fip->reset_req = 1;
			schedule_work(&fip->timer_work);
		}
	}

	if (sel && !sel->fd_flags) {
		if (time_after_eq(jiffies, fip->ctlr_ka_time)) {
			fip->ctlr_ka_time = jiffies + sel->fka_period;
			fip->send_ctlr_ka = 1;
		}
		if (time_after(next_timer, fip->ctlr_ka_time))
			next_timer = fip->ctlr_ka_time;

		if (time_after_eq(jiffies, fip->port_ka_time)) {
			fip->port_ka_time = jiffies +
				msecs_to_jiffies(FIP_VN_KA_PERIOD);
			fip->send_port_ka = 1;
		}
		if (time_after(next_timer, fip->port_ka_time))
			next_timer = fip->port_ka_time;
		mod_timer(&fip->timer, next_timer);
	} else if (fip->sel_time) {
		next_timer = fip->sel_time +
			msecs_to_jiffies(FCOE_CTLR_START_DELAY);
		mod_timer(&fip->timer, next_timer);
	}
	if (fip->send_ctlr_ka || fip->send_port_ka)
		schedule_work(&fip->timer_work);
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
}

#if defined(__VMKLNX__)
/**
 * fcoe_ctlr_l2_link_ok() - Check if L2 link is OK for the local port
 * @lport: The local port to check link on
 *
 * Returns: TRUE if L2 link is OK, FALSE otherwise
 */
static inline int fcoe_ctlr_l2_link_ok(struct fc_lport *lport)
{
        struct net_device *netdev = NULL;
                                
        if (lport->tt.get_cna_netdev) {
            netdev = lport->tt.get_cna_netdev(lport);
            VMK_ASSERT(netdev);
        }
                
	if (netdev && (netdev->flags & IFF_UP) && netif_carrier_ok(netdev)) {
		return TRUE;
	}

	return FALSE;
}
#endif /* defined(__VMKLNX__) */

/**
 * fcoe_ctlr_timer_work() - Worker thread function for timer work
 * @work: Handle to a FCoE controller
 *
 * Sends keep-alives and resets which must not
 * be called from the timer directly, since they use a mutex.
 */
static void fcoe_ctlr_timer_work(struct work_struct *work)
{
	struct fcoe_ctlr *fip;
	struct fc_lport *vport;
	u8 *mac;
	int reset;

	fip = container_of(work, struct fcoe_ctlr, timer_work);
	if (fip->mode == FIP_MODE_VN2VN) {
		fcoe_ctlr_vn_timeout(fip);
		return;
	}
#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	reset = fip->reset_req;
	fip->reset_req = 0;
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)


	if (reset) {
		fc_lport_reset(fip->lp);
#if defined(__VMKLNX__)
		/*
		 * In Linux, FIP VLAN discovery is managed in userland.
		 * For ESX, we drive VLAN discovery directly as we have
		 * no userland agent for this purpose.
		 */
		if (fip->vlan_id ==  FCOE_FIP_NO_VLAN_DISCOVERY) {
			fcoe_ctlr_solicit(fip, NULL);
		} else {
			fcoe_ctlr_vlan_request(fip);
		}
	} else if (fcoe_ctlr_l2_link_ok(fip->lp) && (0 == fip->vlan_id)) {
		/* If L2 link is up, keep retrying VLAN discovery */
		LIBFCOE_FIP_DBG(fip, "host%u: FIP VLAN ID unavail. "
				"Retry VLAN discovery.\n",
				fip->lp->host->host_no);
		fcoe_ctlr_vlan_request(fip);
#endif /* defined(__VMKLNX__) */
	}

	if (fip->send_ctlr_ka) {
		fip->send_ctlr_ka = 0;
		fcoe_ctlr_send_keep_alive(fip, fip->lp, 0, fip->ctl_src_addr);
	}
	if (fip->send_port_ka) {
		fip->send_port_ka = 0;
		mutex_lock(&fip->lp->lp_mutex);
		mac = fip->get_src_addr(fip->lp);
		fcoe_ctlr_send_keep_alive(fip, fip->lp, 1, mac);
		list_for_each_entry(vport, &fip->lp->vports, list) {
			mac = fip->get_src_addr(vport);
			fcoe_ctlr_send_keep_alive(fip, vport, 1, mac);
		}
		mutex_unlock(&fip->lp->lp_mutex);
	}
}

/**
 * fcoe_ctlr_recv_work() - Worker thread function for receiving FIP frames
 * @recv_work: Handle to a FCoE controller
 */
static void fcoe_ctlr_recv_work(struct work_struct *recv_work)
{
	struct fcoe_ctlr *fip;
	struct sk_buff *skb;

	fip = container_of(recv_work, struct fcoe_ctlr, recv_work);
	while ((skb = skb_dequeue(&fip->fip_recv_list)))
		fcoe_ctlr_recv_handler(fip, skb);
}

/**
 * fcoe_ctlr_recv_flogi() - Snoop pre-FIP receipt of FLOGI response
 * @fip: The FCoE controller
 * @lport: not in use
 * @fp:	 The FC frame to snoop
 *
 * Snoop potential response to FLOGI or even incoming FLOGI.
 *
 * The caller has checked that we are waiting for login as indicated
 * by fip->flogi_oxid != FC_XID_UNKNOWN.
 *
 * The caller is responsible for freeing the frame.
 * Fill in the granted_mac address.
 *
 * Return non-zero if the frame should not be delivered to libfc.
 */
/* _VMKLNX_CODECHECK_: fcoe_ctlr_recv_flogi */
int fcoe_ctlr_recv_flogi(struct fcoe_ctlr *fip, struct fc_lport *lport,
			 struct fc_frame *fp)
{
	struct fc_frame_header *fh;
	u8 op;
	u8 *sa;

	sa = eth_hdr(&fp->skb)->h_source;
	fh = fc_frame_header_get(fp);
	if (fh->fh_type != FC_TYPE_ELS)
		return 0;

	op = fc_frame_payload_op(fp);
	if (op == ELS_LS_ACC && fh->fh_r_ctl == FC_RCTL_ELS_REP &&
	    fip->flogi_oxid == ntohs(fh->fh_ox_id)) {

#if defined(__VMKLNX__)
		spin_lock_bh(fip->ctlr_lock);
#else
		mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		if (fip->state != FIP_ST_AUTO && fip->state != FIP_ST_NON_FIP) {
#if defined(__VMKLNX__)
			spin_unlock_bh(fip->ctlr_lock);
#else
			mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
			return -EINVAL;
		}
		fip->state = FIP_ST_NON_FIP;
		LIBFCOE_FIP_DBG(fip,
				"received FLOGI LS_ACC using non-FIP mode\n");

		/*
		 * FLOGI accepted.
		 * If the src mac addr is FC_OUI-based, then we mark the
		 * address_mode flag to use FC_OUI-based Ethernet DA.
		 * Otherwise we use the FCoE gateway addr
		 */
		if (!compare_ether_addr(sa, (u8[6])FC_FCOE_FLOGI_MAC)) {
			fcoe_ctlr_map_dest(fip);
		} else {
			memcpy(fip->dest_addr, sa, ETH_ALEN);
			fip->map_dest = 0;
		}
		fip->flogi_oxid = FC_XID_UNKNOWN;
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		fc_fcoe_set_mac(fr_cb(fp)->granted_mac, fh->fh_d_id);
	} else if (op == ELS_FLOGI && fh->fh_r_ctl == FC_RCTL_ELS_REQ && sa) {
		/*
		 * Save source MAC for point-to-point responses.
		 */
#if defined(__VMKLNX__)
		spin_lock_bh(fip->ctlr_lock);
#else
		mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
		if (fip->state == FIP_ST_AUTO || fip->state == FIP_ST_NON_FIP) {
			memcpy(fip->dest_addr, sa, ETH_ALEN);
			fip->map_dest = 0;
			if (fip->state == FIP_ST_AUTO)
				LIBFCOE_FIP_DBG(fip, "received non-FIP FLOGI. "
						"Setting non-FIP mode\n");
			fip->state = FIP_ST_NON_FIP;
		}
#if defined(__VMKLNX__)
		spin_unlock_bh(fip->ctlr_lock);
#else
		mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	}
	return 0;
}
EXPORT_SYMBOL(fcoe_ctlr_recv_flogi);

/**
 * fcoe_wwn_from_mac() - Converts a 48-bit IEEE MAC address to a 64-bit FC WWN
 * @mac:    The MAC address to convert
 * @scheme: The scheme to use when converting
 * @port:   The port indicator for converting
 *
 * Returns: u64 fc world wide name
 */
/* _VMKLNX_CODECHECK_: fcoe_wwn_from_mac */
u64 fcoe_wwn_from_mac(unsigned char mac[MAX_ADDR_LEN],
		      unsigned int scheme, unsigned int port)
{
	u64 wwn;
	u64 host_mac;

	/* The MAC is in NO, so flip only the low 48 bits */
	host_mac = ((u64) mac[0] << 40) |
		((u64) mac[1] << 32) |
		((u64) mac[2] << 24) |
		((u64) mac[3] << 16) |
		((u64) mac[4] << 8) |
		(u64) mac[5];

	WARN_ON(host_mac >= (1ULL << 48));
	wwn = host_mac | ((u64) scheme << 60);
	switch (scheme) {
	case 1:
		WARN_ON(port != 0);
		break;
	case 2:
		WARN_ON(port >= 0xfff);
		wwn |= (u64) port << 48;
		break;
	default:
		WARN_ON(1);
		break;
	}

	return wwn;
}
EXPORT_SYMBOL_GPL(fcoe_wwn_from_mac);

/**
 * fcoe_libfc_config() - Sets up libfc related properties for local port
 * @lp: The local port to configure libfc for
// * @fip: The FCoE controller in use by the local port
 * @tt: The libfc function template
 *
 * Returns : 0 for success
 */
/* _VMKLNX_CODECHECK_: fcoe_libfc_config */
int fcoe_libfc_config(struct fc_lport *lport,
		      const struct libfc_function_template *tt)
{
	/* Set the function pointers set by the LLDD */
	memcpy(&lport->tt, tt, sizeof(*tt));
	if (fc_fcp_init(lport))
		return -ENOMEM;
	fc_exch_init(lport);
	fc_elsct_init(lport);
	fc_lport_init(lport);
	fc_rport_init(lport);
	fc_disc_init(lport);

	return 0;
}
EXPORT_SYMBOL_GPL(fcoe_libfc_config);

#if defined(__VMKLNX__)
/**
 * fcoe_ctlr_vlan_request() - Send FIP VLAN request.
 * @fip: The FCoE controller
 *
 * Return 0 on success; -1 on failure. 
 */
static int fcoe_ctlr_vlan_request(struct fcoe_ctlr *fip)
{
	struct sk_buff *skb;
	struct fip_vlan_req {
		struct ethhdr eth;
		struct fip_header fip;
		struct fip_mac_desc mac;
	}  __attribute__((packed)) *vlan_req;
    
	struct net_device *netdev;

	VMK_ASSERT(fip->vlan_id != FCOE_FIP_NO_VLAN_DISCOVERY);
    
	skb = dev_alloc_skb(sizeof(*vlan_req));
	if (!skb) {
		LIBFCOE_FIP_DBG(fip, "Cannot allocate skb\n");
		return -1;
	}
    
	vlan_req = (struct fip_vlan_req *)skb->data;
	memset(vlan_req, 0, sizeof(*vlan_req));
	memcpy(vlan_req->eth.h_dest, FIP_ALL_FCF_MACS, ETH_ALEN);
	memcpy(vlan_req->eth.h_source, fip->ctl_src_addr, ETH_ALEN);
	vlan_req->eth.h_proto = htons(ETH_P_FIP);

	vlan_req->fip.fip_ver = FIP_VER_ENCAPS(FIP_VER);
	vlan_req->fip.fip_op = htons(FIP_OP_VLAN);
	vlan_req->fip.fip_subcode = FIP_SC_VL_REQ;
	vlan_req->fip.fip_dl_len = htons(sizeof(vlan_req->mac) / FIP_BPW);
	vlan_req->fip.fip_flags = 0;
    
	vlan_req->mac.fd_desc.fip_dtype = FIP_DT_MAC;
	vlan_req->mac.fd_desc.fip_dlen = sizeof(vlan_req->mac) / FIP_BPW;
	memcpy(vlan_req->mac.fd_mac, fip->ctl_src_addr, ETH_ALEN);	
    
	skb_put(skb, sizeof(*vlan_req));
	skb->protocol = htons(ETH_P_FIP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
    
	fip->vlan_id = 0;
    
	if ((fip->lp)->tt.get_cna_netdev) {
		netdev = (fip->lp)->tt.get_cna_netdev(fip->lp);
		VMK_ASSERT(netdev);
		if (vmklnx_cna_set_vlan_tag(netdev, 0) == -1) {
			LIBFCOE_FIP_DBG(fip, "%s: vmklnx_cna_set_vlan_tag() "
					"failed\n", __FUNCTION__);
		    kfree_skb(skb);
		    return -1;
		}
	}

	fip->send(fip, skb);

#define VLAN_DISC_RETRY_TMO  2000
	mod_timer(&fip->timer, jiffies + msecs_to_jiffies(VLAN_DISC_RETRY_TMO));
    
	LIBFCOE_FIP_DBG(fip, "fcoe_ctlr_vlan_request() is done\n");
	return 0;
}

/**
 * fcoe_ctlr_recv_vlan_notification() - Handle FIP VLAN notification.
 * @fip: The FCoE controller
 * @fh: FIP header
 * 
 * Return 0 on success; -1 on failure.
 */
static int fcoe_ctlr_recv_vlan_notification(struct fcoe_ctlr *fip, struct fip_header *fh)
{
	struct fip_desc *desc;
	struct fip_mac_desc *mp;
	struct fip_vlan_desc *vp;
	size_t rlen;
	size_t dlen;
	u32 desc_mask;
	u16 fd_vlan;
	u16 new_vlan = 0;
	int old_vlan_valid = 0;
    
	LIBFCOE_FIP_DBG(fip, "FIP VLAN notification received\n");

	/*
	 * mask of required descriptors.  Validating each one clears its bit.
	 */
	desc_mask = BIT(FIP_DT_MAC) | BIT(FIP_DT_VLAN);

	rlen = ntohs(fh->fip_dl_len) * FIP_BPW;
	desc = (struct fip_desc *)(fh + 1);
	while (rlen >= sizeof(*desc)) {
		dlen = desc->fip_dlen * FIP_BPW;
		if (dlen > rlen) {
			LIBFCOE_FIP_DBG(fip, "invalid dlen (%lu), larger than rlen (%lu)\n",
				dlen, rlen );
			return -1;
		}
	
		switch (desc->fip_dtype) {
		case FIP_DT_MAC:    
			mp = (struct fip_mac_desc *)desc;
			if (dlen < sizeof(*mp)) {
				LIBFCOE_FIP_DBG(fip, "invalid dlen (%lu) in "
					"FIP_DT_MAC descriptor\n", dlen );
				return -1;
			}

			LIBFCOE_FIP_DBG(fip, "fd_mac=%x:%x:%x:%x:%x:%x\n", 
				mp->fd_mac[0], mp->fd_mac[1], mp->fd_mac[2], 
				mp->fd_mac[3], mp->fd_mac[4], mp->fd_mac[5] );
		
			desc_mask &= ~BIT(FIP_DT_MAC);	
			break;
		case FIP_DT_VLAN:
			vp = (struct fip_vlan_desc *)desc;
			if (dlen < sizeof(*vp)) {
				LIBFCOE_FIP_DBG(fip, "invalid dlen (%lu) in "
					"FIP_DT_VLAN descriptor\n", dlen );
				return -1;
			}
	    
			fd_vlan = ntohs(vp->fd_vlan);   
			LIBFCOE_FIP_DBG(fip, "fd_vlan=%d\n", fd_vlan );
	    
			if (fd_vlan == fip->vlan_id) {
				old_vlan_valid = 1;
			} else if (0 == new_vlan) {
				new_vlan = fd_vlan;
			}
	
			desc_mask &= ~BIT(FIP_DT_VLAN);	
			break;
		default:
			/* standard says ignore unknown descriptors >= 128 */
			if (desc->fip_dtype < FIP_DT_VENDOR_BASE) {
				LIBFCOE_FIP_DBG(fip, "invalid fip_dtype (%d)\n",
					desc->fip_dtype );
			return -1;
			}
	    
			break;
		}
		desc = (struct fip_desc *)((char *)desc + dlen);
		rlen -= dlen;
	}

	if (desc_mask) {
		LIBFCOE_FIP_DBG(fip, "missing descriptors mask %x\n",
			desc_mask);
		return -1;
	}

	if (!old_vlan_valid && (new_vlan != 0)) {
		struct net_device *netdev;

		if (new_vlan > VLAN_MAX_VALID_VID) {
			LIBFCOE_FIP_DBG(fip, "invalid vlan id %d, ignored\n", new_vlan);
			return -1;
		}

		fip->vlan_id = new_vlan;
    
		if ((fip->lp)->tt.get_cna_netdev) {
			netdev = (fip->lp)->tt.get_cna_netdev(fip->lp);
			VMK_ASSERT(netdev);
			if (vmklnx_cna_set_vlan_tag(netdev, fip->vlan_id)
				== -1) {
				LIBFCOE_FIP_DBG(fip,
				    "%s: vmklnx_cna_set_vlan_tag() failed\n",
				    __FUNCTION__);
				return -1;
			}
		}

		fcoe_ctlr_solicit(fip, NULL);
	}

	return 0;
}

#endif /* defined(__VMKLNX__) */
