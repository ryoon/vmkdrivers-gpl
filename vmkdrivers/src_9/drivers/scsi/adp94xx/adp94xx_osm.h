/*
 * Adaptec ADP94xx SAS HBA device driver for Linux. 
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
 * $Id: //depot/razor/linux/src/adp94xx_osm.h#159 $
 * 
 */	

#ifndef ADP94XX_OSM_H
#define ADP94XX_OSM_H

#include <asm/byteorder.h>
#if !defined(__VMKLNX__)
#include <asm/io.h>
#endif /* !defined(__VMKLNX__) */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/smp_lock.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#if !defined(__VMKLNX__)
#include <linux/config.h>
#endif /* !defined(__VMKLNX__) */
#include <linux/slab.h>
#if !defined(__VMKLNX__)
#include <linux/nmi.h>
#endif /* !defined(__VMKLNX__) */
#include <linux/hdreg.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/init.h>

#if defined(__VMKLNX__)
#include <asm/io.h>
#endif /* defined(__VMKLNX__) */

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z)	(((x)<<16) + ((y)<<8) + (z))
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <linux/blk.h>
#include "sd.h"
#include "hosts.h"
#include "scsi.h"
#else
#include <linux/moduleparam.h>
#include <scsi/scsi_host.h>
#if !defined(__VMKLNX__)
#include <scsi/scsi_driver.h>
#endif /* !defined(__VMKLNX__) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <scsi/scsi_request.h>
#endif
#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsicam.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#if !defined(__VMKLNX__)
#include "scsi_priv.h"
#endif /* !defined(__VMKLNX__) */
#endif
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
typedef struct scsi_cmnd Scsi_Cmnd;
typedef struct scsi_device Scsi_Device;
typedef struct scsi_request Scsi_Request;
typedef struct scsi_host_template Scsi_Host_Template;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef SCSI_DATA_READ
#define SCSI_DATA_READ  DMA_FROM_DEVICE
#define SCSI_DATA_WRITE DMA_TO_DEVICE
#define SCSI_DATA_UNKNOWN DMA_BIDIRECTIONAL
#define SCSI_DATA_NONE  DMA_NONE
#define scsi_to_pci_dma_dir(_a) (_a)
#endif
#include <linux/blkdev.h>

#define __packed __attribute__ ((packed))

/* Driver name */
#define ASD_DRIVER_NAME		"adp94xx"
#define ASD_DRIVER_DESCRIPTION	"Adaptec Linux SAS/SATA Family Driver"
#define ASD_MAJOR_VERSION 	1
#define ASD_MINOR_VERSION 	0
#ifdef SEQUENCER_UPDATE
#define ASD_BUILD_VERSION 	8
#define ASD_RELEASE_VERSION 	12
#define ASD_DRIVER_VERSION	"1.0.8.12-6vmw"
#else
#define ASD_BUILD_VERSION 	7
#define ASD_RELEASE_VERSION 	5
#define ASD_DRIVER_VERSION	"1.0.7"
#endif

/* For now, let's limit ourselves on kernel 2.4, 2.6 and greater */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#error "This driver only support kernel 2.4 or greater"
#endif

/************************* Forward Declarations *******************************/
struct asd_softc;
struct asd_port;
struct scb;
struct asd_scb_platform_data; 
struct asd_domain;
struct asd_done_list;

/********************* Definitions Required by the Core ***********************/
/*
 * Number of SG segments we require.  So long as the S/G segments for
 * a particular transaction are allocated in a physically contiguous
 * manner, the number of S/G segments is unrestricted.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/*
 * We dynamically adjust the number of segments in pre-2.5 kernels to
 * avoid fragmentation issues in the SCSI mid-layer's private memory
 * allocator.  See adp94xx_osm.c asd_size_nseg() for details.
 */
extern u_int asd_nseg;
#define	ASD_NSEG 		asd_nseg
#define	ASD_LINUX_MIN_NSEG 	64
#else
#define	ASD_NSEG 		128
#endif

/* Constants definition */
#define ASD_MAX_QUEUE		ASD_MAX_ALLOCATED_SCBS

/*
 * DC: Does MAX_SECTORS 255 works for some SATA drives ?
 *     Preferable set it to 255 for better performance in 2.6 kernel.
 *     In 2.4 kernel, it seems that block layer always limited to 64 sectors.
 */ 
//#define ASD_MAX_SECTORS	8192
#define ASD_MAX_SECTORS		128
#define ASD_MAX_IO_HANDLES	6

/* Device mapping definition */
/*
 * XXX This should really be limited by SAS/SAM.  With
 *     the use of hash tables in the domain and target
 *     structures as well as DDB site recycling, we
 *     should have no limit.
 */
#define ASD_MAX_LUNS		128
#define SAS_LUN_LEN		8

/********************************** Misc Macros *******************************/
#ifndef roundup
#define	roundup(x, y)   	((((x)+((y)-1))/(y))*(y))
#endif

static inline uint32_t
roundup_pow2(uint32_t val)
{
	val--;
	val |= val >> 1;
	val |= val >> 2;
	val |= val >> 4;
	val |= val >> 8;
	val |= val >> 16;
	return (val + 1);
}

#ifndef powerof2
#define	powerof2(x)	((((x)-1)&(x))==0)
#endif

#ifndef MAX
#define MAX(a,b) 	(((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) 	(((a) < (b)) ? (a) : (b))
#endif

#define NUM_ELEMENTS(array) (sizeof(array) / sizeof(*array))

/******************************* Byte Order ***********************************/
#define asd_htobe16(x)		cpu_to_be16(x)
#define asd_htobe32(x)		cpu_to_be32(x)
#define asd_htobe64(x)		cpu_to_be64(x)
#define asd_htole16(x)		cpu_to_le16(x)
#define asd_htole32(x)		cpu_to_le32(x)
#define asd_htole64(x)		cpu_to_le64(x)

#define asd_be16toh(x)		be16_to_cpu(x)
#define asd_be32toh(x)		be32_to_cpu(x)
#define asd_be64toh(x)		be64_to_cpu(x)
#define asd_le16toh(x)		le16_to_cpu(x)
#define asd_le32toh(x)		le32_to_cpu(x)
#define asd_le64toh(x)		le64_to_cpu(x)

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifndef BYTE_ORDER
#if defined(__BIG_ENDIAN)
#define BYTE_ORDER BIG_ENDIAN
#endif
#if defined(__LITTLE_ENDIAN)
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#endif /* BYTE_ORDER */

/***************************** Bus Space/DMA **********************************/

struct asd_dma_tag
{
	uint32_t	alignment;
	uint32_t	maxsize;
};
typedef struct asd_dma_tag *bus_dma_tag_t;

struct asd_dmamap
{
	dma_addr_t	bus_addr;
};
typedef struct asd_dmamap * bus_dmamap_t;

/*
 * Data structure used to track a slab of DMA
 * safe memory.
 */
struct map_node {
	bus_dmamap_t	 dmamap;
	dma_addr_t	 busaddr;
	uint8_t		*vaddr;
	struct list_head links;
};

int	asd_dma_tag_create(struct asd_softc *, uint32_t /*alignment*/,
			   uint32_t /*maxsize*/, int /*flags*/,
			   bus_dma_tag_t * /*dma_tagp*/);
void	asd_dma_tag_destroy(struct asd_softc *, bus_dma_tag_t /*tag*/);
int	asd_dmamem_alloc(struct asd_softc *, bus_dma_tag_t /*dmat*/,
			 void** /*vaddr*/, int /*flags*/,
			 bus_dmamap_t * /*mapp*/, dma_addr_t * /*baddr*/);
void	asd_dmamem_free(struct asd_softc *, bus_dma_tag_t /*dmat*/,
			void* /*vaddr*/, bus_dmamap_t /*map*/);
void	asd_dmamap_destroy(struct asd_softc *, bus_dma_tag_t /*tag*/,
			   bus_dmamap_t /*map*/);
int	asd_alloc_dma_mem(struct asd_softc *, unsigned, void **,
			  dma_addr_t *, bus_dma_tag_t *, struct map_node *);
void	asd_free_dma_mem(struct asd_softc *, bus_dma_tag_t, struct map_node *);

/* IOCTL registration wrappers */
int 	asd_register_ioctl_dev(void);
int 	asd_unregister_ioctl_dev(void);
int 	asd_ctl_init_internal_data(struct asd_softc *asd);
struct asd_softc * asd_get_softc_by_hba_index(uint32_t hba_index);
int 	asd_get_number_of_hbas_present(void);
struct asd_target * 
	asd_get_os_target_from_port(struct asd_softc *asd,
				    struct asd_port *port,
				    struct asd_domain *dm);
struct asd_device *
	asd_get_device_from_lun(struct asd_softc *asd, 
				struct asd_target *targ, uint8_t *saslun);
int 	asd_get_os_platform_map_from_sasaddr(struct asd_softc *asd, 
					     struct asd_port *port,
					     uint8_t *sasaddr, uint8_t *saslun, 
					     uint8_t *host, uint8_t *bus, 
					     uint8_t *target, uint8_t *lun);
struct asd_port *
	asd_get_sas_addr_from_platform_map(struct asd_softc *asd, 
					   uint8_t host, uint8_t bus, 
					   uint8_t target, uint8_t lun, 
					   uint8_t *sasaddr, uint8_t *saslun);
struct asd_target * 
	asd_get_sas_target_from_sasaddr(struct asd_softc *asd, 
					struct asd_port *port, 
					uint8_t *sasaddr);
struct asd_target * 
	asd_get_os_target_from_sasaddr(struct asd_softc *asd, 
			 	       struct asd_domain *dm, 
				       uint8_t *sasddr);
struct scb *
	asd_find_pending_scb_by_qtag(struct asd_softc *asd, uint32_t qtag);

int	asd_hwi_check_cmd_pending(struct asd_softc *asd, struct scb *scb, 
		       		  struct asd_done_list *dl);

/* indicates that the scsi_cmnd is generated by CSMI */
#define ASD_CSMI_COMMAND 0xfaceface

#if (BITS_PER_LONG > 32) || \
    (defined CONFIG_HIGHMEM64G && defined CONFIG_HIGHIO)
	#define ASD_DMA_64BIT_SUPPORT		1
#else
	#define	ASD_DMA_64BIT_SUPPORT		0
#endif 

/*************************** Linux DMA Wrappers *******************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#define	asd_alloc_coherent(asd, size, bus_addr_ptr) \
	dma_alloc_coherent(asd->dev, size, bus_addr_ptr, /*flag*/0)

#define	asd_free_coherent(asd, size, vaddr, bus_addr) \
	dma_free_coherent(asd->dev, size, vaddr, bus_addr)

#define	asd_map_single(asd, buf, size, direction) \
	dma_map_single(asd->dev, buf, size, direction)

#define	asd_unmap_single(asd, busaddr, size, direction) \
	dma_unmap_single(asd->dev, busaddr, size, direction)

#define	asd_map_sg(asd, sg_list, num_sg, direction) \
	dma_map_sg(asd->dev, sg_list, num_sg, direction)

#define	asd_unmap_sg(asd, sg_list, num_sg, direction) \
	dma_unmap_sg(asd->dev, sg_list, num_sg, direction)

#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0) */

#define	asd_alloc_coherent(asd, size, bus_addr_ptr) \
	pci_alloc_consistent(asd->dev, size, bus_addr_ptr)

#define	asd_free_coherent(asd, size, vaddr, bus_addr) \
	pci_free_consistent(asd->dev, size, vaddr, bus_addr)

#define	asd_map_single(asd, buf, size, direction) \
	pci_map_single(asd->dev, buf, size, direction)

#define	asd_unmap_single(asd, busaddr, size, direction) \
	pci_unmap_single(asd->dev, busaddr, size, direction)

#define	asd_map_sg(asd, sg_list, num_sg, direction) \
	pci_map_sg(asd->dev, sg_list, num_sg, direction)

#define	asd_unmap_sg(asd, sg_list, num_sg, direction) \
	pci_unmap_sg(asd->dev, sg_list, num_sg, direction)
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)

#define asd_set_dma_mask(asd, mask) dma_set_mask(asd->dev, mask)
#define asd_set_consistent_dma_mask(asd, mask) \
	pci_set_consistent_dma_mask(asd_dev_to_pdev(asd->dev), mask)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,3)

/*
 * Device softc is NULL for EISA devices.
 */
#define asd_set_dma_mask(asd, mask) 			\
	((asd)->dev == NULL ? 0 : pci_set_dma_mask(asd->dev, mask))

/* Always successfull in 2.4.X kernels */
#define asd_set_consistent_dma_mask(asd, mask) (0)

#else
/*
 * Device softc is NULL for EISA devices.
 * Always "return" 0 for success.
 */
#define asd_set_dma_mask(asd, mask)			\
    (((asd)->dev == NULL)				\
     ? 0						\
     : (((asd)->dev->dma_mask = mask) && 0))

/* Always successfull in 2.4.X kernels */
#define asd_set_consistent_dma_mask(asd, mask) (0)

#endif

/* Forward compatibility */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
typedef struct device	*asd_dev_t;
#else
typedef struct pci_dev	*asd_dev_t;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#define	asd_dev_to_pdev(dev)		to_pci_dev(dev)
#define	asd_pdev_to_dev(pdev)		(&pdev->dev)
#else
#define	asd_dev_to_pdev(dev)		(dev)
#define	asd_pdev_to_dev(pdev)		(pdev)
#endif

#define	asd_pci_dev(asd)		asd_dev_to_pdev((asd)->dev)

#define ASD_GET_PADR(addr)		((uint32_t) (addr))
#if (BITS_PER_LONG > 32)
#define ASD_GET_PUADR(addr)		((uint32_t) ((addr) >> 32))
#else
#define ASD_GET_PUADR(addr)		((uint32_t) (0))
#endif

typedef enum {
	ASD_SCB_UP_EH_SEM	= 0x01,
	ASD_TIMEOUT_ACTIVE	= 0x02,
	ASD_RELEASE_SIMQ	= 0x04,
} asd_scb_flags;

/**************************** Front End Queues ********************************/
/*
 * Data structure used to cast the Linux struct scsi_cmnd to something
 * that allows us to use the queue macros.  The linux structure has
 * plenty of space to hold the links fields as required by the queue
 * macros, but the queue macors require them to have the correct type.
 */
struct asd_cmd_internal {
	/* Area owned by the Linux scsi layer. */
	uint8_t	private[__builtin_offsetof(struct scsi_cmnd, SCp.Status)];
	struct list_head		links;
	uint32_t			end;
};

union asd_cmd {
	struct asd_cmd_internal	icmd;
	struct scsi_cmnd	scsi_cmd;
};

#define acmd_icmd(cmd)		((cmd)->icmd)
#define acmd_scsi_cmd(cmd) 	((cmd)->scsi_cmd)
#define acmd_links 		icmd.links

/************************** Razor Architecture Layers ************************/
/*
 * The following typedefs define the protocols for each layer of the Razor
 * architecture.
 */

/*
 * The Command Set layer consists of everything having to do with the type
 * of command that is sent to the target.
 */
typedef enum {
	ASD_COMMAND_SET_UNKNOWN,
	ASD_COMMAND_SET_SCSI,
	ASD_COMMAND_SET_ATA,
	ASD_COMMAND_SET_ATAPI,
	ASD_COMMAND_SET_SMP,
	ASD_COMMAND_SET_BAD
} COMMAND_SET_TYPE;

/*
 * The Device Protocol layer are things that are specific to a device, 
 * regardless of which command set is used.  For example, a device using
 * ASD_DEVICE_PROTOCOL_ATA might use the command set ASD_COMMAND_SET_ATA or
 * ASD_COMMAND_SET_ATAPI.
 */
typedef enum {
	ASD_DEVICE_PROTOCOL_UNKNOWN,
	ASD_DEVICE_PROTOCOL_SCSI,
	ASD_DEVICE_PROTOCOL_ATA,
	ASD_DEVICE_PROTOCOL_SMP
} DEVICE_PROTOCOL_TYPE;

/*
 * The transport layer can be thought of as the frame which is used to send
 * to a device of type DEVICE_PROTOCOL that supports the COMMAND_SET_TYPE
 * command set.
 */
typedef enum {
	ASD_TRANSPORT_UNKNOWN,
	ASD_TRANSPORT_SSP,
	ASD_TRANSPORT_SMP,
	ASD_TRANSPORT_STP,
	ASD_TRANSPORT_ATA
} TRANSPORT_TYPE;

/* 
 * The management layer deals with issues of routing.
 */
typedef enum {
	ASD_DEVICE_NONE,
	ASD_DEVICE_FANOUT_EXPANDER,
	ASD_DEVICE_EDGE_EXPANDER,
	ASD_DEVICE_END,
	ASD_DEVICE_UNKNOWN
} MANAGEMENT_TYPE;

/*
 * The link layer refers to the link that is connected to the initiator.
 */
typedef enum {
	ASD_LINK_UNKNOWN,
	ASD_LINK_SAS,
	ASD_LINK_SATA,
	ASD_LINK_GPIO,
	ASD_LINK_I2C
} LINK_TYPE;

typedef union asd_cmd *asd_io_ctx_t;

/* Discovery include file */
#include "adp94xx_discover.h"

/* HWI include file */
#include "adp94xx_hwi.h"

/*************************** Device Data Structures ***************************/
/*
 * A per probed device structure used to deal with some error recovery
 * scenarios that the Linux mid-layer code just doesn't know how to
 * handle.  The structure allocated for a device only becomes persistent
 * after a successfully completed inquiry command to the target when
 * that inquiry data indicates a lun is present.
 */
typedef enum {
	ASD_DEV_UNCONFIGURED	 = 0x0001,
	ASD_DEV_FREEZE_TIL_EMPTY = 0x0002, /* Freeze until active is empty */
	ASD_DEV_TIMER_ACTIVE	 = 0x0004, /* Our timer is active */
	ASD_DEV_ON_RUN_LIST	 = 0x0008, /* Queued to be run later */
	ASD_DEV_Q_BASIC		 = 0x0010, /* Allow basic device queuing */
	ASD_DEV_Q_TAGGED	 = 0x0020, /* Allow full SCSI2 cmd queueing */
	ASD_DEV_SLAVE_CONFIGURED = 0x0040, /* Device has been configured. */
	ASD_DEV_DESTROY_WAS_ACTIVE = 0x0080, /* Device has active IO(s) when
						the ITNL timer expired. */
	ASD_DEV_DPC_ACTIVE	= 0x0100   /* There is an active DPC task,*/
} asd_dev_flags;

struct asd_target;
struct asd_device {
	struct list_head 	 links;
	struct list_head	 busyq;
	/*
	 * The number of transactions currently queued to the device.
	 */
	int			 active;
	/*
	 * The currently allowed number of transactions that can be queued to
	 * the device.  Must be signed for conversion from tagged to untagged
	 * mode where the device may have more than one outstanding active 
	 * transaction.
	 */
	int			 openings;
	/*
	 * A positive count indicates that this device's queue is halted.
	 */
	u_int			 qfrozen;
	/*
	 * Cumulative command counter.
	 */
	u_long			 commands_issued;
	/*
	 * The number of tagged transactions when running at our current 
	 * opening level that have been successfully received by this 
	 * device since the last QUEUE FULL.
	 */
	u_int			 tag_success_count;
#define ASD_TAG_SUCCESS_INTERVAL 50

	asd_dev_flags		 flags;
	/*
	 * Per device timer and task.
	 */
	struct timer_list	 timer;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,41)	
	struct tq_struct	 taskq;
#else
	struct work_struct	 workq;
#endif	
	/*
	 * The high limit for the tags variable.
	 */
	u_int			 maxtags;
	/*
	 * The computed number of tags outstanding
	 * at the time of the last QUEUE FULL event.
	 */
	u_int			 tags_on_last_queuefull;
	/*
	 * How many times we have seen a queue full with the same 
	 * number of tags.  This is used to stop our adaptive queue depth 
	 * algorithm on devices with a fixed number of tags.
	 */
	u_int			 last_queuefull_same_count;
#define ASD_LOCK_TAGS_COUNT 50

	/* 
	 * Device mapping path for the OS.
	 * This is a duplicate of info in scsi_device and
	 * should be discarded soon.
	 */
	u_int			 ch;
	u_int			 id;
	u_int			 lun;
	uint8_t			 saslun[SAS_LUN_LEN];
	Scsi_Device	       	*scsi_device;
	struct asd_target	*target;
#ifdef MULTIPATH_IO
	struct asd_target	*current_target;
#endif
};

typedef enum {
	ASD_TARG_FLAGS_NONE		= 0x0000,
	ASD_TARG_IN_RECOVERY		= 0x0001,
	ASD_TARG_ONLINE			= 0x0002,
	ASD_TARG_RESEEN			= 0x0004,
	ASD_TARG_HOT_REMOVED		= 0x0008,
	ASD_TARG_HOT_ADDED		= 0x0010,
	ASD_TARG_NEEDS_MAP		= 0x0020,
	ASD_TARG_MAPPED			= 0x0040,
	ASD_TARG_MAP_BOOT		= 0x0080
} asd_targ_flags;

/* 
 * Contains pertinent fields needed to generete a hardware DDB.
 */  
struct asd_ddb_data {
	uint8_t			sas_addr[SAS_ADDR_LEN];
	uint8_t			hashed_sas_addr[HASHED_SAS_ADDR_LEN];
	uint8_t			conn_rate;
	uint16_t		itnl_const;
	/* Index to DDB entry. */
	uint16_t		conn_handle;
	uint16_t		sister_ddb;
	/*
	 * Indicates whether the device port protocol is SSP, STP, SMP, 
	 * SATA direct attched or SATA Port Multi. This will be used when
	 * setting value for OPEN field.
	 */   
	uint8_t			attr;
	/*
	 * OPEN bit will be set if the device port is connected by
	 * SSP, STP or SMP. In addition for STP, SUPPORT AFFILIATION and 
	 * STP AFFLIATION policy need to be set. 
	 * Otherwise, for SATA direct attached or SATA Port Multi, OPEN bit
 	 * shall not be set.  
	 */  
	uint8_t			open_affl; 	
	/*
	 * Initial FIS status.
	 */
	uint8_t			sata_status;
};

#define FIS_LENGTH		20

/*
 * Defines for the features_state member
 */
#define SATA_USES_DMA			0x0001
#define SATA_USES_48BIT			0x0002
#define SATA_USES_QUEUEING		0x0004
#define SATA_USES_WRITE_FUA		0x0008
#define SATA_USES_REMOVABLE		0x0010
#define SATA_USES_WRITE_BUFFER		0x0020
#define SATA_USES_READ_BUFFER		0x0040
#define SATA_USES_WRITE_CACHE		0x0080
#define SATA_USES_READ_AHEAD		0x0100
#define SATA_USES_SMART			0x0200
#define SATA_USES_UDMA			0x0400

/*
 * Defines for the features_enabled member (to store capability)
 */
#define WRITE_CACHE_FEATURE_ENABLED	0x0001
#define READ_AHEAD_FEATURE_ENABLED	0x0002
#define SMART_FEATURE_ENABLED		0x0004
#define NEEDS_XFER_SETFEATURES		0x0008

int
asd_sata_identify_build(
struct asd_softc	*asd,
struct asd_target	*target,
struct scb		*scb
);

// ----------------------------------------------------------------------------

/*
 * For devices supporting the SCSI command set
 */
struct asd_scsi_command_set {
	unsigned			num_luns;
	uint32_t			flags;
	uint64_t			*luns;
	uint8_t				*ident;
	unsigned			ident_len;
	uint8_t				*inquiry;
};

struct asd_smp_command_set {
	struct SMPResponseReportManufacturerInfo
					manufacturer_info;
};

/*
 * For devices supporting the ATA command set
 */
struct asd_ata_command_set {
	/*
	 * There are a few differences in the structures for the data that
	 * is returned by IDENTIFY and PACKET IDENTIFY.  We are using the
	 * same structure for both ATA and ATAPI.
	 */
	struct hd_driveid		adp_hd_driveid;

	/*
	 * These members are in the command set structure because they are
	 * deriviced by, or control features and functions specified in the
	 * adp_hd_driveid struture.
	 */
	unsigned			features_state;
	unsigned			features_enabled;
	unsigned			dma_mode_level;
};

/*
 * For devices supporting the ATAPI command set
 */
struct asd_atapi_command_set {
	/*
	 * There are a few differences in the structures for the data that
	 * is returned by IDENTIFY and PACKET IDENTIFY.  We are using the
	 * same structure for both ATA and ATAPI.
	 */
	struct hd_driveid		adp_hd_driveid;

	/*
	 * These members are in the command set structure because they are
	 * deriviced by, or control features and functions specified in the
	 * adp_hd_driveid struture.
	 */
	unsigned			features_state;
	unsigned			features_enabled;
	unsigned			dma_mode_level;
};

/*
 * These are members that are specific to a specific type of command set.
 */
union asd_command_set {
	struct asd_scsi_command_set	scsi_command_set;
	struct asd_ata_command_set	ata_command_set;
	struct asd_atapi_command_set	atapi_command_set;
	struct asd_smp_command_set	smp_command_set;
};

#define ata_cmdset	command_set.ata_command_set
#define scsi_cmdset	command_set.scsi_command_set
#define atapi_cmdset	command_set.atapi_command_set
#define smp_cmdset	command_set.smp_command_set


// ----------------------------------------------------------------------------

struct asd_ata_device_protocol {
	uint8_t				initial_fis[FIS_LENGTH];
};

union asd_device_protocol {
	struct asd_ata_device_protocol	ata_device_protocol;
};

/*
 * Defines for the menagement_flags member
 */
#define DEVICE_SET_ROOT		0x0001

struct asd_target {
	/*
	 * A positive count indicates that this
	 * target's queue is halted.
	 */
	u_int			  qfrozen;

	/*
	 * XXX Use hash table for sparse 8byte lun support???
	 */
	struct asd_device	 *devices[ASD_MAX_LUNS];
	u_int			  target_id;
	int			  refcount;
	struct asd_softc	 *softc;
	asd_targ_flags	  	  flags;
	struct asd_domain	 *domain;

	/*
	 * Per target timer.
	 */
	struct timer_list	  timer;

	/*
	 * Command Set Layer (SCSI, ATAPI, ATA, SMP)
	 * --------------------------------------------------------
	 */
	COMMAND_SET_TYPE		command_set_type;
	union asd_command_set		command_set;

	/*
	 * Device Protocol Layer (SCSI, ATA, SMP)
	 * --------------------------------------------------------
	 */
	DEVICE_PROTOCOL_TYPE		device_protocol_type;
	union asd_device_protocol	device_protocol;

	/*
	 * Transport Layer (SSP, SMP, STP, FIS)
	 * --------------------------------------------------------
	 */
	TRANSPORT_TYPE			transport_type;

	/*
	 * Management Layer (DIRECT, EXPANDER, FANOUT)
	 * --------------------------------------------------------
	 */
	MANAGEMENT_TYPE			management_type;
	unsigned			num_phys;
	unsigned			num_route_indexes;
	unsigned			configurable_route_table;
	uint16_t			*route_indexes;
	struct SMPResponseDiscover	*Phy;
	uint8_t				*RouteTable;
	uint32_t			management_flags;

	/*
	 * Our parent SAS device (e.g. expander)
	 * in this SAS domain.
	 */
	struct asd_target	 	*parent;
	/*
	 * Our children SAS devices (if not end device)
	 * in this SAS domain.
	 */
	struct list_head	  	children;
	/*
	 * List of targets that are connected to a parent 
	 * expander (children)
	 */
	struct list_head	  	siblings;
	/*
	 * List links for chaining together all targets
	 * in this SAS domain on to the port object.
	 */
	struct list_head	  	all_domain_targets;
	/*
	 * List links of targets that need to be validated
	 * because they have been hot-added or hot-removed.
	 */
	struct list_head	  	validate_links;   
	/*
	 * List links of target structures that map to the
	 * same physical sevice.
	 */
	struct list_head	  	multipath;   

	/*
	 * Link Layer (SAS, SATA, GPIO, I2C)
	 * --------------------------------------------------------
	 */
	LINK_TYPE			link_type;
	struct asd_ddb_data	  	ddb_profile;

	/*
	 * Controller port used to route to
	 * this device.
	 */
	struct asd_port		 	*src_port;
};

struct asd_domain {
	/*
	 * XXX Use Hash table to support
	 * large/sparse target configurations.
	 */
	struct asd_target 	*targets[ASD_MAX_TARGET_IDS];
	u_int			 channel_mapping;
	u_int			 refcount;
};

typedef enum {
	ASD_DISCOVERY_ACTIVE	= 0x01,
	ASD_DISCOVERY_INIT	= 0x02,
	ASD_DISCOVERY_SHUTDOWN	= 0x04,
	ASD_RECOVERY_SHUTDOWN	= 0x08,
#ifdef ASD_EH_SIMULATION
	ASD_EH_SIMUL_SHUTDOWN	= 0x10
#endif		
} asd_platform_flags;

struct asd_scb_platform_data {
	struct asd_target 	*targ;
	struct asd_device	*dev;
	asd_scb_flags	 	 flags;
	dma_addr_t		 buf_busaddr;
	/*
	 * Timeout timer tick, used to timeout internal cmd that is 
         * sent to the sequencer or target.
	 */	 
	struct timer_list	 timeout;	 
};

struct asd_platform_data {
	spinlock_t		  spinlock;
	struct Scsi_Host	 *scsi_host;

	/*
	 * Channel to Domain Mapping.
	 */
	struct asd_domain	**domains;
	u_int			  num_domains;

	/*
	 * Queued device I/O scheduling support.
	 */
	struct list_head	  pending_os_scbs;
	struct list_head 	  device_runq;
	struct tasklet_struct	  runq_tasklet;
	struct tasklet_struct	  unblock_tasklet;
	u_int			  qfrozen;

	/*
	 * Completion Ordering Queue.
	 */
	struct list_head 	  completeq;

	/*
	 * LRU target DDB ageing queue.  Used to
	 * select candidates for DDB recycling.
	 * LFU would be a better scheme and could
	 * be achieved in O(logN) using a heap
	 * queue, but keep this simple for now.
	 */
	struct list_head 	  lru_ddb_q;

	/*
	 * Discovery Thread Support.
	 */
	pid_t			  discovery_pid;
	struct semaphore	  discovery_sem;
	struct semaphore	  discovery_ending_sem;

	/*
	 * Error Recovery Thread Support.
	 */
	pid_t			  ehandler_pid;
	struct semaphore	  ehandler_sem;
	struct semaphore	  ehandler_ending_sem;

#ifdef ASD_EH_SIMULATION
	/*
	 * EH Recovery Simulation thread.
         */
	pid_t			  eh_simul_pid;
	struct semaphore	  eh_simul_sem;
#endif
	
	/*
	 * Wait Queue.
         */	
	wait_queue_head_t	  waitq;

	/*
	 * Mid-layer error recovery entry point semaphore.
	 */
	struct semaphore	  eh_sem;
	struct semaphore	  wait_sem;

	asd_platform_flags  	  flags;
};

/*
 * Internal data structures.
 */
typedef struct asd_init_status {
	uint8_t		asd_notifier_enabled;
	uint8_t		asd_pci_registered;
	uint8_t		asd_irq_registered;
	uint8_t		asd_ioctl_registered;
	uint8_t		asd_init_state;
} asd_init_status_t;


/* SMP Locking mechanism routines */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) || defined(SCSI_HAS_HOST_LOCK))
#define	SCSI_ML_HAS_HOST_LOCK	1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#define asd_assign_host_lock(asd)				\
	scsi_assign_lock((asd)->platform_data->scsi_host,	\
			 &(asd)->platform_data->spinlock)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)		\
		&& defined(ASD_RED_HAT_LINUX_KERNEL)
#define asd_assign_host_lock(asd)				\
do {								\
	(asd)->platform_data->scsi_host->host_lock =		\
	    &(asd)->platform_data->spinlock;			\
} while (0)
#else
#define asd_assign_host_lock(asd)				\
do {								\
	(asd)->platform_data->scsi_host->lock =			\
	    &(asd)->platform_data->spinlock;			\
} while (0)
#endif
#else	/* !SCSI_ML_HAS_HOST_LOCK */
#define	SCSI_ML_HAS_HOST_LOCK	0
#define asd_assign_host_lock(asd)
#endif	/* !SCSI_ML_HAS_HOST_LOCK */

/* OS utility wrappers */
#define asd_alloc_mem(size, flags)	kmalloc(size, flags)
#define asd_free_mem(ptr)		kfree(ptr)
 
/* Debug Logging macro */

#define ASD_DBG_INIT		0x01
#define ASD_DBG_INFO		0x02
#define ASD_DBG_RUNTIME_B	0x04
#define ASD_DBG_RUNTIME		0x10
#define ASD_DBG_ISR		0x20
#define ASD_DBG_ERROR		0x40


/* Debug mask to control the Debug Level printout. */
extern u_int	debug_mask;

#ifdef ASD_DEBUG

#define asd_dprint(fmt, ...) printk(KERN_NOTICE fmt, ## __VA_ARGS__)

#define asd_log(debug_level, fmt, args...)			\
do {								\
	if ((debug_level & debug_mask) != 0) {			\
		printk("%s(%d) : ", __FUNCTION__, __LINE__);	\
		printk(fmt, ##args);				\
	}							\
} while (0)

/* For initial debugging purpose only. */
#define IN	0
#define OUT	1
#define LINE	2

#define ASD_DBG_FUNC(x)							\
do {									\
	if (x == IN) {							\
		printk("+++ Entering Function: %s:%d.\n", __FUNCTION__, \
			__LINE__);					\
	} else if (x == OUT) {						\
		printk("--- Exiting Function: %s:%d.\n", __FUNCTION__,  \
			__LINE__);					\
	} else {							\
		printk("*** %s (%d).\n", __FUNCTION__, __LINE__);	\
	}								\
} while (0)

#if KDB_ENABLE
#define ASSERT(expression)						\
	if (!(expression)) {						\
		printk("assertion failed: %s, file: %s, line: %d\n",	\
			#expression, __FILE__, __LINE__);		\
		KDB_ENTER();						\
	}
#else /* KDB_ENABLE */
#define ASSERT(expression)						\
	if (!(expression)) {						\
		panic("assertion failed: %s, file: %s, line: %d\n",	\
			#expression, __FILE__, __LINE__);		\
	}
#endif /* KDB_ENABLE */

#else /* ASD_DEBUG */

#define asd_dprint(fmt, ...) 

#define asd_log(debug_level, fmt, args...)	/**/
#define ASD_DBG_FUNC(x)				/**/

#define ASSERT(expression)

#endif /* ASD_DEBUG */ 

#define asd_print(fmt, args...)		printk(fmt, ##args)

#define ASD_DUMP_REG(reg)		/**/

#ifndef IRQ_RETVAL
typedef void	irqreturn_t;
#define IRQ_RETVAL(x)	/**/
#endif

/*
 * SCSI Status Byte
 */
#define	SCSI_STATUS_OK			0x00
#define	SCSI_STATUS_CHECK_COND		0x02
#define	SCSI_STATUS_COND_MET		0x04
#define	SCSI_STATUS_BUSY		0x08
#define SCSI_STATUS_INTERMED		0x10
#define SCSI_STATUS_INTERMED_COND_MET	0x14
#define SCSI_STATUS_RESERV_CONFLICT	0x18
#define SCSI_STATUS_CMD_TERMINATED	0x22	/* Obsolete in SAM-2 */
#define SCSI_STATUS_QUEUE_FULL		0x28
#define SCSI_STATUS_ACA_ACTIVE		0x30
#define SCSI_STATUS_TASK_ABORTED	0x40

struct scsi_sense_data
{
	uint8_t error_code;
#define	SSD_ERRCODE			0x7F
#define		SSD_CURRENT_ERROR	0x70
#define		SSD_DEFERRED_ERROR	0x71
#define	SSD_ERRCODE_VALID	0x80	
	uint8_t segment;
	uint8_t flags;
#define	SSD_KEY				0x0F
#define		SSD_KEY_NO_SENSE	0x00
#define		SSD_KEY_RECOVERED_ERROR	0x01
#define		SSD_KEY_NOT_READY	0x02
#define		SSD_KEY_MEDIUM_ERROR	0x03
#define		SSD_KEY_HARDWARE_ERROR	0x04
#define		SSD_KEY_ILLEGAL_REQUEST	0x05
#define		SSD_KEY_UNIT_ATTENTION	0x06
#define		SSD_KEY_DATA_PROTECT	0x07
#define		SSD_KEY_BLANK_CHECK	0x08
#define		SSD_KEY_Vendor_Specific	0x09
#define		SSD_KEY_COPY_ABORTED	0x0a
#define		SSD_KEY_ABORTED_COMMAND	0x0b		
#define		SSD_KEY_EQUAL		0x0c
#define		SSD_KEY_VOLUME_OVERFLOW	0x0d
#define		SSD_KEY_MISCOMPARE	0x0e
#define		SSD_KEY_RESERVED	0x0f			
#define	SSD_ILI		0x20
#define	SSD_EOM		0x40
#define	SSD_FILEMARK	0x80
	uint8_t info[4];
	uint8_t extra_len;
	uint8_t cmd_spec_info[4];
	uint8_t add_sense_code;
	uint8_t add_sense_code_qual;
	uint8_t fru;
	uint8_t sense_key_spec[3];
#define	SSD_SCS_VALID		0x80
#define SSD_FIELDPTR_CMD	0x40
#define SSD_BITPTR_VALID	0x08
#define SSD_BITPTR_VALUE	0x07
#define SSD_MIN_SIZE 18
	uint8_t extra_bytes[14];
#define SSD_FULL_SIZE sizeof(struct scsi_sense_data)
};

static inline void	scsi_ulto2b(uint32_t val, uint8_t *bytes);
static inline void	scsi_ulto3b(uint32_t val, uint8_t *bytes);
static inline void	scsi_ulto4b(uint32_t val, uint8_t *bytes);
static inline uint32_t	scsi_2btoul(uint8_t *bytes);
static inline uint32_t	scsi_3btoul(uint8_t *bytes);
static inline int32_t	scsi_3btol(uint8_t *bytes);
static inline uint32_t 	scsi_4btoul(uint8_t *bytes);

static inline void
scsi_ulto2b(uint32_t val, uint8_t *bytes)
{
	bytes[0] = (val >> 8) & 0xff;
	bytes[1] = val & 0xff;
}

static inline void
scsi_ulto3b(uint32_t val, uint8_t *bytes)
{
	bytes[0] = (val >> 16) & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = val & 0xff;
}

static inline void
scsi_ulto4b(uint32_t val, uint8_t *bytes)
{
	bytes[0] = (val >> 24) & 0xff;
	bytes[1] = (val >> 16) & 0xff;
	bytes[2] = (val >> 8) & 0xff;
	bytes[3] = val & 0xff;
}

static inline uint32_t
scsi_2btoul(uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static inline uint32_t
scsi_3btoul(uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 16) |
	     (bytes[1] << 8) |
	     bytes[2];
	return (rv);
}

static inline int32_t 
scsi_3btol(uint8_t *bytes)
{
	uint32_t rc = scsi_3btoul(bytes);
 
	if (rc & 0x00800000)
		rc |= 0xff000000;

	return (int32_t) rc;
}

static inline uint32_t
scsi_4btoul(uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 24) |
	     (bytes[1] << 16) |
	     (bytes[2] << 8) |
	     bytes[3];
	return (rv);
}

/*********************** Transaction Access Wrappers **************************/
static inline void	asd_cmd_set_host_status(Scsi_Cmnd *cmd, u_int);
static inline void 	asd_cmd_set_scsi_status(Scsi_Cmnd *cmd, u_int);
static inline uint32_t 	asd_cmd_get_host_status(Scsi_Cmnd *cmd);
static inline uint32_t 	asd_cmd_get_scsi_status(Scsi_Cmnd *cmd);
static inline void 	asd_freeze_scb(struct scb *scb);
static inline void 	asd_unfreeze_scb(struct scb *scb);

#define CMD_DRIVER_STATUS_MASK	0xFF000000
#define CMD_DRIVER_STATUS_SHIFT	24
#define CMD_HOST_STATUS_MASK	0x00FF0000
#define CMD_HOST_STATUS_SHIFT	16
#define CMD_MSG_STATUS_MASK	0x0000FF00
#define CMD_MSG_STATUS_SHIFT	8
#define CMD_SCSI_STATUS_MASK	0x000000FF
#define CMD_SCSI_STATUS_SHIFT	0
#define CMD_REQ_INPROG		0xFF

static inline
void asd_cmd_set_driver_status(Scsi_Cmnd *cmd, u_int status)
{
	cmd->result &= ~CMD_DRIVER_STATUS_MASK;
	cmd->result |= status << CMD_DRIVER_STATUS_SHIFT;
}

static inline
void asd_cmd_set_host_status(Scsi_Cmnd *cmd, u_int status)
{
	cmd->result &= ~CMD_HOST_STATUS_MASK;
	cmd->result |= status << CMD_HOST_STATUS_SHIFT;
}

static inline
void asd_cmd_set_scsi_status(Scsi_Cmnd *cmd, u_int status)
{
	cmd->result &= ~CMD_SCSI_STATUS_MASK;
	cmd->result |= status;
}

static inline
uint32_t asd_cmd_get_host_status(Scsi_Cmnd *cmd)
{
	return ((cmd->result & CMD_HOST_STATUS_MASK) >> CMD_HOST_STATUS_SHIFT);
}

static inline
uint32_t asd_cmd_get_scsi_status(Scsi_Cmnd *cmd)
{
	return ((cmd->result & CMD_SCSI_STATUS_MASK) >> CMD_SCSI_STATUS_SHIFT);
}

static inline void
asd_freeze_scb(struct scb *scb)
{
	if ((scb->flags & SCB_DEV_QFRZN) == 0) {
		scb->flags |= SCB_DEV_QFRZN;
		scb->platform_data->dev->qfrozen++;
        }
}

static inline void
asd_unfreeze_scb(struct scb *scb)
{
	if ((scb->flags & SCB_DEV_QFRZN) != 0) {
		scb->flags &= ~SCB_DEV_QFRZN;
		scb->platform_data->dev->qfrozen--;
        }
}

// TODO - where is right spot for these???
typedef enum {
	ASD_COMMAND_BUILD_OK,
	ASD_COMMAND_BUILD_FAILED,
	ASD_COMMAND_BUILD_FINISHED
} ASD_COMMAND_BUILD_STATUS;

ASD_COMMAND_BUILD_STATUS 
	asd_setup_data(struct asd_softc *asd, struct scb *scb, Scsi_Cmnd *cmd);

DISCOVER_RESULTS
	asd_do_discovery(struct asd_softc *asd, struct asd_port *port);

DISCOVER_RESULTS
	asd_run_state_machine(struct state_machine_context *sm_contextp);

int	asd_platform_alloc(struct asd_softc *asd);
void	asd_platform_free(struct asd_softc *asd);

struct asd_scb_platform_data *
	asd_alloc_scb_platform_data(struct asd_softc *asd);
void	asd_free_scb_platform_data(struct asd_softc *asd,
				   struct asd_scb_platform_data *pdata);
void	asd_unmap_scb(struct asd_softc *asd, struct scb *scb);
void	asd_recover_cmds(struct asd_softc *asd);
void	asd_hwi_release_sata_spinup_hold(struct asd_softc *asd,
					 struct asd_phy	*phy);

struct asd_target *
	asd_alloc_target(struct asd_softc *asd, struct asd_port *src_port);
void	asd_free_target(struct asd_softc *asd, struct asd_target *targ);
int	asd_map_target(struct asd_softc *asd, struct asd_target *targ);

struct asd_device *
	asd_alloc_device(struct asd_softc *asd, struct asd_target *targ, 
			 u_int ch, u_int id, u_int lun);
void	asd_free_device(struct asd_softc *asd, struct asd_device *dev);
struct asd_device *
	asd_get_device(struct asd_softc *asd, u_int ch, u_int id,
		       u_int lun, int alloc);
void	asd_remap_device(struct asd_softc *asd, struct asd_target *target,
			 struct asd_target *multipath_target);
void	asd_timed_run_dev_queue(u_long arg);
#if defined(__VMKLNX__)
void	asd_destroy_device(struct work_struct *arg);
#else /* !defined(__VMKLNX__) */
void	asd_destroy_device(void *arg);
#endif /* defined(__VMKLNX__) */

#ifndef list_for_each_entry_safe
/**
 * list_for_each_entry_safe - iterate over list of given type safe against 
 *                              removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))
#endif

#ifndef list_for_each_entry
/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member),	\
		     prefetch(pos->member.next))

#endif

#define list_move_all(to_list, from_list)				\
	if (!list_empty(from_list)) {					\
		(to_list)->next = (from_list)->next;			\
		(to_list)->prev = (from_list)->prev;			\
		(from_list)->next->prev = to_list;			\
		(from_list)->prev->next = to_list;			\
		INIT_LIST_HEAD(from_list);				\
	} else {							\
		INIT_LIST_HEAD(to_list);				\
	}

#define list_copy(to_list, from_list)					\
do {									\
	(to_list)->next = (from_list)->next;				\
	(to_list)->prev = (from_list)->prev;				\
} while (0)

void asd_hwi_set_ddbsite_byte(struct asd_softc *asd,
			      uint16_t site_offset, uint8_t val);
void asd_hwi_set_ddbsite_word(struct asd_softc *asd,
			      uint16_t site_offset, uint16_t val);
void asd_hwi_set_ddbsite_dword(struct asd_softc *asd,
			       uint16_t site_offset, uint32_t val);
uint8_t asd_hwi_get_ddbsite_byte(struct asd_softc *asd, 
				 uint16_t site_offset);
uint16_t asd_hwi_get_ddbsite_word(struct asd_softc *asd, 
				  uint16_t site_offset);
uint32_t asd_hwi_get_ddbsite_dword(struct asd_softc *asd, 
				   uint16_t site_offset);

#endif /* ADP94XX_OSM_H */ 
