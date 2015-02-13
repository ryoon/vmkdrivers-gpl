/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2005 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

int qla24xx_vport_disable(struct fc_vport *, bool);

/* Scsi_Host attributes. */

static ssize_t
qla2x00_drvr_version_show(struct class_device *cdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", qla2x00_version_str);
}

static ssize_t
qla2x00_fw_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	char fw_str[QLA_FW_VERSION_STR_SIZE];

	return snprintf(buf, PAGE_SIZE, "%s\n",
	    ha->isp_ops->fw_version_str(vha, fw_str));
}

static ssize_t
qla2x00_serial_num_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	uint32_t sn;

	if (IS_FWI2_CAPABLE(ha)) {
		qla2xxx_get_vpd_field(vha, "SN", buf, PAGE_SIZE);
		return snprintf(buf, PAGE_SIZE, "%s\n", buf);
	}

	sn = ((ha->serial0 & 0x1f) << 16) | (ha->serial2 << 8) | ha->serial1;
	return snprintf(buf, PAGE_SIZE, "%c%05d\n", 'A' + sn / 100000,
	    sn % 100000);
}

static ssize_t
qla2x00_isp_name_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	return snprintf(buf, PAGE_SIZE, "ISP%04X\n", ha->pdev->device);
}

static ssize_t
qla2x00_isp_id_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	return snprintf(buf, PAGE_SIZE, "%04x %04x %04x %04x\n",
	    ha->product_id[0], ha->product_id[1], ha->product_id[2],
	    ha->product_id[3]);
}

static ssize_t
qla2x00_model_name_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	return snprintf(buf, PAGE_SIZE, "%s\n", ha->model_number);
}

static ssize_t
qla2x00_model_desc_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	return snprintf(buf, PAGE_SIZE, "%s\n",
	    ha->model_desc ? ha->model_desc: "");
}

static ssize_t
qla2x00_pci_info_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	char pci_info[30];

	return snprintf(buf, PAGE_SIZE, "%s\n",
	    ha->isp_ops->pci_info_str(vha, pci_info));
}

static ssize_t
qla2x00_state_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	int len = 0;

	if (atomic_read(&vha->loop_state) == LOOP_DOWN ||
	    atomic_read(&vha->loop_state) == LOOP_DEAD)
		len = snprintf(buf, PAGE_SIZE, "Link Down\n");
	else if (atomic_read(&vha->loop_state) != LOOP_READY ||
	    test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags) ||
	    test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags))
		len = snprintf(buf, PAGE_SIZE, "Unknown Link State\n");
	else {
		len = snprintf(buf, PAGE_SIZE, "Link Up - ");

		switch (ha->current_topology) {
		case ISP_CFG_NL:
			len += snprintf(buf + len, PAGE_SIZE-len, "Loop\n");
			break;
		case ISP_CFG_FL:
			len += snprintf(buf + len, PAGE_SIZE-len, "FL_Port\n");
			break;
		case ISP_CFG_N:
			len += snprintf(buf + len, PAGE_SIZE-len,
			    "N_Port to N_Port\n");
			break;
		case ISP_CFG_F:
			len += snprintf(buf + len, PAGE_SIZE-len, "F_Port\n");
			break;
		default:
			len += snprintf(buf + len, PAGE_SIZE-len, "Loop\n");
			break;
		}
	}
	return len;
}

static ssize_t
qla2x00_zio_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	int len = 0;

	switch (ha->zio_mode) {
	case QLA_ZIO_MODE_6:
		len += snprintf(buf + len, PAGE_SIZE-len, "Mode 6\n");
		break;
	case QLA_ZIO_DISABLED:
		len += snprintf(buf + len, PAGE_SIZE-len, "Disabled\n");
		break;
	}
	return len;
}

static ssize_t
qla2x00_zio_store(struct class_device *cdev, const char *buf, size_t count)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	int val = 0;
	uint16_t zio_mode;

	if (!IS_ZIO_SUPPORTED(ha))
		return -ENOTSUPP;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val)
		zio_mode = QLA_ZIO_MODE_6;
	else
		zio_mode = QLA_ZIO_DISABLED;

	/* Update per-hba values and queue a reset. */
	if (zio_mode != QLA_ZIO_DISABLED || ha->zio_mode != QLA_ZIO_DISABLED) {
		ha->zio_mode = zio_mode;
		set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
	}
	return strlen(buf);
}

static ssize_t
qla2x00_zio_timer_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));

	return snprintf(buf, PAGE_SIZE, "%d us\n", vha->hw->zio_timer * 100);
}

static ssize_t
qla2x00_zio_timer_store(struct class_device *cdev, const char *buf,
    size_t count)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	int val = 0;
	uint16_t zio_timer;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	if (val > 25500 || val < 100)
		return -ERANGE;

	zio_timer = (uint16_t)(val / 100);
	vha->hw->zio_timer = zio_timer;

	return strlen(buf);
}

static ssize_t
qla2x00_beacon_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	int len = 0;

	if (vha->hw->beacon_blink_led)
		len += snprintf(buf + len, PAGE_SIZE-len, "Enabled\n");
	else
		len += snprintf(buf + len, PAGE_SIZE-len, "Disabled\n");
	return len;
}

static ssize_t
qla2x00_beacon_store(struct class_device *cdev, const char *buf,
    size_t count)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;
	int val = 0;
	int rval;

	if (IS_QLA2100(ha) || IS_QLA2200(ha))
		return -EPERM;

	if (test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) {
		qla_printk(KERN_WARNING, ha,
		    "Abort ISP active -- ignoring beacon request.\n");
		return -EBUSY;
	}

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val)
		rval = ha->isp_ops->beacon_on(vha);
	else
		rval = ha->isp_ops->beacon_off(vha);

	if (rval != QLA_SUCCESS)
		count = 0;

	return count;
}

static ssize_t
qla2x00_optrom_bios_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;

	return snprintf(buf, PAGE_SIZE, "%d.%02d\n", ha->bios_revision[1],
	    ha->bios_revision[0]);
}

static ssize_t
qla2x00_optrom_efi_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;

	return snprintf(buf, PAGE_SIZE, "%d.%02d\n", ha->efi_revision[1],
	    ha->efi_revision[0]);
}

static ssize_t
qla2x00_optrom_fcode_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;

	return snprintf(buf, PAGE_SIZE, "%d.%02d\n", ha->fcode_revision[1],
	    ha->fcode_revision[0]);
}

static ssize_t
qla2x00_optrom_fw_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;

	return snprintf(buf, PAGE_SIZE, "%d.%02d.%02d %d\n",
	    ha->fw_revision[0], ha->fw_revision[1], ha->fw_revision[2],
	    ha->fw_revision[3]);
}

static ssize_t
qla2x00_mpi_version_show(struct class_device *cdev, char *buf)
{
	scsi_qla_host_t *vha = shost_priv(class_to_shost(cdev));
	struct qla_hw_data *ha = vha->hw;

	if (!IS_QLA81XX(ha))
		return snprintf(buf, PAGE_SIZE, "\n");

	return snprintf(buf, PAGE_SIZE, "%02x.%02x.%02x (%x)\n",
	    ha->mpi_version[0], ha->mpi_version[1], ha->mpi_version[2],
	    ha->mpi_capabilities);
}


static CLASS_DEVICE_ATTR(driver_version, S_IRUGO, qla2x00_drvr_version_show,
	NULL);
static CLASS_DEVICE_ATTR(fw_version, S_IRUGO, qla2x00_fw_version_show, NULL);
static CLASS_DEVICE_ATTR(serial_num, S_IRUGO, qla2x00_serial_num_show, NULL);
static CLASS_DEVICE_ATTR(isp_name, S_IRUGO, qla2x00_isp_name_show, NULL);
static CLASS_DEVICE_ATTR(isp_id, S_IRUGO, qla2x00_isp_id_show, NULL);
static CLASS_DEVICE_ATTR(model_name, S_IRUGO, qla2x00_model_name_show, NULL);
static CLASS_DEVICE_ATTR(model_desc, S_IRUGO, qla2x00_model_desc_show, NULL);
static CLASS_DEVICE_ATTR(pci_info, S_IRUGO, qla2x00_pci_info_show, NULL);
static CLASS_DEVICE_ATTR(state, S_IRUGO, qla2x00_state_show, NULL);
static CLASS_DEVICE_ATTR(zio, S_IRUGO | S_IWUSR, qla2x00_zio_show,
    qla2x00_zio_store);
static CLASS_DEVICE_ATTR(zio_timer, S_IRUGO | S_IWUSR, qla2x00_zio_timer_show,
    qla2x00_zio_timer_store);
static CLASS_DEVICE_ATTR(beacon, S_IRUGO | S_IWUSR, qla2x00_beacon_show,
    qla2x00_beacon_store);
static CLASS_DEVICE_ATTR(optrom_bios_version, S_IRUGO,
    qla2x00_optrom_bios_version_show, NULL);
static CLASS_DEVICE_ATTR(optrom_efi_version, S_IRUGO,
    qla2x00_optrom_efi_version_show, NULL);
static CLASS_DEVICE_ATTR(optrom_fcode_version, S_IRUGO,
    qla2x00_optrom_fcode_version_show, NULL);
static CLASS_DEVICE_ATTR(optrom_fw_version, S_IRUGO,
    qla2x00_optrom_fw_version_show, NULL);
static CLASS_DEVICE_ATTR(mpi_version, S_IRUGO, qla2x00_mpi_version_show, NULL);

struct class_device_attribute *qla2x00_host_attrs[] = {
	&class_device_attr_driver_version,
	&class_device_attr_fw_version,
	&class_device_attr_serial_num,
	&class_device_attr_isp_name,
	&class_device_attr_isp_id,
	&class_device_attr_model_name,
	&class_device_attr_model_desc,
	&class_device_attr_pci_info,
	&class_device_attr_state,
	&class_device_attr_zio,
	&class_device_attr_zio_timer,
	&class_device_attr_beacon,
	&class_device_attr_optrom_bios_version,
	&class_device_attr_optrom_efi_version,
	&class_device_attr_optrom_fcode_version,
	&class_device_attr_optrom_fw_version,
	&class_device_attr_mpi_version,
	NULL,
};

/* Host attributes. */

static void
qla2x00_get_host_port_id(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	fc_host_port_id(shost) = vha->d_id.b.domain << 16 |
	    vha->d_id.b.area << 8 | vha->d_id.b.al_pa;
}

static void
qla2x00_get_host_speed(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);
	struct qla_hw_data *ha = vha->hw;
	u32 speed = FC_PORTSPEED_UNKNOWN;

	switch (ha->link_data_rate) {
	case PORT_SPEED_1GB:
		speed = FC_PORTSPEED_1GBIT;
		break;
	case PORT_SPEED_2GB:
		speed = FC_PORTSPEED_2GBIT;
		break;
	case PORT_SPEED_4GB:
		speed = FC_PORTSPEED_4GBIT;
		break;
	case PORT_SPEED_8GB:
		speed = FC_PORTSPEED_8GBIT;
		break;
	case PORT_SPEED_10GB:
		speed = FC_PORTSPEED_10GBIT;
		break;
	}
	fc_host_speed(shost) = speed;
}

static void
qla2x00_get_host_port_type(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);
	struct qla_hw_data *ha = vha->hw;
	uint32_t port_type = FC_PORTTYPE_UNKNOWN;

	if (vha->vp_idx) {
		fc_host_port_type(shost) = FC_PORTTYPE_NPIV;
		return;
	}
	
	switch (ha->current_topology) {
	case ISP_CFG_NL:
		port_type = FC_PORTTYPE_LPORT;
		break;
	case ISP_CFG_FL:
		port_type = FC_PORTTYPE_NLPORT;
		break;
	case ISP_CFG_N:
		port_type = FC_PORTTYPE_PTP;
		break;
	case ISP_CFG_F:
		port_type = FC_PORTTYPE_NPORT;
		break;
	}
	fc_host_port_type(shost) = port_type;
}

static void
qla2x00_get_starget_node_name(struct scsi_target *starget)
{
	struct Scsi_Host *host = dev_to_shost(starget->dev.parent);
	scsi_qla_host_t *vha = shost_priv(host);
	fc_port_t *fcport;
	u64 node_name = 0;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (starget->id == fcport->os_target_id) {
			node_name = wwn_to_u64(fcport->node_name);
			break;
		}
	}

	fc_starget_node_name(starget) = node_name;
}

static void
qla2x00_get_starget_port_name(struct scsi_target *starget)
{
	struct Scsi_Host *host = dev_to_shost(starget->dev.parent);
	scsi_qla_host_t *vha = shost_priv(host);
	fc_port_t *fcport;
	u64 port_name = 0;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (starget->id == fcport->os_target_id) {
			port_name = wwn_to_u64(fcport->port_name);
			break;
		}
	}

	fc_starget_port_name(starget) = port_name;
}

static void
qla2x00_get_starget_port_id(struct scsi_target *starget)
{
	struct Scsi_Host *host = dev_to_shost(starget->dev.parent);
	scsi_qla_host_t *vha = shost_priv(host);
	fc_port_t *fcport;
	uint32_t port_id = ~0U;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (starget->id == fcport->os_target_id) {
			port_id = fcport->d_id.b.domain << 16 |
			    fcport->d_id.b.area << 8 | fcport->d_id.b.al_pa;
			break;
		}
	}

	fc_starget_port_id(starget) = port_id;
}

static void
qla2x00_get_rport_loss_tmo(struct fc_rport *rport)
{
	struct Scsi_Host *host = rport_to_shost(rport);
	scsi_qla_host_t *vha = shost_priv(host);

	rport->dev_loss_tmo = vha->hw->port_down_retry_count + 5;
}

static void
qla2x00_set_rport_loss_tmo(struct fc_rport *rport, uint32_t timeout)
{
	struct Scsi_Host *host = rport_to_shost(rport);
	scsi_qla_host_t *vha = shost_priv(host);
	struct qla_hw_data *ha = vha->hw;

	if (timeout)
		ha->port_down_retry_count = timeout;
	else
		ha->port_down_retry_count = 1;

	rport->dev_loss_tmo = ha->port_down_retry_count + 5;
}

static void
qla2x00_dev_loss_tmo_callbk(struct fc_rport *rport)
{
	struct Scsi_Host *host = rport_to_shost(rport);
	fc_port_t *fcport = *(fc_port_t **)rport->dd_data;

	/*
	 * Don't process anything on this vp if a deletion
	 * is in progress
	 */
	if (!fcport) {
		DEBUG2(printk("dev_loss_tmo_callbk: null fcport %x\n", rport->port_id));
		return;
	}

	if (!test_bit(VP_DELETE_ACTIVE, &fcport->vha->dpc_flags))
		qla2x00_abort_fcport_cmds(fcport);

	/*
	 * Transport has effectively 'deleted' the rport, clear
	 * all local references.
	 */
	spin_lock_irq(host->host_lock);
	fcport->rport = NULL;
	*((fc_port_t **)rport->dd_data) = NULL;
	spin_unlock_irq(host->host_lock);
}

static void
qla2x00_terminate_rport_io(struct fc_rport *rport)
{
	fc_port_t *fcport = *(fc_port_t **)rport->dd_data;

	if (!fcport)
		return;

	/*
	 * At this point all fcport's software-states are cleared.  Perform any
	 * final cleanup of firmware resources (PCBs and XCBs).
	 */
	if (fcport->loop_id != FC_NO_LOOP_ID)
		fcport->vha->hw->isp_ops->fabric_logout(fcport->vha,
			fcport->loop_id, fcport->d_id.b.domain,
			fcport->d_id.b.area, fcport->d_id.b.al_pa);

	/*
	 * Don't process anything on this vp if a deletion
	 * is in progress
	 */
	if (!test_bit(VP_DELETE_ACTIVE, &fcport->vha->dpc_flags))
		qla2x00_abort_fcport_cmds(fcport);

	scsi_target_unblock(&rport->dev);
}

static int
qla2x00_issue_lip(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	qla2x00_loop_reset(vha);
	return 0;
}

static struct fc_host_statistics *
qla2x00_get_fc_host_stats(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);
	struct qla_hw_data *ha = vha->hw;
	int rval = QLA_FUNCTION_FAILED;
	struct link_statistics *stats;
	dma_addr_t stats_dma;
	struct fc_host_statistics *pfc_host_stat;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);

	pfc_host_stat = &ha->fc_host_stat;
	memset(pfc_host_stat, -1, sizeof(struct fc_host_statistics));

	stats = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL, &stats_dma);
	if (stats == NULL) {
		DEBUG2_3_11(printk("%s(%ld): Failed to allocate memory.\n",
		    __func__, vha->host_no));
		goto done;
	}
	memset(stats, 0, DMA_POOL_SIZE);

	if (IS_FWI2_CAPABLE(ha)) {
		rval = qla24xx_get_isp_stats(base_vha, stats, stats_dma);
	} else if (atomic_read(&base_vha->loop_state) == LOOP_READY &&
		    !test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) &&
		    !test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags) &&
		    !base_vha->dpc_active) {
		/* Must be in a 'READY' state for statistics retrieval. */
		rval = qla2x00_get_link_status(base_vha, base_vha->loop_id,
				stats, stats_dma);
	}

	if (rval != QLA_SUCCESS)
		goto done_free;

	pfc_host_stat->link_failure_count = stats->link_fail_cnt;
	pfc_host_stat->loss_of_sync_count = stats->loss_sync_cnt;
	pfc_host_stat->loss_of_signal_count = stats->loss_sig_cnt;
	pfc_host_stat->prim_seq_protocol_err_count = stats->prim_seq_err_cnt;
	pfc_host_stat->invalid_tx_word_count = stats->inval_xmit_word_cnt;
	pfc_host_stat->invalid_crc_count = stats->inval_crc_cnt;
	if (IS_FWI2_CAPABLE(ha)) {
		pfc_host_stat->lip_count = stats->lip_cnt;
		pfc_host_stat->tx_frames = stats->tx_frames;
		pfc_host_stat->rx_frames = stats->rx_frames;
		pfc_host_stat->dumped_frames = stats->dumped_frames;
		pfc_host_stat->nos_count = stats->nos_rcvd;
	}
	pfc_host_stat->fcp_input_megabytes = ha->qla_stats.input_bytes >> 20;
	pfc_host_stat->fcp_output_megabytes = ha->qla_stats.output_bytes >> 20;

done_free:
        dma_pool_free(ha->s_dma_pool, stats, stats_dma);
done:
	return pfc_host_stat;
}

static void
qla2x00_get_host_symbolic_name(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	qla2x00_get_sym_node_name(vha, (uint8_t *)fc_host_symbolic_name(shost));
}

static void
qla2x00_set_host_system_hostname(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	set_bit(REGISTER_FDMI_NEEDED, &vha->dpc_flags);
}

static void
qla2x00_get_host_fabric_name(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);
	u64 node_name;

	if (vha->device_flags & SWITCH_FOUND)
		node_name = wwn_to_u64(vha->fabric_node_name);
	else
		node_name = wwn_to_u64(vha->node_name);

	fc_host_fabric_name(shost) = node_name;
}

static void
qla2x00_get_host_port_state(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);
	struct scsi_qla_host *base_vha = pci_get_drvdata(vha->hw->pdev);

	if (!base_vha->flags.online)
		fc_host_port_state(shost) = FC_PORTSTATE_OFFLINE;
       else {
               switch(atomic_read(&base_vha->loop_state)) {
                       case LOOP_UP:
                       case LOOP_READY:
                       case LOOP_UPDATE:
                               fc_host_port_state(shost) = FC_PORTSTATE_ONLINE;
                               break;
                       case LOOP_DOWN:
                       case LOOP_DEAD:
                               fc_host_port_state(shost) = FC_PORTSTATE_LINKDOWN;
                               break;
                       case LOOP_TIMEOUT:
                       default:
                               fc_host_port_state(shost) = FC_PORTSTATE_UNKNOWN;
                               break;
               }
       }
}

static int
qla24xx_vport_create(struct fc_vport *fc_vport, bool disable)
{
	int	ret = 0;
	uint8_t	qos = 0;
	scsi_qla_host_t *base_vha = (scsi_qla_host_t *) fc_vport->shost->hostdata;
	scsi_qla_host_t *vha = NULL;
	unsigned long cpu_flags = 0;
	struct qla_hw_data *ha = base_vha->hw;
	uint16_t options = 0;
	int cnt;
	struct req_que *req = ha->req_q_map[0];

	ret = qla24xx_vport_create_req_sanity_check(fc_vport);
	if (ret) {
		DEBUG15(printk("qla24xx_vport_create_req_sanity_check failed, "
		    "status %x\n", ret));
		return (ret);
	}

	vha = qla24xx_create_vhost(fc_vport, 0);
	if (vha == NULL) {
		DEBUG15(printk ("qla24xx_create_vhost failed, vha = %p\n",
		    vha));
		return FC_VPORT_FAILED;
	}
	if (disable) {
		atomic_set(&vha->vp_state, VP_OFFLINE);
		fc_vport_set_state(fc_vport, FC_VPORT_DISABLED);
	} else
		atomic_set(&vha->vp_state, VP_FAILED);

	/* ready to create vport */
	qla_printk(KERN_INFO, vha->hw, "VP entry id %d assigned.\n",
							vha->vp_idx);

	/* initialized vport states */
	atomic_set(&vha->loop_state, LOOP_DOWN);
	vha->vp_err_state=  VP_ERR_PORTDWN;
	vha->vp_prev_err_state=  VP_ERR_UNKWN;
	/* Check if physical ha port is Up */
	if (atomic_read(&base_vha->loop_state) == LOOP_DOWN ||
	    atomic_read(&base_vha->loop_state) == LOOP_DEAD) {
		/* Don't retry or attempt login of this virtual port */
		DEBUG15(printk ("scsi(%ld): pport loop_state is not UP.\n",
		    base_vha->host_no));
		atomic_set(&vha->loop_state, LOOP_DEAD);
		if (!disable)
			fc_vport_set_state(fc_vport, FC_VPORT_LINKDOWN);
	}

	if (scsi_add_host(vha->host, &fc_vport->dev)) {
		DEBUG15(printk("scsi(%ld): scsi_add_host failure for VP[%d].\n",
			vha->host_no, vha->vp_idx));
		goto vport_create_failed_2;
	}

	/* initialize attributes */
	fc_host_node_name(vha->host) = wwn_to_u64(vha->node_name);
	fc_host_port_name(vha->host) = wwn_to_u64(vha->port_name);
	fc_host_supported_classes(vha->host) =
		fc_host_supported_classes(base_vha->host);
	fc_host_supported_speeds(vha->host) =
		fc_host_supported_speeds(base_vha->host);

	qla24xx_vport_disable(fc_vport, disable);

	if (ha->flags.cpu_affinity_enabled) {
		req = ha->req_q_map[1];
		goto vport_queue;
	} else if (!ql2xmqqos || !ha->npiv_info)
		goto vport_queue;
	/* Create a request queue in QoS mode for the vport */
	for (cnt = 0; cnt < ha->nvram_npiv_size; cnt++) {
		if (memcmp(ha->npiv_info[cnt].port_name, vha->port_name, 8) == 0
			&& memcmp(ha->npiv_info[cnt].node_name, vha->node_name,
				8) == 0) {
			qos = ha->npiv_info[cnt].q_qos;
			break;
		}
	}
	if (qos) {
		ret = qla25xx_create_req_que(ha, options, vha->vp_idx, 0, 0,
			qos);
		if (!ret)
			qla_printk(KERN_WARNING, ha,
				"Can't create request queue for vp_idx:%d\n",
				vha->vp_idx);
		else {
			DEBUG2(qla_printk(KERN_INFO, ha,
				"Request Que:%d (QoS: %d) created for vp_idx:%d\n",
				ret, qos, vha->vp_idx));
				req = ha->req_q_map[ret];
		}
	}

vport_queue:
	vha->req = req;

	/* Enqueue AEN for SANSurfer to indicate vport create. */
	if (base_vha->ioctl->flags & IOCTL_AEN_TRACKING_ENABLE) {
		spin_lock_irqsave(&ha->hardware_lock, cpu_flags);
		qla2x00_enqueue_aen(base_vha, MBA_PORT_UPDATE, NULL);
		spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);
	}

	return 0;

vport_create_failed_2:
	qla24xx_disable_vp(vha);
	spin_lock_irqsave(&ha->vport_lock, cpu_flags);
	qla24xx_deallocate_vp_id(vha);
 	spin_unlock_irqrestore(&ha->vport_lock, cpu_flags);
	scsi_host_put(vha->host);
	return FC_VPORT_FAILED;
}

int
qla24xx_vport_delete(struct fc_vport *fc_vport)
{
	scsi_qla_host_t *vha = fc_vport->dd_data;
	unsigned long cpu_flags = 0;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	set_bit(VP_DELETE_ACTIVE, &vha->dpc_flags);

	/* Disable vport operations. */
	qla24xx_disable_vp(vha);
	
	/*
	 * This will ensure that all fc transport
	 * cleanup is completed and the work queues 
	 * are flushed. This function blocks till all 
	 * the work in the work queue is completed.
	 */ 
	fc_remove_host(vha->host);

	scsi_remove_host(vha->host);
 
        /* Delete the timer on the vport. */
        if (vha->timer_active) {
                qla2x00_vp_stop_timer(vha);
                DEBUG15(printk ("scsi(%ld): timer for the vport[%d] = %p "
                    "has stopped\n",
                    vha->host_no, vha->vp_idx, vha));
        }

	if (vha->req->id && !ha->flags.cpu_affinity_enabled) {
		if (qla25xx_delete_queues(vha) != QLA_SUCCESS)
			qla_printk(KERN_WARNING, ha,
				"Queue delete failed.\n");
	}

	/*
 	* scsi_add_host takes an additional self reference 
 	* so need to do an another scsi_host_put 
 	* (scsi_remove_host does the first one)
	*/ 
	spin_lock_irqsave(&ha->vport_lock, cpu_flags);
	qla2xxx_vha_put(vha);
	spin_unlock_irqrestore(&ha->vport_lock, cpu_flags);

	/* Enqueue AEN for SANSurfer to indicate vport delete. */
	if (base_vha->ioctl->flags & IOCTL_AEN_TRACKING_ENABLE) {
		spin_lock_irqsave(&ha->hardware_lock, cpu_flags);
		qla2x00_enqueue_aen(base_vha, MBA_PORT_UPDATE, NULL);
		spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);
	}

	return 0;
}

int
qla24xx_vport_disable(struct fc_vport *fc_vport, bool disable)
{
	scsi_qla_host_t *vha = fc_vport->dd_data;

	if (disable)
		qla24xx_disable_vp(vha);
	else
		qla24xx_enable_vp(vha);

	return 0;
}

struct fc_function_template qla2xxx_transport_functions = {

	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_speeds = 1,

	.get_host_port_id = qla2x00_get_host_port_id,
	.show_host_port_id = 1,
	.get_host_speed = qla2x00_get_host_speed,
	.show_host_speed = 1,
	.get_host_port_type = qla2x00_get_host_port_type,
	.show_host_port_type = 1,
	.get_host_symbolic_name = qla2x00_get_host_symbolic_name,
	.show_host_symbolic_name = 1,
	.set_host_system_hostname = qla2x00_set_host_system_hostname,
	.show_host_system_hostname = 1,
	.get_host_fabric_name = qla2x00_get_host_fabric_name,
	.show_host_fabric_name = 1,
	.get_host_port_state = qla2x00_get_host_port_state,
	.show_host_port_state = 1,

	.dd_fcrport_size = sizeof(struct fc_port *),
	.show_rport_supported_classes = 1,

	.get_starget_node_name = qla2x00_get_starget_node_name,
	.show_starget_node_name = 1,
	.get_starget_port_name = qla2x00_get_starget_port_name,
	.show_starget_port_name = 1,
	.get_starget_port_id  = qla2x00_get_starget_port_id,
	.show_starget_port_id = 1,

	.get_rport_dev_loss_tmo = qla2x00_get_rport_loss_tmo,
	.set_rport_dev_loss_tmo = qla2x00_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,

	.issue_fc_host_lip = qla2x00_issue_lip,
	.dev_loss_tmo_callbk = qla2x00_dev_loss_tmo_callbk,
	.terminate_rport_io = qla2x00_terminate_rport_io,
	.get_fc_host_stats = qla2x00_get_fc_host_stats,

	.vport_create = qla24xx_vport_create,
	.vport_disable = qla24xx_vport_disable,
	.vport_delete = qla24xx_vport_delete,
};

struct fc_function_template qla2xxx_transport_vport_functions = {

	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,

	.get_host_port_id = qla2x00_get_host_port_id,
	.show_host_port_id = 1,
	.get_host_speed = qla2x00_get_host_speed,
	.show_host_speed = 1,
	.get_host_port_type = qla2x00_get_host_port_type,
	.show_host_port_type = 1,
	.get_host_symbolic_name = qla2x00_get_host_symbolic_name,
	.show_host_symbolic_name = 1,
	.set_host_system_hostname = qla2x00_set_host_system_hostname,
	.show_host_system_hostname = 1,
	.get_host_fabric_name = qla2x00_get_host_fabric_name,
	.show_host_fabric_name = 1,
	.get_host_port_state = qla2x00_get_host_port_state,
	.show_host_port_state = 1,

	.dd_fcrport_size = sizeof(struct fc_port *),
	.show_rport_supported_classes = 1,

	.get_starget_node_name = qla2x00_get_starget_node_name,
	.show_starget_node_name = 1,
	.get_starget_port_name = qla2x00_get_starget_port_name,
	.show_starget_port_name = 1,
	.get_starget_port_id  = qla2x00_get_starget_port_id,
	.show_starget_port_id = 1,

	.get_rport_dev_loss_tmo = qla2x00_get_rport_loss_tmo,
	.set_rport_dev_loss_tmo = qla2x00_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,

	.issue_fc_host_lip = qla2x00_issue_lip,
	.dev_loss_tmo_callbk = qla2x00_dev_loss_tmo_callbk,
	.terminate_rport_io = qla2x00_terminate_rport_io,
	.get_fc_host_stats = qla2x00_get_fc_host_stats,
};

void
qla2x00_init_host_attr(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	u32 speed = FC_PORTSPEED_UNKNOWN;

	fc_host_node_name(vha->host) = wwn_to_u64(vha->node_name);
	fc_host_port_name(vha->host) = wwn_to_u64(vha->port_name);
	fc_host_supported_classes(vha->host) = FC_COS_CLASS3;
	if(vha->vp_idx) {
		fc_host_max_npiv_vports(vha->host) = -1;
		fc_host_npiv_vports_inuse(vha->host) = -1;
	} else {
		fc_host_max_npiv_vports(vha->host) = ha->max_npiv_vports
		    - (ha->num_vhosts - ha->cur_vport_count); 
		fc_host_npiv_vports_inuse(vha->host) = ha->cur_vport_count;
	}	

	if (IS_QLA81XX(ha))
		speed = FC_PORTSPEED_10GBIT;
	else if (IS_QLA25XX(ha))
		speed = FC_PORTSPEED_8GBIT | FC_PORTSPEED_4GBIT |
		    FC_PORTSPEED_2GBIT | FC_PORTSPEED_1GBIT;
	else if (IS_QLA24XX_TYPE(ha))
		speed = FC_PORTSPEED_4GBIT | FC_PORTSPEED_2GBIT |
		    FC_PORTSPEED_1GBIT;
	else if (IS_QLA23XX(ha))
		speed = FC_PORTSPEED_2GBIT | FC_PORTSPEED_1GBIT;
	else
		speed = FC_PORTSPEED_1GBIT;
	fc_host_supported_speeds(vha->host) = speed;
}
