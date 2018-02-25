/* **********************************************************
 * Copyright 1998 - 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * Please consult with the VMKernel hardware and core teams before making any
 * binary incompatible changes to this file!
 */

/*
 ***********************************************************************
 * PCI                                                            */ /**
 * \addtogroup Device
 * @{
 * \defgroup PCI PCI
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PCI_H_
#define _VMKAPI_PCI_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "hardware/vmkapi_intr.h"

#define VMK_PCI_BUS_NAME "pci"

/** \brief Maximum number of characters in a PCI device name. */
#define VMK_PCI_DEVICE_NAME_LENGTH 32

/** \brief Per-system max for segments */
#define VMK_PCI_NUM_SEGS           256
/** \brief Per-system max for busses */
#define VMK_PCI_NUM_BUSES          256
/** \brief Per-bus max for slots    */          
#define VMK_PCI_NUM_SLOTS          32
/** \brief Per-slot max for functions   */
#define VMK_PCI_NUM_FUNCS          8
/** \brief Per-device max for BARs */
#define VMK_PCI_NUM_BARS           6
/** \brief Per-device max for BARs on a bridge */
#define VMK_PCI_NUM_BARS_BRIDGE    2

/**
 * \brief PCI BAR flags.
 */
typedef enum vmk_PCIBARFlags {
   VMK_PCI_BAR_FLAGS_IO = 0x1,
   VMK_PCI_BAR_FLAGS_MEM_64_BITS = 0x4,
   VMK_PCI_BAR_FLAGS_MEM_PREFETCHABLE = 0x8,
   VMK_PCI_BAR_FLAGS_IO_MASK = 0x3,
   VMK_PCI_BAR_FLAGS_MEM_MASK = 0xF,
} vmk_PCIBARFlags;

/**
 * \brief PCI device identifier.
 *
 */
typedef struct vmk_PCIDeviceID {
   vmk_uint16 vendorID;
   vmk_uint16 deviceID;
   vmk_uint16 subVendorID;
   vmk_uint16 subDeviceID;
   vmk_uint16 classCode;
   vmk_uint16 progIFRevID;
} vmk_PCIDeviceID;

/**
 * \brief PCI device Address (SBDF).
 *
 */
typedef struct vmk_PCIDeviceAddr {
   vmk_uint16   seg;
   vmk_uint8    bus;
   vmk_uint8    dev:5;
   vmk_uint8    fn:3;
} vmk_PCIDeviceAddr;

/**
 * \brief PCI device resource.
 *
 */
typedef struct vmk_PCIResource {
   /** \brief Resource's physical address. */
   vmk_MA start;
   /** \brief Resource size in bytes. */
   vmk_ByteCount size;
   /** \brief Resource flags. */
   vmk_uint64 flags;
} vmk_PCIResource;

/**
 * \brief Configuration space access types
 */
typedef enum vmk_PCIConfigAccess {
   VMK_PCI_CONFIG_ACCESS_8   = 1,
   VMK_PCI_CONFIG_ACCESS_16  = 2,
   VMK_PCI_CONFIG_ACCESS_32  = 4
} vmk_PCIConfigAccess;

/*
 ***********************************************************************
 * vmk_PCIRemoveVF --                                           */ /**
 *
 * \ingroup PCI
 * \brief Remove a PCI virtual function (VF) device.
 *
 * Physical function (PF) driver should carry out any operations 
 * required for the removal of a virtual function device, and 
 * unregister the virtual function device using vmk_PCIUnregisterVF().
 *
 * \param[in]  vf   Handle to PCI VF to be removed.
 *
 * \retval VMK_OK          Success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PCIRemoveVF)(vmk_PCIDevice vf);

/**
 * \brief PCI virtual function (VF) device operations.
 */
typedef struct {
   /** \brief Remove a PCI virtual function from the system. */
   vmk_PCIRemoveVF removeVF;
} vmk_PCIVFDeviceOps;

/**
 ***********************************************************************
 * vmk_PCIQueryDeviceID --                                        */ /**
 *
 * \ingroup PCI
 * \brief Query PCI Device's identifier information.
 *
 * Upon successful completion, devID structure is filled with the
 * identifier information.
 *
 * \param[in]  pciDevice  Pointer to PCI device handle.
 * \param[out] devID      Pointer to PCI Device ID struct.
 *
 * \retval VMK_BAD_PARAM  pciDevice or devID is invalid.
 * \retval VMK_OK         Query successfully processed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIQueryDeviceID(vmk_PCIDevice pciDevice,
                                      vmk_PCIDeviceID *devID);

/**
 ***********************************************************************
 * vmk_PCIQueryDeviceAddr --                                      */ /**
 *
 * \ingroup PCI
 * \brief Query PCI Device's Address (SBDF) information.
 *
 * Upon successful operation, sbdf struct is filled with device's
 * seg, bus, dev, fn information.
 *
 * \param[in]  pciDevice   Pointer to PCI device handle.
 * \param[out] sbdf        Pointer to PCI Device Addr struct.
 *
 * \retval VMK_BAD_PARAM   pciDevice or sbdf is invalid.
 * \retval VMK_OK          Query successfully processed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIQueryDeviceAddr(vmk_PCIDevice pciDevice,
                                        vmk_PCIDeviceAddr *sbdf);

/**
 ***********************************************************************
 * vmk_PCIQueryIOResources --                                     */ /**
 *
 * \ingroup PCI
 * \brief Query PCI Device's BAR resources information.
 *
 * Upon successful completion, resources array is filled with device's
 * first numBars BARs [0 - numBars) information. The resources array
 * passed in should be able to hold numBars resources.
 *
 * \param[in]  pciDevice     Pointer to PCI device handle.
 * \param[in]  numBars       Number of resources queried.
 * \param[out] resources     Pointer to PCI resources array.
 *
 * \retval VMK_BAD_PARAM     pciDevice or resources is invalid.
 * \retval VMK_BAD_PARAM     numBars is greater than VMK_PCI_NUM_BARS
 * \retval VMK_OK            Query successfully processed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIQueryIOResources(vmk_PCIDevice pciDevice,
                                         vmk_uint8 numBars,
                                         vmk_PCIResource resources[]);

/*
 ***********************************************************************
 * vmk_PCIFindCapability --                                       */ /**
 *
 * \ingroup PCI
 * \brief Returns the offset of the capability in device's config space.
 *
 * \param[in] device       PCI device handle
 * \param[in] capID        Capability ID of interest
 * \param[out] capOffset   Pointer to the capability offset
 *
 * \retval VMK_BAD_PARAM If capOffset is NULL or device handle is invalid
 * \retval VMK_NOT_FOUND capability is not found
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIFindCapability(vmk_PCIDevice device,
                                       vmk_uint8 capID,
                                       vmk_uint16 *capOffset);

/*
 ***********************************************************************
 * vmk_PCIReadConfig --                                           */ /**
 *
 * \ingroup PCI
 * \brief Read config space of device at offset.
 *
 * \param[in] moduleID     Module performing the config read.
 * \param[in] device       PCI device handle.
 * \param[in] accessType   Access 1, 2 or 4 bytes.
 * \param[in] offset       Offset to read.
 * \param[out] data        Pointer to the data read.
 *
 * \note Offset is expected to meet the alignment requirements of the
 *       specified access type.
 *
 * \retval VMK_BAD_PARAM      Device handle is NULL/invalid.
 * \retval VMK_BAD_PARAM      Data pointer is NULL.
 * \retval VMK_BAD_PARAM      Offset is not aligned for the access type.
 * \retval VMK_BAD_PARAM      AccessType is not a valid access type.
 * \retval VMK_NO_PERMISSION  Module does not have permission to perform
 *                            this operation on the device.
 * \retval VMK_LIMIT_EXCEEDED Offset is beyond available config space.
 * \retval VMK_FAILURE        Failure to access config space.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIReadConfig(vmk_ModuleID moduleID,
                                   vmk_PCIDevice device,
                                   vmk_PCIConfigAccess accessType,
                                   vmk_uint16 offset,
                                   vmk_uint32 *data);

/*
 ***********************************************************************
 * vmk_PCIWriteConfig --                                          */ /**
 *
 * \ingroup PCI
 * \brief Write data to config space of device at offset.
 *
 * \param[in] moduleID     Module performing the config write.
 * \param[in] device       PCI device handle.
 * \param[in] accessType   Access 1, 2 or 4 bytes.
 * \param[in] offset       Offset to write at.
 * \param[in] data         Data to write.
 *
 * \note Offset is expected to meet the alignment requirements of the
 *       specified access type.
 * \note If the caller needs to wait for the write to propagate to the
 *       device before proceeding, it must explicitly read back the
 *       modified data using vmk_PCIReadConfig.
 *
 * \retval VMK_BAD_PARAM      Device handle is NULL/invalid.
 * \retval VMK_BAD_PARAM      Offset is not aligned for the access type.
 * \retval VMK_BAD_PARAM      AccessType is not a valid access type.
 * \retval VMK_NO_PERMISSION  Module does not have permission to perform
 *                            this operation on the device.
 * \retval VMK_LIMIT_EXCEEDED Offset is beyond available config space.
 * \retval VMK_FAILURE        Failure to access config space.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIWriteConfig(vmk_ModuleID moduleID,
                                    vmk_PCIDevice device,
                                    vmk_PCIConfigAccess accessType,
                                    vmk_uint16 offset,
                                    vmk_uint32 data);

/*
 ***********************************************************************
 * vmk_PCIMapIOResource --                                        */ /**
 *
 * \ingroup PCI
 * \brief Reserve and map PCI IO/Memory space described by pciBar.
 *
 * Reserves the specified BAR with resource manager and maps the memory
 * described by pciBar to a virtual address.
 *
 * \param[in]  moduleID        Module requesting the resource.
 * \param[in]  device          PCI device handle.
 * \param[in]  pciBar          PCI bar to map, must be [0 - 5].
 * \param[out] reservation     IOReservation handle. Only necessary for
 *                             VMK_PCI_BAR_FLAGS_IO type BARs.
 * \param[out] mappedAddress   Pointer to hold the virtual address of
 *                             the mapping.
 *
 * \retval VMK_BAD_PARAM         Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM         PciBar is invalid
 * \retval VMK_BAD_PARAM         reservation is NULL for PIO BAR
 * \retval VMK_BAD_PARAM         MappedAddres is NULL 
 * \retval VMK_NO_RESOURCES      Resource is not owned by device
 * \retval VMK_NO_PERMISSION     Module does not have permission to
 *                               perform this operation on the device.
 * \retval VMK_BAD_PARAM         PciBar is 2nd half of 64-bit MMIO BAR
 * \retval VMK_BAD_PARAM         PciBar addr/size too big for 32-bit MPN
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIMapIOResource(vmk_ModuleID moduleID,
                                      vmk_PCIDevice device,
                                      vmk_uint8 pciBar,
                                      vmk_IOReservation *reservation,
                                      vmk_VA *mappedAddress);

/*
 ***********************************************************************
 * vmk_PCIUnmapIOResource --                                      */ /**
 *
 * \ingroup PCI
 * \brief Unmap and release the established PCI IO/Memory mapping.
 *
 * Upon successful return from the call, the mapping is no longer valid.
 *
 * \param[in]  moduleID        Module requested the resource.
 * \param[in]  device          PCI device handle.
 * \param[in]  pciBar          PCI bar to unmap.
 *
 * \retval VMK_BAD_PARAM         Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM         PciBar is invalid
 * \retval VMK_BAD_PARAM         Resource is not mapped
 * \retval VMK_NO_PERMISSION     Module does not have permission to
 *                               perform this operation on the device.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIUnmapIOResource(vmk_ModuleID moduleID,
                                        vmk_PCIDevice device,
                                        vmk_uint8 pciBar);

/**
 * Interrupt related functions.
 *
 * Driver should first allocate interrupt(s), register and enable the
 * interrupt(s).
 */

/*
 ***********************************************************************
 * vmk_PCIAllocIntrCookie --                                      */ /**
 *
 * \ingroup PCI
 * \brief Allocate interrupt resources for the specified device.
 *
 * On successful return, intrArray will contain the vmk_IntrCookie(s)
 * representing the allocated interrupt resources. numIntrsAlloced
 * will contain the number of interrupts actually allocated.
 *
 * \note If type is VMK_PCI_INTERRUPT_TYPE_LEGACY or type is
 *       VMK_PCI_INTERRUPT_TYPE_MSI then numIntrsDesired and
 *       numIntrsRequired must both be set to 1.
 * \note The caller must provide memory for numIntrsDesired
 *       vmk_IntrCookie(s) in the space referenced by intrArray.
 * \note For MSIX interrupts, the caller specifies the minimum
 *       number of interrupts required in numIntrRequired. An
 *       attempt is made to allocate as many interrupts are possible,
 *       up to numIntrsDesired. If at least numIntrsRequired
 *       interrupts cannot be allocated, an error is returned and
 *       no interrupts are allocated. The caller should specify
 *       the number of interrupts required taking the device
 *       capabilities into account; requiring more interrupts than
 *       the device can support will always return an error.
 * \note The caller can specify the index into MSIX table for the
 *       desired interrupts using indexAarray. indexArray can be NULL
 *       if the caller does not care. If non-NULL, it should specify
 *       index for numIntrDesired entries, and intrArray is filled
 *       with allocated interrupt for requested index values.
 *
 * \param[in]  moduleID          Module allocating the interrupts.
 * \param[in]  device            PCI device handle.
 * \param[in]  type              Interrupt type.
 * \param[in]  numIntrsDesired   Number of interrupts desired. 
 *                               (must be >= numIntrsRequired)
 * \param[in]  numIntrsRequired  Number of interrupts required.
 * \param[in]  indexArray        Array of MSIX table index.
 * \param[out] intrArray         Array of interrupts allocated.
 * \param[out] numIntrsAlloced   Number of interrupts allocated.
 *
 * \retval VMK_BAD_PARAM         props argument is NULL.
 * \retval VMK_BAD_PARAM         Device handle is invalid/NULL.
 * \retval VMK_BAD_PARAM         Device is not interruptive.
 * \retval VMK_BAD_PARAM         type is VMK_INTERRUPT_TYPE_PCI_LEGACY
 *                               the ioapic pin the device is connected
 *                               to is not described in interrupt
 *                               routing tables.
 * \retval VMK_BAD_PARAM         type is VMK_INTERRUPT_TYPE_PCI_LEGACY
 *                               or VMK_INTERRUPT_TYPE_PCI_MSI and
 *                               numIntrsDesired or numIntrsRequired
 *                               is not equal to 1.
 * \retval VMK_BAD_PARAM         type is VMK_INTERRUPT_TYPE_PCI_MSIX and
 *                               numIntrsAlloced is NULL.
 * \retval VMK_BAD_PARAM         type is VMK_INTERRUPT_TYPE_PCI_MSIX and
 *                               numIntrsRequired > numIntrsDesired
 * \retval VMK_BAD_PARAM         All other mal-formed props.
 * \retval VMK_NO_MEMORY         Internal memory allocation failure
 * \retval VMK_NOT_SUPPORTED     Device/platform doesn't support MSI or
 *                               MSIX if request is for MSI or MSIX type
 * \retval VMK_NO_PERMISSION     Module does not have permission to
 *                               perform this operation on the device.
 * \retval VMK_IO_ERROR          PCI Bus error, probably device is
 *                               broken or unplugged
 * \retval VMK_NO_RESOURCES      For MSIX type, the number of interrupts
 *                               available is less than numIntrsRequired.
 * \retval VMK_FAILIURE          All other errors.
 * \retval VMK_OK                Successfully allocated the interrupts.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIAllocIntrCookie(vmk_ModuleID moduleID,
                                        vmk_PCIDevice device,
                                        vmk_PCIInterruptType type,
                                        vmk_uint32 numIntrsDesired,
                                        vmk_uint32 numIntrsRequired,
                                        vmk_uint16 *indexArray,
                                        vmk_IntrCookie *intrArray,
                                        vmk_uint32 *numIntrsAlloced);

/*
 ***********************************************************************
 * vmk_PCIFreeIntrCookie --                                       */ /**
 *
 * \ingroup PCI
 * \brief Free all interrupts that were previously allocated by a call
 *        to vmk_PCIAllocIntrCookie().
 *
 * \param[in] moduleID      Module that allocated the interrupts
 * \param[in] device        PCI device handle
 *
 * \retval VMK_BAD_PARAM        Module ID is invalid.
 * \retval VMK_BAD_PARAM        Device handle is invalid/NULL.
 * \retval VMK_NO_PERMISSION    Module does not have permission to
 *                              perform this operation on the device.
 * \retval VMK_OK               Successfully freed the interrupts.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIFreeIntrCookie(vmk_ModuleID moduleID,
                                       vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIEnableVFs --                                            */ /**
 *
 * \ingroup PCI
 * \brief Enable virtual functions in an SR-IOV physical function.
 *
 * numVFs is the number of virtual functions to be enabled. Setting it to
 * zero requests that all virtual functions (VFs) under the given physical
 * function (PF) be enabled. The actual number of VFs successfully enabled
 * is returned in numVFs upon success.
 *
 * \note For a native PF driver, this function must be called from the
 * driver's vmk_DriverAttachDevice() callback. This will ensure VFs are
 * enabled before the driver receives the vmk_DriverScanDevice() callback,
 * and also prevent VFs from being re-enabled if the PF is quiesced
 * and re-started.
 *
 * \note This function creates a vmk_PCIDevice handle for each VF. The
 * handle of each VF can be retrieved via the function
 * vmk_PCIGetVFPCIDevice().
 *
 * \param[in]     pf     PCI device handle of the SR-IOV PF.
 * \param[in,out] numVFs Number of VFs to enable/enabled.
 *
 * \retval VMK_BAD_PARAM Device handle is invalid or device does not exist.
 * \retval VMK_FAILURE   Cannot enable virtual functions.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIEnableVFs(vmk_PCIDevice pf,
                 vmk_uint16 *numVFs);

/*
 ***********************************************************************
 * vmk_PCIDisableVFs --                                           */ /**
 *
 * \ingroup PCI
 * \brief Disable all virtual functions in an SR-IOV physical function.
 *
 * \note For a native PF driver, this function must be called from the
 * driver's vmk_DriverDetachDevice() callback. This will ensure that
 * all virtual functions (VFs) being disabled have been un-registered
 * with the vmkkernel's device layer (i.e., unregistration occurs in the
 * PF driver's VF remove callback which occurs prior to
 * vmkDriverDetachDevice()). In addition, this will also prevent VFs from
 * being disabled if the PF is quiesced but not detached.
 *
 * \param[in] pf         PCI device handle of physical function (PF).
 *
 * \retval VMK_BAD_PARAM Device handle is invalid or device does not exist.
 * \retval VMK_FAILURE   Device is not in SR-IOV mode.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDisableVFs(vmk_PCIDevice pf);

/*
 ***********************************************************************
 * vmk_PCIGetVFPCIDevice --                                       */ /**
 *
 * \ingroup PCI
 * \brief Retrieves the PCI device handle of a virtual function (VF).
 *
 * \note This function should only be called on enabled VFs (see
 * vmk_PCIEnableVF()).
 *
 * \param[in]  pf        PCI device handle of the parent physical function.
 * \param[in]  vfIndex   Index of the virtual function.
 * \param[out] vf        PCI device handle of the requested virtual function.
 *
 * \retval VMK_BAD_PARAM PF device handle is invalid or device does not exist.
 * \retval VMK_BAD_PARAM VF device handle argument is NULL.
 * \retval VMK_NOT_FOUND VF doesn't exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIGetVFPCIDevice(vmk_PCIDevice pf,
                      vmk_uint16 vfIndex,
                      vmk_PCIDevice *vf);

/*
 ***********************************************************************
 * vmk_PCIGetPFPCIDevice --                                       */ /**
 *
 * \ingroup PCI
 * \brief Retrieve the PCI device handle of a PF, given the VF handle.
 *
 * \param[in] vf        PCI device handle of the virtual function.
 * \param[out] pf       PCI device handle of the parent physical function.
 *
 * \retval VMK_BAD_PARAM VF device handle is invalid or device does not exist.
 * \retval VMK_BAD_PARAM PF device handle argument is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIGetPFPCIDevice(vmk_PCIDevice vf, vmk_PCIDevice *pf);

/*
 ************************************************************************
 * vmk_PCIRegisterVF --                                            */ /**
 *
 * \ingroup PCI
 * \brief Register virtual functions with the vmkernel's device layer.
 *
 * The devOps parameter is the vmk_DeviceOps structure associated with the
 * registered virtual function (VF).
 *
 * \note This function is meant to be used by native PF drivers only.
 * Since this function registers a VF with the vmkernel's device layer,
 * all restrictions associated with device registration apply.
 *
 * \note Before this call returns, the vmkernel's device layer may attach
 * the VF to a driver.
 *
 * \param[in] vf              PCI device handle of the VF to be registered.
 * \param[in] pf              PCI device handle of the parent PF.
 * \param[in] pfDriverHandle  PF driver handle.
 * \param[in] vfDevOps        Device operations associated with a registered VF.
 *
 * \retval VMK_BAD_PARAM VF device handle is invalid.
 * \retval VMK_BAD_PARAM PF device handle is invalid.
 * \retval VMK_FAILURE   VF registration failed.
 *
 ************************************************************************
 */
VMK_ReturnStatus
vmk_PCIRegisterVF(vmk_PCIDevice vf,
                  vmk_PCIDevice pf,
                  vmk_Driver pfDriverHandle,
                  vmk_PCIVFDeviceOps *vfDevOps);

/*
 ************************************************************************
 * vmk_PCIUnregisterVF --                                          */ /**
 *
 * \ingroup PCI
 * \brief Unregister a virtual function from the vmkernel's device layer.
 *
 * \note Since this function unregisters a virtual function (VF) with the
 * vmkernel's device layer, all restrictions associated with device
 * unregistration apply (unregistration of the VF must be done
 * by the physical function driver from the vmk_DeviceRemove function in the
 * vmk_DeviceOps passed during VF registration).
 *
 * \param[in] vf         PCI device handle of the VF to be unregistered.
 *
 * \retval VMK_BAD_PARAM VF device handle is invalid.
 * \retval VMK_FAILURE   VF unregistration failed.
 *
 *************************************************************************
 */
VMK_ReturnStatus
vmk_PCIUnregisterVF(vmk_PCIDevice vf);

/*
 *************************************************************************
 * vmk_PCISetVFPrivateData --                                       */ /**
 *
 * \ingroup PCI
 * \brief Associate private data with a virtual function.
 *
 * Associates a pointer to the given user-defined private data with the
 * given virtual function (VF). The pointer may retrieved via the
 * function vmk_PCIGetVFPrivateData(). This provides a mechanism by which
 * vmkernel components, typically a PF driver and a VF driver, can exchange
 * private data.
 *
 * \param[in] vf           PCI device handle of the VF.
 * \param[in] privateData  User-defined private data.
 *
 * \retval VMK_BAD_PARAM VF device handle is invalid.
 *
 ************************************************************************
 */
VMK_ReturnStatus
vmk_PCISetVFPrivateData(vmk_PCIDevice vf,
                        vmk_AddrCookie privateData);

/*
 *************************************************************************
 * vmk_PCIGetVFPrivateData --                                       */ /**
 *
 * \ingroup PCI
 * \brief Retrieve private data associated with a virtual function.
 *
 * Retrieves a pointer to the user-defined private data associated with
 * the given virtual function (VF). See the description of
 * vmk_PCISetVFPrivateData() for further details.
 *
 * \param[in] vf            PCI device handle of the VF.
 * \param[out] privateData  User-defined private data.
 *
 * \retval VMK_BAD_PARAM VF device handle is invalid.
 * \retval VMK_BAD_PARAM privateData pointer is NULL.
 *
 ************************************************************************
 */
VMK_ReturnStatus
vmk_PCIGetVFPrivateData(vmk_PCIDevice vf,
                        vmk_AddrCookie *privateData);

/*
 ***********************************************************************
 * vmk_PCIEnablePME --                                            */ /**
 *
 * \ingroup PCI
 * \brief Enable PME# generation.
 *
 * Enable PME# generation if the device is capable of asserting
 * the PME# signal from any of the power states.
 *
 * \param[in] moduleID     Module requesting the operation.
 * \param[in] device       PCI device handle.
 *
 * \retval VMK_BAD_PARAM     Module ID is invalid.
 * \retval VMK_BAD_PARAM     Device handle is NULL/invalid.
 * \retval VMK_NO_PERMISSION Module does not have permission to perform
 *                           this operation on the device.
 * \retval VMK_NOT_SUPPORTED Device has no PM capability.
 * \retval VMK_NOT_SUPPORTED Device doesn't support PME# generation
 *                           from any power state.
 *
 ***********************************************************************
 */ 
VMK_ReturnStatus
vmk_PCIEnablePME(vmk_ModuleID moduleID,
                 vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIDisablePME --                                           */ /**
 *
 * \ingroup PCI
 * \brief Disable PME# generation.
 *
 * Disable PME# generation if the device is capable of asserting
 * the PME# signal from any of the power states.
 *
 * \param[in] moduleID     Module requesting the operation.
 * \param[in] device       PCI device handle.
 *
 * \retval VMK_BAD_PARAM     Module ID is invalid.
 * \retval VMK_BAD_PARAM     Device handle is NULL/invalid.
 * \retval VMK_NO_PERMISSION Module does not have permission to perform
 *                           this operation on the device.
 * \retval VMK_NOT_SUPPORTED Device has no PM capability.
 * \retval VMK_NOT_SUPPORTED Device doesn't support PME# generation
 *                           from any power state.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDisablePME(vmk_ModuleID moduleID,
                  vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIDevIteratorAlloc --                                     */ /**
 *
 * \ingroup PCI
 * \brief Allocates a PCI device iterator object.
 *
 * \param[in] heap           Heap used for allocation.
 * \param[in] type           Iterator type.
 * \param[out] iterator      Allocated iterator object.
 *
 * \retval VMK_BAD_PARAM type is invalid.
 * \retval VMK_BAD_PARAM iterator parameter is NULL.
 * \retval VMK_FAILURE   Failed to allocate iterator object.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDevIteratorAlloc(vmk_HeapID heap,
                        vmk_PCIDevIteratorType type,
                        vmk_PCIDevIterator *iterator);

/*
 ***********************************************************************
 * vmk_PCIDevIteratorFree --                                      */ /**
 *
 * \ingroup PCI
 * \brief Frees a given PCI device iterator object.
 *
 * \param[in] iterator       PCI device iterator.
 *
 * \retval VMK_BAD_PARAM iterator is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDevIteratorFree(vmk_PCIDevIterator iterator);

/*
 ***********************************************************************
 * vmk_PCIDevIteratorGetFirst --                                  */ /**
 *
 * \ingroup PCI
 * \brief Returns the first PCI device in the list of PCI devices.
 *
 * Returns the handle for the first device in an iteration over the
 * list of PCI devices in the system. If there are no more devices in
 * the list, returns NULL.
 *
 * \note A driver is allowed to read from PCI devices to which it
 *       is not attached, but must never modify the state of these
 *       devices (i.e., it must not write to the device or perform
 *       reads that have side-effects).
 *
 * \param[in] iterator PCI device list iterator.
 * \param[out] dev     First PCI device, or NULL if no PCI devices
 *                     are found.
 *
 * \retval VMK_BAD_PARAM iterator is NULL.
 * \retval VMK_BAD_PARAM dev is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDevIteratorGetFirst(vmk_PCIDevIterator iterator,
                           vmk_PCIDevice *dev);

/*
 ***********************************************************************
 * vmk_PCIDevIteratorGetNext --                                   */ /**
 *
 * \ingroup PCI
 * \brief Returns the next PCI device in the list of PCI devices.
 *
 * Returns the handle for the next device in an iteration over the
 * list of PCI devices in the system. If there are no more devices in
 * the list, returns NULL.
 *
 * \note A driver is allowed to read from PCI devices to which it
 *       is not attached, but must never modify the state of these
 *       devices (i.e., it must not write to the device or perform
 *       reads that have side-effects).
 *
 * \param[in] iterator PCI device list iterator.
 * \param[out] dev     Next PCI device, or NULL if no more PCI
 *                     devices are found.
 *
 * \retval VMK_BAD_PARAM iterator is NULL.
 * \retval VMK_BAD_PARAM dev is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDevIteratorGetNext(vmk_PCIDevIterator iterator,
                          vmk_PCIDevice *dev);



#endif /* _VMKAPI_PCI_H_ */
/** @} */
/** @} */
