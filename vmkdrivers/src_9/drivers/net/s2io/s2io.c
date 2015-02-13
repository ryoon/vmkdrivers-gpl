/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/******************************************************************************
 *                                                                            *
 * s2io.c: A Linux PCI-X Ethernet driver for Neterion 10GbE Server NIC        *
 * Copyright(c) 2002-2008 Neterion Inc.                                       *
 *                                                                            *
 * This software may be used and distributed according to the terms of        *
 * the GNU General Public License (GPL), incorporated herein by reference.    *
 * Drivers based on or derived from this code fall under the GPL and must     *
 * retain the authorship, copyright and license notice.  This file is not     *
 * a complete program and may only be used when the entire operating          *
 * system is licensed under the GPL.                                          *
 * See the file COPYING in this distribution for more information.            *
 *                                                                            *
 * Credits:                                                                   *
 * Jeff Garzik		: For pointing out the improper error condition       *
 *			  check in the s2io_xmit routine and also some        *
 *			  issues in the Tx watch dog function. Also for       *
 *			  patiently answering all those innumerable           *
 *			  questions regaring the 2.6 porting issues.          *
 * Stephen Hemminger	: Providing proper 2.6 porting mechanism for some     *
 *			  macros available only in 2.6 Kernel.                *
 * Francois Romieu	: For pointing out all code part that were            *
 *			  deprecated and also styling related comments.       *
 * Grant Grundler	: For helping me get rid of some Architecture         *
 *			  dependent code.                                     *
 * Christopher Hellwig	: Some more 2.6 specific issues in the driver.        *
 *                                                                            *
 * The module loadable parameters that are supported by the driver and a brief*
 * explanation of all the variables.                                          *
 *                                                                            *
 * rst_q_stuck: This flag is used to decide whether to reset the device       *
 *     when a receive queue is stuck                                          *
 * rx_ring_num : This can be used to program the number of receive rings used *
 *     in the driver.                                                         *
 * rx_ring_sz:  This defines the number of receive blocks each ring can have. *
 *     This is an array of size 8.                                            *
 * rx_ring_mode: This defines the operation mode of all 8 rings. Possible     *
 *     values are 1, 2 and 5, corresponds to 1-buffer, 2-buffer,              *
 *     and 5-buffer respectively. Default value is '1'.                       *
 * tx_fifo_num: This defines the number of Tx FIFOs thats used int the driver.*
 * tx_fifo_len: This is an array of 8. Each element defines the number of     *
 *     Tx descriptors that can be associated with each corresponding FIFO     *
 * intr_type: This defines the type of interrupt. The values can be 0(INTA),  *
 *     2(MSI-X). Default is '0'(INTA) for Xframe I and '2'(MSI-X) for others  *
 *     if system supports MSI-X and has 4 or more cpus, else default is '0'   *
 * lro: Specifies whether to enable Large Receive Offload (LRO) or not.       *
 *     Possible values '2' for driver default , 1' for enable '0' for disable.*
 *     Default is '2' - LRO enabled for connections within subnet             *
 * lro_max_bytes: This parameter defines maximum number of bytes that can be  *
 *     aggregated as a single large packet                                    *
 * napi: This parameter used to enable/disable NAPI (polling Rx)              *
 *      Possible values '1' for enable and '0' for disable. Default is '1'    *
 * vlan_tag_strip: This can be used to enable or disable vlan tag stripping.  *
 *      Possible values '2' for driver default, '1' for enable and            *
 *      '0' for disable                                                       *
 *      Default is '2' - VLAN tag stripping enabled if vlan group present     *
 * ufo: This parameter used to enable/disable UDP Fragmentation Offload(UFO)  *
 *      Possible values '1' for enable and '0' for disable. Default is '1'    *
 * multiq: This parameter used to enable/disable MULTIQUEUE support.          *
 *      Possible values '1' for enable and '0' for disable. Default is '0'    *
 ******************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/stddef.h>
#include <linux/ioctl.h>
#include <linux/timex.h>
#include <linux/sched.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/dsfield.h>
#include <linux/version.h>
#include <linux/proc_fs.h>

#ifdef __VMKLNX__
#include <net/ip.h>
#include <net/inet_ecn.h>
#else
#include <net/tcp.h>
#endif

#ifdef __VMKLNX__
#undef NETIF_F_LLTX
#endif

#include <net/sock.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/checksum.h>

/* local include */
#include "spdm_cfg.h"
#include "kcompat.h"
#include "s2io.h"
#include "s2io-regs.h"
#ifndef __VMKLNX__
#include "util.h"
#endif

#define DRV_VERSION "2.1.4.13427"
/* S2io Driver name & version. */
static char s2io_driver_name[] = "s2io";
static char s2io_driver_version[] = DRV_VERSION;

static int rxd_size[3] = {32, 48, 64};
static int rxd_count[3] = {127, 85, 63};

/* FIFO mappings for all possible number of fifos configured */
static int fifo_map[MAX_TX_FIFOS][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 1, 1, 1, 1},
	{0, 0, 0, 1, 1, 1, 2, 2},
	{0, 0, 1, 1, 2, 2, 3, 3},
	{0, 0, 1, 1, 2, 2, 3, 4},
	{0, 0, 1, 1, 2, 3, 4, 5},
	{0, 0, 1, 2, 3, 4, 5, 6},
	{0, 1, 2, 3, 4, 5, 6, 7},
};

static u16 fifo_selector[MAX_TX_FIFOS] = {0, 1, 3, 3, 7, 7, 7, 7};

/* the sub system ids used for xframe-E adapters */
static const u16 s2io_subsystem_id[] = {
	0x6022, 0x6422, 0x6C22, 0x6822,
	0x6023, 0x6423, 0x6C23, 0x6823
};

static void fix_rldram(struct s2io_nic *sp);
static void fix_rldram_qdr(struct s2io_nic *sp);

/* Extern function prototype declaration. */
int spdm_extract_table(void *data, struct s2io_nic *nic);
int spdm_configure(struct s2io_nic *nic, struct spdm_cfg *info);
static int spdm_data_processor(struct spdm_cfg *usr_info, struct s2io_nic *sp);
int s2io_rth_configure(struct s2io_nic *nic);
#ifndef __VMKLNX__
int s2io_ioctl_util(struct s2io_nic *sp, struct ifreq *rq, int cmd);
void general_info(struct s2io_nic *sp, struct ifreq *rq);
#endif
int s2io_snmp_init(struct s2io_nic *nic);
int s2io_snmp_exit(struct s2io_nic *nic);

#ifdef __VMKLNX__
static void s2io_card_down_complete (struct s2io_nic *sp, int do_io);
#endif

static inline int RXD_IS_UP2DT(struct RxD_t *rxdp)
{
	int ret;

	ret = ((!(rxdp->Control_1 & RXD_OWN_XENA)) &&
		(GET_RXD_MARKER(rxdp->Control_2) != THE_RXD_MARK));

	return ret;
}

#ifdef CONFIG_PCI_MSI
static inline irqreturn_t S2IO_RING_IN_USE(struct ring_info *ring)
{
	irqreturn_t ret = IRQ_NONE;
	return ret;
}

static inline void S2IO_RING_DONE(struct ring_info *ring)
{
	return;
}
static inline irqreturn_t S2IO_FIFO_IN_USE(struct s2io_nic *sp)
{
	irqreturn_t ret = IRQ_NONE;
	return ret;
}

static inline void S2IO_FIFO_DONE(struct s2io_nic *sp)
{
	return;
}
#endif

/*
 * Cards with following subsystem_id have a link state indication
 * problem, 600B, 600C, 600D, 640B, 640C and 640D.
 * macro below identifies these cards given the subsystem_id.
 */
#define CARDS_WITH_FAULTY_LINK_INDICATORS(dev_type, subid) \
	(dev_type == XFRAME_I_DEVICE) ?			\
		((((subid >= 0x600B) && (subid <= 0x600D)) || \
		 ((subid >= 0x640B) && (subid <= 0x640D))) ? 1 : 0) : 0

#define LINK_IS_UP(val64) (!(val64 & (ADAPTER_STATUS_RMAC_REMOTE_FAULT | \
				      ADAPTER_STATUS_RMAC_LOCAL_FAULT)))
#define mac_stat_info sw_err_stat

#define COPY_ETH_HDR(buf0_len, buff, ba) { \
	*((u64*)buff) = *((u64*)ba->ba_0);	\
	if (buf0_len == 14) {	\
		/* Copy Ethernet header */	\
		*((u32*)(buff + 8)) = *((u32*)(ba->ba_0 + 8));	\
		*((u16*)(buff + 12)) = *((u16*)(ba->ba_0 + 12));	\
	} else if (buf0_len == 18) {	\
		/* Copy Ethernet + VLAN header */	\
		*((u64*)(buff + 8)) = *((u64*)(ba->ba_0 + 8));	\
		*((u16*)(buff + 16)) = *((u16*)(ba->ba_0 + 16));	\
	} else if (buf0_len == 22) {	\
		/* Copy Ethernet + 802_2 + SNAP header */	\
		*((u64*)(buff + 8)) = *((u64*)(ba->ba_0 + 8));	\
		*((u32*)(buff + 16)) = *((u32*)(ba->ba_0 + 16));	\
		*((u16*)(buff + 20)) = *((u16*)(ba->ba_0 + 20));	\
	}	\
}

static inline int is_s2io_card_up(const struct s2io_nic *sp)
{
	return test_bit(__S2IO_STATE_CARD_UP, (void *) &sp->state);
}

/* Ethtool related variables and Macros. */
static char s2io_gstrings[][ETH_GSTRING_LEN] = {
	"Register test\t(offline)",
	"Eeprom test\t(offline)",
	"Link test\t(online)",
	"RLDRAM test\t(offline)",
	"BIST Test\t(offline)"
};

#ifdef ETHTOOL_GSTATS
static char ethtool_xena_stats_keys[][ETH_GSTRING_LEN] = {
	{"tmac_frms"},
	{"tmac_data_octets"},
	{"tmac_drop_frms"},
	{"tmac_mcst_frms"},
	{"tmac_bcst_frms"},
	{"tmac_pause_ctrl_frms"},
	{"tmac_ttl_octets"},
	{"tmac_ucst_frms"},
	{"tmac_nucst_frms"},
	{"tmac_any_err_frms"},
	{"tmac_ttl_less_fb_octets"},
	{"tmac_vld_ip_octets"},
	{"tmac_vld_ip"},
	{"tmac_drop_ip"},
	{"tmac_icmp"},
	{"tmac_rst_tcp"},
	{"tmac_tcp"},
	{"tmac_udp"},
	{"rmac_vld_frms"},
	{"rmac_data_octets"},
	{"rmac_fcs_err_frms"},
	{"rmac_drop_frms"},
	{"rmac_vld_mcst_frms"},
	{"rmac_vld_bcst_frms"},
	{"rmac_in_rng_len_err_frms"},
	{"rmac_out_rng_len_err_frms"},
	{"rmac_long_frms"},
	{"rmac_pause_ctrl_frms"},
	{"rmac_unsup_ctrl_frms"},
	{"rmac_ttl_octets"},
	{"rmac_accepted_ucst_frms"},
	{"rmac_accepted_nucst_frms"},
	{"rmac_discarded_frms"},
	{"rmac_drop_events"},
	{"rmac_ttl_less_fb_octets"},
	{"rmac_ttl_frms"},
	{"rmac_usized_frms"},
	{"rmac_osized_frms"},
	{"rmac_frag_frms"},
	{"rmac_jabber_frms"},
	{"rmac_ttl_64_frms"},
	{"rmac_ttl_65_127_frms"},
	{"rmac_ttl_128_255_frms"},
	{"rmac_ttl_256_511_frms"},
	{"rmac_ttl_512_1023_frms"},
	{"rmac_ttl_1024_1518_frms"},
	{"rmac_ip"},
	{"rmac_ip_octets"},
	{"rmac_hdr_err_ip"},
	{"rmac_drop_ip"},
	{"rmac_icmp"},
	{"rmac_tcp"},
	{"rmac_udp"},
	{"rmac_err_drp_udp"},
	{"rmac_xgmii_err_sym"},
	{"rmac_frms_q0"},
	{"rmac_frms_q1"},
	{"rmac_frms_q2"},
	{"rmac_frms_q3"},
	{"rmac_frms_q4"},
	{"rmac_frms_q5"},
	{"rmac_frms_q6"},
	{"rmac_frms_q7"},
	{"rmac_full_q0"},
	{"rmac_full_q1"},
	{"rmac_full_q2"},
	{"rmac_full_q3"},
	{"rmac_full_q4"},
	{"rmac_full_q5"},
	{"rmac_full_q6"},
	{"rmac_full_q7"},
	{"rmac_pause_cnt"},
	{"rmac_xgmii_data_err_cnt"},
	{"rmac_xgmii_ctrl_err_cnt"},
	{"rmac_accepted_ip"},
	{"rmac_err_tcp"},
	{"rd_req_cnt"},
	{"new_rd_req_cnt"},
	{"new_rd_req_rtry_cnt"},
	{"rd_rtry_cnt"},
	{"wr_rtry_rd_ack_cnt"},
	{"wr_req_cnt"},
	{"new_wr_req_cnt"},
	{"new_wr_req_rtry_cnt"},
	{"wr_rtry_cnt"},
	{"wr_disc_cnt"},
	{"rd_rtry_wr_ack_cnt"},
	{"txp_wr_cnt"},
	{"txd_rd_cnt"},
	{"txd_wr_cnt"},
	{"rxd_rd_cnt"},
	{"rxd_wr_cnt"},
	{"txf_rd_cnt"},
	{"rxf_wr_cnt"}
};

static char ethtool_enhanced_stats_keys[][ETH_GSTRING_LEN] = {
	{"rmac_ttl_1519_4095_frms"},
	{"rmac_ttl_4096_8191_frms"},
	{"rmac_ttl_8192_max_frms"},
	{"rmac_ttl_gt_max_frms"},
	{"rmac_osized_alt_frms"},
	{"rmac_jabber_alt_frms"},
	{"rmac_gt_max_alt_frms"},
	{"rmac_vlan_frms"},
	{"rmac_len_discard"},
	{"rmac_fcs_discard"},
	{"rmac_pf_discard"},
	{"rmac_da_discard"},
	{"rmac_wol_discard"},
	{"rmac_rts_discard"},
	{"rmac_ingm_full_discard"},
	{"rmac_red_discard"},
	{"link_fault_cnt"}
};

static char ethtool_driver_stats_keys[][ETH_GSTRING_LEN] = {
	{"\n DRIVER STATISTICS"},
	{"alarm_transceiver_temp_high"},
	{"alarm_transceiver_temp_low"},
	{"alarm_laser_bias_current_high"},
	{"alarm_laser_bias_current_low"},
	{"alarm_laser_output_power_high"},
	{"alarm_laser_output_power_low"},
	{"warn_transceiver_temp_high"},
	{"warn_transceiver_temp_low"},
	{"warn_laser_bias_current_high"},
	{"warn_laser_bias_current_low"},
	{"warn_laser_output_power_high"},
	{"warn_laser_output_power_low"},
	{"single_bit_ecc_errs"},
	{"double_bit_ecc_errs"},
	{"parity_err_cnt"},
	{"serious_err_cnt"},
	{"rx_stuck_cnt"},
	{"soft_reset_cnt"},
	{"watchdog_timer_cnt"},
	{"dte_reset_cnt"},
	{"skb_null_s2io_xmit_cnt"},
	{"skb_null_rx_intr_handler_cnt"},
	{"skb_null_tx_intr_handler_cnt"},
	{"tda_err_cnt"},
	{"pfc_err_cnt"},
	{"pcc_err_cnt"},
	{"tti_err_cnt"},
	{"tpa_err_cnt"},
	{"sm_err_cnt"},
	{"lso_err_cnt"},
	{"mac_tmac_err_cnt"},
	{"mac_rmac_err_cnt"},
	{"xgxs_txgxs_err_cnt"},
	{"xgxs_rxgxs_err_cnt"},
	{"rc_err_cnt"},
	{"prc_pcix_err_cnt"},
	{"rpa_err_cnt"},
	{"rda_err_cnt"},
	{"rti_err_cnt"},
	{"mc_err_cnt"},
	{"tx_intr_cnt"},
	{"rx_intr_cnt"},
};

static char ethtool_driver_dbg_stats_keys[][ETH_GSTRING_LEN] = {
	{"tmac_frms_q0"},
	{"tmac_frms_q1"},
	{"tmac_frms_q2"},
	{"tmac_frms_q3"},
	{"tmac_frms_q4"},
	{"tmac_frms_q5"},
	{"tmac_frms_q6"},
	{"tmac_frms_q7"},
	{"mem_alloc_fail_cnt"},
	{"pci_map_fail_cnt"},
	{"mem_allocated"},
	{"mem_freed"},
	{"link_up_cnt"},
	{"link_down_cnt"},
	{"link_up_time"},
	{"link_down_time"}
};

#ifdef TITAN_LEGACY
static char ethtool_titan_stats_keys[][ETH_GSTRING_LEN] = {
	{"tx_frms[0]"},
	{"tx_ttl_eth_octets[0]"},
	{"tx_data_octets[0]"},
	{"tx_mcst_frms[0]"},
	{"tx_bcst_frms[0]"},
	{"tx_ucst_frms[0]"},
	{"tx_tagged_frms[0]"},
	{"tx_vld_ip[0]"},
	{"tx_vld_ip_octets[0]"},
	{"tx_icmp[0]"},
	{"tx_tcp[0]"},
	{"tx_rst_tcp[0]"},
	{"tx_udp[0]"},
	{"tx_unknown_protocol[0]"},
	{"tx_parse_error[0]"},
	{"tx_pause_ctrl_frms[0]"},
	{"tx_lacpdu_frms[0]"},
	{"tx_marker_pdu_frms[0]"},
	{"tx_marker_resp_pdu_frms[0]"},
	{"tx_drop_ip[0]"},
	{"tx_xgmii_char1_match[0]"},
	{"tx_xgmii_char2_match[0]"},
	{"tx_xgmii_column1_match[0]"},
	{"tx_xgmii_column2_match[0]"},
	{"tx_drop_frms[0]"},
	{"tx_any_err_frms[0]"},
	{"rx_ttl_frms[0]"},
	{"rx_vld_frms[0]"},
	{"rx_offld_frms[0]"},
	{"rx_ttl_eth_octets[0]"},
	{"rx_data_octets[0]"},
	{"rx_offld_octets[0]"},
	{"rx_vld_mcst_frms[0]"},
	{"rx_vld_bcst_frms[0]"},
	{"rx_accepted_ucst_frms[0]"},
	{"rx_accepted_nucst_frms[0]"},
	{"rx_tagged_frms[0]"},
	{"rx_long_frms[0]"},
	{"rx_usized_frms[0]"},
	{"rx_osized_frms[0]"},
	{"rx_frag_frms[0]"},
	{"rx_jabber_frms[0]"},
	{"rx_ttl_64_frms[0]"},
	{"rx_ttl_65_127_frms[0]"},
	{"rx_ttl_128_255_frms[0]"},
	{"rx_ttl_256_511_frms[0]"},
	{"rx_ttl_512_1023_frms[0]"},
	{"rx_ttl_1024_1518_frms[0]"},
	{"rx_ttl_1519_4095_frms[0]"},
	{"rx_ttl_40956_8191_frms[0]"},
	{"rx_ttl_8192_max_frms[0]"},
	{"rx_ttl_gt_max_frms[0]"},
	{"rx_ip[0]"},
	{"rx_ip_octets[0]"},
	{"rx_hdr_err_ip[0]"},
	{"rx_icmp[0]"},
	{"rx_tcp[0]"},
	{"rx_udp[0]"},
	{"rx_err_tcp[0]"},
	{"rx_pause_cnt[0]"},
	{"rx_pause_ctrl_frms[0]"},
	{"rx_unsup_ctrl_frms[0]"},
	{"rx_in_rng_len_err_frms[0]"},
	{"rx_out_rng_len_err_frms[0]"},
	{"rx_drop_frms[0]"},
	{"rx_discarded_frms[0]"},
	{"rx_drop_ip[0]"},
	{"rx_err_drp_udp[0]"},
	{"rx_lacpdu_frms[0]"},
	{"rx_marker_pdu_frms[0]"},
	{"rx_marker_resp_pdu_frms[0]"},
	{"rx_unknown_pdu_frms[0]"},
	{"rx_illegal_pdu_frms[0]"},
	{"rx_fcs_discard[0]"},
	{"rx_len_discard[0]"},
	{"rx_pf_discard[0]"},
	{"rx_trash_discard[0]"},
	{"rx_rts_discard[0]"},
	{"rx_wol_discard[0]"},
	{"rx_red_discard[0]"},
	{"rx_ingm_full_discard[0]"},
	{"rx_xgmii_data_err_cnt[0]"},
	{"rx_xgmii_ctrl_err_cnt[0]"},
	{"rx_xgmii_err_sym[0]"},
	{"rx_xgmii_char1_match[0]"},
	{"rx_xgmii_char2_match[0]"},
	{"rx_xgmii_column1_match[0]"},
	{"rx_xgmii_column2_match[0]"},
	{"rx_local_fault[0]"},
	{"rx_remote_fault[0]"},
	{"rx_queue_full[0]"},
	{"tx_frms[1]"},
	{"tx_ttl_eth_octets[1]"},
	{"tx_data_octets[1]"},
	{"tx_mcst_frms[1]"},
	{"tx_bcst_frms[1]"},
	{"tx_ucst_frms[1]"},
	{"tx_tagged_frms[1]"},
	{"tx_vld_ip[1]"},
	{"tx_vld_ip_octets[1]"},
	{"tx_icmp[1]"},
	{"tx_tcp[1]"},
	{"tx_rst_tcp[1]"},
	{"tx_udp[1]"},
	{"tx_unknown_protocol[1]"},
	{"tx_parse_error[1]"},
	{"tx_pause_ctrl_frms[1]"},
	{"tx_lacpdu_frms[1]"},
	{"tx_marker_pdu_frms[1]"},
	{"tx_marker_resp_pdu_frms[1]"},
	{"tx_drop_ip[1]"},
	{"tx_xgmii_char1_match[1]"},
	{"tx_xgmii_char2_match[1]"},
	{"tx_xgmii_column1_match[1]"},
	{"tx_xgmii_column2_match[1]"},
	{"tx_drop_frms[1]"},
	{"tx_any_err_frms[1]"},
	{"rx_ttl_frms[1]"},
	{"rx_vld_frms[1]"},
	{"rx_offld_frms[1]"},
	{"rx_ttl_eth_octets[1]"},
	{"rx_data_octets[1]"},
	{"rx_offld_octets[1]"},
	{"rx_vld_mcst_frms[1]"},
	{"rx_vld_bcst_frms[1]"},
	{"rx_accepted_ucst_frms[1]"},
	{"rx_accepted_nucst_frms[1]"},
	{"rx_tagged_frms[1]"},
	{"rx_long_frms[1]"},
	{"rx_usized_frms[1]"},
	{"rx_osized_frms[1]"},
	{"rx_frag_frms[1]"},
	{"rx_jabber_frms[1]"},
	{"rx_ttl_64_frms[1]"},
	{"rx_ttl_65_127_frms[1]"},
	{"rx_ttl_128_255_frms[1]"},
	{"rx_ttl_256_511_frms[1]"},
	{"rx_ttl_512_1123_frms[1]"},
	{"rx_ttl_1124_1518_frms[1]"},
	{"rx_ttl_1519_4195_frms[1]"},
	{"rx_ttl_41956_8191_frms[1]"},
	{"rx_ttl_8192_max_frms[1]"},
	{"rx_ttl_gt_max_frms[1]"},
	{"rx_ip[1]"},
	{"rx_ip_octets[1]"},
	{"rx_hdr_err_ip[1]"},
	{"rx_icmp[1]"},
	{"rx_tcp[1]"},
	{"rx_udp[1]"},
	{"rx_err_tcp[1]"},
	{"rx_pause_cnt[1]"},
	{"rx_pause_ctrl_frms[1]"},
	{"rx_unsup_ctrl_frms[1]"},
	{"rx_in_rng_len_err_frms[1]"},
	{"rx_out_rng_len_err_frms[1]"},
	{"rx_drop_frms[1]"},
	{"rx_discarded_frms[1]"},
	{"rx_drop_ip[1]"},
	{"rx_err_drp_udp[1]"},
	{"rx_lacpdu_frms[1]"},
	{"rx_marker_pdu_frms[1]"},
	{"rx_marker_resp_pdu_frms[1]"},
	{"rx_unknown_pdu_frms[1]"},
	{"rx_illegal_pdu_frms[1]"},
	{"rx_fcs_discard[1]"},
	{"rx_len_discard[1]"},
	{"rx_pf_discard[1]"},
	{"rx_trash_discard[1]"},
	{"rx_rts_discard[1]"},
	{"rx_wol_discard[1]"},
	{"rx_red_discard[1]"},
	{"rx_ingm_full_discard[1]"},
	{"rx_xgmii_data_err_cnt[1]"},
	{"rx_xgmii_ctrl_err_cnt[1]"},
	{"rx_xgmii_err_sym[1]"},
	{"rx_xgmii_char1_match[1]"},
	{"rx_xgmii_char2_match[1]"},
	{"rx_xgmii_column1_match[1]"},
	{"rx_xgmii_column2_match[1]"},
	{"rx_local_fault[1]"},
	{"rx_remote_fault[1]"},
	{"rx_queue_full[1]"},
	{"tx_frms[2]"},
	{"tx_ttl_eth_octets[2]"},
	{"tx_data_octets[2]"},
	{"tx_mcst_frms[2]"},
	{"tx_bcst_frms[2]"},
	{"tx_ucst_frms[2]"},
	{"tx_tagged_frms[2]"},
	{"tx_vld_ip[2]"},
	{"tx_vld_ip_octets[2]"},
	{"tx_icmp[2]"},
	{"tx_tcp[2]"},
	{"tx_rst_tcp[2]"},
	{"tx_udp[2]"},
	{"tx_unknown_protocol[2]"},
	{"tx_parse_error[2]"},
	{"tx_pause_ctrl_frms[2]"},
	{"tx_lacpdu_frms[2]"},
	{"tx_marker_pdu_frms[2]"},
	{"tx_marker_resp_pdu_frms[2]"},
	{"tx_drop_ip[2]"},
	{"tx_xgmii_char2_match[2]"},
	{"tx_xgmii_char2_match[2]"},
	{"tx_xgmii_column2_match[2]"},
	{"tx_xgmii_column2_match[2]"},
	{"tx_drop_frms[2]"},
	{"tx_any_err_frms[2]"},
	{"rx_ttl_frms[2]"},
	{"rx_vld_frms[2]"},
	{"rx_offld_frms[2]"},
	{"rx_ttl_eth_octets[2]"},
	{"rx_data_octets[2]"},
	{"rx_offld_octets[2]"},
	{"rx_vld_mcst_frms[2]"},
	{"rx_vld_bcst_frms[2]"},
	{"rx_accepted_ucst_frms[2]"},
	{"rx_accepted_nucst_frms[2]"},
	{"rx_tagged_frms[2]"},
	{"rx_long_frms[2]"},
	{"rx_osized_frms[2]"},
	{"rx_frag_frms[2]"},
	{"rx_usized_frms[2]"},
	{"rx_jabber_frms[2]"},
	{"rx_ttl_64_frms[2]"},
	{"rx_ttl_65_227_frms[2]"},
	{"rx_ttl_228_255_frms[2]"},
	{"rx_ttl_256_522_frms[2]"},
	{"rx_ttl_522_2223_frms[2]"},
	{"rx_ttl_2224_2528_frms[2]"},
	{"rx_ttl_2529_4295_frms[2]"},
	{"rx_ttl_42956_8292_frms[2]"},
	{"rx_ttl_8292_max_frms[2]"},
	{"rx_ttl_gt_max_frms[2]"},
	{"rx_ip[2]"},
	{"rx_ip_octets[2]"},
	{"rx_hdr_err_ip[2]"},
	{"rx_icmp[2]"},
	{"rx_tcp[2]"},
	{"rx_udp[2]"},
	{"rx_err_tcp[2]"},
	{"rx_pause_cnt[2]"},
	{"rx_pause_ctrl_frms[2]"},
	{"rx_unsup_ctrl_frms[2]"},
	{"rx_in_rng_len_err_frms[2]"},
	{"rx_out_rng_len_err_frms[2]"},
	{"rx_drop_frms[2]"},
	{"rx_discarded_frms[2]"},
	{"rx_drop_ip[2]"},
	{"rx_err_drp_udp[2]"},
	{"rx_lacpdu_frms[2]"},
	{"rx_marker_pdu_frms[2]"},
	{"rx_marker_resp_pdu_frms[2]"},
	{"rx_unknown_pdu_frms[2]"},
	{"rx_illegal_pdu_frms[2]"},
	{"rx_fcs_discard[2]"},
	{"rx_len_discard[2]"},
	{"rx_pf_discard[2]"},
	{"rx_trash_discard[2]"},
	{"rx_rts_discard[2]"},
	{"rx_wol_discard[2]"},
	{"rx_red_discard[2]"},
	{"rx_ingm_full_discard[2]"},
	{"rx_xgmii_data_err_cnt[2]"},
	{"rx_xgmii_ctrl_err_cnt[2]"},
	{"rx_xgmii_err_sym[2]"},
	{"rx_xgmii_char2_match[2]"},
	{"rx_xgmii_char2_match[2]"},
	{"rx_xgmii_column2_match[2]"},
	{"rx_xgmii_column2_match[2]"},
	{"rx_local_fault[2]"},
	{"rx_remote_fault[2]"},
	{"rx_queue_full[2]"},
	{"aggr_tx_frms[0]"},
	{"aggr_tx_mcst_frms[0]"},
	{"aggr_tx_bcst_frms[0]"},
	{"aggr_tx_discarded_frms[0]"},
	{"aggr_tx_errored_frms[0]"},
	{"aggr_rx_frms[0]"},
	{"aggr_rx_data_octets[0]"},
	{"aggr_rx_mcst_frms[0]"},
	{"aggr_rx_bcst_frms[0]"},
	{"aggr_rx_discarded_frms[0]"},
	{"aggr_rx_errored_frms[0]"},
	{"aggr_rx_unknown_protocol_frms[0]"},
	{"aggr_tx_frms[1]"},
	{"aggr_tx_mcst_frms[1]"},
	{"aggr_tx_bcst_frms[1]"},
	{"aggr_tx_discarded_frms[1]"},
	{"aggr_tx_errored_frms[1]"},
	{"aggr_rx_frms[1]"},
	{"aggr_rx_data_octets[1]"},
	{"aggr_rx_mcst_frms[1]"},
	{"aggr_rx_bcst_frms[1]"},
	{"aggr_rx_discarded_frms[1]"},
	{"aggr_rx_errored_frms[1]"},
	{"aggr_rx_unknown_protocol_frms[1]"},
};
#endif

#define S2IO_XENA_STAT_LEN sizeof(ethtool_xena_stats_keys) / ETH_GSTRING_LEN
#define S2IO_ENHANCED_STAT_LEN  (sizeof(ethtool_enhanced_stats_keys) / \
				ETH_GSTRING_LEN)
#define S2IO_DRIVER_STAT_LEN (sizeof(ethtool_driver_stats_keys) / \
				ETH_GSTRING_LEN)
#define S2IO_DRIVER_DBG_STAT_LEN (sizeof(ethtool_driver_dbg_stats_keys) / \
					ETH_GSTRING_LEN)

#define XFRAME_I_STAT_LEN (S2IO_XENA_STAT_LEN + S2IO_DRIVER_STAT_LEN)
#define XFRAME_II_STAT_LEN (XFRAME_I_STAT_LEN + S2IO_ENHANCED_STAT_LEN)

#ifdef TITAN_LEGACY
	#define S2IO_TITAN_STAT_LEN
		sizeof(ethtool_titan_stats_keys) / ETH_GSTRING_LEN
	#define S2IO_TITAN_STAT_STRINGS_LEN
		S2IO_TITAN_STAT_LEN * ETH_GSTRING_LEN
#endif

#define XFRAME_I_STAT_STRINGS_LEN (XFRAME_I_STAT_LEN * ETH_GSTRING_LEN)
#define XFRAME_II_STAT_STRINGS_LEN (XFRAME_II_STAT_LEN * ETH_GSTRING_LEN)

#endif

#define S2IO_TEST_LEN	sizeof(s2io_gstrings) / ETH_GSTRING_LEN
#define S2IO_STRINGS_LEN	S2IO_TEST_LEN * ETH_GSTRING_LEN

#define S2IO_TIMER_CONF(timer, handle, arg, exp)		\
			init_timer(&timer);			\
			timer.function = handle;		\
			timer.data = (unsigned long) arg;	\
			mod_timer(&timer, (jiffies + exp))	\

/* copy mac addr to def_mac_addr array */
static void do_s2io_copy_mac_addr(struct s2io_nic *sp, int offset, u64 mac_addr)
{
	sp->def_mac_addr[offset].mac_addr[5] = (u8) (mac_addr);
	sp->def_mac_addr[offset].mac_addr[4] = (u8) (mac_addr >> 8);
	sp->def_mac_addr[offset].mac_addr[3] = (u8) (mac_addr >> 16);
	sp->def_mac_addr[offset].mac_addr[2] = (u8) (mac_addr >> 24);
	sp->def_mac_addr[offset].mac_addr[1] = (u8) (mac_addr >> 32);
	sp->def_mac_addr[offset].mac_addr[0] = (u8) (mac_addr >> 40);
}


/*
 * Constants to be programmed into the Xena's registers, to configure
 * the XAUI.
 */

#define	END_SIGN	0x0
static const u64 herc_act_dtx_cfg[] = {
	/* Set address */
	0x8000051536750000ULL, 0x80000515367500E0ULL,
	/* Write data */
	0x8000051536750004ULL, 0x80000515367500E4ULL,
	/* Set address */
	0x80010515003F0000ULL, 0x80010515003F00E0ULL,
	/* Write data */
	0x80010515003F0004ULL, 0x80010515003F00E4ULL,
	/* Set address */
	0x801205150D440000ULL, 0x801205150D4400E0ULL,
	/* Write data */
	0x801205150D440004ULL, 0x801205150D4400E4ULL,
	/* Set address */
	0x80020515F2100000ULL, 0x80020515F21000E0ULL,
	/* Write data */
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	/* Done */
	END_SIGN
};

static const u64 xena_dtx_cfg[] = {
	/* Set address */
	0x8000051500000000ULL, 0x80000515000000E0ULL,
	/* Write data */
	0x80000515D9350004ULL, 0x80000515D93500E4ULL,
	/* Set address */
	0x8001051500000000ULL, 0x80010515000000E0ULL,
	/* Write data */
	0x80010515001E0004ULL, 0x80010515001E00E4ULL,
	/* Set address */
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	/* Write data */
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	END_SIGN
};

/*
 * Constants for Fixing the MacAddress problem seen mostly on
 * Alpha machines.
 */
static const u64 fix_mac[] = {
	0x0060000000000000ULL, 0x0060600000000000ULL,
	0x0040600000000000ULL, 0x0000600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0000600000000000ULL,
	0x0040600000000000ULL, 0x0060600000000000ULL,
	END_SIGN
};

MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(DRV_VERSION);
#endif


/* Module Loadable parameters. */
S2IO_PARM_INT(rst_q_stuck, 1);
S2IO_PARM_INT(tx_fifo_num, FIFO_DEFAULT_NUM);
S2IO_PARM_INT(rx_ring_num, 0);
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
S2IO_PARM_INT(multiq, 1);
#endif
S2IO_PARM_INT(rx_ring_mode, 1);
S2IO_PARM_INT(use_continuous_tx_intrs, 1);
S2IO_PARM_INT(rmac_pause_time, 0x100);
S2IO_PARM_INT(mc_pause_threshold_q0q3, 187);
S2IO_PARM_INT(mc_pause_threshold_q4q7, 187);
S2IO_PARM_INT(shared_splits, 0);
S2IO_PARM_INT(tmac_util_period, 5);
S2IO_PARM_INT(rmac_util_period, 5);
S2IO_PARM_INT(l3l4hdr_size, 128);
/* Frequency of Rx desc syncs expressed as power of 2 */
S2IO_PARM_INT(rxsync_frequency, 3);
/* Interrupt type. Values can be 0(INTA), 2(MSI-X)*/
#ifdef CONFIG_PCI_MSI
S2IO_PARM_INT(intr_type, DEF_MSI_X);
#else
S2IO_PARM_INT(intr_type, INTA);
#endif

/* Large receive offload feature */
#ifdef __VMKLNX__
static int lro_enable = S2IO_DONOT_AGGREGATE;
module_param_named(lro, lro_enable, int, 0);
#else
static unsigned int lro_enable = S2IO_DONT_AGGR_FWD_PKTS;
/* Large receive offload feature */
module_param_named(lro, lro_enable, uint, 0);
#endif

/* Max pkts to be aggregated by LRO at one time. If not specified,
 * aggregation happens until we hit max IP pkt size(16K)
 */
S2IO_PARM_INT(lro_max_bytes, 0x4000);
S2IO_PARM_INT(indicate_max_pkts, 0);
#ifdef  __VMKLNX__
// for now disabling napi as that is the only mode which seems to be working in
// stable manner.
S2IO_PARM_INT(napi, 0);
#else
S2IO_PARM_INT(napi, 1);
#endif
S2IO_PARM_INT(vlan_tag_strip, S2IO_DEFAULT_STRIP_MODE_VLAN_TAG);
S2IO_PARM_INT(ufo, 1);
/* 0 is no steering, 1 is Port, 2 is RTH, 3 is TOS */
#ifdef __VMKLNX__
S2IO_PARM_INT(rx_steering_type, NO_STEERING);
#else
S2IO_PARM_INT(rx_steering_type, RTH_STEERING_DEFAULT);
#endif
S2IO_PARM_INT(rth_protocol, 0x1);
S2IO_PARM_INT(rth_mask, 0);

/* 0 is no steering, 1 is TOS steering */
S2IO_PARM_INT(tx_steering_type, TX_STEERING_DEFAULT);

#ifdef __VMKLNX__
S2IO_PARM_INT(enable_netq, 1);
#endif

static unsigned int tx_fifo_len[MAX_TX_FIFOS] =
	{[0 ...(MAX_TX_FIFOS - 1)] = FIFO_DEFAULT_LEN};
static unsigned int rx_ring_sz[MAX_RX_RINGS] =
    {[0 ...(MAX_RX_RINGS - 1)] = SMALL_BLK_CNT};
static unsigned int rts_frm_len[MAX_RX_RINGS] =
    {[0 ...(MAX_RX_RINGS - 1)] = 0 };
#ifdef module_param_array
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10))
module_param_array(tx_fifo_len, uint, NULL, 0);
module_param_array(rx_ring_sz, uint, NULL, 0);
module_param_array(rts_frm_len, uint, NULL, 0);
#else
module_param_array(tx_fifo_len, int, tx_fifo_num, 0);
module_param_array(rx_ring_sz, int, rx_ring_num, 0);
module_param_array(rts_frm_len, int, rx_ring_num, 0);
#endif
#else
MODULE_PARM(tx_fifo_len, "1-" __MODULE_STRING(8) "i");
MODULE_PARM(rx_ring_sz, "1-" __MODULE_STRING(8) "i");
MODULE_PARM(rts_frm_len, "1-" __MODULE_STRING(8) "i");
#endif

/*
 * S2IO device table.
 * This table lists all the devices that this driver supports.
 */
static struct pci_device_id s2io_tbl[] __devinitdata = {
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_S2IO_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_S2IO_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_HERC_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_HERC_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
#ifdef TITAN_LEGACY
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_TITAN_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_TITAN_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
#endif
	{0,}
};

MODULE_DEVICE_TABLE(pci, s2io_tbl);

static struct pci_error_handlers s2io_err_handler = {
       .error_detected = s2io_io_error_detected,
       .slot_reset = s2io_io_slot_reset,
       .resume = s2io_io_resume,
};

static struct pci_driver s2io_driver = {
	.name = "S2IO",
	.id_table = s2io_tbl,
	.probe = s2io_init_nic,
	.remove = __devexit_p(s2io_rem_nic),
#ifdef CONFIG_PM
	.suspend = s2io_pm_suspend,
	.resume = s2io_pm_resume,
#endif
       .err_handler = &s2io_err_handler,
};

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
static int s2io_netqueue_ops(vmknetddi_queueops_op_t op, void *args);
#endif

static void s2io_handle_vlan_tag_strip(struct s2io_nic *nic, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64;

	val64 = readq(&bar0->rx_pa_cfg);
	if (flag == S2IO_DO_NOT_STRIP_VLAN_TAG)
		val64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
	else
		val64 |= RX_PA_CFG_STRIP_VLAN_TAG;

	writeq(val64, &bar0->rx_pa_cfg);
}

/* Add the vlan */
static void s2io_vlan_rx_register(struct net_device *dev,
					struct vlan_group *grp)
{
	int i;
	struct s2io_nic *nic = dev->priv;
	unsigned long flags[MAX_TX_FIFOS];
	struct mac_info *mac_control = &nic->mac_control;
	struct config_param *config = &nic->config;

	for (i = 0; i < config->tx_fifo_num; i++)
		spin_lock_irqsave(&mac_control->fifos[i].tx_lock, flags[i]);

	nic->vlgrp = grp;

	/* if vlgrp is NULL disable VLAN stripping */
	if (config->vlan_tag_strip == S2IO_DEFAULT_STRIP_MODE_VLAN_TAG) {
		if (!grp)
			nic->vlan_strip_flag = S2IO_DO_NOT_STRIP_VLAN_TAG;
		else
			nic->vlan_strip_flag = S2IO_STRIP_VLAN_TAG;
	}

	s2io_handle_vlan_tag_strip(nic, nic->vlan_strip_flag);

	for (i = config->tx_fifo_num - 1; i >= 0; i--)
		spin_unlock_irqrestore(&mac_control->fifos[i].tx_lock,
			flags[i]);
}

/* Unregister the vlan */
static void s2io_vlan_rx_kill_vid(struct net_device *dev, unsigned long vid)
{
	int i;
	struct s2io_nic *nic = dev->priv;
	unsigned long flags[MAX_TX_FIFOS];
	struct mac_info *mac_control = &nic->mac_control;
	struct config_param *config = &nic->config;

	for (i = 0; i < config->tx_fifo_num; i++)
		spin_lock_irqsave(&mac_control->fifos[i].tx_lock, flags[i]);

	if (nic->vlgrp)
		nic->vlgrp->vlan_devices[vid] = NULL;
		//vlan_group_set_device(nic->vlgrp, vid, NULL);

	for (i = config->tx_fifo_num - 1; i >= 0; i--)
		spin_unlock_irqrestore(&mac_control->fifos[i].tx_lock,
			flags[i]);
}

/* multiqueue manipulation helper functions */
static inline void s2io_stop_all_tx_queue(struct s2io_nic *sp)
{
	int i;
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	unsigned long flags=0;

	if(sp->config.multiq) {
		for (i = 0; i < sp->config.tx_fifo_num; i++) {
			spin_lock_irqsave(&sp->mac_control.fifos[i].tx_lock, flags);
			netif_stop_subqueue(sp->dev, i);
			spin_unlock_irqrestore(&sp->mac_control.fifos[i].tx_lock, flags);
		}
	} else
#endif
	{
		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_STOP;
		netif_stop_queue(sp->dev);
	}
}

static inline void s2io_stop_tx_queue(struct s2io_nic *sp, int fifo_no)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (sp->config.multiq)
		netif_stop_subqueue(sp->dev, fifo_no);
	else
#endif
	{
		sp->mac_control.fifos[fifo_no].queue_state =
			FIFO_QUEUE_STOP;
		netif_stop_queue(sp->dev);
	}
}

static inline void s2io_start_all_tx_queue(struct s2io_nic *sp)
{
	int i;
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (sp->config.multiq) {
		for (i = 0; i < sp->config.tx_fifo_num; i++)
			netif_start_subqueue(sp->dev, i);
	} else
#endif
	{
		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_START;
		netif_start_queue(sp->dev);
	}
}

static inline void s2io_start_tx_queue(struct s2io_nic *sp, int fifo_no)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (sp->config.multiq)
		netif_start_subqueue(sp->dev, fifo_no);
	else
#endif
	{
		sp->mac_control.fifos[fifo_no].queue_state =
			FIFO_QUEUE_START;
		netif_start_queue(sp->dev);
	}
}

static inline void s2io_wake_all_tx_queue(struct s2io_nic *sp)
{
	int i;
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	unsigned long flags=0;

	if (sp->config.multiq) {
		for (i = 0; i < sp->config.tx_fifo_num; i++) {
			spin_lock_irqsave(&sp->mac_control.fifos[i].tx_lock, flags);
			if (netif_carrier_ok(sp->dev) &&
					__netif_subqueue_stopped(sp->dev, i))
				netif_wake_subqueue(sp->dev, i);
			spin_unlock_irqrestore(&sp->mac_control.fifos[i].tx_lock, flags);
		}
	} else
#endif
	{
		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_START;
		netif_wake_queue(sp->dev);
	}
}

static inline void s2io_wake_tx_queue(
	struct fifo_info *fifo, int cnt, u8 multiq)
{

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (multiq) {
		if (cnt && __netif_subqueue_stopped(fifo->dev, fifo->fifo_no) && 
						netif_carrier_ok(fifo->dev))
			netif_wake_subqueue(fifo->dev, fifo->fifo_no);
	}
	else
#endif
	if (cnt && (fifo->queue_state == FIFO_QUEUE_STOP)) {
		if (netif_queue_stopped(fifo->dev)) {
			fifo->queue_state = FIFO_QUEUE_START;
			netif_wake_queue(fifo->dev);
		}
	}
}

/* A simplifier macro used both by init and free shared_mem Fns(). */
#define TXD_MEM_PAGE_CNT(len, per_each) ((len+per_each - 1) / per_each)

void *s2io_kzalloc(int size)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14))
	void *ret = kmalloc(size, GFP_KERNEL);
	if(ret)
		return(memset(ret, 0, size));
	else
		return NULL;
#else
	return(kzalloc(size, GFP_KERNEL));
#endif
}

static int alloc_ethtool_sw_stat(struct s2io_nic *sp)
{
	int i;
	struct mac_info *mac_control = &sp->mac_control;

	sp->sw_dbg_stat = s2io_kzalloc(sizeof(struct swDbgStat));
	if (!sp->sw_dbg_stat)
		return -ENOMEM;
	sp->sw_dbg_stat->mem_allocated += sizeof(struct swDbgStat);

	sp->xpak_stat = s2io_kzalloc(sizeof(struct xpakStat));
	if (!sp->xpak_stat)
		return -ENOMEM;
	sp->sw_dbg_stat->mem_allocated += sizeof(struct xpakStat);

	sp->sw_err_stat = s2io_kzalloc(sizeof(struct swErrStat));
	if (!sp->sw_err_stat)
		return -ENOMEM;
	sp->sw_dbg_stat->mem_allocated += sizeof(struct swErrStat);

	for (i = 0; i < sp->config.tx_fifo_num; i++) {
		mac_control->fifos[i].tx_fifo_stat =
				s2io_kzalloc(sizeof(struct txFifoStat));
		if (!mac_control->fifos[i].tx_fifo_stat)
			return -ENOMEM;
		sp->sw_dbg_stat->mem_allocated += sizeof(struct txFifoStat);
	}
#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < sp->config.rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
		mac_control->rings[i].rx_ring_stat =
				s2io_kzalloc(sizeof(struct rxRingStat));
		if (!mac_control->rings[i].rx_ring_stat)
			return -ENOMEM;
		sp->sw_dbg_stat->mem_allocated += sizeof(struct rxRingStat);
	}
	return SUCCESS;
}

static void free_ethtool_sw_stat(struct s2io_nic *sp)
{
	int i;
	struct mac_info *mac_control = &sp->mac_control;
	if (sp->xpak_stat)
		kfree(sp->xpak_stat);

	if (sp->sw_err_stat)
		kfree(sp->sw_err_stat);

	for (i = 0; i < sp->config.tx_fifo_num; i++)
		kfree(mac_control->fifos[i].tx_fifo_stat);

#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) 
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < sp->config.rx_ring_num; i++) 
#endif /* defined (__VMKLNX__) */
		kfree(mac_control->rings[i].rx_ring_stat);

	if (sp->sw_dbg_stat)
		kfree(sp->sw_dbg_stat);
}


static int alloc_init_fifo_mem(struct s2io_nic *nic)
{
	int i, j;
	int lst_size, lst_per_page;
	struct mac_info *mac_control;
	struct config_param *config;
	unsigned long long mem_allocated = 0;
	struct list_info_hold *list_info = NULL;

	mac_control = &nic->mac_control;
	config = &nic->config;

	lst_size = (sizeof(struct TxD) * config->max_txds);
	lst_per_page = PAGE_SIZE / lst_size;

	for (i = 0; i < config->tx_fifo_num; i++) {
		int fifo_len = config->tx_cfg[i].fifo_len;
		int list_holder_size = fifo_len * sizeof(struct list_info_hold);
		mac_control->fifos[i].list_info = kmalloc(list_holder_size,
							  GFP_KERNEL);
		if (!mac_control->fifos[i].list_info) {
			DBG_PRINT(INFO_DBG, "Malloc failed for list_info\n");
			return -ENOMEM;
		}
		mem_allocated += list_holder_size;
		memset(mac_control->fifos[i].list_info, 0, list_holder_size);
	}

	for (i = 0; i < config->tx_fifo_num; i++) {
		int page_num = TXD_MEM_PAGE_CNT(config->tx_cfg[i].fifo_len,
						lst_per_page);
		mac_control->fifos[i].tx_curr_put_info.offset = 0;
		mac_control->fifos[i].tx_curr_put_info.fifo_len =
		    config->tx_cfg[i].fifo_len - 1;
		mac_control->fifos[i].tx_curr_get_info.offset = 0;
		mac_control->fifos[i].tx_curr_get_info.fifo_len =
		    config->tx_cfg[i].fifo_len - 1;
		mac_control->fifos[i].fifo_no = i;
		mac_control->fifos[i].nic = nic;
		mac_control->fifos[i].max_txds = MAX_SKB_FRAGS + 1;
	#ifdef NETIF_F_UFO
		mac_control->fifos[i].max_txds++;
	#endif

		for (j = 0; j < page_num; j++) {
			int k = 0;
			dma_addr_t tmp_p;
			void *tmp_v;
			tmp_v = pci_alloc_consistent(nic->pdev,
						     PAGE_SIZE, &tmp_p);
			if (!tmp_v) {
				DBG_PRINT(INFO_DBG, "pci_alloc_consistent ");
				DBG_PRINT(INFO_DBG, "failed for TxDL\n");
				return -ENOMEM;
			}
			mem_allocated += PAGE_SIZE;
			/* If we got a zero DMA address(can happen on
			 * certain platforms like PPC), reallocate.
			 * Free the virtual address of page we don't want.
			 */
			if (!tmp_p) {
				pci_free_consistent(nic->pdev, PAGE_SIZE,
					tmp_v, (dma_addr_t)0);

				tmp_v = pci_alloc_consistent(nic->pdev,
						     PAGE_SIZE, &tmp_p);
				if (!tmp_v) {
					DBG_PRINT(INFO_DBG,
						"pci_alloc_consistent ");
					DBG_PRINT(INFO_DBG,
						"failed for TxDL\n");
					return -ENOMEM;
				}
				mem_allocated += PAGE_SIZE;
			}
			while (k < lst_per_page) {
				int l = (j * lst_per_page) + k;
				if (l == config->tx_cfg[i].fifo_len)
					break;
				list_info =
					&mac_control->fifos[i].list_info[l];
				list_info->list_virt_addr =
					tmp_v + (k * lst_size);
				list_info->list_phy_addr =
					tmp_p + (k * lst_size);
				k++;
			}
		}
	}
	#ifdef NETIF_F_UFO
	for (i = 0; i < config->tx_fifo_num; i++) {
		lst_size = config->tx_cfg[i].fifo_len;
		mac_control->fifos[i].ufo_in_band_v
			= kcalloc(lst_size, sizeof(u64), GFP_KERNEL);
		if (!mac_control->fifos[i].ufo_in_band_v)
			return -ENOMEM;
		mem_allocated += (lst_size * sizeof(u64));
	}
	#endif

	return mem_allocated;
}

static int alloc_init_ring_mem(struct s2io_nic *nic)
{
	u32 size;
	void *tmp_v_addr, *tmp_v_addr_next;
	dma_addr_t tmp_p_addr, tmp_p_addr_next;
	struct RxD_block *pre_rxd_blk = NULL;
	int i, j, blk_cnt, rx_sz;
	unsigned long tmp;
	struct buffAdd *ba;
	struct mac_info *mac_control;
	struct config_param *config;
	unsigned long long mem_allocated = 0;
	int total_rxds = 0;

	mac_control = &nic->mac_control;
	config = &nic->config;

	/* Allocation and initialization of RXDs in Rings */
	size = 0;
#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
		size += config->rx_cfg[i].num_rxd;
		mac_control->rings[i].rx_blocks =
			kmalloc(rx_ring_sz[i] * sizeof(struct rx_block_info),
				GFP_KERNEL);
		if (!mac_control->rings[i].rx_blocks)
			return -ENOMEM;
		memset(mac_control->rings[i].rx_blocks, 0,
			sizeof(struct rx_block_info) * rx_ring_sz[i]);
		mem_allocated += (rx_ring_sz[i] *
			sizeof(struct rx_block_info));
		mac_control->rings[i].block_count =
			config->rx_cfg[i].num_rxd /
			(rxd_count[nic->rxd_mode] + 1 );
		mac_control->rings[i].pkt_cnt = config->rx_cfg[i].num_rxd -
			mac_control->rings[i].block_count;
	}

	if (nic->rxd_mode == RXD_MODE_1)
		size = (size * (sizeof(struct RxD1)));
	else if (nic->rxd_mode == RXD_MODE_5)
		size = (size * (sizeof(struct RxD5)));
	else
		size = (size * (sizeof(struct RxD3)));
	rx_sz = size;

#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
		mac_control->rings[i].rx_curr_get_info.block_index = 0;
		mac_control->rings[i].rx_curr_get_info.offset = 0;
		mac_control->rings[i].rx_curr_put_info.block_index = 0;
		mac_control->rings[i].rx_curr_put_info.offset = 0;
		mac_control->rings[i].nic = nic;
		mac_control->rings[i].ring_no = i;
		mac_control->rings[i].jiffies = jiffies;
		mac_control->rings[i].interrupt_count = 0;
		mac_control->rings[i].ufc_a = MIN_RX_UFC_A;
		mac_control->rings[i].aggr_ack = 0;
		mac_control->rings[i].max_pkts_aggr = 0;
		mac_control->rings[i].lro = lro_enable;
		for (j = 0; j < MAX_LRO_SESSIONS; j++) {
			struct lro *l_lro = &mac_control->rings[i].lro0_n[j];
			l_lro->in_use = 0;
			l_lro->saw_ts = 0;
			l_lro->last_frag = NULL;
		}
		if ((config->intr_type == MSI_X) &&
			config->napi && mac_control->rings[i].lro)
			mac_control->rings[i].aggr_ack = 1;

		total_rxds = config->rx_cfg[i].num_rxd;
		/* Allocate memory to hold the skb addresses */
		if (nic->rxd_mode == RXD_MODE_5) {
			mac_control->rings[i].skbs =
				kmalloc(total_rxds * sizeof(u64), GFP_KERNEL);
			if (!mac_control->rings[i].skbs)
				return -ENOMEM;
			memset(mac_control->rings[i].skbs, 0,
				total_rxds * sizeof(u64));
			mem_allocated += (total_rxds * sizeof(u64));
		}

		blk_cnt = total_rxds/(rxd_count[nic->rxd_mode] + 1);

		/*  Allocating all the Rx blocks */
		for (j = 0; j < blk_cnt; j++) {
			struct rx_block_info *rx_blocks;
			int l;

			rx_blocks = &mac_control->rings[i].rx_blocks[j];
			size = SIZE_OF_BLOCK; //size is always page size
			tmp_v_addr = pci_alloc_consistent(nic->pdev, size,
							  &tmp_p_addr);
			if (tmp_v_addr == NULL) {
				/*
				 * In case of failure, free_shared_mem()
				 * is called, which should free any
				 * memory that was alloced till the
				 * failure happened.
				 */
				rx_blocks->block_virt_addr = tmp_v_addr;
				return -ENOMEM;
			}
			mem_allocated += size;
			memset(tmp_v_addr, 0, size);
			rx_blocks->block_virt_addr = tmp_v_addr;
			rx_blocks->block_dma_addr = tmp_p_addr;
			rx_blocks->rxds = kmalloc(sizeof(struct rxd_info)*
						  rxd_count[nic->rxd_mode],
						  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allocated += (sizeof(struct rxd_info) *
				rxd_count[nic->rxd_mode]);
			for (l=0; l<rxd_count[nic->rxd_mode]; l++) {
				rx_blocks->rxds[l].virt_addr =
					rx_blocks->block_virt_addr +
					(rxd_size[nic->rxd_mode] * l);
				rx_blocks->rxds[l].dma_addr =
					rx_blocks->block_dma_addr +
					(rxd_size[nic->rxd_mode] * l);
			}
		}
		/* Interlinking all Rx Blocks */
		for (j = 0; j < blk_cnt; j++) {
			struct rx_block_info *rx_blocks;
			rx_blocks = &mac_control->rings[i].rx_blocks[j];
			tmp_v_addr = rx_blocks->block_virt_addr;
			tmp_v_addr_next =
				mac_control->rings[i].rx_blocks[(j + 1) %
					blk_cnt].block_virt_addr;
			tmp_p_addr = rx_blocks->block_dma_addr;
			tmp_p_addr_next =
				mac_control->rings[i].rx_blocks[(j + 1) %
					blk_cnt].block_dma_addr;

			pre_rxd_blk = (struct RxD_block *) tmp_v_addr;
			pre_rxd_blk->reserved_2_pNext_RxD_block =
				(unsigned long) tmp_v_addr_next;
			pre_rxd_blk->pNext_RxD_Blk_physical =
				(u64) tmp_p_addr_next;
		}
	}

	if (nic->rxd_mode >= RXD_MODE_3B) {
		/*
		 * Allocation of Storages for buffer addresses in 2BUFF mode
		 * and the buffers as well.
		 */
		int hdrs_per_cache = L1_CACHE_BYTES / BUF0_LEN;
		int hdr_index = 0;
		void *prev_ba_0 = NULL;
#if defined (__VMKLNX__)
		for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
		for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
			blk_cnt = config->rx_cfg[i].num_rxd /
			   (rxd_count[nic->rxd_mode]+ 1);
			mac_control->rings[i].ba =
				kmalloc((sizeof(struct buffAdd *) * blk_cnt),
					GFP_KERNEL);
			if (!mac_control->rings[i].ba)
				return -ENOMEM;
			mem_allocated += (sizeof(struct buffAdd *) * blk_cnt);
			for (j = 0; j < blk_cnt; j++) {
				int k = 0;
				mac_control->rings[i].ba[j] =
					kmalloc((sizeof(struct buffAdd) *
						(rxd_count[nic->rxd_mode] + 1)),
						GFP_KERNEL);
				if (!mac_control->rings[i].ba[j])
					return -ENOMEM;

				mem_allocated += (sizeof(struct buffAdd) *
						(rxd_count[nic->rxd_mode] + 1));
				while (k != rxd_count[nic->rxd_mode]) {
					ba = &mac_control->rings[i].ba[j][k];
					if (L1_CACHE_BYTES > BUF0_LEN) {
						if (hdr_index ==
							hdrs_per_cache) {
							prev_ba_0 = NULL;
							hdr_index = 0;
						}
						ba->ba_0_org = NULL;
						if (hdr_index == 0) {
							ba->ba_0_org =
							  (void *) kmalloc(2 *
								L1_CACHE_BYTES,
								GFP_KERNEL);
							if (!ba->ba_0_org)
								return -ENOMEM;
							mem_allocated += (2 *
							  L1_CACHE_BYTES);
							ba->ba_0 = (void *)
#if defined (CONFIG_64BIT) || defined (__VMKLNX__)
							 L1_CACHE_ALIGN((u64) \
								ba->ba_0_org);
#else  /* !defined (CONFIG_64BIT) &&  !defined (__VMKLNX__) */
							 L1_CACHE_ALIGN((u32) \
								ba->ba_0_org);
#endif /* defined (CONFIG_64BIT) || defined (__VMKLNX__) */
						} else
							ba->ba_0 = prev_ba_0 +
								BUF0_LEN;
						prev_ba_0 = ba->ba_0;
						hdr_index++;
					} else {
						ba->ba_0_org = (void *) kmalloc
							(BUF0_LEN + ALIGN_SIZE,
								GFP_KERNEL);
						if (!ba->ba_0_org)
							return -ENOMEM;
						mem_allocated += (BUF0_LEN +
							ALIGN_SIZE);
						tmp =
						   (unsigned long)ba->ba_0_org;
						tmp += ALIGN_SIZE;
						tmp &=
						 ~((unsigned long) ALIGN_SIZE);
						ba->ba_0 = (void *) tmp;
					}

					ba->ba_1_org = (void *) kmalloc
					  (BUF1_LEN + ALIGN_SIZE, GFP_KERNEL);
					if (!ba->ba_1_org)
						return -ENOMEM;
					mem_allocated +=
						(BUF1_LEN + ALIGN_SIZE);
					tmp = (unsigned long) ba->ba_1_org;
					tmp += ALIGN_SIZE;
					tmp &= ~((unsigned long) ALIGN_SIZE);
					ba->ba_1 = (void *) tmp;
					k++;
				}
			}
		}
	}
	return mem_allocated;
}


/**
 * init_shared_mem - Allocation and Initialization of Memory
 * @nic: Device private variable.
 * Description: The function allocates all the memory areas shared
 * between the NIC and the driver. This includes Tx descriptors,
 * Rx descriptors and the statistics block.
 */

static int init_shared_mem(struct s2io_nic *nic)
{
	int size;
	void *tmp_v_addr = NULL;
	int i;
	struct net_device *dev = nic->dev;
	struct mac_info *mac_control;
	struct config_param *config;
	unsigned long long mem_allocated = 0;

	mac_control = &nic->mac_control;
	config = &nic->config;


	/* Allocation and initialization of TXDLs in FIOFs */
	size = 0;
	for (i = 0; i < config->tx_fifo_num; i++)
		size += config->tx_cfg[i].fifo_len;

	if (size > MAX_AVAILABLE_TXDS) {
		DBG_PRINT(ERR_DBG, "s2io: Requested TxDs too high, ");
		DBG_PRINT(ERR_DBG, "Requested: %d, max supported: 8192\n",
			size);
		return -EINVAL;
	}

	for (i = 0; i < config->tx_fifo_num; i++) {
		size = config->tx_cfg[i].fifo_len;
		/*
		 * Legal values are from 2 to 8192
		 */
		if (size < 2) {
			DBG_PRINT(ERR_DBG, "s2io: Invalid fifo len (%d) ",
				size);
			DBG_PRINT(ERR_DBG, "for fifo %d\n", i);
			DBG_PRINT(ERR_DBG, "s2io: Legal values for fifo len "
				"are 2 to 8192\n");
			return -EINVAL;
		}
	}

#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
		if (config->rx_cfg[i].num_rxd %
			(rxd_count[nic->rxd_mode] + 1)) {
			DBG_PRINT(ERR_DBG, "%s: RxD count of ", dev->name);
			DBG_PRINT(ERR_DBG, "Ring%d is not a multiple of ", i);
			DBG_PRINT(ERR_DBG, "RxDs per Block");
			return -EINVAL;
		}
	}

	size = alloc_init_fifo_mem(nic);
	if (size == -ENOMEM)
		return size;

	mem_allocated += size;

	size = alloc_init_ring_mem(nic);
	if (size == -ENOMEM)
		return size;

	mem_allocated += size;

	/* Allocation and initialization of Statistics block */
	size = sizeof(struct stat_block);
	mac_control->stats_mem = pci_alloc_consistent
	    (nic->pdev, size, &mac_control->stats_mem_phy);

	if (!mac_control->stats_mem) {
		/*
		 * In case of failure, free_shared_mem() is called, which
		 * should free any memory that was alloced till the
		 * failure happened.
		 */
		return -ENOMEM;
	}
	mem_allocated += size;
	mac_control->stats_mem_sz = size;

	tmp_v_addr = mac_control->stats_mem;
	mac_control->stats_info = (struct stat_block *) tmp_v_addr;
	memset(tmp_v_addr, 0, size);
	if (alloc_ethtool_sw_stat(nic) != SUCCESS)
		return -ENOMEM;

	nic->sw_dbg_stat->mem_allocated += mem_allocated;
	return SUCCESS;
}

/**
 * free_shared_mem - Free the allocated Memory
 * @nic:  Device private variable.
 * Description: This function is to free all memory locations allocated by
 * the init_shared_mem() function and return it to the kernel.
 */

static void free_shared_mem(struct s2io_nic *nic)
{
	int i, j, blk_cnt, size;
	void *tmp_v_addr;
	dma_addr_t tmp_p_addr;
	struct mac_info *mac_control;
	struct config_param *config;
	int lst_size, lst_per_page;
	int page_num = 0;

	if (!nic)
		return;

	mac_control = &nic->mac_control;
	config = &nic->config;

	lst_size = (sizeof(struct TxD) * config->max_txds);
	lst_per_page = PAGE_SIZE / lst_size;

	for (i = 0; i < config->tx_fifo_num; i++) {
		page_num = TXD_MEM_PAGE_CNT(config->tx_cfg[i].fifo_len,
						lst_per_page);
		for (j = 0; j < page_num; j++) {
			int mem_blks = (j * lst_per_page);
			if (!mac_control->fifos[i].list_info)
				return;
			if (!mac_control->fifos[i].list_info[mem_blks].
				 list_virt_addr)
				break;
			pci_free_consistent(nic->pdev, PAGE_SIZE,
					    mac_control->fifos[i].
					    list_info[mem_blks].
					    list_virt_addr,
					    mac_control->fifos[i].
					    list_info[mem_blks].
					    list_phy_addr);
		}

		kfree(mac_control->fifos[i].list_info);
	}

	size = SIZE_OF_BLOCK;
#if defined (__VMKLNX__)
	for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
	for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
		if (nic->rxd_mode == RXD_MODE_5) {
			if (mac_control->rings[i].skbs)
				kfree(mac_control->rings[i].skbs);
		}
		if (!mac_control->rings[i].rx_blocks)
			break;
		blk_cnt = mac_control->rings[i].block_count;
		for (j = 0; j < blk_cnt; j++) {
			tmp_v_addr = mac_control->rings[i].rx_blocks[j].
				block_virt_addr;
			tmp_p_addr = mac_control->rings[i].rx_blocks[j].
				block_dma_addr;
			if (tmp_v_addr == NULL)
				break;
			pci_free_consistent(nic->pdev, size,
					    tmp_v_addr, tmp_p_addr);
			if (NULL == mac_control->rings[i].rx_blocks[j].rxds)
				break;
			kfree(mac_control->rings[i].rx_blocks[j].rxds);
		}
		kfree(mac_control->rings[i].rx_blocks);
	}

	if (nic->rxd_mode >= RXD_MODE_3B) {
		/* Freeing buffer storage addresses in 2BUFF mode. */
#if defined (__VMKLNX__)
		for (i = 0; i < MAX_RX_RINGS; i++) {
#else  /* !defined (__VMKLNX__) */
		for (i = 0; i < config->rx_ring_num; i++) {
#endif /* defined (__VMKLNX__) */
			blk_cnt = config->rx_cfg[i].num_rxd /
			    (rxd_count[nic->rxd_mode] + 1);
			if (!mac_control->rings[i].ba)
				break;
			for (j = 0; j < blk_cnt; j++) {
				int k = 0;
				if (!mac_control->rings[i].ba[j])
					continue;
				while (k != rxd_count[nic->rxd_mode]) {
					struct buffAdd *ba =
					  &mac_control->rings[i].ba[j][k];
					if (NULL == ba)
						break;
					if (ba->ba_0_org)
						kfree(ba->ba_0_org);
					if (ba->ba_1_org)
						kfree(ba->ba_1_org);
					k++;
				}
				kfree(mac_control->rings[i].ba[j]);
			}
			kfree(mac_control->rings[i].ba);
		}
	}

	if (mac_control->stats_mem) {
		pci_free_consistent(nic->pdev,
				    mac_control->stats_mem_sz,
				    mac_control->stats_mem,
				    mac_control->stats_mem_phy);
	}

#ifdef NETIF_F_UFO
	for (i = 0; i < config->tx_fifo_num; i++) {
		if (mac_control->fifos[i].ufo_in_band_v)
			kfree(mac_control->fifos[i].ufo_in_band_v);
	}
#endif
	free_ethtool_sw_stat(nic);
}

#ifdef CONFIG_PM
/**
 * s2io_pm_suspend - s2io power management suspend entry point
 *
 */
static int s2io_pm_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0;
	struct net_device *dev =  pci_get_drvdata(pdev);
	struct s2io_nic *nic = dev->priv;

	if (netif_running(dev)) {
		s2io_card_down(nic);
		netif_device_detach(dev);
	}
	if (nic->device_type == XFRAME_II_DEVICE) {
		ret = pci_set_power_state(pdev, pci_choose_state(pdev, state));
		if (ret)
			DBG_PRINT(ERR_DBG, "%s: Error %d setting power state\n",
				nic->dev->name, ret);
	}
	pci_disable_device(pdev);
	return ret;
}
/**
 * s2io_pm_resume - s2io power management resume entry point
 *
 */
static int s2io_pm_resume(struct pci_dev *pdev)
{
	int ret = 0;
	struct net_device *dev =  pci_get_drvdata(pdev);
	struct s2io_nic *nic = dev->priv;

	if (nic->device_type == XFRAME_II_DEVICE) {
		ret = pci_set_power_state(pdev, PCI_D0);
		if (ret) {
			DBG_PRINT(ERR_DBG, "%s: Error %d setting power state\n",
					nic->dev->name, ret);
			return ret;
		}
	}
	ret = pci_enable_device(pdev);
	pci_set_master(pdev);
	pci_restore_state(pdev, nic->config_space);
#ifdef CONFIG_PCI_MSI
	/* Restore the MSIX table entries from local variables */
	restore_xmsi_data(nic);
#endif

	if (netif_running(dev)) {
		ret = s2io_card_up(nic);
		if (ret)
			DBG_PRINT(ERR_DBG, "%s: H/W initialization failed\n",
				dev->name);
	}
	netif_device_attach(dev);
	s2io_start_all_tx_queue(nic);

	return ret;
}

#endif

/**
 * s2io_verify_pci_mode -
 */

static int s2io_verify_pci_mode(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int     mode;

	val64 = readq(&bar0->pci_mode);
	mode = (u8)GET_PCI_MODE(val64);

	if ( val64 & PCI_MODE_UNKNOWN_MODE)
		return -1;      /* Unknown PCI mode */
	return mode;
}

#define NEC_VENID   0x1033
#define NEC_DEVID   0x0125
static int s2io_on_nec_bridge(struct pci_dev *s2io_pdev)
{
	struct pci_dev *tdev = NULL;
	while ((tdev = S2IO_PCI_FIND_DEVICE(PCI_ANY_ID,
		PCI_ANY_ID, tdev)) != NULL) {
		if ((tdev->vendor == NEC_VENID) &&
			(tdev->device == NEC_DEVID)) {
			if (tdev->bus == s2io_pdev->bus->parent) {
				S2IO_PCI_PUT_DEVICE(tdev);
				return 1;
			}
		}
	}
	return 0;
}

static int bus_speed[8] = {33, 66, 66, 100, 133, 133, 200, 266};
/**
 * s2io_print_pci_mode -
 */
static int s2io_print_pci_mode(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int	mode;
	struct config_param *config = &nic->config;

	val64 = readq(&bar0->pci_mode);
	mode = (u8)GET_PCI_MODE(val64);

	if (val64 & PCI_MODE_UNKNOWN_MODE)
		return -1;	/* Unknown PCI mode */

	config->bus_speed = bus_speed[mode];

	if (s2io_on_nec_bridge(nic->pdev)) {
		DBG_PRINT(ERR_DBG, "%s: Device is on PCI-E bus\n",
							nic->dev->name);
		return mode;
	}

	if (val64 & PCI_MODE_32_BITS) {
		DBG_PRINT(ERR_DBG, "%s: Device is on 32 bit ", nic->dev->name);
	} else {
		DBG_PRINT(ERR_DBG, "%s: Device is on 64 bit ", nic->dev->name);
	}

	switch (mode) {
		case PCI_MODE_PCI_33:
			DBG_PRINT(ERR_DBG, "33MHz PCI bus\n");
			break;
		case PCI_MODE_PCI_66:
			DBG_PRINT(ERR_DBG, "66MHz PCI bus\n");
			break;
		case PCI_MODE_PCIX_M1_66:
			DBG_PRINT(ERR_DBG, "66MHz PCIX(M1) bus\n");
			break;
		case PCI_MODE_PCIX_M1_100:
			DBG_PRINT(ERR_DBG, "100MHz PCIX(M1) bus\n");
			break;
		case PCI_MODE_PCIX_M1_133:
			DBG_PRINT(ERR_DBG, "133MHz PCIX(M1) bus\n");
			break;
		case PCI_MODE_PCIX_M2_66:
			DBG_PRINT(ERR_DBG, "133MHz PCIX(M2) bus\n");
			break;
		case PCI_MODE_PCIX_M2_100:
			DBG_PRINT(ERR_DBG, "200MHz PCIX(M2) bus\n");
			break;
		case PCI_MODE_PCIX_M2_133:
			DBG_PRINT(ERR_DBG, "266MHz PCIX(M2) bus\n");
			break;
		default:
			return -1;	/* Unsupported bus speed */
	}

	return mode;
}

/**
 *	rts_ds_steer - Receive traffic steering based on IPv4 or IPv6 TOS
 *	or Traffic class respectively.
 *	@nic: device private variable
 *	Description: The function configures the receive steering to
 *	desired receive ring.
 *	Return Value:  SUCCESS on success and
 *	'-1' on failure (endian settings incorrect).
 */
static int rts_ds_steer(struct s2io_nic *nic, u8 ds_codepoint, u8 ring)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;

	if (ds_codepoint > 63)
		return FAILURE;

	val64 = S2BIT(ring);

	writeq(val64, &bar0->rts_ds_mem_data);

	val64 = RTS_DS_MEM_CTRL_WE |
			RTS_DS_MEM_CTRL_STROBE_NEW_CMD|
			RTS_DS_MEM_CTRL_OFFSET(ds_codepoint);

	writeq(val64, &bar0->rts_ds_mem_ctrl);

	return wait_for_cmd_complete(&bar0->rts_ds_mem_ctrl,
				RTS_DS_MEM_CTRL_STROBE_CMD_BEING_EXECUTED,
				S2IO_BIT_RESET);
}

/**
 *  init_rti - Initialization receive traffic interrupt scheme
 *  @nic: device private variable
 *  Description: The function configures receive traffic interrupts
 *  Return Value:  SUCCESS on success and
 *  '-1' on failure
 */

static int init_rti(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int i, timer_interval = 0;
	struct config_param *config;
	struct mac_info *mac_control;
#if defined (__VMKLNX__)
	unsigned long flags;
	int ret = SUCCESS;
#endif
	int udp_ring = 0;

	config = &nic->config;
	mac_control = &nic->mac_control;

#if defined (__VMKLNX__)
	spin_lock_irqsave(&nic->rxtx_ti_lock, flags);
#endif

	if (nic->device_type == XFRAME_II_DEVICE) {
		/*
		 * Programmed to generate Apprx 250 Intrs per
		 * second
		 */
		timer_interval = (nic->config.bus_speed * 125)/4;
	} else
		timer_interval = 0xFFF;

	/* RTI Initialization */
	for (i = 0; i < config->rx_ring_num; i++) {
		val64 = RTI_DATA1_MEM_RX_TIMER_VAL(timer_interval);
		mac_control->rings[i].rx_timer_val = timer_interval;
		mac_control->rings[i].rx_timer_val_saved = timer_interval;

		if (nic->device_type == XFRAME_I_DEVICE)
			udp_ring = 0; /* Set TCP settings for XENA rings */
		else if (rx_steering_type == NO_STEERING)
			udp_ring = 0; /* Set TCP settings as default */
		else if (rx_steering_type == RX_TOS_STEERING)
			udp_ring = 0; /* Set TCP settings as default */
		else if (rth_protocol == 2)
			udp_ring = 1; /* Set UDP settings all rings */
		else if ((rth_protocol == 1) && rx_ring_num)
			udp_ring = 0; /* Set TCP settings all rings */
		else if ((config->rx_ring_num > 1) && (0 == i))
			udp_ring = 1; /* Set UDP settings for ring 0 */
		else
			udp_ring = 0; /* Set TCP settings for other rings */

		if (udp_ring) {
			val64 |= RTI_DATA1_MEM_RX_URNG_A(1);
			mac_control->rings[i].urange_a = 1;
		} else {
			val64 |= RTI_DATA1_MEM_RX_URNG_A(0xA);
			mac_control->rings[i].urange_a = 0xA;
		}

		val64 |= RTI_DATA1_MEM_RX_URNG_B(0x10) |
			RTI_DATA1_MEM_RX_URNG_C(0x30) |
			RTI_DATA1_MEM_RX_TIMER_AC_EN;

		mac_control->rings[i].urange_b = 0x10;
		mac_control->rings[i].urange_c = 0x30;
		writeq(val64, &bar0->rti_data1_mem);

		if (udp_ring) {
			val64 = RTI_DATA2_MEM_RX_UFC_A(1) |
				RTI_DATA2_MEM_RX_UFC_B(0x40) ;
			mac_control->rings[i].ufc_a = 1;
			mac_control->rings[i].ufc_b = 0x40;
		} else {
			val64 = RTI_DATA2_MEM_RX_UFC_A(1) |
				RTI_DATA2_MEM_RX_UFC_B(2) ;
			mac_control->rings[i].ufc_a = 1;
			mac_control->rings[i].ufc_b = 2;
		}

		if (config->intr_type == MSI_X) {
			if (udp_ring) {
				val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x60) | \
					RTI_DATA2_MEM_RX_UFC_D(0x80));
				mac_control->rings[i].ufc_c = 0x60;
				mac_control->rings[i].ufc_d = 0x80;
			} else {
				val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x20) | \
					RTI_DATA2_MEM_RX_UFC_D(0x40));
				mac_control->rings[i].ufc_c = 0x20;
				mac_control->rings[i].ufc_d = 0x40;
			}
		}
		else {
			val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x40) | \
				RTI_DATA2_MEM_RX_UFC_D(0x80));
			mac_control->rings[i].ufc_c = 0x40;
			mac_control->rings[i].ufc_d = 0x80;
		}
		writeq(val64, &bar0->rti_data2_mem);

		val64 = RTI_CMD_MEM_WE | RTI_CMD_MEM_STROBE_NEW_CMD
				| RTI_CMD_MEM_OFFSET(i);
		writeq(val64, &bar0->rti_command_mem);

		if (wait_for_cmd_complete(&bar0->rti_command_mem,
			RTI_CMD_MEM_STROBE_NEW_CMD, S2IO_BIT_RESET) != SUCCESS)
#if defined (__VMKLNX__)
			ret = FAILURE;
	}

	spin_unlock_irqrestore(&nic->rxtx_ti_lock, flags);
	return ret;
#else /* !defined (__VMKLNX__) */
			return FAILURE;
	}
	return SUCCESS;
#endif /* defined (__VMKLNX__) */

}

/**
 *  init_tti - Initialization transmit traffic interrupt scheme
 *  @nic: device private variable
 *  transmit interrupts
 *  Description: The function configures transmit traffic interrupts
 *  Return Value:  SUCCESS on success and
 *  '-1' on failure
 */
#if defined (__VMKLNX__)
static int init_tti(struct s2io_nic *nic)
#else /* !defined (__VMKLNX__) */
static int init_tti(struct s2io_nic *nic, int link)
#endif /* defined (__VMKLNX__) */
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int i;
	struct config_param *config;
#if defined (__VMKLNX__)
	unsigned long flags;
	int ret = SUCCESS;
#endif

	config = &nic->config;

#if defined (__VMKLNX__)
	spin_lock_irqsave(&nic->rxtx_ti_lock, flags);
#endif

	for (i = 0; i < config->tx_fifo_num; i++) {
		/*
		 * TTI Initialization. Default Tx timer gets us about
		 * 250 interrupts per sec. Continuous interrupts are enabled
		 * by default.
		 */
		if (nic->device_type == XFRAME_II_DEVICE) {
#if defined (__VMKLNX__)
			// PR 423916 - If tx_coalesce_usecs is set then do the requested interrupt
			// rate. If not, do  driver default (link utilization
			// based interrupts + boundary timer of 250 int/sec).
			int count = (nic->tx_coalesce_usecs)?
						(nic->config.bus_speed * nic->tx_coalesce_usecs) >> 6 :
						(nic->config.bus_speed * 125)/2;
#else /* !defined (__VMKLNX__) */
			int count = (nic->config.bus_speed * 125)/2;
#endif /* defined (__VMKLNX__) */
			val64 = TTI_DATA1_MEM_TX_TIMER_VAL(count);
		} else
			val64 = TTI_DATA1_MEM_TX_TIMER_VAL(0x2078);

		/*
		 * We will use different interrupt schemes when there are more
		 * than 5 fifos configured
		*/
		val64 |= TTI_DATA1_MEM_TX_URNG_A(0xA) |
				TTI_DATA1_MEM_TX_URNG_B(0x10) |
				TTI_DATA1_MEM_TX_URNG_C(0x30) |
				TTI_DATA1_MEM_TX_TIMER_AC_EN;

		if (i == 0)
#if !defined (__VMKLNX__)
			if (use_continuous_tx_intrs && (link == LINK_UP))
#else /* defined (__VMKLNX__) */
			if (use_continuous_tx_intrs && (nic->last_link_state == LINK_UP))
#endif /* !defined (__VMKLNX__) */
				val64 |= TTI_DATA1_MEM_TX_TIMER_CI_EN;

		writeq(val64, &bar0->tti_data1_mem);

		if (nic->config.intr_type == MSI_X){
			val64 = TTI_DATA2_MEM_TX_UFC_A(0x10) |
				TTI_DATA2_MEM_TX_UFC_B(0x100) |
				TTI_DATA2_MEM_TX_UFC_C(0x200) |
				TTI_DATA2_MEM_TX_UFC_D(0x300);
		}
		else {
			if ((config->tx_fifo_num > 1) && (i >= nic->udp_fifo_idx) &&
				(i < (nic->udp_fifo_idx + nic->total_udp_fifos)))
				val64 = TTI_DATA2_MEM_TX_UFC_A(0x50) |
					TTI_DATA2_MEM_TX_UFC_B(0x80) |
					TTI_DATA2_MEM_TX_UFC_C(0x100) |
					TTI_DATA2_MEM_TX_UFC_D(0x120);
			else
				val64 = TTI_DATA2_MEM_TX_UFC_A(0x10) |
					TTI_DATA2_MEM_TX_UFC_B(0x20) |
					TTI_DATA2_MEM_TX_UFC_C(0x40) |
					TTI_DATA2_MEM_TX_UFC_D(0x80);
		}
		writeq(val64, &bar0->tti_data2_mem);

		val64 = TTI_CMD_MEM_WE | TTI_CMD_MEM_STROBE_NEW_CMD |
				TTI_CMD_MEM_OFFSET(i);
		writeq(val64, &bar0->tti_command_mem);

		if (wait_for_cmd_complete(&bar0->tti_command_mem,
			TTI_CMD_MEM_STROBE_NEW_CMD, S2IO_BIT_RESET) != SUCCESS)
#if !defined (__VMKLNX__)
			return FAILURE;
	}
	return SUCCESS;
#else /* defined (__VMKLNX__) */
			ret = FAILURE;
		// No need to set for every tx queue (if we are doing fixed ITR) as there is
		// only 1 vector for all the tx queues.
		if (nic->tx_coalesce_usecs)
			break;
	}

	spin_unlock_irqrestore(&nic->rxtx_ti_lock, flags);
	return ret;
#endif /* !defined (__VMKLNX__) */
}

/**
 *  init_nic - Initialization of hardware
 *  @nic: device peivate variable
 *  Description: The function sequentially configures every block
 *  of the H/W from their reset values.
 *  Return Value:  SUCCESS on success and
 *  '-1' on failure (endian settings incorrect).
 */

static int init_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	struct net_device *dev = nic->dev;
	register u64 val64 = 0;
	void __iomem *add;
	int i, j;
	struct mac_info *mac_control;
	struct config_param *config;
	int dtx_cnt = 0;
	unsigned long long mem_share;
	int mem_size;

	mac_control = &nic->mac_control;
	config = &nic->config;

	/* to set the swapper controle on the card */
	if (s2io_set_swapper(nic)) {
		DBG_PRINT(ERR_DBG,"ERROR: Setting Swapper failed\n");
		return -EIO;
	}

	/*
	 * Herc requires EOI to be removed from reset before XGXS, so..
	 */
	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = 0xA500000000ULL;
		writeq(val64, &bar0->sw_reset);
		msleep(500);
		val64 = readq(&bar0->sw_reset);
	}

	/* Remove XGXS from reset state */
	val64 = 0;
	writeq(val64, &bar0->sw_reset);
	msleep(500);
	val64 = readq(&bar0->sw_reset);

	/* Ensure that it's safe to access registers by checking
	 * RIC_RUNNING bit is reset. Check is valid only for XframeII.
	 */
	if (nic->device_type == XFRAME_II_DEVICE) {
		for (i=0; i < 50; i++) {
			val64 = readq(&bar0->adapter_status);
			if (!(val64 & ADAPTER_STATUS_RIC_RUNNING))
				break;
			msleep(10);
		}
		if (i == 50)
			return -ENODEV;
	}

	/*  Enable Receiving broadcasts */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 |= MAC_RMAC_BCAST_ENABLE;
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) val64, add);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), (add + 4));

	/* Read registers in all blocks */
	val64 = readq(&bar0->mac_int_mask);
	val64 = readq(&bar0->mc_int_mask);
	val64 = readq(&bar0->xgxs_int_mask);

	/*  Set MTU */
	val64 = dev->mtu;
	writeq(vBIT(val64, 2, 14), &bar0->rmac_max_pyld_len);

	if (nic->device_type == XFRAME_II_DEVICE) {
		while (herc_act_dtx_cfg[dtx_cnt] != END_SIGN) {
			SPECIAL_REG_WRITE(herc_act_dtx_cfg[dtx_cnt],
					&bar0->dtx_control, UF);
			if (dtx_cnt & 0x1)
				msleep(1); /* Necessary!! */
			dtx_cnt++;
		}
	} else {
		while (xena_dtx_cfg[dtx_cnt] != END_SIGN) {
			SPECIAL_REG_WRITE(xena_dtx_cfg[dtx_cnt],
					  &bar0->dtx_control, UF);
			val64 = readq(&bar0->dtx_control);
			dtx_cnt++;
		}
	}

	/*  Tx DMA Initialization */
	val64 = 0;
	writeq(val64, &bar0->tx_fifo_partition_0);
	writeq(val64, &bar0->tx_fifo_partition_1);
	writeq(val64, &bar0->tx_fifo_partition_2);
	writeq(val64, &bar0->tx_fifo_partition_3);

	for (i = 0, j = 0; i < config->tx_fifo_num; i++) {
		val64 |=
			vBIT(config->tx_cfg[i].fifo_len - 1, ((j * 32) + 19),
				13) | vBIT(config->tx_cfg[i].fifo_priority,
				((j * 32) + 5), 3);

		if (i == (config->tx_fifo_num - 1)) {
			if (i % 2 == 0)
				i++;
		}

		switch (i) {
		case 1:
			writeq(val64, &bar0->tx_fifo_partition_0);
			val64 = 0;
			j = 0;
			break;
		case 3:
			writeq(val64, &bar0->tx_fifo_partition_1);
			val64 = 0;
			j = 0;
			break;
		case 5:
			writeq(val64, &bar0->tx_fifo_partition_2);
			val64 = 0;
			j = 0;
			break;
		case 7:
			writeq(val64, &bar0->tx_fifo_partition_3);
			val64 = 0;
			j = 0;
			break;
		default:
			j++;
			break;
		}
	}

	/*
	 * Disable 4 PCCs for Xena1, 2 and 3 as per H/W bug
	 * SXE-008 TRANSMIT DMA ARBITRATION ISSUE.
	 */
	if ((nic->device_type == XFRAME_I_DEVICE) &&
		(get_xena_rev_id(nic->pdev) < 4))
		writeq(PCC_ENABLE_FOUR, &bar0->pcc_enable);

	val64 = readq(&bar0->tx_fifo_partition_0);
	DBG_PRINT(INIT_DBG, "Fifo partition at: 0x%p is: 0x%llx\n",
		  &bar0->tx_fifo_partition_0, (unsigned long long) val64);

	/*
	 * Initialization of Tx_PA_CONFIG register to ignore packet
	 * integrity checking.
	 */
	val64 = readq(&bar0->tx_pa_cfg);
	val64 |= TX_PA_CFG_IGNORE_FRM_ERR | TX_PA_CFG_IGNORE_SNAP_OUI |
		TX_PA_CFG_IGNORE_LLC_CTRL | TX_PA_CFG_IGNORE_L2_ERR;
	writeq(val64, &bar0->tx_pa_cfg);

	/* Rx DMA intialization. */
	val64 = 0;
	for (i = 0; i < config->rx_ring_num; i++)
		val64 |=
			vBIT(config->rx_cfg[i].ring_priority,
				(5 + (i * 8)), 3);

	writeq(val64, &bar0->rx_queue_priority);

	/*
	 * Allocating equal share of memory to all the
	 * configured Rings.
	 */
	val64 = 0;
	if (nic->device_type == XFRAME_II_DEVICE)
		mem_size = 32;
	else
		mem_size = 64;

	for (i = 0; i < config->rx_ring_num; i++) {
		switch (i) {
		case 0:
			mem_share = (mem_size / config->rx_ring_num +
				mem_size % config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q0_SZ(mem_share);
			continue;
		case 1:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q1_SZ(mem_share);
			continue;
		case 2:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q2_SZ(mem_share);
			continue;
		case 3:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q3_SZ(mem_share);
			continue;
		case 4:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q4_SZ(mem_share);
			continue;
		case 5:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q5_SZ(mem_share);
			continue;
		case 6:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q6_SZ(mem_share);
			continue;
		case 7:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q7_SZ(mem_share);
			continue;
		}
	}
	writeq(val64, &bar0->rx_queue_cfg);

	/*
	 * Filling Tx round robin registers
	 * as per the number of FIFOs
	 */
	switch (config->tx_fifo_num) {
	case 1:
		val64 = 0x0;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 2:
		val64 = 0x0001000100010001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001000100000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 3:
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0200010200010200ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0102000102000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 4:
		val64 = 0x0001020300010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 5:
		val64 = 0x0001020304000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0304000102030400ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0102030400010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0400010203040001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0203040000000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 6:
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0203040500010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0405000102030405ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0203040500000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 7:
		val64 = 0x0001020304050600ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0102030405060001ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0203040506000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0304050600010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0405060000000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 8:
		val64 = 0x0001020304050607ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	}

	/* Enable all configured Tx FIFO partitions */
	val64 = readq(&bar0->tx_fifo_partition_0);
	val64 |= (TX_FIFO_PARTITION_EN);
	writeq(val64, &bar0->tx_fifo_partition_0);

	/* Filling the Rx round robin registers as per the
	 * number of Rings and steering based on QoS.
	*/

	/*
	 * Classic mode: Specify which queues a frame with a particular
	 * 		QoS value may be steered to
	 * Enhanced mode: Maps QoS to a priority level which is
	 * 		then mapped to a queue
	*/

	switch (config->rx_ring_num) {
	case 1:
		val64 = 0x0;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001800180ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0180018001800180ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x00ULL;
		} else
			val64 = 0x8080808080808080ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 2:
		val64 = 0x0001000100010001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001000100000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001800180ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0140014001400140ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000000001010101ULL;
		} else
			val64 = 0x8080808040404040ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 3:
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0200010200010200ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102000102000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001800140ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0140014001200120ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000000101010202ULL;
		} else
			val64 = 0x8080804040402020ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 4:
		val64 = 0x0001020300010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001400140ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0120012001100110ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000010102020303ULL;
		} else
			val64 = 0x8080404020201010ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 5:
		val64 = 0x0001020304000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0304000102030400ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102030400010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0400010203040001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0203040000000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001400140ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0120012001100108ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000010102020304ULL;
		} else
			val64 = 0x8080404020201008ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 6:
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0203040500010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0405000102030405ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0203040500000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001400140ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0120011001080104ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000010102030405ULL;
		} else
			val64 = 0x8080404020100804ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 7:
		val64 = 0x0001020304050600ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0102030405060001ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0203040506000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0304050600010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0405060000000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180018001400120ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0110010801040102ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0000010203040506ULL;
		} else
			val64 = 0x8080402010080402ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	case 8:
		val64 = 0x0001020304050607ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		if (config->rx_steering_type  &&
			config->rx_steering_type != RX_TOS_STEERING) {
			val64 = 0x0180014001200110ULL;
			writeq(val64, &bar0->rts_p0_p3_map);
			val64 = 0x0108010401020101ULL;
			writeq(val64, &bar0->rts_p4_p7_map);
			val64 = 0x0001020304050607ULL;
		} else
			val64 = 0x8040201008040201ULL;
		writeq(val64, &bar0->rts_qos_steering);

		break;
	}

	if (nic->device_type == XFRAME_II_DEVICE
			&& (nic->device_sub_type == XFRAME_E_DEVICE)) {
		fix_rldram(nic);
		fix_rldram_qdr(nic);
	}

	/* UDP Fix */
	val64 = 0;
	for (i = 0; i < 8; i++)
		writeq(val64, &bar0->rts_frm_len_n[i]);

	/* Set the default rts frame length for the rings configured */
	val64 = MAC_RTS_FRM_LEN_SET(dev->mtu+22);
	for (i = 0; i < config->rx_ring_num; i++)
		writeq(val64, &bar0->rts_frm_len_n[i]);

	/* Set the frame length for the configured rings
	 * desired by the user
	 */
	for (i = 0; i < config->rx_ring_num; i++) {
		/* If rts_frm_len[i] == 0 then it is assumed that user not
		 * specified frame length steering.
		 * If the user provides the frame length then program
		 * the rts_frm_len register for those values or else
		 * leave it as it is.
		 */
		if (rts_frm_len[i] != 0)
			writeq(MAC_RTS_FRM_LEN_SET(rts_frm_len[i]),
				&bar0->rts_frm_len_n[i]);
	}

	/*
	 * Disable differentiated services steering logic
	 */
	for (i = 0; i < 64; i++)
		if (rts_ds_steer(nic, i, 0) == FAILURE) {
			DBG_PRINT(ERR_DBG, "%s: failed rts ds steering",
				dev->name);
			DBG_PRINT(ERR_DBG, "set on codepoint %d\n", i);
			return -ENODEV;
		}

	/*
	* Configuring DS steering
	* MAP DS codepoints 0-7 to ring0-last ring, and codepoints 8-63 to last ring
	*/
	if (config->rx_steering_type == RX_TOS_STEERING) {
	    int ring_num = 0;
	    for (i = 0; i < 64; i++) {
		ring_num = (i < 8) ? i : 7;
		if (ring_num >= config->rx_ring_num)
			ring_num = config->rx_ring_num - 1;

			if (rts_ds_steer(nic, i, ring_num) == FAILURE) {
				DBG_PRINT(ERR_DBG, "%s: failed rts ds steering",
					dev->name);
				DBG_PRINT(ERR_DBG, "set on codepoint %d\n", i);
				return -ENODEV;
			}
	    }
	}

	/* Program statistics memory */
	writeq(mac_control->stats_mem_phy, &bar0->stat_addr);

	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = STAT_BC(0x320);
		writeq(val64, &bar0->stat_byte_cnt);
	}

	/*
	 * Initializing the sampling rate for the device to calculate the
	 * bandwidth utilization.
	 */
	val64 = MAC_TX_LINK_UTIL_VAL(tmac_util_period) |
	    MAC_RX_LINK_UTIL_VAL(rmac_util_period);
	writeq(val64, &bar0->mac_link_util);

	/*
	 * Initializing the Transmit and Receive Traffic Interrupt
	 * Scheme.
	 */

	/* Initialize TTI */
#if defined (__VMKLNX__)
	if (SUCCESS != init_tti(nic))
#else /* !defined (__VMKLNX__) */
	if (SUCCESS != init_tti(nic, nic->last_link_state))
#endif /* defined (__VMKLNX__) */
		return -ENODEV;

	/* Initialize RTI */
	if (SUCCESS != init_rti(nic))
		return -ENODEV;

	/* Configure RTH */
	if ((config->rx_steering_type) &&
		(config->rx_steering_type != RX_TOS_STEERING))
		s2io_rth_configure(nic);

	/*
	 * Initializing proper values as Pause threshold into all
	 * the 8 Queues on Rx side.
	 */
	writeq(0xffbbffbbffbbffbbULL, &bar0->mc_pause_thresh_q0q3);
	writeq(0xffbbffbbffbbffbbULL, &bar0->mc_pause_thresh_q4q7);

	/* Disable RMAC PAD STRIPPING */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 &= ~(MAC_CFG_RMAC_STRIP_PAD);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64), add);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), (add + 4));
	val64 = readq(&bar0->mac_cfg);

	/* Enable FCS stripping by adapter */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 |= MAC_CFG_RMAC_STRIP_FCS;
	if (nic->device_type == XFRAME_II_DEVICE)
		writeq(val64, &bar0->mac_cfg);
	else {
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64), add);
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64 >> 32), (add + 4));
	}

	/*
	 * Set the time value to be inserted in the pause frame
	 * generated by xena.
	 */
	val64 = readq(&bar0->rmac_pause_cfg);
	val64 &= ~(RMAC_PAUSE_HG_PTIME(0xffff));
	val64 |= RMAC_PAUSE_HG_PTIME(nic->mac_control.rmac_pause_time);
	writeq(val64, &bar0->rmac_pause_cfg);

	/*
	 * Set the Threshold Limit for Generating the pause frame
	 * If the amount of data in any Queue exceeds ratio of
	 * (mac_control.mc_pause_threshold_q0q3 or q4q7)/256
	 * pause frame is generated
	 */
	val64 = 0;
	for (i = 0; i < 4; i++) {
		val64 |=
		    (((u64) 0xFF00 | nic->mac_control.
		      mc_pause_threshold_q0q3)
		     << (i * 2 * 8));
	}
	writeq(val64, &bar0->mc_pause_thresh_q0q3);

	val64 = 0;
	for (i = 0; i < 4; i++) {
		val64 |=
		    (((u64) 0xFF00 | nic->mac_control.
		      mc_pause_threshold_q4q7)
		     << (i * 2 * 8));
	}
	writeq(val64, &bar0->mc_pause_thresh_q4q7);

	/*
	 * TxDMA will stop Read request if the number of read split has
	 * exceeded the limit pointed by shared_splits
	 */
	val64 = readq(&bar0->pic_control);
	val64 |= PIC_CNTL_SHARED_SPLITS(shared_splits);
	writeq(val64, &bar0->pic_control);

	if (nic->config.bus_speed == 266) {
		writeq(TXREQTO_VAL(0x7f) | TXREQTO_EN, &bar0->txreqtimeout);
		writeq(0x0, &bar0->read_retry_delay);
		writeq(0x0, &bar0->write_retry_delay);
	}

	/*
	 * Programming the Herc to split every write transaction
	 * that does not start on an ADB to reduce disconnects.
	 */
	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = FAULT_BEHAVIOUR | EXT_REQ_EN |
				 MISC_LINK_STABILITY_PRD(3);
		writeq(val64, &bar0->misc_control);
		val64 = readq(&bar0->pic_control2);
		val64 &= ~(S2BIT(13)|S2BIT(14)|S2BIT(15));
		writeq(val64, &bar0->pic_control2);
	}
	if (strstr(nic->product_name, "CX4")) {
		val64 = TMAC_AVG_IPG(0x17);
		writeq(val64, &bar0->tmac_avg_ipg);
	}

	nic->spdm_entry = 0;
	return SUCCESS;
}
#define LINK_UP_DOWN_INTERRUPT		1
#define MAC_RMAC_ERR_TIMER		2

static int s2io_link_fault_indication(struct s2io_nic *nic)
{
	if (nic->device_type == XFRAME_II_DEVICE)
		return LINK_UP_DOWN_INTERRUPT;
	else
		return MAC_RMAC_ERR_TIMER;
}

/**
 *  do_s2io_write_bits -  update alarm bits in alarm register
 *  @value: alarm bits
 *  @flag: interrupt status
 *  @addr: address value
 *  Description: update alarm bits in alarm register
 *  Return Value:
 *  NONE.
 */
static void do_s2io_write_bits(u64 value, int flag, void __iomem *addr)
{
	u64 temp64;

	temp64 = readq(addr);

	if (flag == ENABLE_INTRS)
		temp64 &= ~((u64) value);
	else
		temp64 |= ((u64) value);
	writeq(temp64, addr);
}

static void en_dis_err_alarms(struct s2io_nic *nic, u16 mask, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 gen_int_mask = 0;
#if defined (__VMKLNX__)
	u64 interruptible;
#else  /* !defined (__VMKLNX__) */
	u16 interruptible;
#endif /* defined (__VMKLNX__) */

	writeq(DISABLE_ALL_INTRS, &bar0->general_int_mask);

	if (mask & TX_DMA_INTR) {

		gen_int_mask |= TXDMA_INT_M;

		do_s2io_write_bits(TXDMA_TDA_INT | TXDMA_PFC_INT |
				TXDMA_PCC_INT | TXDMA_TTI_INT |
				TXDMA_LSO_INT | TXDMA_TPA_INT |
				TXDMA_SM_INT, flag, &bar0->txdma_int_mask);

		do_s2io_write_bits(PFC_ECC_DB_ERR | PFC_SM_ERR_ALARM |
				PFC_MISC_0_ERR | PFC_MISC_1_ERR |
				PFC_PCIX_ERR | PFC_ECC_SG_ERR, flag,
				&bar0->pfc_err_mask);

		do_s2io_write_bits(TDA_Fn_ECC_DB_ERR | TDA_SM0_ERR_ALARM |
				TDA_SM1_ERR_ALARM | TDA_Fn_ECC_SG_ERR |
				TDA_PCIX_ERR, flag, &bar0->tda_err_mask);

		do_s2io_write_bits(PCC_FB_ECC_DB_ERR | PCC_TXB_ECC_DB_ERR |
				PCC_SM_ERR_ALARM | PCC_WR_ERR_ALARM |
				PCC_N_SERR | PCC_6_COF_OV_ERR |
				PCC_7_COF_OV_ERR | PCC_6_LSO_OV_ERR |
				PCC_7_LSO_OV_ERR | PCC_FB_ECC_SG_ERR |
				PCC_TXB_ECC_SG_ERR, flag, &bar0->pcc_err_mask);

		do_s2io_write_bits(TTI_SM_ERR_ALARM | TTI_ECC_SG_ERR |
				TTI_ECC_DB_ERR, flag, &bar0->tti_err_mask);

		do_s2io_write_bits(LSO6_ABORT | LSO7_ABORT |
				LSO6_SM_ERR_ALARM | LSO7_SM_ERR_ALARM |
				LSO6_SEND_OFLOW | LSO7_SEND_OFLOW,
				flag, &bar0->lso_err_mask);

		do_s2io_write_bits(TPA_SM_ERR_ALARM | TPA_TX_FRM_DROP,
				flag, &bar0->tpa_err_mask);

		do_s2io_write_bits(SM_SM_ERR_ALARM, flag, &bar0->sm_err_mask);

	}

	if (mask & TX_MAC_INTR) {
		gen_int_mask |= TXMAC_INT_M;
		do_s2io_write_bits(MAC_INT_STATUS_TMAC_INT, flag,
				&bar0->mac_int_mask);
		do_s2io_write_bits(TMAC_TX_BUF_OVRN | TMAC_TX_SM_ERR |
				TMAC_ECC_SG_ERR | TMAC_ECC_DB_ERR |
				TMAC_DESC_ECC_SG_ERR | TMAC_DESC_ECC_DB_ERR,
				flag, &bar0->mac_tmac_err_mask);
	}

	if (mask & TX_XGXS_INTR) {
		gen_int_mask |= TXXGXS_INT_M;
		do_s2io_write_bits(XGXS_INT_STATUS_TXGXS, flag,
				&bar0->xgxs_int_mask);
		do_s2io_write_bits(TXGXS_ESTORE_UFLOW | TXGXS_TX_SM_ERR |
				TXGXS_ECC_SG_ERR | TXGXS_ECC_DB_ERR,
				flag, &bar0->xgxs_txgxs_err_mask);
	}

	if (mask & RX_DMA_INTR) {
		gen_int_mask |= RXDMA_INT_M;
		do_s2io_write_bits(RXDMA_INT_RC_INT_M | RXDMA_INT_RPA_INT_M |
				RXDMA_INT_RDA_INT_M | RXDMA_INT_RTI_INT_M,
				flag, &bar0->rxdma_int_mask);
		do_s2io_write_bits(RC_PRCn_ECC_DB_ERR | RC_FTC_ECC_DB_ERR |
				RC_PRCn_SM_ERR_ALARM | RC_FTC_SM_ERR_ALARM |
				RC_PRCn_ECC_SG_ERR | RC_FTC_ECC_SG_ERR |
				RC_RDA_FAIL_WR_Rn, flag, &bar0->rc_err_mask);
		do_s2io_write_bits(PRC_PCI_AB_RD_Rn | PRC_PCI_AB_WR_Rn |
				PRC_PCI_AB_F_WR_Rn | PRC_PCI_DP_RD_Rn |
				PRC_PCI_DP_WR_Rn | PRC_PCI_DP_F_WR_Rn, flag,
				&bar0->prc_pcix_err_mask);
		do_s2io_write_bits(RPA_SM_ERR_ALARM | RPA_CREDIT_ERR |
				RPA_ECC_SG_ERR | RPA_ECC_DB_ERR, flag,
				&bar0->rpa_err_mask);
		do_s2io_write_bits(RDA_RXDn_ECC_DB_ERR | RDA_FRM_ECC_DB_N_AERR |
				RDA_SM1_ERR_ALARM | RDA_SM0_ERR_ALARM |
				RDA_RXD_ECC_DB_SERR | RDA_RXDn_ECC_SG_ERR |
				RDA_FRM_ECC_SG_ERR | RDA_MISC_ERR|RDA_PCIX_ERR,
				flag, &bar0->rda_err_mask);
		do_s2io_write_bits(RTI_SM_ERR_ALARM |
				RTI_ECC_SG_ERR | RTI_ECC_DB_ERR,
				flag, &bar0->rti_err_mask);
	}

	if (mask & RX_MAC_INTR) {
		gen_int_mask |= RXMAC_INT_M;
		do_s2io_write_bits(MAC_INT_STATUS_RMAC_INT, flag,
				&bar0->mac_int_mask);
		interruptible = RMAC_RX_BUFF_OVRN | RMAC_RX_SM_ERR |
				RMAC_UNUSED_INT | RMAC_SINGLE_ECC_ERR |
				RMAC_DOUBLE_ECC_ERR;

		if (s2io_link_fault_indication(nic) == MAC_RMAC_ERR_TIMER)
			interruptible |= RMAC_LINK_STATE_CHANGE_INT;

		do_s2io_write_bits(interruptible,
				flag, &bar0->mac_rmac_err_mask);
	}

	if (mask & RX_XGXS_INTR) {
		gen_int_mask |= RXXGXS_INT_M;
		do_s2io_write_bits(XGXS_INT_STATUS_RXGXS, flag,
				&bar0->xgxs_int_mask);
		do_s2io_write_bits(RXGXS_ESTORE_OFLOW | RXGXS_RX_SM_ERR, flag,
				&bar0->xgxs_rxgxs_err_mask);
	}

	if (mask & MC_INTR) {
		gen_int_mask |= MC_INT_M;
		do_s2io_write_bits(MC_INT_MASK_MC_INT, flag,
				&bar0->mc_int_mask);
		do_s2io_write_bits(MC_ERR_REG_SM_ERR | MC_ERR_REG_ECC_ALL_SNG |
				MC_ERR_REG_ECC_ALL_DBL | PLL_LOCK_N, flag,
				&bar0->mc_err_mask);
	}
	nic->general_int_mask = gen_int_mask;

	/* Remove this line when alarm interrupts are enabled */
	nic->general_int_mask = 0;
}

/**
 *  en_dis_able_nic_intrs - Enable or Disable the interrupts
 *  @nic: device private variable,
 *  @mask: A mask indicating which Intr block must be modified and,
 *  @flag: A flag indicating whether to enable or disable the Intrs.
 *  Description: This function will either disable or enable the interrupts
 *  depending on the flag argument. The mask argument can be used to
 *  enable/disable any Intr block.
 *  Return Value: NONE.
 */

static void en_dis_able_nic_intrs(struct s2io_nic *nic, u16 mask, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 temp64 = 0, intr_mask = 0;

	intr_mask = nic->general_int_mask;

	/*  Top level interrupt classification */
	/*  PIC Interrupts */
	if (mask & TX_PIC_INTR) {
		/*  Enable PIC Intrs in the general intr mask register */
		intr_mask |= TXPIC_INT_M ;
		if (flag == ENABLE_INTRS) {
			/*
			 * If Hercules adapter enable GPIO otherwise
			 * disable all PCIX, Flash, MDIO, IIC and GPIO
			 * interrupts for now.
			 * TODO
			 */
			if (s2io_link_fault_indication(nic) ==
					LINK_UP_DOWN_INTERRUPT ) {
				do_s2io_write_bits(PIC_INT_GPIO, flag,
						&bar0->pic_int_mask);
				do_s2io_write_bits(GPIO_INT_MASK_LINK_UP, flag,
						&bar0->gpio_int_mask);
			} else
				writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable PIC Intrs in the general
			 * intr mask register
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		}
	}

	/*  Tx traffic interrupts */
	if (mask & TX_TRAFFIC_INTR) {
		intr_mask |= TXTRAFFIC_INT_M;
		if (flag == ENABLE_INTRS) {
			/*
			 * Enable all the Tx side interrupts
			 * writing 0 Enables all 64 TX interrupt levels
			 */
			writeq(0x0, &bar0->tx_traffic_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable Tx Traffic Intrs in the general intr mask
			 * register.
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->tx_traffic_mask);
		}
	}

	/*  Rx traffic interrupts */
	if (mask & RX_TRAFFIC_INTR) {
		intr_mask |= RXTRAFFIC_INT_M;
		if (flag == ENABLE_INTRS) {
			/* writing 0 Enables all 8 RX interrupt levels */
			writeq(0x0, &bar0->rx_traffic_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable Rx Traffic Intrs in the general intr mask
			 * register.
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->rx_traffic_mask);
		}
	}

	temp64 = readq(&bar0->general_int_mask);
	if (flag == ENABLE_INTRS)
		temp64 &= ~((u64) intr_mask);
	else
		temp64 = DISABLE_ALL_INTRS;
	writeq(temp64, &bar0->general_int_mask);

	nic->general_int_mask = readq(&bar0->general_int_mask);
}
/**
 *  verify_pcc_quiescent- Checks for PCC quiescent state
 *  Return: 1 If PCC is quiescence
 *          0 If PCC is not quiescence
 */

static int verify_pcc_quiescent(struct s2io_nic *sp, int flag)
{
	int ret = 0, herc;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = readq(&bar0->adapter_status);

	herc = (sp->device_type == XFRAME_II_DEVICE);

	if (flag == FALSE) {
		if ((!herc && (get_xena_rev_id(sp->pdev) >= 4)) || herc) {
			if (!(val64 & ADAPTER_STATUS_RMAC_PCC_IDLE))
				ret = 1;
		} else {
			if (!(val64 & ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE))
				ret = 1;
		}
	} else {
		if ((!herc && (get_xena_rev_id(sp->pdev) >= 4)) || herc) {
			if (((val64 & ADAPTER_STATUS_RMAC_PCC_IDLE) ==
			     ADAPTER_STATUS_RMAC_PCC_IDLE))
				ret = 1;
		} else {
			if (((val64 & ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE) ==
			     ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE))
				ret = 1;
		}
	}

	return ret;
}

/**
 *  verify_xena_quiescence - Checks whether the H/W is ready
 *  Description: Returns whether the H/W is ready to go or not. Depending
 *  on whether adapter enable bit was written or not the comparison
 *  differs and the calling function passes the input argument flag to
 *  indicate this.
 *  Return: 1 If xena is quiescence
 *          0 If Xena is not quiescence
 */
static int verify_xena_quiescence(struct s2io_nic *sp)
{
	int  mode;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = readq(&bar0->adapter_status);
	mode = s2io_verify_pci_mode(sp);

	if (!(val64 & ADAPTER_STATUS_TDMA_READY)) {
		DBG_PRINT(ERR_DBG, "%s", "TDMA is not ready!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_RDMA_READY)) {
		DBG_PRINT(ERR_DBG, "%s", "RDMA is not ready!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_PFC_READY)) {
		DBG_PRINT(ERR_DBG, "%s", "PFC is not ready!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_TMAC_BUF_EMPTY)) {
		DBG_PRINT(ERR_DBG, "%s", "TMAC BUF is not empty!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_PIC_QUIESCENT)) {
		DBG_PRINT(ERR_DBG, "%s", "PIC is not QUIESCENT!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_MC_DRAM_READY)) {
		DBG_PRINT(ERR_DBG, "%s", "MC_DRAM is not ready!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_MC_QUEUES_READY)) {
		DBG_PRINT(ERR_DBG, "%s", "MC_QUEUES is not ready!");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_M_PLL_LOCK)) {
		DBG_PRINT(ERR_DBG, "%s", "M_PLL is not locked!");
		return 0;
	}
	/*
	* In PCI 33 mode, the P_PLL is not used, and therefore,
	* the the P_PLL_LOCK bit in the adapter_status register will
	* not be asserted.
	*/
	if (!(val64 & ADAPTER_STATUS_P_PLL_LOCK) &&
		(sp->device_type == XFRAME_II_DEVICE) &&
		(mode != PCI_MODE_PCI_33)) {
			DBG_PRINT(ERR_DBG, "%s", "P_PLL is not locked!");
			return 0;
	}
	/* RC_PRC bit is only valid when ADAPTER_EN is not asserted */
	if (!(readq(&bar0->adapter_control) & ADAPTER_CNTL_EN))
		if (!((val64 & ADAPTER_STATUS_RC_PRC_QUIESCENT)
			== ADAPTER_STATUS_RC_PRC_QUIESCENT)) {
			DBG_PRINT(ERR_DBG, "%s", "RC_PRC is not QUIESCENT!");
			return 0;
	}

	return 1;
}

/**
* fix_mac_address -  Fix for Mac addr problem on Alpha platforms
* @sp: Pointer to device specifc structure
* Description :
* New procedure to clear mac address reading  problems on Alpha platforms
*
*/

static void fix_mac_address(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	int i = 0;

	while (fix_mac[i] != END_SIGN) {
		writeq(fix_mac[i++], &bar0->gpio_control);
		udelay(10);
		val64 = readq(&bar0->gpio_control);
	}
}

/*
 * fix_rldram
 * Description:
 * For XFrame II devices, causes the MC test controller
 * to issue 4 reads to all rldram banks which cause the
 * memory to reset.  This works around issues seen with
 * rldram ram.  Once out of test mode, you can not return.
 * Assumes the following register are hardware defaults or
 * are set by flash
 * MC_RLDRAM_GENERATION 0000_0100_0000_0000
 * MC_RLDRAM_MRS_HERC   0003_5700_0301_0300
 *
 */
static void fix_rldram(struct s2io_nic * sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* Put in test mode */
	writeq(0x0000000000010000ULL, &bar0->mc_rldram_test_ctrl);
	/* Disable ODT to fix 0xFF failures */
	writeq(0x0003560003010300ULL, &bar0->mc_rldram_mrs_herc);
	/* Remove out of test mode */
	writeq(0x0000000000000000ULL, &bar0->mc_rldram_test_ctrl);

	return;
}

/*
 * fix_rldram_qdr
 * Description:
 * MC_QDR_CTRL 0x0001_0101_0101_0100 instead change value to 0101_0101_0101_0100
 *
 */
 static void fix_rldram_qdr(struct s2io_nic *sp)
 {
	u64 val64 = 0;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	val64 = readq(&bar0->mc_qdr_ctrl);
	val64 |= S2BIT(7);
	writeq(val64, &bar0->mc_qdr_ctrl);

	/* Read back to flush */
	val64 = readq(&bar0->mc_qdr_ctrl);
	return;
 }

/**
 *  start_nic - Turns the device on
 *  @nic : device private variable.
 *  Description:
 *  This function actually turns the device on. Before this  function is
 *  called,all Registers are configured from their reset states
 *  and shared memory is allocated but the NIC is still quiescent. On
 *  calling this function, the device interrupts are cleared and the NIC is
 *  literally switched on by writing into the adapter control register.
 *  Return Value:
 *  SUCCESS on success and -1 on failure.
 */

static int start_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	struct net_device *dev = nic->dev;
	register u64 val64 = 0;
	u16 subid, i;
	struct mac_info *mac_control;
	struct config_param *config;

	mac_control = &nic->mac_control;
	config = &nic->config;

	/*  PRC Initialization and configuration */
	for (i = 0; i < config->rx_ring_num; i++) {
		writeq((u64) mac_control->rings[i].rx_blocks[0].block_dma_addr,
		       &bar0->prc_rxd0_n[i]);

		val64 = readq(&bar0->prc_ctrl_n[i]);

		if (nic->rxd_mode == RXD_MODE_1)
			val64 |= PRC_CTRL_RC_ENABLED;
		else if (nic->rxd_mode == RXD_MODE_5)
			val64 |= PRC_CTRL_RC_ENABLED | PRC_CTRL_RING_MODE_5 |
						PRC_CTRL_RTH_DISABLE;
		else
			val64 |= PRC_CTRL_RC_ENABLED | PRC_CTRL_RING_MODE_3 |
						PRC_CTRL_RTH_DISABLE;

		if (nic->device_type == XFRAME_II_DEVICE)
			val64 |= PRC_CTRL_GROUP_READS;
		val64 &= ~PRC_CTRL_RXD_BACKOFF_INTERVAL(0xFFFFFF);
		val64 |= PRC_CTRL_RXD_BACKOFF_INTERVAL(0x1000);
		writeq(val64, &bar0->prc_ctrl_n[i]);
	}

	if (nic->rxd_mode == RXD_MODE_3B) {
		/* Enabling 2 buffer mode by writing into Rx_pa_cfg reg. */
		val64 = readq(&bar0->rx_pa_cfg);
		val64 |= RX_PA_CFG_IGNORE_L2_ERR;
		writeq(val64, &bar0->rx_pa_cfg);
	}

	s2io_handle_vlan_tag_strip(nic, nic->vlan_strip_flag);

	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = readq(&bar0->rx_pa_cfg);
		val64 |= RX_PA_CFG_UDP_ZERO_CS_EN;
		writeq(val64, &bar0->rx_pa_cfg);
	}

	/*
	 * Enabling MC-RLDRAM. After enabling the device, we timeout
	 * for around 100ms, which is approximately the time required
	 * for the device to be ready for operation.
	 */
	val64 = readq(&bar0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_QUEUE_SIZE_ENABLE | MC_RLDRAM_MRS_ENABLE;
	SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_mrs, UF);
	val64 = readq(&bar0->mc_rldram_mrs);

	msleep(100);	/* Delay by around 100 ms. */

	/* Enabling ECC Protection. */
	val64 = readq(&bar0->adapter_control);
	val64 &= ~ADAPTER_ECC_EN;
	writeq(val64, &bar0->adapter_control);

	/*
	 * Verify if the device is ready to be enabled, if so enable
	 * it.
	 */
	val64 = readq(&bar0->adapter_status);
	if (!verify_xena_quiescence(nic)) {
		DBG_PRINT(ERR_DBG, "%s: device is not ready, ", dev->name);
		DBG_PRINT(ERR_DBG, "Adapter status reads: 0x%llx\n",
			  (unsigned long long) val64);
		return FAILURE;
	}

	/*
	 * With some switches, link might be already up at this point.
	 * Because of this weird behavior, when we enable laser,
	 * we may not get link. We need to handle this. We cannot
	 * figure out which switch is misbehaving. So we are forced to
	 * make a global change.
	 */

	/* Enabling Laser. */
	val64 = readq(&bar0->adapter_control);
	val64 |= ADAPTER_EOI_TX_ON;
	writeq(val64, &bar0->adapter_control);

	if (s2io_link_fault_indication(nic) == MAC_RMAC_ERR_TIMER) {
		/*
		 * Dont see link state interrupts initally on some switches,
		 * so directly scheduling the link state task here.
		 */
		schedule_work(&nic->set_link_task);
	}
	/* SXE-002: Initialize link and activity LED */
	subid = nic->pdev->subsystem_device;
	if (((subid & 0xFF) >= 0x07) &&
	    (nic->device_type == XFRAME_I_DEVICE)) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->gpio_control);
		val64 = 0x0411040400000000ULL;
		writeq(val64, (void __iomem *)bar0 + 0x2700);
	}

	return SUCCESS;
}
/**
 * s2io_txdl_getskb - Get the skb from txdl, unmap and return skb
 */
static struct sk_buff *s2io_txdl_getskb(struct fifo_info *fifo_data, struct \
TxD *txdlp, int get_off)
{
	struct s2io_nic *nic = fifo_data->nic;
	struct sk_buff *skb;
	struct TxD *txds;
	u16 j, frg_cnt;

	txds = txdlp;
#ifdef NETIF_F_UFO
	if (txds->Host_Control == (u64)(long)fifo_data->ufo_in_band_v) {
		pci_unmap_single(nic->pdev, (dma_addr_t)
			txds->Buffer_Pointer, sizeof(u64),
			PCI_DMA_TODEVICE);
		txds++;
	}
#endif

	skb = (struct sk_buff *) ((unsigned long)
			txds->Host_Control);
	if (!skb) {
		memset(txdlp, 0, (sizeof(struct TxD) * fifo_data->max_txds));
		return NULL;
	}
	pci_unmap_single(nic->pdev, (dma_addr_t)
			 txds->Buffer_Pointer,
			 skb->len - skb->data_len,
			 PCI_DMA_TODEVICE);
	frg_cnt = skb_shinfo(skb)->nr_frags;
	if (frg_cnt) {
		txds++;
		for (j = 0; j < frg_cnt; j++, txds++) {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[j];
			if (!txds->Buffer_Pointer)
				break;
			pci_unmap_page(nic->pdev, (dma_addr_t)
					txds->Buffer_Pointer,
				       frag->size, PCI_DMA_TODEVICE);
		}
	}
	memset(txdlp, 0, (sizeof(struct TxD) * fifo_data->max_txds));
	return(skb);
}

/**
 *  free_tx_buffers - Free all queued Tx buffers
 *  @nic : device private variable.
 *  Description:
 *  Free all queued Tx buffers.
 *  Return Value: void
*/

static void free_tx_buffers(struct s2io_nic *nic)
{
	struct net_device *dev = nic->dev;
	struct sk_buff *skb;
	struct TxD *txdp;
	int i, j;
	struct mac_info *mac_control;
	struct config_param *config;
	int cnt = 0;
	struct swDbgStat *stats = nic->sw_dbg_stat;

	mac_control = &nic->mac_control;
	config = &nic->config;

	for (i = 0; i < config->tx_fifo_num; i++) {
		unsigned long flags;

		spin_lock_irqsave(&mac_control->fifos[i].tx_lock, flags);
		for (j = 0; j < config->tx_cfg[i].fifo_len; j++) {
			txdp = (struct TxD *) mac_control->fifos[i].
				list_info[j].list_virt_addr;
			skb = s2io_txdl_getskb(&mac_control->fifos[i], txdp, j);
			if (skb) {
				stats->mem_freed += skb->truesize;
				dev_kfree_skb(skb);
				cnt++;
			}
		}
		DBG_PRINT(INTR_DBG,
			  "%s:forcibly freeing %d skbs on FIFO%d\n",
			  dev->name, cnt, i);
		mac_control->fifos[i].tx_curr_get_info.offset = 0;
		mac_control->fifos[i].tx_curr_put_info.offset = 0;
		spin_unlock_irqrestore(&mac_control->fifos[i].tx_lock, flags);
	}
}

/**
 *   stop_nic -  To stop the nic
 *   @nic ; device private variable.
 *   Description:
 *   This function does exactly the opposite of what the start_nic()
 *   function does. This function is called to stop the device.
 *   Return Value:
 *   void.
 */

static void stop_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	u16 interruptible;

	/*  Disable all interrupts */
	en_dis_err_alarms(nic, ENA_ALL_INTRS, DISABLE_INTRS);
	interruptible = TX_TRAFFIC_INTR | RX_TRAFFIC_INTR;
	interruptible |= TX_PIC_INTR;
	en_dis_able_nic_intrs(nic, interruptible, DISABLE_INTRS);

	/* Clearing Adapter_En bit of ADAPTER_CONTROL Register */
	val64 = readq(&bar0->adapter_control);
	val64 &= ~(ADAPTER_CNTL_EN);
	writeq(val64, &bar0->adapter_control);
}

static int fill_rxd_5buf(struct s2io_nic *nic, struct RxD5 *rxdp,
struct sk_buff *skb, int ring_no, int rxd_index)
{
	struct sk_buff *frag_list = NULL;
	u64 tmp;
	struct swDbgStat *stats = nic->sw_dbg_stat;

	/* Buffer-1 receives L3/L4 headers */
	((struct RxD5 *)rxdp)->Buffer1_ptr = pci_map_single
			(nic->pdev, skb->data, l3l4hdr_size + 4,
			PCI_DMA_FROMDEVICE);
	if ((((struct RxD5 *)rxdp)->Buffer1_ptr == 0) ||
		(((struct RxD5 *)rxdp)->Buffer1_ptr == DMA_ERROR_CODE))
		return -ENOMEM;;

	/* skb_shinfo(skb)->frag_list will have L4 data payload */
	skb_shinfo(skb)->frag_list = netdev_alloc_skb(nic->dev, MODE5_BUF_SIZE + ALIGN_SIZE);
	if (skb_shinfo(skb)->frag_list == NULL) {
		stats->mem_alloc_fail_cnt++;
		goto unmap_buf1;
	}
	frag_list = skb_shinfo(skb)->frag_list;
	skb->truesize += frag_list->truesize; /* updating skb->truesize */
	stats->mem_allocated += frag_list->truesize;
	frag_list->next = NULL;

	/*Align the RxD on 128 byte boundary */
	tmp = (u64)(unsigned long) frag_list->data;
	tmp += ALIGN_SIZE;
	tmp &= ~ALIGN_SIZE;
	frag_list->data = (void *) (unsigned long)tmp;
	S2IO_SKB_INIT_TAIL(frag_list, tmp);

	rxdp->Buffer2_ptr = pci_map_single(nic->pdev,
				frag_list->data, MODE5_BUF_SIZE,
				PCI_DMA_FROMDEVICE);
	if ((rxdp->Buffer2_ptr == 0) ||
		(rxdp->Buffer2_ptr == DMA_ERROR_CODE))
		goto unmap_buf1;

	rxdp->Control_3	&= (~MASK_BUFFER3_SIZE_5);
	if (nic->skbs_per_rxd > 2) {
		/* Allocate and map buffer3 */
		frag_list->next = netdev_alloc_skb(nic->dev, MODE5_BUF_SIZE + ALIGN_SIZE);
		if (frag_list->next == NULL) {
			stats->mem_alloc_fail_cnt++;
			goto unmap_buf2;
		}

		frag_list = frag_list->next;
		skb->truesize += frag_list->truesize; /*updating skb->truesize*/
		stats->mem_allocated += frag_list->truesize;
		frag_list->next = NULL;

		/*Align the RxD on 128 byte boundary */
		tmp = (u64)(unsigned long) frag_list->data;
		tmp += ALIGN_SIZE;
		tmp &= ~ALIGN_SIZE;
		frag_list->data = (void *) (unsigned long)tmp;
		S2IO_SKB_INIT_TAIL(frag_list, tmp);

		rxdp->Buffer3_ptr = pci_map_single(nic->pdev,
					frag_list->data, MODE5_BUF_SIZE,
					PCI_DMA_FROMDEVICE);
		if ((rxdp->Buffer3_ptr == 0) ||
			(rxdp->Buffer3_ptr == DMA_ERROR_CODE)) {
			frag_list = skb_shinfo(skb)->frag_list;
			goto unmap_buf2;
		}
		rxdp->Control_3	|= SET_BUFFER3_SIZE_5(MODE5_BUF_SIZE);
	} else {
		rxdp->Buffer3_ptr = rxdp->Buffer2_ptr;
		rxdp->Control_3	|= SET_BUFFER3_SIZE_5(1);
	}

	rxdp->Control_3	&= (~MASK_BUFFER4_SIZE_5);
	if (nic->skbs_per_rxd > 3) {
		/* Allocate and map buffer4 */
		frag_list->next = netdev_alloc_skb(nic->dev, MODE5_BUF_SIZE + ALIGN_SIZE);
		if (frag_list->next == NULL) {
			stats->mem_alloc_fail_cnt++;
			frag_list = skb_shinfo(skb)->frag_list;
			goto unmap_buf3;
		}

		frag_list = frag_list->next;
		skb->truesize += frag_list->truesize; /*updating skb->truesize*/
		stats->mem_allocated += frag_list->truesize;
		frag_list->next = NULL;

		/*Align the RxD on 128 byte boundary */
		tmp = (u64)(unsigned long) frag_list->data;
		tmp += ALIGN_SIZE;
		tmp &= ~ALIGN_SIZE;
		frag_list->data = (void *) (unsigned long)tmp;
		S2IO_SKB_INIT_TAIL(frag_list, tmp);

		rxdp->Buffer4_ptr = pci_map_single(nic->pdev,
					frag_list->data, MODE5_BUF_SIZE,
					PCI_DMA_FROMDEVICE);
		if ((rxdp->Buffer4_ptr == 0) ||
			(rxdp->Buffer4_ptr == DMA_ERROR_CODE)) {
			frag_list = skb_shinfo(skb)->frag_list;
			goto unmap_buf3;
		}
		rxdp->Control_3 |= SET_BUFFER4_SIZE_5(MODE5_BUF_SIZE);
	} else {
		rxdp->Buffer4_ptr = rxdp->Buffer3_ptr;
		rxdp->Control_3 |= SET_BUFFER4_SIZE_5(1);
	}

	/* Update the buffer sizes */
	rxdp->Control_2	&= (~MASK_BUFFER0_SIZE_5);
	rxdp->Control_2	|= SET_BUFFER0_SIZE_5(BUF0_LEN);
	rxdp->Control_2	&= (~MASK_BUFFER1_SIZE_5);
	rxdp->Control_2	|= SET_BUFFER1_SIZE_5(l3l4hdr_size + 4);
	rxdp->Control_2	&= (~MASK_BUFFER2_SIZE_5);
	rxdp->Control_2	|= SET_BUFFER2_SIZE_5(MODE5_BUF_SIZE);

	rxdp->Host_Control = rxd_index;
	nic->mac_control.rings[ring_no].skbs[rxd_index] = (unsigned long) (skb);
	return SUCCESS;

unmap_buf3:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(nic->pdev,
			(dma_addr_t)(unsigned long)frag_list->next->data,
			MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
unmap_buf2:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(nic->pdev,
			(dma_addr_t)(unsigned long)frag_list->data,
			MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
unmap_buf1:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(nic->pdev,
			(dma_addr_t)(unsigned long)skb->data, l3l4hdr_size + 4,
			PCI_DMA_FROMDEVICE);
		return -ENOMEM ;
}


/**
 *  fill_rx_buffers - Allocates the Rx side skbs
 *  @nic:  device private variable
 *  @ring_no: ring number
 *  Description:
 *  The function allocates Rx side skbs and puts the physical
 *  address of these buffers into the RxD buffer pointers, so that the NIC
 *  can DMA the received frame into these locations.
 *  The NIC supports 2 receive modes, viz
 *  1. single buffer,
 *  2. Five buffer modes.
 *  Each mode defines how many fragments the received frame will be split
 *  up into by the NIC. The frame is split into L3 header, L4 Header,
 *  L4 payload in three buffer mode and in 5 buffer mode, L4 payload itself
 *  is split into 3 fragments. As of now only single buffer mode is
 *  supported.
 *   Return Value:
 *  SUCCESS on success or an appropriate -ve value on failure.
 */

static int fill_rx_buffers(struct ring_info *ring)
{
	struct net_device *dev = ring->dev;
	struct sk_buff *skb = NULL;
	struct RxD_t *rxdp = NULL;
	int off, size, block_no, block_no1;
	u32 alloc_tab = 0;
	u32 alloc_cnt;
	u64 tmp;
	struct buffAdd *ba = NULL;
	struct RxD_t *first_rxdp = NULL;
	u64 Buffer0_ptr = 0, Buffer1_ptr = 0;
	int	rxd_index = 0;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct RxD5 *rxdp5;
	struct swDbgStat *stats = ring->nic->sw_dbg_stat;

	alloc_cnt = ring->pkt_cnt - ring->rx_bufs_left;

	block_no1 = ring->rx_curr_get_info.block_index;
	while (alloc_tab < alloc_cnt) {
		block_no = ring->rx_curr_put_info.block_index;
		off = ring->rx_curr_put_info.offset;

		rxdp = ring->rx_blocks[block_no].rxds[off].virt_addr;

		rxd_index = off + 1;
		if (block_no)
			rxd_index += (block_no * ring->rxd_count);

		if ((block_no == block_no1) &&
			(off == ring->rx_curr_get_info.offset) &&
			(rxdp->Host_Control)) {
			DBG_PRINT(INTR_DBG, "%s: Get and Put",
				dev->name);
			DBG_PRINT(INTR_DBG, " info equated\n");
			goto end;
		}

		if (off == ring->rxd_count) {
			ring->rx_curr_put_info.block_index++;
			if (ring->rx_curr_put_info.block_index ==
							ring->block_count)
				ring->rx_curr_put_info.block_index = 0;
			block_no = ring->rx_curr_put_info.block_index;
			off = 0;
			ring->rx_curr_put_info.offset = off;
			rxdp = ring->rx_blocks[block_no].block_virt_addr;
			DBG_PRINT(INTR_DBG, "%s: Next block at: %p\n",
				  dev->name, rxdp);
		}

		if ((rxdp->Control_1 & RXD_OWN_XENA) &&
			((ring->rxd_mode == RXD_MODE_3B) &&
				(rxdp->Control_2 & S2BIT(0)))) {
			ring->rx_curr_put_info.offset = off;
			goto end;
		}

		/* calculate size of skb based on ring mode */
		size = dev->mtu + HEADER_ETHERNET_II_802_3_SIZE +
				HEADER_802_2_SIZE + HEADER_SNAP_SIZE;
		if (ring->rxd_mode == RXD_MODE_1)
			size += NET_IP_ALIGN;
		else if (ring->rxd_mode == RXD_MODE_3B)
			size = dev->mtu + ALIGN_SIZE + BUF0_LEN + 4;
		else
			size = l3l4hdr_size + ALIGN_SIZE + BUF0_LEN + 4;

		/* allocate skb */
		skb = netdev_alloc_skb(dev, size);
		if(!skb) {
			DBG_PRINT(INFO_DBG, "%s: Out of ", dev->name);
			DBG_PRINT(INFO_DBG, "memory to allocate SKBs, alloc_cnt %d, alloc_tab %d\n", alloc_cnt, alloc_tab);
			if (first_rxdp) {
				wmb();
				first_rxdp->Control_1 |= RXD_OWN_XENA;
			}
			stats->mem_alloc_fail_cnt++;
			return -ENOMEM ;
		}
		stats->mem_allocated += skb->truesize;
		if (ring->rxd_mode == RXD_MODE_1) {
			/* 1 buffer mode - normal operation mode */
			rxdp1 = (struct RxD1 *)rxdp;
			memset(rxdp, 0, sizeof(struct RxD1));
			skb_reserve(skb, NET_IP_ALIGN);
			rxdp1->Buffer0_ptr = pci_map_single
				(ring->pdev, skb->data, size - NET_IP_ALIGN,
				PCI_DMA_FROMDEVICE);
			if ((rxdp1->Buffer0_ptr == 0) ||
				(rxdp1->Buffer0_ptr == DMA_ERROR_CODE))
				goto pci_map_failed;

			rxdp->Control_2 =
				SET_BUFFER0_SIZE_1(size - NET_IP_ALIGN);
			rxdp->Host_Control = (unsigned long) (skb);
		} else if (ring->rxd_mode >= RXD_MODE_3B) {
			/*
			 * 2 buffer mode -
			 * 2 buffer mode provides 128
			 * byte aligned receive buffers.
			 *
			 */
			ba = &ring->ba[block_no][off];

			rxdp3 = (struct RxD3 *)rxdp;
			rxdp5 = (struct RxD5 *)rxdp;
			/* save the buffer pointers to avoid frequent
			* dma mapping
			*/
			Buffer0_ptr = rxdp3->Buffer0_ptr;
			Buffer1_ptr = rxdp3->Buffer1_ptr;
			if (ring->rxd_mode == RXD_MODE_5)
				memset(rxdp, 0, sizeof(struct RxD5));
			else
				memset(rxdp, 0, sizeof(struct RxD3));
			/* restore the buffer pointers for dma sync*/
			rxdp3->Buffer0_ptr = Buffer0_ptr;
			rxdp3->Buffer1_ptr = Buffer1_ptr;
			skb_reserve(skb, BUF0_LEN);
			tmp = (u64)(unsigned long) skb->data;
			tmp += ALIGN_SIZE;
			tmp &= ~ALIGN_SIZE;
			skb->data = (void *) (unsigned long)tmp;
			S2IO_SKB_INIT_TAIL(skb, tmp);

			if (!(rxdp3->Buffer0_ptr))
				rxdp3->Buffer0_ptr =
					pci_map_single(ring->pdev, ba->ba_0,
						BUF0_LEN, PCI_DMA_FROMDEVICE);
			else
				pci_dma_sync_single_for_device(ring->pdev,
					(dma_addr_t) rxdp3->Buffer0_ptr,
					BUF0_LEN, PCI_DMA_FROMDEVICE);

			if ((rxdp3->Buffer0_ptr == 0) ||
				(rxdp3->Buffer0_ptr == DMA_ERROR_CODE))
				goto pci_map_failed;

			if (ring->rxd_mode == RXD_MODE_5) {
				if (fill_rxd_5buf(ring->nic, rxdp5, skb,
					ring->ring_no, rxd_index) == -ENOMEM) {
					stats->mem_freed += skb->truesize;
					dev_kfree_skb_irq(skb);
					if (first_rxdp) {
						wmb();
						first_rxdp->Control_1 |=
							RXD_OWN_XENA;
					}
					return -ENOMEM ;
				}
			} else {
				rxdp->Control_2 = SET_BUFFER0_SIZE_3(BUF0_LEN);
				if (ring->rxd_mode == RXD_MODE_3B) {
					/* Two buffer mode */

					/*
					*Buffer2 will have L3/L4 header plus
					* L4 payload
					*/

					rxdp3->Buffer2_ptr =
						pci_map_single(ring->pdev,
							skb->data,
							dev->mtu + 4,
							PCI_DMA_FROMDEVICE);

					if ((rxdp3->Buffer2_ptr == 0) ||
						(rxdp3->Buffer2_ptr ==
							DMA_ERROR_CODE))
						goto pci_map_failed;

					/* Buffer-1 will be dummy buffer */
					if (!(rxdp3->Buffer1_ptr))
						rxdp3->Buffer1_ptr =
						    pci_map_single(ring->pdev,
							ba->ba_1, BUF1_LEN,
							PCI_DMA_FROMDEVICE);

					if ((rxdp3->Buffer1_ptr == 0) ||
						(rxdp3->Buffer1_ptr ==
							DMA_ERROR_CODE)) {
						pci_unmap_single(ring->pdev,
						    (dma_addr_t)(unsigned long)
							skb->data,
						    dev->mtu + 4,
						    PCI_DMA_FROMDEVICE);
						goto pci_map_failed;
					}
					rxdp->Control_2 |=
						SET_BUFFER1_SIZE_3(1);
					rxdp->Control_2 |=
					    SET_BUFFER2_SIZE_3(dev->mtu + 4);
				}
				rxdp->Control_2 |= S2BIT(0);
				rxdp->Host_Control = (unsigned long) (skb);
			}
		}

		if (alloc_tab & ((1 << rxsync_frequency) - 1))
			rxdp->Control_1 |= RXD_OWN_XENA;
		off++;
		if (off == (ring->rxd_count + 1))
			off = 0;
		ring->rx_curr_put_info.offset = off;

		rxdp->Control_2 |= SET_RXD_MARKER;
		if (!(alloc_tab & ((1 << rxsync_frequency) - 1))) {
			if (first_rxdp) {
				wmb();
				first_rxdp->Control_1 |= RXD_OWN_XENA;
			}
			first_rxdp = rxdp;
		}
		ring->rx_bufs_left += 1;
		alloc_tab++;
	}

end:
	/* Transfer ownership of first descriptor to adapter just before
	 * exiting. Before that, use memory barrier so that ownership
	 * and other fields are seen by adapter correctly.
	 */
	if (first_rxdp) {
		wmb();
		first_rxdp->Control_1 |= RXD_OWN_XENA;
	}

	return SUCCESS;
pci_map_failed:
	stats->pci_map_fail_cnt++;
	stats->mem_freed += skb->truesize;
	dev_kfree_skb_irq(skb);
	return -ENOMEM;
}

static void free_rxd_blk(struct s2io_nic *sp, int ring_no, int blk)
{
	struct net_device *dev = sp->dev;
	int j;
	struct sk_buff *skb;
	struct RxD_t *rxdp;
	struct mac_info *mac_control;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct RxD5 *rxdp5;
	struct swDbgStat *stats = sp->sw_dbg_stat;

	mac_control = &sp->mac_control;
	for (j = 0 ; j < rxd_count[sp->rxd_mode]; j++) {
		rxdp = mac_control->rings[ring_no].
			rx_blocks[blk].rxds[j].virt_addr;
		if (sp->rxd_mode == RXD_MODE_5) {
			rxdp5 = (struct RxD5 *)rxdp;
			skb = (struct sk_buff *)
				((unsigned long)mac_control->rings[ring_no].
					skbs[((u64) rxdp5->Host_Control)]);
		} else
			skb = (struct sk_buff *)
				((unsigned long) rxdp->Host_Control);

		if (!skb)
			continue;

		switch (sp->rxd_mode) {
		case RXD_MODE_1:
			rxdp1 = (struct RxD1 *)rxdp;
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp1->Buffer0_ptr,
				dev->mtu +
				HEADER_ETHERNET_II_802_3_SIZE +
				HEADER_802_2_SIZE +
				HEADER_SNAP_SIZE,
				PCI_DMA_FROMDEVICE);
				memset(rxdp, 0, sizeof(struct RxD1));
			break;

		case RXD_MODE_5:
			rxdp5 = (struct RxD5 *)rxdp;
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp5->Buffer0_ptr,
				BUF0_LEN,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp5->Buffer1_ptr,
				l3l4hdr_size + 4,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp5->Buffer2_ptr,
				MODE5_BUF_SIZE,
				PCI_DMA_FROMDEVICE);
			if (sp->skbs_per_rxd > 2)
				pci_unmap_single(sp->pdev, (dma_addr_t)
					rxdp5->Buffer3_ptr,
					MODE5_BUF_SIZE,
					PCI_DMA_FROMDEVICE);
			if (sp->skbs_per_rxd > 3)
				pci_unmap_single(sp->pdev, (dma_addr_t)
					rxdp5->Buffer4_ptr,
					MODE5_BUF_SIZE,
					PCI_DMA_FROMDEVICE);
			memset(rxdp, 0, sizeof(struct RxD5));
			break;

		case RXD_MODE_3B:
			rxdp3 = (struct RxD3 *)rxdp;
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp3->Buffer0_ptr,
				BUF0_LEN,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp3->Buffer1_ptr,
				BUF1_LEN,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev, (dma_addr_t)
				rxdp3->Buffer2_ptr,
				dev->mtu + 4,
				PCI_DMA_FROMDEVICE);
			memset(rxdp, 0, sizeof(struct RxD3));
			break;
		}
		stats->mem_freed += skb->truesize;
		dev_kfree_skb(skb);
		mac_control->rings[ring_no].rx_bufs_left -= 1;
	}
}

/**
 *  free_rx_buffers - Frees all Rx buffers
 *  @sp: device private variable.
 *  Description:
 *  This function will free all Rx buffers allocated by host.
 *  Return Value:
 *  NONE.
 */

static void free_rx_buffers(struct s2io_nic *sp)
{
	struct net_device *dev = sp->dev;
	int i, blk = 0, buf_cnt = 0;
	struct mac_info *mac_control;
	struct config_param *config;

	mac_control = &sp->mac_control;
	config = &sp->config;

	for (i = 0; i < config->rx_ring_num; i++) {

		for (blk = 0; blk < rx_ring_sz[i]; blk++)
			free_rxd_blk(sp, i, blk);

		mac_control->rings[i].rx_curr_put_info.block_index = 0;
		mac_control->rings[i].rx_curr_get_info.block_index = 0;
		mac_control->rings[i].rx_curr_put_info.offset = 0;
		mac_control->rings[i].rx_curr_get_info.offset = 0;
		mac_control->rings[i].rx_bufs_left = 0;

		DBG_PRINT(INIT_DBG, "%s:Freed 0x%x Rx Buffers on ring%d\n",
			  dev->name, buf_cnt, i);
	}
}

static int s2io_chk_rx_buffers(struct ring_info *ring)
{
	if (fill_rx_buffers(ring) == -ENOMEM) {
		DBG_PRINT(INFO_DBG, "%s - %s:Out of memory", __FUNCTION__,
		ring->dev->name);
		DBG_PRINT(INFO_DBG, " in Rx Intr!!\n");
	}
	return 0;
}

/**
 * s2io_poll - Rx interrupt handler for New NAPI support
 * @dev : pointer to the device structure.
 * @budget : The number of packets that were budgeted to be processed
 * during  one pass through the 'Poll" function.
 * Description:
 * Comes into picture only if NAPI support has been incorporated. It does
 * the same thing that rx_intr_handler does, but not in a interrupt context
 * also It will process only a given number of packets.
 * Return value:
 * No of Rx packets processed.
 */

#if defined(HAVE_NETDEV_POLL)
static int s2io_poll_msix(struct napi_struct *napi, int budget)
{
	struct ring_info *ring = container_of(napi, struct ring_info, napi);
	struct net_device *dev = ring->dev;
	struct config_param *config;
	struct mac_info *mac_control;
	int pkts_processed = 0;
	u8 *addr, val8;
	struct s2io_nic *nic = dev->priv;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	int budget_org = budget;

	config = &nic->config;
	mac_control = &nic->mac_control;

	if (unlikely(!is_s2io_card_up(nic)))
		return 0;

	pkts_processed = rx_intr_handler(ring, budget);
	s2io_chk_rx_buffers(ring);

	if (pkts_processed < budget_org) {
		netif_rx_complete(dev, napi);
		/*Re Enable MSI-Rx Vector*/
		addr = (u8 *)&bar0->xmsi_mask_reg;
		addr += 7 - ring->ring_no;
		val8 = (ring->ring_no == 0) ? 0x3f : 0xbf;
		writeb(val8, addr);
		val8 = readb(addr);
	}
	return pkts_processed;
}
static int s2io_poll_inta(struct napi_struct *napi, int budget)
{
	struct s2io_nic *nic = container_of(napi, struct s2io_nic, napi);
	struct ring_info *ring;
	struct net_device *dev = nic->dev;
	struct config_param *config;
	struct mac_info *mac_control;
	int pkts_processed = 0;
	int ring_pkts_processed, i;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	int budget_org = budget;

	config = &nic->config;
	mac_control = &nic->mac_control;

	if (unlikely(!is_s2io_card_up(nic)))
		return 0;

	for (i = 0; i < config->rx_ring_num; i++) {
		ring = &mac_control->rings[i];
		ring_pkts_processed = rx_intr_handler(ring, budget);
		s2io_chk_rx_buffers(ring);
		pkts_processed += ring_pkts_processed;
		budget -= ring_pkts_processed;

		if (budget <= 0)
			break;
	}

	if (pkts_processed < budget_org) {
		netif_rx_complete(dev, napi);
		/* Re enable the Rx interrupts for the ring */
		writeq(0, &bar0->rx_traffic_mask);
		readl(&bar0->rx_traffic_mask);
	}
	return pkts_processed;
}
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
/**
 * s2io_netpoll - netpoll event handler entry point
 * @dev : pointer to the device structure.
 * Description:
 * 	This function will be called by upper layer to check for events on the
 * interface in situations where interrupts are disabled. It is used for
 * specific in-kernel networking tasks, such as remote consoles and kernel
 * debugging over the network (example netdump in RedHat).
 */
static void s2io_netpoll(struct net_device *dev)
{
	struct s2io_nic *nic = dev->priv;
	struct mac_info *mac_control;
	struct config_param *config;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64 = 0xFFFFFFFFFFFFFFFFULL;
	int i;
	if (pci_channel_offline(nic->pdev))
		return;

	disable_irq(dev->irq);

	mac_control = &nic->mac_control;
	config = &nic->config;

	writeq(val64, &bar0->rx_traffic_int);
	writeq(val64, &bar0->tx_traffic_int);

	/* we need to free up the transmitted skbufs or else netpoll will
	 * run out of skbs and will fail and eventually netpoll application such
	 * as netdump will fail.
	 */
	for (i = 0; i < config->tx_fifo_num; i++)
		tx_intr_handler(&mac_control->fifos[i]);

	/* check for received packet and indicate up to network */
	for (i = 0; i < config->rx_ring_num; i++)
		rx_intr_handler(&mac_control->rings[i], 0);

	for (i = 0; i < config->rx_ring_num; i++) {
		if (fill_rx_buffers(&mac_control->rings[i]) == -ENOMEM) {
			DBG_PRINT(INFO_DBG, "%s - %s:Out of memory",
			__FUNCTION__, dev->name);
			DBG_PRINT(INFO_DBG, " in Rx Netpoll!!\n");
			break;
		}
	}
	enable_irq(dev->irq);
	return;
}
#endif

static inline void clear_lro_session(struct lro *lro)
{
	lro->in_use = 0;
	lro->saw_ts = 0;
	lro->last_frag = NULL;
}

static inline void queue_rx_frame(struct sk_buff *skb, u16 vlan_tag)
{
	struct net_device *dev = skb->dev;
	struct s2io_nic *sp = dev->priv;

	skb->protocol = eth_type_trans(skb, dev);
	if (sp->vlgrp && vlan_tag
		&& (sp->vlan_strip_flag == S2IO_STRIP_VLAN_TAG)) {
		/* Queueing the vlan frame to the upper layer */
		if (sp->config.napi)
			vlan_hwaccel_receive_skb(skb, sp->vlgrp, vlan_tag);
		else
			vlan_hwaccel_rx(skb, sp->vlgrp, vlan_tag);
	} else {
		if (sp->config.napi)
			netif_receive_skb(skb);
		else
			netif_rx(skb);
	}
}

/**
 *  rx_intr_handler - Rx interrupt handler
 *  @nic: device private variable.
 *  Description:
 *  If the interrupt is because of a received frame or if the
 *  receive ring contains fresh as yet un-processed frames,this function is
 *  called. It picks out the RxD at which place the last Rx processing had
 *  stopped and sends the skb to the OSM's Rx handler and then increments
 *  the offset.
 *  Return Value:
 *  No. of napi packets processed.
 */
static int rx_intr_handler(struct ring_info *ring, int budget)
{
	struct s2io_nic *nic = ring->nic;
	struct net_device *dev = (struct net_device *) ring->dev;
	int get_block, put_block;
	struct rx_curr_get_info get_info, put_info;
	struct RxD_t *rxdp;
	struct sk_buff *skb;
	int pkt_cnt = 0, napi_pkts = 0;
	int i;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct RxD5 *rxdp5;

	get_info = ring->rx_curr_get_info;
	get_block = get_info.block_index;
	memcpy(&put_info, &ring->rx_curr_put_info, sizeof(put_info));
	put_block = put_info.block_index;
	rxdp = ring->rx_blocks[get_block].rxds[get_info.offset].virt_addr;
	while (RXD_IS_UP2DT(rxdp)) {
		/* If your are next to put index then it's FIFO
		* full condition
		*/
		if ((get_block == put_block) &&
			((get_info.offset + 1) == put_info.offset)) {
			DBG_PRINT(INTR_DBG, "%s: Ring Full\n",dev->name);
			break;
		}
		if (ring->rxd_mode == RXD_MODE_5)
			skb = (struct sk_buff *)
				((unsigned long)ring->skbs[((unsigned long) \
					((struct RxD5 *)rxdp)->Host_Control)]);
		else
#if defined (CONFIG_64BIT) || defined (__VMKLNX__)
			skb = (struct sk_buff *) ((u64)rxdp->Host_Control);
#else  /* !defined (CONFIG_64BIT) && !defined (__VMKLNX__) */
			skb = (struct sk_buff *) ((u32)rxdp->Host_Control);
#endif /* defined (CONFIG_64BIT) || defined (__VMKLNX__) */

		if (skb == NULL) {
			DBG_PRINT(INTR_DBG, "%s: The skb is ",
				  dev->name);
			DBG_PRINT(INTR_DBG, "Null in Rx Intr\n");
			nic->sw_err_stat->skb_null_rx_intr_handler_cnt++;
			return 0;
		}

		switch (ring->rxd_mode) {
		case RXD_MODE_1:
			rxdp1 = (struct RxD1 *)rxdp;
			pci_unmap_single(ring->pdev, (dma_addr_t)
				rxdp1->Buffer0_ptr,
				dev->mtu +
				HEADER_ETHERNET_II_802_3_SIZE +
				HEADER_802_2_SIZE +
				HEADER_SNAP_SIZE,
				PCI_DMA_FROMDEVICE);
			break;

		case RXD_MODE_5:
			rxdp5 = (struct RxD5 *)rxdp;
			pci_dma_sync_single_for_cpu(ring->pdev, (dma_addr_t)
				rxdp5->Buffer0_ptr, BUF0_LEN,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(ring->pdev, (dma_addr_t)
				rxdp5->Buffer1_ptr,
				l3l4hdr_size + 4,
				PCI_DMA_FROMDEVICE);
			pci_unmap_single(ring->pdev, (dma_addr_t)
				rxdp5->Buffer2_ptr,
				MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
			if (ring->nic->skbs_per_rxd > 2)
				pci_unmap_single(ring->pdev, (dma_addr_t)
					rxdp5->Buffer3_ptr,
					MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
			if (nic->skbs_per_rxd > 3)
				pci_unmap_single(ring->pdev, (dma_addr_t)
					rxdp5->Buffer4_ptr,
					MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
			break;

		case RXD_MODE_3B:
			rxdp3 = (struct RxD3 *)rxdp;
			pci_dma_sync_single_for_cpu(ring->pdev, (dma_addr_t)
				rxdp3->Buffer0_ptr,
				BUF0_LEN, PCI_DMA_FROMDEVICE);
			pci_unmap_single(ring->pdev, (dma_addr_t)
				rxdp3->Buffer2_ptr,
				dev->mtu + 4,
				PCI_DMA_FROMDEVICE);
			break;
		}

		(void) rx_osm_handler(ring, rxdp);
		get_info.offset++;
		ring->rx_curr_get_info.offset = get_info.offset;
		rxdp = ring->rx_blocks[get_block].
				rxds[get_info.offset].virt_addr;
		if (get_info.offset == ring->rxd_count) {
			get_info.offset = 0;
			ring->rx_curr_get_info.offset = get_info.offset;
			get_block++;
			if (get_block == ring->block_count)
				get_block = 0;
			ring->rx_curr_get_info.block_index = get_block;
			rxdp = ring->rx_blocks[get_block].block_virt_addr;
		}

		if (nic->config.napi) {
			budget--;
			napi_pkts++;
			if (!budget)
				break;
		}

		pkt_cnt++;
		if ((indicate_max_pkts) && (pkt_cnt > indicate_max_pkts))
			break;

	}

	if (ring->lro) {
		/* Clear all LRO sessions before exiting */
		for (i=0; i<MAX_LRO_SESSIONS; i++) {
			struct lro *lro = &ring->lro0_n[i];
			if (lro->in_use) {
				update_L3L4_header(nic, lro, ring->ring_no);
				queue_rx_frame(lro->parent, lro->vlan_tag);
				clear_lro_session(lro);
			}
		}
	}
	return(napi_pkts);
}

/**
 *  tx_intr_handler - Transmit interrupt handler
 *  @nic : device private variable
 *  Description:
 *  If an interrupt was raised to indicate DMA complete of the
 *  Tx packet, this function is called. It identifies the last TxD
 *  whose buffer was freed and frees all skbs whose data have already
 *  DMA'ed into the NICs internal memory.
 *  Return Value:
 *  NONE
 */

static void tx_intr_handler(struct fifo_info *fifo_data)
{
	u8 err;
	struct s2io_nic *nic = fifo_data->nic;
	struct tx_curr_get_info get_info, put_info;
	struct sk_buff *skb = NULL;
	struct TxD *txdlp;
	struct sk_buff *head = NULL;
	struct sk_buff **temp;
	int pkt_cnt = 0;
	unsigned long flags = 0;
	struct swDbgStat *stats = nic->sw_dbg_stat;

	if (!spin_trylock_irqsave(&fifo_data->tx_lock, flags))
		return;

	get_info = fifo_data->tx_curr_get_info;
	put_info = fifo_data->tx_curr_put_info;
	txdlp = (struct TxD *) fifo_data->list_info[get_info.offset].
		list_virt_addr;
	while ((!(txdlp->Control_1 & TXD_LIST_OWN_XENA)) &&
		(get_info.offset != put_info.offset) &&
		(txdlp->Host_Control)) {
		/* Check for TxD errors */
		err = GET_TXD_T_CODE(txdlp->Control_1);
		if (err) {

			/* update t_code statistics */
			switch (err) {
			case 2:
				fifo_data->tx_fifo_stat->tx_buf_abort_cnt++;
				break;

			case 3:
				fifo_data->tx_fifo_stat->tx_desc_abort_cnt++;
				break;

			case 7:
				fifo_data->tx_fifo_stat->tx_parity_err_cnt++;
				break;

			case 10:
				fifo_data->tx_fifo_stat->tx_link_loss_cnt++;
				break;

			case 15:
				fifo_data->tx_fifo_stat->tx_list_proc_err_cnt++;
				break;
			}
		}

		skb = s2io_txdl_getskb(fifo_data, txdlp, get_info.offset);
		if (skb == NULL) {
			spin_unlock_irqrestore(&fifo_data->tx_lock, flags);
			DBG_PRINT(INTR_DBG, "%s: Null skb ",
			__FUNCTION__);
			DBG_PRINT(INTR_DBG, "in Tx Free Intr\n");
			nic->sw_err_stat->skb_null_tx_intr_handler_cnt++;
			return;
		}
		temp = (struct sk_buff **)&skb->cb;
		*temp = head;
		head = skb;
		pkt_cnt++;

		/* Updating the statistics block */
		nic->stats.tx_bytes += skb->len;
		stats->mem_freed += skb->truesize;

		get_info.offset++;
		if (get_info.offset == get_info.fifo_len + 1)
			get_info.offset = 0;
		txdlp = (struct TxD *) fifo_data->list_info
			[get_info.offset].list_virt_addr;
		fifo_data->tx_curr_get_info.offset =
		    get_info.offset;
	}

	s2io_wake_tx_queue(fifo_data, pkt_cnt, nic->config.multiq);

	spin_unlock_irqrestore(&fifo_data->tx_lock, flags);

	while (head) {
		skb = head;
		temp = (struct sk_buff **)&skb->cb;
		head = *temp;
		*temp = NULL;
		dev_kfree_skb_irq(skb);
	}
}

/**
 *  s2io_mdio_write - Function to write in to MDIO registers
 *  @mmd_type : MMD type value (PMA/PMD/WIS/PCS/PHYXS)
 *  @addr     : address value
 *  @value    : data value
 *  @dev      : pointer to net_device structure
 *  Description:
 *  This function is used to write values to the MDIO registers
 *  NONE
 */
static void s2io_mdio_write(u32 mmd_type, u64 addr, u16 value, struct net_device *dev)
{
	u64 val64 = 0x0;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* address transaction */
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x0);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/*Data transaction */
	val64 = 0x0;
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x0)
			| MDIO_MDIO_DATA(value)
			| MDIO_OP(MDIO_OP_WRITE_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	val64 = 0x0;
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
	| MDIO_MMD_DEV_ADDR(mmd_type)
	| MDIO_MMS_PRT_ADDR(0x0)
	| MDIO_OP(MDIO_OP_READ_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

}

/**
 *  s2io_mdio_read - Function to write in to MDIO registers
 *  @mmd_type : MMD type value (PMA/PMD/WIS/PCS/PHYXS)
 *  @addr     : address value
 *  @dev      : pointer to net_device structure
 *  Description:
 *  This function is used to read values to the MDIO registers
 *  NONE
 */
static u64 s2io_mdio_read(u32 mmd_type, u64 addr, struct net_device *dev)
{
	u64 val64 = 0x0;
	u64 rval64 = 0x0;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* address transaction */
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x0);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/* Data transaction */
	val64 = 0x0;
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x0)
			| MDIO_OP(MDIO_OP_READ_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/* Read the value from regs */
	rval64 = readq(&bar0->mdio_control);
	rval64 = rval64 & 0xFFFF0000;
	rval64 = rval64 >> 16;
	return rval64;
}

static u64 s2io_dtx_read(u32 mmd_type, u64 addr, struct net_device *dev)
{
	u64 val64 = 0x0;
	u64 rval64 = 0x0;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* address transaction */
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x15);

	SPECIAL_REG_WRITE(val64,&bar0->dtx_control, UF); 
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	SPECIAL_REG_WRITE(val64,&bar0->dtx_control, UF); 
	udelay(100);

	/* Data transaction */
	val64 = 0x0;
	val64 = val64 | MDIO_MMD_INDX_ADDR(addr)
			| MDIO_MMD_DEV_ADDR(mmd_type)
			| MDIO_MMS_PRT_ADDR(0x15)
			| MDIO_OP(MDIO_OP_READ_TRANS);
	SPECIAL_REG_WRITE(val64,&bar0->dtx_control, UF); 
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	SPECIAL_REG_WRITE(val64,&bar0->dtx_control, UF); 
	udelay(100);

	/* Read the value from regs */
	rval64 = readq(&bar0->dtx_control);
	rval64 = rval64 & 0xFFFF0000;
	rval64 = rval64 >> 16;
	return rval64;
}


/**
 *  s2io_chk_xpak_counter - Function to check the status of the xpak counters
 *  @counter      : couter value to be updated
 *  @flag         : flag to indicate the status
 *  @type         : counter type
 *  Description:
 *  This function is to check the status of the xpak counters value
 *  NONE
 */

static void s2io_chk_xpak_counter(u64 *counter, u64 *regs_stat, u32 index,
	u16 flag, u16 type)
{
	u64 mask = 0x3;
	u64 val64;
	int i;

	for (i = 0; i < index; i++)
		mask = mask << 0x2;

	if (flag > 0) {
		*counter = *counter + 1;
		val64 = *regs_stat & mask;
		val64 = val64 >> (index * 0x2);
		val64 = val64 + 1;
		if (val64 == 3) {
			switch(type)
			{
			case 1:
				DBG_PRINT(ERR_DBG, "Take Xframe NIC out of "
					"service. Excessive temperatures may "
					"result in premature transceiver "
					"failure \n");
				break;
			case 2:
				DBG_PRINT(ERR_DBG, "Take Xframe NIC out of "
					"service Excessive bias currents may "
					"indicate imminent laser diode "
					"failure \n");
				break;
			case 3:
				DBG_PRINT(ERR_DBG, "Take Xframe NIC out of "
					"service Excessive laser output "
					"power may saturate far-end "
					"receiver\n");
				break;
			default:
				DBG_PRINT(ERR_DBG, "Incorrect XPAK Alarm "
					  "type \n");
			}
			val64 = 0x0;
		}
		val64 = val64 << (index * 0x2);
		*regs_stat = (*regs_stat & (~mask)) | (val64);

	} else
		*regs_stat = *regs_stat & (~mask);
}

/**
 *  s2io_updt_xpak_counter - Function to update the xpak counters
 *  @dev         : pointer to net_device struct
 *  Description:
 *  This function is to upate the status of the xpak counters value
 *  NONE
 */
static void s2io_updt_xpak_counter(struct net_device *dev)
{
	u16 flag  = 0x0;
	u16 type  = 0x0;
	u16 val16 = 0x0;
	u64 val64 = 0x0;
	u64 addr  = 0x0;

	struct s2io_nic *sp = dev->priv;
	struct xpakStat *xpak_stat = sp->xpak_stat;

	/* Check the communication with the MDIO slave */
	addr = 0x0000;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMA_DEV_ADDR, addr, dev);
	if ((val64 == 0xFFFF) || (val64 == 0x0000)) {
		DBG_PRINT(ERR_DBG, "ERR: MDIO slave access failed - "
			  "Returned %llx\n", (unsigned long long)val64);
		return;
	}

	/* Check for the expecte value of 2040 at PMA address 0x0000 */
	if (val64 != 0x2040) {
		DBG_PRINT(ERR_DBG, "Incorrect value at PMA address 0x0000 - ");
		DBG_PRINT(ERR_DBG, "Returned: %llx- Expected: 0x2040\n",
			  (unsigned long long)val64);
		return;
	}

	/* Loading the DOM register to MDIO register */
	addr = 0xA100;
	s2io_mdio_write(MDIO_MMD_PMA_DEV_ADDR, addr, val16, dev);
	val64 = s2io_mdio_read(MDIO_MMD_PMA_DEV_ADDR, addr, dev);

	/* Reading the Alarm flags */
	addr = 0xA070;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMA_DEV_ADDR, addr, dev);

	flag = CHECKBIT(val64, 0x7);
	type = 1;
	s2io_chk_xpak_counter(&xpak_stat->alarm_transceiver_temp_high,
				&xpak_stat->xpak_regs_stat,
				0x0, flag, type);

	if (CHECKBIT(val64, 0x6))
		xpak_stat->alarm_transceiver_temp_low++;

	flag = CHECKBIT(val64, 0x3);
	type = 2;
	s2io_chk_xpak_counter(&xpak_stat->alarm_laser_bias_current_high,
				&xpak_stat->xpak_regs_stat,
				0x2, flag, type);

	if (CHECKBIT(val64, 0x2))
		xpak_stat->alarm_laser_bias_current_low++;

	flag = CHECKBIT(val64, 0x1);
	type = 3;
	s2io_chk_xpak_counter(&xpak_stat->alarm_laser_output_power_high,
				&xpak_stat->xpak_regs_stat,
				0x4, flag, type);

	if (CHECKBIT(val64, 0x0))
		xpak_stat->alarm_laser_output_power_low++;

	/* Reading the Warning flags */
	addr = 0xA074;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMA_DEV_ADDR, addr, dev);

	if (CHECKBIT(val64, 0x7))
		xpak_stat->warn_transceiver_temp_high++;

	if (CHECKBIT(val64, 0x6))
		xpak_stat->warn_transceiver_temp_low++;

	if (CHECKBIT(val64, 0x3))
		xpak_stat->warn_laser_bias_current_high++;

	if (CHECKBIT(val64, 0x2))
		xpak_stat->warn_laser_bias_current_low++;

	if (CHECKBIT(val64, 0x1))
		xpak_stat->warn_laser_output_power_high++;

	if (CHECKBIT(val64, 0x0))
		xpak_stat->warn_laser_output_power_low++;
}

/**
 *  wait_for_cmd_complete - waits for a command to complete.
 *  @addr: address value
 *  @busy_bit: bit in the register that needs to be checked
 *  @bit_state: flag to check if bit is set or reset
 *  Description: Function that waits for a command to be completed
 *  and returns either success or error depending on whether the
 *  command was complete or not.
 *  Return value:
 *   SUCCESS or FAILURE.
 */

static int wait_for_cmd_complete(void  __iomem *addr, u64 busy_bit,
				int bit_state)
{
	int ret = FAILURE, cnt = 0;
	u64 val64;
	int delay = 1;

	if ((bit_state != S2IO_BIT_RESET) && (bit_state != S2IO_BIT_SET))
		return FAILURE;

	do {
		val64 = readq(addr);
		if (bit_state == S2IO_BIT_RESET) {
			if (!(val64 & busy_bit)) {
				ret = SUCCESS;
				break;
			}
		} else {
			if (val64 & busy_bit) {
				ret = SUCCESS;
				break;
			}
		}

		if (in_interrupt())
			mdelay(delay);
		else
			msleep(delay);

		if (++cnt >= 10)
			delay = 50;

	} while (cnt < 20);
	return ret;
}

/*
 * check_pci_device_id - Checks if the device id is supported
 * @id : device id
 * Description: Function to check if the pci device id is supported by the driver.
 * Return value: Actual device id if supported else PCI_ANY_ID
 */
static u16 check_pci_device_id(u16 id)
{
	switch (id) {
#ifdef TITAN_LEGACY
	case PCI_DEVICE_ID_TITAN_WIN:
	case PCI_DEVICE_ID_TITAN_UNI:
		return TITAN_DEVICE;
#endif
	case PCI_DEVICE_ID_HERC_WIN:
	case PCI_DEVICE_ID_HERC_UNI:
		return XFRAME_II_DEVICE;
	case PCI_DEVICE_ID_S2IO_UNI:
	case PCI_DEVICE_ID_S2IO_WIN:
		return XFRAME_I_DEVICE;
	default:
		return PCI_ANY_ID;
	}
}

/**
 *  s2io_reset - Resets the card.
 *  @sp : private member of the device structure.
 *  Description: Function to Reset the card. This function then also
 *  restores the previously saved PCI configuration space registers as
 *  the card reset also resets the configuration space.
 *  Return value:
 *  void.
 */

static void s2io_reset(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	u16 subid, pci_cmd;
	int i;
	u16 val16;
	struct swErrStat err_stat;

	DBG_PRINT(INIT_DBG, "%s - Resetting XFrame card %s\n",
		__FUNCTION__, sp->dev->name);

	/* Back up  the PCI-X CMD reg, dont want to lose MMRBC, OST settings */
	pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER, &(pci_cmd));

	val64 = SW_RESET_ALL;
	writeq(val64, &bar0->sw_reset);

	if (strstr(sp->product_name, "CX4"))
		msleep(750);
	msleep(250);

	for (i = 0; i < S2IO_MAX_PCI_CONFIG_SPACE_REINIT; i++) {

		/* Restore the PCI state saved during initialization. */
		pci_restore_state(sp->pdev, sp->config_space);
		pci_read_config_word(sp->pdev, 0x2, &val16);
		if (check_pci_device_id(val16) != (u16)PCI_ANY_ID)
			break;
		msleep(200);
	}

	if (check_pci_device_id(val16) == (u16)PCI_ANY_ID)
		DBG_PRINT(ERR_DBG, "%s SW_Reset failed!\n", __FUNCTION__);

	pci_write_config_word(sp->pdev, PCIX_COMMAND_REGISTER, pci_cmd);

	s2io_init_pci(sp);

	/* Set swapper to enable I/O register access */
	s2io_set_swapper(sp);

	/* restore mac_addr entries */
	do_s2io_restore_unicast_mc(sp);

#ifdef CONFIG_PCI_MSI
	/* Restore the MSIX table entries from local variables */
	restore_xmsi_data(sp);
#endif

	/* Clear certain PCI/PCI-X fields after reset */
	if (sp->device_type == XFRAME_II_DEVICE) {
		/* Clear "detected parity error" bit */
		pci_write_config_word(sp->pdev, PCI_STATUS, 0x8000);

		/* Clearing PCIX Ecc status register */
		pci_write_config_dword(sp->pdev, 0x68, 0x7C);

		/* Clearing PCI_STATUS error reflected here */
		writeq(S2BIT(62), &bar0->txpic_int_reg);
	}

	/* Reset device statistics maintained by OS */
	memset(&sp->stats, 0, sizeof (struct net_device_stats));

	/* save all the software error statistics */
	memcpy(&err_stat, &sp->mac_stat_info, sizeof(struct swErrStat));

	memset(sp->mac_control.stats_info, 0, sizeof(struct stat_block));

	/* restore the software error statistics */
	memcpy(&sp->mac_stat_info, &err_stat, sizeof(struct swErrStat));

	/* SXE-002: Configure link and activity LED to turn it off */
	subid = sp->pdev->subsystem_device;
	if (((subid & 0xFF) >= 0x07) &&
		(sp->device_type == XFRAME_I_DEVICE)) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->gpio_control);
		val64 = 0x0411040400000000ULL;
		writeq(val64, (void __iomem *)bar0 + 0x2700);
	}

	/*
	 * Clear spurious ECC interrupts that would have occured on
	 * XFRAME II cards after reset.
	 */
	if (sp->device_type == XFRAME_II_DEVICE) {
		val64 = readq(&bar0->pcc_err_reg);
		writeq(val64, &bar0->pcc_err_reg);
	}

	sp->device_enabled_once = FALSE;
}

/**
 *  s2io_set_swapper - to set the swapper controle on the card
 *  @sp : private member of the device structure,
 *  pointer to the s2io_nic structure.
 *  Description: Function to set the swapper control on the card
 *  correctly depending on the 'endianness' of the system.
 *  Return value:
 *  SUCCESS on success and FAILURE on failure.
 */

static int s2io_set_swapper(struct s2io_nic *sp)
{
	struct net_device *dev = sp->dev;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64, valt, valr, sw_wr, rth_wr;

	/*
	 * Set proper endian settings and verify the same by reading
	 * the PIF Feed-back register.
	 */

	val64 = readq(&bar0->pif_rd_swapper_fb);
	if (val64 != 0x0123456789ABCDEFULL) {
		int i = 0;
		u64 value[] = { 0xC30000C3C30000C3ULL,	/* FE=1, SE=1 */
				0x8100008181000081ULL,	/* FE=1, SE=0 */
				0x4200004242000042ULL,	/* FE=0, SE=1 */
				0};			/* FE=0, SE=0 */

		while (i < 4) {
			writeq(value[i], &bar0->swapper_ctrl);
			val64 = readq(&bar0->pif_rd_swapper_fb);
			if (val64 == 0x0123456789ABCDEFULL)
				break;
			i++;
		}
		if (i == 4) {
			DBG_PRINT(ERR_DBG, "%s: Endian settings are wrong, ",
				dev->name);
			DBG_PRINT(ERR_DBG, "feedback read %llx\n",
				(unsigned long long) val64);
			return FAILURE;
		}
		valr = value[i];
	} else
		valr = readq(&bar0->swapper_ctrl);

	valt = 0x0123456789ABCDEFULL;
	writeq(valt, &bar0->xmsi_address);
	val64 = readq(&bar0->xmsi_address);

	if (val64 != valt) {
		int i = 0;
		u64 value[] = { 0x00C3C30000C3C300ULL,  /* FE=1, SE=1 */
				0x0081810000818100ULL,  /* FE=1, SE=0 */
				0x0042420000424200ULL,  /* FE=0, SE=1 */
				0};                     /* FE=0, SE=0 */

		while (i < 4) {
			writeq((value[i] | valr), &bar0->swapper_ctrl);
			writeq(valt, &bar0->xmsi_address);
			val64 = readq(&bar0->xmsi_address);
			if(val64 == valt)
				break;
			i++;
		}
		if (i == 4) {
			unsigned long long x = val64;
			DBG_PRINT(ERR_DBG, "Write failed, Xmsi_addr ");
			DBG_PRINT(ERR_DBG, "reads:0x%llx\n", x);
			return FAILURE;
		}
	}
	val64 = readq(&bar0->swapper_ctrl);
	val64 &= 0xFFFF000000000000ULL;
	sw_wr = (val64 & vBIT(3, 8, 2));
	rth_wr = (sw_wr >> 2);
	val64 |= rth_wr;

#ifdef  __BIG_ENDIAN
	/*
	 * The device by default set to a big endian format, so a
	 * big endian driver need not set anything.
	 */
	val64 |= (SWAPPER_CTRL_TXP_FE |
		 SWAPPER_CTRL_TXP_SE |
		 SWAPPER_CTRL_TXD_R_FE |
		 SWAPPER_CTRL_TXD_W_FE |
		 SWAPPER_CTRL_TXF_R_FE |
		 SWAPPER_CTRL_RXD_R_FE |
		 SWAPPER_CTRL_RXD_W_FE |
		 SWAPPER_CTRL_RXF_W_FE |
		 SWAPPER_CTRL_XMSI_FE |
		 SWAPPER_CTRL_STATS_FE | SWAPPER_CTRL_STATS_SE);
	if (sp->config.intr_type == INTA)
		val64 |= SWAPPER_CTRL_XMSI_SE;
	writeq(val64, &bar0->swapper_ctrl);
#else
	/*
	 * Initially we enable all bits to make it accessible by the
	 * driver, then we selectively enable only those bits that
	 * we want to set.
	 */
	val64 |= (SWAPPER_CTRL_TXP_FE |
		 SWAPPER_CTRL_TXP_SE |
		 SWAPPER_CTRL_TXD_R_FE |
		 SWAPPER_CTRL_TXD_R_SE |
		 SWAPPER_CTRL_TXD_W_FE |
		 SWAPPER_CTRL_TXD_W_SE |
		 SWAPPER_CTRL_TXF_R_FE |
		 SWAPPER_CTRL_RXD_R_FE |
		 SWAPPER_CTRL_RXD_R_SE |
		 SWAPPER_CTRL_RXD_W_FE |
		 SWAPPER_CTRL_RXD_W_SE |
		 SWAPPER_CTRL_RXF_W_FE |
		 SWAPPER_CTRL_XMSI_FE |
		 SWAPPER_CTRL_STATS_FE | SWAPPER_CTRL_STATS_SE);
	if (sp->config.intr_type == INTA)
		val64 |= SWAPPER_CTRL_XMSI_SE;
	writeq(val64, &bar0->swapper_ctrl);
#endif
	val64 = readq(&bar0->swapper_ctrl);

	/*
	 * Verifying if endian settings are accurate by reading a
	 * feedback register.
	 */
	val64 = readq(&bar0->pif_rd_swapper_fb);
	if (val64 != 0x0123456789ABCDEFULL) {
		/* Endian settings are incorrect, calls for another dekko. */
		DBG_PRINT(ERR_DBG, "%s: Endian settings are wrong, ",
			  dev->name);
		DBG_PRINT(ERR_DBG, "feedback read %llx\n",
			  (unsigned long long) val64);
		return FAILURE;
	}

	return SUCCESS;
}

#ifdef CONFIG_PCI_MSI
static int wait_for_msix_trans(struct s2io_nic *nic, int i)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64;
	int ret = 0, cnt = 0;

	do {
		val64 = readq(&bar0->xmsi_access);
		if (!(val64 & S2BIT(15)))
			break;
		mdelay(1);
		cnt++;
	} while(cnt < 5);
	if (cnt == 5) {
		DBG_PRINT(ERR_DBG, "XMSI # %d Access failed\n", i);
		ret = 1;
	}

	return ret;
}

static void restore_xmsi_data(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64;
	int i, msix_indx;

	if (nic->device_type == XFRAME_I_DEVICE)
		return;

	for (i = 0; i < MAX_REQUESTED_MSI_X; i++) {
		msix_indx = (i) ? ((i-1) * 8 + 1): 0;
		writeq(nic->msix_info[i].addr, &bar0->xmsi_address);
		writeq(nic->msix_info[i].data, &bar0->xmsi_data);
		val64 = (S2BIT(7) | S2BIT(15) | vBIT(msix_indx, 26, 6));
		writeq(val64, &bar0->xmsi_access);
		if (wait_for_msix_trans(nic, msix_indx)) {
			DBG_PRINT(ERR_DBG, "failed in %s\n", __FUNCTION__);
			continue;
		}
	}
}

static void store_xmsi_data(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64, addr, data;
	int i, msix_indx;

	if (nic->device_type == XFRAME_I_DEVICE)
		return;

	/* Store and display */
	for (i = 0; i < MAX_REQUESTED_MSI_X; i++) {
		msix_indx = (i) ? ((i-1) * 8 + 1): 0;
		val64 = (S2BIT(15) | vBIT(msix_indx, 26, 6));
		writeq(val64, &bar0->xmsi_access);
		if (wait_for_msix_trans(nic, msix_indx)) {
			DBG_PRINT(ERR_DBG, "failed in %s\n", __FUNCTION__);
			continue;
		}
		addr = readq(&bar0->xmsi_address);
		data = readq(&bar0->xmsi_data);
		if (addr && data) {
			nic->msix_info[i].addr = addr;
			nic->msix_info[i].data = data;
		}
	}
}

static int s2io_enable_msi_x(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 rx_mat;
	u16 msi_control; /* Temp variable */
	int ret, i, j, msix_indx = 1;

	nic->entries = kmalloc(nic->num_entries * sizeof(struct msix_entry),
				GFP_KERNEL);
	if (nic->entries == NULL) {
		DBG_PRINT(INFO_DBG, "%s: Memory allocation failed\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	memset(nic->entries, 0, nic->num_entries * sizeof(struct msix_entry));

	nic->s2io_entries =
		kmalloc(nic->num_entries * sizeof(struct s2io_msix_entry),
				   GFP_KERNEL);
	if (nic->s2io_entries == NULL) {
		DBG_PRINT(INFO_DBG, "%s: Memory allocation failed\n",
			__FUNCTION__);
		kfree(nic->entries);
		return -ENOMEM;
	}
	memset(nic->s2io_entries, 0,
		nic->num_entries * sizeof(struct s2io_msix_entry));

	nic->entries[0].entry = 0;
	nic->s2io_entries[0].entry = 0;
	nic->s2io_entries[0].in_use = MSIX_FLG;
	nic->s2io_entries[0].type = MSIX_ALARM_TYPE;
	nic->s2io_entries[0].arg = &nic->mac_control.fifos;

	for (i = 1; i < nic->num_entries; i++) {
		nic->entries[i].entry = ((i - 1) * 8) + 1;
		nic->s2io_entries[i].entry = ((i - 1) * 8) + 1;
		nic->s2io_entries[i].arg = NULL;
		nic->s2io_entries[i].in_use = 0;
	}

	rx_mat = readq(&bar0->rx_mat);
	for (j = 0; j < nic->config.rx_ring_num; j++) {
		rx_mat |= RX_MAT_SET(j, msix_indx);
		nic->s2io_entries[j+1].arg = &nic->mac_control.rings[j];
		nic->s2io_entries[j+1].type = MSIX_RING_TYPE;
		nic->s2io_entries[j+1].in_use = MSIX_FLG;
		msix_indx += 8;
	}
	writeq(rx_mat, &bar0->rx_mat);
	readq(&bar0->rx_mat);

	ret = pci_enable_msix(nic->pdev, nic->entries, nic->num_entries);
	/* We fail init if error or we get less vectors than min required */
	if (ret) {
		DBG_PRINT(ERR_DBG, "s2io: Enabling MSIX failed\n");
		if (ret > 0)
			DBG_PRINT(ERR_DBG, "s2io: Requested for %d, " \
				"but got only %d vector(s) try with  " \
				"rx_ring_num = %d\n",
				nic->num_entries, ret, (ret - 1));
		kfree(nic->entries);
		kfree(nic->s2io_entries);
		nic->entries = NULL;
		nic->s2io_entries = NULL;
		return -ENOMEM;
	}

	/*
	 * To enable MSI-X, MSI also needs to be enabled, due to a bug
	 * in the herc NIC. (Temp change, needs to be removed later)
	 */
	pci_read_config_word(nic->pdev, 0x42, &msi_control);
	msi_control |= 0x1; /* Enable MSI */
	pci_write_config_word(nic->pdev, 0x42, msi_control);

	return 0;
}

/* Handle software interrupt used during MSI(X) test */
static irqreturn_t __devinit s2io_test_intr(int irq, void *dev_id)
{
	struct s2io_nic *sp = dev_id;

	sp->msi_detected = 1;
	wake_up(&sp->msi_wait);

	return IRQ_HANDLED;
}

/* Test interrupt path by forcing a a software IRQ */
static int __devinit s2io_test_msi(struct s2io_nic *sp)
{
	struct pci_dev *pdev = sp->pdev;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	int err;
	u64 val64, saved64;

	err = request_irq(sp->entries[1].vector, s2io_test_intr, 0,
		sp->name, sp);
	if (err) {
		DBG_PRINT(ERR_DBG, "s2io: PCI %s: cannot assign irq %d\n",
			pci_name(pdev), pdev->irq);
		return err;
	}

	init_waitqueue_head(&sp->msi_wait);
	sp->msi_detected = 0;

	saved64 = val64 = readq(&bar0->scheduled_int_ctrl);
	val64 |= SCHED_INT_CTRL_ONE_SHOT;
	val64 |= SCHED_INT_CTRL_TIMER_EN;
	val64 |= SCHED_INT_CTRL_INT2MSI(1);
	writeq(val64, &bar0->scheduled_int_ctrl);

	wait_event_timeout(sp->msi_wait, sp->msi_detected, HZ/10);

	if (!sp->msi_detected) {
		/* MSI(X) test failed, go back to INTx mode */
		DBG_PRINT(ERR_DBG, "s2io: PCI %s: No interrupt was generated "
			"using MSI(X) during test\n", pci_name(pdev));

		err = -EOPNOTSUPP;
	}

	free_irq(sp->entries[1].vector, sp);

	writeq(saved64, &bar0->scheduled_int_ctrl);

	return err;
}
#endif

#ifdef CONFIG_PCI_MSI
static void do_rem_msix_isr(struct s2io_nic *sp)
{
	int i;
	u16 msi_control;

	for (i = 0; i < sp->num_entries; i++) {
		if (sp->s2io_entries[i].in_use ==
			MSIX_REGISTERED_SUCCESS) {
			int vector = sp->entries[i].vector;
			void *arg = sp->s2io_entries[i].arg;
			free_irq(vector, arg);
		}
	}

	kfree(sp->entries);
	kfree(sp->s2io_entries);
	sp->entries = NULL;
	sp->s2io_entries = NULL;

	pci_read_config_word(sp->pdev, 0x42, &msi_control);
	msi_control &= 0xFFFE; /* Disable MSI */
	pci_write_config_word(sp->pdev, 0x42, msi_control);

	pci_disable_msix(sp->pdev);
}
#endif

static void do_rem_inta_isr(struct s2io_nic *sp)
{
	struct net_device *dev = sp->dev;
	/* Waiting till all Interrupt handlers are complete */
	free_irq(sp->pdev->irq, dev);
}

/* ********************************************************* *
 * Functions defined below concern the OS part of the driver *
 * ********************************************************* */

/**
 *  s2io_open - open entry point of the driver
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function is the open entry point of the driver. It mainly calls a
 *  function to allocate Rx buffers and inserts them into the buffer
 *  descriptors and then enables the Rx part of the NIC.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *   file on failure.
 */

static int s2io_open(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	int err = 0;

#ifdef CONFIG_PCI_MSI
	struct swDbgStat *stats = sp->sw_dbg_stat;
#endif

	/*
	 * Make sure you have link off by default every time
	 * Nic is initialized
	 */
	netif_carrier_off(dev);
	sp->last_link_state = 0;

#ifdef __VMKLNX__
	clear_bit (__S2IO_STATE_RESET_CARD, &(sp->state));
	clear_bit (__S2IO_STATE_LINK_RESET_TASK, &(sp->state));
#endif

	/* Initialize H/W and enable interrupts */
	err = s2io_card_up(sp);
	if (err) {
		DBG_PRINT(ERR_DBG, "%s: H/W initialization failed\n",
			  dev->name);
		goto hw_init_failed;
	}

	if (do_s2io_prog_unicast(dev, dev->dev_addr) == FAILURE) {
		DBG_PRINT(ERR_DBG, "Set Mac Address Failed\n");
		s2io_card_down(sp);
		err = -ENODEV;
		goto hw_init_failed;
	}

	s2io_start_all_tx_queue(sp);

	return 0;

hw_init_failed:
#ifdef CONFIG_PCI_MSI
	if (sp->config.intr_type == MSI_X) {
		if (sp->entries) {
			kfree(sp->entries);
			stats->mem_freed +=
				(sp->num_entries * sizeof(struct msix_entry));
		}
		if (sp->s2io_entries) {
			kfree(sp->s2io_entries);
			stats->mem_freed +=
				(sp->num_entries *
				sizeof(struct s2io_msix_entry));
		}
	}
#endif
	return err;
}

/**
 *  s2io_close -close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver. It needs to undo exactly
 *  whatever was done by the open entry point,thus it's usually referred to
 *  as the close function.Among other things this function mainly stops the
 *  Rx side of the NIC and frees all the Rx buffers in the Rx rings.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */

static int s2io_close(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;

	/* Return if the device is already closed 		*
	*  Can happen when s2io_card_up failed in change_mtu 	*
	*/
#if !defined (__VMKLNX__)
	if (!is_s2io_card_up(sp))
		return 0;
#else /* defined (__VMKLNX__) */
	int wait_count=0;

	// wait for any previous reset to finish
	while (test_and_set_bit (__S2IO_STATE_RESET_CARD, &(sp->state))) {
		msleep(50);
		wait_count++;
		if (wait_count > 400) {
			VMK_ASSERT(0);
			break;
		}
	}

	// wait for any previous reset to finish
	wait_count=0;
	while (test_and_set_bit (__S2IO_STATE_LINK_RESET_TASK, &(sp->state))) {
		msleep(50);
		wait_count++;
		if (wait_count > 400) {
			VMK_ASSERT(0);
			break;
		}
	}
#endif /* !defined (__VMKLNX__) */

	s2io_stop_all_tx_queue(sp);

	/* Reset card, free Tx and Rx buffers. */
#if !defined (__VMKLNX__)
	s2io_card_down(sp);
#else /* defined (__VMKLNX__) */
	if (!is_s2io_card_up(sp))
		return 0;
	del_timer_sync(&sp->alarm_timer);
	s2io_card_down_complete (sp, 1);
#endif /* !defined (__VMKLNX__) */

	return 0;
}

/**
 *  s2io_xmit - Tx entry point of te driver
 *  @skb : the socket buffer containing the Tx data.
 *  @dev : device pointer.
 *  Description :
 *  This function is the Tx entry point of the driver. S2IO NIC supports
 *  certain protocol assist features on Tx side, namely  CSO, S/G, LSO.
 *  NOTE: when device cant queue the pkt,just the trans_start variable will
 *  not be upadted.
 *  Return value:
 *  0 on success & 1 on failure.
 */
static int s2io_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u16 frg_cnt, frg_len, queue_len, put_off, get_off;
	u64 val64;
	u32 val32;
	struct TxD *txdp;
	struct TxFIFO_element __iomem *tx_fifo;
	unsigned long flags = 0;
	u16 vlan_tag = 0;
	struct mac_info *mac_control;
	struct fifo_info *fifo = NULL;
	int txd_cnt = 0;
	int offload_type;
	int enable_per_list_interrupt = 0;
	int do_spin_lock = 1;
	struct swDbgStat *stats = sp->sw_dbg_stat;
	u16 counter = sp->other_fifo_idx;

	mac_control = &sp->mac_control;

	DBG_PRINT(TX_DBG, "%s: In Neterion Tx routine\n", dev->name);

	/* A buffer with no data will be dropped */
	if (unlikely(skb->len <= 0)) {
		DBG_PRINT(TX_DBG, "%s:Buffer has no data..\n", dev->name);
		dev_kfree_skb_any(skb);
		return 0;
	}

	/* In debug mode, return if a serious error had occured */
	if (unlikely(1 == sp->serious_err) && unlikely(0 == sp->exec_mode)) {
		dev_kfree_skb_any(skb);
		return 0;
	}

	if (unlikely(!is_s2io_card_up(sp))) {
		DBG_PRINT(TX_DBG, "%s: Card going down for reset\n",
			  dev->name);
		dev_kfree_skb_any(skb);
		return 0;
	}

	if (sp->vlgrp && vlan_tx_tag_present(skb))
		/* Get Fifo number to Transmit based on vlan priority */
		vlan_tag = vlan_tx_tag_get(skb);

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	VMK_ASSERT(skb->queue_mapping < sp->config.tx_fifo_num);
	counter = skb->queue_mapping;
#else /* defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__) */
	if (sp->config.tx_steering_type == TX_STEERING_DEFAULT) {
		if (skb->protocol == htons(ETH_P_IP)) {
			struct iphdr *ip;
			struct tcphdr *th;
			ip = ip_hdr(skb);
			if ((ip->frag_off & htons(IP_OFFSET|IP_MF)) == 0) {
				th = (struct tcphdr *)(((unsigned char *)ip) +
						ip->ihl*4);

				if (ip->protocol == IPPROTO_TCP) {
					queue_len = sp->total_tcp_fifos;
					counter = (ntohs(th->source) + ntohs(th->dest)) &
					    sp->fifo_selector[queue_len - 1];
					if (counter >= queue_len)
						counter = queue_len - 1;
				} else if (ip->protocol == IPPROTO_UDP) {
					queue_len = sp->total_udp_fifos;
					counter = (ntohs(th->source) + ntohs(th->dest)) &
					    sp->fifo_selector[queue_len - 1];
					if (counter >= queue_len)
						counter = queue_len - 1;
					counter += sp->udp_fifo_idx;
					if (skb->len > 1024)
						enable_per_list_interrupt = 1;
#ifdef NETIF_F_LLTX
					do_spin_lock = 0;
#endif
				}
			}
		}
	} else if (sp->config.tx_steering_type == TX_PRIORITY_STEERING) {
		counter = s2io_get_tx_priority(skb, vlan_tag);
		counter = sp->fifo_mapping[counter];
	}
#endif /* defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__) */

	fifo = &mac_control->fifos[counter];

	if (do_spin_lock)
		spin_lock_irqsave(&fifo->tx_lock, flags);
	else {
		if (unlikely(!spin_trylock_irqsave(&fifo->tx_lock, flags)))
			return NETDEV_TX_LOCKED;
	}

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (sp->config.multiq) {
		if (__netif_subqueue_stopped(dev, fifo->fifo_no)) {
			spin_unlock_irqrestore(&fifo->tx_lock, flags);
			return NETDEV_TX_BUSY;
		}
	} else
#endif
	if (unlikely(fifo->queue_state == FIFO_QUEUE_STOP)) {
		if (netif_queue_stopped(dev)) {
			spin_unlock_irqrestore(&fifo->tx_lock, flags);
			return NETDEV_TX_BUSY;
		}
	}
	put_off = (u16) fifo->tx_curr_put_info.offset;
	get_off = (u16) fifo->tx_curr_get_info.offset;
	txdp = (struct TxD *) fifo->list_info[put_off].list_virt_addr;

	queue_len = fifo->tx_curr_put_info.fifo_len + 1;
	/* Avoid "put" pointer going beyond "get" pointer */
	if (txdp->Host_Control ||
		   ((put_off+1) == queue_len ? 0 : (put_off+1)) == get_off) {
		DBG_PRINT(TX_DBG, "Error in xmit, No free TXDs.\n");
		s2io_stop_tx_queue(sp, fifo->fifo_no);

		spin_unlock_irqrestore(&fifo->tx_lock, flags);
		dev_kfree_skb(skb);
		return 0;
	}

	offload_type = s2io_offload_type(skb);

	/* only txd0 Buffer pointer, Buffer length changes depending upon
	 * whether amd_fix is enabled, or UFO is enabled. Rest all bits of
	 * txd0 will remain same so lets set them in the beginning itself
	 * before we start processing the txds so that we can reduce number of
	 * if conditions
	 */
#ifdef NETIF_F_TSO
	if (offload_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6)) {
		txdp->Control_1 |= TXD_TCP_LSO_EN;
		txdp->Control_1 |= TXD_TCP_LSO_MSS(s2io_tcp_mss(skb));
	}
#endif
	if (skb->ip_summed == CHECKSUM_PARTIAL)
		txdp->Control_2 |=
			(TXD_TX_CKO_IPV4_EN | TXD_TX_CKO_TCP_EN |
			TXD_TX_CKO_UDP_EN);

	txdp->Control_1 |= TXD_GATHER_CODE_FIRST;
	txdp->Control_1 |= TXD_LIST_OWN_XENA;
	txdp->Control_2 |= TXD_INT_NUMBER(fifo->fifo_no);
	if (enable_per_list_interrupt)
		if (put_off & (queue_len >> 5))
			txdp->Control_2 |= TXD_INT_TYPE_PER_LIST;
	if (vlan_tag) {
		txdp->Control_2 |= TXD_VLAN_ENABLE;
		txdp->Control_2 |= TXD_VLAN_TAG(vlan_tag);
	}
	frg_len = skb->len - skb->data_len;

#ifdef NETIF_F_UFO
	if (offload_type == SKB_GSO_UDP) {
		int ufo_size;

		ufo_size = s2io_udp_mss(skb);
		ufo_size &= ~7;
		txdp->Control_1 |= TXD_UFO_EN;
		txdp->Control_1 |= TXD_UFO_MSS(ufo_size);
		txdp->Control_1 |= TXD_BUFFER0_SIZE(8);
#ifdef __BIG_ENDIAN
		fifo->ufo_in_band_v[put_off] =
				(u64)skb_shinfo(skb)->ip6_frag_id;
#else
		fifo->ufo_in_band_v[put_off] =
				(u64)skb_shinfo(skb)->ip6_frag_id << 32;
#endif
		txdp->Host_Control = (unsigned long)fifo->ufo_in_band_v;
		txdp->Buffer_Pointer = pci_map_single(sp->pdev,
					fifo->ufo_in_band_v,
					sizeof(u64), PCI_DMA_TODEVICE);
		if ((txdp->Buffer_Pointer == 0) ||
			(txdp->Buffer_Pointer == DMA_ERROR_CODE))
				goto pci_map_failed;
		txdp++;
	}
#endif

	txdp->Buffer_Pointer = pci_map_single
		(sp->pdev, skb->data, frg_len, PCI_DMA_TODEVICE);

	if ((txdp->Buffer_Pointer == 0) ||
		(txdp->Buffer_Pointer == DMA_ERROR_CODE))
			goto pci_map_failed;

	txdp->Host_Control = (unsigned long) skb;
	txdp->Control_1 |= TXD_BUFFER0_SIZE(frg_len);
	if (offload_type == SKB_GSO_UDP)
		txdp->Control_1 |= TXD_UFO_EN;

	frg_cnt = skb_shinfo(skb)->nr_frags;
	/* For fragmented SKB. */
	for (counter = 0; counter < frg_cnt; counter++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[counter];
		/* A '0' length fragment will be ignored */
		if (!frag->size)
			continue;
		txdp++;
		txdp->Buffer_Pointer = (u64) pci_map_page
			(sp->pdev, frag->page, frag->page_offset,
			frag->size, PCI_DMA_TODEVICE);
		txdp->Control_1 = TXD_BUFFER0_SIZE(frag->size);
		if (offload_type == SKB_GSO_UDP)
			txdp->Control_1 |= TXD_UFO_EN;
	}
	txdp->Control_1 |= TXD_GATHER_CODE_LAST;

	if (offload_type == SKB_GSO_UDP)
		frg_cnt++; /* as Txd0 was used for inband header */

	tx_fifo = mac_control->tx_FIFO_start[fifo->fifo_no];
	val64 = fifo->list_info[put_off].list_phy_addr;
	writeq(val64, &tx_fifo->TxDL_Pointer);

	wmb();

	val64 = (TX_FIFO_LAST_TXD_NUM(txd_cnt + frg_cnt) | TX_FIFO_FIRST_LIST |
		TX_FIFO_LAST_LIST);
	if (offload_type)
		val64 |= TX_FIFO_SPECIAL_FUNC;

	writeq(val64, &tx_fifo->List_Control);

	val32 = readl(&bar0->general_int_status);
	/* if the returned value is all F's free skb */
	if (unlikely(val32 == S2IO_32_BIT_MINUS_ONE)) {
		s2io_stop_tx_queue(sp, fifo->fifo_no);

		txdp = fifo->list_info[put_off].list_virt_addr;
		skb = s2io_txdl_getskb(fifo, txdp, put_off);
		spin_unlock_irqrestore(&fifo->tx_lock, flags);
		if (skb == NULL) {
			DBG_PRINT(TX_DBG, "%s: Null skb ",
			__FUNCTION__);
			sp->mac_stat_info->skb_null_s2io_xmit_cnt++;
			return 0;
		}
		dev_kfree_skb(skb);
		return 0;
	}

	put_off++;
	if (put_off == fifo->tx_curr_put_info.fifo_len + 1)
		put_off = 0;
	fifo->tx_curr_put_info.offset = put_off;

	/* Avoid "put" pointer going beyond "get" pointer */
	if (((put_off+1) == queue_len ? 0 : (put_off+1)) == get_off) {
		fifo->tx_fifo_stat->fifo_full_cnt++;
		DBG_PRINT(TX_DBG,
			"No free TxDs for xmit, Put: 0x%x Get:0x%x\n",
			put_off, get_off);
		s2io_stop_tx_queue(sp, fifo->fifo_no);
	}
#if defined (__VMKLNX__)
	fifo->tx_fifo_stat->tx_pkt_cnt++;
#else
	/* Since these stats are not being retrieved, disabling it. */
	stats->tmac_frms_q[fifo->fifo_no]++;
#endif /* defined (__VMKLNX__) */
	sp->sw_dbg_stat->mem_allocated += skb->truesize;
	dev->trans_start = jiffies;

	spin_unlock_irqrestore(&fifo->tx_lock, flags);

	if(sp->config.intr_type == MSI_X)
		tx_intr_handler(fifo);

	return 0;
pci_map_failed:
	stats->pci_map_fail_cnt++;
	s2io_stop_tx_queue(sp, fifo->fifo_no);
	stats->mem_freed += skb->truesize;
	spin_unlock_irqrestore(&fifo->tx_lock, flags);
	dev_kfree_skb(skb);
	return 0;
}

/**
 * s2io_dtx_write - Function to write in to DTX registers
 * @mmd_type : always 5
 * @addr     : address value
 * @value    : data value
 * @dev      : pointer to net_device structure
 * Description:
 * This function is used to write values to the DTE registers
 * NONE
 */
static void s2io_dtx_write(u32 mmd_type, u64 addr, u16 value, struct net_device *dev)
{
	u64 val64 = 0x0;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	
	/* address transaction */
	val64 = MDIO_MMD_INDX_ADDR(addr)
		| MDIO_MMD_DEV_ADDR(mmd_type)
		| MDIO_MMS_PRT_ADDR(0x15);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	udelay(100);
 
	/*Data transaction */
	val64 = 0x0;
	val64 = MDIO_MMD_INDX_ADDR(addr)
		| MDIO_MMD_DEV_ADDR(mmd_type)
		| MDIO_MMS_PRT_ADDR(0x15)
		| MDIO_MDIO_DATA(value)
		| MDIO_OP(MDIO_OP_WRITE_TRANS);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	udelay(100);

	val64 = 0x0;
	val64 = MDIO_MMD_INDX_ADDR(addr)
		| MDIO_MMD_DEV_ADDR(mmd_type)
		| MDIO_MMS_PRT_ADDR(0x15)
		| MDIO_OP(MDIO_OP_READ_TRANS);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	SPECIAL_REG_WRITE(val64, &bar0->dtx_control, UF); 
	udelay(100);
}

 /**
 *  s2io_chk_dte_rx_local_fault - Function to detect dte rx local fault
 *  @dev     : n/w device instance
 *  Description:
 *  This function checks the occurrence of a dtx receive local fault
 *  in the absence of a rx local fault in the pcs (transponder) and will
 *  issue a dte reset if in this state. Since there is a remote
 *  possibility that the fault can persist after the dte reset, this
 *  function needs to be called from a periodic timer thread. If this function is
 *  invoked due to a link state change (link down) interrupt, and if the
 *  error persists after the dte reset, the link down interrupt will not
 *  fire again as the driver's link handling masks the link down
 *  interrupt expecting a link up (link change) interrupt, causing the adapter to
 *  remain in a persistant link down state.
 */
#define MDIO_DTE_STATUS_RECEIVE_FAULT         S2BIT(53)
#define MDIO_PCS_STATUS_RECEIVE_FAULT         S2BIT(53)
#define MDIO_DTE_CONTROL1_RESET               S2BIT(48)

static void s2io_chk_dte_rx_local_fault(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;

	/*
	 * Note: For mdio and dte reads, first read clears the history - the
	 * bit that was latched long ago. Second read checks if a new bit
	 * was latched and is persisting.
	 */

	val64 = readq(&bar0->adapter_status);
	if (val64 & ADAPTER_STATUS_RMAC_LOCAL_FAULT) {
		u64 dte_val, pcs_val;

		/* dte fault condition on receive path*/
		dte_val = s2io_dtx_read(5, 8, dev);
		dte_val = s2io_dtx_read(5, 8, dev);
		
		/* pcs no-fault condition on receive path*/

		pcs_val = s2io_mdio_read(3, 8, dev);
		pcs_val = s2io_mdio_read(3, 8, dev);

		if ((dte_val & MDIO_DTE_STATUS_RECEIVE_FAULT) &&
				!(pcs_val & MDIO_PCS_STATUS_RECEIVE_FAULT)) {
			u16 val16;
			/* dte control1 reset*/
			val64 = s2io_dtx_read(5, 0, dev);
			val64 = s2io_dtx_read(5, 0, dev);
			
			val64 |= MDIO_DTE_CONTROL1_RESET;
			val16 = *((u16 *)&val64);
			s2io_dtx_write(5, 0, val16, dev);

			sp->mac_stat_info->dte_reset_cnt++;
		}
	}
	return;
}
 
static void
poll_queue_stuck(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	struct ring_info *ring;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = 0, orig_thresh_q0q3, orig_thresh_q4q7, mod_thresh_q0q3, mod_thresh_q4q7;
	int i;

	/*
		Force the HIGH threshold UP_CROSSED event to be re-evaluated by
	temporarily moving the threshold to a value that can't be up-crossed.
	This fakes out the hardware such that it thinks the amount of queued
	data is less than the threshold.  When the threshold is restored to its
	original value, if the amount of queued data is above the threshold then
	the UP_CROSSED event will latch in the register.
	*/
	for (i=0; i < MAX_RX_RINGS; i++)
		val64 |= RMAC_HIGH_UP_CROSSED_Qn(i);
	writeq(val64, &bar0->rmac_pthresh_cross);

	orig_thresh_q0q3 = readq(&bar0->mc_pause_thresh_q0q3);
	val64 = orig_thresh_q0q3 | 0x00FF00FF00FF00FF;
	writeq(val64, &bar0->mc_pause_thresh_q0q3);
	val64 = readq(&bar0->mc_pause_thresh_q0q3);
	writeq(orig_thresh_q0q3, &bar0->mc_pause_thresh_q0q3);

	orig_thresh_q4q7 = readq(&bar0->mc_pause_thresh_q4q7);
	val64 = orig_thresh_q4q7 | 0x00FF00FF00FF00FF;
	writeq(val64, &bar0->mc_pause_thresh_q4q7);
	val64 = readq(&bar0->mc_pause_thresh_q0q3);
	writeq(orig_thresh_q4q7, &bar0->mc_pause_thresh_q4q7);

	/*
		Force the LOW threshold DOWN_CROSSED event to be re-evaluated by
		temporarily moving the threshold to a value that can't be down-crossed.
		This fakes out the hardware such that it thinks the amount of queued
		data is more than the threshold.  When the threshold is restored to its
		original value, if the amount of queued data is below the threshold then
		the DOWN_CROSSED event will latch in the register.
	*/

	/* set mc_pause_thresh_q0q3, mc_pause_thresh_q4q7 LOW_THR fields to
	the same value as their corresponding HIGH_THR fields */
	mod_thresh_q0q3 = orig_thresh_q0q3 & 0x00FF00FF00FF00FF;
	mod_thresh_q0q3 |= (mod_thresh_q0q3 << 8);
	mod_thresh_q4q7 = orig_thresh_q4q7 & 0x00FF00FF00FF00FF;
	mod_thresh_q4q7 |= (mod_thresh_q4q7 << 8);

	/* Ensures that we know where we're starting from before
		we set the LOW_THR to 0x00 */
	writeq(mod_thresh_q0q3, &bar0->mc_pause_thresh_q0q3);
	writeq(mod_thresh_q4q7, &bar0->mc_pause_thresh_q4q7);

	val64 = 0;
	for (i=0, val64=0; i < MAX_RX_RINGS; i++)
		val64 |= RMAC_LOW_DOWN_CROSSED_Qn(i);
	writeq(val64, &bar0->rmac_pthresh_cross);

	val64 = mod_thresh_q0q3 & 0x00FF00FF00FF00FF;
	writeq(val64, &bar0->mc_pause_thresh_q0q3);
	val64 = readq(&bar0->mc_pause_thresh_q0q3);
	writeq(mod_thresh_q0q3, &bar0->mc_pause_thresh_q0q3);

	val64 = mod_thresh_q4q7 & 0x00FF00FF00FF00FF;
	writeq(val64, &bar0->mc_pause_thresh_q4q7);
	val64 = readq(&bar0->mc_pause_thresh_q0q3);
	writeq(mod_thresh_q4q7, &bar0->mc_pause_thresh_q4q7);

	/* Read the up_crossed/down_crossed registers */
	val64 = readq(&bar0->rmac_pthresh_cross);

	/* See if any of the queues are stuck */
	for (i = 0; i < sp->config.rx_ring_num; i++) {
		ring = &sp->mac_control.rings[i];
		if ((val64 & RMAC_HIGH_UP_CROSSED_Qn(i)) &&
			!(val64 & RMAC_LOW_DOWN_CROSSED_Qn(i))) {
			if (++ring->queue_pause_cnt >=
					PAUSE_STUCK_THRESHOLD)
			{
				ring->queue_pause_cnt = 0;
				/* No errors if packets received */
				if (ring->rx_packets ==
						ring->prev_rx_packets)
					goto reset;
			}
		} else
			ring->queue_pause_cnt = 0;

		ring->prev_rx_packets = ring->rx_packets;
	}

	return;
reset:
	if (sp->exec_mode) {
		sp->mac_stat_info->rx_stuck_cnt++;
		if (sp->config.rst_q_stuck) {
			s2io_stop_all_tx_queue(sp);
#if defined (__VMKLNX__)
			printk ("%s: Scheduling RESET task for q_stuck %s\n", __FUNCTION__, dev->name);
#endif
			schedule_work(&sp->rst_timer_task);
		}
	} else
		sp->serious_err = 1;
}

static void
s2io_alarm_handle(unsigned long data)
{
	struct s2io_nic *sp = (struct s2io_nic *)data;
	struct net_device *dev = sp->dev;

	if (is_s2io_card_up(sp)) {
		if (++sp->chk_device_error_count >= MAX_DEVICE_CHECK_COUNT) {
			s2io_handle_errors(dev);
			sp->chk_device_error_count = 0;
		}

		if ((sp->device_type == XFRAME_II_DEVICE)
				&& (sp->device_sub_type == XFRAME_E_DEVICE)) {

			if (++sp->chk_rx_queue_count >=
					MAX_RX_QUEUE_CHECK_COUNT) {
				poll_queue_stuck(dev);
				sp->chk_rx_queue_count = 0;
			}
		}

		if (++sp->chk_dte_count >= MAX_DTE_CHECK_COUNT) {
			s2io_chk_dte_rx_local_fault(dev);
			sp->chk_dte_count = 0;
		}
	}

	mod_timer(&sp->alarm_timer, jiffies + HZ/10);
}


/**
 *  dynamic_rti - Modifies receive traffic interrupt scheme during run time
 *  @ring: pointer to receive ring structure
 *  Description: The function configures receive traffic interrupts
 *  Return Value: Nothing
 */

void dynamic_rti(struct ring_info *ring)
{
	struct s2io_nic *nic = ring->nic;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64;

	val64 = RTI_DATA1_MEM_RX_TIMER_VAL(ring->rx_timer_val);

	val64 |= RTI_DATA1_MEM_RX_URNG_A(ring->urange_a) |
		RTI_DATA1_MEM_RX_URNG_B(ring->urange_b) |
		RTI_DATA1_MEM_RX_URNG_C(ring->urange_c) |
		RTI_DATA1_MEM_RX_TIMER_AC_EN;
	writeq(val64, &bar0->rti_data1_mem);

	val64 = RTI_DATA2_MEM_RX_UFC_A(ring->ufc_a) |
		RTI_DATA2_MEM_RX_UFC_B(ring->ufc_b)|
		RTI_DATA2_MEM_RX_UFC_C(ring->ufc_c)|
		RTI_DATA2_MEM_RX_UFC_D(ring->ufc_d);
	writeq(val64, &bar0->rti_data2_mem);

	val64 = RTI_CMD_MEM_WE | RTI_CMD_MEM_STROBE_NEW_CMD
			| RTI_CMD_MEM_OFFSET(ring->ring_no);
	writeq(val64, &bar0->rti_command_mem);

	return;
}

/**
 *  adaptive_coalesce_rx_interrupts - Changes the interrupt coalescing
 *  if the interrupts are not within a range
 *  @ring: pointer to receive ring structure
 *  Description: The function increases of decreases the packet counts within
 *  the ranges of traffic utilization, if the interrupts due to this ring are
 *  not within a fixed range.
 *  Return Value: Nothing
 */
static void adaptive_coalesce_rx_interrupts(struct ring_info *ring)
{
	int cnt = 0;

	ring->interrupt_count++;
	if (jiffies > ring->jiffies + HZ/100) {
		ring->jiffies = jiffies;
		if (ring->interrupt_count > MAX_INTERRUPT_COUNT) {
			if (ring->ufc_a < MAX_RX_UFC_A) {
				ring->ufc_a++;
				ring->rx_timer_val >>= 5;
				if (ring->rx_timer_val <= 0)
					ring->rx_timer_val = MIN_RX_TIMER_VAL;
				cnt++;
			}
		} else if (ring->interrupt_count < MAX_INTERRUPT_COUNT) {
			if (ring->ufc_a > MIN_RX_UFC_A) {
				ring->ufc_a--;
				ring->rx_timer_val <<= 5;
				if (ring->rx_timer_val >
					ring->rx_timer_val_saved)
					ring->rx_timer_val =
						ring->rx_timer_val_saved;
				cnt++;
			}
		}

		if (cnt)
			dynamic_rti(ring);

		ring->interrupt_count = 0;
	}
}


#ifdef CONFIG_PCI_MSI
static irqreturn_t s2io_msix_ring_handle(int irq, void *dev_id)
{
	struct ring_info *ring = (struct ring_info *)dev_id;
	struct s2io_nic *sp = ring->nic;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	struct net_device *dev = sp->dev;

	if (IRQ_HANDLED == S2IO_RING_IN_USE(ring))
		return IRQ_HANDLED;

	/*Increment the per ring msix interrupt counter*/
	ring->msix_intr_cnt++;

	/* In debug mode, return if a serious error has occured */
	if (sp->serious_err && (0 == sp->exec_mode)) {
		S2IO_RING_DONE(ring);
		return IRQ_NONE;
	}

	if (unlikely(!is_s2io_card_up(sp))) {
		S2IO_RING_DONE(ring);
		return IRQ_NONE;
	}

	if (ring->aggr_ack)
		adaptive_coalesce_rx_interrupts(ring);

	if (ring->config_napi) {
		u8 *addr = NULL, val8 = 0;
		addr = (u8 *)&bar0->xmsi_mask_reg;
		addr += (7 - ring->ring_no);
		val8 = (ring->ring_no == 0) ? 0x7f : 0xff;
		writeb(val8, addr);
		val8 = readb(addr);
		netif_rx_schedule(dev, &ring->napi);
	} else {
		rx_intr_handler(ring, 0);
		s2io_chk_rx_buffers(ring);
	}

	S2IO_RING_DONE(ring);
	return IRQ_HANDLED;
}

static irqreturn_t s2io_msix_fifo_handle(int irq, void *dev_id)
{
	int i;
	struct fifo_info *fifos = (struct fifo_info *)dev_id;
	struct s2io_nic *sp = fifos->nic;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	struct config_param *config  = &sp->config;
	u64 reason;

	if (IRQ_HANDLED == S2IO_FIFO_IN_USE(sp))
		return IRQ_HANDLED;

	/*Increment the fifo interrupt counter*/
	sp->tx_intr_cnt++;

	/* In debug mode, return if a serious error has occured */
	if (sp->serious_err && (0 == sp->exec_mode)) {
		S2IO_FIFO_DONE(sp);
		return IRQ_NONE;
	}

	if (unlikely(!is_s2io_card_up(sp))) {
		S2IO_FIFO_DONE(sp);
		return IRQ_NONE;
	}

	reason = readq(&bar0->general_int_status);
	if (unlikely(reason == S2IO_MINUS_ONE)) {
		/* Nothing much can be done. Get out */
		return IRQ_HANDLED;
	}

	if (reason & (GEN_INTR_TXPIC | GEN_INTR_TXTRAFFIC)) {
		writeq(S2IO_MINUS_ONE, &bar0->general_int_mask);

		if (reason & GEN_INTR_TXPIC)
			s2io_txpic_intr_handle(sp);

		if (reason & GEN_INTR_TXTRAFFIC)
			writeq(S2IO_MINUS_ONE, &bar0->tx_traffic_int);

		for (i = 0; i < config->tx_fifo_num; i++)
			tx_intr_handler(&fifos[i]);

		writeq(sp->general_int_mask, &bar0->general_int_mask);
		readl(&bar0->general_int_status);

		S2IO_FIFO_DONE(sp);
		return IRQ_HANDLED;
	}

	/* The interrupt was not raised by us */
	S2IO_FIFO_DONE(sp);
	return IRQ_NONE;
}

#endif
static void s2io_txpic_intr_handle(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;

	val64 = readq(&bar0->pic_int_status);
	if (val64 & PIC_INT_GPIO) {
		val64 = readq(&bar0->gpio_int_reg);
		if ((val64 & GPIO_INT_REG_LINK_DOWN) &&
			(val64 & GPIO_INT_REG_LINK_UP)) {
			/*
			 * This is unstable state so clear both up/down
			 * interrupt and adapter to re-evaluate the link state.
			 */
			val64 |=  GPIO_INT_REG_LINK_DOWN;
			val64 |= GPIO_INT_REG_LINK_UP;
			writeq(val64, &bar0->gpio_int_reg);
			val64 = readq(&bar0->gpio_int_mask);
			val64 &= ~(GPIO_INT_MASK_LINK_UP |
				   GPIO_INT_MASK_LINK_DOWN);
			writeq(val64, &bar0->gpio_int_mask);
		} else if (val64 & GPIO_INT_REG_LINK_UP) {
			/* Enable Adapter */
			val64 = readq(&bar0->adapter_control);
			val64 |= ADAPTER_CNTL_EN;
			writeq(val64, &bar0->adapter_control);
			val64 |= ADAPTER_LED_ON;
			writeq(val64, &bar0->adapter_control);
			sp->device_enabled_once = TRUE;
			s2io_link(sp, LINK_UP);
			/*
			 * unmask link down interrupt and mask link-up
			 * intr
			 */
			val64 = readq(&bar0->gpio_int_mask);
			val64 &= ~GPIO_INT_MASK_LINK_DOWN;
			val64 |= GPIO_INT_MASK_LINK_UP;
			writeq(val64, &bar0->gpio_int_mask);

		} else if (val64 & GPIO_INT_REG_LINK_DOWN) {
			s2io_link(sp, LINK_DOWN);
			/* Link is down so unmask link up interrupt */
			val64 = readq(&bar0->gpio_int_mask);
			val64 &= ~GPIO_INT_MASK_LINK_UP;
			val64 |= GPIO_INT_MASK_LINK_DOWN;
			writeq(val64, &bar0->gpio_int_mask);
			/* turn off LED */
			val64 = readq(&bar0->adapter_control);
			val64 = val64 & (~ADAPTER_LED_ON);
			writeq(val64, &bar0->adapter_control);
		}
	}
	readq(&bar0->gpio_int_mask);
}

 /**
 *  do_s2io_chk_alarm_bit - Check for alarm and incrment the counter
 *  @value: alarm bits
 *  @addr: address value
 *  @cnt: counter variable
 *  Description: Check for alarm and increment the counter
 *  Return Value:
 *  1 - if alarm bit set
 *  0 - if alarm bit is not set
 */
static int do_s2io_chk_alarm_bit(u64 value, void __iomem *addr,
		unsigned long long *cnt)
{
	u64 val64;
	val64 = readq(addr);
	if (val64 & value) {
		writeq(val64, addr);
		(*cnt)++;
		return 1;
	}
	return 0;
}

 /**
 *  s2io_handle_errors - Xframe error indication handler
 *  @nic: device private variable
 *  Description: Handle alarms such as loss of link, single or double  ECC
 *  errors, critical and serious errors.
 *  Return Value: NONE
 */
#if 1 || defined (__VMKLNX__)
static void s2io_handle_errors(void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 temp64 = 0, val64 = 0;
	int i = 0;
	struct xpakStat *stats = sp->xpak_stat;
	struct mac_info *mac_control;
	mac_control = &sp->mac_control;

	if (!is_s2io_card_up(sp))
		return;

	if (pci_channel_offline(sp->pdev))
		return;

	/* Handling the XPAK counters update */
	if (stats->xpak_timer_count < 72000) {
		/* waiting for an hour */
		stats->xpak_timer_count++;
	} else {
		s2io_updt_xpak_counter(dev);
		/* reset the count to zero */
		stats->xpak_timer_count = 0;
	}

	/* Handling link status change error Intr */
	if (s2io_link_fault_indication(sp) == MAC_RMAC_ERR_TIMER) {
		val64 = readq(&bar0->mac_rmac_err_reg);
		writeq(val64, &bar0->mac_rmac_err_reg);
		if (val64 & RMAC_LINK_STATE_CHANGE_INT)
			schedule_work(&sp->set_link_task);
	}

	/* In case of a serious error, the device will be Reset. */
	if (do_s2io_chk_alarm_bit(SERR_SOURCE_ANY, &bar0->serr_source,
				&sp->mac_stat_info->serious_err_cnt)) {
		printk ("%s: Invoking reset due to error case 1 for %s\n", __FUNCTION__, dev->name);
		goto reset;
	}

	/* Check for data parity error */
	if (do_s2io_chk_alarm_bit(GPIO_INT_REG_DP_ERR_INT, &bar0->gpio_int_reg,
				&sp->mac_stat_info->parity_err_cnt)) {
		printk ("%s: Invoking reset due to error case 2 for %s\n", __FUNCTION__, dev->name);
		goto reset;
	}

	val64 = readq(&bar0->gpio_int_reg);
	if (val64 & GPIO_INT_REG_DP_ERR_INT) {
		sp->mac_stat_info->parity_err_cnt++;
		printk ("%s: Invoking reset due to error case 3 for %s\n", __FUNCTION__, dev->name);
		goto reset;
	}

	/* Check for ring full counter */
	if (sp->device_type == XFRAME_II_DEVICE) {
		for (i = 0; i < sp->config.rx_ring_num; i++) {
			if (i < 4)
				val64 = readq(&bar0->ring_bump_counter1);
			else
				val64 = readq(&bar0->ring_bump_counter2);
			temp64 = (val64 & vBIT(0xFFFF, (i*16), 16));
			temp64 >>= 64 - ((i+1)*16);
			mac_control->rings[i].rx_ring_stat->ring_full_cnt +=
				temp64;
		}
	}


	val64 = readq(&bar0->txdma_int_status);
	/*check for pfc_err*/
	if (val64 & TXDMA_PFC_INT) {
		if (do_s2io_chk_alarm_bit(PFC_ECC_DB_ERR | PFC_SM_ERR_ALARM |
				PFC_MISC_0_ERR | PFC_MISC_1_ERR |
				PFC_PCIX_ERR, &bar0->pfc_err_reg,
				&sp->mac_stat_info->pfc_err_cnt)) {
			printk ("%s: Invoking reset due to error case 4 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(PFC_ECC_SG_ERR, &bar0->pfc_err_reg,
				&sp->mac_stat_info->pfc_err_cnt);
	}

	/*check for tda_err*/
	if (val64 & TXDMA_TDA_INT) {
		if (do_s2io_chk_alarm_bit(TDA_Fn_ECC_DB_ERR |
				TDA_SM0_ERR_ALARM |
				TDA_SM1_ERR_ALARM, &bar0->tda_err_reg,
				&sp->mac_stat_info->tda_err_cnt)) {
			printk ("%s: Invoking reset due to error case 5 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(TDA_Fn_ECC_SG_ERR | TDA_PCIX_ERR,
				&bar0->tda_err_reg,
				&sp->mac_stat_info->tda_err_cnt);
	}
	/*check for pcc_err*/
	if (val64 & TXDMA_PCC_INT) {
		val64 = readq(&bar0->pcc_err_reg);
		temp64 = PCC_TXB_ECC_DB_ERR
			| PCC_SM_ERR_ALARM | PCC_WR_ERR_ALARM
			| PCC_N_SERR | PCC_6_COF_OV_ERR
			| PCC_7_COF_OV_ERR | PCC_6_LSO_OV_ERR
			| PCC_7_LSO_OV_ERR | PCC_FB_ECC_DB_ERR
			| PCC_TXB_ECC_DB_ERR;
		if (val64 & temp64) {
			writeq(val64, &bar0->pcc_err_reg);
			sp->mac_stat_info->pcc_err_cnt++;
			if (val64 & PCC_N_SERR)
				sp->serious_err = 1;
			printk ("%s: Invoking reset due to error case 6 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(PCC_FB_ECC_SG_ERR | PCC_TXB_ECC_SG_ERR,
				&bar0->pcc_err_reg,
				&sp->mac_stat_info->pcc_err_cnt);
	}

	/*check for tti_err*/
	if (val64 & TXDMA_TTI_INT) {
		if (do_s2io_chk_alarm_bit(TTI_SM_ERR_ALARM, &bar0->tti_err_reg,
					&sp->mac_stat_info->tti_err_cnt)) {
			printk ("%s: Invoking reset due to error case 7 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(LSO6_SEND_OFLOW | LSO7_SEND_OFLOW,
				&bar0->lso_err_reg,
				&sp->mac_stat_info->tti_err_cnt);
	}

	/*check for lso_err*/
	if (val64 & TXDMA_LSO_INT) {
		if (do_s2io_chk_alarm_bit(LSO6_ABORT | LSO7_ABORT
				| LSO6_SM_ERR_ALARM | LSO7_SM_ERR_ALARM,
				&bar0->lso_err_reg,
				&sp->mac_stat_info->lso_err_cnt)) {
			printk ("%s: Invoking reset due to error case 8 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(LSO6_SEND_OFLOW | LSO7_SEND_OFLOW,
				&bar0->lso_err_reg,
				&sp->mac_stat_info->lso_err_cnt);
	}

	/*check for tpa_err*/
	if (val64 & TXDMA_TPA_INT) {
		if (do_s2io_chk_alarm_bit(TPA_SM_ERR_ALARM, &bar0->tpa_err_reg,
				&sp->mac_stat_info->tpa_err_cnt)) {
			printk ("%s: Invoking reset due to error case 9 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(TPA_TX_FRM_DROP, &bar0->tpa_err_reg,
				&sp->mac_stat_info->tpa_err_cnt);
	}

	/*check for sm_err*/
	if (val64 & TXDMA_SM_INT) {
		if (do_s2io_chk_alarm_bit(SM_SM_ERR_ALARM, &bar0->sm_err_reg,
			&sp->mac_stat_info->sm_err_cnt)) {
			printk ("%s: Invoking reset due to error case 10 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
	}

	val64 = readq(&bar0->mac_int_status);
	if (val64 & MAC_INT_STATUS_TMAC_INT) {
		if (do_s2io_chk_alarm_bit(TMAC_TX_BUF_OVRN | TMAC_TX_SM_ERR,
				&bar0->mac_tmac_err_reg,
				&sp->mac_stat_info->mac_tmac_err_cnt)) {
			printk ("%s: Invoking reset due to error case 11 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(TMAC_ECC_SG_ERR | TMAC_ECC_DB_ERR
				| TMAC_DESC_ECC_SG_ERR | TMAC_DESC_ECC_DB_ERR,
				&bar0->mac_tmac_err_reg,
				&sp->mac_stat_info->mac_tmac_err_cnt);

	}

	val64 = readq(&bar0->xgxs_int_status);
	if (val64 & XGXS_INT_STATUS_TXGXS) {
		if (do_s2io_chk_alarm_bit(TXGXS_ESTORE_UFLOW | TXGXS_TX_SM_ERR,
				&bar0->xgxs_txgxs_err_reg,
				&sp->mac_stat_info->xgxs_txgxs_err_cnt)) {
			printk ("%s: Invoking reset due to error case 12 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(TXGXS_ECC_SG_ERR | TXGXS_ECC_DB_ERR,
				&bar0->xgxs_txgxs_err_reg,
				&sp->mac_stat_info->xgxs_txgxs_err_cnt);
	}

	val64 = readq(&bar0->rxdma_int_status);
	if (val64 & RXDMA_INT_RC_INT_M) {
		if (do_s2io_chk_alarm_bit(RC_PRCn_ECC_DB_ERR |
				RC_FTC_ECC_DB_ERR | RC_PRCn_SM_ERR_ALARM |
				RC_FTC_SM_ERR_ALARM, &bar0->rc_err_reg,
				&sp->mac_stat_info->rc_err_cnt)) {
			printk ("%s: Invoking reset due to error case 13 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(RC_PRCn_ECC_SG_ERR | RC_FTC_ECC_SG_ERR
				| RC_RDA_FAIL_WR_Rn, &bar0->rc_err_reg,
				&sp->mac_stat_info->rc_err_cnt);
		if (do_s2io_chk_alarm_bit(PRC_PCI_AB_RD_Rn | PRC_PCI_AB_WR_Rn
				| PRC_PCI_AB_F_WR_Rn, &bar0->prc_pcix_err_reg,
				&sp->mac_stat_info->prc_pcix_err_cnt)) {
			printk ("%s: Invoking reset due to error case 14 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}

		do_s2io_chk_alarm_bit(PRC_PCI_DP_RD_Rn | PRC_PCI_DP_WR_Rn
				| PRC_PCI_DP_F_WR_Rn, &bar0->prc_pcix_err_reg,
				&sp->mac_stat_info->prc_pcix_err_cnt);
	}

	if (val64 & RXDMA_INT_RPA_INT_M) {
		if (do_s2io_chk_alarm_bit(RPA_SM_ERR_ALARM | RPA_CREDIT_ERR,
				&bar0->rpa_err_reg,
				&sp->mac_stat_info->rpa_err_cnt)) {
			printk ("%s: Invoking reset due to error case 15 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(RPA_ECC_SG_ERR | RPA_ECC_DB_ERR,
				&bar0->rpa_err_reg,
				&sp->mac_stat_info->rpa_err_cnt);
	}

	if (val64 & RXDMA_INT_RDA_INT_M) {
		if (do_s2io_chk_alarm_bit(RDA_RXDn_ECC_DB_ERR
				| RDA_FRM_ECC_DB_N_AERR | RDA_SM1_ERR_ALARM
				| RDA_SM0_ERR_ALARM | RDA_RXD_ECC_DB_SERR,
				&bar0->rda_err_reg,
				&sp->mac_stat_info->rda_err_cnt)) {
			printk ("%s: Invoking reset due to error case 16 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(RDA_RXDn_ECC_SG_ERR | RDA_FRM_ECC_SG_ERR
				| RDA_MISC_ERR | RDA_PCIX_ERR,
				&bar0->rda_err_reg,
				&sp->mac_stat_info->rda_err_cnt);
	}

	if (val64 & RXDMA_INT_RTI_INT_M) {
		if (do_s2io_chk_alarm_bit(RTI_SM_ERR_ALARM, &bar0->rti_err_reg,
				&sp->mac_stat_info->rti_err_cnt)) {
			printk ("%s: Invoking reset due to error case 17 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(RTI_ECC_SG_ERR | RTI_ECC_DB_ERR,
				&bar0->rti_err_reg,
				&sp->mac_stat_info->rti_err_cnt);
	}

	val64 = readq(&bar0->mac_int_status);
	if (val64 & MAC_INT_STATUS_RMAC_INT) {
		if (do_s2io_chk_alarm_bit(RMAC_RX_BUFF_OVRN | RMAC_RX_SM_ERR,
				&bar0->mac_rmac_err_reg,
				&sp->mac_stat_info->mac_rmac_err_cnt)) {
			printk ("%s: Invoking reset due to error case 18 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
		do_s2io_chk_alarm_bit(RMAC_UNUSED_INT|RMAC_SINGLE_ECC_ERR|
				RMAC_DOUBLE_ECC_ERR, &bar0->mac_rmac_err_reg,
				&sp->mac_stat_info->mac_rmac_err_cnt);
	}

	val64 = readq(&bar0->xgxs_int_status);
	if (val64 & XGXS_INT_STATUS_RXGXS) {
		if (do_s2io_chk_alarm_bit(RXGXS_ESTORE_OFLOW | RXGXS_RX_SM_ERR,
				&bar0->xgxs_rxgxs_err_reg,
				&sp->mac_stat_info->xgxs_rxgxs_err_cnt)) {
			printk ("%s: Invoking reset due to error case 19 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}
	}

	val64 = readq(&bar0->mc_int_status);
	if (val64 & MC_INT_STATUS_MC_INT) {
		if (do_s2io_chk_alarm_bit(MC_ERR_REG_SM_ERR, &bar0->mc_err_reg,
				&sp->mac_stat_info->mc_err_cnt)) {
			printk ("%s: Invoking reset due to error case 20 for %s\n", __FUNCTION__, dev->name);
			goto reset;
		}

		/* Handling Ecc errors */
		val64 = readq(&bar0->mc_err_reg);
		if (val64 & (MC_ERR_REG_ECC_ALL_SNG |
			MC_ERR_REG_ECC_ALL_DBL)) {
			writeq(val64, &bar0->mc_err_reg);
			if (val64 & MC_ERR_REG_ECC_ALL_DBL) {
				sp->mac_stat_info->double_ecc_errs++;
				if (sp->device_type != XFRAME_II_DEVICE) {
					/* Reset XframeI only if critical
					* error occured
					*/
					if (val64 &
						(MC_ERR_REG_MIRI_ECC_DB_ERR_0 |
						MC_ERR_REG_MIRI_ECC_DB_ERR_1)) {
						printk ("%s: Invoking reset due to error case 21 for %s\n", __FUNCTION__, dev->name);
						goto reset;
					}
					}
			} else
				sp->mac_stat_info->single_ecc_errs++;
		}
	}
	return;

reset:
	s2io_stop_all_tx_queue(sp);

	if (sp->exec_mode) {
		printk ("%s: Scheduling RESET task for %s\n", __FUNCTION__, dev->name);
		schedule_work(&sp->rst_timer_task);
		sp->mac_stat_info->soft_reset_cnt++;
	} else
		sp->serious_err = 1;
	return;
}
#endif /* 1 || defined (__VMKLNX__) */

/**
 *  s2io_isr - ISR handler of the device .
 *  @irq: the irq of the device.
 *  @dev_id: a void pointer to the dev structure of the NIC.
 *  @pt_regs: pointer to the registers pushed on the stack.
 *  Description:  This function is the ISR handler of the device. It
 *  identifies the reason for the interrupt and calls the relevant
 *  service routines. As a contongency measure, this ISR allocates the
 *  recv buffers, if their numbers are below the panic value which is
 *  presently set to 25% of the original number of rcv buffers allocated.
 *  Return value:
 *   IRQ_HANDLED: will be returned if IRQ was handled by this routine
 *   IRQ_NONE: will be returned if interrupt is not from our device
 */
static irqreturn_t s2io_isr(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	int i;
	u64 reason;
	struct mac_info *mac_control;
	struct config_param *config;

	/* Pretend we handled any irq's from a disconnected card */
	if (pci_channel_offline(sp->pdev))
		return IRQ_NONE;

	/* In debug mode, return if a serious error has occured */
	if (sp->serious_err && (0 == sp->exec_mode))
		return IRQ_NONE;

	if (unlikely(!is_s2io_card_up(sp)))
		return IRQ_NONE;

	mac_control = &sp->mac_control;
	config = &sp->config;

	/*
	 * Identify the cause for interrupt and call the appropriate
	 * interrupt handler. Causes for the interrupt could be;
	 * 1. Rx of packet.
	 * 2. Tx complete.
	 * 3. Link down.
	 * 4. Error in any functional blocks of the NIC.
	 */
	reason = readq(&bar0->general_int_status);

	if (unlikely(reason == S2IO_MINUS_ONE)) {
		/* Nothing much can be done. Get out */
		return IRQ_HANDLED;
	}

	if (reason & (GEN_INTR_RXTRAFFIC |
		GEN_INTR_TXTRAFFIC | GEN_INTR_TXPIC)) {
		writeq(S2IO_MINUS_ONE, &bar0->general_int_mask);

		if (sp->config.napi) {
			if (reason & GEN_INTR_RXTRAFFIC) {
				sp->rx_intr_cnt++;

				writeq(S2IO_MINUS_ONE, &bar0->rx_traffic_mask);
				writeq(S2IO_MINUS_ONE, &bar0->rx_traffic_int);
				readl(&bar0->rx_traffic_int);
				netif_rx_schedule(dev, &sp->napi);
			}
		} else {
			/*
			 * rx_traffic_int reg is an R1 register, writing all 1's
			 * will ensure that the actual interrupt causing bit
			 * gets cleared and hence a read can be avoided.
			 */
			if (reason & GEN_INTR_RXTRAFFIC) {
				sp->rx_intr_cnt++;
				writeq(S2IO_MINUS_ONE, &bar0->rx_traffic_int);
			}

			for (i = 0; i < config->rx_ring_num; i++)
				rx_intr_handler(&mac_control->rings[i], 0);
		}

		/*
		 * tx_traffic_int reg is an R1 register, writing all 1's
		 * will ensure that the actual interrupt causing bit get's
		 * cleared and hence a read can be avoided.
		 */
		if (reason & GEN_INTR_TXTRAFFIC) {
			sp->tx_intr_cnt++;
			writeq(S2IO_MINUS_ONE, &bar0->tx_traffic_int);
		}

		for (i = 0; i < config->tx_fifo_num; i++)
			tx_intr_handler(&mac_control->fifos[i]);

		if (reason & GEN_INTR_TXPIC)
			s2io_txpic_intr_handle(sp);

		/*
		 * Reallocate the buffers from the interrupt handler itself.
		 */
		if (!config->napi)
			for (i = 0; i < config->rx_ring_num; i++)
				s2io_chk_rx_buffers(&mac_control->rings[i]);

		writeq(sp->general_int_mask, &bar0->general_int_mask);
		readl(&bar0->general_int_status);
		return IRQ_HANDLED;
	} else if (unlikely(!reason)) {
		/* The interrupt was not raised by us */
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

/**
 * s2io_updt_stats -
 */
static void s2io_updt_stats(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	int cnt = 0;

	if (pci_channel_offline(sp->pdev))
		return;

	if (is_s2io_card_up(sp)) {
		/* Apprx 30us on a 133 MHz bus */
		val64 = SET_UPDT_CLICKS(10) |
			STAT_CFG_ONE_SHOT_EN | STAT_CFG_STAT_EN;
		writeq(val64, &bar0->stat_cfg);
		do {
			udelay(100);
			val64 = readq(&bar0->stat_cfg);
			if (!(val64 & S2BIT(0)))
				break;
			cnt++;
			if (cnt == 5)
				break; /* Updt failed */
		} while(1);
	}
}

/**
 *  s2io_get_stats - Updates the device statistics structure.
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function updates the device statistics structure in the s2io_nic
 *  structure and returns a pointer to the same.
 *  Return value:
 *  pointer to the updated net_device_stats structure.
 */

static struct net_device_stats *s2io_get_stats(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	struct mac_info *mac_control;
	struct stat_block *stats_info = sp->mac_control.stats_info;
	struct config_param *config;
	int i;

	mac_control = &sp->mac_control;
	config = &sp->config;

	/* Configure Stats for immediate updt */
	s2io_updt_stats(sp);

	sp->stats.tx_packets =
		(u64)le32_to_cpu(stats_info->tmac_frms_oflow) << 32  |
		le32_to_cpu(stats_info->tmac_frms);
	sp->stats.tx_errors =
		(u64)le32_to_cpu(stats_info->tmac_any_err_frms_oflow) << 32  |
		le32_to_cpu(stats_info->tmac_any_err_frms);
	sp->stats.rx_errors =
		le64_to_cpu(stats_info->rmac_drop_frms);
	sp->stats.multicast =
		(u64)le32_to_cpu(stats_info->rmac_vld_mcst_frms_oflow) << 32  |
		le32_to_cpu(stats_info->rmac_vld_mcst_frms);
	sp->stats.rx_length_errors =
		le64_to_cpu(stats_info->rmac_long_frms);

	/* collect per-ring rx_packets and rx_bytes */
	sp->stats.rx_packets = sp->stats.rx_bytes = 0;
	for (i = 0; i < config->rx_ring_num; i++) {
		sp->stats.rx_packets += mac_control->rings[i].rx_packets;
		sp->stats.rx_bytes += mac_control->rings[i].rx_bytes;
	}

	return (&sp->stats);
}

/**
 *  s2io_set_multicast - entry point for multicast address enable/disable.
 *  @dev : pointer to the device structure
 *  Description:
 *  This function is a driver entry point which gets called by the kernel
 *  whenever multicast addresses must be enabled/disabled. This also gets
 *  called to set/reset promiscuous mode. Depending on the deivce flag, we
 *  determine, if multicast address must be enabled or if promiscuous mode
 *  is to be disabled etc.
 *  Return value:
 *  void.
 */

static void s2io_set_multicast(struct net_device *dev)
{
	int i, j, prev_cnt;
	struct dev_mc_list *mclist;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = 0, multi_mac = 0x010203040506ULL, mask =
	    0xfeffffffffffULL;
	u64 dis_addr = S2IO_DISABLE_MAC_ENTRY, mac_addr = 0;
	void __iomem *add;
	struct config_param *config = &sp->config;

	if ((dev->flags & IFF_ALLMULTI) && (!sp->m_cast_flg)) {
		/*  Enable all Multicast addresses */
		writeq(RMAC_ADDR_DATA0_MEM_ADDR(multi_mac),
		       &bar0->rmac_addr_data0_mem);
		writeq(RMAC_ADDR_DATA1_MEM_MASK(mask),
		       &bar0->rmac_addr_data1_mem);
		val64 = RMAC_ADDR_CMD_MEM_WE |
		    RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
		    RMAC_ADDR_CMD_MEM_OFFSET(config->max_mc_addr - 1);
		writeq(val64, &bar0->rmac_addr_cmd_mem);
		/* Wait till command completes */
		wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
				RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
				S2IO_BIT_RESET);

		sp->m_cast_flg = 1;
		sp->all_multi_pos = config->max_mc_addr - 1;
	} else if ((dev->flags & IFF_ALLMULTI) && (sp->m_cast_flg)) {
		/*  Disable all Multicast addresses */
		writeq(RMAC_ADDR_DATA0_MEM_ADDR(dis_addr),
			&bar0->rmac_addr_data0_mem);
		writeq(RMAC_ADDR_DATA1_MEM_MASK(0x0),
			&bar0->rmac_addr_data1_mem);
		val64 = RMAC_ADDR_CMD_MEM_WE |
			RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
			RMAC_ADDR_CMD_MEM_OFFSET(sp->all_multi_pos);
		writeq(val64, &bar0->rmac_addr_cmd_mem);
		/* Wait till command completes */
		wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
				RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
				S2IO_BIT_RESET);

		sp->m_cast_flg = 0;
		sp->all_multi_pos = 0;
	}

	if ((dev->flags & IFF_PROMISC) && (!sp->promisc_flg)) {
		/*  Put the NIC into promiscuous mode */
		add = &bar0->mac_cfg;
		val64 = readq(&bar0->mac_cfg);
		val64 |= MAC_CFG_RMAC_PROM_ENABLE;

		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) val64, add);
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64 >> 32), (add + 4));

		val64 = readq(&bar0->mac_cfg);
		sp->promisc_flg = 1;
		DBG_PRINT(INFO_DBG, "%s: entered promiscuous mode\n",
			dev->name);
	} else if (!(dev->flags & IFF_PROMISC) && (sp->promisc_flg)) {
		/*  Remove the NIC from promiscuous mode */
		add = (void *) &bar0->mac_cfg;
		val64 = readq(&bar0->mac_cfg);
		val64 &= ~MAC_CFG_RMAC_PROM_ENABLE;

		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) val64, add);
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64 >> 32), (add + 4));

		val64 = readq(&bar0->mac_cfg);
		sp->promisc_flg = 0;
		DBG_PRINT(INFO_DBG, "%s: left promiscuous mode\n",
			dev->name);
	}

	/*  Update individual M_CAST address list */
	if ((!sp->m_cast_flg) && dev->mc_count) {
		if (dev->mc_count >
			(config->max_mc_addr - config->max_mac_addr)) {
			DBG_PRINT(ERR_DBG, "%s: No more Rx filters ",
				dev->name);
			DBG_PRINT(ERR_DBG, "can be added, please enable ");
			DBG_PRINT(ERR_DBG, "ALL_MULTI instead\n");
			return;
		}

		prev_cnt = sp->mc_addr_count;
		sp->mc_addr_count = dev->mc_count;

		/* Clear out the previous list of Mc in the H/W. */
		for (i = 0; i < prev_cnt; i++) {
			writeq(RMAC_ADDR_DATA0_MEM_ADDR(dis_addr),
				&bar0->rmac_addr_data0_mem);
			writeq(RMAC_ADDR_DATA1_MEM_MASK(0ULL),
				&bar0->rmac_addr_data1_mem);
			val64 = RMAC_ADDR_CMD_MEM_WE |
				RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
				RMAC_ADDR_CMD_MEM_OFFSET
				(config->mc_start_offset + i);
			writeq(val64, &bar0->rmac_addr_cmd_mem);

			/* Wait for command completes */
			if (wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
				RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
				S2IO_BIT_RESET)) {
				DBG_PRINT(ERR_DBG, "%s: Adding ", dev->name);
				DBG_PRINT(ERR_DBG, "Multicasts failed\n");
				return;
			}
		}

		/* Create the new Rx filter list and update the same in H/W. */
		for (i = 0, mclist = dev->mc_list; i < dev->mc_count;
			i++, mclist = mclist->next) {
			mac_addr = 0;
			for (j = 0; j < ETH_ALEN; j++) {
				mac_addr |= mclist->dmi_addr[j];
				mac_addr <<= 8;
			}
			mac_addr >>= 8;
			writeq(RMAC_ADDR_DATA0_MEM_ADDR(mac_addr),
				&bar0->rmac_addr_data0_mem);
			writeq(RMAC_ADDR_DATA1_MEM_MASK(0ULL),
				&bar0->rmac_addr_data1_mem);
			val64 = RMAC_ADDR_CMD_MEM_WE |
				RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
				RMAC_ADDR_CMD_MEM_OFFSET
				(i + config->mc_start_offset);
			writeq(val64, &bar0->rmac_addr_cmd_mem);

			/* Wait for command completes */
			if (wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
				RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
				S2IO_BIT_RESET)) {
				DBG_PRINT(ERR_DBG, "%s: Adding ", dev->name);
				DBG_PRINT(ERR_DBG, "Multicasts failed\n");
				return;
			}
		}
	}
}

/* read from CAM unicast & multicast addresses and store it in def_mac_addr
* structure
*/
void do_s2io_store_unicast_mc(struct s2io_nic *sp)
{
	int offset;
	u64 mac_addr = 0x0;
	struct config_param *config = &sp->config;

	/* store unicast & multicast mac addresses */
	for (offset = 0; offset < config->max_mc_addr; offset++) {
		mac_addr = do_s2io_read_unicast_mc(sp, offset);
		/* if read fails disable the entry */
		if (mac_addr == FAILURE)
			mac_addr = S2IO_DISABLE_MAC_ENTRY;
		do_s2io_copy_mac_addr(sp, offset, mac_addr);
	}
}

/* restore unicast MAC addresses(0-15 entries)& multicast(16-31)
 * to CAM from def_mac_addr structure
 **/
static void do_s2io_restore_unicast_mc(struct s2io_nic *sp)
{
	int offset;
	struct config_param *config = &sp->config;
	/* restore unicast mac address */
	for (offset = 0; offset < config->max_mac_addr; offset++)
		do_s2io_prog_unicast(sp->dev,
			sp->def_mac_addr[offset].mac_addr);

	/* restore multicast mac address */
	for (offset = config->mc_start_offset;
		offset < config->max_mc_addr; offset++)
		do_s2io_add_mc(sp, sp->def_mac_addr[offset].mac_addr);
}

/* add a multicast MAC address to CAM. CAM entries 16-31 are used to
 * store multicast MAC entries
 **/
static int do_s2io_add_mc(struct s2io_nic *sp, u8 *addr)
{
	int i;
	u64 mac_addr = 0;
	struct config_param *config = &sp->config;

	for (i = 0; i < ETH_ALEN; i++) {
		mac_addr <<= 8;
		mac_addr |= addr[i];
	}
	if ((0ULL == mac_addr) || (mac_addr == S2IO_DISABLE_MAC_ENTRY))
		return SUCCESS;

	/* check if the multicast mac already preset in CAM */
	for (i = config->mc_start_offset; i < config->max_mc_addr; i++) {
		u64 tmp64;
		tmp64 = do_s2io_read_unicast_mc(sp, i);
		if (tmp64 == S2IO_DISABLE_MAC_ENTRY) /* CAM entry is empty */
			break;

		if (tmp64 == mac_addr)
			return SUCCESS;
	}

	if (i == config->max_mc_addr) {
	    DBG_PRINT(ERR_DBG, "CAM full no space left for multicast MAC\n");
	    return FAILURE;
	}
	/* Update the internal structure with this new mac address */
	do_s2io_copy_mac_addr(sp, i, mac_addr);

	return (do_s2io_add_mac(sp, mac_addr, i));
}

/* add MAC address to CAM */
static int do_s2io_add_mac(struct s2io_nic *sp, u64 addr, int off)
{
	u64 val64;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	writeq(RMAC_ADDR_DATA0_MEM_ADDR(addr),
		&bar0->rmac_addr_data0_mem);

	val64 =
		RMAC_ADDR_CMD_MEM_WE | RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
		RMAC_ADDR_CMD_MEM_OFFSET(off);
	writeq(val64, &bar0->rmac_addr_cmd_mem);

	/* Wait till command completes */
	if (wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
		RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
		S2IO_BIT_RESET)) {
		DBG_PRINT(INFO_DBG, "do_s2io_add_mac failed\n");
		return FAILURE;
	}

	#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
		do_s2io_prog_da_steering(sp, off);
	#endif

	return SUCCESS;
}

/* deletes a specified unicast/multicast mac entry from CAM */
static int do_s2io_delete_unicast_mc(struct s2io_nic *sp, u64 addr)
{
	int off;
	u64 dis_addr = S2IO_DISABLE_MAC_ENTRY, tmp64;
	struct config_param *config = &sp->config;

	for (off = 1;
		off < config->max_mc_addr; off++) {
		tmp64 = do_s2io_read_unicast_mc(sp, off);
		if (tmp64 == addr) {
			/* disable the entry by writing  0xffffffffffffULL */
			if (do_s2io_add_mac(sp, dis_addr, off) ==  FAILURE)
				return FAILURE;
			/* store the new mac list from CAM */
			do_s2io_store_unicast_mc(sp);
			return SUCCESS;
		}
	}
	DBG_PRINT(ERR_DBG, "MAC address 0x%llx not found in CAM\n",
			(unsigned long long)addr);
	return FAILURE;
}

/* read mac entries from CAM */
static u64 do_s2io_read_unicast_mc(struct s2io_nic *sp, int offset)
{
	u64 tmp64 = 0xffffffffffff0000ULL, val64;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* read mac addr */
	val64 =
		RMAC_ADDR_CMD_MEM_RD | RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
		RMAC_ADDR_CMD_MEM_OFFSET(offset);
	writeq(val64, &bar0->rmac_addr_cmd_mem);

	/* Wait till command completes */
	if (wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
		RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
		S2IO_BIT_RESET)) {
		DBG_PRINT(INFO_DBG, "do_s2io_read_unicast_mc failed\n");
		return FAILURE;
	}
	tmp64 = readq(&bar0->rmac_addr_data0_mem);
	return (tmp64 >> 16);
}

/**
 * s2io_set_mac_addr driver entry point
 */

static int s2io_set_mac_addr(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	/* store the MAC address in CAM */
	return (do_s2io_prog_unicast(dev, dev->dev_addr));
}

/**
 *  do_s2io_prog_unicast - Programs the Xframe mac address
 *  @dev : pointer to the device structure.
 *  @addr: a uchar pointer to the new mac address which is to be set.
 *  Description : This procedure will program the Xframe to receive
 *  frames with new Mac Address
 *  Return value: SUCCESS on success and an appropriate (-)ve integer
 *  as defined in errno.h file on failure.
 */

static int do_s2io_prog_unicast(struct net_device *dev, u8 *addr)
{
	struct s2io_nic *sp = dev->priv;
	register u64 mac_addr = 0, perm_addr = 0;
	int i;
	u64 tmp64;
	struct config_param *config = &sp->config;

	/*
	* Set the new MAC address as the new unicast filter and reflect this
	* change on the device address registered with the OS. It will be
	* at offset 0.
	*/
	for (i = 0; i < ETH_ALEN; i++) {
		mac_addr <<= 8;
		mac_addr |= addr[i];
		perm_addr <<= 8;
		perm_addr |= sp->def_mac_addr[0].mac_addr[i];
	}
	/* is addr is 0 or broadcast fail open */
	if ((0 == mac_addr) || (mac_addr == S2IO_DISABLE_MAC_ENTRY))
		return FAILURE;

	/* check if the dev_addr is different than perm_addr */
	if (mac_addr == perm_addr)
		return SUCCESS;

	/* check if the mac already preset in CAM */
	for (i = 1; i < config->max_mac_addr; i++) {
		tmp64 = do_s2io_read_unicast_mc(sp, i);
		if (tmp64 == S2IO_DISABLE_MAC_ENTRY) /* CAM entry is empty */
			break;

		if (tmp64 == mac_addr) {
			DBG_PRINT(INFO_DBG,
			"MAC addr:0x%llx already present in CAM\n",
			(unsigned long long)mac_addr);
			return SUCCESS;
		}
	}
	if (i == config->max_mac_addr) {
		DBG_PRINT(ERR_DBG, "CAM full no space left for Unicast MAC\n");
		return FAILURE;
	}
	/* Update the internal structure with this new mac address */
	do_s2io_copy_mac_addr(sp, i, mac_addr);
	return (do_s2io_add_mac(sp, mac_addr, i));
}

/**
 * s2io_ethtool_sset - Sets different link parameters.
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @info: pointer to the structure with parameters given by ethtool to set
 * link information.
 * Description:
 * The function sets different link parameters provided by the user onto
 * the NIC.
 * Return value:
 * 0 on success.
*/

static int s2io_ethtool_sset(struct net_device *dev,
			     struct ethtool_cmd *info)
{
	struct s2io_nic *sp = dev->priv;
	if ((info->autoneg == AUTONEG_ENABLE) ||
		(info->speed != SPEED_10000) || (info->duplex != DUPLEX_FULL))
		return -EINVAL;

	if (netif_running(dev)) {
		s2io_close(sp->dev);
		s2io_open(sp->dev);
	}

	return 0;
}

/**
 * s2io_ethtol_gset - Return link specific information.
 * @sp : private member of the device structure, pointer to the
 *      s2io_nic structure.
 * @info : pointer to the structure with parameters given by ethtool
 * to return link information.
 * Description:
 * Returns link specific information like speed, duplex etc.. to ethtool.
 * Return value :
 * return 0 on success.
 */

static int s2io_ethtool_gset(struct net_device *dev, struct ethtool_cmd *info)
{
	struct s2io_nic *sp = dev->priv;
	info->supported = (SUPPORTED_10000baseT_Full | SUPPORTED_FIBRE);
#ifdef ADVERTISED_10000baseT_Full
	info->advertising = ADVERTISED_10000baseT_Full;
#else
	info->advertising = SUPPORTED_10000baseT_Full;
#endif
#ifdef ADVERTISED_FIBRE
	info->advertising  |= ADVERTISED_FIBRE;
#else
	info->advertising  |= SUPPORTED_FIBRE;
#endif
	info->port = PORT_FIBRE;
	info->transceiver = XCVR_EXTERNAL;

	if (netif_carrier_ok(sp->dev)) {
		info->speed = SPEED_10000;
		info->duplex = DUPLEX_FULL;
	} else {
		info->speed = -1;
		info->duplex = -1;
	}

	info->autoneg = AUTONEG_DISABLE;
	return 0;
}

/**
 * s2io_ethtool_gdrvinfo - Returns driver specific information.
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @info : pointer to the structure with parameters given by ethtool to
 * return driver information.
 * Description:
 * Returns driver specefic information like name, version etc.. to ethtool.
 * Return value:
 *  void
 */

static void s2io_ethtool_gdrvinfo(struct net_device *dev,
				  struct ethtool_drvinfo *info)
{
	struct s2io_nic *sp = dev->priv;

	strncpy(info->driver, s2io_driver_name, sizeof(info->driver));
	strncpy(info->version, s2io_driver_version, sizeof(info->version));
	strncpy(info->fw_version, "", sizeof(info->fw_version));
	strncpy(info->bus_info, pci_name(sp->pdev), sizeof(info->bus_info));
	info->regdump_len = XENA_REG_SPACE;
	info->eedump_len = XENA_EEPROM_SPACE;
	info->testinfo_len = S2IO_TEST_LEN;
#ifdef ETHTOOL_GSTATS
#ifdef TITAN_LEGACY
	if (sp->device_type == TITAN_DEVICE)
		info->n_stats = S2IO_TITAN_STAT_LEN;
	else
#endif
	{
		if (sp->device_type == XFRAME_I_DEVICE)
			info->n_stats = XFRAME_I_STAT_LEN +
				(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
				(sp->config.rx_ring_num * NUM_RX_SW_STAT);
		else
			info->n_stats = XFRAME_II_STAT_LEN +
				(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
				(sp->config.rx_ring_num * NUM_RX_SW_STAT);

		if (!sp->exec_mode)
			info->n_stats += S2IO_DRIVER_DBG_STAT_LEN;
	}
#endif
}

/**
 *  s2io_ethtool_gregs - dumps the entire space of Xfame into the buffer.
 *  @sp: private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  @regs : pointer to the structure with parameters given by ethtool for
 *  dumping the registers.
 *  @reg_space: The input argumnet into which all the registers are dumped.
 *  Description:
 *  Dumps the entire register space of xFrame NIC into the user given
 *  buffer area.
 * Return value :
 * void .
*/

static void s2io_ethtool_gregs(struct net_device *dev,
			       struct ethtool_regs *regs, void *space)
{
	int i;
	u64 reg;
	u8 *reg_space = (u8 *) space;
	struct s2io_nic *sp = dev->priv;

	regs->len = XENA_REG_SPACE;
	regs->version = sp->pdev->subsystem_device;

	for (i = 0; i < regs->len; i += 8) {
		reg = readq(sp->bar0 + i);
		memcpy((reg_space + i), &reg, 8);
	}
}

/**
 *  s2io_phy_id  - timer function that alternates adapter LED.
 *  @data : address of the private member of the device structure, which
 *  is a pointer to the s2io_nic structure, provided as an u32.
 * Description: This is actually the timer function that alternates the
 * adapter LED bit of the adapter control bit to set/reset every time on
 * invocation. The timer is set for 1/2 a second, hence tha NIC blinks
 *  once every second.
*/
static void s2io_phy_id(unsigned long data)
{
	struct s2io_nic *sp = (struct s2io_nic *) data;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = 0;
	u16 subid;

	subid = sp->pdev->subsystem_device;
	if ((sp->device_type == XFRAME_II_DEVICE) ||
		   ((subid & 0xFF) >= 0x07)) {
		val64 = readq(&bar0->gpio_control);
		val64 ^= GPIO_CTRL_GPIO_0;
		writeq(val64, &bar0->gpio_control);
	} else {
		val64 = readq(&bar0->adapter_control);
		val64 ^= ADAPTER_LED_ON;
		writeq(val64, &bar0->adapter_control);
	}

	mod_timer(&sp->id_timer, jiffies + HZ / 2);
}

/**
 * s2io_ethtool_idnic - To physically identify the nic on the system.
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @id : pointer to the structure with identification parameters given by
 * ethtool.
 * Description: Used to physically identify the NIC on the system.
 * The Link LED will blink for a time specified by the user for
 * identification.
 * NOTE: The Link has to be Up to be able to blink the LED. Hence
 * identification is possible only if it's link is up.
 * Return value:
 * int , returns 0 on success
 */

static int s2io_ethtool_idnic(struct net_device *dev, u32 data)
{
	u64 val64 = 0, last_gpio_ctrl_val;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u16 subid;

	subid = sp->pdev->subsystem_device;
	last_gpio_ctrl_val = readq(&bar0->gpio_control);
	if ((sp->device_type == XFRAME_I_DEVICE) &&
		((subid & 0xFF) < 0x07)) {
		val64 = readq(&bar0->adapter_control);
		if (!(val64 & ADAPTER_CNTL_EN)) {
			DBG_PRINT(ERR_DBG,
				"Adapter Link down, cannot blink LED\n");
			return -EFAULT;
		}
	}
	if (sp->id_timer.function == NULL) {
		init_timer(&sp->id_timer);
		sp->id_timer.function = s2io_phy_id;
		sp->id_timer.data = (unsigned long) sp;
	}
	mod_timer(&sp->id_timer, jiffies);
	if (data)
		msleep_interruptible(data * HZ);
	else
		msleep_interruptible(MAX_FLICKER_TIME);
	del_timer_sync(&sp->id_timer);

	if (CARDS_WITH_FAULTY_LINK_INDICATORS(sp->device_type, subid)) {
		writeq(last_gpio_ctrl_val, &bar0->gpio_control);
		last_gpio_ctrl_val = readq(&bar0->gpio_control);
	}

	return 0;
}


static void s2io_ethtool_gringparam(struct net_device *dev,
				    struct ethtool_ringparam *ering)
{
	struct s2io_nic *sp = dev->priv;
	int i, tx_desc_count = 0, rx_desc_count = 0;

	memset(ering, 0, sizeof(struct ethtool_ringparam));
	ering->tx_max_pending = MAX_TX_DESC;
	for (i = 0 ; i < sp->config.tx_fifo_num; i++)
		tx_desc_count += sp->config.tx_cfg[i].fifo_len;

	DBG_PRINT(INFO_DBG, "\nmax txds : %d\n", sp->config.max_txds);
	ering->tx_pending = tx_desc_count;

	for (i = 0; i < MAX_RX_RINGS; i++)
		ering->rx_max_pending += rx_ring_sz[i] * rxd_count[sp->rxd_mode];

	ering->rx_jumbo_max_pending = ering->rx_max_pending;
	rx_desc_count = 0;
	for (i = 0 ; i < sp->config.rx_ring_num ; i++)
		rx_desc_count += sp->config.rx_cfg[i].num_rxd;
	ering->rx_pending = rx_desc_count;
	ering->rx_jumbo_pending = rx_desc_count;
	ering->rx_mini_max_pending = 0;
	ering->rx_mini_pending = 0;
}

/**
 * s2io_ethtool_getpause_data -Pause frame frame generation and reception.
 * @sp : private member of the device structure, which is a pointer to the
 *	s2io_nic structure.
 * @ep : pointer to the structure with pause parameters given by ethtool.
 * Description:
 * Returns the Pause frame generation and reception capability of the NIC.
 * Return value:
 *  void
 */
static void s2io_ethtool_getpause_data(struct net_device *dev,
		struct ethtool_pauseparam *ep)
{
	u64 val64;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	val64 = readq(&bar0->rmac_pause_cfg);
	if (val64 & RMAC_PAUSE_GEN_ENABLE)
		ep->tx_pause = TRUE;
	if (val64 & RMAC_PAUSE_RX_ENABLE)
		ep->rx_pause = TRUE;
	ep->autoneg = FALSE;
}

/**
 * s2io_ethtool_setpause_data -  set/reset pause frame generation.
 * @sp : private member of the device structure, which is a pointer to the
 *      s2io_nic structure.
 * @ep : pointer to the structure with pause parameters given by ethtool.
 * Description:
 * It can be used to set or reset Pause frame generation or reception
 * support of the NIC.
 * Return value:
 * int, returns 0 on Success
 */

static int s2io_ethtool_setpause_data(struct net_device *dev,
		struct ethtool_pauseparam *ep)
{
	u64 val64;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	val64 = readq(&bar0->rmac_pause_cfg);
	if (ep->tx_pause)
		val64 |= RMAC_PAUSE_GEN_ENABLE;
	else
		val64 &= ~RMAC_PAUSE_GEN_ENABLE;
	if (ep->rx_pause)
		val64 |= RMAC_PAUSE_RX_ENABLE;
	else
		val64 &= ~RMAC_PAUSE_RX_ENABLE;
	writeq(val64, &bar0->rmac_pause_cfg);
	return 0;
}

/**
 * read_eeprom - reads 4 bytes of data from user given offset.
 * @sp : private member of the device structure, which is a pointer to the
 *      s2io_nic structure.
 * @off : offset at which the data must be written
 * @data : Its an output parameter where the data read at the given
 *	offset is stored.
 * Description:
 * Will read 4 bytes of data from the user given offset and return the
 * read data.
 * NOTE: Will allow to read only part of the EEPROM visible through the
 *   I2C bus.
 * Return value:
 *  -1 on failure and 0 on success.
 */

#define S2IO_DEV_ID		5
static int read_eeprom(struct s2io_nic *sp, int off, u64 *data)
{
	int ret = -1;
	u32 exit_cnt = 0;
	u64 val64;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	if (sp->device_type == XFRAME_I_DEVICE) {
		val64 = I2C_CONTROL_DEV_ID(S2IO_DEV_ID) |
		I2C_CONTROL_ADDR(off) | I2C_CONTROL_BYTE_CNT(0x3) |
		I2C_CONTROL_READ | I2C_CONTROL_CNTL_START;
		SPECIAL_REG_WRITE(val64, &bar0->i2c_control, LF);

		while (exit_cnt < 5) {
			val64 = readq(&bar0->i2c_control);
			if (I2C_CONTROL_CNTL_END(val64)) {
				*data = I2C_CONTROL_GET_DATA(val64);
				ret = 0;
				break;
			}
			msleep(50);
			exit_cnt++;
		}
	}

	if (sp->device_type == XFRAME_II_DEVICE) {
		val64 = SPI_CONTROL_KEY(0x9) | SPI_CONTROL_SEL1 |
			SPI_CONTROL_BYTECNT(0x3) |
			SPI_CONTROL_CMD(0x3) | SPI_CONTROL_ADDR(off);
		SPECIAL_REG_WRITE(val64, &bar0->spi_control, LF);
		val64 |= SPI_CONTROL_REQ;
		SPECIAL_REG_WRITE(val64, &bar0->spi_control, LF);
		while (exit_cnt < 5) {
			val64 = readq(&bar0->spi_control);
			if (val64 & SPI_CONTROL_NACK) {
				ret = 1;
				break;
			} else if (val64 & SPI_CONTROL_DONE) {
				*data = readq(&bar0->spi_data);
				*data &= 0xffffff;
				ret = 0;
				break;
			}
			msleep(50);
			exit_cnt++;
		}
	}
	return ret;
}

/**
 *  write_eeprom - actually writes the relevant part of the data value.
 *  @sp : private member of the device structure, which is a pointer to the
 *       s2io_nic structure.
 *  @off : offset at which the data must be written
 *  @data : The data that is to be written
 *  @cnt : Number of bytes of the data that are actually to be written into
 *  the Eeprom. (max of 3)
 * Description:
 *  Actually writes the relevant part of the data value into the Eeprom
 *  through the I2C bus.
 * Return value:
 *  0 on success, -1 on failure.
 */

static int write_eeprom(struct s2io_nic *sp, int off, u64 data, int cnt)
{
	int exit_cnt = 0, ret = -1;
	u64 val64;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	if (sp->device_type == XFRAME_I_DEVICE) {
		val64 = I2C_CONTROL_DEV_ID(S2IO_DEV_ID) |
		I2C_CONTROL_ADDR(off) | I2C_CONTROL_BYTE_CNT(cnt) |
		I2C_CONTROL_SET_DATA((u32)data) | I2C_CONTROL_CNTL_START;
		SPECIAL_REG_WRITE(val64, &bar0->i2c_control, LF);

		while (exit_cnt < 5) {
			val64 = readq(&bar0->i2c_control);
			if (I2C_CONTROL_CNTL_END(val64)) {
				if (!(val64 & I2C_CONTROL_NACK))
					ret = 0;
				break;
			}
			msleep(50);
			exit_cnt++;
		}
	}

	if (sp->device_type == XFRAME_II_DEVICE) {
		int write_cnt = (cnt == 8) ? 0 : cnt;
		writeq(SPI_DATA_WRITE(data,(cnt<<3)), &bar0->spi_data);

		val64 = SPI_CONTROL_KEY(0x9) | SPI_CONTROL_SEL1 |
			SPI_CONTROL_BYTECNT(write_cnt) |
			SPI_CONTROL_CMD(0x2) | SPI_CONTROL_ADDR(off);
		SPECIAL_REG_WRITE(val64, &bar0->spi_control, LF);
		val64 |= SPI_CONTROL_REQ;
		SPECIAL_REG_WRITE(val64, &bar0->spi_control, LF);
		while (exit_cnt < 5) {
			val64 = readq(&bar0->spi_control);
			if (val64 & SPI_CONTROL_NACK) {
				ret = 1;
				break;
			} else if (val64 & SPI_CONTROL_DONE) {
				ret = 0;
				break;
			}
			msleep(50);
			exit_cnt++;
		}
	}
	return ret;
}

static void s2io_vpd_read(struct s2io_nic *nic)
{
	u8 *vpd_data;
	u8 data;
	int i=0, cnt, fail = 0;
	int vpd_addr = 0x80;
	struct swDbgStat *stats = nic->sw_dbg_stat;

	if (nic->device_type == XFRAME_II_DEVICE) {
		strcpy(nic->product_name, "Xframe II 10GbE network adapter");
		vpd_addr = 0x80;
	} else {
		strcpy(nic->product_name, "Xframe I 10GbE network adapter");
		vpd_addr = 0x50;
	}
	strcpy(nic->serial_num, "NOT AVAILABLE");

	vpd_data = kmalloc(256, GFP_KERNEL);
	if (!vpd_data) {
		stats->mem_alloc_fail_cnt++;
		return;
	}
	stats->mem_allocated += 256;

	for (i = 0; i < 256; i +=4 ) {
		pci_write_config_byte(nic->pdev, (vpd_addr + 2), i);
		pci_read_config_byte(nic->pdev,  (vpd_addr + 2), &data);
		pci_write_config_byte(nic->pdev, (vpd_addr + 3), 0);
		for (cnt = 0; cnt <5; cnt++) {
			msleep(2);
			pci_read_config_byte(nic->pdev, (vpd_addr + 3), &data);
			if (data == 0x80)
				break;
		}
		if (cnt >= 5) {
			DBG_PRINT(ERR_DBG, "Read of VPD data failed\n");
			fail = 1;
			break;
		}
		pci_read_config_dword(nic->pdev,  (vpd_addr + 4),
				(u32 *)&vpd_data[i]);
	}
	if (!fail) {
		/* read serial number of adapter */
		for (cnt = 0; cnt < 256; cnt++) {
			if ((vpd_data[cnt] == 'S') &&
				(vpd_data[cnt+1] == 'N') &&
				(vpd_data[cnt+2] < VPD_STRING_LEN)) {
				memset(nic->serial_num, 0, VPD_STRING_LEN);
				memcpy(nic->serial_num, &vpd_data[cnt + 3],
					vpd_data[cnt+2]);
				break;
			}
		}
	}
	if ((!fail) && (vpd_data[1] < VPD_STRING_LEN)) {
		memset(nic->product_name, 0, vpd_data[1]);
		memcpy(nic->product_name, &vpd_data[3], vpd_data[1]);
	}
	kfree(vpd_data);
	stats->mem_freed += 256;
}

/**
 *  s2io_ethtool_geeprom  - reads the value stored in the Eeprom.
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  @eeprom : pointer to the user level structure provided by ethtool,
 *  containing all relevant information.
 *  @data_buf : user defined value to be written into Eeprom.
 *  Description: Reads the values stored in the Eeprom at given offset
 *  for a given length. Stores these values int the input argument data
 *  buffer 'data_buf' and returns these to the caller (ethtool.)
 *  Return value:
 *  int  0 on success
 */

static int s2io_ethtool_geeprom(struct net_device *dev,
			 struct ethtool_eeprom *eeprom, u8 *data_buf)
{
	u32 i, valid;
	u64 data;
	struct s2io_nic *sp = dev->priv;

	eeprom->magic = sp->pdev->vendor | (sp->pdev->device << 16);

	if ((eeprom->offset + eeprom->len) > (XENA_EEPROM_SPACE))
		eeprom->len = XENA_EEPROM_SPACE - eeprom->offset;

	for (i = 0; i < eeprom->len; i += 4) {
		if (read_eeprom(sp, (eeprom->offset + i), &data)) {
			DBG_PRINT(ERR_DBG, "Read of EEPROM failed\n");
			return -EFAULT;
		}
		valid = INV(data);
		memcpy((data_buf + i), &valid, 4);
	}
	return 0;
}

/**
 *  s2io_ethtool_seeprom - tries to write the user provided value in Eeprom
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  @eeprom : pointer to the user level structure provided by ethtool,
 *  containing all relevant information.
 *  @data_buf ; user defined value to be written into Eeprom.
 *  Description:
 *  Tries to write the user provided value in the Eeprom, at the offset
 *  given by the user.
 *  Return value:
 *  0 on success, -EFAULT on failure.
 */

static int s2io_ethtool_seeprom(struct net_device *dev,
				struct ethtool_eeprom *eeprom,
				u8 *data_buf)
{
	int len = eeprom->len, cnt = 0;
	u64 valid = 0, data;
	struct s2io_nic *sp = dev->priv;

	if (eeprom->magic != (sp->pdev->vendor | (sp->pdev->device << 16))) {
		DBG_PRINT(ERR_DBG, "ETHTOOL_WRITE_EEPROM Err: Magic value ");
		DBG_PRINT(ERR_DBG, "is wrong, Its not 0x%x\n", eeprom->magic);
		return -EFAULT;
	}

	while (len) {
		data = (u32) data_buf[cnt] & 0x000000FF;
		if (data)
			valid = (u32) (data << 24);
		else
			valid = data;

		if (write_eeprom(sp, (eeprom->offset + cnt), valid, 0)) {
			DBG_PRINT(ERR_DBG, "ETHTOOL_WRITE_EEPROM Err: ");
			DBG_PRINT(ERR_DBG,
			"Cannot write into the specified offset\n");
			return -EFAULT;
		}
		cnt++;
		len--;
	}

	return 0;
}

/**
 * s2io_register_test - reads and writes into all clock domains.
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @data : variable that returns the result of each of the test conducted b
 * by the driver.
 * Description:
 * Read and write into all clock domains. The NIC has 3 clock domains,
 * see that registers in all the three regions are accessible.
 * Return value:
 * 0 on success.
 */

static int s2io_register_test(struct s2io_nic *sp, uint64_t *data)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = 0, exp_val;
	int fail = 0;

	val64 = readq(&bar0->pif_rd_swapper_fb);
	if (val64 != 0x123456789abcdefULL) {
		fail = 1;
		DBG_PRINT(INFO_DBG, "Read Test level 1 fails\n");
	}

	val64 = readq(&bar0->rmac_pause_cfg);
	if (val64 != 0xc000ffff00000000ULL) {
		fail = 1;
		DBG_PRINT(INFO_DBG, "Read Test level 2 fails\n");
	}

	val64 = readq(&bar0->rx_queue_cfg);
	if (sp->device_type == XFRAME_II_DEVICE)
		exp_val = 0x0404040404040404ULL;
	else
		exp_val = 0x0808080808080808ULL;
	if (val64 != exp_val) {
		fail = 1;
		DBG_PRINT(INFO_DBG, "Read Test level 3 fails\n");
	}

	val64 = readq(&bar0->xgxs_efifo_cfg);
	if (val64 != 0x000000001923141EULL) {
		fail = 1;
		DBG_PRINT(INFO_DBG, "Read Test level 4 fails\n");
	}

	val64 = 0x5A5A5A5A5A5A5A5AULL;
	writeq(val64, &bar0->xmsi_data);
	val64 = readq(&bar0->xmsi_data);
	if (val64 != 0x5A5A5A5A5A5A5A5AULL) {
		fail = 1;
		DBG_PRINT(ERR_DBG, "Write Test level 1 fails\n");
	}

	val64 = 0xA5A5A5A5A5A5A5A5ULL;
	writeq(val64, &bar0->xmsi_data);
	val64 = readq(&bar0->xmsi_data);
	if (val64 != 0xA5A5A5A5A5A5A5A5ULL) {
		fail = 1;
		DBG_PRINT(ERR_DBG, "Write Test level 2 fails\n");
	}

	*data = fail;
	return fail;
}

/**
 * s2io_eeprom_test - to verify that EEprom in the xena can be programmed.
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 * Description:
 * Verify that EEPROM in the xena can be programmed using I2C_CONTROL
 * register.
 * Return value:
 * 0 on success.
 */

static int s2io_eeprom_test(struct s2io_nic *sp, uint64_t *data)
{
	int fail = 0;
	u64 ret_data, org_4F0, org_7F0;
	u8 saved_4F0 = 0, saved_7F0 = 0;
	struct net_device *dev = sp->dev;

	/* Test Write Error at offset 0 */
	/* Note that SPI interface allows write access to all areas
	 * of EEPROM. Hence doing all negative testing only for Xframe I.
	 */
	if (sp->device_type == XFRAME_I_DEVICE)
		if (!write_eeprom(sp, 0, 0, 3))
			fail = 1;

	/* Save current values at offsets 0x4F0 and 0x7F0 */
	if (!read_eeprom(sp, 0x4F0, &org_4F0))
		saved_4F0 = 1;
	if (!read_eeprom(sp, 0x7F0, &org_7F0))
		saved_7F0 = 1;

	/* Test Write at offset 4f0 */
	if (write_eeprom(sp, 0x4F0, 0x012345, 3))
		fail = 1;
	if (read_eeprom(sp, 0x4F0, &ret_data))
		fail = 1;

	if (ret_data != 0x012345) {
		DBG_PRINT(ERR_DBG, "%s: eeprom test error at offset 0x4F0. "
			"Data written %llx Data read %llx\n",
			dev->name, (unsigned long long)0x12345,
			(unsigned long long)ret_data);
		fail = 1;
	}

	/* Reset the EEPROM data go FFFF */
	write_eeprom(sp, 0x4F0, 0xFFFFFF, 3);

	/* Test Write Request Error at offset 0x7c */
	if (sp->device_type == XFRAME_I_DEVICE)
		if (!write_eeprom(sp, 0x07C, 0, 3))
			fail = 1;

	/* Test Write Request at offset 0x7f0 */
	if (write_eeprom(sp, 0x7F0, 0x012345, 3))
		fail = 1;
	if (read_eeprom(sp, 0x7F0, &ret_data))
		fail = 1;

	if (ret_data != 0x012345) {
		DBG_PRINT(ERR_DBG, "%s: eeprom test error at offset 0x7F0. "
			"Data written %llx Data read %llx\n",
			dev->name, (unsigned long long)0x12345,
			(unsigned long long)ret_data);
		fail = 1;
	}

	/* Reset the EEPROM data go FFFF */
	write_eeprom(sp, 0x7F0, 0xFFFFFF, 3);

	if (sp->device_type == XFRAME_I_DEVICE) {
		/* Test Write Error at offset 0x80 */
		if (!write_eeprom(sp, 0x080, 0, 3))
			fail = 1;

		/* Test Write Error at offset 0xfc */
		if (!write_eeprom(sp, 0x0FC, 0, 3))
			fail = 1;

		/* Test Write Error at offset 0x100 */
		if (!write_eeprom(sp, 0x100, 0, 3))
			fail = 1;

		/* Test Write Error at offset 4ec */
		if (!write_eeprom(sp, 0x4EC, 0, 3))
			fail = 1;
	}

	/* Restore values at offsets 0x4F0 and 0x7F0 */
	if (saved_4F0)
		write_eeprom(sp, 0x4F0, org_4F0, 3);
	if (saved_7F0)
		write_eeprom(sp, 0x7F0, org_7F0, 3);

	*data = fail;
	return fail;
}

/**
 * s2io_bist_test - invokes the MemBist test of the card .
 * @sp : private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 * Description:
 * This invokes the MemBist test of the card. We give around
 * 2 secs time for the Test to complete. If it's still not complete
 * within this peiod, we consider that the test failed.
 * Return value:
 * 0 on success and -1 on failure.
 */

static int s2io_bist_test(struct s2io_nic *sp, uint64_t *data)
{
	u8 bist = 0;
	int cnt = 0, ret = -1;

	pci_read_config_byte(sp->pdev, PCI_BIST, &bist);
	bist |= PCI_BIST_START;
	pci_write_config_word(sp->pdev, PCI_BIST, bist);

	while (cnt < 20) {
		pci_read_config_byte(sp->pdev, PCI_BIST, &bist);
		if (!(bist & PCI_BIST_START)) {
			*data = (bist & PCI_BIST_CODE_MASK);
			ret = 0;
			break;
		}
		msleep(100);
		cnt++;
	}

	return ret;
}

/**
 * s2io-link_test - verifies the link state of the nic
 * @sp ; private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @data: variable that returns the result of each of the test conducted by
 * the driver.
 * Description:
 * The function verifies the link state of the NIC and updates the input
 * argument 'data' appropriately.
 * Return value:
 * 0 on success.
 */

static int s2io_link_test(struct s2io_nic *sp, uint64_t *data)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;

	val64 = readq(&bar0->adapter_status);
	if (!(LINK_IS_UP(val64)))
		*data = 1;
	else
		*data = 0;

	return *data;
}

/**
 * s2io_rldram_test - offline test for access to the RldRam chip on the NIC
 * @sp - private member of the device structure, which is a pointer to the
 * s2io_nic structure.
 * @data - variable that returns the result of each of the test
 * conducted by the driver.
 * Description:
 *  This is one of the offline test that tests the read and write
 *  access to the RldRam chip on the NIC.
 * Return value:
 *  0 on success.
 */

static int s2io_rldram_test(struct s2io_nic *sp, uint64_t *data)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	int cnt, iteration = 0, test_fail = 0;

	val64 = readq(&bar0->adapter_control);
	val64 &= ~ADAPTER_ECC_EN;
	writeq(val64, &bar0->adapter_control);

	val64 = readq(&bar0->mc_rldram_test_ctrl);
	val64 |= MC_RLDRAM_TEST_MODE;
	SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_test_ctrl, LF);

	val64 = readq(&bar0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_QUEUE_SIZE_ENABLE;
	SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_mrs, UF);

	val64 |= MC_RLDRAM_MRS_ENABLE;
	SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_mrs, UF);

	while (iteration < 2) {
		val64 = 0x55555555aaaa0000ULL;
		if (iteration == 1)
			val64 ^= 0xFFFFFFFFFFFF0000ULL;

		writeq(val64, &bar0->mc_rldram_test_d0);

		val64 = 0xaaaa5a5555550000ULL;
		if (iteration == 1)
			val64 ^= 0xFFFFFFFFFFFF0000ULL;

		writeq(val64, &bar0->mc_rldram_test_d1);

		val64 = 0x55aaaaaaaa5a0000ULL;
		if (iteration == 1)
			val64 ^= 0xFFFFFFFFFFFF0000ULL;

		writeq(val64, &bar0->mc_rldram_test_d2);

		val64 = (u64) (0x0000003ffffe0100ULL);
		writeq(val64, &bar0->mc_rldram_test_add);

		val64 = MC_RLDRAM_TEST_MODE | MC_RLDRAM_TEST_WRITE |
			MC_RLDRAM_TEST_GO;
		SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_test_ctrl, LF);

		for (cnt = 0; cnt < 5; cnt++) {
			val64 = readq(&bar0->mc_rldram_test_ctrl);
			if (val64 & MC_RLDRAM_TEST_DONE)
				break;
			msleep(200);
		}

		if (cnt == 5)
			break;

		val64 = MC_RLDRAM_TEST_MODE | MC_RLDRAM_TEST_GO;
		SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_test_ctrl, LF);

		for (cnt = 0; cnt < 5; cnt++) {
			val64 = readq(&bar0->mc_rldram_test_ctrl);
			if (val64 & MC_RLDRAM_TEST_DONE)
				break;
			msleep(500);
		}

		if (cnt == 5)
			break;

		val64 = readq(&bar0->mc_rldram_test_ctrl);
		if (!(val64 & MC_RLDRAM_TEST_PASS))
			test_fail = 1;

		iteration++;
	}

	*data = test_fail;

	/* Bring the adapter out of test mode */
	SPECIAL_REG_WRITE(0, &bar0->mc_rldram_test_ctrl, LF);

	return test_fail;
}

/**
 *  s2io_ethtool_test - conducts 6 tsets to determine the health of card.
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  @ethtest : pointer to a ethtool command specific structure that will be
 *  returned to the user.
 *  @data : variable that returns the result of each of the test
 * conducted by the driver.
 * Description:
 *  This function conducts 6 tests ( 4 offline and 2 online) to determine
 *  the health of the card.
 * Return value:
 *  void
 */

static void s2io_ethtool_test(struct net_device *dev,
			struct ethtool_test *ethtest,
			uint64_t *data)
{
	struct s2io_nic *sp = dev->priv;
	int orig_state = netif_running(sp->dev);

	if (ethtest->flags == ETH_TEST_FL_OFFLINE) {
		/* Offline Tests. */
		if (orig_state)
			s2io_close(sp->dev);

		if (s2io_register_test(sp, &data[0]))
			ethtest->flags |= ETH_TEST_FL_FAILED;

		s2io_reset(sp);

		if (s2io_rldram_test(sp, &data[3]))
			ethtest->flags |= ETH_TEST_FL_FAILED;

		s2io_reset(sp);

		if (s2io_eeprom_test(sp, &data[1]))
			ethtest->flags |= ETH_TEST_FL_FAILED;

		if (s2io_bist_test(sp, &data[4]))
			ethtest->flags |= ETH_TEST_FL_FAILED;

		if (orig_state)
			s2io_open(sp->dev);

		data[2] = 0;
	} else {
		/* Online Tests. */
		if (!orig_state) {
			DBG_PRINT(ERR_DBG,
				"%s: is not up, cannot run test\n",
				dev->name);
			data[0] = -1;
			data[1] = -1;
			data[2] = -1;
			data[3] = -1;
			data[4] = -1;
		}

		if (s2io_link_test(sp, &data[2]))
			ethtest->flags |= ETH_TEST_FL_FAILED;

		data[0] = 0;
		data[1] = 0;
		data[3] = 0;
		data[4] = 0;
	}
}

#ifdef ETHTOOL_GSTATS
static void s2io_get_ethtool_stats(struct net_device *dev,
				struct ethtool_stats *estats,
				u64 *tmp_stats)
{
	int i = 0, k = 0, j = 0;
#ifdef TITAN_LEGACY
	int j, index;
#endif
	struct s2io_nic *sp = dev->priv;
	struct stat_block *stat_info = sp->mac_control.stats_info;
	struct swDbgStat *stats = sp->sw_dbg_stat;
	struct xpakStat *xpak_stat = sp->xpak_stat;
	struct swErrStat *sw_err_stat = sp->sw_err_stat;
	struct mac_info *mac_control = &sp->mac_control;
#define lro_stat mac_control->rings[j].rx_ring_stat->sw_lro_stat
#ifdef TITAN_LEGACY
	u64 *statslinkinfo;

	if (sp->device_type == TITAN_DEVICE) {
		for (index = 0; index < MAC_LINKS; index++) {
			statslinkinfo = &stat_info->stats_link_info[index];
			for (j = 0; j < LINK_MAX; j++)
				tmp_stats[i++] =
					le64_to_cpu(*(statslinkinfo++));
		}
		for (index = 0; index < MAC_AGGREGATORS; index++) {
			statslinkinfo = &stat_info->stats_aggr_info[index];
			for (j = 0; j < AGGR_MAX; j++)
				tmp_stats[i++] =
					le64_to_cpu(*(statslinkinfo++));
		}
	} else {
#endif
	s2io_updt_stats(sp);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_frms_oflow) << 32  |
		le32_to_cpu(stat_info->tmac_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_data_octets_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_data_octets);
	tmp_stats[i++] = le64_to_cpu(stat_info->tmac_drop_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_mcst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_mcst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_bcst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_bcst_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->tmac_pause_ctrl_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_ttl_octets_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_ttl_octets);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_ucst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_ucst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_nucst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_nucst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_any_err_frms_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_any_err_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->tmac_ttl_less_fb_octets);
	tmp_stats[i++] = le64_to_cpu(stat_info->tmac_vld_ip_octets);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_vld_ip_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_vld_ip);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_drop_ip_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_drop_ip);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_icmp_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_icmp);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->tmac_rst_tcp_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_rst_tcp);
	tmp_stats[i++] = le64_to_cpu(stat_info->tmac_tcp);
	tmp_stats[i++] = (u64)le32_to_cpu(stat_info->tmac_udp_oflow) << 32 |
		le32_to_cpu(stat_info->tmac_udp);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_vld_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_vld_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_data_octets_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_data_octets);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_fcs_err_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_drop_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_vld_mcst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_vld_mcst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_vld_bcst_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_vld_bcst_frms);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_in_rng_len_err_frms);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_out_rng_len_err_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_long_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_pause_ctrl_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_unsup_ctrl_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_ttl_octets_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_ttl_octets);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_accepted_ucst_frms_oflow)
		<< 32 | le32_to_cpu(stat_info->rmac_accepted_ucst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_accepted_nucst_frms_oflow)
		 << 32 | le32_to_cpu(stat_info->rmac_accepted_nucst_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_discarded_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_discarded_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_drop_events_oflow)
		 << 32 | le32_to_cpu(stat_info->rmac_drop_events);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_less_fb_octets);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_usized_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_usized_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_osized_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_osized_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_frag_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_frag_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_jabber_frms_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_jabber_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_64_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_65_127_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_128_255_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_256_511_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_512_1023_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_1024_1518_frms);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_ip_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_ip);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ip_octets);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_hdr_err_ip);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_drop_ip_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_drop_ip);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_icmp_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_icmp);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_tcp);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_udp_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_udp);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_err_drp_udp_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_err_drp_udp);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_xgmii_err_sym);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q0);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q1);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q2);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q3);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q4);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q5);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q6);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_frms_q7);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q0);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q1);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q2);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q3);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q4);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q5);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q6);
	tmp_stats[i++] = le16_to_cpu(stat_info->rmac_full_q7);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_pause_cnt_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_pause_cnt);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_xgmii_data_err_cnt);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_xgmii_ctrl_err_cnt);
	tmp_stats[i++] =
		(u64)le32_to_cpu(stat_info->rmac_accepted_ip_oflow) << 32 |
		le32_to_cpu(stat_info->rmac_accepted_ip);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_err_tcp);
	tmp_stats[i++] = le32_to_cpu(stat_info->rd_req_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->new_rd_req_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->new_rd_req_rtry_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->rd_rtry_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->wr_rtry_rd_ack_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->wr_req_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->new_wr_req_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->new_wr_req_rtry_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->wr_rtry_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->wr_disc_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->rd_rtry_wr_ack_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->txp_wr_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->txd_rd_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->txd_wr_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->rxd_rd_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->rxd_wr_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->txf_rd_cnt);
	tmp_stats[i++] = le32_to_cpu(stat_info->rxf_wr_cnt);

	/* Enhanced statistics exist only for Hercules */
	if (sp->device_type == XFRAME_II_DEVICE) {
		tmp_stats[i++] =
			le64_to_cpu(stat_info->rmac_ttl_1519_4095_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_4096_8191_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_8192_max_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_ttl_gt_max_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_osized_alt_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_jabber_alt_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_gt_max_alt_frms);
	tmp_stats[i++] = le64_to_cpu(stat_info->rmac_vlan_frms);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_len_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_fcs_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_pf_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_da_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_wol_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_rts_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_ingm_full_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->rmac_red_discard);
	tmp_stats[i++] = le32_to_cpu(stat_info->link_fault_cnt);
	}

	tmp_stats[i++] = 0;
	tmp_stats[i++] = xpak_stat->alarm_transceiver_temp_high;
	tmp_stats[i++] = xpak_stat->alarm_transceiver_temp_low;
	tmp_stats[i++] = xpak_stat->alarm_laser_bias_current_high;
	tmp_stats[i++] = xpak_stat->alarm_laser_bias_current_low;
	tmp_stats[i++] = xpak_stat->alarm_laser_output_power_high;
	tmp_stats[i++] = xpak_stat->alarm_laser_output_power_low;
	tmp_stats[i++] = xpak_stat->warn_transceiver_temp_high;
	tmp_stats[i++] = xpak_stat->warn_transceiver_temp_low;
	tmp_stats[i++] = xpak_stat->warn_laser_bias_current_high;
	tmp_stats[i++] = xpak_stat->warn_laser_bias_current_low;
	tmp_stats[i++] = xpak_stat->warn_laser_output_power_high;
	tmp_stats[i++] = xpak_stat->warn_laser_output_power_low;

	tmp_stats[i++] = sw_err_stat->single_ecc_errs;
	tmp_stats[i++] = sw_err_stat->double_ecc_errs;
	tmp_stats[i++] = sw_err_stat->parity_err_cnt;
	tmp_stats[i++] = sw_err_stat->serious_err_cnt;
	tmp_stats[i++] = sw_err_stat->rx_stuck_cnt;
	tmp_stats[i++] = sw_err_stat->soft_reset_cnt;
	tmp_stats[i++] = sw_err_stat->watchdog_timer_cnt;
	tmp_stats[i++] = sw_err_stat->dte_reset_cnt;

	tmp_stats[i++] = sw_err_stat->skb_null_s2io_xmit_cnt;
	tmp_stats[i++] = sw_err_stat->skb_null_rx_intr_handler_cnt;
	tmp_stats[i++] = sw_err_stat->skb_null_tx_intr_handler_cnt;

	tmp_stats[i++] = sw_err_stat->tda_err_cnt;
	tmp_stats[i++] = sw_err_stat->pfc_err_cnt;
	tmp_stats[i++] = sw_err_stat->pcc_err_cnt;
	tmp_stats[i++] = sw_err_stat->tti_err_cnt;
	tmp_stats[i++] = sw_err_stat->tpa_err_cnt;
	tmp_stats[i++] = sw_err_stat->sm_err_cnt;
	tmp_stats[i++] = sw_err_stat->lso_err_cnt;
	tmp_stats[i++] = sw_err_stat->mac_tmac_err_cnt;
	tmp_stats[i++] = sw_err_stat->mac_rmac_err_cnt;
	tmp_stats[i++] = sw_err_stat->xgxs_txgxs_err_cnt;
	tmp_stats[i++] = sw_err_stat->xgxs_rxgxs_err_cnt;
	tmp_stats[i++] = sw_err_stat->rc_err_cnt;
	tmp_stats[i++] = sw_err_stat->prc_pcix_err_cnt;
	tmp_stats[i++] = sw_err_stat->rpa_err_cnt;
	tmp_stats[i++] = sw_err_stat->rda_err_cnt;
	tmp_stats[i++] = sw_err_stat->rti_err_cnt;
	tmp_stats[i++] = sw_err_stat->mc_err_cnt;

	tmp_stats[i++] = sp->tx_intr_cnt;
	tmp_stats[i++] = sp->rx_intr_cnt;

#if defined (__VMKLNX__)
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->tx_pkt_cnt;
#endif /* defined (__VMKLNX__) */
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->fifo_full_cnt;
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->tx_buf_abort_cnt;
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->tx_desc_abort_cnt;
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->tx_parity_err_cnt;
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
			mac_control->fifos[k].tx_fifo_stat->tx_link_loss_cnt;
	for (k = 0; k < sp->config.tx_fifo_num; k++)
		tmp_stats[i++] =
		mac_control->fifos[k].tx_fifo_stat->tx_list_proc_err_cnt;

	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->ring_full_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_parity_err_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_abort_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_parity_abort_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_rda_fail_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_unkn_prot_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_fcs_err_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_buf_size_err_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_rxd_corrupt_cnt;
	for (k = 0; k < sp->config.rx_ring_num; k++)
		tmp_stats[i++] =
			mac_control->rings[k].rx_ring_stat->rx_unkn_err_cnt;

	for (j = 0; j < sp->config.rx_ring_num; j++) {
		tmp_stats[i++] = lro_stat.clubbed_frms_cnt;
		tmp_stats[i++] = lro_stat.sending_both;
		tmp_stats[i++] = lro_stat.outof_sequence_pkts;
		tmp_stats[i++] = lro_stat.flush_max_pkts;

		if (lro_stat.num_aggregations) {
			u64 tmp = lro_stat.sum_avg_pkts_aggregated;
			int count = 0;
			/*
			 * Since 64-bit divide does not work on all platforms,
			 * do repeated subtraction.
			 */
			while (tmp >= lro_stat.num_aggregations) {
				tmp -= lro_stat.num_aggregations;
				count++;
			}
			tmp_stats[i++] = count;
		}
		else
			tmp_stats[i++] = 0;

		tmp_stats[i] = 0;
		for (k = 0; k < MAX_RX_RINGS; k++) {
			if (sp->mac_control.rings[k].max_pkts_aggr >
								tmp_stats[i])
				 tmp_stats[i] =
					sp->mac_control.rings[k].max_pkts_aggr;
		}
		i++;
	}

	/* Don't print these statistics in release mode */
	if (!sp->exec_mode) {
		for (k = 0; k < MAX_TX_FIFOS; k++)
			tmp_stats[i++] = sp->sw_dbg_stat->tmac_frms_q[k];
		tmp_stats[i++] = sp->sw_dbg_stat->mem_alloc_fail_cnt;
		tmp_stats[i++] = sp->sw_dbg_stat->pci_map_fail_cnt;
		tmp_stats[i++] = sp->sw_dbg_stat->mem_allocated;
		tmp_stats[i++] = sp->sw_dbg_stat->mem_freed;
		tmp_stats[i++] = sp->sw_dbg_stat->link_up_cnt;
		tmp_stats[i++] = sp->sw_dbg_stat->link_down_cnt;
		if (sp->last_link_state == LINK_UP)
			stats->link_up_time
				= jiffies - sp->start_time;
		tmp_stats[i++] = sp->sw_dbg_stat->link_up_time;
		if (sp->last_link_state == LINK_DOWN)
			stats->link_down_time
				= jiffies - sp->start_time;
		tmp_stats[i++] = sp->sw_dbg_stat->link_down_time;
	}

#ifdef TITAN_LEGAY
	}
#endif
}
#endif

#ifdef SET_ETHTOOL_OPS
static int s2io_ethtool_get_regs_len(struct net_device *dev)
{
	return (XENA_REG_SPACE);
}


static u32 s2io_ethtool_get_rx_csum(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;

	return (sp->rx_csum);
}

static int s2io_ethtool_set_rx_csum(struct net_device *dev, u32 data)
{
	struct s2io_nic *sp = dev->priv;

	if (data)
		sp->rx_csum = 1;
	else
		sp->rx_csum = 0;

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 00)) || \
    (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23))
static int s2io_get_eeprom_len(struct net_device *dev)
{
	return (XENA_EEPROM_SPACE);
}
#endif

static int s2io_ethtool_self_test_count(struct net_device *dev)
{
	return (S2IO_TEST_LEN);
}

void add_print_string(char *prnt_string, int j, int *stat_size, u8 *data,
	struct s2io_nic *sp)
{
	char data_string[ETH_GSTRING_LEN] = {};
	sprintf(data_string, prnt_string, j);
	memcpy(data + *stat_size, data_string, ETH_GSTRING_LEN);
	*stat_size += ETH_GSTRING_LEN;
}

static void s2io_ethtool_get_strings(struct net_device *dev,
				     u32 stringset, u8 *data)
{
	int stat_size = 0, i;
	struct s2io_nic *sp = dev->priv;

	switch (stringset) {
	case ETH_SS_TEST:
		memcpy(data, s2io_gstrings, S2IO_STRINGS_LEN);
		break;
	case ETH_SS_STATS:
#ifdef TITAN_LEGACY
	if (sp->device_type == TITAN_DEVICE)
		memcpy(data, &ethtool_titan_stats_keys,
			sizeof(ethtool_titan_stats_keys));
	else
#endif
	{
		stat_size = sizeof(ethtool_xena_stats_keys);
		memcpy(data, &ethtool_xena_stats_keys, stat_size);

		if (sp->device_type == XFRAME_II_DEVICE) {
			memcpy(data + stat_size,
				&ethtool_enhanced_stats_keys,
				sizeof(ethtool_enhanced_stats_keys));
			stat_size += sizeof(ethtool_enhanced_stats_keys);
		}

		memcpy(data + stat_size,
			&ethtool_driver_stats_keys,
			sizeof(ethtool_driver_stats_keys));
		stat_size += sizeof(ethtool_driver_stats_keys);

#if defined (__VMKLNX__)
		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_pkt_cnt_%d",
				i, &stat_size, data, sp);
#endif /* defined (__VMKLNX__) */

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("fifo_full_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_buf_abort_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_desc_abort_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_parity_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_link_loss_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			add_print_string("tx_list_proc_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("ring_full_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_parity_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_abort_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_parity_abort_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_rda_fail_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_unkn_prot_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_fcs_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_buf_size_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_rxd_corrupt_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++)
			add_print_string("rx_unkn_err_cnt_%d",
				i, &stat_size, data, sp);

		for (i = 0; i < sp->config.rx_ring_num; i++) {
			add_print_string("lro_aggregated_pkts_%d\t\t",
				i, &stat_size, data, sp);
			add_print_string("lro_flush_both_count_%d\t\t",
				i, &stat_size, data, sp);
			add_print_string("lro_out_of_sequence_pkts_%d\t\t",
				i, &stat_size, data, sp);
			add_print_string("lro_flush_due_to_max_pkts_%d\t",
				i, &stat_size, data, sp);
			add_print_string("lro_avg_aggr_pkts_%d\t\t",
				i, &stat_size, data, sp);
			add_print_string("lro_max_pkts_aggr_%d\t\t",
				i, &stat_size, data, sp);
		}

		if (!sp->exec_mode)
			memcpy(data + stat_size,
				&ethtool_driver_dbg_stats_keys,
				sizeof(ethtool_driver_dbg_stats_keys));
		}
	}
}

static int s2io_ethtool_get_stats_count(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	int stat_count = 0;

	switch (sp->device_type) {

#ifdef TITAN_LEGACY
	case TITAN_DEVICE:
		stat_count = S2IO_TITAN_STAT_COUNT;
	break;
#endif
	case XFRAME_I_DEVICE:
		stat_count = XFRAME_I_STAT_LEN +
			(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
			(sp->config.rx_ring_num * NUM_RX_SW_STAT);
		if (!sp->exec_mode)
			stat_count += S2IO_DRIVER_DBG_STAT_LEN;
	break;

	case XFRAME_II_DEVICE:
		stat_count = XFRAME_II_STAT_LEN +
			(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
			(sp->config.rx_ring_num * NUM_RX_SW_STAT);
		if (!sp->exec_mode)
			stat_count += S2IO_DRIVER_DBG_STAT_LEN;
	break;
	}

	return stat_count;
}

static u32 s2io_ethtool_get_link(struct net_device *dev)
{
		u64 val64 = 0;
		struct s2io_nic *sp = dev->priv;
		struct XENA_dev_config __iomem *bar0 = sp->bar0;

		val64 = netif_carrier_ok(dev);
		if (val64) {
			/* Verify Adapter_Enable bit which automatically
			* transitions to 0 in the event of a link fault
			*/
			val64 = readq(&bar0->adapter_control);
		}
	return (val64 & ADAPTER_CNTL_EN) ? 1 : 0;
}

static int s2io_ethtool_op_set_tx_csum(struct net_device *dev, u32 data)
{
	struct s2io_nic *sp = dev->priv;

	if (data) {
		if (sp->device_type == XFRAME_II_DEVICE)
			dev->features |= NETIF_F_HW_CSUM;
		else
			dev->features |= NETIF_F_IP_CSUM;
	} else {
		dev->features &= ~NETIF_F_IP_CSUM;
		if (sp->device_type == XFRAME_II_DEVICE)
			dev->features &= ~NETIF_F_HW_CSUM;
	}

	return 0;
}


#ifdef NETIF_F_TSO6
static u32 s2io_ethtool_op_get_tso(struct net_device *dev)
{
	return (dev->features & NETIF_F_TSO) != 0;
}

static int s2io_ethtool_op_set_tso(struct net_device *dev, u32 data)
{
	if (data)
		dev->features |= (NETIF_F_TSO | NETIF_F_TSO6);
	else
		dev->features &= ~(NETIF_F_TSO | NETIF_F_TSO6);

	return 0;
}
#endif


#if defined (__VMKLNX__)
static int s2io_ethtool_get_coalesce(struct net_device *netdev,
				struct ethtool_coalesce *ec)
{
	struct s2io_nic *sp = netdev_priv(netdev);

	ec->rx_coalesce_usecs = sp->rx_coalesce_usecs;
	if (sp->tx_coalesce_usecs == 0) {
		// If not set, then the driver is not doing fixed ITR, so 
		// return the default boundary timer value of 4000 us.
		ec->tx_coalesce_usecs = S2IO_DEF_TX_COALESCE_USECS;
	}
	printk ("%s: tx coalesce settings  %d usecs for dev %s\n",
				__FUNCTION__, ec->tx_coalesce_usecs, netdev->name);
	return 0;
}

static int s2io_ethtool_set_coalesce(struct net_device *netdev,
				struct ethtool_coalesce *ec)
{
	struct s2io_nic *sp = netdev_priv(netdev);

	if (ec->tx_coalesce_usecs > 1) {
		sp->tx_coalesce_usecs = ec->tx_coalesce_usecs;
		printk ("%s: Setting tx coalesce settings to %d usecs for dev %s\n",
				__FUNCTION__, ec->tx_coalesce_usecs, netdev->name);
	}
	else {
		sp->tx_coalesce_usecs=0;
		printk ("%s: Reverting to driver default of 250 compl/sec, being set %d usecs for dev %s\n",
				__FUNCTION__, ec->tx_coalesce_usecs, netdev->name);
	}

	init_tti(sp);
	return 0;
}
#endif /* defined (__VMKLNX__) */

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_settings = s2io_ethtool_gset,
	.set_settings = s2io_ethtool_sset,
	.get_drvinfo = s2io_ethtool_gdrvinfo,
	.get_regs_len = s2io_ethtool_get_regs_len,
	.get_regs = s2io_ethtool_gregs,
	.get_link = s2io_ethtool_get_link,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 00)) || \
    (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23))
	.get_eeprom_len = s2io_get_eeprom_len,
#endif
	.get_eeprom = s2io_ethtool_geeprom,
	.set_eeprom = s2io_ethtool_seeprom,
	.get_ringparam = s2io_ethtool_gringparam,
	.get_pauseparam = s2io_ethtool_getpause_data,
	.set_pauseparam = s2io_ethtool_setpause_data,
	.get_rx_csum = s2io_ethtool_get_rx_csum,
	.set_rx_csum = s2io_ethtool_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = s2io_ethtool_op_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = s2io_ethtool_op_get_tso,
	.set_tso = s2io_ethtool_op_set_tso,
#endif
#ifdef NETIF_F_UFO
	.get_ufo = ethtool_op_get_ufo,
	.set_ufo = ethtool_op_set_ufo,
#endif
	.self_test_count = s2io_ethtool_self_test_count,
	.self_test = s2io_ethtool_test,
	.get_strings = s2io_ethtool_get_strings,
	.phys_id = s2io_ethtool_idnic,
	.get_stats_count = s2io_ethtool_get_stats_count,
#if defined (__VMKLNX__)
	.get_ethtool_stats = s2io_get_ethtool_stats,
	.get_coalesce = s2io_ethtool_get_coalesce,
	.set_coalesce = s2io_ethtool_set_coalesce
#else /* !defined (__VMKLNX__) */
	.get_ethtool_stats = s2io_get_ethtool_stats
#endif /* defined (__VMKLNX__) */
};
#endif

/**
 *  s2io_ioctl - Entry point for the Ioctl
 *  @dev :  Device pointer.
 *  @ifr :  An IOCTL specefic structure, that can contain a pointer to
 *  a proprietary structure used to pass information to the driver.
 *  @cmd :  This is used to distinguish between the different commands that
 *  can be passed to the IOCTL functions.
 *  Description:
 *  This function has support for ethtool, adding multiple MAC addresses on
 *  the NIC and some DBG commands for the util tool.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */

static int s2io_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct s2io_nic *sp = dev->priv;

	switch (cmd) {
#ifndef SET_ETHTOOL_OPS
	case SIOCETHTOOL:
		return s2io_ethtool(dev, rq);
#endif
	default:
#ifndef __VMKLNX__
		if (s2io_ioctl_util(sp, rq, cmd) < 0)
			return -EFAULT;
#else
		break;
#endif
	}
	return SUCCESS;
}

#if defined (__VMKLNX__)
static void 
s2io_set_rx_rings (struct s2io_nic *sp, 
						struct config_param *config)
{
	struct net_device *dev = sp->dev;

	config->rx_ring_num = rx_ring_num;

	if (rx_ring_num == 0) {
		if (sp->device_type == XFRAME_I_DEVICE)
			config->rx_ring_num = 1;
		else if (config->intr_type == MSI_X)
			config->rx_ring_num =  min((int)num_online_cpus(), MAX_RX_RINGS);
		else
			config->rx_ring_num = 1;
		if (dev->mtu > ETH_DATA_LEN)
			config->rx_ring_num = min((u32)4, config->rx_ring_num);
	}

	DBG_PRINT(ERR_DBG, "%s: Using %d Rx ring(s)\n", dev->name,
		  sp->config.rx_ring_num);

	if ((sp->device_type == XFRAME_II_DEVICE) &&
		(config->intr_type == MSI_X)) 
		sp->num_entries = config->rx_ring_num + 1;
}
#endif

int s2io_restart_card(struct s2io_nic *sp)
{
	int ret = 0;
	if (test_and_set_bit(__S2IO_STATE_RESET_CARD, &(sp->state)))
		return -1;
	s2io_card_down(sp);
#if defined (__VMKLNX__)
	s2io_set_rx_rings(sp, &sp->config);
#endif
	ret = s2io_card_up(sp);
	if (ret) {
		DBG_PRINT(ERR_DBG, "%s: Device bring up failed\n",
				__FUNCTION__);
		return ret;
	}
	clear_bit(__S2IO_STATE_RESET_CARD, &(sp->state));

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	/* Invalidate netqueue state as filters have been lost after reinit */
	vmknetddi_queueops_invalidate_state(sp->dev);
#endif /* defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__) */

	return ret;
}

/**
 *  s2io_change_mtu - entry point to change MTU size for the device.
 *   @dev : device pointer.
 *   @new_mtu : the new MTU size for the device.
 *   Description: A driver entry point to change MTU size for the device.
 *   Before changing the MTU the device must be stopped.
 *  Return value:
 *   0 on success and an appropriate (-)ve integer as defined in errno.h
 *   file on failure.
 */

static int s2io_change_mtu(struct net_device *dev, int new_mtu)
{
	struct s2io_nic *sp = dev->priv;
	int ret = 0;

	if ((new_mtu < MIN_MTU) || (new_mtu > MAX_PYLD_JUMBO)) {
		DBG_PRINT(ERR_DBG, "%s: MTU size is invalid.\n",
			  dev->name);
		return -EPERM;
	}

	dev->mtu = new_mtu;
	if (netif_running(dev)) {
#if defined(__VMKLNX__)
		// Do the following to ensure that s2io reset does not trip NetWatchDogTimeout
		// as chip re-init takes upto a couple of seconds. see PR 284806
		sp->dev->trans_start=jiffies;
#else
		s2io_stop_all_tx_queue(sp);
#endif

		ret = s2io_restart_card(sp);
		if (ret)
			return ret;
		s2io_wake_all_tx_queue(sp);
	} else { /* Device is down */
		struct XENA_dev_config __iomem *bar0 = sp->bar0;
		u64 val64 = new_mtu;

		writeq(vBIT(val64, 2, 14), &bar0->rmac_max_pyld_len);
	}

	DBG_PRINT(ERR_DBG, "%s: MTU is changed to %d\n", dev->name, new_mtu);

	return ret;
}

/**
 * s2io_set_link - Set the LInk status
 * @data: long pointer to device private structue
 * Description: Sets the link status for the adapter
 */

static void s2io_set_link(struct work_struct *work)
{
	struct s2io_nic *nic = container_of(work, struct s2io_nic,
			set_link_task);
	struct net_device *dev = nic->dev;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64;
	u16 subid;

	if (!netif_running(dev))
		return;

	if (test_and_set_bit(__S2IO_STATE_LINK_RESET_TASK, &(nic->state))) {
		/* The card is being reset, no point doing anything */
		return;
	}

	subid = nic->pdev->subsystem_device;
	if (s2io_link_fault_indication(nic) == MAC_RMAC_ERR_TIMER) {
		/*
		 * Allow a small delay for the NICs self initiated
		 * cleanup to complete.
		 */
		msleep(100);
	}
	val64 = readq(&bar0->adapter_status);
	if (LINK_IS_UP(val64)) {
		if (nic->device_type == XFRAME_I_DEVICE) {
			if (verify_xena_quiescence(nic)) {
				val64 = readq(&bar0->adapter_control);
				val64 |= ADAPTER_CNTL_EN;
				writeq(val64, &bar0->adapter_control);

				if (CARDS_WITH_FAULTY_LINK_INDICATORS(
					nic->device_type, subid)) {
					val64 = readq(&bar0->gpio_control);
					val64 |= GPIO_CTRL_GPIO_0;
					writeq(val64, &bar0->gpio_control);
					val64 = readq(&bar0->gpio_control);
				} else {
					val64 |= ADAPTER_LED_ON;
					writeq(val64, &bar0->adapter_control);
				}
				nic->device_enabled_once = TRUE;
			} else {
				DBG_PRINT(ERR_DBG, "%s: Error: ", dev->name);
				DBG_PRINT(ERR_DBG,
					"device is not Quiescent\n");
				s2io_stop_all_tx_queue(nic);
			}
		} else {
			val64 = readq(&bar0->adapter_control);
			val64 |= ADAPTER_CNTL_EN;
			writeq(val64, &bar0->adapter_control);
			val64 |= ADAPTER_LED_ON;
			writeq(val64, &bar0->adapter_control);
			nic->device_enabled_once = TRUE;
		}
		s2io_link(nic, LINK_UP);
	} else {
		if (CARDS_WITH_FAULTY_LINK_INDICATORS(nic->device_type,
				subid)) {
			val64 = readq(&bar0->gpio_control);
			val64 &= ~GPIO_CTRL_GPIO_0;
			writeq(val64, &bar0->gpio_control);
			val64 = readq(&bar0->gpio_control);
		}
		/* turn off LED */
		val64 = readq(&bar0->adapter_control);
		val64 = val64 & (~ADAPTER_LED_ON);
		writeq(val64, &bar0->adapter_control);
		s2io_link(nic, LINK_DOWN);
	}
	clear_bit(__S2IO_STATE_LINK_RESET_TASK, &(nic->state));
}

static int set_rxd_buffer_pointer(struct s2io_nic *sp, struct RxD_t *rxdp,
				struct buffAdd *ba,
				struct sk_buff **skb, u64 *temp0, u64 *temp1,
				u64 *temp2, u64 *temp3, u64 *temp4, int size,
				int ring_no, int rxd_index)
{
	struct net_device *dev = sp->dev;
	struct sk_buff *frag_list;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct RxD5 *rxdp5;
	struct swDbgStat *stats = sp->sw_dbg_stat;

	if (sp->rxd_mode == RXD_MODE_5) {
		if (((struct RxD5 *)rxdp)->Host_Control)
			return 0;
	} else
		if (rxdp->Host_Control)
			return 0;

	switch (sp->rxd_mode) {
	case RXD_MODE_1:
		/* allocate skb */
		rxdp1 = (struct RxD1 *)rxdp;
		if (*skb) {
			DBG_PRINT(INFO_DBG, "SKB is not NULL\n");
			/*
			* As Rx frame are not going to be processed,
			* using same mapped address for the Rxd
			* buffer pointer
			*/
			rxdp1->Buffer0_ptr = *temp0;
		} else {
			*skb = netdev_alloc_skb(dev, size);
			if (!(*skb)) {
				DBG_PRINT(INFO_DBG, "%s: Out of ", dev->name);
				DBG_PRINT(INFO_DBG, "memory to allocate ");
				DBG_PRINT(INFO_DBG, "1 buf mode SKBs\n");
				stats->mem_alloc_fail_cnt++;
				return -ENOMEM ;
			}
			stats->mem_allocated +=	(*skb)->truesize;
			/* storing the mapped addr in a temp variable
			 * such it will be used for next rxd whose
			 * Host Control is NULL
			 */
			rxdp1->Buffer0_ptr = *temp0 =
				pci_map_single( sp->pdev, (*skb)->data,
				size - NET_IP_ALIGN,
				PCI_DMA_FROMDEVICE);
			if ((rxdp1->Buffer0_ptr == 0) ||
				(rxdp1->Buffer0_ptr == DMA_ERROR_CODE)) {
				stats->pci_map_fail_cnt++;
				stats->mem_freed += (*skb)->truesize;
				dev_kfree_skb(*skb);
				return -ENOMEM;
			}
			rxdp->Host_Control = (unsigned long) (*skb);
		}
		break;

	case RXD_MODE_5:
		rxdp5 = (struct RxD5 *)rxdp;
		if (*skb) {
			rxdp5->Buffer0_ptr = *temp0;
			rxdp5->Buffer1_ptr = *temp1;
			rxdp5->Buffer2_ptr = *temp2;
			rxdp5->Buffer3_ptr = *temp3;
			rxdp5->Buffer4_ptr = *temp4;
		} else {
			/* Allocate Buffer 0 */
			*skb = netdev_alloc_skb(dev, size);
			if (!(*skb)) {
				DBG_PRINT(INFO_DBG, "%s: Out of ", dev->name);
				DBG_PRINT(INFO_DBG, "memory to allocate ");
				DBG_PRINT(INFO_DBG, "5 buf mode SKBs\n");
				stats->mem_alloc_fail_cnt++;
				return -ENOMEM ;
			}

			stats->mem_allocated +=	(*skb)->truesize;
			rxdp5->Buffer0_ptr = *temp0 =
				pci_map_single(sp->pdev, ba->ba_0, BUF0_LEN,
				PCI_DMA_FROMDEVICE);
			if ((rxdp5->Buffer0_ptr == 0) ||
				(rxdp5->Buffer0_ptr == DMA_ERROR_CODE))
					goto free_5b_0;

			/* Buffer-1 receives L3/L4 headers */
			rxdp5->Buffer1_ptr = *temp1 =
				pci_map_single( sp->pdev, (*skb)->data,
				l3l4hdr_size + 4,
				PCI_DMA_FROMDEVICE);
			if ((rxdp5->Buffer1_ptr == 0) ||
				(rxdp5->Buffer1_ptr == DMA_ERROR_CODE))
					goto free_5b_1;
			/*
			 * skb_shinfo(skb)->frag_list will have L4
			 * data payload
			 */
			skb_shinfo(*skb)->frag_list =
				netdev_alloc_skb(dev, MODE5_BUF_SIZE + ALIGN_SIZE);
			if (skb_shinfo(*skb)->frag_list == NULL) {
				DBG_PRINT(INFO_DBG, "%s: dev_alloc_skb \
				failed\n ", dev->name);
				stats->mem_alloc_fail_cnt++;
				goto free_5b_2;
			}
			frag_list = skb_shinfo(*skb)->frag_list;
			frag_list->next = NULL;
			(*skb)->truesize += frag_list->truesize;
			stats->mem_allocated += frag_list->truesize;
			/*
			 * Buffer-2 receives L4 data payload
			 */
			rxdp5->Buffer2_ptr = *temp2 =
				pci_map_single( sp->pdev, frag_list->data,
					MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
			if ((rxdp5->Buffer2_ptr == 0) ||
				(rxdp5->Buffer2_ptr == DMA_ERROR_CODE))
					goto free_5b_2;

			if (sp->skbs_per_rxd > 2) {
				/*
				 * frag_list->next will have L4
				 * data payload
				 */
				frag_list->next =
					netdev_alloc_skb(dev, MODE5_BUF_SIZE +
						ALIGN_SIZE);
				if (frag_list->next == NULL) {
					DBG_PRINT(ERR_DBG, "%s: dev_alloc_skb \
						failed\n ", dev->name);
					stats->mem_alloc_fail_cnt++;
					goto free_5b_3;
				}

				frag_list = frag_list->next;
				frag_list->next = NULL;
				(*skb)->truesize += frag_list->truesize;
				stats->mem_allocated += frag_list->truesize;

				/*
				 * Buffer-3 receives L4 data payload
				 */
				rxdp5->Buffer3_ptr = *temp3 =
					pci_map_single(sp->pdev,
						frag_list->data, MODE5_BUF_SIZE,
						PCI_DMA_FROMDEVICE);
				if ((rxdp5->Buffer3_ptr == 0) ||
				    (rxdp5->Buffer3_ptr == DMA_ERROR_CODE)) {
						frag_list =
						    skb_shinfo(*skb)->frag_list;
						goto free_5b_3;
				}
			} else
				rxdp5->Buffer3_ptr = *temp3 =
					rxdp5->Buffer2_ptr;

			if (sp->skbs_per_rxd > 3) {
				frag_list->next = netdev_alloc_skb(dev, MODE5_BUF_SIZE +
								ALIGN_SIZE);
				if (skb_shinfo(*skb)->frag_list == NULL) {
					DBG_PRINT(INFO_DBG, "%s: dev_alloc_skb \
					failed\n ", dev->name);
					stats->mem_alloc_fail_cnt++;
					frag_list = skb_shinfo(*skb)->frag_list;
					goto free_5b_4;
				}
				frag_list->next = NULL;
				(*skb)->truesize += frag_list->truesize;
				stats->mem_allocated +=
					frag_list->truesize;

				/* Get physical address for buffer 4 */
				rxdp5->Buffer4_ptr = *temp4 =
					pci_map_single(sp->pdev,
						frag_list->data,
						MODE5_BUF_SIZE,
						PCI_DMA_FROMDEVICE);
				if ((rxdp5->Buffer4_ptr == 0) ||
					(rxdp5->Buffer4_ptr ==
						DMA_ERROR_CODE)) {
						frag_list =
						  skb_shinfo(*skb)->frag_list;
						goto free_5b_4;
				}
			} else
				rxdp5->Buffer4_ptr = *temp4 =
					rxdp5->Buffer3_ptr;

			rxdp5->Host_Control = rxd_index;
			sp->mac_control.rings[ring_no].skbs[rxd_index] =
				(unsigned long) (*skb);
			break;

free_5b_4:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)frag_list->next->data,
			MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
free_5b_3:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)frag_list->data,
			MODE5_BUF_SIZE, PCI_DMA_FROMDEVICE);
free_5b_2:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)(*skb)->data,
			l3l4hdr_size + 4, PCI_DMA_FROMDEVICE);
free_5b_1:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)ba->ba_0,
			BUF0_LEN, PCI_DMA_FROMDEVICE);
free_5b_0:
		stats->pci_map_fail_cnt++;
		stats->mem_freed += (*skb)->truesize;
		dev_kfree_skb(*skb);
		return -ENOMEM;
		}
	break;

	case RXD_MODE_3B: /* Two buffer Mode */
		rxdp3 = (struct RxD3 *)rxdp;
		if (*skb) {
			rxdp3->Buffer2_ptr = *temp2;
			rxdp3->Buffer0_ptr = *temp0;
			rxdp3->Buffer1_ptr = *temp1;
		} else {
			*skb = netdev_alloc_skb(dev, size);
			if (!(*skb)) {
				DBG_PRINT(INFO_DBG, "%s: Out of ", dev->name);
				DBG_PRINT(INFO_DBG, "memory to allocate ");
				DBG_PRINT(INFO_DBG, "2 buf mode SKBs\n");
				stats->mem_alloc_fail_cnt++;
				return -ENOMEM ;
			}

			rxdp3->Buffer0_ptr = *temp0 =
				pci_map_single(sp->pdev, ba->ba_0, BUF0_LEN,
				PCI_DMA_FROMDEVICE);
			if ((rxdp3->Buffer0_ptr == 0) ||
				(rxdp3->Buffer0_ptr == DMA_ERROR_CODE))
					goto free_3b_0;

			/* Buffer-1 will be dummy buffer not used */
			rxdp3->Buffer1_ptr = *temp1 =
				pci_map_single(sp->pdev, ba->ba_1, BUF1_LEN,
				PCI_DMA_FROMDEVICE);
			if ((rxdp3->Buffer1_ptr == 0) ||
				(rxdp3->Buffer1_ptr == DMA_ERROR_CODE))
					goto free_3b_1;

			stats->mem_allocated += (*skb)->truesize;
			rxdp3->Buffer2_ptr = *temp2 =
				pci_map_single(sp->pdev, (*skb)->data,
				dev->mtu + 4,
				PCI_DMA_FROMDEVICE);
			if ((rxdp3->Buffer2_ptr == 0) ||
				(rxdp3->Buffer2_ptr == DMA_ERROR_CODE))
					goto free_3b_2;

			rxdp->Host_Control = (unsigned long) (*skb);
			break;

free_3b_2:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)ba->ba_1,
			BUF1_LEN, PCI_DMA_FROMDEVICE);
free_3b_1:
		stats->pci_map_fail_cnt++;
		pci_unmap_single(sp->pdev,
			(dma_addr_t)(unsigned long)ba->ba_0,
			BUF0_LEN, PCI_DMA_FROMDEVICE);
free_3b_0:
		stats->pci_map_fail_cnt++;
		stats->mem_freed += (*skb)->truesize;
		dev_kfree_skb(*skb);
		return -ENOMEM;
		}
	break;
	}
	return 0;
}

static void set_rxd_buffer_size(struct s2io_nic *sp, struct RxD_t *rxdp,
		int size)
{
	struct net_device *dev = sp->dev;
	struct RxD5 *rxdp5;

	switch (sp->rxd_mode) {
	case RXD_MODE_1:
		rxdp->Control_2 = SET_BUFFER0_SIZE_1( size - NET_IP_ALIGN);
		break;

	case RXD_MODE_5:
		rxdp5 = (struct RxD5 *)rxdp;
		rxdp->Control_2	&= (~MASK_BUFFER0_SIZE_5);
		rxdp->Control_2	|= SET_BUFFER0_SIZE_5(BUF0_LEN);
		rxdp->Control_2	&= (~MASK_BUFFER1_SIZE_5);
		rxdp->Control_2	|= SET_BUFFER1_SIZE_5(l3l4hdr_size + 4);
		rxdp->Control_2	&= (~MASK_BUFFER2_SIZE_5);
		rxdp->Control_2	|= SET_BUFFER2_SIZE_5(MODE5_BUF_SIZE);
		rxdp5->Control_3	&= (~MASK_BUFFER3_SIZE_5);
		if (sp->skbs_per_rxd > 2)
			rxdp5->Control_3 |=
				SET_BUFFER3_SIZE_5(MODE5_BUF_SIZE);
		else
			rxdp5->Control_3 |= SET_BUFFER3_SIZE_5(1);
		rxdp5->Control_3	&= (~MASK_BUFFER4_SIZE_5);
		if (sp->skbs_per_rxd > 3)
			rxdp5->Control_3 |=
				SET_BUFFER4_SIZE_5(MODE5_BUF_SIZE);
		else
			rxdp5->Control_3 |= SET_BUFFER4_SIZE_5(1);
		break;

	case RXD_MODE_3B:
		rxdp->Control_2 = SET_BUFFER0_SIZE_3(BUF0_LEN);
		rxdp->Control_2 |= SET_BUFFER1_SIZE_3(1);
		rxdp->Control_2 |= SET_BUFFER2_SIZE_3( dev->mtu + 4);
		break;

	}
}

static  int rxd_owner_bit_reset(struct s2io_nic *sp)
{
	int i, j, k, blk_cnt = 0, size;
	struct mac_info *mac_control = &sp->mac_control;
	struct config_param *config = &sp->config;
	struct net_device *dev = sp->dev;
	struct RxD_t *rxdp = NULL;
	struct sk_buff *skb = NULL;
	struct buffAdd *ba = NULL;
	u64 temp0_64 = 0, temp1_64 = 0, temp2_64 = 0;
	u64 temp3_64 = 0, temp4_64 = 0;
	int	rxd_index = 0;

	/* Calculate the size based on ring mode */
	size = dev->mtu + HEADER_ETHERNET_II_802_3_SIZE +
		HEADER_802_2_SIZE + HEADER_SNAP_SIZE;
	if (sp->rxd_mode == RXD_MODE_1)
		size += NET_IP_ALIGN;
	else if (sp->rxd_mode == RXD_MODE_3B)
		size = dev->mtu + ALIGN_SIZE + BUF0_LEN + 4;
	else
		size = l3l4hdr_size + ALIGN_SIZE + BUF0_LEN + 4;

	for (i = 0; i < config->rx_ring_num; i++) {
		blk_cnt = config->rx_cfg[i].num_rxd /
			(rxd_count[sp->rxd_mode] +1);

		for (j = 0; j < blk_cnt; j++) {
			for (k = 0; k < rxd_count[sp->rxd_mode]; k++) {
				rxdp = mac_control->rings[i].
					rx_blocks[j].rxds[k].virt_addr;

				rxd_index = k + 1;
				if (j)
					rxd_index +=
						(j * rxd_count[sp->rxd_mode]);

				if (sp->rxd_mode >= RXD_MODE_3B)
					ba = &mac_control->rings[i].ba[j][k];
				if (set_rxd_buffer_pointer(sp, rxdp, ba,
						&skb, (u64 *)&temp0_64,
						(u64 *)&temp1_64,
						(u64 *)&temp2_64,
						(u64 *)&temp3_64,
						(u64 *)&temp4_64,
						size, i, rxd_index) == -ENOMEM)
					return 0;

				set_rxd_buffer_size(sp, rxdp, size);
				wmb();
				/* flip the Ownership bit to Hardware */
				rxdp->Control_1 |= RXD_OWN_XENA;
			}
		}
	}
	return 0;

}

static int s2io_add_isr(struct s2io_nic *sp)
{
	struct net_device *dev = sp->dev;
	int err = 0;

#ifdef CONFIG_PCI_MSI
	int ret = 0;
	if (sp->config.intr_type == MSI_X)
		ret = s2io_enable_msi_x(sp);

	if (ret) {
		DBG_PRINT(ERR_DBG, "%s: Defaulting to INTA\n", dev->name);
		sp->config.intr_type = INTA;
	}

	/* Store the values of the MSIX table in the s2io_nic structure */
	store_xmsi_data(sp);

	/* After proper initialization of H/W, register ISR */
	if (sp->config.intr_type == MSI_X) {
		int i, msix_rx_cnt = 0;

		for (i = 0; i < sp->num_entries; i++) {
			if(sp->s2io_entries[i].in_use == MSIX_FLG) {
				if (sp->s2io_entries[i].type ==
					MSIX_RING_TYPE) {
					sprintf(sp->desc[i], "%s:MSI-X-%d-RX",
						dev->name, i);
					err = request_irq(sp->entries[i].vector,
						s2io_msix_ring_handle, 0, sp->desc[i],
						sp->s2io_entries[i].arg);
				} else if (sp->s2io_entries[i].type ==
					MSIX_ALARM_TYPE) {
					sprintf(sp->desc[i], "%s:MSI-X-%d-TX",
					dev->name, i);
					err = request_irq(sp->entries[i].vector,
						s2io_msix_fifo_handle, 0, sp->desc[i],
						sp->s2io_entries[i].arg);

				}
				/* if either data or addr is zero print it. */
				if (!(sp->msix_info[i].addr &&
					sp->msix_info[i].data)) {
 					DBG_PRINT(ERR_DBG,
 					"%s @ Addr:0x%llx Data:0x%llx\n",
					sp->desc[i],
					(unsigned long long)
						sp->msix_info[i].addr,
					(unsigned long long)
						ntohl(sp->msix_info[i].data));
				} else
					msix_rx_cnt++;
				if (err) {
					do_rem_msix_isr(sp);

					DBG_PRINT(ERR_DBG,
						"%s:MSI-X-%d registration "
						"failed\n", dev->name, i);

					DBG_PRINT(ERR_DBG,
						"%s: Defaulting to INTA\n",
						dev->name);
					sp->config.intr_type = INTA;
					break;
				}
				sp->s2io_entries[i].in_use =
					MSIX_REGISTERED_SUCCESS;
			}
		}
		if (!err) {
			DBG_PRINT(INFO_DBG, "MSI-X-RX %d entries enabled\n",
				  --msix_rx_cnt);
			DBG_PRINT(INFO_DBG, "MSI-X-TX entries enabled"
						" through alarm vector\n");
		}
	}
#endif
	if (sp->config.intr_type == INTA) {
		err = request_irq((int) sp->pdev->irq,
				s2io_isr, IRQF_SHARED,
				sp->name, dev);
		if (err) {
			DBG_PRINT(ERR_DBG, "%s: ISR registration failed\n",
				  dev->name);
			return -1;
		}
	}
	return 0;
}

static void s2io_rem_isr(struct s2io_nic *sp)
{
#ifdef CONFIG_PCI_MSI
	if (sp->config.intr_type == MSI_X)
		do_rem_msix_isr(sp);
	else
#endif
	do_rem_inta_isr(sp);
}

#if !defined (__VMKLNX__)
static void do_s2io_card_down(struct s2io_nic *sp, int do_io)
{
	int cnt;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	register u64 val64 = 0;
	struct config_param *config = &sp->config;

	if (!is_s2io_card_up(sp))
		return;

	del_timer_sync(&sp->alarm_timer);

	/* If s2io_set_link task/restart task is executing,
	 * wait till it completes. */
	while (test_and_set_bit(__S2IO_STATE_LINK_RESET_TASK, &(sp->state)))
		msleep(50);

	clear_bit(__S2IO_STATE_CARD_UP, &sp->state);

	/* Disable napi */
	if (config->napi) {
		if (config->intr_type == MSI_X) {
			for (cnt=0; cnt < config->rx_ring_num; cnt++)
				napi_disable(&sp->mac_control.rings[cnt].napi);
		}
		else
			napi_disable(&sp->napi);
	}

	/* disable Tx and Rx traffic on the NIC */
	if (do_io)
		stop_nic(sp);

	s2io_rem_isr(sp);

#if defined(__VMKLNX__)
	// Do the following to ensure that s2io reset does not trip NetWatchDogTimeout
	// as chip re-init takes upto a couple of seconds. see PR 284806
	s2io_link(sp, LINK_DOWN);
#endif
	cnt = 0;
	/* Check if the device is Quiescent and then Reset the NIC */
	while (do_io) {
		/* As per the HW requirement we need to replenish the
		 * receive buffer to avoid the ring bump. Since there is
		 * no intention of processing the Rx frame at this pointwe are
		 * just settting the ownership bit of rxd in Each Rx
		 * ring to HW and set the appropriate buffer size
		 * based on the ring mode
		 */
		rxd_owner_bit_reset(sp);

		val64 = readq(&bar0->adapter_status);
		if (verify_xena_quiescence(sp)) {
			if (verify_pcc_quiescent(sp, sp->device_enabled_once))
				break;
		}

		msleep(50);
		cnt++;
		if (cnt == 10) {
			DBG_PRINT(ERR_DBG,
				  "%s:Device not Quiescent ", __FUNCTION__);
			DBG_PRINT(ERR_DBG, "adaper status reads 0x%llx\n",
				(unsigned long long) val64);
			break;
		}
	}

	if (do_io)
		s2io_reset(sp);

	/* Free all Tx buffers */
	free_tx_buffers(sp);

	/* Free all Rx buffers */
	free_rx_buffers(sp);

	clear_bit(__S2IO_STATE_LINK_RESET_TASK, &(sp->state));
}
#else /* defined (__VMKLNX__) */
static void s2io_card_down_complete (struct s2io_nic *sp, int do_io)
{
	struct config_param *config = &sp->config;
	int cnt;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	register u64 val64 = 0;

	clear_bit(__S2IO_STATE_CARD_UP, &sp->state);

	/* Disable napi */
	if (config->napi) {
		if (config->intr_type == MSI_X) {
			for (cnt=0; cnt < config->rx_ring_num; cnt++)
				napi_disable(&sp->mac_control.rings[cnt].napi);
		}
		else
			napi_disable(&sp->napi);
	}

	/* disable Tx and Rx traffic on the NIC */
	if (do_io)
		stop_nic(sp);

	s2io_rem_isr(sp);

#if defined(__VMKLNX__)
	// Do the following to ensure that s2io reset does not trip NetWatchDogTimeout
	// as chip re-init takes upto a couple of seconds. see PR 284806
	s2io_link(sp, LINK_DOWN);
#endif
	cnt = 0;
	/* Check if the device is Quiescent and then Reset the NIC */
	while (do_io) {
		/* As per the HW requirement we need to replenish the
		 * receive buffer to avoid the ring bump. Since there is
		 * no intention of processing the Rx frame at this pointwe are
		 * just settting the ownership bit of rxd in Each Rx
		 * ring to HW and set the appropriate buffer size
		 * based on the ring mode
		 */
		rxd_owner_bit_reset(sp);

		val64 = readq(&bar0->adapter_status);
		if (verify_xena_quiescence(sp)) {
			if (verify_pcc_quiescent(sp, sp->device_enabled_once))
				break;
		}

		msleep(50);
		cnt++;
		if (cnt == 10) {
			u16 pci_cmd = 0, pcix_cmd = 0;
			/* Disable the pci bus master so dma's do not occur while resetting 
			 * the device.
			 */
			pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
				&(pcix_cmd));
			pci_write_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
		      (pcix_cmd & ~PCI_COMMAND_MASTER));

			msleep(50);
			DBG_PRINT(ERR_DBG,
				  "%s:Device not Quiescent. DMA bus master turned OFF before"
				  "reset ", __FUNCTION__);
			DBG_PRINT(ERR_DBG, "adaper status reads 0x%llx\n",
				(unsigned long long) val64);
			break;
		}
	}

	if (do_io)
		s2io_reset(sp);

	/* Free all Tx buffers */
	free_tx_buffers(sp);

	/* Free all Rx buffers */
	free_rx_buffers(sp);
}

static void do_s2io_card_down(struct s2io_nic *sp, int do_io)
{

	if (!is_s2io_card_up(sp)) {
		return;
	}

	del_timer_sync(&sp->alarm_timer);


	/* If s2io_set_link task/restart task is executing,
	 * wait till it completes. */
	printk ("%s: Doing card down for %s\n", __FUNCTION__, sp->dev->name);

	while (test_and_set_bit(__S2IO_STATE_LINK_RESET_TASK, &(sp->state)))
		msleep(50);

	printk ("%s: Acquired _LINK_RESET_TASK flag for %s\n", __FUNCTION__, sp->dev->name);

	s2io_card_down_complete (sp, do_io);

	clear_bit(__S2IO_STATE_LINK_RESET_TASK, &(sp->state));
}
#endif /* !defined (__VMKLNX__) */

static void s2io_card_down(struct s2io_nic *sp)
{
	do_s2io_card_down(sp, 1);
}

static int s2io_card_up(struct s2io_nic *sp)
{
	int i, ret = 0;
	struct mac_info *mac_control;
	struct config_param *config;
	struct net_device *dev = (struct net_device *) sp->dev;
	u16 interruptible;

	/* Initialize the H/W I/O registers */
	ret = init_nic(sp);
	if (ret != 0) {
		DBG_PRINT(ERR_DBG, "%s: H/W initialization failed\n",
			dev->name);
		if (ret != -EIO)
			s2io_reset(sp);
		return ret;
	}

	/*
	 * Initializing the Rx buffers. For now we are considering only 1
	 * Rx ring and initializing buffers into 30 Rx blocks
	 */
	mac_control = &sp->mac_control;
	config = &sp->config;

	sp->skbs_per_rxd = 1;
	if (sp->rxd_mode == RXD_MODE_5) {

		if (dev->mtu <= MODE5_BUF_SIZE)
			sp->skbs_per_rxd = 2; /* l3l4 Header + Payload */
		else
			if (dev->mtu <= (2 * MODE5_BUF_SIZE))
				sp->skbs_per_rxd = 3; /* Header+2*Payload */
			else
				sp->skbs_per_rxd = 4; /* Header+3*Payload */
	}

	for (i = 0; i < config->rx_ring_num; i++) {
		ret = fill_rx_buffers(&mac_control->rings[i]);
		if (ret) {
			DBG_PRINT(ERR_DBG, "%s - %s: Out of memory in Open\n",
				__FUNCTION__, dev->name);
			// Would like to continue as this might be just a temporary condition.
#if !defined (__VMKLNX__)
			s2io_reset(sp);
			free_rx_buffers(sp);
			return -ENOMEM;
#endif
		}
		DBG_PRINT(INFO_DBG, "Buf in ring:%d is %d:\n", i,
			  mac_control->rings[i].rx_bufs_left);
	}

	/* Initialize napi */
	if (config->napi) {
		if (config->intr_type == MSI_X) {
			for (i = 0; i < sp->config.rx_ring_num; i++)
				napi_enable(&sp->mac_control.rings[i].napi);
		}
		else
			napi_enable(&sp->napi);
	}

	/* Maintain the state prior to the open */
	if (sp->promisc_flg)
		sp->promisc_flg = 0;
	if (sp->m_cast_flg) {
		sp->m_cast_flg = 0;
		sp->all_multi_pos = 0;
	}

	/* Setting its receive mode */
	s2io_set_multicast(dev);

	if (sp->lro) {
		/* Initialize max aggregatable pkts per session based on MTU */
		sp->lro_max_aggr_per_sess = (lro_max_bytes - 1) / dev->mtu;

		if (sp->lro_max_aggr_per_sess < MIN_LRO_PACKETS)
			sp->lro_max_aggr_per_sess = MIN_LRO_PACKETS;

		if (sp->lro_max_aggr_per_sess > MAX_LRO_PACKETS)
			sp->lro_max_aggr_per_sess = MAX_LRO_PACKETS;
	}

	/* Enable Rx Traffic and interrupts on the NIC */
	if (start_nic(sp)) {
		DBG_PRINT(ERR_DBG, "%s: Starting NIC failed\n", dev->name);
		s2io_reset(sp);
		free_rx_buffers(sp);
		return -ENODEV;
	}

	/* Add interrupt service routine */
	if (s2io_add_isr(sp) != 0) {
		s2io_reset(sp);
		free_rx_buffers(sp);
		return -ENODEV;
	}

	sp->chk_dte_count=0;
	sp->chk_device_error_count = 0;
	sp->chk_rx_queue_count = 0;

	S2IO_TIMER_CONF(sp->alarm_timer, s2io_alarm_handle, sp, (HZ/2));

	set_bit(__S2IO_STATE_CARD_UP, &sp->state);


	/*  Enable select interrupts */
	en_dis_err_alarms(sp, ENA_ALL_INTRS, ENABLE_INTRS);
	if (sp->config.intr_type != INTA) {
		interruptible = TX_TRAFFIC_INTR | TX_PIC_INTR;
		en_dis_able_nic_intrs(sp, interruptible, ENABLE_INTRS);
	} else {
		interruptible = TX_TRAFFIC_INTR | RX_TRAFFIC_INTR;
		interruptible |= TX_PIC_INTR;
		en_dis_able_nic_intrs(sp, interruptible, ENABLE_INTRS);
	}
	return 0;
}

/**
 * s2io_restart_nic - Resets the NIC.
 * @data : long pointer to the device private structure
 * Description:
 * This function is scheduled to be run by the s2io_tx_watchdog
 * function after 0.5 secs to reset the NIC. The idea is to reduce
 * the run time of the watch dog routine which is run holding a
 * spin lock.
 */

static void s2io_restart_nic(struct work_struct *work)
{
	struct s2io_nic *sp = container_of(work, struct s2io_nic,
		rst_timer_task);
	struct net_device *dev = sp->dev;
	int ret = 0;

	if (!netif_running(dev))
		return;

	ret = s2io_restart_card(sp);
	if(ret)
		return;

	s2io_wake_all_tx_queue(sp);

	sp->mac_stat_info->soft_reset_cnt++;
}

/**
 *  s2io_tx_watchdog - Watchdog for transmit side.
 *  @dev : Pointer to net device structure
 *  Description:
 *  This function is triggered if the Tx Queue is stopped
 *  for a pre-defined amount of time when the Interface is still up.
 *  If the Interface is jammed in such a situation, the hardware is
 *  reset (by s2io_close) and restarted again (by s2io_open) to
 *  overcome any problem that might have been caused in the hardware.
 *  Return value:
 *  void
 */

static void s2io_tx_watchdog(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;
	/* Donot reset in debug mode */
	if (netif_carrier_ok(dev)) {
		sp->mac_stat_info->watchdog_timer_cnt++;
		if (sp->exec_mode) {
#if defined (__VMKLNX__)
			printk("%s: Scheduling RESET task for %s\n",__FUNCTION__,dev->name);
#endif
			schedule_work(&sp->rst_timer_task);
		}
	}
}

/**
 *   rx_osm_handler - To perform some OS related operations on SKB.
 *   @sp: private member of the device structure,pointer to s2io_nic structure.
 *   @skb : the socket buffer pointer.
 *   @len : length of the packet
 *   @cksum : FCS checksum of the frame.
 *   @ring_no : the ring from which this RxD was extracted.
 *   Description:
 *   This function is called by the Rx interrupt serivce routine to perform
 *   some OS related operations on the SKB before passing it to the upper
 *   layers. It mainly checks if the checksum is OK, if so adds it to the
 *   SKBs cksum variable, increments the Rx packet count and passes the SKB
 *   to the upper layer. If the checksum is wrong, it increments the Rx
 *   packet error count, frees the SKB and returns error.
 *   Return value:
 *   SUCCESS on success and -1 on failure.
 */
static int rx_osm_handler(struct ring_info *ring, struct RxD_t *rxdp)
{
	struct s2io_nic *sp = ring->nic;
	struct net_device *dev = (struct net_device *) ring->dev;
	struct sk_buff *skb = NULL;
	int ring_no = ring->ring_no;
	u16 l3_csum, l4_csum;
	u8 err = rxdp->Control_1 & RXD_T_CODE;
	struct lro *lro = NULL;
	struct swDbgStat *stats = sp->sw_dbg_stat;
	struct swLROStat *lro_stats = &ring->rx_ring_stat->sw_lro_stat;

	if (ring->rxd_mode == RXD_MODE_5)
		skb = (struct sk_buff *) \
				((unsigned long)ring->skbs[((unsigned long) \
					((struct RxD5 *)rxdp)->Host_Control)]);
	else
		skb = (struct sk_buff *)
				((unsigned long)rxdp->Host_Control);

#ifndef __VMKLNX__
#ifdef virt_addr_valid
	if (!virt_addr_valid(skb)) {
		DBG_PRINT(INFO_DBG, "%s: not valid SKB: %p\n",
			dev->name, skb);
		return 0;
	}
#endif
#endif
	skb->dev = dev;
	err = GET_RXD_T_CODE(rxdp->Control_1);
	if (err) {
		switch (err) {
		case 1:
			ring->rx_ring_stat->rx_parity_err_cnt++;
			break;

		case 2:
			ring->rx_ring_stat->rx_abort_cnt++;
			break;

		case 3:
			ring->rx_ring_stat->rx_parity_abort_cnt++;
			break;

		case 4:
			 ring->rx_ring_stat->rx_rda_fail_cnt++;
			break;

		case 5:
			ring->rx_ring_stat->rx_unkn_prot_cnt++;
			break;

		case 6:
			ring->rx_ring_stat->rx_fcs_err_cnt++;
			break;

		case 7:
			ring->rx_ring_stat->rx_buf_size_err_cnt++;
			break;

		case 8:
			ring->rx_ring_stat->rx_rxd_corrupt_cnt++;
			break;

		case 15:
			ring->rx_ring_stat->rx_unkn_err_cnt++;
			break;
		}

		/*
		* Drop the packet if bad transfer code. Exception being
		* 0x5, which could be due to unsupported IPv6 extension header.
		* In this case, we let stack handle the packet.
		* Note that in this case, since checksum will be incorrect,
		* stack will validate the same.
		*/
		if (err != 0x5) {
			DBG_PRINT(INFO_DBG, "%s: Rx error Value: 0x%x\n",
				dev->name, err);
			sp->stats.rx_crc_errors++;
			stats->mem_freed += skb->truesize;
			dev_kfree_skb(skb);
			ring->rx_bufs_left -= 1;
			rxdp->Host_Control = 0;
			return 0;
		}
	}

	/* Updating statistics */
	if (ring->rxd_mode == RXD_MODE_5)
		((struct RxD5 *)rxdp)->Host_Control = 0;
	else
		rxdp->Host_Control = 0;

	ring->rx_packets++;
	if (ring->rxd_mode == RXD_MODE_1) {
		int len = RXD_GET_BUFFER0_SIZE_1(rxdp->Control_2);
		ring->rx_bytes += len;
		skb_put(skb, len);
	} else if (ring->rxd_mode == RXD_MODE_5) {
		int get_block = ring->rx_curr_get_info.block_index;
		int get_off = ring->rx_curr_get_info.offset;
		int buf0_len = RXD_GET_BUFFER0_SIZE_5(rxdp->Control_2);
		int buf1_len = RXD_GET_BUFFER1_SIZE_5(rxdp->Control_2);
		int buf2_len = RXD_GET_BUFFER2_SIZE_5(rxdp->Control_2);
		int buf3_len =
		    RXD_GET_BUFFER3_SIZE_5(((struct RxD5 *)rxdp)->Control_3);
		int buf4_len =
		    RXD_GET_BUFFER4_SIZE_5(((struct RxD5 *)rxdp)->Control_3);
		unsigned char *buff = skb_push(skb, buf0_len);
		struct buffAdd *ba = &ring->ba[get_block][get_off];
		struct sk_buff *frag_list = NULL;

		COPY_ETH_HDR(buf0_len, buff, ba);

		ring->rx_bytes += buf0_len + buf1_len + buf2_len +
				buf3_len + buf4_len;

		skb_put(skb, buf1_len);
		skb->len += buf2_len;
		skb->data_len += buf2_len;
		frag_list = skb_shinfo(skb)->frag_list;
		skb_put(frag_list, buf2_len);
		if (sp->skbs_per_rxd > 2) {
			skb->len += buf3_len;
			skb->data_len += buf3_len;
			skb_put(frag_list->next, buf3_len);
		}
		if (sp->skbs_per_rxd > 3) {
			skb->len += buf4_len;
			skb->data_len += buf4_len;
			frag_list = frag_list->next;
			skb_put(frag_list->next, buf4_len);
		}
	} else if (ring->rxd_mode >= RXD_MODE_3B) {
		int get_block = ring->rx_curr_get_info.block_index;
		int get_off = ring->rx_curr_get_info.offset;
		int buf0_len = RXD_GET_BUFFER0_SIZE_3(rxdp->Control_2);
		int buf2_len = RXD_GET_BUFFER2_SIZE_3(rxdp->Control_2);
		unsigned char *buff = skb_push(skb, buf0_len);

		struct buffAdd *ba = &ring->ba[get_block][get_off];
		ring->rx_bytes += buf0_len + buf2_len;

		COPY_ETH_HDR(buf0_len, buff, ba);

		skb_put(skb, buf2_len);
	}

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	if (ring->ring_no) {
		vmknetddi_queueops_set_skb_queueid(skb,
			VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(ring->ring_no));
	}
#endif


	if ((rxdp->Control_1 & RXD_TCP_OR_UDP_FRAME) && ((!ring->lro) ||
		(ring->lro && (!(rxdp->Control_1 & RXD_FRAME_IP_FRAG)))) &&
		(sp->rx_csum)) {
		l3_csum = RXD_GET_L3_CKSUM(rxdp->Control_1);
		l4_csum = RXD_GET_L4_CKSUM(rxdp->Control_1);
		if ((l3_csum == L3_CKSUM_OK) && (l4_csum == L4_CKSUM_OK)) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			if (ring->lro && ((rxdp->Control_1 & RXD_TCP_IPV4_FRAME) ==
				RXD_TCP_IPV4_FRAME)) {
				u32 tcp_len = 0;
				u8 *tcp;
				int ret = 0;

				ret = s2io_club_tcp_session(ring, skb->data,
					&tcp, &tcp_len, &lro, rxdp, sp);

				if (lro && (lro->sg_num > ring->max_pkts_aggr))
					ring->max_pkts_aggr = lro->sg_num;

				if (LRO_AGGR_PACKET == ret) {
					lro_append_pkt(sp, lro, skb, tcp_len,
						ring->aggr_ack, ring->ring_no);
					goto aggregate;
				} else if (LRO_BEG_AGGR == ret) {
					lro->parent = skb;
					if (sp->rxd_mode == RXD_MODE_5) {
						struct sk_buff *frag_list =
						    skb_shinfo(skb)->frag_list;
						if (frag_list) {
							/* Traverse to the end
							* of the list*/
							while (frag_list->next)
							  frag_list =
							    frag_list->next;
							lro->last_frag =
								frag_list;
							lro->frags_len =
								skb->data_len;
						}
					}
					goto aggregate;
				} else if (LRO_FLUSH_SESSION == ret) {
					lro_append_pkt(sp, lro, skb, tcp_len,
						ring->aggr_ack, ring->ring_no);
					queue_rx_frame(lro->parent,
						lro->vlan_tag);
					clear_lro_session(lro);
					lro_stats->flush_max_pkts++;
					goto aggregate;
				} else if (LRO_FLUSH_BOTH == ret) {
					lro->parent->data_len = lro->frags_len;
					lro_stats->sending_both++;
					queue_rx_frame(lro->parent,
						lro->vlan_tag);
					clear_lro_session(lro);
					goto send_up;
				} else if ((0 != ret) && (-1 != ret) &&
						(5 != ret)) {
						DBG_PRINT(INFO_DBG,
						 "%s:Tcp Session Unhandled\n",
							__FUNCTION__);
						BUG();
				}
			}
		} else {
			/*
			 * Packet with erroneous checksum, let the
			 * upper layers deal with it.
			 */
			skb->ip_summed = CHECKSUM_NONE;
		}
	} else
		skb->ip_summed = CHECKSUM_NONE;

	stats->mem_freed += skb->truesize;
send_up:
	queue_rx_frame(skb, RXD_GET_VLAN_TAG(rxdp->Control_2));
	dev->last_rx = jiffies;
aggregate:
	sp->mac_control.rings[ring_no].rx_bufs_left -= 1;
	return SUCCESS;
}

/**
 *  s2io_link - stops/starts the Tx queue.
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  @link : inidicates whether link is UP/DOWN.
 *  Description:
 *  This function stops/starts the Tx queue depending on whether the link
 *  status of the NIC is is down or up. This is called by the Alarm
 *  interrupt handler whenever a link change interrupt comes up.
 *  Return value:
 *  void.
 */

static void s2io_link(struct s2io_nic *sp, int link)
{
	struct net_device *dev = (struct net_device *) sp->dev;
	struct swDbgStat *stats = sp->sw_dbg_stat;
	struct mac_info *mac_control = &sp->mac_control;
	struct config_param *config = &sp->config;
	int i = 0;
	unsigned long flags[MAX_TX_FIFOS];

#if !defined(__VMKLNX__)
	if (link != sp->last_link_state) {
		init_tti(sp, link);
#else  /* defined (__VMKLNX__) */
	int last_link;

	last_link=sp->last_link_state;
	sp->last_link_state = link;
	if (link != last_link) {
		init_tti(sp);
#endif  /* !defined (__VMKLNX__) */
		if (link == LINK_DOWN) {
			DBG_PRINT(ERR_DBG, "%s: Link down\n", dev->name);
#if defined(__VMKLNX__)
			netif_carrier_off(dev);

			s2io_stop_all_tx_queue(sp);
#else  /* !defined (__VMKLNX__) */
			s2io_stop_all_tx_queue(sp);

			netif_carrier_off(dev);
#endif /* defined(__VMKLNX__) */
			if (stats->link_up_cnt)
				stats->link_up_time = jiffies - sp->start_time;
			stats->link_down_cnt++;
#if defined(__VMKLNX__)
			if (likely(is_s2io_card_up(sp))) {
				/* We've lost link, so the controller stops DMA,
				 * but we've got queued Tx work that's never going
				 * to get done, so reset controller to flush Tx.
				 * (Do the reset outside of interrupt context).
				 * PR 384971 
				 */

				printk ("%s: Scheduling RESET task for %s\n", __FUNCTION__, dev->name);
				schedule_work(&sp->rst_timer_task);
				sp->mac_stat_info->soft_reset_cnt++;
			}
#endif /* defined(__VMKLNX__) */
		} else {
			DBG_PRINT(ERR_DBG, "%s: Link Up\n", dev->name);

			if (stats->link_down_cnt)
				stats->link_down_time =
					jiffies - sp->start_time;
			stats->link_up_cnt++;
			netif_carrier_on(dev);
			s2io_wake_all_tx_queue(sp);
		}
	}
#if !defined(__VMKLNX__)
	sp->last_link_state = link;
#endif
	sp->start_time = jiffies;
}

/**
 *  get_xena_rev_id - to identify revision ID of xena.
 *  @pdev : PCI Dev structure
 *  Description:
 *  Function to identify the Revision ID of xena.
 *  Return value:
 *  returns the revision ID of the device.
 */

static int get_xena_rev_id(struct pci_dev *pdev)
{
	u8 id = 0;
	pci_read_config_byte(pdev, PCI_REVISION_ID, (u8 *) &id);
	return id;
}

/* Returns 1 if system has AMD 8131a or Broadcom HT 1000 chipset.
* Returns 0 otherwise
*/
static int check_for_pcix_safe_param(void)
{
	struct pci_dev *tdev = NULL;
	u8 id = 0, set_param = 0;

	if ((tdev = S2IO_PCI_FIND_DEVICE(0x1022, 0x7450, NULL))!= NULL) {
		S2IO_PCI_PUT_DEVICE(tdev);
		pci_read_config_byte(tdev, PCI_REVISION_ID, (u8 *) &id);
		if (id <= 0x12) {
			DBG_PRINT(INIT_DBG, "Found AMD 8131a bridge\n");
			set_param = 1;
		}
	}

	if ((tdev = S2IO_PCI_FIND_DEVICE(0x1166, 0x0104, NULL)) != NULL) {
		S2IO_PCI_PUT_DEVICE(tdev);
		DBG_PRINT(INIT_DBG, "Found Broadcom HT 1000 bridge\n");
		set_param = 1;
	}
	return set_param;
}

static u8 update_8132_hyper_transport;
static void amd_8132_update_hyper_tx(u8 set_reset)
{
	struct pci_dev *tdev = NULL;
	u8 id = 0, ret;

	while ((tdev = S2IO_PCI_FIND_DEVICE(PCI_VENDOR_ID_AMD,
		PCI_DEVICE_ID_AMD_8132_BRIDGE, tdev)) !=  NULL) {
		ret = pci_read_config_byte(tdev, 0xf6, (u8 *) &id);
		if (set_reset & S2IO_BIT_SET) {
			if (id != 1) {
				ret = pci_write_config_byte(tdev, 0xf6, 1);
				update_8132_hyper_transport = 1;
			}
		} else {
			if (update_8132_hyper_transport && (id == 1))
				ret = pci_write_config_byte(tdev, 0xf6, 0);
		}
	}
}

/**
 *  s2io_init_pci -Initialization of PCI and PCI-X configuration registers .
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  Description:
 *  This function initializes a few of the PCI and PCI-X configuration
 *  registers with recommended values.
 *  Return value:
 *  void
 */

static void s2io_init_pci(struct s2io_nic *sp)
{
	u16 pci_cmd = 0, pcix_cmd = 0;

	/* Enable Data Parity Error Recovery in PCI-X command register. */
	pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
		&(pcix_cmd));
	pci_write_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
			      (pcix_cmd | 1));
	pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
			     &(pcix_cmd));

	/* Set the PErr Response bit in PCI command register. */
	pci_read_config_word(sp->pdev, PCI_COMMAND, &pci_cmd);
	pci_write_config_word(sp->pdev, PCI_COMMAND,
			      (pci_cmd | PCI_COMMAND_PARITY));
	pci_read_config_word(sp->pdev, PCI_COMMAND, &pci_cmd);

	/*
	 * If system has AMD 8131a(rev12 and below) OR
	 * Broadcom HT 1000 bridge, set safe PCI-X
	 * parameters(MMRBC=1K and max splits = 2)
	 */
	if (check_for_pcix_safe_param()) {
		pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
			&(pcix_cmd));
		pcix_cmd &= ~(0x7c);
		pcix_cmd |= 0x14;
		pci_write_config_word(sp->pdev, PCIX_COMMAND_REGISTER,
			pcix_cmd);
		DBG_PRINT(INIT_DBG, "Found AMD 8131a bridge\n");
	}
}

static void print_static_param_list(struct net_device *dev)
{
	struct s2io_nic *sp = dev->priv;

	DBG_PRINT(ERR_DBG, "%s: Using %d Tx fifo(s)\n", dev->name,
		  sp->config.tx_fifo_num);

	DBG_PRINT(ERR_DBG, "%s: %d-Buffer receive mode enabled\n",
		dev->name, rx_ring_mode);

	if (lro_enable == S2IO_DONT_AGGR_FWD_PKTS) {
		DBG_PRINT(ERR_DBG, "%s: Large receive offload enabled\n",
			dev->name);
	} else if (lro_enable == S2IO_ALWAYS_AGGREGATE) {
		DBG_PRINT(ERR_DBG, "%s: Large receive offload enabled "
			"for all packets\n", dev->name);
	} else
		DBG_PRINT(ERR_DBG, "%s: Large receive offload disabled\n",
			dev->name);

	if (vlan_tag_strip == S2IO_STRIP_VLAN_TAG) {
		DBG_PRINT(ERR_DBG, "%s: Vlan tag stripping enabled\n",
			dev->name);
	} else if (vlan_tag_strip == S2IO_DO_NOT_STRIP_VLAN_TAG)
		DBG_PRINT(ERR_DBG, "%s: Vlan tag stripping disabled\n",
			dev->name);
	if (ufo) {
		if (sp->device_type != XFRAME_II_DEVICE) {
			DBG_PRINT(ERR_DBG, "%s: Xframe I does not support UDP "
				"Fragmentation Offload(UFO)\n", dev->name);
		} else
			DBG_PRINT(ERR_DBG, "%s: UDP Fragmentation Offload(UFO)"
				" enabled\n", dev->name);
	} else
		DBG_PRINT(ERR_DBG, "%s: UDP Fragmentation Offload(UFO) "
			"disabled\n", dev->name);

	if (sp->config.multiq) {
		DBG_PRINT(ERR_DBG, "%s: Multiqueue support enabled\n",
			dev->name);
	} else
		DBG_PRINT(ERR_DBG, "%s: Multiqueue support disabled\n",
			dev->name);

	if (dev->mtu == 1500)
		DBG_PRINT(ERR_DBG, "%s: MTU size is %d\n",
			dev->name, dev->mtu);

	switch (sp->config.intr_type) {
	case INTA:
		DBG_PRINT(ERR_DBG, "%s: Interrupt type INTA\n", dev->name);
		break;
	case MSI_X:
		DBG_PRINT(ERR_DBG, "%s: Interrupt type MSI-X\n", dev->name);
		break;
	}

	if (sp->config.tx_steering_type == TX_PRIORITY_STEERING)
		DBG_PRINT(ERR_DBG, "%s: Priority steering enabled for "
						"transmit\n", dev->name);

	if (sp->config.rx_steering_type == RX_TOS_STEERING)
		DBG_PRINT(ERR_DBG, "%s: TOS steering enabled for receive\n", dev->name);

	if (sp->config.rx_steering_type == PORT_STEERING)
		DBG_PRINT(ERR_DBG, "%s: PORT steering enabled\n", dev->name);

	if (sp->config.rx_steering_type == RTH_STEERING) {
		switch (rth_protocol) {
		case 1:
		case 2:
			if (1 == rth_protocol) {
				DBG_PRINT(ERR_DBG, "%s: " \
					"RTH steering enabled for TCP_IPV4\n",
					dev->name);
			} else {
				DBG_PRINT(ERR_DBG, "%s: " \
					"RTH steering enabled for UDP_IPV4\n",
					dev->name);
			}

			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV6_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV6_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
				rth_mask = 0;
			}

			break;

		case 3:
			DBG_PRINT(ERR_DBG,
				"%s: RTH steering enabled for IPV4 \n",
				dev->name);
			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV6_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV6_DST) ||
				(rth_mask & S2IO_RTS_RTH_MASK_L4_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_L4_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
				rth_mask = 0;
			}
			break;

		case 4:
		case 5:
			if (4 == rth_protocol) {
				DBG_PRINT(ERR_DBG,
					"%s: RTH steering enabled for "  \
					"TCP_IPV6 \n", dev->name);
			} else {
				DBG_PRINT(ERR_DBG,
					"%s: RTH steering enabled for " \
					"UDP_IPV6 \n", dev->name);
			}

			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV4_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV4_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
				rth_mask = 0;
			}
			break;

		case 6:
			DBG_PRINT(ERR_DBG,
				"%s: RTH steering enabled for IPV6 \n",
				dev->name);
			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV4_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV4_DST) ||
				(rth_mask & S2IO_RTS_RTH_MASK_L4_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_L4_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
				rth_mask = 0;
			}
			break;

		case 7:
		case 8:
		case 9:
			if (7 == rth_protocol) {
				DBG_PRINT(ERR_DBG,
					"%s: RTH steering enabled for " \
					"TCP_IPV6_EX \n", dev->name);
			} else if (8 == rth_protocol) {
				DBG_PRINT(ERR_DBG,
					"%s: RTH steering enabled for " \
					"UDP_IPV6_EX \n", dev->name);
			} else {
				DBG_PRINT(ERR_DBG,
					"%s: RTH steering enabled for " \
					"IPV6_EX \n", dev->name);
			}

			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV4_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV4_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
					rth_mask = 0;
			}
			break;

		default:
			DBG_PRINT(ERR_DBG, "%s: RTH Function is not correct. "
				"Defaulting to TCP_IPV4.\n", dev->name);
			rth_protocol =  0x1;
			if ((rth_mask & S2IO_RTS_RTH_MASK_IPV6_SRC) ||
				(rth_mask & S2IO_RTS_RTH_MASK_IPV6_DST)) {
				DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
					"Disabling mask.\n");
				rth_mask = 0;
			}

			break;
		}

		if ((rth_mask < 0) || (rth_mask > 255)) {
			DBG_PRINT(ERR_DBG, "RTH Mask is not correct. "
				"Disabling mask.\n");
			rth_mask = 0;
		}
	}

	if (sp->config.napi) {
		DBG_PRINT(ERR_DBG, "%s: NAPI enabled\n", dev->name);
	} else {
		DBG_PRINT(ERR_DBG, "%s: NAPI disabled\n", dev->name);
	}

	return;
}

static void s2io_verify_parm(struct pci_dev *pdev, u8 *dev_intr_type,
	u8 *dev_steer_type, u8 *dev_multiq)
{
	if (lro_enable) {
		if (lro_enable > S2IO_DONT_AGGR_FWD_PKTS) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9))
			DBG_PRINT(ERR_DBG, "s2io: Unsupported LRO option. "
				"Disabling LRO\n");
			lro_enable = S2IO_DONOT_AGGREGATE;
#else
			DBG_PRINT(ERR_DBG,
				"s2io: Unsupported LRO option. Enabling LRO "
				"for local destination\n");
			lro_enable = S2IO_DONT_AGGR_FWD_PKTS;
#endif
		}
	}

	if (vlan_tag_strip) {
		if (vlan_tag_strip > S2IO_DEFAULT_STRIP_MODE_VLAN_TAG) {
			DBG_PRINT(ERR_DBG,
				"s2io: Unsupported vlan tag sripping option. "
				"Enabling vlan tag stripping "
				"if vlan group present\n");
			vlan_tag_strip = S2IO_DEFAULT_STRIP_MODE_VLAN_TAG;
		}
	}

	if ((tx_fifo_num > MAX_TX_FIFOS) || (tx_fifo_num == 0)) {
		DBG_PRINT(ERR_DBG, "s2io: Requested number of tx fifos "
			"(%d) not supported\n", tx_fifo_num);

		if (tx_fifo_num == 0)
			tx_fifo_num = 1;
		else
			tx_fifo_num = MAX_TX_FIFOS;

		DBG_PRINT(ERR_DBG, "s2io: Default to %d ", tx_fifo_num);
		DBG_PRINT(ERR_DBG, "tx fifos\n");
	}

	if (tx_steering_type && (1 == tx_fifo_num)) {
		if (tx_steering_type != TX_STEERING_DEFAULT)
			DBG_PRINT(ERR_DBG,
				"s2io: Tx steering is not supported with "
				"one fifo. Disabling Tx steering.\n");
		tx_steering_type = NO_STEERING;
	}

	if ((tx_steering_type < NO_STEERING) ||
		(tx_steering_type > TX_STEERING_DEFAULT)) {
		DBG_PRINT(ERR_DBG, "s2io: Requested transmit steering not "
			 "supported\n");
		DBG_PRINT(ERR_DBG, "s2io: Disabling transmit steering\n");
		tx_steering_type = NO_STEERING;
	}

	if (rx_steering_type && (1 == rx_ring_num)) {
		if (rx_steering_type != RTH_STEERING_DEFAULT)
			DBG_PRINT(ERR_DBG,
				"s2io: Receive steering is not supported with "
				"one ring. Disabling receive steering.\n");
		*dev_steer_type = NO_STEERING;
	}

	if (rx_steering_type) {
		/* Check if it is a xena card */
		if ((pdev->device != PCI_DEVICE_ID_HERC_WIN) &&
			(pdev->device != PCI_DEVICE_ID_HERC_UNI)) {
			if (rx_steering_type != RTH_STEERING_DEFAULT)
				DBG_PRINT(ERR_DBG,
					"s2io: Receive steering disabled on Xframe I\n");
			*dev_steer_type = NO_STEERING;
		}
	}

	if (rx_ring_num > MAX_RX_RINGS) {
		DBG_PRINT(ERR_DBG, "s2io: Requested number of rx rings not "
			 "supported\n");
		DBG_PRINT(ERR_DBG, "s2io: Default to %d rx rings\n",
			MAX_RX_RINGS);
		rx_ring_num = MAX_RX_RINGS;
	}

#ifndef CONFIG_PCI_MSI
	if (intr_type != INTA) {
		DBG_PRINT(ERR_DBG, "s2io: This kernel does not support "
			  "MSI-X. Defaulting to INTA\n");
		*dev_intr_type = INTA;
		intr_type = INTA;
	}
#else
	if ((intr_type != INTA) && (intr_type != MSI_X)) {
		DBG_PRINT(ERR_DBG, "s2io: Wrong intr_type requested. "
			  "Defaulting to INTA\n");
		*dev_intr_type = INTA;
		intr_type = INTA;
	}

	if ((pdev->device != PCI_DEVICE_ID_HERC_WIN) &&
			(pdev->device != PCI_DEVICE_ID_HERC_UNI)) {
		if (intr_type == MSI_X)
			DBG_PRINT(ERR_DBG, "s2io: Xframe I does not support "
				"MSI_X. Defaulting to INTA\n");
		*dev_intr_type = INTA;
	}
#endif

#ifndef NETIF_F_UFO
	if (ufo) {
		DBG_PRINT(ERR_DBG, "s2io: This kernel does not support "
			"UDP Fragmentation Offload(UFO)\n");
		DBG_PRINT(ERR_DBG,
			"s2io: UDP Fragmentation Offload(UFO) disabled\n");
		ufo = 0;
	}
#endif

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	*dev_multiq = multiq;
#endif

#if defined CONFIG_PPC64
	if (rx_ring_mode == 2) {
		rx_ring_mode = 1;
		DBG_PRINT(ERR_DBG, "s2io: 2 Buffer mode is not supported "
		"on PPC64 Arch. Switching to 1-buffer mode \n");
	}
#endif

	if ((rx_ring_mode != 1) && (rx_ring_mode != 2) &&
		(rx_ring_mode != 5)) {
		DBG_PRINT(ERR_DBG,
			"s2io: Requested ring mode not supported\n");
		DBG_PRINT(ERR_DBG, "s2io: Defaulting to 1-buffer mode\n");
		rx_ring_mode = 1;
	}
	return;
}

/**
 *  s2io_init_nic - Initialization of the adapter .
 *  @pdev : structure containing the PCI related information of the device.
 *  @pre: List of PCI devices supported by the driver listed in s2io_tbl.
 *  Description:
 *  The function initializes an adapter identified by the pci_dec structure.
 *  All OS related initialization including memory and device structure and
 *  initlaization of the device private variable is done. Also the swapper
 *  control register is initialized to enable read and write into the I/O
 *  registers of the device.
 *  Return value:
 *  returns 0 on success and negative on failure.
 */

static int __devinit
s2io_init_nic(struct pci_dev *pdev, const struct pci_device_id *pre)
{
	struct s2io_nic *sp;
	struct net_device *dev = NULL;
	int i, j, ret;
	int dma_flag = FALSE;
	u32 mac_up, mac_down;
	u64 val64 = 0, tmp64 = 0;
	struct XENA_dev_config __iomem *bar0 = NULL;
	u16 subid;
	struct mac_info *mac_control;
	struct config_param *config;
	int mode;
	u8 dev_intr_type, dev_multiq = 0;
	u8 dev_steer_type = rx_steering_type;
	int no_cpus = 1;

	DBG_PRINT(ERR_DBG, "Copyright(c) 2002-2009 Neterion Inc.\n");

#ifdef __VMKLNX__
	/*
	 * Enable MSI-X if "enable_netq" is set.
	 */
	if (enable_netq)
		if (intr_type == DEF_MSI_X)
			intr_type = MSI_X;
#endif


#ifndef __VMKLNX__
	no_cpus = num_online_cpus();
	if (DEF_MSI_X == intr_type) {
		/* Don't enable MSI-X if there are less than 4 CPUs */
		if (no_cpus < 4)
			intr_type = INTA;
		else
			intr_type = MSI_X;
	}
#endif
	dev_intr_type = intr_type;

	s2io_verify_parm(pdev, &dev_intr_type, &dev_steer_type, &dev_multiq);

	ret = pci_enable_device(pdev);
	if (ret) {
		DBG_PRINT(ERR_DBG,
			"%s: pci_enable_device failed\n", __FUNCTION__);
		goto enable_device_failed;
	}

	if (!pci_set_dma_mask(pdev, DMA_64BIT_MASK)) {
		DBG_PRINT(INIT_DBG, "%s: Using 64bit DMA\n", __FUNCTION__);
		dma_flag = TRUE;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 00)) || \
		defined CONFIG_IA64_SGI_SN2
		if (pci_set_consistent_dma_mask(pdev, DMA_64BIT_MASK)) {
			DBG_PRINT(ERR_DBG, "%s: \
				Unable to obtain 64bit DMA for \
				consistent allocations\n", __FUNCTION__);
			ret = -ENOMEM;
			goto set_dma_mask_failed;
		}
#endif
	} else if (!pci_set_dma_mask(pdev, DMA_32BIT_MASK)) {
		DBG_PRINT(INIT_DBG, "%s: Using 32bit DMA\n", __FUNCTION__);
	} else {
		ret = -ENOMEM;
		goto set_dma_mask_failed;
	}

	ret = pci_request_regions(pdev, s2io_driver_name);
	if (ret) {
		DBG_PRINT(ERR_DBG, "%s: Request Regions failed - %x \n",
			 __FUNCTION__, ret);
		ret = -ENODEV;
		goto pci_request_regions_failed;
	}
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (dev_multiq)
		dev = alloc_etherdev_mq(sizeof(struct s2io_nic), tx_fifo_num);
	else
#endif
		dev = alloc_etherdev(sizeof(struct s2io_nic));

	if (dev == NULL) {
		DBG_PRINT(ERR_DBG, "%s: Device allocation failed\n",
			__FUNCTION__);
		ret = -ENODEV;
		goto alloc_etherdev_failed;
	}

	pci_set_master(pdev);
	pci_set_drvdata(pdev, dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	/*  Private member variable initialized to s2io NIC structure */
	sp = dev->priv;
	memset(sp, 0, sizeof(struct s2io_nic));
	/* Default execution mode is release mode*/
	sp->exec_mode = 1;
	sp->serious_err = 0;
	sp->dev = dev;
	sp->pdev = pdev;
	sp->high_dma_flag = dma_flag;
	sp->device_enabled_once = FALSE;
	if (rx_ring_mode == 1)
		sp->rxd_mode = RXD_MODE_1;
	if (rx_ring_mode == 2)
		sp->rxd_mode = RXD_MODE_3B;
	if (rx_ring_mode == 5)
		sp->rxd_mode = RXD_MODE_5;

#if defined(__VMKLNX__)
	sp->rx_coalesce_usecs =  0;
	sp->tx_coalesce_usecs =  0;
#endif

	/*
	 * Setting the device configuration parameters.
	 * Most of these parameters can be specified by the user during
	 * module insertion as they are module loadable parameters. If
	 * these parameters are not not specified during load time, they
	 * are initialized with default values.
	 */
	mac_control = &sp->mac_control;
	config = &sp->config;

	config->rst_q_stuck = rst_q_stuck;
	config->napi = napi;
	config->intr_type = dev_intr_type;
	config->rx_steering_type = dev_steer_type;
	config->tx_steering_type = tx_steering_type;
	config->multiq = dev_multiq;
	config->vlan_tag_strip = vlan_tag_strip;

	sp->vlan_strip_flag = config->vlan_tag_strip;
	if (sp->vlan_strip_flag == S2IO_DEFAULT_STRIP_MODE_VLAN_TAG)
		sp->vlan_strip_flag = S2IO_DO_NOT_STRIP_VLAN_TAG;

#ifdef SNMP_SUPPORT
	strcpy(sp->cName, "NETERION");
	strcpy(sp->cVersion, DRV_VERSION);
#endif
#ifdef TITAN_LEGACY
	if ((pdev->device == PCI_DEVICE_ID_TITAN_WIN) ||
		(pdev->device == PCI_DEVICE_ID_TITAN_UNI))
		sp->device_type = TITAN_DEVICE;
	else
#endif
	if ((pdev->device == PCI_DEVICE_ID_HERC_WIN) ||
		(pdev->device == PCI_DEVICE_ID_HERC_UNI)) {
		
		u16 pci_cmd;
		sp->device_type = XFRAME_II_DEVICE;
		pci_read_config_word(sp->pdev, PCI_SUBSYSTEM_ID, &pci_cmd);
		for (i = 0; i < ARRAY_SIZE(s2io_subsystem_id); i++)
			if (pci_cmd == s2io_subsystem_id[i]) {
				sp->device_sub_type = XFRAME_E_DEVICE;
				break;
			}
	} else
		sp->device_type = XFRAME_I_DEVICE;

	sp->lro = lro_enable;

	/* Initialize some PCI/PCI-X fields of the NIC. */
	s2io_init_pci(sp);

	/* By default, the mapping from MSI/MSI-X to hyper transport interrupts
	is disabled on AMD 8132 bridge. Enable this to support MSI-X interrupts
	*/
	if (MSI_X == intr_type)
		amd_8132_update_hyper_tx(S2IO_BIT_SET);

	sp->bar0 = (caddr_t) ioremap(pci_resource_start(pdev, 0),
				     pci_resource_len(pdev, 0));
	if (!sp->bar0) {
		DBG_PRINT(ERR_DBG, "%s: ", __FUNCTION__);
		DBG_PRINT(ERR_DBG, "cannot remap io mem1\n");
		ret = -ENOMEM;
		goto bar0_remap_failed;
	}

	sp->bar1 = (caddr_t) ioremap(pci_resource_start(pdev, 2),
					pci_resource_len(pdev, 2));
	if (!sp->bar1) {
		DBG_PRINT(ERR_DBG, "%s: ", __FUNCTION__);
		DBG_PRINT(ERR_DBG, "cannot remap io mem2\n");
		ret = -ENOMEM;
		goto bar1_remap_failed;
	}

	dev->irq = pdev->irq;
	dev->base_addr = (unsigned long) sp->bar0;

	/* Setting swapper control on the NIC, for proper reset operation */
	if (s2io_set_swapper(sp)) {
		DBG_PRINT(ERR_DBG, "%s: ", __FUNCTION__);
		DBG_PRINT(ERR_DBG, "swapper settings are wrong\n");
		ret = -EAGAIN;
		goto set_swap_failed;
	}

	/* Verify if the Herc works on the slot its placed into */
	if (sp->device_type == XFRAME_II_DEVICE) {
		mode = s2io_verify_pci_mode(sp);
		if (mode < 0) {
			DBG_PRINT(ERR_DBG, "%s: ", __FUNCTION__);
			DBG_PRINT(ERR_DBG, " Unsupported PCI bus mode\n");
			ret = -EBADSLT;
			goto set_swap_failed;
		}
	}

	/* Tx side parameters. */
	if (config->tx_steering_type == TX_PRIORITY_STEERING)
		config->tx_fifo_num = MAX_TX_FIFOS;
	else
		config->tx_fifo_num = tx_fifo_num;

#if defined (__VMKLNX__) && defined CONFIG_NETDEVICES_MULTIQUEUE
	dev->real_num_tx_queues=config->tx_fifo_num;
#endif

	/* Initialize the fifos used for tx steering */
	if (config->tx_fifo_num < 5) {
			if (config->tx_fifo_num  == 1)
				sp->total_tcp_fifos = 1;
			else
				sp->total_tcp_fifos = config->tx_fifo_num - 1;
			sp->udp_fifo_idx = config->tx_fifo_num - 1;
			sp->total_udp_fifos = 1;
			sp->other_fifo_idx = sp->total_tcp_fifos - 1;
	} else {
		sp->total_tcp_fifos = (tx_fifo_num - FIFO_UDP_MAX_NUM -
						FIFO_OTHER_MAX_NUM);
		sp->udp_fifo_idx = sp->total_tcp_fifos;
		sp->total_udp_fifos = FIFO_UDP_MAX_NUM;
		sp->other_fifo_idx = sp->udp_fifo_idx + FIFO_UDP_MAX_NUM;
	}

	for (i = 0; i < config->tx_fifo_num; i++) {
		config->tx_cfg[i].fifo_len = tx_fifo_len[i];
		config->tx_cfg[i].fifo_priority = i;
	}

	/* mapping the QoS priority to the configured fifos */
	for (i = 0; i < MAX_TX_FIFOS; i++)
		sp->fifo_mapping[i] = fifo_map[config->tx_fifo_num - 1][i];

	/* map the hashing selector table to the configured fifos */
	for (i = 0; i < config->tx_fifo_num; i++)
		sp->fifo_selector[i] = fifo_selector[i];

	config->tx_intr_type = TXD_INT_TYPE_UTILZ;
	for (i = 0; i < config->tx_fifo_num; i++) {
		if (config->tx_cfg[i].fifo_len < 65) {
			config->tx_intr_type = TXD_INT_TYPE_PER_LIST;
			break;
		}
	}
	config->max_txds = MAX_SKB_FRAGS + 1;
#ifdef NETIF_F_UFO
	config->max_txds++;
#endif

#if !defined (__VMKLNX__)
	/* All this will now be done in s2io_set_rx_rings() */
	/* Rx side parameters. */
	config->rx_ring_num = rx_ring_num;

	if (rx_ring_num == 0) {
		if (sp->device_type == XFRAME_I_DEVICE)
			config->rx_ring_num = 1;
		else if (config->intr_type == MSI_X)
#ifndef __VMKLNX__
			config->rx_ring_num =
				no_cpus > RTH_MIN_RING_NUM ?
					no_cpus : RTH_MIN_RING_NUM;
		else
			config->rx_ring_num = RTH_MIN_RING_NUM;
#else
			config->rx_ring_num = MAX_RX_RINGS;
		else
			config->rx_ring_num = 1;
#endif
	}

	if (config->rx_ring_num > MAX_RX_RINGS)
		config->rx_ring_num = MAX_RX_RINGS;

#else  /* !defined (__VMKLNX__) */
	s2io_set_rx_rings(sp, config);
#endif /* !defined (__VMKLNX__) */

#ifdef CONFIG_PCI_MSI
	if ((sp->device_type == XFRAME_II_DEVICE) &&
		(config->intr_type == MSI_X)) {
		// Done in s2io_set_rx_rings().
#if !defined (__VMKLNX__)
		sp->num_entries = config->rx_ring_num + 1;
#endif
		ret = s2io_enable_msi_x(sp);
		if (!ret) {
			ret = s2io_test_msi(sp);
			/* rollback MSI-X, will re-enable during add_isr() */
			do_rem_msix_isr(sp);
		}

		if (ret) {
			DBG_PRINT(ERR_DBG,
				"MSI-X requested but failed to enable\n");
			config->intr_type = INTA;
		}
	}
#endif

#ifndef __VMKLNX__
	if ((config->intr_type == MSI_X) &&
		(config->rx_steering_type == RTH_STEERING_DEFAULT) &&
		(config->rx_ring_num > RTH_MIN_RING_NUM))
			config->rx_steering_type = RTH_STEERING;
#endif

	if ((config->rx_steering_type == RX_TOS_STEERING) &&
		(sp->device_type != XFRAME_I_DEVICE))
		config->rx_ring_num = MAX_RX_RINGS;

#if !defined (__VMKLNX__)
	for (i = 0; i < config->rx_ring_num; i++) {
#else  /* !defined (__VMKLNX__) */
	// Need to do this one time thing for MAX rings.
	for (i = 0; i < MAX_RX_RINGS; i++) {
#endif /* !defined (__VMKLNX__) */
		config->rx_cfg[i].num_rxd = rx_ring_sz[i] *
			(rxd_count[sp->rxd_mode] + 1);
		config->rx_cfg[i].ring_priority = i;
		mac_control->rings[i].rx_bufs_left = 0;
		mac_control->rings[i].lro = sp->lro;
		mac_control->rings[i].rxd_mode = sp->rxd_mode;
		mac_control->rings[i].rxd_count = rxd_count[sp->rxd_mode];
		mac_control->rings[i].pdev = sp->pdev;
		mac_control->rings[i].dev = sp->dev;
		mac_control->rings[i].config_napi= config->napi;
	}

#if !defined (__VMKLNX__)
	// This has been moved to later to get accurate count
	// of rx filters based on device type.
#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	spin_lock_init(&config->netqueue_lock);

	config->n_tx_fifo_allocated = 0;
	config->n_rx_ring_allocated = 0;
	for (i = 0; i < MAX_RX_RINGS; i++) {
		config->rx_cfg[i].allocated = FALSE;
		config->rx_cfg[i].active_filter_count = 0;
		for (j=0; j<S2IO_MAX_QUEUE_RXFILTERS; j++) {
			config->rx_cfg[i].filter[j].active = FALSE;
		}
	}
	for (i = 0; i < MAX_TX_FIFOS; i++) {
		config->tx_cfg[i].allocated = FALSE;
	}
#endif
#else /* defined (__VMKLNX__) */
	spin_lock_init(&sp->rxtx_ti_lock);
#endif /* !defined (__VMKLNX__) */

	/*  Setting Mac Control parameters */
	mac_control->rmac_pause_time = rmac_pause_time;
	mac_control->mc_pause_threshold_q0q3 = mc_pause_threshold_q0q3;
	mac_control->mc_pause_threshold_q4q7 = mc_pause_threshold_q4q7;

	/*  initialize the shared memory used by the NIC and the host */
	ret = init_shared_mem(sp);
	if (ret != SUCCESS) {
		DBG_PRINT(ERR_DBG, "%s: ", __FUNCTION__);
		if (ret == -ENOMEM) {
			DBG_PRINT(ERR_DBG, "Memory allocation failed\n");
			goto mem_alloc_failed;
		} else {
			DBG_PRINT(ERR_DBG,
				"Memory allocation configuration invalid\n");
			goto configuration_failed;
		}
	}

	/* Initializing the BAR1 address as the start of the FIFO pointer. */
	for (j = 0; j < config->tx_fifo_num; j++)
		mac_control->tx_FIFO_start[j] =
			(struct TxFIFO_element __iomem *)
				(sp->bar1 + (j * 0x00020000));

	/*  Driver entry points */
	dev->open = &s2io_open;
	dev->stop = &s2io_close;
	dev->hard_start_xmit = &s2io_xmit;
	dev->get_stats = &s2io_get_stats;
	dev->set_multicast_list = &s2io_set_multicast;
	dev->do_ioctl = &s2io_ioctl;
	dev->set_mac_address = &s2io_set_mac_addr;
	dev->change_mtu = &s2io_change_mtu;
#ifdef SET_ETHTOOL_OPS
	SET_ETHTOOL_OPS(dev, (struct ethtool_ops *)&netdev_ethtool_ops);
#endif

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	VMKNETDDI_REGISTER_QUEUEOPS(dev, s2io_netqueue_ops);
#endif

	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#ifdef NETIF_F_LLTX
	if (sp->device_type != XFRAME_I_DEVICE)
		dev->features |= NETIF_F_LLTX;
#endif
	dev->vlan_rx_register = s2io_vlan_rx_register;
	dev->vlan_rx_kill_vid = (void *)s2io_vlan_rx_kill_vid;

#if defined(HAVE_NETDEV_POLL)
	if (config->intr_type == MSI_X) {
#if !defined (__VMKLNX__)
		for (i = 0; i < config->rx_ring_num ; i++)
#else  /* !defined (__VMKLNX__) */
		// need to do this 1 time thing for max rings.
		for (i = 0; i < MAX_RX_RINGS ; i++)
#endif /* !defined (__VMKLNX__) */
		netif_napi_add(dev, &mac_control->rings[i].napi,
			s2io_poll_msix, 64);
	} else
		netif_napi_add(dev, &sp->napi, s2io_poll_inta, 64);
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = s2io_netpoll;
#endif

	dev->features |= NETIF_F_SG;
	if (sp->high_dma_flag == TRUE)
		dev->features |= NETIF_F_HIGHDMA;
#ifdef NETIF_F_TSO
	dev->features |= NETIF_F_TSO;
#endif
#ifdef NETIF_F_TSO6
	dev->features |= NETIF_F_TSO6;
#endif
#ifdef NETIF_F_UFO
	if ((sp->device_type == XFRAME_II_DEVICE) && (ufo))
		dev->features |= NETIF_F_UFO;
#endif
	if (sp->device_type == XFRAME_II_DEVICE)
		dev->features |= NETIF_F_HW_CSUM;
	else
		dev->features |= NETIF_F_IP_CSUM;

#if !defined (__VMKLNX__)
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (config->multiq)
		dev->features |= NETIF_F_MULTI_QUEUE;
#endif
#endif
	dev->tx_timeout = &s2io_tx_watchdog;
	dev->watchdog_timeo = WATCH_DOG_TIMEOUT;
	INIT_WORK(&sp->rst_timer_task, s2io_restart_nic);
	INIT_WORK(&sp->set_link_task, s2io_set_link);

	pci_save_state(sp->pdev, sp->config_space);

	/* Not needed for Herc */
	if (sp->device_type == XFRAME_I_DEVICE) {
		/*
		 * Fix for all "FFs" MAC address problems observed on
		 * Alpha platforms
		 */
		fix_mac_address(sp);
		s2io_reset(sp);
	}

	/*
	 * MAC address initialization.
	 * For now only one mac address will be read and used.
	 */
	bar0 = sp->bar0;
	val64 = RMAC_ADDR_CMD_MEM_RD | RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	    RMAC_ADDR_CMD_MEM_OFFSET(0 + S2IO_MAC_ADDR_START_OFFSET);
	writeq(val64, &bar0->rmac_addr_cmd_mem);
	wait_for_cmd_complete(&bar0->rmac_addr_cmd_mem,
		RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
		S2IO_BIT_RESET);
	tmp64 = readq(&bar0->rmac_addr_data0_mem);
	mac_down = (u32) tmp64;
	mac_up = (u32) (tmp64 >> 32);

	sp->def_mac_addr[0].mac_addr[3] = (u8) (mac_up);
	sp->def_mac_addr[0].mac_addr[2] = (u8) (mac_up >> 8);
	sp->def_mac_addr[0].mac_addr[1] = (u8) (mac_up >> 16);
	sp->def_mac_addr[0].mac_addr[0] = (u8) (mac_up >> 24);
	sp->def_mac_addr[0].mac_addr[5] = (u8) (mac_down >> 16);
	sp->def_mac_addr[0].mac_addr[4] = (u8) (mac_down >> 24);
	/*  Set the factory defined MAC address initially   */
	dev->addr_len = ETH_ALEN;
	memcpy(dev->dev_addr, sp->def_mac_addr, ETH_ALEN);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14))
	memcpy(dev->perm_addr, dev->dev_addr, ETH_ALEN);
#endif

	/* initialize number of multicast & unicast MAC entries variables */
	if (sp->device_type == XFRAME_I_DEVICE) {
		config->max_mc_addr = S2IO_XENA_MAX_MC_ADDRESSES;
		config->max_mac_addr = S2IO_XENA_MAX_MAC_ADDRESSES;
		config->mc_start_offset = S2IO_XENA_MC_ADDR_START_OFFSET;
	} else if (sp->device_type == XFRAME_II_DEVICE) {
		config->max_mc_addr = S2IO_HERC_MAX_MC_ADDRESSES;
		config->max_mac_addr = S2IO_HERC_MAX_MAC_ADDRESSES;
		config->mc_start_offset = S2IO_HERC_MC_ADDR_START_OFFSET;
	}

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
	spin_lock_init(&config->netqueue_lock);

	config->n_tx_fifo_allocated = 0;
	config->n_rx_ring_allocated = 0;
	for (i = 0; i < MAX_RX_RINGS; i++) {
		config->rx_cfg[i].allocated = FALSE;
		config->rx_cfg[i].active_filter_count = 0;
		for (j=0; j < S2IO_MAX_AVAIL_RX_FILTERS(config); j++) {
			config->rx_cfg[i].filter[j].active = FALSE;
		}
	}
	for (i = 0; i < MAX_TX_FIFOS; i++) {
		config->tx_cfg[i].allocated = FALSE;
	}
#endif
	/* store mac addresses from CAM to s2io_nic structure */
	do_s2io_store_unicast_mc(sp);

#ifdef CONFIG_PCI_MSI
	if ((sp->device_type == XFRAME_II_DEVICE) &&
		(config->intr_type == MSI_X))
#if !defined (__VMKLNX__)
		sp->num_entries = config->rx_ring_num + 1;
#endif
		/* Store the values of the MSIX table in the
		 * s2io_nic structure
		 */
		store_xmsi_data(sp);
#endif

	/* reset Nic and bring it to known state */
	s2io_reset(sp);

	/*
	 * Initialize the link state flags
	 * and the card state parameter
	 */
	sp->state = 0;
	/* Initialize spinlocks */
	for (i = 0; i < config->tx_fifo_num; i++) {
		spin_lock_init(&mac_control->fifos[i].tx_lock);
		mac_control->fifos[i].intr_type = config->intr_type;
		mac_control->fifos[i].dev = sp->dev;
		mac_control->fifos[i].multiq = config->multiq;
	}

	/*
	 * SXE-002: Configure link and activity LED to init state
	 * on driver load.
	 */
	subid = sp->pdev->subsystem_device;
	if ((subid & 0xFF) >= 0x07) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->gpio_control);
		val64 = 0x0411040400000000ULL;
		writeq(val64, (void __iomem *) bar0 + 0x2700);
		val64 = readq(&bar0->gpio_control);
	}

	sp->rx_csum = 1; /* Rx chksum verify enabled by default */

	if (register_netdev(dev)) {
		DBG_PRINT(ERR_DBG, "Device registration failed\n");
		ret = -ENODEV;
		goto register_failed;
	}
	s2io_vpd_read(sp);
	DBG_PRINT(ERR_DBG, "%s: Neterion %s (rev %d)\n", dev->name,
		  sp->product_name, get_xena_rev_id(sp->pdev));
	DBG_PRINT(ERR_DBG, "%s: Driver version %s\n", dev->name,
		  s2io_driver_version);
	DBG_PRINT(ERR_DBG, "%s: MAC ADDR: "
			"%02x:%02x:%02x:%02x:%02x:%02x, ", dev->name,
			sp->def_mac_addr[0].mac_addr[0],
			sp->def_mac_addr[0].mac_addr[1],
			sp->def_mac_addr[0].mac_addr[2],
			sp->def_mac_addr[0].mac_addr[3],
			sp->def_mac_addr[0].mac_addr[4],
			sp->def_mac_addr[0].mac_addr[5]);
	DBG_PRINT(ERR_DBG, "SERIAL NUMBER: %s\n", sp->serial_num);
	if (sp->device_type == XFRAME_II_DEVICE) {
		mode = s2io_print_pci_mode(sp);
		if (mode < 0) {
			DBG_PRINT(ERR_DBG, " Unsupported PCI bus mode\n");
			ret = -EBADSLT;
			goto check_pci_bus_mode_failed;
		}
	}


	print_static_param_list(dev);

	/* Initialize device name */
	sprintf(sp->name, "%s Neterion %s", dev->name, sp->product_name);

	/*
	 * Make Link state as off at this point, when the Link change
	 * interrupt comes the state will be automatically changed to
	 * the right state.
	 */
	netif_carrier_off(dev);

#ifdef SNMP_SUPPORT
	s2io_snmp_init(sp);
#endif

	return 0;

check_pci_bus_mode_failed:
	unregister_netdev(dev);

register_failed:
mem_alloc_failed:
	free_shared_mem(sp);

configuration_failed:
set_swap_failed:
	iounmap(sp->bar1);

bar1_remap_failed:
	iounmap(sp->bar0);

bar0_remap_failed:
	pci_set_drvdata(pdev, NULL);
	free_netdev(dev);

alloc_etherdev_failed:
	pci_release_regions(pdev);

pci_request_regions_failed:
set_dma_mask_failed:
	pci_disable_device(pdev);

enable_device_failed:
	return ret;
}

/**
 * s2io_rem_nic - Free the PCI device
 * @pdev: structure containing the PCI related information of the device.
 * Description: This function is called by the Pci subsystem to release a
 * PCI device and free up all resource held up by the device. This could
 * be in response to a Hot plug event or when the driver is to be removed
 * from memory.
 */

static void __devexit s2io_rem_nic(struct pci_dev *pdev)
{
	struct net_device *dev =
	    (struct net_device *) pci_get_drvdata(pdev);
	struct s2io_nic *sp;
	int cnt;
	u64 tmp64;

	if (dev == NULL) {
		DBG_PRINT(ERR_DBG, "Driver Data is NULL!!\n");
		return;
	}

	sp = dev->priv;

	/* delete all populated mac entries */
	for (cnt = 1; cnt < sp->config.max_mc_addr; cnt++) {
		tmp64 = do_s2io_read_unicast_mc(sp, cnt);
		if (tmp64 != S2IO_DISABLE_MAC_ENTRY)
			do_s2io_delete_unicast_mc(sp, tmp64);
	}

	flush_scheduled_work();

	unregister_netdev(dev);

	free_shared_mem(sp);
	iounmap(sp->bar0);
	iounmap(sp->bar1);
	pci_release_regions(pdev);

	pci_set_drvdata(pdev, NULL);
#ifdef SNMP_SUPPORT
	s2io_snmp_exit(sp);
#endif
	free_netdev(dev);

	pci_disable_device(pdev);
}

/**
 * s2io_starter - Entry point for the driver
 * Description: This function is the entry point for the driver. It verifies
 * the module loadable parameters and initializes PCI configuration space.
 */

static int __init s2io_starter(void)
{

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0))
	lro_enable = lro;
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17))
	return pci_module_init(&s2io_driver);
#else
	return pci_register_driver(&s2io_driver);
#endif
}

/**
 * s2io_closer - Cleanup routine for the driver
 * Description: This function is the cleanup routine for the driver.
 * It unregisters the driver.
 */

static __exit void s2io_closer(void)
{
	pci_unregister_driver(&s2io_driver);

	/* Reset the hyper transport interrupts if set on AMD 8132 bridge */
	if (intr_type == MSI_X)
		amd_8132_update_hyper_tx(S2IO_BIT_RESET);

	DBG_PRINT(INIT_DBG, "cleanup done\n");
}

module_init(s2io_starter);
module_exit(s2io_closer);


static int check_L2_lro_capable(u8 *buffer, struct iphdr **ip,
		struct tcphdr **tcp, struct RxD_t *rxdp,
		struct s2io_nic *sp)
{
	int ip_off;
	u32 daddr = 0;

	u8 l2_type = (u8)((rxdp->Control_1 >> 37) & 0x7), ip_len;

	if ((l2_type == 0) ||	 /* DIX type */
		(l2_type == 4)) {	 /* DIX type with VLAN */
		ip_off = HEADER_ETHERNET_II_802_3_SIZE;
		/*
		* If vlan stripping is disabled and the frame is VLAN tagged,
		* shift the offset by the VLAN header size bytes.
		*/
		if ((sp->vlan_strip_flag == S2IO_DO_NOT_STRIP_VLAN_TAG) &&
			(rxdp->Control_1 & RXD_FRAME_VLAN_TAG))
			ip_off += HEADER_VLAN_SIZE;
	} else {
		/* LLC, SNAP etc are considered non-mergeable */
		return -1;
	}

	*ip = (struct iphdr *)((u8 *)buffer + ip_off);
	ip_len = (u8)((*ip)->ihl);
	ip_len <<= 2;
	*tcp = (struct tcphdr *)((unsigned long)*ip + ip_len);

	if (lro_enable == S2IO_DONT_AGGR_FWD_PKTS) {

		daddr = (*ip)->daddr;
		/* Check if it is a broadcast or multicast ip */
		if (!IN_MULTICAST(daddr) && (INADDR_BROADCAST != daddr)) {
			struct in_device *in_dev = NULL;
			struct in_ifaddr *ifa = NULL;
			/*
			 * Does this packets destined for this interface?
			 */
			in_dev = sp->dev->ip_ptr;
			if (in_dev != NULL) {
				ifa = in_dev->ifa_list;
				while (ifa != NULL) {
					if (daddr == ifa->ifa_local)
						return 0;
					ifa = ifa->ifa_next;
				}
			}
			/* Packet is to be forwarded. Don't aggregate */
			return -1;
		}
	}

	return 0;
}

static int check_for_socket_match(struct lro *lro, struct iphdr *ip,
				  struct tcphdr *tcp)
{
	DBG_PRINT(INFO_DBG, "%s: Been here...\n", __FUNCTION__);
	if ((lro->iph->saddr != ip->saddr) || (lro->iph->daddr != ip->daddr) ||
		(lro->tcph->source != tcp->source) ||
		(lro->tcph->dest != tcp->dest))
		return -1;
	return 0;
}

#define S2IO_TS_SAVE 2
#define S2IO_TS_VERIFY 1
#define S2IO_TS_UPDATE 0

static int update_tcp_timestamp_slow(struct tcphdr *th, struct lro *lro, int save)
{
	unsigned char *ptr;
	int opt_cnt = 0;
	int length = (th->doff*4) - sizeof(struct tcphdr);

	ptr = (unsigned char *)(th + 1);

	while (length > 0) {
		int opcode=*ptr++;
		int opsize;

		switch (opcode) {
			case TCPOPT_EOL:
				return 1;
			case TCPOPT_NOP:
				length--;
				continue;
			default:
				/* Not sure about this check, but not taking a chance ... */
				if ((opcode == TCPOPT_SACK_PERM) || (opcode == TCPOPT_SACK))
					return 1;
				opsize=*ptr++;
				if (opsize < 2)
					return 1;
				/* don't parse partial options */
				if (opsize > length)
					return 1;
				if (++opt_cnt > 3)
					return 1;
				if (opcode == TCPOPT_TIMESTAMP) {
					if (opsize==TCPOLEN_TIMESTAMP) {

						if (save == S2IO_TS_SAVE){
							lro->cur_tsval = ntohl(* (__be32 *)ptr);
							lro->cur_tsecr = *(__be32 *)(ptr + 4);
						}
						else if (save == S2IO_TS_VERIFY){
						/* Ensure timestamp value increases monotonically */
							if (lro->cur_tsval > ntohl(*((__be32 *)ptr)))
								return -1;
							/* timestamp echo reply should be non-zero */
							if (*((__be32 *)(ptr + 4)) == 0)
								return -1;
						}
						else {
							__be32 *tmp_ptr = (__be32 *)(ptr + 4);
							*tmp_ptr = lro->cur_tsecr;
						}
						return 0;
					}
				}
				ptr+=opsize-2;
				length-=opsize;
		}
	}
	return 1;
}

static int update_tcp_timestamp(struct tcphdr *th, struct lro *lro, int save)
{
	if (th->doff == sizeof(struct tcphdr)>>2) {
		return 1;
	} else if (th->doff == (sizeof(struct tcphdr)>>2)
	 		+(TCPOLEN_TSTAMP_ALIGNED>>2)) {
		__be32 *ptr = (__be32 *)(th + 1);
		if (*ptr == htonl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16)
				  | (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP)) {

			++ptr;
			if (save == S2IO_TS_SAVE){
				lro->cur_tsval = ntohl(*(__be32 *)ptr);
				lro->cur_tsecr = *(__be32 *)(ptr + 1);
			}
			else if (save == S2IO_TS_VERIFY){
				/* Ensure timestamp value increases monotonically */
				if (lro->cur_tsval > ntohl(*((__be32 *)ptr)))
					return -1;

				/* timestamp echo reply should be non-zero */
				if (*((__be32 *)(ptr + 1)) == 0)
					return -1;
			}
			else
				*(ptr + 1) = lro->cur_tsecr;

			return 0;
		}
	}
	return (update_tcp_timestamp_slow(th, lro, save));
}

static inline int get_l4_pyld_length(struct iphdr *ip, struct tcphdr *tcp)
{
	return(ntohs(ip->tot_len) - (ip->ihl << 2) - (tcp->doff << 2));
}

static void initiate_new_session(struct lro *lro,
	struct iphdr *ip, struct tcphdr *tcp, u32 tcp_pyld_len, u16 vlan_tag)
{
	DBG_PRINT(INFO_DBG, "%s: Been here...\n", __FUNCTION__);
	lro->iph = ip;
	lro->tcph = tcp;
	lro->tcp_next_seq = tcp_pyld_len + ntohl(tcp->seq);
	lro->tcp_ack = tcp->ack_seq;
	lro->window = tcp->window;
	lro->sg_num = 1;
	lro->total_len = ntohs(ip->tot_len);
	lro->frags_len = 0;
	lro->vlan_tag = vlan_tag;

	/*
	 * check if we saw TCP timestamp. Other consistency checks have
	 * already been done.
	 */
	if (!update_tcp_timestamp(tcp, lro, S2IO_TS_SAVE))
		lro->saw_ts = 1;

	lro->in_use = 1;
}

static void update_L3L4_header(struct s2io_nic *sp, struct lro *lro,
int ring_no)
{
	struct iphdr *ip = lro->iph;
	struct tcphdr *tcp = lro->tcph;
	u16 nchk;
	struct ring_info *ring = &sp->mac_control.rings[ring_no];

	DBG_PRINT(INFO_DBG, "%s: Been here...\n", __FUNCTION__);

	/* Update L3 header */
	ip->tot_len = htons(lro->total_len);
	ip->check = 0;
	nchk = ip_fast_csum((u8 *)lro->iph, ip->ihl);
	ip->check = nchk;

	/* Update L4 header */
	tcp->ack_seq = lro->tcp_ack;
	tcp->window = lro->window;

	/* Update tsecr field if this session has timestamps enabled */
	if (lro->saw_ts)
		update_tcp_timestamp(tcp, lro, S2IO_TS_UPDATE);

	/* Update counters required for calculation of
	 * average no. of packets aggregated.
	 */
	ring->rx_ring_stat->sw_lro_stat.sum_avg_pkts_aggregated += lro->sg_num;
	ring->rx_ring_stat->sw_lro_stat.num_aggregations++;
}

static void aggregate_new_rx(struct lro *lro, struct iphdr *ip,
		struct tcphdr *tcp, u32 l4_pyld)
{
	DBG_PRINT(INFO_DBG, "%s: Been here...\n", __FUNCTION__);
	lro->total_len += l4_pyld;
	lro->frags_len += l4_pyld;
	lro->tcp_next_seq += l4_pyld;
	lro->sg_num++;

	/* Update ack seq no. and window ad(from this pkt) in LRO object */
	lro->tcp_ack = tcp->ack_seq;
	lro->window = tcp->window;

	if (lro->saw_ts)
		update_tcp_timestamp(tcp, lro, S2IO_TS_SAVE);
}

static int verify_l3_l4_lro_capable(struct lro *l_lro, struct iphdr *ip,
			struct tcphdr *tcp, u32 tcp_pyld_len, u8 agg_ack)
{
	DBG_PRINT(INFO_DBG, "%s: Been here...\n", __FUNCTION__);

	if (!agg_ack && !tcp_pyld_len) {
		/* Runt frame or a pure ack */
		return -1;
	}

	if (ip->ihl != 5) /* IP has options */
		return -1;

	/*
	 * TODO: Currently works with normal ECN. No support for
	 * ECN with nonces(RFC3540).
	 */
	/* If we see CE codepoint in IP header, packet is not mergeable */
	if (INET_ECN_is_ce(ipv4_get_dsfield(ip)))
		return -1;

	/* If we see ECE or CWR flags in TCP header, packet is not mergeable */

	if (tcp->urg || tcp->psh || tcp->rst || tcp->syn || tcp->fin ||
				    tcp->ece || tcp->cwr || !tcp->ack) {
		/*
		 * Currently recognize only the ack control word and
		 * any other control field being set would result in
		 * flushing the LRO session
		 */
		return -1;
	}

	if (l_lro)
		if (update_tcp_timestamp(tcp, l_lro, S2IO_TS_VERIFY) == -1)
			return -1;

	return 0;
}

static int
s2io_club_tcp_session(struct ring_info *ring, u8 *buffer, u8 **tcp,
	u32 *tcp_len, struct lro **lro, struct RxD_t *rxdp,
	struct s2io_nic *sp)
{
	struct iphdr *ip;
	struct tcphdr *tcph;
	int ret = 0, i;
	u16 vlan_tag = 0;
	struct swLROStat *lro_stats = &ring->rx_ring_stat->sw_lro_stat;

	if (!(ret = check_L2_lro_capable(buffer, &ip, (struct tcphdr **)tcp,
					 rxdp, sp))) {
		DBG_PRINT(INFO_DBG,"IP Saddr: %x Daddr: %x\n",
			  ip->saddr, ip->daddr);
	} else
		return ret;

	vlan_tag = RXD_GET_VLAN_TAG(rxdp->Control_2);
	tcph = (struct tcphdr *)*tcp;
	*tcp_len = get_l4_pyld_length(ip, tcph);

	for (i=0; i<MAX_LRO_SESSIONS; i++) {
		struct lro *l_lro = &ring->lro0_n[i];
		if (l_lro->in_use) {
			if (check_for_socket_match(l_lro, ip, tcph))
				continue;
			/* Sock pair matched */
			*lro = l_lro;

			if ((*lro)->tcp_next_seq != ntohl(tcph->seq)) {
				DBG_PRINT(INFO_DBG,
					"%s:Out of order. expected "
					"0x%x, actual 0x%x\n", __FUNCTION__,
					(*lro)->tcp_next_seq,
					ntohl(tcph->seq));

				lro_stats->outof_sequence_pkts++;
				ret = LRO_FLUSH_BOTH;
				break;
			}

			if (!verify_l3_l4_lro_capable(l_lro, ip, tcph,
					*tcp_len, ring->aggr_ack))
				ret = LRO_AGGR_PACKET; /* Aggregate */
			else
				ret = LRO_FLUSH_BOTH; /* Flush both */

			break;
		}
	}

	if (ret == 0) {
		/* Before searching for available LRO objects,
		 * check if the pkt is L3/L4 aggregatable. If not
		 * don't create new LRO session. Just send this
		 * packet up.
		 */
		if (verify_l3_l4_lro_capable(NULL, ip, tcph, *tcp_len,
				ring->aggr_ack))
			return 5;

		for (i=0; i<MAX_LRO_SESSIONS; i++) {
			struct lro *l_lro = &ring->lro0_n[i];
			if (!(l_lro->in_use)) {
				*lro = l_lro;
				ret = LRO_BEG_AGGR; /* Begin anew */
				break;
			}
		}
	}

	if (ret == 0) { /* sessions exceeded */
		DBG_PRINT(INFO_DBG,"%s:All LRO sessions already in use\n",
			  __FUNCTION__);
		*lro = NULL;
		return ret;
	}

	if (LRO_AGGR_PACKET == ret) {
		aggregate_new_rx(*lro, ip, tcph, *tcp_len);
		if ((*lro)->sg_num == sp->lro_max_aggr_per_sess) {
			update_L3L4_header(sp, *lro, ring->ring_no);
			ret = LRO_FLUSH_SESSION; /* Flush the LRO */
		}
	} else if (LRO_BEG_AGGR == ret)
		initiate_new_session(*lro, ip, tcph, *tcp_len,
			vlan_tag);
	else if (LRO_FLUSH_BOTH == ret)
		update_L3L4_header(sp, *lro, ring->ring_no);
	else
		DBG_PRINT(ERR_DBG, "%s:LRO Unhandled.\n", __FUNCTION__);

	return ret;
}

static void lro_append_pkt(struct s2io_nic *sp, struct lro *lro,
	struct sk_buff *skb, u32 tcp_len, u8 aggr_ack, int ring_no)
{
	struct sk_buff *first = lro->parent;
	struct sk_buff *frag_list = NULL;
	struct sk_buff *data_skb = NULL;
	int data_skb_true_size = 0;
	struct ring_info *ring = &sp->mac_control.rings[ring_no];
	struct swLROStat *lro_stats = &ring->rx_ring_stat->sw_lro_stat;

	first->len += tcp_len;
	first->data_len = lro->frags_len;
	lro_stats->clubbed_frms_cnt++;

	if (aggr_ack && (tcp_len == 0)) {
		dev_kfree_skb_any(skb);
		return;
	}

	if (sp->rxd_mode == RXD_MODE_5) {
		/* leave the Buf2 which contains l3l4 header */
		data_skb = skb_shinfo(skb)->frag_list;
		data_skb_true_size = data_skb->truesize;
		skb_shinfo(skb)->frag_list = NULL;
		skb->data_len = 0;
		skb->len = skb->len - tcp_len;
		/* calcualate the size of the skbs containg only data*/
		for (frag_list = data_skb; frag_list->next;
			frag_list = frag_list->next)
			data_skb_true_size += frag_list->truesize;

		/* single header skb with truesize*/
		skb->truesize -= data_skb_true_size;
		/* Free the SKB containing the l3l4 header*/
		dev_kfree_skb(skb);
		/* skb will now point to the actual l3l4 data*/
		skb = data_skb;
		skb->truesize = data_skb_true_size;
	} else
		skb_pull(skb, (skb->len - tcp_len));

	frag_list = skb_shinfo(first)->frag_list;
	if (frag_list) {
		if (lro->last_frag) /* valid only in 5 buffer mode */
			lro->last_frag->next = skb;
	} else
		skb_shinfo(first)->frag_list = skb;

	first->truesize += skb->truesize; /*updating skb->truesize */

	if (sp->rxd_mode == RXD_MODE_5) {
		frag_list = data_skb;
		if (frag_list) {
			/* Traverse to the end of the list*/
			for (; frag_list->next; frag_list = frag_list->next);
		}
		lro->last_frag = frag_list;
	} else
		lro->last_frag = skb;

	return;
}

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)

static void do_s2io_prog_da_steering(struct s2io_nic *sp, int index)
{
	int section;
	u64 val64;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	section = index/32;
	DBG_PRINT(INFO_DBG, "section %d enabled\n", section);

	val64 = readq(&bar0->rts_da_cfg);

	switch (section) {
		case 0:
			val64 |= RTS_DA_SECT0_EN;
			break;
		case 1:
			val64 |= RTS_DA_SECT1_EN;
			break;
		case 2:
			val64 |= RTS_DA_SECT2_EN;
			break;
		case 3:
			val64 |= RTS_DA_SECT3_EN;
			break;
		case 4:
			val64 |= RTS_DA_SECT4_EN;
			break;
		case 5:
			val64 |= RTS_DA_SECT5_EN;
			break;
		case 6:
			val64 |= RTS_DA_SECT6_EN;
			break;
		case 7:
			val64 |= RTS_DA_SECT7_EN;
			break;
		default:
			DBG_PRINT(ERR_DBG, "invalid section %d ", section);
			break;
	}

	writeq(val64,  &bar0->rts_da_cfg);

	/* Enabled enhanced RTS steering */
	val64 = readq(&bar0->rts_ctrl);
	val64 |= RTS_CTRL_ENHANCED;
	writeq(val64, &bar0->rts_ctrl);
}

static int
s2io_rts_mac_disable(struct net_device *dev, int index)
{
	int rval;
	struct s2io_nic *sp = dev->priv;
	u64 broadcast_macadd;
	broadcast_macadd = S2IO_DISABLE_MAC_ENTRY;

	if((rval = do_s2io_add_mac(sp, broadcast_macadd, index)))
		return rval;

	/* store the new mac list from CAM */
	do_s2io_copy_mac_addr(sp, index, broadcast_macadd);
	return SUCCESS;
}

static int
s2io_rts_mac_enable(struct net_device *dev, int index, u8 *macaddr)
{
	struct s2io_nic *sp = dev->priv;
	register u64 mac_addr = 0;
	int rval;
	int i = 0;
	struct config_param *config = &sp->config;

	if (index >= config->max_mac_addr) {
		DBG_PRINT(ERR_DBG, "%s: s2io_rts_mac_enable failed - "
		"index (%d) > max supported (%d)\n",
		dev->name, index, config->max_mac_addr);
		return FAILURE;
	}

	for (i = 0; i < ETH_ALEN; i++) {
		mac_addr <<= 8;
		mac_addr |= macaddr[i];
	}

	if((rval = do_s2io_add_mac(sp, mac_addr, index)))
		return rval;

	//Storing internally
	do_s2io_copy_mac_addr(sp, index, mac_addr);

	return SUCCESS;
}

static int
s2io_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
s2io_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct s2io_nic *nic = args->netdev->priv;
	struct config_param *config = &nic->config;

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		if (config->tx_fifo_num > 1) {
			args->count = config->tx_fifo_num - 1;
		}
		else {
			args->count = 0;
		}
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		if (config->rx_ring_num > 1) {
			args->count = config->rx_ring_num - 1;
		}
		else {
			args->count = 0;
		}
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else {
		printk("s2io_get_channel_count: invalid queue type - %s\n", args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	struct s2io_nic *nic = args->netdev->priv;
	struct config_param *config = &nic->config;
	/*
	 * Tx does not support filters
	 */
	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		printk("s2io_get_filter_count: tx queues not supported - %s\n", args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	args->count = S2IO_MAX_AVAIL_FILTERS_PER_RXQ(config);
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
s2io_validate_queueid(struct net_device *netdev,
			vmknetddi_queueops_queueid_t qid)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;
	u16 cidx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(qid)) {
		if (cidx > config->rx_ring_num) {
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		else {
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	else if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(qid)) {
		if (cidx > config->tx_fifo_num) {
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		else {
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	else {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_validate_filterid(struct net_device *netdev,
		       vmknetddi_queueops_filterid_t filterid)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;

	u16 fidx = VMKNETDDI_QUEUEOPS_FILTERID_VAL(filterid);

	if (fidx >= S2IO_MAX_AVAIL_RX_FILTERS(config)) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	else {
		return VMKNETDDI_QUEUEOPS_OK;
	}
}

static int
s2io_alloc_tx_queue(struct net_device *netdev,
		      vmknetddi_queueops_queueid_t *p_qid,
			  u16 *queue_mapping)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;

	if (config->n_tx_fifo_allocated >= config->tx_fifo_num) {
		printk("s2io_alloc_tx_queue: no free tx queues, currently allocated %d for %s\n", 
							config->n_tx_fifo_allocated, netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	else {
		int i;
		for (i = 1; i < config->tx_fifo_num; i++) {
			if (!config->tx_cfg[i].allocated) {
				config->tx_cfg[i].allocated = TRUE;
				*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
				*queue_mapping = i;
				config->n_tx_fifo_allocated++;
				printk("s2io_alloc_tx_queue: allocated queue %d, currently allocated %d for %s\n", 
							i, config->n_tx_fifo_allocated, netdev->name);
				return VMKNETDDI_QUEUEOPS_OK;
			}
		}
	}
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
s2io_alloc_rx_queue(struct net_device *netdev,
		      vmknetddi_queueops_queueid_t *p_qid)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;

	if (config->n_rx_ring_allocated >= config->rx_ring_num) {
		printk("s2io_alloc_rx_queue: no free rx queues, currently allocated %d for %s\n", 
							config->n_rx_ring_allocated, netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	else {
		int i;
		spin_lock(&config->netqueue_lock);
		for (i=1; i < config->rx_ring_num; i++) {
			if (!config->rx_cfg[i].allocated) {
				config->rx_cfg[i].allocated = TRUE;
				*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(i);
				config->n_rx_ring_allocated++;
				printk ("s2io_alloc_rx_queue: Allocating queue no. %d, total so far %d for %s\n", 
									i, config->n_rx_ring_allocated, netdev->name);
				spin_unlock(&config->netqueue_lock);
				return VMKNETDDI_QUEUEOPS_OK;
			}
		}
		spin_unlock(&config->netqueue_lock);
		printk("s2io_alloc_rx_queue: no free rx queues found! current allocated %d for %s\n", 
							config->n_rx_ring_allocated, netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return s2io_alloc_tx_queue(args->netdev, &args->queueid,
		                           &args->queue_mapping);
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return s2io_alloc_rx_queue(args->netdev, &args->queueid);
	}
	else {
		printk("s2io_alloc_queue: invalid queue type \n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_free_tx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (!config->tx_cfg[queue].allocated) {
		printk("s2io_free_tx_queue: tx queue not allocated for %s\n", netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	config->tx_cfg[queue].allocated = FALSE;
	config->n_tx_fifo_allocated--;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
s2io_free_rx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct s2io_nic *nic = netdev->priv;
	struct config_param *config = &nic->config;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);


	if (!config->rx_cfg[queue].allocated) {
		printk("s2io_free_rx_queue: rx queue not allocated but trying to be freed %d, count %d for %s\n", 
							queue, config->n_rx_ring_allocated, netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	spin_lock(&config->netqueue_lock);
	config->rx_cfg[queue].allocated = FALSE;
	config->n_rx_ring_allocated--;
	printk("s2io_free_rx_queue: rx queue freed %d , count %d for %s\n", 
						queue, config->n_rx_ring_allocated, netdev->name);
	spin_unlock(&config->netqueue_lock);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
s2io_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
#if 0
	// PR 382875 - Validation may fail as free queue could be coming
	// down after a change in number of queues (due to MTU change).
	// Suspend all validations for now.
	if (s2io_validate_queueid(args->netdev, args->queueid)
						!= VMKNETDDI_QUEUEOPS_OK) {
		printk("s2io_free_queue: failed to validate queue id\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
#endif

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return s2io_free_tx_queue(args->netdev, args->queueid);
	}
	else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return s2io_free_rx_queue(args->netdev, args->queueid);
	}
	else {
		printk("s2io_free_queue: invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	//XXX: not implemented yet
	printk("s2io_get_queue_vector: not implemented\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
s2io_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
                args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
                args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else {
		printk("s2io_get_default_queue: invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	struct s2io_nic *nic;
	struct config_param *config;
	s2io_rx_filter_t *queue_filter;
	struct rx_ring_config *queue_cfg;
	int status, rval, fidx;
	u8 *macaddr;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		printk("s2io_apply_rx_filter: not an rx queue 0x%x, %s\n",
			args->queueid, args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (vmknetddi_queueops_get_filter_class(&args->filter)
					!= VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		printk("s2io_apply_rx_filter: only mac filters supported, %s \n", args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	queue_filter = NULL;
	nic = args->netdev->priv;
	config = &nic->config;
	queue_cfg = &config->rx_cfg[queue];

	if (!queue_cfg->allocated) {
		printk("s2io_apply_rx_filter: queue not alloacted for %s \n", args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	spin_lock(&config->netqueue_lock);

	if (queue_cfg->active_filter_count >= S2IO_MAX_AVAIL_FILTERS_PER_RXQ(config)) {
		printk("s2io_apply_rx_filter: filter count exceeded, %s\n", args->netdev->name);
		status = VMKNETDDI_QUEUEOPS_ERR;
		goto out;
	}

	for (fidx=0; fidx < S2IO_MAX_AVAIL_FILTERS_PER_RXQ(config); fidx++) {
		if (!queue_cfg->filter[fidx].active) {
			queue_filter = &queue_cfg->filter[fidx];
			break;
		}
	}
	if (!queue_filter) {
		printk("s2io_apply_rx_filter: all filters slots are active, %s \n", args->netdev->name);
		status = VMKNETDDI_QUEUEOPS_ERR;
		goto out;
	}

	macaddr = vmknetddi_queueops_get_filter_macaddr(&args->filter);

	rval = s2io_rts_mac_enable(args->netdev, queue+fidx*8, macaddr);
	if (rval == SUCCESS) {
		queue_cfg->active_filter_count++;
		queue_filter->active = TRUE;
		memcpy(queue_filter->macaddr, macaddr, ETH_ALEN);
		args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(fidx);
		status = VMKNETDDI_QUEUEOPS_OK;
		printk("s2io_apply_rx_filter: applied filter %d to queue %d for %s \n", 
							fidx, queue, args->netdev->name);
	}
	else {
		printk("s2io_apply_rx_filter: failed to apply filter %d to queue %d for %s \n", 
							fidx, queue, args->netdev->name);
		status = VMKNETDDI_QUEUEOPS_ERR;
	}

out:
	spin_unlock(&config->netqueue_lock);
	return status;
}

static int
s2io_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	int rval;
	struct s2io_nic *nic = args->netdev->priv;
	struct config_param *config = &nic->config;
	u16 cidx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fidx = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct rx_ring_config *queue_cfg;
	s2io_rx_filter_t *queue_filter;

#if 0
	// PR 382875 - Validation may fail as free queue could be coming
	// down after a change in number of queues (due to MTU change).
	// Suspend all validations for now.
	if (s2io_validate_queueid(args->netdev, args->queueid)
						!= VMKNETDDI_QUEUEOPS_OK) {
		printk("s2io_remove_rx_filter: failed to validate queue id\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
#endif

	if (s2io_validate_filterid(args->netdev, args->filterid)
						!= VMKNETDDI_QUEUEOPS_OK) {
		printk("s2io_remove_rx_filter: failed to validate filter id for %s\n", args->netdev->name);		
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	queue_cfg = &config->rx_cfg[cidx];
	queue_filter = &queue_cfg->filter[fidx];

	spin_lock(&config->netqueue_lock);
	rval = s2io_rts_mac_disable(args->netdev, cidx+fidx*8);
	if (rval == SUCCESS) {
		queue_filter->active = FALSE;
		memset(queue_filter->macaddr, 0xff, ETH_ALEN);
		queue_cfg->active_filter_count--;
		spin_unlock(&config->netqueue_lock);
		printk("s2io_remove_rx_filter: removed filter %d from queue %d for %s \n", 
							fidx, cidx, args->netdev->name);
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else {
		spin_unlock(&config->netqueue_lock);
		printk("s2io_remove_rx_filter: failed to remove filter %d from queue %d for %s \n", 
							fidx, cidx, args->netdev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
s2io_get_queue_stats(void *args)
{
	//XXX: not implemented
	printk("s2io_get_queue_stats: not implemented\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
s2io_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);
}

static int
s2io_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	switch (op) {
		case VMKNETDDI_QUEUEOPS_OP_GET_VERSION:
		return s2io_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
		return s2io_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		return s2io_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		return s2io_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		return s2io_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		return s2io_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		return s2io_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		return s2io_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		return s2io_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		return s2io_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;

		case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		return s2io_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		break;

		default:
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

#endif /* defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__) */

/**
* s2io_io_error_detected - called when PCI error is detected
* @pdev: Pointer to PCI device
* @state: The current pci connection state
*
* This function is called after a PCI bus error affecting
* this device has been detected.
*/
static pci_ers_result_t s2io_io_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct s2io_nic *sp = netdev->priv;

	netif_device_detach(netdev);

	if (netif_running(netdev)) {
		/* Bring down the card, while avoiding PCI I/O */
		do_s2io_card_down(sp, 0);
#if defined (__VMKLNX__)
		printk ("%s: Bringing card down for %s\n", __FUNCTION__, netdev->name);
#endif
	}
	pci_disable_device(pdev);

	return PCI_ERS_RESULT_NEED_RESET;
}

/**
* s2io_io_slot_reset - called after the pci bus has been reset.
* @pdev: Pointer to PCI device
*
* Restart the card from scratch, as if from a cold-boot.
* At this point, the card has exprienced a hard reset,
* followed by fixups by BIOS, and has its config space
* set up identically to what it was at cold boot.
*/
static pci_ers_result_t s2io_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct s2io_nic *sp = netdev->priv;

	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "s2io: "
				"Cannot re-enable PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	s2io_reset(sp);

	return PCI_ERS_RESULT_RECOVERED;
}

/**
* s2io_io_resume - called when traffic can start flowing again.
* @pdev: Pointer to PCI device
*
* This callback is called when the error recovery driver tells
* us that its OK to resume normal operation.
*/
static void s2io_io_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct s2io_nic *sp = netdev->priv;

	if (netif_running(netdev)) {
		if (s2io_card_up(sp)) {
			printk(KERN_ERR "s2io: "
				"Can't bring device back up after reset.\n");
			return;
		}

		if (s2io_set_mac_addr(netdev, netdev->dev_addr) == FAILURE) {
			s2io_card_down(sp);
			printk(KERN_ERR "s2io: "
				"Can't resetore mac addr after reset.\n");
			return;
		}
	}

	netif_device_attach(netdev);
}

/* Following code not present in kernel release */
#ifndef __VMKLNX__
int s2io_ioctl_util(struct s2io_nic *sp, struct ifreq *rq, int cmd)
{
	int ret;
	u8 mac_addr[6];
	switch (cmd) {
	/* Private IOCTLs used by the util tool. */
	case SIOCDEVPRIVATE + 4:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		struct config_param *config = &sp->config;
		int i, j;
		int lst_size = config->max_txds * sizeof(struct TxD);
		void *to_buf = io->buffer, *fr_buf;
		io->size = 0;

		DBG_PRINT(INFO_DBG, "Tx get_offset %d, "
			  "put_offset %d \n",
			  sp->mac_control.fifos[0].
			  tx_curr_get_info.offset,
			  sp->mac_control.fifos[0].
			  tx_curr_put_info.offset);

		for (i = 0; i < config->tx_fifo_num; i++) {
			int fifo_len = config->tx_cfg[i].fifo_len;
			io->size += fifo_len;
			for (j = 0; j < fifo_len; j++) {
				fr_buf = sp->mac_control.fifos[i].
						list_info[j].
						list_virt_addr;
				ret = copy_to_user(to_buf,
						   fr_buf,
						   lst_size);
				if (ret)
					return -EFAULT;
				to_buf += lst_size;
			}
		}
		io->size *= lst_size;
		break;
	}

	case SIOCDEVPRIVATE + 5:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		int i, j, row_sz;

		io->size = sp->mac_control.stats_mem_sz;
		s2io_updt_stats(sp);

		row_sz = (io->size + 15)/16;
		DBG_PRINT(INFO_DBG, "Stats Dump:\n");
		for (i = 0; i <  row_sz; i++) {
			DBG_PRINT(INFO_DBG, "%03x: ", (i*16));
			for (j = 0; j < 16; j++) {
				u8 *x = (u8 *)
				(sp->mac_control.stats_mem + (i*16)+j);
				if (j == 8)
					DBG_PRINT(INFO_DBG, " ");
				DBG_PRINT(INFO_DBG, "%02x ", *x);
				if (((i*16)+j) == io->size-1)
					goto end;
			}
			DBG_PRINT(INFO_DBG, "\n");
		}
end:
		DBG_PRINT(INFO_DBG, "\n");

		ret = copy_to_user((void *)io->buffer,
			(void *)sp->mac_control.stats_mem,
			io->size);
		if (ret)
			return -EFAULT;
		break;
	}

	case SIOCDEVPRIVATE + 7:
	{
		unsigned int i = 0, j = 0;
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;

		DBG_PRINT(INFO_DBG, "Rx get_offset %d, "
			  "put_offset %d \n",
			  sp->mac_control.rings[0].
			  rx_curr_get_info.offset,
			  sp->mac_control.rings[0].
			  rx_curr_put_info.offset);

		io->size = 4096 * rx_ring_sz[0];
		while (i < sp->config.rx_cfg[0].num_rxd) {
			char *c =
			    (char *) sp->mac_control.rings[0].
				rx_blocks[j].block_virt_addr;
			ret = copy_to_user(
				(io->buffer + (j * 4096)),
				c, 4096);
			if (ret)
				return -EFAULT;
			i += (MAX_RXDS_PER_BLOCK_1 + 1);
			j++;
		}
		break;
	}

	case SIOCDEVPRIVATE + 8:
	{
		struct spdm_cfg *io = (struct spdm_cfg *)rq->ifr_data;
		spdm_data_processor(io, sp);
		break;
	}

	case SIOCDEVPRIVATE + 9:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		u64 val64;
		if (!is_s2io_card_up(sp)) {
			DBG_PRINT(ERR_DBG,
				"%s: Device is down!!\n", __FUNCTION__);
			break;
		}

		val64 = readq((sp->bar0 + io->offset));
		io->value = val64;
		break;
	}

	case SIOCDEVPRIVATE + 10:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		writeq(io->value, (sp->bar0 + io->offset));
		break;
	}

	case SIOCDEVPRIVATE + 11:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		sp->exec_mode = io->value;
		break;
	}

	case SIOCDEVPRIVATE + 12:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		mac_addr[5] = (u8) (io->value);
		mac_addr[4] = (u8) (io->value >> 8);
		mac_addr[3] = (u8) (io->value >> 16);
		mac_addr[2] = (u8) (io->value >> 24);
		mac_addr[1] = (u8) (io->value >> 32);
		mac_addr[0] = (u8) (io->value >> 40);
		do_s2io_prog_unicast(sp->dev, mac_addr);
		break;
	}

	case SIOCDEVPRIVATE + 13:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		mac_addr[5] = (u8) (io->value);
		mac_addr[4] = (u8) (io->value >> 8);
		mac_addr[3] = (u8) (io->value >> 16);
		mac_addr[2] = (u8) (io->value >> 24);
		mac_addr[1] = (u8) (io->value >> 32);
		mac_addr[0] = (u8) (io->value >> 40);
		do_s2io_add_mc(sp, mac_addr);
		break;
	}

	case SIOCDEVPRIVATE + 14:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		do_s2io_delete_unicast_mc(sp, io->value);
		break;
	}

	case SIOCDEVPRIVATE + 15:
	{
		struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
		io->value = do_s2io_read_unicast_mc(sp, io->offset);
		break;
	}

	case SIOCDEVPRIVATE + 3:
	{
		general_info(sp, rq);
		break;
	}

	default:
		return -EOPNOTSUPP;
	}
	return SUCCESS;
}

/*General Info*/
void general_info(struct s2io_nic *sp, struct ifreq *rq)
{
	struct net_device *dev = sp->dev;
	struct ioctlInfo *io = (struct ioctlInfo *) rq->ifr_data;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	register u64 val64 = 0;
	int mode, i;
	u16 pci_cmd;
	u8 data;

	/*Get current values of per ring msix interrupt counter*/
	for (i = 0; i < 8; i++)
		io->ginfo.rx_msix_intr_cnt[i] =
			sp->mac_control.rings[i].msix_intr_cnt;
	io->ginfo.tx_intr_cnt = sp->tx_intr_cnt;
	io->ginfo.rx_intr_cnt = sp->rx_intr_cnt;
	io->ginfo.deviceid = sp->pdev->device;
	io->ginfo.vendorid = PCI_VENDOR_ID_S2IO;
	strcpy(io->ginfo.driver_version, s2io_driver_version);
	io->ginfo.intr_type = sp->config.intr_type;
	io->ginfo.tx_fifo_num = sp->config.tx_fifo_num;
	io->ginfo.rx_ring_num = sp->config.rx_ring_num;
	io->ginfo.rxd_mode = sp->rxd_mode;
	io->ginfo.lro = sp->lro;
	io->ginfo.lro_max_pkts = sp->lro_max_aggr_per_sess;
	io->ginfo.napi = sp->config.napi;
	io->ginfo.rx_steering_type = sp->config.rx_steering_type;
	io->ginfo.rth_mask = rth_mask;
	io->ginfo.vlan_tag_strip = sp->config.vlan_tag_strip;
	io->ginfo.device_type = sp->device_type;
	io->ginfo.mtu = dev->mtu;
	pci_read_config_byte(sp->pdev, PCI_LATENCY_TIMER, &data);
	io->ginfo.latency_timer = data;
	io->ginfo.rx_csum = sp->rx_csum;
	if (sp->device_type == XFRAME_II_DEVICE)
		io->ginfo.tx_csum = ((dev->features & NETIF_F_HW_CSUM)? 1: 0);
	else
		io->ginfo.tx_csum = ((dev->features & NETIF_F_IP_CSUM)? 1: 0);

	io->ginfo.sg = ((dev->features & NETIF_F_SG)?1:0);
	io->ginfo.perm_addr = do_s2io_read_unicast_mc(sp, 0);

	pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER, &(pci_cmd));
	io->ginfo.pcix_cmd = pci_cmd;
	io->ginfo.tx_urng = readq(&bar0->tti_data1_mem);
	io->ginfo.rx_urng = readq(&bar0->rti_data1_mem);
	io->ginfo.tx_ufc  = readq(&bar0->tti_data2_mem);
	io->ginfo.rx_ufc  = readq(&bar0->rti_data2_mem);
#ifndef NETIF_F_UFO
	io->ginfo.ufo = 0;
#else
	io->ginfo.ufo = 0;
	if ((sp->device_type == XFRAME_II_DEVICE) && ufo)
		io->ginfo.ufo = 1;
#endif
#ifndef NETIF_F_TSO
	io->ginfo.tso = 0;
#else
	io->ginfo.tso = 1;
#endif
	for (i = 0; i < sp->config.tx_fifo_num; i++)
		io->ginfo.fifo_len[i] = sp->config.tx_cfg[i].fifo_len;

	for (i = 0; i < sp->config.rx_ring_num; i++)
		io->ginfo.rx_ring_size[i] = rx_ring_sz[i];

	io->ginfo.rth_bucket_size = ((sp->config.rx_ring_num + 1)/2);
	val64 = readq(&bar0->pci_mode);
	mode = (u8)GET_PCI_MODE(val64);
	if (val64 & PCI_MODE_UNKNOWN_MODE)
		strcpy(io->ginfo.pci_type, "Unknown PCI mode");
	if (s2io_on_nec_bridge(sp->pdev))
		strcpy(io->ginfo.pci_type, "PCI-E bus");

	switch (mode) {
	case PCI_MODE_PCI_33:
		strcpy(io->ginfo.pci_type, "33MHz PCI bus");
		break;
	case PCI_MODE_PCI_66:
		strcpy(io->ginfo.pci_type, "66MHz PCI bus");
		break;
	case PCI_MODE_PCIX_M1_66:
		strcpy(io->ginfo.pci_type, "66MHz PCIX(M1) bus");
		break;
	case PCI_MODE_PCIX_M1_100:
		strcpy(io->ginfo.pci_type, "100MHz PCIX(M1) bus");
		break;
	case PCI_MODE_PCIX_M1_133:
		strcpy(io->ginfo.pci_type, "133MHz PCIX(M1) bus");
		break;
	case PCI_MODE_PCIX_M2_66:
		strcpy(io->ginfo.pci_type, "133MHz PCIX(M2) bus");
		break;
	case PCI_MODE_PCIX_M2_100:
		strcpy(io->ginfo.pci_type, "200MHz PCIX(M2) bus");
		break;
	case PCI_MODE_PCIX_M2_133:
		strcpy(io->ginfo.pci_type, "266MHz PCIX(M2) bus");
		break;
	default:
		strcpy(io->ginfo.pci_type, "Unsupported bus speed");
	}

	if (sp->config.rx_steering_type == RTH_STEERING
		 || sp->config.rx_steering_type == RTH_STEERING_DEFAULT) {
		val64 = readq(&bar0->rts_rth_cfg);

		switch (rth_protocol) {
		case 1:
			if (val64 & RTS_RTH_TCP_IPV4_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_TCP_IPV4_EN");
			break;
		case 2:
			if (val64 & RTS_RTH_UDP_IPV4_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_UDP_IPV4_EN");
			break;
		case 3:
			if (val64 & RTS_RTH_IPV4_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_IPV4_EN");
			break;
		case 4:
			if (val64 & RTS_RTH_TCP_IPV6_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_TCP_IPV6_EN");
			break;
		case 5:
			if (val64 & RTS_RTH_UDP_IPV6_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_UDP_IPV6_EN");
			break;
		case 6:
			if (val64 & RTS_RTH_IPV6_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_IPV6_EN");
			break;
		case 7:
			if (val64 & RTS_RTH_TCP_IPV6_EX_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_TCP_IPV6_EX_EN");
			break;
		case 8:
			if (val64 & RTS_RTH_UDP_IPV6_EX_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_UDP_IPV6_EX_EN");
			break;
		case 9:
			if (val64 & RTS_RTH_IPV6_EX_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_IPV6_EX_EN");
			break;
		default:
			if (val64 & RTS_RTH_TCP_IPV4_EN)
				strcpy(io->ginfo.rth_steering_mask,
					"RTS_RTH_TCP_IPV4_EN");
		}
	}
}
#endif
/**
 * spdm_extract_table -
 */
static int spdm_extract_table(void *data, struct s2io_nic *nic)
{
	struct XENA_dev_config *bar0 = (struct XENA_dev_config *) nic->bar0;
	u64 val64, table_content;
	int line, entry = 0;

	while (entry < nic->spdm_entry) {
		u64 *tmp = (u64 *)((u8 *)data + (0x40 * entry));
		for (line = 0; line < 8; line++, tmp++) {
			val64 = RTS_RTH_SPDM_MEM_CTRL_OFFSET(entry) |
				RTS_RTH_SPDM_MEM_CTRL_LINE_SEL(line) |
				RTS_RTH_SPDM_MEM_CTRL_STROBE;
			writeq(val64, &bar0->rts_rth_spdm_mem_ctrl);
			if (wait_for_cmd_complete(&bar0->rts_rth_spdm_mem_ctrl,
				RTS_RTH_SPDM_MEM_CTRL_STROBE,
				S2IO_BIT_RESET) == FAILURE)
				return FAILURE;
			table_content = readq(&bar0->rts_rth_spdm_mem_data);
			if (!line && !table_content)
				goto end;
			*tmp = table_content;
		}
		entry++;
	}
end:
	return entry;
}

static int spdm_clear_table(struct s2io_nic *nic)
{
	struct XENA_dev_config *bar0 = (struct XENA_dev_config *) nic->bar0;
	u64 val64;
	u32 start_tbl, entry_offset, i;
	void *element_addr;

	val64 = readq(&bar0->spdm_bir_offset);
	start_tbl = (u32)(val64 >> 32);
	start_tbl *= 8;

	for (entry_offset = 0; entry_offset < nic->spdm_entry; entry_offset++) {
		element_addr = (void *)((u8 *)bar0 + start_tbl + entry_offset);
		for (i = 0; i < 8; i++)
			writeq(0, element_addr + (i * 8));
		msleep(20);
	}
	nic->spdm_entry = 0;
	return 0;
}

/**
 * s2io_program_spdm_table -
 */
static int s2io_program_spdm_table(struct spdm_cfg *info, int entry,
		struct s2io_nic *nic)
{
	struct XENA_dev_config *bar0 = (struct XENA_dev_config *) nic->bar0;
	u64 val64;
	unsigned long tmp;
	int ring;
	u32 start_tbl, entry_offset;
	struct spdm_entry element;
	void *element_addr;
	u16 sprt, dprt;
	u32 sip, dip, hash;

	ring = info->t_queue;
	entry = nic->spdm_entry;

	val64 = readq(&bar0->spdm_bir_offset);
	start_tbl = (u32)(val64 >> 32);
	start_tbl *= 8;
	entry_offset = entry * sizeof(struct spdm_entry);

	element_addr = (void *)((u8 *)bar0 + start_tbl + entry_offset);
	tmp = (unsigned long)element_addr;

	sprt = info->sprt;
	dprt = info->dprt;
	sip = info->sip;
	dip = info->dip;
	element.port_n_entry_control_0 = SPDM_PGM_L4_SRC_PORT(sprt) |
			SPDM_PGM_L4_DST_PORT(dprt) |
			SPDM_PGM_TARGET_QUEUE(ring);
	if (info->sprt) {
		if (rth_protocol == 1) /* TCP */
			element.port_n_entry_control_0 |= SPDM_PGM_IS_TCP |
				SPDM_PGM_IS_IPV4;
		else if (rth_protocol == 2) /* UDP */
			element.port_n_entry_control_0 |= SPDM_PGM_IS_IPV4;
	}
	writeq(element.port_n_entry_control_0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;

	element.ip.ipv4_sa_da = sip;
	element.ip.ipv4_sa_da <<= 32;
	element.ip.ipv4_sa_da |= dip;
	writeq(element.ip.ipv4_sa_da, element_addr);

	tmp += 8;
	element_addr = (void *)tmp;
	writeq(0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;
	writeq(0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;
	writeq(0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;
	writeq(0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;
	writeq(0, element_addr);
	tmp += 8;
	element_addr = (void *)tmp;

	hash = info->hash;
	element.hash_n_entry_control_1 = SPDM_PGM_HASH(hash) |
					SPDM_PGM_ENABLE_ENTRY;
	writeq(element.hash_n_entry_control_1, element_addr);
	msleep(20);

	return 0;
}

/**
 * spdm_configure -
 */
static int spdm_configure(struct s2io_nic *nic, struct spdm_cfg *info)
{
	struct XENA_dev_config *bar0 = (struct XENA_dev_config *) nic->bar0;
	struct net_device *dev = nic->dev;
	u64 val64;
	int ret;

	val64 = readq(&bar0->spdm_bir_offset);
	if (!(val64 & (vBIT(3, 0, 2)))) {
		s2io_program_spdm_table(info, nic->spdm_entry, nic);
		nic->spdm_entry++;
		if (nic->spdm_entry == MAX_SUPPORTED_SPDM_ENTRIES)
			nic->spdm_entry = 0;
		ret = SUCCESS;
	} else {
		DBG_PRINT(ERR_DBG, "SPDM table of %s is not in BAR0!!\n",
			  dev->name);
		info->ret = SPDM_TABLE_UNKNOWN_BAR;
		ret = FAILURE;
	}

	return ret;
}

static int spdm_data_processor(struct spdm_cfg *usr_info, struct s2io_nic *sp)
{
	int ret;
	struct swDbgStat *stats = sp->sw_dbg_stat;

	if (sp->device_type == XFRAME_I_DEVICE) {
		usr_info->ret = SPDM_XENA_IF;
		return FAILURE;
	}

	if (!netif_running(sp->dev)) {
		usr_info->ret = SPDM_HW_UNINITIALIZED;
		return FAILURE;
	}
	if (usr_info->ret == SPDM_GET_CFG_DATA) {/* Retrieve info */
		u8 *data = kmalloc(MAX_SPDM_ENTRIES_SIZE, GFP_KERNEL);
		if (!data) {
			usr_info->ret = SPDM_TABLE_MALLOC_FAIL;
			stats->mem_alloc_fail_cnt++;
			return FAILURE;
		}

		ret = spdm_extract_table(data, sp);
		if (ret != FAILURE) {
			memcpy(usr_info->data, data,
				(sp->spdm_entry * 0x40));
			usr_info->data_len = ret;
			usr_info->ret = SPDM_CONF_SUCCESS;
			ret = SUCCESS;
		} else {
			usr_info->ret = SPDM_TABLE_ACCESS_FAILED;
			ret = FAILURE;
		}

		kfree(data);
		return ret;
	} else if (usr_info->ret == SPDM_GET_CLR_DATA) {/* Clear info */
		ret = spdm_clear_table(sp);
		if (ret != FAILURE) {
			usr_info->ret = SPDM_CLR_SUCCESS;
			ret = SUCCESS;
		} else {
			usr_info->ret = SPDM_CLR_FAIL;
			ret = FAILURE;
		}
		return ret;
	}

	if (!usr_info->dip || !usr_info->sip || !usr_info->sprt ||
		!usr_info->dprt) {
		usr_info->ret = SPDM_INCOMPLETE_SOCKET;
		return FAILURE;
	}

	ret = spdm_configure(sp, usr_info);
	if (ret == SUCCESS)
		usr_info->ret = SPDM_CONF_SUCCESS;

	return SUCCESS;
}

/* Rx Steering parameter list explanation
 * rx_steering_type: There are 2 ways to steer Rx'ed frames to correct rings
 *	on the host either RTH or L4 Port based steering. This parameter can
 *	be used to choose any one scheme. To use RTH, set this parameter to 2
 *	or for L4 Port steering set it to 1.
 *
 * rth_ports: The specefic ports whicch will be routed to programmed rings. A
 *	maximum of 'MAX_STEERABLE_PORTS' ports can be configured.
 *
 * port_type: This parameter can be used to specify if the ports being
 *	scanned by the H/W is a Destination port or source port. Default is
 *	destination port.
 *
 * rth_protocol: This parameter can be used to define
 *		1 is IPV4 Src and dst address and TCP src abd Dst ports
 *		2 is IPV4 Src and dst address and UDP src abd Dst ports
 *		3 is IPV4 Src and dst address
 *		4 is IPV6 Src and dst address and TCP src abd Dst ports
 *		5 is IPV6 Src and dst address and UDP src abd Dst ports
 *		6 is IPV6 Src and dst address
 *		7 is extended IPV6 Src and dst address and TCP src abd Dst
 *		ports
 *		8 is extended IPV6 Src and dst address and UDP src abd Dst
 *		ports
 *		9 is extended IPV6 Src and dst address
 * rth_mask: This parameter can be used to mask
 *		any of the six parameters used for RTH calculation mentioned
 *		above. The bits 0 to 5 represent IPV4 SA,
 *		IPV4 DA, IPV6 SA, IPV6 DA, L4 SP, L4 DP in that order.
 */

S2IO_PARM_INT(port_type, DP);
static int rth_ports[MAX_STEERABLE_PORTS] =
			{[0 ...(MAX_STEERABLE_PORTS - 1)] = 0 };
#ifdef module_param_array
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10))
module_param_array(rth_ports, uint, NULL, 0);
#else
int num_rth_ports = MAX_STEERABLE_PORTS;
module_param_array(rth_ports, int, num_rth_ports, 0);
#endif
#else
MODULE_PARM(rth_ports, "1-" __MODULE_STRING(MAX_STEERABLE_PORTS) "i");
#endif

/**
 * s2io_rth_configure -
 */
int s2io_rth_configure(struct s2io_nic *nic)
{
	struct XENA_dev_config *bar0 = (struct XENA_dev_config *) nic->bar0;
	register u64 val64 = 0;
	struct config_param *config;
	int buckets, i, ring = 0, cnt = 0;

	config = &nic->config;

	/* Enabled enhanced RTS steering */
	val64 = readq(&bar0->rts_ctrl);
	val64 |= RTS_CTRL_ENHANCED;
	writeq(val64, &bar0->rts_ctrl);

	if (config->rx_steering_type == PORT_STEERING) {
		for (i = 0, ring = 0; i < MAX_STEERABLE_PORTS; i++) {
			int port = rth_ports[i];
			if (!port)
				break;
			ring = i % config->rx_ring_num;
			val64 = S2BIT(7) | vBIT(port, 8, 16) |
				vBIT(ring, 37, 3) |  S2BIT(63);
			if (port_type == SP)
				val64 = S2BIT(47);
			writeq(val64, &bar0->rts_pn_cam_data);
			val64 = S2BIT(7) | S2BIT(15) | vBIT(i, 24, 8);
			writeq(val64, &bar0->rts_pn_cam_ctrl);
			mdelay(5);
		}
	} else {
		int rth_sz = 0;
		int start_ring = 0;

		if (config->rx_steering_type == RTH_STEERING_DEFAULT) {
			rth_sz = config->rx_ring_num - 1;
			start_ring = 1;
		} else {
			if (rx_ring_num == 0) {
				start_ring = 1;
				rth_sz = config->rx_ring_num - 1;
			} else {
				/*
				 * If the user overrides the driver
				 * configuration of number of rings then
				 * include ring zero in the steering criteria
				 */
				rth_sz = config->rx_ring_num;
				start_ring = 0;
			}
		}

		ring = start_ring;
		buckets = 1 << rth_sz;

		for (i = 0; i < buckets; i++) {
		    val64 = RTS_RTH_MAP_MEM_DATA_ENTRY_EN |
				RTS_RTH_MAP_MEM_DATA_(ring);
			writeq(val64, &bar0->rts_rth_map_mem_data);

			val64 = RTS_RTH_MAP_MEM_CTRL_WE |
				RTS_RTH_MAP_MEM_CTRL_STROBE |
				RTS_RTH_MAP_MEM_CTRL_OFFSET(i);
			writeq(val64, &bar0->rts_rth_map_mem_ctrl);

			do {
				val64 = readq(&bar0->rts_rth_map_mem_ctrl);
				if (val64 & RTS_RTH_MAP_MEM_CTRL_STROBE) {
					cnt++;
					msleep(10);
					continue;
				}
				break;
			} while (cnt < 5);
			if (cnt == 5)
				return FAILURE;

			if (++ring > rth_sz)
				ring = start_ring;
		}

		/*
		 * Mask all parameters as per user's input.
		 */
		for (i = 0; i < 6; i++) {
			if (!((rth_mask >> i) & 0x1))
				continue;
			switch (i) {
			case 0:
			val64 = readq(&bar0->rts_rth_hash_mask_n[4]);
			val64 |= RTS_RTH_HASH_MASK_IPV4_SA(0xFFFFFFFF);
			writeq(val64, &bar0->rts_rth_hash_mask_n[4]);
			break;
			case 1:
				val64 = readq(&bar0->rts_rth_hash_mask_n[4]);
				val64 |= RTS_RTH_HASH_MASK_IPV4_DA(0xFFFFFFFF);
				writeq(val64, &bar0->rts_rth_hash_mask_n[4]);
				break;
			case 2:
				val64 = 0xFFFFFFFFFFFFFFFFULL;
				writeq(val64, &bar0->rts_rth_hash_mask_n[0]);
				val64 = 0xFFFFFFFFFFFFFFFFULL;
				writeq(val64, &bar0->rts_rth_hash_mask_n[1]);
				break;
			case 3:
				val64 = 0xFFFFFFFFFFFFFFFFULL;
				writeq(val64, &bar0->rts_rth_hash_mask_n[2]);
				val64 = 0xFFFFFFFFFFFFFFFFULL;
				writeq(val64, &bar0->rts_rth_hash_mask_n[3]);
				break;
			case 4:
				val64 = readq(&bar0->rts_rth_hash_mask_5);
				val64 |= RTS_RTH_HASH_MASK_L4_SP(0xFFFF);
				writeq(val64, &bar0->rts_rth_hash_mask_5);
				break;
			case 5:
				val64 = readq(&bar0->rts_rth_hash_mask_5);
				val64 |= RTS_RTH_HASH_MASK_L4_DP(0xFFFF);
				writeq(val64, &bar0->rts_rth_hash_mask_5);
				break;
			}
		}

		/*
		 * Set the RTH function type as per user's input and enable RTH.
		 */
		switch (rth_protocol) {
		case 1:
			val64 = RTS_RTH_TCP_IPV4_EN;
			break;
		case 2:
			val64 = RTS_RTH_UDP_IPV4_EN;
			break;
		case 3:
			val64 = RTS_RTH_IPV4_EN;
			break;
		case 4:
			val64 = RTS_RTH_TCP_IPV6_EN;
			break;
		case 5:
			val64 = RTS_RTH_UDP_IPV6_EN;
			break;
		case 6:
			val64 = RTS_RTH_IPV6_EN;
			break;
		case 7:
			val64 = RTS_RTH_TCP_IPV6_EX_EN;
			break;
		case 8:
			val64 = RTS_RTH_UDP_IPV6_EX_EN;
			break;
		case 9:
			val64 = RTS_RTH_IPV6_EX_EN;
			break;
		default:
			val64 = RTS_RTH_TCP_IPV4_EN;
			break;
		}
		val64 |= RTS_RTH_EN | RTS_RTH_BUCKET_SIZE(rth_sz);
		writeq(val64, &bar0->rts_rth_cfg);
	}

	return 0;
}

#ifndef SET_ETHTOOL_OPS
/**
 * s2io_ethtool -to support all ethtool features .
 * @dev : device pointer.
 * @ifr :   An IOCTL specefic structure, that can contain a pointer to
 * a proprietary structure used to pass information to the driver.
 * Description:
 * Function used to support all ethtool fatures except dumping Device stats
 * as it can be obtained from the util tool for now.
 * Return value:
 * 0 on success and an appropriate (-)ve integer as defined in errno.h
 * file on failure.
 */

static int s2io_ethtool(struct net_device *dev, struct ifreq *rq)
{
	struct s2io_nic *sp = dev->priv;
	void *data = rq->ifr_data;
	u32 ecmd;
	u64 val64 = 0;
	int stat_size = 0;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	struct swDbgStat *stats = sp->sw_dbg_stat;

	if (get_user(ecmd, (u32 *) data))
		return -EFAULT;

	switch (ecmd) {
	case ETHTOOL_GSET:
	{
		struct ethtool_cmd info = { ETHTOOL_GSET };
		s2io_ethtool_gset(dev, &info);
		if (copy_to_user(data, &info, sizeof(info)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_SSET:
	{
		struct ethtool_cmd info;

		if (copy_from_user(&info, data, sizeof(info)))
			return -EFAULT;
		if (s2io_ethtool_sset(dev, &info))
			return -EFAULT;
		break;
	}
	case ETHTOOL_GDRVINFO:
	{
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };

		s2io_ethtool_gdrvinfo(dev, &info);
		if (copy_to_user(data, &info, sizeof(info)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_GREGS:
	{
		struct ethtool_regs regs = { ETHTOOL_GREGS };
		u8 *reg_space;
		int ret = 0;

		regs.version = sp->pdev->subsystem_device;

		reg_space = kmalloc(XENA_REG_SPACE, GFP_KERNEL);
		if (reg_space == NULL) {
			DBG_PRINT(ERR_DBG,
				  "Memory allocation to dump ");
			DBG_PRINT(ERR_DBG, "registers failed\n");
			stats->mem_alloc_fail_cnt++;
			ret = -EFAULT;
		}
		stats->mem_allocated += XENA_REG_SPACE;
		memset(reg_space, 0, XENA_REG_SPACE);
		s2io_ethtool_gregs(dev, &regs, reg_space);
		if (copy_to_user(data, &regs, sizeof(regs))) {
			ret = -EFAULT;
			goto last_gregs;
		}
		data += offsetof(struct ethtool_regs, data);
		if (copy_to_user(data, reg_space, regs.len)) {
			ret = -EFAULT;
			goto last_gregs;
		}
last_gregs:
		kfree(reg_space);
		stats->mem_freed += XENA_REG_SPACE;
		if (ret)
			return ret;
		break;
	}
	case ETHTOOL_GLINK:
	{
		struct ethtool_value link = { ETHTOOL_GLINK };
		link.data = netif_carrier_ok(dev);
		if (link.data) {
			val64 = readq(&bar0->adapter_control);
			link.data = (val64 & ADAPTER_CNTL_EN);
		}
		if (copy_to_user(data, &link, sizeof(link)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_PHYS_ID:
	{
		struct ethtool_value id;

		if (copy_from_user(&id, data, sizeof(id)))
			return -EFAULT;
		s2io_ethtool_idnic(dev, id.data);
		break;
	}
	case ETHTOOL_GRINGPARAM:
	{
		struct ethtool_ringparam ep =  { ETHTOOL_GRINGPARAM };
		s2io_ethtool_gringparam(dev, &ep);
		if (copy_to_user(data, &ep, sizeof(ep)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_GPAUSEPARAM:
	{
		struct ethtool_pauseparam ep =  { ETHTOOL_GPAUSEPARAM };
		s2io_ethtool_getpause_data(dev, &ep);
		if (copy_to_user(data, &ep, sizeof(ep)))
			return -EFAULT;
		break;

	}
	case ETHTOOL_SPAUSEPARAM:
	{
		struct ethtool_pauseparam ep;

		if (copy_from_user(&ep, data, sizeof(ep)))
			return -EFAULT;
		s2io_ethtool_setpause_data(dev, &ep);
		break;
	}
	case ETHTOOL_GRXCSUM:
	{
		struct ethtool_value ev = { ETHTOOL_GRXCSUM };

		ev.data = sp->rx_csum;
		if (copy_to_user(data, &ev, sizeof(ev)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_GTXCSUM:
	{
		struct ethtool_value ev = { ETHTOOL_GTXCSUM };
		if (sp->device_type == XFRAME_II_DEVICE)
			ev.data = (dev->features & NETIF_F_HW_CSUM);
		else
			ev.data = (dev->features & NETIF_F_IP_CSUM);

		if (copy_to_user(data, &ev, sizeof(ev)))
			return -EFAULT;
		break;
	}
	case ETHTOOL_GSG:
	{
		struct ethtool_value ev = { ETHTOOL_GSG };
		ev.data = (dev->features & NETIF_F_SG);

		if (copy_to_user(data, &ev, sizeof(ev)))
			return -EFAULT;
		break;
	}
#ifdef NETIF_F_TSO
	case ETHTOOL_GTSO:
	{
		struct ethtool_value ev = { ETHTOOL_GTSO };
		ev.data = (dev->features & NETIF_F_TSO);

		if (copy_to_user(data, &ev, sizeof(ev)))
			return -EFAULT;
		break;
	}
#endif
	case ETHTOOL_STXCSUM:
	{
		struct ethtool_value ev;

		if (copy_from_user(&ev, data, sizeof(ev)))
			return -EFAULT;

		if (ev.data) {
			if (sp->device_type == XFRAME_II_DEVICE)
				dev->features |= NETIF_F_HW_CSUM;
			else
				dev->features |= NETIF_F_IP_CSUM;
		} else {
			dev->features &= ~NETIF_F_IP_CSUM;
			if (sp->device_type == XFRAME_II_DEVICE)
				dev->features &= ~NETIF_F_HW_CSUM;
		}
		break;
	}
	case ETHTOOL_SRXCSUM:
	{
		struct ethtool_value ev;

		if (copy_from_user(&ev, data, sizeof(ev)))
			return -EFAULT;

		if (ev.data)
			sp->rx_csum = 1;
		else
			sp->rx_csum = 0;

		break;
	}
	case ETHTOOL_SSG:
	{
		struct ethtool_value ev;

		if (copy_from_user(&ev, data, sizeof(ev)))
			return -EFAULT;

		if (ev.data)
			dev->features |= NETIF_F_SG;
		else
			dev->features &= ~NETIF_F_SG;
		break;
	}
#ifdef NETIF_F_TSO
	case ETHTOOL_STSO:
	{
		struct ethtool_value ev;

		if (copy_from_user(&ev, data, sizeof(ev)))
			return -EFAULT;

		if (ev.data)
			dev->features |= NETIF_F_TSO;
		else
			dev->features &= ~NETIF_F_TSO;
		break;
	}
#endif
	case ETHTOOL_GEEPROM:
	{
		struct ethtool_eeprom eeprom = { ETHTOOL_GEEPROM };
		char *data_buf;
		int ret = 0;

		if (copy_from_user(&eeprom, data, sizeof(eeprom)))
			return -EFAULT;

		if (eeprom.len <= 0)
			return -EINVAL;
		data_buf = kmalloc(XENA_EEPROM_SPACE, GFP_KERNEL);
		if (!data_buf) {
			stats->mem_alloc_fail_cnt++;
			return -ENOMEM;
		}
		stats->mem_allocated +=
				XENA_EEPROM_SPACE;
		s2io_ethtool_geeprom(dev, &eeprom, data_buf);

		if (copy_to_user(data, &eeprom, sizeof(eeprom))) {
			ret = -EFAULT;
			goto last_geprom;
		}

		data += offsetof(struct ethtool_eeprom, data);

		if (copy_to_user(data, (void *)data_buf, eeprom.len)) {
			ret = -EFAULT;
			goto last_geprom;
		}

last_geprom:
		kfree(data_buf);
		stats->mem_freed += XENA_EEPROM_SPACE;
		if (ret)
			return ret;
		break;
	}
	case ETHTOOL_SEEPROM:
	{
		struct ethtool_eeprom eeprom;
		unsigned char *data_buf;
		void *ptr;
		int ret = 0;

		if (copy_from_user(&eeprom, data, sizeof(eeprom)))
			return -EFAULT;

		data_buf = kmalloc(eeprom.len, GFP_KERNEL);
		if (!data_buf) {
			stats->mem_alloc_fail_cnt++;
			return -ENOMEM;
		}

		stats->mem_allocated += eeprom.len;
		ptr = (void *) data_buf;

		data += offsetof(struct ethtool_eeprom, data);
		if (copy_from_user(ptr, data, eeprom.len)) {
			ret = -EFAULT;
			goto last_seprom;
		}

		if ((eeprom.offset + eeprom.len) > (XENA_EEPROM_SPACE)) {
			DBG_PRINT(ERR_DBG, "%s Write ", dev->name);
			DBG_PRINT(ERR_DBG, "request overshoots ");
			DBG_PRINT(ERR_DBG, "the EEPROM area\n");
			ret = -EFAULT;
			goto last_seprom;
		}
		if (s2io_ethtool_seeprom(dev, &eeprom, data_buf)) {
			ret = -EFAULT;
			goto last_seprom;
		}

last_seprom:
		kfree(data_buf);
		stats->mem_freed += eeprom.len;
		if (ret)
			return ret;
		break;
	}
	case ETHTOOL_GSTRINGS:
	{
		struct ethtool_gstrings gstrings = { ETHTOOL_GSTRINGS };
		char *strings = NULL;
		int ret = 0, mem_sz;

		if (copy_from_user(&gstrings, data, sizeof(gstrings)))
			return -EFAULT;

		switch (gstrings.string_set) {
		case ETH_SS_TEST:
			gstrings.len = S2IO_TEST_LEN;
			mem_sz = S2IO_STRINGS_LEN;
			strings = kmalloc(mem_sz, GFP_KERNEL);
			if (!strings) {
				stats->mem_alloc_fail_cnt++;
				return -ENOMEM;
			}

			memcpy(strings, s2io_gstrings, S2IO_STRINGS_LEN);
			break;

#ifdef ETHTOOL_GSTATS
		case ETH_SS_STATS:
#ifdef TITAN_LEGACY
			if (sp->device_type == TITAN_DEVICE) {
				gstrings.len = S2IO_TITAN_STAT_LEN;
				mem_sz = S2IO_TITAN_STAT_STRINGS_LEN;
				strings = kmalloc(mem_sz, GFP_KERNEL);
				if (!strings) {
					stats->mem_alloc_fail_cnt++;
					return -ENOMEM;
				}
				memcpy(strings,
					&ethtool_titan_stats_keys,
					sizeof(ethtool_titan_stats_keys));
			} else
#endif
			{
				if (sp->device_type == XFRAME_I_DEVICE) {
					gstrings.len = XFRAME_I_STAT_LEN;
					mem_sz = XFRAME_I_STAT_STRINGS_LEN;
				} else {
					gstrings.len = XFRAME_II_STAT_LEN;
					mem_sz = XFRAME_II_STAT_STRINGS_LEN;
				}
				gstrings.len +=	(sp->config.tx_fifo_num
					* NUM_TX_SW_STAT * ETH_GSTRING_LEN) +
					(sp->config.rx_ring_num
					* NUM_RX_SW_STAT * ETH_GSTRING_LEN);
				mem_sz += (sp->config.tx_fifo_num
					* NUM_TX_SW_STAT * ETH_GSTRING_LEN) +
					(sp->config.rx_ring_num
					* NUM_RX_SW_STAT * ETH_GSTRING_LEN);

				if (!sp->exec_mode) {
					gstrings.len +=
						S2IO_DRIVER_DBG_STAT_LEN;
					mem_sz += S2IO_DRIVER_DBG_STAT_LEN *
							ETH_GSTRING_LEN;
				}

				strings = kmalloc(mem_sz, GFP_KERNEL);
				if (!strings) {
					stats->mem_alloc_fail_cnt++;
					return -ENOMEM;
				}

				stat_size = sizeof(ethtool_xena_stats_keys);
				memcpy(strings,
					&ethtool_xena_stats_keys, stat_size);

				if (sp->device_type == XFRAME_II_DEVICE) {
					memcpy(strings + stat_size,
					&ethtool_enhanced_stats_keys,
					sizeof(ethtool_enhanced_stats_keys));

					stat_size +=
					   sizeof(ethtool_enhanced_stats_keys);
				}

				memcpy(strings + stat_size,
					&ethtool_driver_stats_keys,
					sizeof(ethtool_driver_stats_keys));

				if (!sp->exec_mode) {
					stat_size += sizeof(ethtool_driver_stats_keys);
					memcpy(strings + stat_size,
						&ethtool_driver_dbg_stats_keys,
						sizeof(ethtool_driver_dbg_stats_keys));
				}

				}
			break;
#endif
		default:
			return -EOPNOTSUPP;
		}

		if (copy_to_user(data, &gstrings, sizeof(gstrings)))
			ret = -EFAULT;
		if (!ret) {
			data +=
			offsetof(struct ethtool_gstrings,
				     data);
			if (copy_to_user(data, strings, mem_sz))
				ret = -EFAULT;
		}
		kfree(strings);
		if (ret)
			return ret;
		break;
	}
	case ETHTOOL_TEST:
	{
		struct {
			struct ethtool_test ethtest;
			uint64_t data[S2IO_TEST_LEN];
		} test = { {ETHTOOL_TEST} };

		if (copy_from_user(&test.ethtest, data, sizeof(test.ethtest)))
			return -EFAULT;

		s2io_ethtool_test(dev, &test.ethtest, test.data);
		if (copy_to_user(data, &test, sizeof(test)))
			return -EFAULT;

		break;
	}
#ifdef ETHTOOL_GSTATS
	case ETHTOOL_GSTATS:
	{
		struct ethtool_stats eth_stats;
		int ret;
		u64 *stat_mem;

		if (copy_from_user(&eth_stats, data, sizeof(eth_stats)))
			return -EFAULT;
#ifdef TITAN_LEGACY
		if (sp->device_type == TITAN_DEVICE)
			eth_stats.n_stats = S2IO_TITAN_STAT_LEN;
		else
#endif
		{
			if (sp->device_type == XFRAME_I_DEVICE)
				eth_stats.n_stats += XFRAME_I_STAT_LEN +
				(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
				(sp->config.rx_ring_num * NUM_RX_SW_STAT);
			else
				eth_stats.n_stats += XFRAME_II_STAT_LEN +
				(sp->config.tx_fifo_num * NUM_TX_SW_STAT) +
				(sp->config.rx_ring_num * NUM_RX_SW_STAT);

			if (!sp->exec_mode)
				eth_stats.n_stats += S2IO_DRIVER_DBG_STAT_LEN;
		}

		stat_mem =
		kmalloc(eth_stats.n_stats * sizeof(u64), GFP_USER);
		if (!stat_mem) {
			stats->mem_alloc_fail_cnt++;
			return -ENOMEM;
		}
		stats->mem_allocated +=
			(eth_stats.n_stats * sizeof(u64));
		s2io_get_ethtool_stats(dev, &eth_stats, stat_mem);
		ret = 0;
		if (copy_to_user(data, &eth_stats, sizeof(eth_stats)))
			ret = -EFAULT;
		data += sizeof(eth_stats);
		if (copy_to_user(data, stat_mem,
				eth_stats.n_stats * sizeof(u64)))
			ret = -EFAULT;
		kfree(stat_mem);
		stats->mem_freed += (eth_stats.n_stats * sizeof(u64));
		return ret;
	}
#endif

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}
#endif

#ifdef SNMP_SUPPORT
int s2io_snmp_init(struct s2io_nic *nic)
{
	struct net_device *dev = nic->dev;
	struct timeval tm;
	if (!s2io_bdsnmp_init(dev))
		DBG_PRINT(INIT_DBG, "Error Creating Proc directory for SNMP\n");

	do_gettimeofday(&tm);
	nic->lDate = tm.tv_sec;
	return 0;
}

int s2io_snmp_exit(struct s2io_nic *nic)
{
	struct net_device *dev = nic->dev;
	s2io_bdsnmp_rem(dev);
	return 0;
}

/**
 * fnBaseDrv - Get the driver information
 * @pBaseDrv -Pointer to Base driver structure which contains the offset
 * and length of each of the field.
 * Description
 * This function copies the driver specific information from the dev structure
 * to the pBaseDrv stucture. It calculates the number of physical adapters by
 * parsing the dev_base global variable maintained by the kernel. This
 * variable has to read locked before accesing.This function is called by
 * fnBaseReadProc function.
 *
 */

static void fnBaseDrv(struct stBaseDrv *pBaseDrv, struct net_device *dev)
{

	struct pci_dev *pdev = NULL;
	struct net_device *ndev;
	struct s2io_nic *sp = dev->priv;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	int nCount = 0;
	int i;
	struct swDbgStat *stats = sp->sw_dbg_stat;

	strncpy(pBaseDrv->m_cName, sp->cName, 20);
	strncpy(pBaseDrv->m_cVersion, sp->cVersion, 20);
	pBaseDrv->m_nStatus = sp->last_link_state;
	pBaseDrv->m_nMemorySize = stats->mem_allocated;
	sprintf(pBaseDrv->m_cDate, "%ld", sp->lDate);
	pBaseDrv->m_dev_id = sp->pdev->device;
	pBaseDrv->m_ven_id = PCI_VENDOR_ID_S2IO;
	pBaseDrv->m_tx_intr_cnt = sp->tx_intr_cnt;
	pBaseDrv->m_rx_intr_cnt = sp->rx_intr_cnt;
	pBaseDrv->m_intr_type = sp->config.intr_type;
	pBaseDrv->m_tx_fifo_num = sp->config.tx_fifo_num;
	pBaseDrv->m_rx_ring_num = sp->config.rx_ring_num;
	pBaseDrv->m_rxd_mode = sp->rxd_mode;
	pBaseDrv->m_lro = sp->lro;
	pBaseDrv->m_lro_max_pkts = sp->lro_max_aggr_per_sess;
	pBaseDrv->m_napi = sp->config.napi;
	pBaseDrv->m_rx_steering_type = sp->config.rx_steering_type;
	pBaseDrv->m_vlan_tag_strip = sp->config.vlan_tag_strip;
	pBaseDrv->m_rx_csum = sp->rx_csum;
	if (sp->device_type == XFRAME_II_DEVICE)
		pBaseDrv->m_tx_csum = ((dev->features & NETIF_F_HW_CSUM)? 1: 0);
	else
		pBaseDrv->m_tx_csum = ((dev->features & NETIF_F_IP_CSUM)? 1: 0);
	pBaseDrv->m_sg = ((dev->features & NETIF_F_SG)?1:0);
#ifndef NETIF_F_UFO
	pBaseDrv->m_ufo = 0;
#else
	pBaseDrv->m_ufo = 0;
	if ((sp->device_type == XFRAME_II_DEVICE) && ufo)
		pBaseDrv->m_ufo = 1;
#endif
#ifndef NETIF_F_TSO
	pBaseDrv->m_tso = 0;
#else
	pBaseDrv->m_tso = 1;
#endif
	pBaseDrv->m_nFeature = ((pBaseDrv->m_tx_csum && sp->rx_csum &&
				pBaseDrv->m_tso)?3:((pBaseDrv->m_tso)?1:0));
	pBaseDrv->m_fifo_len = 0;
	for (i = 0; i < sp->config.tx_fifo_num; i++)
		pBaseDrv->m_fifo_len += sp->config.tx_cfg[i].fifo_len;
	pBaseDrv->m_rx_ring_size = 0;
	for (i = 0; i < sp->config.rx_ring_num; i++)
		pBaseDrv->m_rx_ring_size += rx_ring_sz[i];
	pBaseDrv->m_rth_bucket_size = ((sp->config.rx_ring_num + 1)/2);
	pBaseDrv->m_tx_urng = readq(&bar0->tti_data1_mem);
	pBaseDrv->m_rx_urng = readq(&bar0->rti_data1_mem);
	pBaseDrv->m_tx_ufc  = readq(&bar0->tti_data2_mem);
	pBaseDrv->m_rx_ufc  = readq(&bar0->rti_data2_mem);
	/*
	 * Find all the ethernet devices on the system using
	 * pci_find_device. Get the private data which will be the
	 * net_device structure assigned by the driver.
	 */
	while ((pdev =
		S2IO_PCI_FIND_DEVICE(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
		if (pdev->vendor == PCI_VENDOR_ID_S2IO) {
			ndev = (struct net_device *) pci_get_drvdata(pdev);
			if (ndev == NULL)
				continue;
			memcpy(pBaseDrv->m_stPhyAdap[nCount].m_cName,
				ndev->name, 20);
			pBaseDrv->m_stPhyAdap[nCount].m_nIndex = ndev->ifindex;
			nCount++;
		}
	}
	pBaseDrv->m_nPhyCnt = nCount;
}

/*
*  fnBaseReadProc - Read entry point for the proc file
*  @page - Buffer pointer where the data is written
*  @start- Pointer to buffer ptr . It is used if the data is more than a page
*  @off- the offset to the page where data is written
*  @count - number of bytes to write
*  @eof - to indicate end of file
*  @data - pointer to device structure.
*
* Description -
* This function gets  Base driver specific information from the fnBaseDrv
* function and writes into the BDInfo file. This function is called whenever
* the user reads the file. The length of data written cannot exceed 4kb.
* If it exceeds then use the start pointer to write multiple pages
* Return - the length of the string written to proc file
*/
static int fnBaseReadProc(char *page, char **start, off_t off, int count,
			  int *eof, void *data)
{
	struct stBaseDrv *pBaseDrv;
	int nLength = 0;
	int nCount = 0;
	struct net_device *dev = (struct net_device *) data;
	int nIndex = 0;

	pBaseDrv = kmalloc(sizeof(struct stBaseDrv), GFP_KERNEL);
	if (pBaseDrv == NULL) {
		DBG_PRINT(ERR_DBG, "Error allocating memory\n");
		return -ENOMEM;
	}
	fnBaseDrv(pBaseDrv, dev);
	sprintf(page + nLength, "%-30s%-20s\n", "Base Driver Name",
		pBaseDrv->m_cName);
	nLength += 51;
	if (pBaseDrv->m_nStatus == 2)
		sprintf(page + nLength, "%-30s%-20s\n", "Load Status",
			"Loaded");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Load Status",
			"UnLoaded");
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20s\n", "Base Driver Version",
		pBaseDrv->m_cVersion);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Feature Supported",
		pBaseDrv->m_nFeature);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20lld\n",
		"Base Driver Memrory in Bytes", pBaseDrv->m_nMemorySize);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20s\n", "Base Driver Date",
		pBaseDrv->m_cDate);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "No of Phy Adapter",
		pBaseDrv->m_nPhyCnt);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20x\n", "Device id",
		pBaseDrv->m_dev_id);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20x\n", "Vendor id",
		pBaseDrv->m_ven_id);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Tx Interrupt Count",
		pBaseDrv->m_tx_intr_cnt);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Rx Interrupt Count",
		pBaseDrv->m_rx_intr_cnt);
	nLength += 51;
	if (pBaseDrv->m_intr_type == 0)
		sprintf(page + nLength, "%-30s%-20s\n", "Interrupt Type",
			"INTA");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Interrupt Type",
			"MSI-X");
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "FIFOs Configured",
		pBaseDrv->m_tx_fifo_num);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Rings Configured",
		pBaseDrv->m_rx_ring_num);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Fifo Length",
		pBaseDrv->m_fifo_len);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Rx Ring Size",
		pBaseDrv->m_rx_ring_size);
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "Rth Bucket Size",
		pBaseDrv->m_rth_bucket_size);
	nLength += 51;
	if (pBaseDrv->m_rxd_mode == 0)
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Buffer Mode",
			"1-buffer mode");
	else if (pBaseDrv->m_rxd_mode == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Buffer Mode",
			"2-buffer mode");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Buffer Mode",
			"5-buffer mode");
	nLength += 51;
	if (pBaseDrv->m_lro == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "LRO",
			"LRO ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "LRO",
			"LRO DISABLED");
	nLength += 51;
	sprintf(page + nLength, "%-30s%-20d\n", "LRO Max Packets",
		pBaseDrv->m_lro_max_pkts);
	nLength += 51;
	if (pBaseDrv->m_napi == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "NAPI",
			"NAPI ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "NAPI",
			"NAPI DISABLED");
	nLength += 51;
	if (pBaseDrv->m_ufo == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "UFO",
			"UFO ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "UFO",
			"UFO DISABLED");
	nLength += 51;
	if (pBaseDrv->m_tso == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "TSO",
			"TSO ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "TSO",
			"TSO DISABLED");
	nLength += 51;
	if (pBaseDrv->m_rx_steering_type == 0)
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Steering Type",
			"NO STEERING");
	else if (pBaseDrv->m_rx_steering_type == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Steering Type",
			"PORT STEERING");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Steering Type",
			"RTH ENABLED");
	nLength += 51;
	if (pBaseDrv->m_vlan_tag_strip == 0)
		sprintf(page + nLength, "%-30s%-20s\n", "VLAN Tag Stripping",
			"DISABLED");
	else if (pBaseDrv->m_vlan_tag_strip == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "VLAN Tag Stripping",
			"ENABLED");
	else
		sprintf(page + nLength, "%-30s%-23s\n", "VLAN Tag Stripping",
			"DEFAULT MODE");
	nLength += 55;
	if (pBaseDrv->m_rx_csum == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Checksum",
			"RX CSUM ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Rx Checksum",
			"RX CSUM DISABLED");
	nLength += 51;
	if (pBaseDrv->m_tx_csum == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "Tx Checksum",
			"TX CSUM ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Tx Checksum",
			"TX CSUM DISABLED");
	nLength += 51;
	if (pBaseDrv->m_sg == 1)
		sprintf(page + nLength, "%-30s%-20s\n", "Scatter Gather",
			"SG ENABLED");
	else
		sprintf(page + nLength, "%-30s%-20s\n", "Scatter Gather",
			"SG DISABLED");
	nLength += 51;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_URNG_A",
		((unsigned long long)(0x7F0000 & pBaseDrv->m_tx_urng) >> 16));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_URNG_B",
		((unsigned long long)(0x7F00 & pBaseDrv->m_tx_urng) >> 8));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_URNG_C",
		(unsigned long long)(0x7F & pBaseDrv->m_tx_urng));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_URNG_A",
		((unsigned long long)(0x7F0000 & pBaseDrv->m_rx_urng) >> 16));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_URNG_B",
		((unsigned long long)(0x7F00 & pBaseDrv->m_rx_urng) >> 8));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_URNG_C",
		(unsigned long long)(0x7F & pBaseDrv->m_rx_urng));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_UFC_A",
		((unsigned long long)
			(0xFFFF000000000000ULL & pBaseDrv->m_tx_ufc) >> 48));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_UFC_B",
		((unsigned long long)
			(0xFFFF00000000ULL & pBaseDrv->m_tx_ufc) >> 32));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_UFC_C",
		((unsigned long long)(0xFFFF0000 & pBaseDrv->m_tx_ufc) >> 16));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "TX_UFC_D",
		(unsigned long long)(0xFFFF & pBaseDrv->m_tx_ufc));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_UFC_A",
		((unsigned long long)
			(0xFFFF000000000000ULL & pBaseDrv->m_rx_ufc) >> 48));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_UFC_B",
		((unsigned long long)
			(0xFFFF00000000ULL & pBaseDrv->m_rx_ufc) >> 32));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_UFC_C",
		((unsigned long long)(0xFFFF0000 & pBaseDrv->m_rx_ufc) >> 16));
	nLength += 53;
	sprintf(page + nLength, "%-30s0x%-20llx\n", "RX_UFC_D",
		(unsigned long long)(0xFFFF & pBaseDrv->m_rx_ufc));
	nLength += 53;
	sprintf(page + nLength, "%-30s%-20s\n\n", "Phy Adapter Index",
		"Phy Adapter Name");
	nLength += 42;
	for (nIndex = 0, nCount = pBaseDrv->m_nPhyCnt; nCount != 0;
		nCount--, nIndex++) {
		sprintf(page + nLength, "%-20d%-20s\n",
			pBaseDrv->m_stPhyAdap[nIndex].m_nIndex,
			pBaseDrv->m_stPhyAdap[nIndex].m_cName);
		nLength += 41;
	}

	*eof = 1;
	kfree(pBaseDrv);
	return nLength;
}

/*
 *  fnPhyAdapReadProc - Read entry point for the proc file
 *  @page - Buffer pointer where the data is written
 *  @start- Pointer to buffer ptr . It is used if the data is more than a page
 *  @off- the offset to the page where data is written
 *  @count - number of bytes to write
 *  @eof - to indicate end of file
 *  @data - pointer to device structure.
 *
 * Description -
 * This function gets  physical adapter information. This function is called
 * whenever the user reads the file. The length of data written cannot
 * exceed 4kb. If it exceeds then use the start pointer to write multiple page
 *
 * Return - the length of the string written to proc file
 */
static int fnPhyAdapReadProc(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{

	struct stPhyData *pPhyData;
	struct net_device *pNetDev = (struct net_device *) data;
	struct s2io_nic *sp = pNetDev->priv;
	struct net_device_stats *pNetStat;
	struct pci_dev *pdev = NULL;
	int nLength = 0;
	u64 pmaddr;
	unsigned char cMAC[20];
	unsigned char pMAC[20];
	pPhyData = kmalloc(sizeof(struct stPhyData), GFP_KERNEL);
	if (pPhyData == NULL) {
		DBG_PRINT(ERR_DBG, "Error allocating memory\n");
		return -ENOMEM;
	}

	/* Print the header in the PhyAdap proc file */
	sprintf(page + nLength,
		"%-10s%-22s%-10s%-10s%-22s%-22s%-22s%-10s%-10s%-10s%-10s%-10s"
		"%-10s%-10s%-10s%-10s%-10s%-10s%-10s%-10s%-10s\n",
		"Index", "Description", "Mode", "Type", "Speed", "PMAC", "CMAC",
		"Status", "Slot", "Bus", "IRQ", "Colis", "Multi",
		"RxBytes", "RxDrop", "RxError", "RxPacket", "TRxBytes",
		"TRxDrop", "TxError", "TxPacket");

	/* 259 is the lenght of the above string copied in the page  */
	nLength += 259;

	while ((pdev =
		S2IO_PCI_FIND_DEVICE(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
		if (pdev->vendor == PCI_VENDOR_ID_S2IO) {
			/* Private data will point to the netdevice structure */
			pNetDev = (struct net_device *) pci_get_drvdata(pdev);
			if (pNetDev == NULL)
				continue;
			if (pNetDev->addr_len != 0) {
				pNetStat = pNetDev->get_stats(pNetDev);
				pPhyData->m_nIndex = pNetDev->ifindex;
				memcpy(pPhyData->m_cDesc, pNetDev->name, 20);
				pPhyData->m_nMode = 0;
				pPhyData->m_nType = 0;
				switch (pPhyData->m_nType) {
				/*
				case IFT_ETHER:
					memcpy(pPhyData->m_cSpeed,
						"10000000",20);
					break;

				case 9:
					memcpy(pPhyData->m_cSpeed,
						"4000000",20);
					break;
				 */
				default:
					memcpy(pPhyData->m_cSpeed,
						"10000000", 20);
					break;
				}
				pmaddr = do_s2io_read_unicast_mc(sp, 0);
				memcpy(pPhyData->m_cPMAC, &pmaddr,
					ETH_ALEN);
				memcpy(pPhyData->m_cCMAC, pNetDev->dev_addr,
					ETH_ALEN);
				pPhyData->m_nLinkStatus =
					test_bit(__LINK_STATE_START,
						&pNetDev->state);
				pPhyData->m_nPCISlot = PCI_SLOT(pdev->devfn);
				pPhyData->m_nPCIBus = pdev->bus->number;
				pPhyData->m_nIRQ = pNetDev->irq;
				pPhyData->m_nCollision = pNetStat->collisions;
				pPhyData->m_nMulticast = pNetStat->multicast;

				pPhyData->m_nRxBytes = pNetStat->rx_bytes;
				pPhyData->m_nRxDropped = pNetStat->rx_dropped;
				pPhyData->m_nRxErrors = pNetStat->rx_errors;
				pPhyData->m_nRxPackets = pNetStat->rx_packets;

				pPhyData->m_nTxBytes = pNetStat->tx_bytes;
				pPhyData->m_nTxDropped = pNetStat->tx_dropped;
				pPhyData->m_nTxErrors = pNetStat->tx_errors;
				pPhyData->m_nTxPackets = pNetStat->tx_packets;

				sprintf(cMAC, "%02x:%02x:%02x:%02x:%02x:%02x",
					pPhyData->m_cCMAC[0],
					pPhyData->m_cCMAC[1],
					pPhyData->m_cCMAC[2],
					pPhyData->m_cCMAC[3],
					pPhyData->m_cCMAC[4],
					pPhyData->m_cCMAC[5]);

				sprintf(pMAC, "%02x:%02x:%02x:%02x:%02x:%02x",
					pPhyData->m_cPMAC[5],
					pPhyData->m_cPMAC[4],
					pPhyData->m_cPMAC[3],
					pPhyData->m_cPMAC[2],
					pPhyData->m_cPMAC[1],
					pPhyData->m_cPMAC[0]);

				sprintf(page + nLength,
					"%-10d%-22s%-10d%-10d%-22s%-22s%-22s"
					"%-10d%-10d%-10d%-10d%-10d%-10d%-10lld"
					"%-10lld%-10lld%-10lld%-10lld%-10lld"
					"%-10lld%-10lld\n",
				pPhyData->m_nIndex, pPhyData->m_cDesc,
				pPhyData->m_nMode, pPhyData->m_nType,
				pPhyData->m_cSpeed, pMAC, cMAC,
				pPhyData->m_nLinkStatus,
				pPhyData->m_nPCISlot, pPhyData->m_nPCIBus,
				pPhyData->m_nIRQ, pPhyData->m_nCollision,
				pPhyData->m_nMulticast,
				pPhyData->m_nRxBytes,
				pPhyData->m_nRxDropped,
				pPhyData->m_nRxErrors,
				pPhyData->m_nRxPackets,
				pPhyData->m_nTxBytes,
				pPhyData->m_nTxDropped,
				pPhyData->m_nTxErrors,
				pPhyData->m_nTxPackets);
				nLength += 259;
			}
		}
	}
	*eof = 1;
	kfree(pPhyData);
	return nLength;
}

/*
 * s2io_bdsnmp_init - Entry point to create proc file
 * @dev-  Pointer to net device structure passed by the driver.
 * Return
 * Success If creates all the files
 * ERROR_PROC_ENTRY /ERROR_PROC_DIR Error If could not create all the files
 * Description
 * This functon is called when the driver is loaded. It creates the S2IO
 * proc file system in the /proc/net/ directory. This directory is used to
 * store the info about the base driver afm driver, lacp, vlan  and nplus.
 * It checks if S2IO directory already exists else creates it and creates
 * the files BDInfo  files and assiciates read function to each of the files.
 */

int s2io_bdsnmp_init(struct net_device *dev)
{
#if defined(__VMKLNX__)
        /*
	 * Create S2IODIRNAME in case it is not created yet.
	 */
	if (!vmklnx_proc_entry_exists(S2IODIRNAME, proc_net)) {
           if (!create_proc_entry(S2IODIRNAME, S_IFDIR, proc_net)) {
              DBG_PRINT(INIT_DBG, "Error Creating Proc directory for SNMP\n");
              return ERROR_PROC_DIR;
           }
        }

	if (!create_proc_read_entry(BDFILENAME, S_IFREG | S_IRUSR, proc_net,
			fnBaseReadProc, (void *) dev)) {
		DBG_PRINT(INIT_DBG, "Error Creating Proc File for Base Drvr\n");
		return ERROR_PROC_ENTRY;
	}

	if (!create_proc_read_entry(PADAPFILENAME, S_IFREG | S_IRUSR,
			proc_net, fnPhyAdapReadProc, (void *) dev)) {
		DBG_PRINT(INIT_DBG, "Error Creating Proc File for Phys Adap\n");
		return ERROR_PROC_ENTRY;
	}
#else /* !defined(__VMKLNX__) */
	struct proc_dir_entry *S2ioDir;
	struct proc_dir_entry *BaseDrv;
	struct proc_dir_entry *PhyAdap;
	int nLength = 0;

	nLength = strlen(S2IODIRNAME);
	/* IF the directory already exists then just return */
	for (S2ioDir = proc_net->subdir; S2ioDir != NULL;
		S2ioDir = S2ioDir->next) {
		if ((S2ioDir->namelen == nLength)
			&& (!memcmp(S2ioDir->name, S2IODIRNAME, nLength)))
			break;
	}
	if (S2ioDir == NULL)
		/* Create the s2io directory */
		if (!(S2ioDir =
			create_proc_entry(S2IODIRNAME, S_IFDIR, proc_net))) {
			DBG_PRINT(INIT_DBG,
				"Error Creating Proc directory for SNMP\n");
			return ERROR_PROC_DIR;
		}
	/* Create the BDInfo file to store driver info and associate
	* read funtion
	*/
	if (!(BaseDrv =
		create_proc_read_entry(BDFILENAME, S_IFREG | S_IRUSR, S2ioDir,
			fnBaseReadProc, (void *) dev))) {
		DBG_PRINT(INIT_DBG, "Error Creating Proc File for Base Drvr\n");
		return ERROR_PROC_ENTRY;
	}
	if (!(PhyAdap =
		create_proc_read_entry(PADAPFILENAME, S_IFREG | S_IRUSR,
			S2ioDir, fnPhyAdapReadProc, (void *) dev))) {
		DBG_PRINT(INIT_DBG, "Error Creating Proc File for Phys Adap\n");
		return ERROR_PROC_ENTRY;
	}
#endif /* defined(__VMKLNX__) */


	return SUCCESS;
}

/**
 * s2io_bdsnmp_rem : Removes the proc file entry
 * @dev - pointer to netdevice structre
 * Return - void
 * Description
 * This functon is called when the driver is Unloaded. It checks if the
 * S2IO directoy exists and deletes the files in the reverse order of
 * creation.
 */

void s2io_bdsnmp_rem(struct net_device *dev)
{
#if defined(__VMKLNX__)
	remove_proc_entry(BDFILENAME, proc_net);
	remove_proc_entry(PADAPFILENAME, proc_net);

	/*
	 * if S2IODIRNAME is not empty, the
	 * remove call will fail.
	 */
        remove_proc_entry(S2IODIRNAME, proc_net);
#else /* !defined(__VMKLNX__)
	int nLength = 0;
	struct proc_dir_entry *S2ioDir;
	nLength = strlen(S2IODIRNAME);
	/*
	* Check if the S2IO directory exists or not and then delete
	* all the files in the S2IO Directory
	*/
	for (S2ioDir = proc_net->subdir; S2ioDir != NULL;
		S2ioDir = S2ioDir->next) {
			if ((S2ioDir->namelen == nLength)
				&& (!memcmp(S2ioDir->name, S2IODIRNAME,
					nLength)))
			break;
	}
	if (S2ioDir == NULL)
		return;
	remove_proc_entry(BDFILENAME, S2ioDir);
	remove_proc_entry(PADAPFILENAME, S2ioDir);
	if (S2ioDir->subdir == NULL)
		remove_proc_entry(S2IODIRNAME, proc_net);
#endif /* defined(__VMKLNX__) */
}
#endif
/*
 *  To build the driver,
 * gcc -D__KERNEL__ -DMODULE -I/usr/src/linux-2.4/include -Wall
 * -Wstrict-prototypes -O2 -c s2io.c
 */
