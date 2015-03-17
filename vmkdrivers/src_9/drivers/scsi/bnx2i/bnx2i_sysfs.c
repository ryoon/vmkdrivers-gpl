/*
 * QLogic NetXtreme II iSCSI offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Anil Veerabhadrappa (anilgv@broadcom.com)
 */

#include "bnx2i.h"
#include <linux/ethtool.h>

#ifndef __VMKLNX__

#define BNX2I_SYSFS_VERSION	0x3


static ssize_t bnx2i_show_net_if_name(struct class_device *cdev, char *buf)
{
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	return sprintf(buf, "%s\n", hba->netdev->name);
}

static ssize_t bnx2i_show_sq_info(struct class_device *cdev, char *buf)
{
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	return sprintf(buf, "0x%x\n", hba->max_sqes);
}

static ssize_t bnx2i_set_sq_info(struct class_device *cdev,
				 const char *buf, size_t count)
{
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);
	u32 val;
	int max_sq_size;

	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		max_sq_size = BNX2I_5770X_SQ_WQES_MAX;
	else
		max_sq_size = BNX2I_570X_SQ_WQES_MAX;

	if (sscanf(buf, " 0x%x ", &val) > 0) {
		if ((val >= BNX2I_SQ_WQES_MIN) && (val <= max_sq_size ))
			hba->max_sqes = val;
	}
	return count;
}

static ssize_t bnx2i_show_rq_info(struct class_device *cdev, char *buf)
{
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	return sprintf(buf, "0x%x\n", hba->max_rqes);
}

static ssize_t bnx2i_set_rq_info(struct class_device *cdev, const char *buf,
							size_t count)
{
	u32 val;
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	if (sscanf(buf, " 0x%x ", &val) > 0) {
		if ((val >= BNX2I_RQ_WQES_MIN) &&
		    (val <= BNX2I_RQ_WQES_MAX)) {
			hba->max_rqes = val;
		}
	}
	return count;
}


static ssize_t bnx2i_show_ccell_info(struct class_device *cdev, char *buf)
{
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	return sprintf(buf, "0x%x\n", hba->num_ccell);
}

static ssize_t bnx2i_set_ccell_info(struct class_device *cdev,
				    const char *buf, size_t count)
{
	u32 val;
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	if (sscanf(buf, " 0x%x ", &val) > 0) {
		if ((val >= BNX2I_CCELLS_MIN) &&
		    (val <= BNX2I_CCELLS_MAX)) {
			hba->num_ccell = val;
		}
	}
	return count;
}


static ssize_t bnx2i_read_pci_trigger_reg(struct class_device *cdev,
					  char *buf)
{
	u32 reg_val;
	struct bnx2i_hba *hba =
		container_of(cdev, struct bnx2i_hba, class_dev);

	if (!hba->regview)
		return 0;

#define PCI_EVENT_TRIGGER_REG	0xCAC	/* DMA WCHAN STAT10 REG */
	reg_val = readl(hba->regview + PCI_EVENT_TRIGGER_REG);
	return sprintf(buf, "0x%x\n", reg_val);
}


static CLASS_DEVICE_ATTR (net_if_name, S_IRUGO,
			 bnx2i_show_net_if_name, NULL);
static CLASS_DEVICE_ATTR (sq_size, S_IRUGO | S_IWUSR,
			 bnx2i_show_sq_info, bnx2i_set_sq_info);
static CLASS_DEVICE_ATTR (rq_size, S_IRUGO | S_IWUSR,
			 bnx2i_show_rq_info, bnx2i_set_rq_info);
static CLASS_DEVICE_ATTR (num_ccell, S_IRUGO | S_IWUSR,
			 bnx2i_show_ccell_info, bnx2i_set_ccell_info);
static CLASS_DEVICE_ATTR (pci_trigger, S_IRUGO,
			 bnx2i_read_pci_trigger_reg, NULL);


static struct class_device_attribute *bnx2i_class_attributes[] = {
	&class_device_attr_net_if_name,
	&class_device_attr_sq_size,
	&class_device_attr_rq_size,
	&class_device_attr_num_ccell,
	&class_device_attr_pci_trigger,
};

static void bnx2i_sysfs_release(struct class_device *class_dev)
{
}

struct class_device port_class_dev;


static struct class bnx2i_class = {
	.name	= "bnx2i",
	.release = bnx2i_sysfs_release,
};

int bnx2i_register_sysfs(struct bnx2i_hba *hba)
{
	struct class_device *class_dev = &hba->class_dev;
	char dev_name[BUS_ID_SIZE];
	struct ethtool_drvinfo drv_info;
	u32 bus_no;
	u32 dev_no;
	u32 func_no;
	u32 extra;
	int ret;
	int i;

	if (hba->cnic && hba->cnic->netdev) {
		hba->cnic->netdev->ethtool_ops->get_drvinfo(hba->cnic->netdev,
							    &drv_info);
		sscanf(drv_info.bus_info, "%x:%x:%x.%d",
		       &extra, &bus_no, &dev_no, &func_no);
	}
	class_dev->class = &bnx2i_class;
	class_dev->class_data = hba;
	snprintf(dev_name, BUS_ID_SIZE, "%.2x:%.2x.%.1x",
			 bus_no, dev_no, func_no);
	strlcpy(class_dev->class_id, dev_name, BUS_ID_SIZE);

	ret = class_device_register(class_dev);
	if (ret)
		goto err;

	for (i = 0; i < ARRAY_SIZE(bnx2i_class_attributes); ++i) {
		ret = class_device_create_file(class_dev,
					       bnx2i_class_attributes[i]);
		if (ret)
			goto err_unregister;
	}

	return 0;

err_unregister:
	class_device_unregister(class_dev);
err:
	return ret;
}

void bnx2i_unregister_sysfs(struct bnx2i_hba *hba)
{
	class_device_unregister(&hba->class_dev);
}

int bnx2i_sysfs_setup(void)
{
	int ret;
	ret = class_register(&bnx2i_class);
	return ret;
}

void bnx2i_sysfs_cleanup(void)
{
	class_unregister(&bnx2i_class);
}

#else	/* __VMKLNX__ */

extern unsigned int bnx2i_debug_level;

struct bnx2i_proc_param {
	const char *name;
	int type;
	unsigned int *value;
};

#define BNX2I_PARAM_TYPE_HEX		1
#define BNX2I_PARAM_TYPE_DEC		2
#define BNX2I_PARAM_TYPE_STR		3

static struct bnx2i_proc_param proc_param[] = {
	{ "bnx2i_debug_level=", BNX2I_PARAM_TYPE_HEX, &bnx2i_debug_level},
	{ NULL, NULL },
};

struct info_str {
        char *buffer;
        int length;
        off_t offset;
        int pos;
};

static void
copy_mem_info(struct info_str *info, char *data, int len)
{
	if (info->pos + len > info->offset + info->length) {
		len = info->offset + info->length - info->pos;
	}

	if (info->pos + len < info->offset) {
		info->pos += len;
		return;
	}

	if (info->pos < info->offset) {
		off_t partial;

		partial = info->offset - info->pos;
		data += partial;
		info->pos += partial;
		len  -= partial;
	}

	if (len > 0) {
		memcpy(info->buffer, data, len);
		info->pos += len;
		info->buffer += len;
	}
}

static int
copy_info(struct info_str *info, char *fmt, ...)
{
	va_list args;
	char buf[256];
	int len;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info, buf, len);
	return (len);
}


static void bnx2i_read_proc_data(struct bnx2i_hba *hba, char *buffer)
{
	int i;

	for (i = 0; proc_param[i].name != NULL; i++) {
		if (strncmp(buffer, proc_param[i].name,
		    strlen(proc_param[i].name)) == 0) {
			switch(proc_param[i].type) {
			case BNX2I_PARAM_TYPE_HEX:
            			sscanf(buffer + strlen(proc_param[i].name),
					"0x%x", proc_param[i].value);
				break;
			case BNX2I_PARAM_TYPE_DEC:
				/* Nothing yet */
				break;
			case BNX2I_PARAM_TYPE_STR:
				/* Nothing yet */
				break;
			default:
				printk(KERN_ERR "bnx2i: unknown proc param\n");
			}
		}
	}
}

int bnx2i_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
		    off_t offset, int length, int inout)
{
	int retval = -EINVAL;
	struct info_str info;
	struct bnx2i_hba *hba;
	struct bnx2i_sess *sess;
	extern unsigned int event_coal_div;
	extern unsigned int bnx2i_nopout_when_cmds_active;
	extern unsigned int en_tcp_dack;
	extern unsigned int time_stamps;
	extern unsigned int error_mask1;
	extern unsigned int error_mask2;
	extern unsigned int sq_size;
	extern unsigned int rq_size;
	extern unsigned int tcp_buf_size;
	extern unsigned int event_coal_min;

	if (shost == NULL)
		return (-EINVAL);
	hba = (struct bnx2i_hba *)shost->hostdata;


	if (inout == TRUE) {
		bnx2i_read_proc_data(hba, buffer);
		retval = length;
		goto done;
	}

	if (start)
		*start = buffer;

	info.buffer     = buffer;
	info.length     = length;
	info.offset     = offset;
	info.pos        = 0;

	copy_info(&info, "QLogic NetXtreme II Offload iSCSI Initiator\n");
	copy_info(&info, "  Global Params\n");
	copy_info(&info, "    event_coal_div                = %u\n", event_coal_div);
	copy_info(&info, "    bnx2i_nopout_when_cmds_active = %u\n", bnx2i_nopout_when_cmds_active);
	copy_info(&info, "    en_tcp_dack                   = %u\n", en_tcp_dack);
	copy_info(&info, "    time_stamps                   = %u\n", time_stamps);
	copy_info(&info, "    error_mask1                   = %u\n", error_mask1);
	copy_info(&info, "    error_mask2                   = %u\n", error_mask2);
	copy_info(&info, "    sq_size                       = %u\n", sq_size);
	copy_info(&info, "    rq_size                       = %u\n", rq_size);
	copy_info(&info, "    tcp_buf_size                  = %u\n", tcp_buf_size);
	copy_info(&info, "    event_coal_min                = %u\n", event_coal_min);
	copy_info(&info, "    cmd_cmpl_per_work             = %u\n", cmd_cmpl_per_work);
	copy_info(&info, "    max_bnx2x_sessions            = %u\n", max_bnx2x_sessions);
	copy_info(&info, "    max_bnx2_sessions             = %u\n", max_bnx2_sessions);
	copy_info(&info, "    bnx2i_debug_level             = %u\n", bnx2i_debug_level);

	copy_info(&info, "  HBA\n");
	copy_info(&info, "    age                  = %u\n", hba->age);
	copy_info(&info, "    OOO COUNT            = %u\n", hba->cnic->ooo_tx_count);
	copy_info(&info, "    PMTU mismatch        = %u\n", hba->cnic->pmtu_fails);
	copy_info(&info, "    cnic_dev_type        = %lu\n", hba->cnic_dev_type);
	copy_info(&info, "    mail_queue_access    = %u\n", hba->mail_queue_access);
	copy_info(&info, "    reg_with_cnic        = %lu\n", hba->reg_with_cnic);
	copy_info(&info, "    adapter_state        = %lu\n", hba->adapter_state);
	copy_info(&info, "    mtu_supported        = %lu\n", hba->mtu_supported);
	copy_info(&info, "    max - sqes           = %u\n", hba->max_sqes);
	copy_info(&info, "    max - rqes           = %u\n", hba->max_rqes);
	copy_info(&info, "    max - cqes           = %u\n", hba->max_cqes);
	copy_info(&info, "    max - num_ccell      = %u\n", hba->num_ccell);
	copy_info(&info, "    num_active_sess      = %d\n", hba->num_active_sess);
	copy_info(&info, "    ofld_conns_active    = %d\n", hba->ofld_conns_active);
	copy_info(&info, "    max_active_conns     = %d\n", hba->max_active_conns);
	copy_info(&info, "    conn_teardown_tmo    = %d\n", hba->conn_teardown_tmo);
	copy_info(&info, "    conn_ctx_destroy_tmo = %d\n", hba->conn_ctx_destroy_tmo);
	copy_info(&info, "    hba_shutdown_tmo     = %d\n", hba->hba_shutdown_tmo);
	copy_info(&info, "    ctx_ccell_tasks      = %d\n", hba->ctx_ccell_tasks);
	copy_info(&info, "    num_wqe_sent         = %u\n", hba->num_wqe_sent);
	copy_info(&info, "    num_cqe_rcvd         = %u\n", hba->num_cqe_rcvd);
	copy_info(&info, "    num_intr_claimed     = %u\n", hba->num_intr_claimed);
	copy_info(&info, "    link_changed_count   = %u\n", hba->link_changed_count);
	copy_info(&info, "    ipaddr_changed_count = %u\n", hba->ipaddr_changed_count);
	copy_info(&info, "    num_sess_opened      = %u\n", hba->num_sess_opened);
	copy_info(&info, "    num_conn_opened      = %u\n", hba->num_conn_opened);
	copy_info(&info, "    stop_evt - ifc_poll  = %u\n", hba->stop_event_ifc_abort_poll);
	copy_info(&info, "    stop_evt - ifc_bind  = %u\n", hba->stop_event_ifc_abort_bind);
	copy_info(&info, "    stop_evt - ifc_login = %u\n", hba->stop_event_ifc_abort_login);
	copy_info(&info, "    stop_evt - ep_rej    = %u\n", hba->stop_event_ep_conn_failed);
	copy_info(&info, "    stop_evt - repeat    = %u\n", hba->stop_event_repeat);
	copy_info(&info, "    task_cleanup_failed  = %u\n", hba->task_cleanup_failed);
	copy_info(&info, "    tcp_error_kcqes      = %u\n", hba->tcp_error_kcqes);
	copy_info(&info, "    iscsi_error_kcqes    = %u\n", hba->iscsi_error_kcqes);
	copy_info(&info, "    ep_tmo_active_cnt     = %u\n", hba->ep_tmo_active_cnt);
	copy_info(&info, "    ep_tmo_cmpl_cnt       = %u\n", hba->ep_tmo_cmpl_cnt);
	copy_info(&info, "    max_scsi_task_queued  = %u\n", hba->max_scsi_task_queued);

	copy_info(&info, "  SESSIONS\n");
	spin_lock(&hba->lock);
	list_for_each_entry(sess, &hba->active_sess, link) {
		u32 l5_cid = 0xFF, cid = 0xFF;
		if (sess->lead_conn && sess->lead_conn->ep) {
			l5_cid = sess->lead_conn->ep->ep_iscsi_cid;
			cid = sess->lead_conn->ep->ep_cid;
		}
		copy_info(&info, "    SESSION(%p)\n", sess);
		copy_info(&info, "      timestamp                    = %lu\n", sess->timestamp);
		copy_info(&info, "      worker_time_slice            = %lu\n", sess->worker_time_slice);
		copy_info(&info, "      state                        = %u\n", sess->state);
		copy_info(&info, "      recovery_state               = %lu\n", sess->recovery_state);
		copy_info(&info, "      old_recovery_state           = %lu\n", sess->old_recovery_state);
		copy_info(&info, "      tmf_active                   = %u\n",
		          atomic_read(&sess->tmf_active));
		copy_info(&info, "      do_recovery_inprogess        = %u\n",
		          atomic_read(&sess->do_recovery_inprogess));
		copy_info(&info, "      device_offline               = %u\n",
		          atomic_read(&sess->device_offline));
		copy_info(&info, "      max_iscsi_tasks              = %d\n", sess->max_iscsi_tasks);
		copy_info(&info, "      num_free_cmds                = %d\n", sess->num_free_cmds);
		copy_info(&info, "      allocated_cmds               = %d\n", sess->allocated_cmds);
		copy_info(&info, "      total_cmds_allocated         = %d\n", sess->total_cmds_allocated);
		copy_info(&info, "      total_cmds_freed             = %d\n", sess->total_cmds_freed);
		copy_info(&info, "      sq_size                      = %d\n", sess->sq_size);
		copy_info(&info, "      login_noop_pending           = %u\n",
		          atomic_read(&sess->login_noop_pending));
		copy_info(&info, "      tmf_pending                  = %u\n",
		          atomic_read(&sess->tmf_pending));
		copy_info(&info, "      logout_pending               = %u\n",
		          atomic_read(&sess->logout_pending));
		copy_info(&info, "      nop_resp_pending             = %u\n",
		          atomic_read(&sess->nop_resp_pending));
		copy_info(&info, "      pend_cmd_count               = %u\n", sess->pend_cmd_count);
		copy_info(&info, "      active_cmd_count             = %u\n", sess->active_cmd_count);
		copy_info(&info, "      cmd_cleanup_req              = %d\n", sess->cmd_cleanup_req);
		copy_info(&info, "      cmd_cleanup_cmpl             = %d\n", sess->cmd_cleanup_cmpl);
		copy_info(&info, "      total_cmds_sent              = %u\n", sess->total_cmds_sent);
		copy_info(&info, "      total_cmds_queued            = %u\n", sess->total_cmds_queued);
		copy_info(&info, "      total_cmds_completed         = %u\n", sess->total_cmds_completed);
		copy_info(&info, "      total_cmds_failed            = %u\n", sess->total_cmds_failed);
		copy_info(&info, "      total_cmds_completed_by_chip = %u\n", sess->total_cmds_completed_by_chip);
		copy_info(&info, "      cmdsn                        = %u\n", sess->cmdsn);
		copy_info(&info, "      exp_cmdsn                    = %u\n", sess->exp_cmdsn);
		copy_info(&info, "      max_cmdsn                    = %u\n", sess->max_cmdsn);
		copy_info(&info, "      initial_r2t                  = %d\n", sess->initial_r2t);
		copy_info(&info, "      max_r2t                      = %u\n", sess->max_r2t);
		copy_info(&info, "      imm_data                     = %u\n", sess->imm_data);
		copy_info(&info, "      first_burst_len              = %u\n", sess->first_burst_len);
		copy_info(&info, "      max_burst_len                = %u\n", sess->max_burst_len);
		copy_info(&info, "      time2wait                    = %u\n", sess->time2wait);
		copy_info(&info, "      time2retain                  = %u\n", sess->time2retain);
		copy_info(&info, "      pdu_inorder                  = %d\n", sess->pdu_inorder);
		copy_info(&info, "      dataseq_inorder              = %d\n", sess->dataseq_inorder);
		copy_info(&info, "      erl                          = %d\n", sess->erl);
		copy_info(&info, "      tgt_prtl_grp                 = %d\n", sess->tgt_prtl_grp);
		copy_info(&info, "      target_name                  = %s\n", sess->target_name);
		copy_info(&info, "      isid                         = %s\n", sess->isid);
		copy_info(&info, "      num_active_conn              = %u\n", sess->num_active_conn);
		copy_info(&info, "      max_conns                    = %u\n", sess->max_conns);
		copy_info(&info, "      last_nooput_requested        = %lu\n", sess->last_nooput_requested);
		copy_info(&info, "      last_nooput_posted           = %lu\n", sess->last_nooput_posted);
		copy_info(&info, "      last_noopin_indicated        = %lu\n", sess->last_noopin_indicated);
		copy_info(&info, "      last_noopin_processed        = %lu\n", sess->last_noopin_processed);
		copy_info(&info, "      last_nooput_sn               = %lu\n", sess->last_nooput_sn);
		copy_info(&info, "      noopout_resp_count           = %lu\n", sess->noopout_resp_count);
		copy_info(&info, "      unsol_noopout_count          = %lu\n", sess->unsol_noopout_count);
		copy_info(&info, "      noopout_requested_count      = %d\n", sess->noopout_requested_count);
		copy_info(&info, "      noopout_posted_count         = %d\n", sess->noopout_posted_count);
		copy_info(&info, "      noopin_indicated_count       = %d\n", sess->noopin_indicated_count);
		copy_info(&info, "      noopin_processed_count       = %d\n", sess->noopin_processed_count);
		copy_info(&info, "      tgt_noopin_count             = %d\n", sess->tgt_noopin_count);
		copy_info(&info, "      alloc_scsi_task_failed       = %lu\n", sess->alloc_scsi_task_failed);
		copy_info(&info, "      cid                          = %x\n", cid);
		copy_info(&info, "      iscsi cid                    = %x\n", l5_cid);
	}
	spin_unlock(&hba->lock);

	copy_info(&info, "\n\0");

	retval = info.pos > info.offset ? info.pos - info.offset : 0;
done:
	return retval;
}
#endif
