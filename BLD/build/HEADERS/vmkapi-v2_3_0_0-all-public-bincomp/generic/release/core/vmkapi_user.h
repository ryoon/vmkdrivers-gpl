/* **********************************************************
 * Copyright 2010-2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * User Space Memory Interface                                             */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup UserMem User Space Memory
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_USERMEMORY_H_
#define _VMKAPI_CORE_USERMEMORY_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/*
 ***********************************************************************
 * vmk_UserMapCallback --                                         */ /**
 *
 * \brief Callback function invoked when user mapping is released.
 *
 * \param[in] callbackParam    Opaque parameter for callback function.
 *
 ***********************************************************************
 */
typedef void (*vmk_UserMapCallback)(void *);


/*
 ***********************************************************************
 * vmk_UserMapFaultCallback --                                    */ /**
 *
 * \brief Callback when a page fault occurs on a user mapping.
 *
 * This callback function will be invoked any time a page fault occurs
 * on a page mapped through vmk_UserMap() and the callback was
 * specified.  This can occur if the page is specified in a call to
 * vmk_UserMapEnableNotify().
 *
 * This callback is expected to provide an MPN to back the specified
 * VPN.  The vmkernel will attempt to update the page table entry with
 * the provided MPN and report if that was successful when invoking
 * a second callback, vmk_UserMapPostFaultCallback.
 *
 * \note The vmkernel assumes the MPN will remain valid until this page
 *       is unmapped or has been marked not present via
 *       vmk_UserMapEnableNotify().
 *
 * \note This callback may block.
 *
 * \note vmk_UserMapPostFaultCallback is only invoked for a mapping
 *       when this function returns VMK_OK.
 *
 * \param[in]  notifyParam    Opaque parameter provided to
 *                            vmk_UserMap().
 * \param[in]  worldID        The world that owns this mapping.
 * \param[in]  vpn            The vpn accessed.
 * \param[out] mpn            The mpn to place in the page table
 *                            entry for this vpn.
 *
 * \retval VMK_OK                The callback successfully completed.
 * \retval VMK_MAPFAULT_RETRY    The callback failed but the fault
 *                               handler should retry satisfying
 *                               this mapping.  The
 *                               vmk_UserMapPostFaultCallback will not
 *                               be invoked for this mapping.
 * \retval VMK_FAILURE           The callback failed and the fault
 *                               handler should not retry satisfying
 *                               this mapping.  Note this will likely
 *                               result in the userworld being killed.
 *                               The vmk_UserMapPostFaultCallback will
 *                               not be invoked for this mapping.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UserMapFaultCallback)(void *faultParam,
                                                     vmk_WorldID worldID,
                                                     vmk_VPN vpn,
                                                     vmk_MPN *mpn);

/*
 ***********************************************************************
 * vmk_UserMapPostFaultCallback --                                */ /**
 *
 * \brief Second callback when a page fault occurs on a user mapping.
 *
 * This callback function will be invoked by the system after a call to
 * vmk_UserMapFaultCallback that returned VMK_OK.  It is used to report
 * whether the page table entry was successfully filled with the MPN
 * specified or not.
 *
 * In cases where the page table entry was successfully filled, the
 * success argument will be VMK_TRUE.
 *
 * In cases where the page table entry was not succesfully filed, the
 * success argument will be VMK_FALSE.  The mapping will not be
 * satisfied and the system will either retry or fail to satisfy the
 * page fault depending on the nature of the error.  Any work done in
 * vmk_UserMapFaultCallback should be undone.
 *
 * \note This callback may block.
 *
 * \note At the point this callback is invoked, if success is VMK_TRUE,
 *       the page table entry has been set and the page fault handler
 *       has not yet returned.  The world accessing this VPN that
 *       generated the fault has not yet accessed the specified MPN,
 *       however another world in the same cartel accessing the same VPN
 *       may access the set MPN prior to this function being invoked.
 *
 * \param[in]  postFaultParam  Opaque parameter provided to
 *                             vmk_UserMap().
 * \param[in]  success         Whether the page table entry was
 *                             successfully populated.
 * \param[in]  worldID         The world that owns this mapping.
 * \param[in]  vpn             The vpn accessed.
 * \param[in]  mpn             The mpn populated or attempted to be
 *                             populated in the page table entry for
 *                             vpn.
 *
 ***********************************************************************
 */
typedef void (*vmk_UserMapPostFaultCallback)(void *postFaultParam,
                                             vmk_Bool success,
                                             vmk_WorldID worldID,
                                             vmk_VPN vpn,
                                             vmk_MPN mpn);

/** \brief Handle to a VMK_USER_MAP_SHARED mapping. */
typedef struct vmkUserMapHandleInt* vmk_UserMapHandle;


/** \brief Invalid handle. */
#define VMK_USER_MAP_HANDLE_INVALID     ((vmk_UserMapHandle)-1)

/**
 * \brief Types of mappings performed by vmk_UserMap().
 */
typedef enum vmk_UserMapType {
   /** \brief Mapping of provided MPNs. */
   VMK_USER_MAP_MPNS = 1,
   /**
    * \brief Mapping of anonymous memory.
    *
    * Anonymous memory is allocated internally by the system when the pages are
    * first accessed.
    * */
   VMK_USER_MAP_ANON = 2,
   /**
    * \brief Mapping of anonymous memory that may be shared between worlds.
    *
    * Anonymous memory is allocated internally by the system when the pages are
    * first accessed.
    *
    * \note The memory backing an anonymous shared region may not be swappable
    *       and should therefore be used sparingly.
    * */
   VMK_USER_MAP_SHARED = 3,
} vmk_UserMapType;

/**
 * \brief Properties of a vmk_UserMap() map request.
 */
typedef struct vmk_UserMapProps {
   /** \brief Module ID of module requesting mapping. */
   vmk_ModuleID moduleID;
   /** \brief Function to call when mapping is released. */
   vmk_UserMapCallback callbackFunction;
   /** \brief Opaque parameter for callbackFunction. */
   vmk_AddrCookie callbackParam;
   /** \brief Type of mapping. */
   vmk_UserMapType type;
   union {
      /** \brief For VMK_USER_MAP_MPNS mapping type. */
      struct {
         /**
          * \brief Pointer to map request structure.
          *
          * \note See Mapping section for description
          *       of map request structure.
          */
         vmk_MapRequest *mapRequest;
         /**
          * First function to invoke when a fault occurs on a user
          * mapping.  See description of vmk_UserMapFaultCallback for
          * more details.
          *
          * \note If faultFunction is specified then
          *       postfaultFunction must be specified as well.
          */
         vmk_UserMapFaultCallback faultFunction;
         /** \brief Opaque parameter for faultFunction. */
         vmk_AddrCookie faultParam;
         /**
          * Second function to invoke when a fault occurs on a user
          * mapping.  See description of vmk_UserMapPostNotifyCallback
          * for more details.
          *
          * \note If postFaultFunction is specified then faultFunction
          *       must be specified as well.
          */
         vmk_UserMapPostFaultCallback postFaultFunction;
         /** \brief Opaque parameter for postFaultFunction. */
         vmk_AddrCookie postFaultParam;
      } mpns;
      /** \brief For VMK_USER_MAP_ANON mapping type. */
      struct {
         /** \brief Attributes to use for this mapping. */
         vmk_MapAttrs mapAttrs;
         /** \brief Length of this mapping. */
         vmk_ByteCount mapLength;
      } anon;
      /** \brief For VMK_USER_MAP_SHARED mapping type. */
      struct {
         /**
          * \brief Create or attach mapping.
          *
          * If this Bool is VMK_TRUE then this is a mapping request to
          * create a shared region; if VMK_FALSE this is a mapping
          * request to attach to an existing shared region.
          *
          * On create, vmk_UserMap()'s handle argument will be filled with the
          * handle.  When attaching to this same mapping, that handle is
          * specified in vmk_UserMap()'s handle argument.
          *
          * The notifyFunction and restoreMapfunction may only be set when this
          * shared mapping is created.
          */
         vmk_Bool create;
         /** \brief Attributes to use for this mapping. */
         vmk_MapAttrs mapAttrs;
         /** \brief Length of this mapping. */
         vmk_ByteCount mapLength;
      } shared;
   };
} vmk_UserMapProps;


/*
 ***********************************************************************
 * vmk_UserMap --                                                 */ /**
 *
 * \brief Map the provided request into a contiguous virtual address
 *        space of current user world.
 *
 * \note The only supported mapping attributes are READONLY, READWRITE,
 *       WRITECOMBINE, and UNCACHED.  Any other attribute will cause
 *       mapping to fail with return status VMK_BAD_PARAM.
 *
 * \param[in]     props   Properties of this mapping request.
 * \param[in,out] vaddr   Pointer to virtual address of mapping
 *                        (non-zero to specify a virtual address,
 *                        or zero for default address).
 * \param[in,out] handle  When creating a shared mapping, the address
 *                        of the handle to be filled in.  When
 *                        attaching to a shared mapping, the handle
 *                        to attach to.
 *
 * \retval VMK_OK              Map is successful.
 * \retval VMK_BAD_PARAM       Input parameter is invalid.
 * \retval VMK_NO_MEMORY       Unable to allocate mapping request.
 * \retval VMK_NO_RESOURCES    Unable to allocate mapping request.
 * \retval VMK_INVALID_ADDRESS Requested address not in map range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserMap(
   vmk_UserMapProps *props,
   vmk_VA *vaddr,
   vmk_UserMapHandle *handle);

/*
 ***********************************************************************
 * vmk_UserUnmap --                                               */ /**
 *
 * \brief Unmap user world virtual address space mapped by vmk_UserMap().
 *
 * \param[in] type             Type of vmk_UserMap mapping to unmap.
 * \param[in] vaddr            Virtual address to unmap.
 * \param[in] length           Length of address space in bytes.
 *
 * \retval VMK_OK              Unmap is successful.
 * \retval VMK_NOT_FOUND       Virtual address and length not mapped.
 * \retval VMK_INVALID_ADDRESS Requested address is not page aligned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserUnmap(
   vmk_UserMapType type,
   vmk_VA vaddr,
   vmk_ByteCount length);

/*
 ***********************************************************************
 * vmk_UserAddValidMPNRange --                                    */ /**
 *
 * \brief Indicate a range of consecutive MPNs can be referenced by
 *        user worlds.
 *
 * \param[in] mpn              First MPN in range.
 * \param[in] numPages         Number of machine pages in range.
 *
 * \retval VMK_OK              MPNs added to user worlds.
 * \retval VMK_BAD_PARAM       Input parameter is invalid.
 * \retval VMK_NO_MEMORY       Unable to allocate memory for request.
 * \retval VMK_INVALID_PAGE_NUMBER  MPN range intersects with existing
 *                             MPN range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserAddValidMPNRange(
   vmk_MPN mpn,
   vmk_uint32 numPages);

/*
 ***********************************************************************
 * vmk_UserRemoveValidMPNRange --                                 */ /**
 *
 * \brief Remove a range of consecutive MPNs from user worlds.
 *
 * \param[in] mpn              First MPN in range.
 * \param[in] numPages         Number of machine pages in range.
 *
 * \retval VMK_OK              MPNs removed from user worlds.
 * \retval VMK_NOT_FOUND       MPN range not found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserRemoveValidMPNRange(
   vmk_MPN mpn,
   vmk_uint32 numPages);

/*
 ***********************************************************************
 * vmk_UserPinPage --                                             */ /**
 *
 * \brief Marks the specified VPN in the specified world as not
 *        swappable.
 *
 * \note  This VPN must have been mapped by a call to vmk_UserMap() with
 *        type VMK_USER_MAP_ANON or VMK_USER_MAP_SHARED.  There is no
 *        need to pin pages of type VMK_USER_MAP_MPNS since they are
 *        by definition pinned as long as they are mapped.
 *
 * \param[in]  worldID         ID of world whose mapping will be pinned.
 * \param[in]  vpn             VPN to pin.
 * \param[out] mpn             MPN backing this pinned VPN.
 *
 * \retval VMK_OK              VPNs was pinned successfully.
 * \retval VMK_BAD_PARAM       An invalid argument was provided.
 * \retval VMK_INVALID_WORLD   An invalid worldID was provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserPinPage(
   vmk_WorldID worldID,
   vmk_VPN vpn,
   vmk_MPN *mpn);

/*
 ***********************************************************************
 * vmk_UserUnpinPage --                                           */ /**
 *
 * \brief Marks the specified VPN in the specified world as swappable.
 *
 * \note  This VPN must have been pinned by a call to vmk_UserPinPage().
 *
 * \param[in]  worldID         ID of world whose mapping will be
 *                             unpinned.
 * \param[in]  vpn             VPN to unpin.
 *
 * \retval VMK_OK              VPNs was unpinned successfully.
 * \retval VMK_BAD_PARAM       An invalid argument was provided.
 * \retval VMK_INVALID_WORLD   An invalid worldID was provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserUnpinPage(
   vmk_WorldID worldID,
   vmk_VPN vpn);

/*
 ***********************************************************************
 * vmk_UserMapEnableNotify --                                     */ /**
 *
 * \brief Enables notification of next access to the specified vpn.
 *
 * This enables a single notification for the specified vpn the next
 * time it is accessed within the specified world.  The notification is
 * sent by invoking the vmk_UserMapFaultCallback.
 *
 * \note Once the notification is received the notificaion status is
 *       automatically disabled.  vmk_UserMapDisableNotify() need not
 *       be called for a vpn that previously was specified in a call to
 *       this function and also received the notification.
 *
 * \note This VPN must have been mapped by the specified world using
 *       vmk_UserMap() and a vmk_UserMapFaultCallback must have been
 *       provided.  This also means that only VPNs mapped with mapping
 *       types that allow a vmk_UserMapFaultCallback to be specified can
 *       be provided to this function.
 *
 * \param[in]  worldID         ID of world with mapping.
 * \param[in]  vpn             VPN to notify when accessed.
 *
 * \retval VMK_OK              VPN set not present successfully.
 * \retval VMK_BAD_PARAM       An invalid argument was provided.
 * \retval VMK_INVALID_WORLD   An invalid worldID was provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserMapEnableNotify(
   vmk_WorldID worldID,
   vmk_VPN vpn);

/*
 ***********************************************************************
 * vmk_UserMapDisableNotify --                                    */ /**
 *
 * \brief Disables notification of a page access for the specified vpn.
 *
 * \note This VPN must have had vmk_UserMapEnableNotify() called for it
 *       and the notification should not already have been received.  If
 *       the notification was already received, the VPN is considered
 *       a bad parameter and this call will fail.
 *
 * \param[in]  worldID         The world whose mapping should be
 *                             changed.
 * \param[in]  vpn             VPN to mark present.
 *
 * \retval VMK_OK              VPN set present successfully.
 * \retval VMK_BAD_PARAM       An invalid argument was provided.
 * \retval VMK_INVALID_WORLD   An invalid worldID was provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserMapDisableNotify(
   vmk_WorldID worldID,
   vmk_VPN vpn);


/*
 ******************************************************************************
 * vmk_CopyFromUser --                                                   */ /**
 *
 * Copy memory from a user space application into a kernel buffer
 *
 * \note Caller must not hold any spinlocks.
 * \note Must be called from a blockable context
 *
 * \param[in] dest   Copy-to location.
 * \param[in] src    Copy-from location.
 * \param[in] len    Amount to copy.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_CopyFromUser(
   vmk_VA dest,
   vmk_VA src,
   vmk_ByteCount len);

/*
 ******************************************************************************
 * vmk_CopyToUser --                                                     */ /**
 *
 * Copy memory from a kernel buffer into a user space application.
 *
 * \note Caller must not hold any spinlocks.
 * \note Must be called from a blockable context
 *
 * \param[in] dest   Copy-to location.
 * \param[in] src    Copy-from location.
 * \param[in] len    Amount to copy.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_CopyToUser(
   vmk_VA dest,
   vmk_VA src,
   vmk_ByteCount len);


#endif
/** @} */
/** @} */
