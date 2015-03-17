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
#include <linux/bitops.h>
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

extern u8 fcoe_all_vn2vn[ETH_ALEN];
extern u8 fcoe_all_p2p[ETH_ALEN];

 /**
 * fcoe_ctlr_vn_send() - Send a FIP VN2VN Probe Request or Reply.
 * @fip: The FCoE controller
 * @sub: sub-opcode for probe request, reply, or advertisement.
 * @dest: The destination Ethernet MAC address
 * @min_len: minimum size of the Ethernet payload to be sent
 */
static void fcoe_ctlr_vn_send(struct fcoe_ctlr *fip,
			      enum fip_vn2vn_subcode sub,
			      const u8 *dest, size_t min_len)
{
	struct sk_buff *skb;
	struct fip_frame {
		struct ethhdr eth;
		struct fip_header fip;
		struct fip_mac_desc mac;
		struct fip_wwn_desc wwnn;
		struct fip_vn_desc vn;
	} __attribute__((packed)) *frame;
	struct fip_fc4_feat *ff;
	struct fip_size_desc *size;
	u32 fcp_feat;
	u8 *mac_ptr;
	size_t len;
	size_t dlen;

	len = sizeof(*frame);
	dlen = 0;
	if (sub == FIP_SC_VN_CLAIM_NOTIFY || sub == FIP_SC_VN_CLAIM_REP) {
		dlen = sizeof(struct fip_fc4_feat) +
		       sizeof(struct fip_size_desc);
		len += dlen;
	}
	dlen += sizeof(frame->mac) + sizeof(frame->wwnn) + sizeof(frame->vn);
	len = max(len, min_len + sizeof(struct ethhdr));

	skb = dev_alloc_skb(len);
	if (!skb)
		return;

	frame = (struct fip_frame *)skb->data;
	memset(frame, 0, len);
	memcpy(frame->eth.h_dest, dest, ETH_ALEN);
	if (sub == FIP_SC_VN_BEACON) {
		hton24(frame->eth.h_source, FIP_VN_FC_MAP);
		mac_ptr = &frame->eth.h_source[3];
		hton24(mac_ptr, fip->port_id);
	} else {
		memcpy(frame->eth.h_source, fip->ctl_src_addr, ETH_ALEN);
	}
	frame->eth.h_proto = htons(ETH_P_FIP);

	frame->fip.fip_ver = FIP_VER_ENCAPS(FIP_VER);
	frame->fip.fip_op = htons(FIP_OP_VN2VN);
	frame->fip.fip_subcode = sub;
	frame->fip.fip_dl_len = htons(dlen / FIP_BPW);

	frame->mac.fd_desc.fip_dtype = FIP_DT_MAC;
	frame->mac.fd_desc.fip_dlen = sizeof(frame->mac) / FIP_BPW;
	memcpy(frame->mac.fd_mac, fip->ctl_src_addr, ETH_ALEN);

	frame->wwnn.fd_desc.fip_dtype = FIP_DT_NAME;
	frame->wwnn.fd_desc.fip_dlen = sizeof(frame->wwnn) / FIP_BPW;
	put_unaligned_be64(fip->lp->wwnn, &frame->wwnn.fd_wwn);

	frame->vn.fd_desc.fip_dtype = FIP_DT_VN_ID;
	frame->vn.fd_desc.fip_dlen = sizeof(frame->vn) / FIP_BPW;
	hton24(frame->vn.fd_mac, FIP_VN_FC_MAP);
	mac_ptr = &frame->vn.fd_mac[3];
	hton24(mac_ptr, fip->port_id);
	hton24(frame->vn.fd_fc_id, fip->port_id);
	put_unaligned_be64(fip->lp->wwpn, &frame->vn.fd_wwpn);

	/*
	 * For claims, add FC-4 features.
	 * TBD: Add interface to get fc-4 types and features from libfc.
	 */
	if (sub == FIP_SC_VN_CLAIM_NOTIFY || sub == FIP_SC_VN_CLAIM_REP) {
		ff = (struct fip_fc4_feat *)(frame + 1);
		ff->fd_desc.fip_dtype = FIP_DT_FC4F;
		ff->fd_desc.fip_dlen = sizeof(*ff) / FIP_BPW;
		ff->fd_fts = fip->lp->fcts;

		fcp_feat = 0;
		if (fip->lp->service_params & FCP_SPPF_INIT_FCN)
			fcp_feat |= FCP_FEAT_INIT;
		if (fip->lp->service_params & FCP_SPPF_TARG_FCN)
			fcp_feat |= FCP_FEAT_TARG;
		fcp_feat <<= (FC_TYPE_FCP * 4) % 32;
		ff->fd_ff.fd_feat[FC_TYPE_FCP * 4 / 32] = htonl(fcp_feat);

		size = (struct fip_size_desc *)(ff + 1);
		size->fd_desc.fip_dtype = FIP_DT_FCOE_SIZE;
		size->fd_desc.fip_dlen = sizeof(*size) / FIP_BPW;
		size->fd_size = htons(fcoe_ctlr_fcoe_size(fip));
	}

	skb_put(skb, len);
	skb->protocol = htons(ETH_P_FIP);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);

	fip->send(fip, skb);
}

/**
 * fcoe_ctlr_vnport_rcu_free - RCU free handler for fcoe_vn_port.
 * @rcu: the RCU structure inside the fcoe_vn_port to be freed.
 */
static void fcoe_ctlr_vnport_rcu_free(struct rcu_head *rcu)
{
	struct fcoe_vn_port *vnport;

	vnport = container_of(rcu, struct fcoe_vn_port, rcu);
	kfree(vnport);
}

/**
 * fcoe_ctlr_vnport_delete - delete a fcoe_vn_port structure and free it
 * @vnport: the fcoe_vn_port to be removed and freed via RCU
 */
static void fcoe_ctlr_vnport_delete(struct fcoe_vn_port *vnport)
{
	list_del_rcu(&vnport->list);
	call_rcu(&vnport->rcu, fcoe_ctlr_vnport_rcu_free);
}

/**
 * fcoe_ctlr_vn_rport_callback - Event handler for rport events.
 * @lport: The lport which is receiving the event
 * @rdata: remote port private data
 * @event: The event that occured
 *
 * Locking Note:  The rport lock must not be held when calling this function.
 */
static void fcoe_ctlr_vn_rport_callback(struct fc_lport *lport,
					struct fc_rport_priv *rdata,
					enum fc_rport_event event)
{
	struct fcoe_ctlr *fip = lport->disc.priv;
	struct fcoe_vn_port *vnport;

	LIBFCOE_FIP_DBG(fip, "vn_rport_callback %x event %d\n",
			rdata->ids.port_id, event);
#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	list_for_each_entry(vnport, &fip->remote_vn_ports, list) {
		if (vnport->rdata != rdata)
			continue;
		switch (event) {
		case RPORT_EV_READY:
			vnport->login_count = 0;
			break;
		case RPORT_EV_LOGO:
		case RPORT_EV_FAILED:
		case RPORT_EV_STOP:
			vnport->login_count++;
			vnport->time = 0;
			LIBFCOE_FIP_DBG(fip, "detach rdata %x count %u\n",
					vnport->port_id, vnport->login_count);
			vnport->rdata = NULL;
			kref_put(&rdata->kref, lport->tt.rport_destroy);
			fcoe_ctlr_vnport_delete(vnport);
			break;
		default:
			break;
		}
		break;
	}
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
}

/**
 * fcoe_ctlr_vn_login() - Make rport for VN2VN entry and start it logging in.
 * @fip: The FCoE controller
 * @vnport: VN2VN entry
 *
 * Called with ctlr_mutex and disc_mutex held.
 */
static void fcoe_ctlr_vn_login(struct fcoe_ctlr *fip,
			       struct fcoe_vn_port *vnport)
{
	struct fc_lport *lport = fip->lp;
	struct fc_rport_priv *rdata;
	struct fc_rport_identifiers *ids;

	LIBFCOE_FIP_DBG(fip, "vn_login %x%s%s\n",
		vnport->port_id, lport->disc.disc_callback ? "" : " - no disc",
		vnport->rdata ? " - rdata already present" : "");
	if (!lport->disc.disc_callback)
		return;
	if (vnport->login_count >= FCOE_CTLR_VN2VN_LOGIN_LIMIT) {
		LIBFCOE_FIP_DBG(fip, "vn_login %x login limit %u reached\n",
				vnport->port_id, vnport->login_count);
		return;
	}

	rdata = vnport->rdata;
	if (rdata) {
		if (!lport->tt.rport_login(rdata))
			return;
		kref_put(&rdata->kref, lport->tt.rport_destroy);
		vnport->rdata = NULL;
		vnport->login_count++;
	}

	rdata = lport->tt.rport_create(lport, vnport->port_id);
	if (!rdata)
		return;

	kref_get(&rdata->kref);
	vnport->rdata = rdata;
	rdata->event_callback = fcoe_ctlr_vn_rport_callback;
	rdata->disc_id = lport->disc.disc_id;

	ids = &rdata->ids;
	if ((ids->port_name != -1 && ids->port_name != vnport->port_name) ||
	    (ids->node_name != -1 && ids->node_name != vnport->node_name))
		lport->tt.rport_logoff(rdata);
	ids->port_name = vnport->port_name;
	ids->node_name = vnport->node_name;

	lport->tt.rport_login(rdata);
}

/**
 * fcoe_ctlr_vn_logout() - Disassociated rport from vnport
 * @fip: The FCoE controller
 * @vnport: VN2VN entry
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_logout(struct fcoe_ctlr *fip,
				struct fcoe_vn_port *vnport)
{
	struct fc_lport *lport = fip->lp;
	struct fc_rport_priv *rdata;

	LIBFCOE_FIP_DBG(fip, "vn_logout %x\n", vnport->port_id);
	rdata = vnport->rdata;
	if (rdata) {
		mutex_lock(&lport->disc.disc_mutex);
		vnport->rdata = NULL;
		rdata->disc_id = 0;
		lport->tt.rport_logoff(rdata);
		mutex_unlock(&lport->disc.disc_mutex);

		kref_put(&rdata->kref, lport->tt.rport_destroy);
	}
	fcoe_ctlr_vnport_delete(vnport);
}

/**
 * fcoe_ctlr_vnport_free_all() - free all fcoe_vn_ports for a fcoe_ctlr.
 * @fip: The FCoE controller
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vnport_free_all(struct fcoe_ctlr *fip)
{
	struct fcoe_vn_port *vnport;
	struct fcoe_vn_port *next;

	list_for_each_entry_safe(vnport, next, &fip->remote_vn_ports, list)
		fcoe_ctlr_vn_logout(fip, vnport);
}

/**
 * fcoe_ctlr_disc_stop_locked() - stop discovery in VN2VN mode
 * @fip: The FCoE controller
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_disc_stop_locked(struct fc_lport *lport)
{
	struct fcoe_ctlr *fip = lport->disc.priv;

	mutex_lock(&lport->disc.disc_mutex);
	lport->disc.disc_callback = NULL;
	mutex_unlock(&lport->disc.disc_mutex);
	fcoe_ctlr_vnport_free_all(fip);
}

/**
 * fcoe_ctlr_disc_stop() - stop discovery in VN2VN mode
 * @fip: The FCoE controller
 *
 * Called through the local port template for discovery.
 * Called without the ctlr_mutex held.
 */
void fcoe_ctlr_disc_stop(struct fc_lport *lport)
{
	struct fcoe_ctlr *fip = lport->disc.priv;

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
	fcoe_ctlr_disc_stop_locked(lport);
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
	fcoe_ctlr_disc_stop_locked(lport);
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
}

/**
 * fcoe_ctlr_disc_stop_final() - stop discovery for shutdown in VN2VN mode
 * @fip: The FCoE controller
 *
 * Called through the local port template for discovery.
 * Called without the ctlr_mutex held.
 */
void fcoe_ctlr_disc_stop_final(struct fc_lport *lport)
{
	fcoe_ctlr_disc_stop(lport);
	lport->tt.rport_flush_queue();
	synchronize_rcu();
}

/**
 * fcoe_ctlr_vn_restart() - VN2VN probe restart with new port_id
 * @fip: The FCoE controller
 *
 * Called with fcoe_ctlr lock held.
 */
static void fcoe_ctlr_vn_restart(struct fcoe_ctlr *fip)
{
	unsigned long wait;
	u32 port_id;

	printk("In fcoe_ctlr_vn_restart\n");
	fcoe_ctlr_disc_stop_locked(fip->lp);

	/*
	 * Get proposed port ID.
	 * If this is the first try after link up, use any previous port_id.
	 * If there was none, use the low bits of the port_name.
	 * On subsequent tries, get the next random one.
	 * Don't use reserved IDs, use another non-zero value, just as random.
	 */
	port_id = fip->port_id;
	if (fip->probe_tries) {
		get_random_bytes(&port_id, 4);
		port_id &= 0xffff;
	}
	else if (!port_id)
		port_id = fip->lp->wwpn & 0xffff;

	if (!port_id || port_id == 0xffff)
		port_id = 1;
	fip->port_id = port_id;

	if (fip->probe_tries < FIP_VN_RLIM_COUNT) {
		fip->probe_tries++;
		get_random_bytes(&wait, 4);
		wait %= FIP_VN_PROBE_WAIT;
	} else
		wait = FIP_VN_RLIM_INT;
	mod_timer(&fip->timer, jiffies + msecs_to_jiffies(wait));
	fip->state = FIP_ST_VNMP_START;
}

/**
 * fcoe_ctlr_vn_start() - Start in VN2VN mode
 * @fip: The FCoE controller
 *
 * Called with fcoe_ctlr lock held.
 */
void fcoe_ctlr_vn_start(struct fcoe_ctlr *fip)
{
	fip->probe_tries = 0;
	fcoe_ctlr_vn_restart(fip);
}

/**
 * fcoe_ctlr_vn_parse - parse probe request or response
 * @fip: The FCoE controller
 * @skb: incoming packet
 * @vnport: resulting parsed VN entry, if valid
 *
 * Returns non-zero error number on error.
 * Does not consume the packet.
 */
static int fcoe_ctlr_vn_parse(struct fcoe_ctlr *fip,
			      struct sk_buff *skb,
			      struct fcoe_vn_port *vnport)
{
	struct fip_header *fiph;
	struct fip_desc *desc = NULL;
	struct fip_mac_desc *macd = NULL;
	struct fip_wwn_desc *wwn = NULL;
	struct fip_vn_desc *vn = NULL;
	struct fip_size_desc *size = NULL;
	size_t rlen;
	size_t dlen;

	memset(vnport, 0, sizeof(*vnport));

	fiph = (struct fip_header *)skb->data;
	vnport->flags = ntohs(fiph->fip_flags);

	rlen = ntohs(fiph->fip_dl_len) * 4;
	if (rlen + sizeof(*fiph) > skb->len)
		return -EINVAL;

	desc = (struct fip_desc *)(fiph + 1);
	while (rlen > 0) {
		dlen = desc->fip_dlen * FIP_BPW;
		if (dlen < sizeof(*desc) || dlen > rlen)
			return -EINVAL;
		switch (desc->fip_dtype) {
		case FIP_DT_MAC:
			if (dlen != sizeof(struct fip_mac_desc))
				goto len_err;
			macd = (struct fip_mac_desc *)desc;
			if (!is_valid_ether_addr(macd->fd_mac)) {
				LIBFCOE_FIP_DBG(fip,
					"Invalid MAC addr %pM in FIP probe\n",
					 macd->fd_mac);
				return -EINVAL;
			}
			memcpy(vnport->enode_mac, macd->fd_mac, ETH_ALEN);
			break;
		case FIP_DT_NAME:
			if (dlen != sizeof(struct fip_wwn_desc))
				goto len_err;
			wwn = (struct fip_wwn_desc *)desc;
			vnport->node_name = get_unaligned_be64(&wwn->fd_wwn);
			break;
		case FIP_DT_VN_ID:
			if (dlen != sizeof(struct fip_vn_desc))
				goto len_err;
			vn = (struct fip_vn_desc *)desc;
			memcpy(vnport->vn_mac, vn->fd_mac, ETH_ALEN);
			vnport->port_id = ntoh24(vn->fd_fc_id);
			vnport->port_name = get_unaligned_be64(&vn->fd_wwpn);
			break;
		case FIP_DT_FC4F:
			if (dlen != sizeof(struct fip_fc4_feat))
				goto len_err;
			break;
		case FIP_DT_FCOE_SIZE:
			if (dlen != sizeof(struct fip_size_desc))
				goto len_err;
			size = (struct fip_size_desc *)desc;
			vnport->fcoe_len = ntohs(size->fd_size);
			break;
		default:
			LIBFCOE_FIP_DBG(fip, "unexpected descriptor type %x "
					"in FIP probe\n", desc->fip_dtype);
			/* standard says ignore unknown descriptors >= 128 */
			if (desc->fip_dtype < FIP_DT_VENDOR_BASE)
				return -EINVAL;
			break;
		}
		desc = (struct fip_desc *)((char *)desc + dlen);
		rlen -= dlen;
	}
	if (!macd || !wwn || !vn)
		return -EINVAL;
	if ((fiph->fip_subcode == FIP_SC_VN_CLAIM_NOTIFY ||
	    fiph->fip_subcode == FIP_SC_VN_CLAIM_REP) && !size)
		return -EINVAL;
	return 0;

len_err:
	LIBFCOE_FIP_DBG(fip, "FIP length error in descriptor type %x len %zu\n",
			desc->fip_dtype, dlen);
	return -EINVAL;
}

/**
 * fcoe_ctlr_vn_send_claim() - send multicast FIP VN2VN Claim Notification.
 * @fip: The FCoE controller
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_send_claim(struct fcoe_ctlr *fip)
{
	fcoe_ctlr_vn_send(fip, FIP_SC_VN_CLAIM_NOTIFY, fcoe_all_vn2vn, 0);
	fip->sol_time = jiffies;
}

/**
 * fcoe_ctlr_vn_probe_req() - handle incoming VN2VN probe request.
 * @fip: The FCoE controller
 * @vnport: parsed VN entry from the probe request
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_probe_req(struct fcoe_ctlr *fip,
				   struct fcoe_vn_port *vnport)
{
	if (vnport->port_id != fip->port_id)
		return;

	switch (fip->state) {
	case FIP_ST_VNMP_CLAIM:
	case FIP_ST_VNMP_UP:
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REP,
				  vnport->enode_mac, 0);
		break;
	case FIP_ST_VNMP_PROBE1:
	case FIP_ST_VNMP_PROBE2:
		if (fip->lp->wwpn > vnport->port_name) {
			fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REP,
					  vnport->enode_mac, 0);
			break;
		}
		/* fall through */
	case FIP_ST_VNMP_START:
		fcoe_ctlr_vn_restart(fip);
		break;
	default:
		break;
	}
}

/**
 * fcoe_ctlr_vn_probe_reply() - handle incoming VN2VN probe reply.
 * @fip: The FCoE controller
 * @vn: parsed VN entry from the probe reply
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_probe_reply(struct fcoe_ctlr *fip,
				     struct fcoe_vn_port *vnport)
{
	if (vnport->port_id != fip->port_id)
		return;
	switch (fip->state) {
	case FIP_ST_VNMP_START:
	case FIP_ST_VNMP_PROBE1:
	case FIP_ST_VNMP_PROBE2:
	case FIP_ST_VNMP_CLAIM:
		fcoe_ctlr_vn_restart(fip);
		break;
	case FIP_ST_VNMP_UP:
		fcoe_ctlr_vn_send_claim(fip);
		break;
	default:
		break;
	}
}

/**
 * fcoe_ctlr_vn_add() - Add a VN2VN entry to the list, based on a claim reply.
 * @fip: The FCoE controller
 * @new: newly parsed VN2VN entry
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_add(struct fcoe_ctlr *fip,
			     const struct fcoe_vn_port *new)
{
	struct fcoe_vn_port *vnport;

	if (new->port_id == fip->port_id)
		return;

	/*
	 * Look for matching existing entry.  Match on port_id only.
	 * If there are duplicate port names, one should age out.
	 */
	list_for_each_entry(vnport, &fip->remote_vn_ports, list)
		if (vnport->port_id == new->port_id)
			goto update;

	vnport = kzalloc(sizeof(*new), GFP_ATOMIC);
	if (!vnport)
		return;
	list_add_tail_rcu(&vnport->list, &fip->remote_vn_ports);
	LIBFCOE_FIP_DBG(fip, "vn_add rport %x\n", new->port_id);
update:
	LIBFCOE_FIP_DBG(fip, "vn_update rport %x\n", new->port_id);
	vnport->time = 0;
	vnport->port_id = new->port_id;
	vnport->port_name = new->port_name;
	vnport->node_name = new->node_name;
	vnport->fcoe_len = new->fcoe_len;
	memcpy(vnport->enode_mac, new->enode_mac, sizeof(vnport->enode_mac));
	memcpy(vnport->vn_mac, new->vn_mac, sizeof(vnport->vn_mac));
}

/**
 * fcoe_ctlr_vn_lookup() - Find VN remote port's MAC address
 * @fip: The FCoE controller
 * @port_id:  The port_id of the remote VN_node
 * @mac: buffer which will hold the VN_NODE destination MAC address, if found.
 *
 * Returns non-zero error if no remote port found.
 */
int fcoe_ctlr_vn_lookup(struct fcoe_ctlr *fip, u32 port_id, u8 *mac)
{
	struct fcoe_vn_port *vnport;
	int ret = -1;

	rcu_read_lock();
	list_for_each_entry_rcu(vnport, &fip->remote_vn_ports, list) {
		if (vnport->port_id == port_id) {
			memcpy(mac, vnport->enode_mac, ETH_ALEN);
			ret = 0;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}

/**
 * fcoe_ctlr_vn_claim_notify() - handle received FIP VN2VN Claim Notification
 * @fip: The FCoE controller
 * @vnport: newly parsed VN2VN entry
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_claim_notify(struct fcoe_ctlr *fip,
				      struct fcoe_vn_port *vnport)
{
	if (vnport->flags & FIP_FL_P2P) {
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REQ, fcoe_all_vn2vn, 0);
		return;
	}
	switch (fip->state) {
	case FIP_ST_VNMP_START:
	case FIP_ST_VNMP_PROBE1:
	case FIP_ST_VNMP_PROBE2:
		if (vnport->port_id == fip->port_id)
			fcoe_ctlr_vn_restart(fip);
		break;
	case FIP_ST_VNMP_CLAIM:
	case FIP_ST_VNMP_UP:
		if (vnport->port_id == fip->port_id) {
			if (vnport->port_name > fip->lp->wwpn) {
				fcoe_ctlr_vn_restart(fip);
				break;
			}
			fcoe_ctlr_vn_send_claim(fip);
			break;
		}
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_CLAIM_REP, vnport->enode_mac,
				  min((u32)vnport->fcoe_len,
				      fcoe_ctlr_fcoe_size(fip)));
		fcoe_ctlr_vn_add(fip, vnport);
		break;
	default:
		break;
	}
}

/**
 * fcoe_ctlr_vn_claim_resp() - handle received Claim Response
 * @fip: The FCoE controller that received the frame
 * @vnport: The parsed vn_port from the Claim Response
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_claim_resp(struct fcoe_ctlr *fip,
				   struct fcoe_vn_port *vnport)
{
	LIBFCOE_FIP_DBG(fip, "claim resp from from rport %x - state %x\n",
			vnport->port_id, fip->state);
	if (fip->state == FIP_ST_VNMP_UP || fip->state == FIP_ST_VNMP_CLAIM)
		fcoe_ctlr_vn_add(fip, vnport);
}

/**
 * fcoe_ctlr_vn_beacon() - handle received beacon.
 * @fip: The FCoE controller that received the frame
 * @bp: The parsed vnport from the beacon
 *
 * Called with ctlr_mutex held.
 */
static void fcoe_ctlr_vn_beacon(struct fcoe_ctlr *fip, struct fcoe_vn_port *bp)
{
	struct fcoe_vn_port *vnport;
	struct fcoe_vn_port *next;

	if (bp->flags & FIP_FL_P2P) {
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REQ, fcoe_all_vn2vn, 0);
		return;
	}

	if (fip->state != FIP_ST_VNMP_UP)
		return;
	list_for_each_entry_safe(vnport, next, &fip->remote_vn_ports, list) {
		if (vnport->port_id == bp->port_id &&
		    vnport->node_name == bp->node_name &&
		    vnport->port_name == bp->port_name) {
			if (!vnport->time)
				fcoe_ctlr_vn_login(fip, vnport);
			vnport->time = jiffies;
			return;
		}
	}

	/*
	 * Beacon from a new neighbor.
	 * Send a claim notify if one hasn't been sent recently.
	 * Don't add the neighbor yet.
	 */
	LIBFCOE_FIP_DBG(fip, "beacon from new rport %x. sending claim notify\n",
			bp->port_id);
	if (time_after(jiffies,
		       fip->sol_time + msecs_to_jiffies(FIP_VN_ANN_WAIT)))
		fcoe_ctlr_vn_send_claim(fip);
}

/**
 * fcoe_ctlr_vn_age() - Check for VN_ports without recent beacons
 * @fip: The FCoE controller
 *
 * Called with ctlr_mutex held.
 * Called only in state FIP_ST_VNMP_UP.
 */
static void fcoe_ctlr_vn_age(struct fcoe_ctlr *fip)
{
	struct fcoe_vn_port *vnport;
	struct fcoe_vn_port *next;
	unsigned long epoch;

	epoch = jiffies - msecs_to_jiffies(FIP_VN_BEACON_INT * 25 / 10);

	list_for_each_entry_safe(vnport, next, &fip->remote_vn_ports, list)
		if (vnport->time && time_before(vnport->time, epoch))
			fcoe_ctlr_vn_logout(fip, vnport);
}

/**
 * fcoe_ctlr_vn_recv() - Receive a FIP frame
 * @fip: The FCoE controller that received the frame
 * @skb: The received FIP frame
 *
 * Returns non-zero if the frame is dropped.
 * Always consumes the frame.
 */
int fcoe_ctlr_vn_recv(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	struct fip_header *fiph;
	enum fip_vn2vn_subcode sub;
	struct fcoe_vn_port vnport;
	int rc;

	fiph = (struct fip_header *)skb->data;
	sub = fiph->fip_subcode;

	rc = fcoe_ctlr_vn_parse(fip, skb, &vnport);
	if (rc) {
		LIBFCOE_FIP_DBG(fip, "vn_recv vn_parse error %d\n", rc);
		goto drop;
	}

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	switch (sub) {
	case FIP_SC_VN_PROBE_REQ:
		fcoe_ctlr_vn_probe_req(fip, &vnport);
		break;
	case FIP_SC_VN_PROBE_REP:
		fcoe_ctlr_vn_probe_reply(fip, &vnport);
		break;
	case FIP_SC_VN_CLAIM_NOTIFY:
		fcoe_ctlr_vn_claim_notify(fip, &vnport);
		break;
	case FIP_SC_VN_CLAIM_REP:
		fcoe_ctlr_vn_claim_resp(fip, &vnport);
		break;
	case FIP_SC_VN_BEACON:
		fcoe_ctlr_vn_beacon(fip, &vnport);
		break;
	default:
		LIBFCOE_FIP_DBG(fip, "vn_recv unknown subcode %d\n", sub);
		rc = -1;
		break;
	}
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
drop:
	kfree_skb(skb);
	return rc;
}

/**
 * fcoe_ctlr_disc_recv - discovery receive handler for VN2VN mode.
 * @fip: The FCoE controller
 *
 * This should never be called since we don't see RSCNs or other
 * fabric-generated ELSes.
 */
void fcoe_ctlr_disc_recv(struct fc_seq *seq, struct fc_frame *fp,
				struct fc_lport *lport)
{
	struct fc_seq_els_data rjt_data;

	rjt_data.fp = NULL;
	rjt_data.reason = ELS_RJT_UNSUP;
	rjt_data.explan = ELS_EXPL_NONE;
	lport->tt.seq_els_rsp_send(seq, ELS_LS_RJT, &rjt_data);
	fc_frame_free(fp);
}

/**
 * fcoe_ctlr_disc_recv - start discovery for VN2VN mode.
 * @fip: The FCoE controller
 *
 * This sets a flag indicating that remote ports should be created
 * and started for the peers we discover.  We use the disc_callback
 * pointer as that flag.  Peers already discovered are created here.
 *
 * The lport lock is held during this call. The callback must be done
 * later, without holding either the lport or discovery locks.
 * The fcoe_ctlr lock may also be held during this call.
 */
void fcoe_ctlr_disc_start(void (*callback)(struct fc_lport *,
						  enum fc_disc_event),
				 struct fc_lport *lport)
{
	printk("In fcoe_ctlr_disc_start\n");
	struct fc_disc *disc = &lport->disc;
	struct fcoe_ctlr *fip = disc->priv;

	mutex_lock(&disc->disc_mutex);
	disc->disc_callback = callback;
	disc->disc_id = (disc->disc_id + 2) | 1;
	disc->pending = 1;
	schedule_work(&fip->timer_work);
	mutex_unlock(&disc->disc_mutex);
}

/**
 * fcoe_ctlr_vn_disc() - report FIP VN_port discovery results after claim state.
 * @fip: The FCoE controller
 *
 * Starts the FLOGI and PLOGI login process to each discovered rport for which
 * we've received at least one beacon.
 * Performs the discovery complete callback.
 */
static void fcoe_ctlr_vn_disc(struct fcoe_ctlr *fip)
{
	struct fc_lport *lport = fip->lp;
	struct fc_disc *disc = &lport->disc;
	struct fcoe_vn_port *vnport;
	struct fcoe_vn_port *next;
	void (*callback)(struct fc_lport *, enum fc_disc_event);

	mutex_lock(&disc->disc_mutex);
	callback = disc->pending ? disc->disc_callback : NULL;
	disc->pending = 0;
	list_for_each_entry_safe(vnport, next, &fip->remote_vn_ports, list)
		if (vnport->time)
			fcoe_ctlr_vn_login(fip, vnport);
	mutex_unlock(&disc->disc_mutex);
	if (callback)
		callback(lport, DISC_EV_SUCCESS);
}

/**
 * fcoe_ctlr_vn_timeout - timer work function for VN2VN mode.
 * @fip: The FCoE controller
 */
void fcoe_ctlr_vn_timeout(struct fcoe_ctlr *fip)
{
	unsigned long next_wait;
	u8 mac[ETH_ALEN];
	u32 new_port_id = 0, wait_period = 0;
	u8 *mac_ptr;

#if defined(__VMKLNX__)
	spin_lock_bh(fip->ctlr_lock);
#else
	mutex_lock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)
	switch (fip->state) {
	case FIP_ST_VNMP_START:
		fip->state = FIP_ST_VNMP_PROBE1;
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REQ, fcoe_all_vn2vn, 0);
		next_wait = msecs_to_jiffies(FIP_VN_PROBE_WAIT);
		break;
	case FIP_ST_VNMP_PROBE1:
               fip->state = FIP_ST_VNMP_PROBE2;
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_PROBE_REQ, fcoe_all_vn2vn, 0);
		next_wait = msecs_to_jiffies(FIP_VN_ANN_WAIT);
		break;
	case FIP_ST_VNMP_PROBE2:
               fip->state = FIP_ST_VNMP_CLAIM;
		new_port_id = fip->port_id;
		hton24(mac, FIP_VN_FC_MAP);
		mac_ptr = &mac[3];
		hton24(mac_ptr, new_port_id);
		fcoe_ctlr_map_dest(fip);
		fip->update_mac(fip->lp, mac);
		fcoe_ctlr_vn_send_claim(fip);
		next_wait = msecs_to_jiffies(FIP_VN_ANN_WAIT);
		break;
	case FIP_ST_VNMP_CLAIM:
		/*
		 * This may be invoked either by starting discovery so don't
		 * go to the next state unless it's been long enough.
		 */
		next_wait = msecs_to_jiffies(FIP_VN_ANN_WAIT);
		if (time_after_eq(jiffies, fip->sol_time + next_wait)) {
                        fip->state = FIP_ST_VNMP_UP;
			fcoe_ctlr_vn_send(fip, FIP_SC_VN_BEACON,
					  fcoe_all_vn2vn, 0);
		} else {
			next_wait -= jiffies - fip->sol_time;
		}
		fcoe_ctlr_vn_disc(fip);
		break;
	case FIP_ST_VNMP_UP:
		fcoe_ctlr_vn_send(fip, FIP_SC_VN_BEACON, fcoe_all_vn2vn, 0);
		fcoe_ctlr_vn_age(fip);
		/* fall through */
	default:
		get_random_bytes(&wait_period, 4);
		next_wait = msecs_to_jiffies(FIP_VN_BEACON_INT +
					     (wait_period % FIP_VN_BEACON_FUZZ));
		break;
	}
	mod_timer(&fip->timer, jiffies + next_wait);
#if defined(__VMKLNX__)
	spin_unlock_bh(fip->ctlr_lock);
#else
	mutex_unlock(&fip->ctlr_mutex);
#endif // defined(__VMKLNX__)

	/* If port ID is new, notify local port after dropping ctlr_mutex */
	if (new_port_id)
		fc_lport_set_local_id(fip->lp, new_port_id);
}
