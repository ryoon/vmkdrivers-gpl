/* Copyright (C) 2010 - 2013 Broadcom Corporation.
 * Portions Copyright (C) VMware, Inc. 2007-2013. All Rights Reserved.
 */

#define TG3_NETQ_WAIT_EVENT_TIMEOUT	msecs_to_jiffies(100)
#define TG3_VRQ_CHANGE_TIMEOUT_US	10000
#define TG3_VRQ_FLUSH_TIMEOUT_US	50000
#define TG3_VRQ_MAX_NUM_TX_QS(tp) \
	(tg3_flag((tp), ENABLE_TSS) ? (tp)->irq_cnt - 2 : 0)
#define TG3_VRQ_MAX_NUM_RX_QS(tp)	(tp->irq_cnt - 1)

static void tg3_vmware_timer(struct tg3 *tp)
{
	/*
	 * If the driver says this NIC is in promiscuous mode, the hardware
	 * better agree with it. See PR 88299 for more details, but there
	 * are cases (eg. bcm5704 rev10) that the set promiscuous bit is
	 * lost for no reason and this seems to be the least intrusive
	 * workaround for that issue (likely a hardware bug).
	 *
	 * This is happening on other cards as well (BCM5700 rev04) PR 97750
	 * Also, on some cards (5703) we have seen other bits getting messed up
	 */
	if (netif_carrier_ok(tp->dev)) {
		u32 rx_mode = tr32(MAC_RX_MODE);
		if (!(rx_mode & RX_MODE_PROMISC) && (rx_mode != tp->rx_mode)) {
			/*
			 * We love to warn the users every time there is such a
			 * register reset, but we do not want to do it forever
			 * given some NICs are really that bad (PR 105488).
			 */
			if (tp->vmware.rx_mode_reset_counter < 200) {
				DP(TG3_MSG_HW, "rx_mode "
					    "0x%x(%s)=>0x%x",
					    rx_mode,
				       rx_mode & RX_MODE_PROMISC ? "on" : "off",
					    tp->rx_mode);
				tp->vmware.rx_mode_reset_counter++;
			}
			tw32_f(MAC_RX_MODE, tp->rx_mode);
		}
	}

	/*
	 * we've seen ssome 5703's (rev 10) which don't seen to
	 * generate an interrupt on link-up state changes. bug 89197.
	 */
	if (!netif_carrier_ok(tp->dev) &&
	    !(tg3_flag(tp, USE_LINKCHG_REG)) &&
	    !(tg3_flag(tp, POLL_SERDES))) {
		struct tg3_hw_status *sblk = tp->napi[0].hw_status;
		if (sblk->status & SD_STATUS_LINK_CHG) {
			sblk->status = SD_STATUS_UPDATED |
			       (sblk->status & ~SD_STATUS_LINK_CHG);
			tg3_setup_phy(tp, 0);
		}
	}
}

static netdev_features_t tg3_vmware_tune_tso(struct tg3 *tp,
					     netdev_features_t features)
{
	/* VMWare does not have skb_gso_segment() to workaround TSO_BUG */
	if (tg3_flag(tp, TSO_BUG))
		features &= ~(NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_TSO_ECN);

        if (disable_tso) {
                features &= ~(NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_TSO_ECN);
                tg3_flag_clear(tp, TSO_CAPABLE);
                tg3_flag_clear(tp, TSO_BUG);
        }

	return features;
}

/* The following debug buffers and exported routines are used by GDB to access
 * * tg3 hardware registers when doing live debug over serial port. */
#define DBG_BUF_SZ  128

static u32 tg3_dbg_buf[DBG_BUF_SZ];

void tg3_dbg_read32(struct net_device *dev, u32 off, u32 len)
{
	struct tg3 *tp = netdev_priv(dev);
	u32 *buf = tg3_dbg_buf;

	memset(tg3_dbg_buf, 0, sizeof(tg3_dbg_buf));

	if (off & 0x3) {
		len += off & 0x3;
		off &= ~0x3;
	}

	if (off >= TG3_REG_BLK_SIZE)
		return;

	if (len & 0x3)
		len = (len + 3) & ~3;

	if (len > DBG_BUF_SZ)
		len = DBG_BUF_SZ;

	if (off + len > TG3_REG_BLK_SIZE)
		len = TG3_REG_BLK_SIZE - off;

	while (len > 0) {
		*buf = tr32(off);
		buf++;
		off += 4;
		len -= 4;
	}
}
EXPORT_SYMBOL(tg3_dbg_read32);

void tg3_dbg_write32(struct net_device *dev, u32 off, u32 val)
{
	struct tg3 *tp = netdev_priv(dev);

	if (off & 0x3)
		return;

	tw32(off, val);
}
EXPORT_SYMBOL(tg3_dbg_write32);

#if !defined(TG3_VMWARE_BMAPILNX_DISABLE)
static int
tg3_vmware_ioctl_cim(struct net_device *dev, struct ifreq *ifr)
{
	struct tg3 *tp = netdev_priv(dev);
	void __user *useraddr = ifr->ifr_data;
	struct brcm_vmware_ioctl_req req;
	int rc = 0;
	u32 val;

	if (copy_from_user(&req, useraddr, sizeof(req))) {
		DP_ERR("could not copy from user tg3_ioctl_req\n");
		return -EFAULT;
	}

	switch (req.cmd) {
	case BRCM_VMWARE_CIM_CMD_ENABLE_NIC:
		DP(TG3_MSG_HW, "enable NIC\n");

		rc = tg3_open(tp->dev);
		break;
	case BRCM_VMWARE_CIM_CMD_DISABLE_NIC:
		DP(TG3_MSG_HW, "disable NIC\n");

		rc = tg3_close(tp->dev);
		break;
	case BRCM_VMWARE_CIM_CMD_REG_READ: {
		struct brcm_vmware_ioctl_reg_read_req *rd_req;

		rd_req = &req.cmd_req.reg_read_req;
		if (0x7ffc < rd_req->reg_offset) {
			DP_ERR("%s: "
			       "out of range: req reg: 0x%x\n",
			       "reg read", rd_req->reg_offset);
			rc = -EINVAL;
			break;
		}

		if (rd_req->reg_offset & 0x3) {
			DP_ERR("%s:offset not dword aligned: req reg: 0x%x\n",
			       "reg read", rd_req->reg_offset);
			rc = -EINVAL;
			break;
		}

		switch (rd_req->reg_access_type) {
		case BRCM_VMWARE_REG_ACCESS_DIRECT:
			val = tr32(rd_req->reg_offset);
			break;
		case BRCM_VMWARE_REG_ACCESS_PCI_CFG:
			pci_read_config_dword(tp->pdev,
					      rd_req->reg_offset, &val);
			break;
		default:
			DP_ERR("%s:invalid access method: access type: 0x%x\n",
				   "reg read",
				   rd_req->reg_access_type);
			rc = -EINVAL;
			break;
		}

		req.cmd_req.reg_read_req.reg_value = val;
		DP(TG3_MSG_HW, "%s: reg: 0x%x value:0x%x",
			    "reg read", rd_req->reg_offset,
			    rd_req->reg_value);

		break;
	} case BRCM_VMWARE_CIM_CMD_REG_WRITE: {
		struct brcm_vmware_ioctl_reg_write_req *wr_req;

		wr_req = &req.cmd_req.reg_write_req;
		if (0x7ffc < wr_req->reg_offset) {
			DP_ERR("%s:out of range: req reg: 0x%x\n",
			       "reg write", wr_req->reg_offset);
			rc = -EINVAL;
			break;
		}

		if (wr_req->reg_offset & 0x3) {
			DP_ERR("%s:offset not dword aligned: req reg: 0x%x\n",
			       "reg write", wr_req->reg_offset);
			rc = -EINVAL;
			break;
		}

		switch (wr_req->reg_access_type) {
		case BRCM_VMWARE_REG_ACCESS_DIRECT:
			tw32(wr_req->reg_offset, wr_req->reg_value);
			break;
		case BRCM_VMWARE_REG_ACCESS_PCI_CFG:
			pci_write_config_dword(tp->pdev, wr_req->reg_offset,
					       wr_req->reg_value);
			break;
		default:
			DP_ERR("%s:invalid access method: access type: 0x%x\n",
			       "reg write", wr_req->reg_access_type);
			rc = -EINVAL;
			break;
		}

		DP(TG3_MSG_HW, "%s: reg: 0x%x value:0x%x",
			       "reg write", wr_req->reg_offset,
			       wr_req->reg_value);

		break;
	} case BRCM_VMWARE_CIM_CMD_GET_NIC_PARAM:
		DP(TG3_MSG_HW, "get NIC param\n");

		req.cmd_req.get_nic_param_req.mtu = dev->mtu;
		memcpy(req.cmd_req.get_nic_param_req.current_mac_addr,
		       dev->dev_addr,
		       sizeof(req.cmd_req.get_nic_param_req.current_mac_addr));
		break;
	case BRCM_VMWARE_CIM_CMD_GET_NIC_STATUS:
		DP(TG3_MSG_HW, "get NIC status\n");

		req.cmd_req.get_nic_status_req.nic_status = netif_running(dev);
		break;
	default:
		DP_ERR("unknown req.cmd: 0x%x\n", req.cmd);
		rc = -EINVAL;
	}

	if (rc == 0 &&
	    copy_to_user(useraddr, &req, sizeof(req))) {
		DP_ERR("couldn't copy to user tg3_ioctl_req\n");
		return -EFAULT;
	}

	return rc;
}
#endif  /* TG3_VMWARE_BMAPILNX */

#ifdef TG3_VMWARE_NETQ_ENABLE
static void tg3_vmware_fetch_stats(struct tg3 *tp)
{
	int i;
	u32 addr;
	struct tg3_napi *tnapi = &tp->napi[0];

	tnapi->netq.stats.rx_bytes_hw   += tr32(0x9d0);
	tnapi->netq.stats.rx_errors_hw  += tr32(0x9d4);
	tnapi->netq.stats.rx_packets_hw += tr32(0x9d8);

	for (i = 1, addr = 0xa00; i < 16; i++, addr += 0x20) {
		tnapi = &tp->napi[i];

		tnapi->netq.stats.tx_bytes         += tr32(addr + 0x00);
		tnapi->netq.stats.tx_ucast_packets += tr32(addr + 0x04);
		tnapi->netq.stats.tx_mcast_packets += tr32(addr + 0x08);
		tnapi->netq.stats.tx_bcast_packets += tr32(addr + 0x0c);

		tnapi->netq.stats.rx_bytes_hw   += tr32(addr + 0x10);
		tnapi->netq.stats.rx_errors_hw  += tr32(addr + 0x14);
		tnapi->netq.stats.rx_packets_hw += tr32(addr + 0x18);
	}
}

static void tg3_set_prod_bdinfo(struct tg3 *tp, u32 bdinfo_addr,
				dma_addr_t mapping, u32 maxlen_flags)
{
	tw32(bdinfo_addr + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_HIGH,
	     ((u64) mapping >> 32));
	tw32(bdinfo_addr + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_LOW,
	     ((u64) mapping & 0xffffffff));
	tw32(bdinfo_addr + TG3_BDINFO_MAXLEN_FLAGS, maxlen_flags);
	/* Leave the nic addr field alone */
}

static void tg3_rx_prod_rcb_disable(struct tg3 *tp, u32 bdinfo_addr)
{
	tw32(bdinfo_addr + TG3_BDINFO_MAXLEN_FLAGS, BDINFO_FLAGS_DISABLED);
	tw32(bdinfo_addr + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_HIGH, 0);
	tw32(bdinfo_addr + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_LOW, 0);
}

static void tg3_disable_prod_rcbs(struct tg3 *tp, u32 ring)
{
	struct tg3_rx_prodring_set *tpr;
	u32 offset;

	if (!(tg3_flag(tp, IOV_CAPABLE)))
		return;

	tpr = &tp->napi[ring].prodring;
	offset = RCVDBDI_JMB_BD_RING1 + (ring - 1) * 2 * TG3_BDINFO_SIZE;

	/* Disable the jumbo ring */
	tg3_rx_prod_rcb_disable(tp, offset);
	offset += TG3_BDINFO_SIZE;

	/* Disable the standard ring */
	tg3_rx_prod_rcb_disable(tp, offset);
}

static void tg3_setup_prod_rcbs(struct tg3 *tp, u32 ring)
{
	struct tg3_rx_prodring_set *tpr;
	u32 offset;

	if (!(tg3_flag(tp, ENABLE_IOV)))
		return;

	tpr = &tp->napi[ring].prodring;
	offset = RCVDBDI_JMB_BD_RING1 + (ring - 1) * 2 * TG3_BDINFO_SIZE;

	if (tg3_flag(tp, JUMBO_RING_ENABLE)) {
		tg3_set_prod_bdinfo(tp, offset, tpr->rx_jmb_mapping,
		       (TG3_RX_JMB_MAX_SIZE_5717 << BDINFO_FLAGS_MAXLEN_SHIFT) |
			      BDINFO_FLAGS_USE_EXT_RECV);
		tpr->rx_jmb_prod_idx = tp->rx_jumbo_pending;
		tw32_rx_mbox(tpr->rx_jmb_mbox, tpr->rx_jmb_prod_idx);
	}

	offset += TG3_BDINFO_SIZE;

	tg3_set_prod_bdinfo(tp, offset, tpr->rx_std_mapping,
	(TG3_RX_STD_MAX_SIZE_5717 << BDINFO_FLAGS_MAXLEN_SHIFT) |
		(TG3_RX_STD_DMA_SZ << 2));
	tpr->rx_std_prod_idx = tp->rx_pending;
	tw32_rx_mbox(tpr->rx_std_mbox, tpr->rx_std_prod_idx);
}

static void tg3_setup_prod_mboxes(struct tg3 *tp, u32 ring)
{
	struct tg3_rx_prodring_set *tpr = &tp->napi[ring].prodring;

	if (!ring) {
		tpr->rx_std_mbox = TG3_RX_STD_PROD_IDX_REG;
		tpr->rx_jmb_mbox = TG3_RX_JMB_PROD_IDX_REG;
		return;
	}

	tpr->rx_std_mbox = MAILBOX_RCV_STD_PROD_IDX_RING1 + (ring - 1) * 4;
	if (ring % 2)
		tpr->rx_std_mbox -= 4;
	else
		tpr->rx_std_mbox += 4;

	if (ring < 12)
		tpr->rx_jmb_mbox = MAILBOX_RCV_JUMBO_PROD_IDX_RING1 +
				   (ring - 1) * 4;
	else
		tpr->rx_jmb_mbox = MAILBOX_RCV_JMB_PROD_IDX_RING12 +
				   (ring - 12) * 4;

	if (ring % 2)
		tpr->rx_jmb_mbox -= 4;
	else
		tpr->rx_jmb_mbox += 4;
}

static int
tg3_netq_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);

	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	if (tg3_flag(tp, ENABLE_TSS))
		args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = TG3_VRQ_MAX_NUM_RX_QS(tp);

		DP(TG3_MSG_NETQ, "Using %d RX NetQ rings\n", args->count);

		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = TG3_VRQ_MAX_NUM_TX_QS(tp);

		DP(TG3_MSG_NETQ, "Using %d TX NetQ rings\n", args->count);

		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		DP_ERR("Counting queue: invalid queue type\n");

		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

#define RX_FILTERS_PER_NETQ  4

static void
tg3_netq_init_rx_filters(struct tg3 *tp)
{
	int rx_filters_per_q;
	int total_rx_filters, i;
	struct netq_rx_filter *rx_filter;

	INIT_LIST_HEAD(&tp->vmware.netq.free_filters_list);

	if (multi_rx_filters == 0) {
		/* Only support 1 Mac filter per queue */
		DP(TG3_MSG_NETQ, "Multiple RX filters disabled forced to "
				     "use only 1 filter.\n");
		rx_filters_per_q = 1;
	} else {
#if (VMWARE_ESX_DDK_VERSION >= 55000)
		rx_filters_per_q = RX_FILTERS_PER_NETQ;
#else
		/* used static distribution */
		rx_filters_per_q = (TG3_NETQ_MAX_MACADDR - 1) /
					TG3_VRQ_MAX_NUM_RX_QS(tp);
		/* tg3 can't support more than 4 filters per queue */
		rx_filters_per_q = min_t (int,
					rx_filters_per_q,
					RX_FILTERS_PER_NETQ);
#endif
		if (multi_rx_filters > 0) {
			multi_rx_filters = min_t(int,
					RX_FILTERS_PER_NETQ,
					multi_rx_filters);
			if (rx_filters_per_q < multi_rx_filters)
				DP(TG3_MSG_NETQ,
				   "Forced RX filter setting %d higher "
				   "than checked value %d\n",
				   multi_rx_filters,
				   rx_filters_per_q);

#if (VMWARE_ESX_DDK_VERSION >= 55000)
			rx_filters_per_q = multi_rx_filters;
#else
			rx_filters_per_q = min_t(int,
					  rx_filters_per_q,
					  multi_rx_filters);
#endif
		}
	}

	tp->vmware.netq.rx_filters_per_q = rx_filters_per_q ;
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	total_rx_filters = TG3_NETQ_MAX_MACADDR - 1;
#else
	total_rx_filters = rx_filters_per_q * TG3_VRQ_MAX_NUM_RX_QS(tp);
#endif
	tp->vmware.netq.total_rx_filters = total_rx_filters;

	rx_filter = tp->vmware.netq.rx_filters;
	for (i = 0; i < total_rx_filters; i++) {
		rx_filter[i].fid = i + 1;
		rx_filter[i].qid = 0;
		rx_filter[i].sid = 0;
		rx_filter[i].type = VMKNETDDI_QUEUEOPS_FILTER_NONE;
		list_add(&rx_filter[i].link,
			 &tp->vmware.netq.free_filters_list);
	}
	DP(TG3_MSG_NETQ,"# netq: %d,  filter assigned per netq: %d "
			"Total Avaiable filters: %d\n",
			TG3_VRQ_MAX_NUM_RX_QS(tp),
			rx_filters_per_q, total_rx_filters);
}

static struct netq_rx_filter *tg3_netq_alloc_rx_filter(struct tg3 *tp)
{
	struct list_head *listp;

	if (list_empty(&tp->vmware.netq.free_filters_list))
		return NULL;

	listp = (struct list_head *)tp->vmware.netq.free_filters_list.next;
	list_del_init(listp);
	return (struct netq_rx_filter *)listp;
}

static int
tg3_netq_get_filter_count(struct tg3 *tp,
		vmknetddi_queueop_get_filter_count_args_t *args)
{
	tg3_netq_init_rx_filters(tp);
	args->count = tp->vmware.netq.rx_filters_per_q;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_alloc_tx_queue(struct net_device *netdev,
			 vmknetddi_queueops_queueid_t *p_qid,
			 u16 *queue_mapping)
{
	struct tg3 *tp = netdev_priv(netdev);
	int i;

	if (tp->vmware.netq.n_tx_queues_allocated >= TG3_VRQ_MAX_NUM_TX_QS(tp))
		return VMKNETDDI_QUEUEOPS_ERR;

	for (i = 1; i < 1 + TG3_VRQ_MAX_NUM_TX_QS(tp); i++) {
		struct tg3_napi *tnapi = &tp->napi[i + 1];
		if (!(tnapi->netq.flags & TG3_NETQ_TXQ_ALLOCATED)) {
			tnapi->netq.flags |= TG3_NETQ_TXQ_ALLOCATED;
			tp->vmware.netq.n_tx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
			*queue_mapping = i;

			tw32(HOSTCC_TXCOL_TICKS_VEC1 + i * 0x18,
			     tp->coal.tx_coalesce_usecs);
			tw32(HOSTCC_TXMAX_FRAMES_VEC1 + i * 0x18,
			     tp->coal.tx_max_coalesced_frames);
			tw32(HOSTCC_TXCOAL_MAXF_INT_VEC1 + i * 0x18,
			     tp->coal.tx_max_coalesced_frames_irq);

			DP(TG3_MSG_NETQ, "TX NetQ allocated on %d\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

	DP_ERR("No free tx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
tg3_netq_alloc_rx_queue(struct net_device *netdev,
			 vmknetddi_queueops_queueid_t *p_qid,
			 struct napi_struct **napi_p)
{
	int i;
	struct tg3 *tp = netdev_priv(netdev);

	if (tp->vmware.netq.n_rx_queues_allocated >= TG3_VRQ_MAX_NUM_RX_QS(tp)) {
		DP_ERR("RX Q alloc: No queues available!\n" );
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 1; i < TG3_VRQ_MAX_NUM_RX_QS(tp) + 1; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];
		if (!(tnapi->netq.flags & TG3_NETQ_RXQ_ALLOCATED)) {
			tnapi->netq.flags |= TG3_NETQ_RXQ_ALLOCATED;
			tp->vmware.netq.n_rx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(i);
			*napi_p = &tnapi->napi;

			DP(TG3_MSG_NETQ_VERBOSE, "RX NetQ allocated on %d\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	DP_ERR("No free rx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
tg3_netq_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct tg3 *tp = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return tg3_netq_alloc_tx_queue(args->netdev, &args->queueid,
					     &args->queue_mapping);
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return tg3_netq_alloc_rx_queue(args->netdev, &args->queueid,
						&args->napi);
	} else {
		DP_ERR("Trying to alloc invalid queue type: %x\n",
			   args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static void tg3_netq_txq_free(struct tg3 *tp, int qid)
{
	struct tg3_napi *tnapi = &tp->napi[qid + 1];

	tnapi->netq.flags &= ~TG3_NETQ_TXQ_ALLOCATED;
	tp->vmware.netq.n_tx_queues_allocated--;

	/* Don't sit on tx packet completions.
	 * Send them up as soon as they are ready.
	 */
	tw32(HOSTCC_TXCOL_TICKS_VEC1 + qid * 0x18, 0);
	tw32(HOSTCC_TXMAX_FRAMES_VEC1 + qid * 0x18, 1);
	tw32(HOSTCC_TXCOAL_MAXF_INT_VEC1 + qid * 0x18, 1);
}

static int
tg3_netq_free_tx_queue(struct net_device *netdev,
		       vmknetddi_queueops_queueid_t qid)
{
	struct tg3 *tp = netdev_priv(netdev);
	struct tg3_napi *tnapi;

	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	if (index == 0 || index > TG3_VRQ_MAX_NUM_TX_QS(tp)) {
		DP_ERR("Trying to free invalid tx queue: %d\n", index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	tnapi = &tp->napi[index + 1];

	if (!(tnapi->netq.flags & TG3_NETQ_TXQ_ALLOCATED)) {
		DP_ERR("Trying to free unallocated tx queue: %d\n", index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	tg3_netq_txq_free(tp, index);

	if (tnapi->tx_cons != tnapi->tx_prod) {
		DP_ERR("Timeout submitting free NetQ TX Queue: %x\n",
			    index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	DP(TG3_MSG_NETQ, "Free NetQ TX Queue: %d\n", index);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_remove_filter(struct tg3 *tp, int fid)
{
	struct netq_rx_filter *rx_filter;
	struct tg3_napi *tnapi;
	u32 shift;

	rx_filter = &tp->vmware.netq.rx_filters[fid-1];
	if (rx_filter->sid == 0) {
		DP_ERR("invalid VRQ mapping slot detected!\n");
		return 1;
	}
	if (!rx_filter->qid) {
		DP_ERR("invalid netq queue id detected!\n");
		return 1;
	}
	tnapi = &tp->napi[rx_filter->qid];
	shift = rx_filter->sid - 1;
	if (!(tnapi->netq.filter_bitmap & (1 << shift))) {
		DP_ERR("VRQ mapping slot %d has already been cleared!\n",
			rx_filter->sid);
		return 1;
	}

	tnapi->netq.filter_bitmap &= ~(1 << shift);

	switch (rx_filter->type) {
	case VMKNETDDI_QUEUEOPS_FILTER_VLAN:
	/* fall through */
	case VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR:
		/* Disable the VLAN match term */
		tw32(MAC_VRQFLT_FLTSET + fid * 4, 0);
		tw32(MAC_VRQFLT_CFG + fid * 4, 0);
		tw32(MAC_VRQFLT_PTRN + fid * 4, 0);
		if (rx_filter->type == VMKNETDDI_QUEUEOPS_FILTER_VLAN)
			break;
	/* fall through */
	case VMKNETDDI_QUEUEOPS_FILTER_MACADDR:
		/* Remove the perfect match filter. */
		if (fid < 4) {
			tw32(MAC_ADDR_0_HIGH + (fid * 8),
					tr32(MAC_ADDR_0_HIGH));
			tw32(MAC_ADDR_0_LOW + (fid * 8),
					tr32(MAC_ADDR_0_LOW));
		} else {
			tw32(MAC_VRQ_PMATCH_HI_5 + (fid - 4) * 8,
				tr32(MAC_ADDR_0_HIGH));
			tw32(MAC_VRQ_PMATCH_LO_5 + (fid - 4) * 8,
				tr32(MAC_ADDR_0_LOW));
		}
		break;
	default:
		DP_ERR("invalid filter type (%x)\n", rx_filter->type);
		return 1;
	}

	rx_filter->type = VMKNETDDI_QUEUEOPS_FILTER_NONE;
	rx_filter->qid = 0;
	rx_filter->sid = 0;
	list_add_tail(&rx_filter->link, &tp->vmware.netq.free_filters_list);
	return 0;
}

static void
tg3_netq_disable_queue(struct tg3 *tp, int qid, bool chk_irq)
{
	int i;
	struct tg3_napi *tnapi = &tp->napi[qid];
	u32 val;

	/* Disable all h/w flush capabilities. Rely on s/w timer */
	tw32(VRQ_FLUSH_CTRL, 0);

	/* Disable the VRQ */
	tw32(MAC_VRQ_ENABLE, tr32(MAC_VRQ_ENABLE) & ~(1 << qid));
	/* Disable the VRQ mapper for this vector */
	tw32(MAC_VRQMAP_2H + qid * 8, 0);
	/* Disable the perfect match term */
	tw32(MAC_VRQMAP_1H + qid * 8, 0);

	/* wait for all packets to be drained. the assumption is driver/OS
	can continue to service incoming RX packets and interrupts during
	the wait time */
	/* Force a DMA of all remaining rx packets in this VRQ */
	tw32(HOSTCC_MODE,
	     tp->coalesce_mode | HOSTCC_MODE_ENABLE | tnapi->coal_now);

	/* There isn't a reliable way to tell that all the rx packets have
	 * made it to the host.  The status tag can change before all packets
	 * have drained.  The best we can do is wait a little bit.
	 */
	msleep(50);

	if (chk_irq) {
		/* disable interrupts */
		napi_disable(&tnapi->napi);
		tw32_mailbox_f(tnapi->int_mbox, 1);
	}

	/* safe to do RCB disable */
	tnapi->netq.flags &= ~TG3_NETQ_RXQ_ENABLED;

	tg3_disable_prod_rcbs(tp, qid);

	/* Implement HC flush, MB flush, as well as software
	VRQ flush routines. */
	tw32(HOSTCC_PARAM_SET_RESET, 1 << qid);

	tw32_rx_mbox(tnapi->prodring.rx_std_mbox, 0);
	tw32_rx_mbox(tnapi->prodring.rx_jmb_mbox, 0);
	tw32_rx_mbox(tnapi->consmbox, 0);
	tnapi->rx_rcb_ptr = 0;

	tw32(HOSTCC_RXCOL_TICKS_VEC1 + (qid - 1) * 0x18, 0);
	tw32(HOSTCC_RXMAX_FRAMES_VEC1 + (qid - 1) * 0x18, 0);
	tw32(HOSTCC_RXCOAL_MAXF_INT_VEC1 + (qid - 1) * 0x18, 0);

	val = ~(1 << qid) << 15;
	val |= VRQ_FLUSH_SW_FLUSH;
	tw32(VRQ_FLUSH_CTRL, val);

	/* Poll for acknowledgement from the hardware. */
	for (i = 0; i < TG3_VRQ_FLUSH_TIMEOUT_US / 10; i++) {
		if (!(tr32(VRQ_FLUSH_CTRL) & VRQ_FLUSH_SW_FLUSH))
			break;
		udelay(10);
	}
	if (tr32(VRQ_FLUSH_CTRL) & VRQ_FLUSH_SW_FLUSH) {
		DP_ERR("Timeout flushing hardware queue: %x\n",
			    qid);
	}

	/* Force a status block update to refresh all the producer
	 * and consumer indexes.  Also, we want to make sure we
	 * use the right tag when reenabling interrupts.
	 */
	tw32_f(HOSTCC_MODE, tp->coalesce_mode |
	       HOSTCC_MODE_ENABLE | HOSTCC_MODE_NOINT_ON_NOW |
	       tnapi->coal_now);

	for (i = 0; i < 500; i++) {
		if (tnapi->hw_status->rx_jumbo_consumer == 0 &&
		    tnapi->hw_status->rx_consumer == 0 &&
		    tnapi->hw_status->rx_mini_consumer == 0 &&
		    tnapi->hw_status->idx[0].rx_producer == 0)
			break;
		mdelay(1);
	}

	if (i == 500) {
		DP_ERR("%d: Timeout waiting for final status block update.\n",
			    qid);
	}

	tg3_rx_prodring_free(tp, &tnapi->prodring);

	tnapi->last_tag = tnapi->hw_status->status_tag;
	tnapi->last_irq_tag = tnapi->last_tag;

	tnapi->chk_msi_cnt = 0;
	tnapi->last_rx_cons = 0;
	tnapi->last_tx_cons = 0;
	if (chk_irq) {
		napi_enable(&tnapi->napi);
		tg3_int_reenable(tnapi);
	}
}

static int
tg3_netq_free_rx_queue(struct net_device *netdev,
		       vmknetddi_queueops_queueid_t qid)
{
	struct tg3 *tp = netdev_priv(netdev);
	struct tg3_napi *tnapi;

	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	if (index == 0 || index > TG3_VRQ_MAX_NUM_RX_QS(tp)) {
		DP_ERR("Trying to free invalid rx queue: %d\n", index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	tnapi = &tp->napi[index];
	if (!(tnapi->netq.flags & TG3_NETQ_RXQ_ALLOCATED)) {
		DP_ERR("Attempt to free a queue that is already free: %x\n",
			    qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (tnapi->netq.flags & TG3_NETQ_RXQ_ENABLED) {

		tg3_netq_disable_queue(tp, index, true);

#ifdef BCM_HAS_NEW_IRQ_SIG
		tg3_msi(0, tnapi);
#else
		tg3_msi(0, tnapi, 0);
#endif
	}

	tnapi->netq.flags &= ~TG3_NETQ_RXQ_ALLOCATED;
	tp->vmware.netq.n_rx_queues_allocated--;

	DP(TG3_MSG_NETQ_VERBOSE, "Free NetQ RX Queue: %x\n", index);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);
	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return tg3_netq_free_tx_queue(args->netdev, args->queueid);
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return tg3_netq_free_rx_queue(args->netdev, args->queueid);
	} else {
		DP_ERR("free netq: invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
tg3_netq_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	int qid;
	struct tg3 *tp = netdev_priv(args->netdev);

	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		if (qid > tp->irq_cnt - 2) {
			DP_ERR("Attempt to get vector for "
			       "invalid TX queue ID 0x%x\n",
				   qid);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		qid++;
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		if (qid > tp->irq_cnt - 1) {
			DP_ERR("Attempt to get vector for "
			       "invalid RX queue ID 0x%x\n",
			       qid);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	} else {
		DP_ERR("Attempt to get vector for invalid "
		       "queue type, ID 0x%x\n",
		       qid);
	}

	args->vector = tp->napi[qid].irq_vec;

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		args->napi = &tp->napi[0].napi;
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;
	} else
		return VMKNETDDI_QUEUEOPS_ERR;
}

static int
tg3_netq_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);
	struct tg3_napi *tnapi;
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DP_ERR("0X%x is not a valid QID\n", qid );
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (qid == 0 || qid > TG3_VRQ_MAX_NUM_RX_QS(tp)) {
		DP_ERR("QID 0X%x is out of range\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	if (fid > tp->vmware.netq.total_rx_filters) {
		DP_ERR("fid %d is out of range (%d)\n",
			fid, tp->vmware.netq.total_rx_filters);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	tnapi = &tp->napi[qid];

	if (!(tnapi->netq.flags & TG3_NETQ_RXQ_ENABLED) ||
	    !tnapi->netq.num_rx_filters) {
		DP_ERR("Filter %d not allocated on QID %d\n",
				fid, qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	if (tg3_netq_remove_filter(tp, fid)) {
		DP_ERR("Filter %d not removed on QID %d\n",
				fid, qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	tnapi->netq.num_rx_filters--;
	if (!tnapi->netq.num_rx_filters) {
		/* remove rx buff resources if no more filters active*/
		tg3_netq_disable_queue(tp, qid, true);

#ifdef BCM_HAS_NEW_IRQ_SIG
		tg3_msi(0, tnapi);
#else
		tg3_msi(0, tnapi, 0);
#endif
	}
	DP(TG3_MSG_NETQ_VERBOSE,
		"NetQ remove RX filter: %d on qid %d\n", fid, qid);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_set_rx_filter(struct tg3 *tp, struct netq_rx_filter *rx_filter,
		u8 *macaddr, u16 vlan_id, u8 *bitmap)
{
	int fid = rx_filter->fid;
	int qid = rx_filter->qid;
	u32 val, vrq_map_reg, mapper_shift = 0;

	for (val = 0; val < RX_FILTERS_PER_NETQ; val++) {
		if (*bitmap & (1 << val))
			continue;
		*bitmap |= (1<< val);
		rx_filter->sid = val + 1;
		mapper_shift = val * 8;
		break;
	}
	if (rx_filter->sid == 0) {
		DP_ERR("VRQ mapping slot full, can't program filter!\n");
		return 1;
	}
	if (macaddr) {
		rx_filter->mac[0] = macaddr[0];
		rx_filter->mac[1] = macaddr[1];
		rx_filter->mac[2] = macaddr[2];
		rx_filter->mac[3] = macaddr[3];
		rx_filter->mac[4] = macaddr[4];
		rx_filter->mac[5] = macaddr[5];
		/* Program the perfect match filter. */
		if (fid < 4) {
			val = (macaddr[0] << 8) | macaddr[1];
			tw32(MAC_ADDR_0_HIGH + (fid * 8), val);

			val = ((macaddr[2] << 24) | (macaddr[3] << 16) |
			       (macaddr[4] <<  8) | (macaddr[5] <<  0));
			tw32(MAC_ADDR_0_LOW + (fid * 8), val);
		} else {
			val = (macaddr[0] << 8)  | macaddr[1];
			tw32(MAC_VRQ_PMATCH_HI_5 + (fid - 4) * 8, val);

			val = (macaddr[2] << 24) | (macaddr[3] << 16) |
				  (macaddr[4] <<  8) | (macaddr[5] <<  0);
			tw32(MAC_VRQ_PMATCH_LO_5 + (fid - 4) * 8, val);
		}

		/* Tell the hardware which perfect match filter to use. */
		vrq_map_reg = MAC_VRQMAP_1H + qid * 8;
		val = tr32(vrq_map_reg);
		val &= ~(MAC_VRQMAP_1H_PTA_MASK << mapper_shift);
		val |= (MAC_VRQMAP_1H_PTA_PFEN | fid) << mapper_shift;
		tw32(vrq_map_reg, val);
	}

	vrq_map_reg = MAC_VRQMAP_2H + qid * 8;
	if (vlan_id) {
		rx_filter->vlan = vlan_id;
		val = MAC_VRQFLT_PTRN_VLANID | (vlan_id << 16);
		tw32(MAC_VRQFLT_PTRN + fid * 4, val);

		tw32(MAC_VRQFLT_FLTSET + fid * 4, 1 << fid);

		val = MAC_VRQFLT_ELEM_EN | MAC_VRQFLT_HDR_VLAN;
		tw32(MAC_VRQFLT_CFG + fid * 4, val);

		val = MAC_VRQMAP_2H_PTA_VFEN | fid;
	} else
		val = 0;

	if (macaddr && vlan_id)
		val |= MAC_VRQMAP_2H_PTA_AND;
	else
		val |= MAC_VRQMAP_2H_PTA_OR;

	val |= MAC_VRQMAP_2H_PTA_EN;
	val = (tr32(vrq_map_reg) & (~(MAC_VRQMAP_2H_PTA_MASK << mapper_shift))) |
		 (val << mapper_shift);
	tw32(vrq_map_reg, val);
	return 0;
}

static int
tg3_netq_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	u8 *macaddr = NULL;
	struct tg3_napi *tnapi;
	struct tg3 *tp = netdev_priv(args->netdev);
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	vmknetddi_queueops_filter_class_t class;
	DECLARE_MAC_BUF(mac);
	u32 val;
	u16 vlan_id = 0;
	struct netq_rx_filter *rx_filter;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DP_ERR("Invalid NetQ RX ID: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (qid == 0 || qid > TG3_VRQ_MAX_NUM_RX_QS(tp)) {
		DP_ERR("Applying filter with invalid RX NetQ %d ID\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	tnapi = &tp->napi[qid];

	if (!(tnapi->netq.flags & TG3_NETQ_RXQ_ALLOCATED)) {
		DP_ERR("RX NetQ %d not allocated\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	class = vmknetddi_queueops_get_filter_class(&args->filter);
	switch (class) {
	case VMKNETDDI_QUEUEOPS_FILTER_VLAN:
	case VMKNETDDI_QUEUEOPS_FILTER_MACADDR:
	case VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR:
		break;
	default:
		DP_ERR("Received invalid RX NetQ filter: %x\n",
			   class);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (class == VMKNETDDI_QUEUEOPS_FILTER_MACADDR ||
	    class == VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR)
		macaddr = (void *)vmknetddi_queueops_get_filter_macaddr(&args->filter);

	if (class == VMKNETDDI_QUEUEOPS_FILTER_VLAN ||
	    class == VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR)
		vlan_id = vmknetddi_queueops_get_filter_vlanid(&args->filter);
	if (!tnapi->netq.num_rx_filters) {
		/* initialize rx buff resource for the netq for the 1st time */
		/* Populate the producer rings with skbs */
		if (tg3_rx_prodring_alloc(tp, &tnapi->prodring)) {
			DP_ERR("Failed to allocate queue buffers!\n");
			return VMKNETDDI_QUEUEOPS_ERR;
		}

		/* Make the producer rings available to the hardware */
		tg3_setup_prod_rcbs(tp, qid);
	}
	if (tnapi->netq.num_rx_filters + 1 > tp->vmware.netq.rx_filters_per_q) {
		DP_ERR("number of filters per queue exceed %d\n",
				tp->vmware.netq.rx_filters_per_q);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	rx_filter = tg3_netq_alloc_rx_filter(tp);
	if (!rx_filter) {
		DP_ERR("Failed to allocate rx filter for queue #%d!\n",
			   qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	rx_filter->qid = qid;
	rx_filter->type = class;
	if (tg3_netq_set_rx_filter(tp, rx_filter, macaddr,
				vlan_id, &tnapi->netq.filter_bitmap)) {
		DP_ERR("Failed to set rx filter for queue #%d!\n",
			   qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!tnapi->netq.num_rx_filters) {
		tw32(HOSTCC_RXCOL_TICKS_VEC1 + (qid - 1) * 0x18,
		     tp->coal.rx_coalesce_usecs);
		tw32(HOSTCC_RXMAX_FRAMES_VEC1 + (qid - 1) * 0x18,
		     tp->coal.rx_max_coalesced_frames);
		tw32(HOSTCC_RXCOAL_MAXF_INT_VEC1 + (qid - 1) * 0x18,
		     tp->coal.rx_max_coalesced_frames_irq);

		/* Enable the VRQ */
		val = tr32(MAC_VRQ_ENABLE);
		tw32(MAC_VRQ_ENABLE, val | (1 << qid));
		tnapi->netq.flags |= TG3_NETQ_RXQ_ENABLED;
	}
	tnapi->netq.num_rx_filters++;
	/*  Apply RX filter code here */
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(rx_filter->fid);

	DP(TG3_MSG_NETQ_VERBOSE,
		    "NetQ set RX Filter: qid %d fid %d {%d}[%s %d]\n",
		    qid, rx_filter->fid, tnapi->netq.num_rx_filters,
		    macaddr ? print_mac(mac, macaddr) : "00:00:00:00:00:00",
		    vmknetddi_queueops_get_filter_vlanid(&args->filter));

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
tg3_netq_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	struct tg3_napi *tnapi;
	struct tg3 *tp = netdev_priv(args->netdev);
	struct net_device_stats *netstats;

	tnapi = &tp->napi[qid];
	netstats = &tnapi->netq.net_stats;

	netstats->rx_packets = tnapi->netq.stats.rx_packets_sw;
	netstats->rx_bytes = tnapi->netq.stats.rx_bytes_sw;
	netstats->rx_errors = tnapi->netq.stats.rx_errors_sw;
	netstats->rx_crc_errors = tnapi->netq.stats.rx_crc_errors;
	netstats->rx_frame_errors = tnapi->netq.stats.rx_frame_errors;
	netstats->tx_packets = tnapi->netq.stats.tx_ucast_packets +
			       tnapi->netq.stats.tx_mcast_packets +
			       tnapi->netq.stats.tx_bcast_packets;
	netstats->tx_bytes = tnapi->netq.stats.tx_bytes;

	args->stats = netstats;

	return VMKNETDDI_QUEUEOPS_OK;
}

#if (VMWARE_ESX_DDK_VERSION >= 41000)
static int tg3_netq_get_supported_feature(
	vmknetddi_queueop_get_sup_feat_args_t * args)
{
	args->features = VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE;
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC;
#endif
	return VMKNETDDI_QUEUEOPS_OK;
}
#endif /* (VMWARE_ESX_DDK_VERSION >= 41000) */

#if (VMWARE_ESX_DDK_VERSION >= 50000)
static int tg3_get_supported_filter_class(
	vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	args->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR |
		      VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
	return VMKNETDDI_QUEUEOPS_OK;
}

#if (VMWARE_ESX_DDK_VERSION >= 55000)
static int tg3_netq_get_filter_count_of_device(
	vmknetddi_queueop_get_filter_count_of_device_args_t *args)
{
	struct tg3 *tp = netdev_priv(args->netdev);

	args->filters_of_device_count = tp->vmware.netq.total_rx_filters;
	args->filters_per_queue_count = tp->vmware.netq.rx_filters_per_q;
	DP(TG3_MSG_NETQ,
	   "VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE: (%d %d)\n",
	   args->filters_of_device_count, args->filters_per_queue_count);
	return VMKNETDDI_QUEUEOPS_OK;
}

static int tg3_netq_realloc_queue_with_attr(
		vmknetddi_queueop_realloc_queue_with_attr_args_t *args)
{
	int i, ret;
	vmknetddi_queueop_alloc_queue_with_attr_args_t *alloc_args = 
							args->alloc_args;
	struct tg3 *tp = netdev_priv(alloc_args->netdev);

	if (alloc_args->nattr) {
		DP_ERR("nattr is non-zero!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < args->rm_filter_count; i++) {
		ret = tg3_netq_remove_rx_filter((args->rm_filters_args + i));
		if (ret != VMKNETDDI_QUEUEOPS_OK) {
			DP_ERR("failed removing rx filters!\n");
			return ret;
		}
	}
	ret = tg3_netq_free_rx_queue(alloc_args->netdev, alloc_args->queueid);
	if (ret == VMKNETDDI_QUEUEOPS_OK) {
		ret = tg3_netq_alloc_rx_queue(alloc_args->netdev,
					&alloc_args->queueid,
					&alloc_args->napi);
		if (ret == VMKNETDDI_QUEUEOPS_OK)
			ret = tg3_netq_apply_rx_filter(args->apply_rx_filter_args);
		DP(TG3_MSG_NETQ, "REALLOC of queue #%d to %d successfully!\n",
				args->rm_filters_args[0].queueid,
				alloc_args->queueid);
	}
	return ret;
}
#endif /*(VMWARE_ESX_DDK_VERSION >= 55000)*/
#endif /*(VMWARE_ESX_DDK_VERSION >= 50000)*/

static int
tg3_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	int rc;
	struct tg3 *tp;

	if (op == VMKNETDDI_QUEUEOPS_OP_GET_VERSION)
		return vmknetddi_queueops_version(
			(vmknetddi_queueop_get_version_args_t *)args);

	tp = netdev_priv(((vmknetddi_queueop_get_features_args_t *)args)->netdev);

	if (!(tg3_flag(tp, USING_MSIX)) ||
	    !(tg3_flag(tp, INIT_COMPLETE)) ||
	    tp->irq_cnt < 2) {
		DP_ERR("Device not ready for netq ops!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	mutex_lock(&tp->vmware.netq_lock);
	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
		rc = tg3_netq_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		rc = tg3_netq_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		rc = tg3_netq_get_filter_count(tp,
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		rc = tg3_netq_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		rc = tg3_netq_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		rc = tg3_netq_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		rc = tg3_netq_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		rc = tg3_netq_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		rc = tg3_netq_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		rc = tg3_netq_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		break;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR:
		rc = VMKNETDDI_QUEUEOPS_ERR;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		rc = tg3_netq_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		break;

#if (VMWARE_ESX_DDK_VERSION >= 50000)
	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS:
		rc = tg3_get_supported_filter_class(
			(vmknetddi_queueop_get_sup_filter_class_args_t *)args);
		break;
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	case VMKNETDDI_QUEUEOPS_OP_REALLOC_QUEUE_WITH_ATTR:
		rc = tg3_netq_realloc_queue_with_attr(
		  (vmknetddi_queueop_realloc_queue_with_attr_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE:
		rc = tg3_netq_get_filter_count_of_device(
		  (vmknetddi_queueop_get_filter_count_of_device_args_t *)args);
		break;
#endif
#endif
#endif
	/*  Unsupported for now */
	default:
		DP_ERR("Unhandled NETQUEUE OP %d\n", op);
		rc = VMKNETDDI_QUEUEOPS_ERR;
	}

	mutex_unlock(&tp->vmware.netq_lock);
	return rc;
}

static void tg3_netq_init(struct tg3 *tp)
{
	struct tg3_netq_napi *tnetq;

	tp->msg_enable |= (TG3_MSG_NETQ | TG3_MSG_HW);
	mutex_init(&tp->vmware.netq_lock);

	if (tg3_netq_force[tp->vmware.netq.index] >= 0) {
		tp->rxq_req = tg3_netq_force[tp->vmware.netq.index] + 1;

		tp->txq_req = tg3_netq_force[tp->vmware.netq.index];
		tp->txq_req = min(tp->txq_req, tp->txq_max);
		tp->txq_req = max_t(u32, tp->txq_req, 1);
	}

	if (!tg3_netq_force[tp->vmware.netq.index])
		return;

	tnetq = &tp->napi[0].netq;
	tnetq->flags |= TG3_NETQ_RXQ_ALLOCATED | TG3_NETQ_RXQ_ENABLED;

	tnetq->flags |= TG3_NETQ_TXQ_ALLOCATED;
	tnetq = &tp->napi[1].netq;
	tnetq->flags |= TG3_NETQ_TXQ_ALLOCATED;

	VMKNETDDI_REGISTER_QUEUEOPS(tp->dev, tg3_netqueue_ops);
	DP(TG3_MSG_NETQ, "VMware NetQueue Ops is registered\n");
}

static int tg3_netq_remove_all_filters(struct tg3 *tp,
				struct tg3_napi *tnapi, int qid)
{
	int j, fid;
	struct netq_rx_filter *rx_filter = tp->vmware.netq.rx_filters; 

	if (tnapi->netq.num_rx_filters) {
		for (j = 0;
		     j < tp->vmware.netq.total_rx_filters;
		     j++) {
			if (rx_filter[j].qid != qid)
				continue;
			fid = rx_filter[j].fid;
			if (tg3_netq_remove_filter(tp,fid))
				DP_ERR("Filter %d not removed on QID %d\n",
					fid, qid);
			else {
				tnapi->netq.num_rx_filters--;
				DP(TG3_MSG_NETQ,
				   "NetQ remove RX filter: %d on qid %d\n",
				   fid, qid);
			}
			if (!tnapi->netq.num_rx_filters)
				/* removed all rx filters on this netq */
				return 0;
		}
		/* can't find all the filters for this netq, something is wrong */
		return 1;
	}
	/* no rx filters need to be removed for this netq */
	return 0;
}

static void tg3_netq_free_all_qs(struct tg3 *tp)
{
	int i;

	mutex_lock(&tp->vmware.netq_lock);
	for (i = 1; i < tp->irq_max; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];

		if (i != 1 && (tnapi->netq.flags & TG3_NETQ_TXQ_ALLOCATED))
			tg3_netq_txq_free(tp, i - 1);

		if (tnapi->netq.flags & TG3_NETQ_RXQ_ALLOCATED) {
			if (tnapi->netq.flags & TG3_NETQ_RXQ_ENABLED) {	
				if (!tg3_netq_remove_all_filters(tp, tnapi, i))
					tg3_netq_disable_queue(tp, i, false);
				else {
					DP_ERR("filter active "
					       "when freeing netq (%d %d)\n",
					       i, tnapi->netq.num_rx_filters);
					continue;
				}
			}
			tnapi->netq.flags &= ~TG3_NETQ_RXQ_ALLOCATED;
			tp->vmware.netq.n_rx_queues_allocated--;
			DP(TG3_MSG_NETQ, "Free NetQ RX Queue: %x\n", i); 
		}
	}
	mutex_unlock(&tp->vmware.netq_lock);
}

static void tg3_netq_invalidate_state(struct tg3 *tp)
{
	if (!(tg3_flag(tp, ENABLE_IOV)))
		return;

	tg3_netq_free_all_qs(tp);
	/* note the invalidate call below causes OS stack to send down     */
	/* requests to free the filters/queues that are already freed from */ 
        /* the tg3_netq_free_all_qs() above. */
	DP(TG3_MSG_NETQ, "called vmknetddi_queueops_invalidate_state\n");
	vmknetddi_queueops_invalidate_state(tp->dev);
}

static void tg3_netq_restore(struct tg3 *tp)
{
	if (!(tg3_flag(tp, IOV_CAPABLE)))
		return;

	/* Enable the VRQs */
	tw32(MAC_VRQ_ENABLE, MAC_VRQ_ENABLE_DFLT_VRQ);
}

static void tg3_netq_limit_dflt_queue_counts(struct tg3 *tp)
{
	if (!tg3_flag(tp, IOV_CAPABLE))
		return;

	/* If the number of rx and tx queues was not formally
	 * requested, artificially cap the number of queues
	 * to ease system resource consumption.
	 */
	if (!tp->rxq_req) {
		/* Allocated 7 queues (6 + 1) by default. */
		tp->rxq_cnt = min_t(u32, tp->rxq_cnt + 1, 7);
	}

	if (!tp->txq_req) {
		tp->txq_cnt = min_t(u32, tp->txq_cnt, 6);
	}
}

static u32 tg3_netq_tune_vector_count(struct tg3 *tp)
{
	u32 irqcnt = max(tp->rxq_cnt, tp->txq_cnt);

	if (irqcnt > 1 && tp->txq_cnt > tp->rxq_cnt - 1)
		irqcnt++;

	return irqcnt;
}

static int tg3_netq_stats_size(struct tg3 *tp)
{
	int size = TG3_NUM_STATS;

	if (!tg3_flag(tp, ENABLE_IOV))
		return size;

	size += ARRAY_SIZE(tg3_vmware_ethtool_stats_keys) * tp->irq_cnt;

	return size;
}

static void tg3_netq_stats_get_strings(struct tg3 *tp, u8 *buf)
{
	int i;

	for (i = 0; i < tp->irq_cnt; i++) {
		sprintf(buf, "[%d]: rx_packets (sw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_packets (hw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_bytes (sw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_bytes (hw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_errors (sw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_errors (hw)", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_crc_errors", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: rx_frame_errors", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: tx_bytes", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: tx_ucast_packets", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: tx_mcast_packets", i);
		buf += ETH_GSTRING_LEN;
		sprintf(buf, "[%d]: tx_bcast_packets", i);
		buf += ETH_GSTRING_LEN;
	}
}

static void tg3_netq_stats_get(struct tg3 *tp, u64 *tmp_stats)
{
	int i;

	if (!tg3_flag(tp, ENABLE_IOV))
		return;

	/* Copy over the NetQ specific statistics */
	for (i = 0; i < tp->irq_cnt; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];

		memcpy(tmp_stats, &tnapi->netq.stats,
		       sizeof(struct tg3_netq_stats));
		tmp_stats += TG3_NETQ_NUM_STATS;
	}
}

static void tg3_netq_stats_clear(struct tg3 *tp)
{
	int i;

	for (i = 0; i < tp->irq_max; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];

		memset(&tnapi->netq.stats, 0,
		       sizeof(struct tg3_netq_stats));
	}
}
#endif /* TG3_VMWARE_NETQ_ENABLE */

#if (defined(__VMKLNX__) && VMWARE_ESX_DDK_VERSION >= 55000)

static void tg3_ape_dump(struct tg3 *tp, u32 reg_offset,
	u32 *dest_addr,	u32 word_cnt)
{
	u32 *dst = dest_addr;
	u32 i;

	for (i = 0; i < word_cnt; i++)	{
		*dst = tg3_ape_read32(tp, reg_offset);
		dst++;
		reg_offset += 4;
	}
}

static void tg3_reg_dump(struct tg3 *tp, u32 reg_offset,
	u32 *dest_addr,	u32 word_cnt)
{
	u32 *dst = dest_addr;
	u32 i;

	for (i = 0; i < word_cnt; i++)	{
		*dst = tp->read32(tp, reg_offset);
		dst++;
		reg_offset += 4;
	}
}

static void tg3_ape_scratchpad_dump(struct tg3 *tp, u32 reg_offset,
	u32 *dest_addr,	u32 word_cnt)
{
	int result;
	result = tg3_ape_scratchpad_read(tp, dest_addr, reg_offset, word_cnt*4);
	if (result != 0)
		memset(&dest_addr[reg_offset/4], 0, word_cnt * 4);
}

static void tg3_phyreg_dump(struct tg3 *tp, u32 reg_offset,
	u32 *dest_addr,	u32 word_cnt)
{
	u32 *dst = dest_addr;
	u32 i;

	for (i = 0; i < word_cnt; i++)	{
		if  (tg3_readphy(tp, reg_offset, dst) != 0)
			*dst = 0xDEADBEEF;
		dst++;
		reg_offset++;
	}
}

static VMK_ReturnStatus tg3_fwdmp_callback(void *cookie, vmk_Bool liveDump)
{
	VMK_ReturnStatus status = VMK_OK;
	u32 idx, total_dmp_size;
	u32 *dst, chksum;
	struct chip_core_dmp_t *dmp;
	struct tg3 *tp;

	for (idx = 0; idx < TG3_MAX_NIC; idx++) {
		if (tg3_fwdmp_va_ptr && fwdmp_tp_ptr[idx]) {
			/* dump chip information to buffer */
			tp = fwdmp_tp_ptr[idx];
			dmp = (struct chip_core_dmp_t *)tg3_fwdmp_va_ptr;
#define CUR_DMP_SIZ(ptr)  ((u8 *)(ptr) - (u8 *)tg3_fwdmp_va_ptr)
			snprintf(dmp->fw_hdr.name, sizeof(dmp->fw_hdr.name),
				"%s", tp->dev->name);
			dmp->fw_hdr.tp = (void *)tp;
			dmp->fw_hdr.chip_id = tg3_asic_rev(tp);
			dmp->fw_hdr.len = sizeof(struct fw_dmp_hdr);
			dmp->fw_hdr.ver = 0x000700006;
			dst = dmp->fw_dmp_buf;

			if (tg3_asic_rev(tp) <= ASIC_REV_5906) {
#define FWDMP_LEGACY_REG_DUMP_SIZ			TG3_REG_BLK_SIZE
			/* 1a. A legacy register dump for older chip */
				*dst++ = FWDMP_MARKER;
				*dst++ = 0;   /* reserved */
				*dst++ = FWDMP_LEGACY_REG_DUMP;
				/* proceed if we have sufficient space */
				if (CUR_DMP_SIZ(dst+1+FWDMP_LEGACY_REG_DUMP_SIZ/4) <
					TG3_FWDMP_SIZE) {
					*dst++ = FWDMP_LEGACY_REG_DUMP_SIZ;
					memset(dst, 0xa5, FWDMP_LEGACY_REG_DUMP_SIZ);
					tg3_dump_legacy_regs(tp, dst);
					dst += FWDMP_LEGACY_REG_DUMP_SIZ/4;
				} else	{
					*dst++ = 0;   /* indicate there's no data */
				}
			} else {
			/* 1b. A full register dump from 0x0000 to 0x7c00. */
#define FWDMP_REG_DUMP_SIZ		0x7c00
				*dst++ = FWDMP_MARKER;
				*dst++ = 0;   /* reserved */
				*dst++ = FWDMP_REG_DUMP;
				/* proceed if we have sufficient space */
				if (CUR_DMP_SIZ(dst+1+FWDMP_REG_DUMP_SIZ/4) <
					TG3_FWDMP_SIZE) {
					*dst++ = FWDMP_REG_DUMP_SIZ;
					tg3_reg_dump(tp, 0, dst, FWDMP_REG_DUMP_SIZ/4);
					dst += FWDMP_REG_DUMP_SIZ/4;
				} else	{
					*dst++ = 0;   /* indicate there's no data */
				}
			}
			/* 2. A register dump from 0x0000 to 0x5000 from
				tp->aperegs.*/
#define FWDMP_APE_DUMP_SIZ		0x5000
			if (tg3_flag(tp, ENABLE_APE)) {
				*dst++ = FWDMP_MARKER;
				*dst++ = 0;   /* reserved */
				*dst++ = FWDMP_APE_DUMP;
				/* proceed if we have sufficient space */
				if (CUR_DMP_SIZ(dst+1+FWDMP_APE_DUMP_SIZ/4) <
					TG3_FWDMP_SIZE) {
					*dst++ = FWDMP_APE_DUMP_SIZ;
					tg3_ape_dump(tp, 0, dst,
						FWDMP_APE_DUMP_SIZ/4);
					dst += FWDMP_APE_DUMP_SIZ/4;
				} else {
					/* indicate there's no data */
					*dst++ = 0;
				}
			}
			/* 3. ape scratchpad dump from
				offset 0x0000 to 0x18000. */
#define FWDMP_SCRATCHPAD_DUMP_SIZ		0x18000
			if (tg3_flag(tp, ENABLE_APE) &&
				tg3_flag(tp, APE_HAS_NCSI)) {
				*dst++ = FWDMP_MARKER;
				*dst++ = 0;   /* reserved */
				*dst++ = FWDMP_SCRATCHPAD_DUMP;
				/* proceed if we have sufficient space */
				if (CUR_DMP_SIZ(dst+1+
					FWDMP_SCRATCHPAD_DUMP_SIZ/4) <
					TG3_FWDMP_SIZE){
					*dst++ = FWDMP_SCRATCHPAD_DUMP_SIZ;
					tg3_ape_scratchpad_dump(tp, 0, dst,
						FWDMP_SCRATCHPAD_DUMP_SIZ/4);
					dst += FWDMP_SCRATCHPAD_DUMP_SIZ/4;
				} else {
					/* indicate there's no data */
					*dst++ = 0;
				}
			}
			/* 4. PHY register dump from
				offset 0x00 to 0x1F. */
#define FWDMP_PHY_REG_DUMP_SIZ		0x20
			if (tg3_flag(tp, ENABLE_APE)) {
				*dst++ = FWDMP_MARKER;
				*dst++ = 0;   /* reserved */
				*dst++ = FWDMP_PHY_DUMP;
				/* proceed if we have sufficient space */
				if (CUR_DMP_SIZ(dst+1+
					FWDMP_PHY_REG_DUMP_SIZ) <
					TG3_FWDMP_SIZE){
					*dst++ = FWDMP_PHY_REG_DUMP_SIZ * 4;
					tg3_phyreg_dump(tp, 0, dst,
						FWDMP_PHY_REG_DUMP_SIZ);
					dst += FWDMP_PHY_REG_DUMP_SIZ;
				} else {
					/* indicate there's no data */
					*dst++ = 0;
				}
			}
			*dst++ = FWDMP_END_MARKER;
			/* append CRC to the end of everything */
			chksum = calc_crc((unsigned char *)tg3_fwdmp_va_ptr,
				CUR_DMP_SIZ(dst));
			*dst++ = chksum;
			total_dmp_size = CUR_DMP_SIZ(dst);
			if (total_dmp_size > TG3_FWDMP_SIZE)
				total_dmp_size = TG3_FWDMP_SIZE;
			dmp->fw_hdr.dmp_size = total_dmp_size -
				sizeof(struct fw_dmp_hdr);
			status = vmklnx_dump_range(tg3_fwdmp_dh,
					tg3_fwdmp_va_ptr, total_dmp_size);
			if (status != VMK_OK) {
				printk(KERN_ERR "####failed to dump TG3 fw/chip data !\n");
				break;
			}
		}
	}
	return status;
}
#endif /* (defined(__VMKLNX__) && VMWARE_ESX_DDK_VERSION >= 55000) */

