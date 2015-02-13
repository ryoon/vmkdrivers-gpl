/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2004-2007 Adaptec, Inc
 *
 * Copyright (c) 2004-2007 Adaptec, Inc. (aacraid@adaptec.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *   csmi.h
 *
 * Abstract: All CSMI IOCTL definitions are here
 */

/*
 *	This file is based on the following CSMI revision
 */
#define	CSMI_MAJOR_REVISION	0
#define	CSMI_MINOR_REVISION	82

/*
 *	IoctlHeader.ReturnCode
 */
#define	CSMI_SAS_STATUS_SUCCESS			0
#define	CSMI_SAS_STATUS_FAILED			1
#define	CSMI_SAS_STATUS_BAD_CNTL_CODE		2
#define	CSMI_SAS_STATUS_INVALID_PARAMETER	3
#define CSMI_SAS_PHY_INFO_NOT_CHANGEABLE	2000
#define CSMI_SAS_NO_SATA_DEVICE			2009

/*
 *	Status.uStatus
 */
#define	CSMI_SAS_CNTLR_STATUS_GOOD	1
#define	CSMI_SAS_CNTLR_STATUS_FAILED	2
#define CSMI_SAS_CNTLR_STATUS_OFFLINE	3

/*
 *	Status.uOfflineReason
 */
#define CSMI_SAS_OFFLINE_REASON_NO_REASON	0

/*
 *	IoctlHeader.ControlCode
 */
#define CSMI_SAS_RAID_SET_OUT_OF_RANGE	1000

/*
 *	Parameters.uFlags
 */
#define	CSMI_SAS_STP_READ		0x00000001
#define	CSMI_SAS_STP_DMA		0x00000020
#define	CSMI_SAS_STP_DMA_QUEUED		0x00000080
#define	CSMI_SAS_STP_RESET_DEVICE	0x00000200	

/*
 *	Status.bConnectionStatus
 */
#define	CSMI_SAS_OPEN_ACCEPT		0

/*
 *	Configuration.bIoBusType
 */
#define	CSMI_SAS_BUS_TYPE_PCI		3

/*
 *	Configuration.bControllerClass
 */
#define	CSMI_SAS_CNTLR_CLASS_HBA	5

/*
 *	Configuration.uControllerFlags
 */
#define	CSMI_SAS_CNTLR_SAS_HBA		0x00000001
#define	CSMI_SAS_CNTLR_SAS_RAID		0x00000002
#define	CSMI_SAS_CNTLR_SATA_HBA		0x00000004
#define	CSMI_SAS_CNTLR_SATA_RAID	0x00000008

/*
 *	Configuration.usSlotNumber
 */
#define SLOT_NUMBER_UNKNOWN		0xFFFF

/*
 *	CSMI ioctl commands
 */
/* #define CSMI_ALL_SIGNATURE		"CSMIALL" */
#define	CC_CSMI_SAS_GET_DRIVER_INFO	0xCC770001
#define	CC_CSMI_SAS_GET_CNTLR_CONFIG	0xCC770002
#define	CC_CSMI_SAS_GET_CNTLR_STATUS	0xCC770003
#define	CC_CSMI_SAS_FIRMWARE_DOWNLOAD	0xCC770004

/* #define CSMI_RAID_SIGNATURE		"CSMIARY" */
#define	CC_CSMI_SAS_GET_RAID_INFO	0xCC77000A
#define	CC_CSMI_SAS_GET_RAID_CONFIG	0xCC77000B

/* #define CSMI_SAS_SIGNATURE		"CSMISAS" */
#define	CC_CSMI_SAS_GET_PHY_INFO	0xCC770014
#define	CC_CSMI_SAS_SET_PHY_INFO	0xCC770015
#define	CC_CSMI_SAS_GET_LINK_ERRORS	0xCC770016
#define	CC_CSMI_SAS_SSP_PASSTHRU	0xCC770017
#define	CC_CSMI_SAS_SMP_PASSTHRU	0xCC770018
#define	CC_CSMI_SAS_STP_PASSTHRU	0xCC770019
#define CC_CSMI_SAS_GET_SATA_SIGNATURE	0xCC770020
#define	CC_CSMI_SAS_GET_SCSI_ADDRESS	0xCC770021
#define	CC_CSMI_SAS_GET_DEVICE_ADDRESS	0xCC770022
#define	CC_CSMI_SAS_TASK_MANAGEMENT	0xCC770023
#define	CC_CSMI_SAS_GET_CONNECTOR_INFO	0xCC770024

/* #define CSMI_PHY_SIGNATURE		"CSMIPHY" */
#define CC_CSMI_SAS_PHY_CONTROL		0xCC77003C

typedef struct {
	u32	IOControllerNumber;
	u32	Length;
	u32	ReturnCode;
	u32	Timeout;
	u16	Direction;
#ifdef CSMI_8_BYTE_ALIGNED
	u16	Reserved[3];
#endif
} IOCTL_HEADER;
typedef IOCTL_HEADER *PIOCTL_HEADER;

/* CC_CSMI_SAS_GET_DRIVER_INFO */

typedef struct {
	u8	szName[81];
	u8	szDescription[81];
	u16	usMajorRevision;
	u16	usMinorRevision;
	u16	usBuildRevision;
	u16	usReleaseRevision;
	u16	usCSMIMajorRevision;
	u16	usCSMIMinorRevision;
#ifdef CSMI_8_BYTE_ALIGNED
	u16	usReserved;
#endif
} CSMI_SAS_DRIVER_INFO;

typedef struct {
	IOCTL_HEADER	IoctlHeader;
	CSMI_SAS_DRIVER_INFO	Information;
} CSMI_SAS_DRIVER_INFO_BUFFER;
typedef CSMI_SAS_DRIVER_INFO_BUFFER * PCSMI_SAS_DRIVER_INFO_BUFFER;

/* CC_CSMI_SAS_GET_CNTLR_CONFIG */

typedef struct {
	u8	bBusNumber;
	u8	bDeviceNumber;
	u8	bFunctionNumber;
	u8	bReserved;
} CSMI_SAS_PCI_BUS_ADDRESS;

typedef union {
	CSMI_SAS_PCI_BUS_ADDRESS	PciAddress;
	u8	bReserved[32];
} CSMI_SAS_IO_BUS_ADDRESS;

typedef struct {
	u32	uBaseIoAddress;
#ifdef CSMI_8_BYTE_ALIGNED
	u32	uReserved;
#endif
	struct {
		u32	uLowPart;
		u32	uHighPart;
	} BaseMemoryAddress;
	u32	uBoardID;
	u16	usSlotNumber;
	u8	bControllerClass;
	u8	bIoBusType;
	CSMI_SAS_IO_BUS_ADDRESS	BusAddress;
	u8	szSerialNumber[81];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserve;
#endif
	u16	usMajorRevision;
	u16	usMinorRevision;
	u16	usBuildRevision;
	u16	usReleaseRevision;
	u16	usBIOSMajorRevision;
	u16	usBIOSMinorRevision;
	u16	usBIOSBuildRevision;
	u16	usBIOSReleaseRevision;
#ifdef CSMI_8_BYTE_ALIGNED
	u16	usReserved;
#endif
	u32	uControllerFlags;
	u16	usRromMajorRevision;
	u16	usRromMinorRevision;
	u16	usRromBuildRevision;
	u16	usRromReleaseRevision;
	u16	usRromBIOSMajorRevision;
	u16	usRromBIOSMinorRevision;
	u16	usRromBIOSBuildRevision;
	u16	usRromBIOSReleaseRevision;
	u8	bReserved[7];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved1;
#endif
} CSMI_SAS_CNTLR_CONFIG;

typedef struct {
	IOCTL_HEADER	IoctlHeader;
	CSMI_SAS_CNTLR_CONFIG	Configuration;
} CSMI_SAS_CNTLR_CONFIG_BUFFER;
typedef CSMI_SAS_CNTLR_CONFIG_BUFFER * PCSMI_SAS_CNTLR_CONFIG_BUFFER;

/* CC_CSMI_SAS_GET_CNTLR_STATUS */

typedef struct {
	u32	uStatus;
	u32	uOfflineReason;
	u8	bReserved[28];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved[4];
#endif
} CSMI_SAS_CNTLR_STATUS;

typedef struct {
	IOCTL_HEADER	IoctlHeader;
	CSMI_SAS_CNTLR_STATUS	Status;
} CSMI_SAS_CNTLR_STATUS_BUFFER;
typedef CSMI_SAS_CNTLR_STATUS_BUFFER * PCSMI_SAS_CNTLR_STATUS_BUFFER;

/* CC_CSMI_SAS_GET_SATA_SIGNATURE */

typedef struct {
	u8	pPhyIdentifier;
	u8	bReserved[3];
	u8	bSignatureFIS[20];
} CSMI_SAS_SATA_SIGNATURE;

typedef struct {
	IOCTL_HEADER IoctlHeader;
	CSMI_SAS_SATA_SIGNATURE Signature;
} CSMI_SAS_SATA_SIGNATURE_BUFFER;
typedef CSMI_SAS_SATA_SIGNATURE_BUFFER * PCSMI_SAS_SATA_SIGNATURE_BUFFER;

/* CC_CSMI_SAS_GET_RAID_INFO */

typedef struct {
	u32	uNumRaidSets;
	u32	uMaxDrivesPerSet;
	u8	bReserved[92];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved1[4];
#endif
} CSMI_SAS_RAID_INFO;

typedef struct {
	IOCTL_HEADER	IoctlHeader;
	CSMI_SAS_RAID_INFO	Information;
} CSMI_SAS_RAID_INFO_BUFFER;
typedef CSMI_SAS_RAID_INFO_BUFFER * PCSMI_SAS_RAID_INFO_BUFFER;

/* CC_CSMI_SAS_GET_RAID_CONFIG */

typedef struct {
	u8	bModel[40];
	u8	bFirmware[8];
	u8	bSerialNumber[40];
	u8	bSASAddress[8];
	u8	bSASLun[8];
	u8	bDriveStatus;
	u8	bDriveUsage;
	u8	bReserved[30];
} CSMI_SAS_RAID_DRIVES;

typedef struct {
	u32	uRaidSetIndex;
	u32	uCapacity;
	u32	uStripeSize;
	u8	bRaidType;
	u8	bStatus;
	u8	bInformation;
	u8	bDriveCount;
	u8	bReserved[20];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved1[4];
#endif
	CSMI_SAS_RAID_DRIVES	Drives[1];
} CSMI_SAS_RAID_CONFIG;

typedef struct {
	IOCTL_HEADER IoctlHeader;
	CSMI_SAS_RAID_CONFIG Configuration;
} CSMI_SAS_RAID_CONFIG_BUFFER;
typedef CSMI_SAS_RAID_CONFIG_BUFFER * PCSMI_SAS_RAID_CONFIG_BUFFER;

/* CC_CSMI_SAS_GET_PHY_INFO */

typedef struct {
	u8	bDeviceType;
	u8	bRestricted;
	u8	bInitiatorPortProtocol;
	u8	bTargetPortProtocol;
	u8	bRestricted2[8];
	u8	bSASAddress[8];
	u8	bPhyIdentifier;
	u8	bSignalClass;
	u8	bReserved[6];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved1[4];
#endif
} CSMI_SAS_IDENTIFY;

typedef struct {
	CSMI_SAS_IDENTIFY	Identify;
	u8	bPortIdentifier;
	u8	bNegotiatedLinkRate;
	u8	bMinimumLinkRate;
	u8	bMaximumLinkRate;
	u8	bPhyChangeCount;
	u8	bAutoDiscover;
	u8	bReserved[2];
	CSMI_SAS_IDENTIFY	Attached;
} CSMI_SAS_PHY_ENTITY;

typedef struct {
	u8	bNumberofPhys;
	u8	bReserved[3];
#ifdef CSMI_8_BYTE_ALIGNED
	u8	bReserved1[4];
#endif
	CSMI_SAS_PHY_ENTITY Phy[32];
} CSMI_SAS_PHY_INFO;

typedef struct {
	IOCTL_HEADER IoctlHeader;
	CSMI_SAS_PHY_INFO Information;
} CSMI_SAS_PHY_INFO_BUFFER;
typedef CSMI_SAS_PHY_INFO_BUFFER * PCSMI_SAS_PHY_INFO_BUFFER;

/* CC_CSMI_SAS_SET_PHY_INFO */

typedef struct {
	u8	bPhyIdentifier;
	u8	bNegotiatedLinkRate;
	u8	bProgrammedMinimumLinkRate;
	u8	bProgrammedMaximumLinkRate;
	u8	bSignalClass;
	u8	bReserved[3];
} CSMI_SAS_SET_PHY_INFO;

typedef struct {
	IOCTL_HEADER IoctlHeader;
	CSMI_SAS_SET_PHY_INFO Information;
} CSMI_SAS_SET_PHY_INFO_BUFFER;
typedef CSMI_SAS_SET_PHY_INFO_BUFFER * PCSMI_SAS_SET_PHY_INFO_BUFFER;

/* CC_CSMI_SAS_STP_PASSTHRU */

typedef struct {
	u8	bPhyIdentifier;
	u8	bPortIdentifier;
	u8	bConnectionRate;
	u8	bReserved;
	u8	bDestinationSASAddress[8];
	u8	bReserved2[4];
	u8	bCommandFIS[20];
	u32	uFlags;
	u32	uDataLength;
#ifdef CSMI_8_BYTE_ALIGNED
	u32	uReserved;
#endif
} CSMI_SAS_STP_PASSTHRU;

typedef struct {
	u8	bConnectionStatus;
	u8	bReserved[3];
	u8	bStatusFIS[20];
	u32	uSCR[16];
	u32	uDataBytes;
#ifdef CSMI_8_BYTE_ALIGNED
	u32	uReserved;
#endif
} CSMI_SAS_STP_PASSTHRU_STATUS;

typedef struct {
	IOCTL_HEADER	IoctlHeader;
	CSMI_SAS_STP_PASSTHRU	Parameters;
	CSMI_SAS_STP_PASSTHRU_STATUS	Status;
	u8	bDataBuffer[1];
} CSMI_SAS_STP_PASSTHRU_BUFFER;
typedef CSMI_SAS_STP_PASSTHRU_BUFFER * PCSMI_SAS_STP_PASSTHRU_BUFFER;

int aac_csmi_ioctl(struct aac_dev *, int, void __user *);
