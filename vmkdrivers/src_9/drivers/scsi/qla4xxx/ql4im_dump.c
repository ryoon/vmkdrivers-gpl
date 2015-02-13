/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2007 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/klist.h>
#include <asm/semaphore.h>
#include "ql4_def.h"
#include "ql4im_def.h"
#include "ql4im_dump.h"
#include <scsi/scsi_dbg.h>

static void ql4_dump_header(struct scsi_qla_host *ha, struct dump_image_header *hdr)
{
	extern char drvr_ver[];

	ENTER_IOCTL(__func__, ha->host_no);

	memset(hdr, 0, sizeof(struct dump_image_header));

	hdr->cookie = QLGC_COOKIE;
	sprintf((char *)&hdr->dump_id_string,"%4x Dump", ha->pdev->device);
	hdr->time_stamp = get_jiffies_64();
	hdr->total_image_size  = DUMP_IMAGE_SIZE;
	hdr->core_dump_offset  = CORE_DUMP_OFFSET;
	hdr->probe_dump_offset = PROBE_DUMP_OFFSET;
	sprintf((char *)&hdr->driver,"qla4xxx_%d v%s", ha->instance, drvr_ver);
	sprintf((char *)&hdr->ioctlmod,"qisioctl_%d %s",
		    ha->instance, QL4IM_VERSION);

	LEAVE_IOCTL(__func__, ha->host_no);
}

/************************************************************
 *
 *                  Core Dump Routines
 *
 ************************************************************/

/*
 * Perform a Write operation via the MADI registers
 */
static int qla4_write_MADI(struct scsi_qla_host *ha, uint32_t addr,
				uint32_t data)
{
	int done = 0;
	int count = 10000;
	unsigned long flags;
	int status = QLA_SUCCESS;
	
	spin_lock_irqsave(&ha->hardware_lock, flags);

	if (((readl(&ha->reg->arc_madi_cmd) & MADI_STAT_MASK) >> 27) ==
		MADI_STAT_COMMAND_BUSY) {
		status = QLA_ERROR;
		goto exit_write_MADI;
	}

	writel(addr, &ha->reg->arc_madi_cmd);
	writel(data, &ha->reg->arc_madi_data);

	while (!done && count--) {
		switch ((readl(&ha->reg->arc_madi_cmd) & MADI_STAT_MASK) >> 27) {
		case MADI_STAT_DATA_VALID:
			done = 1;
			break;

		case MADI_STAT_DATA_INVALID:
			writel(addr, &ha->reg->arc_madi_cmd);
			writel(data, &ha->reg->arc_madi_data);
			break;

		default:
			break;
		}
	}
	if (!count)
		status = QLA_ERROR;

exit_write_MADI:
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
	return status;
}

/*
 * Perform a Read operation via the MADI registers
 */
static int qla4_read_MADI(struct scsi_qla_host *ha, uint32_t addr,
			uint32_t *data)
{
	int done = 0;
	int count = 10000;
	unsigned long flags;
	int status = QLA_SUCCESS;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	writel((MADI_READ_CMD | addr), &ha->reg->arc_madi_cmd);

	while (!done && count--) {
		switch ((readl(&ha->reg->arc_madi_cmd) & MADI_STAT_MASK) >> 27){
		case MADI_STAT_DATA_VALID:
			done = 1;
			break;

		case MADI_STAT_DATA_INVALID:
			writel((MADI_READ_CMD | addr), &ha->reg->arc_madi_cmd);
			break;

		default:
			break;
		}
	}

	if (!count)
		status = QLA_ERROR;

	*data = readl(&ha->reg->arc_madi_data);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	return status;
}

static void
ql4_dump_core(struct scsi_qla_host *ha, struct core_dump *core_dump)
{
	uint32_t		addr, data, rval;
	volatile uint32_t	 __iomem *reg_ptr;
	unsigned long		flags;
	uint8_t			page, num_pci_pages;

	ENTER_IOCTL(__func__, ha->host_no);

	/* 1 - Select OAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000000) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 1 - Select OAP Processor Failed1\n", __func__,
			(int)ha->host_no));
		goto core_dump_exit;
	}

	/* 2 - Halt the OAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_AUX_REG | 0x00000005, 0x00000002) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 2 - Halt OAP Processor Failed\n", __func__,
			(int)ha->host_no));
		goto core_dump_exit;
	}

	/* 3 - Disable SRAM Parity */
	if (qla4_write_MADI(ha, MADI_DEST_AUX_REG | 0x00000020, 0x00000001) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 3 - Disable SRAM Parity Failed\n", __func__,
			(int)ha->host_no));
		goto core_dump_exit;
	}

	/* 4 - Select IAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000001) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 4 - Select IAP Processor Failed\n", __func__,
			(int)ha->host_no));
		goto core_dump_exit;
	}

	/* 5 - Halt IAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_AUX_REG | 0x00000005, 0x00000002) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 5 - Halt IAP Processor Failed\n",
			__func__, (int)ha->host_no));
		goto core_dump_exit;
	}

	/* 6 - Select OAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000000) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 6 - Select OAP Processor Failed\n",
			__func__, (int)ha->host_no));
		goto core_dump_exit;
	}

	/* 7 - PCI Registers from Processor's Perspective */
	for (addr = (PCI_START >> 2); (addr <= (PCI_END >> 2)) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD) , &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 7 - PCI Registers "
				"from Processor's Perspective Failed\n",
				__func__, (int)ha->host_no));
			DEBUG2(printk("%s(%d): PCIReg 0 addr = 0x%08x Failed0\n",
			__func__, (int)ha->host_no, (addr << 2)));
			goto core_dump_exit;
		}
		core_dump->PCIRegProc[(addr & (0xFFC >> 2))] = data;
	}
	DEBUG10(printk("%s(%d): 7 - PCI Registers from Processor's Perspective:\n",
			__func__, (int)ha->host_no));


	/* 8 - SRAM Content */
	for (addr = (RAM_START >> 2); (addr <= (RAM_END >> 2)) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD) , &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 8 - SRAM Content Failed,  addr = 0x%08x\n",
				__func__, (int)ha->host_no, (addr << 2)));
			goto core_dump_exit;
		}
		core_dump->SRAM[addr] = data;
	}
	DEBUG10(printk("%s(%d): 8 - SRAM Content:\n", __func__, (int)ha->host_no));


	/* 9 - OAP Core Registers */
	for (addr = CORE_START; (addr <= CORE_END) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD | MADI_DEST_CORE_REG) , &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 9 - OAP Core Reg Failed, addr = 0x%08x\n",
                                __func__, (int)ha->host_no, (addr)));
			goto core_dump_exit;
		}
		core_dump->OAPCoreReg[addr] = data;
	}
	DEBUG10(printk("%s(%d): 9 - OAP Core Register:\n", __func__, (int)ha->host_no));


	/* 10 - OAP Auxiliary Registers */
	for (addr = 0; (addr <= 0x309) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD | MADI_DEST_AUX_REG) , &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 10 - OAP Aux Reg Failed, "
				"addr = 0x%08x\n", __func__, (int)ha->host_no, (addr)));
			goto core_dump_exit;
		}
		core_dump->OAPAuxReg[addr] = data;
	}
	DEBUG10(printk("%s(%d): 10 - OAP Auxiliary Registers:\n", __func__, (int)ha->host_no));


	/* 11 - Select IAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000001) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 11 - Select IAP Processor Failed\n",
			__func__, (int)ha->host_no));
		goto core_dump_exit;
	}

	/* 12 - IAP Core Registers */
	for (addr = 0; (addr <= 0x3F) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD | MADI_DEST_CORE_REG), &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 12 - IAP Core Reg Failed, "
				"addr = 0x%08x \n", __func__, (int)ha->host_no, addr));
			goto core_dump_exit;
		}
		core_dump->IAPCoreReg[addr] = data;
	}
	DEBUG10(printk("%s(%d): 12 - IAP Core Registers:\n", __func__, (int)ha->host_no));


	/* 13 - IAP Auxiliary Registers */
	for (addr = 0; (addr <= 0x309) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD | MADI_DEST_AUX_REG), &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 13 - IAP Aux Reg Failed, "
				"addr = 0x%08x\n", __func__, (int)ha->host_no, (addr)));
			goto core_dump_exit;
		}
		core_dump->IAPAuxReg[addr] = data;
	}
	DEBUG10(printk("%s(%d): 13 - IAP Auxiliary Registers:\n", __func__, (int)ha->host_no));

	/* 14 - Save IAP load/store RAM */
	for (addr = (LDST_START >> 2); (addr <= (LDST_END >> 2)) ; addr++) {
		rval = qla4_read_MADI(ha, (addr | MADI_READ_CMD), &data);
		if (rval != QLA_SUCCESS) {
			DEBUG2(printk("%s(%d): 14 - IAP SRAM Content Failed, "
				"addr = 0x%08x\n", __func__, (int)ha->host_no, (addr << 2)));
			goto core_dump_exit;
		}
		core_dump->IAPSRAM[(addr & (0x1FFC >> 2))] = data;
	}
	DEBUG10(printk("%s(%d): 14 - IAP Load/Store RAM\n", __func__, (int)ha->host_no));


	/* 15 - Select OAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000000) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 15 - Select OAP Processor Failed3\n",
			__func__, (int)ha->host_no));
		goto core_dump_exit;
	}

	/* 16 - Save Host PCI Registers */
	if (is_qla4010(ha))
                num_pci_pages = 4;
	else
                num_pci_pages = 3;

	for (page = 0; page < num_pci_pages; page++) {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		writel((clr_rmask(CSR_SCSI_PAGE_SELECT) | page), &ha->reg->ctrl_status);
		reg_ptr = &ha->reg->mailbox[0];
		for (addr = 0; addr < 64; addr++) {
			core_dump->HostPCIRegPage[page][addr] = readl(reg_ptr);
			reg_ptr++;
		}
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		DEBUG10(printk("%s(%d): 16 - Host PCI Registers Page %d:\n",
			__func__, (int)ha->host_no, page));
	}

	/* 17 - Save statistics registers */
	if (is_qla4010(ha)) {
		/* Statistics registers were saved from page 3 registers above */
	}
	else {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		writel(clr_rmask(CSR_SCSI_PAGE_SELECT), &ha->reg->ctrl_status);
		writel(0, &ha->reg->u2.isp4022.p0.stats_index);
		for (addr = 0; addr < 64; addr++) {
			data = readl(&ha->reg->u2.isp4022.p0.stats_read_data_inc);
			core_dump->HostPCIRegPage[PROT_STAT_PAGE][addr] = data;
		}
		DEBUG10(printk("%s(%d): 17 - Statistics Registers:\n",
			__func__, (int)ha->host_no));
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
	}

	/* Select OAP Processor */
	if (qla4_write_MADI(ha, MADI_DEST_ARC_DEBUG, 0x00000000) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 18 - Select OAP Processor Failed\n",
			__func__, (int)ha->host_no));
		goto core_dump_exit;
	}
	/* Enable SRAM Parity */
	if (qla4_write_MADI(ha, MADI_DEST_AUX_REG | 0x00000020, 0x00000000) != QLA_SUCCESS) {
		DEBUG2(printk("%s(%d): 19 - Enable SRAM Parity Failed\n",
			__func__, (int)ha->host_no));
	}

core_dump_exit:
	LEAVE_IOCTL(__func__, ha->host_no);
}


/************************************************************
 *
 *                  Probe Dump Routines
 *
 ************************************************************/
//
// 4010 ProbeMux table
//
static PROBEMUX_INFO  probeModuleInfo4010[] = {
     {"0"        , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"DA"       , CLK_BIT(SYSCLK) | CLK_BIT(PCICLK)    , MUX_SELECT_MAX}
   , {"NRM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ODE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SRM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SCM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"NCM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"PRD"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RBM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"TDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RA"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ERM"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"RMI"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"OAP"      ,                   CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"ECM"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"NPF"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"IAP"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"OTP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"TTM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ITP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"MAM"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"BLM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ILM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IFP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IPV"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"OIP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"OFB"      , CLK_BIT(SYSCLK) | CLK_BIT(NRXCLK)    , MUX_SELECT_MAX}
   , {"MAC"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IFB"      , CLK_BIT(SYSCLK) | CLK_BIT(NRXCLK)    , MUX_SELECT_MAX}
   , {"PCORE"    , CLK_BIT(PCICLK)                      , MUX_SELECT_MAX}
   , {"20"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"21"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"22"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"23"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"24"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"25"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"26"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"27"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"28"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"29"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2A"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2B"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2C"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2D"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2E"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2F"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"30"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"31"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"32"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"33"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"34"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"35"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"36"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"37"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"38"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"39"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3a"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3b"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3c"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3d"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3e"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3f"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
};

//
// 4022/4032 ProbeMux table
//

static PROBEMUX_INFO  probeModuleInfo4022[] = {
     {"0"        , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"DA"       , CLK_BIT(SYSCLK) | CLK_BIT(PCICLK)    , MUX_SELECT_MAX}
   , {"BPM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ODE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SRM0"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SRM1"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"PMD"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"PRD"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RMD"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"TDE"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RA"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"REG"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RMI"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"OAP"      ,                   CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"ECM"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"NPF"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"IAP"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"OTP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"TTM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ITP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"MAM"      , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"BLM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ILM"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IFP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IPV"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"OIP"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"OFB"      , CLK_BIT(SYSCLK) | CLK_BIT(NRXCLK)    , MUX_SELECT_MAX}
   , {"MAC"      , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"IFB"      , CLK_BIT(SYSCLK) | CLK_BIT(NRXCLK)    , MUX_SELECT_MAX}
   , {"PCORE"    , CLK_BIT(PCICLK)                      , MUX_SELECT_MAX}
   , {"NRM0"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"NRM1"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SCM0"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"SCM1"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"NCM0"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"NCM1"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RBM0"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RBM1"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RBM2"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"RBM3"     , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"ERM0"     , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"ERM1"     , CLK_BIT(SYSCLK) | CLK_BIT(CPUCLK)    , MUX_SELECT_MAX}
   , {"PERF0"    , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"PERF1"    , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2E"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"2F"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"30"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"31"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"32"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"33"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"34"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"35"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"36"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"37"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"38"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"39"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3a"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3b"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3c"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3d"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3e"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
   , {"3f"       , CLK_BIT(SYSCLK)                      , MUX_SELECT_MAX}
};

static void
ql4_probe_dump(struct scsi_qla_host *ha, struct probe_dump *probe_dump)
{
	uint32_t   probeModule;
	uint32_t   probeClock;
	uint32_t   muxSelect;
	uint32_t   oldPage;
	unsigned long	flags;
	uint32_t   probeAddr;
	struct probe_data *pd;

	PROBEMUX_INFO  *probeModuleInfo;
	
	ENTER_IOCTL(__func__, ha->host_no);

	probeModuleInfo = (is_qla4010(ha)) ? probeModuleInfo4010 : probeModuleInfo4022;
	pd = (struct probe_data *)probe_dump;

	for (probeModule = probe_DA; probeModule <= probe_PERF1; probeModule++) {
		for (probeClock = 0; probeClock < 4; probeClock++) {
			if (probeModuleInfo[probeModule].clocks & (1 << probeClock)) {
				probeAddr = (probeModule << 8) | (probeClock << 6);
				for (muxSelect = 0;
					muxSelect < probeModuleInfo[probeModule].maxSelect;
					muxSelect++) {
					spin_lock_irqsave(&ha->hardware_lock, flags);
					oldPage = readl(&ha->reg->ctrl_status) & 0x0003;
					writel(0x00030000, &ha->reg->ctrl_status);  // Set to page 0
	
					writel((u_long)(probeAddr | PROBE_RE | PROBE_UP | muxSelect),
						isp_probe_mux_addr(ha));
					pd->high = (readl(isp_probe_mux_data(ha)) >> 24) & 0xff;

					writel((u_long)(probeAddr | PROBE_RE | PROBE_LO | muxSelect),
						isp_probe_mux_addr(ha));
					pd->low = readl(isp_probe_mux_data(ha));

					writel((u_long)(0x00030000 | oldPage), &ha->reg->ctrl_status); // Reset page
					spin_unlock_irqrestore(&ha->hardware_lock, flags);
					pd++;
				}
			}
		}
	}
	LEAVE_IOCTL(__func__, ha->host_no);
}

void ql4_core_dump(struct scsi_qla_host *ha, void *pdump)
{
	struct dump_image *image = (struct dump_image *)pdump;

	ENTER_IOCTL(__func__, ha->host_no);

	ql4_dump_header(ha, &image->dump_header);
	ql4_dump_core(ha, &image->core_dump);
	ql4_probe_dump(ha, &image->probe_dump);

	LEAVE_IOCTL(__func__, ha->host_no);
}
