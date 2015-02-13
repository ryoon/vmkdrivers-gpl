/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Machine Memory Interface                                              */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup MachMem Machine Memory
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_MACHINEMEMORY_H_
#define _VMKAPI_CORE_MACHINEMEMORY_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ******************************************************************************
 * vmk_MachMemMaxAddr --                                                 */ /**
 *
 * \brief Returns the highest machine address ESX supports on this system.
 * 
 * \return Machine address of the highest machine address that the system
 *         supports.
 *
 * \note    The number does not correlate with the maximum amount of memory
 *          available in the system.
 * \note    This function will not block.
 *
 ******************************************************************************
 */

vmk_MA vmk_MachMemMaxAddr(void);


/*
 ******************************************************************************
 * vmk_VA2MA --                                                          */ /**
 *
 * \brief Get the machine address backing the supplied virtual address
 *
 * \param[in]  va       Virtual address to resolve
 * \param[in]  length   Length of the virtual buffer that is assumed to be
 *                      physically contiguous.
 * \param[out] ma       The backing machine address
 *
 * \return     VMK_OK on success, error code otherwise
 *
 * \note    This function will not block.
 *
 ******************************************************************************
 */

VMK_ReturnStatus vmk_VA2MA(
   vmk_VA        va,
   vmk_ByteCount length,
   vmk_MA       *ma);


/*
 ******************************************************************************
 * vmk_MA2VA --                                                          */ /**
 *
 * \brief Get the virtual address for a machine address that was resolved from a
 *        previously allocated virtual buffer
 *
 * \param[in]  ma       Machine address to resolve
 * \param[in]  length   Length of the virtual buffer that this ma refers to
 * \param[out] va       The corresponding virtual address
 *
 * \return     VMK_OK on success, error code otherwise
 *
 * \note   This function should not be used to resolve arbitrary machine
 *         addresses to virtual addresses. Arbitrary machine addresses can be 
 *         mapped using the mapping interface.
 * \note   This function will not block.
 *
 ******************************************************************************
 */

VMK_ReturnStatus vmk_MA2VA(
   vmk_MA        ma,
   vmk_ByteCount length,
   vmk_VA       *va);


/*
 ******************************************************************************
 * vmk_MAGetConstraint --                                                */ /**
 *
 * \brief Get the most restrictive physical address constraint for the given MA
 *
 * \param[in]  ma       Machine address to resolve
 *
 * \return     Physical address constraint
 *
 * \note   This function will not block.
 *
 ******************************************************************************
 */

vmk_MemPhysAddrConstraint
vmk_MAGetConstraint(vmk_MA ma);


/*
 ******************************************************************************
 * vmk_MAAssertIOAbilityInt --
 *
 * This function is used internally by vmk_MAAssertIOAbility.  VMKAPI clients
 * should not call this function directly.
 *
 ******************************************************************************
 */

/** \cond nodoc */
void vmk_MAAssertIOAbilityInt(
   vmk_MA ma,
   vmk_ByteCount len);
/** \endcond */


/*
 ******************************************************************************
 * vmk_MAAssertIOAbility --                                              */ /**
 *
 * \brief Panic the system if a region of machine memory does not
 *        support IO.
 *
 * \note This function only performs a check on debug builds.
 * 
 * \param[in] ma         Starting machine address of the range to check
 * \param[in] len        Length of the range to check in bytes
 *
 * \note   This function will not block.
 *
 ******************************************************************************
 */

static VMK_ALWAYS_INLINE void 
vmk_MAAssertIOAbility(
   vmk_MA ma,
   vmk_ByteCount len)
{
#ifdef VMX86_DEBUG
   vmk_MAAssertIOAbilityInt(ma, len);
#endif
}

/*
 ******************************************************************************
 * vmk_MAGetZeroBuffer --                                                */ /**
 *
 * \brief Return the machine address of the global zero buffer
 *
 * \param[out] len  Global zero buffer size in bytes
 *
 * \return Machine address for the global zero buffer
 *
 * \note   This function will not block.
 *
 ******************************************************************************
 */

vmk_MA
vmk_MAGetZeroBuffer(vmk_ByteCount *len);

#endif
/** @} */
/** @} */
