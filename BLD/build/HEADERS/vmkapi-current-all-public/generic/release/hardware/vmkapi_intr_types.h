/* **********************************************************
 * Copyright 2010 - 2013 VMware, Inc.  All rights reserved.
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
 * \defgroup Interrupt Interrupt Interfaces
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_INTR_TYPES_H_
#define _VMKAPI_INTR_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle to Interrupt.
 */
typedef vmk_uint32 vmk_IntrCookie;

/**
 * \brief Invalid interrupt.
 */
#define VMK_INVALID_INTRCOOKIE ((vmk_IntrCookie)~0)

/*
 ******************************************************************************
 * vmk_IntrHandler --                                                    */ /**
 *
 * \brief Interrupt callback function.
 *
 * \note Callback is allowed to call functions that involuntarily deschedule
 *       the calling context.  Callback is not allowed to block.
 *
 * \param[in] handlerData  Callback argument specified while adding
 *                         the handler.
 * \param[in] intrCookie   Interrupt cookie associated with the interrupt.
 *
 * \retval None
 *
 ******************************************************************************
 */

typedef void (*vmk_IntrHandler)(void *handlerData,
                                vmk_IntrCookie intrCookie);

/*
 ******************************************************************************
 * vmk_IntrAcknowledge --                                                */ /**
 *
 * \brief Callback function for device drivers to acknowledge an interrupt
 *
 * This callback is called synchronously when an interrupt fires to do device
 * specific interrupt acknowledgement. The handler is expected to acknowledge
 * the interrupt with the device for example through device register writes but
 * nothing more. All other interrupt work should be done in the interrupt
 * handler.  The vmkernel will enforce a very short execution time of 5us for
 * this callback.
 *
 * \note Callback is not allowed to block
 *
 * \param[in] handlerData  Callback argument specified while adding
 *                         the handler.
 * \param[in] intrCookie   Interrupt cookie associated with the interrupt.
 *
 * \retval VMK_OK              Interrupt has been acknowledged and handler
 *                             should be called.
 * \retval VMK_IGNORE          Interrupt was for this device and has been
 *                             acknowledged, but handler is not needed and
 *                             should not be called.
 * \retval VMK_NOT_THIS_DEVICE Interrupt was not for this device and has not
 *                             been acknowledged; handler should not be called.
 *
 ******************************************************************************
 */

typedef VMK_ReturnStatus (*vmk_IntrAcknowledge)(void *handlerData,
                                                vmk_IntrCookie intrCookie);


#endif /* _VMKAPI_INTR_TYPES_H_ */
/** @} */
/** @} */
