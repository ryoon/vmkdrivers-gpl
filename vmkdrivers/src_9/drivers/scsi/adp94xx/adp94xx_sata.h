/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Written by : Robert Tarte  <robt@PacificCodeWorks.com>
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
 * $Id: //depot/razor/linux/src/adp94xx_sata.h#14 $
 * 
 */	

#ifdef ASD_DEBUG
#define INLINE
#else
#define INLINE	inline
#endif

#ifndef ADP94XX_SATA_H
#define ADP94XX_SATA_H
#define	FIS_HOST_TO_DEVICE	0x27
#define	FIS_DEVICE_TO_HOST	0x34
#define FIS_COMMAND		0x80

#define ASD_H2D_FIS(ata_hscb)	\
	((struct adp_host_to_dev_fis *)&ata_hscb->host_to_dev_reg_fis[0])

#define ASD_D2H_FIS(ata_resp_edb)	\
	((struct adp_dev_to_host_fis *)&ata_resp_edb->ending_fis[0])

#define FIS_OFFSET(x)	((unsigned long)(&((struct adp_host_to_dev_fis *)0)->x))

#define ATA2SCSI_2(x)	((((x) & 0x00ff) << 8) | (((x) & 0xff00) >> 8))
#define ATA2SCSI_4(x)	((((x) & 0x000000ff) << 24) | \
			 (((x) & 0x0000ff00) << 8) | \
			 (((x) & 0x00ff0000) >> 8) | \
			 (((x) & 0xff000000) >> 24))
#define ATA2SCSI_8(x)	((((x) & 0x00000000000000ffLL) << 56) | \
			 (((x) & 0x000000000000ff00LL) << 40) | \
			 (((x) & 0x0000000000ff0000LL) << 24) | \
			 (((x) & 0x00000000ff000000LL) << 8) | \
			 (((x) & 0x000000ff00000000LL) >> 8) | \
			 (((x) & 0x0000ff0000000000LL) >> 24) | \
			 (((x) & 0x00ff000000000000LL) >> 40) | \
			 (((x) & 0xff00000000000000LL) >> 56))

// for the SStatus register
#define SSTATUS_IPM_DET_MASK		0x00000f0f
#define SSTATUS_SPD_MASK		0x000000f0

#define SSTATUS_DET_COM_ESTABLISHED	0x00000003
#define SSTATUS_SPD_NOT_ESTABLISHED	0x00000000
#define SSTATUS_IPM_ACTIVE		0x00000100

// for the ATA Identify command
#define IDENT_48BIT_SUPPORT		0x0400
#define IDENT_QUEUED_SUPPORT		0x0002
#define IDENT_DMA_SUPPORT		0x01
#define POWER_MANAGEMENT_SUPPORTED	0x0008
#define IDENT_WRITE_FUA_SUPPORT		0x0080

#define ATA_READ_BUFFER_CAPABLE		0x2000
#define ATA_WRITE_BUFFER_CAPABLE	0x1000
#define ATA_LOOK_AHEAD_CAPABLE		0x0040
#define ATA_WRITE_CACHE_CAPABLE		0x0020
#define ATA_SMART_CAPABLE		0x0001

#define ATA_READ_BUFFER_ENABLED		0x2000
#define ATA_WRITE_BUFFER_ENABLED	0x1000
#define ATA_LOOK_AHEAD_ENABLED		0x0040
#define ATA_WRITE_CACHE_ENABLED		0x0020
#define ATA_SMART_ENABLED		0x0001


#define ATA_SMART_ENABLED		0x0001

#define RW_DMA_LBA_SIZE			(1 << 28)
#define RW_DMA_MAX_SECTORS		(1 << 8)

#define ATA_DMA_ULTRA_VALID		0x0004
#define ATA_PIO_MODES_VALID		0x0002
#define DMA_ULTRA_MODE_MASK		0x007f

#define UDMA_XFER_MODE		0x40


// for the ATA Check Power Mode command
#define ATA_STANDBY_MODE		0x00
#define ATA_IDLE_MODE			0x80
#define ATA_ACTIVE			0xff

// for the INQUIRY command
#define ATA_REMOVABLE			0x0080
#define SCSI_REMOVABLE			0x0008
#define SCSI_3_RESPONSE_DATA_FORMAT	0x02
#define CMD_QUEUE_BIT			0x02
#define	CMD_DT				0x02
#define LUN_FIELD			0xE0
#define EVPD				0x01
#define SUPPORTED_VPD			0x00
#define UNIT_SERIAL_VPD			0x80
#define ATA_PRODUCT_SERIAL_LENGTH	20
#define INQUIRY_RESPONSE_SIZE		58

// for the READ_CAPACITY command
#define READ_CAPACITY_DATA_LEN		8
#define ATA_BLOCK_SIZE			512

// for the REQUEST_SENSE command
#define SENSE_DATA_SIZE				18
#define INVALID_FIELD_IN_CDB			0x24
#define INVALID_COMMAND_OPERATION		0x20
#define LOGICAL_UNIT_NOT_READY			0x04
#define INITIALIZING_COMMAND_REQUIRED		0x02
#define NOTIFY_REQUIRED				0x11
#define MEDIUM_NOT_PRESENT			0x3a
#define IO_PROCESS_TERMINATED_ASC		0x00
#define IO_PROCESS_TERMINATED_ASCQ		0x06
#define LOGICAL_UNIT_ASC			0x08
#define COMMUNICATION_CRC_ERROR			0x03
#define OPERATOR_ASC				0x5a
#define MEDIUM_REMOVAL_REQUEST			0x01
#define MEDIUM_CHANGED				0x28
#define LOGICAL_UNIT_FAILURE			0x3e
#define FAILED_SELF_TEST			0x03
#define FORMAT_FAILED				0x31
#define FORMAT_COMMAND_FAILED			0x01

#define RESOURCE_FAILURE		0x55
#define MEMORY_OUT_OF_SPACE		0x06

#define NM_ERR		TRK0_ERR
#define UNC_ERR		ECC_ERR

// for the START_STOP command byte 4
#define START_STOP_LOEJ			0x02
#define START_STOP_START		0x01

// for LOG_SENSE command byte 2
#define SMART_DATA			0x31
#define PAGE_CODE_MASK			0x3f
#define PAGE_CONTROL_CURRENT		0x00
#define PAGE_CONTROL_CUMULATIVE		0x40
#define SMART_READ_DATA			0xd0

#define SET_NO_IO_MODE(ata_hscb)
#define SET_PIO_MODE(ata_hscb)
#define SET_DMA_MODE(ata_hscb)	{ (ata_hscb)->ata_flags |= DMA_XFER_MODE; }

// for PACKET Command
#define DMADIR_BIT_NEEDED		0x8000
#define DMADIR_BIT_DEV_TO_HOST		0x4
#define PACKET_DMA_BIT			0x1

// for WRITE_BUFFER command
#define DATA_ONLY_MODE			0x02
#define DESCRIPTOR_MODE			0x03
#define BUFFER_MODE_MASK		0x1f
#define READ_BUFFER_DESCRIPTOR_LENGTH	0x04
#define ATA_BUFFER_SIZE			512

// no Linux ATA command
#define WIN_WRITE_DMA_FUA_EXT		0x3d
#define WIN_WRITE_DMA_QUEUED_FUA_EXT	0x3e
#define SCSI_WRITE_FUA_BIT		0x08

// for MODE_SELECT command
#define SP_BIT				0x01
#define PF_BIT				0x10

// for MODE_SENSE command
#define DBD_BIT				0x08
#define PAGE_CODE_MASK			0x3f
#define PAGE_CONTROL_MASK		0xc0
#define LLBA_MASK			0x10
#define PAGE_CONTROL_CURRENT		0x00
#define PAGE_CONTROL_CHANGEABLE		0x40
#define PAGE_CONTROL_DEFAULT		0x80
#define PAGE_CONTROL_SAVED			0xC0


// pages for MODE_SENSE & MODE_SELECT
#define MODE_PARAMETER_HEADER_LENGTH_10		8
#define MODE_PARAMETER_HEADER_LENGTH_6		4
#define BLOCK_DESCRIPTOR_LENGTH_8		8
#define BLOCK_DESCRIPTOR_LENGTH_16		16

#define READ_WRITE_ERROR_RECOVERY_MODE_PAGE	0x01
#define DISCONNECT_RECONNECT_PAGE		0x02
#define FORMAT_DEVICE_PAGE			0x03
#define RIGID_DISK_GEOMETRY_PAGE		0x04
#define CACHING_MODE_PAGE			0x08
#define CONTROL_MODE_PAGE			0x0a
#define INFORMATIONAL_EXCEPTION_CONTROL_PAGE	0x1c
#define RETURN_ALL_PAGES			0x3f

#define READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN		0x0c
#define CACHING_MODE_PAGE_LEN				0x14
#define CONTROL_MODE_PAGE_LEN				0x0c
#define INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN	0x0c

#define SCSI_DRA	0x20
#define SCSI_WCE	0x04
#define SCSI_DEXCPT	0x08

// for FORMAT_UNIT command
#define FORMAT_WRITE_BUFFER_LEN		4096
#define FORMAT_BLOCK_SIZE		512
#define FORMAT_SECTORS \
	(FORMAT_WRITE_BUFFER_LEN / FORMAT_BLOCK_SIZE)



// for SEND_DIAGNOSTIC
#define SELFTEST			0x04
#define SELFTEST_CODE_MASK		0xe0

#ifndef REPORT_LUNS
#define REPORT_LUNS			0xa0
#endif
#define REPORT_LUNS_SIZE		16

#ifndef ASSERT
#ifdef ASD_DEBUG
#define ASSERT(x) \
	if (!(x)) \
	{ \
		printk("Assertion failed: %s:%d\n", __FUNCTION__, __LINE__); \
	}
#else
#define ASSERT(x)
#endif
#endif

#ifndef WIN_READ_EXT
#define WIN_READ_EXT			0x24 /* 48-Bit */
#define WIN_READDMA_EXT			0x25 /* 48-Bit */
#define WIN_READDMA_QUEUED_EXT		0x26 /* 48-Bit */
#define WIN_FLUSH_CACHE			0xE7
#define WIN_VERIFY_EXT			0x42 /* 48-Bit */
#define WIN_WRITE_EXT			0x34 /* 48-Bit */
#define WIN_WRITEDMA_EXT		0x35 /* 48-Bit */
#define WIN_WRITEDMA_QUEUED_EXT		0x36 /* 48-Bit */
#define lba_capacity_2			words94_125[6]
#endif


#define LBA_MODE	0x40

struct adp_host_to_dev_fis {
	uint8_t		fis_type;
	uint8_t		cmd_devcontrol;
	uint8_t		command;
	uint8_t		features;

	uint8_t		sector_number;
	uint8_t		cyl_lo;
	uint8_t		cyl_hi;
	uint8_t		dev_head;

	uint8_t		sector_number_exp;
	uint8_t		cyl_lo_exp;
	uint8_t		cyl_hi_exp;
	uint8_t		features_exp;

	uint8_t		sector_count;
	uint8_t		sector_count_exp;
	uint8_t		res1;
	uint8_t		control;

	uint8_t		res2;
	uint8_t		res3;
	uint8_t		res4;
	uint8_t		res5;
} __packed;


#define	lba0		sector_number
#define lba1		cyl_lo
#define lba2		cyl_hi
#define lba3		sector_number_exp
#define byte_count_lo	cyl_lo
#define byte_count_hi	cyl_hi

struct adp_dev_to_host_fis {
	uint8_t		fis_type;
	uint8_t		interrupt;
	uint8_t		status;
	uint8_t		error;

	uint8_t		sector_number;
	uint8_t		cyl_lo;
	uint8_t		cyl_hi;
	uint8_t		dev_head;

	uint8_t		sector_number_exp;
	uint8_t		cyl_lo_exp;
	uint8_t		cyl_hi_exp;
	uint8_t		res1;

	uint8_t		sector_count;
	uint8_t		sector_count_exp;
	uint8_t		res2;
	uint8_t		res3;

	uint8_t		res4;
	uint8_t		res5;
	uint8_t		res6;
	uint8_t		res7;
} __packed;

/* -------------------------------------------------------------------------- */

ASD_COMMAND_BUILD_STATUS
asd_build_ata_scb(
struct asd_softc	*asd,
struct scb		*scb,
union asd_cmd		*acmd
);

ASD_COMMAND_BUILD_STATUS
asd_build_atapi_scb(
struct asd_softc	*asd,
struct scb		*scb,
union asd_cmd		*acmd
);

INLINE
void
asd_sata_setup_fis(
struct asd_ata_task_hscb	*ata_hscb,
uint8_t				command
);

void
asd_sata_compute_support(
struct asd_softc	*asd,
struct asd_target	*target
);

INLINE
void
asd_sata_setup_lba_ext(
struct asd_ata_task_hscb	*ata_hscb,
unsigned			lba,
unsigned			sectors
);

INLINE
void
asd_sata_setup_lba(
struct asd_ata_task_hscb	*ata_hscb,
unsigned			lba,
unsigned			sectors
);

void
asd_sata_set_check_condition(
struct scsi_cmnd	*cmd,
unsigned		sense_key,
unsigned		additional_sense,
unsigned		additional_sense_qualifier
);

void
asd_sata_check_registers(
struct ata_resp_edb	*ata_resp_edbp,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_completion_status(
struct scsi_cmnd	*cmd,
struct ata_resp_edb	*ata_resp_edbp
);
INLINE
struct ata_resp_edb *
asd_sata_get_edb(
struct asd_softc	*asd,
struct asd_done_list	*done_listp,
struct scb **pescb, 
u_int *pedb_index
);

ASD_COMMAND_BUILD_STATUS
asd_sata_format_unit_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_format_unit_free_memory(
struct asd_softc	*asd,
struct scsi_cmnd	*write_cmd
);

void
asd_sata_format_unit_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_inquiry_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_inquiry_evd_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd,
u_char			*inquiry_data
);

ASD_COMMAND_BUILD_STATUS
asd_sata_log_sense_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_log_sense_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_mode_select_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_mode_sense_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_read_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_read_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_write_buffer_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_write_buffer_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_read_buffer_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_read_buffer_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_read_capacity_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_report_luns_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_request_sense_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_rezero_unit_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_seek_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

ASD_COMMAND_BUILD_STATUS
asd_sata_send_diagnostic_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_send_diagnostic_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_start_stop_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_start_stop_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_synchronize_cache_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_synchronize_cache_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_test_unit_ready_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_test_unit_ready_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_verify_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_verify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_write_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_write_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_write_verify_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
);

void
asd_sata_write_verify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_sata_identify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_sata_configure_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_sata_atapi_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

ASD_COMMAND_BUILD_STATUS
asd_sata_read_write_error_recovery_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
);

ASD_COMMAND_BUILD_STATUS
asd_sata_caching_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
);

ASD_COMMAND_BUILD_STATUS
asd_sata_control_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
);

ASD_COMMAND_BUILD_STATUS
asd_sata_informational_exception_control_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
);

int
asd_sata_set_features_build(
struct asd_softc	*asd,
struct asd_target	*target,
struct scb		*scb,
uint8_t			feature,
uint8_t			sector_count
);

uint8_t *
asd_sata_informational_exception_control_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr,
unsigned			page_control
);

uint8_t *
asd_sata_control_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr
);

uint8_t *
asd_sata_caching_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr,
unsigned			page_control
);

uint8_t *
asd_sata_read_write_error_recovery_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr
);

COMMAND_SET_TYPE asd_sata_get_type(struct adp_dev_to_host_fis *fis);

#endif /* ADP94XX_SATA_H */ 
