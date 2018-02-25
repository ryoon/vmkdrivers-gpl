/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Accounting                                                     */ /**
 * \defgroup Accounting System Time Accounting
 *
 * System time accounting allows work for a particular service to be
 * charged to a world. This allows work to be offloaded to several worlds
 * or other contexts but charged to the appropriate world on whose behalf
 * the work is being done.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_ACCOUNTING_H_
#define _VMKAPI_ACCOUNTING_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Well-known accounting service names
 */
#define VMK_SERVICE_ACCT_NAME_KERNEL  "kernel"
#define VMK_SERVICE_ACCT_NAME_SCSI    "scsi"
#define VMK_SERVICE_ACCT_NAME_NET     "net"

/**
 * \ingroup Accounting
 * \brief Opaque handle representing a service to charge to.
 */
typedef vmk_uint64 vmk_ServiceAcctID;

/**
 * \ingroup Accounting
 * \brief Opaque handle to a world-related accounting context.
 */
typedef struct Sched_SysAcctContext *vmk_ServiceTimeContext;

/**
 * \ingroup Accounting
 * \brief A service time context that isn't associated with any service.
 */
#define VMK_SERVICE_TIME_CONTEXT_NONE ((vmk_ServiceTimeContext) 0)

/*
 ***********************************************************************
 * vmk_ServiceGetID --                                            */ /**
 *
 * \ingroup Accounting
 * \brief Lookup a service accounting ID handle by name
 *
 * \param[in]  name        Well-known accounting service name to lookup.
 * \param[out] serviceID   Service identifier handle corresponding to
 *                         the name.
 *
 * \note This function will not block.
 *
 * \retval VMK_INVALID_NAME   The specified service name is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ServiceGetID(
   const char *name,
   vmk_ServiceAcctID *serviceID);

/*
 ***********************************************************************
 * vmk_ServiceTimeChargeBeginWorld --                             */ /**
 *
 * \ingroup Accounting
 * \brief Begin charging work for a serivce to a world ID
 *
 * \param[in] serviceID    Type of service for the work.
 * \param[in] worldID      World on whose behalf the work is being done.
 *                         May be VMK_INVALID_WORLD_ID if the caller is
 *                         charging to a particular service category
 *                         not on behalf of any particular world.
 *
 * \note A world should not deschedule between the invocation of
 *       vmk_ServiceTimeChargeBeginWorld() and its corresponding
 *       vmk_ServiceTimeChargeEndWorld().
 * \note This function will not block.
 * \note Caller is responsible for invoking 
 *       vmk_ServiceTimeChargeEndWorld() when it finishes servicing the
 *       current context.
 *
 * \return An opaque handle to an accounting context.
 *
 ***********************************************************************
 */
vmk_ServiceTimeContext vmk_ServiceTimeChargeBeginWorld(
   vmk_ServiceAcctID serviceID,
   vmk_WorldID worldID);

/*
 ***********************************************************************
 * vmk_ServiceTimeChargeEndWorld --                               */ /**
 *
 * \ingroup Accounting
 * \brief Stop charging work against a world.
 *
 * \param[in] context   Accounting context to cease charging against.
 *                      May be VMK_SERVICE_TIME_CONTEXT_NONE  in which 
 *                      case no action is taken.
 *
 * \note This function will not block.
 * \note This function accounts for elapsed service time since previous
 *       call to vmk_ServiceTimeChargeBeginWorld().
 *
 ***********************************************************************
 */
void vmk_ServiceTimeChargeEndWorld(
   vmk_ServiceTimeContext context);

/*
 ***********************************************************************
 * vmk_ServiceTimeChargeSetWorld --                               */ /**
 *
 * \ingroup Accounting
 * \brief Set the worldID for charging the work.
 *
 * \param[in] context   Accounting context used to charge.
 *                      "context" can be VMK_SERVICE_TIME_CONTEXT_NONE in
 *                      which case, the currently established context
 *                      will be used to charge for given worldID 
 *
 * \note This function will not block.
 *
 * \param[in] worldID   World on whose behalf the work is being done.
 *
 ***********************************************************************
 */
void
vmk_ServiceTimeChargeSetWorld(vmk_ServiceTimeContext context,
                              vmk_WorldID worldID);

#endif /* _VMKAPI_ACCOUNTING_H_ */
/** @} */
