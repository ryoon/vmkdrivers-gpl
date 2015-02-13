/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
/*
 * Error codes for HAL - NIC interface.
 *
 */

#ifndef _NX_ERRORCODE_H_
#define _NX_ERRORCODE_H_

#define NX_RCODE_DRIVER_INFO 	   0x20000000
#define NX_RCODE_DRIVER_CAN_RELOAD 0x40000000
#define NX_RCODE_FATAL_ERROR	   0x80000000
#define NX_ERROR_PEGNUM(incode)		(incode & 0x0FF)
#define NX_ERROR_ERRCODE(incode)	((incode & 0x0FFFFF00) >> 8)


/*****************************************************************************
 *        Common Error Codes
 *****************************************************************************/

#define	NX_RCODE_SUCCESS          0
#define	NX_RCODE_NO_HOST_MEM      1	/* Insuff. mem resource on host */
#define	NX_RCODE_NO_HOST_RESOURCE 2	/* Insuff. misc. resources on host */
#define	NX_RCODE_NO_CARD_CRB      3	/* Insuff. crb resources on card */
#define	NX_RCODE_NO_CARD_MEM      4	/* Insuff. mem resources on card */
#define	NX_RCODE_NO_CARD_RESOURCE 5	/* Insuff. misc. resources on card */
#define	NX_RCODE_INVALID_ARGS     6	/* One or more args to routine were
					   out-of-range */
#define	NX_RCODE_INVALID_ACTION   7	/* Requested action is invalid / in
					   error */
#define	NX_RCODE_INVALID_STATE    8	/* Requested RX/TX has invalid state */
#define	NX_RCODE_NOT_SUPPORTED    9	/* Requested action is not supported */
#define	NX_RCODE_NOT_PERMITTED    10	/* Requested action is not allowed */
#define	NX_RCODE_NOT_READY        11	/* System not ready for action */
#define	NX_RCODE_DOES_NOT_EXIST   12	/* Target of requested action does
					   not exist */
#define	NX_RCODE_ALREADY_EXISTS   13	/* Requested action already
					   performed/complete */
#define NX_RCODE_BAD_SIGNATURE    14    /* Invalid signature provided */
#define NX_RCODE_CMD_NOT_IMPL     15    /* Valid command, not implemented */
#define NX_RCODE_CMD_INVALID      16    /* Invalid/Unknown command */
#define NX_RCODE_TIMEOUT          17    /* Timeout on polling rsp status  */
#define NX_RCODE_CMD_FAILED       18
#define NX_RCODE_FATAL_TEMP       19	/* Temperature has exceeded max value */
#define NX_RCODE_MAX_EXCEEDED     20
#define NX_RCODE_MAX              21

/*****************************************************************************
 *       Macros
 *****************************************************************************/
#define NX_IS_RCODE_VALID(ERR) 	(ERR >= NX_RCODE_MAX)

#define NX_RCODE_FATAL_TEMP_MSG	  "Device temperature exceeds maximum allowed. Hardware has been shut down.\n"

#endif /* _NX_ERRORCODE_H_ */
