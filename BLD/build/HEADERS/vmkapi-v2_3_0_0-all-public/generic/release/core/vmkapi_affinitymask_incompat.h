/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Affinity Mask                                                         */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup AffMask Affinity Mask
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_AFFINITYMASK_INCOMPAT_H_
#define _VMKAPI_AFFINITYMASK_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief A set of CPUs */
typedef struct vmk_AffinityMaskInt *vmk_AffinityMask;

/** \brief Affinity mask returned if a new mask cannot be created */
#define VMK_INVALID_AFFINITY_MASK   ((vmk_AffinityMask)NULL)


/*
 ***********************************************************************
 * vmk_AffinityMaskCreate --                                      */ /**
 *
 * \brief Allocate zeroed out affinity bitmask object.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[in] module    The module whose heap will be used to allocate
 *                      the affinity mask.
 *
 * \note This function will not block.
 *
 * \return Allocated affinity mask on success, NULL on failure.
 *
 ***********************************************************************
 */
vmk_AffinityMask vmk_AffinityMaskCreate(
   vmk_ModuleID module);

/*
 ***********************************************************************
 * vmk_AffinityMaskDestroy --                                     */ /**
 *
 * \brief Free affinity bitmask object.
 *
 * \param[in] affinityMask    Affinity mask to be freed.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
void vmk_AffinityMaskDestroy(
   vmk_AffinityMask affinityMask);

/*
 ***********************************************************************
 * vmk_AffinityMaskAdd --                                         */ /**
 *
 * \brief Add a CPU to an affinity bitmask.
 *
 * \param[in]  cpuNum         Index of the CPU, starting at 0.
 * \param[out] affinityMask   The affinity mask to be modified.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
void vmk_AffinityMaskAdd(
   vmk_uint32 cpuNum,
   vmk_AffinityMask affinityMask);

/*
 ***********************************************************************
 * vmk_AffinityMaskDel --                                         */ /**
 *
 * \brief Delete a CPU from an affinity bitmask.
 *
 * \param[in]  cpuNum        Index of the CPU, starting at 0.
 * \param[out] affinityMask  The affinity mask to be modified.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
void vmk_AffinityMaskDel(
   vmk_uint32 cpuNum,
   vmk_AffinityMask affinityMask);

/*
 ***********************************************************************
 * vmk_AffinityMaskClear --                                       */ /**
 *
 * \brief Clear an affinity bitmask of all CPUs.
 *
 * \param[out] affinityMask  The affinity mask to be modified.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
void vmk_AffinityMaskClear(
   vmk_AffinityMask affinityMask);

/*
 ***********************************************************************
 * vmk_AffinityMaskFill --                                        */ /**
 *
 * \brief Set an affinity to include all CPUs.
 *
 * \param[out] affinityMask   The affinity mask to be modified.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
void vmk_AffinityMaskFill(
   vmk_AffinityMask affinityMask);

/*
 ***********************************************************************
 * vmk_AffinityMaskHasPCPU --                                     */ /**
 *
 * \brief Test if a given affinity mask includes a particular PCPU number.
 *
 * \param[in] affinityMask    The affinity mask to be tested for
 *                            inclusion.
 * \param[in] cpuNum          The CPU number.
 *
 * \note This function will not block.
 *
 * \retval VMK_TRUE  cpuNum is represented in the bitmask.
 * \retval VMK_FALSE cpuNum is not represented in the bitmask.
 *
 ***********************************************************************
 */
vmk_Bool vmk_AffinityMaskHasPCPU(
   vmk_AffinityMask affinityMask,
   vmk_uint32 cpuNum);

#endif /* _VMKAPI_AFFINITYMASK_INCOMPAT_H_ */
/* @} */
/* @} */
