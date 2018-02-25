/* **********************************************************
 * Copyright 1998 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Heap                                                           */ /**
 * \addtogroup Core
 * @{
 * \defgroup Heap Heaps
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_HEAP_INCOMPAT_H_
#define _VMKAPI_HEAP_INCOMPATH_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_HeapFreeByMA --                                            */ /**
 *
 * \brief Free memory allocated with vmk_HeapAlloc by machine address.
 *
 * \note  This function will not block.
 *
 * \param[in] heap   Heap that memory to be freed was allocated from.
 * \param[in] ma     Machine address of memory to be freed.
 *                   Should not be 0.
 *
 * \pre The heap given must be a low-memory contiguous heap.
 *
 ***********************************************************************
 */
void vmk_HeapFreeByMA(vmk_HeapID heap, vmk_MA ma);


#endif /* _VMKAPI_HEAP_INCOMPAT_H_ */
/** @} */
/** @} */
