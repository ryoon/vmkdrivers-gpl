/* cnic.c: QLogic CNIC core network driver.
 *
 * Copyright (c) 2009-2014 QLogic Corporation
 *
 * Portions Copyright (c) VMware, Inc. 2009-2013, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Original skeleton written by: John(Zongxi) Chen (zongxi@broadcom.com)
 * Modified and maintained by: Michael Chan <mchan@broadcom.com>
 */

#include <linux/version.h>
#if (LINUX_VERSION_CODE < 0x020612)
#include <linux/config.h>
#endif

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/dma-mapping.h>
#include <asm/byteorder.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define BCM_VLAN 1
#endif
#include <net/ip.h>
#include <net/tcp.h>
#if !defined (__VMKLNX__)
#include <net/arp.h>
#include <net/neighbour.h>
#include <net/route.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/ip6_checksum.h>
#include <net/addrconf.h>
#ifdef HAVE_NETEVENT
#include <net/netevent.h>
#endif
#include <scsi/iscsi_proto.h>
#endif /* !defined (__VMKLNX__) */

#define NEW_BNX2X_HSI 70

#include "cnic_if.h"
#include "bnx2x.h"
#include "bnx2_compat0.h"
#include "bnx2.h"
#include "bnx2x_reg.h"
#include "bnx2x_fw_defs.h"
#include "bnx2x_hsi.h"
#include "57xx_iscsi_constants.h"
#include "57xx_iscsi_hsi.h"
#include "bnx2fc_constants.h"
#include "cnic.h"
#if (NEW_BNX2X_HSI >= 60)
#include "bnx2x_57710_int_offsets.h"
#include "bnx2x_57711_int_offsets.h"
#include "bnx2x_57712_int_offsets.h"
#endif
#include "cnic_defs.h"

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000)
#include "cnic_register.h"
#endif /* defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000) */

#define CNIC_MODULE_NAME		"cnic"
#define PFX CNIC_MODULE_NAME	": "

static char version[] __devinitdata =
	"QLogic NetXtreme II CNIC Driver " CNIC_MODULE_NAME " v" CNIC_MODULE_VERSION " (" CNIC_MODULE_RELDATE ")\n";

MODULE_AUTHOR("Michael Chan <mchan@broadcom.com> and John(Zongxi) "
	      "Chen (zongxi@broadcom.com");
MODULE_DESCRIPTION("QLogic NetXtreme II CNIC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(CNIC_MODULE_VERSION);

unsigned long cnic_dump_kwqe_en = 0;
module_param(cnic_dump_kwqe_en, long, 0644);
MODULE_PARM_DESC(cnic_dump_kwqe_en, "parameter to enable/disable kwqe dump\n");

static struct list_head cnic_dev_list;
#if !defined (__VMKLNX__)
static DEFINE_RWLOCK(cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
static DEFINE_MUTEX(cnic_lock);

static int cnic_alloc_kcq(struct cnic_dev *dev, struct kcq_info *info,
                          int use_pg_tbl);

static struct cnic_ulp_ops *cnic_ulp_tbl[MAX_CNIC_ULP_TYPE];

/* helper function, assuming cnic_lock is held */
static inline struct cnic_ulp_ops *cnic_ulp_tbl_prot(int type)
{
	return rcu_dereference_protected(cnic_ulp_tbl[type],
					 lockdep_is_held(&cnic_lock));
}

static int cnic_service_bnx2(void *, void *);
static int cnic_service_bnx2x(void *, void *);
static int cnic_ctl(void *, struct cnic_ctl_info *);

#if (CNIC_ISCSI_OOO_SUPPORT)
static void cnic_alloc_bnx2_ooo_resc(struct cnic_dev *dev);
static void cnic_alloc_bnx2x_ooo_resc(struct cnic_dev *dev);
static void cnic_start_bnx2_ooo_hw(struct cnic_dev *dev);
static void cnic_start_bnx2x_ooo_hw(struct cnic_dev *dev);
static void cnic_stop_bnx2_ooo_hw(struct cnic_dev *dev);
static void cnic_stop_bnx2x_ooo_hw(struct cnic_dev *dev);
static void cnic_handle_bnx2_ooo_rx_event(struct cnic_dev *dev);
static void cnic_handle_bnx2x_ooo_rx_event(struct cnic_dev *dev);
static void cnic_handle_bnx2_ooo_tx_event(struct cnic_dev *dev);
static void cnic_handle_bnx2x_ooo_tx_event(struct cnic_dev *dev);
static void cnic_bnx2_ooo_iscsi_conn_update(struct cnic_dev *dev,
					    struct kwqe *kwqe);
static void cnic_bnx2_ooo_iscsi_destroy(struct cnic_dev *dev,
					struct kwqe *kwqe);
static void cnic_bnx2x_ooo_iscsi_conn_update(struct cnic_dev *dev,
					     struct kwqe *kwqe);
static void cnic_free_ooo_resc(struct cnic_dev *dev);
static void cnic_conn_ooo_init(struct cnic_local *cp, u32 l5_cid);
static void cnic_flush_ooo(struct cnic_dev *dev, u32 l5_cid);
#endif

static struct cnic_ops cnic_bnx2_ops = {
	.cnic_owner	= THIS_MODULE,
	.cnic_handler	= cnic_service_bnx2,
	.cnic_ctl	= cnic_ctl,
};

static struct cnic_ops cnic_bnx2x_ops = {
	.cnic_owner	= THIS_MODULE,
	.cnic_handler	= cnic_service_bnx2x,
	.cnic_ctl	= cnic_ctl,
};

static struct workqueue_struct *cnic_wq;

static inline void cnic_hold(struct cnic_dev *dev)
{
	atomic_inc(&dev->ref_count);
}

static inline void cnic_put(struct cnic_dev *dev)
{
	atomic_dec(&dev->ref_count);
}

static inline void csk_hold(struct cnic_sock *csk)
{
	atomic_inc(&csk->ref_count);
}

static inline void csk_put(struct cnic_sock *csk)
{
	atomic_dec(&csk->ref_count);
}

static void cnic_print_ramrod_info(struct cnic_dev *dev, u32 cmd, u32 cid,
				   struct kwqe_16 *kwqe)
{
	char kwq_op[16];

	if (!cnic_dump_kwqe_en)
		return;

	switch(cmd) {
	/* Common RAMROD's */
	case RAMROD_CMD_ID_ETH_CLIENT_SETUP:
		sprintf(kwq_op, "%s", "CLIENT_SETUP");
		break;
#if (NEW_BNX2X_HSI < 60)
	case RAMROD_CMD_ID_ETH_CFC_DEL:
		sprintf(kwq_op, "%s", "ETH_CFC_DEL");
		break;
#endif
	case RAMROD_CMD_ID_COMMON_CFC_DEL:
		sprintf(kwq_op, "%s", "CFC_DEL");
		break;
	/* iSCSI RAMROD's */
	case L5CM_RAMROD_CMD_ID_TCP_CONNECT:
		sprintf(kwq_op, "%s", "TCP_CONNECT");
		break;
	case ISCSI_RAMROD_CMD_ID_UPDATE_CONN:
		sprintf(kwq_op, "%s", "UPDATE_CONN");
		break;
	case L5CM_RAMROD_CMD_ID_CLOSE:
		sprintf(kwq_op, "%s", "TCP_CLOSE");
		break;
	case L5CM_RAMROD_CMD_ID_ABORT:
		sprintf(kwq_op, "%s", "TCP_ABORT");
		break;
	/* FCoE RAMROD's */
#if (NEW_BNX2X_HSI >= 64)
	case FCOE_RAMROD_CMD_ID_INIT_FUNC:
#else
	case FCOE_RAMROD_CMD_ID_INIT:
#endif
		sprintf(kwq_op, "%s", "FCOE_INIT");
		break;
	case FCOE_RAMROD_CMD_ID_OFFLOAD_CONN:
		sprintf(kwq_op, "%s", "FCOE_OFLD_CONN");
		break;
	case FCOE_RAMROD_CMD_ID_ENABLE_CONN:
		sprintf(kwq_op, "%s", "FCOE_EN_CONN");
		break;
	case FCOE_RAMROD_CMD_ID_DISABLE_CONN:
		sprintf(kwq_op, "%s", "FCOE_DIS_CONN");
		break;
	case FCOE_RAMROD_CMD_ID_TERMINATE_CONN:
		sprintf(kwq_op, "%s", "FCOE_TERM_CONN");
		break;
#if (NEW_BNX2X_HSI >= 64)
	case FCOE_RAMROD_CMD_ID_DESTROY_FUNC:
#else
	case FCOE_RAMROD_CMD_ID_DESTROY:
#endif
		sprintf(kwq_op, "%s", "FCOE_DESTROY");
		break;
	default:
		sprintf(kwq_op, "%s:%x", "Unknown", cmd);
	}
	printk(KERN_INFO PFX "%s: CID=0x%x, %s RAMROD :: 0x%x, 0x%x"
			     "0x%x, 0x%x\n", dev->netdev->name, cid, kwq_op,
			     kwqe->kwqe_info0, kwqe->kwqe_info1,
			     kwqe->kwqe_info2, kwqe->kwqe_info3);
}


static void cnic_dump_kcq_entry(struct kcqe *kcqe, int index, char *add_info)
{
	if (add_info)
		printk(KERN_INFO PFX "KCQE[%x] = 0x%0x, 0x%x, 0x%x, 0x%x, "
		       "0x%x, 0x%x, 0x%x, OP=0x%x ---> [%s]\n", index,
		       kcqe->kcqe_info0, kcqe->kcqe_info1, kcqe->kcqe_info2,
		       kcqe->kcqe_info3, kcqe->kcqe_info4, kcqe->kcqe_info5,
		       kcqe->kcqe_info6, kcqe->kcqe_op_flag, add_info);
	else
		printk(KERN_INFO PFX "KCQE[%x] = 0x%0x, 0x%x, 0x%x, 0x%x, "
		       "0x%x, 0x%x, 0x%x, OP=0x%x\n", index,
		       kcqe->kcqe_info0, kcqe->kcqe_info1, kcqe->kcqe_info2,
		       kcqe->kcqe_info3, kcqe->kcqe_info4, kcqe->kcqe_info5,
		       kcqe->kcqe_info6, kcqe->kcqe_op_flag);
}

/* This function will dump KCQEs pointed by page
 */
static void cnic_dump_kcq_page(struct cnic_dev *dev, int kcq_page,
			       u16 sw_prod, u16 hw_prod)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct kcq_info *info = &cp->kcq1;
	struct kcqe *kcqe;
	int i;

	kcqe = &info->kcq[kcq_page][0];

	for (i = 0; i < MAX_KCQE_CNT; i++, kcqe++) {
		if (KCQ_PG(sw_prod) == kcq_page && KCQ_IDX(sw_prod) == i)
			/* SW PROD INDEX */
			cnic_dump_kcq_entry(kcqe, i, "SW_PROD");
		else if (KCQ_PG(hw_prod) == kcq_page && KCQ_IDX(hw_prod) == i)
			/* HW PROD INDEX */
			cnic_dump_kcq_entry(kcqe, i, "HW_PROD");
		else
			cnic_dump_kcq_entry(kcqe, i, NULL);
	}
}

/* This function will print 3 pages worth of KCQEs,
 * currently active, previous and next page.
 */
void cnic_dump_kcq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct kcq_info *info = &cp->kcq1;
	int current_page;
	int prev_page;
	int next_page;
	u16 sw_prod = 0;
	u16 hw_prod;

	sw_prod = info->sw_prod_idx;
	sw_prod &= MAX_KCQ_IDX;
	hw_prod = *info->hw_prod_idx_ptr;
	hw_prod = info->hw_idx(hw_prod);

	printk(KERN_INFO PFX "%s: sw_prod_idx = 0x%x, hw_prod_idx = 0x%x\n",
	       dev->netdev->name, info->sw_prod_idx, hw_prod);

	current_page = KCQ_PG(sw_prod);
	if (current_page == 0)
		prev_page = KCQ_PAGE_CNT - 1;
	else
		prev_page = current_page - 1;

	if (current_page == KCQ_PAGE_CNT - 1)
		next_page = 0;
	else
		next_page = current_page + 1;

	printk(KERN_INFO PFX "%s: dumping PREVIOUS PAGE (%d)\n",
	       dev->netdev->name, prev_page);
	cnic_dump_kcq_page(dev, prev_page, sw_prod, hw_prod);

	printk(KERN_INFO PFX "%s: dumping CURRENT PAGE (%d)\n",
	       dev->netdev->name, current_page);
	cnic_dump_kcq_page(dev, current_page, sw_prod, hw_prod);

	printk(KERN_INFO PFX "%s: dumping NEXT PAGE (%d)\n",
	       dev->netdev->name, next_page);
	cnic_dump_kcq_page(dev, next_page, sw_prod, hw_prod);
}
EXPORT_SYMBOL(cnic_dump_kcq);

#if !defined(__VMKLNX__) || \
    (defined(__VMKLNX__) && \
     ((VMWARE_ESX_DDK_VERSION >= 50000) && !defined (CNIC_INBOX)) || \
     ((VMWARE_ESX_DDK_VERSION >= 55000) && defined (CNIC_INBOX)))
static struct cnic_dev *cnic_from_netdev(struct net_device *netdev)
{
	struct cnic_dev *cdev;

#if !defined(__VMKLNX__)
	read_lock(&cnic_dev_lock);
#endif
	list_for_each_entry(cdev, &cnic_dev_list, list) {
		if (netdev == cdev->netdev) {
			cnic_hold(cdev);
#if !defined(__VMKLNX__)
			read_unlock(&cnic_dev_lock);
#endif /* !defined(__VMKLNX__) */
			return cdev;
		}
	}
#if !defined(__VMKLNX__)
	read_unlock(&cnic_dev_lock);
#endif
	return NULL;
}
#endif

static inline void ulp_get(struct cnic_ulp_ops *ulp_ops)
{
	atomic_inc(&ulp_ops->ref_count);
}

static inline void ulp_put(struct cnic_ulp_ops *ulp_ops)
{
	atomic_dec(&ulp_ops->ref_count);
}

static void cnic_ctx_wr(struct cnic_dev *dev, u32 cid_addr, u32 off, u32 val)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_io *io = &info.data.io;

	info.cmd = DRV_CTL_CTX_WR_CMD;
	io->cid_addr = cid_addr;
	io->offset = off;
	io->data = val;
	ethdev->drv_ctl(dev->netdev, &info);
}

static void cnic_ctx_tbl_wr(struct cnic_dev *dev, u32 off, dma_addr_t addr)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_io *io = &info.data.io;

	info.cmd = DRV_CTL_CTXTBL_WR_CMD;
	io->offset = off;
	io->dma_addr = addr;
	ethdev->drv_ctl(dev->netdev, &info);
}

static void cnic_npar_ring_ctl(struct cnic_dev *dev, int start)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;

	if (start)
		info.cmd = DRV_CTL_START_NPAR_CMD;
	else
		info.cmd = DRV_CTL_STOP_NPAR_CMD;

	ethdev->drv_ctl(dev->netdev, &info);
}

static void cnic_reg_wr_ind(struct cnic_dev *dev, u32 off, u32 val)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_io *io = &info.data.io;

	info.cmd = DRV_CTL_IO_WR_CMD;
	io->offset = off;
	io->data = val;
	ethdev->drv_ctl(dev->netdev, &info);
}

static u32 cnic_reg_rd_ind(struct cnic_dev *dev, u32 off)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_io *io = &info.data.io;

	info.cmd = DRV_CTL_IO_RD_CMD;
	io->offset = off;
	ethdev->drv_ctl(dev->netdev, &info);
	return io->data;
}

static void cnic_ulp_ctl(struct cnic_dev *dev, int ulp_type, bool reg)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct fcoe_capabilities *fcoe_cap =
		&info.data.register_data.fcoe_features;

	if (reg) {		
		info.cmd = DRV_CTL_ULP_REGISTER_CMD;
		if (ulp_type == CNIC_ULP_FCOE && dev->fcoe_cap)
			memcpy(fcoe_cap, dev->fcoe_cap, sizeof(*fcoe_cap));
	} else
		info.cmd = DRV_CTL_ULP_UNREGISTER_CMD;

	info.data.ulp_type = ulp_type;
	ethdev->drv_ctl(dev->netdev, &info);
}

static int cnic_in_use(struct cnic_sock *csk)
{
	return test_bit(SK_F_INUSE, &csk->flags);
}

static void cnic_spq_completion(struct cnic_dev *dev, int cmd, u32 count)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;

	info.cmd = cmd;
	info.data.credit.credit_count = count;
	ethdev->drv_ctl(dev->netdev, &info);
}

#if (CNIC_ISCSI_OOO_SUPPORT)
static int cnic_get_ooo_cqe(struct cnic_dev *dev, struct cnic_ooo_cqe* cqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_ooo_cqe *ooo_cqe = &info.data.ooo_cqe;

	info.cmd = DRV_CTL_GET_OOO_CQE;
	ooo_cqe->cqe = cqe;
	return ethdev->drv_ctl(dev->netdev, &info);
}

static int cnic_send_ooo_pkt(struct sk_buff *skb, struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_ooo_pkt *ooo_pd = &info.data.pkt_desc;

	info.cmd = DRV_CTL_SEND_OOO_PKT;
	ooo_pd->skb = skb;

	return ethdev->drv_ctl(dev->netdev, &info);
}

static int cnic_reuse_ooo_pkt(struct sk_buff *skb, struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;
	struct drv_ctl_ooo_pkt *ooo_pd = &info.data.pkt_desc;

	if (!skb) {
		CNIC_ERR("skb is NULL in reuse!\n");
		return -EINVAL;
	}
	info.cmd = DRV_CTL_REUSE_OOO_PKT;
	ooo_pd->skb = skb;
	return ethdev->drv_ctl(dev->netdev, &info);
}

static int cnic_comp_ooo_tx_pkts(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct drv_ctl_info info;

	info.cmd = DRV_CTL_COMP_OOO_TX_PKTS;
	return ethdev->drv_ctl(dev->netdev, &info);
}
#endif

static int cnic_get_l5_cid(struct cnic_local *cp, u32 cid, u32 *l5_cid)
{
	u32 i;

	if (!cp->ctx_tbl)
		return -EINVAL;

	for (i = 0; i < cp->max_cid_space; i++) {
		if (cp->ctx_tbl[i].cid == cid) {
			*l5_cid = i;
			return 0;
		}
	}
	return -EINVAL;
}

static int cnic_offld_prep(struct cnic_sock *csk)
{
	if (test_and_set_bit(SK_F_OFFLD_SCHED, &csk->flags))
		return 0;

	if (!test_bit(SK_F_CONNECT_START, &csk->flags)) {
		clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
		return 0;
	}

	return 1;
}

static int cnic_close_prep(struct cnic_sock *csk)
{
	clear_bit(SK_F_CONNECT_START, &csk->flags);
	smp_mb__after_clear_bit();

	if (test_and_clear_bit(SK_F_OFFLD_COMPLETE, &csk->flags)) {
		while (test_and_set_bit(SK_F_OFFLD_SCHED, &csk->flags))
			msleep(1);

		return 1;
	}

	return 0;
}

static int cnic_abort_prep(struct cnic_sock *csk)
{
	int i = 0;

	clear_bit(SK_F_CONNECT_START, &csk->flags);
	smp_mb__after_clear_bit();

	while (test_and_set_bit(SK_F_OFFLD_SCHED, &csk->flags)) {
		if (++i > 10000) {
			printk(KERN_INFO PFX "%s: cnic_abort_prep stuck on CID %x, aborting\n",
					 csk->dev->netdev->name, csk->cid);
			break;
		}
		msleep(1);
	}

	if (test_and_clear_bit(SK_F_OFFLD_COMPLETE, &csk->flags)) {
		csk->state = L4_KCQE_OPCODE_VALUE_RESET_COMP;
		return 1;
	}

	return 0;
}

int cnic_register_driver(int ulp_type, struct cnic_ulp_ops *ulp_ops)
{
	struct cnic_dev *dev;

	if (ulp_ops->version != CNIC_ULP_OPS_VER) {
		printk(KERN_WARNING PFX "ulp %x not compatible with cnic, "
					"expecting: 0x%x got: 0x%x\n", ulp_type,
					CNIC_ULP_OPS_VER, ulp_ops->version);
		return -EINVAL;
	}

	if (ulp_type < 0 || ulp_type >= MAX_CNIC_ULP_TYPE) {
		printk(KERN_ERR "Bad type %d\n", ulp_type);
		return -EINVAL;
	}
	mutex_lock(&cnic_lock);
	if (cnic_ulp_tbl[ulp_type]) {
		printk(KERN_ERR "Type %d has already been registered\n",
		       ulp_type);
		mutex_unlock(&cnic_lock);
		return -EBUSY;
	}

#if !defined (__VMKLNX__)
	read_lock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
	list_for_each_entry(dev, &cnic_dev_list, list) {
		struct cnic_local *cp = dev->cnic_priv;

		clear_bit(ULP_F_INIT, &cp->ulp_flags[ulp_type]);
	}
#if !defined (__VMKLNX__)
	read_unlock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */

	atomic_set(&ulp_ops->ref_count, 0);
	rcu_assign_pointer(cnic_ulp_tbl[ulp_type], ulp_ops);
	mutex_unlock(&cnic_lock);

	/* Prevent race conditions with netdev_event */
#if !defined (__VMKLNX__)
	rtnl_lock();
	read_lock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
	list_for_each_entry(dev, &cnic_dev_list, list) {
		struct cnic_local *cp = dev->cnic_priv;

		if (!test_and_set_bit(ULP_F_INIT, &cp->ulp_flags[ulp_type]))
			ulp_ops->cnic_init(dev);
	}
#if !defined (__VMKLNX__)
	read_unlock(&cnic_dev_lock);
	rtnl_unlock();
#endif /* !defined (__VMKLNX__) */

	return 0;
}

int cnic_unregister_driver(int ulp_type)
{
	struct cnic_dev *dev;
	struct cnic_ulp_ops *ulp_ops;
	int i = 0;

	if (ulp_type < 0 || ulp_type >= MAX_CNIC_ULP_TYPE) {
		printk(KERN_ERR "Bad type %d\n", ulp_type);
		return -EINVAL;
	}
	mutex_lock(&cnic_lock);
	ulp_ops = cnic_ulp_tbl[ulp_type];
	if (!ulp_ops) {
		printk(KERN_ERR "Type %d has not been registered\n", ulp_type);
		goto out_unlock;
	}
#if !defined (__VMKLNX__)
	read_lock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
	list_for_each_entry(dev, &cnic_dev_list, list) {
		struct cnic_local *cp = dev->cnic_priv;
		
		if (rcu_dereference(cp->ulp_ops[ulp_type])) {
			CNIC_ERR("Type %d still has devices registered\n",
				 ulp_type);
#if !defined (__VMKLNX__)
			read_unlock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
			goto out_unlock;
		}
	}
#if !defined (__VMKLNX__)
	read_unlock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */

	rcu_assign_pointer(cnic_ulp_tbl[ulp_type], NULL);

	mutex_unlock(&cnic_lock);
	synchronize_rcu();
	while ((atomic_read(&ulp_ops->ref_count) != 0) && (i < 20)) {
		msleep(100);
		i++;
	}

	if (atomic_read(&ulp_ops->ref_count) != 0)
		printk(KERN_WARNING PFX "%s: Failed waiting for ref count to go"
			" to zero.\n", dev->netdev->name);
	return 0;

out_unlock:
	mutex_unlock(&cnic_lock);
	return -EINVAL;
}

EXPORT_SYMBOL(cnic_register_driver);
EXPORT_SYMBOL(cnic_unregister_driver);

static int cnic_register_netdev(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int err;

	if (!ethdev)
		return -ENODEV;

	if (ethdev->drv_state & CNIC_DRV_STATE_REGD)
		return 0;

	err = ethdev->drv_register_cnic(dev->netdev, cp->cnic_ops, dev);
	if (err)
		CNIC_ERR("register_cnic failed\n");

	dev->max_iscsi_conn = ethdev->max_iscsi_conn;
	if (ethdev->drv_state & CNIC_DRV_STATE_NO_ISCSI)
		dev->max_iscsi_conn = 0;

	return err;
}

static void cnic_unregister_netdev(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;

#if defined (__VMKLNX__) && (VMWARE_ESX_DDK_VERSION < 50000)
	VMK_ReturnStatus rc = vmk_ModuleDecUseCount(dev->netdev->module_id);
	if (rc != VMK_OK) {
		printk(KERN_WARNING PFX "Unable to dec ref count for bnx2x: rc=%d\n", rc);
		return;
	}
#endif

	if (!ethdev)
		return;

	if (ethdev->drv_state & CNIC_DRV_STATE_REGD)
		ethdev->drv_unregister_cnic(dev->netdev);
}

static int cnic_start_hw(struct cnic_dev *);
static void cnic_stop_hw(struct cnic_dev *);

static int cnic_register_device(struct cnic_dev *dev, int ulp_type,
				void *ulp_ctx)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_ulp_ops *ulp_ops;

	if (ulp_type < 0 || ulp_type >= MAX_CNIC_ULP_TYPE) {
		CNIC_ERR("Bad type %d\n", ulp_type);
		return -EINVAL;
	}
	mutex_lock(&cnic_lock);
	if (cnic_ulp_tbl[ulp_type] == NULL) {
		CNIC_ERR("Driver with type %d has not been registered\n",
			 ulp_type);
		mutex_unlock(&cnic_lock);
		return -EAGAIN;
	}
	if (rcu_dereference(cp->ulp_ops[ulp_type])) {
		CNIC_ERR("Type %d has already been registered to this device\n",
			 ulp_type);
		mutex_unlock(&cnic_lock);
		return -EBUSY;
	}

	clear_bit(ULP_F_START, &cp->ulp_flags[ulp_type]);
	cp->ulp_handle[ulp_type] = ulp_ctx;
	ulp_ops = cnic_ulp_tbl[ulp_type];
	rcu_assign_pointer(cp->ulp_ops[ulp_type], ulp_ops);
	cnic_hold(dev);
	if (!dev->use_count) {
		if (!test_bit(CNIC_F_IF_GOING_DOWN, &dev->flags)) {
			if (dev->netdev->flags & IFF_UP)
				set_bit(CNIC_F_IF_UP, &dev->flags);
		}
	}
	dev->use_count++;

	if (test_bit(CNIC_F_CNIC_UP, &dev->flags))
		if (!test_and_set_bit(ULP_F_START, &cp->ulp_flags[ulp_type]))
			ulp_ops->cnic_start(cp->ulp_handle[ulp_type]);

	mutex_unlock(&cnic_lock);

	cnic_ulp_ctl(dev, ulp_type, true);

	return 0;

}

static int cnic_unregister_device(struct cnic_dev *dev, int ulp_type)
{
	struct cnic_local *cp = dev->cnic_priv;
	int i = 0;

	if (ulp_type < 0 || ulp_type >= MAX_CNIC_ULP_TYPE) {
		CNIC_ERR("Bad type %d\n", ulp_type);
		return -EINVAL;
	}
	mutex_lock(&cnic_lock);
	if (rcu_dereference(cp->ulp_ops[ulp_type])) {
		dev->use_count--;
		rcu_assign_pointer(cp->ulp_ops[ulp_type], NULL);
		cnic_put(dev);
	} else {
		CNIC_ERR("device not registered to this ulp type %d\n",
			 ulp_type);
		mutex_unlock(&cnic_lock);
		return -EINVAL;
	}
	mutex_unlock(&cnic_lock);

	if (ulp_type == CNIC_ULP_FCOE)
		dev->fcoe_cap = NULL;

	synchronize_rcu();

	while (test_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[ulp_type]) &&
	       i < 20) {
		msleep(100);
		i++;
	}
	if (test_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[ulp_type]))
		printk(KERN_WARNING PFX "%s: Failed waiting for ULP up call"
					" to complete.\n", dev->netdev->name);

	cnic_ulp_ctl(dev, ulp_type, false);

	return 0;
}

static int cnic_init_id_tbl(struct cnic_id_tbl *id_tbl, u32 size, u32 start_id,
			    u32 next)
{
	id_tbl->start = start_id;
	id_tbl->max = size;
	id_tbl->next = next;
	spin_lock_init(&id_tbl->lock);
	id_tbl->table = kzalloc(DIV_ROUND_UP(size, 32) * 4, GFP_KERNEL);
	if (!id_tbl->table)
		return -ENOMEM;

	return 0;
}

static u32 cnic_free_id_tbl(struct cnic_id_tbl *id_tbl)
{
	kfree(id_tbl->table);
	id_tbl->table = NULL;
	return id_tbl->next;
}

/* Returns -1 if not successful */
static u32 cnic_alloc_new_id(struct cnic_id_tbl *id_tbl)
{
	u32 id;

	spin_lock(&id_tbl->lock);
	id = find_next_zero_bit(id_tbl->table, id_tbl->max, id_tbl->next);
	if (id >= id_tbl->max) {
		id = -1;
		if (id_tbl->next != 0) {
			id = find_first_zero_bit(id_tbl->table, id_tbl->next);
			if (id >= id_tbl->next)
				id = -1;
		}
	}

	if (id < id_tbl->max) {
		set_bit(id, id_tbl->table);
		id_tbl->next = (id + 1) & (id_tbl->max - 1);
		id += id_tbl->start;
	}

	spin_unlock(&id_tbl->lock);

	return id;
}

void cnic_free_id(struct cnic_id_tbl *id_tbl, u32 id)
{
	if (id == -1)
		return;

	id -= id_tbl->start;
	if (id >= id_tbl->max)
		return;

	clear_bit(id, id_tbl->table);
}

static void cnic_free_dma(struct cnic_dev *dev, struct cnic_dma *dma)
{
	int i;

	if (!dma->pg_arr)
		return;

	for (i = 0; i < dma->num_pages; i++) {
		if (dma->pg_arr[i]) {
			pci_free_consistent(dev->pcidev, BNX2_PAGE_SIZE,
					    dma->pg_arr[i], dma->pg_map_arr[i]);
			dma->pg_arr[i] = NULL;
		}
	}
	if (dma->pgtbl) {
		pci_free_consistent(dev->pcidev, dma->pgtbl_size,
				    dma->pgtbl, dma->pgtbl_map);
		dma->pgtbl = NULL;
	}
	kfree(dma->pg_arr);
	dma->pg_arr = NULL;
	dma->num_pages = 0;
}

static void cnic_setup_page_tbl(struct cnic_dev *dev, struct cnic_dma *dma)
{
	int i;
	__le32 *page_table = (__le32 *) dma->pgtbl;

	for (i = 0; i < dma->num_pages; i++) {
		/* Each entry needs to be in big endian format. */
		*page_table = cpu_to_le32((u64) dma->pg_map_arr[i] >> 32);
		page_table++;
		*page_table = cpu_to_le32(dma->pg_map_arr[i] & 0xffffffff);
		page_table++;
	}
}

static void cnic_setup_page_tbl_le(struct cnic_dev *dev, struct cnic_dma *dma)
{
	int i;
	__le32 *page_table = (__le32 *) dma->pgtbl;

	for (i = 0; i < dma->num_pages; i++) {
		/* Each entry needs to be in little endian format. */
		*page_table = cpu_to_le32(dma->pg_map_arr[i] & 0xffffffff);
		page_table++;
		*page_table = cpu_to_le32((u64) dma->pg_map_arr[i] >> 32);
		page_table++;
	}
}

static int cnic_alloc_dma(struct cnic_dev *dev, struct cnic_dma *dma,
			  int pages, int use_pg_tbl)
{
	int i, size;
	struct cnic_local *cp = dev->cnic_priv;

	size = pages * (sizeof(void *) + sizeof(dma_addr_t));
	dma->pg_arr = kzalloc(size, GFP_ATOMIC);
	if (dma->pg_arr == NULL)
		return -ENOMEM;

	dma->pg_map_arr = (dma_addr_t *) (dma->pg_arr + pages);
	dma->num_pages = pages;

	for (i = 0; i < pages; i++) {
		dma->pg_arr[i] = pci_alloc_consistent(dev->pcidev,
						      BNX2_PAGE_SIZE,
						      &dma->pg_map_arr[i]);
		if (dma->pg_arr[i] == NULL)
			goto error;
	}
	if (!use_pg_tbl)
		return 0;

	dma->pgtbl_size = ((pages * 8) + BNX2_PAGE_SIZE - 1) &
			  ~(BNX2_PAGE_SIZE - 1);
	dma->pgtbl = pci_alloc_consistent(dev->pcidev, dma->pgtbl_size,
					  &dma->pgtbl_map);
	if (dma->pgtbl == NULL)
		goto error;

	cp->setup_pgtbl(dev, dma);

	return 0;

error:
	cnic_free_dma(dev, dma);
	return -ENOMEM;
}

static void cnic_free_context(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int i;

	for (i = 0; i < cp->ctx_blks; i++) {
		if (cp->ctx_arr[i].ctx) {
			pci_free_consistent(dev->pcidev, cp->ctx_blk_size,
					    cp->ctx_arr[i].ctx,
					    cp->ctx_arr[i].mapping);
			cp->ctx_arr[i].ctx = NULL;
		}
	}
}

static void cnic_free_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_free_ooo_resc(dev);
#endif
	cnic_free_context(dev);
	kfree(cp->ctx_arr);
	cp->ctx_arr = NULL;
	cp->ctx_blks = 0;

	cnic_free_dma(dev, &cp->gbl_buf_info);
#if (NEW_BNX2X_HSI <= 60)
	cnic_free_dma(dev, &cp->conn_buf_info);
#endif
	cnic_free_dma(dev, &cp->kwq_info);
	cnic_free_dma(dev, &cp->kwq_16_data_info);
	cnic_free_dma(dev, &cp->kcq2.dma);
	cnic_free_dma(dev, &cp->kcq1.dma);
	kfree(cp->iscsi_tbl);
	cp->iscsi_tbl = NULL;
	kfree(cp->ctx_tbl);
	cp->ctx_tbl = NULL;

	cnic_free_id_tbl(&cp->fcoe_cid_tbl);
	cnic_free_id_tbl(&cp->cid_tbl);
}

static int cnic_alloc_context(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	if (BNX2_CHIP(cp) == BNX2_CHIP_5709) {
		int i, k, arr_size;

		cp->ctx_blk_size = BNX2_PAGE_SIZE;
		cp->cids_per_blk = BNX2_PAGE_SIZE / 128;
		arr_size = BNX2_MAX_CID / cp->cids_per_blk *
			   sizeof(struct cnic_ctx);
		cp->ctx_arr = kzalloc(arr_size, GFP_KERNEL);
		if (cp->ctx_arr == NULL)
			return -ENOMEM;

		k = 0;
		for (i = 0; i < 2; i++) {
			u32 j, reg, off, lo, hi;

			if (i == 0)
				off = BNX2_PG_CTX_MAP;
			else
				off = BNX2_ISCSI_CTX_MAP;

			reg = cnic_reg_rd_ind(dev, off);
			lo = reg >> 16;
			hi = reg & 0xffff;
			for (j = lo; j < hi; j += cp->cids_per_blk, k++)
				cp->ctx_arr[k].cid = j;
		}

		cp->ctx_blks = k;
		if (cp->ctx_blks >= (BNX2_MAX_CID / cp->cids_per_blk)) {
			cp->ctx_blks = 0;
			return -ENOMEM;
		}

		for (i = 0; i < cp->ctx_blks; i++) {
			cp->ctx_arr[i].ctx =
				pci_alloc_consistent(dev->pcidev, BNX2_PAGE_SIZE,
						     &cp->ctx_arr[i].mapping);
			if (cp->ctx_arr[i].ctx == NULL)
				return -ENOMEM;
		}
	}
	return 0;
}

static u16 cnic_bnx2_next_idx(u16 idx)
{
	return idx + 1;
}

static u16 cnic_bnx2_hw_idx(u16 idx)
{
	return idx;
}

static u16 cnic_bnx2x_next_idx(u16 idx)
{
	idx++;
	if ((idx & MAX_KCQE_CNT) == MAX_KCQE_CNT)
		idx++;

	return idx;
}

static u16 cnic_bnx2x_hw_idx(u16 idx)
{
	if ((idx & MAX_KCQE_CNT) == MAX_KCQE_CNT)
		idx++;
	return idx;
}

static int cnic_alloc_kcq(struct cnic_dev *dev, struct kcq_info *info,
			  int use_pg_tbl)
{
	int err, i;
	struct kcqe **kcq;

	err = cnic_alloc_dma(dev, &info->dma, KCQ_PAGE_CNT, use_pg_tbl);
	if (err)
		return err;

	kcq = (struct kcqe **) info->dma.pg_arr;
	info->kcq = kcq;

	info->next_idx = cnic_bnx2_next_idx;
	info->hw_idx = cnic_bnx2_hw_idx;
	if (use_pg_tbl)
		return 0;

	info->next_idx = cnic_bnx2x_next_idx;
	info->hw_idx = cnic_bnx2x_hw_idx;

	for (i = 0; i < KCQ_PAGE_CNT; i++) {
		struct bnx2x_bd_chain_next *next =
			(struct bnx2x_bd_chain_next *) &kcq[i][MAX_KCQE_CNT];
		int j = i + 1;

		if (j >= KCQ_PAGE_CNT)
			j = 0;
		next->addr_hi = (u64) info->dma.pg_map_arr[j] >> 32;
		next->addr_lo = info->dma.pg_map_arr[j] & 0xffffffff;
	}
	return 0;
}

static int cnic_alloc_bnx2_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int i, ret;

	cp->max_cid_space = MAX_ISCSI_TBL_SZ;

	cp->iscsi_tbl = kzalloc(sizeof(struct cnic_iscsi) * MAX_ISCSI_TBL_SZ,
				GFP_KERNEL);
	if (!cp->iscsi_tbl) {
		CNIC_ERR("failed to allocate iscsi_tbl\n");
		ret = -ENOMEM;
		goto error;
	}

	cp->ctx_tbl = kzalloc(sizeof(struct cnic_context) * cp->max_cid_space,
			      GFP_KERNEL);
	if (!cp->ctx_tbl) {
		CNIC_ERR("failed to allocate ctx_arr\n");
		ret = -ENOMEM;
		goto error;
	}

	for (i = 0; i < MAX_ISCSI_TBL_SZ; i++) {
		cp->ctx_tbl[i].proto.iscsi = &cp->iscsi_tbl[i];
		cp->ctx_tbl[i].ulp_proto_id = CNIC_ULP_ISCSI;
	}

	ret = cnic_alloc_dma(dev, &cp->kwq_info, KWQ_PAGE_CNT, 1);
	if (ret) {
		CNIC_ERR("failed to allocate kwq\n");
		goto error;
	}
	cp->kwq = (struct kwqe **) cp->kwq_info.pg_arr;

	ret = cnic_alloc_kcq(dev, &cp->kcq1, 1);
	if (ret)
		goto error;

	ret = cnic_alloc_context(dev);
	if (ret) {
		CNIC_ERR("failed to allocate kcq\n");
		goto error;
	}

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_alloc_bnx2_ooo_resc(dev);
#endif
	return 0;
	
error:
	cnic_free_resc(dev);
	return ret;
}

static int cnic_alloc_bnx2x_context(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	int ctx_blk_size = cp->ethdev->ctx_blk_size;
	int total_mem, blks, i;

	total_mem = BNX2X_CONTEXT_MEM_SIZE * cp->max_cid_space;
	blks = total_mem / ctx_blk_size;
	if (total_mem % ctx_blk_size)
		blks++;

	if (blks > cp->ethdev->ctx_tbl_len) {
		CNIC_ERR("blks(%d) > cp->ethdev->ctx_tbl_len(%d)\n",
			  blks, cp->ethdev->ctx_tbl_len);
		return -ENOMEM;
	}

	cp->ctx_arr = kcalloc(blks, sizeof(struct cnic_ctx), GFP_KERNEL);
	if (cp->ctx_arr == NULL) {
		CNIC_ERR("failed to allocate ctx_arr\n");
		return -ENOMEM;
	}

	cp->ctx_blks = blks;
	cp->ctx_blk_size = ctx_blk_size;
	if (!CHIP_IS_E1(bp))
		cp->ctx_align = 0;
	else
		cp->ctx_align = ctx_blk_size;

	cp->cids_per_blk = ctx_blk_size / BNX2X_CONTEXT_MEM_SIZE;

	for (i = 0; i < blks; i++) {
		cp->ctx_arr[i].ctx =
			pci_alloc_consistent(dev->pcidev, cp->ctx_blk_size,
					     &cp->ctx_arr[i].mapping);
		if (cp->ctx_arr[i].ctx == NULL)
			return -ENOMEM;

		if (cp->ctx_align && cp->ctx_blk_size == ctx_blk_size) {
			if (cp->ctx_arr[i].mapping & (cp->ctx_align - 1)) {
				cnic_free_context(dev);
				cp->ctx_blk_size += cp->ctx_align;
				i = -1;
				continue;
			}
		}
	}
	return 0;
}

static int cnic_alloc_bnx2x_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct cnic_eth_dev *ethdev = cp->ethdev;
	u32 start_cid = ethdev->starting_cid;
	int i, j, n, ret, pages;
	struct cnic_dma *kwq_16_dma = &cp->kwq_16_data_info;


#if (NEW_BNX2X_HSI >= 60)
	if (CHIP_IS_E1(bp))
		cp->iro_arr = e1_iro_arr;
	else if (CHIP_IS_E1H(bp))
		cp->iro_arr = e1h_iro_arr;
	else if (BNX2X_CHIP_IS_E2_PLUS(bp))
		cp->iro_arr = e2_iro_arr;
#endif

	cp->max_cid_space = MAX_ISCSI_TBL_SZ;
	cp->iscsi_start_cid = start_cid;
	cp->fcoe_start_cid = start_cid + MAX_ISCSI_TBL_SZ;

	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		cp->max_cid_space += dev->max_fcoe_conn;
		cp->fcoe_init_cid = ethdev->fcoe_init_cid;
		if (!cp->fcoe_init_cid)
			cp->fcoe_init_cid = 0x10;
	}

	if (start_cid < BNX2X_ISCSI_START_CID) {
		u32 delta = BNX2X_ISCSI_START_CID - start_cid;

		cp->iscsi_start_cid = BNX2X_ISCSI_START_CID;
		cp->fcoe_start_cid += delta;
		cp->max_cid_space += delta;
	}

	cp->iscsi_tbl = kzalloc(sizeof(struct cnic_iscsi) * MAX_ISCSI_TBL_SZ,
				GFP_KERNEL);
	if (!cp->iscsi_tbl) {
		CNIC_ERR("failed to allocate iscsi_tbl\n");
		goto error;
	}

	cp->ctx_tbl = kzalloc(sizeof(struct cnic_context) *
				cp->max_cid_space, GFP_KERNEL);
	if (!cp->ctx_tbl) {
		CNIC_ERR("failed to allocate ctx_tbl\n");
		goto error;
	}

	for (i = 0; i < MAX_ISCSI_TBL_SZ; i++) {
		cp->ctx_tbl[i].proto.iscsi = &cp->iscsi_tbl[i];
		cp->ctx_tbl[i].ulp_proto_id = CNIC_ULP_ISCSI;
	}

	for (i = MAX_ISCSI_TBL_SZ; i < cp->max_cid_space; i++)
		cp->ctx_tbl[i].ulp_proto_id = CNIC_ULP_FCOE;

	pages = PAGE_ALIGN(cp->max_cid_space * CNIC_KWQ16_DATA_SIZE) /
		PAGE_SIZE;

	ret = cnic_alloc_dma(dev, kwq_16_dma, pages, 0);
	if (ret) {
		CNIC_ERR("failed to allocate kwq\n");
		return -ENOMEM;
	}

	n = PAGE_SIZE / CNIC_KWQ16_DATA_SIZE;
	for (i = 0, j = 0; i < cp->max_cid_space; i++) {
		long off = CNIC_KWQ16_DATA_SIZE * (i % n);

		cp->ctx_tbl[i].kwqe_data = kwq_16_dma->pg_arr[j] + off;
		cp->ctx_tbl[i].kwqe_data_mapping = kwq_16_dma->pg_map_arr[j] +
						   off;

		if ((i % n) == (n - 1))
			j++;
	}

	ret = cnic_alloc_kcq(dev, &cp->kcq1, 0);

	if (ret) {
		CNIC_ERR("failed to allocate kcq\n");
		goto error;
	}

	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
#if (NEW_BNX2X_HSI >= 64)
		ret = cnic_alloc_kcq(dev, &cp->kcq2, 1);
#else
		ret = cnic_alloc_kcq(dev, &cp->kcq2, 0);
#endif

		if (ret)
			goto error;
	}

#if (NEW_BNX2X_HSI <= 60)
	pages = PAGE_ALIGN(BNX2X_ISCSI_NUM_CONNECTIONS *
			   BNX2X_ISCSI_CONN_BUF_SIZE) / PAGE_SIZE;
	ret = cnic_alloc_dma(dev, &cp->conn_buf_info, pages, 1);
	if (ret)
		goto error;
#endif

	pages = PAGE_ALIGN(BNX2X_ISCSI_GLB_BUF_SIZE) / PAGE_SIZE;
	ret = cnic_alloc_dma(dev, &cp->gbl_buf_info, pages, 0);
	if (ret) {
		CNIC_ERR("failed to allocate gbl_buf_info\n");
		goto error;
	}

	ret = cnic_alloc_bnx2x_context(dev);
	if (ret)
		goto error;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_alloc_bnx2x_ooo_resc(dev);
#endif
	return 0;

error:
	cnic_free_resc(dev);
	return -ENOMEM;
}

static inline u32 cnic_kwq_avail(struct cnic_local *cp)
{
	return cp->max_kwq_idx -
		((cp->kwq_prod_idx - cp->kwq_con_idx) & cp->max_kwq_idx);
}

static int cnic_submit_bnx2_kwqes(struct cnic_dev *dev, struct kwqe *wqes[],
				  u32 num_wqes)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct kwqe *prod_qe;
	u16 prod, sw_prod, i;
#if (CNIC_ISCSI_OOO_SUPPORT)
	u32 opcode;
	struct kwqe *kwqe;
#endif

	if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return -EAGAIN;		/* bnx2 is down */

	spin_lock_bh(&cp->cnic_ulp_lock);
	if (num_wqes > cnic_kwq_avail(cp) &&
	    !test_bit(CNIC_LCL_FL_KWQ_INIT, &cp->cnic_local_flags)) {
		spin_unlock_bh(&cp->cnic_ulp_lock);
		return -EAGAIN;
	}

	clear_bit(CNIC_LCL_FL_KWQ_INIT, &cp->cnic_local_flags);

	prod = cp->kwq_prod_idx;
	sw_prod = prod & MAX_KWQ_IDX;
	for (i = 0; i < num_wqes; i++) {
		prod_qe = &cp->kwq[KWQ_PG(sw_prod)][KWQ_IDX(sw_prod)];
#if (CNIC_ISCSI_OOO_SUPPORT)
		kwqe = wqes[i];
		opcode = KWQE_OPCODE(kwqe->kwqe_op_flag);
		switch (opcode) {
		case ISCSI_KWQE_OPCODE_UPDATE_CONN:
			cnic_bnx2_ooo_iscsi_conn_update(dev, kwqe);
			break;
		case ISCSI_KWQE_OPCODE_DESTROY_CONN:
			cnic_bnx2_ooo_iscsi_destroy(dev, kwqe);
			break;
		default:
			break;
		}
#endif
		memcpy(prod_qe, wqes[i], sizeof(struct kwqe));
		prod++;
		sw_prod = prod & MAX_KWQ_IDX;
	}
	cp->kwq_prod_idx = prod;

	CNIC_WR16(dev, cp->kwq_io_addr, cp->kwq_prod_idx);

	spin_unlock_bh(&cp->cnic_ulp_lock);
	return 0;
}

static void *cnic_get_kwqe_16_data(struct cnic_local *cp, u32 l5_cid,
				   union l5cm_specific_data *l5_data)
{
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	dma_addr_t map;

	map = ctx->kwqe_data_mapping;
	l5_data->phy_address.lo = (u64) map & 0xffffffff;
	l5_data->phy_address.hi = (u64) map >> 32;
	return ctx->kwqe_data;
}

static int cnic_submit_kwqe_16(struct cnic_dev *dev, u32 cmd, u32 cid,
				u32 type, union l5cm_specific_data *l5_data)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct l5cm_spe kwqe;
	struct kwqe_16 *kwq[1];
	u16 type_16;
	int ret;

	kwqe.hdr.conn_and_cmd_data =
		cpu_to_le32(((cmd << SPE_HDR_CMD_ID_SHIFT) |
			     BNX2X_HW_CID(bp, cid)));
#if (NEW_BNX2X_HSI >= 60)
	type_16 = (type << SPE_HDR_CONN_TYPE_SHIFT) & SPE_HDR_CONN_TYPE;
	type_16 |= (bp->pfid << SPE_HDR_FUNCTION_ID_SHIFT) &
		   SPE_HDR_FUNCTION_ID;
#else
	type_16 = type;
#endif

	kwqe.hdr.type = cpu_to_le16(type_16);
	kwqe.hdr.reserved1 = 0;
	kwqe.data.phy_address.lo = cpu_to_le32(l5_data->phy_address.lo);
	kwqe.data.phy_address.hi = cpu_to_le32(l5_data->phy_address.hi);

	kwq[0] = (struct kwqe_16 *) &kwqe;

	spin_lock_bh(&cp->cnic_ulp_lock);
	ret = cp->ethdev->drv_submit_kwqes_16(dev->netdev, kwq, 1);
	spin_unlock_bh(&cp->cnic_ulp_lock);

	cnic_print_ramrod_info(dev, cmd, cid, kwq[0]);

	if (ret == 1)
		return 0;

	return ret;
}

static void cnic_reply_bnx2x_kcqes(struct cnic_dev *dev, int ulp_type,
				   struct kcqe *cqes[], u32 num_cqes)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_ulp_ops *ulp_ops;

	rcu_read_lock();
	ulp_ops = rcu_dereference(cp->ulp_ops[ulp_type]);
	if (likely(ulp_ops)) {
		ulp_ops->indicate_kcqes(cp->ulp_handle[ulp_type],
					  cqes, num_cqes);
	}
	rcu_read_unlock();
}

static void cnic_bnx2x_set_tcp_options(struct cnic_dev *dev, int time_stamps,
					int en_tcp_dack)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u8 xstorm_flags = XSTORM_L5CM_TCP_FLAGS_WND_SCL_EN;
	u16 tstorm_flags = 0;

	if (time_stamps) {
		xstorm_flags |= XSTORM_L5CM_TCP_FLAGS_TS_ENABLED;
		tstorm_flags |= TSTORM_L5CM_TCP_FLAGS_TS_ENABLED;
	}
	if (en_tcp_dack)
		tstorm_flags |= TSTORM_L5CM_TCP_FLAGS_DELAYED_ACK_EN;

	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		XSTORM_ISCSI_TCP_VARS_FLAGS_OFFSET(bp->pfid), xstorm_flags);

	CNIC_WR16(dev, BAR_TSTRORM_INTMEM +
		TSTORM_ISCSI_TCP_VARS_FLAGS_OFFSET(bp->pfid), tstorm_flags);
}

static int cnic_bnx2x_iscsi_init1(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct iscsi_kwqe_init1 *req1 = (struct iscsi_kwqe_init1 *) kwqe;
	int hq_bds, pages;
	u32 pfid = bp->pfid;

	cp->num_iscsi_tasks = req1->num_tasks_per_conn;
	cp->num_ccells = req1->num_ccells_per_conn;
	cp->task_array_size = BNX2X_ISCSI_TASK_CONTEXT_SIZE *
			      cp->num_iscsi_tasks;
	cp->r2tq_size = cp->num_iscsi_tasks * BNX2X_ISCSI_MAX_PENDING_R2TS *
			BNX2X_ISCSI_R2TQE_SIZE;
	cp->hq_size = cp->num_ccells * BNX2X_ISCSI_HQ_BD_SIZE;
	pages = PAGE_ALIGN(cp->hq_size) / PAGE_SIZE;
	hq_bds = pages * (PAGE_SIZE / BNX2X_ISCSI_HQ_BD_SIZE);
	cp->num_cqs = req1->num_cqs;

	if (!dev->max_iscsi_conn)
		return 0;

	/* init Tstorm RAM */
	CNIC_WR16(dev, BAR_TSTRORM_INTMEM + TSTORM_ISCSI_RQ_SIZE_OFFSET(pfid),
		  req1->rq_num_wqes);
	CNIC_WR16(dev, BAR_TSTRORM_INTMEM + TSTORM_ISCSI_PAGE_SIZE_OFFSET(pfid),
		  PAGE_SIZE);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_PAGE_SIZE_LOG_OFFSET(pfid), PAGE_SHIFT);
	CNIC_WR16(dev, BAR_TSTRORM_INTMEM +
		  TSTORM_ISCSI_NUM_OF_TASKS_OFFSET(pfid),
		  req1->num_tasks_per_conn);

	/* init Ustorm RAM */
	CNIC_WR16(dev, BAR_USTRORM_INTMEM +
		  USTORM_ISCSI_RQ_BUFFER_SIZE_OFFSET(pfid),
		  req1->rq_buffer_size);
	CNIC_WR16(dev, BAR_USTRORM_INTMEM + USTORM_ISCSI_PAGE_SIZE_OFFSET(pfid),
		  PAGE_SIZE);
	CNIC_WR8(dev, BAR_USTRORM_INTMEM +
		 USTORM_ISCSI_PAGE_SIZE_LOG_OFFSET(pfid), PAGE_SHIFT);
	CNIC_WR16(dev, BAR_USTRORM_INTMEM +
		  USTORM_ISCSI_NUM_OF_TASKS_OFFSET(pfid),
		  req1->num_tasks_per_conn);
	CNIC_WR16(dev, BAR_USTRORM_INTMEM + USTORM_ISCSI_RQ_SIZE_OFFSET(pfid),
		  req1->rq_num_wqes);
	CNIC_WR16(dev, BAR_USTRORM_INTMEM + USTORM_ISCSI_CQ_SIZE_OFFSET(pfid),
		  req1->cq_num_wqes);
	CNIC_WR16(dev, BAR_USTRORM_INTMEM + USTORM_ISCSI_R2TQ_SIZE_OFFSET(pfid),
		  cp->num_iscsi_tasks * BNX2X_ISCSI_MAX_PENDING_R2TS);

	/* init Xstorm RAM */
	CNIC_WR16(dev, BAR_XSTRORM_INTMEM + XSTORM_ISCSI_PAGE_SIZE_OFFSET(pfid),
		  PAGE_SIZE);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_PAGE_SIZE_LOG_OFFSET(pfid), PAGE_SHIFT);
	CNIC_WR16(dev, BAR_XSTRORM_INTMEM +
		  XSTORM_ISCSI_NUM_OF_TASKS_OFFSET(pfid),
		  req1->num_tasks_per_conn);
	CNIC_WR16(dev, BAR_XSTRORM_INTMEM + XSTORM_ISCSI_HQ_SIZE_OFFSET(pfid),
		  hq_bds);
	CNIC_WR16(dev, BAR_XSTRORM_INTMEM + XSTORM_ISCSI_SQ_SIZE_OFFSET(pfid),
		  req1->num_tasks_per_conn);
	CNIC_WR16(dev, BAR_XSTRORM_INTMEM + XSTORM_ISCSI_R2TQ_SIZE_OFFSET(pfid),
		  cp->num_iscsi_tasks * BNX2X_ISCSI_MAX_PENDING_R2TS);

	/* init Cstorm RAM */
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM + CSTORM_ISCSI_PAGE_SIZE_OFFSET(pfid),
		  PAGE_SIZE);
	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		 CSTORM_ISCSI_PAGE_SIZE_LOG_OFFSET(pfid), PAGE_SHIFT);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_ISCSI_NUM_OF_TASKS_OFFSET(pfid),
		  req1->num_tasks_per_conn);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM + CSTORM_ISCSI_CQ_SIZE_OFFSET(pfid),
		  req1->cq_num_wqes);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM + CSTORM_ISCSI_HQ_SIZE_OFFSET(pfid),
		  hq_bds);

	cnic_bnx2x_set_tcp_options(dev,
		req1->flags & ISCSI_KWQE_INIT1_TIME_STAMPS_ENABLE,
		req1->flags & ISCSI_KWQE_INIT1_DELAYED_ACK_ENABLE);

	return 0;
}

static int cnic_bnx2x_iscsi_init2(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct iscsi_kwqe_init2 *req2 = (struct iscsi_kwqe_init2 *) kwqe;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 pfid = bp->pfid;
	struct iscsi_kcqe kcqe;
	struct kcqe *cqes[1];

	memset(&kcqe, 0, sizeof(kcqe));
	if (!dev->max_iscsi_conn) {
		kcqe.completion_status =
			ISCSI_KCQE_COMPLETION_STATUS_ISCSI_NOT_SUPPORTED;
		goto done;
	}

	CNIC_WR(dev, BAR_TSTRORM_INTMEM +
		TSTORM_ISCSI_ERROR_BITMAP_OFFSET(pfid), req2->error_bit_map[0]);
	CNIC_WR(dev, BAR_TSTRORM_INTMEM +
		TSTORM_ISCSI_ERROR_BITMAP_OFFSET(pfid) + 4,
		req2->error_bit_map[1]);

	CNIC_WR16(dev, BAR_USTRORM_INTMEM +
		  USTORM_ISCSI_CQ_SQN_SIZE_OFFSET(pfid), req2->max_cq_sqn);
	CNIC_WR(dev, BAR_USTRORM_INTMEM +
		USTORM_ISCSI_ERROR_BITMAP_OFFSET(pfid), req2->error_bit_map[0]);
	CNIC_WR(dev, BAR_USTRORM_INTMEM +
		USTORM_ISCSI_ERROR_BITMAP_OFFSET(pfid) + 4,
		req2->error_bit_map[1]);

	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_ISCSI_CQ_SQN_SIZE_OFFSET(pfid), req2->max_cq_sqn);

	kcqe.completion_status = ISCSI_KCQE_COMPLETION_STATUS_SUCCESS;

done:
	kcqe.op_code = ISCSI_KCQE_OPCODE_INIT;
	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_ISCSI, cqes, 1);

	return 0;
}

static void cnic_free_bnx2x_conn_resc(struct cnic_dev *dev, u32 l5_cid)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];

	if (ctx->ulp_proto_id == CNIC_ULP_ISCSI) {
		struct cnic_iscsi *iscsi = ctx->proto.iscsi;

		cnic_free_dma(dev, &iscsi->hq_info);
		cnic_free_dma(dev, &iscsi->r2tq_info);
		cnic_free_dma(dev, &iscsi->task_array_info);
		cnic_free_id(&cp->cid_tbl, ctx->cid);
	} else {
		cnic_free_id(&cp->fcoe_cid_tbl, ctx->cid);
	}

	ctx->cid = 0;
}

static int cnic_alloc_bnx2x_conn_resc(struct cnic_dev *dev, u32 l5_cid)
{
	u32 cid;
	int ret, pages;
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	struct cnic_iscsi *iscsi = ctx->proto.iscsi;

	if (ctx->ulp_proto_id == CNIC_ULP_FCOE) {
		cid = cnic_alloc_new_id(&cp->fcoe_cid_tbl);
		if (cid == -1) {
			CNIC_ERR("failed to allocate new id\n");
			ret = -ENOMEM;
			goto error;
		}
		ctx->cid = cid;
		return 0;
	}

	cid = cnic_alloc_new_id(&cp->cid_tbl);
	if (cid == -1) {
		CNIC_ERR("failed to allocate new id\n");
		ret = -ENOMEM;
		goto error;
	}

	ctx->cid = cid;
	pages = PAGE_ALIGN(cp->task_array_size) / PAGE_SIZE;

	ret = cnic_alloc_dma(dev, &iscsi->task_array_info, pages, 1);
	if (ret)
		goto error;

	pages = PAGE_ALIGN(cp->r2tq_size) / PAGE_SIZE;
	ret = cnic_alloc_dma(dev, &iscsi->r2tq_info, pages, 1);
	if (ret)
		goto error;

	pages = PAGE_ALIGN(cp->hq_size) / PAGE_SIZE;
	ret = cnic_alloc_dma(dev, &iscsi->hq_info, pages, 1);
	if (ret)
		goto error;

	return 0;

error:
	cnic_free_bnx2x_conn_resc(dev, l5_cid);
	return ret;
}

static void *cnic_get_bnx2x_ctx(struct cnic_dev *dev, u32 cid, int init,
				struct regpair *ctx_addr)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int blk = (cid - ethdev->starting_cid) / cp->cids_per_blk;
	int off = (cid - ethdev->starting_cid) % cp->cids_per_blk;
	unsigned long align_off = 0;
	dma_addr_t ctx_map;
	void *ctx;

	if (cp->ctx_align) {
		unsigned long mask = cp->ctx_align - 1;

		if (cp->ctx_arr[blk].mapping & mask)
			align_off = cp->ctx_align -
				    (cp->ctx_arr[blk].mapping & mask);
	}
	ctx_map = cp->ctx_arr[blk].mapping + align_off +
		(off * BNX2X_CONTEXT_MEM_SIZE);
	ctx = cp->ctx_arr[blk].ctx + align_off +
	       (off * BNX2X_CONTEXT_MEM_SIZE);
	if (init)
		memset(ctx, 0, BNX2X_CONTEXT_MEM_SIZE);

	ctx_addr->lo = ctx_map & 0xffffffff;
	ctx_addr->hi = (u64) ctx_map >> 32;
	return ctx;
}

static int cnic_setup_bnx2x_ctx(struct cnic_dev *dev, struct kwqe *wqes[],
				u32 num)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct iscsi_kwqe_conn_offload1 *req1 =
			(struct iscsi_kwqe_conn_offload1 *) wqes[0];
	struct iscsi_kwqe_conn_offload2 *req2 =
			(struct iscsi_kwqe_conn_offload2 *) wqes[1];
	struct iscsi_kwqe_conn_offload3 *req3;
	struct cnic_context *ctx = &cp->ctx_tbl[req1->iscsi_conn_id];
	struct cnic_iscsi *iscsi = ctx->proto.iscsi;
	u32 cid = ctx->cid;
	u32 hw_cid = BNX2X_HW_CID(bp, cid);
	struct iscsi_context *ictx;
	struct regpair context_addr;
	int i, j, n = 2, n_max;
#if (NEW_BNX2X_HSI >= 64)
	u8 port = BP_PORT(bp);
#endif

	ctx->ctx_flags = 0;
	if (!req2->num_additional_wqes)
		return -EINVAL;

	n_max = req2->num_additional_wqes + 2;

	ictx = cnic_get_bnx2x_ctx(dev, cid, 1, &context_addr);
	if (ictx == NULL)
		return -ENOMEM;

	req3 = (struct iscsi_kwqe_conn_offload3 *) wqes[n++];

	ictx->xstorm_ag_context.hq_prod = 1;

	ictx->xstorm_st_context.iscsi.first_burst_length =
		ISCSI_DEF_FIRST_BURST_LEN;
	ictx->xstorm_st_context.iscsi.max_send_pdu_length =
		ISCSI_DEF_MAX_RECV_SEG_LEN;
	ictx->xstorm_st_context.iscsi.sq_pbl_base.lo =
		req1->sq_page_table_addr_lo;
	ictx->xstorm_st_context.iscsi.sq_pbl_base.hi =
		req1->sq_page_table_addr_hi;
	ictx->xstorm_st_context.iscsi.sq_curr_pbe.lo = req2->sq_first_pte.hi;
	ictx->xstorm_st_context.iscsi.sq_curr_pbe.hi = req2->sq_first_pte.lo;
	ictx->xstorm_st_context.iscsi.hq_pbl_base.lo =
		iscsi->hq_info.pgtbl_map & 0xffffffff;
	ictx->xstorm_st_context.iscsi.hq_pbl_base.hi =
		(u64) iscsi->hq_info.pgtbl_map >> 32;
	ictx->xstorm_st_context.iscsi.hq_curr_pbe_base.lo =
		iscsi->hq_info.pgtbl[0];
	ictx->xstorm_st_context.iscsi.hq_curr_pbe_base.hi =
		iscsi->hq_info.pgtbl[1];
	ictx->xstorm_st_context.iscsi.r2tq_pbl_base.lo =
		iscsi->r2tq_info.pgtbl_map & 0xffffffff;
	ictx->xstorm_st_context.iscsi.r2tq_pbl_base.hi =
		(u64) iscsi->r2tq_info.pgtbl_map >> 32;
	ictx->xstorm_st_context.iscsi.r2tq_curr_pbe_base.lo =
		iscsi->r2tq_info.pgtbl[0];
	ictx->xstorm_st_context.iscsi.r2tq_curr_pbe_base.hi =
		iscsi->r2tq_info.pgtbl[1];
	ictx->xstorm_st_context.iscsi.task_pbl_base.lo =
		iscsi->task_array_info.pgtbl_map & 0xffffffff;
	ictx->xstorm_st_context.iscsi.task_pbl_base.hi =
		(u64) iscsi->task_array_info.pgtbl_map >> 32;
	ictx->xstorm_st_context.iscsi.task_pbl_cache_idx =
		BNX2X_ISCSI_PBL_NOT_CACHED;
	ictx->xstorm_st_context.iscsi.flags.flags |=
		XSTORM_ISCSI_CONTEXT_FLAGS_B_IMMEDIATE_DATA;
	ictx->xstorm_st_context.iscsi.flags.flags |=
		XSTORM_ISCSI_CONTEXT_FLAGS_B_INITIAL_R2T;
#if (NEW_BNX2X_HSI >= 64)
	ictx->xstorm_st_context.common.ethernet.reserved_vlan_type =
		ETH_P_8021Q;
	if (BNX2X_CHIP_IS_E2_PLUS(bp) &&
	    bp->common.chip_port_mode == CHIP_2_PORT_MODE) {

		port = 0;
	}
#if (NEW_BNX2X_HSI == 64)
        ictx->xstorm_st_context.common.tx_switching =
		(1 << XSTORM_COMMON_CONTEXT_SECTION_PHYSQ_INITIALIZED_SHIFT) |
		(port << XSTORM_COMMON_CONTEXT_SECTION_PBF_PORT_SHIFT);
#endif
#endif

	ictx->tstorm_st_context.iscsi.hdr_bytes_2_fetch = ISCSI_HEADER_SIZE;
	/* TSTORM requires the base address of RQ DB & not PTE */
	ictx->tstorm_st_context.iscsi.rq_db_phy_addr.lo =
		req2->rq_page_table_addr_lo & PAGE_MASK;
	ictx->tstorm_st_context.iscsi.rq_db_phy_addr.hi =
		req2->rq_page_table_addr_hi;
	ictx->tstorm_st_context.iscsi.iscsi_conn_id = req1->iscsi_conn_id;
	ictx->tstorm_st_context.tcp.cwnd = 0x5A8;
	ictx->tstorm_st_context.tcp.flags2 |=
		TSTORM_TCP_ST_CONTEXT_SECTION_DA_EN;
#if (NEW_BNX2X_HSI >= 60)
	ictx->tstorm_st_context.tcp.ooo_support_mode =
		TCP_TSTORM_OOO_DROP_AND_PROC_ACK;
#endif

	ictx->timers_context.flags |= TIMERS_BLOCK_CONTEXT_CONN_VALID_FLG;

	ictx->ustorm_st_context.ring.rq.pbl_base.lo =
		req2->rq_page_table_addr_lo;
	ictx->ustorm_st_context.ring.rq.pbl_base.hi =
		req2->rq_page_table_addr_hi;
	ictx->ustorm_st_context.ring.rq.curr_pbe.lo = req3->qp_first_pte[0].hi;
	ictx->ustorm_st_context.ring.rq.curr_pbe.hi = req3->qp_first_pte[0].lo;
	ictx->ustorm_st_context.ring.r2tq.pbl_base.lo =
		iscsi->r2tq_info.pgtbl_map & 0xffffffff;
	ictx->ustorm_st_context.ring.r2tq.pbl_base.hi =
		(u64) iscsi->r2tq_info.pgtbl_map >> 32;
	ictx->ustorm_st_context.ring.r2tq.curr_pbe.lo =
		iscsi->r2tq_info.pgtbl[0];
	ictx->ustorm_st_context.ring.r2tq.curr_pbe.hi =
		iscsi->r2tq_info.pgtbl[1];
	ictx->ustorm_st_context.ring.cq_pbl_base.lo =
		req1->cq_page_table_addr_lo;
	ictx->ustorm_st_context.ring.cq_pbl_base.hi =
		req1->cq_page_table_addr_hi;
	ictx->ustorm_st_context.ring.cq[0].cq_sn = ISCSI_INITIAL_SN;
	ictx->ustorm_st_context.ring.cq[0].curr_pbe.lo = req2->cq_first_pte.hi;
	ictx->ustorm_st_context.ring.cq[0].curr_pbe.hi = req2->cq_first_pte.lo;
	ictx->ustorm_st_context.task_pbe_cache_index =
		BNX2X_ISCSI_PBL_NOT_CACHED;
	ictx->ustorm_st_context.task_pdu_cache_index =
		BNX2X_ISCSI_PDU_HEADER_NOT_CACHED;

	for (i = 1, j = 1; i < cp->num_cqs; i++, j++) {
		if (j == 3) {
			if (n >= n_max)
				break;
			req3 = (struct iscsi_kwqe_conn_offload3 *) wqes[n++];
			j = 0;
		}
		ictx->ustorm_st_context.ring.cq[i].cq_sn = ISCSI_INITIAL_SN;
		ictx->ustorm_st_context.ring.cq[i].curr_pbe.lo =
			req3->qp_first_pte[j].hi;
		ictx->ustorm_st_context.ring.cq[i].curr_pbe.hi =
			req3->qp_first_pte[j].lo;
	}

	ictx->ustorm_st_context.task_pbl_base.lo =
		iscsi->task_array_info.pgtbl_map & 0xffffffff;
	ictx->ustorm_st_context.task_pbl_base.hi =
		(u64) iscsi->task_array_info.pgtbl_map >> 32;
	ictx->ustorm_st_context.tce_phy_addr.lo =
		iscsi->task_array_info.pgtbl[0];
	ictx->ustorm_st_context.tce_phy_addr.hi =
		iscsi->task_array_info.pgtbl[1];
	ictx->ustorm_st_context.iscsi_conn_id = req1->iscsi_conn_id;
	ictx->ustorm_st_context.num_cqs = cp->num_cqs;
	ictx->ustorm_st_context.negotiated_rx |= ISCSI_DEF_MAX_RECV_SEG_LEN;
	ictx->ustorm_st_context.negotiated_rx_and_flags |=
		ISCSI_DEF_MAX_BURST_LEN;
	ictx->ustorm_st_context.negotiated_rx |=
		ISCSI_DEFAULT_MAX_OUTSTANDING_R2T <<
		USTORM_ISCSI_ST_CONTEXT_MAX_OUTSTANDING_R2TS_SHIFT;

	ictx->cstorm_st_context.hq_pbl_base.lo =
		iscsi->hq_info.pgtbl_map & 0xffffffff;
	ictx->cstorm_st_context.hq_pbl_base.hi =
		(u64) iscsi->hq_info.pgtbl_map >> 32;
	ictx->cstorm_st_context.hq_curr_pbe.lo = iscsi->hq_info.pgtbl[0];
	ictx->cstorm_st_context.hq_curr_pbe.hi = iscsi->hq_info.pgtbl[1];
	ictx->cstorm_st_context.task_pbl_base.lo =
		iscsi->task_array_info.pgtbl_map & 0xffffffff;
	ictx->cstorm_st_context.task_pbl_base.hi =
		(u64) iscsi->task_array_info.pgtbl_map >> 32;
	/* CSTORM and USTORM initialization is different, CSTORM requires
	 * CQ DB base & not PTE addr */
	ictx->cstorm_st_context.cq_db_base.lo =
		req1->cq_page_table_addr_lo & PAGE_MASK;
	ictx->cstorm_st_context.cq_db_base.hi = req1->cq_page_table_addr_hi;
	ictx->cstorm_st_context.iscsi_conn_id = req1->iscsi_conn_id;
	ictx->cstorm_st_context.cq_proc_en_bit_map = (1 << cp->num_cqs) - 1;
	for (i = 0; i < cp->num_cqs; i++) {
		ictx->cstorm_st_context.cq_c_prod_sqn_arr.sqn[i] =
			ISCSI_INITIAL_SN;
		ictx->cstorm_st_context.cq_c_sqn_2_notify_arr.sqn[i] =
			ISCSI_INITIAL_SN;
	}

	ictx->xstorm_ag_context.cdu_reserved =
		CDU_RSRVD_VALUE_TYPE_A(hw_cid, CDU_REGION_NUMBER_XCM_AG,
				       ISCSI_CONNECTION_TYPE);
	ictx->ustorm_ag_context.cdu_usage =
		CDU_RSRVD_VALUE_TYPE_A(hw_cid, CDU_REGION_NUMBER_UCM_AG,
				       ISCSI_CONNECTION_TYPE);
	return 0;

}

static int cnic_bnx2x_iscsi_ofld1(struct cnic_dev *dev, struct kwqe *wqes[],
				   u32 num, int *work)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct iscsi_kwqe_conn_offload1 *req1;
	struct iscsi_kwqe_conn_offload2 *req2;
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx;
	struct iscsi_kcqe kcqe;
	struct kcqe *cqes[1];
	u32 l5_cid;
	int ret = 0;

	if (num < 2) {
		*work = num;
		return -EINVAL;
	}

	req1 = (struct iscsi_kwqe_conn_offload1 *) wqes[0];
	req2 = (struct iscsi_kwqe_conn_offload2 *) wqes[1];
	if ((num - 2) < req2->num_additional_wqes) {
		*work = num;
		return -EINVAL;
	}
	*work = 2 + req2->num_additional_wqes;

	l5_cid = req1->iscsi_conn_id;
	if (l5_cid >= MAX_ISCSI_TBL_SZ)
		return -EINVAL;

	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.op_code = ISCSI_KCQE_OPCODE_OFFLOAD_CONN;
	kcqe.iscsi_conn_id = l5_cid;
	kcqe.completion_status = ISCSI_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAILURE;

	ctx = &cp->ctx_tbl[l5_cid];
	if (test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags)) {
		kcqe.completion_status =
			ISCSI_KCQE_COMPLETION_STATUS_CID_BUSY;
		goto done;
	}

	if (atomic_inc_return(&cp->iscsi_conn) > dev->max_iscsi_conn) {
		atomic_dec(&cp->iscsi_conn);
		goto done;
	}
	ret = cnic_alloc_bnx2x_conn_resc(dev, l5_cid);
	if (ret) {
		atomic_dec(&cp->iscsi_conn);
		ret = 0;
		goto done;
	}
	ret = cnic_setup_bnx2x_ctx(dev, wqes, num);
	if (ret < 0) {
		cnic_free_bnx2x_conn_resc(dev, l5_cid);
		atomic_dec(&cp->iscsi_conn);
		goto done;
	}

	kcqe.completion_status = ISCSI_KCQE_COMPLETION_STATUS_SUCCESS;
	kcqe.iscsi_conn_context_id = BNX2X_HW_CID(bp, cp->ctx_tbl[l5_cid].cid);

done:
	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_ISCSI, cqes, 1);
	return 0;
}


static int cnic_bnx2x_iscsi_update(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iscsi_kwqe_conn_update *req =
		(struct iscsi_kwqe_conn_update *) kwqe;
	void *data;
	union l5cm_specific_data l5_data;
	u32 l5_cid, cid = BNX2X_SW_CID(req->context_id);
	int ret;

	if (cnic_get_l5_cid(cp, cid, &l5_cid) != 0)
		return -EINVAL;

	data = cnic_get_kwqe_16_data(cp, l5_cid, &l5_data);
	if (!data)
		return -ENOMEM;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_bnx2x_ooo_iscsi_conn_update(dev, kwqe);
#endif
	memcpy(data, kwqe, sizeof(struct kwqe));

	ret = cnic_submit_kwqe_16(dev, ISCSI_RAMROD_CMD_ID_UPDATE_CONN,
			req->context_id, ISCSI_CONNECTION_TYPE, &l5_data);
	return ret;
}

static int cnic_bnx2x_destroy_ramrod(struct cnic_dev *dev, u32 l5_cid)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	union l5cm_specific_data l5_data;
	int ret;
	u32 hw_cid, type;

	init_waitqueue_head(&ctx->waitq);
	ctx->wait_cond = 0;
	memset(&l5_data, 0, sizeof(l5_data));
	hw_cid = BNX2X_HW_CID(bp, ctx->cid);
#if (NEW_BNX2X_HSI >= 60)
	type = NONE_CONNECTION_TYPE;

	ret = cnic_submit_kwqe_16(dev, RAMROD_CMD_ID_COMMON_CFC_DEL,
				  hw_cid, type, &l5_data);
#else
	type = ETH_CONNECTION_TYPE | (1 << SPE_HDR_COMMON_RAMROD_SHIFT);

	ret = cnic_submit_kwqe_16(dev, RAMROD_CMD_ID_ETH_CFC_DEL,
				  hw_cid, type, &l5_data);
#endif

	if (ret == 0) {
		wait_event_timeout(ctx->waitq, ctx->wait_cond, BNX2X_RAMROD_TO);
		if (unlikely(test_bit(CTX_FL_CID_ERROR, &ctx->ctx_flags)))
			return -EBUSY;
	}

	return 0;
}

static int cnic_bnx2x_iscsi_destroy(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iscsi_kwqe_conn_destroy *req =
		(struct iscsi_kwqe_conn_destroy *) kwqe;
	u32 l5_cid = req->reserved0;
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	int ret = 0;
	struct iscsi_kcqe kcqe;
	struct kcqe *cqes[1];

	if (!test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags))
		goto skip_cfc_delete;

	if (!time_after(jiffies, ctx->timestamp + (2 * HZ))) {
		unsigned long delta = ctx->timestamp + (2 * HZ) - jiffies;

		if (delta > (2 * HZ))
			delta = 0;

		set_bit(CTX_FL_DELETE_WAIT, &ctx->ctx_flags);
		queue_delayed_work(cnic_wq, &cp->delete_task, delta);
		goto destroy_reply;
	}

	ret = cnic_bnx2x_destroy_ramrod(dev, l5_cid);

skip_cfc_delete:
	cnic_free_bnx2x_conn_resc(dev, l5_cid);

	if (!ret) {
		atomic_dec(&cp->iscsi_conn);
		clear_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags);
	}

destroy_reply:
	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.op_code = ISCSI_KCQE_OPCODE_DESTROY_CONN;
	kcqe.iscsi_conn_id = l5_cid;
	kcqe.completion_status = ISCSI_KCQE_COMPLETION_STATUS_SUCCESS;
	kcqe.iscsi_conn_context_id = req->context_id;

	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_ISCSI, cqes, 1);

	return 0;
}

static void cnic_init_storm_conn_bufs(struct cnic_dev *dev,
				      struct l4_kwq_connect_req1 *kwqe1,
				      struct l4_kwq_connect_req3 *kwqe3,
				      struct l5cm_active_conn_buffer *conn_buf)
{
	struct l5cm_conn_addr_params *conn_addr = &conn_buf->conn_addr_buf;
	struct l5cm_xstorm_conn_buffer *xstorm_buf =
		&conn_buf->xstorm_conn_buffer;
	struct l5cm_tstorm_conn_buffer *tstorm_buf =
		&conn_buf->tstorm_conn_buffer;
	struct regpair context_addr;
	u32 cid = BNX2X_SW_CID(kwqe1->cid);
	struct in6_addr src_ip, dst_ip;
	int i;
	u32 *addrp;

	addrp = (u32 *) &conn_addr->local_ip_addr;
	for (i = 0; i < 4; i++, addrp++)
		src_ip.in6_u.u6_addr32[i] = cpu_to_be32(*addrp);

	addrp = (u32 *) &conn_addr->remote_ip_addr;
	for (i = 0; i < 4; i++, addrp++)
		dst_ip.in6_u.u6_addr32[i] = cpu_to_be32(*addrp);

	cnic_get_bnx2x_ctx(dev, cid, 0, &context_addr);

	xstorm_buf->context_addr.hi = context_addr.hi;
	xstorm_buf->context_addr.lo = context_addr.lo;
	xstorm_buf->mss = 0xffff;
	xstorm_buf->rcv_buf = kwqe3->rcv_buf;
	if (kwqe1->tcp_flags & L4_KWQ_CONNECT_REQ1_NAGLE_ENABLE)
		xstorm_buf->params |= L5CM_XSTORM_CONN_BUFFER_NAGLE_ENABLE;
	xstorm_buf->pseudo_header_checksum =
		swab16(~csum_ipv6_magic(&src_ip, &dst_ip, 0, IPPROTO_TCP, 0));

	if (kwqe3->ka_timeout) {
		tstorm_buf->ka_enable = 1;
		tstorm_buf->ka_timeout = kwqe3->ka_timeout;
		tstorm_buf->ka_interval = kwqe3->ka_interval;
		tstorm_buf->ka_max_probe_count = kwqe3->ka_max_probe_count;
	}
#if (NEW_BNX2X_HSI <= 60)
	tstorm_buf->rcv_buf = kwqe3->rcv_buf;
	tstorm_buf->snd_buf = kwqe3->snd_buf;
#endif
	tstorm_buf->max_rt_time = 0xffffffff;
}

static void cnic_init_bnx2x_mac(struct cnic_dev *dev, u8 *mac)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 pfid = bp->pfid;
	memcpy(cp->ethdev->iscsi_mac, mac, 6);

	if (cp->ethdev->mf_mode == MULTI_FUNCTION_SI || 
	    cp->ethdev->mf_mode == MULTI_FUNCTION_AFEX)
		cnic_npar_ring_ctl(dev, 1);	

	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR0_OFFSET(pfid), mac[0]);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR1_OFFSET(pfid), mac[1]);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR2_OFFSET(pfid), mac[2]);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR3_OFFSET(pfid), mac[3]);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR4_OFFSET(pfid), mac[4]);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_LOCAL_MAC_ADDR5_OFFSET(pfid), mac[5]);

	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_LSB_LOCAL_MAC_ADDR_OFFSET(pfid), mac[5]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_LSB_LOCAL_MAC_ADDR_OFFSET(pfid) + 1,
		 mac[4]);
#if (NEW_BNX2X_HSI >= 64)
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MID_LOCAL_MAC_ADDR_OFFSET(pfid), mac[3]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MID_LOCAL_MAC_ADDR_OFFSET(pfid) + 1,
		 mac[2]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid), mac[1]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid) + 1,
		 mac[0]);
#else
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid), mac[3]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid) + 1,
		 mac[2]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid) + 2,
		 mac[1]);
	CNIC_WR8(dev, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_TCP_VARS_MSB_LOCAL_MAC_ADDR_OFFSET(pfid) + 3,
		 mac[0]);
#endif
}

static int cnic_bnx2x_connect(struct cnic_dev *dev, struct kwqe *wqes[],
			      u32 num, int *work)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct l4_kwq_connect_req1 *kwqe1 =
		(struct l4_kwq_connect_req1 *) wqes[0];
	struct l4_kwq_connect_req3 *kwqe3;
	struct l5cm_active_conn_buffer *conn_buf;
	struct l5cm_conn_addr_params *conn_addr;
	union l5cm_specific_data l5_data;
	u32 l5_cid = kwqe1->pg_cid;
	struct cnic_sock *csk = &cp->csk_tbl[l5_cid];
	int ret;

	if (num < 2) {
		*work = num;
		return -EINVAL;
	}

	if (kwqe1->conn_flags & L4_KWQ_CONNECT_REQ1_IP_V6)
		*work = 3;
	else
		*work = 2;

	if (num < *work) {
		*work = num;
		return -EINVAL;
	}

	if (sizeof(*conn_buf) > CNIC_KWQ16_DATA_SIZE) {
		CNIC_ERR("conn_buf size too big\n");
		return -ENOMEM;
	}
	conn_buf = cnic_get_kwqe_16_data(cp, l5_cid, &l5_data);
	if (!conn_buf)
		return -ENOMEM;

	memset(conn_buf, 0, sizeof(*conn_buf));

	conn_addr = &conn_buf->conn_addr_buf;
	conn_addr->remote_addr_0 = csk->ha[0];
	conn_addr->remote_addr_1 = csk->ha[1];
	conn_addr->remote_addr_2 = csk->ha[2];
	conn_addr->remote_addr_3 = csk->ha[3];
	conn_addr->remote_addr_4 = csk->ha[4];
	conn_addr->remote_addr_5 = csk->ha[5];

#if defined (__VMKLNX__)
	/* Use the stored MAC address */
	cnic_init_bnx2x_mac(dev, cp->srcMACAddr);
#endif /* defined (__VMKLNX__) */

	if (kwqe1->conn_flags & L4_KWQ_CONNECT_REQ1_IP_V6) {
		struct l4_kwq_connect_req2 *kwqe2 =
			(struct l4_kwq_connect_req2 *) wqes[1];

		conn_addr->local_ip_addr.ip_addr_hi_hi = kwqe2->src_ip_v6_4;
		conn_addr->local_ip_addr.ip_addr_hi_lo = kwqe2->src_ip_v6_3;
		conn_addr->local_ip_addr.ip_addr_lo_hi = kwqe2->src_ip_v6_2;

		conn_addr->remote_ip_addr.ip_addr_hi_hi = kwqe2->dst_ip_v6_4;
		conn_addr->remote_ip_addr.ip_addr_hi_lo = kwqe2->dst_ip_v6_3;
		conn_addr->remote_ip_addr.ip_addr_lo_hi = kwqe2->dst_ip_v6_2;
		conn_addr->params |= L5CM_CONN_ADDR_PARAMS_IP_VERSION;
	}
	kwqe3 = (struct l4_kwq_connect_req3 *) wqes[*work - 1];

	conn_addr->local_ip_addr.ip_addr_lo_lo = kwqe1->src_ip;
	conn_addr->remote_ip_addr.ip_addr_lo_lo = kwqe1->dst_ip;
	conn_addr->local_tcp_port = kwqe1->src_port;
	conn_addr->remote_tcp_port = kwqe1->dst_port;

	conn_addr->pmtu = kwqe3->pmtu;
	cnic_init_storm_conn_bufs(dev, kwqe1, kwqe3, conn_buf);

	CNIC_WR16(dev, BAR_XSTRORM_INTMEM +
		  XSTORM_ISCSI_LOCAL_VLAN_OFFSET(bp->pfid), csk->vlan_id);

	ret = cnic_submit_kwqe_16(dev, L5CM_RAMROD_CMD_ID_TCP_CONNECT,
			kwqe1->cid, ISCSI_CONNECTION_TYPE, &l5_data);

	return ret;
}

static int cnic_bnx2x_close(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct l4_kwq_close_req *req = (struct l4_kwq_close_req *) kwqe;
	union l5cm_specific_data l5_data;
	int ret;

	memset(&l5_data, 0, sizeof(l5_data));
	ret = cnic_submit_kwqe_16(dev, L5CM_RAMROD_CMD_ID_CLOSE,
			req->cid, ISCSI_CONNECTION_TYPE, &l5_data);
	return ret;
}

static int cnic_bnx2x_reset(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct l4_kwq_reset_req *req = (struct l4_kwq_reset_req *) kwqe;
	union l5cm_specific_data l5_data;
	int ret;

	memset(&l5_data, 0, sizeof(l5_data));
	ret = cnic_submit_kwqe_16(dev, L5CM_RAMROD_CMD_ID_ABORT,
			req->cid, ISCSI_CONNECTION_TYPE, &l5_data);
	return ret;
}
static int cnic_bnx2x_offload_pg(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct l4_kwq_offload_pg *req = (struct l4_kwq_offload_pg *) kwqe;
	struct l4_kcq kcqe;
	struct kcqe *cqes[1];

	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.pg_host_opaque = req->host_opaque;
	kcqe.pg_cid = req->host_opaque;
	kcqe.op_code = L4_KCQE_OPCODE_VALUE_OFFLOAD_PG;
	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_L4, cqes, 1);
	return 0;
}

static int cnic_bnx2x_update_pg(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct l4_kwq_update_pg *req = (struct l4_kwq_update_pg *) kwqe;
	struct l4_kcq kcqe;
	struct kcqe *cqes[1];

	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.pg_host_opaque = req->pg_host_opaque;
	kcqe.pg_cid = req->pg_cid;
	kcqe.op_code = L4_KCQE_OPCODE_VALUE_UPDATE_PG;
	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_L4, cqes, 1);
	return 0;
}

static int cnic_bnx2x_fcoe_stat(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct fcoe_kwqe_stat *req;
	struct fcoe_stat_ramrod_params *fcoe_stat;
	union l5cm_specific_data l5_data;
	struct cnic_local *cp = dev->cnic_priv;
	int ret;
	u32 cid;

	req = (struct fcoe_kwqe_stat *) kwqe;
	cid = BNX2X_HW_CID(bp, cp->fcoe_init_cid);

	fcoe_stat = cnic_get_kwqe_16_data(cp, BNX2X_FCOE_L5_CID_BASE, &l5_data);
	if (!fcoe_stat)
		return -ENOMEM;

	memset(fcoe_stat, 0, sizeof(*fcoe_stat));
	memcpy(&fcoe_stat->stat_kwqe, req, sizeof(*req));

#if (NEW_BNX2X_HSI >= 64)
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_STAT_FUNC, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#else
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_STAT, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#endif
	return ret;
}

static int cnic_bnx2x_fcoe_init1(struct cnic_dev *dev, struct kwqe *wqes[],
				 u32 num, int *work)
{
	int ret;
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 cid;
	struct fcoe_init_ramrod_params *fcoe_init;
	struct fcoe_kwqe_init1 *req1;
	struct fcoe_kwqe_init2 *req2;
	struct fcoe_kwqe_init3 *req3;
	union l5cm_specific_data l5_data;

	if (num < 3) {
		*work = num;
		return -EINVAL;
	}
	req1 = (struct fcoe_kwqe_init1 *) wqes[0];
	req2 = (struct fcoe_kwqe_init2 *) wqes[1];
	req3 = (struct fcoe_kwqe_init3 *) wqes[2];
	if (req2->hdr.op_code != FCOE_KWQE_OPCODE_INIT2) {
		*work = 1;
		return -EINVAL;
	}
	if (req3->hdr.op_code != FCOE_KWQE_OPCODE_INIT3) {
		*work = 2;
		return -EINVAL;
	}

	if (sizeof(*fcoe_init) > CNIC_KWQ16_DATA_SIZE) {
		printk(KERN_ERR PFX "%s: fcoe_init size too big\n",
			       dev->netdev->name);
		return -ENOMEM;
	}
	fcoe_init = cnic_get_kwqe_16_data(cp, BNX2X_FCOE_L5_CID_BASE, &l5_data);
	if (!fcoe_init)
		return -ENOMEM;

	memset(fcoe_init, 0, sizeof(*fcoe_init));
	memcpy(&fcoe_init->init_kwqe1, req1, sizeof(*req1));
	memcpy(&fcoe_init->init_kwqe2, req2, sizeof(*req2));
	memcpy(&fcoe_init->init_kwqe3, req3, sizeof(*req3));
#if (NEW_BNX2X_HSI >= 64)
	fcoe_init->eq_pbl_base.lo = cp->kcq2.dma.pgtbl_map & 0xffffffff;
	fcoe_init->eq_pbl_base.hi = (u64) cp->kcq2.dma.pgtbl_map >> 32;
	fcoe_init->eq_pbl_size = cp->kcq2.dma.num_pages;
#else
	fcoe_init->eq_addr.lo = cp->kcq2.dma.pg_map_arr[0] & 0xffffffff;
	fcoe_init->eq_addr.hi = (u64) cp->kcq2.dma.pg_map_arr[0] >> 32;
	fcoe_init->eq_next_page_addr.lo =
		cp->kcq2.dma.pg_map_arr[1] & 0xffffffff;
	fcoe_init->eq_next_page_addr.hi =
		(u64) cp->kcq2.dma.pg_map_arr[1] >> 32;
#endif

	fcoe_init->sb_num = cp->status_blk_num;
	fcoe_init->eq_prod = MAX_KCQ_IDX;
#if (NEW_BNX2X_HSI >= 60)
	fcoe_init->sb_id = HC_INDEX_FCOE_EQ_CONS;
#else
	fcoe_init->sb_id = HC_INDEX_U_FCOE_EQ_CONS;
#endif
	cp->kcq2.sw_prod_idx = 0;

	cid = BNX2X_HW_CID(bp, cp->fcoe_init_cid);
	printk(KERN_ERR "bdbg: submitting INIT RAMROD\n");
#if (NEW_BNX2X_HSI >= 64)
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_INIT_FUNC, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#else
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_INIT, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#endif
	*work = 3;
	return ret;
}

static int cnic_bnx2x_fcoe_ofld1(struct cnic_dev *dev, struct kwqe *wqes[],
				 u32 num, int *work)
{
	int ret = 0;
	u32 cid = -1, l5_cid;
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct fcoe_kwqe_conn_offload1 *req1;
	struct fcoe_kwqe_conn_offload2 *req2;
	struct fcoe_kwqe_conn_offload3 *req3;
	struct fcoe_kwqe_conn_offload4 *req4;
	struct fcoe_conn_offload_ramrod_params *fcoe_offload;
	struct cnic_context *ctx;
	struct fcoe_context *fctx;
	struct regpair ctx_addr;
	union l5cm_specific_data l5_data;
	struct fcoe_kcqe kcqe;
	struct kcqe *cqes[1];

	if (num < 4) {
		*work = num;
		return -EINVAL;
	}
	req1 = (struct fcoe_kwqe_conn_offload1 *) wqes[0];
	req2 = (struct fcoe_kwqe_conn_offload2 *) wqes[1];
	req3 = (struct fcoe_kwqe_conn_offload3 *) wqes[2];
	req4 = (struct fcoe_kwqe_conn_offload4 *) wqes[3];

	*work = 4;

	l5_cid = req1->fcoe_conn_id;
	if (l5_cid >= dev->max_fcoe_conn)
		goto err_reply;

	l5_cid += BNX2X_FCOE_L5_CID_BASE;

	ctx = &cp->ctx_tbl[l5_cid];
	if (test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags))
		goto err_reply;

	ret = cnic_alloc_bnx2x_conn_resc(dev, l5_cid);
	if (ret) {
		ret = 0;
		goto err_reply;
	}
	cid = ctx->cid;

	fctx = cnic_get_bnx2x_ctx(dev, cid, 1, &ctx_addr);
	if (fctx) {
		u32 hw_cid = BNX2X_HW_CID(bp, cid);
		u32 val;

		val = CDU_RSRVD_VALUE_TYPE_A(hw_cid, CDU_REGION_NUMBER_XCM_AG,
					     FCOE_CONNECTION_TYPE);
		fctx->xstorm_ag_context.cdu_reserved = val;
		val = CDU_RSRVD_VALUE_TYPE_A(hw_cid, CDU_REGION_NUMBER_UCM_AG,
					     FCOE_CONNECTION_TYPE);
		fctx->ustorm_ag_context.cdu_usage = val;
	}
	if (sizeof(*fcoe_offload) > CNIC_KWQ16_DATA_SIZE) {
		printk(KERN_ERR PFX "%s: fcoe_offload size too big\n",
			       dev->netdev->name);
		goto err_reply;
	}
	fcoe_offload = cnic_get_kwqe_16_data(cp, l5_cid, &l5_data);
	if (!fcoe_offload)
		goto err_reply;

	memset(fcoe_offload, 0, sizeof(*fcoe_offload));
	memcpy(&fcoe_offload->offload_kwqe1, req1, sizeof(*req1));
	memcpy(&fcoe_offload->offload_kwqe2, req2, sizeof(*req2));
	memcpy(&fcoe_offload->offload_kwqe3, req3, sizeof(*req3));
	memcpy(&fcoe_offload->offload_kwqe4, req4, sizeof(*req4));

	cid = BNX2X_HW_CID(bp, cid);
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_OFFLOAD_CONN, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
	if (!ret)
		set_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags);

	return ret;

err_reply:
	if (cid != -1)
		cnic_free_bnx2x_conn_resc(dev, l5_cid);

	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.op_code = FCOE_KCQE_OPCODE_OFFLOAD_CONN;
	kcqe.fcoe_conn_id = req1->fcoe_conn_id;
	kcqe.completion_status = FCOE_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAILURE;

	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_FCOE, cqes, 1);
	return ret;
}

static int cnic_bnx2x_fcoe_enable(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct fcoe_kwqe_conn_enable_disable *req;
	struct fcoe_conn_enable_disable_ramrod_params *fcoe_enable;
	union l5cm_specific_data l5_data;
	int ret;
	u32 cid, l5_cid;
	struct cnic_local *cp = dev->cnic_priv;

	req = (struct fcoe_kwqe_conn_enable_disable *) kwqe;
	cid = req->context_id;
	l5_cid = req->conn_id + BNX2X_FCOE_L5_CID_BASE;

	if (sizeof(*fcoe_enable) > CNIC_KWQ16_DATA_SIZE) {
		printk(KERN_ERR PFX "%s: fcoe_enable size too big\n",
			       dev->netdev->name);
		return -ENOMEM;
	}
	fcoe_enable = cnic_get_kwqe_16_data(cp, l5_cid, &l5_data);
	if (!fcoe_enable)
		return -ENOMEM;

	memset(fcoe_enable, 0, sizeof(*fcoe_enable));
	memcpy(&fcoe_enable->enable_disable_kwqe, req, sizeof(*req));
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_ENABLE_CONN, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
	return ret;
}

static int cnic_bnx2x_fcoe_disable(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct fcoe_kwqe_conn_enable_disable *req;
	struct fcoe_conn_enable_disable_ramrod_params *fcoe_disable;
	union l5cm_specific_data l5_data;
	int ret;
	u32 cid, l5_cid;
	struct cnic_local *cp = dev->cnic_priv;

	req = (struct fcoe_kwqe_conn_enable_disable *) kwqe;
	cid = req->context_id;
	l5_cid = req->conn_id;
	if (l5_cid >= dev->max_fcoe_conn)
		return -EINVAL;

	l5_cid += BNX2X_FCOE_L5_CID_BASE;

	if (sizeof(*fcoe_disable) > CNIC_KWQ16_DATA_SIZE) {
		printk(KERN_ERR PFX "%s: fcoe_disable size too big\n",
			       dev->netdev->name);
		return -ENOMEM;
	}
	fcoe_disable = cnic_get_kwqe_16_data(cp, l5_cid, &l5_data);
	if (!fcoe_disable)
		return -ENOMEM;

	memset(fcoe_disable, 0, sizeof(*fcoe_disable));
	memcpy(&fcoe_disable->enable_disable_kwqe, req, sizeof(*req));
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_DISABLE_CONN, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
	return ret;
}

static int cnic_bnx2x_fcoe_destroy(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct fcoe_kwqe_conn_destroy *req;
	union l5cm_specific_data l5_data;
	int ret;
	u32 cid, l5_cid;
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx;
	struct fcoe_kcqe kcqe;
	struct kcqe *cqes[1];

	req = (struct fcoe_kwqe_conn_destroy *) kwqe;
	cid = req->context_id;
	l5_cid = req->conn_id;
	if (l5_cid >= dev->max_fcoe_conn)
		return -EINVAL;

	l5_cid += BNX2X_FCOE_L5_CID_BASE;

	ctx = &cp->ctx_tbl[l5_cid];

	init_waitqueue_head(&ctx->waitq);
	ctx->wait_cond = 0;

	memset(&kcqe, 0, sizeof(kcqe));
	kcqe.completion_status = FCOE_KCQE_COMPLETION_STATUS_ERROR;
	memset(&l5_data, 0, sizeof(l5_data));
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_TERMINATE_CONN, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
	if (ret == 0) {
		wait_event_timeout(ctx->waitq, ctx->wait_cond, BNX2X_RAMROD_TO);
		if (ctx->wait_cond)
			kcqe.completion_status = 0;
	}
	set_bit(CTX_FL_DELETE_WAIT, &ctx->ctx_flags);
	//queue_delayed_work(cnic_wq, &cp->delete_task,	// Anil
	schedule_delayed_work(&cp->delete_task,
				msecs_to_jiffies(2000));

	kcqe.op_code = FCOE_KCQE_OPCODE_DESTROY_CONN;
	kcqe.fcoe_conn_id = req->conn_id;
	kcqe.fcoe_conn_context_id = cid;

	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, CNIC_ULP_FCOE, cqes, 1);
	return ret;
}

static void cnic_bnx2x_delete_wait(struct cnic_dev *dev, u32 start_cid)
{
	struct cnic_local *cp = dev->cnic_priv;
	u32 i;

	for (i = start_cid; i < cp->max_cid_space; i++) {
		struct cnic_context *ctx = &cp->ctx_tbl[i];
		int j;

		while (test_bit(CTX_FL_DELETE_WAIT, &ctx->ctx_flags))
			msleep(10);

		for (j = 0; j < 5; j++) {
			if (!test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags))
				break;
			msleep(20);
		}

		if (test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags))
			printk(KERN_WARNING PFX "%s: CID %x not deleted\n",
				dev->netdev->name,   
				ctx->cid);
	}
}

static int cnic_bnx2x_fcoe_fw_destroy(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct fcoe_kwqe_destroy *req;
	union l5cm_specific_data l5_data;
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	int ret;
	u32 cid;

	cnic_bnx2x_delete_wait(dev, MAX_ISCSI_TBL_SZ);

	req = (struct fcoe_kwqe_destroy *) kwqe;
	cid = BNX2X_HW_CID(bp, cp->fcoe_init_cid);

	memset(&l5_data, 0, sizeof(l5_data));
#if (NEW_BNX2X_HSI >= 64)
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_DESTROY_FUNC, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#else
	ret = cnic_submit_kwqe_16(dev, FCOE_RAMROD_CMD_ID_DESTROY, cid,
				  FCOE_CONNECTION_TYPE, &l5_data);
#endif
	return ret;
}

static void cnic_bnx2x_kwqe_err(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct kcqe kcqe;
	struct kcqe *cqes[1];
	u32 cid;
	u32 opcode = KWQE_OPCODE(kwqe->kwqe_op_flag);
	u32 layer_code = kwqe->kwqe_op_flag & KWQE_LAYER_MASK;
	int ulp_type;

	cid = kwqe->kwqe_info0;
	memset(&kcqe, 0, sizeof(kcqe));

	if (layer_code == KWQE_FLAGS_LAYER_MASK_L5_ISCSI) {
		ulp_type = CNIC_ULP_ISCSI;
		if (opcode == ISCSI_KWQE_OPCODE_UPDATE_CONN)
			cid = kwqe->kwqe_info1;

		kcqe.kcqe_op_flag = (opcode + 0x10) << KCQE_FLAGS_OPCODE_SHIFT;
		kcqe.kcqe_op_flag |= KCQE_FLAGS_LAYER_MASK_L5_ISCSI;
		kcqe.kcqe_info1 = FCOE_KCQE_COMPLETION_STATUS_PARITY_ERROR;
		kcqe.kcqe_info2 = cid;
		cnic_get_l5_cid(cp, BNX2X_SW_CID(cid), &kcqe.kcqe_info0);

	} else if (layer_code == KWQE_FLAGS_LAYER_MASK_L4) {
		struct l4_kcq *l4kcqe = (struct l4_kcq *) &kcqe;
		u32 kcqe_op;

		ulp_type = CNIC_ULP_L4;
		if (opcode == L4_KWQE_OPCODE_VALUE_CONNECT1)
			kcqe_op = L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE;
		else if (opcode == L4_KWQE_OPCODE_VALUE_RESET)
			kcqe_op = L4_KCQE_OPCODE_VALUE_RESET_COMP;
		else if (opcode == L4_KWQE_OPCODE_VALUE_CLOSE)
			kcqe_op = L4_KCQE_OPCODE_VALUE_CLOSE_COMP;
		else
			return;

		kcqe.kcqe_op_flag = (kcqe_op << KCQE_FLAGS_OPCODE_SHIFT) |
				    KCQE_FLAGS_LAYER_MASK_L4;
		l4kcqe->status = L4_KCQE_COMPLETION_STATUS_PARITY_ERROR;
		l4kcqe->cid = cid;
		cnic_get_l5_cid(cp, BNX2X_SW_CID(cid), &l4kcqe->conn_id);
	} else {
		return;
	}

	cqes[0] = (struct kcqe *) &kcqe;
	cnic_reply_bnx2x_kcqes(dev, ulp_type, cqes, 1);
}

static int cnic_submit_bnx2x_iscsi_kwqes(struct cnic_dev *dev,
					 struct kwqe *wqes[], u32 num_wqes)
{
	int i, work, ret;
	u32 opcode;
	struct kwqe *kwqe;

	if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return -EAGAIN;		/* bnx2 is down */

	for (i = 0; i < num_wqes; ) {
		kwqe = wqes[i];
		opcode = KWQE_OPCODE(kwqe->kwqe_op_flag);
		work = 1;

		switch (opcode) {
		case ISCSI_KWQE_OPCODE_INIT1:
			ret = cnic_bnx2x_iscsi_init1(dev, kwqe);
			break;
		case ISCSI_KWQE_OPCODE_INIT2:
			ret = cnic_bnx2x_iscsi_init2(dev, kwqe);
			break;
		case ISCSI_KWQE_OPCODE_OFFLOAD_CONN1:
			ret = cnic_bnx2x_iscsi_ofld1(dev, &wqes[i],
						     num_wqes - i, &work);
			break;
		case ISCSI_KWQE_OPCODE_UPDATE_CONN:
			ret = cnic_bnx2x_iscsi_update(dev, kwqe);
			break;
		case ISCSI_KWQE_OPCODE_DESTROY_CONN:
			ret = cnic_bnx2x_iscsi_destroy(dev, kwqe);
			break;
		case L4_KWQE_OPCODE_VALUE_CONNECT1:
			ret = cnic_bnx2x_connect(dev, &wqes[i], num_wqes - i,
						 &work);
			break;
		case L4_KWQE_OPCODE_VALUE_CLOSE:
			ret = cnic_bnx2x_close(dev, kwqe);
			break;
		case L4_KWQE_OPCODE_VALUE_RESET:
			ret = cnic_bnx2x_reset(dev, kwqe);
			break;
		case L4_KWQE_OPCODE_VALUE_OFFLOAD_PG:
			ret = cnic_bnx2x_offload_pg(dev, kwqe);
			break;
		case L4_KWQE_OPCODE_VALUE_UPDATE_PG:
			ret = cnic_bnx2x_update_pg(dev, kwqe);
			break;
		case L4_KWQE_OPCODE_VALUE_UPLOAD_PG:
			ret = 0;
			break;
		default:
			ret = 0;
			CNIC_ERR("Unknown type of KWQE(0x%x)\n", opcode);
			break;
		}
		if (ret < 0) {
			CNIC_ERR("KWQE(0x%x) failed\n", opcode);
			
			if (ret == -EIO || ret == -EAGAIN)
				cnic_bnx2x_kwqe_err(dev, kwqe);
		}
		i += work;
	}
	return 0;
}

static int cnic_submit_bnx2x_fcoe_kwqes(struct cnic_dev *dev,
					struct kwqe *wqes[], u32 num_wqes)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	int i, work, ret;
	u32 opcode;
	struct kwqe *kwqe;

	if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return -EAGAIN;		/* bnx2 is down */

	if (!BNX2X_CHIP_IS_E2_PLUS(bp))
		return -EINVAL;

	for (i = 0; i < num_wqes; ) {
		kwqe = wqes[i];
		opcode = KWQE_OPCODE(kwqe->kwqe_op_flag);
		work = 1;

		switch (opcode) {
		case FCOE_KWQE_OPCODE_INIT1:
			ret = cnic_bnx2x_fcoe_init1(dev, &wqes[i],
						    num_wqes - i, &work);
			break;
		case FCOE_KWQE_OPCODE_OFFLOAD_CONN1:
			ret = cnic_bnx2x_fcoe_ofld1(dev, &wqes[i],
						    num_wqes - i, &work);
			break;
		case FCOE_KWQE_OPCODE_ENABLE_CONN:
			ret = cnic_bnx2x_fcoe_enable(dev, kwqe);
			break;
		case FCOE_KWQE_OPCODE_DISABLE_CONN:
			ret = cnic_bnx2x_fcoe_disable(dev, kwqe);
			break;
		case FCOE_KWQE_OPCODE_DESTROY_CONN:
			ret = cnic_bnx2x_fcoe_destroy(dev, kwqe);
			break;
		case FCOE_KWQE_OPCODE_DESTROY:
			ret = cnic_bnx2x_fcoe_fw_destroy(dev, kwqe);
			break;
		case FCOE_KWQE_OPCODE_STAT:
			ret = cnic_bnx2x_fcoe_stat(dev, kwqe);
			break;
		default:
			ret = 0;
			printk(KERN_ERR PFX "%s: Unknown type of KWQE(0x%x)\n",
			       dev->netdev->name, opcode);
			break;
		}
		if (ret < 0)
			printk(KERN_ERR PFX "%s: KWQE(0x%x) failed\n",
			       dev->netdev->name, opcode);
		i += work;
	}
	return 0;
}

static int cnic_submit_bnx2x_kwqes(struct cnic_dev *dev, struct kwqe *wqes[],
				   u32 num_wqes)
{
	int ret = -EINVAL;
	u32 layer_code;

	if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return -EAGAIN;		/* bnx2x is down */

	if (!num_wqes)
		return 0;

	layer_code = wqes[0]->kwqe_op_flag & KWQE_LAYER_MASK;
	switch (layer_code) {
	case KWQE_FLAGS_LAYER_MASK_L5_ISCSI:
	case KWQE_FLAGS_LAYER_MASK_L4:
	case KWQE_FLAGS_LAYER_MASK_L2:
		ret = cnic_submit_bnx2x_iscsi_kwqes(dev, wqes, num_wqes);
		break;

	case KWQE_FLAGS_LAYER_MASK_L5_FCOE:
		ret = cnic_submit_bnx2x_fcoe_kwqes(dev, wqes, num_wqes);
		break;
	}
	return ret;
}

static inline u32 cnic_get_kcqe_layer_mask(u32 opflag)
{
	if (unlikely(KCQE_OPCODE(opflag) == FCOE_RAMROD_CMD_ID_TERMINATE_CONN))
		return KCQE_FLAGS_LAYER_MASK_L4;

	return opflag & KCQE_FLAGS_LAYER_MASK;
}

static void service_kcqes(struct cnic_dev *dev, int num_cqes)
{
	struct cnic_local *cp = dev->cnic_priv;
	int i, j, comp = 0;

	i = 0;
	j = 1;
	while (num_cqes) {
		struct cnic_ulp_ops *ulp_ops;
		int ulp_type;
		u32 kcqe_op_flag = cp->completed_kcq[i]->kcqe_op_flag;
		u32 kcqe_layer = cnic_get_kcqe_layer_mask(kcqe_op_flag);

		if (unlikely(kcqe_op_flag & KCQE_RAMROD_COMPLETION))
			comp++;

		while (j < num_cqes) {
			u32 next_op = cp->completed_kcq[i + j]->kcqe_op_flag;

			if (cnic_get_kcqe_layer_mask(next_op) != kcqe_layer)
				break;

			if (unlikely(next_op & KCQE_RAMROD_COMPLETION))
				comp++;
			j++;
		}

		if (kcqe_layer == KCQE_FLAGS_LAYER_MASK_L5_RDMA)
			ulp_type = CNIC_ULP_RDMA;
		else if (kcqe_layer == KCQE_FLAGS_LAYER_MASK_L5_ISCSI)
			ulp_type = CNIC_ULP_ISCSI;
		else if (kcqe_layer == KCQE_FLAGS_LAYER_MASK_L5_FCOE)
			ulp_type = CNIC_ULP_FCOE;
		else if (kcqe_layer == KCQE_FLAGS_LAYER_MASK_L4)
			ulp_type = CNIC_ULP_L4;
		else if (kcqe_layer == KCQE_FLAGS_LAYER_MASK_L2)
			goto end;
		else {
			CNIC_ERR("Unknown type of KCQE(0x%x)\n", kcqe_op_flag);
			goto end;
		}

		rcu_read_lock();
		ulp_ops = rcu_dereference(cp->ulp_ops[ulp_type]);
		if (likely(ulp_ops)) {
			ulp_ops->indicate_kcqes(cp->ulp_handle[ulp_type],
						  cp->completed_kcq + i, j);
		}
		rcu_read_unlock();
end:
		num_cqes -= j;
		i += j;
		j = 1;
	}
	if (unlikely(comp))
		cnic_spq_completion(dev, DRV_CTL_RET_L5_SPQ_CREDIT_CMD, comp);
}



static int cnic_get_kcqes(struct cnic_dev *dev, struct kcq_info *info)
{
	struct cnic_local *cp = dev->cnic_priv;
	u16 i, ri, hw_prod, last;
	struct kcqe *kcqe;
	int kcqe_cnt = 0, last_cnt = 0;
	u32 kcq_diff;

	i = ri = last = info->sw_prod_idx;
	ri &= MAX_KCQ_IDX;
	hw_prod = *info->hw_prod_idx_ptr;
	hw_prod = info->hw_idx(hw_prod);

	if (unlikely(hw_prod < last))
		kcq_diff = (65536 + hw_prod) - last;
	else
		kcq_diff = hw_prod - last;

	if (unlikely(kcq_diff > MAX_KCQ_IDX))
		printk(KERN_WARNING PFX "%s: kcq abs(hw_prod(%d) - sw_prod(%d))"
			" > MAX_KCQ_IDX(%lu)\n", dev->netdev->name,
			hw_prod, last, (unsigned long) MAX_KCQ_IDX);

	while ((i != hw_prod) && (kcqe_cnt < MAX_COMPLETED_KCQE)) {
		kcqe = &info->kcq[KCQ_PG(ri)][KCQ_IDX(ri)];
		cp->completed_kcq[kcqe_cnt++] = kcqe;
		i = info->next_idx(i);
		ri = i & MAX_KCQ_IDX;
		if (likely(!(kcqe->kcqe_op_flag & KCQE_FLAGS_NEXT))) {
			last_cnt = kcqe_cnt;
			last = i;
		}
	}

	info->sw_prod_idx = last;
	return last_cnt;
}

static u32 cnic_service_bnx2_queues(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	u32 status_idx = (u16) *cp->kcq1.status_idx_ptr;
	int kcqe_cnt;

	/* status block index must be read before reading other fields */
	rmb();
	cp->kwq_con_idx = *cp->kwq_con_idx_ptr;

	while ((kcqe_cnt = cnic_get_kcqes(dev, &cp->kcq1))) {

		service_kcqes(dev, kcqe_cnt);

		/* Tell compiler that status_blk fields can change. */
		barrier();
		status_idx = (u16) *cp->kcq1.status_idx_ptr;
		/* status block index must be read first */
		rmb();
		cp->kwq_con_idx = *cp->kwq_con_idx_ptr;
	}

	CNIC_WR16(dev, cp->kcq1.io_addr, cp->kcq1.sw_prod_idx);

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_handle_bnx2_ooo_rx_event(dev);
	cnic_handle_bnx2_ooo_tx_event(dev);
#endif
	return status_idx;
}

static int cnic_service_bnx2(void *data, void *status_blk)
{
	struct cnic_dev *dev = data;

	if (unlikely(!test_bit(CNIC_F_CNIC_UP, &dev->flags))) {
		struct status_block *sblk = status_blk;

		return sblk->status_idx;
	}

	return cnic_service_bnx2_queues(dev);
}

static void cnic_service_bnx2_msix(unsigned long data)
{
	struct cnic_dev *dev = (struct cnic_dev *) data;
	struct cnic_local *cp = dev->cnic_priv;

	cp->last_status_idx = cnic_service_bnx2_queues(dev);

	CNIC_WR(dev, BNX2_PCICFG_INT_ACK_CMD, cp->int_num |
		BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID | cp->last_status_idx);
}

static void cnic_doirq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	if (likely(test_bit(CNIC_F_CNIC_UP, &dev->flags))) {
		u16 prod = cp->kcq1.sw_prod_idx & MAX_KCQ_IDX;

		prefetch(cp->status_blk.gen);
		prefetch(&cp->kcq1.kcq[KCQ_PG(prod)][KCQ_IDX(prod)]);

		tasklet_schedule(&cp->cnic_irq_task);
	}
}

#if (LINUX_VERSION_CODE >= 0x20613) || defined(__VMKLNX__)
static irqreturn_t cnic_irq(int irq, void *dev_instance)
#else
static irqreturn_t cnic_irq(int irq, void *dev_instance, struct pt_regs *regs)
#endif
{
	struct cnic_dev *dev = dev_instance;
	struct cnic_local *cp = dev->cnic_priv;

	if (cp->ack_int)
		cp->ack_int(dev);

	cnic_doirq(dev);

	return IRQ_HANDLED;
}

static inline void cnic_ack_bnx2x_int(struct cnic_dev *dev, u8 id, u8 storm,
				      u16 index, u8 op, u8 update)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 hc_addr = (HC_REG_COMMAND_REG + BP_PORT(bp) * 32 +
		       COMMAND_REG_INT_ACK);
	struct igu_ack_register igu_ack;

	igu_ack.status_block_index = index;
	igu_ack.sb_id_and_flags =
			((id << IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT) |
			 (storm << IGU_ACK_REGISTER_STORM_ID_SHIFT) |
			 (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
			 (op << IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT));

	CNIC_WR(dev, hc_addr, (*(u32 *)&igu_ack));
}

#if (NEW_BNX2X_HSI >= 60)
static void cnic_ack_igu_sb(struct cnic_dev *dev, u8 igu_sb_id, u8 segment,
			    u16 index, u8 op, u8 update)
{
	struct igu_regular cmd_data;
	u32 igu_addr = BAR_IGU_INTMEM + (IGU_CMD_INT_ACK_BASE + igu_sb_id) * 8;

	cmd_data.sb_id_and_flags =
		(index 		<< IGU_REGULAR_SB_INDEX_SHIFT		)|
		(segment	<< IGU_REGULAR_SEGMENT_ACCESS_SHIFT	)|
		(update		<< IGU_REGULAR_BUPDATE_SHIFT		)|
		(op		<< IGU_REGULAR_ENABLE_INT_SHIFT		);


	CNIC_WR(dev, igu_addr, cmd_data.sb_id_and_flags);
}
#endif

static void cnic_ack_bnx2x_msix(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	cnic_ack_bnx2x_int(dev, cp->bnx2x_igu_sb_id, CSTORM_ID, 0,
			   IGU_INT_DISABLE, 0);
}

#if (NEW_BNX2X_HSI >= 60)
static void cnic_ack_bnx2x_e2_msix(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	cnic_ack_igu_sb(dev, cp->bnx2x_igu_sb_id, IGU_SEG_ACCESS_DEF, 0,
			IGU_INT_DISABLE, 0);
}
#endif

static u32 cnic_service_bnx2x_kcq(struct cnic_dev *dev, struct kcq_info *info)
{
	u32 last_status = *info->status_idx_ptr;
	int kcqe_cnt;

	/* status block index must be read before reading the KCQ */
	rmb();
	while ((kcqe_cnt = cnic_get_kcqes(dev, info))) {

		service_kcqes(dev, kcqe_cnt);

		/* Tell compiler that sblk fields can change. */
		barrier();

		last_status = *info->status_idx_ptr;
		/* status block index must be read before reading the KCQ */
		rmb();
	}
	return last_status;
}


static void cnic_service_bnx2x_bh(unsigned long data)
{
	struct cnic_dev *dev = (struct cnic_dev *) data;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct cnic_local *cp = dev->cnic_priv;
	u32 status_idx, new_status_idx;

	if (unlikely(!test_bit(CNIC_F_CNIC_UP, &dev->flags)))
		return;

	while (1) {
		status_idx = cnic_service_bnx2x_kcq(dev, &cp->kcq1);

		CNIC_WR16(dev, cp->kcq1.io_addr,
			  cp->kcq1.sw_prod_idx + MAX_KCQ_IDX);
#if (NEW_BNX2X_HSI < 60)
		cnic_ack_bnx2x_int(dev, cp->bnx2x_igu_sb_id, CSTORM_ID,
				   status_idx, IGU_INT_ENABLE, 1);
#endif

#if (CNIC_ISCSI_OOO_SUPPORT)
		cnic_handle_bnx2x_ooo_tx_event(dev);
		cnic_handle_bnx2x_ooo_rx_event(dev);
#endif

		if (!BNX2X_CHIP_IS_E2_PLUS(bp)) {
			cnic_ack_bnx2x_int(dev, cp->bnx2x_igu_sb_id, USTORM_ID,
					   status_idx, IGU_INT_ENABLE, 1);
			break;
		}
#if (NEW_BNX2X_HSI >= 60)
		new_status_idx = cnic_service_bnx2x_kcq(dev, &cp->kcq2);

		if (new_status_idx != status_idx)
			continue;

		CNIC_WR16(dev, cp->kcq2.io_addr, cp->kcq2.sw_prod_idx +
			  MAX_KCQ_IDX);

		cnic_ack_igu_sb(dev, cp->bnx2x_igu_sb_id, IGU_SEG_ACCESS_DEF,
				status_idx, IGU_INT_ENABLE, 1);
#endif
		break;
	}
}

static int cnic_service_bnx2x(void *data, void *status_blk)
{
	struct cnic_dev *dev = data;
	struct cnic_local *cp = dev->cnic_priv;

	if (!(cp->ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX))
		cnic_doirq(dev);

	return 0;
}

static void cnic_ulp_stop(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int if_type;

	for (if_type = 0; if_type < MAX_CNIC_ULP_TYPE; if_type++) {
		struct cnic_ulp_ops *ulp_ops;

		mutex_lock(&cnic_lock);
		ulp_ops = cp->ulp_ops[if_type];
		if (!ulp_ops) {
			mutex_unlock(&cnic_lock);
			continue;
		}
		set_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[if_type]);
		mutex_unlock(&cnic_lock);

		if (test_and_clear_bit(ULP_F_START, &cp->ulp_flags[if_type]))
			ulp_ops->cnic_stop(cp->ulp_handle[if_type]);

		clear_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[if_type]);
	}
}

static void cnic_ulp_start(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int if_type;

	for (if_type = 0; if_type < MAX_CNIC_ULP_TYPE; if_type++) {
		struct cnic_ulp_ops *ulp_ops;

		if (if_type == CNIC_ULP_ISCSI && !dev->max_iscsi_conn)
			continue;

		mutex_lock(&cnic_lock);
		ulp_ops = cp->ulp_ops[if_type];
		if (!ulp_ops || !ulp_ops->cnic_start) {
			mutex_unlock(&cnic_lock);
			continue;
		}
		set_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[if_type]);
		mutex_unlock(&cnic_lock);

		if (!test_and_set_bit(ULP_F_START, &cp->ulp_flags[if_type]))
			ulp_ops->cnic_start(cp->ulp_handle[if_type]);

		clear_bit(ULP_F_CALL_PENDING, &cp->ulp_flags[if_type]);
	}
}

static int cnic_copy_ulp_stats(struct cnic_dev *dev, int ulp_type)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_ulp_ops *ulp_ops;
	int rc;

	mutex_lock(&cnic_lock);
	ulp_ops = cnic_ulp_tbl_prot(ulp_type);
	if (ulp_ops && ulp_ops->cnic_get_stats)
		rc = ulp_ops->cnic_get_stats(cp->ulp_handle[ulp_type]);
	else
		rc = -ENODEV;
	mutex_unlock(&cnic_lock);
	return rc;
}

static int cnic_ctl(void *data, struct cnic_ctl_info *info)
{
	struct cnic_dev *dev = data;
	int ulp_type = CNIC_ULP_ISCSI;

	switch (info->cmd) {
	case CNIC_CTL_STOP_CMD:
		cnic_hold(dev);

		if (test_bit(CNIC_F_IF_UP, &dev->flags)) {
			clear_bit(CNIC_F_IF_UP, &dev->flags);
			cnic_ulp_stop(dev);
			cnic_stop_hw(dev);
		}

		cnic_put(dev);
		break;
	case CNIC_CTL_START_CMD:
		cnic_hold(dev);

		set_bit(CNIC_F_IF_UP, &dev->flags);
		if (!cnic_start_hw(dev))
			cnic_ulp_start(dev);

		cnic_put(dev);
		break;
	case CNIC_CTL_COMPLETION_CMD: {
		struct cnic_ctl_completion *comp = &info->data.comp;
		u32 cid = BNX2X_SW_CID(comp->cid);
		u32 l5_cid;
		struct cnic_local *cp = dev->cnic_priv;

		if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
			break;
		
		if (cnic_get_l5_cid(cp, cid, &l5_cid) == 0) {
			struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];

			if (unlikely(comp->error)) {
				set_bit(CTX_FL_CID_ERROR, &ctx->ctx_flags);
				printk(KERN_ERR PFX "%s: "
					"CID %x CFC delete comp error %x\n",
					dev->netdev->name, cid, comp->error);
			}

			ctx->wait_cond = 1;
			wake_up(&ctx->waitq);
		}
		break;
	}
	case CNIC_CTL_FCOE_STATS_GET_CMD:
		ulp_type = CNIC_ULP_FCOE;
		/* fall through */
	case CNIC_CTL_ISCSI_STATS_GET_CMD:
		cnic_hold(dev);
		cnic_copy_ulp_stats(dev, ulp_type);
		cnic_put(dev);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

#if !defined(__VMKLNX__)
static void cnic_ulp_init(struct cnic_dev *dev)
{
	int i;
	struct cnic_local *cp = dev->cnic_priv;

	for (i = 0; i < MAX_CNIC_ULP_TYPE_EXT; i++) {
		struct cnic_ulp_ops *ulp_ops;

		mutex_lock(&cnic_lock);
		ulp_ops = cnic_ulp_tbl[i];
		if (!ulp_ops || !ulp_ops->cnic_init) {
			mutex_unlock(&cnic_lock);
			continue;
		}
		ulp_get(ulp_ops);
		mutex_unlock(&cnic_lock);

		if (!test_and_set_bit(ULP_F_INIT, &cp->ulp_flags[i]))
			ulp_ops->cnic_init(dev);

		ulp_put(ulp_ops);
	}
}

static void cnic_ulp_exit(struct cnic_dev *dev)
{
	int i;
	struct cnic_local *cp = dev->cnic_priv;

	for (i = 0; i < MAX_CNIC_ULP_TYPE_EXT; i++) {
		struct cnic_ulp_ops *ulp_ops;

		mutex_lock(&cnic_lock);
		ulp_ops = cnic_ulp_tbl[i];
		if (!ulp_ops || !ulp_ops->cnic_exit) {
			mutex_unlock(&cnic_lock);
			continue;
		}
		ulp_get(ulp_ops);
		mutex_unlock(&cnic_lock);

		if (test_and_clear_bit(ULP_F_INIT, &cp->ulp_flags[i]))
			ulp_ops->cnic_exit(dev);

		ulp_put(ulp_ops);
	}
}
#endif /* !defined(__VMKLNX__) */

static int cnic_queue_work(struct cnic_local *cp, u32 work_type, void *data)
{
	struct cnic_work_node *node;
	int bytes = sizeof(u32 *);

	spin_lock_bh(&cp->wr_lock);

	node = &cp->cnic_work_ring[cp->cnic_wr_prod];
	node->work_type = work_type;
	if (work_type == WORK_TYPE_KCQE)
		bytes = sizeof(struct kcqe);
	if (work_type == WORK_TYPE_REDIRECT)
		bytes = sizeof(struct cnic_redirect_entry);
	memcpy(&node->work_data, data, bytes);
	cp->cnic_wr_prod++;
	cp->cnic_wr_prod &= WORK_RING_SIZE_MASK;

	spin_unlock_bh(&cp->wr_lock);
	return 0;
}

static int cnic_cm_offload_pg(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_offload_pg *l4kwqe;
	struct kwqe *wqes[1];
#if defined (__VMKLNX__)
	struct cnic_local *cp = dev->cnic_priv;
#else  /* !defined (__VMKLNX__) */
	struct neighbour *neigh = csk->dst->neighbour;
	struct net_device *netdev = neigh->dev;
#endif /* defined (__VMKLNX__) */

#ifndef HAVE_NETEVENT
#if !defined (__VMKLNX__)
	memcpy(csk->old_ha, &neigh->ha[0], 6);
#endif /* !defined (__VMKLNX__) */
#endif
	l4kwqe = (struct l4_kwq_offload_pg *) &csk->kwqe1;
	memset(l4kwqe, 0, sizeof(*l4kwqe));
	wqes[0] = (struct kwqe *) l4kwqe;

	l4kwqe->op_code = L4_KWQE_OPCODE_VALUE_OFFLOAD_PG;
	l4kwqe->flags =
		L4_LAYER_CODE << L4_KWQ_OFFLOAD_PG_LAYER_CODE_SHIFT;
	l4kwqe->l2hdr_nbytes = ETH_HLEN;
#if defined (__VMKLNX__)
	l4kwqe->da0 = csk->ha[0];
	l4kwqe->da1 = csk->ha[1];
	l4kwqe->da2 = csk->ha[2];
	l4kwqe->da3 = csk->ha[3];
	l4kwqe->da4 = csk->ha[4];
	l4kwqe->da5 = csk->ha[5];

	l4kwqe->sa0 = cp->srcMACAddr[0];
	l4kwqe->sa1 = cp->srcMACAddr[1];
	l4kwqe->sa2 = cp->srcMACAddr[2];
	l4kwqe->sa3 = cp->srcMACAddr[3];
	l4kwqe->sa4 = cp->srcMACAddr[4];
	l4kwqe->sa5 = cp->srcMACAddr[5];
#else  /* !defined (__VMKLNX__) */
	l4kwqe->da0 = neigh->ha[0];
	l4kwqe->da1 = neigh->ha[1];
	l4kwqe->da2 = neigh->ha[2];
	l4kwqe->da3 = neigh->ha[3];
	l4kwqe->da4 = neigh->ha[4];
	l4kwqe->da5 = neigh->ha[5];

	l4kwqe->sa0 = netdev->dev_addr[0];
	l4kwqe->sa1 = netdev->dev_addr[1];
	l4kwqe->sa2 = netdev->dev_addr[2];
	l4kwqe->sa3 = netdev->dev_addr[3];
	l4kwqe->sa4 = netdev->dev_addr[4];
	l4kwqe->sa5 = netdev->dev_addr[5];
#endif /* defined (__VMKLNX__) */

	l4kwqe->etype = ETH_P_IP;
        
	l4kwqe->ipid_start = DEF_IPID_START;
	l4kwqe->host_opaque = csk->l5_cid;

	if (csk->vlan_id) {
		l4kwqe->pg_flags |= L4_KWQ_OFFLOAD_PG_VLAN_TAGGING;
		l4kwqe->vlan_tag = csk->vlan_id;
		l4kwqe->l2hdr_nbytes += 4;
	}

	return dev->submit_kwqes(dev, wqes, 1);
}

#if !defined (__VMKLNX__)
static int cnic_cm_update_pg(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_update_pg *l4kwqe;
	struct kwqe *wqes[1];
	struct neighbour *neigh = csk->dst->neighbour;

#ifndef HAVE_NETEVENT
	memcpy(csk->old_ha, &neigh->ha[0], 6);
#endif
	l4kwqe = (struct l4_kwq_update_pg *) &csk->kwqe1;
	memset(l4kwqe, 0, sizeof(*l4kwqe));
	wqes[0] = (struct kwqe *) l4kwqe;

	l4kwqe->opcode = L4_KWQE_OPCODE_VALUE_UPDATE_PG;
	l4kwqe->flags =
		L4_LAYER_CODE << L4_KWQ_UPDATE_PG_LAYER_CODE_SHIFT;
	l4kwqe->pg_cid = csk->pg_cid;
	l4kwqe->da0 = neigh->ha[0];
	l4kwqe->da1 = neigh->ha[1];
	l4kwqe->da2 = neigh->ha[2];
	l4kwqe->da3 = neigh->ha[3];
	l4kwqe->da4 = neigh->ha[4];
	l4kwqe->da5 = neigh->ha[5];

	l4kwqe->pg_host_opaque = csk->l5_cid;
	l4kwqe->pg_valids = L4_KWQ_UPDATE_PG_VALIDS_DA;

	return dev->submit_kwqes(dev, wqes, 1);
}
#endif /* !defined (__VMKLNX__) */

static int cnic_cm_upload_pg(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_upload *l4kwqe;
	struct kwqe *wqes[1];

	l4kwqe = (struct l4_kwq_upload *) &csk->kwqe1;
	memset(l4kwqe, 0, sizeof(*l4kwqe));
	wqes[0] = (struct kwqe *) l4kwqe;

	l4kwqe->opcode = L4_KWQE_OPCODE_VALUE_UPLOAD_PG;
	l4kwqe->flags =
		L4_LAYER_CODE << L4_KWQ_UPLOAD_LAYER_CODE_SHIFT;
	l4kwqe->cid = csk->pg_cid;

	return dev->submit_kwqes(dev, wqes, 1);
}

#ifdef HAVE_NETEVENT
static void cnic_redirect(struct cnic_local *cp, struct dst_entry *new,
			  struct dst_entry *old)
{
	int i, found = 0;

	for (i = 0; i < MAX_CM_SK_TBL_SZ && !found; i++) {
		struct cnic_sock *csk;
		struct cnic_redirect_entry cnic_redir;

		csk = &cp->csk_tbl[i];
		csk_hold(csk);
		if (cnic_in_use(csk) && csk->dst == old) {
			found = 1;
			dst_hold(new);
			dst_hold(old);

			cnic_redir.old_dst = old;
			cnic_redir.new_dst = new;
			cnic_queue_work(cp, WORK_TYPE_REDIRECT, &cnic_redir);
			tasklet_schedule(&cp->cnic_task);
		}
		csk_put(csk);
	}
}

static void cnic_update_neigh(struct cnic_local *cp, struct neighbour *neigh)
{
	int i, found = 0;

	for (i = 0; i < MAX_CM_SK_TBL_SZ && !found; i++) {
		struct cnic_sock *csk;

		csk = &cp->csk_tbl[i];
		csk_hold(csk);
		if (cnic_in_use(csk) && csk->dst) {
			if (csk->dst->neighbour == neigh) {
				found = 1;
				neigh_hold(neigh);

				cnic_queue_work(cp, WORK_TYPE_NEIGH_UPDATE,
						&neigh);
				tasklet_schedule(&cp->cnic_task);
			}
		}
		csk_put(csk);
	}
}

static int cnic_net_callback(struct notifier_block *this, unsigned long event,
	void *ptr)
{
	struct cnic_local *cp = container_of(this, struct cnic_local, cm_nb);

	if (event == NETEVENT_NEIGH_UPDATE) {
		struct neighbour *neigh = ptr;

		cnic_update_neigh(cp, neigh);

	} else if (event == NETEVENT_REDIRECT) {
		struct netevent_redirect *netevent = ptr;
		struct dst_entry *old_dst = netevent->old;
		struct dst_entry *new_dst = netevent->new;

		cnic_redirect(cp, new_dst, old_dst);
	}
	return 0;
}
#endif

static int cnic_cm_conn_req(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_connect_req1 *l4kwqe1;
	struct l4_kwq_connect_req2 *l4kwqe2;
	struct l4_kwq_connect_req3 *l4kwqe3;
	struct kwqe *wqes[3];
	u8 tcp_flags = 0;
	int num_wqes = 2;

	l4kwqe1 = (struct l4_kwq_connect_req1 *) &csk->kwqe1;
	l4kwqe2 = (struct l4_kwq_connect_req2 *) &csk->kwqe2;
	l4kwqe3 = (struct l4_kwq_connect_req3 *) &csk->kwqe3;
	memset(l4kwqe1, 0, sizeof(*l4kwqe1));
	memset(l4kwqe2, 0, sizeof(*l4kwqe2));
	memset(l4kwqe3, 0, sizeof(*l4kwqe3));

	l4kwqe3->op_code = L4_KWQE_OPCODE_VALUE_CONNECT3;
	l4kwqe3->flags =
		L4_LAYER_CODE << L4_KWQ_CONNECT_REQ3_LAYER_CODE_SHIFT;
	l4kwqe3->ka_timeout = csk->ka_timeout;
	l4kwqe3->ka_interval = csk->ka_interval;
	l4kwqe3->ka_max_probe_count = csk->ka_max_probe_count;
	l4kwqe3->tos = csk->tos;
	l4kwqe3->ttl = csk->ttl;
	l4kwqe3->snd_seq_scale = csk->snd_seq_scale;
	l4kwqe3->pmtu = csk->pmtu;
	l4kwqe3->rcv_buf = csk->rcv_buf;
	l4kwqe3->snd_buf = csk->snd_buf;
	l4kwqe3->seed = csk->seed;

	wqes[0] = (struct kwqe *) l4kwqe1;
	if (test_bit(SK_F_IPV6, &csk->flags)) {
		wqes[1] = (struct kwqe *) l4kwqe2;
		wqes[2] = (struct kwqe *) l4kwqe3;
		num_wqes = 3;

		l4kwqe1->conn_flags = L4_KWQ_CONNECT_REQ1_IP_V6;
		l4kwqe2->op_code = L4_KWQE_OPCODE_VALUE_CONNECT2;
		l4kwqe2->flags =
			L4_KWQ_CONNECT_REQ2_LINKED_WITH_NEXT |
			L4_LAYER_CODE << L4_KWQ_CONNECT_REQ2_LAYER_CODE_SHIFT;
		l4kwqe2->src_ip_v6_2 = be32_to_cpu(csk->src_ip[1]);
		l4kwqe2->src_ip_v6_3 = be32_to_cpu(csk->src_ip[2]);
		l4kwqe2->src_ip_v6_4 = be32_to_cpu(csk->src_ip[3]);
		l4kwqe2->dst_ip_v6_2 = be32_to_cpu(csk->dst_ip[1]);
		l4kwqe2->dst_ip_v6_3 = be32_to_cpu(csk->dst_ip[2]);
		l4kwqe2->dst_ip_v6_4 = be32_to_cpu(csk->dst_ip[3]);
		l4kwqe3->mss = l4kwqe3->pmtu - sizeof(struct ipv6hdr) -
			       sizeof(struct tcphdr);
	} else {
		wqes[1] = (struct kwqe *) l4kwqe3;
		l4kwqe3->mss = l4kwqe3->pmtu - sizeof(struct iphdr) -
			       sizeof(struct tcphdr);
	}

	l4kwqe1->op_code = L4_KWQE_OPCODE_VALUE_CONNECT1;
	l4kwqe1->flags =
		(L4_LAYER_CODE << L4_KWQ_CONNECT_REQ1_LAYER_CODE_SHIFT) |
		 L4_KWQ_CONNECT_REQ3_LINKED_WITH_NEXT;
	l4kwqe1->cid = csk->cid;
	l4kwqe1->pg_cid = csk->pg_cid;
	l4kwqe1->src_ip = be32_to_cpu(csk->src_ip[0]);
	l4kwqe1->dst_ip = be32_to_cpu(csk->dst_ip[0]);
	l4kwqe1->src_port = be16_to_cpu(csk->src_port);
	l4kwqe1->dst_port = be16_to_cpu(csk->dst_port);
	if (csk->tcp_flags & SK_TCP_NO_DELAY_ACK)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_NO_DELAY_ACK;
	if (csk->tcp_flags & SK_TCP_KEEP_ALIVE)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_KEEP_ALIVE;
	if (csk->tcp_flags & SK_TCP_NAGLE)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_NAGLE_ENABLE;
	if (csk->tcp_flags & SK_TCP_TIMESTAMP)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_TIME_STAMP;
	if (csk->tcp_flags & SK_TCP_SACK)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_SACK;
	if (csk->tcp_flags & SK_TCP_SEG_SCALING)
		tcp_flags |= L4_KWQ_CONNECT_REQ1_SEG_SCALING;

	l4kwqe1->tcp_flags = tcp_flags;

	return dev->submit_kwqes(dev, wqes, num_wqes);
}

static int cnic_cm_close_req(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_close_req *l4kwqe;
	struct kwqe *wqes[1];

	l4kwqe = (struct l4_kwq_close_req *) &csk->kwqe2;
	memset(l4kwqe, 0, sizeof(*l4kwqe));
	wqes[0] = (struct kwqe *) l4kwqe;

	l4kwqe->op_code = L4_KWQE_OPCODE_VALUE_CLOSE;
	l4kwqe->flags = L4_LAYER_CODE << L4_KWQ_CLOSE_REQ_LAYER_CODE_SHIFT;
	l4kwqe->cid = csk->cid;

	return dev->submit_kwqes(dev, wqes, 1);
}

static int cnic_cm_abort_req(struct cnic_sock *csk)
{
	struct cnic_dev *dev = csk->dev;
	struct l4_kwq_reset_req *l4kwqe;
	struct kwqe *wqes[1];

	l4kwqe = (struct l4_kwq_reset_req *) &csk->kwqe2;
	memset(l4kwqe, 0, sizeof(*l4kwqe));
	wqes[0] = (struct kwqe *) l4kwqe;

	l4kwqe->op_code = L4_KWQE_OPCODE_VALUE_RESET;
	l4kwqe->flags = L4_LAYER_CODE << L4_KWQ_RESET_REQ_LAYER_CODE_SHIFT;
	l4kwqe->cid = csk->cid;

	return dev->submit_kwqes(dev, wqes, 1);
}

static int cnic_cm_create(struct cnic_dev *dev, int ulp_type, u32 cid,
			  u32 l5_cid, struct cnic_sock **csk, void *context)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_sock *csk1;

	if (l5_cid >= MAX_CM_SK_TBL_SZ)
		return -EINVAL;

	if (cp->ctx_tbl) {
		struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];

		if (test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags))
			return -EAGAIN;
	}

	csk1 = &cp->csk_tbl[l5_cid];
	if (atomic_read(&csk1->ref_count))
		return -EAGAIN;

	if (test_and_set_bit(SK_F_INUSE, &csk1->flags))
		return -EBUSY;

	csk1->dev = dev;
	csk1->cid = cid;
	csk1->l5_cid = l5_cid;
	csk1->ulp_type = ulp_type;
	csk1->context = context;

	csk1->ka_timeout = DEF_KA_TIMEOUT;
	csk1->ka_interval = DEF_KA_INTERVAL;
	csk1->ka_max_probe_count = DEF_KA_MAX_PROBE_COUNT;
	csk1->tos = DEF_TOS;
	csk1->ttl = DEF_TTL;
	csk1->snd_seq_scale = DEF_SND_SEQ_SCALE;
	csk1->rcv_buf = DEF_RCV_BUF;
	csk1->snd_buf = DEF_SND_BUF;
	csk1->seed = DEF_SEED;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_conn_ooo_init(cp, l5_cid);
#endif
	*csk = csk1;

	return 0;
}

static void cnic_cm_cleanup(struct cnic_sock *csk)
{
#if !defined (__VMKLNX__)
	if (csk->dst) {
		if (csk->dst->neighbour)
			neigh_release(csk->dst->neighbour);
		dst_release(csk->dst);
		csk->dst = NULL;
	}
	csk->src_port = 0;
#endif /* !defined (__VMKLNX__) */
	if (csk->src_port) {
		struct cnic_dev *dev = csk->dev;
		struct cnic_local *cp = dev->cnic_priv;

		cnic_free_id(&cp->csk_port_tbl, be16_to_cpu(csk->src_port));
		csk->src_port = 0;
	}
}

static void cnic_close_conn(struct cnic_sock *csk)
{
	if (test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags)) {
		cnic_cm_upload_pg(csk);
		clear_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags);
	}
	cnic_cm_cleanup(csk);
}

static int cnic_cm_destroy(struct cnic_sock *csk)
{
	if (!cnic_in_use(csk))
		return -EINVAL;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_flush_ooo(csk->dev, csk->l5_cid);
#endif
	csk_hold(csk);
	clear_bit(SK_F_INUSE, &csk->flags);
	smp_mb__after_clear_bit();
	while (atomic_read(&csk->ref_count) != 1)
		msleep(1);
	cnic_cm_cleanup(csk);

	csk->flags = 0;
	csk_put(csk);
	return 0;
}

static inline u16 cnic_get_vlan(struct net_device *dev,
				struct net_device **vlan_dev)
{
#if !defined (__VMKLNX__)
	if (dev->priv_flags & IFF_802_1Q_VLAN) {
#ifdef VLAN_DEV_INFO
		*vlan_dev = VLAN_DEV_INFO(dev)->real_dev;
		return VLAN_DEV_INFO(dev)->vlan_id;
#else
#ifdef VLAN_TX_COOKIE_MAGIC
		*vlan_dev = vlan_dev_info(dev)->real_dev;
		return vlan_dev_info(dev)->vlan_id;
#else
		*vlan_dev = vlan_dev_real_dev(dev);
		return vlan_dev_vlan_id(dev);
#endif
#endif
	}
#endif /* !defined (__VMKLNX__) */
	*vlan_dev = dev;
	return 0;
}

#if !defined (__VMKLNX__)
static int cnic_get_v4_route(struct sockaddr_in *dst_addr,
			     struct sockaddr_in *src_addr,
			     struct dst_entry **dst)
{
	struct flowi fl;
	int err;
	struct rtable *rt;

	memset(&fl, 0, sizeof(fl));
	fl.nl_u.ip4_u.daddr = dst_addr->sin_addr.s_addr;
	if (src_addr)
		fl.nl_u.ip4_u.saddr = src_addr->sin_addr.s_addr;

#if (LINUX_VERSION_CODE >= 0x020619)
	err = ip_route_output_key(&init_net, &rt, &fl);
#else
	err = ip_route_output_key(&rt, &fl);
#endif
	if (!err)
		*dst = &rt->u.dst;
	return err;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static struct dst_entry *cnic_ip6_rte_output(struct sock *sk, struct flowi *fl)
{
#if (LINUX_VERSION_CODE >= 0x02061a)
	struct dst_entry *(*fn)(struct net *, struct sock *, struct flowi *);
#else
	struct dst_entry *(*fn)(struct sock *, struct flowi *);
#endif
	struct dst_entry *dst = NULL;

	fn = symbol_get(ip6_route_output);
	if (fn) {
#if (LINUX_VERSION_CODE >= 0x02061a)
		dst = (*fn)(&init_net, sk, fl);
#else
		dst = (*fn)(sk, fl);
#endif
		symbol_put(ip6_route_output);
	}
	return dst;
}

static int cnic_ipv6_addr_type(const struct in6_addr *addr)
{
	int (*fn)(const struct in6_addr *addr);
	int type = 0;

	fn = symbol_get(__ipv6_addr_type);
	if (fn) {
		type = fn(addr) & 0xffff;
		symbol_put(__ipv6_addr_type);
	}
	return type;
}

static int cnic_ipv6_get_saddr(struct dst_entry *dst,
			       const struct in6_addr *daddr,
			       struct in6_addr *saddr)
{
	int rc = -ENOENT;

#if (LINUX_VERSION_CODE >= 0x02061b)
	int (*fn)(struct net *, struct net_device *,
		  const struct in6_addr *daddr, unsigned int prefs,
		  struct in6_addr *saddr);

	fn = symbol_get(ipv6_dev_get_saddr);
	if (fn) {
		rc = fn(&init_net, dst->dev, daddr, 0, saddr);
		symbol_put(ipv6_dev_get_saddr);
	}

#elif (LINUX_VERSION_CODE >= 0x02061a)
	int (*fn)(struct net_device *,
		  const struct in6_addr *daddr, unsigned int prefs,
		  struct in6_addr *saddr);

	fn = symbol_get(ipv6_dev_get_saddr);
	if (fn) {
		rc = fn(dst->dev, daddr, 0, saddr);
		symbol_put(ipv6_dev_get_saddr);
	}

#else
	int (*fn)(struct dst_entry *,
		  struct in6_addr *daddr, struct in6_addr *saddr);

	fn = symbol_get(ipv6_get_saddr);
	if (fn) {
		rc = fn(dst, (struct in6_addr *) daddr, saddr);
		symbol_put(ipv6_get_saddr);
	}
#endif
	return rc;
}

#endif

static int cnic_get_v6_route(struct sockaddr_in6 *dst_addr,
			     struct sockaddr_in6 *src_addr,
			     struct dst_entry **dst)
{
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	struct flowi fl;

	memset(&fl, 0, sizeof(fl));
	ipv6_addr_copy(&fl.fl6_dst, &dst_addr->sin6_addr);
	if (cnic_ipv6_addr_type(&fl.fl6_dst) & IPV6_ADDR_LINKLOCAL)
		fl.oif = dst_addr->sin6_scope_id;

	if (src_addr)
		ipv6_addr_copy(&fl.fl6_src, &src_addr->sin6_addr);

	*dst = cnic_ip6_rte_output(NULL, &fl);
	if (*dst)
		return 0;
#endif

	return -ENETUNREACH;
}
#endif /* !defined (__VMKLNX__) */

#if defined  (__VMKLNX__)
static struct cnic_dev *cnic_cm_select_dev(vmk_IscsiNetHandle iscsiNetHandle,
					   struct sockaddr_in *dst_addr,
					   int ulp_type)
{
	char devName[IFNAMSIZ];
	struct cnic_dev *dev;
	int found = 0;
	VMK_ReturnStatus status;

	status = vmk_IscsiTransportGetUplink(iscsiNetHandle, devName);
	if (status != VMK_OK) 
		return NULL;

	list_for_each_entry(dev, &cnic_dev_list, list) {
		struct cnic_local *cp = dev->cnic_priv;

		if (dev->netdev && test_bit(CNIC_F_IF_UP, &dev->flags)) {
			if (!strcmp(dev->netdev->name, devName)) {
				found = 1;
	                        /* Retrieve the source MAC */
	                        if ((status = vmk_IscsiTransportGetSrcMAC(
                                        iscsiNetHandle,
					cp->srcMACAddr)) != VMK_OK) {
					printk(KERN_ALERT "%s Get SRC MAC failed %d\n",
						dev->netdev->name, status);
					found = 0;
					break;
				}
				/* Next hop (arp) resolve */
				if ((status = vmk_IscsiTransportGetNextHopMAC(
					iscsiNetHandle, 
					cp->nextHopMACAddr)) != VMK_OK) {
					printk(KERN_ALERT "%s Get next hop MAC failed %d\n",
						dev->netdev->name, status);
					found = 0;
					break;
				}
				/* Retrieve the source IP Address */
				if ((status = vmk_IscsiTransportGetSrcIP(
					iscsiNetHandle,
					&cp->srcFamily, 
					cp->srcIPAddr)) != VMK_OK) {
					printk(KERN_ALERT "%s Get SRC IP failed %d\n",
						dev->netdev->name, status);
					found = 0;
					break;
				}
				/* Path MTU */
				if ((status = vmk_IscsiTransportGetPmtu(
					iscsiNetHandle,
					&cp->pmtu)) != VMK_OK) {
					printk(KERN_ALERT "%s Get PMTU failed %d\n",
						dev->netdev->name, status);
					cp->pmtu = 1500;
				}
				if (cp->pmtu > dev->netdev->mtu) {
					dev->pmtu_fails++;
					cp->pmtu = dev->netdev->mtu;
				} 
				/* VLAN Tag */
				if ((status = vmk_IscsiTransportGetVlan(
					iscsiNetHandle,
					&cp->vlan_id)) != VMK_OK) {
					printk(KERN_ALERT "%s Get vlan ID failed %d\n",
						dev->netdev->name, status);
					cp->vlan_id = 0;
				}


				/* Allocate TCP ports, only once per device */
				if (cp->csk_port_tbl.table)
					break;

				cp->cnic_local_port_nr = MAX_CM_SK_TBL_SZ;
				cp->cnic_local_port_min = 1;
				status = vmk_IscsiTransportGetPortReservation(
						iscsiNetHandle,
						&cp->cnic_local_port_nr,
						&cp->cnic_local_port_min);
				if (status == VMK_OK) {
					u32 port_id;

					get_random_bytes(&port_id,
							 sizeof(port_id));
					port_id %= cp->cnic_local_port_nr;

					cnic_init_id_tbl(&cp->csk_port_tbl, 
						cp->cnic_local_port_nr, 
						cp->cnic_local_port_min,
						port_id);
				} else {
					found = 0; 
					printk(KERN_ALERT "%s TCP port alloc failed %d\n",
						dev->netdev->name, status);
				}
				break;
			}
		}
	}

	if (!found)
		dev = NULL;
	return dev;
}

#else  /* !defined (__VMKLNX__) */
static struct cnic_dev *cnic_cm_select_dev(struct sockaddr_in *dst_addr,
					   int ulp_type)
{
	struct cnic_dev *dev = NULL;
	struct dst_entry *dst;
	struct net_device *netdev = NULL;
	int err = -ENETUNREACH;

	if (dst_addr->sin_family == AF_INET)
		err = cnic_get_v4_route(dst_addr, NULL, &dst);
	else if (dst_addr->sin_family == AF_INET6) {
		struct sockaddr_in6 *dst_addr6 =
			(struct sockaddr_in6 *) dst_addr;

		err = cnic_get_v6_route(dst_addr6, NULL, &dst);
	} else
		return NULL;

	if (err)
		return NULL;

	if (!dst->dev)
		goto done;

	cnic_get_vlan(dst->dev, &netdev);

	dev = cnic_from_netdev(netdev);

done:
	dst_release(dst);
	if (dev)
		cnic_put(dev);
	return dev;
}
#endif  /* defined (__VMKLNX__) */

#if !defined (__VMKLNX__)
static int cnic_resolve_addr(struct cnic_sock *csk)
{
	struct neighbour *neigh = csk->dst->neighbour;
	int err = 0;
#ifndef HAVE_NETEVENT
	int retry = 0;
#endif

	if (neigh->nud_state & NUD_VALID) {
		err = -EINVAL;
		if (cnic_offld_prep(csk))
			err = cnic_cm_offload_pg(csk);
		goto done;
	}

	set_bit(SK_F_NDISC_WAITING, &csk->flags);
	neigh_event_send(neigh, NULL);
#ifndef HAVE_NETEVENT
	while (!(neigh->nud_state & NUD_VALID) && (retry < 3)) {
		msleep(1000);
		retry++;
	}
	if (!(neigh->nud_state & NUD_VALID))
		err = -ETIMEDOUT;
	else {
		err = -EINVAL;
		if (cnic_offld_prep(csk))
			err = cnic_cm_offload_pg(csk);
	}
	clear_bit(SK_F_NDISC_WAITING, &csk->flags);
#endif
done:
	return err;
}

static int cnic_get_route(struct cnic_sock *csk, struct cnic_sockaddr *saddr)
{
	struct cnic_dev *dev = csk->dev;
	int is_v6, err;
	struct dst_entry *dst;
	struct net_device *realdev;

	if (saddr->local.v6.sin6_family == AF_INET6 &&
	    saddr->remote.v6.sin6_family == AF_INET6)
		is_v6 = 1;
	else if (saddr->local.v4.sin_family == AF_INET &&
		 saddr->remote.v4.sin_family == AF_INET)
		is_v6 = 0;
	else
		return -EINVAL;

	clear_bit(SK_F_IPV6, &csk->flags);

	if (is_v6) {
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
		set_bit(SK_F_IPV6, &csk->flags);
		err = cnic_get_v6_route(&saddr->remote.v6,
					&saddr->local.v6, &dst);
		if (err)
			return err;

		if (!dst || dst->error || !dst->dev)
			goto err_out;

		cnic_ipv6_get_saddr(dst, &saddr->remote.v6.sin6_addr,
				    &saddr->local.v6.sin6_addr);

		memcpy(&csk->src_ip[0], &saddr->local.v6.sin6_addr,
		       sizeof(struct in6_addr));
		memcpy(&csk->dst_ip[0], &saddr->remote.v6.sin6_addr,
		       sizeof(struct in6_addr));
		csk->src_port = saddr->local.v6.sin6_port;
		csk->dst_port = saddr->remote.v6.sin6_port;
#else
		return -ENETUNREACH;
#endif

	} else {
		err = cnic_get_v4_route(&saddr->remote.v4, &saddr->local.v4,
					&dst);
		if (err)
			return err;

		if (!dst || dst->error || !dst->dev)
			goto err_out;

		csk->dst_ip[0] = saddr->remote.v4.sin_addr.s_addr;
		csk->src_ip[0] = saddr->local.v4.sin_addr.s_addr;
		csk->src_port = saddr->local.v4.sin_port;
		csk->dst_port = saddr->remote.v4.sin_port;

		if (csk->src_ip[0] == 0) {
			csk->src_ip[0] =
				inet_select_addr(dst->dev, csk->dst_ip[0],
						 RT_SCOPE_LINK);
		}
	}

	csk->vlan_id = cnic_get_vlan(dst->dev, &realdev);
	if (realdev != dev->netdev)
		goto err_out;

	csk->dst = dst;
	csk->pmtu = dst_mtu(csk->dst);
	return 0;

err_out:
	dst_release(dst);
	return -ENETUNREACH;
}
#else /* defined (__VMKLNX__) */

static int cnic_get_route(struct cnic_sock *csk, struct cnic_sockaddr *saddr)
{
	struct cnic_dev *dev = csk->dev;
	struct cnic_local *cp = dev->cnic_priv;
	int is_v6, err;
	int local_port;

	if (saddr->local.v6.sin6_family == AF_INET6 &&
	    saddr->remote.v6.sin6_family == AF_INET6)
		is_v6 = 1;
	else if (saddr->local.v4.sin_family == AF_INET &&
		 saddr->remote.v4.sin_family == AF_INET)
		is_v6 = 0;
	else
		return -EINVAL;

	/* Use stored next hop MAC address */
	if (!cp->nextHopMACAddr[0] && !cp->nextHopMACAddr[1] &&
		!cp->nextHopMACAddr[2]) {
		printk(KERN_ALERT "Zero next hop address, aborting\n");
		return -EINVAL;
	}

	if (!cnic_offld_prep(csk))
		return -EINVAL;

	clear_bit(SK_F_IPV6, &csk->flags);
	memcpy(csk->ha, cp->nextHopMACAddr, 6);
	local_port = cnic_alloc_new_id(&cp->csk_port_tbl);
	if (local_port == -1) {
		clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
		return -ENOMEM;
	}
	csk->pmtu = cp->pmtu;
	csk->vlan_id = cp->vlan_id;

	if (is_v6) {
		set_bit(SK_F_IPV6, &csk->flags);
		memcpy((u8 *)csk->src_ip, cp->srcIPAddr, 16);
		memcpy((u8 *)csk->dst_ip, saddr->remote.v6.sin6_addr.s6_addr, 16);
		csk->src_port = htons(local_port);
		csk->dst_port = saddr->remote.v6.sin6_port;
	} else {
		csk->src_ip[0] = *((vmk_uint32 *)cp->srcIPAddr);
		csk->dst_ip[0] = saddr->remote.v4.sin_addr.s_addr;
		csk->src_port = htons(local_port);
		csk->dst_port = saddr->remote.v4.sin_port;
	}
	err = cnic_cm_offload_pg(csk);

	return err;
}
#endif /* !defined (__VMKLNX__) */

static void cnic_init_csk_state(struct cnic_sock *csk)
{
	csk->state = 0;
	clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
	clear_bit(SK_F_CLOSING, &csk->flags);
}

static int cnic_cm_connect(struct cnic_sock *csk, struct cnic_sockaddr *saddr)
{
#if !defined (__VMKLNX__)
	struct neighbour *neigh;
#endif /* !defined (__VMKLNX__) */
	int err = 0;

	if (!cnic_in_use(csk))
		return -EINVAL;

	if (test_and_set_bit(SK_F_CONNECT_START, &csk->flags))
		return -EINVAL;

	cnic_init_csk_state(csk);

	err = cnic_get_route(csk, saddr);
	if (err)
		goto err_out;

#if !defined (__VMKLNX__)
	neigh = csk->dst->neighbour;
	if (!neigh)
		goto err_out;

	neigh_hold(neigh);

	err = cnic_resolve_addr(csk);
	if (!err)
		return 0;

	neigh_release(neigh);

#endif /* !defined (__VMKLNX__) */
err_out:
#if !defined (__VMKLNX__)
	if (csk->dst) {
		dst_release(csk->dst);
		csk->dst = NULL;
	}
#endif /* !defined (__VMKLNX__) */
	clear_bit(SK_F_CONNECT_START, &csk->flags);
	return err;
}

static int cnic_cm_abort(struct cnic_sock *csk)
{
	struct cnic_local *cp = csk->dev->cnic_priv;
	u32 opcode = L4_KCQE_OPCODE_VALUE_RESET_COMP;

	if (!cnic_in_use(csk))
		return -EINVAL;

	if (cnic_abort_prep(csk))
		return cnic_cm_abort_req(csk);

	/* Getting here means that we haven't started connect, or
	 * connect was not successful, or it has been reset by the target.
	 */

	cp->close_conn(csk, opcode);
	if (csk->state != opcode) {
		/* Wait for remote reset sequence to complete */
		while (test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags))
			msleep(1);

		return -EALREADY;
	}
	return 0;
}

static int cnic_cm_close(struct cnic_sock *csk)
{
	if (!cnic_in_use(csk))
		return -EINVAL;

	if (cnic_close_prep(csk)) {
		csk->state = L4_KCQE_OPCODE_VALUE_CLOSE_COMP;
		return cnic_cm_close_req(csk);
	} else {
		/* Wait for remote reset sequence to complete */
		while (test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags))
			msleep(1);

		return -EALREADY;
	}
	return 0;
}

static void cnic_cm_upcall(struct cnic_local *cp, struct cnic_sock *csk,
			   u8 opcode)
{
	struct cnic_ulp_ops *ulp_ops;
	int ulp_type = csk->ulp_type;

	rcu_read_lock();
	ulp_ops = rcu_dereference(cp->ulp_ops[ulp_type]);
	if (ulp_ops) {
		if (opcode == L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE)
			ulp_ops->cm_connect_complete(csk);
		else if (opcode == L4_KCQE_OPCODE_VALUE_CLOSE_COMP)
			ulp_ops->cm_close_complete(csk);
		else if (opcode == L4_KCQE_OPCODE_VALUE_RESET_RECEIVED)
			ulp_ops->cm_remote_abort(csk);
		else if (opcode == L4_KCQE_OPCODE_VALUE_RESET_COMP)
			ulp_ops->cm_abort_complete(csk);
		else if (opcode == L4_KCQE_OPCODE_VALUE_CLOSE_RECEIVED)
			ulp_ops->cm_remote_close(csk);
	}
	rcu_read_unlock();
}

#if !defined (__VMKLNX__)
static int cnic_cm_set_pg(struct cnic_sock *csk)
{
	struct neighbour *neigh = csk->dst->neighbour;
	int valid = neigh->nud_state & NUD_VALID;

	if (!valid) {
		if (test_and_clear_bit(SK_F_NDISC_WAITING, &csk->flags)) {
			clear_bit(SK_F_CONNECT_START, &csk->flags);
			cnic_cm_cleanup(csk);
			return -ETIMEDOUT;
		}
	}

	if (cnic_offld_prep(csk)) {
		if (test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags))
			cnic_cm_update_pg(csk);
		else
			cnic_cm_offload_pg(csk);
	}
	clear_bit(SK_F_NDISC_WAITING, &csk->flags);
	return 0;
}
#endif /* !defined (__VMKLNX__) */

static void cnic_cm_process_neigh(struct cnic_dev *dev, struct neighbour *neigh)
{
#if !defined (__VMKLNX__)
	struct cnic_local *cp = dev->cnic_priv;
	int i;

	for (i = 0; i < MAX_CM_SK_TBL_SZ; i++) {
		struct cnic_sock *csk;
		int abort = 0;

		csk = &cp->csk_tbl[i];
		csk_hold(csk);
		if (cnic_in_use(csk) && csk->dst &&
		    csk->dst->neighbour == neigh) {
			if (cnic_cm_set_pg(csk))
				abort = 1;
		}
		if (abort)
			cnic_cm_upcall(cp, csk,
				L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE);
		csk_put(csk);
	}
	neigh_release(neigh);
#endif /* !defined (__VMKLNX__) */
}

static void cnic_cm_process_redirect(struct cnic_dev *dev,
				     struct cnic_redirect_entry *redir)
{
#if !defined (__VMKLNX__)
	struct cnic_local *cp = dev->cnic_priv;
	int i;

	for (i = 0; i < MAX_CM_SK_TBL_SZ; i++) {
		struct cnic_sock *csk;
		int abort = 0;

		csk = &cp->csk_tbl[i];
		csk_hold(csk);
		if (cnic_in_use(csk) && csk->dst == redir->old_dst) {
			csk->dst = redir->new_dst;
			dst_hold(csk->dst);
			neigh_hold(csk->dst->neighbour);
			if (redir->old_dst->neighbour);
				neigh_release(redir->old_dst->neighbour);
			dst_release(redir->old_dst);
			if (cnic_cm_set_pg(csk))
				abort = 1;
		}
		if (abort)
			cnic_cm_upcall(cp, csk,
				L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE);
		csk_put(csk);
	}

	dst_release(redir->new_dst);
	dst_release(redir->old_dst);
#endif /* !defined (__VMKLNX__) */
}

static void cnic_cm_process_offld_pg(struct cnic_dev *dev, struct l4_kcq *kcqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	u32 l5_cid = kcqe->pg_host_opaque;
	u8 opcode = kcqe->op_code;
	struct cnic_sock *csk = &cp->csk_tbl[l5_cid];

	csk_hold(csk);
	if (!cnic_in_use(csk))
		goto done;

	if (opcode == L4_KCQE_OPCODE_VALUE_UPDATE_PG) {
		clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
		goto done;
	}
	/* Possible PG kcqe status:  SUCCESS, OFFLOADED_PG, or CTX_ALLOC_FAIL */
	if (kcqe->status == L4_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAIL) {
		clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
		cnic_cm_upcall(cp, csk,
			       L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE);
		goto done;
	}

	csk->pg_cid = kcqe->pg_cid;
	set_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags);
	cnic_cm_conn_req(csk);

done:
	csk_put(csk);
}

static void cnic_process_fcoe_term_conn(struct cnic_dev *dev, struct kcqe *kcqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct fcoe_kcqe *fc_kcqe = (struct fcoe_kcqe *) kcqe;
	u32 l5_cid = fc_kcqe->fcoe_conn_id + BNX2X_FCOE_L5_CID_BASE;
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];

	ctx->timestamp = jiffies;
	ctx->wait_cond = 1;
	wake_up(&ctx->waitq);
}

static void cnic_cm_process_kcqe(struct cnic_dev *dev, struct kcqe *kcqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct l4_kcq *l4kcqe = (struct l4_kcq *) kcqe;
	u8 opcode = l4kcqe->op_code;
	u32 l5_cid;
	struct cnic_sock *csk;

	switch (opcode) {
	case FCOE_RAMROD_CMD_ID_TERMINATE_CONN:
		cnic_process_fcoe_term_conn(dev, kcqe);
		return;
	case L4_KCQE_OPCODE_VALUE_OFFLOAD_PG:
	case L4_KCQE_OPCODE_VALUE_UPDATE_PG:
		cnic_cm_process_offld_pg(dev, l4kcqe);
		return;
#if (CNIC_ISCSI_OOO_SUPPORT)
	case L4_KCQE_OPCODE_VALUE_OOO_FLUSH:
		cnic_flush_ooo(dev, l4kcqe->cid);
		return;

	case L4_KCQE_OPCODE_VALUE_OOO_EVENT_NOTIFICATION:
		set_bit(IOOO_START_HANDLER, &cp->iooo_mgmr.flags);
		return;
#endif
	}

	l5_cid = l4kcqe->conn_id;
	/* Hack */
	if (opcode & 0x80)
		l5_cid = l4kcqe->cid;
	if (l5_cid >= MAX_CM_SK_TBL_SZ)
		return;

	csk = &cp->csk_tbl[l5_cid];
	csk_hold(csk);

	if (!cnic_in_use(csk)) {
		csk_put(csk);
		return;
	}

	switch (opcode) {
	case L5CM_RAMROD_CMD_ID_TCP_CONNECT:
		if (l4kcqe->status != 0) {
			clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
			cnic_cm_upcall(cp, csk,
				       L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE);
		}
		break;
	case L4_KCQE_OPCODE_VALUE_CONNECT_COMPLETE:
		if (l4kcqe->status == 0) {
			if (test_bit(CNIC_F_BNX2X_CLASS, &dev->flags)) {
				struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
				set_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags);
			}
                        set_bit(SK_F_OFFLD_COMPLETE, &csk->flags);
                } else {
			printk(KERN_WARNING PFX "%s: Connect completion "
						"failed: "
						"status: 0x%x cid: 0x%x "
						"l5_cid: 0x%x",
						dev->netdev->name,
						l4kcqe->status, l4kcqe->cid,
						l5_cid);
			if (l4kcqe->status == L4_KCQE_COMPLETION_STATUS_PARITY_ERROR)
				set_bit(SK_F_HW_ERR, &csk->flags);
		}

		smp_mb__before_clear_bit();
		clear_bit(SK_F_OFFLD_SCHED, &csk->flags);
		cnic_cm_upcall(cp, csk, opcode);
		break;
	case L5CM_RAMROD_CMD_ID_CLOSE: {
		struct iscsi_kcqe *l5kcqe = (struct iscsi_kcqe *)kcqe;

		if (l4kcqe->status != 0 || l5kcqe->completion_status != 0) {
			netdev_warn(dev->netdev, "RAMROD CLOSE compl with status 0x%x completion status 0x%x\n",
				    l4kcqe->status, l5kcqe->completion_status);
			opcode = L4_KCQE_OPCODE_VALUE_CLOSE_COMP;
			/* Fall through */
		} else {
			break;
		}
	}
	case L4_KCQE_OPCODE_VALUE_RESET_RECEIVED:
	case L4_KCQE_OPCODE_VALUE_CLOSE_COMP:
	case L4_KCQE_OPCODE_VALUE_RESET_COMP:
	case L5CM_RAMROD_CMD_ID_SEARCHER_DELETE:
	case L5CM_RAMROD_CMD_ID_TERMINATE_OFFLOAD:
		if (l4kcqe->status == L4_KCQE_COMPLETION_STATUS_PARITY_ERROR)
			set_bit(SK_F_HW_ERR, &csk->flags);
		
		cp->close_conn(csk, opcode);
		break;

	case L4_KCQE_OPCODE_VALUE_CLOSE_RECEIVED:
		/* after we already sent CLOSE_REQ */
		if (test_bit(CNIC_F_BNX2X_CLASS, &dev->flags) &&
		    !test_bit(SK_F_OFFLD_COMPLETE, &csk->flags) &&
		    csk->state == L4_KCQE_OPCODE_VALUE_CLOSE_COMP)
			cp->close_conn(csk, L4_KCQE_OPCODE_VALUE_RESET_COMP);
		else
			cnic_cm_upcall(cp, csk, opcode);
		break;
	}
	csk_put(csk);
}

static void cnic_cm_indicate_kcqe(void *data, struct kcqe *kcqe[], u32 num_cqe)
{
	struct cnic_dev *dev = data;
	int i;
	struct cnic_local *cp = dev->cnic_priv;

	for (i = 0; i < num_cqe; i++)
		cnic_queue_work(cp, WORK_TYPE_KCQE, kcqe[i]);

	tasklet_schedule(&cp->cnic_task);
}

static void cnic_cm_indicate_event(void *data, unsigned long event)
{
}

static void cnic_cm_dummy(void *data)
{
}

static struct cnic_ulp_ops cm_ulp_ops = {
	.cnic_start		= cnic_cm_dummy,
	.cnic_stop		= cnic_cm_dummy,
	.indicate_kcqes		= cnic_cm_indicate_kcqe,
	.indicate_netevent	= cnic_cm_indicate_event,
	.indicate_inetevent	= cnic_cm_indicate_event,
};

static void cnic_task(unsigned long data)
{
	struct cnic_local *cp = (struct cnic_local *) data;
	struct cnic_dev *dev = cp->dev;
	u32 cons = cp->cnic_wr_cons;
	u32 prod = cp->cnic_wr_prod;

	while (cons != prod) {
		struct cnic_work_node *node;

		node = &cp->cnic_work_ring[cons];
		if (node->work_type == WORK_TYPE_KCQE)
			cnic_cm_process_kcqe(dev, &node->work_data.kcqe);
		else if (node->work_type == WORK_TYPE_NEIGH_UPDATE)
			cnic_cm_process_neigh(dev, node->work_data.neigh);
		else if (node->work_type == WORK_TYPE_REDIRECT)
			cnic_cm_process_redirect(dev,
				&node->work_data.cnic_redir);
		cons++;
		cons &= WORK_RING_SIZE_MASK;
	}
	cp->cnic_wr_cons = cons;
}

static void cnic_free_dev(struct cnic_dev *dev)
{
	int i = 0;

	while ((atomic_read(&dev->ref_count) != 0) && i < 10) {
		msleep(100);
		i++;
	}
	if (atomic_read(&dev->ref_count) != 0)
		CNIC_ERR("Failed waiting for ref count to goto zero.\n");

	printk(KERN_INFO PFX "Removed CNIC device: %s\n", dev->netdev->name);
	dev_put(dev->netdev);
	kfree(dev);
}

static void cnic_cm_free_mem(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	kfree(cp->csk_tbl);
	cp->csk_tbl = NULL;
	cp->next_tcp_port = cnic_free_id_tbl(&cp->csk_port_tbl);
}

static int cnic_cm_alloc_mem(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	cp->csk_tbl = kzalloc(sizeof(struct cnic_sock) * MAX_CM_SK_TBL_SZ,
			      GFP_KERNEL);
	if (!cp->csk_tbl)
		return -ENOMEM;

	return 0;
}

#ifndef HAVE_NETEVENT
#if !defined (__VMKLNX__)
static void cnic_timer(unsigned long data)
{
	struct cnic_local *cp = (struct cnic_local *) data;
	struct cnic_dev *dev = cp->dev;
	int i, found = 0;
	struct neighbour *neigh = NULL;

	if (!test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return;

	for (i = 0; i < MAX_CM_SK_TBL_SZ && !found; i++) {
		struct cnic_sock *csk;

		csk = &cp->csk_tbl[i];
		csk_hold(csk);
		if (cnic_in_use(csk) && csk->dst &&
		    test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags)) {
			neigh = csk->dst->neighbour;
			if (memcmp(csk->old_ha, neigh->ha, 6)) {
				found = 1;
				neigh_hold(neigh);

				cnic_queue_work(cp, WORK_TYPE_NEIGH_UPDATE,
						&neigh);
				tasklet_schedule(&cp->cnic_task);
			}
		}
		csk_put(csk);
	}

	cp->cnic_timer.expires = jiffies + cp->cnic_timer_off;
	add_timer(&cp->cnic_timer);
}
#endif /* !defined (__VMKLNX__) */
#endif

static int cnic_ready_to_close(struct cnic_sock *csk, u32 opcode)
{
	if (test_and_clear_bit(SK_F_OFFLD_COMPLETE, &csk->flags)) {
		/* Unsolicited RESET_COMP or RESET_RECEIVED */
		opcode = L4_KCQE_OPCODE_VALUE_RESET_RECEIVED;
		csk->state = opcode;
	}

	/* 1. If event opcode matches the expected event in csk->state
	 * 2. If the expected event is CLOSE_COMP or RESET_COMP, we accept any
	 *    event
	 * 3. If the expected event is 0, meaning the connection was never
	 *    never established, we accept the opcode from cm_abort.
	 */
	if (opcode == csk->state || csk->state == 0 ||
	    csk->state == L4_KCQE_OPCODE_VALUE_CLOSE_COMP ||
	    csk->state == L4_KCQE_OPCODE_VALUE_RESET_COMP) {
		if (!test_and_set_bit(SK_F_CLOSING, &csk->flags)) {
			if (csk->state == 0)
				csk->state = opcode;
			return 1;
		}
	}
	return 0;
}

static void cnic_close_bnx2_conn(struct cnic_sock *csk, u32 opcode)
{
	struct cnic_dev *dev = csk->dev;
	struct cnic_local *cp = dev->cnic_priv;

	clear_bit(SK_F_CONNECT_START, &csk->flags);
	cnic_close_conn(csk);
	cnic_cm_upcall(cp, csk, opcode);
}

static void cnic_cm_stop_bnx2_hw(struct cnic_dev *dev)
{
}

static int cnic_cm_init_bnx2_hw(struct cnic_dev *dev)
{
	u32 seed;

#if (LINUX_VERSION_CODE >= 0x020612)
	get_random_bytes(&seed, 4);
#else
	seed = 0x12345678;
#endif
	cnic_ctx_wr(dev, 45, 0, seed);
	return 0;
}

static void cnic_close_bnx2x_conn(struct cnic_sock *csk, u32 opcode)
{
	struct cnic_dev *dev = csk->dev;
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx = &cp->ctx_tbl[csk->l5_cid];
	union l5cm_specific_data l5_data;
	u32 cmd = 0;
	int close_complete = 0;

	switch (opcode) {
	case L4_KCQE_OPCODE_VALUE_RESET_RECEIVED:
	case L4_KCQE_OPCODE_VALUE_CLOSE_COMP:
	case L4_KCQE_OPCODE_VALUE_RESET_COMP:
		if (cnic_ready_to_close(csk, opcode)) {
			if (test_bit(SK_F_HW_ERR, &csk->flags))
				close_complete = 1;
			else if (test_bit(SK_F_PG_OFFLD_COMPLETE, &csk->flags))
				cmd = L5CM_RAMROD_CMD_ID_SEARCHER_DELETE;
			else
				close_complete = 1;
		}
		break;
	case L5CM_RAMROD_CMD_ID_SEARCHER_DELETE:
		cmd = L5CM_RAMROD_CMD_ID_TERMINATE_OFFLOAD;
		break;
	case L5CM_RAMROD_CMD_ID_TERMINATE_OFFLOAD:
		close_complete = 1;
		break;
	}
	if (cmd) {
		memset(&l5_data, 0, sizeof(l5_data));

		cnic_submit_kwqe_16(dev, cmd, csk->cid, ISCSI_CONNECTION_TYPE,
				    &l5_data);
	} else if (close_complete) {
		ctx->timestamp = jiffies;
		cnic_close_conn(csk);
		cnic_cm_upcall(cp, csk, csk->state);
	}
}

static void cnic_cm_stop_bnx2x_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	if (!cp->ctx_tbl)
		return;

#if !defined (__VMKLNX__)
	if (!netif_running(dev->netdev))
		return;
#endif
	cnic_bnx2x_delete_wait(dev, 0);

	cancel_delayed_work(&cp->delete_task);
	flush_workqueue(cnic_wq);

	if (atomic_read(&cp->iscsi_conn) != 0)
		printk(KERN_WARNING PFX "%s: %d iSCSI connections not destroyed\n", dev->netdev->name, atomic_read(&cp->iscsi_conn));
}

static int cnic_cm_init_bnx2x_hw(struct cnic_dev *dev)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 pfid = bp->pfid;
	u32 port = BP_PORT(bp);
	struct net_device *netdev = dev->netdev;

	cnic_init_bnx2x_mac(dev, netdev->dev_addr);
	cnic_bnx2x_set_tcp_options(dev, 0, 1);

	CNIC_WR16(dev, BAR_XSTRORM_INTMEM +
		  XSTORM_ISCSI_LOCAL_VLAN_OFFSET(pfid), 0);

	CNIC_WR(dev, BAR_XSTRORM_INTMEM +
		XSTORM_TCP_GLOBAL_DEL_ACK_COUNTER_ENABLED_OFFSET(port), 1);
	CNIC_WR(dev, BAR_XSTRORM_INTMEM +
		XSTORM_TCP_GLOBAL_DEL_ACK_COUNTER_MAX_COUNT_OFFSET(port),
		DEF_MAX_DA_COUNT);

	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_TCP_VARS_TTL_OFFSET(pfid), DEF_TTL);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_TCP_VARS_TOS_OFFSET(pfid), DEF_TOS);
	CNIC_WR8(dev, BAR_XSTRORM_INTMEM +
		 XSTORM_ISCSI_TCP_VARS_ADV_WND_SCL_OFFSET(pfid), 0);
	CNIC_WR(dev, BAR_XSTRORM_INTMEM +
		XSTORM_TCP_TX_SWS_TIMER_VAL_OFFSET(pfid), DEF_SWS_TIMER);

	CNIC_WR(dev, BAR_TSTRORM_INTMEM + TSTORM_TCP_MAX_CWND_OFFSET(pfid),
		DEF_MAX_CWND);
	return 0;
}

#ifdef INIT_DELAYED_WORK
static void cnic_delete_task(struct work_struct *work)
#else
static void cnic_delete_task(void *data)
#endif
{
	struct cnic_local *cp;
	struct cnic_dev *dev;
	u32 i;
	int need_resched = 0;

#ifdef INIT_DELAYED_WORK
	cp = container_of(work, struct cnic_local, delete_task.work);
#else
	cp = (struct cnic_local *) data;
#endif
	dev = cp->dev;

	for (i = 0; i < cp->max_cid_space; i++) {
		struct cnic_context *ctx = &cp->ctx_tbl[i];
		int err;

		if (!test_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags) ||
		    !test_bit(CTX_FL_DELETE_WAIT, &ctx->ctx_flags))
			continue;

		if (!time_after(jiffies, ctx->timestamp + (2 * HZ))) {
			need_resched = 1;
			continue;
		}

		if (!test_and_clear_bit(CTX_FL_DELETE_WAIT, &ctx->ctx_flags))
			continue;

		err = cnic_bnx2x_destroy_ramrod(dev, i);

		cnic_free_bnx2x_conn_resc(dev, i);
		if (!err) {
			if (ctx->ulp_proto_id == CNIC_ULP_ISCSI)
				atomic_dec(&cp->iscsi_conn);

			clear_bit(CTX_FL_OFFLD_START, &ctx->ctx_flags);
		}
	}

	if (need_resched)
		queue_delayed_work(cnic_wq, &cp->delete_task,
				   msecs_to_jiffies(10));
}

static int cnic_cm_open(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int err;

	err = cnic_cm_alloc_mem(dev);
	if (err)
		return err;

	err = cp->start_cm(dev);

	if (err)
		goto err_out;

	spin_lock_init(&cp->wr_lock);

	tasklet_init(&cp->cnic_task, cnic_task, (unsigned long) cp);

#ifdef INIT_DELAYED_WORK
	INIT_DELAYED_WORK(&cp->delete_task, cnic_delete_task);
#else
	INIT_WORK(&cp->delete_task, cnic_delete_task, cp);
#endif

#ifdef HAVE_NETEVENT
	cp->cm_nb.notifier_call = cnic_net_callback;
	register_netevent_notifier(&cp->cm_nb);
#else
#if !defined (__VMKLNX__)
	init_timer(&cp->cnic_timer);
	cp->cnic_timer_off = 2 * HZ;
	cp->cnic_timer.expires = jiffies + cp->cnic_timer_off;
	cp->cnic_timer.data = (unsigned long) cp;
	cp->cnic_timer.function = cnic_timer;
	add_timer(&cp->cnic_timer);
#endif /* !defined (__VMKLNX__) */
#endif

	dev->cm_create = cnic_cm_create;
	dev->cm_destroy = cnic_cm_destroy;
	dev->cm_connect = cnic_cm_connect;
	dev->cm_abort = cnic_cm_abort;
	dev->cm_close = cnic_cm_close;
	dev->cm_select_dev = cnic_cm_select_dev;

	cp->ulp_handle[CNIC_ULP_L4] = dev;
	rcu_assign_pointer(cp->ulp_ops[CNIC_ULP_L4], &cm_ulp_ops);
	return 0;

err_out:
	cnic_cm_free_mem(dev);
	return err;
}

static int cnic_cm_shutdown(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int i;

#ifdef HAVE_NETEVENT
	unregister_netevent_notifier(&cp->cm_nb);
#else
#if !defined (__VMKLNX__)
	del_timer_sync(&cp->cnic_timer);
#endif /* !defined (__VMKLNX__) */
#endif

	tasklet_kill(&cp->cnic_task);

	if (!cp->csk_tbl)
		return 0;

	for (i = 0; i < MAX_CM_SK_TBL_SZ; i++) {
		struct cnic_sock *csk = &cp->csk_tbl[i];

		clear_bit(SK_F_INUSE, &csk->flags);
		cnic_cm_cleanup(csk);
	}
	cnic_cm_free_mem(dev);

	return 0;
}

static void cnic_init_context(struct cnic_dev *dev, u32 cid)
{
	u32 cid_addr;
	int i;

	cid_addr = GET_CID_ADDR(cid);

	for (i = 0; i < CTX_SIZE; i += 4)
		cnic_ctx_wr(dev, cid_addr, i, 0);
}

static int cnic_setup_5709_context(struct cnic_dev *dev, int valid)
{
	struct cnic_local *cp = dev->cnic_priv;
	int ret = 0, i;
	u32 valid_bit = valid ? BNX2_CTX_HOST_PAGE_TBL_DATA0_VALID : 0;

	if (BNX2_CHIP(cp) != BNX2_CHIP_5709)
		return 0;

	for (i = 0; i < cp->ctx_blks; i++) {
		int j;
		u32 idx = cp->ctx_arr[i].cid / cp->cids_per_blk;
		u32 val;

		memset(cp->ctx_arr[i].ctx, 0, BNX2_PAGE_SIZE);

		CNIC_WR(dev, BNX2_CTX_HOST_PAGE_TBL_DATA0,
			(cp->ctx_arr[i].mapping & 0xffffffff) | valid_bit);
		CNIC_WR(dev, BNX2_CTX_HOST_PAGE_TBL_DATA1,
			(u64) cp->ctx_arr[i].mapping >> 32);
		CNIC_WR(dev, BNX2_CTX_HOST_PAGE_TBL_CTRL, idx |
			BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ);
		for (j = 0; j < 10; j++) {

			val = CNIC_RD(dev, BNX2_CTX_HOST_PAGE_TBL_CTRL);
			if (!(val & BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ))
				break;
			udelay(5);
		}
		if (val & BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ) {
			ret = -EBUSY;
			break;
		}
	}
	return ret;
}

static void cnic_free_irq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;

	if (ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX) {
		cp->disable_int_sync(dev);
		tasklet_kill(&cp->cnic_irq_task);
		if (test_bit(CNIC_LCL_FL_IRQ_REQD, &cp->cnic_local_flags))
			free_irq(ethdev->irq_arr[0].vector, dev);
		clear_bit(CNIC_LCL_FL_IRQ_REQD, &cp->cnic_local_flags);
	}
}

static int cnic_request_irq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int err;

	if (test_bit(CNIC_LCL_FL_IRQ_REQD, &cp->cnic_local_flags))
		return 0;

	err = request_irq(ethdev->irq_arr[0].vector, cnic_irq, 0, "cnic", dev);
	if (err) {
		CNIC_ERR("Failed to request irq [%d]", err);
		tasklet_disable(&cp->cnic_irq_task);
	} else
		set_bit(CNIC_LCL_FL_IRQ_REQD, &cp->cnic_local_flags);

	return err;
}

static int cnic_init_bnx2_irq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;

	if (ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX) {
		int err, i = 0;
		int sblk_num = cp->status_blk_num;
		u32 base = ((sblk_num - 1) * BNX2_HC_SB_CONFIG_SIZE) +
			   BNX2_HC_SB_CONFIG_1;

		CNIC_WR(dev, base, BNX2_HC_SB_CONFIG_1_ONE_SHOT);

		CNIC_WR(dev, base + BNX2_HC_COMP_PROD_TRIP_OFF, (2 << 16) | 8);
		CNIC_WR(dev, base + BNX2_HC_COM_TICKS_OFF, (64 << 16) | 220);
		CNIC_WR(dev, base + BNX2_HC_CMD_TICKS_OFF, (64 << 16) | 220);

		cp->last_status_idx = cp->status_blk.bnx2->status_idx;

		tasklet_init(&cp->cnic_irq_task, cnic_service_bnx2_msix,
			     (unsigned long) dev);
		err = cnic_request_irq(dev);
		if (err)
			return err;

		while (cp->status_blk.bnx2->status_completion_producer_index &&
		       i < 10) {
			CNIC_WR(dev, BNX2_HC_COALESCE_NOW,
				1 << (11 + sblk_num));
			udelay(10);
			i++;
			barrier();
		}
		if (cp->status_blk.bnx2->status_completion_producer_index) {

			cnic_free_irq(dev);
			goto failed;
		}

	} else {
		struct status_block *sblk = cp->status_blk.gen;
		u32 hc_cmd = CNIC_RD(dev, BNX2_HC_COMMAND);
		int i = 0;

		while (sblk->status_completion_producer_index && i < 10) {
			CNIC_WR(dev, BNX2_HC_COMMAND,
				hc_cmd | BNX2_HC_COMMAND_COAL_NOW_WO_INT);
			udelay(10);
			i++;
			barrier();
		}
		if (sblk->status_completion_producer_index)
			goto failed;

	}
	return 0;

failed:
	CNIC_ERR("KCQ index not resetting to 0.\n");
	return -EBUSY;
}

static void cnic_enable_bnx2_int(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;

	if (!(ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX))
		return;

	CNIC_WR(dev, BNX2_PCICFG_INT_ACK_CMD, cp->int_num |
		BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID | cp->last_status_idx);
}

static void cnic_disable_bnx2_int_sync(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;

	if (!(ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX))
		return;

	CNIC_WR(dev, BNX2_PCICFG_INT_ACK_CMD, cp->int_num |
		BNX2_PCICFG_INT_ACK_CMD_MASK_INT);
	CNIC_RD(dev, BNX2_PCICFG_INT_ACK_CMD);
	synchronize_irq(ethdev->irq_arr[0].vector);
}

#if (CNIC_ISCSI_OOO_SUPPORT)
static void cnic_init_bnx2_tx_ring_start(struct cnic_dev *dev, u32 cid,
					 dma_addr_t ring_map)
{
	struct cnic_local *cp = dev->cnic_priv;
	u32 cid_addr, val, offset0, offset1, offset2, offset3;

	cid_addr = GET_CID_ADDR(cid);
	if (BNX2_CHIP(cp) == BNX2_CHIP_5709) {
		int i;
		u32 cid_addr2 = GET_CID_ADDR(cid + 4) + 0x40;

		for (i = 0; i < PHY_CTX_SIZE; i += 4)
			cnic_ctx_wr(dev, cid_addr2, i, 0);

		offset0 = BNX2_L2CTX_TYPE_XI;
		offset1 = BNX2_L2CTX_CMD_TYPE_XI;
		offset2 = BNX2_L2CTX_TBDR_BHADDR_HI_XI;
		offset3 = BNX2_L2CTX_TBDR_BHADDR_LO_XI;
	} else {
		cnic_init_context(dev, cid);
		cnic_init_context(dev, cid + 1);

		offset0 = BNX2_L2CTX_TYPE;
		offset1 = BNX2_L2CTX_CMD_TYPE;
		offset2 = BNX2_L2CTX_TBDR_BHADDR_HI;
		offset3 = BNX2_L2CTX_TBDR_BHADDR_LO;
	}
	val = BNX2_L2CTX_TYPE_TYPE_L2 | BNX2_L2CTX_TYPE_SIZE_L2;
	cnic_ctx_wr(dev, cid_addr, offset0, val);

	val = BNX2_L2CTX_CMD_TYPE_TYPE_L2 | (8 << 16);
	cnic_ctx_wr(dev, cid_addr, offset1, val);

	val = (u64) ring_map >> 32;
	cnic_ctx_wr(dev, cid_addr, offset2, val);

	val = (u64) ring_map & 0xffffffff;
	cnic_ctx_wr(dev, cid_addr, offset3, val);
}

static void cnic_init_bnx2_rx_ring_start(struct cnic_dev *dev, u32 cid,
					 u16 *sb_idx, dma_addr_t ring_map,
					 u32 sb_id)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	u32 cid_addr, val, coal_reg, coal_val;
	int i;

	cnic_init_context(dev, cid);
	coal_reg = BNX2_HC_COMMAND;
	coal_val = CNIC_RD(dev, coal_reg);
	if (ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX) {
		coal_reg = BNX2_HC_COALESCE_NOW;
		coal_val = 1 << (11 + sb_id);
	}
	i = 0;
	while (*sb_idx != 0 && i < 10) {
		CNIC_WR(dev, coal_reg, coal_val);
		udelay(10);
		i++;
		barrier();
	}

	cid_addr = GET_CID_ADDR(cid);
	val = BNX2_L2CTX_CTX_TYPE_CTX_BD_CHN_TYPE_VALUE |
	      BNX2_L2CTX_CTX_TYPE_SIZE_L2 | (0x02 << 8);
	cnic_ctx_wr(dev, cid_addr, BNX2_L2CTX_CTX_TYPE, val);

	if (sb_id == 0)
		val = 2 << BNX2_L2CTX_L2_STATUSB_NUM_SHIFT;
	else
		val = BNX2_L2CTX_L2_STATUSB_NUM(sb_id);
	cnic_ctx_wr(dev, cid_addr, BNX2_L2CTX_HOST_BDIDX, val);

	val = (u64) ring_map >> 32;
	cnic_ctx_wr(dev, cid_addr, BNX2_L2CTX_NX_BDHADDR_HI, val);

	val = (u64) ring_map & 0xffffffff;
	cnic_ctx_wr(dev, cid_addr, BNX2_L2CTX_NX_BDHADDR_LO, val);
}

static void cnic_set_bnx2_rxbd(struct bnx2_rx_bd *rxbd, u32 len, dma_addr_t map)
{
	rxbd->rx_bd_len = len;
	rxbd->rx_bd_flags = RX_BD_FLAGS_START | RX_BD_FLAGS_END;
	rxbd->rx_bd_haddr_hi = (u64) map >> 32;
	rxbd->rx_bd_haddr_lo = (u64) map & 0xffffffff;
}
#endif

static int cnic_start_bnx2_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	struct status_block *sblk = cp->status_blk.gen;
	u32 val;
	int err;

	val = CNIC_RD(dev, BNX2_MQ_CONFIG);
	val &= ~BNX2_MQ_CONFIG_KNL_BYP_BLK_SIZE;
	if (BNX2_PAGE_BITS > 12)
		val |= (12 - 8)  << 4;
	else
		val |= (BNX2_PAGE_BITS - 8)  << 4;

	CNIC_WR(dev, BNX2_MQ_CONFIG, val);

	CNIC_WR(dev, BNX2_HC_COMP_PROD_TRIP, (2 << 16) | 8);
	CNIC_WR(dev, BNX2_HC_COM_TICKS, (64 << 16) | 220);
	CNIC_WR(dev, BNX2_HC_CMD_TICKS, (64 << 16) | 220);

	err = cnic_setup_5709_context(dev, 1);
	if (err)
		return err;

	cnic_init_context(dev, KWQ_CID);
	cnic_init_context(dev, KCQ_CID);

	cp->kwq_cid_addr = GET_CID_ADDR(KWQ_CID);
	cp->kwq_io_addr = MB_GET_CID_ADDR(KWQ_CID) + L5_KRNLQ_HOST_QIDX;

	cp->max_kwq_idx = MAX_KWQ_IDX;
	cp->kwq_prod_idx = 0;
	cp->kwq_con_idx = 0;
	set_bit(CNIC_LCL_FL_KWQ_INIT, &cp->cnic_local_flags);

	if (BNX2_CHIP(cp) == BNX2_CHIP_5706 || BNX2_CHIP(cp) == BNX2_CHIP_5708)
		cp->kwq_con_idx_ptr = &sblk->status_rx_quick_consumer_index15;
	else
		cp->kwq_con_idx_ptr = &sblk->status_cmd_consumer_index;

	/* Initialize the kernel work queue context. */
	val = KRNLQ_TYPE_TYPE_KRNLQ | KRNLQ_SIZE_TYPE_SIZE |
	      (BNX2_PAGE_BITS - 8) | KRNLQ_FLAGS_QE_SELF_SEQ;
	cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_TYPE, val);
	
	val = (BNX2_PAGE_SIZE / sizeof(struct kwqe) - 1) << 16;
	cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_QE_SELF_SEQ_MAX, val);
	
	val = ((BNX2_PAGE_SIZE / sizeof(struct kwqe)) << 16) | KWQ_PAGE_CNT;
	cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_PGTBL_NPAGES, val);
	
	val = (u32) ((u64) cp->kwq_info.pgtbl_map >> 32);
	cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_PGTBL_HADDR_HI, val);
	
	val = (u32) cp->kwq_info.pgtbl_map;
	cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_PGTBL_HADDR_LO, val);

	cp->kcq_cid_addr = GET_CID_ADDR(KCQ_CID);
	cp->kcq1.io_addr = MB_GET_CID_ADDR(KCQ_CID) + L5_KRNLQ_HOST_QIDX;

	cp->kcq1.sw_prod_idx = 0;
	cp->kcq1.hw_prod_idx_ptr =
		(u16 *) &sblk->status_completion_producer_index;

	cp->kcq1.status_idx_ptr = (u16 *) &sblk->status_idx;

	/* Initialize the kernel complete queue context. */
	val = KRNLQ_TYPE_TYPE_KRNLQ | KRNLQ_SIZE_TYPE_SIZE |
	      (BNX2_PAGE_BITS - 8) | KRNLQ_FLAGS_QE_SELF_SEQ;
	cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_TYPE, val);

	val = (BNX2_PAGE_SIZE / sizeof(struct kcqe) - 1) << 16;
	cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_QE_SELF_SEQ_MAX, val);

	val = ((BNX2_PAGE_SIZE / sizeof(struct kcqe)) << 16) | KCQ_PAGE_CNT;
	cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_PGTBL_NPAGES, val);

	val = (u32) ((u64) cp->kcq1.dma.pgtbl_map >> 32);
	cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_PGTBL_HADDR_HI, val);

	val = (u32) cp->kcq1.dma.pgtbl_map;
	cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_PGTBL_HADDR_LO, val);

	cp->int_num = 0;
	if (ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX) {
		struct status_block_msix *msblk = cp->status_blk.bnx2;
		u32 sb_id = cp->status_blk_num;
		u32 sb = BNX2_L2CTX_L5_STATUSB_NUM(sb_id);

		cp->kcq1.hw_prod_idx_ptr =
			(u16 *) &msblk->status_completion_producer_index;
		cp->kcq1.status_idx_ptr = (u16 *) &msblk->status_idx;
		cp->kwq_con_idx_ptr = (u16 *) &msblk->status_cmd_consumer_index;
		cp->int_num = sb_id << BNX2_PCICFG_INT_ACK_CMD_INT_NUM_SHIFT;
		cnic_ctx_wr(dev, cp->kwq_cid_addr, L5_KRNLQ_HOST_QIDX, sb);
		cnic_ctx_wr(dev, cp->kcq_cid_addr, L5_KRNLQ_HOST_QIDX, sb);
	}

	/* Enable Commnad Scheduler notification when we write to the
	 * host producer index of the kernel contexts. */
	CNIC_WR(dev, BNX2_MQ_KNL_CMD_MASK1, 2);

	/* Enable Command Scheduler notification when we write to either
	 * the Send Queue or Receive Queue producer indexes of the kernel
	 * bypass contexts. */
	CNIC_WR(dev, BNX2_MQ_KNL_BYP_CMD_MASK1, 7);
	CNIC_WR(dev, BNX2_MQ_KNL_BYP_WRITE_MASK1, 7);

	/* Notify COM when the driver post an application buffer. */
	CNIC_WR(dev, BNX2_MQ_KNL_RX_V2P_MASK2, 0x2000);

	/* Set the CP and COM doorbells.  These two processors polls the
	 * doorbell for a non zero value before running.  This must be done
	 * after setting up the kernel queue contexts. */
	val = cnic_reg_rd_ind(dev, BNX2_CP_SCRATCH + 0x20);
	cnic_reg_wr_ind(dev, BNX2_CP_SCRATCH + 0x20, val | 1);

	val = cnic_reg_rd_ind(dev, BNX2_COM_SCRATCH + 0x20);
	cnic_reg_wr_ind(dev, BNX2_COM_SCRATCH + 0x20, val | 1);

	err = cnic_init_bnx2_irq(dev);
	if (err) {
		CNIC_ERR("cnic_init_irq failed\n");

		val = cnic_reg_rd_ind(dev, BNX2_CP_SCRATCH + 0x20);
		cnic_reg_wr_ind(dev, BNX2_CP_SCRATCH + 0x20, val & ~0x1);

		val = cnic_reg_rd_ind(dev, BNX2_COM_SCRATCH + 0x20);
		cnic_reg_wr_ind(dev, BNX2_COM_SCRATCH + 0x20, val & ~0x1);

		return err;
	}

	ethdev->drv_state |= CNIC_DRV_STATE_HANDLES_IRQ;
#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_start_bnx2_ooo_hw(dev);
#endif
	return 0;
}

static void cnic_setup_bnx2x_context(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	u32 start_offset = ethdev->ctx_tbl_offset;
	int i;

	for (i = 0; i < cp->ctx_blks; i++) {
		struct cnic_ctx *ctx = &cp->ctx_arr[i];
		dma_addr_t map = ctx->mapping;

		if (cp->ctx_align) {
			unsigned long mask = cp->ctx_align - 1;

			map = (map + mask) & ~mask;
		}

		cnic_ctx_tbl_wr(dev, start_offset + i, map);
	}
}

static int cnic_init_bnx2x_irq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int err = 0;

	tasklet_init(&cp->cnic_irq_task, cnic_service_bnx2x_bh,
		     (unsigned long) dev);
	if (ethdev->drv_state & CNIC_DRV_STATE_USING_MSIX)
		err = cnic_request_irq(dev);

	return err;
}

#if (NEW_BNX2X_HSI >= 60)
static inline void cnic_storm_memset_hc_disable(struct cnic_dev *dev,
						u16 sb_id, u8 sb_index,
						u8 disable)
{
	struct bnx2x *bp = netdev_priv(dev->netdev);

	u32 addr = BAR_CSTRORM_INTMEM +
			CSTORM_STATUS_BLOCK_DATA_OFFSET(sb_id) +
			offsetof(struct hc_status_block_data_e1x, index_data) +
			sizeof(struct hc_index_data)*sb_index +
			offsetof(struct hc_index_data, flags);
	u16 flags = CNIC_RD16(dev, addr);
	/* clear and set */
	flags &= ~HC_INDEX_DATA_HC_ENABLED;
	flags |= (((~disable) << HC_INDEX_DATA_HC_ENABLED_SHIFT) &
		  HC_INDEX_DATA_HC_ENABLED);
	CNIC_WR16(dev, addr, flags);
}
#endif

static void cnic_enable_bnx2x_int(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u8 sb_id = cp->status_blk_num;

#if (NEW_BNX2X_HSI >= 60)
	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
			CSTORM_STATUS_BLOCK_DATA_OFFSET(sb_id) +
			offsetof(struct hc_status_block_data_e1x, index_data) +
			sizeof(struct hc_index_data)*HC_INDEX_ISCSI_EQ_CONS +
			offsetof(struct hc_index_data, timeout), 64 / 4);
	cnic_storm_memset_hc_disable(dev, sb_id, HC_INDEX_ISCSI_EQ_CONS, 0);
#elif (NEW_BNX2X_HSI == 50)
	int port = BP_PORT(bp);

	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		 CSTORM_SB_HC_TIMEOUT_C_OFFSET(port, sb_id,
					       HC_INDEX_C_ISCSI_EQ_CONS),
		 64 / 12);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_SB_HC_DISABLE_C_OFFSET(port, sb_id,
					        HC_INDEX_C_ISCSI_EQ_CONS), 0);
#elif (NEW_BNX2X_HSI == 48)
	int port = BP_PORT(bap);

	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		 CSTORM_SB_HC_TIMEOUT_OFFSET(port, sb_id,
					     HC_INDEX_C_ISCSI_EQ_CONS),
		 64 / 12);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_SB_HC_DISABLE_OFFSET(port, sb_id,
					      HC_INDEX_C_ISCSI_EQ_CONS), 0);
#endif
}

static void cnic_disable_bnx2x_int_sync(struct cnic_dev *dev)
{
}


static void cnic_init_bnx2x_kcq(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	u32 pfid = bp->pfid;

	cp->kcq1.io_addr = BAR_CSTRORM_INTMEM +
			   CSTORM_ISCSI_EQ_PROD_OFFSET(pfid, 0);
	cp->kcq1.sw_prod_idx = 0;

#if (NEW_BNX2X_HSI >= 60)
	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		struct host_hc_status_block_e2 *sb = cp->status_blk.gen;

		cp->kcq1.hw_prod_idx_ptr =
			&sb->sb.index_values[HC_INDEX_ISCSI_EQ_CONS];
		cp->kcq1.status_idx_ptr =
			&sb->sb.running_index[SM_RX_ID];
	} else {
		struct host_hc_status_block_e1x *sb = cp->status_blk.gen;

		cp->kcq1.hw_prod_idx_ptr =
			&sb->sb.index_values[HC_INDEX_ISCSI_EQ_CONS];
		cp->kcq1.status_idx_ptr =
			&sb->sb.running_index[SM_RX_ID];
	}
#else
	cp->kcq1.hw_prod_idx_ptr =
		&cp->status_blk.bnx2x->c_status_block.index_values[HC_INDEX_C_ISCSI_EQ_CONS];
	cp->kcq1.status_idx_ptr =
		&cp->status_blk.bnx2x->c_status_block.status_block_index;
#endif

#if (NEW_BNX2X_HSI >= 60)
	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		struct host_hc_status_block_e2 *sb = cp->status_blk.gen;

		cp->kcq2.io_addr = BAR_USTRORM_INTMEM +
					USTORM_FCOE_EQ_PROD_OFFSET(pfid);
		cp->kcq2.sw_prod_idx = 0;
		cp->kcq2.hw_prod_idx_ptr =
			&sb->sb.index_values[HC_INDEX_FCOE_EQ_CONS];
		cp->kcq2.status_idx_ptr =
			&sb->sb.running_index[SM_RX_ID];
	}
#endif
}

static int cnic_start_bnx2x_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int func = CNIC_FUNC(cp), ret;
#if (NEW_BNX2X_HSI <= 60)
	int i;
#endif
	u32 pfid;
	u16 eq_idx;
#if (NEW_BNX2X_HSI < 60)
	int port = BP_PORT(bp);
	u8 sb_id = cp->status_blk_num;
#endif

	dev->stats_addr = ethdev->addr_drv_info_to_mcp;
	cp->func = bp->pf_num;

	func = CNIC_FUNC(cp);
	pfid = bp->pfid;

	ret = cnic_init_id_tbl(&cp->cid_tbl, MAX_ISCSI_TBL_SZ,
				cp->iscsi_start_cid, 0);

	if (ret)
		return -ENOMEM;

	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		ret = cnic_init_id_tbl(&cp->fcoe_cid_tbl,
					dev->max_fcoe_conn,
					cp->fcoe_start_cid, 0);

		if (ret)
			return -ENOMEM;
	}

#if (NEW_BNX2X_HSI >= 60)
	cp->bnx2x_igu_sb_id = ethdev->irq_arr[0].status_blk_num2;
#else
	cp->bnx2x_igu_sb_id = ethdev->irq_arr[0].status_blk_num;
#endif

	cnic_init_bnx2x_kcq(dev);

	/* Only 1 EQ */
	CNIC_WR16(dev, cp->kcq1.io_addr, MAX_KCQ_IDX);
	CNIC_WR(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_CONS_OFFSET(pfid, 0), 0);
	CNIC_WR(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_NEXT_PAGE_ADDR_OFFSET(pfid, 0),
		cp->kcq1.dma.pg_map_arr[1] & 0xffffffff);
	CNIC_WR(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_NEXT_PAGE_ADDR_OFFSET(pfid, 0) + 4,
		(u64) cp->kcq1.dma.pg_map_arr[1] >> 32);
	CNIC_WR(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_NEXT_EQE_ADDR_OFFSET(pfid, 0),
		cp->kcq1.dma.pg_map_arr[0] & 0xffffffff);
	CNIC_WR(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_NEXT_EQE_ADDR_OFFSET(pfid, 0) + 4,
		(u64) cp->kcq1.dma.pg_map_arr[0] >> 32);
	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_NEXT_PAGE_ADDR_VALID_OFFSET(pfid, 0), 1);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_SB_NUM_OFFSET(pfid, 0), cp->status_blk_num);
#if (NEW_BNX2X_HSI >= 60)
	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_SB_INDEX_OFFSET(pfid, 0),
		HC_INDEX_ISCSI_EQ_CONS);
#else
	CNIC_WR8(dev, BAR_CSTRORM_INTMEM +
		CSTORM_ISCSI_EQ_SB_INDEX_OFFSET(pfid, 0),
		HC_INDEX_C_ISCSI_EQ_CONS);
#endif

#if (NEW_BNX2X_HSI <= 60)
	for (i = 0; i < cp->conn_buf_info.num_pages; i++) {
		CNIC_WR(dev, BAR_TSTRORM_INTMEM +
			TSTORM_ISCSI_CONN_BUF_PBL_OFFSET(pfid, i),
			cp->conn_buf_info.pgtbl[2 * i]);
		CNIC_WR(dev, BAR_TSTRORM_INTMEM +
			TSTORM_ISCSI_CONN_BUF_PBL_OFFSET(pfid, i) + 4,
			cp->conn_buf_info.pgtbl[(2 * i) + 1]);
	}
#endif

	CNIC_WR(dev, BAR_USTRORM_INTMEM +
		USTORM_ISCSI_GLOBAL_BUF_PHYS_ADDR_OFFSET(pfid),
		cp->gbl_buf_info.pg_map_arr[0] & 0xffffffff);
	CNIC_WR(dev, BAR_USTRORM_INTMEM +
		USTORM_ISCSI_GLOBAL_BUF_PHYS_ADDR_OFFSET(pfid) + 4,
		(u64) cp->gbl_buf_info.pg_map_arr[0] >> 32);

#if (NEW_BNX2X_HSI >= 60)
	CNIC_WR(dev, BAR_TSTRORM_INTMEM +
		TSTORM_ISCSI_TCP_LOCAL_ADV_WND_OFFSET(pfid), DEF_RCV_BUF);
#endif

	cnic_setup_bnx2x_context(dev);

#if (NEW_BNX2X_HSI >= 60)
	eq_idx = 0;
#elif (NEW_BNX2X_HSI == 50)
	eq_idx = CNIC_RD16(dev, BAR_CSTRORM_INTMEM +
			   CSTORM_SB_HOST_STATUS_BLOCK_C_OFFSET(port, sb_id) +
			   offsetof(struct cstorm_status_block_c,
				    index_values[HC_INDEX_C_ISCSI_EQ_CONS]));
#elif (NEW_BNX2X_HSI == 48)
	eq_idx = CNIC_RD16(dev, BAR_CSTRORM_INTMEM +
			   CSTORM_SB_HOST_STATUS_BLOCK_OFFSET(port, sb_id) +
			   offsetof(struct cstorm_status_block,
				    index_values[HC_INDEX_C_ISCSI_EQ_CONS]));
#endif
	if (eq_idx != 0) {
		CNIC_ERR("EQ cons index %x != 0\n", eq_idx);
		return -EBUSY;
	}

#if (CNIC_ISCSI_OOO_SUPPORT)
	cnic_start_bnx2x_ooo_hw(dev);
#endif
	ret = cnic_init_bnx2x_irq(dev);
	if (ret)
		return ret;

	ethdev->drv_state |= CNIC_DRV_STATE_HANDLES_IRQ;
	return 0;
}

static int cnic_start_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_eth_dev *ethdev = cp->ethdev;
	int err;

	if (test_bit(CNIC_F_CNIC_UP, &dev->flags))
		return -EALREADY;

#if !defined (__VMKLNX__)
	if (!try_module_get(ethdev->drv_owner))
		return -EBUSY;
#endif /* !defined (__VMKLNX__) */

	dev->regview = ethdev->io_base;
	pci_dev_get(dev->pcidev);
	cp->func = PCI_FUNC(dev->pcidev->devfn);
	cp->status_blk.gen = ethdev->irq_arr[0].status_blk;
	cp->status_blk_num = ethdev->irq_arr[0].status_blk_num;

	err = cp->alloc_resc(dev);
	if (err) {
		CNIC_ERR("allocate resource failure\n");
		goto err1;
	}

	err = cp->start_hw(dev);
	if (err)
		goto err1;

	set_bit(CNIC_LCL_FL_HW_START, &cp->cnic_local_flags);

	err = cnic_cm_open(dev);
	if (err)
		goto err1;

	set_bit(CNIC_F_CNIC_UP, &dev->flags);

	cp->enable_int(dev);

	return 0;

err1:
	cp->free_resc(dev);
	pci_dev_put(dev->pcidev);
#if !defined (__VMKLNX__)
	module_put(ethdev->drv_owner);
#endif /* !defined (__VMKLNX__) */
	return err;
}

static void cnic_stop_bnx2_hw(struct cnic_dev *dev)
{
	u32 val;

	cnic_disable_bnx2_int_sync(dev);

	val = cnic_reg_rd_ind(dev, BNX2_CP_SCRATCH + 0x20);
	cnic_reg_wr_ind(dev, BNX2_CP_SCRATCH + 0x20, val & ~0x1);

	val = cnic_reg_rd_ind(dev, BNX2_COM_SCRATCH + 0x20);
	cnic_reg_wr_ind(dev, BNX2_COM_SCRATCH + 0x20, val & ~0x1);

	cnic_init_context(dev, KWQ_CID);
	cnic_init_context(dev, KCQ_CID);

	cnic_setup_5709_context(dev, 0);
	cnic_free_irq(dev);

	cnic_free_resc(dev);
}

static void cnic_stop_bnx2x_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
#if (NEW_BNX2X_HSI < 60)
	u8 sb_id = cp->status_blk_num;
	int port = BP_PORT(bp);
#endif
	u32 hc_index = HC_INDEX_ISCSI_EQ_CONS;
	u32 sb_id = cp->status_blk_num;
	u32 idx_off, syn_off;

	if (cp->ethdev->mf_mode == MULTI_FUNCTION_SI ||
	    cp->ethdev->mf_mode == MULTI_FUNCTION_AFEX)
		cnic_npar_ring_ctl(dev, 0);

	cnic_free_irq(dev);

	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		idx_off = offsetof(struct hc_status_block_e2, index_values) +
			  (hc_index * sizeof(u16));

		syn_off = CSTORM_HC_SYNC_LINE_INDEX_E2_OFFSET(hc_index, sb_id);
	} else {
		idx_off = offsetof(struct hc_status_block_e1x, index_values) +
			  (hc_index * sizeof(u16));

		syn_off = CSTORM_HC_SYNC_LINE_INDEX_E1X_OFFSET(hc_index, sb_id);
	}
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM + syn_off, 0);
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM + CSTORM_STATUS_BLOCK_OFFSET(sb_id) +
		  idx_off, 0);

#if (NEW_BNX2X_HSI >= 60)
	*cp->kcq1.hw_prod_idx_ptr = 0;
#elif (NEW_BNX2X_HSI == 50)
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_SB_HOST_STATUS_BLOCK_C_OFFSET(port, sb_id) +
		  offsetof(struct cstorm_status_block_c,
			   index_values[HC_INDEX_C_ISCSI_EQ_CONS]),
		  0);
#elif (NEW_BNX2X_HSI == 48)
	CNIC_WR16(dev, BAR_CSTRORM_INTMEM +
		  CSTORM_SB_HOST_STATUS_BLOCK_OFFSET(port, sb_id) +
		  offsetof(struct cstorm_status_block,
			   index_values[HC_INDEX_C_ISCSI_EQ_CONS]),
		  0);
#endif
	CNIC_WR(dev, BAR_CSTRORM_INTMEM + 
		CSTORM_ISCSI_EQ_CONS_OFFSET(bp->pfid, 0), 0);
	CNIC_WR16(dev, cp->kcq1.io_addr, 0);
	cnic_free_resc(dev);
}

static void cnic_stop_hw(struct cnic_dev *dev)
{
	if (test_bit(CNIC_F_CNIC_UP, &dev->flags)) {
		struct cnic_local *cp = dev->cnic_priv;

#if (CNIC_ISCSI_OOO_SUPPORT)
		/* Must stop the ooo engine before freeing kwqe resources */
		cp->stop_ooo_hw(dev);
#endif
		cp->stop_cm(dev);
		cp->ethdev->drv_state &= ~CNIC_DRV_STATE_HANDLES_IRQ;
		clear_bit(CNIC_F_CNIC_UP, &dev->flags);
		rcu_assign_pointer(cp->ulp_ops[CNIC_ULP_L4], NULL);
		synchronize_rcu();
		cnic_cm_shutdown(dev);
		cp->stop_hw(dev);
		clear_bit(CNIC_LCL_FL_HW_START, &cp->cnic_local_flags);
		pci_dev_put(dev->pcidev);
#if !defined (__VMKLNX__)
		module_put(cp->ethdev->drv_owner);
#endif /* !defined (__VMKLNX__) */
	}
}

static struct cnic_dev *cnic_alloc_dev(struct net_device *dev,
				       struct pci_dev *pdev)
{
	struct cnic_dev *cdev;
	struct cnic_local *cp;
	int alloc_size;

	alloc_size = sizeof(struct cnic_dev) + sizeof(struct cnic_local);

	cdev = kzalloc(alloc_size , GFP_KERNEL);
	if (cdev == NULL) {
		printk(KERN_ERR PFX "%s: allocate dev struct failure\n",
		       dev->name);
		return NULL;
	}

	cdev->version = CNIC_DEV_VER;
	cdev->netdev = dev;
	cdev->cnic_priv = (char *)cdev + sizeof(struct cnic_dev);
	cdev->register_device = cnic_register_device;
	cdev->unregister_device = cnic_unregister_device;
	cp = cdev->cnic_priv;
	cp->dev = cdev;

	spin_lock_init(&cp->cnic_ulp_lock);
	printk(KERN_INFO PFX "Added CNIC device: %s\n", dev->name);

	return cdev;
}

static struct cnic_dev *init_bnx2_cnic(struct net_device *dev)
{
	struct pci_dev *pdev;
	struct cnic_dev *cdev;
	struct cnic_local *cp;
#if (VMWARE_ESX_DDK_VERSION < 50000)
	struct bnx2 *bp = netdev_priv(dev);
#endif
	struct cnic_eth_dev *ethdev = NULL;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	struct cnic_eth_dev *(*probe)(struct net_device *) = NULL;
#endif

#if defined (CNIC_INBOX)
#if defined (__VMKLNX__)
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	probe = cnic_register_get_callback("bnx2");
	if (probe)
		ethdev = (*probe)(dev);
#else
	if (bp->version == BNX2_DEV_VER && bp->cnic_probe) {
		VMK_ReturnStatus rc = vmk_ModuleIncUseCount(dev->module_id);
		if (rc != VMK_OK) {
			printk(KERN_WARNING PFX "Unable to inc ref count for bnx2: rc=%d\n", rc);
			return NULL;
		}

		VMKAPI_MODULE_CALL(dev->module_id, ethdev, bp->cnic_probe, dev);
	}

#endif /* (VMWARE_ESX_DDK_VERSION >= 50000) */
#else /* !defined (__VMKLNX__) */
	probe = symbol_get(bnx2_cnic_probe);
	if (probe) {
		ethdev = (*probe)(dev);
		symbol_put(bnx2_cnic_probe);
	}
#endif /* defined (__VMKLNX__) */
#else /* !defined(CNIC_INBOX) */
#if defined (__VMKLNX__)
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	probe = cnic_register_get_callback("bnx2");
	if (probe)
		ethdev = (*probe)(dev);
#else
	if (bp->version == BNX2_DEV_VER && bp->cnic_probe) {
		VMK_ReturnStatus rc = vmk_ModuleIncUseCount(dev->module_id);
		if (rc != VMK_OK) {
			printk(KERN_WARNING PFX "Unable to inc ref count for bnx2: rc=%d\n", rc);
			return NULL;
		}

		VMKAPI_MODULE_CALL(dev->module_id, ethdev, bp->cnic_probe, dev);
	}
#endif /* (VMWARE_ESX_DDK_VERSION >= 50000) */
#else /* !defined (__VMKLNX__) */
	probe = symbol_get(bnx2_cnic_probe2);
	if (probe) {
		ethdev = (*probe)(dev);
		symbol_put(bnx2_cnic_probe2);
	}
#endif /* defined (__VMKLNX__) */
#endif /* defined(CNIC_INBOX) */
	if (!ethdev)
		return NULL;

	if (ethdev->version != CNIC_ETH_DEV_VER) {
		printk(KERN_WARNING PFX "bnx2 not compatible with cnic "
					"expecting: 0x%x got: 0x%x\n",
					CNIC_ETH_DEV_VER, ethdev->version);
		return NULL;
	}

	pdev = ethdev->pdev;
	if (!pdev)
		return NULL;

	dev_hold(dev);
	pci_dev_get(pdev);
	if (pdev->device == PCI_DEVICE_ID_NX2_5709 ||
	    pdev->device == PCI_DEVICE_ID_NX2_5709S) {
		u8 rev;

		pci_read_config_byte(pdev, PCI_REVISION_ID, &rev);
		if (rev < 0x10) {
			pci_dev_put(pdev);
			goto cnic_err;
		}
	}
	pci_dev_put(pdev);

	cdev = cnic_alloc_dev(dev, pdev);
	if (cdev == NULL)
		goto cnic_err;

	set_bit(CNIC_F_BNX2_CLASS, &cdev->flags);
	cdev->submit_kwqes = cnic_submit_bnx2_kwqes;

	cp = cdev->cnic_priv;
	cp->ethdev = ethdev;
	cdev->pcidev = pdev;
	cp->chip_id = ethdev->chip_id;

	cdev->max_iscsi_conn = ethdev->max_iscsi_conn;
	printk (KERN_ERR PFX "%s - Num 1G iSCSI licenses = %d\n", dev->name, cdev->max_iscsi_conn);

	cp->cnic_ops = &cnic_bnx2_ops;
	cp->start_hw = cnic_start_bnx2_hw;
	cp->stop_hw = cnic_stop_bnx2_hw;
	cp->setup_pgtbl = cnic_setup_page_tbl;
	cp->alloc_resc = cnic_alloc_bnx2_resc;
	cp->free_resc = cnic_free_resc;
	cp->start_cm = cnic_cm_init_bnx2_hw;
	cp->stop_cm = cnic_cm_stop_bnx2_hw;
	cp->enable_int = cnic_enable_bnx2_int;
	cp->disable_int_sync = cnic_disable_bnx2_int_sync;
	cp->close_conn = cnic_close_bnx2_conn;

#if (CNIC_ISCSI_OOO_SUPPORT)
	cp->stop_ooo_hw = cnic_stop_bnx2_ooo_hw;
#endif
	return cdev;

cnic_err:
	dev_put(dev);
	return NULL;
}

static void cnic_print_device_probe_info(struct cnic_dev *dev,
					 struct cnic_eth_dev *ethdev)
{

	CNIC_INFO("version %x drv_state %x, chip_id %x, ctx_tbl_offset %x,"
		  " ctx_tbl_len %x \n ctx_blk_size %d, starting_cid 0x%x, "
		  "max_iscsi_conn %d, max_fcoe_conn %d \n fcoe_init_cid 0x%x, "
		  "iscsi_l2_client_id %x, num_irq %d\n",
		  ethdev->version, ethdev->drv_state, ethdev->chip_id,
		  ethdev->ctx_tbl_offset, ethdev->ctx_tbl_len,
		  ethdev->ctx_blk_size, ethdev->starting_cid,
		  ethdev->max_iscsi_conn, ethdev->max_fcoe_conn,
		  ethdev->fcoe_init_cid, ethdev->iscsi_l2_client_id,
		  ethdev->num_irq);
}

static struct cnic_dev *init_bnx2x_cnic(struct net_device *dev)
{
	struct pci_dev *pdev;
	struct cnic_dev *cdev;
	struct cnic_local *cp;
	struct cnic_eth_dev *ethdev = NULL;
#if (VMWARE_ESX_DDK_VERSION >= 50000) || !defined(__VMKLNX__)
	struct cnic_eth_dev *(*probe)(struct net_device *) = NULL;
#endif
	struct bnx2x *bp = netdev_priv(dev);

#if defined (CNIC_INBOX)
#if defined (__VMKLNX__)
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	probe = cnic_register_get_callback("bnx2x");
	if (probe) {
		ethdev = (*probe)(dev);
	}
#else
	if (bp->version == BNX2X_DEV_VER && bp->cnic_probe) {
		VMK_ReturnStatus rc = vmk_ModuleIncUseCount(dev->module_id);
		if (rc != VMK_OK) {
			printk(KERN_WARNING PFX "Unable to inc ref count for bnx2x: rc=%d\n", rc);
			return NULL;
		}

		VMKAPI_MODULE_CALL(dev->module_id, ethdev, bp->cnic_probe, dev);
	}
#endif /* (VMWARE_ESX_DDK_VERSION >= 50000) */
#else /* !defined (__VMKLNX__) */
	probe = symbol_get(bnx2x_cnic_probe);
	if (probe) {
		ethdev = (*probe)(dev);
		symbol_put(bnx2x_cnic_probe);
	}
#endif /* defined (__VMKLNX__) */
#else /* !defined (CNIC_INBOX) */
#if defined (__VMKLNX__)
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	probe = cnic_register_get_callback("bnx2x");
	if (probe) {
		ethdev = (*probe)(dev);
	}
#else
	if (bp->version == BNX2X_DEV_VER && bp->cnic_probe) {
		VMK_ReturnStatus rc = vmk_ModuleIncUseCount(dev->module_id);
		if (rc != VMK_OK) {
			printk(KERN_WARNING PFX "Unable to inc ref count for bnx2x: rc=%d\n", rc);
			return NULL;
		}

		VMKAPI_MODULE_CALL(dev->module_id, ethdev, bp->cnic_probe, dev);
	}
#endif /* (VMWARE_ESX_DDK_VERSION >= 50000) */
#else /* !defined (__VMKLNX__) */
	probe = symbol_get(bnx2x_cnic_probe2);
	if (probe) {
		ethdev = (*probe)(dev);
		symbol_put(bnx2x_cnic_probe2);
	}
#endif /* defined (__VMKLNX__) */
#endif /* defined (CNIC_INBOX) */
	if (!ethdev)
		return NULL;

	if (ethdev->version != CNIC_ETH_DEV_VER) {
		printk(KERN_WARNING PFX "bnx2x not compatible with cnic "
					"expecting: 0x%x got: 0x%x\n",
					CNIC_ETH_DEV_VER, ethdev->version);
		return NULL;
	}

	pdev = ethdev->pdev;
	if (!pdev)
		return NULL;

	dev_hold(dev);
	cdev = cnic_alloc_dev(dev, pdev);
	if (cdev == NULL) {
		dev_put(dev);
		return NULL;
	}

#if defined (__VMKLNX__)
	cnic_print_device_probe_info(cdev, ethdev);
#endif

	set_bit(CNIC_F_BNX2X_CLASS, &cdev->flags);
	cdev->submit_kwqes = cnic_submit_bnx2x_kwqes;

	cp = cdev->cnic_priv;
	cp->ethdev = ethdev;
	cdev->pcidev = pdev;
	cdev->mf_mode   = ethdev->mf_mode;
	cdev->e1hov_tag = ethdev->e1hov_tag;
#if ((!defined (CNIC_INBOX) && (VMWARE_ESX_DDK_VERSION >= 50000)) || \
     (defined (CNIC_INBOX) && (VMWARE_ESX_DDK_VERSION >= 55000)))
	cdev->cna_vlgrp = ethdev->cna_vlgrp;
	cdev->fcoe_wwpn = (((u64)ethdev->fcoe_wwn_port_name_hi << 32) |
			   ethdev->fcoe_wwn_port_name_lo);
	cdev->fcoe_wwnn = (((u64)ethdev->fcoe_wwn_node_name_hi << 32) |
			   ethdev->fcoe_wwn_node_name_lo);
#endif
	cp->chip_id = ethdev->chip_id;
	
	cdev->stats_addr = ethdev->addr_drv_info_to_mcp;

	if (!(ethdev->drv_state & CNIC_DRV_STATE_NO_ISCSI)) {
		cdev->max_iscsi_conn = ethdev->max_iscsi_conn;
		printk (KERN_ERR PFX "%s - Num 10G iSCSI licenses = %d\n", dev->name, cdev->max_iscsi_conn);
	}
	if (BNX2X_CHIP_IS_E2_PLUS(bp) &&
	    !(ethdev->drv_state & CNIC_DRV_STATE_NO_FCOE))
		cdev->max_fcoe_conn = ethdev->max_fcoe_conn;

	if (cdev->max_fcoe_conn > BNX2X_FCOE_NUM_CONNECTIONS)
		cdev->max_fcoe_conn = BNX2X_FCOE_NUM_CONNECTIONS;

	memcpy(cdev->mac_addr, ethdev->iscsi_mac, 6);

	cp->cnic_ops = &cnic_bnx2x_ops;
	cp->start_hw = cnic_start_bnx2x_hw;
	cp->stop_hw = cnic_stop_bnx2x_hw;
	cp->setup_pgtbl = cnic_setup_page_tbl_le;
	cp->alloc_resc = cnic_alloc_bnx2x_resc;
	cp->free_resc = cnic_free_resc;
	cp->start_cm = cnic_cm_init_bnx2x_hw;
	cp->stop_cm = cnic_cm_stop_bnx2x_hw;
	cp->enable_int = cnic_enable_bnx2x_int;
	cp->disable_int_sync = cnic_disable_bnx2x_int_sync;
#if (NEW_BNX2X_HSI >= 60)
	if (BNX2X_CHIP_IS_E2_PLUS(bp))
		cp->ack_int = cnic_ack_bnx2x_e2_msix;
	else
#endif
		cp->ack_int = cnic_ack_bnx2x_msix;
	cp->close_conn = cnic_close_bnx2x_conn;
#if (CNIC_ISCSI_OOO_SUPPORT)
	cp->stop_ooo_hw = cnic_stop_bnx2x_ooo_hw;
#endif
	return cdev;
}

static struct cnic_dev *is_cnic_dev(struct net_device *dev)
{
	struct ethtool_drvinfo drvinfo;
	struct cnic_dev *cdev = NULL;

	if (dev->ethtool_ops && dev->ethtool_ops->get_drvinfo) {
		memset(&drvinfo, 0, sizeof(drvinfo));
		dev->ethtool_ops->get_drvinfo(dev, &drvinfo);

		if (!strcmp(drvinfo.driver, "bnx2"))
			cdev = init_bnx2_cnic(dev);
		if (!strcmp(drvinfo.driver, "bnx2x"))
			cdev = init_bnx2x_cnic(dev);
		if (cdev) {
#if !defined (__VMKLNX__)
			write_lock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
			list_add(&cdev->list, &cnic_dev_list);
#if !defined (__VMKLNX__)
			write_unlock(&cnic_dev_lock);
#endif /* !defined (__VMKLNX__) */
		}
	}
	return cdev;
}

#if defined(__VMKLNX__) && \
    ((VMWARE_ESX_DDK_VERSION >= 50000) && !defined (CNIC_INBOX) || \
     (VMWARE_ESX_DDK_VERSION >= 55000) && defined (CNIC_INBOX))
/**
 * netdev event handler
 */
static int cnic_netdev_event_esx(struct notifier_block *this, unsigned long event,
							 void *ptr)
{
	struct net_device *netdev = ptr;
	struct cnic_dev *dev;
	int if_type;

	dev = cnic_from_netdev(netdev);

	if (dev) {
		struct cnic_local *cp = dev->cnic_priv;

		for (if_type = 0; if_type < MAX_CNIC_ULP_TYPE; if_type++) {
			struct cnic_ulp_ops *ulp_ops;
			void *ctx;

			ulp_ops = rcu_dereference(cp->ulp_ops[if_type]);
			if (!ulp_ops || !ulp_ops->indicate_netevent)
				continue;

			ctx = cp->ulp_handle[if_type];
			ulp_ops->indicate_netevent(ctx, event);
		}

	}
	return NOTIFY_DONE;
}

static struct notifier_block cnic_netdev_notifier_esx = {
	cnic_netdev_event_esx,
	0
};

#elif !defined(__VMKLNX__)
/**
 * IP event handler
 */
static int cnic_ip_event(struct notifier_block *this, unsigned long event,
						 void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *) ptr;
	struct net_device *netdev = (struct net_device *) ifa->ifa_dev->dev;
	struct cnic_dev *dev;
	int if_type;
	u32 my_dev = 0;

	read_lock(&cnic_dev_lock);
	list_for_each_entry(dev, &cnic_dev_list, list) {
		if (netdev == dev->netdev) {
			my_dev = 1;
			cnic_hold(dev);
			break;
		}
	}
	read_unlock(&cnic_dev_lock);

	if (my_dev) {
		struct cnic_local *cp = dev->cnic_priv;

		rcu_read_lock();
		for (if_type = 0; if_type < MAX_CNIC_ULP_TYPE; if_type++) {
			struct cnic_ulp_ops *ulp_ops;

			ulp_ops = rcu_dereference(cp->ulp_ops[if_type]);
			if (ulp_ops) {
				void *ctx = cp->ulp_handle[if_type];

				ulp_ops->indicate_inetevent(ctx, event);
			}
		}
		rcu_read_unlock();

		cnic_put(dev);
	}

	return NOTIFY_DONE;
}

/**
 * netdev event handler
 */
static int cnic_netdev_event(struct notifier_block *this, unsigned long event,
							 void *ptr)
{
	struct net_device *netdev = ptr;
	struct cnic_dev *dev;
	int if_type;
	int new_dev = 0;

	dev = cnic_from_netdev(netdev);

	if (!dev && (event == NETDEV_REGISTER || event == NETDEV_UP)) {
		/* Check for the hot-plug device */
		dev = is_cnic_dev(netdev);
		if (dev) {
			new_dev = 1;
			cnic_hold(dev);
		}
	}
	if (dev) {
		struct cnic_local *cp = dev->cnic_priv;

		if (new_dev)
			cnic_ulp_init(dev);
		else if (event == NETDEV_UNREGISTER)
			cnic_ulp_exit(dev);

		if (event == NETDEV_UP) {
			if (cnic_register_netdev(dev) != 0) {
				cnic_put(dev);
				goto done;
			}
			set_bit(CNIC_F_IF_UP, &dev->flags);
			if (!cnic_start_hw(dev))
				cnic_ulp_start(dev);
		}

		rcu_read_lock();
		for (if_type = 0; if_type < MAX_CNIC_ULP_TYPE; if_type++) {
			struct cnic_ulp_ops *ulp_ops;
			void *ctx;

			ulp_ops = rcu_dereference(cp->ulp_ops[if_type]);
			if (!ulp_ops || !ulp_ops->indicate_netevent)
				continue;

			ctx = cp->ulp_handle[if_type];

			ulp_ops->indicate_netevent(ctx, event);
		}
		rcu_read_unlock();

		if (event == NETDEV_GOING_DOWN) {
			clear_bit(CNIC_F_IF_UP, &dev->flags);
			set_bit(CNIC_F_IF_GOING_DOWN, &dev->flags);
			cnic_ulp_stop(dev);
			cnic_stop_hw(dev);
		} else if (event == NETDEV_DOWN) {
			clear_bit(CNIC_F_IF_GOING_DOWN, &dev->flags);
			cnic_unregister_netdev(dev);
		} else if (event == NETDEV_UNREGISTER) {
			write_lock(&cnic_dev_lock);
			list_del_init(&dev->list);
			write_unlock(&cnic_dev_lock);

			cnic_put(dev);
			cnic_free_dev(dev);
			goto done;
		}
		cnic_put(dev);
	}
done:
	return NOTIFY_DONE;
}

static struct notifier_block cnic_ip_notifier = {
	cnic_ip_event,
	0
};

static struct notifier_block cnic_netdev_notifier = {
	cnic_netdev_event,
	0
};
#endif /* !defined (__VMKLNX__) */

#if (CNIC_ISCSI_OOO_SUPPORT)
/* General OOO engine initialization after it is enabled successfully */
static void ooo_init(struct iooo_mgmt *im)
{
	int i;

	/* Defaults */
	im->flags = 0;

	/* Rings */
	im->rxr.rx_max_ring = 0;
	im->txr.tx_max_ring = 0;

	/* Packet descriptors */
	for (i = 0; i < MAX_OOO_RX_DESC_CNT; i++)
		im->rxr.rx_pkt_desc[i] = NULL;
	for (i = 0; i < MAX_OOO_TX_DESC_CNT; i++)
		im->txr.tx_pkt_desc[i] = NULL;
	im->blk_prod = MAX_IOOO_BLOCK_SUPPORTED - 1;
	im->blk_cons = 0;

	/* Blocks */
	for (i = 0; i < MAX_IOOO_BLOCK_SUPPORTED; i++) {
		im->blk_alloc[i] = i;
		im->blk[i].id = i;
		im->blk[i].pkt_cnt = 0;
		INIT_LIST_HEAD(&im->blk[i].pd_head.list);
	}

	/* Pending queues */
	INIT_LIST_HEAD(&im->txr.tx_pend_pd_head.list);
	im->txr.tx_pend_pd_cnt = 0;

	/* Statistics */
	im->txr.tx_total_pkt_sent = 0;

	spin_lock_init(&im->lock);
}

static int ooo_free_buf_single(struct cnic_dev *dev,
			       struct iooo_pkt_desc *pd)
{
	struct cnic_local *cp = dev->cnic_priv;

	if (!pd) {
		CNIC_ERR("ooo_free_buf_single: pd = NULL!\n");
		return -EINVAL;
	}
	if (pd->buf) {
		dma_free_coherent(&dev->pcidev->dev,
				  cp->iooo_mgmr.pkt_buf_size,
				  pd->buf, pd->mapping);
		pd->buf = NULL;
	}
	if (pd->skb) {
		if (cnic_reuse_ooo_pkt(pd->skb, dev))
			CNIC_ERR("ooo_free_buf_single: Error freeing skb\n");
		pd->skb = NULL;
	}
	kfree(pd);

	return 0;
}

static void ooo_free_tx_pend(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	struct iooo_pkt_desc *pd;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &im->txr.tx_pend_pd_head.list) {
		pd = list_entry(pos, struct iooo_pkt_desc, list);
		list_del(pos);
		if (ooo_free_buf_single(dev, pd))
			CNIC_ERR("Error freeing tx pend list\n");
		im->txr.tx_pend_pd_cnt--;
	}
	if (im->txr.tx_pend_pd_cnt)
		CNIC_ERR("tx_pend_pd_cnt = %d\n", im->txr.tx_pend_pd_cnt);
}

static int ooo_free_blk(struct cnic_dev *dev, struct iooo_block *blk)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	struct list_head *pos, *q;
	struct iooo_pkt_desc *pd;

	if (test_bit(IOOO_BLK_EMPTY, &im->flags)) {
		CNIC_ERR("Freeing an empty blk list?!\n");
		return -EINVAL;
	}

	if (blk->pkt_cnt) {
		list_for_each_safe(pos, q, &blk->pd_head.list) {
			pd = list_entry(pos, struct iooo_pkt_desc, list);
			list_del(pos);
			ooo_free_buf_single(dev, pd);
			blk->pkt_cnt--;
		}
		if (blk->pkt_cnt) {
			CNIC_ERR("blk free error! pkt_cnt=%d\n", blk->pkt_cnt);
			blk->pkt_cnt = 0;
		}
	}

	im->blk_prod++;
	if (im->blk_prod >= MAX_IOOO_BLOCK_SUPPORTED)
		im->blk_prod = 0;

	im->blk_alloc[im->blk_prod] = blk->id;

	if (im->blk_cons == im->blk_prod)
		set_bit(IOOO_BLK_EMPTY, &im->flags);
	clear_bit(IOOO_BLK_FULL, &im->flags);
	return 0;
}

static void ooo_free_tx_buf(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_tx_ring_info *txr = &cp->iooo_mgmr.txr;
	u32 hw_tx_cons, sw_cons;
	struct iooo_pkt_desc *pd;

	if (!test_bit(IOOO_START_TX_FREE, &cp->iooo_mgmr.flags))
		return;

	hw_tx_cons = *txr->tx_cons_idx_ptr;
	while (hw_tx_cons != txr->tx_cons) {
		sw_cons = txr->tx_cons % BNX2_TX_DESC_CNT;
		if (sw_cons != BNX2_MAX_TX_DESC_CNT) {
			pd = txr->tx_pkt_desc[txr->tx_cons &
				(txr->tx_desc_cnt_max - 1)];
			txr->tx_pkt_desc[txr->tx_cons &
				(txr->tx_desc_cnt_max - 1)] = NULL;
			txr->tx_desc_cnt++;
			if (pd)
				ooo_free_buf_single(dev, pd);
		}
		txr->tx_cons++;
	}
}

static void ooo_free_all_buf(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	int i;

	for (i = 0; i < MAX_OOO_RX_DESC_CNT; i++)
		if (im->rxr.rx_pkt_desc[i]) {
			ooo_free_buf_single(dev, im->rxr.rx_pkt_desc[i]);
			im->rxr.rx_pkt_desc[i] = NULL;
		}

	for (i = 0; i < MAX_IOOO_BLOCK_SUPPORTED; i++)
		if (im->blk[i].pkt_cnt)
			ooo_free_blk(dev, &im->blk[i]);

	for (i = 0; i < MAX_OOO_TX_DESC_CNT; i++)
		if (im->txr.tx_pkt_desc[i]) {
			ooo_free_buf_single(dev, im->txr.tx_pkt_desc[i]);
			im->txr.tx_pkt_desc[i] = NULL;
		}

	ooo_free_tx_pend(dev);
}

/* Whenever the rxbd's prod - cons < 1/2 MAX, replenish
   gfp presents ATOMIC vs. KERNEL | COMP (sleep-able) */
static int ooo_alloc_buf_single(struct cnic_dev *dev, int gfp,
				 struct iooo_pkt_desc **pd, int len)
{
	*pd = kmalloc(sizeof(struct iooo_pkt_desc), gfp);
	if (*pd == NULL) {
		CNIC_ERR("Failed to alloc rx pkt_desc\n");
		return -ENOMEM;
	}

	(*pd)->buf = NULL;
	if (test_bit(CNIC_F_BNX2_CLASS, &dev->flags)) {
		(*pd)->buf = dma_alloc_coherent(&dev->pcidev->dev, len,
						&(*pd)->mapping, gfp);
		if (!(*pd)->buf) {
			CNIC_ERR("Failed to alloc rx buf\n");
			kfree(*pd);
			return -ENOMEM;
		}
	}

	(*pd)->skb = NULL;

	return 0;
}

static int ooo_alloc_blk(struct iooo_mgmt *im)
{
	int ret;

	if (test_bit(IOOO_BLK_FULL, &im->flags))
		return MAX_IOOO_BLOCK_SUPPORTED;

	ret = im->blk_alloc[im->blk_cons];

	im->blk[ret].pkt_cnt = 0;

	im->blk_cons++;
	if (im->blk_cons >= MAX_IOOO_BLOCK_SUPPORTED)
		im->blk_cons = 0;
	if (im->blk_cons == im->blk_prod)
		set_bit(IOOO_BLK_FULL, &im->flags);
	clear_bit(IOOO_BLK_EMPTY, &im->flags);
	return ret;
}

static int ooo_alloc_rx_buf(struct cnic_dev *dev, int gfp)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_rx_ring_info *rxr = &cp->iooo_mgmr.rxr;
	struct iooo_pkt_desc *pd = NULL;
	struct bnx2_rx_bd *rxbd;
	u32 ring, sw_rx_prod, want;
	int ret = 0, cnt = 0;

	want = rxr->rx_desc_cnt_max - rxr->rx_desc_cnt -
	       (rxr->rx_desc_cnt_max / BNX2_RX_DESC_CNT);

	while (want > cnt) {
		ring = (rxr->rx_prod & (rxr->rx_desc_cnt_max - 1)) /
			BNX2_RX_DESC_CNT;
		sw_rx_prod = rxr->rx_prod % BNX2_RX_DESC_CNT;
		rxbd = &rxr->rx_desc_ring[ring][sw_rx_prod];
		if (sw_rx_prod != BNX2_MAX_RX_DESC_CNT) {
			if ((gfp != GFP_KERNEL) &&
			    (rxr->rx_desc_cnt >= want >> 1))
				goto done;

			ret = ooo_alloc_buf_single(dev, gfp, &pd,
						   cp->iooo_mgmr.pkt_buf_size);
			if (ret)
				goto done;

			rxr->rx_prod_bseq += cp->iooo_mgmr.pkt_buf_size;
			rxr->rx_pkt_desc[rxr->rx_prod &
				(rxr->rx_desc_cnt_max - 1)] = pd;
			rxbd->rx_bd_haddr_hi = (u64) pd->mapping >> 32;
			rxbd->rx_bd_haddr_lo = (u64) pd->mapping & 0xffffffff;
			rxr->rx_desc_cnt++;
			cnt++;
		}
		rxr->rx_prod++;
	}
done:
	CNIC_WR16(dev, rxr->rx_bidx_addr, rxr->rx_prod);
	CNIC_WR(dev, rxr->rx_bseq_addr, rxr->rx_prod_bseq);
	return ret;
}

static void ooo_free_ring(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_tx_ring_info *txr = &cp->iooo_mgmr.txr;
	struct iooo_rx_ring_info *rxr = &cp->iooo_mgmr.rxr;
	int i;

	for (i = 0; i < rxr->rx_max_ring; i++) {
		if (rxr->rx_desc_ring[i]) {
			dma_free_coherent(&dev->pcidev->dev,
					BNX2_PAGE_SIZE,
					rxr->rx_desc_ring[i],
					rxr->rx_desc_mapping[i]);
			rxr->rx_desc_ring[i] = NULL;
		}
	}
	for (i = 0; i < txr->tx_max_ring; i++) {
		if (txr->tx_desc_ring[i]) {
			dma_free_coherent(&dev->pcidev->dev,
					BNX2_PAGE_SIZE,
					txr->tx_desc_ring[i],
					txr->tx_desc_mapping[i]);
			txr->tx_desc_ring[i] = NULL;
		}
	}
}

static int ooo_alloc_ring(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_tx_ring_info *txr = &cp->iooo_mgmr.txr;
	struct iooo_rx_ring_info *rxr = &cp->iooo_mgmr.rxr;
	int i;

	rxr->rx_max_ring = rxr->rx_desc_cnt_max / BNX2_RX_DESC_CNT;
	if (rxr->rx_desc_cnt_max % BNX2_RX_DESC_CNT)
		++(rxr->rx_max_ring);
	for (i = 0; i < rxr->rx_max_ring; i++) {
		rxr->rx_desc_ring[i] = dma_alloc_coherent(&dev->pcidev->dev,
					  BNX2_PAGE_SIZE,
					  &rxr->rx_desc_mapping[i],
					  GFP_KERNEL);
		if (!rxr->rx_desc_ring[i])
			goto free;
	}

	txr->tx_max_ring = txr->tx_desc_cnt_max / BNX2_TX_DESC_CNT;
	if (txr->tx_desc_cnt_max % TX_DESC_CNT)
		++(txr->tx_max_ring);
	for (i = 0; i < txr->tx_max_ring; i++) {
		txr->tx_desc_ring[i] = dma_alloc_coherent(&dev->pcidev->dev,
					  BNX2_PAGE_SIZE,
					  &txr->tx_desc_mapping[i],
					  GFP_KERNEL);
		if (!txr->tx_desc_ring[i])
			goto free;
	}
	return 0;

free:
	ooo_free_ring(dev);
	return -ENOMEM;
}

static void ooo_init_rings(struct iooo_mgmt *im)
{
	struct iooo_rx_ring_info *rxr = &im->rxr;
	struct iooo_tx_ring_info *txr = &im->txr;
	struct bnx2_tx_bd *txbd;
	u32 next;
	int i, j;

	for (i = 0; i < rxr->rx_max_ring; i++) {
		next = i + 1;
		if (next >= rxr->rx_max_ring)
			next = 0;
		for (j = 0; j < BNX2_MAX_RX_DESC_CNT; j++)
			cnic_set_bnx2_rxbd(&rxr->rx_desc_ring[i][j],
					   im->pkt_buf_size,
					   (dma_addr_t) NULL);
		cnic_set_bnx2_rxbd(&rxr->rx_desc_ring[i][j],
				   BNX2_PAGE_SIZE,
				   rxr->rx_desc_mapping[next]); 
	}
	for (i = 0; i < txr->tx_max_ring; i++) {
		next = i + 1;
		if (next >= txr->tx_max_ring)
			next = 0;
		for (j = 0; j < BNX2_MAX_TX_DESC_CNT; j++) {
			txbd = &txr->tx_desc_ring[i][j];
			txbd->tx_bd_vlan_tag_flags = TX_BD_FLAGS_START |
						     TX_BD_FLAGS_END;
		}
		txbd = &txr->tx_desc_ring[i][BNX2_MAX_TX_DESC_CNT];
		txbd->tx_bd_haddr_hi =
				(u64) txr->tx_desc_mapping[next] >> 32;
		txbd->tx_bd_haddr_lo =
				(u64) txr->tx_desc_mapping[next] & 0xffffffff;
	}
}

/* Actual placement of the pkt to the txbd */
static int ooo_send(struct iooo_tx_ring_info *txr,
		    struct iooo_pkt_desc *pd)
{
	struct bnx2_tx_bd *txbd;
	u32 ring, sw_tx_prod;
	int i;

	dma_addr_t txpd_mapping;
	if (!txr->tx_desc_cnt)
		return -ENOMEM;

	for (i = 0; i < 2; i++) {
		ring = (txr->tx_prod & (txr->tx_desc_cnt_max - 1)) /
			BNX2_TX_DESC_CNT;
		sw_tx_prod = txr->tx_prod % BNX2_TX_DESC_CNT;
		txbd = &txr->tx_desc_ring[ring][sw_tx_prod];
		if (sw_tx_prod != BNX2_MAX_TX_DESC_CNT) {
			txr->tx_pkt_desc[txr->tx_prod &
				(txr->tx_desc_cnt_max - 1)] = pd;
			txpd_mapping = pd->mapping + BNX2_RX_OFFSET;
			txbd->tx_bd_mss_nbytes = pd->pkt_len;
			txbd->tx_bd_haddr_hi = (u64) txpd_mapping >> 32;
			txbd->tx_bd_haddr_lo = (u64) txpd_mapping & 0xffffffff;
			txr->tx_prod_bseq += pd->pkt_len;
			txr->tx_prod++;
			txr->tx_desc_cnt--;
			txr->tx_total_pkt_sent++;
			return 0;
		} else
			txr->tx_prod++;
	}
	return 0;
}

static int ooo_send_bnx2x(struct cnic_dev *dev, struct iooo_tx_ring_info *txr,
			  struct iooo_pkt_desc *pd)
{
	int ret;

	ret = cnic_send_ooo_pkt(pd->skb, dev);
	if (NETDEV_TX_OK == ret) {
		txr->tx_total_pkt_sent++;
		/* Once sent, cnic no longer owns the skb */
		pd->skb = NULL;
		ret = 0;
	} else
		CNIC_ERR("send_ooo ret=%d\n", ret);

	ooo_free_buf_single(dev, pd);

	return ret;
}

static void ooo_send_pend(struct cnic_dev *dev, struct iooo_tx_ring_info *txr)
{
	struct iooo_pkt_desc *pd;
	struct list_head *pos, *q;
	int cnt = 0;

	if (list_empty(&txr->tx_pend_pd_head.list)) {
		if (txr->tx_pend_pd_cnt)
			CNIC_ERR("pend cnt out of sync=%d\n",
				txr->tx_pend_pd_cnt);
		return;
	}
	list_for_each_safe(pos, q, &txr->tx_pend_pd_head.list) {
		pd = list_entry(pos, struct iooo_pkt_desc, list);
		list_del(pos);
		if (test_bit(CNIC_F_BNX2_CLASS, &dev->flags))
			ooo_send(txr, pd);
		else
			ooo_send_bnx2x(dev, txr, pd);

		dev->ooo_tx_count++;
		txr->tx_pend_pd_cnt--;
		cnt++;
	}
	if (test_bit(CNIC_F_BNX2_CLASS, &dev->flags) && cnt) {
		CNIC_WR16(dev, txr->tx_bidx_addr, txr->tx_prod);
		CNIC_WR(dev, txr->tx_bseq_addr, txr->tx_prod_bseq);
	}
}

static void ooo_send_pkt(struct iooo_tx_ring_info *txr,
			 struct iooo_pkt_desc *pd)
{
	list_add_tail(&pd->list, &txr->tx_pend_pd_head.list);
	txr->tx_pend_pd_cnt++;
}

static int ooo_send_blk(struct iooo_tx_ring_info *txr,
			struct iooo_block *blk)
{
	struct iooo_pkt_desc *pd;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &blk->pd_head.list) {
		pd = list_entry(pos, struct iooo_pkt_desc, list);
		list_del(pos);
		ooo_send_pkt(txr, pd);
		blk->pkt_cnt--;
	}
	return blk->pkt_cnt;
}

static void ooo_engine(struct cnic_dev *dev, struct iooo_pkt_desc *pkt_desc,
		       u32 l5_cid)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	struct cnic_iscsi *iscsi = ctx->proto.iscsi;
	struct iooo_block *pen = &iscsi->pen;
	struct iooo_block *blk, *blk_nxt;
	struct list_head *pos, *q;
	int blk_idx, new_blk_idx;
/*
	printk("cnic: cid=0x%x op %d blk %d dblk %d size %d bcnt=%d\n",
		im->hsi.iscsi_cid, im->hsi.opcode, im->hsi.blk_idx,
		im->hsi.drop_blk_idx, im->hsi.drop_size,
		iscsi->blk_cnt);
*/
	if (im->hsi.drop_size) {
		if (!im->hsi.drop_blk_idx || list_empty(&pen->list)) {
			CNIC_ERR("drop_blk_idx=%d or list empty\n",
				im->hsi.drop_blk_idx);
			goto orphan;
		}
		blk_idx = 1;
		list_for_each_safe(pos, q, &pen->list) {
			if (im->hsi.drop_blk_idx == blk_idx++) {
				blk = list_entry(pos, struct iooo_block, list);
				ooo_free_blk(dev, blk);
				iscsi->blk_cnt--;
				im->hsi.drop_blk_idx++;
				list_del(pos);
				if (!(--im->hsi.drop_size))
					break;
			}
		}
	}

	blk = NULL;
	switch (im->hsi.opcode) {
	case OOO_OPCODE_ADD_RIGHT:
		blk_idx = 1;
		list_for_each_entry(blk, &pen->list, list) {
			if (im->hsi.blk_idx == blk_idx++)
				break;
		}
		if (list_empty(&pen->list) || im->hsi.blk_idx != --blk_idx) {
			CNIC_ERR("can't find block to add right to!\n");
			goto orphan;
		}
		list_add_tail(&pkt_desc->list, &blk->pd_head.list);
		blk->pkt_cnt++;
		break;

	case OOO_OPCODE_ADD_LEFT:
		blk_idx = 1;
		list_for_each_entry(blk, &pen->list, list) {
			if (im->hsi.blk_idx == blk_idx++)
				break;
		}
		if (list_empty(&pen->list) || im->hsi.blk_idx != --blk_idx) {
			CNIC_ERR("can't find block to add left to!\n");
			goto orphan;
		}
		list_add(&pkt_desc->list, &blk->pd_head.list);
		blk->pkt_cnt++;
		break;

	case OOO_OPCODE_ADD_NEW:
		new_blk_idx = ooo_alloc_blk(im);
		if (MAX_IOOO_BLOCK_SUPPORTED == new_blk_idx) {
			CNIC_ERR("max blk reached!\n");
			goto orphan;
		}
		/* Find blk to add to */
		if (im->hsi.blk_idx == 1)
			blk = pen;
		else {
			blk_idx = 2;
			list_for_each_entry(blk, &pen->list, list) {
				if (im->hsi.blk_idx == blk_idx++)
					break;
			}
		}
		list_add(&im->blk[new_blk_idx].list, &blk->list);
		iscsi->blk_cnt++;

		/* Attach pkt to blk */
		blk = &im->blk[new_blk_idx];
		list_add(&pkt_desc->list, &blk->pd_head.list);
		blk->pkt_cnt++;
		break;

	case OOO_OPCODE_JOIN:
		if (!im->hsi.blk_idx) {
			if (list_empty(&pen->list)) {
				CNIC_ERR("can't find block to join 0!\n");
				goto orphan;
			}
			blk = list_first_entry(&pen->list, struct iooo_block,
				list);
			list_add(&pkt_desc->list, &blk->pd_head.list);
			blk->pkt_cnt++;
			if (ooo_send_blk(&im->txr, blk)) {
				CNIC_ERR("blk sent err! pkt_cnt=%d\n",
					blk->pkt_cnt);
				blk->pkt_cnt = 0;
			}
			ooo_free_blk(dev, blk);
			iscsi->blk_cnt--;
			list_del(&blk->list);
		} else {
			blk_idx = 1;
			blk = blk_nxt = NULL;
			list_for_each_entry(blk, &pen->list, list) {
				if (im->hsi.blk_idx == blk_idx++) {
					blk_nxt = list_entry(blk->list.next,
							     struct iooo_block, list);
					break;
				}
			}
			if (!blk || !blk_nxt || blk_nxt == pen) {
				CNIC_ERR("can't find block to join!\n");
				goto orphan;
			}
			list_add_tail(&pkt_desc->list, &blk->pd_head.list);
			blk->pkt_cnt++;
			/* Append all the pkts from the nxt blk to this blk */
			list_for_each_safe(pos, q, &blk_nxt->pd_head.list) {
				list_move_tail(pos, &blk->pd_head.list);
				blk->pkt_cnt++;
				blk_nxt->pkt_cnt--;
			}
			ooo_free_blk(dev, blk_nxt);
			iscsi->blk_cnt--;
			list_del(&blk_nxt->list);
		}
		break;

	case OOO_OPCODE_ADD_PEN:
		ooo_send_pkt(&im->txr, pkt_desc);
		break;

	default:
		break;
	}

	return;

orphan:
	if (pkt_desc)
		ooo_free_buf_single(dev, pkt_desc);
}

static void ooo_handle_rx_event(struct cnic_dev *dev,
			       struct iooo_pkt_desc *pd)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	u32 l5_cid;

	/* For 0 len placement, just free the pkt */
	if (unlikely(!pd->pkt_len)) {
		ooo_free_buf_single(dev, pd);
		return;
	}

	if (cnic_get_l5_cid(cp, im->hsi.iscsi_cid, &l5_cid) == 0) {
		if (l5_cid >= MAX_CM_SK_TBL_SZ) {
			CNIC_ERR("bad l5_cid=%d\n", l5_cid);
			ooo_free_buf_single(dev, pd);
		} else
			ooo_engine(dev, pd, l5_cid);
	} else {
		CNIC_ERR("get l5_cid failed\n");
		ooo_free_buf_single(dev, pd);
	}
}

static void cnic_handle_bnx2_ooo_rx_event(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	struct iooo_pkt_desc *pkt_desc;
	struct bnx2_ooo_fhdr *pkt_hsi;
	u16 hw_rx_cons, sw_cons;

	/* Process only if ready to start handler */
	if (unlikely(!test_bit(IOOO_START_HANDLER, &im->flags)))
		return;

	/* Handle RX placement */
	hw_rx_cons = *im->rxr.rx_cons_idx_ptr;
	while (hw_rx_cons != im->rxr.rx_cons) {
		sw_cons = im->rxr.rx_cons % BNX2_RX_DESC_CNT;
		if (sw_cons != BNX2_MAX_RX_DESC_CNT) {
			pkt_desc = im->rxr.rx_pkt_desc[im->rxr.rx_cons &
				(im->rxr.rx_desc_cnt_max - 1)];
			if (!pkt_desc) {
				CNIC_ERR("pkt_desc = NULL?! rx_cons=%d\n",
					im->rxr.rx_cons &
					(im->rxr.rx_desc_cnt_max - 1));
				goto out;
			}
			im->rxr.rx_pkt_desc[im->rxr.rx_cons &
				(im->rxr.rx_desc_cnt_max - 1)] = NULL;

			pkt_hsi = (struct bnx2_ooo_fhdr *)pkt_desc->buf;
			pkt_desc->pkt_len = pkt_hsi->pkt_len;
			im->hsi.iscsi_cid = pkt_hsi->icid;
			im->hsi.opcode = pkt_hsi->opcode;
			im->hsi.blk_idx = pkt_hsi->blk_idx;
			im->hsi.drop_size = pkt_hsi->drop_size;
			im->hsi.drop_blk_idx = pkt_hsi->drop_blk_idx;

			ooo_handle_rx_event(dev, pkt_desc);

			im->rxr.rx_desc_cnt--;
		}
out:
		im->rxr.rx_cons++;
	}

	/* If already stopped, keep processing the rx queue but do not alloc
	   more buffers */

	if (unlikely(!test_bit(IOOO_START, &im->flags)))
		return;

	ooo_alloc_rx_buf(dev, GFP_ATOMIC);
}

static void cnic_handle_bnx2_ooo_tx_event(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	u16 hw_cons, sw_cons;

	if (unlikely(!test_bit(IOOO_START_HANDLER, &im->flags)))
		return;

	/* Handle fwd ring tx completion */
	hw_cons = *im->txr.tx_cons_idx_ptr;
	sw_cons = im->txr.tx_cons;
	if (sw_cons == hw_cons && !im->txr.tx_pend_pd_cnt)
		return;

	/* Must wait for at least 1 tx completion before attempting to free */
	if (!test_bit(IOOO_START_TX_FREE, &cp->iooo_mgmr.flags) &&
	    im->txr.tx_desc_cnt < (im->txr.tx_desc_cnt_max >> 1))
		set_bit(IOOO_START_TX_FREE, &cp->iooo_mgmr.flags);

	spin_lock(&im->lock);
	ooo_free_tx_buf(dev);
	spin_unlock(&im->lock);

	if (unlikely(!test_bit(IOOO_START, &im->flags)))
		return;

	ooo_send_pend(dev, &im->txr);
}

static void cnic_handle_bnx2x_ooo_rx_event(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	struct iooo_pkt_desc *pkt_desc;
	struct iooo_hsi_bnx2x *pkt_hsi;
	struct cnic_ooo_cqe ooo_cqe;
	int ret;
	u16 hw_cons, sw_cons;

	/* Process only if ready to start */
	if (unlikely(!test_bit(IOOO_START, &im->flags)))
		return;

	hw_cons = *im->rxr.rx_cons_idx_ptr;
	sw_cons = im->rxr.rx_cons;
	if (sw_cons == hw_cons)
		return;

	im->rxr.rx_cons = hw_cons;

	ooo_cqe.cqe_type = 0xffffffff;
	/* Handle the rx cqe */
	do {
		ret = cnic_get_ooo_cqe(dev, &ooo_cqe);
		if (ret < 0) {
			CNIC_ERR("ERROR at retrieving OOO CQE\n");
			goto error;
		}
		if (0xffffffff == ooo_cqe.cqe_type)
			goto empty;
		else if (OOO_BD_CQE != ooo_cqe.cqe_type) {
			CNIC_ERR("OOO CQE type=%d!\n", ooo_cqe.cqe_type);
			goto reuse;
		}
		if (!ooo_alloc_buf_single(dev, GFP_ATOMIC, &pkt_desc, 0)) {
			pkt_desc->skb = ooo_cqe.u.cqe.pkt_desc;
			pkt_desc->pkt_len = pkt_desc->skb->len;
			pkt_hsi = (struct iooo_hsi_bnx2x *)
				   ooo_cqe.u.cqe.raw_data;
			/* Must mask out for port identifier (bit 23) */
			im->hsi.iscsi_cid = BNX2X_SW_CID(pkt_hsi->iscsi_cid);
			im->hsi.opcode = pkt_hsi->opcode;
			im->hsi.blk_idx = pkt_hsi->blk_idx;
			im->hsi.drop_size = pkt_hsi->drop_size;
			im->hsi.drop_blk_idx = pkt_hsi->drop_blk_idx;

			ooo_handle_rx_event(dev, pkt_desc);

		} else {
			CNIC_ERR("Failed to allocate pk desc!\n");
reuse:
			cnic_reuse_ooo_pkt(ooo_cqe.u.cqe.pkt_desc, dev);
			return;
		}
	} while (ret);
error:
empty:
	/* Send any tx pending pkt */
	ooo_send_pend(dev, &im->txr);

	return;
}

static void cnic_handle_bnx2x_ooo_tx_event(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	u16 hw_cons, sw_cons;

	/* Handle fwd ring tx completion */
	hw_cons = *im->txr.tx_cons_idx_ptr;
	sw_cons = im->txr.tx_cons;
	if (sw_cons == hw_cons)
		return;

	im->txr.tx_cons = hw_cons;

	/* Handle tx completion by sending event to bnx2x */
	if (unlikely(cp->ethdev->drv_state & CNIC_DRV_STATE_NO_ISCSI_OOO))
		return;

	cnic_comp_ooo_tx_pkts(dev);

	/* Lastly, send any tx pending pkt */
	if (unlikely(!test_bit(IOOO_START, &im->flags)))
		return;

	ooo_send_pend(dev, &im->txr);
}

static void cnic_alloc_bnx2_ooo_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	int ret;

	/* Chip specific */
	cp->iooo_mgmr.rxr.rx_desc_cnt_max = MAX_BNX2_OOO_RX_DESC_CNT;
	cp->iooo_mgmr.txr.tx_desc_cnt_max = MAX_BNX2_OOO_TX_DESC_CNT;
	cp->iooo_mgmr.pkt_buf_size = dev->netdev->mtu + ETH_HLEN +
				     BNX2_RX_OFFSET + 8;
	/* General */
	ooo_init(&cp->iooo_mgmr);

	if (BNX2_CHIP(cp) != BNX2_CHIP_5709)
		return;

	ret = ooo_alloc_ring(dev);
	if (!ret)
		set_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags);
}

static void cnic_alloc_bnx2x_ooo_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	/* General */
	ooo_init(&cp->iooo_mgmr);
	set_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags);
}

static void cnic_stop_bnx2_ooo_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;
	int cnt = 10;
	struct kwqe *wqes[1], l2kwqe;

	if (!(test_bit(IOOO_START, &cp->iooo_mgmr.flags)))
		return;

	clear_bit(IOOO_START_HANDLER, &im->flags);

	/* Send kwqe to clean up the L2 OOO rx ring */
	memset(&l2kwqe, 0, sizeof(l2kwqe));
	wqes[0] = &l2kwqe;
	l2kwqe.kwqe_op_flag = (L2_LAYER_CODE << KWQE_LAYER_SHIFT) |
			      (L2_KWQE_OPCODE_VALUE_FLUSH <<
			       KWQE_OPCODE_SHIFT) | RX_CATCHUP_CID;
	dev->submit_kwqes(dev, wqes, 1);

	/* Wait for the hardware indexes to match producer */
	while (*im->rxr.rx_cons_idx_ptr && cnt) {
		barrier();
		msleep(10);
		cnt--;
	}

	if (!cnt)
		CNIC_ERR("hw rx_cons=%d\n", *im->rxr.rx_cons_idx_ptr);

	clear_bit(IOOO_START, &im->flags);
}

static void cnic_stop_bnx2x_ooo_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iooo_mgmt *im = &cp->iooo_mgmr;

	/* Nothing to do here as the free_ooo_resc gets called in stop_hw */
	clear_bit(IOOO_START, &im->flags);
}

static void cnic_start_bnx2_ooo_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct status_block *sblk = (struct status_block *)
		((unsigned long)cp->status_blk.gen & PAGE_MASK);
	struct iooo_rx_ring_info *rxr = &cp->iooo_mgmr.rxr;
	struct iooo_tx_ring_info *txr = &cp->iooo_mgmr.txr;
	u32 val;

	if (!(test_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags)))
		return;

	rxr->rx_cid_addr = GET_CID_ADDR(RX_CATCHUP_CID);
	rxr->rx_bidx_addr = MB_GET_CID_ADDR(RX_CATCHUP_CID) +
			    L5_KRNLQ_HOST_QIDX;
	rxr->rx_bseq_addr = MB_GET_CID_ADDR(RX_CATCHUP_CID) +
			    L5_KRNLQ_HOST_FW_QIDX;
	rxr->rx_prod = 0;
	rxr->rx_prod_bseq = 0;
	rxr->rx_cons = 0;

	rxr->rx_cons_idx_ptr = (u16 *) (&sblk->status_rx_quick_consumer_index1);
	cnic_init_bnx2_rx_ring_start(dev, RX_CATCHUP_CID, rxr->rx_cons_idx_ptr,
				     rxr->rx_desc_mapping[0], 0);
	if (*rxr->rx_cons_idx_ptr)
		CNIC_ERR("stale hw rx_cons=%d\n", *rxr->rx_cons_idx_ptr);

	txr->tx_cid_addr = GET_CID_ADDR(TX_CATCHUP_CID);
	txr->tx_bidx_addr = MB_GET_CID_ADDR(TX_CATCHUP_CID) +
			    BNX2_L2CTX_TX_HOST_BIDX;
	txr->tx_bseq_addr = MB_GET_CID_ADDR(TX_CATCHUP_CID) +
			    BNX2_L2CTX_TX_HOST_BSEQ;
	txr->tx_cons_idx_ptr = (u16 *) (&sblk->status_tx_quick_consumer_index1);
	txr->tx_prod = 0;
	txr->tx_prod_bseq = 0;
	txr->tx_cons = 0;

	cnic_init_bnx2_tx_ring_start(dev, TX_CATCHUP_CID,
				     txr->tx_desc_mapping[0]); 

	ooo_init_rings(&cp->iooo_mgmr);

	val = BNX2_L2CTX_CMD_TYPE_TYPE_L2 | (TX_OOO_EST_NBD << 16);
	cnic_ctx_wr(dev, txr->tx_cid_addr, BNX2_L2CTX_CMD_TYPE_XI, val);

	/* Allocate rx buf, no tx buf yet */
	rxr->rx_desc_cnt = 0;
	txr->tx_desc_cnt = txr->tx_desc_cnt_max - txr->tx_desc_cnt_max /
			   BNX2_TX_DESC_CNT;
	ooo_alloc_rx_buf(dev, GFP_KERNEL);
	set_bit(IOOO_START, &cp->iooo_mgmr.flags);
}

static void cnic_start_bnx2x_ooo_hw(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct bnx2x *bp = netdev_priv(dev->netdev);
	struct iooo_rx_ring_info *rxr = &cp->iooo_mgmr.rxr;
	struct iooo_tx_ring_info *txr = &cp->iooo_mgmr.txr;
	int i;

	if (!(test_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags)))
		return;

	for (i = 0; i < MAX_OOO_RX_DESC_CNT; i++)
		rxr->rx_pkt_desc[i] = NULL;
	for (i = 0; i < MAX_OOO_TX_DESC_CNT; i++)
		txr->tx_pkt_desc[i] = NULL;

	if (BNX2X_CHIP_IS_E2_PLUS(bp)) {
		struct host_hc_status_block_e2 *sb = cp->status_blk.gen;

		txr->tx_cons_idx_ptr =
			&sb->sb.index_values[HC_INDEX_FWD_TX_CQ_CONS];
		rxr->rx_cons_idx_ptr =
			&sb->sb.index_values[HC_INDEX_OOO_RX_CQ_CONS];
	} else {
		struct host_hc_status_block_e1x *sb = cp->status_blk.gen;

		txr->tx_cons_idx_ptr =
			&sb->sb.index_values[HC_INDEX_FWD_TX_CQ_CONS];
		rxr->rx_cons_idx_ptr =
			&sb->sb.index_values[HC_INDEX_OOO_RX_CQ_CONS];
	}
	rxr->rx_cons = *rxr->rx_cons_idx_ptr;
	txr->tx_cons = *txr->tx_cons_idx_ptr;

	set_bit(IOOO_START, &cp->iooo_mgmr.flags);
	if (cp->ethdev->drv_state & CNIC_DRV_STATE_NO_ISCSI_OOO)
		clear_bit(IOOO_START, &cp->iooo_mgmr.flags);
}

static void cnic_free_ooo_resc(struct cnic_dev *dev)
{
	struct cnic_local *cp = dev->cnic_priv;

	if (!(test_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags)))
		return;

	clear_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags);
	ooo_free_all_buf(dev);
	ooo_free_ring(dev);
}

static void cnic_conn_ooo_init(struct cnic_local *cp, u32 l5_cid)
{
	struct cnic_context *ctx = &cp->ctx_tbl[l5_cid];
	struct cnic_iscsi *iscsi = ctx->proto.iscsi;

	iscsi->blk_cnt = 0;
	iscsi->pen.pkt_cnt = 0;
	INIT_LIST_HEAD(&iscsi->pen.list);
	INIT_LIST_HEAD(&iscsi->pen.pd_head.list);
}

/* Flush the associated iooo_block for the connection specified */
static void cnic_flush_ooo(struct cnic_dev *dev, u32 l5_cid)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct cnic_context *ctx;
	struct cnic_iscsi *iscsi;
	struct list_head *pos, *q;
	struct iooo_block *blk;

	spin_lock_bh(&cp->iooo_mgmr.lock);
	if (l5_cid >= MAX_CM_SK_TBL_SZ)
		goto skip;

	ctx = &cp->ctx_tbl[l5_cid];
	if (!ctx)
		goto skip;

	iscsi = ctx->proto.iscsi;

	if (!iscsi->blk_cnt)
		goto skip;

	list_for_each_safe(pos, q, &iscsi->pen.list) {
		blk = list_entry(pos, struct iooo_block, list);
		ooo_free_blk(dev, blk);
		iscsi->blk_cnt--;
		list_del(pos);
	}
	if (iscsi->blk_cnt) {
		CNIC_ERR("blk cnt=%d != 0\n", iscsi->blk_cnt);
		iscsi->blk_cnt = 0;
	}
skip:
	if (test_bit(CNIC_F_BNX2_CLASS, &dev->flags))
		ooo_free_tx_buf(dev);

	clear_bit(IOOO_START_TX_FREE, &cp->iooo_mgmr.flags);
	spin_unlock_bh(&cp->iooo_mgmr.lock);
}

static void cnic_bnx2_ooo_iscsi_conn_update(struct cnic_dev *dev,
					    struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iscsi_kwqe_conn_update *req =
		(struct iscsi_kwqe_conn_update *) kwqe;
	u32 l5_cid;
	struct cnic_context *ctx;

	l5_cid = req->reserved2;
	if (l5_cid >= MAX_ISCSI_TBL_SZ)
		return;

	ctx = &cp->ctx_tbl[l5_cid];
	ctx->cid = req->context_id << 7;

	if (!(test_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags)))
		return;

	if (test_bit(CNIC_F_ISCSI_OOO_ENABLE, &dev->flags)) {
		cnic_reg_wr_ind(dev, BNX2_RXP_SCRATCH_OOO_RX_CID,
				GET_CID_ADDR(RX_CATCHUP_CID));
		cnic_reg_wr_ind(dev, BNX2_RXP_SCRATCH_OOO_FLAGS,
				BNX2_IOOO_FLAGS_OVERRIDE |
				BNX2_IOOO_FLAGS_ENABLE);
	}
}

static void cnic_bnx2_ooo_iscsi_destroy(struct cnic_dev *dev, struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iscsi_kwqe_conn_destroy *req =
		(struct iscsi_kwqe_conn_destroy *) kwqe;
	u32 l5_cid = req->reserved0;
	struct cnic_context *ctx;

	if (l5_cid >= MAX_ISCSI_TBL_SZ)
		return;

	ctx = &cp->ctx_tbl[l5_cid];
	ctx->cid = 0;
}

static void cnic_bnx2x_ooo_iscsi_conn_update(struct cnic_dev *dev,
					     struct kwqe *kwqe)
{
	struct cnic_local *cp = dev->cnic_priv;
	struct iscsi_kwqe_conn_update *req =
		(struct iscsi_kwqe_conn_update *) kwqe;

	if (!(test_bit(IOOO_RESC_AVAIL, &cp->iooo_mgmr.flags)))
		return;

	if (test_bit(CNIC_F_ISCSI_OOO_ENABLE, &dev->flags) &&
	    !(cp->ethdev->drv_state & CNIC_DRV_STATE_NO_ISCSI_OOO))
		req->conn_flags =
			(req->conn_flags &
			 ~ISCSI_KWQE_CONN_UPDATE_OOO_SUPPORT_MODE) |
			(TCP_TSTORM_OOO_SUPPORTED <<
			 ISCSI_KWQE_CONN_UPDATE_OOO_SUPPORT_MODE_SHIFT);
}

#endif  /* CNIC_ISCSI_OOO_SUPPORT */

static void cnic_release(void)
{
	struct cnic_dev *dev;

	while (!list_empty(&cnic_dev_list)) {
		dev = list_entry(cnic_dev_list.next, struct cnic_dev, list);
		if (test_bit(CNIC_F_CNIC_UP, &dev->flags))
			cnic_stop_hw(dev);

		cnic_unregister_netdev(dev);
		list_del_init(&dev->list);
		cnic_free_dev(dev);
	}
}

static int __init cnic_init(void)
{
	int rc = 0;
#if defined (__VMKLNX__)
	struct net_device *dev;
#endif  /* defined (__VMKLNX__) */

	INIT_LIST_HEAD(&cnic_dev_list);

	printk(KERN_INFO "%s", version);

	cnic_wq = create_singlethread_workqueue("cnic_wq");
	if (!cnic_wq) {
		return -ENOMEM;
	}

#if defined (__VMKLNX__)
	rtnl_lock();

	/* Find Teton devices */
#if (LINUX_VERSION_CODE >= 0x020618)
	for_each_netdev(&init_net, dev)
#elif (LINUX_VERSION_CODE >= 0x20616)
	for_each_netdev(dev)
#else
	for (dev = dev_base; dev; dev = dev->next)
#endif
	{
		struct cnic_dev *cdev;

		/* No netdev notifier in VMWare, so we find all Broadcom
		 * devices here and assume they will remain statically
		 * configured.
		 */
		cdev = is_cnic_dev(dev);
		if (cdev && (dev->flags & IFF_UP)) {
			set_bit(CNIC_F_IF_UP, &cdev->flags);
			/* Register with net driver once and start hw.
			 * If using MSI-X, we'll request IRQ below in
			 * cnic_start_hw().  During cnic unload or
			 * CNIC_CTL_STOP_CMD, we'll call cnic_stop_hw().
			 * IRQ will only be freed during cnic unload to
			 * prevent context mismatch in VMWare.
			 */
			if (cnic_register_netdev(cdev) == 0)
				cnic_start_hw(cdev);
		}
	}

	rtnl_unlock();

#if ((VMWARE_ESX_DDK_VERSION >= 50000) && !defined (CNIC_INBOX) || \
     (VMWARE_ESX_DDK_VERSION >= 55000) && defined (CNIC_INBOX))
	register_netdevice_notifier(&cnic_netdev_notifier_esx);
#endif	/* (VMWARE_ESX_DDK_VERSION >= 50000) && !defined (CNIC_INBOX) */
#else  /* !defined (__VMKLNX__) */
	rc = register_inetaddr_notifier(&cnic_ip_notifier);
	if (rc)
		cnic_release();
	rc = register_netdevice_notifier(&cnic_netdev_notifier);
	if (rc) {
		unregister_inetaddr_notifier(&cnic_ip_notifier);
		cnic_release();
	}
#endif  /* defined (__VMKLNX__) */

	return rc;
}

static void __exit cnic_exit(void)
{
#if !defined (__VMKLNX__)
	unregister_inetaddr_notifier(&cnic_ip_notifier);
	unregister_netdevice_notifier(&cnic_netdev_notifier);
#elif ((VMWARE_ESX_DDK_VERSION >= 50000) && !defined (CNIC_INBOX)) || \
       ((VMWARE_ESX_DDK_VERSION >= 55000) && defined (CNIC_INBOX))
	cnic_register_release_all_callbacks();
	unregister_netdevice_notifier(&cnic_netdev_notifier_esx);
#endif  /* !defined (__VMKLNX__) */
	cnic_release();
	destroy_workqueue(cnic_wq);
}

module_init(cnic_init);
module_exit(cnic_exit);
