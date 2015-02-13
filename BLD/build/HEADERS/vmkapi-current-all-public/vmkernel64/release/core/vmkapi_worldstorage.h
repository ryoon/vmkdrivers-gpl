/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Per-World Storage                                              */ /**
 * \addtogroup Core
 * @{                                                
 * \defgroup WorldStorage Per-World Storage
 * @{
 *
 * Worlds are the basic schedulable entity in the VMKernel.  The per-
 * world storage system allows the allocation of an identifier that
 * can be used to retrieve a block of memory that will be available
 * in each world in the system.
 *
 * Accesses to the current world's per-world storage area may be
 * performed at any time.  Accesses to other worlds' storage area
 * provide reference counting semantics so that a world's storage
 * will not disappear while it is being accessed from another world.
 *
 * Constructors and destructors may be optionally provided to ensure
 * that the per-world storage memory is properly allocated and cleaned
 * up when worlds are created and destroyed.
 * 
 ***********************************************************************
 */

#ifndef _VMKAPI_WORLDSTORAGE_H_
#define _VMKAPI_WORLDSTORAGE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle for a per-world storage region.
 */
typedef vmk_uint64 vmk_WorldStorageHandle;

/**
 * \brief World storage allocation type.
 */
typedef enum {
   VMK_WORLD_STORAGE_TYPE_SIMPLE = 0,
} vmk_WorldStorageType;

/*
 ***********************************************************************
 * vmk_WorldStorageConstructor --                                 */ /**
 *
 * \brief Object constructor - optional user defined callback function.
 *
 * The constructor runs for each world's storage area when the area
 * is allocated. The return value may be used to indicate that
 * initializing the storage failed.  Since this will cause
 * world creation to fail, failure should only be returned for
 * extremely important reasons.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] wid     World ID of the world owning this storage.
 * \param[in] object  Pointer to the storage for the object.
 * \param[in] size    Size of object in bytes.
 * \param[in] arg     User-provided argument.
 *
 * \retval VMK_OK   Indicates object construction has succeeded.
 * \retval Other    Indicates that object construction failed.  If object
 *                  construction fails during storage allocation, the
 *                  allocation will fail, and all constructed objects
 *                  will be destroyed.  If object construction fails during
 *                  world creation, the world creation will fail.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_WorldStorageConstructor)(vmk_WorldID wid, 
                                                        void *object,
                                                        vmk_ByteCountSmall size,
                                                        vmk_AddrCookie arg);


/*
 ***********************************************************************
 * vmk_WorldStorageDestructor --                                  */ /**
 *
 * \brief Object destructor - optional user defined callback function.  
 *
 * The destructor runs for each world's storage area before the area
 * is freed.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] wid     World ID of the world owning this storage.
 * \param[in] object  Pointer to the storage for the object.
 * \param[in] size    Size of object in bytes.
 * \param[in] arg     User-provided argument.
 *
 ***********************************************************************
 */
typedef void (*vmk_WorldStorageDestructor)(vmk_WorldID wid, void *object, 
                                           vmk_ByteCountSmall size, 
                                           vmk_AddrCookie arg);


/**
 * \brief World storage allocation properties structure.
 *
 * Allocation properties for a per-world storage allocation.
 */
typedef struct {
   /** \brief Type of world storage allocation. */
   vmk_WorldStorageType type;

   /** \brief Allocating module ID. */
   vmk_ModuleID moduleID;

   /** \brief Size of the allocation, in bytes. */
   vmk_ByteCountSmall size;

   /** \brief Alignment of the allocation, in bytes. */
   vmk_ByteCountSmall align;

   /**
    * \brief Object constructor.  
    *
    * If set to NULL, the memory will be initialized to 0.
    */
   vmk_WorldStorageConstructor constructor;

   /** 
    * \brief Object destructor.
    *
    * Set to NULL if no destructor is desired.
    */
   vmk_WorldStorageDestructor destructor;

   /** \brief Client-specified argument for destructor and constructor. */
   vmk_AddrCookie arg;
} vmk_WorldStorageProps;

#define VMK_WORLD_STORAGE_HANDLE_INVALID ((vmk_WorldStorageHandle) -1)

/*
 ***********************************************************************
 * vmk_WorldStorageCreate --                                      */ /**
 *
 * \brief Create a new per-world storage area.
 *
 * Creates a new per-world storage area of the specified size and 
 * alignment.  If non-NULL, the constructor is invoked on all existing
 * worlds before this function returns.  If non-NULL, the destructor
 * will be invoked on each world as it dies, or when the storage area
 * is deleted by vmk_WorldStorageDestroy.
 *
 * \note   This function may block.
 *
 * \param[in]  props  Properties for the allocation.
 * \param[out] handle Lookup handle.
 *
 * \retval VMK_OK          Memory was allocated and constructed.
 * \retval VMK_NO_MEMORY   Not enough space was available for this
 *                         operation.
 * \retval Other           The return value of props->cons may be
 *                         returned by this function.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldStorageCreate(vmk_WorldStorageProps *props,
                                        vmk_WorldStorageHandle *handle);


/*
 ***********************************************************************
 * vmk_WorldStorageDestroy --                                     */ /**
 *
 * \brief Delete an existing per-world storage area.
 *
 * Deletes the per-world storage area identified by handle.  If
 * this storage area has a non-NULL destructor, the destructor is
 * invoked for all worlds before this function will return.
 *
 * This function must be called from blockable context.  It may need
 * to invoke the destructor a large number of times, and may need to
 * wait for some worlds to be cleaned up.  So this function may take
 * a long time to complete running.
 * 
 * \note   This function may block.
 *
 * \param[in] handle Handle to be deleted.
 *
 * \retval VMK_OK          The storage associated with handle was
 *                         deleted.
 * \retval VMK_BAD_PARAM   handle was invalid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldStorageDestroy(vmk_WorldStorageHandle handle);


/*
 ***********************************************************************
 * vmk_WorldStorageLookUpLocal --                                 */ /**
 *
 * \brief Returns a pointer to the current world's per-world storage.
 *
 * Retrives the storage associated with handle for the current world.
 * This object does not need to be released.
 *
 * The object returns will only be valid as long as the current world
 * is valid and the per-world storage element has not been freed.
 * This object should only be used by the current world.
 * The pointer should not be stored in global variables where it
 * can be read by other worlds, since it will become invalid when
 * the current world terminates.
 * 
 * \note   This function will not block.
 *
 * \param[in]  handle Handle to look up.
 * \param[out] object Pointer to the per-world storage.
 *
 * \retval VMK_OK          The returned object is valid to use.
 * \retval VMK_BAD_PARAM   Handle was invalid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldStorageLookUpLocal(vmk_WorldStorageHandle handle, 
                                             void **object);


/*
 ***********************************************************************
 * vmk_WorldStorageLookUp --                                      */ /**
 *
 * \brief Returns a pointer to a world's per-world storage.
 *
 * Retrives the storage associated with handle for the world wid.
 * This object must subsequently be released with 
 * vmk_WorldStorageRelease() after using the returned object.
 * 
 * \note   This function will not block.
 *
 * \param[in]  wid    World whose storage element will be returned.
 * \param[in]  handle Handle to look up.
 * \param[out] object Pointer to the per-world storage.
 *
 * \retval VMK_OK          The returned object is valid to use.
 * \retval VMK_BAD_PARAM   Handle was invalid
 * \retval VMK_NOT_FOUND   Wid was not a valid world ID.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldStorageLookUp(vmk_WorldID wid, 
                                        vmk_WorldStorageHandle handle,
                                        void **object);


/*
 ***********************************************************************
 * vmk_WorldStorageRelease --                                     */ /**
 *
 * \brief Releases a previously retrieved per-world storage pointer.
 *
 * Releases a pointer previous acquired with vmk_WorldStorageLookUp.  
 * Each pointer acquired with vmk_WorldStorageLookUp must be 
 * subsequently released by vmk_WorldStorageRelease.
 * 
 * \note   This function will not block.
 *
 * \param[in]  wid    World whose storage element will be returned.
 * \param[out] object Pointer to the per-world storage.
 *
 * \retval VMK_OK          The returned object is valid to use.
 * \retval VMK_BAD_PARAM   Handle was invalid.
 * \retval VMK_NOT_FOUND   Wid was not a valid world ID.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldStorageRelease(vmk_WorldID wid, void **object);

#endif /* _VMKAPI_WORLDSTORAGE_H_ */
/** @} */
/** @} */
