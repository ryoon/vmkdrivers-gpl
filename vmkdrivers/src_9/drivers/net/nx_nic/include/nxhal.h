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
/*
Data structures private to hwlib
*/
#ifndef _NXHAL_H_
#define _NXHAL_H_

#include "nxhal_cmn.h"

#define NXHAL_VERSION 	1

/**********************************************************
 *                    Config Params
 **********************************************************/

#define NX_HOST_MAX_RDS_RING_SIZE	1024
#define NX_HOST_MAX_SDS_RING_SIZE	1024
#define NX_HOST_MAX_CDS_RING_SIZE	1024

/*************************************************
 *                    RX ctx                        
 *************************************************/

struct nx_host_sds_ring_s {
	I8 *host_addr;		/* Base addrs of sds ring */
	nx_dma_addr_t host_phys;
	nx_reg_addr_t host_sds_consumer;	/* CRB */
	nx_reg_addr_t interrupt_crb;	/* CRB */
	U32 consumer_index;
	U32 ring_size;
	U16 msi_index;
	nx_host_rx_ctx_t *parent_ctx;
	void *os_data;
};

struct nx_host_rds_ring_s {
	I8 *host_addr;		/* Base addrs of rds ring */
	nx_dma_addr_t host_phys;
	I8 *card_rx_consumer;
	nx_reg_addr_t host_rx_producer;	/* CRB */
	U32 producer_index;
	U64 buff_size;
	U32 ring_size;
	U32 ring_kind;		/* ring type */
	nx_host_rx_ctx_t *parent_ctx;
	void *os_data;
};

struct nx_host_rx_ctx_s {
	nx_host_nic_t *nx_dev;
	U32 alloc_len;
	U32 this_id;

	/* rds, sds, rules are arrays allocated at ctx creation time */
	nx_host_rds_ring_t *rds_rings;	/* Alloc max rds per ctx */
	nx_host_sds_ring_t *sds_rings;	/* Alloc max sds per ctx */
	nx_rx_rule_t *rules;	/* Alloc max rules per ctx */
	U16 num_rds_rings;
	U16 num_sds_rings;
	U16 num_rules;
	U16 active_rx_rules;
	nx_host_ctx_state_t state;
	U32 context_id;
	/* Allow aggregation of multiple buffers for larger packets */
	BOOLEAN chaining_allowed;
	/* Rx contexts created in multi-context mode (pNIC/steering) */
	BOOLEAN multi_context;
	/* Copied from nic for convenience */
	U16 port;
	U16 pci_func;
	U32 num_fn_per_port;	/* How many PCI fn share the port */
	U32 consumer[2];	/* ?? where DMAs will go */
	void *os_data;
};

/**********************************************
 *                    TX ctx    
 **********************************************/

struct nx_host_cds_ring_s {
	I8 *host_addr;		/* Base addrs of cds ring */
	nx_dma_addr_t host_phys;
	I8 *card_tx_consumer;
	nx_reg_addr_t host_tx_producer;	/* CRB */
	U32 producer_index;
	U32 ring_size;
	U16 int_enable;
	nx_host_rx_ctx_t *parent_ctx;
	void *os_data;
};

struct nx_host_tx_ctx_s {
	nx_host_nic_t *nx_dev;
	U32 alloc_len;
	U32 this_id;

	nx_host_cds_ring_t *cds_ring;
	nx_host_ctx_state_t state;
	nx_dma_addr_t dummy_dma_addr;
	nx_dma_addr_t cmd_cons_dma_addr;
	U32 context_id;
	U16 port;
	U16 pci_func;
	U16 interrupt_ctl;
	U16 msi_index;
        nx_hostrq_pexq_t pexq_req; 
        nx_cardrsp_pexq_t pexq_rsp;
        U8  use_pexq;
	void *os_data;
};

#define NX_SERIAL_NUM_BYTES	32
#define SERIAL_NUM_ADDRESS	0x3e881c

/* Board types */
#define  NX_NIC_GBE		0x01
#define  NX_NIC_XGBE		0x02

#define NX_INTERNAL_REV_1	0x01
#define NX_INTERNAL_REV_2	0x02

#define ETH_LENGTH_OF_ADDRESS	6
#define ETHERNET_CRC_SIZE	4

#if 0

/*
 * Debug tags
 *
 */
#define UNM_NIC_TAG		((U32)'UNM')

typedef enum fw_download_type {
	fw_source_flash = 1,
	fw_source_driver,
	fw_source_manual
} fw_download_source;

#endif /* if 0 */

typedef enum nx_link_state_s {
	nx_link_down = 0,
	nx_link_up
} nx_link_state_t;

struct nx_host_nic_s {
	/* Driver's handle for this device */
	nx_dev_handle_t nx_drv_handle;
	nx_host_dev_capabilities_t nx_capabilities;

	/* From FW controlled capability settings */
	U32 n_rx_rds_rings;
	U32 n_rx_sds_rings;
	U32 n_rx_rules;
	U32 n_rx_ctxs;
	U32 n_tx_ctxs;

	/* Array of all rx contexts */
	nx_host_rx_ctx_t **rx_ctxs;
	/* Array of all tx contexts */
	nx_host_tx_ctx_t **tx_ctxs;
	/* Default for cmd/rsp */
	nx_host_rx_ctx_t *default_rx_ctx;
	nx_host_tx_ctx_t *default_tx_ctx;
	nx_rx_rule_t *saved_rules;
	U32 alloc_len;
	U16 alloc_rx_ctxs;
	U16 alloc_tx_ctxs;
	U16 active_rx_ctxs;
	U16 active_tx_ctxs;

	/* PCI resources */
	U16 pci_func;
	void *pci_base;
	U32 pci_len;
	U32 rx_ctxs_state;
	U32 vendor_id;
	U32 device_id;
	U32 int_rev_id;

	void *drbell_base;
	U32 drbell_len;
	BOOLEAN p32MB;
	BOOLEAN flag_dev_ready;
	nx_link_state_t link_state;

	U32 max_frame_sz;
#if 0
	fw_download_source fw_download_control;
#endif
	U32 lro_enable;
	U32 enable_flow_control;
#if 0
	unm_board_info_t boardcfg;
	U32 board_type;
	U8 serial_num[NX_SERIAL_NUM_BYTES];
	U8 cur_mac_addr[ETH_LENGTH_OF_ADDRESS];
	U8 perm_mac_addr[ETH_LENGTH_OF_ADDRESS];
	U32 gboardid;
	BOOLEAN flag_mcast_set;
	BOOLEAN flag_override_addr;
	U32 mcast_addr_count;
	U32 uni_addr_count;
	U32 mma_count;

	/* REVIEW: MANDAR */
	I32 led_test_ret;
	I32 led_test_last;
	U32 led_state;
#endif
};

#if 0
typedef struct _boardlist {
	U8 brdsrnum[NX_SERIAL_NUM_BYTES];
	U32 num_drivers;
	U8 ghalt;
	U8 updateRxBuf_DB;
} BoardListInfo;

#define NX_BOARDS_PER_SYSTEM 8	// So we can handle upto 2 Quad gig correctly !

BoardListInfo gNetXenBrdList[NX_BOARDS_PER_SYSTEM];
#endif /* if 0 */

#define get_next_index(index,length)  ((((index)  + 1) == length)?0:(index) +1)

/* 
 * Function prototypes
 */

#if 0
U32 unm_driver_read(nx_host_nic_t * nx_dev, U64 offset);
I32 unm_nic_get_flash_size(void);
I32 dma_watchdog_shutdown_request(nx_host_nic_t * nx_dev);
I32 dma_watchdog_shutdown_poll_result(nx_host_nic_t * nx_dev);
I32 dma_watchdog_wakeup(nx_host_nic_t * nx_dev);
I32 dma_watchdog_wakeup_poll_result(nx_host_nic_t * nx_dev);
I32 WriteMemThroughAgent(nx_host_nic_t * nx_dev, U64 addr, U64 data);
I32 ReadMemThroughAgent(nx_host_nic_t * nx_dev, U64 addr, U64 * data);
void EnableBusMaster(nx_host_nic_t * nx_dev);
I32 unm_nic_get_board_info(nx_host_nic_t * nx_dev);
U64 unm_nic_pci_set_window(nx_host_nic_t * nx_dev, U64 addr);
void unm_nic_pci_mem_write8(nx_host_nic_t * nx_dev, U64 Offset, void *Data,
			    U32 Length);
void unm_nic_pci_mem_write(nx_host_nic_t * nx_dev, U64 Offset, U32 Data,
			   U32 Length);
U32 unm_nic_pci_set_crbwindow(nx_host_nic_t * nx_dev, U64 off);
U32 unm_set_direct_crbwindow(nx_host_nic_t * nx_dev, U64 off);
U32 normalize_index(nx_host_nic_t * nx_dev, U32 off);
void unm_nic_write_crb(nx_host_nic_t * nx_dev, U32 Index, U32 Data);
void unm_nic_read_crb(nx_host_nic_t * nx_dev, U32 Index, U32 * Data);
void unm_nic_hw_write8(nx_host_nic_t * nx_dev, U64 Offset, void *Data,
		       U32 Length);
void unm_nic_hw_write(nx_host_nic_t * nx_dev, U64 Offset, U32 Data, U32 Length);
void unm_nic_hw_read(nx_host_nic_t * nx_dev, U64 off, void *data, I32 len);
void unm_nic_imb_write(nx_host_nic_t * nx_dev, U64 Offset, void *Data,
		       U32 Length);
void unm_nic_imb_read(nx_host_nic_t * nx_dev, U64 off, void *data, I32 len);
void unm_nic_init_gbe(nx_host_nic_t * nx_dev);
void unm_nic_clear_printf_buffer(nx_host_nic_t * nx_dev);
void unm_nic_init_printf_buffer(nx_host_nic_t * nx_dev);
void unm_load_cam_ram(nx_host_nic_t * nx_dev);
void unm_tcl_phaninit(nx_host_nic_t * nx_dev);
void unm_nic_init_niu(nx_host_nic_t * nx_dev);
void PrePostImageProcessing(nx_host_nic_t * nx_dev);
void PostImageProcessing(nx_host_nic_t * nx_dev);
void unm_nic_set_pauseparam(nx_host_nic_t * nx_dev);
I32 unm_niu_gbe_set_rx_flow_ctl(nx_host_nic_t * nx_dev, I32 enable);
I32 unm_niu_gbe_set_tx_flow_ctl(nx_host_nic_t * nx_dev, I32 enable);
I32 unm_niu_xg_set_tx_flow_ctl(nx_host_nic_t * nx_dev, I32 enable);
void resetall(nx_host_nic_t * nx_dev);
void caspreset(nx_host_nic_t * nx_dev);
void peg_clr_all(nx_host_nic_t * nx_dev);
void new_pinit(nx_host_nic_t * nx_dev);
void clear_printf_regs(nx_host_nic_t * nx_dev);
void init_printf_regs(nx_host_nic_t * nx_dev);
I32 load_crbinit_from_rom(nx_host_nic_t * nx_dev, I32 verbose);
I32 load_peg_image(nx_host_nic_t * nx_dev);
void reset_hw(nx_host_nic_t * nx_dev);
I32 initialize_adapter_offload(nx_host_nic_t * nx_dev);
I32 phantom_init(nx_host_nic_t * nx_dev);
I32 check_hw_init(nx_host_nic_t * nx_dev);
I32 phantom_init_and_load(nx_host_nic_t * nx_dev);
void unm_enable_lro(nx_host_nic_t * nx_dev);
void unm_nic_pci_mem_read(nx_host_nic_t * nx_dev, U64 off, void *data, I32 len);

I32 tap_crb_mbist_clear(nx_host_nic_t * nx_dev);
#endif /* if 0 */

/*****************************************************************************
 *              HAL-FW function prototype
 *****************************************************************************/

/*
 *  QUERIES
 */
nx_rcode_t nx_fw_cmd_submit_capabilities(nx_host_nic_t * nx_dev,
					 U32 pci_func, U32 * in, U32 * out);
nx_rcode_t nx_fw_cmd_get_toe_license(nx_host_nic_t* nx_dev, 
					 U32 pci_func, U32 *out);
nx_rcode_t nx_fw_cmd_query_max_rds_per_ctx(nx_host_nic_t * nx_dev,
					   U32 pci_func, U32 * max);
nx_rcode_t nx_fw_cmd_query_max_sds_per_ctx(nx_host_nic_t * nx_dev,
					   U32 pci_func, U32 * max);
nx_rcode_t nx_fw_cmd_query_max_rules_per_ctx(nx_host_nic_t * nx_dev,
					     U32 pci_func, U32 * max);
nx_rcode_t nx_fw_cmd_query_max_rx_ctx(nx_host_nic_t * nx_dev,
				      U32 pci_func, U32 * max);
nx_rcode_t nx_fw_cmd_query_max_tx_ctx(nx_host_nic_t * nx_dev,
				      U32 pci_func, U32 * max);
nx_rcode_t nx_fw_cmd_query_max_mtu(nx_host_nic_t *nx_dev, U32 pci_func,
				   U32 *max);
nx_rcode_t nx_fw_cmd_query_max_lro(nx_host_nic_t *nx_dev, U32 pci_func,
				   U32 *max);
nx_rcode_t nx_fw_cmd_query_max_lro_per_board(nx_host_nic_t *nx_dev, U32 pci_func,
                                   U32 *max);
nx_rcode_t nx_fw_cmd_query_phy(nx_dev_handle_t drv_handle, U32 pci_func,
		    	       U32 reg, U32 *pval);
nx_rcode_t nx_fw_cmd_query_hw_reg(nx_dev_handle_t drv_handle, U32 pci_func,
				  U32 reg, U32 offset_unit, U32 *pval);
nx_rcode_t nx_fw_cmd_get_flow_ctl(nx_dev_handle_t drv_handle, U32 pci_func,
		   		  U32 direction, U32 *pval);
nx_rcode_t nx_fw_cmd_set_gbe_port(nx_dev_handle_t drv_handle,
		  U32 pci_func, U32 speed, U32 duplex, U32 autoneg);

/*
 * DEV
 */

nx_rcode_t
nx_os_dev_alloc(nx_host_nic_t ** ppnx_dev,
		nx_dev_handle_t drv_handle,
		U32 pci_func, U32 max_rx_ctxs, U32 max_tx_ctxs);

nx_rcode_t nx_os_dev_free(nx_host_nic_t * nx_dev);

nx_rcode_t 
nx_fw_cmd_set_phy(nx_dev_handle_t drv_handle,
		    U32 pci_func,
		    U32 reg,
		    U32 val);


nx_rcode_t 
nx_fw_cmd_set_flow_ctl(nx_dev_handle_t drv_handle,
		   U32 pci_func,
		   U32 direction,
		   U32 val);

nx_rcode_t
nx_fw_cmd_configure_toe(nx_host_rx_ctx_t *prx_ctx,
		        U32 pci_func, 
                        U32 op_code);

nx_rcode_t
nx_fw_cmd_func_attrib(nx_host_nic_t *nx_dev, U32 pci_func,
    struct nx_cardrsp_func_attrib *attribs);

nx_rcode_t 
nx_fw_cmd_query_pexq(nx_host_nic_t* nx_dev, 
		        U32 pci_func, 
                        U32 *pexq_length,
                        U32 *pexq_fc_array_size);

/*
 * RX
 */

nx_rcode_t
nx_fw_cmd_create_rx_ctx_alloc(nx_host_nic_t * nx_dev,
			      U32 num_rds_rings,
			      U32 num_sds_rings, U32 num_rules,
			      nx_host_rx_ctx_t ** prx_ctx);
nx_rcode_t
nx_fw_cmd_create_rx_ctx_free(nx_host_nic_t * nx_dev,
			     nx_host_rx_ctx_t * prx_ctx);

nx_rcode_t
nx_fw_cmd_create_rx_ctx_alloc_dma(nx_host_nic_t * nx_dev,
				  U32 num_rds_rings,
				  U32 num_sds_rings,
				  struct nx_dma_alloc_s *hostrq,
				  struct nx_dma_alloc_s *hostrsp);

nx_rcode_t
nx_fw_cmd_create_rx_ctx_free_dma(nx_host_nic_t * nx_dev,
				 struct nx_dma_alloc_s *hostrq,
				 struct nx_dma_alloc_s *hostrsp);

nx_rcode_t
nx_fw_cmd_create_rx_ctx(nx_host_rx_ctx_t * rx_ctx,
			struct nx_dma_alloc_s *hostrq,
			struct nx_dma_alloc_s *hostrsp);

nx_rcode_t 
nx_fw_cmd_destroy_rx_ctx(nx_host_rx_ctx_t * rx_ctx, U32 destroy_cmd);


nx_rcode_t 
nx_fw_cmd_set_mtu(nx_host_rx_ctx_t *prx_ctx,
		  U32 pci_func,
		  U32 mtu);

/*
 * TX
 */
nx_rcode_t
nx_fw_cmd_create_tx_ctx_alloc(nx_host_nic_t * nx_dev,
			      U32 num_cds_rings, nx_host_tx_ctx_t ** ptx_ctx);

nx_rcode_t
nx_fw_cmd_create_tx_ctx_free(nx_host_nic_t * nx_dev,
			     nx_host_tx_ctx_t * ptx_ctx);

nx_rcode_t
nx_fw_cmd_create_tx_ctx_alloc_dma(nx_host_nic_t * nx_dev,
				  U32 num_cds_rings,
				  struct nx_dma_alloc_s *hostrq,
				  struct nx_dma_alloc_s *hostrsp);
nx_rcode_t
nx_fw_cmd_create_tx_ctx_free_dma(nx_host_nic_t * nx_dev,
				 struct nx_dma_alloc_s *hostrq,
				 struct nx_dma_alloc_s *hostrsp);

nx_rcode_t
nx_fw_cmd_create_tx_ctx(nx_host_tx_ctx_t * tx_ctx,
			struct nx_dma_alloc_s *hostrq,
			struct nx_dma_alloc_s *hostrsp);
nx_rcode_t nx_fw_cmd_destroy_tx_ctx(nx_host_tx_ctx_t * tx_ctx, U32 destroy_cmd);

#ifdef NX_USE_NEW_ALLOC
extern nx_rcode_t
nx_os_event_wait_setup(nx_dev_handle_t drv_handle,
		       nic_request_t * req,
		       U64 * rsp_word, nx_os_wait_event_t *wait);

nx_rcode_t
nx_os_event_wait(nx_dev_handle_t drv_handle,
		 nx_os_wait_event_t * wait, I32 utimelimit);

nx_rcode_t
nx_os_event_wakeup_on_response(nx_dev_handle_t drv_handle,
			       nic_response_t * rsp);

extern U32
nx_os_send_cmd_descs(nx_host_tx_ctx_t * ptx_ctx,
		     nic_request_t * req, U32 nr_elements);

extern nx_rcode_t
nx_os_send_nic_request(nx_host_tx_ctx_t * ptx_ctx,
		       nx_host_rx_ctx_t * prx_ctx,
		       U32 opcode,
		       U64 * rqbody, U32 size, U32 is_sync, U64 * rspword);

extern void
nx_os_handle_nic_response(nx_dev_handle_t drv_handle, nic_response_t * rsp);


nx_rcode_t
nx_os_pf_add_l2_mac(nx_host_nic_t *nx_dev, nx_host_rx_ctx_t *rx_ctx,
		                char *mac);
nx_rcode_t
nx_os_pf_remove_l2_mac(nx_host_nic_t *nx_dev, nx_host_rx_ctx_t *rx_ctx, 
		char* mac);


int
api_lock(nx_dev_handle_t drv_handle);
int
api_unlock(nx_dev_handle_t drv_handle);

extern unsigned long long  ctx_addr_sig_regs[][3];

#define CRB_CTX_ADDR_REG_LO(FUNC_ID)         (ctx_addr_sig_regs[FUNC_ID][0])
#define CRB_CTX_ADDR_REG_HI(FUNC_ID)         (ctx_addr_sig_regs[FUNC_ID][2])
#define CRB_CTX_SIGNATURE_REG(FUNC_ID)       (ctx_addr_sig_regs[FUNC_ID][1])

#endif /* NX_USE_NEW_ALLOC */

#endif /*_NXHAL_H_*/
