/*
 * Portions Copyright 2010, 2012 VMware, Inc.
 */

/*
 * driver.c - centralized device driver management
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "base.h"

#define to_dev(node) container_of(node, struct device, driver_list)

#if !defined(__VMKLLNX__)
#define to_drv(obj) container_of(obj, struct device_driver, kobj)
#endif /* !defined(__VMKLNX__) */


static struct device * next_device(struct klist_iter * i)
{
	struct klist_node * n = klist_next(i);
	return n ? container_of(n, struct device, knode_driver) : NULL;
}

/**
 *	driver_for_each_device - Iterator for devices bound to a driver.
 *	@drv:	Driver we're iterating.
 *	@start: Device to begin with
 *	@data:	Data to pass to the callback.
 *	@fn:	Function to call for each device.
 *
 *	Iterate over the @drv's list of devices calling @fn for each one.
 *
 *      The iteration stops if function "fn" returns an error.
 *
 *      RETURN VALUE:
 *      0 if the iteration completes, else the error returned by "fn".
 */

/* _VMKLNX_CODECHECK_: driver_for_each_device */
int driver_for_each_device(struct device_driver * drv, struct device * start, 
			   void * data, int (*fn)(struct device *, void *))
{
	struct klist_iter i;
	struct device * dev;
	int error = 0;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (!drv)
		return -EINVAL;

	klist_iter_init_node(&drv->klist_devices, &i,
			     start ? &start->knode_driver : NULL);
	while ((dev = next_device(&i)) && !error)
		error = fn(dev, data);
	klist_iter_exit(&i);
	return error;
}

EXPORT_SYMBOL_GPL(driver_for_each_device);


/**
 * driver_find_device - device iterator for locating a particular device
 * @drv: The device's driver
 * @start: Device to begin with
 * @data: Data to pass to match function
 * @match: Callback function to check device
 *
 * This is similar to the driver_for_each_device(), but
 * it returns a reference to a device that is 'found' for later use, as
 * determined by the @match callback.
 *
 * The callback should return 0 if the device doesn't match and non-zero
 * if it does.  If the callback returns non-zero, this function will
 * return to the caller and not iterate over any more devices.
 *
 * RETURN VALUE:
 * Pointer to device
 */
/* _VMKLNX_CODECHECK_: driver_find_device */
struct device * driver_find_device(struct device_driver *drv,
				   struct device * start, void * data,
				   int (*match)(struct device *, void *))
{
	struct klist_iter i;
	struct device *dev;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (!drv)
		return NULL;

	klist_iter_init_node(&drv->klist_devices, &i,
			     (start ? &start->knode_driver : NULL));
	while ((dev = next_device(&i)))
		if (match(dev, data) && get_device(dev))
			break;
	klist_iter_exit(&i);
	return dev;
}
EXPORT_SYMBOL_GPL(driver_find_device);

#if !defined(__VMKLNX__)
int driver_create_file(struct device_driver * drv, struct driver_attribute * attr)
{
	int error;
	if (get_driver(drv)) {
		error = sysfs_create_file(&drv->kobj, &attr->attr);
		put_driver(drv);
	} else
		error = -EINVAL;
	return error;
}

void driver_remove_file(struct device_driver * drv, struct driver_attribute * attr)
{
	if (get_driver(drv)) {
		sysfs_remove_file(&drv->kobj, &attr->attr);
		put_driver(drv);
	}
}
#endif /* !defined(__VMKLNX__) */

#if defined(__VMKLNX__)
/*
 * All device_driver structs are statically defined. There should be no need for a memory
 * release routine, but kref_put() requires one. This is why this null body function is
 * defined here.
 */
static inline void
device_driver_release(struct kref *kref)
{
}
#endif /* defined(__VMKLNX__) */


/**
 *	get_driver - increment driver reference count.
 *	@drv:	driver.
 *
 *      This simply forwards the call to kobject_get(), though
 *      we do take care to provide for the case that we get a NULL
 *      pointer passed in.
 *
 *      RETURN VALUE:
 *      Pointer to the device_driver structure of specified driver.
 */
/* _VMKLNX_CODECHECK_: get_driver */
struct device_driver * get_driver(struct device_driver * drv)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	if (drv) {
		kref_get(&drv->kref);
	}

	return drv;
#else /* !defined(__VMKLNX__) */
	return drv ? to_drv(kobject_get(&drv->kobj)) : NULL;
#endif /* defined(__VMKLNX__) */
}

/**
 *	put_driver - decrement driver's refcount.
 *	@drv:	driver.
 *
 *	Decrement driver's reference count.
 *
 */
/* _VMKLNX_CODECHECK_: put_driver */
void put_driver(struct device_driver * drv)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	if (drv) {
		kref_put(&drv->kref, device_driver_release);
	}
#else /* !defined(__VMKLNX__) */
	kobject_put(&drv->kobj);
#endif /* defined(__VMKLNX__) */
}

/**
 *	driver_register - register driver with bus
 *	@drv:	driver to register
 *
 *	Registers the given device_driver with its bus.  Most registration work
 *	is delegated to bus_add_driver, which deals with the bus structures.
 *
 *	RETURN VALUE:
 *	0 for success,
 *	or other error code from bus_add_driver upon failure to register the
 *	given device (one of -EINVAL, -ENOMEM, -EFAULT, -EEXIST, or -ENOENT)
 *
 *	SEE ALSO:
 *	bus_add_driver
 */
/* _VMKLNX_CODECHECK_: driver_register */
int driver_register(struct device_driver * drv)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if ((drv->bus->probe && drv->probe) ||
	    (drv->bus->remove && drv->remove) ||
	    (drv->bus->shutdown && drv->shutdown)) {
		printk(KERN_WARNING "Driver '%s' needs updating - please use bus_type methods\n", drv->name);
	}
	klist_init(&drv->klist_devices, NULL, NULL);
	return bus_add_driver(drv);
}

/**
 *	driver_unregister - remove driver from system
 *	@drv:	driver
 *
 *	Unregisters the given device_driver with its bus.  Unregistration work
 *	is delegated to bus_remove_driver, which deals with bus structures.
 *
 *	RETURN VALUE:
 *	This function does not return a value
 *
 *	SEE ALSO:
 *	bus_remove_driver
 */

/* _VMKLNX_CODECHECK_: driver_unregister */
void driver_unregister(struct device_driver * drv)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	bus_remove_driver(drv);
}

#if !defined(__VMKLNX__)
/**
 *	driver_find - locate driver on a bus by its name.
 *	@name:	name of the driver.
 *	@bus:	bus to scan for the driver.
 *
 *	Call kset_find_obj() to iterate over list of drivers on
 *	a bus to find driver by name. Return driver if found.
 *
 *	Note that kset_find_obj increments driver's reference count.
 */
struct device_driver *driver_find(const char *name, struct bus_type *bus)
{
	struct kobject *k = kset_find_obj(&bus->drivers, name);
	if (k)
		return to_drv(k);
	return NULL;
}
#endif /* !defined(__VMKLNX__) */

EXPORT_SYMBOL_GPL(driver_register);
EXPORT_SYMBOL_GPL(driver_unregister);
EXPORT_SYMBOL_GPL(get_driver);
EXPORT_SYMBOL_GPL(put_driver);
#if !defined(__VMKLNX__)
EXPORT_SYMBOL_GPL(driver_find);

EXPORT_SYMBOL_GPL(driver_create_file);
EXPORT_SYMBOL_GPL(driver_remove_file);
#endif /* !defined(__VMKLNX__) */
