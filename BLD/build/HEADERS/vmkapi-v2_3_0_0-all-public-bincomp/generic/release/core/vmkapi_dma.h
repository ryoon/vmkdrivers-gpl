/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * DMA Address Space Management                                   */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup DMA DMA Address Space Management
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_DMA_H_
#define _VMKAPI_DMA_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief An opaque handle representing the DMA information related
 *        to a device.
 */
typedef struct vmk_DMADeviceInt *vmk_DMADevice;

#define VMK_DMA_DEVICE_INVALID   ((vmk_DMADevice)NULL)

/**
 * \brief An opaque handle representing a DMA engine on a device.
 */
typedef struct vmk_DMAEngineInt *vmk_DMAEngine;

#define VMK_DMA_ENGINE_INVALID   ((vmk_DMAEngine)NULL)

/**
 * \brief A description of the address, size, and other
 *        constraints on a DMA engine.
 */
typedef struct vmk_DMAConstraints {
   /**
    * \brief Address bits that are valid for the DMA device.
    *
    * \note Address masks must be contiguous. "Holes" in an address
    *       mask are not supported.
    *
    * \note This field may not be set to zero.
    */
   vmk_IOA addressMask;

   /**
    * \brief Maximum total DMA transfer size in bytes.
    *
    * Set to 0 if there is no maximum transfer limit.
    */
   vmk_ByteCount maxTransfer;

   /**
    * \brief Maximum number of SG array entries for a DMA transfer.
    *
    * Set to 0 if there is no maximum number of SG array entries limit.
    */
   vmk_uint32 sgMaxEntries;

   /**
    * \brief Maximum size of a SG array element.
    *
    * Must be at least VMK_PAGE_SIZE.
    *
    * Set to 0 if this constraint does not apply.
    */
   vmk_ByteCountSmall sgElemMaxSize;

   /**
    *\brief  The SG elements in an array except the last element
    *        must be a multiple of this number of bytes.
    *
    * Must be a power of 2 and less than VMK_PAGE_SIZE
    *
    * Set to 0 if this constraint does not apply.
    */
   vmk_ByteCountSmall sgElemSizeMult;

   /**
    * \brief  SG elements must be aligned to this number of bytes.
    *
    * Must be a power of 2 and less than or equal to VMK_PAGE_SIZE
    *
    * Set to 0 or 1 if this constraint does not apply.
    */
   vmk_ByteCountSmall sgElemAlignment;

   /**
    * \brief SG elements cannot straddle multiple regions of this size.
    *
    * Must be a power of 2.
    *
    * Must be at least VMK_PAGE_SIZE.
    *
    * Set to 0 if this constraint does not apply.
    */
   vmk_IOA sgElemStraddle;
} vmk_DMAConstraints;

/**
 * \brief Type of DMA bounce buffer pool.
 */
typedef enum vmk_DMABouncePoolType {
   VMK_DMA_BOUNCE_POOL_TYPE_NONE=0,
   VMK_DMA_BOUNCE_POOL_TYPE_UNKNOWN=1,
} vmk_DMABouncePoolType;

/**
 * \brief Empty value for the flags field indicating no flags are set.
 */
#define VMK_DMA_ENGINE_FLAGS_NONE       0

 /**
  * \brief Indicates that this engine needs coherent mappings.
  *
  * A DMA-coherent mapping is one where a write into the mapped memory from
  * either the device or the processor will be visible to the other without the
  * need for the mapping to be flushed.  Note that this places more
  * restrictions on the mapper's ability to successfully map memory and
  * therefore should only be used when necessary.
  */
#define VMK_DMA_ENGINE_FLAGS_COHERENT   (1 << 0)

/**
 * \brief Request IOMMU mapper for this engine.
 *
 * Indicates that this engine requests IOMMU mapper regardless of the default
 * DMA policy.  In many production systems, the default DMA policy is IOMMU
 * disabled, and this flag can override the default.
 */
#define VMK_DMA_ENGINE_FLAGS_REQUEST_IOMMU_MAPPER   (1 << 1)

/**
 * \brief Bounce buffer memory pool properties.
 *
 * Bounce-buffer pools are used when memory needs to be allocated
 * at a particular address to meet the constraints of a DMA
 * Engine.
 *
 * If no bounce buffer is supplied, the caller is expected to
 * deal with all DMA mapping errors and handle bounce-buffering
 * on its own.
 */
typedef struct {
   /** \brief Module allocating a new bounce buffer pool */
   vmk_ModuleID module;

   /** \brief Type of bounce pool these properties describe. */
   vmk_DMABouncePoolType type;
} vmk_DMABouncePoolProps;

/**
 * \brief DMA Engine creation properties.
 */
typedef struct vmk_DMAEngineProps {
   /** \brief Name of the DMA engine for debugging purposes. */
   vmk_Name name;

   /** \brief Module allocating the new DMA engine. */
   vmk_ModuleID module;

   /**
    * \brief Flags to specify additional properties of the DMA engine.
    */
   vmk_uint32 flags;

   /** \brief Parent device the DMA engine resides on. */
   vmk_Device device;

   /** \brief Constraints for the DMA engine. */
   vmk_DMAConstraints *constraints;

   /** \brief The bounce buffer pool for the DMA engine. */
   vmk_DMABouncePoolProps *bounce;
} vmk_DMAEngineProps;

/**
 * \brief Device DMA registration properties.
 */
typedef struct vmk_DMADeviceProps {
   /** \brief Name of the device properties for debugging purposes. */
   vmk_Name name;

   /** \brief Module registering the device. */
   vmk_ModuleID module;

   /** \brief Device to register */
   vmk_Device device;

   /** \brief Constraints on DMA imposed by the device */
   vmk_DMAConstraints *constraints;
} vmk_DMADeviceProps;

/**
 * \brief Direction of data flow for a DMA.
 */
typedef enum vmk_DMADirection {
   /** No DMA to/from main memory. */
   VMK_DMA_DIRECTION_NONE=0,

   /** DMA from device to main memory. */
   VMK_DMA_DIRECTION_TO_MEMORY=1,

   /** DMA to device from main memory. */
   VMK_DMA_DIRECTION_FROM_MEMORY=2,

   /** DMA may go to/from main memory. */
   VMK_DMA_DIRECTION_BIDIRECTIONAL=3,
} vmk_DMADirection;

/**
 * \brief Reasons for a DMA mapping failure.
 */
typedef enum vmk_DMAMapErrorReason {
   /** There was no DMA mapping error. */
   VMK_DMA_MAP_ERROR_REASON_NONE=0,

   /** There was an unknown DMA error. */
   VMK_DMA_MAP_ERROR_UNKNOWN=1,

   /** The transfer was too large for the DMA egnine */
   VMK_DMA_MAP_ERROR_REASON_TRANSFER_TOO_LARGE=2,

   /**
    * There were too many SG array elements in the
    * SG array and the mapping code was unable to
    * coalesce the array to fit.
    */
   VMK_DMA_MAP_ERROR_REASON_TOO_MANY_ENTRIES=3,

   /**
    * Could not map SG array to fit within
    * sgMaxEntries constraint with the sgElemMaxSize
    * constraint.
    */
   VMK_DMA_MAP_ERROR_REASON_SG_ELEM_MAX_SIZE=4,

   /**
    * Could not map SG array to fit within the
    * sgMaxEntries constraint with the sgElemMult
    * constraint.
    */
   VMK_DMA_MAP_ERROR_REASON_SG_ELEM_SIZE_MULT=5,

   /**
    * Could not map SG array to fit within the
    * sgMaxEntries constraint with the sgElemStraddle
    * constraint.
    */
   VMK_DMA_MAP_ERROR_REASON_SG_ELEM_STRADDLE=6,

   /**
    * One or more SG array elements don't meet the
    * address mask of the DMA engine.
    */
   VMK_DMA_MAP_ERROR_REASON_SG_ELEM_ADDRESS=7,

   /**
    * One or more SG array elements are not aligned properly.
    */
   VMK_DMA_MAP_ERROR_REASON_SG_ELEM_ALIGNMENT=8,

   /**
    * Not enough IO address space available to map elements.
    */
   VMK_DMA_MAP_ERROR_REASON_NO_IO_SPACE=9,
} vmk_DMAMapErrorReason;

/**
 * \brief Additional information for certain DMA mapping errors
 */
typedef struct vmk_DMAMapErrorInfo {
   /**
    * Worst-case reason why the DMA mapping failed.
    */
   vmk_DMAMapErrorReason reason;

   /**
    * Additional information about the failure.
    */
   union {
      /** Info for VMK_DMA_MAP_ERROR_REASON_TRANSFER_TOO_LARGE */
      struct {
         /** Largest transfer size for this DMA engine. */
         vmk_ByteCount maxTransfer;
      } tooLarge;

      /** Info for VMK_DMA_MAP_ERROR_REASON_TOO_MANY_ENTRIES */
      struct {
         /** Max number of entries for this DMA engine. */
         vmk_uint32 sgMaxEntries;
      } tooManyEntries;

       /** Info for VMK_DMA_MAP_ERROR_REASON_SG_ELEM_MAX_SIZE */
      struct {
         /** The max size of an SG element on the DMA engine. */
         vmk_ByteCountSmall sgElemMaxSize;

         /** First element detected with size problem. */
         vmk_uint32 badElem;
      } elemMaxSize;

      /** Info for VMK_DMA_MAP_ERROR_REASON_SG_ELEM_SIZE_MULT */
      struct {
         /** The transfer multiple for the DMA engine */
         vmk_ByteCount sgElemSizeMult;

         /** First element detected with size multiple problem. */
         vmk_uint32 badElem;
      } elemSizeMult;

      /** Info for VMK_DMA_MAP_ERROR_REASON_SG_ELEM_STRADDLE */
      struct {
         /** The SG element straddle constraint for the DMA engine. */
         vmk_IOA sgElemStraddle;

         /** First element detected with straddle problem. */
         vmk_uint32 badElem;
      } elemStraddle;

      /** Info for VMK_DMA_MAP_ERROR_REASON_SG_ELEM_ADDRESS */
      struct {
         /** The address mask for the DMA engine */
         vmk_IOA addressMask;

         /** First element detected with alignment problem. */
         vmk_uint32 badElem;
      } elemAddress;

      /** Info for VMK_DMA_MAP_ERROR_REASON_SG_ELEM_ALIGNMENT */
      struct {
         /** The alignment for the DMA engine */
         vmk_ByteCountSmall sgElemAlignment;

         /** First element detected with alignment problem. */
         vmk_uint32 badElem;
      } elemAlignment;

      /** Info for VMK_DMA_MAP_ERROR_REASON_NO_IO_SPACE */
      struct {
         /** Number of bytes in request that could be mapped. */
         vmk_ByteCount maxMappable;
      } noIOSpace;
   } info;
} vmk_DMAMapErrorInfo;

/**
 * \brief DMA protections for a machine memory region
 */
typedef struct vmk_DMAProtectionAttr {
   /** The DMA directions that are allowed on the memory */
   vmk_DMADirection access;
} vmk_DMAProtectionAttr;

/**
 * \brief Length used to indicate that an entire DMA SG array should
 *        be flushed.
 */
#define VMK_DMA_FLUSH_SG_ALL VMK_UINT64_MAX

/*
 ***********************************************************************
 * vmk_DMADeviceRegister --                                       */ /**
 *
 * \brief Register the DMA constraints imposed by a device between
 *        DMA engines and main-memory.
 *
 * This call is used to register DMA constraints imposed by bridge
 * devices between an endpoint device and system memory. This
 * information is used to compute the complete set of DMA constraints
 * from an engine all the way to main memory.
 *
 * \note This call may only be made when a device is initializing but
 *       before any child devices or DMA engines are added.
 *
 * \note This function may block.
 *
 * \param[in]  props    The DMA properties of a device.
 * \param[out] device   A handle representing the DMA constraint
 *                      information of a device.
 *
 * \retval VMK_BAD_PARAM      The specified properties are problematic.
 * \retval VMK_NO_MODULE_HEAP This module's heap is not set.
 * \retval VMK_BUSY           The device is no longer eligible to
 *                            register for constraints because it has
 *                            at least one active child device or DMA
 *                            engine.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMADeviceRegister(vmk_DMADeviceProps *props,
                                       vmk_DMADevice *device);

/*
 ***********************************************************************
 * vmk_DMADeviceUnregister --                                     */ /**
 *
 * \brief Unregister DMA constraints imposed by a device on
 *        upstream devices.
 *
 * \note This call should be made when a device no longer has
 *       child devices or engines that it is managing.
 *
 * \note This function may block.
 *
 * \param[in] device    A handle representing the DMA constraint
 *                      information of a device.
 *
 * \retval VMK_BAD_PARAM      The specified DMA engine is invalid.
 * \retval VMK_BUSY           The device is still has at least
 *                            one active device or DMA engine.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMADeviceUnregister(vmk_DMADevice device);

/*
 ***********************************************************************
 * vmk_DMAEngineCreate --                                         */ /**
 *
 * \brief Create a handle representing a DMA engine on a device.
 *
 * There may be several DMA engines on a device, each imposing their
 * own DMA constraints based on particular hardware properties of
 * the engine.
 *
 * \note This function may block.
 *
 * \param[in]  props    The properties of the DMA engine to create.
 * \param[out] engine   A handle representing a DMA engine on a
 *                      device.
 *
 * \retval VMK_BAD_PARAM      The specified properties are problematic.
 * \retval VMK_NO_MODULE_HEAP This module's heap is not set.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAEngineCreate(vmk_DMAEngineProps *props,
                                     vmk_DMAEngine *engine);

/*
 ***********************************************************************
 * vmk_DMAEngineDestroy --                                        */ /**
 *
 * \brief Destroy a handle representing a DMA engine on a device.
 *
 * \note This function will not block.
 *
 * \param[in] engine   A handle representing a DMA engine on a device.
 *
 * \retval VMK_BAD_PARAM      The specified DMA engine is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAEngineDestroy(vmk_DMAEngine engine);

/*
 ***********************************************************************
 * vmk_DMAMapSg --                                                */ /**
 *
 * \brief Map machine memory in an SG array to IO addresses.
 *
 * This call will attempt to map a machine-address SG array and create
 * a new IO-address SG array element.
 *
 * \note The input SG array must not be freed or modified while it
 *       is mapped or the results are undefined.
 *
 * \note If an SG array is simultaneously mapped with multiple DMA
 *       directions, the contents of the memory the SG array represents
 *       are undefined.
 *
 * \note This function will not block.
 *
 * \param[in]  engine      A handle representing a DMA engine to map to.
 * \param[in]  direction   Direction of the DMA transfer for the mapping.
 * \param[in]  sgOps       Scatter-gather ops handle used to allocate
 *                         the new SG array for the mapping.
 * \param[in]  in          SG array containing machine addresses to
 *                         map for the DMA engine.
 * \param[out] out         New SG array containing mapped IO addresses.
 *                         Note that this may be the same SG array as
 *                         the array passed in depending on choices
 *                         made by the kernel's mapping code.
 * \param[out] err         If this call fails with VMK_DMA_MAPPING_FAILED,
 *                         additional information about the failure may
 *                         be found here. This may be set to NULL if the
 *                         information is not desired.
 *
 * \retval VMK_BAD_PARAM            The specified DMA engine is invalid.
 * \retval VMK_DMA_MAPPING_FAILED   The mapping failed because the
 *                                  DMA constraints could not be met.
 *                                  Additional information about the
 *                                  failure can be found in the "err"
 *                                  argument.
 * \retval VMK_NO_MEMORY            There is currently insufficient
 *                                  memory available to construct the
 *                                  mapping.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAMapSg(vmk_DMAEngine engine,
                              vmk_DMADirection direction,
                              vmk_SgOpsHandle sgOps,
                              vmk_SgArray *in,
                              vmk_SgArray **out,
                              vmk_DMAMapErrorInfo *err);

/*
 ***********************************************************************
 * vmk_DMAFlushSg --                                              */ /**
 *
 * \brief Synchronize a DMA mapping specified in a SG array.
 *
 * This call is used to synchronize data if the CPU needs to read or
 * write after an DMA mapping is active on a region of machine memory
 * but before the DMA mapping is unmapped.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_FROM_MEMORY after CPU writes are complete but
 * before any new DMA read transactions occur on the memory.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_TO_MEMORY before CPU reads but after
 * any write DMA transactions complete on the memory.
 *
 * DMA map and unmap calls will implicitly perform a flush of the entire
 * SG array.
 *
 * The code may flush bytes rounded up to the nearest page or other
 * HW-imposed increment.
 *
 * \note The sg array supplied to this function must be an array output
 *       from vmk_DMAMapSG or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing the DMA engine used
 *                        for the mapping.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] sg          Scatter-gather array containing the
 *                        IO-address ranges to flush.
 * \param[in] offset      Offset into the buffer the SG array represents.
 * \param[in] len         Number of bytes to flush or VMK_DMA_FLUSH_SG_ALL
 *                        to flush the entire SG array starting from
 *                        the offset.
 *
 *
 * \retval VMK_BAD_PARAM         Unknown duration or direction, or
 *                               unsupported direction.
 * \retval VMK_INVALID_ADDRESS   Memory in the specified scatter-gather
 *                               array is not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAFlushSg(vmk_DMAEngine engine,
                                vmk_DMADirection direction,
                                vmk_SgArray *sg,
                                vmk_ByteCount offset,
                                vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_DMAUnmapSg --                                              */ /**
 *
 * \brief Unmaps previously mapped IO addresses from an SG array.
 *
 * \note The direction must match the direction at the time of mapping
 *       or the results of this call are undefined.
 *
 * \note The sg array supplied to this function must be one mapped with
 *       vmk_DMAMapSG or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing a DMA engine
 *                        to unmap from.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] sg          Scatter-gather array containing the
 *                        IO address ranges to unmap.
 * \param[in] sgOps       Scatter-gather ops handle used to free
 *                        the SG array if necessary.
 *
 * \retval VMK_BAD_PARAM         Unknown direction, or unsupported
 *                               direction.
 * \retval VMK_INVALID_ADDRESS   One ore more pages in the specified
 *                               machine address range are not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAUnmapSg(vmk_DMAEngine engine,
                                vmk_DMADirection direction,
                                vmk_SgOpsHandle sgOps,
                                vmk_SgArray *sg);

/*
 ***********************************************************************
 * vmk_DMAMapElem --                                              */ /**
 *
 * \brief Map machine memory of a single machine address range to an
 *        IO address range.
 *
 * This call will attempt to map a single machine-address range and
 * create a new IO-address address range that maps to it.
 *
 * \note The input range must not be freed or modified while it
 *       is mapped or the results are undefined.
 *
 * \note If the range is simultaneously mapped with multiple DMA
 *       directions, the contents of the memory the SG array represents
 *       are undefined.
 *
 * \note This function will not block.
 *
 * \param[in]  engine      A handle representing a DMA engine to map to.
 * \param[in]  direction   Direction of the DMA transfer for the mapping.
 * \param[in]  in          A single SG array element containing a single
 *                         machine addresse range to map for the
 *                         DMA engine.
 * \param[in]  lastElem    Indicates if this is the last element in
 *                         the transfer.
 * \param[out] out         A single SG array element to hold the mapped
 *                         IO address for the range.
 * \param[out] err         If this call fails with VMK_DMA_MAPPING_FAILED,
 *                         additional information about the failure may
 *                         be found here. This may be set to NULL if the
 *                         information is not desired.
 *
 * \retval VMK_BAD_PARAM            The specified DMA engine is invalid.
 * \retval VMK_DMA_MAPPING_FAILED   The mapping failed because the
 *                                  DMA constraints could not be met.
 *                                  Additional information about the
 *                                  failure can be found in the "err"
 *                                  argument.
 * \retval VMK_NO_MEMORY            There is currently insufficient
 *                                  memory available to construct the
 *                                  mapping.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAMapElem(vmk_DMAEngine engine,
                                vmk_DMADirection direction,
                                vmk_SgElem *in,
                                vmk_Bool   lastElem,
                                vmk_SgElem *out,
                                vmk_DMAMapErrorInfo *err);

/*
 ***********************************************************************
 * vmk_DMAFlushElem --                                            */ /**
 *
 * \brief Synchronize a DMA mapping for a single IO address range.
 *
 * This call is used to synchronize data if the CPU needs to read or
 * write after an DMA mapping is active on a region of machine memory
 * but before the DMA mapping is unmapped.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_FROM_MEMORY after CPU writes are complete but
 * before any new DMA read transactions occur on the memory.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_TO_MEMORY before CPU reads but after
 * any write DMA transactions complete on the memory.
 *
 * DMA map and unmap calls will implicitly perform a flush of the
 * element.
 *
 * The code may flush bytes rounded up to the nearest page or other
 * HW-imposed increment.
 *
 * \note The IO element supplied to this function must be an element
 *       output from vmk_DMAMapElem or the results of this call are
 *       undefined.
 *
 *       Do not use this to flush a single element in an SG array
 *       that was mapped by vmk_DMAMapSg.
 *
 * \note The original element supplied to this function must be
 *       the one supplied to vmk_DMAMapElem when the IO element
 *       was created or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing the DMA engine used
 *                        for the mapping.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to flush.
 *
 * \retval VMK_BAD_PARAM         Unknown duration or direction, or
 *                               unsupported direction.
 * \retval VMK_INVALID_ADDRESS   Memory in the specified element
 *                               is not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAFlushElem(vmk_DMAEngine engine,
                                  vmk_DMADirection direction,
                                  vmk_SgElem *IOElem);

/*
 ***********************************************************************
 * vmk_DMAUnmapElem --                                            */ /**
 *
 * \brief Unmaps previously mapped IO address range.
 *
 * \note The direction must match the direction at the time of mapping
 *       or the results of this call are undefined.
 *
 * \note The element supplied to this function must be one mapped with
 *       vmk_DMAMapElem or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing a DMA engine
 *                        to unmap from.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to unmap.
 *
 * \retval VMK_BAD_PARAM         Unknown direction, or unsupported
 *                               direction.
 * \retval VMK_INVALID_ADDRESS   One ore more pages in the specified
 *                               machine address range are not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAUnmapElem(vmk_DMAEngine engine,
                                  vmk_DMADirection direction,
                                  vmk_SgElem *IOElem);

/*
 ***********************************************************************
 * vmk_DMAGetAllocAddrConstraint --                               */ /**
 *
 * \brief Get the optimal allocation address constraint for a DMA engine.
 *
 * The function returns the allocation address constraint that would
 * yield allocations at addresses well-suited for the given DMA engine.
 *
 * \note This function will not block.
 *
 * \param[in]  engine      A handle representing a DMA engine
 *                         to find the allocation constraint for.
 * \param[out] con         The allocation address constraint well-suited
 *                         for the given DMA engine.
 *
 * \retval VMK_BAD_PARAM   The DMA engine was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAGetAllocAddrConstraint(vmk_DMAEngine engine,
                                               vmk_MemPhysAddrConstraint *con);

/*
 ***********************************************************************
 * vmk_DMASetProtection --                                        */ /**
 *
 * \brief Set DMA permissions on a machine-page range.
 *
 * This call sets maximum allowable permissions for DMA mapping on
 * a machine page range.
 *
 * \note Permissions set here are advisory. The permissions may be
 *       enforced during DMA mapping, lazily enforced in hardware,
 *       partially, or not at all.
 *
 * \note Allocated memory and memory from IO stacks is generally
 *       already set with sufficient permissions to map it or may
 *       already be mapped.
 *
 *       This call should only be used to protect critical regions
 *       from being mapped accidentally and is not intended for
 *       use on fast paths.
 *
 * \note Protection regions set by this call should be cleared
 *       before the memory is freed.
 *
 * \note This function will not block.
 *
 * \param[in] range        Machine page range to protect.
 * \param[in] prot         Protections to set on a page range.
 *
 * \retval VMK_BAD_PARAM         The specified permissions are invalid.
 * \retval VMK_INVALID_ADDRESS   The specified machine address range
 *                               is unbacked by physical memory.
 * \retval VMK_NO_PERMISSION     The caller does not have permission
 *                               to modify the address range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMASetProtection(vmk_MpnRange *range,
                                      vmk_DMAProtectionAttr *prot);

/*
 ***********************************************************************
 * vmk_DMAClearProtection --                                      */ /**
 *
 * \brief Clear any DMA permissions on a machine-page range.
 *
 * This function resets the permissions on the specified machine pages to
 * the default permissions.
 *
 * \note This function will not block.
 *
 * \param[in] range        Machine page range from which to reset
 *                         permissions.
 *
 * \retval VMK_INVALID_ADDRESS   The specified machine address range
 *                               is unbacked by physical memory.
 * \retval VMK_NO_PERMISSION     The caller does not have permission
 *                               to modify the address range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAClearProtection(vmk_MpnRange *range);


/*
 ***********************************************************************
 * vmk_DMADirectionToString --                                    */ /**
 *
 * \brief Turn a vmk_DMADirection into a printable string.
 *
 * \note This function will not block.
 *
 * \param[in] direction    Direction to convert.
 *
 * \return A printable string corresponding to the DMA direction.
 *
 ***********************************************************************
 */
const char *vmk_DMADirectionToString(vmk_DMADirection direction);

/*
 ***********************************************************************
 * vmk_DMAMapErrorReasonToString --                               */ /**
 *
 * \brief Turn a vmk_DMAMapErrorReason into a printable string.
 *
 * \note This function will not block.
 *
 * \param[in] reason    Reason to convert.
 *
 * \return A printable string corresponding to the DMA map error reason.
 *
 ***********************************************************************
 */
const char *vmk_DMAMapErrorReasonToString(vmk_DMAMapErrorReason reason);

#endif /* _VMKAPI_DMA_H_ */
/** @} */
/** @} */
