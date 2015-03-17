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
 * the caller must own the device lock */
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


/* recursively traverse device tree, unclaim devices;
 * the caller must own the device lock */
static int usb_passthrough_traverse_unclaim(struct usb_device *dev, char *boot_dev)
{
	int i, ret = 0;

	usb_passthrough_unclaim(dev, boot_dev);

	for (i = 0; i < dev->maxchild; i++) {
		struct usb_device *childdev = dev->children[i];

		if (childdev) {
			usb_lock_device(childdev);
			ret = usb_passthrough_traverse_unclaim(childdev, boot_dev);
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
	int ret = 0;
	char boot_dev[VMK_MISC_NAME_MAX + 1];

	switch (cmd) {
	case USBPASSTHROUGH_UNCLAIM:
                if (copy_from_user(boot_dev, (void __user *)arg, sizeof(boot_dev)))
			return -EFAULT;
		
		if (!strncmp(boot_dev, "off", sizeof("off"))) {
			atomic_set(&in_usb_passthrough, 0);
			printk(KERN_INFO "usb passthrough disabled\n");
			break;
		}
			
		atomic_set(&in_usb_passthrough, 1);

		boot_dev[VMK_MISC_NAME_MAX] = 0;
		printk(KERN_INFO "usb passthrough enabled; all eligible "
			"devices will be unclaimed by kernel drivers%s%s\n",
			boot_dev[0] ? " except for ESXi boot device " : "",
			boot_dev);
		mutex_lock(&usb_bus_list_lock);
		list_for_each_entry(bus, &usb_bus_list, bus_list) {
			if (!bus->root_hub)
				continue;
			usb_lock_device(bus->root_hub);
			ret = usb_passthrough_traverse_unclaim(bus->root_hub, boot_dev);
			usb_unlock_device(bus->root_hub);
			if (ret < 0) {
				mutex_unlock(&usb_bus_list_lock);
				return ret;
			}
		}
		mutex_unlock(&usb_bus_list_lock);
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
