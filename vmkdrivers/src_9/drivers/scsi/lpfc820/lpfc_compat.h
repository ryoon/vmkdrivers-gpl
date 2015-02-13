/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2010 Emulex.  All rights reserved.           *
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

#include <asm/byteorder.h>

static inline void
lpfc_memcpy_to_slim(void __iomem *dest, void *src, unsigned int bytes)
{
	uint32_t __iomem *dest32;
	uint32_t *src32;
	unsigned int four_bytes;


	dest32  = (uint32_t __iomem *) dest;
	src32  = (uint32_t *) src;

	/* write input bytes, 4 bytes at a time */
	for (four_bytes = bytes /4; four_bytes > 0; four_bytes--) {
		writel( *src32, dest32);
		readl(dest32); /* flush */
		dest32++;
		src32++;
	}

	return;
}

static inline void
lpfc_memcpy_from_slim( void *dest, void __iomem *src, unsigned int bytes)
{
	uint32_t *dest32;
	uint32_t __iomem *src32;
	unsigned int four_bytes;


	dest32  = (uint32_t *) dest;
	src32  = (uint32_t __iomem *) src;

	/* read input bytes, 4 bytes at a time */
	for (four_bytes = bytes /4; four_bytes > 0; four_bytes--) {
		*dest32 = readl( src32);
		dest32++;
		src32++;
	}

	return;
}

#ifndef PTR_ALIGN
#define PTR_ALIGN(p, a)((typeof(p))ALIGN((unsigned long)(p), (a)))
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#endif
