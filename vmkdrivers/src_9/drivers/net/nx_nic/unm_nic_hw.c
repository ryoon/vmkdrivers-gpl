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
 * Source file for NIC routines to access the Phantom hardware
 *
 * $Id: //depot/vmkdrivers/vsphere67u3/src_9/drivers/net/nx_nic/unm_nic_hw.c#1 $
 *
 */
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/pci.h>
#include "unm_nic.h"
#include "unm_nic_hw.h"
#include "nic_cmn.h"
#include "unm_version.h"
#include "unm_brdcfg.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"

#define MASK(n)			((1ULL<<(n))-1)
#define MN_WIN(addr) (((addr & 0x1fc0000) >> 1) | ((addr >> 25) & 0x3ff))
//#define OCM_WIN(addr) MN_WIN(addr)
#define OCM_WIN(addr) (((addr & 0x1ff0000) >> 1) | ((addr >> 25) & 0x3ff))  // 64K?
#define MS_WIN(addr) (addr & 0x0ffc0000)
#define UNM_PCI_MN_2M   (0)
#define UNM_PCI_MS_2M   (0x80000)
#define UNM_PCI_OCM0_2M (0xc0000)
#define VALID_OCM_ADDR(addr) (((addr) & 0x3f800) != 0x3f800)
#define GET_MEM_OFFS_2M(addr) (addr & MASK(18))

#define CRB_BLK(off)	((off >> 20) & 0x3f)
#define CRB_SUBBLK(off)	((off >> 16) & 0xf)
#define CRB_WINDOW_2M	(0x130060)
#define UNM_PCI_CAMQM_2M_END	(0x04800800UL)
#define CRB_HI(off)	((crb_hub_agt[CRB_BLK(off)] << 20) | ((off) & 0xf0000))
#define UNM_PCI_CAMQM_2M_BASE	(0x000ff800UL)
#define CRB_INDIRECT_2M	(0x1e0000UL)


crb_128M_2M_block_map_t crb_128M_2M_map[64] = {
    {{{0, 0,         0,         0}}},		/* 0: PCI */
    {{{1, 0x0100000, 0x0102000, 0x120000},	/* 1: PCIE */
	  {1, 0x0110000, 0x0120000, 0x130000},
	  {1, 0x0120000, 0x0122000, 0x124000},
	  {1, 0x0130000, 0x0132000, 0x126000},
	  {1, 0x0140000, 0x0142000, 0x128000},
	  {1, 0x0150000, 0x0152000, 0x12a000},
	  {1, 0x0160000, 0x0170000, 0x110000},
	  {1, 0x0170000, 0x0172000, 0x12e000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {1, 0x01e0000, 0x01e0800, 0x122000},
	  {0, 0x0000000, 0x0000000, 0x000000}}},
	{{{1, 0x0200000, 0x0210000, 0x180000}}},/* 2: MN */
    {{{0, 0,         0,         0}}},	    /* 3: */
    {{{1, 0x0400000, 0x0401000, 0x169000}}},/* 4: P2NR1 */
    {{{1, 0x0500000, 0x0510000, 0x140000}}},/* 5: SRE   */
    {{{1, 0x0600000, 0x0610000, 0x1c0000}}},/* 6: NIU   */
    {{{1, 0x0700000, 0x0704000, 0x1b8000}}},/* 7: QM    */
    {{{1, 0x0800000, 0x0802000, 0x170000},  /* 8: SQM0  */
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {1, 0x08f0000, 0x08f2000, 0x172000}}},
    {{{1, 0x0900000, 0x0902000, 0x174000},	/* 9: SQM1*/
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {1, 0x09f0000, 0x09f2000, 0x176000}}},
    {{{0, 0x0a00000, 0x0a02000, 0x178000},	/* 10: SQM2*/
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {1, 0x0af0000, 0x0af2000, 0x17a000}}},
    {{{0, 0x0b00000, 0x0b02000, 0x17c000},	/* 11: SQM3*/
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {0, 0x0000000, 0x0000000, 0x000000},
      {1, 0x0bf0000, 0x0bf2000, 0x17e000}}},
	{{{1, 0x0c00000, 0x0c04000, 0x1d4000}}},/* 12: I2Q */
	{{{1, 0x0d00000, 0x0d04000, 0x1a4000}}},/* 13: TMR */
	{{{1, 0x0e00000, 0x0e04000, 0x1a0000}}},/* 14: ROMUSB */
	{{{1, 0x0f00000, 0x0f01000, 0x164000}}},/* 15: PEG4 */
	{{{0, 0x1000000, 0x1004000, 0x1a8000}}},/* 16: XDMA */
	{{{1, 0x1100000, 0x1101000, 0x160000}}},/* 17: PEG0 */
	{{{1, 0x1200000, 0x1201000, 0x161000}}},/* 18: PEG1 */
	{{{1, 0x1300000, 0x1301000, 0x162000}}},/* 19: PEG2 */
	{{{1, 0x1400000, 0x1401000, 0x163000}}},/* 20: PEG3 */
	{{{1, 0x1500000, 0x1501000, 0x165000}}},/* 21: P2ND */
	{{{1, 0x1600000, 0x1601000, 0x166000}}},/* 22: P2NI */
	{{{0, 0,         0,         0}}},	/* 23: */
	{{{0, 0,         0,         0}}},	/* 24: */
	{{{0, 0,         0,         0}}},	/* 25: */
	{{{0, 0,         0,         0}}},	/* 26: */
	{{{0, 0,         0,         0}}},	/* 27: */
	{{{0, 0,         0,         0}}},	/* 28: */
	{{{1, 0x1d00000, 0x1d10000, 0x190000}}},/* 29: MS */
    {{{1, 0x1e00000, 0x1e01000, 0x16a000}}},/* 30: P2NR2 */
    {{{1, 0x1f00000, 0x1f10000, 0x150000}}},/* 31: EPG */
	{{{0}}},				/* 32: PCI */
	{{{1, 0x2100000, 0x2102000, 0x120000},	/* 33: PCIE */
	  {1, 0x2110000, 0x2120000, 0x130000},
	  {1, 0x2120000, 0x2122000, 0x124000},
	  {1, 0x2130000, 0x2132000, 0x126000},
	  {1, 0x2140000, 0x2142000, 0x128000},
	  {1, 0x2150000, 0x2152000, 0x12a000},
	  {1, 0x2160000, 0x2170000, 0x110000},
	  {1, 0x2170000, 0x2172000, 0x12e000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000},
	  {0, 0x0000000, 0x0000000, 0x000000}}},
	{{{1, 0x2200000, 0x2204000, 0x1b0000}}},/* 34: CAM */
	{{{0}}},				/* 35: */
	{{{0}}},				/* 36: */
	{{{0}}},				/* 37: */
	{{{0}}},				/* 38: */
	{{{0}}},				/* 39: */
	{{{1, 0x2800000, 0x2804000, 0x1a4000}}},/* 40: TMR */
	{{{1, 0x2900000, 0x2901000, 0x16b000}}},/* 41: P2NR3 */
	{{{1, 0x2a00000, 0x2a00400, 0x1ac400}}},/* 42: RPMX1 */
	{{{1, 0x2b00000, 0x2b00400, 0x1ac800}}},/* 43: RPMX2 */
	{{{1, 0x2c00000, 0x2c00400, 0x1acc00}}},/* 44: RPMX3 */
	{{{1, 0x2d00000, 0x2d00400, 0x1ad000}}},/* 45: RPMX4 */
	{{{1, 0x2e00000, 0x2e00400, 0x1ad400}}},/* 46: RPMX5 */
	{{{1, 0x2f00000, 0x2f00400, 0x1ad800}}},/* 47: RPMX6 */
	{{{1, 0x3000000, 0x3000400, 0x1adc00}}},/* 48: RPMX7 */
	{{{0, 0x3100000, 0x3104000, 0x1a8000}}},/* 49: XDMA */
	{{{1, 0x3200000, 0x3204000, 0x1d4000}}},/* 50: I2Q */
	{{{1, 0x3300000, 0x3304000, 0x1a0000}}},/* 51: ROMUSB */
	{{{0}}},				/* 52: */
	{{{1, 0x3500000, 0x3500400, 0x1ac000}}},/* 53: RPMX0 */
	{{{1, 0x3600000, 0x3600400, 0x1ae000}}},/* 54: RPMX8 */
	{{{1, 0x3700000, 0x3700400, 0x1ae400}}},/* 55: RPMX9 */
	{{{1, 0x3800000, 0x3804000, 0x1d0000}}},/* 56: OCM0 */
	{{{1, 0x3900000, 0x3904000, 0x1b4000}}},/* 57: CRYPTO */
	{{{1, 0x3a00000, 0x3a04000, 0x1d8000}}},/* 58: SMB */
	{{{0}}},				/* 59: I2C0 */
	{{{0}}},				/* 60: I2C1 */
	{{{1, 0x3d00000, 0x3d04000, 0x1dc000}}},/* 61: LPC */
	{{{1, 0x3e00000, 0x3e01000, 0x167000}}},/* 62: P2NC */
	{{{1, 0x3f00000, 0x3f01000, 0x168000}}}	/* 63: P2NR0 */
};

/*
 * top 12 bits of crb internal address (hub, agent)
 */
unsigned crb_hub_agt[64] =
{
	0,
	UNM_HW_CRB_HUB_AGT_ADR_PS,
	UNM_HW_CRB_HUB_AGT_ADR_MN,
	UNM_HW_CRB_HUB_AGT_ADR_MS,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_SRE,
	UNM_HW_CRB_HUB_AGT_ADR_NIU,
	UNM_HW_CRB_HUB_AGT_ADR_QMN,
	UNM_HW_CRB_HUB_AGT_ADR_SQN0,
	UNM_HW_CRB_HUB_AGT_ADR_SQN1,
	UNM_HW_CRB_HUB_AGT_ADR_SQN2,
	UNM_HW_CRB_HUB_AGT_ADR_SQN3,
	UNM_HW_CRB_HUB_AGT_ADR_I2Q,
	UNM_HW_CRB_HUB_AGT_ADR_TIMR,
	UNM_HW_CRB_HUB_AGT_ADR_ROMUSB,
	UNM_HW_CRB_HUB_AGT_ADR_PGN4,
	UNM_HW_CRB_HUB_AGT_ADR_XDMA,
	UNM_HW_CRB_HUB_AGT_ADR_PGN0,
	UNM_HW_CRB_HUB_AGT_ADR_PGN1,
	UNM_HW_CRB_HUB_AGT_ADR_PGN2,
	UNM_HW_CRB_HUB_AGT_ADR_PGN3,
	UNM_HW_CRB_HUB_AGT_ADR_PGND,
	UNM_HW_CRB_HUB_AGT_ADR_PGNI,
	UNM_HW_CRB_HUB_AGT_ADR_PGS0,
	UNM_HW_CRB_HUB_AGT_ADR_PGS1,
	UNM_HW_CRB_HUB_AGT_ADR_PGS2,
	UNM_HW_CRB_HUB_AGT_ADR_PGS3,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_PGSI,
	UNM_HW_CRB_HUB_AGT_ADR_SN,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_EG,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_PS,
	UNM_HW_CRB_HUB_AGT_ADR_CAM,
	0,
	0,
	0,
	0,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_TIMR,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX1,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX2,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX3,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX4,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX5,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX6,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX7,
	UNM_HW_CRB_HUB_AGT_ADR_XDMA,
	UNM_HW_CRB_HUB_AGT_ADR_I2Q,
	UNM_HW_CRB_HUB_AGT_ADR_ROMUSB,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX0,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX8,
	UNM_HW_CRB_HUB_AGT_ADR_RPMX9,
	UNM_HW_CRB_HUB_AGT_ADR_OCM0,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_SMB,
	UNM_HW_CRB_HUB_AGT_ADR_I2C0,
	UNM_HW_CRB_HUB_AGT_ADR_I2C1,
	0,
	UNM_HW_CRB_HUB_AGT_ADR_PGNC,
	0,
};


#define CRB_WIN_LOCK_TIMEOUT 100000000

int crb_win_lock(struct unm_adapter_s *adapter)
{
    int i;
    int done = 0, timeout = 0;

    while (!done) {
        /* acquire semaphore3 from PCI HW block */
        done = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM7_LOCK));
        if (done == 1)
            break;
        if (timeout >= CRB_WIN_LOCK_TIMEOUT) {
            return -1;
        }
        timeout++;
        /*
         * Yield CPU
         */
        if(!in_atomic())
                schedule();
        else {
                for(i = 0; i < 20; i++)
                        cpu_relax();    /*This a nop instr on i386*/
        }
    }
    NXWR32(adapter, UNM_CRB_WIN_LOCK_ID, adapter->portnum);
    return 0;
}

void crb_win_unlock(struct unm_adapter_s *adapter)
{
    int val;

    val = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM7_UNLOCK));
}
/*
 * Changes the CRB window to the specified window.
 */
void
unm_nic_pci_change_crbwindow_128M(unm_adapter *adapter, uint32_t wndw)
{
        unm_pcix_crb_window_t        window;
        unsigned long                offset;
        uint32_t                     tmp;

        if (adapter->curr_window == wndw) {
                return;
        }

        /*
         * Move the CRB window.
         * We need to write to the "direct access" region of PCI
         * to avoid a race condition where the window register has
         * not been successfully written across CRB before the target
         * register address is received by PCI. The direct region bypasses
         * the CRB bus.
         */
        offset = PCI_OFFSET_SECOND_RANGE(adapter,
                  UNM_PCIX_PH_REG(PCIE_CRB_WINDOW_REG(adapter->ahw.pci_func)));

        *(unm_crbword_t *)&window = 0;
        window.addrbit = wndw;
        UNM_NIC_PCI_WRITE_32(*(unsigned int *)&window, (void*)(offset));
        /* MUST make sure window is set before we forge on... */
        while ((tmp = UNM_NIC_PCI_READ_32((void*)offset)) !=
            *(uint32_t *)&window) {
                printk(KERN_WARNING "%s: %s WARNING: CRB window value not "
                                    "registered properly: 0x%08x.\n",
                                    unm_nic_driver_name, __FUNCTION__, tmp);
        }

        adapter->curr_window = wndw;
        return;
}

/*
 * Return -1 if off is not valid,
 *	 1 if window access is needed. 'off' is set to offset from
 *	   CRB space in 128M pci map
 *	 0 if no window access is needed. 'off' is set to 2M addr
 * In: 'off' is offset from base in 128M pci map
 */
int
unm_nic_pci_get_crb_addr_2M(unm_adapter *adapter, u64 *off, int len)
{
	unsigned long end = *off + len;
	crb_128M_2M_sub_block_map_t *m;


	if (*off >= UNM_CRB_MAX)
		return -1;
    	
	if (*off >= UNM_PCI_CAMQM && (end <= UNM_PCI_CAMQM_2M_END)) {
	    	*off = (*off - UNM_PCI_CAMQM) + UNM_PCI_CAMQM_2M_BASE +
					adapter->ahw.pci_base0;
		return 0;
	}

	if (*off < UNM_PCI_CRBSPACE)
		return -1;

	*off -= UNM_PCI_CRBSPACE;
	end = *off + len;
	/*
	 * Try direct map
	 */
	
	m = &crb_128M_2M_map[CRB_BLK(*off)].sub_block[CRB_SUBBLK(*off)];

	if (m->valid && (m->start_128M <= *off) && (m->end_128M >= end)) {
	    	*off = *off + m->start_2M - m->start_128M + adapter->ahw.pci_base0;
//            printk("off = %016llX  pcibase0:%08lX\n", *off, adapter->ahw.pci_base0);
		return 0;
	}

	/*
	 * Not in direct map, use crb window
	 */
	return 1;
}
/*
 * In: 'off' is offset from CRB space in 128M pci map
 * Out: 'off' is 2M pci map addr
 * side effect: lock crb window
 */
static void
unm_nic_pci_set_crbwindow_2M(unm_adapter *adapter, u64 *off)
{
        u32 win_read;

	adapter->crb_win = CRB_HI(*off);
	UNM_NIC_PCI_WRITE_32(adapter->crb_win, (void *)(CRB_WINDOW_2M +
			                adapter->ahw.pci_base0));
        /* Read back value to make sure write has gone through before trying
         * to use it. */
        win_read = UNM_NIC_PCI_READ_32(
                        (void *)(CRB_WINDOW_2M + adapter->ahw.pci_base0));
        if (win_read != adapter->crb_win) {
                printk("%s: Written crbwin (0x%x) != Read crbwin (0x%x), off=0x%llx\n",
                    __FUNCTION__, adapter->crb_win, win_read, *off);
        }
	*off = (*off & MASK(16)) + CRB_INDIRECT_2M + adapter->ahw.pci_base0;
}

/*
 * Set the CRB window based on the offset.
 * Return 0 if successful; 1 otherwise
 */
static inline unsigned long
unm_nic_pci_set_crbwindow(unm_adapter *adapter, u64 off)
{
        /*
         * See if we are currently pointing to the region we want to use next.
         */
        if ((off >= UNM_CRB_PCIX_HOST) && (off < UNM_CRB_DDR_NET)) {
                /*
                 * No need to change window. PCIX and PCIE regs are in both
                 * windows.
                 */
                return (off);
        }

        if ((off >= UNM_CRB_PCIX_HOST) && (off < UNM_CRB_PCIX_HOST2)) {
                /* We are in first CRB window */
                if (adapter->curr_window != 0) {
			unm_nic_pci_change_crbwindow_128M(adapter, 0);
                }
                return (off);
        }

        if ((off > UNM_CRB_PCIX_HOST2) && (off < UNM_CRB_MAX)) {
                /* We are in second CRB window */
                off = off - UNM_CRB_PCIX_HOST2 + UNM_CRB_PCIX_HOST;

                if (adapter->curr_window != 1) {
			unm_nic_pci_change_crbwindow_128M(adapter, 1);
                }
                return (off);
        }

        if ((off >= UNM_PCI_DIRECT_CRB) && (off < UNM_PCI_CAMQM_MAX)) {
                /*
                 * We are in the QM or direct access register region - do
                 * nothing
                 */
                return (off);
        }

        /* strange address given */
        dump_stack();
        printk(KERN_WARNING"%s: Warning: unm_nic_pci_set_crbwindow called with"
                " an unknown address(%llx)\n", unm_nic_driver_name, off);

        return (off);
}


int
unm_nic_hw_read_ioctl_2M(unm_adapter *adapter, u64 off, void *data, int len)
{
	unsigned long flags = 0;
	int rv;

	rv = unm_nic_pci_get_crb_addr_2M(adapter, &off, len);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&adapter->adapter_lock, flags);
		crb_win_lock(adapter);
		unm_nic_pci_set_crbwindow_2M(adapter, &off);
	}

	DPRINTK(1, INFO, "read from offset %llx, len=%d\n", off, len);

	switch (len) {
	case 1:
		*(__uint8_t  *)data = UNM_NIC_PCI_READ_8((void *)(uptr_t)off);
		break;
	case 2:
		*(__uint16_t *)data = UNM_NIC_PCI_READ_16((void *)(uptr_t)off);
		break;
	case 4:
		*(__uint32_t *)data = UNM_NIC_PCI_READ_32((void *)(uptr_t)off);
		break;
	case 8:
		*(__uint64_t *)data = UNM_NIC_PCI_READ_64((void *)(uptr_t)off);
		break;
	default:
#if !defined(NDEBUG)
		if ((len & 0x7) != 0)
			printk("%s: %s len(%d) not multiple of 8.\n",
				unm_nic_driver_name, __FUNCTION__, len);
#endif
		UNM_NIC_HW_BLOCK_READ_64(data, (void *)(uptr_t)off, (len>>3));
		break;
	}

	DPRINTK(1, INFO, "read %lx\n", *(unsigned long *)data);

	if (rv == 1) {
    	        crb_win_unlock(adapter);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
	}

	return 0;
}

int
unm_nic_hw_write_ioctl_2M(unm_adapter *adapter, u64 off, void *data, int len)
{
	unsigned long flags = 0;
	int rv;

	rv = unm_nic_pci_get_crb_addr_2M(adapter, &off, len);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&adapter->adapter_lock, flags);
		crb_win_lock(adapter);
		unm_nic_pci_set_crbwindow_2M(adapter, &off);
	}

	DPRINTK(1, INFO, "write data %lx to offset %llx, len=%d\n",
		*(unsigned long *)data, off, len);

	switch (len) {
	case 1:
		UNM_NIC_PCI_WRITE_8(*(__uint8_t *)data, (void *)(uptr_t)off);
		break;
	case 2:
		UNM_NIC_PCI_WRITE_16(*(__uint16_t *)data,(void *)(uptr_t)off);
		break;
	case 4:
		UNM_NIC_PCI_WRITE_32(*(__uint32_t *)data, (void *)(uptr_t)off);
		break;
	case 8:
		UNM_NIC_PCI_WRITE_64(*(__uint64_t *)data, (void *)(uptr_t)off);
		break;
	default:
#if !defined(NDEBUG)
                if ((len & 0x7) != 0)
                        printk("%s: %s  len(%d) not multiple of 8.\n",
                                unm_nic_driver_name, __FUNCTION__, len);
#endif
                DPRINTK(1, INFO, "writing data %lx to offset %llx, num words=%d\n",
                            *(unsigned long *)data, off, (len>>3));

		UNM_NIC_HW_BLOCK_WRITE_64(data, (uptr_t)off, (len>>3));
                break;
        }
	if (rv == 1) {
    	        crb_win_unlock(adapter);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
        }

        return 0;
}

int
unm_nic_hw_write_ioctl_128M(unm_adapter *adapter, u64 off, void *data, int len)
{
	void		*addr;
	unsigned long	flags=0;
	u64		offset = off;
	uint8_t		*mem_ptr = NULL;
	unsigned long	mem_base;
	unsigned long	mem_page;

	if(ADDR_IN_WINDOW1(off)) {//Window 1
		addr = CRB_NORMALIZE(adapter, off);
		if(!addr) {
			mem_base = pci_resource_start(adapter->ahw.pdev, 0);
			offset = CRB_NORMAL(off);
			if (adapter->ahw.pci_len0 == 0)
			    offset -= UNM_PCI_CRBSPACE;

			mem_page = offset & PAGE_MASK;
			if(mem_page != ((offset + len - 1) & PAGE_MASK))
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
			else
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
			if(mem_ptr == 0UL) {
				return 1;
			}
			addr = mem_ptr;
			addr += offset & (PAGE_SIZE - 1);
		}
		read_lock(&adapter->adapter_lock);
	} else {//Window 0
		addr = (void *)(uptr_t)(pci_base_offset(adapter, off));
		if(!addr) {
			mem_base = pci_resource_start(adapter->ahw.pdev, 0);
			if (adapter->ahw.pci_len0 == 0)
			    off -= UNM_PCI_CRBSPACE;
			mem_page = off & PAGE_MASK;
			if(mem_page != ((off + len - 1) & PAGE_MASK))
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
			else
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
			if(mem_ptr == 0UL) {
				return 1;
			}
			addr = mem_ptr;
			addr += off & (PAGE_SIZE - 1);
		}
		write_lock_irqsave(&adapter->adapter_lock, flags);
		unm_nic_pci_change_crbwindow_128M(adapter, 0);
	}

	DPRINTK(1, INFO, "writing to base %lx offset %llx addr %p"
			" data %llx len %d\n",
			pci_base(adapter, off), off, addr,
			*(unsigned long long *)data, len);
	switch (len) {
       	case 1:
		UNM_NIC_PCI_WRITE_8 (*(__uint8_t *)data, addr);
		break;
	case 2:
		UNM_NIC_PCI_WRITE_16 (*(__uint16_t *)data, addr);
		break;
	case 4:
		UNM_NIC_PCI_WRITE_32 (*(__uint32_t *)data, addr);
		break;
	case 8:
		UNM_NIC_PCI_WRITE_64 (*(__uint64_t *)data, addr);
		break;
	default:
#if !defined(NDEBUG)
		if ((len & 0x7) != 0)
			printk("%s: %s len(%d) not multiple of 8.\n",
				unm_nic_driver_name, __FUNCTION__, len);
#endif
		DPRINTK(1, INFO, "writing data %lx to offset %llx, num words=%d\n",
			*(unsigned long *)data, off, (len>>3));

		UNM_NIC_HW_BLOCK_WRITE_64(data, addr, (len>>3));
		break;
	}
	if(ADDR_IN_WINDOW1(off)) {//Window 1
		read_unlock(&adapter->adapter_lock);
	} else {//Window 0
		unm_nic_pci_change_crbwindow_128M(adapter, 1);
       		write_unlock_irqrestore(&adapter->adapter_lock, flags);
	}

	if(mem_ptr)
		iounmap(mem_ptr);
	return 0;
}

int
unm_nic_hw_write_wx_128M(unm_adapter *adapter, u64 off, u32 data)
{
        void *addr;
        unsigned long        flags = 0;


        if(ADDR_IN_WINDOW1(off))
        {//Window 1
                addr = CRB_NORMALIZE(adapter, off);
                read_lock(&adapter->adapter_lock);
        }
        else{//Window 0
                addr = (void *)(uptr_t)(pci_base_offset(adapter, off));
                write_lock_irqsave(&adapter->adapter_lock, flags);
		        unm_nic_pci_change_crbwindow_128M(adapter, 0);
        }


        DPRINTK(1, INFO, "writing to base %lx offset %llx addr %p"
                            " data %llx \n",
                            pci_base(adapter, off), off, addr,
                            data);
	if(!addr) {
		if(ADDR_IN_WINDOW1(off)) {//Window 1
			read_unlock(&adapter->adapter_lock);
		} else {//Window 0
			unm_nic_pci_change_crbwindow_128M(adapter, 1);
			write_unlock_irqrestore(&adapter->adapter_lock, flags);
		}
		return 1;
	}

	UNM_NIC_PCI_WRITE_32 (data, addr);

	if(ADDR_IN_WINDOW1(off))
	{//Window 1
		read_unlock(&adapter->adapter_lock);
	}
	else{//Window 0
		unm_nic_pci_change_crbwindow_128M(adapter, 1);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
	}

	return 0;
}

/*
 * Note : only 32-bit writes!
 */
int unm_nic_pci_write_immediate_128M(unm_adapter *adapter, u64 off, u32 *data)
{
    UNM_NIC_PCI_WRITE_32(*data, (void *)(uptr_t)(PCI_OFFSET_SECOND_RANGE(adapter, off)));
	return 0;
}

/*
 * Note : only 32-bit reads!
 */
int unm_nic_pci_read_immediate_128M(unm_adapter *adapter, u64 off, u32 *data)
{
	*data = UNM_NIC_PCI_READ_32((void *)(uptr_t)(pci_base_offset(adapter, off)));
	return 0;
}

/*
 * Note : only 32-bit writes!
 */
int unm_nic_pci_write_immediate_2M(unm_adapter *adapter, u64 off, u32 *data)
{
    NXWR32(adapter, off, *data);
    return 0;
}

/*
 * Note : only 32-bit reads!
 */
int unm_nic_pci_read_immediate_2M(unm_adapter *adapter, u64 off, u32 *data)
{
    u32 temp;

    temp = NXRD32(adapter, off);

    *data = temp;
    return 0;
}


/* MINIDUMP RELATED FUNCTONS
*/

int unm_nic_hw_write_bar_reg_2M(unm_adapter *adapter, u64 off, u32 data)
{
	if (off >= 0xC0000 && off < 0x100000) { /* OCM */
		UNM_NIC_PCI_WRITE_32(data, (void *)(uptr_t)(off + adapter->ahw.pci_base0));
	} else {
		unm_nic_hw_indirect_write_2M(adapter, off, data);
	}

        return 0;
}

u32 unm_nic_hw_read_bar_reg_2M(unm_adapter *adapter, u64 off)
{
	u32 data;

	if (off >= 0xC0000 && off < 0x100000) { /* OCM */
		data = UNM_NIC_PCI_READ_32((void *)(uptr_t)(off + adapter->ahw.pci_base0));
	} else {
		data = unm_nic_hw_indirect_read_2M(adapter, off);
	}

	return data;
}


/*
 * write cross hw window boundary is not supported
 * 'len' should be 4
 */
int
unm_nic_hw_indirect_write_2M(unm_adapter *adapter, u64 off, u32 data)
{
	unsigned long flags = 0;
	u32 win_read;
	u32 win_base;

	/* Take the lock */
	write_lock_irqsave(&adapter->adapter_lock, flags);
	crb_win_lock(adapter);

	/* Set the window base */
	win_base = off & 0xFFFF0000;

	UNM_NIC_PCI_WRITE_32(win_base, (void *)(CRB_WINDOW_2M +
                                        adapter->ahw.pci_base0));
        /* Read back value to make sure write has gone through before trying
         * to use it. */
        win_read = UNM_NIC_PCI_READ_32(
                        (void *)(CRB_WINDOW_2M + adapter->ahw.pci_base0));
        if (win_read != win_base) {
                printk("%s: Written crbwin (0x%x) != Read crbwin (0x%x), off=0x%llx\n",
                    __FUNCTION__, win_base, win_read, off);
        }

	/* Get bar offset */
        off = (off & 0x0000FFFF) + CRB_INDIRECT_2M + adapter->ahw.pci_base0;

	DPRINTK(1, INFO, "write data %lx to offset %llx, len=%d\n",
				data, off, len);

	/* Do the write */
	UNM_NIC_PCI_WRITE_32(data, (void *)(uptr_t)off);

	/* Release the lock */
   	crb_win_unlock(adapter);
	write_unlock_irqrestore(&adapter->adapter_lock, flags);

	return 0;
}

u32
unm_nic_hw_indirect_read_2M(unm_adapter *adapter, u64 off)
{
	unsigned long flags = 0;
	u32 data;
	u32 win_read;
	u32 win_base;

	/* Take the lock */
	write_lock_irqsave(&adapter->adapter_lock, flags);
	crb_win_lock(adapter);

	/* Set the window base */
	win_base = off & 0xFFFF0000;

	UNM_NIC_PCI_WRITE_32(win_base, (void *)(CRB_WINDOW_2M +
                                         adapter->ahw.pci_base0));
        /* Read back value to make sure write has gone through before trying
         * to use it. */
	win_read = UNM_NIC_PCI_READ_32(
                        (void *)(CRB_WINDOW_2M + adapter->ahw.pci_base0));
	if (win_read != win_base) {
                printk("%s: Written crbwin (0x%x) != Read crbwin (0x%x), off=0x%llx\n",
                    __FUNCTION__, win_base, win_read, off);
       }

	/* Get bar offset */
        off = (off & 0x0000FFFF) + CRB_INDIRECT_2M + adapter->ahw.pci_base0;

	DPRINTK(1, INFO, "read from offset %llx, len=%d\n", off, len);

	/* Do the read */
	data = UNM_NIC_PCI_READ_32((void *)(uptr_t)off);

	DPRINTK(1, INFO, "read %lx\n", *(unsigned long *)data);

	/* Release the lock */
   	crb_win_unlock(adapter);
	write_unlock_irqrestore(&adapter->adapter_lock, flags);

	return data;
}


/*
 * write cross hw window boundary is not supported
 * 'len' should be 4
 */
int
unm_nic_hw_write_wx_2M(unm_adapter *adapter, u64 off, u32 data)
{
	unsigned long flags = 0;
	int rv;
	
	rv = unm_nic_pci_get_crb_addr_2M(adapter, &off, 4);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&adapter->adapter_lock, flags);
		crb_win_lock(adapter);
		unm_nic_pci_set_crbwindow_2M(adapter, &off);
	}

	DPRINTK(1, INFO, "write data %lx to offset %llx, len=%d\n",
				data, off, len);

	UNM_NIC_PCI_WRITE_32(data, (void *)(uptr_t)off);

	if (rv == 1) {
    	        crb_win_unlock(adapter);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
        }

        return 0;
}

 int
unm_nic_hw_read_ioctl_128M(unm_adapter *adapter, u64 off, void *data, int len)
{
	void		*addr;
	unsigned long	flags=0;
	u64		offset;
	uint8_t		*mem_ptr = NULL;
	unsigned long	mem_base;
	unsigned long	mem_page;

	if(ADDR_IN_WINDOW1(off)) {//Window 1
		addr = CRB_NORMALIZE(adapter, off);
		if(!addr) {
			mem_base = pci_resource_start(adapter->ahw.pdev, 0);
			offset = CRB_NORMAL(off);
			if (adapter->ahw.pci_len0 == 0)
			    offset -= UNM_PCI_CRBSPACE;
			mem_page = offset & PAGE_MASK;
			if(mem_page != ((offset + len - 1) & PAGE_MASK))
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
			else
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
			if(mem_ptr == 0UL) {
				*(__uint8_t  *)data = 0;
				return 1;
			}
			addr = mem_ptr;
			addr += offset & (PAGE_SIZE - 1);
		}
		read_lock(&adapter->adapter_lock);
	} else {//Window 0
		addr = (void *)(uptr_t)(pci_base_offset(adapter, off));
		if(!addr) {
			mem_base = pci_resource_start(adapter->ahw.pdev, 0);
			if (adapter->ahw.pci_len0 == 0)
			    off -= UNM_PCI_CRBSPACE;
			mem_page = off & PAGE_MASK;
			if(mem_page != ((off + len - 1) & PAGE_MASK))
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
			else
				mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
			if(mem_ptr == 0UL) {
				return 1;
			}
			addr = mem_ptr;
			addr += off & (PAGE_SIZE - 1);
		}
		write_lock_irqsave(&adapter->adapter_lock, flags);
		unm_nic_pci_change_crbwindow_128M(adapter, 0);
	}

	DPRINTK(1, INFO, "reading from base %lx offset %llx addr %p\n",
			pci_base(adapter, off), off, addr);

	switch (len) {
	case 1:
		*(__uint8_t  *)data = UNM_NIC_PCI_READ_8(addr);
		break;
	case 2:
		*(__uint16_t *)data = UNM_NIC_PCI_READ_16(addr);
		break;
	case 4:
		*(__uint32_t *)data = UNM_NIC_PCI_READ_32(addr);
		break;
	case 8:
		*(__uint64_t *)data = UNM_NIC_PCI_READ_64(addr);
		break;
	default:
#if !defined(NDEBUG)
		if ((len & 0x7) != 0)
			printk("%s: %s len(%d) not multiple of 8.\n",
				unm_nic_driver_name, __FUNCTION__, len);
#endif
		UNM_NIC_HW_BLOCK_READ_64(data, addr, (len>>3));
		break;
	}
	DPRINTK(1, INFO, "read %lx\n", *(unsigned long *)data);

	if(ADDR_IN_WINDOW1(off)) {//Window 1
		read_unlock(&adapter->adapter_lock);
	} else {//Window 0
		unm_nic_pci_change_crbwindow_128M(adapter, 1);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
	}

	if(mem_ptr)
		iounmap(mem_ptr);
	return 0;
}

u32
unm_nic_hw_read_wx_2M(unm_adapter *adapter, u64 off)
{
	unsigned long flags = 0;
	int rv;
	u32 data;

	rv = unm_nic_pci_get_crb_addr_2M(adapter, &off, 4);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&adapter->adapter_lock, flags);
		crb_win_lock(adapter);
		unm_nic_pci_set_crbwindow_2M(adapter, &off);
	}

	DPRINTK(1, INFO, "read from offset %llx, len=%d\n", off, len);

	data = UNM_NIC_PCI_READ_32((void *)(uptr_t)off);

	DPRINTK(1, INFO, "read %lx\n", *(unsigned long *)data);

	if (rv == 1) {
    	        crb_win_unlock(adapter);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
	}

	return data;
}

u32
unm_nic_hw_read_wx_128M(unm_adapter *adapter, u64 off)
{
        void *addr;
        unsigned long        flags = 0;
	u32 data;

        if(ADDR_IN_WINDOW1(off))
        {//Window 1
                addr = CRB_NORMALIZE(adapter, off);
		read_lock(&adapter->adapter_lock);
        }
        else{//Window 0
                addr = (void *)(uptr_t)(pci_base_offset(adapter, off));
		write_lock_irqsave(&adapter->adapter_lock, flags);
		unm_nic_pci_change_crbwindow_128M(adapter, 0);
        }

        DPRINTK(1, INFO, "reading from base %lx offset %llx addr %p\n",
                            pci_base(adapter, off), off, addr);

	if(!addr) {
		if(ADDR_IN_WINDOW1(off)) {//Window 1
			read_unlock(&adapter->adapter_lock);
		} else {//Window 0
			unm_nic_pci_change_crbwindow_128M(adapter, 1);
			write_unlock_irqrestore(&adapter->adapter_lock, flags);
		}
		return 1;
	}

	data = UNM_NIC_PCI_READ_32(addr);

        DPRINTK(1, INFO, "read %lx\n", *(unsigned long *)data);

        if(ADDR_IN_WINDOW1(off))
        {//Window 1
		read_unlock(&adapter->adapter_lock);
        }
        else{//Window 0
		unm_nic_pci_change_crbwindow_128M(adapter, 1);
		write_unlock_irqrestore(&adapter->adapter_lock, flags);
        }

        return data;
}

/*  PCI Windowing for DDR regions.  */
#define ADDR_IN_RANGE(addr, low, high)            \
        (((addr) <= (high)) && ((addr) >= (low)))

/*
 * check memory access boundary.
 * used by test agent. support ddr access only for now
 */
static unsigned long
unm_nic_pci_mem_bound_check(struct unm_adapter_s *adapter,
        unsigned long long addr, int size)
{
        if (!ADDR_IN_RANGE(addr, UNM_ADDR_DDR_NET, UNM_ADDR_DDR_NET_MAX) ||
            !ADDR_IN_RANGE(addr + size -1, UNM_ADDR_DDR_NET, UNM_ADDR_DDR_NET_MAX) ||
            ((size != 1) && (size != 2) && (size != 4) && (size != 8))) {
                return 0;
        }

        return 1;
}

int unm_pci_set_window_warning_count = 0;

unsigned long unm_nic_pci_set_window_128M (struct unm_adapter_s *adapter,
					    unsigned long long addr)
{
        int			window;
	unsigned long long	qdr_max;

	qdr_max = NX_P3_ADDR_QDR_NET_MAX;

	/* printk("unm_nic_pci_set_window addr_128M: %016llx\n", addr); */
        if (ADDR_IN_RANGE(addr, UNM_ADDR_DDR_NET, UNM_ADDR_DDR_NET_MAX)) {
                /* DDR network side */
                /* printk("MN DDR\n"); */
		BUG();	/* MN access should never come here */
#if	1
                addr -= UNM_ADDR_DDR_NET;
                window = (addr >> 25 )  & 0x3ff;
                if (adapter->ahw.ddr_mn_window != window) {
                        /* printk("Change the MN window!!! \n"); */
                        adapter->ahw.ddr_mn_window = window;
                        UNM_NIC_PCI_WRITE_32(window, (void *)(uptr_t)(PCI_OFFSET_SECOND_RANGE(adapter,
                            UNM_PCIX_PH_REG(PCIE_MN_WINDOW_REG(adapter->ahw.pci_func)))));
                        /* MUST make sure window is set before we forge on... */
                        UNM_NIC_PCI_READ_32((void *)(uptr_t)(PCI_OFFSET_SECOND_RANGE(adapter,
                            UNM_PCIX_PH_REG(PCIE_MN_WINDOW_REG(adapter->ahw.pci_func)))));
                }
                addr -= (window * 0x2000000);
                addr+= UNM_PCI_DDR_NET;
#else
		addr = -1UL;
#endif
        } else if (ADDR_IN_RANGE(addr, UNM_ADDR_OCM0, UNM_ADDR_OCM0_MAX)) {
                addr -= UNM_ADDR_OCM0;
                addr += UNM_PCI_OCM0;
        } else if (ADDR_IN_RANGE(addr, UNM_ADDR_OCM1, UNM_ADDR_OCM1_MAX)) {
                addr -= UNM_ADDR_OCM1;
                addr += UNM_PCI_OCM1;
        } else if (ADDR_IN_RANGE(addr, UNM_ADDR_QDR_NET, qdr_max)) {
                /* printk("SN QDR\n"); */
                /* QDR network side */
                addr -= UNM_ADDR_QDR_NET;
                window = (addr >> 22) & 0x3f;
                if (adapter->ahw.qdr_sn_window != window) {
                        /* printk("Change the SN window!!! \n"); */
                        adapter->ahw.qdr_sn_window = window;
                        UNM_NIC_PCI_WRITE_32((window << 22),
                               (void *)(uptr_t)(PCI_OFFSET_SECOND_RANGE(adapter,
                               UNM_PCIX_PH_REG(PCIE_SN_WINDOW_REG(adapter->ahw.pci_func)))));
                        /* MUST make sure window is set before we forge on... */
                        UNM_NIC_PCI_READ_32((void *)(uptr_t)(PCI_OFFSET_SECOND_RANGE(adapter,
                               UNM_PCIX_PH_REG(PCIE_SN_WINDOW_REG(adapter->ahw.pci_func)))));
                }
                addr -= (window * 0x400000);
                addr+= UNM_PCI_QDR_NET;
        } else {
                /*
                 * peg gdb frequently accesses memory that doesn't exist,
                 * this limits the chit chat so debugging isn't slowed down.
                 */
                if((unm_pci_set_window_warning_count++ < 8)
                    || (unm_pci_set_window_warning_count%64 == 0)) {
                        printk("%s: Warning:unm_nic_pci_set_window() "
                                "Unknown address range!\n",unm_nic_driver_name);
                }
		addr = -1UL;
        }
        /* printk("New address: 0x%08lx\n",addr); */
        return addr;
}

unsigned long
unm_nic_pci_set_window_2M (struct unm_adapter_s *adapter, unsigned long long addr)
{
	int window;
    u32 win_read;

	if (ADDR_IN_RANGE(addr, UNM_ADDR_DDR_NET, UNM_ADDR_DDR_NET_MAX)) {
		/* DDR network side */
		window = MN_WIN(addr);
		adapter->ahw.ddr_mn_window = window;
		NXWR32(adapter, adapter->ahw.mn_win_crb | UNM_PCI_CRBSPACE, window);
		win_read = NXRD32(adapter, adapter->ahw.mn_win_crb | UNM_PCI_CRBSPACE);
        if ((win_read << 17) != window) {
            printk("%s: Written MNwin (0x%x) != Read MNwin (0x%x)\n",
                   __FUNCTION__, window, win_read);
        }
        addr = GET_MEM_OFFS_2M(addr) + UNM_PCI_DDR_NET;
	} else if (ADDR_IN_RANGE(addr, UNM_ADDR_OCM0, UNM_ADDR_OCM0_MAX)) {
		unsigned int temp1;
//        OCM: pci_addr[20:18] == 011 && pci_addr[17:11] != 7f
        if ((addr & 0x00ff800) == 0xff800) {            // if bits 19:18&17:11 are on
            printk("%s: QM access not handled.\n", __FUNCTION__);
            addr = -1UL;
        }

		window = OCM_WIN(addr);
		adapter->ahw.ddr_mn_window = window;
		NXWR32(adapter, adapter->ahw.mn_win_crb | UNM_PCI_CRBSPACE, window);
		win_read = NXRD32(adapter, adapter->ahw.mn_win_crb | UNM_PCI_CRBSPACE);
		temp1 = ((window & 0x1FF) << 7) | ((window & 0x0FFFE0000) >> 17);
        if ( win_read != temp1 ) {
            printk("%s: Written OCMwin (0x%x) != Read OCMwin (0x%x)\n",
                   __FUNCTION__, temp1, win_read);
        }
		addr = GET_MEM_OFFS_2M(addr) + UNM_PCI_OCM0_2M;

	} else if (ADDR_IN_RANGE(addr, UNM_ADDR_QDR_NET, NX_P3_ADDR_QDR_NET_MAX)) {
		                                /* QDR network side */
		window = MS_WIN(addr);
		adapter->ahw.qdr_sn_window = window;
		NXWR32(adapter, adapter->ahw.ms_win_crb | UNM_PCI_CRBSPACE, window);
		win_read = NXRD32(adapter, adapter->ahw.ms_win_crb | UNM_PCI_CRBSPACE);
        if (win_read != window) {
            printk("%s: Written MSwin (0x%x) != Read MSwin (0x%x)\n",
                   __FUNCTION__, window, win_read);
        }
        addr = GET_MEM_OFFS_2M(addr) + UNM_PCI_QDR_NET;

	} else {
		/*
		 * peg gdb frequently accesses memory that doesn't exist,
		 * this limits the chit chat so debugging isn't slowed down.
		 */
		if((unm_pci_set_window_warning_count++ < 8)
		    || (unm_pci_set_window_warning_count%64 == 0)) {
			printk("%s: Warning:%s Unknown address range!\n", __FUNCTION__,
                   unm_nic_driver_name);
		}
		addr = -1UL;
	}
	/* printk("New address: 0x%08lx\n",addr); */
	return addr;
}



/* check if address is in the same windows as the previous access */
static int unm_nic_pci_is_same_window(struct unm_adapter_s *adapter,
				      unsigned long long addr)
{
	int			window;
	unsigned long long	qdr_max;

	qdr_max = NX_P3_ADDR_QDR_NET_MAX;

	if (ADDR_IN_RANGE(addr, UNM_ADDR_DDR_NET, UNM_ADDR_DDR_NET_MAX)) {
		/* DDR network side */
		BUG();	/* MN access can not come here */
#if 0
		window = ((addr - UNM_ADDR_DDR_NET) >> 25) & 0x3ff;
		if (adapter->ahw.ddr_mn_window == window) {
			return 1;
		}
#endif
	} else if (ADDR_IN_RANGE(addr, UNM_ADDR_OCM0, UNM_ADDR_OCM0_MAX)) {
		return 1;
	} else if (ADDR_IN_RANGE(addr, UNM_ADDR_OCM1, UNM_ADDR_OCM1_MAX)) {
		return 1;
	} else if (ADDR_IN_RANGE(addr, UNM_ADDR_QDR_NET, qdr_max)) {
		/* QDR network side */
		window = ((addr - UNM_ADDR_QDR_NET) >> 22) & 0x3f;
		if (adapter->ahw.qdr_sn_window == window) {
			return 1;
		}
	}

	return 0;
}


static int unm_nic_pci_mem_read_direct(struct unm_adapter_s *adapter,
                        u64 off, void *data, int size)
{
        unsigned long   flags;
        void           *addr;
        int             ret = 0;
        u64             start;
        uint8_t         *mem_ptr = NULL;
        unsigned long   mem_base;
        unsigned long   mem_page;

#if 0
	/*
	 * This check can not be currently executed, since phanmon findq
	 * command breaks this check whereby 8 byte reads are being attempted
	 * on "aligned-by-4" addresses on x86. Reason this works is our version
	 * of "readq"/"writeq" breaks up the access into 2 consecutive 4
	 * byte writes; on other architectures, readq/writeq might require
	 * "aligned-by-8" addresses and we will run into trouble.
	 *
	 * Check alignment for expected sizes of 1, 2, 4, 8. Other size
	 * values will not trigger access.
	 */
	if ((off & (size - 1)) != 0)
		return(-1);
#endif

        write_lock_irqsave(&adapter->adapter_lock, flags);

	/*
	 * If attempting to access unknown address or straddle hw windows,
         * do not access.
	 */
	if (((start = adapter->unm_nic_pci_set_window(adapter, off)) == -1UL) ||
                (unm_nic_pci_is_same_window(adapter, off + size -1) == 0)) {
                write_unlock_irqrestore(&adapter->adapter_lock, flags);
                printk(KERN_ERR"%s out of bound pci memory access. "
                        "offset is 0x%llx\n", unm_nic_driver_name, off);
                return -1;
        }

        addr = (void *)(uptr_t)(pci_base_offset(adapter, start));
        if(!addr) {
                write_unlock_irqrestore(&adapter->adapter_lock, flags);
                mem_base = pci_resource_start(adapter->ahw.pdev, 0);
                mem_page = start & PAGE_MASK;
                /* Map two pages whenever user tries to access addresses in two
                   consecutive pages.
                 */
                if(mem_page != ((start + size - 1) & PAGE_MASK))
                        mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
                else
                        mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
                if(mem_ptr == 0UL) {
                        *(__uint8_t  *)data = 0;
                        return -1;
                }
                addr = mem_ptr;
                addr += start & (PAGE_SIZE - 1);
                write_lock_irqsave(&adapter->adapter_lock, flags);
       }

        switch (size) {
        case 1:
                *(__uint8_t  *)data = UNM_NIC_PCI_READ_8(addr);
                break;
        case 2:
                *(__uint16_t *)data = UNM_NIC_PCI_READ_16(addr);
                break;
        case 4:
                *(__uint32_t *)data = UNM_NIC_PCI_READ_32(addr);
                break;
        case 8:
                *(__uint64_t *)data = UNM_NIC_PCI_READ_64(addr);
                break;
        default:
                ret = -1;
                break;
        }
        write_unlock_irqrestore(&adapter->adapter_lock, flags);
        DPRINTK(1, INFO, "read %llx\n", *(unsigned long long*)data);

	if(mem_ptr)
		iounmap(mem_ptr);
        return ret;
}

static int
unm_nic_pci_mem_write_direct(struct unm_adapter_s *adapter, u64 off,
                        void *data, int size)
{
        unsigned long   flags;
        void           *addr;
        int             ret = 0;
        u64             start;
        uint8_t         *mem_ptr = NULL;
        unsigned long   mem_base;
        unsigned long   mem_page;

#if 0
	/*
	 * This check can not be currently executed, since firmware load
	 * breaks this check whereby 8 byte writes are being attempted on
	 * "aligned-by-4" addresses on x86. Reason this works is our version
	 * of "readq"/"writeq" breaks up the access into 2 consecutive 4
	 * byte writes; on other architectures, readq/writeq might require
	 * "aligned-by-8" addresses and we will run into trouble.
	 *
	 * Check alignment for expected sizes of 1, 2, 4, 8. Other size
	 * values will not trigger access.
	 */
	if ((off & (size - 1)) != 0)
		return(-1);
#endif

        write_lock_irqsave(&adapter->adapter_lock, flags);

	/*
	 * If attempting to access unknown address or straddle hw windows,
         * do not access.
	 */
	    if (((start = adapter->unm_nic_pci_set_window(adapter, off)) == -1UL) ||
                (unm_nic_pci_is_same_window(adapter, off + size -1) == 0)) {
                write_unlock_irqrestore(&adapter->adapter_lock, flags);
                printk(KERN_ERR"%s out of bound pci memory access. "
                        "offset is 0x%llx\n", unm_nic_driver_name, off);
                return -1;
        }

        addr = (void *)(uptr_t)(pci_base_offset(adapter, start));
        if(!addr) {
                write_unlock_irqrestore(&adapter->adapter_lock, flags);
                mem_base = pci_resource_start(adapter->ahw.pdev, 0);
                mem_page = start & PAGE_MASK;
                /* Map two pages whenever user tries to access addresses in two
                   consecutive pages.
                 */
                if(mem_page != ((start + size - 1) & PAGE_MASK))
                        mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE*2);
                else
                        mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
                if(mem_ptr == 0UL) {
                        return -1;
                }
                addr = mem_ptr;
                addr += start & (PAGE_SIZE - 1);
                write_lock_irqsave(&adapter->adapter_lock, flags);
        }

        switch (size) {
        case 1:
                UNM_NIC_PCI_WRITE_8( *(__uint8_t  *)data, addr);
                break;
        case 2:
                UNM_NIC_PCI_WRITE_16(*(__uint16_t *)data, addr);
                break;
        case 4:
                UNM_NIC_PCI_WRITE_32(*(__uint32_t *)data, addr);
                break;
        case 8:
                UNM_NIC_PCI_WRITE_64(*(__uint64_t *)data, addr);
                break;
        default:
                ret = -1;
                break;
        }
        write_unlock_irqrestore(&adapter->adapter_lock, flags);
        DPRINTK(1, INFO, "writing data %llx to offset %llx\n",
                            *(unsigned long long *)data, start);
	if(mem_ptr)
		iounmap(mem_ptr);
        return ret;
}


int
unm_nic_pci_mem_write_128M(struct unm_adapter_s *adapter, u64 off, void *data,
    int size)
{
	unsigned long   flags;
	int	     i, j, ret = 0, loop, sz[2], off0;
	__uint32_t      temp;
	__uint64_t      off8, mem_crb, tmpw, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */
	if (unm_nic_pci_mem_bound_check(adapter, off, size) == 0)
		return(unm_nic_pci_mem_write_direct(adapter, off, data, size));

	off8 = off & 0xfffffff8;
	off0 = off & 0x7;
	sz[0] = (size < (8 - off0)) ? size : (8 - off0);
	sz[1] = size - sz[0];
	loop = ((off0 + size - 1) >> 3) + 1;
	mem_crb = (uptr_t)(pci_base_offset(adapter, UNM_CRB_DDR_NET));

	if ((size != 8) || (off0 != 0))  {
		for (i = 0; i < loop; i++) {
			if (adapter->unm_nic_pci_mem_read(adapter,
				off8 + (i << 3), &word[i], 8))
				return -1;
		}
	}

	switch (size) {
	case 1:
		tmpw = *((__uint8_t *)data);
		break;
	case 2:
		tmpw = *((__uint16_t *)data);
		break;
	case 4:
		tmpw = *((__uint32_t *)data);
		break;
	case 8:
	default:
		tmpw = *((__uint64_t *)data);
		break;
	}
	word[0] &= ~((~(~0ULL << (sz[0] * 8))) << (off0 * 8));
	word[0] |= tmpw << (off0 * 8);

	if (loop == 2) {
		word[1] &= ~(~0ULL << (sz[1] * 8));
		word[1] |= tmpw >> (sz[0] * 8);
	}

	write_lock_irqsave(&adapter->adapter_lock, flags);
	unm_nic_pci_change_crbwindow_128M(adapter, 0);

	for (i = 0; i < loop; i++) {
		UNM_NIC_PCI_WRITE_32((__uint32_t)(off8 + (i << 3)),
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_ADDR_LO));
		UNM_NIC_PCI_WRITE_32(0,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_ADDR_HI));
		UNM_NIC_PCI_WRITE_32(word[i] & 0xffffffff,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_WRDATA_LO));
		UNM_NIC_PCI_WRITE_32((word[i] >> 32) & 0xffffffff,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_WRDATA_HI));
		UNM_NIC_PCI_WRITE_32(MIU_TA_CTL_ENABLE|MIU_TA_CTL_WRITE,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));
		UNM_NIC_PCI_WRITE_32(MIU_TA_CTL_START|MIU_TA_CTL_ENABLE|MIU_TA_CTL_WRITE,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));

		for (j = 0; j< MAX_CTL_CHECK; j++) {
			temp = UNM_NIC_PCI_READ_32(
			     (void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));
			if ((temp & MIU_TA_CTL_BUSY) == 0) {
				break;
			}
		}
	
		if (j >= MAX_CTL_CHECK) {
			printk("%s: %s Fail to write through agent\n", __FUNCTION__, unm_nic_driver_name);
			ret = -1;
			break;
		}
	}
	
	unm_nic_pci_change_crbwindow_128M(adapter, 1);
	write_unlock_irqrestore(&adapter->adapter_lock, flags);
	return ret;
}

int
unm_nic_pci_mem_read_128M(struct unm_adapter_s *adapter, u64 off, void *data,
    int size)
{
	unsigned long   flags;
	int	     i, j=0, k, start, end, loop, sz[2], off0[2];
	__uint32_t      temp;
	__uint64_t      off8, val, mem_crb, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */
	if (unm_nic_pci_mem_bound_check(adapter, off, size) == 0)
		return(unm_nic_pci_mem_read_direct(adapter, off, data, size));
	
	off8 = off & 0xfffffff8;
	off0[0] = off & 0x7;
	off0[1] = 0;
	sz[0] = (size < (8 - off0[0])) ? size : (8 - off0[0]);
	sz[1] = size - sz[0];
	loop = ((off0[0] + size - 1) >> 3) + 1;
	mem_crb = (uptr_t)(pci_base_offset(adapter, UNM_CRB_DDR_NET));

	write_lock_irqsave(&adapter->adapter_lock, flags);
	unm_nic_pci_change_crbwindow_128M(adapter, 0);

	for (i = 0; i < loop; i++) {
		UNM_NIC_PCI_WRITE_32((__uint32_t)(off8 + (i << 3)),
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_ADDR_LO));
		UNM_NIC_PCI_WRITE_32(0,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_ADDR_HI));
		UNM_NIC_PCI_WRITE_32(MIU_TA_CTL_ENABLE,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));
		UNM_NIC_PCI_WRITE_32(MIU_TA_CTL_START|MIU_TA_CTL_ENABLE,
			(void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));

		for (j = 0; j< MAX_CTL_CHECK; j++) {
			temp = UNM_NIC_PCI_READ_32(
			      (void *)(uptr_t)(mem_crb+MIU_TEST_AGT_CTRL));
			if ((temp & MIU_TA_CTL_BUSY) == 0) {
				break;
			}
		}
	
		if (j >= MAX_CTL_CHECK) {
			printk("%s: %s Fail to read through agent\n", __FUNCTION__, unm_nic_driver_name);
			break;
		}

		start = off0[i] >> 2;
		end   = (off0[i] + sz[i] - 1) >> 2;
		word[i] = 0;
		for (k = start; k <= end; k++) {
			word[i] |= ((__uint64_t) UNM_NIC_PCI_READ_32(
				    (void *)(uptr_t)(mem_crb +
				    MIU_TEST_AGT_RDDATA(k))) << (32*k));
		}
	}

	unm_nic_pci_change_crbwindow_128M(adapter, 1);
	write_unlock_irqrestore(&adapter->adapter_lock, flags);

	if (j >= MAX_CTL_CHECK)
		return -1;

	if (sz[0] == 8) {
		val = word[0];
	} else {
		val = ((word[0] >> (off0[0] * 8)) & (~(~0ULL << (sz[0] * 8)))) |
			((word[1] & (~(~0ULL << (sz[1] * 8)))) << (sz[0] * 8));
	}

	switch (size) {
	case 1:
		*(__uint8_t  *)data = val;
		break;
	case 2:
		*(__uint16_t *)data = val;
		break;
	case 4:
		*(__uint32_t *)data = val;
		break;
	case 8:
		*(__uint64_t *)data = val;
		break;
	}
	DPRINTK(1, INFO, "read %llx\n", *(unsigned long long*)data);
	return 0;
}



int
unm_nic_pci_mem_write_2M(struct unm_adapter_s *adapter, u64 off, void *data,
    int size)
{
        int             i, j, ret = 0, loop, sz[2], off0;
        __uint32_t      temp;
        __uint64_t      off8, mem_crb, tmpw, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */
        if (off >= UNM_ADDR_QDR_NET && off <= NX_P3_ADDR_QDR_NET_MAX) {
            mem_crb = UNM_CRB_QDR_NET;
        } else {
            mem_crb = UNM_CRB_DDR_NET;
            if (unm_nic_pci_mem_bound_check(adapter, off, size) == 0)
		        return(unm_nic_pci_mem_write_direct(adapter, off, data, size));
        }

        off8 = off & 0xfffffff8;
        off0 = off & 0x7;
        sz[0] = (size < (8 - off0)) ? size : (8 - off0);
        sz[1] = size - sz[0];
        loop = ((off0 + size - 1) >> 3) + 1;

        if ((size != 8) || (off0 != 0))  {
            for (i = 0; i < loop; i++) {
                if (adapter->unm_nic_pci_mem_read(adapter, off8 + (i << 3),
                                              &word[i], 8))
                    return -1;
            }
        }

        switch (size) {
        case 1:
                tmpw = *((__uint8_t *)data);
                break;
        case 2:
                tmpw = *((__uint16_t *)data);
                break;
        case 4:
                tmpw = *((__uint32_t *)data);
                break;
        case 8:
        default:
                tmpw = *((__uint64_t *)data);
                break;
        }

        word[0] &= ~((~(~0ULL << (sz[0] * 8))) << (off0 * 8));
        word[0] |= tmpw << (off0 * 8);

        if (loop == 2) {
                word[1] &= ~(~0ULL << (sz[1] * 8));
                word[1] |= tmpw >> (sz[0] * 8);
        }

// don't lock here - write_wx gets the lock if each time
//        write_lock_irqsave(&adapter->adapter_lock, flags);
//        unm_nic_pci_change_crbwindow_128M(adapter, 0);

        for (i = 0; i < loop; i++) {
            temp = off8 + (i << 3);
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_ADDR_LO, temp);
            temp = 0;
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_ADDR_HI, temp);
            temp = word[i] & 0xffffffff;
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_WRDATA_LO, temp);
            temp = (word[i] >> 32) & 0xffffffff;
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_WRDATA_HI, temp);
            temp = MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_CTRL, temp);
            temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
            NXWR32(adapter, mem_crb+MIU_TEST_AGT_CTRL, temp);

                for (j = 0; j< MAX_CTL_CHECK; j++) {
                        temp = NXRD32(adapter, mem_crb + MIU_TEST_AGT_CTRL);
                        if ((temp & MIU_TA_CTL_BUSY) == 0) {
                                break;
                        }
                }

                if (j >= MAX_CTL_CHECK) {
                        printk("%s: Fail to write through agent\n", unm_nic_driver_name);
                        ret = -1;
                        break;
                }
        }

//        unm_nic_pci_change_crbwindow_128M(adapter, 1);
//        write_unlock_irqrestore(&adapter->adapter_lock, flags);
        return ret;
}

//MINIDUMP related
int
unm_nic_pci_mem_read_md(struct unm_adapter_s *adapter, u64 off, void *data,
    int size, int mem_type)
{
//        unsigned long   flags;
        int             i, j=0, k, start, end, loop, sz[2], off0[2];
        __uint32_t      temp;
        __uint64_t      off8, val, mem_crb, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */

        if (mem_type == RDMEM) {
            mem_crb = UNM_CRB_QDR_NET;
        } else {
            mem_crb = UNM_CRB_DDR_NET;
            if (unm_nic_pci_mem_bound_check(adapter, off, size) == 0)
		        return(unm_nic_pci_mem_read_direct(adapter, off, data, size));
        }

        off8 = off & 0xfffffff8;
        off0[0] = off & 0x7;
        off0[1] = 0;
        sz[0] = (size < (8 - off0[0])) ? size : (8 - off0[0]);
        sz[1] = size - sz[0];
        loop = ((off0[0] + size - 1) >> 3) + 1;

// don't get lock - write_wx will get it
//	    write_lock_irqsave(&adapter->adapter_lock, flags);
//	unm_nic_pci_change_crbwindow_128M(adapter, 0);

        for (i = 0; i < loop; i++) {
                temp = off8 + (i << 3);
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_ADDR_LO,
                                       temp);
                temp = 0;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_ADDR_HI,
                                 temp);
                temp = MIU_TA_CTL_ENABLE;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_CTRL,
                                       temp);
                temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_CTRL,
                               temp);

                for (j = 0; j< MAX_CTL_CHECK; j++) {
                        temp = NXRD32(adapter,
						mem_crb + MIU_TEST_AGT_CTRL);
                        if ((temp & MIU_TA_CTL_BUSY) == 0) {
                                break;
                        }
                }

                if (j >= MAX_CTL_CHECK) {
                        printk("%s: Fail to read through agent\n",unm_nic_driver_name);
                        break;
                }

                start = off0[i] >> 2;
                end   = (off0[i] + sz[i] - 1) >> 2;
                for (k = start; k <= end; k++) {
                    temp = NXRD32(adapter,
						mem_crb + MIU_TEST_AGT_RDDATA(k));
                    word[i] |= ((__uint64_t)temp << (32 * k));
                }
        }

//        unm_nic_pci_change_crbwindow_128M(adapter, 1);
//        write_unlock_irqrestore(&adapter->adapter_lock, flags);

        if (j >= MAX_CTL_CHECK)
                return -1;

        if (sz[0] == 8) {
                val = word[0];
        } else {
                val = ((word[0] >> (off0[0] * 8)) & (~(~0ULL << (sz[0] * 8)))) |
                        ((word[1] & (~(~0ULL << (sz[1] * 8)))) << (sz[0] * 8));
        }

        switch (size) {
        case 1:
                *(__uint8_t  *)data = val;
                break;
        case 2:
                *(__uint16_t *)data = val;
                break;
        case 4:
                *(__uint32_t *)data = val;
                break;
        case 8:
                *(__uint64_t *)data = val;
                break;
        }
        DPRINTK(1, INFO, "read %llx\n", *(unsigned long long*)data);
        return 0;
}

int
unm_nic_pci_mem_read_2M(struct unm_adapter_s *adapter, u64 off, void *data,
    int size)
{
//        unsigned long   flags;
        int             i, j=0, k, start, end, loop, sz[2], off0[2];
        __uint32_t      temp;
        __uint64_t      off8, val, mem_crb, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */

        if (off >= UNM_ADDR_QDR_NET && off <= NX_P3_ADDR_QDR_NET_MAX) {
            mem_crb = UNM_CRB_QDR_NET;
        } else {
            mem_crb = UNM_CRB_DDR_NET;
            if (unm_nic_pci_mem_bound_check(adapter, off, size) == 0)
		        return(unm_nic_pci_mem_read_direct(adapter, off, data, size));
        }

        off8 = off & 0xfffffff8;
        off0[0] = off & 0x7;
        off0[1] = 0;
        sz[0] = (size < (8 - off0[0])) ? size : (8 - off0[0]);
        sz[1] = size - sz[0];
        loop = ((off0[0] + size - 1) >> 3) + 1;

// don't get lock - write_wx will get it
//	    write_lock_irqsave(&adapter->adapter_lock, flags);
//	unm_nic_pci_change_crbwindow_128M(adapter, 0);

        for (i = 0; i < loop; i++) {
                temp = off8 + (i << 3);
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_ADDR_LO,
                                       temp);
                temp = 0;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_ADDR_HI,
                                 temp);
                temp = MIU_TA_CTL_ENABLE;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_CTRL,
                                       temp);
                temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE;
                NXWR32(adapter, mem_crb + MIU_TEST_AGT_CTRL,
                               temp);

                for (j = 0; j< MAX_CTL_CHECK; j++) {
                        temp = NXRD32(adapter,
						mem_crb + MIU_TEST_AGT_CTRL);
                        if ((temp & MIU_TA_CTL_BUSY) == 0) {
                                break;
                        }
                }

                if (j >= MAX_CTL_CHECK) {
                        printk("%s: Fail to read through agent\n",unm_nic_driver_name);
                        break;
                }

                start = off0[i] >> 2;
                end   = (off0[i] + sz[i] - 1) >> 2;
                for (k = start; k <= end; k++) {
                    temp = NXRD32(adapter,
						mem_crb + MIU_TEST_AGT_RDDATA(k));
                    word[i] |= ((__uint64_t)temp << (32 * k));
                }
        }

//        unm_nic_pci_change_crbwindow_128M(adapter, 1);
//        write_unlock_irqrestore(&adapter->adapter_lock, flags);

        if (j >= MAX_CTL_CHECK)
                return -1;

        if (sz[0] == 8) {
                val = word[0];
        } else {
                val = ((word[0] >> (off0[0] * 8)) & (~(~0ULL << (sz[0] * 8)))) |
                        ((word[1] & (~(~0ULL << (sz[1] * 8)))) << (sz[0] * 8));
        }

        switch (size) {
        case 1:
                *(__uint8_t  *)data = val;
                break;
        case 2:
                *(__uint16_t *)data = val;
                break;
        case 4:
                *(__uint32_t *)data = val;
                break;
        case 8:
                *(__uint64_t *)data = val;
                break;
        }
        DPRINTK(1, INFO, "read %llx\n", *(unsigned long long*)data);
        return 0;
}

int
unm_nic_get_board_info(struct unm_adapter_s *adapter)
{
	int                rv=0;
	unm_board_info_t  *boardinfo;
	int                i;
	int                addr = BRDCFG_START;
	uint32_t          *ptr32;
	uint32_t          gpioval;

	boardinfo = &adapter->ahw.boardcfg;
	ptr32 = (uint32_t *)boardinfo;

	for (i=0;i<sizeof(unm_board_info_t)/sizeof(uint32_t);i++) {
		if (rom_fast_read(adapter, addr, ptr32) == -1) {
			printk("%s: Rom read failed after %d \n",
					unm_nic_driver_name,i);
			return -1;
		}
		//printk("ROM: %08X\n",*ptr32);
		ptr32++;
		addr+= sizeof(uint32_t);
	}
	if (boardinfo->magic != UNM_BDINFO_MAGIC) {
		printk("%s: ERROR reading %s board config."
				" Read %x, expected %x\n", unm_nic_driver_name,
				unm_nic_driver_name, boardinfo->magic, UNM_BDINFO_MAGIC);
		rv=-1;
	}
	if (boardinfo->header_version != UNM_BDINFO_VERSION) {
		printk("%s: Unknown board config version."
				" Read %x, expected %x\n", unm_nic_driver_name,
				boardinfo->header_version, UNM_BDINFO_VERSION);
		rv=-1;
	}

	if (boardinfo->board_type == UNM_BRDTYPE_P3_4_GB_MM) {
		gpioval = NXRD32(adapter, UNM_ROMUSB_GLB_PAD_GPIO_I);
		if ((gpioval & 0x8000) == 0) {
			boardinfo->board_type = UNM_BRDTYPE_P3_10G_TP;
		}
	}

	DPRINTK(1, INFO, "Discovered board type:0x%x  ",boardinfo->board_type);
	switch((unm_brdtype_t)boardinfo->board_type){
		case UNM_BRDTYPE_P3_HMEZ:
		case UNM_BRDTYPE_P3_XG_LOM:
		case UNM_BRDTYPE_P3_10G_CX4:
		case UNM_BRDTYPE_P3_10G_CX4_LP:
		case UNM_BRDTYPE_P3_IMEZ:
		case UNM_BRDTYPE_P3_10G_SFP_PLUS:
		case UNM_BRDTYPE_P3_10G_XFP:
		case UNM_BRDTYPE_P3_10000_BASE_T:
			adapter->ahw.board_type = UNM_NIC_XGBE;
			break;
		case UNM_BRDTYPE_P3_REF_QG:
		case UNM_BRDTYPE_P3_4_GB:
		case UNM_BRDTYPE_P3_4_GB_MM:
			adapter->ahw.board_type = UNM_NIC_GBE;
			break;
		case UNM_BRDTYPE_P1_BD:
		case UNM_BRDTYPE_P1_SB:
		case UNM_BRDTYPE_P1_SMAX:
		case UNM_BRDTYPE_P1_SOCK:
			adapter->ahw.board_type = UNM_NIC_GBE;
			break;
		case UNM_BRDTYPE_P3_10G_TP:
			if (adapter->portnum < 2) {
				adapter->ahw.board_type = UNM_NIC_XGBE;
			} else {
				adapter->ahw.board_type = UNM_NIC_GBE;
			}
			break;
		default:
			printk("%s: Unknown(%x)\n",unm_nic_driver_name,
					boardinfo->board_type);
			break;
	}

	return rv;
}

int
unm_nic_get_board_num(unm_adapter *adapter)
{
        return adapter->ahw.boardcfg.board_num;
}

#define MTU_FUDGE_FACTOR 100
int nx_nic_p2_set_mtu(struct unm_adapter_s *adapter, int new_mtu)
{
        int	ret = 0;

        new_mtu += MTU_FUDGE_FACTOR; /* so that MAC accepts frames > MTU */

	if (adapter->physical_port == 0) {
		NXWR32(adapter, UNM_NIU_XGE_MAX_FRAME_SIZE,
				new_mtu);
	} else {
		NXWR32(adapter, UNM_NIU_XG1_MAX_FRAME_SIZE,
				new_mtu);
	}

        return ret;
}

int
unm_nic_set_promisc_mode(struct unm_adapter_s *adapter)
{
        int                   ret;

	if (adapter->promisc)
		return 0;

	ret = unm_niu_xg_set_promiscuous_mode(adapter, UNM_NIU_PROMISCOUS_MODE);

	if (!ret)
		adapter->promisc = 1;

	return ret;
}

int unm_nic_unset_promisc_mode(struct unm_adapter_s *adapter)
{
	return (0);
}

#define UNM_UNICAST_ADDR(port, index) (UNM_UNICAST_ADDR_BASE+(port*32)+(index*8))

int
unm_nic_enable_mcast_filter(struct unm_adapter_s *adapter)
{
	__u32	val = 0;
	unm_mac_addr_cntl_t *ctl = (unm_mac_addr_cntl_t*)&val;
	__u16 port = adapter->physical_port;

	if (adapter->mc_enabled)
		return 0;

	//#ifndef NX_PORT_SHARING
	ctl->enable_pool = 0xf;
	//#endif

	ctl->mode_select = (adapter->ahw.board_type == UNM_NIC_XGBE) ? 3 : 0;

	ctl->id_pool_0 = 0;
	ctl->id_pool_1 = 1;
	ctl->id_pool_2 = 2;
	ctl->id_pool_3 = 3;

	ctl->enable_xtnd_0 = ctl->enable_xtnd_1 = 1;
	ctl->enable_xtnd_2 = ctl->enable_xtnd_3 = 1;

	NXWR32(adapter, UNM_MAC_ADDR_CNTL_REG, val);

	val = 0xffffff;
	NXWR32(adapter, UNM_UNICAST_ADDR(port,0), val);
	NXWR32(adapter, UNM_UNICAST_ADDR(port,0)+4, val);

	memcpy(&val, adapter->mac_addr, 3);
	NXWR32(adapter, UNM_UNICAST_ADDR(port,1), val);

	memcpy(&val, adapter->mac_addr+3, 3);
	NXWR32(adapter, UNM_UNICAST_ADDR(port,1)+4, val);

	adapter->mc_enabled = 1;
	return 0;
}

int
unm_nic_disable_mcast_filter(struct unm_adapter_s *adapter)
{
	__u32	val = 0;
	__u16 port = adapter->physical_port;

	if (!adapter->mc_enabled)
		return 0;

	NXWR32(adapter, UNM_MAC_ADDR_CNTL_REG, val);

	memcpy(&val, adapter->mac_addr, 3);
	NXWR32(adapter, UNM_UNICAST_ADDR(port,0), val);

	memcpy(&val, adapter->mac_addr+3, 3);
	NXWR32(adapter, UNM_UNICAST_ADDR(port,0)+4, val);

	adapter->mc_enabled = 0;
	return 0;
}

#define UNM_MCAST_ADDR(port, index)	(UNM_MULTICAST_ADDR_BASE+(port*0x80)+(index*8))

int
unm_nic_set_mcast_addr(struct unm_adapter_s *adapter, int index, __u8 *addr)
{
	__u32 hi = 0, lo = 0;
	__u16 port = adapter->physical_port;

	memcpy(&hi, addr, 3);
	memcpy(&lo, addr+3, 3);

	NXWR32(adapter, UNM_MCAST_ADDR(port,index), hi);
	NXWR32(adapter, UNM_MCAST_ADDR(port,index)+4, lo);

	return 0;
}

long unm_nic_init_port(struct unm_adapter_s *adapter)
{
	unm_nic_set_link_parameters(adapter);

	return (0);
}

void nx_nic_p2_stop_port(struct unm_adapter_s *adapter)
{
	unm_niu_disable_xg_port(adapter);
        return;
}

/* Functions required by UNM Hal routines */
unsigned long
unm_xport_lock(void)
{
        /* No lock necessary */
        return 0;
}

void DELAY(A)
{
        unsigned long remainder;

        remainder = A/50000;
        do {
                if (remainder > 1000) {
                        udelay(1000);
                        remainder -= 1000;
                } else {
                        udelay(remainder + 1);
                        remainder = 0;
                }
        } while (remainder > 0);
}

void unm_nic_set_link_parameters(struct unm_adapter_s *adapter)
{
	unm_niu_phy_status_t status;
	uint32_t autoneg;
	u32 port_mode = 0;
	uint32_t rv;
	uint64_t cur_fn = (uint64_t) unm_nic_set_link_parameters;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);

	if (!netif_carrier_ok(adapter->netdev)) {
		adapter->link_speed   = 0;
		adapter->link_duplex  = -1;
		adapter->link_autoneg = (port_mode == UNM_PORT_MODE_802_3_AP) ? 
			AUTONEG_DISABLE : AUTONEG_ENABLE;
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return;
	}

	/*
	 * For cards using kr phy, port_mode may not be set correctly
	 * if link is still down. When link is up, this function will
	 * be called again, and the speed will be set correctly
	 */

	if (port_mode == UNM_PORT_MODE_802_3_AP) {
		adapter->state        = PORT_UP;
		adapter->link_speed   = SPEED_1000;
		adapter->link_duplex  = DUPLEX_FULL;
		adapter->link_autoneg = AUTONEG_DISABLE;
		NX_NIC_TRC_FN(adapter, cur_fn, port_mode);
		return;
	}

	if(adapter->ahw.board_type != UNM_NIC_GBE) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->ahw.board_type);
		return;
	}

	rv = nx_fw_cmd_query_phy(adapter, adapter->portnum,
			UNM_NIU_GB_MII_MGMT_ADDR_PHY_STATUS,
			(unm_crbword_t *)&status);
	if ((rv == 0) && status.link) {
		if (nx_fw_cmd_query_phy(adapter, adapter->portnum,
					UNM_NIU_GB_MII_MGMT_ADDR_AUTONEG,
					&autoneg)!= 0) {
			adapter->link_autoneg = autoneg;
		}
	}

	if (rv == 0) {
		if(status.link) {
			adapter->state = 1;
			switch(status.speed) {
				case 0: adapter->link_speed = SPEED_10;
						break;
				case 1: adapter->link_speed = SPEED_100;
						break;
				case 2: adapter->link_speed = SPEED_1000;
						break;
				default:
						adapter->link_speed = -1; // unknown speed
						break;
			}
			switch(status.duplex) {
				case 0: adapter->link_duplex = DUPLEX_HALF;
						break;
				case 1: adapter->link_duplex = DUPLEX_FULL;
						break;
				default:
						adapter->link_duplex = -1; // unknown mode
						break;
			}
		} else {
			adapter->state = -1;
			adapter->link_speed = -1;
			adapter->link_duplex = -1;
		}
	} else {
		adapter->state = -1;
		adapter->link_speed = -1;
		adapter->link_duplex = -1;
	}
	NX_NIC_TRC_FN(adapter, cur_fn, status.link);
	NX_NIC_TRC_FN(adapter, cur_fn, rv);
}

#define FLASH_SIZE      (0x100000)

int
unm_nic_get_flash_size(void)
{
        return FLASH_SIZE;
}

void unm_nic_get_serial_num(struct unm_adapter_s *adapter)
{
	unm_user_info_t user_info;
	int              i;
	int              addr = USER_START;
	uint32_t         *ptr32;

	ptr32 = (U32 *)&user_info;
	for (i=0; i<sizeof(unm_user_info_t)/sizeof(uint32_t); i++) {
		if (rom_fast_read(adapter, addr, ptr32) == -1) {
			printk("%s: ERROR reading %s board userarea.\n",
					unm_nic_driver_name, unm_nic_driver_name);
			return;
		}
		ptr32++;
		addr+= sizeof(uint32_t);
	}

	memcpy(adapter->id, user_info.serial_num,sizeof(user_info.serial_num));
#if 0
	printk("%s adapter_id=%x size=%x\n",__FUNCTION__,adapter->id,sizeof(user_info.serial_num));
	for(i = 0; i < 32;i++) {
		printk("%x ",(unsigned char)*(adapter->id+i));
	}
#endif
	//      printk("%s: adapter_id=%x\n",__FUNCTION__,adapter->id);
}

void unm_nic_flash_print(struct unm_adapter_s* adapter)
{
	int valid = 1;
	
	unm_board_info_t* board_info = &(adapter->ahw.boardcfg);
	if (board_info->magic != UNM_BDINFO_MAGIC) {
		printk("%s UNM Unknown board config, Read 0x%x expected as 0x%x\n",
			unm_nic_driver_name,board_info->magic,UNM_BDINFO_MAGIC);
		valid = 0;
	}
	if (board_info->header_version != UNM_BDINFO_VERSION) {
		printk("%s UNM Unknown board config version."
			" Read %x, expected %x\n",unm_nic_driver_name,
			board_info->header_version, UNM_BDINFO_VERSION);
		valid = 0;
	}
	if(valid) {
        char            *brd_name = NULL;
        unm_user_info_t  user_info;
        int              i;
        int              addr = USER_START;
        uint32_t         *ptr32;

        ptr32 = (U32 *)&user_info;
        for (i=0; i<sizeof(unm_user_info_t)/sizeof(uint32_t); i++) {
             if (rom_fast_read(adapter, addr, ptr32) == -1) {
                 printk("%s: ERROR reading %s board userarea.\n",
                         unm_nic_driver_name, unm_nic_driver_name);
                 return;
             }
             ptr32++;
             addr+= sizeof(uint32_t);
        }
        GET_BRD_NAME_BY_TYPE(board_info->board_type, brd_name);
        printk(KERN_INFO "%s %s Board S/N %s  Chip id 0x%x\n",
                        unm_nic_driver_name, brd_name, user_info.serial_num,
                        board_info->chip_id);

	}
}

int nx_flash_read_version(struct unm_adapter_s *adapter, __uint32_t offset,
			  nic_version_t *version)
{
	__uint32_t	tmp;

	if (!(rom_fast_read(adapter, offset, (int *)&tmp))) {
		version->major = tmp & 0xff;
         	version->minor = (tmp >> 8) & 0xff;
		version->sub = tmp >> 16;
		return (0);
	}

	return (-1);
}
