/* **********************************************************
 * Copyright 1998, 2007-2008, 2010, 2015 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * linux_proc.c --
 *
 *      Linux proc filesystem emulation.	
 */

#include <linux/proc_fs.h>
#include "linux_stubs.h"
#include <linux/slab.h>
#include <vmkplexer_procfs.h>

#define VMKLNX_LOG_HANDLE LinProc
#include <vmklinux_log.h>

/*
 * Define the Linux internal proc nodes that driver may need.
 * These nodes are /proc, /proc/driver, /proc/net, /proc/scsi,
 * and /proc/bus.
 *
 * These nodes are predefined in the vmkplexer layer.
 */
struct proc_dir_entry proc_root = {
   .name      = "proc",
   .module_id = VMK_INVALID_MODULE_ID,
   .heap_id   = VMK_INVALID_HEAP_ID,
};
EXPORT_SYMBOL(proc_root);

struct proc_dir_entry proc_root_driver_dummy = {
   .name      = "driver",
   .module_id = VMK_INVALID_MODULE_ID,
   .heap_id   = VMK_INVALID_HEAP_ID,
};

struct proc_dir_entry *proc_root_driver = &proc_root_driver_dummy;
EXPORT_SYMBOL(proc_root_driver );

struct proc_dir_entry proc_net_dummy = {
   .name      = "net",
   .module_id = VMK_INVALID_MODULE_ID,
   .heap_id   = VMK_INVALID_HEAP_ID,
};
struct proc_dir_entry *proc_net = &proc_net_dummy;
EXPORT_SYMBOL(proc_net );

struct proc_dir_entry proc_scsi_dummy = {
   .name      = "scsi",
   .module_id = VMK_INVALID_MODULE_ID,
   .heap_id   = VMK_INVALID_HEAP_ID,
};
struct proc_dir_entry *proc_scsi = &proc_scsi_dummy;

struct proc_dir_entry proc_bus_dummy = {
   .name      = "bus",
   .module_id = VMK_INVALID_MODULE_ID,
   .heap_id   = VMK_INVALID_HEAP_ID,
};
struct proc_dir_entry *proc_bus = &proc_bus_dummy;

/*
 * Pointers to the vmkplexer predefined proc nodes for
 * /proc, /proc/driver, /proc/net, /proc/scsi, and /proc/bus.
 */
static vmkplxr_ProcfsEntry *linuxProc;
static vmkplxr_ProcfsEntry *linuxDriver;
static vmkplxr_ProcfsEntry *linuxNet;
static vmkplxr_ProcfsEntry *linuxScsi;
static vmkplxr_ProcfsEntry *linuxBus;

static vmk_Semaphore       linuxScratchPageLock;

static int LinuxProcRead(vmkplxr_ProcfsEntry *entry, char *buffer, int *len);
static int LinuxProcWrite(vmkplxr_ProcfsEntry *entry, char *buffer, int *len);

/*
 *-----------------------------------------------------------------------------
 *
 *  LinuxProc_Init --
 *
 *      Initialize the Linux proc emulation.
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
LinuxProc_Init(void)
{
   VMK_ReturnStatus status;

   VMKLNX_CREATE_LOG();

   /*
    * Retrieve the predefined nodes in vmkplexer.
    */
   linuxProc   = vmkplxr_ProcfsFindEntry(NULL,      NULL);
   linuxDriver = vmkplxr_ProcfsFindEntry(linuxProc, "driver");
   linuxNet    = vmkplxr_ProcfsFindEntry(linuxProc, "net");
   linuxScsi   = vmkplxr_ProcfsFindEntry(linuxProc, "scsi");
   linuxBus    = vmkplxr_ProcfsFindEntry(linuxProc, "bus");

   VMK_ASSERT(linuxProc   != NULL);
   VMK_ASSERT(linuxDriver != NULL);
   VMK_ASSERT(linuxNet    != NULL);
   VMK_ASSERT(linuxScsi   != NULL);
   VMK_ASSERT(linuxBus    != NULL);

   /*
    * Save the vmkplexer predefined nodes in their
    * corresponding Linux nodes.
    */
   proc_root.vmkplxr_proc_entry         = linuxProc;
   proc_root_driver->vmkplxr_proc_entry = linuxDriver;
   proc_net->vmkplxr_proc_entry         = linuxNet;
   proc_scsi->vmkplxr_proc_entry        = linuxScsi;
   proc_bus->vmkplxr_proc_entry         = linuxBus;

   status = vmk_BinarySemaCreate(&linuxScratchPageLock,
                                 vmklinuxModID,
		                 "procfsScratchPgLock");
   VMK_ASSERT(status == VMK_OK);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  LinuxProc_Cleanup --
 *
 *      Clean up the Linux proc emulation.
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
LinuxProc_Cleanup(void)
{
   VMKLNX_DESTROY_LOG();

   vmk_SemaDestroy(&linuxScratchPageLock);
}

struct proc_dir_entry *
vmklnx_create_proc_entry(vmk_ModuleID module_id,
                         vmk_HeapID heap_id,
                         const char *path,
                         mode_t mode,
                         struct proc_dir_entry *parent)
{
   VMK_ReturnStatus status;
   struct proc_dir_entry *pde;
   vmkplxr_ProcfsEntry *entry;
   vmk_ByteCount base_name_len;
   vmk_ByteCount alloc_size;
   vmk_ByteCount path_len;
   vmk_Bool is_dir;
   const char *slashPtr, *base_name;
   char *entry_name;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(module_id != VMK_INVALID_MODULE_ID);
   VMK_ASSERT(heap_id   != VMK_INVALID_HEAP_ID  );

   path_len = vmk_Strnlen(path, VMKPLXR_PROCFS_MAX_NAME);
   if (path_len >= VMKPLXR_PROCFS_MAX_NAME) {
      /*
       * path_len should be at most (VMKPLXR_PROCFS_MAX_NAME - 1)
       */
      VMKLNX_WARN("Length of path (%p) is too long (%ld)", path, path_len);
      return NULL;
   }

   if (parent == NULL) {
      parent = &proc_root;
   }

   /*
    * Find the base name in the path
    */
   slashPtr = vmk_Strrchr(path, '/');
   if (slashPtr) {
      base_name = slashPtr + 1;
   } else {
      base_name = path;
   }

   base_name_len = vmk_Strnlen(base_name, VMKPLXR_PROCFS_MAX_NAME-1);

   /*
    * Allocate memory for proc_dir_entry plus the buffer for the name
    * Always add one to length of the name to accomodate the trailing
    * null byte.
    */
   alloc_size = sizeof(struct proc_dir_entry) + base_name_len + 1;
   pde = vmk_HeapAlloc(heap_id, alloc_size);
   if (pde == NULL) {
      return NULL;
   }
   vmk_Memset(pde, 0, alloc_size);
 
   /* memory for name starts at the end of pde */
   entry_name = (char *)(pde + 1);
   vmk_Strncpy(entry_name, base_name, base_name_len);
   entry_name[base_name_len] = '\0';

   pde->name      = entry_name;
   pde->namelen   = base_name_len;
   pde->mode      = mode;
   pde->module_id = module_id;
   pde->heap_id   = heap_id;

   is_dir = S_ISDIR(mode) ? VMK_TRUE : VMK_FALSE;

   entry = vmkplxr_ProcfsAllocEntry(module_id);
   if (!entry) {
      goto error_return;
   }

   pde->vmkplxr_proc_entry = entry;
   vmkplxr_ProcfsSetPrivateData(entry, pde);

   vmkplxr_ProcfsInitEntry(entry, 
                           base_name, 
                           is_dir, 
                           is_dir? NULL : LinuxProcRead, 
                           is_dir? NULL : LinuxProcWrite); 

   /*
    * Make the new node visible in the /proc name space
    */
   status = vmkplxr_ProcfsAttachEntry(parent->vmkplxr_proc_entry, path, entry);

   if (status == VMK_OK) {
      return pde;
   }

   vmkplxr_ProcfsFreeEntry(entry);

error_return:
   vmk_HeapFree(heap_id, pde);
   return NULL;
}
EXPORT_SYMBOL(vmklnx_create_proc_entry);


struct proc_dir_entry *
vmklnx_proc_mkdir(vmk_ModuleID module_id,
                  vmk_HeapID heap_id,
		  const char *path,
                  struct proc_dir_entry *parent)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return vmklnx_create_proc_entry(module_id, heap_id, path, S_IFDIR, parent);
}
EXPORT_SYMBOL(vmklnx_proc_mkdir);

void 
vmklnx_remove_proc_entry(vmk_ModuleID module_id,
                         const char *path, 
                         struct proc_dir_entry *from)
{
   vmkplxr_ProcfsEntry *entry;
   struct proc_dir_entry *pde;
   VMK_ReturnStatus status;
   vmk_ByteCount path_len;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   path_len = vmk_Strnlen(path, VMKPLXR_PROCFS_MAX_NAME);
   if (path_len >= VMKPLXR_PROCFS_MAX_NAME) {
      /*
       * path_len should be at most (VMKPLXR_PROCFS_MAX_NAME - 1)
       */
      VMKLNX_WARN("Length of path (%p) is too long (%ld)", path, path_len);
      return;
   }

   if (from == NULL) {
      from = &proc_root;
   }

   /*
    * Take off the detaching node from the /proc name space.
    */
   status = vmkplxr_ProcfsDetachEntry(module_id, from->vmkplxr_proc_entry, path, &entry);
   if (status != VMK_OK) {
      VMKLNX_WARN("Cannot remove proc node %s/%s (%s).",
                  from->name, path, vmk_StatusToString(status));
      return;
   }

   /*
    * Get the associated proc_dir_entry
    */
   pde = vmkplxr_ProcfsGetPrivateData(entry);
   VMK_ASSERT(pde != NULL && pde->vmkplxr_proc_entry == entry);

   /*
    * Free both the vmkplexer node and the vmklinux node
    */
   vmkplxr_ProcfsFreeEntry(entry);
   vmk_HeapFree(pde->heap_id, pde);
}
EXPORT_SYMBOL(vmklnx_remove_proc_entry);

/**                                          
 *  vmklnx_proc_entry_exists - check to see if a proc entry is already in procfs
 *  @path: Proc entry path to check
 *  @parent: Parent proc node to search under
 *
 *  RETURN VALUE:
 *  Non-zero if the node exists; 0 otherwise.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_proc_entry_exists */
int
vmklnx_proc_entry_exists(const char *path,
		         struct proc_dir_entry *parent)
{
   vmkplxr_ProcfsEntry *vmkplxr_parent;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (parent == NULL) {
      vmkplxr_parent = linuxProc;
   } else {
      vmkplxr_parent = parent->vmkplxr_proc_entry;
   }

   return vmkplxr_ProcfsFindEntry(vmkplxr_parent, path) != NULL ;
}
EXPORT_SYMBOL(vmklnx_proc_entry_exists);


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxProcAllocScratchPage --
 *
 *      Allocate a scratch page for proc_read.  Currently, the size of the free 
 *      page pool this routine is managing has only one page. This should be
 *      sufficient for proc_read purposes.
 *
 * Results:
 *      Pointer to the internal scratch page buffer.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static void *
LinuxProcAllocScratchPage()
{
   static char scratch_page[VMK_PAGE_SIZE];

   vmk_SemaLock(&linuxScratchPageLock);

   return scratch_page;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxProcFreeScratchPage --
 *
 *      Freed the previous allocated scratch page.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static void
LinuxProcFreeScratchPage(void *page)
{
   vmk_SemaUnlock(&linuxScratchPageLock);
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxProcRead --
 *
 *      Generic proc read handler. Calls the appropriate (driver) proc read 
 *      handler. 
 *      Read the full contents of the node into "buffer." 
 *      XXX The buffer is assumed to be large enough. 
 *
 * Results:
 *      Result of the lower level proc handler.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static int 
LinuxProcRead(vmkplxr_ProcfsEntry *entry,
              char *buffer,
              int *len)
{
   struct proc_dir_entry* pde = (struct proc_dir_entry *) vmkplxr_ProcfsGetPrivateData(entry);
   char *start;
   char *scratch_page;
   int32_t count, eof, nread;
   uint32_t off;

#  define PROC_BLOCK_SIZE (VMK_PAGE_SIZE - 1024)
  
   VMKLNX_DEBUG(2, "Start"); 

   VMK_ASSERT(pde != NULL && pde->vmkplxr_proc_entry == entry);

   if (!pde->read_proc) {
      VMKLNX_WARN("Not reading (NULL read handler for node %s)",
		   pde->name);
      return VMK_FAILURE;
   }

   /*
    * We want to emulate the Linux proc_read function as closely as possible;
    * which is to hand the proc handler a page to scribble every time and let us
    * know finally where the current chunk starts and how long it is. We then 
    * copy the correct chunks into the main proc buffer. We can't use the
    * proc buffer directly due to the possible overwrite on every read.
    */

   scratch_page = LinuxProcAllocScratchPage();
#if defined(VMX86_DEBUG)
   const char guard_byte = 0xFA;
   memset(scratch_page + PROC_BLOCK_SIZE, guard_byte, 
          VMK_PAGE_SIZE - PROC_BLOCK_SIZE);
#endif
   eof = 0;
   off = 0;
   // This is the size Linux uses on proc reads. 
   count = PROC_BLOCK_SIZE; 
   nread = 1;
   while ( !eof 
           && nread != 0
           && (off + count) < VMK_PROC_READ_MAX) {
      start = NULL;

      VMKAPI_MODULE_CALL(pde->module_id, 
                         nread, 
                         pde->read_proc,
                         scratch_page, 
                         &start, 
                         off, 
                         count, 
                         &eof, 
                         pde->data);
      VMK_ASSERT(nread <= PROC_BLOCK_SIZE);
#if defined(VMX86_DEBUG)
      uint32_t bytes_overwritten = 0;
      char *guard_itr = scratch_page + PROC_BLOCK_SIZE;
      /*
       * Check if guard bytes were overwritten, panic if there was an overwrite
       */
      while(guard_itr < scratch_page + PAGE_SIZE) {
         if(*guard_itr != guard_byte) {
            bytes_overwritten++;
         }   
         guard_itr++;
      }
      if (bytes_overwritten > 0) {
         VMKLNX_PANIC("The driver %s overwrote its buffer during read_proc" 
                       " by %d byte(s)", pde->name, bytes_overwritten);
      }
#endif
      if (!start) {
         if (nread <= off) {
            break;
         }
         start = scratch_page + off;
         nread -= off;
      }
      else if (start >= scratch_page) {

        /*
         * If readproc() wrote past its buffer log a warning and assert
         */
         if(start + nread > scratch_page + PROC_BLOCK_SIZE)
         {
            VMKLNX_WARN("The driver %s has written past its buffer", pde->name);
            VMK_ASSERT(false);
         }
      }
      memcpy(buffer + off, start, nread);
      off += nread;
   }

   VMKLNX_DEBUG(5, "read %d bytes", off);
   *len = off;

   LinuxProcFreeScratchPage(scratch_page);
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxProcWrite --
 *
 *      Generic proc write handler. Calls the appropriate (driver) proc write 
 *      handler. 
 *
 * Results:
 *      Result of the lower level proc handler.
 *
 * Side effects:
 *      None. 
 *
 *-----------------------------------------------------------------------------
 */

static int 
LinuxProcWrite(vmkplxr_ProcfsEntry *entry,
               char *buffer,        // IN: buffer containing data
               int *len)            // IN: pointer to length of buffer
{
   struct proc_dir_entry *pde = (struct proc_dir_entry *) vmkplxr_ProcfsGetPrivateData(entry);
   int32_t ret;

   VMKLNX_DEBUG(2, "Start"); 

   VMK_ASSERT(pde != NULL && pde->vmkplxr_proc_entry == entry);

   if (!pde->write_proc) {
      VMKLNX_WARN("Not writing (NULL write handler for node %s)",
		   pde->name);

      return VMK_FAILURE;
   }
   /*
    * write_proc takes struct file as the first arg.
    * Proc write handlers don't really use it.
    */
   VMKAPI_MODULE_CALL(pde->module_id, 
                      ret, 
                      pde->write_proc, 
                      NULL, 
                      buffer, 
                      *len, 
                      pde->data);

   return VMK_OK;
}
