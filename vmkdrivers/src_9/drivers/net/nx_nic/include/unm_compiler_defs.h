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
#ifndef __UNM_COMPILER_DEFS_H_
#define __UNM_COMPILER_DEFS_H_

#if defined(__GNUC__) /* gcc */

#if UNM_CONF_PROCESSOR==UNM_CONF_X86
#define	rarely(ARG)	ARG
#define	frequently(ARG)	ARG
#endif

#define PREALIGN(x)
#define POSTALIGN(x) __attribute__((aligned(x)))

#elif defined(_MSC_VER) /* windows and not gcc */

#define	rarely(ARG)	ARG
#define	frequently(ARG)	ARG
#define PREALIGN(x) __declspec(align(x))
#define POSTALIGN(x)
#define inline __inline

#else /* neither windows nor gcc */

#define	rarely(ARG)	ARG
#define	frequently(ARG)	ARG
#define PREALIGN(x)
#define POSTALIGN(x)

#endif	/* _GNUC_, _MSC_VER */

#endif /* __UNM_COMPILER_DEFS_H_ */
