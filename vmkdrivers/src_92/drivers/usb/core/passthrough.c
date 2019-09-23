/*
 * Copyright 2009 VMware, Inc.
 */
/*****************************************************************************/
/*
 *      passthrough.c  --  USB passthrough ioctl capability
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Id: passthrough.c,v 1.1 2009/07/22 14:57:35 kigor Exp $
 *
 *  This file implements the USB passthrough ioctl capability.  Ioctl
 *  interfaces are presented which enable controlled sharing of devices
 *  between kernel mode drivers on the one hand and user mode drivers
 *  and USB passthrough to VM functionality on the other hand.
 */
/*****************************************************************************/

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/hcd.h>
#include <asm/uaccess.h>


#define USBPASSTHROUGH_UNCLAIM _IOW('P', 0, char[VMK_MISC_NAME_MAX])
#define USBPASSTHROUGH_DEV "usbpassthrough"

atomic_t in_usb_passthrough = ATOMIC_INIT(0);

/* unclaim interfaces in device's active configuration;
 * the caller must own the device lock
 * XXX the return value is ignored so the recursion never fails */
static int usb_passthrough_unclaim(struct usb_device *dev, char *boot_dev)
{
	int i;
	int ret = 0;
	struct usb_driver* driver;

	if (!dev->actconfig)
		return ret;

	for (i = 0; i < dev->actconfig->desc.bNumInterfaces; i++) {
		struct usb_interface *iface = dev->actconfig->interface[i];
		driver = iface->dev.driver ? to_usb_driver(iface->dev.driver) : NULL;
		if (driver && driver->ioctl) {
			iface->unclaim_rq = 1;
			VMKAPI_MODULE_CALL(driver->module->moduleID, ret, driver->ioctl, 
				iface, USBPASSTHROUGH_UNCLAIM, boot_dev);
			if (ret < 0 && ret != -ENOSYS)
				dev_warn(&iface->dev, "usb passthrough: unclaim failed with %d\n", ret);
		}
	}

	return ret;
}


struct usb_dev_entry {
	struct usb_device *dev;
	struct list_head   dev_list;
};


/* determine device's to be (re)claimed, the caller must own the device lock
 * so there will be a deadlock if we try to claim the device here.
 * XXX the return value is ignored so the recursion never fails */
static int
usb_passthrough_claim(struct usb_device *dev, struct list_head *dev_list)
{
	int err = 0;

	dev_info(&dev->dev, "device %p has passthrough %s be attached\n",
		dev, dev->passthrough ? "on, will" : "off, will NOT");
	if (dev->passthrough) {
		struct usb_dev_entry *entry = kmalloc(sizeof(struct usb_dev_entry),
						      GFP_KERNEL);
		if (!entry) {
			dev_warn(&dev->dev, "No memory to add device %p to "
				"dev_list\n", dev);
			err = -ENOMEM;
		} else {
			dev_info(&dev->dev, "Adding device %p to dev_list"
				" for attachment after traverse\n", dev);
			entry->dev = dev;
			list_add_tail(&entry->dev_list, dev_list);
		}
	}
	return (err);
}


/* recursively traverse device tree, (un)claim devices;
 * the caller must own the device lock
 * XXX failures are ignored so we traverse the whole tree skipping failures */
static int usb_passthrough_traverse(struct usb_device *dev, char *boot_dev,
				    int claim, struct list_head *usb_dev_list)
{
	int i, ret = 0;

	if (claim) {
		/* XXX return value is ignored so the recursion never fails */
		usb_passthrough_claim(dev, usb_dev_list);
	} else {
		/* XXX return value is ignored so the recursion never fails */
		usb_passthrough_unclaim(dev, boot_dev);
	}

	for (i = 0; i < dev->maxchild; i++) {
		struct usb_device *childdev = dev->children[i];

		if (childdev) {
			usb_lock_device(childdev);
			ret = usb_passthrough_traverse(childdev, boot_dev,
						       claim, usb_dev_list);
			usb_unlock_device(childdev);
			if (ret < 0)
				break;
		}
	}

	return ret;
}


static int usb_passthrough_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usb_bus *bus;
	int claim, ret = 0;
	char boot_dev[VMK_MISC_NAME_MAX + 1];
	LIST_HEAD (usb_dev_list);

	switch (cmd) {
	case USBPASSTHROUGH_UNCLAIM:
		if (copy_from_user(boot_dev, (void __user *)arg, sizeof(boot_dev)))
			return -EFAULT;

		if (!strncmp(boot_dev, "off", sizeof("off"))) {
			atomic_set(&in_usb_passthrough, 0);
			claim = 1;
			boot_dev[0] = 0;
		} else {
			atomic_set(&in_usb_passthrough, 1);
			claim = 0;
			boot_dev[VMK_MISC_NAME_MAX] = 0;
		}

		printk(KERN_INFO "usb passthrough %s; all eligible devices will"
			         " be %sclaimed by kernel drivers%s%s\n",
		       claim ? "disabled" : "enabled", claim ? "" : "un",
		       boot_dev[0] ? " except for ESXi boot device " : "",
		       boot_dev);
		/*
		 * Walk the bus list, visiting all devices on each tree and
		 *
		 * Unclaim case (boot_dev != "off") just unclaim the device by
		 * sending an ioctl to device's driver.  This avoids lock issue.
		 *
		 * Claim case (boot_dev == "off") add device to list of devices
		 * that will be claimed in step 2 because we have the device
		 * locked during traverse and can't release or attach drivers.
		 * Note: we can't send an ioctl because we don't know driver.
		 */
		mutex_lock(&usb_bus_list_lock);
		list_for_each_entry(bus, &usb_bus_list, bus_list) {
			/*
			 * XXX If ret < 0 just finish loop and clean up list
			 */
			if (!bus->root_hub || ret < 0)
				continue;
			usb_lock_device(bus->root_hub);
			ret = usb_passthrough_traverse(bus->root_hub, boot_dev,
							claim, &usb_dev_list);
			usb_unlock_device(bus->root_hub);
			if (ret < 0) {
				mutex_unlock(&usb_bus_list_lock);
				printk("Unexpected recursion failure in ioctl "
					"handler for USBPASSTHROUGH_UNCLAIM\n");
				VMK_ASSERT(0); /* XXX comment out in 6.0, 5.5 */
				return ret;
			}
		}
		mutex_unlock(&usb_bus_list_lock);
		if (claim) {
			struct usb_dev_entry *dev_entry, *dev_entry_temp;

			// Now walk the list of devices from step 1
			list_for_each_entry_safe(dev_entry, dev_entry_temp,
						&usb_dev_list, dev_list) {
				int err = 0;
				struct usb_device *dev = dev_entry->dev;

				/* XXX Release device if it's bound to "usb" */
				//printk ("releasing dev %p\n", dev);
				device_release_driver(&dev->dev);

				/* XXX Allow device to be rebound */
				//printk ("attaching dev %p\n", dev);
				err = device_attach(&dev->dev);
				if (!err) {
					dev_warn(&dev->dev, "Unable to attach "
						"driver for device: %p\n", dev);
				} else {
					dev_info(&dev->dev, "Attached %s driver"
						" to device: %p\n",
						dev->dev.driver ?
						dev->dev.driver->name : "no",
						dev);
				}

				// Delete entry from list and free it
				list_del(&dev_entry->dev_list);
				kfree(dev_entry);
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	
	return ret;
}


static const struct file_operations usb_passthrough_fops = {
	.ioctl	=	usb_passthrough_ioctl,
};


static int usb_passthrough_major;


int __init usb_passthrough_init(void)
{
	if (usb_passthrough_major)
		return 0;

	usb_passthrough_major = register_chrdev(0, USBPASSTHROUGH_DEV, &usb_passthrough_fops);
	if (usb_passthrough_major < 0) {
		printk ("usb passthrough: failed to register %s (%d)\n",
			USBPASSTHROUGH_DEV, usb_passthrough_major);
		usb_passthrough_major = 0;
	}

	return usb_passthrough_major ? 0 : 1;
}


void usb_passthrough_cleanup(void)
{
	if (!usb_passthrough_major)
		return;

	unregister_chrdev(usb_passthrough_major, USBPASSTHROUGH_DEV);
}
