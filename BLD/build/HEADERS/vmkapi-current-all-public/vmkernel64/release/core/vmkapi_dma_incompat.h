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
 * vmk_DMAMapElem --                                              */ /**
 *
 * \brief Map machine memory of a single machine address range to an
 *        IO address range.
 *
 * This call will attempt to map a single machine-address range and
 * create a new IO-address address range that maps to it.
 *
 * \note The input range must not be freed or modified while it
 *       is mapped or the results are undefined.
 *
 * \note If the range is simultaneously mapped with multiple DMA
 *       directions, the contents of the memory the SG array represents
 *       are undefined.
 *
 * \note This function will not block.
 *
 * \param[in]  engine      A handle representing a DMA engine to map to.
 * \param[in]  direction   Direction of the DMA transfer for the mapping.
 * \param[in]  in          A single SG array element containing a single
 *                         machine addresse range to map for the
 *                         DMA engine.
 * \param[in]  lastElem    Indicates if this is the last element in
 *                         the transfer.
 * \param[out] out         A single SG array element to hold the mapped
 *                         IO address for the range.
 * \param[out] err         If this call fails with VMK_DMA_MAPPING_FAILED,
 *                         additional information about the failure may
 *                         be found here. This may be set to NULL if the
 *                         information is not desired.
 *
 * \retval VMK_BAD_PARAM            The specified DMA engine is invalid.
 * \retval VMK_DMA_MAPPING_FAILED   The mapping failed because the
 *                                  DMA constraints could not be met.
 *                                  Additional information about the
 *                                  failure can be found in the "err"
 *                                  argument.
 * \retval VMK_NO_MEMORY            There is currently insufficient
 *                                  memory available to construct the
 *                                  mapping.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAMapElem(vmk_DMAEngine engine,
                                vmk_DMADirection direction,
                                vmk_SgElem *in,
                                vmk_Bool   lastElem,
                                vmk_SgElem *out,
                                vmk_DMAMapErrorInfo *err);

/*
 ***********************************************************************
 * vmk_DMAFlushElem --                                            */ /**
 *
 * \brief Synchronize a DMA mapping for a single IO address range.
 *
 * This call is used to synchronize data if the CPU needs to read or
 * write after an DMA mapping is active on a region of machine memory
 * but before the DMA mapping is unmapped.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_FROM_MEMORY after CPU writes are complete but
 * before any new DMA read transactions occur on the memory.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_TO_MEMORY before CPU reads but after
 * any write DMA transactions complete on the memory.
 *
 * DMA map and unmap calls will implicitly perform a flush of the
 * element.
 *
 * The code may flush bytes rounded up to the nearest page or other
 * HW-imposed increment.
 *
 * \note The IO element supplied to this function must be an element
 *       output from vmk_DMAMapElem or the results of this call are
 *       undefined.
 *
 *       Do not use this to flush a single element in an SG array
 *       that was mapped by vmk_DMAMapSg.
 *
 * \note The original element supplied to this function must be
 *       the one supplied to vmk_DMAMapElem when the IO element
 *       was created or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing the DMA engine used
 *                        for the mapping.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to flush.
 *
 * \retval VMK_BAD_PARAM         Unknown duration or direction, or
 *                               unsupported direction.
 * \retval VMK_INVALID_ADDRESS   Memory in the specified element
 *                               is not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAFlushElem(vmk_DMAEngine engine,
                                  vmk_DMADirection direction,
                                  vmk_SgElem *IOElem);

/*
 ***********************************************************************
 * vmk_DMAUnmapElem --                                            */ /**
 *
 * \brief Unmaps previously mapped IO address range.
 *
 * \note The direction must match the direction at the time of mapping
 *       or the results of this call are undefined.
 *
 * \note The element supplied to this function must be one mapped with
 *       vmk_DMAMapElem or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] engine      A handle representing a DMA engine
 *                        to unmap from.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to unmap.
 *
 * \retval VMK_BAD_PARAM         Unknown direction, or unsupported
 *                               direction.
 * \retval VMK_INVALID_ADDRESS   One ore more pages in the specified
 *                               machine address range are not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DMAUnmapElem(vmk_DMAEngine engine,
                                  vmk_DMADirection direction,
                                  vmk_SgElem *IOElem);

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
