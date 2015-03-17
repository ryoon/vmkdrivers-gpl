/* **********************************************************
 * Copyright 2008 - 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Please consult with the VMKernel hardware and core teams before making any
 * binary incompatible changes to this file!
 */

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Interrupt                                                      */ /**
 * \addtogroup Device
 * @{
 * \defgroup Interrupt Interrupt Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_INTR_H_
#define _VMKAPI_INTR_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Interrupt is an entropy source.
 *
 * Set this if device interrupts contributes to entropy pool.
 */
#define VMK_INTR_ATTRS_ENTROPY_SOURCE           (1 << 0)

/**
 * \brief Properties for registering the interrupt.
 */
typedef struct vmk_IntrProps {
   /** \brief Device registering the interrupt. */
   vmk_Device device;
   /** \brief Name of the device registering the interrupt. */
   vmk_Name deviceName;
   /** \brief Interrupt acknowledging function */
   vmk_IntrAcknowledge acknowledgeInterrupt;
   /** \brief Interrupt handler function. */
   vmk_IntrHandler handler;
   /** \brief Interrupt handler client data. */
   void *handlerData;
   /** \brief Interrupt attributes.
    *
    * Interrupt attributes can be used to specify special attributes
    * for a interrupt.
    */
   vmk_uint64 attrs;
} vmk_IntrProps;

/** \brief Function to invoke with interrupts disabled. */
typedef void (*vmk_IntrDisabledFunc)(vmk_AddrCookie data);

/*
 ***********************************************************************
 * vmk_IntrRegister --                                            */ /**
 *
 * \ingroup Interrupt
 * \brief Register the interrupt with the system.
 *
 * \note Interrupt sharing is implicitly allowed for level-triggered
 *       interrupts.
 *
 * \param[in] moduleID    Module registering the interrupt
 * \param[in] intrCookie  Interrupt cookie to register
 * \param[in] props       Properties of the interrupt being registered
 *
 * \retval VMK_BAD_PARAM  props is NULL or mal-formed props
 * \retval VMK_BAD_PARAM  intrCookie is not valid
 * \retval VMK_BAD_PARAM  Null props->handlerData is specified for
 *                        shared interrupt.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrRegister(vmk_ModuleID moduleID,
                                  vmk_IntrCookie intrCookie,
                                  vmk_IntrProps *props);

/*
 ***********************************************************************
 * vmk_IntrUnregister --                                          */ /**
 *
 * \ingroup Interrupt
 * \brief Unregister a previously registered interrupt.
 *
 * \param[in] moduleID      Module that registered interrupt before
 * \param[in] intrCookie    Interrupt to unregister
 * \param[in] handlerData   Interrupt handler data that was used while
 *                          registering the interrupt
 *
 * \retval VMK_BAD_PARAM   moduleID is not valid.
 * \retval VMK_BAD_PARAM   intrCookie is not valid.
 * \retval VMK_BAD_PARAM   If handlerData is NULL and the interrupt is
 *                         shared.
 * \retval VMK_FAILURE     handlerData doesn't match with what's
 *                         provided with vmk_IntrRegister().
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrUnregister(vmk_ModuleID moduleID,
                                    vmk_IntrCookie intrCookie,
                                    void *handlerData);

/*
 ***********************************************************************
 * vmk_IntrEnable --                                              */ /**
 *
 * \ingroup Interrupt
 * \brief Start interrupt delivery. Kernel starts calling interrupt
 *        handlers registered for this interrupt.
 *
 * \note The interrupt is unmasked if needed.
 *
 * \param[in] intrCookie  Interrupt that has to be started.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrEnable(vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_IntrDisable --                                             */ /**
 *
 * \ingroup Interrupt
 * \brief Stops interrupt delivery.
 *
 * \note The interrupt is masked if there are no registered handlers
 *       for this interrupt.
 *
 * \warning This API should not be used for indefinite periods for
 *          shared interrupts as this will block interrupts for other
 *          devices that may share the same interrupt line.
 *
 * \param[in] intrCookie Interrupt that has to be stopped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrDisable(vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_IntrSync --                                                */ /**
 *
 * \ingroup Interrupt
 * \brief Blocks, waiting till interrupt is inactive on all CPUs.
 *
 * \param[in] intrCookie Interrupt to synchronize.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IntrSync(vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_IntrWithAllDisabledInvoke --                               */ /**
 *
 * \ingroup Vector
 * \brief Invokes a function with all interrupts disabled.
 *
 * \warning The function invoked is not allowed to take more than 5us.
 *
 * \param[in] moduleID  Module implementing function to invoke.
 * \param[in] func      Function to invoke
 * \param[in] data      Data to pass to function
 *
 * \retval VMK_BAD_PARAM  moduleID is invalid
 * \retval VMK_BAD_PARAM  func is invalid
 * \retval VMK_OK         func was invoked
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_IntrWithAllDisabledInvoke(vmk_ModuleID moduleID,
                                               vmk_IntrDisabledFunc func,
                                               vmk_AddrCookie data);

#endif /* _VMKAPI_INTR_H_ */
/** @} */
/** @} */

