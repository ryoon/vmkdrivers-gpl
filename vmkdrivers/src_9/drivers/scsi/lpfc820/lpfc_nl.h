/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2010 Emulex.  All rights reserved.                *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

/*
 * All net link event payloads will begin with and event type
 * and subcategory. The event type must come first.
 * The subcategory further defines the data that follows in the rest
 * of the payload. Each category will have its own unique header plus
 * any addtional data unique to the subcategory.
 * The payload sent via the fc transport is one-way driver->application.
 */

/* RSCN event header */
struct lpfc_rscn_event_header {
	uint32_t event_type;
	uint32_t payload_length; /* RSCN data length in bytes */
	uint32_t rscn_payload[];
};

/* els event header */
struct lpfc_els_event_header {
	uint32_t event_type;
	uint32_t subcategory;
	uint8_t wwpn[8];
	uint8_t wwnn[8];
};

/* special els lsrjt event */
struct lpfc_lsrjt_event {
	struct lpfc_els_event_header header;
	uint32_t command;
	uint32_t reason_code;
	uint32_t explanation;
};

/* special els logo event */
struct lpfc_logo_event {
	struct lpfc_els_event_header header;
	uint8_t logo_wwpn[8];
};

/* fabric event header */
struct lpfc_fabric_event_header {
	uint32_t event_type;
	uint32_t subcategory;
	uint8_t wwpn[8];
	uint8_t wwnn[8];
};

/* special case fabric fcprdchkerr event */
struct lpfc_fcprdchkerr_event {
	struct lpfc_fabric_event_header header;
	uint32_t lun;
	uint32_t opcode;
	uint32_t fcpiparam;
};


/* scsi event header */
struct lpfc_scsi_event_header {
	uint32_t event_type;
	uint32_t subcategory;
	uint32_t lun;
	uint8_t wwpn[8];
	uint8_t wwnn[8];
};

/* special case scsi varqueuedepth event */
struct lpfc_scsi_varqueuedepth_event {
	struct lpfc_scsi_event_header scsi_event;
	uint32_t oldval;
	uint32_t newval;
};

/* special case scsi check condition event */
struct lpfc_scsi_check_condition_event {
	struct lpfc_scsi_event_header scsi_event;
	uint8_t opcode;
	uint8_t sense_key;
	uint8_t asc;
	uint8_t ascq;
};

/* board event header */
struct lpfc_board_event_header {
	uint32_t event_type;
	uint32_t subcategory;
};

/* adapter event header */
struct lpfc_adapter_event_header {
	uint32_t event_type;
	uint32_t subcategory;
};

/* Common format for all the SD Events */
struct lpfc_sd_event {
	uint8_t vport_name[8];
	uint8_t event_payload[];
};
