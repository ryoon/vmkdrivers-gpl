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

#ifndef __NX_PEXQ_H__
#define __NX_PEXQ_H__

/* Top-level PexQ Configuration
 */
#define NX_PEXQ_DBELL_BUF_ORDER       (4)
#define NX_PEXQ_DBELL_BUF_SIZE        (PAGE_SIZE << NX_PEXQ_DBELL_BUF_ORDER)
#define NX_PEXQ_DBELL_BUF_QMSGS       (NX_PEXQ_DBELL_BUF_SIZE / sizeof(unm_msg_t))
#define NX_PEXQ_DBELL_INCR_IDX(x,y)   ((x + y) % NX_PEXQ_DBELL_BUF_QMSGS)
#define NX_PEXQ_DBELL_GET_OFFSET(idx) ((idx) * sizeof(unm_msg_t))
#define NX_PEXQ_DBELL_MAX_PENDING     (NX_PEXQ_DBELL_BUF_QMSGS >> 2)
#define NX_PEXQ_DBELL_COMPL_THRES     (NX_PEXQ_DBELL_MAX_PENDING >> 3)

/* PexQ Doorbell Layout 
 */
#define PEXQ_DB_FIELD_ADDR_40(x)            ((x) & 0xffffffffffULL)
#define PEXQ_DB_FIELD_NUM_MSGS(x)           (((U64)(x & 0xff)) << 40)
#define PEXQ_DB_FIELD_2_QW_PER_MSG          (0ULL << 48)
#define PEXQ_DB_FIELD_4_QW_PER_MSG          (1ULL << 48)
#define PEXQ_DB_FIELD_8_QW_PER_MSG          (2ULL << 48)
#define PEXQ_DB_FIELD_DISABLE_COMPL         (1ULL << 51)
#define PEXQ_DB_FIELD_ENABLE_COMPL          (0ULL << 51)
#define PEXQ_DB_FIELD_COMPL_HDR_FROM_REG    (1ULL << 52)
#define PEXQ_DB_FIELD_COMPL_HDR_FROM_MSG    (0ULL << 52)
#define PEXQ_DB_FIELD_DATA_IN_HOST          (1ULL << 53)
#define PEXQ_DB_FIELD_DATA_COMA2T           (0ULL << 53)

/* PexQ per Function HW Config
 */
#define NX_PEXQ_PF2DBELL_NUMBER(x)        (((x) & 0x3f) + 0x40)
#define NX_PEXQ_DMA_REG_QW0_LO            (0x10)
#define NX_PEXQ_DMA_REG_QW0_HI            (0x11)
#define NX_PEXQ_DMA_REG_QW1_LO            (0x12)
#define NX_PEXQ_DMA_REG_QW1_HI            (0x13)
#define NX_PEXQ_DMA_REG_QW2_LO            (0x14)
#define NX_PEXQ_DMA_REG_QW2_HI            (0x15)
#define NX_PEXQ_DMA_REG_QW3_LO            (0x16)
#define NX_PEXQ_DMA_REG_QW3_HI            (0x17)
#define NX_PEXQ_DMA_REG_QW4_LO            (0x18)
#define NX_PEXQ_DMA_REG_QW4_HI            (0x19)
#define NX_PEXQ_DMA_REG_QW5_LO            (0x1a)
#define NX_PEXQ_DMA_REG_QW5_HI            (0x1b)
#define NX_PEXQ_DMA_REG_QW6_LO            (0x1c)
#define NX_PEXQ_DMA_REG_QW6_HI            (0x1d)
#define NX_PEXQ_DMA_REG_QW7_LO            (0x1e)
#define NX_PEXQ_DMA_REG_QW7_HI            (0x1f)


/* PexQ msg Circular Buffer

     ----- <== BASE: qbuf_vaddr
       |
       | Free  
       |
 -   ----- <== qbuf_pending_compl_idx
 |     |
 |P    | Completed
 |E    |   qbuf_pending_compl_idx, qbuf_free_cnt, and 
 |N    |   qbuf_pending_cnt can be updated
 |D    |
 |I  ----- <== compl_in_vaddr
 |N    |
 |G    | Doorbell sent, in progress
 |     |   Either an xdma update of compl_in_vaddr was scheduled
 |C    |   but has not yet completed or COMPL_THRES has not been
 |N    |   reached
 |T    |
 -   ----- <== qbuf_needs_db_idx
       |   
       | Message constructed, awaiting doorbell
       |   Either nx_ring_pexqdb() not yet called or flow control
       |   is holding off pexq
       |
     ----- <== qbuf_new_idx
       |
       | Free
       |
     ----- <== BUF_SIZE bytes, BUF_QMSGS messages
*/

typedef struct {
        spinlock_t	lock;
	U16             pci_fn;
	U16             db_number;
	
	/* Doorbell */
        U64             dbell_paddr;
        void           *dbell_vaddr;
        U64             dbell_reflection_paddr;
        void           *dbell_reflection_vaddr;

	/* QMSG Buffer */
        dma_addr_t      qbuf_paddr;
        unm_msg_t      *qbuf_vaddr;
	U16             qbuf_pending_compl_idx;
	U16             qbuf_needs_db_idx;
	U16             qbuf_new_idx;
	I16             qbuf_free_cnt;
	I16             qbuf_pending_cnt;

	/* Completion State 
	 *    in = Updated by periodic XDMA (cur idx -> in)
	 */
	unm_msg_hdr_t   xdma_hdr;
        dma_addr_t      compl_in_paddr;
	U64            *compl_in_vaddr;
	U64             card_fc_array;
        U32             card_fc_size; 
        U32             pexq_q_length;
        U32             reflection_offset;
} nx_pexq_dbell_t;


/* Initialize pexq for this pci function
 *   Failure: could no allocate needed host resources
 */
extern nx_rcode_t 
nx_pexq_allocate_memory(struct unm_adapter_s *adapter);

extern nx_rcode_t 
nx_init_pexq_dbell(struct unm_adapter_s *adapter);

/* Destroy pexq setup for this pci function
 */
extern void 
nx_free_pexq_dbell(struct unm_adapter_s *adapter);

/* Send a message via pexq doorbell
 *   Failure: msg could not be sent, retry later 
 *            (not enough card or host resources)
 */
extern nx_rcode_t 
nx_schedule_pexqdb(nx_pexq_dbell_t *pexq,
		   unm_msg_t *user_msg, 
                   U32 num_msgs);

#endif /* __NX_PEXQ_H__ */
