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
 /********************************************************************
  * Host FW interface functions.
  *
  ********************************************************************/




/* TB removed after resolve */

#include "nx_errorcode.h"
#include "nxplat.h"

#if 0
#include "message.h"
#include "unm_brdcfg.h"
#endif
#include "unm_nic.h"

#include "unm_inc.h"
#include "nic_cmn.h"

#include "nic_phan_reg.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"
#include "pegnet_msgtypes.h"

#define NX_OS_CRB_UDELAY_VALUE  1000 /* arbitrary usec delay */
#define NX_OS_CRB_RETRY_COUNT   4000 /* arbitrary retries */

#define NX_OS_API_LOCK_DRIVER   0x01234567

#define NX_PEGNET_H2C_MAJOR        UNM_MSGQ_SQM_TYPE1
#define NX_PEGNET_H2C_MINOR        (UNM_MSGQ_TYPE1_FIRST_AVAIL+3)

int
api_lock(nx_dev_handle_t drv_handle)
{
#ifdef NX_OS_USE_API_LOCK
	int done = 0, timeout = 0;

	while (!done) {
		/* Acquire PCIE HW semaphore5 */
		nx_os_nic_reg_read_w0(drv_handle, UNM_PCIE_REG(PCIE_SEM5_LOCK), &done);
		
		if (done == 1) {
			break;
		}
		if (timeout >= NX_OS_CRB_RETRY_COUNT) {
			NX_DBGPRINTF(NX_DBG_ERROR, 
				     ("%s: api lock timeout.\n",
				      __FUNCTION__));
			return -1;
		}
		timeout++;
		
		/*
		 * Yield CPU
		 */
		nx_os_udelay(NX_OS_CRB_UDELAY_VALUE);
#ifdef NX_OS_HAS_SCHEDULE
		nx_os_schedule();
#else
		{
			int i;
			for(i = 0; i < 20; i++) {
				nx_os_relax(100000);
			}
		}
#endif
	}
	
	//nx_os_nic_reg_write_w1(drv_handle, UNM_API_LOCK_ID, NX_OS_API_LOCK_DRIVER);
#endif
	return 0;
}

int
api_unlock(nx_dev_handle_t drv_handle)
{
#ifdef NX_OS_USE_API_LOCK
	int val;
	
	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));
	
	/* Release PCIE HW semaphore5 */
	nx_os_nic_reg_read_w0(drv_handle, UNM_PCIE_REG(PCIE_SEM5_UNLOCK), &val);
#endif
	return 0;
}


/*
 * Poll for response and return correct endianess NX_CDRP_RSP code.
 */

static U32
poll_rsp(nx_dev_handle_t drv_handle) 
{
	U32 		rsp = NX_CDRP_RSP_OK;
	I32 		i = 0;

	struct unm_adapter_s *adapter 
				= (struct unm_adapter_s *)drv_handle;

	U32			old_alive_counter = NXRD32(adapter,
              			      UNM_PEG_ALIVE_COUNTER);
	U32			new_alive_counter = old_alive_counter;
	uint64_t cur_fn = (uint64_t)poll_rsp;

	do {
		U32 raw_rsp;

		/*
		 * Yield CPU
		 */
		nx_os_udelay(NX_OS_CRB_UDELAY_VALUE);
#ifdef NX_OS_HAS_SCHEDULE
		nx_os_schedule();
#else
		{
			int i;
			for(i = 0; i < 20; i++) {
				nx_os_relax(100000);
			}
		}
#endif

		nx_os_nic_reg_read_w1(drv_handle, NX_CDRP_CRB_OFFSET,
				      &raw_rsp);

		i++;

		if(i > (NX_OS_CRB_RETRY_COUNT >> 1)) {
    		new_alive_counter = NXRD32(adapter,
              			      UNM_PEG_ALIVE_COUNTER);
			if(old_alive_counter == new_alive_counter) {
				NX_NIC_TRC_FN(adapter, cur_fn, NX_CDRP_RSP_TIMEOUT);
				return NX_CDRP_RSP_TIMEOUT;
			}
		}

		if(i > NX_OS_CRB_RETRY_COUNT) {
			NX_NIC_TRC_FN(adapter, cur_fn, NX_CDRP_RSP_TIMEOUT);
			return NX_CDRP_RSP_TIMEOUT;
		}
		rsp = NX_OS_LE32_TO_CPU(raw_rsp);
	} while (!NX_CDRP_IS_RSP(rsp));

	NX_NIC_TRC_FN(adapter, cur_fn, rsp);
	return(rsp);
}

U32
issue_cmd(nx_dev_handle_t drv_handle,
	  U32 pci_fn,
	  U32 version,
	  U32 input_arg1,
	  U32 input_arg2,
	  U32 input_arg3,
	  U32 cmd,
	  U32 *output_arg1,
	  U32 *output_arg2,
	  U32 *output_arg3)
{
	U32 rsp;
	U32 signature = 0;
	U32 rcode = NX_RCODE_SUCCESS;
	int fw_reset_count = 0;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)drv_handle;
	uint64_t cur_fn = (uint64_t)issue_cmd;

	api_lock(drv_handle);
	fw_reset_count = NXRD32(adapter,UNM_FW_RESET);
	api_unlock(drv_handle);

	if(adapter->is_up != FW_DEAD && fw_reset_count == 0) {

		signature = NX_CDRP_SIGNATURE_MAKE(pci_fn, version);
	
		/* Acquire semaphore before accessing CRB
		 */
		if (api_lock(drv_handle)) {
			adapter->is_up = FW_DEAD;
			NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_TIMEOUT);
			return NX_RCODE_TIMEOUT;
		}

		nx_os_nic_reg_write_w1(drv_handle, NX_SIGN_CRB_OFFSET,
				       NX_OS_CPU_TO_LE32(signature));

		nx_os_nic_reg_write_w1(drv_handle, NX_ARG1_CRB_OFFSET,
				       NX_OS_CPU_TO_LE32(input_arg1));

		nx_os_nic_reg_write_w1(drv_handle, NX_ARG2_CRB_OFFSET,
				       NX_OS_CPU_TO_LE32(input_arg2));

		nx_os_nic_reg_write_w1(drv_handle, NX_ARG3_CRB_OFFSET,
			    	   NX_OS_CPU_TO_LE32(input_arg3));

		nx_os_nic_reg_write_w1(drv_handle, NX_CDRP_CRB_OFFSET,
				       NX_OS_CPU_TO_LE32(NX_CDRP_FORM_CMD(cmd)));
	
		rsp = poll_rsp(drv_handle);

		if (rsp == NX_CDRP_RSP_TIMEOUT) {
			NX_DBGPRINTF(NX_DBG_ERROR, 
				     ("%s:Timeout. No card response.\n",
				      __FUNCTION__));

			adapter->is_up = FW_DEAD;
			fw_reset_count = NXRD32(adapter,UNM_FW_RESET);
			if(fw_reset_count != 1) {
				NXWR32(adapter,UNM_FW_RESET,1);
				NX_NIC_TRC_FN(adapter, cur_fn, fw_reset_count);
			}

			rcode = NX_RCODE_TIMEOUT;
			NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		} else if (rsp == NX_CDRP_RSP_FAIL) {
			nx_os_nic_reg_read_w1(drv_handle, NX_ARG1_CRB_OFFSET, &rsp);

			rcode = (nx_rcode_t)(NX_OS_LE32_TO_CPU(rsp));
			NX_DBGPRINTF(NX_DBG_ERROR,("%s:Fail card response. rcode:%x\n",
					   __FUNCTION__, rcode));
			NX_NIC_TRC_FN(adapter, cur_fn, rcode);

		} else if (rsp == NX_CDRP_RSP_OK) {
			if (output_arg1)
				nx_os_nic_reg_read_w1(drv_handle, NX_ARG1_CRB_OFFSET, output_arg1);

			if (output_arg2)
				nx_os_nic_reg_read_w1(drv_handle, NX_ARG2_CRB_OFFSET, output_arg2);

			if (output_arg3)
				nx_os_nic_reg_read_w1(drv_handle, NX_ARG3_CRB_OFFSET, output_arg3);
		}

        /* Release semaphore
         */
        api_unlock(drv_handle);

		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		return rcode;

	} else {
		/* Need to check if we need diff error code... */
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_TIMEOUT);
		return NX_RCODE_TIMEOUT;	
	}

}

/*************************************************************
 *				DEV
 *************************************************************/

#ifdef NX_USE_NEW_ALLOC
nx_rcode_t 
nx_os_dev_alloc(nx_host_nic_t **ppnx_dev,
		nx_dev_handle_t drv_handle,
		U32 pci_func,
		U32 max_rx_ctxs,
		U32 max_tx_ctxs)
{
	int len;
	nx_host_nic_t *pnx_dev;
	nx_rcode_t rcode;

	len = (sizeof(nx_host_nic_t) + 
	       (max_rx_ctxs * sizeof(nx_host_rx_ctx_t*)) +
	       (max_tx_ctxs * sizeof(nx_host_tx_ctx_t*)));

	rcode = nx_os_alloc_mem(drv_handle,
				(void **)ppnx_dev,
				len,
				NX_OS_ALLOC_KERNEL,
				0);

	if(rcode != NX_RCODE_SUCCESS){
		return NX_RCODE_NO_HOST_MEM;
	}

	pnx_dev = *ppnx_dev;
	nx_os_zero_memory(pnx_dev, len);

	pnx_dev->alloc_len = len;
	pnx_dev->alloc_rx_ctxs = max_rx_ctxs;
	pnx_dev->alloc_tx_ctxs = max_tx_ctxs;
	pnx_dev->nx_drv_handle = drv_handle;
	pnx_dev->pci_func = pci_func;

	pnx_dev->rx_ctxs = ((nx_host_rx_ctx_t**)
			    (((U8*)pnx_dev) + sizeof(nx_host_nic_t)));
	pnx_dev->tx_ctxs = ((nx_host_tx_ctx_t**)
			    (((U8*)pnx_dev->rx_ctxs) + 
			     (max_rx_ctxs * sizeof(nx_host_rx_ctx_t*))));

	return NX_RCODE_SUCCESS;		
}

nx_rcode_t 
nx_os_dev_free(nx_host_nic_t *nx_dev)
{
	nx_dev_handle_t drv_handle;
	
	if (nx_dev == NULL) {
		return NX_RCODE_SUCCESS;
	}
	drv_handle = nx_dev->nx_drv_handle;

	/* Warn on remaining contexts */
	if (nx_dev->active_rx_ctxs > 0) {
		NX_WARN("Rx contexts still exist\n");
	}
	if (nx_dev->active_tx_ctxs > 0) {
		NX_WARN("Tx contexts still exist\n");
	}

	nx_os_free_mem(drv_handle, 
		       nx_dev,
		       nx_dev->alloc_len,
		       0);

	return NX_RCODE_SUCCESS;	
}

static I32
find_open_rx_id(nx_host_nic_t *pnx_dev)
{
	I32 i;

	for (i=0; i<pnx_dev->alloc_rx_ctxs; i++) {
		if (pnx_dev->rx_ctxs[i] == NULL && ((pnx_dev->rx_ctxs_state & (1 << i)) == 0)) {
			return i;
		}
	}
	
	if (i >= pnx_dev->alloc_rx_ctxs) {
		return -1;
	}

	return i;
}

static I32
find_open_tx_id(nx_host_nic_t *pnx_dev)
{
	I32 i;

	for (i=0; i<pnx_dev->alloc_tx_ctxs; i++) {
		if (pnx_dev->tx_ctxs[i] == NULL) {
			return i;
		}
	}
	
	if (i >= pnx_dev->alloc_tx_ctxs) {
		return -1;
	}

	return i;
}
#endif /* NX_USE_NEW_ALLOC */

/*************************************************************
 *				RX
 *************************************************************/

nx_rcode_t 
nx_fw_cmd_create_rx_ctx_alloc(nx_host_nic_t* nx_dev, 
			      U32 num_rds_rings,
			      U32 num_sds_rings,
			      U32 num_rules,
			      nx_host_rx_ctx_t **pprx_ctx)
{
	nx_dev_handle_t drv_handle;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	nx_host_rx_ctx_t *prx_ctx;
	U32 len = 0;
	U32 index = 0;
	I32 id = 0;
	struct unm_adapter_s *adapter; 
	uint64_t cur_fn = (uint64_t)nx_fw_cmd_create_rx_ctx_alloc;

	if (nx_dev == NULL ||
	    pprx_ctx == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;

#ifdef NX_USE_NEW_ALLOC
	if ((id = find_open_rx_id(nx_dev)) < 0) {
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_MAX_EXCEEDED);
		return NX_RCODE_MAX_EXCEEDED;
	}
#endif
	if (id == 0) {
		num_rules = MAX_SUPPORTED_FILTERS - adapter->num_rx_queues;
	}

	len = (sizeof(nx_host_rx_ctx_t) +
	       (sizeof(nx_host_rds_ring_t) * num_rds_rings) +
	       (sizeof(nx_host_sds_ring_t) * num_sds_rings) +
	       (sizeof(nx_rx_rule_t) * num_rules));

	rcode = nx_os_alloc_mem(drv_handle,
				(void **)pprx_ctx,
				len,
				NX_OS_ALLOC_KERNEL,
				0);

	if(rcode != NX_RCODE_SUCCESS){
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_NO_HOST_MEM);
		return NX_RCODE_NO_HOST_MEM;
	}

	prx_ctx = *pprx_ctx;
	nx_os_zero_memory(prx_ctx, len);

	prx_ctx->nx_dev = nx_dev;
	prx_ctx->alloc_len = len;
	prx_ctx->state = NX_HOST_CTX_STATE_ALLOCATED;
	prx_ctx->num_rds_rings = (U16)num_rds_rings;
	prx_ctx->num_sds_rings = (U16)num_sds_rings;
	prx_ctx->num_rules = (U16)num_rules;
	prx_ctx->pci_func = nx_dev->pci_func;

	prx_ctx->rds_rings = 
		(nx_host_rds_ring_t *)(((char*)prx_ctx) + 
				       sizeof(nx_host_rx_ctx_t));
	
	prx_ctx->sds_rings = 
		(nx_host_sds_ring_t *)(((char*)(prx_ctx->rds_rings)) +
				       (sizeof(nx_host_rds_ring_t) * 
					num_rds_rings));
	prx_ctx->rules = 
		(nx_rx_rule_t *)(((char*)(prx_ctx->sds_rings)) +
				       (sizeof(nx_host_sds_ring_t) * 
					num_sds_rings));


	for (index = 0; index < num_rds_rings; index++) {
		prx_ctx->rds_rings[index].parent_ctx = prx_ctx;
	}
	for (index = 0; index < num_sds_rings; index++) {
		prx_ctx->sds_rings[index].parent_ctx = prx_ctx;
	}

	prx_ctx->this_id = id;
#ifdef NX_USE_NEW_ALLOC
	nx_dev->rx_ctxs[id] = prx_ctx;
	nx_dev->active_rx_ctxs++;
#endif
	NX_NIC_TRC_FN(adapter, cur_fn, id);
	NX_NIC_TRC_FN(adapter, cur_fn, nx_dev->active_rx_ctxs);

	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;	
}

nx_rcode_t 
nx_fw_cmd_create_rx_ctx_free(nx_host_nic_t* nx_dev, 
			     nx_host_rx_ctx_t *prx_ctx)
{
	nx_dev_handle_t drv_handle;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_rx_ctx_free;

	if (nx_dev == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	
	if (prx_ctx == NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return NX_RCODE_SUCCESS;
	}

#ifdef NX_USE_NEW_ALLOC
	if (nx_dev->rx_ctxs[prx_ctx->this_id] != prx_ctx ||
	    nx_dev->active_rx_ctxs < 1) {
		NX_ERR("fatal error\n");
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_CMD_FAILED);
		return NX_RCODE_CMD_FAILED;
	}

	nx_dev->rx_ctxs[prx_ctx->this_id] = NULL;
	nx_dev->active_rx_ctxs--;
	NX_NIC_TRC_FN(adapter, cur_fn, nx_dev->active_rx_ctxs);
#endif
	nx_os_free_mem(drv_handle, 
		       prx_ctx,
		       prx_ctx->alloc_len,
		       0);
	prx_ctx = NULL;
	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;	
}

nx_rcode_t 
nx_fw_cmd_create_rx_ctx_alloc_dma(nx_host_nic_t* nx_dev, 
				  U32 num_rds_rings,
				  U32 num_sds_rings,
				  struct nx_dma_alloc_s *hostrq,
				  struct nx_dma_alloc_s *hostrsp)
{
	nx_dev_handle_t drv_handle;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	U32 memflags = 0;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_rx_ctx_alloc_dma;

	if (nx_dev == NULL ||
	    hostrq == NULL ||
	    hostrsp == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;
	
	nx_os_zero_memory(hostrq, sizeof(struct nx_dma_alloc_s));
	nx_os_zero_memory(hostrsp, sizeof(struct nx_dma_alloc_s));

	hostrq->size = SIZEOF_HOSTRQ_RX(nx_hostrq_rx_ctx_t,
					num_rds_rings,
					num_sds_rings);
	
	rcode = nx_os_alloc_dma_mem(drv_handle,
				    &hostrq->ptr,
				    &hostrq->phys,
				    hostrq->size,
				    memflags);

	if(rcode != NX_RCODE_SUCCESS){
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		return NX_RCODE_NO_HOST_MEM;
	}

	hostrsp->size = SIZEOF_CARDRSP_RX(nx_cardrsp_rx_ctx_t,
					  num_rds_rings,
					  num_sds_rings);

	rcode = nx_os_alloc_dma_mem(drv_handle,
				    &hostrsp->ptr,
				    &hostrsp->phys,
				    hostrsp->size,
				    memflags);

	if(rcode != NX_RCODE_SUCCESS){
		nx_os_free_dma_mem(drv_handle, 
				   hostrq->ptr,
				   hostrq->phys, 
				   hostrq->size, 
				   memflags);
		nx_os_zero_memory(hostrq, sizeof(struct nx_dma_alloc_s));
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		return NX_RCODE_NO_HOST_MEM;
	}

	nx_os_zero_memory(hostrq->ptr, hostrq->size);
	nx_os_zero_memory(hostrsp->ptr, hostrsp->size);	

	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;
}

nx_rcode_t 
nx_fw_cmd_create_rx_ctx_free_dma(nx_host_nic_t* nx_dev, 
				 struct nx_dma_alloc_s *hostrq,
				 struct nx_dma_alloc_s *hostrsp)
{
	nx_dev_handle_t drv_handle;

	if (nx_dev == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;

	if (hostrq) {
		nx_os_free_dma_mem(drv_handle, 
				   hostrq->ptr,
				   hostrq->phys, 
				   hostrq->size, 
				   0);
		nx_os_zero_memory(hostrq, 
				  sizeof(struct nx_dma_alloc_s));
	}

	if (hostrsp) {
		nx_os_free_dma_mem(drv_handle, 
				   hostrsp->ptr,
				   hostrsp->phys, 
				   hostrsp->size, 
				   0);
		nx_os_zero_memory(hostrsp, 
				  sizeof(struct nx_dma_alloc_s));
	}

	return NX_RCODE_SUCCESS;
}

/*
 * This Function fill up request to FW to create RXcontext.
 * It then process response from FW and return to upper level.
 */ 
nx_rcode_t 
nx_fw_cmd_create_rx_ctx(nx_host_rx_ctx_t *prx_ctx,
			struct nx_dma_alloc_s *hostrq,
			struct nx_dma_alloc_s *hostrsp)
{
	nx_hostrq_rx_ctx_t *prq_rx_ctx;
	nx_cardrsp_rx_ctx_t *prsp_rx_ctx;
	nx_hostrq_rds_ring_t *prq_rds_ring = NULL;
	nx_hostrq_sds_ring_t *prq_sds_ring = NULL;
	nx_cardrsp_rds_ring_t *prsp_rds_ring = NULL;
	nx_cardrsp_sds_ring_t *prsp_sds_ring = NULL;
	nx_host_rds_ring_t *prds_rings = NULL;
	nx_host_sds_ring_t *psds_rings = NULL;
	nx_dev_handle_t drv_handle;
	U64 tmp_phys_addr = 0;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	I32 i = 0;
	U32 temp = 0;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_rx_ctx;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	if (prx_ctx == NULL ||
			hostrq == NULL ||
			hostrsp == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = prx_ctx->nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;
	prq_rx_ctx = (nx_hostrq_rx_ctx_t *)hostrq->ptr;
	prsp_rx_ctx = (nx_cardrsp_rx_ctx_t *)hostrsp->ptr;

	if (NX_HOST_CTX_STATE_ALLOCATED != prx_ctx->state){
		NX_DBGPRINTF(NX_DBG_INFO,("%s:Invalid RX CTX state:%x\n",
					__FUNCTION__, prx_ctx->state));

		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_INVALID_ARGS);
		return (NX_RCODE_INVALID_ARGS);
	}

	prq_rx_ctx->host_rsp_dma_addr = 
		NX_OS_CPU_TO_LE64((nx_os_dma_addr_to_u64(hostrsp->phys)));

	temp = (NX_CAP0_LEGACY_CONTEXT | 
			NX_CAP0_LEGACY_MN |
			NX_CAP0_LRO);
	if (prx_ctx->multi_context) {
		temp |= NX_CAP0_MULTI_CONTEXT;
	}
	if (!prx_ctx->chaining_allowed) {
		temp |= NX_CAP0_JUMBO_CONTIGUOUS | NX_CAP0_LRO_CONTIGUOUS;
	}

	prq_rx_ctx->capabilities[0] = NX_OS_CPU_TO_LE32(temp);
	prq_rx_ctx->host_int_crb_mode = 
		NX_OS_CPU_TO_LE32(NX_HOST_INT_CRB_MODE_SHARED); 	
	prq_rx_ctx->host_rds_crb_mode = 
		NX_OS_CPU_TO_LE32(NX_HOST_RDS_CRB_MODE_UNIQUE); 	

	prq_rx_ctx->num_rds_rings = NX_OS_CPU_TO_LE16(prx_ctx->num_rds_rings);
	prq_rx_ctx->num_sds_rings = NX_OS_CPU_TO_LE16(prx_ctx->num_sds_rings);
	prq_rx_ctx->rds_ring_offset = NX_OS_CPU_TO_LE32(0);
	prq_rx_ctx->sds_ring_offset = 
		NX_OS_CPU_TO_LE32(prq_rx_ctx->rds_ring_offset +
				(sizeof(nx_hostrq_rds_ring_t) *
				 prq_rx_ctx->num_rds_rings));

	prq_rds_ring = (nx_hostrq_rds_ring_t *)(prq_rx_ctx->data + 
			prq_rx_ctx->rds_ring_offset);	
	prds_rings = prx_ctx->rds_rings;

	for (i = 0; i < prx_ctx->num_rds_rings; i++) {
		prq_rds_ring[i].host_phys_addr = 
			NX_OS_CPU_TO_LE64(nx_os_dma_addr_to_u64
					(prds_rings[i].host_phys));
		prq_rds_ring[i].ring_size = 
			NX_OS_CPU_TO_LE32(prds_rings[i].ring_size);
		prq_rds_ring[i].ring_kind = 
			NX_OS_CPU_TO_LE32(prds_rings->ring_kind);
		prq_rds_ring[i].buff_size = 
			NX_OS_CPU_TO_LE64(prds_rings[i].buff_size); 
	}

	prq_sds_ring = (nx_hostrq_sds_ring_t*)(prq_rx_ctx->data + 
			prq_rx_ctx->sds_ring_offset);	
	psds_rings 	= prx_ctx->sds_rings;

	for (i = 0; i < prx_ctx->num_sds_rings; i++) {
		prq_sds_ring[i].host_phys_addr = 
			NX_OS_CPU_TO_LE64((nx_os_dma_addr_to_u64
						(psds_rings[i].host_phys)));
		prq_sds_ring[i].ring_size = 
			NX_OS_CPU_TO_LE32(psds_rings[i].ring_size);
		prq_sds_ring[i].msi_index = 
			NX_OS_CPU_TO_LE32(psds_rings[i].msi_index);
	}

	tmp_phys_addr = nx_os_dma_addr_to_u64(hostrq->phys);

	rcode = issue_cmd(drv_handle,
			prx_ctx->pci_func,
			NXHAL_VERSION,
			(U32)(tmp_phys_addr >> 32),
			((U32)tmp_phys_addr),
			hostrq->size,
			NX_CDRP_CMD_CREATE_RX_CTX,
			NULL,
			NULL,
			NULL);

	if (rcode != NX_RCODE_SUCCESS) {
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		goto failure;
	}

	prsp_rds_ring = ((nx_cardrsp_rds_ring_t *)
			&prsp_rx_ctx->data[prsp_rx_ctx->rds_ring_offset]);
	prds_rings = prx_ctx->rds_rings;

	for (i = 0; i < NX_OS_LE32_TO_CPU(prsp_rx_ctx->num_rds_rings); i++){
		prds_rings[i].host_rx_producer = 
			NX_OS_LE32_TO_CPU(prsp_rds_ring[i].host_producer_crb);
	}

	prsp_sds_ring =((nx_cardrsp_sds_ring_t *)
			&prsp_rx_ctx->data[prsp_rx_ctx->sds_ring_offset]);
	psds_rings = prx_ctx->sds_rings;

	for (i = 0; i < NX_OS_LE32_TO_CPU(prsp_rx_ctx->num_sds_rings); i++){
		psds_rings[i].host_sds_consumer = 
			NX_OS_LE32_TO_CPU(prsp_sds_ring[i].host_consumer_crb);
		psds_rings[i].interrupt_crb =
			NX_OS_LE32_TO_CPU(prsp_sds_ring[i].interrupt_crb);
#if 0
		/* This should be used somewhere. It's the interrupt 
		   mask crb */
		psds_rings[i].host_interrupt_crb = 
			NX_OS_LE32_TO_CPU(prsp_sds_ring[i].interrupt_crb);
#endif
	}

	prx_ctx->state = NX_OS_LE32_TO_CPU(prsp_rx_ctx->host_ctx_state);	
	prx_ctx->context_id = (U32)(NX_OS_LE16_TO_CPU(prsp_rx_ctx->context_id));
	prx_ctx->port = (U32)(NX_OS_LE16_TO_CPU(prsp_rx_ctx->virt_port));
	prx_ctx->num_fn_per_port = NX_OS_LE32_TO_CPU(prsp_rx_ctx->num_fn_per_port);

#ifdef NX_USE_NEW_ALLOC
	if (prx_ctx->nx_dev->default_rx_ctx == NULL) {
		prx_ctx->nx_dev->default_rx_ctx = prx_ctx;
		NX_NIC_TRC_FN(adapter, cur_fn, prx_ctx);
	}
#endif
failure:

	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);
} 	/* end of create_rx_ctx */ 

/*
 * This Function will destroy a given RX context.
 */ 
nx_rcode_t 
nx_fw_cmd_destroy_rx_ctx(nx_host_rx_ctx_t *prx_ctx,
			 U32 destroy_cmd)
{
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_destroy_rx_ctx;

	if (prx_ctx == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = prx_ctx->nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;

	if (prx_ctx->state > NX_HOST_CTX_STATE_MAX){ 

		NX_DBGPRINTF(NX_DBG_INFO,("%s:Invalid RX CTX state:%x\n",
					__FUNCTION__, prx_ctx->state));
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_INVALID_STATE);
		return (NX_RCODE_INVALID_STATE);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, destroy_cmd);
	if(destroy_cmd != NX_DESTROY_CTX_NO_FWCMD ) {
		rcode = issue_cmd(drv_handle,
				prx_ctx->pci_func,
				NXHAL_VERSION,
				prx_ctx->context_id,
				destroy_cmd,
				0,
				NX_CDRP_CMD_DESTROY_RX_CTX,
				NULL,
				NULL,
				NULL);
	}
#ifdef NX_USE_NEW_ALLOC
	if (prx_ctx->nx_dev->default_rx_ctx == prx_ctx) {
		/* TBD: This needs to find another active Rx */
		NX_NIC_TRC_FN(adapter, cur_fn, prx_ctx);
		prx_ctx->nx_dev->default_rx_ctx = NULL;
	}
#endif

	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);
}	/* end destroy_rx_ctx */  


/*************************************************************
 *		 		TX  
 *************************************************************/

nx_rcode_t 
nx_fw_cmd_create_tx_ctx_alloc(nx_host_nic_t* nx_dev, 
			      U32 num_cds_rings,
			      nx_host_tx_ctx_t **pptx_ctx)
{
	nx_dev_handle_t drv_handle;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	nx_host_tx_ctx_t *ptx_ctx;
	U32 len = 0;
	I32 id = 0;

	if (nx_dev == NULL ||
	    pptx_ctx == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;

#ifdef NX_USE_NEW_ALLOC
	if ((id = find_open_tx_id(nx_dev)) < 0) {
		return NX_RCODE_MAX_EXCEEDED;
	}
#endif
	len = (sizeof(nx_host_tx_ctx_t) +
	       (sizeof(nx_host_cds_ring_t) * num_cds_rings));
	
	rcode = nx_os_alloc_mem(drv_handle,
				(void **)pptx_ctx,
				len,
				NX_OS_ALLOC_KERNEL,
				0);

	if(rcode != NX_RCODE_SUCCESS){
		return NX_RCODE_NO_HOST_MEM;
	}

	ptx_ctx = *pptx_ctx;
	nx_os_zero_memory(ptx_ctx, len);

	ptx_ctx->nx_dev = nx_dev;
	ptx_ctx->alloc_len = len;
	ptx_ctx->state = NX_HOST_CTX_STATE_ALLOCATED;
	ptx_ctx->pci_func = nx_dev->pci_func;

	ptx_ctx->cds_ring = 
		(nx_host_cds_ring_t *)(((char*)ptx_ctx) +
				       sizeof(nx_host_tx_ctx_t));

	ptx_ctx->this_id = id;
#ifdef NX_USE_NEW_ALLOC
	nx_dev->tx_ctxs[id] = ptx_ctx;
	nx_dev->active_tx_ctxs++;
#endif
	return NX_RCODE_SUCCESS;	
}

nx_rcode_t 
nx_fw_cmd_create_tx_ctx_free(nx_host_nic_t* nx_dev, 
			     nx_host_tx_ctx_t *ptx_ctx)
{
	nx_dev_handle_t drv_handle;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_tx_ctx_free;

	if (nx_dev == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *) drv_handle;

	if (ptx_ctx == NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
		return NX_RCODE_SUCCESS;
	}

#ifdef NX_USE_NEW_ALLOC
	if (nx_dev->tx_ctxs[ptx_ctx->this_id] != ptx_ctx ||
	    nx_dev->active_tx_ctxs < 1) {
		NX_ERR("fatal error\n");
		NX_NIC_TRC_FN(adapter, cur_fn, nx_dev->tx_ctxs[ptx_ctx->this_id]);
		NX_NIC_TRC_FN(adapter, cur_fn, nx_dev->active_tx_ctxs);
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_CMD_FAILED);
		return NX_RCODE_CMD_FAILED;
	}

	nx_dev->tx_ctxs[ptx_ctx->this_id] = NULL;
	nx_dev->active_tx_ctxs--;
#endif
	nx_os_free_mem(drv_handle, 
		       ptx_ctx,
		       ptx_ctx->alloc_len,
		       0);

	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;	
}

nx_rcode_t 
nx_fw_cmd_create_tx_ctx_alloc_dma(nx_host_nic_t* nx_dev, 
				  U32 num_cds_rings,
				  struct nx_dma_alloc_s *hostrq,
				  struct nx_dma_alloc_s *hostrsp)
{
	nx_dev_handle_t drv_handle;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	U32 memflags = 0;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_tx_ctx_alloc_dma;

	if (nx_dev == NULL ||
	    hostrq == NULL ||
	    hostrsp == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *) drv_handle;

	nx_os_zero_memory(hostrq, sizeof(struct nx_dma_alloc_s));
	nx_os_zero_memory(hostrsp, sizeof(struct nx_dma_alloc_s));

	hostrq->size = SIZEOF_HOSTRQ_TX(nx_hostrq_tx_ctx_t);
	
	rcode = nx_os_alloc_dma_mem(drv_handle,
				    &hostrq->ptr,
				    &hostrq->phys,
				    hostrq->size,
				    memflags);

	if(rcode != NX_RCODE_SUCCESS){
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_NO_HOST_MEM);
		return NX_RCODE_NO_HOST_MEM;
	}

	hostrsp->size = SIZEOF_CARDRSP_TX(nx_cardrsp_tx_ctx_t);

	rcode = nx_os_alloc_dma_mem(drv_handle,
				    &hostrsp->ptr,
				    &hostrsp->phys,
				    hostrsp->size,
				    memflags);

	if(rcode != NX_RCODE_SUCCESS){
		nx_os_free_dma_mem(drv_handle, 
				   hostrq->ptr,
				   hostrq->phys, 
				   hostrq->size, 
				   memflags);
		nx_os_zero_memory(hostrq, sizeof(struct nx_dma_alloc_s));
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_NO_HOST_MEM);
		return NX_RCODE_NO_HOST_MEM;
	}

	nx_os_zero_memory(hostrq->ptr, hostrq->size);
	nx_os_zero_memory(hostrsp->ptr, hostrsp->size);	
	
	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;
}

nx_rcode_t 
nx_fw_cmd_create_tx_ctx_free_dma(nx_host_nic_t* nx_dev, 
				 struct nx_dma_alloc_s *hostrq,
				 struct nx_dma_alloc_s *hostrsp)
{
	nx_dev_handle_t drv_handle;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_tx_ctx_free_dma;


	if (nx_dev == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *) drv_handle;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (hostrq) {
		nx_os_free_dma_mem(drv_handle, 
				   hostrq->ptr,
				   hostrq->phys, 
				   hostrq->size, 
				   0);
		nx_os_zero_memory(hostrq, 
				  sizeof(struct nx_dma_alloc_s));
	}

	if (hostrsp) {
		nx_os_free_dma_mem(drv_handle, 
				   hostrsp->ptr,
				   hostrsp->phys, 
				   hostrsp->size, 
				   0);
		nx_os_zero_memory(hostrsp, 
				  sizeof(struct nx_dma_alloc_s));
	}

	NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_SUCCESS);
	return NX_RCODE_SUCCESS;
}


/*
 * This function will create and allocate request TX ctx. It:
 * - allocates DMAable address for requested and response tx contexes
 *   	by calling nx_os_alloc_dma_mem()
 * - updates request tx ctx
 * - write this DMA address and command to a set of CRBs via PCI access.
 * - poll rsp tx ctx for response from FW and take appropriate actions.
 */
nx_rcode_t 
nx_fw_cmd_create_tx_ctx(nx_host_tx_ctx_t *ptx_ctx,
			struct nx_dma_alloc_s *hostrq,
			struct nx_dma_alloc_s *hostrsp)
{
	nx_hostrq_tx_ctx_t 	*prq_tx_ctx;
	nx_hostrq_cds_ring_t 	*prq_cds_ring;
	nx_cardrsp_tx_ctx_t 	*prsp_tx_ctx;
	nx_host_cds_ring_t 	*pcds_rings;
	nx_dev_handle_t drv_handle;
	U64 tmp_phys_addr = 0;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	U32 temp = 0;
	struct unm_adapter_s *adapter;
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_create_tx_ctx;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));


	if (ptx_ctx == NULL ||
			hostrq == NULL ||
			hostrsp == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = ptx_ctx->nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *) drv_handle;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	prq_tx_ctx = (nx_hostrq_tx_ctx_t *)hostrq->ptr;
	prsp_tx_ctx = (nx_cardrsp_tx_ctx_t *)hostrsp->ptr;

	/* sanity check if context was allocated */
	if (NX_HOST_CTX_STATE_ALLOCATED != ptx_ctx->state) { 

		NX_DBGPRINTF(NX_DBG_ERROR,("%s:Invalid TX CTX state:%x\n", 
					__FUNCTION__, ptx_ctx->state));

		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_INVALID_ARGS);
		return (NX_RCODE_INVALID_ARGS);
	}

	prq_tx_ctx->host_rsp_dma_addr = 
		NX_OS_CPU_TO_LE64(nx_os_dma_addr_to_u64(hostrsp->phys));

	temp = (NX_CAP0_LEGACY_CONTEXT | 
			NX_CAP0_LEGACY_MN |
			NX_CAP0_LSO);

	prq_tx_ctx->capabilities[0] |= NX_OS_CPU_TO_LE32(temp);

	prq_tx_ctx->host_int_crb_mode = 
		NX_OS_CPU_TO_LE32(NX_HOST_INT_CRB_MODE_SHARED);

	prq_tx_ctx->interrupt_ctl = NX_OS_CPU_TO_LE32(ptx_ctx->interrupt_ctl);
	prq_tx_ctx->msi_index = NX_OS_CPU_TO_LE32(ptx_ctx->msi_index);

	prq_tx_ctx->dummy_dma_addr = 
		NX_OS_CPU_TO_LE64(nx_os_dma_addr_to_u64
				(ptx_ctx->dummy_dma_addr));
	prq_tx_ctx->cmd_cons_dma_addr = 
		NX_OS_CPU_TO_LE64(nx_os_dma_addr_to_u64
				(ptx_ctx->cmd_cons_dma_addr));

	prq_cds_ring = &prq_tx_ctx->cds_ring;
	pcds_rings = ptx_ctx->cds_ring;
	prq_cds_ring->host_phys_addr =
		NX_OS_CPU_TO_LE64(nx_os_dma_addr_to_u64(pcds_rings->host_phys));
	prq_cds_ring->ring_size = NX_OS_CPU_TO_LE32(pcds_rings->ring_size);

	tmp_phys_addr = nx_os_dma_addr_to_u64(hostrq->phys);

	rcode = issue_cmd(drv_handle,
			ptx_ctx->pci_func,
			NXHAL_VERSION,
			(U32)(tmp_phys_addr >> 32),
			((U32)tmp_phys_addr),
			hostrq->size,
			NX_CDRP_CMD_CREATE_TX_CTX,
			NULL,
			NULL,
			NULL);

	if (rcode != NX_RCODE_SUCCESS) {
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		goto failure;
	}

	ptx_ctx->cds_ring->host_tx_producer	= (nx_reg_addr_t)
		(NX_OS_LE32_TO_CPU(prsp_tx_ctx->cds_ring.host_producer_crb));
	ptx_ctx->state = NX_OS_LE32_TO_CPU(prsp_tx_ctx->host_ctx_state);	
	ptx_ctx->context_id = (U32)(NX_OS_LE16_TO_CPU(prsp_tx_ctx->context_id));
#ifdef NX_USE_NEW_ALLOC
	if (ptx_ctx->nx_dev->default_tx_ctx == NULL) {
		ptx_ctx->nx_dev->default_tx_ctx = ptx_ctx;
		NX_NIC_TRC_FN(adapter, cur_fn, ptx_ctx);
	}
	NX_NIC_TRC_FN(adapter, cur_fn, ptx_ctx->context_id);
#endif
failure:
	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);
}	/* end of create_tx_ctx */	


/*
 * This Function will destroy a given TX context.
 */ 
nx_rcode_t 
nx_fw_cmd_destroy_tx_ctx(nx_host_tx_ctx_t *ptx_ctx,
			 U32 destroy_cmd)
{
	nx_rcode_t 		rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t 	drv_handle;
	struct unm_adapter_s *adapter;	
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_destroy_tx_ctx;
	
	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));


	if (ptx_ctx == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}
	drv_handle = ptx_ctx->nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (ptx_ctx->state > NX_HOST_CTX_STATE_MAX) { 

		NX_DBGPRINTF(NX_DBG_ERROR,("%s:Invalid TX CTX state:%x\n", 
					__FUNCTION__, ptx_ctx->state));
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_INVALID_STATE);
		return (NX_RCODE_INVALID_STATE);
	}
	
	NX_NIC_TRC_FN(adapter, cur_fn, destroy_cmd);
	if(destroy_cmd != NX_DESTROY_CTX_NO_FWCMD) {
		rcode = issue_cmd(drv_handle,
				ptx_ctx->pci_func,
				NXHAL_VERSION,
				ptx_ctx->context_id,
				destroy_cmd,
				0,
				NX_CDRP_CMD_DESTROY_TX_CTX,
				NULL,
				NULL,
				NULL);
	}

#ifdef NX_USE_NEW_ALLOC
	if (ptx_ctx->nx_dev->default_tx_ctx == ptx_ctx) {
		/* TBD: This needs to find another active Tx */
		NX_NIC_TRC_FN(adapter, cur_fn, ptx_ctx);
		ptx_ctx->nx_dev->default_tx_ctx = NULL;
	}
#endif

	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);
} 	/* end of destroy_tx_ctx */ 


/*
 * This function will submit capability to FW.
 */
nx_rcode_t 
nx_fw_cmd_submit_capabilities(nx_host_nic_t* nx_dev, 
			      U32 pci_func, 
			      U32 *in, 
			      U32 *out)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  in[0],
			  in[1],
			  in[2],
			  NX_CDRP_CMD_SUBMIT_CAPABILITIES,
			  &out[0],
			  &out[1],
			  &out[2]);

	if (rcode != NX_RCODE_SUCCESS) {
		goto submit_fail;
	}

 submit_fail:
	return(rcode);

}	/* end of submit_capabilities */  

/* 
 * This function query for value of a phy register
 */
nx_rcode_t 
nx_fw_cmd_query_phy(nx_dev_handle_t drv_handle,
		    U32 pci_func,
		    U32 reg,
		    U32 *pval)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)drv_handle;	
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_query_phy;

	NX_NIC_TRC_FN(adapter, cur_fn, pci_func);
	NX_NIC_TRC_FN(adapter, cur_fn, reg);

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  reg,
			  0,
			  0,
			  NX_CDRP_CMD_READ_PHY,
			  pval,
			  NULL,
			  NULL);

	if (rcode != NX_RCODE_SUCCESS) {
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		return rcode;
	}

	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	NX_NIC_TRC_FN(adapter, cur_fn, *pval);
	return rcode;
}
nx_rcode_t 
nx_fw_cmd_set_phy(nx_dev_handle_t drv_handle,
		    U32 pci_func,
		    U32 reg,
		    U32 val)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  reg,
			  val,
			  0,
			  NX_CDRP_CMD_WRITE_PHY,
			  NULL,
			  NULL,
			  NULL);

	return rcode;
}

/* 
 * This function query for value of an hw register
 */
nx_rcode_t 
nx_fw_cmd_query_hw_reg(nx_dev_handle_t drv_handle,
		    U32 pci_func,
		    U32 reg,
		    U32 offset_unit,
		    U32 *pval)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  reg,
			  offset_unit,
			  0,
			  NX_CDRP_CMD_READ_HW_REG,
			  pval,
			  NULL,
			  NULL);

	if (rcode != NX_RCODE_SUCCESS) {
		return rcode;
	}

	return rcode;
}

/* 
 * This function query for flow control setting (pause frame)
 */
nx_rcode_t 
nx_fw_cmd_get_flow_ctl(nx_dev_handle_t drv_handle,
		   U32 pci_func,
		   U32 is_tx,
		   U32 *pval)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	U32 raw_rsp;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  is_tx,
			  0,
			  0,
			  NX_CDRP_CMD_GET_FLOW_CTL,
			  &raw_rsp,
			  NULL,
			  NULL);

	if (rcode != NX_RCODE_SUCCESS) {
		return rcode;
	}

	*pval = NX_OS_LE32_TO_CPU(raw_rsp);
	return rcode;
}

/* 
 * This function set flow control value (enable/disable pause frame)
 */
nx_rcode_t 
nx_fw_cmd_set_flow_ctl(nx_dev_handle_t drv_handle,
		   U32 pci_func,
		   U32 is_tx,
		   U32 val)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  is_tx,
			  val,
			  0,
			  NX_CDRP_CMD_SET_FLOW_CTL,
			  NULL,
			  NULL,
			  NULL);

	return rcode;
}

/* 
 * This function query for max RDS per RX context.
 */
nx_rcode_t 
nx_fw_cmd_query_max_rds_per_ctx(nx_host_nic_t* nx_dev, 
				U32 pci_func,
				U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;
	
	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  0,
			  0,
			  0,
			  NX_CDRP_CMD_READ_MAX_RDS_PER_CTX,
			  NULL,
			  NULL,
			  NULL);

	/* TBD -  update response data */

	return(rcode);

}	/* end of max rds */  


nx_rcode_t 
nx_fw_cmd_query_max_sds_per_ctx(nx_host_nic_t* nx_dev,
				U32 pci_func,
				U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;
	
	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  0,
			  0,
			  0,
			  NX_CDRP_CMD_READ_MAX_SDS_PER_CTX,
			  NULL,
			  NULL,
			  NULL);

	/* TBD -  update response data */
	
	return(rcode);

}	/* end of nx_fw_cmd_query_max_sds_per_ctx() */  


nx_rcode_t 
nx_fw_cmd_query_max_rules_per_ctx(nx_host_nic_t* nx_dev, 
				  U32 pci_func,
				  U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)drv_handle;	
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_query_max_rules_per_ctx;
	
	NX_NIC_TRC_FN(adapter, cur_fn, pci_func);

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  0,
			  0,
			  0,
			  NX_CDRP_CMD_READ_MAX_RULES_PER_CTX,
			  max,
			  NULL,
			  NULL);

	/* TBD -  update response data for rules */

	NX_NIC_TRC_FN(adapter, cur_fn, *max);
	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);

}	/* end of nx_fw_cmd_query_max_rules_per_ctx() */  


nx_rcode_t 
nx_fw_cmd_query_max_rx_ctx(nx_host_nic_t* nx_dev, 
			   U32 pci_func,
			   U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;
	
	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));
	
	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  0,
			  0,
			  0,
			  NX_CDRP_CMD_READ_MAX_RX_CTX,
			  max,
			  NULL,
			  NULL);

	/* TBD -  update response data for max ctx */

	return(rcode);
}	/* end of nx_fw_cmd_query_max_rx_ctx() */

nx_rcode_t 
nx_fw_cmd_query_max_tx_ctx(nx_host_nic_t* nx_dev, 
			   U32 pci_func,
			   U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  0,
			  0,
			  0,
			  NX_CDRP_CMD_READ_MAX_TX_CTX,
			  max,
			  NULL,
			  NULL);
	
	/* TBD -  update response data max tx ctx */

	return(rcode);
} 	/* end of nx_fw_cmd_query_max_tx_ctx() */

nx_rcode_t nx_fw_cmd_query_max_mtu(nx_host_nic_t *nx_dev, U32 pci_func,
				   U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle, pci_func, NXHAL_VERSION,
			  0, 0, 0, NX_CDRP_CMD_READ_MAX_MTU, max, NULL, NULL);

	return (rcode);
}

nx_rcode_t nx_fw_cmd_query_max_lro(nx_host_nic_t *nx_dev, U32 pci_func,
				   U32 *max)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;

	NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

	rcode = issue_cmd(drv_handle, pci_func, NXHAL_VERSION,
			  0, 0, 0, NX_CDRP_CMD_READ_MAX_LRO, max, NULL, NULL);

	return (rcode);
}

nx_rcode_t nx_fw_cmd_query_max_lro_per_board(nx_host_nic_t *nx_dev, U32 pci_func,
                                   U32 *max)
{
        nx_rcode_t      rcode = NX_RCODE_SUCCESS;
        nx_dev_handle_t drv_handle = nx_dev->nx_drv_handle;

        NX_DBGPRINTF(NX_DBG_INFO,("%s\n", __FUNCTION__));

        rcode = issue_cmd(drv_handle, pci_func, NXHAL_VERSION,
                          0, 0, 0, NX_CDRP_CMD_READ_MAX_LRO_PER_BOARD,
                          max, NULL, NULL);

        return (rcode);
}

nx_rcode_t
nx_fw_cmd_set_gbe_port(nx_dev_handle_t drv_handle,
		  U32 pci_func, U32 speed, U32 duplex, U32 autoneg)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  speed,
			  duplex,
			  autoneg,
			  NX_CDRP_CMD_CONFIG_GBE_PORT,
			  NULL,
			  NULL,
			  NULL);

	return(rcode);
}

nx_rcode_t 
nx_fw_cmd_set_mtu(nx_host_rx_ctx_t *prx_ctx,
		  U32 pci_func,
		  U32 mtu)
{
	nx_rcode_t 	rcode = NX_RCODE_SUCCESS;
	nx_dev_handle_t drv_handle;
	struct unm_adapter_s *adapter;	
	uint64_t cur_fn = (uint64_t) nx_fw_cmd_set_mtu;

	if (prx_ctx == NULL ||
	    prx_ctx->nx_dev == NULL) {
		return NX_RCODE_INVALID_ARGS;
	}

	drv_handle = prx_ctx->nx_dev->nx_drv_handle;
	adapter = (struct unm_adapter_s *)drv_handle;	
	NX_NIC_TRC_FN(adapter, cur_fn, pci_func);
	NX_NIC_TRC_FN(adapter, cur_fn, mtu);

	rcode = issue_cmd(drv_handle,
			  pci_func,
			  NXHAL_VERSION,
			  prx_ctx->context_id,
			  mtu,
			  0,
			  NX_CDRP_CMD_SET_MTU,
			  NULL,
			  NULL,
			  NULL);

	NX_NIC_TRC_FN(adapter, cur_fn, rcode);
	return(rcode);
}

nx_rcode_t
nx_fw_cmd_func_attrib(nx_host_nic_t *nx_dev, U32 pci_func,
    struct nx_cardrsp_func_attrib *attribs)
{
	nx_rcode_t		rcode;
	nx_dev_handle_t		drv_handle = nx_dev->nx_drv_handle;
	struct nx_dma_alloc_s	rarea;
	U64			taddr;

	rarea.size = sizeof(struct nx_cardrsp_func_attrib);

	/*
	 * Get temporary response DMA area.
	 */
	if (nx_os_alloc_dma_mem(drv_handle, &rarea.ptr, &rarea.phys,
	    rarea.size, 0) != NX_RCODE_SUCCESS)
		return NX_RCODE_NO_HOST_MEM;

	taddr = nx_os_dma_addr_to_u64(rarea.phys);

	rcode = issue_cmd(drv_handle, pci_func, NXHAL_VERSION,
	    (U32)(taddr >> 32), (U32)taddr, 0, NX_CDRP_CMD_FUNC_ATTRIB,
        NULL, NULL, NULL);

	*attribs = *(struct nx_cardrsp_func_attrib *)rarea.ptr;
	attribs->fenable = NX_OS_LE64_TO_CPU(attribs->fenable);
	attribs->freq = NX_OS_LE64_TO_CPU(attribs->freq);

	/*
	 * Free temporary response DMA area.
	 */
	nx_os_free_dma_mem(drv_handle, rarea.ptr, rarea.phys, rarea.size, 0);

	return rcode;
}

nx_rcode_t
unm_nic_get_minidump_template_size(struct unm_adapter_s *adapter)
{
    nx_rcode_t rcode = NX_RCODE_SUCCESS;
	uint64_t cur_fn = (uint64_t)unm_nic_get_minidump_template_size;

    rcode = issue_cmd((nx_dev_handle_t)adapter,
            adapter->ahw.pci_func,
            NXHAL_VERSION,
            0,
            0,
            0,
            NX_CDRP_CMD_MINIDUMP_TEMPLATE_SIZE,
            NULL,
            &(adapter->mdump.md_template_size),
            &(adapter->mdump.md_template_ver));

    if (rcode != NX_RCODE_SUCCESS) {
        adapter->mdump.md_template_size = 0;
        nx_nic_print3(NULL, "Failed to get minidump template size, err_code : %d\n", rcode);
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
    }

    return rcode;
}

nx_rcode_t
unm_nic_get_minidump_template(struct unm_adapter_s *adapter)
{
    nx_rcode_t rcode = NX_RCODE_SUCCESS;
    dma_addr_t md_template_addr;
    void *addr;
	u32 template_size = 0;
    u32 arg3 = 0;
    u32 size, offset = 0;
	uint64_t cur_fn = (uint64_t)unm_nic_get_minidump_template;

    size = adapter->mdump.md_template_size;
	if(size == 0) {
        nx_nic_print3(NULL, "Can not capture minidump template: Invalid size.\n");
		return NX_RCODE_INVALID_ARGS;
	}

    addr = pci_alloc_consistent(adapter->pdev,
            size, &md_template_addr);
    if (!addr) {
        nx_nic_print3(NULL, "Unable to allocate dmable memory for template.\n");
		NX_NIC_TRC_FN(adapter, cur_fn, -ENOMEM);
        return -ENOMEM;
    }

    memset(addr, 0, size);

    arg3 |= (size & 0xffff);
    arg3 |= ((offset & 0xffff) << 16); 

    rcode = issue_cmd((nx_dev_handle_t)adapter,
            adapter->ahw.pci_func,
            NXHAL_VERSION,
            (u32)(md_template_addr & 0xffffffff),
            (u32)(md_template_addr >> 32),
            arg3,
            NX_CDRP_CMD_MINIDUMP_GET_TEMPLATE, 
            NULL,
            &template_size,
            NULL);

    if ((rcode == NX_RCODE_SUCCESS) && (size == template_size)) {
        memcpy(adapter->mdump.md_template, addr, size);
    } else {
        nx_nic_print3(NULL, "Failed to get minidump template, err_code : %d, " 
				"requested_size : %d, actual_size : %d\n", rcode, size, template_size);
		NX_NIC_TRC_FN(adapter, cur_fn, rcode);
		NX_NIC_TRC_FN(adapter, cur_fn, size);
		NX_NIC_TRC_FN(adapter, cur_fn, template_size);
    }

    pci_free_consistent(adapter->pdev, size, addr, md_template_addr);

    return rcode;
}

#ifdef NX_USE_NEW_ALLOC

/* ptx_ctx: CMD ring on which the command will be sent
   prx_ctx: Rx ring on which a response (if requested) will arrive
   opcode : The requested action
   rqbody : body to send with command
   size   : size of the body in bytes
   is_sync: is synchronous and will wait for response
   rspword: if is_sync, the response word value 
*/
nx_rcode_t 
nx_os_send_nic_request(nx_host_tx_ctx_t *ptx_ctx,
                       nx_host_rx_ctx_t *prx_ctx,
                       U32 opcode,
                       U64 *rqbody,
                       U32 size,
                       U32 is_sync,
                       U64 *rspword)
{
	nic_request_t req;
	int rv = NX_RCODE_SUCCESS;
	nx_os_wait_event_t wait;
	U64 dummy_rspword;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)ptx_ctx->nx_dev->nx_drv_handle;	
	uint64_t cur_fn = (uint64_t) nx_os_send_nic_request;

	NX_NIC_TRC_FN(adapter, cur_fn, opcode);
	NX_NIC_TRC_FN(adapter, cur_fn, is_sync);

	if (opcode == NX_NIC_H2C_OPCODE_CONFIG_L2_MAC) {
		/* This is a special case to maintain compatibility
		   to old interfaces */
		req.opcode = NIC_REQUEST;
		req.body.cmn.req_hdr.opcode = UNM_MAC_EVENT;
	} else {
		req.opcode = NX_NIC_HOST_REQUEST;
		req.body.cmn.req_hdr.opcode = opcode;
	}
	req.body.cmn.req_hdr.ctxid = prx_ctx->context_id;
	req.body.cmn.req_hdr.need_completion = is_sync;

	if (rspword == NULL) {
		rspword = &dummy_rspword;
	}

	if (size > 0) {
		nx_os_copy_memory(&req.body.cmn.words[0], &rqbody[0], size);
	}

	if (is_sync) {
		if (nx_os_event_wait_setup(ptx_ctx->nx_dev->nx_drv_handle,
					&req, rspword, &wait)
				!= NX_RCODE_SUCCESS) {
			NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_CMD_FAILED);
			return NX_RCODE_CMD_FAILED;
		}
	}

	if (nx_os_send_cmd_descs(ptx_ctx, &req, 1)) {
		NX_NIC_TRC_FN(adapter, cur_fn, NX_RCODE_CMD_FAILED);
		return NX_RCODE_CMD_FAILED;
	}

	if (is_sync) {
		nx_os_event_wait(ptx_ctx->nx_dev->nx_drv_handle, &wait, 
				(NX_OS_CRB_RETRY_COUNT * 
				 NX_OS_CRB_UDELAY_VALUE));
	}
	NX_NIC_TRC_FN(adapter, cur_fn, rv);
	return (rv);
}
#endif /* NX_USE_NEW_ALLOC */

static nx_rcode_t
validate_dev_for_cmd(nx_host_nic_t *nx_dev) 
{
	if (nx_dev == NULL ||
	    nx_dev->active_rx_ctxs <= 0 || 
	    nx_dev->active_tx_ctxs <= 0 ||
	    nx_dev->default_rx_ctx == NULL ||
	    nx_dev->default_tx_ctx == NULL) {
		return NX_RCODE_CMD_FAILED;
	}
	return NX_RCODE_SUCCESS;
}

#ifdef NX_USE_NEW_ALLOC
nx_rcode_t
nx_os_pf_add_l2_mac(nx_host_nic_t *nx_dev, nx_host_rx_ctx_t *rx_ctx,
		    char *mac)
{
	nx_rcode_t rv;
	mac_request_t mac_req;
	struct unm_adapter_s *adapter;	
	uint64_t cur_fn = (uint64_t) nx_os_pf_add_l2_mac;

	rv = validate_dev_for_cmd(nx_dev);

	if(rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	adapter = (struct unm_adapter_s *)nx_dev->nx_drv_handle;	
	NX_NIC_TRC_FN(adapter, cur_fn, rv);

	nx_os_zero_memory(&mac_req, sizeof(mac_request_t));
	
	mac_req.op = UNM_MAC_ADD;
	nx_os_copy_memory(mac_req.mac_addr, mac, 6);

	NX_NIC_TRC_FN(adapter, cur_fn, UNM_MAC_ADD);

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    rx_ctx, 
				    NX_NIC_H2C_OPCODE_CONFIG_L2_MAC,
				    (U64 *)(&mac_req),
				    sizeof(mac_request_t),
				    0, NULL);
	
	NX_NIC_TRC_FN(adapter, cur_fn, rv);
	return rv;
}

nx_rcode_t
nx_os_pf_remove_l2_mac(nx_host_nic_t *nx_dev, nx_host_rx_ctx_t *rx_ctx, 
		       char* mac)
{
	nx_rcode_t rv;
	mac_request_t mac_req;
	struct unm_adapter_s *adapter;	
	uint64_t cur_fn = (uint64_t) nx_os_pf_remove_l2_mac;

	rv = validate_dev_for_cmd(nx_dev);

	if(rv != NX_RCODE_SUCCESS) {
		return rv;
	}
	adapter = (struct unm_adapter_s *)nx_dev->nx_drv_handle;	
	NX_NIC_TRC_FN(adapter, cur_fn, rv);

	nx_os_zero_memory(&mac_req, sizeof(mac_request_t));
	
	mac_req.op = UNM_MAC_DEL;
	nx_os_copy_memory(mac_req.mac_addr, mac, 6);

	NX_NIC_TRC_FN(adapter, cur_fn, UNM_MAC_DEL);
	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    rx_ctx, 
				    NX_NIC_H2C_OPCODE_CONFIG_L2_MAC,
				    (U64 *)(&mac_req),
				    sizeof(mac_request_t),
				    0, NULL);
	
	NX_NIC_TRC_FN(adapter, cur_fn, rv);
	return rv;
}
#endif

#if 0
nx_rcode_t
nx_os_pf_set_mac_addr(nx_host_nic_t *nx_dev,
		      U8 *addr,
		      U8 is_sync)
{
	nx_rcode_t rv;
	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_SET_MAC,
				    addr, 6,
				    is_sync, NULL);
	
	return rv;
}

nx_rcode_t 
nx_os_pf_get_mac_addr(nx_host_nic_t *nx_dev,
		      __uint8_t *addr)
{
	nx_rcode_t rv;
	U64 rsp_word = 0;

	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_GET_MAC,
				    NULL, 0,
				    1, &rsp_word);
	if ((rsp_word & 0xff) == 0) {
		
	}

	return rv;
}

nx_rcode_t 
nx_os_pf_set_mtu(nx_host_nic_t *nx_dev,
		 U32 mtu,
		 U8 is_sync)
{
	nx_rcode_t rv;
	U64 rsp_word;

	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_SET_MTU,
				    &mtu, 4,
				    is_sync, &rsp_word);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	return NX_RCODE_SUCCESS;
}

nx_rcode_t
nx_os_pf_set_promiscuous(nx_host_nic_t *nx_dev,
			 U8 is_sync)
{
	nx_rcode_t rv;
	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_SET_PROMISCUOUS,
				    NULL, 0,
				    is_sync, NULL);
	return rv;
}

nx_rcode_t
nx_os_pf_clr_promiscuous(nx_host_nic_t *nx_dev,
			 U8 is_sync)
{
	nx_rcode_t rv;
	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_CLR_PROMISCUOUS,
				    NULL, 0,
				    is_sync, NULL);
	return rv;
}

nx_rcode_t
nx_os_pf_enable_port(nx_host_nic_t *nx_dev,
		     U8 is_sync)
{
	nx_rcode_t rv;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)nx_dev->nx_drv_handle;	

	rv = validate_dev_for_cmd(nx_dev);

	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_ENABLE_PORT,
				    NULL, 0,
				    is_sync, NULL);
	return rv;
}

nx_rcode_t
nx_os_pf_disable_port(nx_host_nic_t *nx_dev,
		      U8 is_sync)
{
	nx_rcode_t rv;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)nx_dev->nx_drv_handle;	
	rv = validate_dev_for_cmd(nx_dev);
	if (rv != NX_RCODE_SUCCESS) {
		return rv;
	}

	rv = nx_os_send_nic_request(nx_dev->default_tx_ctx,
				    nx_dev->rx_ctx_list,
				    NX_NIC_H2C_OPCODE_CONFIG_DISABLE_PORT,
				    NULL, 0,
				    is_sync, NULL);
	return rv;
}

#endif



