/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PageSlabIncompat                                              */ /**
 * \addtogroup Core
 * @{
 *
 * \addtogroup PageSlab
 * @{
 *
 * \defgroup Incompatible APIs for page slabs
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PAGESLAB_INCOMPAT_H_
#define _VMKAPI_PAGESLAB_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/*
 ***********************************************************************
 * vmk_PageSlabFreeWithoutSlabID --                             */ /**
 *
 * \brief Free a page slab page without knowing the page slab ID.
 *
 * \param[in]  mpn	    An machine page number previously returned by
 *                          vmk_PageSlabAlloc
 *
 ***********************************************************************
 */

void
vmk_PageSlabFreeWithoutSlabID(vmk_MPN mpn);


#endif /* _VMKAPI_PAGESLAB_INCOMPAT_H_ */
/** @} */
/** @} */
/** @} */
