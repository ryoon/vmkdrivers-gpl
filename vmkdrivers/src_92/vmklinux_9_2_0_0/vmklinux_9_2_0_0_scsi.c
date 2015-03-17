/* ****************************************************************
 * Copyright 2010-2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include "vmkapi.h"
#include "vmklinux_scsi.h"
#include <scsi/scsi_host.h>

int
vmklnx_scsi_get_num_ioqueue_9_2_0_0(struct Scsi_Host *sh,
                                    unsigned int maxQueue)
{
      return vmklnx_scsi_get_num_ioqueue(maxQueue);
}
VMK_MODULE_EXPORT_SYMBOL_ALIASED(vmklnx_scsi_get_num_ioqueue_9_2_0_0,
                                 vmklnx_scsi_get_num_ioqueue);
