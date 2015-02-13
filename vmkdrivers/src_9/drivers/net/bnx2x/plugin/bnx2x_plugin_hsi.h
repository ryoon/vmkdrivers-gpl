/* bnx2x_plugin_hsi.h: Broadcom bnx2x NPA plugin
 *
 * Copyright 2009-2011 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by: Shmulik Ravid
 *
 */
#ifndef __BNX2X_PLUGIN_HSI__
#define __BNX2X_PLUGIN_HSI__

/*
 * common data for all protocols
 */
struct doorbell_hdr {
	u8 header;
#define DOORBELL_HDR_RX (0x1<<0)
#define DOORBELL_HDR_RX_SHIFT 0
#define DOORBELL_HDR_DB_TYPE (0x1<<1)
#define DOORBELL_HDR_DB_TYPE_SHIFT 1
#define DOORBELL_HDR_DPM_SIZE (0x3<<2)
#define DOORBELL_HDR_DPM_SIZE_SHIFT 2
#define DOORBELL_HDR_CONN_TYPE (0xF<<4)
#define DOORBELL_HDR_CONN_TYPE_SHIFT 4
};


/*
 * doorbell message sent to the chip
 */
struct doorbell_set_prod {
#if defined(__BIG_ENDIAN)
	u16 prod;
	u8 zero_fill1;
	struct doorbell_hdr header;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr header;
	u8 zero_fill1;
	u16 prod;
#endif
};


struct regpair {
	__le32 lo;
	__le32 hi;
};


/*
 * 3 lines. status block $$KEEP_ENDIANNESS$$
 */
struct hc_status_block_e1x {
	__le16 index_values[HC_SB_MAX_INDICES_E1X];
	__le16 running_index[HC_SB_MAX_SM];
	u32 rsrv;
};

/*
 * host status block
 */
struct host_hc_status_block_e1x {
	struct hc_status_block_e1x sb;
};


/*
 * 3 lines. status block $$KEEP_ENDIANNESS$$
 */
struct hc_status_block_e2 {
	__le16 index_values[HC_SB_MAX_INDICES_E2];
	__le16 running_index[HC_SB_MAX_SM];
	u32 reserved;
};

/*
 * host status block
 */
struct host_hc_status_block_e2 {
	struct hc_status_block_e2 sb;
};


/*
 * IGU driver acknowledgment register
 */
struct igu_ack_register {
#if defined(__BIG_ENDIAN)
	u16 sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F<<0)
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7<<5)
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1<<8)
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3<<9)
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F<<11)
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
	u16 status_block_index;
#elif defined(__LITTLE_ENDIAN)
	u16 status_block_index;
	u16 sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F<<0)
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7<<5)
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1<<8)
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3<<9)
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F<<11)
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
#endif
};


/*
 * Control register for the IGU command register
 */
struct igu_ctrl_reg {
	u32 ctrl_data;
#define IGU_CTRL_REG_ADDRESS (0xFFF<<0)
#define IGU_CTRL_REG_ADDRESS_SHIFT 0
#define IGU_CTRL_REG_FID (0x7F<<12)
#define IGU_CTRL_REG_FID_SHIFT 12
#define IGU_CTRL_REG_RESERVED (0x1<<19)
#define IGU_CTRL_REG_RESERVED_SHIFT 19
#define IGU_CTRL_REG_TYPE (0x1<<20)
#define IGU_CTRL_REG_TYPE_SHIFT 20
#define IGU_CTRL_REG_UNUSED (0x7FF<<21)
#define IGU_CTRL_REG_UNUSED_SHIFT 21
};


/*
 * IGU driver acknowledgement register
 */
struct igu_regular {
	u32 sb_id_and_flags;
#define IGU_REGULAR_SB_INDEX (0xFFFFF<<0)
#define IGU_REGULAR_SB_INDEX_SHIFT 0
#define IGU_REGULAR_RESERVED0 (0x1<<20)
#define IGU_REGULAR_RESERVED0_SHIFT 20
#define IGU_REGULAR_SEGMENT_ACCESS (0x7<<21)
#define IGU_REGULAR_SEGMENT_ACCESS_SHIFT 21
#define IGU_REGULAR_BUPDATE (0x1<<24)
#define IGU_REGULAR_BUPDATE_SHIFT 24
#define IGU_REGULAR_ENABLE_INT (0x3<<25)
#define IGU_REGULAR_ENABLE_INT_SHIFT 25
#define IGU_REGULAR_RESERVED_1 (0x1<<27)
#define IGU_REGULAR_RESERVED_1_SHIFT 27
#define IGU_REGULAR_CLEANUP_TYPE (0x3<<28)
#define IGU_REGULAR_CLEANUP_TYPE_SHIFT 28
#define IGU_REGULAR_CLEANUP_SET (0x1<<30)
#define IGU_REGULAR_CLEANUP_SET_SHIFT 30
#define IGU_REGULAR_BCLEANUP (0x1<<31)
#define IGU_REGULAR_BCLEANUP_SHIFT 31
	u32 reserved_2;
};


/*
 * Parser parsing flags field
 */
struct parsing_flags {
	__le16 flags;
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE (0x1<<0)
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE_SHIFT 0
#define PARSING_FLAGS_INNER_VLAN_EXIST (0x1<<1)
#define PARSING_FLAGS_INNER_VLAN_EXIST_SHIFT 1
#define PARSING_FLAGS_OUTER_VLAN_EXIST (0x1<<2)
#define PARSING_FLAGS_OUTER_VLAN_EXIST_SHIFT 2
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL (0x3<<3)
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT 3
#define PARSING_FLAGS_IP_OPTIONS (0x1<<5)
#define PARSING_FLAGS_IP_OPTIONS_SHIFT 5
#define PARSING_FLAGS_FRAGMENTATION_STATUS (0x1<<6)
#define PARSING_FLAGS_FRAGMENTATION_STATUS_SHIFT 6
#define PARSING_FLAGS_OVER_IP_PROTOCOL (0x3<<7)
#define PARSING_FLAGS_OVER_IP_PROTOCOL_SHIFT 7
#define PARSING_FLAGS_PURE_ACK_INDICATION (0x1<<9)
#define PARSING_FLAGS_PURE_ACK_INDICATION_SHIFT 9
#define PARSING_FLAGS_TCP_OPTIONS_EXIST (0x1<<10)
#define PARSING_FLAGS_TCP_OPTIONS_EXIST_SHIFT 10
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG (0x1<<11)
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG_SHIFT 11
#define PARSING_FLAGS_CONNECTION_MATCH (0x1<<12)
#define PARSING_FLAGS_CONNECTION_MATCH_SHIFT 12
#define PARSING_FLAGS_LLC_SNAP (0x1<<13)
#define PARSING_FLAGS_LLC_SNAP_SHIFT 13
#define PARSING_FLAGS_RESERVED0 (0x3<<14)
#define PARSING_FLAGS_RESERVED0_SHIFT 14
};


/*
 * SDM operation gen command (generate aggregative interrupt)
 */
struct sdm_op_gen {
	__le32 command;
#define SDM_OP_GEN_COMP_PARAM (0x1F<<0)
#define SDM_OP_GEN_COMP_PARAM_SHIFT 0
#define SDM_OP_GEN_COMP_TYPE (0x7<<5)
#define SDM_OP_GEN_COMP_TYPE_SHIFT 5
#define SDM_OP_GEN_AGG_VECT_IDX (0xFF<<8)
#define SDM_OP_GEN_AGG_VECT_IDX_SHIFT 8
#define SDM_OP_GEN_AGG_VECT_IDX_VALID (0x1<<16)
#define SDM_OP_GEN_AGG_VECT_IDX_VALID_SHIFT 16
#define SDM_OP_GEN_RESERVED (0x7FFF<<17)
#define SDM_OP_GEN_RESERVED_SHIFT 17
};


/*
 * union for sgl and raw data.
 */
union eth_sgl_or_raw_data {
	__le16 sgl[8];
	__le32 raw_data[4];
};

/*
 * regular eth FP CQE parameters struct $$KEEP_ENDIANNESS$$
 */
struct eth_fast_path_rx_cqe {
	u8 type_error_flags;
#define ETH_FAST_PATH_RX_CQE_TYPE (0x1<<0)
#define ETH_FAST_PATH_RX_CQE_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG (0x1<<1)
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG_SHIFT 1
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG (0x1<<2)
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG_SHIFT 2
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG (0x1<<3)
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_START_FLG (0x1<<4)
#define ETH_FAST_PATH_RX_CQE_START_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_END_FLG (0x1<<5)
#define ETH_FAST_PATH_RX_CQE_END_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL (0x3<<6)
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL_SHIFT 6
	u8 status_flags;
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE (0x7<<0)
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG (0x1<<3)
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG (0x1<<4)
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG (0x1<<5)
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG (0x1<<6)
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG_SHIFT 6
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG (0x1<<7)
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG_SHIFT 7
	u8 placement_offset;
	u8 queue_index;
	__le32 rss_hash_result;
	__le16 vlan_tag;
	__le16 pkt_len;
	__le16 len_on_bd;
	struct parsing_flags pars_flags;
	union eth_sgl_or_raw_data sgl_or_raw_data;
};


/*
 * The eth Rx Buffer Descriptor
 */
struct eth_rx_bd {
	__le32 addr_lo;
	__le32 addr_hi;
};


/*
 * Place holder for ramrods protocol specific data
 */
struct ramrod_data {
	__le32 data_lo;
	__le32 data_hi;
};

/*
 * Eth Rx Cqe structure- general structure for ramrods $$KEEP_ENDIANNESS$$
 */
struct common_ramrod_eth_rx_cqe {
	u8 ramrod_type;
#define COMMON_RAMROD_ETH_RX_CQE_TYPE (0x1<<0)
#define COMMON_RAMROD_ETH_RX_CQE_TYPE_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_ERROR (0x1<<1)
#define COMMON_RAMROD_ETH_RX_CQE_ERROR_SHIFT 1
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0 (0x3F<<2)
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0_SHIFT 2
	u8 conn_type;
	__le16 reserved1;
	__le32 conn_and_cmd_data;
#define COMMON_RAMROD_ETH_RX_CQE_CID (0xFFFFFF<<0)
#define COMMON_RAMROD_ETH_RX_CQE_CID_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID (0xFF<<24)
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT 24
	struct ramrod_data protocol_data;
	__le32 reserved2[4];
};

/*
 * Rx Last CQE in page (in ETH)
 */
struct eth_rx_cqe_next_page {
	__le32 addr_lo;
	__le32 addr_hi;
	__le32 reserved[6];
};

/*
 * union for all eth rx cqe types (fix their sizes)
 */
union eth_rx_cqe {
	struct eth_fast_path_rx_cqe fast_path_cqe;
	struct common_ramrod_eth_rx_cqe ramrod_cqe;
	struct eth_rx_cqe_next_page next_page_cqe;
};



/*
 * The eth Rx SGE Descriptor
 */
struct eth_rx_sge {
	__le32 addr_lo;
	__le32 addr_hi;
};



/*
 * Tx regular BD structure $$KEEP_ENDIANNESS$$
 */
struct eth_tx_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	__le16 total_pkt_bytes;
	__le16 nbytes;
	u8 reserved[4];
};


/*
 * structure for easy accessibility to assembler
 */
struct eth_tx_bd_flags {
	u8 as_bitfield;
#define ETH_TX_BD_FLAGS_IP_CSUM (0x1<<0)
#define ETH_TX_BD_FLAGS_IP_CSUM_SHIFT 0
#define ETH_TX_BD_FLAGS_L4_CSUM (0x1<<1)
#define ETH_TX_BD_FLAGS_L4_CSUM_SHIFT 1
#define ETH_TX_BD_FLAGS_VLAN_MODE (0x3<<2)
#define ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT 2
#define ETH_TX_BD_FLAGS_START_BD (0x1<<4)
#define ETH_TX_BD_FLAGS_START_BD_SHIFT 4
#define ETH_TX_BD_FLAGS_IS_UDP (0x1<<5)
#define ETH_TX_BD_FLAGS_IS_UDP_SHIFT 5
#define ETH_TX_BD_FLAGS_SW_LSO (0x1<<6)
#define ETH_TX_BD_FLAGS_SW_LSO_SHIFT 6
#define ETH_TX_BD_FLAGS_IPV6 (0x1<<7)
#define ETH_TX_BD_FLAGS_IPV6_SHIFT 7
};


/*
 * The eth Tx Buffer Descriptor $$KEEP_ENDIANNESS$$
 */
struct eth_tx_start_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	__le16 nbd;
	__le16 nbytes;
	__le16 vlan_or_ethertype;
	struct eth_tx_bd_flags bd_flags;
	u8 general_data;
#define ETH_TX_START_BD_HDR_NBDS (0x3F<<0)
#define ETH_TX_START_BD_HDR_NBDS_SHIFT 0
#define ETH_TX_START_BD_ETH_ADDR_TYPE (0x3<<6)
#define ETH_TX_START_BD_ETH_ADDR_TYPE_SHIFT 6
};

/*
 * Tx parsing BD structure for ETH E1/E1h $$KEEP_ENDIANNESS$$
 */
struct eth_tx_parse_bd_e1x {
	u8 global_data;
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W (0xF<<0)
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_RESERVED0 (0x1<<4)
#define ETH_TX_PARSE_BD_E1X_RESERVED0_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN (0x1<<5)
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN_SHIFT 5
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN (0x1<<6)
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_NS_FLG (0x1<<7)
#define ETH_TX_PARSE_BD_E1X_NS_FLG_SHIFT 7
	u8 tcp_flags;
#define ETH_TX_PARSE_BD_E1X_FIN_FLG (0x1<<0)
#define ETH_TX_PARSE_BD_E1X_FIN_FLG_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_SYN_FLG (0x1<<1)
#define ETH_TX_PARSE_BD_E1X_SYN_FLG_SHIFT 1
#define ETH_TX_PARSE_BD_E1X_RST_FLG (0x1<<2)
#define ETH_TX_PARSE_BD_E1X_RST_FLG_SHIFT 2
#define ETH_TX_PARSE_BD_E1X_PSH_FLG (0x1<<3)
#define ETH_TX_PARSE_BD_E1X_PSH_FLG_SHIFT 3
#define ETH_TX_PARSE_BD_E1X_ACK_FLG (0x1<<4)
#define ETH_TX_PARSE_BD_E1X_ACK_FLG_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_URG_FLG (0x1<<5)
#define ETH_TX_PARSE_BD_E1X_URG_FLG_SHIFT 5
#define ETH_TX_PARSE_BD_E1X_ECE_FLG (0x1<<6)
#define ETH_TX_PARSE_BD_E1X_ECE_FLG_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_CWR_FLG (0x1<<7)
#define ETH_TX_PARSE_BD_E1X_CWR_FLG_SHIFT 7
	u8 ip_hlen_w;
	s8 reserved;
	__le16 total_hlen_w;
	__le16 tcp_pseudo_csum;
	__le16 lso_mss;
	__le16 ip_id;
	__le32 tcp_send_seq;
};

/*
 * Tx parsing BD structure for ETH E2 $$KEEP_ENDIANNESS$$
 */
struct eth_tx_parse_bd_e2 {
	__le16 dst_mac_addr_lo;
	__le16 dst_mac_addr_mid;
	__le16 dst_mac_addr_hi;
	__le16 src_mac_addr_lo;
	__le16 src_mac_addr_mid;
	__le16 src_mac_addr_hi;
	__le32 parsing_data;
#define ETH_TX_PARSE_BD_E2_TCP_HDR_START_OFFSET_W (0x1FFF<<0)
#define ETH_TX_PARSE_BD_E2_TCP_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW (0xF<<13)
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW_SHIFT 13
#define ETH_TX_PARSE_BD_E2_LSO_MSS (0x3FFF<<17)
#define ETH_TX_PARSE_BD_E2_LSO_MSS_SHIFT 17
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR (0x1<<31)
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR_SHIFT 31
};

/*
 * The last BD in the BD memory will hold a pointer to the next BD memory
 */
struct eth_tx_next_bd {
	__le32 addr_lo;
	__le32 addr_hi;
	u8 reserved[8];
};

/*
 * union for 4 Bd types
 */
union eth_tx_bd_types {
	struct eth_tx_start_bd start_bd;
	struct eth_tx_bd reg_bd;
	struct eth_tx_parse_bd_e1x parse_bd_e1x;
	struct eth_tx_parse_bd_e2 parse_bd_e2;
	struct eth_tx_next_bd next_bd;
};






/*
 * Three RX producers for ETH
 */
struct ustorm_eth_rx_producers {
#if defined(__BIG_ENDIAN)
	u16 bd_prod;
	u16 cqe_prod;
#elif defined(__LITTLE_ENDIAN)
	u16 cqe_prod;
	u16 bd_prod;
#endif
#if defined(__BIG_ENDIAN)
	u16 reserved;
	u16 sge_prod;
#elif defined(__LITTLE_ENDIAN)
	u16 sge_prod;
	u16 reserved;
#endif
};





/*
 * hold PCI identification variables- used in various places in firmware
 */
struct pci_entity {
#if defined(__BIG_ENDIAN)
	u8 vf_valid;
	u8 vf_id;
	u8 vnic_id;
	u8 pf_id;
#elif defined(__LITTLE_ENDIAN)
	u8 pf_id;
	u8 vnic_id;
	u8 vf_id;
	u8 vf_valid;
#endif
};



/*
 * zone A per-queue data
 */
struct ustorm_queue_zone_data {
	struct ustorm_eth_rx_producers eth_rx_producers;
	struct regpair reserved[3];
};

#endif /* __BNX2X_PLUGIN_HSI__ */
