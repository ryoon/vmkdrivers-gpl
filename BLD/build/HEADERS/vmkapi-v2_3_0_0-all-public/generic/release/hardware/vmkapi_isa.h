/***************************************************************************
 * Copyright 2008 - 2009, 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * ISA                                                            */ /**
 * \addtogroup Device
 * @{
 * \defgroup ISA ISA-Bus Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_ISA_H_
#define _VMKAPI_ISA_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_ISAMapIRQToIntrCookie --                                   */ /**
 *
 * \ingroup ISA
 * \brief Retrieves the intrCookie the kernel assigned for an isa IRQ
 *
 * \param[in]  isaIRQ      ISA IRQ value.
 * \param[out] intrCookie  Corresponding intrCookie for specified IRQ.
 *
 * \retval VMK_BAD_PARAM   isaIRQ is out of range.
 * \retval VMK_BAD_PARAM   intrCookie was NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ISAMapIRQToIntrCookie(vmk_uint32 isaIRQ,
                                           vmk_IntrCookie *intrCookie);
#endif /* _VMKAPI_ISA_H_ */
/** @} */
/** @} */
