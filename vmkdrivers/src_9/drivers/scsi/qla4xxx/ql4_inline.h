/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

static inline struct ddb_entry *
qla4xxx_lookup_ddb_by_os_index(struct scsi_qla_host *ha, int os_idx)
{
	struct ddb_entry *ddb_entry = NULL;
	struct ddb_entry *detemp;
	int found = 0;

	list_for_each_entry_safe(ddb_entry, detemp, &ha->ddb_list, list) {
		if (ddb_entry->os_target_id == os_idx) {
			found = 1;
			break;
		}
	}

	if (!found)
	   ddb_entry = NULL;

	DEBUG3(printk("scsi%d: %s: ddb[%d], ddb_entry = %p\n",
	    ha->host_no, __func__, fw_ddb_index, ddb_entry));

	return ddb_entry;
}

/**
 * qla4xxx_queue_aen_log - queue AENs to be reported to application layer
 * @ha: Pointer to host adapter structure.
 * @mbox_sts: Pointer to mailbox status structure.
 *
 * Store AENs to be reported to the application layer when requested
 **/
static inline void
qla4xxx_queue_aen_log(struct scsi_qla_host *ha, uint32_t *mbox_sts)
{
	int i;
	if (ha->aen_log.count < MAX_AEN_ENTRIES) {
		for (i = 0; i < MBOX_AEN_REG_COUNT; i++)
			ha->aen_log.entry[ha->aen_log.count].mbox_sts[i] =
				mbox_sts[i];
		ha->aen_log.count++;
	}
}

static inline void qla4xxx_queue_lun_change_aen(struct scsi_qla_host *ha,
						     uint32_t index)
{
	uint32_t mbox_sts[MBOX_REG_COUNT];
	memset(mbox_sts, 0, sizeof(mbox_sts));
	mbox_sts[0] = MBOX_DRVR_ASTS_LUN_STATUS_CHANGE;
	mbox_sts[1] = index;
	qla4xxx_queue_aen_log(ha, &mbox_sts[0]);

	DEBUG4(printk("scsi%d: %s: AEN 0x7003 index[%d]\n",
		ha->host_no, __func__, index));
}

/*
 *
 * qla4xxx_lookup_ddb_by_fw_index
 *	This routine locates a device handle given the firmware device
 *	database index.	 If device doesn't exist, returns NULL.
 *
 * Input:
 *	ha - Pointer to host adapter structure.
 *	fw_ddb_index - Firmware's device database index
 *
 * Returns:
 *	Pointer to the corresponding internal device database structure
 */
static inline struct ddb_entry *
qla4xxx_lookup_ddb_by_fw_index(struct scsi_qla_host *ha, uint32_t fw_ddb_index)
{
	struct ddb_entry *ddb_entry = NULL;

	if ((fw_ddb_index < MAX_DDB_ENTRIES) &&
	    (ha->fw_ddb_index_map[fw_ddb_index] != NULL)) {
		ddb_entry = ha->fw_ddb_index_map[fw_ddb_index];
	}

	DEBUG3(printk("scsi%d: %s: index [%d], ddb_entry = %p\n",
	    ha->host_no, __func__, fw_ddb_index, ddb_entry));

	return ddb_entry;
}

static inline void
__qla4xxx_enable_intrs(struct scsi_qla_host *ha)
{
	if (is_qla4022(ha) | is_qla4032(ha)) {
		writel(set_rmask(IMR_SCSI_INTR_ENABLE),
		       &ha->reg->u1.isp4022.intr_mask);
		readl(&ha->reg->u1.isp4022.intr_mask);
	} else {
		writel(set_rmask(CSR_SCSI_INTR_ENABLE), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
	}
	set_bit(AF_INTERRUPTS_ON, &ha->flags);
}

static inline void
__qla4xxx_disable_intrs(struct scsi_qla_host *ha)
{
	if (is_qla4022(ha) | is_qla4032(ha)) {
		writel(clr_rmask(IMR_SCSI_INTR_ENABLE),
		       &ha->reg->u1.isp4022.intr_mask);
		readl(&ha->reg->u1.isp4022.intr_mask);
	} else {
		writel(clr_rmask(CSR_SCSI_INTR_ENABLE), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
	}
	clear_bit(AF_INTERRUPTS_ON, &ha->flags);
}

static inline void
qla4xxx_enable_intrs(struct scsi_qla_host *ha)
{
	unsigned long flags;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	__qla4xxx_enable_intrs(ha);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static inline void
qla4xxx_disable_intrs(struct scsi_qla_host *ha)
{
	unsigned long flags;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	__qla4xxx_disable_intrs(ha);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}
