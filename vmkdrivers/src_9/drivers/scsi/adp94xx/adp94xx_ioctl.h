/*
 * Adaptec ADP94xx SAS HBA driver for Linux - IOCTL data structures 
 *
 * Written by : Naveen Chandrasekaran <naveen_chandrasekaran@adaptec.com>
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
 * $Id: //depot/razor/linux/src/adp94xx_ioctl.h#7 $
 * 
 */	

#ifndef _ASD_IOCTL_H_
#define _ASD_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* Data Direction */
#define ASD_SAS_DATA_READ			0
#define ASD_SAS_DATA_WRITE			1

/* Status Codes */
#define ASD_SAS_STATUS_SUCCESS			0
#define ASD_SAS_STATUS_FAILED			1
#define ASD_SAS_STATUS_BAD_CNTL_CODE		2
#define ASD_SAS_STATUS_INVALID_PARAMETER	3

/* I/O Bus Types
 * ISA and EISA bus types are not supported: (bIoBusType)
 */
#define ASD_SAS_BUS_TYPE_PCI 			3
#define ASD_SAS_BUS_TYPE_PCMCIA 		4

/* Controller Class: (bControllerClass) */
#define ASD_SAS_CNTLR_CLASS_HBA 		5

/* Controller Flag bits: (uControllerFlags) */
#define ASD_SAS_CNTLR_SAS_HBA 			0x00000001
#define ASD_SAS_CNTLR_SAS_RAID			0x00000002
#define ASD_SAS_CNTLR_SATA_HBA			0x00000004
#define ASD_SAS_CNTLR_SATA_RAID			0x00000008

/* for firmware download */
#define ASD_SAS_CNTLR_FWD_SUPPORT		0x00010000
#define ASD_SAS_CNTLR_FWD_ONLINE 		0x00020000
#define ASD_SAS_CNTLR_FWD_SRESET 		0x00040000
#define ASD_SAS_CNTLR_FWD_HRESET 		0x00080000
#define ASD_SAS_CNTLR_FWD_RROM			0x00100000

/* IOCTL Header */
struct IOCTL_HEADER {
	__u32 IOControllerNumber;
	__u32 Length;
	__u32 ReturnCode;
	__u32 Timeout;
	__u16 Direction;
}__attribute__ ((packed));

/*************************************************************************/
/* OS INDEPENDENT CODE                                                   */
/*************************************************************************/

/* * * * * * * * * Adaptec Vendor Unique Class IOCTL Constants * * * * * * * */

/* Timeout value default of 60 seconds: IoctlHeader.Timeout */
#define ASD_ADAPTEC_TIMEOUT      60

/* Controller flags: uControllerFlags */
#define ASD_ADPT_CNTLR_FLASH_NOT_PRESENT          0x00000001
#define ASD_ADPT_CNTLR_FLASH_READONLY             0x00000002

/* Invalid value for usFlashManufacturerID */
#define ASD_SAS_APDT_INVALID_FLASH_MANUFACTURER_ID   0xFFFF

/* Invalid value for usFlashDeviceID */
#define ASD_SAS_ADPT_INVALID_FLASH_DEVICE_ID         0xFFFF

/* Segment identifiers: uSegmentID */
#define ASD_SAS_SEGMENT_ID_FLASH_DIRECTORY0       0x80000000
#define ASD_SAS_SEGMENT_ID_CTRL_A_USER_SETTINGS0  0x800000E0
#define ASD_SAS_SEGMENT_ID_MANUFACTURING_SECTOR0  0x80000120
#define ASD_SAS_SEGMENT_ID_COMPATIBILITY_SECTOR0  0x80000140

#define ASD_SAS_SEGMENT_ID_FLASH0                 0x90000000
#define ASD_SAS_SEGMENT_ID_SEEPROM0               0xA0000000

/* NVRAM status: (uStatus) */
#define ASD_SAS_NV_SUCCESS                  0
#define ASD_SAS_NV_FAILURE                  1
#define ASD_SAS_NV_INVALID_SEGMENT_ID       3
#define ASD_SAS_NV_INVALID_REQUEST          4
#define ASD_SAS_NV_WRITE_NOT_SUPPORTED      5
#define ASD_SAS_NV_NO_NVRAM                 6

/* NVRAM segment attributes: (usSegmentAttributes) */
#define ASD_SAS_SEGMENT_ATTRIBUTE_ERASEWRITE   0x0001
#define ASD_SAS_SEGMENT_ATTRIBUTE_READONLY     0x0002

/* Invalid value for uSegmentSize */
#define ASD_SAS_SEGMENT_SIZE_INVALID        0xFFFFFFFF

/* Invalid value for uSegmentImageSize */
#define ASD_SAS_SEGMENT_IMAGE_SIZE_INVALID  0xFFFFFFFF

#define SLOT_NUMBER_UNKNOWN	0xFFFF
/*************************************************************************/
/* DATA STRUCTURES                                                       */
/*************************************************************************/

/* * * * * * * * * * Adaptec Vendor Unique Class Structures * * * * * * * * * */

/* ASD_CC_SAS_CNTLR_CONFIGURATION */
struct ASD_SAS_PCI_BUS_ADDRESS {
	__u8 bBusNumber;
	__u8 bDeviceNumber;
	__u8 bFunctionNumber;
	__u8 bReserved;
}__attribute__ ((packed));

union ASD_SAS_IO_BUS_ADDRESS {
	struct ASD_SAS_PCI_BUS_ADDRESS PciAddress;
	__u8 bReserved[32];
}__attribute__ ((packed));

struct ASD_SAS_CNTLR_CONFIG {
	__u32 uBaseIoAddress;
	struct {
		__u32 uLowPart;
		__u32 uHighPart;
	} BaseMemoryAddress;
	__u32 uBoardID;
	__u16 usSlotNumber;
	__u8 bControllerClass;
	__u8 bIoBusType;
	union ASD_SAS_IO_BUS_ADDRESS BusAddress;
	__u8 szSerialNumber[81];
	__u16 usMajorRevision;
	__u16 usMinorRevision;
	__u16 usBuildRevision;
	__u16 usReleaseRevision;
	__u16 usBIOSMajorRevision;
	__u16 usBIOSMinorRevision;
	__u16 usBIOSBuildRevision;
	__u16 usBIOSReleaseRevision;
	__u32 uControllerFlags;
	__u16 usRromMajorRevision;
	__u16 usRromMinorRevision;
	__u16 usRromBuildRevision;
	__u16 usRromReleaseRevision;
	__u16 usRromBIOSMajorRevision;
	__u16 usRromBIOSMinorRevision;
	__u16 usRromBIOSBuildRevision;
	__u16 usRromBIOSReleaseRevision;
	__u8 bReserved[7];
}__attribute__ ((packed));

struct ASD_SAS_CNTLR_CONFIG_BUFFER {
	struct IOCTL_HEADER IoctlHeader;
	struct ASD_SAS_CNTLR_CONFIG Configuration;
}__attribute__ ((packed));

/* ASD_CC_SAS_GET_ADPT_CNTLR_CONFIG */
struct ASD_SAS_GET_ADPT_CNTLR_CONFIG {
	__u16 usPCIVendorID;
	__u16 usPCIDeviceID;
	__u16 usPCISubsystemVendorID;
	__u16 usPCISubsystemID;
	__u32 uControllerFlags;
	__u16 usFlashManufacturerID;
	__u16 usFlashDeviceID;
}__attribute__ ((packed));

struct ASD_SAS_GET_ADPT_CNTLR_CONFIG_BUFFER {
	struct IOCTL_HEADER IoctlHeader;
	struct ASD_SAS_GET_ADPT_CNTLR_CONFIG Configuration;
}__attribute__ ((packed));

/* ASD_CC_SAS_GET_NV_SEGMENT_PROPERTIES */
struct ASD_SAS_NV_SEGMENT_PROPERTIES {
	__u32 uStatus;
	__u32 uSegmentID;
	__u16 usSegmentAttributes;
	__u32 uSegmentSize;
	__u32 uSegmentImageSize;
}__attribute__ ((packed));

struct ASD_SAS_NV_SEGMENT_PROPERTIES_BUFFER {
	struct IOCTL_HEADER IoctlHeader;
	struct ASD_SAS_NV_SEGMENT_PROPERTIES Information;
}__attribute__ ((packed));

/* ASD_CC_SAS_WRITE_NV_SEGMENT */
struct ASD_SAS_WRITE_NV_SEGMENT {
	__u32 uStatus;
	__u32 uSegmentID;
	__u32 uDestinationOffset;
	__u32 uBufferLength;
}__attribute__ ((packed));

struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER {
	struct IOCTL_HEADER IoctlHeader;
	struct ASD_SAS_WRITE_NV_SEGMENT Information;
	__u8 bSourceBuffer[1];
}__attribute__ ((packed));

/* ASD_CC_SAS_READ_NV_SEGMENT */
struct ASD_SAS_READ_NV_SEGMENT {
	__u32 uStatus;
	__u32 uSegmentID;
	__u32 uSourceOffset;
	__u32 uBytesToRead;
	__u32 uBytesRead;
}__attribute__ ((packed));

struct ASD_SAS_READ_NV_SEGMENT_BUFFER {
	struct IOCTL_HEADER IoctlHeader;
	struct ASD_SAS_READ_NV_SEGMENT Information;
	__u8 bDestinationBuffer[1];
}__attribute__ ((packed));

#define ASD_ADAPTEC_MAGIC 'E'

/* Control Codes */
#define ASD_CC_SAS_GET_CNTLR_CONFIG		\
	_IOWR(ASD_ADAPTEC_MAGIC, 0x10,		\
	      struct ASD_SAS_CNTLR_CONFIG_BUFFER) 

#define ASD_CC_SAS_GET_ADPT_CNTLR_CONFIG	\
	_IOWR(ASD_ADAPTEC_MAGIC, 0x11,		\
	      struct ASD_SAS_GET_ADPT_CNTLR_CONFIG_BUFFER) 

#define ASD_CC_SAS_GET_NV_SEGMENT_PROPERTIES	\
	_IOWR(ASD_ADAPTEC_MAGIC, 0x12, 		\
	      struct ASD_SAS_NV_SEGMENT_PROPERTIES_BUFFER)

#define ASD_CC_SAS_WRITE_NV_SEGMENT 		\
	_IOWR(ASD_ADAPTEC_MAGIC, 0x13, 		\
	      struct ASD_SAS_WRITE_NV_SEGMENT_BUFFER) 

#define ASD_CC_SAS_READ_NV_SEGMENT 		\
	_IOWR(ASD_ADAPTEC_MAGIC, 0x14, 		\
	      struct ASD_SAS_READ_NV_SEGMENT_BUFFER) 

#endif /* _ASD_IOCTL_H_ */
