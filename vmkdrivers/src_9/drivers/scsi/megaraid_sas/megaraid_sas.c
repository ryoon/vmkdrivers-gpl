/*
 *  Linux MegaRAID driver for SAS based RAID controllers
 *
 *  Copyright (c) 2009-2012  LSI Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  FILE: megaraid_sas.c
 *
 *  Authors: LSI Corporation
 *           Sreenivas Bagalkote
 *           Sumant Patro
 *           Bo Yang
 *           Adam Radford <linuxraid@lsi.com>
 *
 *  Send feedback to: <megaraidlinux@lsi.com>
 *
 *  Mail to: LSI Corporation, 1621 Barber Lane, Milpitas, CA 95035
 *     ATTN: Linuxraid
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uio.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#if defined(__VMKLNX__)
#include <linux/miscdevice.h>
#include <scsi/scsi_tcq.h>
#include "vmklinux_scsi.h"
#endif
#include <linux/poll.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "megaraid_sas_fusion.h"
#include "megaraid_sas.h"

#if defined(__VMKLNX__)
/* Throttle commands when mid-layer starts
 * retrying commands, issues resets, or aborts.
 */

#define THROTTLE_MAX_CMD_PER_LUN 16

/* Perc6i can't handle more than max cmd per lun with the current
 * VMware storage stack timeouts. 
 * Perc5i can handle up to 32, but not much more, for the same reason;
 * however, Perc6i is PPC chip, Perc5i is xScale.
 */
 
#define MAX_CMD_PER_LUN 128

#define TRACK_IS_BUSY(a,b)

#endif /* defined(__VMKLNX__) */
/*
 * Modules parameters
 */

/*
 * Fast driver load option, skip scanning for physical devices during load.
 * This would result in physical devices being skipped during driver load
 * time. These can be later added though, using /proc/scsi/scsi
 */
static unsigned int fast_load = 0;
module_param_named(fast_load, fast_load, int, 0);
MODULE_PARM_DESC(fast_load,
	"megasas: Faster loading of the driver, skips physical devices! \
	 (default=0)");

/*
 * Number of sectors per IO command
 * Will be set in megasas_init_mfi if user does not provide
 */
static unsigned int max_sectors = 0;
module_param_named(max_sectors, max_sectors, int, 0);
MODULE_PARM_DESC(max_sectors,
	"Maximum number of sectors per IO command");

/*
 * Number of cmds per logical unit
 */
static unsigned int cmd_per_lun = MEGASAS_DEFAULT_CMD_PER_LUN;
module_param_named(cmd_per_lun, cmd_per_lun, int, 0);
MODULE_PARM_DESC(cmd_per_lun,
	"Maximum number of commands per logical unit (default=128)");

static int msix_disable = 0;
module_param(msix_disable, int, S_IRUGO);
MODULE_PARM_DESC(msix_disable, "Disable MSI interrupt handling. Default: 0");

MODULE_LICENSE("GPL");
MODULE_VERSION(MEGASAS_VERSION);
MODULE_AUTHOR("megaraidlinux@lsi.com");
MODULE_DESCRIPTION("LSI MegaRAID SAS Driver");

/*
 * PCI ID table for all supported controllers
 */
static struct pci_device_id megasas_pci_table[] = {

	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1064R)},
	/* xscale IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078R)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078DE)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0079GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0073SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0071SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VERDE_ZCR)},
	/* xscale IOP, vega */
	{PCI_DEVICE(PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_PERC5)},
	/* xscale IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_FUSION)},
	/* Fusion */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_INVADER)},
	/* Invader */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_FURY)},
	/* Fury */
	{}
};

MODULE_DEVICE_TABLE(pci, megasas_pci_table);
#if !defined(__VMKLNX__)
static int megasas_mgmt_majorno;
#endif

static struct megasas_mgmt_info megasas_mgmt_info;
static struct fasync_struct *megasas_async_queue;
static DEFINE_MUTEX(megasas_async_queue_mutex);
static int megasas_poll_wait_aen;
static DECLARE_WAIT_QUEUE_HEAD ( megasas_poll_wait );
extern void
poll_wait(struct file *filp, wait_queue_head_t *q, poll_table *token);

spinlock_t poll_aen_lock;

u32 megasas_dbg_lvl;
static u32 dbg_print_cnt;

void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status);
int megasas_transition_to_ready(struct megasas_instance* instance, int ocr);
static int megasas_get_pd_list(struct megasas_instance *instance);
static int megasas_get_ld_list(struct megasas_instance *instance);
static int megasas_issue_init_mfi(struct megasas_instance *instance);
static int megasas_register_aen(struct megasas_instance *instance, u32 seq_num, u32 class_locale_word);
static u32 megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem * regs);
static int megasas_adp_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * reg_set);
static irqreturn_t megasas_isr(int irq, void *devp, struct pt_regs *regs);
static u32
megasas_init_adapter_mfi(struct megasas_instance *instance);
u32
megasas_build_and_issue_cmd(struct megasas_instance *instance, struct scsi_cmnd *scmd);
static void megasas_complete_cmd_dpc(unsigned long instance_addr);
void
megasas_release_fusion(struct megasas_instance *instance);
int
megasas_ioc_init_fusion(struct megasas_instance *instance);
void
megasas_free_cmds_fusion(struct megasas_instance *instance);
u8
megasas_get_map_info(struct megasas_instance *instance);
int
megasas_sync_map_info(struct megasas_instance *instance);
int
wait_and_poll(struct megasas_instance *instance, struct megasas_cmd *cmd);
void  megasas_reset_reply_desc(struct megasas_instance *instance);
u8 MR_ValidateMapInfo(struct megasas_instance *instance);
int megasas_reset_fusion(struct Scsi_Host *shost);
void megasas_fusion_ocr_wq(struct work_struct *arg);

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 21)
int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	int i, bars = 0;
	for (i = 0; i < PCI_NUM_RESOURCES; i++)
		if (pci_resource_flags(dev, i) & flags)
			bars |= (1 << i);
	return bars;
}

int pci_request_selected_regions(struct pci_dev *pdev, int bars,
				 const char *res_name)
{
	int i;

	for (i = 0; i < 6; i++)
		if (bars & (1 << i))
			if(pci_request_region(pdev, i, res_name))
				goto err_out;
	return 0;

err_out:
	while(--i >= 0)
		if (bars & (1 << i))
			pci_release_region(pdev, i);

	return -EBUSY;
}

void pci_release_selected_regions(struct pci_dev *pdev, int bars)
{
	int i;

	for (i = 0; i < 6; i++)
		if (bars & (1 << i))
			pci_release_region(pdev, i);
}
#endif

void
megasas_issue_dcmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	instance->instancet->fire_cmd(instance,
		cmd->frame_phys_addr, 0, instance->reg_set);
}

/**
 * megasas_get_cmd -	Get a command from the free pool
 * @instance:		Adapter soft state
 *
 * Returns a free command from the pool
 */
struct megasas_cmd *megasas_get_cmd(struct megasas_instance
						  *instance)
{
	unsigned long flags;
	struct megasas_cmd *cmd = NULL;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	if (!list_empty(&instance->cmd_pool)) {
		cmd = list_entry((&instance->cmd_pool)->next,
				 struct megasas_cmd, list);
		list_del_init(&cmd->list);
	} else {
		if (megasas_dbg_lvl)
			printk(KERN_ERR "megasas: Command pool empty!\n");
	}

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
	return cmd;
}

/**
 * megasas_return_cmd -	Return a cmd to free command pool
 * @instance:		Adapter soft state
 * @cmd:		Command packet to be returned to free command pool
 */
void
megasas_return_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	cmd->scmd = NULL;
	cmd->frame_count = 0;
	list_add_tail(&cmd->list, &instance->cmd_pool);

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}


/**
*	The following functions are defined for xscale 
*	(deviceid : 1064R, PERC5) controllers
*/

/**
 * megasas_enable_intr_xscale -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_xscale(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;

	writel(0, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_xscale -Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_xscale(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0x1f;

	regs = instance->reg_set;

	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_xscale - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_xscale(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_msg_0);
}
/**
 * megasas_clear_interrupt_xscale -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 status;
	u32 mfiStatus = 0;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_OB_INTR_STATUS_MASK)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
	if (status & MFI_XSCALE_OMR0_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	if (mfiStatus)
		writel(status, &regs->outbound_intr_status);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_status);
	
	return mfiStatus;
}

/**
 * megasas_fire_cmd_xscale -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_xscale(struct megasas_instance *instance, dma_addr_t frame_phys_addr,u32 frame_count, struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr >> 3)|(frame_count),
	       &(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_adp_reset_xscale -	For controller reset
 * @regs:				MFI register set
 */
static int 
megasas_adp_reset_xscale(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
        u32 i;
	u32 pcidata;
	writel(MFI_ADP_RESET, &regs->inbound_doorbell);

	for (i=0; i < 3; i++)
		msleep(1000); /* sleep for 3 secs */
	pcidata =0;
	pci_read_config_dword(instance->pdev, MFI_1068_PCSR_OFFSET, &pcidata);
	printk("pcidata = %x\n", pcidata);
	if (pcidata & 0x2) {
		printk("mfi 1068 offset read=%x\n", pcidata);
		pcidata &= ~0x2;
		pci_write_config_dword(instance->pdev, MFI_1068_PCSR_OFFSET, pcidata);

		for (i=0; i<2; i++)
			msleep(1000); /* need to wait 2 secs again */

		pcidata =0;
		pci_read_config_dword(instance->pdev, MFI_1068_FW_HANDSHAKE_OFFSET, &pcidata);
		printk("mfi 1068 offset handshake read=%x\n", pcidata);
		if ((pcidata & 0xffff0000) == MFI_1068_FW_READY) {
			printk("mfi 1068 offset handshake=%x\n", pcidata);
			pcidata = 0;
			pci_write_config_dword(instance->pdev, MFI_1068_FW_HANDSHAKE_OFFSET, pcidata);
		}
	}
	return 0;
}

/**
 * megasas_check_reset_xscale -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_xscale(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	u32 consumer;
	consumer = *instance->consumer;

	if ((instance->adprecovery != MEGASAS_HBA_OPERATIONAL) && (*instance->consumer == MEGASAS_ADPRESET_INPROG_SIGN)) {
		return 1;
	}

	return 0;
}

static struct megasas_instance_template megasas_instance_template_xscale = {

	.fire_cmd = megasas_fire_cmd_xscale,
	.enable_intr = megasas_enable_intr_xscale,
	.disable_intr = megasas_disable_intr_xscale,
	.clear_intr = megasas_clear_intr_xscale,
	.read_fw_status_reg = megasas_read_fw_status_reg_xscale,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_xscale,
	.service_isr = megasas_isr,
	.coredump_poll_handler = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
*	This is the end of set of functions & definitions specific 
*	to xscale (deviceid : 1064R, PERC5) controllers
*/

/**
*	The following functions are defined for ppc (deviceid : 0x60) 
* 	controllers
*/

/**
 * megasas_enable_intr_ppc -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_ppc(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;

	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);
    
	writel(~0x80000004, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_ppc -	Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_ppc(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;

	regs = instance->reg_set;

	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_ppc - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_ppc(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_ppc -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 status, mfiStatus = 0;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_REPLY_1078_MESSAGE_INTERRUPT)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;

	if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_doorbell_clear);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_doorbell_clear);

	return mfiStatus;
}

/**
 * megasas_fire_cmd_ppc -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_ppc(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1, 
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_check_reset_ppc -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_ppc(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_ppc = {
	
	.fire_cmd = megasas_fire_cmd_ppc,
	.enable_intr = megasas_enable_intr_ppc,
	.disable_intr = megasas_disable_intr_ppc,
	.clear_intr = megasas_clear_intr_ppc,
	.read_fw_status_reg = megasas_read_fw_status_reg_ppc,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_ppc,
	.service_isr = megasas_isr,
	.coredump_poll_handler = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
 * megasas_enable_intr_skinny -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_skinny(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;

	regs = instance->reg_set;

	writel(0xFFFFFFFF, &(regs)->outbound_intr_mask);
    
	/* write ~0x00000005 (4 & 1) to the intr mask*/
	writel(~MFI_SKINNY_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_skinny -	Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_skinny(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;

	regs = instance->reg_set;

	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_skinny - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_skinny(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}


/**
 * megasas_clear_interrupt_skinny -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_skinny(struct megasas_register_set __iomem * regs)
{
	u32 status;
	u32 mfiStatus = 0;

	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_SKINNY_ENABLE_INTERRUPT_MASK)) {
		return 0;
	}

        /*
         * Check if it is our interrupt
         */
	if ((megasas_read_fw_status_reg_gen2( regs) & MFI_STATE_MASK ) == MFI_STATE_FAULT ){
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
	}else
		mfiStatus |=  MFI_INTR_FLAG_REPLY_MESSAGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_intr_status);
	
	/*
	* dummy read to flush PCI 
	*/
	readl(&regs->outbound_intr_status);

	return mfiStatus;
}
/**
 * megasas_fire_cmd_skinny -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_skinny(struct megasas_instance *instance, dma_addr_t frame_phys_addr, u32 frame_count, struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel(0, &(regs)->inbound_high_queue_port);
	writel((frame_phys_addr | (frame_count<<1))|1, 
			&(regs)->inbound_low_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
	/*msleep(5);*/
}

/**
 * megasas_check_reset_skinny -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_skinny(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		return 1;
	}

	return 0;
}

static struct megasas_instance_template megasas_instance_template_skinny = {
	
	.fire_cmd = megasas_fire_cmd_skinny,
	.enable_intr = megasas_enable_intr_skinny,
	.disable_intr = megasas_disable_intr_skinny,
	.clear_intr = megasas_clear_intr_skinny,
	.read_fw_status_reg = megasas_read_fw_status_reg_skinny,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_skinny,
	.service_isr = megasas_isr,
	.coredump_poll_handler = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
 * megasas_enable_intr_gen2 -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_gen2(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;

	regs = instance->reg_set;

	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);
    
	/* write ~0x00000005 (4 & 1) to the intr mask*/
	writel(~MFI_GEN2_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_gen2 -	Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_gen2(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;

	regs = instance->reg_set;

	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_gen2 - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_gen2 -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_gen2(struct megasas_register_set __iomem * regs)
{
        u32 status;
        u32 mfiStatus = 0;
        /*
         * Check if it is our interrupt
         */
        status = readl(&regs->outbound_intr_status);

        if (status & MFI_GEN2_ENABLE_INTERRUPT_MASK)
        {
                mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
        }
        if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT)
        {
                mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
        }

        /*
         * Clear the interrupt by writing back the same value
         */
        if (mfiStatus)
                writel(status, &regs->outbound_doorbell_clear);

        /* Dummy readl to force pci flush */
        readl(&regs->outbound_intr_status);

        return mfiStatus;

}
/**
 * megasas_fire_cmd_gen2 -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_gen2(struct megasas_instance *instance, dma_addr_t frame_phys_addr,
			u32 frame_count, struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1, 
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_adp_reset_gen2 -     For controller reset
 * @regs:                               MFI register set
 */
static int
megasas_adp_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * reg_set)
{

        u32                     retry = 0, delay = 0;
        u32                     HostDiag;
        u32                     *seq_offset = &reg_set->seq_offset;
        u32                     *hostdiag_offset = &reg_set->host_diag;

        if ( instance->instancet ==  &megasas_instance_template_skinny ){
                seq_offset = &reg_set->fusion_seq_offset;
                hostdiag_offset = &reg_set->fusion_host_diag;
        }

        writel(0, seq_offset);
        writel(4, seq_offset);
        writel(0xb, seq_offset);
        writel(2, seq_offset);
        writel(7, seq_offset);
        writel(0xd, seq_offset);

        msleep(1000);

        HostDiag = (u32)readl(hostdiag_offset);

        while ( !( HostDiag & DIAG_WRITE_ENABLE) )
        {
                msleep(100);
                HostDiag = (u32)readl(hostdiag_offset);
                printk("ADP_RESET_GEN2: retry time=%x, hostdiag=%x\n", retry, HostDiag);

                if (retry++ >= 100)
                        return 1;
        }

        printk("ADP_RESET_GEN2: HostDiag=%x\n", HostDiag);

        writel((HostDiag | DIAG_RESET_ADAPTER), hostdiag_offset);

        for (delay=0; delay<10; delay++)
        {
                msleep(1000);
        }

        HostDiag = (u32)readl(hostdiag_offset);

        while ( ( HostDiag & DIAG_RESET_ADAPTER) )
        {
                msleep(100);
                HostDiag = (u32)readl(hostdiag_offset);
                printk("ADP_RESET_GEN2: retry time=%x, hostdiag=%x\n", retry, HostDiag);

                if (retry++ >= 1000)
                        return 1;

        }
        return 0;
}

/**
 * megasas_check_reset_gen2 -   For controller reset check
 * @regs:                               MFI register set
 */
static int
megasas_check_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		return 1;
	}

        return 0;
}

static struct megasas_instance_template megasas_instance_template_gen2 = {
	
	.fire_cmd = megasas_fire_cmd_gen2,
	.enable_intr = megasas_enable_intr_gen2,
	.disable_intr = megasas_disable_intr_gen2,
	.clear_intr = megasas_clear_intr_gen2,
	.read_fw_status_reg = megasas_read_fw_status_reg_gen2,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_gen2,
	.service_isr = megasas_isr,
	.coredump_poll_handler = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
*	This is the end of set of functions & definitions
* 	specific to ppc (deviceid : 0x60) controllers
*/

/*
 * Template added for TB (Fusion)
 */
extern struct megasas_instance_template megasas_instance_template_fusion;

/**
 * megasas_issue_polled -	Issues a polling command
 * @instance:			Adapter soft state
 * @cmd:			Command packet to be issued 
 *
 * For polling, MFI requires the cmd_status to be set to 0xFF before posting.
 */
int
megasas_issue_polled(struct megasas_instance *instance, struct megasas_cmd *cmd)
{

	struct megasas_header *frame_hdr = &cmd->frame->hdr;

	frame_hdr->cmd_status = 0xFF;
	frame_hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	/*
	 * Issue the frame using inbound queue port
	 */
	instance->instancet->issue_dcmd(instance, cmd);

	/*
	 * Wait for cmd_status to change
	 */
	return wait_and_poll(instance, cmd);
}

/**
 * megasas_issue_blocked_cmd -	Synchronous wrapper around regular FW cmds
 * @instance:			Adapter soft state
 * @cmd:			Command to be issued
 *
 * This function waits on an event for the command to be returned from ISR.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 * Used to issue ioctl commands.
 */
static int
megasas_issue_blocked_cmd(struct megasas_instance *instance,
			  struct megasas_cmd *cmd)
{
	cmd->cmd_status = ENODATA;

	instance->instancet->issue_dcmd(instance, cmd);

	wait_event(instance->int_cmd_wait_q, cmd->cmd_status != ENODATA);

	return 0;
}

/**
 * megasas_issue_blocked_abort_cmd -	Aborts previously issued cmd
 * @instance:				Adapter soft state
 * @cmd_to_abort:			Previously issued cmd to be aborted
 *
 * MFI firmware can abort previously issued AEN comamnd (automatic event
 * notification). The megasas_issue_blocked_abort_cmd() issues such abort
 * cmd and waits for return status.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 */
static int
megasas_issue_blocked_abort_cmd(struct megasas_instance *instance,
				struct megasas_cmd *cmd_to_abort)
{
	struct megasas_cmd *cmd;
	struct megasas_abort_frame *abort_fr;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -1;

	abort_fr = &cmd->frame->abort;

	/*
	 * Prepare and issue the abort frame
	 */
	abort_fr->cmd = MFI_CMD_ABORT;
	abort_fr->cmd_status = 0xFF;
	abort_fr->flags = 0;
	abort_fr->abort_context = cmd_to_abort->index;
	abort_fr->abort_mfi_phys_addr_lo = cmd_to_abort->frame_phys_addr;
	abort_fr->abort_mfi_phys_addr_hi = 0;

	cmd->sync_cmd = 1;
	cmd->cmd_status = 0xFF;

	instance->instancet->issue_dcmd(instance, cmd);

	/*
	 * Wait for this cmd to complete
	 */
	wait_event(instance->abort_cmd_wait_q, cmd->cmd_status != 0xFF);
	cmd->sync_cmd = 0;


	megasas_return_cmd(instance, cmd);
	return 0;
}

/**
 * megasas_make_sgl32 -	Prepares 32-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl32(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	/*
	 * Return 0 if there is no data transfer
	 */
	if (!scp->request_buffer || !scp->request_bufflen)
		return 0;

	if (!scp->use_sg) {
		mfi_sgl->sge32[0].phys_addr = pci_map_single(instance->pdev,
							     scp->
							     request_buffer,
							     scp->
							     request_bufflen,
							     scp->
							     sc_data_direction);
		mfi_sgl->sge32[0].length = scp->request_bufflen;

		return 1;
	}

	os_sgl = (struct scatterlist *)scp->request_buffer;
	sge_count = pci_map_sg(instance->pdev, os_sgl, scp->use_sg,
			       scp->sc_data_direction);

	for (i = 0; i < sge_count; i++) {
		mfi_sgl->sge32[i].length = sg_dma_len(os_sgl);
		mfi_sgl->sge32[i].phys_addr = sg_dma_address(os_sgl);
#if defined(__VMKLNX__)
		os_sgl = sg_next(os_sgl);
#else
		os_sgl++;
#endif
	}

	return sge_count;
}

/**
 * megasas_make_sgl64 -	Prepares 64-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl64(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	/*
	 * Return 0 if there is no data transfer
	 */
	if (!scp->request_buffer || !scp->request_bufflen)
		return 0;

	if (!scp->use_sg) {
		mfi_sgl->sge64[0].phys_addr = pci_map_single(instance->pdev,
							     scp->
							     request_buffer,
							     scp->
							     request_bufflen,
							     scp->
							     sc_data_direction);

		mfi_sgl->sge64[0].length = scp->request_bufflen;

		return 1;
	}

	os_sgl = (struct scatterlist *)scp->request_buffer;
	sge_count = pci_map_sg(instance->pdev, os_sgl, scp->use_sg,
			       scp->sc_data_direction);

	for (i = 0; i < sge_count; i++) {
		mfi_sgl->sge64[i].length = sg_dma_len(os_sgl);
		mfi_sgl->sge64[i].phys_addr = sg_dma_address(os_sgl);
#if defined(__VMKLNX__)
		os_sgl = sg_next(os_sgl);
#else
		os_sgl++;
#endif
	}

	return sge_count;
}
 
/**
 * megasas_make_sgl_kinny -	Prepares 64-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl_skinny(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	/*
	 * Return 0 if there is no data transfer
	 */
	if (!scp->request_buffer || !scp->request_bufflen)
		return 0;

	if (!scp->use_sg) {
		mfi_sgl->sge_skinny[0].phys_addr = pci_map_single(instance->pdev,
							     scp->
							     request_buffer,
							     scp->
							     request_bufflen,
							     scp->
							     sc_data_direction);

		mfi_sgl->sge_skinny[0].length = scp->request_bufflen;

		return 1;
	}

	os_sgl = (struct scatterlist *)scp->request_buffer;
	sge_count = pci_map_sg(instance->pdev, os_sgl, scp->use_sg,
			       scp->sc_data_direction);

	for (i = 0; i < sge_count; i++) {
		mfi_sgl->sge_skinny[i].length = sg_dma_len(os_sgl);
		mfi_sgl->sge_skinny[i].phys_addr = sg_dma_address(os_sgl);
		mfi_sgl->sge_skinny[i].flag = 0;
#if defined(__VMKLNX__)
		os_sgl = sg_next(os_sgl);
#else
		os_sgl++;
#endif
	}

	return sge_count;
}

 /**
 * megasas_get_frame_count - Computes the number of frames
 * @sge_count		: number of sg elements
 * @frame_type		: type of frame- io or pthru frame
 *
 * Returns the number of frames required for numnber of sge's (sge_count)
 */
static u32 megasas_get_frame_count(struct megasas_instance *instance, u8 sge_count, u8 frame_type)
{
	int num_cnt;
	int sge_bytes;
	u32 sge_sz;
	u32 frame_count=0;

	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);

	}

	/*
	 * Main frame can contain 2 SGEs for 64-bit SGLs and
	 * 3 SGEs for 32-bit SGLs for ldio &
	 * 1 SGEs for 64-bit SGLs and
	 * 2 SGEs for 32-bit SGLs for pthru frame
	 */
	if (unlikely(frame_type == PTHRU_FRAME)) {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;

		} else if (IS_DMA64)
			num_cnt = sge_count - 1;
		else
			num_cnt = sge_count - 2;
	} else {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;

		} else if (IS_DMA64)
			num_cnt = sge_count - 2;
		else
			num_cnt = sge_count - 3;
	}

	if(num_cnt>0){
		sge_bytes = sge_sz * num_cnt;

		frame_count = (sge_bytes / MEGAMFI_FRAME_SIZE) +
		    ((sge_bytes % MEGAMFI_FRAME_SIZE) ? 1 : 0) ;
	}
	/* Main frame */
	frame_count +=1;

	if (frame_count > 7)
		frame_count = 8;
	return frame_count;
}

/**
 * megasas_build_dcdb -	Prepares a direct cdb (DCDB) command
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to be prepared in
 *
 * This function prepares CDB commands. These are typcially pass-through
 * commands to the devices.
 */
static int
megasas_build_dcdb(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 is_logical;
	u32 device_id;
	u16 flags = 0;
	struct megasas_pthru_frame *pthru;

	is_logical = MEGASAS_IS_LOGICAL(scp);
	device_id = MEGASAS_DEV_INDEX(instance, scp);
	pthru = (struct megasas_pthru_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;
	else if (scp->sc_data_direction == PCI_DMA_NONE)
		flags = MFI_FRAME_DIR_NONE;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	/*
	 * Prepare the DCDB frame
	 */
	pthru->cmd = (is_logical) ? MFI_CMD_LD_SCSI_IO : MFI_CMD_PD_SCSI_IO;
	pthru->cmd_status = 0x0;
	pthru->scsi_status = 0x0;
	pthru->target_id = device_id;
	pthru->lun = scp->device->lun;
	pthru->cdb_len = scp->cmd_len;
	pthru->timeout = 0;
	pthru->pad_0 = 0;
	pthru->flags = flags;
	pthru->data_xfer_len = scp->request_bufflen;

	memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

        /*
         * If the command is for the tape device, set the
         * pthru timeout to the os layer timeout value.
         */
        if(scp->device->type == TYPE_TAPE) {
                if((scp->timeout_per_command / HZ) > 0xFFFF)
                        pthru->timeout = 0xFFFF;
                else
                        pthru->timeout = scp->timeout_per_command / HZ;
        }

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee == 1) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl_skinny(instance, scp,
						      &pthru->sgl);

	} else if (IS_DMA64) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl64(instance, scp,
						      &pthru->sgl);
	} else
		pthru->sge_count = megasas_make_sgl32(instance, scp,
						      &pthru->sgl);

       if (pthru->sge_count > instance->max_num_sge)
                return 0;
	/*
	 * Sense info specific
	 */
	pthru->sense_len = SCSI_SENSE_BUFFERSIZE;
	pthru->sense_buf_phys_addr_hi = 0;
	pthru->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance, pthru->sge_count,
							PTHRU_FRAME);

	return cmd->frame_count;
}

static inline u64 megasas_time_to_usecs(struct timeval *tv)
{
        return ((tv->tv_sec*1000000) + tv->tv_usec);
}

inline void UpdateIOMetric(struct megasas_instance *instance, u8 TargetId, u8 isRead, u64 startBlock, u32 NumBlocks)
{
    MR_IO_METRICS_SIZE          *pIOSizeMetric;
    MR_IO_METRICS_RANDOMNESS    *pIORandomMetric;
    struct timeval              current_time;

    if (!instance->PerformanceMetric.LogOn)
        return;

    if (TargetId > MAX_PERF_COLLECTION_VD)
        return;

    if (isRead)
    {
        pIOSizeMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].readSize;
        pIORandomMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].readRandomness;
        instance->PerformanceMetric.IoMetricsLD[TargetId].readMB += NumBlocks; // lba to MB conversion happens later                                           
    }
    else
    {
        pIOSizeMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].writeSize;
        pIORandomMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].writeRandomness;
        instance->PerformanceMetric.IoMetricsLD[TargetId].writeMB += NumBlocks; // lba to MB conversion happens later                                          
    }

    do_gettimeofday(&current_time);
    instance->PerformanceMetric.LastIOTime[TargetId] = megasas_time_to_usecs(&current_time);

    if (NumBlocks  <= 1)
        pIOSizeMetric->lessThan512B++;


    else if (NumBlocks  <=  8)
        pIOSizeMetric->between512B_4K++;
    else if (NumBlocks  <=  32)
        pIOSizeMetric->between4K_16K++;
    else if (NumBlocks  <=  128)
        pIOSizeMetric->between16K_64K++;
    else if (NumBlocks  <=  256)
        pIOSizeMetric->between64K_256K++;
    else if (NumBlocks  >  256)
        pIOSizeMetric->moreThan256K++;                        // Number of IOs: 256K < size                                                                    
    if (instance->PerformanceMetric.LastBlock[TargetId] == startBlock)
        pIORandomMetric->sequential++;                         // Number of IOs: sequential ( inter-LBA distance is 0)                                         
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 128 ) <= startBlock)
        pIORandomMetric->lessThan64K++;                        // Number of IOs: within 64KB of previous IO                                                    
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 1024 ) <= startBlock)
        pIORandomMetric->between64K_512K++;                    // Number of IOs:  64K < LBA <=512K                                                             
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 32768) <= startBlock)
        pIORandomMetric->between512K_16M++;                    // Number of IOs: 512K < LBA <=16M                                                              
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 524288) <= startBlock)
        pIORandomMetric->between16M_256M++;                    // Number of IOs:  16M < LBA <=256M                                                             
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 2097152) <= startBlock)
        pIORandomMetric->between256M_1G++;                     // Number of IOs: 256M < LBA <=1G                                                               
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 2097152) > startBlock)
        pIORandomMetric->moreThan1G++;                         // Number of IOs:   1G < LBA                                                                    

    instance->PerformanceMetric.LastBlock[TargetId] = startBlock + NumBlocks;

}

static void CopyPerfMetricData(struct megasas_instance *instance, u64 CurrentTime)
{
	u8 i;
	u64 temp;

	// Block to MB conversion & idle time calculation.                         
	for (i=0; i <MAX_PERF_COLLECTION_VD; i++ ) {
		instance->PerformanceMetric.IoMetricsLD[i].readMB  >>= BLOCKTOMB_BITSHIFT;
		instance->PerformanceMetric.IoMetricsLD[i].writeMB >>= BLOCKTOMB_BITSHIFT;
		instance->PerformanceMetric.IoMetricsLD[i].targetId = i;
		temp = CurrentTime - instance->PerformanceMetric.LastIOTime[i];
		do_div(temp, 1000000);
		instance->PerformanceMetric.IoMetricsLD[i].idleTime += (u16)temp;
	}
	temp = CurrentTime - instance->PerformanceMetric.CollectStartTime;
	do_div(temp, 1000000);
	instance->PerformanceMetric.SavedCollectTimeSecs = (u32)temp;

	memcpy(instance->PerformanceMetric.SavedIoMetricsLD, instance->PerformanceMetric.IoMetricsLD, sizeof(MR_IO_METRICS_LD_OVERALL) * MAX_PERF_COLLECTION_VD);

	// Zero out the original buffer for new collection.                        
	memset(instance->PerformanceMetric.IoMetricsLD, 0, sizeof(MR_IO_METRICS_LD_OVERALL) * MAX_PERF_COLLECTION_VD);
}

static void ProcessPerfMetricAEN(struct megasas_instance *instance, MR_CTRL_IO_METRICS_CMD_TYPE MetricType)
{
    struct timeval      current_time;
    u8 i;

    do_gettimeofday(&current_time);

    switch (MetricType) {
    case MR_CTRL_IO_METRICS_CMD_STOP:
        if (instance->PerformanceMetric.LogOn) {

            // Set the flag to Stop collection.                                 
            instance->PerformanceMetric.LogOn = 0;

            CopyPerfMetricData(instance, megasas_time_to_usecs(&current_time));
        }
        break;

    case MR_CTRL_IO_METRICS_CMD_START:

		// Save the previously collected data.                                  
		if (instance->PerformanceMetric.LogOn)
			CopyPerfMetricData(instance, megasas_time_to_usecs(&current_time));

        //Initialize the timers.                                                
        instance->PerformanceMetric.CollectStartTime = megasas_time_to_usecs(&current_time);
        for (i=0;i <MAX_PERF_COLLECTION_VD ; i++ )
            instance->PerformanceMetric.LastIOTime[i]  = megasas_time_to_usecs(&current_time);

        // Set the flag to start collection.                                    
        instance->PerformanceMetric.LogOn = 1;

        break;
    default:
        printk("megaraid_sas: unepxected value %x for metric type\n", MetricType);
        break;
    }
}

static void ProcessPerfMetricRequest(struct megasas_instance *instance, u8* ioctlBuffer, u32 dataTransferlength)
{
        MR_IO_METRICS *pIOMetric = (MR_IO_METRICS *)ioctlBuffer;
        MR_IO_METRICS_LD_OVERALL_LIST   *pldIoMetrics = (MR_IO_METRICS_LD_OVERALL_LIST *)(ioctlBuffer + pIOMetric->ctrlIoCache.size);

	if (instance->CurLdCount) {
		pldIoMetrics->size = (sizeof(MR_IO_METRICS_LD_OVERALL)*(min((u32)MAX_PERF_COLLECTION_VD, instance->CurLdCount) -1) + sizeof(MR_IO_METRICS_LD_OVERALL_LIST));
		if ((pldIoMetrics->size + pIOMetric->ctrlIoCache.size) <= dataTransferlength)
			memcpy(&pldIoMetrics->ldIOOverall[0], instance->PerformanceMetric.SavedIoMetricsLD, sizeof(MR_IO_METRICS_LD_OVERALL)*min((u32)MAX_PERF_COLLECTION_VD, instance->CurLdCount));
	}
	pldIoMetrics->collectionPeriod = instance->PerformanceMetric.SavedCollectTimeSecs;
}

/**
 * megasas_build_ldio -	Prepares IOs to logical devices
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to to be prepared
 *
 * Frames (and accompanying SGLs) for regular SCSI IOs use this function.
 */
static int
megasas_build_ldio(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 device_id;
	u8 sc = scp->cmnd[0];
	u16 flags = 0;
	struct megasas_io_frame *ldio;
	unsigned long spinlock_flags;
	u64 lba;

	device_id = MEGASAS_DEV_INDEX(instance, scp);
	ldio = (struct megasas_io_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}
	
	/*
	 * Prepare the Logical IO frame: 2nd bit is zero for all read cmds
	 */
	ldio->cmd = (sc & 0x02) ? MFI_CMD_LD_WRITE : MFI_CMD_LD_READ;
	ldio->cmd_status = 0x0;
	ldio->scsi_status = 0x0;
	ldio->target_id = device_id;
	ldio->timeout = 0;
	ldio->reserved_0 = 0;
	ldio->pad_0 = 0;
	ldio->flags = flags;
	ldio->start_lba_hi = 0;
	ldio->access_byte = (scp->cmd_len != 6) ? scp->cmnd[1] : 0;

	/*
	 * 6-byte READ(0x08) or WRITE(0x0A) cdb
	 */
	if (scp->cmd_len == 6) {
		ldio->lba_count = (u32) scp->cmnd[4];
		ldio->start_lba_lo = ((u32) scp->cmnd[1] << 16) |
		    ((u32) scp->cmnd[2] << 8) | (u32) scp->cmnd[3];

		ldio->start_lba_lo &= 0x1FFFFF;
	}

	/*
	 * 10-byte READ(0x28) or WRITE(0x2A) cdb
	 */
	else if (scp->cmd_len == 10) {
		ldio->lba_count = (u32) scp->cmnd[8] |
		    ((u32) scp->cmnd[7] << 8);
		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	/*
	 * 12-byte READ(0xA8) or WRITE(0xAA) cdb
	 */
	else if (scp->cmd_len == 12) {
		ldio->lba_count = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	/*
	 * 16-byte READ(0x88) or WRITE(0x8A) cdb
	 */
	else if (scp->cmd_len == 16) {
		ldio->lba_count = ((u32) scp->cmnd[10] << 24) |
		    ((u32) scp->cmnd[11] << 16) |
		    ((u32) scp->cmnd[12] << 8) | (u32) scp->cmnd[13];

		ldio->start_lba_lo = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_hi = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];

	}

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl_skinny(instance, scp,
					      &ldio->sgl);

	} else if (IS_DMA64) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl64(instance, scp, &ldio->sgl);
	} else
		ldio->sge_count = megasas_make_sgl32(instance, scp, &ldio->sgl);

        if (ldio->sge_count > instance->max_num_sge)
                return 0;

	/*
	 * Sense info specific
	 */
	ldio->sense_len = SCSI_SENSE_BUFFERSIZE;
	ldio->sense_buf_phys_addr_hi = 0;
	ldio->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance, ldio->sge_count, IO_FRAME);

        lba     = (u64)ldio->start_lba_hi << 32 | ldio->start_lba_lo;
        spin_lock_irqsave(&instance->hba_lock, spinlock_flags);
        UpdateIOMetric(instance, device_id, ldio->cmd == MFI_CMD_LD_READ ? 1 : 0, lba, ldio->lba_count);
        spin_unlock_irqrestore(&instance->hba_lock, spinlock_flags);

	return cmd->frame_count;
}

/**
 * megasas_is_ldio -		Checks if the cmd is for logical drive
 * @scmd:			SCSI command
 *	
 * Called by megasas_queue_command to find out if the command to be queued
 * is a logical drive command	
 */
int megasas_is_ldio(struct scsi_cmnd *cmd)
{
	if (!MEGASAS_IS_LOGICAL(cmd))
		return 0;
	switch (cmd->cmnd[0]) {
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
	case READ_6:
	case WRITE_6:
	case READ_16:
	case WRITE_16:
		return 1;
	default:
		return 0;
	}
}

 /**
 * megasas_dump_pending_frames -	Dumps the frame address of all pending cmds
 *                              	in FW
 * @instance:				Adapter soft state
 */
static inline void
megasas_dump_pending_frames(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i,n;
	union megasas_sgl *mfi_sgl;
	struct megasas_io_frame *ldio;
	struct megasas_pthru_frame *pthru;
	u32 sgcount;
	u32 max_cmd = instance->max_fw_cmds;

	printk(KERN_ERR "\nmegasas[%d]: Dumping Frame Phys Address of all pending cmds in FW\n",instance->host->host_no);
	printk(KERN_ERR "megasas[%d]: Total OS Pending cmds : %d\n",instance->host->host_no,atomic_read(&instance->fw_outstanding));
	if (IS_DMA64)
		printk(KERN_ERR "\nmegasas[%d]: 64 bit SGLs were sent to FW\n",instance->host->host_no);
	else
		printk(KERN_ERR "\nmegasas[%d]: 32 bit SGLs were sent to FW\n",instance->host->host_no);

	printk(KERN_ERR "megasas[%d]: Pending OS cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if(!cmd->scmd)
			continue;
		printk(KERN_ERR "megasas[%d]: Frame addr :0x%08lx : ",instance->host->host_no,(unsigned long)cmd->frame_phys_addr);
		if (megasas_is_ldio(cmd->scmd)){
			ldio = (struct megasas_io_frame *)cmd->frame;
			mfi_sgl = &ldio->sgl;
			sgcount = ldio->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lba lo : 0x%x, lba_hi : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no, cmd->frame_count,ldio->cmd,ldio->target_id, ldio->start_lba_lo,ldio->start_lba_hi,ldio->sense_buf_phys_addr_lo,sgcount);
		}
		else {
			pthru = (struct megasas_pthru_frame *) cmd->frame;
			mfi_sgl = &pthru->sgl;
			sgcount = pthru->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lun : 0x%x, cdb_len : 0x%x, data xfer len : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no,cmd->frame_count,pthru->cmd,pthru->target_id,pthru->lun,pthru->cdb_len , pthru->data_xfer_len,pthru->sense_buf_phys_addr_lo,sgcount);
		}
	if(megasas_dbg_lvl & MEGASAS_DBG_LVL){
		for (n = 0; n < sgcount; n++){
			if (IS_DMA64)
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%08lx ",mfi_sgl->sge64[n].length , (unsigned long)mfi_sgl->sge64[n].phys_addr) ;
			else
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%x ",mfi_sgl->sge32[n].length , mfi_sgl->sge32[n].phys_addr) ;
			}
		}
		printk(KERN_ERR "\n");
	} /*for max_cmd*/
	printk(KERN_ERR "\nmegasas[%d]: Pending Internal cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if(cmd->sync_cmd == 1){
			printk(KERN_ERR "0x%08lx : ", (unsigned long)cmd->frame_phys_addr);
		}
	}
	printk(KERN_ERR "megasas[%d]: Dumping Done.\n\n",instance->host->host_no);
}

u32
megasas_build_and_issue_cmd(struct megasas_instance *instance, struct scsi_cmnd *scmd)
{
	struct megasas_cmd *cmd;
	u32 frame_count;
	

	cmd = megasas_get_cmd(instance);
	if (!cmd)
		return SCSI_MLQUEUE_HOST_BUSY;

	/*
	 * Logical drive command
	 */
	if (megasas_is_ldio(scmd))
		frame_count = megasas_build_ldio(instance, scmd, cmd);
	else
		frame_count = megasas_build_dcdb(instance, scmd, cmd);

	if (!frame_count) {
		goto out_return_cmd;
	}

	cmd->scmd = scmd;
	scmd->SCp.ptr = (char *)cmd;

	/*
	 * Issue the command to the FW
	 */
	atomic_inc(&instance->fw_outstanding);

	instance->instancet->fire_cmd(instance, cmd->frame_phys_addr,
				cmd->frame_count-1, instance->reg_set);

	return 0;
out_return_cmd:
	megasas_return_cmd(instance,cmd);
	return 1;
}

/**
 * megasas_queue_command -	Queue entry point
 * @scmd:			SCSI command to be queued
 * @done:			Callback entry point
 */
static int
megasas_queue_command(struct scsi_cmnd *scmd, void (*done) (struct scsi_cmnd *))
{
	struct megasas_instance *instance;
	unsigned long flags;
	unsigned long sec;

	instance = (struct megasas_instance *)
    scmd->device->host->hostdata;

	if (instance->issuepend_done == 0)
		return SCSI_MLQUEUE_HOST_BUSY;

#if defined(__VMKLNX__)   // This from Vmware
	/*
	 * 1078R's firmware doesn't support SCSI reservation, fake it.
	 */
	if ((scmd->cmnd[0] == RESERVE ||
		scmd->cmnd[0] == RELEASE)) {
	    scmd->result = DID_OK << 16;
	    goto out_done;
	}
#endif /*  defined(__VMKLNX__) */

	spin_lock_irqsave(&instance->hba_lock, flags);

	//Don't process if we have already declared adapter dead
	// If we are in middle of bringing up the HBA, send the busy status to mid-layer
	// till the process is complete
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

#if defined(__VMKLNX__)
	/* If FW is busy do no accept any more cmds */
	if (instance->in_taskmgmt || instance->is_busy) {	  
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return SCSI_MLQUEUE_HOST_BUSY;
	}
#endif

	spin_unlock_irqrestore(&instance->hba_lock, flags);

	scmd->scsi_done = done;
	scmd->result = 0;

	if (MEGASAS_IS_LOGICAL(scmd) &&
	    (scmd->device->id >= MEGASAS_MAX_LD || scmd->device->lun)) {
		/* PR 570447 MN Storage Device Driver SCSI Host Status Audit (PMT #3018)
		 * PR 560419, further audit.
		 * Remove DID_BAD_TARGET and replace with DID_NO_CONNECT in
		 * support of MN PSA requirements.
		 */
#if defined(__VMKLNX__)
		scmd->result = DID_NO_CONNECT << 16;
#else
		scmd->result = DID_BAD_TARGET << 16;
#endif
		goto out_done;
	}

	switch (scmd->cmnd[0]) {
	case SYNCHRONIZE_CACHE:
		/*
		 * FW takes care of flush cache on its own
		 * No need to send it down
		 */
		scmd->result = DID_OK << 16;
		goto out_done;
	}
	
#if defined(__VMKLNX__)
	spin_lock_irqsave(&instance->hba_lock, flags);
#endif
	/* If FW is busy donot accept any more cmds */
	if(instance->is_busy){
		sec = (jiffies - instance->last_time) / HZ;
		if(sec<10) {
#if defined(__VMKLNX__)
			spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif
               return SCSI_MLQUEUE_HOST_BUSY;
		} else{
               instance->is_busy=0;
               instance->last_time=0;
		}
	}
#if defined(__VMKLNX__)
	spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif

	if(scmd->retries>1){
#if defined(__VMKLNX__)
		spin_lock_irqsave(&instance->hba_lock, flags);
#endif
		instance->is_busy=1;
		instance->last_time=jiffies;
#if defined(__VMKLNX__)
		instance->cmd_per_lun = THROTTLE_MAX_CMD_PER_LUN;
		TRACK_IS_BUSY("megasas_queue_command",instance->is_busy)
		spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif
	}

	if (instance->instancet->build_and_issue_cmd(instance,scmd)) {
		printk(KERN_ERR "megasas: Err returned from build_and_issue_cmd\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	return 0;

// out_return_cmd:
//	megasas_return_cmd(instance, cmd);
 out_done:
	done(scmd);
	return 0;
}

static struct megasas_instance *megasas_lookup_instance(u16 host_no)
{
	int i;

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {

		if ((megasas_mgmt_info.instance[i]) &&
		    (megasas_mgmt_info.instance[i]->host->host_no == host_no))
			return megasas_mgmt_info.instance[i];
	}

	return NULL;
}

static int megasas_slave_configure(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct  megasas_instance *instance ;

	instance = megasas_lookup_instance(sdev->host->host_no);

	/*
	 * Don't export physical disk devices to the disk driver.
	 *
	 * FIXME: Currently we don't export them to the midlayer at all.
	 * 	  That will be fixed once LSI engineers have audited the
	 * 	  firmware for possible issues.
	 */
	if (sdev->channel < MEGASAS_MAX_PD_CHANNELS && sdev->type == TYPE_DISK) {
		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) + sdev->id;
                if ((instance->pd_list[pd_index].driveState == MR_PD_STATE_SYSTEM) && (instance->pd_list[pd_index].driveType == TYPE_DISK)) {
			sdev->timeout = 90 * HZ;
			return 0;
		}
#if defined(__VMKLNX__)
		printk ("megasas_slave_configure: do not export physical disk devices to upper layer.\n");
		sdev->no_uld_attach = 1;
		return 0;
#else
		return -ENXIO;
#endif
	}

	/*
	* The RAID firmware may require extended timeouts.
	*/
	sdev->timeout = MEGASAS_DEFAULT_CMD_TIMEOUT * HZ;
	return 0;
}

void megaraid_sas_kill_hba(struct megasas_instance *instance)
{
       if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
       (instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
	   (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	   (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY) ||
	   (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
       {
               writel(MFI_STOP_ADP,
                       &instance->reg_set->doorbell);
               /* Flush */
               readl(&instance->reg_set->doorbell);
       } else {
               writel(MFI_STOP_ADP,
                       &instance->reg_set->inbound_doorbell);
       }
}


/**
 * megasas_complete_cmd_tasklet	 -	Returns FW's controller structure
 * @instance_addr:			Address of adapter soft state
 *
 * Tasklet to complete cmds
 */
static void megasas_complete_cmd_tasklet(unsigned long instance_addr)
{
	u32 producer;
	u32 consumer;
	u32 context;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance = (struct megasas_instance *)instance_addr;
	unsigned long flags;

#if defined(__VMKLNX__)
	
	spin_lock_irqsave(&instance->hba_lock, flags);

	/* If we have already declared adapter dead, donot complete cmds. Also
	 * don't complete them if we are in recovery, as the command will have
	 * been pended.  If the pend queue finds that the command has already
	 * been completed, it will note an error as an "unexpected cmd". Let the
	 * pend queue handling after reset deal with completing commands then.
	 */

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR ) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return;
	}

#else
	/* If we have already declared adapter dead, donot complete cmds */
	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR ) { 
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return;
	}
#endif

	spin_unlock_irqrestore(&instance->hba_lock, flags);

	spin_lock_irqsave(&instance->completion_lock, flags);

	producer = *instance->producer;
	consumer = *instance->consumer;

	while (consumer != producer) {
		context = instance->reply_queue[consumer];

		if (context >= instance->max_fw_cmds) {
			printk("ERROR ERROR: unexpected context value %x\n", context);
			BUG();
		}
		cmd = instance->cmd_list[context];

		megasas_complete_cmd(instance, cmd, DID_OK);

		consumer++;
		if (consumer == (instance->max_fw_cmds + 1)) {
			consumer = 0;
		}
	}

	*instance->consumer = producer;
	spin_unlock_irqrestore(&instance->completion_lock, flags);

#if defined(__VMKLNX__)
 /*
  * There is no need to hold the completion lock when managing the
  * is_busy flag.
  */

	spin_lock_irqsave(&instance->hba_lock, flags);

	if(instance->is_busy 
	&& time_after(jiffies, instance->last_time + 5 * HZ)
	&& atomic_read(&instance->fw_outstanding) < THROTTLE_MAX_CMD_PER_LUN) {
         instance->last_time = 0;
         instance->is_busy=0;
         TRACK_IS_BUSY("megasas_complete_cmd_tasklet",instance->is_busy)
   }
   spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif

}

void megasas_do_ocr(struct megasas_instance *instance);

/**
 * megasas_wait_for_outstanding -	Wait for all outstanding cmds
 * @instance:				Adapter soft state
 *
 * This function waits for upto MEGASAS_RESET_WAIT_TIME seconds for FW to
 * complete all its outstanding commands. Returns error if one or more IOs
 * are pending after this time period. It also marks the controller dead.
 */
static int megasas_wait_for_outstanding(struct megasas_instance *instance)
{
       int i;
	u32 reset_index;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;
	u8 adprecovery;
	unsigned long flags;
	struct list_head clist_local;
	struct megasas_cmd *reset_cmd;

	// If we are in-process if internal reset, we should wait for that process to
	// complete
	spin_lock_irqsave(&instance->hba_lock, flags);
	adprecovery = instance->adprecovery;
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	if (adprecovery != MEGASAS_HBA_OPERATIONAL) {

		// We take the ownership of all the pending commands. These would be failed to the OS
		// after a successful recovery from adapter internal reset condition.
		INIT_LIST_HEAD(&clist_local);
		spin_lock_irqsave(&instance->hba_lock, flags);
		list_splice_init(&instance->internal_reset_pending_q, &clist_local);
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		printk("megasas: HBA reset handler invoked while adapter internal reset in progress, wait till that's over...\n");
		for (i = 0; i < wait_time; i++) {
			msleep(1000);
			// Are we there yet?
			spin_lock_irqsave(&instance->hba_lock, flags);
			adprecovery = instance->adprecovery;
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			if (adprecovery == MEGASAS_HBA_OPERATIONAL)
				break;
		}

		// Are we out of reset yet? If not, HBA is toasted :-(
		if (adprecovery != MEGASAS_HBA_OPERATIONAL) {
			printk("megasas: HBA reset handler timedout for internal reset. Stopping the HBA.\n");
			spin_lock_irqsave(&instance->hba_lock, flags);
			instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			return FAILED;
		}

		printk("megasas: HBA internal reset condition discovered to be cleared.\n");

		// Send the pending commands back to the OS with reset condition
		reset_index	= 0;
		while (!list_empty(&clist_local)) {
			reset_cmd	= list_entry((&clist_local)->next, struct megasas_cmd, list);
			list_del_init(&reset_cmd->list);
			if (reset_cmd->scmd) {
				reset_cmd->scmd->result = DID_RESET << 16;
				printk("megasas: %d:%p reset scsi command [%02x], %#lx\n",
					reset_index, reset_cmd, reset_cmd->scmd->cmnd[0], reset_cmd->scmd->serial_number);
				reset_cmd->scmd->scsi_done(reset_cmd->scmd);
				megasas_return_cmd(instance, reset_cmd);
			}
			else if (reset_cmd->sync_cmd) {
				// Such commands have no timeout, we re-issue this guy again.
				printk("megasas: %p synchronous command detected on the internal reset queue, re-issuing it.\n", reset_cmd);
				reset_cmd->cmd_status = ENODATA;
				instance->instancet->fire_cmd(instance, reset_cmd->frame_phys_addr ,0,instance->reg_set);
			}
			else {
				printk("megasas: %p unexpected command on the internal reset defer list.\n", reset_cmd);
			}
			reset_index++;
		}

		printk("megaraid_sas: All pending commands have been cleared for reset condition.\n");

		return SUCCESS;
	}

	// Kernel reset without internal reset in progress.
	printk("megaraid_sas: HBA reset handler invoked without an internal reset condition.\n");
	instance->adprecovery   = MEGASAS_ADPRESET_SM_INFAULT;
	for (i = 0; i < wait_time; i++) {

		int outstanding = atomic_read(&instance->fw_outstanding);

		if (!outstanding)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
#if !defined(__VMKLNX__) /* reduce log spew in vmkernel */
			printk(KERN_NOTICE "megasas: [%2d]waiting for %d "
			       "commands to complete\n",i,outstanding);
#endif /* defined(__VMKLNX__) */
			/*
			 * Call cmd completion routine. Cmd to be 
			 * be completed directly without depending on isr.
			 */
			megasas_complete_cmd_tasklet((unsigned long)instance);
		}

		msleep(1000);
	}
#if defined(__VMKLNX__)
	printk(KERN_DEBUG "megaraid_sas: megasas_wait_for_outstanding: line %d: AFTER HBA reset handler invoked without an internal reset condition:  "
	" took %d seconds. Max is %d.\n", 
	   __LINE__,
	   i, 
	   wait_time);
#endif

        /**
        for the fw state not fault case, it is maybe fw hang, driver need to reset the controller before kill the adapter.
        */
        if (atomic_read(&instance->fw_outstanding)) {
                if (instance->disableOnlineCtrlReset == 0) {
                        megasas_do_ocr(instance);
                        printk("megasas: waiting_for_outstanding: after issue OCR. \n");

                        /* wait for 5 secs to let the FW finish all the pending cmds*/
                        for (i = 0; i < wait_time; i++) {
                                int outstanding = atomic_read(&instance->fw_outstanding);

                                if (!outstanding)
                                        return SUCCESS;
                                msleep(1000);
                        }
#if defined(__VMKLNX__)

                        printk(KERN_DEBUG "megaraid_sas: megasas_wait_for_outstanding: line %d: Waited "
                        "%d seconds to finish all pending commands, but did not return SUCCESS.", 
                        __LINE__, wait_time);
 #endif
                }
        }
        else {
			instance->adprecovery   = MEGASAS_HBA_OPERATIONAL;
			printk("megaraid_sas: 180 seconds wait had cleared all pending commands.\n");
			return SUCCESS;
        }

        if (atomic_read(&instance->fw_outstanding)) {

		printk("megaraid_sas: pending commands remain even after reset handling.\n");
		/*
		* Send signal to FW to stop processing any pending cmds.
		* The controller will be taken offline by the OS now.
		*/
		if (instance->is_imr) {
			writel(MFI_STOP_ADP,
                               &instance->reg_set->doorbell);
		} else {
			writel(MFI_STOP_ADP,
				&instance->reg_set->inbound_doorbell);
		}
		megasas_dump_pending_frames(instance);
		spin_lock_irqsave(&instance->hba_lock, flags);
		instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return FAILED;
	}

	printk("megaraid_sas: no more pending commands remain after reset handling.\n");

	return SUCCESS;
}

/**
 * megasas_generic_reset -	Generic reset routine
 * @scmd:			Mid-layer SCSI command
 *
 * This routine implements a generic reset handler for device, bus and host
 * reset requests. Device, bus and host specific reset handlers can use this
 * function after they do their specific tasks.
 */
static int megasas_generic_reset(struct scsi_cmnd *scmd)
{
	int ret_val;
	struct megasas_instance *instance;
#if defined(__VMKLNX__)
	unsigned long flags;
#endif

	instance = (struct megasas_instance *)scmd->device->host->hostdata;

#if defined(__VMKLNX__)
	/* Mark as BUSY. The is_busy flag is used stop queuing commands under two 
	 * conditions:
	 *  1.  Throttle exceeded or max cmds reached
	 *  2.  A reset or abort is in process, and more commands should not be
	 *      queued until this completes.
	 *
	 *  To disambiguate these states in the command completion, we mark when
	 *  an abort is in process, so that it doesn't release the throttle and
	 *  clear the is_busy flag merely because the number of commands has
	 *  dropped below throttle threshold. It should also not release is_busy
	 *  if an abort is in progress.
	*/
	spin_lock_irqsave(&instance->hba_lock, flags);
	instance->in_taskmgmt = 1;
	TRACK_IS_BUSY("megasas_generic_reset",instance->is_busy)
	spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif
	scmd_printk(KERN_NOTICE, scmd, "megasas: RESET -%ld cmd=%x retries=%x\n",
		 scmd->serial_number, scmd->cmnd[0], scmd->retries);

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR ) {
		printk(KERN_ERR "megasas: cannot recover from previous reset "
		       "failures\n");
#if defined(__VMKLNX__)
		 TRACK_IS_BUSY("megasas_generic_reset: HW in critical unrecoverable state.",
		    instance->is_busy)
#endif
		return FAILED;
	}

	ret_val = megasas_wait_for_outstanding(instance);
	if (ret_val == SUCCESS)
		printk(KERN_NOTICE "megasas: reset successful\n\n");
	else
		printk(KERN_ERR "megasas: failed to do reset\n\n");

#if defined(__VMKLNX__)
	spin_lock_irqsave(&instance->hba_lock, flags);
	instance->in_taskmgmt = 0;
	spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif

	return ret_val;
}

/*
 * This callback is used by the scsi_host_template field
 * .eh_timed_out which isn't supported in vmklinux26 today
 */
#if defined(__VMKLNX__)
/**
 * megasas_abort -	SCSI command abort handler entry point
 */
static int megasas_abort(struct scsi_cmnd *scmd)
{
	struct megasas_instance *instance;
	int ret;

	instance = (struct megasas_instance *)scmd->device->host->hostdata;

	printk("megasas: ABORT sn %ld cmd=0x%x retries=%d tmo=%d\n",
	    scmd->serial_number, scmd->cmnd[0], scmd->retries,
	    scmd->timeout_per_command);

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY)) {
		ret = megasas_reset_fusion(scmd->device->host);
		return ret;
	} else {
		mutex_lock(&instance->reset_mutex);
		ret = megasas_generic_reset(scmd);
		mutex_unlock(&instance->reset_mutex);
	}

	if (ret == FAILED) {
		/**
		 * reset failed, driver has marked the controller dead and
		 * killed the controller. All the pending cmds in controller
		 * has been aborted at this point
		 */
		return SUCCESS;
	}

	/* All pending cmds are completed (not aborted) */
	return FAILED;
}
#endif /* defined(__VMKLNX__) */

/**
 * megasas_reset_device -	Device reset handler entry point
 */
static int megasas_reset_device(struct scsi_cmnd *scmd)
{
	int ret;
        struct megasas_instance *instance;
        instance = (struct megasas_instance *)scmd->device->host->hostdata;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY))
		ret = megasas_reset_fusion(scmd->device->host);
	else {
		mutex_lock(&instance->reset_mutex);
		ret = megasas_generic_reset(scmd);
		mutex_unlock(&instance->reset_mutex);
	}

	return ret;
}

/**
 * megasas_reset_bus_host -	Bus & host reset handler entry point
 */
static int megasas_reset_bus_host(struct scsi_cmnd *scmd)
{
	int ret;
	struct megasas_instance *instance;
	instance = (struct megasas_instance *)scmd->device->host->hostdata;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_FURY))
		ret = megasas_reset_fusion(scmd->device->host);
	else {
		mutex_lock(&instance->reset_mutex);
		ret = megasas_generic_reset(scmd);
		mutex_unlock(&instance->reset_mutex);
	}

	return ret;
}

/**
 * megasas_bios_param - Returns disk geometry for a disk
 * @sdev: 		device handle
 * @bdev:		block device
 * @capacity:		drive capacity
 * @geom:		geometry parameters
 */
static int
megasas_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		 sector_t capacity, int geom[])
{
	int heads;
	int sectors;
	sector_t cylinders;
	unsigned long tmp;
	/* Default heads (64) & sectors (32) */
	heads = 64;
	sectors = 32;

	tmp = heads * sectors;
	cylinders = capacity;

	sector_div(cylinders, tmp);

	/*
	 * Handle extended translation size for logical drives > 1Gb
	 */

	if (capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		tmp = heads*sectors;
		cylinders = capacity;
		sector_div(cylinders, tmp);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
}

#if defined(__VMKLNX__)

struct megasas_hotplug_event {
	struct work_struct hotplug_work;
	struct megasas_instance *instance;
};

static int
megasas_get_seq_num(struct megasas_instance *instance,
		    struct megasas_evt_log_info *eli);
static int
megasas_register_aen(struct megasas_instance *instance, u32 seq_num,
		     u32 class_locale_word);

static void megasas_hotplug_work(struct work_struct *work)
{
	struct megasas_hotplug_event *ev =
		container_of(work, struct megasas_hotplug_event, hotplug_work);
	struct megasas_instance *instance = ev->instance;
	struct megasas_evt_log_info eli;
	union megasas_evt_class_locale class_locale;
	int doscan = FALSE;
	u32 seq_num;
	int error;
	unsigned long flags;

	if (!instance) {
		printk(KERN_ERR "%s: invalid instance!\n", __FUNCTION__);
		kfree(ev);
		return;
	}

	if (instance->evt_detail) {
		printk(KERN_INFO "%s[%d]: event code 0x%04x\n", __FUNCTION__,
			instance->host->host_no, instance->evt_detail->code);

		switch (instance->evt_detail->code) {
		case MR_EVT_LD_CREATED:
		case MR_EVT_PD_INSERTED:
		case MR_EVT_LD_DELETED:
		case MR_EVT_LD_OFFLINE:
		case MR_EVT_CFG_CLEARED:
		case MR_EVT_PD_REMOVED:
		case MR_EVT_FOREIGN_CFG_IMPORTED:
		case MR_EVT_LD_STATE_CHANGE:
		case MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED:
			doscan = TRUE;
			break;
                case MR_EVT_CTRL_PERF_COLLECTION:
                        spin_lock_irqsave(&instance->hba_lock, flags);
                        ProcessPerfMetricAEN(instance , (MR_CTRL_IO_METRICS_CMD_TYPE)(instance->evt_detail->description[42] - 48));
                        spin_unlock_irqrestore(&instance->hba_lock, flags);
                        break;
		default:
			doscan = FALSE;
			break;
		}
	} else {
		printk(KERN_ERR "%s[%d]: invalid evt_detail!\n",
			__FUNCTION__, instance->host->host_no);
		kfree(ev);
		return;
	}

	if (doscan) {
		printk(KERN_INFO "%s[%d]: scanning ...\n",
			__FUNCTION__, instance->host->host_no);
		memset(instance->pd_list, 0, MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));
		megasas_get_pd_list(instance);
		megasas_get_ld_list(instance);
		scsi_scan_host(instance->host);
		msleep(1000);
	}

	/*
	 * Get the latest sequence number from FW
	 */
	memset(&eli, 0, sizeof(eli));

	if (megasas_get_seq_num(instance, &eli)) {
		printk(KERN_ERR "%s[%d]: failed to get seq_num\n",
			__FUNCTION__, instance->host->host_no);
		kfree(ev);
		return;
	}

       if ( instance->aen_cmd != NULL ) {
               kfree(ev);
               return ;
       }

	seq_num = instance->evt_detail->seq_num + 1;

	/*
	 * Register AEN with FW for latest sequence number plus 1
	 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	down(&instance->aen_mutex);
	error = megasas_register_aen(instance, seq_num,
				    class_locale.word);
	up(&instance->aen_mutex);

	if(error)
		printk(KERN_ERR "%s[%d]: register aen failed error %x\n",
			__FUNCTION__, instance->host->host_no, error);
	else
		printk(KERN_INFO "%s[%d]: aen registered\n",
			__FUNCTION__, instance->host->host_no);
	kfree(ev);
}

#endif /* defined(__VMKLNX__) */

/**
 * megasas_service_aen -	Processes an event notification
 * @instance:			Adapter soft state
 * @cmd:			AEN command completed by the ISR
 *
 * For AEN, driver sends a command down to FW that is held by the FW till an
 * event occurs. When an event of interest occurs, FW completes the command
 * that it was previously holding.
 *
 * This routines sends SIGIO signal to processes that have registered with the
 * driver for AEN.
 */
static void
megasas_service_aen(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;

	/*
	 * Don't signal app if it is just an aborted previously registered aen
	 */
	if ((!cmd->abort_aen) && (instance->unload ==0)) {
			spin_lock_irqsave(&poll_aen_lock, flags);
			megasas_poll_wait_aen = 1;
			spin_unlock_irqrestore(&poll_aen_lock, flags);
#if defined(__VMKLNX__)
			wake_up_interruptible(&megasas_poll_wait);
#endif
		kill_fasync(&megasas_async_queue, SIGIO, POLL_IN);
	}
	else
		cmd->abort_aen = 0;

	instance->aen_cmd = NULL;
	megasas_return_cmd(instance, cmd);
	if ((instance->unload == 0) && ((instance->issuepend_done == 1))) {
#if defined(__VMKLNX__)
		printk(KERN_INFO "%s[%d]: aen received\n",
			__FUNCTION__, instance->host->host_no);

		struct megasas_hotplug_event *ev;
		ev = kzalloc(sizeof(*ev), GFP_ATOMIC);
		if (!ev) {
			printk(KERN_ERR "%s: out of memory\n", __FUNCTION__);
		} else {
			ev->instance = instance;
			INIT_WORK(&ev->hotplug_work, megasas_hotplug_work);
			schedule_delayed_work((struct delayed_work *)&ev->hotplug_work, 0);
		}
#endif /* defined(__VMKLNX__) */
	}
}

#if defined(__VMKLNX__)
static struct megasas_instance *megasas_lookup_instance_on_ctrlId(u8 ctrlId)
{
	int i;

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {

		if ((megasas_mgmt_info.instance[i]) &&
		    (megasas_mgmt_info.instance[i]->ctrlId == ctrlId))
			return megasas_mgmt_info.instance[i];
	}

	return NULL;
}
#endif /* defined(__VMKLNX__) */

static int megasas_slave_alloc(struct scsi_device *sdev) {
	u16		pd_index = 0;
	struct megasas_instance *instance ;
	int tmp_fastload = fast_load;
	instance = megasas_lookup_instance(sdev->host->host_no);

	if (tmp_fastload && sdev->channel < MEGASAS_MAX_PD_CHANNELS) {
		if ((sdev->id == MEGASAS_MAX_DEV_PER_CHANNEL -1) &&
			(sdev->channel == MEGASAS_MAX_PD_CHANNELS - 1)) {
			/* If fast load option was set and scan for last device is
			 * over, reset the fast_load flag so that during a possible
			 * next scan, devices can be made available
			 */
			fast_load = 0;
		}

		/*
		 * Open the OS scan to the SYSTEM PD
		 */

		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) + sdev->id;		
		if ((instance->pd_list[pd_index].driveState == MR_PD_STATE_SYSTEM) &&
			(instance->pd_list[pd_index].driveType == TYPE_DISK)) {
			return 0;
		}

		return -ENXIO;
	}

	return 0;

}

/**
 * megasas_complete_cmd_dpc	 -	Returns FW's controller structure
 * @instance_addr:			Address of adapter soft state
 *
 * Tasklet to complete cmds
 */
static void megasas_complete_cmd_dpc(unsigned long instance_addr)
{
	u32 producer;
	u32 consumer;
	u32 context;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance =
				(struct megasas_instance *)instance_addr;
	unsigned long flags;

#if defined(__VMKLNX__)
        /* If we have already declared adapter dead, donot complete cmds. Also
	 * don't complete them if we are in recovery, as the command will have
	 * been pended.  If the pend queue finds that the command has already
	 * been completed, it will note an error as an "unexpected cmd". Let the
	 * pend queue handling after reset deal with completing commands then.
	 */
        spin_lock_irqsave(&instance->hba_lock, flags);
        if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
                spin_unlock_irqrestore(&instance->hba_lock, flags);
                printk("megasas: adapter %p(%d) recovery inprogress in completion routine: adprecovery = %d.\n",
                        instance,
                        instance->unique_id,
                        instance->adprecovery);
                return;
        }
#else
	/* If we have already declared adapter dead, donot complete cmds */
	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR ) { 
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		return;
	}

#endif
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	spin_lock_irqsave(&instance->completion_lock, flags);

	producer = *instance->producer;
	consumer = *instance->consumer;

	while (consumer != producer) {
		context = instance->reply_queue[consumer];

		if (context >= instance->max_fw_cmds) {
			printk("ERROR ERROR: unexpected context value %x\n", context);
			BUG();
		}
		cmd = instance->cmd_list[context];

		megasas_complete_cmd(instance, cmd, DID_OK);

		consumer++;
		if (consumer == (instance->max_fw_cmds + 1)) {
			consumer = 0;
		}
	}

	*instance->consumer = producer;
	spin_unlock_irqrestore(&instance->completion_lock, flags);
}

/*
 * Scsi host template for megaraid_sas driver
 */
static struct scsi_host_template megasas_template = {

	.module = THIS_MODULE,
	.name = "LSI Logic SAS based MegaRAID driver",
	.proc_name = "megaraid_sas",
	.slave_configure = megasas_slave_configure,
	.slave_alloc = megasas_slave_alloc,
	.queuecommand = megasas_queue_command,
#if defined(__VMKLNX__)
	.eh_abort_handler = megasas_abort,
#endif /* defined(__VMKLNX__) */
	.eh_device_reset_handler = megasas_reset_device,
	.eh_bus_reset_handler = megasas_reset_bus_host,
	.eh_host_reset_handler = megasas_reset_bus_host,
	.bios_param = megasas_bios_param,
	.use_clustering = ENABLE_CLUSTERING,
};

/**
 * megasas_complete_int_cmd -	Completes an internal command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 *
 * The megasas_issue_blocked_cmd() function waits for a command to complete
 * after it issues a command. This function wakes up that waiting routine by
 * calling wake_up() on the wait queue.
 */
static void
megasas_complete_int_cmd(struct megasas_instance *instance,
			 struct megasas_cmd *cmd)
{
	cmd->cmd_status = cmd->frame->io.cmd_status;

	if (cmd->cmd_status == ENODATA) {
		cmd->cmd_status = 0;
	}
	wake_up(&instance->int_cmd_wait_q);
}

/**
 * megasas_complete_abort -	Completes aborting a command
 * @instance:			Adapter soft state
 * @cmd:			Cmd that was issued to abort another cmd
 *
 * The megasas_issue_blocked_abort_cmd() function waits on abort_cmd_wait_q 
 * after it issues an abort on a previously issued command. This function 
 * wakes up all functions waiting on the same wait queue.
 */
static void
megasas_complete_abort(struct megasas_instance *instance,
		       struct megasas_cmd *cmd)
{
	if (cmd->sync_cmd) {
		cmd->sync_cmd = 0;
		cmd->cmd_status = 0;
		wake_up(&instance->abort_cmd_wait_q);
	}

	return;
}

/**
 * megasas_unmap_sgbuf -	Unmap SG buffers
 * @instance:			Adapter soft state
 * @cmd:			Completed command
 */
static void
megasas_unmap_sgbuf(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	dma_addr_t buf_h;
	u8 opcode;

	if (cmd->scmd->use_sg) {
		pci_unmap_sg(instance->pdev, cmd->scmd->request_buffer,
			     cmd->scmd->use_sg, cmd->scmd->sc_data_direction);
		return;
	}

	if (!cmd->scmd->request_bufflen)
		return;

	opcode = cmd->frame->hdr.cmd;

	if ((opcode == MFI_CMD_LD_READ) || (opcode == MFI_CMD_LD_WRITE)) {
		if (instance->flag_ieee) {
			buf_h = cmd->frame->io.sgl.sge_skinny[0].phys_addr;

		} else if (IS_DMA64)
			buf_h = cmd->frame->io.sgl.sge64[0].phys_addr;
		else
			buf_h = cmd->frame->io.sgl.sge32[0].phys_addr;
	} else {
		if (instance->flag_ieee) {
			buf_h = cmd->frame->pthru.sgl.sge_skinny[0].phys_addr;

		} else if (IS_DMA64)
			buf_h = cmd->frame->pthru.sgl.sge64[0].phys_addr;
		else
			buf_h = cmd->frame->pthru.sgl.sge32[0].phys_addr;
	}

	pci_unmap_single(instance->pdev, buf_h, cmd->scmd->request_bufflen,
			 cmd->scmd->sc_data_direction);
	return;
}

/**
 * megasas_complete_cmd -	Completes a command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 * @alt_status:			If non-zero, use this value as status to 
 * 				SCSI mid-layer instead of the value returned
 * 				by the FW. This should be used if caller wants
 * 				an alternate status (as in the case of aborted
 * 				commands)
 */
void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status)
{
	unsigned long flags;
	int exception = 0;
	u32	pd_index;
	struct megasas_header *hdr = &cmd->frame->hdr;
	int outstanding;
	struct fusion_context *fusion = instance->ctrl_context;

#if defined(__VMKLNX__)
	struct scsi_device * cmd_device = NULL;
#endif
       // If the commands complete successfully, the retry counter should also be reset
        // for future re-tries.
        cmd->retry_for_fw_reset = 0;

	if (cmd->scmd)
		cmd->scmd->SCp.ptr = NULL;

#if defined(__VMKLNX__)		
       if(cmd->scmd) {
           if(cmd->scmd->device) {
               cmd_device = cmd->scmd->device;
           }
       }
#endif

	switch (hdr->cmd) {

	case MFI_CMD_PD_SCSI_IO:
	case MFI_CMD_LD_SCSI_IO:

		/*
		 * MFI_CMD_PD_SCSI_IO and MFI_CMD_LD_SCSI_IO could have been
		 * issued either through an IO path or an IOCTL path. If it
		 * was via IOCTL, we will send it to internal completion.
		 */
		if (cmd->sync_cmd) {
			cmd->sync_cmd = 0;
			megasas_complete_int_cmd(instance, cmd);
			break;
		}
#if defined(__VMKLNX__)
		/*
		 * Don't export physical disk devices to mid-layer.
		 */
		if (!MEGASAS_IS_LOGICAL(cmd->scmd) &&
			(hdr->cmd_status == MFI_STAT_OK) &&
			(cmd->scmd->cmnd[0] == INQUIRY)) {
				if (((*(u8 *) cmd->scmd->request_buffer) & 0x1F) ==
					TYPE_DISK) {
					pd_index = MEGASAS_DEV_INDEX(instance, cmd->scmd);
					if (instance->pd_list[pd_index].driveState !=
							MR_PD_STATE_SYSTEM) {
						cmd->scmd->result = DID_BAD_TARGET << 16;
						exception = 1;
					}
				}
		}

#endif /* defined(__VMKLNX__) */

	case MFI_CMD_LD_READ:
	case MFI_CMD_LD_WRITE:

		if (alt_status) {
			cmd->scmd->result = alt_status << 16;
			exception = 1;
		}

		if (exception) {

			atomic_dec(&instance->fw_outstanding);

			megasas_unmap_sgbuf(instance, cmd);
			cmd->scmd->scsi_done(cmd->scmd);
			megasas_return_cmd(instance, cmd);

			break;
		}

		switch (hdr->cmd_status) {

		case MFI_STAT_OK:
			cmd->scmd->result = DID_OK << 16;
			break;

		case MFI_STAT_SCSI_IO_FAILED:
			if (hdr->scsi_status == SAM_STAT_CHECK_CONDITION) {
			
				cmd->scmd->result = (DID_OK << 16) | hdr->scsi_status;
				memset(cmd->scmd->sense_buffer, 0,
				       SCSI_SENSE_BUFFERSIZE);
				memcpy(cmd->scmd->sense_buffer, cmd->sense,
				       hdr->sense_len);

#if defined(__VMKLNX__)
                               if (!cmd->scmd->sense_buffer[0]) {
                                       printk(KERN_INFO "megasas: cmd %x sn %ld CHKCOND "
                                         "invalid sense buffer sk %x sb_len %x\n",
                                         cmd->scmd->cmnd[0], cmd->scmd->serial_number,
                                         cmd->scmd->sense_buffer[2] & 0xf,
                                         hdr->sense_len);

                                       /*
                                        * PR482700/PR505654- device didn't fill in sense
                                        * buffer, set the first byte to 0x70 (fixed format)
                                        * and set sk to 0 (no sense) for now
                                        */
                                       cmd->scmd->sense_buffer[0] = 0x70;
                                       cmd->scmd->sense_buffer[2] = 0;
                               }
#endif /* defined(__VMKLNX__) */

				cmd->scmd->result |= DRIVER_SENSE << 24;
			} else {

				cmd->scmd->result =
			    		(DID_ERROR << 16) | hdr->scsi_status;
			}

			break;

		case MFI_STAT_LD_INIT_IN_PROGRESS:
			cmd->scmd->result =
			    (DID_ERROR << 16) | hdr->scsi_status;
			break;

		case MFI_STAT_SCSI_DONE_WITH_ERROR:

			cmd->scmd->result = (DID_OK << 16) | hdr->scsi_status;

			if (hdr->scsi_status == SAM_STAT_CHECK_CONDITION) {
				memset(cmd->scmd->sense_buffer, 0,
				       SCSI_SENSE_BUFFERSIZE);
				memcpy(cmd->scmd->sense_buffer, cmd->sense,
				       hdr->sense_len);

#if defined(__VMKLNX__)
				if (!cmd->scmd->sense_buffer[0]) {
					printk(KERN_INFO "megasas: cmd %x sn %ld CHKCOND "
					  "invalid sense buffer sk %x sb_len %x\n",
		 			  cmd->scmd->cmnd[0], cmd->scmd->serial_number,
					  cmd->scmd->sense_buffer[2] & 0xf,
					  hdr->sense_len);

					/*
					 * PR482700/PR505654- device didn't fill in sense
					 * buffer, set the first byte to 0x70 (fixed format)
					 * and set sk to 0 (no sense) for now
					 */
					cmd->scmd->sense_buffer[0] = 0x70;
					cmd->scmd->sense_buffer[2] = 0;
				}
#endif /* defined(__VMKLNX__) */

				cmd->scmd->result |= DRIVER_SENSE << 24;
			}

			break;

		case MFI_STAT_LD_OFFLINE:
		case MFI_STAT_DEVICE_NOT_FOUND:
		/* PR 560419: Returning BAD_TARGET breaks VMware Pluggable Storage Architecture 
		   (PSA) assumptions. BAD_TARGET should only be returned when the target is out
		   of range or otherwise truly invalid, not just offline or not reporting. Need
		   to return DID_NO_CONNECT instead of DID_BAD_TARGET in such cases.
		 */
#if defined(__VMKLNX__)
			cmd->scmd->result = DID_NO_CONNECT << 16;
#else
			cmd->scmd->result = DID_BAD_TARGET << 16;
#endif
			break;

		default:
			printk(KERN_DEBUG "megasas: MFI FW status %#x\n",
			       hdr->cmd_status);
			cmd->scmd->result = DID_ERROR << 16;
			break;
		}

		atomic_dec(&instance->fw_outstanding);

		megasas_unmap_sgbuf(instance, cmd);
		cmd->scmd->scsi_done(cmd->scmd);
		megasas_return_cmd(instance, cmd);

		break;

	case MFI_CMD_SMP:
	case MFI_CMD_STP:
	case MFI_CMD_DCMD:
		/* Check for LD map update */
		if ((cmd->frame->dcmd.opcode == MR_DCMD_LD_MAP_GET_INFO) &&
			(cmd->frame->dcmd.mbox.b[1] == 1)) {
			fusion->fast_path_io = 0;
			spin_lock_irqsave(instance->host->host_lock, flags);
			if (cmd->frame->hdr.cmd_status != 0) {
				if (cmd->frame->hdr.cmd_status != MFI_STAT_NOT_FOUND)
					printk(KERN_WARNING "megasas: map sync failed, status = 0x%x.\n", cmd->frame->hdr.cmd_status);
				else {
					megasas_return_cmd(instance,cmd);
					spin_unlock_irqrestore(instance->host->host_lock, flags);
					break;
				}
			} else
				instance->map_id++;
			megasas_return_cmd(instance, cmd);
			// Set fast path io to ZERO. Validate Map will set proper value. Meanwhile all IOs will go 
			// as LD IO.
			if (MR_ValidateMapInfo(instance))
				fusion->fast_path_io = 1;
			else
				fusion->fast_path_io = 0;
			megasas_sync_map_info(instance);
			spin_unlock_irqrestore(instance->host->host_lock, flags);
			break;
		}
/*		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET_INFO || 
			cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET) {
			spin_lock_irqsave(&poll_aen_lock, flags);
			megasas_poll_wait_aen = 0;
			spin_unlock_irqrestore(&poll_aen_lock, flags);
		}*/
		/*
		 * See if got an event notification
		 */
		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_WAIT)
			megasas_service_aen(instance, cmd);
		else
			megasas_complete_int_cmd(instance, cmd);

		break;

	case MFI_CMD_ABORT:
		/*
		 * Cmd issued to abort another cmd returned
		 */
		megasas_complete_abort(instance, cmd);
		break;

	default:
		printk("megasas: Unknown command completed! [0x%X]\n",
		       hdr->cmd);
		break;
	}
	
#if defined(__VMKLNX__)
	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->in_taskmgmt == 0 && instance->is_busy) {
		outstanding = atomic_read(&instance->fw_outstanding);
		if(outstanding< THROTTLE_MAX_CMD_PER_LUN) {
			/* Unthrottle I/O */
			instance->is_busy=0;
			instance->cmd_per_lun = MAX_CMD_PER_LUN;
			printk("megasas_complete_cmd: Adjusting scsi queue depth to %d\n",
			   instance->cmd_per_lun);
			if(cmd_device) {
				scsi_adjust_queue_depth(cmd_device, MSG_SIMPLE_TAG, instance->cmd_per_lun);
			}
			TRACK_IS_BUSY("megasas_complete_cmd",instance->is_busy)
		}
	}
	spin_unlock_irqrestore(&instance->hba_lock, flags);
#else
	if(instance->is_busy){
		outstanding = atomic_read(&instance->fw_outstanding);
		if(outstanding<17)
			instance->is_busy=0;
	}
#endif
}

 /**
 * megasas_issue_pending_cmds_again -	issue all pending cmds
 *                              	in FW again because of the fw reset
 * @instance:				Adapter soft state
 */
static void megasas_issue_pending_cmds_again(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct list_head clist_local;
	union megasas_evt_class_locale class_locale;
	unsigned long flags;
	u32 seq_num;

	INIT_LIST_HEAD(&clist_local);
	spin_lock_irqsave(&instance->hba_lock, flags);
	list_splice_init(&instance->internal_reset_pending_q, &clist_local);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	while (!list_empty(&clist_local)) {
		cmd	= list_entry((&clist_local)->next, struct megasas_cmd, list);
		list_del_init(&cmd->list);

               if (cmd->sync_cmd || cmd->scmd) {
                       printk("megaraid_sas: command %p, %p:%d detected to be pending while HBA reset.\n", cmd, cmd->scmd, cmd->sync_cmd);

                       cmd->retry_for_fw_reset++;

                       // If a command has continuously been tried multiple times and causing
                       // a FW reset condition, no further recoveries should be performed on
                       // the controller
                       if (cmd->retry_for_fw_reset == 3) {
                               printk("megaraid_sas: command %p, %p:%d was tried multiple times during adapter reset. Shutting down the HBA\n", cmd, cmd->scmd, cmd->sync_cmd);
                               megaraid_sas_kill_hba(instance);

				instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
                               return;
                       }
               }

		if (cmd->sync_cmd == 1) {
			if (cmd->scmd) {
				printk("megaraid_sas: unexpected SCSI command attached to internal command!\n");
			}
			printk("megasas: %p synchronous command detected on the internal reset queue, issue it again.\n", cmd);
			cmd->cmd_status = ENODATA;
			instance->instancet->fire_cmd(instance,cmd->frame_phys_addr ,0,instance->reg_set);
		} else if (cmd->scmd) {
			printk("megasas: %p scsi command [%02x], %#lx detected on the internal reset queue, issue it again.\n", cmd, cmd->scmd->cmnd[0], cmd->scmd->serial_number);
			atomic_inc(&instance->fw_outstanding);
			instance->instancet->fire_cmd(instance, cmd->frame_phys_addr ,cmd->frame_count-1,instance->reg_set);
		}
		else {
			printk("megasas: %p unexpected command on the internal reset defer list while re-issue!!\n", cmd);
		}
	}

	// Re-register AEN
	if (instance->aen_cmd) {
		printk("megaraid_sas: existing aen_cmd discovered in deferred processing, freeing...\n");
		megasas_return_cmd(instance, instance->aen_cmd);
		instance->aen_cmd	= NULL;
	}

	/*
	* Initiate AEN (Asynchronous Event Notification)
	*/
	seq_num = instance->last_seq_num;
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	megasas_register_aen(instance, seq_num, class_locale.word);
}


/**
 * Move the internal reset pending commands to a deferred queue.
 *
 * We move the commands pending at internal reset time to a pending queue. This queue would
 * be flushed after successful completion of the internal reset sequence.
 * if the internal reset did not complete in time, the kernel reset handler would flush these
 * commands.
 * Note: make sure that spin_lock_irqsave(&instance->hba_lock,hba_flags) is called 
 * if defined(__VMKLNX__) before entering this function, referring to PR#878045
 **/
static void megasas_internal_reset_defer_cmds(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i;
	u32 max_cmd = instance->max_fw_cmds;
	u32 defer_index;
	unsigned long flags;

	defer_index	= 0;
	spin_lock_irqsave(&instance->cmd_pool_lock, flags);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if (cmd->sync_cmd == 1 || cmd->scmd) {
			printk("megasas: moving cmd[%d]:%p:%d:%p on the defer queue as internal reset in progress.\n",
				defer_index, cmd, cmd->sync_cmd, cmd->scmd);
			if (!list_empty(&cmd->list)) {
				printk("megaraid_sas: ERROR while moving this cmd:%p, %d %p, it was discovered on some list?\n", cmd, cmd->sync_cmd, cmd->scmd);
				list_del_init(&cmd->list);
			}
			defer_index++;
			list_add_tail(&cmd->list, &instance->internal_reset_pending_q);
		}
	}
	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}

static void process_fw_state_change_wq(struct work_struct *work)
{
	unsigned long flags;

	struct megasas_instance *instance =  container_of(work, struct megasas_instance, work_init);
	printk(KERN_DEBUG "megaraid_sas: process_fw_state_change_wq:  instance addr:  0x%p, adprecovery:  0x%X\n", 
	instance, instance->adprecovery);
        
	if (instance->adprecovery != MEGASAS_ADPRESET_SM_INFAULT) {
                printk("megaraid_sas: error, unexpected adapter recovery state %x in %s\n", instance->adprecovery, __FUNCTION__);
                return;
        }

	if (instance->adprecovery == MEGASAS_ADPRESET_SM_INFAULT) {
		printk("megaraid_sas: FW detected to be in fault state, restarting it...\n");

		instance->instancet->disable_intr(instance);
                atomic_set(&instance->fw_outstanding, 0);

                atomic_set(&instance->fw_reset_no_pci_access, 1);
                instance->instancet->adp_reset(instance, instance->reg_set);
                atomic_set(&instance->fw_reset_no_pci_access, 0 );

		printk("megaraid_sas: FW was restarted successfully, initiating next stage...\n");

		printk("megaraid_sas: HBA recovery state machine, state 2 starting...\n");

                /*waitting for about 30 second before start the second init*/
		msleep(30000);

                if (megasas_transition_to_ready(instance, 1))
                {
                        printk("megaraid_sas: out: controller is not in ready state\n");

                        megaraid_sas_kill_hba(instance);
			instance->adprecovery	= MEGASAS_HW_CRITICAL_ERROR;
                        return ;
                }

                if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1064R) ||
                        (instance->pdev->device == PCI_DEVICE_ID_DELL_PERC5) ||
                        (instance->pdev->device == PCI_DEVICE_ID_LSI_VERDE_ZCR))
                {
                        *instance->consumer = *instance->producer;
                } else {
                        *instance->consumer = 0;
                        *instance->producer = 0;
                }

		// Transition the FW to operational state
		megasas_issue_init_mfi(instance);

		/*
		 * Setting the adapter to OPERATIONAL at this point is very important. This would
		 * prevent other subsystems (reset, aen, and ioctls) to block till the recovery
		 * logic has run it's course
		 */
		spin_lock_irqsave(&instance->hba_lock, flags);
		instance->adprecovery	= MEGASAS_HBA_OPERATIONAL;
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		instance->instancet->enable_intr(instance);

		printk("megaraid_sas: second stage of reset complete, FW is ready now.\n");
		megasas_issue_pending_cmds_again(instance);

		instance->issuepend_done = 1;


	}
	return ;
}

/**
 * megasas_process_interrupt -	Processes all completed commands
 * @instance:				Adapter soft state
 * @alt_status:				Alternate status to be returned to
 * 					SCSI mid-layer instead of the status
 * 					returned by the FW
 * Note: this must be called with hba lock held
 */
#if defined(__VMKLNX__)
static irqreturn_t
megasas_process_interrupt(struct megasas_instance *instance, u8 alt_status, u8 * schedule_tasklet)
#else
static irqreturn_t
megasas_process_interrupt(struct megasas_instance *instance, u8 alt_status)
#endif
{
	u32 mfiStatus;
	u32 fw_state;

	// If the adapter is under a reset recovery, all interrupts coming from it must be acknowledged
	// if the consumer pointer value indicates so.
        if((mfiStatus = instance->instancet->check_reset(instance, instance->reg_set)) == 1) {
                return IRQ_HANDLED;
        }

	// Clear the interrupt on the HBA
	if((mfiStatus = instance->instancet->clear_intr(instance->reg_set)) == 0) {
		/* Hardware may not set outbound_intr_status in MSI-X mode */
		if (!instance->msix_vectors)
			return IRQ_NONE;
	}

	instance->mfiStatus = mfiStatus;

	// If the current soft state indicates an OPERATIONAL state _and_ now we have
	// detected state change, this should be FW FAULT case.
	if ((mfiStatus & MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE)) {
		fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;

                if (fw_state != MFI_STATE_FAULT) {
                        printk("megaraid_sas: fw state while internal state change operational, state:%x\n", fw_state);
                }

               	if ((fw_state == MFI_STATE_FAULT) &&
			(instance->disableOnlineCtrlReset == 0) &&
			(instance->issuepend_done == 1)) {
			printk("megaraid_sas: adapter reset condition is detected, waiting for it to restart...\n");

                       	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1064R) ||
                               	(instance->pdev->device == PCI_DEVICE_ID_DELL_PERC5) ||
                               	(instance->pdev->device == PCI_DEVICE_ID_LSI_VERDE_ZCR))
                               	{
					*instance->consumer	= MEGASAS_ADPRESET_INPROG_SIGN;
                       	}


			instance->instancet->disable_intr(instance);
			instance->adprecovery	= MEGASAS_ADPRESET_SM_INFAULT;    // indicates adapter restart stage 1 is in progress
			instance->issuepend_done = 0;

			// The pending commands are moved to a deferred list. We would pick commands up and
			// re-issue once the reset processing is over.
			atomic_set(&instance->fw_outstanding, 0);
			megasas_internal_reset_defer_cmds(instance);

			// Schedule a low-priorty thread to perform the function for current stage of
			// adapter reset state machine.
                       	printk("megaraid_sas: FW state detected, current:%x, reset stage:%d\n", fw_state, instance->adprecovery);
			schedule_work(&instance->work_init);
			return IRQ_HANDLED;
		}
		else {
			printk("megaraid_sas: fw state while internal state changes, state:%x, disableOCR=%x\n",
				fw_state, instance->disableOnlineCtrlReset);
		}
	}

#if defined(__VMKLNX__)
	/* Schedule the tasklet for cmd completion.  Note that the hba_lock is NOT held, as
	 * the tasklet will be taking the hba_lock in the COREDUMP case, causing a deadlock
	 * in that situation.
	 */
	*schedule_tasklet = 1;
#else
	// Schedule the tasklet for cmd completion
	tasklet_schedule(&instance->isr_tasklet);
#endif
	return IRQ_HANDLED;
}

/**
 * megasas_isr - isr entry point
 */
#if defined(__VMKLNX__)
static irqreturn_t megasas_isr(int irq, void *devp, struct pt_regs *r)
#else
static irqreturn_t megasas_isr(int irq, void *devp)
#endif /* !defined(__VMKLNX__) */
{
	struct megasas_irq_context *irq_context = devp;
	struct megasas_instance *instance = irq_context->instance;
	unsigned long flags;
	irqreturn_t	rc;
#if defined(__VMKLNX__)  
	u8 schedule_tasklet = 0;
#endif
      
	if (atomic_read(&instance->fw_reset_no_pci_access))
		return IRQ_HANDLED;

   /* PR329916, PR572321: In the coredump code path, megasas_complete_cmd_tasklet 
    * and later megasas_complete_cmd are invoked by the top half interrupt handler 
    * in the non-interrupt mode. The same hba_lock spinlock is already taken in 
    * the top half interrupt handler. Do not take the hba_lock again to avoid the 
    * deadlock.  
    *
    * Since the tasklet is NOT scheduled in all circumstances where IRQ_HANDLED
    * is returned, the schedule_tasklet flag lets us know when it's truly necessary
    * to actually do so.
   */
#if defined(__VMKLNX__)  
	spin_lock_irqsave(&instance->hba_lock, flags);
	rc = megasas_process_interrupt(instance, DID_OK, &schedule_tasklet);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
	if (schedule_tasklet) {
		tasklet_schedule(&instance->isr_tasklet);
	}
#else
	spin_lock_irqsave(&instance->hba_lock, flags);
	rc = megasas_process_interrupt(instance, DID_OK);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

#endif
	return rc;
}

/**
 * megasas_transition_to_ready -	Move the FW to READY state
 * @instance:				Adapter soft state
 *
 * During the initialization, FW passes can potentially be in any one of
 * several possible states. If the FW in operational, waiting-for-handshake
 * states, driver must take steps to bring it to ready state. Otherwise, it
 * has to wait for the ready state.
 */
int
megasas_transition_to_ready(struct megasas_instance* instance, int ocr)
{
	int i;
	u8 max_wait;
	u32 fw_state;
	u32 cur_state;
	u32 abs_state, curr_abs_state;

	fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;

	if (fw_state != MFI_STATE_READY)
 		printk(KERN_INFO "megasas: Waiting for FW to come to ready"
 		       " state\n");

	while (fw_state != MFI_STATE_READY) {
	
		abs_state = instance->instancet->read_fw_status_reg(instance->reg_set);

		switch (fw_state) {

		case MFI_STATE_FAULT:

			printk(KERN_DEBUG "megasas: FW in FAULT state!!\n");
			if (ocr) {
				max_wait = MEGASAS_RESET_WAIT_TIME;
				cur_state = MFI_STATE_FAULT;
				break;
			} else
				return -ENODEV;

		case MFI_STATE_WAIT_HANDSHAKE:
			/*
			 * Set the CLR bit in inbound doorbell
			 */
			if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY))
			{
				writel(MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
					&instance->reg_set->doorbell);
			} else {
				writel(MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);
			}

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_WAIT_HANDSHAKE;
			break;

		case MFI_STATE_BOOT_MESSAGE_PENDING:
			if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
			{
				writel(MFI_INIT_HOTPLUG,
					&instance->reg_set->doorbell);
			} else 
				writel(MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BOOT_MESSAGE_PENDING;
			break;

		case MFI_STATE_OPERATIONAL:
			/*
			 * Bring it to READY state; assuming max wait 10 secs
			 */
			instance->instancet->disable_intr(instance);
			if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY) ||
			    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER))
			{
				writel(MFI_RESET_FLAGS, &instance->reg_set->doorbell);
				if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
				    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
				    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY)) {
					for (i = 0; i < (10 * 1000); i++) {
						if (readl(&instance->reg_set->doorbell) & 1)
							msleep(1);
						else
							break;
					}
				}
			} else 
				writel(MFI_RESET_FLAGS, &instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_OPERATIONAL;
			break;

		case MFI_STATE_UNDEFINED:
			/*
			 * This state should not last for more than 2 seconds
			 */
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_UNDEFINED;
			break;

		case MFI_STATE_BB_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BB_INIT;
			break;

		case MFI_STATE_FW_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT;
			break;

		case MFI_STATE_FW_INIT_2:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT_2;
			break;

		case MFI_STATE_DEVICE_SCAN:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_DEVICE_SCAN;
			break;

		case MFI_STATE_FLUSH_CACHE:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FLUSH_CACHE;
			break;

		default:
			printk(KERN_DEBUG "megasas: Unknown state 0x%x\n",
			       fw_state);
			return -ENODEV;
		}

		/*
		 * The cur_state should not last for more than max_wait secs
		 */
		for (i = 0; i < (max_wait * 1000); i++) {
			fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) &  
					MFI_STATE_MASK ;
			curr_abs_state = instance->instancet->read_fw_status_reg(instance->reg_set);

			if (abs_state == curr_abs_state) {
				msleep(1);
			} else
				break;
		}

		/*
		 * Return error if fw_state hasn't changed after max_wait
		 */
		if (curr_abs_state == abs_state) {
			printk(KERN_DEBUG "FW state [%d] hasn't changed "
			       "in %d secs\n", fw_state, max_wait);
			return -ENODEV;
		}
	}
 	printk("megasas: FW now in Ready state\n");

	return 0;
}

/**
 * megasas_teardown_frame_pool -	Destroy the cmd frame DMA pool
 * @instance:				Adapter soft state
 */
static void megasas_teardown_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd = instance->max_mfi_cmds;
	struct megasas_cmd *cmd;

	if (!instance->frame_dma_pool)
		return;

	/*
	 * Return all frames to pool
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if (cmd->frame)
			pci_pool_free(instance->frame_dma_pool, cmd->frame,
				      cmd->frame_phys_addr);

		if (cmd->sense)
			pci_pool_free(instance->sense_dma_pool, cmd->sense,
				      cmd->sense_phys_addr);
	}

	/*
	 * Now destroy the pool itself
	 */
	pci_pool_destroy(instance->frame_dma_pool);
	pci_pool_destroy(instance->sense_dma_pool);

	instance->frame_dma_pool = NULL;
	instance->sense_dma_pool = NULL;
}

/**
 * megasas_create_frame_pool -	Creates DMA pool for cmd frames
 * @instance:			Adapter soft state
 *
 * Each command packet has an embedded DMA memory buffer that is used for
 * filling MFI frame and the SG list that immediately follows the frame. This
 * function creates those DMA memory buffers for each command packet by using
 * PCI pool facility.
 */
static int megasas_create_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd;
	u32 sge_sz;
	u32 sgl_sz;
	u32 total_sz;
	u32 frame_count;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_mfi_cmds;

	/*
	 * Size of our frame is 64 bytes for MFI frame, followed by max SG
	 * elements and finally SCSI_SENSE_BUFFERSIZE bytes for sense buffer
	 */
	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	/*
	 * Calculated the number of 64byte frames required for SGL
	 */
	sgl_sz = sge_sz * instance->max_num_sge;
	frame_count = 15;

	/*
	 * We need one extra frame for the MFI command
	 */
	frame_count++;

	total_sz = MEGAMFI_FRAME_SIZE * frame_count;
	/*
	 * Use DMA pool facility provided by PCI layer
	 */
	instance->frame_dma_pool = pci_pool_create("megasas frame pool",
						   instance->pdev, total_sz, 64,
						   0);

	if (!instance->frame_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup frame pool\n");
		return -ENOMEM;
	}

	instance->sense_dma_pool = pci_pool_create("megasas sense pool",
						   instance->pdev, 128, 4, 0);

	if (!instance->sense_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup sense pool\n");

		pci_pool_destroy(instance->frame_dma_pool);
		instance->frame_dma_pool = NULL;

		return -ENOMEM;
	}

	/*
	 * Allocate and attach a frame to each of the commands in cmd_list.
	 * By making cmd->index as the context instead of the &cmd, we can
	 * always use 32bit context regardless of the architecture
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		cmd->frame = pci_pool_alloc(instance->frame_dma_pool,
					    GFP_KERNEL, &cmd->frame_phys_addr);

		memset(cmd->frame, 0, total_sz);

		cmd->sense = pci_pool_alloc(instance->sense_dma_pool,
					    GFP_KERNEL, &cmd->sense_phys_addr);

		/*
		 * megasas_teardown_frame_pool() takes care of freeing
		 * whatever has been allocated
		 */
		if (!cmd->frame || !cmd->sense) {
			printk(KERN_DEBUG "megasas: pci_pool_alloc failed \n");
			megasas_teardown_frame_pool(instance);
			return -ENOMEM;
		}

		cmd->frame->io.context = cmd->index;

		/*
		 * Initialize pad_0 to 0, otherwise it could corrupt
		 * the value of context and cause FW crash
		 */
		cmd->frame->io.pad_0 = 0;
	}

	return 0;
}

/**
 * megasas_free_cmds -	Free all the cmds in the free cmd pool
 * @instance:		Adapter soft state
 */
void megasas_free_cmds(struct megasas_instance *instance)
{
	int i;
	/* First free the MFI frame pool */
	megasas_teardown_frame_pool(instance);

	/* Free all the commands in the cmd_list */
	for (i = 0; i < instance->max_mfi_cmds; i++)
		kfree(instance->cmd_list[i]);

	/* Free the cmd_list buffer itself */
	kfree(instance->cmd_list);
	instance->cmd_list = NULL;

	INIT_LIST_HEAD(&instance->cmd_pool);
}

/**
 * megasas_alloc_cmds -	Allocates the command packets
 * @instance:		Adapter soft state
 *
 * Each command that is issued to the FW, whether IO commands from the OS or
 * internal commands like IOCTLs, are wrapped in local data structure called
 * megasas_cmd. The frame embedded in this megasas_cmd is actually issued to
 * the FW.
 *
 * Each frame has a 32-bit field called context (tag). This context is used
 * to get back the megasas_cmd from the frame when a frame gets completed in
 * the ISR. Typically the address of the megasas_cmd itself would be used as
 * the context. But we wanted to keep the differences between 32 and 64 bit
 * systems to the mininum. We always use 32 bit integers for the context. In
 * this driver, the 32 bit values are the indices into an array cmd_list.
 * This array is used only to look up the megasas_cmd given the context. The
 * free commands themselves are maintained in a linked list called cmd_pool.
 */
int megasas_alloc_cmds(struct megasas_instance *instance)
{
	int i;
	int j;
	u32 max_cmd;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_mfi_cmds;

	/*
	 * instance->cmd_list is an array of struct megasas_cmd pointers.
	 * Allocate the dynamic array first and then allocate individual
	 * commands.
	 */
	instance->cmd_list = kmalloc(sizeof(struct megasas_cmd *) * max_cmd,
				     GFP_KERNEL);

	if (!instance->cmd_list) {
		printk(KERN_DEBUG "megasas: out of memory\n");
		return -ENOMEM;
	}

	memset(instance->cmd_list, 0, sizeof(struct megasas_cmd *) * max_cmd);

	for (i = 0; i < max_cmd; i++) {
		instance->cmd_list[i] = kmalloc(sizeof(struct megasas_cmd),
						GFP_KERNEL);

		if (!instance->cmd_list[i]) {

			for (j = 0; j < i; j++)
				kfree(instance->cmd_list[j]);

			kfree(instance->cmd_list);
			instance->cmd_list = NULL;

			return -ENOMEM;
		}

		memset(instance->cmd_list[i], 0, sizeof(struct megasas_cmd));
	}

	/*
	 * Add all the commands to command pool (instance->cmd_pool)
	 */
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		memset(cmd, 0, sizeof(struct megasas_cmd));
		cmd->index = i;
		cmd->scmd = NULL;
		cmd->instance = instance;

		list_add_tail(&cmd->list, &instance->cmd_pool);
	}

	/*
	 * Create a frame pool and assign one frame to each cmd
	 */
	if (megasas_create_frame_pool(instance)) {
		printk(KERN_DEBUG "megasas: Error creating frame DMA pool\n");
		megasas_free_cmds(instance);
	}

	return 0;
}

/**
 * megasas_get_pd_list_info -	Returns FW's pd_list structure
 * @instance:				Adapter soft state
 * @pd_list:				pd_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_pd_list(struct megasas_instance *instance)
{
	int ret = 0, pd_index = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_PD_LIST *ci;
	struct MR_PD_ADDRESS *pd_addr;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_pd_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for pd_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.b[0] = MR_PD_QUERY_TYPE_EXPOSED_TO_HOST;
	dcmd->mbox.b[1] = 0;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);
	dcmd->opcode = MR_DCMD_PD_LIST_QUERY;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;

	} else {
		ret = -1;
	}

	/*
	* the following function will get the instance PD LIST.
	*/

	pd_addr = ci->addr;

	if ( ret == 0 &&
		(ci->count < (MEGASAS_MAX_PD_CHANNELS * MEGASAS_MAX_DEV_PER_CHANNEL))){

		memset(instance->local_pd_list, 0,
			MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));

		for (pd_index = 0; pd_index < ci->count; pd_index++) {

			instance->local_pd_list[pd_addr->deviceId].tid	=
							pd_addr->deviceId;
			instance->local_pd_list[pd_addr->deviceId].driveType	=
							pd_addr->scsiDevType;
			instance->local_pd_list[pd_addr->deviceId].driveState	=
							MR_PD_STATE_SYSTEM;

			pd_addr++;
		}

	}

	pci_free_consistent(instance->pdev, MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST),
				ci, ci_h); 

	memcpy(instance->pd_list, instance->local_pd_list, sizeof(instance->pd_list));

	megasas_return_cmd(instance, cmd);

	return ret;

}

/**
 * megasas_get_ld_list_info -	Returns FW's ld_list structure
 * @instance:				Adapter soft state
 * @ld_list:				ld_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_ld_list(struct megasas_instance *instance)
{
	int ret = 0, ld_index = 0, ids = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_LIST *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (megasas_get_ld_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev, sizeof(struct MR_LD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for megasas_get_ld_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->data_xfer_len = sizeof(struct MR_LD_LIST);
	dcmd->opcode = MR_DCMD_LD_GET_LIST;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct MR_LD_LIST);
	dcmd->pad_0  = 0;

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;

	} else {
		ret = -1;
	}

	/*
	* the following function will get the instance LD LIST.
	*/

	if ( (ret == 0) && (ci->ldCount <= (MAX_LOGICAL_DRIVES))){
		instance->CurLdCount = ci->ldCount;
		memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);

		for (ld_index = 0; ld_index < ci->ldCount; ld_index++) {
			if (ci->ldList[ld_index].state != 0) {
				ids = ci->ldList[ld_index].ref.targetId;
				instance->ld_ids[ids] = ci->ldList[ld_index].ref.targetId;
			}
								
		}

	}

	pci_free_consistent(instance->pdev, sizeof(struct MR_LD_LIST), ci, ci_h); 
		

	megasas_return_cmd(instance, cmd);

	return ret;
}

/**
 * megasas_get_controller_info -	Returns FW's controller structure
 * @instance:				Adapter soft state
 * @ctrl_info:				Controller information structure
 *
 * Issues an internal command (DCMD) to get the FW's controller structure.
 * This information is mainly used to find out the maximum IO transfer per
 * command supported by the FW.
 */
static int
megasas_get_ctrl_info(struct megasas_instance *instance,
		      struct megasas_ctrl_info *ctrl_info)
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_ctrl_info *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a free cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  sizeof(struct megasas_ctrl_info), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for ctrl info\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_ctrl_info);
	dcmd->opcode = MR_DCMD_CTRL_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_ctrl_info);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
		memcpy(ctrl_info, ci, sizeof(struct megasas_ctrl_info));
	} else {
		ret = -1;
	}

	pci_free_consistent(instance->pdev, sizeof(struct megasas_ctrl_info),
			    ci, ci_h);

	megasas_return_cmd(instance, cmd);
	return ret;
}

/**
 * megasas_issue_init_mfi -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * Issues the INIT MFI cmd
 */
static int
megasas_issue_init_mfi(struct megasas_instance *instance)
{
	u32 context;

	struct megasas_cmd *cmd;

	struct megasas_init_frame *init_frame;
	struct megasas_init_queue_info *initq_info;
	dma_addr_t init_frame_h;
	dma_addr_t initq_info_h;

	/*
	 * Prepare a init frame. Note the init frame points to queue info
	 * structure. Each frame has SGL allocated after first 64 bytes. For
	 * this frame - since we don't need any SGL - we use SGL's space as
	 * queue info structure
	 *
	 * We will not get a NULL command below. We just created the pool.
	 */
	cmd = megasas_get_cmd(instance);

	init_frame = (struct megasas_init_frame *)cmd->frame;
	initq_info = (struct megasas_init_queue_info *)
	    ((unsigned long)init_frame + 64);

	init_frame_h = cmd->frame_phys_addr;
	initq_info_h = init_frame_h + 64;

	context = init_frame->context;
	memset(init_frame, 0, MEGAMFI_FRAME_SIZE);
	memset(initq_info, 0, sizeof(struct megasas_init_queue_info));
	init_frame->context = context;
	
	initq_info->reply_queue_entries = instance->max_fw_cmds + 1;
	initq_info->reply_queue_start_phys_addr_lo = instance->reply_queue_h;

	initq_info->producer_index_phys_addr_lo = instance->producer_h;
	initq_info->consumer_index_phys_addr_lo = instance->consumer_h;

	init_frame->cmd = MFI_CMD_INIT;
	init_frame->cmd_status = 0xFF;

	if (instance->verbuf) {
		snprintf((char *)instance->verbuf, strlen(MEGASAS_VERSION) + 2, "%s\n",
			 MEGASAS_VERSION);

		init_frame->driver_ver_lo = instance->verbuf_h;
		init_frame->driver_ver_hi = 0;
	}

	init_frame->queue_info_new_phys_addr_lo = initq_info_h;

	init_frame->data_xfer_len = sizeof(struct megasas_init_queue_info);

	/*
	 * disable the intr before firing the init frame to FW 
	 */	
	instance->instancet->disable_intr(instance);
	
	/*
	 * Issue the init frame in polled mode
	 */

	if (megasas_issue_polled(instance, cmd)) {
		printk(KERN_DEBUG "megasas: Failed to init firmware\n");
		megasas_return_cmd(instance, cmd);
		goto fail_fw_init;
	}

	megasas_return_cmd(instance, cmd);

	return 0;

	fail_fw_init:
		return -EINVAL;
}

/**
 * megasas_start_timer - Initializes a timer object
 * @instance:		Adapter soft state
 * @timer:		timer object to be initialized
 * @fn:			timer function
 * @interval:		time interval between timer function call
 *
 */
static inline void
megasas_start_timer(struct megasas_instance *instance,
			struct timer_list *timer,
			void *fn, unsigned long interval)
{
	init_timer(timer);
	timer->expires = jiffies + interval;
	timer->data = (unsigned long)instance;
	timer->function = fn;
	add_timer(timer);
}
static void megasas_internal_reset_defer_cmds(struct megasas_instance *instance);

void megasas_do_ocr(struct megasas_instance *instance)
{
#if defined(__VMKLNX__)
	unsigned long flags;
      printk("megaraid_sas: megasas_do_ocr: line %d: pdev->device:  0x%X :: "
                   "disableonlineCtrlReset: 0x%X :: issuepend_done: 0x%X\n",
                   __LINE__, 
                   instance->pdev->device, 
                   instance->disableOnlineCtrlReset, 
                   instance->issuepend_done);
#endif
       if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1064R) ||
               (instance->pdev->device == PCI_DEVICE_ID_DELL_PERC5) ||
               (instance->pdev->device == PCI_DEVICE_ID_LSI_VERDE_ZCR))
       {
               *instance->consumer     = MEGASAS_ADPRESET_INPROG_SIGN;
       }

       instance->instancet->disable_intr(instance);
#if defined(__VMKLNX__)
       spin_lock_irqsave(&instance->hba_lock, flags);
#endif
       instance->adprecovery   = MEGASAS_ADPRESET_SM_INFAULT;

       atomic_set(&instance->fw_outstanding, 0);
       megasas_internal_reset_defer_cmds(instance);
#if defined(__VMKLNX__)
       spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif
       /*process_fw_state_change_wq(instance);*/
	schedule_work(&instance->work_init);
}

static u32
megasas_init_adapter_mfi(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *reg_set;
	u32 context_sz;
	u32 reply_q_sz;

	reg_set = instance->reg_set;

	/*
	 * Get various operational parameters from status register
	 */
	instance->max_fw_cmds = instance->instancet->read_fw_status_reg(reg_set) & 0x00FFFF;
	/*
	 * Reduce the max supported cmds by 1. This is to ensure that the
	 * reply_q_sz (1 more than the max cmd that driver may send)
	 * does not exceed max cmds that the FW can support
	 */
	instance->max_fw_cmds = instance->max_fw_cmds-1;
	instance->max_mfi_cmds = instance->max_fw_cmds;
	instance->max_num_sge = (instance->instancet->read_fw_status_reg(reg_set) & 0xFF0000) >> 
					0x10;
	/*
	 * Create a pool of commands
	 */
	if (megasas_alloc_cmds(instance))
		goto fail_alloc_cmds;

	/*
	 * Allocate memory for reply queue. Length of reply queue should
	 * be _one_ more than the maximum commands handled by the firmware.
	 *
	 * Note: When FW completes commands, it places corresponding contex
	 * values in this circular reply queue. This circular queue is a fairly
	 * typical producer-consumer queue. FW is the producer (of completed
	 * commands) and the driver is the consumer.
	 */
	context_sz = sizeof(u32);
	reply_q_sz = context_sz * (instance->max_fw_cmds + 1);

	instance->reply_queue = pci_alloc_consistent(instance->pdev,
						     reply_q_sz,
						     &instance->reply_queue_h);

	if (!instance->reply_queue) {
		printk(KERN_DEBUG "megasas: Out of DMA mem for reply queue\n");
		goto fail_reply_queue;
	}

	if (megasas_issue_init_mfi(instance))
		goto fail_fw_init;

	instance->fw_support_ieee = 0; 
	instance->fw_support_ieee = (instance->instancet->read_fw_status_reg(reg_set) & 0x04000000); 

	printk("megasas_init: fw_support_ieee=%d", instance->fw_support_ieee);
	if (instance->fw_support_ieee)
		instance->flag_ieee = 1;

	return 0;

fail_fw_init:

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);
fail_reply_queue:
	megasas_free_cmds(instance);

fail_alloc_cmds:
	return 1;
}


/**
 * megasas_init_fw -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * This is the main function for initializing firmware.
 */
static int megasas_init_fw(struct megasas_instance *instance)
{
	u32 max_sectors_1;
	u32 max_sectors_2;
	u32 tmp_sectors, msix_enable, scratch_pad_2;
	struct megasas_register_set __iomem *reg_set;
	struct megasas_ctrl_info *ctrl_info;
	unsigned long bar_list;
	int i, loop, fw_msix_count = 0;

	/* Find first memory bar */
        bar_list = pci_select_bars(instance->pdev, IORESOURCE_MEM);
        instance->bar = find_first_bit(&bar_list, sizeof(unsigned long));
        instance->base_addr = pci_resource_start(instance->pdev, instance->bar);
        if (pci_request_selected_regions(instance->pdev, instance->bar, "megasas: LSI")) {
		printk(KERN_DEBUG "megasas: IO memory region busy!\n");
		return -EBUSY;
        }

	instance->reg_set = ioremap_nocache(instance->base_addr, 8192);

	if (!instance->reg_set) {
		printk(KERN_DEBUG "megasas: Failed to map IO mem\n");
		goto fail_ioremap;
	}

	reg_set = instance->reg_set;

	switch(instance->pdev->device)
	{
		case PCI_DEVICE_ID_LSI_FUSION:
		case PCI_DEVICE_ID_LSI_INVADER:
		case PCI_DEVICE_ID_LSI_FURY:
			instance->instancet = &megasas_instance_template_fusion;
			break;
		case PCI_DEVICE_ID_LSI_SAS1078R:	
		case PCI_DEVICE_ID_LSI_SAS1078DE:	
			instance->instancet = &megasas_instance_template_ppc;
#if defined(__VMKLNX__)
			printk("\n megasas_init_mfi:  Line %d: PPC Template. instance->pdev->device = 0x%X",
				__LINE__, (unsigned int)instance->pdev->device);
#endif
			break;
		case PCI_DEVICE_ID_LSI_SAS1078GEN2:
		case PCI_DEVICE_ID_LSI_SAS0079GEN2:
#if defined(__VMKLNX__)

			printk("\n megasas_init_mfi:  Line %d: gen2 Template. SAS 1078Gen2, 0079Gen2 instance->pdev->device = 0x%X",
				__LINE__, (unsigned int)instance->pdev->device);
#endif
			instance->instancet = &megasas_instance_template_gen2;
			break;
		case PCI_DEVICE_ID_LSI_SAS0073SKINNY:
		case PCI_DEVICE_ID_LSI_SAS0071SKINNY:
#if defined(__VMKLNX__)
			printk("\n megasas_init_mfi:  Line %d: skinny Template. instance->pdev->device = 0x%X",
				__LINE__, (unsigned int)instance->pdev->device);
#endif
			instance->instancet = &megasas_instance_template_skinny;
			break;
		case PCI_DEVICE_ID_LSI_SAS1064R:
		case PCI_DEVICE_ID_DELL_PERC5:
		default:
#if defined(__VMKLNX__)
			printk("\n megasas_init_mfi:  Line %d: DEFAULT, PERC5, SAS1064R get the XScale Template. "
		   " instance->pdev->device = 0x%X",
		      __LINE__,
				(unsigned int)instance->pdev->device);
#endif
			instance->instancet = &megasas_instance_template_xscale;
			break;
	}

	/*
	 * We expect the FW state to be READY
	 */
	if (megasas_transition_to_ready(instance, 0))
		goto fail_ready_state;

	/*
	* MSI-X host index 0 is common for all adapter.
	* It is used only for Fustion Adapters.
	*/
	instance->reply_post_host_index_addr[0] =
		(u32 *)((u8 *)instance->reg_set +
		MPI2_REPLY_POST_HOST_INDEX_OFFSET);

	/* Check if MSI-X is supported while in ready state */
	msix_enable = (instance->instancet->read_fw_status_reg(reg_set) & 0x4000000) >> 0x1a;
	if (msix_enable && !msix_disable) {
		scratch_pad_2 = readl
			(&instance->reg_set->outbound_scratch_pad_2);
		/* Check max MSI-X vectors */
		if (instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) {
			instance->msix_vectors = (scratch_pad_2
				& MR_MAX_REPLY_QUEUES_OFFSET) + 1;
			fw_msix_count = instance->msix_vectors;
		} else if ((instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER)
			|| (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY)) {
			/* Invader/Fury supports more than 8 MSI-X */
			instance->msix_vectors = ((scratch_pad_2
				& MR_MAX_REPLY_QUEUES_EXT_OFFSET)
				>> MR_MAX_REPLY_QUEUES_EXT_OFFSET_SHIFT) + 1;
			fw_msix_count = instance->msix_vectors;
		
			/* Save 1-15 reply post index address to local memory 
			 * Index 0 is already saved from reg offset 
		 	 * MPI2_REPLY_POST_HOST_INDEX_OFFSET
		 	 */
			for (loop = 1; loop < MR_MAX_MSIX_REG_ARRAY; loop++) {
				instance->reply_post_host_index_addr[loop] =
					(u32 *)((u8 *)instance->reg_set +
					MPI2_SUP_REPLY_POST_HOST_INDEX_OFFSET
					+ (loop * 0x10));
			}
		} else
			instance->msix_vectors = 1;
		/* Don't bother allocating more MSI-X vectors than cpus */
		instance->msix_vectors = min(instance->msix_vectors,
					     (unsigned int)num_online_cpus());
		for (i = 0; i < instance->msix_vectors; i++)
			instance->msixentry[i].entry = i;
		if ((i = pci_enable_msix(instance->pdev, instance->msixentry, instance->msix_vectors)) >= 0) {
			if (i) {
				if (!pci_enable_msix(instance->pdev, instance->msixentry, i))
					instance->msix_vectors = i;
				else
					instance->msix_vectors = 0;
			}
		} else
			instance->msix_vectors = 0;

		printk(KERN_INFO "[scsi%d]: FW supports <%d> MSIX vector, Online"
			"CPUs: <%d>, Current MSIX <%d>\n", instance->host->host_no,
			fw_msix_count, (unsigned int)num_online_cpus(),
			instance->msix_vectors);
	}

	/* Get operational params, sge flags, send init cmd to controller */
	if (instance->instancet->init_adapter(instance))
		goto fail_init_adapter;

	printk(KERN_ERR "megasas: INIT adapter done \n");

	/** for passthrough
	 * the following function will get the PD LIST.
	 */

	memset(instance->pd_list, 0, MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));
	megasas_get_pd_list(instance);

	memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
	megasas_get_ld_list(instance);

	ctrl_info = kmalloc(sizeof(struct megasas_ctrl_info), GFP_KERNEL);

	/*
	 * Compute the max allowed sectors per IO: The controller info has two
	 * limits on max sectors. Driver should use the minimum of these two.
	 *
	 * 1 << stripe_sz_ops.min = max sectors per strip
	 *
	 * Note that older firmwares ( < FW ver 30) didn't report information
	 * to calculate max_sectors_1. So the number ended up as zero always.
	 */
	tmp_sectors = 0;
	if (ctrl_info && !megasas_get_ctrl_info(instance, ctrl_info)) {

		max_sectors_1 = (1 << ctrl_info->stripe_sz_ops.min) *
		    ctrl_info->max_strips_per_io;
		max_sectors_2 = ctrl_info->max_request_size;

	        /*Check whether controller is iMR or MR */
	        if (ctrl_info->memory_size) {
	            instance->is_imr = 0;
	            printk("megaraid_sas: Controller type: MR, Memory size is: %dMB\n", ctrl_info->memory_size);
	        } else {
	            instance->is_imr = 1;
	            printk("megaraid_sas: Controller type: iMR\n");
	        }
	       
		tmp_sectors = (max_sectors_1 < max_sectors_2)
		    ? max_sectors_1 : max_sectors_2;
                instance->disableOnlineCtrlReset = ctrl_info->properties.OnOffProperties.disableOnlineCtrlReset;
		//printk(KERN_WARNING "instance->disableOnlineCtrlReset = %d\n", instance->disableOnlineCtrlReset);
		instance->UnevenSpanSupport = ctrl_info->adapterOperations2.supportUnevenSpans;
		if(instance->UnevenSpanSupport) {
			struct fusion_context *fusion = instance->ctrl_context;
			printk("megaraid_sas: FW supports: UnevenSpanSupport=%x\n", 
				instance->UnevenSpanSupport);
			if (MR_ValidateMapInfo(instance))
				fusion->fast_path_io = 1;
			else
				fusion->fast_path_io = 0;
		
		}
	}
 
	instance->max_sectors_per_req = instance->max_num_sge *
		    PAGE_SIZE / 512;
	printk(KERN_ERR "megasas: max_sectors_per_req = 0x%x\n", instance->max_sectors_per_req);
	if (tmp_sectors && (instance->max_sectors_per_req > tmp_sectors))
		instance->max_sectors_per_req = tmp_sectors;
	printk(KERN_ERR "megasas: tmp_sectors = 0x%x\n", tmp_sectors);
	kfree(ctrl_info);
#if defined(__VMKLNX__)
       init_waitqueue_head(&megasas_poll_wait);
#endif


        /*
	* Setup tasklet for cmd completion
	*/

	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
                        (unsigned long)instance);

	return 0;

fail_init_adapter:
fail_ready_state:
	iounmap(instance->reg_set);
fail_ioremap:
	pci_release_selected_regions(instance->pdev, instance->bar);

	return -EINVAL;
}

/**
 * megasas_release_mfi -	Reverses the FW initialization
 * @intance:			Adapter soft state
 */
static void megasas_release_mfi(struct megasas_instance *instance)
{
	u32 reply_q_sz = sizeof(u32) * (instance->max_mfi_cmds + 1);

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);

	megasas_free_cmds(instance);

	iounmap(instance->reg_set);

	pci_release_selected_regions(instance->pdev, instance->bar);
}

/**
 * megasas_get_seq_num -	Gets latest event sequence numbers
 * @instance:			Adapter soft state
 * @eli:			FW event log sequence numbers information
 *
 * FW maintains a log of all events in a non-volatile area. Upper layers would
 * usually find out the latest sequence number of the events, the seq number at
 * the boot etc. They would "read" all the events below the latest seq number
 * by issuing a direct fw cmd (DCMD). For the future events (beyond latest seq
 * number), they would subsribe to AEN (asynchronous event notification) and
 * wait for the events to happen.
 */
static int
megasas_get_seq_num(struct megasas_instance *instance,
		    struct megasas_evt_log_info *eli)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_evt_log_info *el_info;
	dma_addr_t el_info_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;
	el_info = pci_alloc_consistent(instance->pdev,
				       sizeof(struct megasas_evt_log_info),
				       &el_info_h);

	if (!el_info) {
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(el_info, 0, sizeof(*el_info));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_log_info);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = el_info_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_log_info);

	megasas_issue_blocked_cmd(instance, cmd);

	/*
	 * Copy the data back into callers buffer
	 */
	memcpy(eli, el_info, sizeof(struct megasas_evt_log_info));

	pci_free_consistent(instance->pdev, sizeof(struct megasas_evt_log_info),
			    el_info, el_info_h);

	megasas_return_cmd(instance, cmd);

	return 0;
}

/**
 * megasas_register_aen -	Registers for asynchronous event notification
 * @instance:			Adapter soft state
 * @seq_num:			The starting sequence number
 * @class_locale:		Class of the event
 *
 * This function subscribes for AEN for events beyond the @seq_num. It requests
 * to be notified if and only if the event is of type @class_locale
 */
static int
megasas_register_aen(struct megasas_instance *instance, u32 seq_num,
		     u32 class_locale_word)
{
	int ret_val;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	union megasas_evt_class_locale curr_aen;
	union megasas_evt_class_locale prev_aen;

	/*
	 * If there an AEN pending already (aen_cmd), check if the
	 * class_locale of that pending AEN is inclusive of the new
	 * AEN request we currently have. If it is, then we don't have
	 * to do anything. In other words, whichever events the current
	 * AEN request is subscribing to, have already been subscribed
	 * to.
	 *
	 * If the old_cmd is _not_ inclusive, then we have to abort
	 * that command, form a class_locale that is superset of both
	 * old and current and re-issue to the FW
	 */

	curr_aen.word = class_locale_word;

	if (instance->aen_cmd) {

		prev_aen.word = instance->aen_cmd->frame->dcmd.mbox.w[1];

		/*
		 * A class whose enum value is smaller is inclusive of all
		 * higher values. If a PROGRESS (= -1) was previously
		 * registered, then a new registration requests for higher
		 * classes need not be sent to FW. They are automatically
		 * included.
		 *
		 * Locale numbers don't have such hierarchy. They are bitmap
		 * values
		 */
		if ((prev_aen.members.class <= curr_aen.members.class) &&
		    !((prev_aen.members.locale & curr_aen.members.locale) ^
		      curr_aen.members.locale)) {
			/*
			 * Previously issued event registration includes
			 * current request. Nothing to do.
			 */
			printk(KERN_INFO "%s[%d]: already registered\n",
				__FUNCTION__, instance->host->host_no);
			return 0;
		} else {
			curr_aen.members.locale |= prev_aen.members.locale;

			if (prev_aen.members.class < curr_aen.members.class)
				curr_aen.members.class = prev_aen.members.class;

			instance->aen_cmd->abort_aen = 1;
			ret_val = megasas_issue_blocked_abort_cmd(instance,
								  instance->
								  aen_cmd);

			if (ret_val) {
				printk(KERN_DEBUG "megasas: Failed to abort "
				       "previous AEN command\n");
				return ret_val;
			}
		}
	}

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -ENOMEM;

	dcmd = &cmd->frame->dcmd;

	memset(instance->evt_detail, 0, sizeof(struct megasas_evt_detail));

	/*
	 * Prepare DCMD for aen registration
	 */
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_detail);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_WAIT;
	dcmd->mbox.w[0] = seq_num;
        instance->last_seq_num = seq_num;
	dcmd->mbox.w[1] = curr_aen.word;
	dcmd->sgl.sge32[0].phys_addr = (u32) instance->evt_detail_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_detail);
	
	if ( instance->aen_cmd != NULL ) {
		megasas_return_cmd(instance, cmd);
		return 0;
	}

	/*
	 * Store reference to the cmd used to register for AEN. When an
	 * application wants us to register for AEN, we have to abort this
	 * cmd and re-register with a new EVENT LOCALE supplied by that app
	 */
	instance->aen_cmd = cmd;

	/*
	 * Issue the aen registration frame
	 */
	instance->instancet->issue_dcmd(instance, cmd);

	return 0;
}

/**
 * megasas_start_aen -	Subscribes to AEN during driver load time
 * @instance:		Adapter soft state
 */
static int megasas_start_aen(struct megasas_instance *instance)
{
	struct megasas_evt_log_info eli;
	union megasas_evt_class_locale class_locale;

	/*
	 * Get the latest sequence number from FW
	 */
	memset(&eli, 0, sizeof(eli));

	if (megasas_get_seq_num(instance, &eli))
		return -1;

	/*
	 * Register AEN with FW for latest sequence number plus 1
	 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	return megasas_register_aen(instance, eli.newest_seq_num + 1,
				    class_locale.word);
}

static ssize_t 
sysfs_max_sectors_read(struct kobject *kobj, struct bin_attribute  *bin_attr,
			 char *buf, loff_t off, size_t count)
{
	struct Scsi_Host *host = class_to_shost(container_of(kobj,
					struct class_device, kobj));
	struct megasas_instance *instance = 
				(struct megasas_instance *)host->hostdata;

	count = sprintf(buf,"%u\n", instance->max_sectors_per_req);

	return count+1;
}

static struct bin_attribute sysfs_max_sectors_attr = {
	.attr = {
		.name = "max_sectors",
		.mode = S_IRUSR|S_IRGRP|S_IROTH,
		.owner = THIS_MODULE,
	},
	.size = 7,
	.read = sysfs_max_sectors_read,
};

/**
 * megasas_io_attach -	Attaches this driver to SCSI mid-layer
 * @instance:		Adapter soft state
 */
static int megasas_io_attach(struct megasas_instance *instance)
{
	struct Scsi_Host *host = instance->host;
	int error;

	/*
	 * Export parameters required by SCSI mid-layer
	 */
	host->irq = instance->pdev->irq;
	host->unique_id = instance->unique_id;
	if (instance->is_imr) {
		host->can_queue = instance->max_fw_cmds - MEGASAS_SKINNY_INT_CMDS;
	} else 
		host->can_queue = instance->max_fw_cmds - MEGASAS_INT_CMDS;
	host->this_id = instance->init_id;
	host->sg_tablesize = instance->max_num_sge;

#if defined(__VMKLNX__)
	printk(KERN_INFO "megasas: io_attach:  host->irq: %d  host->unique_id: %d  host->can_queue: %d.\n",
		host->irq, host->unique_id, host->can_queue);
#endif
	if (instance->fw_support_ieee)
		instance->max_sectors_per_req = MEGASAS_MAX_SECTORS_IEEE;

	/*
	 * Check if the module parameter value for max_sectors can be used
	 */
	if (max_sectors && max_sectors < instance->max_sectors_per_req)
		instance->max_sectors_per_req = max_sectors;
	else {
		if (max_sectors)
			printk(KERN_INFO "megasas: max_sectors should be > 0 and"
			 	"<= %d\n",instance->max_sectors_per_req);
	}

	host->max_sectors = instance->max_sectors_per_req;

#if defined(__VMKLNX__)
	/* Setting to current max to accomodate storage stack timeouts under heavy stress.
	 * Check if the module parameter value for cmd_per_lun can be used
	 */
	instance->cmd_per_lun = MAX_CMD_PER_LUN;
	if (cmd_per_lun && cmd_per_lun <= MAX_CMD_PER_LUN)
		instance->cmd_per_lun = cmd_per_lun;
	else
		printk(KERN_INFO "megasas: cmd_per_lun should be > 0 and"
			 	"<= %d\n",MAX_CMD_PER_LUN);

#else
	/*
	 * Check if the module parameter value for cmd_per_lun can be used
	 */
	instance->cmd_per_lun = MEGASAS_DEFAULT_CMD_PER_LUN;
	if (cmd_per_lun && cmd_per_lun <= MEGASAS_DEFAULT_CMD_PER_LUN)
		instance->cmd_per_lun = cmd_per_lun;
	else
		printk(KERN_INFO "megasas: cmd_per_lun should be > 0 and"
			 	"<= %d\n",MEGASAS_DEFAULT_CMD_PER_LUN);
#endif
		
	host->cmd_per_lun = instance->cmd_per_lun;

	printk(KERN_DEBUG "megasas: max_sectors : 0x%x, cmd_per_lun : 0x%x\n",
			instance->max_sectors_per_req, instance->cmd_per_lun);

	host->max_channel = MEGASAS_MAX_CHANNELS - 1;
	host->max_id = MEGASAS_MAX_DEV_PER_CHANNEL;
	host->max_lun = MEGASAS_MAX_LUN;
	host->max_cmd_len = 16;

	/*
	 * Notify the mid-layer about the new controller
	 */
	if (scsi_add_host(host, &instance->pdev->dev)) {
		printk(KERN_DEBUG "megasas: scsi_add_host failed\n");
		return -ENODEV;
	}

#if defined(__VMKLNX__)
	vmklnx_scsi_register_poll_handler( host,
		instance->msix_vectors ? instance->msixentry[0].vector : instance->pdev->irq,
		(irq_handler_t)instance->instancet->coredump_poll_handler, &instance->irq_context[0]);
#endif /* defined(__VMKLNX__) */
	/*
	 * Create sysfs entries for module paramaters
	 */
	error = sysfs_create_bin_file(&instance->host->shost_classdev.kobj,
			&sysfs_max_sectors_attr);
	if (error) {
		printk(KERN_INFO "megasas: Error in creating the sysfs entry"
				" max_sectors.\n");
		goto out_remove_host;
	}

	/*
	 * Trigger SCSI to scan our drives
	 */
	scsi_scan_host(host);
	return 0;

out_remove_host:
	scsi_remove_host(host);
	return error;
}

static int
megasas_set_dma_mask(struct pci_dev *pdev)
{
	/*
	 * All our contollers are capable of performing 64-bit DMA
	 */
	if (IS_DMA64) {
		if (pci_set_dma_mask(pdev, DMA_64BIT_MASK) != 0) {

			if (pci_set_dma_mask(pdev, DMA_32BIT_MASK) != 0)
				goto fail_set_dma_mask;
		}
	} else {
		if (pci_set_dma_mask(pdev, DMA_32BIT_MASK) != 0)
			goto fail_set_dma_mask;
	}
	return 0;

fail_set_dma_mask:
	return 1;
}

#if defined(__VMKLNX__)
/**
 * megasas_check_fw - Check for firmware known issues
 * @instance:   Adapter soft state
 * 
 * Check firmware version for the hardware and warn if current firmware version is known to be buggy. See PR 713251
 */
static void
megasas_check_fw(struct megasas_instance *instance)
{
        struct megasas_ctrl_info ctrl_info;

        if (megasas_get_ctrl_info(instance, &ctrl_info)) {
                printk(KERN_ERR "megasas: unable to get controller info.\n");
                return;
        }

        printk("megasas: controller %p has firmware version %s.\n",
                instance,
                ctrl_info.package_version);

        switch(instance->pdev->device) {
        case PCI_DEVICE_ID_LSI_SAS0079GEN2:
                /* 
 		 * This versions of firmware for Dell PERC H700 controller have problems dealing with IEEE format scatter-gather lists and can generate PCIE fatal error. 
		 * See VMware PR 604455 and Dell support page (http://support.dell.com)
		 */
                if (!strncmp(ctrl_info.package_version, "12.3.0-0032", 0x60) ||
                    !strncmp(ctrl_info.package_version, "12.0.1-0091", 0x60) ||
                    !strncmp(ctrl_info.package_version, "12.0.1-0083", 0x60)) {
                        printk(VMKLNX_KERN_ALERT "Found MegaRAID SAS controller on PCI bus %d:%d:%d with firmware version %s. "
                                "This firmware is known to have issues that could cause the system to hang or crash.\n",
                                instance->pdev->bus->number,
                                PCI_SLOT(instance->pdev->devfn),
                                PCI_FUNC(instance->pdev->devfn),
                                ctrl_info.package_version);
                        printk(VMKLNX_KERN_ALERT "To protect your data, please contact your hardware vendor to upgrade the firmware.\n");
                }
                break;
        default:
                /* no known issues with this device and firmware. */
                break;
   }
}
#endif

/**
 * megasas_probe_one -	PCI hotplug entry point
 * @pdev:		PCI device structure
 * @id:			PCI ids of supported hotplugged adapter	
 */
static int __devinit
megasas_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rval, i, j;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	/*
	 * Announce PCI information
	 */
	printk(KERN_INFO "megasas: %#4.04x:%#4.04x:%#4.04x:%#4.04x: ",
	       pdev->vendor, pdev->device, pdev->subsystem_vendor,
	       pdev->subsystem_device);

#if !defined(__VMKLNX__)
	printk("bus %d:slot %d:func %d\n",
	       pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
#else /* defined(__VMKLNX__) */
	printk("domain %d bus %d:slot %d:func %d\n",
	       pci_domain_nr(pdev->bus), pdev->bus->number,
	       PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
#endif /*  defined(__VMKLNX__) */

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device(pdev);

	if (rval) {
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	host = scsi_host_alloc(&megasas_template,
			       sizeof(struct megasas_instance));

	if (!host) {
		printk(KERN_DEBUG "megasas: scsi_host_alloc failed\n");
		goto fail_alloc_instance;
	}

	instance = (struct megasas_instance *)host->hostdata;
	memset(instance, 0, sizeof(*instance));

	atomic_set( &instance->fw_reset_no_pci_access, 0 );

	/*
	 * Initialize PCI related and misc parameters
	 */
	instance->pdev = pdev;
	instance->host = host;
	instance->unique_id = pdev->bus->number << 8 | pdev->devfn;
	instance->init_id = MEGASAS_DEFAULT_INIT_ID;

	switch(instance->pdev->device)
	{
		case PCI_DEVICE_ID_LSI_FUSION:
		case PCI_DEVICE_ID_LSI_INVADER:
		case PCI_DEVICE_ID_LSI_FURY:
		{	
			struct fusion_context *fusion;

	 		instance->ctrl_context = kzalloc(sizeof(struct fusion_context), GFP_KERNEL);
			if (!instance->ctrl_context) {
				printk(KERN_DEBUG "megasas: Failed to allocate memory for "
				"Fusion context info\n");
				goto fail_alloc_dma_buf;
			}
			fusion = instance->ctrl_context;
			INIT_LIST_HEAD(&fusion->cmd_pool);
			spin_lock_init(&fusion->cmd_pool_lock);
		}
			break;
		default: /* For all other supported controllers */

			instance->producer = pci_alloc_consistent(pdev, sizeof(u32),
									&instance->producer_h);
			instance->consumer = pci_alloc_consistent(pdev, sizeof(u32),
									&instance->consumer_h);

			if (!instance->producer || !instance->consumer) {
				printk(KERN_DEBUG "megasas: Failed to allocate memory for "
				"producer, consumer\n");
				goto fail_alloc_dma_buf;
			}

			*instance->producer = 0;
			*instance->consumer = 0;
			break;
	}

        instance->verbuf = pci_alloc_consistent(pdev, 
						MEGASAS_MAX_NAME*sizeof(u32),
                                                &instance->verbuf_h);
	if (!instance->verbuf) {
		printk(KERN_DEBUG "megasas: Can't allocate version buffer\n");
	}

	instance->flag_ieee = 0;
 	instance->issuepend_done = 1;
	instance->adprecovery = MEGASAS_HBA_OPERATIONAL;
	megasas_poll_wait_aen = 0;

	instance->evt_detail = pci_alloc_consistent(pdev,
						    sizeof(struct
							   megasas_evt_detail),
						    &instance->evt_detail_h);

	if (!instance->evt_detail) {
		printk(KERN_DEBUG "megasas: Failed to allocate memory for "
		       "event detail structure\n");
		goto fail_alloc_dma_buf;
	}

	/*
	 * Initialize locks and queues
	 */
	INIT_LIST_HEAD(&instance->cmd_pool);
	INIT_LIST_HEAD(&instance->internal_reset_pending_q);

	atomic_set(&instance->fw_outstanding,0);

	init_waitqueue_head(&instance->int_cmd_wait_q);
	init_waitqueue_head(&instance->abort_cmd_wait_q);

	spin_lock_init(&instance->cmd_pool_lock);
	spin_lock_init(&instance->hba_lock);
	spin_lock_init(&instance->completion_lock);
	spin_lock_init(&poll_aen_lock);

	sema_init(&instance->aen_mutex, 1);
	mutex_init(&instance->reset_mutex);

	/*
	 * Initialize PCI related and misc parameters
	 */
	instance->pdev = pdev;
	instance->host = host;
	instance->unique_id = pdev->bus->number << 8 | pdev->devfn;
	instance->init_id = MEGASAS_DEFAULT_INIT_ID;

	megasas_dbg_lvl = 0;
	instance->is_busy=0;
#if defined(__VMKLNX__)
	TRACK_IS_BUSY("megasas_probe_one",instance->is_busy)
	instance->in_taskmgmt = 0;
#endif
	instance->unload=1;
	instance->last_time=0;
	instance->disableOnlineCtrlReset = 1;
	instance->UnevenSpanSupport = 0;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY))
		INIT_WORK(&instance->work_init, megasas_fusion_ocr_wq);
	else
		INIT_WORK(&instance->work_init, process_fw_state_change_wq);

	/*
	 * Initialize Firmware
	 */
	if (megasas_init_fw(instance))
		goto fail_init_mfi;

#if defined(__VMKLNX__)
        /*
	 * Check for known firwware issues
	 */
        megasas_check_fw(instance);
#endif


	if (instance->is_imr) {
		instance->flag_ieee = 1;
		sema_init(&instance->ioctl_sem, MEGASAS_SKINNY_INT_CMDS);
	
	} else 
		sema_init(&instance->ioctl_sem, MEGASAS_INT_CMDS);

	/*
   	 * Register IRQ
	 */
retry_irq_register:
	if (instance->msix_vectors) {
		for (i = 0 ; i < instance->msix_vectors; i++) {
			instance->irq_context[i].instance = instance;
			instance->irq_context[i].MSIxIndex = i;
			if (request_irq(instance->msixentry[i].vector, (irq_handler_t)instance->instancet->service_isr, 0, "megasas", &instance->irq_context[i])) {
				printk(KERN_DEBUG "megasas: Failed to register IRQ for vector %d.\n", i);
				for (j = 0 ; j < i ; j++)
					free_irq(instance->msixentry[j].vector, &instance->irq_context[j]);
				/* Retry irq register for IO_APIC*/
				instance->msix_vectors = 0;
				goto retry_irq_register;
			}
		}
	} else {
		instance->irq_context[0].instance = instance;
		instance->irq_context[0].MSIxIndex = 0;
		if (request_irq(pdev->irq, (irq_handler_t)instance->instancet->service_isr, IRQF_SHARED, "megasas", &instance->irq_context[0])) {
			printk(KERN_DEBUG "megasas: Failed to register IRQ\n");
			goto fail_irq;
		}
	}

	instance->instancet->enable_intr(instance);

	/*
	 * Store instance in PCI softstate
	 */
	pci_set_drvdata(pdev, instance);

#if defined(__VMKLNX__)
	instance->ctrlId = megasas_mgmt_info.max_index;
#endif /* defined(__VMKLNX__) */

	/*
	 * Add this controller to megasas_mgmt_info structure so that it
	 * can be exported to management applications
	 */
	megasas_mgmt_info.count++;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = instance;
	megasas_mgmt_info.max_index++;

	/*
	 * Register with SCSI mid-layer
	 */
	if (megasas_io_attach(instance))
		goto fail_io_attach;

	/* Now Driver loaded success */	
	instance->unload = 0 ;

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance)) {
		printk(KERN_DEBUG "megasas: start aen failed\n");
		goto fail_start_aen;
	}

	return 0;

      fail_start_aen:
      fail_io_attach:
	megasas_mgmt_info.count--;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = NULL;
	megasas_mgmt_info.max_index--;

	pci_set_drvdata(pdev, NULL);
	instance->instancet->disable_intr(instance);
	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	fail_irq:
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_FUSION) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_INVADER) ||
	    (instance->pdev->device == PCI_DEVICE_ID_LSI_FURY))
		megasas_release_fusion(instance);
	else
		megasas_release_mfi(instance);
	fail_init_mfi:
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
	fail_alloc_dma_buf:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				    instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				    instance->consumer_h);
	scsi_host_put(host);

	fail_alloc_instance:
	fail_set_dma_mask:
	pci_disable_device(pdev);

	return -ENODEV;
}

/**
 * megasas_flush_cache -	Requests FW to flush all its caches
 * @instance:			Adapter soft state
 */
static void megasas_flush_cache(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = MR_DCMD_CTRL_CACHE_FLUSH;
	dcmd->mbox.b[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

/**
 * megasas_shutdown_controller -	Instructs FW to shutdown the controller
 * @instance:				Adapter soft state
 * @opcode:				Shutdown/Hibernate
 */
static void megasas_shutdown_controller(struct megasas_instance *instance,
					u32 opcode)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	if (instance->aen_cmd)
		megasas_issue_blocked_abort_cmd(instance, instance->aen_cmd);
        if (instance->map_update_cmd)
                megasas_issue_blocked_abort_cmd(instance, instance->map_update_cmd);
	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = opcode;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

/**
 * megasas_suspend -	driver suspend entry point
 * @pdev:		PCI device structure
 * @state:				
 */
static int __devinit
megasas_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	int i;

	instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	host = instance->host;

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_HIBERNATE_SHUTDOWN);
	tasklet_kill(&instance->isr_tasklet);

	pci_set_drvdata(instance->pdev, instance); 
	instance->instancet->disable_intr(instance);

	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);

	scsi_host_put(host);

	pci_save_state(pdev);
	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

/**
 * megasas_resume-	driver resume entry point
 * @pdev:		PCI device structure
 */
static int __devinit
megasas_resume(struct pci_dev *pdev)
{
	int rval, i, j;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	
	instance = pci_get_drvdata(pdev);
	host = instance->host;
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device(pdev);

	if (rval) {
		printk(KERN_INFO "megasas: Enable device failed\n");
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	/*
	 * We expect the FW state to be READY
	 */
	if (megasas_transition_to_ready(instance, 0))
		goto fail_ready_state;

	/* Now re-enable MSI-X */
	if (instance->msix_vectors)
		pci_enable_msix(instance->pdev, instance->msixentry, instance->msix_vectors);

	/*
	 * Initialize Firmware
	 */
	atomic_set(&instance->fw_outstanding,0);
	switch(instance->pdev->device)
	{
		case PCI_DEVICE_ID_LSI_FUSION:
		case PCI_DEVICE_ID_LSI_INVADER:
		case PCI_DEVICE_ID_LSI_FURY:
		{
			megasas_reset_reply_desc(instance);
			if (megasas_ioc_init_fusion(instance)) {
				megasas_free_cmds(instance);
				megasas_free_cmds_fusion(instance);
				goto fail_init_mfi;
			}
                        if (!megasas_get_map_info(instance))
                                megasas_sync_map_info(instance);
		}
		break;
		default:
			*instance->producer = 0;
			*instance->consumer = 0;
			if (megasas_issue_init_mfi(instance))
				goto fail_init_mfi;
			break;
	}

	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
                        (unsigned long)instance);	

	/*
	 * Register IRQ
	 */
	if (instance->msix_vectors) {
		for (i = 0 ; i < instance->msix_vectors; i++) {
			instance->irq_context[i].instance = instance;
			instance->irq_context[i].MSIxIndex = i;
			if (request_irq(instance->msixentry[i].vector, (irq_handler_t)instance->instancet->service_isr, 0, "megasas", &instance->irq_context[i])) {
				printk(KERN_DEBUG "megasas: Failed to register IRQ for vector %d.\n", i);
				for (j = 0 ; j < i ; j++)
					free_irq(instance->msixentry[j].vector, &instance->irq_context[j]);
				goto fail_irq;
			}
		}
	} else {
		instance->irq_context[0].instance = instance;
		instance->irq_context[0].MSIxIndex = 0;
		if (request_irq(pdev->irq, (irq_handler_t)instance->instancet->service_isr, IRQF_SHARED, "megasas", &instance->irq_context[0])) {
			printk(KERN_DEBUG "megasas: Failed to register IRQ\n");
			goto fail_irq;
		}
	}

	instance->instancet->enable_intr(instance);

	/*
	 * Store instance in PCI softstate
	 */
	pci_set_drvdata(pdev, instance);

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance))
		printk(KERN_ERR "megasas: Start AEN failed\n");

	instance->unload = 0;
	
	return 0;	
	
      fail_irq:
      fail_init_mfi:

	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				    instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				    instance->consumer_h);
	scsi_host_put(host);
      fail_set_dma_mask:
      fail_ready_state:
	pci_disable_device(pdev);

	return -ENODEV;
}

/**
 * megasas_detach_one -	PCI hot"un"plug entry point
 * @pdev:		PCI device structure
 */
static void megasas_detach_one(struct pci_dev *pdev)
{
	int i;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	struct fusion_context *fusion;

	instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	host = instance->host;

	fusion = instance->ctrl_context;

	sysfs_remove_bin_file(&host->shost_classdev.kobj, &sysfs_max_sectors_attr);
	scsi_remove_host(instance->host);
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);
	tasklet_kill(&instance->isr_tasklet);

	/*
	 * Take the instance off the instance array. Note that we will not
	 * decrement the max_index. We let this array be sparse array
	 */
	for (i = 0; i < megasas_mgmt_info.max_index; i++) {
		if (megasas_mgmt_info.instance[i] == instance) {
			megasas_mgmt_info.count--;
			megasas_mgmt_info.instance[i] = NULL;

			break;
		}
	}

	pci_set_drvdata(instance->pdev, NULL);

	instance->instancet->disable_intr(instance);

	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);

	switch(instance->pdev->device)
	{
		case PCI_DEVICE_ID_LSI_FUSION:
		case PCI_DEVICE_ID_LSI_INVADER:
		case PCI_DEVICE_ID_LSI_FURY:
			megasas_release_fusion(instance);
                        for (i = 0; i < 2 ; i++)
                                if (fusion->ld_map[i])
                                        dma_free_coherent(&instance->pdev->dev, fusion->map_sz,
							  fusion->ld_map[i], fusion->ld_map_phys[i]);
			kfree(instance->ctrl_context);
		break;
		default:
			megasas_release_mfi(instance);
			pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
						instance->evt_detail, instance->evt_detail_h);
			pci_free_consistent(pdev, sizeof(u32), instance->producer,
						instance->producer_h);

			pci_free_consistent(pdev, sizeof(u32), instance->consumer,
						instance->consumer_h);
			break;
	}

#if !defined(__VMKLNX__)
	destroy_waitqueue_head(&instance->int_cmd_wait_q);
	destroy_waitqueue_head(&instance->abort_cmd_wait_q);

	spin_lock_destroy(&instance->cmd_pool_lock);
#endif /* defined(__VMKLNX__) */

	if (instance->verbuf) {
		pci_free_consistent(pdev, MEGASAS_MAX_NAME*sizeof(u32), 
				    instance->verbuf, instance->verbuf_h);
	}

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);

	pci_disable_device(pdev);

	return;
}

/**
 * megasas_shutdown -	Shutdown entry point
 * @device:		Generic device structure
 */
static void megasas_shutdown(struct pci_dev *pdev)
{
	int i;
	struct megasas_instance *instance = pci_get_drvdata(pdev);

	instance->unload = 1;
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);
	instance->instancet->disable_intr(instance);
	if (instance->msix_vectors)
		for (i = 0 ; i < instance->msix_vectors; i++)
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
}

/**
 * megasas_mgmt_open -	char node "open" entry point
 */
static int megasas_mgmt_open(struct inode *inode, struct file *filep)
{
	/*
	 * Allow only those users with admin rights
	 */
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	return 0;
}

/**
 * megasas_mgmt_release - char node "release" entry point
 */
static int megasas_mgmt_release(struct inode *inode, struct file *filep)
{
	filep->private_data = NULL;
	fasync_helper(-1, filep, 0, &megasas_async_queue);

	return 0;
}

/**
 * megasas_mgmt_fasync -	Async notifier registration from applications
 *
 * This function adds the calling process to a driver global queue. When an
 * event occurs, SIGIO will be sent to all processes in this queue.
 */
static int megasas_mgmt_fasync(int fd, struct file *filep, int mode)
{
	int rc;

	mutex_lock(&megasas_async_queue_mutex);

	rc = fasync_helper(fd, filep, mode, &megasas_async_queue);

	mutex_unlock(&megasas_async_queue_mutex);

	if (rc >= 0) {
		/* For sanity check when we get ioctl */
		filep->private_data = filep;
		return 0;
	}

	printk(KERN_DEBUG "megasas: fasync_helper failed [%d]\n", rc);

	return rc;
}

 /**
 * megasas_mgmt_poll -  char node "poll" entry point
 */
static unsigned int megasas_mgmt_poll(struct file *file, poll_table *wait) 
{
 	unsigned int mask;
	unsigned long flags;
 
   	poll_wait(file, &megasas_poll_wait, wait);
	spin_lock_irqsave(&poll_aen_lock, flags);
/*	if (megasas_poll_wait_aen)
      		mask =   (POLLIN | POLLRDNORM);
	else*/
	if (megasas_poll_wait_aen) {
                mask = (POLLIN | POLLRDNORM);
        }
        else {
		mask = 0;
	}
	megasas_poll_wait_aen = 0;

	spin_unlock_irqrestore(&poll_aen_lock, flags);

   	return mask;
}

/**
 * megasas_mgmt_fw_ioctl -	Issues management ioctls to FW
 * @instance:			Adapter soft state
 * @argp:			User's ioctl packet
 */
static int
megasas_mgmt_fw_ioctl(struct megasas_instance *instance,
#if !defined(__VMKLNX__)
		      struct megasas_iocpacket __user * user_ioc,
#endif /* !defined(__VMKLNX__) */
		      struct megasas_iocpacket *ioc)
{
	struct megasas_sge32 *kern_sge32;
	struct megasas_cmd *cmd;
	void *kbuff_arr[MAX_IOCTL_SGE];
	dma_addr_t buf_handle = 0;
	int error = 0, i;
	void *sense = NULL;
	dma_addr_t sense_handle;
	unsigned long *sense_ptr;

	memset(kbuff_arr, 0, sizeof(kbuff_arr));

	if (ioc->sge_count > MAX_IOCTL_SGE) {
		printk(KERN_DEBUG "megasas: SGE count [%d] >  max limit [%d]\n",
		       ioc->sge_count, MAX_IOCTL_SGE);
		return -EINVAL;
	}

	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a cmd packet\n");
		return -ENOMEM;
	}

	/*
	 * User's IOCTL packet has 2 frames (maximum). Copy those two
	 * frames into our cmd's frames. cmd->frame's context will get
	 * overwritten when we copy from user's frames. So set that value
	 * alone separately
	 */
	memcpy(cmd->frame, ioc->frame.raw, 2 * MEGAMFI_FRAME_SIZE);
	cmd->frame->hdr.context = cmd->index;
	cmd->frame->hdr.pad_0 = 0;

	/*
	 * The management interface between applications and the fw uses
	 * MFI frames. E.g, RAID configuration changes, LD property changes
	 * etc are accomplishes through different kinds of MFI frames. The
	 * driver needs to care only about substituting user buffers with
	 * kernel buffers in SGLs. The location of SGL is embedded in the
	 * struct iocpacket itself.
	 */
	kern_sge32 = (struct megasas_sge32 *)
	    ((unsigned long)cmd->frame + ioc->sgl_off);

	/*
	 * For each user buffer, create a mirror buffer and copy in
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		if (!ioc->sgl[i].iov_len)
			continue;

		kbuff_arr[i] = dma_alloc_coherent(&instance->pdev->dev,
						    ioc->sgl[i].iov_len,
						    &buf_handle, GFP_KERNEL);
		if (!kbuff_arr[i]) {
			printk(KERN_DEBUG "megasas: Failed to alloc "
			       "kernel SGL buffer for IOCTL \n");
			error = -ENOMEM;
			goto out;
		}

		/*
		 * We don't change the dma_coherent_mask, so
		 * pci_alloc_consistent only returns 32bit addresses
		 */
		kern_sge32[i].phys_addr = (u32) buf_handle;
		kern_sge32[i].length = ioc->sgl[i].iov_len;

		/*
		 * We created a kernel buffer corresponding to the
		 * user buffer. Now copy in from the user buffer
		 */
		if (copy_from_user(kbuff_arr[i], ioc->sgl[i].iov_base,
				   (u32) (ioc->sgl[i].iov_len))) {
			error = -EFAULT;
			goto out;
		}
	}

	if (ioc->sense_len) {
		sense = dma_alloc_coherent(&instance->pdev->dev, ioc->sense_len,
					     &sense_handle, GFP_KERNEL);
		if (!sense) {
			error = -ENOMEM;
			goto out;
		}

		sense_ptr =
                   (unsigned long *) ((unsigned long)cmd->frame + ioc->sense_off);
		*sense_ptr = sense_handle;
	}

	/*
	 * Set the sync_cmd flag so that the ISR knows not to complete this
	 * cmd to the SCSI mid-layer
	 */
	cmd->sync_cmd = 1;
	megasas_issue_blocked_cmd(instance, cmd);
	cmd->sync_cmd = 0;

	if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_IO_METRICS_GET) {
		ProcessPerfMetricRequest(instance , kbuff_arr[0], ioc->sgl[0].iov_len);
	}

	/*
	 * copy out the kernel buffers to user buffers
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		if (copy_to_user(ioc->sgl[i].iov_base, kbuff_arr[i],
				 ioc->sgl[i].iov_len)) {
			error = -EFAULT;
			goto out;
		}
	}

	/*
	 * copy out the sense
	 */
	if (ioc->sense_len) {
		/*
		 * sense_buff points to the location that has the user
		 * sense buffer address
		 */

               sense_ptr = (unsigned long *) ((unsigned long)ioc->frame.raw +
					ioc->sense_off);
		if (copy_to_user((void __user *)((unsigned long)(*sense_ptr)),
                                 sense, ioc->sense_len)) {
                        printk(KERN_ERR "megasas: Failed to copy out to user"
                               "sense data\n");
                        error = -EFAULT;
                        goto out;
                }
	}

	/*
	 * copy the status codes returned by the fw
	 */
#if !defined(__VMKLNX__)
	if (copy_to_user(&user_ioc->frame.hdr.cmd_status,
			 &cmd->frame->hdr.cmd_status, sizeof(u8)))
	{
		printk(KERN_DEBUG "megasas: Error copying out cmd_status\n");
		error = -EFAULT;
	}
#else /* defined(__VMKLNX__) */
	memcpy(&ioc->frame.hdr.cmd_status,
			 &cmd->frame->hdr.cmd_status, sizeof(u8));
#endif /* !defined(__VMKLNX__) */

      out:
	if (sense) {
		dma_free_coherent(&instance->pdev->dev, ioc->sense_len,
				    sense, sense_handle);
	}

	for (i = 0; i < ioc->sge_count && kbuff_arr[i]; i++) {
		dma_free_coherent(&instance->pdev->dev,
				    kern_sge32[i].length,
				    kbuff_arr[i], kern_sge32[i].phys_addr);
	}

	megasas_return_cmd(instance, cmd);
	return error;
}

static int megasas_mgmt_ioctl_fw(struct file *file, unsigned long arg)
{
#if !defined(__VMKLNX__)
	struct megasas_iocpacket __user *user_ioc =
	    (struct megasas_iocpacket __user *)arg;
	struct megasas_iocpacket *ioc;
#else /* defined(__VMKLNX__) */
	struct megasas_iocpacket *ioc = (struct megasas_iocpacket *)arg;
#endif /* !defined(__VMKLNX__) */
	struct megasas_instance *instance;
	int error;
	int i;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;
	unsigned long flags;

#if !defined(__VMKLNX__)
	ioc = kmalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return -ENOMEM;

	if (copy_from_user(ioc, user_ioc, sizeof(*ioc))) {
		error = -EFAULT;
		goto out_kfree_ioc;
	}
#endif /* !defined(__VMKLNX__) */

	instance = megasas_lookup_instance(ioc->host_no);
	if (!instance) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR) {
		printk("Controller in crit error\n");
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->unload == 1) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	/*
	 * We will allow only MEGASAS_INT_CMDS number of parallel ioctl cmds
	 */
	if (down_interruptible(&instance->ioctl_sem)) {
		error = -ERESTARTSYS;
		goto out_kfree_ioc;
	}

	// If HBA is undergoing a reset recovery, wait for that to complete
	// before issuing this command
	for (i = 0; i < wait_time; i++) {

		spin_lock_irqsave(&instance->hba_lock, flags);
		if (instance->adprecovery == MEGASAS_HBA_OPERATIONAL) {
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: %s waiting for controller reset to finish\n", __FUNCTION__);
		}

		msleep(1000);
	}

	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		printk("megaraid_sas: %s timed out while waiting for HBA to recover.\n", __FUNCTION__);
		error = -ENODEV;
		goto out_up;
	}
	spin_unlock_irqrestore(&instance->hba_lock, flags);

#if !defined(__VMKLNX__)
	error = megasas_mgmt_fw_ioctl(instance, user_ioc, ioc);
#else /* defined(__VMKLNX__) */
	error = megasas_mgmt_fw_ioctl(instance, ioc);
#endif /* !defined(__VMKLNX__) */
out_up:
	up(&instance->ioctl_sem);

out_kfree_ioc:
#if !defined(__VMKLNX__)
	kfree(ioc);
#endif /* !defined(__VMKLNX__) */

	return error;
}

static int megasas_mgmt_ioctl_aen(struct file *file, unsigned long arg)
{
	struct megasas_instance *instance;
	struct megasas_aen aen;
	int error;
	int i;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;
	unsigned long flags;

	if (file->private_data != file) {
		if (!dbg_print_cnt) 
			printk(KERN_DEBUG "megasas: fasync_helper was not "
			       "called first\n");
		dbg_print_cnt = 1;

		return -EINVAL;
	}

	if (copy_from_user(&aen, (void __user *)arg, sizeof(aen)))
		return -EFAULT;

	instance = megasas_lookup_instance(aen.host_no);

	if (!instance)
		return -ENODEV;
#if defined(__VMKLNX__)
 	spin_lock_irqsave(&instance->hba_lock, flags);
#endif
	if (instance->adprecovery == MEGASAS_HW_CRITICAL_ERROR) {
#if defined(__VMKLNX__)
		spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif
		return -ENODEV;
       }
#if defined(__VMKLNX__)
  spin_unlock_irqrestore(&instance->hba_lock, flags);
#endif

   if (instance->unload == 1) {
           return -ENODEV;
   }


	down(&instance->aen_mutex);
	for (i = 0; i < wait_time; i++) {

		spin_lock_irqsave(&instance->hba_lock, flags);
		if (instance->adprecovery == MEGASAS_HBA_OPERATIONAL) {
			spin_unlock_irqrestore(&instance->hba_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: waiting for controller reset to finish\n");
		}

		msleep(1000);
	}

	spin_lock_irqsave(&instance->hba_lock, flags);
	if (instance->adprecovery != MEGASAS_HBA_OPERATIONAL) {
		spin_unlock_irqrestore(&instance->hba_lock, flags);
		printk("megaraid_sas: %s timed out while waiting for HBA to recover.\n", __FUNCTION__);
		up(&instance->aen_mutex);
		return -ENODEV;
	}
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	error = megasas_register_aen(instance, aen.seq_num,
				     aen.class_locale_word);
	up(&instance->aen_mutex);
	return error;
}

#if defined(__VMKLNX__)
static int megasas_mgmt_ioctl_host_info(struct file *file, unsigned long arg)
{
        struct megasas_instance *instance;
        struct megasas_host_info hostinfo;

        if (copy_from_user(&hostinfo, (void __user *)arg, sizeof(hostinfo)))
                return -EFAULT;

        instance = megasas_lookup_instance_on_ctrlId(hostinfo.ctrlId);

        if (!instance)
                return -ENODEV;

        hostinfo.pci_bus  = instance->pdev->bus->number;
        hostinfo.pci_dev  = PCI_SLOT(instance->pdev->devfn);
        hostinfo.pci_func = PCI_FUNC(instance->pdev->devfn);
        hostinfo.host_no  = instance->host->host_no;

        if (copy_to_user((void __user *)arg, &hostinfo, sizeof(hostinfo))) {
                printk(KERN_DEBUG "megasas: Error copying out host info\n");
                return -EFAULT;
        }

        return 0;
}

static int megasas_mgmt_ioctl_drv_info(struct file *file, unsigned long arg)
{
        struct megasas_drv_info drv_info;

        snprintf(drv_info.rel_date, sizeof(drv_info.rel_date), "%s", MEGASAS_RELDATE);
        snprintf(drv_info.version, sizeof(drv_info.version), "%s", MEGASAS_VERSION);

        if (copy_to_user((void __user *)arg, &drv_info, sizeof(drv_info))) {
                printk(KERN_DEBUG "megasas: Error copying out drv info\n");
                return -EFAULT;
        }

        return 0;
}

/*
 * IOCtl entry to fetch PCI related info.
 */
static int megasas_mgmt_ioctl_pciinfo(struct file *file, unsigned long arg)
{
	struct megasas_instance *instance;
	MR_DRV_PCI_INFO drv_pciinfo;
	MR_DRV_PCI_INFORMATION *PciInfo = &drv_pciinfo.PciInfo;
	MR_DRV_PCI_LINK_CAPABILITY *linkCapability = &(PciInfo->capability.linkCapability);
	MR_DRV_PCI_LINK_STATUS_CAPABILITY *linkStatusCapability = &(PciInfo->capability.linkStatusCapability);

	u8 *pChar = NULL;
	u32 pos, count;

	/* Clear drv_pciinfo data structure */
	memset(&drv_pciinfo, 0, sizeof(MR_DRV_PCI_INFO));

	/* Read hostno from user land */
	if (copy_from_user(&drv_pciinfo, (void __user *)arg, sizeof(MR_DRV_PCI_INFO)))
        	return -EFAULT;

	/* Extract instance of device for which PCI info has to be fetched */
	instance =  megasas_lookup_instance(drv_pciinfo.hostno);
	if (!instance)
		return -ENODEV;

	/* Read standard PCI config space */
	pChar = (u8 *) &(PciInfo->pciHeaderInfo);
	for(count = 0; count < sizeof(MR_DRV_PCI_COMMON_HEADER); count++) {
		pci_read_config_byte( instance->pdev, count, pChar);
		pChar++;
	}

	PciInfo->busNumber = instance->pdev->bus->number;
	PciInfo->deviceNumber = PCI_SLOT(instance->pdev->devfn);
	PciInfo->functionNumber = PCI_FUNC(instance->pdev->devfn);
	PciInfo->interruptVector = PciInfo->pciHeaderInfo.u.type0.interruptLine;

	/* Extract PCI capabilities info for link & link status */
	pos = pci_find_capability(instance->pdev, PCI_CAP_ID_EXP);
	if(pos) {
		pci_read_config_dword(instance->pdev, pos + PCI_EXP_LNKCAP, (u32 *) &(linkCapability->u.asUlong) );
		pci_read_config_word(instance->pdev, pos + PCI_EXP_LNKSTA, (u16 *) &(linkStatusCapability->u.asUshort) );
	}

	/* Copy PCI info to userspace */
	if (copy_to_user((void __user *)arg, &drv_pciinfo, sizeof(MR_DRV_PCI_INFO))) {
		printk(KERN_DEBUG "megasas: Error copying out pci info\n");
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(__VMKLNX__) */

/**
 * megasas_mgmt_ioctl -	char node ioctl entry point
 */
static long
megasas_mgmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        switch (cmd) {
        case MEGASAS_IOC_FIRMWARE:
#if !defined(__VMKLNX__)
                return megasas_mgmt_ioctl_fw(file, arg);
#else /* defined(__VMKLNX__) */
        {
                struct megasas_iocpacket ioc;

                if (copy_from_user(&ioc, (struct megasas_iocpacket __user *)arg,
                            sizeof(struct megasas_iocpacket))) {
                        return EFAULT;
                }
                return megasas_mgmt_ioctl_fw(file, (unsigned long)&ioc);
        }
#endif /* !defined(__VMKLNX__) */

        case MEGASAS_IOC_GET_AEN:
                return megasas_mgmt_ioctl_aen(file, arg);

#if defined(__VMKLNX__)
        case MEGASAS_IOC_GET_HOST_INFO:
                return megasas_mgmt_ioctl_host_info(file, arg);

        case MEGASAS_IOC_GET_DRV_INFO:
                return megasas_mgmt_ioctl_drv_info(file, arg);
#endif /* defined(__VMKLNX__) */
        }

	return -ENOTTY;
}

#ifdef CONFIG_COMPAT
static int megasas_mgmt_compat_ioctl_fw(struct file *file, unsigned long arg)
{
	struct compat_megasas_iocpacket __user *cioc =
	    (struct compat_megasas_iocpacket __user *)arg;
#if !defined(__VMKLNX__)
	struct megasas_iocpacket __user *ioc =
	    compat_alloc_user_space(sizeof(struct megasas_iocpacket));
#else /* defined(__VMKLNX__) */
	/* This is just about 300 bytes and so define on stack */
	struct megasas_iocpacket ioc;
#endif /* !defined(__VMKLNX__) */
	int i;
	int error = 0;

#if !defined(__VMKLNX__)
	if (clear_user(ioc, sizeof(*ioc)))
		return -EFAULT;
#else /* defined(__VMKLNX__) */
	memset(&ioc, 0, sizeof(struct megasas_iocpacket));
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
	if (copy_in_user(&ioc->host_no, &cioc->host_no, sizeof(u16)) ||
	    copy_in_user(&ioc->sgl_off, &cioc->sgl_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_off, &cioc->sense_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_len, &cioc->sense_len, sizeof(u32)) ||
	    copy_in_user(ioc->frame.raw, cioc->frame.raw, 128) ||
	    copy_in_user(&ioc->sge_count, &cioc->sge_count, sizeof(u32)))
		return -EFAULT;
#else /* defined(__VMKLNX__) */
	if (copy_from_user(&ioc.host_no, &cioc->host_no, sizeof(u16)) ||
	    copy_from_user(&ioc.sgl_off, &cioc->sgl_off, sizeof(u32)) ||
	    copy_from_user(&ioc.sense_off, &cioc->sense_off, sizeof(u32)) ||
	    copy_from_user(&ioc.sense_len, &cioc->sense_len, sizeof(u32)) ||
	    copy_from_user(ioc.frame.raw, cioc->frame.raw, 128) ||
	    copy_from_user(&ioc.sge_count, &cioc->sge_count, sizeof(u32))) {
		return -EFAULT;
	}
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
	for (i = 0; i < MAX_IOCTL_SGE; i++) {
		compat_uptr_t ptr;

		if (get_user(ptr, &cioc->sgl[i].iov_base) ||
		    put_user(compat_ptr(ptr), &ioc->sgl[i].iov_base) ||
		    copy_in_user(&ioc->sgl[i].iov_len,
				 &cioc->sgl[i].iov_len, sizeof(compat_size_t)))
			return -EFAULT;
	}
#else /* defined(__VMKLNX__) */
	for (i = 0; i < MAX_IOCTL_SGE; i++) {
		if (copy_from_user(&ioc.sgl[i].iov_base, &cioc->sgl[i].iov_base, sizeof(u32)) ||
		    copy_from_user(&ioc.sgl[i].iov_len,
				 &cioc->sgl[i].iov_len, sizeof(compat_size_t)))
			return -EFAULT;
	}
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
	error = megasas_mgmt_ioctl_fw(file, (unsigned long)ioc);
#else /* defined(__VMKLNX__) */
	error = megasas_mgmt_ioctl_fw(file, (unsigned long)&ioc);
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
	if (copy_in_user(&cioc->frame.hdr.cmd_status,
			 &ioc->frame.hdr.cmd_status, sizeof(u8))) {
#else /* defined(__VMKLNX__) */
	if (copy_to_user(&cioc->frame.hdr.cmd_status,
			 &ioc.frame.hdr.cmd_status, sizeof(u8))) {
#endif /* !defined(__VMKLNX__) */
		printk(KERN_DEBUG "megasas: error copy_in_user cmd_status\n");
		return -EFAULT;
	}
	return error;
}

static long
megasas_mgmt_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
        switch (cmd) {
        case MEGASAS_IOC_FIRMWARE32:
                return megasas_mgmt_compat_ioctl_fw(file, arg);
        case MEGASAS_IOC_GET_AEN:
                return megasas_mgmt_ioctl_aen(file, arg);
#if defined(__VMKLNX__)
        case MEGASAS_IOC_GET_HOST_INFO:
                return megasas_mgmt_ioctl_host_info(file, arg);
       case MEGASAS_IOC_GET_DRV_INFO:
                return megasas_mgmt_ioctl_drv_info(file, arg);
	case MEGASAS_IOC_GET_PCI_INFO:
		return megasas_mgmt_ioctl_pciinfo(file, arg); 
#endif /* defined(__VMKLNX__) */
        }

        return -ENOTTY;

}
#endif

/*
 * File operations structure for management interface
 */
static const struct file_operations megasas_mgmt_fops = {
	.owner = THIS_MODULE,
	.open = megasas_mgmt_open,
	.release = megasas_mgmt_release,
	.fasync = megasas_mgmt_fasync,
	.unlocked_ioctl = megasas_mgmt_ioctl,
	.poll = megasas_mgmt_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = megasas_mgmt_compat_ioctl,
#endif

};

#if defined(__VMKLNX__)
static struct miscdevice megasas_ioctl_miscdev = {
        MEGASAS_MINOR,
        "megaraid_sas_ioctl",
        &megasas_mgmt_fops
};
#endif /* defined(__VMKLNX__) */

/*
 * PCI hotplug support registration structure
 */
static struct pci_driver megasas_pci_driver = {

	.name = "megaraid_sas",
	.id_table = megasas_pci_table,
	.probe = megasas_probe_one,
	.remove = __devexit_p(megasas_detach_one),
	.suspend = megasas_suspend,
	.resume = megasas_resume,
	.shutdown = megasas_shutdown,
};

/*
 * Sysfs driver attributes
 */
static ssize_t megasas_sysfs_show_version(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_VERSION) + 2, "%s\n",
			MEGASAS_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO, megasas_sysfs_show_version, NULL);

static ssize_t
megasas_sysfs_show_release_date(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_RELDATE) + 2, "%s\n",
			MEGASAS_RELDATE);
}

static DRIVER_ATTR(release_date, S_IRUGO, megasas_sysfs_show_release_date,
		   NULL);

static ssize_t
megasas_sysfs_show_dbg_lvl(struct device_driver *dd, char *buf)
{
	return sprintf(buf,"%u\n",megasas_dbg_lvl);
}

static ssize_t
megasas_sysfs_set_dbg_lvl(struct device_driver *dd, const char *buf, size_t count)
{
	int retval = count;
	if(sscanf(buf,"%u",&megasas_dbg_lvl)<1){
		printk(KERN_ERR "megasas: could not set dbg_lvl\n");
		retval = -EINVAL;
	}
	return retval;
}

static DRIVER_ATTR(dbg_lvl, S_IRUGO|S_IWUGO, megasas_sysfs_show_dbg_lvl,
		   megasas_sysfs_set_dbg_lvl);

/**
 * megasas_init - Driver load entry point
 */
static int __init megasas_init(void)
{
	int rval;

	/*
	 * Announce driver version and other information
	 */
	printk(KERN_INFO "megasas: %s\n", MEGASAS_VERSION);

	memset(&megasas_mgmt_info, 0, sizeof(megasas_mgmt_info));

	/*
	 * Register character device node
	 */
#if defined(__VMKLNX__)
        rval = misc_register(&megasas_ioctl_miscdev);
        if (rval < 0) {
                printk("megasas: Can't register misc device\n");
                return rval;
        }
#else /* defined(__VMKLNX__) */

	rval = register_chrdev(0, "megaraid_sas_ioctl", &megasas_mgmt_fops);

	if (rval < 0) {
		printk(KERN_DEBUG "megasas: failed to open device node\n");
		return rval;
	}

	megasas_mgmt_majorno = rval;
#endif /* defined(__VMKLNX__) */

	/*
	 * Register ourselves as PCI hotplug module
	 */
	rval = pci_register_driver(&megasas_pci_driver);

	if (rval) {
		printk(KERN_DEBUG "megasas: PCI hotplug regisration failed \n");
		goto err_pcidrv;
	}

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_version);
	if (rval)
		goto err_dcf_attr_ver;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_release_date);
	if (rval)
		goto err_dcf_rel_date;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_dbg_lvl);
	if (rval)
		goto err_dcf_dbg_lvl;

	return rval;

	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
err_dcf_dbg_lvl:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);
err_dcf_rel_date:
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);
err_dcf_attr_ver:
	pci_unregister_driver(&megasas_pci_driver);
err_pcidrv:
#if defined(__VMKLNX__)
	misc_deregister(&megasas_ioctl_miscdev);
#else /* !defined(__VMKLNX__) */
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
#endif /* defined(__VMKLNX__) */
  	return rval;
}

/**
 * megasas_exit - Driver unload entry point
 */
static void __exit megasas_exit(void)
{
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);

	pci_unregister_driver(&megasas_pci_driver);
#if defined(__VMKLNX__)
	misc_deregister(&megasas_ioctl_miscdev);
#else /* !defined(__VMKLNX__) */
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
#endif /* defined(__VMKLNX__) */
}

module_init(megasas_init);
module_exit(megasas_exit);
