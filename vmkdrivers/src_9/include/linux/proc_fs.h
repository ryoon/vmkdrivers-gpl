/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>

/*
 * The proc filesystem constants/structures
 */

/*
 * Offset of the first process in the /proc root directory..
 */
#define FIRST_PROCESS_ENTRY 256


/*
 * We always define these enumerators
 */

enum {
	PROC_ROOT_INO = 1,
};

#define PROC_SUPER_MAGIC 0x9fa0

/*
 * This is not completely implemented yet. The idea is to
 * create an in-memory tree (like the actual /proc filesystem
 * tree) of these proc_dir_entries, so that we can dynamically
 * add new files to /proc.
 *
 * The "next" pointer creates a linked list of one /proc directory,
 * while parent/subdir create the directory structure (every
 * /proc file has a parent, but "subdir" is NULL for all
 * non-directory entries).
 *
 * "get_info" is called at "read", while "owner" is used to protect module
 * from unloading while proc_dir_entry is in use
 */

typedef	int (read_proc_t)(char *page, char **start, off_t off,
			  int count, int *eof, void *data);
typedef	int (write_proc_t)(struct file *file, const char __user *buffer,
			   unsigned long count, void *data);
typedef int (get_info_t)(char *, char **, off_t, int);

#if defined(__VMKLNX__)
struct proc_dir_entry {
	unsigned short namelen;
	const char *name;
	mode_t mode;
	loff_t size;
	struct module *owner;
	void *data;
	read_proc_t *read_proc;
	write_proc_t *write_proc;

	vmk_ModuleID module_id;    /* owner module ID                   */
        vmk_HeapID heap_id;        /* owner heap ID                     */
	void *vmkplxr_proc_entry;  /* link to vmkplexer associated node */
};
#else /* !defined(__VMKLNX__) */
struct proc_dir_entry {
	unsigned int low_ino;
	unsigned short namelen;
	const char *name;
	mode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	loff_t size;
	struct inode_operations * proc_iops;
	const struct file_operations * proc_fops;
	get_info_t *get_info;
	struct module *owner;
	struct proc_dir_entry *next, *parent, *subdir;
	void *data;
	read_proc_t *read_proc;
	write_proc_t *write_proc;
	atomic_t count;		/* use count */
	int deleted;		/* delete flag */
	void *set;
};
#endif /* defined(__VMKLNX__) */

struct kcore_list {
	struct kcore_list *next;
	unsigned long addr;
	size_t size;
};

struct vmcore {
	struct list_head list;
	unsigned long long paddr;
	unsigned long long size;
	loff_t offset;
};

#ifdef CONFIG_PROC_FS

extern struct proc_dir_entry proc_root;
extern struct proc_dir_entry *proc_root_fs;
extern struct proc_dir_entry *proc_net;
extern struct proc_dir_entry *proc_net_stat;
extern struct proc_dir_entry *proc_bus;
extern struct proc_dir_entry *proc_root_driver;
extern struct proc_dir_entry *proc_root_kcore;

extern spinlock_t proc_subdir_lock;

extern void proc_root_init(void);
extern void proc_misc_init(void);

struct mm_struct;

void proc_flush_task(struct task_struct *task);
struct dentry *proc_pid_lookup(struct inode *dir, struct dentry * dentry, struct nameidata *);
int proc_pid_readdir(struct file * filp, void * dirent, filldir_t filldir);
unsigned long task_vsize(struct mm_struct *);
int task_statm(struct mm_struct *, int *, int *, int *, int *);
char *task_mem(struct mm_struct *, char *);

#if defined(__VMKLNX__)
/**                                          
 *  create_proc_entry - Creates new entry under proc
 *  @name: proc node
 *  @mode: mode for creation of the node
 *  @parent: pointer to the parent node
 *                                           
 *  This function creates an entry in the proc system under the parent with the 
 *  specified 'name'. Important fields are filled and a pointer to the newly 
 *  created proc entry is returned.
 *
 *  RETURN VALUE:                     
 *  Pointer to the newly created proc entry of type proc_dir_entry. 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: create_proc_entry */
static inline struct proc_dir_entry *
create_proc_entry(const char *name, mode_t mode, struct proc_dir_entry *parent)
{
   return vmklnx_create_proc_entry(vmklnx_this_module_id,
                                   VMK_MODULE_HEAP_ID,
                                   name,
				   mode,
				   parent);
}

/**                                          
 *  remove_proc_entry - remove a /proc entry and free it if it's not currently in use. 
 *  @name: Proc entry name to remove
 *  @parent: Parent proc node to search under
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: remove_proc_entry */
static inline void 
remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
   vmklnx_remove_proc_entry(vmklnx_this_module_id, name, parent);
}
#else /* !defined(__VMKLNX__) */
extern struct proc_dir_entry *create_proc_entry(const char *name, mode_t mode,
						struct proc_dir_entry *parent);
extern void remove_proc_entry(const char *name, struct proc_dir_entry *parent);
#endif /* defined(__VMKLNX__) */

extern struct vfsmount *proc_mnt;
extern int proc_fill_super(struct super_block *,void *,int);
extern struct inode *proc_get_inode(struct super_block *, unsigned int, struct proc_dir_entry *);

extern int proc_match(int, const char *,struct proc_dir_entry *);

/*
 * These are generic /proc routines that use the internal
 * "struct proc_dir_entry" tree to traverse the filesystem.
 *
 * The /proc root directory has extended versions to take care
 * of the /proc/<pid> subdirectories.
 */
extern int proc_readdir(struct file *, void *, filldir_t);
extern struct dentry *proc_lookup(struct inode *, struct dentry *, struct nameidata *);

extern const struct file_operations proc_kcore_operations;
extern const struct file_operations proc_kmsg_operations;
extern const struct file_operations ppc_htab_operations;

/*
 * proc_tty.c
 */
struct tty_driver;
extern void proc_tty_init(void);
extern void proc_tty_register_driver(struct tty_driver *driver);
extern void proc_tty_unregister_driver(struct tty_driver *driver);

/*
 * proc_devtree.c
 */
#ifdef CONFIG_PROC_DEVICETREE
struct device_node;
struct property;
extern void proc_device_tree_init(void);
extern void proc_device_tree_add_node(struct device_node *, struct proc_dir_entry *);
extern void proc_device_tree_add_prop(struct proc_dir_entry *pde, struct property *prop);
extern void proc_device_tree_remove_prop(struct proc_dir_entry *pde,
					 struct property *prop);
extern void proc_device_tree_update_prop(struct proc_dir_entry *pde,
					 struct property *newprop,
					 struct property *oldprop);
#endif /* CONFIG_PROC_DEVICETREE */

extern struct proc_dir_entry *proc_symlink(const char *,
		struct proc_dir_entry *, const char *);

#if defined(__VMKLNX__)
/**
 *  proc_mkdir - create an entry in the proc file system.
 *  @name: the name of the proc entry
 *  @parent: the node under which the proc entry is created
 *
 *  Creates a proc entry @name in the proc filesystem under node @parent. @name
 *  can be a path and @parent can be NULL.
 *
 *  RETURN VALUE:
 *  Returns a pointer to the newly created proc_dir_entry, NULL otherwise.
 *
 */
/* _VMKLNX_CODECHECK_: proc_mkdir */
static inline struct proc_dir_entry *
proc_mkdir(const char *name, struct proc_dir_entry *parent)
{
   return vmklnx_proc_mkdir(vmklnx_this_module_id,
                     VMK_MODULE_HEAP_ID,
                     name,
		     parent);
}
#else /* !defined(__VMKLNX__) */
extern struct proc_dir_entry *proc_mkdir(const char *,struct proc_dir_entry *);
#endif /* defined(__VMKLNX__) */

extern struct proc_dir_entry *proc_mkdir_mode(const char *name, mode_t mode,
			struct proc_dir_entry *parent);

/**                                          
 *  create_proc_read_entry - Create a new read proc entry
 *  @name: Name of the proc entry
 *  @mode: Mode of the proc entry
 *  @base: Pointer to base struct proc_dir_entry
 *  @read_proc: Read proc entry
 *  @data: Data
 *                                           
 *  Creates a new read proc entry. A new proc entry is first created and it 
 *  is filled with the @read_proc and @data if the creation was successful.
 *                                           
 *  RETURN VALUE:
 *  Pointer to the newly created proc entry of type @base
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: create_proc_read_entry */
static inline struct proc_dir_entry *create_proc_read_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, 
	read_proc_t *read_proc, void * data)
{
	struct proc_dir_entry *res=create_proc_entry(name,mode,base);
	if (res) {
		res->read_proc=read_proc;
		res->data=data;
	}
	return res;
}
 
#if !defined(__VMKLNX__)
static inline struct proc_dir_entry *create_proc_info_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, get_info_t *get_info)
{
	struct proc_dir_entry *res=create_proc_entry(name,mode,base);
	if (res) res->get_info=get_info;
	return res;
}
 
static inline struct proc_dir_entry *proc_net_create(const char *name,
	mode_t mode, get_info_t *get_info)
{
	return create_proc_info_entry(name,mode,proc_net,get_info);
}

static inline struct proc_dir_entry *proc_net_fops_create(const char *name,
	mode_t mode, const struct file_operations *fops)
{
	struct proc_dir_entry *res = create_proc_entry(name, mode, proc_net);
	if (res)
		res->proc_fops = fops;
	return res;
}

static inline void proc_net_remove(const char *name)
{
	remove_proc_entry(name,proc_net);
}
#endif /* !defined(__VMKLNX__) */

#else

#define proc_root_driver NULL
#define proc_net NULL
#define proc_bus NULL

#define proc_net_fops_create(name, mode, fops)  ({ (void)(mode), NULL; })
#define proc_net_create(name, mode, info)	({ (void)(mode), NULL; })
static inline void proc_net_remove(const char *name) {}

static inline void proc_flush_task(struct task_struct *task) { }

static inline struct proc_dir_entry *create_proc_entry(const char *name,
	mode_t mode, struct proc_dir_entry *parent) { return NULL; }

#define remove_proc_entry(name, parent) do {} while (0)

static inline struct proc_dir_entry *proc_symlink(const char *name,
		struct proc_dir_entry *parent,const char *dest) {return NULL;}
static inline struct proc_dir_entry *proc_mkdir(const char *name,
	struct proc_dir_entry *parent) {return NULL;}

static inline struct proc_dir_entry *create_proc_read_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, 
	read_proc_t *read_proc, void * data) { return NULL; }
static inline struct proc_dir_entry *create_proc_info_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, get_info_t *get_info)
	{ return NULL; }

struct tty_driver;
static inline void proc_tty_register_driver(struct tty_driver *driver) {};
static inline void proc_tty_unregister_driver(struct tty_driver *driver) {};

extern struct proc_dir_entry proc_root;

#endif /* CONFIG_PROC_FS */

#if !defined(CONFIG_PROC_KCORE)
static inline void kclist_add(struct kcore_list *new, void *addr, size_t size)
{
}
#else
extern void kclist_add(struct kcore_list *, void *, size_t);
#endif

struct proc_inode {
	struct pid *pid;
	int fd;
	union {
		int (*proc_get_link)(struct inode *, struct dentry **, struct vfsmount **);
		int (*proc_read)(struct task_struct *task, char *page);
	} op;
	struct proc_dir_entry *pde;
	struct inode vfs_inode;
};

static inline struct proc_inode *PROC_I(const struct inode *inode)
{
	return container_of(inode, struct proc_inode, vfs_inode);
}

static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
	return PROC_I(inode)->pde;
}

struct proc_maps_private {
	struct pid *pid;
	struct task_struct *task;
	struct vm_area_struct *tail_vma;
};

#endif /* _LINUX_PROC_FS_H */
