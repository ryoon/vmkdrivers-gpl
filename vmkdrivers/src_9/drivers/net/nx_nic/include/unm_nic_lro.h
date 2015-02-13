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
/* Header file for lro definitions */

#ifndef UNM_NIC_LRO_H
#define UNM_NIC_LRO_H


/* These are message types meaningful only on
 *   the NIC's stage1 Rx queue 
 */
#define NX_RX_STAGE1_MSGTYPE_FBQ    0x1  /* L2/L4 Miss */
#define NX_RX_STAGE1_MSGTYPE_L2IFQ  0x2  /* L2 Hit, L4 Miss */

#define NX_RX_STAGE1_MAX_FLOWS    32

#define NX_NUM_LRO_ENTRIES NX_RX_STAGE1_MAX_FLOWS
#define NX_LRO_CHECK_INTVL 0x7ff


#define ETH_HDR_SIZE 14

#define IPV4_HDR_SIZE		20
#define IPV6_HDR_SIZE		40
#define TCP_HDR_SIZE		20
#define TCP_TS_OPTION_SIZE	12
#define TCP_IPV4_HDR_SIZE	(TCP_HDR_SIZE + IPV4_HDR_SIZE)
#define TCP_IPV6_HDR_SIZE	(TCP_HDR_SIZE + IPV6_HDR_SIZE)
#define TCP_TS_HDR_SIZE		(TCP_HDR_SIZE + TCP_TS_OPTION_SIZE)
#define MAX_TCP_HDR             64

#define OFFSET_TYPE_TO_TCP_HDR_SIZE(type) (TCP_HDR_SIZE+(type ? TCP_TS_OPTION_SIZE:0))

#endif
