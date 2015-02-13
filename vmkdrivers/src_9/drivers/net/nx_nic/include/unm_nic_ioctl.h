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
#ifndef __UNM_NIC_IOCTL_H__
#define __UNM_NIC_IOCTL_H__

#ifndef SOLARIS
#include <linux/sockios.h>
#endif
#ifdef SOLARIS
#include"solaris.h"
#endif

/* ioctl's dealing with PCI read/writes */
#define UNM_CMD_START SIOCDEVPRIVATE
#define UNM_NIC_CMD  (UNM_CMD_START + 1)
#define UNM_NIC_NAME (UNM_CMD_START + 2)
#define UNM_NIC_SEND_TEST	(UNM_CMD_START + 3)
//#define UNM_NIC_IP_FILTER	(UNM_CMD_START + 4)
#define UNM_NIC_IRQ_TEST	(UNM_CMD_START + 5)
#define UNM_NIC_ILB_TEST	(UNM_CMD_START + 6)
#define UNM_NIC_ELB_TEST	(UNM_CMD_START + 7)
#define UNM_NIC_LINK_TEST	(UNM_CMD_START + 8)
#define UNM_NIC_HW_TEST		(UNM_CMD_START + 9)
#define UNM_NIC_CIS_TEST	(UNM_CMD_START + 10)
#define UNM_NIC_CR_TEST		(UNM_CMD_START + 11)
#define UNM_NIC_PROTSTATS	(UNM_CMD_START + 12)
#define UNM_NIC_LED_TEST	(UNM_CMD_START + 13)

#define UNM_NIC_NAME_LEN    16
#define UNM_NIC_NAME_RSP    "NETXEN"
#pragma pack(1)
/* Error codes - loopback test */
enum {
	LB_TEST_OK,
	LB_UCOPY_PARAM_ERR,
	LB_UCOPY_DATA_ERR,
	LB_NOMEM_ERR,
	LB_TX_NOSKB_ERR,
	LB_SHORT_DATA_ERR,
	LB_SEQUENCE_ERR,
	LB_DATA_ERR,
	LB_ERRCNT,
	LB_NOT_SUPPORTED
};

/* Error codes - CR test */
enum {
	CR_TEST_OK,
    CR_ERROR,
	CR_ERRCNT
};

/* Error codes - CIS test */
enum {
	CIS_TEST_OK,
	CIS_WMARK,
	CIS_ERRCNT
};

/* Error Codes - LED Test */
enum{
LED_TEST_OK,
LED_TEST_ERR,
LED_TEST_UNKNOWN_PHY,
LED_TEST_NOT_SUPPORTED
};

enum{
LED_OFF=2,
LED_ON
};

typedef struct {
       __uint32_t state;
       __uint32_t rate;
}led_params_t;

/* Error Codes - INTERRUPT Test */
enum {

	INT_TEST_OK,
	INT_TEST_ERR,
	INT_NOT_SUPPORTED
};

/* Error codes - HW test */
enum {
	HW_TEST_OK,
	HW_DMA_BZ_0,
	HW_DMA_BZ_1,
	HW_DMA_BZ_2,
	HW_DMA_BZ_3,
	HW_SRE_PBI_HALT,
	HW_SRE_L1IPQ,
	HW_SRE_L2IFQ,
	HW_SRE_FREEBUF,
	HW_IPQ,
	HW_PQ_W_PAUSE,
	HW_PQ_W_FULL,
	HW_IFQ_W_PAUSE,
	HW_IFQ_W_FULL,
	HW_MEN_BP_TOUT,
	HW_DOWN_BP_TOUT,
	HW_FBUFF_POOL_WM,
	HW_PBUF_ERR,
	HW_FM_MSG_HDR,
	HW_FM_MSG,
	HW_EPG_CTRL_Q,
	HW_EPG_MSG_BUF,
	HW_EPG_QREAD_TOUT,
	HW_EPG_QWRITE_TOUT,
	HW_EPG_CQ_W_FULL,
	HW_EPG_MSG_CHKSM,
   	HW_EPG_MTLQ_TOUT,
	HW_PEG0,
	HW_PEG1,
	HW_PEG2,
	HW_PEG3,
	HW_ERRCNT
};


struct sections
{

   long long is_bss_flag;
   long long file_offset;
   long long size;
   long long load_offset;
};
struct driverimg_header
{
   long long no_of_sections;
   struct sections sec[1];
};

#define UNM_NIC_CMD_NONE 		0
#define UNM_NIC_CMD_PCI_READ 		1
#define UNM_NIC_CMD_PCI_WRITE 		2
#define UNM_NIC_CMD_PCI_MEM_READ 	3
#define UNM_NIC_CMD_PCI_MEM_WRITE 	4
#define UNM_NIC_CMD_PCI_CONFIG_READ 	5
#define UNM_NIC_CMD_PCI_CONFIG_WRITE 	6
#define UNM_NIC_CMD_GET_VERSION 	9
#define UNM_NIC_CMD_SET_LED_CONFIG	10
#define UNM_NIC_CMD_GET_PHY_TYPE 	11
#define UNM_NIC_CMD_EFUSE_CHIP_ID	12
#define UNM_NIC_CMD_GET_LIC_FINGERPRINT	13
#define UNM_NIC_CMD_LIC_INSTALL		14
#define UNM_NIC_CMD_GET_LIC_FEATURES 	15
#define UNM_NIC_CMD_SET_PID_TRAP 	16
#define UNM_NIC_CMD_FLASH_READ 		50
#define UNM_NIC_CMD_FLASH_WRITE 	51
#define UNM_NIC_CMD_FLASH_SE		52

#define UNM_FLASH_READ_SIZE     4
#define UNM_FLASH_WRITE_SIZE    4

typedef struct {
        __uint32_t cmd;
        __uint32_t unused1;
        __uint64_t off;
        __uint32_t size;
        __uint32_t rv;
        char u[64];
	__uint64_t ptr;
} unm_nic_ioctl_data_t;

#if 0
/* FIXME vijo,  not used for now */
/* ioctl's dealing w. interrupt counts */
#define UNM_INTR_CMD 3
typedef enum {
        unm_intr_cmd_none = 0,
        unm_intr_cmd_clear,
        unm_intr_cmd_get
} unm_intr_cmd_t;

typedef enum {
        unm_intr_src_unspecified = -1,
        unm_intr_src_min = 0,
        unm_intr_src_dma_0 = 0,
        unm_intr_src_dma_1 = 1,
        unm_intr_src_i2q = 2,
        unm_intr_src_dma_2 = 3,
        unm_intr_src_dma_3 = 4,
        unm_intr_src_rc = 5,
        unm_intr_src_mega_err = 6,
        unm_intr_src_target0 = 7,
        unm_intr_src_target1 = 8,
        unm_intr_src_target2 = 9,
        unm_intr_src_target3 = 10,
        unm_intr_src_target4 = 11,
        unm_intr_src_target5 = 12,
        unm_intr_src_target6 = 13,
        unm_intr_src_target7 = 14,
        unm_intr_src_max = 15

} unm_intr_src_t;

typedef struct {
        unm_intr_cmd_t cmd;
        unm_intr_src_t int_src;
        int data;
} unm_intr_data_t;
#endif

typedef enum {
	UNM_TX_START = 1,
	UNM_TX_STOP,
	UNM_TX_SET_PARAM,
	UNM_TX_SET_PACKET,
	UNM_LOOPBACK_START,
	UNM_LOOPBACK_STOP
} unm_send_test_cmd_t;

typedef struct {
	unm_send_test_cmd_t cmd;
	__uint64_t   ifg;
	__uint32_t   count;
} unm_send_test_t;

#pragma pack()
#endif
