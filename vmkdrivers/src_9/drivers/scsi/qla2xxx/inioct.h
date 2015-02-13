/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */

/*
 * File Name: inioct.h
 *
 * San/Device Management Ioctl Header
 * File is created to adhere to Solaris requirement using 8-space tabs.
 *
 * !!!!! PLEASE DO NOT REMOVE THE TABS !!!!!
 * !!!!! PLEASE NO SINGLE LINE COMMENTS: // !!!!!
 * !!!!! PLEASE NO MORE THAN 80 CHARS PER LINE !!!!!
 *
 *
 * Revision History:
 *
 * Rev. 0	June 15, 2001
 * YPL	- Created.
 *
 * Rev. 1	June 26, 2001
 * YPL	- Change the loop back structure and delete cc that is not used.
 *
 * Rev. 2	June 29, 2001
 * YPL	- Use new EXT_CC defines from exioct.h
 *
 * Rev. 3	July 12, 2001
 * RL	- Added definitions for loopback mbx command completion codes.
 *
 * Rev. 4	July 12, 2001
 * RL	- Added definitions for loopback mbx command completion codes.
 *
 * Rev. 5	October 9, 2002
 * AV	- Added definition for Read Option ROM IOCTL.
 *
 * Rev. 6	May 27, 2003
 * RL	- Modified loopback rsp buffer structure definition to add
 *        diagnostic Echo command support.
 *
 * Rev. 7	February 25, 2005
 * RL	- Added VPD get/update command codes.
 *
 * Rev. 8	March 30, 2005
 * RL	- Updated Option Rom Region definitions.
 *
 */

#ifndef	_INIOCT_H
#define	_INIOCT_H

#ifndef INT8
#define	INT8		int8_t
#endif

#ifndef INT16
#define	INT16		int16_t
#endif

#ifndef INT32
#define	INT32		int32_t
#endif

#ifndef UINT8
#define	UINT8		uint8_t
#endif

#ifndef UINT16
#define	UINT16		uint16_t
#endif

#ifndef UINT32
#define	UINT32		uint32_t
#endif

#ifndef UINT64
#define	UINT64		uint64_t
#endif

#ifndef UINT64_O
#define	UINT64_O	void *	/* old define for FC drivers */
#endif

/*
 * ***********************************************************************
 * X OS type definitions
 * ***********************************************************************
 */
#ifdef _MSC_VER						/* NT */
#pragma pack(1)
#endif

/*
 * ***********************************************************************
 * INT_IOCTL SubCode definition.
 * These macros are being used for setting SubCode field in EXT_IOCTL
 * structure.
 * ***********************************************************************
 */

/*
 * Currently supported DeviceControl / ioctl command codes
 */
#define	INT_CC_GET_PORT_STAT_FC		EXT_CC_RESERVED0A_OS
#define	INT_CC_LOOPBACK			EXT_CC_RESERVED0B_OS
#define	INT_CC_UPDATE_OPTION_ROM	EXT_CC_RESERVED0C_OS
#define	INT_CC_ADD_TARGET_DEVICE	EXT_CC_RESERVED0D_OS
#define	INT_CC_READ_NVRAM		EXT_CC_RESERVED0E_OS
#define	INT_CC_UPDATE_NVRAM		EXT_CC_RESERVED0F_OS
#define	INT_CC_SWAP_TARGET_DEVICE	EXT_CC_RESERVED0G_OS
#define	INT_CC_READ_OPTION_ROM		EXT_CC_RESERVED0H_OS
#define	INT_CC_GET_OPTION_ROM_LAYOUT	EXT_CC_RESERVED0I_OS
#define	INT_CC_GET_VPD			EXT_CC_RESERVED0J_OS
#define	INT_CC_UPDATE_VPD		EXT_CC_RESERVED0K_OS
#define INT_CC_GET_SFP_DATA		EXT_CC_RESERVED0L_OS
#define INT_CC_GET_FW_DUMP		EXT_CC_RESERVED0M_OS
#define INT_CC_PORT_PARAM		EXT_CC_RESERVED0N_OS
#define INT_CC_VF_COMMAND		EXT_CC_RESERVED0O_OS
#define INT_CC_A84_MGMT_COMMAND		EXT_CC_RESERVED0P_OS
#define INT_CC_RESET_FW_COMMAND		EXT_CC_RESERVED0Q_OS
#define INT_CC_FCP_PRIO_CFG		EXT_CC_RESERVED0R_OS
#define INT_CC_GET_BOARD_TEMP		EXT_CC_RESERVED0T_OS
#define	INT_CC_LEGACY_LOOPBACK		EXT_CC_RESERVED0Z_OS

/* NVRAM */
#define	INT_SC_NVRAM_HARDWARE		0	/* Save */
#define	INT_SC_NVRAM_DRIVER		1	/* Driver (Apply) */
#define	INT_SC_NVRAM_ALL		2	/* NVRAM/Driver (Save+Apply) */

#define OPTION_EXTERNAL_LOOPBACK	0xf2
/* Loopback */
typedef struct _INT_LOOPBACK_REQ_O
{
	UINT16 Options;				/* 2   */
	UINT32 TransferCount;			/* 4   */
	UINT32 IterationCount;			/* 4   */
	UINT64_O BufferAddress;			/* 8  */
	UINT32 BufferLength;			/* 4  */
	UINT16 Reserved[9];			/* 18  */
} INT_LOOPBACK_REQ_O, *PINT_LOOPBACK_REQ_O;	/* 408 */
typedef struct _INT_LOOPBACK_REQ
{
	UINT16 Options;				/* 2   */
	UINT32 TransferCount;			/* 4   */
	UINT32 IterationCount;			/* 4   */
	UINT64 BufferAddress;			/* 8  */
	UINT32 BufferLength;			/* 4  */
	UINT16 Reserved[9];			/* 18  */
} __attribute__((packed)) INT_LOOPBACK_REQ, *PINT_LOOPBACK_REQ;	/* 408 */

typedef struct _INT_LOOPBACK_RSP_O
{
	UINT64_O BufferAddress;			/* 8  */
	UINT32 BufferLength;			/* 4  */
	UINT16 CompletionStatus;		/* 2  */
	UINT16 CrcErrorCount;			/* 2  */
	UINT16 DisparityErrorCount;		/* 2  */
	UINT16 FrameLengthErrorCount;		/* 2  */
	UINT32 IterationCountLastError;		/* 4  */
	UINT8  CommandSent;			/* 1  */
	UINT8  Reserved1;			/* 1  */
	UINT16 Reserved2[7];			/* 16 */
} INT_LOOPBACK_RSP_O, *PINT_LOOPBACK_RSP_O;	/* 40 */
typedef struct _INT_LOOPBACK_RSP
{
	UINT64 BufferAddress;			/* 8  */
	UINT32 BufferLength;			/* 4  */
	UINT16 CompletionStatus;		/* 2  */
	UINT16 CrcErrorCount;			/* 2  */
	UINT16 DisparityErrorCount;		/* 2  */
	UINT16 FrameLengthErrorCount;		/* 2  */
	UINT32 IterationCountLastError;		/* 4  */
	UINT8  CommandSent;			/* 1  */
	UINT8  Reserved1;			/* 1  */
	UINT16 Reserved2[7];			/* 16 */
} __attribute__((packed)) INT_LOOPBACK_RSP, *PINT_LOOPBACK_RSP;	/* 40 */

/* definition for interpreting CompletionStatus values */
#define	INT_DEF_LB_COMPLETE	0x4000
#define INT_DEF_LB_ECHO_CMD_ERR 0x4005
#define	INT_DEF_LB_PARAM_ERR	0x4006
#define	INT_DEF_LB_LOOP_DOWN	0x400b
#define	INT_DEF_LB_CMD_ERROR	0x400c

/* definition for interpreting CommandSent field */
#define INT_DEF_LB_LOOPBACK_CMD 	0
#define INT_DEF_LB_ECHO_CMD		1

/* definition for option rom */
#define INT_OPT_ROM_REGION_NONE				0x00
#define INT_OPT_ROM_REGION_FW				0x01
				/* ISP2422/2432: Uncompressed FW */
#define INT_OPT_ROM_REGION_PHBIOS_FCODE_EFI_CFW		0x02
				/* ISP2300/2310/2312: BIOS
				 * w/pcihdr and compressed FW OR
				 * FCODE w/pcihdr and compressed FW OR
				 * EFI w/pcihdr and compressed FW
				 */
#define INT_OPT_ROM_REGION_PHEFI_PHECFW_PHVPD		0x03
				/* ISP2300/2310/2312 HP HBAs:
				 * EFI w/pcihdr and compressed
				 * FW w/pcihdr and VPD w/pcihdr
				 */
#define INT_OPT_ROM_REGION_PHBIOS_CFW			0x04
				/* ISP6212: BIOS w/pcihdr and
				 * compressed FW
				 */
#define INT_OPT_ROM_REGION_PHBIOS_PHFCODE_PHEFI_FW	0x05
				/* ISP2322: BIOS w/pcihdr and
				 * uncompressed FW OR
				 * FCODE w/pcihdr and uncompressed FW OR
				 * EFI w/pcihdr and uncompressed FW OR
				 * BIOS w/pcihdr and FCODE w/pcihdr
				 * and EFI w/pcihdr and uncompressed FW
				 */
#define INT_OPT_ROM_REGION_PHBIOS_FW			0x06
				/* ISP6322: BIOS w/pcihdr and
				 * uncompressed FW
				 */
#define INT_OPT_ROM_REGION_PHBIOS_PHFCODE_PHEFI		0x07
				/* ISP2422/2432: BIOS w/pcihdr OR
				 * FCODE w/pcihdr OR
				 * EFI w/pcihdr OR
				 * BIOS w/pcihdr and FCODE
				 * w/pcihdr and EFI w/pcihdr
				 */
#define INT_OPT_ROM_REGION_VPD_HBAPARAM		0x08
				/* ISP2532: VPD and NVRAM (HBA parameters)
				 */
#define INT_OPT_ROM_REGION_FW_DATA		0x13
				/* ISP2532: FW table */
#define	INT_OPT_ROM_REGION_NPIV_CONFIG_0	0x29
				/* ISP24XX/ISP2532: Port 0 NPIV config
				 * parameters
				 */
#define	INT_OPT_ROM_REGION_NPIV_CONFIG_1	0x2A
				/* ISP24XX/ISP2532: Port 1 NPIV config
				 * parameters
				 */
#define	INT_OPT_ROM_REGION_SERDES		0x2B
				/* ISP2532: Serdes parameters */
#define INT_OPT_ROM_REGION_MPI_RISC_FW	0X40
#define INT_OPT_ROM_REGION_EDC_PHY_FW	0X45

#define INT_OPT_ROM_REGION_ALL				0xFF
				/* Region that includes all regions */
#define INT_OPT_ROM_REGION_INVALID			0xFFFFFFFF
				/* Invalid region */

/* Image device id (PCI_DATA_STRUCTURE.DeviceId)  */

#define INT_PDS_DID_VPD		0x0001
#define INT_PDS_DID_ISP23XX_FW	0x0003

/* Image code type (PCI_DATA_STRUCTURE.CodeType) */

#define INT_PDS_CT_X86		0x0000
#define INT_PDS_CT_PCI_OPEN_FW	0x0001
#define INT_PDS_CT_HP_PA_RISC	0x0002
#define INT_PDS_CT_EFI		0x0003

// Last image indicator (PCI_DATA_STRUCTURE.Indicator)

#define INT_PDS_ID_LAST_IMAGE	0x80

typedef struct _INT_PCI_ROM_HEADER
{
    UINT16 Signature;       // 0xAA55
    UINT8  Reserved[0x16];
    UINT16 PcirOffset;      // Relative pointer to pci data structure

} INT_PCI_ROM_HEADER, *PINT_PCI_ROM_HEADER;
#define INT_PCI_ROM_HEADER_SIGNATURE	0xAA55
#define INT_PCI_ROM_HEADER_SIZE		sizeof(INT_PCI_ROM_HEADER)

typedef struct _INT_PCI_DATA_STRUCT
{
    UINT32 Signature;       // 'PCIR'
    UINT16 VendorId;
    UINT16 DeviceId;        // Image type
    UINT16 Reserved0;
    UINT16 Length;          // Size of this structure
    UINT8  Revision;
    UINT8  ClassCode[3];
    UINT16 ImageLength;     // Total image size (512 byte segments)
    UINT16 CodeRevision;
    UINT8  CodeType;
    UINT8  Indicator;       // 0x80 indicates last this is image
    UINT16 Reserved1;
} INT_PCI_DATA_STRUCT, *PINT_PCI_DATA_STRUCT;
#define INT_PCI_DATA_STRUCT_SIGNATURE   0x52494350 // "PCIR"
#define INT_PCI_DATA_STRUCT_SIZE        sizeof(INT_PCI_DATA_STRUCT)

#define INT_PCI_IMAGE_SECTOR_SIZE       512
#define INT_PCI_MULTIBOOT_MAXIMUM_SIZE  0x40000

#define INT_PCI_IMAGE_MINIMUM_SIZE      \
        (INT_PCI_ROM_HEADER_SIZE + INT_PCI_DATA_STRUCT_SIZE)

#define INT_PCI_IMAGE_MAXIMUM_START_OFFSET      \
        (INT_PCI_MULTIBOOT_MAXIMUM_SIZE - INT_PCI_IMAGE_MINIMUM_SIZE)

#define INT_FIRMWARE_START_OFFSET       0x80000


typedef struct _INT_LZHEADER
{
    UINT16 LzMagicNum;      // 'LZ'
    UINT16 Reserved1;
    UINT32 CompressedSize;
    UINT32 UnCompressedSize;
    struct
    {
        UINT16 sub;
        UINT16 minor;
        UINT16 majorLo;
        UINT16 majorHi;     // Usually always zero
    } RiscFwRev;
    UINT8 Reserved2[12];
} INT_LZHEADER, *PINT_LZHEADER;
#define INT_PCI_FW_LZ_HEADER_SIGNATURE  0x5A4C // "LZ"

typedef struct _INT_OPT_ROM_REGION
{
    UINT32  Region;
    UINT32  Size;
    UINT32  Beg;
    UINT32  End;
} INT_OPT_ROM_REGION, *PINT_OPT_ROM_REGION;

typedef struct _INT_OPT_ROM_LAYOUT
{
    UINT32      Size;			// Size of the option rom
    UINT32      NoOfRegions;
    INT_OPT_ROM_REGION	Region[1];
} INT_OPT_ROM_LAYOUT, *PINT_OPT_ROM_LAYOUT;

#define INT_OPT_ROM_MAX_REGIONS     0xF
#define INT_OPT_ROM_SIZE_2312       0x20000     /* 128k */
#define INT_OPT_ROM_SIZE_2322       0x100000    /* 1 M  */
#define INT_OPT_ROM_SIZE_2422       0x100000    /* 1 M  */
#define INT_OPT_ROM_SIZE_25XX       0x100000    /* 1 M  */

typedef struct _OPT_ROM_TABLE
{
	INT_OPT_ROM_REGION  Region;
} OPT_ROM_TABLE, *POPT_ROM_TABLE;

typedef struct _INT_PORT_PARAM {
	EXT_DEST_ADDR FCScsiAddr;
	UINT16 Mode;
	UINT16 Speed;
} INT_PORT_PARAM, *PINT_PORT_PARAM;

#define INT_SC_A84_RESET            1  /* Reset */
#define INT_SC_A84_GET_FW_VERSION   2  /* Chip FW versions */
#define INT_SC_A84_UPDATE_FW        3  /* Update (Diagnostic or * Operation) Firmware */
#define INT_SC_A84_MANAGE_INFO      4  /* Manage Chip */


typedef struct _A84_RESET {
	UINT16 Flags;
	UINT16 Reserved;
#define A84_RESET_FLAG_ENABLE_DIAG_FW   1
} __attribute__((packed)) A84_RESET, *PA84_RESET;

typedef struct _A84_GET_FW_VERSION {
	UINT32 FwVersion;
} __attribute__((packed)) A84_GET_FW_VERSION, *PA84_GET_FW_VERSION;

typedef struct _A84_UPDATE_FW {
        UINT16 Flags;
#define A84_UPDATE_FW_FLAG_DIAG_FW  0x0008  /* if flag is cleared then it
					       * must be an operation fw */
	UINT16 Reserved;
	UINT32 TotalByteCount;
	UINT64 pFwDataBytes;
} __attribute__((packed)) A84_UPDATE_FW, *PA84_UPDATE_FW;

typedef struct _A84_ACCESS_PARAMETERS {
	union {
		struct {
			UINT32 StartingAddr;
			UINT32 Reserved2;
			UINT32 Reserved3;
		} Memory;   /* For Read & Write Memory */

		struct {
			UINT32 ConfigParamID;
#define CONFIG_PARAM_ID_RESERVED    1
#define CONFIG_PARAM_ID_UIF         2
#define CONFIG_PARAM_ID_FCOE_COS    3
#define CONFIG_PARAM_ID_PAUSE_TYPE  4
#define CONFIG_PARAM_ID_TIMEOUTS    5
			UINT32 ConfigParamData0;
			UINT32 ConfigParamData1;
		} Config;  /* For change Configuration */

		struct {
			UINT32 InfoDataType;
#define INFO_DATA_TYPE_CONFIG_LOG_DATA    1  /* Fetch Configuration Log Data */
#define INFO_DATA_TYPE_LOG_DATA           2  /* Fetch Log Data */
#define INFO_DATA_TYPE_PORT_STATISTICS    3  /* Fetch Port Statistics */
#define INFO_DATA_TYPE_LIF_STATISTICS     4  /* Fetch LIF Statistics */
#define INFO_DATA_TYPE_ASIC_STATISTICS    5  /* Fetch ASIC Statistics */
#define INFO_DATA_TYPE_CONFIG_PARAMETERS  6  /* Fetch Config Parameters */
#define INFO_DATA_TYPE_PANIC_LOG          7  /* Fetch Panic Log */
			UINT32 InfoContext;
			/* InfoContext defines for INFO_DATA_TYPE_LOG_DATA */
#define IC_LOG_DATA_LOG_ID_DEBUG_LOG                    0
#define IC_LOG_DATA_LOG_ID_LEARN_LOG                    1
#define IC_LOG_DATA_LOG_ID_FC_ACL_INGRESS_LOG           2
#define IC_LOG_DATA_LOG_ID_FC_ACL_EGRESS_LOG            3
#define IC_LOG_DATA_LOG_ID_ETHERNET_ACL_INGRESS_LOG     4
#define IC_LOG_DATA_LOG_ID_ETHERNET_ACL_EGRESS_LOG      5
#define IC_LOG_DATA_LOG_ID_MESSAGE_TRANSMIT_LOG         6
#define IC_LOG_DATA_LOG_ID_MESSAGE_RECEIVE_LOG          7
#define IC_LOG_DATA_LOG_ID_LINK_EVENT_LOG               8
#define IC_LOG_DATA_LOG_ID_DCX_LOG                      9

			/* InfoContext defines for
			 * INFO_DATA_TYPE_PORT_STATISTICS */
#define IC_PORT_STATISTICS_PORT_NUMBER_ETHERNET_PORT0   0
#define IC_PORT_STATISTICS_PORT_NUMBER_ETHERNET_PORT1   1
#define IC_PORT_STATISTICS_PORT_NUMBER_NSL_PORT0        2
#define IC_PORT_STATISTICS_PORT_NUMBER_NSL_PORT1        3
#define IC_PORT_STATISTICS_PORT_NUMBER_FC_PORT0         4
#define IC_PORT_STATISTICS_PORT_NUMBER_FC_PORT1         5

			/* InfoContext defines for
			 * INFO_DATA_TYPE_LIF_STATISTICS */
#define IC_LIF_STATISTICS_LIF_NUMBER_ETHERNET_PORT0     0
#define IC_LIF_STATISTICS_LIF_NUMBER_ETHERNET_PORT1     1
#define IC_LIF_STATISTICS_LIF_NUMBER_FC_PORT0           2
#define IC_LIF_STATISTICS_LIF_NUMBER_FC_PORT1           3
#define IC_LIF_STATISTICS_LIF_NUMBER_CPU                6
			UINT32 Reserved;
		} Info; /* For fetch Info */
	} ap;
} __attribute__((packed)) A84_ACCESS_PARAMETERS, *PA84_ACCESS_PARAMETERS;

typedef struct _A84_MANAGE_INFO {
        UINT16 Operation;
#define A84_OP_READ_MEM         0  /* Read CS84XX Memory */
#define A84_OP_WRITE_MEM        1  /* Write CS84XX Memory */
#define A84_OP_CHANGE_CONFIG    2  /* Change Configuration */
#define A84_OP_GET_INFO         3  /* Fetch CS84XX Info (Logs,
				      & Statistics, Configuration) */
	UINT16 Reserved;
	A84_ACCESS_PARAMETERS Parameters;
	UINT32 TotalByteCount;
#define INFO_DATA_TYPE_LOG_CONFIG_TBC      ((10*7)+1)*4
#define INFO_DATA_TYPE_PORT_STAT_ETH_TBC   0x194
#define INFO_DATA_TYPE_PORT_STAT_FC_TBC    0xC0
#define INFO_DATA_TYPE_LIF_STAT_TBC        0x40
#define INFO_DATA_TYPE_ASIC_STAT_TBC       0x5F8
#define INFO_DATA_TYPE_CONFIG_TBC          0x140

	UINT64 pDataBytes;
} __attribute__((packed)) A84_MANAGE_INFO, *PA84_MANAGE_INFO;

#define A84_FC_CHECKSUM_FAILURE          0x01
#define A84_FC_INVALID_LENGTH            0x02
#define A84_FC_INVALID_ADDRESS           0x04
#define A84_FC_INVALID_CONFIG_ID_TYPE    0x05
#define A84_FC_INVALID_CONFIG_DATA       0x06
#define A84_FC_INVALID_INFO_CONTEXT      0x07

typedef struct _SD_A84_MGT{
	union {
		A84_RESET          Reset;
		A84_GET_FW_VERSION GetFwVer;
		A84_UPDATE_FW      UpdateFw;
		A84_MANAGE_INFO    ManageInfo;
	} sp;
} __attribute__((packed)) SD_A84_MGT, *PSD_A84_MGT;

struct a84_mgmt_request {
	union {
		struct verify_chip_entry_84xx request;
		struct verify_chip_rsp_84xx response;
		struct access_chip_84xx mgmt_request;
		struct access_chip_rsp_84xx mgmt_response;
	} p;
};

/* Management commands and data structures */

#define A84_ISSUE_WRITE_TYPE_CMD        0
#define A84_ISSUE_READ_TYPE_CMD         1
#define A84_CLEANUP_CMD                 2
#define A84_ISSUE_RESET_OP_FW           3
#define A84_ISSUE_RESET_DIAG_FW         4
#define A84_ISSUE_UPDATE_OPFW_CMD       5
#define A84_ISSUE_UPDATE_DIAGFW_CMD     6

struct qla_cs84xx_mgmt {
	uint16_t options;
#define	DUMP_A84_MEMORY		0
#define	LOAD_A84_MEMORY		1
#define	CHANGE_A84_CONFIG_PARAM	2
#define	REQUEST_A84_INFO		3

	uint32_t parameter1;
	uint32_t parameter2;
	uint32_t parameter3;

	char     *data;
	uint32_t data_size;
	dma_addr_t dseg_dma;
	uint8_t  current_state;
#define A84_CMD_STATE_NONE              0
#define A84_CMD_STATE_PROCESSING        1
#define A84_CMD_STATE_DONE              2

	uint8_t  cmd_valid;
	uint8_t  flags;
#define	A84_FLAGS_PAGE_ALLOC		1
};

/* 81XX IOCTL SubCodes */
#define INT_SC_RESET_FC_FW	0x01
#define INT_SC_RESET_MPI_FW	0x02

#ifdef _MSC_VER
#pragma pack()
#endif

#endif /* _INIOCT_H */
