/*
 *  Copyright 2013 Cisco Systems, Inc.  All rights reserved.
 */ 
#include "kcompat.h"
#include "enic_clsf.h"

int enic_addfltr_mac_vlan(struct enic *enic, u8 *macaddr,
	u16 vlan_id, u16 rq_id, u16 *filter_id)
{
	int ret;
	struct filter data;
	struct filter_mac_vlan *mac_filter;
	
	memset(&data, 0, sizeof(data));
	data.type = FILTER_MAC_VLAN;
	mac_filter = &data.u.mac_vlan;
	memcpy(mac_filter->mac_addr, macaddr, ETH_ALEN);
	mac_filter->vlan = vlan_id;
	mac_filter->flags = FILTER_FIELDS_MAC_VLAN;

	spin_lock_bh(&enic->devcmd_lock);
	ret = vnic_dev_classifier(enic->vdev, CLSF_ADD, &rq_id, &data);
	*filter_id = rq_id;	
	spin_unlock_bh(&enic->devcmd_lock);
		
	return ret;
}

/*
 * enic_addfltr_5t - Add ipv4 5tuple filter
 *	@enic: enic struct of vnic
 *	@keys: flow_keys of ipv4 5tuple
 *	@rq: rq number to steer to
 *
 * This function returns filter_id(hardware_id) of the filter
 * added. In case of error it returns an negative number.
 */
int enic_addfltr_5t(struct enic *enic, struct flow_keys *keys, u16 rq)
{
	int res;
	struct filter data;

	switch (keys->ip_proto){
	case IPPROTO_TCP:
		data.u.ipv4.protocol = PROTO_TCP;
		break;
	case IPPROTO_UDP:
		data.u.ipv4.protocol = PROTO_UDP;
		break;
	default:
		return -EPROTONOSUPPORT;
	};
	data.type = FILTER_IPV4_5TUPLE;
	data.u.ipv4.src_addr = ntohl(keys->src);
	data.u.ipv4.dst_addr = ntohl(keys->dst);
	data.u.ipv4.src_port = ntohs(keys->port16[0]);
	data.u.ipv4.dst_port = ntohs(keys->port16[1]);
	data.u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;

	spin_lock_bh(&enic->devcmd_lock);
	res = vnic_dev_classifier(enic->vdev, CLSF_ADD, &rq, &data);
	spin_unlock_bh(&enic->devcmd_lock);
	res = (res == 0) ? rq : res;

	return res;
}

/*
 * enic_delfltr - Delete clsf filter
 * 	@enic: enic struct of vnic
 * 	@filter_id: filter_is(hardware_id) of filter to be deleted
 *
 * This function returns zero in case of success, negative number incase of
 * error.
 */
int enic_delfltr(struct enic *enic, u16 filter_id)
{
	int ret;

	spin_lock_bh(&enic->devcmd_lock);
	ret = vnic_dev_classifier(enic->vdev, CLSF_DEL, &filter_id, NULL); 
	spin_unlock_bh(&enic->devcmd_lock);
	
	return ret;
}

struct enic_rfs_fltr_node *htbl_fltr_search(struct enic *enic, u16 fltr_id)
{
	int i;

	for (i = 0; i < (1 << ENIC_RFS_FLW_BITSHIFT); i++) {
		struct hlist_head *hhead;
		struct hlist_node *tmp;
		struct enic_rfs_fltr_node *n;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 00))
		struct hlist_node *pos;
#endif

		hhead = &enic->rfs_h.ht_head[i];
		enic_hlist_for_each_entry_safe(n, pos, tmp, hhead, node)
			if (n->fltr_id == fltr_id)
				return n;
	}

	return NULL;
}

/*
 * enic_rfs_flw_tbl_init - initialize enic->rfs_h members
 *	@enic: enic data
 */
void enic_rfs_flw_tbl_init(struct enic *enic)
{
	int i;

	spin_lock_init(&enic->rfs_h.lock);

	for (i = 0; i <= ENIC_RFS_FLW_MASK; i++)
		INIT_HLIST_HEAD(&enic->rfs_h.ht_head[i]);

	enic->rfs_h.max = enic->config.num_arfs;
	enic->rfs_h.free = enic->rfs_h.max;
	enic->rfs_h.toclean = 0;
	enic_rfs_timer_init(enic);
}

void enic_rfs_flw_tbl_free(struct enic *enic)
{
	int i, res;

	enic_rfs_timer_free(enic);
	spin_lock_bh(&enic->rfs_h.lock);

	enic->rfs_h.free = 0;

	for (i=0; i < (1 << ENIC_RFS_FLW_BITSHIFT); i++) {
		struct hlist_head *hhead;
		struct hlist_node *tmp;
		struct enic_rfs_fltr_node *n;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 00))
		struct hlist_node *pos;
#endif

		hhead = &enic->rfs_h.ht_head[i];
		enic_hlist_for_each_entry_safe(n, pos, tmp, hhead, node) {
			res = enic_delfltr(enic, n->fltr_id);
			hlist_del(&n->node);
			kfree(n);
		}
	}

	spin_unlock_bh(&enic->rfs_h.lock);
}

