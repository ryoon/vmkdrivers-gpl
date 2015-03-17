/*
 * Portions Copyright 2011 VMware, Inc.
 */
/*
 *  pm_wakeup.h - Power management wakeup interface
 *
 *  Copyright (C) 2008 Alan Stern
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_PM_WAKEUP_H
#define _LINUX_PM_WAKEUP_H

/*
 * VMKLNX note:
 * From Linux 2.6.34.1, git commit 3db48f5c1a68e801146ca58ff94f3898c6fbf90e.
 * Due to an include ordering issue with the vmklinux version of
 * device.h, the inline functions have been rewritten as macros and
 * this file is included from pm.h instead of device.h.
 */

#if defined(__VMKLNX__)
#ifndef _LINUX_PM_H
# error "please don't include this file directly"
#endif
#else
#ifndef _DEVICE_H_
# error "please don't include this file directly"
#endif
#endif

/*
 * XXX Future vmklinux cleanup opportunity: It might be good to remove
 * the ifdef here and instead expose the CONFIG_PM versions of these
 * functions to all drivers, because network drivers use them in
 * connection with WOL, which ESX does support.  That might reduce
 * some ifdefs in those drivers.
 */
#ifdef CONFIG_PM

#define device_init_wakeup(dev, val) \
   ((dev)->power.can_wakeup = (dev)->power.should_wakeup = !!(val))

#define device_set_wakeup_capable(dev, val) \
   ((dev)->power.can_wakeup = !!(val))

#define device_can_wakeup(dev) \
   ((dev)->power.can_wakeup)

#define device_set_wakeup_enable(dev, val) \
   ((dev)->power.should_wakeup = !!(val))

#define device_may_wakeup(dev) \
   ((dev)->power.can_wakeup && (dev)->power.should_wakeup)

#else /* !CONFIG_PM */

#define device_init_wakeup(dev, val) \
   ((dev)->power.can_wakeup = !!(val))

#define device_set_wakeup_capable(dev, val)

#define device_can_wakeup(dev) \
   ((dev)->power.can_wakeup)

#define device_set_wakeup_enable(dev, val)

#define device_may_wakeup(dev) 0

#endif /* !CONFIG_PM */

#endif /* _LINUX_PM_WAKEUP_H */
