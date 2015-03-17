/*
 * Copyright (c) 2008-2009 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 Intel Corporation.  All rights reserved.
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

#ifndef _LIBFCOE_H
#define _LIBFCOE_H

#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <scsi/fc/fc_fcoe.h>
#include <scsi/libfc.h>

/*
 * FIP tunable parameters.
 */
#define FCOE_CTLR_START_DELAY	2000	/* mS after first adv. to choose FCF */
#define FCOE_CTRL_SOL_TOV	2000	/* min. solicitation interval (mS) */
#define FCOE_CTLR_FCF_LIMIT	20	/* max. number of FCF entries */
#define FCOE_CTLR_VN2VN_LOGIN_LIMIT 3	/* max. VN2VN rport login retries */ //VN2VN add

extern unsigned int libfcoe_debug_logging;
#if defined(__VMKLNX__)
/* libfcoe log level*/
extern vmk_LogComponent libfcoeLog;
#endif /* #if defined(__VMKLNX__) */

#define LIBFCOE_LOGGING     0x01 /* General logging, not categorized */
#define LIBFCOE_FIP_LOGGING 0x02 /* FIP logging */

#if defined(__VMKLNX__)
#define LIBFCOE_CHECK_LOGGING(LEVEL, CMD)                               \
do {                                                                    \
        if (unlikely(vmk_LogGetCurrentLogLevel(libfcoeLog) & LEVEL))    \
                do {                                                    \
                        CMD;                                            \
                } while (0);                                            \
} while (0)
#else
#define LIBFCOE_CHECK_LOGGING(LEVEL, CMD)               \
do {                                                    \
        if (unlikely(libfcoe_debug_logging & LEVEL))    \
                do {                                    \
                        CMD;                            \
                } while (0);                            \
} while (0)
#endif /* #if defined(__VMKLNX__) */

#define LIBFCOE_DBG(fmt, args...)                                       \
        LIBFCOE_CHECK_LOGGING(LIBFCOE_LOGGING,                          \
                              printk(KERN_INFO "libfcoe: " fmt, ##args);)

#define LIBFCOE_FIP_DBG(fip, fmt, args...)                              \
        LIBFCOE_CHECK_LOGGING(LIBFCOE_FIP_LOGGING,                      \
                              printk(KERN_INFO "host%d: fip: " fmt,     \
                                     (fip)->lp->host->host_no, ##args);)


#define ETH_ADDR_LENGTH  6
#define WWN_LENGTH_BYTES 8

/**
 * enum fip_state - internal state of FCoE controller.
 * @FIP_ST_DISABLED:	controller has been disabled or not yet enabled.
 * @FIP_ST_LINK_WAIT:	the physical link is down or unusable.
 * @FIP_ST_AUTO:	determining whether to use FIP or non-FIP mode.
 * @FIP_ST_NON_FIP:	non-FIP mode selected.
 * @FIP_ST_ENABLED:	FIP mode selected.
 * @FIP_ST_VNMP_START:	VN2VN multipath mode start, wait
 * @FIP_ST_VNMP_PROBE1:	VN2VN sent first probe, listening
 * @FIP_ST_VNMP_PROBE2:	VN2VN sent second probe, listening
 * @FIP_ST_VNMP_CLAIM:	VN2VN sent claim, waiting for responses
 * @FIP_ST_VNMP_UP:	VN2VN multipath mode operation
 */
enum fip_state {
	FIP_ST_DISABLED,
	FIP_ST_LINK_WAIT,
	FIP_ST_AUTO,
	FIP_ST_NON_FIP,
	FIP_ST_ENABLED,
	FIP_ST_VNMP_START,
	FIP_ST_VNMP_PROBE1,
	FIP_ST_VNMP_PROBE2,
	FIP_ST_VNMP_CLAIM,
	FIP_ST_VNMP_UP,
};

#define FIP_MODE_AUTO		FIP_ST_AUTO
#define FIP_MODE_NON_FIP	FIP_ST_NON_FIP
#define FIP_MODE_FABRIC		FIP_ST_ENABLED
#define FIP_MODE_VN2VN		FIP_ST_VNMP_START

/*
 * struct fcoe_ctlr - FCoE Controller and FIP state
 * @state:	   internal FIP state for network link and FIP or non-FIP mode.
 * @mode:	   LLD-selected mode.
 * @lp:		   &fc_lport: libfc local port.
 * @sel_fcf:	   currently selected FCF, or NULL.
 * @fcfs:	   list of discovered FCFs.
 * @fcf_count:	   number of discovered FCF entries.
 * @sol_time:	   time when a multicast solicitation was last sent.
 * @sel_time:	   time after which to select an FCF.
 * @port_ka_time:  time of next port keep-alive.
 * @ctlr_ka_time:  time of next controller keep-alive.
 * @timer:	   timer struct used for all delayed events.
 * @timer_work:	   &work_struct for doing keep-alives and resets.
 * @recv_work:	   &work_struct for receiving FIP frames.
 * @fip_recv_list: list of received FIP frames.
 * @user_mfs:	   configured maximum FC frame size, including FC header.
 * @flogi_oxid:    exchange ID of most recent fabric login.
 * @flogi_count:   number of FLOGI attempts in AUTO mode.
 * @reset_req:
 * @map_dest:	   use the FC_MAP mode for destination MAC addresses.
 * @spma:	   supports SPMA server-provided MACs mode
 * @send_ctlr_ka:  need to send controller keep alive
 * @send_port_ka:  need to send port keep alives
 * @dest_addr:	   MAC address of the selected FC forwarder.
 * @ctl_src_addr:  the native MAC address of our local port.
 * @vlan_id:
 * @send:	   LLD-supplied function to handle sending FIP Ethernet frames
 * @update_mac:    LLD-supplied function to handle changes to MAC addresses.
 * @get_src_addr:  LLD-supplied function to supply a source MAC address.
 * @ctlr_lock:	   lock protecting this structure.
 *
 * This structure is used by all FCoE drivers.  It contains information
 * needed by all FCoE low-level drivers (LLDs) as well as internal state
 * for FIP, and fields shared with the LLDS.
 */
struct fcoe_ctlr {
	enum fip_state state;
	enum fip_state mode;
	struct fc_lport *lp;
	struct fcoe_fcf *sel_fcf;
	struct list_head fcfs;
	u16 fcf_count;
        u8 probe_tries;
	u32 port_id;
	unsigned long sol_time;
	unsigned long sel_time;
	unsigned long port_ka_time;
	unsigned long ctlr_ka_time;
	struct timer_list timer;
	struct work_struct timer_work;
	struct work_struct recv_work;
	struct sk_buff_head fip_recv_list;
	u16 user_mfs;
	u16 flogi_oxid;
	u8 flogi_count;
	u8 reset_req;
	u8 map_dest;
	u8 spma;
	u8 send_ctlr_ka;
	u8 send_port_ka;
	u8 dest_addr[ETH_ALEN];
	u8 ctl_src_addr[ETH_ALEN];
#if defined (__VMKLNX__)
	u16 vlan_id;
/*
 * Some drivers discovers VLAN ID internally and there's
 * no need to send the FIP VLAN Discovery frame.
 * The driver can tell libfcoe not to send the frame
 * by setting the vlan_id field to FCOE_FIP_NO_VLAN_DISCOVER
 * (an invalid value).
 */
#define FCOE_FIP_NO_VLAN_DISCOVERY    0xffff
#endif

	void (*send)(struct fcoe_ctlr *, struct sk_buff *);
	void (*update_mac)(struct fc_lport *, u8 *addr);
	u8 * (*get_src_addr)(struct fc_lport *);
#if defined(__VMKLNX__)
/*
 * Linux drivers don't maintain binary compatibility, hence
 * they don't care about the size of data structure. ESXi
 * maintains binary compatibility with previous release
 * hence the size of the struct needs to be the same. Original
 * code is exceeding the size of this struct, so we have to
 * manage it some how, hence used pointer instead of inline
 * allocated memory for it.
 */
	spinlock_t *ctlr_lock;
#else
	struct mutex ctlr_mutex;
#endif /* defined(__VMKLNX__) */
	struct list_head remote_vn_ports;
};

/*
 * This structure is the definition for API at the version
 * 9_2_2_x and older. Do not use for any other purpose other
 * than checking the size for binary compatibility
 */
struct fcoe_ctlr_9_2_2_x {
        enum fip_state state;
        enum fip_state mode;
        struct fc_lport *lp;
        struct fcoe_fcf *sel_fcf;
        struct list_head fcfs;
        u16 fcf_count;
        unsigned long sol_time;
        unsigned long sel_time;
        unsigned long port_ka_time;
        unsigned long ctlr_ka_time;
        struct timer_list timer;
        struct work_struct timer_work;
        struct work_struct recv_work;
        struct sk_buff_head fip_recv_list;
        u16 user_mfs;
        u16 flogi_oxid;
        u8 flogi_count;
        u8 reset_req;
        u8 map_dest;
        u8 spma;
        u8 send_ctlr_ka;
        u8 send_port_ka;
        u8 dest_addr[ETH_ALEN];
        u8 ctl_src_addr[ETH_ALEN];
#if defined (__VMKLNX__)
        u16 vlan_id;
#endif
        void (*send)(struct fcoe_ctlr *, struct sk_buff *);
        void (*update_mac)(struct fc_lport *, u8 *addr);
        u8 * (*get_src_addr)(struct fc_lport *);
        spinlock_t lock;
};

/*
 * struct fcoe_fcf - Fibre-Channel Forwarder
 * @list:	 list linkage
 * @time:	 system time (jiffies) when an advertisement was last received
 * @switch_name: WWN of switch from advertisement
 * @fabric_name: WWN of fabric from advertisement
 * @fc_map:	 FC_MAP value from advertisement
 * @fcf_mac:	 Ethernet address of the FCF
 * @vfid:	 virtual fabric ID
 * @pri:	 selection priority, smaller values are better
 * @flags:	 flags received from advertisement
 * @fka_period:	 keep-alive period, in jiffies
 * @fd_flags:
 *
 * A Fibre-Channel Forwarder (FCF) is the entity on the Ethernet that
 * passes FCoE frames on to an FC fabric.  This structure represents
 * one FCF from which advertisements have been received.
 *
 * When looking up an FCF, @switch_name, @fabric_name, @fc_map, @vfid, and
 * @fcf_mac together form the lookup key.
 */
struct fcoe_fcf {
	struct list_head list;
	unsigned long time;

	u64 switch_name;
	u64 fabric_name;
	u32 fc_map;
	u16 vfid;
	u8 fcf_mac[ETH_ALEN];

	u8 pri;
	u16 flags;
	u32 fka_period;
	u8 fd_flags:1;
};

/**
 * struct fcoe_vnport - VN2VN remote port
 * @list:	list linkage on fcoe_ctlr remote_vn_ports
 * @time:	time of create or last beacon packet received from node
 * @port_name:	world-wide port name
 * @node_name:	world-wide node name
 * @port_id:	FC_ID self-assigned by node
 * @fcoe_len:	max FCoE frame size, not including VLAN or Ethernet headers
 * @flags:	flags from probe or claim
 * @login_count: number of unsuccessful rport logins to this port
 * @enode_mac:	E_Node control MAC address
 * @vn_mac:	VN_Node assigned MAC address for data
 * @rdata:	remote port private data
 * @rcu:	structure for RCU (release consistent update)
 */
struct fcoe_vn_port {
	struct list_head list;
	unsigned long time;

	u64 port_name;
	u64 node_name;
	u32 port_id;
	u16 fcoe_len;
        u16 flags;
	u8 login_count;
	u8 enode_mac[ETH_ALEN];
	u8 vn_mac[ETH_ALEN];
	struct fc_rport_priv *rdata;
	struct rcu_head rcu;
};

/* FIP API functions */
void fcoe_ctlr_init(struct fcoe_ctlr *);
void fcoe_ctlr_destroy(struct fcoe_ctlr *);
void fcoe_ctlr_link_up(struct fcoe_ctlr *);
int fcoe_ctlr_link_down(struct fcoe_ctlr *);
int fcoe_ctlr_els_send(struct fcoe_ctlr *, struct fc_lport *, struct sk_buff *);
void fcoe_ctlr_recv(struct fcoe_ctlr *, struct sk_buff *);
int fcoe_ctlr_recv_flogi(struct fcoe_ctlr *, struct fc_lport *,
			 struct fc_frame *);

/* FIP VN2VN API functions */

void fcoe_ctlr_vn_start(struct fcoe_ctlr *);
int fcoe_ctlr_vn_recv(struct fcoe_ctlr *, struct sk_buff *);
void fcoe_ctlr_vn_timeout(struct fcoe_ctlr *);
int fcoe_ctlr_vn_lookup(struct fcoe_ctlr *, u32, u8 *);

void fcoe_ctlr_disc_stop(struct fc_lport *lport);
void fcoe_ctlr_disc_stop_final(struct fc_lport *lport);
void fcoe_ctlr_disc_recv(struct fc_seq *seq, struct fc_frame *fp,
				struct fc_lport *lport);
void fcoe_ctlr_disc_start(void (*callback)(struct fc_lport *,
						  enum fc_disc_event),
				 struct fc_lport *lport);


u32 fcoe_ctlr_fcoe_size(struct fcoe_ctlr *fip);
void fcoe_ctlr_map_dest(struct fcoe_ctlr *fip);

/* libfcoe funcs */
u64 fcoe_wwn_from_mac(unsigned char mac[], unsigned int, unsigned int);
int fcoe_libfc_config(struct fc_lport *, const struct libfc_function_template *);
void fcoe_disc_init(struct fc_lport *, struct fcoe_ctlr *);

/*
 * Added on 10/06/2011 from open-fcoe.org (fcoe-next.git)
 * Version: Linux 2.6.36
 */
/**
 * is_fip_mode() - returns true if FIP mode selected.
 * @fip:       FCoE controller.
 */
/* _VMKLNX_CODECHECK_: is_fip_mode */
static inline bool is_fip_mode(struct fcoe_ctlr *fip)
{
	return fip->state == FIP_ST_ENABLED;
}

#endif /* _LIBFCOE_H */
