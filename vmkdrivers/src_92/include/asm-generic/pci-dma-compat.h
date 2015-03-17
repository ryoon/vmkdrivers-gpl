/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
/* include this file if the platform implements the dma_ DMA Mapping API
 * and wants to provide the pci_ DMA Mapping API in terms of it */

#ifndef _ASM_GENERIC_PCI_DMA_COMPAT_H
#define _ASM_GENERIC_PCI_DMA_COMPAT_H

#include <linux/dma-mapping.h>

/* note pci_set_dma_mask isn't here, since it's a public function
 * exported from drivers/pci, use dma_supported instead */

#if !defined(__VMKLNX__)
static inline int
pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
	return dma_supported(hwdev == NULL ? NULL : &hwdev->dev, mask);
}
#else /* defined(__VMKLNX__) */
struct vmklnx_codma;
extern struct vmklnx_codma vmklnx_codma;

static inline int
pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
        return vmklnx_dma_supported(&vmklnx_codma,
                        hwdev == NULL ? NULL : &hwdev->dev, mask);
}

static inline int
vmklnx_pci_dma_supported(struct vmklnx_codma *codma, struct pci_dev *hwdev, u64 mask)
{
        return vmklnx_dma_supported(codma,
                        hwdev == NULL ? NULL : &hwdev->dev, mask);
}
#endif /* defined(__VMKLNX__) */

/**                                          
 *  pci_alloc_consistent - allocate and map kernel buffer using consistent mode DMA for PCI device
 *  @hwdev: pci device
 *  @size: size of the buffer
 *  @dma_handle: dma address
 *
 *  Allocate and map kernel buffer using consistent mode DMA for PCI
 *  device.  Returns non-NULL cpu-view pointer to the buffer if
 *  successful and sets *DMA_ADDRP to the pci side dma address as well,
 *  else DMA_ADDRP is undefined.
 *
 *  RETURN VALUE:
 *  non-NULL cpu-view pointer to the buffer if successful, NULL otherwise
 */                                          
/* _VMKLNX_CODECHECK_: pci_alloc_consistent */
static inline void *
pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
		     dma_addr_t *dma_handle)
{
	return dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, dma_handle, GFP_ATOMIC);
}

/**                                          
 *  pci_free_consistent - Free the memory that was set up for DMA for the given device
 *  @hwdev: PCI device that was used in the DMA   
 *  @size: size of the memory being freed   
 *  @vaddr: virtual address of the memory being freed
 *  @dma_handle: bus address of the memory being freed
 *                                           
 *  pci_free_consistent frees the given memory and tears down any special bus
 *  connectivity that was needed to make this memory reachable by the given
 *  device.  If the given device is NULL, the memory is just returned to the
 *  module's memory pool.
 *                                           
 *  ESX Deviation Notes:                     
 *  It is valid on some Linux configurations to call pci_free_consistent on 
 *  a subset region of memory that was part of a larger pci_alloc_consistent
 *  allocation.  This is not valid on ESX.  You must free the entire size
 *  (with the base vaddr and handle) that was obtained with pci_alloc_consistent
 *                                           
 *  RETURN VALUE:
 *  Does not return any value
 */                                          
/* _VMKLNX_CODECHECK_: pci_free_consistent */
static inline void
pci_free_consistent(struct pci_dev *hwdev, size_t size,
		    void *vaddr, dma_addr_t dma_handle)
{
	dma_free_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, vaddr, dma_handle);
}

/**                                          
 *  pci_map_single - Converts the virtual address to physical address.
 *  @hwdev: pointer to PCI device structure
 *  @ptr: virtual address of the memory that needs to be converted
 *  @size: size of the memory
 *  @direction: direction of the DMA operation
 *
 * Converts the virtual address of memory (@ptr) of size (@size) for DMA
 * operation with direction (@direction) on PCI device identified by @hwdev 
 * to its corresponding physical address and returns it.
 *                                           
 *  RETURN VALUE:
 *  virtual address, if successful
 *  NULL, if failure
 */                                          
/* _VMKLNX_CODECHECK_: pci_map_single */
static inline dma_addr_t
pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
	return dma_map_single(hwdev == NULL ? NULL : &hwdev->dev, ptr, size, (enum dma_data_direction)direction);
}

/**
 *  pci_unmap_single - unmap a single memory region
 *  @hwdev: pointer to the PCI device
 *  @dma_addr: the DMA address
 *  @size: the size of the region
 *  @direction: the data direction
 *
 *  Unmap a single memory region previously mapped. All the parameters must be
 *  identical to those passed in (and returned) by the mapping API.
 *
 */
/* _VMKLNX_CODECHECK_: pci_unmap_single */
static inline void
pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
		 size_t size, int direction)
{
	dma_unmap_single(hwdev == NULL ? NULL : &hwdev->dev, dma_addr, size, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_map_page - Map a page region into PCI bus addressable memory
 *  @hwdev: pointer to PCI device structure
 *  @page: pointer to a page structure
 *  @offset: offset in the page where data starts
 *  @size: size of the memory in bytes
 *  @direction: direction of the DMA operation
 *                                           
 *  Map a portion of a page into bus addressable memory, beginning at 
 *  the specified offset, and continuing to the end of the page.
 *
 *  ESX Deviation Notes:
 *  ESX runs exclusively on the x86 architecture, where all main memory
 *  is permanently mapped into PCI bus memory.  Consequently, no actual
 *  mapping is performed (as is also the case in Linux).
 *                                           
 *  RETURN VALUE:
 *  Bus address of the page, if successful
 *  NULL, if failure
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_map_page */
static inline dma_addr_t
pci_map_page(struct pci_dev *hwdev, struct page *page,
	     unsigned long offset, size_t size, int direction)
{
	return dma_map_page(hwdev == NULL ? NULL : &hwdev->dev, page, offset, size, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_unmap_page - remove mapping between pci accessible memory and page
 *  @hwdev: pointer to PCI device structure
 *  @dma_address: the bus address to be unmapped
 *  @size: size of the memory in bytes
 *  @direction: direction of the DMA operation
 *                                           
 *  Unmap page previously mapped into bus addressable memory by pci_map_page.
 *                                           
 *  RETURN VALUE:
 *  NONE
 */                                          
/* _VMKLNX_CODECHECK_: pci_unmap_page */
static inline void
pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address,
	       size_t size, int direction)
{
	dma_unmap_page(hwdev == NULL ? NULL : &hwdev->dev, dma_address, size, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_map_sg - map a scatter-gather into PCI bus addressable memory 
 *  @hwdev: pointer to PCI device structure
 *  @sg: pointer to the scatter-gather list 
 *  @nents: number of scatterlist entries 
 *  @direction: direction of the DMA operation
 *                                           
 *  Map the memory of each element of a scatter-gather list into PCI bus
 *  addressable memory.  The dma_address field of each scatterlist element
 *  is modified with the PCI bus address.
 *
 *  ESX Deviation Notes:
 *  ESX runs exclusively on the x86 architecture, where all main memory
 *  is permanently mapped into PCI bus memory.  Consequently, no actual
 *  mapping is performed (as is also the case in Linux).
 *
 *  RETURN VALUE:
 *  Number of DMA buffers to transfer
 */                                          
/* _VMKLNX_CODECHECK_: pci_map_sg */
static inline int
pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
	   int nents, int direction)
{
	return dma_map_sg(hwdev == NULL ? NULL : &hwdev->dev, sg, nents, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_unmap_sg - unmap a scatter-gather in PCI bus addressable memory 
 *  @hwdev: pointer to PCI device structure
 *  @sg: pointer to the scatter-gather list 
 *  @nents: number of scatterlist entries 
 *  @direction: direction of the DMA operation
 *                                           
 *  Unmap the memory of each element of a scatter-gather list from PCI bus
 *  addressable memory.  
 *
 *  ESX Deviation Notes:
 *  ESX runs exclusively on the x86 architecture, where all main memory
 *  is permanently mapped into PCI bus memory.  Consequently, no actual
 *  unmapping is performed (as is also the case in Linux).
 *
 *  RETURN VALUE:
 *  	None 
 */                                          
/* _VMKLNX_CODECHECK_: pci_unmap_sg */
static inline void
pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
	     int nents, int direction)
{
	dma_unmap_sg(hwdev == NULL ? NULL : &hwdev->dev, sg, nents, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_dma_sync_single_for_cpu - non-operational function
 *  @hwdev: Ignored
 *  @dma_handle: Ignored
 *  @size: Ignored
 *  @direction: Ignored                                        
 *  
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:                     
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_dma_sync_single_for_cpu */
static inline void
pci_dma_sync_single_for_cpu(struct pci_dev *hwdev, dma_addr_t dma_handle,
		    size_t size, int direction)
{
	dma_sync_single_for_cpu(hwdev == NULL ? NULL : &hwdev->dev, dma_handle, size, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_dma_sync_single_for_device - non-operational function
 *  @hwdev: Ignored
 *  @dma_handle: Ignored
 *  @size: Ignored
 *  @direction: Ignored                                        
 *  
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:                     
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_dma_sync_single_for_device */
static inline void
pci_dma_sync_single_for_device(struct pci_dev *hwdev, dma_addr_t dma_handle,
		    size_t size, int direction)
{
	dma_sync_single_for_device(hwdev == NULL ? NULL : &hwdev->dev, dma_handle, size, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_dma_sync_sg_for_cpu - non-operational function
 *  @hwdev: Ignored
 *  @sg: Ignored
 *  @nelems: Ignored
 *  @direction: Ignored                                        
 *  
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:                     
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_dma_sync_sg_for_cpu */
static inline void
pci_dma_sync_sg_for_cpu(struct pci_dev *hwdev, struct scatterlist *sg,
		int nelems, int direction)
{
	dma_sync_sg_for_cpu(hwdev == NULL ? NULL : &hwdev->dev, sg, nelems, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_dma_sync_sg_for_device - non-operational function
 *  @hwdev: Ignored
 *  @sg: Ignored
 *  @nelems: Ignored
 *  @direction: Ignored                                        
 *  
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:                     
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value                                           
 */                                          
/* _VMKLNX_CODECHECK_: pci_dma_sync_sg_for_device */
static inline void
pci_dma_sync_sg_for_device(struct pci_dev *hwdev, struct scatterlist *sg,
		int nelems, int direction)
{
	dma_sync_sg_for_device(hwdev == NULL ? NULL : &hwdev->dev, sg, nelems, (enum dma_data_direction)direction);
}

/**                                          
 *  pci_dma_mapping_error - Identify the errors with the DMA address returned by
 *  PCI DMA mapping functions
 *  @dma_addr: address returned by the PCI DMA mapping function
 *                                           
 * Finds out if there are any errors with the DMA address returned by PCI DMA
 * mapping functions like pci_map_single.
 *                                           
 * Return Value:
 * 0 if success
 * non-zero otherwise
 */                                          
/* _VMKLNX_CODECHECK_: pci_dma_mapping_error */
static inline int
pci_dma_mapping_error(dma_addr_t dma_addr)
{
	return dma_mapping_error(dma_addr);
}

#endif
