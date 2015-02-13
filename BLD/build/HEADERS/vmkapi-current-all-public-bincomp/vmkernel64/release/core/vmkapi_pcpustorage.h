/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Per-PCPU Storage                                               */ /**
 * \addtogroup Core
 * @{
 * \defgroup PCPUStorage Per-PCPU Storage
 * @{
 *
 * The per-PCPU storage system allows the allocation of a portion of
 * memory for each PCPU on the system.  A handle is provided when
 * the storage is created and a pointer to the memory for a particular
 * PCPU can be looked up.  That pointer must be released when no longer
 * used to ensure proper reference counting.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_PCPUSTORAGE_H_
#define _VMKAPI_PCPUSTORAGE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle for a per-pcpu storage region.
 */
typedef struct vmk_PCPUStorageHandleInt *vmk_PCPUStorageHandle;

/**
 * \brief PCPU storage allocation type.
 */
typedef enum {
   /** Storage mostly intended for read access. */
   VMK_PCPU_STORAGE_TYPE_READ = 0,
   /** Storage intented for read and write access, but mostly from local PCPU. */
   VMK_PCPU_STORAGE_TYPE_WRITE_LOCAL = 1,
   /** Storage intented for read and write access from any PCPU. */
   VMK_PCPU_STORAGE_TYPE_WRITE_REMOTE = 2,
   /** Marker for last type. */
   VMK_PCPU_STORAGE_TYPE_MAX = 3,
} vmk_PCPUStorageType;


/*
 ***********************************************************************
 * vmk_PCPUStorageConstructor --                                  */ /**
 *
 * \brief Object constructor - optional user defined callback function.
 *
 * The constructor runs for each PCPU's storage area when the area
 * is allocated. The return value may be used to indicate that
 * initializing the storage failed.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] pcpu    PCPU number whose storage is being constructed.
 * \param[in] object  Pointer to the storage for the object.
 * \param[in] size    Size of object in bytes.
 * \param[in] arg     User-provided argument.
 *
 * \retval VMK_OK   Indicates object construction has succeeded.
 * \retval Other    Indicates that object construction failed.  If object
 *                  construction fails during storage allocation, the
 *                  allocation will fail, and all constructed objects
 *                  will be destroyed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PCPUStorageConstructor)(vmk_PCPUID pcpu,
                                                       void *object,
                                                       vmk_ByteCountSmall size,
                                                       vmk_AddrCookie arg);


/*
 ***********************************************************************
 * vmk_PCPUStorageDestructor --                                   */ /**
 *
 * \brief Object destructor - optional user defined callback function.
 *
 * The destructor runs for each PCPU's storage area before the area
 * is freed.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] pcpu    PCPU number whose storage is being constructed.
 * \param[in] object  Pointer to the storage for the object.
 * \param[in] size    Size of object in bytes.
 * \param[in] arg     User-provided argument.
 *
 ***********************************************************************
 */
typedef void (*vmk_PCPUStorageDestructor)(vmk_PCPUID pcpu,
                                          void *object,
                                          vmk_ByteCountSmall size,
                                          vmk_AddrCookie arg);


/**
 * \brief PCPU storage allocation properties structure.
 *
 * Allocation properties for a per-PCPU storage allocation.
 */
typedef struct {
   /** \brief Type of PCPU storage allocation. */
   vmk_PCPUStorageType type;

   /** \brief Allocating module ID. */
   vmk_ModuleID moduleID;

   /** \brief Name of this storage. */
   vmk_Name name;

   /** \brief Size of the allocation, in bytes. */
   vmk_ByteCountSmall size;

   /** \brief Alignment of the allocation, in bytes. */
   vmk_ByteCountSmall align;

   /**
    * \brief Object constructor.
    *
    * If set to NULL, the memory will be initialized to 0.
    */
   vmk_PCPUStorageConstructor constructor;

   /**
    * \brief Object destructor.
    *
    * Set to NULL if no destructor is desired.
    */
   vmk_PCPUStorageDestructor destructor;

   /** \brief Client-specified argument for destructor and constructor. */
   vmk_AddrCookie arg;
} vmk_PCPUStorageProps;


#define VMK_PCPU_STORAGE_HANDLE_INVALID ((vmk_PCPUStorageHandle) -1)

/*
 ***********************************************************************
 * vmk_PCPUStorageCreate --                                       */ /**
 *
 * \brief Create a new per-pcpu storage area.
 *
 * Creates a new per-pcpu storage area of the specified size and
 * alignment.
 *
 * \note This function will not block.
 *
 * \param[in]  props   Properties for the allocation.
 * \param[out] handle  Lookup handle.
 *
 * \retval VMK_OK          Memory was allocated and constructed.
 * \retval VMK_NO_MEMORY   Not enough space was available for this
 *                         operation.
 * \retval VMK_BAD_PARAM   A bad parameter was provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCPUStorageCreate(vmk_PCPUStorageProps *props,
                                       vmk_PCPUStorageHandle *handle);


/*
 ***********************************************************************
 * vmk_PCPUStorageDestroy --                                      */ /**
 *
 * \brief Delete an existing per-pcpu storage area.
 *
 * Deletes the per-pcpu storage area identified by handle.
 *
 * \note This function will not block.
 *
 * \param[in] handle  Handle previously created by
 *                    vmk_PCPUStorageCreate().
 *
 * \retval VMK_OK          The storage associated with handle was
 *                         deleted.
 * \retval VMK_BAD_PARAM   handle was invalid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCPUStorageDestroy(vmk_PCPUStorageHandle handle);


/*
 ***********************************************************************
 * vmk_PCPUStorageLookUp --                                       */ /**
 *
 * \brief Returns a pointer to a PCPU's per-PCPU storage.
 *
 * Retrieves the storage associated with handle for the PCPU.
 * This object must subsequently be released with
 * vmk_PCPUStorageRelease() after using the returned object.  A handle
 * cannot be destroyed until the pointer is released.
 *
 * \note This function will not block.
 *
 * \param[in]  pcpu    PCPU whose storage element will be returned.
 * \param[in]  handle  Handle to look up.
 * \param[out] object  Pointer to the per-PCPU storage.
 *
 * \retval VMK_OK          The returned object is valid to use.
 * \retval VMK_BAD_PARAM   PCPU or Handle was invalid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCPUStorageLookUp(vmk_PCPUID pcpu,
                                       vmk_PCPUStorageHandle handle,
                                       void **object);


/*
 ***********************************************************************
 * vmk_PCPUStorageRelease --                                      */ /**
 *
 * \brief Releases a previously retrieved per-pcpu storage pointer.
 *
 * Releases a pointer previous acquired with vmk_PCPUStorageLookUp.
 * Each pointer acquired with vmk_PCPUStorageLookUp must be
 * subsequently released by vmk_PCPUStorageRelease.
 *
 * \note This function will not block.
 *
 * \param[in] handle  Handle that was looked up.
 * \param[in] object  Pointer that was retrieved.
 *
 * \retval VMK_OK          The object was released.
 * \retval VMK_BAD_PARAM   PCPU or Handle was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PCPUStorageRelease(vmk_PCPUStorageHandle handle,
                                        void *object);

#endif /* _VMKAPI_PCPUSTORAGE_H_ */
/** @} */
/** @} */
