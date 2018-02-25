/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2014, Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
/*
 * Portions Copyright 2008, 2010 VMware, Inc.
 */
/*
 * This file contains VMware-specific changes to the standard Linux hpsa driver.
 */
/*
 * The following #defines allow the hpsa driver to be compiled for a 
 * variety of kernels.  Despite having names like RHEL5, SLES11, these
 * are more about the kernel than about the OS.  So for instance, if
 * you're running RHEL5 (typically 2.6.18-ish kernel), but you've compiled
 * a custom 2.6.38 or 3.x kernel and you're running that, then you don't want
 * the RHEL5 define, you probably want the default kernel.org (as of this
 * writing circa March 2012)  If you're running the OS vendor's kernel
 * or a kernel that is of roughly the same vintage as the OS vendor's kernel
 * then you can go by the OS name.
 *
 * If you have some intermediate kernel which doesn't quite match any of
 * the predefined sets of kernel features here, you may have to make your own 
 * define for your particular kernel and mix and match the kernel features
 * to fit the kernel you're compiling for.  How can you tell?  By studying
 * the source of this file and the source of the kernel you're compiling for
 * and understanding which "KFEATURES" your kernel has.
 *
 * Usually, if you get it wrong, it won't compile, but there are no doubt
 * some cases in which, if you get it wrong, it will compile, but won't
 * work right.  In any case, if you're compiling this, you're on your own
 * and likely nobody has tested this particular code with your particular
 * kernel, so, good luck, and pay attention to the compiler warnings.
 *
 */

/* #define SLES11sp1 */
/* #define SLES11sp2plus */
/* #define RHEL6 */
/* Default is kernel.org */
#ifdef HPSA_ESX4
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 0
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 0
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 0
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 1
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 0
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 0
#define VMW_HEADERS 1
#define VMW_NEEDS_HPSA_SET_IRQ 1
#define VMW_LIMIT_MAXSGENTRIES 1
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 1
#define VMW_DEFINE_DEBUG_FUNCTIONS 1
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 1
#define VMW_NEEDS_TASK_MANAGEMENT 1
#define VMW_NEEDS_TIME_REDEFINE 1
#define SA_CONTROLLERS_GEN6 1
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#else
#ifdef HPSA_ESX5
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 0
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 0
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 0
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 1
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 0
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 0
#define VMW_HEADERS 1
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 1
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 1
#define VMW_DEFINE_DEBUG_FUNCTIONS 1
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 1
#define VMW_NEEDS_TASK_MANAGEMENT 1
#define VMW_NEEDS_TIME_REDEFINE 1
#define SA_CONTROLLERS_GEN6 1
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1
#else
#ifdef HPSA_ESX6
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 0
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 0
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 0
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 1
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 0
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 0
#define VMW_HEADERS 1
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 1
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 1
#define VMW_DEFINE_DEBUG_FUNCTIONS 1
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 1
#define VMW_NEEDS_TASK_MANAGEMENT 1
#define VMW_NEEDS_TIME_REDEFINE 1
#define SA_CONTROLLERS_GEN6 1
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1
#else

#ifdef RHEL6 /************ RHEL 6 ************/
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 1
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 1
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 0
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 1
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 1
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 1
#define VMW_HEADERS 0
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 0
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 0
#define VMW_DEFINE_DEBUG_FUNCTIONS 0
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 0
#define VMW_NEEDS_TASK_MANAGEMENT 0
#define SA_CONTROLLERS_GEN6 1
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1

#else

#ifdef SLES11sp1 /************* SLES11 sp1 ********/
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 1
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 1
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 0
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 1
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 1
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 1
#define VMW_HEADERS 0
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 0
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 0
#define VMW_DEFINE_DEBUG_FUNCTIONS 0
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 0
#define VMW_NEEDS_TASK_MANAGEMENT 0
#define SA_CONTROLLERS_GEN6 0
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1

#else
#ifdef SLES11sp2plus /************* SLES11 sp2 and after ********/
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 0
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 0
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 1
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 1
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 1
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 0
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 1
#define KFATURE_HAS_ALLOC_WORKQUEUE 0
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 0
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 1
#define VMW_HEADERS 0
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 0
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 0
#define VMW_DEFINE_DEBUG_FUNCTIONS 0
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 0
#define VMW_NEEDS_TASK_MANAGEMENT 0
#define SA_CONTROLLERS_GEN6 0
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1

#else /* Default, kernel.org */
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT 1
#define KFEATURE_HAS_WAIT_FOR_COMPLETION_IO 1
#define KFEATURE_HAS_2011_03_INTERRUPT_HANDLER 1
#define KFEATURE_CHANGE_QDEPTH_HAS_REASON 1
#define KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR 1
#define KFEATURE_HAS_SCSI_QDEPTH_DEFAULT 1
#define KFEATURE_HAS_SCSI_FOR_EACH_SG 1
#define KFEATURE_HAS_SCSI_DEVICE_TYPE 1
#define KFEATURE_SCAN_START_PRESENT 1
#define KFEATURE_SCAN_START_IMPLEMENTED 1
#define KFEATURE_HAS_2011_03_QUEUECOMMAND 1
#define KFEATURE_HAS_SHOST_PRIV 1
#define KFEATURE_HAS_SCSI_DMA_FUNCTIONS 1
#define KFEATURE_HAS_SCSI_SET_RESID 1
#define KFEATURE_HAS_UACCESS_H_FILE 1
#define KFEATURE_HAS_SMP_LOCK_H 0 /* include/linux/smp_lock.h removed between 2.6.38 and 2.6.39 */
#define KFEATURE_HAS_NEW_DMA_MAPPING_ERROR 1
#define KFATURE_HAS_ALLOC_WORKQUEUE 1
#define KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE 1
#define HPSA_SUPPORTS_STORAGEWORKS_1210m 1
#define VMW_HEADERS 0
#define VMW_NEEDS_HPSA_SET_IRQ 0
#define VMW_LIMIT_MAXSGENTRIES 0
#define VMW_NEEDS_SCSI_DMA_FUNCTIONS 0
#define VMW_DEFINE_DEBUG_FUNCTIONS 0
#define VMW_NEEDS_SAS_INFO_FUNCTIONS 0
#define VMW_NEEDS_TASK_MANAGEMENT 0
#define SA_CONTROLLERS_GEN6 1
#define SA_CONTROLLERS_GEN8 1
#define SA_CONTROLLERS_GEN8_2 1
#define SA_CONTROLLERS_GEN8_5 1
#define SA_CONTROLLERS_GEN9 1

#endif /* SLES11sp2plus */
#endif /* SLES11sp1 */
#endif /* RHEL6 */
#endif /* ESX6 */
#endif /* ESX5 */
#endif /* ESX4 */

#if !KFEATURE_HAS_WAIT_FOR_COMPLETION_IO_TIMEOUT
static inline unsigned long wait_for_completion_io_timeout(struct completion *x,
			__attribute__((unused)) unsigned long timeout)
{
	return wait_for_completion_timeout(x, timeout);
}
#endif

#if !KFEATURE_HAS_WAIT_FOR_COMPLETION_IO
static inline unsigned long wait_for_completion_io(struct completion *x)
{
	wait_for_completion(x);
	return 0;
}
#endif

#if KFEATURE_HAS_2011_03_INTERRUPT_HANDLER
	/* new style interrupt handler */
#	define DECLARE_INTERRUPT_HANDLER(handler) \
		static irqreturn_t handler(int irq, void *queue)
#	define INTERRUPT_HANDLER_TYPE(handler) \
		irqreturn_t (*handler)(int, void *)
#else
	/* old style interrupt handler */
#	define DECLARE_INTERRUPT_HANDLER(handler) \
		static irqreturn_t handler(int irq, void *queue, \
			struct pt_regs *regs)
#	define INTERRUPT_HANDLER_TYPE(handler) \
		irqreturn_t (*handler)(int, void *, struct pt_regs *)
#endif


#if KFEATURE_CHANGE_QDEPTH_HAS_REASON
#	define DECLARE_CHANGE_QUEUE_DEPTH(func) \
	static int func(struct scsi_device *sdev, \
		int qdepth, int reason)
#	define BAIL_ON_BAD_REASON \
		{ if (reason != SCSI_QDEPTH_DEFAULT) \
			return -ENOTSUPP; }
#else
#	define DECLARE_CHANGE_QUEUE_DEPTH(func) \
	static int func(struct scsi_device *sdev, int qdepth)
#	define BAIL_ON_BAD_REASON
#endif


#if KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR
#if defined(__VMKLNX__)
#	define DECLARE_DEVATTR_SHOW_FUNC(func) \
		static ssize_t func(struct device *dev, \
				char *buf)

#	define DECLARE_DEVATTR_STORE_FUNC(func) \
	static ssize_t func(struct device *dev, \
		const char *buf, size_t count)
#else
#	define DECLARE_DEVATTR_SHOW_FUNC(func) \
		static ssize_t func(struct device *dev, \
			struct device_attribute *attr, char *buf)

#	define DECLARE_DEVATTR_STORE_FUNC(func) \
	static ssize_t func(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count)
#endif
#	define DECLARE_HOST_DEVICE_ATTR(xname, xmode, xshow, xstore) \
		DEVICE_ATTR(xname, xmode, xshow, xstore)

#	define DECLARE_HOST_ATTR_LIST(xlist) \
	static struct device_attribute *xlist[]
#else /* not KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR */

#	define DECLARE_DEVATTR_SHOW_FUNC(func) \
	static ssize_t func(struct class_device *dev, char *buf)

#	define DECLARE_DEVATTR_STORE_FUNC(func) \
	static ssize_t func(struct class_device *dev, \
		const char *buf, size_t count)

#	define DECLARE_HOST_DEVICE_ATTR(xname, xmode, xshow, xstore) \
	struct class_device_attribute dev_attr_##xname = {\
		.attr = { \
			.name = #xname, \
			.mode = xmode, \
		}, \
		.show = xshow, \
		.store = xstore, \
	};

#	define DECLARE_HOST_ATTR_LIST(xlist) \
	static struct class_device_attribute *xlist[]

#endif /* KFEATURE_HAS_2011_03_STYLE_DEVICE_ATTR */

#ifndef SCSI_QDEPTH_DEFAULT
#	define SCSI_QDEPTH_DEFAULT 0
#endif

#if !KFEATURE_HAS_SCSI_FOR_EACH_SG
#	define scsi_for_each_sg(cmd, sg, nseg, __i) \
	for (__i = 0, sg = scsi_sglist(cmd); __i < (nseg); __i++, (sg)++)
#endif

#if !KFEATURE_HAS_SHOST_PRIV
	static inline void *shost_priv(struct Scsi_Host *shost)
	{
		return (void *) shost->hostdata;
	}
#endif

#if !KFEATURE_HAS_SCSI_DMA_FUNCTIONS
	/* Does not have things like scsi_dma_map, scsi_dma_unmap, scsi_sg_count,
	 * sg_dma_address, sg_dma_len...
	 */

static void hpsa_map_sg_chain_block(struct ctlr_info *h,
	struct CommandList *c);

/* It is not reasonably possible to retrofit the new scsi dma interfaces
 * onto the old code.  So we retrofit at a higher level, at the dma mapping
 * function of the hpsa driver itself.
 *
 * hpsa_scatter_gather takes a struct scsi_cmnd, (cmd), and does the pci
 * dma mapping  and fills in the scatter gather entries of the
 * hpsa command, cp.
 */
static int hpsa_scatter_gather(struct ctlr_info *h,
		struct CommandList *cp,
		struct scsi_cmnd *cmd)
{
	unsigned int len;
	u64 addr64;
	int use_sg, i, sg_index, chained = 0;
	struct SGDescriptor *curr_sg;
	struct scatterlist *sg = (struct scatterlist *) cmd->request_buffer;

	if (!cmd->use_sg) {
		if (cmd->request_bufflen) { /* Just one scatter gather entry */
			addr64 = (__u64) pci_map_single(h->pdev,
				cmd->request_buffer, cmd->request_bufflen,
				cmd->sc_data_direction);

			cp->SG[0].Addr.lower =
				(__u32) (addr64 & (__u64) 0x0FFFFFFFF);
			cp->SG[0].Addr.upper =
				(__u32) ((addr64 >> 32) & (__u64) 0x0FFFFFFFF);
			cp->SG[0].Len = cmd->request_bufflen;
			use_sg = 1;
		} else /* Zero sg entries */
			use_sg = 0;
	} else {
		BUG_ON(cmd->use_sg > h->maxsgentries);

		/* Many sg entries */
		use_sg = pci_map_sg(h->pdev, cmd->request_buffer, cmd->use_sg,
				cmd->sc_data_direction);

		if (use_sg < 0)
			return use_sg;

		sg_index = 0;
		curr_sg = cp->SG;
		use_sg = cmd->use_sg;

		for (i = 0; i < use_sg; i++) {
			if (i == h->max_cmd_sg_entries - 1 &&
				use_sg > h->max_cmd_sg_entries) {
				chained = 1;
				curr_sg = h->cmd_sg_list[cp->cmdindex];
				sg_index = 0;
			}
			addr64 = (__u64) sg_dma_address(&sg[i]);
			len  = sg_dma_len(&sg[i]);
			curr_sg->Addr.lower =
				(u32) (addr64 & 0x0FFFFFFFFULL);
			curr_sg->Addr.upper =
				(u32) ((addr64 >> 32) & 0x0FFFFFFFFULL);
			curr_sg->Len = len;
			curr_sg->Ext = 0;  /* we are not chaining */
			curr_sg++;
		}
	}

	if (use_sg + chained > h->maxSG)
		h->maxSG = use_sg + chained;

	if (chained) {
		cp->Header.SGList = h->max_cmd_sg_entries;
		cp->Header.SGTotal = (u16) (use_sg + 1);
		hpsa_map_sg_chain_block(h, cp);
		return 0;
	}

	cp->Header.SGList = (u8) use_sg;   /* no. SGs contig in this cmd */
	cp->Header.SGTotal = (u16) use_sg; /* total sgs in this cmd list */
	return 0;
}

static void hpsa_unmap_sg_chain_block(struct ctlr_info *h,
	struct CommandList *c);
static void hpsa_scatter_gather_unmap(struct ctlr_info *h,
	struct CommandList *c, struct scsi_cmnd *cmd)
{
	union u64bit addr64;

	if (cmd->use_sg) {
		pci_unmap_sg(h->pdev, cmd->request_buffer, cmd->use_sg,
			cmd->sc_data_direction);
		if (c->Header.SGTotal > h->max_cmd_sg_entries)
			hpsa_unmap_sg_chain_block(h, c);
		return;
	}
	if (cmd->request_bufflen) {
		addr64.val32.lower = c->SG[0].Addr.lower;
		addr64.val32.upper = c->SG[0].Addr.upper;
		pci_unmap_single(h->pdev, (dma_addr_t) addr64.val,
		cmd->request_bufflen, cmd->sc_data_direction);
	}
}

static inline void scsi_dma_unmap(struct scsi_cmnd *cmd)
{
	struct CommandList *c = (struct CommandList *) cmd->host_scribble;

	hpsa_scatter_gather_unmap(c->h, c, cmd);
}

#endif

#if !KFEATURE_HAS_SCSI_DEVICE_TYPE
	/**
	 * scsi_device_type - Return 17 char string indicating device type.
	 * @type: type number to look up
	 */
	const char *scsi_device_type(unsigned type)
	{
		if (type == 0x1e)
			return "Well-known LUN   ";
		if (type == 0x1f)
			return "No Device        ";
		if (type >= ARRAY_SIZE(scsi_device_types))
			return "Unknown          ";
		return scsi_device_types[type];
	}
#endif

#if KFEATURE_SCAN_START_IMPLEMENTED
	/* .scan_start is present in scsi host template AND implemented.
	 * Used to bail out of queuecommand if no scan_start and REPORT_LUNS
	 * encountered
	 */
	static inline int bail_on_report_luns_if_no_scan_start(
		__attribute__((unused)) struct scsi_cmnd *cmd,
		__attribute__((unused)) void (*done)(struct scsi_cmnd *))
	{
		return 0;
	}

	/* RHEL6, kernel.org have functioning ->scan_start() method in kernel
	 * so this is no-op.
	 */
	static inline void hpsa_initial_update_scsi_devices(
		__attribute__((unused)) struct ctlr_info *h)
	{
		return;
	}
#else /* not KFEATURE_SCAN_START_IMPLEMENTED */
	static inline int bail_on_report_luns_if_no_scan_start(
		struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
	{
		/*
		 * This thing bails out of our queue command early on SCSI
		 * REPORT_LUNS This is needed when the kernel doesn't really
		 * support the scan_start method of the scsi host template.
		 *
		 * Since we do our own mapping in our driver, and we handle
		 * adding/removing of our own devices.
		 *
		 * We want to prevent the mid-layer from doing it's own
		 * adding/removing of drives which is what it would do
		 * if we allow REPORT_LUNS to be processed.
		 *
		 * On RHEL5, scsi mid-layer never calls scan_start and
		 * scan_finished even though they exist in scsi_host_template.
		 *
		 * On RHEL6 we use scan_start and scan_finished to tell
		 * mid-layer that we do our own device adding/removing
		 * therefore we can handle REPORT_LUNS.
		 */

		if (cmd->cmnd[0] == REPORT_LUNS) {
			cmd->result = (DID_OK << 16);           /* host byte */
			cmd->result |= (COMMAND_COMPLETE << 8); /* msg byte */
			cmd->result |= SAM_STAT_CHECK_CONDITION;
			memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
			cmd->sense_buffer[2] = ILLEGAL_REQUEST;
			done(cmd);
			return 1;
		}
		return 0;
	}

	/* Need this if no functioning ->scan_start() method in kernel. */
	static void hpsa_update_scsi_devices(struct ctlr_info *h, int hostno);
	static inline void hpsa_initial_update_scsi_devices(
				struct ctlr_info *h)
	{
		hpsa_update_scsi_devices(h, -1);
	}
#endif /* KFEATURE_SCAN_START_IMPLEMENTED */

#if KFEATURE_SCAN_START_PRESENT
	/* .scan_start is present in scsi host template */
	#define INITIALIZE_SCAN_START(funcptr) .scan_start = funcptr,
	#define INITIALIZE_SCAN_FINISHED(funcptr) .scan_finished = funcptr,
#else /* .scan start is not even present in scsi host template */
	#define INITIALIZE_SCAN_START(funcptr)
	#define INITIALIZE_SCAN_FINISHED(funcptr)
#endif

#if KFEATURE_HAS_2011_03_QUEUECOMMAND
#	define DECLARE_QUEUECOMMAND(func) \
		static int func##_lck(struct scsi_cmnd *cmd, \
			void (*done)(struct scsi_cmnd *))
#	define DECLARE_QUEUECOMMAND_WRAPPER(func) static DEF_SCSI_QCMD(func)
#else
#	define DECLARE_QUEUECOMMAND(func) \
	static int func(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
#	define DECLARE_QUEUECOMMAND_WRAPPER(func)
#endif

#if !KFEATURE_HAS_SCSI_SET_RESID
	static inline void scsi_set_resid(struct scsi_cmnd *cmd, int resid)
	{
		cmd->resid = resid;
	}
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#endif

/* Define old style irq flags SA_* if the IRQF_* ones are missing. */
#ifndef IRQF_DISABLED
#define IRQF_DISABLED (SA_INTERRUPT | SA_SAMPLE_RANDOM)
#endif

#if KFEATURE_HAS_UACCESS_H_FILE
#if defined(__VMKLNX__)
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif
#else
#endif

#if KFEATURE_HAS_SMP_LOCK_H
#include <linux/smp_lock.h>
#endif

/*
 * Support for packaged storage solutions.
 * Enabled by default for kernel.org 
 * Enable above as required for distros.
 */
#if HPSA_SUPPORTS_STORAGEWORKS_1210m
#define HPSA_STORAGEWORKS_1210m_PCI_IDS \
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_CISSE, 0x103C, 0x3233}, \
	{PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_CISSF, 0x103C, 0x333F},	\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x0076},\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x007d},\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x0077},\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x0087},\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x0088},\
	{PCI_VENDOR_ID_3PAR,	PCI_DEVICE_ID_3PAR,	0x1590, 0x0089},

#define HPSA_STORAGEWORKS_1210m_PRODUCT_ENTRIES \
	{0x3233103C, "HP StorageWorks 1210m", &SA5_access}, \
	{0x333F103C, "HP StorageWorks 1210m", &SA5_access}, \
   {0x00761590, "HP Storage P1224 Array Controller", &SA5_access}, \
   {0x007d1590, "HP Storage P1228 Array Controller", &SA5_access}, \
   {0x00771590, "HP Storage P1228m Array Controller", &SA5_access}, \
   {0x00871590, "HP Storage P1224e Array Controller", &SA5_access}, \
   {0x00881590, "HP Storage P1228e Array Controller", &SA5_access}, \
   {0x00891590, "HP Storage P1228em Array Controller", &SA5_access},
   

#else
#define HPSA_STORAGEWORKS_1210m_PCI_IDS	
#define HPSA_STORAGEWORKS_1210m_PRODUCT_ENTRIES
#endif

#if VMW_HEADERS
	/*************
	 * INCLUDES
	 *************/
	#include <scsi/scsi_transport_sas.h>
	#include <linux/proc_fs.h>

	/**************
	 * DEFINES
	 **************/

	#define scsi_for_each_sg(cmd, sg, nseg, __i)	\
		for_each_sg(scsi_sglist(cmd), sg, nseg, __i)
	#if !(defined(HPSA_ESX5_0) || defined(HPSA_ESX5_1) || defined(HPSA_ESX5_5) || defined(HPSA_ESX6_0))
	#define for_each_sg(sglist, sg, nr, __i)	\
		for (__i = 0, sg = (sglist);__i < (nr); __i++, sg = sg_next(sg))
	#endif /* not ESX5_x */
	#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
	#define CONTROLLER_DEVICE 7     /* Physical device type of controller */
	#define MAX_CTLR        10       /* Only support 10 controllers in VMware */
	#define HPSA_VMWARE_SGLIMIT 129	/* limit maxsgentries to conserve heap */
	struct scsi_transport_template *hpsa_transport_template = NULL;

	#define ENG_GIG 1000000000
	#define ENG_GIG_FACTOR (ENG_GIG/512)

	/**************
	 * STRUCTURES
	 **************/
	u64 target_sas_id[MAX_CTLR][MAX_EXT_TARGETS];
	u64 cntl_sas_id[MAX_CTLR];

	static struct proc_dir_entry *proc_hpsa;

	/* translate scsi cmd->result values into english */
	static const char *host_status[] =
	{ 	"OK", 		/* 0x00 */
		"NO_CONNECT", 	/* 0x01 */
		"BUS_BUSY",	/* 0x02 */
		"TIME_OUT", 	/* 0x03 */
		"BAD_TARGET",	/* 0x04 */
		"ABORT",	/* 0x05 */
		"PARITY",	/* 0x06 */
		"ERROR",	/* 0x07 */
		"RESET",	/* 0x08 */
		"BAD_INTR",	/* 0x09 */
		"PASSTHROUGH",	/* 0x0a */
		"SOFT_ERROR",	/* 0x0b */
		"IMM_RETRY",	/* 0x0c */
		"REQUEUE"	/* 0x0d */
	};

	/* Keep a link between character devices and matching controllers */
	struct ctlr_rep {
		int chrmajor;		/* major dev# of ctlr's character device */
		struct ctlr_info *h;	/* pointer to driver's matching ctlr struct */
	};
	static struct ctlr_rep cch_devs[MAX_CTLR]; /* ctlr char devices array */
#endif

#if VMW_NEEDS_HPSA_SET_IRQ
	/* hpsa_set_irq
	 *	Set pdev->irq, so that it contains the one that was registered.
	 *	This hack is needed to fix coredump failure - see pr# 360662
	 */
	static inline void hpsa_set_irq(struct ctlr_info *h, struct pci_dev *pdev)
	{
	        pdev->irq = h->intr[PERF_MODE_INT];
	        printk("%s: <0x%x> at PCI %s - pdev->irq: %u\n",
	                h->devname, pdev->device, pci_name(pdev), pdev->irq);
	}
#endif

#if VMW_DEFINE_DEBUG_FUNCTIONS
	/* hpsa_dbmsg
	 *	Print the indicated message,
	 *	with variable args,
	 *	if controller's debug level
	 *	is at or above given value.
	 */
	static void hpsa_dbmsg(struct ctlr_info *h, int debuglvl, char *fmt, ...)
	{
		if(h->debug_msg >= debuglvl) {
			char msg[256];
			va_list argp;   /* pointer to var args */
			va_start(argp, fmt);
			vsprintf(msg, fmt, argp);
			va_end(argp);
			printk(KERN_WARNING "hpsa%d: %s", h->ctlr, msg);
		}
	}


	/* hpsa_db_dev
	 * Show some hpsa device information.
	 */
	static void hpsa_db_dev(char * label, struct ctlr_info *h, struct hpsa_scsi_dev_t *d, int dnum)
	{
		if (dnum < 0 ) {
			/* don't include device number if it is irrelevant: */
			hpsa_dbmsg(h, 5, "%s: B%d:T%d:L%d\n",
				label, d->bus, d->target, d->lun);
			hpsa_dbmsg(h, 5, "%s: devtype:    "
				"0x%02x\n", label, d->devtype);
			hpsa_dbmsg(h, 5, "%s: vendor:     "
				"%.8s\n", label, d->vendor);
			hpsa_dbmsg(h, 5, "%s: model:      "
				"%.16s\n", label, d->model);
			hpsa_dbmsg(h, 5, "%s: device_id:  "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x "
				"%02x%02x%02x%02x %02x%02x%02x%02x\n",
				label, dnum,
				d->device_id[0], d->device_id[1],
				d->device_id[2], d->device_id[3],
				d->device_id[4], d->device_id[5],
				d->device_id[6], d->device_id[7],
				d->device_id[8], d->device_id[9],
				d->device_id[10], d->device_id[11],
				d->device_id[12], d->device_id[13],
				d->device_id[14], d->device_id[15]);
			hpsa_dbmsg(h, 5, "%s: raid_level: "
				"%d\n", label, d->raid_level);
			hpsa_dbmsg(h, 5, "%s: scsi3addr:  "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
				label,
				d->scsi3addr[0], d->scsi3addr[1],
				d->scsi3addr[2], d->scsi3addr[3],
				d->scsi3addr[4], d->scsi3addr[5],
				d->scsi3addr[6], d->scsi3addr[7]);
		}
		else {
			/* include device number in messages*/
			hpsa_dbmsg(h, 5, "%s: device %d: B%d:T%d:L%d\n",
				label, dnum, d->bus, d->target, d->lun);
			hpsa_dbmsg(h, 5, "%s: device %d: devtype:    "
				"0x%02x\n", label, dnum, d->devtype);
			hpsa_dbmsg(h, 5, "%s: device %d: vendor:     "
				"%.8s\n", label, dnum, d->vendor);
			hpsa_dbmsg(h, 5, "%s: device %d: model:      "
				"%.16s\n", label, dnum, d->model);
			hpsa_dbmsg(h, 5, "%s: device %d: device_id:  "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x "
				"%02x%02x%02x%02x %02x%02x%02x%02x\n",
				label, dnum,
				d->device_id[0], d->device_id[1],
				d->device_id[2], d->device_id[3],
				d->device_id[4], d->device_id[5],
				d->device_id[6], d->device_id[7],
				d->device_id[8], d->device_id[9],
				d->device_id[10], d->device_id[11],
				d->device_id[12], d->device_id[13],
				d->device_id[14], d->device_id[15]);
			hpsa_dbmsg(h, 5, "%s: device %d: raid_level: "
				"%d\n", label, dnum, d->raid_level);
			hpsa_dbmsg(h, 5, "%s: device %d: scsi3addr:  "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
				label, dnum,
				d->scsi3addr[0], d->scsi3addr[1],
				d->scsi3addr[2], d->scsi3addr[3],
				d->scsi3addr[4], d->scsi3addr[5],
				d->scsi3addr[6], d->scsi3addr[7]);
		}
	}

	/* hpsa_db_sdev
	 * Show some scsi device information.
	 */
	static void hpsa_db_sdev(char * label, struct ctlr_info *h, struct scsi_device *d)
	{
		hpsa_dbmsg(h, 5, "%s: B%d:T%d:L%d\n",
			label, d->channel, d->id, d->lun);
		hpsa_dbmsg(h, 5, "%s: Mfg:         "
			"%u\n", label, d->manufacturer);
		hpsa_dbmsg(h, 5, "%s: type:        "
			"0x%02x\n", label, d->type);
		hpsa_dbmsg(h, 5, "%s: scsi_level:  "
			"0x%02x\n", label, d->scsi_level);
		hpsa_dbmsg(h, 5, "%s: periph_qual: "
			"0x%02x\n", label, d->inq_periph_qual);
		hpsa_dbmsg(h, 5, "%s: vendor:      "
			"%.8s\n", label, d->vendor);
		hpsa_dbmsg(h, 5, "%s: model:       "
			"%.16s\n", label, d->model);
		hpsa_dbmsg(h, 5, "%s: rev:         "
			"%.4s\n", label, d->rev);
		hpsa_dbmsg(h, 5, "%s: scsi_target: "
			"B%d:T%d\n", label,
			d->sdev_target->channel,
			d->sdev_target->id);
	}

	/* hpsa_db_scmnd
	 * Show some scsi command information
	 */
	static void hpsa_db_scmnd(char * label, struct ctlr_info *h, struct scsi_cmnd *c)
	{

		if (c == NULL ) {
			hpsa_dbmsg(h, 5, "%s: command: NULL\n", label);
			return;
		}
		if (c->device == NULL ) {
			hpsa_dbmsg(h, 5, "%s: command: DEVICE is NULL\n", label);
			return;
		}

		hpsa_dbmsg(h, 5, "%s: command: sent to device : B%d:T%d:L%d\n",
			label, c->device->channel, c->device->id, c->device->lun);
		hpsa_dbmsg(h, 5, "%s: command: SN:              %lu\n",
			label, c->serial_number);
		//hpsa_dbmsg(h, 3, "%s: command: cmd_len:         %u\n",
		//	label, c->cmd_len);
		hpsa_dbmsg(h, 5, "%s: command: request_bufflen: %u\n",
			label, c->request_bufflen);

		hpsa_dbmsg(h, 5, "%s: command:                  "
			"[%02x %02x %02x %02x %02x %02x %02x %02x"
			" %02x %02x %02x %02x %02x %02x %02x %02x]\n",
			label,
			c->cmnd[0], c->cmnd[1], c->cmnd[2], c->cmnd[3],
			c->cmnd[4], c->cmnd[5], c->cmnd[6], c->cmnd[7],
			c->cmnd[8], c->cmnd[9], c->cmnd[10], c->cmnd[11],
			c->cmnd[12], c->cmnd[13], c->cmnd[14], c->cmnd[15]);
		hpsa_dbmsg(h, 5, "%s: command: use_sg:           %u\n",
			label, c->use_sg);
		hpsa_dbmsg(h, 5, "%s: command: sglist_len:       %u\n",
			label, c->sglist_len);
		hpsa_dbmsg(h, 5, "%s: command: underflow:        %u\n",
			label, c->underflow);
		hpsa_dbmsg(h, 5, "%s: command: transfersize:     %u\n",
			label, c->transfersize);
		hpsa_dbmsg(h, 5, "%s: command: resid:            %u\n",
			label, c->resid);
		hpsa_dbmsg(h, 5, "%s: command: result:           %d\n",
			label, c->result);
	}

	/* hpsa_set_debug
	 * 	Set controller debug level feature
	 *	and return true if proc_write content
	 * 	is a command to set or change debug level.
	 *	Otherwise return false.
	 */
	static int hpsa_set_debug(struct ctlr_info *h, char *cmd)
	{
		unsigned long flags;

	        /* debug printing level */
		if (strncmp("debug", cmd, 5)==0) {
			spin_lock_irqsave( &h->lock, flags);
			if (cmd[5] == '\0')
				h->debug_msg=1;
			else if (cmd[5] == '1')
				h->debug_msg=1;
			else if (cmd[5] == '2')
				h->debug_msg=2;
			else if (cmd[5] == '3')
				h->debug_msg=3;
			else if (cmd[5] == '4')
				h->debug_msg=4;
			else if (cmd[5] == '5')
				h->debug_msg=5;
			else
				h->debug_msg=0;
			spin_unlock_irqrestore(&h->lock, flags);
			printk(KERN_WARNING
				"hpsa%d: debug messaging set to level %d\n",
				h->ctlr, h->debug_msg);
			return 1;
		}
		/* no debug printing */
		else if (strcmp("nodebug", cmd)==0) {
			spin_lock_irqsave(&h->lock, flags);
			h->debug_msg=0;
			spin_unlock_irqrestore(&h->lock, flags);
			printk(KERN_WARNING
				"hpsa%d: Disabled debug messaging.\n",
				h->ctlr);
			return 1;
		}
		return 0;
	}

	/*
	 * decode_CC
	 * 	Decode check condition codes.
	 *	For certain types of errors,
	 * 	change command's result and/or
	 * 	change command's residual count.
	 * Args:
	 * 	h	The controller info pointer
	 * 	cp	The command pointer
	 * 	msg	Buffer for detailed decode messages
	 * 	ml 	Detail message length, cumulative
	 * 	dmsg	Buffer for error decode msgs for dev_warn
	 * 	dl 	dev_warn message length, cumulative
	 * Returns:
	 *	1 - if we changed the cmd->result.
	 *	0 - if we did not change it.
	 */
	static int decode_CC(
		struct ctlr_info *h,
		struct CommandList *cp,
		char *msg,
		int *ml,
		char *dmsg,
		int *dl)
	{
		struct scsi_cmnd *cmd;
		struct ErrorInfo *ei;

		unsigned char sense_key;
		unsigned char asc;      /* additional sense code */
		unsigned char ascq;     /* additional sense code qualifier */
		int orig;		/* For detecting when we change cmd result */

	        ei = cp->err_info;
	        cmd = (struct scsi_cmnd *) cp->scsi_cmd;
		if (cmd == NULL )
			return 0;
		orig = cmd->result;	/* remember the original result */

		if ( ei->ScsiStatus  ) {
			sense_key = 0xf & ei->SenseInfo[2];
			asc = ei->SenseInfo[12];
			ascq = ei->SenseInfo[13];
		}
		else {
			sense_key = 0;
			asc = 0;
			ascq = 0;
		}


		switch(sense_key)
		{

		case RECOVERED_ERROR:
			*ml+=sprintf(msg+*ml,"RECOVERED_ERROR. ");
			*dl+=sprintf(dmsg+*dl,"Recovered error. ");
			break;

		case NOT_READY:
			*ml+=sprintf(msg+*ml, "NOT_READY. ");
			*dl+=sprintf(dmsg+*dl, "is not ready. ");
			*ml+=sprintf(msg+*ml, "Reset resid. ");
			cmd->resid = cmd->request_bufflen;

			if ( ( asc == 0x04)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Cause not reportable. ");
			else if ( ( asc == 0x04)  && (ascq == 0x01) )
				*ml+=sprintf(msg+*ml, "In process, becoming ready. ");
			else if ( ( asc == 0x04)  && (ascq == 0x02) )
				*ml+=sprintf(msg+*ml, "Start unit command needed. ");
			else if ( ( asc == 0x04)  && (ascq == 0x03) ) {
				cmd->result = DID_NO_CONNECT << 16;
				*ml+=sprintf(msg+*ml, "Manual Intervention required. ");
			}
			else if ( ( asc == 0x05)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Logical unit does not respond "
					"to selection. ");
			else if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) )
				*ml+=sprintf(msg+*ml, "ALUA asymmetric access "
					"state change. ");
			else if ( ( asc == 0x3a)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Medium not present. ");
			else if ( ( asc == 0x3e)  && (ascq == 0x01) )
				*ml+=sprintf(msg+*ml, "Logical Unit Failure. ");
			else if ( ( asc == 0x3e)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "LUN not configured. ");
			break;


		case MEDIUM_ERROR:
			*ml+=sprintf(msg+*ml, "MEDIUM ERROR. ");
			*dl+=sprintf(dmsg+*dl, "has medium error. ");
			*ml+=sprintf(msg+*ml, "Reset resid. ");
			cmd->resid = cmd->request_bufflen;

			if ( ( asc == 0x0c)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Unrecovered write. ");
			else if ( ( asc == 0x11)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Unrecovered read. ");
			break;


		case HARDWARE_ERROR:
			*ml+=sprintf(msg+*ml, "HARDWARE_ERROR. ");
			*dl+=sprintf(dmsg+*dl, "has hardware error. ");
			if ( ( asc == 0x0c)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Write error. ");
			break;

		case ILLEGAL_REQUEST:
			*ml+=sprintf(msg+*ml, "ILLEGAL_REQUEST. ");
			*dl+=sprintf(dmsg+*dl, "Illegal Request. ");
			*ml+=sprintf(msg+*ml, "Reset resid. ");
			cmd->resid = cmd->request_bufflen;

			/*
			 * Suppress Illegal Request messages in hpsa for hpsa's
			 * unsupported scsi commands.
			 *
			 * Note: The decode for command 0x93 WRITE SAME (16) is not
			 * in the scsi.h decodes as a standard defined value. It
			 * is an optional SCSI command not supported in hpsa, but
			 * causes occasional log spew when the calling storage
			 * stacks invoke it.
			 */

			/* suppress msg for unsupported LOG_SENSE code */
			if ( ( cp->Request.CDB[0] == LOG_SENSE ) ) {
				*ml = 0;
				*dl = 0;
			}
			/* suppress msg for unsupported 0x93 WRITE SAME (16) code */
			else if ( ( cp->Request.CDB[0] == 0x93 ) ) {
				*ml = 0;
				*dl = 0;
			}
			else if ( ( asc == 0x20)  && (ascq == 0x0) ) {
				*ml+=sprintf(msg+*ml, "Invalid OP Code. ");
				if (cp->Request.CDB[0] == REPORT_LUNS) {
					*ml = 0; /* suppress noisy complaint */
					*dl = 0; /* SA does not support report luns. */
				}
			}
			else if ( ( asc == 0x24)  && (ascq == 0x0) ) {
				*ml+=sprintf(msg+*ml, "Invalid field in CDB. ");

				/* suppress msg for unsupported inquiry codes */
				if  (	cp->Request.CDB[0] == INQUIRY
					&&
					( cp->Request.CDB[2] != 0x00 &&
					cp->Request.CDB[2] != 0x83 &&
					cp->Request.CDB[2] != 0xC1 &&
					cp->Request.CDB[2] != 0xC2
					) ) {
					*ml = 0;
					*dl = 0;
				}
				/* suppress msg for unsupported sense codes */
				if  (	cp->Request.CDB[0] == MODE_SENSE
					&&
					( cp->Request.CDB[2] != 0x00 &&
					cp->Request.CDB[2] != 0x01 &&
					cp->Request.CDB[2] != 0x03 &&
					cp->Request.CDB[2] != 0x04 &&
					cp->Request.CDB[2] != 0x08 &&
					cp->Request.CDB[2] != 0x3F &&
					cp->Request.CDB[2] != 0x19
					) ) {
					*ml = 0;
					*dl = 0;
				}
			}
			else if ( ( asc == 0x25)  && (ascq == 0x0) ) {
				*ml+=sprintf(msg+*ml, "LUN not supported. ");
				*dl+=sprintf(dmsg+*dl, "LUN not supported. No Connection. ");
				cmd->result = DID_NO_CONNECT << 16;
			}
			else if ( ( asc == 0x26)  && (ascq == 0x04) )
				*ml+=sprintf(msg+*ml, "Invalid release of "
					"persistent reservation. ");
			else if ( ( asc == 0x27)  && (ascq == 0x01) )
				*ml+=sprintf(msg+*ml, "LUN is write protected "
					"by LUN mapping configuration. ");
			else if ( ( asc == 0x55)  && (ascq == 0x02) )
				*ml+=sprintf(msg+*ml, "Insufficient persistent "
					"reservation resources. ");
			break;

		case UNIT_ATTENTION:
			*ml+=sprintf(msg+*ml, "UNIT_ATTENTION. ");
			*dl+=sprintf(dmsg+*dl, "has unit attention. ");
			*ml+=sprintf(msg+*ml, "Reset resid. ");
			cmd->resid = cmd->request_bufflen;
			cmd->result = DID_SOFT_ERROR << 16;
			if ( ( asc == 0x28)  && (ascq == 0x00) )
				*ml+=sprintf(msg+*ml, "Logical Drive just created. ");
			else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x00) )
				*ml+=sprintf(msg+*ml, "Power On, reset, or Target "
					"reset occurred. ");
			else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x01) )
				*ml+=sprintf(msg+*ml, "Power On or controller "
					"reboot occurred. ");
			else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x02) )
				*ml+=sprintf(msg+*ml, "LIP or SCSI bus "
					"reset occurred. ");
			else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x03) )
				*ml+=sprintf(msg+*ml, "Target or Logical Drive "
					"reset occurred. ");
			else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x04) )
				*ml+=sprintf(msg+*ml, "Controller Failover or "
					"reset occurred. ");
			else if ( ( asc == STATE_CHANGED)  && (ascq == 0x03) )
				*ml+=sprintf(msg+*ml, "Reservations preempted. ");
			else if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) )
				*ml+=sprintf(msg+*ml, "ALUA asymmetric access "
					"state change. ");
			else if ( ( asc == STATE_CHANGED)  && (ascq == 0x07) )
				*ml+=sprintf(msg+*ml, "ALUA implicit  "
					"state transition failed. ");
			else if ( ( asc == STATE_CHANGED)  && (ascq == 0x09) )
				*ml+=sprintf(msg+*ml, "LUN size has changed. ");
			else if ( ( asc == 0x2f)  && (ascq == 0x00) )
				*ml+=sprintf(msg+*ml, "Commands cleared by "
					"another initiator. ");
			else if ( ( asc == 0x3F)  && (ascq == 0x0E) ) {
				*ml+=sprintf(msg+*ml, "Reported LUN data changed. ");
				*ml+=sprintf(msg+*ml, "Waking rescan thread. ");
				//wake_up_process(h->rescan_thread);
				h->drv_req_rescan = 1;	/* schedule controller for a rescan */
			}
			break;

		case DATA_PROTECT:
			*ml+=sprintf(msg+*ml, "DATA_PROTECT. ");
			*dl+=sprintf(dmsg+*dl, "Data is protected. ");
			break;


		case BLANK_CHECK:
			*ml+=sprintf(msg+*ml, "BLANK_CHECK. ");
			*dl+=sprintf(dmsg+*dl, "Blank Check. ");
			break;


		case 0x09: /* Vendor-specific issues */
			*ml+=sprintf(msg+*ml, "HP-SPECIFIC ERROR. ");
			*dl+=sprintf(dmsg+*dl, "A vendor-specific error occured. ");
			if ( ( asc == 0x80)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Could not allocate memory. ");
			else if ( ( asc == 0x80)  && (ascq == 0x01) )
				*ml+=sprintf(msg+*ml, "Passthrough not allowed. ");
			else if ( ( asc == 0x80)  && (ascq == 0x02) )
				*ml+=sprintf(msg+*ml, "Invalid Backend channel. ");
			else if ( ( asc == 0x80)  && (ascq == 0x03) )
				*ml+=sprintf(msg+*ml, "Target did not respond. ");
			else if ( ( asc == 0x80)  && (ascq == 0x04) )
				*ml+=sprintf(msg+*ml, "General error. ");
			break;

		case COPY_ABORTED:
			*ml+=sprintf(msg+*ml, "COPY_ABORTED. ");
			*dl+=sprintf(dmsg+*dl, "Copy operation aborted. ");
			break;

		case ABORTED_COMMAND:
			*ml+=sprintf(msg+*ml, "ABORTED_COMMAND. ");
			*dl+=sprintf(dmsg+*dl, "Aborted command. ");
			*ml+=sprintf(msg+*ml, "Reset resid. ");
			cmd->resid = cmd->request_bufflen;
			if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) )
				*ml+=sprintf(msg+*ml,
					"ALUA asymmetric access state change: "
					"Forwarded command aborted during failover. ");
			else if ( ( asc == 0x47)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Parity or CRC error in data. ");
			else if ( ( asc == 0x4e)  && (ascq == 0x0) )
				*ml+=sprintf(msg+*ml, "Overlapped commands. ");
			break;

		case VOLUME_OVERFLOW:
			*ml+=sprintf(msg+*ml, "VOLUME_OVERFLOW. ");
			*dl+=sprintf(dmsg+*dl, "detected volume overflow. ");
			break;

		case MISCOMPARE:
			*ml+=sprintf(msg+*ml, "MISCOMPARE. ");
			*dl+=sprintf(dmsg+*dl, "detected miscompare. ");
			break;

		default:
			*ml+=sprintf(msg+*ml, "UNKNOWN SENSE KEY. ");
			*dl+=sprintf(dmsg+*dl, "Had unknown sense key. ");
			break;

		} /* end switch sense_key */


		/* If we changed the cmd->result, indicate that
		 * via the return value.
		 */
		if (cmd->result == orig )
			return 0;	/* no change */
		else
			return 1;	/* changed */
	}

#ifdef HPSA_DEBUG
/* is_abort_cmd
 *	Return true if the indicated command
 *	is a Task Management abort request.
 */
static int is_abort_cmd(struct CommandList *c)
{
	if (c == NULL )
		return 0;
	if (	(c->cmd_type == CMD_IOCTL_PEND)
		&&
		(c->Request.CDB[0] == HPSA_TASK_MANAGEMENT)
		&&
		(c->Request.CDB[1] == HPSA_TMF_ABORT_TASK)
	)
		return 1;

	return 0;
}
#endif /* HPSA_DEBUG */
#endif

#if VMW_LIMIT_MAXSGENTRIES
	/* hpsa_limit_maxsgentries
	 *	Set h->maxsgentries to reduce total memory consumed by sg chaining
	 *	on VMware, since total memory consumed by driver for all controllers
	 *	needs to be < 30MB.
	 */
	static inline void hpsa_limit_maxsgentries(struct ctlr_info *h)
	{
		if ( h->maxsgentries > HPSA_VMWARE_SGLIMIT ) {
			hpsa_dbmsg(h, 0, "Reducing controller-supported maxsgentries "
				"of %d to %d to conserve driver heap memory.\n",
				h->maxsgentries, HPSA_VMWARE_SGLIMIT);
			h->maxsgentries = HPSA_VMWARE_SGLIMIT;
		}
	}
#endif

#if VMW_NEEDS_SCSI_DMA_FUNCTIONS
	/**
	 * scsi_dma_map - perform DMA mapping against command's sg lists
	 * @cmd:	scsi command
	 *
	 * Returns the number of sg lists actually used, zero if the sg lists
	 * is NULL, or -ENOMEM if the mapping failed.
	 */
	int scsi_dma_map(struct scsi_cmnd *cmd)
	{
		int nseg = 0;

		if (scsi_sg_count(cmd)) {
			struct device *dev = cmd->device->host->shost_gendev.parent;

			nseg = dma_map_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
				cmd->sc_data_direction);
			if (unlikely(!nseg))
				return -ENOMEM;
		}
		return nseg;
	}

	/**
	 * scsi_dma_unmap - unmap command's sg lists mapped by scsi_dma_map
	 * @cmd:        scsi command
	 */
	void scsi_dma_unmap(struct scsi_cmnd *cmd)
	{
		if (scsi_sg_count(cmd)) {
			struct device *dev = cmd->device->host->shost_gendev.parent;
			dma_unmap_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
				cmd->sc_data_direction);
		}
	}
#endif

#if VMW_NEEDS_SAS_INFO_FUNCTIONS
	static int
	hpsa_get_linkerrors(struct sas_phy *phy)
	{
	    /* dummy function - placeholder */
	    return 0;
	}

	static int
	hpsa_get_enclosure_identifier(struct sas_rphy *rphy, u64 *identifier)
	{
	   /* dummy function - placeholder */
	   return 0;
	}

	static int
	hpsa_get_bay_identifier(struct sas_rphy *rphy)
	{
	   /* dummy function - placeholder */
	   return 0;
	}

	static int
	hpsa_phy_reset(struct sas_phy *phy, int hard_reset)
	{
	   /* dummy function - placeholder */
	   return 0;
	}

	static int
	hpsa_get_initiator_sas_identifier(struct Scsi_Host *sh, u64 *sas_id)
	{
	        struct ctlr_info *ci;
	        int ctlr;
	        ci = (struct ctlr_info *) sh->hostdata[0];
	        if (ci == NULL) {
	        	printk(KERN_INFO "hpsa: hpsa_get_sas_initiator_identifier: "
				"hostdata is NULL for host_no %d\n", sh->host_no);
	        	return -EINVAL;
	        }
	        ctlr = ci->ctlr;
	        if (ctlr > MAX_CTLR || ctlr < 0) {
	                printk(KERN_INFO "hpsa: hpsa_get_sas_initiator_identifier: "
				"MAX_CTLR exceeded.\n");
	                return -EINVAL;
	        }
	        *sas_id = cntl_sas_id[ctlr];

	   return SUCCESS;
	}

	static int
	hpsa_get_target_sas_identifier(struct scsi_target *starget, u64 *sas_id)
	{
	        struct ctlr_info *ci;
	        int ctlr;
	        struct Scsi_Host *host = dev_to_shost(&starget->dev);

	        ci = (struct ctlr_info *) host->hostdata[0];
	        if (ci == NULL) {
	        	printk(KERN_INFO "hpsa: hpsa_get_sas_target_identifier:"
				" hostdata is NULL\n");
		        return -EINVAL;
	        }
	        ctlr = ci->ctlr;
	        if (ctlr > MAX_CTLR || ctlr < 0) {
	                printk(KERN_INFO "hpsa: hpsa_get_sas_target_identifier: "
				"MAX_CTLR exceeded.\n");
	                return -EINVAL;
	        }
	        *sas_id = target_sas_id[ctlr][starget->id];

	   return SUCCESS;
	}

	static struct sas_function_template hpsa_transport_functions = {
		.get_linkerrors			= hpsa_get_linkerrors,
		.get_enclosure_identifier	= hpsa_get_enclosure_identifier,
		.get_bay_identifier		=  hpsa_get_bay_identifier,
		.phy_reset			= hpsa_phy_reset,
		.get_initiator_sas_identifier	= hpsa_get_initiator_sas_identifier,
		.get_target_sas_identifier	= hpsa_get_target_sas_identifier,
	};
#endif

#if VMW_NEEDS_TASK_MANAGEMENT
	/* hpsa_set_tmf_support
	 *      Determine Task Management capabilities of controller
	 *      and remember these in controller info structure so we
	 *      don't have to query controller every time.
	 */
	static void hpsa_set_tmf_support(struct ctlr_info *h)
	{
	        h->TMFSupportFlags = readl(&(h->cfgtable->TMFSupportFlags));
	        if (h->TMFSupportFlags & HPSATMF_BITS_SUPPORTED) {
			hpsa_dbmsg(h, 0, "Task Management function bits"
				" are supported on this controller.\n");

			/* Mask support */
	                if ( h->TMFSupportFlags & HPSATMF_MASK_SUPPORTED)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Task Management Functions MASK\n");

			/* Physical abilities */
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_LUN_RESET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical LUN Reset\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_NEX_RESET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Nexus Reset\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_TASK_ABORT)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Task Aborts\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_TSET_ABORT)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Task Set Abort\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_CLEAR_ACA)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Clear ACA\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_CLEAR_TSET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Clear Task Set\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_TASK)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Query Task\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_TSET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Query Task Set\n");
	                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_ASYNC)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Physical Query Async\n");


			/* Logical abilities */
	                if ( h->TMFSupportFlags & HPSATMF_LOG_LUN_RESET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical LUN Reset\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_NEX_RESET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Nexus Reset\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_TASK_ABORT)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Task Aborts\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_TASK_ABORT)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Task Aborts\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_TSET_ABORT)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Task Set Abort\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_CLEAR_ACA)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Clear ACA\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_CLEAR_TSET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Clear Task Set\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_TASK)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Query Task\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_TSET)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Query Task Set\n");
	                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_ASYNC)
				hpsa_dbmsg(h, 0, "TMF Support for: "
					"Logical Query Async \n");
	        }
	        else
			hpsa_dbmsg(h, 0, "Task Management function bits"
				" are not supported on this controller.\n");
	}
#endif

#if VMW_NEEDS_TIME_REDEFINE
#define time_after64 time_after
#define time_before64 time_before
#endif

/* sles10sp4 apparently doesn't have DIV_ROUND_UP.  Normally it comes
 * from include/linux/kernel.h.  Other sles10's have it I think.
 */
#if !defined(DIV_ROUND_UP)
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

/* Newer dma_mapping_error function takes 2 args, older version only takes 1 arg.
 * This macro makes the code do the right thing depending on which variant we have.
 */
#if KFEATURE_HAS_NEW_DMA_MAPPING_ERROR
#define hpsa_dma_mapping_error(x, y) dma_mapping_error(x, y)
#else
#define hpsa_dma_mapping_error(x, y) dma_mapping_error(y)
#endif

#if !KFEATURE_HAS_ALLOC_WORKQUEUE
/* Earlier implementations had no concpet of WQ_MEM_RECLAIM so far as I know.
 * FIXME: RHEL6 and VMKLNX do not have WQ_MEM_RECLAIM flag. What should we do?
 * WQ_MEM_RECLAIM is supposed to reserve a "rescue thread" so that the work
 * queue can always make forward progress even in low memory situations. E.g.
 * if OS is scramblin for memory and trying to swap out ot disk, it is bad if
 * the thread that is doing the swapping i/o needs to allocate.
 */

#define WQ_MEM_RECLAIM (0)
#if defined(__VMKLNX__)
#define alloc_workqueue(name, flags, max_active) __create_workqueue(name, 0);
#else
#define alloc_workqueue(name, flags, max_active) __create_workqueue(name, flags, max_active, 0)
#endif
#endif

#if !KFEATURE_HAS_ATOMIC_DEC_IF_POSITIVE
static inline int atomic_dec_if_positive(atomic_t *v)
{
	int c, old, dec;
	c = atomic_read(v);
	for (;;) {
		dec = c - 1;
		if (unlikely(dec < 0))
			break;
		old = atomic_cmpxchg((v), c, dec);
		if (likely(old == c))
			break;
		c = old;
	}
	return dec;
}
#endif

/* these next three disappeared in 3.8-rc4 */
#ifndef __devinit
#define __devinit
#endif

#ifndef __devexit
#define __devexit
#endif

#ifndef __devexit_p
#define __devexit_p(x) x
#endif

/* Kernel.org since about July 2013 has nice %XphN formatting for bytes
 * Older kernels don't.  So we have this instead.
 */
#define phnbyte(x, n) ((int) ((x)[(n)]))
#define phN16 "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define phNbytes16(x) \
	phnbyte((x), 0), phnbyte((x), 1), phnbyte((x), 2), phnbyte((x), 3), \
	phnbyte((x), 4), phnbyte((x), 5), phnbyte((x), 6), phnbyte((x), 7), \
	phnbyte((x), 8), phnbyte((x), 9), phnbyte((x), 10), phnbyte((x), 11), \
	phnbyte((x), 12), phnbyte((x), 13), phnbyte((x), 14), phnbyte((x), 15)

