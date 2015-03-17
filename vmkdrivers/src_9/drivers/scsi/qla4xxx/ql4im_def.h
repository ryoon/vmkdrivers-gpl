/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef __QIM_DEF_H__
#define __QIM_DEF_H__

#include <linux/blkdev.h>
#include "qlisioct.h"
#include "qlinioct.h"
#include "ql4im_dbg.h"

#define QL4IM_VERSION   "v2.01.00b9"
#if defined(__VMKLNX__)
#define QL_TMP_BUF_SIZE		(PAGE_SIZE * 2)
#else
#define QL_TMP_BUF_SIZE    PAGE_SIZE
#endif

/* 
 * INT_DEF_FLASH_BLK_SIZE is which is the maximum flash transfer that can
 * happen is the maximum dma size that could ever happen
 */
#define QL_DMA_BUF_SIZE		\
	(((INT_DEF_FLASH_BLK_SIZE + PAGE_SIZE -1)/PAGE_SIZE) * PAGE_SIZE)
	
struct hba_ioctl {
#ifdef __VMKLNX__
	struct semaphore	ioctl_sem;
#else
	uint32_t		flag;
#define HBA_IOCTL_BUSY		0x0001

	struct mutex		ioctl_sem;
#endif
	uint32_t		aen_reg_mask;
	struct scsi_qla_host	*ha;
	void			*dma_v;
	dma_addr_t		dma_p;
	int			dma_len;
	uint16_t		pt_in_progress;
	uint16_t		aen_read;
	struct scsi_cmnd	pt_scsi_cmd;
	struct srb		pt_srb;
	struct scsi_device	pt_scsi_device;
	struct request		pt_request;
	struct ql4_aen_log	aen_log;
	char			tmp_buf[QL_TMP_BUF_SIZE];
	void			*core;
};

#define IOCTL_INVALID_STATUS			0xffff

#define MIN_CMD_TOV		25

#define MIN(x,y)            ((x)<(y)?(x):(y))
#define MAX(x,y)            ((x)>(y)?(x):(y))
#define LSB(x)  ((uint8_t)(x))
#define MSB(x)  ((uint8_t)((uint16_t)(x) >> 8))

#define SCSI_GOOD                         0x00

#include "ql4im_glbl.h"
#endif /* ifndef __QIM_DEF_H__ */


