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
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>

#include "nx_errorcode.h"
#include "nxplat.h"

#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"

#include "unm_nic_hw.h"
#include "nic_cmn.h"
#include "nxhal.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#include <linux/wrapper.h>
#endif
#ifndef _LINUX_MODULE_PARAMS_H
#include <linux/moduleparam.h>
#endif

#include "unm_nic.h"
#include "kernel_compatibility.h"
#include "unm_version.h"
#include "nx_pexq.h"


static inline void
form_dma_sync_msg(unm_msg_t *msg,
		   U16 msg_idx,
		   U16 pci_fn,
		   U64 xdma_hdr_word,
		   U64 card_fc_array,
		   U64 compl_in_paddr)
{
	U64 card_addr = card_fc_array + 
                            8*NX_PEXQ_DBELL_INCR_IDX(msg_idx, 1);

	msg->hdr.word = xdma_hdr_word;
        msg->body.values[0] = (8 << 0)                   | /* length */
	               ((((U64)pci_fn) >> 2) << 26)      |
                       (4ULL<<48)                        | /* card to host */
                       (((((U64)pci_fn) & 0x3) <<4)<<48) | 
                       ((1ULL << 14)<<48)                | /* end flag */
                       ((1ULL << 15)<<48) ;                /* cmd.valid  */
	msg->body.values[1] = card_addr;                /* src */
	msg->body.values[2] = compl_in_paddr            /* dst */

	nx_nic_print7(NULL, "Gen compl update req idx %d\n",
		      msg_idx);
}


static inline nx_rcode_t pexq_update_pending_doorbell(nx_pexq_dbell_t *pexq)
{

	U32 rv = NX_RCODE_SUCCESS;
	I16 in, cnt;

	in = *pexq->compl_in_vaddr;
	if (in == pexq->qbuf_pending_compl_idx) {
		nx_nic_print7(NULL,
		      "%s: max pending cnt %d reached. "
			      "pend idx %d in idx %d\n",
			      __FUNCTION__,
			      pexq->qbuf_pending_cnt,
			      pexq->qbuf_pending_compl_idx, in);
		return NX_RCODE_NOT_READY;
	}
	
	/* One or more pexq ops completed */
	cnt = (in - pexq->qbuf_pending_compl_idx);
	if (in < pexq->qbuf_pending_compl_idx) {
		cnt += NX_PEXQ_DBELL_BUF_QMSGS;
	}

	nx_nic_print7(NULL, "%s: compl %d idx %d\n",
		      __FUNCTION__, cnt, in);

	pexq->qbuf_pending_compl_idx = in;
	pexq->qbuf_pending_cnt -= cnt;
	pexq->qbuf_free_cnt += cnt;
	if (pexq->qbuf_pending_cnt < 0) {
		nx_nic_print3(NULL,
			      "%s: pending_cnt %d out of range\n",
			      __FUNCTION__,
			      pexq->qbuf_pending_cnt);
	}
	if (pexq->qbuf_free_cnt > NX_PEXQ_DBELL_BUF_QMSGS) {
		nx_nic_print3(NULL,
			      "%s: free_cnt %d out of range\n",
			      __FUNCTION__,
			      pexq->qbuf_free_cnt);
	}

        return rv;
}

static inline void pexq_ring_doorbell(U64 post_cmd, 
                                      void *dbell_vaddr, 
                                      void *dbell_reflection_vaddr)
{
        U64  reflection_value;
retry: 
	writeq(__cpu_to_le64(post_cmd), dbell_vaddr);
        reflection_value = __le64_to_cpu(readq(dbell_reflection_vaddr));
        if (reflection_value == post_cmd) {
                return;
        }

        nx_nic_print3(NULL, "%s: Writing 0x%llx. Read back 0x%llx\n", 
             __FUNCTION__, post_cmd, reflection_value);
        goto retry;
}


/* Send a message via pexq doorbell
 *   Failure: msg could not be sent, retry later 
 *            (not enough card or host resources)
 */
#define MAX_PEXQ_DBELL  4
nx_rcode_t 
nx_schedule_pexqdb(nx_pexq_dbell_t *pexq,
		   unm_msg_t *user_msg, 
                   U32 num_requested_msgs)
{
        unm_msg_t *msg;
	U16 msg_idx;
        U32 qbuf_free_cnt;
        U16 qbuf_pending_cnt;
        U32 ii = 0;
        U32 num_dbells = 0;
        U32 num_dbell_qmsgs = 0;
        struct {
                U64 qbuf_paddr;
                U32 num_msgs;
        } dbell_log[MAX_PEXQ_DBELL];

	spin_lock_bh(&pexq->lock);

        /* Ignore return value. */
        pexq_update_pending_doorbell(pexq);

	msg_idx = pexq->qbuf_new_idx;
        qbuf_free_cnt = pexq->qbuf_free_cnt;
        qbuf_pending_cnt = pexq->qbuf_pending_cnt;
        dbell_log[0].qbuf_paddr = pexq->qbuf_paddr + 
                                    NX_PEXQ_DBELL_GET_OFFSET(msg_idx);
        while (ii<num_requested_msgs) {
	        U16 new_msg_idx;
                
                if (qbuf_free_cnt == 0) {
                        nx_nic_print5(NULL, 
                            "%s: Failure: qbuf_free_cnt==0\n", 
                            __FUNCTION__);
                        goto failure;
                }
	        msg = &pexq->qbuf_vaddr[msg_idx];
	        qbuf_free_cnt --;
	
	        if ((qbuf_free_cnt & (NX_PEXQ_DBELL_COMPL_THRES - 1)) == 0) {
		        form_dma_sync_msg(msg,
				   msg_idx,
				   pexq->pci_fn,
				   pexq->xdma_hdr.word,
				   pexq->card_fc_array,
				   pexq->compl_in_paddr);
	        } else {
	                memcpy(msg, user_msg, sizeof(*msg));
                        user_msg++;
                        ii++;
                }
                num_dbell_qmsgs++;
                qbuf_pending_cnt++;
	        new_msg_idx = NX_PEXQ_DBELL_INCR_IDX(msg_idx, 1);
                if ((new_msg_idx<msg_idx) || (num_dbell_qmsgs==255)) {
                        /* Here if its time for another doobell ring. */
                        if (qbuf_pending_cnt >= NX_PEXQ_DBELL_MAX_PENDING) {
                                nx_nic_print5(NULL, 
                                    "%s: Failure: qbuf_pending_cnt==0x%x\n", 
                                    __FUNCTION__, qbuf_pending_cnt);
                                goto failure;
                        }
                        /* Log door bell info. */
                        dbell_log[num_dbells].num_msgs = num_dbell_qmsgs;
                        if (++num_dbells == MAX_PEXQ_DBELL) {
                                nx_nic_print5(NULL, 
                                    "%s: Failure: num_dbells==0x%x\n", 
                                    __FUNCTION__, num_dbells);
                                goto failure;
                        }
                        dbell_log[num_dbells].qbuf_paddr = pexq->qbuf_paddr + 
                                    NX_PEXQ_DBELL_GET_OFFSET(new_msg_idx);
                        num_dbell_qmsgs = 0;
                }
                msg_idx = new_msg_idx;
        }
        if (num_dbell_qmsgs) { 
                if (qbuf_pending_cnt >= NX_PEXQ_DBELL_MAX_PENDING) {
                        nx_nic_print5(NULL, 
                            "%s: 1: Failure: qbuf_pending_cnt==0x%x\n", 
                            __FUNCTION__, qbuf_pending_cnt);
                        goto failure;
                }
                /* Log door bell info. */
                dbell_log[num_dbells].num_msgs = num_dbell_qmsgs;
                num_dbells++;
        }
       
        /* We're good to go. */
        /* Update pexq. */ 
	pexq->qbuf_new_idx = msg_idx;
        pexq->qbuf_free_cnt = qbuf_free_cnt;
        pexq->qbuf_pending_cnt = qbuf_pending_cnt;

        /* Ring away. */
        for (ii=0; ii<num_dbells; ii++) {
                U64 post_cmd;
	        post_cmd = (PEXQ_DB_FIELD_ADDR_40(dbell_log[ii].qbuf_paddr) |
		            PEXQ_DB_FIELD_NUM_MSGS(dbell_log[ii].num_msgs) |
		            PEXQ_DB_FIELD_8_QW_PER_MSG |
		            PEXQ_DB_FIELD_DISABLE_COMPL |
		            PEXQ_DB_FIELD_COMPL_HDR_FROM_REG |
		            PEXQ_DB_FIELD_DATA_IN_HOST);
                pexq_ring_doorbell(post_cmd, pexq->dbell_vaddr, 
                            pexq->dbell_reflection_vaddr);
        }

	nx_nic_print7(NULL, "%s: retAddr=%p, qbuf_new %d free %d\n", 
		      __FUNCTION__, __builtin_return_address(0), 
		      msg_idx,
		      pexq->qbuf_free_cnt);
                    

	spin_unlock_bh(&pexq->lock);
        return NX_RCODE_SUCCESS;

failure:
        /* Here if we don't have enough resources. */
	spin_unlock_bh(&pexq->lock);
	return NX_RCODE_NO_HOST_RESOURCE;
}

/* Testing only
 * Enqueue a series of test messages via pexq
 */
#define NUM_TEST_MSGS   8
static void
pexq_db_test(nx_pexq_dbell_t *pexq, 
	     U64 qhdr)
{
        I64 i;
        I64 j;
        unm_msg_t msg[NUM_TEST_MSGS];
	
        for (j=0; j<2; j++) {
                unm_msg_t *p = &msg[0];
                for (i=0; i<NUM_TEST_MSGS; i++) {
			
                        p->hdr.word = qhdr;
                        p->body.values[0] = 0x00000000000000ULL + (j<<4) +i;
                        p->body.values[1] = 0x11111111111100ULL + (j<<4) +i;
                        p->body.values[2] = 0x22222222222200ULL + (j<<4) +i;
                        p->body.values[3] = 0x33333333333300ULL + (j<<4) +i;
                        p->body.values[4] = 0x44444444444400ULL + (j<<4) +i;
                        p->body.values[5] = 0x55555555555500ULL + (j<<4) +i;
                        p->body.values[6] = qhdr;
                        p++;
                }
                nx_schedule_pexqdb(pexq, &msg[0], NUM_TEST_MSGS);
        }
}

static void
pexq_db_do_test(nx_pexq_dbell_t *pexq)
{
	nx_nic_print5(NULL,
		      "%s: Starting\n",
		      __FUNCTION__);
	
        /* Gratuitous doorbell ringging. */
        pexq_db_test(pexq, 0x5400000000440003ULL);
        pexq_db_test(pexq, 0x5400000000450003ULL);
        pexq_db_test(pexq, 0x5400000000460003ULL);
        pexq_db_test(pexq, 0x5400000000470003ULL);

	nx_nic_print5(NULL,
		      "%s: free %d pending %d : compl %d in %d db %d new %d\n",
		      __FUNCTION__,
		      pexq->qbuf_free_cnt,
		      pexq->qbuf_pending_cnt,
		      pexq->qbuf_pending_compl_idx,
		      (U16)*pexq->compl_in_vaddr,
		      pexq->qbuf_needs_db_idx,
		      pexq->qbuf_new_idx);
}

/* Initialize pexq for this pci function
 *   Failure: could no allocate needed host resources
 */

nx_rcode_t 
nx_init_pexq_dbell(struct unm_adapter_s *adapter)
{
	nx_rcode_t rv = NX_RCODE_SUCCESS;
	struct pci_dev *pdev = adapter->pdev;
        nx_pexq_dbell_t *pexq = &adapter->pexq;

	pexq->qbuf_free_cnt = NX_PEXQ_DBELL_BUF_QMSGS;

	/* Setup doorbell */
        pexq->dbell_paddr = (pci_resource_start(pdev,4) +
			     (pexq->db_number << 12));
        pexq->dbell_vaddr = ((void *)
			     ioremap_nocache(pexq->dbell_paddr, 8));
        pexq->dbell_reflection_paddr = (pci_resource_start(pdev,0) +
			     pexq->reflection_offset + (pexq->db_number << 3));
        pexq->dbell_reflection_vaddr = ((void *)
			     ioremap_nocache(pexq->dbell_reflection_paddr, 8));
//// Start debug.
        printk("%s:pci function=0x%x\n", __FUNCTION__, adapter->ahw.pci_func); 
        printk("    pexq_card_fc_q=0x%016llx, pex_card_fc_address=0x%llx\n", 
            pexq->xdma_hdr.word, 
            pexq->card_fc_array);
        printk("    pexq_card_fc_size=0x%x, pexq_q_length=0x%x\n", 
            pexq->card_fc_size,
            pexq->pexq_q_length);
        printk("    pexq_dbell_number=0x%x, dbell_vaddr=%p\n", 
            pexq->db_number, pexq->dbell_vaddr);
//        printk("    pexq_reflection_offset=0x%x, reflection_value=%p\n", 
//            pexq->reflection_offset, readq(pexq->dbell_reflection_vaddr));
//// End debug.

	pexq_db_do_test(pexq);

	return rv;
}

nx_rcode_t 
nx_pexq_allocate_memory(struct unm_adapter_s *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	nx_rcode_t rv = NX_RCODE_SUCCESS;
	nx_pexq_dbell_t *pexq = &adapter->pexq;

	nx_nic_print7(adapter,
		      "%s: pci_fn %d\n",
		      __FUNCTION__, adapter->ahw.pci_func);

        memset(pexq, 0, sizeof (*pexq));

	pexq->pci_fn = adapter->ahw.pci_func;

        /* Make sure that our host settings are compatible with
         * our card settings. */ 
        rv = nx_fw_cmd_query_pexq(adapter->nx_dev, pexq->pci_fn, 
                   &pexq->pexq_q_length, &pexq->card_fc_size);
        if (rv != NX_RCODE_SUCCESS) {
                goto failure;
        }
        if (pexq->card_fc_size < NX_PEXQ_DBELL_BUF_QMSGS) {
		nx_nic_print3(adapter,
			      "%s: Failed: card_fc_size=0x%ux < "
                              "pexq_buf_size=0x%lx",
                              __FUNCTION__, pexq->card_fc_size,
                              NX_PEXQ_DBELL_BUF_SIZE);
                rv = NX_RCODE_NO_HOST_MEM;
                goto failure;
        }
        /*TODO: Need to find actual number of functions.  For now
         * hardwire to 8. */
        if ((pexq->pexq_q_length/8) < NX_PEXQ_DBELL_MAX_PENDING) {
		nx_nic_print3(adapter,
			      "%s: Failed: pexq_q_length=0x%ux incompatible"
                              " with pexq_buf_size=0x%lx",
                              __FUNCTION__, pexq->pexq_q_length,
                              NX_PEXQ_DBELL_BUF_SIZE);
                rv = NX_RCODE_NO_HOST_MEM;
                goto failure;
        }


        spin_lock_init(&pexq->lock);

	/* Allocate space for general pexq msg use 
	 */
#ifdef ESX
	/*
	 * adapter->gfp_mask should have the appropriate mask set
	 * to instruct __get_free_pages() to allocate pages from the
	 * appropriate memory zone base on the adapter DMA range.
	 */
	pexq->qbuf_vaddr = ((unm_msg_t *)
			    __get_free_pages(adapter->gfp_mask|GFP_KERNEL, 
			 		     NX_PEXQ_DBELL_BUF_ORDER));
#else
        pexq->qbuf_vaddr = ((unm_msg_t *)
			    __get_free_pages(GFP_KERNEL, 
					     NX_PEXQ_DBELL_BUF_ORDER));
#endif

        if (pexq->qbuf_vaddr == NULL) {
		nx_nic_print3(adapter,
			      "Could not allocate pexq_qbuf_buf\n");
                rv = NX_RCODE_NO_HOST_MEM;
		goto failure;
        }
	
        pexq->qbuf_paddr = pci_map_single(pdev, 
                                          (void *)pexq->qbuf_vaddr, 
                                          NX_PEXQ_DBELL_BUF_SIZE,
                                          PCI_DMA_TODEVICE); 
        if (pexq->qbuf_paddr == (dma_addr_t)0) {
		nx_nic_print3(adapter,
			      "Could not map qbuf_vaddr\n");
                rv = NX_RCODE_NO_HOST_MEM;
		goto failure;
        }
//// Start debug.
//        printk("%s: qbuf_paddr=0x%llx\n", __FUNCTION__, 
//            pexq->qbuf_paddr);
//// End debug.

#ifdef ESX
	/*
	 * adapter->gfp_mask should have the appropriate mask set
	 * to instruct __get_free_pages() to allocate pages from the
	 * appropriate memory zone base on the adapter DMA range.
	 */
	pexq->compl_in_vaddr = ((U64 *)
				__get_free_pages(adapter->gfp_mask|GFP_KERNEL, 0));
#else
	pexq->compl_in_vaddr = ((U64 *)
				__get_free_pages(GFP_KERNEL, 0));
#endif

        if (pexq->compl_in_vaddr == NULL) {
		nx_nic_print3(adapter,
			      "Could not allocate compl_in_vaddr\n");
                rv = NX_RCODE_NO_HOST_MEM;
		goto failure;
        }
	memset(pexq->compl_in_vaddr, 0, PAGE_SIZE);
	
        pexq->compl_in_paddr = pci_map_single(pdev, 
					      (void *)pexq->compl_in_vaddr,
					      PAGE_SIZE,
					      PCI_DMA_FROMDEVICE); 
        if (pexq->compl_in_paddr == (dma_addr_t)0) {
		nx_nic_print3(adapter,
			      "Could not map compl_in_vaddr\n");
                rv = NX_RCODE_NO_HOST_MEM;
		goto failure;
        }

        return NX_RCODE_SUCCESS;
	
 failure:
	nx_free_pexq_dbell(adapter);
	return rv;
}


/* Destroy pexq setup for this pci function
 */

void  
nx_free_pexq_dbell(struct unm_adapter_s *adapter)
{
	nx_pexq_dbell_t *pexq = &adapter->pexq;
	struct pci_dev *pdev = adapter->pdev;
	
        nx_nic_print7(adapter,
		      "%s:\n", __FUNCTION__);
	
	if (pexq->qbuf_paddr != (dma_addr_t)0) {
		pci_unmap_single(pdev, pexq->qbuf_paddr,
				 NX_PEXQ_DBELL_BUF_SIZE,
				 PCI_DMA_TODEVICE);
	}

	if (pexq->qbuf_vaddr != NULL) {
		free_pages((unsigned long) (pexq->qbuf_vaddr), 
			   NX_PEXQ_DBELL_BUF_ORDER);
	}

	if (pexq->compl_in_paddr != (dma_addr_t)0) {
		pci_unmap_single(pdev, pexq->compl_in_paddr,
				 PAGE_SIZE,
				 PCI_DMA_FROMDEVICE);
	}
	
	if (pexq->compl_in_vaddr != NULL) {
		free_pages((unsigned long) (pexq->compl_in_vaddr), 0);
	}
	
	if (pexq->dbell_vaddr != NULL) {
		iounmap(pexq->dbell_vaddr);
	}
	
	memset(pexq, 0, sizeof (*pexq));
}
