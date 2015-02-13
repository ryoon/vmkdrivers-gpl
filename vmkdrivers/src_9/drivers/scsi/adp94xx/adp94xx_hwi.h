/*
 * Portions Copyright 2008 VMware, Inc.
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
 * $Id: //depot/razor/linux/src/adp94xx_hwi.h#117 $
 * 
 */	

#ifndef ADP94XX_HWI_H
#define ADP94XX_HWI_H

#include "adp94xx_sas.h"
#include "adp94xx_reg.h"

struct asd_softc;
struct asd_platform_data;
struct asd_target;
struct asd_device;
struct asd_conn_desc_format;
struct asd_nd_phydesc_format;

/* CHIP Revision ID */
#define AIC9410_DEV_REV_A0	0
#define AIC9410_DEV_REV_A1	1
#define AIC9410_DEV_REV_B0	8
#define AIC9410_DEV_REV_B1	9

#define COMSTOCK_LATEST_RTL	26

/* Controller profile */
#define ASD_MAX_WWN_LEN		SAS_ADDR_LEN	/* World Wide Name size */

#if SAS_COMSTOCK_SUPPORT
#define ASD_MAX_PHYS		1		/* Max supported phys */
#define ASD_MAX_PORTS		ASD_MAX_PHYS	/* Max supported ports */
#else
#define ASD_MAX_PHYS		8		/* Max supported phys */
#define ASD_MAX_PORTS		ASD_MAX_PHYS	/* Max supported ports */
#endif

#define ASD_MAX_SCB_SITES	512		/*
						 * Max supported built-in
						 * SCB sites
						 */
#ifdef EXTENDED_SCB
#define ASD_EXTENDED_SCB_NUMBER 512
#endif
#define ASD_RSVD_SCBS		11		/* Reserved SCBs */
#define ASD_MAX_EDBS_PER_SCB	7		/* 
						 * Max number of Empty
						 * data buffers that can
						 * be queued in a single SCB.
						 */
#define ASD_MAX_EDBS		21		/* 
						 * Max number of Empty
						 * data buffers.
						 */

/*
 * There is a workaround fix required by the sequencer to skip
 * using the free scb sites that ended with FF.
 */ 
static inline u_int
asd_calc_free_scb_sites(u_int max_scbs)
{
	uint16_t	site_no;
	u_int		usable_scb_sites;

	usable_scb_sites = max_scbs;
	for (site_no = 0; site_no < max_scbs; site_no++) {
		if ((site_no & 0xF0FF) == 0x00FF)
			usable_scb_sites--;
#ifdef SATA_SKIP_FIX
		if ((site_no & 0xF0FF) <= 0x001F)
			usable_scb_sites--;
#endif
	}
	return (usable_scb_sites);
}

#define ASD_USABLE_SCB_SITES	asd_calc_free_scb_sites(ASD_MAX_SCB_SITES)

#ifndef EXTENDED_SCB
#define ASD_MAX_ALLOCATED_SCBS	\
	(ASD_USABLE_SCB_SITES - (ASD_MAX_EDBS/ASD_MAX_EDBS_PER_SCB))
#else
#define ASD_MAX_ALLOCATED_SCBS	\
	(ASD_USABLE_SCB_SITES + ASD_EXTENDED_SCB_NUMBER - (ASD_MAX_EDBS/ASD_MAX_EDBS_PER_SCB))
#endif

#define ASD_MAX_USABLE_SCBS	ASD_USABLE_SCB_SITES

/*
 * Since an SES device may give device IDs to devices to be presented
 * to the OS, we give "free" devices IDs which start at 128, and SES
 * devices give device IDs which start at 0.
 */
#define ASD_MAX_TARGET_IDS      256
#define ASD_MAX_DEVICES		128		/* Max supported devices */
#define ASD_MAX_EXPANDERS	16		/* Max supported expanders */
#define ASD_MAX_TARGETS		(ASD_MAX_DEVICES - ASD_MAX_EXPANDERS)
#define ASD_MAX_DDBS		128		/* Max number of HW DDB sites */
#define ASD_MIN_DLS		4		/* Minimum DL amount allowed */
#define ASD_MAX_DLS		16384		/* Maximum DL amount allowed */
#define ASD_SCB_SIZE		sizeof(union hardware_scb)
#define ASD_DDB_SIZE		sizeof(struct asd_ddb)
#define ASD_DL_SIZE		sizeof(struct asd_done_list)

#define ASD_QDONE_PASS_DEF	1
		 
/* Controller Delay */
#define ASD_DELAY_COUNT		5		/* Set to 5 microseconds */
#define ASD_REG_TIMEOUT_VAL	2000
#define ASD_REG_TIMEOUT_TICK	((ASD_REG_TIMEOUT_VAL * 1000) / ASD_DELAY_COUNT)
 
/* Controller functionality */
#define ASD_PIO_DOWNLOAD_SEQS	1		/* 
						 * Download the SEQS code using
						 * PIO mode.
						 */		
#define ASD_DMA_DOWNLOAD_SEQS	0		/*
						 * Download the SEQS code using
						 * Overlay DMA mode.
						 */

/* Tunable constant. */
#define SAS_NOTIFY_TIMER_TIMEOUT_CONST	500	/* 
						 * The default value 500 (in 
						 * msecs) for time interval for
						 * transmission interval between
						 * successive NOTIFY 
						 * (ENABLE_SPINUP) primitive.
						 */

/* Available activity LED bits in GPIOx registers */
#define ASD_MAX_XDEVLED_BITS		8

typedef enum {
	SCB_EH_NONE 		= 0x0000,
	SCB_EH_INITIATED	= 0x0001,
	SCB_EH_IN_PROGRESS	= 0x0002,
	SCB_EH_TIMEDOUT		= 0x0004,
	SCB_EH_DONE		= 0x0008,
	SCB_EH_ABORT_REQ	= 0x0010,
	SCB_EH_LU_RESET_REQ	= 0x0020,
	SCB_EH_DEV_RESET_REQ	= 0x0040,
	SCB_EH_PORT_RESET_REQ	= 0x0080,
	SCB_EH_CLR_NXS_REQ	= 0x0100,
	SCB_EH_SUSPEND_SENDQ	= 0x0200,
	SCB_EH_RESUME_SENDQ	= 0x0400,
	SCB_EH_PHY_NO_OP_REQ	= 0x0800,
	SCB_EH_PHY_REPORT_REQ	= 0x1000
} scb_eh_state;

#define SCB_EH_LEVEL_MASK	0x0FF0

typedef enum {
	SCB_EH_SUCCEED,
	SCB_EH_FAILED
} scb_eh_status;
typedef enum {
	SCB_FLAG_NONE		= 0x00,
	SCB_ACTIVE		= 0x01,
	SCB_DEV_QFRZN		= 0x02,
	SCB_TIMEDOUT		= 0x04,
	SCB_PENDING		= 0x08,
	SCB_INTERNAL		= 0x10,
	SCB_RECOVERY		= 0x20,
	SCB_RESERVED		= 0x40,
	SCB_ABORT_DONE		= 0x80
} scb_flag;

/* Data structures used to manage CSMI commands */
struct asd_ctl_mgmt {
	wait_queue_head_t	waitq;
	struct semaphore 	sem;
	struct timer_list 	timer;
	u_long 			busy;
	int 			err;
};

struct asd_ctl_internal_data {
	struct asd_ctl_mgmt mgmt; 
};

typedef void asd_scb_post_t(struct asd_softc *, struct scb *,
			    struct asd_done_list *);
typedef void asd_scb_eh_post_t(struct asd_softc *, struct scb *);

#define SCB_POST_STACK_DEPTH	5

struct asd_scb_post_stack {
	/*
	 * Completion callback.
	 */
	asd_scb_post_t		*post;

	/*
	 * OS per I/O context structure.
	 */
	asd_io_ctx_t		 io_ctx;

	/*
	 * Timeout function for SCB (used internally)
	 */
	void			(*timeout_func)(u_long);
};

/*
 * SCB buffer descriptor.  Contains HWI layer information
 * pertaining to SCB data DMAed to and from the controller.
 */
struct scb {
	/*
	 * Links for chaining SCBs onto the pending, or free list.
	 */
	struct list_head	 hwi_links;
	/*
	 * Links for use by the owner (e.g. originator) of the SCB.
	 */
	struct list_head	 owner_links;
	/*
	 * Links for chaining SCBs onto the timedout SCB list.
	 */
	struct list_head	 timedout_links;
	/*
	 * Completion callback.
	 */
	asd_scb_post_t		*post;
	/*
	 * OS per I/O context structure.
	 */
	asd_io_ctx_t		 io_ctx;
	/*
	 * OSM specific, per-context data.
	 */
	struct asd_scb_platform_data *platform_data;
	/*
	 * Per-context, hardware specific, data that is
	 * actually DMA'ed to the controller.
	 */
	union hardware_scb	*hscb;
	struct map_node		*hscb_map;
	dma_addr_t		 hscb_busaddr;
	/*
	 * Index to the Device Descriptor Block representing
	 * the device to which this transaction is targeted.
	 */
	uint16_t		 conn_handle;
	/*
	 * Scatter/Gather list data structures.
	 */
	struct map_node		*sg_map;
	struct sg_element	*sg_list;
	dma_addr_t		 sg_list_busaddr;
	u_int			 sg_count;
	/*
	 * Pointer back to our per-controller instance.
	 * Used only in timer callbacks where the SCB
	 * is the saved argument to the timer callback.
	 */
	struct asd_softc	*softc;
	/*
	 * Misc Flags.
	 */
	scb_flag		 flags;
	/*
	 * Error Recovery state and status info.
	 */ 
	scb_eh_state     	 eh_state;
	scb_eh_status     	 eh_status;
	asd_scb_eh_post_t	*eh_post;

	uint32_t			post_stack_depth;
	struct asd_scb_post_stack	post_stack[SCB_POST_STACK_DEPTH];

	/* The following are used for DIY sent SCBs. */
	u32 status;
	struct asd_done_list *dl;

#if defined(__VMKLNX__)
	void *bounce_buffer;
#endif
};

#define	SCB_GET_INDEX(SCB)	(asd_le16toh((SCB)->hscb->header.index))
#define SCB_GET_OPCODE(SCB)	((SCB)->hscb->header.opcode)
#define SCB_GET_SRC_PORT(SCB)	((SCB)->platform_data->targ->src_port)
//JD
#define	SCB_GET_SSP_TAG(SCB)	(asd_le16toh((SCB)->hscb->ssp_task.sas_header.tag))

/* Common attributes for Phy and Port. */
typedef enum {
	ASD_SSP_INITIATOR	= 0x01,
	ASD_SMP_INITIATOR 	= 0x02,
	ASD_STP_INITIATOR 	= 0x04,
	ASD_SATA_SPINUP_HOLD 	= 0x08,
	ASD_SAS_MODE      	= 0x10,
	ASD_SATA_MODE     	= 0x20,
	ASD_DEVICE_PRESENT 	= 0x40 
} asd_attrib;

typedef enum {
	ASD_IDLE	  	= 0x00,
	ASD_RESP_PENDING  	= 0x01,
	ASD_ID_ADDR_RCVD  	= 0x02,
	ASD_DISCOVERY_REQ 	= 0x04,
	ASD_DISCOVERY_PROCESS 	= 0x08,
	ASD_DISCOVERY_EVENT 	= 0x10,
	ASD_VALIDATION_REQ	= 0x20,
	ASD_LOSS_OF_SIGNAL	= 0X40,
	ASD_DISCOVERY_RETRY	= 0X80
} asd_event;

/*
 * Port structure.
 * It is used to reference a SCSI or SATA initiator port for the adapter.
 */
typedef enum {
	ASD_PORT_UNUSED,
	ASD_PORT_ONLINE,
	ASD_PORT_OFFLINE
} asd_port_state;

struct discover_context {
	struct asd_softc	*asd;
	struct asd_port		*port;
	uint8_t			 openStatus;
	unsigned		 resid_len;
//JD
	unsigned		 retry_count;

	/*
	 * -------------------------------------
	 * DMA-able memory used for SMP request
	 */
	struct SMPRequest	*SMPRequestFrame;
	dma_addr_t		 SMPRequestBusAddr;
	bus_dma_tag_t		 smp_req_dmat;
	struct map_node		 smp_req_map;

	/*
	 * -------------------------------------
	 * DMA-able memory used for SMP response
	 */
	struct SMPResponse	*SMPResponseFrame;
	dma_addr_t		 SMPResponseBusAddr;
	bus_dma_tag_t		 smp_resp_dmat;
	struct map_node		 smp_resp_map;

	/*
	 * -------------------------------------
	 * DMA-able memory used for Report LUNS and Unit Serial Number
	 */
	uint8_t			*SASInfoFrame;
	dma_addr_t		 SASInfoBusAddr;
	unsigned		 sas_info_len;
	bus_dma_tag_t		 sas_info_dmat;
	struct map_node		 sas_info_map;

	/*
	 * For state machine
	 * -------------------------------------
	 */
	struct state_machine_context	sm_context;
	struct timer_list		request_timer;
};

struct asd_port {
	struct list_head	 phys_attached;
	struct list_head	 targets;
	struct list_head	 targets_to_validate;
	void			*softc;
	uint8_t			 sas_addr[SAS_ADDR_LEN];
	uint8_t			 hashed_sas_addr[HASHED_SAS_ADDR_LEN];
	uint8_t			 attached_sas_addr[SAS_ADDR_LEN];
	u_char			 num_phys;
	uint8_t			 conn_rate;
	/* 
	 * Bit field indicating which phy id belongs to this port.
	 */  
	uint8_t			 conn_mask;
	uint8_t			 reset_mask;
	u_char			 id;
	asd_port_state		 state;
	asd_attrib		 attr;
	asd_event		 events;
	LINK_TYPE		 link_type;
	MANAGEMENT_TYPE		 management_type;
	struct discover_context	 dc;
	struct asd_target	*tree_root;
};

/*
 * Phy structure.
 * It is used to reference a service delivery subsystem for the adapter.
 */
typedef enum {
	ASD_PHY_UNUSED,
	ASD_PHY_WAITING_FOR_ID_ADDR,
	ASD_PHY_ONLINE,
	ASD_PHY_OFFLINE,
	ASD_PHY_CONNECTED
} asd_phy_state;

#define PRODUCT_SERIAL_NUMBER_LEN	64

struct asd_phy {
	struct list_head	 pending_scbs;
	struct list_head	 links;
	void			*softc;
    	struct asd_port		*src_port;
	uint8_t			 sas_addr[SAS_ADDR_LEN];
	u_int			 max_link_rate;
	u_int			 min_link_rate;
	u_int			 conn_rate;
	uint8_t			 id;
	asd_phy_state 		 state;
	asd_attrib		 attr;
	asd_event		 events;

	/* Phy Error information. */
	uint8_t			 brdcst_rcvd_cnt;

	/* Phy control register info */
	uint8_t			 phy_state;
	uint8_t			 phy_ctl0;
	uint8_t			 phy_ctl1;
	uint8_t			 phy_ctl2;
	uint8_t			 phy_ctl3;

	/* Identify Address frame buffer. */
	bus_dma_tag_t		 id_addr_dmat;
	struct map_node		 id_addr_map;
	union sas_bytes_dmaed	 bytes_dmaed_rcvd;

	u_int			 link_rst_cnt;
#define MAX_LINK_RESET_RETRY	 2

	/* TBD: Temp var, should remove after checking with HW */
	uint8_t			 pat_gen;
};

#define PHY_GET_ID_SAS_ADDR(PHY)	\
	((PHY)->bytes_dmaed_rcvd.id_addr_rcvd.sas_addr)

typedef enum {
	ASD_IO_SPACE,
	ASD_MEMORY_SPACE
} asd_mem_space_type;

struct asd_io_handle {
	union {
		uint8_t		*membase;
		unsigned long	 iobase;
	} baseaddr;
	uint32_t		 bar_base; 
	uint32_t		 length;
	uint32_t		 swb_base;
	uint8_t			 index;		/* BAR index */
	asd_mem_space_type	 type;
};

struct asd_host_profile {
	uint16_t		max_cmds_per_lun;
	uint16_t		can_queue;
	uint8_t			initiator_id;
	uint8_t			max_luns;
	unsigned int		max_scsi_ids;
	uint8_t			max_channels;
	uint8_t			irq;
	uint8_t			unit;
	uint8_t			dma64_addr;
	dma_addr_t		dma_mask;
	char			*name;
};

/*
 * Controller specific information.
 */
struct asd_hw_profile {
	/* HW info. */
	uint8_t			wwn[ASD_MAX_WWN_LEN];
	uint8_t			max_ports;
	uint8_t			ports_allocated;
	uint8_t			max_phys;
	uint8_t			enabled_phys;
	int			rev_id;
	/* Target info. */
	uint16_t		max_devices;
	uint16_t		max_targets;
	uint8_t			max_expanders;
	/* Runtime info. */
	uint16_t		max_scbs;
	uint16_t		max_ddbs;
	/* Queue info. */
	uint16_t		scbpro_cnt;	
	/* NVRAM info */
	uint8_t			flash_present;
	uint32_t		flash_method;
	uint16_t		flash_manuf_id;
	uint16_t		flash_dev_id;
	uint32_t		flash_wr_prot;
	u_char			nv_exist;
	uint32_t		nv_cookie_addr;
	u_int			nv_cookie_found;
	uint32_t		nv_segment_type;
	uint32_t		nv_flash_bar;
	uint32_t		bios_bld_num;
	uint8_t			bios_maj_ver;
	uint8_t			bios_min_ver;

	int   addr_range;
	int   port_name_base;
	int   dev_name_base;
	int   sata_name_base;
};

typedef enum {
	ASD_SCB_RESOURCE_SHORTAGE	= 0x01,
	ASD_RUNNING_DONE_LIST		= 0x02,
	ASD_WAITING_FOR_RESOURCE	= 0x04
} asd_softc_flags;

struct asd_softc {
	/*
	 * The next hardware SCB already known to the sequencer.
	 */
	union hardware_scb	*next_queued_hscb;
	struct map_node		*next_queued_hscb_map;
	uint64_t		 next_queued_hscb_busaddr;
	/*
	 * Pool of SCBs available to execute new commands.
	 */
	struct list_head	 free_scbs;
	/*
	 * Pool of reserved SCBS to be used for error recovery.
         */	 
	struct list_head	 rsvd_scbs;
	/*
	 * SCBs that have been sent to the controller
	 */
	struct list_head	 pending_scbs;
	/*
	 * Mapping from 16bit scb index to SCB.
	 */
	struct scb		**scbindex;
	/*
	 * Platform specific data.
	 */
	struct asd_platform_data *platform_data;
	/* 
	 * Command Queues info
	 */
	struct asd_done_list	*done_list;
	uint16_t		 dl_next;
	uint16_t		 dl_valid;
	uint16_t		 dl_wrap_mask;
	uint16_t		 qinfifonext;
	uint16_t		*qinfifo;
#define	ASD_QIN_WRAP(asd) ((asd)->qinfifonext & (asd->hw_profile.max_scbs - 1))

	/*
	 * Free Device Descriptor Block bitmap.
	 */
	u_long			*free_ddb_bitmap;
	u_int			 ddb_bitmap_size;
#define ASD_INVALID_DDB_INDEX 0xFFFF

	/*
	 * Misc controller state flags.
	 */
	asd_softc_flags		 flags;
	/*
	 * SCBs whose timeout routine has been called.
	 */
	struct list_head	 timedout_scbs;
	/*
	 * SCBs that have been sent to the controller
	 */
	struct list_head	 empty_scbs;
	/*
	 * "Bus" addresses of our data structures.
	 */
	bus_dma_tag_t	 	 shared_data_dmat;
	struct map_node	 	 shared_data_map;
	
	/* dmat for our hardware SCB array */
	bus_dma_tag_t	 	 hscb_dmat;
	/* dmat for our sg segments */
	bus_dma_tag_t	 	 sg_dmat;
	struct list_head 	 hscb_maps;
	struct list_head 	 sg_maps;
	/* Unallocated scbs in head map_node */
	int		 	 scbs_left;
	/* Unallocated sgs in head map_node */
	int			 sgs_left;	
	/* Allocated edbs */
	int		 	 edbs;	
	uint16_t	 	 numscbs;
	/*
	 * Platform specific device information.
	 */
	asd_dev_t		 dev;
	/*
	 * Used to chain all controller instances
	 * managed by this driver.
	 */
	struct list_head	 link;
	struct list_head	*old_discover_listp;
	struct list_head	*discover_listp;
	struct asd_port		*port_list[ASD_MAX_PORTS];
	struct asd_phy		*phy_list[ASD_MAX_PHYS];
	struct asd_host_profile  profile;
	struct asd_hw_profile	 hw_profile;
	const struct pci_device_id *pci_entry;
	struct asd_io_handle	**io_handle;
	struct asd_ctl_internal_data asd_ctl_internal;
	uint32_t		 asd_hba_index;
	uint8_t			 io_handle_cnt;
	uint8_t			 init_level;

	unsigned		 num_discovery;
	unsigned		 num_unit_elements;
	struct asd_unit_element_format	*unit_elements;
#ifdef ASD_DEBUG
	unsigned		 debug_flag;
#endif
#ifdef EXTENDED_SCB
	struct map_node		ext_scb_map;
	bus_dma_tag_t		ext_scb_dmat;
#endif
};


/* Scatter/Gather list Handling */
#define	asd_sglist_size(asd) (ASD_NSEG * sizeof(struct sg_element))


/*
 * Hardware Interface functions prototypes.
 */
void		asd_intr_enable(struct asd_softc *asd, int enable);
int		asd_hwi_init_hw(struct asd_softc *asd);
int		asd_hwi_process_irq(struct asd_softc *asd);
struct asd_softc * asd_alloc_softc(asd_dev_t dev);
void		asd_free_softc(struct asd_softc *asd);

/* 
 * Device Descriptor Block resource management. 
 */
uint16_t	asd_alloc_ddb(struct asd_softc *asd);
void		asd_free_ddb(struct asd_softc *asd, uint16_t ddb_index);
int		asd_hwi_setup_ddb_site(struct asd_softc *asd,
				       struct asd_target *target);

/* 
 * Function prototypes for interfacing with Sequencers. 
 */
int		asd_hwi_pause_cseq(struct asd_softc *asd);
int		asd_hwi_pause_lseq(struct asd_softc *asd, uint8_t lseq_mask);
int		asd_hwi_unpause_cseq(struct asd_softc *asd);
int		asd_hwi_unpause_lseq(struct asd_softc *asd, uint8_t lseq_mask);
int		asd_hwi_download_seqs(struct asd_softc *asd);
void		asd_hwi_setup_seqs(struct asd_softc *asd);
int		asd_hwi_start_cseq(struct asd_softc *asd);
int		asd_hwi_start_lseq(struct asd_softc *asd, uint8_t link_num);
struct scb *	asd_hwi_get_scb(struct asd_softc *asd, int rsvd_pool);
void		asd_hwi_post_scb(struct asd_softc *asd, struct scb *scb);
union edb *	asd_hwi_indexes_to_edb(struct asd_softc *asd, struct scb **pscb,
				       u_int escb_index, u_int edb_index);
void		asd_hwi_free_edb(struct asd_softc  *asd,
				 struct scb *scb, int edb_index);
void		asd_hwi_init_internal_ddb(struct asd_softc *asd);
void		asd_hwi_init_ddb_sites(struct asd_softc *asd);
void		asd_hwi_build_ddb_site(struct asd_softc  *asd, 
				      struct asd_target *target);
void		asd_hwi_update_sata(struct asd_softc *asd, 
				      struct asd_target *target);
void		asd_hwi_update_conn_mask(struct asd_softc *asd,
					 struct asd_target *target);
int		asd_hwi_enable_phy(struct asd_softc *asd, struct asd_phy *phy);
void		asd_hwi_release_sata_spinup_hold(struct asd_softc *asd,
						 struct asd_phy *phy);

/*
 * Error handling function prototypes.
 */
int		asd_hwi_post_eh_scb(struct asd_softc *asd, struct scb *scb);
void		asd_hwi_build_ssp_tmf(struct scb *scb, struct asd_target *targ,
				      uint8_t *lun, u_int tmf_opcode);
void		asd_hwi_build_clear_nexus(struct scb *scb, u_int nexus_ind,
					  u_int parm, u_int context);
void		asd_hwi_build_abort_task(struct scb *scb,
					 struct scb *scb_to_abort);
void		asd_hwi_build_query_task(struct scb *scb, 
					 struct scb *scb_to_query);
void		asd_hwi_build_control_phy(struct scb *scb, struct asd_phy *phy, 
					  uint8_t sub_func);
void		asd_hwi_map_tmf_resp(struct scb *scb, u_int resp_code);

/* SCB build routines */
void 		asd_hwi_build_smp_task(struct scb *scb, struct asd_target *targ,
				       uint64_t req_bus_addr, u_int req_len,
				       uint64_t resp_bus_addr, u_int resp_len);
void 		asd_hwi_build_ssp_task(struct scb *scb, struct asd_target *targ,
					uint8_t *saslun, uint8_t *cdb,
					uint32_t cdb_len, uint8_t addl_cdb_len,
				       	uint32_t data_len);
void 		asd_hwi_build_stp_task(struct scb *scb, struct asd_target *targ,
				       uint32_t data_len);

int 		asd_hwi_get_nv_conn_info(struct asd_softc *asd, uint8_t **pconn,
					 uint32_t *pconn_size, uint8_t **pnoded,
					 uint32_t *pnoded_size);

int 		asd_hwi_control_activity_leds(struct asd_softc *asd,
					      uint8_t phy_id,
					      uint32_t asd_phy_ctl_func);

int		asd_hwi_search_nv_segment(struct asd_softc *asd, 
					  u_int segment_id, uint32_t *offset,
					  uint32_t *pad_size, 
					  uint32_t *image_size, 
					  uint32_t *attr);
int 		asd_hwi_write_nv_segment(struct asd_softc *asd,
					void *src, u_int segment_id, 
					uint32_t dest_offset, 
					uint32_t bytes_to_write);

int asd_hwi_read_nv_segment(struct asd_softc *asd, uint32_t segment_id, 
					void *dest, uint32_t src_offset, 
					uint32_t bytes_to_read,
					uint32_t *bytes_read);

 
/* Debug function prototypes. */
#ifdef ASD_DEBUG
void	asd_hwi_dump_seq_state(struct asd_softc *asd, uint8_t lseq_mask);
#ifdef SEQUENCER_UPDATE
void	asd_hwi_dump_sata_stp_ddb_site(struct asd_softc *asd, u_int site_no);
void	asd_hwi_dump_ssp_smp_ddb_site(struct asd_softc *asd, u_int site_no);
#else
void	asd_hwi_dump_ddb_site(struct asd_softc *asd, u_int site_no);
#endif
#endif
void asd_hwi_dump_ddb_site_raw(struct asd_softc *asd, uint16_t site_no);
void asd_hwi_dump_ddb_sites_raw(struct asd_softc *asd);
void asd_hwi_dump_scb_site_raw(struct asd_softc *asd, uint16_t site_no);
void asd_hwi_dump_scb_sites_raw(struct asd_softc *asd);

#ifdef SEQUENCER_UPDATE
struct asd_step_data {
	struct asd_softc	*asd;
	struct timer_list	single_step_timer;
	unsigned		instruction_count;
	unsigned		lseq_trace;
	unsigned		stepping;
	unsigned		link_num;
};

struct asd_step_data	*asd_hwi_alloc_step(struct asd_softc *asd);
void			asd_hwi_free_step(struct asd_step_data *step_datap);
int			asd_hwi_cseq_init_step(
				struct asd_step_data *step_datap);
int			asd_hwi_lseq_init_step(
				struct asd_step_data *step_datap,
				unsigned link_num);
void			asd_hwi_start_step_timer(
				struct asd_step_data *step_datap);
#endif

void asd_hwi_set_ddbptr(struct asd_softc *asd, uint16_t val);
void asd_hwi_set_cseq_breakpoint(struct asd_softc *asd, uint16_t addr);
#if 0
void asd_hwi_set_lseq_breakpoint(struct asd_softc *asd, int phy_id,
	uint16_t addr);
#else
void asd_hwi_set_lseq_breakpoint(struct asd_softc *asd, int phy_id, 
	uint32_t breakid, uint16_t addr);
#endif

#if ASD_PIO_DOWNLOAD_SEQS
/* PIO Mode */
int		asd_hwi_pio_load_seqs(struct asd_softc *asd, uint8_t *code,
				      uint32_t code_size, uint8_t lseq_mask);
				      
#define ASD_HWI_DOWNLOAD_SEQS(asd, code, code_size, lseq_mask)		\
		asd_hwi_pio_load_seqs(asd, (uint8_t *) code,		\
				     (uint32_t) code_size, (uint8_t) lseq_mask)	
#else 
/* Overlay DMA Mode */
int		asd_hwi_dma_load_seqs(struct asd_softc *asd, uint8_t *code,
				      uint32_t code_size, uint8_t lseq_mask);

#define ASD_HWI_DOWNLOAD_SEQS(asd, code, code_size, lseq_mask)		\
		asd_hwi_dma_load_seqs(asd, (uint8_t *) code,		\
				     (uint32_t) code_size, (uint8_t) lseq_mask)

#endif /* ASD_DMA_DOWNLOAD_SEQS */


/********** External Memory (NVRAM, SEEPROM, etc) strucutres layout ***********/

#define NVRAM_SUPPORT			1

#if NVRAM_SUPPORT

#define NVRAM_RESET			0xF0
#define NVRAM_NO_SEGMENT_ID		0xFF
#define NVRAM_INVALID_SIZE		0xFFFFFFFF
#define NVRAM_COOKIE_SIZE		0x20
#define NVRAM_FIRST_DIR_ENTRY		0x90
#define NVRAM_NEXT_ENTRY_OFFSET		0x2000
#define NVRAM_MAX_ENTRIES		0x20
#define NVRAM_SEGMENT_ATTR_ERASEWRITE	1

/* NVRAM Entry Type Values. */
#define NVRAM_FLASH_DIR			0x00000000
#define NVRAM_BIOS_CORE			0x00000020
#define NVRAM_BIOS_UTILS		0x00000040
#define NVRAM_BIOS_CHIM			0x00000060
#define NVRAM_CTRL_A_SETTING		0x000000E0
#define NVRAM_CTRL_A_DEFAULT		0x00000100
#define NVRAM_MANUF_TYPE		0x00000120


#define SAS_ADDR_WWID_MASK		0xF0

/* OCM directory entry format */
struct asd_ocm_entry_format {
	uint8_t		type;
#define OCM_DE_OCM_DIR			0x00
#define OCM_DE_WIN_DRVR			0x01
#define OCM_DE_BIOS_CHIM		0x02
#define OCM_DE_RAID_ENGN		0x03
#define OCM_DE_BIOS_INTL		0x04
#define OCM_DE_BIOS_CHIM_OSM		0x05
#define OCM_DE_BIOS_CHIM_DYNAMIC	0x06
#define OCM_DE_ADDC2C_RES0		0x07
#define OCM_DE_ADDC2C_RES1		0x08
#define OCM_DE_ADDC2C_RES2		0x09
#define OCM_DE_ADDC2C_RES3		0x0A

	uint8_t		offset[3];
/* retrieves offset/size from the 3 byte array */
#define OCM_DE_OFFSET_SIZE_CONV(x) ((x[2] << 16) | (x[1] << 8) | x[0])

	uint8_t		res1;
	uint8_t		size[3];
} __packed;

/* OCM directory format */
struct asd_ocm_dir_format {
	/* 'O'n-chip memory 'D'irectory */
	uint16_t	signature;	

#define OCM_DIR_SIGN 0x4F4D
	
#if 0
	uint16_t	res1;
#else
	uint8_t		res1a;
	uint8_t		res1b;
#endif
	uint8_t		maj_ver;
	uint8_t		min_ver;
	uint8_t		res2;
	uint8_t		num_entries;
#define OCM_NUM_ENTRIES_MASK	0x0f

	//struct asd_ocm_entry_format entry[1];
} __packed;

#if 0
//TBD: check and remove
struct asd_unit_id_direct_attached_sata {
	uint8_t		phy_id;
	uint8_t		res[15];
} __packed;

struct asd_unit_id_logical_unit {
	uint8_t		sas_address[SAS_ADDR_LEN];
	uint8_t		lun[8];
} __packed;

struct asd_unit_id_volume_set {
	uint8_t		volume_set_uid[8];
	uint8_t		res[8];
} __packed;

/* unit element */
struct asd_unit_element_format {
	uint8_t		id[16];
#define UNIT_ELEMENT_DIRECT_ATTACHED_SATA	0x00
#define UNIT_ELEMENT_LOGICAL_UNIT		0x01
#define UNIT_ELEMENT_VOLUME_SET			0x02
	uint8_t		type;
	uint8_t		res1;
	uint8_t		dos_drv_num;
	uint8_t		removable;
	uint32_t	res2;
} __packed;
#endif

struct asd_uid_lu_naa_wwn {
	uint8_t		naa;
	/* length specified by unit_id_len */
} __packed;

struct asd_uid_lu_sata_model_num {
	uint32_t	id; /* "ata." */
#define UNITID_SATA_ID 0x6174612E

	uint8_t		model[40];
	uint8_t		serial[20];
	uint32_t	null_data;
	/* length specified by unit_id_len */
} __packed;


struct asd_uid_vol_set {
#define UNITID_VOLUME_SET_UID_LEN	24
	uint8_t		vol_set_uid[UNITID_VOLUME_SET_UID_LEN];
	/* length specified by unit_id_len */
} __packed;

struct asd_uid_lu_sgpio {
	uint8_t		conn_idx;
	uint8_t		box_id;
	uint8_t		bay_id;
	uint32_t	res1[5];
	uint8_t		lun[8];
	/* length specified by unit_id_len */
} __packed;

struct asd_uid_lu_sas_topology {
	uint8_t		exp_dev_name[8];
	uint8_t		lun[8];
	uint8_t		phy_id;
	/* length specified by unit_id_len */
} __packed;

/* unit element */
struct asd_unit_element_format {
	uint8_t		id;
#define UNITELEM_LU_NAA_WWN		0x00
#define UNITELEM_LU_SATA_MOD_SERIAL_NUM	0x01
#define UNITELEM_VOLUME_SET_DDF_GUID	0x02
#define UNITELEM_LU_SGPIO		0x03
#define UNITELEM_LU_SAS_TOPOLOGY	0x04

	uint8_t		dos_drv_num;
	uint8_t		removable_unit;
#define UNITELEM_REMOVABLE_UNIT_MASK 	0x01

	uint32_t	res1;
	uint8_t		unit_id_len;
	uint8_t		res2[8];
} __packed;

/* BIOS CHIM section */
struct asd_bios_chim_format {
	uint32_t	signature;	/* 'BIOS' or 'ASPI' */
#define OCM_BC_BIOS_SIGN 0x42494F53
#define OCM_BC_ASPI_SIGN 0x41535049

	uint8_t		maj_ver;	/* 0x01 */
	uint8_t		min_ver;	/* 0x00 */
	uint8_t		bios_maj_ver;
	uint8_t		bios_min_ver;
	uint32_t	bios_bld_num;
	uint8_t		bios_present;
#define OCM_BC_BBS_MASK		0x04
#define OCM_BC_BIOS_INSLD_MASK	0x02
#define OCM_BC_BIOS_PRSNT_MASK	0x01
	
	uint8_t		pci_slot_num;
	uint16_t	num_elements;
	uint16_t	size_unit_elem;
	uint8_t		res1[14];	
	struct asd_unit_element_format unit_elm[1];
} __packed;

/* 
 * These macros take a 8 byte base SAS address and get/set 
 * it's constituent elements. 
 */

/* Default: 0x5 */
#define ASD_BSAR_GET_NAA(x) 	((x[0] >> 4) & 0x0f)

/* Default: 0x0000D1 - 3 byte IEEE Id assigned to Adaptec */
#define ASD_BSAR_GET_IEEE_ID(x) \
(((x[0] & 0x0f) << 20) | (x[1] << 12) | (x[2] << 4) | ((x[3] >> 4) & 0x0f)) 

/* get the 4 byte Razor serial number */
#define ASD_BSAR_GET_SN(x) \
(((x[3] & 0x0f) << 28) | (x[4] << 20) | (x[5] << 12) \
| (x[6] << 4) | (((x[7] >> 5) & 0x07) << 1)) 

/*Default: 00000b - configurable using Ctrl-A */
#define ASD_BSAR_GET_LSB(x) 	(x[7] & 0x1f)
#define ASD_BSAR_SET_LSB(x, y) 	(x[7] = ((x[7] & 0x1f) & (y & 0x1f)))

struct asd_manuf_seg_sign {
	uint16_t	signature;
#define NVRAM_MNF_BASE_SEG_SIGN	 0x4D53	/* 'M'anufacturing 'S'egment */
#define NVRAM_MNF_PHY_PARAM_SIGN 0x4D50	/* 'M'anufacturing 'P'hy Parameters */
#define NVRAM_MNF_CONN_MAP	 0x434D	/* 'C'onnector 'M'ap */

	uint16_t	next_seg_ptr;
} __packed;

/* Manufacturing Base Segment Layout. */
struct asd_manuf_base_seg_layout {
	struct asd_manuf_seg_sign seg_sign;
	uint8_t		major_ver;	/* 0x00 */
	uint8_t		minor_ver;	/* 0x00 */
	uint16_t	checksum;
	uint16_t	sector_size;
	uint8_t		res1[6];
	uint8_t		base_sas_addr[8];
	uint8_t		pcba_sn[12];
} __packed;

/* Phy Descriptor sub structure */
struct asd_phy_desc_format {
	uint8_t		state;
#define NVRAM_PHY_ENABLEABLE		0x00
#define NVRAM_PHY_REPORTED		0x01
#define NVRAM_PHY_HIDDEN		0x02
#define PHY_STATE_MASK			0x0F
#define PHY_STATE_DEFAULT		NVRAM_PHY_ENABLEABLE

	uint8_t		phy_id;
	uint16_t	res1;
	uint8_t		ctl0;
#define PHY_CTL0_DEFAULT		0xF6
#define PHY_CTL0_MASK			0x1C
	
	uint8_t		ctl1;
#define PHY_CTL1_DEFAULT		0x10

	uint8_t		ctl2;
#define PHY_CTL2_DEFAULT		0x43

	uint8_t		ctl3;
#define PHY_CTL3_DEFAULT		0x83

} __packed;

/* Manufacturing Phy Parameters Layout */
struct asd_manuf_phy_param_layout {
	struct asd_manuf_seg_sign seg_sign;
	uint8_t		major_ver;	/* 0x00 */
	uint8_t		minor_ver;	/* 0x02 */
	uint8_t		num_phy_desc;	/* 0x08 */
	uint8_t		phy_desc_size;	/* 0x08 */
	uint8_t		res1[3];
	uint8_t		usage_model_id;
	uint8_t		res2[4];
} __packed;


/* Phy Descriptor Unattached Layout */
struct asd_pd_ua_format {
	uint16_t	res1;
	uint8_t		rel_phy_id;
	/* size is specified by the node_desc format */
} __packed;

/* Phy Descriptor connector attached Layout */
struct asd_pd_ca_format {
	uint8_t		idx;
	uint8_t		lane;
	uint8_t		rel_phy_id;
	/* size is specified by the node_desc format */
} __packed;

/* Phy Descriptor node attached Layout */
struct asd_pd_na_format {
	uint8_t		idx;
	uint8_t		att_phy_id;
	uint8_t		rel_phy_id;
	/* size is specified by the node_desc format */
} __packed;

/* Node descriptor's Phy descriptor entries */
struct asd_nd_phydesc_format {
	uint8_t		type;
#define NV_PD_UATTACH		0x00
#define NV_PD_CONATTACH		0x01
#define NV_PD_NODEATTACH	0x02

	union {
		struct asd_pd_ua_format uatt;
		struct asd_pd_ca_format catt;
		struct asd_pd_na_format natt;
	}att_type;
} __packed;

/* Node Descriptor Layout */
struct asd_node_desc_format {
	uint8_t		type;
#define NV_NODET_IOP		0x00
#define NV_NODET_IOCTLR		0x01
#define NV_NODET_EXPANDER	0x02
#define NV_NODET_PORT_MUL	0x03
#define NV_NODET_PORT_MUX	0x04
#define NV_NODET_PORT_MDI2C_BUS	0x05

	uint8_t		num_phy_desc;
	uint8_t		phy_desc_size;
	uint8_t		res1;

#define NV_NODED_INST_NAME_SIZE 16
	uint8_t		inst_name[NV_NODED_INST_NAME_SIZE];
} __packed;

/* SideBand Descriptor Layout */
struct asd_sb_desc_format {
	uint8_t		type;
#define NV_SBT_UNKNOWN		0x00
#define NV_SBT_SGPIO		0x01
#define NV_SBT_ADPTI2C		0x80
#define NV_SBT_VEN_UNQ01	0x81
#define NV_SBT_VEN_UNQ7F	0xFF

	uint8_t		node_desc_idx;
	uint8_t		conn_desc_idx;
	uint8_t		res1;
	/* size is specified by the conn_desc format */
} __packed;

/* Connector Descriptor Layout */
struct asd_conn_desc_format {
	uint8_t		type;
#define NV_CONNT_UNKNOWN	0x00
#define NV_CONNT_SFF8470	0x08
#define NV_CONNT_SFF8482	0x09
#define NV_CONNT_SFF8484	0x0A
#define NV_CONNT_PCIX_D0	0x80
#define NV_CONNT_SAS_D0		0x81
#define NV_CONNT_VEN_UNQ02	0x82
#define NV_CONNT_VEN_UNQ7F	0xFF

	uint8_t		location;
#define NV_CONNL_UNKNOWN	0x00
#define NV_CONNL_INTERNAL	0x01
#define NV_CONNL_EXTERNAL	0x02
#define NV_CONNL_B2B		0x03

	uint8_t		num_sb_desc;
	uint8_t		sb_desc_size;
	uint32_t	res1;

#define NV_CONND_INST_NAME_SIZE 16
	uint8_t		inst_name[NV_CONND_INST_NAME_SIZE];
} __packed;

/* Connector Map Layout */
struct asd_conn_map_format {
	struct asd_manuf_seg_sign seg_sign;
	uint8_t		major_ver;	/* 0x00 */
	uint8_t		minor_ver;	/* 0x00 */
	uint16_t	size;
	uint8_t		num_conn_desc;
	uint8_t		conn_desc_size;
	uint8_t		num_node_desc;
	uint8_t		usage_model_id;
	uint32_t	res1;
} __packed;

struct asd_settings_layout {
	uint8_t		id;
#define NVRAM_PHY_SETTINGS_ID   	0x68
#define NVRAM_BOOT_UNIT_SETTINGS_ID 	0x42
#define NVRAM_BOOTLUN_SETTINGS_ID 	0x4c

	uint8_t		res;
	uint16_t	next_ptr;
} __packed;


/*
 * The size of Flash directory structure must be 64 bytes for backward
 * compatibility.
 */
struct asd_fd_entry_layout {
	uint32_t	attr;
#define FD_ENTRYTYPE_CODE	0x3FFFFFFF
#define FD_SEGMENT_ATTR		0x40000000

	uint32_t	offset;
	uint32_t	pad_size;
	uint32_t	image_size;
	uint32_t	checksum;
	uint8_t		res[12];
	uint8_t		ver_data[32];
} __packed;

struct asd_flash_dir_layout {
	
	uint8_t		cookie[32];	/* "*** ADAPTEC FLASH DIRECTORY *** " */
	uint32_t	rev;		/* 0x00000002 */
	uint32_t	wchk_sum;
	uint32_t	wchk_sum_antidote;
	uint32_t	build_num;
	uint8_t		build_id[32];
	uint8_t		ver_data[32];
	uint32_t	ae_mask;
	uint32_t	v_mask;
	uint32_t	oc_mask;
	uint8_t		res1[20];
	//struct asd_fd_entry_layout entry[1];
} __packed;

struct asd_phy_settings_entry_layout {
	uint8_t		sas_addr[SAS_ADDR_LEN];
	uint8_t		link_rate;		/*
						 * bits 0-3: minimum link rate
						 * bits 4-7: maximum link rate
						 */
#define PHY_MIN_LINK_RATE_15	0x08
#define PHY_MIN_LINK_RATE_30	0x09
#define PHY_MIN_LINK_RATE_MASK	0x0F

#define PHY_MAX_LINK_RATE_15	0x80
#define PHY_MAX_LINK_RATE_30	0x90
#define PHY_MAX_LINK_RATE_MASK	0xF0

	uint8_t		attr;			/*
						 * bit 0: disable CRC checking
						 * bit 1: SATA spinup hold
						 */
	uint8_t		res1[6];
} __packed;
    
struct asd_phy_settings_layout {
	struct asd_settings_layout settings;
	uint8_t		num_phys;
	uint8_t		res2[3];
	//struct asd_phy_settings_entry_layout entry[1];
} __packed;

struct asd_boot_lu_layout {
	uint8_t		serial_no[4];		/* Adapter serial number */
	/* Boot device's SAS address */
	uint8_t		dev_port_sas_addr[SAS_ADDR_LEN];
	uint8_t		lun[8];			/* Boot device LUN */
	uint8_t		phy_id;			/*
						 * Phy ID of the attached boot
						 * device (direct attached
						 * SATA only).
						 */
	uint8_t		res1;
	uint16_t	attr;
	uint8_t		conn_rate;		/*
						 * bit   0: force connection
						 * bit 4-7: connection rate
						 */
#define BOOT_FORCE_CONN 	0x01
#define BOOT_CONN_RATE_15	0x80
#define BOOT_CONN_RATE_30	0x90
#define BOOT_CONN_RATE_MASK	0xF0

	uint8_t		res2[7];
} __packed;
    
struct asd_pci_layout {
	uint8_t		id;
	uint8_t		res;
	uint16_t	next_ptr;
	uint8_t		major_ver;
	uint8_t		minor_ver;
	uint16_t	checksum;
	uint16_t	image_size;
} __packed;

/************************ Flash part related macros ***********************/

enum {
	FLASH_METHOD_UNKNOWN,
	FLASH_METHOD_A,
	FLASH_METHOD_B
};

#define FLASH_MANUF_ID_AMD		0x01
#define FLASH_MANUF_ID_ST		0x20
#define FLASH_MANUF_ID_FUJITSU		0x04
#define FLASH_MANUF_ID_MACRONIX		0xC2
#define FLASH_MANUF_ID_UNKNOWN		0xFF

#define FLASH_DEV_ID_AM29LV008BT	0x3E
#define FLASH_DEV_ID_AM29LV800DT	0xDA
#define FLASH_DEV_ID_STM29W800DT	0xD7
#define FLASH_DEV_ID_MBM29LV800TE	0xDA
#define FLASH_DEV_ID_MBM29LV008TA	0x3E
#define FLASH_DEV_ID_AM29LV640MT	0x7E
#define FLASH_DEV_ID_MX29LV800BT	0xDA
#define FLASH_DEV_ID_UNKNOWN		0xFF

/* status bit mask values */
#define FLASH_STATUS_BIT_MASK_DQ6	0x40
#define FLASH_STATUS_BIT_MASK_DQ5	0x20
#define FLASH_STATUS_BIT_MASK_DQ2	0x04

/* minimum value in micro seconds needed for checking status */
#define FLASH_STATUS_ERASE_DELAY_COUNT	50
#define FLASH_STATUS_WRITE_DELAY_COUNT	25

#endif /* NVRAM_SUPPORT */

#endif /* ADP94XX_HWI_H */

