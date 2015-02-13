/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "exioct.h"
#include "inioct.h"

#if defined(CONFIG_COMPAT) && !defined(CONFIG_IA64)
#include "qla_32ioctl.h"
#endif

#define QLA_PT_CMD_DRV_TOV		(ql2xioctltimeout + 1) /* drv timeout */
#define QLA_IOCTL_ACCESS_WAIT_TIME	(ql2xioctltimeout + 10) /* wait_q tov */
#define QLA_INITIAL_IOCTLMEM_SIZE	8192
#define QLA_IOCTL_SCRAP_SIZE		0x8000 /* scrap memory for local use. */

/* ELS related defines */
#define FC_HEADER_LEN		24
#define ELS_RJT_LENGTH		0x08	/* 8  */
#define ELS_RPS_ACC_LENGTH	0x40	/* 64 */
#define ELS_RLS_ACC_LENGTH	0x1C	/* 28 */

/* ELS cmd Reply Codes */
#define ELS_STAT_LS_RJT		0x01
#define ELS_STAT_LS_ACC		0x02

#define IOCTL_INVALID_STATUS    0xffff

static int ql2xcmdtimermin = QLA_CMD_TIMER_MINIMUM;
module_param(ql2xcmdtimermin, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xcmdtimermin,
                "Minimum command timeout value. "
                "Default is 30 seconds.");

extern void qla2x00_sp_free_dma(struct qla_hw_data *, srb_t *);
extern srb_t * qla2x00_get_new_sp(scsi_qla_host_t *, fc_port_t *,
    struct scsi_cmnd *, void (*done)(struct scsi_cmnd *));

extern int qla2x00_read_nvram(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_update_nvram(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_send_loopback(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_read_option_rom(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_update_option_rom(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_get_option_rom_layout(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_get_vpd(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_update_vpd(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_get_sfp_data(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_update_port_param(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2x00_get_fw_dump(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla84xx_mgmt_command(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2xxx_reset_fw_command(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2xxx_fcp_prio_cfg_cmd(scsi_qla_host_t *, EXT_IOCTL *, int);
extern int qla2xxx_get_board_temp(scsi_qla_host_t *, EXT_IOCTL *, int);

/* For scli vport support. */
extern uint32_t qla24xx_getinfo_vport(scsi_qla_host_t *ha, 
		    fc_vport_info_t *vp_info , uint32_t vp_id);
static int qla2x00_vf_ioctl(scsi_qla_host_t *, EXT_IOCTL *);

/* For scli vport support. */
extern uint32_t qla24xx_getinfo_vport(scsi_qla_host_t *ha, 
		    fc_vport_info_t *vp_info , uint32_t vp_id);
static int qla2x00_vf_ioctl(scsi_qla_host_t *, EXT_IOCTL *);

/*
 * Local prototypes
 */
static int qla2x00_get_tgt_lun_by_q(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_find_curr_host(uint16_t , scsi_qla_host_t **);

static int qla2x00_wwpn_to_scsiaddr(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_scsi_passthru(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_sc_scsi_passthru(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, struct scsi_device *, int);
static int qla2x00_sc_fc_scsi_passthru(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, struct scsi_device *, int);
static int qla2x00_sc_scsi3_passthru(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, struct scsi_device *, int);
static int qla2x00_ioctl_scsi_queuecommand(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, struct scsi_device *, fc_port_t *, fc_lun_t *);


static int qla2x00_query(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_hba_node(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_hba_port(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_disc_port(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_disc_tgt(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_chip(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_cna_port(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2xx_query_adapter_versions(scsi_qla_host_t *, EXT_IOCTL *);

static int qla2x00_get_data(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_statistics(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_fc_statistics(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_port_summary(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_fcport_summary(scsi_qla_host_t *, EXT_DEVICEDATAENTRY *,
    void *, uint32_t, uint32_t, uint32_t *, uint32_t *);
static int qla2x00_query_driver(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_query_fw(scsi_qla_host_t *, EXT_IOCTL *, int);

static int qla2x00_aen_reg(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_aen_get(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_send_els_rnid(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_set_rnid_params(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_set_led_state(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_set_led_23xx(scsi_qla_host_t *, EXT_BEACON_CONTROL *,
    uint32_t *, uint32_t *);
static int qla2x00_set_led_24xx(scsi_qla_host_t *, EXT_BEACON_CONTROL *,
    uint32_t *, uint32_t *);

static int qla2x00_msiocb_passthru(scsi_qla_host_t *, EXT_IOCTL *, int, int);
static int qla2x00_send_els_passthru(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, fc_port_t *, fc_lun_t *, int);
static int qla2x00_send_fcct(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, fc_port_t *, fc_lun_t *, int);
static int qla2x00_ioctl_ms_queuecommand(scsi_qla_host_t *, EXT_IOCTL *,
    struct scsi_cmnd *, fc_port_t *, fc_lun_t *, EXT_ELS_PT_REQ *);
static int qla2x00_start_ms_cmd(scsi_qla_host_t *, EXT_IOCTL *, srb_t *,
    EXT_ELS_PT_REQ *);
static int qla2x00_get_driver_specifics(EXT_IOCTL *, scsi_qla_host_t *);
static int qla2x00_get_rnid_params(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_led_state(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_set_host_data(scsi_qla_host_t *, EXT_IOCTL *, int);
static int qla2x00_get_dcbx_params(scsi_qla_host_t *, EXT_IOCTL *, int);

static fc_lun_t * qla2x00_add_lun(fc_port_t *, uint64_t);
static int qla2x00_report_lun(scsi_qla_host_t *, fc_port_t *);
static void qla2x00_lun_discovery(scsi_qla_host_t *, fc_port_t *);
static int qla2x00_rpt_lun_discovery(scsi_qla_host_t *, fc_port_t *,
    inq_cmd_rsp_t *, dma_addr_t);
static fc_lun_t *qla2x00_cfg_lun(scsi_qla_host_t *, fc_port_t *, uint16_t,
    inq_cmd_rsp_t *, dma_addr_t);
static int qla2x00_inquiry(scsi_qla_host_t *, fc_port_t *, uint16_t,
    inq_cmd_rsp_t *, dma_addr_t);

static int apidev_major;

#ifdef CONFIG_COMPAT
/* Keeping the name as unlocked to be inline with the linux implementation. */
static long
apidev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return qla2x00_ioctl(NULL, (int)cmd, (void*)arg);
}
#endif	
static int
apidev_ioctl(struct inode *inode, struct file *fp, unsigned int cmd,
    unsigned long arg)
{
	return qla2x00_ioctl(NULL, (int)cmd, (void*)arg);
}

static struct file_operations apidev_fops = {
	.owner = THIS_MODULE,
	.ioctl = apidev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = apidev_unlocked_ioctl,
#endif	
};

int
qla2x00_ioctl_init(void)
{
	apidev_major = register_chrdev(0, QLA2XXX_DRIVER_NAME, &apidev_fops);
	if (apidev_major < 0) {
		DEBUG(printk("%s(): Unable to register CHAR device (%d)\n",
		    __func__, apidev_major));

		return apidev_major;
	}
	return 0;
}

int
qla2x00_ioctl_exit(void)
{
	unregister_chrdev(apidev_major, QLA2XXX_DRIVER_NAME);
	return 0;
}

void *
Q64BIT_TO_PTR(uint64_t buf_addr, uint16_t addr_mode)
{
#if (defined(CONFIG_COMPAT) && !defined(CONFIG_IA64)) || !defined(CONFIG_64BIT)
	union ql_doublelong {
		struct {
			uint32_t	lsl;
			uint32_t	msl;
		} longs;
		uint64_t	dl;
	};

	union ql_doublelong tmpval;

	tmpval.dl = buf_addr;

#if defined(CONFIG_COMPAT) && !defined(CONFIG_IA64)
	/* 32bit user - 64bit kernel */
	if (addr_mode == EXT_DEF_ADDR_MODE_32) {
		DEBUG9(printk("%s: got 32bit user address.\n", __func__));
		return((void *)(uint64_t)(tmpval.longs.lsl));
	} else {
		DEBUG9(printk("%s: got 64bit user address.\n", __func__));
		return((void *)buf_addr);
	}
#else
	return((void *)(tmpval.longs.lsl));
#endif
#else
	return((void *)buf_addr);
#endif
}

/*
 * qla2x00_scsi_pt_done
 *
 * Description:
 *   Resets ioctl progress flag and wakes up the ioctl completion semaphore.
 *
 * Input:
 *   pscsi_cmd - pointer to the passthru Scsi cmd structure which has completed.
 *
 * Returns:
 */
static void
qla2x00_scsi_pt_done(struct scsi_cmnd *pscsi_cmd)
{
	struct Scsi_Host *host = NULL;
	scsi_qla_host_t  *vha = NULL;

	host = pscsi_cmd->device->host;
	vha = (scsi_qla_host_t *) host->hostdata;

	DEBUG9(printk("%s post function called OK\n", __func__));

	/* save detail status for IOCTL reporting */
	vha->ioctl->SCSIPT_InProgress = 0;
	vha->ioctl->ioctl_tov = 0;
	vha->ioctl_err_cmd = NULL;

	up(&vha->ioctl->cmpl_sem);

	DEBUG9(printk("%s: exiting.\n", __func__));

	return;
}

/*
 * qla2x00_msiocb_done
 *
 * Description:
 *   Resets MSIOCB ioctl progress flag and wakes up the ioctl completion
 *   semaphore.
 *
 * Input:
 *   cmd - pointer to the passthru Scsi cmd structure which has completed.
 *
 * Returns:
 */
static void
qla2x00_msiocb_done(struct scsi_cmnd *pscsi_cmd)
{
	struct Scsi_Host *host;
	scsi_qla_host_t  *vha;

	host = pscsi_cmd->device->host;
	vha = (scsi_qla_host_t *) host->hostdata;

	DEBUG9(printk("%s post function called OK\n", __func__));

	vha->ioctl->MSIOCB_InProgress = 0;
	vha->ioctl->ioctl_tov = 0;

	up(&vha->ioctl->cmpl_sem);

	DEBUG9(printk("%s: exiting.\n", __func__));
		
	return;
}

/**************************************************************************
*   qla2x00_cmd_timeout
*
* Description:
*       Handles the command if it times out in any state.
*
* Input:
*     sp - pointer to validate
*
* Returns:
* None.
**************************************************************************/
void
qla2x00_cmd_timeout(srb_t *sp)
{
	int cnt = 0;
	scsi_qla_host_t *vis_ha = NULL;
	struct scsi_cmnd *cmd = NULL;
	unsigned long flags;
	fc_port_t *fcport = NULL;
	srb_t *find_sp = NULL;
	struct req_que *req;

	DEBUG3(printk("%s() Entering. \n",__func__));
	if (!sp) {
		qla_printk(KERN_WARNING, sp->fcport->vha->hw,
			"Command Timeout: sp is NULL");
		return;
	}

	cmd = sp->cmd;
	if (!cmd) {
		qla_printk(KERN_WARNING, sp->fcport->vha->hw,
		    "Command Timeout: command is NULL, already returned "
		    "sp=%p flags=%x \n", sp, sp->flags);
		return;
	}

	vis_ha = (scsi_qla_host_t *)sp->fcport->vha;
	req = vis_ha->hw->req_q_map[0];

	DEBUG2(printk("scsi(%ld): Command timeout: sp=%p sp->flags=%x\n",
	    vis_ha->host_no, sp, sp->flags));

	fcport = sp->fcport;
	if (!fcport) {
	/* XXX Call off cleanup? */	
		return;
	}

	/* Cleaning up the timed out command */
	spin_lock_irqsave(&vis_ha->hw->hardware_lock, flags);
	/* Search the command to be terminated from the outstanding
	 * command list. 
	 */
	for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		find_sp = req->outstanding_cmds[cnt];
		if (find_sp == sp) {
			req->outstanding_cmds[cnt] = NULL;
		}
	}
	spin_unlock_irqrestore(&vis_ha->hw->hardware_lock, flags);

	/*
	 *  SV : Setting command completion status
	 * If FC_DEVICE is marked as dead return the cmd with
	 * DID_NO_CONNECT status.  Otherwise set the host_byte to
	 * DID_BUS_BUSY for the application to retry this cmd.
	 */
	if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD ||
		atomic_read(&vis_ha->loop_state) == LOOP_DEAD) {
		cmd->result = DID_NO_CONNECT << 16;
		CMD_SET_COMPL_STATUS(cmd, CS_PORT_UNAVAILABLE);
	} else {
		if (host_byte(cmd->result) != DID_NO_CONNECT)
			cmd->result = DID_IMM_RETRY << 16;
		CMD_SET_COMPL_STATUS(cmd, CS_PORT_BUSY);
	}

	/* Making sure that the response doesn't process the sense buffer */
	CMD_SET_SNSLEN(cmd, 0);

	/* save detail status for IOCTL reporting */
	if (vis_ha->ioctl->SCSIPT_InProgress)	
		vis_ha->ioctl->SCSIPT_InProgress = 0;
	vis_ha->ioctl->ioctl_tov = 0;
	vis_ha->ioctl_err_cmd = NULL;

	/* Notifying that the command execution is done */
	up(&vis_ha->ioctl->cmpl_sem);

	DEBUG3(printk("%s() Leaving. \n",__func__));
	return;
}

/**************************************************************************
*   qla2xxx_check_if_dpc_dependent
*
* Description:
*       Checks if an IOCTl call needs to wait for the dpc thread to
*       complete, in case it is in progress when a call is made.
*
* Input:
*     cmd - IOCTL command.
*     pext - Data structure associated with the command.
*
* Returns:
* 	1 if dpc dependent.
* 	0 if dpc independent.
**************************************************************************/
int qla2xxx_check_if_dpc_dependent(int cmd, EXT_IOCTL *pext)
{
	int ret = 1;

	switch (cmd) {
		case EXT_CC_QUERY:
			switch (pext->SubCode) {
				case EXT_SC_QUERY_HBA_NODE:
				case EXT_SC_QUERY_CHIP:
				case EXT_SC_QUERY_DISC_LUN:
					/*XXX*/
				case EXT_SC_QUERY_HBA_PORT:
				case EXT_SC_QUERY_DISC_PORT:
				case EXT_SC_QUERY_DISC_TGT:
				case EXT_SC_QUERY_ADAPTER_VERSIONS:
					ret = 0;
					break;

				default:
					ret = 1;
					break;
			}
			break;

		case EXT_CC_GET_DATA:
			switch (pext->SubCode) {
				case EXT_SC_QUERY_DRIVER:
				case EXT_SC_QUERY_FW:
					/*XXX*/
				case EXT_SC_GET_STATISTICS:
				case EXT_SC_GET_FC_STATISTICS:
				case EXT_SC_GET_PORT_SUMMARY:
				case EXT_SC_GET_RNID:
				case EXT_SC_GET_LUN_BY_Q:
				case EXT_SC_GET_BEACON_STATE:
					ret = 0;
					break;

				default:
					ret = 1;
					break;
			}
			break;

		case EXT_CC_REG_AEN:
		case EXT_CC_GET_AEN:
		case INT_CC_GET_OPTION_ROM_LAYOUT:
		case INT_CC_GET_SFP_DATA:
		case INT_CC_PORT_PARAM:
//		case INT_CC_READ_NVRAM:
//		case INT_CC_GET_VPD:
			ret = 0;
			break;

		default:
			ret = 1;
			break;
	}

	return ret;
}

/*************************************************************************
 * qla2x00_ioctl
 *
 * Description:
 *   Performs additional ioctl requests not satisfied by the upper levels.
 *
 * Returns:
 *   ret  = 0    Success
 *   ret != 0    Failed; detailed status copied to EXT_IOCTL structure
 *               if possible
 *************************************************************************/
int
qla2x00_ioctl(struct scsi_device *dev, int cmd, void *arg)
{
	int		mode = 0;
	int		tmp_rval = 0;
	int		ret = -EINVAL;
	uint8_t		*temp;
	uint8_t		tempbuf[8];
	uint32_t	i, status;
	unsigned long   flags;
	int current_wait = 0, retries = 0;

	scsi_qla_host_t	*vha;
	EXT_IOCTL	*pext;


	DEBUG9(printk("%s: entry to command (%x), arg (%p)\n",
	    __func__, cmd, arg));

	/* Catch any non-exioct ioctls */
	if (_IOC_TYPE(cmd) != QLMULTIPATH_MAGIC) {
		return (ret);
	}

	/* Allocate ioctl structure buffer to support multiple concurrent
	 * entries.
	 */
	pext = kmalloc(sizeof(EXT_IOCTL), GFP_KERNEL);
	if (pext == NULL) {
		/* error */
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ERROR in main ioctl buffer allocation.\n"));
		return (-ENOMEM);
	}

	/* copy in application layer EXT_IOCTL */
	ret = copy_from_user(pext, arg, sizeof(EXT_IOCTL));
	if (ret) {
		DEBUG9_10(printk("%s: ERROR COPY_FROM_USER "
		    "EXT_IOCTL sturct. cmd=%x arg=%p.\n",
		    __func__, cmd, arg));

		kfree(pext);
		return (ret);
	}

	/* check signature of this ioctl */
	temp = (uint8_t *) &pext->Signature;

	for (i = 0; i < 4; i++, temp++)
		tempbuf[i] = *temp;

	if ((tempbuf[0] == 'Q') && (tempbuf[1] == 'L') &&
	    (tempbuf[2] == 'O') && (tempbuf[3] == 'G'))
		status = 0;
	else
		status = 1;

	if (status != 0) {
		DEBUG9_10(printk("%s: signature did not match. "
		    "cmd=%x arg=%p.\n", __func__, cmd, arg));
		pext->Status = EXT_STATUS_INVALID_PARAM;
		ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

		kfree(pext);
		return (ret);
	}

	/* check version of this ioctl */
	if (pext->Version > EXT_VERSION) {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ioctl interface version not supported = %d.\n",
		    pext->Version));

		kfree(pext);
		return (-EINVAL);
	}

	/* check for special cmds used during application's setup time. */
	switch (cmd) {
	case EXT_CC_GET_HBA_CNT:
		DEBUG9(printk("%s: got startioctl command.\n", __func__));

		pext->Instance = num_hosts;
		pext->Status = EXT_STATUS_OK;
		ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

		kfree(pext);
		return (ret);

	case EXT_CC_SETINSTANCE:
		/* This call is used to return the HBA's host number to
		 * ioctl caller.  All subsequent ioctl commands will put
		 * the host number in HbaSelect field to tell us which
		 * HBA is the destination.
		 */
		if (pext->Instance < MAX_HBAS) {
			/*
			 * Return host number via pext->HbaSelect for
			 * specified API instance number.
			 */
			if (qla2x00_find_curr_host(pext->Instance, &vha) != 0) {
				pext->Status = EXT_STATUS_DEV_NOT_FOUND;
				ret = copy_to_user(arg, pext,
				    sizeof(EXT_IOCTL));
				DEBUG9_10(printk("%s: SETINSTANCE invalid inst "
				    "%d. num_hosts=%d vha=%p ret=%d.\n",
				    __func__, pext->Instance, num_hosts, vha,
				    ret));

				kfree(pext);
				return (ret); /* ioctl completed ok */
			}

			pext->HbaSelect = vha->host_no;
			pext->Status = EXT_STATUS_OK;

			DEBUG9(printk("%s: Matching instance %d to hba "
			    "%ld.\n", __func__, pext->Instance, vha->host_no));
			/* Drop vport reference */
			spin_lock_irqsave(&(vha->hw->vport_lock), flags);
			qla2xxx_vha_put(vha);
			spin_unlock_irqrestore(&(vha->hw->vport_lock), flags);
		} else {
			DEBUG9_10(printk("%s: ERROR EXT_SETINSTANCE."
			    " Instance=%d num_hosts=%d\n",
			    __func__, pext->Instance, num_hosts));
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		}
		ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));
		kfree(pext);

		DEBUG9(printk("%s: SETINSTANCE exiting. ret=%d.\n",
		    __func__, ret));

		return (ret);

	case EXT_CC_DRIVER_SPECIFIC:
		if (qla2x00_find_curr_host(pext->HbaSelect, &vha) != 0) {
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;
			ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));
		} else {
			ret = qla2x00_get_driver_specifics(pext, vha);
			tmp_rval = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

			/* Drop vport reference */
			spin_lock_irqsave(&(vha->hw->vport_lock), flags);
			qla2xxx_vha_put(vha);
			spin_unlock_irqrestore(&(vha->hw->vport_lock), flags);
			if (!ret)
				ret = tmp_rval;
		}

		kfree(pext);
		return (ret);

	default:
		break;
	}

	/* Use HbaSelect value to get a matching vha instance
	 * for this ioctl command.
	 */
	if (qla2x00_find_curr_host(pext->HbaSelect, &vha) != 0) {

		DEBUG9_10(printk("%s: ERROR matching pext->HbaSelect "
		    "%d to an HBA Instance.\n",
		    __func__, pext->HbaSelect));

		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

		kfree(pext);
		return (ret);
	}

	DEBUG9(printk("%s: active host_inst=%ld CC=%x SC=%x.\n",
	    __func__, vha->instance, cmd, pext->SubCode));

	/*
	 * Get permission to process ioctl command. Only one will proceed
	 * at a time.
	 */
	if (qla2x00_down_timeout(&vha->ioctl->access_sem,
				QLA_IOCTL_ACCESS_WAIT_TIME * HZ) != 0) {

		/* Drop vport reference */
		spin_lock_irqsave(&(vha->hw->vport_lock), flags);
		qla2xxx_vha_put(vha);
		spin_unlock_irqrestore(&(vha->hw->vport_lock), flags);

		/* error timed out */
		DEBUG9_10(printk("%s: ERROR timeout getting ioctl "
		    "access. host no=%d.\n", __func__, pext->HbaSelect));

		pext->Status = EXT_STATUS_BUSY;
		ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

		kfree(pext);
		return (ret);
	}

#define MAX_WAIT 10
#define MAX_RETRIES 6	
	while (vha->dpc_active && qla2xxx_check_if_dpc_dependent(cmd, pext)) {
		if (signal_pending(current))
			break;   /* get out */

		current_wait++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
		if (current_wait >= MAX_WAIT) {
			/* Wait longer if abort-isp is active */
			if(test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) && retries++ < MAX_RETRIES) {
				/* Resetting grace period during abort-isp */
				current_wait = 0;
				continue;
			}
			pext->Status = EXT_STATUS_BUSY;
			ret = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

			up(&vha->ioctl->access_sem);
			spin_lock_irqsave(&vha->hw->vport_lock, flags);
			qla2xxx_vha_put(vha);
			spin_unlock_irqrestore(&vha->hw->vport_lock, flags);
			kfree(pext);
			return (ret);
		}
	}

	switch (cmd) { /* switch on EXT IOCTL COMMAND CODE */

	case EXT_CC_QUERY:
		ret = qla2x00_query(vha, pext, 0);

		break;
	case EXT_CC_GET_DATA:
		ret = qla2x00_get_data(vha, pext, 0);

		break;

	case EXT_CC_SEND_SCSI_PASSTHRU:
		ret = qla2x00_scsi_passthru(vha, pext, mode);
		break;

	case EXT_CC_REG_AEN:
		ret = qla2x00_aen_reg(vha, pext, mode);

		break;

	case EXT_CC_GET_AEN:
		ret = qla2x00_aen_get(vha, pext, mode);

		break;

	case EXT_CC_WWPN_TO_SCSIADDR:
		ret = qla2x00_wwpn_to_scsiaddr(vha, pext, 0);
		break;

	case EXT_CC_SEND_ELS_PASSTHRU:
		if (IS_QLA2100(vha->hw) || IS_QLA2200(vha->hw))
			goto fail;
		/*FALLTHROUGH*/
	case EXT_CC_SEND_FCCT_PASSTHRU:
		ret = qla2x00_msiocb_passthru(vha, pext, cmd, mode);

		break;

	case EXT_CC_SEND_ELS_RNID:
		ret = qla2x00_send_els_rnid(vha, pext, mode);
		break;

	case EXT_CC_SET_DATA:
		ret = qla2x00_set_host_data(vha, pext, mode);
		break;

	case INT_CC_READ_NVRAM:
		ret = qla2x00_read_nvram(vha, pext, mode);
		break;

	case INT_CC_UPDATE_NVRAM:
		ret = qla2x00_update_nvram(vha, pext, mode);
		break;

	case INT_CC_LOOPBACK:
		ret = qla2x00_send_loopback(vha, pext, mode);
		break;

	case INT_CC_READ_OPTION_ROM:
		ret = qla2x00_read_option_rom(vha, pext, mode);
		break;

	case INT_CC_UPDATE_OPTION_ROM:
		ret = qla2x00_update_option_rom(vha, pext, mode);
		break;

	case INT_CC_GET_OPTION_ROM_LAYOUT:
		ret = qla2x00_get_option_rom_layout(vha, pext, mode);
		break;

	case INT_CC_GET_VPD:
		ret = qla2x00_get_vpd(vha, pext, mode);
		break;

	case INT_CC_UPDATE_VPD:
		ret = qla2x00_update_vpd(vha, pext, mode);
		break;

        case INT_CC_GET_SFP_DATA:
		ret = qla2x00_get_sfp_data(vha, pext, mode);
		break;

	case INT_CC_A84_MGMT_COMMAND:
		ret = qla84xx_mgmt_command(vha, pext, mode);
		break;

	case INT_CC_PORT_PARAM:
		ret = qla2x00_update_port_param(vha, pext, mode);
		break;

	case INT_CC_GET_FW_DUMP:
		ret = qla2x00_get_fw_dump(vha, pext, mode);
	break;

	case INT_CC_VF_COMMAND:
		ret = qla2x00_vf_ioctl(vha, pext);
		break;

	case INT_CC_RESET_FW_COMMAND:
		ret = qla2xxx_reset_fw_command(vha, pext, mode);
		break;

	case INT_CC_FCP_PRIO_CFG:
		ret = qla2xxx_fcp_prio_cfg_cmd(vha, pext, mode);
		break;

	case INT_CC_GET_BOARD_TEMP:
		ret = qla2xxx_get_board_temp(vha, pext, mode);
		break;

	default:
	fail:
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		break;

	} /* end of CC decode switch */

	/* Always try to copy values back regardless what happened before. */
	tmp_rval = copy_to_user(arg, pext, sizeof(EXT_IOCTL));

	if (ret == 0)
		ret = tmp_rval;

	DEBUG9(printk("%s: exiting. tmp_rval(%d) ret(%d)\n",
	    __func__, tmp_rval, ret));

	up(&vha->ioctl->access_sem);
	/* Drop vport reference */
	spin_lock_irqsave(&(vha->hw->vport_lock), flags);
	qla2xxx_vha_put(vha);
	spin_unlock_irqrestore(&(vha->hw->vport_lock), flags);

	kfree(pext);
	return (ret);
}

  /*
  * qla2x00_vf_ioctl
  *     Provides functions for virtual fabric ioctl() subcalls.
  *
  * Input:
  *     ha   = adapter state pointer.
  *     pext = Address of application EXT_IOCTL cmd data
  *     mode = flags (currently unused)
  *
  * Returns:
  *     Return value is the ioctl rval_p return value.
  *     0 = success
  *
  * Context:
  *     Kernel context.
  */
 static int
 qla2x00_vf_ioctl(scsi_qla_host_t *vha, EXT_IOCTL *pext)
 {
 	int ret = EXT_STATUS_ERR;

 	uint32_t stat = VP_RET_CODE_FATAL;

 	VF_STRUCT npiv;
 	VF_STRUCT *user = (VF_STRUCT *)pext->RequestAdr;

 	ret = copy_from_user(&npiv, user, sizeof(npiv));
 	if (ret != 0) {
 		DEBUG9(printk("%s: failed copy_from_user:\n", __func__));
 		goto leave;
 	}

 	switch (pext->SubCode) {

 		case VF_SC_VPORT_GETINFO:
 		stat = qla24xx_getinfo_vport(vha, &npiv.u.vp_info, npiv.instance);
 		break;
 	default:
 		DEBUG9(printk("%s: unknown subcode %x:\n", __func__, pext->SubCode));
 		stat = VP_RET_CODE_FATAL;
 		break;
 	}

 	ret = copy_to_user(user, &npiv, sizeof(npiv));
 	if (ret != 0) {
 		DEBUG9(printk("%s: failed copy_to_user:\n", __func__));
	 	goto leave;
 	}

 	pext->Status = stat;

 leave:
 	return ret;
 }
                                                                           

/*
 * qla2x00_find_curr_host
 *	Searches and returns the pointer to the adapter host_no specified.
 *
 * Input:
 *	host_inst = driver internal adapter instance number to search.
 *	ha = adapter state pointer of the instance requested.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_find_curr_host(uint16_t host_inst, scsi_qla_host_t **ret_vha)
{
	int	rval = QLA_SUCCESS;
	int	found;
	struct list_head *hal;
	scsi_qla_host_t *search_vha = NULL;

	/*
 	 * Set ha context for this IOCTL by matching host_no.
	 */
	found = 0;
	mutex_lock(&instance_lock);
	list_for_each(hal, &qla_hostlist) {
		search_vha = list_entry(hal, scsi_qla_host_t, hostlist);

		if (search_vha->instance == host_inst) {
			/* Skip if vport delete is in progress. */
			if (!test_bit(VP_DELETE_ACTIVE, &search_vha->dpc_flags)) {
				qla2xxx_vha_get(search_vha);
				found++;
			}
			break;
		}
	}
	mutex_unlock(&instance_lock);

	if (!found) {
 		DEBUG10(printk("%s: ERROR matching host_inst "
 		    "%d to an HBA Instance.\n", __func__, host_inst));
		rval = QLA_FUNCTION_FAILED;
	} else {
		DEBUG9(printk("%s: found matching host_inst "
		    "%d to an HBA Instance.\n", __func__, host_inst));
		*ret_vha = search_vha;
	}

	return rval;
}

/*
 * qla2x00_alloc_ioctl_mem
 *	Allocates memory needed by IOCTL code.
 *
 * Input:
 *	ha = adapter state pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
int
qla2x00_alloc_ioctl_mem(scsi_qla_host_t *vha)
{
	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_new_ioctl_dma_mem(vha, QLA_INITIAL_IOCTLMEM_SIZE) !=
	    QLA_SUCCESS) {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ERROR in ioctl physical memory allocation\n"));

		return QLA_MEMORY_ALLOC_FAILED;
	}

	/* Allocate context memory buffer */
	vha->ioctl = kmalloc(sizeof(struct hba_ioctl), GFP_KERNEL);
	if (vha->ioctl == NULL) {
		/* error */
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ERROR in ioctl context allocation.\n"));
		return QLA_MEMORY_ALLOC_FAILED;
	}
	memset(vha->ioctl, 0, sizeof(struct hba_ioctl));

	/* Allocate AEN tracking buffer */
	vha->ioctl->aen_tracking_queue =
	    kmalloc(EXT_DEF_MAX_AEN_QUEUE * sizeof(EXT_ASYNC_EVENT), GFP_KERNEL);
	if (vha->ioctl->aen_tracking_queue == NULL) {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ERROR in ioctl aen_queue allocation.\n"));
		return QLA_MEMORY_ALLOC_FAILED;
	}
	memset(vha->ioctl->aen_tracking_queue, 0,
			EXT_DEF_MAX_AEN_QUEUE * sizeof(EXT_ASYNC_EVENT));

	/* Pick the largest size we'll need per ha of all ioctl cmds.
	 * Use this size when freeing.
	 */
	vha->ioctl->scrap_mem = kmalloc(QLA_IOCTL_SCRAP_SIZE, GFP_KERNEL);
	if (vha->ioctl->scrap_mem == NULL) {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: ERROR in ioctl scrap_mem allocation.\n"));
		return QLA_MEMORY_ALLOC_FAILED;
	}
	memset(vha->ioctl->scrap_mem, 0, QLA_IOCTL_SCRAP_SIZE);

	vha->ioctl->scrap_mem_size = QLA_IOCTL_SCRAP_SIZE;
	vha->ioctl->scrap_mem_used = 0;
	DEBUG9(printk("%s(%ld): scrap_mem_size=%d.\n",
	    __func__, vha->host_no, vha->ioctl->scrap_mem_size));

	init_MUTEX(&vha->ioctl->access_sem);

 	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
 	    __func__, vha->host_no, vha->instance));

 	return QLA_SUCCESS;
}

/*
 * qla2x00_free_ioctl_mem
 *	Frees memory used by IOCTL code for the specified ha.
 *
 * Input:
 *	ha = adapter state pointer.
 *
 * Context:
 *	Kernel context.
 */
void
qla2x00_free_ioctl_mem(scsi_qla_host_t *vha)
{
	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (vha->ioctl) {
		kfree(vha->ioctl->scrap_mem);
		vha->ioctl->scrap_mem = NULL;
		vha->ioctl->scrap_mem_size = 0;

		kfree(vha->ioctl->aen_tracking_queue);
		vha->ioctl->aen_tracking_queue = NULL;

		kfree(vha->ioctl);
		vha->ioctl = NULL;
	}

	/* free memory allocated for ioctl operations */
	if (vha->ioctl_mem)
		dma_free_coherent(&vha->hw->pdev->dev, vha->ioctl_mem_size,
		    vha->ioctl_mem, vha->ioctl_mem_phys);
	vha->ioctl_mem = NULL;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

}

/*
 * qla2x00_get_new_ioctl_dma_mem
 *	Allocates dma memory of the specified size.
 *	This is done to replace any previously allocated ioctl dma buffer.
 *
 * Input:
 *	ha = adapter state pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
int
qla2x00_get_new_ioctl_dma_mem(scsi_qla_host_t *vha, uint32_t size)
{
	DEBUG9(printk("%s entered.\n", __func__));

	if (vha->ioctl_mem) {
		DEBUG9(printk("%s: ioctl_mem was previously allocated. "
		    "Dealloc old buffer.\n", __func__));

	 	/* free the memory first */
	 	pci_free_consistent(vha->hw->pdev, vha->ioctl_mem_size,
			 vha->ioctl_mem, vha->ioctl_mem_phys);
	}

	/* Get consistent memory allocated for ioctl I/O operations. */
	vha->ioctl_mem = dma_alloc_coherent(&vha->hw->pdev->dev, size,
	    &vha->ioctl_mem_phys, GFP_KERNEL);
	if (vha->ioctl_mem == NULL) {
		DEBUG9_10(printk(KERN_WARNING
		    "%s: ERROR in ioctl physical memory allocation. "
		    "Requested length=%x.\n", __func__, size));

		vha->ioctl_mem_size = 0;
		return QLA_MEMORY_ALLOC_FAILED;
	}
	vha->ioctl_mem_size = size;

	DEBUG9(printk("%s exiting.\n", __func__));

	return QLA_SUCCESS;
}

/*
 * qla2x00_get_ioctl_scrap_mem
 *	Returns pointer to memory of the specified size from the scrap buffer.
 *	This can be called multiple times before the free call as long
 *	as the memory is to be used by the same ioctl command and
 *	there's still memory left in the scrap buffer.
 *
 * Input:
 *	ha = adapter state pointer.
 *	ppmem = pointer to return a buffer pointer.
 *	size = size of buffer to return.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
int
qla2x00_get_ioctl_scrap_mem(scsi_qla_host_t *vha, void **ppmem, uint32_t size)
{
	int		ret = QLA_SUCCESS;
	uint32_t	free_mem;

	DEBUG9(printk("%s(%ld): inst=%ld entered. size=%d.\n",
	    __func__, vha->host_no, vha->instance, size));

	free_mem = vha->ioctl->scrap_mem_size - vha->ioctl->scrap_mem_used;
	if (free_mem >= size) {
		*ppmem = vha->ioctl->scrap_mem + vha->ioctl->scrap_mem_used;
		vha->ioctl->scrap_mem_used += size;
	} else {
		DEBUG10(printk("%s(%ld): no more scrap memory.\n",
		    __func__, vha->host_no));

		ret = QLA_FUNCTION_FAILED;
	}

	DEBUG9(printk("%s(%ld): exiting. ret=%d.\n",
	    __func__, vha->host_no, ret));

	return (ret);
}

/*
 * qla2x00_free_ioctl_scrap_mem
 *	Makes the entire scrap buffer free for use.
 *
 * Input:
 *	ha = adapter state pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 */
void
qla2x00_free_ioctl_scrap_mem(scsi_qla_host_t *vha)
{
	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	memset(vha->ioctl->scrap_mem, 0, vha->ioctl->scrap_mem_size);
	vha->ioctl->scrap_mem_used = 0;

	DEBUG9(printk("%s(%ld): exiting.\n",
	    __func__, vha->host_no));
}

/*
 * qla2x00_query
 *	Handles all subcommands of the EXT_CC_QUERY command.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int rval = 0;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* All Query type ioctls are done here */
	switch(pext->SubCode) {

	case EXT_SC_QUERY_HBA_NODE:
		/* fill in HBA NODE Information */
		rval = qla2x00_query_hba_node(vha, pext, mode);
		break;

	case EXT_SC_QUERY_HBA_PORT:
		/* return HBA PORT related info */
		rval = qla2x00_query_hba_port(vha, pext, mode);
		break;

	case EXT_SC_QUERY_DISC_PORT:
		/* return discovered port information */
		rval = qla2x00_query_disc_port(vha, pext, mode);
		break;

	case EXT_SC_QUERY_DISC_TGT:
		/* return discovered target information */
		rval = qla2x00_query_disc_tgt(vha, pext, mode);
		break;

	case EXT_SC_QUERY_CHIP:
		rval = qla2x00_query_chip(vha, pext, mode);
		break;

	case EXT_SC_QUERY_DISC_LUN:
		pext->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;

	case EXT_SC_QUERY_CNA_PORT:
		rval = qla2x00_query_cna_port(vha, pext, mode);
		break;

	case EXT_SC_QUERY_ADAPTER_VERSIONS:
		rval = qla2xx_query_adapter_versions(vha, pext);
		break;

	default:
 		DEBUG9_10(printk("%s(%ld): inst=%ld unknown SubCode %d.\n",
 		    __func__, vha->host_no, vha->instance, pext->SubCode));
		pext->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

 	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
 	    __func__, vha->host_no, vha->instance));
	return rval;
}

/*
 * qla2x00_query_hba_node
 *	Handles EXT_SC_QUERY_HBA_NODE subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_hba_node(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint32_t	i, transfer_size;
	EXT_HBA_NODE	*ptmp_hba_node;
	char		*next_str;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_hba_node,
	    sizeof(EXT_HBA_NODE))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_HBA_NODE)));
		return (ret);
	}

	/* fill all available HBA NODE Information */
	for (i = 0; i < 8 ; i++)
		ptmp_hba_node->WWNN[i] = vha->node_name[i];

	sprintf((char *)(ptmp_hba_node->Manufacturer), "QLogic Corporation");
	sprintf((char *)(ptmp_hba_node->Model), ha->model_number);

	ptmp_hba_node->SerialNum[0] = ha->serial0;
	ptmp_hba_node->SerialNum[1] = ha->serial1;
	ptmp_hba_node->SerialNum[2] = ha->serial2;

	/*
	 * For OEM set the serial number in Reserved field the SerialNum field
	 * is of size 4 only, OEM needs at least 12.
	 */
        if (IS_OEM_002(ha))
		memcpy(ptmp_hba_node->Reserved, ha->oem_serial_num,
		    MAX_OEM_SERIAL_LEN);

	sprintf((char *)(ptmp_hba_node->DriverVersion), qla2x00_version_str);
	sprintf((char *)(ptmp_hba_node->FWVersion),"%2d.%02d.%02d",
	    ha->fw_major_version,
	    ha->fw_minor_version,
	    ha->fw_subminor_version);
	DEBUG9_10(printk("%s(%ld): inst=%ld fw ver=%02d.%02d.%02d.\n",
		__func__, vha->host_no, vha->instance,
		ha->fw_major_version, ha->fw_minor_version,
		ha->fw_subminor_version));

	/* Option ROM version string. */
	memset(ptmp_hba_node->OptRomVersion, 0,
	    sizeof(ptmp_hba_node->OptRomVersion));
	next_str = (char *)ptmp_hba_node->OptRomVersion;
	sprintf(next_str, "0.00");
	if (test_bit(ROM_CODE_TYPE_BIOS, &ha->code_types)) {
		sprintf(next_str, "%d.%02d", ha->bios_revision[1],
		    ha->bios_revision[0]);
	}
	/* Extended Option ROM versions. */
	ptmp_hba_node->BIValid = 0;
	memset(ptmp_hba_node->BIEfiVersion, 0,
	    sizeof(ptmp_hba_node->BIEfiVersion));
	memset(ptmp_hba_node->BIFCodeVersion, 0,
	    sizeof(ptmp_hba_node->BIFCodeVersion));
	if (test_bit(ROM_CODE_TYPE_FCODE, &ha->code_types)) {
		if (IS_QLA24XX(ha) || IS_QLA54XX(ha) || 
				IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
			ptmp_hba_node->BIValid |= EXT_HN_BI_FCODE_VALID;
			ptmp_hba_node->BIFCodeVersion[0] = ha->fcode_revision[1];
			ptmp_hba_node->BIFCodeVersion[1] = ha->fcode_revision[0];
		} else {
			unsigned int barray[3];

			memset (barray, 0, sizeof(barray));
			ptmp_hba_node->BIValid |= EXT_HN_BI_FCODE_VALID;
			sscanf((char *)ha->fcode_revision, "%u.%u.%u", &barray[0],
			    &barray[1], &barray[2]);
			ptmp_hba_node->BIFCodeVersion[0] = barray[0];
			ptmp_hba_node->BIFCodeVersion[1] = barray[1];
			ptmp_hba_node->BIFCodeVersion[2] = barray[2];
		}
	}
	if (test_bit(ROM_CODE_TYPE_EFI, &ha->code_types)) {
		ptmp_hba_node->BIValid |= EXT_HN_BI_EFI_VALID;
		ptmp_hba_node->BIEfiVersion[0] = ha->efi_revision[1];
		ptmp_hba_node->BIEfiVersion[1] = ha->efi_revision[0];
	}
	if (IS_QLA24XX(ha) || IS_QLA54XX(ha) || 
	    IS_QLA25XX(ha) || IS_QLA2322(ha) || IS_QLA81XX(ha)) {
		ptmp_hba_node->BIValid |= EXT_HN_BI_FW_VALID;
		ptmp_hba_node->BIFwVersion[0] = ha->fw_revision[0];
		ptmp_hba_node->BIFwVersion[1] = ha->fw_revision[1];
		ptmp_hba_node->BIFwVersion[2] = ha->fw_revision[2];
		ptmp_hba_node->BIFwVersion[3] = ha->fw_revision[3];

		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld fw rev=%04d.%04d.%04d.%04d.\n",
		    __func__, vha->host_no, vha->instance,
		    ha->fw_revision[0], ha->fw_revision[1],
		    ha->fw_revision[2], ha->fw_revision[3]));
	}

	if (vha->vp_idx) {
		ptmp_hba_node->InterfaceType = EXT_DEF_VIRTUAL_FC_INTF_TYPE;
		ptmp_hba_node->PortCount = vha->vp_idx;
	} else {
 		ptmp_hba_node->InterfaceType = EXT_DEF_FC_INTF_TYPE;
		ptmp_hba_node->PortCount = 1;
	}
	ptmp_hba_node->DriverAttr = 0;

	/* now copy up the HBA_NODE to user */
	if (pext->ResponseLen < sizeof(EXT_HBA_NODE))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_HBA_NODE);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_hba_node, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	qla2x00_free_ioctl_scrap_mem(vha);
	return (ret);
}

/*
 * qla2x00_query_hba_port
 *	Handles EXT_SC_QUERY_HBA_PORT subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_hba_port(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint32_t	transfer_size;
	uint32_t	port_cnt;
	fc_port_t	*fcport;
	EXT_HBA_PORT	*ptmp_hba_port;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_hba_port,
	    sizeof(EXT_HBA_PORT))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_HBA_PORT)));
		return (ret);
	}

	/* reflect all HBA PORT related info */
	ptmp_hba_port->WWPN[7] = vha->port_name[7];
	ptmp_hba_port->WWPN[6] = vha->port_name[6];
	ptmp_hba_port->WWPN[5] = vha->port_name[5];
	ptmp_hba_port->WWPN[4] = vha->port_name[4];
	ptmp_hba_port->WWPN[3] = vha->port_name[3];
	ptmp_hba_port->WWPN[2] = vha->port_name[2];
	ptmp_hba_port->WWPN[1] = vha->port_name[1];
	ptmp_hba_port->WWPN[0] = vha->port_name[0];
	ptmp_hba_port->Id[0] = 0;
	ptmp_hba_port->Id[1] = vha->d_id.r.d_id[2];
	ptmp_hba_port->Id[2] = vha->d_id.r.d_id[1];
	ptmp_hba_port->Id[3] = vha->d_id.r.d_id[0];
	ptmp_hba_port->Type =  EXT_DEF_INITIATOR_DEV;

	switch (ha->current_topology) {
	case ISP_CFG_NL:
	case ISP_CFG_FL:
		ptmp_hba_port->Mode = EXT_DEF_LOOP_MODE;
		break;

	case ISP_CFG_N:
	case ISP_CFG_F:
		ptmp_hba_port->Mode = EXT_DEF_P2P_MODE;
		break;

	default:
		ptmp_hba_port->Mode = EXT_DEF_UNKNOWN_MODE;
		break;
	}

	port_cnt = 0;
		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type != FCT_TARGET) {
				DEBUG9_10(printk(
			    	"%s(%ld): inst=%ld port "
			    	"%02x%02x%02x%02x%02x%02x%02x%02x not target dev\n",
			    	__func__, vha->host_no, vha->instance,
			    	fcport->port_name[0], fcport->port_name[1],
			    	fcport->port_name[2], fcport->port_name[3],
			    	fcport->port_name[4], fcport->port_name[5],
			    	fcport->port_name[6], fcport->port_name[7]));
				continue;
			}

			/* if removed or missing */
			if (atomic_read(&fcport->state) != FCS_ONLINE) {
				DEBUG9_10(printk(
			    	"%s(%ld): inst=%ld port "
			    	"%02x%02x%02x%02x%02x%02x%02x%02x not online\n",
			    	__func__, vha->host_no, vha->instance,
			    	fcport->port_name[0], fcport->port_name[1],
			    	fcport->port_name[2], fcport->port_name[3],
			    	fcport->port_name[4], fcport->port_name[5],
			    	fcport->port_name[6], fcport->port_name[7]));
				continue;
			}
			port_cnt++;
		}

	DEBUG9_10(printk("%s(%ld): inst=%ld disc_port cnt=%d.\n",
	    __func__, vha->host_no, vha->instance,
	    port_cnt));
	ptmp_hba_port->DiscPortCount   = port_cnt;
/* Use port count, as only targets are counted in port_cnt */
	ptmp_hba_port->DiscTargetCount = port_cnt;

	if (atomic_read(&vha->loop_state) == LOOP_DOWN ||
	    atomic_read(&vha->loop_state) == LOOP_DEAD) {
		ptmp_hba_port->State = EXT_DEF_HBA_LOOP_DOWN;
	} else if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags)) {

		ptmp_hba_port->State = EXT_DEF_HBA_SUSPENDED;
	} else {
		ptmp_hba_port->State = EXT_DEF_HBA_OK;
	}

	ptmp_hba_port->DiscPortNameType = EXT_DEF_USE_PORT_NAME;

	/* Return supported FC4 type depending on driver support. */
	ptmp_hba_port->PortSupportedFC4Types = EXT_DEF_FC4_TYPE_SCSI;
	ptmp_hba_port->PortActiveFC4Types = EXT_DEF_FC4_TYPE_SCSI;
	if (!IS_QLA2100(ha) && !IS_QLA2200(ha)) {
		ptmp_hba_port->PortSupportedFC4Types |= EXT_DEF_FC4_TYPE_IP;
		ptmp_hba_port->PortActiveFC4Types |= EXT_DEF_FC4_TYPE_IP;
	}

	/* Return supported speed depending on adapter type */
	if (IS_QLA2100(ha) || IS_QLA2200(ha))
		ptmp_hba_port->PortSupportedSpeed = EXT_DEF_PORTSPEED_1GBIT;
	else if (IS_QLA23XX(ha))
		ptmp_hba_port->PortSupportedSpeed = EXT_DEF_PORTSPEED_2GBIT;
	else if(IS_QLA24XX_TYPE(ha))
		ptmp_hba_port->PortSupportedSpeed = EXT_DEF_PORTSPEED_4GBIT;
	else if(IS_QLA25XX(ha))
		ptmp_hba_port->PortSupportedSpeed = EXT_DEF_PORTSPEED_8GBIT;
	else if(IS_QLA81XX(ha))
		ptmp_hba_port->PortSupportedSpeed = EXT_DEF_PORTSPEED_10GBIT;

	switch (ha->link_data_rate) {
	case 0:
		ptmp_hba_port->PortSpeed = EXT_DEF_PORTSPEED_1GBIT;
		break;
	case 1:
		ptmp_hba_port->PortSpeed = EXT_DEF_PORTSPEED_2GBIT;
		break;
	case 3:
		ptmp_hba_port->PortSpeed = EXT_DEF_PORTSPEED_4GBIT;
		break;
	case 4:
		ptmp_hba_port->PortSpeed = EXT_DEF_PORTSPEED_8GBIT;
		break;
	case 0x13:
		ptmp_hba_port->PortSpeed = EXT_DEF_PORTSPEED_10GBIT;
		break;
	default:
		/* unknown */
		ptmp_hba_port->PortSpeed = 0;
		break;
	}

	/* Get the SFP status */
	ptmp_hba_port->LinkState2 = 0;
	if (IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
		uint16_t	mbx[2];
	
		if (qla2x00_get_firmware_state(vha, mbx) == QLA_SUCCESS)
			ptmp_hba_port->LinkState2 = mbx[1];
	}

	if(ha->vmhba_name)
#if defined(__VMKLNX__)
		sscanf(ha->vmhba_name, "vmhba%d", &ptmp_hba_port->OSDeviceName);
#else
		ptmp_hba_port->OSDeviceName = 0;
#endif

	/* now copy up the HBA_PORT to user */
	if (pext->ResponseLen < sizeof(EXT_HBA_PORT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_HBA_PORT);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_hba_port, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return ret;
}

/*
 * qla2x00_query_disc_port
 *	Handles EXT_SC_QUERY_DISC_PORT subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_disc_port(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	int		found;
	uint32_t	transfer_size, inst;
	fc_port_t	*fcport;
	EXT_DISC_PORT	*ptmp_disc_port;

	DEBUG9(printk("%s(%ld): inst=%ld entered. Port inst=%02d.\n",
	    __func__, vha->host_no, vha->instance, pext->Instance));

	inst = 0;
	found = 0;
	fcport = NULL;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		if (atomic_read(&fcport->state) != FCS_ONLINE) {
			/* port does not exist anymore */
			DEBUG9(printk("%s(%ld): fcport marked lost. "
			    "port=%02x%02x%02x%02x%02x%02x%02x%02x "
			    "loop_id=%02x not online.\n",
			    __func__, vha->host_no,
			    fcport->port_name[0], fcport->port_name[1],
			    fcport->port_name[2], fcport->port_name[3],
			    fcport->port_name[4], fcport->port_name[5],
			    fcport->port_name[6], fcport->port_name[7],
			    fcport->loop_id));
			continue;
		}

		if (inst != pext->Instance) {
			DEBUG9(printk("%s(%ld): found fcport %02d "
			    "d_id=%02x%02x%02x. Skipping.\n",
			    __func__, vha->host_no, inst,
			    fcport->d_id.b.domain,
			    fcport->d_id.b.area,
			    fcport->d_id.b.al_pa));

			inst++;
			continue;
		}

		DEBUG9(printk("%s(%ld): inst=%ld found matching fcport %02d "
		    "online. d_id=%02x%02x%02x loop_id=%02x online.\n",
		    __func__, vha->host_no, vha->instance, inst,
		    fcport->d_id.b.domain,
		    fcport->d_id.b.area,
		    fcport->d_id.b.al_pa,
		    fcport->loop_id));

		/* Found the matching port still connected. */
		found++;
		break;
	}

	if (!found) {
		DEBUG9_10(printk("%s(%ld): inst=%ld dev not found.\n",
		    __func__, vha->host_no, vha->instance));

		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_disc_port,
	    sizeof(EXT_DISC_PORT))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_DISC_PORT)));
		return (ret);
	}

	memcpy(ptmp_disc_port->WWNN, fcport->node_name, WWN_SIZE);
	memcpy(ptmp_disc_port->WWPN, fcport->port_name, WWN_SIZE);

	ptmp_disc_port->Id[0] = 0;
	ptmp_disc_port->Id[1] = fcport->d_id.r.d_id[2];
	ptmp_disc_port->Id[2] = fcport->d_id.r.d_id[1];
	ptmp_disc_port->Id[3] = fcport->d_id.r.d_id[0];

	/* Currently all devices on fcport list are target capable devices */
	/* This default value may need to be changed after we add non target
	 * devices also to this list.
	 */
	ptmp_disc_port->Type = EXT_DEF_TARGET_DEV;

	if (fcport->flags & FCF_FABRIC_DEVICE) {
		ptmp_disc_port->Type |= EXT_DEF_FABRIC_DEV;
	}
	if (fcport->flags & FCF_TAPE_PRESENT) {
		ptmp_disc_port->Type |= EXT_DEF_TAPE_DEV;
	}
	if (fcport->port_type == FCT_INITIATOR) {
		ptmp_disc_port->Type |= EXT_DEF_INITIATOR_DEV;
	}

	ptmp_disc_port->LoopID = fcport->loop_id;
	ptmp_disc_port->Status = 0;
	ptmp_disc_port->Bus    = 0;
	ptmp_disc_port->TargetId = fcport->os_target_id;

	/* now copy up the DISC_PORT to user */
	if (pext->ResponseLen < sizeof(EXT_DISC_PORT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_DISC_PORT);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_disc_port, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_query_disc_tgt
 *	Handles EXT_SC_QUERY_DISC_TGT subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_disc_tgt(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint32_t	transfer_size, inst;
	uint32_t	cnt;
	uint16_t	found = 0;
	fc_port_t	*tgt_fcport, *fcport;
	struct list_head	*fcll;
	EXT_DISC_TARGET	*ptmp_disc_target;

	DEBUG9(printk("%s(%ld): inst=%ld entered for tgt inst %d.\n",
	    __func__, vha->host_no, vha->instance, pext->Instance));

	fcport = NULL;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		if (atomic_read(&fcport->state) != FCS_ONLINE) {
			/* port does not exist anymore */
			DEBUG9(printk("%s(%ld): fcport marked lost. "
			    "port=%02x%02x%02x%02x%02x%02x%02x%02x "
			    "loop_id=%02x not online.\n",
			    __func__, vha->host_no,
			    fcport->port_name[0], fcport->port_name[1],
			    fcport->port_name[2], fcport->port_name[3],
			    fcport->port_name[4], fcport->port_name[5],
			    fcport->port_name[6], fcport->port_name[7],
			    fcport->loop_id));
			continue;
		}
		
		inst = fcport->os_target_id;
		if (inst != pext->Instance) {
			DEBUG9(printk("%s(%ld): found fcport %02d "
			    "d_id=%02x%02x%02x. Skipping.\n",
			    __func__, vha->host_no, inst,
			    fcport->d_id.b.domain,
			    fcport->d_id.b.area,
			    fcport->d_id.b.al_pa));

			inst++;
			continue;
		}

		DEBUG9(printk("%s(%ld): inst=%ld found matching fcport %02d "
		    "online. d_id=%02x%02x%02x loop_id=%02x online.\n",
		    __func__, vha->host_no, vha->instance, inst,
		    fcport->d_id.b.domain,
		    fcport->d_id.b.area,
		    fcport->d_id.b.al_pa,
		    fcport->loop_id));

		/* Found the matching port still connected. */
		found++;
		break;
	}

	if (!found) {
		DEBUG9_10(printk("%s(%ld): inst=%ld dev not found.\n",
		    __func__, vha->host_no, vha->instance));

		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_disc_target,
	    sizeof(EXT_DISC_TARGET))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_DISC_TARGET)));
		return (ret);
	}

	tgt_fcport = fcport;
	memcpy(ptmp_disc_target->WWNN, tgt_fcport->node_name, WWN_SIZE);
	memcpy(ptmp_disc_target->WWPN, tgt_fcport->port_name, WWN_SIZE);

	ptmp_disc_target->Id[0] = 0;
	ptmp_disc_target->Id[1] = tgt_fcport->d_id.r.d_id[2];
	ptmp_disc_target->Id[2] = tgt_fcport->d_id.r.d_id[1];
	ptmp_disc_target->Id[3] = tgt_fcport->d_id.r.d_id[0];

	/* All devices on ha->otgt list are target capable devices. */
	ptmp_disc_target->Type = EXT_DEF_TARGET_DEV;

	if (tgt_fcport->flags & FCF_FABRIC_DEVICE) {
		ptmp_disc_target->Type |= EXT_DEF_FABRIC_DEV;
	}
	if (tgt_fcport->flags & FCF_TAPE_PRESENT) {
		ptmp_disc_target->Type |= EXT_DEF_TAPE_DEV;
	}
	if (tgt_fcport->port_type & FCT_INITIATOR) {
		ptmp_disc_target->Type |= EXT_DEF_INITIATOR_DEV;
	}

	ptmp_disc_target->LoopID   = tgt_fcport->loop_id;
	ptmp_disc_target->Status   = 0;
	if (atomic_read(&tgt_fcport->state) != FCS_ONLINE) {
		ptmp_disc_target->Status |= EXT_DEF_TGTSTAT_OFFLINE;
	}

	ptmp_disc_target->Bus      = 0;
	ptmp_disc_target->TargetId = tgt_fcport->os_target_id;

	cnt = 0;

	 /* Do LUN discovery. */
	qla2x00_lun_discovery(vha, tgt_fcport);
	/* enumerate available LUNs under this TGT (if any) */
 	list_for_each(fcll, &tgt_fcport->fcluns) {
		cnt++;
	}
	ptmp_disc_target->LunCount = cnt;

	DEBUG9(printk("%s(%ld): copying data for tgt id %d. ",
	    __func__, vha->host_no, fcport->os_target_id));
	DEBUG9(printk("port=%p:%02x%02x%02x%02x%02x%02x%02x%02x. "
	    "lun cnt=%d.\n",
	    tgt_fcport,
	    tgt_fcport->port_name[0],
	    tgt_fcport->port_name[1],
	    tgt_fcport->port_name[2],
	    tgt_fcport->port_name[3],
	    tgt_fcport->port_name[4],
	    tgt_fcport->port_name[5],
	    tgt_fcport->port_name[6],
	    tgt_fcport->port_name[7],
	    cnt));

	/* now copy up the DISC_PORT to user */
	if (pext->ResponseLen < sizeof(EXT_DISC_PORT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_DISC_TARGET);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_disc_target, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_query_chip
 *	Handles EXT_SC_QUERY_CHIP subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_chip(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	int		pcie_reg;
	uint32_t	pcie_lcap, transfer_size, i;
	uint16_t 	pcie_lstat;
	uint8_t		rev_id;
	EXT_CHIP		*ptmp_isp;
	struct Scsi_Host	*host;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

 	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_isp,
 	    sizeof(EXT_CHIP))) {
 		/* not enough memory */
 		pext->Status = EXT_STATUS_NO_MEMORY;
 		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
 		    "size requested=%ld.\n",
 		    __func__, vha->host_no, vha->instance,
 		    (ulong)sizeof(EXT_CHIP)));
 		return (ret);
 	}

	host = vha->host;
	ptmp_isp->VendorId       = ha->pdev->vendor;
	ptmp_isp->DeviceId       = ha->pdev->device;
	ptmp_isp->SubVendorId    = ha->pdev->subsystem_vendor;
	ptmp_isp->SubSystemId    = ha->pdev->subsystem_device;
	ptmp_isp->PciBusNumber   = ha->pdev->bus->number;
	ptmp_isp->PciDevFunc     = ha->pdev->devfn;
	ptmp_isp->PciSlotNumber  = PCI_SLOT(ha->pdev->devfn);
	ptmp_isp->DomainNr       = pci_domain_nr(ha->pdev->bus);
	/* These values are not 64bit architecture safe. */
	ptmp_isp->IoAddr         = 0; //(UINT32)ha->pio_address;
	ptmp_isp->IoAddrLen      = 0; //(UINT32)ha->pio_length;
	ptmp_isp->MemAddr        = 0; //(UINT32)ha->mmio_address;
	ptmp_isp->MemAddrLen     = 0; //(UINT32)ha->mmio_length;
	ptmp_isp->ChipType       = 0; /* ? */
	ptmp_isp->InterruptLevel = ha->pdev->irq;

	for (i = 0; i < 8; i++)
		ptmp_isp->OutMbx[i] = 0;


	pcie_reg = pci_find_capability(ha->pdev, PCI_CAP_ID_EXP);
	if (pcie_reg) {
		pcie_reg += 0x0c;
		pci_read_config_dword(ha->pdev, pcie_reg, &pcie_lcap);

		pcie_reg += 0x06;
		pci_read_config_word(ha->pdev, pcie_reg, &pcie_lstat);

		ptmp_isp->PcieLinkCap = pcie_lcap;
		ptmp_isp->PcieLinkStat = pcie_lstat;
	}

	ptmp_isp->FlashBlockSize = ha->fdt_block_size;

	pci_read_config_byte(ha->pdev, PCI_CLASS_REVISION, &rev_id);
	ptmp_isp->ChipType = rev_id;

	/* now copy up the ISP to user */
	if (pext->ResponseLen < sizeof(EXT_CHIP))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_CHIP);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_isp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_query_cna_port
 *	Handles EXT_SC_QUERY_CNA_PORT subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_cna_port(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	ret = QLA_SUCCESS;
	uint32_t transfer_size;
	PEXT_CNA_PORT ptmp_cna_port = NULL;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_cna_port,
	    sizeof(EXT_CNA_PORT))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_CNA_PORT)));
		return QLA_FUNCTION_FAILED;
	}

	ptmp_cna_port->VLanId = vha->fcoe_vlan_id;
	memcpy(ptmp_cna_port->VNPortMACAddress, vha->fcoe_vn_port_mac,
			EXT_DEF_MAC_ADDRESS_SIZE);
	ptmp_cna_port->FabricParam = ha->switch_cap;

	if (pext->ResponseLen < sizeof(EXT_CNA_PORT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_CNA_PORT);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    ptmp_cna_port, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_query_adapter_versions
 *	Handles EXT_SC_QUERY_ADAPTER_VERSIONS subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
#define NO_OF_VERSIONS	1
static int
qla2xx_query_adapter_versions(scsi_qla_host_t *vha, EXT_IOCTL *pext) 
{
	int	ret = QLA_SUCCESS;
	uint8_t mpi_cap = 0;
	uint32_t transfer_size = 0;
	uint32_t adapterdata_size = 0;
	PEXT_ADAPTERREGIONVERSION padapter_version = NULL;
	struct qla_hw_data *ha = vha->hw;

	VMK_ASSERT(vha != NULL);
	VMK_ASSERT(pext != NULL);

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* Sizeof (Length + Reserved) = 8 Bytes*/
	/* 8142s do not have a EDC PHY firmware. */
	mpi_cap = (uint8_t)(ha->mpi_capabilities >> 8);
	if(mpi_cap == MPI_CAP_LIST_QLE8142 || mpi_cap  == MPI_CAP_LIST_QMI8142)
		adapterdata_size = (sizeof(EXT_REGIONVERSION) * NO_OF_VERSIONS) + 8;
	else
		adapterdata_size = (sizeof(EXT_REGIONVERSION) * (NO_OF_VERSIONS + 1)) + 8;
 

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&padapter_version,
	    adapterdata_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)adapterdata_size));
		return QLA_FUNCTION_FAILED;
	}

	padapter_version->Length = NO_OF_VERSIONS;
	/* MPI Version */
	padapter_version->RegionVersion[0].Region = INT_OPT_ROM_REGION_MPI_RISC_FW;
	memcpy(padapter_version->RegionVersion[0].Version, ha->mpi_version, 3);
	padapter_version->RegionVersion[0].VersionLength = 3;
	padapter_version->RegionVersion[0].Location = RUNNING_VERSION;
	if(!(mpi_cap == MPI_CAP_LIST_QLE8142 || mpi_cap  == MPI_CAP_LIST_QMI8142)) {
		/* Phy Version */
		padapter_version->RegionVersion[1].Region = INT_OPT_ROM_REGION_EDC_PHY_FW;
		memcpy(padapter_version->RegionVersion[1].Version, ha->phy_version, 3);
		padapter_version->RegionVersion[1].VersionLength = 3;
		padapter_version->RegionVersion[1].Location = RUNNING_VERSION;
		padapter_version->Length = NO_OF_VERSIONS + 1;
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	if (pext->ResponseLen < adapterdata_size) {
		//Calculate the No. of valid versions being returned.
		padapter_version->Length = 
			(pext->ResponseLen - 8)/sizeof(EXT_REGIONVERSION);
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		pext->DetailStatus = EXT_STATUS_BUFFER_TOO_SMALL;
		transfer_size = pext->ResponseLen;
	} else
		transfer_size = adapterdata_size;

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    padapter_version, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_get_data
 *	Handles all subcommands of the EXT_CC_GET_DATA command.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_data(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	tmp_rval = 0;

	switch(pext->SubCode) {
	case EXT_SC_GET_STATISTICS:
		tmp_rval = qla2x00_get_statistics(vha, pext, mode);
		break;

	case EXT_SC_GET_FC_STATISTICS:
		tmp_rval = qla2x00_get_fc_statistics(vha, pext, mode);
		break;

	case EXT_SC_GET_PORT_SUMMARY:
		tmp_rval = qla2x00_get_port_summary(vha, pext, mode);
		break;

	case EXT_SC_QUERY_DRIVER:
		tmp_rval = qla2x00_query_driver(vha, pext, mode);
		break;

	case EXT_SC_QUERY_FW:
		tmp_rval = qla2x00_query_fw(vha, pext, mode);
		break;

	case EXT_SC_GET_RNID:
		tmp_rval = qla2x00_get_rnid_params(vha, pext, mode);
		break;

	case EXT_SC_GET_LUN_BY_Q:
		tmp_rval = qla2x00_get_tgt_lun_by_q(vha, pext, mode);
		break;

	case EXT_SC_GET_DCBX_PARAM:
		tmp_rval = qla2x00_get_dcbx_params(vha, pext, mode);
		break;

	case EXT_SC_GET_BEACON_STATE:
		if (!IS_QLA2100(vha->hw) && !IS_QLA2200(vha->hw)) {
			tmp_rval = qla2x00_get_led_state(vha, pext, mode);
			break;
		}
		/*FALLTHROUGH*/
	default:
		DEBUG10(printk("%s(%ld): inst=%ld unknown SubCode %d.\n",
		    __func__, vha->host_no, vha->instance, pext->SubCode));
		pext->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	 }

	return (tmp_rval);
}

/*
 * qla2x00_get_statistics
 *	Issues get_link_status mbx cmd and returns statistics
 *	relavent to the specified adapter.
 *
 * Input:
 *	ha = pointer to adapter struct of the specified adapter.
 *	pext = pointer to EXT_IOCTL structure containing values from user.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_statistics(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	EXT_HBA_PORT_STAT	*ptmp_stat;
	int		ret = 0;
	struct link_statistics	*stat_buf;
	uint8_t		rval;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	transfer_size;
	dma_addr_t 	stat_dma;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	stat_buf = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL, &stat_dma);
	if (stat_buf == NULL) {
		DEBUG2_3_11(printk("%s(%ld): Failed to allocate memory.\n",
		    __func__, vha->host_no));
		return QLA_MEMORY_ALLOC_FAILED;
	}
	memset(stat_buf, 0, DMA_POOL_SIZE);

	/* check on loop down */
	if ((!(IS_FWI2_CAPABLE(ha)) &&
	    atomic_read(&vha->loop_state) != LOOP_READY) ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
	    vha->dpc_active) {

		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		    __func__, vha->host_no, vha->instance));

		goto get_stat_done;		
	}

	if (vha->ioctl_mem_size < sizeof(struct link_statistics)) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, sizeof(struct link_statistics)) !=
		    QLA_SUCCESS) {
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
			    "requested DMA buffer size %Zx.\n",
			    __func__, vha->host_no, vha->instance,
			    sizeof(struct link_statistics)));
			pext->Status = EXT_STATUS_NO_MEMORY;
			goto get_stat_done;
		}
	}
	/* clear ioctl_mem to be used */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);

	/* Send mailbox cmd to get more. */
	if (IS_FWI2_CAPABLE(ha)) 
		rval = qla24xx_get_isp_stats(vha, stat_buf, stat_dma); 
	else
		rval = qla2x00_get_link_status(vha, vha->loop_id, stat_buf, stat_dma);

	if (rval != QLA_SUCCESS) {
		if (rval == BIT_0) {
			pext->Status = EXT_STATUS_NO_MEMORY;
		} else if (rval == BIT_1) {
			pext->Status = EXT_STATUS_MAILBOX;
			pext->DetailStatus = EXT_DSTATUS_NOADNL_INFO;
		} else {
			pext->Status = EXT_STATUS_ERR;
		}

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR mailbox failed. "
		    "rval=%x.\n", __func__, vha->host_no, vha->instance, rval));

		goto get_stat_done;
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_stat,
	    sizeof(EXT_HBA_PORT_STAT))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_HBA_PORT_STAT)));
		goto get_stat_done;
	}

	ptmp_stat->ControllerErrorCount   =  ha->qla_stats.total_isp_aborts;
	ptmp_stat->DeviceErrorCount       =  ha->total_dev_errs;
	ptmp_stat->TotalIoCount           =  ha->total_ios;
	ptmp_stat->TotalMBytes            =  (ha->qla_stats.input_bytes + 
						ha->qla_stats.output_bytes) >> 20;
	ptmp_stat->TotalLipResets         =  ha->total_lip_cnt;
	
	ptmp_stat->TotalLinkFailures               = stat_buf->link_fail_cnt;
	ptmp_stat->TotalLossOfSync                 = stat_buf->loss_sync_cnt;
	ptmp_stat->TotalLossOfSignals              = stat_buf->loss_sig_cnt;
	ptmp_stat->PrimitiveSeqProtocolErrorCount  = stat_buf->prim_seq_err_cnt;
	ptmp_stat->InvalidTransmissionWordCount    = stat_buf->inval_xmit_word_cnt;
	ptmp_stat->InvalidCRCCount                 = stat_buf->inval_crc_cnt;

	if (IS_FWI2_CAPABLE(ha)) {
		ptmp_stat-> TxFrames	= stat_buf->tx_frames;
		ptmp_stat-> RxFrames	= stat_buf->rx_frames;
		ptmp_stat-> NosCount	= stat_buf->nos_rcvd;
		ptmp_stat-> DumpedFrames = stat_buf->dumped_frames;
	}

	/* now copy up the STATISTICS to user */
	if (pext->ResponseLen < sizeof(EXT_HBA_PORT_STAT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_HBA_PORT_STAT);

	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,pext->AddrMode);
	kernel_tmp = (uint8_t *)ptmp_stat;
	ret = copy_to_user(usr_temp, kernel_tmp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		dma_pool_free(ha->s_dma_pool, stat_buf, stat_dma);
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

get_stat_done:
	dma_pool_free(ha->s_dma_pool, stat_buf, stat_dma);
	return (ret);
}

/*
 * qla2x00_get_fc_statistics
 *	Issues get_link_status mbx cmd to the target device with
 *	the specified WWN and returns statistics relavent to the
 *	device.
 *
 * Input:
 *	ha = pointer to adapter struct of the specified device.
 *	pext = pointer to EXT_IOCTL structure containing values from user.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_fc_statistics(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	EXT_HBA_PORT_STAT	*ptmp_stat;
	EXT_DEST_ADDR		addr_struct;
	fc_port_t	*fcport;
	int		port_found;
	struct link_statistics	*stat_buf;
	int		ret = 0;
	uint8_t		rval;
	uint8_t		*usr_temp, *kernel_tmp;
	uint8_t		*req_name;
	uint32_t	transfer_size;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	ret = copy_from_user(&addr_struct, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), pext->RequestLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy req buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	/* find the device's loop_id */
	port_found = 0;
	fcport = NULL;
	switch (addr_struct.DestType) {
	case EXT_DEF_DESTTYPE_WWPN:
		req_name = addr_struct.DestAddr.WWPN;
		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (memcmp(fcport->port_name, req_name,
			    EXT_DEF_WWN_NAME_SIZE) == 0) {
				port_found = 1;
				break;
			}
		}
		break;

	case EXT_DEF_DESTTYPE_WWNN:
	case EXT_DEF_DESTTYPE_PORTID:
	case EXT_DEF_DESTTYPE_FABRIC:
	case EXT_DEF_DESTTYPE_SCSI:
	default:
		pext->Status = EXT_STATUS_INVALID_PARAM;
		pext->DetailStatus = EXT_DSTATUS_NOADNL_INFO;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR Unsupported subcode "
		    "address type.\n", __func__, vha->host_no, vha->instance));
		return (ret);

		break;
	}

	if (!port_found) {
		/* not found */
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		pext->DetailStatus = EXT_DSTATUS_TARGET;
		return (ret);
	}

	/* check on loop down */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
	    vha->dpc_active) {

		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		     __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (vha->ioctl_mem_size < sizeof(struct link_statistics)) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, sizeof(struct link_statistics)) !=
		    QLA_SUCCESS) {
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
			    "requested DMA buffer size %Zx.\n",
			    __func__, vha->host_no, vha->instance,
			    sizeof(struct link_statistics)));
			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}
	/* clear ioctl_mem to be used */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);

	stat_buf = vha->ioctl_mem;
	/* Send mailbox cmd to get more. */
	if ((rval = qla2x00_get_link_status(vha, fcport->loop_id, stat_buf,
			vha->ioctl_mem_phys)) != QLA_SUCCESS) {
		if (rval == BIT_0) {
			pext->Status = EXT_STATUS_NO_MEMORY;
		} else if (rval == BIT_1) {
			pext->Status = EXT_STATUS_MAILBOX;
			pext->DetailStatus = EXT_DSTATUS_NOADNL_INFO;
		} else {
			pext->Status = EXT_STATUS_ERR;
		}

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR mailbox failed. "
		    "rval=%x.\n", __func__, vha->host_no, vha->instance, rval));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptmp_stat,
	    sizeof(EXT_HBA_PORT_STAT))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_HBA_PORT_STAT)));
		return (ret);
	}

	ptmp_stat->ControllerErrorCount   =  ha->qla_stats.total_isp_aborts;
	ptmp_stat->DeviceErrorCount       =  ha->total_dev_errs;
	ptmp_stat->TotalIoCount           =  ha->total_ios;
	ptmp_stat->TotalMBytes            =  (ha->qla_stats.input_bytes + 
						ha->qla_stats.output_bytes) >> 20;
	ptmp_stat->TotalLipResets         =  ha->total_lip_cnt;

	ptmp_stat->TotalLinkFailures               = stat_buf->link_fail_cnt;
	ptmp_stat->TotalLossOfSync                 = stat_buf->loss_sync_cnt;
	ptmp_stat->TotalLossOfSignals              = stat_buf->loss_sig_cnt;
	ptmp_stat->PrimitiveSeqProtocolErrorCount  = stat_buf->prim_seq_err_cnt;
	ptmp_stat->InvalidTransmissionWordCount    = stat_buf->inval_xmit_word_cnt;
	ptmp_stat->InvalidCRCCount                 = stat_buf->inval_crc_cnt;

	/* now copy up the STATISTICS to user */
	if (pext->ResponseLen < sizeof(EXT_HBA_PORT_STAT))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_HBA_PORT_STAT);

	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,pext->AddrMode);
	kernel_tmp = (uint8_t *)ptmp_stat;
	ret = copy_to_user(usr_temp, kernel_tmp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_get_port_summary
 *	Handles EXT_SC_GET_PORT_SUMMARY subcommand.
 *	Returns values of devicedata and dd_entry list.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_port_summary(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	entry_cnt = 0;
	uint32_t	port_cnt = 0;
	uint32_t	top_xfr_size;
	uint32_t	usr_no_of_entries = 0;
	uint32_t	device_types;
	void		*start_of_entry_list;
	fc_port_t	*fcport;

	EXT_DEVICEDATA		*pdevicedata;
	EXT_DEVICEDATAENTRY	*pdd_entry;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pdevicedata,
	    sizeof(EXT_DEVICEDATA))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "pdevicedata requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_DEVICEDATA)));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pdd_entry,
	    sizeof(EXT_DEVICEDATAENTRY))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "pdd_entry requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_DEVICEDATAENTRY)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* Get device types to query. */
	device_types = 0;
	ret = copy_from_user(&device_types, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), sizeof(device_types));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR"
		    "copy_from_user() of struct failed ret=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	/* Get maximum number of entries allowed in response buf */
	usr_no_of_entries = pext->ResponseLen / sizeof(EXT_DEVICEDATAENTRY);

	/* reserve some spaces to be filled in later. */
	top_xfr_size = sizeof(pdevicedata->ReturnListEntryCount) +
	    sizeof(pdevicedata->TotalDevices);

	start_of_entry_list = Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode) +
	    top_xfr_size;

	/* Start copying from devices that exist. */
	ret = qla2x00_get_fcport_summary(vha, pdd_entry, start_of_entry_list,
	    device_types, usr_no_of_entries, &entry_cnt, &pext->Status);

	DEBUG9(printk("%s(%ld): after get_fcport_summary, entry_cnt=%d.\n",
	    __func__, vha->host_no, entry_cnt));

	pdevicedata->ReturnListEntryCount = entry_cnt;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		port_cnt++;
	}
	if (port_cnt > entry_cnt)
		pdevicedata->TotalDevices = port_cnt;
	else
		pdevicedata->TotalDevices = entry_cnt;

	DEBUG9(printk("%s(%ld): inst=%ld EXT_SC_GET_PORT_SUMMARY "
	    "return entry cnt=%d port_cnt=%d.\n",
	    __func__, vha->host_no, vha->instance,
	    entry_cnt, port_cnt));

	/* copy top of devicedata, which is everything other than the
	 * actual entry list data.
	 */
	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,pext->AddrMode);
	kernel_tmp = (uint8_t *)pdevicedata;
	ret = copy_to_user(usr_temp, kernel_tmp, top_xfr_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp "
		    "devicedata buffer=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_get_fcport_summary
 *	Returns port values in user's dd_entry list.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pdd_entry = pointer to a temporary EXT_DEVICEDATAENTRY struct
 *	pstart_of_entry_list = start of user addr of buffer for dd_entry entries
 *	max_entries = max number of entries allowed by user buffer
 *	pentry_cnt = pointer to total number of entries so far
 *	ret_status = pointer to ioctl status field
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_fcport_summary(scsi_qla_host_t *vha, EXT_DEVICEDATAENTRY *pdd_entry,
    void *pstart_of_entry_list, uint32_t device_types, uint32_t max_entries,
    uint32_t *pentry_cnt, uint32_t *ret_status)
{
	int		ret = QLA_SUCCESS;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	b;
	uint32_t	current_offset;
	uint32_t	transfer_size;
	fc_port_t	*fcport;
			
	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (*pentry_cnt >= max_entries)
			break;
		if (fcport->port_type != FCT_TARGET) {
			/* Don't report initiators or broadcast devices. */
			DEBUG9_10(printk("%s(%ld): not reporting non-target "
			    "fcport %02x%02x%02x%02x%02x%02x%02x%02x. "
			    "port_type=%x.\n",
			    __func__, vha->host_no, fcport->port_name[0],
			    fcport->port_name[1], fcport->port_name[2],
			    fcport->port_name[3], fcport->port_name[4],
			    fcport->port_name[5], fcport->port_name[6],
			    fcport->port_name[7], fcport->port_type));
			continue;
		}

		if ((atomic_read(&fcport->state) != FCS_ONLINE)) {
			/* no need to report */
			DEBUG9_10(printk("%s(%ld): not reporting "
			    "fcport %02x%02x%02x%02x%02x%02x%02x%02x. "
			    "state=%i, flags=%02x.\n",
			    __func__, vha->host_no, fcport->port_name[0],
			    fcport->port_name[1], fcport->port_name[2],
			    fcport->port_name[3], fcport->port_name[4],
			    fcport->port_name[5], fcport->port_name[6],
			    fcport->port_name[7], atomic_read(&fcport->state),
			    fcport->flags));
			continue;
		}

		memset(pdd_entry, 0, sizeof(EXT_DEVICEDATAENTRY));

		/* copy from fcport to dd_entry */
		for (b = 0; b < 3 ; b++)
			pdd_entry->PortID[b] = fcport->d_id.r.d_id[2-b];

		if (fcport->flags & FCF_FABRIC_DEVICE) {
			pdd_entry->ControlFlags = EXT_DEF_GET_FABRIC_DEVICE;
		} else {
			pdd_entry->ControlFlags = 0;
		}

		pdd_entry->TargetAddress.Bus    = 0;
		/* Retrieve 'Target' number for port */
		pdd_entry->TargetAddress.Target = fcport->os_target_id;

		memcpy(pdd_entry->NodeWWN, fcport->node_name, WWN_SIZE);
		memcpy(pdd_entry->PortWWN, fcport->port_name, WWN_SIZE);

		pdd_entry->TargetAddress.Lun    = 0;
		pdd_entry->DeviceFlags          = 0;
		pdd_entry->LoopID               = fcport->loop_id;
		pdd_entry->BaseLunNumber        = 0;

		DEBUG9_10(printk("%s(%ld): reporting "
		    "fcport %02x%02x%02x%02x%02x%02x%02x%02x.\n",
		    __func__, vha->host_no, fcport->port_name[0],
		    fcport->port_name[1], fcport->port_name[2],
		    fcport->port_name[3], fcport->port_name[4],
		    fcport->port_name[5], fcport->port_name[6],
		    fcport->port_name[7]));

		current_offset = *pentry_cnt * sizeof(EXT_DEVICEDATAENTRY);

		transfer_size = sizeof(EXT_DEVICEDATAENTRY);

		/* now copy up this dd_entry to user */
		usr_temp = (uint8_t *)pstart_of_entry_list + current_offset;
		kernel_tmp = (uint8_t *)pdd_entry;
	 	ret = copy_to_user(usr_temp, kernel_tmp, transfer_size);
		if (ret) {
			*ret_status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp "
			    "entry list buf=%d.\n",
			    __func__, vha->host_no, vha->instance, ret));
			return (-EFAULT);
		}

		*pentry_cnt += 1;
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_query_driver
 *	Handles EXT_SC_QUERY_DRIVER subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_driver(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	transfer_size;
	EXT_DRIVER	*pdriver_prop;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pdriver_prop,
	    sizeof(EXT_DRIVER))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_DRIVER)));
		return (ret);
	}

	sprintf((char *)pdriver_prop->Version, qla2x00_version_str);
	pdriver_prop->NumOfBus = MAX_BUSES;
	pdriver_prop->TargetsPerBus = MAX_FIBRE_DEVICES;
	pdriver_prop->LunsPerTarget = MAX_LUNS;
	pdriver_prop->MaxTransferLen  = 0xffffffff;
	pdriver_prop->MaxDataSegments = vha->host->sg_tablesize;

	if (vha->hw->flags.enable_64bit_addressing == 1)
		pdriver_prop->DmaBitAddresses = 64;
	else
		pdriver_prop->DmaBitAddresses = 32;

	if (pext->ResponseLen < sizeof(EXT_DRIVER))
		transfer_size = pext->ResponseLen;
	else
		transfer_size = sizeof(EXT_DRIVER);

	/* now copy up the ISP to user */
	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,pext->AddrMode);
	kernel_tmp = (uint8_t *)pdriver_prop;
 	ret = copy_to_user(usr_temp, kernel_tmp, transfer_size);
 	if (ret) {
 		pext->Status = EXT_STATUS_COPY_ERR;
 		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
 		    __func__, vha->host_no, vha->instance));
 		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
 	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

 	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
 	    __func__, vha->host_no, vha->instance));

 	return (ret);
}

/*
 * qla2x00_query_fw
 *	Handles EXT_SC_QUERY_FW subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_query_fw(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
 	int		ret = 0;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	transfer_size;
 	EXT_FW		*pfw_prop;
	struct qla_hw_data *ha = vha->hw;

 	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
 	    __func__, vha->host_no, vha->instance));

 	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pfw_prop,
 	    sizeof(EXT_FW))) {
 		/* not enough memory */
 		pext->Status = EXT_STATUS_NO_MEMORY;
 		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
 		    "size requested=%ld.\n",
 		    __func__, vha->host_no, vha->instance,
 		    (ulong)sizeof(EXT_FW)));
 		return (ret);
 	}

	pfw_prop->Version[0] = ha->fw_major_version;
	pfw_prop->Version[1] = ha->fw_minor_version;
	pfw_prop->Version[2] = ha->fw_subminor_version;

	transfer_size = sizeof(EXT_FW);

	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,pext->AddrMode);
	kernel_tmp = (uint8_t *)pfw_prop;
	ret = copy_to_user(usr_temp, kernel_tmp, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_get_rnid_params
 *	IOCTL to get RNID parameters of the adapter.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_rnid_params(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	int		tmp_rval = 0;
	uint32_t	copy_len;
	uint16_t	mb[MAILBOX_REGISTER_COUNT];

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* check on loop down */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) || vha->dpc_active) {

		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		    __func__, vha->host_no, vha->instance));

		return (ret);
	}

	/* Send command */
	tmp_rval = qla2x00_get_rnid_params_mbx(vha, vha->ioctl_mem_phys,
	    sizeof(EXT_RNID_DATA), &mb[0]);

	if (tmp_rval != QLA_SUCCESS) {
		/* error */
		pext->Status = EXT_STATUS_ERR;

		DEBUG9_10(printk("%s(%ld): inst=%ld cmd FAILED=%x.\n",
		    __func__, vha->host_no, vha->instance, mb[0]));
		return (ret);
	}

	/* Copy the response */
	copy_len = (pext->ResponseLen > sizeof(EXT_RNID_DATA)) ?
	    (uint32_t)sizeof(EXT_RNID_DATA) : pext->ResponseLen;
	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    vha->ioctl_mem, copy_len);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buf\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	pext->ResponseLen = copy_len;
	if (copy_len < sizeof(EXT_RNID_DATA)) {
		pext->Status = EXT_STATUS_DATA_OVERRUN;
		DEBUG9_10(printk("%s(%ld): inst=%ld data overrun. "
		    "exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	} else if (pext->ResponseLen > sizeof(EXT_RNID_DATA)) {
		pext->Status = EXT_STATUS_DATA_UNDERRUN;
		DEBUG9_10(printk("%s(%ld): inst=%ld data underrun. "
		    "exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	} else {
		pext->Status = EXT_STATUS_OK;
		DEBUG9(printk("%s(%ld): inst=%ld exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	}

	return (ret);
}

/*
 * qla2x00_get_tgt_lun_by_q
 *      Get list of enabled luns from all target devices attached to the HBA
 *	by searching through lun queue.
 *
 * Input:
 *      ha = pointer to adapter
 *
 * Return;
 *      0 on success or errno.
 *
 * Context:
 *      Kernel context.
 */
static int
qla2x00_get_tgt_lun_by_q(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	fc_port_t        *fcport;
	fc_lun_t	 *fclun;
	int              ret = 0;
	TGT_LUN_DATA_ENTRY  *entry;
	TGT_LUN_DATA_LIST *u_list, *llist;
	uint8_t		 *u_entry;
	int		 lun_cnt, entry_size, lun_data_list_size;
	int		count = 0;

	DEBUG9(printk("%s: entered.\n", __func__));

	entry_size = (pext->ResponseLen -
			TGT_LUN_DATA_LIST_HEADER_SIZE) / TGT_LUN_DATA_LIST_MAX_ENTRIES;

	lun_data_list_size = TGT_LUN_DATA_LIST_HEADER_SIZE + entry_size;

        lun_cnt = entry_size - (offsetof(TGT_LUN_DATA_ENTRY, Data));
        DEBUG10(printk("(%s) Lun count = %d\n", __func__, lun_cnt));

        /* Lun count must be 256 , 2048, or 4K, multiple of 256 */
        if ((lun_cnt % OLD_MAX_LUNS) != 0) {
                DEBUG9_10(printk("%s: Invalid lun count = %d.\n",
                    __func__, lun_cnt));

                pext->Status = EXT_STATUS_INVALID_REQUEST;
                return (ret);
        }

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&llist,
	    lun_data_list_size)) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%d.\n",
		    __func__, vha->host_no, vha->instance, lun_data_list_size));
		return (-ENOMEM);
	}
	memset(llist, 0, lun_data_list_size);

	entry = &llist->DataEntry[0];

	u_list = (TGT_LUN_DATA_LIST *)Q64BIT_TO_PTR(pext->ResponseAdr,
	    pext->AddrMode);
	u_entry = (uint8_t *)&u_list->DataEntry[0];

	DEBUG9(printk("%s(%ld): entry->Data size=%ld.\n",
	    __func__, vha->host_no, (ulong)sizeof(entry->Data)));

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		if (atomic_read(&fcport->state) != FCS_ONLINE) {
			/* port does not exist anymore */
			DEBUG9(printk("%s(%ld): fcport marked lost. "
			    "port=%02x%02x%02x%02x%02x%02x%02x%02x "
			    "loop_id=%02x not online.\n",
			    __func__, vha->host_no,
			    fcport->port_name[0], fcport->port_name[1],
			    fcport->port_name[2], fcport->port_name[3],
			    fcport->port_name[4], fcport->port_name[5],
			    fcport->port_name[6], fcport->port_name[7],
			    fcport->loop_id));
			continue;
		}
		
		memcpy(entry->PortName, fcport->port_name,
		    EXT_DEF_WWN_NAME_SIZE);
		memcpy(entry->NodeName, fcport->node_name,
		    EXT_DEF_WWN_NAME_SIZE);
		entry->BusNumber = 0;
		entry->TargetId = fcport->os_target_id;

		entry->DevType = EXT_DEF_TARGET_DEV;

		if (fcport->flags & FCF_FABRIC_DEVICE) {
			entry->DevType |= EXT_DEF_FABRIC_DEV;
		}
		if (fcport->flags & FCF_TAPE_PRESENT) {
			entry->DevType |= EXT_DEF_TAPE_DEV;
		}
		if (fcport->port_type & FCT_INITIATOR) {
			entry->DevType |= EXT_DEF_INITIATOR_DEV;
		}

		entry->LoopId   = fcport->loop_id;

		entry->PortId[0] = 0;
		entry->PortId[1] = fcport->d_id.r.d_id[2];
		entry->PortId[2] = fcport->d_id.r.d_id[1];
		entry->PortId[3] = fcport->d_id.r.d_id[0];

		memset(entry->Data, 0, sizeof(entry->Data));

		 /* Do LUN discovery. */
		qla2x00_lun_discovery(vha, fcport);

		list_for_each_entry(fclun, &fcport->fcluns, list) {

			if (fclun->lun >= lun_cnt) {
				continue;
			}

			count++;

			DEBUG9(printk("%s(%ld): lun %d enabled.\n",
			    __func__, vha->host_no, (int)fclun->lun));

			entry->Data[fclun->lun] |= LUN_DATA_ENABLED;
		}

		entry->LunCount = count;

		DEBUG9(printk("%s(%ld): tgt %d lun count=%d.\n",
		    __func__, vha->host_no, fcport->os_target_id, entry->LunCount));

		ret = copy_to_user(u_entry, entry, entry_size);

		if (ret) {
			/* error */
			DEBUG9_10(printk("%s: u_entry %p copy "
			    "error. list->EntryCount=%d.\n",
			    __func__, u_entry, llist->EntryCount));
			pext->Status = EXT_STATUS_COPY_ERR;
			ret = -EFAULT;
			break;
		}

		llist->EntryCount++;

		/* Go to next target */
		u_entry += entry_size;
	}

	DEBUG9(printk("%s: final entry count = %d\n",
	    __func__, llist->EntryCount));

	if (ret == 0) {
		/* copy number of entries */
		ret = copy_to_user(&u_list->EntryCount, &llist->EntryCount,
		    sizeof(llist->EntryCount));
	}

	qla2x00_free_ioctl_scrap_mem(vha);
	DEBUG9(printk("%s: exiting. ret=%d.\n", __func__, ret));

	return ret;
}

/*
 *qla2x00_get_dcbx_params
 *	IOCTL to get QL81xx DCBX parameters
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_dcbx_params(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int ret = 0;
	uint32_t transfer_size = 0;
	struct qla_hw_data *ha = vha->hw;

#define	DCBX_PARAM_BUFFER_SIZE	0x1000

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
				__func__, vha->host_no, vha->instance));

	if (!IS_QLA81XX(ha)) {
		pext->Status = EXT_STATUS_INVALID_REQUEST;
		DEBUG9_10(printk("%s(%ld): inst=%ld not 81xx. exiting.\n",
					__func__, vha->host_no, vha->instance));
		return (ret);
	}

	transfer_size = DCBX_PARAM_BUFFER_SIZE;

	if (pext->ResponseLen < transfer_size) {
		pext->ResponseLen = transfer_size;
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s: ERROR ResponseLen too small.\n",__func__));

		return (ret);
	}

	if (qla2x00_get_new_ioctl_dma_mem(vha, transfer_size) !=
			QLA_SUCCESS) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
					"size requested=%d.\n",
					__func__, vha->host_no, vha->instance,
					transfer_size));
		return (ret);
	}

	ret = qla81xx_get_dcbx_params(vha, vha->ioctl_mem_phys, transfer_size);
	if (ret != QLA_SUCCESS) {
		/* error */
		pext->Status = EXT_STATUS_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR reading DCBX params (%d).\n",
					__func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	ret = copy_to_user((void *)pext->ResponseAdr, vha->ioctl_mem, transfer_size);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
					__func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	pext->ResponseLen = transfer_size;
	pext->Status = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): inst=%ld exiting normally.\n",
				__func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 *qla2x00_get_led_state
 *	IOCTL to get QLA2XXX HBA LED state
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_led_state(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int			ret = 0;
	EXT_BEACON_CONTROL	tmp_led_state;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (pext->ResponseLen < sizeof(EXT_BEACON_CONTROL)) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s: ERROR ResponseLen too small.\n",
		    __func__));

		return (ret);
	}

	if (test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) {
		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	/* Return current state */
	if (vha->hw->beacon_blink_led) {
		tmp_led_state.State = EXT_DEF_GRN_BLINK_ON;
	} else {
		tmp_led_state.State = EXT_DEF_GRN_BLINK_OFF;
	}

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    &tmp_led_state, sizeof(EXT_BEACON_CONTROL));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	pext->Status       = EXT_STATUS_OK;
	pext->DetailStatus = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);

}

static int
qla2x00_ioctl_scsi_queuecommand(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi_cmd, struct scsi_device *pscsi_dev,
    fc_port_t *pfcport, fc_lun_t *pfclun)
{

	int		ret = 0;
	uint8_t		*usr_temp, *kernel_tmp;
	int		tgt_id;	
#if defined(QL_DEBUG_LEVEL_9)
	uint32_t	b, t, l;
#endif

	srb_t *sp = NULL;
	fc_port_t *fcport = pfcport;
	struct qla_hw_data *ha = vha->hw;
#if defined(QL_DEBUG_LEVEL_10) || defined(QL_DEBUG_LEVEL_9)
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);
#endif
	
	DEBUG9(printk("%s(%ld): entered for Physical host=%ld.\n",
	    __func__, vha->host_no, base_vha->host_no));

	/* Assigning the done later. */
	if ((sp = qla2x00_get_new_sp(vha, pfcport, pscsi_cmd, NULL)) == NULL) {

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc sp.\n",
		    __func__, base_vha->host_no, base_vha->instance));

		pext->Status = EXT_STATUS_NO_MEMORY;
		return (QLA_FUNCTION_FAILED);
	}
	switch(pext->SubCode) {
	case EXT_SC_SEND_SCSI_PASSTHRU:
		tgt_id = pscsi_cmd->device->id;

		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type != FCT_TARGET)
				continue;

			if(fcport->os_target_id == tgt_id) { 
				sp->fcport = fcport;
				break;
			}		
		}	

		if (fcport == NULL) {
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;
			DEBUG9_10(printk("%s(%ld): inst=%ld received invalid "
			    "pointers. tgt_id =%d.\n",
			    __func__, vha->host_no, vha->instance, tgt_id));
			mempool_free(sp, ha->srb_mempool);
			return (QLA_FUNCTION_FAILED);
		}
		break;
	case EXT_SC_SEND_FC_SCSI_PASSTHRU:
		if (pfcport == NULL || pfclun == NULL) {
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;
			DEBUG9_10(printk("%s(%ld): inst=%ld received invalid "
			    "pointers. fcport=%p fclun=%p.\n",
			    __func__, vha->host_no, vha->instance, pfcport, pfclun));
			mempool_free(sp, ha->srb_mempool);
			return (QLA_FUNCTION_FAILED);
		}

		break;
	case EXT_SC_SCSI3_PASSTHRU:
		if (pfcport == NULL || pfclun == NULL) {
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;
			DEBUG9_10(printk("%s(%ld): inst=%ld received invalid "
			    "pointers. fcport=%p fclun=%p.\n",
			    __func__,
			    vha->host_no, vha->instance, pfcport, pfclun));
			mempool_free(sp, ha->srb_mempool);
			return (QLA_FUNCTION_FAILED);
		}
		break;
	default:
		break;
	}

	sp->flags             = SRB_IOCTL;

	if (pscsi_cmd->sc_data_direction == DMA_TO_DEVICE) {
		/* sending user data from pext->ResponseAdr to device */
		usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)vha->ioctl_mem;
		ret = copy_from_user(kernel_tmp, usr_temp, pext->ResponseLen);
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy "
			    "failed(%d) on rsp buf.\n",
			    __func__, vha->host_no, vha->instance, ret));
			mempool_free(sp, ha->srb_mempool);
			return (-EFAULT);
		}
	}

	pscsi_cmd->device->host    = vha->host;

	/* mark this as a special delivery and collection command */
	pscsi_cmd->scsi_done = qla2x00_scsi_pt_done;
	pscsi_cmd->device->tagged_supported = 0;
	pscsi_cmd->use_sg               = 0; /* no ScatterGather */
	pscsi_cmd->request_bufflen      = pext->ResponseLen;
	pscsi_cmd->request_buffer       = vha->ioctl_mem;
	if (pscsi_cmd->timeout_per_command == 0)
		pscsi_cmd->timeout_per_command  = ql2xioctltimeout * HZ;

#if defined(QL_DEBUG_LEVEL_9)
	b = pscsi_cmd->device->channel;
	t = pscsi_cmd->device->id;
	l = pscsi_cmd->device->lun;

	printk("\tCDB=%02x %02x %02x %02x; b=%x t=%x l=%x.\n",
	    pscsi_cmd->cmnd[0], pscsi_cmd->cmnd[1], pscsi_cmd->cmnd[2],
	    pscsi_cmd->cmnd[3], b, t, l);
#endif

	/*
	 * Check the status of the port
	 */
	if (atomic_read(&fcport->state) != FCS_ONLINE) {
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD ||
		    atomic_read(&vha->loop_state) == LOOP_DEAD) {
			pscsi_cmd->result = DID_NO_CONNECT << 16;
			DEBUG(printk("TRACE1 %s(%ld): inst=%ld srb(%p) leaked.\n",
				__func__, vha->host_no, vha->instance, sp));
			mempool_free(sp, ha->srb_mempool);
			return (QLA_FUNCTION_FAILED);
		}
		DEBUG(printk("TRACE2 %s(%ld): inst=%ld srb(%p) leaked.\n",
			__func__, vha->host_no, vha->instance, sp));
		mempool_free(sp, ha->srb_mempool);
		return (QLA_BUSY);
	}

	/* set flag to indicate IOCTL SCSI PassThru in progress */
	vha->ioctl->SCSIPT_InProgress = 1;
	vha->ioctl->ioctl_tov = (int)QLA_PT_CMD_DRV_TOV;

	/* prepare for receiving completion. */
	sema_init(&vha->ioctl->cmpl_sem, 0);
	CMD_COMPL_STATUS(pscsi_cmd) = (int) IOCTL_INVALID_STATUS;

	/* send command to adapter */
	DEBUG9(printk("%s(%ld): inst=%ld sending command.\n",
	    __func__, vha->host_no, vha->instance));

	/* Time the command via our standard driver-timer */
	if ((pscsi_cmd->timeout_per_command / HZ) >= ql2xcmdtimermin)
		qla2x00_add_timer_to_cmd(sp,
		    (pscsi_cmd->timeout_per_command / HZ) -
		    QLA_CMD_TIMER_DELTA);
	else
		sp->flags |= SRB_NO_TIMER;

	/* Start IOCTL SCSI command. */
	ret = ha->isp_ops->start_scsi(sp);
	if (ret != QLA_SUCCESS) {
		qla2x00_sp_free_dma(ha, sp);
		mempool_free(sp, ha->srb_mempool);
	}

	DEBUG9(printk("%s(%ld): exiting.\n",
	    __func__, vha->host_no));
	return (ret);
}


/*
 * qla2x00_ioctl_passthru_rsp_handling
 *      Handles the return status for IOCTL passthru commands.
 *
 * Input:
 *      ha = adapter state pointer.
 *      pext = EXT_IOCTL structure pointer.
 *      pscsi_cmd = pointer to scsi command.
 *
 * Returns:
 *      0 = success
 *      others = errno value
 *
 * Context:
 *      Kernel context.
 */
static inline int
qla2x00_ioctl_passthru_rsp_handling(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi_cmd)
{
	int ret = 0;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (vha->ioctl->SCSIPT_InProgress == 1) {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00: scsi%ld ERROR passthru command timeout.\n",
		    vha->host_no));
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		ret = 1;
		return (ret);
	}

	if (CMD_COMPL_STATUS(pscsi_cmd) == (int)IOCTL_INVALID_STATUS) {
		DEBUG9(printk("%s(%ld): inst=%ld ERROR - cmd not completed.\n",
		    __func__, vha->host_no, vha->instance));
		pext->Status = EXT_STATUS_ERR;
		ret = 1;
		return (ret);
	}

	switch (CMD_COMPL_STATUS(pscsi_cmd)) {
	case CS_INCOMPLETE:
	case CS_ABORTED:
	case CS_PORT_UNAVAILABLE:
	case CS_PORT_LOGGED_OUT:
	case CS_PORT_CONFIG_CHG:
	case CS_PORT_BUSY:
	case CS_TIMEOUT:
		DEBUG9_10(printk("%s(%ld): inst=%ld cs err = %x.\n",
		__func__, vha->host_no, vha->instance,
		CMD_COMPL_STATUS(pscsi_cmd)));
		pext->Status = EXT_STATUS_BUSY;
		ret = 1;
		return (ret);
	case CS_RESET:
	case CS_QUEUE_FULL:
		pext->Status = EXT_STATUS_ERR;
		break;
	case CS_DATA_OVERRUN:
		pext->Status = EXT_STATUS_DATA_OVERRUN;
		DEBUG9_10(printk(KERN_INFO
		    "%s(%ld): inst=%ld return overrun.\n",
		    __func__, vha->host_no, vha->instance));
		break;
	case CS_DATA_UNDERRUN:
		pext->Status = EXT_STATUS_DATA_UNDERRUN;
		DEBUG9_10(printk(KERN_INFO
		    "%s(%ld): inst=%ld return underrun.\n",
		    __func__, vha->host_no, vha->instance));
		if (CMD_SCSI_STATUS(pscsi_cmd) & SS_RESIDUAL_UNDER) {
			pext->Status = EXT_STATUS_OK;
		}
		break;
	}

	if (CMD_COMPL_STATUS(pscsi_cmd) == CS_COMPLETE &&
	    CMD_SCSI_STATUS(pscsi_cmd) == 0) {
		DEBUG9_10(printk(KERN_INFO
		    "%s(%ld): Correct completion inst=%ld\n",
		    __func__, vha->host_no, vha->instance));

	} else {
		DEBUG9_10(printk(KERN_INFO "%s(%ld): inst=%ld scsi err. "
		    "host status =0x%x, scsi status = 0x%x.\n",
		    __func__, vha->host_no, vha->instance,
		    CMD_COMPL_STATUS(pscsi_cmd), CMD_SCSI_STATUS(pscsi_cmd)));

		if (CMD_SCSI_STATUS(pscsi_cmd) & SS_CHECK_CONDITION) {
			pext->Status = EXT_STATUS_SCSI_STATUS;
			pext->DetailStatus = CMD_SCSI_STATUS(pscsi_cmd) & 0xff;
		}
	}
	return (ret);
}

/*
 * qla2x00_scsi_passthru
 *	Handles all subcommands of the EXT_CC_SEND_SCSI_PASSTHRU command.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_scsi_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	struct scsi_cmnd *pscsi_cmd = NULL;
	struct scsi_device *pscsi_device = NULL;
	struct request *request = NULL;

	DEBUG9(printk("%s(%ld): entered.\n",
	    __func__, vha->host_no));
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi_cmd,
	    sizeof(struct scsi_cmnd))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct scsi_cmnd)));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi_device,
	    sizeof(struct scsi_device))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct scsi_device)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}
	pscsi_cmd->device = pscsi_device;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&request,
	    sizeof(struct request))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct request)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}
	pscsi_cmd->request = request;
	pscsi_cmd->request->nr_hw_segments = 1;

	switch(pext->SubCode) {
	case EXT_SC_SEND_SCSI_PASSTHRU:
		DEBUG9(printk("%s(%ld): got SCSI passthru cmd.\n",
		    __func__, vha->host_no));
		ret = qla2x00_sc_scsi_passthru(vha, pext, pscsi_cmd,
		    pscsi_device, mode);
		break;
	case EXT_SC_SEND_FC_SCSI_PASSTHRU:
		DEBUG9(printk("%s(%ld): got FC SCSI passthru cmd.\n",
		    __func__, vha->host_no));
		ret = qla2x00_sc_fc_scsi_passthru(vha, pext, pscsi_cmd,
		    pscsi_device, mode);
		break;
	case EXT_SC_SCSI3_PASSTHRU:
		DEBUG9(printk("%s(%ld): got SCSI3 passthru cmd.\n",
		    __func__, vha->host_no));
		ret = qla2x00_sc_scsi3_passthru(vha, pext, pscsi_cmd,
		    pscsi_device, mode);
		break;
	default:
		DEBUG9_10(printk("%s: got invalid cmd.\n", __func__));
		break;
	}

	qla2x00_free_ioctl_scrap_mem(vha);
	DEBUG9(printk("%s(%ld): exiting.\n",
	    __func__, vha->host_no));
	return (ret);
}

/*
 * qla2x00_sc_scsi_passthru
 *	Handles EXT_SC_SEND_SCSI_PASSTHRU subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_sc_scsi_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi_cmd, struct scsi_device *pscsi_device, int mode)
{
	int		ret = 0;
	uint8_t		*usr_temp, *kernel_tmp;
	uint32_t	transfer_len, i;

	EXT_SCSI_PASSTHRU	*pscsi_pass;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (pext->ResponseLen > vha->ioctl_mem_size) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, pext->ResponseLen) !=
		    QLA_SUCCESS) {
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
			    "requested DMA buffer size %x.\n",
			    __func__, vha->host_no, vha->instance,
			    pext->ResponseLen));
			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi_pass,
	    sizeof(EXT_SCSI_PASSTHRU))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_SCSI_PASSTHRU)));
		return (ret);
	}

	/* clear ioctl_mem to be used */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);

	/* Copy request buffer */
	usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode);
	kernel_tmp = (uint8_t *)pscsi_pass;
	ret = copy_from_user(kernel_tmp, usr_temp, 
	    sizeof(EXT_SCSI_PASSTHRU));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	/* set target coordinates */
	pscsi_cmd->device->id = pscsi_pass->TargetAddr.Target;
	pscsi_cmd->device->lun = pscsi_pass->TargetAddr.Lun;

	/* TODO:GSS Verify target exists. */

	/* Copy over cdb */
	if (pscsi_pass->CdbLength == 6) {
		pscsi_cmd->cmd_len = 6;
	} else if (pscsi_pass->CdbLength == 0x0A) {
		pscsi_cmd->cmd_len = 0x0A;
	} else if (pscsi_pass->CdbLength == 0x0C) {
		pscsi_cmd->cmd_len = 0x0C;
	} else {
		DEBUG9_10(printk(KERN_WARNING
		    "%s: Unsupported Cdb Length=%x.\n",
		    __func__, pscsi_pass->CdbLength));

		pext->Status = EXT_STATUS_INVALID_PARAM;
		return (ret);
	}

	memcpy(pscsi_cmd->cmnd, pscsi_pass->Cdb, pscsi_cmd->cmd_len);

	DEBUG9(printk("%s Dump of cdb buffer:\n", __func__));
	DEBUG9(qla2x00_dump_buffer((uint8_t *)&pscsi_cmd->cmnd[0],
	    pscsi_cmd->cmd_len));

	switch (pscsi_pass->Direction) {
	case EXT_DEF_SCSI_PASSTHRU_DATA_OUT:
		pscsi_cmd->sc_data_direction = DMA_TO_DEVICE;
		break;
	case EXT_DEF_SCSI_PASSTHRU_DATA_IN:
		pscsi_cmd->sc_data_direction = DMA_FROM_DEVICE;
		break;
	default :	
		pscsi_cmd->sc_data_direction = DMA_NONE;
		break;
	}

	/* send command to adapter */
	DEBUG9(printk("%s(%ld): inst=%ld sending command.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_ioctl_scsi_queuecommand(vha, pext, pscsi_cmd, 
	    pscsi_device, NULL, NULL)) {
		return (ret);
	}

	DEBUG9(printk("%s(%ld): inst=%ld waiting for completion.\n",
	    __func__, vha->host_no, vha->instance));

	/* Wait for completion */
	down(&vha->ioctl->cmpl_sem);

	DEBUG9(printk("%s(%ld): inst=%ld completed.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_ioctl_passthru_rsp_handling(vha, pext, pscsi_cmd))
		return (ret);

	/* Process completed command */
	DEBUG9(printk("%s(%ld): inst=%ld done. host status=0x%x, "
	    "scsi status=0x%x.\n",
	    __func__, vha->host_no, vha->instance, CMD_COMPL_STATUS(pscsi_cmd),
	    CMD_SCSI_STATUS(pscsi_cmd)));

	/* copy up structure to make sense data available to user */
	pscsi_pass->SenseLength = CMD_ACTUAL_SNSLEN(pscsi_cmd);
	if (CMD_ACTUAL_SNSLEN(pscsi_cmd)) {
		for (i = 0; i < CMD_ACTUAL_SNSLEN(pscsi_cmd); i++)
			pscsi_pass->SenseData[i] = pscsi_cmd->sense_buffer[i];

		DEBUG10(printk("%s Dump of sense buffer:\n", __func__));
		DEBUG10(qla2x00_dump_buffer(
		    (uint8_t *)&pscsi_pass->SenseData[0],
		    CMD_ACTUAL_SNSLEN(pscsi_cmd)));

		usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)pscsi_pass;
		ret = copy_to_user(usr_temp, kernel_tmp,
		    sizeof(EXT_SCSI_PASSTHRU));
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy sense "
			    "buffer.\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	if (pscsi_pass->Direction == EXT_DEF_SCSI_PASSTHRU_DATA_IN) {
		DEBUG9(printk("%s(%ld): inst=%ld copying data.\n",
		    __func__, vha->host_no, vha->instance));

		/* now copy up the READ data to user */
		if ((CMD_COMPL_STATUS(pscsi_cmd) == CS_DATA_UNDERRUN) &&
		    (CMD_RESID_LEN(pscsi_cmd))) {

			transfer_len = pext->ResponseLen -
			    CMD_RESID_LEN(pscsi_cmd);
			pext->ResponseLen = transfer_len;
		} else {
			transfer_len = pext->ResponseLen;
		}

		DEBUG9_10(printk(KERN_INFO
		    "%s(%ld): final transferlen=%d.\n",
		    __func__, vha->host_no, transfer_len));

		usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)vha->ioctl_mem;
		ret = copy_to_user(usr_temp, kernel_tmp, transfer_len);
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk(
			    "%s(%ld): inst=%ld ERROR copy rsp buf\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_sc_fc_scsi_passthru
 *	Handles EXT_SC_SEND_FC_SCSI_PASSTHRU subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_sc_fc_scsi_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pfc_scsi_cmd, struct scsi_device *pfc_scsi_device,
    int mode)
{
	int			ret = 0;
	int			port_found, lun_found;
	fc_lun_t		temp_fclun;
	struct list_head	*fcpl;
	fc_port_t		*fcport;
	struct list_head	*fcll;
	fc_lun_t		*fclun;
	uint8_t			*usr_temp, *kernel_tmp;
	uint32_t		i, transfer_len;

	EXT_FC_SCSI_PASSTHRU	*pfc_scsi_pass;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

#if defined(QL_DEBUG_LEVEL_9) || defined(QL_DEBUG_LEVEL_10)
	if (!pfc_scsi_cmd || !pfc_scsi_device) {
		printk("%s(%ld): invalid pointer received. pfc_scsi_cmd=%p, "
		    "pfc_scsi_device=%p.\n", __func__, vha->host_no,
		    pfc_scsi_cmd, pfc_scsi_device);
		return (ret);
	}
#endif

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pfc_scsi_pass,
	    sizeof(EXT_FC_SCSI_PASSTHRU))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_FC_SCSI_PASSTHRU)));
		return (ret);
	}

	/* clear ioctl_mem to be used */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);

	if (pext->ResponseLen > vha->ioctl_mem_size) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, pext->ResponseLen) !=
		    QLA_SUCCESS) {

			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
			    "requested DMA buffer size %x.\n",
			    __func__, vha->host_no, vha->instance,
			    pext->ResponseLen));

			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}

	/* Copy request buffer */
	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode);
	kernel_tmp = (uint8_t *)pfc_scsi_pass;
	ret = copy_from_user(kernel_tmp, usr_temp,
	    sizeof(EXT_FC_SCSI_PASSTHRU));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	if (pfc_scsi_pass->FCScsiAddr.DestType != EXT_DEF_DESTTYPE_WWPN) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR -wrong Dest type. \n",
		    __func__, vha->host_no, vha->instance));
		ret = EXT_STATUS_ERR;
		return (ret);
	}

	fcport = NULL;
	fclun = NULL;
 	port_found = lun_found = 0;
 	list_for_each(fcpl, &vha->vp_fcports) {
 		fcport = list_entry(fcpl, fc_port_t, list);

		if (memcmp(fcport->port_name,
		    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN, 8) != 0) {
			continue;

		}
 		port_found++;

 		list_for_each(fcll, &fcport->fcluns) {
 			fclun = list_entry(fcll, fc_lun_t, list);
			if (fclun->lun == pfc_scsi_pass->FCScsiAddr.Lun) {
				/* Found the right LUN */
				lun_found++;
				break;
			}
		}
		break;
	}

	if (!port_found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld FC AddrFormat - DID NOT "
		    "FIND Port matching WWPN.\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	/* v5.21b9 - use a temporary fclun */
	if (!lun_found) {
		fclun = &temp_fclun;
		fclun->fcport = fcport;
		fclun->lun = pfc_scsi_pass->FCScsiAddr.Lun;
	}

	/* set target coordinates */
	pfc_scsi_cmd->device->id = 0xff; /* not used. just put something there. */
	pfc_scsi_cmd->device->lun = pfc_scsi_pass->FCScsiAddr.Lun;

	DEBUG9(printk("%s(%ld): inst=%ld cmd for loopid=%04x L=%04x "
	    "WWPN=%02x%02x%02x%02x%02x%02x%02x%02x.\n",
	    __func__, vha->host_no, vha->instance, fclun->fcport->loop_id,
	    pfc_scsi_cmd->device->lun,
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[0],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[1],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[2],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[3],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[4],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[5],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[6],
	    pfc_scsi_pass->FCScsiAddr.DestAddr.WWPN[7]));

	if (pfc_scsi_pass->CdbLength == 6) {
		pfc_scsi_cmd->cmd_len = 6;
	} else if (pfc_scsi_pass->CdbLength == 0x0A) {
		pfc_scsi_cmd->cmd_len = 0x0A;
	} else if (pfc_scsi_pass->CdbLength == 0x0C) {
		pfc_scsi_cmd->cmd_len = 0x0C;
	} else if (pfc_scsi_pass->CdbLength == 0x10) {
		pfc_scsi_cmd->cmd_len = 0x10;
	} else {
		DEBUG9_10(printk(KERN_WARNING
		    "qla2x00_ioctl: FC_SCSI_PASSTHRU Unknown Cdb Length=%x.\n",
		    pfc_scsi_pass->CdbLength));
		pext->Status = EXT_STATUS_INVALID_PARAM;

		return (ret);
	}

	memcpy(pfc_scsi_cmd->cmnd, pfc_scsi_pass->Cdb,
	    pfc_scsi_cmd->cmd_len);

	DEBUG9(printk("%s Dump of cdb buffer:\n", __func__));
	DEBUG9(qla2x00_dump_buffer((uint8_t *)&pfc_scsi_cmd->cmnd[0], 16));

	switch (pfc_scsi_pass->Direction) {
	case EXT_DEF_SCSI_PASSTHRU_DATA_OUT:
		pfc_scsi_cmd->sc_data_direction = DMA_TO_DEVICE;
		break;
	case EXT_DEF_SCSI_PASSTHRU_DATA_IN:
		pfc_scsi_cmd->sc_data_direction = DMA_FROM_DEVICE;
		break;
	default :	
		pfc_scsi_cmd->sc_data_direction = DMA_NONE;
		break;
	}

	/* send command to adapter */
	DEBUG9(printk("%s(%ld): inst=%ld queuing command.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_ioctl_scsi_queuecommand(vha, pext, pfc_scsi_cmd,
	    pfc_scsi_device, fcport, fclun)) {
		return (ret);
	}

	/* Wait for comletion */
	DEBUG9(printk("%s(%ld): inst=%ld waiting for completion.\n",
	    __func__, vha->host_no, vha->instance));
	down(&vha->ioctl->cmpl_sem);

	DEBUG9(printk("%s(%ld): inst=%ld completed.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_ioctl_passthru_rsp_handling(vha, pext, pfc_scsi_cmd))
		return (ret);

	/* Process completed command */
	DEBUG9(printk("%s(%ld): inst=%ld done. host status=0x%x, "
	    "scsi status=0x%x.\n",
	    __func__, vha->host_no, vha->instance, CMD_COMPL_STATUS(pfc_scsi_cmd),
	    CMD_SCSI_STATUS(pfc_scsi_cmd)));

	/* copy up structure to make sense data available to user */
	pfc_scsi_pass->SenseLength = CMD_ACTUAL_SNSLEN(pfc_scsi_cmd);
	if (CMD_ACTUAL_SNSLEN(pfc_scsi_cmd)) {
		for (i = 0; i < CMD_ACTUAL_SNSLEN(pfc_scsi_cmd); i++) {
			pfc_scsi_pass->SenseData[i] =
			pfc_scsi_cmd->sense_buffer[i];
		}


		DEBUG10(printk("%s Dump of sense buffer:\n", __func__));
		DEBUG10(qla2x00_dump_buffer(
		    (uint8_t *)&pfc_scsi_pass->SenseData[0],
		    CMD_ACTUAL_SNSLEN(pfc_scsi_cmd)));
	
		usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)pfc_scsi_pass;
		ret = copy_to_user(usr_temp, kernel_tmp,
		    sizeof(EXT_FC_SCSI_PASSTHRU));
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy sense "
			    "buffer.\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	if (pfc_scsi_pass->Direction == EXT_DEF_SCSI_PASSTHRU_DATA_IN) {
		DEBUG9(printk("%s(%ld): inst=%ld copying data.\n",
		    __func__, vha->host_no, vha->instance));

		/* now copy up the READ data to user */
		if ((CMD_COMPL_STATUS(pfc_scsi_cmd) == CS_DATA_UNDERRUN) &&
		    (CMD_RESID_LEN(pfc_scsi_cmd))) {

			transfer_len = pext->ResponseLen -
			    CMD_RESID_LEN(pfc_scsi_cmd);
			pext->ResponseLen = transfer_len;
		} else {
			transfer_len = pext->ResponseLen;
		}

		usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)vha->ioctl_mem;
		ret = copy_to_user(usr_temp, kernel_tmp, transfer_len);
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk(
			    "%s(%ld): inst=%ld ERROR copy rsp buf\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_sc_scsi3_passthru
 *	Handles EXT_SC_SCSI3_PASSTHRU subcommand.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_sc_scsi3_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi3_cmd, struct scsi_device *pscsi3_device, int mode)
{
#define MAX_SCSI3_CDB_LEN	16

	int			ret = 0;
	int			found;
	fc_lun_t		temp_fclun;
	fc_lun_t		*fclun = NULL;
	struct list_head	*fcpl;
	fc_port_t		*fcport;
	uint8_t			*usr_temp, *kernel_tmp;
	uint32_t		transfer_len;
	uint32_t		i;

	EXT_FC_SCSI_PASSTHRU	*pscsi3_pass;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

#if defined(QL_DEBUG_LEVEL_9) || defined(QL_DEBUG_LEVEL_10)
	if (!pscsi3_cmd || !pscsi3_device) {
		printk("%s(%ld): invalid pointer received. pfc_scsi_cmd=%p, "
		    "pfc_scsi_device=%p.\n", __func__, vha->host_no,
		    pscsi3_cmd, pscsi3_device);
		return (ret);
	}
#endif

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi3_pass,
	    sizeof(EXT_FC_SCSI_PASSTHRU))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_FC_SCSI_PASSTHRU)));
		return (ret);
	}


	/* clear ioctl_mem to be used */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);

	if (pext->ResponseLen > vha->ioctl_mem_size) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, pext->ResponseLen) !=
		    QLA_SUCCESS) {

			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot "
			    "alloc requested DMA buffer size=%x.\n",
			    __func__, vha->host_no, vha->instance,
			    pext->ResponseLen));

			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}

	/* Copy request buffer */
	usr_temp   = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode);
	kernel_tmp = (uint8_t *)pscsi3_pass;
	ret = copy_from_user(kernel_tmp, usr_temp,
	    sizeof(EXT_FC_SCSI_PASSTHRU));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	if (pscsi3_pass->FCScsiAddr.DestType != EXT_DEF_DESTTYPE_WWPN) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR - wrong Dest type.\n",
		    __func__, vha->host_no, vha->instance));
		ret = EXT_STATUS_ERR;
		return (ret);
	}

	/*
	 * For this ioctl command we always assume all 16 bytes are
	 * initialized.
	 */
	if (pscsi3_pass->CdbLength != MAX_SCSI3_CDB_LEN) {
		pext->Status = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR -wrong Cdb Len %d.\n",
		    __func__, vha->host_no, vha->instance,
		    pscsi3_pass->CdbLength));
		return (ret);
	}

 	fcport = NULL;
 	found = 0;
 	list_for_each(fcpl, &vha->vp_fcports) {
 		fcport = list_entry(fcpl, fc_port_t, list);

		if (memcmp(fcport->port_name,
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN, 8) == 0) {
			found++;
			break;
		}
	}
	if (!found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;

		DEBUG9_10(printk("%s(%ld): inst=%ld DID NOT FIND Port for WWPN "
		    "%02x%02x%02x%02x%02x%02x%02x%02x.\n",
		    __func__, vha->host_no, vha->instance,
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[0],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[1],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[2],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[3],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[4],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[5],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[6],
		    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[7]));

		return (ret);
	}

	/* Use a temporary fclun to send out the command. */
	fclun = &temp_fclun;
	fclun->fcport = fcport;
	fclun->lun = pscsi3_pass->FCScsiAddr.Lun;

	/* set target coordinates */
	pscsi3_cmd->device->id = 0xff;  /* not used. just put something there. */
	pscsi3_cmd->device->lun = pscsi3_pass->FCScsiAddr.Lun;

	DEBUG9(printk("%s(%ld): inst=%ld cmd for loopid=%04x L=%04x "
	    "WWPN=%02x%02x%02x%02x%02x%02x%02x%02x.\n",
	    __func__, vha->host_no, vha->instance,
	    fclun->fcport->loop_id, pscsi3_cmd->device->lun,
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[0],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[1],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[2],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[3],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[4],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[5],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[6],
	    pscsi3_pass->FCScsiAddr.DestAddr.WWPN[7]));

	pscsi3_cmd->cmd_len = MAX_SCSI3_CDB_LEN;
	memcpy(pscsi3_cmd->cmnd, pscsi3_pass->Cdb, pscsi3_cmd->cmd_len);

	switch (pscsi3_pass->Direction) {
	case EXT_DEF_SCSI_PASSTHRU_DATA_OUT:
		pscsi3_cmd->sc_data_direction = DMA_TO_DEVICE;
		break;
	case EXT_DEF_SCSI_PASSTHRU_DATA_IN:
		pscsi3_cmd->sc_data_direction = DMA_FROM_DEVICE;
		break;
	default :	
		pscsi3_cmd->sc_data_direction = DMA_NONE;
		break;
	}

 	if (pscsi3_pass->Timeout)
		pscsi3_cmd->timeout_per_command = pscsi3_pass->Timeout * HZ;

	/* send command to adapter */
	DEBUG9(printk("%s(%ld): inst=%ld queuing command.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_ioctl_scsi_queuecommand(vha, pext, pscsi3_cmd,
	    pscsi3_device, fcport, fclun)) {
		return (ret);
	}

	/* Wait for comletion */
	DEBUG9(printk("%s(%ld): inst=%ld waiting for completion.\n",
	    __func__, vha->host_no, vha->instance));
	down(&vha->ioctl->cmpl_sem);

	if (qla2x00_ioctl_passthru_rsp_handling(vha, pext, pscsi3_cmd))
		return (ret);

	/* Process completed command */
	DEBUG9(printk("%s(%ld): inst=%ld done. host status=0x%x, "
	    "scsi status=0x%x.\n",
	    __func__, vha->host_no, vha->instance, CMD_COMPL_STATUS(pscsi3_cmd),
	    CMD_SCSI_STATUS(pscsi3_cmd)));

	/* copy up structure to make sense data available to user */
	pscsi3_pass->SenseLength = CMD_ACTUAL_SNSLEN(pscsi3_cmd);
	if (CMD_ACTUAL_SNSLEN(pscsi3_cmd)) {
		DEBUG9_10(printk("%s(%ld): inst=%ld sense[0]=%x sense[2]=%x.\n",
		    __func__, vha->host_no, vha->instance,
		    pscsi3_cmd->sense_buffer[0],
		    pscsi3_cmd->sense_buffer[2]));

		for (i = 0; i < CMD_ACTUAL_SNSLEN(pscsi3_cmd); i++) {
			pscsi3_pass->SenseData[i] =
			    pscsi3_cmd->sense_buffer[i];
		}

		usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)pscsi3_pass;
		ret = copy_to_user(usr_temp, kernel_tmp,
		    sizeof(EXT_FC_SCSI_PASSTHRU));
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy sense "
			    "buffer.\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	if (pscsi3_pass->Direction == EXT_DEF_SCSI_PASSTHRU_DATA_IN) {

		DEBUG9(printk("%s(%ld): inst=%ld copying data.\n",
		    __func__, vha->host_no, vha->instance));

		/* now copy up the READ data to user */
		if ((CMD_COMPL_STATUS(pscsi3_cmd) == CS_DATA_UNDERRUN) &&
		    (CMD_RESID_LEN(pscsi3_cmd))) {

			transfer_len = pext->ResponseLen -
			    CMD_RESID_LEN(pscsi3_cmd);

			pext->ResponseLen = transfer_len;
		} else {
			transfer_len = pext->ResponseLen;
		}

		DEBUG9_10(printk(KERN_INFO
		    "%s(%ld): final transferlen=%d.\n",
		    __func__, vha->host_no, transfer_len));

		usr_temp = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
		    pext->AddrMode);
		kernel_tmp = (uint8_t *)vha->ioctl_mem;
		ret = copy_to_user(usr_temp, kernel_tmp, transfer_len);
		if (ret) {
			pext->Status = EXT_STATUS_COPY_ERR;
			DEBUG9_10(printk(
			    "%s(%ld): inst=%ld ERROR copy rsp buf\n",
			    __func__, vha->host_no, vha->instance));
			return (-EFAULT);
		}
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}
/*
 * qla2x00_aen_reg
 *	IOCTL management server Asynchronous Event Tracking Enable/Disable.
 *
 * Input:
 *	ha = pointer to the adapter struct of the adapter to register.
 *	cmd = pointer to EXT_IOCTL structure containing values from user.
 *	mode = flags. not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_aen_reg(scsi_qla_host_t *vha, EXT_IOCTL *cmd, int mode)
{
	int		rval = 0;
	EXT_REG_AEN	reg_struct;

	DEBUG9(printk("%s(%ld): inst %ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	rval = copy_from_user(&reg_struct, Q64BIT_TO_PTR(cmd->RequestAdr,
	    cmd->AddrMode), sizeof(EXT_REG_AEN));
	if (rval == 0) {
		cmd->Status = EXT_STATUS_OK;
		if (reg_struct.Enable) {
			vha->ioctl->flags |= IOCTL_AEN_TRACKING_ENABLE;
		} else {
			vha->ioctl->flags &= ~IOCTL_AEN_TRACKING_ENABLE;
		}
	} else {
		DEBUG9(printk("%s(%ld): inst %ld copy error=%d.\n",
		    __func__, vha->host_no, vha->instance, rval));

		cmd->Status = EXT_STATUS_COPY_ERR;
		rval = -EFAULT;
	}

	DEBUG9(printk("%s(%ld): inst %ld reg_struct.Enable(%d) "
	    "ha->ioctl_flag(%x) cmd->Status(%d).",
	    __func__, vha->host_no, vha->instance, reg_struct.Enable,
	    vha->ioctl->flags, cmd->Status));

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (rval);
}

/*
 * qla2x00_aen_flush
 *	Flush all the outstanding AENs to the application.
 *
 * Input:
 *	ha = pointer to the adapter struct of the specified adapter.
 */
void
qla2x00_aen_flush(scsi_qla_host_t *vha)
{
	if ( (vha->ioctl->flags & IOCTL_AEN_PENDING) ) {
		vha->ioctl->flags &= ~IOCTL_AEN_PENDING;
		DEBUG9(printk(KERN_INFO "%s(%ld): inst=%ld exiting.\n",
		    __func__, vha->host_no, vha->instance));
	}
}

/*
 * qla2x00_aen_get
 *	Asynchronous Event Record Transfer to user.
 *	The entire queue will be emptied and transferred back.
 *
 * Input:
 *	ha = pointer to the adapter struct of the specified adapter.
 *	pext = pointer to EXT_IOCTL structure containing values from user.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 *
 * NOTE: Need to use hardware lock to protect the queues from updates
 *	 via isr/enqueue_aen after we get rid of io_request_lock.
 */
static int
qla2x00_aen_get(scsi_qla_host_t *vha, EXT_IOCTL *cmd, int mode)
{
	int		rval = 0;
	EXT_ASYNC_EVENT	*tmp_q;
	EXT_ASYNC_EVENT	*paen;
	uint8_t		i;
	uint8_t		queue_cnt;
	uint8_t		request_cnt;
	uint32_t	stat = EXT_STATUS_OK;
	uint32_t	ret_len = 0;
	unsigned long   cpu_flags = 0;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	request_cnt = (uint8_t)(cmd->ResponseLen / sizeof(EXT_ASYNC_EVENT));

	if (request_cnt < EXT_DEF_MAX_AEN_QUEUE) {
		/* We require caller to alloc for the maximum request count */
		cmd->Status       = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s(%ld): inst=%ld Buffer size %ld too small. "
		    "Exiting normally.",
		    __func__, vha->host_no, vha->instance,
		    (ulong)cmd->ResponseLen));

		return (rval);
	}

	if ((vha->ioctl->flags & IOCTL_AEN_PENDING)) {
                cmd->ResponseLen = 0;
                stat = EXT_STATUS_OK;
                cmd->Status = stat;
                return (rval);
        }
	
	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&paen,
	    sizeof(EXT_ASYNC_EVENT) * EXT_DEF_MAX_AEN_QUEUE)) {
		/* not enough memory */
		cmd->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_ASYNC_EVENT)*EXT_DEF_MAX_AEN_QUEUE));
		return (rval);
	}

	/* 1st: Make a local copy of the entire queue content. */
	tmp_q = (EXT_ASYNC_EVENT *)vha->ioctl->aen_tracking_queue;
	queue_cnt = 0;

	spin_lock_irqsave(&ha->hardware_lock, cpu_flags);
	i = vha->ioctl->aen_q_head;

	for (; queue_cnt < EXT_DEF_MAX_AEN_QUEUE;) {
		if (tmp_q[i].AsyncEventCode != 0) {
			memcpy(&paen[queue_cnt], &tmp_q[i],
			    sizeof(EXT_ASYNC_EVENT));
			queue_cnt++;
			tmp_q[i].AsyncEventCode = 0; /* empty out the slot */
		}

		if (i == vha->ioctl->aen_q_tail) {
			/* done. */
			break;
		}

		i++;

		if (i == EXT_DEF_MAX_AEN_QUEUE) {
			i = 0;
		}
	}

	/* Empty the queue. */
	vha->ioctl->aen_q_head = 0;
	vha->ioctl->aen_q_tail = 0;

	spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);

	/* 2nd: Now transfer the queue content to user buffer */
	/* Copy the entire queue to user's buffer. */
	ret_len = (uint32_t)(queue_cnt * sizeof(EXT_ASYNC_EVENT));
	if (queue_cnt != 0) {
		rval = copy_to_user(Q64BIT_TO_PTR(cmd->ResponseAdr,
		    cmd->AddrMode), paen, ret_len);
	}
	cmd->ResponseLen = ret_len;

	if (rval != 0) {
		DEBUG9_10(printk("%s(%ld): inst=%ld copy FAILED. error = %d\n",
		    __func__, vha->host_no, vha->instance, rval));
		rval = -EFAULT;
		stat = EXT_STATUS_COPY_ERR;
	} else {
		stat = EXT_STATUS_OK;
	}

	cmd->Status = stat;
	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting. rval=%d.\n",
	     __func__, vha->host_no, vha->instance, rval));

	return (rval);
}

/*
 * qla2x00_enqueue_aen
 *
 * Input:
 *  ha = adapter state pointer.
 *  event_code = async event code of the event to add to queue.
 *  payload = event payload for the queue.
 *
 * Context:
 *  Interrupt context.
 * NOTE: Need to hold the hardware lock to protect the queues from
 *   aen_get after we get rid of the io_request_lock.
 */
void
qla2x00_enqueue_aen(scsi_qla_host_t *vha, uint16_t event_code, void *payload)
{
    uint8_t         new_entry; /* index to current entry */
    uint16_t        *mbx;
    EXT_ASYNC_EVENT     *aen_queue;

	if (!vha->fc_vport)
		return;

    DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
        __func__, vha->host_no, vha->instance));

	if (!(vha->ioctl->flags & IOCTL_AEN_TRACKING_ENABLE))
		return;

    aen_queue = (EXT_ASYNC_EVENT *)vha->ioctl->aen_tracking_queue;
    if (aen_queue[vha->ioctl->aen_q_tail].AsyncEventCode != 0) {
        /* Need to change queue pointers to make room. */

        /* Increment tail for adding new entry. */
        vha->ioctl->aen_q_tail++;
        if (vha->ioctl->aen_q_tail == EXT_DEF_MAX_AEN_QUEUE) {
            vha->ioctl->aen_q_tail = 0;
        }

        if (vha->ioctl->aen_q_head == vha->ioctl->aen_q_tail) {
            /*
             * We're overwriting the oldest entry, so need to
             * update the head pointer.
             */
            vha->ioctl->aen_q_head++;
            if (vha->ioctl->aen_q_head == EXT_DEF_MAX_AEN_QUEUE) {
                vha->ioctl->aen_q_head = 0;
            }
        }
    }

    DEBUG(printk("%s(%ld): inst=%ld Adding code 0x%x to aen_q %p @ %d\n",
        __func__, vha->host_no, vha->instance, event_code, aen_queue,
        vha->ioctl->aen_q_tail));

    new_entry = vha->ioctl->aen_q_tail;
    aen_queue[new_entry].AsyncEventCode = event_code;

        /* Update payload */
    switch (event_code) {
    case MBA_LIP_OCCURRED:
    case MBA_LOOP_UP:
    case MBA_LOOP_DOWN:
    case MBA_LIP_RESET:
    case MBA_PORT_UPDATE:
        /* empty */
        break;

    case MBA_RSCN_UPDATE:
	if (payload != NULL) {
		mbx = (uint16_t *)payload;
		aen_queue[new_entry].Payload.RSCN.AddrFormat = MSB(mbx[1]);
		/* al_pa */
		aen_queue[new_entry].Payload.RSCN.RSCNInfo[0] = LSB(mbx[1]);
		/* area */
		aen_queue[new_entry].Payload.RSCN.RSCNInfo[1] = MSB(mbx[2]);
		/* domain */
		aen_queue[new_entry].Payload.RSCN.RSCNInfo[2] = LSB(mbx[2]);
	}
        break;

    default:
        /* Not supported */
        aen_queue[new_entry].AsyncEventCode = 0;
        break;
    }

	vha->ioctl->flags |= IOCTL_AEN_PENDING;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));
}

/*
 * qla2x00_wwpn_to_scsiaddr
 *	Handles the EXT_CC_WWPN_TO_SCSIADDR command.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_wwpn_to_scsiaddr(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int		ret = 0;
	int		found = 0;
	fc_port_t	*fcport;
	uint8_t		tmp_wwpn[EXT_DEF_WWN_NAME_SIZE];
	uint32_t	b, l;
	EXT_SCSI_ADDR	tmp_addr;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (pext->RequestLen != EXT_DEF_WWN_NAME_SIZE ||
	    pext->ResponseLen < sizeof(EXT_SCSI_ADDR)) {
		/* error */
		DEBUG9_10(printk("%s(%ld): inst=%ld invalid WWN buffer size %d "
		    "received.\n",
		    __func__, vha->host_no, vha->instance, pext->ResponseLen));
		pext->Status = EXT_STATUS_INVALID_PARAM;

		return (ret);
	}

	ret = copy_from_user(tmp_wwpn, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), pext->RequestLen);
	if (ret) {
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy_from_user "
		    "failed(%d) on request buf.\n",
		    __func__, vha->host_no, vha->instance, ret));
		pext->Status = EXT_STATUS_COPY_ERR;
		return (-EFAULT);
	}

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		if (memcmp(tmp_wwpn, fcport->port_name,
		    EXT_DEF_WWN_NAME_SIZE) == 0) {
			found++;
			break;
		}
	}

	if (!found) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		return (ret);
	}

	/* Currently we only have bus 0 and no translation on LUN */
	b = 0;
	l = 0;

	/*
	 * Return SCSI address. Currently no translation is done for
	 * LUN.
	 */
	tmp_addr.Bus = b;
	tmp_addr.Target = fcport->os_target_id;
	tmp_addr.Lun = l;

	if (pext->ResponseLen > sizeof(EXT_SCSI_ADDR))
		pext->ResponseLen = sizeof(EXT_SCSI_ADDR);

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    &tmp_addr, pext->ResponseLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	DEBUG9(printk(KERN_INFO
	    "%s(%ld): Found t%d l%d for %02x%02x%02x%02x%02x%02x%02x%02x.\n",
	    __func__, vha->host_no,
	    tmp_addr.Target, tmp_addr.Lun,
	    tmp_wwpn[0], tmp_wwpn[1], tmp_wwpn[2], tmp_wwpn[3],
	    tmp_wwpn[4], tmp_wwpn[5], tmp_wwpn[6], tmp_wwpn[7]));

	pext->Status = EXT_STATUS_OK;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

static int
qla2x00_msiocb_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext, int cmd,
    int mode)
{
	int		ret = 0;
	fc_lun_t	*ptemp_fclun = NULL;	/* buf from scrap mem */
	fc_port_t	*ptemp_fcport = NULL;	/* buf from scrap mem */
	struct scsi_cmnd *pscsi_cmd = NULL;	/* buf from scrap mem */
	struct scsi_device *pscsi_dev = NULL;	/* buf from scrap mem */
	struct request *request = NULL;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* check on current topology */
	if ((ha->current_topology != ISP_CFG_F) &&
	    (ha->current_topology != ISP_CFG_FL)) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR not in F/FL mode\n",
		    __func__, vha->host_no, vha->instance));
		return (ret);
	}

	if (vha->ioctl_mem_size <= 0) {
		if (qla2x00_get_new_ioctl_dma_mem(vha,
		    QLA_INITIAL_IOCTLMEM_SIZE) != QLA_SUCCESS) {

			DEBUG9_10(printk("%s: ERROR cannot alloc DMA "
			    "buffer size=%x.\n",
			    __func__, QLA_INITIAL_IOCTLMEM_SIZE));

			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}

	if (pext->ResponseLen > vha->ioctl_mem_size) {
		if (qla2x00_get_new_ioctl_dma_mem(vha, pext->ResponseLen) !=
		    QLA_SUCCESS) {

			DEBUG9_10(printk("%s: ERROR cannot alloc requested"
			    "DMA buffer size %x.\n",
			    __func__, pext->ResponseLen));

			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}

		DEBUG9(printk("%s(%ld): inst=%ld rsp buf length larger than "
		    "existing size. Additional mem alloc successful.\n",
		    __func__, vha->host_no, vha->instance));
	}

	DEBUG9(printk("%s(%ld): inst=%ld req buf verified.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi_cmd,
	    sizeof(struct scsi_cmnd))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "cmd size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct scsi_cmnd)));
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pscsi_dev,
	    sizeof(struct scsi_device))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "cmd size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct scsi_device)));
		return (ret);
	}

	pscsi_cmd->device = pscsi_dev;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&request,
	    sizeof(struct request))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(struct request)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}
	pscsi_cmd->request = request;
	pscsi_cmd->request->nr_hw_segments = 1;

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptemp_fcport,
	    sizeof(fc_port_t))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "fcport size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(fc_port_t)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&ptemp_fclun,
	    sizeof(fc_lun_t))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "fclun size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(fc_lun_t)));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* initialize */
	memset(vha->ioctl_mem, 0, vha->ioctl_mem_size);
	ptemp_fcport->vha = vha;

	if (pscsi_cmd->timeout_per_command == 0)
		pscsi_cmd->timeout_per_command  = (ql2xioctltimeout + 3) * HZ;

	switch (cmd) {
	case EXT_CC_SEND_FCCT_PASSTHRU:
		DEBUG9(printk("%s: got CT passthru cmd.\n", __func__));
		ret = qla2x00_send_fcct(vha, pext, pscsi_cmd, ptemp_fcport,
		    ptemp_fclun, mode);
		break;
	case EXT_CC_SEND_ELS_PASSTHRU:
		DEBUG9(printk("%s: got ELS passthru cmd.\n", __func__));
		if (!IS_QLA2100(ha) && !IS_QLA2200(ha)) {
			ret = qla2x00_send_els_passthru(vha, pext, pscsi_cmd,
			    ptemp_fcport, ptemp_fclun, mode);
			break;
		}
		/*FALLTHROUGH */
	default:
		DEBUG9_10(printk("%s: got invalid cmd.\n", __func__));
		break;
	}

	qla2x00_free_ioctl_scrap_mem(vha);

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_send_fcct
 *	Passes the FC CT command down to firmware as MSIOCB and
 *	copies the response back when it completes.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_send_fcct(scsi_qla_host_t *vha, EXT_IOCTL *pext,
	struct scsi_cmnd *pscsi_cmd, fc_port_t *ptmp_fcport,
	fc_lun_t *ptmp_fclun, int mode)
{
	int		ret = 0;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (pext->RequestLen > vha->ioctl_mem_size) {
		pext->Status = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR ReqLen too big=%x.\n",
		    __func__, vha->host_no, vha->instance, pext->RequestLen));

		return (ret);
	}

	/* copy request buffer */
	ret = copy_from_user(vha->ioctl_mem, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), pext->RequestLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf. ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));

		return (-EFAULT);
	}

	DEBUG9(printk("%s(%ld): inst=%ld after copy request.\n",
	    __func__, vha->host_no, vha->instance));

	if (qla2x00_mgmt_svr_login(vha) != QLA_SUCCESS) {
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;

		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR login to MS.\n",
		    __func__, vha->host_no, vha->instance));

		return (ret);
	}

	DEBUG9(printk("%s(%ld): success login to MS.\n",
	    __func__, vha->host_no));

	/* queue command */
	if ((ret = qla2x00_ioctl_ms_queuecommand(vha, pext, pscsi_cmd,
	    ptmp_fcport, ptmp_fclun, NULL))) {
		return (ret);
	}

	if ((CMD_COMPL_STATUS(pscsi_cmd) != 0 &&
	    CMD_COMPL_STATUS(pscsi_cmd) != CS_DATA_UNDERRUN &&
	    CMD_COMPL_STATUS(pscsi_cmd) != CS_DATA_OVERRUN)||
	    CMD_ENTRY_STATUS(pscsi_cmd) != 0) {
		DEBUG9_10(printk("%s(%ld): inst=%ld cmd returned error=%x.\n",
		    __func__, vha->host_no, vha->instance,
		    CMD_COMPL_STATUS(pscsi_cmd)));
		pext->Status = EXT_STATUS_ERR;
		return (ret);
	}

	/* sending back data returned from Management Server */
	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    vha->ioctl_mem, pext->ResponseLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

static int
qla2x00_ioctl_ms_queuecommand(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi_cmd, fc_port_t *pfcport, fc_lun_t *pfclun,
    EXT_ELS_PT_REQ *pels_pt_req)
{
	int		ret = 0;
	int		tmp_rval = 0;
	srb_t		*sp = NULL;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	/* alloc sp */
	if ((sp = qla2x00_get_new_sp(vha, pfcport, pscsi_cmd,
	    NULL)) == NULL) {

		DEBUG9_10(printk("%s: ERROR cannot alloc sp %p.\n",
		    __func__, sp));
		pext->Status = EXT_STATUS_NO_MEMORY;

		return (ret);
	}

	DEBUG9(printk("%s(%ld): inst=%ld after alloc sp.\n",
	    __func__, vha->host_no, vha->instance));

	DEBUG9(printk("%s(%ld): pfclun=%p pfcport=%p pscsi_cmd=%p.\n",
	    __func__, vha->host_no, pfclun, pfcport, pscsi_cmd));

	sp->cmd = pscsi_cmd;
	sp->flags = SRB_IOCTL;

	pfclun->fcport = pfcport;
	pfclun->lun = 0;

	DEBUG9(printk("%s(%ld): pscsi_cmd->device=%p.\n",
	    __func__, vha->host_no, pscsi_cmd->device));

	/* init scsi_cmd */
	pscsi_cmd->device->host = vha->host;
	pscsi_cmd->scsi_done = qla2x00_msiocb_done;

	/* check on loop down (2)- check again just before sending cmd out. */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags)) {

		DEBUG9_10(printk("%s(%ld): inst=%ld before issue cmd- loop "
		    "not ready.\n",
		    __func__, vha->host_no, vha->instance));
		mempool_free(sp, ha->srb_mempool);
		pext->Status = EXT_STATUS_BUSY;

		return (ret);
	}

	DEBUG9(printk("%s(%ld): inst=%ld going to issue command.\n",
	    __func__, vha->host_no, vha->instance));

	tmp_rval = qla2x00_start_ms_cmd(base_vha, pext, sp, pels_pt_req);

	DEBUG9(printk("%s(%ld): inst=%ld after issue command.\n",
	    __func__, vha->host_no, vha->instance));

	if (tmp_rval != 0) {
		/* We waited and post function did not get called */
		DEBUG9_10(printk("%s(%ld): inst=%ld command timed out.\n",
		    __func__, vha->host_no, vha->instance));
		mempool_free(sp, ha->srb_mempool);
		pext->Status = EXT_STATUS_MS_NO_RESPONSE;

		return (ret);
	}

	return (ret);
}

/*
 * qla2x00_start_ms_cmd
 *	Allocates an MSIOCB request pkt and sends out the passthru cmd.
 *
 * Input:
 *	ha = adapter state pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_start_ms_cmd(scsi_qla_host_t *vha, EXT_IOCTL *pext, srb_t *sp,
    EXT_ELS_PT_REQ *pels_pt_req)
{
#define	ELS_REQUEST_RCTL	0x22
#define ELS_REPLY_RCTL		0x23
#define SRB_ACTIVE_STATE        2    /* Request in Active Array */

	uint32_t	usr_req_len;
	uint32_t	usr_resp_len;

	ms_iocb_entry_t		*pkt;
	unsigned long		cpu_flags = 0;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = ha->req_q_map[0];
	struct rsp_que *rsp = ha->rsp_q_map[0];

	/* get spin lock for this operation */
	spin_lock_irqsave(&ha->hardware_lock, cpu_flags);

	/* Get MS request packet. */
	pkt = (ms_iocb_entry_t *)qla2x00_ms_req_pkt(vha, req, rsp, sp);
	if (pkt == NULL) {
		/* release spin lock and return error. */
		spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);

		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld MSIOCB PT - could not get "
		    "Request Packet.\n", __func__, vha->host_no, vha->instance));
		printk("%s(%ld): inst=%ld MSIOCB PT - could not get "
		    "Request Packet.\n", __func__, vha->host_no, vha->instance);
		return (QLA_MEMORY_ALLOC_FAILED);
	}

	usr_req_len = pext->RequestLen;
	usr_resp_len = pext->ResponseLen;

	if (IS_FWI2_CAPABLE(ha)) {
		struct ct_entry_24xx *ct_pkt;
		struct els_entry_24xx *els_pkt;

		ct_pkt = (struct ct_entry_24xx *)pkt;
		els_pkt = (struct els_entry_24xx *)pkt;

		if (pels_pt_req != NULL) {
			/* ELS Passthru */
			usr_req_len -= sizeof(EXT_ELS_PT_REQ);
			usr_resp_len -= sizeof(EXT_ELS_PT_REQ);

			els_pkt->entry_type = ELS_IOCB_TYPE;
			els_pkt->entry_count = 1;
			els_pkt->nport_handle = cpu_to_le16(pels_pt_req->Lid);
			els_pkt->tx_dsd_count = __constant_cpu_to_le16(1);
			els_pkt->rx_dsd_count = __constant_cpu_to_le16(1);
			els_pkt->rx_byte_count = cpu_to_le32(usr_resp_len);
			els_pkt->tx_byte_count = cpu_to_le32(usr_req_len);
			els_pkt->sof_type = EST_SOFI3; /* assume class 3 */
			els_pkt->opcode = 0;
			els_pkt->control_flags = 0;

			if (pext->ResponseLen == 0) {
				memcpy(els_pkt->port_id, &pels_pt_req->Id[1],
				    3);
			}

			els_pkt->tx_address[0] =
			    cpu_to_le32(LSD(vha->ioctl_mem_phys));
			els_pkt->tx_address[1] =
			    cpu_to_le32(MSD(vha->ioctl_mem_phys));
			els_pkt->tx_len = els_pkt->tx_byte_count;
			els_pkt->rx_address[0] =
			    cpu_to_le32(LSD(vha->ioctl_mem_phys));
			els_pkt->rx_address[1] =
			    cpu_to_le32(MSD(vha->ioctl_mem_phys));
			els_pkt->rx_len = els_pkt->rx_byte_count;
		} else {
			/* CT Passthru */
			ct_pkt->entry_type = CT_IOCB_TYPE;
			ct_pkt->entry_count = 1;
			ct_pkt->nport_handle =
			    cpu_to_le16(vha->mgmt_svr_loop_id);
			ct_pkt->timeout = cpu_to_le16(ql2xioctltimeout);
			ct_pkt->cmd_dsd_count = __constant_cpu_to_le16(1);
			ct_pkt->rsp_dsd_count = __constant_cpu_to_le16(1);
			ct_pkt->rsp_byte_count = cpu_to_le32(usr_resp_len);
			ct_pkt->cmd_byte_count = cpu_to_le32(usr_req_len);
			ct_pkt->dseg_0_address[0] =
			    cpu_to_le32(LSD(vha->ioctl_mem_phys));
			ct_pkt->dseg_0_address[1] =
			    cpu_to_le32(MSD(vha->ioctl_mem_phys));
			ct_pkt->dseg_0_len = ct_pkt->cmd_byte_count;
			ct_pkt->dseg_1_address[0] =
			    cpu_to_le32(LSD(vha->ioctl_mem_phys));
			ct_pkt->dseg_1_address[1] =
			    cpu_to_le32(MSD(vha->ioctl_mem_phys));
			ct_pkt->dseg_1_len = ct_pkt->rsp_byte_count;
		}
	} else {
		pkt->entry_type  = MS_IOCB_TYPE;
		pkt->entry_count = 1;

		if (pels_pt_req != NULL) {
			/* process ELS passthru command */
			usr_req_len -= sizeof(EXT_ELS_PT_REQ);
			usr_resp_len -= sizeof(EXT_ELS_PT_REQ);

			/* ELS passthru enabled */
			pkt->control_flags = cpu_to_le16(BIT_15);
			SET_TARGET_ID(ha, pkt->loop_id, pels_pt_req->Lid);
			pkt->type    = 1; /* ELS frame */

			if (pext->ResponseLen != 0) {
				pkt->r_ctl = ELS_REQUEST_RCTL;
				pkt->rx_id = 0;
			} else {
				pkt->r_ctl = ELS_REPLY_RCTL;
				pkt->rx_id =
				    cpu_to_le16(pels_pt_req->Rxid);
			}
		} else {
			usr_req_len = pext->RequestLen;
			usr_resp_len = pext->ResponseLen;
			SET_TARGET_ID(ha, pkt->loop_id, vha->mgmt_svr_loop_id);
		}

		DEBUG9_10(printk("%s(%ld): inst=%ld using loop_id=%02x "
		    "req_len=%d, resp_len=%d. Initializing pkt.\n",
		    __func__, vha->host_no, vha->instance,
		    pkt->loop_id.extended, usr_req_len, usr_resp_len));

		pkt->timeout = cpu_to_le16(ql2xioctltimeout);
		pkt->cmd_dsd_count = __constant_cpu_to_le16(1);
		pkt->total_dsd_count = __constant_cpu_to_le16(2);
		pkt->rsp_bytecount = cpu_to_le32(usr_resp_len);
		pkt->req_bytecount = cpu_to_le32(usr_req_len);

		/*
		 * Loading command payload address. user request is assumed
		 * to have been copied to ioctl_mem.
		 */
		pkt->dseg_req_address[0] = cpu_to_le32(LSD(vha->ioctl_mem_phys));
		pkt->dseg_req_address[1] = cpu_to_le32(MSD(vha->ioctl_mem_phys));
		pkt->dseg_req_length = cpu_to_le32(usr_req_len);

		/* loading response payload address */
		pkt->dseg_rsp_address[0] = cpu_to_le32(LSD(vha->ioctl_mem_phys));
		pkt->dseg_rsp_address[1] =cpu_to_le32(MSD(vha->ioctl_mem_phys));
		pkt->dseg_rsp_length = cpu_to_le32(usr_resp_len);
	}

	/* set flag to indicate IOCTL MSIOCB cmd in progress */
	vha->ioctl->MSIOCB_InProgress = 1;

	/* prepare for receiving completion. */
	sema_init(&vha->ioctl->cmpl_sem, 0);

	/* Adds the sp timer for processing ms-ioctl cmd timeouts */
	if ((sp->cmd->timeout_per_command / HZ) >= ql2xcmdtimermin)
		qla2x00_add_timer_to_cmd(sp,
		    (sp->cmd->timeout_per_command / HZ) -
		    QLA_CMD_TIMER_DELTA);
	else
		sp->flags |= SRB_NO_TIMER;

	qla2x00_isp_cmd(vha, req);

	DEBUG9(printk("%s(%ld): inst=%ld releasing hardware_lock.\n",
	    __func__, vha->host_no, vha->instance));
	spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);

	DEBUG9(printk("%s(%ld): inst=%ld sleep for completion.\n",
	    __func__, vha->host_no, vha->instance));

	down(&vha->ioctl->cmpl_sem);

	if (vha->ioctl->MSIOCB_InProgress == 1) {
	 	DEBUG9_10(printk("%s(%ld): inst=%ld timed out. exiting.\n",
		    __func__, vha->host_no, vha->instance));
		return QLA_FUNCTION_FAILED;
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return QLA_SUCCESS;
}

/*
 * qla2x00_send_els_passthru
 *	Passes the ELS command down to firmware as MSIOCB and
 *	copies the response back when it completes.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = EXT_IOCTL structure pointer.
 *	mode = not used.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_send_els_passthru(scsi_qla_host_t *vha, EXT_IOCTL *pext,
    struct scsi_cmnd *pscsi_cmd, fc_port_t *ptmp_fcport, fc_lun_t *ptmp_fclun,
    int mode)
{
	int		ret = 0;

	uint8_t		invalid_wwn = 0;
	uint8_t		*ptmp_stat;
	uint8_t		*pusr_req_buf;
	uint8_t		*presp_payload;
	uint32_t	payload_len;
	uint32_t	usr_req_len;

	int		found;
	uint16_t	next_loop_id;
	fc_port_t	*fcport;

	EXT_ELS_PT_REQ	*pels_pt_req;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	usr_req_len = pext->RequestLen - sizeof(EXT_ELS_PT_REQ);
	if (usr_req_len > vha->ioctl_mem_size) {
		pext->Status = EXT_STATUS_INVALID_PARAM;

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR ReqLen too big=%x.\n",
		    __func__, vha->host_no, vha->instance, pext->RequestLen));

		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&pels_pt_req,
	    sizeof(EXT_ELS_PT_REQ))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "els_pt_req size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_ELS_PT_REQ)));
		return (ret);
	}

	/* copy request buffer */
	
	ret = copy_from_user(pels_pt_req, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), sizeof(EXT_ELS_PT_REQ));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR"
		    "copy_from_user() of struct failed (%d).\n",
		    __func__, vha->host_no, vha->instance, ret));

		return (-EFAULT);
	}

	pusr_req_buf = (uint8_t *)Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode) + sizeof(EXT_ELS_PT_REQ);
	
	ret = copy_from_user(vha->ioctl_mem, pusr_req_buf, usr_req_len);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR"
		    "copy_from_user() of request buf failed (%d).\n",
		    __func__, vha->host_no, vha->instance, ret));

		return (-EFAULT);
	}

	DEBUG9(printk("%s(%ld): inst=%ld after copy request.\n",
	    __func__, vha->host_no, vha->instance));
	
	/* check on loop down (1) */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags)) {

		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld before dest port validation- loop not "
		    "ready; cannot proceed.\n",
		    __func__, vha->host_no, vha->instance));

		pext->Status = EXT_STATUS_BUSY;

		return (ret);
	}

	/*********************************/
	/* Validate the destination port */
	/*********************************/

	/* first: WWN cannot be zero if no PID is specified */
	invalid_wwn = qla2x00_is_wwn_zero(pels_pt_req->WWPN);
	if (invalid_wwn && !(pels_pt_req->ValidMask & EXT_DEF_PID_VALID)) {
		/* error: both are not set. */
		pext->Status = EXT_STATUS_INVALID_PARAM;

		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR no valid WWPN/PID\n",
		    __func__, vha->host_no, vha->instance));

		return (ret);
	}

	/* second: it cannot be the local/current HBA itself */
	if (!invalid_wwn) {
		if (memcmp(vha->port_name, pels_pt_req->WWPN,
		    EXT_DEF_WWN_NAME_SIZE) == 0) {

			/* local HBA specified. */

			pext->Status = EXT_STATUS_INVALID_PARAM;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR local HBA's "
			    "WWPN found.\n",
			    __func__, vha->host_no, vha->instance));

			return (ret);
		}
	} else { /* using PID */
		if (pels_pt_req->Id[1] == vha->d_id.r.d_id[2] 
		    && pels_pt_req->Id[2] == vha->d_id.r.d_id[1] 
		    && pels_pt_req->Id[3] == vha->d_id.r.d_id[0]) {

			/* local HBA specified. */

			pext->Status = EXT_STATUS_INVALID_PARAM;
			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR local HBA's "
			    "PID found.\n",
			    __func__, vha->host_no, vha->instance));

			return (ret);
		}
	}

	/************************/
	/* Now find the loop ID */
	/************************/

	found = 0;
	fcport = NULL;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_INITIATOR ||
		    fcport->port_type != FCT_TARGET)
			continue;

		if (!invalid_wwn) {
			/* search with WWPN */
			if (memcmp(pels_pt_req->WWPN, fcport->port_name,
			    EXT_DEF_WWN_NAME_SIZE))
				continue;
		} else {
			/* search with PID */
			if (pels_pt_req->Id[1] != fcport->d_id.r.d_id[2]
			    || pels_pt_req->Id[2] != fcport->d_id.r.d_id[1]
			    || pels_pt_req->Id[3] != fcport->d_id.r.d_id[0])
				continue;
		}

		found++;
	}

	if (!found) {
		/* invalid WWN or PID specified */
		pext->Status = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR WWPN/PID invalid.\n",
		    __func__, vha->host_no, vha->instance));

		return (ret);
	}

	/* If this is for a host device, check if we need to perform login */
	if (fcport->port_type == FCT_INITIATOR &&
	    fcport->loop_id >= vha->hw->max_loop_id) {

		next_loop_id = 0;
		ret = qla2x00_fabric_login(vha, fcport, &next_loop_id);
		if (ret != QLA_SUCCESS) {
			/* login failed. */
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;

			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR login to "
			    "host port failed. loop_id=%02x pid=%02x%02x%02x "
			    "ret=%d.\n",
			    __func__, vha->host_no, vha->instance,
			    fcport->loop_id, fcport->d_id.b.domain,
			    fcport->d_id.b.area, fcport->d_id.b.al_pa, ret));

			return (ret);
		}
	}

	/* queue command */
	pels_pt_req->Lid = fcport->loop_id;

	if ((ret = qla2x00_ioctl_ms_queuecommand(vha, pext, pscsi_cmd,
		ptmp_fcport, ptmp_fclun, pels_pt_req))) {
		return (ret);
	}

	if ((CMD_COMPL_STATUS(pscsi_cmd) != 0 &&
	    CMD_COMPL_STATUS(pscsi_cmd) != CS_DATA_UNDERRUN &&
	    CMD_COMPL_STATUS(pscsi_cmd) != CS_DATA_OVERRUN)||
	    CMD_ENTRY_STATUS(pscsi_cmd) != 0) {
		DEBUG9_10(printk("%s(%ld): inst=%ld cmd returned error=%x.\n",
		    __func__, vha->host_no, vha->instance,
		    CMD_COMPL_STATUS(pscsi_cmd)));
		pext->Status = EXT_STATUS_ERR;
		return (ret);
	}

	/* check on data returned */
	ptmp_stat = (uint8_t *)vha->ioctl_mem + FC_HEADER_LEN;

	if (*ptmp_stat == ELS_STAT_LS_RJT) {
		payload_len = FC_HEADER_LEN + ELS_RJT_LENGTH;

	} else if (*ptmp_stat == ELS_STAT_LS_ACC) {
		payload_len = pext->ResponseLen - sizeof(EXT_ELS_PT_REQ);

	} else {
		/* invalid. just copy the status word. */
		DEBUG9_10(printk("%s(%ld): inst=%ld invalid stat "
		    "returned =0x%x.\n",
		    __func__, vha->host_no, vha->instance, *ptmp_stat));

		payload_len = FC_HEADER_LEN + 4;
	}

	DEBUG9(printk("%s(%ld): inst=%ld data dump-\n",
	    __func__, vha->host_no, vha->instance));
	DEBUG9(qla2x00_dump_buffer((uint8_t *)ptmp_stat,
	    pext->ResponseLen - sizeof(EXT_ELS_PT_REQ) - FC_HEADER_LEN));
	
	/* Verify response buffer to be written */
	/* The data returned include FC frame header */
	presp_payload = (uint8_t *)Q64BIT_TO_PTR(pext->ResponseAdr,
	    pext->AddrMode) + sizeof(EXT_ELS_PT_REQ);

	/* copy back data returned to response buffer */
	ret = copy_to_user(presp_payload, vha->ioctl_mem, payload_len);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy rsp buffer.\n",
		    __func__, vha->host_no, vha->instance));
		return (-EFAULT);
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting normally.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_send_els_rnid
 *	IOCTL to send extended link service RNID command to a target.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_send_els_rnid(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	EXT_RNID_REQ	*tmp_rnid;
	int		ret = 0;
	uint16_t	mb[MAILBOX_REGISTER_COUNT];
	uint32_t	copy_len;
	int		found;
	uint16_t	next_loop_id;
	fc_port_t	*fcport;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (vha->ioctl_mem_size < SEND_RNID_RSP_SIZE) {
		if (qla2x00_get_new_ioctl_dma_mem(vha,
		    SEND_RNID_RSP_SIZE) != QLA_SUCCESS) {

			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR cannot alloc "
			    "DMA buffer. size=%x.\n",
			    __func__, vha->host_no, vha->instance,
			    SEND_RNID_RSP_SIZE));

			pext->Status = EXT_STATUS_NO_MEMORY;
			return (ret);
		}
	}

	if (pext->RequestLen != sizeof(EXT_RNID_REQ)) {
		/* parameter error */
		DEBUG9_10(printk("%s(%ld): inst=%ld invalid req length %d.\n",
		    __func__, vha->host_no, vha->instance, pext->RequestLen));
		pext->Status = EXT_STATUS_INVALID_PARAM;
		return (ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&tmp_rnid,
	    sizeof(EXT_RNID_REQ))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_RNID_REQ)));
		return (ret);
	}

	ret = copy_from_user(tmp_rnid, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), pext->RequestLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	/* Find loop ID of the device */
	found = 0;
	fcport = NULL;
	switch (tmp_rnid->Addr.Type) {
	case EXT_DEF_TYPE_WWNN:
		DEBUG9(printk("%s(%ld): inst=%ld got node name.\n",
		    __func__, vha->host_no, vha->instance));

		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type != FCT_INITIATOR ||
			    fcport->port_type != FCT_TARGET)
				continue;

			if (memcmp(tmp_rnid->Addr.FcAddr.WWNN,
			    fcport->node_name, EXT_DEF_WWN_NAME_SIZE))
				continue;

			if (fcport->port_type == FCT_TARGET) {
				if (atomic_read(&fcport->state) != FCS_ONLINE)
					continue;
			} else { /* FCT_INITIATOR */
				if (!fcport->d_id.b24)
					continue;
			}

			found++;
			break;
		}
		break;

	case EXT_DEF_TYPE_WWPN:
		DEBUG9(printk("%s(%ld): inst=%ld got port name.\n",
		    __func__, vha->host_no, vha->instance));

		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (!(fcport->port_type == FCT_INITIATOR ||
			        fcport->port_type == FCT_TARGET))
				continue;

			if (memcmp(tmp_rnid->Addr.FcAddr.WWPN,
			    fcport->port_name, EXT_DEF_WWN_NAME_SIZE))
				continue;

			if (fcport->port_type == FCT_TARGET) {
				if (atomic_read(&fcport->state) != FCS_ONLINE)
					continue;
			} else { /* FCT_INITIATOR */
				if (!fcport->d_id.b24)
					continue;
			}

			found++;
		}
		break;

	case EXT_DEF_TYPE_PORTID:
		DEBUG9(printk("%s(%ld): inst=%ld got port ID.\n",
		    __func__, vha->host_no, vha->instance));

		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (!(fcport->port_type == FCT_INITIATOR ||
			        fcport->port_type == FCT_TARGET))
				continue;

			/* PORTID bytes entered must already be big endian */
			if (memcmp(&tmp_rnid->Addr.FcAddr.Id[1],
			    &fcport->d_id, EXT_DEF_PORTID_SIZE_ACTUAL))
				continue;

			if (fcport->port_type == FCT_TARGET) {
				if (atomic_read(&fcport->state) != FCS_ONLINE)
					continue;
			}

			found++;
			break;
		}
		break;
	default:
		/* parameter error */
		pext->Status = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk("%s(%ld): inst=%ld invalid addressing type.\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	if (!found || (fcport->port_type == FCT_TARGET &&
	    fcport->loop_id > ha->max_loop_id)) {
		/*
		 * No matching device or the target device is not configured;
		 * just return error.
		 */
		pext->Status = EXT_STATUS_DEV_NOT_FOUND;
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* check on loop down */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) || 
	    vha->dpc_active) {

		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		    __func__, vha->host_no, vha->instance));

		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	/* If this is for a host device, check if we need to perform login */
	if (fcport->port_type == FCT_INITIATOR &&
	    fcport->loop_id >= ha->max_loop_id) {
		next_loop_id = 0;
		ret = qla2x00_fabric_login(vha, fcport, &next_loop_id);
		if (ret != QLA_SUCCESS) {
			/* login failed. */
			pext->Status = EXT_STATUS_DEV_NOT_FOUND;

			DEBUG9_10(printk("%s(%ld): inst=%ld ERROR login to "
			    "host port failed. loop_id=%02x pid=%02x%02x%02x "
			    "ret=%d.\n",
			    __func__, vha->host_no, vha->instance,
			    fcport->loop_id, fcport->d_id.b.domain,
			    fcport->d_id.b.area, fcport->d_id.b.al_pa, ret));

			qla2x00_free_ioctl_scrap_mem(vha);
			return (ret);
		}
	}

	/* Send command */
	DEBUG9(printk("%s(%ld): inst=%ld sending rnid cmd.\n",
	    __func__, vha->host_no, vha->instance));

	ret = qla2x00_send_rnid_mbx(vha, fcport->loop_id,
	    (uint8_t)tmp_rnid->DataFormat, vha->ioctl_mem_phys,
	    SEND_RNID_RSP_SIZE, &mb[0]);

	if (ret != QLA_SUCCESS) {
		/* error */
		pext->Status = EXT_STATUS_ERR;

                DEBUG9_10(printk("%s(%ld): inst=%ld FAILED. rval = %x.\n",
                    __func__, vha->host_no, vha->instance, mb[0]));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	DEBUG9(printk("%s(%ld): inst=%ld rnid cmd sent ok.\n",
	    __func__, vha->host_no, vha->instance));

	/* Copy the response */
	copy_len = (pext->ResponseLen > SEND_RNID_RSP_SIZE) ?
	    SEND_RNID_RSP_SIZE : pext->ResponseLen;

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    vha->ioctl_mem, copy_len);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy rsp buf\n",
		    __func__, vha->host_no, vha->instance));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	if (SEND_RNID_RSP_SIZE > pext->ResponseLen) {
		pext->Status = EXT_STATUS_DATA_OVERRUN;
		DEBUG9(printk("%s(%ld): inst=%ld data overrun. "
		    "exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	} else {
		pext->Status = EXT_STATUS_OK;
		DEBUG9(printk("%s(%ld): inst=%ld exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	}
	pext->ResponseLen = copy_len;

	qla2x00_free_ioctl_scrap_mem(vha);
	return (ret);
}

/*
 * qla2x00_set_host_data
 *	IOCTL command to set host/adapter related data.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_set_host_data(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int	ret = 0;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* switch on command subcode */
	switch (pext->SubCode) {
	case EXT_SC_SET_RNID:
		ret = qla2x00_set_rnid_params(vha, pext, mode);
		break;
	case EXT_SC_SET_BEACON_STATE:
		if (!IS_QLA2100(vha->hw) && !IS_QLA2200(vha->hw)) {
			ret = qla2x00_set_led_state(vha, pext, mode);
			break;
		}
		/*FALLTHROUGH*/
	default:
		/* function not supported. */
		pext->Status = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

/*
 * qla2x00_set_rnid_params
 *	IOCTL to set RNID parameters of the adapter.
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_set_rnid_params(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	EXT_SET_RNID_REQ	*tmp_set;
	EXT_RNID_DATA	*tmp_buf;
	int		ret = 0;
	int		tmp_rval = 0;
	uint16_t	mb[MAILBOX_REGISTER_COUNT];

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	/* check on loop down */
	if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) || 
	    vha->dpc_active) {

		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld loop not ready.\n",
		    __func__, vha->host_no, vha->instance));

		return (ret);
	}

	if (pext->RequestLen != sizeof(EXT_SET_RNID_REQ)) {
		/* parameter error */
		pext->Status = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk("%s(%ld): inst=%ld invalid request length.\n",
		    __func__, vha->host_no, vha->instance));
		return(ret);
	}

	if (qla2x00_get_ioctl_scrap_mem(vha, (void **)&tmp_set,
	    sizeof(EXT_SET_RNID_REQ))) {
		/* not enough memory */
		pext->Status = EXT_STATUS_NO_MEMORY;
		DEBUG9_10(printk("%s(%ld): inst=%ld scrap not big enough. "
		    "size requested=%ld.\n",
		    __func__, vha->host_no, vha->instance,
		    (ulong)sizeof(EXT_SET_RNID_REQ)));
		return (ret);
	}

	ret = copy_from_user(tmp_set, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), pext->RequestLen);
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld ERROR copy req buf ret=%d\n",
		    __func__, vha->host_no, vha->instance, ret));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (-EFAULT);
	}

	tmp_rval = qla2x00_get_rnid_params_mbx(vha, vha->ioctl_mem_phys,
	    sizeof(EXT_RNID_DATA), &mb[0]);
	if (tmp_rval != QLA_SUCCESS) {
		/* error */
		pext->Status = EXT_STATUS_ERR;

                DEBUG9_10(printk("%s(%ld): inst=%ld read cmd FAILED=%x.\n",
                    __func__, vha->host_no, vha->instance, mb[0]));
		qla2x00_free_ioctl_scrap_mem(vha);
		return (ret);
	}

	tmp_buf = (EXT_RNID_DATA *)vha->ioctl_mem;
	/* Now set the params. */
	memcpy(tmp_buf->IPVersion, tmp_set->IPVersion, 2);
	memcpy(tmp_buf->UDPPortNumber, tmp_set->UDPPortNumber, 2);
	memcpy(tmp_buf->IPAddress, tmp_set->IPAddress, 16);
	tmp_rval = qla2x00_set_rnid_params_mbx(vha, vha->ioctl_mem_phys,
	    sizeof(EXT_RNID_DATA), &mb[0]);

	if (tmp_rval != QLA_SUCCESS) {
		/* error */
		pext->Status = EXT_STATUS_ERR;

		DEBUG9_10(printk("%s(%ld): inst=%ld set cmd FAILED=%x.\n",
		    __func__, vha->host_no, vha->instance, mb[0]));
	} else {
		pext->Status = EXT_STATUS_OK;
		DEBUG9(printk("%s(%ld): inst=%ld exiting normally.\n",
		    __func__, vha->host_no, vha->instance));
	}

	qla2x00_free_ioctl_scrap_mem(vha);
	return (ret);
}

/*
 *qla2x00_set_led_state
 *	IOCTL to set QLA2XXX HBA LED state
 *
 * Input:
 *	ha = adapter state pointer.
 *	pext = User space CT arguments pointer.
 *	mode = flags.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_set_led_state(scsi_qla_host_t *vha, EXT_IOCTL *pext, int mode)
{
	int			ret = 0;
	uint32_t		tmp_ext_stat = 0;
	uint32_t		tmp_ext_dstat = 0;
	EXT_BEACON_CONTROL	tmp_led_state;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (pext->RequestLen < sizeof(EXT_BEACON_CONTROL)) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s: ERROR RequestLen too small.\n",
		    __func__));
		return (ret);
	}

	if (test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) {
		pext->Status = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld abort isp active.\n",
		     __func__, vha->host_no, vha->instance));
		return (ret);
	}

	ret = copy_from_user(&tmp_led_state, Q64BIT_TO_PTR(pext->RequestAdr,
	    pext->AddrMode), sizeof(EXT_BEACON_CONTROL));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s(%ld): inst=%ld ERROR copy req buf=%d.\n",
		    __func__, vha->host_no, vha->instance, ret));
		return (-EFAULT);
	}

	if (IS_QLA23XX(vha->hw)) {
		ret = qla2x00_set_led_23xx(vha, &tmp_led_state, 
		    &tmp_ext_stat, &tmp_ext_dstat);
	} else if (IS_QLA24XX(vha->hw) || IS_QLA54XX(vha->hw)) {
		ret = qla2x00_set_led_24xx(vha, &tmp_led_state, 
		    &tmp_ext_stat, &tmp_ext_dstat);
	} else {
		/* not supported */
		tmp_ext_stat = EXT_STATUS_UNSUPPORTED_SUBCODE;
	}

	pext->Status       = tmp_ext_stat;
	pext->DetailStatus = tmp_ext_dstat;

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

static int
qla2x00_set_led_23xx(scsi_qla_host_t *vha, EXT_BEACON_CONTROL *ptmp_led_state,
    uint32_t *pext_stat, uint32_t *pext_dstat)
{
	int			ret = 0;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	uint16_t		gpio_enable, gpio_data;
	unsigned long		cpu_flags = 0;


	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	if (ptmp_led_state->State != EXT_DEF_GRN_BLINK_ON &&
	    ptmp_led_state->State != EXT_DEF_GRN_BLINK_OFF) {
		*pext_stat = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Unknown Led State set "
		    "operation recieved %x.\n",
		    __func__, vha->host_no, vha->instance,
		    ptmp_led_state->State));
		return (ret);
	}

	if (test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) {
		*pext_stat = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld abort isp active.\n",
		     __func__, vha->host_no, vha->instance));
		return (ret);
	}

	switch (ptmp_led_state->State) {
	case EXT_DEF_GRN_BLINK_ON:

		DEBUG9(printk("%s(%ld): inst=%ld start blinking led \n",
		    __func__, vha->host_no, vha->instance));

		DEBUG9(printk("%s(%ld): inst=%ld orig firmware options "
		    "fw_options1=0x%x fw_options2=0x%x fw_options3=0x%x.\n",
		     __func__, vha->host_no, vha->instance, ha->fw_options[1],
		     ha->fw_options[2], ha->fw_options[3]));

		ha->fw_options[1] &= ~FO1_SET_EMPHASIS_SWING;
		ha->fw_options[1] |= FO1_DISABLE_GPIO6_7;

		if (qla2x00_set_fw_options(vha, ha->fw_options) != QLA_SUCCESS) {
			*pext_stat = EXT_STATUS_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld set"
			    "firmware  options failed.\n",
			    __func__, vha->host_no, vha->instance));
			break;
		}

		if (ha->pio_address)
			reg = (struct device_reg_2xxx *)ha->pio_address;

		/* Turn off LEDs */
		spin_lock_irqsave(&ha->hardware_lock, cpu_flags);
		if (ha->pio_address) {
			gpio_enable = RD_REG_WORD_PIO(&reg->gpioe);
			gpio_data   = RD_REG_WORD_PIO(&reg->gpiod);
		} else {
			gpio_enable = RD_REG_WORD(&reg->gpioe);
			gpio_data   = RD_REG_WORD(&reg->gpiod);
		}
		gpio_enable |= GPIO_LED_MASK;

		/* Set the modified gpio_enable values */
		if (ha->pio_address)
			WRT_REG_WORD_PIO(&reg->gpioe, gpio_enable);
		else {
			WRT_REG_WORD(&reg->gpioe, gpio_enable);
			RD_REG_WORD(&reg->gpioe);
		}

		/* Clear out previously set LED colour */
		gpio_data &= ~GPIO_LED_MASK;
		if (ha->pio_address)
			WRT_REG_WORD_PIO(&reg->gpiod, gpio_data);
		else {
			WRT_REG_WORD(&reg->gpiod, gpio_data);
			RD_REG_WORD(&reg->gpiod);
		}
		spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);

		/* Let the per HBA timer kick off the blinking process based on
		 * the following flags. No need to do anything else now.
		 */
		ha->beacon_blink_led = 1;
		ha->beacon_color_state = 0;

		/* end of if (ptmp_led_state.State == EXT_DEF_GRN_BLINK_ON)) */

		*pext_stat  = EXT_STATUS_OK;
		*pext_dstat = EXT_STATUS_OK;
		break;

	case EXT_DEF_GRN_BLINK_OFF:
		DEBUG9(printk("%s(%ld): inst=%ld stop blinking led \n",
		    __func__, vha->host_no, vha->instance));

		ha->beacon_blink_led = 0;
		/* Set the on flag so when it gets flipped it will be off */
		if (IS_QLA2322(ha)) {
			ha->beacon_color_state = QLA_LED_ALL_ON;
		} else {
			ha->beacon_color_state = QLA_LED_GRN_ON;
		}
		qla23xx_blink_led(vha);	/* This turns green LED off */

		DEBUG9(printk("%s(%ld): inst=%ld orig firmware"
		    " options fw_options1=0x%x fw_options2=0x%x "
		    "fw_options3=0x%x.\n",
		    __func__, vha->host_no, vha->instance, ha->fw_options[1],
		    ha->fw_options[2], ha->fw_options[3]));

		ha->fw_options[1] &= ~FO1_SET_EMPHASIS_SWING;
		ha->fw_options[1] &= ~FO1_DISABLE_GPIO6_7;

		if (qla2x00_set_fw_options(vha, ha->fw_options) != QLA_SUCCESS) {
			*pext_stat = EXT_STATUS_ERR;
			DEBUG9_10(printk("%s(%ld): inst=%ld set"
			    "firmware  options failed.\n",
			    __func__, vha->host_no, vha->instance));
			break;
		}

		/* end of if (ptmp_led_state.State == EXT_DEF_GRN_BLINK_OFF) */

		*pext_stat  = EXT_STATUS_OK;
		*pext_dstat = EXT_STATUS_OK;
		break;
	default:
		*pext_stat = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (ret);
}

static int
qla2x00_set_led_24xx(scsi_qla_host_t *vha, EXT_BEACON_CONTROL *ptmp_led_state,
    uint32_t *pext_stat, uint32_t *pext_dstat)
{
	int			rval = 0;
	/* struct device_reg_24xx __iomem *reg24 = &ha->iobase->isp24; */
	uint32_t		led_state;
	struct qla_hw_data *ha = vha->hw;

	DEBUG9(printk("%s(%ld): inst=%ld entered.\n",
	    __func__, vha->host_no, vha->instance));

	led_state = ptmp_led_state->State;
	if (led_state != EXT_DEF_GRN_BLINK_ON &&
	    led_state != EXT_DEF_GRN_BLINK_OFF) {
		*pext_stat = EXT_STATUS_INVALID_PARAM;
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld Unknown Led State set "
		    "operation recieved %x.\n",
		    __func__, vha->host_no, vha->instance,
		    ptmp_led_state->State));
		return (rval);
	}

	if (test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) {
		*pext_stat = EXT_STATUS_BUSY;
		DEBUG9_10(printk("%s(%ld): inst=%ld abort isp active.\n",
		     __func__, vha->host_no, vha->instance));
		return (rval);
	}

	DEBUG9_10(printk("%s(%ld): inst=%ld orig firmware options "
	    "fw_options1=0x%x fw_options2=0x%x fw_options3=0x%x.\n",
	     __func__, vha->host_no, vha->instance, ha->fw_options[1],
	     ha->fw_options[2], ha->fw_options[3]));

	switch (led_state) {
	case EXT_DEF_GRN_BLINK_ON:

		DEBUG9(printk("%s(%ld): inst=%ld start blinking led \n",
		    __func__, vha->host_no, vha->instance));

		if(ha->isp_ops->beacon_on(vha) != QLA_SUCCESS) {
			DEBUG9_10(printk("%s(%ld): inst=%ld "
                                    "beacon on failed.\n",
                                    __func__, vha->host_no, vha->instance));
                                break;
		}	
		
		*pext_stat  = EXT_STATUS_OK;
		*pext_dstat = EXT_STATUS_OK;

		DEBUG9(printk("%s(%ld): inst=%ld LED setup to blink.\n",
		    __func__, vha->host_no, vha->instance));

		break;

	case EXT_DEF_GRN_BLINK_OFF:
		DEBUG9(printk("%s(%ld): inst=%ld stop blinking led \n",
		    __func__, vha->host_no, vha->instance));

		if(ha->isp_ops->beacon_off(vha) != QLA_SUCCESS) {
                        DEBUG9_10(printk("%s(%ld): inst=%ld "
                                    "beacon off failed.\n",
                                    __func__, vha->host_no, vha->instance));
                                break;
                }
		
		*pext_stat  = EXT_STATUS_OK;
		*pext_dstat = EXT_STATUS_OK;

		DEBUG9(printk("%s(%ld): inst=%ld all LED blinking stopped.\n",
		    __func__, vha->host_no, vha->instance));

		break;

	default:
		DEBUG9_10(printk(
		    "%s(%ld): inst=%ld invalid state received=%x.\n",
		    __func__, vha->host_no, vha->instance, led_state));

		*pext_stat = EXT_STATUS_UNSUPPORTED_SUBCODE;
		break;
	}

	DEBUG9(printk("%s(%ld): inst=%ld exiting.\n",
	    __func__, vha->host_no, vha->instance));

	return (rval);
}

/*
 * qla2x00_get_driver_specifics
 *	Returns driver specific data in the response buffer.
 *
 * Input:
 *	pext = pointer to EXT_IOCTL structure containing values from user.
 *
 * Returns:
 *	0 = success
 *	others = errno value
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_get_driver_specifics(EXT_IOCTL *pext, scsi_qla_host_t *vha)
{
	int			ret = 0;
	EXT_LN_DRIVER_DATA	data;

	DEBUG9(printk("%s: entered.\n", __func__));

	if (pext->ResponseLen < sizeof(EXT_LN_DRIVER_DATA)) {
		pext->Status = EXT_STATUS_BUFFER_TOO_SMALL;
		DEBUG9_10(printk("%s: ERROR ResponseLen too small.\n",
		    __func__));

		return (ret);
	}
	/* Clear out the data */
	memset(&data, 0, sizeof(EXT_LN_DRIVER_DATA));

	data.DrvVer.Major = QLA_DRIVER_MAJOR_VER;
	data.DrvVer.Minor = QLA_DRIVER_MINOR_VER;
	data.DrvVer.Patch = QLA_DRIVER_PATCH_VER;
	data.DrvVer.Beta = QLA_DRIVER_BETA_VER;

	/* This driver supports large luns */
	data.Flags |= EXT_DEF_SUPPORTS_LARGE_LUN;

	/* Driver supports empty preferred mask */
	data.Flags |= EXT_DEF_SUPPORTS_NO_PREF_CHUNKS;

	ret = copy_to_user(Q64BIT_TO_PTR(pext->ResponseAdr, pext->AddrMode),
	    &data, sizeof(EXT_LN_DRIVER_DATA));
	if (ret) {
		pext->Status = EXT_STATUS_COPY_ERR;
		DEBUG9_10(printk("%s: ERROR copy resp buf = %d.\n",
		    __func__, ret));
		ret = -EFAULT;
	} else {
		pext->Status = EXT_STATUS_OK;
	}

	DEBUG9(printk("%s: exiting. ret=%d.\n", __func__, ret));

	return (ret);
}

/*
 * qla2x00_lun_discovery
 *	Issue SCSI inquiry command for LUN discovery.
 *
 * Input:
 *	ha:		adapter state pointer.
 *	fcport:		FC port structure pointer.
 *
 * Context:
 *	Kernel context.
 */
static void
qla2x00_lun_discovery(scsi_qla_host_t *vha, fc_port_t *fcport)
{
	inq_cmd_rsp_t	*inq;
	dma_addr_t	inq_dma;
	uint16_t	lun;
	struct qla_hw_data *ha = vha->hw;

	inq = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL, &inq_dma);
	if (inq == NULL) {
		qla_printk(KERN_WARNING, ha,
		    "Memory Allocation failed - INQ\n");
		return;
	}

	/* If report LUN works, exit. */
	if (qla2x00_rpt_lun_discovery(vha, fcport, inq, inq_dma) !=
	    QLA_SUCCESS) {
		for (lun = 0; lun < 255; lun++) {
			/* Configure LUN. */
			qla2x00_cfg_lun(vha, fcport, lun, inq, inq_dma);
		}
	}

	dma_pool_free(ha->s_dma_pool, inq, inq_dma);
}

/*
 * qla2x00_rpt_lun_discovery
 *	Issue SCSI report LUN command for LUN discovery.
 *
 * Input:
 *	ha:		adapter state pointer.
 *	fcport:		FC port structure pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_rpt_lun_discovery(scsi_qla_host_t *vha, fc_port_t *fcport,
    inq_cmd_rsp_t *inq, dma_addr_t inq_dma)
{
	int			rval;
	uint32_t		len, cnt;
	uint16_t		lun;
	struct qla_hw_data *ha = vha->hw;

	/* Assume a failed status */
	rval = QLA_FUNCTION_FAILED;

	/* No point in continuing if the device doesn't support RLC */
	if ((fcport->flags & FCF_RLC_SUPPORT) == 0) {
		return (rval);
	}

	rval = qla2x00_report_lun(vha, fcport);
	if (rval != QLA_SUCCESS) {
		return (rval);
	}

	len = be32_to_cpu(ha->rlc_rsp->list.hdr.len);
	len /= 8;
	if (len > MAX_LUNS)
		len = MAX_LUNS;
	for (cnt = 0; cnt < len; cnt++) {
		lun = CHAR_TO_SHORT(ha->rlc_rsp->list.lst[cnt].lsb,
		    ha->rlc_rsp->list.lst[cnt].msb.b);

		DEBUG3(printk("scsi(%ld): RLC lun = (%d)\n", vha->host_no, lun));

		/* We only support 0 through MAX_LUNS-1 range */
		if (lun < MAX_LUNS) {
			qla2x00_cfg_lun(vha, fcport, lun, inq, inq_dma);
		}
	}

	return (rval);
}

/*
 * qla2x00_report_lun
 *	Issue SCSI report LUN command.
 *
 * Input:
 *	ha:		adapter state pointer.
 *	fcport:		FC port structure pointer.
 *
 * Returns:
 *	qla2x00 local function return status code.
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_report_lun(scsi_qla_host_t *vha, fc_port_t *fcport)
{
	int rval;
	uint16_t retries;
	uint16_t comp_status;
	uint16_t scsi_status;
	uint16_t *cstatus, *sstatus;
	uint8_t *sense_data;
	rpt_lun_cmd_rsp_t *rlc;
	dma_addr_t rlc_dma;
	uint16_t next_loopid;
	struct qla_hw_data *ha = vha->hw;

	rval = QLA_FUNCTION_FAILED;
	rlc = ha->rlc_rsp;
	rlc_dma = ha->rlc_rsp_dma;

	if (IS_FWI2_CAPABLE(ha)) {
		cstatus = &rlc->p.rsp24.comp_status;
		sstatus = &rlc->p.rsp24.scsi_status;
		sense_data = rlc->p.rsp24.data;
	} else {
		cstatus = &rlc->p.rsp.comp_status;
		sstatus = &rlc->p.rsp.scsi_status;
		sense_data = rlc->p.rsp.req_sense_data;
	}

	for (retries = 3; retries; retries--) {
		if (IS_FWI2_CAPABLE(ha)) {
			memset(rlc, 0, sizeof(rpt_lun_cmd_rsp_t));
			rlc->p.cmd24.entry_type = COMMAND_TYPE_7;
			rlc->p.cmd24.entry_count = 1;
			rlc->p.cmd24.nport_handle =
			    cpu_to_le16(fcport->loop_id);
			rlc->p.cmd24.port_id[0] = fcport->d_id.b.al_pa;
			rlc->p.cmd24.port_id[1] = fcport->d_id.b.area;
			rlc->p.cmd24.port_id[2] = fcport->d_id.b.domain;
			rlc->p.cmd24.task_mgmt_flags =
			    __constant_cpu_to_le16(TMF_READ_DATA);
			rlc->p.cmd24.task = TSK_SIMPLE;
			rlc->p.cmd24.fcp_cdb[0] = REPORT_LUNS;
			rlc->p.cmd24.fcp_cdb[8] = MSB(sizeof(rpt_lun_lst_t));
			rlc->p.cmd24.fcp_cdb[9] = LSB(sizeof(rpt_lun_lst_t));
			host_to_fcp_swap(rlc->p.cmd24.fcp_cdb,
			    sizeof(rlc->p.cmd24.fcp_cdb));
			rlc->p.cmd24.dseg_count = __constant_cpu_to_le16(1);
			rlc->p.cmd24.timeout = __constant_cpu_to_le16(10);
			rlc->p.cmd24.byte_count =
			    __constant_cpu_to_le32(sizeof(rpt_lun_lst_t));

			rlc->p.cmd24.dseg_0_address[0] = cpu_to_le32(
			    LSD(rlc_dma + sizeof(struct sts_entry_24xx)));
			rlc->p.cmd24.dseg_0_address[1] = cpu_to_le32(
			    MSD(rlc_dma + sizeof(struct sts_entry_24xx)));
			rlc->p.cmd24.dseg_0_len =
			    __constant_cpu_to_le32(sizeof(rpt_lun_lst_t));
		} else {
			memset(rlc, 0, sizeof(rpt_lun_cmd_rsp_t));
			rlc->p.cmd.entry_type = COMMAND_A64_TYPE;
			rlc->p.cmd.entry_count = 1;
			SET_TARGET_ID(ha, rlc->p.cmd.target, fcport->loop_id);
			rlc->p.cmd.control_flags =
			    __constant_cpu_to_le16(CF_READ | CF_SIMPLE_TAG);
			rlc->p.cmd.scsi_cdb[0] = REPORT_LUNS;
			rlc->p.cmd.scsi_cdb[8] = MSB(sizeof(rpt_lun_lst_t));
			rlc->p.cmd.scsi_cdb[9] = LSB(sizeof(rpt_lun_lst_t));
			rlc->p.cmd.dseg_count = __constant_cpu_to_le16(1);
			rlc->p.cmd.timeout = __constant_cpu_to_le16(10);
			rlc->p.cmd.byte_count =
			    __constant_cpu_to_le32(sizeof(rpt_lun_lst_t));
			rlc->p.cmd.dseg_0_address[0] = cpu_to_le32(
			    LSD(rlc_dma + sizeof(sts_entry_t)));
			rlc->p.cmd.dseg_0_address[1] = cpu_to_le32(
			    MSD(rlc_dma + sizeof(sts_entry_t)));
			rlc->p.cmd.dseg_0_length =
			    __constant_cpu_to_le32(sizeof(rpt_lun_lst_t));
		}

		rval = qla2x00_issue_iocb(vha, rlc, rlc_dma,
		    sizeof(rpt_lun_cmd_rsp_t));

		if (rval == QLA_SUCCESS && rlc->p.rsp.entry_status != 0) {
			DEBUG(printk("scsi(%ld): RLC failed to complete IOCB "
			    "-- error status (%x).\n", vha->host_no,
			    rlc->p.rsp.entry_status));
			rval = QLA_FUNCTION_FAILED;
			break;
		}

		comp_status = le16_to_cpup(cstatus);
		scsi_status = le16_to_cpup(sstatus);

		if (rval != QLA_SUCCESS || comp_status != CS_COMPLETE ||
		    scsi_status & SS_CHECK_CONDITION) {

			/* Device underrun, treat as OK. */
			if (rval == QLA_SUCCESS &&
			    comp_status == CS_DATA_UNDERRUN &&
			    scsi_status & SS_RESIDUAL_UNDER &&
			    !(scsi_status & SS_CHECK_CONDITION)) {

				rval = QLA_SUCCESS;
				break;
			}

			DEBUG(printk("scsi(%ld): RLC failed to issue iocb! "
			    "fcport=[%04x/%p] rval=%x cs=%x ss=%x\n",
			    vha->host_no, fcport->loop_id, fcport, rval,
			    comp_status, scsi_status));

			/*
			 * If the device loggod-out, then re-login and try
			 * again.
			 */
			if (rval == QLA_SUCCESS &&
			    comp_status == CS_PORT_LOGGED_OUT &&
			    atomic_read(&fcport->state) != FCS_DEVICE_DEAD) {
				if (fcport->flags & FCF_FABRIC_DEVICE) {
					DEBUG2(printk("scsi(%ld): Attempting "
					    "to re-login to %04x/%p.\n",
					    vha->host_no, fcport->loop_id,
					    fcport));
					next_loopid = 0;
					qla2x00_fabric_login(vha, fcport,
					    &next_loopid);
					continue;
				} else {
					/* Loop device gone but no LIP... */
					rval = QLA_FUNCTION_FAILED;
					break;
				}
			}

			if (scsi_status & SS_CHECK_CONDITION) {
				/* Skip past any FCP RESPONSE data. */
				if (IS_FWI2_CAPABLE(ha)) {
					host_to_fcp_swap(sense_data,
					    sizeof(rlc->p.rsp24.data));
					if (scsi_status &
					    SS_RESPONSE_INFO_LEN_VALID)
						sense_data += le32_to_cpu(
						    rlc->p.rsp24.rsp_data_len);
				}

				DEBUG2(printk("scsi(%ld): RLC "
				    "SS_CHECK_CONDITION Sense Data "
				    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				    vha->host_no, sense_data[0], sense_data[1],
				    sense_data[2], sense_data[3], sense_data[4],
				    sense_data[5], sense_data[6],
				    sense_data[7]));
				if (sense_data[2] == ILLEGAL_REQUEST) {
					fcport->flags &= ~(FCF_RLC_SUPPORT);
					break;
				}
			}
		} else {
			break;
		}
	}

	return (rval);
}

/*
 * qla2x00_cfg_lun
 *	Configures LUN into fcport LUN list.
 *
 * Input:
 *	fcport:		FC port structure pointer.
 *	lun:		LUN number.
 *
 * Context:
 *	Kernel context.
 */
static fc_lun_t *
qla2x00_cfg_lun(scsi_qla_host_t *vha, fc_port_t *fcport, uint16_t lun,
    inq_cmd_rsp_t *inq, dma_addr_t inq_dma)
{
	fc_lun_t *fclun;
	uint8_t	  device_type;

	/* Bypass LUNs that failed. */
	if (qla2x00_inquiry(vha, fcport, lun, inq, inq_dma) != QLA_SUCCESS) {
		DEBUG2(printk("scsi(%ld): Failed inquiry - loop id=0x%04x "
		    "lun=%d\n", vha->host_no, fcport->loop_id, lun));

		return (NULL);
	}
	device_type = (inq->inq[0] & 0x1f);
	switch (device_type) {
	case TYPE_DISK:
	case TYPE_PROCESSOR:
	case TYPE_WORM:
	case TYPE_ROM:
	case TYPE_SCANNER:
	case TYPE_MOD:
	case TYPE_MEDIUM_CHANGER:
	case TYPE_ENCLOSURE:
	case 0x20:
	case 0x0C:
		break;
	case TYPE_TAPE:
		fcport->flags |= FCF_TAPE_PRESENT;
		break;
	default:
		DEBUG2(printk("scsi(%ld): Unsupported lun type -- "
		    "loop id=0x%04x lun=%d type=%x\n",
		    vha->host_no, fcport->loop_id, lun, device_type));
		return (NULL);
	}

	fcport->device_type = device_type;
	fclun = qla2x00_add_lun(fcport, lun);

	return (fclun);
}

/*
 * qla2x00_add_lun
 *	Adds LUN to database
 *
 * Input:
 *	fcport:		FC port structure pointer.
 *	lun:		LUN number.
 *
 * Context:
 *	Kernel context.
 */
static fc_lun_t *
qla2x00_add_lun(fc_port_t *fcport, uint64_t lun)
{
	int		found;
	fc_lun_t	*fclun;

	if (fcport == NULL) {
		DEBUG(printk("scsi: Unable to add lun to NULL port\n"));
		return (NULL);
	}

	/* Allocate LUN if not already allocated. */
	found = 0;
	list_for_each_entry(fclun, &fcport->fcluns, list) {
		if (fclun->lun == lun) {
			found++;
			break;
		}
	}
	if (found) {
		fclun->device_type = fcport->device_type;
		DEBUG3(printk("scsi(%ld): Clear lun -- "
		    "loop id=0x%04x lun=%d\n",
		    fcport->vha->host_no, fcport->loop_id,
			(int)fclun->lun));
		return (fclun);
	}

	fclun = kmalloc(sizeof(fc_lun_t), GFP_KERNEL);
	if (fclun == NULL) {
		printk(KERN_WARNING
		    "%s(): Memory Allocation failed - FCLUN\n",
		    __func__);
		return (NULL);
	}

	/* Setup LUN structure. */
	memset(fclun, 0, sizeof(fc_lun_t));
	fclun->lun = lun;
	fclun->fcport = fcport;
	fclun->device_type = fcport->device_type;

	DEBUG3(printk("scsi(%ld): Add lun %p -- loop id=0x%04x lun=%d, "
	    "flags=%x\n", fcport->vha->host_no, fclun, fcport->loop_id,
	    (int)fclun->lun, fclun->flags));

	list_add_tail(&fclun->list, &fcport->fcluns);

	return (fclun);
}

/*
 * qla2x00_inquiry
 *	Issue SCSI inquiry command.
 *
 * Input:
 *	ha = adapter block pointer.
 *	fcport = FC port structure pointer.
 *
 * Return:
 *	0  - Success
 *  BIT_0 - error
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_inquiry(scsi_qla_host_t *vha, fc_port_t *fcport, uint16_t lun,
    inq_cmd_rsp_t *inq, dma_addr_t inq_dma)
{
	int rval;
	uint16_t retries;
	uint16_t comp_status;
	uint16_t scsi_status;
	uint16_t *cstatus, *sstatus;
	uint8_t *sense_data;
	uint16_t next_loopid;
	struct qla_hw_data *ha = vha->hw;

	rval = QLA_FUNCTION_FAILED;

	if (IS_FWI2_CAPABLE(ha)) {
		cstatus = &inq->p.rsp24.comp_status;
		sstatus = &inq->p.rsp24.scsi_status;
		sense_data = inq->p.rsp24.data;
	} else {
		cstatus = &inq->p.rsp.comp_status;
		sstatus = &inq->p.rsp.scsi_status;
		sense_data = inq->p.rsp.req_sense_data;
	}

	for (retries = 3; retries; retries--) {
		if (IS_FWI2_CAPABLE(ha)) {
			memset(inq, 0, sizeof(inq_cmd_rsp_t));
			inq->p.cmd24.entry_type = COMMAND_TYPE_7;
			inq->p.cmd24.entry_count = 1;
			inq->p.cmd24.nport_handle =
			    cpu_to_le16(fcport->loop_id);
			inq->p.cmd24.port_id[0] = fcport->d_id.b.al_pa;
			inq->p.cmd24.port_id[1] = fcport->d_id.b.area;
			inq->p.cmd24.port_id[2] = fcport->d_id.b.domain;
			int_to_fcp_lun(lun, inq->p.cmd24.lun.scsi_lun);
			host_to_fcp_swap(inq->p.cmd24.lun.scsi_lun,
			    sizeof(inq->p.cmd24.lun));
			inq->p.cmd24.task_mgmt_flags =
			    __constant_cpu_to_le16(TMF_READ_DATA);
			inq->p.cmd24.task = TSK_SIMPLE;
			inq->p.cmd24.fcp_cdb[0] = INQUIRY;
			inq->p.cmd24.fcp_cdb[4] = INQ_DATA_SIZE;
			host_to_fcp_swap(inq->p.cmd24.fcp_cdb,
			    sizeof(inq->p.cmd24.fcp_cdb));
			inq->p.cmd24.dseg_count = __constant_cpu_to_le16(1);
			inq->p.cmd24.timeout = __constant_cpu_to_le16(10);
			inq->p.cmd24.byte_count =
			    __constant_cpu_to_le32(INQ_DATA_SIZE);
			inq->p.cmd24.dseg_0_address[0] = cpu_to_le32(
			    LSD(inq_dma + sizeof(struct sts_entry_24xx)));
			inq->p.cmd24.dseg_0_address[1] = cpu_to_le32(
			    MSD(inq_dma + sizeof(struct sts_entry_24xx)));
			inq->p.cmd24.dseg_0_len =
			    __constant_cpu_to_le32(INQ_DATA_SIZE);
			if (vha->vp_idx) {
				inq->p.cmd24.vp_index = vha->vp_idx;
			}
		} else {
			memset(inq, 0, sizeof(inq_cmd_rsp_t));
			inq->p.cmd.entry_type = COMMAND_A64_TYPE;
			inq->p.cmd.entry_count = 1;
			inq->p.cmd.lun = cpu_to_le16(lun);
			SET_TARGET_ID(ha, inq->p.cmd.target, fcport->loop_id);
			inq->p.cmd.control_flags =
			    __constant_cpu_to_le16(CF_READ | CF_SIMPLE_TAG);
			inq->p.cmd.scsi_cdb[0] = INQUIRY;
			inq->p.cmd.scsi_cdb[4] = INQ_DATA_SIZE;
			inq->p.cmd.dseg_count = __constant_cpu_to_le16(1);
			inq->p.cmd.timeout = __constant_cpu_to_le16(10);
			inq->p.cmd.byte_count =
			    __constant_cpu_to_le32(INQ_DATA_SIZE);
			inq->p.cmd.dseg_0_address[0] = cpu_to_le32(
			    LSD(inq_dma + sizeof(sts_entry_t)));
			inq->p.cmd.dseg_0_address[1] = cpu_to_le32(
			    MSD(inq_dma + sizeof(sts_entry_t)));
			inq->p.cmd.dseg_0_length =
			    __constant_cpu_to_le32(INQ_DATA_SIZE);
		}

		DEBUG5(printk("scsi(%ld): Lun Inquiry - fcport=[%04x/%p],"
		    " lun (%d)\n",
		    vha->host_no, fcport->loop_id, fcport, lun));

		rval = qla2x00_issue_iocb(vha, inq, inq_dma,
		    sizeof(inq_cmd_rsp_t));

		if (rval == QLA_SUCCESS && inq->p.rsp.entry_status != 0) {
			DEBUG(printk("scsi(%ld): INQ failed to complete IOCB "
			    "-- error status (%x).\n", vha->host_no,
			    inq->p.rsp.entry_status));
			rval = QLA_FUNCTION_FAILED;
			break;
		}

		comp_status = le16_to_cpup(cstatus);
		scsi_status = le16_to_cpup(sstatus);

		DEBUG5(printk("scsi(%ld): lun (%d) inquiry - "
		    "inq[0]= 0x%x, comp status 0x%x, scsi status 0x%x, "
		    "rval=%d\n",
		    vha->host_no, lun, inq->inq[0], comp_status, scsi_status,
		    rval));

		if (rval != QLA_SUCCESS || comp_status != CS_COMPLETE ||
		    scsi_status & SS_CHECK_CONDITION) {

			DEBUG(printk("scsi(%ld): INQ failed to issue iocb! "
			    "fcport=[%04x/%p] rval=%x cs=%x ss=%x\n",
			    vha->host_no, fcport->loop_id, fcport, rval,
			    comp_status, scsi_status));

			/*
			 * If the device loggod-out, then re-login and try
			 * again.
			 */
			if (rval == QLA_SUCCESS &&
			    comp_status == CS_PORT_LOGGED_OUT &&
			    atomic_read(&fcport->state) != FCS_DEVICE_DEAD) {
				if (fcport->flags & FCF_FABRIC_DEVICE) {
					DEBUG2(printk("scsi(%ld): Attempting "
					    "to re-login to %04x/%p.\n",
					    vha->host_no, fcport->loop_id,
					    fcport));
					next_loopid = 0;
					qla2x00_fabric_login(vha, fcport,
					    &next_loopid);
					continue;
				} else {
					/* Loop device gone but no LIP... */
					rval = QLA_FUNCTION_FAILED;
					break;
				}
			}

			if (rval == QLA_SUCCESS)
				rval = QLA_FUNCTION_FAILED;

			if (scsi_status & SS_CHECK_CONDITION) {
				/* Skip past any FCP RESPONSE data. */
				if (IS_FWI2_CAPABLE(ha)) {
					host_to_fcp_swap(sense_data,
					    sizeof(inq->p.rsp24.data));
					if (scsi_status &
					    SS_RESPONSE_INFO_LEN_VALID)
						sense_data += le32_to_cpu(
						    inq->p.rsp24.rsp_data_len);
				}

				DEBUG2(printk("scsi(%ld): INQ "
				    "SS_CHECK_CONDITION Sense Data "
				    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				    vha->host_no, sense_data[0], sense_data[1],
				    sense_data[2], sense_data[3], sense_data[4],
				    sense_data[5], sense_data[6],
				    sense_data[7]));

			}

			/* Device underrun drop LUN. */
			if (comp_status == CS_DATA_UNDERRUN &&
			    scsi_status & SS_RESIDUAL_UNDER) {
				break;
			}
		} else {
			break;
		}
	}

	return (rval);
}

