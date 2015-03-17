/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
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
 */
/*
 * Message structures for communication to/from, and within, our chip.
 */
#ifndef __MESSAGE_H
#define __MESSAGE_H

/*
 * A single word in a message.
 */
#ifdef SOLARIS
#include"solaris.h"
#endif
typedef __uint64_t	unm_msgword_t;

#define NX_QM_ENQ_TAIL	0x0  /* enqueue at end of queue */
#define NX_QM_ENQ_HEAD	0x1  /* enqueue at front of queue */

/*
 * We can use raw format (as the hardware defines it) or unpacked
 * (normal C struct's) for processing at the endpoints of a message,
 * but all transmission and/or reception must be done in the raw format.
 */
typedef union {
    struct {
	unm_msgword_t
		dst_minor:18,	/* Minor queue number (0-256K) */
		dst_subq:1,	/* Use minor number (1) or not (0) */
		dst_major:4,	/* Major queue number (0-15) */
		dst_side:1,	/* Which side of chip: net(0)/md(1) */
		dst_c2cport:4,	/* Which C2C port to use (0 for local) */
		src_minor:18,	/* Minor queue number (0-256K) */
		src_subq:1,	/* Use minor number (1) or not (0) */
		src_major:4,	/* Major queue number (0-15) */
		src_side:1,	/* Which side of chip: net(0)/md(1) */
		src_c2cport:4,	/* Which C2C port to use (0 for local) */
		hw_opcode:2,	/* Enqueue at head or tail */
		type:6;		/* Overall type of message (0-64) */
    } ;
    __uint64_t  word ;
} unm_msg_hdr_t;
typedef struct {
	int	type;		/* Overall type of message (0-64) */
	int	src_c2cport;	/* Which C2C port to use (0 for local) */
	int	src_side;	/* Which side of chip: net(0)/md(1) */
	int	src_major;	/* Major queue number (0-15) */
	int	src_subq;	/* Use minor number (1) or not (0) */
	int	src_minor;	/* Minor queue number (0-256K) */
	int	dst_c2cport;	/* Which C2C port to use (0 for local) */
	int	dst_side;	/* Which side of chip: net(0)/md(1) */
	int	dst_major;	/* Major queue number (0-15) */
	int	dst_subq;	/* Use minor number (1) or not (0) */
	int	dst_minor;	/* Minor queue number (0-256K) */
} unm_msg_hdr_easy_t;

/*
 * A convenience structure for the body of a message.
 */
typedef struct {
	unm_msgword_t values[7];/* just the payload of the message */
} unm_msg_body_t;

/*
 * A convenience structure a message as a whole (in hardware native format).
 */
typedef struct {
	unm_msg_hdr_t	hdr;
	unm_msg_body_t	body;
} unm_msg_t;

typedef struct {
        unsigned int    major:4,
                        subq:1,
                        minor:18;
} nx_qaddr_t;

#define	UNM_MESSAGE_SIZE	64	/* number bytes in a message */

/*
 * Convenience macro to fill in the src/dst of a message.
 */
#define	UNM_MSGQ_ADDR(HDR,WHICH,S,M,U,N)	\
	((HDR).WHICH##_c2cport = 0,	(HDR).WHICH##_side = (S),	\
	 (HDR).WHICH##_major = (M),	(HDR).WHICH##_subq = (U),	\
	 (HDR).WHICH##_minor = (N))

#endif /* __MESSAGE_H */
