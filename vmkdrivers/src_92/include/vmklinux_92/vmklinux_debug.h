/* ****************************************************************
 * Copyright 2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * ****************************************************************/
#ifndef _VMKLINUX_DEBUG_H_
#define _VMKLINUX_DEBUG_H_

#if defined(VMX86_DEBUG)

#define VMKLNX_DEBUG_ONLY(x)  x

#else /* !defined(VMX86_DEBUG) */

#define VMKLNX_DEBUG_ONLY(x)

#endif /* defined(VMX86_DEBUG) */

#endif /*_VMKLINUX_DEBUG_H_ */
