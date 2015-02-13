/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2010 Emulex.  All rights reserved.        	   *
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

#ifndef _H_LPFC_EVENTS
#define _H_LPFC_EVENTS

/* Temperature events */
#define LPFC_CRIT_TEMP		0x1
#define LPFC_THRESHOLD_TEMP	0x2
#define LPFC_NORMAL_TEMP	0x3

/* Event definitions for RegisterForEvent */
#define FC_REG_LINK_EVENT		0x01	/* link up / down events */
#define FC_REG_RSCN_EVENT		0x02	/* RSCN events */
#define FC_REG_CT_EVENT			0x04	/* CT request events */
#define FC_REG_SANDIAGS_EVENT           0x08	/* SanDiag Event */
#define FC_REG_DUMP_EVENT		0x10	/* Dump events */
#define FC_REG_TEMPERATURE_EVENT	0x20	/* temperature events */

#define FC_REG_DRIVER_GENERATED_EVENT	0x40	/* Driver generated, temp, dump, etc */
#define FC_REG_ALL_PORTS        	0x80	/* Register for all ports */

#define FC_REG_VPORTRSCN_EVENT		0x0040	/* Vport RSCN events */
#define FC_REG_ELS_EVENT		0x0080	/* lpfc els events */
#define FC_REG_FABRIC_EVENT		0x0100	/* lpfc fabric events */
#define FC_REG_SCSI_EVENT		0x0200	/* lpfc scsi events */
#define FC_REG_BOARD_EVENT		0x0400	/* lpfc board events */
#define FC_REG_ADAPTER_EVENT		0x0800	/* lpfc adapter events */

#define FC_REG_EVENT_MASK		(FC_REG_LINK_EVENT | \
					 FC_REG_RSCN_EVENT | \
					 FC_REG_CT_EVENT | \
					 FC_REG_DUMP_EVENT | \
					 FC_REG_TEMPERATURE_EVENT | \
					 FC_REG_VPORTRSCN_EVENT | \
					 FC_REG_ELS_EVENT | \
					 FC_REG_FABRIC_EVENT | \
					 FC_REG_SCSI_EVENT | \
					 FC_REG_BOARD_EVENT | \
					 FC_REG_ADAPTER_EVENT)

/* subcategory codes for FC_REG_ELS_EVENT */
#define LPFC_EVENT_PLOGI_RCV		0x01
#define LPFC_EVENT_PRLO_RCV		0x02
#define LPFC_EVENT_ADISC_RCV		0x04
#define LPFC_EVENT_LSRJT_RCV		0x08
#define LPFC_EVENT_LOGO_RCV		0x10

/* subcategory codes for FC_REG_SCSI_EVENT */
#define LPFC_EVENT_QFULL	0x0001
#define LPFC_EVENT_DEVBSY	0x0002
#define LPFC_EVENT_CHECK_COND	0x0004
#define LPFC_EVENT_LUNRESET	0x0008
#define LPFC_EVENT_TGTRESET	0x0010
#define LPFC_EVENT_BUSRESET	0x0020
#define LPFC_EVENT_VARQUEDEPTH	0x0040

/* subcategory codes for FC_REG_FABRIC_EVENT */
#define LPFC_EVENT_FABRIC_BUSY		0x01
#define LPFC_EVENT_PORT_BUSY		0x02
#define LPFC_EVENT_FCPRDCHKERR		0x04

/* event codes for FC_REG_BOARD_EVENT */
#define LPFC_EVENT_PORTINTERR		0x01

/* event codes for FC_REG_ADAPTER_EVENT */
#define LPFC_EVENT_ARRIVAL	0x01

/* BEGIN __KERNEL__ only */
#ifdef __KERNEL__

void lpfc_send_event(void *, u_int32_t, void *, struct event_type);

#endif
/* END __KERNEL__ only */

/* _H_LPFC_EVENTS */
#endif

