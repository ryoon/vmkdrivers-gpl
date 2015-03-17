/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Preemption                                                     */ /**
 * \defgroup Preemption Preemption State Management
 *
 * These interfaces can be used to change the current preemption
 * capability of the current context.
 *
 * @{
 ***********************************************************************
 */
 
#ifndef _VMKAPI_PREEMPT_H_
#define _VMKAPI_PREEMPT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief An encoding of an execution context's pre-emptibility.
 */
typedef vmk_uint8 vmk_PreemptionState;

/*
 ***********************************************************************
 * vmk_PreemptionDisable --                                       */ /**
 *
 * \ingroup Preemption
 * \brief Disable pre-emption in the current executing context.
 *
 * \return The previous pre-emption state.
 *
 ***********************************************************************
 */
vmk_PreemptionState vmk_PreemptionDisable(
   void);

/*
 ***********************************************************************
 * vmk_PreemptionRestore --                                       */ /**
 *
 * \ingroup Preemption
 * \brief Restore the pre-emption state of the current context.
 *
 * \param[in]  restoreState  The state that will be restored.
 *
 ***********************************************************************
 */
void vmk_PreemptionRestore(
   vmk_PreemptionState restoreState);

/*
 ***********************************************************************
 * vmk_PreemptionIsEnabled --                                     */ /**
 *
 * \ingroup Preemption
 * \brief Determine if pre-emption is enabled in the current context.
 *
 * \return VMK_TRUE   Pre-emption is currently enabled.
 * \return VMK_FALSE  Pre-emption is currently disabled.
 *
 ***********************************************************************
 */
vmk_Bool vmk_PreemptionIsEnabled(
   void);

#endif /* _VMKAPI_PREEMPT_H_ */
/** @} */
