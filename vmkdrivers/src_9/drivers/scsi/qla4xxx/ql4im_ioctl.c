/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

/*
 * ioctl support functions
 */
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/klist.h>
#include <asm/semaphore.h>

#include "ql4_def.h"
#include "ql4im_def.h"
#include "ql4im_dump.h"
#include <scsi/scsi_dbg.h>

#ifdef __VMKLNX__
#include <vmklinux_scsi.h>  /* for vmklnx_get_vmhba_name */
#endif


struct ql4_ioctl_tbl {
	int cmd;
	char *s;
};

static struct ql4_ioctl_tbl QL4_IOCTL_CMD_TBL[] =
{
	{EXT_CC_QUERY, "EXT_CC_QUERY"},
	{EXT_CC_REG_AEN, "EXT_CC_REG_AEN"},
	{EXT_CC_GET_AEN, "EXT_CC_GET_AEN"},
	{EXT_CC_GET_DATA, "EXT_CC_GET_DATA"},
	{EXT_CC_SET_DATA, "EXT_CC_SET_DATA"},
	{EXT_CC_SEND_SCSI_PASSTHRU, "EXT_CC_SEND_SCSI_PASSTHRU"},
	{EXT_CC_SEND_ISCSI_PASSTHRU, "EXT_CC_SEND_ISCSI_PASSTHRU"},
	{EXT_CC_GET_HBACNT, "EXT_CC_GET_HBACNT"},
	{EXT_CC_GET_HOST_NO, "EXT_CC_GET_HOST_NO"},
	{EXT_CC_DRIVER_SPECIFIC, "EXT_CC_DRIVER_SPECIFIC"},
	{EXT_CC_GET_PORT_DEVICE_NAME, "EXT_CC_GET_PORT_DEVICE_NAME"},
	{INT_CC_LOGOUT_ISCSI, "INT_CC_LOGOUT_ISCSI"},
	{INT_CC_DIAG_PING, "INT_CC_DIAG_PING"},
	{INT_CC_GET_DATA, "INT_CC_GET_DATA"},
	{INT_CC_SET_DATA, "INT_CC_SET_DATA"},
	{INT_CC_HBA_RESET, "INT_CC_HBA_RESET"},
	{INT_CC_COPY_FW_FLASH, "INT_CC_COPY_FW_FLASH"},
	{INT_CC_IOCB_PASSTHRU, "INT_CC_IOCB_PASSTHRU"},
	{INT_CC_RESTORE_FACTORY_DEFAULTS, "INT_CC_RESTORE_FACTORY_DEFAULTS"},
	{INT_SC_GET_CORE_DUMP, "INT_SC_GET_CORE_DUMP"},
	{0, "UNKNOWN"},
};

static struct ql4_ioctl_tbl QL4_IOCTL_SCMD_QUERY_TBL[] =
{
	{EXT_SC_QUERY_HBA_ISCSI_NODE, "EXT_SC_QUERY_HBA_ISCSI_NODE"},
	{EXT_SC_QUERY_HBA_ISCSI_PORTAL, "EXT_SC_QUERY_HBA_ISCSI_PORTAL"},
	{EXT_SC_QUERY_DISC_ISCSI_NODE, "EXT_SC_QUERY_DISC_ISCSI_NODE"},
	{EXT_SC_QUERY_DISC_ISCSI_PORTAL, "EXT_SC_QUERY_DISC_ISCSI_PORTAL"},
	{EXT_SC_QUERY_DRIVER, "EXT_SC_QUERY_DRIVER"},
	{EXT_SC_QUERY_FW, "EXT_SC_QUERY_FW"},
	{EXT_SC_QUERY_CHIP, "EXT_SC_QUERY_CHIP"},
	{0, "UNKNOWN"},
};

static struct ql4_ioctl_tbl QL4_IOCTL_SCMD_EGET_DATA_TBL[] =
{
	{EXT_SC_GET_STATISTICS_ISCSI, "EXT_SC_GET_STATISTICS_ISCSI"},
	{EXT_SC_GET_DEVICE_ENTRY_ISCSI, "EXT_SC_GET_DEVICE_ENTRY_ISCSI"},
	{EXT_SC_GET_DEVICE_ENTRY_DEFAULTS_ISCSI, "EXT_SC_GET_DEVICE_ENTRY_DEFAULTS_ISCSI"},
	{EXT_SC_GET_INIT_FW_ISCSI, "EXT_SC_GET_INIT_FW_ISCSI"},
	{EXT_SC_GET_INIT_FW_DEFAULTS_ISCSI, "EXT_SC_GET_INIT_FW_DEFAULTS_ISCSI"},
	{EXT_SC_GET_ISNS_SERVER, "EXT_SC_GET_ISNS_SERVER"},
	{EXT_SC_GET_ISNS_DISCOVERED_TARGETS, "EXT_SC_GET_ISNS_DISCOVERED_TARGETS"},
	{0, "UNKNOWN"},
};

static struct ql4_ioctl_tbl QL4_IOCTL_SCMD_ESET_DATA_TBL[] =
{
	{EXT_SC_RST_STATISTICS_GEN, "EXT_SC_RST_STATISTICS_GEN"},
	{EXT_SC_RST_STATISTICS_ISCSI, "EXT_SC_RST_STATISTICS_ISCSI"},
	{EXT_SC_SET_DEVICE_ENTRY_ISCSI, "EXT_SC_SET_DEVICE_ENTRY_ISCSI"},
	{EXT_SC_SET_INIT_FW_ISCSI, "EXT_SC_SET_INIT_FW_ISCSI"},
	{EXT_SC_SET_ISNS_SERVER, "EXT_SC_SET_ISNS_SERVER"},
	{EXT_SC_SET_ACB, "EXT_SC_SET_ACB"},
	{0, "UNKNOWN"},
};

static struct ql4_ioctl_tbl QL4_IOCTL_SCMD_IGET_DATA_TBL[] =
{
	{INT_SC_GET_FLASH, "INT_SC_GET_FLASH"},
	{INT_SC_GET_HOST_NO, "INT_SC_GET_HOST_NO"},
	{0, "UNKNOWN"},
};

static struct ql4_ioctl_tbl QL4_IOCTL_SCMD_ISET_DATA_TBL[] =
{
	{INT_SC_SET_FLASH, "INT_SC_SET_FLASH"},
	{0, "UNKNOWN"},
};


static char *IOCTL_TBL_STR(int cc, int sc)
{
	struct ql4_ioctl_tbl *r;
	int cmd;

	switch (cc) {
	case EXT_CC_QUERY:
		r = QL4_IOCTL_SCMD_QUERY_TBL;
		cmd = sc;
		break;
	case EXT_CC_GET_DATA:
		r = QL4_IOCTL_SCMD_EGET_DATA_TBL;
		cmd = sc;
		break;
	case EXT_CC_SET_DATA:
		r = QL4_IOCTL_SCMD_ESET_DATA_TBL;
		cmd = sc;
		break;
	case INT_CC_GET_DATA:
		r = QL4_IOCTL_SCMD_IGET_DATA_TBL;
		cmd = sc;
		break;
	case INT_CC_SET_DATA:
		r = QL4_IOCTL_SCMD_ISET_DATA_TBL;
		cmd = sc;
		break;
	default:
		r = QL4_IOCTL_CMD_TBL;
		cmd = cc;
		break;
	}

	while (r->cmd != 0) {
		if (r->cmd == cmd) break;
		r++;
	}
	return(r->s);

}

static void *
Q64BIT_TO_PTR(uint64_t buf_addr, uint16_t addr_mode)
{
#if defined(CONFIG_COMPAT) || !defined(CONFIG_IA64) || !defined(CONFIG_64BIT)
	union ql_doublelong {
		struct {
			uint32_t        lsl;
			uint32_t        msl;
		} longs;
		uint64_t        dl;
	};

	union ql_doublelong tmpval;

	tmpval.dl = buf_addr;

#if defined(CONFIG_COMPAT) && !defined(CONFIG_IA64)
	/* 32bit user - 64bit kernel */
	if (addr_mode == EXT_DEF_ADDR_MODE_32) {
		DEBUG9(printk("%s: got 32bit user address.\n", __func__);)
		return((void *)(uint64_t)(tmpval.longs.lsl));
	} else {
		DEBUG9(printk("%s: got 64bit user address.\n", __func__);)
		return((void *)buf_addr);
	}
#else
	return((void *)(tmpval.longs.lsl));
#endif
#else
	return((void *)buf_addr);
#endif
}

/* Start of External ioctls */

static int ql4_reg_aen(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	EXT_REG_AEN_ISCSI reg_aen;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (ioctl->RequestLen > sizeof(EXT_REG_AEN_ISCSI)) {
		DEBUG2(printk("qisioctl%d: %s: memory area too small\n",
		    (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_reg_aen;
	}

	if ((status = copy_from_user((void *)&reg_aen,
	    Q64BIT_TO_PTR(ioctl->RequestAdr, ioctl->AddrMode),
				     sizeof(EXT_REG_AEN_ISCSI))) != 0) {
		DEBUG2(printk("qisioctl%d: %s: unable to copy data from "
		    "user's memory area\n", (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_reg_aen;
	}

	ql4im_ha->aen_reg_mask = reg_aen.Enable;

	DEBUG4(printk("qisioctl%d: %s: mask = 0x%x\n",
	    (int)ha->host_no, __func__, ql4im_ha->aen_reg_mask));

	ioctl->Status = EXT_STATUS_OK;

exit_reg_aen:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int get_rhel5_aen(struct hba_ioctl *ql4im_ha,
	EXT_ASYNC_EVENT *async_event)
{
	struct scsi_qla_host *ha;
	uint32_t num_aens = 0;
	uint16_t i, aen_in;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	aen_in = ha->aen_in;
	if (ql4im_ha->aen_read < aen_in) {
		for (i = ql4im_ha->aen_read;
			((i < aen_in)&&(num_aens < EXT_DEF_MAX_AEN_QUEUE));
			i++) {
			async_event[num_aens].AsyncEventCode =
				ha->aen_q[i].mbox_sts[0];
			async_event[num_aens].Payload[0] =
				ha->aen_q[i].mbox_sts[1];
			async_event[num_aens].Payload[1] =
				ha->aen_q[i].mbox_sts[2];
			async_event[num_aens].Payload[2] =
				ha->aen_q[i].mbox_sts[3];
			async_event[num_aens].Payload[3] =
				ha->aen_q[i].mbox_sts[4];
			num_aens++;
		}
	} else if (ql4im_ha->aen_read > aen_in) {
		for (i = ql4im_ha->aen_read;
			((i < MAX_AEN_ENTRIES)&&
				(num_aens < EXT_DEF_MAX_AEN_QUEUE));
			i++) {
			async_event[num_aens].AsyncEventCode =
				ha->aen_q[i].mbox_sts[0];
			async_event[num_aens].Payload[0] =
				ha->aen_q[i].mbox_sts[1];
			async_event[num_aens].Payload[1] =
				ha->aen_q[i].mbox_sts[2];
			async_event[num_aens].Payload[2] =
				ha->aen_q[i].mbox_sts[3];
			async_event[num_aens].Payload[3] =
				ha->aen_q[i].mbox_sts[4];
			num_aens++;
		}
		for (i = 0;
			((i < aen_in)&&
				(num_aens < EXT_DEF_MAX_AEN_QUEUE));
			i++) {
			async_event[num_aens].AsyncEventCode =
				ha->aen_q[i].mbox_sts[0];
			async_event[num_aens].Payload[0] =
				ha->aen_q[i].mbox_sts[1];
			async_event[num_aens].Payload[1] =
				ha->aen_q[i].mbox_sts[2];
			async_event[num_aens].Payload[2] =
				ha->aen_q[i].mbox_sts[3];
			async_event[num_aens].Payload[3] =
				ha->aen_q[i].mbox_sts[4];
			num_aens++;
		}
	}
	ql4im_ha->aen_read = aen_in;

	LEAVE_IOCTL(__func__, ha->host_no);

	return num_aens;
}

static int ql4_get_aen(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	EXT_ASYNC_EVENT *async_event;
	uint32_t num_aens = 0;
	uint16_t i, j;
	int status = 0;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	DEBUG4(printk("qisioctl%d: %s: mask = 0x%x\n",
		(int)ha->host_no, __func__, ql4im_ha->aen_reg_mask));

	if (ql4im_ha->aen_reg_mask == EXT_DEF_ENABLE_NO_AENS) {
		DEBUG2(printk("qisioctl%d: %s: AEN mask not enabled\n",
			(int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_OK;
		return 0;
	}
	if (ioctl->ResponseLen < 
		(sizeof(EXT_ASYNC_EVENT) * EXT_DEF_MAX_AEN_QUEUE)) {
		DEBUG2(printk("qisioctl%d: %s: memory area too small\n",
			(int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_get_gen;
	}
	async_event = (EXT_ASYNC_EVENT *)ql4im_ha->tmp_buf;
	memset(async_event, 0, sizeof(EXT_ASYNC_EVENT) * EXT_DEF_MAX_AEN_QUEUE);

	if ((drvr_major == 5)&&(drvr_minor == 0)){
		num_aens = get_rhel5_aen(ql4im_ha, async_event);
	} else {
		ha->ql4getaenlog(ha, &ql4im_ha->aen_log);
		if (ql4im_ha->aen_log.count) {
		 	for (i = 0; (i < ql4im_ha->aen_log.count); i++) {
				for (j = 0; j < MAX_AEN_ENTRIES; j++)
					async_event[num_aens].AsyncEventCode =
						ql4im_ha->aen_log.entry[i].mbox_sts[j];
				num_aens++;
			}
		}
	}

	ioctl->ResponseLen = sizeof(EXT_ASYNC_EVENT) * num_aens;
	ioctl->Status = EXT_STATUS_OK;

	/*
	 * Copy the IOCTL EXT_ASYNC_EVENT buffer to the user's data space
	 */
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), async_event,
				   ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl%d: %s: memory area too small\n",
		    (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}
exit_get_gen:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}



static int
ql4_query_hba_iscsi_node(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	EXT_HBA_ISCSI_NODE	*phba_node = NULL;
	struct init_fw_ctrl_blk	*init_fw_cb;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	phba_node = (EXT_HBA_ISCSI_NODE *)ql4im_ha->tmp_buf;

	if (!ioctl->ResponseAdr ||
		ioctl->ResponseLen < sizeof(EXT_HBA_ISCSI_NODE)) {
		DEBUG2(printk("qisioctl%lx: %s: rsp buffer too small\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_hba_node;
	}

	/*
	 * Send mailbox command
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_INIT_FW_CTRL_BLOCK;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = sizeof(struct init_fw_ctrl_blk);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) == QLA_ERROR) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];

		goto exit_query_hba_node;
	}

	/*
	 * Transfer data from Fw's DEV_DB_ENTRY buffer to IOCTL's
	 * EXT_HBA_ISCSI_NODE buffer
	 */
	init_fw_cb = (struct init_fw_ctrl_blk *) ql4im_ha->dma_v;

	memset(phba_node, 0, sizeof(EXT_HBA_ISCSI_NODE));
	phba_node->PortNumber = le16_to_cpu(init_fw_cb->pri.ipv4_port);
	phba_node->NodeInfo.PortalCount = 1;

	memcpy(phba_node->NodeInfo.IPAddr.IPAddress, init_fw_cb->pri.ipv4_addr,
	    sizeof(init_fw_cb->pri.ipv4_addr));
	memcpy(phba_node->NodeInfo.iSCSIName, init_fw_cb->pri.iscsi_name,
	    sizeof(init_fw_cb->pri.iscsi_name));

	sprintf(phba_node->DeviceName, "/proc/scsi/qla4xxx/%d",
	    (int)ha->host_no);

	/*
	 * Copy the IOCTL EXT_HBA_ISCSI_NODE buffer to the user's data space
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   phba_node, ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl%lx %s: copy_to_user failed\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_query_hba_node:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int
ql4_query_hba_iscsi_portal(struct hba_ioctl *ql4im_ha,
				    EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	EXT_HBA_ISCSI_PORTAL *phba_portal;
	struct flash_sys_info *sys_info;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr ||
		(ioctl->ResponseLen < sizeof(*phba_portal))) {
		DEBUG2(printk("qisioctl(%d): %s: no response buffer found.\n",
		    (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_hba_portal;
	}

	phba_portal = (EXT_HBA_ISCSI_PORTAL *)ql4im_ha->tmp_buf;
	memset(phba_portal, 0, sizeof(EXT_HBA_ISCSI_PORTAL));

	strcpy(phba_portal->DriverVersion, drvr_ver);
	sprintf(phba_portal->FWVersion, "%02d.%02d Patch %02d Build %02d",
		ha->firmware_version[0], ha->firmware_version[1],
		ha->patch_number, ha->build_number);

	/* Get firmware state information */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_FW_STATE;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 4, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: MBOX_CMD_GET_FW_STATE "
		    "failed w/ status %04x\n",
		    (int)ha->host_no, __func__, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_query_hba_portal;
	}

	switch (mbox_sts[1]) {
	case FW_STATE_READY:
		phba_portal->State = EXT_DEF_CARD_STATE_READY;
		break;
	case FW_STATE_CONFIG_WAIT:
		phba_portal->State = EXT_DEF_CARD_STATE_CONFIG_WAIT;
		break;
	case 0x0002: /*case FW_STATE_WAIT_LOGIN:*/
		phba_portal->State = EXT_DEF_CARD_STATE_LOGIN;
		break;
	case FW_STATE_ERROR:
		phba_portal->State = EXT_DEF_CARD_STATE_ERROR;
		break;
	}

	switch (mbox_sts[3] & 0x0001) {
	case 0:/* case FW_ADDSTATE_COPPER_MEDIA:*/
		phba_portal->Type = EXT_DEF_TYPE_COPPER;
		break;
	case FW_ADDSTATE_OPTICAL_MEDIA:
		phba_portal->Type = EXT_DEF_TYPE_OPTICAL;
		break;
	}

	/* Get ddb entry information */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY;
	mbox_cmd[1] = 0;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 7, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: GET_DATABASE_ENTRY failed!\n",
		    (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->RequestLen = 0;
		ioctl->DetailStatus = ioctl->Instance;

		goto exit_query_hba_portal;
	}

	phba_portal->DiscTargetCount = (uint16_t) mbox_sts[2];

	/* Get flash sys info information */
	sys_info = (struct flash_sys_info *) ql4im_ha->dma_v;

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_READ_FLASH;
	mbox_cmd[1] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[2] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = INT_ISCSI_SYSINFO_FLASH_OFFSET;
	mbox_cmd[4] = sizeof(*sys_info);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: MBOX_CMD_READ_FLASH failed w/"
		    " status %04X\n",
		    (int)ha->host_no, __func__, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];

		goto exit_query_hba_portal;
	}

	phba_portal->SerialNum = le32_to_cpu(sys_info->serialNumber);
	memcpy(phba_portal->IPAddr.IPAddress, ha->ip_address,
	    MIN(sizeof(phba_portal->IPAddr.IPAddress), sizeof(ha->ip_address)));
	memcpy(phba_portal->MacAddr, sys_info->physAddr[0].address,
	    sizeof(phba_portal->MacAddr));
	memcpy(phba_portal->Manufacturer, sys_info->vendorId,
	    sizeof(phba_portal->Manufacturer));
	memcpy(phba_portal->Model, sys_info->productId,
	    sizeof(phba_portal->Model));

	/*
	 * Copy the IOCTL EXT_HBA_ISCSI_PORTAL buffer to the user's data space
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   phba_portal, ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: copy_to_user failed\n",
		    (int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}
	DUMP_HBA_ISCSI_PORTAL(phba_portal);
exit_query_hba_portal:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_query_disc_iscsi_node(struct hba_ioctl *ql4im_ha,
				EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	struct dev_db_entry *fw_ddb_entry = (struct dev_db_entry *)ql4im_ha->dma_v;
	EXT_DISC_ISCSI_NODE *pdisc_node;
	struct ddb_entry *ddb_entry = NULL;
	struct scsi_qla_host *ha;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr ||
		(ioctl->ResponseLen < sizeof(*pdisc_node))) {
		DEBUG2(printk("qisioctl%lx: %s: no response buffer found.\n",
		    ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_disc_node;
	}

	pdisc_node = (EXT_DISC_ISCSI_NODE *)ql4im_ha->tmp_buf;
	
	/* get device database entry info from firmware */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY;
	mbox_cmd[1] = (uint32_t)ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 7, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: failed to get DEV_DB_ENTRY\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->RequestLen = 0;
		ioctl->DetailStatus = ioctl->Instance;
		goto exit_disc_node;
	}

	/* --- Transfer data from Fw's DEV_DB_ENTRY buffer to
	*      IOCTL's EXT_DISC_ISCSI_PORTAL buffer --- */
	memset(pdisc_node, 0, sizeof(EXT_DISC_ISCSI_NODE));
	pdisc_node->NodeInfo.PortalCount = 1;
	pdisc_node->NodeInfo.IPAddr.Type = EXT_DEF_TYPE_ISCSI_IP;
	memcpy(pdisc_node->NodeInfo.IPAddr.IPAddress, fw_ddb_entry->ip_addr,
	    MIN(sizeof(pdisc_node->NodeInfo.IPAddr.IPAddress),
	    sizeof(fw_ddb_entry->ip_addr)));
	strncpy(pdisc_node->NodeInfo.Alias, fw_ddb_entry->iscsi_alias,
	    MIN(sizeof(pdisc_node->NodeInfo.Alias),
	    sizeof(fw_ddb_entry->iscsi_alias)));
#if defined(__VMKLNX__)
	/* Make sure the buffer is null-terminated */
	if (MIN(sizeof(pdisc_node->NodeInfo.Alias),
	    sizeof(fw_ddb_entry->iscsi_alias)) > 0) {
		pdisc_node->NodeInfo.Alias[MIN(sizeof(pdisc_node->NodeInfo.Alias),
			sizeof(fw_ddb_entry->iscsi_alias)) - 1] = '\0';
	}
#endif
	strncpy(pdisc_node->NodeInfo.iSCSIName, fw_ddb_entry->iscsi_name,
	    MIN(sizeof(pdisc_node->NodeInfo.iSCSIName),
	    sizeof(fw_ddb_entry->iscsi_name)));

	if (ioctl->Instance < MAX_DDB_ENTRIES){
		ddb_entry = ha->fw_ddb_index_map[ioctl->Instance];
		if ((ddb_entry == NULL) ||
		   (ddb_entry == (struct ddb_entry *)INVALID_ENTRY))
			ddb_entry = NULL;
	}
	
	if (ddb_entry == NULL) {
		DEBUG2(printk("qisioctl%lx: %s: device index [%d] not logged in. "
		    "Dummy target info returned.\n",
		    ha->host_no, __func__, ioctl->Instance));

		pdisc_node->SessionID	    = 0xDEAD;
		pdisc_node->ConnectionID    = 0xDEAD;
		pdisc_node->PortalGroupID   = 0xDEAD;
		pdisc_node->ScsiAddr.Bus    = 0xFF;
		pdisc_node->ScsiAddr.Target = 0xFF;
		pdisc_node->ScsiAddr.Lun    = 0xFF;
	}
	else {
		pdisc_node->SessionID	    = ddb_entry->target_session_id;
		pdisc_node->ConnectionID    = ddb_entry->connection_id;
		pdisc_node->PortalGroupID   = 0;
		pdisc_node->ScsiAddr.Bus    = 0;
#ifndef __VMKLNX__
		pdisc_node->ScsiAddr.Target = ddb_entry->sess->target_id;
#else
		pdisc_node->ScsiAddr.Target = ddb_entry->sess->targetID;
#endif
		pdisc_node->ScsiAddr.Lun    = 0;
	}

	/* --- Copy Results to user space --- */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   pdisc_node,
				   sizeof(EXT_DISC_ISCSI_NODE))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: copy_to_user failed\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_disc_node:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_query_disc_iscsi_portal(struct hba_ioctl *ql4im_ha,
					EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	struct dev_db_entry *fw_ddb_entry;
	EXT_DISC_ISCSI_PORTAL *pdisc_portal;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr ||
		(ioctl->ResponseLen < sizeof(*pdisc_portal))) {
		DEBUG2(printk("qisioctl%lx: %s: no response buffer found.\n",
		    ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_disc_portal;
	}

	pdisc_portal = (EXT_DISC_ISCSI_PORTAL *)ql4im_ha->tmp_buf;

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	fw_ddb_entry = (struct dev_db_entry *) ql4im_ha->dma_v;

	mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY;
	mbox_cmd[1] = (uint32_t)ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 7, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: failed to get DEV_DB_ENTRY\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->RequestLen = 0;
		ioctl->DetailStatus = ioctl->Instance;
		goto exit_disc_portal;
	}

	memset(pdisc_portal, 0, sizeof(EXT_DISC_ISCSI_PORTAL));
	memcpy(pdisc_portal->IPAddr.IPAddress, fw_ddb_entry->ip_addr,
	    MIN(sizeof(pdisc_portal->IPAddr.IPAddress),
	    sizeof(fw_ddb_entry->ip_addr)));

	pdisc_portal->PortNumber = le16_to_cpu(fw_ddb_entry->port);
	pdisc_portal->IPAddr.Type = EXT_DEF_TYPE_ISCSI_IP;
	pdisc_portal->NodeCount = 0;

	strncpy(pdisc_portal->HostName, fw_ddb_entry->iscsi_name,
	    MIN(sizeof(pdisc_portal->HostName),
	    sizeof(fw_ddb_entry->iscsi_name)));
#if defined(__VMKLNX__)
	/* Make sure the buffer is null-terminated */
	if (MIN(sizeof(pdisc_portal->HostName),
	    sizeof(fw_ddb_entry->iscsi_name)) > 0) {
		pdisc_portal->HostName[MIN(sizeof(pdisc_portal->HostName),
			sizeof(fw_ddb_entry->iscsi_name)) - 1] = '\0';
	}
#endif

	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   pdisc_portal,
				   sizeof(EXT_DISC_ISCSI_PORTAL))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: copy_to_user failed\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}
exit_disc_portal:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_query_driver(struct hba_ioctl *ql4im_ha,
			EXT_IOCTL_ISCSI *ioctl)
{
	EXT_DRIVER_INFO *pdinfo;
	struct scsi_qla_host *ha;
	int status = 0;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || (ioctl->ResponseLen < sizeof(*pdinfo))) {
		DEBUG2(printk("qisioctl%lx: %s: no response buffer found.\n",
		    ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_driver;
	}

	pdinfo = (EXT_DRIVER_INFO *)ql4im_ha->tmp_buf;

	memset(pdinfo, 0, sizeof(EXT_DRIVER_INFO));
	strcpy(pdinfo->Version, drvr_ver);

	pdinfo->NumOfBus	= EXT_DEF_MAX_HBA;
	pdinfo->TargetsPerBus	= EXT_DEF_MAX_TARGET;
	pdinfo->LunPerTarget	= EXT_DEF_MAX_LUN;
	pdinfo->LunPerTargetOS	= EXT_DEF_MAX_BUS;

	if (sizeof(dma_addr_t) > 4)
		pdinfo->DmaBitAddresses = 1;  /* 64-bit */
	else
		pdinfo->DmaBitAddresses = 0;  /* 32-bit */

	pdinfo->IoMapType	= 1;

	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pdinfo,
				   sizeof(EXT_DRIVER_INFO))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: copy_to_user failed\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_query_driver:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_query_fw(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl)
{
	EXT_FW_INFO *pfw_info;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;
	int status = 0;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || (ioctl->ResponseLen < sizeof(*pfw_info))) {
		DEBUG2(printk("qisioctl%lx: %s: no response buffer found.\n",
		    ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_fw;
	}

	pfw_info = (EXT_FW_INFO *)ql4im_ha->tmp_buf;
	memset(pfw_info, 0, sizeof(EXT_FW_INFO));

	/* ----- Get firmware version information ---- */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_ABOUT_FW;

	/*
	 * NOTE: In QLA4010, mailboxes 2 & 3 may hold an address for data.
	 * Make sure that we write 0 to those mailboxes, if unused.
	 */
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: MBOX_CMD_ABOUT_FW failed w/ "
		    "status %04X\n", ha->host_no, __func__, mbox_sts[0]));
		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_query_fw;
	}

	sprintf(pfw_info->Version, "FW Version %d.%d Patch %d Build %d",
	    mbox_sts[1], mbox_sts[2], mbox_sts[3], mbox_sts[4]);

	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pfw_info,
				sizeof(EXT_FW_INFO))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: copy_to_user failed\n",
		    ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_query_fw:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_query_chip(struct hba_ioctl *ql4im_ha,
			EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_CHIP_INFO	*pchip_info;
	struct flash_sys_info *sys_info;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || 
		(ioctl->ResponseLen < sizeof(EXT_CHIP_INFO))) {
		DEBUG2(printk("qisioctl(%d): %s: no response buffer found.\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_chip;
	}

	pchip_info = (EXT_CHIP_INFO *)ql4im_ha->tmp_buf;
	memset(pchip_info, 0, sizeof(EXT_CHIP_INFO));

	sys_info = (struct flash_sys_info *) ql4im_ha->dma_v;

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_READ_FLASH;
	mbox_cmd[1] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[2] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = INT_ISCSI_SYSINFO_FLASH_OFFSET;
	mbox_cmd[4] = sizeof(*sys_info);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: MBOX_CMD_READ_FLASH failed w/"
		    " status %04X\n",
		    (int)ha->host_no, __func__, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];

		goto exit_query_chip;
	}

	pchip_info->VendorId	= le32_to_cpu(sys_info->pciDeviceVendor);
	pchip_info->DeviceId	= le32_to_cpu(sys_info->pciDeviceId);
	pchip_info->SubVendorId = le32_to_cpu(sys_info->pciSubsysVendor);
	pchip_info->SubSystemId = le32_to_cpu(sys_info->pciSubsysId);
	pchip_info->BoardID	= ha->board_id;

	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(
		Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode), 
		pchip_info, sizeof(EXT_CHIP_INFO))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: copy_to_user failed\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}
	DUMP_CHIP_INFO(pchip_info);

exit_query_chip:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_query_ipstate(struct hba_ioctl *ql4im_ha,
			EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_QUERY_IP_STATE *ip_state;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || 
		(ioctl->ResponseLen < sizeof(EXT_QUERY_IP_STATE))) {
		DEBUG2(printk("qisioctl(%d): %s: no response buffer found.\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_ipstate;
	}

	ip_state = (EXT_QUERY_IP_STATE *)ql4im_ha->tmp_buf;
	memset(ip_state, 0, sizeof(EXT_QUERY_IP_STATE));

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_IP_ADDR_STATE;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = ioctl->Reserved1;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 8, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: MBOX_CMD 0x91 failed w/"
		    " status 0x%04x\n",
		    (int)ha->host_no, __func__, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_query_ipstate;
	}

	memcpy(ip_state->IP_ACBState, &mbox_sts[1],
		MIN(sizeof(ip_state->IP_ACBState), sizeof(mbox_sts[1])));

	ip_state->ValidLifetime = le32_to_cpu(mbox_sts[2]);
	ip_state->PreferredLifetime = le32_to_cpu(mbox_sts[3]);

	memcpy(ip_state->IPAddressInfo1, &mbox_sts[4],
		MIN(sizeof(ip_state->IPAddressInfo1), sizeof(mbox_sts[4])));
	memcpy(ip_state->IPAddressInfo2, &mbox_sts[5],
		MIN(sizeof(ip_state->IPAddressInfo2), sizeof(mbox_sts[5])));
	memcpy(ip_state->IPAddressInfo3, &mbox_sts[6],
		MIN(sizeof(ip_state->IPAddressInfo3), sizeof(mbox_sts[6])));
	memcpy(ip_state->IPAddressInfo4, &mbox_sts[7],
		MIN(sizeof(ip_state->IPAddressInfo4), sizeof(mbox_sts[7])));

	ioctl->Status = EXT_STATUS_OK;

	if ((status = copy_to_user(
		Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode), 
		ip_state, sizeof(EXT_QUERY_IP_STATE))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: copy_to_user failed\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_query_ipstate:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_query_cur_ip(struct hba_ioctl *ql4im_ha,
			EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_QUERY_DEVICE_CURRENT_IP *ip;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || 
		(ioctl->ResponseLen < sizeof(EXT_QUERY_DEVICE_CURRENT_IP))) {
		DEBUG2(printk("qisioctl(%d): %s: no response buffer found.\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_query_ipstate;
	}

	ip = (EXT_QUERY_DEVICE_CURRENT_IP *)ql4im_ha->tmp_buf;
	memset(ip, 0, sizeof(EXT_QUERY_DEVICE_CURRENT_IP));

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_DB_ENTRY_CURRENT_IP_ADDR;
	mbox_cmd[1] = ioctl->Instance;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 8, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: MBOX_CMD 0x93 failed w/"
		    " status 0x%04x\n",
		    (int)ha->host_no, __func__, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];

		goto exit_query_ipstate;
	}

	memcpy(&ip->Addr.IPAddress[0], &mbox_sts[3], sizeof(mbox_sts[3]));
	memcpy(&ip->Addr.IPAddress[4], &mbox_sts[4], sizeof(mbox_sts[4]));
	memcpy(&ip->Addr.IPAddress[8], &mbox_sts[5], sizeof(mbox_sts[5]));
	memcpy(&ip->Addr.IPAddress[12], &mbox_sts[6], sizeof(mbox_sts[6]));

	if (mbox_sts[2] & 0x10) 
		ip->Addr.Type = EXT_DEF_TYPE_ISCSI_IPV6;

	ip->DeviceState = le16_to_cpu((mbox_sts[1] >> 16));
	ip->TCPPort 	= le16_to_cpu((mbox_sts[2] >> 16));
	memcpy(&ip->Flags[0], &mbox_sts[2], sizeof(ip->Flags));

	ioctl->Status = EXT_STATUS_OK;

	if ((status = copy_to_user(
		Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode), 
		ip, sizeof(EXT_QUERY_DEVICE_CURRENT_IP))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: copy_to_user failed\n",
		    (int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_query_ipstate:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_query(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;

	switch (ioctl->SubCode) {
	case EXT_SC_QUERY_HBA_ISCSI_NODE:
		status = ql4_query_hba_iscsi_node(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_HBA_ISCSI_PORTAL:
		status = ql4_query_hba_iscsi_portal(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_DISC_ISCSI_NODE:
		status = ql4_query_disc_iscsi_node(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_DISC_ISCSI_PORTAL:
		status = ql4_query_disc_iscsi_portal(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_DRIVER:
		status = ql4_query_driver(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_FW:
		status = ql4_query_fw(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_CHIP:
		status = ql4_query_chip(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_IP_STATE:
		status = ql4_query_ipstate(ql4im_ha, ioctl);
		break;
	case EXT_SC_QUERY_DEVICE_CURRENT_IP:
		status = ql4_query_cur_ip(ql4im_ha, ioctl);
		break;
	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported qry sub-code(%x)\n",
		    ql4im_ha->ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
	}
	return(status);
}

static int ql4_get_statistics_gen(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	EXT_HBA_PORT_STAT_GEN	*pstat_gen;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (ioctl->ResponseLen < sizeof(EXT_HBA_PORT_STAT_GEN)) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_get_stat_gen;
	}

	pstat_gen = (EXT_HBA_PORT_STAT_GEN *)ql4im_ha->tmp_buf;
	memset(pstat_gen, 0, sizeof(EXT_HBA_PORT_STAT_GEN));

	pstat_gen->HBAPortErrorCount	 = ha->adapter_error_count;
	pstat_gen->DevicePortErrorCount  = ha->device_error_count;
	pstat_gen->IoCount		 = ha->total_io_count;
	pstat_gen->MBytesCount		 = ha->total_mbytes_xferred;
	pstat_gen->InterruptCount	 = ha->isr_count;
	pstat_gen->LinkFailureCount	 = ha->link_failure_count;
	pstat_gen->InvalidCrcCount	 = ha->invalid_crc_count;

	ioctl->Status = EXT_STATUS_OK;

	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pstat_gen,
				   ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_stat_gen:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_statistics_iscsi(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	EXT_HBA_PORT_STAT_ISCSI *pstat_local;
	EXT_HBA_PORT_STAT_ISCSI *pstat_user;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_HBA_PORT_STAT_ISCSI))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_stats_iscsi;
	}

	pstat_user = (EXT_HBA_PORT_STAT_ISCSI *)ql4im_ha->tmp_buf;

	/*
	 * Make the mailbox call
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_MANAGEMENT_DATA; 
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: get mngmt data for index [%d] failed "
		    "w/ mailbox ststus 0x%x\n",
		    ha->host_no, __func__, ioctl->Instance, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_stats_iscsi;
	}

	pstat_local = (EXT_HBA_PORT_STAT_ISCSI *) ql4im_ha->dma_v;
	memset(pstat_user, 0, sizeof(EXT_HBA_PORT_STAT_ISCSI));
	pstat_user->MACTxFramesCount	      =
	    le64_to_cpu(pstat_local->MACTxFramesCount);
	pstat_user->MACTxBytesCount	      =
	    le64_to_cpu(pstat_local->MACTxBytesCount);
	pstat_user->MACRxFramesCount	      =
	    le64_to_cpu(pstat_local->MACRxFramesCount);
	pstat_user->MACRxBytesCount	      =
	    le64_to_cpu(pstat_local->MACRxBytesCount);
	pstat_user->MACCRCErrorCount	      =
	    le64_to_cpu(pstat_local->MACCRCErrorCount);
	pstat_user->MACEncodingErrorCount     =
	    le64_to_cpu(pstat_local->MACEncodingErrorCount);
	pstat_user->IPTxPacketsCount	      =
	    le64_to_cpu(pstat_local->IPTxPacketsCount);
	pstat_user->IPTxBytesCount	      =
	    le64_to_cpu(pstat_local->IPTxBytesCount);
	pstat_user->IPTxFragmentsCount	      =
	    le64_to_cpu(pstat_local->IPTxFragmentsCount);
	pstat_user->IPRxPacketsCount	      =
	    le64_to_cpu(pstat_local->IPRxPacketsCount);
	pstat_user->IPRxBytesCount	      =
	    le64_to_cpu(pstat_local->IPRxBytesCount);
	pstat_user->IPRxFragmentsCount	      =
	    le64_to_cpu(pstat_local->IPRxFragmentsCount);
	pstat_user->IPDatagramReassemblyCount =
	    le64_to_cpu(pstat_local->IPDatagramReassemblyCount);
	pstat_user->IPv6RxPacketsCount	      =
	    le64_to_cpu(pstat_local->IPv6RxPacketsCount);
	pstat_user->IPRxPacketErrorCount      =
	    le64_to_cpu(pstat_local->IPRxPacketErrorCount);
	pstat_user->IPReassemblyErrorCount    =
	    le64_to_cpu(pstat_local->IPReassemblyErrorCount);
	pstat_user->TCPTxSegmentsCount	      =
	    le64_to_cpu(pstat_local->TCPTxSegmentsCount);
	pstat_user->TCPTxBytesCount	      =
	    le64_to_cpu(pstat_local->TCPTxBytesCount);
	pstat_user->TCPRxSegmentsCount	      =
	    le64_to_cpu(pstat_local->TCPRxSegmentsCount);
	pstat_user->TCPRxBytesCount	      =
	    le64_to_cpu(pstat_local->TCPRxBytesCount);
	pstat_user->TCPTimerExpiredCount      =
	    le64_to_cpu(pstat_local->TCPTimerExpiredCount);
	pstat_user->TCPRxACKCount	      =
	    le64_to_cpu(pstat_local->TCPRxACKCount);
	pstat_user->TCPTxACKCount	      =
	    le64_to_cpu(pstat_local->TCPTxACKCount);
	pstat_user->TCPRxErrorSegmentCount    =
	    le64_to_cpu(pstat_local->TCPRxErrorSegmentCount);
	pstat_user->TCPWindowProbeUpdateCount =
	    le64_to_cpu(pstat_local->TCPWindowProbeUpdateCount);
	pstat_user->iSCSITxPDUCount	      =
	    le64_to_cpu(pstat_local->iSCSITxPDUCount);
	pstat_user->iSCSITxBytesCount	      =
	    le64_to_cpu(pstat_local->iSCSITxBytesCount);
	pstat_user->iSCSIRxPDUCount	      =
	    le64_to_cpu(pstat_local->iSCSIRxPDUCount);
	pstat_user->iSCSIRxBytesCount	      =
	    le64_to_cpu(pstat_local->iSCSIRxBytesCount);
	pstat_user->iSCSICompleteIOsCount     =
	    le64_to_cpu(pstat_local->iSCSICompleteIOsCount);
	pstat_user->iSCSIUnexpectedIORxCount  =
	    le64_to_cpu(pstat_local->iSCSIUnexpectedIORxCount);
	pstat_user->iSCSIFormatErrorCount     =
	    le64_to_cpu(pstat_local->iSCSIFormatErrorCount);
	pstat_user->iSCSIHeaderDigestCount    =
	    le64_to_cpu(pstat_local->iSCSIHeaderDigestCount);
	pstat_user->iSCSIDataDigestErrorCount =
	    le64_to_cpu(pstat_local->iSCSIDataDigestErrorCount);
	pstat_user->iSCSISeqErrorCount	      =
	    le64_to_cpu(pstat_local->iSCSISeqErrorCount);

	/*
	 * Copy the data from the dma buffer to the user's data space
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   pstat_user, ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_stats_iscsi:

	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_get_device_entry_iscsi(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	struct dev_db_entry	*pfw_ddb_entry;
	EXT_DEVICE_ENTRY_ISCSI	*pdev_entry;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_DEVICE_ENTRY_ISCSI))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_dev_entry;
	}

	/*
	 * Make the mailbox call
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	if (ioctl->SubCode == EXT_SC_GET_DEVICE_ENTRY_ISCSI)
		mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY;
	else
		mbox_cmd[0] = MBOX_CMD_GET_DATABASE_ENTRY_DEFAULTS;

	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: get ddb entry for index [%d] failed "
		    "w/ mailbox ststus 0x%x\n",
		    ha->host_no, __func__, ioctl->Instance, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_dev_entry;
	}

	pdev_entry = (EXT_DEVICE_ENTRY_ISCSI *)ql4im_ha->tmp_buf;
	memset(pdev_entry, 0, sizeof(EXT_DEVICE_ENTRY_ISCSI));
	/*
	 * Transfer data from Fw's DEV_DB_ENTRY buffer to IOCTL's
	 * EXT_DEVICE_ENTRY_ISCSI buffer
	 */
	pfw_ddb_entry = (struct dev_db_entry *) ql4im_ha->dma_v;

	pdev_entry->NumValid	 = mbox_sts[2];
	pdev_entry->NextValid	 = mbox_sts[3];
	pdev_entry->DeviceState  = mbox_sts[4];
	pdev_entry->Options	 = pfw_ddb_entry->options;
	pdev_entry->TargetSessID = le16_to_cpu(pfw_ddb_entry->tsid);
	memcpy(pdev_entry->InitiatorSessID, pfw_ddb_entry->isid,
	    sizeof(pfw_ddb_entry->isid));

	pdev_entry->DeviceInfo.DeviceType = le16_to_cpu(EXT_DEF_ISCSI_REMOTE);
	pdev_entry->DeviceInfo.ExeThrottle =
	    le16_to_cpu(pfw_ddb_entry->exec_throttle);
	pdev_entry->DeviceInfo.InitMarkerlessInt =
	    le16_to_cpu(pfw_ddb_entry->iscsi_max_snd_data_seg_len);
	pdev_entry->DeviceInfo.iSCSIOptions =
	    le16_to_cpu(pfw_ddb_entry->iscsi_options);
	pdev_entry->DeviceInfo.TCPOptions =
	    le16_to_cpu(pfw_ddb_entry->tcp_options);
	pdev_entry->DeviceInfo.IPOptions =
	    le16_to_cpu(pfw_ddb_entry->ip_options);
	pdev_entry->DeviceInfo.MaxPDUSize =
	    le16_to_cpu(pfw_ddb_entry->iscsi_max_rcv_data_seg_len);
	pdev_entry->DeviceInfo.FirstBurstSize =
	    le16_to_cpu(pfw_ddb_entry->iscsi_first_burst_len);
	pdev_entry->DeviceInfo.LogoutMinTime =
	    le16_to_cpu(pfw_ddb_entry->iscsi_def_time2wait);
	pdev_entry->DeviceInfo.LogoutMaxTime =
	    le16_to_cpu(pfw_ddb_entry->iscsi_def_time2retain);
	pdev_entry->DeviceInfo.MaxOutstandingR2T =
	    le16_to_cpu(pfw_ddb_entry->iscsi_max_outsnd_r2t);
	pdev_entry->DeviceInfo.KeepAliveTimeout =
	    le16_to_cpu(pfw_ddb_entry->ka_timeout);
	pdev_entry->DeviceInfo.PortNumber =
	    le16_to_cpu(pfw_ddb_entry->port);
	pdev_entry->DeviceInfo.MaxBurstSize =
	    le16_to_cpu(pfw_ddb_entry->iscsi_max_burst_len);
	pdev_entry->DeviceInfo.TaskMgmtTimeout =
	    le16_to_cpu(pfw_ddb_entry->def_timeout);
	pdev_entry->EntryInfo.PortalCount = mbox_sts[2];
	pdev_entry->ExeCount = le16_to_cpu(pfw_ddb_entry->exec_count);
	pdev_entry->DDBLink = le16_to_cpu(pfw_ddb_entry->ddb_link);

	memcpy(pdev_entry->DeviceInfo.TargetAddr, pfw_ddb_entry->tgt_addr,
	    MIN(sizeof(pdev_entry->DeviceInfo.TargetAddr), 
		sizeof(pfw_ddb_entry->tgt_addr)));
	memcpy(pdev_entry->EntryInfo.IPAddr.IPAddress, pfw_ddb_entry->ip_addr,
	    MIN(sizeof(pdev_entry->EntryInfo.IPAddr.IPAddress),
		sizeof(pfw_ddb_entry->ip_addr)));
	memcpy(pdev_entry->EntryInfo.iSCSIName, pfw_ddb_entry->iscsi_name,
	    MIN(sizeof(pdev_entry->EntryInfo.iSCSIName),
		sizeof(pfw_ddb_entry->iscsi_name)));
	memcpy(pdev_entry->EntryInfo.Alias, pfw_ddb_entry->iscsi_alias,
	    MIN(sizeof(pdev_entry->EntryInfo.Alias),
		sizeof(pfw_ddb_entry->iscsi_alias)));

	/*
	 * Copy the IOCTL EXT_DEVICE_ENTRY_ISCSI buffer to the user's data space
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
				   pdev_entry, ioctl->ResponseLen)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_dev_entry:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}


static int ql4_get_init_fw_iscsi(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_INIT_FW_ISCSI *pinit_fw;
	struct addr_ctrl_blk *acb;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_DEVICE_ENTRY_ISCSI))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_init_fw;
	}

	/*
	 * Send mailbox command
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	switch (ioctl->SubCode) {
	case EXT_SC_GET_INIT_FW_ISCSI:
		mbox_cmd[0] = MBOX_CMD_GET_INIT_FW_CTRL_BLOCK;
		break;
	case EXT_SC_GET_INIT_FW_DEFAULTS_ISCSI:
		mbox_cmd[0] = MBOX_CMD_GET_INIT_FW_CTRL_BLOCK_DEFAULTS;
		break;
	default:
		DEBUG2(printk("qisioctl%lx: %s: invalid subcode (0x%04X) speficied\n",
			ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_init_fw;
	}

	mbox_cmd[1] = 0;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = sizeof(struct init_fw_ctrl_blk);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_init_fw;
	}

	pinit_fw = (EXT_INIT_FW_ISCSI *)ql4im_ha->tmp_buf;
	memset(pinit_fw, 0, sizeof(EXT_INIT_FW_ISCSI));
	/*
	 * Transfer Data from DMA buffer to Local buffer
	 */
	acb = (struct addr_ctrl_blk *)ql4im_ha->dma_v;

	pinit_fw->Version	  = acb->version;
	pinit_fw->FWOptions	  = le16_to_cpu(acb->fw_options);
	pinit_fw->AddFWOptions	  = le16_to_cpu(acb->add_fw_options);
	//FIXME: pinit_fw->WakeupThreshold = le16_to_cpu(acb->WakeupThreshold);
	memcpy(&pinit_fw->IPAddr.IPAddress, &acb->ipv4_addr,
	    MIN(sizeof(pinit_fw->IPAddr.IPAddress),
	    sizeof(acb->ipv4_addr)));
	memcpy(&pinit_fw->SubnetMask.IPAddress, &acb->ipv4_subnet,
	    MIN(sizeof(pinit_fw->SubnetMask.IPAddress),
	    sizeof(acb->ipv4_subnet)));
	memcpy(&pinit_fw->Gateway.IPAddress, &acb->ipv4_gw_addr,
	    MIN(sizeof(pinit_fw->Gateway.IPAddress),
	    sizeof(acb->ipv4_gw_addr)));
	memcpy(&pinit_fw->DNSConfig.IPAddr.IPAddress,
	    &acb->pri_dns_srvr_ip,
	    MIN(sizeof(pinit_fw->DNSConfig.IPAddr.IPAddress),
	    sizeof(acb->pri_dns_srvr_ip)));
	memcpy(&pinit_fw->Alias, &acb->iscsi_alias,
	    MIN(sizeof(pinit_fw->Alias), sizeof(acb->iscsi_alias)));
	memcpy(&pinit_fw->iSCSIName, &acb->iscsi_name,
	    MIN(sizeof(pinit_fw->iSCSIName),
	    sizeof(acb->iscsi_name)));

	pinit_fw->DeviceInfo.DeviceType = le16_to_cpu(EXT_DEF_ISCSI_LOCAL);
	pinit_fw->DeviceInfo.ExeThrottle =
	    le16_to_cpu(acb->exec_throttle);
	pinit_fw->DeviceInfo.iSCSIOptions =
	    le16_to_cpu(acb->iscsi_opts);
	pinit_fw->DeviceInfo.TCPOptions = le16_to_cpu(acb->ipv4_tcp_opts);
	pinit_fw->DeviceInfo.IPOptions = le16_to_cpu(acb->ipv4_ip_opts);
	pinit_fw->DeviceInfo.MaxPDUSize = le16_to_cpu(acb->iscsi_max_pdu_size);
	pinit_fw->DeviceInfo.FirstBurstSize =
	    le16_to_cpu(acb->iscsi_fburst_len);
	pinit_fw->DeviceInfo.LogoutMinTime =
	    le16_to_cpu(acb->iscsi_def_time2wait);
	pinit_fw->DeviceInfo.LogoutMaxTime =
	    le16_to_cpu(acb->iscsi_def_time2retain);
	pinit_fw->DeviceInfo.LogoutMaxTime =
	    le16_to_cpu(acb->iscsi_def_time2retain);
	pinit_fw->DeviceInfo.MaxOutstandingR2T =
	    le16_to_cpu(acb->iscsi_max_outstnd_r2t);
	pinit_fw->DeviceInfo.KeepAliveTimeout =
	    le16_to_cpu(acb->conn_ka_timeout);
	pinit_fw->DeviceInfo.PortNumber = le16_to_cpu(acb->ipv4_port);
	pinit_fw->DeviceInfo.MaxBurstSize =
	    le16_to_cpu(acb->iscsi_max_burst_len);

	/*
	 * Copy the local data to the user's buffer
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pinit_fw,
				   sizeof(EXT_INIT_FW_ISCSI))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_init_fw:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_acb(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_ACB		*pacb;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);
	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_ACB))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_acb;
	}

	pacb = (EXT_ACB *)ql4im_ha->dma_v;
	memset(pacb, 0, sizeof(EXT_ACB));

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_ACB;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = ioctl->ResponseLen;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_acb;
	}

	/*
	 * Copy the local data to the user's buffer
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pacb,
				   sizeof(EXT_ACB))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_acb:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_cache(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl, uint32_t cache_cmd)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	EXT_NEIGHBOR_CACHE	cache;
	EXT_NEIGHBOR_CACHE	*pcache, *pu_cache;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);
	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen > QL_DMA_BUF_SIZE)) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_cache;
	}

	memset(&cache, 0, sizeof(EXT_NEIGHBOR_CACHE));

	/*
	 * Copy the IOCTL EXT_DEVICE_ENTRY_ISCSI buffer from the user's
	 * data space
	 */
	if ((status = copy_from_user((uint8_t *)&cache,
				     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     ioctl->RequestLen)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from user's "
		    "memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_cache;
	}

	/*
	 * Transfer data from IOCTL's EXT_DEVICE_ENTRY_ISCSI buffer to
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = cache_cmd;
		
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = ioctl->Reserved1;
	mbox_cmd[5] = cache.CacheBufferSize;

	pcache = ql4im_ha->dma_v;
	memset(pcache, 0, cache.CacheBufferSize);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 7, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_cache;
	}

	ioctl->VendorSpecificStatus[0] = mbox_sts[0];
	ioctl->VendorSpecificStatus[1] = mbox_sts[1];
	ioctl->VendorSpecificStatus[2] = mbox_sts[2];
	ioctl->VendorSpecificStatus[3] = mbox_sts[3];
	ioctl->VendorSpecificStatus[4] = mbox_sts[4];
	ioctl->VendorSpecificStatus[5] = mbox_sts[5];
	ioctl->VendorSpecificStatus[6] = mbox_sts[6];

	/*
	 * Copy the local data to the user's buffer
	 */
	pu_cache = Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode);
	/*
	 * Copy local DMA buffer to user's response data area
	 */
	if ((status = copy_to_user(pu_cache,
                                   &cache,
				   sizeof(EXT_NEIGHBOR_CACHE))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: FlashData to user failed\n",
			(int)ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_cache;
	}

	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(&pu_cache->Buffer[0], pcache,
				   cache.CacheBufferSize)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_cache;
	}

exit_get_cache:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_isns_server(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	EXT_ISNS_SERVER		isns;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_ISNS_SERVER))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_isns;
	}

	memset(&isns, 0, sizeof(EXT_ISNS_SERVER));
	isns.iSNSNotSupported = 1;

	/*
	 * Copy the local data to the user's buffer
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), &isns,
				   sizeof(EXT_ISNS_SERVER))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_isns:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_stat_iscsi_block(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	EXT_HBA_PORT_STAT_ISCSI_BLOCK *pstat;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);
	if (!ioctl->ResponseAdr || !ioctl->ResponseLen ||
		(ioctl->ResponseLen < sizeof(EXT_HBA_PORT_STAT_ISCSI_BLOCK))) {
		DEBUG2(printk("qisioctl%lx: %s: invalid parameter\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_stat_iscsi_block;
	}

	pstat = (EXT_HBA_PORT_STAT_ISCSI_BLOCK *)ql4im_ha->dma_v;
	memset(pstat, 0, sizeof(EXT_HBA_PORT_STAT_ISCSI_BLOCK));

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_GET_MANAGEMENT_DATA;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 5, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_get_stat_iscsi_block;
	}

	/*
	 * Copy the local data to the user's buffer
	 */
	ioctl->Status = EXT_STATUS_OK;
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode), pstat,
			   sizeof(EXT_HBA_PORT_STAT_ISCSI_BLOCK))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_COPY_ERR;
	}

exit_get_stat_iscsi_block:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_ext_get_data(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	struct scsi_qla_host *ha;
	int status;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	switch (ioctl->SubCode) {
	case EXT_SC_GET_STATISTICS_GEN:
		status = ql4_get_statistics_gen(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_STATISTICS_ISCSI:
		status = ql4_get_statistics_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_DEVICE_ENTRY_ISCSI:
	case EXT_SC_GET_DEVICE_ENTRY_DEFAULTS_ISCSI:
		status = ql4_get_device_entry_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_INIT_FW_ISCSI:
	case EXT_SC_GET_INIT_FW_DEFAULTS_ISCSI:
		status = ql4_get_init_fw_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_ACB:
		status = ql4_get_acb(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_NEIGHBOR_CACHE:
		status = ql4_get_cache(ql4im_ha, ioctl,
				MBOX_CMD_GET_IPV6_NEIGHBOR_CACHE);
		break;
	case EXT_SC_GET_DESTINATION_CACHE:
		status = ql4_get_cache(ql4im_ha, ioctl,
				MBOX_CMD_GET_IPV6_DEST_CACHE);
		break;
	case EXT_SC_GET_DEFAULT_ROUTER_LIST:
		status = ql4_get_cache(ql4im_ha, ioctl,
				MBOX_CMD_GET_IPV6_DEF_ROUTER_LIST);
		break;
	case EXT_SC_GET_LOCAL_PREFIX_LIST:
		status = ql4_get_cache(ql4im_ha, ioctl,
				MBOX_CMD_GET_IPV6_LCL_PREFIX_LIST);
		break;
	case EXT_SC_GET_ISNS_SERVER:
		status = ql4_get_isns_server(ql4im_ha, ioctl);
		break;
	case EXT_SC_GET_STATISTICS_ISCSI_BLOCK:
		status = ql4_get_stat_iscsi_block(ql4im_ha, ioctl);
		break;

	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported external get "
		    "data sub-command code (%X)\n",
		    ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	LEAVE_IOCTL(__func__, ha->host_no);
	return(0);
}

static int ql4_rst_statistics_gen(struct hba_ioctl *ql4im_ha,
				EXT_IOCTL_ISCSI *ioctl)
{
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);
	/*
	 * Reset the general statistics fields
	 */
	ha->adapter_error_count = 0;
	ha->device_error_count = 0;
	ha->total_io_count = 0;
	ha->total_mbytes_xferred = 0;
	ha->isr_count = 0; 
	ha->link_failure_count = 0;
	ha->invalid_crc_count = 0;

	ioctl->Status = EXT_STATUS_OK;

	LEAVE_IOCTL(__func__, ha->host_no);
	return(QLA_SUCCESS);
}

static int ql4_rst_statistics_iscsi(struct hba_ioctl *ql4im_ha,
				EXT_IOCTL_ISCSI *ioctl)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	/*
	 * Make the mailbox call
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_MANAGEMENT_DATA;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = 0;
	mbox_cmd[3] = 0;

	ioctl->Status = EXT_STATUS_OK;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: get mngmt data for index [%d] failed! "
		    "w/ mailbox ststus 0x%x\n",
		    ha->host_no, __func__, ioctl->Instance, mbox_sts[0]));

		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
	}

	LEAVE_IOCTL(__func__, ha->host_no);
	return(QLA_SUCCESS);
}

static int ql4_set_device_entry_iscsi(struct hba_ioctl *ql4im_ha,
				    EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	struct dev_db_entry *pfw_ddb_entry;
	EXT_DEVICE_ENTRY_ISCSI *pdev_entry;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if ((ioctl->RequestLen < sizeof(EXT_DEVICE_ENTRY_ISCSI)) || 
		!ioctl->RequestAdr){
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_set_dev_entry;
	}

	pdev_entry = (EXT_DEVICE_ENTRY_ISCSI *)ql4im_ha->tmp_buf;
	memset(pdev_entry, 0, sizeof(EXT_DEVICE_ENTRY_ISCSI));

	/*
	 * Copy the IOCTL EXT_DEVICE_ENTRY_ISCSI buffer from the user's
	 * data space
	 */
	if ((status = copy_from_user((uint8_t *)pdev_entry,
				     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     ioctl->RequestLen)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from user's "
		    "memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_dev_entry;
	}

	/*
	 * Transfer data from IOCTL's EXT_DEVICE_ENTRY_ISCSI buffer to
	 * Fw's DEV_DB_ENTRY buffer
	 */
	pfw_ddb_entry = (struct dev_db_entry *)ql4im_ha->dma_v;
	memset(pfw_ddb_entry, 0, sizeof(struct dev_db_entry));

	pfw_ddb_entry->options		= 
		cpu_to_le16(pdev_entry->Control << 8 | pdev_entry->Options);
	pfw_ddb_entry->tsid		= cpu_to_le16(pdev_entry->TargetSessID);
	pfw_ddb_entry->exec_count 	= cpu_to_le16(pdev_entry->ExeCount);
	pfw_ddb_entry->ddb_link		= cpu_to_le16(pdev_entry->DDBLink);
	memcpy(pfw_ddb_entry->isid, pdev_entry->InitiatorSessID,
	    sizeof(pdev_entry->InitiatorSessID));

	pfw_ddb_entry->exec_throttle =
	    cpu_to_le16(pdev_entry->DeviceInfo.ExeThrottle);
	pfw_ddb_entry->iscsi_max_snd_data_seg_len =
	    cpu_to_le16(pdev_entry->DeviceInfo.InitMarkerlessInt);
	pfw_ddb_entry->iscsi_options =
	    cpu_to_le16(pdev_entry->DeviceInfo.iSCSIOptions);
	pfw_ddb_entry->tcp_options =
	    cpu_to_le16(pdev_entry->DeviceInfo.TCPOptions);
	pfw_ddb_entry->ip_options =
	    cpu_to_le16(pdev_entry->DeviceInfo.IPOptions);
	pfw_ddb_entry->iscsi_max_rcv_data_seg_len =
	    cpu_to_le16(pdev_entry->DeviceInfo.MaxPDUSize);
	pfw_ddb_entry->iscsi_first_burst_len =
	    cpu_to_le16(pdev_entry->DeviceInfo.FirstBurstSize);
	pfw_ddb_entry->iscsi_def_time2wait =
	    cpu_to_le16(pdev_entry->DeviceInfo.LogoutMinTime);
	pfw_ddb_entry->iscsi_def_time2retain =
	    cpu_to_le16(pdev_entry->DeviceInfo.LogoutMaxTime);
	pfw_ddb_entry->iscsi_max_outsnd_r2t =
	    cpu_to_le16(pdev_entry->DeviceInfo.MaxOutstandingR2T);
	pfw_ddb_entry->ka_timeout =
	    cpu_to_le16(pdev_entry->DeviceInfo.KeepAliveTimeout);
	pfw_ddb_entry->port =
	    cpu_to_le16(pdev_entry->DeviceInfo.PortNumber);
	pfw_ddb_entry->iscsi_max_burst_len =
	    cpu_to_le16(pdev_entry->DeviceInfo.MaxBurstSize);
	pfw_ddb_entry->def_timeout =
	    cpu_to_le16(pdev_entry->DeviceInfo.TaskMgmtTimeout);
#if defined(__VMKLNX__)
	/* Make sure the size of the copy doesn't excceed the size of
	 * the destination buffer */
	memcpy(pfw_ddb_entry->tgt_addr, pdev_entry->DeviceInfo.TargetAddr,
	    MIN(sizeof(pfw_ddb_entry->tgt_addr),
	        sizeof(pdev_entry->DeviceInfo.TargetAddr)));

	memcpy(pfw_ddb_entry->ip_addr, pdev_entry->EntryInfo.IPAddr.IPAddress,
	    MIN(sizeof(pfw_ddb_entry->ip_addr),
	        sizeof(pdev_entry->EntryInfo.IPAddr.IPAddress)));
	memcpy(pfw_ddb_entry->iscsi_name, pdev_entry->EntryInfo.iSCSIName,
	    MIN(sizeof(pfw_ddb_entry->iscsi_name),
	        sizeof(pdev_entry->EntryInfo.iSCSIName)));
	memcpy(pfw_ddb_entry->iscsi_alias, pdev_entry->EntryInfo.Alias,
	    MIN(sizeof(pfw_ddb_entry->iscsi_alias),
	        sizeof(pdev_entry->EntryInfo.Alias)));
#else
	memcpy(pfw_ddb_entry->tgt_addr, pdev_entry->DeviceInfo.TargetAddr,
	    sizeof(pdev_entry->DeviceInfo.TargetAddr));

	memcpy(pfw_ddb_entry->ip_addr, pdev_entry->EntryInfo.IPAddr.IPAddress,
	    sizeof(pdev_entry->EntryInfo.IPAddr.IPAddress));
	memcpy(pfw_ddb_entry->iscsi_name, pdev_entry->EntryInfo.iSCSIName,
	    sizeof(pdev_entry->EntryInfo.iSCSIName));
	memcpy(pfw_ddb_entry->iscsi_alias, pdev_entry->EntryInfo.Alias,
	    sizeof(pdev_entry->EntryInfo.Alias));
#endif

	/*
	 * Make the IOCTL call
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_SET_DATABASE_ENTRY;
	mbox_cmd[1] = (uint32_t) ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);

	ioctl->Status = EXT_STATUS_OK;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: SET DDB Entry failed\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
	}

exit_set_dev_entry:

	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int
ql4_set_init_fw_iscsi(struct hba_ioctl *ql4im_ha,
				EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	EXT_INIT_FW_ISCSI *pinit_fw;
	struct init_fw_ctrl_blk *pinit_fw_cb;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if ((ioctl->RequestLen < sizeof(EXT_INIT_FW_ISCSI)) || 
		!ioctl->RequestAdr){
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_set_init_fw;
	}

	pinit_fw = (EXT_INIT_FW_ISCSI *)ql4im_ha->tmp_buf;
	memset(pinit_fw, 0, sizeof(EXT_INIT_FW_ISCSI));

	/*
	 * Copy the data from the user's buffer
	 */
	if ((status = copy_from_user((uint8_t *)pinit_fw,
				     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(EXT_INIT_FW_ISCSI))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_init_fw;
	}

	/*
	 * First get Initialize Firmware Control Block, so as not to
	 * destroy unaffected data
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_GET_INIT_FW_CTRL_BLOCK;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = sizeof(struct init_fw_ctrl_blk);

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_set_init_fw;
	}

	/*
	 * Transfer Data from Local buffer to DMA buffer
	 */
	pinit_fw_cb = (struct init_fw_ctrl_blk *)ql4im_ha->dma_v;
	memset(pinit_fw_cb, 0, sizeof(struct init_fw_ctrl_blk));

	pinit_fw_cb->pri.version	     = pinit_fw->Version;
	pinit_fw_cb->pri.fw_options	     = cpu_to_le16(pinit_fw->FWOptions);
	pinit_fw_cb->pri.add_fw_options    = cpu_to_le16(pinit_fw->AddFWOptions);
	memcpy(pinit_fw_cb->pri.ipv4_addr, pinit_fw->IPAddr.IPAddress,
	    MIN(sizeof(pinit_fw_cb->pri.ipv4_addr),
	    sizeof(pinit_fw->IPAddr.IPAddress)));
	memcpy(pinit_fw_cb->pri.ipv4_subnet, pinit_fw->SubnetMask.IPAddress,
	    MIN(sizeof(pinit_fw_cb->pri.ipv4_subnet),
	    sizeof(pinit_fw->SubnetMask.IPAddress)));
	memcpy(pinit_fw_cb->pri.ipv4_gw_addr, pinit_fw->Gateway.IPAddress,
	    MIN(sizeof(pinit_fw_cb->pri.ipv4_gw_addr),
	    sizeof(pinit_fw->Gateway.IPAddress)));
	memcpy(pinit_fw_cb->pri.pri_dns_srvr_ip, pinit_fw->DNSConfig.IPAddr.IPAddress,
	    MIN(sizeof(pinit_fw_cb->pri.pri_dns_srvr_ip),
	    sizeof(pinit_fw->DNSConfig.IPAddr.IPAddress)));
	memcpy(pinit_fw_cb->pri.iscsi_alias, pinit_fw->Alias,
	    MIN(sizeof(pinit_fw_cb->pri.iscsi_alias), sizeof(pinit_fw->Alias)));
	memcpy(pinit_fw_cb->pri.iscsi_name, pinit_fw->iSCSIName,
	    MIN(sizeof(pinit_fw_cb->pri.iscsi_name),
	    sizeof(pinit_fw->iSCSIName)));

	pinit_fw_cb->pri.exec_throttle =
	    cpu_to_le16(pinit_fw->DeviceInfo.ExeThrottle);
	pinit_fw_cb->pri.iscsi_opts =
	    cpu_to_le16(pinit_fw->DeviceInfo.iSCSIOptions);
	pinit_fw_cb->pri.ipv4_tcp_opts = cpu_to_le16(pinit_fw->DeviceInfo.TCPOptions);
	pinit_fw_cb->pri.ipv4_ip_opts = cpu_to_le16(pinit_fw->DeviceInfo.IPOptions);
	pinit_fw_cb->pri.iscsi_max_pdu_size = cpu_to_le16(pinit_fw->DeviceInfo.MaxPDUSize);
	pinit_fw_cb->pri.iscsi_fburst_len =
	    cpu_to_le16(pinit_fw->DeviceInfo.FirstBurstSize);
	pinit_fw_cb->pri.iscsi_def_time2wait =
	    cpu_to_le16(pinit_fw->DeviceInfo.LogoutMinTime);
	pinit_fw_cb->pri.iscsi_def_time2retain =
	    cpu_to_le16(pinit_fw->DeviceInfo.LogoutMaxTime);
	pinit_fw_cb->pri.iscsi_max_outstnd_r2t =
	    cpu_to_le16(pinit_fw->DeviceInfo.MaxOutstandingR2T);
	pinit_fw_cb->pri.conn_ka_timeout =
	    cpu_to_le16(pinit_fw->DeviceInfo.KeepAliveTimeout);
	pinit_fw_cb->pri.ipv4_port = cpu_to_le16(pinit_fw->DeviceInfo.PortNumber);
	pinit_fw_cb->pri.iscsi_max_burst_len =
	    cpu_to_le16(pinit_fw->DeviceInfo.MaxBurstSize);

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_INITIALIZE_FIRMWARE;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = sizeof(struct init_fw_ctrl_blk);

	ioctl->Status = EXT_STATUS_OK;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
	}

exit_set_init_fw:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int
ql4_set_acb(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	uint32_t	mbox_cmd[MBOX_REG_COUNT];
	uint32_t	mbox_sts[MBOX_REG_COUNT];
	void *pacb;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if ((ioctl->RequestLen < EXT_DEF_ACB_SIZE) || 
		!ioctl->RequestAdr){
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_set_acb;
	}

	pacb = (void *)ql4im_ha->dma_v;
	memset(pacb, 0, EXT_DEF_ACB_SIZE);

	/*
	 * Copy the data from the user's buffer
	 */
	if ((status = copy_from_user((uint8_t *)pacb,
				     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     EXT_DEF_ACB_SIZE)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data to user's "
		    "memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_acb;
	}

	/*
	 * First get Initialize Firmware Control Block, so as not to
	 * destroy unaffected data
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_SET_ACB;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[4] = ioctl->RequestLen;

	ioctl->Status = EXT_STATUS_OK;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, MBOX_REG_COUNT, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
		goto exit_set_acb;
	}
	ioctl->ResponseLen = mbox_sts[4];

exit_set_acb:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_ext_set_data(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;

	switch (ioctl->SubCode) {
	case EXT_SC_RST_STATISTICS_GEN:
		status = ql4_rst_statistics_gen(ql4im_ha, ioctl);
		break;
	case EXT_SC_RST_STATISTICS_ISCSI:
		status = ql4_rst_statistics_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_SET_DEVICE_ENTRY_ISCSI:
		status = ql4_set_device_entry_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_SET_INIT_FW_ISCSI:
		status = ql4_set_init_fw_iscsi(ql4im_ha, ioctl);
		break;
	case EXT_SC_SET_ACB:
		status = ql4_set_acb(ql4im_ha, ioctl);
		break;
	case EXT_SC_SET_ISNS_SERVER:
	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported set data sub-command "
		    "code (%X)\n",
		    ql4im_ha->ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}
	return(status);
}

/* End of External ioctls */

static void
qla4xxx_scsi_pass_done(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host	*ha;
	struct hba_ioctl *ql4im_ha;


	ha = (struct scsi_qla_host *)cmd->device->host->hostdata;

	ENTER_IOCTL(__func__, ha->host_no);

	ql4im_ha = ql4im_get_adapter_handle(ha->instance);

	if (ql4im_ha) 
		ql4im_ha->pt_in_progress = 0;

	LEAVE_IOCTL(__func__, ha->host_no);

	return;
}

static int ql4_scsi_passthru(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	struct ddb_entry	*ddb_entry;
	EXT_SCSI_PASSTHRU_ISCSI *pscsi_pass;
	struct scsi_cmnd	*pscsi_cmd;
	struct srb		*srb;
	struct scsi_qla_host	*ha;
	int			ql_cmd_to = 6000;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if (!test_bit(AF_ONLINE, &ha->flags)) {
		DEBUG2(printk("qisioctl%lx: %s: command not pocessed, "
			"adapter link down.\n",
			ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_HBA_NOT_READY;
		goto exit_scsi_pass;
	}

	pscsi_pass = (EXT_SCSI_PASSTHRU_ISCSI *)ql4im_ha->tmp_buf;
	memset(pscsi_pass, 0, sizeof(EXT_SCSI_PASSTHRU_ISCSI));

	if ((status = copy_from_user((uint8_t *)pscsi_pass,
				     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(EXT_SCSI_PASSTHRU_ISCSI))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy passthru struct "
			"from user's memory area.\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_scsi_pass;
	}

	if (pscsi_pass->Addr.Target >= MAX_DDB_ENTRIES) {
		ioctl->Status = EXT_STATUS_ERR;
		goto exit_scsi_pass;
	}
#ifndef __VMKLNX__
	ddb_entry = ha->fw_ddb_index_map[pscsi_pass->Addr.Target];
#else
	{
		int found = 0;

		list_for_each_entry(ddb_entry, &ha->ddb_list, list) {
			if ((atomic_read(&ddb_entry->state) != DDB_STATE_REMOVED) &&
			   (ddb_entry->os_target_id == pscsi_pass->Addr.Target)) {
				found = 1;
				break;
			}
		}
		if (!found)
			ddb_entry = NULL;
	}
#endif

	if ((ddb_entry == NULL) || (ddb_entry == (struct ddb_entry *)INVALID_ENTRY)){
		DEBUG2(printk("qisioctl%lx: %s: invalid device (t%d) specified.\n",
		    ha->host_no, __func__, pscsi_pass->Addr.Target));

		ioctl->Status = EXT_STATUS_DEV_NOT_FOUND;
		goto exit_scsi_pass;
	}
	if ((ddb_entry->fw_ddb_device_state != DDB_DS_SESSION_ACTIVE) ||
		(ddb_entry->fw_ddb_index == INVALID_ENTRY)) {
		DEBUG2(printk("qisioctl%lx: %s: device (t%d) not in active state\n",
			ha->host_no, __func__, pscsi_pass->Addr.Target));

		ioctl->Status = EXT_STATUS_DEVICE_NOT_READY;
		goto exit_scsi_pass;
	}
	
	srb = &ql4im_ha->pt_srb;
	memset(srb, 0, sizeof(struct srb));

	pscsi_cmd = &ql4im_ha->pt_scsi_cmd;
	memset(pscsi_cmd, 0, sizeof(struct scsi_cmnd));

	pscsi_cmd->device = &ql4im_ha->pt_scsi_device;
	memset(pscsi_cmd->device, 0, sizeof(struct scsi_device));

	pscsi_cmd->request = &ql4im_ha->pt_request;
	memset(pscsi_cmd->request, 0, sizeof(struct request));

	memset(ql4im_ha->dma_v, 0, ql4im_ha->dma_len);

	pscsi_cmd->device->channel = pscsi_pass->Addr.Bus;
	pscsi_cmd->device->id = pscsi_pass->Addr.Target;
	pscsi_cmd->device->lun = pscsi_pass->Addr.Lun;
	pscsi_cmd->device->host = ha->host;
	pscsi_cmd->request_buffer = ql4im_ha->dma_v;
	pscsi_cmd->scsi_done = qla4xxx_scsi_pass_done;
	pscsi_cmd->timeout_per_command = 60 * HZ;
	pscsi_cmd->use_sg = 0;

	pscsi_cmd->SCp.ptr = (char *) srb;
	srb->cmd = pscsi_cmd;
	srb->fw_ddb_index = ddb_entry->fw_ddb_index;
	srb->flags |= SRB_SCSI_PASSTHRU;
	srb->ha = ha;
	srb->dma_handle = ql4im_ha->dma_p;
	srb->ddb = ddb_entry;
	atomic_set(&srb->ref_count, 1);

	if (pscsi_pass->CdbLength == 6 || pscsi_pass->CdbLength == 10 ||
	    pscsi_pass->CdbLength == 12 || pscsi_pass->CdbLength == 16) {
		pscsi_cmd->cmd_len = pscsi_pass->CdbLength;
	} else {
		DEBUG2(printk("qisioctl%lx: %s: Unsupported CDB length 0x%x \n",
			ha->host_no, __func__, pscsi_cmd->cmd_len));

		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_scsi_pass;
	}

	if (pscsi_pass->Direction == EXT_DEF_SCSI_PASSTHRU_DATA_IN) {
		pscsi_cmd->sc_data_direction = DMA_FROM_DEVICE;
		pscsi_cmd->request_bufflen = ioctl->ResponseLen -
		    sizeof(EXT_SCSI_PASSTHRU_ISCSI);

	} else if (pscsi_pass->Direction ==  EXT_DEF_SCSI_PASSTHRU_DATA_OUT) {
		pscsi_cmd->sc_data_direction = DMA_TO_DEVICE;
		pscsi_cmd->request_bufflen = ioctl->RequestLen -
		    sizeof(EXT_SCSI_PASSTHRU_ISCSI);

		/* Sending user data from ioctl->ResponseAddr to SCSI
		 * command buffer
		 */
		if ((status = copy_from_user((uint8_t *)
					     pscsi_cmd->request_buffer,
					     Q64BIT_TO_PTR(ioctl->RequestAdr,
							   ioctl->AddrMode) +
					     sizeof(EXT_SCSI_PASSTHRU_ISCSI),
					     pscsi_cmd->request_bufflen)) != 0) {
			DEBUG2(printk("qisioctl%lx: %s: unable to copy write buffer "
				"from user's memory area.\n", 
				ha->host_no, __func__));

			ioctl->Status = EXT_STATUS_COPY_ERR;
			goto exit_scsi_pass;
		}
	} else {
		pscsi_cmd->sc_data_direction = DMA_NONE;
		pscsi_cmd->request_buffer  = 0;
		pscsi_cmd->request_bufflen = 0;
	}

	memcpy(pscsi_cmd->cmnd, pscsi_pass->Cdb, pscsi_cmd->cmd_len);

	DEBUG4(printk("qisioctl%lx:%d:%d:%d: %s:\n",
		ha->host_no, pscsi_cmd->device->channel,
		pscsi_cmd->device->id, pscsi_cmd->device->lun, __func__));
	DEBUG4(scsi_print_command(pscsi_cmd));
	DEBUG4(printk("\thost %p rqb %p rqbln %d done %p dir %d \n",
		pscsi_cmd->device->host, pscsi_cmd->request_buffer,
		pscsi_cmd->request_bufflen, pscsi_cmd->scsi_done,
		pscsi_cmd->sc_data_direction));

	ql4im_ha->pt_in_progress = 1;

	srb->cc_stat = IOCTL_INVALID_STATUS;

	if (!test_bit(AF_ONLINE, &ha->flags)) {
		DEBUG2(printk("qisioctl%lx: %s: command not pocessed, "
			"adapter link down.\n",
			ha->host_no, __func__));
		ioctl->Status = EXT_STATUS_HBA_NOT_READY;
		ql4im_ha->pt_in_progress = 0;
		goto exit_scsi_pass;
	}
	if (ha->ql4cmd(ha, srb) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: error sending cmd to isp\n",
		    ha->host_no, __func__));
		ql4im_ha->pt_in_progress = 0;
		ioctl->Status = EXT_STATUS_DEV_NOT_FOUND;
		goto exit_scsi_pass;
	}

	while (ql4im_ha->pt_in_progress && ql_cmd_to) {
		msleep(10);
		ql_cmd_to--;
	}
	if (ql4im_ha->pt_in_progress) {
		DEBUG2(printk("qisioctl%lx: %s: ERROR = command timeout.\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;

		ql4im_ha->pt_in_progress = 0;
		goto exit_scsi_pass;
	}

	DEBUG4(printk("\tresult 0x%x cc_stat 0x%x\n", pscsi_cmd->result, 
		srb->cc_stat));
	ioctl->DetailStatus = pscsi_cmd->result & 0xFFFF;
	pscsi_pass->Reserved[0] = (uint8_t) (pscsi_cmd->result & 0xFFFF);
	pscsi_pass->Reserved[1] = (uint8_t) srb->cc_stat;

	if (((pscsi_cmd->result >> 16) == DID_OK) &&
		((pscsi_cmd->result & 0xFFFF) == SCSI_CHECK_CONDITION)) {
		pscsi_pass->Reserved[2] = 
			(uint8_t)(sizeof(pscsi_cmd->sense_buffer));

		memcpy(pscsi_pass->SenseData, pscsi_cmd->sense_buffer,
		    MIN(sizeof(pscsi_cmd->sense_buffer),
		    sizeof(pscsi_pass->SenseData)));

		DEBUG10(printk("qisioctl%lx: %s: sense data dump:\n",
		    ha->host_no, __func__));
	}
	pscsi_pass->Reserved[3] = (uint8_t) host_byte(pscsi_cmd->result);
	pscsi_pass->Reserved[6] = (uint8_t) 0;
	pscsi_pass->Reserved[7] = (uint8_t) 0;

	if ((pscsi_cmd->result >> 16) == DID_OK) {

		ioctl->Status = EXT_STATUS_OK;

	} else if (srb->cc_stat == SCS_DATA_UNDERRUN) {
		DEBUG2(printk("qisioctl%lx: %s: Data underrun.  Resid = 0x%x\n",
			ha->host_no, __func__, pscsi_cmd->resid));

		ioctl->Status = EXT_STATUS_DATA_UNDERRUN;
		pscsi_pass->Reserved[4] = MSB(pscsi_cmd->resid);
		pscsi_pass->Reserved[5] = LSB(pscsi_cmd->resid);

	} else if (srb->cc_stat == SCS_DATA_OVERRUN) {
		DEBUG2(printk("qisioctl%lx: %s: Data overrun.  Resid = 0x%x\n",
			ha->host_no, __func__, pscsi_cmd->resid));

		ioctl->Status = EXT_STATUS_DATA_OVERRUN;
		pscsi_pass->Reserved[4] = MSB(pscsi_cmd->resid);
		pscsi_pass->Reserved[5] = LSB(pscsi_cmd->resid);

	} else {
		DEBUG2(printk("qisioctl%lx: %s: Command completed in ERROR. "
		    "cs=%04x, ss=%-4x\n", ha->host_no, __func__,
		    srb->cc_stat, (pscsi_cmd->result & 0xFFFF)));

		if ((pscsi_cmd->result & 0xFFFF) != SCSI_GOOD) {
			ioctl->Status = EXT_STATUS_SCSI_STATUS;
		} else {
			ioctl->Status = EXT_STATUS_ERR;
		}
	}

	if (copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode),
			 pscsi_pass, sizeof(EXT_SCSI_PASSTHRU_ISCSI)) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy passthru struct "
			"to user's memory area.\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_scsi_pass;
	}

	if (pscsi_pass->Direction == EXT_DEF_SCSI_PASSTHRU_DATA_IN) {
		void	*xfer_ptr = Q64BIT_TO_PTR(ioctl->ResponseAdr,
						  ioctl->AddrMode) +
				    sizeof(EXT_SCSI_PASSTHRU_ISCSI);
		uint32_t xfer_len = ioctl->ResponseLen -
				    sizeof(EXT_SCSI_PASSTHRU_ISCSI);

		if (srb->cc_stat == SCS_DATA_UNDERRUN && pscsi_cmd->resid) {
			xfer_len -= pscsi_cmd->resid;
		}

		if ((status = copy_to_user(xfer_ptr, pscsi_cmd->request_buffer,
		    xfer_len)) != 0) {
			DEBUG2(printk("qisioctl%lx: %s: unable to copy READ data "
			    "to user's memory area.\n",
			     ha->host_no, __func__));

			ioctl->Status = EXT_STATUS_COPY_ERR;
		}
	}

exit_scsi_pass:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}


static int ql4_get_hbacnt(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	int		status = 0;
	EXT_HBA_COUNT	hba_cnt;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	memset(&hba_cnt, 0, sizeof(EXT_HBA_COUNT));
	hba_cnt.HbaCnt = ql4im_get_hba_count();
	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
		&hba_cnt, sizeof(hba_cnt))) != 0) {
		DEBUG2(printk("qisioctl%d: %s: failed to copy data\n",
		    (int)ql4im_ha->ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_hbacnt;
	}

	DEBUG2(printk("qisioctl%d: %s: hbacnt is %d\n",
	    (int)ql4im_ha->ha->host_no, __func__, hba_cnt.HbaCnt));
	ioctl->Status = EXT_STATUS_OK;

exit_get_hbacnt:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_get_hostno(struct hba_ioctl *ql4im_ha, 
				EXT_IOCTL_ISCSI *ioctl)
{
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	ioctl->HbaSelect = ql4im_ha->ha->host_no;
	ioctl->Status = EXT_STATUS_OK;

	DEBUG4(printk("qisioctl%lxd: %s: nce is %d\n",
	    ql4im_ha->ha->host_no, __func__, ioctl->HbaSelect));

	LEAVE_IOCTL(__func__, ha->host_no);

	return(0);
}

static int ql4_driver_specific(struct hba_ioctl *ql4im_ha, 
					EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	EXT_LN_DRIVER_DATA	data;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if (ioctl->ResponseLen < sizeof(EXT_LN_DRIVER_DATA)) {
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG2(printk("qisioctl: %s: ERROR ResponseLen too small.\n", __func__));
		goto exit_driver_specific;
	}

	data.DrvVer.Major = drvr_major;
	data.DrvVer.Minor = drvr_minor;
	data.DrvVer.Patch = drvr_patch;
	data.DrvVer.Beta  = drvr_beta;

	if (is_qla4010(ql4im_ha->ha))
		data.AdapterModel = EXT_DEF_QLA4010_DRIVER;
	else if (is_qla4022(ql4im_ha->ha))
		data.AdapterModel = EXT_DEF_QLA4022_DRIVER;

	status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode),
			      &data, sizeof(EXT_LN_DRIVER_DATA));

	if (status) {
		ioctl->Status = EXT_STATUS_COPY_ERR;
		DEBUG2(printk("qisioctl: %s: ERROR copy resp buf\n", __func__));
	}

exit_driver_specific:

	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

#ifdef __VMKLNX__
static int ql4_get_port_device_name(struct hba_ioctl *ql4im_ha, 
					EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	EXT_GET_PORT_DEVICE_NAME data;
	struct scsi_qla_host	*ha;
	char 			*vmhba_name;
	int			len;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if (ioctl->ResponseLen < sizeof(EXT_GET_PORT_DEVICE_NAME)) {
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG2(printk("qisioctl: %s: ERROR ResponseLen too small.\n", __func__));
		goto exit_get_port_device_name;
	}

	vmhba_name = vmklnx_get_vmhba_name(ha->host);
	if (vmhba_name == NULL) {
		ioctl->Status = EXT_STATUS_ERR;
		DEBUG2(printk("qisioctl: %s: ERROR vmhba NULL\n", __func__));
		goto exit_get_port_device_name;
	}

	memset(&data, 0, sizeof(EXT_GET_PORT_DEVICE_NAME));
	len = MIN(strlen(vmhba_name), sizeof(data.deviceName));
	strncpy(data.deviceName, vmhba_name, len);

	status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode),
			      &data, sizeof(EXT_GET_PORT_DEVICE_NAME));

	if (status) {
		ioctl->Status = EXT_STATUS_COPY_ERR;
		DEBUG2(printk("qisioctl: %s: ERROR copy resp buf\n", __func__));
	}

exit_get_port_device_name:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}
#endif

static int ql4_disable_acb(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host	*ha;
	int			count = 600;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_DISABLE_ACB;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = ioctl->Reserved1;

	ioctl->Status = EXT_STATUS_OK;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, MBOX_REG_COUNT, &mbox_cmd[0],
					&mbox_sts[0]) != QLA_SUCCESS) {
		if (mbox_sts[0] == MBOX_STS_INTERMEDIATE_COMPLETION) {

			while (count--) {
				memset(&mbox_cmd, 0, sizeof(mbox_cmd));
				memset(&mbox_sts, 0, sizeof(mbox_sts));

				mbox_cmd[0] = MBOX_CMD_GET_IP_ADDR_STATE;
				mbox_cmd[1] = ioctl->Instance;
				if (ha->ql4mbx(ha, MBOX_REG_COUNT, 8,
					&mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
					DEBUG2(printk("qisioctl%lx: %s: command failed \n",
						ha->host_no, __func__));
					ioctl->Status = EXT_STATUS_ERR;
					ioctl->DetailStatus = mbox_sts[0];
					break;
				} else {
					if (!(mbox_sts[1] & 0xF0000000))
						break;
					else
						msleep(100);
				}	
			}
			if (!count)
				ioctl->Status = EXT_STATUS_ERR;
		} else {
			ioctl->Status = EXT_STATUS_ERR;
			ioctl->DetailStatus = mbox_sts[0];
		}
	}

	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_send_router_sol(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host	*ha;
	uint32_t		ip_addr;
	EXT_SEND_ROUTER_SOL	sr_sol;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if (ioctl->ResponseLen < sizeof(EXT_LN_DRIVER_DATA)) {
		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG2(printk("qisioctl: %s: ERROR ResponseLen too small.\n", __func__));
		goto exit_sndr_sol;
	}

	if ((status = copy_from_user((uint8_t *)&sr_sol,
                                     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(sr_sol))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from "
		    "user's memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_sndr_sol;
	}

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_SEND_IPV6_ROUTER_SOL;
	mbox_cmd[1] = ioctl->Instance;
	mbox_cmd[2] = sr_sol.Flags;
	memcpy(&ip_addr, &sr_sol.Addr.IPAddress, sizeof(ip_addr));
	mbox_cmd[3] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &sr_sol.Addr.IPAddress[4], sizeof(ip_addr));
	mbox_cmd[4] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &sr_sol.Addr.IPAddress[8], sizeof(ip_addr));
	mbox_cmd[5] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &sr_sol.Addr.IPAddress[12], sizeof(ip_addr));
	mbox_cmd[6] = cpu_to_le32(ip_addr);

	ioctl->Status = EXT_STATUS_OK;
	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
	}
	ioctl->VendorSpecificStatus[0] = mbox_sts[0];
	ioctl->VendorSpecificStatus[1] = mbox_sts[1];

exit_sndr_sol:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

/* Start of Internal ioctls */

static int ql4_ping(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	INT_PING		ping;
	uint32_t		ip_addr;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if ((status = copy_from_user((uint8_t *)&ping,
                                     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(ping))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from "
		    "user's memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_ping;
	}

	/*
	 * Issue Mailbox Command
	 */
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_PING;
	mbox_cmd[1] = ping.Reserved;
	memcpy(&ip_addr, &ping.IPAddr.IPAddress, sizeof(ip_addr));
	mbox_cmd[2] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &ping.IPAddr.IPAddress[4], sizeof(ip_addr));
	mbox_cmd[3] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &ping.IPAddr.IPAddress[8], sizeof(ip_addr));
	mbox_cmd[4] = cpu_to_le32(ip_addr);
	memcpy(&ip_addr, &ping.IPAddr.IPAddress[12], sizeof(ip_addr));
	mbox_cmd[5] = cpu_to_le32(ip_addr);
	mbox_cmd[6] = cpu_to_le32(ping.PacketSize);

	ioctl->Status = EXT_STATUS_OK;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: command failed \n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_ERR;
		ioctl->DetailStatus = mbox_sts[0];
	}

exit_ping:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_flash(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	uint32_t		data_len;
	uint32_t		data_offset;
	INT_ACCESS_FLASH	*puser_flash;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	puser_flash = Q64BIT_TO_PTR(ioctl->RequestAdr, ioctl->AddrMode);

	if ((status = copy_from_user((void *)&data_len,
				 (void *)&puser_flash->DataLen,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataLen copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_flash;
	}
	if (data_len > sizeof(puser_flash->FlashData)) {
		DEBUG2(printk("qisioctl(%d): %s: DataLen invalid 0x%x\n",
			(int)ha->host_no, __func__, data_len));
		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_get_flash;
	}
	
	if ((status = copy_from_user((void *)&data_offset,
				 (void *)&puser_flash->DataOffset,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataOffset copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_flash;
	}
	DUMP_GET_FLASH(data_offset, data_len);

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_READ_FLASH;
	mbox_cmd[1] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[2] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = data_offset;
	mbox_cmd[4] = data_len;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: READ_FLASH failed st 0x%x\n",
			(int)ha->host_no, __func__, mbox_sts[0]));
		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		ioctl->VendorSpecificStatus[0] = mbox_sts[1];
		goto exit_get_flash;
	}

	puser_flash = Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode);
	/*
	 * Copy local DMA buffer to user's response data area
	 */
	if ((status = copy_to_user(&puser_flash->FlashData[0],
                                   ql4im_ha->dma_v,
				   data_len)) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: FlashData to user failed\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_flash;
	}
	if ((status = copy_to_user(&puser_flash->DataLen,
                                   &data_len,
				   sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataLen to user failed\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_flash;
	}
	if ((status = copy_to_user(&puser_flash->DataOffset,
                                   &data_offset,
				   sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataOffset to user failed\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_get_flash;
	}

	ioctl->Status = EXT_STATUS_OK;
	ioctl->ResponseLen = data_len;

exit_get_flash:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_get_host_no(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	if ((status = copy_to_user(Q64BIT_TO_PTR(ioctl->ResponseAdr,
						 ioctl->AddrMode),
                                   &(ha->host_no),
				   sizeof(ha->host_no))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: failed to copy data\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
	} else {
		ioctl->Status = EXT_STATUS_OK;
	}

	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

static int ql4_get_core_dump(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	INT_ACCESS_CORE_DUMP	cdump, *puser_cdump;
	struct scsi_qla_host	*ha;
	void 			*pdump;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	if ((status = copy_from_user((uint8_t *)&cdump,
                                     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(cdump))) != 0) {
		DEBUG2(printk("qisioctl(%lx): %s: unable to copy data from "
		    "user's memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_core_dump;
	}

        ioctl->Status = EXT_STATUS_OK;
	
	if ((!cdump.DataLen) || (cdump.Offset >= DUMP_IMAGE_SIZE))
		goto exit_core_dump;

	if (ql4im_ha->core == NULL) {
		if((ql4im_ha->core = vmalloc(DUMP_IMAGE_SIZE)) == NULL){
			ioctl->Status = EXT_STATUS_NO_MEMORY;
			goto exit_core_dump;
		}
		ql4_core_dump(ha, ql4im_ha->core);
	}

		
	puser_cdump = Q64BIT_TO_PTR(ioctl->ResponseAdr, ioctl->AddrMode);
	pdump = (unsigned char *)ql4im_ha->core + cdump.Offset;

        ioctl->ResponseLen = cdump.DataLen;
	if ((cdump.Offset + cdump.DataLen) > DUMP_IMAGE_SIZE) {
		cdump.LastBlockFlag = 1;
		ioctl->ResponseLen = DUMP_IMAGE_SIZE - cdump.Offset;
        	if (copy_to_user(puser_cdump, &cdump, sizeof(cdump)) != 0) {
               		DEBUG2(printk("qisioctl(%d): %s: unable to copy data to user's "
                    	"memory area\n", (int)ha->host_no, __func__));
                	ioctl->Status = EXT_STATUS_COPY_ERR;
        	}
	} else 
		cdump.LastBlockFlag = 0;

        if (copy_to_user(&puser_cdump->Data[0], pdump, ioctl->ResponseLen) != 0) {
                DEBUG2(printk("qisioctl(%d): %s: unable to copy data to user's "
                    "memory area\n", (int)ha->host_no, __func__));
                ioctl->Status = EXT_STATUS_COPY_ERR;
        }

	if (cdump.LastBlockFlag) {
		vfree(ql4im_ha->core);
		ql4im_ha->core = NULL;
	}
exit_core_dump:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_int_get_data(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int	status = 0;

	switch (ioctl->SubCode) {
	case INT_SC_GET_FLASH:
		status = ql4_get_flash(ql4im_ha, ioctl);
		break;
	case INT_SC_GET_HOST_NO:
		status = ql4_get_host_no(ql4im_ha, ioctl);
		break;
	case INT_SC_GET_CORE_DUMP:
		status = ql4_get_core_dump(ql4im_ha, ioctl);
		break;
	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported internal get data "
		    "sub-command code (%X)\n",
		    ql4im_ha->ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	return status;
}

static int ql4_set_flash(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int			status = 0;
	uint32_t		mbox_cmd[MBOX_REG_COUNT];
	uint32_t		mbox_sts[MBOX_REG_COUNT];
	uint32_t		area_type;
	uint32_t		data_len;
	uint32_t		data_offset;
	uint32_t		data_options;
	INT_ACCESS_FLASH	*puser_flash;
	struct scsi_qla_host	*ha;

	ha = ql4im_ha->ha;

	ENTER_IOCTL(__func__, ha->host_no);

	puser_flash = Q64BIT_TO_PTR(ioctl->RequestAdr, ioctl->AddrMode);

	if ((status = copy_from_user((void *)&area_type,
				 (void *)&puser_flash->AreaType,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: AreaType copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_flash;
	}

	if ((status = copy_from_user((void *)&data_len,
				 (void *)&puser_flash->DataLen,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataLen copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_flash;
	}
	if (data_len > sizeof(puser_flash->FlashData)) {
		DEBUG2(printk("qisioctl(%d): %s: DataLen invalid 0x%x\n",
			(int)ha->host_no, __func__, data_len));
		ioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_set_flash;
	}
	
	if ((status = copy_from_user((void *)&data_offset,
				 (void *)&puser_flash->DataOffset,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: DataOffset copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_flash;
	}
	
	if ((status = copy_from_user((void *)&data_options,
				 (void *)&puser_flash->Options,
				 sizeof (uint32_t))) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: Options copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_flash;
	}
	
	if ((status = copy_from_user((void *)ql4im_ha->dma_v,
				(void *)&puser_flash->FlashData[0],
				data_len)) != 0) {
		DEBUG2(printk("qisioctl(%d): %s: FlashData copy error\n",
			(int)ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_set_flash;
	}

	DUMP_SET_FLASH(data_offset, data_len, data_options);

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_WRITE_FLASH;
	mbox_cmd[1] = LSDW(ql4im_ha->dma_p);
	mbox_cmd[2] = MSDW(ql4im_ha->dma_p);
	mbox_cmd[3] = data_offset;
	mbox_cmd[4] = data_len;
	mbox_cmd[5] = data_options;

	ioctl->Status = EXT_STATUS_OK;
	ioctl->ResponseLen = data_len;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl(%d): %s: WRITE_FLASH failed st 0x%x\n",
			(int)ha->host_no, __func__, mbox_sts[0]));
		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		ioctl->VendorSpecificStatus[0] = mbox_sts[1];
	}
	msleep(10);
exit_set_flash:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

/**
 * ql4_int_set_data
 *	This routine calls set data IOCTLs based on the IOCTL Sub Code.
 *	Kernel context.
 **/
static int ql4_int_set_data(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int	status = 0;

	switch (ioctl->SubCode) {
	case INT_SC_SET_FLASH:
		status = ql4_set_flash(ql4im_ha, ioctl);
		break;
	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported subCode(0x%x)\n",
			ql4im_ha->ha->host_no, __func__, ioctl->SubCode));

		ioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	return status;
}

/**
 * ql4_hba_reset
 *	This routine resets the specified HBA.
 **/
static int ql4_hba_reset(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	uint8_t		status = 0;
	u_long		wait_count = 180;
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	ql4im_ha->aen_read = 0;

	ioctl->Status = EXT_STATUS_OK;

	set_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags);

	while (wait_count) {
		ssleep(1);

		if ((!test_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags))
			&& test_bit(AF_ONLINE, &ha->flags)) {
			msleep(30);
			break;
		}
		wait_count--;
	}

	if (!wait_count)
		ioctl->Status = EXT_STATUS_ERR;

	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

/**
 * ql4_copy_fw_flash
 *	This routine requests copying the FW image in FLASH from primary-to-
 *	secondary or secondary-to-primary.
 **/
static int ql4_copy_fw_flash(struct hba_ioctl *ql4im_ha, EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	INT_COPY_FW_FLASH copy_flash;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);

	ioctl->Status = EXT_STATUS_OK;

	if ((status = copy_from_user((uint8_t *)&copy_flash,
                                     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof (INT_COPY_FW_FLASH))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from user's "
		    "memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_copy_flash;
	}

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	mbox_cmd[0] = MBOX_CMD_COPY_FLASH;
	mbox_cmd[1] = copy_flash.Options;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 2, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: COPY_FLASH failed w/ "
		    "status %04X\n", ha->host_no, __func__, mbox_sts[0]));
		ioctl->Status = EXT_STATUS_MAILBOX;
		ioctl->DetailStatus = mbox_sts[0];
		ioctl->VendorSpecificStatus[0] = mbox_sts[1];
	}

exit_copy_flash:
	LEAVE_IOCTL(__func__, ha->host_no);

	return(status);
}

/**
 * ql4_restore_factory_defaults
 *	This routine restores factory defaults of the adapter.
**/
static int ql4_restore_factory_defaults(struct hba_ioctl *ql4im_ha, 
					EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	INT_RESTORE_FACTORY_DEFAULTS defaults;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);
	ioctl->Status = EXT_STATUS_OK;

	if (ioctl->RequestLen > sizeof(INT_RESTORE_FACTORY_DEFAULTS)) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
		    ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_defaults;
	}

	if ((status = copy_from_user((void *)&defaults,
                                     Q64BIT_TO_PTR(ioctl->RequestAdr,
						   ioctl->AddrMode),
				     sizeof(INT_RESTORE_FACTORY_DEFAULTS))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from "
		    "user's memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_defaults;
	}

	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));
	mbox_cmd[0] = MBOX_CMD_RESTORE_FACTORY_DEFAULTS;
	mbox_cmd[3] = defaults.BlockMask;
        mbox_cmd[4] = defaults.IFCBMask1;
        mbox_cmd[5] = defaults.IFCBMask2;

	if (ha->ql4mbx(ha, MBOX_REG_COUNT, 1, &mbox_cmd[0], &mbox_sts[0]) != QLA_SUCCESS) {
		DEBUG2(printk("qisioctl%lx: %s: RESTORE_FACTORY_DEFAULTS failed w/ "
		    "status %04X\n", ha->host_no, __func__, mbox_sts[0]));
		ioctl->Status = EXT_STATUS_MAILBOX;
	}

exit_defaults:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

static int ql4_logout(struct hba_ioctl *ql4im_ha, 
			EXT_IOCTL_ISCSI *ioctl)
{
	int status = 0;
	INT_LOGOUT_ISCSI lo;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct scsi_qla_host *ha;
#ifdef __VMKLNX__
	struct ddb_entry *ddb_entry;
	unsigned int timeout;
	int retries = 0;
#endif

	ha = ql4im_ha->ha;
	ENTER_IOCTL(__func__, ha->host_no);
	ioctl->Status = EXT_STATUS_OK;

	if (ioctl->RequestLen > sizeof(INT_LOGOUT_ISCSI)) {
		DEBUG2(printk("qisioctl%lx: %s: memory area too small\n",
			ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		goto exit_logout;
	}

	if ((status = copy_from_user((void *)&lo,
			Q64BIT_TO_PTR(ioctl->RequestAdr, ioctl->AddrMode),
				sizeof(INT_LOGOUT_ISCSI))) != 0) {
		DEBUG2(printk("qisioctl%lx: %s: unable to copy data from "
			"user's memory area\n", ha->host_no, __func__));

		ioctl->Status = EXT_STATUS_COPY_ERR;
		goto exit_logout;
	}

	DEBUG2(printk("qisioctl(%lx): %s: TgtId=0x%04x ConId=0x%04x Opt=0x%04x NTId=0x%08x\n",
		ha->host_no, __func__, lo.TargetID, lo.ConnectionID,
		lo.Options, lo.NewTargetID));

	if (lo.TargetID >= MAX_DDB_ENTRIES) {
		ioctl->Status = EXT_STATUS_ERR;
		DEBUG2(printk("qisioctl%lx: %s: fw_ddb_index %d out of range"
			"\n", ha->host_no, __func__, lo.TargetID));
		goto exit_logout;
	}

#ifdef __VMKLNX__
	ddb_entry = ha->fw_ddb_index_map[lo.TargetID];
	if (ddb_entry == NULL) {
		DEBUG2(dev_info(&ha->pdev->dev, "%s: ddb[%d] ddb_entry NULL\n",
			__func__, lo.TargetID));
		ioctl->Status = EXT_STATUS_MAILBOX;
		goto exit_logout;
	}

	/*
	 * Short circuit the recovery timeout.  If the state is not
	 * already MISSING this will cause us to immediately go from
	 * MISSING to DEAD.
	 */
	ddb_entry->sess->recovery_tmo = 0;

	/* Stop relogin attempts if we are removing DDB */
	if (lo.Options & INT_DEF_DELETE_DDB)
		set_bit(DF_STOP_RELOGIN, &ddb_entry->flags);

	/* We only need to close the connection if the session is active */
	if (ddb_entry->fw_ddb_device_state == DDB_DS_SESSION_ACTIVE) {
#endif

		memset(&mbox_cmd, 0, sizeof(mbox_cmd));
		memset(&mbox_sts, 0, sizeof(mbox_sts));
		mbox_cmd[0] = MBOX_CMD_CONN_CLOSE_SESS_LOGOUT;
		mbox_cmd[1] = lo.TargetID;

		mbox_cmd[3] = 0x4; /* reset and close the tcp connection */
		if (lo.Options & INT_DEF_DELETE_DDB)
			mbox_cmd[3] |= 0x8;/* Free DDB */

		if (ha->ql4mbx(ha, MBOX_REG_COUNT, 3, &mbox_cmd[0], &mbox_sts[0])
			!= QLA_SUCCESS) {
			DEBUG2(printk("qisioctl%lx: %s:"
				" MBOX_CMD_CONN_CLOSE_SESS_LOGOUT failed w/ "
				"cmd 0x%04x 0x%04x 0x%04x 0x%04x"
				"status 0x%04x 0x%04x 0x%04x \n", 
			ha->host_no, __func__, 
			mbox_cmd[0], mbox_cmd[1], mbox_cmd[2], mbox_cmd[3],
			mbox_sts[0], mbox_sts[1], mbox_sts[2]));
			ioctl->Status = EXT_STATUS_MAILBOX;
			goto exit_logout;
		}

#ifdef __VMKLNX__
		/*
		 * Qla spec. states we should wait for the AEN after calling "Close
		 * Connection (56)" before calling "Free Device Database Entry (31h).
		 */
		timeout = 30;
		while (timeout--) {
			if (ddb_entry->fw_ddb_device_state == DDB_DS_NO_CONNECTION_ACTIVE) {
				break;
			}
			ssleep(1);
		}
		if (ddb_entry->fw_ddb_device_state != DDB_DS_NO_CONNECTION_ACTIVE) {
			dev_info(&ha->pdev->dev,
				"%s: ddb[%d] os[%d] close connection timed out\n",
				__func__, ddb_entry->fw_ddb_index,
				ddb_entry->os_target_id);
			ioctl->Status = EXT_STATUS_MAILBOX;
			goto exit_logout;
		}
	}
#endif

        if (lo.Options & INT_DEF_DELETE_DDB) {
		memset(&mbox_cmd, 0, sizeof(mbox_cmd));
		memset(&mbox_sts, 0, sizeof(mbox_sts));
		mbox_cmd[0] = MBOX_CMD_CLEAR_DATABASE_ENTRY;
		mbox_cmd[1] = lo.TargetID;

#ifndef __VMKLNX__
		if (ha->ql4mbx(ha, MBOX_REG_COUNT, 3, &mbox_cmd[0],
			&mbox_sts[0]) != QLA_SUCCESS) {
#else
		/*
		 * There is a chance a relogin attempt is already outstanding
		 * in the firmware.  So we need to retry the free request.
		 */
                while ((retries++ < 3) &&
			((status = ha->ql4mbx(ha, MBOX_REG_COUNT, 3,
			&mbox_cmd[0], &mbox_sts[0])) != QLA_SUCCESS)) {

			DEBUG2(printk("qisioctl%lx: %s: attempt(%d)"
                        	"MBOX_CMD_CLEAR_DATABASE_ENTRY failed w/ "
                        	"cmd 0x%04x 0x%04x 0x%04x 0x%04x status "
				"0x%04x 0x%04x 0x%04x \n",
				ha->host_no, __func__, retries,
				mbox_cmd[0], mbox_cmd[1], mbox_cmd[2], mbox_cmd[3],
				mbox_sts[0], mbox_sts[1], mbox_sts[2]));

			ssleep(1);
		}

		if (status != QLA_SUCCESS) {
#endif
			DEBUG2(printk("qisioctl%lx: %s: "
                        	"MBOX_CMD_CLEAR_DATABASE_ENTRY failed w/ "
                        	"cmd 0x%04x 0x%04x 0x%04x 0x%04x status "
				"0x%04x 0x%04x 0x%04x \n",
			ha->host_no, __func__,
			mbox_cmd[0], mbox_cmd[1], mbox_cmd[2], mbox_cmd[3],
			mbox_sts[0], mbox_sts[1], mbox_sts[2]));
			ioctl->Status = EXT_STATUS_MAILBOX;
#ifdef __VMKLNX__
		} else {
			/*
			 * Now notify vmklinux to remove scsi_target via
			 * our dpc.  We do it from the dpc to block any 
			 * race conditions of relogin / etc.
			 */
			set_bit(DF_REMOVE, &ddb_entry->flags);
			set_bit(DPC_REMOVE_DEVICE, &ha->dpc_flags);

			DEBUG2(printk("scsi%ld: %s: scheduling dpc routine - "
				"dpc flags = 0x%lx\n", ha->host_no, __func__,
				ha->dpc_flags));
			queue_work(ha->dpc_thread, &ha->dpc_work);

			/*
			 * Now wait for the dbc remove to complete.  We need
			 * to wait so no other admin action occur while remove
			 * is in progress.
			 */
			timeout = 30;
			while (timeout--) {
				if (atomic_read(&ddb_entry->state) == DDB_STATE_DEAD) {
					break;
				}
				ssleep(1);
			}
			if (atomic_read(&ddb_entry->state) != DDB_STATE_DEAD) {
				dev_info(&ha->pdev->dev, "%s: ddb[%d] remove timed out\n",
					__func__, lo.TargetID);
				ioctl->Status = EXT_STATUS_MAILBOX;
			}
#endif
		}
	}


exit_logout:
	LEAVE_IOCTL(__func__, ha->host_no);
	return(status);
}

/* End of Internal ioctls */

/**
 * ql4_ioctl
 * 	This the main entry point for all ioctl requests
 *
 * Input:
 *	dev - pointer to SCSI device structure
 *	cmd - internal or external ioctl command code
 *	arg - pointer to the main ioctl structure
 *
 *	Instance field in ioctl structure - to determine which device to
 *	perform ioctl
 *	HbaSelect field in ioctl structure - to determine which adapter to
 *	perform ioctl
 *
 * Output:
 *	The resulting data/status is returned via the main ioctl structure.
 *
 *	When Status field in ioctl structure is valid for normal command errors
 * 	this function returns 0 (QLA_SUCCESS).
 *
 *      All other return values indicate ioctl/system specific error which
 *	prevented the actual ioctl command from completing.
 *
 * Returns:
 *	 QLA_SUCCESS - command completed successfully, either with or without
 *			errors in the Status field of the main ioctl structure
 *    	-EFAULT      - arg pointer is NULL or memory access error
 *    	-EINVAL      - command is invalid
 *    	-ENOMEM      - memory allocation failed
 *
 * Context:
 *	Kernel context.
 **/
int
qla4xxx_ioctl(int cmd, void *arg)
{
	EXT_IOCTL_ISCSI *pioctl = NULL;
	struct hba_ioctl *ql4im_ha = NULL;
	struct scsi_qla_host *ha;
	int status = 0;	/* ioctl status; errno value when function returns */

	/* Catch any non-exioct ioctls */
	if (_IOC_TYPE(cmd) != QLMULTIPATH_MAGIC) {
		printk(KERN_WARNING "qisioctl: invalid ioctl magic number received.\n");
		status = -EINVAL;
		goto exit_qla4xxx_ioctl0;
	}

	/* 
	 * Allocate ioctl structure buffer to support multiple concurrent
	 * entries. NO static structures allowed.
	 */
	pioctl = kzalloc(sizeof(EXT_IOCTL_ISCSI), GFP_ATOMIC);
	if (pioctl == NULL) {
		printk(KERN_WARNING "qisioctl: %s: kzalloc failed\n", __func__);
		status = -ENOMEM;
		goto exit_qla4xxx_ioctl0;
	}

#ifndef __VMKLNX__
	/*
	 * Check to see if we can access the ioctl command structure
	 */
	if (!access_ok(VERIFY_WRITE, arg, sizeof(EXT_IOCTL_ISCSI))) {
		DEBUG2(printk("qisioctl: %s: access_ok error.\n", __func__));
		status = (-EFAULT);
		goto exit_qla4xxx_ioctl1;
	}
#endif

	/*
	 * Copy the ioctl command structure from user space to local structure
	 */
	if ((status = copy_from_user((uint8_t *)pioctl, arg,
	    sizeof(EXT_IOCTL_ISCSI)))) {
		DEBUG2(printk("qisioctl: %s: copy_from_user error.\n",
		    __func__));

		goto exit_qla4xxx_ioctl1;
	}

	/*DEBUG10(printk("EXT_IOCTL_ISCSI structure dump: \n");)
	DEBUG10(ql4im_dump_dwords(pioctl, sizeof(*pioctl));)
	*/

        /* check signature of this ioctl */
	if (memcmp(pioctl->Signature, EXT_DEF_REGULAR_SIGNATURE,
	    sizeof(EXT_DEF_REGULAR_SIGNATURE)) != 0) {
		DEBUG2(printk("qisioctl: %s: signature did not match. "
		    "received cmd=%x arg=%p signature=%s.\n",
		    __func__, cmd, arg, pioctl->Signature));
		pioctl->Status = EXT_STATUS_INVALID_PARAM;
		goto exit_qla4xxx_ioctl;
	}

        /* check version of this ioctl */
        if (pioctl->Version > EXT_VERSION) {
                printk(KERN_WARNING
                    "qisioctl: ioctl interface version not supported = %d.\n",
                    pioctl->Version);

		pioctl->Status = EXT_STATUS_UNSUPPORTED_VERSION;
		goto exit_qla4xxx_ioctl;
        }

	/*
	 * Get the adapter handle for the corresponding adapter instance
	 */
	ql4im_ha = ql4im_get_adapter_handle(pioctl->HbaSelect);
	if (ql4im_ha == NULL) {
		DEBUG2(printk("qisioctl: %s: NULL HBA select %d\n",
			__func__, pioctl->HbaSelect));
		pioctl->Status = EXT_STATUS_DEV_NOT_FOUND;
		goto exit_qla4xxx_ioctl;
	}

#ifndef __VMKLNX__
	DEBUG4(printk("qisioctl%lx: ioctl+ (%s)\n", ql4im_ha->ha->host_no,
	    IOCTL_TBL_STR(cmd, pioctl->SubCode)));
#endif

	ha = ql4im_ha->ha;

	if (!test_bit(AF_ONLINE, &ha->flags)) {
		pioctl->Status = EXT_STATUS_HBA_NOT_READY;
		goto exit_qla4xxx_ioctl;
	}

#ifdef __VMKLNX__
	if (ql4im_down_timeout(&ql4im_ha->ioctl_sem, ioctl_timeout)) {
		pioctl->Status = EXT_STATUS_BUSY;
		DEBUG2(printk("qisioctl%d: %s: ERROR: "
			      "timeout getting ioctl sem\n",
			      pioctl->HbaSelect, __func__));
		goto exit_qla4xxx_ioctl;
	}
#else
	mutex_lock(&ql4im_ha->ioctl_sem);
	if (ql4im_ha->flag & HBA_IOCTL_BUSY) {
               pioctl->Status = EXT_STATUS_BUSY;
	} else {
		ql4im_ha->flag |= HBA_IOCTL_BUSY;
		pioctl->Status = EXT_STATUS_OK;
	}
	mutex_unlock(&ql4im_ha->ioctl_sem);

	if (pioctl->Status == EXT_STATUS_BUSY) {
		DEBUG2(printk("qisioctl: %s: busy\n", __func__));
		goto exit_qla4xxx_ioctl;
	}

#endif

	/*
	 * Issue the ioctl command
	 */
	switch (cmd) {

	case EXT_CC_QUERY:
		status = ql4_query(ql4im_ha, pioctl);
		break;

	case EXT_CC_REG_AEN:
		status = ql4_reg_aen(ql4im_ha, pioctl);
		break;

	case EXT_CC_GET_AEN:
		status = ql4_get_aen(ql4im_ha, pioctl);
		break;

	case EXT_CC_GET_DATA:
		status = ql4_ext_get_data(ql4im_ha, pioctl);
		break;

	case EXT_CC_SET_DATA:
		status = ql4_ext_set_data(ql4im_ha, pioctl);
		break;

	case EXT_CC_SEND_SCSI_PASSTHRU:
		status = ql4_scsi_passthru(ql4im_ha, pioctl);
		break;

	case EXT_CC_GET_HBACNT:
		status = ql4_get_hbacnt(ql4im_ha, pioctl);
		break;

	case EXT_CC_GET_HOST_NO:
		status = ql4_get_hostno(ql4im_ha, pioctl);
		break;

	case EXT_CC_DRIVER_SPECIFIC:
		status = ql4_driver_specific(ql4im_ha, pioctl);
		break;

#ifdef __VMKLNX__
	case EXT_CC_GET_PORT_DEVICE_NAME:
		status = ql4_get_port_device_name(ql4im_ha, pioctl);
		break;
#endif

	case EXT_CC_DISABLE_ACB:
		status = ql4_disable_acb(ql4im_ha, pioctl);
		break;

	case EXT_CC_SEND_ROUTER_SOL:
		status = ql4_send_router_sol(ql4im_ha, pioctl);
		break;

	case INT_CC_RESTORE_FACTORY_DEFAULTS:
		status = ql4_restore_factory_defaults(ql4im_ha, pioctl);
		break;

	case INT_CC_DIAG_PING:
		status = ql4_ping(ql4im_ha, pioctl);
		break;

	case INT_CC_GET_DATA:
		status = ql4_int_get_data(ql4im_ha, pioctl);
		break;

	case INT_CC_SET_DATA:
		status = ql4_int_set_data(ql4im_ha, pioctl);
		break;

	case INT_CC_HBA_RESET:
		status = ql4_hba_reset(ql4im_ha, pioctl);
		break;

	case INT_CC_COPY_FW_FLASH:
		status = ql4_copy_fw_flash(ql4im_ha, pioctl);
		break;

	case INT_CC_LOGOUT_ISCSI:
		status = ql4_logout(ql4im_ha, pioctl);
		break;
	default:
		DEBUG2(printk("qisioctl%lx: %s: unsupported command code (%x)\n",
		    ql4im_ha->ha->host_no, __func__, (uint32_t)cmd));

		pioctl->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
	}

#ifdef __VMKLNX__
	up(&ql4im_ha->ioctl_sem);
#else
	if (!(ql4im_ha->flag & HBA_IOCTL_BUSY)) {
		DEBUG2(printk("qisioctl%lx: %s: flag already clear!\n", 
				ql4im_ha->ha->host_no, __func__));
	}

	mutex_lock(&ql4im_ha->ioctl_sem);
	ql4im_ha->flag &= ~HBA_IOCTL_BUSY;
	mutex_unlock(&ql4im_ha->ioctl_sem);
#endif

exit_qla4xxx_ioctl:
	status = copy_to_user(arg, (void *)pioctl, sizeof(EXT_IOCTL_ISCSI));
exit_qla4xxx_ioctl1:
	kfree(pioctl);
exit_qla4xxx_ioctl0:

	return(status);
}
