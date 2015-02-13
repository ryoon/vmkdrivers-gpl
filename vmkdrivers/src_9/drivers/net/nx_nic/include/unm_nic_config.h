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
#ifndef _UNM_NIC_CONFIG_H_
#define _UNM_NIC_CONFIG_H_

/* NIC configuration */
#define NO_EPG_PROXY      1

#define HOST_CPU_64_BIT   1


#ifdef  PEGNET_NIC

#define UNM_HOST_MSG_TRANSPORT_NIC      1
#define UNM_HOST_MSG_TRANSPORT_QM       2

#define UNM_HOST_MSG_TRANSPORT          UNM_HOST_MSG_TRANSPORT_NIC

/*
 * The host to peg message passing interface.
 */
#define UNM_H2P_TRANSPORT_NIC           1
#define UNM_H2P_TRANSPORT_QM            2

#define UNM_H2P_TRANSPORT               UNM_H2P_TRANSPORT_NIC

#endif  /* PEGNET_NIC */

/* Define to enable native jumbo frame support. */
#define NX_BIG_SRE

/*
 * Work around to account for the fact that p3-a0 cutthrough dma's a 
 * 32 byte L2 header to the host.  The "true" L2 header resides at 
 * the end of this field, next to the subsequent L3 header.
 *
 * Though this is a cut through only feature leave it outside of cut through
 * #define for the unified driver to work.
 */
#define EXTRA_L2_HEADER

#ifdef P3_NIC_CUT_THRU
/* No sre buffers for cut-thru. */
#undef NX_BIG_SRE
#endif

#endif  /* _UNM_NIC_CONFIG_H_ */

