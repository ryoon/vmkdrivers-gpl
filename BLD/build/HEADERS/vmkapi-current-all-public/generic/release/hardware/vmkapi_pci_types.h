/* **********************************************************
 * Copyright 2010 - 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * PCI                                                                   */ /**
 *
 * \addtogroup Device
 * @{
 * \defgroup PCI PCI
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_PCI_TYPES_H_
#define _VMKAPI_PCI_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Type of interrupt triggering
 * If the device is not interruptive, it will have an interrupt type of
 * VMK_PCI_INTERRUPT_TYPE_NONE.
 */
typedef enum vmk_PCIInterruptType {
   VMK_PCI_INTERRUPT_TYPE_NONE   = 0,
   VMK_PCI_INTERRUPT_TYPE_LEGACY = 1,
   VMK_PCI_INTERRUPT_TYPE_MSI    = 2,
   VMK_PCI_INTERRUPT_TYPE_MSIX   = 3,
} vmk_PCIInterruptType;

/**
 * \brief Opaque PCI device handle
 */
typedef struct vmkPCIDevice *vmk_PCIDevice;

/**
 * \brief Opaque PCI device iterator
 *
 * The PCI device iterator can be used to obtain the vmk_PCIDevice handle
 * of any PCI device in the system by iterating over the system's list of
 * PCI devices.
 */
typedef struct vmkPCIDevIterator *vmk_PCIDevIterator;

/**
 * \brief PCI device iterator type
 *
 * The type of the iterator determines the order in which it iterates
 * over the system's list of PCI devices.
 *
 * Type                                Description
 * ----------------------------------------------------------------------------
 * VMK_PCI_DEV_ITERATOR_TYPE_NO_ORDER  The order of devices in the iteration
 *                                     has no relationship to the order of
 *                                     devices in the PCI hierarchy.
 */
typedef enum vmk_PCIDevIteratorType {
   VMK_PCI_DEV_ITERATOR_TYPE_NO_ORDER = 0,
} vmk_PCIDevIteratorType;




#endif
/** @} */
/** @} */
