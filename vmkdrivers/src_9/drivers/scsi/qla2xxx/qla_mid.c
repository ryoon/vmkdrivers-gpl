/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"
#include "qla_gbl.h"

#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/smp_lock.h>
#include <linux/list.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <linux/delay.h>

void qla2x00_vp_stop_timer(scsi_qla_host_t *);

void
qla2x00_vp_stop_timer(scsi_qla_host_t *vha)
{
	if (vha->vp_idx&& vha->timer_active) {
		del_timer_sync(&vha->timer);
		vha->timer_active = 0;
	}
}

uint32_t
qla24xx_allocate_vp_id(scsi_qla_host_t *vha)
{
	uint32_t vp_id;
	unsigned long   flags;
	struct qla_hw_data *ha = vha->hw;

	/* Find an empty slot and assign an vp_id */
	spin_lock_irqsave(&ha->vport_lock, flags);
	vp_id = find_first_zero_bit(ha->vp_idx_map, ha->max_npiv_vports + 1);
	if (vp_id > ha->max_npiv_vports) {
		DEBUG15(printk ("vp_id %d is bigger than max-supported %d.\n",
		    vp_id, ha->max_npiv_vports));
		spin_unlock_irqrestore(&ha->vport_lock, flags);
		return vp_id;
	}

	if(vha->fc_vport)
		ha->cur_vport_count++;

	set_bit(vp_id, ha->vp_idx_map);
	ha->num_vhosts++;
	vha->vp_idx = vp_id;
	list_add_tail(&vha->list, &ha->vp_list);
	spin_unlock_irqrestore(&ha->vport_lock, flags);
	return vp_id;
}

/*
* qla2xxx_deallocate_vp_id(): Move vport to vp_del_list.
* @vha: Vport context.
*
* Note: The caller of this function needs to ensure that
* the vport_lock of the corresponding physical port is
* held while making this call.
*/
void
qla24xx_deallocate_vp_id(scsi_qla_host_t *vha)
{
	uint16_t vp_id;
	struct qla_hw_data *ha = vha->hw;
  
	if(vha->fc_vport)
		ha->cur_vport_count--;

	vp_id = vha->vp_idx;
	ha->num_vhosts--;
	clear_bit(vp_id, ha->vp_idx_map);
	list_del(&vha->list);
	/* Add vport to physical port's del list. */
	list_add_tail(&vha->list, &ha->vp_del_list);
}

scsi_qla_host_t *
qla24xx_find_vhost_by_name(struct qla_hw_data *ha, uint8_t *port_name)
{
	scsi_qla_host_t *vha;
	unsigned long	flags;
 	struct scsi_qla_host *tvha;

	/* Locate matching device in database. */
 	spin_lock_irqsave(&ha->vport_lock, flags);
	list_for_each_entry_safe(vha, tvha, &ha->vp_list, list) {
		if (!memcmp(port_name, vha->port_name, WWN_SIZE))
			goto found_vha;	
	}
	vha = NULL;
found_vha:
 	spin_unlock_irqrestore(&ha->vport_lock, flags);
	return vha;
}

/*
 * qla2x00_mark_vp_devices_dead
 *	Updates fcport state when device goes offline.
 *
 * Input:
 *	ha = adapter block pointer.
 *
 * Return:
 *	None.
 *
 * Context:
 */
static void
qla2x00_mark_vp_devices_dead(scsi_qla_host_t *vha)
{
	fc_port_t *fcport;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {

		DEBUG15(printk("scsi(%ld): Marking port dead, "
		    "loop_id=0x%04x :%x\n",
		    vha->host_no, fcport->loop_id, fcport->vp_idx));

		atomic_set(&fcport->state, FCS_DEVICE_DEAD);
		qla2x00_mark_device_lost(vha, fcport, 0, 0);
		atomic_set(&fcport->state, FCS_UNCONFIGURED);
	}
}

/**************************************************************************
* qla2x00_eh_wait_for_vp_pending_commands
*
* Description:
*    Waits for all the commands to come back from the specified host.
*
* Input:
*    ha - pointer to scsi_qla_host structure.
*
* Returns:
*    0 : SUCCESS
*    1 : FAILED
*
* Note:
**************************************************************************/
int
qla2x00_eh_wait_for_vp_pending_commands(scsi_qla_host_t *vha)
{
	int cnt, status = 0;
	srb_t *sp;
	struct scsi_cmnd *cmd;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;
	 /*
	 * Waiting for all commands for the designated virtual port in
	 * the active array.
	 */
	req = vha->req;
	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		sp = req->outstanding_cmds[cnt];
		if (!sp)
			continue;

		cmd = sp->cmd;
		if (vha->vp_idx != 
				((scsi_qla_host_t *)shost_priv(cmd->device->host))->vp_idx)
			continue;

		/* Wait for all outstanding commands for this vport. */
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		qla2x00_eh_wait_on_command(cmd);
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	return status;
}

/*
 * qla2400_disable_vp
 *	Disable Vport and logout all fcports.
 *
 * Input:
 *	ha = adapter block pointer.
 *
* Returns:
*    0 : SUCCESS
*    -1 : FAILED
 *
 * Context:
 */
int
qla24xx_disable_vp(scsi_qla_host_t *vha)
{
	int ret = 0;
	struct list_head *hal, *tmp_ha;
	scsi_qla_host_t *search_ha = NULL;

	/* 
	 * Logout all targets. After the logout, no further IO
	 * requests are entertained.
	 */
	ret = qla24xx_control_vp(vha, VCE_COMMAND_DISABLE_VPS_LOGO_ALL);
	atomic_set(&vha->loop_state, LOOP_DOWN);
	atomic_set(&vha->loop_down_timer, LOOP_DOWN_TIME);

	/* Complete all outstanding commands on vha. */
	if (qla2x00_eh_wait_for_vp_pending_commands(vha))
		DEBUG2(printk(KERN_INFO
		    "%s(%ld): Could not complete all commands for vport : %d\n",
		    __func__, vha->host_no, vha->vp_idx));

	/* 
	 * Remove vha from hostlist. No further IOCTLs can be 
	 * queued on the corresponding vha. 
	 */
	if (test_bit(VP_DELETE_ACTIVE, &vha->dpc_flags)) {
		mutex_lock(&instance_lock);
		list_for_each_safe(hal, tmp_ha, &qla_hostlist) {
			search_ha = list_entry(hal, scsi_qla_host_t, hostlist);

			if (search_ha->instance == vha->instance) {
				list_del(hal);
				break;
			}
		}

		clear_bit(vha->instance, host_instance_map);
		num_hosts--;
		mutex_unlock(&instance_lock);
	}

	/* Indicate to FC transport & that rports are gone. */
	qla2x00_mark_vp_devices_dead(vha);
	atomic_set(&vha->vp_state, VP_FAILED);
	vha->flags.management_server_logged_in = 0;
	if (ret == QLA_SUCCESS) {
		if (vha->fc_vport)
			fc_vport_set_state(vha->fc_vport, FC_VPORT_DISABLED);
	} else {
		if (vha->fc_vport)
			fc_vport_set_state(vha->fc_vport, FC_VPORT_FAILED);
		return -1;
	}
	return 0;
}

int
qla24xx_enable_vp(scsi_qla_host_t *vha)
{
	int ret;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	/* Check if physical ha port is Up */
	if (atomic_read(&base_vha->loop_state) == LOOP_DOWN ||
		atomic_read(&base_vha->loop_state) == LOOP_DEAD ||
		!(ha->current_topology & ISP_CFG_F)) {
		vha->vp_err_state =  VP_ERR_PORTDWN;
		if (vha->fc_vport)
			fc_vport_set_state(vha->fc_vport, FC_VPORT_LINKDOWN);
		goto enable_failed;
	}

	/* Initialize the new vport unless it is a persistent port. */
	ret = qla24xx_modify_vp_config(vha);

	if (ret != QLA_SUCCESS) {
		if (vha->fc_vport)
			fc_vport_set_state(vha->fc_vport, FC_VPORT_FAILED);
		goto enable_failed;
	}

	DEBUG15(qla_printk(KERN_INFO, ha,
	    "Virtual port with id: %d - Enabled\n", vha->vp_idx));
	return 0;

enable_failed:
	DEBUG15(qla_printk(KERN_INFO, ha,
	    "Virtual port with id: %d - Disabled\n", vha->vp_idx));
	return 1;
}

static void
qla24xx_configure_vp(scsi_qla_host_t *vha)
{
	struct fc_vport *fc_vport;
	int ret;

	fc_vport = vha->fc_vport;

	DEBUG15(printk("scsi(%ld): %s: change request #3 for this host.\n",
	    vha->host_no, __func__));
	ret = qla2x00_send_change_request(vha, 0x3, vha->vp_idx);
	if (ret != QLA_SUCCESS) {
		DEBUG15(qla_printk(KERN_ERR, vha->hw, "Failed to enable receiving"
		    " of RSCN requests: 0x%x\n", ret));
		return;
	} else {
		/* Corresponds to SCR enabled */
		clear_bit(VP_SCR_NEEDED, &vha->vp_flags);
	}

	vha->flags.online = 1;
	if (qla24xx_configure_vhba(vha))
		return;

	atomic_set(&vha->vp_state, VP_ACTIVE);
	if (vha->fc_vport)
		fc_vport_set_state(fc_vport, FC_VPORT_ACTIVE);
}

void
qla2x00_alert_all_vps(struct rsp_que *rsp, uint16_t *mb)
{
	scsi_qla_host_t *vha, *tvha, *localvha = NULL;
	unsigned long   flags;
	struct qla_hw_data *ha = rsp->hw;

	spin_lock_irqsave(&ha->vport_lock, flags);
	list_for_each_entry_safe(vha, tvha, &ha->vp_list, list) {
		
		/* Skip over primary port */
		if (!vha->vp_idx)
			continue;

		/* Skip if vport delete is in progress. */
		if (test_bit(VP_DELETE_ACTIVE, &vha->dpc_flags))
			continue;

		/* Get vport reference. */
		qla2xxx_vha_get(vha);
		/* Get temporary vport reference. */
		if (&tvha->list != &ha->vp_list)
			qla2xxx_vha_get(tvha);
		spin_unlock_irqrestore(&ha->vport_lock, flags);

		switch (mb[0]) {
		case MBA_LIP_OCCURRED:
		case MBA_LOOP_UP:
		case MBA_LOOP_DOWN:
		case MBA_LIP_RESET:
		case MBA_POINT_TO_POINT:
		case MBA_CHG_IN_CONNECTION:
		case MBA_PORT_UPDATE:
		case MBA_RSCN_UPDATE:
			DEBUG15(printk("scsi(%ld)%s: Async_event for"
			    " VP[%d], mb = 0x%x, vha=%p\n",
			    vha->host_no, __func__,vha->vp_idx, *mb, vha));
			qla2x00_async_event(vha, rsp, mb);
			break;
		}

		spin_lock_irqsave(&ha->vport_lock, flags);
		/* Drop vport reference */
		qla2xxx_vha_put(vha);
		if (&tvha->list != &ha->vp_list) {
			localvha = tvha;
			/* Skip current entry as it is in the process of deletion. */
			if (test_bit(VP_DELETE_ACTIVE, &tvha->dpc_flags))
				tvha = list_entry(tvha->list.next, typeof(*tvha),
				list);
			/* Drop temporary vport reference */
			qla2xxx_vha_put(localvha);
		}
	}
	spin_unlock_irqrestore(&ha->vport_lock, flags);
}

int
qla2x00_vp_abort_isp(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	if(!vha->vp_idx)
		return 1;
	/*
	 * Physical port will do most of the abort and recovery work. We can
	 * just treat it as a loop down
	 */
	if (atomic_read(&vha->loop_state) != LOOP_DOWN) {
		atomic_set(&vha->loop_state, LOOP_DOWN);
		qla2x00_mark_all_devices_lost(vha, 0);
	} else {
		if (!atomic_read(&vha->loop_down_timer))
			atomic_set(&vha->loop_down_timer, LOOP_DOWN_TIME);
	}

	/* To exclusively reset vport, we need to log it out first.*/
	if (!test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags))
		qla24xx_control_vp(vha, VCE_COMMAND_DISABLE_VPS_LOGO_ALL);

	/* Host Statistics. */
	vha->hw->qla_stats.total_isp_aborts++;
	DEBUG15(printk("scsi(%ld): Scheduling enable of Vport %d...\n",
	    vha->host_no, vha->vp_idx));
	return qla24xx_enable_vp(vha);
}

int
qla2x00_do_dpc_vp(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	/* Vport's dpc is active. */
	vha->dpc_active = 1;

	/*
	 * Don't process anything on this vp if a deletion
	 * is in progress
	 */
	if (test_bit(VP_DELETE_ACTIVE, &vha->dpc_flags))
		return 0;

	if (test_and_clear_bit(VP_IDX_ACQUIRED, &vha->vp_flags)) {
		/* VP acquired. complete port configuration */
		if (atomic_read(&base_vha->loop_state) == LOOP_READY) {
			qla24xx_configure_vp(vha);
		} else {
			set_bit(VP_IDX_ACQUIRED, &vha->vp_flags);
			set_bit(VP_DPC_NEEDED, &base_vha->dpc_flags);
		}
		return 0;
	}

	if (test_and_clear_bit(RESET_MARKER_NEEDED, &vha->dpc_flags) &&
	    (!(test_and_set_bit(RESET_ACTIVE, &vha->dpc_flags)))) {
		clear_bit(RESET_ACTIVE, &vha->dpc_flags);
	}

	if (test_and_clear_bit(FCPORT_UPDATE_NEEDED, &vha->dpc_flags))
		qla2x00_update_fcports(vha);

	if ((test_and_clear_bit(RELOGIN_NEEDED, &vha->dpc_flags)) &&
		!test_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags) &&
		atomic_read(&vha->loop_state) != LOOP_DOWN) {

		DEBUG(printk("scsi(%ld): qla2x00_port_login()\n",
					vha->host_no));
		qla2x00_relogin(vha);

		DEBUG(printk("scsi(%ld): qla2x00_port_login - end\n",
					vha->host_no));
	}

	if (test_and_clear_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags)) {
		if (!(test_and_set_bit(LOOP_RESYNC_ACTIVE, &vha->dpc_flags))) {
			qla2x00_loop_resync(vha);
			clear_bit(LOOP_RESYNC_ACTIVE, &vha->dpc_flags);
		}
	}

	/* Vport's dpc done. */
	vha->dpc_active = 0;


	return 0;
}

void
qla2x00_do_dpc_all_vps(scsi_qla_host_t *vha)
{
	unsigned long   flags;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *vp, *tvp, *localvp = NULL;

	if (vha->vp_idx)
		return;
	if (list_empty(&ha->vp_list))
		return;

	clear_bit(VP_DPC_NEEDED, &vha->dpc_flags);

	spin_lock_irqsave(&ha->vport_lock, flags);
	list_for_each_entry_safe(vp, tvp, &ha->vp_list, list) {

 		/* Initialization not yet finished. Don't do anything yet. */
 		if (!vp->fc_vport && !vp->flags.init_done)
 			continue;

		/* Skip over primary port */
		if (!vp->vp_idx)
			continue;

		 /* Skip if vport delete is in progress. */
		if (test_bit(VP_DELETE_ACTIVE, &vp->dpc_flags))
			continue;

		/* Get vport reference. */
		qla2xxx_vha_get(vp); 
		/* Get temp vport reference. */
		if (&tvp->list != &ha->vp_list)
			qla2xxx_vha_get(tvp);
		spin_unlock_irqrestore(&ha->vport_lock, flags);

		qla2x00_do_dpc_vp(vp);

		spin_lock_irqsave(&ha->vport_lock, flags);
		/* Drop vport reference */
		qla2xxx_vha_put(vp);
		if (&tvp->list != &ha->vp_list) {
			localvp = tvp;
			/* Skip current vport entry as it is in the process of deletion. */
			if (test_bit(VP_DELETE_ACTIVE, &tvp->dpc_flags))
				tvp = list_entry(tvp->list.next, typeof(*tvp), list);
			/* Drop temp vport reference */
			qla2xxx_vha_put(localvp);
		}
	}
	spin_unlock_irqrestore(&ha->vport_lock, flags);
}

int
qla24xx_vport_create_req_sanity_check(struct fc_vport *fc_vport)
{
	scsi_qla_host_t *base_vha = shost_priv(fc_vport->shost);
	struct qla_hw_data *ha = base_vha->hw;
	scsi_qla_host_t *vha;
	uint8_t port_name[WWN_SIZE];

	if (fc_vport->roles != FC_PORT_ROLE_FCP_INITIATOR)
		return VPCERR_UNSUPPORTED;

	/* Check up the F/W and H/W support NPIV */
	if (!ha->flags.npiv_supported || base_vha->vp_idx)
		return VPCERR_UNSUPPORTED;

	/* Check up whether npiv supported switch presented */
	if (!(ha->switch_cap & FLOGI_MID_SUPPORT))
		return VPCERR_NO_FABRIC_SUPP;

	/* Check up unique WWPN */
	u64_to_wwn(fc_vport->port_name, port_name);
	if (!memcmp(port_name, base_vha->port_name, WWN_SIZE))
		return VPCERR_BAD_WWN;
	vha = qla24xx_find_vhost_by_name(ha, port_name);
	if (vha)
		return VPCERR_BAD_WWN;

	/* Check up max-npiv-supports */
	if (ha->num_vhosts > ha->max_npiv_vports) {
		DEBUG15(printk("scsi(%ld): num_vhosts %ud is bigger than "
		    "max_npv_vports %ud.\n", base_vha->host_no,
		    ha->num_vhosts, ha->max_npiv_vports));
		return VPCERR_UNSUPPORTED;
	}
	return 0;
}

scsi_qla_host_t *
qla24xx_create_vhost(struct fc_vport *fc_vport, uint8_t pre_boot)
{
	scsi_qla_host_t *base_vha = shost_priv(fc_vport->shost);
	struct qla_hw_data *ha = base_vha->hw;
	scsi_qla_host_t *vha;
	struct Scsi_Host *host;

	host = scsi_host_alloc(&qla24xx_driver_template,
	    sizeof(scsi_qla_host_t));
	if (!host) {
		printk(KERN_WARNING
		    "qla2xxx: scsi_host_alloc() failed for vport\n");
		return(NULL);
	}

	vha = shost_priv(host);
	memcpy(vha, base_vha, sizeof (scsi_qla_host_t));

	/* New host info */
	u64_to_wwn(fc_vport->node_name, vha->node_name);
	u64_to_wwn(fc_vport->port_name, vha->port_name);

	vha->host = host;
	vha->host_no = host->host_no;
	vha->hw = ha;
	vha->device_flags = 0;
	vha->fc_vport = NULL;

	if(!pre_boot) {
		fc_vport->dd_data = vha;
		vha->fc_vport = fc_vport;
	}

	INIT_LIST_HEAD(&vha->list);
	INIT_LIST_HEAD(&vha->vp_fcports);
	INIT_LIST_HEAD(&vha->work_list);
	sprintf(vha->host_str, "%s_%ld", QLA2XXX_DRIVER_NAME, vha->host_no);

	vha->vp_idx = qla24xx_allocate_vp_id(vha);
	if (vha->vp_idx > ha->max_npiv_vports) {
		DEBUG15(printk("scsi(%ld): Couldn't allocate vp_id.\n",
			vha->host_no));
		goto create_vhost_failed;
	}
	vha->mgmt_svr_loop_id = 10 + vha->vp_idx;

	kref_init(&vha->kref);

	vha->dpc_flags = 0L;
	vha->dpc_active = 0;
	set_bit(REGISTER_FDMI_NEEDED, &vha->dpc_flags);
	set_bit(REGISTER_FC4_NEEDED, &vha->dpc_flags);

	/* Need to allocate memory for ioctls  */
	vha->ioctl = NULL;
	vha->ioctl_mem = NULL;
	vha->ioctl_mem_size = 0;

 	if(!pre_boot) {
		if (qla2x00_alloc_ioctl_mem(vha)) {
		       	DEBUG15(printk("scsi(%ld): Couldn't allocate vp ioctl memory.\n",
                   		    vha->host_no));
			goto create_vhost_failed;
		}
	}

	set_bit(VP_SCR_NEEDED, &vha->vp_flags);
	atomic_set(&vha->loop_state, LOOP_DOWN);
	atomic_set(&vha->loop_down_timer, LOOP_DOWN_TIME);

	qla2x00_start_timer(vha, qla2x00_timer, WATCH_INTERVAL);

	vha->req = base_vha->req;
	host->can_queue = base_vha->req->length + 128;
	host->this_id = 255;
	host->cmd_per_lun = 3;
	host->max_cmd_len = MAX_CMDSZ;
	host->max_channel = MAX_BUSES - 1;
	host->max_lun = MAX_LUNS;
	host->max_id = MAX_TARGETS;
	host->transportt = qla2xxx_transport_vport_template;

	/* 
	 * Insert new entry into the list of adapters.
	 * IOCTLs can start coming down after this operation.
	 */
	mutex_lock(&instance_lock);
	if(!pre_boot) {	
		list_add_tail(&vha->hostlist, &qla_hostlist);
		num_hosts++;
	}

	vha->instance = find_first_zero_bit(host_instance_map, MAX_HBAS);
	if (vha->instance == MAX_HBAS) {
		DEBUG9_10(printk("Host instance exhausted\n"));
	}
	set_bit(vha->instance, host_instance_map);
	mutex_unlock(&instance_lock);
	host->unique_id = vha->instance;

	DEBUG15(printk("DEBUG: detect vport hba %ld at address = %p\n",
	    vha->host_no, vha));

	if(pre_boot) {
		atomic_set(&vha->vp_state, VP_FAILED);

		/* ready to create vport */
		qla_printk(KERN_INFO, ha, "pre-boot VP entry id %d assigned.\n", vha->vp_idx);

		/* initialized vport states */
		atomic_set(&vha->loop_state, LOOP_DOWN);
		vha->vp_err_state=  VP_ERR_PORTDWN;
		vha->vp_prev_err_state=  VP_ERR_UNKWN;
	} else
		vha->flags.init_done = 1;

	return vha;

create_vhost_failed:
	return NULL;
}

static void
qla25xx_free_req_que(struct scsi_qla_host *vha, struct req_que *req)
{
	struct qla_hw_data *ha = vha->hw;
	uint16_t que_id = req->id;
	unsigned long flags;

	dma_free_coherent(&ha->pdev->dev, (req->length + 1) *
		sizeof(request_t), req->ring, req->dma);
	req->ring = NULL;
	req->dma = 0;
	if (que_id) {
		ha->req_q_map[que_id] = NULL;
		spin_lock_irqsave(&ha->vport_lock, flags);
		clear_bit(que_id, ha->req_qid_map);
		spin_unlock_irqrestore(&ha->vport_lock, flags);
	}
	kfree(req);
	req = NULL;
}

static void
qla25xx_free_rsp_que(struct scsi_qla_host *vha, struct rsp_que *rsp)
{
	struct qla_hw_data *ha = vha->hw;
	uint16_t que_id = rsp->id;
	unsigned long flags;

	if (rsp->msix && rsp->msix->have_irq) {
		free_irq(rsp->msix->vector, rsp);
		rsp->msix->have_irq = 0;
		rsp->msix->rsp = NULL;
	}
	dma_free_coherent(&ha->pdev->dev, (rsp->length + 1) *
		sizeof(response_t), rsp->ring, rsp->dma);
	rsp->ring = NULL;
	rsp->dma = 0;
	if (que_id) {
		ha->rsp_q_map[que_id] = NULL;
		spin_lock_irqsave(&ha->vport_lock, flags);
		clear_bit(que_id, ha->rsp_qid_map);
		spin_unlock_irqrestore(&ha->vport_lock, flags);
	}
	kfree(rsp);
	rsp = NULL;
}

int
qla25xx_delete_req_que(struct scsi_qla_host *vha, struct req_que *req)
{
	int ret = -1;
	struct qla_hw_data *ha = vha->hw;

	if (req) {
		req->options |= BIT_0;
		ret = qla25xx_init_req_que(vha, req);
	}
	if (ret != QLA_SUCCESS)
		ha->isp_ops->fw_dump(vha, 0);
	else
		qla25xx_free_req_que(vha, req);

	return ret;
}

int
qla25xx_delete_rsp_que(struct scsi_qla_host *vha, struct rsp_que *rsp)
{
	int ret = -1;
	struct qla_hw_data *ha = vha->hw;

	if (rsp) {
		rsp->options |= BIT_0;
		ret = qla25xx_init_rsp_que(vha, rsp);
	}
	if (ret != QLA_SUCCESS)
		ha->isp_ops->fw_dump(vha, 0);
	else
		qla25xx_free_rsp_que(vha, rsp);

	return ret;
}

int qla25xx_update_req_que(struct scsi_qla_host *vha, uint8_t que, uint8_t qos)
{
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = ha->req_q_map[que];

	req->options |= BIT_3;
	req->qos = qos;
	ret = qla25xx_init_req_que(vha, req);
	if (ret != QLA_SUCCESS)
		DEBUG2_17(printk(KERN_WARNING "%s failed\n", __func__));
	/* restore options bit */
	req->options = (((req->options >> 3) ^ 1) << 3) |
			(req->options & (BIT_3 - 1));
	return ret;
}


/* Delete all queues for a given vhost */
int
qla25xx_delete_queues(struct scsi_qla_host *vha)
{
	int cnt, ret = 0;
	struct req_que *req = NULL;
	struct rsp_que *rsp = NULL;
	struct qla_hw_data *ha = vha->hw;

 	/* Delete request queues */
 	for (cnt = 1; cnt < ha->max_req_queues; cnt++) {
 		req = ha->req_q_map[cnt];
		if (req) {
			ret = qla25xx_delete_req_que(vha, req);
			if (ret != QLA_SUCCESS) {
				qla_printk(KERN_WARNING, ha,
 				"Couldn't delete req que %d\n",
 				req->id);
				return ret;
			}
		}
 	}
 
 	/* Delete response queues */
 	for (cnt = 1; cnt < ha->max_rsp_queues; cnt++) {
 		rsp = ha->rsp_q_map[cnt];
 		if (rsp) {
 			ret = qla25xx_delete_rsp_que(vha, rsp);
 			if (ret != QLA_SUCCESS) {
 				qla_printk(KERN_WARNING, ha,
 				"Couldn't delete rsp que %d\n",
 				rsp->id);
 				return ret;
  			}
  		}
	}
	return ret;
}

int
qla25xx_create_req_que(struct qla_hw_data *ha, uint16_t options,
 	uint8_t vp_idx, uint16_t rid, int rsp_que, uint8_t qos)
{
	int ret = 0;
	struct req_que *req = NULL;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);
	uint16_t que_id = 0;
	unsigned long flags;
 	device_reg_t __iomem *reg;
 	uint32_t cnt;

	req = kzalloc(sizeof(struct req_que), GFP_KERNEL);
	if (req == NULL) {
		qla_printk(KERN_WARNING, ha, "could not allocate memory"
			"for request que\n");
		goto que_failed;
	}

	req->length = REQUEST_ENTRY_CNT_24XX;
	req->ring = dma_alloc_coherent(&ha->pdev->dev,
			(req->length + 1) * sizeof(request_t),
			&req->dma, GFP_KERNEL);
	if (req->ring == NULL) {
		qla_printk(KERN_WARNING, ha,
		"Memory Allocation failed - request_ring\n");
		goto que_failed;
	}

	spin_lock_irqsave(&ha->vport_lock, flags);
 	que_id = find_first_zero_bit(ha->req_qid_map, ha->max_req_queues);
 	if (que_id >= ha->max_req_queues) {
		spin_unlock_irqrestore(&ha->vport_lock, flags);
		qla_printk(KERN_INFO, ha, "No resources to create "
			 "additional request queue\n");
		goto que_failed;
	}
	set_bit(que_id, ha->req_qid_map);
	ha->req_q_map[que_id] = req;
	req->rid = rid;
	req->vp_idx = vp_idx;
	req->qos = qos;

	if (rsp_que < 0)
		req->rsp = NULL;
	else
		req->rsp = ha->rsp_q_map[rsp_que];
	/* Use alternate PCI bus number */
	if (MSB(req->rid))
		options |= BIT_4;
	/* Use alternate PCI devfn */
	if (LSB(req->rid))
		options |= BIT_5;

	req->options = options;

	for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++)
		req->outstanding_cmds[cnt] = NULL;

	req->current_outstanding_cmd = 1;
	req->ring_ptr = req->ring;
	req->ring_index = 0;
	req->cnt = req->length;
	req->id = que_id;
 	reg = ISP_QUE_REG(ha, que_id);
	req->max_q_depth = ha->req_q_map[0]->max_q_depth;
	spin_unlock_irqrestore(&ha->vport_lock, flags);

 	ret = qla25xx_init_req_que(base_vha, req);
	if (ret != QLA_SUCCESS) {
		qla_printk(KERN_WARNING, ha, "%s failed\n", __func__);
		spin_lock_irqsave(&ha->vport_lock, flags);
		clear_bit(que_id, ha->req_qid_map);
		spin_unlock_irqrestore(&ha->vport_lock, flags);
		ha->isp_ops->fw_dump(base_vha, 0);
		goto que_failed;
	}

	return req->id;

que_failed:
	qla25xx_free_req_que(base_vha, req);
	return 0;
}


/* create response queue */
int
qla25xx_create_rsp_que(struct qla_hw_data *ha, uint16_t options,
 	uint8_t vp_idx, uint16_t rid, int req)
{
	int ret = 0;
	struct rsp_que *rsp = NULL;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);
	uint16_t que_id = 0;
	unsigned long flags;
	device_reg_t __iomem *reg;

	rsp = kzalloc(sizeof(struct rsp_que), GFP_KERNEL);
	if (rsp == NULL) {
		qla_printk(KERN_WARNING, ha, "could not allocate memory for"
				" response que\n");
		goto que_failed;
	}

	rsp->length = RESPONSE_ENTRY_CNT_MQ;
	rsp->ring = dma_alloc_coherent(&ha->pdev->dev,
			(rsp->length + 1) * sizeof(response_t),
			&rsp->dma, GFP_KERNEL);
	if (rsp->ring == NULL) {
		qla_printk(KERN_WARNING, ha,
		"Memory Allocation failed - response_ring\n");
		goto que_failed;
	}

	spin_lock_irqsave(&ha->vport_lock, flags);
 	que_id = find_first_zero_bit(ha->rsp_qid_map, ha->max_rsp_queues);
 	if (que_id >= ha->max_rsp_queues) {
		spin_unlock_irqrestore(&ha->vport_lock, flags);
		qla_printk(KERN_INFO, ha, "No resources to create "
			 "additional response queue\n");
		goto que_failed;
	}
	set_bit(que_id, ha->rsp_qid_map);

	if (ha->flags.msix_enabled)
		rsp->msix = &ha->msix_entries[que_id + 1];
	else
		qla_printk(KERN_WARNING, ha, "msix not enabled\n");

	ha->rsp_q_map[que_id] = rsp;
	rsp->rid = rid;
	rsp->vp_idx = vp_idx;
	rsp->hw = ha;
	/* Use alternate PCI bus number */
	if (MSB(rsp->rid))
		options |= BIT_4;
	/* Use alternate PCI devfn */
	if (LSB(rsp->rid))
		options |= BIT_5;
	/* Enable MSIX handshake mode on for uncapable adapters */
	if (!IS_MSIX_NACK_CAPABLE(ha))
		options |= BIT_6;

	rsp->options = options;
	rsp->id = que_id;
	reg = ISP_QUE_REG(ha, que_id);
	rsp->rsp_q_in = &reg->isp25mq.rsp_q_in;
	rsp->rsp_q_out = &reg->isp25mq.rsp_q_out;
	spin_unlock_irqrestore(&ha->vport_lock, flags);

	ret = qla25xx_request_irq(rsp);
	if (ret)
		goto que_failed;

	ret = qla25xx_init_rsp_que(base_vha, rsp);
	if (ret != QLA_SUCCESS) {
		qla_printk(KERN_WARNING, ha, "%s failed\n", __func__);
		spin_lock_irqsave(&ha->vport_lock, flags);
		clear_bit(que_id, ha->rsp_qid_map);
		spin_unlock_irqrestore(&ha->vport_lock, flags);
		ha->isp_ops->fw_dump(base_vha, 0);
		goto que_failed;
	}
	if (req >= 0)
		rsp->req = ha->req_q_map[req];
	else
		rsp->req = NULL;

	qla2x00_init_response_q_entries(rsp);

	return rsp->id;

que_failed:
	qla25xx_free_rsp_que(base_vha, rsp);
	return 0;
}

int
qla25xx_create_queues(struct scsi_qla_host *vha, uint8_t qos)
{
	uint16_t options = 0;
	uint8_t ret = 0;
	struct qla_hw_data *ha = vha->hw;
 	struct rsp_que *rsp;

	options |= BIT_1;
 	ret = qla25xx_create_rsp_que(ha, options, vha->vp_idx, 0, -1);
	if (!ret) {
		qla_printk(KERN_WARNING, ha, "Rsp Que create failed\n");
		return ret;
	}
	else
		qla_printk(KERN_INFO, ha, "Rsp Que:%d created.\n", ret);
 	rsp = ha->rsp_q_map[ret];

	options = 0;
	if (qos & BIT_7)
		options |= BIT_8;
	ret = qla25xx_create_req_que(ha, options, vha->vp_idx, 0, ret,
					qos & ~BIT_7);
	if (ret) {
 		vha->req = ha->req_q_map[ret];
		qla_printk(KERN_INFO, ha, "Req Que:%d with Qos:%x created.\n",
					ret, qos&~BIT_7);
	} else
		qla_printk(KERN_WARNING, ha, "Req Que create failed\n");
 	rsp->req = ha->req_q_map[ret];

	return ret;
}

static void
__qla2xxx_vha_release(struct kref *kref)
{
	struct scsi_qla_host *vha =
	    container_of(kref, struct scsi_qla_host, kref);
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	if (!vha->vp_idx)
		return;

	qla24xx_deallocate_vp_id(vha);
	set_bit(VPORT_CLEANUP_NEEDED, &base_vha->dpc_flags);
	qla2xxx_wake_dpc(vha);

}

/*
 * qla2xxx_vha_put(): Drop a reference from the vport.
 * If we are the last caller, free vha.
 * @vha: Vport context.
 *
 * Note: The caller of this function needs to ensure that
 * the vport_lock of the corresponding physical port is
 * not held while making this call.
 */
void
qla2xxx_vha_put(struct scsi_qla_host *vha)
{
	if(vha->vp_idx)
		kref_put(&vha->kref, __qla2xxx_vha_release);
}

void
qla2xxx_vha_get(struct scsi_qla_host *vha)
{
	if(vha->vp_idx)
		kref_get(&vha->kref);
}

/*
 * qla24xx_getinfo_vport() -  Query information about virtual fabric port
 * @ha: HA context
 * @vp_info: pointer to buffer of information about virtual port.
 * instance : instance of virtual port from 0 to MAX_MULTI_ID_NPORTS.
 * For Sansurfer Use only.
 *             
 * Returns error code.
 */
uint32_t
qla24xx_getinfo_vport(scsi_qla_host_t *vha, fc_vport_info_t *vp_info , uint32_t  instance)
{
	scsi_qla_host_t *vp;
	struct qla_hw_data *ha = vha->hw; 
	int     i = 0;
	unsigned long flags;

	if (instance >= ha->max_npiv_vports) {
               DEBUG(printk(KERN_INFO "instance number out of range...\n"));
               return VP_RET_CODE_FATAL;
	}

	memset(vp_info, 0, sizeof (fc_vport_info_t));
       /* Always return these numbers */
	vp_info->free_vpids = ha->max_npiv_vports - ha->num_vhosts - 1;
	vp_info->used_vpids = ha->num_vhosts; 
	spin_lock_irqsave(&ha->vport_lock, flags);
	list_for_each_entry(vp, &ha->vp_list, list) {
		if (vp->vp_idx) {
			if (i==instance) {
				vp_info->vp_id = vp->vp_idx; 
				vp_info->vp_state = atomic_read(&vp->vp_state);
				memcpy(vp_info->node_name , vp->node_name, WWN_SIZE);
				memcpy(vp_info->port_name, vp->port_name, WWN_SIZE);
				spin_unlock_irqrestore(&ha->vport_lock, flags);
				return VP_RET_CODE_OK;
			}
			i++;
		}
	}
	spin_unlock_irqrestore(&ha->vport_lock, flags);
	DEBUG(printk(KERN_INFO "instance number not found..\n"));
	return VP_RET_CODE_FATAL;
}

int
qla2xxx_add_boot_host(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw; 
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);

	if (scsi_add_host(vha->host, &ha->pdev->dev)) {
		DEBUG15(printk("scsi(%ld): scsi_add_host failure for VP[%d].\n",
			vha->host_no, vha->vp_idx));
		return 1;
	}

	/* initialize attributes */
	fc_host_node_name(vha->host) = wwn_to_u64(vha->node_name);
	fc_host_port_name(vha->host) = wwn_to_u64(vha->port_name);
	fc_host_supported_classes(vha->host) =
		fc_host_supported_classes(base_vha->host);
	fc_host_supported_speeds(vha->host) =
		fc_host_supported_speeds(base_vha->host);

	return 0;
}

/* 
 * qla2xxx_initialize_boothost() - Initialize pre-boot host.
 * @pha: Physical HA context.
 *  
 *  Instantiate pre-boot hosts and populate the MID init control
 *  block with them.
 */
void
qla2xxx_initialize_boothost(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	unsigned long flags;
	scsi_qla_host_t *vp;
	struct mid_init_cb_24xx *mid_init_cb =
	    (struct mid_init_cb_24xx *) ha->init_cb;

	if (!IS_FWI2_CAPABLE(ha))
		return;

	qla2xxx_flash_npiv_conf(vha, 0);

	/* Initialize control block for boot vports. */
	spin_lock_irqsave(&ha->vport_lock, flags);
	list_for_each_entry(vp, &ha->vp_list, list) {
		if (vp->vp_idx) {
			mid_init_cb->entries[vp->vp_idx - 1].options = 
		    	    cpu_to_le16(BIT_3|BIT_4|BIT_5);
			memcpy(mid_init_cb->entries[vp->vp_idx - 1].port_name,
		    	    vp->port_name, WWN_SIZE);
			memcpy(mid_init_cb->entries[vp->vp_idx - 1].node_name,
		    	    vp->node_name, WWN_SIZE);
		}
	}
	spin_unlock_irqrestore(&ha->vport_lock, flags);
	return;
}	

/* 
 * qla2xxx_delete_boothost() - Delete pre-boot host.
 * @pha: Virtual HA context.
 *  
 */
void
qla2xxx_delete_boothost(scsi_qla_host_t *vha, uint8_t context)
{
 	struct qla_hw_data *ha = vha->hw;
	unsigned int cpu_flags;
	/* Disable vport operations. */
	qla24xx_disable_vp(vha);
	/*
	 * This will ensure that all fc transport
	 * cleanup is completed and the work queues 
	 * are flushed. This function blocks till all 
	 * the work in the work queue is completed.
	 */ 
	fc_remove_host(vha->host);

	if(context)
		scsi_remove_host(vha->host);
 
	/* Delete the timer on the vport. */
	if (vha->timer_active) {
		qla2x00_vp_stop_timer(vha);
		DEBUG15(printk ("scsi(%ld): timer for the vport[%d] = %p "
    	    	    "has stopped\n",
    	    	    vha->host_no, vha->vp_idx, vha));
	}

	/* Note: At this point, the DPC is stopped. Hence we do the
	 * cleanup right here. */
	spin_lock_irqsave(&ha->vport_lock, cpu_flags);
	qla2xxx_vha_put(vha);
	spin_unlock_irqrestore(&ha->vport_lock, cpu_flags);
 
	return;
 }
