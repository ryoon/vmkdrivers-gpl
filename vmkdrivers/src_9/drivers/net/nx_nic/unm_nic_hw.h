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
 * Structures, enums, and macros for the MAC
 */

#ifndef _UNM_NIC_HW_
#define _UNM_NIC_HW_

#include <unm_inc.h>
#include <asm/io.h>

/* Hardware memory size of 128 meg */
#define BAR0_SIZE (128 * 1024 * 1024)
/*
It can be calculated by looking at the first 1 bit of the BAR0 addr after bit 4
#For us lets assume that BAR0 is D8000008, then the size is 0x8000000, 8 represents
#first bit containing 1.   FSL temp notes....pg 162 of PCI systems arch...
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define __iomem
#endif

#ifndef readq
static inline __uint64_t readq(void __iomem *addr)
{
        return readl(addr) |
               (((__uint64_t)readl(addr+4)) << 32LL);
}
#endif

#ifndef writeq
static inline void writeq(__uint64_t val, void __iomem *addr)
{
        writel(((__uint32_t)(val)), (addr));
        writel(((__uint32_t)(val >> 32)), (addr + 4));
}
#endif

/*
 * Following macros require the mapped addresses to access
 * the Phantom memory.
 */
#define UNM_NIC_PCI_READ_8(ADDR)         readb((ADDR))
#define UNM_NIC_PCI_READ_16(ADDR)        readw((ADDR))
#define UNM_NIC_PCI_READ_32(ADDR)        readl((ADDR))
#define UNM_NIC_PCI_READ_64(ADDR)        readq((ADDR))

#define UNM_NIC_PCI_WRITE_8(DATA, ADDR)  writeb(DATA, (ADDR))
#define UNM_NIC_PCI_WRITE_16(DATA, ADDR) writew(DATA, (ADDR))
#define UNM_NIC_PCI_WRITE_32(DATA, ADDR) writel(DATA, (ADDR))
#define UNM_NIC_PCI_WRITE_64(DATA, ADDR) writeq(DATA, (ADDR))

#define UNM_NIC_HW_BLOCK_WRITE_64(DATA_PTR, ADDR, NUM_WORDS)        \
        do {                                                        \
                int i;                                              \
                u64 *a = (u64 *) (DATA_PTR);                        \
                u64 *b = (u64 *) (ADDR);                            \
                for (i=0; i< (NUM_WORDS); i++) {                    \
                        writeq(readq(a), b);                        \
                        b++;                                        \
                        a++;                                        \
                }                                                   \
        } while (0)

#define UNM_NIC_HW_BLOCK_READ_64(DATA_PTR, ADDR, NUM_WORDS)           \
        do {                                                          \
                int i;                                                \
                u64 *a = (u64 *) (DATA_PTR);                          \
                u64 *b = (u64 *) (ADDR);                              \
                for (i=0; i< (NUM_WORDS); i++) {                      \
                        writeq(readq(b), a);                          \
                        b++;                                          \
                        a++;                                          \
                }                                                     \
        } while (0)

#define UNM_DEBUG_PVT_32_ADDR(A)                   \
        (unsigned int)(A)
#define UNM_DEBUG_PVT_32_VALUE(V)                  \
        *((unsigned int *)(V))
#define UNM_DEBUG_PVT_32_VALUE_LIT(V)              \
        (unsigned int)(V)
#define UNM_DEBUG_PVT_64_ADDR_LO(A)                \
        (unsigned int)((unsigned long)(A) & 0xffffffff)
#define UNM_DEBUG_PVT_64_ADDR_HI(A)                \
        (unsigned int)((((unsigned long)(A) >> 16) >> 16) & 0xffffffff)
#define UNM_DEBUG_PVT_64_VALUE_LO(V)               \
        (unsigned int)(*(__uint64_t *)(V) & 0xffffffff)
#define UNM_DEBUG_PVT_64_VALUE_HI(V)               \
        (unsigned int)(((*(__uint64_t *)(V) >> 16) >> 16) & 0xffffffff)
#define UNM_DEBUG_PVT_64_VALUE_LIT_LO(LV)          \
        (unsigned int)((LV) & 0xffffffff)
#define UNM_DEBUG_PVT_64_VALUE_LIT_HI(LV)          \
        (unsigned int)(((LV) >> 32) & 0xffffffff)

#define UNM_PCI_MAPSIZE_BYTES  (UNM_PCI_MAPSIZE << 20)

#define UNM_NIC_LOCKED_READ_REG(X, Y)   \
        addr = (void *)(pci_base_offset(adapter, (X)));     \
        *(uint32_t *)(Y) = UNM_NIC_PCI_READ_32(addr);

#define UNM_NIC_LOCKED_WRITE_REG(X, Y)   \
	addr = (void *)(pci_base_offset(adapter, (X))); \
	UNM_NIC_PCI_WRITE_32(*(uint32_t *)(Y), addr);

/* For Multicard support */

struct unm_adapter_s;
void unm_nic_set_link_parameters(struct unm_adapter_s *adapter);
long xge_mdio_init(struct unm_adapter_s *adapter);
void unm_nic_flash_print(struct unm_adapter_s* adapter);
void unm_nic_get_serial_num(struct unm_adapter_s *adapter);

typedef struct {
	unsigned valid;
	unsigned start_128M;
	unsigned end_128M;
	unsigned start_2M;
} crb_128M_2M_sub_block_map_t;

typedef struct {
	crb_128M_2M_sub_block_map_t sub_block[16];
} crb_128M_2M_block_map_t;


#endif /* _UNM_NIC_HW_ */
