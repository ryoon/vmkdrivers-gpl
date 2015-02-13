/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * User Space Memory Interface                                             */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup UserMem User Space Memory
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_USERMEMORY_H_
#define _VMKAPI_CORE_USERMEMORY_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ******************************************************************************
 * vmk_CopyFromUser --                                                   */ /**
 *
 * Copy memory from a user space application into a kernel buffer
 *
 * \note Caller must not hold any spinlocks.
 * \note Must be called from a blockable context
 *
 * \param[in] dest   Copy-to location.
 * \param[in] src    Copy-from location.
 * \param[in] len    Amount to copy.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_CopyFromUser(
   vmk_VA dest,
   vmk_VA src,
   vmk_ByteCount len);

/*
 ******************************************************************************
 * vmk_CopyToUser --                                                     */ /**
 *
 * Copy memory from a kernel buffer into a user space application.
 *
 * \note Caller must not hold any spinlocks.
 * \note Must be called from a blockable context
 *
 * \param[in] dest   Copy-to location.
 * \param[in] src    Copy-from location.
 * \param[in] len    Amount to copy.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_CopyToUser(
   vmk_VA dest,
   vmk_VA src,
   vmk_ByteCount len);


#endif
/** @} */
/** @} */
