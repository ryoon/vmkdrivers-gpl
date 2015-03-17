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
 * Source file for NIC routines to initialize the Phantom Hardware
 *
 * $Id: unm_nic_init.c,v 1.4 2012/04/12 10:49:19 narender Exp $
 *
 */
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include "unm_nic.h"
#include "unm_nic_hw.h"
#include "nic_cmn.h"
#include "nic_phan_reg.h"
#include "unm_nic_ioctl.h"
#include "nic_phan_reg.h"
#include "unm_version.h"
#include "unm_brdcfg.h"
#include <linux/firmware.h>
#include "p3_cut_thru_nx_fw_hdr.h"
#include "p3_legacy_nx_fw_hdr.h"

#define HP_CLOCK_FIX_FW NX_FW_VERSION(4, 0, 554)

struct crb_addr_pair {
        long addr;
        long data;
};

#define MAX_CRB_XFORM 60
static unsigned int crb_addr_xform[MAX_CRB_XFORM];
#define ADDR_ERROR ((unsigned long ) 0xffffffff )
int crb_table_initialized=0;

#define crb_addr_transform(name) \
        crb_addr_xform[UNM_HW_PX_MAP_CRB_##name] = \
        UNM_HW_CRB_HUB_AGT_ADR_##name << 20
static void
crb_addr_transform_setup(void)
{
        crb_addr_transform(XDMA);
        crb_addr_transform(TIMR);
        crb_addr_transform(SRE);
        crb_addr_transform(SQN3);
        crb_addr_transform(SQN2);
        crb_addr_transform(SQN1);
        crb_addr_transform(SQN0);
        crb_addr_transform(SQS3);
        crb_addr_transform(SQS2);
        crb_addr_transform(SQS1);
        crb_addr_transform(SQS0);
        crb_addr_transform(RPMX7);
        crb_addr_transform(RPMX6);
        crb_addr_transform(RPMX5);
        crb_addr_transform(RPMX4);
        crb_addr_transform(RPMX3);
        crb_addr_transform(RPMX2);
        crb_addr_transform(RPMX1);
        crb_addr_transform(RPMX0);
        crb_addr_transform(ROMUSB);
        crb_addr_transform(SN);
        crb_addr_transform(QMN);
        crb_addr_transform(QMS);
        crb_addr_transform(PGNI);
        crb_addr_transform(PGND);
        crb_addr_transform(PGN3);
        crb_addr_transform(PGN2);
        crb_addr_transform(PGN1);
        crb_addr_transform(PGN0);
        crb_addr_transform(PGSI);
        crb_addr_transform(PGSD);
        crb_addr_transform(PGS3);
        crb_addr_transform(PGS2);
        crb_addr_transform(PGS1);
        crb_addr_transform(PGS0);
        crb_addr_transform(PS);
        crb_addr_transform(PH);
        crb_addr_transform(NIU);
        crb_addr_transform(I2Q);
        crb_addr_transform(EG);
        crb_addr_transform(MN);
        crb_addr_transform(MS);
        crb_addr_transform(CAS2);
        crb_addr_transform(CAS1);
        crb_addr_transform(CAS0);
        crb_addr_transform(CAM);
        crb_addr_transform(C2C1);
        crb_addr_transform(C2C0);
        crb_addr_transform(SMB);
        crb_addr_transform(OCM0);
	/*
	 * Used only in P3 just define it for P2 also.
	 */
	crb_addr_transform(I2C0);

    	crb_table_initialized = 1;
}

/*
 * decode_crb_addr(0 - utility to translate from internal Phantom CRB address
 * to external PCI CRB address.
 */
unsigned long
decode_crb_addr (unsigned long addr)
{
	int i;
	unsigned long base_addr, offset, pci_base;

	if (!crb_table_initialized)
		crb_addr_transform_setup();

	pci_base = ADDR_ERROR;
	base_addr = addr & 0xfff00000;
	offset = addr & 0x000fffff;

	for (i=0; i< MAX_CRB_XFORM; i++) {
		if (crb_addr_xform[i] == base_addr) {
			pci_base = i << 20;
			break;
		}
	}
	if (pci_base == ADDR_ERROR) {
		return pci_base;
	} else {
		return (pci_base + offset);
	}
}

unsigned long
crb_pci_to_internal(unsigned long addr)
{
	addr -= UNM_PCI_CRBSPACE;
	if (!crb_table_initialized)
	    	crb_addr_transform_setup();
	return (crb_addr_xform[((addr >> 20) & 0x3f)] + (addr & 0xfffff));
}

static long rom_max_timeout= 100;
static long rom_lock_timeout= 10000;

int
rom_lock(unm_adapter *adapter)
{
	int i;
	int done = 0, timeout = 0;

	while (!done) {
		/* acquire semaphore2 from PCI HW block */
		done = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM2_LOCK));
		if (done == 1)
			break;
		if (timeout >= rom_lock_timeout) {
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
	NXWR32(adapter, UNM_ROM_LOCK_ID, ROM_LOCK_DRIVER);
	return 0;
}

int
rom_unlock(unm_adapter *adapter)
{
    /* release semaphore2 */
    NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM2_UNLOCK));

    return 0;
}

int
wait_rom_done (unm_adapter *adapter)
{
        long timeout=0;
        long done=0 ;

        /*DPRINTK(1,INFO,"WAIT ROM DONE \n");*/
        /*printk(KERN_INFO "WAIT ROM DONE \n");*/

        while (done == 0) {
                done = NXRD32(adapter, UNM_ROMUSB_GLB_STATUS);
                done &=2;
                timeout++;
                if (timeout >= rom_max_timeout) {
                        printk( "%s: Timeout reached  waiting for rom done",
					unm_nic_driver_name);
                        return -1;
                }
        }
        return 0;
}

int
do_rom_fast_read (unm_adapter *adapter, int addr, int *valp)
{
        /*DPRINTK(1,INFO,"ROM FAST READ \n");*/
        NXWR32(adapter, UNM_ROMUSB_ROM_ADDRESS, addr);
        NXWR32(adapter, UNM_ROMUSB_ROM_ABYTE_CNT, 3);
        udelay(10);
        NXWR32(adapter, UNM_ROMUSB_ROM_DUMMY_BYTE_CNT, 0);
        NXWR32(adapter, UNM_ROMUSB_ROM_INSTR_OPCODE, 0xb);
        if (wait_rom_done(adapter)) {
                printk("%s: Error waiting for rom done\n",unm_nic_driver_name);
                return -1;
        }
        //reset abyte_cnt and dummy_byte_cnt
        NXWR32(adapter, UNM_ROMUSB_ROM_DUMMY_BYTE_CNT, 0);
        udelay(10);
        NXWR32(adapter, UNM_ROMUSB_ROM_ABYTE_CNT, 0);

        *valp = NXRD32(adapter, UNM_ROMUSB_ROM_RDATA);
        return 0;
}

static int do_rom_fast_read_words(unm_adapter *adapter, int addr,
                  u8 *bytes, size_t size)
{
    int addridx;
    int ret = 0;

    for (addridx = addr; addridx < (addr + size); addridx += 4) {
        int v;
        ret = do_rom_fast_read(adapter, addridx, &v);
        if (ret != 0)
            break;

        if (bytes) {
            *(__le32 *)bytes = cpu_to_le32(v);
            bytes += 4;
        }
    }

    return ret;
}

int
nx_nic_rom_fast_read_words(unm_adapter *adapter, int addr,
                u8 *bytes, size_t size)
{
    int ret;

    ret = rom_lock(adapter);
    if (ret < 0)
        return ret;

    ret = do_rom_fast_read_words(adapter, addr, bytes, size);

    rom_unlock(adapter);
    return ret;
}


int
rom_fast_read (struct unm_adapter_s *adapter, int addr, int *valp)
{
    int ret, loops = 0;

    while ((rom_lock(adapter) != 0) && (loops < 50000)) {
        udelay(100);
        schedule();
        loops++;
    }
    if (loops >= 50000) {
        printk("%s: rom_lock failed\n",unm_nic_driver_name);
        return -1;
    }
    ret = do_rom_fast_read(adapter, addr, valp);
    rom_unlock(adapter);
    return ret;
}

/* Error codes */
#define FPGA_NO_ERROR 0
#define FPGA_FILE_ERROR 1
#define FPGA_ILLEGAL_PARAMETER 2
#define FPGA_MEMORY_ERROR 3

/* macros */
#ifdef VIRTEX
#define RESET_STATE  changeState(adapter,1,32)
#else
#define RESET_STATE  changeState(adapter,1,5)
#endif

#define GO_TO_RTI RESET_STATE;\
                  changeState(adapter,0,1)

/*
 *  TCLK = GPIO0 = 0x60
 *  TRST = GPIO1 = 0x64
 *  TMS  = GPIO4 = 0x70
 *  TDO  = GPIO5 = 0xc
 *  TDI  = GPIO8 = 0x80
 *
 */
#define TCLK (UNM_CRB_ROMUSB + 0x60)
#define TRST (UNM_CRB_ROMUSB + 0x64)
#define TMS  (UNM_CRB_ROMUSB + 0x70)
#define TDI  (UNM_CRB_ROMUSB + 0x80)
#define TDO  (UNM_CRB_ROMUSB + 0xc)

#define TDO_SHIFT 5

#define ASSERT_TRST   \
        do { \
                NXWR32(adapter, TRST, 2);\
                NXWR32(adapter, TMS, 2); \
                NXWR32(adapter, TDI, 2); \
                NXWR32(adapter, TCLK, 2);\
                NXWR32(adapter, TRST, 3);\
        } while (0)

#define CLOCK_IN_BIT(tdi,tms)  \
        do { \
                NXWR32(adapter, TRST, 3); \
                NXWR32(adapter, TMS, 2 | (tms)); \
                NXWR32(adapter, TDI, 2 | (tdi)); \
                NXWR32(adapter, TCLK, 2); \
                NXWR32(adapter, TCLK, 3); \
                NXWR32(adapter, TCLK, 2);\
        } while (0)

#define CLOCK_OUT_BIT(bit,tms) \
        do { \
                NXWR32(adapter, TRST, 3); \
                NXWR32(adapter, TMS, 2 | (tms)); \
                NXWR32(adapter, TDI, 2); \
                NXWR32(adapter, TCLK, 2); \
                NXWR32(adapter, TCLK, 3); \
                NXWR32(adapter, TCLK, 2); \
                (bit) = (NXRD32(adapter, TDO) >> TDO_SHIFT) & 1; \
        } while (0)

/* boundary scan instructions */
#define CMD_EXTEST    0x0
#define CMD_CAPTURE   0x1
#define CMD_IDCODE    0x2
#define CMD_SAMPLE    0x3

//Memory BIST
#define CMD_MBSEL     0x4
#define CMD_MBRES     0x5

//Logic BIST
#define CMD_LBSEL     0x6
#define CMD_LBRES     0x7
#define CMD_LBRUN    0x18

//Memory Interface
#define CMD_MEM_WDAT  0x8
#define CMD_MEM_ACTL  0x9
#define CMD_MEM_READ  0xa

//Memory Interface
#define CMD_CRB_WDAT  0xb
#define CMD_CRB_ACTL  0xc
#define CMD_CRB_READ  0xd

#define CMD_TRISTATE  0xe
#define CMD_CLAMP     0xf

#define CMD_STATUS    0x10
#define CMD_XG_SCAN   0x11
#define CMD_BYPASS    0x1f

#ifdef VIRTEX
#define CMD_LENGTH_BITS 6
#else
#define CMD_LENGTH_BITS 5
#endif


/* This is the TDI bit that will be clocked out for don't care value */
#define TDI_DONT_CARE 0

#define TMS_LOW  0
#define TMS_HIGH 1

#define ID_REGISTER_SIZE_BITS 32

#define EXIT_IR 1
#define NO_EXIT_IR 0

#define TAP_DELAY()

__inline__ void
changeState(unm_adapter *adapter,int tms,int tck)
{
        int i;
        DPRINTK(1, INFO, "changing state tms: %d tck: %d\n", tms, tck);
        for (i = 0; i< tck; i++)
        {
                CLOCK_IN_BIT(TDI_DONT_CARE,tms);
        }
}

// Assumes we are in RTI, will return to RTI
__inline__ int
loadInstr(unm_adapter *adapter,int instr,int exit_ir)
{
        int i,j;

        DPRINTK(1, INFO, "in loaderinstr instr: %d exit_ir %d\n", instr,
                                exit_ir);

        // go to Select-IR
        changeState(adapter,1,2);

        // go to Shift-IR
        changeState(adapter,0,2);

#ifdef VIRTEX
        for (i = 0; i< (CMD_LENGTH_BITS * 7); i++)
        {
                CLOCK_IN_BIT(1,TMS_LOW);
        }
#endif
        for (i = 0; i< (CMD_LENGTH_BITS-1); i++)
        {
                j= (instr>>i) & 1;
                CLOCK_IN_BIT(j,TMS_LOW);
        }

        /* clock out last bit, and transition to next state */
        j= (instr >> (CMD_LENGTH_BITS-1)) & 1;
        // last bit, exit into Exit1-IR
        CLOCK_IN_BIT(j,1);
        // go to Update-IR
        changeState(adapter,1,1);
        // go to RTI
        changeState(adapter,0,1);
        return FPGA_NO_ERROR;
}

int
getData(unm_adapter *adapter,u32 *data,int len, int more)
{
        u32 temp=0;
        int i, bit;
        DPRINTK(1, INFO, "doing getData data: %p len: %d more: %d\n",
                                data, len,more);
        // go to Select-DR-Scan
        changeState(adapter,1,1);
        // go to shift-DR
        changeState(adapter,0,1);
#ifdef VIRTEX
        // dummy reads
        for (i=0; i< 6; i++) {
                CLOCK_OUT_BIT(bit,0);
        }
#endif
        for (i=0; i< (len); i++) {
                CLOCK_OUT_BIT(bit,0);
                temp |= (bit << i);
        }
        if (!more) {
                // go to Exit1-DR
                changeState(adapter,1,1);
                // go to Update DR
                changeState(adapter,1,1);
                // go to RTI
                changeState(adapter,0,1);
        }

        *data = temp;

        return 0;
}


int
get_status(unm_adapter *adapter)
{
        int status;

        DPRINTK(1, INFO, "doing get_status: %p\n", adapter);
        // tap_inst_wr(INST_STATUS)
        loadInstr(adapter,CMD_STATUS,EXIT_IR);
        getData(adapter,&status,1,0);
        //printf("Status: 0x%02x\n",status);
        return status;
}
// assumes start in RTI, will return to RTI unless more is set
int
getData64(unm_adapter *adapter,u64 *data,int len, int more)
{
        u64 temp=0;
        u64 i, bit;
        DPRINTK(1, INFO, "getData64 data %p, len %d more %d\n",
                                data, len, more);
        // go to Select-DR-Scan
        changeState(adapter,1,1);
        // go to shift-DR
        changeState(adapter,0,1);
#ifdef VIRTEX
        // dummy reads
        for (i=0; i< 6; i++) {
                CLOCK_OUT_BIT(bit,0);
        }
#endif
        for (i=0; i< (len); i++) {
                CLOCK_OUT_BIT(bit,0);
                temp |= (bit << i);
        }

        if (!more) {
                // go to Exit1-DR
                changeState(adapter,1,1);
                // go to Update DR
                changeState(adapter,1,1);
                // go to RTI
                changeState(adapter,0,1);
        }

        //temp |= (bit << (len - 1));
        //printf("Data read: 0x%016llx\n",temp);
        *data = temp;
        return 0;
}

// assumes start in shift_DR, will return to RTI unless more is set
int
loadData (unm_adapter *adapter,u64 data, int len, int more)
{
        int i, bit;

        DPRINTK(1, INFO, "loading data %llx data %d more %d\n",
                data, len, more);
        for (i=0; i< (len-1); i++) {
                bit = (data >> i) & 1;
                CLOCK_IN_BIT(bit,0);
        }
        if (more) {
                bit = (data >> (len-1)) & 1;
                CLOCK_IN_BIT(bit,0);
        } else {
                bit = (data>>(len - 1)) & 1;
                // last data, go to Exit1-DR
                CLOCK_IN_BIT(bit,1);
                // go to Update DR
                changeState(adapter,1,1);
                // go to RTI
                changeState(adapter,0,1);
        }

        return 0;
}

#define CRB_REG_EX_PC                   0x3c

void nx_msleep(unsigned long msecs)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout((HZ * msecs + 999) / 1000);
}

int pinit_from_rom(unm_adapter *adapter, int verbose)
{
	int addr, val,status;
	int i ;
	int init_delay=0;
	struct crb_addr_pair *buf;
	unsigned long off;
	unsigned offset, n;

	struct crb_addr_pair {
		long addr;
		long data;
	};

	// resetall
	status = unm_nic_get_board_info(adapter);
	if (status)
		printk ("%s: pinit_from_rom: Error getting board info\n",
				unm_nic_driver_name);

	rom_lock(adapter);	/* Grab the lock so that no one can read
						 * flash when we reset the chip. */
	NXWR32(adapter, UNM_ROMUSB_GLB_SW_RESET, 0xffffffff);
	rom_unlock(adapter);	/* Just in case it was held when we
							 * reset the chip */

	if (verbose) {
		int val;
		if (rom_fast_read(adapter, 0x4008, &val) == 0)
			printk("ROM board type: 0x%08x\n", val);
		else
			printk("Could not read board type\n");
		if (rom_fast_read(adapter, 0x400c, &val) == 0)
			printk("ROM board  num: 0x%08x\n", val);
		else
			printk("Could not read board number\n");
		if (rom_fast_read(adapter, 0x4010, &val) == 0)
			printk("ROM chip   num: 0x%08x\n", val);
		else
			printk("Could not read chip number\n");
	}

	if (rom_fast_read(adapter, 0, &n) != 0 || n != 0xcafecafeUL ||
			rom_fast_read(adapter, 4, &n) != 0) {
		printk("NX_NIC: ERROR Reading crb_init area: "
				"n: %08x\n", n);
		return -1;
	}

	offset = n & 0xffffU;
	n = (n >> 16) & 0xffffU;

	if (n  >= 1024) {
		printk("%s: %s:n=0x%x Error! UNM card flash not initialized.\n",
				unm_nic_driver_name,__FUNCTION__, n);
		return -1;
	}
	if (verbose)
		printk("%s: %d CRB init values found in ROM.\n",
				unm_nic_driver_name, n);

	buf = kmalloc(n*sizeof(struct crb_addr_pair), GFP_KERNEL);
	if (buf==NULL) {
		printk("%s: pinit_from_rom: Unable to malloc memory.\n",
				unm_nic_driver_name);
		return -1;
	}
	for (i=0; i< n; i++) {
		if (rom_fast_read(adapter, 8*i + 4*offset, &val) != 0 ||
				rom_fast_read(adapter, 8*i + 4*offset + 4, &addr) != 0) {
			kfree(buf);
			return -1;
		}

		buf[i].addr=addr;
		buf[i].data=val;

		if (verbose)
			printk("%s: PCI:     0x%08x == 0x%08x\n",
					unm_nic_driver_name,
					(unsigned int)decode_crb_addr(
						(unsigned long)addr), val);
	}

	for (i=0; i< n; i++) {
		off = decode_crb_addr((unsigned long)buf[i].addr) +
			UNM_PCI_CRBSPACE;
		/* skipping cold reboot MAGIC */
		if (off == UNM_CAM_RAM(0x1fc)) {
			continue;
		}

		/* do not reset PCI */
		if (off == (ROMUSB_GLB + 0xbc)) {
			continue;
		}
		if (off == (UNM_CRB_PEG_NET_1 + 0x18)) {
			buf[i].data = 0x1020;
		}
		/* skip the function enable register */
		if (off == UNM_PCIE_REG(PCIE_SETUP_FUNCTION)) {
			continue;
		}
		if (off == UNM_PCIE_REG(PCIE_SETUP_FUNCTION2)) {
			continue;
		}
		if ((off & 0x0ff00000) == UNM_CRB_SMB) {
			continue;
		}
		if ((off & 0x0ff00000) == UNM_CRB_DDR_NET) {
			continue;
		}

		if (off == ADDR_ERROR) {
			printk("%s: Err: Unknown addr: 0x%08lx\n",
					unm_nic_driver_name, buf[i].addr);
			continue;
		}

		/* After writing this register, HW needs time for CRB */
		/* to quiet down (else crb_window returns 0xffffffff) */
		if (off == UNM_ROMUSB_GLB_SW_RESET) {
			init_delay=1;
		}

		NXWR32(adapter, off, buf[i].data);

		if (init_delay==1) {
			nx_msleep(1000);
			init_delay=0;
		}

		nx_msleep(1);
	}
	kfree(buf);

	// p2dn replyCount
	NXWR32(adapter, UNM_CRB_PEG_NET_D+0xec, 0x1e);
	// disable_peg_cache 0
	NXWR32(adapter, UNM_CRB_PEG_NET_D+0x4c,8);
	// disable_peg_cache 1
	NXWR32(adapter, UNM_CRB_PEG_NET_I+0x4c,8);

	// peg_clr_all
	// peg_clr 0
	NXWR32(adapter, UNM_CRB_PEG_NET_0+0x8,0);
	NXWR32(adapter, UNM_CRB_PEG_NET_0+0xc,0);
	// peg_clr 1
	NXWR32(adapter, UNM_CRB_PEG_NET_1+0x8,0);
	NXWR32(adapter, UNM_CRB_PEG_NET_1+0xc,0);
	// peg_clr 2
	NXWR32(adapter, UNM_CRB_PEG_NET_2+0x8,0);
	NXWR32(adapter, UNM_CRB_PEG_NET_2+0xc,0);
	// peg_clr 3
	NXWR32(adapter, UNM_CRB_PEG_NET_3+0x8,0);
	NXWR32(adapter, UNM_CRB_PEG_NET_3+0xc,0);

	return 0;
}


int check_for_bad_spd(struct unm_adapter_s *adapter)
{
	u32 val = 0;
	uint64_t cur_fn = (uint64_t) check_for_bad_spd;

	val = NXRD32(adapter,
			BOOT_LOADER_DIMM_STATUS) & NX_BOOT_LOADER_MN_ISSUE;
	if (val & NX_PEG_TUNE_MN_SPD_ZEROED) {
		nx_nic_print1(NULL,
			"Memory DIMM SPD not programmed.  Assumed valid.\n");
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
		return 1;
	} else if (val) {
		nx_nic_print1(NULL,
			"Memory DIMM type incorrect.  Info:%08X.\n", val);
		NX_NIC_TRC_FN(adapter, cur_fn, 2);
		return 2;
	}
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}


#define CACHE_DISABLE 1
#define CACHE_ENABLE  0

int nx_phantom_init(struct unm_adapter_s *adapter, int pegtune_val)
{
	u32 val = 0;
	int retries = 60;
	uint64_t cur_fn = (uint64_t) nx_phantom_init;

	if (!pegtune_val) {
		NX_NIC_TRC_FN(adapter, cur_fn, pegtune_val);
		do {
			val = NXRD32(adapter,
					CRB_CMDPEG_STATE);
			if ((val == PHAN_INITIALIZE_COMPLETE) ||
			    (val == PHAN_INITIALIZE_ACK))
				return 0;

			msleep(500);

		} while (--retries);

		check_for_bad_spd(adapter);

		if (!retries) {
			pegtune_val = NXRD32(adapter,
					UNM_ROMUSB_GLB_PEGTUNE_DONE);
			printk(KERN_WARNING "%s: init failed, "
				"pegtune_val = %x\n", __FUNCTION__, pegtune_val);
			NX_NIC_TRC_FN(adapter, cur_fn, pegtune_val);
			return -1;
		}
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

int
load_from_flash(struct unm_adapter_s *adapter)
{
	int  i;
	long size = 0;
	long flashaddr = BOOTLD_START, memaddr = BOOTLD_START;
	u64 data;
	u32 high, low;
	size = (IMAGE_START - BOOTLD_START)/8;
	int rv = 0;

	for (i = 0; i < size; i++) {
		if (rom_fast_read(adapter, flashaddr, (int *)&low) != 0) {
			DPRINTK(1,ERR, "Error in rom_fast_read(). Will skip"
					"loading flash image\n");
			return -1;
		}
		if (rom_fast_read(adapter, flashaddr + 4, (int *)&high) != 0) {
			DPRINTK(1,ERR, "Error in rom_fast_read(). Will skip"
					"loading flash image\n");
			return -1;
		}
		data = ((u64)high << 32 ) | low ;
		rv = adapter->unm_nic_pci_mem_write(adapter, memaddr, &data, 8);
		if(rv != 0) {
			return rv;
		}
		flashaddr += 8;
		memaddr   += 8;
		if(i%0x1000==0){
			nx_msleep(1);
		}
	}
	udelay(100);
	read_lock(&adapter->adapter_lock);

	NXWR32(adapter, UNM_ROMUSB_GLB_SW_RESET, 0x80001d);

	read_unlock(&adapter->adapter_lock);
	return 0;
}

/*
 * Determines which firmware is the newer one.
 * Returns -ve if ver1 is later one.
 * Returns +ve if ver2 is later one.
 * else returns zero.
*/
static int compare_fw_version(nic_version_t *ver1, nic_version_t *ver2)
{
        int     rv;

        rv = (int)ver1->major - (int)ver2->major;
        if (rv) {
                return (rv);
        }

        rv = (int)ver1->minor - (int)ver2->minor;
        if (rv) {
                return (rv);
        }

        rv = (int)ver1->sub - (int)ver2->sub;
        return (rv);
}

/*
 * Checks if firmware is supported by the driver by version comparision.
 */
static inline int nx_fw_hw_version_check(nic_version_t *version)
{
        int i = 0;
        int num_elems = sizeof(NX_SUPPORTED_FW_VERSIONS) /
                                sizeof(nic_version_t);

        for (; i < num_elems; i++) {
                if (version->major == NX_SUPPORTED_FW_VERSIONS[i].major &&
                    version->minor == NX_SUPPORTED_FW_VERSIONS[i].minor)
                        return 0;
        }
        return -1;
}

static int nx_request_fw(struct unm_adapter_s *adapter,
		__uint32_t fwtype, char *fname)
{
	struct nx_firmware *nx_fw = &adapter->nx_fw;
	switch(fwtype) {
		case NX_P3_MN_TYPE_ROMIMAGE :
			nx_fw->bootld = (uint64_t *)&p3_legacy_bootld;
			nx_fw->fw_img = (uint64_t *)&p3_legacy_fw_img;
			nx_fw->size = sizeof(p3_legacy_fw_img);
			adapter->bootld_size = sizeof(p3_legacy_bootld);
			nx_fw->fw_version = P3_LEGACY_FUSED_FW_VER;
			break;
		case NX_P3_CT_TYPE_ROMIMAGE :
			nx_fw->bootld = (uint64_t *)&p3_cut_thru_bootld;
			nx_fw->fw_img = (uint64_t *)&p3_cut_thru_fw_img;
			nx_fw->size = sizeof(p3_cut_thru_fw_img);
			adapter->bootld_size = sizeof(p3_cut_thru_bootld);
			nx_fw->fw_version = P3_CUT_THRU_FUSED_FW_VER;
			break;
		default                    :
			return -1;
                        }
	adapter->nx_fw.file_fw_state = NX_FILE_FW_READ;
	return 0;
}


void nx_release_fw(struct nx_firmware *nx_fw)
{
	nx_fw->file_fw_state = NX_FILE_FW_RELEASED;
}

static inline void fwfile_read_version(const struct nx_firmware *fw,
		__uint32_t offset,
		nic_version_t *version)
{
	__uint32_t      file_ver;
	if(offset == FW_VERSION_OFFSET)
		file_ver = fw->fw_version;
	else if (offset == FW_BIOS_VERSION_OFFSET)
		file_ver = 0;
	file_ver = cpu_to_le32(file_ver);
	version->major = file_ver & 0xff;
	version->minor = (file_ver >> 8) & 0xff;
	version->sub   = file_ver >> 16;
}

static int do_load_fw_file(struct unm_adapter_s *adapter,
			    struct nx_firmware *nx_fw)
{
	int	i, size=0;
	long	memaddr = BOOTLD_START;
	long	imgaddr = IMAGE_START;
	u64	data;
	int	rv = 0;

	size = adapter->bootld_size;

	UNM_ROUNDUP(size, 8);
	for (i = 0; i < (size/8); i++) {
		data = nx_fw->bootld[i]; //read 8 bytes as an u64
		data = cpu_to_le64(data);
		rv = adapter->unm_nic_pci_mem_write(adapter, memaddr, &data, 8);
		if(rv != 0) {
			return rv;
		}
		memaddr   += 8;
	}

	size = nx_fw->size;//read 4 bytes into size
	size = cpu_to_le32(size);
	for(i = 0; i < (size/8); i++) {
		data = nx_fw->fw_img[i];
		data = cpu_to_le64(data);
		rv = adapter->unm_nic_pci_mem_write(adapter, imgaddr, &data, 8);
		if(rv != 0) {
			return rv;
		}
		imgaddr += 8;
	}
	//The size of firmware may not be a multiple of 8
	size %= 8;
	if(size){
		data = nx_fw->fw_img[i];
		data = cpu_to_le64(data);
		rv = adapter->unm_nic_pci_mem_write(adapter, imgaddr, &data, 8);
		if(rv != 0) {
			return rv;
		}
	}
	udelay(100);
	read_lock(&adapter->adapter_lock);
	NXWR32(adapter, UNM_CAM_RAM(0x1fc), UNM_BDINFO_MAGIC);

	NXWR32(adapter, UNM_ROMUSB_GLB_SW_RESET,
			0x80001d);

	read_unlock(&adapter->adapter_lock);
	return 0;
}


static int nx_get_file_firmware(struct unm_adapter_s *adapter,
					      __uint32_t fwtype,
					      nic_version_t *bios_ver,
					      nic_version_t *version)
{
	nic_version_t	file_bios_ver;
	char		*romimage_array[] = NX_ROMIMAGE_NAME_ARRAY;
	char		*fname = romimage_array[fwtype];

	struct nx_firmware *nx_fw = &adapter->nx_fw;
	int rv = 0;
	rv = nx_request_fw(adapter, fwtype, fname);
	if (rv) {
		nx_nic_print4(NULL, "FW[%s] load failed\n", fname);
		return rv;
	}

	fwfile_read_version(nx_fw, FW_VERSION_OFFSET, version);
	fwfile_read_version(nx_fw, FW_BIOS_VERSION_OFFSET, &file_bios_ver);

	/* If file version is >= 4.0.554 */
	if (NX_FW_VERSION(version->major, version->minor,
                          version->sub) >= HP_CLOCK_FIX_FW) {	
		/* If flash version is < 4.0.554 */
		if (NX_FW_VERSION(adapter->flash_ver.major, adapter->flash_ver.minor,
		 	adapter->flash_ver.sub) < HP_CLOCK_FIX_FW) {
			nx_nic_print3(NULL, "Incompatibility detected between driver and firmware version on flash \n"); 
			nx_nic_print3(NULL, "This configuration is  not recommended \n");
			nx_nic_print3(NULL, "Please update the firmware on flash immediately .\n");	
			nx_release_fw(nx_fw);
			return -1;
		}
	}

	if (nx_fw_hw_version_check(version) == -1) {
		nx_nic_print4(NULL, "Incompatible file FW[%s] "
			      "version[%u.%u.%u:%u], BIOS[%u.%u.%u]\n", fname,
			      version->major, version->minor, version->sub,
			      version->build,
			      bios_ver->major, bios_ver->minor, bios_ver->sub);
		nx_release_fw(nx_fw);
		return -1;
	}

	nx_nic_print7(NULL, "File FW[%s] version[%u.%u.%u:%u], "
		      "BIOS[%u.%u.%u]\n",
		      fname, version->major, version->minor, version->sub,
		      version->build, file_bios_ver.major, file_bios_ver.minor,
		      file_bios_ver.sub);

	return 0;
}

/*
 * This function works like this:
 *	1)	If cmp_versions == LOAD_FW_FROM_FLASH, flash firmware will be
 *		loaded, if compatible.
 *      2)      A firmware is compatible with the driver only when both's
 *		major and minor numbers are same or
 *		if f/w is (3.4.337) & driver is (4,0.xyz).
 *      3)      We will be doing the comparison of the versions of file and
 *		flash firmwares, only when they both are compatible with the
 *		driver & the cmp_versions == LOAD_FW_WITH_COMPARISON &
 *		file f/w is cut-through/nxromimg.bin.
 *	4)      If file f/w is not compatible, will try loading the flash
 *		firmware.
 */
int load_fw(struct unm_adapter_s *adapter, int cmp_versions)
{
	int		rv = 0, flash = 0;
	int		flash_compatible = 0;
	nic_version_t	file_ver = {0};
	nic_version_t	file_bios_ver;
	unsigned int	mn_present;
	__uint32_t	fwtype = NX_P3_CT_TYPE_ROMIMAGE;
	__uint32_t rst;
	uint64_t cur_fn = (uint64_t) load_fw;

	struct nx_firmware *nx_fw = &adapter->nx_fw;

	if (nx_fw->file_fw_state == NX_FILE_FW_READ) {
		fwtype = adapter->fwtype;
		fwfile_read_version(nx_fw, FW_VERSION_OFFSET, &file_ver);
		fwfile_read_version(nx_fw, FW_BIOS_VERSION_OFFSET,
				&file_bios_ver);
		nx_nic_print3(adapter, "Load stored FW\n");
		NX_NIC_TRC_FN(adapter, cur_fn, nx_fw->file_fw_state);
		goto do_load;
	}

	if (nx_fw_hw_version_check(&adapter->flash_ver) == 0) {
		flash_compatible = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, flash_compatible);
	}

	nx_nic_print4(NULL, "Flash Version: Firmware[%u.%u.%u], "
			"BIOS[%u.%u.%u]\n",
			adapter->flash_ver.major, adapter->flash_ver.minor,
			adapter->flash_ver.sub,
			adapter->bios_ver.major, adapter->bios_ver.minor,
			adapter->bios_ver.sub);

	if ((cmp_versions == LOAD_FW_FROM_FLASH)) {
		flash = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, flash);
		goto do_load;
	}      

	if (cmp_versions != LOAD_FW_CUT_THROUGH) {
		// Checking for presence of MN
		mn_present = NXRD32(adapter, NX_PEG_TUNE_CAPABILITY);
		if (mn_present & NX_PEG_TUNE_MN_PRESENT) {
			fwtype = NX_P3_MN_TYPE_ROMIMAGE;
			NX_NIC_TRC_FN(adapter, cur_fn, fwtype);
		} else {
			nx_nic_print4(NULL, "No memory on card. "
					"Load Cut through.\n");
			fwtype = NX_P3_CT_TYPE_ROMIMAGE;
			NX_NIC_TRC_FN(adapter, cur_fn, fwtype);
		}
	}

	rv = nx_get_file_firmware(adapter, fwtype, &adapter->bios_ver, &file_ver);
	if (!rv) {
		if (cmp_versions == LOAD_FW_WITH_COMPARISON &&
				flash_compatible) {
			if (compare_fw_version(&file_ver,
						&adapter->flash_ver) < 0) {
				flash = 1;
				NX_NIC_TRC_FN(adapter, cur_fn, flash);
			}
		}
	} else {
		flash = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, flash);
	}

do_load:
	NXWR32(adapter, CRB_CMDPEG_STATE, 0);
	pinit_from_rom(adapter, 0);
	udelay(500);

	/* at this point, QM is in reset. This could be a problem if there are
	 * incoming d* transition queue messages. QM/PCIE could wedge.
	 * To get around this, QM is brought out of reset.
	 */
	rst = NXRD32(adapter,
			UNM_ROMUSB_GLB_SW_RESET);

	/* unreset qm */
	rst &= ~(1 << 28);
	NXWR32(adapter, UNM_ROMUSB_GLB_SW_RESET,
			rst);

	if (flash) {
		if (nx_fw->file_fw_state == NX_FILE_FW_READ)
			nx_release_fw(nx_fw);
		if (!flash_compatible) {
			nx_nic_print3(NULL, "Flash version[%u.%u.%u] is "
					"incompatible with the driver.\n",
					adapter->flash_ver.major,
					adapter->flash_ver.minor,
					adapter->flash_ver.sub);
			adapter->driver_mismatch = 1;
			NX_NIC_TRC_FN(adapter, cur_fn, flash_compatible);
		}
		nx_nic_print4(NULL, "Loading the firmware from flash.\n");
		rv = load_from_flash(adapter);
		NX_NIC_TRC_FN(adapter, cur_fn, rv);
		return rv;
	} else {
		nx_nic_print4(NULL, "Loading firmware from file , version = %u.%u.%u\n",
				file_ver.major, file_ver.minor, file_ver.sub);
		rv = do_load_fw_file(adapter, &adapter->nx_fw);
		adapter->fwtype = fwtype;
		NX_NIC_TRC_FN(adapter, cur_fn, rv);
		return (rv);
	}
}
