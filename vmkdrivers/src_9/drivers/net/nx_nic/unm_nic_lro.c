/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 */
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
//#include <linux/kgdb-defs.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>

#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"

#include <linux/mii.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include <linux/mm.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#include <linux/wrapper.h>
#endif
#ifndef _LINUX_MODULE_PARAMS_H
#include <linux/moduleparam.h>
#endif
#include <asm/system.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include "kernel_compatibility.h"
#include "unm_nic_hw.h"
#include "unm_nic_config.h"

#include "unm_nic.h"

#include "unm_nic_ioctl.h"
#include "nic_cmn.h"
#include "nxhal.h"
#include "unm_nic_config.h"
#include "unm_nic_lro.h"

#include "unm_nic_hw.h"
#include "unm_version.h"
#include "unm_brdcfg.h"


typedef struct {
	nx_hash_tbl_node_t	hash_node; // Let it be the first field
					   // Add new fields to the end
	nx_host_key_t		key;
	int 			ctx_id;
} lro_entry_t;

static int lro_ipv4_hash(void *key)
{
        uint32_t        hash;
	nx_host_key_t	*ipv4_key = (nx_host_key_t *)key;

        hash = ((ipv4_key->daddr.v4 & 0xF0F0F0F0) >> 4) |
                ((ipv4_key->daddr.v4 & 0x0F0F0F0F) << 4);
	hash = hash ^ ((uint32_t)ipv4_key->dport << 16);
        hash ^= ipv4_key->saddr.v4;
	hash = hash ^ (uint32_t)ipv4_key->sport;
        hash ^= (hash >> 16);
        hash = (hash ^ (hash >> 8)) & (NX_NUM_LRO_ENTRIES - 1);
        return hash;
}

static void nx_lro_destroy_node(nx_hash_tbl_node_t *node)
{
	nx_mem_pool_free(&((struct unm_adapter_s *)node->data)->lro.mem_pool,
			 (void *)node);
}


/* lro hash table functions */
nx_hash_tbl_ops_t lro_hash_ops = {
	.hash		= lro_ipv4_hash,
	.compare_keys	= nx_cmp_ip_key,
	.destroy_cb	= nx_lro_destroy_node,
};

int nx_initiate_lro(struct unm_adapter_s *adapter, nx_host_key_t *key,
		    __uint32_t rss_hash, __uint32_t ctx_id,
		    __uint32_t timestamp)
{
	int			rv = 0;
	nx_nic_lro_request_t	*lro_req;
	nic_request_t		req;
	lro_entry_t		*new;

	new = (lro_entry_t *)nx_mem_pool_alloc(&adapter->lro.mem_pool);
	if (new == NULL) {
		return (rv);
	}
	memcpy(&new->key, key, sizeof(nx_host_key_t));
	new->hash_node.key = &new->key;
	new->hash_node.data = adapter;
	new->ctx_id = ctx_id;
	rv = nx_hash_tbl_insert(&adapter->lro.hash_tbl, &new->hash_node);
	if (rv != NX_HASH_TBL_NODE_INSERTED) {
		nx_nic_print6(adapter, "LRO Entry Exists[%d]\n", rv);

		nx_mem_pool_free(&adapter->lro.mem_pool, new);
		goto done;
	}

	req.opcode = NX_NIC_HOST_REQUEST;

	lro_req = &req.body.lro_request;
	lro_req->req_hdr.word = 0;
	lro_req->req_hdr.opcode = NX_NIC_H2C_OPCODE_LRO_REQUEST;
	lro_req->req_hdr.ctxid = ctx_id;
	lro_req->req_hdr.sub_opcode = NX_NIC_LRO_REQUEST_ADD_FLOW;

	lro_req->daddr.v4 = key->daddr.v4;
	lro_req->saddr.v4 = key->saddr.v4;
	lro_req->dport = key->dport;
	lro_req->sport = key->sport;
	lro_req->family = 0;
	lro_req->rss_hash = rss_hash;
	lro_req->timestamp = timestamp;

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if (rv) {
		nx_nic_print3(adapter, "Sending LRO request to FW failed %d\n",
			      rv);
		nx_hash_tbl_delete(&adapter->lro.hash_tbl, key);
		nx_mem_pool_free(&adapter->lro.mem_pool, new);
	}

  done:
	return rv;
}

int nx_try_initiate_lro(struct unm_adapter_s *adapter, struct sk_buff *skb,
			__uint32_t rss_hash, __uint32_t ctx_id)
{
	int		rv = 1;
	struct tcphdr	*th;
	struct iphdr	*iph;
	nx_host_key_t	key;
	__uint32_t	*ts;
	__uint32_t	timestamp;
	__uint32_t	hdr_size;

	skb_pull(skb, ETH_HLEN);

	if (skb->protocol != htons(ETH_P_IP)) {
		goto esx_done;
	}

	iph = (struct iphdr *)skb->data;

	if (iph->version != NX_IP_VERSION_V4 ||
			iph->ihl != (IPV4_HDR_SIZE >> 2) || 
			iph->protocol != IPPROTO_TCP) {
		goto esx_done;
	}

	if (skb_pull(skb, iph->ihl << 2) == NULL) {
		goto esx_done;
	}

	th = (struct tcphdr *)skb->data;

	/* Check for flags, if SYN/FIN/RST/URG ... not worthy */
	if (th->syn || th->fin || th->urg || th->rst) {
		goto done;
	}

	/* Should connections with smaller packet size should be 
	 * disallowed from using lro ?  -- not for the time being */
	/* Check for pure acks */
	if (ntohs(iph->tot_len) <= ((iph->ihl + th->doff) << 2)) {
		goto done;
	}

	hdr_size = th->doff << 2;
	timestamp = 0;
	if (hdr_size != TCP_HDR_SIZE) {
		/*
		 * If it is not a simple header or a timestamp header don't
		 * do LRO.
		 */
		if (hdr_size != (TCP_HDR_SIZE + TCP_TS_OPTION_SIZE)) {
			goto done;
		}
		skb_pull(skb, TCP_HDR_SIZE);
		ts = (__uint32_t *)skb->data;
		skb_push(skb, TCP_HDR_SIZE);
		if (ntohl(ts[0]) != 0x0101080a) {
			nx_nic_print6(adapter, "LRO: Invalid timestamp "
					"option[0x%x]\n", ntohl(ts[0]));
			goto done;
		}
		timestamp = 1;
	}

	key.daddr.v4 = ntohl(iph->daddr);
	key.saddr.v4 = ntohl(iph->saddr);
	key.sport = ntohs(th->source);
	key.dport = ntohs(th->dest);
	key.ip_version = NX_IP_VERSION_V4;

	rv = nx_initiate_lro(adapter, &key, rss_hash, ctx_id, timestamp);

done:
	skb_push(skb, iph->ihl << 2);

esx_done:
	skb_push(skb, ETH_HLEN);
	return rv;
}

void nx_handle_lro_response(nx_dev_handle_t drv_handle, nic_response_t *resp)
{
	/* Right now the only lro response we get is lro tuple deletion */
	struct unm_adapter_s	*adapter = (struct unm_adapter_s *)drv_handle;
	nx_host_key_t		key;
	nx_hash_tbl_node_t	*entry;

	nx_nic_print6(((struct unm_adapter_s *)drv_handle),
		      "LRO Card Response: %d\n", resp->rsp_hdr.nic.opcode);

	key.daddr.v4 = resp->body.lro_response.daddr.v4;
	key.saddr.v4 = resp->body.lro_response.saddr.v4;
	key.sport = resp->body.lro_response.sport;
	key.dport = resp->body.lro_response.dport;
	key.ip_version = 4;

	if ((entry = nx_hash_tbl_delete(&adapter->lro.hash_tbl, &key))) {
		nx_mem_pool_free(&adapter->lro.mem_pool, (void *)entry);
	} else {
		nx_nic_print3(NULL, "LRO delete failed: source[0x%x:%u], "
			      "dest[0x%x:%u]\n", key.saddr.v4, key.sport,
			      key.daddr.v4, key.dport);
	}
}

int unm_init_lro(struct unm_adapter_s *adapter) 
{
	int		rv;
	uint32_t	max_lro;
	uint64_t cur_fn = (uint64_t) unm_init_lro;

	nx_nic_print6(NULL, "LRO Initialization\n");

	if (nx_fw_cmd_query_max_lro(adapter->nx_dev,
				adapter->ahw.pci_func,
				&max_lro) != NX_RCODE_SUCCESS) {
		nx_nic_print4(adapter, "Failed in query of FW for "
				"max LRO\n");
		return (-EIO);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, max_lro);

	nx_nic_print6(adapter, "Device supports %u LRO entries\n", max_lro);
	rv = nx_mem_pool_create(&adapter->lro.mem_pool, max_lro,
				sizeof(lro_entry_t));
	if (rv) {
		nx_nic_print3(NULL, "LRO memory pool creation failed\n");
		return (rv);
	}
	rv = nx_hash_tbl_create(&adapter->lro.hash_tbl, max_lro,
				&lro_hash_ops);
	if (rv) {
		nx_nic_print3(NULL, "LRO hash table creation failed\n");
		nx_mem_pool_destroy(&adapter->lro.mem_pool);
		return (rv);
	}
	adapter->lro.initialized = 1;
	adapter->lro.enabled = 1;

	return (0);
}

void unm_cleanup_lro(struct unm_adapter_s *adapter) 
{
	nx_nic_print6(NULL, "LRO Destruction\n");
	if (adapter->lro.initialized) {
		nx_hash_tbl_destroy(&adapter->lro.hash_tbl);
		nx_mem_pool_destroy(&adapter->lro.mem_pool);
	}
	adapter->lro.initialized = 0;
}

int nx_lro_send_cleanup(struct unm_adapter_s *adapter)
{
	nx_nic_lro_request_t	*lro_req;
	nic_request_t		req;
	int			rv;

	req.opcode = NX_NIC_HOST_REQUEST;

	lro_req = &req.body.lro_request;
	lro_req->req_hdr.word = 0;
	lro_req->req_hdr.opcode = NX_NIC_H2C_OPCODE_LRO_REQUEST;
	lro_req->req_hdr.ctxid = adapter->nx_dev->rx_ctxs[0]->context_id;
	lro_req->req_hdr.sub_opcode = NX_NIC_LRO_REQUEST_CLEANUP;

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if (rv) {
		nx_nic_print3(adapter, "Sending LRO Cleanup to FW failed %d\n",
			      rv);
	}
	return (rv);
}

void nx_lro_delete_ctx_lro (struct unm_adapter_s *adapter, int ctx_id)
{       
	int                     i;
	nx_hash_tbl_node_t      *node;
	struct list_head        *ptr;
	struct list_head        *temp;
	lro_entry_t             *entry;
	nx_hash_tbl_t           *tbl = &adapter->lro.hash_tbl;
	uint64_t cur_fn = (uint64_t) nx_lro_delete_ctx_lro;

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);


	mutex_lock(&tbl->tbl_lock);
	if(tbl->init_flag == 0 || tbl->buckets == NULL) {
		mutex_unlock(&tbl->tbl_lock);
		return;
	}

	for (i = 0; i < tbl->bucket_cnt; i++) {
		spin_lock_bh(&tbl->buckets[i].lock);

		list_for_each_safe(ptr, temp, &tbl->buckets[i].head) {
			node = list_entry(ptr, nx_hash_tbl_node_t, list);
			if((entry = container_of(node, lro_entry_t, hash_node)) 
					&& (entry->ctx_id == ctx_id)) {

				list_del_init(&node->list);
				nx_mem_pool_free(&adapter->lro.mem_pool, entry);
			}
		}

		spin_unlock_bh(&tbl->buckets[i].lock);
	}       

	mutex_unlock(&tbl->tbl_lock);
}               

static int nx_lro_set_state(struct unm_adapter_s *adapter, int val)
{
	if (adapter->lro.enabled == val) {
		return 0;
	}
	if (!adapter->lro.initialized && val) {
		nx_nic_print3(adapter, "LRO Cannot Set Enable, "
			      "Not Initialized\n");
		return (-EIO);
	}
	if (val == 0 && nx_lro_send_cleanup(adapter) != 0) {
		return (1);
	}
	adapter->lro.enabled = val;

	return 0;
}

int nx_write_lro_state(struct file *file, const char *buffer,
		       unsigned long count, void *data)
{
	struct net_device	*netdev = (struct net_device *)data;
	struct unm_adapter_s	*adapter;
	int			testval = 0;

	adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	if (!capable(CAP_NET_ADMIN)) {
		return -EACCES;
	}

	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}

	memcpy((void *)&testval, (const void *)buffer, 1);

	testval = testval - '0';

	nx_lro_set_state(adapter, testval);
	return count;
}

int nx_read_lro_state(char *buf, char **start, off_t offset, int count,
				int *eof, void *data) 
{
	
	int len = 0;
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	len = sprintf(buf,"%d\n", adapter->lro.enabled);
	*eof = 1;
	return len ;
}

int nx_write_lro_stats(struct file *file, const char *buffer,
		       unsigned long count, void *data)
{
	struct net_device	*netdev = (struct net_device *)data;
	struct unm_adapter_s	*adapter;
	int			testval = 0;
	int			ii;

	adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	if (!capable(CAP_NET_ADMIN)) {
		return -EACCES;
	}

	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}

	memcpy((void *)&testval, (const void *)buffer, 1);

	testval = testval - '0';

	adapter->lro.stats.chained_pkts = 0;
	adapter->lro.stats.contiguous_pkts = 0;
	for (ii = 0; ii < NX_MAX_PKTS_PER_LRO; ii++) {
		adapter->lro.stats.accumulation[ii] = 0;
	}

	for (ii = 0; ii < NX_1K_PER_LRO; ii++) {
		adapter->lro.stats.bufsize[ii] = 0;
	}

	return count;
}

int nx_read_lro_stats(char *buf, char **start, off_t offset, int count,
		      int *eof, void *data) 
{
	
	int len = 0;
	int ii = 0;
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	if (adapter->lro.stats.chained_pkts) {
		len += sprintf(buf + len,
			       "Chained LRO packets:\t%llu\n\n",
			       adapter->lro.stats.chained_pkts);

		len += sprintf(buf + len, "Segments per LRO segment\n");
		len += sprintf(buf + len, "========================\n");

		for (ii = 0; ii < NX_MAX_PKTS_PER_LRO; ii++) {
			len += sprintf(buf + len, "%2d Packets:\t\t%llu\n",
				       (ii + 1),
				       adapter->lro.stats.accumulation[ii]);
		}
	} else {
		len += sprintf(buf + len,
			       "No Chained LRO packets received.\n\n");
	}

	if (adapter->lro.stats.contiguous_pkts) {
		len += sprintf(buf + len,
			       "Contiguous LRO packets:\t%llu\n\n",
			       adapter->lro.stats.contiguous_pkts);

		len += sprintf(buf + len, "Size as 1KB per LRO segment\n");
		len += sprintf(buf + len, "===========================\n");

		for (ii = 0; ii < NX_1K_PER_LRO; ii++) {
			len += sprintf(buf + len, "%2d KB - %2d KB:\t\t%llu\n",
				       ii, (ii + 1),
				       adapter->lro.stats.bufsize[ii]);
		}
	} else {
		len += sprintf(buf + len,
			       "No Contiguous LRO packets received.\n\n");
	}
	*eof = 1;
	return len ;
}
