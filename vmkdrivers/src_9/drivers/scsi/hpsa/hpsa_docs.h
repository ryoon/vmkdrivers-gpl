/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2000-2012 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
#ifndef _HPSA_DOCS_H
#define _HPSA_DOCS_H

/* module documentation macros and 
 * driver revision/version/name/description 
 * can vary per OS type
 */
#define HPSA_ESX6_0
/* VMware-specific */
#if defined(__VMKLNX__)

#ifdef HPSA_ESX4_0
#define DRIVER_REV "4.0.0"
#endif

#ifdef HPSA_ESX4_1
#define DRIVER_REV "4.1.0"
#endif

#ifdef HPSA_ESX5_0
#define DRIVER_REV "5.0.0"
#endif

#ifdef HPSA_ESX5_1
#define DRIVER_REV "5.1.0"
#endif

#ifdef HPSA_ESX6_0
#define DRIVER_REV "6.0.0.44"
#endif

#define DRIVER_REL "4"
#define DRIVER_BUILD "01"
#define HPSA_DRIVER_VERSION DRIVER_REV "-" DRIVER_REL "vmw"
MODULE_SUPPORTED_DEVICE("HP Smart Array Controllers");

#else // not defined(__VMKLNX__)

/* Linux-specific */
#define DRIVER_REL "1"
#define DRIVER_REV "2.0.2"
#define DRIVER_BUILD "17"
#define HPSA_DRIVER_VERSION  DRIVER_REV "-" DRIVER_REL
MODULE_SUPPORTED_DEVICE("HP Smart Array Controllers");

#endif

/* OS independent */
#define DRIVER_NAME "HP HPSA Driver (v " HPSA_DRIVER_VERSION  ")"
MODULE_AUTHOR("Hewlett-Packard Company");
MODULE_LICENSE("GPL");
MODULE_VERSION(HPSA_DRIVER_VERSION);
MODULE_DESCRIPTION("Driver for HP Smart Array Controllers version " HPSA_DRIVER_VERSION);

#endif /* _HPSA_DOCS_H */
