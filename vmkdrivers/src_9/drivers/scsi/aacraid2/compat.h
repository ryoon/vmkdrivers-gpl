/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
 *
 * Copyright (c) 2004-2007 Adaptec, Inc. (aacraid@adaptec.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
/*
 * This file is for backwards compatibility with older kernel versions
 */

#ifndef scsi_sglist
#define scsi_sglist(cmd) ((struct scatterlist *)((cmd)->request_buffer))
#endif
#ifndef scsi_bufflen
#define scsi_bufflen(cmd) ((cmd)->request_bufflen)
#endif
#ifndef scsi_sg_count
#define scsi_sg_count(cmd) ((cmd)->use_sg)
#endif
#define scsi_dma_unmap(cmd) if(scsi_sg_count(cmd))pci_unmap_sg(((struct aac_dev *)cmd->device->host->hostdata)->pdev,scsi_sglist(cmd),scsi_sg_count(cmd),cmd->sc_data_direction)
#define scsi_dma_map(cmd) ((scsi_sg_count(cmd))?pci_map_sg(((struct aac_dev *)cmd->device->host->hostdata)->pdev,scsi_sglist(cmd),scsi_sg_count(cmd),cmd->sc_data_direction):0)
#ifndef scsi_resid
#define scsi_resid(cmd) ((cmd)->resid)
#define scsi_set_resid(cmd,res) (cmd)->resid = res
#endif
#ifndef scsi_for_each_sg
#define scsi_for_each_sg(cmd, sg, nseg, __i) \
	for (__i = 0, sg = scsi_sglist(cmd); __i < (nseg); __i++, (sg)++)
#endif

# define uintptr_t ptrdiff_t

#ifndef sdev_printk
#define sdev_printk(prefix, sdev, fmt, a...) \
	printk(prefix " %d:%d:%d:%d: " fmt, sdev->host->host_no, \
		sdev_channel(sdev), sdev_id(sdev), sdev->lun, ##a)
#endif
