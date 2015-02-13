/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
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
 * Vector                                                         */ /**
 * \addtogroup Device
 * @{
 * \defgroup Vector Interrupt Vector Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VECTOR_H_
#define _VMKAPI_VECTOR_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Options for adding the handler
 */
typedef struct vmk_VectorAddHandlerOptions {
   /** \brief Vector is shared */
   vmk_Bool sharedVector;

   /** \brief  Vector is an entroy source */
   vmk_Bool entropySource;
} vmk_VectorAddHandlerOptions;


/*
 ***********************************************************************
 * vmk_AddInterruptHandler --                                     */ /**
 *
 * \ingroup Vector
 * \brief Add interrupt handler for vector
 *
 * \param[in] vector       Vector to add the handler for
 * \param[in] deviceName   Name the driver wants use with the request
 * \param[in] handler      Interrupt handler
 * \param[in] callbackArg  Callback argument passed to the handler
 *                         for shared interrupt, callbackArg shall be
 *                         non-NULL and is used identify the callers
 *                         sharing the vector.
 * \param[in] options      Specifies how the vector should be used
 *                         (shared vector, entropy source etc.)
 *
 * \retval VMK_BAD_PARAM If vector is not a valid device vector
 * \retval VMK_BAD_PARAM Null callbackArg is specified for
 *                       shared vectors.
 * \retval VMK_BUSY      If request to add as non-shared and the vector
 *                       is already shared
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_AddInterruptHandler(vmk_uint32 vector,
                                         const char *deviceName,
                                         vmk_InterruptHandler handler,
                                         void *callbackArg,
                                         vmk_VectorAddHandlerOptions *options);

/*
 ***********************************************************************
 * vmk_RemoveInterruptHandler --                                  */ /**
 *
 * \ingroup Vector
 * \brief Remove a previously established interrupt handler for vector
 *
 * \param[in] vector       Vector for which to remove the handler.
 * \param[in] callbackArg  Callback argument that was passed
 *                         while adding the handler.
 *
 * \retval VMK_BAD_PARAM If callbackArg is NULL and the vector is shared.
 * \retval VMK_BAD_PARAM If vector is not a valid device vector
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RemoveInterruptHandler(vmk_uint32 vector,
                                            void *callbackArg);

/*
 ***********************************************************************
 * vmk_VectorEnable --                                            */ /**
 *
 * \ingroup Vector
 * \brief Sets up the vector for delivery of interrupts and unmasks the
 *        vector.
 *
 * \param[in] vector    Vector to enable.
 *                      Depending on the interrupt type, vector may
 *                      always be enabled. (It is not possible
 *                      to enable/disable VMK_INTERRUPT_TYPE_PCI_MSI 
 *                      vectors if the device does not support
 *                      per vector masking.)
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VectorEnable(vmk_uint32 vector);

/*
 ***********************************************************************
 * vmk_VectorDisable --                                           */ /**
 *
 * \ingroup Vector
 * \brief Masks the vector and disables interrupt delivery.
 *
 * \warning This API should not be used for indefinite periods for 
 *          shared interrupts as this will block interrupts for other 
 *          devices that may share the same interrupt line.
 *
 * \param[in] vector Vector to disable.
 *                   Depending on the interrupt type, vector will
 *                   always be enabled. (It is not possible
 *                   to enable/disable VMK_INTERRUPT_TYPE_PCI_MSI 
 *                   vectors if the device does not support
 *                   per vector masking.)
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VectorDisable(vmk_uint32 vector);

/*
 ***********************************************************************
 * vmk_VectorSync --                                              */ /**
 *
 * \ingroup Vector
 * \brief Blocks, waiting till vector is inactive on all CPUs.
 *        
 * \param[in] vector    Vector to synchronize.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VectorSync(vmk_uint32 vector);

/*
 ***********************************************************************
 * vmk_VectorChipsetDisable --                                    */ /**
 *
 * \ingroup Vector
 * \brief Masks given vector at the Chipset level. 
 *
 * \warning This API should not be used for indefinite periods for shared 
 * 	    interrupts as this will block interrupts for other devices that 
 * 	    may share the same interrupt line. Specifically, if there is a 
 * 	    device specific method for masking/unmasking interrupts, that 
 * 	    should be preferred over this API. This API is currently 
 * 	    provided for masking exclusive interrupts (such as MSI) in a 
 * 	    device independent manner.
 *
 * \note This vector should already have been setup using
 *       vmk_VectorEnable.
 *        
 * \param[in] vector       Vector to mask
 *
 ***********************************************************************
 */
void vmk_VectorChipsetDisable (vmk_uint32 vector);

/*
 ***********************************************************************
 * vmk_VectorChipsetEnable --                                     */ /**
 *
 * \ingroup Vector
 * \brief Unmasks the given vector at the Chipset level. 
 *
 * \warning If there is a device specific method for masking/unmasking 
 * 	    interrupts, that should be preferred over this API. This API 
 * 	    is currently provided for masking/unmasking exclusive 
 * 	    interrupts(such as MSI) in a device independent manner.
 *
 * \note This vector should already have been setup using vmk_VectorEnable.
 *        
 * \param[in] vector    Vector to unmask
 *
 ***********************************************************************
 */
void vmk_VectorChipsetEnable (vmk_uint32 vector);

#endif /* _VMKAPI_VECTOR_H_ */
/** @} */
/** @} */
