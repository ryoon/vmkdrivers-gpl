/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * CPU                                                               */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup CPU CPU
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_CPU_INCOMPAT_H_
#define _VMKAPI_CORE_CPU_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_NumPCPUs --                                                */ /**
 *
 * \brief Return vmkernels numPCPUs global.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_NumPCPUs(void);


/*
 ***********************************************************************
 * vmk_GetAPICCPUID --                                            */ /**
 *
 * \brief Return the APIC ID of a CPU based on its PCPUNum
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_GetAPICCPUID(
   vmk_uint32 pcpuNum);


/*
 ***********************************************************************
 * vmk_GetPCPUNum --                                              */ /**
 *
 * \brief Return the PCPU we're currently executing on.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_GetPCPUNum(void);


#endif
/** @} */
/** @} */
