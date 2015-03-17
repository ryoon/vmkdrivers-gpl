/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef _X8664_DMA_MAPPING_H
#define _X8664_DMA_MAPPING_H 1

/*
 * IOMMU interface. See Documentation/DMA-mapping.txt and DMA-API.txt for
 * documentation.
 */


#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/swiotlb.h>

struct dma_mapping_ops {
	int             (*mapping_error)(dma_addr_t dma_addr);
	void*           (*alloc_coherent)(struct device *dev, size_t size,
                                dma_addr_t *dma_handle, gfp_t gfp);
	void            (*free_coherent)(struct device *dev, size_t size,
                                void *vaddr, dma_addr_t dma_handle);
	dma_addr_t      (*map_single)(struct device *hwdev, void *ptr,
                                size_t size, int direction);
	/* like map_single, but doesn't check the device mask */
	dma_addr_t      (*map_simple)(struct device *hwdev, char *ptr,
                                size_t size, int direction);
	void            (*unmap_single)(struct device *dev, dma_addr_t addr,
		                size_t size, int direction);
	void            (*sync_single_for_cpu)(struct device *hwdev,
		                dma_addr_t dma_handle, size_t size,
				int direction);
	void            (*sync_single_for_device)(struct device *hwdev,
                                dma_addr_t dma_handle, size_t size,
				int direction);
	void            (*sync_single_range_for_cpu)(struct device *hwdev,
                                dma_addr_t dma_handle, unsigned long offset,
		                size_t size, int direction);
	void            (*sync_single_range_for_device)(struct device *hwdev,
				dma_addr_t dma_handle, unsigned long offset,
		                size_t size, int direction);
	void            (*sync_sg_for_cpu)(struct device *hwdev,
                                struct scatterlist *sg, int nelems,
				int direction);
	void            (*sync_sg_for_device)(struct device *hwdev,
				struct scatterlist *sg, int nelems,
				int direction);
	int             (*map_sg)(struct device *hwdev, struct scatterlist *sg,
		                int nents, int direction);
	void            (*unmap_sg)(struct device *hwdev,
				struct scatterlist *sg, int nents,
				int direction);
	int             (*dma_supported)(struct device *hwdev, u64 mask);
	int		is_phys;
};

extern dma_addr_t bad_dma_address;
extern struct dma_mapping_ops* dma_ops;
extern int iommu_merge;

static inline int valid_dma_direction(int dma_direction)
{
	return ((dma_direction == DMA_BIDIRECTIONAL) ||
		(dma_direction == DMA_TO_DEVICE) ||
		(dma_direction == DMA_FROM_DEVICE));
}

extern void *dma_alloc_coherent(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t gfp);
extern void dma_free_coherent(struct device *dev, size_t size, void *vaddr,
			      dma_addr_t dma_handle);



#if defined(__VMKLNX__)

/*
 * DMA mapping functions on vmklinux are not inlined so as to 
 * support further revision and improvements for the behavior of
 * of stable third-party binary drivers using these functions.
 */

extern int dma_mapping_error(dma_addr_t dma_addr);

#define dma_unmap_page(dev,dma_address,size,dir) \
	dma_unmap_single(dev,dma_address,size,dir)

extern dma_addr_t
dma_map_single(struct device *hwdev, void *ptr, size_t size,
	       int direction);

extern dma_addr_t
dma_map_page(struct device *hwdev, struct page *page, unsigned long offset,
             size_t size, int direction);

extern void
dma_unmap_single(struct device *dev, dma_addr_t addr,size_t size,
		 int direction);
extern void
dma_sync_single_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			size_t size, int direction);
extern void
dma_sync_single_for_device(struct device *hwdev, dma_addr_t dma_handle,
			   size_t size, int direction);
extern void
dma_sync_single_range_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			      unsigned long offset, size_t size, int direction);
extern void
dma_sync_single_range_for_device(struct device *hwdev, dma_addr_t dma_handle,
				 unsigned long offset, size_t size, int direction);
extern void
dma_sync_sg_for_cpu(struct device *hwdev, struct scatterlist *sg,
		    int nelems, int direction);
extern void
dma_sync_sg_for_device(struct device *hwdev, struct scatterlist *sg,
		       int nelems, int direction);
extern int
dma_map_sg(struct device *hwdev, struct scatterlist *sg, int nents, int direction);
extern void
dma_unmap_sg(struct device *hwdev, struct scatterlist *sg, int nents,
	     int direction);

struct vmklnx_codma;
extern struct vmklnx_codma vmklnx_codma;
extern int vmklnx_dma_supported(struct vmklnx_codma *codma,
                                struct device *hwdev, u64 mask);

static inline int dma_supported(struct device *hwdev, u64 mask)
{
        return vmklnx_dma_supported(&vmklnx_codma, hwdev, mask);
}

#else /* !defined(__VMKLNX__) */

/**                                          
 *  dma_mapping_error - Check a bus address for a mapping error       
 *  @dma_addr: bus address previously returned by dma_map_single or dma_map_page
 *                                           
 *  Performs a platform-specific check to determine if the
 *  mapped bus address is valid for use with DMA
 *                                           
 *  RETURN VALUE:
 *  TRUE if the bus address incurred a mapping error, FALSE otherwise
 *
 *  SEE ALSO:
 *  dma_map_single
 *                                           
 */                                          
static inline int dma_mapping_error(dma_addr_t dma_addr)
{
	if (dma_ops->mapping_error)
		return dma_ops->mapping_error(dma_addr);

	return (dma_addr == bad_dma_address);
}

/**                                          
 *  dma_map_single - Map a buffer for streaming DMA use with a given device       
 *  @hwdev: device to be used in the DMA operation    
 *  @ptr: virtual address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the DMA to set up
 *                                           
 *  Sets up any platform-specific bus connectivity required to
 *  make a buffer usable for a DMA operation and returns a mapped bus address
 *  for the buffer.  The mapped address should be checked for an error using 
 *  dma_mapping_error.  When the buffer will no longer be used for DMA, the 
 *  buffer should be unmapped using dma_unmap_single.
 *  'direction' can be any one of
 *  DMA_BIDIRECTIONAL (the device either reads or writes the buffer),
 *  DMA_TO_DEVICE (the device reads the buffer), 
 *  DMA_FROM_DEVICE (the device writes the buffer), or
 *  DMA_NONE (neither reads nor writes should be allowed - may not be supported
 *  on all platforms)
 *
 *  RETURN VALUE:
 *  A bus address accessible by the device
 *
 *  SEE ALSO:
 *  dma_unmap_single, dma_mapping_error
 *                                           
 */                                          
static inline dma_addr_t
dma_map_single(struct device *hwdev, void *ptr, size_t size,
	       int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	return dma_ops->map_single(hwdev, ptr, size, direction);
}

#define dma_map_page(dev,page,offset,size,dir) \
	dma_map_single((dev), page_address(page)+(offset), (size), (dir))

#define dma_unmap_page dma_unmap_single

/**                                          
 *  dma_unmap_single - Tear down a streaming DMA mapping for a buffer       
 *  @dev: device that had been used in the DMA operation     
 *  @addr: mapped bus address for the buffer, previously returned by dma_map_single
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the DMA that was set up by dma_map_single
 *                                           
 *  Tears down the platform-specific bus connectivity that was needed to make a 
 *  buffer usable for DMA.  
 *  'direction' can be any one of
 *  DMA_BIDIRECTIONAL (the device either reads or writes the buffer),
 *  DMA_TO_DEVICE (the device reads the buffer), 
 *  DMA_FROM_DEVICE (the device writes the buffer), or
 *  DMA_NONE (neither reads nor writes should be allowed)
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 *  SEE ALSO:
 *  dma_map_single
 *                                           
 */                                          
static inline void
dma_unmap_single(struct device *dev, dma_addr_t addr,size_t size,
		 int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	dma_ops->unmap_single(dev, addr, size, direction);
}

/**                                          
 *  dma_sync_single_for_cpu - Allow the CPU to access a buffer that is currently DMA-mapped      
 *  @hwdev: device to which the buffer is mapped    
 *  @dma_handle: bus address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the existing DMA mapping
 *
 *  Transfers access ownership for a buffer that has been set up for DMA back to
 *  the CPU and synchronizes any changes that have been made by the device with
 *  the CPU.  The bus mapping that was created with dma_map_single is not 
 *  destroyed.  Afterward, the CPU can safely read and write the buffer.  The
 *  device should not access the buffer until access rights have been 
 *  transferred back to the device using dma_sync_single_for_device.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_sync_single_for_device, dma_map_single
 *                                           
 */                                          
static inline void
dma_sync_single_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			size_t size, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_single_for_cpu)
		dma_ops->sync_single_for_cpu(hwdev, dma_handle, size,
					     direction);
	flush_write_buffers();
}

/**                                          
 *  dma_sync_single_for_device - Re-enable device access to a DMA-mapped buffer     
 *  @hwdev: device to which the buffer is mapped    
 *  @dma_handle: bus address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the existing DMA mapping
 *                                           
 *  Transfers access ownership back to a device from the CPU and synchronizes 
 *  any changes that the CPU has made so that they will be visible by the device.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_sync_single_for_cpu, dma_map_single
 *                                           
 */                                          
static inline void
dma_sync_single_for_device(struct device *hwdev, dma_addr_t dma_handle,
			   size_t size, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_single_for_device)
		dma_ops->sync_single_for_device(hwdev, dma_handle, size,
						direction);
	flush_write_buffers();
}

static inline void
dma_sync_single_range_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			      unsigned long offset, size_t size, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_single_range_for_cpu) {
		dma_ops->sync_single_range_for_cpu(hwdev, dma_handle, offset, size, direction);
	}

	flush_write_buffers();
}

static inline void
dma_sync_single_range_for_device(struct device *hwdev, dma_addr_t dma_handle,
				 unsigned long offset, size_t size, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_single_range_for_device)
		dma_ops->sync_single_range_for_device(hwdev, dma_handle,
						      offset, size, direction);

	flush_write_buffers();
}

static inline void
dma_sync_sg_for_cpu(struct device *hwdev, struct scatterlist *sg,
		    int nelems, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_sg_for_cpu)
		dma_ops->sync_sg_for_cpu(hwdev, sg, nelems, direction);
	flush_write_buffers();
}

static inline void
dma_sync_sg_for_device(struct device *hwdev, struct scatterlist *sg,
		       int nelems, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	if (dma_ops->sync_sg_for_device) {
		dma_ops->sync_sg_for_device(hwdev, sg, nelems, direction);
	}

	flush_write_buffers();
}

/**                                          
 *  dma_map_sg - Map scatter/gather buffers for DMA use with a hardware device  
 *  @hwdev: device to be used in the DMA operations    
 *  @sg: start of the scatter/gather list of entries to be mapped
 *  @nents: number of elements in the list to be mapped
 *  @direction: direction of the DMA, with values as in dma_map_single
 *                                           
 *  Sets up the platform-specific bus connectivity for each of the buffers in a 
 *  scatterlist so that they may be used in DMA with the given hardware device.
 *  dma_unmap_sg should be used on these scatterlist elements when they will no
 *  longer be used with DMA.
 *                                           
 *  RETURN VALUE:
 *  0 if a failure was encountered, nents if the mappings succeeded
 *
 *  SEE ALSO:
 *  dma_map_single, dma_unmap_sg
 *                                           
 */                                          
static inline int
dma_map_sg(struct device *hwdev, struct scatterlist *sg, int nents, int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	return dma_ops->map_sg(hwdev, sg, nents, direction);
}

/**                                          
 *  dma_unmap_sg - Unmap scatter/gather buffers that were previously mapped for DMA
 *  @hwdev: device to which these buffers have been mapped    
 *  @sg: start of the scatter/gather list of entries to be unmapped
 *  @nents: number of elements in the list to be unmapped
 *  @direction: direction of the existing DMA mapping
 *                                           
 *  Tears down the platform-specific bus connectivity for each of the buffers in 
 *  a scatterlist that had been previously set up for DMA using
 *  dma_map_sg.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_map_sg, dma_map_single  
 *
 */                                          
static inline void
dma_unmap_sg(struct device *hwdev, struct scatterlist *sg, int nents,
	     int direction)
{
	BUG_ON(!valid_dma_direction(direction));
	/* take out this ifdef block when we have iommu support */
	dma_ops->unmap_sg(hwdev, sg, nents, direction);
}

extern int dma_supported(struct device *hwdev, u64 mask);

#endif /* !defined(__VMKLNX__) */

/* same for gart, swiotlb, and nommu */
static inline int dma_get_cache_alignment(void)
{
	return boot_cpu_data.x86_clflush_size;
}

#define dma_is_consistent(h) 1

extern int dma_set_mask(struct device *dev, u64 mask);

static inline void
dma_cache_sync(void *vaddr, size_t size, enum dma_data_direction dir)
{
	flush_write_buffers();
}

extern struct device fallback_dev;
extern int panic_on_overflow;

#endif /* _X8664_DMA_MAPPING_H */
