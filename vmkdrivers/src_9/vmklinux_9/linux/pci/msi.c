/* ****************************************************************
 * Portions Copyright 2006 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <linux/pci.h>

#include "vmkapi.h"
#include "linux_pci.h"

extern VMK_ReturnStatus LinuxPCI_EnableMSI(struct pci_dev *dev);
VMK_ReturnStatus LinuxPCI_EnableMSIX(struct pci_dev* dev, struct msix_entry *entris, int nvecs, vmk_Bool bestEffort, int *nvecs_alloced);
extern void LinuxPCI_DisableMSI(struct pci_dev *dev);
extern void LinuxPCI_DisableMSIX(struct pci_dev *dev);

/**                                          
 *  pci_enable_msi - configure device's MSI capability structure
 *  @dev: pointer to the pci_dev data structure of MSI device function
 *                                           
 * Setup the MSI capability structure of device function with
 * a single MSI vector. This function is invoked upon its software driver call
 * to request for MSI mode enabled on its hardware device function.
 *
 *  Return Value:
 *    0 for successful setup of an entry zero with the new MSI vector
 *    -EINVAL otherwise
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_enable_msi */
int
pci_enable_msi(struct pci_dev *dev)
{
   printk("Enabling MSI for dev %s\n", pci_name(dev));
   return (LinuxPCI_EnableMSI(dev) == VMK_OK)? 0 : -EINVAL;
}
#if defined(__VMKLNX__)
EXPORT_SYMBOL(pci_enable_msi);
#endif /* defined(__VMKLNX__) */

/**                                          
 *  pci_disable_msi - Remove device's MSI capability structure
 *  @dev: pointer to the pci_dev data structure of MSI device function
 *
 * Disable the MSI mode of the PCI device.
 *
 *  Return Value:
 *  Does not return any value
 */                                          
/* _VMKLNX_CODECHECK_: pci_disable_msi */
void
pci_disable_msi(struct pci_dev *dev)
{
   if (!dev || !dev->msi_enabled)
      return;

   printk("Disabling MSI for dev %s\n", pci_name(dev));
   return LinuxPCI_DisableMSI(dev);
}
#if defined(__VMKLNX__)
EXPORT_SYMBOL(pci_disable_msi);
#endif /* defined(__VMKLNX__) */

/**
 * pci_enable_msix - configure device's MSI-X capability structure
 * @dev: pointer to the pci_dev data structure of MSI-X device function
 * @entries: pointer to an array of MSI-X entries
 * @nvec: number of MSI-X vectors requested for allocation by device driver
 *
 * Setup the MSI-X capability structure of device function with the number
 * of requested vectors. This function is invoked upon its software driver call
 * to request for MSI-X mode enabled on its hardware device function.
 *
 * Return Value:
 * 0 indicates the successful configuration of MSI-X capability structure
 * with new allocated MSI-X vectors.
 * < 0 indicates a failure.
 * > 0 indicates that driver request is exceeding the number of vectors 
 * available. Driver should use the returned value to re-send its request.
 **/

/* _VMKLNX_CODECHECK_: pci_enable_msix */
int
pci_enable_msix(struct pci_dev* dev,
                struct msix_entry *entries,
                int nvec)
{
   VMK_ReturnStatus status;
   int nvec_allocated = 0;

   status = LinuxPCI_EnableMSIX(dev, entries, nvec, VMK_FALSE, &nvec_allocated);
   if (status == VMK_OK) {
      printk("MSIX enabled for dev %s\n", pci_name(dev));
      return 0;
   } else if (status == VMK_NO_RESOURCES && nvec_allocated > 0) {
         return nvec_allocated;
   }
   
   return -EINVAL;
}
#if defined(__VMKLNX__)
EXPORT_SYMBOL(pci_enable_msix);
#endif /* defined(__VMKLNX__) */

/**                                          
 *  pci_disable_msix - Disable the MSIX capability of a PCI device
 *  @dev:  pointer to the pci_dev data structure of MSIX device function
 *                                           
 *  Disables the MSIX capability of a PCI device. Frees all interrupt vectors
 *  associated with MSIX capability of the PCI device.
 *                                           
 *  Return Value:
 *  Does not return any value
 */                                          
/* _VMKLNX_CODECHECK_: pci_disable_msix */
void pci_disable_msix(struct pci_dev* dev)
{
   if (!dev || !dev->msix_enabled)
      return;

   printk("Disabling MSIX for dev %s\n", pci_name(dev));
   LinuxPCI_DisableMSIX(dev);
}
#if defined(__VMKLNX__)
EXPORT_SYMBOL(pci_disable_msix);
#endif /* defined(__VMKLNX__) */
