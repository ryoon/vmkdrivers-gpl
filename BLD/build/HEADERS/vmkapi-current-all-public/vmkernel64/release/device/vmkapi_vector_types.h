/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Interrupt Handling                                                    */ /**
 *
 * \addtogroup Device
 * @{
 * \defgroup Vector Interrupt Vector Interfaces
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_VECTOR_TYPES_H_
#define _VMKAPI_VECTOR_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/*
 ******************************************************************************
 * vmk_InterruptHandler --                                               */ /**
 *
 * \brief Interrupt callback function.
 *
 * \note Callback is not allowed to block
 *
 * \param[in] clientData   Callback argument specified while adding
 *                         the handler.
 * \param[in] vector       Vector associated with the interrupt.
 *
 * \retval None
 *
 ******************************************************************************
 */

typedef void (*vmk_InterruptHandler)(void *clientData,
                                     vmk_uint32 vector);

#endif
/** @} */
/** @} */
