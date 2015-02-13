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
 *
 */
#ifndef	_NX_TYPES_H_
#define _NX_TYPES_H_

/* #if defined(__KERNEL__) || defined(_unmpeg) */
/* typedef char                    __int8_t; */
/* typedef short                   __int16_t; */
/* typedef int                     __int32_t; */
/* typedef long long               __int64_t; */
/* typedef unsigned char           __uint8_t; */
/* typedef unsigned short          __uint16_t; */
/* typedef unsigned int            __uint32_t; */
/* typedef unsigned long long      __uint64_t; */

/* #endif */

/*
 * The IP address field used in messages.
 */
#define	NX_IP_VERSION_V4	4
#define	NX_IP_VERSION_V6	6
typedef	union {
	__uint32_t	v4;
	__uint32_t	v6[4];
} ip_addr_t;

#endif /* _NX_TYPES_H_ */
