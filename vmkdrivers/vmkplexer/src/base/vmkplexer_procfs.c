/* 
 **********************************************************
 * Copyright 2010, 2015 VMware, Inc.  All rights reserved.
 **********************************************************
 */

/*
 * vmkplexer_procfs.c --
 *
 *      Management of the procfs nodes.
 */

#include "vmkapi.h"
#include <vmkplexer_module.h>
#include <vmkplexer_init.h>
#include <vmkplexer_procfs.h>

#define VMKPLXR_LOG_HANDLE VmkplxrProc
#include <vmkplexer_log.h>

#define DBG_LOG_CALL    1
#define DBG_LOG_LEVEL2	2

/*
 * Definition of a procfs node within vmkplexer
 */
struct VmkplxrProcfsEntry {
   char                      name[VMKPLXR_PROCFS_MAX_NAME]; /* node name    */
   vmk_ModuleID              moduleID;         /* owner module ID           */
   vmk_HeapID                heapID;           /* heap ID of owner          */
   struct vmk_ListLinks      peerList;         /* link to adjacent peers    */
   struct vmk_ListLinks      childList;        /* link to child list        */
   struct VmkplxrProcfsEntry *parent;          /* link to parent            */
   vmk_ProcEntry             vmkEntry;         /* associated vmkapi handle  */
   vmk_Bool                  isDir;            /* true if a directory entry */
   vmkplxr_ProcfsReadFunc    read;             /* read op of the entry      */
   vmkplxr_ProcfsWriteFunc   write;            /* write op of the entry     */
   void                      *privateData;     /* link private data         */
};

static vmkplxr_ProcfsEntry procRoot;           /* internal /proc node        */
static vmkplxr_ProcfsEntry procDriver;         /* internal /proc/driver node */
static vmkplxr_ProcfsEntry procNet;            /* internal /proc/net node    */
static vmkplxr_ProcfsEntry procScsi;           /* internal /proc/scsi node   */
static vmkplxr_ProcfsEntry procBus;            /* internal /proc/bus node    */

static vmk_Semaphore VmkplxrProcfsEntryLock;    /* global lock guarding the   */
                                               /*   access of the internal   */
                                               /*   hierarchial structure of */
                                               /*   proc nodes               */

/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsInitEntry --
 *
 *      Initialize an internal node using the values from the parameters
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

void
vmkplxr_ProcfsInitEntry(vmkplxr_ProcfsEntry *entry, 
                        const char *name, 
                        vmk_Bool isDir,
                        vmkplxr_ProcfsReadFunc read,
                        vmkplxr_ProcfsWriteFunc write)
{
   VMKPLXR_DEBUG(DBG_LOG_CALL, "entry=%p, name=%s, isDir=%s, read=%p, write=%p",
                 entry, name, isDir ? "true" : "false", read, write);

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   vmk_Memset(entry->name, 0, sizeof(entry->name));
   vmk_Strncpy(entry->name, name, sizeof(entry->name)-1);
   vmk_ListInit(&entry->peerList);
   vmk_ListInit(&entry->childList);
   entry->isDir    = isDir;
   entry->read     = read;
   entry->write    = write;
   entry->parent   = NULL;
   entry->vmkEntry = NULL;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsInitEntry);


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsAddEntryToParent --
 *
 *      Add a node to the parent's child list.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static inline void
VmkplxrProcfsAddEntryToParent(vmkplxr_ProcfsEntry *parent, vmkplxr_ProcfsEntry *entry)
{
   VMKPLXR_DEBUG(DBG_LOG_CALL, "parent=%p (%s) entry=%p (%s)",
                 parent, parent->name, entry, entry->name);

   vmk_ListInsert(&entry->peerList, &parent->childList);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsRemoveEntryFromParent --
 *
 *      Remove a node from the parent's child list.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static inline void
VmkplxrProcfsRemoveEntryFromParent(vmkplxr_ProcfsEntry *entry)
{
   VMKPLXR_DEBUG(DBG_LOG_CALL, "entry=%p (%s)", entry, entry->name);

   vmk_ListRemove(&entry->peerList);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsCopyPath --
 *
 *      Copy a proc node path to a buffer. The copied path is normalized
 *      with mutiple consecutive / reduce to one.
 *
 * Results:
 *      Length of the copied path.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static unsigned int
VmkplxrProcfsCopyPath(char *destPath, const char *srcPath, unsigned int destBufLength)
{
   char *dp, *tp, c;
   const char *sp;
   unsigned int count;

   dp = destPath;
   tp = destPath + destBufLength - 1;
   sp = srcPath;

   while ((dp < tp) && *sp) {
      c = *dp++ = *sp++;
 
      /*
       * If current char and the next char in source
       * are both a '/', then skip over the repeated
       * '/'
       */
      while (c == '/' && *sp == '/') {
         c = *sp++;
      }
   }

   *dp = '\0';

   count = dp - destPath;

   VMKPLXR_DEBUG(DBG_LOG_CALL, "srcPath=%s destPath=%s, destBufLength=%d, count=%d",
                 srcPath, destPath, destBufLength, count);

   return count;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsLock --
 *
 *      Acquire the global proc lock. This lock must be taken
 *      before the internal proc node tree is being accessed.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static inline void
VmkplxrProcfsLock()
{
   VMKPLXR_DEBUG(DBG_LOG_CALL, "locked");

   vmk_SemaLock(&VmkplxrProcfsEntryLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsUnlock --
 *
 *      Release the global proc node lock.  This should be done after
 *      completing the access to the internal node tree.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static inline void
VmkplxrProcfsUnlock()
{
   VMKPLXR_DEBUG(DBG_LOG_CALL, "unlocked");

   vmk_SemaUnlock(&VmkplxrProcfsEntryLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsRead --
 *
 *      This callback is registered to the vmkapi proc node to
 *      handle read request.  This callback basically does a
 *      couple sanity checks and sets up the parameters to call
 *      the lower level read handler.
 *
 * Results:
 *      0 for success; non-zero for failure.
 *
 * Side effects:
 *      Return number of character read in len.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmkplxrProcfsRead(vmk_ProcEntry vmkEntry, char *buffer, int *len)
{
   int status;
   vmkplxr_ProcfsEntry *entry = vmk_ProcEntryGetPrivate(vmkEntry);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "vmkEntry=%p, buffer=%p, len=%d",
                 vmkEntry, buffer, *len);

   if (entry == NULL) {
      VMKPLXR_WIRPID("Not reading (NULL entry for node (uuid=0%x))", 
                     vmk_ProcEntryGetGUID(vmkEntry));
      status = -1;
      goto done;
   }

   if (entry->read == NULL) {
      VMKPLXR_WIRPID("Not reading (NULL read handler for node %s)",
                     entry->name);
      status = -1;
      goto done;
   }

   VMKAPI_MODULE_CALL(entry->moduleID, status, entry->read, entry, buffer, len);

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(0%x)", status);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsWrite --
 *
 *      This callback is registered to the vmkapi proc node to
 *      handle write request.  This callback basically does a
 *      couple sanity checks and sets up the parameters to call
 *      the lower level write handler.
 *
 * Results:
 *      0 for success; non-zero for failure.
 *
 * Side effects:
 *      Return the number of character written in len.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmkplxrProcfsWrite(vmk_ProcEntry vmkEntry, char *buffer, int *len)
{
   int status;
   vmkplxr_ProcfsEntry *entry = vmk_ProcEntryGetPrivate(vmkEntry);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "vmkEntry=%p, buffer=%p, len=%d",
                 vmkEntry, buffer, *len);

   if (entry == NULL) {
      VMKPLXR_WIRPID("Not writing (NULL entry for node (uuid=0%x))", 
                     vmk_ProcEntryGetGUID(vmkEntry));
      status = -1;
      goto done;
   }

   if (entry->write == NULL) {
      VMKPLXR_WIRPID("Not writing (NULL write handler for node %s (uuid=0x%x))", 
                     entry->name, vmk_ProcEntryGetGUID(vmkEntry));
      status = -1;
      goto done;
   }

   VMKAPI_MODULE_CALL(entry->moduleID, status, entry->write, entry, buffer, len);

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(0%x)", status);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsFindChildEntry --
 *
 *      Under 'parent' node, find the child node whose name is 'name'.
 *
 * Results:
 *      Return the child node found; otherwise NULL.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static vmkplxr_ProcfsEntry *
VmkplxrProcfsFindChildEntry(vmkplxr_ProcfsEntry *parent, char *name)
{
   vmk_ListLinks *vep;
   vmkplxr_ProcfsEntry *child = NULL, *childFound = NULL;

   VMKPLXR_DEBUG(DBG_LOG_CALL, "parent=%p (%s), name=%s",
                 parent, parent->name, name);

   VMK_LIST_FORALL(&parent->childList, vep) {
      child = VMK_LIST_ENTRY(vep, vmkplxr_ProcfsEntry, peerList);

      VMKPLXR_DEBUG(DBG_LOG_LEVEL2, "cmp child=%p (%s) to name=%s",
                    child, child->name, name);

      if (!vmk_Strncmp(child->name, name, sizeof(child->name))) {
         childFound = child;
         break;
      }
   }

   VMKPLXR_DEBUG(DBG_LOG_CALL, "return((%p, %s))", 
                 childFound, childFound ? childFound->name : "noname");

   return childFound;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsFindEntry --
 *
 *      Find the node as identified by 'path' that is relative to node
 *      'rootDirEntry' 
 *
 * Results:
 *      Return the node found; otherwise NULL.
 *
 * Side effects:
 *      On returning, 'path' does not retain its original value.
 *
 *-----------------------------------------------------------------------------
 */

static vmkplxr_ProcfsEntry *
VmkplxrProcfsFindEntry(vmkplxr_ProcfsEntry *rootDirEntry, char *path)
{
   char *slashPtr, *pathEnd, *name;
   vmkplxr_ProcfsEntry *entry = NULL;

   VMKPLXR_DEBUG(DBG_LOG_CALL, "rootDirEntry=%p (%s), path=%p (%s)",
                 rootDirEntry, rootDirEntry->name, path, path);

   name = path;
   pathEnd = path + vmk_Strnlen(path, VMKPLXR_PROCFS_MAX_NAME);

   while (name < pathEnd) {

      if (!rootDirEntry->isDir) {
         goto done;
      }

      slashPtr = vmk_Strchr(name, '/');
      if (slashPtr == NULL) {
         slashPtr = pathEnd;
      }
      if (slashPtr < pathEnd) {
         *slashPtr = '\0';
      }

      entry = VmkplxrProcfsFindChildEntry(rootDirEntry, name);
      if (!entry) {
         goto done;
      }

      rootDirEntry = entry;
      name = slashPtr + 1;
   }

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return((%p, %s))",
                 entry, entry ? entry->name : "noname");

   return entry;  
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfsFindParentEntry --
 *
 *      Find the parent of the node specified by 'path' which is relative
 *      to the node 'rootDirEntry'.
 *
 * Results:
 *      Return the node found; otherwise NULL.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static vmkplxr_ProcfsEntry *
VmkplxrProcfsFindParentEntry(vmkplxr_ProcfsEntry *rootDirEntry, const char *entryPath)
{
   char path[VMKPLXR_PROCFS_MAX_NAME];
   char *slashPtr;
   vmkplxr_ProcfsEntry *parentFound = NULL;

   VMKPLXR_DEBUG(DBG_LOG_CALL, "rootDirEntry=%p (%s) entryPath=%p (%s)",
                 rootDirEntry, rootDirEntry->name, entryPath, entryPath);
   /*
    * If starting directory level is not provided,
    * then assume we are searching from root level.
    */
   if (rootDirEntry == NULL) {
      rootDirEntry = &procRoot;
   }

   VMK_ASSERT( entryPath != NULL);
   VMK_ASSERT(*entryPath != '\0');

   VmkplxrProcfsCopyPath(path, entryPath, sizeof(path));

   slashPtr = vmk_Strrchr(path, '/');

   if (!slashPtr) {
      parentFound = rootDirEntry;
      goto done;
   }

   *slashPtr = '\0';

   parentFound = VmkplxrProcfsFindEntry(rootDirEntry, path);

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return((%p, %s))", 
                 parentFound, parentFound->name);

   return parentFound;
}

/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrProcfs_Init/Cleanup --
 *
 *      Init and shutdown the procfs emulation. The predefined nodes
 *	/proc, /proc/driver, /proc/net, /proc/scsi and /proc/bus are
 *	being initialized here. These nodes are linked together to
 *	form the initial in-memory proc-node tree. 
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
vmkplxr_ProcfsInit(void)
{
   VMK_ReturnStatus status;
   vmk_Bool isDir = VMK_TRUE;
   int i;
   struct {
      vmkplxr_ProcfsEntry *procEntry;
      char *name;
      vmk_ProcEntryParent vmkParent;
   } entryTable[] = { 
      {&procRoot,   "proc",   VMK_PROC_ROOT       },
      {&procDriver, "driver", VMK_PROC_ROOT_DRIVER},
      {&procNet,    "net",    VMK_PROC_ROOT_NET   },
      {&procScsi,   "scsi",   VMK_PROC_ROOT_SCSI  },
      {&procBus,    "bus",    VMK_PROC_ROOT_BUS   }
   };

   VMKPLXR_CREATE_LOG();

   status = vmk_BinarySemaCreate(&VmkplxrProcfsEntryLock,
         vmkplxr_module_id, "VmkplxrProcfsLock");
   if (status != VMK_OK) {
      VMKPLXR_WARN("Not creating binary semaphore (%s)", vmk_StatusToString(status));
      return status;
   }

   for (i = 0; i < sizeof(entryTable)/sizeof(entryTable[0]); i++) {

      /* Initialize the vmkplexer entry */
      vmkplxr_ProcfsInitEntry(entryTable[i].procEntry,
                              entryTable[i].name,
                              isDir,
                              NULL,
                              NULL);

      /* Create and initialize the vmkapi entry */
      status = vmk_ProcEntryCreate(&entryTable[i].procEntry->vmkEntry,
                                   vmkplxr_module_id,
                                   entryTable[i].name,
                                   VMK_TRUE);
      if (status != VMK_OK) {
         VMKPLXR_WARN("Not creating proc entry %s (%s)", 
	              entryTable[i].name,
	              vmk_StatusToString(status));
         return status;
      }

      /* Associate the vmkplexer entry with the vmkapi predefined parent */
      vmk_ProcSetupPredefinedNode(entryTable[i].vmkParent, 
                                  entryTable[i].procEntry->vmkEntry, 
                                  (i == 0) ? VMK_TRUE : VMK_FALSE);

      /*
       * All the non-zero index entries in the table are child to the node at index 0.
       */
      if (i > 0) {
         VmkplxrProcfsAddEntryToParent(&procRoot, entryTable[i].procEntry);
      }
   }

   return VMK_OK;
}

void
vmkplxr_ProcfsCleanup(void)
{
   int i;
   struct {
      vmkplxr_ProcfsEntry *procEntry;
      char *name;
      vmk_ProcEntryParent vmkParent;
   } entryTable[] = { 
      {&procRoot,   "proc",   VMK_PROC_ROOT       },
      {&procDriver, "driver", VMK_PROC_ROOT_DRIVER},
      {&procNet,    "net",    VMK_PROC_ROOT_NET   },
      {&procScsi,   "scsi",   VMK_PROC_ROOT_SCSI  },
      {&procBus,    "bus",    VMK_PROC_ROOT_BUS   }
   };

   vmk_SemaDestroy(&VmkplxrProcfsEntryLock);

   for (i = 0; i < sizeof(entryTable)/sizeof(entryTable[0]); i++) {
      vmk_ProcRemovePredefinedNode(entryTable[i].vmkParent,
                                   entryTable[i].procEntry->vmkEntry,
                                   (i == 0) ? VMK_TRUE : VMK_FALSE);

      vmk_ProcEntryDestroy(entryTable[i].procEntry->vmkEntry);
   }

   VMKPLXR_DESTROY_LOG();
}


/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsAllocEntry --
 *
 *      Create a new node for the module identified by 'moduleID'
 *
 * Results:
 *      Upon success return the new node; otherwise NULL.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */
vmkplxr_ProcfsEntry *
vmkplxr_ProcfsAllocEntry(vmk_ModuleID moduleID)
{
   unsigned int allocSize;
   vmk_HeapID heapID;
   vmkplxr_ProcfsEntry *entry = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   allocSize = sizeof(vmkplxr_ProcfsEntry);
   heapID = vmk_ModuleGetHeapID(moduleID);
   if (heapID == VMK_INVALID_HEAP_ID) {
      VMKPLXR_WIRPID("Current module has no valid heap ID");
      goto done;
   }
   entry = vmk_HeapAlloc(heapID, allocSize);

   if (!entry) {
      goto done;
   }

   vmk_Memset(entry, '\0', allocSize);
   entry->moduleID = moduleID;
   entry->heapID   = heapID;

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(%p, noname)", entry);

   return entry;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsAllocEntry);


/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsFreeEntry --
 *
 *      Release the memory used by the node 'entry'.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */
void
vmkplxr_ProcfsFreeEntry(vmkplxr_ProcfsEntry *entry)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   if (!entry) {
      VMKPLXR_WARN("entry == NULL");
      return;
   }

   vmk_HeapFree(entry->heapID, entry);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "freed (%p, %s)", entry, entry->name);
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsFreeEntry);


/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsGetPrivateData --
 *
 *      Return the private data area that is associated with the node
 *      'entry'.
 *
 * Results:
 *      Pointer to the private data area.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */
void *
vmkplxr_ProcfsGetPrivateData(vmkplxr_ProcfsEntry *entry)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return entry->privateData;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsGetPrivateData);


/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsSetPrivateData --
 *
 *      Assocate a private data area with the proc node 'entry'.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

void
vmkplxr_ProcfsSetPrivateData(vmkplxr_ProcfsEntry *entry, void *privateData)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   entry->privateData = privateData;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsSetPrivateData);


/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ProcfsFindEntry --
 *
 *      Find the node as identified by 'entryPath' that is relative to node
 *      'rootDirEntry' 
 *
 * Results:
 *      On success, return the node found; otherwise NULL.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */
vmkplxr_ProcfsEntry *
vmkplxr_ProcfsFindEntry(vmkplxr_ProcfsEntry *rootDirEntry, const char *entryPath)
{
   char path[VMKPLXR_PROCFS_MAX_NAME];
   vmkplxr_ProcfsEntry *entryFound = NULL;
   vmk_ByteCount entryPathLength;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "rootDirEntry=%p (%s), entryPath=%p (%s)",
                 rootDirEntry, rootDirEntry->name, entryPath, entryPath);

   /*
    * If starting directory level is not provided,
    * then assume we are searching from root level.
    */
   if (rootDirEntry == NULL) {
      rootDirEntry = &procRoot;
   }

   /*
    * If the relative path is NULL or empty,
    * then return rootDirEntry.
    */
   if (entryPath == NULL || *entryPath == '\0') {
      entryFound = rootDirEntry;
      goto done;
   }

   entryPathLength = vmk_Strnlen(entryPath, VMKPLXR_PROCFS_MAX_NAME);
   if (entryPathLength >= VMKPLXR_PROCFS_MAX_NAME) {
      /*
       * entryPathLength should be at most (VMKPLXR_PROCFS_MAX_NAME - 1)
       */
      VMKPLXR_WARN("Length of entryPath (%p) is too long (%ld)", entryPath, entryPathLength);
      goto done;
   }

   VMK_ASSERT(*entryPath != '/' );

   /*
    * Make a copy of the path so that we can
    * make changes to it.
    */
   VmkplxrProcfsCopyPath(path, entryPath, sizeof(path));

   entryFound = VmkplxrProcfsFindEntry(rootDirEntry, path);

done:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return((%p, %s))",
                 entryFound, entryFound->name);

   return entryFound;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsFindEntry);

/*
 *-----------------------------------------------------------------------------
 *
 * Vmkplxr_ProcfsDetachEntry --
 *
 *      Remove the proc node as specified by 'path' which is relative to
 *      the node 'rootDirEntry'.
 *
 * Results:
 *      On success, return the node that was detached; on failure,
 *      return NULL.
 *
 * Side effects:
 *      May spin if the entry is being used (read/write in progress). 
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
vmkplxr_ProcfsDetachEntry(vmk_ModuleID ownerID, 
                          vmkplxr_ProcfsEntry *rootDirEntry, 
                          const char *path,
                          vmkplxr_ProcfsEntry **entryDetached)
{
   vmkplxr_ProcfsEntry *entry;
   VMK_ReturnStatus status = VMK_FAILURE;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "rootDirEntry=%p (%s), path=%p (%s)",
                 rootDirEntry, rootDirEntry->name, path, path);

   VmkplxrProcfsLock();

   entry = vmkplxr_ProcfsFindEntry(rootDirEntry, path);
   if (!entry) {
      VMKPLXR_WARN("Not detaching node <%s, %s> (not found).",
                   rootDirEntry->name, path);
      status = VMK_NOT_FOUND;
      goto error_return;
   }

   if (entry->moduleID != ownerID) {
      VMKPLXR_WARN("Not detaching node <%s, %s> (not owner).",
                   rootDirEntry->name, path);
      status = VMK_NO_PERMISSION;
      goto error_return;
   }

   /*
    * Don't want to detach a node with children.
    */
   if (entry->isDir && !vmk_ListIsEmpty(&entry->childList)) {
      VMKPLXR_WARN("Not detaching node <%s, %s> (has children).",
                   rootDirEntry->name, path);
      status = VMK_BUSY;
      goto error_return;
   }

   // If a read/write on this node is in progress, this call will spin. 
   vmk_ProcUnRegister(entry->vmkEntry);

   VmkplxrProcfsRemoveEntryFromParent(entry);

   vmk_ProcEntryDestroy(entry->vmkEntry);   

   VmkplxrProcfsUnlock();

   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(%p, %s))",
                 entry, entry->name);

   *entryDetached = entry;
   return VMK_OK;

error_return:
   VmkplxrProcfsUnlock();
   return status;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsDetachEntry);

/*
 *-----------------------------------------------------------------------------
 *
 * Vmkplxr_ProcfsAttachEntry --
 *
 *      Hook the proc node 'newEntry' to the location as identified by
 *      'path' which is relative to the node 'rootDirEntry'.
 *
 * Results:
 *      Return VMK_OK is success.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
vmkplxr_ProcfsAttachEntry(vmkplxr_ProcfsEntry *rootDirEntry,
                          const char          *path,
                          vmkplxr_ProcfsEntry *newEntry)
{
   vmkplxr_ProcfsEntry *entry, *parentEntry;
   VMK_ReturnStatus status;
   vmk_Bool canBlock = VMK_FALSE;
   vmk_ByteCount pathLength;
 
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   VMKPLXR_DEBUG(DBG_LOG_CALL, "rootDirEntry=%p (%s), path=%p (%s) newEntry=%p (%s)",
                 rootDirEntry, rootDirEntry->name, path, path, newEntry, newEntry->name);

   pathLength = vmk_Strnlen(path, VMKPLXR_PROCFS_MAX_NAME);
   if (pathLength >= VMKPLXR_PROCFS_MAX_NAME) {
      /*
       * pathLength should be at most (VMKPLXR_PROCFS_MAX_NAME - 1)
       */
      VMKPLXR_WARN("Length of path (%p) is too long (%ld)", path, pathLength);
      status = VMK_BAD_PARAM;
      goto bad_param_error_return;
   }

   VmkplxrProcfsLock();

   entry = vmkplxr_ProcfsFindEntry(rootDirEntry, path);
   if (entry) {
      VMKPLXR_WARN("Not attaching new node <%s, %s> (already exists)",
                   rootDirEntry->name, path);
      status = VMK_EXISTS;
      goto error_return;
   }

   parentEntry = VmkplxrProcfsFindParentEntry(rootDirEntry, path);
   if (!parentEntry) {
      VMKPLXR_WARN("Not attaching new node <%s, %s> (parent not found)",
                   rootDirEntry->name, path);
      status = VMK_FAILURE;
      goto error_return;
   }

   VmkplxrProcfsAddEntryToParent(parentEntry, newEntry);

   status = vmk_ProcEntryCreate(&newEntry->vmkEntry, 
                                newEntry->moduleID, 
                                newEntry->name, 
                                newEntry->isDir);
   if (status != VMK_OK) {
      VMKPLXR_WARN("Not attaching new node <%s, %s> "
                   "(vmk_ProcEntryCreate() failed with %s)",
                   rootDirEntry->name, path, vmk_StatusToString(status));
      goto error_return;
   }

   if (!newEntry->isDir) {
      canBlock = VMK_TRUE;
   }

   /* now make vmkapi call to setup the new proc entry */
   vmk_ProcEntrySetup(newEntry->vmkEntry,
                      parentEntry->vmkEntry,
                      newEntry->read  ? VmkplxrProcfsRead  : NULL,
                      newEntry->write ? VmkplxrProcfsWrite : NULL,
                      canBlock,
                      (void *)newEntry);

   vmk_ProcRegister(newEntry->vmkEntry);

   VmkplxrProcfsUnlock();

   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(VMK_OK)");

   return VMK_OK; 

error_return:
   VmkplxrProcfsUnlock();

bad_param_error_return:
   VMKPLXR_DEBUG(DBG_LOG_CALL, "return(0%x,%s))", status, vmk_StatusToString(status));
   return status;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ProcfsAttachEntry);
