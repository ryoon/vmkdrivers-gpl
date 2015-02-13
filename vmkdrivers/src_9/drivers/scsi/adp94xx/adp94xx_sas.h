/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 * Serial Attached SCSI (SAS) definitions.
 *
 * Written by : David Chaw <david_chaw@adaptec.com>
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
 * $Id: //depot/razor/linux/src/adp94xx_sas.h#51 $
 * 
 */	

#ifndef ADP94XX_SAS_H
#define ADP94XX_SAS_H


/* 
 * Connection Rate.
 */
#define SAS_60GBPS_RATE			600000000	
#define SAS_30GBPS_RATE			300000000
#define SAS_15GBPS_RATE			150000000
#define SAS_INVALID_RATE		0xFFFFFFFF

/* Connection Rate to be used in OPEN address frame. */
#define SAS_RATE_30GBPS			0x09
#define SAS_RATE_15GBPS			0x08

#define SAS_ADDR_LEN			8
#define HASHED_SAS_ADDR_LEN		3

/*
 * SAS SCB site common definitions.
 */
#define SCB_SITE_Q_NEXT			0x0
#define SCB_SITE_FLAGS			0x49

/*
 * SG Element Format (16 bytes).
 */
struct sg_element {
	uint64_t 	bus_address;
	/*
	 * length and next_sg_offset
	 * are unused for link_element
	 * S/G entries.  These entries
	 * just serve as a pointer to
	 * the next S/G sublist to process.
	 */
	uint32_t 	length;
	uint8_t	 	res_1[2];
	uint8_t	 	next_sg_offset;
#define SG_NEXT_OFFSET	0x20
	uint8_t	 	flags;
#define	SG_EOS		0x80
#define	SG_EOL		0x40
#define	SG_NO_DATA	(SG_EOS|SG_EOL)
#define	SG_DS_MASK	0x30
#define	SG_DS_ONCHIP	0x10
#define	SG_DS_OFFCHIP	0x00
} __packed;


/* 
 * Protocol type definitions.
 */
#define PROTOCOL_TYPE_SSP	(1U << 4)
#define PROTOCOL_TYPE_SATA	(1U << 5)
#define PROTOCOL_TYPE_SMP	(0U << 6)
#define PROTOCOL_TYPE_STP	PROTOCOL_TYPE_SATA
#define PROTOCOL_TYPE_MASK	0x70

/*
 * Data Direction definitions.
 */
#define DATA_DIR_NO_XFER	(0U << 1)
#define DATA_DIR_INBOUND	(1U << 0)	
#define DATA_DIR_OUTBOUND	(1U << 1)
#define DATA_DIR_UNSPECIFIED	(3U << 0)	

/*
 * Task Management Functions.
 */
#define ABORT_TASK_TMF		0x01
#define ABORT_TASK_SET_TMF	0x02
#define CLEAR_TASK_SET_TMF	0x04
#define LOGICAL_UNIT_RESET_TMF	0x08
#define CLEAR_ACA_TMF		0x40
#define QUERY_TASK_TMF		0x80

/*
 * NOTE: Unless specified, all fields should be filled with Little Endian 
 *	 format.
 */
 
/* SCB Common Header (11 bytes) */
struct hscb_header {
	uint64_t 		next_hscb_busaddr;
	uint16_t 		index;
	uint8_t	 		opcode;
} __packed;

/* SAS Frame Header (24 bytes) - All fields are in "wire" format (Big Endian) */
struct asd_sas_header {
	uint8_t	 		frame_type;
#define ID_ADDR_FRAME		0x00
#define OPEN_ADDR_FRAME		0x01
#define XFER_RDY_FRAME		0x05
#define COMMAND_FRAME		0x06
#define RESPONSE_FRAME		0x07
#define TASK_FRAME		0x16
	
	uint8_t  		hashed_dest_sasaddr[HASHED_SAS_ADDR_LEN];
	uint8_t  		res;
	uint8_t  		hashed_src_sasaddr[HASHED_SAS_ADDR_LEN];
	uint8_t	 		res1[2];
	uint8_t	 		retransmit;
	uint8_t	 		num_fill_bytes;
	uint8_t	 		res2[4];
	uint16_t 		tag;
	uint16_t 		target_port_xfer_tag;
	uint32_t 		data_offset;
} __packed;

/* SAS SSP Task Information Unit (28 bytes) */
struct asd_ssp_task_iu {
	uint8_t			lun[8];
	uint16_t		res1;
	uint8_t			tmf;
	uint8_t			res2;
	uint16_t		tag_to_manage;
	uint8_t			res3[14];
} __packed;

/* SAS SSP Response Information Unit (24 byts + response data + sense data) */
struct resp_data_iu {
	uint8_t			res[3];
	uint8_t			resp_code;
#define TMF_COMPLETE		0x00
#define INVALID_FRAME		0x02
#define TMF_NOT_SUPPORTED	0x04
#define TMF_FAILED		0x05
#define TMF_SUCCEEDED		0x08
#define INVALID_LUN		0x09
} __packed;

struct ssp_resp_iu {
	uint8_t			res_1[10];
	uint8_t			datapres;
#define	SSP_RIU_DATAPRES_MASK		0x3
#define		SSP_RIU_DATAPRES_NONE	0x0
#define		SSP_RIU_DATAPRES_RESP	0x1
#define		SSP_RIU_DATAPRES_SENSE	0x2
#define	SSP_RIU_DATAPRES(riu)		\
	((riu)->datapres & SSP_RIU_DATAPRES_MASK)

	uint8_t			status;
	uint8_t			res_2[4];
	uint8_t			sense_len[4];
	uint8_t			response_len[4];
	uint8_t			data[0]; /* Place Holder */
};

struct ssp_resp_frame {
	struct asd_sas_header		sas_header;
	struct ssp_resp_iu		riu;
} __packed;
#define	SSP_RIU_DATA_OFFSET(riu)		\
	(offsetof(struct asd_ssp_response_iu, data))
#define	SSP_RIU_RESPONSE_CODE(riu) (scsi_4btoul((riu)->data) & 0xFF)

/* Retry count used by sequencer for SSP, ATA, and ATAPI task. */
#define TASK_RETRY_CNT		3

/* Suspend data transmission, valid for ABORT TASK and INITIATE SSP TMF. */
#define SUSPEND_DATA				(1U << 2)
#define OVERRIDE_I_T_NEXUS_LOSS_TIMER		(1U << 3)

/*
 * SCB Opcode field
 */
#define SCB_INITIATE_SSP_TASK		0x00 
#define SCB_INITIATE_LONG_SSP_TASK	0x01
#ifdef SEQUENCER_UPDATE
#define SCB_INITIATE_BIDIR_SSP_TASK	0x02
#endif
#define SCB_ABORT_TASK			0x03
#define SCB_INITIATE_SSP_TMF		0x04
#ifdef SEQUENCER_UPDATE
#define SCB_SSP_TARG_GET_DATA		0x05
#define SCB_SSP_TARG_GET_DATA_SND_GOOD	0x06
#define SCB_SSP_TARG_SND_RESP		0x07
#define SCB_QUERY_SSP_TASK		0x08
#endif
#define SCB_INITIATE_ATA_TASK		0x09
#define SCB_INITIATE_ATAPI_TASK		0x0A
#define SCB_CONTROL_ATA_DEVICE		0x0B
#define SCB_INITIATE_SMP_TASK		0x0C
#ifdef SEQUENCER_UPDATE
#define SCB_SMP_TARG_SND_RESP		0x0F
#define SCB_SSP_TARG_SND_DATA		0x40
#define SCB_SSP_TARG_SND_DATA_SND_GOOD	0x41
#endif
#define SCB_CONTROL_PHY			0x80
#ifdef SEQUENCER_UPDATE
#define SCB_SEND_PRIMITIVE		0x81
#endif
#define SCB_LINK_ADMIN_TASK		0x82
#define SCB_EMPTY_BUFFER		0xC0
#ifdef SEQUENCER_UPDATE
#define SCB_ESTABLISH_ICL_TARG_WINDOW	0xC1
#endif
#define SCB_COPY_MEMORY			0xC3
#define SCB_CLEAR_NEXUS			0xC4
#ifndef SEQUENCER_UPDATE
#define SCB_DELIVER_FREE_INIT_PORT_DDBS	0xC5
#endif
#ifdef SEQUENCER_UPDATE
#define SCB_INIT_DDB_ADMIN_TASK		0xC6
#endif
#define SCB_ESTABLISH_NEXUS_EMPTY_SCB	0xD0

/* 
 * Initiate SSP TASK to a SAS target. 
 */
/* SCB_INITIATE_SSP_TASK */
#define SCB_EMBEDDED_CDB_SIZE	16
struct asd_ssp_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t 		xfer_len;
	struct asd_sas_header	sas_header;
	uint8_t	 		lun[8];
	uint8_t	 		res1;
	uint8_t	 		task_attr;
	uint8_t	 		task_mgmt_func;
	uint8_t	 		addl_cdb_len;
	uint8_t			cdb[SCB_EMBEDDED_CDB_SIZE];
	uint16_t 		sister_scb;
	uint16_t 		conn_handle;
	uint8_t	 		data_dir_flags;
	uint8_t			res2;
	uint8_t			retry_cnt;
	uint8_t	 		res3[5];
	
	/* Embedded SG Elements (0-2).  Each SG Element is 16 bytes. */
	struct sg_element	sg_elements[3];
} __packed;

#define LAST_SSP_HSCB_FIELD	res3[0]

/*
 * Initiate Long SSP Task to a SAS target.
 */
/* SCB_INITIATE_LONG_SSP_TASK */
struct asd_ssp_long_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t 		xfer_len;
	struct asd_sas_header	sas_header;
	uint8_t	 		lun[8];
	uint8_t	 		res1;
	uint8_t	 		task_attr;
	uint8_t	 		res2;
	uint8_t			addl_cdb_len;
	uint64_t		long_cdb_busaddr;
	uint32_t		long_cdb_size;
	uint8_t			res3[3];
	uint8_t			long_cdb_ds;
	uint16_t 		sister_scb;
	uint16_t 		conn_handle;
	uint8_t	 		data_dir_flags;
	uint8_t	 		res4;
	uint8_t			retry_cnt;
	uint8_t	 		res5[5];
	struct sg_element	sg_elements[3];
} __packed;

/* 
 * Initiate Abort for an SCB previously sent to CSEQ.
 * This SCB_ABORT_TASK shall be used only to abort an SCB with an opcode of
 * 	- SCB_INITIATE_SSP_TASK
 * 	- SCB_INITIATE_LONG_SSP_TASK
 * 	- SCB_INITIATE_BIDIR_SSP_TASK
 * 	- SCB_INITIATE_SMP_TASK
 * 	- SCB_INITIATE_ATA_TASK
 * 	- SCB_INITIATE_ATAPI_TASK
 */
/* SCB_ABORT_TASK */
struct asd_abort_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t 		res1;
	struct asd_sas_header	sas_header;

	/*
 	 * SSP Task IU field shall ony be valid when aborting an SCB with 
	 * PROTOCOL field of SSP.
	 */
	struct asd_ssp_task_iu	task_iu;
	uint16_t 		sister_scb;
	uint16_t 		conn_handle;
	uint8_t	 		suspend_data;
	uint8_t			res2;
	uint8_t			retry_cnt;
	uint8_t	 		res3[5];
	uint16_t		tc_to_abort;
#ifdef SEQUENCER_UPDATE
	uint16_t		i_t_nexus_timer_constant_override;
	uint8_t			res4[44];
#else
	uint8_t			res4[46];
#endif
} __packed;

/*
 * Initiate an SSP TASK information unit for a LOGICAL UNIT RESET, 
 * ABORT TASK SET, CLEAR TASK SET, or CLEAR ACA task management function.
 */
/* SCB_INITIATE_SSP_TMF */
struct asd_ssp_tmf_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t 		res1;
	struct asd_sas_header	sas_header;
	struct asd_ssp_task_iu	task_iu;
	uint16_t 		sister_scb;
	uint16_t 		conn_handle;
	uint8_t			suspend_data;
	uint8_t			res2;
	uint8_t			retry_cnt;
	uint8_t			res3[5];
#ifdef SEQUENCER_UPDATE
	uint8_t			res4[2];
	uint16_t		i_t_nexus_timer_constant_override;
	uint8_t			res5[44];
#else
	uint8_t			res4[48];
#endif
} __packed;

/*
 * Initiate Query SSP Task.
 */
#define SCB_QUERY_SSP_TASK		0x08
struct asd_query_ssp_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t 		res1;
	struct asd_sas_header	sas_header;
	struct asd_ssp_task_iu	task_iu;
	uint16_t 		sister_scb;
	uint16_t 		conn_handle;
#ifdef SEQUENCER_UPDATE
	uint8_t			override_i_t_nexus;
	uint8_t 		res2;
#else
	uint16_t 		res2;
#endif
	uint8_t			retry_cnt;
	uint8_t	 		res3[5];
	uint16_t		tc_to_query;
#ifdef SEQUENCER_UPDATE
	uint16_t		i_t_nexus_timer_constant_override;
	uint8_t			res4[44];
#else
	uint8_t			res4[46];
#endif
} __packed;

/*
 * Initiate ATA Task to a SATA target.
 */
/* SCB_INITIATE_ATA_TASK */
struct asd_ata_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t		xfer_len;
	uint8_t			host_to_dev_reg_fis[20];
	uint32_t 		data_offset;
	uint8_t			res1[16];
	uint8_t			res2[12];
	uint16_t		sister_scb;
	uint16_t 		conn_handle;
	uint8_t			ata_flags;
#define CSMI_TASK		(1U << 6)
#define DMA_XFER_MODE		(1U << 4)
#define UNTAGGED		(0U << 3)
#ifdef SEQUENCER_UPDATE
#define NATIVE_QUEUING		(1U << 3)
#else
#define LEGACY_QUEUING		(1U << 3)
#define NATIVE_QUEUING		(3U << 2)
#endif

	uint8_t			res3;
	uint8_t			retry_cnt;
	uint8_t			res4;
#define STP_AFFILIATION_POLICY	0x20
#define SET_AFFILIATION_POLICY	0x10
#ifdef SEQUENCER_UPDATE
#define RETURN_PARTIAL_SG_LIST	0x02
#endif
	uint8_t			affiliation_policy;	/*
							 * Bit 4 is Set 
							 * Affiliation Policy
							 * and Bit 5 is STP
							 * Affiliation Policy.
							 */
	uint8_t			res5[3];
	struct sg_element	sg_elements[3]; 
} __packed;

/*
 * Initiate ATAPI Task to an ATAPI target.
 */
/* SCB_INITIATE_ATAPI_TASK */
struct asd_atapi_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint32_t		xfer_len;
	uint8_t			host_to_dev_reg_fis[20];/* Packet Command */
	uint32_t 		data_offset;
	uint8_t			atapi_packet[SCB_EMBEDDED_CDB_SIZE];
	uint8_t			res1[12];
	uint16_t		sister_scb;
	uint16_t 		conn_handle;
	uint8_t			ata_flags;		/* 
							 * ATA flags definition 
							 * is similar to 
							 * INITIATE_ATA_TASK
							 * except bits 2 and 3
							 * (QUEUEING TYPE) 
							 * should be set 0 
							 * (UNTAGGED).
							 */ 
	uint8_t			res2;
	uint8_t			retry_cnt;
	uint8_t			res3;
	uint8_t			affiliation_policy;	/*
							 * Bit 4 is Set 
							 * Affiliation Policy
							 * and Bit 5 is STP
							 * Affiliation Policy.
							 */
	uint8_t			res4[3];
	struct sg_element	sg_elements[3]; 
} __packed;
 
/*
 * Initiate an ATA device control function (e.g. software reset).
 */
/* SCB_CONTROL_ATA_DEVICE */
struct asd_control_ata_dev_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint8_t			res1[4];
	uint8_t			host_to_dev_reg_fis[20];
	uint8_t			res2[32];
	uint16_t		sister_scb;
	uint16_t 		conn_handle;
	uint8_t			ata_flags;		/* Must be set to 0. */
	uint8_t			res3[55];	
} __packed;

/*
 * Initiate an SMP task to an expander.
 */
/* SCB_INITIATE_SMP_TASK */
struct asd_smp_task_hscb {
	struct hscb_header	header;
	uint8_t	 		protocol_conn_rate;
	uint8_t			res1[40];
	uint64_t		smp_req_busaddr;
	uint32_t		smp_req_size;
	uint8_t			res2[3];
	uint8_t			smp_req_ds;
	uint16_t		sister_scb;
	uint16_t 		conn_handle;
	uint8_t			res3[8];
	uint64_t		smp_resp_busaddr;
	uint32_t		smp_resp_size;
	uint8_t			res4[3];
	uint8_t			smp_resp_ds;
	uint8_t			res5[32];
} __packed;

/*
 * Control the operation of a PHY.
 */
/* SCB_CONTROL_PHY */
struct asd_control_phy_hscb {
	struct hscb_header	header;
	uint8_t			phy_id;
	uint8_t			sub_func;
#define DISABLE_PHY		0x00
#define ENABLE_PHY		0x01
#define RELEASE_SPINUP_HOLD	0x02
#define ENABLE_PHY_NO_SAS_OOB	0x03
#define ENABLE_PHY_NO_SATA_OOB	0x04
#define PHY_NO_OP		0x05
#define EXECUTE_HARD_RESET	0x81			
	
	/*
	 * OOB FUNCTION_MASK, SPEED_MASK, and HOT_PLUG_DELAY only valid for 
	 * sub-functions ENABLE_PHY and EXECUTE_HARD_RESET.
	 */
	uint8_t			func_mask;
	uint8_t			speed_mask;
	uint8_t			hot_plug_delay;
	uint8_t			port_type;
#define SSP_INITIATOR_PORT	(1U << 7)	
#define STP_INITIATOR_PORT	(1U << 6)
#define SMP_INITIATOR_PORT	(1U << 5)
	
#ifdef SEQUENCER_UPDATE
#define DEVICE_PRESENT_TIMER_OVERIDE_ENABLE	0x01
#define DISABLE_PHY_IF_OOB_FAILS		0x02
	uint8_t			device_present_timer_ovrd_enable;
	uint32_t		device_present_timeout_const_override;
	uint8_t			link_reset_retries;
	uint8_t			res1[47];
#else
	uint8_t			ovrd_cominit_timer;
	uint32_t		cominit_timer_const_ovrd;
	uint8_t			res1[48];
#endif
	uint16_t		conn_handle;
	uint8_t			res2[56];
} __packed;

/*
 * Initiate an administrative task to the particular link.
 */
/* SCB_LINK_ADMIN_TASK */
struct asd_link_admin_hscb {
	struct hscb_header	header;
	uint8_t			phy_id;
	uint8_t			sub_func;
#define GET_LINK_ERR_CNT	0x00
#define RESET_LINK_ERR_CNT	0x01
#define EN_NOTIFY_SPINUP_INTS	0x02

	uint8_t			res1[57];
	uint16_t		conn_handle;
	uint8_t			res[56];
} __packed;

/* 
 * Empty Buffer Element format (16 bytes). 
 */
struct empty_buf_elem {
	uint64_t		busaddr;
	uint32_t		buffer_size;
	uint8_t			res[3];
	uint8_t			elem_valid_ds;
#define ELEM_BUFFER_VALID_MASK	0xC0
#define ELEM_BUFFER_VALID	0x00
#define ELEM_BUFFER_INVALID	0xC0
#define ELEM_BUFFER_VALID_FIELD(ebe)	\
	((ebe)->elem_valid_ds & ELEM_BUFFER_VALID_MASK)
} __packed;

/* 
 * Provide Empty Data Buffers for Sequencer.
 */
/* SCB_EMPTY_BUFFER */
struct asd_empty_hscb {
	struct hscb_header	header;
	uint8_t			num_valid_elems;
	uint8_t			res1[4];
	struct empty_buf_elem	buf_elem[7];	
} __packed;


/*
 * Initiate a copy between two bus addresses using on-chip DMA engine.
 */
/* SCB_COPY_MEMORY */
struct asd_copy_mem_hscb {
	struct hscb_header	header;
	uint8_t			res1;
	uint16_t		xfer_len;
	uint8_t			res2[2];
	uint64_t		src_bus_addr;
	uint8_t			src_sg_flags;
	uint8_t			res3[45];
	uint16_t		conn_handle;
	uint8_t			res4[8];
	uint64_t		dst_bus_addr;
	uint8_t			dst_sg_flags;
} __packed;

/*
 * Initiate a request that a set of transactions pending for a specified nexus
 * be retun to driver via DL entries and the SCB sites (and tag number) be 
 * returned to the free list of SCBs.
 */
/* SCB_CLEAR_NEXUS */
struct asd_clear_nexus_hscb {
	struct hscb_header	header;
	uint8_t			nexus_ind;
#define CLR_NXS_ADAPTER		0
#define CLR_NXS_I_OR_T		1
#define CLR_NXS_IT_OR_TI	2
#define CLR_NXS_I_T_L		3
#define CLR_NXS_I_T_L_Q_TAG	4
#define CLR_NXS_I_T_L_Q_TC	5
#define CLR_NXS_I_T_L_Q_STAG	6
#ifdef SEQUENCER_UPDATE
#define CLR_NXS_T_L		7
#define CLR_NXS_L		8
#endif

	uint8_t			res1[4];
	uint8_t			queue_ind;
#define NOT_IN_Q		(1U << 0)
#define EXEC_Q			(1U << 1)
#define SEND_Q			(1U << 2)
#define RESUME_TX		(1U << 6)
#define SUSPEND_TX		(1U << 7)

	uint8_t			res2[3];
	uint8_t			conn_mask_to_clr;
	uint8_t			res3[3];
	uint8_t			res4[16];
	uint8_t			lun_to_clr[8];
	uint8_t			res5[4];
	uint16_t		tag_to_clr;
	uint8_t			res6[16];
	uint16_t		conn_handle_to_clr;
	uint8_t			res7[8];
	uint16_t		tc_to_clr;
	uint16_t		nexus_ctx;
	uint8_t			res8[44];
} __packed;

/*
 * SAS SCB data strucutes.
 */ 
union hardware_scb {
	struct hscb_header		header;
	struct asd_ssp_task_hscb 	ssp_task;
	struct asd_ssp_long_task_hscb	long_ssp_task;
	struct asd_abort_task_hscb 	abort_task;
	struct asd_ssp_tmf_hscb 	ssp_tmf;
	struct asd_query_ssp_task_hscb	query_ssp_task;
	struct asd_ata_task_hscb	ata_task; 
	struct asd_atapi_task_hscb	atapi_task; 
	struct asd_control_ata_dev_hscb	control_ata_dev; 
	struct asd_smp_task_hscb	smp_task; 
	struct asd_control_phy_hscb	control_phy;
	struct asd_link_admin_hscb 	link_admin;
	struct asd_empty_hscb		empty_scb; 
	struct asd_copy_mem_hscb 	copy_mem;
	struct asd_clear_nexus_hscb	clear_nexus;
	uint8_t				bytes[128];
} __packed;


/* 
 * Done List Opcode definitions. 
 * TASK_COMPLETE (TASK_COMP) :	Indicates status was received and acknowledged.
 * TASK_FAILED (TASK_F)      : 	Indicates an error occured prior to receving an
 *			       	an acknowledgement for the command.
 * TASK_UNACKED (TASK_UA)    : 	Indicates the command was transmitted to the 
 *			       	target, but an ACK (R_OK) or NAK (R_ERR) was not
 *			       	received.
 * TASK_INTERRUPTED (TASK_INT):	Indicates an error occured after the command was
 *				acknowledged.
 * TASK_COMPLETE_WITH_RESPONSE: Indicates status was received and acknowledged 
 * (TASK_COMP_W_RESP) 		with response.
 */
#define TASK_COMP_WO_ERR		0x00
#define TASK_COMP_W_UNDERRUN		0x01
#define TASK_COMP_W_OVERRUN		0x02
#define TASK_F_W_OPEN_REJECT		0x04
#define TASK_INT_W_BRK_RCVD		0x05
#define TASK_INT_W_PROT_ERR		0x06
#define SSP_TASK_COMP_W_RESP		0x07
#define TASK_INT_W_PHY_DOWN		0x08
#define LINK_ADMIN_TASK_COMP_W_RESP	0x0A
#define CSMI_TASK_COMP_WO_ERR		0x0B
#define ATA_TASK_COMP_W_RESP		0x0C
#ifdef SEQUENCER_UPDATE
#define TASK_UNACKED_W_PHY_DOWN		0x0D
#define TASK_UNACKED_W_BREAK_RCVD	0x0E
#define TASK_INT_W_SATA_TO		0x0F
#endif
#define TASK_INT_W_NAK_RCVD		0x10
#define CONTROL_PHY_TASK_COMP		0x11
#define RESUME_COMPLETE			0x13
#define TASK_INT_W_ACKNAK_TO		0x14
#define TASK_F_W_SMPRSP_TO		0x15
#define TASK_F_W_SMP_XMTRCV_ERR		0x16
#ifdef SEQUENCER_UPDATE
#define TASK_COMPLETE_W_PARTIAL_SGLIST	0x17
#define TASK_UNACKED_W_ACKNAK_TIMEOUT	0x18
#define TASK_UNACKED_W_SATA_TO		0x19
#endif
#define TASK_F_W_NAK_RCVD		0x1A
#define TASK_ABORTED_BY_ITNL_EXP	0x1B
#define ATA_TASK_COMP_W_R_ERR_RCVD	0x1C
#define TMF_F_W_TC_NOT_FOUND		0x1D
#define TASK_ABORTED_ON_REQUEST		0x1E
#define TMF_F_W_TAG_NOT_FOUND		0x1F
#define TMF_F_W_TAG_ALREADY_FREE	0x20
#define TMF_F_W_TASK_ALREADY_DONE	0x21
#define TMF_F_W_CONN_HNDL_NOT_FOUND	0x22
#define TASK_CLEARED			0x23
#ifdef SEQUENCER_UPDATE
#define TASK_INT_W_SYNCS_RCVD		0x24
#endif
#define TASK_UA_W_SYNCS_RCVD		0x25
#ifdef SEQUENCER_UPDATE
#define TASK_F_W_IRTT_TIMEOUT		0x26
#define TASK_F_W_NON_EXISTENT_SMP_CONN	0x27
#define TASK_F_W_IU_TOO_SHORT		0x28
#define TASK_F_W_DATA_OFFSET_ERR	0x29
#define TASK_F_W_INVALID_CONN_HANDLE	0x2A
#define TASK_F_W_ALREADY_REQ_N_PENDING	0x2B
#endif
#define EMPTY_BUFFER_RCVD		0xC0	/* or'ed with EDB element. */	
#ifdef SEQUENCER_UPDATE
#define EST_NEXUS_EMPTY_BUFFER_RCVD	0xD0	/* or'ed with EDB element */
#endif


/*
 * Status block format.
 */

/* Generic status block format. */
struct generic_sb {
	uint32_t		res;
} __packed;

/* Task Complete Without Error format. */
struct no_error_sb {
	uint16_t		cons_idx;		/* SCB consumer index.*/
	uint16_t		res;
} __packed;
 
/* Task Complete With Data UNDERRUN format. */
struct data_underrun_sb {
	uint32_t		res_len;		/* Residual length. */
} __packed;

/* Task Complete With Response format. */
struct response_sb {
	uint16_t		empty_scb_tc;
	uint8_t			empty_buf_len;
	uint8_t			empty_buf_elem;		/*
							 * The first 3 bits 
							 * represents msb of 
							 * empty_buf_len.
							 */
#define EDB_ELEM_MASK		0x70
#define EDB_ELEM_SHIFT		4
#define RSP_EDB_ELEM(rsp)	\
	(((rsp)->empty_buf_elem & EDB_ELEM_MASK) >> EDB_ELEM_SHIFT)
#define EDB_BUF_LEN_MSB_MASK	0x7
#define EDB_BUF_LEN_MSB_SHIFT	8
#define RSP_EDB_BUFLEN(rsp)	\
    ((rsp)->empty_buf_len	\
   | (((rsp)->empty_buf_elem & EDB_BUF_LEN_MSB_MASK) << EDB_BUF_LEN_MSB_SHIFT))
} __packed;

/* Task Failed with Open Reject. */
struct open_reject_sb {
	uint8_t			res1;
	uint8_t			abandon_open;
	uint8_t			reason;
	uint8_t			res2;
} __packed;

/* SSP Response Empty Buffer format. */
struct ssp_resp_edb {
	uint32_t		res_len;

	/* 
	 * TAG_TO_CLEAR field shall valid only for an SCB with an
	 * ABORT TASK SCB, which indicates the value of the 
	 * TAG OF TASK TO BE MANAGED field for the ABORT TASK 
	 * information unit.
	 */ 
	uint16_t		tag_to_clear;
	uint16_t		rcvd_frame_len;
	uint8_t			res[8];
	struct ssp_resp_frame	resp_frame;
} __packed;

/* Get Link Error Counters Empty Buffer format. */
struct link_err_cnt_edb {
	uint32_t		inv_dword_cnt;
	uint32_t		disparity_err_cnt;
	uint32_t		loss_of_sync_cnt;
	uint32_t		phy_reset_cnt;
} __packed;

/* ATA Response Empty Buffer format. */
struct ata_resp_edb {
	uint32_t		residual_len;
	uint16_t		res1;
	uint16_t		rcvd_frame_len;
	uint8_t			res2[8];
	uint8_t			ending_fis[24];
	uint32_t		sstatus;
	uint32_t		serror;
	uint32_t		scontrol;
	uint32_t		sactive;
} __packed;

/*
 * We purposely *do not* pack this union so that an array
 * of edbs is aligned on an efficient machine boundary.
 * Our only requirement is that the fields within an edb
 * be left unpadded.
 */
union edb {
	struct ssp_resp_edb	ssp_resp;
	struct ata_resp_edb	ata_resp;
	struct link_err_cnt_edb	link_err_cnt;
	uint8_t			bytes[1068];
};

/* Control PHY Task Complete format. */
struct control_phy_sb {
	uint8_t			sb_opcode;
#define PHY_RESET_COMPLETED	0x00

	/* The fields below are only used for PHY_RESET_COMPLETED. */ 
	uint8_t			oob_status;
	uint8_t			oob_mode;
	uint8_t			oob_signals;
};

/* I_T Nexus Loss Expired format. */
/* Status Sub-Block format. */
struct open_timeout_ssb {
	uint8_t			break_timeout;
	uint16_t		res;
} __packed;

struct open_reject_ssb {
	uint8_t			abandon_retry_open;
	uint8_t			open_reject_reason;	
	uint8_t			res;
} __packed;

struct itnl_exp_sb {
	uint8_t			reason;
#define TASK_F_W_OPEN_TO	0x03
#define TASK_F_W_OPEN_REJECT	0x04
#define TASK_F_W_PHY_DOWN	0x09
#define TASK_F_W_BREAK_RCVD	0x0E

	union {
		struct open_timeout_ssb	open_timeout;
		struct open_reject_ssb	open_reject;
	} stat_subblk; 
};

#define ABANDON_OPEN_MASK	0x02
#define RETRY_OPEN_MASK		0x01

/* Abandon Open Reject Reason field. */
#define BAD_DESTINATION		0x00
#define CONN_RATE_NOT_SUPPORTED	0x01
#define PROTOCOL_NOT_SUPPORTED	0x02
#define RSVD_ABANDON_0		0x03
#define RSVD_ABANDON_1		0x04
#define RSVD_ABANDON_2		0x05
#define RSVD_ABANDON_3		0x06
#define WRONG_DESTINATION	0x07
#define STP_RESOURCE_BUSY	0x08

/* Retry Open Reject Reason field. */
#define NO_DESTINATION		0x00
#define PATHWAY_BLOCKED		0x10
#define RSVD_CONTINUE_0		0x20
#define RSVD_CONTINUE_1		0x30
#define RSVD_INITIALIZE_0	0x40
#define RSVD_INITIALIZE_1	0x50
#define RSVD_STOP_0		0x60
#define RSVD_STOP_1		0x70
#define RETRY			0x80

/* Task Cleared format. */
struct task_cleared_sb {
	uint16_t		tag_of_cleared_task;
	uint16_t		clr_nxs_ctx;
} __packed;


/* Empty Data Buffer Received format. */
struct bytes_dmaed_subblk {
	uint8_t			protocol;	/* 
						 * MSB represent Initiator or 
						 * Target.
						 */
#define INIT_TGT_MASK		0x80
#define PROTOCOL_MASK		0x70
	
	uint16_t		edb_len;	/*
						 * The msb 5 bits are ignored.
						 */
#define BYTES_DMAED_LEN_MASK	0x07FF	
} __packed;

struct primitive_rcvd_subblk {
	uint8_t			reg_addr;
	uint8_t			reg_content;
	uint8_t			res;
} __packed;

struct phy_event_subblk {
	uint8_t			oob_status;
	uint8_t			oob_mode;
	uint8_t			oob_signals;
} __packed;

struct link_reset_err_subblk {
	uint8_t			error;
#define RCVD_ID_TIMER_EXP	0x00		/* 
						 * Timed out waiting for 
						 * IDENTIFY Address frame
						 * from attached device.
						 */
#ifdef SEQUENCER_UPDATE
#define LOSS_OF_SIGNAL_ERR	0x01		/*
						 * Loss of signal detected
						 * following a completed
						 * PHY reset
						 */
#define LOSS_OF_DWS_ERR		0x02		/*
						 * Loss of dword sync detected
						 * following a completed
						 * PHY reset
						 */
#endif
#define RCV_FIS_TIMER_EXP	0x03		/*
						 * Timed out waiting for initial
						 * Device-to-Host Register FIS
						 * from attached device.
						 */
	uint16_t		res;
} __packed;

struct timer_event_subblk {
	uint8_t			error;
#define DWS_RESET_TO_EXP	0x00		/* 
						 * DWS reset timer timeout
						 * expired.
						 */ 
	uint16_t		res;
} __packed;

/*
 * Common reasons shared by REQ TASK ABORT and REQ DEVICE RESET.
 */ 
#define TASK_UA_W_PHY_DOWN	0x0D

struct req_task_abort_subblk {
	uint16_t		task_tc_to_abort;
	uint8_t			reason;
#define TASK_UA_W_BRK_RCVD	0x0E
#define TASK_UA_W_ACKNAK_TO	0x18
} __packed;

struct req_dev_reset_subblk {
	uint16_t		task_tc_to_abort;
	uint8_t			reason;
#define	TASK_INT_W_SATA_TO	0x0F
#define TASK_UA_W_SATA_TO	0x19
#define TASK_INT_W_SYNCS_RCVD	0x24

} __packed;

struct edb_rcvd_sb {
	uint8_t			sb_opcode;
	/* The opcode defintions below can be or'ed with Receiving Phy ID. */
#define BYTES_DMAED		0x00		/* 00h - 07h */	
#define PRIMITIVE_RCVD		0x08		/* 08h - 0Fh */
#define PHY_EVENT		0x10		/* 10h - 17h */
#define LINK_RESET_ERR		0x18		/* 18h - 1Fh */
#define TIMER_EVENT		0x20		/* 20h - 27h */
#ifdef SEQUENCER_UPDATE
/* Removed .... */
#else
#define ESTABLISH_CONN		0x28		/* 28h - 2Fh */
#endif
#define REQ_TASK_ABORT		0xF0	
#define REQ_DEVICE_RESET	0xF1
#ifdef SEQUENCER_UPDATE
#define SIGNAL_NCQ_ERROR	0xF2
#define CLEAR_NCQ_ERROR		0xF3
#endif
#define EDB_OPCODE_MASK		0x07

	union {
		struct bytes_dmaed_subblk 	bytes_dmaed;
		struct primitive_rcvd_subblk 	prim_rcvd;
		struct phy_event_subblk 	phy_event;
		struct link_reset_err_subblk 	link_reset_err;
		struct timer_event_subblk 	timer_event;
		struct req_task_abort_subblk 	req_task_abort;	
		struct req_dev_reset_subblk	req_dev_reset;
	} edb_subblk;	
} __packed;

/* Done List format. */
struct asd_done_list {
	uint16_t		index;
	uint8_t			opcode;
	union {
		struct generic_sb	generic;
		struct no_error_sb	no_error;
		struct data_underrun_sb data;
		struct open_reject_sb	open_reject;
		struct response_sb	response;
		struct control_phy_sb	control_phy;
		struct itnl_exp_sb	itnl_exp;
		struct task_cleared_sb	task_cleared;
		struct task_cleared_sb	resume_complete;
		struct edb_rcvd_sb 	edb_rcvd;
	} stat_blk;
	uint8_t			toggle;	
#define	ASD_DL_TOGGLE_MASK 0x1
} __packed;

struct asd_int_ddb {
#ifdef SEQUENCER_UPDATE
	uint16_t	q_free_ddb_head;
	uint16_t	q_free_ddb_tail;
	uint16_t	q_free_ddb_cnt;
	uint16_t	q_used_ddb_head;
	uint16_t	q_used_ddb_tail;
#else
	uint8_t		res1[10];
#endif
	uint16_t	shared_mem_lock;
#ifdef SEQUENCER_UPDATE
	uint16_t	smp_conn_tag;
	uint16_t	est_nexus_buf_cnt;
	uint16_t	est_nexus_buf_thresh;
	uint8_t		res2[4];
	uint8_t		settable_max_contents;
	uint8_t		res3[23];
#else
	uint8_t		res2[34];
#endif
	uint8_t		conn_not_active;
	uint8_t		phy_is_up;
	uint8_t		port_map_by_ports[8];
	uint8_t		port_map_by_links[8];
} __packed;

/*
 * Used in the ddb_type field.
 */
#define UNUSED_DDB		0xFF
#define TARGET_PORT_DDB		0xFE
#define INITIATOR_PORT_DDB	0xFD
#define PM_PORT_DDB		0xFC

/*
 * Used in the addr_fr_port field
 */
#define INITIATOR_PORT_MODE	0x80
#define OPEN_ADDR_FRAME		0x01

#ifdef SEQUENCER_UPDATE
struct asd_ssp_smp_ddb {
#else
struct asd_ddb {
#endif
	uint8_t		addr_fr_port;
	
	uint8_t		conn_rate;
	uint16_t	init_conn_tag;
	uint8_t		dest_sas_addr[8];
	uint16_t	send_q_head;
	uint8_t		sqsuspended;
	uint8_t		ddb_type;

	uint16_t	res1;
	uint16_t	awt_default;
	uint8_t		comp_features;
	uint8_t		pathway_blk_cnt;
	uint16_t	arb_wait_time;
	uint32_t	more_comp_features;
	uint8_t		conn_mask;
	uint8_t		open_affl;

#define OPEN_AFFILIATION	0x01
#ifdef SEQUENCER_UPDATE
#define CONCURRENT_CONNECTION_SUPPORT	0x04
#else
#define STP_AFFILIATION		0x20
#define SUPPORTS_AFFILIATION	0x40
#define SATA_MULTIPORT		0x80
#endif

#ifdef SEQUENCER_UPDATE
	uint16_t	res2;
#else
	uint8_t		res2;
	uint8_t		stp_close;
#endif
#define CLOSE_STP_NO_TX		0x00
#define CLOSE_STP_BTW_CMDS	0x01
#define CLOSE_STP_NEVER		0x10

	uint16_t	exec_q_tail;
	uint16_t	send_q_tail;
	uint16_t	sister_ddb;
#ifdef SEQUENCER_UPDATE
	uint8_t		res3[2];
	uint8_t		max_concurrent_connections;
	uint8_t		concurrent_connections;
	uint8_t		tunable_number_contexts;
	uint8_t		res4;
#else
	uint16_t	ata_cmd_scb_ptr;
	uint32_t	sata_tag_mask;
#endif
	uint16_t	active_task_cnt;
#ifdef SEQUENCER_UPDATE
	uint8_t		res5[9];
#else
	uint16_t	res3;
	uint32_t	sata_sactive;
	uint8_t		no_of_sata_tags;
	uint8_t		sata_stat;
	uint8_t		sata_ending_stat;
#endif
	uint8_t		itnl_reason;
#ifdef SEQUENCER_UPDATE
	uint8_t		res6[2];
#else
	uint16_t	ncq_data_scb_ptr;
#endif
	uint16_t	itnl_const;
#define ITNL_TIMEOUT_CONST	0x7D0		/* 2 seconds */	

	uint32_t	itnl_timestamp;
} __packed;

#ifdef SEQUENCER_UPDATE
struct asd_sata_stp_ddb {
	uint8_t		addr_fr_port;
	
	uint8_t		conn_rate;
	uint16_t	init_conn_tag;
	uint8_t		dest_sas_addr[8];
	uint16_t	send_q_head;
	uint8_t		sqsuspended;
	uint8_t		ddb_type;

	uint16_t	res1;
	uint16_t	awt_default;
	uint8_t		comp_features;
	uint8_t		pathway_blk_cnt;
	uint16_t	arb_wait_time;
	uint32_t	more_comp_features;
	uint8_t		conn_mask;
	uint8_t		open_affl;

#define STP_AFFILIATION		0x20
#define SUPPORTS_AFFILIATION	0x40
#define SATA_MULTIPORT		0x80

	uint8_t		res2;
	uint8_t		stp_close;
#define CLOSE_STP_NO_TX		0x00
#define CLOSE_STP_BTW_CMDS	0x01
#ifndef SEQUENCER_UPDATE
#define CLOSE_STP_NEVER		0x10
#endif

	uint16_t	exec_q_tail;
	uint16_t	send_q_tail;
	uint16_t	sister_ddb;
	uint16_t	ata_cmd_scb_ptr;
	uint32_t	sata_tag_mask;
	uint16_t	active_task_cnt;
	uint8_t		res3[2];
	uint32_t	sata_sactive;
	uint8_t		no_of_sata_tags;
	uint8_t		sata_stat;
	uint8_t		sata_ending_stat;
	uint8_t		itnl_reason;
	uint16_t	ncq_data_scb_ptr;
	uint16_t	itnl_const;
	uint32_t	itnl_timestamp;
} __packed;
#endif

struct asd_port_multi_ddb {
	uint8_t		res1[29];
	uint8_t		multi_pm_port;
#define SATA_PORT_MULTI_DDB	(1U << 1)
#define PM_PORT_SHIFT		4
#define PM_PORT_MASK		0xF0

	uint8_t		res2;
	uint8_t		res3[5];
	uint16_t	sister_ddb;
	uint16_t	ata_cmd_scb_ptr;
	uint32_t	sata_tag_alloc_mask;
	uint16_t	active_task_cnt;
	uint16_t	parent_ddb;
	uint32_t	sata_sactive_reg;
	uint8_t		sata_tags;
	uint8_t		sata_stat_reg;
	uint8_t		sata_end_stat_reg;
	uint8_t		res4[9];
} __packed;

#ifndef SEQUENCER_UPDATE
struct asd_port_multi_table_ddb {
	uint16_t	ddb_ptr0;
	uint16_t	ddb_ptr1;
	uint16_t	ddb_ptr2;
	uint16_t	ddb_ptr3;
	uint16_t	ddb_ptr4;
	uint16_t	ddb_ptr5;
	uint16_t	ddb_ptr6;
	uint16_t	ddb_ptr7;
	uint16_t	ddb_ptr8;
	uint16_t	ddb_ptr9;
	uint16_t	ddb_ptr10;
	uint16_t	ddb_ptr11;
	uint16_t	ddb_ptr12;
	uint16_t	ddb_ptr13;
	uint16_t	ddb_ptr14;
	uint16_t	ddb_ptr15;
	uint8_t		res[32];
} __packed;
#endif

/*
 * Transport specific (SAS and SATA) data structures.
 */
struct sas_id_addr {
	uint8_t		addr_frame_type;	/*
						 * Bits 0-3: Address Frame Type
						 * 	4-6: Device Type
						 * 	  7: Restricted  
						 */
#define SAS_END_DEVICE		(1U << 4)
#define SAS_EDGE_EXP_DEVICE	(1U << 5)	
#define SAS_FANOUT_EXP_DEVICE	(1U << 6) 
#define SAS_DEVICE_MASK		0x70

	uint8_t		restricted1;
	uint8_t		init_port_type;		
#define SMP_INIT_PORT		(1U << 1)
#define STP_INIT_PORT		(1U << 2)
#define SSP_INIT_PORT		(1U << 3)
#define INIT_PORT_MASK		0x0E
	
	uint8_t		tgt_port_type;
#define SATA_TGT_PORT		(1U << 0)
#define SMP_TGT_PORT		(1U << 1)
#define STP_TGT_PORT		(1U << 2)
#define SSP_TGT_PORT		(1U << 3)
#define TGT_PORT_MASK		0x0E
	
	uint8_t		restricted2[8];
	uint8_t		sas_addr[8];
	uint8_t		phy_id;
	uint8_t		res[7];
	uint8_t		crc[4];
} __packed; 

struct sas_initial_fis {
	uint8_t		fis[20];
};

union sas_bytes_dmaed {
	struct sas_id_addr	id_addr_rcvd;
	struct sas_initial_fis	initial_fis_rcvd;
};

void	asd_hwi_hash(uint8_t *sas_addr, uint8_t *hashed_addr);

#define ASD_MAX_DATA_PAT_GEN_SIZE_TX 	1024
#define ASD_MAX_DATA_PAT_GEN_SIZE_RX 	2176

#endif /* ADP94XX_SAS_H */
