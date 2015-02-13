/* **********************************************************
 * Copyright 2007 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * BottomHalf                                                             */ /**
 * \addtogroup Core
 * @{
 * \defgroup BottomHalf Bottom-Half
 *
 * Bottom-halves are soft-interrupt-like contexts that run below the priority of
 * hardware interrupts but outside the context of a schedulable entity like a
 * World. This means that bottom-half callbacks may not block while they
 * execute.
 *
 * @{
 *******************************************************************************
 */

#ifndef _VMKAPI_BH_H_
#define _VMKAPI_BH_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Bottom-Half ID
 *
 * Identifies a registered bottom-half
 */
struct vmkBHInt;
typedef struct vmkBHInt *vmk_BHID;

/*
 *******************************************************************************
 * vmk_BHCallBack --                                                      */ /**
 *
 * \brief Prototype for a bottom-half callback function.
 *
 * This function will be called when a scheduled bottom-half is run. Bottom-
 * halves are expected to only run for a short period of time and surrender the
 * PCPU as quickly as possible by returning from the callback.
 * A bottom-half callback may not execute longer than 0.5 milliseconds (500
 * microseconds).
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in]   private Private data as specified to vmk_BHRegister.
 *
 *******************************************************************************
 */
typedef void (*vmk_BHCallBack)(vmk_AddrCookie data);


/**
 * \brief Bottom-half Properties
 *
 * Properties of a bottom-half that is getting registered
 */
typedef struct vmk_BHProps {
   /** Module ID of the registering module */
   vmk_ModuleID moduleID;
   /** Name associated with this bottom-half */
   vmk_Name name;
   /** Function to be called when the bottom-half is run */
   vmk_BHCallBack callback;
   /** Private data pointer that will be passed to the callback */
   void *priv;
} vmk_BHProps;


/*
 *******************************************************************************
 * vmk_BHRegister --                                                      */ /**
 *
 * \brief Register a function that can be scheduled as a bottom-half.
 *
 * \param[in]  props       Properties of this bottom half
 * \param[out] newBH       Bottom-half identifier
 *
 * \note This function will not block.
 *
 * \retval VMK_NO_RESOURCES Bottom-half registration table was full,
 *                          new bottom-half was not registered.
 * \retval VMK_NAME_INVALID Specified name was invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_BHRegister(
      vmk_BHProps *props,
      vmk_BHID *newBH);


/*
 ******************************************************************************
 * vmk_BHUnregister --                                                   */ /**
 *
 * \brief Unregister a previously registered bottom-half callback.
 *
 * \note This function will not block.
 *
 * \param[in] bottomHalf   Bottom-half to unregister.
 *
 ******************************************************************************
 */
void vmk_BHUnregister(
   vmk_BHID bottomHalf);


/*
 ******************************************************************************
 * vmk_BHSchedulePCPU --                                                 */ /**
 *
 * \brief Schedule the execution of a bottom-half on a particular PCPU
 *
 * This schedules the function which has been registered and associated
 * with the bottom-half identifier by vmk_BottomHalfRegister to
 * run as bottom-half with the given physical CPU.
 *
 * \warning Do not schedule a bottom-half on another CPU too frequently
 *          since the bottom-half cache-line is kept on the local CPU.
 *
 * \note This function will not block.
 *
 * \param[in] bottomHalf   Bottom-half to schedule.
 * \param[in] pcpu         PCPU on which the bottom-half should run.
 *
 * \retval VMK_OK The given bottom-half has been scheduled successfully.
 * \retval VMK_INVALID_TARGET The specified PCPU is not available.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_BHSchedulePCPU(
   vmk_BHID bottomHalf,
   vmk_uint32 pcpu);


/*
 ******************************************************************************
 * vmk_BHScheduleAnyPCPU --                                              */ /**
 *
 * \brief Schedule the execution of a bottom-half on an arbitrary PCPU
 *
 * \note This function will not block.
 *
 * \param[in] bottomHalf   Bottom-half to schedule.
 *
 ******************************************************************************
 */
void vmk_BHScheduleAnyPCPU(
   vmk_BHID bottomHalf);


/*
 * XXX PR582861: vmk_BottomHalfCheck needs to be removed before beta. How are
 * 3rd party devs supposed to make sense out of this? This is vmkernel internal
 * stuff. Should probably be replaced by an API for CpuSched_PreemptionPoint
 */

/*
 ***********************************************************************
 * vmk_BottomHalfCheck --                                         */ /**
 *
 * \brief Execute pending bottom-half handlers on the local pcpu.
 *
 * Afterwards, if a reschedule is pending and "canReschedule" is VMK_TRUE
 * then invoke the scheduler.
 *
 * \note This function may block if canReschedule is VMK_TRUE.
 *
 * \param[in] canReschedule   If VMK_TRUE then invoke the scheduler after
 *                            pending bottom-halves execute.
 *
 ***********************************************************************
 */
void vmk_BottomHalfCheck(
   vmk_Bool canReschedule);

#endif /* _VMKAPI_BH_H_ */
/** @} */
/** @} */
