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
#ifndef _NXHAL_NIC_API_H_
#define _NXHAL_NIC_API_H_

typedef U64						nx_host_desc_handle_t;
typedef U64						nx_host_ring_handle_t;
typedef U32						nx_host_ref_handle_t;


#define NX_HOST_MAC_ADDR_SIZE   6


/*
 * NIC device structure
 */
typedef struct nx_host_nic_s	nx_host_nic_t;

/*
 * Rx and Tx context structures
 */
typedef struct nx_host_rx_ctx_s	nx_host_rx_ctx_t;
typedef struct nx_host_tx_ctx_s	nx_host_tx_ctx_t;

/*
 * Descriptor ring context structures
 */
typedef struct nx_host_sds_ring_s	nx_host_sds_ring_t;
typedef struct nx_host_rds_ring_s	nx_host_rds_ring_t;
typedef struct nx_host_cds_ring_s	nx_host_cds_ring_t;

typedef struct nx_host_pci_dev_s
{
	U16                 vend_id;
	U16                 dev_id;
	U16                 subvend_id;
	U16                 subdev_id;
}nx_host_pci_dev_t;

typedef enum nx_host_pci_res_type_s
{
	nx_host_pci_res_bar0 = 0,
	nx_host_pci_res_bar1,
	nx_host_pci_res_bar2,
	nx_host_pci_res_bar3,
	nx_host_pci_res_max
}nx_host_pci_res_type_t;

typedef struct nx_host_dev_capabilities_s
{
	U8                  rss;
	U8                  lso;
	U8                  ip_cksum;
	U8                  tcp_cksum;
	U32                 max_rds;
	U32                 max_sds;
	U32                 max_cds;
	U32                 max_rules;
}nx_host_dev_capabilities_t;

typedef struct nx_host_pci_res_list_s
{
	nx_host_pci_res_type_t    nx_host_pci_res_type;
	void*                     nx_host_pci_bar_mapped_addr;
	U64                       nx_host_pci_bar_addr;
	U8                        nx_host_io_mem;
}nx_host_pci_res_list_t;

typedef struct nx_host_dev_config_s
{
	U32                      nx_host_rss_enable;
	U32                      nx_host_toe_enable;
	U32                      nx_host_ipcksum_enable;
	U32                      nx_host_tcpcksum_enable;
	U8                       nx_host_mac_addr[NX_HOST_MAC_ADDR_SIZE];
}nx_host_dev_config_t;

typedef enum nx_host_rds_ring_type_s
{
	nx_host_rds_ring_type_standard = 0,
	nx_host_rds_ring_type_jumbo,
	nx_host_rds_ring_type_lro,
	nx_host_rds_ring_type_max
}nx_host_rds_ring_type_t;

typedef struct nx_host_nic_rx_config_s
{
	U32                         n_rds_rings;
	U32                         n_sds_rings;
	U32                         n_rules;
}nx_host_nic_rx_config_t;

typedef struct nx_host_nic_tx_config_s
{
	U32                         n_cds_rings;
}nx_host_nic_tx_config_t;


/*
 * Interface functions
 */
extern nx_host_dev_capabilities_t* nx_host_get_dev_capabilities(void);
extern nx_host_pci_dev_t* nx_host_get_supported_devs(U32* pncount);
/*
This routine should compute the total device memory (nondma) that will be needed
by HAL it will be
sizeof (nx_host_nic_t) + n_tx_ctx*sizeof(nx_host_tx_ctx_t) + 
n_rx_ctx*sizeof(nx_host_rx_ctx_t) +
n_tx_ctx*n_cds_rings*sizeof(nx_host_cds_ring_t) +
n_rx_ctx*n_rds_rings*sizeof(nx_host_rds_ring_t) +
n_rx_ctx*n_sds_rings*sizeof(nx_host_sds_ring_t) +
n_rx_ctx*n_rules*sizeof(nx_rx_rule_t)
*/

U32 nx_host_get_dev_mem(U16 pci_fn, U32 n_rx_ctx, U32 n_tx_ctx, 
						nx_host_nic_rx_config_t* prx_cfg, 
						nx_host_nic_tx_config_t* ptx_cfg);



/*
This function will allocate memory needed for nx_host_nic_t. It will call the
driver at nxos_alloc_mem to get the needed memory either from driver's pre-
allocated pool or from OS directly. It will also initialize the nx_host_nic_t
structure with the user passed configuration. It will store the mapped PCI
resources passed in this call.
*/
nx_host_nic_t*
nx_host_create_dev(nx_host_pci_res_list_t* pci_res_list, U32 n_pci_res, 
				   void* drvhandle, nx_host_dev_config_t* pdevcfg);


nx_host_nic_t*
nx_host_alloc_dev(nx_dev_handle_t drv_handle);

void
nx_host_free_dev(nx_host_nic_t*	nx_dev);


/*
This function will allocate memory for rx context container structure. It will
also allocate memory for n_rds_rings number of rds context structures and 
n_sds_rings number of sds context structures.
*/

U32 nx_host_create_rx_ctx(nx_host_nic_t* pnic, nx_host_nic_rx_config_t* prxcfg);

/*
This function will allocate memory for tx context container structure. It will
also allocate memory for n_cds_rings number of cds context structures.
*/
U32 nx_host_create_tx_ctx(nx_host_nic_t* pnic, nx_host_nic_tx_config_t* ptxcfg);


/*
HAL will compute the DMA memory needed for the rds ring using the n_elements and
rx descriptor size. It will allocate DMA memory by calling back into the driver 
at nxos_alloc_dma_mem. Once the ring is created it will return handle to this 
ring to the caller. Driver can make multiple calls into the API to allocate 
multiple rds rings.
(NOTE: Size of the buffer posted on a per descriptor can not exceed the 
buff_size set for the rds ring)
*/
nx_host_ring_handle_t nx_host_rx_create_rdsring(nx_host_rx_ctx_t* prx_ctx, 
												U16 n_elements, U16 buff_size,
												nx_rcode_t* rcode);


/*
HAL will compute the DMA memory needed for the sds ring using the n_elements and
status descriptor size. It will allocate DMA memory by calling back into the 
driver at nxos_alloc_dma_mem. Once the ring is created it will return handle to 
this ring to the caller. Driver can make multiple calls into the API to allocate
multiple sds rings.
*/
nx_host_ring_handle_t nx_host_rx_create_sdsring(nx_host_rx_ctx_t* rx_ctx, 
												U16 n_elements, 
												nx_rcode_t* rcode);



/*
HAL will compute the DMA memory needed for the cds ring using the n_elements and
command descriptor size. It will allocate DMA memory by calling back into the 
driver at nxos_alloc_dma_mem. Once the ring is created it will return handle to 
this ring to the caller. Driver can make multiple calls into the API to allocate
multiple cds rings.
NOTE: Only one cds ring will be allocated per tx context. In order to have 
multiple cds rings, driver will have to allocate those many tx contexts.
*/
void nx_host_tx_create_cdsring(nx_host_tx_ctx_t* tx_ctx, U16 n_elements,
							   nx_rcode_t* rcode);



nx_host_desc_handle_t nx_host_get_rx_desc(nx_host_rx_ctx_t* rx_ctx, 
										  nx_host_ring_handle_t rhandle);


void nx_host_set_rx_buffer_desc(nx_host_rx_ctx_t* rx_ctx, 
								nx_host_ring_handle_t rhandle, 
								nx_host_desc_handle_t dhandle, 
								nx_dma_addr_t pa, nx_host_ref_handle_t ref);


void nx_host_put_rx_desc(nx_host_rx_ctx_t* rx_ctx, 
						 nx_host_ring_handle_t rhandle);


nx_host_desc_handle_t nx_host_get_tx_desc(nx_host_tx_ctx_t* tx_ctx, U32 nfrags,
										  U32 hdrsize);

void nx_host_set_tx_buffer_desc(nx_host_tx_ctx_t* tx_ctx, 
								nx_host_desc_handle_t dhandle, U32 sg_idx, 
								nx_dma_addr_t pa, U32 len, nx_host_ref_handle_t 
								ref, U32 last);

void nx_host_put_tx_desc(nx_host_tx_ctx_t* tx_ctx);

void nx_host_set_tx_desc_lsoinfo(nx_host_tx_ctx_t* tx_ctx, nx_host_desc_handle_t
								 dhandle, U8 hdrlen, U8 iphdroffset, 
								 U8 tcphdroffset, U16 mss);

void nx_host_set_tx_desc_vlaninfo(nx_host_tx_ctx_t* tx_ctx, 
								  nx_host_desc_handle_t dhandle, U8 hdrlen, 
								  U8 flags, U16 tag);

void nx_host_set_tx_desc_cksuminfo(nx_host_tx_ctx_t* tx_ctx, 
								   nx_host_desc_handle_t dhandle, U8 hdrlen, 
								   U8 iphdroffset, U8 tcphdroffset);

/*
Completion path APIs
*/
U32 nx_host_get_tx_pkt_completion_count(nx_host_tx_ctx_t* tx_ctx);

nx_host_desc_handle_t nx_host_get_status_desc(nx_host_rx_ctx_t* rx_ctx, 
											  nx_host_ring_handle_t shandle);
void nx_host_process_status_desc(nx_host_rx_ctx_t* rx_ctx, 
								 nx_host_ring_handle_t shandle, 
								 nx_host_desc_handle_t dhandle);

U32 nx_host_process_sds_ring(nx_host_rx_ctx_t* rx_ctx, 
							 nx_host_ring_handle_t shandle, U32 n_completions);

#endif /*_NXHAL_NIC_API_H_*/
