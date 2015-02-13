/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Written by : David Chaw <david_chaw@adaptec.com>
 * Modified by : Naveen Chandrasekaran <naveen_chandrasekaran@adaptec.com>
 * 
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/razor/linux/src/adp94xx_hwi.c#161 $
 * 
 */	

#include "adp94xx_osm.h"
#include "adp94xx_inline.h"
#if KDB_ENABLE
#include "linux/kdb.h"
#endif

/* Local functions' prototypes */
static u_int	asd_sglist_allocsize(struct asd_softc *asd);
static int	asd_hwi_init_scbdata(struct asd_softc *asd);
static int	asd_hwi_reset_hw(struct asd_softc *asd);
static void	asd_hwi_exit_hw(struct asd_softc *asd);
static void	asd_hwi_setup_sw_bar(struct asd_softc *asd);
static int	asd_hwi_init_phys(struct asd_softc *asd);
static int	asd_hwi_init_ports(struct asd_softc *asd); 
static void	asd_hwi_alloc_scbs(struct asd_softc *asd);
static int	asd_hwi_init_sequencers(struct asd_softc *asd);
static void	asd_hwi_process_dl(struct asd_softc *asd);
static void	asd_hwi_build_id_frame(struct asd_phy *phy);
static void	asd_hwi_build_smp_phy_req(struct asd_port *port, int req_type, 
					  int phy_id, int ctx);

/* SCB completion post routines. */
static void	asd_hwi_process_phy_comp(struct asd_softc *asd, 
					 struct scb *scb,
					 struct control_phy_sb *cntrl_phy);
static void	asd_hwi_process_edb(struct asd_softc *asd, 
				    struct asd_done_list *dl_entry);
static void	asd_hwi_process_prim_event(struct asd_softc *asd,
					   struct asd_phy *phy,
					   u_int reg_addr, u_int reg_content);
static void	asd_hwi_process_phy_event(struct asd_softc *asd, 
					  struct asd_phy *phy, u_int oob_status,
					  u_int oob_mode);
static void asd_hwi_process_timer_event(struct asd_softc *asd,
					  struct asd_phy *phy, uint8_t	error);
static void	asd_hwi_handle_link_rst_err(struct asd_softc *asd,
					    struct asd_phy *phy);
static void	asd_hwi_process_req_task(struct asd_softc *asd,
					 uint8_t req_type, uint16_t index);

/* Error Handling routines. */
static void	asd_scb_eh_timeout(u_long arg);
static void	asd_hwi_abort_scb(struct asd_softc *asd, 
				  struct scb *scb_to_abort, struct scb *scb);
static void	asd_hwi_reset_lu(struct asd_softc *asd, 
				 struct scb *scb_to_reset, struct scb *scb);
static void	asd_hwi_reset_device(struct asd_softc *asd, 
				     struct scb *scb_to_reset, struct scb *scb);
static void	asd_hwi_resume_sendq(struct asd_softc *asd, 
				     struct scb *scb_to_reset, struct scb *scb);
static void asd_hwi_resume_sendq_done(struct asd_softc *asd, struct scb *scb,
			  struct asd_done_list *dl);
static void	asd_hwi_reset_end_device(struct asd_softc *asd,
					 struct scb *scb);
static void	asd_hwi_reset_exp_device(struct asd_softc *asd,
					 struct scb *scb);
static void	asd_hwi_report_phy_err_log(struct asd_softc *asd,
					   struct scb *scb);
static void	asd_hwi_dump_phy_err_log(struct asd_port *port,
					 struct scb *scb);
static void	asd_hwi_reset_port(struct asd_softc *asd, 
				   struct scb *scb_to_reset, struct scb *scb);

static asd_scb_post_t	asd_hwi_abort_scb_done;
static asd_scb_post_t	asd_hwi_reset_lu_done;
static asd_scb_post_t	asd_hwi_reset_device_done;
static asd_scb_post_t	asd_hwi_reset_end_device_done;
static asd_scb_post_t	asd_hwi_reset_exp_device_done;
static asd_scb_post_t	asd_hwi_reset_port_done;
static asd_scb_eh_post_t asd_hwi_req_task_done;

/* Function prototypes for NVRAM access utilites. */
#if NVRAM_SUPPORT

static int	asd_hwi_poll_nvram(struct asd_softc *asd);
static int	asd_hwi_chk_write_status(struct asd_softc *asd, 
					uint32_t sector_addr, 
					uint8_t erase_flag); 
static int	asd_hwi_reset_nvram(struct asd_softc *asd);
int	asd_hwi_search_nv_cookie(struct asd_softc *asd, 
			uint32_t *addr,
			struct asd_flash_dir_layout *pflash_dir_buf);

static int	asd_hwi_erase_nv_sector(struct asd_softc *asd, 
					uint32_t  sector_addr);
static int 	asd_hwi_verify_nv_checksum(struct asd_softc *asd, 
					   u_int segment_id,
					   uint8_t *segment_ptr, 
					   u_int bytes_to_read);
static int	asd_hwi_get_nv_config(struct asd_softc *asd);

static int	asd_hwi_search_nv_id(struct asd_softc *asd, 
				     u_int setting_id, 
				     void *dest, u_int *src_offset, 
				     u_int bytes_to_read);
static int 	asd_hwi_get_nv_phy_settings(struct asd_softc *asd);
static int 	asd_hwi_map_lrate_from_sas(u_int sas_link_rate, 
					   u_int *asd_link_rate);
static int 	asd_hwi_get_nv_manuf_seg(struct asd_softc *asd, void *dest, 
					 uint32_t bytes_to_read,
					 uint32_t *src_offset,
					 uint16_t signature); 
static void	asd_hwi_get_nv_phy_params(struct asd_softc *asd);

static int asd_hwi_check_flash(struct asd_softc *asd); 

#endif /* NVRAM_SUPPORT */

/* OCM access routines */
static int 	asd_hwi_get_ocm_info(struct asd_softc *asd);
static int 	asd_hwi_get_ocm_entry(struct asd_softc *asd, 
				      uint32_t entry_type,
				      struct asd_ocm_entry_format *pocm_de, 
				      uint32_t *src_offset);
static int 	asd_hwi_read_ocm_seg(struct asd_softc *asd, void *dest,
				     uint32_t src_offset, u_int bytes_to_read,
				     u_int *bytes_read);
static int 	asd_hwi_set_speed_mask(u_int asd_link_rate, 
				       uint8_t *asd_speed_mask);

#if SAS_COMSTOCK_SUPPORT
static void	asd_hwi_get_rtl_ver(struct asd_softc *asd);
#endif

#ifdef ASD_TEST
static void 	asd_hwi_dump_phy(struct asd_phy *phy);
static void 	asd_hwi_dump_phy_id_addr(struct asd_phy *phy);
#endif

/*
 * Function:
 *	asd_sglist_allocsize
 *
 * Description:
 * 	Calculate the optimum S/G List allocation size.  S/G elements used
 *	for a given transaction must be physically contiguous.  Assume the
 *	OS will allocate full pages to us, so it doesn't make sense to request
 *	less than a page.
 */
static u_int
asd_sglist_allocsize(struct asd_softc *asd)
{
	uint32_t sg_list_increment;
	uint32_t sg_list_size;
	uint32_t max_list_size;
	uint32_t best_list_size;

	/* Start out with the minimum required for ASD_NSEG. */
	sg_list_increment = asd_sglist_size(asd);
	sg_list_size = sg_list_increment;

	/* Get us as close as possible to a page in size. */
	while ((sg_list_size + sg_list_increment) <= PAGE_SIZE)
		sg_list_size += sg_list_increment;

	/*
	 * Try to reduce the amount of wastage by allocating
	 * multiple pages.
	 */
	best_list_size = sg_list_size;
	max_list_size = roundup(sg_list_increment, PAGE_SIZE);
	if (max_list_size < 4 * PAGE_SIZE)
		max_list_size = 4 * PAGE_SIZE;
	if (max_list_size > (ASD_MAX_ALLOCATED_SCBS * sg_list_increment))
		max_list_size = (ASD_MAX_ALLOCATED_SCBS * sg_list_increment);
	while ((sg_list_size + sg_list_increment) <= max_list_size
	   &&  (sg_list_size % PAGE_SIZE) != 0) {
		uint32_t new_mod;
		uint32_t best_mod;

		sg_list_size += sg_list_increment;
		new_mod = sg_list_size % PAGE_SIZE;
		best_mod = best_list_size % PAGE_SIZE;
		if (new_mod > best_mod || new_mod == 0) {
			best_list_size = sg_list_size;
		}
	}
	return (best_list_size);
}

static int
asd_hwi_init_scbdata(struct asd_softc *asd)
{
	struct scb	*scb;
	u_int		 scb_cnt;
	int		 loop_cnt;
	asd->scbindex = asd_alloc_mem(
				asd->hw_profile.max_scbs * sizeof(struct scb *),
				GFP_ATOMIC);
	if (asd->scbindex == NULL)
		return (-ENOMEM);
	
	memset(asd->scbindex, 0x0, 
	       asd->hw_profile.max_scbs * sizeof(struct scb *));
	asd->init_level++;

	asd->qinfifo = asd_alloc_mem(
			   asd->hw_profile.max_scbs * sizeof(*asd->qinfifo),
			   GFP_ATOMIC);

	if (asd->qinfifo == NULL) {
		asd_free_mem(asd->scbindex);
		return (-ENOMEM);
	}

	asd->init_level++;
	/*
	 * Create our DMA tags.  These tags define the kinds of device
	 * accessible memory allocations and memory mappings we will
	 * need to perform during normal operation.
	 */
	/* DMA tag for our hardware scb structures */
	if (asd_dma_tag_create(asd, 1, PAGE_SIZE, GFP_ATOMIC, 
			       &asd->hscb_dmat) != 0) {
		asd_hwi_exit_hw(asd);
		goto error_exit;
	}
	asd->init_level++;

	/* DMA tag for our S/G structures. */
	if (asd_dma_tag_create(asd, 8, asd_sglist_allocsize(asd),
			       GFP_ATOMIC, &asd->sg_dmat) != 0) {
		asd_hwi_exit_hw(asd);
		goto error_exit;
	}
	asd->init_level++;

	/* Perform initial SCB allocation */
	asd_hwi_alloc_scbs(asd);

	if (asd->numscbs == 0) {
		asd_print("%s: Unable to allocate initial scbs\n", 
			  asd_name(asd));
		asd_hwi_exit_hw(asd);
		goto error_exit;
	}
	/* 
	 * Make sure we are able to allocate more than the reserved 
	 * SCB requirements.
	 */
	loop_cnt = 0;
	while (asd->numscbs < ASD_RSVD_SCBS) {
		/* 
		 * Allocate SCB until we have more than the reserved SCBs
		 * requirement.
		 */
		asd_hwi_alloc_scbs(asd);
		if (++loop_cnt > 4)
			break;
	}

	if (asd->numscbs < ASD_RSVD_SCBS) {
		asd_log(ASD_DBG_ERROR, "Failed to allocate reserved pool of "
			"SCBs.\n");
		asd_hwi_exit_hw(asd);
		goto error_exit;
	}

	scb_cnt = 0;
	/* Save certain amount of SCBs as reserved. */
	while (!list_empty(&asd->free_scbs)) {
		scb = list_entry(asd->free_scbs.next, struct scb, hwi_links);
		list_del(&scb->hwi_links);
		list_add_tail(&scb->hwi_links, &asd->rsvd_scbs);
		if (++scb_cnt > ASD_RSVD_SCBS)
			break;
	}
#ifdef EXTENDED_SCB
	{
/* allocate memory for extended scb*/
/* reserved 128 byte more for 128 bytes alignment*/
		if (asd_alloc_dma_mem(asd, SCB_SIZE * (ASD_EXTENDED_SCB_NUMBER+1),
			(void **)&asd->ext_scb_map.vaddr,
			&asd->ext_scb_map.busaddr,
			&asd->ext_scb_dmat,
			&asd->ext_scb_map) != 0) {
			asd_hwi_exit_hw(asd);
			goto error_exit;
		}
#ifdef ASD_DEBUG
		asd_print("EXTENDED_SCB is allocated\n");
#endif

	}
#endif
	return (0); 

error_exit:
	return (-ENOMEM);
}

/* 
 * Function:
 *	asd_alloc_softc()
 *
 * Description:
 *	Allocate a softc structure and setup necessary fields.
 */
struct asd_softc *
asd_alloc_softc(asd_dev_t dev)
{
	struct asd_softc	*asd;
	
	asd = asd_alloc_mem(sizeof(*asd), GFP_KERNEL);
	if (asd == NULL) {
		asd_log(ASD_DBG_ERROR, "Unable to alloc softc.\n");
		return (NULL);
	}
	
	memset(asd, 0x0, sizeof(*asd));
	INIT_LIST_HEAD(&asd->rsvd_scbs);
	INIT_LIST_HEAD(&asd->free_scbs);
	INIT_LIST_HEAD(&asd->pending_scbs);
	INIT_LIST_HEAD(&asd->timedout_scbs);
	INIT_LIST_HEAD(&asd->empty_scbs);
	INIT_LIST_HEAD(&asd->hscb_maps);
	INIT_LIST_HEAD(&asd->sg_maps);
	asd->dev = dev;

	if (asd_platform_alloc(asd) != 0) {
		asd_free_softc(asd);
		asd = NULL;
	}
	return (asd);
}

/* 
 * Function:
 *	asd_free_softc()
 *
 * Description:
 *	Free the host structure and any memory allocated for its member fields.
 *	Also perform cleanup for module unloading purpose.
 */
void
asd_free_softc(struct asd_softc *asd)
{
	asd_platform_free(asd);

	/* Free any internal data structures */
	asd_hwi_exit_hw(asd);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) && \
    LINUX_VERSION_CODE  < KERNEL_VERSION(2,5,0)
	if (asd->dev != NULL)
		asd->dev->driver = NULL;
#endif

	asd_free_mem(asd);			
}

void
asd_intr_enable(struct asd_softc *asd, int enable)
{
	asd_write_dword(asd, CHIMINTEN, enable ? SET_CHIMINTEN : 0);
}

static int
asd_hwi_reset_hw(struct asd_softc *asd)
{
	u_int i;

#define ASD_RESET_DELAY	10
#define ASD_RESET_LOOP_COUNT (1 * 1000000 / ASD_RESET_DELAY)
	for (i = 0 ; i < 4 ; i++) {
		asd_write_dword(asd, COMBIST, HARDRST);
	}
	for (i = 0; i < ASD_RESET_LOOP_COUNT; i++) {
		asd_delay(ASD_RESET_DELAY);
		if (asd_read_dword(asd, CHIMINT) & HARDRSTDET)
			break;
	}
	if (i >= ASD_RESET_LOOP_COUNT) {
		asd_log(ASD_DBG_ERROR, "Chip reset failed.\n");
		return (-EIO);
	}

	return (0);
} 

#ifdef ASD_DEBUG
typedef struct pcic_regs {
	uint8_t		name[32];
	uint16_t	offset;
} pcic_regs_t;

static pcic_regs_t	PCICREG[] =
{
	{"DEVICE-VENDOR"     ,0x00},
	{"COMMAND-STATUS"    ,0x04},
	{"DEVREV-CLASS"      ,0x08},
	{"SUB_VENDOR-ID"     ,0x2c},
	{"XBAR"	             ,0x30},
	{"PCIX_CAP-CMD"      ,0x42},
	{"PCIX_STATUS"       ,0x44},
	{"MBAR1"             ,0x6c},
	{"MBAR0-WA"          ,0x70},
	{"MBAR0-WB"          ,0x74},
	{"MBAR0-WC"          ,0x78},
	{"MBARKEY"           ,0x7c},
	{"HREX_CTL"          ,0x80},
	{"HREX_STATUS"       ,0x84},
	{"RBI_CTL"           ,0x88},
	{"RBI_STATUS"        ,0x8c},
	{"CF_ADR_L"          ,0x90},
	{"CF_ADR_H"          ,0x94},
	{"DF_ADR_L"          ,0x98},
	{"DF_ADR_H"          ,0x9c},
	{"HSTPCIX_CTL"       ,0xa0},
	{"HSTPCIX_STATUS"    ,0xa4},
	{"FLSH_BAR"          ,0xb8},
	{"", 0 }	/* Last entry should be NULL. */
}; 

void DumpPCI(struct asd_softc *asd)
{
	uint32_t regvalue;
	uint32_t i;
	i=0;
	asd_print("**PCI REG DUMP********************\n");
	while(PCICREG[i].name[0]!=0)
	{
		regvalue = asd_pcic_read_dword(asd, PCICREG[i].offset);
		asd_print(" %s (0x%x) : 0x%x\n", PCICREG[i].name, PCICREG[i].offset, regvalue);
		i++;
	}
	asd_print("**********************************\n");
}
#endif

/* 
 * Function:
 *	asd_hwi_init_hw()
 *
 * Description:
 *      Perform controller specific initialization.
 */
int
asd_hwi_init_hw(struct asd_softc  *asd)
{
	union hardware_scb 	*hscb;
	struct scb		*scb;
	uint8_t			*next_vaddr;
	dma_addr_t	 	 next_baddr;
	dma_addr_t	 	 edb_baddr;
	size_t		 	 driver_data_size;
	size_t		 	 dl_size;
	uint8_t		 	 enabled_phys;
	u_int		 	 num_edbs;
	u_int		 	 num_escbs;
	u_int		 	 i;
	int		 	 error;
	u_long			 flags;

	/* Setup the Sliding Window BAR. */
	asd_hwi_setup_sw_bar(asd);
#if SAS_COMSTOCK_SUPPORT
	/* 
	 * Retrieve the RTL number. The RTL Number will be used to decide which
	 * sequencer version to use.
	 */  
	asd_hwi_get_rtl_ver(asd);

	/* No support for COMSTOCK RTL less than version 14. */
	if (asd->hw_profile.rev_id < COMSTOCK_LATEST_RTL)
		return (-ENODEV);
#endif

	/* Allocate SCB data */
	if (asd_hwi_init_scbdata(asd) != 0)
		return (-ENOMEM);
	
	/*
	 * DMA tag for our done_list, empty buffers, empty hardware SCBs,
	 * and sentinel hardware SCB.  These are data host memory structures
	 * the controller must be able to access.
	 *
	 * The number of elements in our done list must be a powerof2
	 * greater than or equal to 4 that is large enough to guarantee
	 * it cannot overflow.  Since each done list entry is associated
	 * with either an empty data buffer or an SCB, add the counts for
	 * these two objects together and roundup to the next power of 2.
	 * To ensure our sequencers don't stall, we need two empty buffers
	 * per sequencer (1 sequencer per-phy plus central seq).  We round
	 * this up to a multiple of the number of EDBs that we can fit in
	 * a single empty SCB.
	 */
	num_edbs = roundup(2 * (asd->hw_profile.max_phys + 1),
			   ASD_MAX_EDBS_PER_SCB);
	/*
	 * At minimum, allocate 2 empty SCBs so that the sequencers
	 * always have empty buffers while we are trying to queue more.
	 */
	num_edbs = MAX(num_edbs, 2 * ASD_MAX_EDBS_PER_SCB);
	num_escbs = num_edbs / ASD_MAX_EDBS_PER_SCB;
	dl_size = asd->hw_profile.max_scbs + num_edbs;
	dl_size = roundup_pow2(dl_size);
	asd->dl_wrap_mask = dl_size - 1;
	dl_size *= sizeof(*asd->done_list);

	driver_data_size = dl_size + (ASD_SCB_SIZE) /* for sentinel */ + 
			 + (num_escbs * ASD_SCB_SIZE)
			 + (num_edbs * sizeof(union edb));

	if (asd_dma_tag_create(asd, 8, driver_data_size, GFP_ATOMIC,
			       &asd->shared_data_dmat) != 0) {
		asd_hwi_exit_hw(asd);
		return (-ENOMEM);
	}

	asd->init_level++;

	/* Allocation of driver data */
	if (asd_dmamem_alloc(asd, asd->shared_data_dmat,
			     (void **)&asd->shared_data_map.vaddr, GFP_ATOMIC,
			     &asd->shared_data_map.dmamap,
			     &asd->shared_data_map.busaddr) != 0) {
		asd_hwi_exit_hw(asd);
		return (-ENOMEM);
	}

	asd->init_level++;

	/*
	 * Distribute the memory.
	 */
	memset(asd->shared_data_map.vaddr, 0, driver_data_size);
	asd->done_list = (struct asd_done_list *)asd->shared_data_map.vaddr;
	asd->dl_valid = ASD_QDONE_PASS_DEF;
	next_vaddr = asd->shared_data_map.vaddr + dl_size;
	next_baddr = asd->shared_data_map.busaddr + dl_size;

	/*
	 * We need one SCB to serve as the "next SCB".  Since the
	 * tag identifier in this SCB will never be used, there is
	 * no point in using a valid HSCB tag from an SCB pulled from
	 * the standard free pool.  So, we allocate this "sentinel"
	 * specially from the DMA safe memory chunk.
	 */
	asd->next_queued_hscb = (union hardware_scb *)next_vaddr;
	asd->next_queued_hscb_map = &asd->shared_data_map;
	asd->next_queued_hscb_busaddr = asd_htole64(next_baddr);
	next_vaddr += ASD_SCB_SIZE;
	next_baddr += ASD_SCB_SIZE;

	/*
	 * Since Empty SCBs do not require scatter gather lists
	 * we also allocate them outside of asd_alloc_scbs().
	 */
	hscb = asd->next_queued_hscb + 1;
	edb_baddr = next_baddr + (num_escbs * ASD_SCB_SIZE);
	for (i = 0; i < num_escbs; i++, hscb++, next_baddr += ASD_SCB_SIZE) {
		int j;

		/*
		 * Allocate ESCBs.
		 */
		scb = asd_alloc_mem(sizeof(*scb), GFP_ATOMIC);
		if (scb == NULL) {
			error = -ENOMEM;
			goto exit;
		}
		memset(scb, 0, sizeof(*scb));
		INIT_LIST_HEAD(&scb->hwi_links);
		INIT_LIST_HEAD(&scb->owner_links);
		scb->hscb = hscb;
		scb->hscb_busaddr = asd_htole64(next_baddr);
		scb->softc = asd;
		scb->hscb_map = &asd->shared_data_map;
		hscb->header.index = asd_htole16(asd->numscbs);
		hscb->header.opcode = SCB_EMPTY_BUFFER;
		asd->scbindex[asd->numscbs++] = scb;
		hscb->empty_scb.num_valid_elems = ASD_MAX_EDBS_PER_SCB;
		for (j = 0; j < ASD_MAX_EDBS_PER_SCB; j++) {
			hscb->empty_scb.buf_elem[j].busaddr =
			    asd_htole64(edb_baddr);
			hscb->empty_scb.buf_elem[j].buffer_size =
			    asd_htole32(sizeof(union edb));
			hscb->empty_scb.buf_elem[j].elem_valid_ds =
			    ELEM_BUFFER_VALID;
			edb_baddr += sizeof(union edb);
		}
		list_add(&scb->hwi_links, &asd->empty_scbs);
	}

	asd->init_level++;

	/* Allocate Free DDB bitmap. */
	asd->ddb_bitmap_size = roundup(asd->hw_profile.max_ddbs, BITS_PER_LONG);
	asd->ddb_bitmap_size /= BITS_PER_LONG;
	asd->free_ddb_bitmap = asd_alloc_mem((asd->ddb_bitmap_size *
					      sizeof(u_long)), GFP_ATOMIC);
	if (asd->free_ddb_bitmap == NULL)
		return (-ENOMEM);
	memset(asd->free_ddb_bitmap, 0, sizeof(u_long) * asd->ddb_bitmap_size);

	/*
	 * DDB site 0 and 1 are reserved for the firmware for internal use.
	 */
	asd->free_ddb_bitmap[0] |= (u_long)(3UL<<0);
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd->free_ddb_bitmap ptr 0x%x\n",asd->free_ddb_bitmap);
	asd_print("%llx %llx\n",(u64)asd->free_ddb_bitmap[0], (u64)asd->free_ddb_bitmap[1]);
#endif
	asd->init_level++;

#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "After initial BAR Setting\n");
	DumpPCI(asd);
#endif
	/* Retrieve OCM information */
	if (asd_hwi_get_ocm_info(asd)) {
		asd_log(ASD_DBG_ERROR, "Failed to retrieve OCM info.\n");
		/* TBD: return -1; ? */
	}
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "After asd_hwi_get_ocm_info\n");
	DumpPCI(asd);
#endif
#if NVRAM_SUPPORT
	/* 
	 * Retrieves controller NVRAM setting. 
	 */
	error = asd_hwi_get_nv_config(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to retrieve NVRAM config.\n");
	}
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "After asd_hwi_get_nv_config\n");
	DumpPCI(asd);
#endif
#endif

	/* Initialize the phy and port to default settings. */
	error = asd_hwi_init_phys(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to init the phys.\n");
		asd_hwi_exit_hw(asd);
		goto exit;
	}

	error = asd_hwi_init_ports(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to init the ports.\n");
		asd_hwi_exit_hw(asd);
		goto exit;
	}

	if (asd_hwi_reset_hw(asd) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to perform Chip reset.\n");
		asd_hwi_exit_hw(asd);
		goto exit;
	}

	asd_write_dword(asd, CHIMINT, PORRSTDET|HARDRSTDET);

	/*
	 * Reset the producer and consumer index to reflect
	 * no outstanding SCBs.
	 */
	asd->qinfifonext = (asd_read_dword(asd, SCBPRO) & SCBCONS_MASK) >> 16;
	asd_write_dword(asd, SCBPRO, asd->qinfifonext);

	asd->qinfifonext = asd_read_word(asd, SCBPRO+2);

	/* Disable the Host interrupts. */
	asd_write_dword(asd, CHIMINTEN, RST_CHIMINTEN);

	/* Initialize and setup the CSEQ and LSEQ. */
	error = asd_hwi_init_sequencers(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to init the SEQ.\n"); 
		goto exit;
	}		
	
	/* CSEQ should be ready to run. Start the CSEQ. */
	error = asd_hwi_start_cseq(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to start the CSEQ.\n"); 
		goto exit;
	}		

	/* Start the LSEQ(s). */
	i = 0;
	enabled_phys = asd->hw_profile.enabled_phys;
	while (enabled_phys != 0) {
		for ( ; i < asd->hw_profile.max_phys; i++) {
			if (enabled_phys & (1U << i)) {
				enabled_phys &= ~(1U << i);
				break;
			}
		}
		
		error = asd_hwi_start_lseq(asd, i);
		if (error != 0) {
			asd_log(ASD_DBG_ERROR,
				"Failed to start LSEQ %d.\n", i);
			goto exit;
		}		
	}

	asd_lock(asd, &flags);

	/*
	 * Post all of our empty scbs to the central sequencer.
	 */
	list_for_each_entry(scb, &asd->empty_scbs, hwi_links) {
		asd_hwi_post_scb(asd, scb);
	}

	asd_unlock(asd, &flags);

	/* Enabled all the phys. */
	i = 0;
	enabled_phys = asd->hw_profile.enabled_phys;
	while (enabled_phys != 0) { 
		for ( ; i < asd->hw_profile.max_phys; i++) {
			if (enabled_phys & (1 << i)) {
				enabled_phys &= ~(1 << i);
				break;
			} 
		}
	
		error = asd_hwi_enable_phy(asd, asd->phy_list[i]);
		if (error != 0) {
			/*
			 * TODO: This shouldn't happen.
			 *       Need more thought on how to proceed.
			 */
			asd_log(ASD_DBG_ERROR, "Failed to enable phy %d.\n", i);
			break;
		}
	}
exit:
#ifdef ASD_DEBUG
	asd->debug_flag=0;
#endif
	return (error);         
}

#if SAS_COMSTOCK_SUPPORT
/*
 * Function:
 *	asd_hwi_get_rtl_ver()
 *
 * Description:
 * 	Retrive the COMSTOCK rtl version.
 */ 	  	 
static void
asd_hwi_get_rtl_ver(struct asd_softc *asd)
{
	uint32_t	exsi_base_addr;
	uint32_t	reg_addr;
	uint8_t		reg_data;

	exsi_base_addr = EXSI_REG_BASE_ADR + XREGADDR;
	reg_addr = asd_hwi_swb_read_dword(asd, exsi_base_addr);
	asd_hwi_swb_write_dword(asd, exsi_base_addr, (uint32_t)
				(reg_addr & ~(XRADDRINCEN | XREGADD_MASK)));
	reg_data = asd_hwi_swb_read_byte(asd, EXSI_REG_BASE_ADR + XREGDATAR);
	asd_hwi_swb_write_dword(asd, exsi_base_addr, reg_addr);
	
	asd->hw_profile.rev_id = reg_data;
}
#endif /* SAS_COMSTOCK_SUPPORT */

/* 
 * Function:
 *	asd_hwi_exit_hw()
 *
 * Description:
 *      Perform controller specific cleanup.
 */
static void
asd_hwi_exit_hw(struct asd_softc *asd)
{
	struct scb	*scb;
	struct map_node	*map;
	struct asd_phy	*phy;
	struct asd_port	*port;
	u_int		 i;

	/*
	 * Reset the chip so that the sequencers do not
	 * attempt to DMA data into buffers we are about
	 * to remove or issue further interrupts.
	 */
	/* TBRV: This seems to fail all the time. */
	//asd_hwi_reset_hw(asd);

	/* Clean up the phy structures */
	for (i = 0; i <	asd->hw_profile.max_phys; i++) {
		if (asd->phy_list[i] != NULL) {
			phy = asd->phy_list[i];
			/* Free the ID ADDR Frame buffer. */
			asd_free_dma_mem(asd, phy->id_addr_dmat,
					 &phy->id_addr_map);
			asd_free_mem(asd->phy_list[i]);
		}
	}

	/* Clean up the port structures */
	for (i = 0; i < asd->hw_profile.max_ports; i++) {
		if (asd->port_list[i] != NULL) {
			port = asd->port_list[i];

			/*
			 * Free SMP Request Frame buffer.
			 */
			asd_free_dma_mem(asd, 
				port->dc.smp_req_dmat,
				&port->dc.smp_req_map);

			/*
			 * Free SMP Response Frame buffer.
			 */
			asd_free_dma_mem(asd, 
				port->dc.smp_resp_dmat,
				&port->dc.smp_resp_map);

			asd_free_mem(asd->port_list[i]);
		}
	}

	/* Free up the SCBs */
	while (!list_empty(&asd->pending_scbs)) {
		scb = list_entry(asd->pending_scbs.next, struct scb, hwi_links);
asd_log(ASD_DBG_INFO, "freeing pending scb 0x%x scb->hwi_links 0x%p",scb, &scb->hwi_links);
		list_del(&scb->hwi_links);
		asd_free_scb_platform_data(asd, scb->platform_data);
        	asd_free_mem(scb);
	}

	while (!list_empty(&asd->rsvd_scbs)) {
		scb = list_entry(asd->rsvd_scbs.next, struct scb, hwi_links);
		list_del(&scb->hwi_links);
		asd_free_scb_platform_data(asd, scb->platform_data);
        	asd_free_mem(scb);
	}

	while (!list_empty(&asd->free_scbs)) {
		scb = list_entry(asd->free_scbs.next, struct scb, hwi_links);
		list_del(&scb->hwi_links);
		asd_free_scb_platform_data(asd, scb->platform_data);
        	asd_free_mem(scb);
	}
	while (!list_empty(&asd->empty_scbs)) {
		/*
		 * Empties have no OSM data.
		 */
		scb = list_entry(asd->empty_scbs.next, struct scb, hwi_links);
		list_del(&scb->hwi_links);
        	asd_free_mem(scb);
	}

	/* Free up DMA safe memory shared with the controller */
	while (!list_empty(&asd->hscb_maps)) {
		map = list_entry(asd->hscb_maps.next, struct map_node, links);
		list_del(&map->links);
		asd_dmamem_free(asd, asd->hscb_dmat, map->vaddr, map->dmamap);
		asd_free_mem(map);
	}
	while (!list_empty(&asd->sg_maps)) {
		map = list_entry(asd->sg_maps.next, struct map_node, links);
		list_del(&map->links);
		asd_dmamem_free(asd, asd->sg_dmat, map->vaddr, map->dmamap);
		asd_free_mem(map);
	}

	switch (asd->init_level) {
	default:
	case 7:
		asd_free_mem(asd->free_ddb_bitmap);
		/* FALLTHROUGH */
	case 6:
		asd_dmamem_free(asd, asd->shared_data_dmat,
				asd->shared_data_map.vaddr,
				asd->shared_data_map.dmamap);
		/* FALLTHROUGH */
	case 5:
		asd_dma_tag_destroy(asd, asd->shared_data_dmat);
		/* FALLTHROUGH */
	case 4:
#ifdef EXTENDED_SCB
	{
/* free memory for extended scb*/
		asd_free_dma_mem(asd, asd->ext_scb_dmat, &asd->ext_scb_map);
#ifdef ASD_DEBUG
		asd_print("EXTENDED_SCB is freed\n");
#endif
	}
#endif
		asd_dma_tag_destroy(asd, asd->sg_dmat);
		/* FALLTHROUGH */
	case 3:
		asd_dma_tag_destroy(asd, asd->hscb_dmat);
		/* FALLTHROUGH */
	case 2:
		asd_free_mem(asd->qinfifo);
		/* FALLTHROUGH */
	case 1:
		asd_free_mem(asd->scbindex);
		/* FALLTHROUGH */
	case 0:
		break;
	}
}

/* 
 * Function:
 *	asd_hwi_setup_sw_bar()
 *
 * Description:
 *      Setup the location of internal space where the Sliding Window will
 *	point to.
 */
static void
asd_hwi_setup_sw_bar(struct asd_softc *asd)
{
	/* Setup Sliding Window A and B to point to CHIM_REG_BASE_ADR. */
	asd_write_dword(asd, PCIC_BASEA, CHIM_REG_BASE_ADR);
	asd_write_dword(asd, PCIC_BASEB, CHIM_REG_BASE_ADR);
	
	asd->io_handle[0]->swb_base = (uint32_t) CHIM_REG_BASE_ADR;        
}

/* 
 * Function:
 *	asd_hwi_init_phys()
 *
 * Description:
 *      Alllocate phy structures and intialize them to default settings. 
 */
static int
asd_hwi_init_phys(struct asd_softc *asd)
{
	struct asd_phy	*phy;
	u_int		 phy_id;

        for (phy_id = 0; phy_id < asd->hw_profile.max_phys; phy_id++) {
        	phy = asd_alloc_mem(sizeof(*phy), GFP_KERNEL);
                if (phy == NULL) {
                	asd_log(ASD_DBG_ERROR," Alloc Phy failed.\n");
                        return (-ENOMEM);
		}
                
		memset(phy, 0x0, sizeof(*phy));
		
		/* Fill in the default settings. */
		phy->id = phy_id;
		phy->max_link_rate = SAS_30GBPS_RATE;
		phy->min_link_rate = SAS_15GBPS_RATE;       
                /*
                 * Set the phy attributes to support SSP, SMP and STP 
                 * initiator mode. Target mode is not supported.
                 */
		phy->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR | 
			     ASD_STP_INITIATOR);
		
		/* 
		 * By default, use the adapter WWN as the SAS address for 
		 * the phy. 
		 */
                memcpy(phy->sas_addr, asd->hw_profile.wwn, SAS_ADDR_LEN);

		/* Allocate buffer for IDENTIFY ADDRESS frame. */
		if (asd_dma_tag_create(asd, 8, sizeof(struct sas_id_addr),
				       GFP_ATOMIC, &phy->id_addr_dmat) != 0)
			return (-ENOMEM);

		if (asd_dmamem_alloc(asd, phy->id_addr_dmat, 
				    (void **) &phy->id_addr_map.vaddr,
				    GFP_ATOMIC,
				    &phy->id_addr_map.dmamap,
				    &phy->id_addr_map.busaddr) != 0) {
			asd_dma_tag_destroy(asd, phy->id_addr_dmat);
			return (-ENOMEM);
		}

		INIT_LIST_HEAD(&phy->pending_scbs);
		phy->state = ASD_PHY_UNUSED;
		phy->src_port = NULL;
                phy->softc = (void *) asd;
		INIT_LIST_HEAD(&phy->links);
		phy->pat_gen = 0;
		asd->phy_list[phy_id] = phy;
	}
	
	asd_hwi_get_nv_phy_settings(asd);
	asd_hwi_get_nv_phy_params(asd);

	return (0);	
}

/* 
 * Function:
 *	asd_hwi_init_ports()
 *
 * Description:
 *      Alllocate port structures and intialize them to default settings. 
 */
static int
asd_hwi_init_ports(struct asd_softc *asd)
{
	struct asd_port	*port;
	u_char		i;

	for (i = 0; i < asd->hw_profile.max_ports; i++) {
       		port = asd_alloc_mem(sizeof(*port), GFP_ATOMIC);
		if (port == NULL) {
			asd_log(ASD_DBG_ERROR," Alloc Port failed.\n");
			return (-ENOMEM);
		}			
	
		memset(port, 0x0, sizeof(*port));
		INIT_LIST_HEAD(&port->phys_attached);
		INIT_LIST_HEAD(&port->targets);
		INIT_LIST_HEAD(&port->targets_to_validate);

		if (asd_alloc_dma_mem(asd, sizeof(struct SMPRequest),
			(void **)&port->dc.SMPRequestFrame,
			&port->dc.SMPRequestBusAddr,
			&port->dc.smp_req_dmat,
			&port->dc.smp_req_map) != 0) {

			return (-ENOMEM);
		}

		if (asd_alloc_dma_mem(asd, sizeof(struct SMPResponse),
			(void **)&port->dc.SMPResponseFrame,
			&port->dc.SMPResponseBusAddr,
			&port->dc.smp_resp_dmat,
			&port->dc.smp_resp_map) != 0) {

			/*
			 * If we get get the response, free the request.
			 */
			asd_free_dma_mem(asd, 
				port->dc.smp_req_dmat,
				&port->dc.smp_req_map);

			return (-ENOMEM);
		}

		/*
		 * The SASInfoFrame includes the length of the list as the 
		 * first element.
		 */
		port->dc.sas_info_len = MAX(
			(ASD_MAX_LUNS + 1) * sizeof(uint64_t),
			PRODUCT_SERIAL_NUMBER_LEN);

		if (asd_alloc_dma_mem(asd,
			port->dc.sas_info_len,
			(void **)&port->dc.SASInfoFrame,
			&port->dc.SASInfoBusAddr,
			&port->dc.sas_info_dmat,
			&port->dc.sas_info_map) != 0) {

			/*
			 * If we get get the report luns ...
			 */
			asd_free_dma_mem(asd, 
				port->dc.smp_req_dmat,
				&port->dc.smp_req_map);

			asd_free_dma_mem(asd, 
				port->dc.smp_resp_dmat,
				&port->dc.smp_resp_map);

			return (-ENOMEM);
		}

		/* Fill in default settings. */
		port->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR |
			      ASD_STP_INITIATOR);
		port->softc = (void *) asd;
		port->state = ASD_PORT_UNUSED;
		port->events = ASD_IDLE;
		port->link_type = ASD_LINK_UNKNOWN;
		port->management_type = ASD_DEVICE_NONE;
		port->id = i;
		asd->port_list[i] = port;
	}

	return (0);
}

/* 
 * Function:
 *	asd_hwi_alloc_scbs()
 *
 * Description:
 *      Allocate SCB buffers. 
 */
static void
asd_hwi_alloc_scbs(struct asd_softc *asd)
{
	struct scb 		*next_scb;
	union hardware_scb 	*hscb;
	struct map_node 	*hscb_map;
	struct map_node 	*sg_map;
	uint8_t			*segs;
	dma_addr_t	 	 hscb_busaddr;
	dma_addr_t	 	 sg_busaddr;
	int		 	 newcount;
	int		 	 i;

	if (asd->numscbs >= asd->hw_profile.max_scbs)
		/* Can't allocate any more */
		return;

	if (asd->scbs_left != 0) {
		int offset;

		offset = (PAGE_SIZE / sizeof(*hscb)) - asd->scbs_left;
		hscb_map = list_entry(asd->hscb_maps.next,
				      struct map_node, links);
		hscb = &((union hardware_scb *)hscb_map->vaddr)[offset];
		hscb_busaddr = hscb_map->busaddr + (offset * sizeof(*hscb));
	} else {
		hscb_map = asd_alloc_mem(sizeof(*hscb_map), GFP_ATOMIC);

		if (hscb_map == NULL)
			return;

		/* Allocate the next batch of hardware SCBs */
		if (asd_dmamem_alloc(asd, asd->hscb_dmat,
				     (void **) &hscb_map->vaddr, GFP_ATOMIC,
				     &hscb_map->dmamap,
				     &hscb_map->busaddr) != 0) {
			asd_free_mem(hscb_map);
			return;
		}

		list_add(&hscb_map->links, &asd->hscb_maps);
		hscb = (union hardware_scb *)hscb_map->vaddr;
		hscb_busaddr = hscb_map->busaddr;
		asd->scbs_left = PAGE_SIZE / sizeof(*hscb);

		asd_log(ASD_DBG_RUNTIME, "Mapped SCB data. %d SCBs left. "
			"Total SCBs %d.\n", 
			asd->scbs_left, asd->numscbs);
	}

	if (asd->sgs_left != 0) {
		int offset;

		offset = ((asd_sglist_allocsize(asd) / asd_sglist_size(asd))
		     		- asd->sgs_left) * asd_sglist_size(asd);
		sg_map = list_entry(asd->sg_maps.next,
				    struct map_node, links);
		segs = sg_map->vaddr + offset;
		sg_busaddr = sg_map->busaddr + offset;
	} else {
		sg_map = asd_alloc_mem(sizeof(*sg_map), GFP_ATOMIC);

		if (sg_map == NULL)
			return;

		/* Allocate the next batch of S/G lists */
		if (asd_dmamem_alloc(asd, asd->sg_dmat,
				     (void **) &sg_map->vaddr, GFP_ATOMIC,
				     &sg_map->dmamap, &sg_map->busaddr) != 0) {
			asd_free_mem(sg_map);
			return;
		}

		list_add(&sg_map->links, &asd->sg_maps);
		segs = sg_map->vaddr;
		sg_busaddr = sg_map->busaddr;
		asd->sgs_left =
			asd_sglist_allocsize(asd) / asd_sglist_size(asd);
	}

	newcount = MIN(asd->scbs_left, asd->sgs_left);
	newcount = MIN(newcount, (asd->hw_profile.max_scbs - asd->numscbs));

	for (i = 0; i < newcount; i++) {
		struct asd_scb_platform_data *pdata;

		next_scb = (struct scb *) asd_alloc_mem(sizeof(*next_scb),
							GFP_ATOMIC);
		if (next_scb == NULL)
			break;

		memset(next_scb, 0, sizeof(*next_scb));
		INIT_LIST_HEAD(&next_scb->hwi_links);
		INIT_LIST_HEAD(&next_scb->owner_links);

		pdata = asd_alloc_scb_platform_data(asd);
		if (pdata == NULL) {
			asd_free_mem(next_scb);
			break;
		}
		next_scb->platform_data = pdata;
		init_timer(&next_scb->platform_data->timeout);
		next_scb->hscb_map = hscb_map;
		next_scb->sg_map = sg_map;
		next_scb->sg_list = (struct sg_element *)segs;
		memset(hscb, 0, sizeof(*hscb));
		next_scb->hscb = hscb;

		next_scb->hscb_busaddr = asd_htole64(hscb_busaddr);
		next_scb->sg_list_busaddr = sg_busaddr;
		next_scb->softc = asd;
		next_scb->flags = SCB_FLAG_NONE;
		next_scb->eh_state = SCB_EH_NONE;
		next_scb->hscb->header.index = asd_htole16(asd->numscbs);
		asd->scbindex[asd_htole16(asd->numscbs)] = next_scb;
		
		/* Add the scb to the free list. */
		asd_hwi_free_scb(asd, next_scb);
		hscb++;
		hscb_busaddr += sizeof(*hscb);
		segs += asd_sglist_size(asd);
		sg_busaddr += asd_sglist_size(asd);
		asd->numscbs++;
		asd->scbs_left--;
		asd->sgs_left--;
	}
}

/*
 * Function:
 *	asd_alloc_ddb
 *
 * Description:
 *      Allocate a DDB site on the controller.
 *	Returns ASD_INVALID_DDB_INDEX on failure.
 *	Returns DDB index on success.
 */
uint16_t
asd_alloc_ddb(struct asd_softc *asd)
{
	u_int i;
	u_int bit_index;
	for (i = 0; i < asd->ddb_bitmap_size; i++) {
		if (asd->free_ddb_bitmap[i] != ~0UL)
			break;
	}
	if (i >= asd->ddb_bitmap_size)
		return (ASD_INVALID_DDB_INDEX);

	bit_index = ffz(asd->free_ddb_bitmap[i]);
	asd->free_ddb_bitmap[i] |= (u_long)(1UL << bit_index);
	return ((i * BITS_PER_LONG) + bit_index);
}

/*
 * Function:
 *	asd_free_ddb
 *
 * Description:
 *      Mark the DDB site at "ddb_index" as free.
 */
void
asd_free_ddb(struct asd_softc *asd, uint16_t ddb_index)
{
	u_int word_offset;
	u_int bit_offset;

	word_offset = ddb_index / BITS_PER_LONG;
	bit_offset = ddb_index & (BITS_PER_LONG - 1);
	asd->free_ddb_bitmap[word_offset] &= (u_long)(~(1UL << bit_offset));
}

/*
 * Function:
 *	asd_hwi_setup_ddb_site()
 *  
 * Description:
 *	Alloc and DDB site and setup the DDB site for the controller.
 */
int
asd_hwi_setup_ddb_site(struct asd_softc *asd, struct asd_target *target)
{
	uint16_t	ddb_index;

	/* Allocate a free DDB site. */
	ddb_index = asd_alloc_ddb(asd);
	if (ddb_index == ASD_INVALID_DDB_INDEX)
		return (-1);
	
	target->ddb_profile.conn_handle = ddb_index;	
	
	asd_hwi_build_ddb_site(asd, target);

	return (0);
}

/* 
 * Function:
 *	asd_hwi_init_sequencers()
 *
 * Description:
 *      Initialize the Central and Link Sequencers. 
 */
static int
asd_hwi_init_sequencers(struct asd_softc *asd)
{
	/* Pause the CSEQ. */
	if (asd_hwi_pause_cseq(asd) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to pause the CSEQ.\n");
		return (-1);	
	}
	
	/* Pause all the LSEQs. */
	if (asd_hwi_pause_lseq(asd, asd->hw_profile.enabled_phys) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to pause the LSEQs.\n");
		return (-1);		
	}
	/* Download the sequencers. */
	if (asd_hwi_download_seqs(asd) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to setup the SEQs.\n");
		return (-1);
	}

	/*
	 * Zero out all of the ddb_sites
	 */
	asd_hwi_init_ddb_sites(asd);

	/* 
	 * Initialiaze the DDB site 0 and 1used internally by the
	 * sequencer.
	 */
	asd_hwi_init_internal_ddb(asd);

	/* Setup and initialize the CSEQ and LSEQ(s). */
	asd_hwi_setup_seqs(asd);
	
	return (0);			
}

/* 
 * Function:
 *	asd_hwi_get_scb()
 *
 * Description:
 *      Get a free SCB Desc from the free list if any.
 */
struct scb *
asd_hwi_get_scb(struct asd_softc *asd, int rsvd_pool)
{
	struct scb	*scb;

	ASD_LOCK_ASSERT(asd);

	if (rsvd_pool == 1) {
		/* Get an SCB from the reserved pool. */
		if (list_empty(&asd->rsvd_scbs)) {
			/*
		 	 * We shouldn't be running out reserved SCBs.
		 	 */
			asd_log(ASD_DBG_ERROR, "Running out reserved SCBs.\n");
			return (NULL);
		}
		scb = list_entry(asd->rsvd_scbs.next, struct scb, hwi_links);
		scb->flags |= SCB_RESERVED;
#ifdef ASD_DEBUG
		printk("reserved scb 0x%x is allocated\n", scb);
#endif
	} else {
		/* Get an SCB from the free pool. */
		if (list_empty(&asd->free_scbs)) {
			asd_hwi_alloc_scbs(asd);
			if (list_empty(&asd->free_scbs)) {
#if 0
				asd_log(ASD_DBG_ERROR, 
					"Failed to get a free SCB.\n");
#endif
				return (NULL);
			}
		}
		scb = list_entry(asd->free_scbs.next, struct scb, hwi_links);
	}
	list_del(&scb->hwi_links);
	scb->post_stack_depth = 0;

	return (scb);
}

/*
 * Function:
 *	asd_hwi_enable_phy()
 *
 * Description:
 *	Enable the requested phy.
 */
int
asd_hwi_enable_phy(struct asd_softc *asd, struct asd_phy *phy)
{
	struct scb		*scb;
	uint8_t			phy_id;
	u_long		 	flags;

	phy_id = phy->id;

#if SAS_COMSTOCK_SUPPORT
	/*
 	 * For COMSTOCK:
	 * 	1. We need to setup OOB signal detection limits.
	 */
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_BFLTR), 0x40);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_INIT_MIN), 0x06);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_INIT_MAX), 0x13);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_INIT_NEG), 0x13);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_SAS_MIN), 0x13);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_SAS_MAX), 0x36);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_SAS_NEG), 0x36);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_WAKE_MIN), 0x02);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_WAKE_MAX), 0x06);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_WAKE_NEG), 0x06);
	asd_hwi_swb_write_word(asd, LmSEQ_OOB_REG(phy_id, OOB_IDLE_MAX), 
			       0x0080);
	asd_hwi_swb_write_word(asd, LmSEQ_OOB_REG(phy_id, OOB_BURST_MAX), 
			       0x0080);
	/*
 	 *	2. Put the OOB in slow clock mode. That corrects most of the 
	 *	   other timer parameters including the signal transmit values
	 * 	   for 37.5 MHZ.
	 */
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, OOB_MODE), SLOW_CLK);

#endif /* SAS_COMSTOCK_SUPPORT */
				
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, INT_ENABLE_2), 0x0);


#if !SAS_COMSTOCK_SUPPORT
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, HOT_PLUG_DELAY),
			       HOTPLUG_DEFAULT_DELAY);

	/*
	 * Set the PHY SETTINGS values based on the manufacturing 
	 * programmed values that we obtained from the NVRAM.
	 */
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, PHY_CONTROL_0),
			       phy->phy_ctl0);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, PHY_CONTROL_1),
			       phy->phy_ctl1);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, PHY_CONTROL_2),
			       phy->phy_ctl2);
	asd_hwi_swb_write_byte(asd, LmSEQ_OOB_REG(phy_id, PHY_CONTROL_3),
			       phy->phy_ctl3);
#endif

#ifndef SEQUENCER_UPDATE
	/* Initialize COMINIT_TIMER timeout. */
	asd_hwi_swb_write_dword(asd, LmSEQ_COMINIT_TIMEOUT(phy_id),
				SAS_DEFAULT_COMINIT_TIMEOUT);
#endif

	/* Build Identify Frame address. */
	asd_hwi_build_id_frame(phy);

	/* Fill in bus address for Identify Frame buffer. */
	asd_hwi_set_hw_addr(asd, LmSEQ_TX_ID_ADDR_FRAME(phy_id),
			    phy->id_addr_map.busaddr);

	asd_hwi_control_activity_leds(asd, phy->id, ENABLE_PHY);

	asd_lock(asd, &flags);

	scb = asd_hwi_get_scb(asd, 0);
	if (scb == NULL) {
#if defined(__VMKLNX__)
		asd_unlock(asd, &flags);
#endif
		asd_log(ASD_DBG_ERROR, "Failed to get a free SCB.\n");
		return (-1);
	}

	/* Store the phy pointer. */
	scb->io_ctx = (void *) phy;
	scb->flags |= SCB_INTERNAL;

	/* Build CONTROL PHY SCB. */
	asd_hwi_build_control_phy(scb, phy, ENABLE_PHY);

	list_add_tail(&scb->owner_links, &phy->pending_scbs);

	asd_hwi_post_scb(asd, scb);

	asd_unlock(asd, &flags);

	return (0);	
}

void
asd_hwi_release_sata_spinup_hold(
struct asd_softc	*asd,
struct asd_phy		*phy
)
{
	struct scb		*scb;
	uint8_t			phy_id;
	u_long		 	flags;

	phy_id = phy->id;
				
	asd_lock(asd, &flags);

	scb = asd_hwi_get_scb(asd, 0);
	if (scb == NULL) {
#if defined(__VMKLNX__)
		asd_unlock(asd, &flags);
#endif
		asd_log(ASD_DBG_ERROR, "Failed to get a free SCB.\n");
		return;
	}

	/* Store the phy pointer. */
	scb->io_ctx = (void *) phy;
	scb->flags |= SCB_INTERNAL;

	/* Build CONTROL PHY SCB. */
	asd_hwi_build_control_phy(scb, phy, RELEASE_SPINUP_HOLD);

	list_add_tail(&scb->owner_links, &phy->pending_scbs);

	asd_hwi_post_scb(asd, scb);

	asd_unlock(asd, &flags);

	return;
}

void
asd_hwi_process_devint(struct asd_softc *asd)
{
	uint32_t		dchstatus;
	unsigned		link_num;
	uint16_t		break_addr;
	uint32_t		intval;
	struct asd_step_data	*step_datap;

	dchstatus = asd_hwi_swb_read_dword(asd,
		(DCH_SAS_REG_BASE_ADR + DCHSTATUS));

	asd_dprint("DCHSTATUS = 0x%x\n", dchstatus);

	step_datap = NULL;

	if (dchstatus & LSEQINT_MASK) {
		for (link_num = 0 ; link_num < ASD_MAX_PHYS ; link_num++) {
			if ((dchstatus & (1 << link_num)) == 0)  {
				continue;
			}

			intval = asd_hwi_swb_read_dword(asd,
				LmARP2INT(link_num));

			asd_dprint("intval = 0x%x\n", intval);

#ifdef SEQUENCER_UPDATE
			if (intval & ARP2BREAK0) {
				break_addr = (asd_hwi_swb_read_dword(asd,
					LmARP2BREAKADR01(link_num)) &
					BREAKADR0_MASK) * 4;

#ifdef ASD_DEBUG
				printk("LSEQ %d Break @ 0x%x\n",
					link_num, break_addr);
#endif
				step_datap = asd_hwi_alloc_step(asd);

				asd_hwi_lseq_init_step(step_datap,
					link_num);

				asd_hwi_start_step_timer(step_datap);

				asd_hwi_swb_write_dword(asd,
					LmARP2INT(link_num),
					ARP2BREAK0);
			}
			else if (intval & ARP2BREAK1) {
				break_addr = (asd_hwi_swb_read_dword(asd,
					LmARP2BREAKADR01(link_num)) &
					BREAKADR1_MASK) * 4;

#ifdef ASD_DEBUG
				printk("LSEQ %d Break @ 0x%x\n",
					link_num, break_addr);
#endif
				step_datap = asd_hwi_alloc_step(asd);

				asd_hwi_lseq_init_step(step_datap,
					link_num);

				asd_hwi_start_step_timer(step_datap);

				asd_hwi_swb_write_dword(asd,
					LmARP2INT(link_num),
					ARP2BREAK1);
			}
			else if (intval & ARP2BREAK2) {
				break_addr = (asd_hwi_swb_read_dword(asd,
					LmARP2BREAKADR23(link_num)) &
					BREAKADR2_MASK) * 4;

#ifdef ASD_DEBUG
				printk("LSEQ %d Break @ 0x%x\n",
					link_num, break_addr);
#endif
				step_datap = asd_hwi_alloc_step(asd);

				asd_hwi_lseq_init_step(step_datap,
					link_num);

				asd_hwi_start_step_timer(step_datap);

				asd_hwi_swb_write_dword(asd,
					LmARP2INT(link_num),
					ARP2BREAK2);
			}
			else if (intval & ARP2BREAK3) {
				break_addr = (asd_hwi_swb_read_dword(asd,
					LmARP2BREAKADR23(link_num)) &
					BREAKADR3_MASK) * 4;

#ifdef ASD_DEBUG
				printk("LSEQ %d Break @ 0x%x\n",
					link_num, break_addr);
#endif
				step_datap = asd_hwi_alloc_step(asd);

				asd_hwi_lseq_init_step(step_datap,
					link_num);

				asd_hwi_start_step_timer(step_datap);

				asd_hwi_swb_write_dword(asd,
					LmARP2INT(link_num),
					ARP2BREAK3);
			}
#endif
			if (intval & ARP2CIOPERR) {
#ifdef ASD_DEBUG
//				asd_hwi_dump_seq_state(asd, 0xff);
//#else
				asd_print("Fatal: ARP2CIOPERR\n");
#endif
			}

			asd_hwi_swb_write_dword(asd,
				LmARP2INT(link_num), intval);
		}

		if (step_datap == NULL) {
			asd_hwi_unpause_lseq(asd, link_num);
		}
	} else if (dchstatus & CSEQINT) {
		intval = asd_hwi_swb_read_dword(asd, CARP2INT);

		asd_dprint("intval = 0x%x\n", intval);

#ifdef SEQUENCER_UPDATE
		if (intval & ARP2BREAK0) {
			break_addr = (asd_hwi_swb_read_dword(asd,
				CARP2BREAKADR01) & BREAKADR0_MASK) * 4;
#ifdef ASD_DEBUG
			printk("CSEQ Break @ 0x%x\n", break_addr);
#endif
			step_datap = asd_hwi_alloc_step(asd);

			if (asd_hwi_cseq_init_step(step_datap) != 0) {
				asd_hwi_start_step_timer(step_datap);
			} else {
				asd_hwi_free_step(step_datap);
			}

			asd_hwi_swb_write_dword(asd, CARP2INT, ARP2BREAK0);
		}
#endif

		if (intval & ARP2CIOPERR) {
#ifdef ASD_DEBUG
//			asd_hwi_dump_seq_state(asd, 0xff);
//#else
			asd_print("Fatal: ARP2CIOPERR\n");
#endif
		}

		asd_hwi_swb_write_dword(asd, CARP2INT, intval);
	}
}
//JD
#ifdef ASD_DEBUG

typedef struct state_name {
	uint8_t		name[16];
	uint32_t	status;
} state_name_t;

static state_name_t	CHIMINT_STATUS[] =
{
	{"EXT_INT0",		0x00000800},
	{"EXT_INT1",		0x00000400},
	{"PORRSTDET",		0x00000200},
	{"HARDRSTDET",		0x00000100},
	{"DLAVAILQ",		0x00000080},	/* ro */
	{"HOSTERR",			0x00000040},
	{"INITERR",			0x00000020},
	{"DEVINT",			0x00000010},
	{"COMINT",			0x00000008},
	{"DEVTIMER2",		0x00000004},
	{"DEVTIMER1",		0x00000002},
	{"DLAVAIL",			0x00000001},
   {"", 0 }	/* Last entry should be NULL. */
};
	
/* 
 * Function:
 *	dump_CHIMINT()
 *
 * Description:
 *      Display CHIMINT Status (from IRQ).
 */
void dump_CHIMINT(uint32_t intstate)
{
	int i;

	asd_log(ASD_DBG_INFO, "asd_process_irq: CHIMINT is (0x%x):\n",intstate);
	for(i=0;CHIMINT_STATUS[i].status!=0;i++)
	{
		if(intstate & CHIMINT_STATUS[i].status) asd_log(ASD_DBG_INFO, "- %s\n",CHIMINT_STATUS[i].name);
	}
}
#endif //ASD_DEBUG

/* 
 * Function:
 *	asd_hwi_process_irq()
 *
 * Description:
 *      Process any interrupts pending for our controller.
 */
int
asd_hwi_process_irq(struct asd_softc *asd)
{
	struct asd_done_list	*dl_entry;
	int			irq_processed;
	uint32_t		intstat;

	ASD_LOCK_ASSERT(asd);

	/*
	 * Check if any DL entries need to be processed.  If so,
	 * bypass a costly read of our interrupt status register
	 * and assume that the done list entries are the cause of
	 * our interrupt.
	 */
	dl_entry = &asd->done_list[asd->dl_next];
	if ((dl_entry->toggle & ASD_DL_TOGGLE_MASK) == asd->dl_valid)
		intstat = DLAVAIL;
	else
		intstat = asd_read_dword(asd, CHIMINT);
	if (intstat & DLAVAIL) {
		asd_write_dword(asd, CHIMINT, DLAVAIL); 
		/*
		 * Ensure that the chip sees that we've cleared
		 * this interrupt before we walk the done_list.
		 * Otherwise, we may, due to posted bus writes,
		 * clear the interrupt after we finish the scan,
		 * and after the sequencer has added new entries
		 * and asserted the interrupt again.
		 *
		 * NOTE: This extra read, and in fact the clearing
		 *       of the command complete interrupt, will
		 *       not be needed on systems using MSI.
		 */
		asd_flush_device_writes(asd);

		asd_hwi_process_dl(asd);	

		irq_processed = 1;
	} else if ((intstat & CHIMINT_MASK) != 0) {
		if ((intstat & DEVINT) != 0) {
//#ifdef ASD_DEBUG
			printk("DEVINT intstat 0x%x\n", intstat);
//#endif
			asd_hwi_process_devint(asd);
			//asd_hwi_dump_seq_state(asd, 0xff);
			asd_write_dword(asd, CHIMINT, DEVINT); 

			irq_processed = 1;
		} else {
//#ifdef ASD_DEBUG
			printk("unknown interrupt 0x%x\n", intstat);
//#endif
			asd_write_dword(asd, CHIMINT, intstat); 
#ifdef ASD_DEBUG
			dump_CHIMINT(intstat);
			asd_hwi_dump_seq_state(asd, 0xff);
#endif
		}

		irq_processed = 1;
	} else {
		irq_processed = 0;
	}
	
	return (irq_processed);
}

/* 
 * Function:
 *	asd_hwi_process_dl()
 *
 * Description:
 *      Process posted Done List entries.
 */
static void
asd_hwi_process_dl(struct asd_softc *asd)
{
	struct asd_done_list 	*dl_entry;
	struct scb		*scb;

	/*
	 * Look for entries in the done list that have completed.
	 * The valid_tag completion field indicates the validity
	 * of the entry - the valid value toggles each time through
	 * the queue.
	 */
	if ((asd->flags & ASD_RUNNING_DONE_LIST) != 0)
		panic("asd_hwi_process_dl recursion");

	asd->flags |= ASD_RUNNING_DONE_LIST;

	for (;;) {
		dl_entry = &asd->done_list[asd->dl_next];

		if ((dl_entry->toggle & ASD_DL_TOGGLE_MASK) != asd->dl_valid)
			break;

		scb = asd->scbindex[dl_entry->index];
//JD
#ifdef ASD_DEBUG
#if 0
		if(asd->debug_flag==1)
		{
// 		asd_log(ASD_DBG_INFO, "asd_hwi_process_dl: dumping SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x , opcode 0x%x.\n", 
// 			scb->io_ctx->scsi_cmd.cmnd[2],
// 			scb->io_ctx->scsi_cmd.cmnd[3],
// 			scb->io_ctx->scsi_cmd.cmnd[4],
// 			scb->io_ctx->scsi_cmd.cmnd[5],
// 			scb->io_ctx->scsi_cmd.cmnd[7],
// 			scb->io_ctx->scsi_cmd.cmnd[8],
// 			scb->io_ctx->scsi_cmd.cmnd[0]);
//
//		asd_log(ASD_DBG_INFO, "scb ptr=%p dl_next=%d  dl ptr=%p op=%x index = %x\n", scb,asd->dl_next,dl_entry,
//			dl_entry->opcode,dl_entry->index);
		asd_log(ASD_DBG_INFO, "scb ptr=%p opcode=%x index=%x\n", scb,dl_entry->opcode,dl_entry->index);



		}
#endif
#endif
		if ((scb->flags & SCB_PENDING) != 0) {
			list_del(&scb->hwi_links);
			scb->flags &= ~SCB_PENDING;
		}

		/* Process DL Entry. */
		switch (dl_entry->opcode) {
		case TASK_COMP_WO_ERR:
		case TASK_COMP_W_UNDERRUN:
		case TASK_COMP_W_OVERRUN:
		case TASK_F_W_OPEN_REJECT:
		case TASK_INT_W_BRK_RCVD:
		case TASK_INT_W_PROT_ERR:
		case SSP_TASK_COMP_W_RESP:
		case TASK_INT_W_PHY_DOWN:
		case LINK_ADMIN_TASK_COMP_W_RESP:
		case CSMI_TASK_COMP_WO_ERR:
		case ATA_TASK_COMP_W_RESP:
		case TASK_INT_W_NAK_RCVD:
		case RESUME_COMPLETE:
		case TASK_INT_W_ACKNAK_TO:
		case TASK_F_W_SMPRSP_TO:
		case TASK_F_W_SMP_XMTRCV_ERR:
		case TASK_F_W_NAK_RCVD:
		case TASK_ABORTED_BY_ITNL_EXP:
		case ATA_TASK_COMP_W_R_ERR_RCVD:
		case TMF_F_W_TC_NOT_FOUND:
		case TASK_ABORTED_ON_REQUEST:
		case TMF_F_W_TAG_NOT_FOUND:
		case TMF_F_W_TAG_ALREADY_FREE:
		case TMF_F_W_TASK_ALREADY_DONE:
		case TMF_F_W_CONN_HNDL_NOT_FOUND:
		case TASK_CLEARED:
		case TASK_UA_W_SYNCS_RCVD:
		case TASK_UNACKED_W_BREAK_RCVD:
		case TASK_UNACKED_W_ACKNAK_TIMEOUT:
			asd_pop_post_stack(asd, scb, dl_entry);
			break;
			
		case CONTROL_PHY_TASK_COMP:
		{
			struct control_phy_sb	*cntrl_phy;
			
			cntrl_phy = &dl_entry->stat_blk.control_phy;
	
			if (cntrl_phy->sb_opcode == PHY_RESET_COMPLETED) {
				asd_hwi_process_phy_comp(asd, scb, cntrl_phy);
			} else {
				asd_log(ASD_DBG_RUNTIME, "Invalid status "
					"block upcode.\n");
			}

			/* 
			 * Post routine needs to be called if the 
			 * CONTROL PHY is issued as result of error recovery
			 * process or from CSMI.
			 */
			if ((scb->flags & SCB_RECOVERY) != 0)
				asd_pop_post_stack(asd, scb, dl_entry);

			break;
		}

		default:
			/* 
			 * Making default case for EDB received and
			 * and non supported DL opcode. 
			 */
			if ((dl_entry->opcode >= 0xC1) && 
			    (dl_entry->opcode <= 0xC7)) 
				asd_hwi_process_edb(asd, dl_entry);
			else
				asd_log(ASD_DBG_RUNTIME, 
					"Received unsupported "
					"DL entry (opcode = 0x%x).\n",
				  	 dl_entry->opcode);
		      	
			break;

		}

		asd->dl_next = (asd->dl_next + 1) & asd->dl_wrap_mask;
		if (asd->dl_next == 0)
			asd->dl_valid ^= ASD_DL_TOGGLE_MASK;
	}
//JD
#ifdef ASD_DEBUG
// 	asd->debug_flag=0;
#endif
	asd->flags &= ~ASD_RUNNING_DONE_LIST;
} 

/*
 * Function:
 * 	asd_hwi_process_phy_comp()
 * 
 * Description:
 * 	Process phy reset completion.
 */
static void
asd_hwi_process_phy_comp(struct asd_softc *asd, struct scb *scb,
			 struct control_phy_sb *cntrl_phy)
{
	struct asd_phy			*phy;
	struct asd_control_phy_hscb 	*cntrlphy_scb;

	cntrlphy_scb = &scb->hscb->control_phy;
	phy = (struct asd_phy *) scb->io_ctx;

	switch (cntrlphy_scb->sub_func) {
	case DISABLE_PHY:
	{
		asd->hw_profile.enabled_phys &= ~(1 << phy->id);
		phy->state = ASD_PHY_OFFLINE;

		/*
		 * Check if this phy is attached to a target.
	       	 */	 
		if (phy->src_port != NULL) {
			/*
			 * DC: Currently, we treat this similar to loss of 
			 *     signal scenario.
			 *     Need to examine the behavior once the phy is
			 *     is disabled !!
			 *     Prior to disabling the phy that has target
			 *     connected, we need to abort all outstanding
			 *     IO to the affected target ports.
		         */	 
			phy->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR | 
			     	     ASD_STP_INITIATOR);
			phy->src_port->events |= ASD_LOSS_OF_SIGNAL;
			asd_wakeup_sem(&asd->platform_data->discovery_sem);
		}
		
		list_del(&scb->owner_links);	
		asd_hwi_free_scb(asd, scb);

		break;
	}
	
	case ENABLE_PHY:
		/* Check the OOB status from the link reset sequence. */
		if (cntrl_phy->oob_status & CURRENT_OOB_DONE) {
			if ((asd->hw_profile.enabled_phys & 
				(1 << phy->id)) == 0)
				asd->hw_profile.enabled_phys |= (1 << phy->id);

			/* There is a device attached. */
			if (cntrl_phy->oob_status & CURRENT_DEVICE_PRESENT) {
				phy->attr |= ASD_DEVICE_PRESENT;

				if (cntrl_phy->oob_status & CURRENT_SPINUP_HOLD)
					phy->attr |= ASD_SATA_SPINUP_HOLD;

				phy->state = ASD_PHY_WAITING_FOR_ID_ADDR;
			} else {
				phy->state = ASD_PHY_ONLINE;
			}

			/* Get the negotiated connection rate. */
			if (cntrl_phy->oob_mode & PHY_SPEED_30)
				phy->conn_rate = SAS_30GBPS_RATE;
			else if (cntrl_phy->oob_mode & PHY_SPEED_15)
				phy->conn_rate = SAS_15GBPS_RATE;
			
			/* Get the transport mode. */
			if (cntrl_phy->oob_mode & SAS_MODE)
				phy->attr |= ASD_SAS_MODE;
			else if (cntrl_phy->oob_mode & SATA_MODE)
				phy->attr |= ASD_SATA_MODE;
		} else if (cntrl_phy->oob_status & CURRENT_SPINUP_HOLD) {
			/*	
			 * SATA target attached that has not been transmitted
			 * COMWAKE (spun-up).
			 */
			asd_log(ASD_DBG_RUNTIME, "CURRENT SPINUP HOLD.\n");
		
			phy->attr |= ASD_SATA_SPINUP_HOLD;
			phy->state = ASD_PHY_WAITING_FOR_ID_ADDR;

		} else if (cntrl_phy->oob_status & CURRENT_ERR_MASK) {
			asd_log(ASD_DBG_ERROR, "OOB ERROR.\n");
			
			phy->state = ASD_PHY_OFFLINE;
		} else {
			/* 
			 * This should be the case when no device is
			 * connected.
			 */
			phy->state = ASD_PHY_ONLINE;
		}

#ifdef ASD_TEST
		asd_hwi_dump_phy(phy);
#endif
		if ((scb->flags & SCB_RECOVERY) == 0) {
			list_del(&scb->owner_links);	
			asd_hwi_free_scb(asd, scb);
			asd_wakeup_sem(&asd->platform_data->discovery_sem);
		} else {
			if (phy->state == ASD_PHY_OFFLINE)
				scb->eh_status = SCB_EH_FAILED;	
			else
				scb->eh_status = SCB_EH_SUCCEED;
		}
		break;

	case RELEASE_SPINUP_HOLD:
		/* To be implemented */
		asd_log(ASD_DBG_RUNTIME,
			"CONTROL PHY : RELEASE SPINUP HOLD.\n");
		break;
	
	case PHY_NO_OP:
		asd_log(ASD_DBG_RUNTIME, "CONTROL PHY : PHY NO OP.\n");

		if ((scb->flags & SCB_RECOVERY) != 0) {
			/*
		 	 * PHY NO OP control completion. The phy no op was 
			 * issued after HARD RESET completion.
		 	 */
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_SUCCEED;
		}
		break;

	case EXECUTE_HARD_RESET:
		asd_log(ASD_DBG_RUNTIME,"CONTROL PHY : EXECUTE HARD RESET.\n");

		if ((scb->flags & SCB_RECOVERY) != 0) {
			/* 
	    	 	 * Upon HARD RESET completion, we need to issue 
			 * PHY NO OP control to enable the hot-plug timer 
			 * which was disabled prior to issuing HARD RESET.
		 	 */
			scb->eh_state = SCB_EH_PHY_NO_OP_REQ;
			scb->eh_status = SCB_EH_SUCCEED;
		}
		break;

	default:
		asd_log(ASD_DBG_RUNTIME,
			"CONTROL PHY : INVALID SUBFUNC OPCODE.\n");
		break;
	}

	return;
}

#ifdef ASD_TEST
static void
asd_hwi_dump_phy(struct asd_phy  *phy)
{
	u_char	i;
	struct asd_port	*port;

	port = phy->src_port;
	
	asd_print("Phy Id = 0x%x.\n", phy->id);
	asd_print("Phy attr = 0x%x.\n", phy->attr);
	asd_print("Phy state = 0x%x.\n", phy->state);
	asd_print("Phy conn_rate = 0x%x.\n", phy->conn_rate);
	asd_print("Phy src port = %p.\n", phy->src_port);
	for (i = 0; i < 8; i++)
		asd_print("Phy %d SAS ADDR[%d]=0x%x.\n", phy->id, i,
				phy->sas_addr[i]);
}
#endif

union edb *
asd_hwi_indexes_to_edb(struct asd_softc *asd, struct scb **pscb,
		       u_int escb_index, u_int edb_index)
{
	struct scb 		*scb;
	struct asd_empty_hscb 	*escb;
	struct empty_buf_elem 	*ebe;

	if (escb_index > asd->hw_profile.max_scbs)
		return (NULL);
	scb = asd->scbindex[escb_index];
	if (scb == NULL)
		return (NULL);
	escb = &scb->hscb->empty_scb;
	ebe = &escb->buf_elem[edb_index];
	if (ELEM_BUFFER_VALID_FIELD(ebe) != ELEM_BUFFER_VALID)
		return (NULL);
	*pscb = scb;
	return (asd_hwi_get_edb_vaddr(asd, asd_le64toh(ebe->busaddr)));
}

/*
 * Function:
 * 	asd_hwi_process_edb()
 *
 * Description:
 *	Process Empty Data Buffer that was posted by the sequencer.
 */
static void
asd_hwi_process_edb(struct asd_softc *asd, struct asd_done_list *dl_entry)
{
	struct edb_rcvd_sb	*edbr;
	union  edb		*edb;
	struct scb		*scb;
	struct asd_phy		*phy;
	u_char			 phy_id;
	u_char			 elem_id;

	edbr = &dl_entry->stat_blk.edb_rcvd;
	elem_id = (dl_entry->opcode & EDB_OPCODE_MASK) - 1;
	phy_id = edbr->sb_opcode & EDB_OPCODE_MASK;
	phy = asd->phy_list[phy_id];
	edbr->sb_opcode &= ~EDB_OPCODE_MASK;	
	
	edb = asd_hwi_indexes_to_edb(asd, &scb,
				     asd_le16toh(dl_entry->index),
				     elem_id);

	switch (edbr->sb_opcode) {
	case BYTES_DMAED:
	{
		struct bytes_dmaed_subblk	*bytes_dmaed;
		u_int				 bytes_rcvd;

		bytes_dmaed = &edbr->edb_subblk.bytes_dmaed;
		bytes_rcvd = asd_le16toh(bytes_dmaed->edb_len) & 
			     BYTES_DMAED_LEN_MASK;

		if (bytes_rcvd > sizeof(union sas_bytes_dmaed))
			bytes_rcvd = sizeof(union sas_bytes_dmaed);

		memcpy(&phy->bytes_dmaed_rcvd.id_addr_rcvd, edb, bytes_rcvd);
		phy->events |= ASD_ID_ADDR_RCVD;
		phy->state = ASD_PHY_WAITING_FOR_ID_ADDR;
		asd_wakeup_sem(&asd->platform_data->discovery_sem);

#ifdef ASD_TEST
		asd_hwi_dump_phy_id_addr(phy);
#endif

		break;
	}

	case PRIMITIVE_RCVD:
	{
		struct primitive_rcvd_subblk	*prim_rcvd;

		prim_rcvd = &edbr->edb_subblk.prim_rcvd;
		asd_log(ASD_DBG_RUNTIME,
			"EDB: PRIMITIVE RCVD. Addr = 0x%x, Cont = 0x%x\n", 
			 prim_rcvd->reg_addr, prim_rcvd->reg_content);
		/*
		 * Only process primitive for phy(s) that already associated
		 * with port.
		 */
		if (phy->src_port != NULL)
			asd_hwi_process_prim_event(asd, phy,
						   prim_rcvd->reg_addr,
						   prim_rcvd->reg_content);
		break;
	}
	
	case PHY_EVENT:
	{
		struct phy_event_subblk *phy_event;

		phy_event = &edbr->edb_subblk.phy_event;
		asd_log(ASD_DBG_RUNTIME,
			"EDB: PHY_EVENT. Stat 0x%x, Mode 0x%x, Sigs = 0x%x\n",
			phy_event->oob_status, phy_event->oob_mode,
			phy_event->oob_signals);

		phy_event->oob_status &= CURRENT_PHY_MASK;
		asd_hwi_process_phy_event(asd, phy, 
					  phy_event->oob_status,
					  phy_event->oob_mode);
		break;
	}
	
	case LINK_RESET_ERR:
	{
		struct link_reset_err_subblk	*link_rst;

		link_rst = &edbr->edb_subblk.link_reset_err;

		asd_log(ASD_DBG_RUNTIME, "EDB: LINK RESET ERRORS. \n");
		asd_log(ASD_DBG_RUNTIME, "Timedout waiting %s from Phy %d.\n",
			((link_rst->error == RCV_FIS_TIMER_EXP) ?
			 "Initial Device-to-Host Register FIS"  :
			 "IDENTITY Address Frame"), 
			phy_id);

		asd_hwi_handle_link_rst_err(asd, phy);

		break;

	}

	case TIMER_EVENT:
	{
		struct timer_event_subblk *timer_event;

		timer_event = &edbr->edb_subblk.timer_event; 

		asd_log(ASD_DBG_RUNTIME, "EDB: TIMER EVENT. Error = 0x%x \n", 
			timer_event->error);

		asd_hwi_process_timer_event(asd, phy, timer_event->error); 

		break;
	}
	
	case REQ_TASK_ABORT:
	{
		struct req_task_abort_subblk	*req_task_abort;
			
		asd_log(ASD_DBG_RUNTIME, "EDB: REQUEST TASK ABORT. \n");

		req_task_abort = &edbr->edb_subblk.req_task_abort; 

		asd_log(ASD_DBG_RUNTIME, "Req TC to Abort = 0x%x, "
			"Reason = 0x%x.\n", req_task_abort->task_tc_to_abort,
			req_task_abort->reason);

		asd_hwi_process_req_task(asd, edbr->sb_opcode,
					 req_task_abort->task_tc_to_abort);
		break;
	}
		
	case REQ_DEVICE_RESET:
	{
		struct req_dev_reset_subblk	*req_dev_reset;

		asd_log(ASD_DBG_RUNTIME, "EDB: REQUEST DEVICE RESET. \n");

		req_dev_reset = &edbr->edb_subblk.req_dev_reset; 

		asd_log(ASD_DBG_RUNTIME, "Req TC to Reset = 0x%x, "
			"Reason = 0x%x.\n", req_dev_reset->task_tc_to_abort,
			req_dev_reset->reason);

		asd_hwi_process_req_task(asd, edbr->sb_opcode,
					 req_dev_reset->task_tc_to_abort);
		break;
	}

	default:
		asd_log(ASD_DBG_RUNTIME, "Invalid EDB opcode.\n");
		break;
	}
		
	asd_hwi_free_edb(asd, scb, (elem_id));
}

/*
 * Function:
 * 	asd_hwi_process_prim_event()
 * 
 * Description:
 *	Process any recevied primitives that are not handled by the 
 *	firmware (eg. BROADCAST, HARD_RESET, etc.)
 */
static void
asd_hwi_process_prim_event(struct asd_softc *asd, struct asd_phy *phy,
			   u_int reg_addr, u_int reg_content)
{
	uint32_t	reg_val;

	reg_val = 0;
	if (reg_addr == LmPRMSTAT0BYTE1) {
		/* 
		 * First byte of Primitive Status register is intended for
		 * BROADCAST primitives.
		 */
		reg_val = (reg_content << 8) & 0xFF00;
		switch (reg_val) {
		case LmBROADCH:
		case LmBROADRVCH0:
		case LmBROADRVCH1:
			asd_log(ASD_DBG_RUNTIME, "BROADCAST PRIMITIVE "
				"received.\n");
			/* 
			 * Set the event that discovery is needed and
			 * wakeup discovery thread.  
			 */ 
			if(phy->src_port->events & ASD_DISCOVERY_PROCESS)
				phy->src_port->events |= ASD_DISCOVERY_RETRY;
			else
				phy->src_port->events = ASD_DISCOVERY_REQ;
			asd_wakeup_sem(&asd->platform_data->discovery_sem);
			phy->brdcst_rcvd_cnt++;
			break;

		default:
			asd_log(ASD_DBG_ERROR, "Unsupported BROADCAST "
				"primitive.\n");
			break;
		}
	} else if (reg_addr == LmPRMSTAT1BYTE0) {
		reg_val = reg_content & 0xFF;
		if (reg_val == LmHARDRST) {
			asd_log(ASD_DBG_RUNTIME, "HARD_RESET primitive "
				"received.\n");
		}
	} else if (reg_addr == LmPRMSTAT0BYTE3) {
		reg_val = (reg_content << 24) & 0xFF000000;
		if (reg_val == LmUNKNOWNP) {
			asd_log(ASD_DBG_RUNTIME, "Undefined primitive "
				"received.\n");
		}
	} else {
		asd_log(ASD_DBG_ERROR, "Unsupported PRIMITIVE STATUS REG.\n");
	}
}

/*
 * Function:
 * 	asd_hwi_process_phy_event()
 * 
 * Description:
 *	Process received async. phy events.
 */
static void
asd_hwi_process_phy_event(struct asd_softc *asd, struct asd_phy *phy, 
			  u_int oob_status, u_int oob_mode)
{
	switch (oob_status) {
	case DEVICE_REMOVED:
		/*
		 * We received a phy event that notified us that the 
		 * signal is lost with the direct attached device to 
		 * this phy. The device has been hot removed.
		 */	 
		asd_log(ASD_DBG_RUNTIME, 
			"PHY_EVENT (%d) - DEVICE HOT REMOVED.\n", phy->id);
		phy->state = ASD_PHY_ONLINE;
		phy->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR | 
			     ASD_STP_INITIATOR);

		if (phy->src_port != NULL)
			phy->src_port->events |= ASD_LOSS_OF_SIGNAL;

		asd_wakeup_sem(&asd->platform_data->discovery_sem);
		break;

	case DEVICE_ADDED_W_CNT:
	case DEVICE_ADDED_WO_CNT:
		asd_log(ASD_DBG_RUNTIME,
			"PHY_EVENT (%d) - DEVICE HOT ADDED.\n", phy->id);

		/* There is a device attached. */
		if (oob_status & CURRENT_DEVICE_PRESENT) {
			phy->attr |= ASD_DEVICE_PRESENT;

			if (oob_status & CURRENT_SPINUP_HOLD)
				phy->attr |= ASD_SATA_SPINUP_HOLD;

			phy->state = ASD_PHY_WAITING_FOR_ID_ADDR;
		}

		/* Get the negotiated connection rate. */
		if (oob_mode & PHY_SPEED_30) {
			phy->conn_rate = SAS_30GBPS_RATE;
		} else if (oob_mode & PHY_SPEED_15) {
			phy->conn_rate = SAS_15GBPS_RATE;
		}
			
		/* Get the transport mode. */
		if (oob_mode & SAS_MODE) {
			phy->attr |= ASD_SAS_MODE;
		} else if (oob_mode & SATA_MODE) {
			phy->attr |= ASD_SATA_MODE;
		}

		break;
	
	case CURRENT_OOB1_ERROR:
	case CURRENT_OOB2_ERROR:
		asd_print("PHY_EVENT (%d) - OOB ERROR.\n", phy->id);
		break;
	
	default:
		asd_log(ASD_DBG_ERROR,
			"PHY_EVENT (%d) - UNKNOWN EVENT 0x%x.\n", phy->id,
			oob_status);
		break;
	}
}

/*
 * Function:
 * 	asd_hwi_process_timer_event()
 * 
 * Description:
 *	Process received async. timer events.
 */
static void
asd_hwi_process_timer_event(struct asd_softc *asd, struct asd_phy *phy, uint8_t	error)
{
	switch (error) {
	case DWS_RESET_TO_EXP:
		/*
		 * We received a DWS timer event that the direct attached device may be lost,
		 * let's remove the device and restart a discovery.
		 */	 
		asd_log(ASD_DBG_RUNTIME, 
			"TIMER_EVENT (%d) - DWS_RESET_TO_EXP.\n", phy->id);
		phy->state = ASD_PHY_ONLINE;
		phy->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR | 
			     ASD_STP_INITIATOR);

		if (phy->src_port != NULL)
			phy->src_port->events |= ASD_LOSS_OF_SIGNAL;

		asd_wakeup_sem(&asd->platform_data->discovery_sem);
		break;

	default:
		asd_log(ASD_DBG_ERROR,
			"TIMER_EVENT (%d) - UNKNOWN EVENT 0x%x.\n", phy->id,
			error);
		break;
	}
}

#ifdef ASD_TEST
static void
asd_hwi_dump_phy_id_addr(struct asd_phy  *phy)
{
	u_char	i;

	asd_print("ID ADDRESS FRAME RECEIVED.\n");
	asd_print("Addr Frame Type = 0x%x.\n", 
			phy->bytes_dmaed_rcvd.id_addr_rcvd.addr_frame_type);
	asd_print("Init Port Type = 0x%x.\n", 
			phy->bytes_dmaed_rcvd.id_addr_rcvd.init_port_type);
	asd_print("Tgt Port Type = 0x%x.\n", 
			phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type);
	asd_print("Phy ID = 0x%x.\n", 
			phy->bytes_dmaed_rcvd.id_addr_rcvd.phy_id);
	for (i = 0; i < 8; i++)
		asd_print("TGT Sas Addr[%d] = 0x%x.\n",
			i, phy->bytes_dmaed_rcvd.id_addr_rcvd.sas_addr[i]);
}
#endif 

/*
 * Function:
 * 	asd_hwi_handle_link_rst_err()
 * 
 * Description:
 *	Handle Link Reset Error event as a result of timedout while
 *	waiting for Identity Frame Address or Initial Device-to-Host
 *	Register FIS from direct-attached device.
 */
static void
asd_hwi_handle_link_rst_err(struct asd_softc *asd, struct asd_phy *phy)
{
	struct scb	*scb;

	/*
	 * We are skipping OOB register initialization as it was done
	 * the first time we enable the phy.
	 * Should we redo this initialization again??
	 */

	/*
	 * We want to retry the link reset sequence up to a certain
	 * amount of time to retry establishing connection with the device.
	 */
	if (phy->link_rst_cnt > MAX_LINK_RESET_RETRY) {
		/* 
		 * We have tried link reset a few times, the device still
	 	 * failed to return the ID Frame Addr or Device-to-Host
		 * Register FIS.
		 * End the discovery process for this phy.
		 */
		phy->state = ASD_PHY_ONLINE;
		phy->attr = (ASD_SSP_INITIATOR | ASD_SMP_INITIATOR | 
			     ASD_STP_INITIATOR);
		asd_wakeup_sem(&asd->platform_data->discovery_sem);
	}

	if ((scb = asd_hwi_get_scb(asd, 0)) == NULL) {
		asd_log(ASD_DBG_ERROR, "Out of SCB resources.\n");

		return;
	}

	scb->io_ctx = (void *) phy;
	scb->flags |= SCB_INTERNAL;

	asd_hwi_build_control_phy(scb, phy, ENABLE_PHY);

	list_add_tail(&scb->owner_links, &phy->pending_scbs);
	phy->link_rst_cnt++;
	asd_hwi_post_scb(asd, scb);
}

/*
 * Function:
 * 	asd_hwi_process_req_task()
 * 
 * Description:
 *	Process the requested task recevied from the sequencer.
 */
static void
asd_hwi_process_req_task(struct asd_softc *asd, uint8_t req_type,
			 uint16_t index)
{
	struct scb	*scb;
	int		 found;

	if ((req_type != REQ_TASK_ABORT) && (req_type != REQ_DEVICE_RESET)) {
		asd_log(ASD_DBG_ERROR, "Unsupported REQ TASK 0x%x.\n",
			req_type);
		return;
	}

	found = 0;
	list_for_each_entry(scb, &asd->platform_data->pending_os_scbs,
			    owner_links) {
		if (SCB_GET_INDEX(scb) == index) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		asd_log(ASD_DBG_ERROR, "REQ TASK with invalid TC.\n");
#ifdef DEBUG_DDB
#ifdef ASD_DEBUG
		{
			u_long	lseqs_to_dump;
			u_int	lseq_id;
			int		indx;

			for(indx=0;indx< asd->ddb_bitmap_size; indx++)
			{
				lseqs_to_dump = asd->free_ddb_bitmap[indx];
				lseq_id = 0;

				while (lseqs_to_dump != 0) { 
					for ( ; lseq_id < (8 * sizeof(u_long)); lseq_id++) {
						if (lseqs_to_dump & (1UL << lseq_id)) {
							lseqs_to_dump &= ~(1UL << lseq_id);
							break;
						} 
					}
		/* Dump out specific LSEQ Registers state. */
					asd_hwi_dump_ssp_smp_ddb_site(asd, lseq_id + (indx * 8 * sizeof(ulong)));
				}
			}
		}
		asd_hwi_dump_seq_state(asd, asd->hw_profile.enabled_phys);
#endif
#endif
		return;
	}

	if (req_type == REQ_TASK_ABORT)
		scb->eh_state = SCB_EH_ABORT_REQ;
	else
		scb->eh_state = SCB_EH_DEV_RESET_REQ;

	scb->flags |= SCB_TIMEDOUT;
	scb->eh_post = asd_hwi_req_task_done;
	list_add_tail(&scb->timedout_links, &asd->timedout_scbs);
	asd_wakeup_sem(&asd->platform_data->ehandler_sem);
}

static void
asd_hwi_req_task_done(struct asd_softc *asd, struct scb *scb)
{
	asd_log(ASD_DBG_ERROR, "Req Task completed.\n");
}

/*************** Helper Functions to build a specific SCB type. ***************/

/*
 * Function:
 * 	asd_hwi_build_id_frame()
 *
 * Description:
 *	Build an Identify Frame address.
 */
static void
asd_hwi_build_id_frame(struct asd_phy *phy)
{
	struct sas_id_addr	*id_addr;
	u_char			 i;

	id_addr = (struct sas_id_addr *) phy->id_addr_map.vaddr;
	memset(id_addr, 0x0, sizeof(*id_addr));

	/* Set the device type to end-device. */
	id_addr->addr_frame_type |= SAS_END_DEVICE;	

	for (i = 0; i < SAS_ADDR_LEN; i++) 
		id_addr->sas_addr[i] = phy->sas_addr[i]; 
	
	/* Set the Initiator Port attributes. */
	id_addr->init_port_type = (SSP_INIT_PORT | STP_INIT_PORT | 
				   SMP_INIT_PORT);
	id_addr->phy_id = phy->id;
}

/* 
 * Function:
 *	asd_hwi_build_control_phy()
 *
 * Description:
 *      Build a CONTROL PHY SCB.
 *	CONTROL PHY SCB is used to control the operation of a phy, such as
 *	enable or disable phy, execute hard reset, release spinup hold and
 *	control ATA device.
 */
void
asd_hwi_build_control_phy(struct scb *scb, struct asd_phy *phy, 
			  uint8_t sub_func)
{
	struct asd_control_phy_hscb 	*cntrlphy_hscb;
	uint8_t 			 phy_id;
	uint8_t				 speed_mask;

	speed_mask = 0;
	phy_id = phy->id;
	cntrlphy_hscb = &scb->hscb->control_phy;

	cntrlphy_hscb->header.opcode = SCB_CONTROL_PHY;
	cntrlphy_hscb->phy_id = phy_id;
	cntrlphy_hscb->sub_func = sub_func;

	if ((cntrlphy_hscb->sub_func == ENABLE_PHY) || 
	    (cntrlphy_hscb->sub_func == EXECUTE_HARD_RESET) ||
	    (cntrlphy_hscb->sub_func == PHY_NO_OP)) {
		cntrlphy_hscb->func_mask = FUNCTION_MASK_DEFAULT;

		/* 
		 * Hot Plug timer needs to be disabled while performing
		 * Hard Reset.
		 */
		if (cntrlphy_hscb->sub_func == EXECUTE_HARD_RESET)
			cntrlphy_hscb->func_mask |= HOT_PLUG_DIS;

		/* mask all speeds */
		speed_mask = (SAS_SPEED_60_DIS | SAS_SPEED_30_DIS |
			      SAS_SPEED_15_DIS | SATA_SPEED_30_DIS |
			      SATA_SPEED_15_DIS);

		/* enable required speed */	
		asd_hwi_set_speed_mask(phy->max_link_rate, &speed_mask); 
		asd_hwi_set_speed_mask(phy->min_link_rate, &speed_mask);

		/* Razor should enable 1.5 Gbs and disable 3 Gbs for SATA */
		speed_mask &= ~SATA_SPEED_15_DIS;
		speed_mask |= SATA_SPEED_30_DIS;

#if SAS_COMSTOCK_SUPPORT
		/* COMSTOCK only support 1.5 Gbits/s data transfer. */
		speed_mask &= ~SATA_SPEED_15_DIS;
		cntrlphy_hscb->speed_mask = speed_mask | (SATA_SPEED_30_DIS | 
					    SAS_SPEED_60_DIS |
					    SAS_SPEED_30_DIS);	 
#else
		cntrlphy_hscb->speed_mask = speed_mask;
#endif
		/* Set to Hot plug time delay to 100 ms. */
		cntrlphy_hscb->hot_plug_delay = HOTPLUG_DEFAULT_DELAY;
		cntrlphy_hscb->port_type = (SSP_INITIATOR_PORT | 
					    STP_INITIATOR_PORT |
					    SMP_INITIATOR_PORT);
	} else {
		cntrlphy_hscb->func_mask = 0;
		cntrlphy_hscb->speed_mask = 0;
		cntrlphy_hscb->hot_plug_delay = 0;
		cntrlphy_hscb->port_type = 0;
	}

#ifdef SEQUENCER_UPDATE
	cntrlphy_hscb->device_present_timer_ovrd_enable = 0;
	cntrlphy_hscb->device_present_timeout_const_override = 0;
#else
	cntrlphy_hscb->ovrd_cominit_timer = 0;
	cntrlphy_hscb->cominit_timer_const_ovrd = 0;
#endif
	memset(&cntrlphy_hscb->res1[0], 0x0, 111);
	cntrlphy_hscb->conn_handle = 0xFFFF;
}

/* 
 * Function:
 *	asd_hwi_build_abort_task()
 *
 * Description:
 *      Build an ABORT TASK SCB.
 *	ABORT TASK scb is used to abort an SCB previously sent to the 
 *	firmware.
 */
void
asd_hwi_build_abort_task(struct scb *scb, struct scb *scb_to_abort)
{
	struct asd_abort_task_hscb	*abort_hscb;
	struct asd_target		*targ;

	targ = scb_to_abort->platform_data->targ;
	abort_hscb = &scb->hscb->abort_task;
	abort_hscb->header.opcode = SCB_ABORT_TASK;

	memset(&abort_hscb->protocol_conn_rate, 0x0,
	       offsetof(struct asd_abort_task_hscb, res3) -
	       offsetof(struct asd_abort_task_hscb, protocol_conn_rate));
	/* 
	 * The conn_rate is only valid when aborting an SCB with SSP protocol.
	 */
	if (targ->transport_type == ASD_TRANSPORT_SSP)
		abort_hscb->protocol_conn_rate = targ->ddb_profile.conn_rate;

	if ((SCB_GET_OPCODE(scb_to_abort) == SCB_INITIATE_SSP_TASK) ||
	    (SCB_GET_OPCODE(scb_to_abort) == SCB_INITIATE_LONG_SSP_TASK)) 
		abort_hscb->protocol_conn_rate |= PROTOCOL_TYPE_SSP;
	else if ((SCB_GET_OPCODE(scb_to_abort) == SCB_INITIATE_ATA_TASK) ||
		 (SCB_GET_OPCODE(scb_to_abort) == SCB_INITIATE_ATAPI_TASK))
		abort_hscb->protocol_conn_rate |= PROTOCOL_TYPE_STP;
	else
		abort_hscb->protocol_conn_rate |= PROTOCOL_TYPE_SMP;
	/*
	 * Build SSP Frame Header and SSP Task IU, these fields only valid
         * when aborting an SCB with SSP protocol.
	 */
	if (targ->transport_type == ASD_TRANSPORT_SSP) {
		/* SSP Frame Header. */
		abort_hscb->sas_header.frame_type = TASK_FRAME;
		memcpy(abort_hscb->sas_header.hashed_dest_sasaddr,
		       targ->ddb_profile.hashed_sas_addr, HASHED_SAS_ADDR_LEN);
		memcpy(abort_hscb->sas_header.hashed_src_sasaddr,
		       targ->src_port->hashed_sas_addr, HASHED_SAS_ADDR_LEN);
		abort_hscb->sas_header.target_port_xfer_tag = 0xFFFF;
		abort_hscb->sas_header.data_offset = 0;

		/* SSP Task IU. */
		if (scb_to_abort->platform_data->dev != NULL) {
			/*
			 * We could be aborting task for SMP target or
		         * target during discovery and there is no device
			 * associated with that target.
			 */	 
			memcpy(abort_hscb->task_iu.lun,
			       scb_to_abort->platform_data->dev->saslun,
			       SAS_LUN_LEN);
		}

		abort_hscb->task_iu.tmf = ABORT_TASK_TMF;
		/*
		 * Setting tag_to_manage to 0xFFFF will indicate the sequencer
	         * to use conn_handle, lun, and tc_to_abort to determine the
		 * I_T_L_Q nexus of the task to be aborted.
		 */	 
		abort_hscb->task_iu.tag_to_manage = 0xFFFF;
	}

	abort_hscb->sister_scb = 0xFFFF;
	abort_hscb->conn_handle = targ->ddb_profile.conn_handle;

	/* 
	 * For Aborting SSP Task, we need to suspend the data transmission
	 * of the task to be aborted.
	 */
	if (SCB_GET_OPCODE(scb_to_abort) == SCB_INITIATE_SSP_TASK) {
		abort_hscb->suspend_data = SUSPEND_DATA;
		scb->eh_state |= SCB_EH_SUSPEND_SENDQ;
	}

	abort_hscb->retry_cnt = TASK_RETRY_CNT;

	/* Set the TC to be aborted. */
	abort_hscb->tc_to_abort = asd_htole16(SCB_GET_INDEX(scb_to_abort));
}

/* 
 * Function:
 *	asd_hwi_build_query_task()
 *
 * Description:
 *      Build an QUERY TASK SCB.
 *	QUERY TASK scb is used to issue an SSP Task IU for a Query Task
 *	Task Management Function.
 */
void
asd_hwi_build_query_task(struct scb *scb, struct scb *scb_to_query)
{
	struct asd_query_ssp_task_hscb	*query_hscb;
	struct asd_target		*targ;

	targ = scb_to_query->platform_data->targ;
	query_hscb = &scb->hscb->query_ssp_task;
	query_hscb->header.opcode = SCB_QUERY_SSP_TASK;

	memset(&query_hscb->protocol_conn_rate, 0x0,
	       offsetof(struct asd_query_ssp_task_hscb, res3) -
	       offsetof(struct asd_query_ssp_task_hscb, protocol_conn_rate));
		
	query_hscb->protocol_conn_rate = (targ->ddb_profile.conn_rate |
					  PROTOCOL_TYPE_SSP);
	/* SSP Frame Header. */
	query_hscb->sas_header.frame_type = TASK_FRAME;
	memcpy(query_hscb->sas_header.hashed_dest_sasaddr,
	       targ->ddb_profile.hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	memcpy(query_hscb->sas_header.hashed_src_sasaddr,
	       targ->src_port->hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	query_hscb->sas_header.target_port_xfer_tag = 0xFFFF;
	query_hscb->sas_header.data_offset = 0;

	/* SSP Task IU. */
	query_hscb->task_iu.tmf = QUERY_TASK_TMF;
	/*
	 * Setting tag_to_manage to 0xFFFF will indicate the sequencer
         * to use conn_handle, lun, and tc_to_query to determine the
	 * I_T_L_Q nexus of the task to be queried.
	 */	 
	query_hscb->task_iu.tag_to_manage = 0xFFFF;

	query_hscb->sister_scb = 0xFFFF;
	query_hscb->conn_handle = targ->ddb_profile.conn_handle;
	query_hscb->retry_cnt = TASK_RETRY_CNT;
	/* Set the TC to be queried. */
	query_hscb->tc_to_query = asd_htole16(SCB_GET_INDEX(scb_to_query));
}

/* 
 * Function:
 *	asd_hwi_build_clear_nexus()
 *
 * Description:
 *      Build a CLEAR NEXUS SCB.
 *	CLEAR NEXUS SCB is used to request the firmware that a set of pending
 *	transactions pending for a specified nexus be return to the driver
 *	and free the associated SCBs to the free list.
 */
void
asd_hwi_build_clear_nexus(struct scb *scb, u_int nexus_ind, u_int parm,
			  u_int context)
{
	struct asd_clear_nexus_hscb	*clr_nxs_hscb;
	struct asd_target		*targ;

#define RESUME_SENDQ_REQ					\
do {								\
	if (context == SCB_EH_RESUME_SENDQ) 			\
		clr_nxs_hscb->queue_ind = (uint8_t) RESUME_TX;	\
} while (0)
	
	clr_nxs_hscb = &scb->hscb->clear_nexus;
	clr_nxs_hscb->header.opcode = SCB_CLEAR_NEXUS;
	targ = scb->platform_data->targ;
	
	memset(&clr_nxs_hscb->nexus_ind, 0x0, 
	       offsetof(struct asd_clear_nexus_hscb, res8) - 
	       offsetof(struct asd_clear_nexus_hscb, nexus_ind));
	
	clr_nxs_hscb->nexus_ind = nexus_ind;
	switch (nexus_ind) {
	case CLR_NXS_I_OR_T:
		/* Clear Nexus intended for I or T. */
		clr_nxs_hscb->conn_mask_to_clr = targ->src_port->conn_mask;
		break;

	case CLR_NXS_I_T_L:
		/* Clear Nexus intended for I_T_L. */
		if (scb->platform_data->dev != NULL) {
			memcpy(clr_nxs_hscb->lun_to_clr,
			       scb->platform_data->dev->saslun,
			       SAS_LUN_LEN);
		}
		/* Fallthrough */
	case CLR_NXS_IT_OR_TI:
		/* Clear Nexus intended for I_T or T_I. */
		clr_nxs_hscb->conn_handle_to_clr = 
					targ->ddb_profile.conn_handle;
		clr_nxs_hscb->queue_ind = (uint8_t) parm;
		RESUME_SENDQ_REQ;

		break;
		
	case CLR_NXS_I_T_L_Q_TAG:
		/* Clear Nexus intended for I_T_L_Q (by tag). */
		clr_nxs_hscb->tag_to_clr = (uint16_t) parm;
		clr_nxs_hscb->conn_handle_to_clr = 
					targ->ddb_profile.conn_handle;
		if (scb->platform_data->dev != NULL) {
			memcpy(clr_nxs_hscb->lun_to_clr,
			       scb->platform_data->dev->saslun,
			       SAS_LUN_LEN);
		}
		RESUME_SENDQ_REQ;
		break;

	case CLR_NXS_I_T_L_Q_TC:
		/* Clear Nexus intended for I_T_L_Q (by TC). */
		clr_nxs_hscb->tc_to_clr = (uint16_t) parm;
		clr_nxs_hscb->conn_handle_to_clr = 
					targ->ddb_profile.conn_handle;
		if (scb->platform_data->dev != NULL) {
			memcpy(clr_nxs_hscb->lun_to_clr,
			       scb->platform_data->dev->saslun,
			       SAS_LUN_LEN);
		}
		RESUME_SENDQ_REQ;
		break;
		
	case CLR_NXS_I_T_L_Q_STAG:
		/* Clear Nexus intended for I_T_L_Q (by SATA tag). */
		clr_nxs_hscb->conn_handle_to_clr = 
					targ->ddb_profile.conn_handle;
		/* 
		 * Bits 4-0 of the tag_to_clr contain the SATA tag to be
		 * cleared. Bits 15-5 shall be set to zero.
		 */
		clr_nxs_hscb->tag_to_clr = ((uint16_t) parm && 0x001F);
		RESUME_SENDQ_REQ;
		break;

	case CLR_NXS_ADAPTER:
		/* Clear Nexus for Adapter. */
	default:
		/* Unsupported Clear Nexus function. */
		break;	
	}

	clr_nxs_hscb->nexus_ctx = (uint16_t) context;
}

/* 
 * Function:
 *	asd_hwi_build_ssp_tmf()
 *
 * Description:
 *      Build a SSP TMF SCB.
 *	SSP TMF SCB is used to issue an SSP Task information unit for a
 *	LOGICAL UNIT RESET, ABORT TASK SET, CLEAR TASK SET, or CLEAR ACA
 *	task management function.
 */
void
asd_hwi_build_ssp_tmf(struct scb *scb, struct asd_target *targ, 
		      uint8_t *lun, u_int tmf_opcode)
{
	struct asd_ssp_tmf_hscb	*tmf_hscb;

	tmf_hscb = &scb->hscb->ssp_tmf;
	tmf_hscb->header.opcode = SCB_INITIATE_SSP_TMF;

	memset(&tmf_hscb->protocol_conn_rate, 0x0,
	       offsetof(struct asd_ssp_tmf_hscb, res3) -
	       offsetof(struct asd_ssp_tmf_hscb, protocol_conn_rate));

	tmf_hscb->protocol_conn_rate = (targ->ddb_profile.conn_rate | 
					PROTOCOL_TYPE_SSP);
	/* SSP Frame Header. */
	tmf_hscb->sas_header.frame_type = TASK_FRAME;
	memcpy(tmf_hscb->sas_header.hashed_dest_sasaddr,
	       targ->ddb_profile.hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	memcpy(tmf_hscb->sas_header.hashed_src_sasaddr,
	       targ->src_port->hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	tmf_hscb->sas_header.target_port_xfer_tag = 0xFFFF;
	tmf_hscb->sas_header.data_offset = 0;
	
	/* SSP Task IU. */
	memcpy(tmf_hscb->task_iu.lun, lun, SAS_LUN_LEN);
	tmf_hscb->task_iu.tmf = tmf_opcode;

	tmf_hscb->sister_scb = 0xFFFF;
	tmf_hscb->conn_handle = targ->ddb_profile.conn_handle;
	
	/* Suspend data transmission to the target. */
	scb->eh_state |= SCB_EH_SUSPEND_SENDQ;
	tmf_hscb->suspend_data = SUSPEND_DATA;
	tmf_hscb->retry_cnt = TASK_RETRY_CNT;
}
 
/*
 * Function:
 * 	asd_hwi_build_smp_phy_req()
 *
 * Description:
 *	Build a SMP PHY related request.
 */ 
static void
asd_hwi_build_smp_phy_req(struct asd_port *port, int req_type,
			  int phy_id, int ctx)
{
	struct SMPRequest	*smp_req;

	smp_req = port->dc.SMPRequestFrame;
	memset(smp_req, 0, sizeof(*smp_req));

	smp_req->SMPFrameType = SMP_REQUEST_FRAME;

	switch (req_type) {
	case PHY_CONTROL:
		smp_req->Function = PHY_CONTROL;
		smp_req->Request.PhyControl.PhyIdentifier = phy_id;
		smp_req->Request.PhyControl.PhyOperation = ctx;
		break;

	case REPORT_PHY_ERROR_LOG:
		smp_req->Function = REPORT_PHY_ERROR_LOG;
		smp_req->Request.ReportPhyErrorLog.PhyIdentifier = phy_id;
		break;

	default:
		panic("Unknown SMP PHY request type.\n"); 
		break;
	}

	/*
	 * DC: Currently, we are not changing the programmed min/max 
         *     physical link rate for LINK RESET or HARD RESET.
	 *     We might need to change the link rate if CMSI application
	 *     required..
	 */
}

/*
 * Function:
 *	asd_hwi_build_smp_task()
 *
 * Description:
 *	Build a SMP TASK SCB.
 *	SMP TASK SCB is used to send an SMP TASK to an expander.
 */ 
void
asd_hwi_build_smp_task(struct scb *scb, struct asd_target *targ,
		       uint64_t req_bus_addr, u_int req_len, 
		       uint64_t resp_bus_addr, u_int resp_len)
{
	struct asd_smp_task_hscb	*smp_hscb;

	smp_hscb = &scb->hscb->smp_task;
	smp_hscb->header.opcode = SCB_INITIATE_SMP_TASK;

	memset(&smp_hscb->protocol_conn_rate, 0x0,
	       offsetof(struct asd_smp_task_hscb, res5) -
	       offsetof(struct asd_ssp_tmf_hscb, protocol_conn_rate));

	smp_hscb->protocol_conn_rate = (targ->ddb_profile.conn_rate |
					PROTOCOL_TYPE_SMP);
	smp_hscb->smp_req_busaddr = req_bus_addr;
	smp_hscb->smp_req_size = req_len;
	smp_hscb->smp_req_ds = 0;
	smp_hscb->sister_scb = 0xffff;
	smp_hscb->conn_handle = targ->ddb_profile.conn_handle;
	smp_hscb->smp_resp_busaddr = resp_bus_addr;
	smp_hscb->smp_resp_size = resp_len;
	smp_hscb->smp_resp_ds = 0;
}

/*
 * Function:
 *	asd_hwi_build_ssp_task()
 *
 * Description:
 *	Build a SSP TASK SCB.
 *	SSP TASK SCB is used to send an SSP TASK to an expander.
 */ 
void
asd_hwi_build_ssp_task(struct scb *scb, struct asd_target *targ,
		       uint8_t *saslun, uint8_t *cdb, uint32_t cdb_len,
		       uint8_t addl_cdb_len, uint32_t data_len)
{
	struct asd_ssp_task_hscb *ssp_hscb;

	ssp_hscb = &scb->hscb->ssp_task;
	ssp_hscb->header.opcode = SCB_INITIATE_SSP_TASK;
	ssp_hscb->protocol_conn_rate = targ->ddb_profile.conn_rate 
					| PROTOCOL_TYPE_SSP;
	ssp_hscb->xfer_len = asd_htole32(data_len);
	ssp_hscb->sas_header.frame_type = OPEN_ADDR_FRAME;
	
	memcpy(ssp_hscb->sas_header.hashed_dest_sasaddr,
	       targ->ddb_profile.hashed_sas_addr, HASHED_SAS_ADDR_LEN);

	ssp_hscb->sas_header.res = 0;

	memcpy(ssp_hscb->sas_header.hashed_src_sasaddr,
	       targ->src_port->hashed_sas_addr, HASHED_SAS_ADDR_LEN);

	memset(ssp_hscb->sas_header.res1, 0,
	       offsetof(struct asd_sas_header, target_port_xfer_tag) - 
	       offsetof(struct asd_sas_header, res1));

	ssp_hscb->sas_header.target_port_xfer_tag = 0xFFFF;
	ssp_hscb->sas_header.data_offset = 0;

	/* SSP Command IU */
	memset(ssp_hscb->lun, 0,
	       offsetof(struct asd_ssp_task_hscb, cdb) -
	       offsetof(struct asd_ssp_task_hscb, lun));
	memcpy(ssp_hscb->lun, saslun, 8);
	memcpy(ssp_hscb->cdb, cdb, cdb_len);
	
	memset(&ssp_hscb->cdb[cdb_len], 0,
	      SCB_EMBEDDED_CDB_SIZE - cdb_len);

	ssp_hscb->addl_cdb_len = addl_cdb_len;
	ssp_hscb->sister_scb = 0xFFFF;
	ssp_hscb->conn_handle = targ->ddb_profile.conn_handle;
	ssp_hscb->retry_cnt = TASK_RETRY_CNT;
	memset(&ssp_hscb->LAST_SSP_HSCB_FIELD, 0,
	       offsetof(struct asd_ssp_task_hscb, sg_elements) - 
	       offsetof(struct asd_ssp_task_hscb, LAST_SSP_HSCB_FIELD));

	return;
}

/*
 * Function:
 *	asd_hwi_build_stp_task()
 *
 * Description:
 *	Build a STP TASK SCB.
 *	STP TASK SCB is used to send an ATA TASK to an expander.
 */ 
void 
asd_hwi_build_stp_task(struct scb *scb, struct asd_target *targ,
		       uint32_t data_len)
{
	struct asd_ata_task_hscb *ata_hscb;

	ata_hscb = &scb->hscb->ata_task;
	ata_hscb->header.opcode = SCB_INITIATE_ATA_TASK;
	ata_hscb->protocol_conn_rate =
		PROTOCOL_TYPE_SATA | targ->ddb_profile.conn_rate;
	ata_hscb->xfer_len = asd_htole32(data_len);
	ata_hscb->data_offset = 0;
	ata_hscb->sister_scb = 0xffff;
	ata_hscb->conn_handle = targ->ddb_profile.conn_handle;
	ata_hscb->retry_cnt = TASK_RETRY_CNT;
	ata_hscb->affiliation_policy = 0;

	ata_hscb->ata_flags = 0;
#ifdef SEQUENCER_UPDATE
	ata_hscb->ata_flags |= UNTAGGED;
#else
#ifdef TAGGED_QUEUING
	// RST - add support for SATA II queueing
	ata_hscb->ata_flags |= LEGACY_QUEUING;
#else
	ata_hscb->ata_flags |= UNTAGGED;
#endif
#endif
	return;
}

/*
 * Function:
 * 	asd_hwi_hash()
 *
 * Desctiption:
 * 	Convert a 64-bit SAS address into a 24-bit Hash address.
 * 	This is based on the hash implementation from the SAS 1.1 draft.
 */
void
asd_hwi_hash(uint8_t *sas_addr, uint8_t *hashed_addr)
{
	const uint32_t 	distance_9_poly = 0x01DB2777;
	uint32_t 	upperbits;
	uint32_t 	lowerbits;
	uint32_t 	msb;
	uint32_t 	moving_one;
	uint32_t 	leading_bit;
	uint32_t 	regg;
	int	 	i;

	upperbits = scsi_4btoul(sas_addr);
	lowerbits = scsi_4btoul(sas_addr + 4);
	msb = 0x01000000;
	regg = 0;
	moving_one = 0x80000000;
	for (i = 31; i >= 0; i--) {
		leading_bit = 0;
		if (moving_one & upperbits)
			leading_bit = msb;
		regg <<= 1;
		regg ^= leading_bit;
		if (regg & msb)
			regg ^= distance_9_poly;
		moving_one >>= 1;
	}
	moving_one = 0x80000000;
	for (i = 31; i >= 0; i--) {
		leading_bit = 0;
		if (moving_one & lowerbits)
			leading_bit = msb;
		regg <<= 1;
		regg ^= leading_bit;
		if (regg & msb)
			regg ^= distance_9_poly;

		moving_one >>= 1;
	}
	scsi_ulto3b(regg, hashed_addr);
}

/*************************** Error Handling routines **************************/

void
asd_recover_cmds(struct asd_softc *asd)
{
	struct scb	*scb;
	struct scb	*free_scb;
	struct scb	*safe_scb;
	u_long		 flags;
		Scsi_Cmnd 		*cmd;
		struct asd_device *dev;

	if (list_empty(&asd->timedout_scbs)) {
		asd_log(ASD_DBG_ERROR, "Timed-out scbs already completed.\n");
		goto exit;
	}
	list_for_each_entry_safe(scb, safe_scb, &asd->timedout_scbs,
				 timedout_links) {
		asd_lock(asd, &flags);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_ERROR, "asd_recover_cmds: Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);
#endif


		/* 
		 * Error recovery is in progress for this scb.
		 * Proceed to next one.
		 */
		if ((scb->eh_state & SCB_EH_IN_PROGRESS) != 0) {
			asd_unlock(asd, &flags);
			continue;
		}
		/*
		 * Error recovery is completed for this scb.
	         */
		if (scb->eh_state == SCB_EH_DONE) {
			asd_unlock(asd, &flags);
			goto done;
		}
		/*
		 * Only allowed one Error Recovery ongoing for a particular
		 * target.
		 */
		if ((scb->platform_data->targ->flags &
					ASD_TARG_IN_RECOVERY) != 0) {
			asd_unlock(asd, &flags);
			continue;
		}
		/* 
		 * Acquire a free SCB from reserved pool to be used
		 * for error recovery purpose.
		 */
		if ((free_scb = asd_hwi_get_scb(asd, 1)) == NULL) {
			asd_log(ASD_DBG_ERROR, "Failed to get free SCB "
				"for error recovery.\n");
			asd_unlock(asd, &flags);
			continue;
		}
		/* Mark this target to be in recovery mode. */
		scb->platform_data->targ->flags |= ASD_TARG_IN_RECOVERY;

		/* Freeze the target's queue. */
		asd_freeze_targetq(asd, scb->platform_data->targ);

		/* Initialiaze the state. */
		scb->eh_state |= SCB_EH_IN_PROGRESS;
		free_scb->eh_state = SCB_EH_INITIATED;
		free_scb->eh_status = SCB_EH_SUCCEED;
		switch (scb->eh_state & SCB_EH_LEVEL_MASK) {
		case SCB_EH_ABORT_REQ:
			asd_log(ASD_DBG_ERROR, "ABORT ER REQ.\n");
			asd_hwi_abort_scb(asd, scb, free_scb);
			break;

		case SCB_EH_LU_RESET_REQ:
			asd_log(ASD_DBG_ERROR, "LU RESET ER REQ.\n");
			asd_hwi_reset_lu(asd, scb, free_scb);
			break;
		
		case SCB_EH_DEV_RESET_REQ:
			asd_log(ASD_DBG_ERROR, "DEV RESET ER REQ.\n");
			asd_hwi_reset_device(asd, scb, free_scb);
			break;
			
		case SCB_EH_PORT_RESET_REQ:
			asd_log(ASD_DBG_ERROR, "PORT RESET ER REQ.\n");
			asd_hwi_reset_port(asd, scb, free_scb);
			break;
		case SCB_EH_RESUME_SENDQ:
			{
		    struct asd_port		*port;
			asd_log(ASD_DBG_ERROR, "RESUME SENDQ REQ.\n");
			asd_unlock(asd, &flags);
			port = SCB_GET_SRC_PORT(scb);

			asd_delay(2000000);   //wait 2 sec for Broadcast event
		    asd_lock(asd, &flags);

			asd_hwi_resume_sendq(asd, scb, free_scb);
			break;
			}
			
		default:
			asd_log(ASD_DBG_ERROR, "Unknown Error Recovery "
				"Level scb 0x%x.\n",scb);
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			break;
		}
		asd_unlock(asd, &flags);

done:
		if (scb->eh_state == SCB_EH_DONE) {
			/*
			 * Error recovery is done for this scb,
		         * Clean up and free the scb.
			 */	 
			asd_lock(asd, &flags);

			list_del(&scb->timedout_links);
			scb->platform_data->targ->flags &= 
						~ASD_TARG_IN_RECOVERY;
			/* Unfreeze the target's queue. */
			asd_unfreeze_targetq(asd, scb->platform_data->targ);
 				scb->eh_status =SCB_EH_SUCCEED;
 			scb->eh_post(asd, scb);

			if(scb->flags & SCB_ABORT_DONE)
			{
				scb->eh_status =SCB_EH_SUCCEED;
			}
			else
			{
				scb->eh_status =SCB_EH_FAILED;
			}
			if (scb->eh_status != SCB_EH_FAILED) {
				struct asd_device *dev;
				/* 
				 * Schedule a timer to run the device
				 * queue if the device is not frozen and
				 * not in the process of being removed.
				 */
				dev = scb->platform_data->dev;
				if ((dev != NULL) && (dev->qfrozen == 0) &&
				    (dev->flags & ASD_DEV_TIMER_ACTIVE) == 0 &&
				    (dev->target->flags &
				     ASD_TARG_HOT_REMOVED) == 0) {


					asd_setup_dev_timer(
						dev, HZ,
						asd_timed_run_dev_queue);
				}
				/*
			 	 * Only free the scb if the error recovery is
			 	 * successful.
			 	 */



				scb->flags &= ~(SCB_TIMEDOUT+SCB_ABORT_DONE);
				asd_hwi_free_scb(asd, scb);
			} // if (scb->eh_status != SCB_EH_FAILED) 
//JDTEST
			else
			{
				//cleaning up failed recovery scb 
				asd_log(ASD_DBG_ERROR, "scb 0x%x SCB_EH_FAILED flags 0x%x\n",scb, scb->flags);
				dev = scb->platform_data->dev;
				if ((dev != NULL) && (dev->qfrozen == 0) &&
				    (dev->flags & ASD_DEV_TIMER_ACTIVE) == 0 &&
				    (dev->target->flags &
				     ASD_TARG_HOT_REMOVED) == 0) {


					asd_setup_dev_timer(
						dev, HZ,
						asd_timed_run_dev_queue);
				}
				if(scb->flags & SCB_PENDING)
				{

					list_del(&scb->hwi_links);
				}


				if(scb->flags & SCB_INTERNAL)
				{
					list_del(&scb->owner_links);
					asd_hwi_free_scb(asd, scb);
				}
				else
				{
					asd_log(ASD_DBG_INFO,"free scb from pending queue\n");
					list_del(&scb->owner_links);

					cmd = &acmd_scsi_cmd(scb->io_ctx);
					dev = scb->platform_data->dev;
					dev->active--;
					dev->openings++;
					if ((scb->flags & SCB_DEV_QFRZN) != 0) {
						scb->flags &= ~SCB_DEV_QFRZN;
						dev->qfrozen--;
					}

					asd_unmap_scb(asd, scb);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
					asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
					asd_cmd_set_offline_status(cmd);
#endif
					cmd->scsi_done(cmd);
					asd_hwi_free_scb(asd, scb);
				} //if(scb->flags & SCB_INTERNAL)
			} //if (scb->eh_status != SCB_EH_FAILED)
			asd_unlock(asd, &flags);
		} //if (scb->eh_state == SCB_EH_DONE)
	}//list_for_each_entry_safe(scb, safe_scb, &asd->timedout_scbs,
exit:
	return;
}

static void
asd_scb_eh_timeout(u_long arg)
{
	struct asd_softc 	*asd;
	struct scb		*scb;
	struct scb		*err_scb;
	u_long		 	 flags;
	
	scb = (struct scb *) arg;
	err_scb = (struct scb *) scb->post_stack[0].io_ctx;
	asd = scb->softc;
	asd_lock(asd, &flags);

	scb->eh_state = SCB_EH_TIMEDOUT;
	err_scb->eh_state &= ~SCB_EH_IN_PROGRESS;
	/* 
	 * Error recovery SCB timed out.
	 * If this error recovery requested by the OS, we need to mark the
	 * error recovery failed and make an attempt to perform the next
	 * level error recovery if possible.
	 */ 
#ifdef ASD_DEBUG	
//JD
	asd_log(ASD_DBG_INFO, "asd_scb_eh_timeout: err_scb->eh_state 0x%x\n",err_scb->eh_state);
#endif
	switch (err_scb->eh_state) {
	case SCB_EH_ABORT_REQ:
		err_scb->eh_state = SCB_EH_LU_RESET_REQ;
		break;
		
	case SCB_EH_LU_RESET_REQ:
		err_scb->eh_state = SCB_EH_DEV_RESET_REQ;
		break;

	case SCB_EH_DEV_RESET_REQ:
		err_scb->eh_state = SCB_EH_PORT_RESET_REQ;
		break;

	case SCB_EH_PORT_RESET_REQ:
		{

			// scb timeout during reset port
			struct asd_port		*port;
			struct asd_target	*targ;
			port = SCB_GET_SRC_PORT(err_scb);
			
			/* Unfreeze all the targets. */
			list_for_each_entry(targ, &port->targets, all_domain_targets) {
				targ->qfrozen--;
				asd_log(ASD_DBG_INFO,"clear freezen in timeout ptr=%p num=%d\n",targ,targ->qfrozen); 
			}
		}



	default:
		/* 
		 * Currently, our biggest hammer now is PORT RESET.
		 * We might perform ADAPTER RESET later on.
		 */ 
		err_scb->eh_state = SCB_EH_DONE;
		break;
	}

	err_scb->eh_status = SCB_EH_FAILED;
	err_scb->platform_data->targ->flags &= ~ASD_TARG_IN_RECOVERY;
	asd_unlock(asd, &flags);
	asd_wakeup_sem(&asd->platform_data->ehandler_sem);
}

int
asd_hwi_check_cmd_pending(struct asd_softc *asd, struct scb *scb, 
		       	  struct asd_done_list *dl)
{
	struct scb *abort_scb;

	while (!list_empty(&asd->timedout_scbs)) {
		abort_scb = list_entry(asd->timedout_scbs.next, 
				       struct scb, timedout_links);
		if (((struct asd_abort_task_hscb *)
		      &scb->hscb->abort_task)->tc_to_abort == 
		      asd_htole16(SCB_GET_INDEX(abort_scb))) {
			return (1);
		}
	}

	return (0);
}

/*
 * Function: 
 *	asd_hwi_abort_scb()
 *
 * Description:
 *	This routine will issue an ABORT_TASK to abort the requested scb.
 *	ONLY SCB with protocol SSP, SMP and STP can be issued ABORT_TASK TMF.
 */
static void
asd_hwi_abort_scb(struct asd_softc *asd, struct scb *scb_to_abort,
		  struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_log(ASD_DBG_ERROR, "Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);

	/*
	 * DC: We probably need to search the scb_to_abort in the 
	 *     target/device queue as initial step for internal requested
	 *     command.
	 */ 
	switch (scb->eh_state) {
	case SCB_EH_INITIATED:
	{
		/* 
		 * Validate the opcode of scb to be aborted. 
		 * Only scb with specific opcode can be aborted.
		 */
		if ((SCB_GET_OPCODE(scb_to_abort) != SCB_INITIATE_SSP_TASK) &&
	    	    (SCB_GET_OPCODE(scb_to_abort) != SCB_INITIATE_SMP_TASK) &&
	    	    (SCB_GET_OPCODE(scb_to_abort) != 
		     				SCB_INITIATE_LONG_SSP_TASK) &&
	    	    (SCB_GET_OPCODE(scb_to_abort) != SCB_INITIATE_ATA_TASK) &&
	    	    (SCB_GET_OPCODE(scb_to_abort) != SCB_INITIATE_ATAPI_TASK)) {
			asd_log(ASD_DBG_ERROR, "Requested to abort unsupported "
				"SCB request.\n");
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			/* 
			 * Recursively call this function again upon changing
		 	 * the state.
			 */
			asd_hwi_abort_scb(asd, scb_to_abort, scb);
			break;
		}

		scb->platform_data->targ = scb_to_abort->platform_data->targ;
		scb->platform_data->dev = scb_to_abort->platform_data->dev;
		scb->eh_state = SCB_EH_ABORT_REQ;
		asd_hwi_abort_scb(asd, (struct scb *) scb_to_abort, scb);
		break;
	}

	case SCB_EH_ABORT_REQ:
		/* Build the ABORT_TASK SCB. */
		asd_hwi_build_abort_task(scb, scb_to_abort);

		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Prepare to abort scb index(TC): 0x%x, TAG(0x%x) cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x\n",
			SCB_GET_INDEX(scb_to_abort), SCB_GET_SSP_TAG(scb_to_abort), 
			scb_to_abort->io_ctx->scsi_cmd.cmnd[2],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[3],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[4],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[5],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[7],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[8]);
#endif
		asd_push_post_stack(asd, scb, (void *) scb_to_abort,
				    asd_hwi_abort_scb_done);
		/* Post the ABORT_TASK SCB. */
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_CLR_NXS_REQ:
		/* 
		 * The Clear Nexus SCB has been prepared in the abort
		 * post routine. All we need is to post it to the firmware.
		 */
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) scb_to_abort,
				    asd_hwi_abort_scb_done);
		asd_hwi_post_scb(asd, scb);
		break;
	
	case SCB_EH_RESUME_SENDQ:
		/*
		 * We are here because the ABORT TMF failed.
	         * We need to resume the data transmission of the task
		 * that was going to be aborted.
		 */	 
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_hwi_build_clear_nexus(scb, CLR_NXS_I_T_L_Q_TC,
					  SCB_GET_INDEX(scb_to_abort),
					  SCB_EH_RESUME_SENDQ);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) scb_to_abort,
				    asd_hwi_abort_scb_done);
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_DONE:
		scb_to_abort->eh_state = scb->eh_state;
		scb_to_abort->eh_status = scb->eh_status;

		if ((scb_to_abort->eh_state == SCB_EH_DONE) && 
		    (scb_to_abort->eh_status == SCB_EH_FAILED)) {
			/*
		 	 * Failed to perform abort error recovery for the
		 	 * failed command, we shall procced with the next 
			 * level of error recovery (Logical Unit Reset).
			 * We need to change the scb eh_state to 
			 * SCB_EH_LU_RESET_REQ. 
		 	 */
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_hwi_abort_scb failed to perform scb index(TC): 0x%x, TAG(0x%x), cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x, going to reset all.\n",
			SCB_GET_INDEX(scb_to_abort), SCB_GET_SSP_TAG(scb_to_abort), 
			scb_to_abort->io_ctx->scsi_cmd.cmnd[2],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[3],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[4],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[5],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[7],
			scb_to_abort->io_ctx->scsi_cmd.cmnd[8]);

#ifdef DEBUG_DDB
		{
			u_long	lseqs_to_dump;
			u_int	lseq_id;
			int		indx;

			for(indx=0;indx< asd->ddb_bitmap_size; indx++)
			{
				lseq_id = 0;
				lseqs_to_dump = asd->free_ddb_bitmap[indx];

				while (lseqs_to_dump != 0) { 
					for ( ; lseq_id < (8 * sizeof(u_long)); lseq_id++) {
						if (lseqs_to_dump & (1UL << lseq_id)) {
							lseqs_to_dump &= ~(1UL << lseq_id);
							break;
						} 
					}
		/* Dump out specific LSEQ Registers state. */
					asd_hwi_dump_ssp_smp_ddb_site(asd, lseq_id + (indx * 8 * sizeof(ulong)));
				}
			}
		}
		asd_hwi_dump_seq_state(asd, asd->hw_profile.enabled_phys);
#endif
#endif
			scb_to_abort->eh_state = SCB_EH_LU_RESET_REQ;
			scb_to_abort->platform_data->targ->flags &= 
							~ASD_TARG_IN_RECOVERY;
		}
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;

	default:
		asd_log(ASD_DBG_ERROR, "Invalid EH State 0x%x.\n",
			scb->eh_state);

		scb_to_abort->eh_state = SCB_EH_DONE;
		scb_to_abort->eh_status = SCB_EH_FAILED;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}
}

static void
asd_hwi_abort_scb_done(struct asd_softc *asd, struct scb *scb, 
		       struct asd_done_list *dl)
{
	struct scb		*scb_to_abort;
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_hwi_abort_scb_done: scb index(TC): 0x%x, TAG(0x%x), cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x is done\n",
			SCB_GET_INDEX(scb), SCB_GET_SSP_TAG(scb), 
			scb->io_ctx->scsi_cmd.cmnd[2],
			scb->io_ctx->scsi_cmd.cmnd[3],
			scb->io_ctx->scsi_cmd.cmnd[4],
			scb->io_ctx->scsi_cmd.cmnd[5],
			scb->io_ctx->scsi_cmd.cmnd[7],
			scb->io_ctx->scsi_cmd.cmnd[8]);
	asd_log(ASD_DBG_INFO, "asd_hwi_abort_scb_done: scb ptr=%p scb_abort ptr=%p\n",scb,scb->io_ctx);
#endif

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	/*
	 * This post routine is shared by ABORT_TASK and CLEAR_NEXUS
         * issued by this abort handler code.
 	 * Hence, the DL opcodes also apply to both SCB task.
	 */	 
	switch (dl->opcode) {
	case TASK_COMP_WO_ERR:
	case TMF_F_W_TAG_NOT_FOUND:
	case TMF_F_W_CONN_HNDL_NOT_FOUND:
	case TMF_F_W_TASK_ALREADY_DONE:
	case TMF_F_W_TAG_ALREADY_FREE:
	case TMF_F_W_TC_NOT_FOUND:
		/*
		 * For SMP or STP target, firmware will only try to abort
		 * the task if it is still in its execution queue.
		 * If the task to be aborted couldn't be found, mostly like it
		 * has been issued to the target.
		 * We should try next level error recovery.
		 */

		scb_to_abort=(struct scb *)scb->io_ctx;
		asd_log(ASD_DBG_INFO, "protocol type=%x flags=%x \n",
			  scb->platform_data->targ->device_protocol_type,scb_to_abort->flags);
		if (scb->platform_data->targ->device_protocol_type
					!= ASD_DEVICE_PROTOCOL_SCSI) {
			if(!(scb_to_abort->flags & SCB_ABORT_DONE))
			{
				scb->eh_state = SCB_EH_DONE;
				scb->eh_status = SCB_EH_FAILED;
				break;
			}
		}

		/* Fall thru */
	case TASK_ABORTED_BY_ITNL_EXP:
		/* Indicate that Abort succeeded. */
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_SUCCEED;
		break;

	case TASK_F_W_NAK_RCVD:
		/* 
		 * Indicate that Abort failed.
		 * If this is a failure in issuing ABORT TMF, we need to 
		 * resume the data transmission of the task that was
		 * going to be aborted.
		 */
		if ((scb->eh_state & SCB_EH_SUSPEND_SENDQ) != 0)
			scb->eh_state = SCB_EH_RESUME_SENDQ;
		else
			scb->eh_state = SCB_EH_DONE;
		
		scb->eh_status = SCB_EH_FAILED;
		break;

	case SSP_TASK_COMP_W_RESP:
	{
		union edb 		*edb;
		struct scb 		*escb;
		struct ssp_resp_edb  	*redb;
		struct ssp_resp_iu	*riu;
		u_int			 edb_index;
		
		edb = asd_hwi_get_edb_from_dl(asd, scb, dl, &escb, &edb_index);
		if (edb == NULL) {
			asd_log(ASD_DBG_ERROR, "Invalid EDB recv for SSP "
				"comp w/response.\n");
			scb->eh_state = SCB_EH_RESUME_SENDQ;
			scb->eh_status = SCB_EH_SUCCEED;
			break;
		}

		/*
		 * Search if the aborted command still pending on the firmware
	         * queue. If so, we need to send CLEAR_NEXUS to have the 
		 * command freed and returned to us.
		 */	 
		if (!asd_hwi_check_cmd_pending(asd, scb, dl)) {
			asd_log(ASD_DBG_RUNTIME, "Aborted cmd has been "
				"completed.\n");
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_SUCCEED;
			asd_hwi_free_edb(asd, escb, edb_index);
			break;
		}

		redb = &edb->ssp_resp;
		riu = &redb->resp_frame.riu;
		if (SSP_RIU_DATAPRES(riu) == SSP_RIU_DATAPRES_RESP) {
			uint8_t	resp_code;

			resp_code = ((struct resp_data_iu *)
						&riu->data[0])->resp_code;

			/* Handle the SSP TMF response code. */
			asd_hwi_map_tmf_resp(scb, resp_code);
			
			if (scb->eh_state == SCB_EH_CLR_NXS_REQ) {
				asd_log(ASD_DBG_ERROR,
					"Tag to Clear=0x%x.\n",
					asd_be16toh(redb->tag_to_clear));
				/* 
				 * Upon receving TMF_COMPLETE, we need to send
				 * CLEAR_NEXUS SCB for tag_to_clear.
				 */
				asd_hwi_build_clear_nexus(scb, 
							  CLR_NXS_I_T_L_Q_TAG,
							  redb->tag_to_clear,
							  /*ctx*/0);
			}
		} else {
			/* 
			 * Response Data not available. Protocol error.
			 * Indicate that Abort failed. 
			 */
			scb->eh_state = SCB_EH_RESUME_SENDQ;
			scb->eh_status = SCB_EH_FAILED;
		}
		
		asd_hwi_free_edb(asd, escb, edb_index);
		break;
	}

	case TASK_F_W_OPEN_REJECT:
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case RESUME_COMPLETE:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case TASK_CLEARED:
		/* 
	 	 * Previous error recovery SCB that timed-out and
	 	 * was aborted.
	 	 * All we need to do here is just free the scb.
	 	 */ 
		asd_hwi_free_scb(asd, scb);
		return;

	default:
		asd_log(ASD_DBG_ERROR, "DL opcode not handled.\n");
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		scb->eh_status = SCB_EH_FAILED;
		break;		
	}

	asd_hwi_abort_scb(asd, (struct scb *) scb->io_ctx, scb);
}

/*
 * Function:
 *	asd_hwi_reset_lu()
 *	
 * Description:
 *	Issue a Logical Unit Reset to the end device.
 *	LU Reset can only be done for SSP end device.
 */
static void
asd_hwi_reset_lu(struct asd_softc *asd, struct scb *scb_to_reset,
		 struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_log(ASD_DBG_ERROR, "Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);

	switch (scb->eh_state) {
	case SCB_EH_INITIATED:
	{
		struct asd_target	*targ;
		struct asd_device	*dev;
		
		targ = scb_to_reset->platform_data->targ;
		dev = scb_to_reset->platform_data->dev;
		if ((targ == NULL) || (dev == NULL)) {
			/* This shouldn't happen. */
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_reset_lu(asd, scb_to_reset, scb);
			break;
		}

		/*
   		 * Only SSP end device can be issued a LU reset.
		 */
		if (targ->device_protocol_type != ASD_DEVICE_PROTOCOL_SCSI) {
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_reset_lu(asd, (struct scb *) scb_to_reset, scb);
			break;
		}

		scb->platform_data->targ = targ;
		scb->platform_data->dev = dev;
		scb->eh_state = SCB_EH_LU_RESET_REQ;
		asd_hwi_reset_lu(asd, (struct scb *) scb_to_reset, scb);	
		break;
	}

	case SCB_EH_LU_RESET_REQ:
		/* Build LUR Task Management Function. */
		asd_hwi_build_ssp_tmf(scb, scb->platform_data->targ,
				      scb->platform_data->dev->saslun,	
				      LOGICAL_UNIT_RESET_TMF);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
				    asd_hwi_reset_lu_done);
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_CLR_NXS_REQ:
		asd_hwi_build_clear_nexus(scb, CLR_NXS_I_T_L,
					  (RESUME_TX | NOT_IN_Q | SEND_Q),
					  /*ctx*/0);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
				    asd_hwi_reset_lu_done);
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_RESUME_SENDQ:
		/*
		 * If we failed to perform LU Reset, we need to resume
		 * the firmware send queue for the I_T_L.
		 */
		asd_hwi_build_clear_nexus(scb, CLR_NXS_I_T_L,
					  RESUME_TX, /*ctx*/0);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
				    asd_hwi_reset_lu_done);
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_DONE:
	{
		scb_to_reset->eh_state = scb->eh_state;
		scb_to_reset->eh_status = scb->eh_status;

		if ((scb_to_reset->eh_state == SCB_EH_DONE) &&
		    (scb_to_reset->eh_status == SCB_EH_FAILED)) {
			/*
		 	 * Failed to perform LU Reset or LU Reset is not
		 	 * supported then we shall procced with 
			 * the next level of error recovery (Device Port Reset).
			 * We need to change the scb eh_state to 
			 * SCB_EH_DEV_RESET_REQ.
		 	 */
			scb_to_reset->eh_state = SCB_EH_DEV_RESET_REQ;
			scb_to_reset->platform_data->targ->flags &= 
							~ASD_TARG_IN_RECOVERY;
		}
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}

	default:
		asd_log(ASD_DBG_ERROR, "Invalid EH State 0x%x.\n",
			scb->eh_state);
		scb_to_reset->eh_state = SCB_EH_DONE;
		scb_to_reset->eh_status = SCB_EH_FAILED;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}
}

static void
asd_hwi_reset_lu_done(struct asd_softc *asd, struct scb *scb,
		     struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	switch (dl->opcode) {
	case TASK_COMP_WO_ERR:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_SUCCEED;
		break;

	case TMF_F_W_TC_NOT_FOUND:
	case TMF_F_W_TAG_NOT_FOUND:
	case TMF_F_W_CONN_HNDL_NOT_FOUND:
	case TASK_F_W_NAK_RCVD:
		if ((scb->eh_state & SCB_EH_SUSPEND_SENDQ) != 0)
			scb->eh_state = SCB_EH_RESUME_SENDQ;
		else
			scb->eh_state = SCB_EH_DONE;
		
		scb->eh_status = SCB_EH_FAILED;
		break;

	case SSP_TASK_COMP_W_RESP:
	{	
		union edb 		*edb;
		struct scb 		*escb;
		struct ssp_resp_edb  	*redb;
		struct ssp_resp_iu	*riu;
		u_int			 edb_index;

		edb = asd_hwi_get_edb_from_dl(asd, scb, dl, &escb, &edb_index);
		if (edb == NULL) {
			asd_log(ASD_DBG_ERROR, "Invalid EDB recv for SSP "
				"comp w/response.\n");
			scb->eh_state = SCB_EH_RESUME_SENDQ;
			scb->eh_status = SCB_EH_FAILED;
			break;
		}
		
		redb = &edb->ssp_resp;
		riu = &redb->resp_frame.riu;
		if (SSP_RIU_DATAPRES(riu) == SSP_RIU_DATAPRES_RESP) {
			uint8_t	resp_code;

			resp_code = ((struct resp_data_iu *)
						&riu->data[0])->resp_code;

			/* Handle the SSP TMF response code. */
			asd_hwi_map_tmf_resp(scb, resp_code);
		} else {
			/* 
			 * Response Data not available. Protocol error.
			 * Indicate that LUR failed.
			 */
			scb->eh_state = SCB_EH_RESUME_SENDQ;
			scb->eh_status = SCB_EH_FAILED;
		}

		asd_hwi_free_edb(asd, escb, edb_index);
		break;
	}

	case TASK_F_W_OPEN_REJECT:
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case RESUME_COMPLETE:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case TASK_CLEARED:
		/* 
		 * Previous error recovery SCB that timed-out and
		 * was aborted.
		 * All we need to do here is just free the scb.
		 */ 
		asd_hwi_free_scb(asd, scb);
		return;
	
	default:
		asd_log(ASD_DBG_ERROR, "DL Opcode not handled.\n");
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		scb->eh_status = SCB_EH_FAILED;
		break;
	}

	asd_hwi_reset_lu(asd, (struct scb *) scb->io_ctx, scb);
}

void
asd_hwi_map_tmf_resp(struct scb *scb, u_int resp_code)
{
	/* 
	 * Handle TMF Response upon completion of issuing 
	 * Task Management Function to the target..
	 * Based on the response code, we will move to certain
	 * error recovery state.
	 */
	switch (resp_code) {
	case TMF_COMPLETE:
		scb->eh_state = SCB_EH_CLR_NXS_REQ;
		scb->eh_status = SCB_EH_SUCCEED;
		break;

	case INVALID_FRAME:
	case TMF_FAILED:
	case TMF_SUCCEEDED:
	case TMF_NOT_SUPPORTED:
	case INVALID_LUN:
	default:
		/* We treat all this as a failure case. */
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		scb->eh_status = SCB_EH_FAILED;
		break;
	}
}

/*
 * Function:
 *	asd_hwi_reset_device()
 *	
 * Description:
 *	Issue a device reset to the failing device.
 */
static void
asd_hwi_reset_device(struct asd_softc *asd, struct scb *scb_to_reset,
		     struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_log(ASD_DBG_ERROR, "Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);
	asd_log(ASD_DBG_ERROR, "scb_to_reset = %p \n",scb_to_reset);

	switch (scb->eh_state) {
	case SCB_EH_INITIATED:
	{
		struct asd_target	*targ;
		struct asd_device	*dev;

		targ = scb_to_reset->platform_data->targ;
		dev = scb_to_reset->platform_data->dev;
		if ((targ == NULL) || (dev == NULL)) {
			/* This shouldn't happen. */

			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_reset_device(asd, scb_to_reset, scb);
			break;
		}

		/*
	 	 * DC: Currently we are not handling error recovery for
		 *     expander.
		 *     Logic for that will be added later on.
         	 */ 
		if ((targ->device_protocol_type != ASD_DEVICE_PROTOCOL_SCSI) &&
		    (targ->device_protocol_type != ASD_DEVICE_PROTOCOL_ATA)) {
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_reset_device(asd, scb_to_reset, scb);
			break;
		}
		
		scb->platform_data->targ = targ;
		scb->platform_data->dev = dev;

		/* 
		 * Saving final post routine and the scb_to_reset in
		 * the first post stack slot. We need to access the
		 * scb_to_reset during the device reset process.
		 */
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
			    asd_hwi_reset_device_done);
		
		if (SCB_GET_SRC_PORT(scb_to_reset)->management_type == 
							ASD_DEVICE_END) {
			/* We or'ed it to remember its previous state. */

				scb->eh_state  = SCB_EH_CLR_NXS_REQ;
		} else {
			/*
			 * For device attached behind expander,
		         * prior to device reset, we will try to
			 * obtain the error report log of the phy
			 * that the device is connected to.
			 */
			scb->eh_state = SCB_EH_PHY_REPORT_REQ;
		}
		asd_hwi_reset_device(asd, scb_to_reset, scb);	
		break;
	}

	case SCB_EH_DEV_RESET_REQ:
	{
		struct asd_target	*targ;
		struct asd_port		*port;

		targ = scb->platform_data->targ;
		port = SCB_GET_SRC_PORT(scb);
		/*
	 	 * Perform a specific device reset for device that is 
		 * direct-attached or expander-attached.
	 	 */
		if (port->management_type == ASD_DEVICE_END) {
			/* Device is directly attached to the initiator. */
			asd_hwi_reset_end_device(asd, scb);
		} else {
			/* Device is attached behind expander. */
			asd_hwi_reset_exp_device(asd, scb);
		}
	    	break;
	}

	case SCB_EH_PHY_NO_OP_REQ:
	{
		struct asd_phy	*phy;

		phy = (struct asd_phy *) scb->io_ctx;
		/*
		 * Upon completion of HARD RESET, we need to initialize
		 * OOB registers to enable hot-plug timer.
		 * PHY NO OP currently only applied for direct-attached
		 * device.
		 */
		asd_hwi_build_control_phy(scb, phy, PHY_NO_OP);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, scb->io_ctx,
				    asd_hwi_reset_end_device_done);
		asd_hwi_post_scb(asd, scb);
		break;
	}

	case SCB_EH_PHY_REPORT_REQ:
		/*
		 * Obtain the report error log of the phy that the failing
		 * device is connected to.
		 * Ideally, we want to traverse the route and obtain the
		 * error report log of each phy that has pathway to the
		 * device.
		 */
		asd_hwi_report_phy_err_log(asd, scb);
		break;

	case SCB_EH_CLR_NXS_REQ:
		/*
	 	 * Prior to performing Link Reset or Hard Reset to the 
		 * target, we need to clear the firmware's execution queue
		 * and suspend the data transimission to the target.
	 	 */
		asd_hwi_build_clear_nexus(scb, CLR_NXS_IT_OR_TI,
					 (SUSPEND_TX | EXEC_Q),
					  /*ctx*/0);
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
					asd_hwi_reset_device_done);

		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_hwi_post_scb(asd, scb);
		break;
		case SCB_EH_RESUME_SENDQ:
		scb_to_reset->eh_state = SCB_EH_RESUME_SENDQ;
		scb_to_reset->eh_status =SCB_EH_SUCCEED;
		scb_to_reset->platform_data->targ->flags &= 
							~ASD_TARG_IN_RECOVERY;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;

	case SCB_EH_DONE:
		scb_to_reset->eh_state = scb->eh_state;
		scb_to_reset->eh_status = scb->eh_status;
		if(!(scb_to_reset->flags & SCB_ABORT_DONE))
		{
			// scb not retutn from sequencer then set status to fail state
			scb_to_reset->eh_status = SCB_EH_FAILED;
		}

		if ((scb_to_reset->eh_state == SCB_EH_DONE) &&
		    (scb_to_reset->eh_status == SCB_EH_FAILED)) {
			/*
		 	 * Failed to perform DEVICE RESET,
			 * we shall procced with the next level of 
			 * error recovery (Port Reset).
			 * We need to change the scb eh_state to 
			 * SCB_EH_PORT_RESET_REQ.
		 	 */
			scb_to_reset->eh_state = SCB_EH_PORT_RESET_REQ;
			scb_to_reset->platform_data->targ->flags &= 
							~ASD_TARG_IN_RECOVERY;
		}
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;

	default:
		asd_log(ASD_DBG_ERROR, "Invalid EH State 0x%x.\n",
			scb->eh_state);

		scb_to_reset->eh_state = SCB_EH_DONE;
		scb_to_reset->eh_status = SCB_EH_FAILED;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}
}

/*
 * Function:
 *	asd_hwi_reset_device()
 *	
 * Description:
 *	Issue a device reset to the failing device.
 */
static void
asd_hwi_resume_sendq(struct asd_softc *asd, struct scb *scb_to_reset,
		     struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_log(ASD_DBG_ERROR, "Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);
	asd_log(ASD_DBG_ERROR, "scb_to_reset = %p \n",scb_to_reset);

	switch (scb->eh_state) {
	case SCB_EH_INITIATED:
	{
		struct asd_target	*targ;
		struct asd_device	*dev;

		targ = scb_to_reset->platform_data->targ;
		dev = scb_to_reset->platform_data->dev;
		if ((targ == NULL) || (dev == NULL)) {
			/* This shouldn't happen. */

			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_resume_sendq(asd, scb_to_reset, scb);
			break;
		}

		/*
	 	 * DC: Currently we are not handling error recovery for
		 *     expander.
		 *     Logic for that will be added later on.
         	 */ 
		if ((targ->device_protocol_type != ASD_DEVICE_PROTOCOL_SCSI) &&
		    (targ->device_protocol_type != ASD_DEVICE_PROTOCOL_ATA)) {
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_resume_sendq(asd, scb_to_reset, scb);
			break;
		}
		
		scb->platform_data->targ = targ;
		scb->platform_data->dev = dev;

		/* 
		 * Saving final post routine and the scb_to_reset in
		 * the first post stack slot. We need to access the
		 * scb_to_reset during the device reset process.
		 */
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
			    asd_hwi_reset_device_done);
		
		scb->eh_state = SCB_EH_RESUME_SENDQ;
		asd_hwi_resume_sendq(asd, scb_to_reset, scb);	
		break;
	}

		case SCB_EH_RESUME_SENDQ:
	{

	
		/* 
		 * Upon completion of Device Reset, we need to issue
		 * CLEAR NEXUS to the firmware to free up the SCB
		     * resume data transmission.
		 * Also, we will be using the first post stack
		 * we save at the beginning.
		 */
		asd_hwi_build_clear_nexus(scb, CLR_NXS_IT_OR_TI,
					 (RESUME_TX|SEND_Q|NOT_IN_Q),
					  /*ctx*/0);

		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
					asd_hwi_resume_sendq_done);

		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_hwi_post_scb(asd, scb);
		break;
	}
	
	case SCB_EH_DONE:
		scb_to_reset->eh_state = scb->eh_state;
		scb_to_reset->eh_status = scb->eh_status;


		if ((scb_to_reset->eh_state == SCB_EH_DONE) &&
		    (scb_to_reset->eh_status == SCB_EH_FAILED)) {
			/*
		 	 * Failed to perform DEVICE RESET,
			 * we shall procced with the next level of 
			 * error recovery (Port Reset).
			 * We need to change the scb eh_state to 
			 * SCB_EH_PORT_RESET_REQ.
		 	 */
			scb_to_reset->eh_state = SCB_EH_PORT_RESET_REQ;
			scb_to_reset->platform_data->targ->flags &= 
						~ASD_TARG_IN_RECOVERY;
		}
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;

	default:
		asd_log(ASD_DBG_ERROR, "Invalid EH State 0x%x.\n",
			scb->eh_state);

		scb_to_reset->eh_state = SCB_EH_DONE;
		scb_to_reset->eh_status = SCB_EH_FAILED;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}
}
static void
asd_hwi_resume_sendq_done(struct asd_softc *asd, struct scb *scb,
			  struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	switch (dl->opcode) {
	case TASK_COMP_WO_ERR:
	{	
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_SUCCEED;
		break;
	}

	case TMF_F_W_TC_NOT_FOUND:
	case TMF_F_W_TAG_NOT_FOUND:
	case TMF_F_W_CONN_HNDL_NOT_FOUND:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case TASK_CLEARED:
		/* 
		 * Previous error recovery SCB that timed-out and
		 * was aborted.
		 * All we need to do here is just free the scb.
		 */ 
		asd_log(ASD_DBG_ERROR, "task clear free scb=%p \n", scb);
		asd_hwi_free_scb(asd, scb);
		return;

	default:
		asd_log(ASD_DBG_ERROR, "Unhandled DL opcode.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;
	}

	asd_hwi_resume_sendq(asd, (struct scb *) scb->io_ctx, scb);
}
static void
asd_hwi_reset_device_done(struct asd_softc *asd, struct scb *scb,
			  struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	switch (dl->opcode) {
	case TASK_COMP_WO_ERR:
	{	
			if(scb->eh_state==SCB_EH_CLR_NXS_REQ)
			{
			struct asd_port	*port;
		
			port = SCB_GET_SRC_PORT(scb);
			if (port->management_type == ASD_DEVICE_END) {
				/*
		 	 	 * For direct-attached SSP target, we need to
				 * check if it is attached on a wide port. 
				 * If so, we need to issue a HARD RESET on 
				 * one of the phys and LINK RESET on the 
				 * remaining phys.
			 	 * For direct-attached SATA/SATAPI target,
				 * LINK RESET can be done for all the phys
				 * belong to the port.
				 * Get which phy(s) that need to be reset.
		 	 	 */
				port->reset_mask = port->conn_mask;
			}
			scb->eh_state = SCB_EH_DEV_RESET_REQ;
		} else {
			scb->eh_state = SCB_EH_DONE;
		}
		scb->eh_status = SCB_EH_SUCCEED;
		break;
	}

	case TMF_F_W_TC_NOT_FOUND:
	case TMF_F_W_TAG_NOT_FOUND:
	case TMF_F_W_CONN_HNDL_NOT_FOUND:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;

	case TASK_CLEARED:
		/* 
		 * Previous error recovery SCB that timed-out and
		 * was aborted.
		 * All we need to do here is just free the scb.
		 */ 
		asd_log(ASD_DBG_ERROR, "task clear free scb=%p \n", scb);
		asd_hwi_free_scb(asd, scb);
		return;

	default:
		asd_log(ASD_DBG_ERROR, "Unhandled DL opcode.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;
	}

	asd_hwi_reset_device(asd, (struct scb *) scb->io_ctx, scb);
}

/*
 * Function:
 *	asd_hwi_reset_end_device()
 *	
 * Description:
 *	For direct-attached SATA/SATAPI target, device reset is achieved by 
 *	issuing a Link Reset sequence (OOB).
 *	For direct-attached SSP target, device reset is achieved by issuing
 *	a Hard Reset.
 */
static void
asd_hwi_reset_end_device(struct asd_softc *asd, struct scb *scb)
{
	struct asd_target	*targ;
	struct asd_port		*port;
	struct asd_phy		*phy;
	int			 found;

	targ = scb->platform_data->targ;
	port = SCB_GET_SRC_PORT(scb);
	found = 0;
	/*
	 * Find the associated phy that need to be reset.
	 */
	list_for_each_entry(phy, &port->phys_attached, links) {
		if ((port->reset_mask & (1 << phy->id)) != 0) {
			found = 1;
			break;
		}
	}
	if (found != 1) {
		/* This shouldn't happen. */
		asd_log(ASD_DBG_ERROR,"No PHY to reset, PR Mask: 0x%x "
			"PC Mask: 0x%x.\n", port->reset_mask, port->conn_mask);
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		asd_hwi_reset_device(asd,
				    (struct scb *) scb->post_stack[0].io_ctx,
				     scb);
		return;
	}

	if (targ->transport_type == ASD_TRANSPORT_SSP) {
		/* 
	 	 * For SSP wide port target, we need to perform a
	 	 * HARD RESET only on one of the phys and LINK RESET
		 * on the remaining phys.
	 	 */
		asd_hwi_build_control_phy(scb, phy,
					 ((port->reset_mask == port->conn_mask)
					  ? EXECUTE_HARD_RESET : ENABLE_PHY));
	} else {
		/* 
	 	 * For SATA direct-attached end device, 
	 	 * device port reset is done by re-Enabling the phy
	 	 * and hence initiating OOB sequence.
	 	 */
		asd_hwi_build_control_phy(scb, phy, ENABLE_PHY); 	
	}
		
	port->reset_mask &= ~(1 << phy->id);
	scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
	asd_setup_scb_timer(scb, (8 * HZ), asd_scb_eh_timeout);
	asd_push_post_stack(asd, scb, (void *) phy,
			    asd_hwi_reset_end_device_done);
	asd_hwi_post_scb(asd, scb);
}

static void
asd_hwi_reset_end_device_done(struct asd_softc *asd, struct scb *scb, 
			      struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	if (scb->eh_status == SCB_EH_SUCCEED) {
		if (scb->eh_state != SCB_EH_PHY_NO_OP_REQ) {
			/* Check if any remaining phys need to be reset. */
			scb->eh_state =
				((SCB_GET_SRC_PORT(scb)->reset_mask == 0) ?
				  SCB_EH_CLR_NXS_REQ : SCB_EH_DEV_RESET_REQ);
		}
	} else {
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
	}
		
	asd_hwi_reset_device(asd, (struct scb *) scb->post_stack[0].io_ctx,
			     scb);
}

void dumpsmp(uint8_t	*smp_req)
{
	uint32_t index;
	if(smp_req==NULL) return;

	asd_print("\nsmp_req: 0x%x", *smp_req );

	for( index = 0; index < sizeof(struct SMPRequest); index++)
	{
		if(!(index % 16))
		{
			asd_print("   \n");
		}

		asd_print("%02x ", smp_req[index] );
	}
}

static void
asd_hwi_report_phy_err_log(struct asd_softc *asd, struct scb *scb)
{
	struct asd_port		*port;
	struct asd_target	*exp_targ;
	struct asd_target	*targ;
	struct Discover		*disc;
	int			 phy_id;

	targ = scb->platform_data->targ;
	/* Expander that the target is attached to. */
	exp_targ = targ->parent;
	port = SCB_GET_SRC_PORT(scb);

	if (exp_targ == NULL) {
		/* This shouldn't happen. */
		asd_log(ASD_DBG_ERROR, "Parent Expander shouldn't be NULL.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		asd_hwi_reset_device(asd,
				    (struct scb *) scb->post_stack[0].io_ctx,
				     scb);
		return;
	}
	/*
	 * Find the expander phy id that the target is attached to. 
	 */
	for (phy_id = 0; phy_id < exp_targ->num_phys; phy_id++) {
		disc = &(exp_targ->Phy[phy_id].Result);

		if (SAS_ISEQUAL(targ->ddb_profile.sas_addr,
				disc->AttachedSASAddress))
			break;
	}

	if (phy_id == exp_targ->num_phys) {
		/* This shouldn't happen. */
		asd_log(ASD_DBG_ERROR, "Corrupted target, inv. phy id.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		asd_hwi_reset_device(asd,
				    (struct scb *) scb->post_stack[0].io_ctx,
				     scb);
		return;
	}

	/* Build a REPORT PHY ERROR LOG SMP request. */
	asd_hwi_build_smp_phy_req(port, REPORT_PHY_ERROR_LOG, phy_id, 0);

	/* Build a SMP TASK. */
	asd_hwi_build_smp_task(scb, exp_targ,
			       port->dc.SMPRequestBusAddr,
			       sizeof(struct SMPRequestPhyInput),
			       port->dc.SMPResponseBusAddr,
			       sizeof(struct SMPResponseReportPhyErrorLog));

	scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
	asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
#ifdef ASD_DEBUG
	dumpsmp((uint8_t *)port->dc.SMPRequestFrame);	
#endif	
	asd_push_post_stack(asd, scb, (void *) port,
			    asd_hwi_reset_exp_device_done);
	asd_hwi_post_scb(asd, scb);
}

/*
 * Function:
 *	asd_hwi_reset_exp_device()
 *	
 * Description:
 * 	For device that is attached behind an expander device, device reset
 *	is achieved by issuing SMP PHY CONTROL with a phy operation of:
 *	- HARD RESET for SSP and STP device port.
 *	- LINK RESET for SATA/SATAPI device port.
 */
static void
asd_hwi_reset_exp_device(struct asd_softc *asd, struct scb *scb)
{
	struct asd_target	*exp_targ;
	struct asd_target	*targ;
	struct asd_port		*port;
	struct Discover		*disc;
	int			 phy_id;

	port = SCB_GET_SRC_PORT(scb);
	targ = scb->platform_data->targ;
	/* Expander that the target is attached to. */
	exp_targ = targ->parent;
	if (exp_targ == NULL) {
		/* This shouldn't happen. */
		asd_log(ASD_DBG_ERROR, "Parent Expander shouldn't be NULL.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		asd_hwi_reset_device(asd,
				    (struct scb *) scb->post_stack[0].io_ctx,
				     scb);
		return;
	}

	/*
	 * Find the expander phy id that the target is attached to. 
	 */
	for (phy_id = 0; phy_id < exp_targ->num_phys; phy_id++) {
		disc = &(exp_targ->Phy[phy_id].Result);

		if (SAS_ISEQUAL(targ->ddb_profile.sas_addr,
				disc->AttachedSASAddress))
			break;
	}

	if (phy_id == exp_targ->num_phys) {
		/* This shouldn't happen. */
		asd_log(ASD_DBG_ERROR, "Corrupted target, inv. phy id.\n");
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		asd_hwi_reset_device(asd,
				    (struct scb *) scb->post_stack[0].io_ctx,
				     scb);
		return;
	}

	/* 
	 * For SSP/STP target port, CONTROL PHY-HARD RESET will be issued.
	 * For SATA/SATAPI target port, CONTROL PHY-LINK RESET will be issued.
	 */
	asd_hwi_build_smp_phy_req(
		port, PHY_CONTROL, phy_id,
		((targ->transport_type == ASD_TRANSPORT_ATA
		 || targ->transport_type ==ASD_TRANSPORT_STP) ? LINK_RESET :
		 					       HARD_RESET));

	/* Build a SMP REQUEST. */
	asd_hwi_build_smp_task(scb, exp_targ,
			       port->dc.SMPRequestBusAddr,
			       sizeof(struct SMPRequestPhyControl),
			       port->dc.SMPResponseBusAddr,
			       sizeof(struct SMPResponsePhyControl));

	scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
	asd_setup_scb_timer(scb, (8 * HZ), asd_scb_eh_timeout);

#ifdef ASD_DEBUG
	dumpsmp((uint8_t *)port->dc.SMPRequestFrame);	
#endif	
	asd_push_post_stack(asd, scb, (void *) port,
			    asd_hwi_reset_exp_device_done);
	asd_hwi_post_scb(asd, scb);
}

static void
asd_hwi_reset_exp_device_done(struct asd_softc *asd, struct scb *scb, 
			      struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);
	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	switch (dl->opcode) {
	case TASK_COMP_WO_ERR:
	{	
		struct asd_port	*port;

		port = (struct asd_port *) scb->io_ctx;
		/* Check the SMP Response function result. */
		if (port->dc.SMPResponseFrame->FunctionResult ==
			SMP_FUNCTION_ACCEPTED) {
			if (scb->eh_state == SCB_EH_PHY_REPORT_REQ) {
				asd_hwi_dump_phy_err_log(port, scb);
				// do the first time clear nexus after PHY report error
				scb->eh_state = SCB_EH_CLR_NXS_REQ;
			} else {
				scb->eh_state = SCB_EH_RESUME_SENDQ;

			}
			scb->eh_status = SCB_EH_SUCCEED;
		} else {
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
		}
		break;
	}

	case TASK_CLEARED:
		/* 
		 * Previous error recovery SCB that timed-out and
		 * was aborted.
		 * All we need to do here is just free the scb.
		 */ 
		asd_hwi_free_scb(asd, scb);
		return;

	case TASK_F_W_SMPRSP_TO:
	case TASK_F_W_SMP_XMTRCV_ERR:
	default:
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
		break;
	}

	asd_hwi_reset_device(asd, (struct scb *) scb->post_stack[0].io_ctx,
			     scb);
}

static void
asd_hwi_dump_phy_err_log(struct asd_port *port, struct scb *scb)
{
	struct SMPResponseReportPhyErrorLog 	*report_phy_log;
	struct asd_target			*targ;
	struct asd_device			*dev;

	report_phy_log = (struct SMPResponseReportPhyErrorLog *)
				&(port->dc.SMPResponseFrame->Response);
	targ = scb->platform_data->targ;
	dev = scb->platform_data->dev;

	asd_print("REPORT PHY ERROR LOG\n");
	asd_print("---------------------\n");
	asd_print("Phy #%d of Expander 0x%llx.\n",
		  report_phy_log->PhyIdentifier, asd_be64toh(
		  *((uint64_t *) targ->parent->ddb_profile.sas_addr)));
	asd_print("Attached device:\n"); 
	asd_print("Scsi %d Ch %d Tgt %d Lun %d, SAS Addr: 0x%llx.\n",
		  targ->softc->platform_data->scsi_host->host_no,
		  dev->ch, dev->id, dev->lun, asd_be64toh(
		  *((uint64_t *) targ->ddb_profile.sas_addr)));
	asd_print("\nPHY ERROR COUNTS\n");
	asd_print("----------------\n");
	asd_print("Invalid Dword Count: %d.\n",
		  report_phy_log->InvalidDuint16_tCount);
	asd_print("Disparity Error Count: %d.\n",
		  report_phy_log->DisparityErrorCount);
	asd_print("Loss of Dword Synchronization Count: %d.\n",
		  report_phy_log->LossOfDuint16_tSynchronizationCount);
	asd_print("Phy Reset Problem Count: %d.\n",
		  report_phy_log->PhyResetProblemCount);
}

/*
 * Function:
 *	asd_hwi_reset_port()
 *	
 * Description:
 *	Issue a HARD/LINK RESET to the failing port.
 */
static void
asd_hwi_reset_port(struct asd_softc *asd, struct scb *scb_to_reset,
		   struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_log(ASD_DBG_ERROR, "Curr State: 0x%x Status: 0x%x.\n",
		scb->eh_state, scb->eh_status);
	asd_log(ASD_DBG_ERROR, "reset port scb=%p scb_to_reset = %p\n",scb,scb_to_reset);

	switch (scb->eh_state) {
	case SCB_EH_INITIATED:
	{
		struct asd_port		*port;
		struct asd_target	*targ;
		
		port = SCB_GET_SRC_PORT(scb_to_reset);
		if (port == NULL) {
			asd_log(ASD_DBG_ERROR," Invalid port to reset.\n");
			scb->eh_state = SCB_EH_DONE;
			scb->eh_state = SCB_EH_FAILED;
			asd_hwi_reset_port(asd, scb_to_reset, scb);
			break;
		}
		/* 
		 * Freeze all the targets' queue attached to the port that
		 * we are about to reset.
		 */
		list_for_each_entry(targ, &port->targets, all_domain_targets) {
	
			targ->qfrozen++;
		}

		/* 
		 * Saving final post routine and the scb_to_reset in
		 * the first post stack slot. We need to access the
		 * scb_to_reset during the device reset process.
		 */
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
				    asd_hwi_reset_port_done);
		
		/* Bitmask of the phys to perform reset. */
		port->reset_mask = port->conn_mask;
		scb->platform_data->targ = scb_to_reset->platform_data->targ;
		scb->platform_data->dev = scb_to_reset->platform_data->dev;
		scb->eh_state = SCB_EH_CLR_NXS_REQ;
		asd_hwi_reset_port(asd, scb_to_reset, scb);
		break;
	}
	
	case SCB_EH_CLR_NXS_REQ:
		asd_hwi_build_clear_nexus(scb, CLR_NXS_I_OR_T,
				  	  /*parm*/0, /*ctx*/0);
		asd_push_post_stack(asd, scb, (void *) scb_to_reset,
				    asd_hwi_reset_port_done);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_hwi_post_scb(asd, scb);
		break;

	case SCB_EH_PORT_RESET_REQ:
	{
		struct asd_port	*port;
		struct asd_phy	*phy;
		int		 found;

		found = 0;
		port = SCB_GET_SRC_PORT(scb_to_reset);
		list_for_each_entry(phy, &port->phys_attached, links) {
			if ((port->reset_mask & (1 << phy->id)) != 0) {
				found = 1;
				break;
			}
		}
		if (found != 1) {
			/* This shouldn't happen. */
			asd_log(ASD_DBG_ERROR,"No PHY to reset, PR Mask: 0x%x "
				"PC Mask: 0x%x.\n",
				port->reset_mask, port->conn_mask);
			scb->eh_state = SCB_EH_DONE;
			scb->eh_status = SCB_EH_FAILED;
			asd_hwi_reset_port(asd, scb_to_reset, scb);
			break;
		}
		/* 
	 	 * For SSP initiator wide port, we need to perform a
		 * HARD RESET only on one of the phys and LINK RESET on
		 * the remaining phys.
	 	 */
		asd_hwi_build_control_phy(scb, phy,
					 ((port->reset_mask == port->conn_mask)
					  ? EXECUTE_HARD_RESET : ENABLE_PHY));

		port->reset_mask &= ~(1 << phy->id);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (8 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, (void *) phy,
				    asd_hwi_reset_port_done);
		asd_hwi_post_scb(asd, scb);
		break;
	}

	case SCB_EH_PHY_NO_OP_REQ:
	{
		struct asd_phy	*phy;

		phy = (struct asd_phy *) scb->io_ctx;
		/*
		 * Upon completion of HARD RESET, we need to initialize
		 * OOB registers to enable hot-plug timer.
		 */
		asd_hwi_build_control_phy(scb, phy, PHY_NO_OP);
		scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
		asd_setup_scb_timer(scb, (4 * HZ), asd_scb_eh_timeout);
		asd_push_post_stack(asd, scb, scb->io_ctx,
				    asd_hwi_reset_port_done);
		asd_hwi_post_scb(asd, scb);
		break;
	}

	case SCB_EH_DONE:
	{
		struct asd_port		*port;
		struct asd_target	*targ;

		port = SCB_GET_SRC_PORT(scb_to_reset);
		
		/* Unfreeze all the targets. */
		list_for_each_entry(targ, &port->targets, all_domain_targets) {
			targ->qfrozen--;
			asd_log(ASD_DBG_INFO,"clear freezen ptr=%p num=%d\n",targ,targ->qfrozen);
		}
		scb_to_reset->eh_state = scb->eh_state;
		scb_to_reset->eh_status = scb->eh_status;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}

	default:
		asd_log(ASD_DBG_ERROR, "Invalid State.\n");
		scb_to_reset->eh_state = SCB_EH_DONE;
		scb_to_reset->eh_status = SCB_EH_FAILED;
		asd_hwi_free_scb(asd, scb);
		asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		break;
	}
}

static void
asd_hwi_reset_port_done(struct asd_softc *asd, struct scb *scb, 
			struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	/*
       	 * There is a possibility that this post routine is called after
	 * the SCB timedout. So, only delete the timer if the SCB hasn't
	 * timedout.
	 */
	if (scb->eh_state != SCB_EH_TIMEDOUT)
		del_timer_sync(&scb->platform_data->timeout);

	/* CLR NXS completion. */
	if (dl->opcode == TASK_COMP_WO_ERR) {
		scb->eh_state = SCB_EH_PORT_RESET_REQ;
	} else if (scb->eh_status == SCB_EH_SUCCEED) {
		if (scb->eh_state != SCB_EH_PHY_NO_OP_REQ) {
			/* Check if any remaining phys need to be reset. */
			scb->eh_state =
				((SCB_GET_SRC_PORT(scb)->reset_mask == 0) ?
				  SCB_EH_DONE : SCB_EH_PORT_RESET_REQ);
		}
	} else {
		scb->eh_state = SCB_EH_DONE;
		scb->eh_status = SCB_EH_FAILED;
	}
		
	asd_hwi_reset_port(asd,
			  (struct scb *) scb->post_stack[0].io_ctx,
			   scb);
}

/*************************** NVRAM access utilities ***************************/

#if NVRAM_SUPPORT


/*
 * Function:
 *	asd_hwi_poll_nvram()
 *
 * Description:
 *	This routine will poll for the NVRAM to be ready to accept new
 *	command.
 */
static int
asd_hwi_poll_nvram(struct asd_softc *asd)
{
	uint8_t	nv_data;
	uint8_t	toggle_data;
	int loop_cnt;

	loop_cnt = 5000;

	while (loop_cnt) {
		nv_data = asd_hwi_swb_read_byte(asd, 
						asd->hw_profile.nv_flash_bar);

		toggle_data = (nv_data ^ asd_hwi_swb_read_byte(asd,
						asd->hw_profile.nv_flash_bar));

		if (toggle_data == 0) {
			return (0);
		} else {
	  		if (((toggle_data == 0x04) && ((loop_cnt - 1) == 0)) ||
			   ((toggle_data & 0x40) && (toggle_data & 0x20))) {
				return (-1);
			}
		}
	
		loop_cnt--;
		asd_delay(ASD_DELAY_COUNT);
	}
	return (-1);
}

static int
asd_hwi_chk_write_status(struct asd_softc *asd, uint32_t sector_addr, 
			uint8_t erase_flag) 
{
	uint32_t read_addr;
	uint32_t loop_cnt;
	uint8_t	nv_data1, nv_data2;
	uint8_t	toggle_bit1/*, toggle_bit2*/;

	/* 
	 * Read from DQ2 requires sector address 
	 * while it's dont care for DQ6 
	 */
	/* read_addr = asd->hw_profile.nv_flash_bar + sector_addr;*/
	read_addr = asd->hw_profile.nv_flash_bar;
	loop_cnt = 50000;

	while (loop_cnt) {
		nv_data1 = asd_hwi_swb_read_byte(asd, read_addr); 
		nv_data2 = asd_hwi_swb_read_byte(asd, read_addr); 

		toggle_bit1 = ((nv_data1 & FLASH_STATUS_BIT_MASK_DQ6)
				 ^ (nv_data2 & FLASH_STATUS_BIT_MASK_DQ6));
		/* toggle_bit2 = ((nv_data1 & FLASH_STATUS_BIT_MASK_DQ2) 
				^ (nv_data2 & FLASH_STATUS_BIT_MASK_DQ2));*/

		if (toggle_bit1 == 0) {
			return (0);
		} else {
			if (nv_data2 & FLASH_STATUS_BIT_MASK_DQ5) {
				nv_data1 = asd_hwi_swb_read_byte(asd, 
								read_addr); 
				nv_data2 = asd_hwi_swb_read_byte(asd,
								read_addr); 
				toggle_bit1 = 
				((nv_data1 & FLASH_STATUS_BIT_MASK_DQ6) 
				^ (nv_data2 & FLASH_STATUS_BIT_MASK_DQ6));
				/*
				toggle_bit2 = 
				   ((nv_data1 & FLASH_STATUS_BIT_MASK_DQ2) 
				   ^ (nv_data2 & FLASH_STATUS_BIT_MASK_DQ2));
				*/
				if (toggle_bit1 == 0) {
					return 0;
				}
			}
		}
		loop_cnt--;
	
		/* 
		 * ERASE is a sector-by-sector operation and requires
		 * more time to finish while WRITE is byte-byte-byte
		 * operation and takes lesser time to finish. 
		 *
		 * For some strange reason a reduced ERASE delay gives different
		 * behaviour across different spirit boards. Hence we set
		 * a optimum balance of 50mus for ERASE which works well
		 * across all boards.
		 */ 
		if (erase_flag) {
			asd_delay(FLASH_STATUS_ERASE_DELAY_COUNT);
		} else {
			asd_delay(FLASH_STATUS_WRITE_DELAY_COUNT);
		}
	}
	return (-1);
}
/*
 * Function:
 *	asd_hwi_reset_nvram()
 *
 * Description:
 *	Reset the NVRAM section.
 */
static int
asd_hwi_reset_nvram(struct asd_softc *asd)
{
	/* Poll till NVRAM is ready for new command. */
	if (asd_hwi_poll_nvram(asd) != 0)
		return (-1);

	asd_hwi_swb_write_byte(asd, asd->hw_profile.nv_flash_bar, NVRAM_RESET);

	/* Poll if the command is successfully written. */
	if (asd_hwi_poll_nvram(asd) != 0)
		return (-1);	

	return (0);
}

/*
 * Function:
 * 	asd_hwi_search_nv_cookie()
 *
 * Description:
 *	Search the cookie in NVRAM.
 *	If found, return the address offset of the cookie.
 */
int
asd_hwi_search_nv_cookie(struct asd_softc *asd, uint32_t *addr,
			struct asd_flash_dir_layout *pflash_dir_buf)
{
	struct asd_flash_dir_layout flash_dir;
	uint8_t		 cookie_to_find[32]="*** ADAPTEC FLASH DIRECTORY *** ";
	void		*dest_buf;
	uint32_t	 nv_addr;
	int		 cookie_found;
	u_int		 bytes_read;

	memset(&flash_dir, 0x0, sizeof(flash_dir));
	dest_buf = &flash_dir;
	cookie_found = 0;
	nv_addr = 0;
	while (nv_addr < NVRAM_MAX_BASE_ADR) {
		if (asd_hwi_read_nv_segment(asd, NVRAM_NO_SEGMENT_ID,
					    dest_buf, nv_addr,
					    sizeof(flash_dir), 
					    &bytes_read)
					    != 0)
			return (-1);

		if (memcmp(flash_dir.cookie,
			   &cookie_to_find[0],
			   NVRAM_COOKIE_SIZE) == 0) {
			cookie_found = 1;
			if (pflash_dir_buf != NULL) {
				memcpy(pflash_dir_buf, &flash_dir, 
					sizeof(flash_dir));
			}
			break;
		}

		nv_addr += NVRAM_NEXT_ENTRY_OFFSET;
	}
	if (cookie_found == 0) {
		return (-1);
	}
	
	*addr = nv_addr;
	asd->hw_profile.nv_cookie_addr = nv_addr;
	asd->hw_profile.nv_cookie_found = 1;

	return (0);
}

/*
 * Function:
 * 	asd_hwi_search_nv_segment()
 *
 * Description:
 *	Search the requested NVRAM segment.
 *	If exists, the segment offset, attributes, pad_size and image_size
 *	will be returned.
 */
int
asd_hwi_search_nv_segment(struct asd_softc *asd, u_int segment_id,
			  uint32_t *offset, uint32_t *pad_size,
			  uint32_t *image_size, uint32_t *attr)
{
	struct asd_flash_dir_layout 	flash_dir;
	struct asd_fd_entry_layout 	fd_entry;
	uint32_t			nv_addr;
	int				segment_found;
	u_int				bytes_read;
	u_int				i;

	/*
	 * Check if we have NVRAM base addr for the FLASH directory layout.
	 */
	if (asd->hw_profile.nv_cookie_found != 1) {
		if (asd_hwi_search_nv_cookie(asd, &nv_addr,
					&flash_dir) != 0) {
			asd_log(ASD_DBG_ERROR, "Failed to search NVRAM "
				"cookie.\n");
			return (-1);
		}
	} else {
	    nv_addr = asd->hw_profile.nv_cookie_addr;
	}
		    
	nv_addr += NVRAM_FIRST_DIR_ENTRY;
	memset(&fd_entry, 0x0, sizeof(struct asd_fd_entry_layout));
	segment_found = 0;

	for (i = 0; i < NVRAM_MAX_ENTRIES; i++) {
		if (asd_hwi_read_nv_segment(asd, NVRAM_NO_SEGMENT_ID,
					&fd_entry, nv_addr,
					sizeof(struct asd_fd_entry_layout),
					&bytes_read) != 0) {
			return (-1);
		}

		if ((fd_entry.attr & FD_ENTRYTYPE_CODE) == segment_id) {
			segment_found = 1;
			break;
		}

		nv_addr += sizeof(struct asd_fd_entry_layout);
	}

	if (segment_found == 0) {
		return (-1);
	}

	*offset = fd_entry.offset;
	*pad_size = fd_entry.pad_size;
	*attr = ((fd_entry.attr & FD_SEGMENT_ATTR) ? 1 : 0);

	if ((segment_id != NVRAM_CTRL_A_SETTING) &&
		(segment_id != NVRAM_MANUF_TYPE)) {
		*image_size = fd_entry.image_size;
	} else {
		*image_size = NVRAM_INVALID_SIZE;
	}

	return (0);
}

/*
 * Function:
 *	asd_hwi_read_nv_segment()
 *
 * Description:
 *	Retrieves data from an NVRAM segment.
 */
int
asd_hwi_read_nv_segment(struct asd_softc *asd, uint32_t segment_id, void *dest,
			uint32_t src_offset, uint32_t bytes_to_read,
			uint32_t *bytes_read)
{
    	uint8_t		*dest_buf;
    	uint32_t	 nv_offset;
	uint32_t	 pad_size, image_size, attr;
	uint32_t	 i;

	/* Reset the NVRAM. */
	if (asd_hwi_reset_nvram(asd) != 0)
		return (-1);

	nv_offset = 0;
	if (segment_id != NVRAM_NO_SEGMENT_ID) {
		if (asd_hwi_search_nv_segment(asd, segment_id, &nv_offset,
					      &pad_size, &image_size,
					      &attr) != 0) {
			return (-1);
		}
	}
	nv_offset = asd->hw_profile.nv_flash_bar + nv_offset + src_offset;

	dest_buf = (uint8_t *) dest;
	*bytes_read = 0;

	if (asd_hwi_reset_nvram(asd) != 0)
		return (-1);

	for (i = 0; i < bytes_to_read; i++) {
		*(dest_buf + i) = asd_hwi_swb_read_byte(asd, (nv_offset + i));
		if (asd_hwi_poll_nvram(asd) != 0) {
			return (-1);
		}
	}
	*bytes_read = i;

	return (0); 
}

/*
 * Function:
 *	asd_hwi_write_nv_segment()
 *
 * Description:
 *	Writes data to an NVRAM segment.
 */
int
asd_hwi_write_nv_segment(struct asd_softc *asd, void *src, uint32_t segment_id, 
			uint32_t dest_offset, uint32_t bytes_to_write)
{
    	uint8_t	*src_buf;
	uint32_t nv_offset, nv_flash_bar, i;
	uint32_t pad_size, image_size, attr;

	nv_flash_bar = asd->hw_profile.nv_flash_bar;
	src_buf = NULL;
	nv_offset = 0;
	attr = 0;
	pad_size = 0;
	image_size = 0;

	if (asd_hwi_reset_nvram(asd) != 0) {
		return (-1);
	}
	if (segment_id != NVRAM_NO_SEGMENT_ID) {
		if (asd_hwi_search_nv_segment(asd, segment_id, &nv_offset, 
				&pad_size, &image_size, &attr) != 0) {
			return (-1);
		}
		if ((bytes_to_write > pad_size) || (dest_offset != 0)) {
			return (-1);
		}
	}

	nv_offset += dest_offset;
	if (segment_id == NVRAM_NO_SEGMENT_ID 
	   || attr == NVRAM_SEGMENT_ATTR_ERASEWRITE) {
		if (asd_hwi_erase_nv_sector(asd, nv_offset) != 0) {
			printk("<1>adp94xx: Erase failed at offset:0x%x\n",
				nv_offset);
			return (-1);
		}
	}

	if (asd_hwi_reset_nvram(asd) != 0) {
		return (-1);
	}
	src_buf = (uint8_t *)src;
	for (i = 0; i < bytes_to_write; i++) {
		/* Setup program command sequence */
		switch (asd->hw_profile.flash_method) {
		case FLASH_METHOD_A:
		{
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0xAAA), 0xAA);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0x555), 0x55);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0xAAA), 0xA0);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + nv_offset + i),
					(*(src_buf + i)));
			break;
		}
		case FLASH_METHOD_B:
		{
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0x555), 0xAA);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0x2AA), 0x55);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + 0x555), 0xA0);
			asd_hwi_swb_write_byte(asd, 
					(nv_flash_bar + nv_offset + i),
					(*(src_buf + i)));
			break;
		}
		default:
			break;
		}
		if (asd_hwi_chk_write_status(asd, (nv_offset + i), 
			0 /* WRITE operation */) != 0) {
			printk("<1>adp94xx: Write failed at offset:0x%x\n",
				nv_flash_bar + nv_offset + i);
			return -1;
		}
	}

	if (asd_hwi_reset_nvram(asd) != 0) {
		return (-1);
	}
	return (0);
}

/*
 * Function:
 *	asd_hwi_erase_nv_sector()
 *
 * Description:
 *	Erase the requested NVRAM sector.
 */
static int
asd_hwi_erase_nv_sector(struct asd_softc *asd, uint32_t sector_addr) 
{
	uint32_t nv_flash_bar;
	
	nv_flash_bar = asd->hw_profile.nv_flash_bar; 
	if (asd_hwi_poll_nvram(asd) != 0) {
		return (-1);
	}
	/*
	 * Erasing an NVRAM sector needs to be done in six consecutive
	 * write cyles.
	 */
	switch (asd->hw_profile.flash_method) {

	case FLASH_METHOD_A:
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0xAAA), 0xAA);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0x55);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0xAAA), 0x80);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0xAAA), 0xAA);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0x55);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + sector_addr), 0x30);
		break;

	case FLASH_METHOD_B:
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0xAA);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x2AA), 0x55);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0x80);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0xAA);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x2AA), 0x55);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + sector_addr), 0x30);
		break;

	default:
		break;
	}

	if (asd_hwi_chk_write_status(asd, sector_addr, 
				1 /* ERASE operation */) != 0) {
		return (-1);
	}

	return (0);
}

/*
 * Function:
 * 	asd_hwi_verify_nv_checksum()
 *	
 * Description:
 *	Verify if the checksum for particular NV segment is correct.
 */		 	  
static int
asd_hwi_verify_nv_checksum(struct asd_softc *asd, u_int segment_id,
			   uint8_t *segment_ptr, u_int bytes_to_read)
{
	uint32_t		offset;
	u_int			pad_size;
	u_int			image_size;
	u_int			attr;
	u_int			bytes_read;
	int			checksum_bytes;
	uint16_t		sum;
	uint16_t		*seg_ptr;
	u_short			i;
	
	if (asd_hwi_search_nv_segment(asd, segment_id, &offset, &pad_size,
				      &image_size, &attr) != 0) {
		asd_log(ASD_DBG_ERROR, "Requested segment not found in "
			"the NVRAM.\n");
		return (-1);
	}

	memset(segment_ptr, 0x0, bytes_to_read);
	if (asd_hwi_read_nv_segment(asd, NVRAM_NO_SEGMENT_ID, segment_ptr,
				    offset, bytes_to_read, &bytes_read) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to read NVRAM segment %d.\n",
			NVRAM_NO_SEGMENT_ID);
		return (-1);
	}

	if (asd_hwi_reset_nvram(asd) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to reset NVRAM.\n");
		return (-1);
	}

	seg_ptr = (uint16_t *) segment_ptr;
	/*
	 * checksum_bytes is equivalent to the size of the layout.
         * The size of layout is available at the offset of 8.
	 */	 
	checksum_bytes = (*(seg_ptr + 4) / 2);
	offset += asd->hw_profile.nv_flash_bar;
	sum = 0;
	
	for (i = 0; i < checksum_bytes; i++)
		sum += asd_hwi_swb_read_word(asd, (offset + (i*2)));

	if (sum != 0) {
		asd_log(ASD_DBG_ERROR, "Checksum verification failed.\n");
		return (-1);
	}

	return (0);
}

/*
 * Function:
 * 	asd_hwi_get_nv_config() 
 *
 * Description:
 *	Retrieves NVRAM configuration for the controller.
 */	   
static int
asd_hwi_get_nv_config(struct asd_softc  *asd)
{
	struct asd_manuf_base_seg_layout manuf_layout;
	struct asd_pci_layout	pci_layout;
	uint32_t		offset;
	u_int			bytes_read;

	if (asd_hwi_check_flash(asd) < 0) 
		return -1;

	asd->hw_profile.nv_exist = 0;
	/* Verify if the controller NVRAM has CTRL_A_SETTING type. */
	if (asd_hwi_verify_nv_checksum(asd, NVRAM_CTRL_A_SETTING,
				       (uint8_t *) &pci_layout,
				       sizeof(struct asd_pci_layout)) != 0) {
		/* 
		 * CTRL_A type verification failed, verify if the controller  
		 * has NVRAM CTRL_A_DEFAULT type.
		 */  
		if (asd_hwi_verify_nv_checksum(asd, NVRAM_CTRL_A_DEFAULT,
					       (uint8_t *) &pci_layout, 
					       sizeof(struct asd_pci_layout))
					       != 0)
			return (-1);

		asd->hw_profile.nv_segment_type = NVRAM_CTRL_A_DEFAULT;
	} else {
		asd->hw_profile.nv_segment_type = NVRAM_CTRL_A_SETTING;
		asd->hw_profile.nv_exist = 1;
	}

	offset = 0;
	memset(&manuf_layout, 0x0, sizeof (struct asd_manuf_base_seg_layout));

	/* Retrieves Controller Manufacturing NVRAM setting. */
	if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE, &manuf_layout,
			    offset, sizeof(struct asd_manuf_base_seg_layout),
			    &bytes_read) != 0) {
		return (-1);
	}

	memcpy(asd->hw_profile.wwn, manuf_layout.base_sas_addr, 
	       ASD_MAX_WWN_LEN);

	return (0);
}	

/*
 * Function:
 * 	asd_hwi_search_nv_id()
 *
 * Description:
 * 	Search for the requested NV setting ID. If successful copy contents
 * 	to the destination buffer and set appropriate value in offset.
 */
static int
asd_hwi_search_nv_id(struct asd_softc  *asd, u_int setting_id, void *dest,
		     u_int  *src_offset, u_int bytes_to_read)
{
	struct asd_settings_layout *psettings;
	uint32_t offset;
	uint32_t bytes_read;

	offset = 0;
	bytes_read = 0;
	psettings = (struct asd_settings_layout *)dest;
	while (1) {
		
        	if (asd_hwi_read_nv_segment(asd, 
				    asd->hw_profile.nv_segment_type, 
				    psettings,
				    offset, 
				    bytes_to_read,
				    &bytes_read) != 0) {
			return (-1);
		}

		if (psettings->id == setting_id) {
            		break;
        	} else {
			
            		if (psettings->next_ptr == 0)
				return -1;
			offset = (uint32_t)offset + psettings->next_ptr;
		}
	}
	*src_offset = offset;
	return (0);
}

static int
asd_hwi_get_nv_phy_settings(struct asd_softc *asd)
{
	struct asd_phy_settings_layout phy_settings;
	struct asd_phy_settings_entry_layout *phy_entry;
	uint32_t		offset;
	uint32_t		bytes_to_read;
	struct asd_phy		*phy;
	u_int			bytes_read;
	u_int		 	phy_id;
	u_char		 	settings_found;

	if (asd->hw_profile.nv_exist != 1)
		return -1;
	/*
	 * NVRAM setting exists, retrieve Phys user settings, such 
	 * as SAS address, connection rate, and attributes.
	 */  
	
	offset = 0;
	settings_found = 0;
	if (asd_hwi_search_nv_id(asd, NVRAM_PHY_SETTINGS_ID, 
			&phy_settings, &offset, sizeof(phy_settings))) {
		return -1;
	}

	bytes_to_read = phy_settings.num_phys * sizeof(*phy_entry);
	if ((phy_entry = asd_alloc_mem(bytes_to_read, GFP_ATOMIC)) 
	   == NULL) {
		return (-1);
	}

	if (asd_hwi_read_nv_segment(asd, 
		    asd->hw_profile.nv_segment_type, 
		    phy_entry,
		    offset + sizeof(phy_settings),
		    bytes_to_read,
		    &bytes_read) != 0) {
		asd_free_mem(phy_entry);
		return (-1);
	} else {
		settings_found = 1;
	}

	if (settings_found == 1) {
		asd->hw_profile.max_phys = phy_settings.num_phys;
		for (phy_id = 0; 
		     phy_id < asd->hw_profile.max_phys; 
		     phy_id++) {
			
			if ((phy = asd->phy_list[phy_id]) == NULL)
				continue;
			
			memcpy(phy->sas_addr, 
			       phy_entry[phy_id].sas_addr,
			       SAS_ADDR_LEN);
			/* 
			 * if an invalid link rate is specified,
			 * the existing default value is used. 
			 */
			asd_hwi_map_lrate_from_sas(
				(phy_entry[phy_id].link_rate 
				 & PHY_MIN_LINK_RATE_MASK),
				&phy->min_link_rate);

			asd_hwi_map_lrate_from_sas(
				((phy_entry[phy_id].link_rate
				 & PHY_MAX_LINK_RATE_MASK) >> 4),
				&phy->max_link_rate);
			/* TBD: crc */
		}
	}
	
	asd_free_mem(phy_entry);
	return 0;
}

static void
asd_hwi_get_nv_phy_params(struct asd_softc *asd)
{
	struct asd_manuf_base_seg_layout   manuf_layout;	
	struct asd_manuf_phy_param_layout  phy_param;
	struct asd_phy_desc_format	  *pphy_desc;
	struct asd_phy			  *phy;
	uint32_t 	 		   offset;
	uint32_t	 		   bytes_to_read;
	uint32_t 	 		   bytes_read;
	u_int 		 		   phy_id;
	
	/* Set the phy parms to the default value. */
	for (phy_id = 0; phy_id < asd->hw_profile.max_phys; phy_id++) {
		if ((phy = asd->phy_list[phy_id]) == NULL)
			continue;

		phy->phy_state = PHY_STATE_DEFAULT;
		phy->phy_ctl0 = PHY_CTL0_DEFAULT;
		phy->phy_ctl1 = PHY_CTL1_DEFAULT;
		phy->phy_ctl2 = PHY_CTL2_DEFAULT;
		phy->phy_ctl3 = PHY_CTL3_DEFAULT;
	}

	if (asd_hwi_verify_nv_checksum(asd, NVRAM_MANUF_TYPE,
				       (uint8_t *) &manuf_layout,
				       sizeof(struct asd_manuf_base_seg_layout))
				       != 0) {
		asd_log(ASD_DBG_ERROR, "Failed verifying checksum for "
			"NVRAM MANUFACTURING LAYOUT.\n");
		goto exit;
	}

	offset = 0;
	memset(&phy_param, 0x0, sizeof(phy_param));

	if (asd_hwi_get_nv_manuf_seg(asd, &phy_param,
				     sizeof(phy_param), &offset,
				     NVRAM_MNF_PHY_PARAM_SIGN) != 0)
		goto exit;

	if (phy_param.num_phy_desc > asd->hw_profile.max_phys)
		phy_param.num_phy_desc = asd->hw_profile.max_phys;
	
	bytes_to_read = phy_param.num_phy_desc * phy_param.phy_desc_size;
	if ((pphy_desc = asd_alloc_mem(bytes_to_read, GFP_ATOMIC)) == NULL)
		goto exit;

	offset += sizeof(phy_param);
	if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE, pphy_desc, offset,
				    bytes_to_read, &bytes_read) != 0) {
		asd_free_mem(pphy_desc);
		goto exit;
	}

	for (phy_id = 0; phy_id < asd->hw_profile.max_phys; phy_id++) {
		if ((phy = asd->phy_list[phy_id]) == NULL)
			continue;

		phy->phy_state = (pphy_desc[phy_id].state & PHY_STATE_MASK);
		phy->phy_ctl0 &= ~PHY_CTL0_MASK;
		phy->phy_ctl0 |= (pphy_desc[phy_id].ctl0 & PHY_CTL0_MASK);
		phy->phy_ctl1 = pphy_desc[phy_id].ctl1;
		phy->phy_ctl2 = pphy_desc[phy_id].ctl2;
		phy->phy_ctl3 = pphy_desc[phy_id].ctl3;

		if (phy->phy_state == NVRAM_PHY_HIDDEN) {
			asd->hw_profile.enabled_phys &= ~(1 << phy_id);
		}
	}

	asd_free_mem(pphy_desc);
exit:	
	return;
}

/*
 * Function:
 *	asd_hwi_get_nv_conn_info
 *
 * Description:
 * 	This routine reads the Connector Map information from Flash (NVRAM)
 * 	and allocates memory and populates the parameters 'pconn' 
 * 	and 'pnoded' with the read information. 
 *
 * 	At the end of this routine if everything goes fine the data structure 
 * 	layout pointed to by 'pconn' and 'pnoded' will be:
 *
 * 	'pconn' ->
 * 	|-----------|--------------|--------------|--------
 * 	| Connector | Connector    | Connector    | ...
 * 	| Map 	    | Descriptor 0 | Descriptor 1 | 
 * 	|-----------|--------------|--------------|--------
 * 	
 * 	'pnoded' ->
 * 	|--------|--------|--------|---  --|-------|--------|--------|----
 * 	| Node   | Phy    | Phy    |...    | Node  | Phy    | Phy    | ...
 * 	| Desc 0 | Desc 0 | Desc 1 |       | Desc 1| Desc 0 | Desc 1 |
 * 	|--------|--------|--------|---  --|-------|--------|--------|----
 *
 * 	'pconn_size' will hold the size of data pointed to by 'pconn' 
 * 	'pnoded_size' will hold the size of data pointed to by 'pnoded'. 
 * 	
 */
int
asd_hwi_get_nv_conn_info(struct asd_softc *asd,
			 uint8_t **pconn,
			 uint32_t *pconn_size,
			 uint8_t **pnoded,
			 uint32_t *pnoded_size)
{
	struct asd_manuf_base_seg_layout   manuf_layout;	
	struct asd_conn_map_format conn_map;
	struct asd_node_desc_format node_desc;
	//struct asd_conn_desc_format *pconn_desc;
	uint8_t *pconn_buf; 
	uint8_t *pnode_buf; 
	uint32_t offset, cur_offset;
	uint32_t bytes_to_read, bytes_read;
	uint32_t bytes_count, tot_count;
	uint32_t node_num;
	
	if (pconn == NULL || pnoded == NULL || pconn_size == NULL 
	   || pnoded_size == NULL) {
		return -1;
	}

	if (asd_hwi_verify_nv_checksum(asd, NVRAM_MANUF_TYPE,
				       (uint8_t *) &manuf_layout,
				       sizeof(manuf_layout))
				       != 0) {
		asd_log(ASD_DBG_ERROR, "Failed verifying checksum for "
			"NVRAM MANUFACTURING LAYOUT.\n");
		return -1;
	}

	/* 
	 * Determine the memory size to be allocated by reading
	 * the connector_map first
	 */
	offset = 0;
	memset(&conn_map, 0x0, sizeof(conn_map));
	if (asd_hwi_get_nv_manuf_seg(asd, &conn_map, 
				sizeof(conn_map), &offset,
				NVRAM_MNF_CONN_MAP) != 0){
		return (-1);
	}
	
	bytes_to_read = sizeof(conn_map) 
			+ conn_map.num_conn_desc * conn_map.conn_desc_size;
	
	/* 
	 * Now read connector_map and associated connector_decriptor entries
	 */
	*pconn_size = bytes_to_read;
	if ((pconn_buf = asd_alloc_mem(bytes_to_read, GFP_ATOMIC)) 
	   == NULL) {
		return (-1);
	}
	memset(pconn_buf, 0, bytes_to_read);
	if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE,
				pconn_buf, offset,
				bytes_to_read,
				&bytes_read) != 0) {
		asd_free_mem(pconn_buf);
		return (-1);
	}

	/* bypass all read connector_map & connector_descriptor entries */
	offset += (bytes_read + 1);
	
	/* no field in connector map yet */
	bytes_to_read = sizeof(node_desc); 

	/* 
	 * Determine memory to be allocated for reading node descriptors
	 * and their associated phy descriptor entries
	 */
	bytes_count = 0;
	tot_count = 0;
	cur_offset = offset;
	for (node_num = 0; node_num < conn_map.num_node_desc; node_num++) {
		if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE,
					&node_desc, cur_offset,
					bytes_to_read,
					&bytes_read) != 0) {
			asd_free_mem(pconn_buf);
			return (-1);
		}
		bytes_count = (sizeof(node_desc) 
				+ (node_desc.num_phy_desc 
				   * node_desc.phy_desc_size));
		cur_offset += bytes_count;
		tot_count += bytes_count;
	}
	bytes_to_read = tot_count;
	
	/* 
	 * Now read node descriptors and their 
	 * associated phy descriptor entries
	 */
	if ((pnode_buf = asd_alloc_mem(bytes_to_read, GFP_ATOMIC)) 
	   == NULL) {
		asd_free_mem(pconn_buf);
		return (-1);
	}
	if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE,
				pnode_buf, offset,
				bytes_to_read,
				&bytes_read) != 0) {
		asd_free_mem(pconn_buf);
		asd_free_mem(pnode_buf);
		return (-1);
	}

	/* Caller has to free the resources */
	*pconn = pconn_buf;
	*pnoded = pnode_buf;
	*pnoded_size = tot_count;

	return 0;
}

static int
asd_hwi_get_nv_manuf_seg(struct asd_softc *asd, void *dest, 
			 uint32_t bytes_to_read, uint32_t *src_offset,
			 uint16_t signature) 
{
	struct asd_manuf_base_seg_layout manuf_layout;
	uint32_t 			 offset;
	uint32_t 			 bytes_read;
	int				 segments_size;
	int				 segment_off;
	int 				 err;
	
	memset(&manuf_layout, 0x0, sizeof(manuf_layout));
	err = -1;
	offset = 0;

	/* Retrieve Manufacturing Base segment */
	if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE, &manuf_layout,
				    offset, sizeof(manuf_layout), 
				    &bytes_read) != 0) {
		return err;
	}

	/* Retrieve Manufacturing segment specified by signature */
	offset = manuf_layout.seg_sign.next_seg_ptr;
	segments_size = manuf_layout.sector_size - sizeof(manuf_layout); 
	segment_off = offset;

	while (1) {
		if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE,
					    dest, offset, bytes_to_read,
					    &bytes_read) != 0) {
			return (-1);
		}

		if (((struct asd_manuf_seg_sign *) dest)->signature ==
			signature) {
			*src_offset = offset;
			err = 0;
			break;
		}
		
		/*
		 * If next_seg_ptr is 0 then it indicates the last segment.
		 */
		if (((struct asd_manuf_seg_sign *) dest)->next_seg_ptr == 0)
			break;

		offset = ((struct asd_manuf_seg_sign *) dest)->next_seg_ptr;
	}

	return err;
}

static int
asd_hwi_map_lrate_from_sas(u_int sas_link_rate, 
		u_int *asd_link_rate)
{
	switch (sas_link_rate) {
	case SAS_RATE_30GBPS:
		*asd_link_rate = SAS_30GBPS_RATE; 
		break;
	case SAS_RATE_15GBPS:
		*asd_link_rate = SAS_15GBPS_RATE;
		break;
	default:
		return -1;
	}
	return 0;
}

static int
asd_hwi_set_speed_mask(u_int asd_link_rate, 
				  uint8_t *asd_speed_mask)
{
	switch (asd_link_rate) {
	case SAS_60GBPS_RATE:
		*asd_speed_mask &= ~SAS_SPEED_60_DIS; 
		break;
	case SAS_15GBPS_RATE:
		*asd_speed_mask &= ~SAS_SPEED_15_DIS; 
		break;
	case SAS_30GBPS_RATE:
	default:
		*asd_speed_mask &= ~SAS_SPEED_30_DIS; 
		break;
	}

	return 0;
}

/***************************************************************************
*  OCM directory default
***************************************************************************/
static struct asd_ocm_dir_format OCMDirInit =
{
   OCM_DIR_SIGN,		    /* signature                     */
   0,                               /* reserve byte                  */
   0,                               /* reserve byte                  */
   0,                               /* Major Version No.             */
   0,                               /* Minor Version No.             */
   0,                               /* reserve byte                  */
   0x05,                            /* no. of directory entries      */
};

/***************************************************************************
*  OCM directory Entries default
***************************************************************************/
static struct asd_ocm_entry_format OCMDirEntriesInit[5] =
{
   {
      (uint8_t)(OCM_DE_ADDC2C_RES0), /* Entry type  */
      {
        128,                             /* Offset 0                      */
        0,                               /* Offset 1                      */
        0,                               /* Offset 2                      */
      },
      0,                               /* reserve byte                  */
      {
        0,                               /* size 0                        */
        4,                               /* size 1                        */
        0                                /* size 2                        */
      }
   },
   {
      (uint8_t)(OCM_DE_ADDC2C_RES1), /* Entry type  */
      {
        128,                             /* Offset 0                      */
        4,                               /* Offset 1                      */
        0,                               /* Offset 2                      */
      },
      0,                               /* reserve byte                  */
      {
        0,                               /* size 0                        */
        4,                               /* size 1                        */
        0                                /* size 2                        */
      }
   },
   {
      (uint8_t)(OCM_DE_ADDC2C_RES2), /* Entry type  */
      {
        128,                             /* Offset 0                      */
        8,                               /* Offset 1                      */
        0,                               /* Offset 2                      */
      },
      0,                               /* reserve byte                  */
      {
        0,                               /* size 0                        */
        4,                               /* size 1                        */
        0                                /* size 2                        */
      }
   },
   {
      (uint8_t)(OCM_DE_ADDC2C_RES3), /* Entry type  */
      {
        128,                             /* Offset 0                      */
        12,                              /* Offset 1                      */
        0,                               /* Offset 2                      */
      },
      0,                               /* reserve byte                  */
      {
        0,                               /* size 0                        */
        4,                               /* size 1                        */
        0                                /* size 2                        */
      }
   },
   {
      (uint8_t)(OCM_DE_WIN_DRVR), /* Entry type  */
      {
        128,                             /* Offset 0                      */
        16,                              /* Offset 1                      */
        0,                               /* Offset 2                      */
      },
      0,                               /* reserve byte                  */
      {
        128,                             /* size 0 (125824 % 256)         */
        235,                             /* size 1 (125824 / 256)         */
        1                                /* size 2 (125824 /(256 * 256))  */
      }
   }
};

static int
asd_hwi_initialize_ocm_dir (struct asd_softc  *asd)
{
	uint8_t *pOCMData;
	uint8_t i;

   /* load the OCM directory format from OCMDirInit data */
	pOCMData = (uint8_t *) &OCMDirInit;
	for (i = 0; i < sizeof(struct asd_ocm_dir_format); i++)
	{
		asd_hwi_swb_write_byte(asd, OCM_BASE_ADDR + i, pOCMData[i]);
	}

   /* load the OCM directory format from OCMDirInit data */
	pOCMData = (uint8_t *) &OCMDirEntriesInit[0];
	for (i = 0; i < (sizeof(struct asd_ocm_entry_format) * 5); i++)
	{
		asd_hwi_swb_write_byte(asd, OCM_BASE_ADDR + (i + sizeof(struct asd_ocm_dir_format)), pOCMData[i]);
	}
	return 0;
}

static int
asd_hwi_check_ocm_access (struct asd_softc  *asd)
{

	uint32_t exsi_base_addr;
	uint32_t reg_contents;
	uint32_t	 i;
	uint32_t intrptStatus;

	/* check if OCM has been initialized by BIOS */	
	exsi_base_addr = EXSI_REG_BASE_ADR + EXSICNFGR;
	reg_contents = asd_hwi_swb_read_dword(asd, exsi_base_addr);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "Current EXSICNFGR is 0x%x\n",reg_contents);
#endif
	if (!(reg_contents & OCMINITIALIZED)) {
		intrptStatus = asd_pcic_read_dword(asd,PCIC_INTRPT_STAT);
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "OCM is not initialized by BIOS, reinitialize it and ignore it, current IntrptStatus is 0x%x\n",intrptStatus);
#endif
      /* Initialize OCM to avoid future OCM access to get parity error */
         /* clear internal error register */
		asd_pcic_write_dword(asd,  PCIC_INTRPT_STAT, intrptStatus);
		for (i = 0; i < OCM_MAX_SIZE; i += 4)
		{
			asd_hwi_swb_write_dword(asd, OCM_BASE_ADDR + i, 0);
		}
		asd_hwi_initialize_ocm_dir(asd);
		return -1;
	}
	return 0;
}

static int
asd_hwi_get_ocm_info(struct asd_softc  *asd)
{
	struct asd_ocm_entry_format ocm_de;
	struct asd_bios_chim_format *pbios_chim;
	uint32_t offset, bytes_read, bytes_to_read;

	if(asd_hwi_check_ocm_access(asd) != 0) {
		return -1;
	}

	if (asd_hwi_get_ocm_entry(asd, OCM_DE_BIOS_CHIM, 
				  &ocm_de, &offset) != 0) {
		return -1;
	}

	bytes_to_read = OCM_DE_OFFSET_SIZE_CONV(ocm_de.size);
	if ((pbios_chim = asd_alloc_mem(bytes_to_read, GFP_ATOMIC)) 
	   == NULL) {
		return (-1);
	}
	if (asd_hwi_read_ocm_seg(asd, pbios_chim, 
		OCM_DE_OFFSET_SIZE_CONV(ocm_de.offset), 
		bytes_to_read, 
		&bytes_read) != 0) {
		asd_free_mem(pbios_chim);
		return -1;
	}
	if (pbios_chim->signature == OCM_BC_BIOS_SIGN) {
		if (pbios_chim->bios_present 
		   & OCM_BC_BIOS_PRSNT_MASK) {
			asd->hw_profile.bios_maj_ver = 
					pbios_chim->bios_maj_ver;
			asd->hw_profile.bios_min_ver = 
					pbios_chim->bios_min_ver;
			asd->hw_profile.bios_bld_num = 
					pbios_chim->bios_bld_num;
		}
	}

	asd->unit_elements = asd_alloc_mem(pbios_chim->num_elements * 
		sizeof(struct asd_unit_element_format), GFP_ATOMIC);

	if (asd->unit_elements == NULL) {
		asd_free_mem(pbios_chim);
		return 0;
	}

	asd->num_unit_elements = pbios_chim->num_elements;

	memcpy(asd->unit_elements, &pbios_chim->unit_elm[0],
		pbios_chim->num_elements * 
			sizeof(struct asd_unit_element_format));

	asd_free_mem(pbios_chim);

	return 0;
}

static int
asd_hwi_get_ocm_entry(struct asd_softc  *asd, 
				 uint32_t entry_type, 
				 struct asd_ocm_entry_format *pocm_de, 
				 uint32_t *src_offset)
{
	struct asd_ocm_dir_format ocm_dir;
	uint32_t offset, entry_no;
	u_int bytes_read;
	int err;

	memset(&ocm_dir, 0x0, sizeof (ocm_dir));
	memset(pocm_de, 0x0, sizeof (*pocm_de));
	offset = 0;
	err = -1;
	
	if (asd_hwi_read_ocm_seg(asd, &ocm_dir, offset, 
		sizeof(ocm_dir), &bytes_read)) {
		return -1;
	}

	if ((ocm_dir.signature != OCM_DIR_SIGN) 
	   /*|| ((ocm_dir.num_entries & OCM_NUM_ENTRIES_MASK) == 0)*/) {
		return -1;
	}

	offset = sizeof(ocm_dir);
	for (entry_no = 0; 
	     entry_no < (ocm_dir.num_entries & OCM_NUM_ENTRIES_MASK); 
	     entry_no++, offset += sizeof(*pocm_de)) {
		if (asd_hwi_read_ocm_seg(asd, pocm_de, offset, 
			sizeof(*pocm_de), &bytes_read)) {
			return -1;
		}
		if (pocm_de->type == entry_type) {
			err = 0;
			break;
		}
	}
	*src_offset = offset;
	return err;
}

static int
asd_hwi_read_ocm_seg(struct asd_softc *asd, void *dest,
			uint32_t src_offset, u_int bytes_to_read,
			u_int *bytes_read)
{
    	uint8_t		*dest_buf;
    	uint32_t	 nv_offset;
	uint32_t	 i;
	
	nv_offset = 0;
	nv_offset = OCM_BASE_ADDR + nv_offset + src_offset;
	
	dest_buf = (uint8_t *) dest;
	*bytes_read = 0;
	
	for (i = 0; i < bytes_to_read; i++) {
		*(dest_buf+i) = asd_hwi_swb_read_byte(asd, (nv_offset + i));
	}
	*bytes_read = i - 1;

	return (0); 
}

static int asd_hwi_check_flash(struct asd_softc *asd) 
{
	uint32_t nv_flash_bar;
	uint32_t exsi_base_addr;
	uint32_t reg_contents;
	uint8_t manuf_id;
	uint8_t dev_id_boot_blk;
	uint8_t sec_prot;

	/* get Flash memory base address */
	asd->hw_profile.nv_flash_bar = 
				asd_pcic_read_dword(asd, PCIC_FLASH_MBAR);
	nv_flash_bar = asd->hw_profile.nv_flash_bar; 

	/* check presence of flash */	
	exsi_base_addr = EXSI_REG_BASE_ADR + EXSICNFGR;
	reg_contents = asd_hwi_swb_read_dword(asd, exsi_base_addr);
	if (!(reg_contents & FLASHEX)) {
		asd->hw_profile.flash_present = FALSE;
		return -1;
	}
	asd->hw_profile.flash_present = TRUE;
	
	/* Determine flash info */
	if (asd_hwi_reset_nvram(asd) != 0) {
		return (-1);
	}

	asd->hw_profile.flash_method = FLASH_METHOD_UNKNOWN;
	asd->hw_profile.flash_manuf_id = FLASH_MANUF_ID_UNKNOWN;
	asd->hw_profile.flash_dev_id = FLASH_DEV_ID_UNKNOWN;

	/*
	 * The strategy is to try to read the flash ID using "Method A" first.
	 * If that fails, we will try "Method B"
	 */
	/* Issue Unlock sequence for AM29LV800D */
 	asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0xAAA), 0xAA);
 	asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0x55);

	/* Issue the erase command */
	asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0xAAA), 0x90);

	manuf_id = asd_hwi_swb_read_byte(asd, nv_flash_bar);
	dev_id_boot_blk = asd_hwi_swb_read_byte(asd, nv_flash_bar + 1);
	sec_prot = asd_hwi_swb_read_byte(asd, nv_flash_bar + 2);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "Flash MethodA manuf_id(0x%x) dev_id_boot_blk(0x%x) sec_prot(0x%x)\n",manuf_id,dev_id_boot_blk,sec_prot);
#endif
	if (asd_hwi_reset_nvram(asd) != 0) {
		return (-1);
	}

	switch (manuf_id) {

	case FLASH_MANUF_ID_AMD:

		switch (sec_prot) {

	   	case FLASH_DEV_ID_AM29LV800DT:
	   	case FLASH_DEV_ID_AM29LV640MT:
			asd->hw_profile.flash_method = FLASH_METHOD_A;
			break;
		default:
			break;
		}
		break;

	case FLASH_MANUF_ID_ST:

		switch (sec_prot) {

	   	case FLASH_DEV_ID_STM29W800DT:
			asd->hw_profile.flash_method = FLASH_METHOD_A;
			break;
		default:
			break;
		}
		break;

	case FLASH_MANUF_ID_FUJITSU:

		switch (sec_prot) {

		case FLASH_DEV_ID_MBM29LV800TE:
			asd->hw_profile.flash_method = FLASH_METHOD_A;
			break;

		}
		break;

	case FLASH_MANUF_ID_MACRONIX:

		switch (sec_prot) {

		case FLASH_DEV_ID_MX29LV800BT:
			asd->hw_profile.flash_method = FLASH_METHOD_A;
			break;
		}
		break;
	}

	if (asd->hw_profile.flash_method == FLASH_METHOD_UNKNOWN) {

		if (asd_hwi_reset_nvram(asd) != 0) {
			return (-1);
		}

		/* Issue Unlock sequence for AM29LV008BT */
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0xAA);
		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x2AA), 0x55);

		asd_hwi_swb_write_byte(asd, (nv_flash_bar + 0x555), 0x90);

		manuf_id = asd_hwi_swb_read_byte(asd, nv_flash_bar);
		dev_id_boot_blk = asd_hwi_swb_read_byte(asd, nv_flash_bar + 1);
		sec_prot = asd_hwi_swb_read_byte(asd, nv_flash_bar + 2);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "Flash MethodB manuf_id(0x%x) dev_id_boot_blk(0x%x) sec_prot(0x%x)\n",manuf_id,dev_id_boot_blk,sec_prot);
#endif

		if (asd_hwi_reset_nvram(asd) != 0) {
			return (-1);
		}

		switch (manuf_id) {
		case FLASH_MANUF_ID_AMD:

			switch (dev_id_boot_blk) {

		    	case FLASH_DEV_ID_AM29LV008BT:
				asd->hw_profile.flash_method = FLASH_METHOD_B;
				break;
			default:
				break;
			}
			break;

		case FLASH_MANUF_ID_FUJITSU:

			switch (dev_id_boot_blk) {

			case FLASH_DEV_ID_MBM29LV008TA:
				asd->hw_profile.flash_method = FLASH_METHOD_B;
				break;

			}
			break;

		default:
			return -1;
		}
	}

	switch (asd->hw_profile.flash_method)
	{
	case FLASH_METHOD_A:
		return 0;

	case FLASH_METHOD_B:
		break;

	default:
		return -1;
	}

	asd->hw_profile.flash_manuf_id = manuf_id;
	asd->hw_profile.flash_dev_id = dev_id_boot_blk;
	asd->hw_profile.flash_wr_prot = sec_prot;
	return 0;
}
#endif /* NVRAM_SUPPORT */

int
asd_hwi_control_activity_leds(struct asd_softc *asd, uint8_t phy_id, 
			      uint32_t asd_phy_ctl_func)
{
	uint32_t	exsi_base_addr;
	uint32_t	reg_contents;
	uint32_t	reg_data;
	
//JD we support B0,B1...
//	if (asd->hw_profile.rev_id != AIC9410_DEV_REV_B0) 
//		return -EINVAL;

	if (phy_id >= ASD_MAX_XDEVLED_BITS)
		return -EINVAL;

	reg_data = 0;

	/* set the bit map corresponding to phy_id */
	reg_data = (1 << phy_id);

	/* Enable/Disable activity LED for the PHY */
	exsi_base_addr = EXSI_REG_BASE_ADR + GPIOOER;
	reg_contents = asd_hwi_swb_read_dword(asd, exsi_base_addr);

	if (asd_phy_ctl_func == DISABLE_PHY) {
		reg_contents &= ~reg_data;
	} else {
		if (asd_phy_ctl_func == ENABLE_PHY
		   || asd_phy_ctl_func == ENABLE_PHY_NO_SAS_OOB
		   || asd_phy_ctl_func == ENABLE_PHY_NO_SATA_OOB ) {
			reg_contents |= reg_data;
		}
	}
	asd_hwi_swb_write_dword(asd, exsi_base_addr, reg_contents);
	
	/* Set activity source to external/internal */
	exsi_base_addr = EXSI_REG_BASE_ADR + GPIOCNFGR;
	reg_contents = asd_hwi_swb_read_dword(asd, exsi_base_addr);

	if (asd_phy_ctl_func == DISABLE_PHY) {
		reg_contents &= ~reg_data;
	} else {
		if (asd_phy_ctl_func == ENABLE_PHY
		   || asd_phy_ctl_func == ENABLE_PHY_NO_SAS_OOB
		   || asd_phy_ctl_func == ENABLE_PHY_NO_SATA_OOB ) {
			reg_contents |= reg_data;
		}
	}
	asd_hwi_swb_write_dword(asd, exsi_base_addr, reg_contents);

	return 0;
}

