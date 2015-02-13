/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
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

#include "device/vmkapi_vector.h"

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
/** \brief Per-slot max for primary interrupt pins */
#define VMK_PCI_NUM_PINS           4
/** \brief Per-device max for BARs */
#define VMK_PCI_NUM_BARS           6
/** \brief Per-device max for BARs on a bridge */
#define VMK_PCI_NUM_BARS_BRIDGE    2

/**
 * \brief PCI device owner.
 */
typedef enum vmk_PCIDeviceOwner {
   VMK_PCI_DEVICE_OWNER_UNKNOWN = 0,
   VMK_PCI_DEVICE_OWNER_HOST = 1,
   VMK_PCI_DEVICE_OWNER_KERNEL = 2,
   VMK_PCI_DEVICE_OWNER_MODULE = 3,
   VMK_PCI_DEVICE_OWNER_VM = 4,
} vmk_PCIDeviceOwner;

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
 * \brief BAR descriptor.
 */
typedef struct vmk_PCIBARInfo {
   /** BAR base address */
   vmk_MA address;

   /** BAR size (in bytes) */
   vmk_ByteCount size;
   
   /** BAR flags */
   vmk_PCIBARFlags flags;
} vmk_PCIBARInfo;

/**
 * \brief Information about a PCI device.
 *
 * VMK_PCI_NUM_BARS is the maximum number of BARs as per the PCI spec.
 * The actual valid number of BARs depends on the device type;
 * for bridges, this number is VMK_PCI_NUM_BARS_BRIDGE and for
 * non-bridges, it is VMK_PCI_NUM_BARS.
 */
typedef struct vmk_PCIDeviceInfo {
   vmk_uint16 vendorID;
   vmk_uint16 deviceID;
   vmk_uint16 classCode;
   vmk_uint16 progIFRevID;
   vmk_uint16 subVendorID;
   vmk_uint16 subDeviceID;
   vmk_uint8  hdrType;
   vmk_uint8  intLine;
   vmk_uint8  intPin;

   /** \brief Spawned bus number if device is a PCI bridge. */
   vmk_uint8  spawnedBus;

   /** \brief Current owner of the device */
   vmk_PCIDeviceOwner owner;

   /** \brief Device BARs */
   vmk_PCIBARInfo bars[VMK_PCI_NUM_BARS];
} vmk_PCIDeviceInfo;

/**
 * \brief A PCI device capability value.
 */
typedef struct vmk_PCICapability {
   vmk_uint16  capIdx; 
   vmk_uint8   capType;
} vmk_PCICapability;

/**
 * \brief Configuration space access types
 */
typedef enum vmk_ConfigSpaceAccess{
   VMK_ACCESS_8   = 1,
   VMK_ACCESS_16  = 2,
   VMK_ACCESS_32  = 4
} vmk_ConfigSpaceAccess;

/**
 * \brief Opaque map handle
 */
typedef struct vmk_PCIMapHandleInt *vmk_PCIMapHandle;

/**
 * \brief Type of interrupt triggering
 */
typedef enum vmk_PCIInterruptTriggerType {
   VMK_PCI_INTERRUPT_TRIGGER_NONE=0,
   VMK_PCI_INTERRUPT_TRIGGER_LEVEL=1,
   VMK_PCI_INTERRUPT_TRIGGER_EDGE=2,
} vmk_PCIInterruptTriggerType;

/**
 * \brief interrupt information
 */
typedef struct vmk_PCIInterruptInfo {
   /** Device-specific interrupt info */
   void *specific;

   /** Vmkernel vector */
   vmk_uint32 vector;

   /** Type of interrupt */
   vmk_PCIInterruptType type;

   /** Type of interrupt triggering */
   vmk_PCIInterruptTriggerType triggering;
} vmk_PCIInterruptInfo;

/**
 * \brief Device callback reason
 */
typedef enum vmk_PCIDeviceCallbackReason {
      VMK_PCI_DEVICE_INSERTED             = 0,
      VMK_PCI_DEVICE_REMOVED              = 1,
      VMK_PCI_DEVICE_CHANGED_OWNER        = 2,
} vmk_PCIDeviceCallbackReason;

/**
 * \brief Argument passed to a PCI event callback
 */
typedef struct vmk_PCIDeviceCallbackArg {
   /* Reason for callback */
   vmk_PCIDeviceCallbackReason reason;

   /* Data for callback */
   union {
      struct {
         vmk_PCIDeviceOwner old;
         vmk_PCIDeviceOwner new;
      } changedOwner;
   } data;
} vmk_PCIDeviceCallbackArg;

/**
 * \brief Callback to handle PCI events.
 */
typedef void (*vmk_PCICallback)(vmk_PCIDevice device,
                                vmk_PCIDeviceCallbackArg *callbackArgs,
                                void *callbackPrivate);

/*
 ***********************************************************************
 * vmk_PCIGetDevice --                                            */ /**
 *
 * \ingroup PCI
 * \brief Retrieves vmk_PCIDevice that corresponds to the generic
 *        device genDevice.
 *
 * \param[in]  genDevice   Generic device handle.
 * \param[out] pciDevice   Pointer to PCI device handle.
 *
 * \retval VMK_BAD_PARAM genDevice is invalid
 * \retval VMK_OK        PCI device handle is successfully returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetDevice(vmk_Device genDevice,
                                  vmk_PCIDevice *pciDevice);

/*
 ***********************************************************************
 * vmk_PCIGetGenDevice --                                         */ /**
 *
 * \ingroup PCI
 * \brief Retrieves generic vmk_Device that corresponds to the PCI
 *        device pciDevice.
 *
 * \param[in]  pciDevice   PCI device handle.
 * \param[out] genDevice   Pointer to generic device handle.
 *
 * \retval VMK_BAD_PARAM   pciDevice is invalid
 * \retval VMK_OK          Generic device handle is successfully
 *                         returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetGenDevice(vmk_PCIDevice pciDevice,
                                     vmk_Device *genDevice);

/*
 ***********************************************************************
 * vmk_PCIGetPCIDevice --                                            */ /**
 *
 * \ingroup PCI
 * \brief Retrieves vmk_PCIDevice handle that corresponds to segment,
 *        bus, device and function
 *
 * \param[in]  segment   PCI segment
 * \param[in]  bus       Bus number
 * \param[in]  dev       Device number
 * \param[in]  func      Function number
 * \param[out] device    PCI device handle
 *
 * \retval VMK_NOT_FOUND Device with bus, device, number does not exist
 * \retval VMK_BAD_PARAM device argument is NULL
 * \retval VMK_OK        device handle is successfully returned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetPCIDevice(const vmk_uint16 segment,
                                     const vmk_uint8 bus,
                                     const vmk_uint8 dev,
                                     const vmk_uint8 func,
                                     vmk_PCIDevice *device);

/*
 ***********************************************************************
 * vmk_PCIGetSegmentBusDevFunc --                                 */ /**
 *
 * \ingroup PCI
 * \brief Return device's bus, device, func and function
 *
 * \param[in]  device     PCI device handle
 * \param[out] segment    Pointer to Segment number
 * \param[out] bus        Pointer to Bus number
 * \param[out] dev        Pointer to Device number
 * \param[out] func       Pointer to Function number
 *
 * \retval VMK_BAD_PARAM   One or more of bus, dev, func is NULL.
 * \retval VMK_BAD_PARAM   Device handle is invalid or device does
 *                         not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetSegmentBusDevFunc(vmk_PCIDevice device,
                                             vmk_uint16 *segment,
                                             vmk_uint8 *bus,
                                             vmk_uint8 *dev,
                                             vmk_uint8 *func);

/*
 ***********************************************************************
 * vmk_PCIGetIDs --                                               */ /**
 *
 * \ingroup PCI
 * \brief Return device's vendor ID, device ID, sub vendor ID and
 *        sub device iD
 *
 * \param[in] device       PCI device handle
 * \param[out] vendorID    Pointer to vendor ID.
 * \param[out] deviceID    Pointer to device ID.
 * \param[out] subVendorID Pointer to sub vendor ID.
 * \param[out] subDeviceID Pointer to sub device ID.
 *
 * \retval VMK_BAD_PARAM   One or more of vendorID, deviceID
 *                         subVendorID and subDeviceID is NULL.
 * \retval VMK_BAD_PARAM   Device handle is invalid or device does
 *                         not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetIDs(vmk_PCIDevice device,
                               vmk_uint16 *vendorID,
                               vmk_uint16 *deviceID,
                               vmk_uint16 *subVendorID,
                               vmk_uint16 *subDeviceID);

/*
 ***********************************************************************
 * vmk_PCIGetInfo --                                              */ /**
 *
 * \ingroup PCI
 * \brief Fills in the details corresponding to device. 
 *
 * \note The BARs stored in the vmk_PCIDeviceInfo are canonical. User
 * should not read the PCI config registers to obtain the BARs values.
 *
 * \param[in] device       PCI device handle
 * \param[in] deviceInfo   Pointer to vmk_PCIDeviceInfo
 *
 * \retval VMK_BAD_PARAM   DeviceInfo pointer is NULL.
 * \retval VMK_BAD_PARAM   Device handle is invalid or device does
 *                         not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetInfo(vmk_PCIDevice device,
                                vmk_PCIDeviceInfo *deviceInfo);

/*
 ***********************************************************************
 * vmk_PCIGetDeviceList --                                        */ /**
 *
 * \ingroup PCI
 * \brief Returns an array of vmk_PCIDevice handles of all devices with
 *        vendorID and deviceID.
 *
 * The list of devices returned is just a snap shot at the time the call
 * was made. The list can change between invocations and the number of
 * devices may be actually more or less than what is returned to the
 * caller.
 *
 * Only the list of devices with matching vendorID or deviceID are
 * returned. vendorID/deviceID of 0xFFFF matches any vendorID/deviceID.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[in] moduleID     Module ID of the caller
 * \param[in] vendorID     Vendor ID of interest
 * \param[in] deviceID     Device ID of interest
 * \param[out] devices     Pointer to an array of vmk_PCIDevice handles
 * \param[out] numDevices  Pointer to the number of array elements
 *
 * \retval VMK_BAD_PARAM   Devices and/or numDevices ponters are NULL.
 * \retval VMK_NO_MEMORY   Unable to allocate memory to hold the handles
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetDeviceList(vmk_ModuleID moduleID,
                                      vmk_uint16 vendorID,
                                      vmk_uint16 deviceID,
                                      vmk_PCIDevice **devices,
                                      int *numDevices);

/*
 ***********************************************************************
 * vmk_PCISetDeviceName --                                        */ /**
 *
 * \ingroup PCI
 * \brief Sets the device name
 *
 * \param[in] device    PCI device handle
 * \param[in] name      Name to be set
 *
 * \retval VMK_BAD_PARAM Device handle is invalid/non existent or name
 *                       is NULL
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCISetDeviceName(vmk_PCIDevice device,
                                      const char *name);

/*
 ***********************************************************************
 * vmk_PCIGetDeviceName --                                        */ /**
 *
 * \ingroup PCI
 * \brief Gets the device name
 *
 * \param[in]  device      PCI device handle
 * \param[out] name        Pointer to char array to return the name in
 * \param[in]  nameLen     Length of the char array.
 *
 * \retval VMK_BAD_PARAM         Device is invalid or non existent.
 * \retval VMK_BAD_PARAM         Name is NUL or nameLen is greater than
 *                               VMK_PCI_DEVICE_NAME_LENGTH.
 * \retval VMK_DEVICE_NOT_NAMED  Device is not named
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetDeviceName(vmk_PCIDevice device,
                                      char *name,
                                      vmk_ByteCount nameLen);

/*
 ***********************************************************************
 * vmk_PCIGetAlternateName --                                     */ /**
 *
 * \ingroup PCI
 * \brief Gets the alternate name of the device
 *
 * \param[in] device       PCI device handle
 * \param[out] name        Pointer to char array to return the name in
 * \param[in] nameLen      Length of the char array.
 *
 * \retval VMK_BAD_PARAM         Device is invalid or non existent.
 * \retval VMK_BAD_PARAM         Name is NULL or nameLen is greater than
 *                               VMK_PCI_DEVICE_NAME_LENGTH.
 * \retval VMK_DEVICE_NOT_NAMED  Device has no alternate name
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetAlternateName(vmk_PCIDevice device,
                                               char *name,
                                               vmk_ByteCount nameLen);

/*
 ***********************************************************************
 * vmk_PCIGetCapabilityIndex --                                   */ /**
 *
 * \ingroup PCI
 * \brief Returns the offset of the capability in device's config space.
 *
 * \param[in] device       PCI device handle
 * \param[in] capType      Capability of interest
 * \param[out] capIdx      Pointer to the capability index
 *
 * \retval VMK_BAD_PARAM If capIdx is NULL or device handle is NULL/invalid
 * \retval VMK_NOT_FOUND capability is not found
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetCapabilityIndex(vmk_PCIDevice device,
                                           vmk_uint8 capType,
                                           vmk_uint16 *capIdx);

/*
 ***********************************************************************
 * vmk_PCIGetCapabilities --                                      */ /**
 *
 * \ingroup PCI
 * \brief Returns an array of capabilities of the specified device
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[in] moduleID         Caller's module ID
 * \param[in] device           PCI device handle
 * \param[out] capabilities    Pointer to an array of vmk_PCICapability
 *                             structures
 * \param[out] numCapabilities Pointer to the number of array elements
 *
 * \retval VMK_BAD_PARAM Device handle is NULL/invalid.
 * \retval VMK_BAD_PARAM Capabilities or numCapabilities is NULL.
 * \retval VMK_NO_MEMORY Failed to allocate memory to hold capabilities
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 * \retval VMK_OK        Success. Capabilities points to the array of
 *                       vmk_PCICapability structures, the number of
 *                       elements in the array is returned in
 *                       numCapabilities.                       
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetCapabilities(vmk_ModuleID moduleID,
                                        vmk_PCIDevice device,
                                        vmk_PCICapability **capabilities,
                                        int *numCapabilities);
                                       
/*
 ***********************************************************************
 * vmk_PCIGetParent --                                            */ /**
 *
 * \ingroup PCI
 * \brief Returns the parent of device in parent.
 *
 * \param[in] device  PCI device handle
 * \param[out] parent Pointer to parent device handle
 *
 * Returns NULL in parent if device is the root of the hierarchy.
 *
 * \retval VMK_BAD_PARAM Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM Parent is NULL  
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetParent(vmk_PCIDevice device,
                                  vmk_PCIDevice *parent);

/*
 ***********************************************************************
 * vmk_PCIGetChild --                                             */ /**
 *
 * \ingroup PCI
 * \brief Returns the child of device in child.
 *
 * \param[in] device  PCI device handle
 * \param[out] child  Pointer to child device handle
 *
 * Returns NULL in child if device is a leaf in the hierarchy.
 *
 * \retval VMK_BAD_PARAM   Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM   Child is NULL   
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetChild(vmk_PCIDevice device,
                                 vmk_PCIDevice *child);

/*
 ***********************************************************************
 * vmk_PCIGetSibling --                                           */ /**
 *
 * \ingroup PCI
 * \brief Returns the next sibling of device pointed in sibling.
 *
 * \param[in] device    PCI device handle.
 * \param[out] sibling  Pointer to sibling device handle.
 *
 * Sets sibling to NULL if no more siblings.
 *
 * \retval VMK_BAD_PARAM   Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM   Child is sibling   
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetSibling(vmk_PCIDevice device,
                                   vmk_PCIDevice *sibling);

/*
 ***********************************************************************
 * vmk_PCIReadConfigSpace --                                      */ /**
 *
 * \ingroup PCI
 * \brief Read config space of device at offset.
 *
 * \param[in] device       PCI device handle.
 * \param[in] accessType   Access 1, 2 or 4 bytes.
 * \param[in] offset       Offset to read.
 * \param[out] data        Pointer to the data read.
 *
 * \note Offset is expected to meet the alignment requirements of the
 *       specified access type.
 *
 * \retval VMK_BAD_PARAM Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM Data pointer is NULL
 * \retval VMK_BAD_PARAM Offset is not aligned for the access type
 * \retval VMK_BAD_PARAM AccessType is not a valid access type
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIReadConfigSpace(vmk_PCIDevice device,
                                        vmk_ConfigSpaceAccess accessType,
                                        vmk_uint16 offset,
                                        vmk_uint32 *data);

/*
 ***********************************************************************
 * vmk_PCIWriteConfigSpace --                                     */ /**
 *
 * \ingroup PCI
 * \brief Write data to config space of device at offset.
 *
 * \param[in] device       PCI device handle.
 * \param[in] accessType   Access 1, 2 or 4 bytes.
 * \param[in] offset       Offset to write at.
 * \param[in] data         Data to write.
 *
 * \note Offset is expected to meet the alignment requirements of the
 *       specified access type.
 *
 * \retval VMK_BAD_PARAM device handle is NULL/invalid.
 * \retval VMK_BAD_PARAM offset is not aligned for the access type.
 * \retval VMK_BAD_PARAM accessType is not a valid access type.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIWriteConfigSpace(vmk_PCIDevice device,
                                         vmk_ConfigSpaceAccess accessType,
                                         vmk_uint16 offset,
                                         vmk_uint32 data);

/*
 ***********************************************************************
 * vmk_PCIMapIOResource --                                        */ /**
 *
 * \ingroup PCI
 * \brief Map PCI IO/Memory space described by pciBar.
 *
 * Allocates and fills mapHandle that should be used for the subsequent unmap
 * call.
 *
 * \param[in] device          PCI device handle.
 * \param[in] pciBar          PCI bar to map.
 * \param[in] cacheable       Whether the mapping can be in cacheable
 *                            memory.
 * \param[out] mappedAddress  Pointer to the address the BAR is
 *                            mapped at.
 * \param[out] mapHandle      Pointer to the mapping handle; use at
 *                            unmap time.
 *
 * \retval VMK_BAD_PARAM         Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM         PciBar is invalid
 * \retval VMK_BAD_PARAM         MappedAddres is NULL or mapHandle
 *                               is NULL
 * \retval VMK_BAD_PARAM         AccessType is not a valid access type
 * \retval VMK_DEVICE_NOT_OWNED  Device is not owned by kernel
 * \retval VMK_BAD_PARAM         PciBar is second half of 64-bit MMIO BAR
 * \retval VMK_BAD_PARAM         PciBar addr/size are too large for 32-bit MPN
 * \retval VMK_NO_MEMORY         No memory to allocate mapHandle
 * \retval VMK_MAPPING_FAILED    Mapping failed
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIMapIOResource(vmk_PCIDevice device,
                                      vmk_uint8 pciBar,
                                      vmk_Bool cacheable,
                                      vmk_VA *mappedAddress,
                                      vmk_PCIMapHandle *mapHandle);

/*
 ***********************************************************************
 * vmk_PCIReleaseIOResource --                                    */ /**
 *
 * \ingroup PCI
 * \brief Unmap the previously established PCI IO/Memory mapping.
 *
 * Upon successful return from the call, the mapping is no longer valid.
 *
 * \param[in] mapHandle    Mapping handle returned by a previous call
 *                         to vmk_PCIMapIOResource.
 *
 * \retval VMK_BAD_PARAM   mapHandle is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIReleaseIOResource(vmk_PCIMapHandle mapHandle);

/**
 * Interrupt related functions.
 *
 * Driver should first allocate an interrupt vector, add handlers for
 * the vector and then enable the vector.
 */

/*
 ***********************************************************************
 * vmk_PCIAllocateIntVectors --                                   */ /**
 *
 * \ingroup PCI
 * \brief Allocate numVectors of intrType for device.
 *
 * \param[in] device                PCI device handle
 * \param[in] intrType              Interrupt type to allocate
 * \param[in] numVectorsRequested   Number of vectors to allocate
 * \param[in] indexAarray           Array of index that vectors in the
 *                                  vectorArray. Need to be associated
 *                                  with. Used only when intrType is
 *                                  VMK_INTERRUPT_TYPE_PCI_MSIX.
 * \param[in] bestEffortAllocation  Controls MSIX vector allocation
 *                                  behavior. Vmkernel tries to allocate
 *                                  numVectorsRequested vectors but if
 *                                  it is unable to allocate that many
 &                                  vectors and if this flag is VMK_FALSE,
 *                                  this call will return a failure status.
 * \param[out] vectorArray          Array of vectors allocated.
 * \param[out] numVectorsAlloced    Number of vectors actually allocated.
 *
 * \note If intrType is VMK_INTERRUPT_TYPE_PCI_LEGACY, numVectors is
 *       ignored and only 1 vector is allocated.
 *
 * \retval VMK_BAD_PARAM         Vectors argument is NULL.
 * \retval VMK_BAD_PARAM         Device handle is invalid/NULL.
 * \retval VMK_BAD_PARAM         Device is not interruptive.
 * \retval VMK_BAD_PARAM         If intrType is
 *                               VMK_INTERRUPT_TYPE_PCI_LEGACY and
 *                               the ioapic pin the device is connected
 *                               to is not described in interrupt
 *                               routing tables.
 * \retval VMK_BAD_PARAM         If intrType is not
 *                               VMK_INTERRUPT_TYPE_PCI_MSIX
 *                                and numVectors is not equal to 1.
 * \retval VMK_BAD_PARAM         If intrType is
 *                               VMK_INTERRUPT_TYPE_PCI_MSIX and
 *                               indexAarray is NULL.
 * \retval VMK_NO_MEMORY         Internal memory allocation failure
 * \retval VMK_DEVICE_NOT_OWNED  Device is not owned by vmkernel
 * \retval VMK_NO_RESOURCES      If unable to allocate the requested
 *                               number of vectors and
 *                               bestEffortAllocation is VMK_FALSE.
 * \retval VMK_FAILIURE          All other errors.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIAllocateIntVectors(vmk_PCIDevice device,
                                           vmk_PCIInterruptType intrType,
                                           vmk_int32 numVectorsRequested,
                                           vmk_uint16 *indexAarray,
                                           vmk_Bool bestEffortAllocation,
                                           vmk_uint32 *vectorArray,
                                           vmk_int32 *numVectorsAlloced);

/*
 ***********************************************************************
 * vmk_PCIFreeIntVectors --                                       */ /**
 *
 * \ingroup PCI
 * \brief Free numVectors interrupt vectors that were previously
 *        allocated by a call to vmk_PCIAllocateIntVectors().
 *
 * \param[in] device       PCI device handle
 * \param[in] numVectors   Number of vectors in vectorArray
 * \param[in] vectorArray  Array of vectors to be freed.
 *
 * \retval VMK_BAD_PARAM        vectorArray argument is NULL
 * \retval VMK_BAD_PARAM        Device handle is invalid/NULL
 * \retval VMK_DEVICE_NOT_OWNED Device is not owned by vmkernel
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIFreeIntVectors(vmk_PCIDevice device,
                                       vmk_int32 numVectors,
                                       vmk_uint32 *vectorArray);

/*
 ***********************************************************************
 * vmk_PCIGetInterruptInfo --                                     */ /**
 *
 * \ingroup PCI
 * \brief Retrieve interrupt details of device.
 *
 * \param[in] device       PCI device handle
 * \param[in] interrupt    Pointer to vmk_PCIInterruptInfo.
 *
 * \retval VMK_BAD_PARAM         Device handle is NULL/invalid
 * \retval VMK_BAD_PARAM         Interrupt arg is NULL
 * \retval VMK_DEVICE_NOT_OWNED  Device is not owned by vmkernel.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIGetInterruptInfo(vmk_PCIDevice device,
                                         vmk_PCIInterruptInfo *interrupt);

/*
 ***********************************************************************
 * vmk_PCIRegisterCallback --                                     */ /**
 *
 * \ingroup PCI
 * \brief Register event callback.
 *        
 * \param[in] moduleID        Module ID of the caller
 * \param[in] callback        Event callback routine
 * \param[in] callbackPrivate Private data for callback routine.
 *
 * \retval VMK_BAD_PARAM moduleID is already registered for callbacks
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCIRegisterCallback(vmk_ModuleID moduleID,
                                         vmk_PCICallback callback,
                                         void *callbackPrivate);
/*
 ***********************************************************************
 * vmk_PCIDoPreRemove --                                          */ /**
 *
 * \ingroup PCI
 * \brief Invoke a module-specific function before a device is removed.
 *
 * This function should only be called by drivers, not by vmkernel
 * modules.
 *
 * \param[in] moduleID ID of the module whose pre-remove func is to be
 *                     called.
 * \param[in] device   Device handle to be passed to the pre-remove
 *                     function.
 *
 * \retval VMK_BAD_PARAM   Device is invalid/NULL.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDoPreRemove(vmk_ModuleID moduleID,
                   vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIDoPostInsert --                                         */ /**
 *
 * \ingroup PCI
 * \brief Invoke a module-specific function after a device has been
 *        inserted.
 *
 * \param[in] moduleID  ID of the module whose post-insert func is to
 *                      be called.
 * \param[in] device    Device handle to be passed to the post-insert
 *                      function.
 *
 * This function should only be called by drivers, not by vmkernel
 * modules.
 *
 * \retval VMK_BAD_PARAM device is invalid/NULL
 * \retval VMK_OK        success.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDoPostInsert(vmk_ModuleID moduleID,
                    vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIEnableVFs --                                            */ /**
 *
 * \ingroup PCI
 * \brief Enable Virtual Functions (VFs) on an SR-IOV device.
 *
 * \param[in]     device Device handle of the SR-IOV device.
 * \param[in,out] numVFs Number of Virtual Functions to enable/enabled.
 *
 * Passing a value of 0 as *numVFs will request to enable all the
 * Virtual Functions. The actual number of VFs successfully enabled is
 * returned in *numVFs upon success.
 *
 * \retval VMK_BAD_PARAM Device handle is invalid or device does not exist.
 * \retval VMK_FAILURE   Cannot enable Virtual Functions.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIEnableVFs(vmk_PCIDevice device,
                 vmk_uint16 *numVFs);

/*
 ***********************************************************************
 * vmk_PCIDisableVFs --                                           */ /**
 *
 * \ingroup PCI
 * \brief Disable all the Virtual Functions (VFs) on an SR-IOV device.
 *
 * \param[in] device     Device handle of the SR-IOV device.
 *
 * \retval VMK_BAD_PARAM Device handle is invalid or device does not exist.
 * \retval VMK_FAILURE   Device is not in SR-IOV mode.
 * 
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIDisableVFs(vmk_PCIDevice device);

/*
 ***********************************************************************
 * vmk_PCIGetVFPCIDevice --                                       */ /**
 *
 * \ingroup PCI

 * \brief Retrieves the handle of a Virtual Function (VF) given its
 * parent Physical Function (PF).
 
 * \param[in]  pf        PCI device handle of the parent Physical Function.
 * \param[in]  vfIndex   Index of the Virtual Function.
 * \param[out] handle    PCI device handle of the requested Virtual Function.
 *
 * \retval VMK_BAD_PARAM PF device handle is invalid or device does not exist.
 * \retval VMK_BAD_PARAM handle argument is NULL.
 * \retval VMK_NOT_FOUND VF doesn't exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PCIGetVFPCIDevice(vmk_PCIDevice pf,
                      vmk_uint16 vfIndex,
                      vmk_PCIDevice *handle);

#endif /* _VMKAPI_PCI_H_ */
/** @} */
/** @} */
