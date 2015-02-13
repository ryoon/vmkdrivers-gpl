/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
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
typedef struct vmk_PCIDeviceInt *vmk_PCIDevice;


#endif
/** @} */
/** @} */
