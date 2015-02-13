/* bnx2x_plugin_hw.h: Broadcom bnx2x NPA plugin
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
#ifndef __BNX2X_PLUGIN_HW__
#define __BNX2X_PLUGIN_HW__

#define PXP_VF_ADDR_USDM_QUEUES_START	0x3000

#define PXP_VF_ADDR_USDM_QUEUES_SIZE\
	(PXP_VF_ADRR_NUM_QUEUES * PXP_ADDR_QUEUE_SIZE)

#define PXP_VF_ADDR_USDM_QUEUES_END\
	((PXP_VF_ADDR_USDM_QUEUES_START) + (PXP_VF_ADDR_USDM_QUEUES_SIZE) - 1)

#define PXP_VF_ADDR_CSDM_QUEUES_START	0x4100

#define PXP_VF_ADDR_CSDM_QUEUES_SIZE\
	(PXP_VF_ADRR_NUM_QUEUES * PXP_ADDR_QUEUE_SIZE)

#define PXP_VF_ADDR_CSDM_QUEUES_END\
	((PXP_VF_ADDR_CSDM_QUEUES_START) + (PXP_VF_ADDR_CSDM_QUEUES_SIZE) - 1)

#define PXP_VF_ADDR_XSDM_QUEUES_START	0x5200

#define PXP_VF_ADDR_XSDM_QUEUES_SIZE\
	(PXP_VF_ADRR_NUM_QUEUES * PXP_ADDR_QUEUE_SIZE)

#define PXP_VF_ADDR_XSDM_QUEUES_END\
	((PXP_VF_ADDR_XSDM_QUEUES_START) + (PXP_VF_ADDR_XSDM_QUEUES_SIZE) - 1)

#define PXP_VF_ADDR_TSDM_QUEUES_START	0x6300
#define PXP_VF_ADDR_TSDM_QUEUES_SIZE\
	(PXP_VF_ADRR_NUM_QUEUES * PXP_ADDR_QUEUE_SIZE)
#define PXP_VF_ADDR_TSDM_QUEUES_END\
	((PXP_VF_ADDR_TSDM_QUEUES_START) + (PXP_VF_ADDR_TSDM_QUEUES_SIZE) - 1)

#define PRS_FLAG_OVERETH_UNKNOWN	0
#define PRS_FLAG_OVERETH_IPV4		1
#define PRS_FLAG_OVERETH_IPV6		2

#define PRS_FLAG_OVERIP_UNKNOWN		0
#define PRS_FLAG_OVERIP_TCP		1
#define PRS_FLAG_OVERIP_UDP		2

/* IGU - status block ACK */
#define BAR_IGU  			0x0000
#define VF_BAR_DOORBELL_OFFSET      	0x7C00
#define BAR_DOORBELL_STRIDE         	0x80
#define BAR_DOORBELL_TYPE_OFF       	0x40

#define IGU_SEG_ACCESS_NORM		0
#define IGU_SEG_ACCESS_DEF		1
#define IGU_SEG_ACCESS_ATTN		2

/* FID (if VF - [6] = 0; [5:0] = VF number; if PF - [6] = 1; [5:2] = 0; [1:0] = PF number) */
#define IGU_FID_ENCODE_IS_PF        	(0x1<<6)
#define IGU_FID_ENCODE_IS_PF_SHIFT  	6
#define IGU_FID_VF_NUM_MASK         	(0x3f)
#define IGU_FID_PF_NUM_MASK         	(0x3)

#define IGU_CTRL_CMD_TYPE_WR        	1
#define IGU_CTRL_CMD_TYPE_RD        	0
#define IGU_CMD_INT_ACK_BASE		0x0400
#define IGU_INT_ENABLE			0
#define IGU_INT_DISABLE			1
#define IGU_INT_NOP			2


/* Ethernet Ring parameters */
#define X_ETH_LOCAL_RING_SIZE       	13

/*Tx params*/
#define X_ETH_NO_VLAN		        0
#define X_ETH_OUTBAND_VLAN	        1
#define X_ETH_INBAND_VLAN	        2

/* The FW will pad the buffer with this value,
so the IP header will be align to 4 Byte */
#define IP_HEADER_ALIGNMENT_PADDING	2

/* Maximal aggregation queues supported for Vf queues */
#define ETH_MAX_AGG_QUEUES          	8

#define UNKNOWN_ADDRESS 		0
#define UNICAST_ADDRESS 		1
#define MULTICAST_ADDRESS 		2
#define BROADCAST_ADDRESS 		3


/*
 * RSS
 */

/* ETH RSS modes */
#define ETH_RSS_MODE_DISABLED       	0
#define ETH_RSS_MODE_REGULAR        	1
#define ETH_RSS_MODE_VLAN_PRI       	2
#define ETH_RSS_MODE_E1HOV_PRI      	3
#define ETH_RSS_MODE_IP_DSCP        	4

/* RSS hash types */
#define DEFAULT_HASH_TYPE           	0
#define IPV4_HASH_TYPE              	1
#define TCP_IPV4_HASH_TYPE          	2
#define IPV6_HASH_TYPE              	3
#define TCP_IPV6_HASH_TYPE          	4
#define VLAN_PRI_HASH_TYPE          	5
#define E1HOV_PRI_HASH_TYPE         	6
#define DSCP_HASH_TYPE              	7

#define T_ETH_INDIRECTION_TABLE_SIZE 	128

/*
 * Host coalescing constants
 */

/* used by the driver to get the SB offset */
#define USTORM_ID 			0
#define CSTORM_ID 			1
#define XSTORM_ID 			2
#define TSTORM_ID 			3
#define ATTENTION_ID 			4

/* index values - which counter to update */
#define HC_INDEX_U_ETH_RX_CQ_CONS 	1
#define HC_INDEX_C_ETH_TX_CQ_CONS 	5

/* values for RX ETH CQE type field */
#define RX_ETH_CQE_TYPE_ETH_FASTPATH 	0
#define RX_ETH_CQE_TYPE_ETH_RAMROD 	1

/* Number of indices per SB */
#define HC_SB_MAX_INDICES_E1X		8  /* Multiple of 4 */
#define HC_SB_MAX_INDICES_E2		8  /* Multiple of 4 */
#define HC_SB_MAX_INDICES		8  /* The Maximum of all */

/* Number of SB */
#define HC_SB_MAX_SB_E1X		32
#define HC_SB_MAX_SB_E2			136 /* include PF */
#define HC_REGULAR_SEGMENT 0
#define HC_DEFAULT_SEGMENT 1
#define HC_SB_MAX_SM			2 /* Fixed */
#define SB_SEG_U 0
#define SB_SEG_C 1

#define CHIP_NUM(bp)			(bp->chip_id >> 16)
#define CHIP_NUM_57710			0x164e
#define CHIP_NUM_57711			0x164f
#define CHIP_NUM_57711E			0x1650
#define CHIP_NUM_57712			0x1662
#define CHIP_NUM_57712E			0x1663
#define CHIP_NUM_57713			0x1651
#define CHIP_NUM_57713E			0x1652
#define CHIP_IS_E1(bp)			(CHIP_NUM(bp) == CHIP_NUM_57710)
#define CHIP_IS_57711(bp)		(CHIP_NUM(bp) == CHIP_NUM_57711)
#define CHIP_IS_57711E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57711E)
#define CHIP_IS_57712(bp)		(CHIP_NUM(bp) == CHIP_NUM_57712)
#define CHIP_IS_57712E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57712E)
#define CHIP_IS_57713(bp)		(CHIP_NUM(bp) == CHIP_NUM_57713)
#define CHIP_IS_57713E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57713E)
#define CHIP_IS_E1H(bp)			(CHIP_IS_57711(bp) || \
					CHIP_IS_57711E(bp))
#define IS_E1H_OFFSET			CHIP_IS_E1H(bp)
#define CHIP_IS_E2(bp)			(CHIP_IS_57712(bp) || \
					CHIP_IS_57712E(bp) || \
					CHIP_IS_57713(bp)  || \
					CHIP_IS_57713E(bp))

#define CHIP_REV(bp)			(bp->common.chip_id & 0x0000f000)
#define CHIP_REV_Ax			0x00000000

#endif /* __BNX2X_PLUGIN_HW__  */
