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
 * \addtogroup DMA DMA Address Space Management
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_DMA_INCOMPAT_H_
#define _VMKAPI_DMA_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/*
 ***********************************************************************
 * vmk_DMAEngineIsIdentityMapped --                               */ /**
 *
 * \brief Determines whether the provided engine performs
 *        mappings such that machine and IO addresses are
 *        identity (1:1) mapped.
 *
 * \note This function will not block.
 *
 * \param[in]  engine           A handle representing a DMA engine
 *                              to unmap from.
 * \param[out] identityMapped   Whether the provided engine performs
 *                              identity mappings.
 *
 * \retval VMK_BAD_PARAM        Invalid engine or identMap.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAEngineIsIdentityMapped(vmk_DMAEngine engine,
                                               vmk_Bool *identityMapped);

#endif /* _VMKAPI_DMA_INCOMPAT_H_ */
/** @} */
/** @} */
