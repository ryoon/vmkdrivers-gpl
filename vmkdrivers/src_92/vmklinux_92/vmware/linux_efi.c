/* ****************************************************************
 * Portions Copyright 2003, 2007-2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include "vmkapi.h"
#include "linux/efi.h"
#include "linux/module.h"

/*
 * Indicates if EFI is enabled in the system.
 */
int efi_enabled;
EXPORT_SYMBOL(efi_enabled);

/*
 * EFI information
 */
struct efi efi = {
.smbios     = EFI_INVALID_TABLE_ADDR,
};
EXPORT_SYMBOL(efi);

/*
 * This function initializes the vmklinux EFI state by querying the
 * vmkernel for the presence of EFI and the address of the SMBIOS
 * entry point.
 */
void LinuxEFI_Init(void)
{
   void *smbios;

   if (vmk_EFIIsPresent() == VMK_TRUE) {
      if (vmk_EFIGetSMBIOSEntryPoint(&smbios) == VMK_OK) {
         efi_enabled = 0x1;
         efi.smbios = (unsigned long)smbios;
      }
   }
}

