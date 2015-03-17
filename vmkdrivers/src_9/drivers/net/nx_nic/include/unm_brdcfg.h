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
/******************************************************************************
*    unm_bdinfo.h - Phantom board information stored in flash.
*
*******************************************************************************
*/
#ifndef __UNM_BRDINFO_H
#define __UNM_BRDINFO_H
/* The version of the main data structure */
#define   UNM_BDINFO_VERSION 1

/* Magic number to let user know flash is programmed */
#define   UNM_BDINFO_MAGIC 0x12345678

#define P3_CHIP 3
#define NX_P3_A0		0x30
#define NX_P3_A2		0x32
#define NX_P3_B0		0x40
#define NX_P3_B1		0x41
#define NX_P3_B2		0x42

#define	NX_IS_REVISION_P3(REVISION)	(REVISION >= NX_P3_A0)

/* MN */
#define UNM_MEM_MAX_SLOTS	2

#define NX_LICENSE_SIZE		4096

#if defined(P3_LEGACY)
#define BRD_TYPE_MEM_ADDR	0x7f00000
#define MAC_MEM_ADDR		0x7f00010
#else
#define BRD_TYPE_MEM_ADDR	0x3f00000
#define MAC_MEM_ADDR		0x3f00010
#endif

typedef enum {
    UNM_BRDTYPE_P1_BD   = 0x0000,
    UNM_BRDTYPE_P1_SB   = 0x0001,
    UNM_BRDTYPE_P1_SMAX = 0x0002,
    UNM_BRDTYPE_P1_SOCK = 0x0003,

    UNM_BRDTYPE_P2_SOCK_31  =  0x0008,
    UNM_BRDTYPE_P2_SOCK_35  =  0x0009,
    UNM_BRDTYPE_P2_SB35_4G  =  0x000a,
    UNM_BRDTYPE_P2_SB31_10G =  0x000b,
    UNM_BRDTYPE_P2_SB31_2G  =  0x000c,

    UNM_BRDTYPE_P2_SB31_10G_IMEZ =  0x000d,
    UNM_BRDTYPE_P2_SB31_10G_HMEZ =  0x000e,
    UNM_BRDTYPE_P2_SB31_10G_CX4  =  0x000f,

    UNM_BRDTYPE_P3_REF_QG        =  0x0021,  /* Reference quad gig */
    UNM_BRDTYPE_P3_HMEZ          =  0x0022,
    UNM_BRDTYPE_P3_10G_CX4_LP    =  0x0023,  /* Dual CX4 - Low Profile - Red card */
    UNM_BRDTYPE_P3_4_GB          =  0x0024,
    UNM_BRDTYPE_P3_IMEZ          =  0x0025,
    UNM_BRDTYPE_P3_10G_SFP_PLUS  =  0x0026,
    UNM_BRDTYPE_P3_10000_BASE_T  =  0x0027,
    UNM_BRDTYPE_P3_XG_LOM        =  0x0028,

    UNM_BRDTYPE_P3_4_GB_MM       =  0x0029,     /*  NC375i */
    UNM_BRDTYPE_P3_10G_CX4       =  0x0031,     /* Reference CX4 */
    UNM_BRDTYPE_P3_10G_XFP       =  0x0032,     /* Reference XFP */

    UNM_BRDTYPE_P3_10G_TP   =  0x0080,	/* NC375i with NC524SFP */

} unm_brdtype_t;

typedef enum {
    	UNM_NIC_GBE  = 1,
	UNM_NIC_XGBE = 2
} unm_brdclass_t;

#ifdef P3
typedef struct {
    /* Per port bit array. */
    unsigned long    is_10G;
} nx_card_flags_t;
#else
typedef struct {
    long    is_10G:1;           /* = 1 for 10G type cards */
} nx_card_flags_t;
#endif

typedef enum {
	NX_UNKNOWN_TYPE_ROMIMAGE = 0,
	NX_P2_MN_TYPE_ROMIMAGE = 1,
	NX_P3_CT_TYPE_ROMIMAGE,
	NX_P3_MN_TYPE_ROMIMAGE,
	NX_P3_MS_TYPE_ROMIMAGE,
	NX_UNKNOWN_TYPE_ROMIMAGE_LAST,
} nx_fw_type_t;

#define NX_INVALID_ROMIMAGE	"INVALID"
#define NX_P2_MN_ROMIMAGE	"nxromimg.bin"
#define NX_P3_CT_ROMIMAGE	"nx3fwct.bin"
#define NX_P3_MN_ROMIMAGE	"nx3fwmn.bin"
#define NX_P3_MS_ROMIMAGE	"nx3fwms.bin"

#define	NX_ROMIMAGE_NAME_ARRAY			\
{						\
	NX_INVALID_ROMIMAGE,			\
	NX_P2_MN_ROMIMAGE,			\
	NX_P3_CT_ROMIMAGE,			\
	NX_P3_MN_ROMIMAGE,			\
	NX_P3_MS_ROMIMAGE			\
}

/* board type specific information */
typedef struct {
    unm_brdtype_t brdtype;                    /* type of board */
    long          ports;                      /* max no of physical ports */
	nx_fw_type_t	fwtype;		/* The FW Associated with board type */
    char          *short_name;
} unm_brdinfo_t;

#include "driver_info.h"

#define NUM_SUPPORTED_BOARDS (sizeof(unm_boards)/sizeof(unm_brdinfo_t))

#define GET_BRD_PORTS_NAME_BY_TYPE(type, ports, name)   \
{                                                       \
    int i;                                   		\
    ports = 0;                                      	\
    name = "Unknown";                               	\
    for (i = 0; i < NUM_SUPPORTED_BOARDS; ++i) {        \
        if (unm_boards[i].brdtype == type) {            \
            ports = unm_boards[i].ports;                \
            name = unm_boards[i].short_name;            \
            break;                                      \
        }                                               \
    }                                                   \
}

#define GET_BRD_PORTS_BY_TYPE(type, ports)		\
{							\
    int i;						\
							\
    ports = 0;						\
    for (i = 0; i < NUM_SUPPORTED_BOARDS; ++i) {	\
        if (unm_boards[i].brdtype == type) {		\
            ports = unm_boards[i].ports;		\
            break;					\
        }						\
    }							\
}

#define GET_BRD_NAME_BY_TYPE(type, name)            \
{                                                   \
    int i;		                            \
    name = "Unknown";                               \
    for (i = 0; i < NUM_SUPPORTED_BOARDS; ++i) {    \
        if (unm_boards[i].brdtype == type) {        \
            name = unm_boards[i].short_name;        \
            break;                                  \
        }                                           \
    }                                               \
}

#define GET_BRD_CLASS_BY_TYPE(brdtype, ports, brdclass)	\
{							\
	brdclass = UNM_NIC_XGBE;			\
        switch(brdtype) {				\
	case UNM_BRDTYPE_P3_HMEZ:			\
	case UNM_BRDTYPE_P3_XG_LOM:			\
	case UNM_BRDTYPE_P3_10G_CX4:			\
	case UNM_BRDTYPE_P3_10G_CX4_LP:			\
	case UNM_BRDTYPE_P3_IMEZ:			\
	case UNM_BRDTYPE_P3_10G_SFP_PLUS:		\
	case UNM_BRDTYPE_P3_10G_XFP:			\
	case UNM_BRDTYPE_P3_10000_BASE_T:		\
                brdclass = UNM_NIC_XGBE;		\
                break;					\
	case UNM_BRDTYPE_P3_REF_QG:			\
	case UNM_BRDTYPE_P3_4_GB:			\
	case UNM_BRDTYPE_P3_4_GB_MM:			\
		brdclass = UNM_NIC_GBE;			\
		break;					\
	case UNM_BRDTYPE_P3_10G_TP:		\
                if (ports < 2) {			\
                        brdclass = UNM_NIC_XGBE;	\
                } else {				\
		        brdclass = UNM_NIC_GBE;		\
                }					\
		break;					\
        default:					\
                printf("%s: Unknown board: 0x%x\n",	\
			__FUNCTION__, brdtype);		\
 		abort();				\
        }						\
}

#define GET_BRD_FWTYPE_BY_BRDTYPE(BRDTYPE, FWTYPE)			\
{									\
	int	INDEX;							\
									\
	FWTYPE = NX_UNKNOWN_TYPE_ROMIMAGE;				\
	for (INDEX = 0; INDEX < NUM_SUPPORTED_BOARDS; ++INDEX) {	\
		if (unm_boards[INDEX].brdtype == (BRDTYPE)) {		\
			FWTYPE = unm_boards[INDEX].fwtype;		\
			break;						\
		}							\
	}								\
}

typedef enum {
    UNM_BRDMFG_INVENTEC=1
} unm_brdmfg;

typedef struct {
    union {
        struct {
            __uint16_t brdnum;
            unm_brdtype_t  brdtype;
        };
        __uint32_t brdid;
    };
} unm_brdid_t;

#define CHIP_LOT_FAST 1
#define CHIP_LOT_TYPICAL 2
#define CHIP_LOT_SLOW 3

#define CHIP_PKG_31x31 1
#define CHIP_PKG_35x35 2
typedef struct {
    __uint32_t  id:20,
         minor:4,
         major:2,
         lot:3,
         pkg:3;
} unm_chipid_t;

typedef enum {
    MEM_ORG_128Mbx4   =0x0, /* DDR1 only */
    MEM_ORG_128Mbx8   =0x1, /* DDR1 only */
    MEM_ORG_128Mbx16  =0x2, /* DDR1 only */
    MEM_ORG_256Mbx4   =0x3,
    MEM_ORG_256Mbx8   =0x4,
    MEM_ORG_256Mbx16  =0x5,
    MEM_ORG_512Mbx4   =0x6,
    MEM_ORG_512Mbx8   =0x7,
    MEM_ORG_512Mbx16  =0x8,
    MEM_ORG_1Gbx4     =0x9,
    MEM_ORG_1Gbx8     =0xa,
    MEM_ORG_1Gbx16    =0xb,
    MEM_ORG_2Gbx4     =0xc,
    MEM_ORG_2Gbx8     =0xd,
    MEM_ORG_2Gbx16    =0xe,
    MEM_ORG_128Mbx32  =0x10002, /* GDDR only */
    MEM_ORG_256Mbx32  =0x10005 /* GDDR only */
} unm_mn_mem_org_t;

typedef enum {
    MEM_ORG_512Kx36   =0x0,
    MEM_ORG_1Mx36     =0x1,
    MEM_ORG_2Mx36     =0x2
} unm_sn_mem_org_t;

typedef enum {
    MEM_DEPTH_4MB   = 0x1,
    MEM_DEPTH_8MB   = 0x2,
    MEM_DEPTH_16MB  = 0x3,
    MEM_DEPTH_32MB  = 0x4,
    MEM_DEPTH_64MB  = 0x5,
    MEM_DEPTH_128MB = 0x6,
    MEM_DEPTH_256MB = 0x7,
    MEM_DEPTH_512MB = 0x8,
    MEM_DEPTH_1GB   = 0x9,
    MEM_DEPTH_2GB   = 0xa,
    MEM_DEPTH_4GB   = 0xb,
    MEM_DEPTH_8GB   = 0xc,
    MEM_DEPTH_16GB  = 0xd,
    MEM_DEPTH_32GB  = 0xe
} unm_mem_depth_t;

/******************************************************************************
*
*
*
*******************************************************************************
*/
typedef struct {
    __uint32_t header_version;

    __uint32_t board_mfg;
    //unm_brdid_t board_id;
    __uint32_t board_type;
    __uint32_t board_num;
    //unm_chipid_t chip_id;
    __uint32_t chip_id;
    __uint32_t chip_minor;
    __uint32_t chip_major;
    __uint32_t chip_pkg;
    __uint32_t chip_lot;


    __uint32_t port_mask;       /* available niu ports */
    __uint32_t peg_mask;        /* available pegs */
    __uint32_t icache_ok;       /* can we run with icache? */
    __uint32_t dcache_ok;       /* can we run with dcache? */
    __uint32_t casper_ok;

    //unm_eth_addr_t  mac_address[MAX_PORTS];  /*  */
    __uint32_t mac_addr_lo_0;
    __uint32_t mac_addr_lo_1;
    __uint32_t mac_addr_lo_2;
    __uint32_t mac_addr_lo_3;

    /* MN-related config */
    __uint32_t mn_sync_mode;    /* enable/ sync shift cclk/ sync shift mclk */
    __uint32_t mn_sync_shift_cclk;
    __uint32_t mn_sync_shift_mclk;
    __uint32_t mn_wb_en;
    __uint32_t mn_crystal_freq; /* in MHz */
    __uint32_t mn_speed;        /* in MHz */
    __uint32_t mn_org;
    __uint32_t mn_depth;
    __uint32_t mn_ranks_0;        /* ranks per slot */
    __uint32_t mn_ranks_1;        /* ranks per slot */
    __uint32_t mn_rd_latency_0;
    __uint32_t mn_rd_latency_1;
    __uint32_t mn_rd_latency_2;
    __uint32_t mn_rd_latency_3;
    __uint32_t mn_rd_latency_4;
    __uint32_t mn_rd_latency_5;
    __uint32_t mn_rd_latency_6;
    __uint32_t mn_rd_latency_7;
    __uint32_t mn_rd_latency_8;
    __uint32_t mn_dll_val[18];
    __uint32_t mn_mode_reg;       /* See MIU DDR Mode Register */
    __uint32_t mn_ext_mode_reg;   /* See MIU DDR Extended Mode Register */
    __uint32_t mn_timing_0;       /* See MIU Memory Control Timing Rgister */
    __uint32_t mn_timing_1;       /* See MIU Extended Memory Ctrl Timing Register */
    __uint32_t mn_timing_2;       /* See MIU Extended Memory Ctrl Timing2 Register */

    /* SN-related config */
    __uint32_t sn_sync_mode;    /* enable/ sync shift cclk / sync shift mclk */
    __uint32_t sn_pt_mode;    /* pass through mode */
    __uint32_t sn_ecc_en;
    __uint32_t sn_wb_en;
    __uint32_t sn_crystal_freq;
    __uint32_t sn_speed;
    __uint32_t sn_org;
    __uint32_t sn_depth;
    __uint32_t sn_dll_tap;
    __uint32_t sn_rd_latency;

    __uint32_t mac_addr_hi_0;
    __uint32_t mac_addr_hi_1;
    __uint32_t mac_addr_hi_2;
    __uint32_t mac_addr_hi_3;

    __uint32_t magic;          /* indicates flash has been initialized */

    __uint32_t mn_rdimm;
    __uint32_t mn_dll_override;
    __uint32_t coreclock_speed;
#if 0
    __uint32_t  boot_type;        /* is board booting from flash? */

    /* following stuff might be overkill */
    __uint32_t fw_id;              /* what code is loaded here? */
    __uint32_t fw_version;     /* version of flash */
    __uint32_t fw_checksum;
    __uint32_t crb_init_version;   /* info about flash's crb_init values */
    __uint32_t crb_init_checksum;  /* verification for crb_init values */
    __uint32_t pci_ph_type;     /* strap values for host PCI */
    __uint32_t pci_ps_type;     /* strap values for secondary PCI */
#endif

}  unm_board_info_t;

/*Data structure to read license info from flash*/
typedef struct {
	char 	data[NX_LICENSE_SIZE];	
} nx_encrypted_license_info_t;


#define FLASH_SECTOR_START                                  (1)
#define NUM_FLASH_SECTORS                                  (64)
#define FLASH_SECTOR_SIZE                             (64*1024)
#define FLASH_TOTAL_SIZE  (NUM_FLASH_SECTORS*FLASH_SECTOR_SIZE)
#define FLASH_NUM_PORTS                                     (4)

typedef struct {
    __uint32_t flash_addr[32];
} unm_flash_mac_addr_t;

/* flash user area */
typedef struct {
    __uint8_t  flash_md5[16];
    __uint8_t  crbinit_md5[16];
    __uint8_t  brdcfg_md5[16];
    /* bootloader */
    __uint32_t bootld_version;
    __uint32_t bootld_size;
    __uint8_t  bootld_md5[16];
    /* image */
    __uint32_t image_version;
    __uint32_t image_size;
    __uint8_t  image_md5[16];
    /* primary image status */
    __uint32_t primary_status;
    __uint32_t secondary_present;

    /* MAC address , 4 ports */
    unm_flash_mac_addr_t mac_addr[FLASH_NUM_PORTS];

    /* Any user defined data */
} unm_old_user_info_t;

#define FLASH_NUM_MAC_PER_PORT             32
typedef struct {
    __uint8_t  flash_md5[16 * 64];
    // __uint8_t  crbinit_md5[16];
    // __uint8_t  brdcfg_md5[16];
    /* bootloader */
    __uint32_t bootld_version;
    __uint32_t bootld_size;
    // __uint8_t  bootld_md5[16];
    /* image */
    __uint32_t image_version;
    __uint32_t image_size;
    // U8  image_md5[16];
    /* primary image status */
    __uint32_t primary_status;
    __uint32_t secondary_present;

    /* MAC address , 4 ports, 32 address per port */
    __uint64_t mac_addr[FLASH_NUM_PORTS * FLASH_NUM_MAC_PER_PORT];
    __uint32_t sub_sys_id;
    __uint8_t  serial_num[32];
	__uint32_t bios_version;
    __uint32_t pxe_enable;  /* bitmask, per port */
    __uint32_t vlan_tag[FLASH_NUM_PORTS];

    /* Any user defined data */
    __uint32_t vprop_pfn;  /* nxflash dynamically updates for vnic support */
    __uint32_t vprop_pfn2; /* nxflash dynamically updates for vnic support */
} unm_user_info_t;

typedef struct {
   __uint8_t  major;
   __uint8_t  minor;
   __uint16_t sub;
} unm_fw_ver_t;

#define UNM_VPD_LEN         1024
#define MAX_VPD_STING_SIZE    40
#define VPD_KEYWORD_SIZE       2

typedef struct vpd_tab_s {
    __uint8_t      product_id;
    __uint8_t      length[2]; //16 bit integer at odd address boundary !!!
    __uint8_t      data[40];
    __uint8_t      vpd_r_tag;
    __uint8_t      ro_length[VPD_KEYWORD_SIZE];
    __uint8_t      ro_pn_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      pn_length;
    __uint8_t      part_num[11];
    __uint8_t      ro_ec_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      ec_length;
    __uint8_t      ec_data[6];
    __uint8_t      ro_sn_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      sn_length;
    __uint8_t      sn_data[12];
    __uint8_t      ro_v0_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      v0_length;
    __uint8_t      v0_data[19];
    __uint8_t      ro_v2_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      v2_length;
    __uint8_t      v2_data[4];
    __uint8_t      ro_cs_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      ro_cs_length;
    __uint8_t      ro_checksum;
    __uint8_t      ro_reserved;
    __uint8_t      vpd_rw_tag;
    __uint8_t      vpd_rw_length[2]; //16 bit integer at odd address boundary !!!
    __uint8_t      rw_v1_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      v1_length;
    __uint8_t      v1_data[4];
    __uint8_t      rw_v3_keyword[VPD_KEYWORD_SIZE];
    __uint8_t      v3_length;
    __uint8_t      v3_data[12];
    __uint8_t      end_tag;
} unm_vpd_tab_t;

typedef struct vpd_s {
    union {
        unm_vpd_tab_t   vpd_tab;
        __uint32_t             vpd_data[UNM_VPD_LEN/sizeof(__uint32_t)];
    } u;
} unm_vpd_t;

#define VPD_PRODUCT_ID      0x82

#define SECONDARY_IMAGE_PRESENT 0xb3b4b5b6
#define SECONDARY_IMAGE_ABSENT  0xffffffff
#define PRIMARY_IMAGE_GOOD      0x5a5a5a5a
#define PRIMARY_IMAGE_BAD       0xffffffff

/* Flash memory map */
typedef enum {
    CRBINIT_START   = 0,          /* Crbinit section */
    BRDCFG_START    = 0x4000,     /* board config */
    INITCODE_START  = 0x6000,     /* pegtune code */
    BOOTLD_START    = 0x10000,    /* bootld */
    BOOTLD1_START   = 0x14000,	  /*Start of booloader 1*/
    IMAGE_START     = 0x43000,    /* compressed image */
    SECONDARY_START = 0x200000,   /* backup images */
    PXE_FIRST_STAGE_INTEL = 0x3C0000,/* Intel First Stage info */
    PXE_FIRST_STAGE_PPC = 0x3C4000,/* PPC First Stage info */
    PXE_SECOND_STAGE_INTEL = 0x3B0000, /* Intel Second Stage info */
    PXE_SECOND_STAGE_PPC = 0x3A0000, /* Intel Second Stage info */
//    LICENSE_TIME_START = 0x3C0000,/* license expiry time info */
    PXE_START       = 0x3D0000,   /* PXE image area */
    DEFAULT_DATA_START = 0x3e0000, /* where we place default factory data */
    USER_START      = 0x3E8000,   /* User defined region for new boards */
    VPD_START       = 0x3E8C00,   /* Vendor private data */
    LICENSE_START   = 0x3E9000,   /* Firmware License */
    FIXED_START     = 0x3F0000    /* backup of crbinit */
} unm_flash_map_t;

#define USER_START_OLD     PXE_START /* for backward compatibility */

#define FLASH_START			(CRBINIT_START)
#define INIT_SECTOR			(0)
#define INIT_BCKUP_SECTOR		(63)
#define PXE_FIRST_STAGE_SECTOR		(PXE_FIRST_STAGE_INTEL / FLASH_SECTOR_SIZE)
#define PXE_SECOND_STAGE_SECTOR_INTEL	(PXE_SECOND_STAGE_INTEL / FLASH_SECTOR_SIZE)
#define PXE_SECOND_STAGE_SECTOR_PPC	(PXE_SECOND_STAGE_PPC / FLASH_SECTOR_SIZE)
#define PXE_SECTOR			(PXE_START/FLASH_SECTOR_SIZE)
#define USER_SECTOR			(USER_START/FLASH_SECTOR_SIZE)
#define PRIMARY_START			(BOOTLD_START)
#define FLASH_PXE_FIRST_STAGE_SIZE	(0x4000)
#define FLASH_CRBINIT_SIZE		(0x4000)
#define FLASH_BRDCFG_SIZE		(sizeof(unm_board_info_t))
#define FLASH_USER_SIZE			(sizeof(unm_user_info_t)/sizeof(uint32_t))
#define FW_VERSION_OFFSET		(0x3e8408)
#define FW_SIZE_OFFSET			(0x3e840c)
#define FW_BUILD_NUM_OFFSET		(0x3e8410)
#define FW_BIOS_VERSION_OFFSET		(0x3e883c)
#define FLASH_VPD_SIZE			(UNM_VPD_LEN/sizeof(uint32_t))
#define FLASH_SECONDARY_SIZE		(USER_START-SECONDARY_START)
#define NUM_PRIMARY_SECTORS		(0x20)
#define NUM_CONFIG_SECTORS		(2)
#define LICENSE_LEN			(4096)
#define LICENSE_SECTOR_SIZE		(LICENSE_LEN/sizeof(uint32_t))
#define LICENSE_SECTOR			(LICENSE_START/FLASH_SECTOR_SIZE)


#endif  // __UNM_BRDINFO_H
