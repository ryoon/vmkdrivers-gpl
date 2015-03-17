/*
 * Define all base addresses and sizes/counts of things that occupy
 * pre-allocated memory or live at fixed addresses.
 */
#ifndef __UNM_ADDRMAP_H
#define __UNM_ADDRMAP_H

#if 0
#include <hal/arch/hwaddrs_asic.h>
#include <hal/arch/phantom_defines.h>
#include <hal/arch/buglist.h>
#endif

#include <unm_inc.h>
#include <pegnet_msgtypes.h>
#include <unm_nic_config.h>



/***************************** MEMPORT ASSIGNMENTS ***************************
 *
 *
 ****************************************************************************/
/* MEMPORT 1 defines */
#define NX_SDS_PRIMARY_MEMPORT          1

/* MEMPORT 2 defines */
#define TCP_EPG_HEADER_MEMPORT          2
#define TCP_EPG_HEADER_P1D_TAG          3

/* MEMPORT 3 defines */

/* MEMPORT 4 defines */
/***************************** MEMPORT DONE *********************************
 *
 *
 ****************************************************************************/

#define NUM_PEGNET_PEGS            4

///////////////////////////////////////////////
/* This defines the max number of pegnet connections this address
 * map will support.
 */
#define MAX_CONNECTIONS_PER_PEG_SHIFT   11
#define MAX_CONNECTIONS_PER_PEG    (1 << MAX_CONNECTIONS_PER_PEG_SHIFT)
#define MAX_CONNECTIONS            (MAX_CONNECTIONS_PER_PEG*NUM_PEGNET_PEGS)
////////////////////////////////////////////////////

#define MAX_TW_CONN_PER_PEG        512

/*
 * Maximum number of pegnet pegs
 */
#define NUM_PEGS	4

/* Includes all pegs. */
#define NUM_ACTIVE_PEGS         5
#define ACTIVE_PEG_MASK		((1<<NUM_ACTIVE_PEGS)-1)

/*
 * Delay queues - which side to place this.
 */
#define DELAY_Q_SIDE		0

/* SQG2 queues and SQG3 queues minor numbers should not
   intersect so as to not confuse findork */
#define UNM_ADDR_SQG2_NUM_TABOO_QUEUES  MAX_CONNECTIONS_PER_PEG

/* NOTE: The translation for X86 PCI address is done in the xport module now.
 * This is necessary to support DDR and QDR windowing (just like CRB windowing)
 */
#define	UNM_ADDR_NATIVE_TO_PCI(A)	(A)
#define	UNM_ADDR_PCI_TO_NATIVE(A)	(A)

#define UNM_ADDR_RANGE_CODE_4MB   1
#define UNM_ADDR_RANGE_CODE_8MB   2
#define UNM_ADDR_RANGE_CODE_16MB  3
#define UNM_ADDR_RANGE_CODE_32MB  4
#define UNM_ADDR_RANGE_CODE_64MB  5
#define UNM_ADDR_RANGE_CODE_128MB 6
#define UNM_ADDR_RANGE_CODE_256MB 7
#define UNM_ADDR_RANGE_CODE_512MB 8
#define UNM_ADDR_RANGE_CODE_1GB   9
#define UNM_ADDR_RANGE_CODE_2GB   10
#define UNM_ADDR_RANGE_CODE_4GB   11
#define UNM_ADDR_RANGE_CODE_8GB   12
#define UNM_ADDR_RANGE_CODE_16GB  13
#define UNM_ADDR_RANGE_CODE_32GB  14

#define UNM_UMMQ_MIN_BASE               UNM_ADDR_SQG2_NUM_TABOO_QUEUES
#define UNM_UMMQ_MIN                    UNM_UMMQ_MIN_BASE

/*
 * RDMA QDR usage.
 */
#define	RDMA_BUCKET_SHIFT	8
#define	RDMA_TUPLES_SIZE	(1 << (RDMA_BUCKET_SHIFT + 5))

/*
 * NIC LRO QDR usage.
 */
#define MAX_LRO_FLOWS                                (256)
#define NIC_TUPLE_SIZE             (UNM_ADDR_TUPLEBUF_UNIT)
#define NIC_LRO_TUPLES_SIZE  (MAX_LRO_FLOWS*NIC_TUPLE_SIZE)

#define NX_RX_STAGE1_MAJ     0
#define NX_RX_STAGE1_MIN     10
#define NX_RX_STAGE1_SUBQ    1

#define UNM_LRO_INPUT_MAJ	NX_RX_STAGE1_MAJ
#define UNM_LRO_INPUT_MIN	NX_RX_STAGE1_MIN
#define UNM_LRO_INPUT_SUBQ	NX_RX_STAGE1_SUBQ

//#define UNM_NIC_INPUT_Q_LEN 2048

#define NX_RX_STAGE1_Q_LEN   1536
#define UNM_NIC_INPUT_Q_LEN  512

/* size of a QM message (see message.h) */
#define	UNM_ADDR_SQMBUF_UNIT	UNM_MESSAGE_SIZE

/* size of msg ptr */
#define	UNM_ADDR_SQMPTR_UNIT	(8)

/*
 * ====================== DDR MEMORY MAP ======================
 * Map of all users of DDR.
 * ====================== DDR MEMORY MAP ======================
 */
#define	UNM_ADDR_DDR_0_BASE	UNM_ADDR_DDR_NET
#define	UNM_ADDR_DDR_0_SIZE (16*1024*1024) /* reserve space for code OBSOLETE */
/* DDR_1_BASE: SRE Buffers */
#define	UNM_ADDR_DDR_1_BASE	(UNM_ADDR_DDR_0_BASE + UNM_ADDR_DDR_0_SIZE)

#define	UNM_ADDR_DDR_1_SIZE	UNM_TOTAL_MN_MBUF_SIZE

/* SQG2 Message Buffers. */
#define	UNM_ADDR_DDR_5_BASE	(UNM_ADDR_DDR_1_BASE + UNM_ADDR_DDR_1_SIZE)
#define	UNM_ADDR_DDR_5_SIZE     (UNM_ADDR_SQG2BUF_SIZE)

/* SQG3 Message Buffers. */
#define	UNM_ADDR_DDR_7_BASE	(UNM_ADDR_DDR_5_BASE + UNM_ADDR_DDR_5_SIZE)
#define	UNM_ADDR_DDR_7_SIZE     (UNM_ADDR_SQG3BUF_SIZE)
/* Timer state tables */
#define	UNM_ADDR_DDR_8_BASE	(UNM_ADDR_DDR_7_BASE + UNM_ADDR_DDR_7_SIZE)
#define	UNM_ADDR_DDR_8_SIZE	UNM_ADDR_TIMERST_SIZE
/* SQG2 Hidden Message Ptrs. */
#define UNM_ADDR_DDR_9_BASE     (UNM_ADDR_DDR_8_BASE + UNM_ADDR_DDR_8_SIZE)
#define UNM_ADDR_DDR_9_SIZE     UNM_ADDR_SQG2PTR_SIZE
/* SQG3 Hidden Message ptrs */
#define UNM_ADDR_DDR_10_BASE    (UNM_ADDR_DDR_9_BASE + UNM_ADDR_DDR_9_SIZE)
#define UNM_ADDR_DDR_10_SIZE    UNM_ADDR_SQG3PTR_SIZE
/* Sqm state tables */
#define UNM_ADDR_DDR_11_BASE    (UNM_ADDR_DDR_10_BASE + UNM_ADDR_DDR_10_SIZE)
#define UNM_ADDR_DDR_11_SIZE    UNM_ADDR_SQMSTAT_SIZE

#define UNM_ADDR_DDR_END        (UNM_ADDR_DDR_11_BASE + UNM_ADDR_DDR_11_SIZE)
#define UNM_ADDR_DDR_HEAP_SIZE  0x2000000

/*
 * ====================== OCM MEMORY MAP ======================
 * Map of all users of OCM.
 * ====================== OCM MEMORY MAP ======================
 */

/* OCM_0: Local Queues - MUST be 8KB aligned!! */
#define UNM_ADDR_OCM_0_BASE            	UNM_ADDR_OCM0

#define	UNM_ADDR_OCM_0_SIZE	        UNM_ADDR_PQMBUF_SIZE_OCM

/* Nic lso descriptors. */
#define UNM_ADDR_OCM_1_BASE            (UNM_ADDR_OCM_0_BASE + UNM_ADDR_OCM_0_SIZE)
#define UNM_NIC_XG_LSO_DESC_BASE       UNM_ADDR_OCM_1_BASE

/* OCM mbuf pool. */
#define UNM_ADDR_OCM_2_BASE            0x200080000ULL

#define UNM_PQM_MEMSHIFT        13    /* Q base addr shift amount */
#define UNM_PQM_ADDR_MASK       ((1ULL<<UNM_PQM_MEMSHIFT)-1)

/*
 * ====================== QDR MEMORY MAP ======================
 * Map of all users of QDR.
 * ====================== QDR MEMORY MAP ======================
 */
/* QDR_0: Ticket queues */
#define	UNM_ADDR_QDR_0_BASE	UNM_ADDR_QDR_NET

#define	UNM_ADDR_QDR_0_SIZE	UNM_ADDR_TICKETQ_SIZE
/* QDR_1: Sqm state tables */
#define	UNM_ADDR_QDR_1_BASE	(UNM_ADDR_QDR_0_BASE + UNM_ADDR_QDR_0_SIZE)
#define	UNM_ADDR_QDR_1_SIZE	UNM_ADDR_SQMSTAT_SIZE
/* SQG0 Hidden Message ptrs */
#define	UNM_ADDR_QDR_2_BASE	(UNM_ADDR_QDR_1_BASE + UNM_ADDR_QDR_1_SIZE)
#define	UNM_ADDR_QDR_2_SIZE	UNM_ADDR_SQG0PTR_SIZE
/* QDR_3: SRE L2 Look up table - should be 512 byte aligned */
#define UNM_ADDR_QDR_3_BASE     (((UNM_ADDR_QDR_2_BASE + UNM_ADDR_QDR_2_SIZE) +\
                                511ULL) & ~511ULL)
#define	UNM_ADDR_QDR_3_SIZE	(UNM_ADDR_L2LU_SIZE + UNM_ADDR_VPORT_LU_SIZE)
/* QDR_4: Local Queues - MUST be 8KB aligned!! */
#define	UNM_ADDR_QDR_4_BASE	(((UNM_ADDR_QDR_3_BASE + UNM_ADDR_QDR_3_SIZE) +\
                               (UNM_PQM_ADDR_MASK)) & ~UNM_PQM_ADDR_MASK)
#define	UNM_ADDR_QDR_4_SIZE	UNM_ADDR_PQMBUF_SIZE_MS
/* QDR_5: SRE Flow Table */
#define	UNM_ADDR_QDR_5_BASE	((UNM_ADDR_QDR_4_BASE) + (UNM_ADDR_QDR_4_SIZE))
#define	UNM_ADDR_QDR_5_SIZE	UNM_ADDR_SREFLOW_SIZE
/* QDR_6: Timer state tables */
#define	UNM_ADDR_QDR_6_BASE	(UNM_ADDR_QDR_5_BASE + UNM_ADDR_QDR_5_SIZE)
#define	UNM_ADDR_QDR_6_SIZE	UNM_ADDR_TIMERST_SIZE
/* QDR_7: Tuple Buffers */
#define	UNM_ADDR_QDR_7_BASE	(UNM_ADDR_QDR_6_BASE + UNM_ADDR_QDR_6_SIZE)
#define	UNM_ADDR_QDR_7_SIZE	UNM_ADDR_TUPLEBUF_SIZE
/* QDR_8: CAM Buffers */
#define	UNM_ADDR_QDR_8_BASE	(UNM_ADDR_QDR_7_BASE + UNM_ADDR_QDR_7_SIZE)
#define	UNM_ADDR_QDR_8_SIZE	(0)
/* QDR_9: CAM Tables  */
#define	UNM_ADDR_QDR_9_BASE	(UNM_ADDR_QDR_8_BASE + UNM_ADDR_QDR_8_SIZE)
#define	UNM_ADDR_QDR_9_SIZE	(0)
/* QDR_10: RDMA SRE Tuples */
#define	UNM_ADDR_QDR_10_BASE	(UNM_ADDR_QDR_9_BASE + UNM_ADDR_QDR_9_SIZE)
#define	UNM_ADDR_QDR_10_SIZE	(RDMA_TUPLES_SIZE)
/* QDR_11: SRCIP Hash tuples for IPV6 */
#define	UNM_ADDR_QDR_11_BASE	(UNM_ADDR_QDR_10_BASE + UNM_ADDR_QDR_10_SIZE)
#define	UNM_ADDR_QDR_11_SIZE	(UNM_ADDR_SRE_SRCIP_SIZE)
/* SQG1 Hidden Message Ptrs. */
#define	UNM_ADDR_QDR_12_BASE	(UNM_ADDR_QDR_11_BASE + UNM_ADDR_QDR_11_SIZE)
#define	UNM_ADDR_QDR_12_SIZE 	UNM_ADDR_SQG1PTR_SIZE
/* SQG2 Hidden Message Ptrs. */
#define	UNM_ADDR_QDR_13_BASE	(UNM_ADDR_QDR_12_BASE + UNM_ADDR_QDR_12_SIZE)
#define	UNM_ADDR_QDR_13_SIZE 	UNM_ADDR_SQG2PTR_SIZE
/* SQG3 Hidden Message ptrs */
#define	UNM_ADDR_QDR_14_BASE	(UNM_ADDR_QDR_13_BASE + UNM_ADDR_QDR_13_SIZE)
#define	UNM_ADDR_QDR_14_SIZE	UNM_ADDR_SQG3PTR_SIZE

#define MSWIN_OFFSET_IN_BAR0	(512*1024)
#define	UNM_ADDR_QDR_MSWIN_SIZE (256*1024)

#define	UNM_ADDR_QDR_MSWIN_BASE	(UNM_ADDR_QDR_NET + 0x400000)
#if ((UNM_ADDR_QDR_14_BASE + UNM_ADDR_QDR_14_SIZE) > UNM_ADDR_QDR_MSWIN_BASE)
#error "QDR_14 overlaps MSIX in MS"
#endif

/* SQG0 Message buffers. */
#define	UNM_ADDR_QDR_15_BASE	(UNM_ADDR_QDR_MSWIN_BASE + \
				    UNM_ADDR_QDR_MSWIN_SIZE)
#define	UNM_ADDR_QDR_15_SIZE    (UNM_ADDR_SQG0BUF_SIZE)

/* SQG1 Message Buffers. */
#define	UNM_ADDR_QDR_16_BASE	(UNM_ADDR_QDR_15_BASE + UNM_ADDR_QDR_15_SIZE)
#define	UNM_ADDR_QDR_16_SIZE    (UNM_ADDR_SQG1BUF_SIZE)
/* SQG2 Message Buffers. */
#define	UNM_ADDR_QDR_17_BASE	(UNM_ADDR_QDR_16_BASE + UNM_ADDR_QDR_16_SIZE)
#define	UNM_ADDR_QDR_17_SIZE    (UNM_ADDR_SQG2BUF_SIZE)
/* SQG3 Message Buffers. */
#define	UNM_ADDR_QDR_18_BASE	(UNM_ADDR_QDR_17_BASE + UNM_ADDR_QDR_17_SIZE)
#define	UNM_ADDR_QDR_18_SIZE    (UNM_ADDR_SQG3BUF_SIZE)
/*  dstip Hash tuples for ipv4 */
#define	UNM_ADDR_QDR_19_BASE	(UNM_ADDR_QDR_18_BASE + UNM_ADDR_QDR_18_SIZE)
#define	UNM_ADDR_QDR_19_SIZE	(UNM_ADDR_SRE_DSTIP_SIZE)
/*  dstip Hash tuples for ipv6 */
#define	UNM_ADDR_QDR_20_BASE	(UNM_ADDR_QDR_19_BASE + UNM_ADDR_QDR_19_SIZE)
#define	UNM_ADDR_QDR_20_SIZE	(UNM_ADDR_SRE_DSTIPV6_SIZE)
/*  ncsi buffer ring */
#define	UNM_ADDR_QDR_21_BASE	(UNM_ADDR_QDR_20_BASE + UNM_ADDR_QDR_20_SIZE)
#define	UNM_ADDR_QDR_21_SIZE	(UNM_ADDR_NCSIBUF_SIZE)

#define	UNM_ADDR_QDR_END	(UNM_ADDR_QDR_21_BASE + UNM_ADDR_QDR_21_SIZE)

/*
 * ====================== MBUF SUPPORT ======================
 * SRE Bufferes (legacy name: MBUF) and their management.
 * ====================== MBUF SUPPORT ======================
 */
#ifndef NX_BIG_SRE
#error "NX_BIG_SRE must be defined."
#endif


/* MN MBUF POOLS. */
#define	UNM_ADDR_MBUF_BASE	(UNM_ADDR_DDR_1_BASE)
#define	UNM_ADDR_MBUF_UNIT	(2048)
/* Primary mbuf pool.  Used for packets with smaller mtu. */
#define UNM_NUM_MBUF1_UNITS     (1)    /* Units of 2K bytes. */
#define UNM_ADDR_MBUF1_COUNT    (2048)
#define UNM_ADDR_MBUF1_SIZE     (UNM_NUM_MBUF1_UNITS*UNM_ADDR_MBUF_UNIT)

/* Jumbo mbuf pool.  Used for packet with larger mtu. */
#define UNM_NUM_MBUF2_UNITS     (5)     /* Units of 2K bytes. */
#define	UNM_ADDR_MBUF2_COUNT    (8192)
#define UNM_ADDR_MBUF2_SIZE     (UNM_NUM_MBUF2_UNITS*UNM_ADDR_MBUF_UNIT)

#define UNM_ADDR_MN_MBUF_COUNT   (UNM_ADDR_MBUF1_COUNT+UNM_ADDR_MBUF2_COUNT)
#define	UNM_TOTAL_MN_MBUF_SIZE	(UNM_ADDR_MBUF_COUNT * UNM_ADDR_MBUF_UNIT)

/* OCM MBUF POOLS. */
#define	UNM_ADDR_OCM_MBUF_BASE	(UNM_ADDR_OCM_2_BASE - (UNM_ADDR_MBUFPTR3_LO*UNM_ADDR_MBUF_UNIT))

#define UNM_NUM_MBUF3_UNITS     (1)     /* Units of 2K bytes. */
#define	UNM_ADDR_MBUF3_COUNT    (256)
#define UNM_ADDR_MBUF3_SIZE     (UNM_NUM_MBUF3_UNITS*UNM_ADDR_MBUF_UNIT)

#define UNM_ADDR_OCM_MBUF_COUNT (UNM_ADDR_MBUF3_COUNT)

#define	UNM_TOTAL_OCM_MBUF_SIZE	(UNM_ADDR_OCM_MBUF_COUNT * UNM_ADDR_MBUF_UNIT)

#define UNM_ADDR_MBUF_COUNT  (UNM_ADDR_MN_MBUF_COUNT+UNM_ADDR_OCM_MBUF_COUNT)

/*
 * ====================== MESSAGE QUEUE SETUP ======================
 * Config parameters for setting up message queues.
 * ====================== MESSAGE QUEUE SETUP ======================
 */

/* These define the max queues per type 1 remote group. These must be in
* units of 512 */
#define UNM_ADDR_MAX_Q_SQG2_0 (512 + (UNM_ADDR_SQG2_NUM_TABOO_QUEUES))
#define UNM_ADDR_MAX_Q_SQG2_1 ((MAX_CONNECTIONS*2) +\
	       			(UNM_ADDR_SQG2_NUM_TABOO_QUEUES))
#define UNM_ADDR_MAX_Q_SQG2_2 512
#define UNM_ADDR_MAX_Q_SQG2_3 512
#define UNM_ADDR_MAX_Q_SQG3_0 (MAX_CONNECTIONS_PER_PEG)
#define UNM_ADDR_MAX_Q_SQG3_1 (MAX_CONNECTIONS_PER_PEG)
#define UNM_ADDR_MAX_Q_SQG3_2 (MAX_CONNECTIONS_PER_PEG)
#define UNM_ADDR_MAX_Q_SQG3_3 (MAX_CONNECTIONS_PER_PEG)

/*
 * The state table that tells everything about each of the 256K secondary
 * queues in each SQM.
 */
#define	UNM_ADDR_SQMSTAT_BASE	UNM_ADDR_QDR_1_BASE
#define	UNM_ADDR_SQMSTAT_UNIT	(8)
#define	UNM_ADDR_SQMSTAT_COUNT  (UNM_ADDR_MAX_Q_SQG3_0 + UNM_ADDR_MAX_Q_SQG3_1 + \
                                 UNM_ADDR_MAX_Q_SQG3_2 + UNM_ADDR_MAX_Q_SQG3_3 + \
                                 UNM_ADDR_MAX_Q_SQG2_0 + UNM_ADDR_MAX_Q_SQG2_1 + \
                                 UNM_ADDR_MAX_Q_SQG2_2 + UNM_ADDR_MAX_Q_SQG2_3)

#define	UNM_ADDR_SQMSTAT_SIZE	((UNM_ADDR_SQMSTAT_COUNT) * (UNM_ADDR_SQMSTAT_UNIT))
#define	UNM_ADDR_SQMSTAT_MAX	(UNM_ADDR_SQMSTAT_BASE + UNM_ADDR_SQMSTAT_SIZE)

#define       UNM_ADDR_SQMSTAT_BASE_TABLE(G)  /* base of each state table region */\
      (UNM_ADDR_SQMSTAT_BASE +                                \
      (    \
((G)==0 ? 0 : \
((G)==1 ? UNM_ADDR_MAX_Q_SQG2_0 : \
((G)==2 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1 : \
((G)==3 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1  + UNM_ADDR_MAX_Q_SQG2_2: \
((G)==4 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1  + UNM_ADDR_MAX_Q_SQG2_2 + \
          UNM_ADDR_MAX_Q_SQG2_3  : \
((G)==5 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1  + UNM_ADDR_MAX_Q_SQG2_2 + \
          UNM_ADDR_MAX_Q_SQG2_3  + UNM_ADDR_MAX_Q_SQG3_0 : \
((G)==6 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1  + UNM_ADDR_MAX_Q_SQG2_2 + \
          UNM_ADDR_MAX_Q_SQG2_3  + UNM_ADDR_MAX_Q_SQG3_0  + UNM_ADDR_MAX_Q_SQG3_1: \
((G)==7 ? UNM_ADDR_MAX_Q_SQG2_0  + UNM_ADDR_MAX_Q_SQG2_1  + UNM_ADDR_MAX_Q_SQG2_2 + \
          UNM_ADDR_MAX_Q_SQG2_3 + UNM_ADDR_MAX_Q_SQG3_0  + UNM_ADDR_MAX_Q_SQG3_1 + \
          UNM_ADDR_MAX_Q_SQG3_2 : -1 )))))))) * UNM_ADDR_SQMSTAT_UNIT))

/*
 * The pointer queue that is used to allocate/free message buffers by the SQM.
 * We support two pointer pools - one in QDR and one in DDR.
 */
#define	UNM_ADDR_QDR_SQMPTR_BASE	UNM_ADDR_QDR_2_BASE

/* These are the msg pools for the remote queues (max msgs per group). */
#define SQG0_MSGCOUNT (64*1024)
#define SQG1_MSGCOUNT (64*1024)
#define SQG2_MSGCOUNT (64*1024)
#define SQG3_MSGCOUNT (64*1024)

/* These are the local queue sizes.
   Each must be a multiple of 128. */
#define PQM0_MSGCOUNT  128
#define PQM1_MSGCOUNT  256
#define PQM2_MSGCOUNT  512
#define PQM3_MSGCOUNT  1280
#define PQM4_MSGCOUNT  4096
#define PQM5_MSGCOUNT  128
#define PQM6_MSGCOUNT  (128*6)
#define PQM7_MSGCOUNT  512
#define PQM8_MSGCOUNT  128
#define PQM9_MSGCOUNT  (2048 + 128)
#define PQM10_MSGCOUNT (2048 + 128)
#define PQM11_MSGCOUNT 512
#define PQM12_MSGCOUNT 256
#define PQM13_MSGCOUNT 128
#define PQM14_MSGCOUNT (128*8)
#define PQM15_MSGCOUNT 512

/*
 * The actual message storage for all the messages in a PQM.
 */

#define	UNM_ADDR_PQMBUF_UNIT	UNM_MESSAGE_SIZE

#define	UNM_ADDR_PQMBUF_BASE_MN	  0
#define	UNM_ADDR_PQMBUF_COUNT_MN  0
#define UNM_ADDR_PQMBUF_SIZE_MN (UNM_ADDR_PQMBUF_COUNT_MN * UNM_ADDR_PQMBUF_UNIT)

#define	UNM_ADDR_PQMBUF_BASE_MS   UNM_ADDR_QDR_4_BASE
#define	UNM_ADDR_PQMBUF_COUNT_MS  (PQM0_MSGCOUNT + PQM1_MSGCOUNT +\
                                   PQM2_MSGCOUNT + PQM3_MSGCOUNT +\
                                   PQM4_MSGCOUNT + PQM5_MSGCOUNT +\
                                   PQM6_MSGCOUNT + PQM7_MSGCOUNT +\
                                   PQM8_MSGCOUNT + PQM9_MSGCOUNT +\
                                   PQM10_MSGCOUNT + PQM11_MSGCOUNT +\
                                   PQM12_MSGCOUNT + PQM13_MSGCOUNT +\
                                   PQM14_MSGCOUNT + PQM15_MSGCOUNT)
#define UNM_ADDR_PQMBUF_SIZE_MS (UNM_ADDR_PQMBUF_COUNT_MS * UNM_ADDR_PQMBUF_UNIT)

#define	UNM_ADDR_PQMBUF_BASE_OCM  UNM_ADDR_OCM_0_BASE
#define	UNM_ADDR_PQMBUF_COUNT_OCM (PQM1_MSGCOUNT +\
                                   PQM6_MSGCOUNT +\
                                   PQM12_MSGCOUNT+\
                                   PQM14_MSGCOUNT)
#define UNM_ADDR_PQMBUF_SIZE_OCM (UNM_ADDR_PQMBUF_COUNT_OCM * UNM_ADDR_PQMBUF_UNIT)

#define SQG_TYPE0_UNUSED_Q_LEN      32


/*
 * Set up SQG0
 */
// #define NX_SQG0_IN_MN
#ifdef NX_SQG0_IN_MN
#error "sqg0 in mn is not currently allowed"
#else
#define UNM_ADDR_SQG0PTR_BASE    UNM_ADDR_QDR_2_BASE
#define UNM_ADDR_SQG0PTR_SIZE    (SQG0_MSGCOUNT * UNM_ADDR_SQMPTR_UNIT)
#define	UNM_ADDR_SQG0BUF_BASE    UNM_ADDR_QDR_15_BASE
#define	UNM_ADDR_SQG0BUF_SIZE    (SQG0_MSGCOUNT * UNM_ADDR_SQMBUF_UNIT)
#endif

/*
 * Set up SQG1
 */
// #define NX_SQG1_IN_MN
#ifdef NX_SQG1_IN_MN
#error "sqg1 in mn is not currently allowed"
#else
#define UNM_ADDR_SQG1PTR_BASE    UNM_ADDR_QDR_12_BASE
#define UNM_ADDR_SQG1PTR_SIZE    (SQG1_MSGCOUNT * UNM_ADDR_SQMPTR_UNIT)
#define	UNM_ADDR_SQG1BUF_BASE    UNM_ADDR_QDR_16_BASE
#define	UNM_ADDR_SQG1BUF_SIZE    (SQG1_MSGCOUNT * UNM_ADDR_SQMBUF_UNIT)
#endif

/*
 * Set up SQG 2
 */
// #define NX_SQG2_IN_MN
#define UNM_ADDR_SQG2PTR_BASE    UNM_ADDR_QDR_13_BASE
#define SQG2_HQ_TGTTAG           2
#define	UNM_ADDR_SQG2BUF_BASE    UNM_ADDR_QDR_17_BASE
#define SQG2_TGTTAG              2
#define	UNM_ADDR_SQG2BUF_SIZE    (SQG2_MSGCOUNT * UNM_ADDR_SQMBUF_UNIT)
#define UNM_ADDR_SQG2PTR_SIZE    (SQG2_MSGCOUNT * UNM_ADDR_SQMPTR_UNIT)
/*
 * Set up SQG3
 */
// #define NX_SQG3_IN_MN
#define UNM_ADDR_SQG3PTR_BASE    UNM_ADDR_QDR_14_BASE
#define SQG3_HQ_TGTTAG           2
#define	UNM_ADDR_SQG3BUF_BASE    UNM_ADDR_QDR_18_BASE
#define SQG3_TGTTAG              2
#define	UNM_ADDR_SQG3BUF_SIZE    (SQG3_MSGCOUNT * UNM_ADDR_SQMBUF_UNIT)
#define UNM_ADDR_SQG3PTR_SIZE    (SQG3_MSGCOUNT * UNM_ADDR_SQMPTR_UNIT)

/*
 * ====================== TICKET QUEUE TABLES SETUP ======================
 * Support for all of the ticket queues in the system.
 * ====================== TICKET QUEUE TABLES SETUP ======================
 */
/*
 * The ticket queue that is used to allocate/free MBUFs.
 * NOTE: SRE mandates that this be first in the TICKETQ table.
 */
#define	UNM_ADDR_MBUFPTR_COUNT	UNM_ADDR_MBUF1_COUNT
#define	UNM_ADDR_MBUFPTR_LO	(0)	/* NOTE: this must be 0 */
#define	UNM_ADDR_MBUFPTR_HI	(UNM_ADDR_MBUFPTR_LO+UNM_ADDR_MBUFPTR_COUNT)

#define	UNM_ADDR_MBUFPTR2_COUNT UNM_ADDR_MBUF2_COUNT
#define	UNM_ADDR_MBUFPTR2_LO	UNM_ADDR_MBUFPTR_HI
#define	UNM_ADDR_MBUFPTR2_HI	(UNM_ADDR_MBUFPTR2_LO+UNM_ADDR_MBUFPTR2_COUNT)

#define	UNM_ADDR_MBUFPTR3_COUNT UNM_ADDR_MBUF3_COUNT
#define	UNM_ADDR_MBUFPTR3_LO	UNM_ADDR_MBUFPTR2_HI
#define	UNM_ADDR_MBUFPTR3_HI	(UNM_ADDR_MBUFPTR3_LO+UNM_ADDR_MBUFPTR3_COUNT)

/*
 * Tuple allocation.  Formerly a ticket queue.
 */
#define	UNM_ADDR_TUPLEPTR_COUNT MAX_CONNECTIONS
/* Define so we don't use a tuple ticket queue. */
#define NX_NO_TUPLE_TICKETS

/* QID allocation. Formerly a set of ticket queues. */
/* Define so we don't use to the qid ticket queues. */
#define NX_NO_QID_TICKETS

/* Sock struct free pool queue */
#define UNM_ADDR_SOCK_FREE_COUNT   MAX_CONNECTIONS

/*
 * Storage for the ticket queue linked list.
 */
#define	UNM_ADDR_TICKETQ_BASE	UNM_ADDR_QDR_0_BASE
#define	UNM_ADDR_TICKETQ_UNIT	(8)
#define	UNM_ADDR_TICKETQ_COUNT	UNM_ADDR_MBUFPTR3_HI
#define	UNM_ADDR_TICKETQ_SIZE	(UNM_ADDR_TICKETQ_COUNT * UNM_ADDR_TICKETQ_UNIT)
#define	UNM_ADDR_TICKETQ_MAX	(UNM_ADDR_TICKETQ_BASE + UNM_ADDR_TICKETQ_SIZE)

/*
 * ====================== SRE SUPPORT TABLES SETUP ======================
 * Support tables for the SRE block.
 * ====================== SRE SUPPORT TABLES SETUP ======================
 */
/*
 * Storage for SRE L2 look up table
 */
#define UNM_ADDR_L2LU_BASE    UNM_ADDR_QDR_3_BASE
#define UNM_ADDR_L2LU_UNIT      (32)
#define UNM_ADDR_L2LU_COUNT     ((40 * 4) + 1) /* 40 macs/port plus free hdr */
#define UNM_ADDR_L2LU_FLOW_COUNT (64)
#define UNM_ADDR_L2LU_SIZE      (UNM_ADDR_L2LU_COUNT * UNM_ADDR_L2LU_UNIT)
#define UNM_ADDR_L2LU_MAX       (UNM_ADDR_L2LU_BASE + UNM_ADDR_L2LU_SIZE)

#define UNM_CAM_L2LU_LRU_HEAD   (1060) /* cam l2 look up head */
#define UNM_CAM_L2LU_LRU_LEN    (16)   /* cam l2 look up len */
/*
 * Storage for SRE vport look up table
 */
#define UNM_ADDR_VPORT_LU_BASE	UNM_ADDR_L2LU_MAX
#define UNM_ADDR_VPORT_LU_UNIT  (32)
#define MAX_VPORT_RULES 64
#define UNM_ADDR_VPORT_LU_FLOW_COUNT (64)
#define UNM_VPORT_MAX_RX_CONTEXTS (8)
#define UNM_ADDR_VPORT_LU_COUNT ((MAX_VPORT_RULES * UNM_VPORT_MAX_RX_CONTEXTS) + 1)
#define UNM_ADDR_VPORT_LU_SIZE  (UNM_ADDR_VPORT_LU_COUNT * UNM_ADDR_VPORT_LU_UNIT)

#define UNM_CAM_VPORT_LU_LRU_HEAD   (0x800) /* cam vport look up head at
					       beginning of pool 3*/
#define UNM_CAM_VPORT_LU_LRU_LEN    (64)    /* cam vport look up len */

/* work around for P3 B0 Vport */
#define UNM_CAM_L2LU_LRU_HEAD_B0_VP_WA (UNM_CAM_VPORT_LU_LRU_HEAD + UNM_CAM_VPORT_LU_LRU_LEN) /* cam l2 look up head */
#define UNM_CAM_L2LU_LRU_LEN_B0_VP_WA  (4)  /* cam l2 look up len */

#define UNM_CAM_FLOW_HEAD	(0)
/*
 * An array of CAM entries that the SRE directly points to where each entry
 * is used as the head of a linked list of CAM entries for the TUPLE table.
 */
#define	UNM_ADDR_SREFLOW_BASE	UNM_ADDR_QDR_5_BASE
#define	UNM_ADDR_SREFLOW_UNIT	(32)	/* == sizeof(unm_cam_entry_memory_t) */
#define	UNM_ADDR_SREFLOW_COUNT	(512)
#define	UNM_ADDR_SREFLOW_SIZE	(UNM_ADDR_SREFLOW_COUNT * UNM_ADDR_SREFLOW_UNIT)
#define	UNM_ADDR_SREFLOW_MAX	(UNM_ADDR_SREFLOW_BASE + UNM_ADDR_SREFLOW_SIZE)

/* srcip lookups for re2 */

/*
 * An array of CAM entries to be used as linked list heads for
 * srcip hash tables for tcp over ipv6 srcip hash matching in
 * Flow Mapper.
 */
#define UNM_ADDR_SRE_SRCIP_BASE	UNM_ADDR_QDR_11_BASE
#define UNM_ADDR_SRE_SRCIP_UNIT	(32) /* == sizeof(unm_cam_entry_memory_t) */
#define UNM_ADDR_SRE_SRCIP_COUNT	(127)
#define UNM_SRE_TOTAL_SCRIP_TUPLES	(UNM_ADDR_SRE_SRCIP_COUNT << 1)
						/* one ipv6 address -> 2 entries */

#define UNM_SRE_CAM_SRCIP_BUCKETS	64
/* Since the bucket heads can host an entry each, the total size can
   be the same as the number of cam entries we need * sizeof(each entry) +
   One entry to maintain as a free list head. The entire structure will be
   used as an array of bucket heads, followed by a free list head, followed
   by a list of free entries. Force it to end on a cache line.*/

#define UNM_ADDR_SRE_SRCIP_SIZE ((((UNM_SRE_TOTAL_SCRIP_TUPLES + 1) * UNM_ADDR_SRE_SRCIP_UNIT) + 512) & (~(511)))
#define UNM_ADDR_SRE_SRCIP_MAX (UNM_ADDR_SRE_SRCIP_BASE + UNM_ADDR_SRE_SRCIP_SIZE)

/* CAUTION: base and (base + len) should lie in the same cam pool */
/* It's in pool 1 after dstip hash */
#define UNM_SRE_CAM_SRCIP_BASE	(1024 + 128 + 16)
#define UNM_SRE_CAM_INTERNAL_SRCIP_TUPLES	64

/* dstip tupels */
/*
 * An array of CAM entries to be used as linked list heads for
 * srcip hash tables for tcp over ipv6 srcip hash matching in
 * Flow Mapper.
 */
#define UNM_ADDR_SRE_DSTIP_BASE	UNM_ADDR_QDR_19_BASE
#define UNM_ADDR_SRE_DSTIP_UNIT	(32) /* == sizeof(unm_cam_entry_memory_t) */
#define UNM_ADDR_SRE_DSTIP_COUNT	(255)

#define UNM_SRE_CAM_DSTIP_BUCKETS	64
/* Since the bucket heads can host an entry each, the total size can
   be the same as the number of cam entries we need * sizeof(each entry) +
   One entry to maintain as a free list head. The entire structure will be
   used as an array of bucket heads, followed by a free list head, followed
   by a list of free entries. Force it to end on a cache line.*/

#define UNM_ADDR_SRE_DSTIP_SIZE ((((UNM_ADDR_SRE_DSTIP_COUNT + UNM_SRE_CAM_DSTIP_BUCKETS + 1) * UNM_ADDR_SRE_DSTIP_UNIT) + 512) & (~(511)))
#define UNM_ADDR_SRE_DSTIP_MAX (UNM_ADDR_SRE_DSTIP_BASE + UNM_ADDR_SRE_DSTIP_SIZE)

#define UNM_ADDR_SRE_DSTIPV6_BASE	UNM_ADDR_QDR_20_BASE
#define UNM_ADDR_SRE_DSTIPV6_COUNT	(127)
#define UNM_SRE_TOTAL_DSTIPV6_TUPLES	(UNM_ADDR_SRE_DSTIPV6_COUNT << 1)
#define UNM_ADDR_SRE_DSTIPV6_SIZE ((((UNM_SRE_TOTAL_DSTIPV6_TUPLES + UNM_SRE_CAM_DSTIP_BUCKETS + 1) * UNM_ADDR_SRE_DSTIP_UNIT) + 512) & (~(511)))

/* CAUTION: base and (base + len) should lie in the same cam pool */
/* It's in pool 1 after dstip hash */
#define UNM_SRE_CAM_DSTIP_BASE	(2116)
#define UNM_SRE_CAM_INTERNAL_DSTIP_LO_TUPLES	(128)
#define UNM_SRE_CAM_INTERNAL_DSTIP_HI_TUPLES	(32)

#define UNM_SRE_CAM_DSTIP_HI_BASE  (UNM_SRE_CAM_DSTIP_BASE + UNM_SRE_CAM_INTERNAL_DSTIP_LO_TUPLES)

#define UNM_SRE_CAM_INTERNAL_DSTIP_LO_THRESHOLD	(UNM_SRE_CAM_INTERNAL_DSTIP_LO_TUPLES - 8)

#define UNM_SRE_CAM_INTERNAL_DSTIP_HI_THRESHOLD	(UNM_SRE_CAM_INTERNAL_DSTIP_HI_TUPLES - 8)

/*
 * An array of CAM entries that we can attach to the linked lists in the
 * SREFLOW table above.
 */
#define	UNM_ADDR_TUPLEBUF_BASE	UNM_ADDR_QDR_7_BASE
#define	UNM_ADDR_TUPLEBUF_UNIT	(32)
#define	UNM_ADDR_TUPLEBUF_COUNT UNM_ADDR_TUPLEPTR_COUNT
#define	UNM_ADDR_TUPLEBUF_SIZE	(UNM_ADDR_TUPLEBUF_COUNT * UNM_ADDR_TUPLEBUF_UNIT)
#define	UNM_ADDR_TUPLEBUF_MAX	(UNM_ADDR_TUPLEBUF_BASE + UNM_ADDR_TUPLEBUF_SIZE)

/*
 * Where are cams buffers are located.
 */
//#define HAL_CAM_IN_MN
#if defined(HAL_CAM_IN_MN)
#error "Need to allocate an MN buffer for the cams"
#endif

/*
 * ====================== TIMER SUPPORT TABLES SETUP ======================
 * Support tables for the TIMER block.
 * ====================== TIMER SUPPORT TABLES SETUP ======================
 */

/*
 * Storage for the timer state information (counts and flowids, etc).
 */
/* Where are timer state table are located. */
//#define HAL_TIMER_IN_MN
#define	UNM_ADDR_TIMERST_BASE	UNM_ADDR_QDR_6_BASE
#define	UNM_ADDR_TIMERST_UNIT	(16)
#define	UNM_ADDR_TIMERST_COUNT   MAX_CONNECTIONS
#define	UNM_ADDR_TIMERST_SIZE	(UNM_ADDR_TIMERST_COUNT * UNM_ADDR_TIMERST_UNIT)

/*
 * Set the time bases for timer sets.
 */
#define UNM_FLOW_TICKS_PER_SEC    256
#define UNM_FLOW_TO_TV_SHIFT_SEC  8
#define UNM_FLOW_TO_TV_SHIFT_USEC 12
#define UNM_FLOW_TICK_USEC   (1000000ULL/UNM_FLOW_TICKS_PER_SEC)
#define UNM_GLOBAL_TICKS_PER_SEC  (32*UNM_FLOW_TICKS_PER_SEC)
#define UNM_GLOBAL_TICK_USEC (1000000ULL/UNM_GLOBAL_TICKS_PER_SEC)

/* Timebase for 8-bit timers 0, 1, 2, and 3 */
#define UNM_TIMEBASE_A_FLOW_TICK 64              /* timebase A ~ 32 ms */
#define UNM_TIMEBASE_A_USEC (UNM_TIMEBASE_A_FLOW_TICK*UNM_FLOW_TICK_USEC)
/* Timebase B for 8-bit timers 4, 5 */
#define UNM_TIMEBASE_B_FLOW_TICK 4               /* timebase B ~ 2 ms */
#define UNM_TIMEBASE_B_USEC (UNM_TIMEBASE_B_FLOW_TICK*UNM_FLOW_TICK_USEC)
/* Timebase C for 16-bit timer 6 */
#define UNM_TIMEBASE_C_FLOW_TICK 128             /* timebase C ~ 64 ms */
#define UNM_TIMEBASE_C_USEC (UNM_TIMEBASE_C_FLOW_TICK*UNM_FLOW_TICK_USEC)
/* Timebase D for 16-bit timer 7 */
#define UNM_TIMEBASE_D_FLOW_TICK 512             /* timebase D ~ 256 ms */
#define UNM_TIMEBASE_D_USEC (UNM_TIMEBASE_D_FLOW_TICK*UNM_FLOW_TICK_USEC)

#define UNM_MSGQ_PER_PEG_QUEUE_MAJOR    12
#define UNM_MSGQ_PER_PEG_QUEUE_MINOR  (0x00023 + ((__peg__mfsr(0x10)>>60)<<16))
#define UNM_MSGQ_PER_PEG_QUEUE_0        0x00023      /* Peg 0 */
#define UNM_MSGQ_PER_PEG_QUEUE_1        0x10023      /* Peg 1 */
#define UNM_MSGQ_PER_PEG_QUEUE_2        0x20023      /* Peg 2 */
#define UNM_MSGQ_PER_PEG_QUEUE_3        0x30023      /* Peg 3 */

/*
 * Used for chimney offload initiate commands.
 */
#define UNM_MSGQ_CHIMNEY0		0x00025      /* Peg 0 */
#define UNM_MSGQ_CHIMNEY1		0x10025      /* Peg 1 */
#define UNM_MSGQ_CHIMNEY2		0x20025      /* Peg 2 */
#define UNM_MSGQ_CHIMNEY3		0x30025      /* Peg 3 */

/* Storing and retrieving messages on the dma queue. */
#define UNM_PUT_DMA_REPLY_HDR(replyWord,replyHdr)  \
        (replyHdr).word |=  (((__uint64_t)(replyWord))&0xfffffffULL)<<28

#define UNM_GET_DMA_REPLY_HDR(reply_hdr)      \
        ( (((reply_hdr).word)>>28) & 0xfffffffULL )

/*Header creation for a dma request to the specified dma engine*/
#define UNM_MSGQ_DMA_REQUEST_ENGINE(HDR,ENGINE)                                  \
    do { extern unm_hal_init_info_t pegnet_hal_init ;                            \
         (HDR).word = pegnet_hal_init.dma_confinfo.reqhdr[ENGINE].word;          \
    } while( 0 ) ;

#if 0 // MMO
#define PEG_TO_DMA_MAPPING(peg)     ((peg))
#else
/* This mapping seems to reduce (but not eliminate) the dma hang occurrence.
 */
#define PEG_TO_DMA_MAPPING(peg)     ((peg)%2)
#endif

/* Header creation for a dma request from the peg. */
#define UNM_MSGQ_DMA_REQUEST(HDR)   UNM_MSGQ_DMA_REQUEST_ENGINE(HDR, \
                                    PEG_TO_DMA_MAPPING(getCpuId()))

/* Reply queues, used for synchronous dma's. */
#define UNM_MSGQ_REPLY_0            0x00022     /* Peg 0 */
#define UNM_MSGQ_REPLY_1            0x10022     /* Peg 1 */
#define UNM_MSGQ_REPLY_2            0x20022     /* Peg 2 */
#define UNM_MSGQ_REPLY_3            0x30022     /* Peg 3 */
#define UNM_MSGQ_REPLY_Q()          ( UNM_MSGQ_REPLY_0 + ((__peg__mfsr(0x10)>>60)<<16) )


#define UNM_MSGQ_HDR_HOST_NIC(HDR,MAJOR,MINOR) \
    do {*(__uint64_t *)&(HDR) = ((__uint64_t)(MAJOR) << 19 ) | \
                                ((__uint64_t)1 << 18)        | \
                                (__uint64_t)(MINOR);           \
    } while (0)

/*
 * 4,1 used for requests from the host. This is sent by the tx peg to
 * the epg proxy.
 */
#define UNM_MSGQ_HDR_HOST(hdr)      UNM_MSGQ_HDR_HOST_NIC((hdr), 4, 1)

/*
 * ====================== MESSAGE QUEUE DEFINITIONS ======================
 * Define which message queues contain what types of messages.
 * If you change these, you must change the table in sqm.c as well.
 * ====================== MESSAGE QUEUE DEFINITIONS ======================
 */

/* We will reserve 10 type 1 queues for HW blocks */
#define UNM_MSGQ_TYPE1_FIRST_AVAIL 10

#define UNM_MSGQ_ARP_CONTROL_MAJOR_Q   	12

/* SRE type 1 exceptional minor queue numbers (with description )*/
#define UNM_MSGQ_SPQ1 1           /* ARP / RARP packets                    */
#define UNM_MSGQ_SPQ2 2           /* ICMP packets                          */
#define UNM_MSGQ_SPQ3 3           /* Pkt is fragment, but RE1 defrag disabled */
#define UNM_MSGQ_XPQ  10          /* Alien packets - non-IP */


/* Q for sending IP frags to be reassembled by the pegnet software */
#define UNM_MSGQ_SW_IP_FRAG_Q_MAJ	12
#define UNM_MSGQ_SW_IP_FRAG_Q_MIN	UNM_MSGQ_SPQ3

/* Peg priority queues.  Must be in consecutively
 * numbered queues. */
#define PEG_PRIORITY_QMAJOR     0
#define PEG_PRIORITY_SUBQ       1
#define PEG0_PRIORITY_QMINOR    0
#define PEG0_PRIORITY_Q_LEN     1024
#define PEG1_PRIORITY_QMINOR    1
#define PEG1_PRIORITY_Q_LEN     1024
#define PEG2_PRIORITY_QMINOR    2
#define PEG2_PRIORITY_Q_LEN     1024
#define PEG3_PRIORITY_QMINOR    3
#define PEG3_PRIORITY_Q_LEN     1024
#define PEG4_PRIORITY_QMINOR    4
#define PEG4_PRIORITY_Q_LEN     1024

#define RX_HOST_RSP_QMAJOR    0
#define RX0_HOST_RSP_QMINOR   8
#define RX1_HOST_RSP_QMINOR   9

#define RX_HOST_RSP_SUBQ      1
#define RX_HOST_RSP_Q_LEN     512

/* EPG type 1 minor queue numbers */
#define UNM_MSGQ_EPG_COMP 7   /* unused with epg proxy */

#define UNM_MSGQ_I2Q_INT_MAJOR  PEG_PRIORITY_QMAJOR
#define UNM_MSGQ_I2Q_INT_SUBQ   PEG_PRIORITY_SUBQ
#define UNM_MSGQ_I2Q_INT_MINOR  PEG0_PRIORITY_QMINOR
#define UNM_MSGQ_I2Q_INT        PEG0_PRIORITY_QMINOR

/*  Writer request Q  */
#define UNM_MSGQ_WRITER   9

/* Legacy header creation */
#define UNM_MSGQ_HDR(HDR,MAJOR,MINOR,TYPE) \
    UNM_QHDR_CREATE((HDR),(MAJOR),(MINOR),1,TYPE)
#define UNM_LOCAL_MSGQ_HDR(HDR,MAJOR,TYPE) \
    UNM_QHDR_CREATE((HDR),(MAJOR),0,0,TYPE)

/* utilitues to grab upper/lower dwords of a header (for HW block crb init) */
#define UNM_MSGQ_HDR_LO32(HDR)		((void *)&(((__uint32_t *)HDR)[0]))
#define UNM_MSGQ_HDR_HI32(HDR)		((void *)&(((__uint32_t *)HDR)[1]))

/* Now, the queue assignments */
/* SRE free buffer pools. */
#define HW_SRE_FBQ_MAJ          4
#define HW_SRE_FBQ_MIN          0
#define HW_SRE_FBQ_SUBQ         1

#define HW_SRE_FBQ2_MAJ         HW_SRE_FBQ_MAJ
#define HW_SRE_FBQ2_MIN         (HW_SRE_FBQ_MIN+1)
#define HW_SRE_FBQ2_SUBQ        HW_SRE_FBQ_SUBQ
#define HW_SRE_OCM_FBQ_MAJ      HW_SRE_FBQ_MAJ
#define HW_SRE_OCM_FBQ_MIN      (HW_SRE_FBQ_MIN+12)
#define HW_SRE_OCM_FBQ_SUBQ     HW_SRE_FBQ_SUBQ

#define HW_SRE_OCM_FBQ_MAJ_BUG      HW_SRE_FBQ_MAJ
#define HW_SRE_OCM_FBQ_MIN_BUG      (HW_SRE_FBQ_MIN+2)
#define HW_SRE_OCM_FBQ_SUBQ_BUG     HW_SRE_FBQ_SUBQ
#else
#define HW_SRE_OCM_FBQ_MAJ      HW_SRE_FBQ_MAJ
#define HW_SRE_OCM_FBQ_MIN      (HW_SRE_FBQ_MIN+2)
#define HW_SRE_OCM_FBQ_SUBQ     HW_SRE_FBQ_SUBQ

#define HW_SRE_OCM_FBQ2_MAJ     HW_SRE_FBQ_MAJ
#define HW_SRE_OCM_FBQ2_MIN     (HW_SRE_FBQ_MIN+3)
#define HW_SRE_OCM_FBQ2_SUBQ    HW_SRE_FBQ_SUBQ

#define SQG3_LW_INTR_MAJ	4
#define SQG3_LW_INTR_MIN	11
#define SQG3_LW_INTR_SUBQ	1

/* Now, some custom macros to initialize headers */

#define UNM_DMA_REQ_HDR_0 {type:0, \
                           dst_major:HW_DMA0_REQ_MAJ, \
                           dst_subq:HW_DMA0_REQ_SUBQ, \
                           dst_minor:HW_DMA0_REQ_MIN, \
                           dst_side:0}
#define UNM_DMA_REQ_HDR_1 {type:0, \
                           dst_major:HW_DMA1_REQ_MAJ, \
                           dst_subq:HW_DMA1_REQ_SUBQ, \
                           dst_minor:HW_DMA1_REQ_MIN, \
                           dst_side:0}
#define UNM_DMA_REQ_HDR_2 {type:0, \
                           dst_major:HW_DMA2_REQ_MAJ, \
                           dst_subq:HW_DMA2_REQ_SUBQ, \
                           dst_minor:HW_DMA2_REQ_MIN, \
                           dst_side:0}
#define UNM_DMA_REQ_HDR_3 {type:0, \
                           dst_major:HW_DMA3_REQ_MAJ, \
                           dst_subq:HW_DMA3_REQ_SUBQ, \
                           dst_minor:HW_DMA3_REQ_MIN, \
                           dst_side:0}


#define	UNM_MSGQ_SRE_PKT_FBQ(HDR)  UNM_QHDR_CREATE(HDR, \
                                                HW_SRE_FBQ_MAJ,\
                                                HW_SRE_FBQ_MIN,\
                                                HW_SRE_FBQ_SUBQ,\
                                                SRE_FREE)
#define	UNM_MSGQ_SRE_L1RE_SPQ1(HDR)	UNM_MSGQ_HDR(HDR, UNM_MSGQ_SQM_TYPE1, \
                                                 UNM_MSGQ_SPQ1,SRE_ARP)
#define	UNM_MSGQ_SRE_L1RE_SPQ2(HDR)	UNM_MSGQ_HDR(HDR, UNM_MSGQ_SQM_TYPE1, \
                                                 UNM_MSGQ_SPQ2,SRE_ICMP)
#define	UNM_MSGQ_SRE_L1RE_SPQ3(HDR)	UNM_MSGQ_HDR(HDR, UNM_MSGQ_SQM_TYPE1, \
                                                 UNM_MSGQ_SPQ3,SRE_FRAG)

/* Should match FLOW_IPQ */
#define HW_SRE_L1RE_IPQ_MAJ		6
#define HW_SRE_L1RE_IPQ_MIN		0
#define HW_SRE_L1RE_IPQ_SUBQ		0

#define HW_SRE_FLOW_IPQ_MAJ		HW_SRE_L1RE_IPQ_MAJ
#define HW_SRE_FLOW_IPQ_MIN		HW_SRE_L1RE_IPQ_MIN
#define HW_SRE_FLOW_IPQ_SUBQ		HW_SRE_L1RE_IPQ_SUBQ

#define UNM_MSGQ_SRE_L1RE_IPQ(HDR)      UNM_LOCAL_MSGQ_HDR(HDR, \
						HW_SRE_L1RE_IPQ_MAJ, SRE_IPQ)
#define UNM_MSGQ_SRE_FLOW_IPQ(HDR)      UNM_LOCAL_MSGQ_HDR(HDR, \
						HW_SRE_FLOW_IPQ_MAJ, SRE_FLOW)


#define	UNM_MSGQ_EPG_AGGR_COMPLETION(HDR)	UNM_MSGQ_HDR(HDR, UNM_MSGQ_SQM_TYPE1, \
                                                     0,EPG_AGGR_COMP)

/* send alien packets to pegnet_alien_rcv */
#define UNM_MSGQ_SRE_L1RE_XPQ(HDR)      UNM_MSGQ_HDR(HDR, UNM_MSGQ_SQM_TYPE1,UNM_MSGQ_XPQ,SRE_EXP)
#define	UNM_MSGQ_TUPLE_INUSE(HDR)	UNM_MSGQ_HDR(HDR, 4,9,POINTER)

#define	UNM_MSGQ_SRE_L2RE_IFQ(HDR)	UNM_MSGQ_HDR(HDR,12,0,SRE_FLOW)/*FIXED*/
#define	UNM_MSGQ_TIMER_EVENT(HDR)	UNM_MSGQ_HDR(HDR,12,0,TIMER_EVENT)/*FIXED*/

#define UNM_ADDR_USERQ1 10
#define UNM_ADDR_USERQ2 11
#define UNM_ADDR_USERQ3 12
#define UNM_ADDR_USERQ4 13
#define UNM_ADDR_USERQ5 14

/* generic ticket queues */
#define UNM_MSGQ_TICKET_USERQ1(HDR)     UNM_MSGQ_HDR(HDR,4,UNM_ADDR_USERQ1,POINTER)
#define UNM_MSGQ_TICKET_USERQ2(HDR)     UNM_MSGQ_HDR(HDR,4,UNM_ADDR_USERQ2,POINTER)
#define UNM_MSGQ_TICKET_USERQ3(HDR)     UNM_MSGQ_HDR(HDR,4,UNM_ADDR_USERQ3,POINTER)
#define UNM_MSGQ_TICKET_USERQ4(HDR)     UNM_MSGQ_HDR(HDR,4,UNM_ADDR_USERQ4,POINTER)
#define UNM_MSGQ_TICKET_USERQ5(HDR)     UNM_MSGQ_HDR(HDR,4,UNM_ADDR_USERQ5,POINTER)

/******************************************************************************
 *
 *     Pegasus Queue Usage
 *
 ******************************************************************************
 */


/* DMA request queue. */
#define HW_DMA0_REQ_MAJ  2
#define HW_DMA0_REQ_MIN  0
#define HW_DMA0_REQ_SUBQ 0
#define UNM_MSGQ_PEGNET_DMA0_MAJQ       HW_DMA0_REQ_MAJ
#define UNM_MSGQ_PEGNET_DMA0_MINQ       HW_DMA0_REQ_MIN
#define UNM_MSGQ_PEGNET_DMA0_SUBQ       HW_DMA0_REQ_SUBQ

#define RDMA_DOORBELL0                  3

/* DMA request queue. */
#define HW_DMA1_REQ_MAJ  0
#define HW_DMA1_REQ_MIN  7
#define HW_DMA1_REQ_SUBQ 1
#define UNM_MSGQ_PEGNET_DMA1_MAJQ       HW_DMA1_REQ_MAJ
#define UNM_MSGQ_PEGNET_DMA1_MINQ       HW_DMA1_REQ_MIN
#define UNM_MSGQ_PEGNET_DMA1_SUBQ       HW_DMA1_REQ_SUBQ

/* DMA request queue. */
#define HW_DMA2_REQ_MAJ  5
#define HW_DMA2_REQ_MIN  0
#define HW_DMA2_REQ_SUBQ 0
#define UNM_MSGQ_PEGNET_DMA2_MAJQ       HW_DMA2_REQ_MAJ
#define UNM_MSGQ_PEGNET_DMA2_MINQ       HW_DMA2_REQ_MIN
#define UNM_MSGQ_PEGNET_DMA2_SUBQ       HW_DMA2_REQ_SUBQ
#define DMA_XMIT_REQUEST_HDR(hdr)       UNM_LOCAL_MSGQ_HDR((hdr), \
                                            UNM_MSGQ_PEGNET_DMA2_MAJQ, \
                                            UNKNOWN)
#define UNM_LOCAL_QUEUE_UNUSED_1_MAJ   6
#define UNM_LOCAL_QUEUE_UNUSED_1_MIN   0
#define UNM_LOCAL_QUEUE_UNUSED_1_SUBQ  0


#define RDMA_DOORBELL1                  7

#define UNM_LOCAL_QUEUE_UNUSED_2_MAJ   8
#define UNM_LOCAL_QUEUE_UNUSED_2_MIN   0
#define UNM_LOCAL_QUEUE_UNUSED_2_SUBQ  0

#define UNM_NIC_DMA_XMIT_COMPL_MAJ      9
#define UNM_NIC_DMA_XMIT_COMPL_MIN      0
#define UNM_NIC_DMA_XMIT_COMPL_SUBQ     0

#if 0
#define UNM_NIC_DMA_RCVDESC_COMPLETION_MAJ 10
#define UNM_NIC_DMA_RCVDESC_COMPLETION_MIN  0
#define UNM_NIC_DMA_RCVDESC_COMPLETION_SUBQ 0
#endif

#define RDMA_DOORBELL2                  11

#define UNM_NIC_DMA_RCVDESC_COMPLETION_MAJ      PEG_PRIORITY_QMAJOR
#define UNM_NIC_DMA_RCVDESC_COMPLETION_MIN(I)   PEG2_PRIORITY_QMINOR
#define UNM_NIC_DMA_RCVDESC_COMPLETION_SUBQ     PEG_PRIORITY_SUBQ

#define UNM_NIC_DMA_RCVCTX_COMPLETION_MAJ       10
#define UNM_NIC_DMA_RCVCTX_COMPLETION_MIN       0
#define UNM_NIC_DMA_RCVCTX_COMPLETION_SUBQ      0

/* DMA request queue. */
#define HW_DMA3_REQ_MAJ  13
#define HW_DMA3_REQ_MIN  0
#define HW_DMA3_REQ_SUBQ 0
#define UNM_MSGQ_PEGNET_DMA3_MAJQ       HW_DMA3_REQ_MAJ
#define UNM_MSGQ_PEGNET_DMA3_MINQ       HW_DMA3_REQ_MIN
#define UNM_MSGQ_PEGNET_DMA3_SUBQ       HW_DMA3_REQ_SUBQ

/* XDMA request queue and enable bit. */
#define UNM_MSGQ_XDMA_REQ_MAJQ          14
#define UNM_MSGQ_XDMA_REQ_MINQ          0
#define UNM_MSGQ_XDMA_REQ_SUBQ          0
#define UNM_XDMA_ENABLE                 1

/* PEXQ Location */
#define UNM_MSGQ_PEXQ_REQ_MAJQ          4
#define UNM_MSGQ_PEXQ_REQ_MINQ          0
#define UNM_MSGQ_PEXQ_REQ_SUBQ          0

/* NIC EPG completion queue */
#define UNM_NIC_EPG_COMPL2_MAJOR        0
#define UNM_NIC_EPG_COMPL2_MINOR        5
#define UNM_NIC_EPG_COMPL2_SUBQ         1

#define UNM_NIC_EPG_COMPL_MAJOR         0
#define UNM_NIC_EPG_COMPL_MINOR         6
#define UNM_NIC_EPG_COMPL_SUBQ          1

#define UNM_NIC_EPG_COMPL_Q_LEN         1536

#define RDMA_DOORBELL3                  15

/* NIC doorbell queue */
#define UNM_NIC_DOORBELL_MAJOR          RDMA_DOORBELL3
#define UNM_NIC_DOORBELL_MINOR          0
#define UNM_NIC_DOORBELL_SUBQ           0

#define UNM_NIC_RX_DOORBELL_MAJOR       RDMA_DOORBELL2
#define UNM_NIC_RX_DOORBELL_MINOR       0
#define UNM_NIC_RX_DOORBELL_SUBQ        0

/* Primary malloc queues. */
#define UNM_MSGQ_PEGOS_MMGMT_Q0_MAJOR       4
#define UNM_MSGQ_PEGOS_MMGMT_Q0_SUBQ        1
#define UNM_MSGQ_PEGOS_MMGMT_Q0_MINOR       15

#define UNM_MSGQ_PEGOS_MMGMT_Q1_MAJOR       4
#define UNM_MSGQ_PEGOS_MMGMT_Q1_SUBQ        1
#define UNM_MSGQ_PEGOS_MMGMT_Q1_MINOR       4

#define UNM_MSGQ_PEGOS_MMGMT_Q2_MAJOR       4
#define UNM_MSGQ_PEGOS_MMGMT_Q2_SUBQ        1
#define UNM_MSGQ_PEGOS_MMGMT_Q2_MINOR       5

#define UNM_MSGQ_PEGOS_MMGMT_Q3_MAJOR       4
#define UNM_MSGQ_PEGOS_MMGMT_Q3_SUBQ        1
#define UNM_MSGQ_PEGOS_MMGMT_Q3_MINOR       6

/* Backup malloc queues. */
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q0_MAJOR    4
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q0_SUBQ     1
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q0_MINOR    7

#define UNM_MSGQ_PEGOS_MMGMT_BU_Q1_MAJOR    4
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q1_SUBQ     1
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q1_MINOR    8

#define UNM_MSGQ_PEGOS_MMGMT_BU_Q2_MAJOR    4
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q2_SUBQ     1
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q2_MINOR    9

#define UNM_MSGQ_PEGOS_MMGMT_BU_Q3_MAJOR    4
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q3_SUBQ     1
#define UNM_MSGQ_PEGOS_MMGMT_BU_Q3_MINOR    10

/* d3 message queue */
#define UNM_MSGQ_D3_MAJQ                    0
#define UNM_MSGQ_D3_SUBQ                    0
#define UNM_MSGQ_D3_MINQ                    0

#define SRE_FREE_PROXY_MAJOR PEG_PRIORITY_QMAJOR
#define SRE_FREE_PROXY_MINOR PEG0_PRIORITY_QMINOR
#define SRE_FREE_PROXY_SUBQ  PEG_PRIORITY_SUBQ

/* Timer setup. */
#define TIMER_SETUP_MAJQ        0
#define TIMER_SETUP_MINORQ      15
#define TIMER_SETUP_SUBQ        1
#define TIMER_SETUP_Q_LEN       256
#define UNM_MSGQ_TIMER_SETUP(HDR)       UNM_QHDR_CREATE(HDR,\
                                            TIMER_SETUP_MAJQ,\
                                            TIMER_SETUP_MINORQ,\
                                            TIMER_SETUP_SUBQ,\
                                            TIMER_SETUP)

/* REMOTE QUEUE, BLOCK 4 */
/* xg interrupt queue. */
#define UNM_NIC_XG_INT_MAJQ    0
#define UNM_NIC_XG_INT_MINQ    5
#define UNM_NIC_XG_INT_SUBQ    1

/* REMOTE QUEUE, BLOCK 8 */
/* Segment 1 contains the recveive queue and delay queue pools. */
#define RCVBUF_QMAJOR   8
#define RCVBUF_QMINOR   (0x10000 + UNM_ADDR_SQG2_NUM_TABOO_QUEUES)

#define DELAY_QMAJOR   8
#define DELAY_QMINOR   (0x10000+MAX_CONNECTIONS+UNM_ADDR_SQG2_NUM_TABOO_QUEUES)

/* Error queue */

/* Always avoid pure NIC receive path for now */
#if 1

#define SRE_CKSUM_ERRORQ_MAJ   12
#define SRE_CKSUM_ERRORQ_MIN   11
#define SRE_CKSUM_ERRORQ_SUBQ  1

#else

#define SRE_CKSUM_ERRORQ_MAJ   UNM_NIC_INPUT_MAJ
#define SRE_CKSUM_ERRORQ_MIN   UNM_NIC_INPUT_MIN
#define SRE_CKSUM_ERRORQ_SUBQ  UNM_NIC_INPUT_SUBQ

#endif

/* The empty queue is used for pausing dma, epg. MUST REMAIN EMPTY!! */
#define UNM_EMPTY_QUEUE_MAJ 0
#define UNM_EMPTY_QUEUE_MIN 10
#define UNM_EMPTY_QUEUE_SUBQ 1

#define EPG_BACKPRESSURE_REQ_MAJ	4
#define EPG_BACKPRESSURE_REQ_MIN	14
#define EPG_BACKPRESSURE_REQ_SUBQ	1

#define UNM_DUMMY_COMPL_Q_MAJ 	4
#define UNM_DUMMY_COMPL_Q_MIN 	13
#define UNM_DUMMY_COMPL_Q_SUBQ 	1

/* NIC QUEUES */
/* #define      NIC_LOCAL_QUEUE */
/* #ifdef NIC_LOCAL_QUEUE */
/* #define UNM_NIC_INPUT_MAJ   RDMA_DOORBELL0 */
/* #define UNM_NIC_INPUT_MIN   0 */
/* #define UNM_NIC_INPUT_SUBQ  0 */
/* #error "Should not be here" */
/* #else */
/* #error "Should not be here" */
#define UNM_NIC_INPUT_MAJ   NIC_PKTQ_MAJ
#define UNM_NIC_INPUT_MIN   NIC_PKTQ_MIN
#define UNM_NIC_INPUT_SUBQ  NIC_PKTQ_SUBQ
/* #endif */

//#define UNM_NIC_INPUT_Q_LEN 2048

#define UNM_NIC_TUPLE_MIN        (UNM_NIC_INPUT_MIN)
#define UNM_NIC_TUPLE_MAJ   (UNM_NIC_INPUT_MAJ >> 2)

#define UNM_NIC_DMA0_REQUEST_MAJ     HW_DMA2_REQ_MAJ
#define UNM_NIC_DMA0_REQUEST_MIN     HW_DMA2_REQ_MIN
#define UNM_NIC_DMA0_REQUEST_SUBQ    HW_DMA2_REQ_SUBQ

#define UNM_NIC_DMA1_REQUEST_MAJ     HW_DMA3_REQ_MAJ
#define UNM_NIC_DMA1_REQUEST_MIN     HW_DMA3_REQ_MIN
#define UNM_NIC_DMA1_REQUEST_SUBQ    HW_DMA3_REQ_SUBQ

#define UNM_NIC_DMA2_REQUEST_MAJ     HW_DMA1_REQ_MAJ
#define UNM_NIC_DMA2_REQUEST_MIN     HW_DMA1_REQ_MIN
#define UNM_NIC_DMA2_REQUEST_SUBQ    HW_DMA1_REQ_SUBQ

#define UNM_NIC_DMA_RCVDESC_REQUEST_MAJ    HW_DMA3_REQ_MAJ
#define UNM_NIC_DMA_RCVDESC_REQUEST_MIN    HW_DMA3_REQ_MIN
#define UNM_NIC_DMA_RCVDESC_REQUEST_SUBQ   HW_DMA3_REQ_SUBQ

#define UNM_NIC_DMA_XMIT_REQUEST_MAJ   HW_DMA3_REQ_MAJ
#define UNM_NIC_DMA_XMIT_REQUEST_MIN   HW_DMA3_REQ_MIN
#define UNM_NIC_DMA_XMIT_REQUEST_SUBQ  HW_DMA3_REQ_SUBQ

#define EPG_REQUEST_MAJ             1
#define EPG_REQUEST_MIN             0
#define EPG_REQUEST_SUBQ            0
#define EPG_REQUEST_Q_LEN           240

#if 1 // MMO: Until Rob says ok...
#define EPG_REQUEST1_MAJ            12
#define EPG_REQUEST1_MIN            0
#define EPG_REQUEST1_SUBQ           0
#define EPG_REQUEST1_Q_LEN          240

#define EPG_REQUEST2_MAJ            0
#define EPG_REQUEST2_MIN            11
#define EPG_REQUEST2_SUBQ           1
#define EPG_REQUEST2_Q_LEN          240

#define EPG_REQUEST3_MAJ            0
#define EPG_REQUEST3_MIN            12
#define EPG_REQUEST3_SUBQ           1
#define EPG_REQUEST3_Q_LEN          240
#else
#define EPG_REQUEST1_MAJ            1
#define EPG_REQUEST1_MIN            0
#define EPG_REQUEST1_SUBQ           0
#define EPG_REQUEST1_Q_LEN          512

#define EPG_REQUEST2_MAJ            1
#define EPG_REQUEST2_MIN            0
#define EPG_REQUEST2_SUBQ           0
#define EPG_REQUEST2_Q_LEN          256

#define EPG_REQUEST3_MAJ            1
#define EPG_REQUEST3_MIN            0
#define EPG_REQUEST3_SUBQ           0
#define EPG_REQUEST3_Q_LEN          256
#endif

#define UNM_NIC_EPG_INTERRUPT_MAJ      UNM_NIC_LW_INTERRUPT_MAJQ
#define UNM_NIC_EPG_INTERRUPT_MIN      UNM_NIC_LW_INTERRUPT_MINQ
#define UNM_NIC_EPG_INTERRUPT_SUBQ     UNM_NIC_LW_INTERRUPT_SUBQ

/* Major queue values for tuples */
#define UNM_TUPLE_MAJQ_0	0	/* major queue 0 */
#define UNM_TUPLE_MAJQ_12	3	/* major queue 12 */

#define UNM_SRE_TUPLE_SHIFT	2
						
#define PEG_MCAST_QUEUE_MAJOR   12

/*
 * CUT_THROUGH DEFINES.
 */
#define OCM_NONTCP_POOL_BASE            0
#define OCM_NONTCP_POOL_DEPTH           0x100
#define OCM_TCP_POOL_BASE               0x100
#define OCM_TCP_POOL_DEPTH              0
#define OCM_RDMA1_POOL_BASE             0x100
#define OCM_RDMA1_POOL_DEPTH            0
#define OCM_RDMA2_POOL_BASE             0x100
#define OCM_RDMA2_POOL_DEPTH            0
#define OCM_RDMA3_POOL_BASE             0x100
#define OCM_RDMA3_POOL_DEPTH            0
#define OCM_RDMA4_POOL_BASE             0x100
#define OCM_RDMA4_POOL_DEPTH            0
#define OCM_SW_POOL_BASE                0x100
#define OCM_SW_POOL_DEPTH               0

#define OCM_POOL_BASE_MSB              (UNM_ADDR_OCM0 >> 30)

#define OCM_UMMQ_PAGE_SIZE              0x40
#define OCM_SMMQ_PAGE_SIZE              0x40

#define UNM_NIC_CT_OUT_MAJ              0
#define UNM_NIC_CT_OUT_SUBQ             1
#define UNM_NIC_CT_OUT_MIN              9

#define UNM_NIC_CT_OVERFLOW_MAJ         0
#define UNM_NIC_CT_OVERFLOW_SUBQ        0
#define UNM_NIC_CT_OVERFLOW_MIN         0

#define UNM_OCM_MAJ                     14
#define UNM_OCM_SUBQ                    0
#define UNM_OCM_MIN                     0

#define UNM_OCM_OVERFLOW_MAJ            4
#define UNM_OCM_OVERFLOW_SUBQ           1
#define UNM_OCM_OVERFLOW_MIN            14

#define UNM_UMMQ_MAJ                    8
#define UNM_UMMQ_SUBQ                   1

#define UNM_NCSI_MAJ                    NIC_PKTQ_MAJ
#define UNM_NCSI_SUBQ                   NIC_PKTQ_SUBQ
#define UNM_NCSI_MIN                    NIC_PKTQ_MIN
#define UNM_NCSI_EPG_MIN(peg)           NIC_FLOWQ_MIN(peg, 1)
#define UNM_NCSI_Q_LEN			512

#define UNM_NCSI_RATELIMIT_MAJ		0
#define UNM_NCSI_RATELIMIT_MIN		14
#define UNM_NCSI_RATELIMIT_SUBQ		1
#define UNM_NCSI_RATELIMIT_Q_LEN	512

#define UNM_NCSI_BUF_BASE		(UNM_ADDR_QDR_21_BASE)
/* 1 EPG mbuf per sre buffer */
#define UNM_ADDR_NCSIBUF_SIZE		(UNM_ADDR_MBUF_UNIT * UNM_NCSI_Q_LEN)

/* Overrides RDMA queues */
#define UNM_NCSI_LOPOW_MAJ		7
#define UNM_NCSI_LOPOW_MIN		0
#define UNM_NCSI_LOPOW_SUBQ		0

/* nx iscsi work queue */
#define NX_ISCSI_WORK_MAJ		RDMA_DOORBELL0 
#define NX_ISCSI_WORK_MIN		0
#define NX_ISCSI_WORK_SUBQ		0
/*
 * The following are strap value to be driven onto the storage-side PCI
 * bus at reset.
 */
#define UNM_PS_DEVSEL_INIT_STRAP 1
#define UNM_PS_STOP_INIT_STRAP 1
#define UNM_PS_TRDY_INIT_STRAP 1

/*
 * Following defines will be used for MSI-X table and PBA offset/addr
 * calculations.
 */
#define UNM_MSIX_ENTRIES_PER_TBL        64
#define UNM_MSIX_ENTRY_SIZE     	16

#define UNM_MSIX_GET_ENTRY(BASE, IDX)           \
        (BASE + (IDX * UNM_MSIX_ENTRY_SIZE))
#define UNM_MSIX_GET_LO_ADDR(BASE, IDX) (UNM_MSIX_GET_ENTRY(BASE, IDX))
#define UNM_MSIX_GET_HI_ADDR(BASE, IDX) (UNM_MSIX_GET_ENTRY(BASE, IDX) + 4)
#define UNM_MSIX_GET_DATA(BASE, IDX)    (UNM_MSIX_GET_ENTRY(BASE, IDX) + 8)
#define UNM_MSIX_GET_VEC(BASE, IDX)     (UNM_MSIX_GET_ENTRY(BASE, IDX) + 12)

/*
 * Some general utility macros.
 * Note: This looks really dumb, but when applied to a #define'd constant,
 * the compiler will evaluate this at compile time and just give you the
 * single number that results.
 */
#define	UNM_LOG2_BIG(V)				\
    (((unm_dataword_t)(V) >= (1ULL << 47)) ? 47 : (	\
      ((unm_dataword_t)(V) >= (1ULL << 46)) ? 46 : (	\
       ((unm_dataword_t)(V) >= (1ULL << 45)) ? 45 : (	\
	((unm_dataword_t)(V) >= (1ULL << 44)) ? 44 : (	\
	 ((unm_dataword_t)(V) >= (1ULL << 43)) ? 43 : (	\
	  ((unm_dataword_t)(V) >= (1ULL << 42)) ? 42 : (	\
	   ((unm_dataword_t)(V) >= (1ULL << 41)) ? 41 : (	\
	    ((unm_dataword_t)(V) >= (1ULL << 40)) ? 40 : (	\
	     ((unm_dataword_t)(V) >= (1ULL << 39)) ? 39 : (	\
	      ((unm_dataword_t)(V) >= (1ULL << 38)) ? 38 : (	\
	       ((unm_dataword_t)(V) >= (1ULL << 37)) ? 37 : (	\
		((unm_dataword_t)(V) >= (1ULL << 36)) ? 36 : (	\
		 ((unm_dataword_t)(V) >= (1ULL << 35)) ? 35 : (	\
		  ((unm_dataword_t)(V) >= (1ULL << 34)) ? 34 : (	\
		   ((unm_dataword_t)(V) >= (1ULL << 33)) ? 33 : (	\
		    ((unm_dataword_t)(V) >= (1ULL << 32)) ? 32 : (	\
		     ((unm_dataword_t)(V) >= (1ULL << 31)) ? 31 : (	\
		      ((unm_dataword_t)(V) >= (1ULL << 30)) ? 30 : (	\
		       ((unm_dataword_t)(V) >= (1ULL << 29)) ? 29 : (	\
			((unm_dataword_t)(V) >= (1ULL << 28)) ? 28 : (	\
			 ((unm_dataword_t)(V) >= (1ULL << 27)) ? 27 : (	\
			  ((unm_dataword_t)(V) >= (1ULL << 26)) ? 26 : (	\
			   ((unm_dataword_t)(V) >= (1ULL << 25)) ? 25 : (	\
			    ((unm_dataword_t)(V) >= (1ULL << 24)) ? 24 : (	\
			     ((unm_dataword_t)(V) >= (1ULL << 23)) ? 23 : (	\
			      ((unm_dataword_t)(V) >= (1ULL << 21)) ? 21 : (	\
			       ((unm_dataword_t)(V) >= (1ULL << 20)) ? 20 : (	\
				((unm_dataword_t)(V) >= (1ULL << 19)) ? 19 : (	\
				 ((unm_dataword_t)(V) >= (1ULL << 18)) ? 18 : (	\
				  ((unm_dataword_t)(V) >= (1ULL << 17)) ? 17 : (	\
				   ((unm_dataword_t)(V) >= (1ULL << 16)) ? 16 : (	\
				    ((unm_dataword_t)(V) >= (1ULL << 15)) ? 15 : (	\
				     ((unm_dataword_t)(V) >= (1ULL << 14)) ? 14 : (	\
				      ((unm_dataword_t)(V) >= (1ULL << 13)) ? 13 : (	\
				       ((unm_dataword_t)(V) >= (1ULL << 12)) ? 12 : (	\
					((unm_dataword_t)(V) >= (1ULL << 11)) ? 11 : (	\
					 ((unm_dataword_t)(V) >= (1ULL << 10)) ? 10 : (	\
					  ((unm_dataword_t)(V) >= (1ULL <<  9)) ?  9 : (	\
					   ((unm_dataword_t)(V) >= (1ULL <<  8)) ?  8 : (	\
					    ((unm_dataword_t)(V) >= (1ULL <<  7)) ?  7 : (	\
					     ((unm_dataword_t)(V) >= (1ULL <<  6)) ?  6 : (	\
					      ((unm_dataword_t)(V) >= (1ULL <<  5)) ?  5 : (	\
					       ((unm_dataword_t)(V) >= (1ULL <<  4)) ?  4 : (	\
						((unm_dataword_t)(V) >= (1ULL <<  3)) ?  3 : (	\
						 ((unm_dataword_t)(V) >= (1ULL <<  2)) ?  2 : (	\
						  ((unm_dataword_t)(V) >= (1ULL <<  1)) ?  1 : 0))))))))))))))))))))))))))))))))))))))))))))))

#define	UNM_LOG2_SMALL(V)			\
    (((unsigned)(V) >= (1U << 31)) ? 31 : (	\
      ((unsigned)(V) >= (1U << 30)) ? 30 : (	\
       ((unsigned)(V) >= (1U << 29)) ? 29 : (	\
        ((unsigned)(V) >= (1U << 28)) ? 28 : (	\
	 ((unsigned)(V) >= (1U << 27)) ? 27 : (	\
	  ((unsigned)(V) >= (1U << 26)) ? 26 : (	\
	   ((unsigned)(V) >= (1U << 25)) ? 25 : (	\
	    ((unsigned)(V) >= (1U << 24)) ? 24 : (	\
	     ((unsigned)(V) >= (1U << 23)) ? 23 : (	\
	      ((unsigned)(V) >= (1U << 21)) ? 21 : (	\
	       ((unsigned)(V) >= (1U << 20)) ? 20 : (	\
	        ((unsigned)(V) >= (1U << 19)) ? 19 : (	\
		 ((unsigned)(V) >= (1U << 18)) ? 18 : (	\
		  ((unsigned)(V) >= (1U << 17)) ? 17 : (	\
		   ((unsigned)(V) >= (1U << 16)) ? 16 : (	\
		    ((unsigned)(V) >= (1U << 15)) ? 15 : (	\
		     ((unsigned)(V) >= (1U << 14)) ? 14 : (	\
		      ((unsigned)(V) >= (1U << 13)) ? 13 : (	\
		       ((unsigned)(V) >= (1U << 12)) ? 12 : (	\
		        ((unsigned)(V) >= (1U << 11)) ? 11 : (	\
			 ((unsigned)(V) >= (1U << 10)) ? 10 : (	\
			  ((unsigned)(V) >= (1U <<  9)) ?  9 : (	\
			   ((unsigned)(V) >= (1U <<  8)) ?  8 : (	\
			    ((unsigned)(V) >= (1U <<  7)) ?  7 : (	\
			     ((unsigned)(V) >= (1U <<  6)) ?  6 : (	\
			      ((unsigned)(V) >= (1U <<  5)) ?  5 : (	\
			       ((unsigned)(V) >= (1U <<  4)) ?  4 : (	\
			        ((unsigned)(V) >= (1U <<  3)) ?  3 : (	\
				 ((unsigned)(V) >= (1U <<  2)) ?  2 : (	\
				  ((unsigned)(V) >= (1U <<  1)) ?  1 : 0))))))))))))))))))))))))))))))


#endif /* __UNM_ADDRMAP_H */
