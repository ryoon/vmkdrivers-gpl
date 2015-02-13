/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
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
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#ifndef NIC_PHAN_REG_H
#define NIC_PHAN_REG_H

#include "unm_compiler_defs.h"

#define NIC_CRB_BASE               UNM_CAM_RAM(0x200)
#define NIC_CRB_BASE_2             UNM_CAM_RAM(0x700)
#define UNM_NIC_REG(X)             (NIC_CRB_BASE+(X))
#define UNM_NIC_REG_2(X)           (NIC_CRB_BASE_2+(X))

#define CRB_CUT_THRU_PAGE_SIZE      UNM_CAM_RAM(0x170)

#define CRB_CMD_PRODUCER_OFFSET     UNM_NIC_REG(0x08)
#define CRB_CMD_CONSUMER_OFFSET     UNM_NIC_REG(0x0c)
#define CRB_PAUSE_ADDR_LO           UNM_NIC_REG(0x10) /* C0 EPG BUG  */
#define CRB_PAUSE_ADDR_HI           UNM_NIC_REG(0x14)
#define NX_CDRP_CRB_OFFSET          UNM_NIC_REG(0x18)
#define NX_ARG1_CRB_OFFSET          UNM_NIC_REG(0x1c)
#define NX_ARG2_CRB_OFFSET          UNM_NIC_REG(0x20)
#define NX_ARG3_CRB_OFFSET          UNM_NIC_REG(0x24)
#define NX_SIGN_CRB_OFFSET          UNM_NIC_REG(0x28)
#define CRB_CMDPEG_CMDRING          UNM_NIC_REG(0x38)
#define CRB_HOST_DUMMY_BUF_ADDR_HI  UNM_NIC_REG(0x3c)
#define CRB_HOST_DUMMY_BUF_ADDR_LO  UNM_NIC_REG(0x40)
#define CRB_CMDPEG_STATE            UNM_NIC_REG(0x50)
#define BOOT_LOADER_DIMM_STATUS     UNM_NIC_REG(0x54)
#define CRB_GLOBAL_INT_COAL         UNM_NIC_REG(0x64) /* interrupt coalescing */
#define CRB_INT_COAL_MODE           UNM_NIC_REG(0x68)
#define CRB_MAX_RCV_BUFS            UNM_NIC_REG(0x6c)
#define CRB_TX_INT_THRESHOLD        UNM_NIC_REG(0x70)
#define CRB_RX_PKT_TIMER            UNM_NIC_REG(0x74)
#define CRB_TX_PKT_TIMER            UNM_NIC_REG(0x78)
#define CRB_RX_PKT_CNT              UNM_NIC_REG(0x7c)
#define CRB_RX_TMR_CNT              UNM_NIC_REG(0x80)
#define CRB_RCV_INTR_COUNT          UNM_NIC_REG(0x84)
#define CRB_XG_STATE                UNM_NIC_REG(0x94) /* XG Link status */
#define CRB_XG_STATE_P3             UNM_NIC_REG(0x98) /* XG PF Link status */
#define CRB_TX_STATE                UNM_NIC_REG(0xac) /* Debug -performance */
#define CRB_TX_COUNT                UNM_NIC_REG(0xb0)
#define CRB_RX_STATE                UNM_NIC_REG(0xb4)
#define CRB_RX_PERF_DEBUG_1         UNM_NIC_REG(0xb8)
#define CRB_RX_LRO_CONTROL          UNM_NIC_REG(0xbc) /* LRO On/OFF */
#define CRB_MPORT_MODE              UNM_NIC_REG(0xc4) /* Multiport Mode */
#define CRB_DMA_SIZE                UNM_NIC_REG(0xcc) /* DMA mask extension */
#define CRB_INT_VECTOR              UNM_NIC_REG(0xd4)
#define CRB_PF_LINK_SPEED_1         UNM_NIC_REG(0xe8)
#define CRB_PF_LINK_SPEED_2         UNM_NIC_REG(0xec)
#define CRB_PF_MAX_LINK_SPEED_1     UNM_NIC_REG(0xf0)
#define CRB_PF_MAX_LINK_SPEED_2     UNM_NIC_REG(0xf4)
#define CRB_HOST_DUMMY_BUF          UNM_NIC_REG(0xfc)

/* used for ethtool tests */
#define CRB_SCRATCHPAD_TEST         UNM_NIC_REG(0x280)

#define CRB_RCVPEG_STATE            UNM_NIC_REG(0x13c)

#define UNM_PEG_HALT_STATUS1        UNM_CAM_RAM(0x0a8)
#define UNM_PEG_HALT_STATUS2        UNM_CAM_RAM(0x0ac)
#define UNM_PEG_ALIVE_COUNTER       UNM_CAM_RAM(0x0b0)

#define	UNM_FW_CAPABILITIES_1	    UNM_CAM_RAM(0x128)

/* 12 registers to store MAC addresses for 8 PCI functions */
#define CRB_MAC_BLOCK_START         UNM_CAM_RAM(0x1c0)

#define CRB_CMD_PRODUCER_OFFSET_1   UNM_NIC_REG(0x1ac)
#define CRB_CMD_CONSUMER_OFFSET_1   UNM_NIC_REG(0x1b0)
#define CRB_TEMP_STATE              UNM_NIC_REG(0x1b4)
#define CRB_CMD_PRODUCER_OFFSET_2   UNM_NIC_REG(0x1b8)
#define CRB_CMD_CONSUMER_OFFSET_2   UNM_NIC_REG(0x1bc)

#define CRB_CMD_PRODUCER_OFFSET_3   UNM_NIC_REG(0x1d0)
#define CRB_CMD_CONSUMER_OFFSET_3   UNM_NIC_REG(0x1d4)
/*   sw int status/mask registers */
#define CRB_SW_INT_MASK_OFFSET_0   0x1d8
#define CRB_SW_INT_MASK_OFFSET_1   0x1e0
#define CRB_SW_INT_MASK_OFFSET_2   0x1e4
#define CRB_SW_INT_MASK_OFFSET_3   0x1e8
#define CRB_SW_INT_MASK_OFFSET_4   0x450
#define CRB_SW_INT_MASK_OFFSET_5   0x454
#define CRB_SW_INT_MASK_OFFSET_6   0x458
#define CRB_SW_INT_MASK_OFFSET_7   0x45c
#define CRB_SW_INT_MASK_0          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_0)
#define CRB_SW_INT_MASK_1          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_1)
#define CRB_SW_INT_MASK_2          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_2)
#define CRB_SW_INT_MASK_3          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_3)
#define CRB_SW_INT_MASK_4          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_4)
#define CRB_SW_INT_MASK_5          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_5)
#define CRB_SW_INT_MASK_6          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_6)
#define CRB_SW_INT_MASK_7          UNM_NIC_REG(CRB_SW_INT_MASK_OFFSET_7)

#define CRB_NIC_DEBUG_STRUCT_BASE  UNM_NIC_REG(0x288)

static inline
unsigned long CRB_SW_INT_MASK(unsigned long N)
{
        switch(N) {
        case 0:
                return CRB_SW_INT_MASK_0;
        case 1:
                return CRB_SW_INT_MASK_1;
        case 2:
                return CRB_SW_INT_MASK_2;
        case 3:
                return CRB_SW_INT_MASK_3;
        case 4:
                return CRB_SW_INT_MASK_4;
        case 5:
                return CRB_SW_INT_MASK_5;
        case 6:
                return CRB_SW_INT_MASK_6;
        case 7:
                return CRB_SW_INT_MASK_7;
        default:
                return -1;  /* keep compiler happy */
        }
}

/* capabilities register, can be used to selectively enable/disable features
 * for backward compability
 */

/*
 * For P2 V3.4 firmware
 */
#define CRB_NIC_CAPABILITIES_HOST       UNM_NIC_REG(0x1a8)
#define CRB_NIC_CAPABILITIES_FW         UNM_NIC_REG(0x1dc)
#define CRB_NIC_MSI_MODE_HOST           UNM_NIC_REG(0x270)
#define CRB_NIC_MSI_MODE_FW             UNM_NIC_REG(0x274)



#define INTR_SCHEME_PERPORT		0x1
#define MSI_MODE_MULTIFUNC              0x1

#define CRB_EPG_QUEUE_BUSY_COUNT    UNM_NIC_REG(0x200)

#define CRB_V2P_0                   UNM_NIC_REG(0x290)
#define CRB_V2P_1                   UNM_NIC_REG(0x294)
#define CRB_V2P_2                   UNM_NIC_REG(0x298)
#define CRB_V2P_3                   UNM_NIC_REG(0x29c)
#define CRB_V2P(port)               (CRB_V2P_0+((port)*4))
#define CRB_DRIVER_VERSION          UNM_NIC_REG(0x2a0)

#define CRB_CNT_DBG1                UNM_NIC_REG(0x2a4)
#define CRB_CNT_DBG2                UNM_NIC_REG(0x2a8)
#define CRB_CNT_DBG3                UNM_NIC_REG(0x2ac)

	/*
	 * Driver must set the version number register as follows:
	 *	(major << 16) | (minor << 8) | (subminor)
	 */

/* last -> 0x2a0 */

/* Upper 16 bits of CRB_TEMP_STATE:temperature value. Lower 16 bits: state */
#define nx_get_temp_val(x)                  ((x) >> 16)
#define nx_get_temp_state(x)                ((x) & 0xffff)
#define nx_encode_temp(val, state)          (((val) << 16) | (state))


#define lower32(x)             ( (__uint32_t)((x) & 0xffffffff) )
#define upper32(x)             ( (__uint32_t)(((unsigned long long)(x) >> 32) & 0xffffffff))

/*
 * Temperature control.
 */
enum {
    NX_TEMP_UNKNOWN = 0x0,	/* Temparature not available */
    NX_TEMP_NORMAL,			/* Normal operating range */
    NX_TEMP_WARN,			/* Sound alert, temperature getting high */
    NX_TEMP_PANIC			/* Fatal error, hardware has shut down. */
};

#define D3_CRB_REG_FUN2         (UNM_PCIX_PS_REG(0x2084))
#define D3_CRB_REG_FUN3         (UNM_PCIX_PS_REG(0x3084))

#endif /* NIC_PHAN_REG_H */
