/* **********************************************************
 * Copyright 2006 - 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Hash                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Hash Hash
 *
 * The following are interfaces for a hash abstraction which enables
 * arbitrary key-value pair storage.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_HASH_H_
#define _VMKAPI_HASH_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "base/vmkapi_heap.h"

/**
 * \brief Invalid hash handle
 */
#define VMK_INVALID_HASH_HANDLE NULL

/**
 * \brief Handle to a hash table
 */
typedef void *vmk_HashTable;

/**
 * \brief Key used to store key-value pair. A key can really be anything ranging
 *        from a string to integer to whatever data structure one would like
 *        to use as a key.
 */
typedef void *vmk_HashKey;

/**
 * \brief Hash implementation uses these flags to process key.
 */
typedef vmk_uint64 vmk_HashKeyFlags;

/** No flags. */
#define VMK_HASH_KEY_FLAGS_NONE       0x0

/**
 * Hash implementation should do a local copy of the key on insertion
 * and do not assume the memory backing up the key will be persistent.
 */
#define VMK_HASH_KEY_FLAGS_LOCAL_COPY 0x1

/**
 * \brief Key length.
 */
typedef vmk_uint32 vmk_HashKeyLen;

/**
 * \brief Value used as part of a key-value pair. There is no restriction
 *        related to the internal value representation.
 */
typedef void *vmk_HashValue;

/**
 * \brief Key iterator commands.
 */
typedef vmk_uint64 vmk_HashKeyIteratorCmd;

/**
 * Key type supported
 */
typedef enum vmk_HashKeyType {

   /* Integer key */
   VMK_HASH_KEY_TYPE_INT = 0,

   /* String key */
   VMK_HASH_KEY_TYPE_STR,

   /* Opaque key */
   VMK_HASH_KEY_TYPE_OPAQUE,
} vmk_HashKeyType;

/** Keep iterating through the hash table. */
#define VMK_HASH_KEY_ITER_CMD_CONTINUE 0x0

/** Stop iterating through the hash table. */
#define VMK_HASH_KEY_ITER_CMD_STOP     0x1

/** Delete the iterated key-value pair. */
#define VMK_HASH_KEY_ITER_CMD_DELETE   0x2


/*
 *******************************************************************************
 * vmk_HashValueAcquire --                                                */ /**
 *
 * \brief Atomically acquire a reference to a value stored in the hash table.
 *
 * \note  This function must not block.
 *
 * \param[in] value     Value.
 *
 *******************************************************************************
 */
typedef void (*vmk_HashValueAcquire)(vmk_HashValue value);


/*
 *******************************************************************************
 * vmk_HashValueRelease --                                                */ /**
 *
 * \brief Atomically release a reference to a value stored in the hash table.
 *
 * \note  This function must not block.
 *
 * \param[in] value     Value.
 *
 *******************************************************************************
 */
typedef void (*vmk_HashValueRelease)(vmk_HashValue value);


/**
 * Properties for allocating a hash table.
 */
typedef struct vmk_HashProperties {

   /** Module ID request the hash table */
   vmk_ModuleID         moduleID;

   /** The heap used for hash table internal allocation */
   vmk_HeapID           heapID;

   /** Type of keys to be used */
   vmk_HashKeyType      keyType;

   /** Flags relating to keys */
   vmk_HashKeyFlags     keyFlags;

   /**
    * Key size:
    *
    * (1) must be 0 for VMK_HASH_KEY_TYPE_INT
    * (2) maximum string length expected if VMK_HASH_KEY_TYPE_STR
    * (3) opaque object size expected if VMK_HASH_KEY_TYPE_OPAQUE
    */
   vmk_uint32           keySize;

   /** A "best guess" number of expected entries for hash bucket sizing */
   vmk_uint32           nbEntries;

   /** Function to acquire a reference to a value */
   vmk_HashValueAcquire acquire;

   /** Function to release a reference to a value */
   vmk_HashValueRelease release;

} vmk_HashProperties;


/*
 *******************************************************************************
 * vmk_HashGetAllocSize --                                                */ /**
 *
 * \brief Return a best estimate amount of memory necessary to operate the
 *        hash table.
 *
 * \param[in]  nbEntries A "best guess" number of expected entries for hash
 *                       buckets sizing.
 *
 * \retval Best estimate amount of memory in bytes.
 *
 *******************************************************************************
 */
vmk_ByteCount
vmk_HashGetAllocSize(vmk_uint32 nbEntries);



/*
 *******************************************************************************
 * vmk_HashAlloc --                                                       */ /**
 *
 * \brief Allocate a new hash table with provided properties.
 *
 * \note vmk_HashRelease() needs to be called once done with the hash table.
 *
 * \note The hash table returned does not come with locking, it is the
 *       caller's responsibility to provide such mechanism. Lookups can be
 *       lockless, however, if the hash table has functions to atomically 
 *       acquire and release references to values. Callers must provide locking
 *       for operations which insert or remove entries from the hash table.
 *
 * \param[in]  props     Properties for the new hash table.
 * \param[out] hdl       Hash handle allocated for later operations.
 *
 * \retval VMK_OK        Hash table initialization was successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to NULL.
 *                       If props pointer equals to NULL.
 *                       If hash properties are invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashAlloc(vmk_HashProperties *props,
              vmk_HashTable *hdl);


/*
 *******************************************************************************
 * vmk_HashRelease --                                                     */ /**
 *
 * \brief Release a hash table.
 *
 * \note  This function may block.
 *
 * \param[in] hdl        Hash handle.
 *
 * \retval VMK_OK        Hash table was released successful.
 * \retval VMK_BUSY      If the hash table is not empty.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashRelease(vmk_HashTable hdl);


/*
 *******************************************************************************
 * vmk_HashDeleteAll --                                                       */ /**
 *
 * \brief Delete every entry in a hash table.
 *
 * \note If the hash table does not have a value release function, the client
 *       should make sure that the right clean up is done beforehand.
 *
 * \note A subsequent call to vmk_HashIsEmpty() on the given hash table should
 *       return VMK_TRUE.
 *
 * \param[in] hdl        Hash handle.
 *
 * \retval VMK_OK        Every single entry of the hash table was deleted.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashDeleteAll(vmk_HashTable hdl);

/*
 *******************************************************************************
 * vmk_HashKeysCount--                                                    */ /**
 *
 * \brief Count the number of keys present in a given hash table.
 *
 * \param[in]  hdl       Hash handle.
 * \param[out] keysCnt   The number of keys in the hash table.
 *
 * \retval VMK_OK        Hash table counting operation succeeded.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE or if keysCnt
 *                       is NULL.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeysCount(vmk_HashTable hdl,
                  vmk_uint64 *keysCnt);

/*
 *******************************************************************************
 * vmk_HashIsEmpty --                                                     */ /**
 *
 * \brief Check if a given hash table is empty.
 *
 * \param[in] hdl       Hash handle.
 *
 * \retval VMK_TRUE     Hash is empty.
 * \retval VMK_FALSE    Hash has at least one key-value pair.
 *
 *******************************************************************************
 */
vmk_Bool
vmk_HashIsEmpty(vmk_HashTable hdl);


/*
 *******************************************************************************
 * vmk_HashKeyIterator --                                                 */ /**
 *
 * \brief Iterator used to iterate the key-value pairs on a given hash table.
 *
 * \note The return value is a command set given back to the iterator engine to
 *       let it know what to do next. It can be a binary union of any of the
 *       vmk_HashKeyIteratorCmd defined above.
 *
 * \note VMK_HASH_KEY_ITER_CMD_DELETE requires that the caller of
 *       vmk_HashKeyIterate() has the hash table locked.
 *
 * \param[in] hdl       Hash handle.
 * \param[in] key       Key.
 * \param[in] value     Value.
 * \param[in] data      Private data passed while calling vmk_HashKeyIterate.
 *
 * \retval VMK_HASH_KEY_ITER_CMD_CONTINUE Move to the next key-value pair.
 * \retval VMK_HASH_KEY_ITER_CMD_STOP     Stop iterating.
 * \retval VMK_HASH_KEY_ITER_CMD_DELETE   Delete the current key-value pair.
 *
 *******************************************************************************
 */
typedef vmk_HashKeyIteratorCmd (*vmk_HashKeyIterator)(vmk_HashTable hdl,
                                                      vmk_HashKey key,
                                                      vmk_HashValue value,
                                                      vmk_AddrCookie data);


/*
 *******************************************************************************
 * vmk_HashKeyIterate --                                                  */ /**
 *
 * \brief Iterate through the key-value pairs from a given hash table.
 *
 * \param[in] hdl       Hash handle.
 * \param[in] iterator  Iterator callback.
 * \param[in] data      Private data passed to the iterator for each key-value
 *                      pair.
 *
 * \retval VMK_OK        Iterator went through the entire hash table or until
 *                       stop command was issued.
 * \retval VMK_BAD_PARAM If hdl equals to VMK_INVALID_HASH_HANDLE.
 *                       If iterator equals to NULL.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyIterate(vmk_HashTable hdl,
                   vmk_HashKeyIterator iterator,
                   vmk_AddrCookie data);


/*
 *******************************************************************************
 * vmk_HashKeyInsert --                                                   */ /**
 *
 * \brief Insert a key-value pair into a given hash table.
 *
 * \note The key passed will be copied locally only if the flag
 *       VMK_HASH_KEY_FLAGS_LOCAL_COPY was passed while creating the hash.
 *
 * \note The value passed won't be copied, but if the hash has an acquire
 *       function, a new reference to the value will be acquired.
 *
 * \param[in] hdl        Hash handle.
 * \param[in] key        Key.
 * \param[in] value      Value. A NULL value is valid.
 *
 * \retval VMK_OK        Key-value pair insertion successful.
 * \retval VMK_NO_MEMORY Allocation failure.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyInsert(vmk_HashTable hdl,
                  vmk_HashKey key,
                  vmk_HashValue value);

/*
 *******************************************************************************
 * vmk_HashKeyUpdate --                                                   */ /**
 *
 * \brief Update a key-value pair on a given hash table and return the previous
 *        associated value.
 *
 * \note The newValue passed won't be copied, but if the hash has an acquire
 *       function, a new reference to newValue will be acquired.
 *
 * \note If the hash has a release function, that function will be called
 *       asynchronously for the prior value.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[in]  newValue  Updated value. A NULL value is valid.
 * \param[out] oldValue  Value before update. A NULL value would mean that the
 *                       caller is not interested in getting the previous
 *                       associated value. If the caller is interested and
 *                       the hash provides an acquire function, a new reference
 *                       to the old value will be acquired.
 *
 * \retval VMK_OK        Key-value pair update successful.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 * \retval VMK_NO_MEMORY Allocation failure.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyUpdate(vmk_HashTable hdl,
                  vmk_HashKey key,
                  vmk_HashValue newValue,
                  vmk_HashValue *oldValue);


/*
 *******************************************************************************
 * vmk_HashKeyDelete --                                                   */ /**
 *
 * \brief Delete a key-value pair from a given hash table and return the current
 *        value.
 *
 * \note If the hash has a release function, that function will be called
 *       asynchronously for the removed value.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[out] value     Value before remove. A NULL value would mean that the
 *                       caller is not interested in getting the current
 *                       associated value. If the caller is interested and
 *                       the hash provides an acquire function, a new reference
 *                       to the old value will be acquired.
 *
 * \retval VMK_OK        Key-value pair removal successful.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyDelete(vmk_HashTable hdl,
                  vmk_HashKey key,
                  vmk_HashValue *value);


/*
 *******************************************************************************
 * vmk_HashKeyFind --                                                     */ /**
 *
 * \brief Find a key-value pair and return the current associated value.
 *
 * \param[in]  hdl       Hash handle.
 * \param[in]  key       Key.
 * \param[out] value     Value associated to the key. A NULL value would mean
 *                       that the caller is not interested in getting the current
 *                       associated value. If the caller is interested in
 *                       getting the current associated value, a new reference
 *                       will be acquired if the hash has an acquire function.
 *
 * \retval VMK_OK        Key-value pair found successfully.
 * \retval VMK_NOT_FOUND If there is no key-value pair matching the key
 *                       parameter.
 * \retval VMK_BAD_PARAM If hdl pointer equals to VMK_INVALID_HASH_HANDLE.
 *                       If key equals to NULL on a string or opaque keys hash.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_HashKeyFind(vmk_HashTable hdl,
                vmk_HashKey key,
                vmk_HashValue *value);


/*
 *******************************************************************************
 * vmk_HashBytes --                                                       */ /**
 *
 * \brief Calculate 64 bit hash for an array of bytes.
 *
 * \note This API is not used as part of a hash table allocated with the
 *       vmk_HashAlloc() function. It is provided as a helper routine for
 *       callers that need to calculate hashes for their own purpose.
 *
 * \param[in]  key       Byte array pointer.
 * \param[in]  keySize   Array Size in byte.
 * \param[out] hash      The calculated hash value.
 *
 * \retval VMK_OK        Hash calculated successfully.
 * \retval VMK_BAD_PARAM If key or hash is NULL, or keySize equal zero.
 *
 *******************************************************************************
 */

VMK_ReturnStatus
vmk_HashBytes(const vmk_uint8 *key,
              vmk_uint32 keySize,
              vmk_uint64 *hash);

#endif /* _VMKAPI_HASH_H_ */
/** @} */
/** @} */
