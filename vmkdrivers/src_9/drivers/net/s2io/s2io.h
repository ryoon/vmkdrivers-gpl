/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/***************************************************************************
 * s2io.h: A Linux PCI-X Ethernet driver for Neterion 10GbE Server NIC     *
 * Copyright(c) 2002-2008 Neterion Inc.                                    *
 * This software may be used and distributed according to the terms of     *
 * the GNU General Public License (GPL), incorporated herein by reference. *
 * Drivers based on or derived from this code fall under the GPL and must  *
 * retain the authorship, copyright and license notice.  This file is not  *
 * a complete program and may only be used when the entire operating       *
 * system is licensed under the GPL.                                       *
 * See the file COPYING in this distribution for more information.         *
 ***************************************************************************/
#ifndef _S2IO_H
#define _S2IO_H

#if defined(__VMKLNX__)
#include "vmkapi.h"
#endif

#if defined (__VMKLNX__)
#include "s2io-regs.h"
#endif

#define TBD 0
#define S2BIT(loc)		(0x8000000000000000ULL >> (loc))
#define vBIT(val, loc, sz)	(((u64)val) << (64-loc-sz))
#define INV(d)  ((d&0xff)<<24) | (((d>>8)&0xff)<<16) | (((d>>16)&0xff)<<8)| ((d>>24)&0xff)

#ifndef BOOL
#define BOOL    int
#endif

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#undef SUCCESS
#define SUCCESS 0
#define FAILURE -1
#define S2IO_BIT_RESET 1
#define S2IO_BIT_SET 2

/* Macro to added SNMP support in Driver */
#define SNMP_SUPPORT

#define S2IO_MINUS_ONE 0xFFFFFFFFFFFFFFFFULL
#define S2IO_DISABLE_MAC_ENTRY	0xFFFFFFFFFFFFULL
#define S2IO_32_BIT_MINUS_ONE 0xFFFFFFFFULL
#define S2IO_MAX_PCI_CONFIG_SPACE_REINIT 100
#define S2IO_WAIT_FOR_RESET_MAX 25

#define CHECKBIT(value, nbit) (value & (1 << nbit))

/* Maximum time to flicker LED when asked to identify NIC using ethtool */
#define MAX_FLICKER_TIME	60000 /* 60 Secs */

/* Buffer size for 5 buffer mode */
#define MODE5_BUF_SIZE \
	(PAGE_SIZE - sizeof(struct skb_shared_info) - 64 - ALIGN_SIZE)

/* Maximum outstanding splits to be configured into xena. */
enum {
	XENA_ONE_SPLIT_TRANSACTION = 0,
	XENA_TWO_SPLIT_TRANSACTION = 1,
	XENA_THREE_SPLIT_TRANSACTION = 2,
	XENA_FOUR_SPLIT_TRANSACTION = 3,
	XENA_EIGHT_SPLIT_TRANSACTION = 4,
	XENA_TWELVE_SPLIT_TRANSACTION = 5,
	XENA_SIXTEEN_SPLIT_TRANSACTION = 6,
	XENA_THIRTYTWO_SPLIT_TRANSACTION = 7
};
#define XENA_MAX_OUTSTANDING_SPLITS(n) (n << 4)

/*  OS concerned variables and constants */
#define WATCH_DOG_TIMEOUT		15*HZ
#define ALIGN_SIZE				127
#define	PCIX_COMMAND_REGISTER	0x62

#ifndef SET_ETHTOOL_OPS
#define SUPPORTED_10000baseT_Full (1 << 12)
#endif

/*
 * Debug related variables.
 */
/* different debug levels. */
#define	ERR_DBG		0
#define	INIT_DBG	1
#define	INFO_DBG	2
#define	TX_DBG		3
#define	INTR_DBG	4

/* Global variable that defines the present debug level of the driver. */
static int debug_level = ERR_DBG;	/* Default level. */

/* DEBUG message print. */
#define DBG_PRINT(dbg_level, args...)  if(!(debug_level<dbg_level)) printk(args)

/* Protocol assist features of the NIC */
#define L3_CKSUM_OK 0xFFFF
#define L4_CKSUM_OK 0xFFFF

#ifdef TITAN_LEGACY
/*TITAN statistics*/
#define MAC_LINKS       3
#define MAC_AGGREGATORS 2
#define LINK_MAX        91
#define AGGR_MAX        12
#endif

/* Number of receive and transmit s/w statistics*/
#define NUM_RX_SW_STAT 16
#define NUM_TX_SW_STAT 7

#if defined (__VMKLNX__)
/* The default boundary timer interrupt rate of the driver is 250 times/sec */
#define S2IO_DEF_TX_COALESCE_USECS 4000
#endif

/* Driver statistics maintained by driver */
struct swErrStat {
	unsigned long long single_ecc_errs;
	unsigned long long double_ecc_errs;
	unsigned long long parity_err_cnt;
	unsigned long long serious_err_cnt;
	unsigned long long rx_stuck_cnt;
	unsigned long long soft_reset_cnt;
	unsigned long long watchdog_timer_cnt;
	unsigned long long dte_reset_cnt;

	/* skb_null statistics */
	unsigned long long skb_null_s2io_xmit_cnt;
	unsigned long long skb_null_rx_intr_handler_cnt;
	unsigned long long skb_null_tx_intr_handler_cnt;

	/* Error/alarm statistics*/
	unsigned long long tda_err_cnt;
	unsigned long long pfc_err_cnt;
	unsigned long long pcc_err_cnt;
	unsigned long long tti_err_cnt;
	unsigned long long lso_err_cnt;
	unsigned long long tpa_err_cnt;
	unsigned long long sm_err_cnt;
	unsigned long long mac_tmac_err_cnt;
	unsigned long long mac_rmac_err_cnt;
	unsigned long long xgxs_txgxs_err_cnt;
	unsigned long long xgxs_rxgxs_err_cnt;
	unsigned long long rc_err_cnt;
	unsigned long long prc_pcix_err_cnt;
	unsigned long long rpa_err_cnt;
	unsigned long long rda_err_cnt;
	unsigned long long rti_err_cnt;
	unsigned long long mc_err_cnt;
}____cacheline_aligned;

	/* Transfer Code statistics */
struct txFifoStat {
#if defined (__VMKLNX__)
	unsigned long long tx_pkt_cnt;
#endif /* defined (__VMKLNX__) */
	unsigned long long fifo_full_cnt;
	unsigned long long tx_buf_abort_cnt;
	unsigned long long tx_desc_abort_cnt;
	unsigned long long tx_parity_err_cnt;
	unsigned long long tx_link_loss_cnt;
	unsigned long long tx_list_proc_err_cnt;
}____cacheline_aligned;

struct swLROStat {
	/* LRO statistics */
	unsigned long long clubbed_frms_cnt;
	unsigned long long sending_both;
	unsigned long long outof_sequence_pkts;
	unsigned long long flush_max_pkts;
	unsigned long long sum_avg_pkts_aggregated;
	unsigned long long num_aggregations;
}____cacheline_aligned;

struct rxRingStat {
	unsigned long long ring_full_cnt;
	unsigned long long rx_parity_err_cnt;
	unsigned long long rx_abort_cnt;
	unsigned long long rx_parity_abort_cnt;
	unsigned long long rx_rda_fail_cnt;
	unsigned long long rx_unkn_prot_cnt;
	unsigned long long rx_fcs_err_cnt;
	unsigned long long rx_buf_size_err_cnt;
	unsigned long long rx_rxd_corrupt_cnt;
	unsigned long long rx_unkn_err_cnt;
	struct swLROStat sw_lro_stat;
}____cacheline_aligned;

struct swDbgStat {
	/* Other statistics */
	u64 tmac_frms_q[8];
	unsigned long long mem_alloc_fail_cnt;
	unsigned long long pci_map_fail_cnt;
	unsigned long long mem_allocated;
	unsigned long long mem_freed;
	unsigned long long link_up_cnt;
	unsigned long long link_down_cnt;
	unsigned long long link_up_time;
	unsigned long long link_down_time;
}____cacheline_aligned;

/* Xpak releated alarm and warnings */
struct xpakStat {
	u64 alarm_transceiver_temp_high;
	u64 alarm_transceiver_temp_low;
	u64 alarm_laser_bias_current_high;
	u64 alarm_laser_bias_current_low;
	u64 alarm_laser_output_power_high;
	u64 alarm_laser_output_power_low;
	u64 warn_transceiver_temp_high;
	u64 warn_transceiver_temp_low;
	u64 warn_laser_bias_current_high;
	u64 warn_laser_bias_current_low;
	u64 warn_laser_output_power_high;
	u64 warn_laser_output_power_low;
	u64 xpak_regs_stat;
	u32 xpak_timer_count;
}____cacheline_aligned;

#ifdef TITAN_LEGACY
struct statLinkBlock {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(SMP_CACHE_BYTES,
   {
#endif
      u64	tx_frms;
      u64	tx_ttl_eth_octets;
      u64	tx_data_octets;
      u64	tx_mcst_frms;
      u64	tx_bcst_frms;
      u64	tx_ucst_frms;
      u64	tx_tagged_frms;
      u64	tx_vld_ip;
      u64	tx_vld_ip_octets;
      u64	tx_icmp;
      u64	tx_tcp;
      u64	tx_rst_tcp;
      u64	tx_udp;
      u64	tx_unknown_protocol;
      u64	tx_parse_error;
      u64	tx_pause_ctrl_frms;
      u64	tx_lacpdu_frms;
      u64	tx_marker_pdu_frms;
      u64	tx_marker_resp_pdu_frms;
      u64	tx_drop_ip;
      u64	tx_xgmii_char1_match;
      u64	tx_xgmii_char2_match;
      u64	tx_xgmii_column1_match;
      u64	tx_xgmii_column2_match;
      u64	tx_drop_frms;
      u64	tx_any_err_frms;
      u64	rx_ttl_frms;
      u64	rx_vld_frms;
      u64	rx_offld_frms;
      u64	rx_ttl_eth_octets;
      u64	rx_data_octets;
      u64	rx_offld_octets;
      u64	rx_vld_mcst_frms;
      u64	rx_vld_bcst_frms;
      u64	rx_accepted_ucst_frms;
      u64	rx_accepted_nucst_frms;
      u64	rx_tagged_frms;
      u64	rx_long_frms;
      u64	rx_usized_frms;
      u64	rx_osized_frms;
      u64	rx_frag_frms;
      u64	rx_jabber_frms;
      u64	rx_ttl_64_frms;
      u64	rx_ttl_65_127_frms;
      u64	rx_ttl_128_255_frms;
      u64	rx_ttl_256_511_frms;
      u64	rx_ttl_512_1023_frms;
      u64	rx_ttl_1024_1518_frms;
      u64	rx_ttl_1519_4095_frms;
      u64	rx_ttl_40956_8191_frms;
      u64	rx_ttl_8192_max_frms;
      u64	rx_ttl_gt_max_frms;
      u64	rx_ip;
      u64	rx_ip_octets;
      u64	rx_hdr_err_ip;
      u64	rx_icmp;
      u64	rx_tcp;
      u64	rx_udp;
      u64	rx_err_tcp;
      u64	rx_pause_cnt;
      u64	rx_pause_ctrl_frms;
      u64	rx_unsup_ctrl_frms;
      u64	rx_in_rng_len_err_frms;
      u64	rx_out_rng_len_err_frms;
      u64	rx_drop_frms;
      u64	rx_discarded_frms;
      u64	rx_drop_ip;
      u64	rx_err_drp_udp;
      u64	rx_lacpdu_frms;
      u64	rx_marker_pdu_frms;
      u64	rx_marker_resp_pdu_frms;
      u64	rx_unknown_pdu_frms;
      u64	rx_illegal_pdu_frms;
      u64	rx_fcs_discard;
      u64	rx_len_discard;
      u64	rx_pf_discard;
      u64	rx_trash_discard;
      u64	rx_rts_discard;
      u64	rx_wol_discard;
      u64	rx_red_discard;
      u64	rx_ingm_full_discard;
      u64	rx_xgmii_data_err_cnt;
      u64	rx_xgmii_ctrl_err_cnt;
      u64	rx_xgmii_err_sym;
      u64	rx_xgmii_char1_match;
      u64	rx_xgmii_char2_match;
      u64	rx_xgmii_column1_match;
      u64	rx_xgmii_column2_match;
      u64	rx_local_fault;
      u64	rx_remote_fault;
      u64	rx_queue_full;
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}____cacheline_aligned;

struct statsAggrBlock {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(SMP_CACHE_BYTES,
   {  
#endif
      u64	tx_frms;
      u64	tx_mcst_frms;
      u64	tx_bcst_frms;
      u64	tx_discarded_frms;
      u64	tx_errored_frms;
      u64	rx_frms;
      u64	rx_data_octets;
      u64	rx_mcst_frms;
      u64	rx_bcst_frms;
      u64	rx_discarded_frms;
      u64	rx_errored_frms;
      u64	rx_unknown_protocol_frms;
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}____cacheline_aligned;

#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
VMK_ASSERT_LIST(S2IO_TITAN_LEGACY_STRUCT_ALIGN,
   VMK_ASSERT_ON_COMPILE(sizeof(struct statLinkBlock)%SMP_CACHE_BYTES == 0);
   VMK_ASSERT_ON_COMPILE(sizeof(struct statsAggrBlock)%SMP_CACHE_BYTES == 0);
)
#endif
#endif 

/* The statistics block of Xena */
struct stat_block {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(128,
   {
#endif
/* Tx MAC statistics counters. */
#ifndef TITAN_LEGACY
	u32 tmac_data_octets;
	u32 tmac_frms;
	u64 tmac_drop_frms;
	u32 tmac_bcst_frms;
	u32 tmac_mcst_frms;
	u64 tmac_pause_ctrl_frms;
	u32 tmac_ucst_frms;
	u32 tmac_ttl_octets;
	u32 tmac_any_err_frms;
	u32 tmac_nucst_frms;
	u64 tmac_ttl_less_fb_octets;
	u64 tmac_vld_ip_octets;
	u32 tmac_drop_ip;
	u32 tmac_vld_ip;
	u32 tmac_rst_tcp;
	u32 tmac_icmp;
	u64 tmac_tcp;
	u32 reserved_0;
	u32 tmac_udp;

/* Rx MAC Statistics counters. */
	u32 rmac_data_octets;
	u32 rmac_vld_frms;
	u64 rmac_fcs_err_frms;
	u64 rmac_drop_frms;
	u32 rmac_vld_bcst_frms;
	u32 rmac_vld_mcst_frms;
	u32 rmac_out_rng_len_err_frms;
	u32 rmac_in_rng_len_err_frms;
	u64 rmac_long_frms;
	u64 rmac_pause_ctrl_frms;
	u64 rmac_unsup_ctrl_frms;
	u32 rmac_accepted_ucst_frms;
	u32 rmac_ttl_octets;
	u32 rmac_discarded_frms;
	u32 rmac_accepted_nucst_frms;
	u32 reserved_1;
	u32 rmac_drop_events;
	u64 rmac_ttl_less_fb_octets;
	u64 rmac_ttl_frms;
	u64 reserved_2;
	u32 rmac_usized_frms;
	u32 reserved_3;
	u32 rmac_frag_frms;
	u32 rmac_osized_frms;
	u32 reserved_4;
	u32 rmac_jabber_frms;
	u64 rmac_ttl_64_frms;
	u64 rmac_ttl_65_127_frms;
	u64 reserved_5;
	u64 rmac_ttl_128_255_frms;
	u64 rmac_ttl_256_511_frms;
	u64 reserved_6;
	u64 rmac_ttl_512_1023_frms;
	u64 rmac_ttl_1024_1518_frms;
	u32 rmac_ip;
	u32 reserved_7;
	u64 rmac_ip_octets;
	u32 rmac_drop_ip;
	u32 rmac_hdr_err_ip;
	u32 reserved_8;
	u32 rmac_icmp;
	u64 rmac_tcp;
	u32 rmac_err_drp_udp;
	u32 rmac_udp;
	u64 rmac_xgmii_err_sym;
	u64 rmac_frms_q0;
	u64 rmac_frms_q1;
	u64 rmac_frms_q2;
	u64 rmac_frms_q3;
	u64 rmac_frms_q4;
	u64 rmac_frms_q5;
	u64 rmac_frms_q6;
	u64 rmac_frms_q7;
	u16 rmac_full_q3;
	u16 rmac_full_q2;
	u16 rmac_full_q1;
	u16 rmac_full_q0;
	u16 rmac_full_q7;
	u16 rmac_full_q6;
	u16 rmac_full_q5;
	u16 rmac_full_q4;
	u32 reserved_9;
	u32 rmac_pause_cnt;
	u64 rmac_xgmii_data_err_cnt;
	u64 rmac_xgmii_ctrl_err_cnt;
	u32 rmac_err_tcp;
	u32 rmac_accepted_ip;

/* PCI/PCI-X Read transaction statistics. */
	u32 new_rd_req_cnt;
	u32 rd_req_cnt;
	u32 rd_rtry_cnt;
	u32 new_rd_req_rtry_cnt;

/* PCI/PCI-X Write/Read transaction statistics. */
	u32 wr_req_cnt;
	u32 wr_rtry_rd_ack_cnt;
	u32 new_wr_req_rtry_cnt;
	u32 new_wr_req_cnt;
	u32 wr_disc_cnt;
	u32 wr_rtry_cnt;

/*	PCI/PCI-X Write / DMA Transaction statistics. */
	u32 txp_wr_cnt;
	u32 rd_rtry_wr_ack_cnt;
	u32 txd_wr_cnt;
	u32 txd_rd_cnt;
	u32 rxd_wr_cnt;
	u32 rxd_rd_cnt;
	u32 rxf_wr_cnt;
	u32 txf_rd_cnt;

/* Tx MAC statistics overflow counters. */
	u32 tmac_data_octets_oflow;
	u32 tmac_frms_oflow;
	u32 tmac_bcst_frms_oflow;
	u32 tmac_mcst_frms_oflow;
	u32 tmac_ucst_frms_oflow;
	u32 tmac_ttl_octets_oflow;
	u32 tmac_any_err_frms_oflow;
	u32 tmac_nucst_frms_oflow;
	u64 tmac_vlan_frms;
	u32 tmac_drop_ip_oflow;
	u32 tmac_vld_ip_oflow;
	u32 tmac_rst_tcp_oflow;
	u32 tmac_icmp_oflow;
	u32 tpa_unknown_protocol;
	u32 tmac_udp_oflow;
	u32 reserved_10;
	u32 tpa_parse_failure;

/* Rx MAC Statistics overflow counters. */
	u32 rmac_data_octets_oflow;
	u32 rmac_vld_frms_oflow;
	u32 rmac_vld_bcst_frms_oflow;
	u32 rmac_vld_mcst_frms_oflow;
	u32 rmac_accepted_ucst_frms_oflow;
	u32 rmac_ttl_octets_oflow;
	u32 rmac_discarded_frms_oflow;
	u32 rmac_accepted_nucst_frms_oflow;
	u32 rmac_usized_frms_oflow;
	u32 rmac_drop_events_oflow;
	u32 rmac_frag_frms_oflow;
	u32 rmac_osized_frms_oflow;
	u32 rmac_ip_oflow;
	u32 rmac_jabber_frms_oflow;
	u32 rmac_icmp_oflow;
	u32 rmac_drop_ip_oflow;
	u32 rmac_err_drp_udp_oflow;
	u32 rmac_udp_oflow;
	u32 reserved_11;
	u32 rmac_pause_cnt_oflow;
	u64 rmac_ttl_1519_4095_frms;
	u64 rmac_ttl_4096_8191_frms;
	u64 rmac_ttl_8192_max_frms;
	u64 rmac_ttl_gt_max_frms;
	u64 rmac_osized_alt_frms;
	u64 rmac_jabber_alt_frms;
	u64 rmac_gt_max_alt_frms;
	u64 rmac_vlan_frms;
	u32 rmac_len_discard;
	u32 rmac_fcs_discard;
	u32 rmac_pf_discard;
	u32 rmac_da_discard;
	u32 rmac_wol_discard;
	u32 rmac_rts_discard;
	u32 rmac_ingm_full_discard;
	u32 rmac_red_discard;
	u32 reserved_12;
	u32 rmac_accepted_ip_oflow;
	u32 reserved_13;
	u32 link_fault_cnt;
	u8  buffer[20];
#else
	struct statLinkBlock stats_link_info[MAC_LINKS];
	struct statsAggrBlock stats_aggr_info[MAC_AGGREGATORS];
#endif
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}__attribute__ ((aligned (128)));

/* Macros for vlan tag handling */
#define S2IO_DO_NOT_STRIP_VLAN_TAG 		0
#define S2IO_STRIP_VLAN_TAG			1
#define S2IO_DEFAULT_STRIP_MODE_VLAN_TAG	2

/* Macros for LRO handling */
#define S2IO_DONOT_AGGREGATE			0
#define S2IO_ALWAYS_AGGREGATE			1
#define S2IO_DONT_AGGR_FWD_PKTS			2

/*
 * Structures representing different init time configuration
 * parameters of the NIC.
 */

#define MAX_TX_FIFOS 8
#define MAX_RX_RINGS 8

#define RTH_MIN_RING_NUM 2

/* RTH mask bits */
#define S2IO_RTS_RTH_MASK_IPV4_SRC		0x1
#define S2IO_RTS_RTH_MASK_IPV4_DST		0x2
#define S2IO_RTS_RTH_MASK_IPV6_SRC		0x4
#define S2IO_RTS_RTH_MASK_IPV6_DST		0x8
#define S2IO_RTS_RTH_MASK_L4_SRC		0x10
#define S2IO_RTS_RTH_MASK_L4_DST		0x20

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
#define FIFO_DEFAULT_NUM			MAX_TX_FIFOS
#else
#define FIFO_DEFAULT_NUM			5
#endif

#define FIFO_UDP_MAX_NUM			2 /* 0 - even, 1 -odd ports */
#define FIFO_OTHER_MAX_NUM			1
#if defined(__VMKLNX__)
// Such a large FIFO len is not necessary and is a huge drain on
// memory resources when all tx netqueues are enabled.
#define FIFO_DEFAULT_LEN			512
#else
#define FIFO_DEFAULT_LEN			1024
#endif /* defined (__VMKLNX__) */

#define MAX_TX_DESC    (MAX_AVAILABLE_TXDS)

/* Maintains Per FIFO related information. */
struct tx_fifo_config {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(SMP_CACHE_BYTES,
   {
#endif
#define	MAX_AVAILABLE_TXDS			8192
	u32 fifo_len;	/* specifies len of FIFO upto 8192, ie no of TxDLs */
/* Priority definition */
#define TX_FIFO_PRI_0				0	/*Highest */
#define TX_FIFO_PRI_1				1
#define TX_FIFO_PRI_2				2
#define TX_FIFO_PRI_3				3
#define TX_FIFO_PRI_4				4
#define TX_FIFO_PRI_5				5
#define TX_FIFO_PRI_6				6
#define TX_FIFO_PRI_7				7	/*lowest */
	u8 fifo_priority;	/* specifies pointer level for FIFO */
#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	u8 allocated;
#endif
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}____cacheline_aligned;

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
typedef struct s2io_rx_filter {
        u8 active;
        unsigned char macaddr[ETH_ALEN];
} s2io_rx_filter_t;
#endif

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
#define s2io_local_max(a,b)	(((a) > (b)) ? (a) : (b))
#endif
/* Maintains per Ring related information */
struct rx_ring_config {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(SMP_CACHE_BYTES,
   {
#endif
	u32 num_rxd;		/*No of RxDs per Rx Ring */
#define RX_RING_PRI_0				0	/* highest */
#define RX_RING_PRI_1				1
#define RX_RING_PRI_2				2
#define RX_RING_PRI_3				3
#define RX_RING_PRI_4				4
#define RX_RING_PRI_5				5
#define RX_RING_PRI_6				6
#define RX_RING_PRI_7				7	/* lowest */

	u8 ring_priority;	/*Specifies service priority of ring */
#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
#define S2IO_MAX_QUEUE_RXFILTERS (s2io_local_max(S2IO_XENA_MAX_MAC_ADDRESSES, S2IO_HERC_MAX_MAC_ADDRESSES))
#define S2IO_MAX_AVAIL_RX_FILTERS(config) (config->max_mac_addr-1)
#define S2IO_MAX_AVAIL_FILTERS_PER_RXQ(config) ((config->max_mac_addr/config->rx_ring_num)-1) 
	u8 allocated;
	uint active_filter_count;
	s2io_rx_filter_t filter[S2IO_MAX_QUEUE_RXFILTERS];
#endif
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}____cacheline_aligned;

/* This structure provides contains values of the tunable parameters
 * of the H/W
 */
struct config_param {
/* Tx Side */
	u32 tx_fifo_num;	/*Number of Tx FIFOs */
	/* 0-No steering, 1-priority steering, 2-Default fifo map */
#define	TX_PRIORITY_STEERING				0x1
#define TX_STEERING_DEFAULT 			0x2
	u8 tx_steering_type;

	struct tx_fifo_config tx_cfg[MAX_TX_FIFOS];/*Per-Tx FIFO config */
	u32 max_txds;		/*Max no. of Tx buffer descriptor per TxDL */
	/* Specifies if Tx Intr is UTILZ or PER_LIST type. */
	u64 tx_intr_type;

#define INTA					0
#define MSI_X					2
#define DEF_MSI_X				99
	u8 intr_type;

	u8 napi;

/* Rx Side */
	u8 rst_q_stuck;      /* flag to check for reset on queue stuck */
	u32 rx_ring_num;	/*Number of receive rings */

	struct rx_ring_config rx_cfg[MAX_RX_RINGS];	/*Per-Rx Ring config */

#define HEADER_ETHERNET_II_802_3_SIZE		14
#define HEADER_802_2_SIZE			3
#define HEADER_SNAP_SIZE			5
#define HEADER_VLAN_SIZE			4

#define MIN_MTU					68
#define MAX_PYLD				1500
#define MAX_MTU					(MAX_PYLD+18)
#define MAX_MTU_VLAN				(MAX_PYLD+22)
#define MAX_PYLD_JUMBO				9600
#define MAX_MTU_JUMBO				(MAX_PYLD_JUMBO+18)
#define MAX_MTU_JUMBO_VLAN			(MAX_PYLD_JUMBO+22)
	u16 bus_speed;
#define	NO_STEERING				0
#define	PORT_STEERING				0x1
#define	RTH_STEERING				0x2
#define	RX_TOS_STEERING				0x3
/* The driver assumes a default setting of RTH_STEERING internally and
 * will enable some rings based on it.
 * If the user enables rth_steering on the driver load, then the number of
 * rings enabled will be equal to the number of cpus (max of 8), which can
 * be overridden by the user.
 */
#define RTH_STEERING_DEFAULT 0x4
	u8 rx_steering_type;
#define	MAX_STEERABLE_PORTS			256
	int steer_ports[MAX_STEERABLE_PORTS];
	int port_type;
#define	SP	2
#define	DP	1
	int max_mc_addr;	/* xena=64 herc=256 */
	int max_mac_addr;	/* xena=16 herc=64 */
	int mc_start_offset;	/* xena=16 herc=64 */
	u8 multiq;
	u8 vlan_tag_strip;

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	spinlock_t netqueue_lock;   /* netqueue api serialization  lock */
	u32 n_tx_fifo_allocated;    /* tx channels allocated so far */
	u32 n_rx_ring_allocated;    /* rx channels allocated so far */
#endif
}____cacheline_aligned;

/* Structure representing MAC Addrs */
struct mac_addr {
	u8 mac_addr[ETH_ALEN];
};

/* Structure that represent every FIFO element in the BAR1
 * Address location.
 */
struct TxFIFO_element {
	u64 TxDL_Pointer;

	u64 List_Control;
#define TX_FIFO_LAST_TXD_NUM(val)		vBIT(val, 0, 8)
#define TX_FIFO_FIRST_LIST			S2BIT(14)
#define TX_FIFO_LAST_LIST			S2BIT(15)
#define TX_FIFO_FIRSTNLAST_LIST			vBIT(3, 14, 2)
#define TX_FIFO_SPECIAL_FUNC			S2BIT(23)
#define TX_FIFO_DS_NO_SNOOP			S2BIT(31)
#define TX_FIFO_BUFF_NO_SNOOP			S2BIT(30)
};

/* Tx descriptor structure */
struct TxD {
	u64 Control_1;
/* bit mask */
#define TXD_LIST_OWN_XENA			S2BIT(7)
#define TXD_T_CODE				(S2BIT(12)|S2BIT(13)|\
						S2BIT(14)|S2BIT(15))
#define TXD_T_CODE_OK(val)			(|(val & TXD_T_CODE))
#define GET_TXD_T_CODE(val)			((val & TXD_T_CODE) >> 48)
#define TXD_GATHER_CODE				(S2BIT(22) | S2BIT(23))
#define TXD_GATHER_CODE_FIRST			S2BIT(22)
#define TXD_GATHER_CODE_LAST			S2BIT(23)
#define TXD_TCP_LSO_EN				S2BIT(30)
#define TXD_UDP_COF_EN				S2BIT(31)
#define TXD_UFO_EN				S2BIT(31) | S2BIT(30)
#define TXD_TCP_LSO_MSS(val)			vBIT(val, 34, 14)
#define TXD_UFO_MSS(val)			vBIT(val, 34, 14)
#define TXD_BUFFER0_SIZE(val)			vBIT(val, 48, 16)

	u64 Control_2;
#define TXD_TX_CKO_CONTROL			(S2BIT(5)|S2BIT(6)|S2BIT(7))
#define TXD_TX_CKO_IPV4_EN			S2BIT(5)
#define TXD_TX_CKO_TCP_EN			S2BIT(6)
#define TXD_TX_CKO_UDP_EN			S2BIT(7)
#define TXD_VLAN_ENABLE				S2BIT(15)
#define TXD_VLAN_TAG(val)			vBIT(val, 16, 16)
#define TXD_INT_NUMBER(val)			vBIT(val, 34, 6)
#define TXD_INT_TYPE_PER_LIST			S2BIT(47)
#define TXD_INT_TYPE_UTILZ			S2BIT(46)
#define TXD_SET_MARKER				vBIT(0x6, 0, 4)

	u64 Buffer_Pointer;
	u64 Host_Control;	/* reserved for host */
};

/* Structure to hold the phy and virt addr of every TxDL. */
struct list_info_hold {
	dma_addr_t list_phy_addr;
	void *list_virt_addr;
};

/* Rx descriptor structure for 1 buffer mode */
struct RxD_t {
	u64 Host_Control;	/* reserved for host */
	u64 Control_1;
#define RXD_OWN_XENA				S2BIT(7)
#define RXD_T_CODE				(S2BIT(12)|S2BIT(13)|\
						S2BIT(14)|S2BIT(15))
#define GET_RXD_T_CODE(val)			((val & RXD_T_CODE) >> 48)
#define RXD_FRAME_PROTO				vBIT(0xFFFF, 24, 8)
#define RXD_FRAME_VLAN_TAG			S2BIT(24)
#define RXD_FRAME_PROTO_IPV4			S2BIT(27)
#define RXD_FRAME_PROTO_IPV6			S2BIT(28)
#define RXD_FRAME_IP_FRAG			S2BIT(29)
#define RXD_FRAME_PROTO_TCP			S2BIT(30)
#define RXD_FRAME_PROTO_UDP			S2BIT(31)
#define RXD_TCP_OR_UDP_FRAME		(RXD_FRAME_PROTO_TCP | \
									RXD_FRAME_PROTO_UDP)
#define RXD_TCP_IPV4_FRAME			(RXD_FRAME_PROTO_IPV4 | \
									RXD_FRAME_PROTO_TCP) 
#define RXD_GET_L3_CKSUM(val)			((u16)(val >> 16) & 0xFFFF)
#define RXD_GET_L4_CKSUM(val)			((u16)(val) & 0xFFFF)

	u64 Control_2;
#define	THE_RXD_MARK				0x3
#define	SET_RXD_MARKER				vBIT(THE_RXD_MARK, 0, 2)
#define	GET_RXD_MARKER(ctrl)			((ctrl & SET_RXD_MARKER) >> 62)

#define MASK_VLAN_TAG				vBIT(0xFFFF, 48, 16)
#define SET_VLAN_TAG(val)			vBIT(val, 48, 16)
#define SET_NUM_TAG(val)			vBIT(val, 16, 32)
};

#define BUF0_LEN	26
#define BUF1_LEN	1

/* Rx descriptor structure for 1 buffer mode */
struct RxD1 {
	struct RxD_t h;

#define MASK_BUFFER0_SIZE_1			vBIT(0x3FFF, 2, 14)
#define SET_BUFFER0_SIZE_1(val)			vBIT(val, 2, 14)
#define RXD_GET_BUFFER0_SIZE_1(_Control_2) \
	(u16)((_Control_2 & MASK_BUFFER0_SIZE_1) >> 48)
	u64 Buffer0_ptr;
};

/* Rx descriptor structure for 3 or 2 buffer mode */
struct RxD3 {
	struct RxD_t h;

#define MASK_BUFFER0_SIZE_3			vBIT(0xFF, 8, 8)
#define MASK_BUFFER1_SIZE_3			vBIT(0xFFFF, 16, 16)
#define MASK_BUFFER2_SIZE_3			vBIT(0xFFFF, 32, 16)
#define SET_BUFFER0_SIZE_3(val)			vBIT(val, 8, 8)
#define SET_BUFFER1_SIZE_3(val)			vBIT(val, 16, 16)
#define SET_BUFFER2_SIZE_3(val)			vBIT(val, 32, 16)

#define RXD_GET_BUFFER0_SIZE_3(Control_2) \
	(u8)((Control_2 & MASK_BUFFER0_SIZE_3) >> 48)
#define RXD_GET_BUFFER1_SIZE_3(Control_2) \
	(u16)((Control_2 & MASK_BUFFER1_SIZE_3) >> 32)
#define RXD_GET_BUFFER2_SIZE_3(Control_2) \
	(u16)((Control_2 & MASK_BUFFER2_SIZE_3) >> 16)

	u64 Buffer0_ptr;
	u64 Buffer1_ptr;
	u64 Buffer2_ptr;
};

/* Rx descriptor structure for 5 buffer mode */
struct RxD5 {
#ifdef __BIG_ENDIAN
	u32 Host_Control;
	u32 Control_3;
#else
	u32 Control_3;
	u32 Host_Control;
#endif
#define MASK_BUFFER3_SIZE_5			vBIT(0xFFFF, 32, 16)
#define SET_BUFFER3_SIZE_5(val)			vBIT(val, 32, 16)
#define MASK_BUFFER4_SIZE_5			vBIT(0xFFFF, 48, 16)
#define SET_BUFFER4_SIZE_5(val)			vBIT(val, 48, 16)

#define RXD_GET_BUFFER3_SIZE_5(Control_3) \
	(u16)((Control_3 & MASK_BUFFER3_SIZE_5) >> 16)
#define RXD_GET_BUFFER4_SIZE_5(Control_3) \
	(u16)(Control_3 & MASK_BUFFER4_SIZE_5)

	u64 Control_1;
	u64 Control_2;
#define MASK_BUFFER0_SIZE_5			vBIT(0xFFFF, 0, 16)
#define MASK_BUFFER1_SIZE_5			vBIT(0xFFFF, 16, 16)
#define MASK_BUFFER2_SIZE_5			vBIT(0xFFFF, 32, 16)
#define SET_BUFFER0_SIZE_5(val)			vBIT(val, 0, 16)
#define SET_BUFFER1_SIZE_5(val)			vBIT(val, 16, 16)
#define SET_BUFFER2_SIZE_5(val)			vBIT(val, 32, 16)

#define RXD_GET_BUFFER0_SIZE_5(Control_2) \
	(u16)((Control_2 & MASK_BUFFER0_SIZE_5) >> 48)
#define RXD_GET_BUFFER1_SIZE_5(Control_2) \
	(u16)((Control_2 & MASK_BUFFER1_SIZE_5) >> 32)
#define RXD_GET_BUFFER2_SIZE_5(Control_2) \
	(u16)((Control_2 & MASK_BUFFER2_SIZE_5) >> 16)

	u64 Buffer0_ptr;
	u64 Buffer1_ptr;
	u64 Buffer2_ptr;
	u64 Buffer3_ptr;
	u64 Buffer4_ptr;
};

/* Structure that represents the Rx descriptor block which contains
 * 128 Rx descriptors.
 */
struct RxD_block {
#define MAX_RXDS_PER_BLOCK_1            127
	struct RxD1 rxd[MAX_RXDS_PER_BLOCK_1];

	u64 reserved_0;
#define END_OF_BLOCK    0xFEFFFFFFFFFFFFFFULL
	u64 reserved_1;		/* 0xFEFFFFFFFFFFFFFF to mark last
				 * Rxd in this blk */
	u64 reserved_2_pNext_RxD_block;	/* Logical ptr to next */
	u64 pNext_RxD_Blk_physical;	/* Buff0_ptr.In a 32 bit arch
					 * the upper 32 bits should
					 * be 0 */
};

#define SIZE_OF_BLOCK	4096

#define RXD_MODE_1	0 /* One Buffer mode */
#define RXD_MODE_3B	1 /* Two Buffer mode */
#define RXD_MODE_5	2 /* Five Buffer mode */

/* Structure to hold virtual addresses of Buf0 and Buf1 in
 * 2buf mode. */
struct buffAdd {
	void *ba_0_org ____cacheline_aligned;
	void *ba_1_org ____cacheline_aligned;
	void *ba_0;
	void *ba_1;
};

/* Structure which stores all the MAC control parameters */

/* This structure stores the offset of the RxD in the ring
 * from which the Rx Interrupt processor can start picking
 * up the RxDs for processing.
 */
struct rx_curr_get_info {
	u32 block_index;
	u32 offset;
}____cacheline_aligned;

struct rx_curr_put_info {
	u32 block_index;
	u32 offset;
}____cacheline_aligned;

/* This structure stores the offset of the TxDl in the FIFO
 * from which the Tx Interrupt processor can start picking
 * up the TxDLs for send complete interrupt processing.
 */
struct tx_curr_get_info {
	u32 offset;
	u32 fifo_len;
}____cacheline_aligned;

struct rxd_info {
	void *virt_addr;
	dma_addr_t dma_addr;
}____cacheline_aligned;

/* Structure that holds the Phy and virt addresses of the Blocks */
struct rx_block_info {
	void *block_virt_addr;
	dma_addr_t block_dma_addr;
	struct rxd_info *rxds;
};

/* Data structure to represent a LRO session */
struct lro {
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   VMK_PADDED_STRUCT(SMP_CACHE_BYTES,
   {
#endif
	struct sk_buff	*parent;
	struct sk_buff	*last_frag;
	struct iphdr	*iph;
	struct tcphdr	*tcph;
	u32		tcp_next_seq;
	u32		tcp_ack;
	u32		cur_tsval;
	__be32	cur_tsecr;
	int		total_len;
	int		frags_len;
	int		sg_num;
	int		in_use;
	u16		window;
	u16		vlan_tag;
	u8		saw_ts;
#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
   })
#endif
}____cacheline_aligned;

#define MIN_LRO_PACKETS	2
#define MAX_LRO_PACKETS	10

#define LRO_AGGR_PACKET		1
#define LRO_BEG_AGGR		3
#define LRO_FLUSH_SESSION	4
#define LRO_FLUSH_BOTH		2

/*
 * Interrupt count per 10 milliseconds, ie.., 5000 ints/sec.
 */
#define MAX_INTERRUPT_COUNT 50

/* Ring specific structure */
struct ring_info {

	/* per-ring ISR counter (used for MSI-X only) */
	unsigned long long     msix_intr_cnt;
	unsigned long interrupt_count;
	unsigned long jiffies;

#define MAX_RX_UFC_A 4
#define MIN_RX_UFC_A 1
	unsigned long ufc_a;
	unsigned long ufc_b;
	unsigned long ufc_c;
	unsigned long ufc_d;

	unsigned long urange_a;
	unsigned long urange_b;
	unsigned long urange_c;

#define MIN_RX_TIMER_VAL 8
	int rx_timer_val;
	int rx_timer_val_saved;

	/* per-ring buffer counter */
	u32 rx_bufs_left;

	/* per-ring napi flag*/
	struct napi_struct napi;
	u8 config_napi;


	/* The ring number */
	int ring_no;
#define MAX_LRO_SESSIONS	32
	struct	lro lro0_n[MAX_LRO_SESSIONS];
	u8		lro;
	u8		aggr_ack;
	u8		max_pkts_aggr;

	/* copy of sp->rxd_mode flag */
	int rxd_mode;

	/* Number of rxds per block for the rxd_mode */
	int rxd_count;

	/* copy of sp pointer */
	struct s2io_nic *nic;

	/* copy of sp->dev pointer */
	struct net_device *dev;

	/* copy of sp->pdev pointer */
	struct pci_dev *pdev;

	/*
	 *  Place holders for the virtual and physical addresses of
	 *  all the Rx Blocks
	 */
	struct rx_block_info *rx_blocks ____cacheline_aligned;
	int block_count;
	int pkt_cnt;

	/*
	 * Put pointer info which indictes which RxD has to be replenished
	 * with a new buffer.
	 */
	struct rx_curr_put_info rx_curr_put_info;

	/*
	 * Get pointer info which indictes which is the last RxD that was
	 * processed by the driver.
	 */
	struct rx_curr_get_info rx_curr_get_info;

	/* Buffer Address store. Used for 2 buffer mode. */
	struct buffAdd **ba;

	/*
	 * On 64 bit platforms for 5 buf mode we need to store the skb pointers,
	 * as we can't save them in RxD Host_Control field which is 32 bit.
	 */
	u64 *skbs;

	/* per-Ring statistics */
	unsigned long rx_packets;
	unsigned long rx_bytes;

	/* This threshold needs to be greater than 1 */
#define PAUSE_STUCK_THRESHOLD 3
	/* number of times in a row the queue is stuck */
	u8 queue_pause_cnt;
	u64 prev_rx_packets;

	 /*per-ring ethtool stats*/
	struct rxRingStat *rx_ring_stat;

} ring_info_t ____cacheline_aligned;

/* Fifo specific structure */
struct fifo_info {
	/* Place holder of all the TX List's Phy and Virt addresses. */
	struct list_info_hold *list_info;

	/*
	 * Current offset within the tx FIFO where driver would write
	 * new Tx frame
	 */
	struct tx_curr_get_info tx_curr_put_info;

	/*
	 * Current offset within tx FIFO from where the driver would start freeing
	 * the buffers
	 */
	struct tx_curr_get_info tx_curr_get_info;

	spinlock_t tx_lock;
#define FIFO_QUEUE_START 0
#define FIFO_QUEUE_STOP 1
	/* flag used to maintain queue state when MULTIQ is not enabled */
	int queue_state;

	struct s2io_nic *nic;

	/* copy of sp->dev pointer */
	struct net_device *dev;

	/* FIFO number */
	int fifo_no;

	/* Maximum TxDs per TxDL */
	int max_txds;

	/* copy of interrupt type */
	u8 intr_type;

	/* copy of multiq status */
	u8 multiq;

#ifdef NETIF_F_UFO
	u64 *ufo_in_band_v;
#endif
	/*per-fifo ethtool stats*/
	struct txFifoStat *tx_fifo_stat;

}____cacheline_aligned;

/* Information related to the Tx and Rx FIFOs and Rings of Xena
 * is maintained in this structure.
 */
struct mac_info {
/* tx side stuff */
	/* logical pointer of start of each Tx FIFO */
	struct TxFIFO_element __iomem *tx_FIFO_start[MAX_TX_FIFOS];

	/* Fifo specific structure */
	struct fifo_info fifos[MAX_TX_FIFOS];

/* rx side stuff */
	/* Ring specific structure */
	struct ring_info rings[MAX_RX_RINGS];

	u16 rmac_pause_time;
	u16 mc_pause_threshold_q0q3;
	u16 mc_pause_threshold_q4q7;

	void *stats_mem;	/* orignal pointer to allocated mem */
	dma_addr_t stats_mem_phy;	/* Physical address of the stat block */
	u32 stats_mem_sz;
	struct stat_block *stats_info;  /* Logical address of the stat block */
}____cacheline_aligned;

/* structure representing the user defined MAC addresses */
struct usr_addr {
        char addr[ETH_ALEN];
        int usage_cnt;
};

#if defined(__VMKLNX__)
#define SMALL_BLK_CNT   2
#else
#define SMALL_BLK_CNT   10
#endif

/*
 * Structure to keep track of the MSI-X vectors and the corresponding
 * argument registered against each vector
 */
#define MAX_REQUESTED_MSI_X	9
struct s2io_msix_entry
{
	u16 vector;
	u16 entry;
	void *arg;

	u8 type;
#define	MSIX_ALARM_TYPE	1	/* This is used for handling MSI-TX */
#define	MSIX_RING_TYPE	2

	u8 in_use;
#define MSIX_REGISTERED_SUCCESS	0xAA
};

struct msix_info_st {
	u64 addr;
	u64 data;
};

/* SPDM related data */
#define MAX_SUPPORTED_SPDM_ENTRIES 256
struct spdm_entry{
	u64 port_n_entry_control_0;
#define SPDM_PGM_L4_SRC_PORT(port)		vBIT(port, 0, 16)
#define SPDM_PGM_L4_DST_PORT(port)		vBIT(port, 16, 16)
#define SPDM_PGM_TARGET_QUEUE(queue)		vBIT(queue, 53, 3)
#define SPDM_PGM_IS_TCP				S2BIT(59)
#define SPDM_PGM_IS_IPV4			S2BIT(63)

	union {
		u64 ipv4_sa_da;
		u64 ipv6_sa_p0;
	} ip;
	u64 ipv6_sa_p1;
	u64 ipv6_da_p0;
	u64 ipv6_da_p1;
	u64 rsvd_3;
	u64 rsvd_4;

	u64 hash_n_entry_control_1;
#define SPDM_PGM_HASH(hash)			vBIT(hash, 0, 32)
#define SPDM_PGM_ENABLE_ENTRY			S2BIT(63)
};

/* These flags represent the devices temporary state */
enum s2io_device_state_t
{
	__S2IO_STATE_LINK_RESET_TASK = 0,
	__S2IO_STATE_RESET_CARD,
	__S2IO_STATE_CARD_UP
};

/* Structure representing one instance of the NIC */
struct s2io_nic {
	int rxd_mode;
	/*
	* Count of packets to be processed in a given iteration, it will be
	* indicated by the quota field of the device structure when
	* NAPI is enabled.
	*/
	int pkts_to_process;
	struct net_device *dev;
	struct mac_info mac_control;
	struct config_param config;
	struct pci_dev *pdev;
	void __iomem *bar0;
	void __iomem *bar1;

	/* Number of skbs allocated for each RxD. Valid only in case of
	* 5 buffer mode
	*/
	u8	skbs_per_rxd;
	struct mac_addr def_mac_addr[256];
	struct net_device_stats stats;
	int high_dma_flag;
	int device_enabled_once;

	char name[60];

	/* Timer that handles I/O errors/exceptions */
	struct timer_list alarm_timer;

	/* Space to back up the PCI config space */
	u32 config_space[256 / sizeof(u32)];

#define PROMISC     1
#define ALL_MULTI   2

	u16 mc_addr_count;

	u16 m_cast_flg;
	u16 all_multi_pos;
	u16 promisc_flg;

#define MAX_DTE_CHECK_COUNT   40
	int chk_dte_count;

#define MAX_DEVICE_CHECK_COUNT   5
	int chk_device_error_count;

#define MAX_RX_QUEUE_CHECK_COUNT 10
	int chk_rx_queue_count;

	/*  Id timer, used to blink NIC to physically identify NIC. */
	struct timer_list id_timer;

	/*  Restart timer, used to restart NIC if the device is stuck and
	 *  a schedule task that will set the correct Link state once the
	 *  NIC's PHY has stabilized after a state change.
	 */
#ifdef INIT_TQUEUE
	struct tq_struct rst_timer_task;
	struct tq_struct set_link_task;
#else
	struct work_struct rst_timer_task;
	struct work_struct set_link_task;
#endif

	/* Flag that can be used to turn on or turn off the Rx checksum
	 * offload feature.
	 */
	int rx_csum;

	/*  Below variables are used for fifo selection to transmit a packet */

	u8 fifo_mapping[MAX_TX_FIFOS];

	u16 fifo_selector[MAX_TX_FIFOS];

	/* Total fifos for tcp packets */
	u8 total_tcp_fifos;

	/*
	* Beginning index of udp for udp packets
	* Value will be equal to
	* (tx_fifo_num - FIFO_UDP_MAX_NUM - FIFO_OTHER_MAX_NUM)
	*/
	u8 udp_fifo_idx;

	u8 total_udp_fifos;

	/*
	 * Beginning index of fifo for all other packets
	 * Value will be equal to (tx_fifo_num - FIFO_OTHER_MAX_NUM)
	*/
	u8 other_fifo_idx;
	struct napi_struct napi;

	/* Last known link state. */
	u16 last_link_state;
#define	LINK_DOWN	1
#define	LINK_UP		2

	u8 exec_mode;
	u8 serious_err;
	unsigned long long start_time;
#ifdef SNMP_SUPPORT
	char cName[20];
	u64 nMemorySize;
	int nLinkStatus;
	int nFeature;
	char cVersion[20];
	long lDate;
#endif
	struct vlan_group *vlgrp;
#define MSIX_FLG		0xA5
#ifdef CONFIG_PCI_MSI
	int num_entries;
	struct msix_entry *entries;
	int msi_detected;
	wait_queue_head_t msi_wait;
#endif
	struct s2io_msix_entry *s2io_entries;
	char desc[MAX_REQUESTED_MSI_X][25];

	struct msix_info_st msix_info[MAX_REQUESTED_MSI_X];

#define XFRAME_I_DEVICE		1
#define XFRAME_II_DEVICE	2
#ifdef TITAN_LEGACY
#define TITAN_DEVICE		3
#endif

	u8 device_type;
#define XFRAME_E_DEVICE    4
	u8 device_sub_type;

	u8 lro;
	u16 lro_max_aggr_per_sess;
	volatile unsigned long state;
	unsigned long long tx_intr_cnt;
	unsigned long long rx_intr_cnt;
	u64 general_int_mask;
	int spdm_entry;

	struct xpakStat *xpak_stat;
	struct swErrStat *sw_err_stat;
	struct swDbgStat *sw_dbg_stat;

#if defined (__VMKLNX__)
	u32 rx_coalesce_usecs;
	u32 tx_coalesce_usecs;
	// Timer interrupt lock for rx and tx.
	spinlock_t rxtx_ti_lock;
#endif

#define VPD_STRING_LEN 80
	u8  product_name[VPD_STRING_LEN];
	u8  serial_num[VPD_STRING_LEN];
	u8  vlan_strip_flag;
}____cacheline_aligned;

#ifdef __VMK_GCC_BUG_ALIGNMENT_PADDING__
VMK_ASSERT_LIST(S2IO_STRUCT_ALIGN,
   VMK_ASSERT_ON_COMPILE(sizeof(struct stat_block)%128 == 0);
   VMK_ASSERT_ON_COMPILE(sizeof(struct tx_fifo_config)%SMP_CACHE_BYTES == 0);
   VMK_ASSERT_ON_COMPILE(sizeof(struct rx_ring_config)%SMP_CACHE_BYTES == 0);
   VMK_ASSERT_ON_COMPILE(sizeof(struct lro)%SMP_CACHE_BYTES == 0);
)
#endif

/*  OS related system calls */
#ifndef readq
static inline u64 readq(void __iomem *addr)
{
	u64 ret = 0;
	ret = readl(addr + 4);
	ret <<= 32;
	ret |= readl(addr);

	return ret;
}
#endif

#ifndef writeq
static inline void writeq(u64 val, void __iomem *addr)
{
	writel((u32) (val), addr);
	writel((u32) (val >> 32), (addr + 4));
}
#endif

/*
 * Some registers have to be written in a particular order to
 * expect correct hardware operation. The macro SPECIAL_REG_WRITE
 * is used to perform such ordered writes. Defines UF (Upper First)
 * and LF (Lower First) will be used to specify the required write order.
 */
#define UF	1
#define LF	2
static inline void SPECIAL_REG_WRITE(u64 val, void __iomem *addr, int order)
{
	u32 ret;

	if (order == LF) {
		writel((u32) (val), addr);
		ret = readl(addr);
		writel((u32) (val >> 32), (addr + 4));
		ret = readl((addr + 4));
	} else {
		writel((u32) (val >> 32), (addr + 4));
		ret = readl((addr + 4));
		writel((u32) (val), addr);
		ret = readl(addr);
	}
}

/*  Interrupt related values of Xena */

#define ENABLE_INTRS    1
#define DISABLE_INTRS   2

/*  Highest level interrupt blocks */
#define TX_PIC_INTR			(0x0001<<0)
#define TX_DMA_INTR			(0x0001<<1)
#define TX_MAC_INTR			(0x0001<<2)
#define TX_XGXS_INTR			(0x0001<<3)
#define TX_TRAFFIC_INTR			(0x0001<<4)
#define RX_PIC_INTR			(0x0001<<5)
#define RX_DMA_INTR			(0x0001<<6)
#define RX_MAC_INTR			(0x0001<<7)
#define RX_XGXS_INTR			(0x0001<<8)
#define RX_TRAFFIC_INTR			(0x0001<<9)
#define MC_INTR				(0x0001<<10)
#define ENA_ALL_INTRS			(TX_PIC_INTR | \
					TX_DMA_INTR | \
					TX_MAC_INTR | \
					TX_XGXS_INTR | \
					TX_TRAFFIC_INTR | \
					RX_PIC_INTR | \
					RX_DMA_INTR | \
					RX_MAC_INTR | \
					RX_XGXS_INTR | \
					RX_TRAFFIC_INTR | \
					MC_INTR)

/*  Interrupt masks for the general interrupt mask register */
#define DISABLE_ALL_INTRS   0xFFFFFFFFFFFFFFFFULL

#define TXPIC_INT_M			S2BIT(0)
#define TXDMA_INT_M			S2BIT(1)
#define TXMAC_INT_M			S2BIT(2)
#define TXXGXS_INT_M			S2BIT(3)
#define TXTRAFFIC_INT_M			S2BIT(8)
#define PIC_RX_INT_M			S2BIT(32)
#define RXDMA_INT_M			S2BIT(33)
#define RXMAC_INT_M			S2BIT(34)
#define MC_INT_M			S2BIT(35)
#define RXXGXS_INT_M			S2BIT(36)
#define RXTRAFFIC_INT_M			S2BIT(40)

/*  PIC level Interrupts TODO*/

/*  DMA level Inressupts */
#define TXDMA_PFC_INT_M			S2BIT(0)
#define TXDMA_PCC_INT_M			S2BIT(2)

/*  PFC block interrupts */
#define PFC_MISC_ERR_1			S2BIT(0) /* FIFO full Interrupt */

/* PCC block interrupts. */
#define	PCC_FB_ECC_ERR	   vBIT(0xff, 16, 8)	/* Interrupt to indicate
						   PCC_FB_ECC Error. */

#define RXD_GET_VLAN_TAG(Control_2) (u16)(Control_2 & MASK_VLAN_TAG)
/*
 * Prototype declaration.
 */
static int __devinit s2io_init_nic(struct pci_dev *pdev,
				   const struct pci_device_id *pre);
static void __devexit s2io_rem_nic(struct pci_dev *pdev);
static int init_shared_mem(struct s2io_nic *sp);
static void free_shared_mem(struct s2io_nic *sp);
static int init_nic(struct s2io_nic *nic);
static int rx_intr_handler(struct ring_info *ring_data, int budget);
static void s2io_txpic_intr_handle(struct s2io_nic *sp);
static void tx_intr_handler(struct fifo_info *fifo_data);
static void s2io_handle_errors(void *dev_id);
static int s2io_starter(void);
static void s2io_closer(void);
static void s2io_tx_watchdog(struct net_device *dev);
static void s2io_set_multicast(struct net_device *dev);
static int rx_osm_handler(struct ring_info *ring_data, struct RxD_t *rxdp);
static void s2io_link(struct s2io_nic *sp, int link);
static void s2io_reset(struct s2io_nic *sp);
#if defined(HAVE_NETDEV_POLL)
static int s2io_poll_msix(struct napi_struct *napi, int budget);
static int s2io_poll_inta(struct napi_struct *napi, int budget);
#endif
static void s2io_init_pci(struct s2io_nic *sp);
static int do_s2io_prog_unicast(struct net_device *dev, u8 *addr);
static void s2io_alarm_handle(unsigned long data);
#ifndef SET_ETHTOOL_OPS
static int s2io_ethtool(struct net_device *dev, struct ifreq *rq);
#endif
#ifdef CONFIG_PCI_MSI
static void store_xmsi_data(struct s2io_nic *nic);
static void restore_xmsi_data(struct s2io_nic *nic);
static irqreturn_t s2io_msix_ring_handle(int irq, void *dev_id);
static irqreturn_t s2io_msix_fifo_handle(int irq, void *dev_id);
static int s2io_enable_msi_x(struct s2io_nic *nic);
#endif
static irqreturn_t s2io_isr(int irq, void *dev_id);
static int verify_xena_quiescence(struct s2io_nic *sp);
#ifdef SET_ETHTOOL_OPS
static const struct ethtool_ops netdev_ethtool_ops;
#endif
static void s2io_set_link(struct work_struct *work);
static int s2io_set_swapper(struct s2io_nic *sp);
static void s2io_card_down(struct s2io_nic *nic);
static int s2io_card_up(struct s2io_nic *nic);
static int get_xena_rev_id(struct pci_dev *pdev);
static int spdm_extract_table(void *data, struct s2io_nic *nic);
static int spdm_configure(struct s2io_nic *nic, struct spdm_cfg *info);
static int wait_for_cmd_complete(void *addr, u64 busy_bit, int bit_state);
static int s2io_add_isr(struct s2io_nic *sp);
static void s2io_rem_isr(struct s2io_nic *sp);
#ifdef CONFIG_PM
static int s2io_pm_suspend(struct pci_dev *pdev, pm_message_t state);
static int s2io_pm_resume(struct pci_dev *pdev);
#endif

#ifdef SNMP_SUPPORT

#define S2IODIRNAME		"S2IO"

#if defined(__VMKLNX__)
#define BDFILENAME		"S2IO/BDInfo"
#define PADAPFILENAME		"S2IO/PhyAdap"
#else /* !defined(__VMKLNX__) */
#define BDFILENAME		"BDInfo"
#define PADAPFILENAME		"PhyAdap"
#endif /* defined(__VMKLNX__) */

#define ERROR_PROC_DIR		-20
#define ERROR_PROC_ENTRY	-21

struct stDrvData {
	struct stBaseDrv *pBaseDrv;
};

struct stPhyAdap {
	int m_nIndex;
	char m_cName[20];
};
struct stBaseDrv {
	char m_cName[21];
	int m_nStatus;
	char m_cVersion[21];
	int m_nFeature;
	unsigned long long m_nMemorySize;
	char m_cDate[21];
	int m_nPhyCnt;
	char m_cPhyIndex[21];
	int m_dev_id;
	int m_ven_id;
	u32 m_tx_intr_cnt;
	u32 m_rx_intr_cnt;
	u32 m_intr_type;
	u32 m_tx_fifo_num;
	u32 m_rx_ring_num;
	int m_rxd_mode;
	u8  m_lro;
	u16 m_lro_max_pkts;
	u8 m_napi;
	u8 m_rx_steering_type;
	int m_vlan_tag_strip;
	int m_rx_csum;
	int m_tx_csum;
	int m_sg;
	int m_ufo;
	int m_tso;
	int m_fifo_len;
	int m_rx_ring_size;
	int m_rth_bucket_size;
	u64 m_tx_urng;
	u64 m_rx_urng;
	u64 m_tx_ufc;
	u64 m_rx_ufc;
	struct stPhyAdap m_stPhyAdap[5];
};

struct stPhyData {
	int m_nIndex;
	unsigned char m_cDesc[20];
	int m_nMode;
	int m_nType;
	char m_cSpeed[20];
	unsigned char m_cPMAC[20];
	unsigned char m_cCMAC[20];
	int m_nLinkStatus;
	int m_nPCISlot;
	int m_nPCIBus;
	int m_nIRQ;
	int m_nCollision;
	int m_nMulticast;

	unsigned long long m_nRxBytes;
	unsigned long long m_nRxDropped;
	unsigned long long m_nRxErrors;
	unsigned long long m_nRxPackets;
	unsigned long long m_nTxBytes;
	unsigned long long m_nTxDropped;
	unsigned long long m_nTxErrors;
	unsigned long long m_nTxPackets;
};

static int s2io_bdsnmp_init(struct net_device *dev);
static void s2io_bdsnmp_rem(struct net_device *dev);
#endif

static int s2io_club_tcp_session(struct ring_info *ring, u8 *buffer, u8 **tcp,
	u32 *tcp_len, struct lro **lro, struct RxD_t *rxdp,
	struct s2io_nic *sp);
static void clear_lro_session(struct lro *lro);
static void queue_rx_frame(struct sk_buff *skb, u16 vlan_tag);
static void update_L3L4_header(struct s2io_nic *sp, struct lro *lro,
int ring_no);
static void lro_append_pkt(struct s2io_nic *sp, struct lro *lro,
	struct sk_buff *skb, u32 tcp_len, u8 aggr_ack, int ring_no);
static void amd_8132_update_hyper_tx(u8 set_reset);

#ifdef CONFIG_PCI_MSI
static void restore_xmsi_data(struct s2io_nic *nic);
#endif
static void do_s2io_store_unicast_mc(struct s2io_nic *sp);
static void do_s2io_restore_unicast_mc(struct s2io_nic *sp);
static u64 do_s2io_read_unicast_mc(struct s2io_nic *sp, int offset);
static int do_s2io_add_mc(struct s2io_nic *sp, u8 *addr);
static int do_s2io_add_mac(struct s2io_nic *sp, u64 addr, int off);
static void do_s2io_prog_da_steering(struct s2io_nic *sp, int index);
static int do_s2io_delete_unicast_mc(struct s2io_nic *sp, u64 addr);
static void print_static_param_list(struct net_device *dev);
static pci_ers_result_t s2io_io_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state);
static pci_ers_result_t s2io_io_slot_reset(struct pci_dev *pdev);
static void s2io_io_resume(struct pci_dev *pdev);
#ifdef NETIF_F_GSO
#define s2io_tcp_mss(skb) skb_shinfo(skb)->gso_size
#define s2io_udp_mss(skb) skb_shinfo(skb)->gso_size
#define s2io_offload_type(skb) skb_shinfo(skb)->gso_type
#endif

#ifdef module_param
#define S2IO_PARM_INT(X, def_val) \
	static unsigned int X = def_val;\
		module_param(X , uint, 0);
#endif

#if defined(__VMKLNX__)
/*
 * For LRO: Since vmware kernel doesn't define these constants
 */
#define TCPOPT_EOL              0       /* End of options */
#define TCPOPT_NOP              1       /* Padding */
#define TCPOPT_SACK_PERM        4       /* SACK Permitted */
#define TCPOPT_SACK             5       /* SACK Block */
#define TCPOPT_TIMESTAMP        8       /* Better RTT estimations/PAWS */
#define TCPOLEN_TIMESTAMP      10
#define TCPOLEN_TSTAMP_ALIGNED 12
#endif

#endif				/* _S2IO_H */
