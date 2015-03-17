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
#ifndef __PEGNET_MSGTYPES_H_
#define __PEGNET_MSGTYPES_H_

/*
 * ====================== MESSAGE TYPE DEFINITIONS ======================
 * Define types for all messages used in the system.  Each h/w block will
 * be given a different "type" to use in messages it generates so that we
 * know just from the message header what we have just dequeued.
 * ====================== MESSAGE TYPE DEFINITIONS ======================
 */

//#define TX_RATE_LIMITING
#ifdef P4
#define NX_PORT_SHARING
#endif

/*
 * ====================== CLASSES OF MESSAGE TYPES ======================
 * 
 * A message type falls into one or more of the following classes.
 *
 * [FLOW]  PER FLOW QUEUE TYPES
 * [PERPEG] PER PEG (OR GLOBAL) TYPES
 *    The pegos scheduler uses type to index its method table.
 *    This currently includes messages in both SQG flow queues, global
 *    queues which happen to be in the same group as the flow queues,
 *    and per-peg priority queues.  If in use, message types should 
 *    almost exclusively be of this class. 
 *
 *    The purpose of the specific method can be further broken down 
 *    into one of the following catagories:
 *
 *             TOE, RDMA, iSCSI, NIC 
 *
 * [NIC] NIC QUEUE TYPES
 *    These types show up solely on the nic's input, xmit,
 *    or nic specific priority queues. These do not index
 *    the method table. However, as nic becomes integrated
 *    into find work these will become FLOW or PERPEG types. 
 *
 * [SPECIAL] SPECIAL/RESERVED TYPES
 *    These types are reserved for specific architectural reasons
 *    and should not be modified or removed for any reason.
 *
 * [HW] HARDWARE ONLY QUEUE TYPES
 *    These types are not used by software to demux messages
 *    but are instead used to interface to hardware.
 *
 * [USABLE] MASQUERADING AS QUEUE MESSAGE TYPES
 *    These types are never used to access the method table and
 *    no plans exist for them to do so. They could be overloaded
 *    but should really just be moved out  of the message type space.
 * 
 * [OVERLOADS] OVERLOADS A REAL METHOD MSG TYPE
 *    These message types overload real method table types.
 *    The non-method table type should eventually be renamed.
 *
 * [FREE] FREE TYPES                                    
 *    These types are currently unreserved and open for use.
 *
 */


/*
 * =========================== SPECIAL ========================================
 */

/* [SPECIAL]  Message type not valid */
#define UNM_MSGTYPE_UNKNOWN             0x00

/* [SPECIAL]  This message type must not be used on secondary queues: hardware
 * returns msg.hdr 0xbadbad when dequeueing from an empty queue in these
 * groups, and 0x2e maps to the msg.hdr.type.
 */
#define UNM_MSGTYPE_DO_NOT_USE          0x2e    


/*
 * ============================ HW ============================================
 */

/* [HW]  linklist fmt */
#define UNM_MSGTYPE_POINTER             0x01

/* [HW]  Sent to hw in TIMER_SETUP_MAJQ, TIMER_SETUP_MINQ */
#define UNM_MSGTYPE_TIMER_SETUP         0x09

/* [HW]  EPG packet xmit request fmt: on EPG_REQUEST<x>_MIN, where x = 1,2,3 */
#define UNM_MSGTYPE_EPG_WORK            0x0b



/*
 * ============================ FLOW / PERPEG =================================
 */

/* [FLOW TOE]  TCP/IP packet */
#define UNM_MSGTYPE_SRE_IPQ             0x05

/* [FLOW TOE]  ingress pkt linklist fmt */
#define UNM_MSGTYPE_SRE_FLOW            0x07

/* [FLOW TOE]  which timer expired fmt */
#define UNM_MSGTYPE_TIMER_EVENT         0x0a

/* [FLOW TOE]  req/resp to clear tuple */
#define UNM_MSGTYPE_CLEAR_TUPLE		0x0c

/* [FLOW TOE]  Connect request */
#define UNM_MSGTYPE_CONNECT_REQUEST     0x0f

/* [FLOW TOE]  Release request from host */
#define UNM_MSGTYPE_RELEASE_REQUEST     0x10

/* [PERPEG TOE: PEG<x>_PRIORITY_QMINOR]  License, stats to host */
#define UNM_MSGTYPE_ASYNC_DMA_COMPLETE	0x13

/* [PERPEG TOE: ?]  Interrupt from I2Q. */
#define UNM_MSGTYPE_I2Q_INTERRUPT       0x14

/* FCOE reserved */
#define UNM_MSGTYPE_FCOE_RSVD1	0x17

/* [PERPEG TOE: ?]  License, route, neighbour. */
#define UNM_MSGTYPE_MCAST_RPC           0x19

/* [NIC TX] [PERPEG TOE: ?]  This routine is both called by nic Tx and forwarded
   for chimney DMA completion processing on peg0. */
#define UNM_MSGTYPE_INITIATE_OFFLOAD    0x1a

/* Get rid of this after chimney driver changes are in */
#define UNM_MSGTYPE_RESET_STATISTICS	0x1b 
/* FCOE reserved */
//#define UNM_MSGTYPE_FCOE_RSVD2  	0x1b

#define UNM_MSGTYPE_FCOE_RSVD3  	0x1c

/* [FLOW TOE]  TOE: Out of band control messages fir flow. Right now only 
   used by Chimney to send receive window updates to card */
#define UNM_MSGTYPE_FLOW_CONTROL  0x1e

/* [PERPEG TOE: PEG<x>_PRIORITY_QMINOR]  SRE DMA completion. */
#define UNM_MSGTYPE_TCP_DMA_COMP        0x20

/* [FLOW TOE]  CHIMNEY */
#define UNM_MSGTYPE_DISCONNECT_OFFLOAD  0x21

/* [FLOW TOE]  Sent from the listener PEG to the target PEG. This
   triggers the SYN-ACK packet to be sent. */
#define UNM_MSGTYPE_DO_3WAY_HANDSHAKE   0x23

/* [FLOW TOE]  Trigger cleanup on child */
#define UNM_MSGTYPE_CHILD_RELEASE       0x24

/* [FLOW TOE]  New socket creation */
#define UNM_MSGTYPE_INSTANCE_SOCKET     0x25

/* [FLOW TOE]  CHIMNEY */
#define UNM_MSGTYPE_FORWARD_OFFLOAD     0x26

/* [FLOW TOE]  Aggregate epg. */
#define UNM_MSGTYPE_EPG_AGGR_COMP       0x27

/* [FLOW TOE], [PERPEG TOE: peg0 PEGOS_PEGNET_CORE] 
   Request from host to card */
#define UNM_MSGTYPE_HOST_REQUEST        0x29

/* [PERPEG TOE: ?]  Signaled at low Q message space */
#define UNM_MSGTYPE_RESOURCE_ALARM      0x2b

/* [FLOW TOE]  This is sent from the HOST to the listener socket on 
   the CARD for starting the offload.*/
#define UNM_MSGTYPE_RXED_SYN_OFFLOAD	0x2d 

/* [FLOW TOE]  CHIMNEY */
#define UNM_MSGTYPE_TERMINATE_OFFLOAD   0x2f

/* [PERPEG TOE: peg0 PEGOS_PEGNET_CORE] Create new socket */
#define UNM_MSGTYPE_CREATE_REQUEST      0x31

/* [FLOW TOE]  Used in zero copy. To switch a buffer in the TCB from 
   zero copy to regular copy buffer. */
#define UNM_MSGTYPE_TXBUFFER_SWITCH	0x35

/* [FLOW TOE]  Receive dma to host is complete. */
#define UNM_MSGTYPE_RXBUFFER_DMA_RDY    0x36

/* [FLOW TOE]  Post filled send buffer to card. */
#define UNM_MSGTYPE_SENDMSG             0x38

/* [FLOW TOE]  CHIMNEY */
#define UNM_MSGTYPE_TOE_DMA_COMPLETE	0x3b

/* [FLOW TOE]   Error packet */
#define UNM_MSGTYPE_DATA_ERROR          0x33

/* TOE LRO. */
#define UNM_MSGTYPE_TOE_LRO_RCV         0x28

#ifdef NX_FCOE_SUPPORT1

/*
 * ============================= FCOE ===========================
 */
/* [FCOE] Tx: Data XDMA complete */
#define UNM_MSGTYPE_FCOE_XDMA_COMPLETE		0x03

/* [FCOE] Tx: Post SCSI buffer for zero copy */
/* waiting for a free msg type */
#define UNM_FCOE_INPUT_MSGTYPE_POST_BUFF	0x3f

/* [FCOE] Tx: CRC done */
#define UNM_MSGTYPE_FCOE_CRC_COMPLETE		0x04

/* [FCOE] Tx: CRC_DMA complete */
#define UNM_MSGTYPE_FCOE_CRC_DMA_COMPLETE	0x17

/* [FCOE] Rx: FCOE msg from SRE */
#define UNM_FCOE_INPUT_MSGTYPE_PKT		0x30

/* [FCOE] Rx: CRC done */
/*
 * CRC engine cannot output two different msg-types
 * for two different flows (tx, rx), hence we have no
 * option but to use same message type between Tx and
 * Rx path.
 */
#define UNM_FCOE_INPUT_MSGTYPE_RX_CRC_OUT	UNM_MSGTYPE_FCOE_CRC_COMPLETE

/* [FCOE] Rx: DMA complete */
/* waiting for a free msg type */
#define UNM_FCOE_INPUT_MSGTYPE_RX_DMA_COMPL	0x39

#endif /* NX_FCOE_SUPPORT */

/*
 * ============================= ISCSI ========================================
 */

/* [FLOW iSCSI]  (Net yet in use, reserved for P3) P3 Rx PDU Hdr Msg */
#define UNM_MSGTYPE_ISCSI_PDU_HDR 	0x1d

/* TODO: GET RID OF ME. I'M USED ELSEWHERE. */
/* [FLOW iSCSI]  SCSI Command  */
#define UNM_MSGTYPE_ISCSI_SCSI_CMD 	0x28

/* [FLOW iSCSI]  Conn level tasks/resp */
#define UNM_MSGTYPE_ISCSI_CONNECTION    0x30

/*
 * ============================= RDMA =========================================
 */

/* [FLOW RDMA]  Incoming MPA Packets */
#define UNM_MSGTYPE_SREMPA              0x3c

/* [FLOW RDMA]  Connection migration */
#define UNM_MSGTYPE_CONNMOVE            0x3d

/*
 * ============================= NIC ==========================================
 */

/* [NIC: input Q]  Normal nic pkt buffer */
#define UNM_MSGTYPE_SRE_FREE            0x02

/* [NIC: input Q]  Alien packet */
#define UNM_MSGTYPE_SRE_EXP             0x06

/* [NIC: input Q]  For TOE packet capture support */
#define UNM_MSGTYPE_CAPTURE             0x08

/* [NIC: input Q]  Tx request for interrupt; Rx interrupt reminder */
#define UNM_MSGTYPE_INTERRUPT           0x32

/* [NIC: priority Q]  RX peg DMA completion */
#define UNM_MSGTYPE_NIC_DMA_COMPL	0x0e

/* [NIC: priority Q]  RX peg XDMA completion */
#define UNM_MSGTYPE_NIC_XDMA_COMPL	0x18

/* [NIC: input Q]  NIC's LRO packets (reserved for future use) */
#define UNM_MSGTYPE_NIC_LRO             0x11

/* [NIC: priority Q]  NIC's Legacy LRO config (add, cleanup, etc) */
#define UNM_MSGTYPE_NIC_PEGS_CONFIG	0x34

/* [NIC: input Q]  IP fragements go here */
#define UNM_MSGTYPE_SRE_FRAG            0x12

/* [PERPEG NIC: PEG0_PRIORITY_QMINOR]  Free SRE buf req from nic Rx to peg 0 */
#define UNM_MSGTYPE_NIC_FREE_SREBUF     0x15

/* [NIC: input Q] NIC Command from host to card or within card. */
#define UNM_MSGTYPE_NIC_REQUEST		0x1f

/* [NIC: input Q]  Response from card to host */

/* CAVEAT: Don't directly use unm_qm_enqueue() to generate
 * UNM_MSGTYPE_CARD_RESPONSE messages. Instead use send_host_msg(),
 * send_short_host_msg(), send_long_host_msg() to generate these
 * messages. This ensures that all card responses are consistently
 * queued on to PEGNET_HOST_RX_MAJOR, PEGNET_HOST_RX_MINOR,
 * PEGNET_HOST_RX_SUBQ.
 */

#define UNM_MSGTYPE_CARD_RESPONSE       0x2a

/* Overloading with UNM_MSGTYPE_TIMER_SETUP. This is used to
   free memory after DMA completion in Upload/Query */
#define UNM_MSGTYPE_UPLOAD_COMPLETION	0x09

/* [NIC: xmit compl (fold into NIC_DMA_COMPL)] DMA completions in the 
   NIC Tx path. */
#define UNM_MSGTYPE_DMA_UNIT            0x3a

/*
 * Reserved for future NIC developement
 */
#define UNM_MSGTYPE_NIC_RESERVED1	0x2c
#define UNM_MSGTYPE_NIC_RESERVED2	0x22
#define UNM_MSGTYPE_NIC_RESERVED3       0x39
#define UNM_MSGTYPE_NIC_RESERVED4	0x3e
#define UNM_MSGTYPE_NIC_RESERVED5       0x3f

/* ARP, ICMP, FRAG and DATA ERROR messages are handled both in NIC and */
/* LSA. First 3 messages are forwarded from NIC to LSA path while the  */
/* last one is forwarded from LSA to NIC path. Since all ingress sre   */
/* messages are dispatched out of flow queues purely based on message  */
/* type, NIC/LSA handlers fix the message type to that expected by the */
/* target handler before forwarding it.                                */

#define UNM_MSGTYPE_NIC_SRE_ARP         UNM_MSGTYPE_NIC_RESERVED1
#define UNM_MSGTYPE_NIC_SRE_ICMP        UNM_MSGTYPE_NIC_RESERVED2
#define UNM_MSGTYPE_NIC_SRE_FRAG        UNM_MSGTYPE_NIC_RESERVED3
#define UNM_MSGTYPE_NIC_DATA_ERROR      UNM_MSGTYPE_NIC_RESERVED4
#define UNM_MSGTYPE_NIC_IPV6_RCV        UNM_MSGTYPE_NIC_RESERVED5

#define UNM_MSGTYPE_NIC_PHY_EVENT	UNM_MSGTYPE_SINGLE_DMA_REQUEST

/*
 * [NIC: input Q]  Programmed the SRE for FBQ. Will reach Nic input queue.
 * Legacy NIC will use this type for RE1 FBQ messages
 */
#define UNM_MSGTYPE_NIC_RE1_FBQ		0x37

/*
 * [OVERLOADED: UNM_MSGTYPE_NCSI_WORK] PegOS memory mgmt: only shows up on
 * malloc queues UNM_MSGQ_PEGOS_MMGMT_QX_MINOR and
 * UNM_MSGQ_PEGOS_MMGMT_BU_QX_MINOR 
 * (X = 0, 1, 2, 3).
 */
#define UNM_MSGTYPE_PEGOS_MEM           0x0d

/* 
 * [OVERLOADED: UNM_MSGTYPE_PEGOS_MEM]
 * Special NCSI specific type in findwork.
 */
#define UNM_MSGTYPE_NCSI_WORK		0x0d

/*
 * ============================== USABLE ======================================
 */
/* [USABLE]  Evil reminant of deprecated card-host dma used by xmemcpy etc.
   Should be completely removed since this could end up on a flow
   queue and function incorrectly.
   This is reused for nic phy event as UNM_MSGTYPE_NIC_PHY_EVENT */
#define UNM_MSGTYPE_SINGLE_DMA_REQUEST  0x16

/* [USABLE]  DMA watchdog reply dummy queue: only shows up on DMA_WD_REPLY_MIN.
   HW Bug workaround to unhang the DMA engines. 
   [OVERLOADED: UNM_MSGTYPE_NIC_RESERVED1]
   This isn't on flow queue and, additionally, will hopefully disappear. */

#ifdef UNM_HWBUG_DMA_HANG 
#define UNM_MSGTYPE_DMA_WD_DMA_COMP     0x2c
#endif

/*
 * ================================= FREE ====================================
 */


/*
 * ============================== OVERLOADED =================================
 */

/* [OVERLOADS: UNM_MSGTYPE_NIC_LRO_CONFIG]  Non-findwork. Purely for use on
 * Major 8 receive buffer hdrs
 * Post empty rx data buffers to card / send full rx data buffers to host. */
#define UNM_MSGTYPE_RXBUFFER            0x34

/* [OVERLOADS: UNM_MSGTYPE_NIC_RE1_FBQ]  Purely for use on Major 8 receive
 * buffer hdrs Chimney referencing private buffers. */
#define UNM_MSGTYPE_GLOBAL_RXBUFF       0x37


/* [OVERLOADS: UNM_MSGTYPE_POINTER]
   Used by chimney to send rx buffers using nic receive descriptors */
#define UNM_MSGTYPE_TNIC_GLOBAL_BUFFERS    0x01

/* [OVERLOADS: UNM_MSGTYPE_SRE_ARP] 
   This is a status descriptor opcode formed by nic Rx and
   sent to the host. This is a SYN packet that matched a Listener socket 
   and is a candidate for offload. Sent from the card to the host. */
#define UNM_MSGTYPE_NIC_SYN_OFFLOAD	0x03 
#define UNM_MSGTYPE_NIC_SYN_OFFLOAD_V34	0x3e		// for FW v3.4 

/* [OVERLOADS: UNM_MSGTYPE_SRE_ICMP]  
   This is a status descriptor opcode formed by nic Rx and
   sent to the host. It is an opcode to the host to specify that this is 
   a true RX descriptor. Queue messages to host from card arrive as 
   descriptors. The host uses this value to figure out if it is a queue
   message or a RX descriptor.*/
#define UNM_MSGTYPE_NIC_RXPKT_DESC      0x04 
#define UNM_MSGTYPE_NIC_RXPKT_DESC_V34  0x3f 		// for FW v3.4

/* [OVERLOADS: UNM_MSGTYPE_NIC_LRO]
 * This is a status descriptor opcode for LRO chained packets. This is only
 * in the status descriptor and can be overloaded with HW queue values.
 */
#define UNM_MSGTYPE_NIC_LRO_CHAINED     0x11

/* [OVERLOADS: UNM_MSGTYPE_SRE_FRAG]
 * This is a status descriptor opcode for LRO contiugous buffer. This is only
 * in the status descriptor and can be overloaded with HW queue values.
 */
#define UNM_MSGTYPE_NIC_LRO_CONTIGUOUS  0x12

/* [OVERLOADS: UNM_MSGTYPE_INTERRUPT]  
   This is a status descriptor opcode used to mark 
   mac updates to the driver (P3). This might be better 
   folded into the NIC_RESPONSE below. */
#define UNM_MSGTYPE_CARD_MAC_RESPONSE   0x32

/* [OVERLOADS: UNM_MSGTYPE_SRE_IPQ]
   This is a status descriptor opcode used to mark NIC related non-packet 
   response back to the the driver. */
#define UNM_MSGTYPE_NIC_RESPONSE        0x05

#ifdef P3_LEGACY
/* [OVERLOADS: UNM_MSGTYPE_SRE_FRAG]
 *
 */
#define UNM_MSGTYPE_RE1_L2Q		UNM_MSGTYPE_SRE_FRAG

#endif

#define PEGNET_TCP_CONN_MAJOR   12
#define PEGNET_TCP_CONN_MINOR   pegos_qid_requests()

#define UNM_MSGQ_HDR_LO32(HDR)          ((void *)&(((__uint32_t *)HDR)[0]))
#define UNM_MSGQ_HDR_HI32(HDR)          ((void *)&(((__uint32_t *)HDR)[1]))

/* A generic macro to create a  queue header */
#if UNM_CONF_PROCESSOR == UNM_CONF_X86  /* we're on an x86 host */
#define UNM_QHDR_CREATE(HDR,MAJOR,MINOR,SUBQ,TYPE) \
        (memset((char *)&(HDR), 0, sizeof(HDR)),        \
         UNM_MSGQ_ADDR(HDR,dst,0,MAJOR,SUBQ,MINOR),        \
         (HDR).type=UNM_MSGTYPE_##TYPE)

#else

/* let the preprocessor do the dirty work for us */
#define UNM_QHDR_CREATE(HDR, MAJOR, MINOR, SUBQ, TYPE)		\
do {								\
	(HDR).word = ((__uint64_t)(MAJOR) << 19 ) |		\
		     (__uint64_t)(SUBQ) << 18     |		\
		     (__uint64_t)(MINOR)          |		\
		     ((__uint64_t)(UNM_MSGTYPE_##TYPE) << 58);	\
} while (0)
#endif

/* Legacy header creation */
#define UNM_MSGQ_HDR(HDR,MAJOR,MINOR,TYPE) \
    UNM_QHDR_CREATE((HDR),(MAJOR),(MINOR),1,TYPE)

/* Receive global buffer queue.  Currently unused. */
#define UNM_MSGQ_GLOBAL_RXBUFF_MAJ      0
#define UNM_MSGQ_GLOBAL_RXBUFF_MIN      13
#define UNM_MSGQ_GLOBAL_RXBUFF_Q_LEN    256
#define UNM_MSGQ_GLOBAL_RXBUFF_HDR(HDR) UNM_MSGQ_HDR(HDR,                   \
                                                UNM_MSGQ_GLOBAL_RXBUFF_MAJ, \
                                                UNM_MSGQ_GLOBAL_RXBUFF_MIN, \
                                                RXBUFFER)

/* We will reserve 10 type 1 queues for HW blocks */
#define UNM_MSGQ_TYPE1_FIRST_AVAIL 10

/* This is the major queue number that type 1 queues hide behind */
#define UNM_MSGQ_SQM_TYPE1   12
#define NX_QM_TYPE1_MAJQ UNM_MSGQ_SQM_TYPE1  /* the type 1 queue is behind maj q 12 */
#define NX_SQM_SEGNUM(A)           ((A) >> 16) /* 64K per segment */


/* Flow queues reserved for Non-LSA flows*/
#define PEGOS_MAX_OBJECTS 64

/* Queue Id of the last Non-LSA flow */
#define PEGOS_QID_FIRSTALLOC (UNM_MSGQ_TYPE1_FIRST_AVAIL + PEGOS_MAX_OBJECTS)

/* Flow queues for NIC */
#define NIC_FLOWQ_MAX_PER_PEG	32
#define NIC_FLOWQ_MIN_LAST      (PEGOS_QID_FIRSTALLOC)
#define NIC_FLOWQ_MIN_FIRST     (NIC_FLOWQ_MIN_LAST -       \
                                 NIC_FLOWQ_MAX_PER_PEG + 1)

#define NIC_FLOWQ_SUBQ          1
#define NIC_FLOWQ_MAJ           UNM_MSGQ_SQM_TYPE1 
#define NIC_FLOWQ_MIN(peg,idx)  (((peg) << 16) + NIC_FLOWQ_MIN_FIRST + (idx))

#define NIC_PKTQ_SUBQ           NIC_FLOWQ_SUBQ
#define NIC_PKTQ_MAJ            NIC_FLOWQ_MAJ   
#define NIC_PKTQ_MIN            NIC_FLOWQ_MIN(0, 1)

#define NIC_CRCQ_MIN            NIC_FLOWQ_MIN(2, 1)

/*
 * Well - known object queues
 */
#define PEGOS_PEGNET_CORE	(UNM_MSGQ_TYPE1_FIRST_AVAIL+3)

/*
** Messages from the host to the card's network stack go through
** the following [major, minor] pegos queue.
*/
#define PEGNET_H2C_MAJOR        UNM_MSGQ_SQM_TYPE1
#define PEGNET_H2C_MINOR        PEGOS_PEGNET_CORE

#endif /* __PEGNET_MSGTYPES_H_ */
