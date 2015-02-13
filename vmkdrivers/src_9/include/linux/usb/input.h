#ifndef __USB_INPUT_H
#define __USB_INPUT_H

/*
 * Copyright (C) 2005 Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/usb.h>
#include <linux/input.h>
#include <asm/byteorder.h>

/**                                          
 *  usb_to_input_id - populate input_id structure
 *  @dev: pointer to usb device
 *  @id: pointer to input_id structure
 *                                           
 *  Populate the input_id structure with information from usb dev's descriptor
 *  info
 *                                           
 *  RETURN VALUE:
 *  None.
 */                                          
/* _VMKLNX_CODECHECK_: usb_to_input_id */
static inline void
usb_to_input_id(const struct usb_device *dev, struct input_id *id)
{
	id->bustype = BUS_USB;
	id->vendor = le16_to_cpu(dev->descriptor.idVendor);
	id->product = le16_to_cpu(dev->descriptor.idProduct);
	id->version = le16_to_cpu(dev->descriptor.bcdDevice);
}

#endif
