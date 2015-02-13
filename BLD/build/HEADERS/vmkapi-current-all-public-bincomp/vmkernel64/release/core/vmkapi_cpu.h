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

#ifndef _VMKAPI_CORE_CPU_H_
#define _VMKAPI_CORE_CPU_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * Opaque identifier for a PCPU
 */
typedef struct VMKPCPUIDInt *vmk_PCPUID;

/**
 * Opaque structure used for state in VMK_WITH_PCPU_DO macro.
 */
typedef struct vmk_WithPCPUState {
   vmk_uint8 opaque[8];
} vmk_WithPCPUState;


/** \cond never
 * We don't want the following to show up in doxygen.
 */

/*
 ******************************************************************************
 * _vmk_PCPUGet --                                                       */ /**
 *
 * \brief Get an identifier for the PCPU that the code is currtly running on
 *
 * Retrieves an identifier for the current PCPU and disables preemption
 *
 * \note Always use VMK_WITH_PCPU_DO { } VMK_END_WITH_PCPU.  Never use
 *       _vmk_PCPUGet on its own.
 *
 * \note A caller has to release the PCPU and reenable preemption with a
 *       subsequent vmk_PCPURelease call. It is the duty of the caller to
 *       reenable preemption as fast as possible.
 *
 * \return Identifier for the current PCPU
 *
 ******************************************************************************
 */
vmk_PCPUID _vmk_PCPUGet(vmk_WithPCPUState *withPCPU);


/*
 ******************************************************************************
 * _vmk_PCPURelease --                                                   */ /**
 *
 * \brief Release a PCPU previously acquired via vmk_PcpuGet
 *
 * \note Always use VMK_WITH_PCPU_DO { } VMK_END_WITH_PCPU.  Never use
 *       _vmk_PCPURelease on its own.
 *
 * \param[in]   pcpu    PCPU identifier previously returned by _vmk_PCPUGet
 *
 ******************************************************************************
 */
void _vmk_PCPURelease(vmk_PCPUID pcpu,
                      vmk_WithPCPUState *withPCPU);

/** \endcond never */


/*
 ******************************************************************************
 * VMK_WITH_PCPU_DO --                                                   */ /**
 *
 * \brief Start a code section that needs to stay pinned to the current PCPU
 *
 * Provides an identifier for the current PCPU in pcpu and disables preemption.
 *
 * \note Has to be paired with a subsequent VMK_END_WITH_PCPU call
 *
 * \note Callers are responsible for surrendering the PCPU by calling
 *       VMK_END_WITH_PCPU as quickly as possible
 *
 ******************************************************************************
 */
#define VMK_WITH_PCPU_DO(_pcpu)                                               \
do {                                                                          \
   vmk_WithPCPUState _withPCPUState;                                          \
   vmk_PCPUID _pcpu = _vmk_PCPUGet(&_withPCPUState);


/*
 ******************************************************************************
 * VMK_END_WITH_PCPU --                                                  */ /**
 *
 * \brief End a pinned code section
 *
 * Releases the PCPU and restores preemption to its previous state
 *
 * \note Has to be paired with a prior call to VMK_WITH_PCPU_DO
 *
 ******************************************************************************
 */
#define VMK_END_WITH_PCPU(_pcpu)                                              \
   _vmk_PCPURelease(_pcpu, &_withPCPUState);                                  \
} while(0);


#endif
/** @} */
/** @} */
