/* ****************************************************************
 * Copyright 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include "vmkapi.h"
#include <linux/device.h>

int
bus_register_9_2_0_0(struct bus_type *bus)
{
	bus->pm = NULL;
	bus->unused1 = NULL;
	bus->unused2 = NULL;
	bus->unused3 = NULL;
	return bus_register(bus);
}
VMK_MODULE_EXPORT_SYMBOL_ALIASED(bus_register_9_2_0_0, bus_register);

int
class_register_9_2_0_0(struct class *class)
{
	class->pm = NULL;
	class->unused1 = NULL;
	return class_register(class);
}
VMK_MODULE_EXPORT_SYMBOL_ALIASED(class_register_9_2_0_0, class_register);

int
device_register_9_2_0_0(struct device *device)
{
	/*
	 * A device should normally never be registered with a NULL
	 * type, but ddic test code does so; PR 924079.
	 */
	if (device->type != NULL) {
		device->type->pm = NULL;
		device->type->unused1 = NULL;
	}
	return device_register(device);
}
VMK_MODULE_EXPORT_SYMBOL_ALIASED(device_register_9_2_0_0, device_register);

