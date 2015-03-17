
/* **********************************************************
 * Copyright 1998, 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmklinux_dist.h --
 *
 *      Prototypes for functions used in device drivers compiled for vmkernel.
 */

#ifndef _VMKLINUX_DIST_H_
#define _VMKLINUX_DIST_H_

#if !defined(LINUX_MODULE_SKB_MPOOL) && defined(NET_DRIVER)
#define LINUX_MODULE_SKB_MPOOL
#endif

#include "linux/spinlock.h"    /* for spinlock_t  */
#include "linux/irqreturn.h"   /* for irqreturn_t */
#include "vmklinux_version.h"
#include "vmkapi.h"

#define VMKLNX_NOT_IMPLEMENTED()                                     \
      vmk_Panic("NOT_IMPLEMENTED %s:%d\n", __FILE__, __LINE__);

#define VMKLNX_ASSERT_NOT_IMPLEMENTED(condition)                     \
   do {                                                              \
      if (VMK_UNLIKELY(!(condition))) {                              \
         vmk_Panic("NOT_IMPLEMENTED %s:%d -- VMK_ASSERT(%s)\n",   \
                   __FILE__, __LINE__, #condition);                  \
      }                                                              \
   } while(0)

#define VMKLNX_ASSERT_BUG(bug, condition)                            \
   do {                                                              \
      if (VMK_UNLIKELY(!(condition))) {                              \
         vmk_Panic("bugNr%d at %s:%d -- VMK_ASSERT(%s)\n",           \
         bug, __FILE__, __LINE__, #condition);                       \
      }                                                              \
   } while(0)

#define JIFFIES_TO_USEC(j)	((vmk_uint64)(j)*10000)
#define USEC_TO_JIFFIES(u)	((signed long)((u)/10000))

struct task_struct;
struct scsi_cmnd;
struct device_driver;
struct work_struct;
struct sk_buff;
struct net_device;
struct module;
struct seq_file;
struct seq_operations;
struct file;

/*
 * Linux stubs functions.
 */

extern VMK_ReturnStatus vmklnx_errno_to_vmk_return_status(int error);
extern unsigned int vmklnx_get_dump_poll_retries(void);
extern unsigned int vmklnx_get_dump_poll_delay(void);

typedef enum {
   VMKLNX_IRQHANDLER_TYPE1 = 1,  /* linux 2.6.18 irq handler */
   VMKLNX_IRQHANDLER_TYPE2 = 2   /* linux 2.6.19 irq handler */
} vmklnx_irq_handler_type_id;

typedef union {
    irqreturn_t (*handler_type1)(int, void *, struct pt_regs *);
    irqreturn_t (*handler_type2)(int, void *);
} vmklnx_irq_handler_t;

extern unsigned int vmklnx_convert_isa_irq(unsigned int irq);

/*
 * Memory allocator functions
 */

#define __STR__(t)		# t
#define EXPAND_TO_STRING(n)	__STR__(n)
#define VMK_MODULE_HEAP_NAME	EXPAND_TO_STRING(LINUX_MODULE_HEAP_NAME)

#define __MAKE_MODULE_HEAP_ID(moduleID) moduleID ## _HeapID
#define __HEAP_ID(m) __MAKE_MODULE_HEAP_ID(m)
#define VMK_MODULE_HEAP_ID __HEAP_ID(LINUX_MODULE_HEAP_NAME)
extern vmk_HeapID VMK_MODULE_HEAP_ID;

#define __MAKE_MODULE_CODMA_HEAP_NAME(moduleID) __STR__(moduleID ## _codma)
#define __CODMA_HEAP_NAME(m) __MAKE_MODULE_CODMA_HEAP_NAME(m)
#define VMK_MODULE_CODMA_HEAP_NAME __CODMA_HEAP_NAME(LINUX_MODULE_AUX_HEAP_NAME)

struct kmem_cache_s;
struct device;
struct dma_pool;
struct pci_dev;
struct vmklnx_mem_info;

extern void *vmklnx_kmalloc(vmk_HeapID heapID, size_t size, gfp_t flags, void *ra);
extern void *vmklnx_kzmalloc(vmk_HeapID heapID, size_t size, gfp_t flags);
extern void  vmklnx_kfree(vmk_HeapID heapID, const void *p);
extern void *vmklnx_kmalloc_align(vmk_HeapID heapID, size_t size, size_t align, gfp_t flags);
extern struct kmem_cache_s *vmklnx_kmem_cache_create(struct vmklnx_mem_info *mem_desc,
                                                     const char *name , 
                                                     size_t size, size_t offset, 
                                                     void (*ctor)(void *, struct kmem_cache_s *, unsigned long),
                                                     void (*dtor)(void *, struct kmem_cache_s *, unsigned long));
extern int vmklnx_kmem_cache_destroy(struct kmem_cache_s *cache);
extern void *vmklnx_kmem_cache_alloc(struct kmem_cache_s *cache, gfp_t flags);
extern void vmklnx_kmem_cache_free(struct kmem_cache_s *cache, void *item);
extern vmk_ModuleID vmklnx_get_driver_module_id(const struct device_driver *drv);
extern char *vmklnx_kstrdup(vmk_HeapID heapID, const char *s, void *ra);
extern void vmklnx_dma_free_coherent_by_ma(struct device *dev, size_t size, dma_addr_t handle);
extern void vmklnx_dma_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr);
extern void vmklnx_pci_free_consistent_by_ma(struct pci_dev *hwdev, size_t size, dma_addr_t dma_handle);
extern void vmklnx_pci_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr);
extern VMK_ReturnStatus vmklnx_register_module(struct module *module, vmk_ModuleID modID);
extern VMK_ReturnStatus vmklnx_unregister_module(struct module *module);
extern struct module * vmklnx_get_module(vmk_ModuleID modID);
extern struct module * vmklnx_put_module(vmk_ModuleID modID);

struct vmklnx_mem_info {
	vmk_HeapID                heapID;
	vmk_MemPool               mempool;
        vmk_MemPhysAddrConstraint mem_phys_addr_type;
        vmk_MemPhysContiguity     mem_phys_contig_type;
};

struct vmklnx_codma {
   struct semaphore *mutex;
   u64 mask;
   vmk_HeapID heapID;
   char* heapName;
   u32 heapSize;
};
extern struct vmklnx_codma vmklnx_codma;

// per-module rcu data for vmklnx
struct vmklnx_rcu_data {
   // number of active RCU readlock(s)
   atomic64_t            nestingLevel;   /* number of RCU's entered */

   // Queue of callbacks waiting on this mnodule
   spinlock_t            lock;
   volatile int          qLen;           /* number of callsbacks */
   struct rcu_head       *nxtlist;       /* list of callbacks */
   struct rcu_head       **nxttail;      /* tail of callback list */

   // Batches completed
   long                  completed;

   // state of this module
   atomic64_t            generation;

   // tasklet to execute "call_rcu" callbacks
   struct tasklet_struct *callback;

   // timer for waiting on quiescent state
   struct timer_list     *delayTimer;
};
extern struct vmklnx_rcu_data vmklnx_rcu_data;

/*
 * Linux random functions.
 */
typedef void (*vmklnx_add_entropy_function)(int);
typedef VMK_ReturnStatus (*vmklnx_get_entropy_function)(void *entropy,
                                                        int bytesRequested,
                                                        int *bytesReturned);
typedef struct LinuxRandomDriver {
   vmklnx_add_entropy_function add_interrupt_entropy;
   vmklnx_add_entropy_function add_hwrng_entropy;
   vmklnx_add_entropy_function add_keyboard_entropy;
   vmklnx_add_entropy_function add_mouse_entropy;
   vmklnx_add_entropy_function add_storage_entropy;

   vmklnx_get_entropy_function get_hw_random_bytes;
   vmklnx_get_entropy_function get_hwrng_random_bytes;
   vmklnx_get_entropy_function get_hw_random_bytes_nonblocking;
   vmklnx_get_entropy_function get_sw_random_bytes;
   vmklnx_get_entropy_function get_sw_only_random_bytes;
} LinuxRandomDriver;

extern VMK_ReturnStatus vmklnx_register_random_driver(LinuxRandomDriver *driver);
extern VMK_ReturnStatus vmklnx_unregister_random_driver(LinuxRandomDriver *driver);

/*
 * workqueue functions.
 */

extern int vmklnx_cancel_work_sync(struct work_struct *work,
                                   struct timer_list *timer);

/*
 * PCI related functions
 */
struct pci_bus;
extern int pci_domain_nr(struct pci_bus *bus);

typedef void (*vmklnx_vf_callback)(struct pci_dev *pf, struct pci_dev *vf,
                                   int vf_idx, void *data);

extern int vmklnx_enable_vfs(struct pci_dev *pf, int num_vfs,
                             vmklnx_vf_callback cb, void *data);
extern void vmklnx_disable_vfs(struct pci_dev *pf, int numvfs,
                               vmklnx_vf_callback cb, void *data);
extern struct pci_dev *vmklnx_get_vf(struct pci_dev *pf, int vf_idx,
                                     vmk_uint32 *sbdf);
extern int vmklnx_configure_net_vf(struct pci_dev *pf_dev, void *cfg_data,
                                   int vf_idx);

/*
 * Misc vmklinux API
 */
#include "vmkapi.h"

static inline vmk_Bool
vmklnx_is_panic(void)
{
   return vmk_SystemCheckState(VMK_SYSTEM_STATE_PANIC);
}

extern vmk_Bool vmklnx_is_physcontig(void *vaddr, vmk_uint32 size);

extern vmk_ModuleID vmklnx_this_module_id;

typedef enum {
   /* The device's link state is down. */
   VMKLNX_UPLINK_LINK_DOWN=0,

   /* The device's link state is up. */
   VMKLNX_UPLINK_LINK_UP=1,
} vmklnx_uplink_link_state;


typedef enum {
   /* The device's watchdog panic mod is disabled. */
   VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_DISABLE=0,

   /* The device's watchdog panic mod is enabled. */
   VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_ENABLE=1,
} vmklnx_uplink_watchdog_panic_mod_state;

struct proc_dir_entry;

struct proc_dir_entry *
vmklnx_create_proc_entry(vmk_ModuleID module_id,
                         vmk_HeapID heap_id,
                         const char *path,
                         mode_t mode,
                         struct proc_dir_entry *parent);

extern void 
vmklnx_remove_proc_entry(vmk_ModuleID module_id,
                         const char *path, 
                         struct proc_dir_entry *from);

extern struct proc_dir_entry *
vmklnx_proc_mkdir(vmk_ModuleID module_id,
                  vmk_HeapID heap_id,
		  const char *path,
                  struct proc_dir_entry *parent);

extern int
vmklnx_proc_entry_exists(const char *path,
			 struct proc_dir_entry *parent);


extern struct task_struct *
vmklnx_kthread_create(vmk_ModuleID module_id,
                      int (*threadfn)(void *data),
                      void *data,
		      char *threadfn_name);

extern int 
vmklnx_kernel_thread(vmk_ModuleID module_id,
                     int (*fn)(void *),
                     void * arg,
		     char *fn_name);

extern struct task_struct * 
vmklnx_GetCurrent(void);

extern int 
vmklnx_is_skb_frags_owner(struct sk_buff *skb);

extern void 
vmklnx_set_skb_frags_owner_vmkernel(struct sk_buff *skb);

extern struct sk_buff *
vmklnx_net_alloc_skb(struct kmem_cache_s *cache, 
                     unsigned int size, 
		     struct net_device *dev, 
		     gfp_t flags);

extern size_t
vmklnx_skb_real_size(void);

struct net_device *
vmklnx_alloc_netdev_mq(struct module *this_module,
                       int sizeof_priv,
                       const char *name,
                       void (*setup)(struct net_device *),
                       unsigned int queue_count);
extern int
vmklnx_seq_open(struct file *file,
                struct module *mod,
		struct seq_operations *op);

extern int 
vmklnx_single_open(struct file *file, 
                   struct module *mod, 
		   int (*show)(struct seq_file *, void *),
		   void *data);

extern VMK_ReturnStatus 
vmklnx_register_event_callback(struct net_device *dev,
                               void *cbFn,
                               void *cbData, 
			       void **cbHdl);

extern VMK_ReturnStatus 
vmklnx_unregister_event_callback(void *cbHdl);

#endif // _VMKLINUX_DIST_H_
