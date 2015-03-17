/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * System                                                         */ /**
 *
 * \defgroup System General Kernel System Interfaces
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SYSTEM_H_
#define _VMKAPI_SYSTEM_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief
 * The maximum length of a system version info string including the
 * trailing nul.
 */
#define VMK_SYSTEM_VERSION_INFO_MAX_LEN      32

/** \brief A reboot handler callback */
typedef void (*vmk_RebootHandler)(void *data);

/**
 * \brief States for a vmkernel system.
 */
typedef enum {
   /** \brief System is in a normal running state */
   VMK_SYSTEM_STATE_NORMAL=0,

   /** \brief System is currently panicing */
   VMK_SYSTEM_STATE_PANIC=1,

   /** \brief System is currently shutting down/rebooting */
   VMK_SYSTEM_STATE_UNLOADING=2
} vmk_SystemState;

/**
 * \brief General-interest version and name information about vmkernel
 */
typedef struct {
   const char *productName;
   const char *productVersion;
   const char *buildVersion;
} vmk_SystemVersionInfo;

/*
 ***********************************************************************
 * vmk_RegisterRebootHandler --                                   */ /**
 *
 * \ingroup System
 * \brief Register a handler to be called before reboot.
 *
 * \note This function will not block
 *
 * \param[in] handler   Handler function to call before reboot.
 * \param[in] data      Pointer to be passed to reboot handler function.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RegisterRebootHandler(
    vmk_RebootHandler handler, void *data);

/*
 ***********************************************************************
 * vmk_UnregisterRebootHandler --                                 */ /**
 *
 * \ingroup System
 * \brief Unregister a reboot handler.
 *
 * \note This function will not block
 *
 * \param[in] handler   Handler function to unregister.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UnregisterRebootHandler(
    vmk_RebootHandler handler);

/*
 ***********************************************************************
 * vmk_SystemCheckState --                                        */ /**
 *
 * \ingroup System
 * \brief Check if the system is in a particular state.
 *
 * \note A system may be in more than one state simultaneously.
 *
 * \note This is a snapshot of the state at the time of the call and
 *       is subject to change.
 *
 * \note This function will not block
 *
 * \param[in]  state    State to check for.
 *
 * \retval VMK_TRUE     The system is in the specified state.
 * \retval VMK_FALSE    The system is not in the specified state.
 *
 ***********************************************************************
 */
vmk_Bool vmk_SystemCheckState(
   vmk_SystemState state);

/*
 ***********************************************************************
 * vmk_SystemGetIDString --                                       */ /**
 *
 * \ingroup System
 * \brief Get a persistent system identifier string.
 *
 * \note The identifier string conforms to the string
 *       representation cited in section three of RFC 4122.
 *
 * \note This function will not block
 *
 * \param[out] id    The identifier string.
 *
 * \retval VMK_OK      The identifier string was successfully retrieved.
 * \retval VMK_FAILURE The identifier string was not successfully
 *                     retrieved.  This may be because it has not yet
 *                     been configured.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SystemGetIDString(
   const char *id[]);

/*
 ***********************************************************************
 * vmk_SystemGetVersionInfo --                                    */ /**
 *
 * \ingroup System
 * \brief Get string-style version information about vmkernel, for
 *        display purposes only.  No run-time checks should be
 *        performed on the result of this call.
 *
 * \note This function will not block
 *
 * \param[out] info  Version information about vmkernel for display
 *                   purposes.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SystemGetVersionInfo(
   vmk_SystemVersionInfo *info);

#endif /* _VMKAPI_SYSTEM_H_ */
/** @} */
