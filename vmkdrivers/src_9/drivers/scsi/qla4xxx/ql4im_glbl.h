/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

/*
 * Global include file.
 */
#ifndef __QL4IM_GBL_H
#define	__QL4IM_GBL_H

extern uint8_t drvr_major;
extern uint8_t drvr_minor;
extern uint8_t drvr_patch;
extern uint8_t drvr_beta;
extern char drvr_ver[];

/*
 * Defined in ql4im_os.c
 */
extern uint32_t ql4im_get_hba_count(void);
#ifdef __VMKLNX__
extern int ql4im_down_timeout(struct semaphore *sema, unsigned long timeout);
extern int ioctl_timeout;
extern int ql4im_mem_alloc(int hba_idx, struct scsi_qla_host *ha);
extern void ql4im_mem_free(int hba_idx);
#endif

/* 
 * Defined in ql4im_ioctl.c
 */
extern int qla4xxx_ioctl(int cmd, void *arg);
extern struct hba_ioctl *ql4im_get_adapter_handle(uint16_t instance);


#endif /* _QL4IM_GBL_H */
