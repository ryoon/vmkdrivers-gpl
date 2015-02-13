/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2010 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#define LPFC_DRIVER_VERSION "8.2.2.1-18vmw"

#if defined(__VMKLNX__)
/*
 * vmklinux26 requires the driver's name the same as the module name
 */
#define LPFC_DRIVER_NAME "lpfc820"
#define LPFC_SP_DRIVER_HANDLER_NAME     "lpfc820:sp"
#define LPFC_FP_DRIVER_HANDLER_NAME     "lpfc820:fp"
#else /* defined(__VMKLNX__) */
#define LPFC_DRIVER_NAME "lpfc"
#define LPFC_SP_DRIVER_HANDLER_NAME     "lpfc:sp"
#define LPFC_FP_DRIVER_HANDLER_NAME     "lpfc:fp"
#endif /* defined(__VMKLNX__) */

#define LPFC_MODULE_ANNOUNCE "Emulex Corporation " LPFC_DRIVER_VERSION
#define LPFC_MODULE_DESC_LP "Emulex LightPulse FC SCSI " LPFC_DRIVER_VERSION
#define LPFC_MODULE_DESC_OC "Emulex OneConnect FCoE SCSI " LPFC_DRIVER_VERSION
#define LPFC_COPYRIGHT "Copyright(c) 2004-2010 Emulex.  All rights reserved."
#define DFC_API_VERSION "0.0.0"
