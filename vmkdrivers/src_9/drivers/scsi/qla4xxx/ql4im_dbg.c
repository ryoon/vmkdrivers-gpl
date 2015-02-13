/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
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

void dprt_hba_iscsi_portal(PEXT_HBA_ISCSI_PORTAL port)
{
	printk("\tIPAddr\t\t%d.%d.%d.%d\n", 
		port->IPAddr.IPAddress[0],
		port->IPAddr.IPAddress[1],
		port->IPAddr.IPAddress[2],
		port->IPAddr.IPAddress[3]);

	printk("\tMACAddr\t\t%02x.%02x.%02x.%02x.%02x.%02x\n",
		port->MacAddr[0],
		port->MacAddr[1],
		port->MacAddr[2],
		port->MacAddr[3],
		port->MacAddr[4],
		port->MacAddr[5]);
	printk("\tSerialNum\t0x%x\n", port->SerialNum);
	printk("\tManufacturer\t%s\n", port->Manufacturer);
	printk("\tModel\t\t%s\n", port->Model);
	printk("\tDriverVersion\t%s\n", port->DriverVersion);
	printk("\tFWVersion\t%s\n", port->FWVersion);
	printk("\tOptRomVersion\t%s\n", port->OptRomVersion);
	printk("\tState\t\t0x%x\n", port->State);
	printk("\tType\t\t0x%x\n", port->Type);
	printk("\tOptRomVersion\t0x%x\n", port->DriverAttr);
	printk("\tFWAttr\t\t0x%x\n", port->FWAttr);
	printk("\tDiscTargetCount\t0x%x\n", port->DiscTargetCount);
}

void dprt_chip_info(PEXT_CHIP_INFO pcinfo)
{
	printk("\tVendorId\t0x%x\n", pcinfo->VendorId);
	printk("\tDeviceId\t0x%x\n", pcinfo->DeviceId);
	printk("\tSubVendorId\t0x%x\n", pcinfo->SubVendorId);
	printk("\tSubSystemId\t0x%x\n", pcinfo->SubSystemId);
	printk("\tBoardID\t0x%x\n", pcinfo->BoardID);
}

void dprt_rw_flash(int rd, uint32_t offset, uint32_t len, uint32_t options)
{
	printk("\tDataLen 0x%08x", len);
	if (!rd)
		printk(" Options 0x%08x", options);
	printk(" DataOffset 0x%08x", offset);
	if (offset & INT_ISCSI_ACCESS_RAM)
		printk(" RAM");
	else
		printk(" FLASH");
	switch ((offset & INT_ISCSI_PAGE_MASK)) {
		case INT_ISCSI_FW_IMAGE2_FLASH_OFFSET: 
		printk(" FW Image 2\n");
		break;
		case INT_ISCSI_SYSINFO_FLASH_OFFSET: 
		printk(" sysInfo\n");
		break;
		case INT_ISCSI_DRIVER_FLASH_OFFSET: 
		printk(" driver\n");
		break;
		case INT_ISCSI_INITFW_FLASH_OFFSET: 
		printk(" initfw\n");
		break;
		case INT_ISCSI_DDB_FLASH_OFFSET: 
		printk(" ddb\n");
		break;
		case INT_ISCSI_CHAP_FLASH_OFFSET: 
		printk(" CHAP\n");
		break;
		case INT_ISCSI_FW_IMAGE1_FLASH_OFFSET: 
		printk(" FW Image 1\n");
		break;
		case INT_ISCSI_BIOS_FLASH_OFFSET: 
		printk(" BIOS\n");
		break;
		default:
		printk(" Illegal 0x%x\n",
			(offset & INT_ISCSI_PAGE_MASK));
		break;
	}
}

void ql4_dump_buffer(unsigned char *buf, uint32_t len)
{
        uint32_t i;
	uint32_t j, k;

        for (i=0; i < len; i+=16) {
                printk("0x%08x:", i);
		k = len - i;
		if (k >= 16) k = 16;
		for (j = 0; j < k; j++) printk(" %02x", buf[i+j]);
		printk("\n");
        }
}


