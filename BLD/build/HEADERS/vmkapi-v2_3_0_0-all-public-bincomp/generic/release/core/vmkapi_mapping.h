/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Mapping                                                               */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup Mapping Mapping
 *
 * \brief Interfaces and definitions to map machine page ranges into a virtual
 *        address space.
 *
 * The VMKAPI mapping interfaces allow a user to map one or more machine page
 * ranges into a virtually contiguous mapping. The mappings are globally visible
 * on every PCPU.
 *
 * \par Example 1 - Map MPN 12-13 read-only and unmap the mapping:
 *
 * \code
 * vmk_MapRequest    mRequest;
 * vmk_MpnRange      mRange;
 * vmk_VA            va;
 * 
 * mRange.startMPN = 12;
 * mRange.numPages = 2;
 * mRequest.mapType = VMK_MAPTYPE_DEFAULT;
 * mRequest.mapAttrs = VMK_MAPATTRS_READONLY;
 * mRequest.numElements = 1;
 * mRequest.mpnRanges = &mRange;
 * mRequest.reservation = NULL;
 * status = vmk_Map(<myModuleID>, &mRequest, &va);
 * if (status == VMK_OK) {
 *    vmk_Unmap(va);
 * }
 * \endcode
 *
 * If status is VMK_OK then va will contain the starting virtual
 * address of the mapping for MPNs 12-13.
 * 
 * \par Example 2 - Map MPN [6-20] and [42-50] read-write
 *
 * \code
 * vmk_MapRequest    mRequest;
 * vmk_MpnRange      mRange[2];
 * vmk_VA            va;
 *
 * mRange[0].startMPN = 6;
 * mRange[0].numPages = 15;
 * mRange[1].startMPN = 42;
 * mRange[1].numPages = 9;
 * mRequest.mapType = VMK_MAPTYPE_DEFAULT;
 * mRequest.mapAttrs = VMK_MAPATTRS_READWRITE | VMK_MAPATTRS_MIGHTCHANGE;
 * mRequest.numElements = 2;
 * mRequest.mpnRanges = &mRange;
 * mRequest.reservation = NULL;
 * status = vmk_Map(<myModuleID>, &mRequest, &va);
 * if (status == VMK_OK) {
 *    status = vmk_MapChangeAttributes(va, VMK_MAPATTRS_EXECUTABLE);
 *    vmk_Unmap(va);
 * }
 * \endcode
 *
 * if status is VMK_OK then va will contain the starting virtual
 * address of the mapping for MPNs [6-20],[42-50]. If vmk_Map completed
 * successfully we change the mapping to be read-only, executable instead of
 * read-write. Since we are not using the mapping afterwards we don't have to
 * check the return value of vmk_MapChangeAttributes and we'll just unmap the
 * mapping instead.
 * 
 * To map device memory, map type should be VMK_MAPTYPE_DEVICE and an IO 
 * reservation handle obtained from the IOResource interface must be provided
 * in the map request.
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_MAPPING_H_
#define _VMKAPI_CORE_MAPPING_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief MPN Range
 *
 * Used to describe a physically contiguous block of machine pages.
 */
typedef struct vmk_MpnRange {
   /** Page number of the first machine page in the range to be mapped */
   vmk_MPN    startMPN;
   /** Total number of pages in the range */
   vmk_uint32 numPages;
} vmk_MpnRange;

/**
 * \brief Map type 
 *
 * Type of memory being mapped.
 */

typedef enum {
   /** Undefined map type */
   VMK_MAPTYPE_NONE    = 0,
   /** Default: machine memory */
   VMK_MAPTYPE_DEFAULT = 1,
   /** Device memory */
   VMK_MAPTYPE_DEVICE  = 2,
} vmk_MapRequestType;

/**
 * \brief Mapping Attributes
 *
 * Mapping attributes are used to specify special attributes for a mapping
 * operation. If no special attributes are specified then the mappings will be
 * not executable and read-only. A user is allowed to combine multiple
 * attributes with a binary OR operation e.g (VMK_MAPATTRS_READWRITE |
 * VMK_MAPATTRS_EXECUTABLE). 
 */
typedef vmk_uint64 vmk_MapAttrs;

/** 
 * \brief Map pages read-only. 
 *
 * Establishing read-only mappings is very fast since it usually doesn't require
 * any CPU cache invalidation 
 */
#define VMK_MAPATTRS_READONLY           (0)

/** Map all page ranges read- and writeable */
#define VMK_MAPATTRS_READWRITE          (1<<0)

/** 
 * \brief Map all page ranges executable. 
 *
 * This causes ESX to not set the no-executable (NX) bit for all mapped ranges.
 * Please note that ESX has an extensive module loading interface that should
 * usually be sufficient for any on-the-fly code additions that migh be needed.
 */
#define VMK_MAPATTRS_EXECUTABLE         (1<<1)

/** 
 * \brief Map pages as write-combined.
 *
 * Please make sure that you consult the x86 architecture manuals about the
 * interactions between write combined page table attributes and memory typed
 * registers before using this attribute. 
 */
#define VMK_MAPATTRS_WRITECOMBINE       (1<<2)

/**
 * \brief Mark page table entries as uncached
 *
 * Page table entries for the mapping will not be cached in the TLB. Use this
 * attribute with caution it will cause a noticable slow down when accessing the
 * mapped pages. Usually this attribute is only needed when mapping 'special'
 * pages e.g. memory backed device registers.
 */
#define VMK_MAPATTRS_UNCACHED           (1<<3)

/**
 * \brief User of the mapping might use vmk_MapChangeAttributes
 *
 * Needed if a user of the mapping might use vmk_MapChangeAttributes on the
 * mapping.
 */
#define VMK_MAPATTRS_MIGHTCHANGE        (1<<4)


/**
 * \brief Map Request Descriptor
 *
 * This struct describes a mapping request for a virtually contiguous mapping of
 * one or multiple machine page ranges. The user provides a filled in mpnRanges
 * array of size numElements.
 */
typedef struct vmk_MapRequest {
   /** Type of memory being mapped */
   vmk_MapRequestType mapType;
   /** Attributes for the mapping */
   vmk_MapAttrs mapAttrs; 
   /** Number of elements in the vmk_MpnRange array */
   vmk_uint32    numElements;
   /** Array of numElements vmk_MpnRanges */
   vmk_MpnRange *mpnRanges;
   /** IO reservation handle required if mapping device memory */
   vmk_IOReservation reservation;
} vmk_MapRequest;


/*
 ******************************************************************************
 * vmk_Map --                                                            */ /**
 *
 * \brief Map the provided machine address ranges to a virtual address
 *
 * This function establishes a globally visible virtual contiguous mapping for
 * the provided machine page address ranges.
 *
 * \param[in]     moduleID    Module ID of the caller
 * \param[in]     mapRequest  Pointer to a mapRequest struct.
 * \param[out]    va          If the function completes successfully ESX will 
 *                            fill va with the starting virtual address of 
 *                            the mapping.
 * 
 * \return VMK_OK if the range was successfully mapped, error code otherwise.
 *
 * \note This function is only callable from a regular execution context.
 * \note Mapping requests are accounted against the caller's module. During 
 *       module removal the module has to release all previously acquired 
 *       mappings. Establishing read-only mappings for one machine address 
 *       range backed by regular memory (i.e., not device memory) is
 *       significantly faster than all other mapping operations.
 * \note This function will not block.
 *
 ******************************************************************************
 */

VMK_ReturnStatus 
vmk_Map(vmk_ModuleID    moduleID,
        vmk_MapRequest *mapRequest,
        vmk_VA         *va);


/*
 ******************************************************************************
 * vmk_Unmap --                                                          */ /**
 *
 * \brief Unmap a mapping created by vmk_Map
 *
 * \param[in]  va     Virtual address previously returned by vmk_Map
 *
 * \note This function is only callable from a regular execution context
 * \note This function will not block.
 *
 ******************************************************************************
 */

void
vmk_Unmap(vmk_VA va);


/*
 ******************************************************************************
 * vmk_MapChangeAttributes --                                            */ /**
 *
 * \brief Change the mapping attributes of a mapping created via vmk_Map
 *
 * This function changes the attributes of a mapping that has previously been
 * created via vmk_Map.
 *
 * \param[in] va      Virtual address previously returned by vmk_Map
 * \param[in] attrs   New attributes for the mapping
 * 
 * \return VMK_OK if the range was successfully mapped, error code otherwise.
 *
 * \note This function is only callable from a regular execution context.
 * \note This function only operates on mappings that were mapped with the
 *       VMK_MAPATTRS_MIGHTCHANGE flag.
 * \note Changing the attributes of a mapping has significant costs and should 
 *       not be done on a fast path.
 * \note This function will not block.
 *
 ******************************************************************************
 */

VMK_ReturnStatus 
vmk_MapChangeAttributes(vmk_VA va,
                        vmk_MapAttrs attrs);


#endif
/** @} */
/** @} */
