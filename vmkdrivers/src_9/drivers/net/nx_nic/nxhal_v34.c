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
/********************************************************************
  * Host FW interface functions( For Old Arvana FW ).
  *
  ********************************************************************/


/* TB removed after resolve */

#include "nx_errorcode.h"
#include "nxplat.h"
#include "unm_inc.h"
#include "nic_cmn.h"

#include "nic_phan_reg.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"
#include "nxhal_v34.h"

#define NUM_RCV_DESC_RINGS_V34      3

unsigned long long  ctx_addr_sig_regs[][3] = {
    {UNM_NIC_REG(0x188), UNM_NIC_REG(0x18c), UNM_NIC_REG(0x1c0)},
    {UNM_NIC_REG(0x190), UNM_NIC_REG(0x194), UNM_NIC_REG(0x1c4)},
    {UNM_NIC_REG(0x198), UNM_NIC_REG(0x19c), UNM_NIC_REG(0x1c8)},
    {UNM_NIC_REG(0x1a0), UNM_NIC_REG(0x1a4), UNM_NIC_REG(0x1cc)}
};

/* CRB registers per function interrupt */
typedef struct {
        int PREALIGN(512) CRB_INTERRUPT_OFFSET[8] POSTALIGN(512);
} nx_interrupt_crb_t;

static nx_interrupt_crb_t intr_crb_registers[] =
{
        /* Context 0 receive CRB registers: Maps to port 0 */
        {{
                CRB_SW_INT_MASK_0,
                /* UNM_NIC_REG_2(0x040), */
                UNM_NIC_REG_2(0x044),
                UNM_NIC_REG_2(0x048),
                UNM_NIC_REG_2(0x04c),
                UNM_NIC_REG_2(0x050),
                UNM_NIC_REG_2(0x054),
                UNM_NIC_REG_2(0x058),
                UNM_NIC_REG_2(0x05c),
                }},
        {{
                CRB_SW_INT_MASK_1,
                /* UNM_NIC_REG_2(0x060), */
                UNM_NIC_REG_2(0x064),
                UNM_NIC_REG_2(0x068),
                UNM_NIC_REG_2(0x06c),
                UNM_NIC_REG_2(0x070),
                UNM_NIC_REG_2(0x074),
                UNM_NIC_REG_2(0x078),
                UNM_NIC_REG_2(0x07c),
                }},
        {{
                CRB_SW_INT_MASK_2,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                }},
        {{
                CRB_SW_INT_MASK_3,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                }},
};

/* CRB registers per Rcv Descriptor ring */
typedef struct {
    int PREALIGN(512) CRB_RCV_PRODUCER_OFFSET POSTALIGN(512);
    int CRB_RCV_CONSUMER_OFFSET;
    int CRB_GLOBALRCV_RING;
    int CRB_RCV_RING_SIZE;
} unm_rcv_desc_crb_t;

/*
 * CRB registers used by the receive peg logic.
 */

typedef struct {
        unm_rcv_desc_crb_t rcv_desc_crb[NUM_RCV_DESC_RINGS_V34];

        int     comp_rsvd_1;
        int     comp_rsvd_2;
        int     CRB_RCV_STATUS_CONSUMER[8];
        int     CRB_RCVPEG_STATE_OLD;
        int     comp_rsvd_3;
} unm_recv_crb_t;

static unm_recv_crb_t recv_crb_registers[] =
{
        /* Context 0 receive CRB registers: Maps to port 0 */
        {
                //rcv_desc_crb:
                {
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x100),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x104),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x108),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x10c),
                        },
                        /* Jumbo frames */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x110),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x114),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x118),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x11c),
                        },
                        /* LRO */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x120),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x124),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x128),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x12c),
                        }
                },
                //CRB_RCVSTATUS_RING:
                UNM_NIC_REG(0x130),
                //CRB_RCV_STATUS_PRODUCER:
                UNM_NIC_REG(0x134),
                //CRB_RCV_STATUS_CONSUMER:
                {
                        UNM_NIC_REG(0x138),
                        UNM_NIC_REG_2(0x000),
                        UNM_NIC_REG_2(0x004),
                        UNM_NIC_REG_2(0x008),
                        UNM_NIC_REG_2(0x00c),
                        UNM_NIC_REG_2(0x010),
                        UNM_NIC_REG_2(0x014),
                        UNM_NIC_REG_2(0x018),
                },
                //CRB_RCVPEG_STATE:
                UNM_NIC_REG(0x13c),
                //CRB_STATUS_RING_SIZE
                UNM_NIC_REG(0x140),
        },

        /* Context 1 receive CRB registers: Maps to port 1 */
        {
                {
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x144),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x148),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x14c),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x150),
                        },
                        /* Jumbo frames */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x154),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x158),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x15c),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x160),
                        },
                        /* LRO */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x164),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x168),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x16c),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x170),
                        }
                },
                //CRB_RCVSTATUS_RING:
                UNM_NIC_REG(0x174),
                //CRB_RCV_STATUS_PRODUCER:
                UNM_NIC_REG(0x178),
                //CRB_RCV_STATUS_CONSUMER:
                {
                        UNM_NIC_REG(0x17c),
                        UNM_NIC_REG_2(0x020),
                        UNM_NIC_REG_2(0x024),
                        UNM_NIC_REG_2(0x028),
                        UNM_NIC_REG_2(0x02c),
                        UNM_NIC_REG_2(0x030),
                        UNM_NIC_REG_2(0x034),
                        UNM_NIC_REG_2(0x038),
                },
                //CRB_RCVPEG_STATE:
                UNM_NIC_REG(0x180),
                //CRB_STATUS_RING_SIZE
                UNM_NIC_REG(0x184),
        },

        /* Context 2 receive CRB registers: Maps to port 2 */
        {
                //instance 3
                {
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x2b0),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x2b4),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x1f0),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x1f4),
                        },
                        /* Jumbo frames */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x1f8),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x1fc),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x200),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x204),
                        },
                        /* LRO */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x208),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x20c),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x210),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x214),
                        }
                },
                //CRB_RCVSTATUS_RING:
                UNM_NIC_REG(0x218),
                //CRB_RCV_STATUS_PRODUCER:
                UNM_NIC_REG(0x21c),
                //CRB_RCV_STATUS_CONSUMER:
                {
                        UNM_NIC_REG(0x220),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                },
                //CRB_RCVPEG_STATE:
                UNM_NIC_REG(0x224),
                //CRB_STATUS_RING_SIZE
                UNM_NIC_REG(0x228),
        },

        /* Context 3 receive CRB registers: Maps to port 3 */
        {
                //instance 4
                {
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x22c),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x230),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x234),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x238),
                        },
                        /* Jumbo frames */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x23c),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x240),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x244),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x248),
                        },
                        /* LRO */
                        {
                                //CRB_RCV_PRODUCER_OFFSET:
                                UNM_NIC_REG(0x24c),
                                //CRB_RCV_CONSUMER_OFFSET:
                                UNM_NIC_REG(0x250),
                                //CRB_GLOBALRCV_RING:
                                UNM_NIC_REG(0x254),
                                //CRB_RCV_RING_SIZE:
                                UNM_NIC_REG(0x258),
                        }
                },
                //CRB_RCVSTATUS_RING:
                UNM_NIC_REG(0x25c),
                //CRB_RCV_STATUS_PRODUCER:
                UNM_NIC_REG(0x260),
                //CRB_RCV_STATUS_CONSUMER:
                {
                        UNM_NIC_REG(0x264),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                        UNM_NIC_REG_2(0x03c),
                },
                //CRB_RCVPEG_STATE:
                UNM_NIC_REG(0x268),
                //CRB_STATUS_RING_SIZE
                UNM_NIC_REG(0x26c),
        },
};

U32
issue_v34_cmd(nx_dev_handle_t drv_handle,
          U32 pci_func,
          U32 version,
          U32 arg1,
          U32 arg2)
{
        U32 rcode = NX_RCODE_SUCCESS;


       /* Acquire semaphore before accessing CRB */

        if (api_lock(drv_handle)) {
                return NX_RCODE_TIMEOUT;
        }

        nx_os_nic_reg_write_w1(drv_handle, CRB_CTX_SIGNATURE_REG(pci_func),
                               NX_OS_CPU_TO_LE32(UNM_CTX_SIGNATURE_V2 | pci_func));

        nx_os_nic_reg_write_w1(drv_handle, CRB_CTX_ADDR_REG_HI(pci_func),
                               NX_OS_CPU_TO_LE32(arg1));

        nx_os_nic_reg_write_w1(drv_handle, CRB_CTX_ADDR_REG_LO(pci_func),
                               NX_OS_CPU_TO_LE32(arg2));

       /* Release semaphore
         */
        api_unlock(drv_handle);

        return rcode;

}

nx_rcode_t
nx_fw_cmd_v34_create_ctx(nx_host_rx_ctx_t *prx_ctx,
                              nx_host_tx_ctx_t *ptx_ctx,
                              struct nx_dma_alloc_s *hostrq)
{
        nx_host_rds_ring_t *prds_rings = NULL;
        nx_host_sds_ring_t *psds_rings = NULL;
        nx_dev_handle_t drv_handle;
        RingContext *ctx_dma = (RingContext*)hostrq->ptr;
        U64 tmp_phys_addr = 0;
        nx_rcode_t rcode = NX_RCODE_SUCCESS;
        I32 i = 0;
        U32 temp = 0;

        NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

        if (prx_ctx == NULL ||
            hostrq == NULL) {
                return NX_RCODE_INVALID_ARGS;
        }

        drv_handle = prx_ctx->nx_dev->nx_drv_handle;

        nx_os_zero_memory(hostrq->ptr, sizeof(RingContext) + sizeof(uint32_t));

        if (NX_HOST_CTX_STATE_ALLOCATED != prx_ctx->state) {
                NX_DBGPRINTF(NX_DBG_INFO,("%s:Invalid RX CTX state:%x\n",
                             __FUNCTION__, prx_ctx->state));
                return (NX_RCODE_INVALID_ARGS);
        }

        temp = (NX_CAP0_LEGACY_CONTEXT | NX_CAP0_LEGACY_MN);

        ctx_dma->CMD_CONSUMER_OFFSET = ptx_ctx->cmd_cons_dma_addr;
        ctx_dma->CmdRingAddrLo       = ptx_ctx->cds_ring[0].host_phys & 0xffffffffUL;
        ctx_dma->CmdRingAddrHi       = ((U64)ptx_ctx->cds_ring[0].host_phys >> 32);
        ctx_dma->CmdRingSize         = ptx_ctx->cds_ring[0].ring_size;
        prds_rings = prx_ctx->rds_rings;

        for (i = 0; i < prx_ctx->num_rds_rings; i++) {
                ctx_dma->RcvContext[i].RcvRingAddrLo = prds_rings[i].host_phys & 0xffffffffUL;
                ctx_dma->RcvContext[i].RcvRingAddrHi = ((U64)prds_rings[i].host_phys >> 32);
                ctx_dma->RcvContext[i].RcvRingSize   = prds_rings[i].ring_size;
        }

        psds_rings = prx_ctx->sds_rings;
        for (i = 0; i < prx_ctx->num_sds_rings; i++) {
                if (i == 0) {
                        ctx_dma->StsRingAddrLo = psds_rings[i].host_phys & 0xffffffffUL;
                        ctx_dma->StsRingAddrHi = ((U64)psds_rings[i].host_phys >> 32);
                        ctx_dma->StsRingSize   = psds_rings[i].ring_size;
                }
                ctx_dma->StsRings[i].StsRingAddr = psds_rings[i].host_phys;
                ctx_dma->StsRings[i].StsRingSize = psds_rings[i].ring_size;
                ctx_dma->StsRings[i].msix_entry_idx = psds_rings[i].msi_index;
        }

        ctx_dma->StsRingCount = prx_ctx->num_sds_rings;

        tmp_phys_addr = nx_os_dma_addr_to_u64(hostrq->phys);

        rcode = issue_v34_cmd(drv_handle,
                                   prx_ctx->pci_func,
                                   NXHAL_VERSION,
                                   (U32)(tmp_phys_addr >> 32),
                                   ((U32)tmp_phys_addr));

        if (rcode != NX_RCODE_SUCCESS) {
                goto failure;
        }

        prds_rings = prx_ctx->rds_rings;

        for (i = 0; i < NUM_RCV_DESC_RINGS_V34; i++){
                prds_rings[i].host_rx_producer = NX_OS_CPU_TO_LE32(
                        recv_crb_registers[prx_ctx->pci_func].rcv_desc_crb[i].CRB_RCV_PRODUCER_OFFSET);
        }

        psds_rings = prx_ctx->sds_rings;
        for (i = 0; i < prx_ctx->num_sds_rings; i++) {
                psds_rings[i].host_sds_consumer = NX_OS_CPU_TO_LE32(
                        recv_crb_registers[prx_ctx->pci_func].CRB_RCV_STATUS_CONSUMER[i]);
                psds_rings[i].interrupt_crb = NX_OS_CPU_TO_LE32(
                        intr_crb_registers[prx_ctx->pci_func].CRB_INTERRUPT_OFFSET[i]);
        }

        prx_ctx->state = NX_HOST_CTX_STATE_ACTIVE;
        prx_ctx->context_id = prx_ctx->pci_func;
#ifdef NX_USE_NEW_ALLOC
        if (prx_ctx->nx_dev->default_rx_ctx == NULL) {
                prx_ctx->nx_dev->default_rx_ctx = prx_ctx;
        }
#endif

        ptx_ctx->cds_ring->host_tx_producer =
                NX_OS_CPU_TO_LE32(CRB_CMD_PRODUCER_OFFSET);
        ptx_ctx->state = NX_HOST_CTX_STATE_ACTIVE;
        ptx_ctx->context_id = ptx_ctx->pci_func;
#ifdef NX_USE_NEW_ALLOC
        if (ptx_ctx->nx_dev->default_tx_ctx == NULL) {
                ptx_ctx->nx_dev->default_tx_ctx = ptx_ctx;
        }
#endif
failure:
        return(rcode);
}

