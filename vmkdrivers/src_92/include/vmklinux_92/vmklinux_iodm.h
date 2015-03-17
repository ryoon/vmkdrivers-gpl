/* ****************************************************************
 * Copyright 2008-2011 VMware, Inc.
 * * ****************************************************************/
#ifndef _VMKLNX_IODM_H
#define _VMKLNX_IODM_H

#include <scsi/scsi_host.h>

/*
 * iodm scsi event types, add new event type at the end,
 * do not change the order of those values
 */

enum iodm_scsi_event_type {
  IODM_IOERROR,
  IODM_RSCN,
  IODM_LINKUP,
  IODM_LINKDOWN,
  IODM_FRAMEDROP,
  IODM_LUNRESET,
  IODM_FCOE_CVL,
  IODM_MAX_EVENTID,
};

void vmklnx_iodm_event(struct Scsi_Host *sh, unsigned int id, 
                       void *addr, unsigned long data);

void vmklnx_iodm_enable_events(struct Scsi_Host *sh);
void vmklnx_iodm_disable_events(struct Scsi_Host *sh);
#endif /* _VMKLNX_IODM_H */
