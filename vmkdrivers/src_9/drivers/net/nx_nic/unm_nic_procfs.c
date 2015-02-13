/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
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
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>

#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"

#include "unm_inc.h"
#include "unm_brdcfg.h"
#include "unm_nic.h"

#include "nx_nic_linux_tnic_api.h"
#include "nxhal.h"
#include "nxhal_v34.h"
#include <unm_pstats.h>

#define SNMP_ETH_FILENAME       "snmp_eth"
static int unm_nic_port_read_proc(char *buf, char **start, off_t offset,
					int count, int *eof, void *data);
int unm_init_proc_drv_dir(void);
void unm_cleanup_proc_drv_entries(void);
void unm_init_proc_entries(struct unm_adapter_s *adapter);
void unm_cleanup_proc_entries(struct unm_adapter_s *adapter);
int nx_read_lro_state(char *buf, char **start, off_t offset, int count,
				int *eof, void *data); 
int nx_write_lro_state(struct file *file, const char *buffer,
		unsigned long count, void *data);
int nx_read_lro_stats(char *buf, char **start, off_t offset, int count,
		      int *eof, void *data);
int nx_read_auto_fw_reset(char *buf, char **start, off_t offset, int count,
		      int *eof, void *data);
int nx_write_lro_stats(struct file *file, const char *buffer,
		       unsigned long count, void *data);
int nx_write_auto_fw_reset(struct file *file, const char *buffer,
		       unsigned long count, void *data);


/*Contains all the procfs related fucntions here */
static struct proc_dir_entry *unm_proc_dir_entry;

/*
 * Gets the proc file directory where the procfs files are created.
 *
 * Parameters:
 *	None
 *
 * Returns:
 *	NULL - If the file system is not created.
 *	The directory that was created.
 */
struct proc_dir_entry *nx_nic_get_base_procfs_dir(void)
{
	return (unm_proc_dir_entry);
}
int unm_init_proc_drv_dir(void) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	unm_proc_dir_entry = proc_mkdir(unm_nic_driver_name, init_net.proc_net);
#else
	unm_proc_dir_entry = proc_mkdir(unm_nic_driver_name, proc_net);
#endif
	if(!unm_proc_dir_entry) {
		printk(KERN_WARNING "%s: Unable to create /proc/net/%s",
		       unm_nic_driver_name, unm_nic_driver_name);
		return -ENOMEM;
	}
	unm_proc_dir_entry->owner = THIS_MODULE;
	return 0;
}
void unm_cleanup_proc_drv_entries(void) {

	if (unm_proc_dir_entry != NULL) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		remove_proc_entry(unm_proc_dir_entry->name, init_net.proc_net);
#else
		remove_proc_entry(unm_proc_dir_entry->name, proc_net);
#endif
		unm_proc_dir_entry = NULL;
	}
}

void unm_init_proc_entries(struct unm_adapter_s *adapter) {
	struct net_device *netdev = adapter->netdev;
	struct proc_dir_entry *stats_file, *state_file, *rate_file = NULL;
	struct proc_dir_entry *lro_file = NULL;
	struct proc_dir_entry *lro_stats_file = NULL;
	struct proc_dir_entry *auto_fw_reset_file = NULL;

	adapter->dev_dir = proc_mkdir(adapter->procname, unm_proc_dir_entry);
	stats_file = create_proc_entry("stats", S_IRUGO, adapter->dev_dir);
       	state_file = create_proc_entry("led_blink_state", S_IRUGO|S_IWUSR, adapter->dev_dir);
	rate_file = create_proc_entry("led_blink_rate", S_IRUGO|S_IWUSR, adapter->dev_dir);	
	lro_file = create_proc_entry("lro_enabled", S_IRUGO|S_IWUSR, adapter->dev_dir);	
	lro_stats_file = create_proc_entry("lro_stats", S_IRUGO|S_IWUSR, adapter->dev_dir);
	if(adapter->portnum == 0)
		auto_fw_reset_file = create_proc_entry("auto_fw_reset",S_IRUGO|S_IWUSR ,adapter->dev_dir);
	if (stats_file) {
		stats_file->data = netdev;
		stats_file->owner = THIS_MODULE;
		stats_file->read_proc = unm_nic_port_read_proc;
	}
	if (state_file) {
		state_file->data = netdev;
		state_file->owner = THIS_MODULE;
		state_file->read_proc = unm_read_blink_state;
		state_file->write_proc = unm_write_blink_state;
	}
	if (rate_file) {
		rate_file->data = netdev;
		rate_file->owner = THIS_MODULE;
		rate_file->read_proc = unm_read_blink_rate;
		rate_file->write_proc = unm_write_blink_rate;
	}
	if (lro_file) {
		lro_file->data = netdev;
		lro_file->owner = THIS_MODULE;
		lro_file->read_proc = nx_read_lro_state;
		lro_file->write_proc = nx_write_lro_state;
	}
	if (lro_stats_file) {
		lro_stats_file->data = netdev;
		lro_stats_file->owner = THIS_MODULE;
		lro_stats_file->read_proc = nx_read_lro_stats;
		lro_stats_file->write_proc = nx_write_lro_stats;
	}

	if(adapter->portnum == 0) {
		if(auto_fw_reset_file) {
			auto_fw_reset_file->data = netdev;
			auto_fw_reset_file->owner = THIS_MODULE;
			auto_fw_reset_file->read_proc = nx_read_auto_fw_reset;
			auto_fw_reset_file->write_proc = nx_write_auto_fw_reset;
		}
	}

#ifdef UNM_NIC_SNMP
	{
		struct proc_dir_entry *snmp_proc = NULL;
		snmp_proc = create_proc_entry(SNMP_ETH_FILENAME, S_IRUGO, adapter->dev_dir);

		if (snmp_proc) {
			snmp_proc->data = netdev;
			snmp_proc->owner = THIS_MODULE;
			snmp_proc->read_proc = unm_nic_snmp_ether_read_proc;
		}
	}
#endif
}
void unm_cleanup_proc_entries(struct unm_adapter_s *adapter) {

	if (strlen(adapter->procname) > 0) {
#ifdef UNM_NIC_SNMP
		remove_proc_entry(SNMP_ETH_FILENAME, adapter->dev_dir);
#endif
		if(adapter->portnum == 0)
			remove_proc_entry("auto_fw_reset", adapter->dev_dir);
		remove_proc_entry("stats", adapter->dev_dir);
		remove_proc_entry("led_blink_state", adapter->dev_dir);
		remove_proc_entry("led_blink_rate", adapter->dev_dir);
		remove_proc_entry("lro_enabled", adapter->dev_dir);
		remove_proc_entry("lro_stats", adapter->dev_dir);
		remove_proc_entry(adapter->procname, unm_proc_dir_entry);
	}

}

#ifndef ESX
static int nx_procfs_print_tcp_ext_statistics(char *buffer, 
					      nx_ctx_stats_t *pstat);
static int nx_procfs_print_tcp_statistics(char *buffer, nx_ctx_stats_t *pstat);
static int nx_procfs_print_ip_statistics(char *buffer, nx_ctx_stats_t *pstat);
static int nx_procfs_print_ethernet_statistics(char *buffer, 
						nx_ctx_stats_t *pstat);
#endif

static int
unm_nic_port_read_proc(char *buf, char **start, off_t offset, int count,
		       int *eof, void *data)
{
	struct net_device *netdev = (struct net_device *)data;
	int j;
	int len = 0;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	 rds_host_ring_t *host_rds_ring = NULL;
	 sds_host_ring_t         *host_sds_ring = NULL;
	 nx_host_sds_ring_t      *nxhal_sds_ring = NULL;

	if (netdev == NULL) {
		len = sprintf(buf, "No Statistics available now. Device is"
			      " NULL\n");
		*eof = 1;
		return len;
	}
	len = sprintf(buf + len, "%s NIC port statistics\n",
		      unm_nic_driver_name);
	len += sprintf(buf + len, "\n");
	len += sprintf(buf + len, "Interface Name           : %s\n",
		       netdev->name);
	len += sprintf(buf + len, "PCIE Function Number     : %d\n",
		       adapter->ahw.pci_func);
	if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		len += sprintf(buf + len, "Virtual Port Number      : %d\n",
		       adapter->portnum);
		len += sprintf(buf + len, "Physical Port Number     : %d\n",
			adapter->physical_port);
	} else {
		if (adapter->interface_id != -1) {
			len += sprintf(buf + len,
			    "Virtual Port Number      : %d\n",
			    adapter->interface_id >> PORT_BITS);
			len += sprintf(buf + len,
			    "Physical Port Number     : %d\n",
			    adapter->interface_id & PORT_MASK);
		} else {
			len += sprintf(buf + len,
			    "Virtual Port Number      : Unavailable\n");
			len += sprintf(buf + len,
			    "Physical Port Number     : Unavailable\n");
		}
	}
	len += sprintf(buf + len, "Bad SKB                  : %lld\n",
		       adapter->stats.rcvdbadskb);
	len += sprintf(buf + len, "Xmit called              : %lld\n",
		       adapter->stats.xmitcalled);
	len += sprintf(buf + len, "Xmited Frames            : %lld\n",
		       adapter->stats.xmitedframes);
	len += sprintf(buf + len, "Bad SKB length           : %lld\n",
		       adapter->stats.badskblen);
	len += sprintf(buf + len, "Cmd Desc Error           : %lld\n",
		       adapter->stats.nocmddescriptor);
	len += sprintf(buf + len, "Polled for Rcv           : %lld\n",
		       adapter->stats.polled);
	len += sprintf(buf + len, "Received Desc            : %lld\n",
		       adapter->stats.no_rcv);
	len += sprintf(buf + len, "Rcv to stack             : %lld\n",
		       adapter->stats.uphappy);
	len += sprintf(buf + len, "Stack dropped            : %lld\n",
		       adapter->stats.updropped);
	len += sprintf(buf + len, "Xmit finished            : %lld\n",
		       adapter->stats.xmitfinished);
	len += sprintf(buf + len, "Tx dropped SKBs          : %lld\n",
		       adapter->stats.txdropped);
	len += sprintf(buf + len, "TxOutOfBounceBuf dropped : %lld\n",
                       adapter->stats.txOutOfBounceBufDropped);
	len += sprintf(buf + len, "Rcv of CSUMed SKB        : %lld\n",
		       adapter->stats.csummed);
	len += sprintf(buf + len, "skb alloc fail immediate : %lld\n",
		       adapter->stats.alloc_failures_imm);
	len += sprintf(buf + len, "skb alloc fail deferred  : %lld\n",
		       adapter->stats.alloc_failures_def);
	len += sprintf(buf + len, "rcv buf crunch           : %lld\n",
		       adapter->stats.rcv_buf_crunch);
	len += sprintf(buf + len, "\n");
	len += sprintf(buf + len, "Ring Statistics\n");
	len += sprintf(buf + len, "Command Producer    : %d\n",
		       adapter->cmdProducer);
	len += sprintf(buf + len, "LastCommand Consumer: %d\n",
		       adapter->lastCmdConsumer);
	if(adapter->is_up == ADAPTER_UP_MAGIC) {
		for (j = 0; j < adapter->max_rds_rings; j++) {
			host_rds_ring =
			    (rds_host_ring_t *) adapter->nx_dev->rx_ctxs[0]->
		    				rds_rings[j].os_data;
			len += sprintf(buf + len, "Rcv Ring %d\n", j);
			len += sprintf(buf + len, "\tReceive Producer [%d]:"
					" %d\n", j,host_rds_ring->producer);
		}
		for (j = 0; j < adapter->nx_dev->rx_ctxs[0]->
						num_sds_rings; j++) {
                	nxhal_sds_ring  = &adapter->nx_dev->rx_ctxs[0]->
							sds_rings[j];
	                host_sds_ring   = (sds_host_ring_t *)
						nxhal_sds_ring->os_data;
        	        len += sprintf(buf+len, "Rx Status Producer[%d]: %d\n",
                	               j, host_sds_ring->producer);
	                len += sprintf(buf+len, "Rx Status Consumer[%d]: %d\n",
        	                       j, host_sds_ring->consumer);
                	len += sprintf(buf+len, "Rx Status Polled[%d]: %llu\n",
                        	       j, host_sds_ring->polled);
			len += sprintf(buf+len, "Rx Status Processed[%d]: "
				       "%llu\n", j, host_sds_ring->count);
		}
	} else {
		for (j = 0; j < adapter->max_rds_rings; j++) {
			len += sprintf(buf + len, "Rcv Ring %d\n", j);	
			len += sprintf(buf + len, "\tReceive Producer [%d]: "
					"0\n", j);
		}
	}
	len += sprintf(buf + len, "\n");
	if (adapter->link_width < 8) {
		len += sprintf(buf + len, "PCIE Negotiated Link width : x%d\n",
			       adapter->link_width);
	} else {
		len += sprintf(buf + len, "PCIE Negotiated Link width : x%d\n",
			       adapter->link_width);
	}

#ifndef ESX
	if (adapter->nic_net_stats.data) {
		len += nx_procfs_print_tcp_ext_statistics(buf + len,
						adapter->nic_net_stats.data);
		len += nx_procfs_print_tcp_statistics(buf + len,
						adapter->nic_net_stats.data);
		len += nx_procfs_print_ip_statistics(buf + len,
						adapter->nic_net_stats.data);
		len += nx_procfs_print_ethernet_statistics(buf + len,
						adapter->nic_net_stats.data);
	}
#endif

	*eof = 1;
	return len;
}

int nx_write_auto_fw_reset(struct file *file, const char *buffer,
                       unsigned long count, void *data)
{
        struct net_device       *netdev = (struct net_device *)data;
        struct unm_adapter_s    *adapter;
        int                     val = 0;

        adapter = (struct unm_adapter_s *)netdev_priv(netdev);
        if (!capable(CAP_NET_ADMIN)) {
                return -EACCES;
        }


#if (defined(ESX) || defined(ESX_3X_COS))
        memcpy((void *)&val, (const void *)buffer, 1);
#else
        if (copy_from_user(&val, buffer, 1)) {
                return -EFAULT;
        }
#endif
        val = val - '0';
	adapter->auto_fw_reset = val;
        return count;
}


int nx_read_auto_fw_reset(char *buf, char **start, off_t offset, int count,
                                int *eof, void *data)
{

        int len = 0;
        struct net_device *netdev = (struct net_device *)data;
        struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
        len = sprintf(buf,"%d\n", adapter->auto_fw_reset);
        *eof = 1;
        return len ;
}

#ifndef ESX
static char *l2_stats_str[] = {
	"RxBytes",
	"TxBytes",
};

static char *ip_stats_str[] = {
        "IpInReceives",
        "IpInHdrErrors",
        "IpInAddrErrors",
        "IpInNoRoutes",
        "IpInDiscards",
        "IpInDelivers",
        "IpOutRequests",
        "IpOutDiscards",
        "IpOutNoRoutes",
        "IpReasmTimeout",
        "IpReasmReqds",
        "IpReasmOKs",
        "IpReasmFails",
        "IpFragOKs",
        "IpFragFails",
        "IpFragCreates"
};

static char *tcp_stats_str[] = {
        "TcpActiveOpens",
        "TcpPassiveOpens",
        "TcpAttemptFails",
        "TcpEstabResets",
        "TcpCurrEstab",
        "TcpInSegs",
        "TcpOutSegs",
        "TcpSlowOutSegs",
        "TcpRetransSegs",
        "TcpInErrs",
        "TcpOutRsts",
        "TcpOutCollapsed",
        "TcpTimeWaitConns",
};

static char *tcp_ext_stats_str[] = {
	"LroSegs",
	"TcpPureAcks",
	"TcpDelayedAcks",
	"TcpDelayedAckLost",
	"TcpListenDrops",
	"TcpRenoRecovery",
	"TcpSAckRecovery",
	"TcpSAckReneging",
	"TcpFAackReorder",
	"TcpSAckReorder",
	"TcpRenoReorder",
	"TcpTSReorder",
	"TcpFullUndo",
	"TcpPartialUndo",
	"TcpDSAckUndo",
	"TcpLossUndo",
	"TcpLoss",
	"TcpLostRetransmit",
	"TcpRenoFailures",
	"TcpSAckFailures",
	"TcpLossFailures",
	"TcpFastRetrans",
	"TcpForwardRetrans",
	"TcpSlowStartRetrans",
	"TcpRenoRecoveryFail",
	"TcpSAckRecoveryFail",
	"TcpDSAckOldSent",
	"TcpDSAckOfoSent",
	"TcpDSAckRcv",
	"TcpDSAckOfoRcv",
	"TcpAbortFailed",
	"MmemoryPressure",
	"TcpHPMiss",
	"TcpHPAcksMiss",
	"TcpPruneCalled",
	"TcpRcvPruned",
	"TcpOfoPruned",
	"TcpRcvCollapse",
	"PAWSActiveRejected",
	"PAWSEstabRejected",
	"TcpTimeOuts",
	"AbortObSyn",
	"AbortOnData",
	"AbortOnClose",
	"AbortOnMemory",
	"AbortOnTimeOut",
	"AbortOnLinger",
};

static int nx_procfs_print_ethernet_statistics(char *buffer, nx_ctx_stats_t *pstat)
{
	int		len = 0;
	int		ii;
	int		cnt1;
	int		cnt2;
	int		jj;
	__uint64_t	value;

	cnt1 = sizeof (l2_stats_str) / sizeof (char *);
	cnt2 = sizeof (nx_l2_stats_t) / sizeof (__uint64_t);

	len += sprintf(buffer + len, "%s", "\nETHERNET Statistics");
	len += sprintf(buffer + len, "%s", "\n===================\n");

	if (cnt1 != (cnt2 - L2_RSVD)) {
		len += sprintf(buffer + len, "Mismatch in number of elements "
                               "in Ethernet string and statistics: strings[%d] != "
                               "statistics[%d]\n", cnt1, cnt2);
		return (len);
	}

	for (jj = 0; jj < cnt1; jj++) {
		len += sprintf(buffer + len, "%-20s", l2_stats_str[jj]);
		value = 0;
		for (ii = 0; ii < NETXEN_NUM_PEGS; ++ii) {
			value += ((__uint64_t *)&pstat->peg[ii].l2_stats)[jj];
		}
		len += sprintf(buffer + len, "%10llu\n", value);
	}

	return (len);
}

static int nx_procfs_print_ip_statistics(char *buffer, nx_ctx_stats_t *pstat)
{
	int		len = 0;
	int		ii;
	int		cnt1;
	int		cnt2;
	int		jj;
	__uint64_t	value;

	cnt1 = sizeof (ip_stats_str) / sizeof (char *);
	cnt2 = sizeof (nx_ip_mib_t) / sizeof (__uint64_t);

	len += sprintf(buffer + len, "%s", "\nIP Statistics");
	len += sprintf(buffer + len, "%s", "\n=============\n");

	if (cnt1 != cnt2) {
		len += sprintf(buffer + len, "Mismatch in number of elements "
                               "in IP string and statistics: strings[%d] != "
                               "statistics[%d]\n", cnt1, cnt2);
		return (len);
	}

	for (jj = 0; jj < cnt1; jj++) {
		len += sprintf(buffer + len, "%-20s", ip_stats_str[jj]);
		value = 0;
		for (ii = 0; ii < NETXEN_NUM_PEGS; ++ii) {
			value += ((__uint64_t *)&pstat->peg[ii].ip_stats)[jj];
		}
		len += sprintf(buffer + len, "%10llu\n", value);
	}

	return (len);
}

static int nx_procfs_print_tcp_statistics(char *buffer, nx_ctx_stats_t *pstat)
{
	int		len = 0;
	int		ii;
	int		cnt1;
	int		cnt2;
	int		jj;
	__uint64_t	value;

	cnt1 = sizeof (tcp_stats_str) / sizeof (char *);
	cnt2 = sizeof (nx_tcp_mib_t) / sizeof (__uint64_t);

	len += sprintf(buffer + len, "%s", "\nTCP Statistics");
	len += sprintf(buffer + len, "%s", "\n==============\n");

	if (cnt1 != (cnt2 - TCP_RSVD)) {
		len += sprintf(buffer + len, "Mismatch in number of elements "
                               "in TCP string and statistics: strings[%d] != "
                               "statistics[%d]\n", cnt1, cnt2);
		return (len);
	}

	for (jj = 1; jj < cnt1; jj++) { 
		len += sprintf(buffer + len, "%-20s", tcp_stats_str[jj]);
		value = 0;
		for (ii = 0; ii < NETXEN_NUM_PEGS; ++ii) {
			value += ((__uint64_t *)&pstat->peg[ii].tcp_stats)[jj];
		}
		len += sprintf(buffer + len, "%10llu\n", value);
	}
	len += sprintf(buffer + len, "\n");
	return (len);
}

static int nx_procfs_print_tcp_ext_statistics(char *buffer, nx_ctx_stats_t *pstat)
{
	int		len = 0;
	int		ii;
	int		cnt1;
	int		cnt2;
	int		jj;
	__uint64_t	value;

	cnt1 = sizeof (tcp_ext_stats_str) / sizeof (char *);
	cnt2 = sizeof (nx_tcp_ext_stats_t) / sizeof (__uint64_t);

	len += sprintf(buffer + len, "%s", "\nTCP Extended Statistics");
	len += sprintf(buffer + len, "%s", "\n=======================\n");

	if (cnt1 != (cnt2 - TCP_EXT_RSVD)) {
		len += sprintf(buffer + len, "Mismatch in number of elements "
                               "in TCP EXT string and statistics: strings[%d] != "
                               "statistics[%d]\n", cnt1, cnt2);
		return (len);
	}

	for (jj = 0; jj < cnt1; jj++) { 
		len += sprintf(buffer + len, "%-20s", tcp_ext_stats_str[jj]);
		value = 0;
		for (ii = 0; ii < NETXEN_NUM_PEGS; ++ii) {
			value += ((__uint64_t *)&pstat->peg[ii].tcp_ext_stats)[jj];
		}
		len += sprintf(buffer + len, "%10llu\n", value);
	}
	len += sprintf(buffer + len, "\n");
	return (len);
}
#endif

EXPORT_SYMBOL(nx_nic_get_base_procfs_dir);
