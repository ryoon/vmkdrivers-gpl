/*
 * Portions Copyright 2009 - 2011 VMware, Inc.
 */
#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include "vmklinux_92/vmklinux_scsi.h"

extern int ata_scsi_abort(struct scsi_cmnd *cmd);
extern int ata_scsi_device_reset(struct scsi_cmnd *cmd);
extern int ata_scsi_bus_reset(struct scsi_cmnd *cmd);
extern int ata_scsi_host_reset(struct scsi_cmnd *cmd);

#define init_timer_deferrable(_timer) init_timer((_timer))
#define cancel_rearming_delayed_work(_work) cancel_delayed_work((_work))

#if !defined(GPCMD_WRITE_12)
#define GPCMD_WRITE_12		0xaa
#endif

static inline int strncasecmp(const char *s1, const char *s2, size_t n)
{
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while ((--n > 0) && c1 == c2 && c1 != 0);
	return c1 - c2;
}

static inline int strcasecmp(const char *s1, const char *s2)
{
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != 0);
	return c1 - c2;
}

#endif /* _KCOMPAT_H_ */
