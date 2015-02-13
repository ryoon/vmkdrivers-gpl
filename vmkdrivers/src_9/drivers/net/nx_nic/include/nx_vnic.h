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
#ifndef _NX_VNIC_H_
#define _NX_VNIC_H_

#define	VPROP_DATA_START	DEFAULT_DATA_START
#define	MAX_VNICS_PER_CARD	8	/* currently tied to PCIE functions */

#define	VPROP_ACTIVE	0xFADE
#define	VPROP_NOVLAN	0
#define	VPROP_UNITSIZE	8

enum vprop_types {
	VTYPE_BASIC = 0,
	VTYPE_LAST
};

/*
 * Single header at start of vNIC property area.
 */
struct vprop_header {
	unsigned short	vprop_magic;	/* Card ignores if not VPROP_ACTIVE */
	unsigned short	vprop_nvnics;	/* Total #vnics described */
	unsigned short	vprop_nblocks;	/* No. of parameter blocks */
	unsigned short	vprop_qbytes;	/* # 8 byte units in vprop area */
};

/*
 * Generic property descriptor for each parameter block.
 */
struct vprop_desc {
	unsigned short	vprop_bnum;	/* First vnic# described by block */
	unsigned short	vprop_bsize;	/* # 8 byte units in each element */
	unsigned char	vprop_type;	/* Property type */
	unsigned char	vprop_rsvd0;
	unsigned char	vprop_rsvd1;
	unsigned char	vprop_nactive;	/* No. of vnics in this block */
};

/*
 * vNIC basic property descriptor.
 */
struct vprop_basic {
	unsigned char	vprop_mac[6];	/* MAC address for vNIC */
	unsigned short	vprop_vlan;	/* VLAN tag, 0 for untagged */
	unsigned char	vprop_port;	/* Physical port to use for vNIC */
	unsigned char   vprop_pad1[3];
	unsigned short	vprop_txmax;	/* Tx max */
	unsigned short	vprop_txmin;	/* Tx min */
};

#endif /* _NX_VNIC_H_ */
