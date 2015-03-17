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
/* Header file with definitions for both host and Phantom */

#ifndef UNM_NIC_CMN_H
#define UNM_NIC_CMN_H

#include "unm_nic_config.h"
#include "unm_compiler_defs.h"
#include "nx_types.h"

/****************************************************************************
 *		Define of versions when support for features where added.
 *		Helps maintain compatibility.
 ****************************************************************************/
#define	NX_MSIX_SUPPORT_MAJOR		3
#define	NX_MSIX_SUPPORT_MINOR		4
#define	NX_MSIX_SUPPORT_SUBVERSION	330
#define IS_NX_FW_DEV_BUILD(BUILDID)	(BUILDID == 0xffffffff)

/****************************************************************************/

#define	NX_MSIX_MEM_REGION_THRESHOLD	0x2000000

#define IP_ALIGNMENT_BYTES		2  /* make ip aligned on 16byteaddr */

#if 0
#define P3_MAX_MTU			(9600)
#else
#define P3_MAX_MTU			(9000)
#endif

#define NX_ETHERMTU			1500
#define NX_MAX_ETHERHDR			32 /* This contains some padding */

#define NX_RX_NORMAL_BUF_MAX_LEN	(NX_MAX_ETHERHDR + NX_ETHERMTU)
#define NX_P3_RX_JUMBO_BUF_MAX_LEN	(NX_MAX_ETHERHDR + P3_MAX_MTU)

#define RX_JUMBO_DMA_MAP_LEN (MAX_RX_JUMBO_BUFFER_LENGTH-IP_ALIGNMENT_BYTES)

/* 
 * In the Old 3.4.xyz firmware the check for jumbo was skewed.
 * So need to allocate more memory for for normal ring.
*/
#define NX_RX_V34_NORMAL_BUF_MAX_LEN	(1750)

/*
 * I am not sure of the math behind 9046 above, so setting this to -
 *   (9046-8000) + (9*1024)
 *
 * Even more unsure. Not used in FW. All the drivers should get rid of this and
 * use NX_P2_RX_JUMBO_BUF_MAX_LEN or NX_P3_RX_JUMBO_BUF_MAX_LEN.
 */
#define	MAX_RX_JUMBO_BUFFER_LENGTH	((9 * 1024) + (9046 - 8000))


#define MAX_RX_LRO_BUFFER_LENGTH       ((8*1024)-512)
#define RX_JUMBO_DMA_MAP_LEN   (MAX_RX_JUMBO_BUFFER_LENGTH-IP_ALIGNMENT_BYTES)
#define RX_LRO_DMA_MAP_LEN       (MAX_RX_LRO_BUFFER_LENGTH-IP_ALIGNMENT_BYTES)

#define	NUM_EPG_FILTERS		64

/* Opcodes to be used with the commands */
#define    TX_ETHER_PKT    0x01
/* The following opcodes are for IP checksum    */
#define    TX_TCP_PKT      0x02
#define    TX_UDP_PKT      0x03
#define    TX_IP_PKT       0x04
#define    TX_TCP_LSO      0x05
#define    TX_TCP_LSO6     0x06
#define    TX_IPSEC        0x07
#ifdef NX_FCOE_SUPPORT1
/* Opcode for FCoE packet */
#define	   TX_FCOE_PKT     0x08
#endif
#define    TX_IPSEC_CMD    0x0a
#define    TX_TCPV6_PKT    0x0b
#define    TX_UDPV6_PKT    0x0c
#define    TX_TOE_LSO      0x0d
#define    TX_TOE_LSO6     0x0e
#define    TX_FLUSH_QUEUE  0x0f


/* The following opcodes are for internal consumption. */
#define UNM_CONTROL_OP		0x10
#define PEGNET_REQUEST		0x11
#define NX_NIC_HOST_REQUEST	0x13
#define NIC_REQUEST             0x14
#define NX_NIC_LRO_REQUEST	0x15

#define	NX_FILTER		0x16
#define	FILTER_ADD		0
#define	FILTER_DEL		1
#define	FILTER_REPLACE		2

#ifdef UNM_RSS
#define       RSS_CNTRL_CMD       0x20
#endif

#define    DESC_CHAIN      0xFF    /* descriptor command continuation */

#define MAX_BUFFERS_PER_CMD 16

#define DUMMY_BUF_UNINIT        0x55555555
#define DUMMY_BUF_INIT          0
/*
 * Following are the states of the Phantom. Phantom will set them and
 * Host will read to check if the fields are correct.
 */
#define PHAN_INITIALIZE_START         0xff00
#define PHAN_INITIALIZE_FAILED        0xffff
#define PHAN_INITIALIZE_COMPLETE      0xff01

/* Host writes the following to notify that it has done the init-handshake */
#define PHAN_INITIALIZE_ACK           0xf00f

/* Following defines will be used in the status descriptor */
#define TX_ETHER_PKT_COMPLETE  0xB  /* same for both commands */

#define NUM_RCV_DESC_RINGS     2 /* No of Rcv Descriptor contexts */
#define NUM_RX_RULES     1 

/* descriptor types */
#define RCV_DESC_NORMAL        0x01
#define RCV_DESC_JUMBO         0x02
#define RCV_DESC_LRO           0x04
#define RCV_DESC_NORMAL_CTXID     0
#define RCV_DESC_JUMBO_CTXID      1
#define RCV_DESC_LRO_CTXID        2

#define RCV_DESC_TYPE(ID) \
       ((ID == RCV_DESC_JUMBO_CTXID) ? RCV_DESC_JUMBO :  \
         ((ID == RCV_DESC_LRO_CTXID) ? RCV_DESC_LRO : (RCV_DESC_NORMAL)))

#define RCV_DESC_TYPE_NAME(ID) \
       ((ID == RCV_DESC_JUMBO_CTXID)  ? "Jumbo"  :  \
        (ID == RCV_DESC_LRO_CTXID)    ? "LRO"    :  \
        (ID == RCV_DESC_NORMAL_CTXID) ? "Normal" : "Unknown")


/*
 * TBD: All these #defines need to go to OS specific headers and should not
 * be in nic_cmn.h. No longer has anything to do with Firmware.
 */
#define MAX_CMD_DESCRIPTORS       4096
#define MAX_CMD_DESCRIPTORS_HOST  (MAX_CMD_DESCRIPTORS / 4)

#define NX_MAX_SUPPORTED_RDS_SIZE	(32 * 1024)
#define NX_MAX_SUPPORTED_JUMBO_RDS_SIZE	(4 * 1024)
#define NX_MIN_SUPPORTED_RDS_SIZE	(2)

#define MAX_EPG_DESCRIPTORS       (MAX_CMD_DESCRIPTORS  * 8)


#define MAX_FRAME_SIZE            0x10000    /* 64K MAX size  for LSO */

#define PHAN_PEG_RCV_INITIALIZED      0xff01
#define PHAN_PEG_RCV_START_INITIALIZE 0xff00

/*
 *  CAVEAT: Use INCR_RING_INDEX and INCR_RING_INDEX_BY_COUNT only when
 *  ring_size is a power of 2
 */
#define INCR_RING_INDEX_BY_COUNT(ring_index, count, ring_size) \
	ring_index = (((ring_index) + (count)) & ((ring_size) - 1))

#define INCR_RING_INDEX(ring_index, ring_size) \
	INCR_RING_INDEX_BY_COUNT(ring_index, 1, ring_size)

#define get_next_index(index,length)  ((((index)  + 1) == length)?0:(index) +1)


#define get_index_range(index,length,count)  ((((index) + (count)) >= length)? \
                       (((index)  + (count))-(length)):((index) + (count)))

/*
 * Set the time bases for timer sets.
 */
#define UNM_FLOW_TICKS_PER_SEC    256
#define UNM_FLOW_TO_TV_SHIFT_SEC  8
#define UNM_FLOW_TO_TV_SHIFT_USEC 12
#define UNM_FLOW_TICK_USEC   (1000000ULL/UNM_FLOW_TICKS_PER_SEC)
#define UNM_GLOBAL_TICKS_PER_SEC  (32*UNM_FLOW_TICKS_PER_SEC)
#define UNM_GLOBAL_TICK_USEC (1000000ULL/UNM_GLOBAL_TICKS_PER_SEC)

/*
 * Following data structures describe the descriptors that will be used.
 * Added fileds of tcpHdrSize and ipHdrSize, The driver needs to do it only when
 * we are doing LSO (above the 1500 size packet) only.
 * This is an overhead but we need it. Let me know if you have questions.
 */

/* the size of reference handle been changed to 16 bits to pass the MSS fields
   for the LSO packet */

#define FLAGS_MCAST                  0x01
#define FLAGS_LSO_ENABLED            0x02
#define FLAGS_IPSEC_SA_ADD           0x04
#define FLAGS_IPSEC_SA_DELETE        0x08
#define FLAGS_VLAN_TAGGED            0x10
#define FLAGS_COMPLETION             0x20
#define FLAGS_VLAN_OOB		     0x40

#if UNM_CONF_PROCESSOR==UNM_CONF_X86
#ifndef SYS_TYPES
typedef __uint64_t U64;
typedef __uint32_t U32;
typedef __uint16_t U16;
typedef __uint8_t  U8;
#endif
#endif

#define MAX_RING_CTX 4  /* Should no longer be used */
#if defined(NX_PORT_SHARING) || defined(P3)
#define MAX_RX_CONTEXTS 64
#else
#define MAX_RX_CONTEXTS 8
#endif
#define MAX_TX_CONTEXTS 8

#if defined(P3)
#define NCSI_CTXID	(MAX_RX_CONTEXTS-1)
#endif /* P3 */

#define RCV_CTX_OK    0x00f0
#define RING_CTX_OK   0x0f00

#define PEXQ_DB_NUMBER          4

#define SIGNATURE_STATE_MASK 	0xfff0
#define SIGNATURE_FUNCTION_MASK 0x000f
#define SIGNATURE_VERSION_MASK	0x00ff0000

#define SIGNATURE_TO_STATE(Signature) ((Signature) & SIGNATURE_STATE_MASK)
#define SIGNATURE_TO_FUNCTION(Signature) ((Signature) & SIGNATURE_FUNCTION_MASK)
#define SIGNATURE_TO_VERSION(Signature)			\
	(((Signature) & SIGNATURE_VERSION_MASK) >> 16)

#define UNM_CTX_SIGNATURE_V2	0x0002dee0
#define UNM_CTX_SIGNATURE       0xdee0
#define UNM_CTX_RESET           0xbad0
#define UNM_CTX_D3_RESET        0xacc0

#define UNM_CTX_SIGNATURE_RCV_CTX_OK  (RCV_CTX_OK | UNM_CTX_SIGNATURE)//0xdef0
#define UNM_CTX_RESET_RCV_CTX_OK      (RCV_CTX_OK | UNM_CTX_RESET)    //0xbaf0
#define UNM_CTX_D3_RESET_RCV_CTX_OK   (RCV_CTX_OK | UNM_CTX_D3_RESET) //0xacf0

#define UNM_CTX_SIGNATURE_RING_CTX_OK (RING_CTX_OK | UNM_CTX_SIGNATURE)//0xdfe0
#define UNM_CTX_RESET_RING_CTX_OK     (RING_CTX_OK | UNM_CTX_RESET)    //0xbfd0
#define UNM_CTX_D3_RESET_RING_CTX_OK  (RING_CTX_OK | UNM_CTX_D3_RESET) //0xafc0

#define IS_RCV_CTX_PENDING(Signature, BaseState) 			\
        (SIGNATURE_TO_STATE(Signature) == BaseState ||          	\
         SIGNATURE_TO_STATE(Signature) == (RING_CTX_OK | BaseState))

#define IS_RING_CTX_PENDING(Signature, BaseState) 			\
        (SIGNATURE_TO_STATE(Signature) == BaseState ||          	\
         SIGNATURE_TO_STATE(Signature) == (RCV_CTX_OK | BaseState))
                
//define opcode for ctx_msg
#define  RX_PRODUCER 0
#define  RX_PRODUCER_JUMBO 1
#define  RX_PRODUCER_LRO 2
#define  TX_PRODUCER 3
#define  UPDATE_STATUS_CONSUMER 4
#define  RESET_CTX  5

#define  NUM_DB_CODE 6

#define UNM_RCV_PRODUCER(ringid)        (ringid)
#define UNM_CMD_PRODUCER                TX_PRODUCER
#define UNM_RCV_STATUS_CONSUMER         UPDATE_STATUS_CONSUMER

typedef struct __msg
{
    __uint32_t  PegId:2,   // 0x2 for tx and 01 for rx.
                privId:1,  // must be 1
                Count:15,  // for doorbell
                CtxId:10,  // Ctx_id
                Opcode:4;  //opcode
}ctx_msg,CTX_MSG,*PCTX_MSG;

typedef struct __int_msg
{
    __uint32_t  Count:18, // INT
                ConsumerIdx:10,
                CtxId:4; // Ctx_id
}int_msg,INT_MSG,*PINT_MSG;

/* For use in CRB_MPORT_MODE */
#define MPORT_SINGLE_FUNCTION_MODE 0x1111
#define MPORT_MULTI_FUNCTION_MODE 0x2222

//#ifdef UNM_MPORT
typedef struct _RcvContext
{
    __uint64_t     RcvRingAddrLo:32,
                   RcvRingAddrHi:32;
    __uint32_t     RcvRingSize;
    __uint32_t     Rsrv;
}RcvContext;

/*
 * CARD --> HOST
 *
 * Send received data to host using NIC receive buffers.
 */
typedef struct {
	union {
		struct {
			__uint64_t      ctx:32,
					flags:8,
					ring_type:4,
					rsvd_0:9,
					desc_cnt:3,     /* Set in the NIC path
							   before DMA to host*/
					owner:2,        /* Set in the NIC path
							   before DMA to host*/
					qmsg_type:6;       /* Qmsg or NIC rcv
							      desc type */

			__uint32_t	seq_num;	/* Sequence # */
			__uint16_t	ref_handle;	/* rcv buff refhandle */
			__uint16_t	len;       	/* Length of data */
		};
		__uint64_t    body[2];
	};
} tnic_rxbuff_t;

/*
 * HOST --> CARD
 *
 * The following are the opcodes for messages from host -> card for NIC config
 * requests. These messages have queue message type UNM_MSGTYPE_NIC_REQUEST.
 */
#define NX_NIC_H2C_OPCODE_START				0
#define NX_NIC_H2C_OPCODE_CONFIG_RSS			1
#define NX_NIC_H2C_OPCODE_CONFIG_RSS_TBL		2
#define NX_NIC_H2C_OPCODE_CONFIG_INTR_COALESCE		3
#define NX_NIC_H2C_OPCODE_CONFIG_LED			4
#define NX_NIC_H2C_OPCODE_CONFIG_PROMISCUOUS		5
#define NX_NIC_H2C_OPCODE_CONFIG_L2_MAC			6
#define NX_NIC_H2C_OPCODE_LRO_REQUEST			7
#define NX_NIC_H2C_OPCODE_GET_SNMP_STATS		8
#define NX_NIC_H2C_OPCODE_PROXY_START_REQUEST		9
#define NX_NIC_H2C_OPCODE_PROXY_STOP_REQUEST		10
#define NX_NIC_H2C_OPCODE_PROXY_SET_MTU			11
#define NX_NIC_H2C_OPCODE_PROXY_SET_VPORT_MISS_MODE	12
#define NX_NIC_H2C_OPCODE_GET_FINGER_PRINT_REQUEST	13
#define NX_NIC_H2C_OPCODE_INSTALL_LICENSE_REQUEST	14
#define NX_NIC_H2C_OPCODE_GET_LICENSE_CAPABILITY_REQUEST 15
#define NX_NIC_H2C_OPCODE_GET_NET_STATS_REQUEST		16
#define NX_NIC_H2C_OPCODE_PROXY_UPDATE_P2V		17
#define NX_NIC_H2C_OPCODE_CONFIG_IPADDR			18
#define NX_NIC_H2C_OPCODE_CONFIG_LOOPBACK		19
#define NX_NIC_H2C_OPCODE_PROXY_STOP_DONE		20
#define NX_NIC_H2C_OPCODE_GET_LINKEVENT			21
#define NX_NIC_C2C_OPCODE				22
#define NX_NIC_H2C_OPCODE_CONFIG_BRIDGING		23
#define NX_NIC_H2C_OPCODE_LAST				24

/*
 * CARD --> HOST
 *
 * The following are the opcodes for messages from card -> host for NIC config
 * request/response. These messages have queue message type
 * UNM_MSGTYPE_NIC_RESPONSE.
 */
#define NX_NIC_C2H_OPCODE_START				128
#define NX_NIC_C2H_OPCODE_CONFIG_RSS_RESPONSE		129
#define NX_NIC_C2H_OPCODE_CONFIG_RSS_TBL_RESPONSE	130
#define NX_NIC_C2H_OPCODE_CONFIG_MAC_RESPONSE		131
#define NX_NIC_C2H_OPCODE_CONFIG_PROMISCUOUS_RESPONSE	132
#define NX_NIC_C2H_OPCODE_CONFIG_L2_MAC_RESPONSE	133
#define NX_NIC_C2H_OPCODE_LRO_DELETE_RESPONSE		134
#define NX_NIC_C2H_OPCODE_LRO_ADD_FAILURE_RESPONSE	135	
#define NX_NIC_C2H_OPCODE_GET_SNMP_STATS		136
#define NX_NIC_C2H_OPCODE_GET_FINGER_PRINT_REPLY	137
#define NX_NIC_C2H_OPCODE_INSTALL_LICENSE_REPLY		138
#define NX_NIC_C2H_OPCODE_GET_LICENSE_CAPABILITIES_REPLY 139
#define NX_NIC_C2H_OPCODE_GET_NET_STATS_RESPONSE	140
#define NX_NIC_C2H_OPCODE_GET_LINKEVENT_RESPONSE	141
#define NX_NIC_C2H_OPCODE_LAST				142


enum {
	NX_ERR_INVALID_CONTEXT_ID = 1,
	NX_ERR_INVALID_TBL_IDX,
	NX_ERR_INVALID_OPERATION_TYPE,
};

#define	MAX_RSS_RINGS			8
#define	MAX_RSS_INDIR_TBL		64
#define	MAX_RSS_INDIR_MSG_ENTRY		40
#define	RSS_SECRET_KEY_QW		5
#define	CARD_SIZED_MAX_RSS_RINGS	4

/* HEADER HOST => CARD
 */
typedef union {
	struct {
		__uint64_t	opcode : 8,
				comp_id : 8,
				ctxid : 16,
				need_completion : 1,
				rsvd : 23,
				sub_opcode : 8;		/* Used by some opcodes
							 * e.g. LRO */
	};
	__uint64_t	word;
} nic_req_hdr_t;

#define RSS_HASHTYPE_NO_HASH    0x0
#define RSS_HASHTYPE_IP         0x1
#define RSS_HASHTYPE_TCP        0x2
#define RSS_HASHTYPE_IP_TCP     0x3

typedef struct {
	nic_req_hdr_t   req_hdr;
        __uint64_t      hash_method : 4,
			hash_type_v4 : 2,
			hash_type_v6 : 2,
			enable : 1,
			use_indir_tbl : 1,
			rsvd_1 : 38,
			indir_tbl_mask : 16;
        __uint64_t      secret_key[RSS_SECRET_KEY_QW];
} rss_config_t;

typedef struct {
	nic_req_hdr_t   req_hdr;
        __uint64_t      start_idx : 8,
			end_idx : 8,
			rsvd_1 : 48;
        __uint8_t       indir_tbl[MAX_RSS_INDIR_MSG_ENTRY];
} rss_config_indir_tbl_t;

enum {
	NX_NIC_INTR_ADAPTIVE_TX = 0x01,
	NX_NIC_INTR_ADAPTIVE_RX = 0x02,
	NX_NIC_INTR_ADAPTIVE_RX_TX = (NX_NIC_INTR_ADAPTIVE_TX |      \
                                         NX_NIC_INTR_ADAPTIVE_RX),
	NX_NIC_INTR_DEFAULT = 0x04,
	NX_NIC_INTR_PERIODIC = 0x08
};
/*
 * Interrupt coalescing defaults. The defaults are for 1500 MTU. It is
 * adjusted based on configured MTU.
 */
#define NX_DEFAULT_INTR_COALESCE_RX_TIME_US     3
#define NX_DEFAULT_INTR_COALESCE_RX_PACKETS     256
#define NX_DEFAULT_INTR_COALESCE_TX_PACKETS     64
#define NX_DEFAULT_INTR_COALESCE_TX_TIME_US     4

enum {
	NX_PERIODIC_INTR_KILL = 0x0,
	NX_PERIODIC_INTR_ONE_TIME,
	NX_PERIODIC_INTR_TIMER
};

typedef union {
        struct {
                __uint16_t      rx_packets;
                __uint16_t      rx_time_us;
                __uint16_t      tx_packets;
                __uint16_t      tx_time_us;
        } data;
        __uint64_t              word;
} nx_nic_intr_coalesce_data_t;

typedef union {
        struct {
               __uint32_t      timeout_us;
               __uint32_t      operation : 8,
                               sts_ring_idx_mask : 8,
                               rsvd : 16;
        } data;
        __uint64_t              word;
} nx_periodic_intr_t;

typedef struct {
	nic_req_hdr_t			req_hdr;
        __uint16_t                      stats_time_us;
        __uint16_t                      rate_sample_time;
        __uint16_t                      flags;
        __uint16_t                      rsvd_1;
        __uint32_t                      low_threshold;
        __uint32_t                      high_threshold;
        nx_nic_intr_coalesce_data_t     normal;
        nx_nic_intr_coalesce_data_t     low;
        nx_nic_intr_coalesce_data_t     high;
        nx_periodic_intr_t              periodic_intr;
} nx_nic_intr_coalesce_t;

typedef struct {
	nic_req_hdr_t			hdr;
	__uint32_t			ctx_id;
	__uint32_t			blink_rate;
	__uint32_t			blink_state;
        __uint32_t                      rsvd[9];
} nx_nic_led_config_t;

typedef struct {
	nic_req_hdr_t                   hdr;
	__uint32_t			loopback_flag;
	__uint32_t			rsvd[11];
} nx_nic_loopback_t;

/* P3: L2 filtering defines. 
 * This will look a little strange due to the need to
 * maintain compatibility with a pre-existing interface */
#define UNM_MAC_EVENT           1

#define UNM_MAC_NOOP            0
#define UNM_MAC_ADD             1
#define UNM_MAC_DEL             2

#define UNM_SRE_MAC_SUCCEED 1
#define UNM_SRE_MAC_FAIL    2

typedef struct {
        char    op;             /* Add, Del */
        char    tag;
        char    mac_addr[6];    /* ETH_ALEN */
} mac_request_t;

typedef struct {
	nic_req_hdr_t	req_hdr;
	mac_request_t	body;
} nx_mac_config_t;

/*
 * LRO related configuration.
 */
enum {
	NX_NIC_LRO_REQUEST_FIRST = 0,
	NX_NIC_LRO_REQUEST_ADD_FLOW,
	NX_NIC_LRO_REQUEST_DELETE_FLOW,
	NX_NIC_LRO_REQUEST_TIMER,
	NX_NIC_LRO_REQUEST_CLEANUP,
	NX_NIC_LRO_REQUEST_ADD_FLOW_SCHEDULED,	/* Used internally in the FW */
	NX_TOE_LRO_REQUEST_ADD_FLOW,
	NX_TOE_LRO_REQUEST_ADD_FLOW_RESPONSE,
	NX_TOE_LRO_REQUEST_DELETE_FLOW,
	NX_TOE_LRO_REQUEST_DELETE_FLOW_RESPONSE,
	NX_TOE_LRO_REQUEST_TIMER,
	NX_NIC_LRO_REQUEST_LAST
};
typedef struct {
	nic_req_hdr_t	req_hdr;
	ip_addr_t	daddr;
	ip_addr_t	saddr;
	__uint16_t	dport;
	__uint16_t	sport;
	__uint32_t	family : 8,	/* 0: IPv4 / 1: IPv6 */
			timestamp : 8,	/* 0: No timestamp / 1: timestamp */
			rsvd : 16;
	__uint32_t	rss_hash;
	__uint32_t	host_handle;
} nx_nic_lro_request_t;

typedef struct {
	nic_req_hdr_t	req_hdr;
	__uint64_t	normal_q_hdr;
	__uint64_t	jumbo_q_hdr;
	__uint64_t	lro_q_hdr;	/* Not supported. Use Jumbo Hdr */
	__uint8_t	jumbo_contiguous;
	__uint8_t	lro_contiguous;
	__uint16_t	jumbo_size;
	__uint8_t	pci_func;
	__uint8_t	rsvd2;
	__uint16_t	rsvd3;
	__uint64_t	rsvd4[2];
} nx_proxy_start_request_t;

typedef struct {
	nic_req_hdr_t	req_hdr;
        __uint8_t      src_peg;
        __uint8_t      rsvd1[7];
	__uint64_t	rsvd2[5];
} nx_proxy_stop_request_t;

typedef struct {
	nic_req_hdr_t           hdr;
        __uint64_t              dma_to_addr;
        __uint32_t              dma_size;
        __uint16_t              ring_ctx;
        __uint16_t              rsvd;           /* 64 bit alignment */
} nx_nic_get_stats_request_t;

typedef struct {
	nic_req_hdr_t	req_hdr;
	__uint32_t	mtu;
} nx_proxy_mtu_info_t;

#define VPORT_MISS_MODE_DROP		0 /* drop all packets */
#define VPORT_MISS_MODE_ACCEPT_ALL	1 /* accept all packets */
#define VPORT_MISS_MODE_ACCEPT_MULTI	2 /* accept only multicast packets */
typedef struct {
	nic_req_hdr_t   req_hdr;
	__uint32_t      mode;
} nx_proxy_vport_miss_mode_t;

typedef struct {
        nic_req_hdr_t   req_hdr;
	__uint8_t       P2V[4];
} nx_proxy_update_p2v_t;

/* used to send down the IP address to card */
typedef struct {
        nic_req_hdr_t   req_hdr;
	__uint64_t	cmd;
	ip_addr_t	ipaddr;	
} nx_nic_config_ipaddr_t;

typedef struct {
	nic_req_hdr_t 	req_hdr;
	__uint8_t 	notification_enable;
	__uint8_t	get_linkevent;
	__uint16_t	rsvd1;
	__uint32_t	rsvd2;
} nic_linkevent_request_t;

typedef struct {
	nic_req_hdr_t 	req_hdr;
	__uint8_t 	enable;
	__uint8_t	rsvd0;
	__uint16_t	rsvd1;
	__uint32_t	rsvd2;
} nic_config_bridging_t;

typedef struct {
	nic_req_hdr_t	req_hdr;
	__uint64_t	dma_addr;
	__uint16_t	interval; /* DMA time interval in ms */
	__uint16_t	dma_size;
	__uint8_t	periodic_dma;
	__uint8_t	stats_reset;
	__uint16_t	rsvd;
} nx_nic_get_net_stats_t;


/* MESSAGE HOST => CARD
 *   This should be the type used for all requests. Any
 *   overlays should be declared above and added below.
 *   Please don't use casting or declare request types elsewhere.
 */
typedef struct {
	union {
		struct {
			__uint64_t      dst_minor:18,
					dst_subq:1,
					dst_major:4,
					opcode:6,
					hw_rsvd_0:3,
					msginfo:24,
					hw_rsvd_1:2,
					qmsg_type:6;
		};
		__uint64_t	word;
	};

	union {
		struct {
			nic_req_hdr_t   req_hdr;
			__uint64_t      words[6];
		} cmn;

		rss_config_t			rss_config;
		rss_config_indir_tbl_t		rss_config_indir_tbl;
		nx_nic_intr_coalesce_t		nx_nic_intr_coalesce;
		nx_nic_led_config_t		nx_nic_led_config;	
		nx_nic_loopback_t		nx_nic_loopback_config;
		nx_nic_lro_request_t		lro_request;
		nx_proxy_start_request_t	proxy_start_request;
		nx_proxy_stop_request_t		proxy_stop_request;
		nx_proxy_mtu_info_t		mtu_info;
		nx_proxy_vport_miss_mode_t	vport_miss_mode;
		nx_mac_config_t			mac_config;
		nx_proxy_update_p2v_t		update_p2v;
		nx_nic_config_ipaddr_t		ipaddr_config;
		nic_linkevent_request_t		linkevent_request;
		nic_config_bridging_t           config_bridging;
		nx_nic_get_net_stats_t		stats_req;
		__uint64_t			words[7];
	} body;
} nic_request_t;





/* HEADER: CARD => HOST
 * Header of message sent between host and pegasus.
 */
typedef union {
        struct {
                __uint64_t      ctx : 32,
                                opcode : 8,
                                op_id : 8,
                                rsvd : 5,
                                desc_cnt : 3,   /* Card -> Host.
                                                   Used by NIC path */
                                owner : 2,      /* Card -> Host.
                                                   Used by NIC path */
                                qmsg_type : 6;  /* Card -> Host.
                                                   Used by NIC path */
        };

        struct {
                __uint64_t      rsvd1 : 16,
				error_code : 16,
				opcode : 8,
                                compid : 8,
                         /* !! See above !! */
                                rsvd2 : 5,
                                desc_cnt : 3,
                                owner : 2,
                                qmsg_type : 6;
        } nic;

        struct {
                __uint64_t      sid    : 16,
                                cid    : 16,
                                opcode : 8,
                                unused : 8,
                         /* !! See above !! */
                                rsvd : 5,
                                desc_cnt : 3,
                                owner : 2,
                                qmsg_type : 6;
        } iscsi;

        __uint64_t              word;
} host_peg_msg_hdr_t;


typedef struct {
        __uint64_t      type : 16,
			ctxid : 16,
			comp_id : 16,
			status : 16;
} rss_config_response_t;

typedef struct nic_msg_host_s {
        __uint32_t              response;
        __uint32_t              tag;
} nx_mac_cfg_response_t;


typedef struct {
	__uint32_t	host_handle;
	__uint32_t	rss_hash;
	ip_addr_t	daddr;
	ip_addr_t	saddr;
	__uint16_t	dport;
	__uint16_t	sport;
	__uint32_t	family : 8,	/* 0: IPv4 / 1: IPv6 */
			rsvd : 24;
} nx_nic_lro_response_t;

typedef struct {
	__uint64_t response;
} nx_nic_get_stats_response_t;

/* module types */
#define LINKEVENT_MODULE_NOT_PRESENT			1
#define LINKEVENT_MODULE_OPTICAL_UNKNOWN		2
#define LINKEVENT_MODULE_OPTICAL_SRLR			3
#define LINKEVENT_MODULE_OPTICAL_LRM			4
#define LINKEVENT_MODULE_OPTICAL_SFP_1G			5
#define LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLE	6
#define LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLELEN	7
#define LINKEVENT_MODULE_TWINAX				8

#define LINKSPEED_10GBPS		10000
#define LINKSPEED_1GBPS			1000
#define LINKSPEED_100MBPS		100
#define LINKSPEED_10MBPS		10

#define LINKSPEED_ENCODED_1GBPS		2
#define LINKSPEED_ENCODED_100MBPS	1
#define LINKSPEED_ENCODED_10MBPS	0

#define LINKEVENT_AUTONEG_DISABLED	0
#define LINKEVENT_AUTONEG_ENABLED	1

#define LINKEVENT_HALF_DUPLEX		0
#define LINKEVENT_FULL_DUPLEX		1

#define LINKEVENT_LINKSPEED_MBPS	0
#define LINKEVENT_LINKSPEED_ENCODED	1

typedef struct {
	__uint32_t	cable_OUI;
	__uint16_t	cable_len;
	__uint16_t 	link_speed;
	__uint8_t 	link_status;
	__uint8_t 	module;
	__uint8_t 	full_duplex;
	__uint8_t	link_autoneg;
	__uint32_t	rsvd2;
} nic_linkevent_response_t;

/* CARD => HOST
 *   This should be the type used for all responses. Any
 *   overlays should be declared above and added below.
 *   Please don't use casting or declare response types elsewhere.
 */
typedef struct {

	host_peg_msg_hdr_t		rsp_hdr;

	union {
		__uint64_t		word;
		rss_config_response_t	rss_config;
		nx_mac_cfg_response_t	mac_cfg;
		nx_nic_lro_response_t	lro_response;
		nic_linkevent_response_t	linkevent_response;
	} body;
} nic_response_t;




/* Request to DMA the statistics to a certain location.
 * reset_stats_request_t and get_stats_response_t defined in hostInterface.h
 */
typedef struct {
	host_peg_msg_hdr_t      hdr;
        __uint64_t              dma_to_addr;
        __uint32_t              dma_size;
        __uint16_t              ring_ctx;
        __uint16_t              rsvd;           /* 64 bit alignment */
} get_stats_request_t;


typedef struct PREALIGN(64) _RingContext
{
    /* one command ring */
    __uint64_t         CMD_CONSUMER_OFFSET;
    __uint64_t         CmdRingAddrLo:32,
                       CmdRingAddrHi:32;
    __uint32_t         CmdRingSize;
    __uint32_t         Rsrv;

    /* three receive rings */
    RcvContext         RcvContext[3];

    /* one status ring */
    __uint64_t		StsRingAddrLo:32,
			StsRingAddrHi:32;
    __uint32_t		StsRingSize;

    __uint32_t		CtxId;

    __uint64_t		D3_STATE_REGISTER;
    __uint64_t		DummyDmaAddrLo:32,
			DummyDmaAddrHi:32;

    __uint64_t		end_v1_context;	/* Used for size for DMA */
    /*
     * Additions to support RSS & MSI-X & other attributes.
     */
    __uint32_t		StsRingCount;
    __uint32_t		rsvd_2;
    struct {
	    __uint64_t	StsRingAddr;
	    __uint32_t	StsRingSize;
	    __uint32_t	msix_entry_idx;
    } StsRings[MAX_RSS_RINGS];

} POSTALIGN(64) RingContext,RING_CTX,*PRING_CTX;

typedef struct PREALIGN(64) cmdDescType0
{
   union {
	struct {
   	    __uint64_t  tcpHdrOffset:8,     // For LSO only
        		ipHdrOffset:8,      // For LSO only
        		flags:7,            // as defined above
        		opcode:6,           // This location/size must not change...
        		Unused:3,           //
        		numOfBuffers:8,     // total number of segments (buffers
                        		    // for this packet. (could be more than 4)

        		totalLength:24;     // Total size of the packet
	};
	__uint64_t	word0;
    };

    union {
        struct {
            __uint32_t AddrLowPart2;
            __uint32_t AddrHighPart2;
        };
        __uint64_t AddrBuffer2;
	__uint64_t	word1;
    };

   union {
	struct {
    	     __uint64_t referenceHandle:16, // changed to U16 to add mss
        		mss:16,             // passed by NDIS_PACKET for LSO
        		port:4, /* JAMBU: Deprecated. Remove it? */
        		//rsvd1:4,
        		ctx_id:4,
        		totalHdrLength:8,   // LSO only : MAC+IP+TCP Hdr size
        		connID:16;          // IPSec offoad only
	};
	   __uint64_t	comp_hdr;	/* Used for EPG Flush: P2P only */
	__uint64_t	word2;
    };

    union {
        struct {
            __uint32_t AddrLowPart3;
            __uint32_t AddrHighPart3;
        };
        __uint64_t AddrBuffer3;
	__uint64_t	word3;
    };

    union {
        struct {
            __uint32_t AddrLowPart1;
            __uint32_t AddrHighPart1;
        };
        __uint64_t AddrBuffer1;
	__uint64_t	word4;
    };

    union {
        struct {
    	    __uint64_t buffer1Length:16,
        		buffer2Length:16,
        		buffer3Length:16,
        		buffer4Length:16;
        };
	__uint64_t	word5;
    };

    union {
        struct {
            __uint32_t AddrLowPart4;
            __uint32_t AddrHighPart4;
        };
        __uint64_t AddrBuffer4;
	__uint64_t	word6;
    };

    union {
	__uint64_t unused;
	__uint64_t mcastAddr;
	__uint64_t comp_data;	/* Used for EPG Flush: P2P only */
    };

} POSTALIGN(64) cmdDescType0_t;


/*
 * This is the command descriptor used for moving qmessages from the host to
 * the NIC TX path and then as queue messages to the queue as specified by
 * the destination fields.
 */
typedef struct PREALIGN(64) pegnet_cmd_desc
{
    union {
        struct {
            __uint64_t      dst_minor:18,   /* The pegnet queue Minor this
                                                 * message is destined to. */
                            dst_subq:1,     /* The destination subq */
                            dst_major:4,    /* The destination Major */
                            opcode:6,       /* The opcode (PEGNET_REQUEST).
                                             * It has to be in this
                                             * location and cannot be moved. */
                            qmsg_length:3,  /* The length of the queue msg
                                             * body */
                            msginfo:24,     /* This is not used */
                            rsvd:2,
                            qmsg_type:6;    /* The actual qmsg type this
                                             * represents. This will be set in
                                             * the qmsg to the destination. */
        };
        __uint64_t      hdr;
    };

    __uint64_t      body[7];

} POSTALIGN(64) pegnet_cmd_desc_t;

#define RCV_DESC_HOST_FREE      0x01
#define RCV_DESC_HOST_POSTED    0x00
#define RCV_DESC_HOST_PEG_USED  0x04

/* Note: sizeof(rcvDesc) should always be a mutliple of 2 */
typedef struct rcvDesc
{
    __uint64_t referenceHandle:16,
               flags:16,
               bufferLength:32;        // allocated buffer length (usually 2K)
    __uint64_t AddrBuffer;
}  rcvDesc_t;

/* for status field in statusDesc_t */
#define STATUS_CKSUM_NONE				(0x1)
#define STATUS_CKSUM_OK					(0x2)
#define STATUS_CKSUM_NOT_OK				(0x3)

/* owner bits of statusDesc_t */
#define STATUS_OWNER_HOST                              (1ULL)
#define STATUS_OWNER_PHANTOM                           (2ULL)
#define HOST_STATUS_DESC          ((STATUS_OWNER_HOST) << 48)
#define PHANTOM_STATUS_DESC    ((STATUS_OWNER_PHANTOM) << 48)

#define UNM_PROT_IP                                       (1)
#define UNM_PROT_UNKNOWN                                  (0)

/* LRO specific bits of statusDesc_t */
#define LRO_LAST_FRAG                                     (1)
#define LRO_NORMAL_FRAG                                   (0)
#define LRO_LAST_FRAG_DESC              ((LRO_LAST_FRAG)<<63)
#define LRO_NORMAL_FRAG_DESC          ((LRO_NORMAL_FRAG)<<63)

/*
 * CARD --> HOST
 *
 * Send accumulated lro data to host using NIC receive buffers.
 */
typedef struct {
	__uint64_t      ref_handle:16,
			length:16,
			l2_hdr_offset:8,
			l4_hdr_offset:8,
			timestamp:1,
			type:3,		/* type/index of descriptor ring */
			psh:1,
			desc_cnt:3,     /* Set in the NIC path
					   before DMA to host */
			owner:2,        /* Set in the NIC path
					   before DMA to host */
			qmsg_type:6;	/* Qmsg or NIC rcv desc type */

	__uint32_t	seq_number;
	__uint32_t      rss_hash;
} nx_lro2_hdr_desc_t;

typedef struct {
	__uint64_t      ref_handle:16,
			length:16,
			l2_hdr_offset:8,
			data_offset:8,
			rsvd1:4,
			psh:1,
			desc_cnt:3,     /* Set in the NIC path
					   before DMA to host*/
			owner:2,        /* Set in the NIC path
					   before DMA to host*/
			qmsg_type:6;	/* Qmsg or NIC rcv desc type */

	__uint16_t	count:8,
			rsvd2:4,
			type:4;         /* type/index of descriptor ring */
	__uint8_t	l4_hdr_offset;
	__uint8_t	rsvd3;
	__uint32_t      rss_hash;
} nx_lro_hdr_desc_t;

#define NX_LRO_PKTS_PER_STATUS_DESC	4
typedef struct {
	union {
		struct {
			__uint16_t	ref_handle;
			__uint16_t	length;
		} s;
		__uint32_t	word;
	} pkts[NX_LRO_PKTS_PER_STATUS_DESC];
} nx_lro_frags_desc_t;

#define UNM_QUEUE_MSG_DESC_COUNT        5
#define NX_MAX_LRO_MSG_DESC_COUNT	6
#define	NX_SYN_PACKET_IDENTIFIER	0xffff	/* Set in the listener_id for
						   SYN packet */
#define	NX_PROT_NIC_ICMP 	0x2ULL
#define NX_PROT_MASK		0xFULL
#define NX_PROT_SHIFT_VAL(x)	(x << 44)

typedef struct PREALIGN(16) statusDesc
{
    union {
        struct {
                __uint64_t      port:4,         /* initially to be used
						 * but noe now */
				status:4,       /* completion status
						 * may not have use */
                                type:4,         /* type/index of
						 * descriptor ring */
                                totalLength:16, /* NIC mode...no use yet */
                                referenceHandle:16, /* handle for the
						     * associated packet */
				prot:4,         /* Pkt protocol */
				pkt_offset:5,  /* L2 size (bytes)  */
				descCnt:3,      /* This indicates the num of
						 * descriptors part of this
						 * descriptor chain. */
				owner:2,
				opcode:6;

		union {
			struct {
				__uint32_t	hashValue;
				__uint16_t	vlan;
				__uint8_t       hashType;
				union {
					/*
					 * For LRO count is set
					 * Last LRO fragment is set when it is
					 * the last frag as the name says.
					 */
                                        __uint8_t lro_frag:7,
					  	  last_lro_frag:1;

                                        /*
                                         * Used to indicate direction in case
                                         * of captured packets. Egress will
                                         * contain EPG input, while ingress
                                         * contains an skb copy.
                                         */
#define         NX_CAP_DIRN_OUT 1
#define         NX_CAP_DIRN_IN  2
                                        __uint8_t direction;

					/*
                                         * Currently for Legacy this is 0.
                                         */
					__uint8_t	nr_frags;
                                };
 			};
			struct {
                                __uint16_t frag_handles[4];
                        };
		};
        };
	nx_lro_hdr_desc_t	lro_hdr;
	nx_lro_frags_desc_t	lro_frags;
	nx_lro2_hdr_desc_t	lro2;
        __uint64_t		body[2];
    };

} POSTALIGN(16) statusDesc_t;


#define STATUS_OWNER_NAME(sd) \
        (((sd)->owner == STATUS_OWNER_HOST) ? "Host" : "Phantom")

#ifdef UNM_IPSECOFFLOAD

#define MAX_IPSEC_SAS            1024
#define RECEIVE_IPSEC_SA_BASE           0x8000

/*
* IPSEC related structures and defines
*/

// Values for DIrFlag in the ipsec_sa_t structure below:
#define UNM_IPSEC_SA_DIR_INBOUND    1
#define UNM_IPSEC_SA_DIR_OUTBOUND    2

// Values for Operation Field below:
#define UNM_IPSEC_SA_AUTHENTICATE    1
#define UNM_IPSEC_SA_ENDECRYPT        2

// COnfidential Algorithm Types:
#define UNM_IPSEC_CONF_NONE    0    // NULL encryption?
#define UNM_IPSEC_CONF_DES    1
#define UNM_IPSEC_CONF_RESERVED    2
#define UNM_IPSEC_CONF_3DES    3

// Integrity algorithm (AH) types:
#define UNM_IPSEC_INTEG_NONE    0
#define UNM_IPSEC_INTEG_MD5    1
#define UNM_IPSEC_INTEG_SHA1    2

#define UNM_PROTOCOL_OFFSET    0x9    // from ip header begin, in bytes
#define UNM_PKT_TYPE_AH        0x33
#define UNM_PKT_TYPE_ESP    0x32


#define UNM_AHOUTPUT_LENGTH     12      // 96 bits of output for MD5/SHA1 algorithms
#define UNM_DES_ICV_LENGTH      8       // 8 bytes (64 bits) of ICV value for each block of DES_CBC at the begin of ESP payload

#if UNM_CONF_PROCESSOR==UNM_CONF_X86

typedef struct PREALIGN(512) s_ipsec_sa {
    U32    SrcAddr;
    U32 SrcMask;
    U32 DestAddr;
    U32 DestMask;
    U32    Protocol:8,
        DirFlag:4,
        IntegCtxInit:2,
        ConfCtxInit:2,
        No_of_keys:8,
        Operation:8;
    U32    IntegAlg:8,
        IntegKeyLen:8,
        ConfAlg:8,
        ConfAlgKeyLen:8;
    U32    SAIndex;
    U32    SPI_Id;
    U64    Key1[124];
} POSTALIGN(512) unm_ipsec_sa_t;

#else

typedef struct PREALIGN(512) s_ipsec_sa {
    unm_msgword_t    SrcAddr:32,
                    SrcMask:32;
    unm_msgword_t    DestAddr:32,
                    DestMask:32;
    unm_msgword_t    Protocol:8,
                    DirFlag:4,
                    IntegCtxInit:2,
                    ConfCtxInit:2,
                    No_of_keys:8,
                    Operation:8,
                    IntegAlg:8,
                    IntegKeyLen:8,
                    ConfAlg:8,
                    ConfAlgKeyLen:8;
    unm_msgword_t    SAIndex:32,
                    SPI_Id:32;
    unm_msgword_t    Key1[124];        // to round up to 1K of structure
} POSTALIGN(512) unm_ipsec_sa_t;

#endif //NOT-X86

// Other common header formats that may be needed

typedef struct _unm_ip_header_s {
    U32    HdrVer:8,
        diffser:8,
        TotalLength:16;
    U32     ipId:16,
        flagfrag:16;
    U32    TTL:8,
        Protocol:8,
        Chksum:16;
    U32    srcaddr;
    U32    destaddr;
} unm_ip_header_t;

typedef struct _unm_ah_header_s {
    U32    NextProto:8,
        length:8,
        reserved:16;
    U32    SPI;
    U32    seqno;
    U16    ICV;
    U16    ICV1;
    U16    ICV2;
    U16    ICV3;
    U16    ICV4;
    U16    ICV5;
} unm_ah_header_t;

typedef struct _unm_esp_hdr_s {
    U32 SPI;
    U32 seqno;
} unm_esp_hdr_t;

#endif //UNM_IPSECOFFLOAD

/*
 * Defines for various loop counts. These determine the behaviour of the
 * system. The classic tradeoff between latency and throughput.
 */

/*
 * MAX_DMA_LOOPCOUNT : After how many interations do we start the dma for
 * the status descriptors.
 */
#define MAX_DMA_LOOPCOUNT    (32)

/*
 * MAX_TX_DMA_LOOP_COUNT : After how many interations do we start the dma for
 * the command descriptors.
 */
#define MAX_TX_DMA_LOOP_COUNT    1000


/*
 * XMIT_LOOP_THRESHOLD : How many times do we spin before we process the
 * transmit buffers.
 */
#define XMIT_LOOP_THRESHOLD        0x1

/*
 * XMIT_DESC_THRESHOLD : How many descriptors pending before we process
 * the descriptors.
 */
#define XMIT_DESC_THRESHOLD    0x4

/*
 * TX_DMA_THRESHOLD : When do we start the dma of the command descriptors.
 * We need these number of command descriptors, or we need to exceed the
 * loop count.   P1 only.
 */
#define TX_DMA_THRESHOLD        16

/*
 * TASKLET_DELAY : Ratio of interrupts to be ignored before scheduling
 * the tasklet. We want to process the receive interrupts quickly, but can
 * delay processing the tx interrupts.
 */
#define TASKLET_DELAY            4

#if defined(UNM_IP_FILTER)
/*
 * Commands. Must match the definitions in nic/Linux/include/unm_nic_ioctl.h
 */
enum {
    UNM_IP_FILTER_CLEAR = 1,
    UNM_IP_FILTER_ADD,
    UNM_IP_FILTER_DEL,
    UNM_IP_FILTER_SHOW
};

#define MAX_FILTER_ENTRIES        16

typedef struct {
    __int32_t count;
    __uint32_t ip_addr[15];
} unm_ip_filter_t;
#endif /* UNM_IP_FILTER */

enum {
	UNM_RCV_PEG_0 = 0,
	UNM_RCV_PEG_1
};

enum {
	NX_FW_CAPABILITY_MEM = 1 << 0,
	NX_FW_CAPABILITY_LSO = 1 << 1,
	NX_FW_CAPABILITY_LRO = 1 << 2,
	NX_FW_CAPABILITY_TOE = 1 << 3,
	NX_FW_CAPABILITY_COE = 1 << 4,	/* Chimney/Windows offload engine */
	/* f/w can send aync link event notifications */
	NX_FW_CAPABILITY_LINK_NOTIFICATION = 1 << 5,
	NX_FW_CAPABILITY_SWITCHING = 1 << 6,
	NX_FW_CAPABILITY_PEXQ = 1 << 7,
	NX_FW_CAPABILITY_BDG = 1 << 8,
	NX_FW_CAPABILITY_FVLANTX = 1 << 9,
	NX_FW_CAPABILITY_HW_LRO = 1 << 10,
	NX_FW_CAPABILITY_GBE_LINK_CFG = 1 << 11,
};


#define NX_STATS_DMA_ONE_TIME		0
#define NX_STATS_DMA_START_PERIODIC	1
#define NX_STATS_DMA_STOP_PERIODIC	2
#endif
