/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#include "bnx2fc.h"

static struct list_head adapter_list;
static u32 adapter_count;
static volatile u32 in_prog;
atomic_t bnx2fc_reg_device = ATOMIC_INIT(0);
static DEFINE_MUTEX(bnx2fc_dev_lock);
DEFINE_PER_CPU(struct fcoe_percpu_s, bnx2fc_percpu);

#define DRV_MODULE_NAME		"bnx2fc"
#define DRV_MODULE_VERSION	BNX2FC_VERSION
#define DRV_MODULE_RELDATE	"May 21, 2014"
static char version[] __devinitdata =
		"QLogic NetXtreme II FCoE Driver " DRV_MODULE_NAME \
		" v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic NetXtreme II BCM57712 FCoE Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

unsigned long bnx2fc_debug_level = 0x00;
module_param(bnx2fc_debug_level, long, 0644);

/* Option to select vmk_MemPool vs pci_consistent()
 * 0 - pci_alloc_consistent - Default.
 * 1 - vmk_MemPool
 */
#define BNX2FC_EN_VMKPOOL	1
unsigned long bnx2fc_vmk_pool = 1;
module_param(bnx2fc_vmk_pool, long, 0644);
MODULE_PARM_DESC(bnx2fc_vmk_pool, "parameter to enable/disable vmk_memPool\n");

unsigned long bnx2fc_enable_cvl_fix = 1;
module_param(bnx2fc_enable_cvl_fix, long, 0644);
MODULE_PARM_DESC(bnx2fc_enable_cvl_fix, "parameter to enable/disable insertion of VxPort ID CVL\n");

#define FCOE_MAX_QUEUE_DEPTH 256
#define FCOE_LOW_QUEUE_DEPTH	32
#define FCOE_GW_ADDR_MODE           0x00
#define FCOE_FCOUI_ADDR_MODE        0x01

#define FCOE_WORD_TO_BYTE  4

static struct bnx2fc_hba *bnx2fc_hba_lookup(struct net_device *phys_dev);
#ifdef __VMKLNX__
static struct fc_host_statistics *bnx2fc_get_host_stats(struct Scsi_Host *);
static int bnx2fc_rcv(struct sk_buff *, struct net_device *,
		struct packet_type *, struct net_device *);
struct bnx2fc_hba *bnx2fc_find_hba_for_dev(struct net_device *dev);
static void bnx2fc_restart_fcoe_disc(struct bnx2fc_hba *hba);

static int bnx2fc_fcoe_create(struct net_device *netdev);
static int bnx2fc_fcoe_destroy(struct net_device *netdev);
static int bnx2fc_fcoe_wakeup(struct net_device *netdev);
static int bnx2fc_fcoe_recv(struct sk_buff *skb);
static int bnx2fc_fip_recv(struct sk_buff *skb);
#if defined(__FIP_VLAN_DISC_CMPL__)
static int bnx2fc_vlan_disc_cmpl(struct net_device *netdev);
#endif
static u8 *bnx2fc_get_src_mac(struct fc_lport *lport);

static int bnx2fc_fcoe_xport_registered;

enum bnx2x_board_type {
	BCM57712 = 0,
	BCM57712E = 1,
	BCM57800 = 2,
	BCM57800MF = 3,
	BCM57810 = 4,
	BCM57810MF = 5,
	BCM57840_4X10 = 6,
	BCM57840_2X20 = 7,
	BCM57840MF = 8,
};

static DEFINE_PCI_DEVICE_TABLE(bnx2fc_pci_tbl) = {
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57712), BCM57712 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57712E), BCM57712E },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57800), BCM57800 },
	{ PCI_VDEVICE(BROADCOM,PCI_DEVICE_ID_NX2_57800_MF), BCM57800MF },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57810), BCM57810 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57810_MF), BCM57810MF },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57840_4_10), BCM57840_4X10 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57840_2_20), BCM57840_2X20 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57840_MF), BCM57840MF },
	{ 0 }
};

static struct vmklnx_fcoe_template bnx2fc_fcoe_template = {
    .name		= "fcoe_brcm_esx",
    .fcoe_create	= bnx2fc_fcoe_create,
    .fcoe_destroy	= bnx2fc_fcoe_destroy,
    .id_table		= bnx2fc_pci_tbl,
    .fcoe_wakeup	= bnx2fc_fcoe_wakeup,
    .fcoe_recv		= bnx2fc_fcoe_recv,
    .fip_recv		= bnx2fc_fip_recv,
#if defined(__FIP_VLAN_DISC_CMPL__)
    .fcoe_vlan_disc_cmpl= bnx2fc_vlan_disc_cmpl,
#endif
};

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION == 50000)
/**
 * is_fip_mode() - returns true if FIP mode selected.
 * @fip:        FCoE controller.
 */
static inline bool is_fip_mode(struct fcoe_ctlr *fip)
{
	return fip->state == FIP_ST_ENABLED;
}
#endif

#if defined(__FIP_VLAN_DISC_CMPL__)
static int bnx2fc_vlan_disc_cmpl(struct net_device *cna_dev)
{
	struct net_device *netdev = dev_get_by_name(cna_dev->name);
	struct bnx2fc_hba *hba = NULL;
	struct cnic_dev *cnic_dev;

	if (!netdev)
		return -ENODEV;

	hba = bnx2fc_hba_lookup(netdev);
	if (!hba) {
		printk(KERN_ERR PFX "vlan_disc_cmpl: hba not found\n");
		return -ENODEV;
	}

	cnic_dev = hba->cnic;
	if (cnic_dev->mf_mode == MULTI_FUNCTION_SD) {
		hba->vlan_id = 0;
		clear_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
	} else  {
#ifdef __FCOE_ENABLE_BOOT__
		hba->vlan_id = vmklnx_cna_get_vlan_tag(cna_dev) & VLAN_VID_MASK;
#else
		hba->vlan_id = vmklnx_cna_get_vlan_tag(cna_dev);
#endif
		set_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
	}

	printk(KERN_ERR "%s: vlan_disc_cmpl: hba is on vlan_id %d\n",
	       hba->netdev->name, hba->vlan_id);

	return 0;
}

#endif

static int bnx2fc_fcoe_wakeup(struct net_device *netdev)
{
	return 0;
}


static int bnx2fc_drop_packet_vlan(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *) netdev->fcoe_ptr;
	struct cnic_dev *cdev;
	struct vlan_group *vlgrp;
	u16 vlan_tag;

	bnx2fc_dbg(LOG_FRAME, "vlan_rx_tag_present():%d\n",
		   vlan_rx_tag_present(skb));

	if (vlan_rx_tag_present(skb))
		vlan_tag = vlan_rx_tag_get(skb);
	else
		return 0;

	if (!hba)
		return 0;
	
	cdev = hba->cnic;
	
	if (cdev && cdev->cna_vlgrp)
		vlgrp = *cdev->cna_vlgrp;
	else {
		hba->vlan_grp_unset_drop_stat++;
		return -EIO;
	}

	if (vlgrp && vlgrp->vlan_devices[vlan_tag & VLAN_VID_MASK])
		return 0;
	else {
		hba->vlan_invalid_drop_stat++;
		return -EINVAL;
	}
}

static int bnx2fc_fcoe_recv(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *) netdev->fcoe_ptr;
	struct packet_type *ptype = NULL;

	bnx2fc_dbg(LOG_FRAME, "skb %p, hba %p\n", skb, hba);
	if (!hba) {
		kfree_skb(skb);
		return 0;
	}

	if (bnx2fc_drop_packet_vlan(skb) != 0) {
		kfree_skb(skb);
		return 0;
	}

	ptype = (struct packet_type *) &(hba->fcoe_packet_type);
	return bnx2fc_rcv(skb, netdev, ptype, netdev);
}

static int bnx2fc_ffa_get_fcoe_mac(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)netdev->fcoe_ptr;
	struct fip_header *fiph;
	struct fip_desc *desc;
	int desc_size;
	int dlen;
        u16 opcode;
	u8 sub_code;
	int flogi_resp = 0;
	int desc_cnt;

	fiph = (struct fip_header *)skb->data;
	opcode = ntohs(fiph->fip_op);
	sub_code = fiph->fip_subcode;

	if ((opcode == FIP_OP_LS) && (sub_code == FIP_SC_REP)) {
		desc_size = ntohs(fiph->fip_dl_len) * FIP_BPW;
		desc = (struct fip_desc *)(fiph + 1);

		desc_cnt = 0;
		while ((desc_size >= sizeof(*desc)) &&
		       (desc_size >= desc->fip_dlen * FIP_BPW)) {
			dlen = desc->fip_dlen * FIP_BPW;
			desc_cnt++;
			switch (desc->fip_dtype) {
			case FIP_DT_FLOGI:
				flogi_resp = 1;
				break;
			case FIP_DT_MAC:
				if (flogi_resp == 1) {
					struct fip_mac_desc *mac_desc =
						(struct fip_mac_desc *)desc;
					if (desc_cnt == 2) {
						memcpy(hba->granted_mac,
						       mac_desc->fd_mac, ETH_ALEN);
						printk(KERN_ERR PFX "Granted MAC :"
							"%02x:%02x:%02x:%02x:"
							"%02x:%02x\n",
							hba->granted_mac[0],
							hba->granted_mac[1],
							hba->granted_mac[2],
							hba->granted_mac[3],
							hba->granted_mac[4],
							hba->granted_mac[5]);
					} else if (desc_cnt == 3) {
						memcpy(hba->ffa_fcoe_mac,
						       mac_desc->fd_mac, ETH_ALEN);
						memcpy(mac_desc->fd_mac,
						       hba->granted_mac, ETH_ALEN);
						set_bit(BNX2FC_ADAPTER_FFA, &hba->adapter_type);
						printk(KERN_ERR PFX "FFA FCOE_MAC :"
							"%02x:%02x:%02x:%02x:"
							"%02x:%02x\n",
							hba->ffa_fcoe_mac[0],
							hba->ffa_fcoe_mac[1],
							hba->ffa_fcoe_mac[2],
							hba->ffa_fcoe_mac[3],
							hba->ffa_fcoe_mac[4],
							hba->ffa_fcoe_mac[5]);
					}
				}
				break;
			default:
				;
			}	/* switch */
			desc = (struct fip_desc *)((char *)desc + dlen);
			desc_size -= dlen;
		}		/* while */
	}			/* if */

	return 0;
}

static void bnx2fc_copy_skb_header(struct sk_buff *new,
				   const struct sk_buff *old)
{
	unsigned long offset = new->data - old->data;

	new->dev	= old->dev;
	new->priority	= old->priority;
	new->protocol	= old->protocol;

	new->h.raw	= old->h.raw + offset;
	new->nh.raw	= old->nh.raw + offset;
	new->mac.raw	= old->mac.raw + offset;

	new->csum	= old->csum;
	new->ip_summed	= old->ip_summed;
}

struct sk_buff *bnx2fc_skb_copy_expand(const struct sk_buff *skb,
				       int newheadroom, int newtailroom,
				       gfp_t gfp_mask)
{
	/* Allocate the copy buffer
	 */
	struct sk_buff *n = __alloc_skb(newheadroom + skb->len + newtailroom,
					gfp_mask, 0);
	int oldheadroom = skb_headroom(skb);
	int head_copy_len, head_copy_off;

	if (!n)
		return NULL;

	skb_reserve(n, newheadroom);

	/* Set the tail pointer and length */
	skb_put(n, skb->len);

	head_copy_len = oldheadroom;
	head_copy_off = 0;
	if (newheadroom <= head_copy_len)
		head_copy_len = newheadroom;
	else
		head_copy_off = newheadroom - head_copy_len;

	/* Copy the linear header and data. */
	if (skb_copy_bits(skb, -head_copy_len, n->head + head_copy_off,
			  skb->len + head_copy_len))
		BUG();

	bnx2fc_copy_skb_header(n, skb);

	return n;
}


static struct sk_buff * bnx2fc_validate_fip_cvl(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)netdev->fcoe_ptr;
	struct fip_header *fiph;
	struct fip_desc *desc;
	u32 desc_mask;
	int desc_size;
	int desc_len;
	int num_of_desc = 0;
	u16 opcode;
	u8 sub_code;

	/* Eventhough this issue is seen in a particular OEM platform,
	 * this is still a generic issue. So the decision is apply this
	 * fix for all
	if (!test_bit(BNX2FC_ADAPTER_FFA, &hba->adapter_type))
		return;
	*/

	fiph = (struct fip_header *)skb->data;
	opcode = ntohs(fiph->fip_op);
	sub_code = fiph->fip_subcode;

	if (!((opcode == FIP_OP_CTRL) && (sub_code == FIP_SC_CLR_VLINK)))
			return skb;

	desc_size = ntohs(fiph->fip_dl_len) * FIP_BPW;
	desc = (struct fip_desc *)(fiph + 1);
	desc_mask = BIT(FIP_DT_MAC) | BIT(FIP_DT_NAME) | BIT(FIP_DT_VN_ID);
	if (desc_mask) {
		if (unlikely(skb->tail + sizeof(struct fip_vn_desc) >
			     skb->end)) {
			struct sk_buff *skb_tmp;

			hba->ffa_cvl_fix_alloc++;
			skb_tmp = bnx2fc_skb_copy_expand(skb,
						  0, sizeof(struct fip_vn_desc),
						  GFP_ATOMIC);
			if (skb_tmp == NULL) {
				hba->ffa_cvl_fix_alloc_err++;
				return skb;
			}

			kfree_skb(skb);
			skb = skb_tmp;
			fiph = (struct fip_header *)skb->data;
			desc = (struct fip_desc *)(fiph + 1);
		}
	}

	while (desc_size >= sizeof(*desc)) {
		desc_len = desc->fip_dlen * FIP_BPW;
		if (desc_len > desc_size)
			return skb;

		switch (desc->fip_dtype) {
		case FIP_DT_MAC:
			desc_mask &= ~BIT(FIP_DT_MAC);
			break;
		case FIP_DT_NAME:
			desc_mask &= ~BIT(FIP_DT_NAME);
			break;
		case FIP_DT_VN_ID:
			desc_mask &= ~BIT(FIP_DT_VN_ID);
			break;
		default:
			break;
		}
		desc = (struct fip_desc *)((char *)desc + desc_len);
		desc_size -= desc_len;
		num_of_desc++;
	}

	if (desc_mask) {
		/* add VN_ID descriptor to the frame */
		struct fip_vn_desc vn_id_desc = { 0 };
		struct fc_lport *lp = hba->ctlr.lp;
		DECLARE_MAC_BUF(mac);

		bnx2fc_dbg(LOG_FRAME, "FIP: FFA Adapter - desc_mask 0x%x\n",
			   desc_mask);
		if (desc_mask & BIT(FIP_DT_VN_ID))
			bnx2fc_dbg(LOG_FRAME, "FIP: missing Vx_PORT ID Desc\n");

		vn_id_desc.fd_desc.fip_dlen = sizeof(vn_id_desc) / FIP_BPW;
		vn_id_desc.fd_desc.fip_dtype = FIP_DT_VN_ID;
		memcpy(vn_id_desc.fd_mac, bnx2fc_get_src_mac(lp), ETH_ALEN);
		put_unaligned_be64(lp->wwpn, &vn_id_desc.fd_wwpn);
		hton24(vn_id_desc.fd_fc_id, fc_host_port_id(lp->host));
		bnx2fc_dbg(LOG_FRAME, "preparing VN desc: mac %s id:0x%x%x%x\n",
			   print_mac(mac, vn_id_desc.fd_mac),
			   vn_id_desc.fd_fc_id[0], vn_id_desc.fd_fc_id[1],
			   vn_id_desc.fd_fc_id[2]);

		/* Assumption is there is enough tail room to stuff in
		 * VN_ID descriptor. If not we need to clone the skb and
		 * send it up. We shall not hit this case RX BUF size = 2500B
		 * and CVL is < 100B.
		 */
		memcpy(desc, &vn_id_desc, sizeof(vn_id_desc));
		fiph->fip_dl_len = htons(ntohs(fiph->fip_dl_len) + (sizeof(struct fip_vn_desc) / FIP_BPW));

		hba->ffa_cvl_fix++;
		printk(KERN_ERR PFX "FIP: FFA Adapter : vn_id_desc.fip_dlen %d, fiph->fip_dl_len %d,\n", vn_id_desc.fd_desc.fip_dlen, fiph->fip_dl_len);
		skb_put(skb, sizeof(struct fip_vn_desc));
	}

	return skb;
}

static int bnx2fc_validate_fip_vlan_notify(struct sk_buff *skb)
{
	struct fip_vlan_desc *vlan_desc;
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)netdev->fcoe_ptr;
        struct fip_header *fiph;
        struct fip_desc *desc;
	int desc_size;
	int dlen;
        u16 opcode;
        u8 sub_code;
	int vlan_valid = 0;
	int vlan_desc_cnt = 0;
	u16 vlan_id;

        fiph = (struct fip_header *)skb->data;
        opcode = ntohs(fiph->fip_op);
        sub_code = fiph->fip_subcode;

	if ((opcode == FIP_OP_VLAN) && (sub_code == FIP_SC_VL_REP)) {
		desc_size = ntohs(fiph->fip_dl_len) * FIP_BPW;
        	desc = (struct fip_desc *)(fiph + 1);

		while ((desc_size >= sizeof(*desc)) &&
		       (desc_size >= desc->fip_dlen * FIP_BPW)) {
			dlen = desc->fip_dlen * FIP_BPW;
			switch (desc->fip_dtype) {
			case FIP_DT_VLAN:
				if (dlen < sizeof(struct fip_vlan_desc)) {
					goto frame_err;
				}
				vlan_desc_cnt++;
				vlan_desc = (struct fip_vlan_desc *)desc;
				vlan_id = ntohs(vlan_desc->fd_vlan);

				printk(KERN_ERR PFX "%s: %s vlan_id: %d, vlan_desc_cnt: %d\n", __FUNCTION__, netdev->name, vlan_id, vlan_desc_cnt);

				/* Drop VID=4095, ESX cannot handle it as 4095
				 * has special meaning in ESX. This issue is
				 * tracked by PR# 747835
				 */
				if (vlan_id == 4095) {
					printk(KERN_ERR PFX "%s : FCF allocated"
						" invalid VID 4095\n",
						netdev->name);
					vlan_valid = 0;
					goto frame_err;
				} else if (vlan_id == 0) {
					struct cnic_dev *cnic_dev = hba->cnic;
					if (cnic_dev->mf_mode == MULTI_FUNCTION_SD) {
						vlan_id = htons(cnic_dev->e1hov_tag & VLAN_VID_MASK);
						vlan_desc->fd_vlan = vlan_id;
						vlan_valid = 1;
					} else {
						vlan_valid = 0;
						goto frame_err;
					}
				} else
					vlan_valid = 1;
				break;
			default:
				;
			}	/* switch */
			desc = (struct fip_desc *)((char *)desc + dlen);
			desc_size -= dlen;
		}		/* while */

		if (vlan_desc_cnt == 1 && vlan_valid)
			printk(KERN_ERR PFX "%s : Valid VID 0x%x\n", netdev->name, vlan_id);
		else
			goto frame_err;

		printk(KERN_ERR PFX "%s: %s vlan_id: %d SUCCESS\n",  __FUNCTION__, netdev->name, vlan_id);
	}			/* if */

	return 0;

frame_err:
	printk(KERN_ERR PFX "%s: %s FAIL\n",  __FUNCTION__, netdev->name);
	return -EINVAL;
}

static int bnx2fc_fip_recv(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *) netdev->fcoe_ptr;
	int rc;

	bnx2fc_dbg(LOG_FRAME, "skb %p, skb len: %d\n", skb, skb->len);

	rc = bnx2fc_drop_packet_vlan(skb);
	if (rc) {
		bnx2fc_dbg(LOG_FRAME, "%s: drop skb %p, reason %d\n",
			   netdev->name, skb, rc);
		kfree_skb(skb);
		return rc;
	}

	/* TODO : check if the device is HP-FFA and only then call this function */
	bnx2fc_ffa_get_fcoe_mac(skb);

	if (bnx2fc_enable_cvl_fix)
		skb = bnx2fc_validate_fip_cvl(skb);

	if (hba && !bnx2fc_validate_fip_vlan_notify(skb)) {
		bnx2fc_dbg(LOG_FRAME, "calling fcoe_ctlr_recv hba: %p\n", hba);
		fcoe_ctlr_recv(&hba->ctlr, skb);
	} else {
		printk(KERN_ERR "%s: %s NULL hba: %p\n", __FUNCTION__, netdev->name, hba);
		kfree_skb(skb);
	}
	return 0;
}

static struct net_device *bnx2fc_get_fcoe_netdev(const struct fc_lport *lp)
{
	struct bnx2fc_port *port   = NULL;
	struct bnx2fc_hba *hba     = NULL;
	struct net_device *cna_dev = NULL;

	if (lp) {
		port = lport_priv(lp);

		if (port) {
			hba = port->hba;

			if (hba)
				cna_dev = hba->fcoe_net_dev;
		}
	}

	bnx2fc_dbg(LOG_INIT, "mapped lport to CNA netdev - dev = 0x%p\n", cna_dev);
	return cna_dev;
}

static void bnx2fc_get_lesb(struct fc_lport *lport, struct fc_els_lesb *lesb)
{
	struct fc_host_statistics *host_stats;

	bnx2fc_get_host_stats(lport->host);

        host_stats = &lport->host_stats;
	lesb->lesb_link_fail = htonl(host_stats->link_failure_count);
	lesb->lesb_sync_loss = htonl(host_stats->loss_of_sync_count);
	lesb->lesb_sig_loss = htonl(host_stats->loss_of_signal_count);
	lesb->lesb_prim_err = htonl(host_stats->prim_seq_protocol_err_count);
	lesb->lesb_inv_word = htonl(host_stats->invalid_tx_word_count);
	lesb->lesb_inv_crc = htonl(host_stats->invalid_crc_count);
}

static int bnx2fc_init_dma_mem_pool(struct bnx2fc_hba *hba)
{
	VMK_ReturnStatus vmk_ret;
	vmk_MemPoolProps pool_props;

	if (bnx2fc_vmk_pool != BNX2FC_EN_VMKPOOL) {
		bnx2fc_dbg(LOG_INIT, "Running in non vmk_MemPool mode. \n");
		return 0;
	}
	bnx2fc_dbg(LOG_INIT, "Running in vmk_MemPool mode. \n");
	pool_props.module = THIS_MODULE->moduleID;
	pool_props.parentMemPool = VMK_MEMPOOL_INVALID;
	pool_props.memPoolType = VMK_MEM_POOL_LEAF;
	pool_props.resourceProps.reservation = BNX2FC_INITIAL_POOLSIZE;
	vmk_ret = vmk_NameFormat(&pool_props.name, "bnx2fc_%s",
				hba->netdev->name);
	VMK_ASSERT(vmk_ret == VMK_OK);

	pool_props.resourceProps.limit = BNX2FC_MAX_POOLSIZE_5771X;

	vmk_ret = vmk_MemPoolCreate(&pool_props, &hba->bnx2fc_dma_pool);

	if (vmk_ret != VMK_OK) {
		printk(KERN_ERR PFX "%s: pool allocation failed(%x)",
						__func__, vmk_ret);
		return -ENOMEM;
	}
	bnx2fc_dbg(LOG_INIT, "Vmk_Mem_Pool - hba:%p pool:%p limit:0x%x\n",
				hba, hba->bnx2fc_dma_pool,
				pool_props.resourceProps.limit);

	return 0;
}

static void *pci_alloc_consistent_esx(struct bnx2fc_hba *hba, size_t size,
					     dma_addr_t *mapping)
{
	char *virt_mem;
	VMK_ReturnStatus status;
	vmk_MPN pfn;
	vmk_MemPoolAllocProps pool_alloc_props;
	vmk_MpnRange range;
	vmk_MemPoolAllocRequest alloc_request;

	if (!hba || !hba->bnx2fc_dma_pool)
		return NULL;

	pool_alloc_props.physContiguity = VMK_MEM_PHYS_CONTIGUOUS;
	pool_alloc_props.physRange = VMK_PHYS_ADDR_ANY;
	pool_alloc_props.creationTimeoutMS = VMK_TIMEOUT_NONBLOCKING;
	if (dma_get_required_mask(&hba->pcidev->dev) >= hba->pcidev->dma_mask)
		pool_alloc_props.physRange = VMK_PHYS_ADDR_BELOW_4GB;

	alloc_request.numPages = 1 << get_order(size);
	alloc_request.numElements = 1;
	alloc_request.mpnRanges = &range;

	status = vmk_MemPoolAlloc(hba->bnx2fc_dma_pool,
				&pool_alloc_props, &alloc_request);
	if (unlikely(status != VMK_OK)) {
		printk(KERN_ERR PFX "allocation failed size=%lu\n", size);
		return NULL;
	}
	pfn = range.startMPN;
	virt_mem = page_to_virt(pfn_to_page(pfn));

	memset(virt_mem, 0, size);
	*mapping = pci_map_single(hba->pcidev, virt_mem, size,
				  PCI_DMA_BIDIRECTIONAL);

	return virt_mem;
}

static void pci_free_consistent_esx(struct bnx2fc_hba *hba, size_t size,
					   void *virt, dma_addr_t mapping)
{
	vmk_MpnRange range;
	vmk_MemPoolAllocRequest alloc_request;

	pci_unmap_single(hba->pcidev, mapping, size, PCI_DMA_BIDIRECTIONAL);

	range.startMPN = virt_to_page(virt);
	range.numPages = 1 << get_order(size);
	alloc_request.mpnRanges = &range;
	alloc_request.numPages = range.numPages;
	alloc_request.numElements = 1;
	vmk_MemPoolFree(&alloc_request);
}

#endif

void *bnx2fc_alloc_dma(struct bnx2fc_hba *hba, size_t size,
					     dma_addr_t *mapping)
{
	void *ret = NULL;

#ifdef __VMKLNX__
	if (bnx2fc_vmk_pool == BNX2FC_EN_VMKPOOL)
		ret = pci_alloc_consistent_esx(hba, size, mapping);
	else
#endif
		ret = pci_alloc_consistent(hba->pcidev, size, mapping);

	return ret;

}
void bnx2fc_free_dma(struct bnx2fc_hba *hba, size_t size,
				void *virt, dma_addr_t mapping)
{
#ifdef __VMKLNX__
	if (bnx2fc_vmk_pool == BNX2FC_EN_VMKPOOL)
		pci_free_consistent_esx(hba, size, virt, mapping);
	else
#endif
		pci_free_consistent(hba->pcidev, size, virt, mapping);
}
void *bnx2fc_alloc_dma_bd(struct bnx2fc_hba *hba, size_t size,
					     dma_addr_t *mapping)
{
	void *ret = NULL;

#ifdef __VMKLNX__
	if (bnx2fc_vmk_pool == BNX2FC_EN_VMKPOOL)
		ret = pci_alloc_consistent_esx(hba, size, mapping);
	else
#endif
		ret = dma_alloc_coherent(&hba->pcidev->dev, size,
					mapping, GFP_KERNEL);

	return ret;
}

/* vlan */

static struct scsi_transport_template 	*bnx2fc_transport_template;
static struct scsi_transport_template 	*bnx2fc_vport_xport_template;

struct bnx2fc_global_s bnx2fc_global;

static struct cnic_ulp_ops bnx2fc_cnic_cb;
static struct libfc_function_template bnx2fc_libfc_fcn_templ;
static struct scsi_host_template bnx2fc_shost_template;
static struct fc_function_template bnx2fc_transport_function;
static struct fc_function_template bnx2fc_vport_xport_function;
#ifndef __VMKLNX__
static int bnx2fc_create(const char *, struct kernel_param *);
static int bnx2fc_destroy(const char *, struct kernel_param *);
#endif

static void bnx2fc_recv_frame(struct sk_buff *skb);

static u64 bnx2fc_wwn_from_mac(unsigned char mac[MAX_ADDR_LEN],
		      unsigned int scheme, unsigned int port);
static inline int bnx2fc_start_io(struct bnx2fc_hba *hba, struct sk_buff *skb);
static void bnx2fc_start_disc(struct bnx2fc_hba *hba);
static void bnx2fc_check_wait_queue(struct fc_lport *lp, struct sk_buff *skb);
#ifdef __VMKLNX__
static int bnx2fc_shost_config(struct fc_lport *lport, struct device *dev);
#else
static int bnx2fc_shost_config(struct fc_lport *lp);
#endif
static int bnx2fc_net_config(struct fc_lport *lp);
static int bnx2fc_lport_config(struct fc_lport *lport);
static int bnx2fc_em_config(struct fc_lport *lport);
static int bnx2fc_bind_adapter_devices(struct bnx2fc_hba *hba);
static void bnx2fc_unbind_adapter_devices(struct bnx2fc_hba *hba);
static int bnx2fc_bind_pcidev(struct bnx2fc_hba *hba);
static void bnx2fc_unbind_pcidev(struct bnx2fc_hba *hba);
static struct fc_lport *bnx2fc_if_create(struct bnx2fc_hba *hba,
				  struct device *parent, int npiv);
static void bnx2fc_destroy_work(struct work_struct *work);

#ifndef __VMKLNX__
static struct net_device *bnx2fc_if_to_netdev(const char *buffer);
#endif
static struct bnx2fc_hba *bnx2fc_find_hba_for_cnic(struct cnic_dev *cnic);

static int bnx2fc_fw_init(struct bnx2fc_hba *hba);
static void bnx2fc_fw_destroy(struct bnx2fc_hba *hba);

static void bnx2fc_port_shutdown(struct fc_lport *lport);
static void bnx2fc_stop(struct bnx2fc_hba *hba);
static int __init bnx2fc_mod_init(void);
static void __exit bnx2fc_mod_exit(void);

static u64 bnx2fc_wwn_from_mac(unsigned char mac[MAX_ADDR_LEN],
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
static inline int bnx2fc_start_io(struct bnx2fc_hba *hba, struct sk_buff *skb)
{
	struct sk_buff *nskb;
	int rc;

	bnx2fc_dbg(LOG_FRAME, "skb %p\n", skb);

	nskb = skb_clone(skb, GFP_ATOMIC);
	rc = dev_queue_xmit(nskb);
	if (rc != 0)
		return rc;
	kfree_skb(skb);
	return 0;
}

static void bnx2fc_clean_rx_queue(struct fc_lport *lp)
{
	struct bnx2fc_global_s *bg;
	struct fcoe_rcv_info *fr;
	struct sk_buff_head *list;
	struct sk_buff *skb, *next;
	struct sk_buff *head;

	bg = &bnx2fc_global;
	spin_lock_bh(&bg->fcoe_rx_list.lock);
	list = &bg->fcoe_rx_list;
	head = list->next;
	for (skb = head; skb != (struct sk_buff *)list;
	     skb = next) {
		next = skb->next;
		fr = fcoe_dev_from_skb(skb);
		if (fr->fr_dev == lp) {
			__skb_unlink(skb, list);
			kfree_skb(skb);
		}
	}
	spin_unlock_bh(&bg->fcoe_rx_list.lock);
}

static void bnx2fc_queue_timer(ulong lport)
{
	bnx2fc_check_wait_queue((struct fc_lport *) lport, NULL);
}

static void bnx2fc_clean_pending_queue(struct fc_lport *lp)
{
	struct bnx2fc_port *port;
	struct sk_buff *skb;
	port = lport_priv(lp);

#ifdef __VMKLNX__
	/* Need to use qlen quilfier to ensure that the
         * skb is not taken out to _start_io() from 
         * _check_wait_queue().
	 */ 
	spin_lock_bh(&port->fcoe_pending_queue.lock);
	while (port->fcoe_pending_queue.qlen) {
		skb = __skb_dequeue(&port->fcoe_pending_queue);
		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		if(skb)
			kfree_skb(skb);
		else
			msleep(10);	
		spin_lock_bh(&port->fcoe_pending_queue.lock);
	}
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
#else
	spin_lock_bh(&port->fcoe_pending_queue.lock);
	while ((skb = __skb_dequeue(&port->fcoe_pending_queue)) != NULL) {
		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		kfree_skb(skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);
	}
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
#endif
}

static void bnx2fc_check_wait_queue(struct fc_lport *lp, struct sk_buff *skb)
{
	struct bnx2fc_port *port = lport_priv(lp);
	struct bnx2fc_hba *hba   = port->hba;
	int rc = 0;

	spin_lock_bh(&port->fcoe_pending_queue.lock);

	if (skb)
		__skb_queue_tail(&port->fcoe_pending_queue, skb);

	if (port->fcoe_pending_queue_active)
		goto out;
	port->fcoe_pending_queue_active = 1;

	while (port->fcoe_pending_queue.qlen) {
		/* keep qlen > 0 until bnx2fc_start_io succeeds */
		port->fcoe_pending_queue.qlen++;
		skb = __skb_dequeue(&port->fcoe_pending_queue);

		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		rc = bnx2fc_start_io(hba, skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);

		if (rc) {
			__skb_queue_head(&port->fcoe_pending_queue, skb);
			/* undo temporary increment above */
			port->fcoe_pending_queue.qlen--;
			break;
		}
		/* undo temporary increment above */
		port->fcoe_pending_queue.qlen--;
	}

	if (port->fcoe_pending_queue.qlen < FCOE_LOW_QUEUE_DEPTH)
		lp->qfull = 0;
	if (port->fcoe_pending_queue.qlen && !timer_pending(&port->timer))
		mod_timer(&port->timer, jiffies + 2);
	port->fcoe_pending_queue_active = 0;
out:
	if (port->fcoe_pending_queue.qlen > FCOE_MAX_QUEUE_DEPTH)
		lp->qfull = 1;
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
}

u32 bnx2fc_crc(struct fc_frame *fp)
{
	struct sk_buff *skb = fp_skb(fp);
	struct skb_frag_struct *frag;
	unsigned char *data;
	unsigned long off, len, clen;
	u32 crc;
	unsigned i;

	crc = crc32(~0, skb->data, skb_headlen(skb));

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		off = frag->page_offset;
		len = frag->size;
		while (len > 0) {
			clen = min(len, PAGE_SIZE - (off & ~PAGE_MASK));
			data = kmap_atomic(frag->page + (off >> PAGE_SHIFT),
					   KM_SKB_DATA_SOFTIRQ);
			crc = crc32(crc, data + (off & ~PAGE_MASK), clen);
			kunmap_atomic(data, KM_SKB_DATA_SOFTIRQ);
			off += clen;
			len -= clen;
		}
	}
	return crc;
}
int bnx2fc_get_paged_crc_eof(struct sk_buff *skb, int tlen)
{
	struct bnx2fc_global_s *bg;
	struct page *page;

	if (skb_shinfo(skb)->nr_frags >= MAX_SKB_FRAGS)
		return -EINVAL;
	
	bg = &bnx2fc_global;
	page = bg->crc_eof_page;
	if (!page) {
		page = alloc_page(GFP_ATOMIC);
		if (!page)
			return -ENOMEM;
		bg->crc_eof_page = page;
		bg->crc_eof_offset = 0;
	}

	get_page(page);
	skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags, page,
			   bg->crc_eof_offset, tlen);
	skb->len += tlen;
	skb->data_len += tlen;
	skb->truesize += tlen;
	bg->crc_eof_offset += sizeof(struct fcoe_crc_eof);

	if (bg->crc_eof_offset >= PAGE_SIZE) {
		bg->crc_eof_page = NULL;
		bg->crc_eof_offset = 0;
		put_page(page);
	}
	return 0;
}

static void bnx2fc_abort_io(struct fc_lport *lport)
{
	/*
	 * This function is no-op for bnx2fc, but we do
	 * not want to leave it as NULL either, as libfc
	 * can call the default function which is
	 * fc_fcp_abort_io.
	 */
}

static void bnx2fc_cleanup(struct fc_lport *lport)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	struct bnx2fc_rport *tgt;
	int i;

	bnx2fc_dbg(LOG_IOERR, "Entered bnx2fc_cleanup\n");
	mutex_lock(&hba->hba_mutex);
	spin_lock_bh(&hba->hba_lock);
	for (i = 0; i < BNX2FC_NUM_MAX_SESS; i++) {
		tgt = hba->tgt_ofld_list[i];
		if (tgt) {
			/* Cleanup IOs belonging to requested vport */
			if (tgt->port == port) {
				spin_unlock_bh(&hba->hba_lock);
				bnx2fc_dbg(LOG_IOERR, "flush/cleanup\n");
				bnx2fc_flush_active_ios(tgt);
				spin_lock_bh(&hba->hba_lock);
			}
		}
	}
	spin_unlock_bh(&hba->hba_lock);
	mutex_unlock(&hba->hba_mutex);
}

static int bnx2fc_xmit_l2_frame(struct bnx2fc_rport *tgt,
			     struct fc_frame *fp)
{
	struct bnx2fc_hba *hba = tgt->port->hba;
	struct fc_rport_priv *rdata = tgt->rdata;
	struct fc_frame_header *fh;
	int rc = 0;

	fh = fc_frame_header_get(fp);
	bnx2fc_dbg(LOG_FRAME, "Xmit L2 frame rport = 0x%x, oxid = 0x%x, "
			"r_ctl = 0x%x\n", rdata->ids.port_id,
			ntohs(fh->fh_ox_id), fh->fh_r_ctl);
	if ((fh->fh_type == FC_TYPE_ELS) &&
	    (fh->fh_r_ctl == FC_RCTL_ELS_REQ)) {

		switch (fc_frame_payload_op(fp)) {
		case ELS_ADISC:
			rc = bnx2fc_send_adisc(tgt, fp);
			break;
		case ELS_LOGO:
			rc = bnx2fc_send_logo(tgt, fp);
			break;
		case ELS_RLS:
			rc = bnx2fc_send_rls(tgt, fp);
			break;
		default:
			break;
		}
	} else if ((fh->fh_type ==  FC_TYPE_BLS) &&
	    (fh->fh_r_ctl == FC_RCTL_BA_ABTS))
		bnx2fc_dbg(LOG_FRAME, "ABTS frame\n");
	else {
		bnx2fc_dbg(LOG_FRAME, "Send L2 frame type 0x%x "
				"rctl 0x%x thru non-offload path\n",
				fh->fh_type, fh->fh_r_ctl);
		return -ENODEV;
	}
	if (rc)
		return -ENOMEM;
	else
		return 0;
}

/**
 * bnx2fc_xmit - bnx2fc's FCoE frame transmit function
 *
 * @lport:	the associated local port
 * @fp:	the fc_frame to be transmitted
 */
static int bnx2fc_xmit(struct fc_lport *lport, struct fc_frame *fp)
{
	struct ethhdr 		*eh;
	struct fcoe_crc_eof 	*cp;
	struct sk_buff 		*skb;
	struct fc_frame_header 	*fh;
	struct bnx2fc_hba 	*hba;
#ifndef __VMKLNX__
	struct fcoe_dev_stats *stats;
#endif
	struct bnx2fc_port 	*port;
	struct fcoe_hdr 	*hp;
	struct bnx2fc_rport 	*tgt;
	u8			sof,eof;
	u32			crc;
	unsigned int		hlen,tlen,elen;
	int			wlen, rc = 0;

	port = (struct bnx2fc_port *)lport_priv(lport);
	hba = port->hba;
	
	bnx2fc_dbg(LOG_FRAME, "%s lport: %p, netdev: %s\n",
		   __FUNCTION__, lport, hba->netdev->name);

	fh = fc_frame_header_get(fp);

	skb = fp_skb(fp);
	if (unlikely(fh->fh_r_ctl == FC_RCTL_ELS_REQ)) {
		if (!hba->ctlr.sel_fcf) {
			bnx2fc_dbg(LOG_FRAME, "FCF not selected yet!\n");
			kfree_skb(skb);
			return -EINVAL;
		}
		if (fcoe_ctlr_els_send(&hba->ctlr, lport, skb)) {
			printk(KERN_ERR "%s: fcoe_ctlr_els_send()\n", __FUNCTION__);
			return 0;
		}
	}

	sof = fr_sof(fp);
	eof = fr_eof(fp);

	/*
	 * Snoop the frame header to check if the frame is for
	 * an offloaded session
	 */
	/*
	 * tgt_ofld_list access is synchronized using
	 * both hba mutex and hba lock. Atleast hba mutex or
	 * hba lock needs to be held for read access.
	 */

	spin_lock_bh(&hba->hba_lock);
	tgt = bnx2fc_tgt_lookup(port, ntoh24(fh->fh_d_id));
	if (tgt && (test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags))) {
		/* This frame is for offloaded session */
		bnx2fc_dbg(LOG_SESS, "xmit: Frame is for offloaded session "
				"port_id = 0x%x\n", ntoh24(fh->fh_d_id));
		spin_unlock_bh(&hba->hba_lock);
		rc = bnx2fc_xmit_l2_frame(tgt, fp);
		if (rc != -ENODEV) {
			printk(KERN_ERR "%s: xmit l2 failed\n", __FUNCTION__);
			kfree_skb(skb);
			return rc;
		}
	} else {
		spin_unlock_bh(&hba->hba_lock);
	}

	elen = sizeof(struct ethhdr);
	hlen = sizeof(struct fcoe_hdr);
	tlen = sizeof(struct fcoe_crc_eof);
	wlen = (skb->len - tlen + sizeof(crc)) / FCOE_WORD_TO_BYTE;

	skb->ip_summed = CHECKSUM_NONE;
	crc = bnx2fc_crc(fp);

	/* copy port crc and eof to the skb buff */
	if (skb_is_nonlinear(skb)) {
		skb_frag_t *frag;
		rc = bnx2fc_get_paged_crc_eof(skb, tlen);
		if (rc) {
			if (rc == -ENOMEM)
				printk(KERN_ERR "%s bnx2fc_get_paged_crc_eof()\n", __FUNCTION__);
			else
				hba->num_max_frags++;
			kfree_skb(skb);
			return rc;
		}
		frag = &skb_shinfo(skb)->frags[skb_shinfo(skb)->nr_frags - 1];
		cp = kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ)
				+ frag->page_offset;
	} else {
		cp = (struct fcoe_crc_eof *)skb_put(skb, tlen);
	}

	memset(cp, 0, sizeof(*cp));
	cp->fcoe_eof = eof;
	cp->fcoe_crc32 = cpu_to_le32(~crc);
	if (skb_is_nonlinear(skb)) {
		kunmap_atomic(cp, KM_SKB_DATA_SOFTIRQ);
		cp = NULL;
	}

	/* adjust skb network/transport offsets to match mac/fcoe/port */
	skb_push(skb, elen + hlen);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_FCOE);
	skb->dev = hba->netdev;

	/* fill up mac and fcoe headers */
	eh = eth_hdr(skb);
	eh->h_proto = htons(ETH_P_FCOE);
	if (hba->ctlr.map_dest)
		fc_fcoe_set_mac(eh->h_dest, fh->fh_d_id);
	else
		/* insert GW address */
		memcpy(eh->h_dest, hba->ctlr.dest_addr, ETH_ALEN);

	if (unlikely(hba->ctlr.flogi_oxid != FC_XID_UNKNOWN))
		memcpy(eh->h_source, hba->ctlr.ctl_src_addr, ETH_ALEN);
	else
		memcpy(eh->h_source, port->data_src_addr, ETH_ALEN);

	hp = (struct fcoe_hdr *)(eh + 1);
	memset(hp, 0, sizeof(*hp));
	if (FC_FCOE_VER)
		FC_FCOE_ENCAPS_VER(hp, FC_FCOE_VER);
	hp->fcoe_sof = sof;

	/* fcoe lso, mss is in max_payload which is non-zero for FCP data */
	if (lport->seq_offload && fr_max_payload(fp)) {
		skb_shinfo(skb)->gso_type = SKB_GSO_FCOE;
		skb_shinfo(skb)->gso_size = fr_max_payload(fp);
	} else {
		skb_shinfo(skb)->gso_type = 0;
		skb_shinfo(skb)->gso_size = 0;
	}

	/*update tx stats */
#ifndef __VMKLNX__
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	stats->TxFrames++;
	stats->TxWords += wlen;
	put_cpu();
#endif

	/* send down to lld */
	fr_dev(fp) = lport;
	if (port->fcoe_pending_queue.qlen)
		bnx2fc_check_wait_queue(lport, skb);
	else if (bnx2fc_start_io(hba, skb))
		bnx2fc_check_wait_queue(lport, skb);

	return 0;
}

/**
 * bnx2fc_rcv - This is bnx2fc's receive function called by NET_RX_SOFTIRQ
 *
 * @skb:	the receive socket buffer
 * @dev:	associated net device
 * @ptype:	context
 * @olddev:	last device
 *
 * This function receives the packet and builds FC frame and passes it up
 */
static int bnx2fc_rcv(struct sk_buff *skb, struct net_device *dev,
		struct packet_type *ptype, struct net_device *olddev)
{
	struct fc_lport *lport;
	struct bnx2fc_hba *hba;
	struct fc_frame_header *fh;
	struct fcoe_rcv_info *fr;
	struct bnx2fc_global_s *bg;
	unsigned short oxid;

	hba = container_of(ptype, struct bnx2fc_hba, fcoe_packet_type);
	lport = hba->ctlr.lp;

	if (unlikely(lport == NULL)) {
		printk(KERN_ALERT PFX "bnx2fc_rcv: lport is NULL\n");
		goto err;
	}

	if (unlikely(eth_hdr(skb)->h_proto != htons(ETH_P_FCOE))) {
		printk(KERN_ALERT PFX "bnx2fc_rcv: Wrong FC type frame\n");
		goto err;
	}

	/*
	 * Check for minimum frame length, and make sure required FCoE
	 * and FC headers are pulled into the linear data area.
	 */
	if (unlikely((skb->len < FCOE_MIN_FRAME) ||
	    !pskb_may_pull(skb, FCOE_HEADER_LEN)))
		goto err;

	skb_set_transport_header(skb, sizeof(struct fcoe_hdr));
	fh = (struct fc_frame_header *) skb_transport_header(skb);

	oxid = ntohs(fh->fh_ox_id);

	fr = fcoe_dev_from_skb(skb);
	fr->fr_dev = lport;
	fr->ptype = ptype;

	bg = &bnx2fc_global;
	spin_lock_bh(&bg->fcoe_rx_list.lock);

	__skb_queue_tail(&bg->fcoe_rx_list, skb);
	if (bg->fcoe_rx_list.qlen == 1)
		wake_up_process(bg->l2_thread);

	spin_unlock_bh(&bg->fcoe_rx_list.lock);

	return 0;
err:
	kfree_skb(skb);
	return -1;
}

static int bnx2fc_l2_rcv_thread(void *arg)
{
	struct bnx2fc_global_s *bg = arg;
	struct sk_buff *skb;

	set_user_nice(current, -20);
	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_RUNNING);
		spin_lock_bh(&bg->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&bg->fcoe_rx_list)) != NULL) {
			struct bnx2fc_hba *hba = (struct bnx2fc_hba *)skb->dev->fcoe_ptr;
			if (hba->last_recv_skb != NULL) 
				kfree_skb(hba->last_recv_skb);

			hba->last_recv_skb = skb_clone(skb, GFP_ATOMIC);

			spin_unlock_bh(&bg->fcoe_rx_list.lock);

			bnx2fc_recv_frame(skb);
			spin_lock_bh(&bg->fcoe_rx_list.lock);
		}
		spin_unlock_bh(&bg->fcoe_rx_list.lock);
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
	return 0;
}


static void bnx2fc_recv_frame(struct sk_buff *skb)
{
	u32 fr_len;
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
#ifndef __VMKLNX__
	struct fcoe_dev_stats *stats;
#endif
	struct fc_frame_header *fh;
	struct fcoe_crc_eof crc_eof;
	struct fc_frame *fp;
	struct fc_lport *vn_port = NULL;
	struct bnx2fc_port *port;
	u32 f_ctl;
	int fip_mode;

	u8 *mac = NULL;
	u8 *dest_mac = NULL;
	struct fcoe_hdr *hp;
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)skb->dev->fcoe_ptr;

	fr = fcoe_dev_from_skb(skb);
	lport = fr->fr_dev;
	if (unlikely(lport == NULL)) {
		printk(KERN_ALERT PFX "Invalid lport struct\n");
		kfree_skb(skb);
		return;
	}

	if (skb_is_nonlinear(skb))
		skb_linearize(skb);
	mac = eth_hdr(skb)->h_source;
	dest_mac = eth_hdr(skb)->h_dest;

	/* Pull the header */
#ifdef __VMKLNX__
	hp = (struct fcoe_hdr *)skb->data;
#else
	hp = (struct fcoe_hdr *) skb_network_header(skb);
	fh = (struct fc_frame_header *) skb_transport_header(skb);
#endif

	skb_pull(skb, sizeof(struct fcoe_hdr));
	fr_len = skb->len - sizeof(struct fcoe_crc_eof);

#ifndef __VMKLNX__
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	stats->RxFrames++;
	stats->RxWords += fr_len / FCOE_WORD_TO_BYTE;
#endif

	fp = (struct fc_frame *)skb;
	fc_frame_init(fp);
	fr_dev(fp) = lport;
	fr_sof(fp) = hp->fcoe_sof;
	if (skb_copy_bits(skb, fr_len, &crc_eof, sizeof(crc_eof))) {
#ifndef __VMKLNX__
		put_cpu();
#endif
		kfree_skb(skb);
		return;
	}
	fr_eof(fp) = crc_eof.fcoe_eof;
	fr_crc(fp) = crc_eof.fcoe_crc32;
	if (pskb_trim(skb, fr_len)) {
#ifndef __VMKLNX__
		put_cpu();
#endif
		kfree_skb(skb);
		return;
	}

	fh = fc_frame_header_get(fp);

	if (ntoh24(&dest_mac[3]) != ntoh24(fh->fh_d_id)) {
		DECLARE_MAC_BUF(mac);

		bnx2fc_dbg(LOG_ERROR, "FC frame d_id mismatch with MAC:%s\n", print_mac(mac, dest_mac));
#ifndef __VMKLNX__
		put_cpu();
#endif
		hba->drop_d_id_mismatch++;
		kfree_skb(skb);
		return;
	}

#ifndef __VMKLNX__
	vn_port = fc_vport_id_lookup(lport, ntoh24(fh->fh_d_id));
#else
	vn_port = fc_host_port_id(lport->host) == ntoh24(fh->fh_d_id) ? lport : NULL;
#endif

	if (vn_port) {
		port = lport_priv(vn_port);
		if (compare_ether_addr(port->data_src_addr, dest_mac)
		     						!= 0) {
			int i;
			unsigned int *x = (unsigned int *)fh;

			printk(KERN_ERR PFX "ERROR! FPMA mismatch..."
					" drop packet\n");
			bnx2fc_dbg(LOG_ERROR, "expected mac = ");
			for (i = 0; i < 6; i++)
				bnx2fc_dbg(LOG_ERROR, "%2x:", port->data_src_addr[i]);
			bnx2fc_dbg(LOG_FRAME, "dumping frame header...\n");
			for (i=0; i < 6; i++)
				bnx2fc_dbg(LOG_FRAME, "%8x", *x++);
			bnx2fc_dbg(LOG_FRAME, "\n");
			bnx2fc_dbg(LOG_FRAME, "received dest_mac = ");
			for (i = 0; i < 6; i++)
				bnx2fc_dbg(LOG_FRAME, "%2x:", dest_mac[i]);
#ifndef __VMKLNX__
			put_cpu();
#endif
			kfree_skb(skb);
			hba->drop_fpma_mismatch++;
			return;
		}
	} else {
		if (fc_host_port_id(lport->host) != 0) {
			if (hba->last_vn_port_zero != NULL) 
				kfree_skb(hba->last_vn_port_zero);

			hba->last_vn_port_zero = skb;

			BNX2FC_ERR("ERROR! vn_port == 0\n");

			hba->drop_vn_port_zero++;
			return;
		}
	}

	fip_mode = is_fip_mode(&hba->ctlr);
	if (fip_mode && compare_ether_addr(mac, hba->ctlr.dest_addr)) {
		DECLARE_MAC_BUF(mac_buf);
		DECLARE_MAC_BUF(mac_buf2);

		bnx2fc_dbg(LOG_FRAME, "wrong source mac address: fip_mode: %d mac:%s hba->ctlr.dest_addr: %s\n",
			fip_mode, print_mac(mac_buf, mac), print_mac(mac_buf2, hba->ctlr.dest_addr));
#ifndef __VMKLNX__
		put_cpu();
 #endif
		kfree_skb(skb);
		hba->drop_wrong_source_mac++;
		return;
	}

	if (fh->fh_r_ctl == FC_RCTL_DD_SOL_DATA &&
	    fh->fh_type == FC_TYPE_FCP) {
		bnx2fc_dbg(LOG_INIT, "DD_SOL_DATA\n");
		/* Drop FCP data. We dont this in L2 path */
#ifndef __VMKLNX__
		put_cpu();
#endif
		kfree_skb(skb);
		return;
	}
	if (fh->fh_r_ctl == FC_RCTL_ELS_REQ &&
	    fh->fh_type == FC_TYPE_ELS) {
		switch (fc_frame_payload_op(fp)) {
		case ELS_LOGO:
			if (ntoh24(fh->fh_s_id) == FC_FID_FLOGI) {
				/* drop non-FIP LOGO */
#ifndef __VMKLNX__
				put_cpu();
#endif
				kfree_skb(skb);
				return;
			}
			break;
		}
	}
	if (fh->fh_r_ctl == FC_RCTL_BA_ABTS) {
		/* Drop incoming ABTS */
#ifndef __VMKLNX__
		put_cpu();
#endif
		kfree_skb(skb);
		return;
	}

	/* workaround for CQ61013 */
	f_ctl = ntoh24(fh->fh_f_ctl);
	if ((fh->fh_type == FC_TYPE_BLS) && (f_ctl & FC_FC_SEQ_CTX) &&
	    (f_ctl & FC_FC_EX_CTX)) {
		/* Drop incoming ABTS response that has both SEQ/EX CTX set */
#ifndef __VMKLNX__
		put_cpu();
#endif
		kfree_skb(skb);

		hba->drop_abts_seq_ex_ctx_set++;
		return;
	}

	if (le32_to_cpu(fr_crc(fp)) !=
	    ~crc32(~0, skb->data, fr_len)) {
#ifndef __VMKLNX__
		if (stats->InvalidCRCCount < 5)
			printk(KERN_ERR PFX "dropping frame with "
				"CRC error \n");
		stats->InvalidCRCCount++;
		put_cpu();
#endif
		kfree_skb(skb);
		return;
	}

#ifndef __VMKLNX__
	put_cpu();
#endif
	fc_exch_recv(lport, fp);
}

static struct fc_host_statistics *bnx2fc_get_host_stats(struct Scsi_Host *shost)
{
	struct fc_host_statistics *bnx2fc_stats;
	struct fc_lport *lport = shost_priv(shost);
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	struct fcoe_statistics_params *fw_stats;
	int rc = 0;

	fw_stats = (struct fcoe_statistics_params *)hba->stats_buffer;
	if (!fw_stats)
		return NULL;

	bnx2fc_stats = fc_get_host_stats(shost);

	init_completion(&hba->stat_req_done);
	if (bnx2fc_send_stat_req(hba))
		return bnx2fc_stats;
	rc = wait_for_completion_timeout(&hba->stat_req_done, (2 * HZ));
	if (!rc) {
		bnx2fc_dbg(LOG_INIT, "FW stat req timed out\n");
		return bnx2fc_stats;
	}
	bnx2fc_stats->invalid_crc_count += fw_stats->rx_stat2.fc_crc_cnt;
	bnx2fc_stats->tx_frames += fw_stats->tx_stat.fcoe_tx_pkt_cnt;
	bnx2fc_stats->tx_words += (fw_stats->tx_stat.fcoe_tx_byte_cnt) / 4;
	bnx2fc_stats->rx_frames += fw_stats->rx_stat0.fcoe_rx_pkt_cnt;
	bnx2fc_stats->rx_words += (fw_stats->rx_stat0.fcoe_rx_byte_cnt) / 4;

	bnx2fc_stats->fcp_output_megabytes >>= 20;
	bnx2fc_stats->fcp_input_megabytes >>= 20;

	bnx2fc_stats->dumped_frames = 0;
	bnx2fc_stats->lip_count = 0;
	bnx2fc_stats->nos_count = 0;
	bnx2fc_stats->loss_of_sync_count = 0;
	bnx2fc_stats->loss_of_signal_count = 0;
	bnx2fc_stats->prim_seq_protocol_err_count = 0;

	return bnx2fc_stats;
}

#ifdef __VMKLNX__
static int bnx2fc_shost_config(struct fc_lport *lport, struct device *dev)
#else
static int bnx2fc_shost_config(struct fc_lport *lport)
#endif
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	struct Scsi_Host *shost = lport->host;
	int rc = 0;
#if defined (__VMKLNX__)
	u16 vlan_id; 
#endif

	shost->max_cmd_len = BNX2FC_MAX_CMD_LEN;
	shost->max_lun = BNX2FC_MAX_LUN;
	shost->max_id = BNX2FC_MAX_FCP_TGT;
	shost->max_channel = 0;
	if (lport->vport)
		shost->transportt = bnx2fc_vport_xport_template;
	else
		shost->transportt = bnx2fc_transport_template;

	shost->dma_boundary = hba->pcidev->dma_mask;

	/* Add the new host to SCSI-ml */
#ifdef __VMKLNX__
	rc = scsi_add_host(lport->host, dev);
#else
	rc = scsi_add_host(lport->host, NULL);
#endif
	if (rc) {
		printk(KERN_ERR PFX "Error on scsi_add_host\n");
		return rc;
	}

	if (!lport->vport)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
		fc_host_max_npiv_vports(lport->host) = USHRT_MAX;
#else
		fc_host_max_npiv_vports(lport->host) = USHORT_MAX;
#endif

	snprintf(fc_host_symbolic_name(lport->host), 256,
		 "QLogic %s %s v%s over %s", hba->sym_name,
		 BNX2FC_NAME, BNX2FC_VERSION, hba->netdev->name);

#if defined (__VMKLNX__)
        /*
         * Initialize already known information FCOE adapter attributes.
         */
        vlan_id = vmklnx_cna_get_vlan_tag(hba->fcoe_net_dev) & VLAN_VID_MASK;
        vmklnx_init_fcoe_attribs(lport->host, hba->fcoe_net_dev->name,
                                 vlan_id, hba->ctlr.ctl_src_addr, NULL, NULL);
#endif /* defined (__VMKLNX__) */

	return 0;
}

static int bnx2fc_link_ok(struct fc_lport *lport)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	struct net_device *dev = hba->phys_dev;
	struct ethtool_cmd ecmd = { ETHTOOL_GSET };
	int rc = 0;

	if ((dev->flags & IFF_UP) && netif_carrier_ok(dev)) {
		dev = hba->netdev;
		if (dev->ethtool_ops->get_settings) {
			dev->ethtool_ops->get_settings(dev, &ecmd);
			lport->link_supported_speeds &=
				~(FC_PORTSPEED_1GBIT | FC_PORTSPEED_10GBIT);
			if (ecmd.supported & (SUPPORTED_1000baseT_Half |
					      SUPPORTED_1000baseT_Full))
				lport->link_supported_speeds |=
							FC_PORTSPEED_1GBIT;
			if (ecmd.supported & SUPPORTED_10000baseT_Full)
				lport->link_supported_speeds |=
					FC_PORTSPEED_10GBIT;
			if (ecmd.speed == SPEED_1000)
				lport->link_speed = FC_PORTSPEED_1GBIT;
			if (ecmd.speed == SPEED_2500)
				lport->link_speed = FC_PORTSPEED_2GBIT;
			if (ecmd.speed == SPEED_10000)
				lport->link_speed = FC_PORTSPEED_10GBIT;
		}
		clear_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
	} else {
		set_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
		rc = -1;
	}
	return rc;
}

/**
 * bnx2fc_get_link_state - get network link state
 *
 * @hba:	adapter instance pointer
 *
 * updates adapter structure flag based on netdev state
 */
void bnx2fc_get_link_state(struct bnx2fc_hba *hba)
{
	if (test_bit(__LINK_STATE_NOCARRIER, &hba->netdev->state))
		set_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
	else
		clear_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
}

static inline u64 bnx2fc_get_wwnn(struct bnx2fc_hba *hba)
{
	u64 wwnn;

	if (hba->cnic->fcoe_wwnn)
		wwnn = hba->cnic->fcoe_wwnn;
	else
		wwnn = bnx2fc_wwn_from_mac(hba->ctlr.ctl_src_addr, 1, 0);

	return wwnn;
}

static inline u64 bnx2fc_get_wwpn(struct bnx2fc_hba *hba)
{
	u64 wwpn;

	if (hba->cnic->fcoe_wwpn)
		wwpn = hba->cnic->fcoe_wwpn;
	else
		wwpn = bnx2fc_wwn_from_mac(hba->ctlr.ctl_src_addr, 2, 0);

	return wwpn;
}

static int bnx2fc_net_config(struct fc_lport *lport)
{
	struct bnx2fc_hba *hba;
	struct net_device *netdev;
	struct bnx2fc_port *port;
	u64 wwnn, wwpn;

	port = lport_priv(lport);
	hba = port->hba;
	netdev = hba->netdev;

	/* require support for get_pauseparam ethtool op. */
	if (!hba->phys_dev->ethtool_ops ||
	    !hba->phys_dev->ethtool_ops->get_pauseparam)
		return -EOPNOTSUPP;

	if (fc_set_mfs(lport, BNX2FC_MFS))
		return -EINVAL;

	skb_queue_head_init(&port->fcoe_pending_queue);
	port->fcoe_pending_queue_active = 0;
	setup_timer(&port->timer, bnx2fc_queue_timer, (unsigned long) lport);

	if (!lport->vport) {
		wwnn = bnx2fc_get_wwnn(hba);
		bnx2fc_dbg(LOG_INIT, "WWNN = 0x%llx\n", wwnn);
		fc_set_wwnn(lport, wwnn);

		wwpn = bnx2fc_get_wwpn(hba);
		bnx2fc_dbg(LOG_INIT, "WWPN = 0x%llx\n", wwpn);
		fc_set_wwpn(lport, wwpn);
	}

	return 0;
}

static void bnx2fc_destroy_timer(unsigned long data)
{
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)data;

	bnx2fc_dbg(LOG_IOERR, "Destroy compl not received!!\n");
	set_bit(BNX2FC_FLAG_DESTROY_CMPL, &hba->flags);
	wake_up_interruptible(&hba->destroy_wait);
}

static int bnx2fc_link_ok_wait(struct fc_lport *lport)
{
	struct bnx2fc_hba *hba;
	struct bnx2fc_port *port;
	int wait_cnt = 0;
	int rc = -1;

	port = lport_priv(lport);
	hba = port->hba;

	rc = bnx2fc_link_ok(lport);
	while (rc) {
		msleep(250);
		/* give up after 3 secs */
		if (++wait_cnt > 12) {
			bnx2fc_dbg(LOG_INIT, "link_ok_wait:%s Link wait expired\n",
						hba->netdev->name);
			break;
		}
		rc = bnx2fc_link_ok(lport);
	}
	return rc;
}

static void bnx2fc_netevent_handler(struct bnx2fc_hba *hba, unsigned long event)
{
	struct fc_lport *lport;
	u32 link_possible = 1;

	lport = hba->ctlr.lp;

	bnx2fc_dbg(LOG_DEV_EVT, "netevent handler - event=%s %ld\n",
				hba->netdev->name, event);

	switch (event) {
	case NETDEV_UP:
		bnx2fc_dbg(LOG_DEV_EVT, "Port up, adapter_state = %ld\n",
			hba->adapter_state);
		if (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state))
			printk(KERN_ERR "indicate_netevent: "\
					"adapter is not UP!!\n");
		else if (test_bit(BNX2FC_ADAPTER_BOOT, &hba->adapter_type))
			/* For boot devices VLAN ID is obtained from the boot
			 * table and not through VLAN DISCOVERY process. If this
			 * dev is marked as boot adapter, use previously set VID
			 */
			set_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
		break;
	case NETDEV_GOING_DOWN:
		bnx2fc_dbg(LOG_DEV_EVT, "Port going down\n");
		link_possible = 0;
		break;
	default:
		printk(KERN_ERR PFX "Unkonwn netevent %ld", event);
		return;
	}

	if (link_possible && !bnx2fc_link_ok_wait(lport)) {
		printk(KERN_ERR "netevent_handler: call ctlr_link_up\n");
		bnx2fc_restart_fcoe_disc(hba);
	} else {
		printk(KERN_ERR "netevent_handler: call ctlr_link_down\n");
		if (fcoe_ctlr_link_down(&hba->ctlr)) {
			bnx2fc_dbg(LOG_DEV_EVT, "Link Down : returned from fcoe_ctlr_link_down()\n");

			clear_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
			fc_host_port_type(lport->host) = FC_PORTTYPE_UNKNOWN;

			bnx2fc_dbg(LOG_DEV_EVT, "Link Down : clean pending Q\n");
			bnx2fc_clean_pending_queue(lport);
		}
	}
	bnx2fc_dbg(LOG_DEV_EVT, "%s: %s - event %ld END!!\n",
		__FUNCTION__, hba->netdev->name, event);

}

#ifdef __FCOE_IF_RESTART_WQ__
static void bnx2fc_indicate_netevent_wq(struct work_struct *context)
{
	struct bnx2fc_netdev_work *work = (struct bnx2fc_netdev_work *)context;
	struct bnx2fc_hba *hba;

	if (!work) {
		printk(KERN_ERR "%s Invalid arg!! work: %p\n", __FUNCTION__, work);
		return;
	}

	hba = work->hba;

	if (!hba) {
		printk(KERN_ERR "%s Invalid arg!! hba: %p\n", __FUNCTION__, hba);
		return;
	}

	bnx2fc_dbg(LOG_DEV_EVT, "netdev_wq processing event=%ld\n",
				work->event);
	bnx2fc_netevent_handler(work->hba, work->event);

	kfree(work);
}
#endif
/**
 * bnx2fc_indicate_netevent - Generic netdev event handler
 *
 * @context:	adapter structure pointer
 * @event:	event type
 *
 * Handles NETDEV_UP, NETDEV_DOWN, NETDEV_GOING_DOWN,NETDEV_CHANGE and
 * NETDEV_CHANGE_MTU events
 */
static void bnx2fc_indicate_netevent(void *context, unsigned long event)
{
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)context;
	struct fc_lport *lport = hba->ctlr.lp;
#ifdef __FCOE_IF_RESTART_WQ__
	struct bnx2fc_netdev_work *work = NULL;
#endif

	if (!test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
		bnx2fc_dbg(LOG_DEV_EVT, "driver not ready. event=%s %ld\n",
			   hba->netdev->name, event);
		return;
	}
	/*
	 * ASSUMPTION:
	 * indicate_netevent cannot be called from cnic unless bnx2fc
	 * does register_device
	 */
	BUG_ON(!lport);

	bnx2fc_dbg(LOG_DEV_EVT, "indicate_netevent - event=%ld\n", event);

#ifdef __FCOE_IF_RESTART_WQ__
	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		printk(KERN_ERR PFX "Unable to allocate work event \n");
		return ;
	}
	work->hba = hba;
	work->event = event;
	INIT_WORK( (struct work_struct *)work, bnx2fc_indicate_netevent_wq);
	queue_work(hba->fcoe_if_restart_wq, (struct work_struct *)work);
#else
	bnx2fc_netevent_handler(hba, event);
#endif
}

/**
 * bnx2fc_ulp_get_stats - cnic callback to populate FCoE stats
 *
 * @handle:	transport handle pointing to adapter struture
 */
 static int bnx2fc_ulp_get_stats(void *handle)
 {
	struct bnx2fc_hba *hba = handle;
	struct cnic_dev *cnic;
	struct fcoe_stats_info *stats_addr;

	if (!hba)
		return -EINVAL;

	cnic = hba->cnic;
	stats_addr = &cnic->stats_addr->fcoe_stat;
	if (!stats_addr)
		return -EINVAL;

	strlcpy(stats_addr->version, BNX2FC_VERSION,
		sizeof(stats_addr->version));
	stats_addr->txq_size = BNX2FC_SQ_WQES_MAX;
	stats_addr->rxq_size = BNX2FC_CQ_WQES_MAX;

	return 0;
 }

static int bnx2fc_em_config(struct fc_lport *lport)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;

	if (!fc_exch_mgr_alloc(lport, FC_CLASS_3, FCOE_MIN_XID,
				FCOE_MAX_XID, NULL)) {
		printk(KERN_ERR PFX "em_config:fc_exch_mgr_alloc failed\n");
		return -ENOMEM;
	}

	hba->cmd_mgr = bnx2fc_cmd_mgr_alloc(hba, BNX2FC_MIN_XID,
					    BNX2FC_MAX_XID);

	if (!hba->cmd_mgr) {
		printk(KERN_ERR PFX "em_config:bnx2fc_cmd_mgr_alloc failed\n");
		fc_exch_mgr_free(lport);
		return -ENOMEM;
	}
	return 0;
}

static int bnx2fc_lport_config(struct fc_lport *lport)
{
	lport->link_up = 0;
	lport->qfull = 0;
	lport->max_retry_count = BNX2FC_MAX_RETRY_CNT;
	lport->max_rport_retry_count = BNX2FC_MAX_RPORT_RETRY_CNT;
	lport->e_d_tov = 2 * 1000;
	lport->r_a_tov = 10 * 1000;

	/* REVISIT: enable when supporting tape devices
	lport->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS |
				FCP_SPPF_RETRY | FCP_SPPF_CONF_COMPL);
	*/
	lport->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS);
#ifdef __VMKLNX__
	lport->does_npiv = 0;
#else
	lport->does_npiv = 1;
#endif

	memset(&lport->rnid_gen, 0, sizeof(struct fc_els_rnid_gen));
	lport->rnid_gen.rnid_atype = BNX2FC_RNID_HBA;

	/* alloc stats structure */
	if (fc_lport_init_stats(lport))
		return -ENOMEM;

	/* Finish fc_lport configuration */
	fc_lport_config(lport);

	return 0;
}

#ifndef __VMKLNX__
/**
 * bnx2fc_fip_recv - handle a received FIP frame.
 * @skb: the received skb
 * @dev: associated &net_device
 * @ptype: the &packet_type structure which was used to register this handler.
 * @orig_dev: original receive &net_device, in case @ dev is a bond.
 *
 * Returns: 0 for success
 */
static int bnx2fc_fip_recv(struct sk_buff *skb, struct net_device *dev,
			   struct packet_type *ptype,
			   struct net_device *orig_dev)
{
	struct bnx2fc_hba *hba;
	hba = container_of(ptype, struct bnx2fc_hba, fip_packet_type);
	fcoe_ctlr_recv(&hba->ctlr, skb);
	return 0;
}
#endif

/**
 * bnx2fc_update_src_mac - Update Ethernet MAC filters.
 *
 * @fip: FCoE controller.
 * @old: Unicast MAC address to delete if the MAC is non-zero.
 * @new: Unicast MAC address to add.
 *
 * Remove any previously-set unicast MAC filter.
 * Add secondary FCoE MAC address filter for our OUI.
 */
static void bnx2fc_update_src_mac(struct fc_lport *lport, u8 *addr)
{
	struct bnx2fc_port *port = lport_priv(lport);

	memcpy(port->data_src_addr, addr, ETH_ALEN);
}

/**
 * bnx2fc_get_src_mac - return the ethernet source address for an lport
 *
 * @lport: libfc port
 */
static u8 *bnx2fc_get_src_mac(struct fc_lport *lport)
{
	struct bnx2fc_port *port;

	port = (struct bnx2fc_port *)lport_priv(lport);
	return port->data_src_addr;
}
/**
 * bnx2fc_fip_send() - send an Ethernet-encapsulated FIP frame.
 * @fip: FCoE controller.
 * @skb: FIP Packet.
 */
static void bnx2fc_fip_send(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	struct bnx2fc_hba *hba;

#ifdef __VMKLNX__
	skb->dev = bnx2fc_from_ctlr(fip)->fcoe_net_dev;
#else
	skb->dev = bnx2fc_from_ctlr(fip)->netdev;
#endif

	hba = (struct bnx2fc_hba *)skb->dev->fcoe_ptr;

	{
	bnx2fc_dbg(LOG_INIT, "skb %p\n", skb);
	}
	dev_queue_xmit(skb);
}

static int bnx2fc_vport_create(struct fc_vport *vport, bool disabled)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct bnx2fc_port *port = lport_priv(n_port);
	struct bnx2fc_hba *hba = port->hba;
	struct net_device *netdev = hba->netdev;
	struct fc_lport *vn_port;

	if (!test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done)) {
		printk(KERN_ERR PFX "vn ports cannot be created on"
			"this hba\n");
		return -EIO;
	}
	vn_port = bnx2fc_if_create(hba, &vport->dev, 1);

	if (IS_ERR(vn_port)) {
		printk(KERN_ERR PFX "bnx2fc_vport_create (%s) failed\n",
			netdev->name);
		return -EIO;
	}

	if (disabled) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
	} else {
		vn_port->boot_time = jiffies;
		fc_lport_init(vn_port);
		fc_fabric_login(vn_port);
		fc_vport_setlink(vn_port);
	}
	return 0;
}

static int bnx2fc_vport_destroy(struct fc_vport *vport)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fc_lport *vn_port = vport->dd_data;
	struct bnx2fc_port *port = lport_priv(vn_port);

	mutex_lock(&n_port->lp_mutex);
	list_del(&vn_port->list);
	mutex_unlock(&n_port->lp_mutex);
	schedule_work(&port->destroy_work);
	return 0;
}

static int bnx2fc_vport_disable(struct fc_vport *vport, bool disable)
{
	struct fc_lport *lport = vport->dd_data;

	if (disable) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
		fc_fabric_logoff(lport);
	} else {
		lport->boot_time = jiffies;
		fc_fabric_login(lport);
		fc_vport_setlink(lport);
	}
	return 0;
}


static int bnx2fc_netdev_setup(struct bnx2fc_hba *hba)
{
	struct net_device *netdev = hba->netdev;
#ifndef __VMKLNX__
	struct net_device *physdev = hba->phys_dev;
	struct netdev_hw_addr *ha;
#endif
	int sel_san_mac = 0;

	bnx2fc_dbg(LOG_INIT, "netdev_setup - hba = 0x%p\n", hba);
	/* Do not support for bonding device */
	if ((netdev->priv_flags & IFF_MASTER_ALB) ||
			(netdev->priv_flags & IFF_SLAVE_INACTIVE) ||
			(netdev->priv_flags & IFF_MASTER_8023AD)) {
		return -EOPNOTSUPP;
	}

#ifdef __VMKLNX__
	/* setup FIP Controller MAC Address */
	if (hba->fcoe_net_dev) {
		hba->fcoe_net_dev->fcoe_ptr = hba;
		memcpy(hba->ctlr.ctl_src_addr, hba->fcoe_net_dev->dev_addr, ETH_ALEN);
		sel_san_mac = 1;
	}
#else
	rcu_read_lock();
	for_each_dev_addr(physdev, ha) {
		bnx2fc_dbg(LOG_INIT, "net_config: ha->type = %d, fip_mac = ",
			ha->type);
		printk(KERN_INFO "%2x:%2x:%2x:%2x:%2x:%2x\n", ha->addr[0],
				ha->addr[1], ha->addr[2], ha->addr[3],
				ha->addr[4], ha->addr[5]);

		if ((ha->type == NETDEV_HW_ADDR_T_SAN) &&
		    (is_valid_ether_addr(ha->addr))) {
			memcpy(hba->ctlr.ctl_src_addr, ha->addr, ETH_ALEN);
			sel_san_mac = 1;
			bnx2fc_dbg(LOG_INIT, "Found SAN MAC\n");
		}
	}
	rcu_read_unlock();
#endif

	if (!sel_san_mac)
		return -ENODEV;

	hba->fip_packet_type.type = htons(ETH_P_FIP);
	hba->fip_packet_type.dev = hba->netdev;
#ifndef __VMKLNX__
	hba->fip_packet_type.func = bnx2fc_fip_recv;
	dev_add_pack(&hba->fip_packet_type);
#endif

	hba->fcoe_packet_type.type = __constant_htons(ETH_P_FCOE);
	hba->fcoe_packet_type.dev = hba->netdev;
#ifndef __VMKLNX__
	hba->fcoe_packet_type.func = bnx2fc_rcv;
	dev_add_pack(&hba->fcoe_packet_type);
#endif

	return 0;	
}

static int bnx2fc_attach_transport(void)
{
	bnx2fc_transport_template =
		fc_attach_transport(&bnx2fc_transport_function);

	if (bnx2fc_transport_template == NULL) {
		printk(KERN_ERR PFX "Failed to attach FC transport\n");
		return -ENODEV;
	}

	bnx2fc_vport_xport_template =
		fc_attach_transport(&bnx2fc_vport_xport_function);
	if (bnx2fc_vport_xport_template == NULL) {
		printk(KERN_ERR PFX 
		       "Failed to attach FC transport for vport\n");
		fc_release_transport(bnx2fc_transport_template);
		bnx2fc_transport_template = NULL;
		return -ENODEV;
	}
	return 0;
}
static void bnx2fc_release_transport(void)
{
	fc_release_transport(bnx2fc_transport_template);
	fc_release_transport(bnx2fc_vport_xport_template);
	bnx2fc_transport_template = NULL;
	bnx2fc_vport_xport_template = NULL;
}

static void bnx2fc_interface_release(struct kref *kref)
{
	struct bnx2fc_hba *hba;
	struct net_device *netdev;
	struct net_device *phys_dev;

	hba = container_of(kref, struct bnx2fc_hba, kref);
	bnx2fc_dbg(LOG_INIT, "Interface is being released hba = 0x%p\n", hba);

	netdev = hba->netdev;
	phys_dev = hba->phys_dev;

	/* tear-down FIP controller */
	if (test_and_clear_bit(BNX2FC_CTLR_INIT_DONE, &hba->init_done))
		fcoe_ctlr_destroy(&hba->ctlr);

	/* Free the command manager */
	if (hba->cmd_mgr) {
		bnx2fc_cmd_mgr_free(hba->cmd_mgr);
		hba->cmd_mgr = NULL;
	}
	module_put(THIS_MODULE);
}

static inline void bnx2fc_interface_get(struct bnx2fc_hba *hba)
{
	kref_get(&hba->kref);
}

static inline void bnx2fc_interface_put(struct bnx2fc_hba *hba)
{
	kref_put(&hba->kref, bnx2fc_interface_release);
}
static void bnx2fc_interface_destroy(struct bnx2fc_hba *hba)
{
#ifdef __VMKLNX__
	if ((bnx2fc_vmk_pool == BNX2FC_EN_VMKPOOL) && (hba->bnx2fc_dma_pool))
		vmk_MemPoolDestroy(hba->bnx2fc_dma_pool);
#endif
	bnx2fc_unbind_pcidev(hba);
	kfree(hba);
}


/**
 * bnx2fc_set_capabilities - get new fcoe capabilities
 *
 * @hba:	FCoE interface to set capabilities
 *
 * Creates a new FCoE instance on the given device which include allocating
 *	hba structure, scsi_host and lport structures.
 */
static void bnx2fc_set_capabilities(struct bnx2fc_hba *hba)
{
	struct fcoe_capabilities *fcoe_cap;
	fcoe_cap = &hba->fcoe_cap;

	fcoe_cap->capability1 = BNX2FC_TM_MAX_SQES <<
					FCOE_IOS_PER_CONNECTION_SHIFT;
	fcoe_cap->capability1 |= BNX2FC_NUM_MAX_SESS <<
					FCOE_LOGINS_PER_PORT_SHIFT;
	fcoe_cap->capability2 = BNX2FC_MAX_OUTSTANDING_CMNDS <<
					FCOE_NUMBER_OF_EXCHANGES_SHIFT;
	fcoe_cap->capability2 |= BNX2FC_MAX_NPIV <<
					FCOE_NPIV_WWN_PER_PORT_SHIFT;
	fcoe_cap->capability3 = BNX2FC_NUM_MAX_SESS <<
					FCOE_TARGETS_SUPPORTED_SHIFT;
	fcoe_cap->capability3 |= BNX2FC_MAX_OUTSTANDING_CMNDS <<
					FCOE_OUTSTANDING_COMMANDS_SHIFT;
	fcoe_cap->capability4 = FCOE_CAPABILITY4_STATEFUL;
}

/**
 * bnx2fc_interface_create - create a new fcoe instance
 *
 * @cnic:	pointer to cnic device
 *
 * Creates a new FCoE instance on the given device which include allocating
 *	hba structure, scsi_host and lport structures.
 */
static struct bnx2fc_hba *bnx2fc_interface_create(struct cnic_dev *cnic)
{
	struct bnx2fc_hba *hba;
	int rc;

	hba = kzalloc(sizeof(*hba), GFP_KERNEL);
	if (!hba) {
		printk(KERN_ERR PFX "Unable to allocate hba structure\n");
		return NULL;
	}
	spin_lock_init(&hba->hba_lock);
	mutex_init(&hba->hba_mutex);

	hba->cnic = cnic;
	rc = bnx2fc_bind_pcidev(hba);
	if (rc)
		goto bind_err;
#ifdef __VMKLNX__
	rc = bnx2fc_init_dma_mem_pool(hba);
	if (rc) {
		printk(KERN_ERR PFX "Failed to create dma pool\n");
		goto dma_pool_err;
	}
#endif
	hba->phys_dev = cnic->netdev;
	/* will get overwritten after we do vlan discovery */
	hba->netdev = hba->phys_dev;

	bnx2fc_set_capabilities(hba);

	init_waitqueue_head(&hba->shutdown_wait);
	init_waitqueue_head(&hba->destroy_wait);
	return hba;
bind_err:
	printk(KERN_ERR PFX "create_interface: bind error\n");
#ifdef __VMKLNX__
dma_pool_err:
#endif
	kfree(hba);
	return NULL;
}

#ifdef __VMKLNX__
static int bnx2fc_interface_setup(struct bnx2fc_hba *hba)
#else
static int bnx2fc_interface_setup(struct bnx2fc_hba *hba,
				  enum fip_state fip_mode)
#endif
{
	int rc = 0;
	struct net_device *netdev = hba->netdev;

	dev_hold(netdev);
	kref_init(&hba->kref);

	hba->flags = 0;

	/* Initialize FIP */
#ifdef __VMKLNX__
	fcoe_ctlr_init(&hba->ctlr);
#else
	fcoe_ctlr_init(&hba->ctlr, fip_mode);
#endif

	hba->ctlr.send = bnx2fc_fip_send;
	hba->ctlr.update_mac = bnx2fc_update_src_mac;
	hba->ctlr.get_src_addr = bnx2fc_get_src_mac;
	set_bit(BNX2FC_CTLR_INIT_DONE, &hba->init_done);

	rc = bnx2fc_netdev_setup(hba);
	if (rc)
		goto setup_err;

	hba->next_conn_id = 0;

	memset(hba->tgt_ofld_list, 0, sizeof(hba->tgt_ofld_list));
	hba->num_ofld_sess = 0;

	return 0;

setup_err:
	fcoe_ctlr_destroy(&hba->ctlr);
	dev_put(netdev);
	bnx2fc_interface_put(hba);
	return rc;
}


/**
 * bnx2fc_if_create - Create FCoE instance on a given interface
 *
 * @hba:	FCoE interface to create a local port on
 * @parent:	Device pointer to be the parent in sysfs for the SCSI host
 * @npiv:	Indicates if the port is vport or not
 *
 * Creates a fc_lport instance and a Scsi_Host instance and configure them.
 *
 * Returns:	Allocated fc_lport or an error pointer
 */
static struct fc_lport *bnx2fc_if_create(struct bnx2fc_hba *hba,
				  struct device *parent, int npiv)
{
	struct fc_lport		*lport = NULL;
	struct bnx2fc_port	*port;
	struct Scsi_Host 	*shost;
	struct fc_vport		*vport;
	int 			rc = 0;

	/* Allocate Scsi_Host structure */
	if (!npiv) {
		lport = libfc_host_alloc(&bnx2fc_shost_template,
					  sizeof(struct bnx2fc_port));
	} else {
		vport = dev_to_vport(parent);
#ifdef __BNX2FC_SLES11SP1__
		lport = libfc_vport_create2(vport,
					   sizeof(struct bnx2fc_port));	
#else
		lport = libfc_vport_create(vport,
					   sizeof(struct bnx2fc_port));	
#endif
	}
	bnx2fc_dbg(LOG_INIT, "lport: %p, netdev: %s, fcoe_net_dev %p\n",
		   lport, hba->netdev->name, hba->fcoe_net_dev);

	if (!lport) {
		printk(KERN_ERR PFX "could not allocate scsi host structure\n");
		return NULL;
	}

	shost = lport->host;
	port = lport_priv(lport);
	port->lport = lport;
	port->hba = hba;
	INIT_WORK(&port->destroy_work, bnx2fc_destroy_work);

	/* Configure bnx2fc_port */
	rc = bnx2fc_lport_config(lport);
	if (rc)
		goto lp_config_err;

	if (npiv) {
		vport = dev_to_vport(parent);
		printk(KERN_ERR PFX "Setting vport names, 0x%llX 0x%llX\n",
			vport->node_name, vport->port_name);
		fc_set_wwnn(lport, vport->node_name);
		fc_set_wwpn(lport, vport->port_name);
	}
	/* Configure netdev and networking properties of the lport */
	rc = bnx2fc_net_config(lport);
	if (rc) {
		printk(KERN_ERR PFX "Error on bnx2fc_net_config\n");
		goto lp_config_err;
	}

#if defined(__VMKLNX__)
	/* Mark transport types as FCoE */
	shost->xportFlags = VMKLNX_SCSI_TRANSPORT_TYPE_FCOE;
#endif /* defined(__VMKLNX__) */

#ifdef __VMKLNX__
	device_initialize(&port->dummy_dev);
	port->dummy_dev.parent = &hba->pcidev->dev;
	rc = bnx2fc_shost_config(lport, &port->dummy_dev);
#else
	rc = bnx2fc_shost_config(lport);
#endif

	if (rc != 0) {
		printk(KERN_ERR PFX "Couldnt configure shost for %s\n",
			hba->netdev->name);
		goto lp_config_err;
	}

	/* Initialize the libfc library */
#ifdef __VMKLNX__
	vmklnx_scsi_attach_cna(lport->host, hba->fcoe_net_dev);
	rc = fcoe_libfc_config(lport, &bnx2fc_libfc_fcn_templ);
#else
	rc = fcoe_libfc_config(lport, &hba->ctlr, &bnx2fc_libfc_fcn_templ, 1);
#endif
	if (rc) {
		printk(KERN_ERR PFX "Couldnt configure libfc\n");
		goto shost_err;
	}
	fc_host_port_type(lport->host) = FC_PORTTYPE_UNKNOWN;

	/* Allocate exchange manager */
	if (!npiv) {
		rc = bnx2fc_em_config(lport);
		if (rc) {
			printk(KERN_ERR PFX "Error on bnx2fc_em_config\n");
			goto shost_err;
		}
	}

	bnx2fc_interface_get(hba);
	return lport;

shost_err:
	scsi_remove_host(shost);
lp_config_err:
	scsi_host_put(lport->host);
	return NULL;
}

static void bnx2fc_netdev_cleanup(struct bnx2fc_hba *hba)
{
#ifndef __VMKLNX__
	__dev_remove_pack(&hba->fcoe_packet_type);
	__dev_remove_pack(&hba->fip_packet_type);
	synchronize_net();
#endif
	if (hba->fcoe_net_dev) {
		if (vmklnx_cna_remove_macaddr(hba->fcoe_net_dev, hba->ctlr.ctl_src_addr)) {
			printk("bnx2fc: %s: Unable to remove FCOE Controller MAC filter.\n",
				hba->fcoe_net_dev->name);
		} else {
			printk("bnx2fc: %s: Removed FCOE Controller MAC filter.\n",
				hba->fcoe_net_dev->name);
		}
	}
}

static void bnx2fc_if_destroy(struct fc_lport *lport)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;

	bnx2fc_dbg(LOG_INIT, "ENTERED bnx2fc_if_destroy\n");
	/* Stop the transmit retry timer */
	del_timer_sync(&port->timer);

	/* Free existing transmit skbs */
	bnx2fc_clean_pending_queue(lport);

	bnx2fc_interface_put(hba);

	/* Free queued packets for the receive thread */
	bnx2fc_clean_rx_queue(lport);

	/* Detach from scsi-ml */
	fc_remove_host(lport->host);
	scsi_remove_host(lport->host);

	/*
	 * Note that only the physical lport will have the exchange manager.
	 * for vports, this function is NOP
	 */
	fc_exch_mgr_free(lport);

	/* Free memory used by statistical counters */
	fc_lport_free_stats(lport);

	/* Release Scsi_Host */
	scsi_host_put(lport->host);
}

/**
 * bnx2fc_destroy() - Destroy a bnx2fc FCoE interface
 * @buffer: The name of the Ethernet interface to be destroyed
 * @kp:     The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
#ifdef __VMKLNX__
static int bnx2fc_fcoe_destroy(struct net_device *netdevice)
#else
static int bnx2fc_destroy(const char *buffer, struct kernel_param *kp)
#endif
{
	struct bnx2fc_hba *hba = NULL;
	struct net_device *netdev;
	struct net_device *phys_dev;
	int rc = 0;

#ifndef __VMKLNX__
	if (!rtnl_trylock())
		return restart_syscall();
#endif

	mutex_lock(&bnx2fc_dev_lock);
#ifdef CONFIG_SCSI_BNX2X_FCOE_MODULE
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto netdev_err;
	}
#endif
#ifdef __VMKLNX__
	netdev = netdevice;
#else
	netdev = bnx2fc_if_to_netdev(buffer);
#endif
	if (!netdev) {
		rc = -ENODEV;
		goto netdev_err;
	}
#ifdef __VMKLNX__
	dev_hold(netdev);
#endif

	/* obtain physical netdev */
#ifdef __VMKLNX__
	phys_dev = dev_get_by_name(netdev->name);
#else
	if (netdev->priv_flags & IFF_802_1Q_VLAN)
		phys_dev = vlan_dev_real_dev(netdev);
	else {
		printk(KERN_ERR PFX "Not a vlan device\n");
		rc = -ENODEV;
		goto put_err;
	}
#endif

	hba = bnx2fc_hba_lookup(phys_dev);
	if (!hba || !hba->ctlr.lp) {
		rc = -ENODEV;
		printk(KERN_ERR PFX "bnx2fc_destroy: hba not found\n");
		goto put_err;
	}

	if (!test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
		printk(KERN_ERR PFX "bnx2fc_destroy: Create not called\n");
		goto put_err;
	}

	bnx2fc_netdev_cleanup(hba);

	bnx2fc_stop(hba);

	bnx2fc_if_destroy(hba->ctlr.lp);

#ifdef __FCOE_IF_RESTART_WQ__
	destroy_workqueue(hba->fcoe_if_restart_wq);
#endif
	destroy_workqueue(hba->timer_work_queue);

	if (test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done))
		bnx2fc_fw_destroy(hba);
	clear_bit(BNX2FC_CREATE_DONE, &hba->init_done);
put_err:
	dev_put(netdev);
netdev_err:
	mutex_unlock(&bnx2fc_dev_lock);
#ifndef __VMKLNX__
	rtnl_unlock();
#endif
	return rc;
}

static void bnx2fc_destroy_work(struct work_struct *work)
{
	struct bnx2fc_port *port;
	struct fc_lport *lport;
	struct bnx2fc_hba *hba;

	port = container_of(work, struct bnx2fc_port, destroy_work);
	lport = port->lport;
	hba = port->hba;

	bnx2fc_dbg(LOG_INIT, "Entered bnx2fc_destroy_work\n");

	bnx2fc_port_shutdown(lport);
	rtnl_lock();
	mutex_lock(&bnx2fc_dev_lock);
	bnx2fc_if_destroy(lport);
	mutex_unlock(&bnx2fc_dev_lock);
	rtnl_unlock();
}

static void bnx2fc_unbind_adapter_devices(struct bnx2fc_hba *hba)
{
	bnx2fc_dbg(LOG_INIT, "ENTERED unbind_adapter\n");
	bnx2fc_free_fw_resc(hba);
	bnx2fc_free_task_ctx(hba);
}

/**
 * bnx2fc_bind_adapter_devices - binds bnx2fc adapter with the associated
 *			pci structure
 * @hba:		Adapter instance
 **/
static int bnx2fc_bind_adapter_devices(struct bnx2fc_hba *hba)
{
	if (bnx2fc_setup_task_ctx(hba))
		goto mem_err;

	if (bnx2fc_setup_fw_resc(hba))
		goto mem_err;

	return 0;
mem_err:
	bnx2fc_unbind_adapter_devices(hba);
	return -ENOMEM;
}

static int bnx2fc_bind_pcidev(struct bnx2fc_hba *hba)
{
	struct cnic_dev *cnic;

	if (!hba->cnic) {
		printk(KERN_ERR PFX "cnic is NULL\n");
		return -ENODEV;
	}
	cnic = hba->cnic;
	hba->pcidev = cnic->pcidev;
	if (!hba->pcidev)
		return -ENODEV;

	pci_dev_get(hba->pcidev);

	switch (hba->pcidev->device) {
	case PCI_DEVICE_ID_NX2_57712:
	case PCI_DEVICE_ID_NX2_57712E:
		strncpy(hba->sym_name, "BCM57712", BNX2FC_SYM_NAME_LEN);
		break;
	case PCI_DEVICE_ID_NX2_57800:
	case PCI_DEVICE_ID_NX2_57800_MF:
		strncpy(hba->sym_name, "BCM57800", BNX2FC_SYM_NAME_LEN);
		break;
	case PCI_DEVICE_ID_NX2_57810:
	case PCI_DEVICE_ID_NX2_57810_MF:
		strncpy(hba->sym_name, "BCM57810", BNX2FC_SYM_NAME_LEN);
		break;
	case PCI_DEVICE_ID_NX2_57840_MF:
	case PCI_DEVICE_ID_NX2_57840_2_20:
	case PCI_DEVICE_ID_NX2_57840_4_10:
		strncpy(hba->sym_name, "BCM57840", BNX2FC_SYM_NAME_LEN);
		break;
	default:
		printk(KERN_ERR PFX "Unknown device id 0x%x\n",
		       hba->pcidev->device);
		break;
	}

	return 0;
}

static void bnx2fc_unbind_pcidev(struct bnx2fc_hba *hba)
{
	if (hba->pcidev)
		pci_dev_put(hba->pcidev);

	strcpy(hba->sym_name, "");
	hba->pcidev = NULL;
}

static void bnx2fc_restart_fcoe_disc(struct bnx2fc_hba *hba)
{
	struct fc_lport *lport = hba->ctlr.lp;

	printk(KERN_ERR PFX "%s - %s - work entered\n",
			__FUNCTION__, hba->netdev->name);
	lport->tt.frame_send = bnx2fc_xmit;
	bnx2fc_start_disc(hba);
}

#ifdef __FCOE_IF_RESTART_WQ__
static void bnx2fc_restart_fcoe_init(struct work_struct *work)
{
	struct bnx2fc_hba *hba = container_of(work, struct bnx2fc_hba,
						 fcoe_disc_work.work);
	bnx2fc_restart_fcoe_disc(hba);
}
#endif

/**
 * bnx2fc_ulp_start - cnic callback to initialize & start adapter instance
 *
 * @handle:	transport handle pointing to adapter struture
 *
 * This function maps adapter structure to pcidev structure and initiates
 *	firmware handshake to enable/initialize on-chip FCoE components.
 *	This bnx2fc - cnic interface api callback is used after following
 *	conditions are met -
 *	a) underlying network interface is up (marked by event NETDEV_UP
 *		from netdev
 *	b) bnx2fc adatper structure is registered.
 */
static void bnx2fc_ulp_start(void *handle)
{
	struct bnx2fc_hba *hba = handle;

	bnx2fc_dbg(LOG_INIT, "Entered hba = %p\n", hba);
	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
		int rc;

		printk(KERN_ERR PFX "bnx2fc_start: %s ; re-init FCoE CNA Que\n",
			hba->fcoe_net_dev->name);
	 	vmknetddi_queueops_getset_state(hba->fcoe_net_dev, true);
		rc = vmklnx_cna_reinit_queues(hba->fcoe_net_dev);
		if (rc == 0) {
			hba->cna_init_queue++;
			set_bit(BNX2FC_CNA_QUEUES_ALLOCED, &hba->flags2);
		}
	}

	mutex_lock(&bnx2fc_dev_lock);

	if (test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done))
		goto start_disc;

	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done))
		bnx2fc_fw_init(hba);
start_disc:
	mutex_unlock(&bnx2fc_dev_lock);

	bnx2fc_dbg(LOG_INIT, "bnx2fc started.\n");

	/* Kick off Fabric discovery*/
	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
#ifdef __FCOE_IF_RESTART_WQ__
		printk(KERN_ERR PFX "ulp_init: start discovery,"
				"queue fcoe_if_restart_wq\n");
		INIT_DELAYED_WORK(&hba->fcoe_disc_work,
				  bnx2fc_restart_fcoe_init);
		queue_delayed_work(hba->fcoe_if_restart_wq,
				   &hba->fcoe_disc_work,
				   msecs_to_jiffies(2000));
#else
		bnx2fc_restart_fcoe_disc(hba);
#endif
	}
}

static void bnx2fc_port_shutdown(struct fc_lport *lport)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;

	bnx2fc_dbg(LOG_INIT, "ENTERED: flush fcoe_if_restart_wq\n");
#ifdef __FCOE_IF_RESTART_WQ__
	if (hba)
		flush_workqueue(hba->fcoe_if_restart_wq);
#endif
	fc_fabric_logoff(lport);
	fc_lport_destroy(lport);
}

/**
 * bnx2fc_stop - cnic callback to shutdown adapter instance
 * @handle:	transport handle pointing to adapter structure
 *
 * Driver checks if adapter is already in shutdown mode, if not start
 * 	the shutdown process.
 **/
static void bnx2fc_stop(struct bnx2fc_hba *hba)
{
	struct fc_lport *lport;
	struct fc_lport *vport;
	int stop = (test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done) &&
		    test_bit(BNX2FC_CREATE_DONE, &hba->init_done));

	bnx2fc_dbg(LOG_INIT, "ENTERED: init_done = %ld\n", hba->init_done);
	if (stop) {
		lport = hba->ctlr.lp;
		bnx2fc_port_shutdown(lport);
		bnx2fc_dbg(LOG_INIT, "bnx2fc_stop: waiting for %d "
				"offloaded sessions\n",
				hba->num_ofld_sess);
		wait_event_interruptible(hba->shutdown_wait,
					 (hba->num_ofld_sess == 0));
		mutex_lock(&lport->lp_mutex);
		list_for_each_entry(vport, &lport->vports, list)
			fc_host_port_type(vport->host) = FC_PORTTYPE_UNKNOWN;
		mutex_unlock(&lport->lp_mutex);
		fc_host_port_type(lport->host) = FC_PORTTYPE_UNKNOWN;
		fcoe_ctlr_link_down(&hba->ctlr);
		clear_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);

		printk(KERN_ERR PFX "bnx2fc_stop: %s ; Clean-up FCoE CNA Queues\n",
			hba->fcoe_net_dev->name);

		bnx2fc_clean_pending_queue(lport);
	}
#if defined (__FCOE_CNA_QUEUE_CLEANUP__)
	if (test_bit(BNX2FC_CNA_QUEUES_ALLOCED, &hba->flags2)) {
		printk(KERN_ERR "%s - dev->netq_state 0x%lx\n", __FUNCTION__, hba->fcoe_net_dev->netq_state);
		vmknetddi_queueops_getset_state(hba->fcoe_net_dev, false);
		vmklnx_cna_cleanup_queues(hba->fcoe_net_dev);
		hba->cna_clean_queue++;
		
	}
#endif /* !defined (__VMKLNX__) */
	if (stop) {
		mutex_lock(&hba->hba_mutex);
		clear_bit(ADAPTER_STATE_UP, &hba->adapter_state);
		clear_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state);

		clear_bit(ADAPTER_STATE_READY, &hba->adapter_state);
		mutex_unlock(&hba->hba_mutex);
	}
}

static int bnx2fc_fw_init(struct bnx2fc_hba *hba)
{
#define BNX2FC_INIT_POLL_TIME		(1000 / HZ)
	int rc = -1;
	int i = HZ;

	rc = bnx2fc_bind_adapter_devices(hba);
	if (rc) {
		printk(KERN_ALERT PFX
			"bnx2fc_bind_adapter_devices failed - rc = %d\n", rc);
		goto err_out;
	}

	rc = bnx2fc_send_fw_fcoe_init_msg(hba);
	if (rc) {
		printk(KERN_ALERT PFX
			"bnx2fc_send_fw_fcoe_init_msg failed - rc = %d\n", rc);
		goto err_unbind;
	}

	/*
	 * Wait until the adapter init message is complete, and adapter
	 * state is UP.
	 */
	while (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state) && i--)
		msleep(BNX2FC_INIT_POLL_TIME);

	if (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state)) {
		printk(KERN_ERR PFX "bnx2fc_start: %s failed to initialize.  "
				"Ignoring...\n",
				hba->cnic->netdev->name);
		rc = -1;
		goto err_unbind;
	}


	/* Mark HBA to indicate that the FW INIT is done */
	set_bit(BNX2FC_FW_INIT_DONE, &hba->init_done);
	return 0;

err_unbind:
	bnx2fc_unbind_adapter_devices(hba);
err_out:
	return rc;
}
static void bnx2fc_fw_destroy(struct bnx2fc_hba *hba)
{
	if (test_and_clear_bit(BNX2FC_FW_INIT_DONE, &hba->init_done)) {
		hba->flags = 0;
		if (bnx2fc_send_fw_fcoe_destroy_msg(hba) == 0) {
			init_timer(&hba->destroy_timer);
			hba->destroy_timer.expires = BNX2FC_FW_TIMEOUT +
								jiffies;
			hba->destroy_timer.function = bnx2fc_destroy_timer;
			hba->destroy_timer.data = (unsigned long)hba;
			add_timer(&hba->destroy_timer);
			wait_event_interruptible(hba->destroy_wait,
					 test_bit(BNX2FC_FLAG_DESTROY_CMPL,
						  &hba->flags));
			clear_bit(BNX2FC_FLAG_DESTROY_CMPL, &hba->flags);
			/* This should never happen */
			bnx2fc_dbg(LOG_INIT, "hba->flags = 0x%x\n", hba->flags);
			if (signal_pending(current))
				flush_signals(current);

			del_timer_sync(&hba->destroy_timer);
		}
		bnx2fc_unbind_adapter_devices(hba);
	}
}

/**
 * bnx2fc_ulp_stop - cnic callback to shutdown adapter instance
 * @handle:	transport handle pointing to adapter structure
 *
 * Driver checks if adapter is already in shutdown mode, if not start
 *	the shutdown process.
 */
static void bnx2fc_ulp_stop(void *handle)
{
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)handle;

	printk(KERN_ERR "ULP_STOP\n");

	mutex_lock(&bnx2fc_dev_lock);
	bnx2fc_stop(hba);
	bnx2fc_fw_destroy(hba);
	mutex_unlock(&bnx2fc_dev_lock);
}

static void bnx2fc_start_disc(struct bnx2fc_hba *hba)
{
	struct fc_lport *lport;

	printk(KERN_ERR PFX "Entered bnx2fc_start_disc\n");
	/* Kick off FIP/FLOGI */
	if (!test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done)) {
		printk(KERN_ERR PFX "Init not done yet\n");
		return;
	}

	lport = hba->ctlr.lp;
	bnx2fc_dbg(LOG_INIT, "Waiting (max 3sec) for link up \n");

	if (bnx2fc_link_ok_wait(lport)) 
		return;

	bnx2fc_dbg(LOG_INIT, "init_one - ctlr_link_up\n");
	fcoe_ctlr_link_up(&hba->ctlr);
	fc_host_port_type(lport->host) = FC_PORTTYPE_NPORT;
	set_bit(ADAPTER_STATE_READY, &hba->adapter_state);

        if (fc_set_mfs(lport, BNX2FC_MFS)) {
                printk(KERN_ERR PFX "start_disc: set mfs failure\n");
                return;
        }

	bnx2fc_dbg(LOG_INIT, "Calling fc_fabric_login \n");
	fc_fabric_login(lport);
}


#ifndef __VMKLNX__
static struct net_device *bnx2fc_if_to_netdev(const char *buffer)
{
	char *cp;
	char ifname[IFNAMSIZ + 2];

	if (buffer) {
		strlcpy(ifname, buffer, IFNAMSIZ);
		cp = ifname + strlen(ifname);
		while (--cp >= ifname && *cp == '\n')
			*cp = '\0';
		return dev_get_by_name(&init_net, ifname);
	}
	return NULL;
}
#endif

/**
 * bnx2fc_ulp_init - Initialize an adapter instance
 *
 * @dev :	cnic device handle
 * Called from cnic_register_driver() context to initialize all
 *	enumerated cnic devices. This routine allocates adapter structure
 *	and other device specific resources.
 */
static void bnx2fc_ulp_init(struct cnic_dev *dev)
{
	struct bnx2fc_hba *hba = NULL;
	int rc = 0;

	bnx2fc_dbg(LOG_INIT, "bnx2fc_ulp_init\n");
	/* bnx2fc works only when bnx2x is loaded & the device supports
	 * FCoE offload feature
	 */
	if (!test_bit(CNIC_F_BNX2X_CLASS, &dev->flags) || !dev->max_fcoe_conn) {
		printk(KERN_ERR PFX "bnx2fc FCoE not supported on %s,"
				    " flags: %lx, fcoe_conn=%d\n",
			dev->netdev->name, dev->flags, dev->max_fcoe_conn);
		return;
	}

	/* Configure FCoE interface */
	hba = bnx2fc_interface_create(dev);
	if (!hba) {
		printk(KERN_ERR PFX "hba initialization failed\n");
		return;
	}

	/* Add HBA to the adapter list */
	mutex_lock(&bnx2fc_dev_lock);
	list_add_tail(&hba->link, &adapter_list);
	adapter_count++;
	mutex_unlock(&bnx2fc_dev_lock);

	dev->fcoe_cap = &hba->fcoe_cap;
	clear_bit(BNX2FC_CNIC_REGISTERED, &hba->flags2);
	rc = dev->register_device(dev, CNIC_ULP_FCOE, 
						(void *) hba);
	if (rc) {
		printk(KERN_ALERT PFX "register_device failed, rc = %d\n", rc);
		return;
	} else {
		set_bit(BNX2FC_CNIC_REGISTERED, &hba->flags2);
	}

	return;
}

/**
 * bnx2fc_create() - Create bnx2fc FCoE interface
 * @buffer: The name of Ethernet interface to create on
 * @kp:     The associated kernel param
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
#ifdef __VMKLNX__
static int bnx2fc_fcoe_create(struct net_device *netdevice)
#else
static int bnx2fc_create(const char *buffer, struct kernel_param *kp)
#endif
{
#ifndef __VMKLNX__
	enum fip_state fip_mode = (enum fip_state)(long)kp->arg;
#endif
	struct bnx2fc_hba *hba = NULL;
	struct cnic_dev *dev;
	struct net_device *netdev;
	struct net_device *phys_dev;
	struct fc_lport *lport;
	struct ethtool_drvinfo drvinfo;
	int rc = 0;
	int vlan_id;

	bnx2fc_dbg(LOG_INIT, "Entered bnx2fc_create\n");
	
#ifndef __VMKLNX__
	if (!rtnl_trylock()) {
		printk(KERN_ERR "trying for rtnl_lock\n");
		return restart_syscall();
	}
#endif
	mutex_lock(&bnx2fc_dev_lock);

#ifdef CONFIG_SCSI_BNX2X_FCOE_MODULE
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto mod_err;
	}
#endif

	if (!try_module_get(THIS_MODULE)) {
		rc = -EINVAL;
		goto mod_err;
	}

#ifdef __VMKLNX__
	netdev = netdevice;
#else
	netdev = bnx2fc_if_to_netdev(buffer);
#endif
	if (!netdev) {
		rc = -EINVAL;
		goto netdev_err;
	}
#if defined(__VMKLNX__)
	dev_hold(netdev);
#endif /* defined(__VMKLNX__) */


#ifdef __VMKLNX__
	phys_dev = dev_get_by_name(netdev->name);

#ifdef __FCOE_ENABLE_BOOT__
	vlan_id = vmklnx_cna_get_vlan_tag(netdev) & VLAN_VID_MASK;
#else
	vlan_id = vmklnx_cna_get_vlan_tag(netdev);
#endif

printk(KERN_ERR PFX "%s - phys_dev %p, cna device %p, vlan_id %x\n", __FUNCTION__, phys_dev, netdevice, vlan_id);
	
#else
	/* obtain physical netdev */
	if (netdev->priv_flags & IFF_802_1Q_VLAN) {
		phys_dev = vlan_dev_real_dev(netdev);
		vlan_id = vlan_dev_vlan_id(netdev);
	} else {
		printk(KERN_ERR PFX "Not a vlan device\n");
		rc = -EINVAL;
		goto putdev_err;
	}
#endif
printk(KERN_ERR PFX "%s - check if it's BRCM NX2 device phys_dev %p\n", __FUNCTION__, phys_dev);
	
	/* verify if the physical device is a netxtreme2 device */
	if (phys_dev->ethtool_ops && phys_dev->ethtool_ops->get_drvinfo) {
		memset(&drvinfo, 0, sizeof(drvinfo));
		phys_dev->ethtool_ops->get_drvinfo(phys_dev, &drvinfo);
		if (strcmp(drvinfo.driver, "bnx2x")) {
			printk(KERN_ERR PFX "Not a netxtreme2 device\n");
			rc = -EINVAL;
			goto putdev_err;
		}
	} else {
		printk(KERN_ERR PFX "unable to obtain drv_info\n");
		rc = -EINVAL;
		goto putdev_err;
	}

	printk(KERN_ERR PFX "phys_dev is netxtreme2 device\n");

	/* obtain hba and initialize rest of the structure */
	hba = bnx2fc_hba_lookup(phys_dev);
	if (!hba) {
		rc = -ENODEV;
		printk(KERN_ERR PFX "bnx2fc_create: hba not found\n");
		goto putdev_err;
	}

	if (!test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done)) {
		rc = bnx2fc_fw_init(hba);
		if (rc)
			goto netdev_err;
	}

	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
		rc = -EEXIST;
		goto putdev_err;
	}

	/* update netdev with vlan netdev */
	hba->netdev = netdev;
	dev = hba->cnic;
#ifdef __VMKLNX__
	hba->fcoe_net_dev = netdev;

#ifdef __FCOE_ENABLE_BOOT__
	if (vlan_id != 0 && vlan_id < 4095) {
		hba->vlan_id = vlan_id;
		set_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);

		/* For boot adapters VLAN ID is read from the boot table and not
		 * obtained through VLAN DISCOVERY process. A valid VID at this
		 * time means this device is a FCoE boot adapter
		 */
		set_bit(BNX2FC_ADAPTER_BOOT, &hba->adapter_type);
	}
	else
		clear_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
#else
	clear_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
#endif

#else
	hba->vlan_id = vlan_id;
	set_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
#endif

#ifdef __VMKLNX__
	rc = bnx2fc_interface_setup(hba);
#else
	rc = bnx2fc_interface_setup(hba, fip_mode);
#endif
	if (rc) {
		printk(KERN_ERR PFX "bnx2fc_interface_setup failed\n");
		goto ifput_err;
	}

	hba->timer_work_queue =
			create_singlethread_workqueue("bnx2fc_timer_wq");
	if (!hba->timer_work_queue) {
		printk(KERN_ERR PFX "ulp_init could not create timer_wq\n");
		rc = -EINVAL;
		goto ifput_err;
	}
#ifdef __FCOE_IF_RESTART_WQ__
	hba->fcoe_if_restart_wq =
			create_singlethread_workqueue("bnx2fc_fcoe_if_restart");
	if (!hba->fcoe_if_restart_wq) {
		printk(KERN_ERR PFX "ulp_init could not create fcoe_if_restart_wq \n");
		rc = -EINVAL;
		goto restart_wq_err;
	}
#endif

#ifdef __VMKLNX__
	lport = bnx2fc_if_create(hba, NULL, 0);
#else
	lport = bnx2fc_if_create(hba, &netdev->dev, 0);
#endif
	if (!lport) {
		printk(KERN_ERR PFX "Failed to create interface (%s)\n",
			netdev->name);
		bnx2fc_netdev_cleanup(hba);
		rc = -EINVAL;
		goto if_create_err;
	}

	lport->boot_time = jiffies;

	/* Make this master N_port */
	hba->ctlr.lp = lport;

	set_bit(BNX2FC_CREATE_DONE, &hba->init_done);
	/* It is safe to assume LinCNA has created L2 NetQ's before calling
	 * fcoe_create()
	 */
	set_bit(BNX2FC_CNA_QUEUES_ALLOCED, &hba->flags2);

	printk(KERN_ERR PFX "create: START DISC\n");
	bnx2fc_start_disc(hba);
	/*
	 * Release from kref_init in bnx2fc_interface_setup, on success
	 * lport should be holding a reference taken in bnx2fc_if_create
	 */
	bnx2fc_interface_put(hba);
	/* put netdev that was held while calling dev_get_by_name */
	dev_put(netdev);
	mutex_unlock(&bnx2fc_dev_lock);

#ifndef __VMKLNX__
	rtnl_unlock();
#endif
	return 0;

if_create_err:
#ifdef __FCOE_IF_RESTART_WQ__
	destroy_workqueue(hba->fcoe_if_restart_wq);
restart_wq_err:
#endif
	destroy_workqueue(hba->timer_work_queue);
ifput_err:
	bnx2fc_interface_put(hba);
putdev_err:
	dev_put(netdev);
netdev_err:
	module_put(THIS_MODULE);
mod_err:
	mutex_unlock(&bnx2fc_dev_lock);
#ifndef __VMKLNX__
	rtnl_unlock();
#endif
	return rc;
}

/**
 * bnx2fc_find_hba_for_cnic - maps cnic instance to bnx2fc adapter instance
 *
 * @cnic:	Pointer to cnic device instance
 *
 **/
static struct bnx2fc_hba *bnx2fc_find_hba_for_cnic(struct cnic_dev *cnic)
{
	struct list_head *list;
	struct list_head *temp;
	struct bnx2fc_hba *hba;

	/* Called with bnx2fc_dev_lock held */
	list_for_each_safe(list, temp, &adapter_list) {
		hba = (struct bnx2fc_hba *)list;
		if (hba->cnic == cnic)
			return hba;
	}
	return NULL;
}

static struct bnx2fc_hba *bnx2fc_hba_lookup(struct net_device *phys_dev)
{
	struct list_head *list;
	struct list_head *temp;
	struct bnx2fc_hba *hba;

	/* Called with bnx2fc_dev_lock held */
	list_for_each_safe(list, temp, &adapter_list) {
		hba = (struct bnx2fc_hba *)list;
		if (hba->phys_dev == phys_dev)
			return hba;
	}
	printk(KERN_ERR PFX "hba_lookup: hba NULL\n");
	return NULL;
}

/**
 * bnx2fc_ulp_exit - shuts down adapter instance and frees all resources
 *
 * @dev		cnic device handle
 */
static void bnx2fc_ulp_exit(struct cnic_dev *dev)
{
	struct bnx2fc_hba *hba = NULL;

	bnx2fc_dbg(LOG_INIT, "Entered\n");

	if (!test_bit(CNIC_F_BNX2X_CLASS, &dev->flags)) {
		printk(KERN_ERR PFX "bnx2fc port check: %s, flags: %lx\n",
			dev->netdev->name, dev->flags);
		return;
	}

	mutex_lock(&bnx2fc_dev_lock);
	hba = bnx2fc_find_hba_for_cnic(dev);
	if (!hba) {
		printk(KERN_ERR PFX "hba not found, dev 0%p\n", dev);
		mutex_unlock(&bnx2fc_dev_lock);
		return;
	}

	bnx2fc_dbg(LOG_INIT, "removed hba 0x%p from adapter list\n", hba);
	list_del_init(&hba->link);
	adapter_count--;

	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done)) {
		/* destroy not called yet, move to quiesced list */
		bnx2fc_netdev_cleanup(hba);
		bnx2fc_if_destroy(hba->ctlr.lp);
	}
	mutex_unlock(&bnx2fc_dev_lock);

	bnx2fc_ulp_stop(hba);
	/* unregister cnic device */
	if (test_and_clear_bit(BNX2FC_CNIC_REGISTERED, &hba->flags2))
		hba->cnic->unregister_device(hba->cnic, CNIC_ULP_FCOE);
	bnx2fc_interface_destroy(hba);
}

/**
 * bnx2fc_fcoe_reset() - Resets the fcoe
 * @shost: shost the reset is from
 *
 * Returns: always 0
 */
static int bnx2fc_fcoe_reset(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);
	fc_lport_reset(lport);
	return 0;
}

/**
 * bnx2fc_mod_init - module init entry point
 *
 * Initialize driver wide global data structures, and register
 * with cnic module
 **/
static int __init bnx2fc_mod_init(void)
{
	struct bnx2fc_global_s *bg;
	struct task_struct *l2_thread;
	int rc = 0;

	printk(KERN_INFO PFX "%s", version);
	INIT_LIST_HEAD(&adapter_list);
	mutex_init(&bnx2fc_dev_lock);

	adapter_count = 0;

	/* Attach FC transport template */
	rc = bnx2fc_attach_transport();
	if (rc)
		return rc;

#if defined(__VMKLNX__)
	if (fcoe_attach_transport(&bnx2fc_fcoe_template)) {
		printk(KERN_ERR PFX "Register Template Failed\n");
	} else {
		bnx2fc_fcoe_xport_registered = 1;
		printk(KERN_ERR "Registered Template Successfully\n.");
	}
#endif /* defined(__VMKLNX__) */

	bg = &bnx2fc_global;
	skb_queue_head_init(&bg->fcoe_rx_list);
	l2_thread = kthread_create(bnx2fc_l2_rcv_thread,
				   (void *)bg,
				   "bnx2fc_l2_thread");
	if (IS_ERR(bg->l2_thread)) {
		rc = PTR_ERR(l2_thread);
		return rc;
	}
	wake_up_process(l2_thread);
	spin_lock_bh(&bg->fcoe_rx_list.lock);
	bg->l2_thread = l2_thread;
	spin_unlock_bh(&bg->fcoe_rx_list.lock);

	cnic_register_driver(CNIC_ULP_FCOE, &bnx2fc_cnic_cb);
	return 0;
}

static void __exit bnx2fc_mod_exit(void)
{
	LIST_HEAD(to_be_deleted);
	struct bnx2fc_hba *hba, *next;
	struct bnx2fc_global_s *bg;
	struct task_struct *l2_thread;
	struct sk_buff *skb;

	/*
	 * NOTE: Since cnic calls register_driver routine rtnl_lock,
	 * it will have higher precedence than bnx2fc_dev_lock.
	 * unregister_device() cannot be called with bnx2fc_dev_lock
	 * held.
	 */
	mutex_lock(&bnx2fc_dev_lock);
	list_splice(&adapter_list, &to_be_deleted);
	INIT_LIST_HEAD(&adapter_list);
	adapter_count = 0;
	mutex_unlock(&bnx2fc_dev_lock);

#if defined (__VMKLNX__)
	if (bnx2fc_fcoe_xport_registered) {
        	if (fcoe_release_transport(&bnx2fc_fcoe_template)) {
			printk(KERN_ERR PFX "Unregister template failed\n");
		} else {
			bnx2fc_fcoe_xport_registered = 0;
			printk(KERN_ERR "Unregistered template successfully\n.");
		}
	}
#endif /* defined (__VMKLNX__) */

	/* Unregister with cnic */
	list_for_each_entry_safe(hba, next, &to_be_deleted, link) {
		list_del_init(&hba->link);
		printk(KERN_ERR PFX "MOD_EXIT:destroy hba = 0x%p, kref = %d\n",
			hba, atomic_read(&hba->kref.refcount));
		bnx2fc_ulp_stop(hba);
		/* unregister cnic device */
		if (test_and_clear_bit(BNX2FC_CNIC_REGISTERED,
				       &hba->flags2))
			hba->cnic->unregister_device(hba->cnic, CNIC_ULP_FCOE);
		bnx2fc_interface_destroy(hba);
	}
	cnic_unregister_driver(CNIC_ULP_FCOE);

	/* Destroy global thread */
	bg = &bnx2fc_global;
	spin_lock_bh(&bg->fcoe_rx_list.lock);
	l2_thread = bg->l2_thread;
	bg->l2_thread = NULL;
	while ((skb = __skb_dequeue(&bg->fcoe_rx_list)) != NULL)
		kfree_skb(skb);

	spin_unlock_bh(&bg->fcoe_rx_list.lock);

	if (l2_thread)
		kthread_stop(l2_thread);

	/* flush any async interface destroy */
	flush_scheduled_work();
	/* flush out VN_Ports scheduled for destruction */
	flush_scheduled_work();
	/*
	 * detach from scsi transport
	 * must happen after all destroys are done
	 */
	bnx2fc_release_transport();
}
			
module_init(bnx2fc_mod_init);
module_exit(bnx2fc_mod_exit);

static struct fc_function_template bnx2fc_transport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = (sizeof(struct fc_rport_libfc_priv) +
				sizeof(struct bnx2fc_rport)),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = bnx2fc_get_host_stats,

	.issue_fc_host_lip = bnx2fc_fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

	.vport_create = bnx2fc_vport_create,
	.vport_delete = bnx2fc_vport_destroy,
	.vport_disable = bnx2fc_vport_disable,
};

static struct fc_function_template bnx2fc_vport_xport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = (sizeof(struct fc_rport_libfc_priv) +
				sizeof(struct bnx2fc_rport)),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = bnx2fc_fcoe_reset,
	.terminate_rport_io = fc_rport_terminate_io,
};

/**
 * scsi_host_template structure used while registering with SCSI-ml
 */
static struct scsi_host_template bnx2fc_shost_template = {
	.module			= THIS_MODULE,
	.name			= "QLogic Offload FCoE Initiator",
	.queuecommand		= bnx2fc_queuecommand,
	.eh_abort_handler	= bnx2fc_eh_abort,	  /* abts */
	.eh_device_reset_handler = bnx2fc_eh_device_reset,/* lun reset */
	.eh_bus_reset_handler    = bnx2fc_eh_bus_reset,   /* bus reset */
#ifndef __VMKLNX__
	.eh_target_reset_handler= bnx2fc_eh_target_reset, /* tgt reset */
	.proc_name = NULL,
#else
	.proc_name = "bnx2fc",
	.proc_info = bnx2fc_proc_info,
#endif
	.eh_host_reset_handler	= fc_eh_host_reset,	  /* lport reset */
	.slave_alloc		= fc_slave_alloc,
	.change_queue_depth	= fc_change_queue_depth,
	.change_queue_type	= fc_change_queue_type,
	.this_id		= -1,
	.cmd_per_lun		= 3,
	.can_queue		= (BNX2FC_MAX_OUTSTANDING_CMNDS/2),
	.use_clustering		= ENABLE_CLUSTERING,
	.sg_tablesize		= BNX2FC_MAX_BDS_PER_CMD,
	.max_sectors		= 512,
};

static struct libfc_function_template bnx2fc_libfc_fcn_templ = {
	.frame_send		= bnx2fc_xmit,
	.elsct_send		= bnx2fc_elsct_send,
	.fcp_abort_io		= bnx2fc_abort_io,
	.fcp_cleanup		= bnx2fc_cleanup,
	.rport_event_callback	= bnx2fc_rport_event_handler,
#ifdef __VMKLNX__
	.get_cna_netdev		= bnx2fc_get_fcoe_netdev,
	.get_lesb		= bnx2fc_get_lesb,
#endif
};

/**
 * bnx2fc_cnic_cb - global template of bnx2fc - cnic driver interface
 *			structure carrying callback function pointers
 */
static struct cnic_ulp_ops bnx2fc_cnic_cb = {
	.version		= CNIC_ULP_OPS_VER,
	.owner			= THIS_MODULE,
	.cnic_init		= bnx2fc_ulp_init,
	.cnic_exit		= bnx2fc_ulp_exit,
	.cnic_start		= bnx2fc_ulp_start,
	.cnic_stop		= bnx2fc_ulp_stop,
	.indicate_kcqes		= bnx2fc_indicate_kcqe,
	.indicate_netevent	= bnx2fc_indicate_netevent,
	.cnic_get_stats         = bnx2fc_ulp_get_stats,
};
