/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2003-2010 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

/*
 * $Id: lpfc_ioctl.h 3037 2007-05-22 14:02:22Z bsebastian $
 */

#ifndef _H_LPFC_IOCTL
#define _H_LPFC_IOCTL

/* Used for libdfc/lpfc driver rev lock*/
#define DFC_MAJOR_REV   4	
#define DFC_MINOR_REV	0	

#define LPFC_INQSN_SZ      64   /* Max size of Inquiry serial number */

#define LPFC_WWPN_TYPE		0
#define LPFC_PORTID_TYPE	1
#define LPFC_WWNN_TYPE		2

/** Bitmasks for interfaces supported with HBA in the on-line mode */
enum ondi_masks {
        ONDI_MBOX      = 0x1,     /* allows non-destructive mailbox commands    */
        ONDI_IOINFO    = 0x2,     /* supports retrieval of I/O info             */
        ONDI_LNKINFO   = 0x4,     /* supports retrieval of link info            */
        ONDI_NODEINFO  = 0x8,     /* supports retrieval of node info            */
	ONDI_TRACEINFO = 0x10,    /* supports retrieval of trace info           */
        ONDI_SETTRACE  = 0x20,    /* supports configuration of trace info       */
        ONDI_SLI1      = 0x40,    /* hardware supports SLI-1 interface          */
        ONDI_SLI2      = 0x80,    /* hardware supports SLI-2 interface          */
        ONDI_BIG_ENDIAN= 0x100,   /* DDI interface is BIG Endian                */
        ONDI_LTL_ENDIAN= 0x200,   /* DDI interface is LITTLE Endian             */
        ONDI_RMEM      = 0x400,   /* allows reading of adapter shared memory    */
        ONDI_RFLASH    = 0x800,   /* allows reading of adapter flash            */
        ONDI_RPCI      = 0x1000,  /* allows reading of adapter pci registers    */
        ONDI_RCTLREG   = 0x2000,  /* allows reading of adapter cntrol registers */
        ONDI_CFGPARAM  = 0x4000,  /* supports get/set configuration parameters  */
        ONDI_CT        = 0x8000,  /* supports passthru CT interface             */
        ONDI_HBAAPI    = 0x10000, /* supports HBA API interface                 */
        ONDI_SBUS      = 0x20000, /* supports SBUS adapter interface            */
        ONDI_FAILOVER  = 0x40000, /* supports adapter failover                  */
        ONDI_MPULSE    = 0x80000  /* This is a MultiPulse adapter               */
};

/** Bitmasks for interfaces supported with HBA in the off-line mode */
enum offdi_masks {
        OFFDI_MBOX     = 0x1,       /* allows all mailbox commands                */
        OFFDI_RMEM     = 0x2,       /* allows reading of adapter shared memory    */
        OFFDI_WMEM     = 0x4,       /* allows writing of adapter shared memory    */
        OFFDI_RFLASH   = 0x8,       /* allows reading of adapter flash            */
        OFFDI_WFLASH   = 0x10,      /* allows writing of adapter flash            */
        OFFDI_RPCI     = 0x20,      /* allows reading of adapter pci registers    */
        OFFDI_WPCI     = 0x40,      /* allows writing of adapter pci registers    */
        OFFDI_RCTLREG  = 0x80,      /* allows reading of adapter cntrol registers */
        OFFDI_WCTLREG  = 0x100,     /* allows writing of adapter cntrol registers */
        OFFDI_OFFLINE  = 0x80000000 /* if set, adapter is in offline state        */
};

/* Define values for SetDiagEnv flag */
enum set_diag_commands {
	DDI_SHOW       = 0x00,
	DDI_ONDI       = 0x01,
	DDI_OFFDI      = 0x02,
	DDI_WARMDI     = 0x03,
	DDI_DIAGDI     = 0x04,
	DDI_BRD_SHOW   = 0x10,
	DDI_BRD_ONDI   = 0x11,
	DDI_BRD_OFFDI  = 0x12,
	DDI_BRD_WARMDI = 0x13,
	DDI_BRD_DIAGDI = 0x14,
	DDI_UNUSED     = 0xFFFFFFFFL
};

/* Define the CfgParam structure shared between libdfc and the driver. */
#define MAX_CFG_PARAM 64
struct CfgParam {
	char    a_string[32];
	uint32_t a_low;
	uint32_t a_hi;
	uint32_t a_default;
	uint32_t a_current;
	uint16_t a_flag;
	uint16_t a_changestate;
	char    a_help[80];
};

#ifdef __KERNEL__
/* Define the CfgEntry structure shared between lpfc_attr and lpfc_ioctl. */
struct CfgEntry {
	struct CfgParam *entry;
	uint32_t (*getcfg)(struct lpfc_hba *phba);
	int (*setcfg)(struct lpfc_hba *phba, uint32_t val);
};

/* Define the CfgEntry structure shared between lpfc_attr and lpfc_ioctl. */
struct VPCfgEntry {
	struct CfgParam *entry;
	uint32_t (*getcfg)(struct lpfc_vport *vport);
	int (*setcfg)(struct lpfc_vport *vport, uint32_t val);
};

/* values for a_flag */
enum CfgParam_flag {
	CFG_EXPORT = 0x0001, /* Export this parameter to the end user */
	CFG_IGNORE = 0x0002, /* Ignore this parameter */
	CFG_THIS_HBA = 0x0004, /* Applicable to this HBA */
	CFG_COMMON = 0x0008, /* Common to all HBAs */
	CFG_SLI4   = 0x1000, /* SLI4 */
	CFG_SLI3   = 0x2000, /* SLI3 */
	CFG_FCOE   = 0x4000, /* FCoE */
	CFG_FC     = 0x8000  /* FC   */
};

enum CfgParam_changestate {
	CFG_REBOOT    = 0x0, /* Changes effective after system reboot */
	CFG_DYNAMIC   = 0x1, /* Changes effective immediately */
	CFG_RESTART   = 0x2, /* Changes effective after driver restart */
	CFG_LINKRESET = 0x3  /* Changes effective after link reset */
};

struct iocb_timeout_args {
	struct lpfc_hba   *phba;
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_iocbq *rspiocbq;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_dmabuf *db0;
	struct lpfc_dmabuf *db1;
	struct lpfc_dmabuf *db2;
	struct lpfc_dmabuf *db3;
	struct lpfc_dmabufext *dbext0;
	struct lpfc_dmabufext *dbext1;
	struct lpfc_dmabufext *dbext2;
	struct lpfc_dmabufext *dbext3;
};
typedef struct iocb_timeout_args IOCB_TIMEOUT_T;

/* Max depth of unsolicted event processing queue */
#define LPFC_IOCTL_MAX_EVENTS 128
#define LPFC_IOCTL_PAGESIZE   8192

#endif

/* Definition for LPFC_HBA_GET_EVENT events */
struct event_type {
	uint32_t mask;
	uint32_t cat;
	uint32_t sub;
};


/* Definitions for Temperature event processing */
struct temp_event {
	unsigned int event_type;	/* FC_REG_TEMPERATURE_EVENT */
	unsigned int event_code;	/* Criticality */
	unsigned int data;		/* Temperature */
}; 
typedef struct temp_event tempEvent_t;

#define LPFC_CRIT_TEMP          0x1
#define LPFC_THRESHOLD_TEMP     0x2
#define LPFC_NORMAL_TEMP        0x3

/* IOCTL LPFC_CMD Definitions */

/* Debug API - Range 0 - 99, x00 - x63 */
#define LPFC_LIP			0x01	/* Issue a LIP */ 
#define LPFC_SD_TEST			0x02	/* Inject SD Event */
#define LPFC_SET_SD_TEST		0x03	/* Set SD Testing */
#define LPFC_IOCTL_NODE_STAT		0x04	/* Get Node Stats */
#define LPFC_RESET			0x05	/* Reset the adapter */
#define LPFC_DEVP			0x0a	/* Get Device information */

/* Primary IOCTL CMD Definitions. Range 100 - 299, 0x64 - 0x12b */
#define LPFC_WRITE_PCI				0x64
#define LPFC_READ_PCI				0x65
#define LPFC_READ_MEM				0x66
#define LPFC_MBOX				0x67
#define LPFC_GET_DFC_REV			0x68
#define LPFC_WRITE_CTLREG			0x69
#define LPFC_READ_CTLREG			0x6a
#define LPFC_INITBRDS				0x6b
#define LPFC_SETDIAG				0x6c
#define LPFC_GETCFG				0x6d
#define LPFC_SETCFG				0x6e
#define LPFC_WRITE_MEM				0x6f
#define LPFC_GET_VPD				0x70
#define LPFC_GET_LPFCDFC_INFO			0x71
#define LPFC_GET_DUMPREGION			0x72
#define LPFC_LOOPBACK_TEST      		0x73
#define LPFC_LOOPBACK_MODE      		0x74
#define LPFC_TEMP_SENSOR_SUPPORT		0x75
#define LPFC_VPORT_GET_ATTRIB			0x76
#define LPFC_VPORT_GET_LIST			0x77
#define LPFC_VPORT_GET_RESRC			0x78
#define LPFC_VPORT_GET_NODE_INFO		0x79
#define LPFC_NPIV_READY				0x7a
#define LPFC_LINKINFO				0x7b
#define LPFC_IOINFO  				0x7c
#define LPFC_NODEINFO  				0x7d
#define LPFC_MENLO				0x7e
#define LPFC_CT					0x7f
#define LPFC_SEND_ELS				0x80
#define LPFC_HBA_SEND_SCSI			0x81
#define LPFC_HBA_SEND_FCP			0x82
#define LPFC_HBA_SET_EVENT			0x83
#define LPFC_HBA_GET_EVENT			0x84
#define LPFC_HBA_SEND_MGMT_CMD			0x85
#define LPFC_HBA_SEND_MGMT_RSP			0x86
#define LPFC_LIST_BIND                  	0x87
#define LPFC_VPORT_GETCFG               	0x88
#define LPFC_VPORT_SETCFG               	0x89
#define LPFC_HBA_UNSET_EVENT			0x8a

/* Start OC5.0 definitions. */
#define LPFC_HBA_ISSUE_MBX_RTRYV2          	0x90
#define LPFC_HBA_GET_PERSISTENT_LINK_DOWN  	0x91
#define LPFC_HBA_SET_PERSISTENT_LINK_DOWN  	0x92
#define LPFC_HBA_GET_FCF_LIST			0x93
#define LPFC_HBA_GET_PARAMS        		0x94
#define LPFC_HBA_SET_PARAMS                	0x95
#define LPFC_HBA_GET_FCF_CONN_LIST         	0x96
#define LPFC_HBA_SET_FCF_CONN_LIST         	0x97
#define LPFC_HBA_ISSUE_DMP_MBX             	0x98
#define LPFC_HBA_ISSUE_UPD_MBX			0x99
#define LPFC_GET_LOGICAL_LINK_SPEED		0x9a
#define LPFC_PRIMARY_IOCTL_RANGE_END		0x12b	/* End range. */

/*  HBAAPI IOCTL CMD Definitions Range 300 - 499, 0x12c - x1f3 */
#define LPFC_HBA_ADAPTERATTRIBUTES	0x12c	/* Get attributes of HBA */
#define LPFC_HBA_PORTATTRIBUTES		0x12d	/* Get attributes of HBA Port */
#define LPFC_HBA_PORTSTATISTICS		0x12e	/* Get statistics of HBA Port */
#define LPFC_HBA_DISCPORTATTRIBUTES	0x12f	/* Get attibutes of the discovered adapter Ports */
#define LPFC_HBA_WWPNPORTATTRIBUTES	0x130	/* Get attributes of the Port specified by WWPN */
#define LPFC_HBA_INDEXPORTATTRIBUTES	0x131	/* Get attributes of the Port specified by index */
#define LPFC_HBA_FCPTARGETMAPPING	0x132	/* Get info for all FCP tgt's */
#define LPFC_HBA_FCPBINDING		0x133	/* Binding info for FCP tgts */
#define LPFC_HBA_SETMGMTINFO		0x134	/* Sets driver values with default HBA_MGMTINFO vals */
#define LPFC_HBA_GETMGMTINFO		0x135	/* Get driver values for HBA_MGMTINFO vals */
#define LPFC_HBA_RNID			0x136	/* Send an RNID request */
#define LPFC_HBA_REFRESHINFO		0x137	/* Do a refresh of the stats */
#define LPFC_HBA_GETEVENT		0x138	/* Get HBAAPI event(s) */
/*  LPFC_LAST_IOCTL_USED 	        0x138       Last LPFC Ioctl used  */

#define INTERNAL_LOOP_BACK              0x1
#define EXTERNAL_LOOP_BACK              0x2
#define LOOPBACK_MAX_BUFSIZE            0x2000	/* 8192 (dec) bytes */
/* the DfcRevInfo structure */
struct DfcRevInfo {
	uint32_t a_Major;
	uint32_t a_Minor;
};

#define DFC_DRVID_STR_SZ 16
#define DFC_FW_STR_SZ 32

struct dfc_info {
	uint32_t a_pci;
	uint32_t a_busid;
	uint32_t a_devid;
	uint32_t a_ddi;
	uint32_t a_onmask;
	uint32_t a_offmask;
	uint8_t  a_drvrid[DFC_DRVID_STR_SZ];
	uint8_t  a_fwname[DFC_FW_STR_SZ];
	uint8_t  a_wwpn[8];
	uint8_t  a_pciFunc;
};

/* Define the idTypes for the nport_id. */
#define NPORT_ID_TYPE_WWPN 0
#define NPORT_ID_TYPE_DID  1
#define NPORT_ID_TYPE_WWNN 2

struct nport_id {
   uint32_t    idType;         /* 0 - wwpn, 1 - d_id, 2 - wwnn */
   uint32_t    d_id;
   uint8_t     wwpn[8];
};

#define LPFC_EVENT_LIP_OCCURRED		1
#define LPFC_EVENT_LINK_UP		2
#define LPFC_EVENT_LINK_DOWN		3
#define LPFC_EVENT_LIP_RESET_OCCURRED	4
#define LPFC_EVENT_RSCN			5
#define LPFC_EVENT_PROPRIETARY		0xFFFF

struct lpfc_hba_event_info {
	uint32_t event_code;
	uint32_t port_id;
	union {
		uint32_t rscn_event_info;
		uint32_t pty_event_info;
	} event;
};

/* Define the character device name. */
#define LPFC_CHAR_DEV_NAME "lpfcdfc"

/* Used for ioctl command */
#define LPFC_DFC_CMD_IOCTL_MAGIC 0xFC
#define LPFC_DFC_CMD_IOCTL _IOWR(LPFC_DFC_CMD_IOCTL_MAGIC, 0x1,\
		struct lpfcCmdInput)

/*
 * Diagnostic (DFC) Command & Input structures: (LPFC)
 */
struct lpfcCmdInput {
	short    lpfc_brd;
	short    lpfc_ring;
	short    lpfc_iocb;
	short    lpfc_flag;
	void    *lpfc_arg1;
	void    *lpfc_arg2;
	void    *lpfc_arg3;
	char    *lpfc_dataout;
	uint32_t lpfc_cmd;
	uint32_t lpfc_outsz;
	uint32_t lpfc_arg4;
	uint32_t lpfc_arg5;
	uint32_t lpfc_cntl;
	uint8_t  pad[4];
};

#if defined(CONFIG_COMPAT)
/* 32 bit version */
struct lpfcCmdInput32 {
	short    lpfc_brd;
	short    lpfc_ring;
	short    lpfc_iocb;
	short    lpfc_flag;
	u32	lpfc_arg1;
	u32	lpfc_arg2;
	u32     lpfc_arg3;
	u32     lpfc_dataout;
	uint32_t lpfc_cmd;
	uint32_t lpfc_outsz;
	uint32_t lpfc_arg4;
	uint32_t lpfc_arg5;
	uint32_t lpfc_cntl;
	uint8_t  pad[4];
};

#define LPFC_DFC_CMD_IOCTL32 _IOWR(LPFC_DFC_CMD_IOCTL_MAGIC, 0x1, \
		struct lpfcCmdInput32)
#endif


/*
 * Command input control definition.  Inform the driver the calling
 * application is running in i386, 32bit mode.
 */
#define LPFC_CNTL_X86_APP  0x01

#define SLI_CT_ELX_LOOPBACK 0x10
enum ELX_LOOPBACK_CMD {
	ELX_LOOPBACK_XRI_SETUP = 100,
	ELX_LOOPBACK_DATA      = 101
};

/*
 * Define the driver information structure. This structure is shared
 * with libdfc and has 32/64 bit alignment requirements.
 */
struct lpfc_dfc_drv_info {
	uint8_t  version[64];  /* Driver Version string */
	uint8_t  name[32];     /* Driver Name */
	uint32_t sliMode;      /* current operation SLI mode used */
	uint32_t align_1;
	uint64_t featureList;
	uint32_t hbaType;
	uint32_t align_2;
};

enum lpfc_dfc_drv_info_sliMode {
	FBIT_DIAG         = 0x0001, /* Diagnostic support */
	FBIT_LUNMAP       = 0x0002, /* LUN Mapping support */
	FBIT_DHCHAP       = 0x0004, /* Authentication/security support */
	FBIT_IKE          = 0x0008, /* Authentication/security support */
	FBIT_NPIV         = 0x0010, /* NPIV support */
	FBIT_RESET_WWN    = 0x0020, /* Driver supports resets to new WWN */
	FBIT_VOLATILE_WWN = 0x0040, /* Driver supports volitile WWN */
	FBIT_E2E_AUTH     = 0x0080, /* End-to-end authentication */
	FBIT_SD           = 0x0100, /* SanDiag support */
	FBIT_FCOE_SUPPORT = 0x0200, /* Driver supports FCoE if set */
	FBIT_PERSIST_LINK_SUPPORT = 0x0400, /* Link Persistence support */
	FBIT_TARGET_MODE_SUPPORT  = 0x0800, /* Target Mode Supported */
	FEATURE_LIST      = (FBIT_DIAG | FBIT_NPIV |  FBIT_RESET_WWN |
			     FBIT_VOLATILE_WWN | FBIT_SD)
};


/* Define the IO information structure */
struct lpfc_io_info {
	uint32_t a_mbxCmd;	/* mailbox commands issued */
	uint32_t a_mboxCmpl;	/* mailbox commands completed */
	uint32_t a_mboxErr;	/* mailbox commands completed, error status */
	uint32_t a_iocbCmd;	/* iocb command ring issued */
	uint32_t a_iocbRsp;	/* iocb rsp ring received */
	uint32_t a_adapterIntr;	/* adapter interrupt events */
	uint32_t a_fcpCmd;	/* FCP commands issued */
	uint32_t a_fcpCmpl;	/* FCP command completions received */
	uint32_t a_fcpErr;	/* FCP command completions errors */
	uint32_t a_seqXmit;	/* IP xmit sequences sent */
	uint32_t a_seqRcv;	/* IP sequences received */
	uint32_t a_bcastXmit;	/* cnt of successful xmit bcast cmds issued */
	uint32_t a_bcastRcv;	/* cnt of receive bcast cmds received */
	uint32_t a_elsXmit;	/* cnt of successful ELS req cmds issued */
	uint32_t a_elsRcv;	/* cnt of ELS request commands received */
	uint32_t a_RSCNRcv;	/* cnt of RSCN commands received */
	uint32_t a_seqXmitErr;	/* cnt of unsuccessful xmit bcast cmds issued */
	uint32_t a_elsXmitErr;	/* cnt of unsuccessful ELS req cmds issued  */
	uint32_t a_elsBufPost;	/* cnt of ELS buffers posted to adapter */
	uint32_t a_ipBufPost;	/* cnt of IP buffers posted to adapter */
	uint32_t a_cnt1;	/* generic counter */
	uint32_t a_cnt2;	/* generic counter */
	uint32_t a_cnt3;	/* generic counter */
	uint32_t a_cnt4;	/* generic counter */
};

/* Define the nodeinfo structure */
struct lpfc_node_info {
	uint16_t a_flag;
	uint16_t a_state;
	uint32_t a_did;
	uint8_t a_wwpn[8];
	uint8_t a_wwnn[8];
	uint32_t a_targetid;
};

/* Values for a_flag field in struct NODEINFO */
enum node_info_flags {
	NODE_RPI_XRI    = 0x1,    /* creating xri for entry             */
	NODE_REQ_SND    = 0x2,    /* sent ELS request for this entry    */
	NODE_ADDR_AUTH  = 0x4,    /* Authenticating addr for this entry */
	NODE_RM_ENTRY   = 0x8,    /* Remove this entry                  */
	NODE_FARP_SND   = 0x10,   /* sent FARP request for this entry   */
	NODE_FABRIC     = 0x20,   /* this entry represents the Fabric   */
	NODE_FCP_TARGET = 0x40,   /* this entry is an FCP target        */
	NODE_IP_NODE    = 0x80,   /* this entry is an IP node           */
	NODE_DISC_START = 0x100,  /* start discovery on this entry      */
	NODE_SEED_WWPN  = 0x200,  /* Entry scsi id is seeded for WWPN   */
	NODE_SEED_WWNN  = 0x400,  /* Entry scsi id is seeded for WWNN   */
	NODE_SEED_DID   = 0x800,  /* Entry scsi id is seeded for DID    */
	NODE_SEED_MASK  = 0xe00,  /* mask for seeded flags              */
	NODE_AUTOMAP    = 0x1000, /* This entry was automap'ed          */
	NODE_NS_REMOVED = 0x2000  /* This entry removed from NameServer */
};


/* Values for  a_state in struct lpfc_node_info */
enum node_info_states {
	NODE_UNUSED =  0,
	NODE_LIMBO  = 0x1, /* entry needs to hang around for wwpn / sid  */
	NODE_LOGOUT = 0x2, /* NL_PORT is not logged in - entry is cached */
	NODE_PLOGI  = 0x3, /* PLOGI was sent to NL_PORT                  */
	NODE_LOGIN  = 0x4, /* NL_PORT is logged in / login REG_LOGINed   */
	NODE_PRLI   = 0x5, /* PRLI was sent to NL_PORT                   */
	NODE_ALLOC  = 0x6, /* NL_PORT is  ready to initiate adapter I/O  */
	NODE_SEED   = 0x7  /* seed scsi id bind in table                 */
};

struct lpfc_link_info {
	uint32_t a_linkEventTag;
	uint32_t a_linkUp;
	uint32_t a_linkDown;
	uint32_t a_linkMulti;
	uint32_t a_DID;
	uint8_t a_topology;
	uint8_t a_linkState;
	uint8_t a_alpa;
	uint8_t a_alpaCnt;
	uint8_t a_alpaMap[128];
	uint8_t a_wwpName[8];
	uint8_t a_wwnName[8];
};

enum lpfc_link_info_topology {
	LNK_LOOP        = 0x1,
	LNK_PUBLIC_LOOP = 0x2,
	LNK_FABRIC      = 0x3,
	LNK_PT2PT       = 0x4
};

enum lpfc_link_info_linkState {
	LNK_DOWN        = 0x1,
	LNK_UP          = 0x2,
	LNK_FLOGI       = 0x3,
	LNK_DISCOVERY   = 0x4,
	LNK_REDISCOVERY = 0x5,
	LNK_READY       = 0x6,
	LNK_DOWN_PERSIST = 0x7
};

enum lpfc_host_event_code  {
	LPFCH_EVT_LIP            = 0x1,
	LPFCH_EVT_LINKUP         = 0x2,
	LPFCH_EVT_LINKDOWN       = 0x3,
	LPFCH_EVT_LIPRESET       = 0x4,
	LPFCH_EVT_RSCN           = 0x5,
	LPFCH_EVT_ADAPTER_CHANGE = 0x103,
	LPFCH_EVT_PORT_UNKNOWN   = 0x200,
	LPFCH_EVT_PORT_OFFLINE   = 0x201,
	LPFCH_EVT_PORT_ONLINE    = 0x202,
	LPFCH_EVT_PORT_FABRIC    = 0x204,
	LPFCH_EVT_LINK_UNKNOWN   = 0x500,
	LPFCH_EVT_VENDOR_UNIQUE  = 0xffff,
};

#define ELX_LOOPBACK_HEADER_SZ \
	(size_t)(&((struct lpfc_sli_ct_request *)NULL)->un)

struct lpfc_host_event {
	uint32_t seq_num;
	enum lpfc_host_event_code event_code;
	uint32_t data;
};

#ifdef __KERNEL__
struct lpfcdfc_host;

/* Initialize/Un-initialize char device */
int lpfc_cdev_init(void);
void lpfc_cdev_exit(void);
void lpfcdfc_host_del(struct lpfcdfc_host *);
struct lpfcdfc_host *lpfcdfc_host_add(struct pci_dev *, struct Scsi_Host *,
				      struct lpfc_hba *);
void lpfc_ioctl_timeout_iocb_cmpl(struct lpfc_hba *, struct lpfc_iocbq *,
				  struct lpfc_iocbq *);
int lpfc_process_ioctl_hbaapi(struct lpfc_hba *, struct lpfcCmdInput *);
int lpfc_process_ioctl_dfc(struct lpfc_hba *, struct lpfcCmdInput *);
void lpfc_ioctl_temp_event(struct lpfc_hba *, void *);
void lpfc_ioctl_dump_event(struct lpfc_hba *);
void lpfc_ioctl_rscn_event(struct lpfc_hba *, void *, uint32_t);
void lpfc_ioctl_linkstate_event(struct lpfc_hba *, uint32_t);

#endif	/* __KERNEL__ */

/*&&&PAE.  Begin 7.4 IOCTL inclusions.  Structure for OUTFCPIO command */
typedef struct dfcptr {
	uint32_t addrhi;
	uint32_t addrlo;
} dfcptr_t;

typedef struct dfcu64 {
	uint32_t hi;
	uint32_t lo;
} dfcu64_t;

typedef struct dfcringmask {
	uint8_t rctl;
	uint8_t type;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcringmask_t;

typedef struct dfcringinit {
	dfcringmask_t prt[LPFC_MAX_RING_MASK];
	uint32_t num_mask;
	uint32_t iotag_ctr;
	uint16_t numCiocb;
	uint16_t numRiocb;
	uint8_t  pad[4]; /* pad structure to 8 byte boundary */
} dfcringinit_t;

typedef struct dfcsliinit {
	dfcringinit_t ringinit[LPFC_MAX_RING];
	uint32_t num_rings;
	uint32_t sli_flag;
} dfcsliinit_t;

typedef struct dfcsliring {
	uint16_t txq_cnt;
	uint16_t txq_max;
	uint16_t txcmplq_cnt;
	uint16_t txcmplq_max;
	uint16_t postbufq_cnt;
	uint16_t postbufq_max;
	uint32_t missbufcnt;
	dfcptr_t cmdringaddr;
	dfcptr_t rspringaddr;
	uint8_t  rspidx;
	uint8_t  cmdidx;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcsliring_t;


typedef struct dfcslistat {
	dfcu64_t iocbEvent[LPFC_MAX_RING];
	dfcu64_t iocbCmd[LPFC_MAX_RING];
	dfcu64_t iocbRsp[LPFC_MAX_RING];
	dfcu64_t iocbCmdFull[LPFC_MAX_RING];
	dfcu64_t iocbCmdEmpty[LPFC_MAX_RING];
	dfcu64_t iocbRspFull[LPFC_MAX_RING];
	dfcu64_t mboxStatErr;
	dfcu64_t mboxCmd;
	dfcu64_t sliIntr;
	uint32_t errAttnEvent;
	uint32_t linkEvent;
	uint32_t mboxEvent;
	uint32_t mboxBusy;
} dfcslistat_t;

struct out_fcp_devp {
	uint16_t target;
	uint16_t lun;
	uint16_t tx_count;
	uint16_t txcmpl_count;
	uint16_t delay_count;
	uint16_t sched_count;
	uint16_t lun_qdepth;
	uint16_t current_qdepth;
	uint32_t qcmdcnt;
	uint32_t iodonecnt;
	uint32_t errorcnt;
	uint8_t  pad[4]; /* pad structure to 8 byte boundary */
};

/* Structure for VPD command */

struct vpd {
	uint32_t version;
#define VPD_VERSION1     1
	uint8_t  ModelDescription[256];    /* VPD field V1 */
	uint8_t  Model[80];                /* VPD field V2 */
	uint8_t  ProgramType[256];         /* VPD field V3 */
	uint8_t  PortNum[20];              /* VPD field V4 */
};

/* Structure used for transfering mailbox extension data */
struct ioctl_mailbox_ext_data {
	uint32_t in_ext_byte_len;
	uint32_t out_ext_byte_len;
	uint8_t  mbox_offset_word;
	uint8_t  mbox_extension_data[MAILBOX_EXT_SIZE];
};

typedef struct dfcsli {
	dfcsliinit_t sliinit;
	dfcsliring_t ring[LPFC_MAX_RING];
	dfcslistat_t slistat;
	dfcptr_t MBhostaddr;
	uint16_t mboxq_cnt;
	uint16_t mboxq_max;
	uint32_t fcp_ring;
} dfcsli_t;

typedef struct dfchba {
        dfcsli_t sli;
        uint32_t hba_state;
        uint32_t cmnds_in_flight;
        uint8_t fc_busflag;
        uint8_t  pad[3]; /* pad structure to 8 byte boundary */
} dfchba_t;


typedef struct dfcnodelist {
	uint32_t nlp_failMask;
	uint16_t nlp_type;
	uint16_t nlp_rpi;
	uint16_t nlp_state;
	uint16_t nlp_xri;
	uint32_t nlp_flag;
	uint32_t nlp_DID;
	uint32_t nlp_oldDID;
	uint8_t  nlp_portname[8];
	uint8_t  nlp_nodename[8];
	uint16_t nlp_sid;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcnodelist_t;

typedef struct dfcscsilun {
	dfcu64_t lun_id;
	uint32_t lunFlag;
	uint32_t failMask;
	uint8_t  InquirySN[LPFC_INQSN_SZ];
	uint8_t  Vendor[8];
	uint8_t  Product[16];
	uint8_t  Rev[4];
	uint8_t  sizeSN;
	uint8_t  pad[3]; /* pad structure to 8 byte boundary */
} dfcscsilun_t;


typedef struct dfcscsitarget {
	dfcptr_t context;
	uint16_t max_lun;
	uint16_t scsi_id;
	uint16_t targetFlags;
	uint16_t addrMode;
        uint16_t rptLunState;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcscsitarget_t;

typedef struct dfcstat {
	uint32_t elsRetryExceeded;
	uint32_t elsXmitRetry;
	uint32_t elsRcvDrop;
	uint32_t elsRcvFrame;
	uint32_t elsRcvRSCN;
	uint32_t elsRcvRNID;
	uint32_t elsRcvFARP;
	uint32_t elsRcvFARPR;
	uint32_t elsRcvFLOGI;
	uint32_t elsRcvPLOGI;
	uint32_t elsRcvADISC;
	uint32_t elsRcvPDISC;
	uint32_t elsRcvFAN;
	uint32_t elsRcvLOGO;
	uint32_t elsRcvPRLO;
	uint32_t elsRcvPRLI;
        uint32_t elsRcvRRQ;
	uint32_t elsRcvLIRR;
	uint32_t elsRcvRPS;
	uint32_t elsRcvRPL;
	uint32_t frameRcvBcast;
	uint32_t frameRcvMulti;
	uint32_t strayXmitCmpl;
	uint32_t frameXmitDelay;
	uint32_t xriCmdCmpl;
	uint32_t xriStatErr;
	uint32_t LinkUp;
	uint32_t LinkDown;
	uint32_t LinkMultiEvent;
	uint32_t NoRcvBuf;
	uint32_t fcpCmd;
	uint32_t fcpCmpl;
	uint32_t fcpRspErr;
	uint32_t fcpRemoteStop;
	uint32_t fcpPortRjt;
	uint32_t fcpPortBusy;
	uint32_t fcpError;
} dfcstats_t;


typedef struct dfc_vpqos {  /* temporary holding place */
       uint32_t  buf;
} DFC_VPQoS ;

#define VP_ATTRIB_VERSION       3 /* Data structure version */
#define VP_ATTRIB_NO_DELETE     1 /* VMware does not support VP Delete */
typedef struct dfc_VpAttrib {
	uint8_t ver;            /* [OUT]; set to VP_ATTRIB_VERSION*/
	uint8_t reserved1[3];
	HBA_WWN wwpn;           /* [OUT] virtual WWPN */
	HBA_WWN wwnn;           /* [OUT] virtual WWNN */
	char name[256];         /* [OUT] NS registered symbolic WWPN */
	uint32_t options;       /* Not Supported */
	uint32_t portFcId;     	/* [OUT] FDISC assigned DID to vport. */
	uint8_t state;          /* [OUT] */
	uint8_t restrictLogin;  /* Not Supported */
	uint8_t flags;		/* [OUT] for DFC_VPGetAttrib. */
	uint8_t reserved2;	/* Not used. */
	uint64_t buf;           /* [OUT] platform dependent specific info */
	DFC_VPQoS QoS;          /* Not Supported */
	HBA_WWN fabricName;     /* [OUT] Fabric WWN */
	uint32_t checklist;     /* Not Supported */
	uint8_t accessKey[32];  /* Not Supported*/
} DFC_VPAttrib;

enum dfc_VpAttrib_state {
	ATTRIB_STATE_UNKNOWN  = 0,
	ATTRIB_STATE_LINKDOWN = 1,
	ATTRIB_STATE_INIT     = 2,
	ATTRIB_STATE_NO_NPIV  = 3,
	ATTRIB_STATE_NO_RESRC = 4,
	ATTRIB_STATE_LOGO     = 5,
	ATTRIB_STATE_REJECT   = 6,
	ATTRIB_STATE_FAILED   = 7,
	ATTRIB_STATE_ACTIVE   = 8,
	ATTRIB_STATE_FAUTH    = 9
};

typedef struct dfc_NodeInfoEntry {
    uint32_t type ;
    HBA_SCSIID scsiId;
    HBA_FCPID fcpId ;
    uint32_t nodeState ;
    uint32_t reserved ;
} DFC_NodeInfoEntry;

/* To maintain backward compatibility with libdfc,
 * do not modify the NodeStateEntry structure. Bump
 * the NODE_STAT_VERSION and make a new structure,
 * DFC_NodeStatEntry_Vx where x is the next number.
 * Let libdfc figure out which version the driver supports
 * and act appropriately.
 */
#define NODE_STAT_VERSION 1
struct DFC_NodeStatEntry_V1 {
	HBA_WWN wwpn;
	HBA_WWN wwnn;
	HBA_UINT32 fc_did;
	HBA_UINT32 TargetNumber;
	HBA_UINT32 TargetQDepth;
	HBA_UINT32 TargetMaxCnt;
	HBA_UINT32 TargetActiveCnt;
	HBA_UINT32 TargetBusyCnt;
	HBA_UINT32 TargetFcpErrCnt;
	HBA_UINT32 TargetAbtsCnt;
	HBA_UINT32 TargetTimeOutCnt;
	HBA_UINT32 TargetNoRsrcCnt;
	HBA_UINT32 TargetInvldRpiCnt;
	HBA_UINT32 TargetLclRjtCnt;
	HBA_UINT32 TargetResetCnt;
	HBA_UINT32 TargetLunResetCnt;
} ;

enum dfc_NodeInfoEntry_type {
	NODE_INFO_TYPE_DID                 = 0x00,
	NODE_INFO_TYPE_WWNN                = 0x01,
	NODE_INFO_TYPE_WWPN                = 0x02,
	NODE_INFO_TYPE_AUTOMAP             = 0x04,
	NODE_INFO_TYPE_UNMASK_LUNS         = 0x08,
	NODE_INFO_TYPE_DISABLE_LUN_AUTOMAP = 0x10,
	NODE_INFO_TYPE_ALPA                = 0x20
};

enum dfc_NodeInfoEntry_nodeState {
	NODE_INFO_STATE_EXIST      = 0x01,
	NODE_INFO_STATE_READY      = 0x02,
	NODE_INFO_STATE_LINKDOWN   = 0x04,
	NODE_INFO_STATE_UNMAPPED   = 0x08,
	NODE_INFO_STATE_PERSISTENT = 0x10
};

typedef struct dfc_GetNodeInfo {
    uint32_t numberOfEntries ;  /* number of nodes */
    DFC_NodeInfoEntry nodeInfo[1];  /* start of the DFC_NodeInfo array */
} DFC_GetNodeInfo;

struct DFC_GetNodeStat {
    uint32_t version;           /* Version of DFC_NodeStatEntry */
    uint32_t numberOfEntries ;  /* number of nodes */
    struct DFC_NodeStatEntry_V1 nodeStat[1];  /* start of DFC_NodeInfo array */
};


typedef struct dfc_VpEntry {
        HBA_WWN wwpn; /* vport wwpn */
        HBA_WWN wwnn; /* vport wwnn */
        uint32_t PortFcId; /* DID from successful FDISC. */
} DFC_VPEntry;

typedef struct DFC_VPENTRYLIST {
    uint32_t numberOfEntries;
    DFC_VPEntry vpentry[1];
} DFC_VPEntryList;

typedef struct DFC_VPRESOURCE {
        uint32_t vlinks_max;
        uint32_t vlinks_inuse;
        uint32_t rpi_max;
        uint32_t rpi_inuse;
} DFC_VPResource;

typedef union IOARG{
	struct {
		HBA_WWN  vport_wwpn; /* Input Arg */
		HBA_WWN  targ_wwpn;  /* Input Arg */  
	}Iarg;

	struct {
		uint32_t rspcnt; /* Output Arg */
		uint32_t snscnt; /* Output Arg */
	}Oarg;
} IOargUn;

#define CHECKLIST_BIT_NPIV	0x0001	/* Set if driver NPIV enabled */
#define	CHECKLIST_BIT_SLI3	0x0002	/* Set if SLI-3 enabled */
#define CHECKLIST_BIT_HBA	0x0004	/* Set if HBA support NPIV */
#define CHECKLIST_BIT_RSRC	0x0008	/* Set if resources available*/
#define CHECKLIST_BIT_LINK	0x0010	/* Set if link up */
#define CHECKLIST_BIT_FBRC	0x0020	/* Set if point-to-point fabric connection */
#define CHECKLIST_BIT_FSUP	0x0040	/* Set if Fabric support NPIV */
#define CHECKLIST_BIT_NORSRC	0x0080	/* Set if FDISC fails w/no LS_RJT reason*/

/* SD DIAG EVENT DEBUG */
enum sd_inject_set {
	SD_INJECT_PLOGI =       1,
	SD_INJECT_PRLO  =       2,
	SD_INJECT_ADISC =       3,
	SD_INJECT_LSRJCT =      4,
	SD_INJECT_LOGO =	5,
	SD_INJECT_FBUSY =	6,
	SD_INJECT_PBUSY =	7,
	SD_INJECT_FCPERR =	8,
	SD_INJECT_QFULL =	9,
	SD_INJECT_DEVBSY =	10,
	SD_INJECT_CHKCOND =	11,
	SD_INJECT_LUNRST =	12,
	SD_INJECT_TGTRST =	13,
	SD_INJECT_BUSRST =	14,
	SD_INJECT_QDEPTH =	15,
	SD_INJECT_PERR =	16,
	SD_INJECT_ARRIVE =	17
};
#endif				/* _H_LPFC_IOCTL */
