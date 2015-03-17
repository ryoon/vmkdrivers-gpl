/* **********************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Large Space Mapping                                                   */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup LargeSpaceMapping Large Space Mapping
 *
 * \brief Methods to map large amounts of machine pages into address spaces.
 *
 * The VMKAPI large space mapping interfaces allow a user to map one or more
 * machine page ranges into a virtually contiguous mapping.  The mappings are
 * globally visible on every PCPU.  These interfaces are designed for mapping
 * large amounts of address space, potentially with sparse backing store,
 * and for mapping large device memory regions.
 *
 * As a mapping Large Space Mapping reservation only returns a starting 
 * virtual address, clients are required to manage this region manually,
 * mapping and unmapping at specific virtual addresses.  This is unlike
 * the normal Mapping interface which simply returns a new virtual address
 * managed by the system.
 *
 * \par Example 1 - Reserve 256MB of space for a Large Space area
 *
 * \code
 *
 * vmk_ModuleID            myModuleID;
 * vmk_HeapCreateProps     heapProps;
 * vmk_LargeSpaceHandle    lHandle = VMK_LARGESPACE_INVALID_HANDLE;
 * vmk_VPN                 myStartVPN;
 * vmk_HeapID              myHeapID;
 * vmk_Name                myName;
 *
 * myModuleID = vmk_ModuleStackTop();
 *
 * vmk_NameInitialize(&myName, "myModule");
 * vmk_NameInitialize(&heapProps.name, "myModule");
 * VMK_ASSERT(status == VMK_OK);
 * heapProps.type = VMK_HEAP_TYPE_SIMPLE;
 * heapProps.module = myModuleID;
 * heapProps.initial = 4 * VMK_KILOBYTE;
 * heapProps.max = 1 * VMK_MEGABYTE;
 * heapProps.creationTimeoutMS = VMK_TIMEOUT_NONBLOCKING;
 *
 * status = vmk_HeapCreate(&heapProps, &myHeapID);
 * if (status != VMK_OK) {
 *    vmk_WarningMessage("vmk_HeapCreate failed: %s",
 *                       vmk_StatusToString(status));
 *    return VMK_NO_MEMORY;
 * }
 *
 * status = vmk_LargeSpaceReserve(myModuleID,
 *                                &myName,
 *                                65536,
 *                                &lHandle,
 *                                &myStartVPN);
 *
 * \endcode
 *
 * \par Example 2 - Map MPN 98304-114688 with the default flags and
 *      unmap the mapping
 *
 * \code
 * vmk_VA            va;
 * 
 * va = myStartVPN;
 * status = vmk_LargeSpaceMap(lHandle,
 *                            va,
 *                            98304,
 *                            VMK_LARGESPACE_MAP_FLAG_NONE,
 *                            NULL,
 *                            16384);
 * if (status == VMK_OK) {
 *    vmk_LargeSpaceUnmap(lHandle, va, 16384, VMK_LARGESPACE_MAP_FLAG_NONE);
 * }
 *
 * \endcode
 *
 * \par Example 3 - Map MPN 98304-114688 read-only and unmap the mapping,
 *      and with large page mappings
 *
 * \code
 *
 * vmk_VA            va;
 * 
 * va = myStartVPN + 16384;
 * status = vmk_LargeSpaceMap(lHandle,
 *                            va,
 *                            98304,
 *                            VMK_LARGESPACE_MAP_READONLY |
 *                            VMK_LARGESPACE_MAP_LARGE_PAGE,
 *                            NULL,
 *                            16384);
 * if (status == VMK_OK) {
 *    vmk_LargeSpaceUnmap(lHandle, va, 16384, VMK_LARGESPACE_MAP_LARGE_PAGE);
 * }
 *
 * \endcode
 *
 * To map device memory, map type should be VMK_MAPTYPE_DEVICE and an IO 
 * reservation handle obtained from the IOResource interface must be provided
 * in the map request.
 *
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_LARGESPACE_H_
#define _VMKAPI_CORE_LARGESPACE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Large Space handle
 *
 * Filled in by vmk_LargeSpaceMap().
 */
typedef struct vmk_LargeSpaceHandleInt *vmk_LargeSpaceHandle;

#define VMK_LARGESPACE_INVALID_HANDLE ((vmk_LargeSpaceHandle)-1)


/**
 * \brief Large Space Mapping Flags
 */
typedef enum vmk_LargeSpaceMapFlags {
   /* Mapping has no special flags */
   VMK_LARGESPACE_MAP_FLAG_NONE =           0x0000,

   /* The item that needs to be mapped/unmapped is a large page */
   VMK_LARGESPACE_MAP_FLAG_LARGE_PAGE =     0x0001,

   /* Map pages as uncached */
   VMK_LARGESPACE_MAP_FLAG_UNCACHED =       0x0002,

   /* Map device memory, default is machine memory */
   VMK_LARGESPACE_MAP_FLAG_DEVICE =         0x0004,

   /* Map these pages read-only */
   VMK_LARGESPACE_MAP_FLAG_READONLY =       0x0008,

   /* Map these pages with write combining */
   VMK_LARGESPACE_MAP_FLAG_WRITECOMBINE =   0x0010,
} vmk_LargeSpaceMapFlags;


/*
 ******************************************************************************
 * vmk_LargeSpaceReserve --                                              */ /**
 *
 * \brief Reserve a space capable of mapping extremely large mappings.
 *
 * This function establishes a globally visible virtual contiguous mapping for
 * the provided machine page address ranges.
 *
 * \note This function is only callable from a regular execution context.
 * \note This function may block.
 * \note This function requires there be a vmk_HeapID associated with the
         moduleID.
 * \note The vmk_LargeSpaceAllocSize() function should be used to ensure the
 *       default heap for this moduleID is large enough.
 *
 * \param[in]     moduleID    Module ID of the caller
 * \param[in]     mapName     Name of this LargeSpace
 * \param[in]     numPages    Number of virtual pages to reserve
 * \param[out]    lHandle     Large Space handle, filled in upon success
 * \param[out]    vpn         Starting Virtual Page Number of this Large Space
 *                            in 4KB pages.
 * 
 * \retval VMK_OK             If the reservation was successfully created.
 *
 ******************************************************************************
 */

VMK_ReturnStatus 
vmk_LargeSpaceReserve(vmk_ModuleID moduleID,
                      const vmk_Name *mapName,
                      vmk_uint64 numPages,
                      vmk_LargeSpaceHandle *lHandle,
                      vmk_VPN *vpn);


/*
 ******************************************************************************
 * vmk_LargeSpaceDelete --                                              */ /**
 *
 * \brief Delete a Large Space reservation obtained from vmk_LargeSpaceReserve
 *
 * This function establishes a globally visible virtual contiguous mapping for
 * the provided machine page address ranges.
 *
 * \note This function is only callable from a regular execution context.
 * \note This function will not block.
 *
 * \param[in]     lHandle     Large Space handle returned from Reserve
 * 
 * \retval VMK_OK             If the reservation was successfully deleted.
 * \retval VMK_BUSY           If the reservation has active mappings.
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_LargeSpaceDelete(vmk_LargeSpaceHandle lHandle);

/*
 ******************************************************************************
 * vmk_LargeSpaceMap --                                                  */ /**
 *
 * \brief Map the provided machine address to a virtual address
 *
 * This function establishes a globally visible virtual contiguous mapping for
 * the provided machine page address ranges.
 *
 * \note This function is only callable from a regular execution context.
 * \note Mapping requests are accounted against the caller's module.  During 
 *       module removal the module has to release all previously acquired 
 *       mappings.
 * \note Mapping large pages requires numPages counts to be 2MB aligned.
 * \note This function may block.
 *
 * \param[in]     lHandle       Large Space handle from vmk_LargeSpaceReserve.
 * \param[in]     vpn           The Virtual Page Number to map the below MPN.
 * \param[in]     mpn           The Machine Page Number to begin mapping at.
 * \param[in]     mapFlags      Flags to map these pages with.
 * \param[in]     reservation   IO reservation obtained from
 *                              vmk_IOResourceRegister().  Required if mapping
 *                              device memory.
 * \param[in]     numPages      Number of virtual pages to map, counted in 4KB
 *                              pages.
 *
 * \retval VMK_OK               If the range was successfully mapped.
 * \retval VMK_BAD_PARAM        If an invalid handle, vpn, mpn, flag or number
 *                              of pages is passed, or the mapping is outside
 *                              of this Large Space.
 * \retval VMK_BAD_ADDR_RANGE   If the range was outside of the IO reservation.
 * \retval VMK_BUSY             VPN already in use.
 *
 ******************************************************************************
 */

VMK_ReturnStatus 
vmk_LargeSpaceMap(vmk_LargeSpaceHandle lHandle,
                  vmk_VPN vpn,
                  vmk_MPN mpn,
                  vmk_LargeSpaceMapFlags mapFlags,
                  vmk_IOReservation reservation,
                  vmk_uint64 numPages);


/*
 ******************************************************************************
 * vmk_LargeSpaceUnmap --                                                */ /**
 *
 * \brief Unmap a mapping created by vmk_LargeSpaceMap
 *
 * \note This function is only callable from a regular execution context
 * \note This function may block.
 *
 * \param[in]  lHandle      Large Space handle from vmk_LargeSpaceReserve
 * \param[in]  vpn          Virtual Page Number previously given to
 *                          vmk_LargeSpaceMap
 * \param[in]  numPages     Number of virtual pages to unmap, in 4KB Pages
 * \param[in]  mapFlags     Either VASPACE_MAP_FLAG_LARGE_PAGE or
 *                          VASPACE_MAP_FLAG_NONE
 *
 * \retval VMK_OK           If the range was successfully unmapped.
 * \retval VMK_NOT_FOUND    Specified range was not mapped.
 * \retval VMK_BAD_PARAM    Invalid numPages, mapFlags or handle.
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_LargeSpaceUnmap(vmk_LargeSpaceHandle lHandle,
                    vmk_VA vpn,
                    vmk_uint64 numPages,
                    vmk_LargeSpaceMapFlags mapFlags);


/*
 ******************************************************************************
 * vmk_LargeSpaceAllocSize --                                            */ /**
 *
 * \brief Returns the size necessary for a vmk_LargeSpaceHandle
 *
 * \note This function will not block.
 *
 * \retval Allocation size
 *
 ******************************************************************************
 */

vmk_ByteCountSmall
vmk_LargeSpaceAllocSize(void);


#endif
/** @} */
/** @} */
