/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This file contains helper routines that handle debug control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#ifndef __BNX2FC_DEBUG__
#define __BNX2FC_DEBUG__


/* Log level bit mask */
#define LOG_IOERR	0x00000001	/* scsi cmd error, cleanup */
#define LOG_SESS	0x00000002	/* Session setup, cleanup, etc' */
#define LOG_DEV_EVT	0x00000004	/* Device events, link, mtu, etc' */
#define LOG_SCSI_TM	0x00000008	/* SCSI Task Mgmt */
#define LOG_LPORT	0x00000010	/* lport related */
#define LOG_RPORT	0x00000020	/* rport related */
#define LOG_ELS		0x00000040	/* ELS logs */
#define LOG_FRAME	0x00000080	/* fcoe L2 frame related logs*/
#define LOG_INIT	0x00000100	/* Init logs */
#define LOG_DISCO	0x00000200	/* Link discovery events */
#define LOG_TIMER	0x00000400	/* Timer events */
#define LOG_INFO	0x00000800	/* Informational logs, e.g. device MFS,
					 * MAC address, WWPN, WWNN */
#define LOG_MP_REQ	0x00001000	/* Middle Path (MP) related */
#define LOG_ERROR	0x00002000	/* log non-fatal errors */
#define LOG_UNSOL	0x00004000	/* unsolicited event */
#define LOG_NPIV	0x00008000	/* unsolicited event */
#define LOG_FCP_ERR	0x00010000	/* log fcp errors */
#define LOG_VLAN	0x00020000	/* log vlan info */
#define LOG_INITV	0x40000000	/* Init logs */
#define LOG_SESSV	0x80000000	/* Session setup, cleanup, etc' */
#define LOG_ALL		0xffffffff	/* LOG all messages */


extern unsigned long bnx2fc_debug_level;

#define bnx2fc_dbg(mask, fmt, ...)					\
	do {								\
	if (unlikely((mask) & bnx2fc_debug_level))			\
		printk(KERN_ALERT PFX "[%s:%d(%s)]" fmt,		\
		       __func__, __LINE__,				\
		       (hba != NULL ? 					\
		       (hba->netdev ? (hba->netdev->name) : "?") :	\
		       ""),						\
		       ##__VA_ARGS__);					\
} while (0)

/* for errors (never masked) */
#define BNX2FC_ERR(fmt, ...)						\
do {									\
	printk(KERN_ERR "[%s:%d(%s)]" fmt,				\
		__func__, __LINE__,					\
		(hba != NULL ?						\
			(hba->netdev ? (hba->netdev->name) : "?") :	\
		""),							\
		##__VA_ARGS__);						\
} while (0)

#endif
