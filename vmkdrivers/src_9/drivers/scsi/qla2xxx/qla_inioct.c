/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "exioct.h"
#include "inioct.h"

static int qla2x00_read_option_rom_ext(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_update_option_rom_ext(scsi_qla_host_t *, EXT_IOCTL *, int);
static void qla2x00_get_option_rom_table(scsi_qla_host_t *,
    INT_OPT_ROM_REGION **, unsigned long *);
static int qla81xx_execute_access_data_cmd(scsi_qla_host_t *,
		struct qla_cs84xx_mgmt *);

/* Option ROM definitions. */
INT_OPT_ROM_REGION OptionRomTable2312[] =
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_PHBIOS_FCODE_EFI_CFW, INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION OptionRomTable6312[] = // 128k x20000
{
    {INT_OPT_ROM_REGION_ALL,    INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_PHBIOS_CFW, INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION OptionRomTableHp[] = // 128k x20000
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_PHEFI_PHECFW_PHVPD, INT_OPT_ROM_SIZE_2312,
	    0, INT_OPT_ROM_SIZE_2312-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION  OptionRomTable2322[] = // 1 M x100000
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_2322,
	    0, INT_OPT_ROM_SIZE_2322-1},
    {INT_OPT_ROM_REGION_PHBIOS_PHFCODE_PHEFI_FW, INT_OPT_ROM_SIZE_2322,
	    0, INT_OPT_ROM_SIZE_2322-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION  OptionRomTable6322[] = // 1 M x100000
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_2322,
	    0, INT_OPT_ROM_SIZE_2322-1},
    {INT_OPT_ROM_REGION_PHBIOS_FW, INT_OPT_ROM_SIZE_2322,
	    0, INT_OPT_ROM_SIZE_2322-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION OptionRomTable2422[] = // 1 M x100000
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_2422,
	    0, INT_OPT_ROM_SIZE_2422-1},
    {INT_OPT_ROM_REGION_PHBIOS_PHFCODE_PHEFI, 0x40000,
	    0, 0x40000-1 },
    {INT_OPT_ROM_REGION_NPIV_CONFIG_0, 0x4000,
	    0x58000, 0x5C000-1},
    {INT_OPT_ROM_REGION_NPIV_CONFIG_1, 0x4000,
	    0x5C000, 0x60000-1},
    {INT_OPT_ROM_REGION_FW, 0x80000,
	    0x80000, INT_OPT_ROM_SIZE_2422-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

INT_OPT_ROM_REGION OptionRomTable25XX[] = // 1 M + 64 K  x130000
{
    {INT_OPT_ROM_REGION_ALL, INT_OPT_ROM_SIZE_25XX,
	    0, INT_OPT_ROM_SIZE_25XX-1},
    {INT_OPT_ROM_REGION_PHBIOS_PHFCODE_PHEFI, 0x80000,
	    0, 0x80000-1 },
    {INT_OPT_ROM_REGION_FW, 0x80000,
	    0x80000, INT_OPT_ROM_SIZE_25XX-1},
    {INT_OPT_ROM_REGION_VPD_HBAPARAM, 0x10000,
	    0x120000, 0x130000-1 },
    {INT_OPT_ROM_REGION_FW_DATA, 0x20000,
	    0x100000, 0x120000-1},
    {INT_OPT_ROM_REGION_SERDES, 0x8000,
	    0x148000, 0x150000-1},
    {INT_OPT_ROM_REGION_NPIV_CONFIG_0, 0x4000,
	    0x170000, 0x174000-1},
    {INT_OPT_ROM_REGION_NPIV_CONFIG_1, 0x4000,
	    0x174000, 0x178000-1},
    {INT_OPT_ROM_REGION_NONE, 0, 0, 0 }
};

/* ========================================================================= */
int
qla2x00_read_nvram(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	ret = 0;
	uint32_t transfer_size;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("qla2x00_read_nvram: entered.\n"));

	transfer_size = ha->nvram_size;
	if (pext->ResponseLen < ha->nvram_size)
		transfer_size = pext->ResponseLen;

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ha->nvram, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("qla2x00_read_nvram: exiting.\n"));

	return (ret);
}


/*
 * qla2x00_update_nvram
 *	Write data to NVRAM.
 *
 * Input:
 *	ha = adapter block pointer.
 *	pext = pointer to driver internal IOCTL structure.
 *
 * Returns:
 *
 * Context:
 *	Kernel context.
 */
int
qla2x00_update_nvram(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	uint8_t cnt;
	uint8_t *usr_tmp, *kernel_tmp;
	nvram_t *pnew_nv;
	uint32_t transfer_size;
	unsigned long flags;
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("qla2x00_update_nvram: entered.\n"));

	if (!ha->isp_ops->write_nvram) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
				"%s(%ld): inst=%ld not 24xx or 25xx. exiting.\n",
				__func__, vha->host_no, vha->instance));
		return ret;
	}

	if (pext->RequestLen < ha->nvram_size)
		transfer_size = pext->RequestLen;
	else
		transfer_size = ha->nvram_size;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pnew_nv,
	    ha->nvram_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance, ha->nvram_size));
		return (ret);
	}

	/* Read from user buffer */
	kernel_tmp = (uint8_t *)pnew_nv;
	usr_tmp = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	ret = copy_from_user(kernel_tmp, usr_tmp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "qla2x00_update_nvram: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	/* Checksum NVRAM. */
	if (IS_FWI2_CAPABLE(ha)) {
		uint32_t *iter;
		uint32_t chksum;

		iter = (uint32_t *)pnew_nv;
		chksum = 0;
		for (cnt = 0; cnt < ((ha->nvram_size >> 2) - 1); cnt++)
			chksum += le32_to_cpu(*iter++);
		chksum = ~chksum + 1;
		*iter = cpu_to_le32(chksum);
	} else {
		uint8_t *iter;
		uint8_t chksum;

		iter = (uint8_t *)pnew_nv;
		chksum = 0;
		for (cnt = 0; cnt < ha->nvram_size - 1; cnt++)
			chksum += *iter++;
		chksum = ~chksum + 1;
		*iter = chksum;
	}

	/* Write NVRAM. */
	if (IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
		ret = ha->isp_ops->write_nvram(vha, (uint8_t *)pnew_nv, 
		    ha->nvram_base, transfer_size);
	} else {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		ret = ha->isp_ops->write_nvram(vha, (uint8_t *)pnew_nv, 
		    ha->nvram_base, transfer_size);
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
	}

	if (ret == QLA_MEMORY_ALLOC_FAILED)
		ret = EXT_STATUS_NO_MEMORY;

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("qla2x00_update_nvram: exiting.\n"));

	/* Schedule DPC to restart the RISC */
	set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
	qla2xxx_wake_dpc(vha);

	if (qla2x00_wait_for_chip_reset(vha) != QLA_SUCCESS)
		pext->Status = EXT_STATUS_ERR;

	return ret;
}

int
qla2x00_read_option_rom(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		rval = 0;
	uint8_t		*image_ptr;
	struct qla_hw_data *ha = vha->hw;

	if (pext->SubCode)
		return qla2x00_read_option_rom_ext(vha, pext, mode);

	DEBUG9(printk("%s: entered.\n", __func__));

	/* These interfaces are not valid for 24xx and 25xx chips. */
	if (IS_FWI2_CAPABLE(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		return rval;
	}

	/* The ISP2312 v2 chip cannot access the FLASH registers via MMIO. */
	if (IS_QLA2312(ha) && ha->product_id[3] == 0x2 && !ha->pio_address) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		return rval;
	}

	if (pext->ResponseLen != OPTROM_SIZE_2300) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		return rval;
	}

	image_ptr = vmalloc(OPTROM_SIZE_2300);
	if (image_ptr == NULL) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk(KERN_WARNING
		    "%s: ERROR in flash allocation.\n", __func__));
		return rval;
	}

	memset(image_ptr, 0, OPTROM_SIZE_2300);
 	ha->isp_ops->read_optrom(vha, image_ptr, 0, OPTROM_SIZE_2300);

	/* Copy data to user */
	pext->Status = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	rval = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr,
	    pext->AddrMode), image_ptr, OPTROM_SIZE_2300);
	if (rval) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		vfree(image_ptr);
		return (-EFAULT);
	}

	vfree(image_ptr);
	DEBUG9(printk("%s: exiting.\n", __func__));
	
	return rval;
}

int
qla2x00_read_option_rom_ext(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		iter, found;
	int		rval = 0;
	uint8_t		*image_ptr;
	uint32_t	saddr, length;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s: entered.\n", __func__));

	found = 0;
	saddr = length = 0;

	/* Retrieve region or raw starting address. */
	if (pext->SubCode == 0xFFFF) {
		saddr = pext->Reserved1;
		length = pext->ResponseLen;
		found++;
	} else {
		INT_OPT_ROM_REGION *OptionRomTable = NULL;
		unsigned long OptionRomTableSize;

		/* Pick the right OptionRom table based on device id */
		qla2x00_get_option_rom_table(vha, &OptionRomTable,
		    &OptionRomTableSize);

		for (iter = 0; OptionRomTable != NULL && iter <
		    (OptionRomTableSize / sizeof(INT_OPT_ROM_REGION));
		    iter++) {
			if (OptionRomTable[iter].Region == pext->SubCode) {
				saddr = OptionRomTable[iter].Beg;
				length = OptionRomTable[iter].Size;
				found++;
				break;
			}
		}
	}

	if (!found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		return rval;
	}

	if (pext->ResponseLen < length) {
		pext->Status = EXT_STATUS_COPY_ERR;
		return (-EFAULT);
	}

	image_ptr = vmalloc(length);
	if (image_ptr == NULL) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk(KERN_WARNING
		    "%s: ERROR in flash allocation.\n", __func__));
		return rval;
	}
	memset(image_ptr, 0, length);
	
	/* Dump FLASH. */
 	ha->isp_ops->read_optrom(vha, image_ptr, saddr, length);

	pext->Status = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	rval = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr,
	    pext->AddrMode), image_ptr, length);
	if (rval) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		vfree(image_ptr);
		return (-EFAULT);
	}
	vfree(image_ptr);

	DEBUG9(printk("%s: exiting.\n", __func__));

	return rval;
}

int
qla2x00_update_option_rom(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		rval = 0;

	uint8_t		*usr_tmp;
	uint8_t		*kern_tmp;
	uint16_t	status;
	struct qla_hw_data *ha = vha->hw;

	if (pext->SubCode)
		return qla2x00_update_option_rom_ext(vha, pext, mode);

	DEBUG9(printk("%s: entered.\n", __func__));
	
	/* These interfaces are not valid for 24xx and 25xx chips. */
	if (IS_FWI2_CAPABLE(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		return rval;
	}

	/* The ISP2312 v2 chip cannot access the FLASH registers via MMIO. */
	if (IS_QLA2312(ha) && ha->product_id[3] == 0x2 && !ha->pio_address) {
		DEBUG10(printk("%s: got 2312 and no flash access via mmio.\n",
		    __func__));
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		return rval;
	}

	if (pext->RequestLen != OPTROM_SIZE_2300) {
		DEBUG10(printk("%s: wrong RequestLen=%d, should be %d.\n",
		    __func__, pext->RequestLen, OPTROM_SIZE_2300));
		pext->Status = EXT_STATUS_INVALID_PARAM;
		return rval;
	}

	/* Read from user buffer */
	kern_tmp = vmalloc(OPTROM_SIZE_2300);
	if (kern_tmp == NULL) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG10(printk("%s: vmalloc failed.\n", __func__));
		return rval;
	}
	memset(kern_tmp, 0, OPTROM_SIZE_2300);
	usr_tmp = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	DEBUG9(printk("%s(%ld): going to copy from user.\n",
	    __func__, vha->host_no));

	rval = copy_from_user(kern_tmp, usr_tmp, OPTROM_SIZE_2300);
	if (rval) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n",
		    __func__, Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		vfree(kern_tmp);
		return (-EFAULT);
	}

	DEBUG9(printk("%s(%ld): done copy from user. data dump:\n",
	    __func__, vha->host_no));
	DEBUG9(qla2x00_dump_buffer((uint8_t *)kern_tmp,
	    OPTROM_SIZE_2300));

	/* Go with update */
	status = ha->isp_ops->write_optrom(vha, kern_tmp, 0, OPTROM_SIZE_2300);

	if (status) {
		pext->Status = EXT_STATUS_ERR;
		DEBUG9_10(printk("%s: ERROR updating flash.\n", __func__));
	} else {
		pext->Status = EXT_STATUS_OK;
		pext->DetailStatus = EXT_STATUS_OK;
	}
	vfree(kern_tmp);

	DEBUG9(printk("%s: exiting.\n", __func__));

	return rval;
}

int
qla2x00_update_option_rom_ext(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		iter, found;
	int		ret = 0;

	uint16_t	status;
	uint8_t		*usr_tmp;
	uint8_t		*kern_tmp;
	uint32_t	saddr, length;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s: entered.\n", __func__));

	found = 0;
	saddr = length = 0;
	/* Retrieve region or raw starting address. */
	if (pext->SubCode == 0xFFFF) {
		saddr = pext->Reserved1;
		length = pext->RequestLen;
		found++;
	} else {
		INT_OPT_ROM_REGION *OptionRomTable = NULL;
		unsigned long  OptionRomTableSize;

		/* Pick the right OptionRom table based on device id */
		qla2x00_get_option_rom_table(vha, &OptionRomTable,
		    &OptionRomTableSize);

		for (iter = 0; OptionRomTable != NULL && iter <
		    (OptionRomTableSize / sizeof(INT_OPT_ROM_REGION));
		    iter++) {
			if (OptionRomTable[iter].Region == pext->SubCode) {
				saddr = OptionRomTable[iter].Beg;
				length = OptionRomTable[iter].Size;
				found++;
				break;
			}
		}
	}

	if (!found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		return ret;
	}

	if (pext->RequestLen < length) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		return ret;
	}

	/* Read from user buffer */
	usr_tmp = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	kern_tmp = vmalloc(length);
	if (kern_tmp == NULL) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk(KERN_WARNING
		    "%s: ERROR in flash allocation.\n", __func__));
		return ret;
	}
	memset(kern_tmp, 0, length);

	ret = copy_from_user(kern_tmp, usr_tmp, length);
	if (ret) {
		vfree(kern_tmp);
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", __func__,
		    Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode)));
		return (-EFAULT);
	}

	/* Go with update */
	status = ha->isp_ops->write_optrom(vha, kern_tmp, saddr, length);

	vfree(kern_tmp);
	pext->Status = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	if (status) {
		pext->Status = EXT_STATUS_ERR;
		DEBUG9_10(printk("%s: ERROR updating flash.\n", __func__));
	}


	DEBUG9(printk("%s: exiting.\n", __func__));

	return ret;
}

int
qla2x00_get_vpd(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;

	if (!(IS_FWI2_CAPABLE(ha))) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld not 24xx or 25xx. exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	if (pext->ResponseLen < ha->vpd_size) {
		pext->ResponseLen = ha->vpd_size;
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Response buffer too small.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (IS_NOCACHE_VPD_TYPE(ha)) {
		ha->vpd = ha->nvram + VPD_OFFSET;
		ha->isp_ops->read_optrom(vha, (uint8_t *)ha->vpd, ha->flt_region_vpd,
				ha->vpd_size);
	}

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ha->vpd, ha->vpd_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return (ret);
}

int
qla2x00_update_vpd(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint8_t		*usr_tmp, *kernel_tmp, *pnew_nv;
	uint32_t	data_offset;
	uint32_t	transfer_size;
	struct qla_hw_data *ha = vha->hw;

	if (!(IS_FWI2_CAPABLE(ha)) || !ha->isp_ops->write_nvram ||
			!ha->isp_ops->read_nvram) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld not 24xx or 25xx. exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	transfer_size = FA_NVRAM_VPD_SIZE; /* byte count */
	if (pext->RequestLen < transfer_size)
		transfer_size = pext->RequestLen;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pnew_nv, transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance, transfer_size));
		return (ret);
	}

	DEBUG9(printk("%s(%ld): transfer_size=%d.\n",
	    __func__, vha->host_no, transfer_size));

	/* Read from user buffer */
	kernel_tmp = (uint8_t *)pnew_nv;
	usr_tmp = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	ret = copy_from_user(kernel_tmp, usr_tmp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): ERROR in buffer copy READ. RequestAdr=%p\n",
		    __func__, vha->host_no, Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	if (PCI_FUNC(ha->pdev->devfn))
		data_offset = FA_NVRAM_VPD1_ADDR;
	else
		data_offset = FA_NVRAM_VPD0_ADDR;

	ha->isp_ops->write_nvram(vha, pnew_nv, data_offset, transfer_size);
	ha->isp_ops->read_nvram(vha, ha->vpd, data_offset, FA_NVRAM_VPD_SIZE);

	/* Update the Flash versions 4G and above */
	if(!IS_FWI2_CAPABLE(ha))
		goto done;

	kernel_tmp = vmalloc(256);
	if(!kernel_tmp) {
		DEBUG2(printk(KERN_INFO "%s(%ld) Memory"
			" Allocation failed for flash version update\n", 
			__func__, vha->host_no));
		goto done;
	}

	ret = ha->isp_ops->get_flash_version(vha, kernel_tmp);
	if(ret != QLA_SUCCESS) {
		DEBUG2(printk(KERN_INFO "%s(%ld): Get flash version failed\n",
			__func__, vha->host_no));
	}
	vfree(kernel_tmp);
	/* Force Application to Refresh its info. */
	qla2x00_enqueue_aen(vha, MBA_PORT_UPDATE, NULL);

done:	

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	/* No need to reset the 24xx. */
	return ret;
}

int
qla2x00_get_sfp_data(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint8_t		*ptmp_buf, *ptmp_iter;
	uint32_t	transfer_size;
	uint16_t	iter, addr, offset;
	int		rval;
	struct qla_hw_data *ha = vha->hw;

	if (!IS_FWI2_CAPABLE(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld not 24xx or 25xx. exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	transfer_size = SFP_DEV_SIZE * 2;
	if (pext->ResponseLen < transfer_size) {
		pext->ResponseLen = transfer_size;
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Response buffer too small.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_buf,
	    transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance,
		    ha->nvram_size));
		return (ret);
	}

	ptmp_iter = ptmp_buf;
	addr = 0xa0;
	for (iter = 0, offset = 0; iter < (SFP_DEV_SIZE * 2) / SFP_BLOCK_SIZE;
	    iter++, offset += SFP_BLOCK_SIZE) {
		if (iter == 4) {
			/* Skip to next device address. */
			addr = 0xa2;
			offset = 0;
		}

		rval = qla2x00_read_sfp(vha, ha->sfp_data_dma, addr, offset,
		    SFP_BLOCK_SIZE);
		if (rval != QLA_SUCCESS) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR reading SFP "
			    "data (%x/%x/%x).\n",
			    __func__, vha->host_no, vha->instance, rval, addr,
			    offset));
			qla2x00_free_ioctl_scrap_mem(vha);
			return (-EFAULT);
		}
		memcpy(ptmp_iter, ha->sfp_data, SFP_BLOCK_SIZE);
		ptmp_iter += SFP_BLOCK_SIZE;
	}

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_buf, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return (ret);
}

int
qla2x00_update_port_param(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0, rval, port_found;
	uint16_t	mb[MAILBOX_REGISTER_COUNT];
	uint16_t	idma_speed;
	uint8_t		*usr_temp, *kernel_tmp;
	fc_port_t	*fcport;
	INT_PORT_PARAM	*port_param;
	struct qla_hw_data *ha = vha->hw;

	if (!IS_IIDMA_CAPABLE(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld not 24xx. exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&port_param,
	    sizeof(INT_PORT_PARAM))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%Zd.\n",
		    __func__, vha->host_no, vha->instance,
		    sizeof(INT_PORT_PARAM)));
		return (ret);
	}
	/* Copy request buffer */
	usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode);
	kernel_tmp = (uint8_t *)port_param;
	ret = copy_from_user(kernel_tmp, usr_temp,
	    sizeof(INT_PORT_PARAM));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	if (port_param->FCScsiAddr.DestType != EXT_DEF_TYPE_WWPN) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR -wrong Dest "
		    "type.\n", __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	port_found = 0;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (memcmp(fcport->port_name,
		    port_param->FCScsiAddr.DestAddr.WWPN, WWN_SIZE))
			continue;

		port_found++;
		break;
	}
	if (!port_found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld FC AddrFormat - DID NOT "
		    "FIND Port matching WWPN.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* Go with operation. */
	if (port_param->Mode) {
		switch (port_param->Speed) {
		case EXT_DEF_PORTSPEED_1GBIT:
			idma_speed = PORT_SPEED_1GB;
			break;
		case EXT_DEF_PORTSPEED_2GBIT:
			idma_speed = PORT_SPEED_2GB;
			break;
		case EXT_DEF_PORTSPEED_4GBIT:
			idma_speed = PORT_SPEED_4GB;
			break;
		case EXT_DEF_PORTSPEED_8GBIT:
			idma_speed = PORT_SPEED_8GB;
			break;
		case EXT_DEF_PORTSPEED_10GBIT:
			idma_speed = PORT_SPEED_10GB;
			break;
		default:
			pext->Status = EXT_STATUS_INVALID_PARAM;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR -invalid "
			    "speed.\n", __func__, vha->host_no, vha->instance));
			qla2x00_free_ioctl_scrap_mem(vha);
			return (ret);
		}

		rval = qla2x00_set_idma_speed(vha, fcport->loop_id, idma_speed,
		    mb);
		if (rval != QLA_SUCCESS) {
			if (mb[0] == MBS_COMMAND_ERROR && mb[1] == 0x09)
				pext->Status = EXT_STATUS_DEVICE_NOT_READY;
			else if (mb[0] == MBS_COMMAND_PARAMETER_ERROR)
				pext->Status = EXT_STATUS_INVALID_PARAM;
			else
				pext->Status = EXT_STATUS_ERR;

			DEBUG9_10(printk("%s(%ld): inst=%ld set iDMA cmd "
			    "FAILED=%x.\n", __func__, vha->host_no,
			    vha->instance, mb[0]));
			qla2x00_free_ioctl_scrap_mem(vha);
			return (ret);
		}
	} else {
		rval = qla2x00_get_idma_speed(vha, fcport->loop_id,
		    &idma_speed, mb);
		if (rval != QLA_SUCCESS) {
			if (mb[0] == MBS_COMMAND_ERROR && mb[1] == 0x09)
				pext->Status = EXT_STATUS_DEVICE_NOT_READY;
			else if (mb[0] == MBS_COMMAND_PARAMETER_ERROR)
				pext->Status = EXT_STATUS_INVALID_PARAM;
			else
				pext->Status = EXT_STATUS_ERR;

			DEBUG9_10(printk("%s(%ld): inst=%ld get iDMA cmd "
			    "FAILED=%x.\n", __func__, vha->host_no,
			    vha->instance, mb[0]));
			qla2x00_free_ioctl_scrap_mem(vha);
			return (ret);
		}

		switch (idma_speed) {
		case PORT_SPEED_1GB:
			port_param->Speed = EXT_DEF_PORTSPEED_1GBIT;
			break;
		case PORT_SPEED_2GB:
			port_param->Speed = EXT_DEF_PORTSPEED_2GBIT;
			break;
		case PORT_SPEED_4GB:
			port_param->Speed = EXT_DEF_PORTSPEED_4GBIT;
			break;
		case PORT_SPEED_8GB:
			port_param->Speed = EXT_DEF_PORTSPEED_8GBIT;
			break;
		case PORT_SPEED_10GB:
			port_param->Speed = EXT_DEF_PORTSPEED_10GBIT;
			break;
		default:
			port_param->Speed = 0xFFFF;
			break;
		}

		usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)port_param;
		ret = copy_to_user(usr_temp, kernel_tmp,
		    sizeof(INT_PORT_PARAM));
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk(
			    "%s(%ld): inst=%ld ERROR copy rsp buf ret=%d\n",
			    __func__, vha->host_no, vha->instance, ret));
			qla2x00_free_ioctl_scrap_mem(vha);
			return (-EFAULT);
		}
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return (ret);
}

int
qla2x00_send_loopback(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
#define MAX_LOOPBACK_BUFFER_SIZE	(4 * 1024 * 1024)
#define	MAX_81XX_ECHO_BUFFER_SIZE	(252)
        int             rval = 0;
        int             status;
        uint16_t        ret_mb[MAILBOX_REGISTER_COUNT];
        INT_LOOPBACK_REQ req;
        INT_LOOPBACK_RSP rsp;
	struct qla_hw_data *ha = vha->hw;

        DEBUG9(printk("qla2x00_send_loopback: entered.\n"));


        if (pext->RequestLen != sizeof(INT_LOOPBACK_REQ)) {
                pext->Status = EXT_STATUS_INVALID_PARAM;
                DEBUG9_10(printk(
                    "qla2x00_send_loopback: invalid RequestLen =%d.\n",
                    pext->RequestLen));
                return rval;
        }

        if (pext->ResponseLen != sizeof(INT_LOOPBACK_RSP)) {
                pext->Status = EXT_STATUS_INVALID_PARAM;
                DEBUG9_10(printk(
                    "qla2x00_send_loopback: invalid ResponseLen =%d.\n",
                    pext->ResponseLen));
                return rval;
        }

	status = copy_from_user(&req, Q64BIT_TO_PTR(pext->RequestAdr,
            pext->AddrMode), pext->RequestLen);
        if (status) {
                pext->Status = EXT_STATUS_COPY_ERR;
                DEBUG9_10(printk("qla2x00_send_loopback: ERROR copy read of "
                    "request buffer.\n"));
                return (-EFAULT);
        }

        status = copy_from_user(&rsp, Q64BIT_TO_PTR(pext->ResponseAdr,
            pext->AddrMode), pext->ResponseLen);
        if (status) {
                pext->Status = EXT_STATUS_COPY_ERR;
                DEBUG9_10(printk("qla2x00_send_loopback: ERROR copy read of "
                    "response buffer.\n"));
                return (-EFAULT);
        }

        if (req.TransferCount > MAX_LOOPBACK_BUFFER_SIZE ||
            req.TransferCount > req.BufferLength ||
            req.TransferCount > rsp.BufferLength) {

                /* Buffer lengths not large enough. */
                pext->Status = EXT_STATUS_INVALID_PARAM;
                DEBUG9_10(printk(
                    "qla2x00_send_loopback: invalid TransferCount =%d. "
                    "req BufferLength =%d rspBufferLength =%d.\n",
                    req.TransferCount, req.BufferLength, rsp.BufferLength));

                return rval;
        }

	if (req.TransferCount > vha->ioctl_mem_size) {
                if (qla2x00_get_new_ioctl_dma_mem(vha, req.TransferCount) !=
                    QLA_SUCCESS) {
                        DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
                            "requested DMA buffer size %x.\n",
                            __func__, vha->host_no, vha->instance,
                            req.TransferCount));

                        pext->Status = EXT_STATUS_NO_MEMORY;
                        return rval;
                }
        }

        status = copy_from_user(vha->ioctl_mem, Q64BIT_TO_PTR(req.BufferAddress,
            pext->AddrMode), req.TransferCount);
        if (status) {
                pext->Status = EXT_STATUS_COPY_ERR;
                DEBUG9_10(printk("qla2x00_send_loopback: ERROR copy read of "
                    "user loopback data buffer.\n"));
                return (-EFAULT);
        }


        DEBUG9(printk("qla2x00_send_loopback: req -- bufadr=%lx, buflen=%x, "
            "xfrcnt=%x, rsp -- bufadr=%lx, buflen=%x.\n",
            (unsigned long)req.BufferAddress, req.BufferLength,
            req.TransferCount, (unsigned long)rsp.BufferAddress,
            rsp.BufferLength));

        /*
         * AV - the caller of this IOCTL expects the FW to handle
         * a loopdown situation and return a good status for the
         * call function and a LOOPDOWN status for the test operations
         */
        if (atomic_read(&vha->loop_state) != LOOP_READY ||
            test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
            test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
            vha->dpc_active) {

                pext->Status = EXT_STATUS_BUSY;
                DEBUG9_10(printk("qla2x00_send_loopback(%ld): "
                    "loop not ready.\n", vha->host_no));
                return rval;
        }

	if (ha->current_topology == ISP_CFG_F &&
	    req.Options == OPTION_EXTERNAL_LOOPBACK) {
                if (IS_QLA2100(ha) || IS_QLA2200(ha)) {
                        pext->Status = EXT_STATUS_INVALID_REQUEST ;
                        DEBUG9_10(printk("qla2x00_send_loopback: ERROR "
                            "command only supported for QLA23xx.\n"));
                        return rval;
		} else if (IS_QLA81XX(ha) &&
		    (req.TransferCount > MAX_81XX_ECHO_BUFFER_SIZE)) {

			/* TransferCount should be max 252 for Echo ELS */
			pext->Status = EXT_STATUS_INVALID_PARAM;
			DEBUG9_10(printk(
				"qla2x00_send_loopback: invalid TransferCount =%d. "
				"req BufferLength =%d rspBufferLength =%d.\n",
				req.TransferCount,
				req.BufferLength,
				rsp.BufferLength));

			return rval;
		}

		/* Setting Echo-ELS opcode for ISP81XX */
		if (IS_QLA81XX(ha)) {
			char *ptr = (char *)vha->ioctl_mem;

			ptr[0] = 0x10;
			ptr[1] = 0x00;
			ptr[2] = 0x00;
			ptr[3] = 0x00;
		}
		status = qla2x00_echo_test(vha, &req, ret_mb);
        } else {
                status = qla2x00_loopback_test(vha, &req, ret_mb);
        }

        if (status) {
                if (status == QLA_FUNCTION_TIMEOUT) {
                        pext->Status = EXT_STATUS_BUSY;
                        DEBUG9_10(printk("qla2x00_send_loopback: ERROR "
                            "command timed out.\n"));
                        return rval;
                } else {
                        /* EMPTY. Just proceed to copy back mailbox reg
                         * values for users to interpret.
                         */
			pext->Status = EXT_STATUS_ERR;
                        DEBUG10(printk("qla2x00_send_loopback: ERROR "
                            "loopback command failed 0x%x.\n", ret_mb[0]));
                }
        }

        DEBUG9(printk("qla2x00_send_loopback: loopback mbx cmd ok. "
            "copying data.\n"));

        /* put loopback return data in user buffer */
        status = copy_to_user(Q64BIT_TO_PTR(rsp.BufferAddress,
            pext->AddrMode), vha->ioctl_mem, req.TransferCount);
        if (status) {
                pext->Status = EXT_STATUS_COPY_ERR;
                DEBUG9_10(printk("qla2x00_send_loopback: ERROR copy "
                    "write of return data buffer.\n"));
                return (-EFAULT);
        }

        rsp.CompletionStatus = ret_mb[0];
        if (ha->current_topology == ISP_CFG_F) {
                rsp.CommandSent = INT_DEF_LB_ECHO_CMD;
        } else {
                if (rsp.CompletionStatus == INT_DEF_LB_COMPLETE ||
                    rsp.CompletionStatus == INT_DEF_LB_CMD_ERROR) {
                        rsp.CrcErrorCount = ret_mb[1];
                        rsp.DisparityErrorCount = ret_mb[2];
                        rsp.FrameLengthErrorCount = ret_mb[3];
                        rsp.IterationCountLastError =
                            (ret_mb[19] << 16) | ret_mb[18];
                }
        }

	status = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr,
            pext->AddrMode), &rsp, pext->ResponseLen);
        if (status) {
                pext->Status = EXT_STATUS_COPY_ERR;
                DEBUG9_10(printk("qla2x00_send_loopback: ERROR copy "
                    "write of response buffer.\n"));
                return (-EFAULT);
        }

        pext->Status       = EXT_STATUS_OK;
        pext->DetailStatus = EXT_STATUS_OK;


        DEBUG9(printk("qla2x00_send_loopback: exiting.\n"));
        return rval;
}

int
qla2x00_get_option_rom_layout(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0, iter;
	INT_OPT_ROM_REGION *OptionRomTable = NULL;
	INT_OPT_ROM_LAYOUT *optrom_layout;
	unsigned long	OptionRomTableSize;
#if defined(QL_DEBUG_LEVEL_9) || defined(QL_DEBUG_LEVEL_10)
	struct qla_hw_data *ha = vha->hw;
#endif

	DEBUG9(printk("%s: entered.\n", __func__));

	/* Pick the right OptionRom table based on device id */
	qla2x00_get_option_rom_table(vha, &OptionRomTable, &OptionRomTableSize);

	if (OptionRomTable == NULL) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld) Option Rom Table for device_id=0x%x "
		    "not defined\n", __func__, vha->host_no, ha->pdev->device));
		return ret;
	}

	if (pext->ResponseLen < OptionRomTableSize) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s(%ld) buffer too small: response_len = %d "
		    "optrom_table_len=%ld.\n", __func__, vha->host_no,
		    pext->ResponseLen, OptionRomTableSize));
		return ret;
	}
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&optrom_layout,
	    OptionRomTableSize)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n", __func__, vha->host_no,
		    vha->instance, OptionRomTableSize));
		return ret;
	}

	// Dont Count the NULL Entry.
	optrom_layout->NoOfRegions = (UINT32)
	    (OptionRomTableSize / sizeof(INT_OPT_ROM_REGION) - 1);

	for (iter = 0; iter < optrom_layout->NoOfRegions; iter++) {
		optrom_layout->Region[iter].Region =
		    OptionRomTable[iter].Region;
		optrom_layout->Region[iter].Size =
		    OptionRomTable[iter].Size;
		optrom_layout->Region[iter].Beg =
		    OptionRomTable[iter].Beg;
		optrom_layout->Region[iter].End =
		    OptionRomTable[iter].End;

		if (OptionRomTable[iter].Region == INT_OPT_ROM_REGION_ALL)
			optrom_layout->Size = OptionRomTable[iter].Size;
	}

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    optrom_layout, OptionRomTableSize);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s: exiting.\n", __func__));

	return ret;
}

static void
qla2x00_get_option_rom_table(scsi_qla_host_t *vha,
    INT_OPT_ROM_REGION **pOptionRomTable, unsigned long  *OptionRomTableSize)
{
	struct qla_hw_data *ha = vha->hw;
	DEBUG9(printk("%s: entered.\n", __func__));

	switch (vha->hw->pdev->device) {
	case PCI_DEVICE_ID_QLOGIC_ISP6312:
		*pOptionRomTable = OptionRomTable6312;
		*OptionRomTableSize = sizeof(OptionRomTable6312);
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2312:
		/* HBA Model 6826A - is 2312 V3 Chip */

		if (IS_OEM_002(ha)) {
			*pOptionRomTable = OptionRomTableHp;
			*OptionRomTableSize = sizeof(OptionRomTableHp);
		} else {
			*pOptionRomTable = OptionRomTable2312;
			*OptionRomTableSize = sizeof(OptionRomTable2312);
		}
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2322:
		*pOptionRomTable = OptionRomTable2322;
		*OptionRomTableSize = sizeof(OptionRomTable2322);
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP6322:
		*pOptionRomTable = OptionRomTable6322;
		*OptionRomTableSize = sizeof(OptionRomTable6322);
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2422:
	case PCI_DEVICE_ID_QLOGIC_ISP2432:
	case PCI_DEVICE_ID_QLOGIC_ISP5422:
	case PCI_DEVICE_ID_QLOGIC_ISP5432:
	case PCI_DEVICE_ID_QLOGIC_ISP8432:
		*pOptionRomTable = OptionRomTable2422;
		*OptionRomTableSize = sizeof(OptionRomTable2422);
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2532:
		*pOptionRomTable = OptionRomTable25XX;
		*OptionRomTableSize = sizeof(OptionRomTable25XX);
		break;
	default:
		DEBUG9_10(printk("%s(%ld) Option Rom Table for device_id=0x%x "
		    "not defined\n", __func__, vha->host_no, ha->pdev->device));
		break;
	}

	DEBUG9(printk("%s: exiting.\n", __func__));
}


/*
 * qla84xx_execute_access_data_cmd
 *      Performs the actual IOCB execution for data accesses.
 *
 * Input:
 *
 * Returns:
 *
 * Context:
 *      Kernel context.
 */
static int
qla84xx_execute_access_data_cmd(scsi_qla_host_t *vha,
    struct qla_cs84xx_mgmt *cmd)
{
	int rval = QLA_FUNCTION_FAILED;
	dma_addr_t mn_dma;
	struct a84_mgmt_request  *mn;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	mn = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL, &mn_dma);
	if (mn == NULL) {
		qla_printk(KERN_ERR, ha,
		    "%s(%ld): failed to allocate Access "
		    "CS84 IOCB.\n", __func__, vha->host_no);
		return QLA_MEMORY_ALLOC_FAILED;
	}
	memset(mn, 0, sizeof(struct a84_mgmt_request));
	mn->p.mgmt_request.entry_type     = ACCESS_CHIP_IOCB_TYPE;
	mn->p.mgmt_request.entry_count    = 1;

	mn->p.mgmt_request.options        = cpu_to_le16(cmd->options);
	mn->p.mgmt_request.parameter1     = cpu_to_le32(cmd->parameter1);
	mn->p.mgmt_request.parameter2     = cpu_to_le32(cmd->parameter2);
	mn->p.mgmt_request.parameter3     = cpu_to_le32(cmd->parameter3);
	mn->p.mgmt_request.total_byte_cnt = cpu_to_le32(cmd->data_size);

	DEBUG16(printk("%s(%ld) Input cmd option=%x, data_size=%x "
	    "parameter1=%x parameter2=%x parameter3=%x\n",
	    __func__, vha->host_no, cmd->options,
	    cmd->data_size, cmd->parameter1,
	    cmd->parameter2, cmd->parameter3));

	DEBUG16(printk("%s(%ld): Request for data_size: %d\n", __func__,
	    vha->host_no, cmd->data_size));

	/* if DMA required */
	if (cmd->options != ACO_CHANGE_CONFIG_PARAM) {
		mn->p.mgmt_request.dseg_count     = cpu_to_le16(0x1);
		mn->p.mgmt_request.dseg_address[0] = cpu_to_le32(LSD(cmd->dseg_dma));
		mn->p.mgmt_request.dseg_address[1] = cpu_to_le32(MSD(cmd->dseg_dma));
		mn->p.mgmt_request.dseg_length    = cpu_to_le32(cmd->data_size);
	}

	DEBUG16(printk("%s(%ld): Dump of Access CS84XX IOCB request \n",
	    __func__, vha->host_no));
	DEBUG16(qla2x00_dump_buffer((uint8_t *)mn,
	    sizeof(struct a84_mgmt_request)));

	rval = qla2x00_issue_iocb(vha, mn, mn_dma, 0);
	if (rval != QLA_SUCCESS) {
		DEBUG2_16(printk("%s(%ld): failed to issue Access"
		    "CS84XX IOCB (%x).\n", __func__, vha->host_no, rval));
	} else {
		DEBUG16(printk("%s(%ld): Dump of Access CS84XX IOCB response\n",
		    __func__, vha->host_no));
		DEBUG16(qla2x00_dump_buffer((uint8_t *)mn,
		    sizeof(struct a84_mgmt_request)));

		DEBUG16(printk("scsi(%ld): ql24xx_verify_cs84xx: "
		    "comp_status: %x failure code: %x\n", vha->host_no,
		    le16_to_cpu(mn->p.mgmt_response.comp_status),
		    le16_to_cpu(mn->p.mgmt_response.failure_code)));
		if (mn->p.mgmt_response.comp_status !=
		    __constant_cpu_to_le16(CS_COMPLETE))
			rval = QLA_FUNCTION_FAILED;
	}
	dma_pool_free(ha->s_dma_pool, mn, mn_dma);

	DEBUG9(printk("%s(%ld): rval: %x\n", __func__, vha->host_no, rval));

	return rval;
}

/*
 * qla84xx_access_data
 *      Handles the requests related to data.
 *      Processes following operation
 *		- Read memory
 *		- Write memory
 *		- Change configuration parameters
 *		- Request information
 *
 * Input:
 *
 * Returns:
 *
 * Context:
 *      Kernel context.
 */

static int
qla84xx_access_data(scsi_qla_host_t *vha, SD_A84_MGT *p_mgmt, EXT_IOCTL *pext)
{
	int rval = QLA_SUCCESS;
	int is_read_type_cmd;
	A84_MANAGE_INFO *pMgmtInfo = &p_mgmt->sp.ManageInfo;
	struct qla_cs84xx_mgmt  cs84xx_mgmt;
	int ret;
	struct qla_hw_data *ha = vha->hw;
	/* Set up the command parameters */
	cs84xx_mgmt.options = pMgmtInfo->Operation;

	is_read_type_cmd = pMgmtInfo->Operation == A84_OP_READ_MEM ||
	    pMgmtInfo->Operation == A84_OP_GET_INFO;

	if (pMgmtInfo->Operation == A84_OP_CHANGE_CONFIG) {
		cs84xx_mgmt.data_size = pMgmtInfo->TotalByteCount;
		cs84xx_mgmt.parameter1 =
		    pMgmtInfo->Parameters.ap.Config.ConfigParamID;
		cs84xx_mgmt.parameter2 =
		    pMgmtInfo->Parameters.ap.Config.ConfigParamData0;
		cs84xx_mgmt.parameter3 =
		    pMgmtInfo->Parameters.ap.Config.ConfigParamData1;
	}
	if (pMgmtInfo->Operation == A84_OP_READ_MEM ||
	    pMgmtInfo->Operation ==  A84_OP_WRITE_MEM) {
		cs84xx_mgmt.data_size =
		    pMgmtInfo->TotalByteCount,
		cs84xx_mgmt.parameter1 =
		    pMgmtInfo->Parameters.ap.Memory.StartingAddr;
		cs84xx_mgmt.parameter2 = 0;
		cs84xx_mgmt.parameter3 = 0;
	}
	if (pMgmtInfo->Operation == A84_OP_GET_INFO) {
		cs84xx_mgmt.data_size =
		    pMgmtInfo->TotalByteCount;
		cs84xx_mgmt.parameter1 =
		    pMgmtInfo->Parameters.ap.Info.InfoDataType;
		cs84xx_mgmt.parameter2 =
		    pMgmtInfo->Parameters.ap.Info.InfoContext;
		cs84xx_mgmt.parameter3 = 0;
	}

	cs84xx_mgmt.data = NULL;
	if (cs84xx_mgmt.data_size) {
		cs84xx_mgmt.data = dma_alloc_coherent(&ha->pdev->dev,
		    cs84xx_mgmt.data_size, &cs84xx_mgmt.dseg_dma, GFP_KERNEL);
		if (cs84xx_mgmt.data == NULL) {
			qla_printk(KERN_WARNING, ha,
			   "Unable to allocate memory for CS84XX Mgmt data\n");
			return QLA_FUNCTION_FAILED;
		}
	}

	/* If this is a write and there is some data to be read from user
	   copy in local buffer. For cs84xx change configuration, data size
	   will be zero, so no copy from user involved
	 */
	if (!is_read_type_cmd && cs84xx_mgmt.data_size) {
		/* Copy data from user space pointer */
		ret = copy_from_user(cs84xx_mgmt.data,
			Q64BIT_TO_PTR(pMgmtInfo->pDataBytes, pext->AddrMode),
			cs84xx_mgmt.data_size);
		if (ret) {
			qla_printk(KERN_WARNING, ha,
				"Unable to copy data bytes from user\n");
			rval = QLA_FUNCTION_FAILED;
			goto cs84xx_mgmt_failed;
		}
	}

	if (rval == QLA_SUCCESS) {
		if (IS_QLA84XX(ha))
			rval = qla84xx_execute_access_data_cmd(vha, &cs84xx_mgmt);
		else if (IS_QLA81XX(ha))
			rval = qla81xx_execute_access_data_cmd(vha, &cs84xx_mgmt);
		if (rval != QLA_SUCCESS) {
			printk("Execute access data cmd failed\n");
			goto cs84xx_mgmt_failed;
		}
		if (is_read_type_cmd && cs84xx_mgmt.data_size) {
			ret = copy_to_user(Q64BIT_TO_PTR(
					pMgmtInfo->pDataBytes,
					pext->AddrMode), cs84xx_mgmt.data,
					cs84xx_mgmt.data_size);
			if (ret) {
				qla_printk(KERN_WARNING, ha,
					"Unable to copy data to user\n");
				rval = QLA_FUNCTION_FAILED;
				goto cs84xx_mgmt_failed;
			}
		}
	}

cs84xx_mgmt_failed:
	if (cs84xx_mgmt.data)
		dma_free_coherent(&ha->pdev->dev, cs84xx_mgmt.data_size,
			cs84xx_mgmt.data, cs84xx_mgmt.dseg_dma);

	return rval;
}

/*
 * get f/w version of 84XX
 */
static int
qla84xx_fwversion(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	SD_A84_MGT	*pcs84xx_mgmt;
	uint8_t		*usr_cs84xx_mgmt;
	uint32_t	transfer_size;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));
	transfer_size = pext->RequestLen;
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pcs84xx_mgmt,
	    transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance,
		    transfer_size));
		return (ret);
	}
	usr_cs84xx_mgmt = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	/* Get the paramters from user space */
	ret = copy_from_user(pcs84xx_mgmt, usr_cs84xx_mgmt, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "qla8xxx_cs84xx_mgmt_command: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return ret;
	}

	transfer_size = sizeof(ha->cs84xx->op_fw_version);	/* byte count */
	if (pext->ResponseLen < transfer_size) {
		pext->ResponseLen = transfer_size;
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Response buffer too small.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}
	pcs84xx_mgmt->sp.GetFwVer.FwVersion =  (UINT32) ha->cs84xx->op_fw_version;
	/* Copy back the struct to user */
	usr_cs84xx_mgmt = Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode);
	transfer_size = pext->ResponseLen;
	ret = copy_to_user(usr_cs84xx_mgmt, pcs84xx_mgmt, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return (ret);
}

static int
qla84xx_reset(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	SD_A84_MGT	*pcs84xx_mgmt;
	uint8_t		*usr_cs84xx_mgmt;
	uint32_t	transfer_size;
	A84_RESET 	*pResetInfo;
	int 		cmd;
	uint16_t 	cmd_status;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	transfer_size = pext->RequestLen;
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pcs84xx_mgmt,
	    transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance,
		    transfer_size));
		return (-EFAULT);
	}

	/* Get the paramters from user space */
	usr_cs84xx_mgmt = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);
	ret = copy_from_user(pcs84xx_mgmt, usr_cs84xx_mgmt, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "qla84xx_reset: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* Take action based on the sub command */
	pResetInfo = &pcs84xx_mgmt->sp.Reset;
	cmd = pResetInfo->Flags == A84_RESET_FLAG_ENABLE_DIAG_FW ?
	    A84_ISSUE_RESET_DIAG_FW: A84_ISSUE_RESET_OP_FW;
	ret = qla84xx_reset_chip(vha, cmd == A84_ISSUE_RESET_DIAG_FW,
	    &cmd_status);
	if (ret != QLA_SUCCESS ||
	    cmd_status != MBS_COMMAND_COMPLETE) {
		DEBUG9_10(printk("%s(%ld): ISP8XXX Reset"
		    " command failed ret=%xh cmd_status=%xh\n",
		    __func__, vha->host_no, ret, cmd_status));
		pext->Status = EXT_STATUS_ERR;
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}
	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);
	return (ret);
}

static int
qla84xx_mgmt_control(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	SD_A84_MGT	*pcs84xx_mgmt;
	uint8_t		*usr_cs84xx_mgmt;
	uint32_t	transfer_size;
	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	transfer_size = pext->RequestLen;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pcs84xx_mgmt,
	    transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance,
		    transfer_size));
		return (-EFAULT);
	}

	usr_cs84xx_mgmt = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);

	/* Get the paramters from user space */
	ret = copy_from_user(pcs84xx_mgmt, usr_cs84xx_mgmt, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "qla84xx_mgmt_control: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* Take action based on the sub command */
	ret = qla84xx_access_data(vha, pcs84xx_mgmt, pext);
	if (ret != QLA_SUCCESS) {
		pext->Status = EXT_STATUS_ERR;
		pext->DetailStatus = EXT_STATUS_UNKNOWN;
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);
	return (ret);
}

int
qla84xx_update_chip_fw(scsi_qla_host_t *vha,
    struct qla_cs84xx_mgmt *cs84xx_mgmt, uint8_t is_op_fw,
    uint16_t *comp_status, uint16_t *fail_status)
{
	struct a84_mgmt_request  *mn;
	dma_addr_t mn_dma;
	uint32_t *fw_code;
	uint32_t fw_ver;
	uint16_t options = 0;
	int rval;
	struct qla_hw_data *ha = vha->hw;

	fw_code = (uint32_t *)cs84xx_mgmt->data;
	fw_ver  = le32_to_cpu(fw_code[2]);
	if (fw_ver == 0) {
		DEBUG16(printk("scsi(%ld): Not a valid Cs84XX FW image to flash\n",
		    vha->host_no));
		return QLA_FUNCTION_FAILED;
	}

	mn = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL, &mn_dma);
	if (mn == NULL) {
		qla_printk(KERN_ERR, ha,
		    "%s(%ld): failed to allocate Verify "
		    "Cs84 IOCB.\n", __func__, vha->host_no);
		return QLA_MEMORY_ALLOC_FAILED;
	}

	memset(mn, 0, sizeof(struct a84_mgmt_request));
	options |= VCO_FORCE_UPDATE | VCO_END_OF_DATA;
	if (!is_op_fw)
		options |= VCO_DIAG_FW;

	/* Fill in the IOCB headers */
	mn->p.request.entry_type  = VERIFY_CHIP_IOCB_TYPE;
	mn->p.request.entry_count = 1;
	mn->p.request.options     = cpu_to_le16(options);

	/* Fill in the FW details of the IOCB */
	mn->p.request.fw_ver = cpu_to_le32(fw_ver);
	mn->p.request.fw_size = cpu_to_le32(cs84xx_mgmt->data_size);
	mn->p.request.fw_seq_size = cpu_to_le32(cs84xx_mgmt->data_size);

	mn->p.mgmt_request.dseg_address[0] = cpu_to_le32(LSD(cs84xx_mgmt->dseg_dma));
	mn->p.mgmt_request.dseg_address[1] = cpu_to_le32(MSD(cs84xx_mgmt->dseg_dma));
	mn->p.mgmt_request.dseg_length     = cpu_to_le32(cs84xx_mgmt->data_size);
	mn->p.request.data_seg_cnt = cpu_to_le16(1);

	DEBUG16(printk("%s(%ld): Dump of Verify CS84XX (FW update) IOCB "
	    "request \n", __func__, vha->host_no));
	DEBUG16(qla2x00_dump_buffer((uint8_t *)mn, 
		sizeof(struct a84_mgmt_request)));

	mutex_lock(&ha->cs84xx->fw_update_mutex);
	rval = qla2x00_issue_iocb_timeout(vha, mn, mn_dma, 0, 120);
	if (rval != QLA_SUCCESS) {
		DEBUG2_16(printk("%s(%ld): failed to issue Verify "
		    "CS84XX IOCB (FW update) (%x).\n", __func__,
		    vha->host_no, rval));
		goto fw_update_done;
	}

	DEBUG9_10(printk("%s(%ld): Dump of CS84XX Management "
	    "response\n", __func__, vha->host_no);
		qla2x00_dump_buffer((uint8_t *)mn,
			sizeof(struct a84_mgmt_request)););

	DEBUG16(printk("scsi(%ld): ql24xx_verify_CS84XX: "
	    "comp_status: %x failure code: %x\n", vha->host_no,
	    le16_to_cpu(mn->p.response.comp_status),
	    le16_to_cpu(mn->p.response.failure_code)));

	if (comp_status)
		*comp_status = le16_to_cpu(mn->p.response.comp_status);
	if (fail_status)
		*fail_status = le16_to_cpu(mn->p.response.comp_status) ==
		    CS_TRANSPORT ? le16_to_cpu(mn->p.response.failure_code): 0;

fw_update_done:
	mutex_unlock(&ha->cs84xx->fw_update_mutex);
	dma_pool_free(ha->s_dma_pool, mn, mn_dma);

	DEBUG11(printk("%s(%ld): rval: %x\n", __func__, vha->host_no, rval));

	return (rval);
}

static int
qla84xx_updatefw(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	SD_A84_MGT	*pcs84xx_mgmt;
	A84_UPDATE_FW      *pupdate_fw;
	int cmd;
	uint16_t cmd_status;
	uint16_t fail_code;
	uint8_t *usr_cs84xx_mgmt;
	uint32_t transfer_size;
	struct qla_cs84xx_mgmt cs84xx_mgmt;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	transfer_size = pext->RequestLen;
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pcs84xx_mgmt,
	    transfer_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance,
		    transfer_size));
		return (ret);
	}

	/* Get the parameters from user space */
	usr_cs84xx_mgmt = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);
	ret = copy_from_user(pcs84xx_mgmt, usr_cs84xx_mgmt, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "qla84xx_updatefw: ERROR in buffer copy READ. "
		    "RequestAdr=%p\n", Q64BIT_TO_PTR(pext->RequestAdr,
			    pext->AddrMode)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	pupdate_fw = &pcs84xx_mgmt->sp.UpdateFw;
	cs84xx_mgmt.data_size = pupdate_fw->TotalByteCount;

	/* Allocate memory */
	cs84xx_mgmt.data = dma_alloc_coherent(&ha->pdev->dev,
	    cs84xx_mgmt.data_size, &cs84xx_mgmt.dseg_dma, GFP_KERNEL);
	if (cs84xx_mgmt.data == NULL) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		qla_printk(KERN_WARNING, ha,
		    "Unable to allocate memory for Cs84 Mgmt data\n");
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* Copy the firmware to be updated from user space */
	ret = copy_from_user(cs84xx_mgmt.data,
		Q64BIT_TO_PTR(pupdate_fw->pFwDataBytes, pext->AddrMode),
		cs84xx_mgmt.data_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("qla84xx_updatefw: Copy from user failed\n"));
	}


	if (!ret) {

		cmd = pupdate_fw->Flags == A84_UPDATE_FW_FLAG_DIAG_FW ?
		    A84_ISSUE_UPDATE_DIAGFW_CMD: A84_ISSUE_UPDATE_OPFW_CMD;

		ret = qla84xx_update_chip_fw(vha, &cs84xx_mgmt, cmd ==
			A84_ISSUE_UPDATE_OPFW_CMD, &cmd_status, &fail_code);
		if (ret != QLA_SUCCESS || cmd_status != 0) {
			DEBUG16(printk("%s(%ld): Cs84 update FW failed "
				" ret=%xh cmd_satus=%xh failure_code=%xh\n",
				__func__, vha->host_no, ret,
				cmd_status, fail_code));
			pext->Status = EXT_STATUS_ERR;
			pext->DetailStatus = EXT_STATUS_UNKNOWN;
		}
	}

	if (!ret) {
		pext->Status       = EXT_STATUS_OK;
		pext->DetailStatus = EXT_STATUS_OK;
	}

	/* Free up the memory */
	dma_free_coherent(&ha->pdev->dev, cs84xx_mgmt.data_size,
	    cs84xx_mgmt.data, cs84xx_mgmt.dseg_dma);
	qla2x00_free_ioctl_scrap_mem(vha);

	return (ret);
}

/*
 * qla8xxx_mgmt_command
 *      This is the main entry point for the ISP 8XXX IOCTL path.
 *
 * Input:
 *
 * Returns:
 *
 * Context:
 *      Kernel context.
 */
int
qla84xx_mgmt_command(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;

	if (!IS_QLA84XX(ha) && !IS_QLA81XX(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld not 8xxx exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	/* Take action based on the sub command */
	switch (pext->SubCode) {
	case INT_SC_A84_RESET:
		ret = qla84xx_reset(vha, pext, mode);
		break;
	case INT_SC_A84_GET_FW_VERSION:
		ret = qla84xx_fwversion(vha, pext, mode);
		break;
	case INT_SC_A84_MANAGE_INFO:
		ret = qla84xx_mgmt_control(vha, pext, mode);
		break;
	case INT_SC_A84_UPDATE_FW:
		ret = qla84xx_updatefw(vha, pext, mode);
		break;
	default:
		DEBUG9_10(printk("%s(%ld): inst=%ld Invalid sub command.\n",
		    __func__, vha->host_no, vha->instance));
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		break;
	}
	return (ret);
}


        
int
qla2x00_get_fw_dump(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	ret = 0;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	if (!ha->fw_dumped) {
		pext->Status = EXT_STATUS_HBA_NOT_READY;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld No firmware dump available.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (pext->ResponseLen < ha->fw_dump_len) {
		pext->ResponseLen = ha->fw_dump_len;
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Response buffer too small.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	ret = copy_to_user((void *)(pext->ResponseAdr), ha->fw_dump,
	    ha->fw_dump_len);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	printk(KERN_INFO
	    "scsi(%ld): Firmware dump cleared.\n", vha->host_no);
	ha->fw_dumped = 0;

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	pext->ResponseLen = ha->fw_dump_len;

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return (ret);
}

int qla81xx_mpi_reset(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	ret = 0;
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	if (ha->flags.npiv_supported) {
		if (!vha->vp_idx) {
			pext->Status = EXT_STATUS_INVALID_REQUEST;
			DEBUG10(printk("%s(%ld): MPI reset for vport not supported.\n",
						__func__, vha->host_no));
			return (-EFAULT);
		}
	}
	ret = qla81xx_restart_mpi_firmware(vha, mb);
	if (ret != QLA_SUCCESS) {
		DEBUG9_10(printk("%s(%ld): inst=%ld MPI restart cmd "
					"FAILED=%x.\n",
					__func__, vha->host_no, vha->instance, mb[0]));
		pext->Status = EXT_STATUS_ERR;
		return (QLA_FUNCTION_FAILED);
	} else if (mb[0] == MBS_COMMAND_ERROR) {
		DEBUG9_10(printk("%s(%ld): inst=%ld MPI restart cmd "
					"FAILED=%x.  MPI is executing out of ROM code\n",
					__func__, vha->host_no, vha->instance, mb[0]));
		if (mb[1] == MPI_SCODE_FW_IMAGE_ERROR)
			DEBUG9_10(printk("MPI FW image error\n"));
		else if (mb[1] == MPI_SCODE_FW_AUTO_LOAD_CFG_ERROR)
			DEBUG9_10(printk("MPI FW auto load configuration error\n"));
		pext->Status = EXT_STATUS_ERR;
		return (QLA_FUNCTION_FAILED);
	}

	pext->Status = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): exiting.\n", __func__, vha->host_no));

	return ret;
}

/*
 * qla2xxx_reset_fw_command
 *
 * Input:
 *
 * Returns:
 *
 * Context:
 *      Kernel context.
 */
int
qla2xxx_reset_fw_command(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int ret = 0;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	/* Take action based on the sub command */
	switch (pext->SubCode) {
		case INT_SC_RESET_FC_FW:
			printk(KERN_INFO "scsi(%ld): Issuing ISP abort on host.\n",
					vha->host_no);
			/* Block any further I/Os being queued */
			scsi_block_requests(vha->host);

			if (vha->vp_idx) {
				/* Wait for all outstanding cmds to complete */
				qla2x00_eh_wait_for_vp_pending_commands(vha);
				qla2x00_vp_abort_isp(vha);
			} else {
				/* Wait for all outstanding cmds to complete */
				qla2x00_eh_wait_for_pending_commands(vha);
				set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
				qla2xxx_wake_dpc(vha);
				if (qla2x00_wait_for_chip_reset(vha) != QLA_SUCCESS)
					pext->Status = EXT_STATUS_ERR;
			}

			scsi_unblock_requests(vha->host);
			break;

		case INT_SC_RESET_MPI_FW:
			if (!IS_QLA81XX(vha->hw) || vha->vp_idx) {
				DEBUG9_10(printk(
						"%s(%ld): inst=%ld MPI reset not supported.\n",
						__func__, vha->host_no, vha->instance));
				pext->Status = EXT_STATUS_INVALID_REQUEST;
				return QLA_FUNCTION_FAILED;
			}
			printk(KERN_INFO "scsi(%ld): Issuing MPI reset on host.\n",
					vha->host_no);
			/* Make sure FC side is not in reset */
			if (qla2x00_wait_for_chip_reset(vha) != QLA_SUCCESS) {
				pext->Status = EXT_STATUS_ERR;
				return QLA_FUNCTION_FAILED;
			}

			/* Block any further I/Os being queued */
			scsi_block_requests(vha->host);
			/* Wait for all outstanding cmds to complete */
			qla2x00_eh_wait_for_pending_commands(vha);

			ret = qla81xx_mpi_reset(vha, pext, mode);

			scsi_unblock_requests(vha->host);
			break;

		default:
			DEBUG9_10(printk("%s(%ld): inst=%ld Invalid sub command.\n",
				__func__, vha->host_no, vha->instance));
			pext->Status = EXT_STATUS_INVALID_REQUEST;
			break;
	}

	return (ret);
}

int qla2xxx_fcp_prio_cfg_valid(struct qla_fcp_prio_cfg *pri_cfg, uint8_t flag)
{
	int i, ret = QLA_FUNCTION_FAILED, num_valid;
	uint8_t *bcode;
	struct qla_fcp_prio_entry *pri_entry;

	num_valid = 0;
	bcode = (uint8_t *)pri_cfg;

	if (bcode[0x0] != 'H' || bcode[0x1] != 'Q' || bcode[0x2] != 'O' ||
	    bcode[0x3] != 'S')
		return QLA_SUCCESS;

	if (flag != 1)
		return ret;

	pri_entry = &pri_cfg->entry[0];
	for (i = 0; i < pri_cfg->num_entries; i++) {
		if (pri_entry->flags & (FCP_PRIO_ENTRY_VALID |
					FCP_PRIO_ENTRY_TAG_VALID))
			num_valid++;
		pri_entry++;
	}

	if (pri_cfg->num_entries && num_valid == 0)
		ret = QLA_SUCCESS;

	return ret;
}

int qla2xxx_fcp_prio_cfg_cmd(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int ret = QLA_SUCCESS;
	uint8_t *usr_tmp, *kernel_tmp;
	uint32_t transfer_size = 0;
	struct qla_hw_data *ha = vha->hw;

	if (test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &vha->dpc_flags)) {
		pext->Status = EXT_STATUS_BUSY;
		return (-EBUSY);
	}

	/* Only set config is allowed if config memory is not allocated */
	if (!vha->fcp_prio_cfg && (pext->SubCode != INT_SC_FCP_PRIO_SET_CONFIG)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		return (-EINVAL);
	}

	/* Take action based on the sub command */
	switch (pext->SubCode) {
		case INT_SC_FCP_PRIO_DISABLE:
			if (vha->flags.fcp_prio_enabled) {
				vha->flags.fcp_prio_enabled = 0;
				vha->fcp_prio_cfg->attributes &= ~FCP_PRIO_ATTR_ENABLE;
				qla2xxx_update_all_fcp_prio(vha);
				pext->Status = EXT_STATUS_OK;
			}
			break;

		case INT_SC_FCP_PRIO_ENABLE:
			if (!vha->flags.fcp_prio_enabled) {
				if (vha->fcp_prio_cfg) {
					vha->flags.fcp_prio_enabled = 1;
					vha->fcp_prio_cfg->attributes |= FCP_PRIO_ATTR_ENABLE;
					qla2xxx_update_all_fcp_prio(vha);
					pext->Status = EXT_STATUS_OK;
				} else {
					pext->Status = EXT_STATUS_INVALID_REQUEST;
					return (-EINVAL);
				}
			}
			break;

		case INT_SC_FCP_PRIO_GET_CONFIG:
			if (!pext->ResponseLen || pext->ResponseLen > FCP_PRIO_CFG_SIZE) {
				pext->Status = EXT_STATUS_INVALID_PARAM;
				return (-EINVAL);
			}

			transfer_size = pext->ResponseLen < FCP_PRIO_CFG_SIZE ?
				pext->ResponseLen : FCP_PRIO_CFG_SIZE;

			ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr,
						pext->AddrMode), vha->fcp_prio_cfg, transfer_size);
			if (ret) {
				DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
							__func__, vha->host_no, vha->instance));
				pext->Status = EXT_STATUS_COPY_ERR;
				return (-EFAULT);
			}

			pext->Status = EXT_STATUS_OK;
			break;

		case INT_SC_FCP_PRIO_SET_CONFIG:
			if (!pext->RequestLen || pext->RequestLen > FCP_PRIO_CFG_SIZE) {
				pext->Status = EXT_STATUS_INVALID_PARAM;
				return (-EINVAL);
			}

			if (!vha->fcp_prio_cfg) {
				vha->fcp_prio_cfg = vmalloc(FCP_PRIO_CFG_SIZE);
				if (!vha->fcp_prio_cfg) {
					qla_printk(KERN_WARNING, ha,
							"Unable to allocate memory for fcp prio "
							"config data (%x).\n", FCP_PRIO_CFG_SIZE);
					pext->Status = EXT_STATUS_NO_MEMORY;
					return (-ENOMEM);
				}
			}
			memset(vha->fcp_prio_cfg, 0, FCP_PRIO_CFG_SIZE);

			/* Read from user buffer */
			kernel_tmp = (uint8_t *)vha->fcp_prio_cfg;
			usr_tmp = Q64BIT_TO_PTR(pext->RequestAdr, pext->AddrMode);
			transfer_size = pext->RequestLen < FCP_PRIO_CFG_SIZE ?
				pext->RequestLen : FCP_PRIO_CFG_SIZE;

			ret = copy_from_user(kernel_tmp, usr_tmp, transfer_size);
			if (ret) {
				DEBUG9_10(printk("%s: ERROR in buffer copy READ. "
						"RequestAdr=%p\n", __func__,
						Q64BIT_TO_PTR(pext->RequestAdr,	pext->AddrMode)));
				pext->Status = EXT_STATUS_COPY_ERR;
				return (-EFAULT);
			}

			/* validate fcp priority data */
			if (!qla2xxx_fcp_prio_cfg_valid(vha->fcp_prio_cfg, 1)) {
				pext->Status = EXT_STATUS_INVALID_PARAM;
				return (-EINVAL);
			}

			vha->flags.fcp_prio_enabled = 0;
			if (vha->fcp_prio_cfg->attributes & FCP_PRIO_ATTR_ENABLE)
				vha->flags.fcp_prio_enabled = 1;
			qla2xxx_update_all_fcp_prio(vha);
			pext->Status = EXT_STATUS_OK;
			break;

		default:
			pext->Status = EXT_STATUS_INVALID_REQUEST;
			return (-EINVAL);
	}

	return QLA_SUCCESS;
}

/*
 * qla2xxx_get_board_temp
 *
 * Input: None
 *
 * Returns: Board temperature
 *
 * Context:
 *      Kernel context.
 */
int
qla2xxx_get_board_temp(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int rval = QLA_FUNCTION_FAILED;
	EXT_BOARD_TEMP	*ptmp_board_temp = NULL;
	uint32_t transfer_size;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, ha->host_no, ha->instance));

	if (!vha->flags.thermal_supported) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;

		return QLA_SUCCESS;
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_board_temp,
	    sizeof(EXT_BOARD_TEMP))) {
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_BOARD_TEMP)));

		return rval;
	}

	rval = qla2x00_get_thermal_temp(vha, &ptmp_board_temp->IntTemp,
			&ptmp_board_temp->FracTemp);
	if (rval) {
		vha->flags.thermal_supported = 0;
		pext->Status = EXT_STATUS_ERR;
		DEBUG9_10(printk("%s(%ld): Error in acquiring Board Temperature: %d",
				__func__, vha->host_no, rval));
		qla2x00_free_ioctl_scrap_mem(vha);

		return rval;
	}

	/* now copy up the BOARD_TEMP to user */
	if (pext->ResponseLen < sizeof(EXT_BOARD_TEMP))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_BOARD_TEMP);

	rval = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_board_temp, transfer_size);
	if (rval) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, rval));
		qla2x00_free_ioctl_scrap_mem(vha);

		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
			__func__, vha->host_no, vha->instance));

	qla2x00_free_ioctl_scrap_mem(vha);

	return rval;
}

/*
 * qla81xx_execute_access_data_cmd
 *      Performs the actual execution for data accesses.
 * - XGMAC MB Cmd only for now
 *
 * Input:
 *
 *
 * Returns:
 *
 * Context:
 *      Kernel context.
 */
static int
qla81xx_execute_access_data_cmd(scsi_qla_host_t *vha,
		struct qla_cs84xx_mgmt *cmd)
{
	int rval = QLA_FUNCTION_FAILED;

	DEBUG9(printk("%s(%ld): entered.\n", __func__, vha->host_no));

	rval = qla81xx_get_xgmac_stats(vha, cmd->dseg_dma, cmd->data_size);

	if (rval != QLA_SUCCESS) {
		DEBUG2_16(printk("%s(%ld): failed to issue Get XGMAC"
				"Stats MB Cmd (%x).\n", __func__, vha->host_no, rval));
	}

	DEBUG9(printk("%s(%ld): rval: %x\n", __func__, vha->host_no, rval));

	return rval;
}

