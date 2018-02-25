/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * Worldlet Types                                                        */ /**
 * \addtogroup Worldlet
 *
 * Shared worldlet types.
 *
 * @{
 * \defgroup Deprecated Deprecated APIs
 * @{
 *******************************************************************************
 */
#ifndef _VMKAPI_WORLDLET_TYPES_H_
#define _VMKAPI_WORLDLET_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Handle for vmk_Worldlet
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef struct vmk_WorldletInt* vmk_Worldlet;
#define VMK_INVALID_WORLDLET    ((vmk_Worldlet)NULL)

/**
 * \brief Worldlet identifier
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef vmk_uint32 vmk_WorldletID;
#define VMK_INVALID_WORLDLET_ID    ((vmk_WorldletID)-1)

/** \cond nodoc */
#define VMK_WDT_OPT_LIST(action)                                                 \
   action(WDT_OPT_FORCE_IPI_DISPATCH, "force IPI dispatch", ForceIPIDispatch)    \
   action(WDT_OPT_NON_INTERFERING, "ignore interference effect", NonInterfering) \
   action(WDT_OPT_ACTION_AFFINITY, "action affinity", ActionAffinity)            \
   action(WDT_OPT_ENABLED_VMKSTATS, "enable vmkstats", EnableVmkstats)

#define VMK_WDT_OPT_LIST_POPULATE(name, ignore1, ignore2) \
   /** \brief Option name */ VMK_##name,
/** \endcond */

/**
 * \brief vmk_WorldletOptions
 *        Worldlet Option Flags - various flags to control worldlet behavior.
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef enum vmk_WorldletOptions {
   VMK_WDT_OPT_LIST(VMK_WDT_OPT_LIST_POPULATE)
} vmk_WorldletOptions;

#undef VMK_WDT_OPT_LIST_POPULATE

/**
 * \brief States of worldlets.
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef enum {
   VMK_WDT_RELEASE = 1,
   VMK_WDT_SUSPEND = 2,
   VMK_WDT_READY = 4,
} vmk_WorldletState;

/**
 * \brief Data about execution provided by the worldlet.
 *
 *        A pointer to this structure is passed to a running worldlet and the
 *        worldlet may report various items to the vmkernel about its
 *        exectution.
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef struct vmk_WorldletRunData {
   /** \brief Set by the worldlet to indicate completion state. */
   vmk_WorldletState    state;

   /** \brief Performance instrumentation counters.
    *
    *         Set by the worldlet to counts worldlet-specific events
    *         (e.g. number of packets process per worldlet invocation). Totals
    *         and EWMA of these counters are published via VSI.
    */
   vmk_uint64           userData1;
   vmk_uint64           userData2;
} vmk_WorldletRunData;


#endif /* _VMKAPI_WORLDLET_TYPES_H_ */
/** @} */
/** @} */
