/*
 *	Adaptec AAC series RAID controller driver
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

#ifndef PRINT_BUFFER_SIZE

#define PRINT_BUFFER_SIZE     512       /* Debugging print buffer size */

#define HBA_FLAGS_DBG_FLAGS_MASK         0x0000ffff  /* Mask for debug flags */
#define HBA_FLAGS_DBG_KERNEL_PRINT_B     0x00000001  /* Kernel Debugger Print */
#define HBA_FLAGS_DBG_FW_PRINT_B         0x00000002  /* Firmware Debugger Print */
#define HBA_FLAGS_DBG_FUNCTION_ENTRY_B   0x00000004  /* Function Entry Point */
#define HBA_FLAGS_DBG_FUNCTION_EXIT_B    0x00000008  /* Function Exit */
#define HBA_FLAGS_DBG_ERROR_B            0x00000010  /* Error Conditions */
#define HBA_FLAGS_DBG_INIT_B             0x00000020  /* Init Prints */
#define HBA_FLAGS_DBG_OS_COMMANDS_B      0x00000040  /* OS Command Info */
#define HBA_FLAGS_DBG_SCAN_B             0x00000080  /* Device Scan */
#define HBA_FLAGS_DBG_COALESCE_B         0x00000100  /* Coalescing Queueing flags */
#define HBA_FLAGS_DBG_IOCTL_COMMANDS_B   0x00000200  /* IOCTL Command Info */
#define HBA_FLAGS_DBG_SYNC_COMMANDS_B    0x00000400  /* SYNC Command Info */
#define HBA_FLAGS_DBG_COMM_B             0x00000800  /* Comm Info */
#define HBA_FLAGS_DBG_CSMI_COMMANDS_B    0x00001000  /* CSMI Command Info */
#define HBA_FLAGS_DBG_AIF_B              0x00001000  /* Aif Info */

#define FW_DEBUG_STR_LENGTH_OFFSET       0x00
#define FW_DEBUG_FLAGS_OFFSET            0x04
#define FW_DEBUG_BLED_OFFSET             0x08
#define FW_DEBUG_FLAGS_NO_HEADERS_B      0x01 

int aac_get_fw_debug_buffer(struct aac_dev *);
void aac_fw_printf(struct aac_dev *, unsigned long, const char *, ...);
void aac_fw_print_mem(struct aac_dev *, unsigned long, u8 *, int);

#define	CT_GET_LOG_SIZE		189
struct aac_get_log_size {
	__le32	command;	/* VM_ContainerConfig & ST_OK response */
	__le32	type;		/* CT_GET_LOG_SIZE */
	__le32	index;
	__le32	size;
	__le32	count;
};

#define CT_GET_NVLOG_ENTRY	57
struct aac_get_nvlog_entry {
	__le32	command;	/* VM_ContainerConfig & ST_OK response */
	__le32	type;		/* CT_GET_NVLOG_ENTRY */
	__le32	status;		/* CT_OK response */
	__le32	index;
	__le32	count;
	__le32	parm3;
	__le32	parm4;
	__le32	parm5;
	u8	data[512-sizeof(__le32)*8-sizeof(struct aac_fibhdr)]; /* 448 */
};

#endif
