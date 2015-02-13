/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Page Table Definitions                                         */ /**
 * \addtogroup X86
 * @{
 * \defgroup PagetableDefs Pagetable Definitions
 * Constants around X86 page sizes and page tables
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PAGETABLE_H_
#define _VMKAPI_PAGETABLE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Number of bits that all of the byte offsets in a page
 *        can occupy in an address
 */
#define VMK_PAGE_SHIFT 12

/**
 * \brief Number of bytes in a page
 */
#define VMK_PAGE_SIZE (1<<VMK_PAGE_SHIFT)

/**
 * \brief Bitmask that corresponds to the bits that all of the byte
 *        offsets in a page can occupy in an address.
 */
#define VMK_PAGE_MASK (VMK_PAGE_SIZE - 1)

#endif
/** @} */
/** @} */
