/*
 * Adaptec ADP94xx SAS HBA device driver for Linux. 
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Modified by : Robert Tarte  <robt@PacificCodeWorks.com>
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
 * This code is adapted from: SASDiscoverSimulation.h
 *
 * $Id: //depot/razor/linux/src/adp94xx_discover.h#21 $
 * 
 */
#ifndef ADP94XX_DISCOVER_H
#define ADP94XX_DISCOVER_H

// state machine stack
// push a new state machine stack
// always operate the top state machine until it completes
// pop state machine and call post routine

typedef enum {
	DISCOVER_OK,
	DISCOVER_FAILED,
	DISCOVER_CONTINUE,
	DISCOVER_FINISHED
} DISCOVER_RESULTS;

#define STATE_STACK_SIZE	(20 * sizeof(unsigned long))
#define STATE_MACHINE_DEPTH	5

struct state_information {
	uint32_t		current_state;
	struct state_machine	*state_machine_p;
	uint8_t			stack[STATE_STACK_SIZE];
	int32_t	 		stack_top;
};

struct state_machine_context;
typedef void			(*StateMachineWakeupFunction)(
					struct state_machine_context *);

struct state_machine_context {
	int8_t				state_stack_top;
	struct state_information	state_stack[STATE_MACHINE_DEPTH];
	void 				*state_handle;
	StateMachineWakeupFunction	wakeup_state_machine;
};

typedef DISCOVER_RESULTS	(*DiscoveryFunction)(
					struct state_machine_context *);

typedef DISCOVER_RESULTS	(*VDiscoveryFunction)(
					struct state_machine_context *,
					void *);

typedef void			(*DiscoveryFinishFunction)(
					struct state_machine_context *,
					DISCOVER_RESULTS);

typedef void			(*DiscoveryAbortFunction)(
					struct state_machine_context *);

struct state_machine {
	VDiscoveryFunction	initialize;
	DiscoveryFunction	state_machine;
	DiscoveryFinishFunction	finish;
	DiscoveryAbortFunction	abort;
	unsigned		first_state;
};

#define SASCPY(dest, src)	*((uint64_t *)dest) = *((uint64_t *)src)
#define SAS_ISEQUAL(sas1, sas2)	(*((uint64_t *)sas1) == *((uint64_t *)sas2))
#define SAS_ISZERO(sas)		(*((uint64_t *)sas) == 0)
#define SAS_ZERO(sas)		*((uint64_t *)sas) = 0

#define SETUP_STATE(sm_contextp)					    \
	state_infop = &(sm_contextp)->state_stack[			    \
		(sm_contextp)->state_stack_top];

#define GET_STATE_CONTEXT(sm_contextp, context)				    \
	SETUP_STATE(sm_contextp);					    \
	context = (typeof(context))&state_infop->stack[0];

#define NEW_CONTEXT(context)						    \
	state_infop = &sm_contextp->state_stack[sm_contextp->state_stack_top]; \
	context = (typeof(context))&state_infop->stack[0];		    \
	state_infop->stack_top += sizeof(*context);			    \
	if (state_infop->stack_top >= STATE_STACK_SIZE)	{		    \
		printk("state stack overrun\n");			    \
	}								    \

#define PUSH_STACK(var) {						    \
	*((typeof(var) *)&state_infop->stack[state_infop->stack_top]) = var;\
	state_infop->stack_top += sizeof(var);				    \
	if (state_infop->stack_top >= STATE_STACK_SIZE)	{		    \
		printk("state stack overrun\n");			    \
	}								    \
}

#define PUSH_STACK_ARRAY(var) {						    \
	memcpy((void *)&state_infop->stack[state_infop->stack_top], 	    \
		(void *)var, sizeof(var));				    \
	state_infop->stack_top += sizeof(var);				    \
	if (state_infop->stack_top >= STATE_STACK_SIZE)	{		    \
		printk("state stack overrun\n");			    \
	}								    \
}

#define POP_STACK(var) {						    \
	state_infop->stack_top -= sizeof(var);				    \
	if (state_infop->stack_top < 0)	{				    \
		printk("state stack underrun\n");			    \
	}								    \
	var = *((typeof(var) *)&state_infop->stack[state_infop->stack_top]);\
}

#define POP_STACK_ARRAY(var) {						    \
	state_infop->stack_top -= sizeof(var);				    \
	if (state_infop->stack_top < 0)	{				    \
		printk("state stack underrun\n");			    \
	}								    \
	memcpy((void *)var, (void *)&state_infop->stack[		    \
		state_infop->stack_top], sizeof(var));			    \
}

#define PUT_STACK(var)							    \
	*((typeof(var) *)&state_infop->stack[var##_addr]) = var;
	
#define PUT_STACK_ARRAY(var)						    \
	memcpy((void *)&state_infop->stack[var##_addr], (void *)var,	    \
		sizeof(var))

#define VAR_REF(var)							    \
	(typeof(var) *)&state_infop->stack[var##_addr]
	

#define RETURN_STACK(var) {						    \
	struct state_information	*prev_state_infop;		    \
	prev_state_infop = &sm_contextp->state_stack[			    \
		sm_contextp->state_stack_top - 1];			    \
	*((typeof(var) *)&prev_state_infop->stack[			    \
		prev_state_infop->stack_top]) =	var;			    \
	prev_state_infop->stack_top += sizeof(var);			    \
	if (prev_state_infop->stack_top >= STATE_STACK_SIZE)	{	    \
		printk("state stack overrun\n");			    \
	}								    \
}

#define POP_STATE(sm_contextp)						    \
	sm_contextp->state_stack_top--;					    \
	state_infop = &sm_contextp->state_stack[sm_contextp->state_stack_top];

#define GET_CURRENT_STATE()		state_infop->current_state



typedef enum {
	// Overall discovery state machine
	ASD_STATE_DISCOVER_START = 0x100,
	ASD_STATE_DISCOVER_ATTACHED,
	ASD_STATE_FIND_BOUNDARY,
	ASD_STATE_CONFIG_BOUNDARY_SET,
	ASD_STATE_CONFIG_ATTACHED_SET,
	ASD_STATE_SATA_SPINHOLD,
	ASD_STATE_INIT_SATA,
	ASD_STATE_INIT_SAS,
	ASD_STATE_INIT_SMP,
	ASD_STATE_FINISHED,
	ASD_STATE_FAILED,

	// General Report and Discover state machine 
	ASD_STATE_REPORT_AND_DISCOVER_START = 0x200,
	ASD_STATE_ISSUE_REPORT_GENERAL,
	ASD_STATE_ISSUE_DISCOVER_LOOP,
	ASD_STATE_REPORT_AND_DISCOVER_FINISHED,
	ASD_STATE_REPORT_AND_DISCOVER_FAILED,

	// Find Boundary state machine
	ASD_STATE_FIND_BOUNDARY_START = 0x300,
	ASD_STATE_FIND_BOUNDARY_LOOP,
	ASD_STATE_FIND_BOUNDARY_FINISHED,
	ASD_STATE_FIND_BOUNDARY_FAILED,

	// Configure Set state machine
	ASD_STATE_CONFIG_SET_START = 0x400,
	ASD_STATE_CONFIG_SET_ISSUE_DISCOVER,
	ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER,
	ASD_STATE_CONFIG_SET_FINISHED,
	ASD_STATE_CONFIG_SET_FAILED,

	// Configure Expander state machine
	ASD_STATE_CONFIG_EXPANDER_START = 0x500,
	ASD_STATE_CONFIG_EXPANDER_ROUTE,
	ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP,
	ASD_STATE_CONFIG_EXPANDER_FINISHED,
	ASD_STATE_CONFIG_EXPANDER_FAILED,

	// SATA initialization state machine
	ASD_STATE_INIT_SATA_START = 0x600,
	ASD_STATE_INIT_SATA_REPORT_PHY,
	ASD_STATE_INIT_SATA_IDENTIFY,
	ASD_STATE_INIT_SATA_CONFIGURE_FEATURES,
	ASD_STATE_INIT_SATA_FINISHED,
	ASD_STATE_INIT_SATA_FAILED,

	// SATA release spinhold state machine
	ASD_STATE_SATA_SPINHOLD_START = 0x700,
	ASD_STATE_SATA_SPINHOLD_PHY_CONTROL,
	ASD_STATE_SATA_SPINHOLD_DISCOVER,
	ASD_STATE_SATA_SPINHOLD_FINISHED,
	ASD_STATE_SATA_SPINHOLD_FAILED,

	// SAS initialization state machine
	ASD_STATE_INIT_SAS_START = 0x800,
	ASD_STATE_INIT_SAS_INQUIRY,
	ASD_STATE_INIT_SAS_GET_DEVICE_ID,
	ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER,
	ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS,
	ASD_STATE_INIT_SAS_GET_PORT_CONTROL,
	ASD_STATE_INIT_SAS_FINISHED,
	ASD_STATE_INIT_SAS_FAILED,

	// SMP initialization state machine
	ASD_STATE_INIT_SMP_START = 0x900,
	ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO,
	ASD_STATE_INIT_SMP_FINISHED,
	ASD_STATE_INIT_SMP_FAILED,

	// Configure ATA devices
	ASD_STATE_CONFIGURE_ATA_START = 0xA00,
	ASD_STATE_CONFIGURE_ATA_FEATURES,
	ASD_STATE_CONFIGURE_ATA_FINISHED,
	ASD_STATE_CONFIGURE_ATA_FAILED

} ASD_DISCOVERY_STATES;


#if 0
--------------------------
// Level 1

// Level 2
typedef enum {
	ASD_STATE_DISCOVER_ATTACHED_START,
	ASD_STATE_DISCOVER_ATTACHED_DISCOVER,
	ASD_STATE_DISCOVER_ATTACHED_DISCOVER_LOOP
	ASD_STATE_DISCOVER_ATTACHED_FINISHED
};

// Level 2
typdef enum {
	ASD_STATE_FIND_BOUNDARY_START,
	ASD_STATE_FIND_BOUNDARY_REPORT_GENERAL,
	ASD_STATE_FIND_BOUNDARY_DISCOVER,
	ASD_STATE_FIND_BOUNDARY_DISCOVER_LOOP,
	ASD_STATE_FIND_BOUNDARY_FINISHED
};

// Level 2
typedef enum {
	ASD_STATE_CONFIG_BOUNDARY_SET_START,
	ASD_STATE_DISCOVERY_DISCOVER_AND_CONFIG_BOUNDARY_SET,
	ASD_STATE_DISCOVERY_CONFIG_DISCOVER,
	ASD_STATE_DISCOVERY_CONFIG_DISCOVER_LOOP,
	ASD_STATE_DISCOVERY_CONFIG_EXPANDER_PHY,
	ASD_STATE_DISCOVERY_CONFIG_ROUTE_LOOP,
	ASD_STATE_DISCOVERY_CONFIG_ROUTE_INDEX_LOOP,
	ASD_STATE_DISCOVERY_CONFIG_EXPANDER_LOOP,
	ASD_STATE_DISCOVERY_DISCOVER_AND_CONFIG_LOOP,
	ASD_STATE_DISCOVERY_FINISHED,
	ASD_STATE_DISCOVERY_FAILED
} ASD_DISCOVERY_STATES;
--------------------------
#endif

/*
 * assume the maximum number of phys in an expander device is 128
 */
#define MAXIMUM_EXPANDER_PHYS			128

/*
 * assume the maximum number of indexes per phy is 128
 */
#define MAXIMUM_EXPANDER_INDEXES		128

/*
 * limit to 8 initiators for this example
 */
#define MAXIMUM_INITIATORS			8

/*
 * retry on SAS INQUIRY during discovery 
*/
#define MAX_SAS_INQUIRY_RETRY		2
/*
 * defines for address frame types
 */
#define ADDRESS_IDENTIFY_FRAME			0x00
#define ADDRESS_OPEN_FRAME			0x01

/*
 * defines for SMP frame types
 */
#define SMP_REQUEST_FRAME			0x40
#define SMP_RESPONSE_FRAME			0x41

/*
 * defines for SMP request functions
 */
#define REPORT_GENERAL				0x00
#define REPORT_MANUFACTURER_INFORMATION		0x01
#define DISCOVER				0x10
#define REPORT_PHY_ERROR_LOG			0x11
#define REPORT_PHY_SATA				0x12
#define REPORT_ROUTE_INFORMATION		0x13
#define CONFIGURE_ROUTE_INFORMATION		0x90
#define PHY_CONTROL				0x91

/*
 * defines for the protocol bits
 */
#define SATA					0x01
#define SMP					0x02
#define STP					0x04
#define SSP					0x08

/*
 * defines for open responses, arbitrary values, not defined in the spec
 */
#define OPEN_ACCEPT				0
#define OPEN_REJECT_BAD_DESTINATION		1
#define OPEN_REJECT_RATE_NOT_SUPPORTED		2
#define OPEN_REJECT_NO_DESTINATION		3
#define OPEN_REJECT_PATHWAY_BLOCKED		4
#define OPEN_REJECT_PROTOCOL_NOT_SUPPORTED	5
#define OPEN_REJECT_RESERVE_ABANDON		6
#define OPEN_REJECT_RESERVE_CONTINUE		7
#define OPEN_REJECT_RESERVE_INITIALIZE		8
#define OPEN_REJECT_RESERVE_STOP		9
#define OPEN_REJECT_RETRY			10
#define OPEN_REJECT_STP_RESOURCES_BUSY		11
#define OPEN_REJECT_WRONG_DESTINATION		12

/*
 * defines for INQUIRY VPD Page Device Identification (0x83)
 */
#define IDENTIFIER_TYPE_VENDOR_SPECIFIC		0x00
#define IDENTIFIER_TYPE_T10			0x01
#define IDENTIFIER_TYPE_EIU_64			0x02
#define IDENTIFIER_TYPE_NAA			0x03
#define IDENTIFIER_TYPE_RELATIVE_TARGET_PORT	0x04
#define IDENTIFIER_TYPE_TARGET_PORT_GROUP	0x05
#define IDENTIFIER_TYPE_LOGICAL_UNIT_GROUP	0x06
#define IDENTIFIER_TYPE_MD5_LOGICAL_UNIT	0x07
#define IDENTIFIER_TYPE_SCSI_NAME_STRING	0x08


/*
 * definitions for discovery algorithm use
 */
enum
{
	SAS_SIMPLE_LEVEL_DESCENT = 0,
	SAS_UNIQUE_LEVEL_DESCENT
};

/*
 * definitions for SMP function results
 */
enum SMPFunctionResult
{
	SMP_FUNCTION_ACCEPTED = 0,
	SMP_UNKNOWN_FUNCTION,
	SMP_FUNCTION_FAILED,
	SMP_INVALID_REQUEST_FRAME_LENGTH,
	SMP_PHY_DOES_NOT_EXIST = 0x10,
	SMP_INDEX_DOES_NOT_EXIST,
	SMP_PHY_DOES_NOT_SUPPORT_SATA,
	SMP_UNKNOWN_PHY_OPERATION
};

/*
 * DeviceTypes
 */
enum DeviceTypes
{
	NO_DEVICE = 0,
	END_DEVICE,
	EDGE_EXPANDER_DEVICE,
	FANOUT_EXPANDER_DEVICE
};

/*
 * RoutingAttribute
 */
enum RoutingAttribute
{
	DIRECT = 0,
	SUBTRACTIVE,
	TABLE
};

/*
 * RouteFlag
 */
enum DisableRouteEntry
{
	ENABLED = 0,
	DISABLED
};

/*
 * PhyLinkRate(s)
 */
typedef enum
{
	RATE_UNKNOWN = 0,
	PHY_DISABLED,
	PHY_FAILED,
	SPINUP_HOLD_OOB,
	GBPS_1_5 = 8,
	GBPS_3_0
} PhysicalLinkRate;

/*
 * PhyOperation
 */
typedef enum
{
	SAS_NOP = 0,
	LINK_RESET,
	HARD_RESET,
	DISABLE,
	CLEAR_ERROR_LOG = 5,
	CLEAR_AFFILIATION,
	TRANSMIT_SATA_PORT_SELECTION
} PhyOperation;

/*
 * the structures assume a char bitfield is valid, this is compiler
 * dependent defines would be more portable, but less descriptive
 * the Identify frame is exchanged following OOB, for this
 * code it contains the identity information for the attached device
 * and the initiator application client
 */
struct Identify
{
	// byte 0
	uint8_t AddressFrame:4;	// ADDRESS_IDENTIFY_FRAME
	uint8_t DeviceType:3;	// END_DEVICE
				//

	uint8_t RestrictedByte0Bit7:1;

	// byte 1
	uint8_t RestrictedByte1;

	// byte 2
	union
	{
		struct
		{
			uint8_t RestrictedByte2Bit0:1;
			uint8_t SMPInitiator:1;
			uint8_t STPInitiator:1;
			uint8_t SSPInitiator:1;
			uint8_t ReservedByte2Bit4_7:4;
		};
		uint8_t InitiatorBits;
	};

	// byte 3
	union
	{
		struct
		{
			uint8_t RestrictedByte3Bit0:1;
			uint8_t SMPTarget:1;
			uint8_t STPTarget:1;
			uint8_t SSPTarget:1;
			uint8_t ReservedByte3Bit4_7:4;
		};
		uint8_t TargetBits;
	};

	// byte 4-11
	uint8_t RestrictedByte4_11[8];

	// byte 12-19
	uint8_t SASAddress[8];

	// byte 20
	uint8_t PhyIdentifier;

	// byte 21-23
	uint8_t RestrictedByte20_23[3];

	// byte 24-27
	uint8_t ReservedByte24_27[4];

	// byte 28-31
	uint32_t CRC;
} __packed;

/*
 * the Open address frame is used to send open requests
 */
struct OpenAddress
{
	// byte 0
	uint8_t AddressFrame:4;		// ADDRESS_OPEN_FRAME
	uint8_t Protocol:3;		// SMP
					// STP
					// SSP
	uint8_t Initiator:1;

	// byte 1
	uint8_t ConnectionRate:4;		// GBPS_1_5

	// GBPS_3_0
	uint8_t Features:4;

	// byte 2-3
	uint16_t InitiatorConnectionTag;

	// byte 4-11
	uint8_t DestinationSASAddress[8];

	// byte 12-19
	uint8_t SourceSASAddress[8];

	// byte 20
	uint8_t CompatibleFeatures;

	// byte 21
	uint8_t PathwayBlockedCount;

	// byte 22-23
	uint16_t ArbitrationWaitTime;

	// byte 24-27
	uint8_t MoreCompatibleFeatures[4];

	// byte 28-31
	uint32_t CRC[4];
} __packed;

/*
 * request specific bytes for a general input function
 */
struct SMPRequestGeneralInput
{
	// byte 4-7
	uint32_t CRC;
} __packed;

/*
 * request specific bytes for a phy input function
 */
struct SMPRequestPhyInput
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t IgnoredByte10;

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-15
	uint32_t CRC;
} __packed;

/*
 * the ConfigureRouteInformation structure is used to provide the
 * expander route entry for the expander route table, it is intended
 * to be referenced by the SMPRequestConfigureRouteInformation struct
 */
struct ConfigureRouteInformation
{
	// byte 12
	uint8_t IgnoredByte12Bit0_6:7;
	uint8_t DisableRouteEntry:1;	// if a routing error is detected
					// then the route is disabled by
					// setting this bit

	// byte 13-15
	uint8_t IgnoredByte13_15[3];

	// byte 16-23
	uint8_t RoutedSASAddress[8];	// identical to the AttachedSASAddress
					// found through discovery

	// byte 24-35
	uint8_t IgnoredByte24_35[12];

	// byte 36-39
	uint8_t ReservedByte36_39[4];
} __packed;

/*
 * request specific bytes for SMP ConfigureRouteInformation function
 */
struct SMPRequestConfigureRouteInformation
{
	// byte 4-5
	uint8_t ReservedByte4_5[2];

	// byte 6-7
	uint16_t ExpanderRouteIndex;

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10-11
	uint8_t ReservedByte10_11[2];

	// byte 12-39
	struct ConfigureRouteInformation Configure;

	// byte 40-43
	uint32_t CRC;
} __packed;

/*
 * the PhyControlInformation structure is used to provide the
 * expander phy control values, it is intended
 * to be referenced by the SMPRequestPhyControl struct
 */
struct PhyControlInformation
{
	// byte 12-31
	uint8_t IgnoredByte12_31[20];

	// byte 32
	uint8_t IgnoredByte32Bit0_3:4;
	uint8_t ProgrammedMinimumPhysicalLinkRate:4;

	// byte 33
	uint8_t IgnoredByte33Bit0_3:4;
	uint8_t ProgrammedMaximumPhysicalLinkRate:4;

	// byte 34-35
	uint8_t IgnoredByte34_35[2];

	// byte 36
	uint8_t PartialPathwayTimeoutValue:4;
	uint8_t ReservedByte36Bit4_7:4;

	// byte 37-39
	uint8_t ReservedByte37_39[3];
} __packed;

/*
 * request specific bytes for SMP Phy Control function
 */
struct SMPRequestPhyControl
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t PhyOperation;

	// byte 11
	uint8_t UpdatePartialPathwayTimeoutValue:1;
	uint8_t ReservedByte11Bit1_7:7;

	// byte 12-39
	struct PhyControlInformation Control;

	// byte 40-43
	uint32_t CRC;
} __packed;

/*
 * generic structure referencing an SMP Request, must be initialized
 * before being used
 */
struct SMPRequest
{
	// byte 0
	uint8_t SMPFrameType;		// always SMP_REQUEST_FRAME

	// byte 1
	uint8_t Function;			// REPORT_GENERAL
					// REPORT_MANUFACTURER_INFORMATION
					// DISCOVER
					// REPORT_PHY_ERROR_LOG
					// REPORT_PHY_SATA
					// REPORT_ROUTE_INFORMATION
					// CONFIGURE_ROUTE_INFORMATION
					// PHY_CONTROL

	// byte 2-3
	uint8_t ReservedByte2_3[2];

	// byte 4-n
	union
	{
		struct SMPRequestGeneralInput ReportGeneral;
		struct SMPRequestGeneralInput ReportManufacturerInfo;
		struct SMPRequestPhyInput Discover;
		struct SMPRequestPhyInput ReportPhyErrorLog;
		struct SMPRequestPhyInput ReportPhySATA;
		struct SMPRequestPhyInput ReportRouteInformation;
		struct SMPRequestConfigureRouteInformation ConfigureRouteInformation;
		struct SMPRequestPhyControl PhyControl;
	} Request;
} __packed;

/*
 * request specific bytes for SMP Report General response, intended to be
 * referenced by SMPResponse
 */
struct SMPResponseReportGeneral
{
	// byte 4-5
	uint16_t ExpanderChangeCount;

	// byte 6-7
	uint16_t ExpanderRouteIndexes;

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t NumberOfPhys;

	// byte 10
	uint8_t ConfigurableRouteTable:1;
	uint8_t Configuring:1;
	uint8_t ReservedByte10Bit2_7:6;

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-15
	uint32_t CRC;
} __packed;

/*
 * request specific bytes for SMP Report Manufacturer Information response,
 * intended to be referenced by SMPResponse
 */
struct SMPResponseReportManufacturerInfo
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9-10
	uint8_t IgnoredByte9_10[2];

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-19
	uint8_t VendorIdentification[8];

	// byte 20-35
	uint8_t ProductIdentification[16];

	// byte 36-39
	uint8_t ProductRevisionLevel[4];

	// byte 40-59
	uint8_t VendorSpecific[20];

	// byte 60-63
	uint32_t CRC;
} __packed;

/*
 * the Discover structure is used to retrieve expander port information
 * it is intended to be referenced by the SMPResponseDiscover structure
 */
struct Discover
{
	// byte 12
	uint8_t ReservedByte12Bit0_3:4;
	uint8_t AttachedDeviceType:3;
	uint8_t IgnoredByte12Bit7:1;

	// byte 13
	uint8_t NegotiatedPhysicalLinkRate:4;
	uint8_t ReservedByte13Bit4_7:4;

	// byte 14
	union
	{
		struct
		{
			uint8_t AttachedSATAHost:1;
			uint8_t AttachedSMPInitiator:1;
			uint8_t AttachedSTPInitiator:1;
			uint8_t AttachedSSPInitiator:1;
			uint8_t ReservedByte14Bit4_7:4;
		};
		uint8_t InitiatorBits;
	};

	// byte 15
	union
	{
		struct
		{
			uint8_t AttachedSATADevice:1;
			uint8_t AttachedSMPTarget:1;
			uint8_t AttachedSTPTarget:1;
			uint8_t AttachedSSPTarget:1;
			uint8_t ReservedByte15Bit4_7:4;
		};
		uint8_t TargetBits;
	};

	// byte 16-23
	uint8_t SASAddress[8];

	// byte 24-31
	uint8_t AttachedSASAddress[8];

	// byte 32
	uint8_t AttachedPhyIdentifier;

	// byte 33-39
	uint8_t ReservedByte33_39[7];

	// byte 40
	uint8_t HardwareMinimumPhysicalLinkRate:4;
	uint8_t ProgrammedMinimumPhysicalLinkRate:4;

	// byte 41
	uint8_t HardwareMaximumPhysicalLinkRate:4;
	uint8_t ProgrammedMaximumPhysicalLinkRate:4;

	// byte 42
	uint8_t PhyChangeCount;

	// byte 43
	uint8_t PartialPathwayTimeoutValue:4;
	uint8_t IgnoredByte36Bit4_6:3;
	uint8_t VirtualPhy:1;

	// byte 44
	uint8_t RoutingAttribute:4;
	uint8_t ReservedByte44Bit4_7:4;

	// byte 45-49
	uint8_t ReservedByte45_49[5];

	// byte 50-51
	uint8_t VendorSpecific[2];

	// byte 52-55
	uint32_t CRC;
} __packed;

/*
 * response specific bytes for SMP Discover, intended to be referenced by
 * SMPResponse
 */
struct SMPResponseDiscover
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t IgnoredByte10;

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-55
	struct Discover Result;
} __packed;

/*
 * response specific bytes for SMP Report Phy Error Log, intended to be
 * referenced by SMPResponse
 */
struct SMPResponseReportPhyErrorLog
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t IgnoredByte10;

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-15
	uint32_t InvalidDuint16_tCount;

	// byte 16-19
	uint32_t DisparityErrorCount;

	// byte 20-23
	uint32_t LossOfDuint16_tSynchronizationCount;

	// byte 24-27
	uint32_t PhyResetProblemCount;

	// byte 28-31
	uint32_t CRC;
} __packed;

/*
 * this structure describes the Register Device to Host FIS defined in the
 * SATA specification
 */
struct RegisterDeviceToHostFIS
{
	// byte 24
	uint8_t FISType;

	// byte 25
	uint8_t ReservedByte25Bit0_5:6;
	uint8_t Interrupt:1;
	uint8_t ReservedByte25Bit7:1;

	// byte 26
	uint8_t Status;

	// byte 27
	uint8_t Error;

	// byte 28
	uint8_t SectorNumber;

	// byte 29
	uint8_t CylLow;

	// byte 30
	uint8_t CylHigh;

	// byte 31
	uint8_t DevHead;

	// byte 32
	uint8_t SectorNumberExp;

	// byte 33
	uint8_t CylLowExp;

	// byte 34
	uint8_t CylHighExp;

	// byte 35
	uint8_t ReservedByte35;

	// byte 36
	uint8_t SectorCount;

	// byte 37
	uint8_t SectorCountExp;

	// byte 38-43
	uint8_t ReservedByte38_43[6];
} __packed;

/*
 * response specific bytes for SMP Report Phy SATA, intended to be
 * referenced by SMPResponse
 */
struct SMPResponseReportPhySATA
{
	// byte 4-7
	uint8_t IgnoredByte4_7[4];

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t IgnoredByte10;

	// byte 11
	uint8_t AffilationValid:1;
	uint8_t AffilationsSupported:1;
	uint8_t ReservedByte11Bit2_7:6;

	// byte 12-15
	uint8_t ReservedByte12_15[4];

	// byte 16-32
	uint8_t STPSASAddress[8];

	// byte 24-43
	struct RegisterDeviceToHostFIS FIS;

	// byte 44-47
	uint8_t ReservedByte44_47[4];

	// byte 48-55
	uint8_t AffiliatedSTPInitiatorSASAddress[8];

	// byte 56-59
	uint32_t CRC;
} __packed;

struct ReportRouteInformation
{
	// byte 12
	uint8_t IgnoredByte12Bit0_6:7;
	uint8_t ExpanderRouteEntryDisabled:1;

	// byte 13-15
	uint8_t IgnoredByte13_15[3];

	// byte 16-23
	uint8_t RoutedSASAddress[8];

	// byte 24-35
	uint8_t IgnoredByte24_35[12];

	// byte 36-39
	uint8_t ReservedByte36_39[4];
} __packed;

/*
 * response specific bytes for SMP Report Route Information, intended to be
 * referenced by SMPResponse
 */
struct SMPResponseReportRouteInformation
{
	// byte 4-5
	uint8_t IgnoredByte4_5[4];

	// byte 6-7
	uint16_t ExpanderRouteIndex;

	// byte 8
	uint8_t ReservedByte8;

	// byte 9
	uint8_t PhyIdentifier;

	// byte 10
	uint8_t IgnoredByte10;

	// byte 11
	uint8_t ReservedByte11;

	// byte 12-39
	struct ReportRouteInformation Result;

	// byte 40-43
	uint32_t CRC;
} __packed;

/*
 * response specific bytes for SMP Configure Route Information,
 * intended to be referenced by SMPResponse
 */
struct SMPResponseConfigureRouteInformation
{
	// byte 4-7
	uint32_t CRC;
} __packed;

/*
 * response specific bytes for SMP Phy Control,
 * intended to be referenced by SMPResponse
 */
struct SMPResponsePhyControl
{
	// byte 4-7
	uint32_t CRC;
} __packed;

/*
 * generic structure referencing an SMP Response, must be initialized
 * before being used
 */
struct SMPResponse
{
	// byte 0
	uint8_t SMPFrameType;		// always 41h for SMP responses

	// byte 1
	uint8_t Function;

	// byte 2
	uint8_t FunctionResult;

	// byte 3
	uint8_t ReservedByte3;

	// byte 4-n
	union
	{
		struct SMPResponseReportGeneral ReportGeneral;
		struct SMPResponseReportManufacturerInfo
			ReportManufacturerInfo;
		struct SMPResponseDiscover Discover;
		struct SMPResponseReportPhyErrorLog ReportPhyErrorLog;
		struct SMPResponseReportPhySATA ReportPhySATA;
		struct SMPResponseReportRouteInformation ReportRouteInformation;
		struct SMPResponseConfigureRouteInformation ConfigureRouteInformation;
		struct SMPResponsePhyControl PhyControl;
	} Response;
} __packed;

DISCOVER_RESULTS
asd_smp_request(
struct state_machine_context	*sm_contextp,
struct asd_target		*target,
unsigned			request_length,
unsigned			response_length
);

void
asd_destroy_discover_list(
struct asd_softc	*asd,
struct list_head	*discover_list
);

/* -------------------------------------------------------------------------- */

struct asd_InitSMP_SM_Arguments {
	struct list_head		*discover_listp;
};

struct asd_InitSAS_SM_Arguments {
	struct list_head		*discover_listp;
};

struct asd_SATA_SpinHoldSM_Arguments {
	struct list_head		*discover_listp;
};

struct asd_InitSATA_SM_Arguments {
	struct list_head		*discover_listp;
};

struct asd_ConfigureATA_SM_Arguments {
	struct asd_target		*target;
};

struct asd_ConfigureExpanderSM_Arguments {
	struct list_head		*discover_listp;
	struct asd_target		*newExpander;
};

struct asd_DiscoverConfigSetSM_Arguments {
	struct list_head		*discover_listp;
	struct list_head		*found_listp;
	struct list_head		*old_discover_listp;
	struct asd_target		*currentExpander;
};

struct asd_DiscoverFindBoundarySM_Arguments {
	struct asd_target		*expander;
	struct list_head		*found_listp;
	struct list_head		*old_discover_listp;
};

struct asd_DiscoverExpanderSM_Arguments {
	uint8_t				*sas_addr;
	struct asd_target		*upstreamExpander;
	unsigned			attachedDeviceType;
	struct list_head		*old_discover_listp;
	struct list_head		*found_listp;
	unsigned			conn_rate;
};

extern struct state_machine asd_DiscoverySM;
extern struct state_machine asd_DiscoverExpanderSM;
extern struct state_machine asd_DiscoverFindBoundarySM;
extern struct state_machine asd_DiscoverConfigSetSM;
extern struct state_machine asd_ConfigureExpanderSM;
extern struct state_machine asd_ConfigureATA_SM;
extern struct state_machine asd_InitSATA_SM;
extern struct state_machine asd_SATA_SpinHoldSM;
extern struct state_machine asd_InitSAS_M;
extern struct state_machine asd_InitSMP_M;

DISCOVER_RESULTS
asd_push_state_machine(
struct state_machine_context	*sm_contextp,
char				*s,
struct state_machine		*state_machine_p,
void				*arg
);

#define ASD_PUSH_STATE_MACHINE(sm_contextp, state_machine_p, arg)	\
	asd_push_state_machine(sm_contextp, #state_machine_p,		\
	state_machine_p, arg)


#define STATE_MACHINE_DECLARATION(name) \
DISCOVER_RESULTS		\
name##_Initialize(		\
struct state_machine_context *,	\
void *				\
);				\
				\
DISCOVER_RESULTS		\
name##_StateMachine(		\
struct state_machine_context *	\
);				\
				\
void				\
name##_Finish(			\
struct state_machine_context *,	\
DISCOVER_RESULTS		\
);				\
				\
void				\
name##_Abort(			\
struct state_machine_context *	\
);

STATE_MACHINE_DECLARATION(asd_DiscoverySM);
STATE_MACHINE_DECLARATION(asd_DiscoverExpanderSM);
STATE_MACHINE_DECLARATION(asd_DiscoverFindBoundarySM);
STATE_MACHINE_DECLARATION(asd_DiscoverConfigSetSM);
STATE_MACHINE_DECLARATION(asd_ConfigureExpanderSM);
STATE_MACHINE_DECLARATION(asd_InitSATA_SM);
STATE_MACHINE_DECLARATION(asd_ConfigureATA_SM);
STATE_MACHINE_DECLARATION(asd_SATA_SpinHoldSM);
STATE_MACHINE_DECLARATION(asd_InitSAS_SM);
STATE_MACHINE_DECLARATION(asd_InitSMP_SM);

struct asd_target *
asd_find_target(
struct list_head	*discover_list,
uint8_t			*SASAddress
);

DISCOVER_RESULTS
asd_state_init_sata(
struct state_machine_context	*sm_contextp
);

ASD_DISCOVERY_STATES
asd_state_init_sata_post(
struct state_machine_context	*sm_contextp
);

uint8_t
asd_get_feature_to_enable(
struct asd_target	*target,
uint8_t			*sector_count
);

void
asd_sata_identify_request_done(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_smp_request_done(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_ssp_request_done(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);

void
asd_sata_configure_features_done(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
);
#endif /* ADP94XX_DISCOVER_H */
