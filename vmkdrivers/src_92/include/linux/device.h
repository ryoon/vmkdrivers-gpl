/*
 * Portions Copyright 2008, 2010-2011, 2013 VMware, Inc.
 */
/*
 * device.h - generic, centralized driver model
 *
 * Copyright (c) 2001-2003 Patrick Mochel <mochel@osdl.org>
 * Copyright (c) 2004-2007 Greg Kroah-Hartman <gregkh@suse.de>
 *
 * This file is released under the GPLv2
 *
 * See Documentation/driver-model/ for more information.
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <linux/ioport.h>
#include <linux/kobject.h>
#include <linux/klist.h>
#include <linux/list.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <asm/semaphore.h>
#include <asm/atomic.h>
#include <asm/device.h>

#define DEVICE_NAME_SIZE	50
#define DEVICE_NAME_HALF	__stringify(20)	/* Less than half to accommodate slop */
#define DEVICE_ID_SIZE		32
#define BUS_ID_SIZE		KOBJ_NAME_LEN

#if defined(__VMKLNX__) && defined(VMKLNX_ALLOW_DEPRECATED)
#pragma message "WARNING: VMKLNX_ALLOW_DEPRECATED enabled. This module is using deprecated interfaces that may be removed in future vmklinux releases!"
#endif

struct device;
struct device_driver;
struct class;
struct class_device;
struct bus_type;

struct bus_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct bus_type *, char * buf);
	ssize_t (*store)(struct bus_type *, const char * buf, size_t count);
};

#define BUS_ATTR(_name,_mode,_show,_store)	\
struct bus_attribute bus_attr_##_name = __ATTR(_name,_mode,_show,_store)

#if defined(__VMKLNX__)
static inline int __must_check 
bus_create_file(struct bus_type *bus, struct bus_attribute *attr)
{
	return 0;
}

static inline void 
bus_remove_file(struct bus_type *bus, struct bus_attribute *attr)
{
}
#else /* !defined(__VMKLNX__) */
extern int __must_check bus_create_file(struct bus_type *,
					struct bus_attribute *);
extern void bus_remove_file(struct bus_type *, struct bus_attribute *);
#endif /* defined(__VMKLNX__) */

struct bus_type {
	const char		* name;
	struct module		* owner;

#if defined(__VMKLNX__)
	/*
	 * Use kref directly for reference counting
	 * instead of using kobject.
	 */
	struct kref     	kref;
#else /* !defined(__VMKLNX__) */
	struct kset		subsys;
	struct kset		drivers;
	struct kset		devices;
#endif /* !defined(__VMKLNX__) */
	struct klist		klist_devices;
	struct klist		klist_drivers;

	struct blocking_notifier_head bus_notifier;

	struct bus_attribute	* bus_attrs;
	struct device_attribute	* dev_attrs;
	struct driver_attribute	* drv_attrs;

	int		(*match)(struct device * dev, struct device_driver * drv);
	int		(*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int		(*probe)(struct device * dev);
	int		(*remove)(struct device * dev);
	void		(*shutdown)(struct device * dev);

#if defined(__VMKLNX__)
	/*
	 * ESX Deviation Note:
	 *
	 * System suspend/resume is not supported in ESX.  Prior to
	 * ESX 6.0, this struct included system suspend/resume
	 * callbacks, but they were never called.  Space is left for
	 * them in the struct to keep it the same size and make
	 * shimming easier.  In ESX 6.0, dev_pm_ops has been added
	 * to support USB autosuspend, reusing some of the space.
	 */
	const struct dev_pm_ops *pm;
	void *unused1;
	void *unused2;
	void *unused3;
#else
	int (*suspend)(struct device * dev, pm_message_t state);
	int (*suspend_late)(struct device * dev, pm_message_t state);
	int (*resume_early)(struct device * dev);
	int (*resume)(struct device * dev);
#endif

	unsigned int drivers_autoprobe:1;
};

extern int __must_check bus_register(struct bus_type * bus);
extern void bus_unregister(struct bus_type * bus);

extern int __must_check bus_rescan_devices(struct bus_type * bus);

/* iterator helpers for buses */

int bus_for_each_dev(struct bus_type * bus, struct device * start, void * data,
		     int (*fn)(struct device *, void *));
struct device * bus_find_device(struct bus_type *bus, struct device *start,
				void *data, int (*match)(struct device *, void *));

int __must_check bus_for_each_drv(struct bus_type *bus,
		struct device_driver *start, void *data,
		int (*fn)(struct device_driver *, void *));

/*
 * Bus notifiers: Get notified of addition/removal of devices
 * and binding/unbinding of drivers to devices.
 * In the long run, it should be a replacement for the platform
 * notify hooks.
 */
struct notifier_block;

extern int bus_register_notifier(struct bus_type *bus,
				 struct notifier_block *nb);
extern int bus_unregister_notifier(struct bus_type *bus,
				   struct notifier_block *nb);

/* All 4 notifers below get called with the target struct device *
 * as an argument. Note that those functions are likely to be called
 * with the device semaphore held in the core, so be careful.
 */
#define BUS_NOTIFY_ADD_DEVICE		0x00000001 /* device added */
#define BUS_NOTIFY_DEL_DEVICE		0x00000002 /* device removed */
#define BUS_NOTIFY_BOUND_DRIVER		0x00000003 /* driver bound to device */
#define BUS_NOTIFY_UNBIND_DRIVER	0x00000004 /* driver about to be
						      unbound */

struct device_driver {
	const char		* name;
	struct bus_type		* bus;

#if defined(__VMKLNX__)
	/*
	 * Use kref directly for reference counting
	 * instead of using kobject.
	 */
	struct kref		kref;
#endif /* defined(__VMKLNX__) */
	struct kobject		kobj;
	struct klist		klist_devices;
	struct klist_node	knode_bus;

	struct module		* owner;
	const char 		* mod_name;	/* used for built-in modules */
	struct module_kobject	* mkobj;

	int	(*probe)	(struct device * dev);
	int	(*remove)	(struct device * dev);
	void	(*shutdown)	(struct device * dev);
	int	(*suspend)	(struct device * dev, pm_message_t state);
	int	(*resume)	(struct device * dev);
};


extern int __must_check driver_register(struct device_driver * drv);
extern void driver_unregister(struct device_driver * drv);

extern struct device_driver * get_driver(struct device_driver * drv);
extern void put_driver(struct device_driver * drv);
extern struct device_driver *driver_find(const char *name, struct bus_type *bus);
extern int driver_probe_done(void);

/* sysfs interface for exporting driver attributes */

struct driver_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct device_driver *, char * buf);
	ssize_t (*store)(struct device_driver *, const char * buf, size_t count);
};

#define DRIVER_ATTR(_name,_mode,_show,_store)	\
struct driver_attribute driver_attr_##_name = __ATTR(_name,_mode,_show,_store)

#if defined(__VMKLNX__)
/**
 *	driver_create_file - create sysfs file for a driver
 *	@drv:	ignored
 *	@attr:	ignored
 *
 *	A non-operational function provided to help reduce
 *      kernel ifdefs. It is not supported in this release of ESX.
 *
 *	ESX Deviation Notes:
 *	This function is a non-operational in ESX.  The interface is
 *	provided purely to avoid driver changes (i.e. #ifdefs).  If
 *      the driver actually relies upon sysfs for configuration or
 *      management, then some other means must be implemented for
 *      ESX.
 *
 *	RETURN VALUE:
 *	0 always
 */

/* _VMKLNX_CODECHECK_: driver_create_file */
static inline int
driver_create_file(struct device_driver * drv, struct driver_attribute * attr)
{
	return 0;
}

/**
 *	driver_remove_file - non-operational function
 *	@drv:	ignored
 *	@attr:	ignored
 *
 *	This function is a non-operational function provided to help reduce
 *	kernel ifdefs.  It is not supported in this release of ESX.
 *
 *	ESX Deviation Notes:
 *	This function is a non-operational function provided to help reduce
 *	kernel ifdefs.  It is not supported in this release of ESX.
 *
 *	RETURN VALUE:
 *	This function does not return a value
 */

/* _VMKLNX_CODECHECK_: driver_remove_file */
static inline void
driver_remove_file(struct device_driver * drv, struct driver_attribute * attr)
{
	return;
}
#else /* !defined(__VMKLNX__) */
extern int __must_check driver_create_file(struct device_driver *,
					struct driver_attribute *);
extern void driver_remove_file(struct device_driver *, struct driver_attribute *);
#endif /* defined(__VMKLNX__) */

extern int __must_check driver_for_each_device(struct device_driver * drv,
		struct device *start, void *data,
		int (*fn)(struct device *, void *));
struct device * driver_find_device(struct device_driver *drv,
				   struct device *start, void *data,
				   int (*match)(struct device *, void *));

/*
 * device classes
 */
struct class {
	const char		* name;
	struct module		* owner;

#if defined(__VMKLNX__)
	/*
	 * Use kref directly for reference counting
	 * instead of using kobject.
	 */
	struct kref		kref;
#else /* !defined(__VMKLNX__) */
	struct kset		subsys;
#endif /* !defined(__VMKLNX__) */
	struct list_head	children;
	struct list_head	devices;
	struct list_head	interfaces;
#if !defined(__VMKLNX__)
	struct kset		class_dirs;
#endif /* !defined(__VMKLNX__) */
	struct semaphore	sem;	/* locks both the children and interfaces lists */

	struct class_attribute		* class_attrs;
	struct class_device_attribute	* class_dev_attrs;
	struct device_attribute		* dev_attrs;

	int	(*uevent)(struct class_device *dev, struct kobj_uevent_env *env);
	int	(*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);

	void	(*release)(struct class_device *dev);
	void	(*class_release)(struct class *class);
	void	(*dev_release)(struct device *dev);

#if defined(__VMKLNX__)
	/*
	 * ESX Deviation Note:
	 *
	 * System suspend/resume is not supported in ESX.  Prior to
	 * ESX 6.0, this struct included system suspend/resume
	 * callbacks, but they were never called.  Space is left for
	 * them in the struct to keep it the same size and make
	 * shimming easier.  In ESX 6.0, dev_pm_ops has been added
	 * to support USB autosuspend, reusing some of the space.
	 */
	const struct dev_pm_ops *pm;
	void *unused1;
#else
	int	(*suspend)(struct device *, pm_message_t state);
	int	(*resume)(struct device *);
#endif
};

extern int __must_check class_register(struct class *);

extern void class_unregister(struct class *);


struct class_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct class *, char * buf);
	ssize_t (*store)(struct class *, const char * buf, size_t count);
};

#define CLASS_ATTR(_name,_mode,_show,_store)			\
struct class_attribute class_attr_##_name = __ATTR(_name,_mode,_show,_store) 

extern int __must_check class_create_file(struct class *,
					const struct class_attribute *);
#if defined(__VMKLNX__)
static inline void 
class_remove_file(struct class *cls, const struct class_attribute *attr)
{
}
#else /* !defined(__VMKLNX__) */
extern void class_remove_file(struct class *, const struct class_attribute *);
#endif /* defined(__VMKLNX__) */

struct class_device_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct class_device *, char * buf);
	ssize_t (*store)(struct class_device *, const char * buf, size_t count);
};

#define CLASS_DEVICE_ATTR(_name,_mode,_show,_store)		\
struct class_device_attribute class_device_attr_##_name = 	\
	__ATTR(_name,_mode,_show,_store)

#if defined(__VMKLNX__)
/**
 * class_device_create_file - create an attribute file for the class device object
 * @class_dev: pointer of the struct class_device
 * @attr: pointer of the struct class_device_attribute
 *
 *
 * This function is a non-operational function provided to help reduce
 * kernel ifdefs.  It is not supported in this release of ESX.
 *
 * ESX Deviation Notes:
 * This function is a non-operational function provided to help reduce
 * kernel ifdefs.  It is not supported in this release of ESX.
 *
 *
 * RETURN VALUE:
 * Always returns 0.
 *
 */
/* _VMKLNX_CODECHECK_: class_device_create_file */
static inline int 
class_device_create_file(struct class_device * class_dev,
			     const struct class_device_attribute * attr)
{
	return 0;
}
#else /* !defined(__VMKLNX__) */
extern int __must_check class_device_create_file(struct class_device *,
				    const struct class_device_attribute *);
#endif /* defined(__VMKLNX__) */

/**
 * struct class_device - class devices
 * @class: pointer to the parent class for this class device.  This is required.
 * @devt: for internal use by the driver core only.
 * @node: for internal use by the driver core only.
 * @kobj: for internal use by the driver core only.
 * @groups: optional additional groups to be created
 * @dev: if set, a symlink to the struct device is created in the sysfs
 * directory for this struct class device.
 * @class_data: pointer to whatever you want to store here for this struct
 * class_device.  Use class_get_devdata() and class_set_devdata() to get and
 * set this pointer.
 * @parent: pointer to a struct class_device that is the parent of this struct
 * class_device.  If NULL, this class_device will show up at the root of the
 * struct class in sysfs (which is probably what you want to have happen.)
 * @release: pointer to a release function for this struct class_device.  If
 * set, this will be called instead of the class specific release function.
 * Only use this if you want to override the default release function, like
 * when you are nesting class_device structures.
 * @uevent: pointer to a uevent function for this struct class_device.  If
 * set, this will be called instead of the class specific uevent function.
 * Only use this if you want to override the default uevent function, like
 * when you are nesting class_device structures.
 * @kref: Used to reference count this class device
 * @class_id: Unique string for this class
 */
struct class_device {
	struct list_head	node;
#if defined(__VMKLNX__)
	/*
	 * Use kref directly for reference counting
	 * instead of using kobject.
	 */
	struct kref		kref;
#endif /* defined(__VMKLNX__) */
	struct kobject		kobj;
	struct class		* class;	/* required */
	dev_t			devt;		/* dev_t, creates the sysfs "dev" */
	struct device		* dev;		/* not necessary, but nice to have */
	void			* class_data;	/* class-specific data */
	struct class_device	*parent;	/* parent of this child device, if there is one */
	struct attribute_group  ** groups;	/* optional groups */

	void	(*release)(struct class_device *dev);
#if !defined(__VMKLNX__)
	int	(*uevent)(struct class_device *dev, struct kobj_uevent_env *env);
#endif /* !defined(__VMKLNX__) */
	char	class_id[BUS_ID_SIZE];	/* unique to this class */
};

/**
 * class_get_devdata - get class-specific data for a device
 * @dev: pointer of the struct class_device
 *
 * Get class-specific data for a device.
 *
 *  RETURN VALUE:
 *  Device class-specific data pointer.
 */
/* _VMKLNX_CODECHECK_: class_get_devdata */
static inline void *
class_get_devdata (struct class_device *dev)
{
	return dev->class_data;
}

/**                                          
 *  class_set_devdata - set class_data of struct class_device  
 *  @dev: class_device struct
 *  @data: data to be set
 *                                           
 *  Sets class_data of class_device struct
 *                                           
 *  RETURN VALUE:                     
 *  None 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: class_set_devdata */
static inline void
class_set_devdata (struct class_device *dev, void *data)
{
	dev->class_data = data;
}


extern int __must_check class_device_register(struct class_device *);
extern void class_device_unregister(struct class_device *);
extern void class_device_initialize(struct class_device *);
extern int __must_check class_device_add(struct class_device *);
extern void class_device_del(struct class_device *);

extern struct class_device * class_device_get(struct class_device *);
extern void class_device_put(struct class_device *);

#if defined(__VMKLNX__)
/**
 * class_device_remove_file - remove an attribute file for the class device object
 * @class_dev: pointer to the struct class_device
 * @attr: pointer to the struct class_device_attribute
 *
 *
 * This function is a non-operational function provided to help reduce
 * kernel ifdefs.  It is not supported in this release of ESX.
 *
 * ESX Deviation Notes:
 * This function is a non-operational function provided to help reduce
 * kernel ifdefs.  It is not supported in this release of ESX.
 *
 * RETURN VALUE:
 * This function does not have a return value.
 */
/* _VMKLNX_CODECHECK_: class_device_remove_file */
static inline void 
class_device_remove_file(struct class_device * class_dev,
			      const struct class_device_attribute * attr)
{
	return;
}
static inline int __must_check 
class_device_create_bin_file(struct class_device *class_dev,
					struct bin_attribute *attr)
{
	return 0;
}

static inline void 
class_device_remove_bin_file(struct class_device *class_dev, struct bin_attribute *attr)
{
}
#else /* !defined(__VMKLNX__) */
extern void class_device_remove_file(struct class_device *, 
				     const struct class_device_attribute *);
extern int __must_check class_device_create_bin_file(struct class_device *,
					struct bin_attribute *);
extern void class_device_remove_bin_file(struct class_device *,
					 struct bin_attribute *);
#endif /* defined(__VMKLNX__) */

struct class_interface {
	struct list_head	node;
	struct class		*class;

	int (*add)	(struct class_device *, struct class_interface *);
	void (*remove)	(struct class_device *, struct class_interface *);
	int (*add_dev)		(struct device *, struct class_interface *);
	void (*remove_dev)	(struct device *, struct class_interface *);
};

#if defined(__VMKLNX__)
static inline int __must_check 
class_interface_register(struct class_interface *cls_intf)
{
	return 0;
}
static inline void 
class_interface_unregister(struct class_interface *cls_intf)
{
}
#else /* !defined(__VMKLNX__) */
extern int __must_check class_interface_register(struct class_interface *);
extern void class_interface_unregister(struct class_interface *);
#endif /* defined(__VMKLNX__) */

extern struct class *class_create(struct module *owner, const char *name);
extern void class_destroy(struct class *cls);
extern struct class_device *class_device_create(struct class *cls,
						struct class_device *parent,
						dev_t devt,
						struct device *device,
						const char *fmt, ...)
					__attribute__((format(printf,5,6)));
extern void class_device_destroy(struct class *cls, dev_t devt);

/*
 * The type of device, "struct device" is embedded in. A class
 * or bus can contain devices of different types
 * like "partitions" and "disks", "mouse" and "event".
 * This identifies the device type and carries type-specific
 * information, equivalent to the kobj_type of a kobject.
 * If "name" is specified, the uevent will contain it in
 * the DEVTYPE variable.
 */
struct device_type {
	const char *name;
	struct attribute_group **groups;
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	void (*release)(struct device *dev);

#if defined(__VMKLNX__)
	/*
	 * ESX Deviation Note:
	 *
	 * System suspend/resume is not supported in ESX.  Prior to
	 * ESX 6.0, this struct included system suspend/resume
	 * callbacks, but they were never called.  Space is left for
	 * them in the struct to keep it the same size and make
	 * shimming easier.  In ESX 6.0, dev_pm_ops has been added
	 * to support USB autosuspend, reusing some of the space.
	 */
	const struct dev_pm_ops *pm;
	void *unused1;
#else
	int (*suspend)(struct device * dev, pm_message_t state);
	int (*resume)(struct device * dev);
#endif
};

/* interface for exporting device attributes */
struct device_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count);
};

#define DEVICE_ATTR(_name,_mode,_show,_store) \
struct device_attribute dev_attr_##_name = __ATTR(_name,_mode,_show,_store)

#if defined(__VMKLNX__)
/**
 *	device_create_file - non-operational function
 *	@dev:	ignored
 *	@attr:	ignored
 *
 *	This is a non-operational function provided to help reduce
 *	kernel ifdefs. It is not supported in this release of ESX. 
 *
 *	ESX Deviation Notes:
 *	This is a non-operational function provided to help reduce
 *	kernel ifdefs. It is not supported in this release of ESX. 
 *
 *	RETURN VALUE:
 *	0 always
 *
 *	SEE ALSO:
 *	sysfs_create_file
 */
/* _VMKLNX_CODECHECK_: device_create_file */
static inline int 
device_create_file(struct device * dev, struct device_attribute * attr)
{
	return 0;
}

/**
 *	device_remove_file - non-operational function
 *	@dev:	ignored
 *	@attr:	ignored
 *
 *	This is a non-operational function provided to help reduce
 *	kernel ifdefs. It is not supported in this release of ESX. 
 *
 *	ESX Deviation Notes:
 *	This is a non-operational function provided to help reduce
 *	kernel ifdefs. It is not supported in this release of ESX. 
 *
 *	RETURN VALUE:
 *	None
 *
 *	SEE ALSO:
 *	sysfs_remove_file
 */
/* _VMKLNX_CODECHECK_: device_remove_file */
static inline void 
device_remove_file(struct device * dev, struct device_attribute * attr)
{
}
#else /* !defined(__VMKLNX__) */
extern int __must_check device_create_file(struct device *device,
					struct device_attribute * entry);
extern void device_remove_file(struct device * dev, struct device_attribute * attr);
#endif /* defined(__VMKLNX__) */

extern int __must_check device_create_bin_file(struct device *dev,
					       struct bin_attribute *attr);
extern void device_remove_bin_file(struct device *dev,
				   struct bin_attribute *attr);
#if defined(__VMKLNX__)
static inline int 
device_schedule_callback_owner(struct device *dev, void (*func)(struct device *), struct module *owner)
{
	return 0;
}
#else /* !defined(__VMKLNX__) */
extern int device_schedule_callback_owner(struct device *dev,
		void (*func)(struct device *), struct module *owner);
#endif /* defined(__VMKLNX__) */

/* This is a macro to avoid include problems with THIS_MODULE */
#define device_schedule_callback(dev, func)			\
	device_schedule_callback_owner(dev, func, THIS_MODULE)

/* device resource management */
typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

#ifdef CONFIG_DEBUG_DEVRES
extern void * __devres_alloc(dr_release_t release, size_t size, gfp_t gfp,
			     const char *name);
#define devres_alloc(release, size, gfp) \
	__devres_alloc(release, size, gfp, #release)
#else
extern void * devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
#endif
extern void devres_free(void *res);
extern void devres_add(struct device *dev, void *res);
extern void * devres_find(struct device *dev, dr_release_t release,
			  dr_match_t match, void *match_data);
extern void * devres_get(struct device *dev, void *new_res,
			 dr_match_t match, void *match_data);
extern void * devres_remove(struct device *dev, dr_release_t release,
			    dr_match_t match, void *match_data);
extern int devres_destroy(struct device *dev, dr_release_t release,
			  dr_match_t match, void *match_data);

/* devres group */
extern void * __must_check devres_open_group(struct device *dev, void *id,
					     gfp_t gfp);
extern void devres_close_group(struct device *dev, void *id);
extern void devres_remove_group(struct device *dev, void *id);
extern int devres_release_group(struct device *dev, void *id);

/* managed kzalloc/kfree for device drivers, no kmalloc, always use kzalloc */
extern void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
extern void devm_kfree(struct device *dev, void *p);

#if defined(__VMKLNX__)
/*
 * usb bus type and others need to be added if needed in future
 */
enum vmklnx_device_type {
   UNKNOWN_DEVICE_TYPE,
   SCSI_HOST_TYPE,
   SCSI_TARGET_TYPE,
   SCSI_DEVICE_TYPE,
   PCI_DEVICE_TYPE,
   FC_RPORT_TYPE,
   FC_VPORT_TYPE,
   SAS_PORT_DEVICE_TYPE,
   SAS_PHY_DEVICE_TYPE,
   SAS_END_DEVICE_TYPE,
   SAS_EXPANDER_DEVICE_TYPE,
};
#endif

struct device {
	struct klist		klist_children;
	struct klist_node	knode_parent;		/* node in sibling list */
	struct klist_node	knode_driver;
	struct klist_node	knode_bus;
	struct device		*parent;
#if defined(__VMKLNX__)
	/*
	 * Use kref directly for reference counting
	 * instead of using kobject.
	 */
	struct kref		kref;
#endif /* defined(__VMKLNX__) */
	struct kobject kobj;
	char	bus_id[BUS_ID_SIZE];	/* position on parent bus */
	struct device_type	*type;
	unsigned		is_registered:1;
	unsigned		uevent_suppress:1;

	struct semaphore	sem;	/* semaphore to synchronize calls to
					 * its driver.
					 */

	struct bus_type	* bus;		/* type of bus device is on */
	struct device_driver *driver;	/* which driver has allocated this
					   device */
	void		*driver_data;	/* data private to the driver */
	void		*platform_data;	/* Platform specific data, device
					   core doesn't touch it */
	struct dev_pm_info	power;

#ifdef CONFIG_NUMA
	int		numa_node;	/* NUMA node this device is close to */
#endif
	u64		*dma_mask;	/* dma mask (if dma'able device) */
	u64		coherent_dma_mask;/* Like dma_mask, but for
					     alloc_coherent mappings as
					     not all hardware supports
					     64 bit addresses for consistent
					     allocations such descriptors. */
#if defined(__VMKLNX__)
	union {
		struct list_head	reserved2;
		struct dev_pm_info2	*power2; /* additions to dev_pm_info */
	};
#else
	struct list_head	dma_pools;	/* dma pools (if dma'ble) */
#endif

	struct dma_coherent_mem	*dma_mem; /* internal for coherent mem
					     override */
#if defined(__VMKLNX__)
        union {
                 struct dev_archdata	reserved;
		 u64                    dma_boundary;
        };
#else /* !defined(__VMKLNX__) */
	/* arch specific additions */
	struct dev_archdata	archdata;
#endif /* defined(__VMKLNX__) */

	spinlock_t		devres_lock;
	struct list_head	devres_head;

	/* class_device migration path */
	struct list_head	node;
	struct class		*class;
	dev_t			devt;		/* dev_t, creates the sysfs "dev" */
#if !defined(__VMKLNX__)
	struct attribute_group	**groups;	/* optional groups */
#endif /* !defined(__VMKLNX__) */

	void	(*release)(struct device * dev);

#if defined(__VMKLNX__)
        enum vmklnx_device_type dev_type;
        
        void *dma_engine_primary;
        void *dma_engine_secondary;
	int dma_identity_mapped;
#endif /* defined(__VMKLNX__) */
};

#if defined(__VMKLNX__) && defined(VMX86_DEBUG)
/* Retained the old struct device here for offset comparison */
struct device_OLD {
	struct klist		klist_children;
	struct klist_node	knode_parent;
	struct klist_node	knode_driver;
	struct klist_node	knode_bus;
	struct device		*parent;
#if defined(__VMKLNX__)
	struct kref		kref;
#endif /* defined(__VMKLNX__) */
	struct kobject          kobj;
	char	                bus_id[BUS_ID_SIZE];
	struct device_type	*type;
	unsigned		is_registered:1;
	unsigned		uevent_suppress:1;

	struct semaphore	sem;

	struct bus_type	* bus;
	struct device_driver    *driver;
	void		        *driver_data;
	void		        *platform_data;
	struct dev_pm_info	power;

#ifdef CONFIG_NUMA
	int		        numa_node;
#endif
	u64		        *dma_mask;
	u64		        coherent_dma_mask;
	struct list_head	dma_pools;

	struct dma_coherent_mem	*dma_mem;
	struct dev_archdata	archdata;

	spinlock_t		devres_lock;
	struct list_head	devres_head;

	struct list_head	node;
	struct class		*class;
	dev_t			devt;
#if !defined(__VMKLNX__)
	struct attribute_group	**groups;
#endif /* !defined(__VMKLNX__) */

	void (*release)(struct device * dev);

#if defined(__VMKLNX__)
        enum vmklnx_device_type dev_type;
        
        void                    *dma_engine_primary;
        void                    *dma_engine_secondary;
	int                     dma_identity_mapped;
#endif /* defined(__VMKLNX__) */
};

#define SAME_OFFSET(st1, st2, field) (offsetof(st1, field) == offsetof(st2, field))
VMK_ASSERT_LIST(struct_device_offset_check,
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, klist_children));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, knode_parent));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, knode_driver));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, knode_bus));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, parent));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, kref));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, bus_id));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, type));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, sem));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, bus));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, driver));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, driver_data));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, platform_data));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, power));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dma_mask));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, coherent_dma_mask));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dma_mem));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, devres_lock));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, devres_head));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, node));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, class));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, devt));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, release));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dev_type));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dma_engine_primary));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dma_engine_secondary));
    VMK_ASSERT_ON_COMPILE(SAME_OFFSET(struct device, struct device_OLD, dma_identity_mapped));
)
#endif /* defined(__VMKLNX__) && defined(VMX86_DEBUG) */

#if defined(__VMKLNX__)
/*
 * 2009: From linux 2.6.29 include/linux/device.h, released under GPLv2
 */
static inline const char *dev_name(const struct device *dev)
{
        return dev->bus_id;
}

extern int dev_set_name(struct device *dev, const char *name, ...)
			__attribute__((format(printf, 2, 3)));
#endif /* defined(__VMKLNX__) */

#ifdef CONFIG_NUMA
static inline int dev_to_node(struct device *dev)
{
	return dev->numa_node;
}

static inline void set_dev_node(struct device *dev, int node)
{
	dev->numa_node = node;
}
#else
/**
 *  dev_to_node - A non operational function 
 *  @dev: Ignored
 *
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 * 
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Always returns -1 
 *
 */
/* _VMKLNX_CODECHECK_: dev_to_node */

static inline int dev_to_node(struct device *dev)
{
	return -1;
}
/**
 *  set_dev_node - A non operational function 
 *  @dev: Ignored
 *  @node: Ignored
 *
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 * 
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value 
 *
 */
/* _VMKLNX_CODECHECK_: set_dev_node */
static inline void set_dev_node(struct device *dev, int node)
{
}
#endif

/**                                          
 *  dev_get_drvdata - Gets driver private data.       
 *  @dev: pointer to the device.
 *                                           
 *  Returns the device's private data.                       
 *                                           
 *  RETURN VALUE: 
 *  Device's private data
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dev_get_drvdata */
static inline void *
dev_get_drvdata (struct device *dev)
{
	return dev->driver_data;
}

/**                                          
 *  dev_set_drvdata - Attach driver private data to a device.
 *  @dev: device to be set up with driver specific data.
 *  @data: pointer to driver specific data
 *
 *  Typically called from a driver's "probe" function, this service
 *  attaches a driver specific data structure to the specified device
 *  structure.
 *
 *  RETURN VALUE:                     
 *  None.
 */                                          
/* _VMKLNX_CODECHECK_: dev_set_drvdata */
static inline void
dev_set_drvdata (struct device *dev, void *data)
{
	dev->driver_data = data;
}

/**                                          
 *  device_is_registered - returns if device is registered with bus.
 *  @dev: pointer to the device to check
 *                                           
 *  This function checks whether the device is registered with the bus                        
 *  subsystem.
 *
 *  RETURN VALUE:                     
 *  1 if device is registered
 *  0 if device is not registered. 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: device_is_registered */
static inline int device_is_registered(struct device *dev)
{
	return dev->is_registered;
}

void driver_init(void);

/*
 * High level routines for use by the bus drivers
 */
extern int __must_check device_register(struct device * dev);
extern void device_unregister(struct device * dev);
extern void device_initialize(struct device * dev);
extern int __must_check device_add(struct device * dev);
extern void device_del(struct device * dev);
extern int device_for_each_child(struct device *, void *,
		     int (*fn)(struct device *, void *));
#if defined(__VMKLNX__)
extern int device_for_each_child_safe(struct device * parent, void * data,
		     int (*fn)(struct device *, void *));
#endif /* defined(__VMKLNX__) */
extern struct device *device_find_child(struct device *, void *data,
					int (*match)(struct device *, void *));
#if defined(__VMKLNX__)
static inline int 
device_rename(struct device *dev, char *new_name)
{
	return 0;
}

static inline int 
device_move(struct device *dev, struct device *new_parent)
{
	return 0;
}
#else /* !defined(__VMKLNX__) */
extern int device_rename(struct device *dev, char *new_name);
extern int device_move(struct device *dev, struct device *new_parent);
#endif /* defined(__VMKLNX__) */

/*
 * Manual binding of a device to driver. See drivers/base/bus.c
 * for information on use.
 */
extern int __must_check device_bind_driver(struct device *dev);
extern void device_release_driver(struct device * dev);
extern int  __must_check device_attach(struct device * dev);
extern int __must_check driver_attach(struct device_driver *drv);
extern int __must_check device_reprobe(struct device *dev);

/*
 * Easy functions for dynamically creating devices on the fly
 */
extern struct device *device_create(struct class *cls, struct device *parent,
				    dev_t devt, const char *fmt, ...)
				    __attribute__((format(printf,4,5)));
extern void device_destroy(struct class *cls, dev_t devt);

/*
 * Platform "fixup" functions - allow the platform to have their say
 * about devices and actions that the general device layer doesn't
 * know about.
 */
/* Notify platform of device discovery */
extern int (*platform_notify)(struct device * dev);

extern int (*platform_notify_remove)(struct device * dev);


/**
 * get_device - atomically increment the reference count for the device.
 *
 *  @dev: Pointer to the device struct to increment the reference count
 *
 *  RETURN VALUE
 *  Pointer to the device structure 
 */
extern struct device * get_device(struct device * dev);
extern void put_device(struct device * dev);


/* drivers/base/power/shutdown.c */
extern void device_shutdown(void);

/* drivers/base/sys.c */
extern void sysdev_shutdown(void);


/* drivers/base/firmware.c */
#if !defined(__VMKLNX__)
extern int __must_check firmware_register(struct kset *);
extern void firmware_unregister(struct kset *);
#endif /* !defined(__VMKLNX__) */

/* debugging and troubleshooting/diagnostic helpers. */
extern const char *dev_driver_string(struct device *dev);

/**
 *  dev_printk - A wrapper for printk(), that prints driver identification and
 *  device location information
 *  @level: log level
 *  @dev: pointer to device struct
 *  @format: printk style format string
 *
 *  A wrapper for printk(), that prints driver identification and
 *  device location information
 *
 *  SYNOPSIS:
 *      #define dev_printk(level, dev, format, arg...)
 *
 *  RETURN VALUE:
 *  NONE 
 *
 */
/* _VMKLNX_CODECHECK_: dev_printk */
#define dev_printk(level, dev, format, arg...)	\
	printk(level "%s %s: " format , dev_driver_string(dev) , (dev)->bus_id , ## arg)

#ifdef DEBUG
/**
 *  dev_dbg - print messages to the vmkernel log
 *  @dev: device to print
 *  @format: format string
 *  @arg: values for format string
 *
 *  Prints formatted messages to the vmkernel log.  This function
 *  is a no-op if DEBUG is not set.
 *
 *  SYNOPSIS:
 *  #define dev_dbg(dev, format, arg...)
 *
 *  SEE ALSO:
 *  printk
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_dbg */
#define dev_dbg(dev, format, arg...)		\
	dev_printk(KERN_DEBUG , dev , format , ## arg)
#else
#if defined(__VMKLNX__)
#define dev_dbg(dev, format, arg...) do {} while (0 && (dev))
#else /* !defined(__VMKLNX__) */
static inline int __attribute__ ((format (printf, 2, 3)))
dev_dbg(struct device * dev, const char * fmt, ...)
{
	return 0;
}
#endif /* defined(__VMKLNX__) */
#endif

#ifdef VERBOSE_DEBUG
#define dev_vdbg	dev_dbg
#else
#if defined(__VMKLNX__)
#define dev_vdbg(dev, format, arg...) do {} while (0 && (dev))
#else /* !defined(__VMKLNX__) */
static inline int __attribute__ ((format (printf, 2, 3)))
dev_vdbg(struct device * dev, const char * fmt, ...)
{
	return 0;
}
#endif /* defined(__VMKLNX__) */
#endif

/**
 *  dev_err - print messages to the vmkernel log
 *  @dev: device to print
 *  @format: format string
 *  @arg: values for format string
 *
 *  Prints formatted messages to the vmkernel log.
 *
 *  SYNOPSIS:
 *  #define dev_err(dev, format, arg...)
 *
 *  SEE ALSO:
 *  printk
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_err */
#define dev_err(dev, format, arg...)		\
	dev_printk(KERN_ERR , dev , format , ## arg)
/**
 *  dev_info - print messages to the vmkernel log
 *  @dev: device to print
 *  @format: format string
 *  @arg: values for format string
 *
 *  Prints formatted messages to the vmkernel log.
 *
 *  SYNOPSIS:
 *  #define dev_info(dev, format, arg...)
 *
 *  SEE ALSO:
 *  printk
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_info */
#define dev_info(dev, format, arg...)		\
	dev_printk(KERN_INFO , dev , format , ## arg)
/**
 *  dev_warn - print messages to the vmkernel log
 *  @dev: device to print
 *  @format: format string
 *  @arg: values for format string
 *
 *  Prints formatted messages to the vmkernel log.
 *
 *  SYNOPSIS:
 *  #define dev_warn(dev, format, arg...)
 *
 *  SEE ALSO:
 *  printk
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_warn */
#define dev_warn(dev, format, arg...)		\
	dev_printk(KERN_WARNING , dev , format , ## arg)
/**
 *  dev_notice - print messages to the vmkernel log
 *  @dev: device to print
 *  @format: format string
 *  @arg: values for format string
 *
 *  Prints formatted messages to the vmkernel log.
 *
 *  SYNOPSIS:
 *  #define dev_notice(dev, format, arg...)
 *
 *  SEE ALSO:
 *  printk
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_notice */
#define dev_notice(dev, format, arg...)		\
	dev_printk(KERN_NOTICE , dev , format , ## arg)

/* Create alias, so I can be autoloaded. */
#define MODULE_ALIAS_CHARDEV(major,minor) \
	MODULE_ALIAS("char-major-" __stringify(major) "-" __stringify(minor))
#define MODULE_ALIAS_CHARDEV_MAJOR(major) \
	MODULE_ALIAS("char-major-" __stringify(major) "-*")
#endif /* _DEVICE_H_ */
