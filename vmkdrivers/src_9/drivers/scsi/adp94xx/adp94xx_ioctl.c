/*
 * Adaptec ADP94xx SAS HBA driver for Linux - IOCTL interface for Linux.
 *
 * Written by : Naveen Chandrasekaran <naveen_chandrasekaran@adaptec.com>
 * Modifications and cleanups: Luben Tuikov <luben_tuikov@adaptec.com>
 * 
 * Copyright (c) 2004-05 Adaptec Inc.
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
 * $Id: //depot/razor/linux/src/adp94xx_ioctl.c#11 $
 *
 */	

#include "adp94xx_osm.h"
#include "adp94xx_inline.h"
#include "adp94xx_ioctl.h"

#ifdef __x86_64__
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)	
#if !defined(__VMKLNX__)
#include <linux/ioctl32.h>
#endif /* !defined(__VMKLNX__) */
#include <linux/syscalls.h>
#else
#include <asm/ioctl32.h>
#endif
#endif

/* IOCTL control device name */
#define ASD_CTL_DEV_NAME	"asdctl"
static int asd_ctl_major;

/* protos for char device entry points */
static int asd_ctl_open(struct inode *inode, struct file *file);
static int asd_ctl_close(struct inode *inode, struct file *file);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
static int asd_ctl_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg);
#else
static long asd_unlocked_ctl_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);
static long asd_compat_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg);
#endif

/* protos for ioctl handlers */
static int asd_ctl_get_cntlr_config(unsigned long arg);

/* protos for Adaptec NVRAM access ioctl handlers */
static int asd_ctl_sas_get_adpt_cntlr_conf(unsigned long arg);
static int asd_ctl_sas_get_nv_seg_prop(unsigned long arg);
static int asd_ctl_sas_write_nv_seg(unsigned long arg);
static int asd_ctl_sas_read_nv_seg(unsigned long arg);

/* protos for helper - work horse routines for ioctl handlers */
static int asd_ctl_write_to_nvram(struct asd_softc *asd,
	struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER *pasd_write_nv_buf,
	uint8_t *psource_buffer);

static int asd_ctl_read_from_nvram(struct asd_softc *asd,
	struct ASD_SAS_READ_NV_SEGMENT_BUFFER *pasd_read_nv_buf,
	uint8_t *pdest_buffer);

static uint32_t asd_ctl_write_to_flash (struct asd_softc *asd, 
			uint8_t *src_img_addr, uint32_t segment_id, 
			uint32_t src_img_offset, uint32_t src_img_size);

/* protos for extern routines */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
#ifdef __x86_64__
extern int register_ioctl32_conversion(unsigned int cmd, 
	int (*handler)(unsigned int, unsigned int, unsigned long,
	struct file *));

extern int unregister_ioctl32_conversion(unsigned int cmd); 
#endif
#endif

extern int asd_hwi_search_nv_cookie(struct asd_softc *asd, 
			uint32_t *addr,
			struct asd_flash_dir_layout *pflash_dir_buf);

/* fops table */
static struct file_operations asd_ctl_fops = {
	.owner		= THIS_MODULE,
	.open		= asd_ctl_open,
	.release	= asd_ctl_close,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
	.ioctl		= asd_ctl_ioctl
#else
	.unlocked_ioctl = asd_unlocked_ctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = asd_compat_ioctl
#endif
#endif
};

/* macros */
#define asd_valloc_mem(size)		vmalloc(size)
#define asd_vfree_mem(ptr)		vfree(ptr)
#define ASD_CTL_TIMEOUT(pioctl_header) 	\
((pioctl_header->Timeout == 0? ASD_ADAPTEC_TIMEOUT: \
				pioctl_header->Timeout) * HZ) 

/* represents busy bit number */
#define ASD_CTL_INTERNAL_BUSY_BIT_NR 0

/* inline routines */
/*
 * Function:
 * 	asd_ctl_check_buf_len()
 *
 * Description:
 * 	This routine validates the buffer length specified in the CSMI
 * 	IOCTL header with the actual size of the buffer needed for the 
 * 	corresponding IOCTL.
 */
static inline int asd_ctl_check_buf_len(unsigned long arg, int buffer_length)
{
	struct IOCTL_HEADER ioctl_header;
	int err;
	
	if ((err = copy_from_user(&ioctl_header, (void *)arg, 
				 sizeof(ioctl_header)))) {
		return -EFAULT;
	}

	if (ioctl_header.Length < buffer_length) {
		ioctl_header.ReturnCode = ASD_SAS_STATUS_INVALID_PARAMETER;
		if ((err = copy_to_user((void *)arg, &ioctl_header, 
					sizeof(ioctl_header)))) {
			return -EFAULT;
		}
		return -EINVAL;
	}
	return 0;
}

static inline int asd_ctl_copy_to_user(void *dest, void *src, uint32_t length, 
			uint32_t status)
{
	struct IOCTL_HEADER *pioctl_header;
	int err;

	pioctl_header = (struct IOCTL_HEADER *)src;
	pioctl_header->ReturnCode = status;
	if ((err = copy_to_user(dest, src, length))) {
		return -EFAULT;
	}
	return 0;
}

static inline int asd_ctl_map_nv_segid_from_ext(uint32_t ext_segid, 
			uint32_t *asd_segid)
{
	switch (ext_segid) {
	case ASD_SAS_SEGMENT_ID_CTRL_A_USER_SETTINGS0:
		*asd_segid = NVRAM_CTRL_A_SETTING;
		break;
	case ASD_SAS_SEGMENT_ID_MANUFACTURING_SECTOR0: 
		*asd_segid = NVRAM_MANUF_TYPE;
		break;
	case ASD_SAS_SEGMENT_ID_FLASH_DIRECTORY0:/*TBD*/
	case ASD_SAS_SEGMENT_ID_FLASH0:
	case ASD_SAS_SEGMENT_ID_SEEPROM0:
	case ASD_SAS_SEGMENT_ID_COMPATIBILITY_SECTOR0:
		*asd_segid = NVRAM_NO_SEGMENT_ID;
	default:
		return -1;
	}

	return 0;
}

static inline int asd_ctl_map_seg_attr_to_ext(uint16_t asd_seg_attr, 
			uint16_t *ext_seg_attr)
{
	switch (asd_seg_attr) {
	case 0:
		*ext_seg_attr = ASD_SAS_SEGMENT_ATTRIBUTE_READONLY;
		break;	
	case 1:
		*ext_seg_attr = ASD_SAS_SEGMENT_ATTRIBUTE_ERASEWRITE;
		break;
	default:
		return -1;
	}
	return 0;
}

/* registration routines */
int asd_register_ioctl_dev(void)
{
	int err;
	
	err = 0;
	asd_ctl_major = 0;

	/* 
	 * Register the IOCTL control device and request 
	 * for a dynamic major number 
	 */
	if (!(asd_ctl_major = register_chrdev(0, ASD_CTL_DEV_NAME, 
						&asd_ctl_fops))) {
		return -ENODEV;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
#ifdef __x86_64__
	/* 
	 * This ioctl handler module is compatible between 64 and 32 bit
	 * environments, hence register the default handler as 32bit 
	 * ioctl conversion handler.
	 */
	err |= register_ioctl32_conversion(ASD_CC_SAS_GET_CNTLR_CONFIG, 
							(void *)sys_ioctl);
	err |= register_ioctl32_conversion(ASD_CC_SAS_GET_ADPT_CNTLR_CONFIG,
							(void *)sys_ioctl);
	err |= register_ioctl32_conversion(
				ASD_CC_SAS_GET_NV_SEGMENT_PROPERTIES,
							(void *)sys_ioctl);
	err |= register_ioctl32_conversion(ASD_CC_SAS_WRITE_NV_SEGMENT,
							(void *)sys_ioctl);
	err |= register_ioctl32_conversion(ASD_CC_SAS_READ_NV_SEGMENT,
							(void *)sys_ioctl);
#endif /* #ifdef __x86_64__ */
#endif
	return err;
}

int asd_unregister_ioctl_dev(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
#ifdef __x86_64__
	unregister_ioctl32_conversion(ASD_CC_SAS_GET_CNTLR_CONFIG);
	unregister_ioctl32_conversion(ASD_CC_SAS_GET_ADPT_CNTLR_CONFIG);
	unregister_ioctl32_conversion(ASD_CC_SAS_GET_NV_SEGMENT_PROPERTIES);
	unregister_ioctl32_conversion(ASD_CC_SAS_WRITE_NV_SEGMENT);
	unregister_ioctl32_conversion(ASD_CC_SAS_READ_NV_SEGMENT);
#endif /* #ifdef __x86_64__ */
#endif

	return unregister_chrdev(asd_ctl_major, ASD_CTL_DEV_NAME);
}

/* init routine */
int asd_ctl_init_internal_data(struct asd_softc *asd)
{
	struct asd_ctl_mgmt *pasd_ctl_mgmt;
	
	pasd_ctl_mgmt = &asd->asd_ctl_internal.mgmt;
	
	/* Initialize waitq */
	init_waitqueue_head(&pasd_ctl_mgmt->waitq);
	
	/* Initialize busy flags */
	pasd_ctl_mgmt->busy = 0;
	
	/* Initialize semaphore */
	init_MUTEX(&pasd_ctl_mgmt->sem);
	
	/* Initialize timers */
	init_timer(&pasd_ctl_mgmt->timer);

	/* Initialize err flag */
	pasd_ctl_mgmt->err = SCB_EH_FAILED;
	
	return 0;
}

/* entry points */
static int asd_ctl_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)	
	MOD_INC_USE_COUNT;
#endif	
	return 0;
}

static int asd_ctl_close(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)	
	MOD_DEC_USE_COUNT;
#endif	
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
static int asd_ctl_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
#else
static long asd_unlocked_ctl_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
#endif
{
	struct IOCTL_HEADER ioctl_header;
	int asd_user_buf_len;
	int hba, max_hba;
	int err;

	if (!capable(CAP_SYS_ADMIN)) {
		return -EACCES;
	}
	
	asd_user_buf_len = sizeof(ioctl_header);
	if ((err = copy_from_user(&ioctl_header, (void *)arg, 
				asd_user_buf_len))) {
		return -EFAULT;
	}

	hba = ioctl_header.IOControllerNumber;

	max_hba = asd_get_number_of_hbas_present();

	if (hba < 0 || hba >= max_hba) {
		asd_ctl_copy_to_user((void *)arg, 
				     &ioctl_header, 
				     asd_user_buf_len,
				     ASD_SAS_STATUS_INVALID_PARAMETER);
		return -ENODEV;
	}

	switch (cmd) {
	case ASD_CC_SAS_GET_CNTLR_CONFIG:
		err = asd_ctl_get_cntlr_config(arg);
		break;
	case ASD_CC_SAS_GET_ADPT_CNTLR_CONFIG:
		err = asd_ctl_sas_get_adpt_cntlr_conf(arg);
		break;
	case ASD_CC_SAS_GET_NV_SEGMENT_PROPERTIES:
		err = asd_ctl_sas_get_nv_seg_prop(arg);
		break;
	case ASD_CC_SAS_WRITE_NV_SEGMENT:
		err = asd_ctl_sas_write_nv_seg(arg);
		break;
	case ASD_CC_SAS_READ_NV_SEGMENT:
		err = asd_ctl_sas_read_nv_seg(arg);
		break;
	/* Unsupported/Unknown IOCTLs */
	default:
		asd_ctl_copy_to_user((void *)arg, 
				     &ioctl_header, 
				     asd_user_buf_len,
				     ASD_SAS_STATUS_BAD_CNTL_CODE);
		return -EINVAL;
	}

	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
#ifdef CONFIG_COMPAT
static long asd_compat_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	return asd_unlocked_ctl_ioctl(file, cmd, arg);
}
#endif
#endif


/* ioctl handler routines */
static int asd_ctl_get_cntlr_config(unsigned long arg)
{
	struct ASD_SAS_CNTLR_CONFIG_BUFFER asd_cntlr_conf_buf;
	struct ASD_SAS_CNTLR_CONFIG *pasd_sas_cntlr_config;
	union ASD_SAS_IO_BUS_ADDRESS *pasd_sas_io_bus_address;
	struct ASD_SAS_PCI_BUS_ADDRESS *pasd_sas_pci_bus_address;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_softc *asd;
	int asd_user_buf_len;
	int retcode;
	int err;
	
	asd_user_buf_len = sizeof(asd_cntlr_conf_buf);
	if ((err = asd_ctl_check_buf_len(arg, asd_user_buf_len))) {
		return -EINVAL;
	}
	
	if ((err = copy_from_user(&asd_cntlr_conf_buf, (void *)arg, 
				  asd_user_buf_len))) {
		return -EFAULT;
	}
	
	pioctl_header = &asd_cntlr_conf_buf.IoctlHeader;
	
	asd = asd_get_softc_by_hba_index(pioctl_header->IOControllerNumber);
	if (asd == NULL) {
		/* Requested controller not found */
		retcode = ASD_SAS_STATUS_INVALID_PARAMETER;
		err = -ENODEV;
		goto done;
	}
		
	retcode = ASD_SAS_STATUS_SUCCESS;
	err = 0;
	pasd_sas_cntlr_config = &asd_cntlr_conf_buf.Configuration;
	pasd_sas_io_bus_address = &pasd_sas_cntlr_config->BusAddress;
	pasd_sas_pci_bus_address = &pasd_sas_io_bus_address->PciAddress;

	pasd_sas_cntlr_config->uBaseIoAddress = 
				asd_pcic_read_dword(asd, PCI_BASE_ADDRESS_0);
	pasd_sas_cntlr_config->BaseMemoryAddress.uLowPart =
				asd_pcic_read_dword(asd, PCI_BASE_ADDRESS_2);
	pasd_sas_cntlr_config->BaseMemoryAddress.uHighPart =
				asd_pcic_read_dword(asd, PCI_BASE_ADDRESS_3);

	pasd_sas_cntlr_config->uBoardID =
				(asd_pci_dev(asd)->device << 16) |
				asd_pci_dev(asd)->vendor;

	pasd_sas_cntlr_config->usSlotNumber = 
				PCI_SLOT(asd_pci_dev(asd)->devfn);
	pasd_sas_cntlr_config->bControllerClass = ASD_SAS_CNTLR_CLASS_HBA;
	pasd_sas_cntlr_config->bIoBusType = ASD_SAS_BUS_TYPE_PCI;
	pasd_sas_pci_bus_address->bBusNumber = asd_pci_dev(asd)->bus->number;
	pasd_sas_pci_bus_address->bDeviceNumber =
				PCI_SLOT(asd_pci_dev(asd)->devfn);
	pasd_sas_pci_bus_address->bFunctionNumber =
				PCI_FUNC(asd_pci_dev(asd)->devfn);
	
	sprintf((char *)pasd_sas_cntlr_config->szSerialNumber,"%01x%06x%08x%x", 
			ASD_BSAR_GET_NAA(asd->hw_profile.wwn), 
			ASD_BSAR_GET_IEEE_ID(asd->hw_profile.wwn),
			ASD_BSAR_GET_SN(asd->hw_profile.wwn),
			ASD_BSAR_GET_LSB(asd->hw_profile.wwn));

	pasd_sas_cntlr_config->usMajorRevision = 0;
	pasd_sas_cntlr_config->usMinorRevision = 0;
	pasd_sas_cntlr_config->usBuildRevision = 0;
	pasd_sas_cntlr_config->usReleaseRevision = 0;
	pasd_sas_cntlr_config->usBIOSMajorRevision = 
				asd->hw_profile.bios_maj_ver; 
	pasd_sas_cntlr_config->usBIOSMinorRevision = 
				asd->hw_profile.bios_min_ver; 
	pasd_sas_cntlr_config->usBIOSBuildRevision = 
				asd->hw_profile.bios_bld_num & 0xffff; 
	pasd_sas_cntlr_config->usBIOSReleaseRevision = 
				asd->hw_profile.bios_bld_num >> 16; 
	
	pasd_sas_cntlr_config->uControllerFlags = ASD_SAS_CNTLR_SAS_HBA;
					   //|ASD_SAS_CNTLR_SATA_HBA;//TBD
	pasd_sas_cntlr_config->usRromMajorRevision = 0;
	pasd_sas_cntlr_config->usRromMinorRevision = 0;
	pasd_sas_cntlr_config->usRromBuildRevision = 0;
	pasd_sas_cntlr_config->usRromReleaseRevision = 0;
	pasd_sas_cntlr_config->usRromBIOSMajorRevision = 0;
	pasd_sas_cntlr_config->usRromBIOSMinorRevision = 0;
	pasd_sas_cntlr_config->usRromBIOSBuildRevision = 0;
	pasd_sas_cntlr_config->usRromBIOSReleaseRevision = 0;

done:
	/* return status */	
	asd_ctl_copy_to_user((void *)arg, &asd_cntlr_conf_buf, 
			     asd_user_buf_len, retcode);
	return err;
}	

/* Adaptec extension IOCTL handler routines */
static int asd_ctl_sas_get_adpt_cntlr_conf(unsigned long arg)
{
	struct ASD_SAS_GET_ADPT_CNTLR_CONFIG_BUFFER asd_adpt_cntlr_conf_buf;
	struct ASD_SAS_GET_ADPT_CNTLR_CONFIG *pasd_sas_adpt_cntlr_config;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_softc *asd;
	u_long flags;
	int asd_user_buf_len;
	int retcode;
	int err;
	
	asd_user_buf_len = sizeof(asd_adpt_cntlr_conf_buf);
	if ((err = asd_ctl_check_buf_len(arg, asd_user_buf_len))) {
		return -EINVAL;
	}
	
	if ((err = copy_from_user(&asd_adpt_cntlr_conf_buf, 
				  (void *)arg, 
				  asd_user_buf_len))) {
		return -EFAULT;
	}
	
	pioctl_header = &asd_adpt_cntlr_conf_buf.IoctlHeader;
	
	asd = asd_get_softc_by_hba_index(pioctl_header->IOControllerNumber);
	if (asd == NULL) {
		/* Requested controller not found */
		retcode = ASD_SAS_STATUS_INVALID_PARAMETER;
		err = -ENODEV;
		goto done;
	}

	pasd_sas_adpt_cntlr_config = &asd_adpt_cntlr_conf_buf.Configuration;

	asd_lock(asd, &flags);
	pasd_sas_adpt_cntlr_config->usPCIVendorID = asd_pci_dev(asd)->vendor;
	pasd_sas_adpt_cntlr_config->usPCIDeviceID = asd_pci_dev(asd)->device;
	pasd_sas_adpt_cntlr_config->usPCISubsystemVendorID =  
					asd_pci_dev(asd)->subsystem_vendor;
	pasd_sas_adpt_cntlr_config->usPCISubsystemID = 
					asd_pci_dev(asd)->subsystem_device;
		
	if (asd->hw_profile.flash_present) {
		pasd_sas_adpt_cntlr_config->usFlashManufacturerID =
					asd->hw_profile.flash_manuf_id;
		pasd_sas_adpt_cntlr_config->usFlashDeviceID = 
					asd->hw_profile.flash_dev_id;
		if (asd->hw_profile.flash_wr_prot) {
			pasd_sas_adpt_cntlr_config->uControllerFlags 
				= ASD_ADPT_CNTLR_FLASH_READONLY;
		}
		retcode = ASD_SAS_STATUS_SUCCESS;
	} else {
		pasd_sas_adpt_cntlr_config->usFlashManufacturerID 
				= ASD_SAS_APDT_INVALID_FLASH_MANUFACTURER_ID; 
		pasd_sas_adpt_cntlr_config->usFlashDeviceID 
				= ASD_SAS_ADPT_INVALID_FLASH_DEVICE_ID; 
		pasd_sas_adpt_cntlr_config->uControllerFlags 
				= ASD_ADPT_CNTLR_FLASH_NOT_PRESENT; 
		retcode = ASD_SAS_STATUS_FAILED;
	}
	asd_unlock(asd, &flags);
done:
	/* return status */	
	asd_ctl_copy_to_user((void *)arg, &asd_adpt_cntlr_conf_buf, 
			     asd_user_buf_len, retcode);
	return err;
}

static int asd_ctl_sas_get_nv_seg_prop(unsigned long arg)
{
	struct ASD_SAS_NV_SEGMENT_PROPERTIES_BUFFER asd_nvseg_prop_buf;
	struct ASD_SAS_NV_SEGMENT_PROPERTIES *pasd_nvseg_prop;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_softc *asd;
	u_long flags;
	uint32_t seg_offset, nv_offset;
	uint32_t pad_size, image_size, attr, bytes_read;
	int asd_user_buf_len;
	int err;
	int retcode; 
	
	asd_user_buf_len = sizeof(asd_nvseg_prop_buf);
	if ((err = asd_ctl_check_buf_len(arg, asd_user_buf_len))) {
		return -EINVAL;
	}
	
	if ((err = copy_from_user(&asd_nvseg_prop_buf, 
				  (void *)arg, 
				  asd_user_buf_len))) {
		return -EFAULT;
	}
	
	pioctl_header = &asd_nvseg_prop_buf.IoctlHeader;
	
	asd = asd_get_softc_by_hba_index(pioctl_header->IOControllerNumber);
	if (asd == NULL) {
		/* Requested controller not found */
		retcode = ASD_SAS_STATUS_INVALID_PARAMETER; 
		err = -ENODEV;
		goto done;
	}
	
	err = 0;
	retcode = ASD_SAS_STATUS_SUCCESS;
	pasd_nvseg_prop = &asd_nvseg_prop_buf.Information;
	asd_lock(asd, &flags);
	
	switch (pasd_nvseg_prop->uSegmentID) {
	case ASD_SAS_SEGMENT_ID_MANUFACTURING_SECTOR0:
	{
		struct asd_manuf_base_seg_layout manuf_layout;
		
		if ((err = asd_hwi_search_nv_segment(asd, NVRAM_MANUF_TYPE,
				&nv_offset, &pad_size, &image_size, 
				&attr)) != 0) {
			err = -1;
			break;
		}
		asd_ctl_map_seg_attr_to_ext(attr, 
			&pasd_nvseg_prop->usSegmentAttributes); 
		
		seg_offset = 0;
		memset(&manuf_layout, 0x0, sizeof(manuf_layout));
		
		/* Read Manufacturing base segment */
		if (asd_hwi_read_nv_segment(asd, NVRAM_MANUF_TYPE, 
				&manuf_layout, seg_offset, 
				sizeof(manuf_layout), &bytes_read) != 0) {
			err = -1;
			break;
		}
		image_size = manuf_layout.sector_size;
		pad_size = image_size;
		break;
	}
	case ASD_SAS_SEGMENT_ID_CTRL_A_USER_SETTINGS0:
	{
		struct asd_pci_layout pci_layout;

		if ((err = asd_hwi_search_nv_segment(asd, NVRAM_CTRL_A_SETTING,
				&nv_offset, &pad_size, &image_size, 
				&attr)) != 0) {
			err = -1;
			break;
		}
		asd_ctl_map_seg_attr_to_ext(attr, 
			&pasd_nvseg_prop->usSegmentAttributes); 
		
		seg_offset = 0;
		memset(&pci_layout, 0x0, sizeof(pci_layout));
		
		/* Read Ctrl-A first segment */
		if (asd_hwi_read_nv_segment(asd, NVRAM_CTRL_A_SETTING, 
				&pci_layout, seg_offset, sizeof(pci_layout),
				&bytes_read) != 0) {
			err = -1;
			break;
		}
		image_size = pci_layout.image_size;
		pad_size = image_size;
		break;
	}
	case ASD_SAS_SEGMENT_ID_FLASH_DIRECTORY0:
	{
		struct asd_flash_dir_layout flash_dir;
		uint32_t nv_addr, active_entries;
		int i;
		if (asd_hwi_search_nv_cookie(asd, &nv_addr, &flash_dir) != 0) {
			err = -1;
			break;
		}
		seg_offset = 0;
		active_entries = 0;
		for (i = 0; i < NVRAM_MAX_ENTRIES; i++) {
			if (flash_dir.ae_mask & (1 << i)) {
				active_entries++;
			}
		}
		image_size = sizeof(flash_dir) +
			     (active_entries * 
			     sizeof(struct asd_fd_entry_layout));
		pad_size = sizeof(flash_dir) + 
			   (NVRAM_MAX_ENTRIES * 
			   sizeof(struct asd_fd_entry_layout));
		break;
	}
	case ASD_SAS_SEGMENT_ID_FLASH0:
	case ASD_SAS_SEGMENT_ID_SEEPROM0:
	case ASD_SAS_SEGMENT_ID_COMPATIBILITY_SECTOR0:
	{
		pad_size = ASD_SAS_SEGMENT_SIZE_INVALID;
		image_size = ASD_SAS_SEGMENT_IMAGE_SIZE_INVALID;
		pasd_nvseg_prop->usSegmentAttributes = 0;
		break;
	}
	default:
		err = -1;
		break;
	}
	asd_unlock(asd, &flags);

	if (err) {
		pasd_nvseg_prop->uStatus = ASD_SAS_NV_FAILURE;
		retcode = ASD_SAS_STATUS_FAILED;
	} else {
		pasd_nvseg_prop->uSegmentSize = pad_size;
		pasd_nvseg_prop->uSegmentImageSize = image_size;
		pasd_nvseg_prop->uStatus = ASD_SAS_NV_SUCCESS;
		retcode = ASD_SAS_STATUS_SUCCESS;
	}
done:
	/* return status */	
	asd_ctl_copy_to_user((void *)arg, &asd_nvseg_prop_buf, 
			     asd_user_buf_len, retcode);
	return err;
}

static int asd_ctl_sas_write_nv_seg(unsigned long arg)
{
	struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER *pasd_write_nv_buf;
	struct ASD_SAS_WRITE_NV_SEGMENT *pasd_write_nv_seg;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_ctl_mgmt *pasd_ctl_mgmt;
	struct asd_softc *asd;
	uint8_t *psource_buffer;
	int asd_user_buf_len;
	int retcode;
	int err;
	
	/* 
	 * Leave the data buffer at the end of the struct as the
	 * data buffer will be allocated separately
	 */
	asd_user_buf_len = offsetof(struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER, 
				     bSourceBuffer) ;
	
	if ((pasd_write_nv_buf = asd_alloc_mem(asd_user_buf_len,
					       GFP_ATOMIC)) == NULL) {
		return -ENOMEM;
	}

	if ((err = copy_from_user(pasd_write_nv_buf, (void *)arg, 
				  asd_user_buf_len))) {
		asd_free_mem(pasd_write_nv_buf);
		return -EFAULT;
	}
	
	pioctl_header = &pasd_write_nv_buf->IoctlHeader;
	pasd_write_nv_seg = 
		&pasd_write_nv_buf->Information;

	if ((err = asd_ctl_check_buf_len(arg, 
			  asd_user_buf_len
			  + pasd_write_nv_seg->uBufferLength))) {
		asd_free_mem(pasd_write_nv_buf);
		return -EINVAL;
	}
	
	asd = asd_get_softc_by_hba_index(pioctl_header->IOControllerNumber);
	
	if (asd == NULL) {
		/* Requested controller not found */
		retcode = ASD_SAS_STATUS_INVALID_PARAMETER;
		err = -ENODEV;
		goto done;
	}

	if (!asd->hw_profile.flash_present) {
		retcode = ASD_SAS_STATUS_FAILED;
		err = -EINVAL;
		goto done;
	}
	
	err = 0;
	retcode = ASD_SAS_STATUS_SUCCESS; 
	psource_buffer = NULL;	
	pasd_ctl_mgmt = &asd->asd_ctl_internal.mgmt;

	if ((psource_buffer = (uint8_t *)asd_valloc_mem(
				pasd_write_nv_seg->uBufferLength)) == NULL) {
		retcode = ASD_SAS_STATUS_FAILED;
		err = -ENOMEM;
		goto done;
	}
	memset(psource_buffer, 0, pasd_write_nv_seg->uBufferLength);

	if ((err = copy_from_user(psource_buffer,
			 (void *)arg + asd_user_buf_len,
			 pasd_write_nv_seg->uBufferLength))) {
		asd_vfree_mem(psource_buffer);
		retcode = ASD_SAS_STATUS_FAILED;
		err = -EFAULT;
		goto done;
	}
	
	down_interruptible(&pasd_ctl_mgmt->sem);
	err = asd_ctl_write_to_nvram(asd, pasd_write_nv_buf, 
				     psource_buffer);
	up(&pasd_ctl_mgmt->sem);

	asd_vfree_mem(psource_buffer);

	retcode = err ? ASD_SAS_STATUS_FAILED : ASD_SAS_STATUS_SUCCESS; 
done:
	asd_ctl_copy_to_user((void *)arg, pasd_write_nv_buf, 
			     asd_user_buf_len, retcode);
	asd_free_mem(pasd_write_nv_buf);
	return err;
}

static int asd_ctl_sas_read_nv_seg(unsigned long arg)
{
	struct ASD_SAS_READ_NV_SEGMENT_BUFFER *pasd_read_nv_buf;
	struct ASD_SAS_READ_NV_SEGMENT *pasd_read_nv_seg;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_ctl_mgmt *pasd_ctl_mgmt;
	struct asd_softc *asd;
	uint8_t *pdest_buffer;
	int asd_user_buf_len;
	int retcode;
	int err;
	
	/* 
	 * Leave the data buffer at the end of the struct as the
	 * data buffer will be allocated separately
	 */
	asd_user_buf_len = offsetof(struct ASD_SAS_READ_NV_SEGMENT_BUFFER, 
				     bDestinationBuffer) ;
	
	if ((pasd_read_nv_buf = asd_alloc_mem(asd_user_buf_len,
					      GFP_ATOMIC)) == NULL) {
		return -ENOMEM;
	}

	if ((err = copy_from_user(pasd_read_nv_buf, (void *)arg, 
				  asd_user_buf_len)) != 0) {
		asd_free_mem(pasd_read_nv_buf);
		return -EFAULT;
	}
	
	pioctl_header = &pasd_read_nv_buf->IoctlHeader;
	pasd_read_nv_seg = &pasd_read_nv_buf->Information;

	if ((err = asd_ctl_check_buf_len(arg, 
			  asd_user_buf_len
			  + pasd_read_nv_seg->uBytesToRead))) {
		asd_free_mem(pasd_read_nv_buf);
		return -EINVAL;
	}
	
	asd = asd_get_softc_by_hba_index(pioctl_header->IOControllerNumber);
	
	if (asd == NULL) {
		/* Requested controller not found */
		retcode = ASD_SAS_STATUS_INVALID_PARAMETER;
		err = -ENODEV;
		goto done;
	}
	
	if (!asd->hw_profile.flash_present) {
		retcode = ASD_SAS_STATUS_FAILED;
		err = -EINVAL;
		goto done;
	}
	
	err = 0;
	retcode = ASD_SAS_STATUS_SUCCESS; 
	pdest_buffer = NULL;
	pasd_ctl_mgmt = &asd->asd_ctl_internal.mgmt;
	
	if ((pdest_buffer = (uint8_t *)asd_valloc_mem(
				pasd_read_nv_seg->uBytesToRead)) == NULL) {
		retcode = ASD_SAS_STATUS_FAILED;
		err = -ENOMEM;
		goto done;
	}
	memset(pdest_buffer, 0, pasd_read_nv_seg->uBytesToRead);
	
	down_interruptible(&pasd_ctl_mgmt->sem);
	err = asd_ctl_read_from_nvram(asd, pasd_read_nv_buf, 
				      pdest_buffer);
	up(&pasd_ctl_mgmt->sem);

	if (err) {
		retcode = ASD_SAS_STATUS_FAILED; 
	} else {
		retcode = ASD_SAS_STATUS_SUCCESS; 
		if ((err = copy_to_user((void *)arg + asd_user_buf_len,
					pdest_buffer,
					pasd_read_nv_seg->uBytesRead))) {
			retcode = ASD_SAS_STATUS_FAILED; 
		}
	}

	asd_vfree_mem(pdest_buffer);
done:
	asd_ctl_copy_to_user((void *)arg, pasd_read_nv_buf, 
			     asd_user_buf_len, retcode);
	asd_free_mem(pasd_read_nv_buf);
	return err;
}

/* helper routines */
static int asd_ctl_write_to_nvram(struct asd_softc *asd,
	struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER *pasd_write_nv_buf,
	uint8_t *psource_buffer)
{
	struct ASD_SAS_WRITE_NV_SEGMENT *pasd_write_nv_seg;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_ctl_mgmt *pasd_ctl_mgmt;
	uint32_t nv_segment_id=0, nv_offset;
	uint32_t pad_size, image_size, attr;
	u_long flags;
	int retcode;
	int err;
	
	pioctl_header = &pasd_write_nv_buf->IoctlHeader;
	pasd_write_nv_seg = &pasd_write_nv_buf->Information;
	pasd_ctl_mgmt = &asd->asd_ctl_internal.mgmt;
	
	if (test_and_set_bit(ASD_CTL_INTERNAL_BUSY_BIT_NR, 
			     &pasd_ctl_mgmt->busy)) { 
		return -EBUSY;
	}
	asd_lock(asd, &flags);
	err = 0;
	retcode = ASD_SAS_STATUS_SUCCESS;
	switch (pasd_write_nv_seg->uSegmentID) {
	case ASD_SAS_SEGMENT_ID_MANUFACTURING_SECTOR0:
	{
		asd_ctl_map_nv_segid_from_ext(pasd_write_nv_seg->uSegmentID,
					      &nv_segment_id);
		if ((err = asd_hwi_search_nv_segment(asd, nv_segment_id, 
				&nv_offset, &pad_size, &image_size, 
				&attr)) != 0) {
			err = -1;
			pasd_write_nv_seg->uStatus = ASD_SAS_NV_FAILURE;
			break;
		}
		err = asd_ctl_write_to_flash(asd, psource_buffer, 
			nv_segment_id,
			pasd_write_nv_seg->uDestinationOffset, 
			pasd_write_nv_seg->uBufferLength);
		break;
	}
	case ASD_SAS_SEGMENT_ID_FLASH0:
	{
		err = asd_ctl_write_to_flash(asd, psource_buffer, 
			NVRAM_NO_SEGMENT_ID,
			pasd_write_nv_seg->uDestinationOffset, 
			pasd_write_nv_seg->uBufferLength);
		break;
	}
	case ASD_SAS_SEGMENT_ID_SEEPROM0:
	case ASD_SAS_SEGMENT_ID_FLASH_DIRECTORY0:
	case ASD_SAS_SEGMENT_ID_CTRL_A_USER_SETTINGS0:
	case ASD_SAS_SEGMENT_ID_COMPATIBILITY_SECTOR0:
	default:
		pasd_write_nv_seg->uStatus = ASD_SAS_NV_FAILURE;
		err = -EINVAL;
		break;
	}

	asd_unlock(asd, &flags);

	/* return */
	clear_bit(ASD_CTL_INTERNAL_BUSY_BIT_NR, &pasd_ctl_mgmt->busy);
	return err;
}

static int asd_ctl_read_from_nvram(struct asd_softc *asd,
	struct ASD_SAS_READ_NV_SEGMENT_BUFFER *pasd_read_nv_buf,
	uint8_t *pdest_buffer)
{
	struct ASD_SAS_READ_NV_SEGMENT *pasd_read_nv_seg;
	struct IOCTL_HEADER *pioctl_header;
	struct asd_ctl_mgmt *pasd_ctl_mgmt;
	uint32_t nv_segment_id=0;
	u_long flags;
	int retcode;
	int err;
	
	pioctl_header = &pasd_read_nv_buf->IoctlHeader;
	pasd_read_nv_seg = &pasd_read_nv_buf->Information;
	pasd_ctl_mgmt = &asd->asd_ctl_internal.mgmt;
	
	if (test_and_set_bit(ASD_CTL_INTERNAL_BUSY_BIT_NR, 
			     &pasd_ctl_mgmt->busy)) { 
		return -EBUSY;
	}
	err = 0;
	retcode = ASD_SAS_STATUS_SUCCESS;
	
	asd_lock(asd, &flags);
	switch (pasd_read_nv_seg->uSegmentID) {
	case ASD_SAS_SEGMENT_ID_CTRL_A_USER_SETTINGS0:
	case ASD_SAS_SEGMENT_ID_MANUFACTURING_SECTOR0:
	{
		asd_ctl_map_nv_segid_from_ext(
			pasd_read_nv_seg->uSegmentID,
			&nv_segment_id);

		if (asd_hwi_read_nv_segment(asd, 
				nv_segment_id,
				(void *)pdest_buffer,
				pasd_read_nv_seg->uSourceOffset, 
				pasd_read_nv_seg->uBytesToRead, 
				&pasd_read_nv_seg->uBytesRead
				) != 0) {
			pasd_read_nv_seg->uStatus = ASD_SAS_NV_FAILURE;
			err = -EINVAL;
			break;
		}
		pasd_read_nv_seg->uStatus = ASD_SAS_NV_SUCCESS;
		break;
	}
	case ASD_SAS_SEGMENT_ID_FLASH_DIRECTORY0:
	{
		struct asd_flash_dir_layout flash_dir;
		uint32_t nv_addr;

		if (asd_hwi_search_nv_cookie(asd, &nv_addr, &flash_dir) != 0) {
			err = -1;
			break;
		}
		nv_addr += /*NVRAM_FIRST_DIR_ENTRY +*/
			pasd_read_nv_seg->uSourceOffset;

		if (asd_hwi_read_nv_segment(asd, NVRAM_NO_SEGMENT_ID,
				(void *)pdest_buffer, nv_addr,
				pasd_read_nv_seg->uBytesToRead, 
				&pasd_read_nv_seg->uBytesRead
				) != 0) {
			err = -1;
			break;
		}
		break;
	}	
	case ASD_SAS_SEGMENT_ID_FLASH0:
	{
		if (asd_hwi_read_nv_segment(asd, 
				NVRAM_NO_SEGMENT_ID,
				(void *)pdest_buffer,
				pasd_read_nv_seg->uSourceOffset, 
				pasd_read_nv_seg->uBytesToRead, 
				&pasd_read_nv_seg->uBytesRead
				) != 0) {
			pasd_read_nv_seg->uStatus = ASD_SAS_NV_FAILURE;
			err = -EINVAL;
			break;
		}
		pasd_read_nv_seg->uStatus = ASD_SAS_NV_SUCCESS;
		break;
	}
	case ASD_SAS_SEGMENT_ID_SEEPROM0:
	case ASD_SAS_SEGMENT_ID_COMPATIBILITY_SECTOR0:/*TBD*/
	default:
		pasd_read_nv_seg->uStatus = ASD_SAS_NV_FAILURE;
		err = -EINVAL;
		break;
	}
	asd_unlock(asd, &flags);

	/* return */
	clear_bit(ASD_CTL_INTERNAL_BUSY_BIT_NR, &pasd_ctl_mgmt->busy);
	return err;
}

static uint32_t asd_ctl_write_to_flash(struct asd_softc *asd, 
			uint8_t *src_img_addr, uint32_t segment_id, 
			uint32_t dest_nv_offset, uint32_t src_img_size)
{
	uint32_t flash_addr;

	flash_addr = 0;

	/* Need to unlock MBAR key register */
	flash_addr = asd_pcic_read_dword(asd, PCIC_MBAR_KEY);
	if (flash_addr != 0) {
		/* currently locked, need to unlock */
		asd_pcic_write_dword(asd, PCIC_MBAR_KEY, flash_addr);
	}
	if (asd_hwi_write_nv_segment(asd, 
			(void *)src_img_addr, segment_id, 
			dest_nv_offset, src_img_size) != 0) {
		return -EINVAL;
	}
	return 0;
}
