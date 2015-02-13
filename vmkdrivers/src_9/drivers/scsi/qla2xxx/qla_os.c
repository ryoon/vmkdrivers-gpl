/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>

#if defined(__VMKLNX__)
#include <vmklinux_9/vmklinux_scsi.h>
#endif

/*
 * Driver version
 */
char qla2x00_version_str[40];

/*
 * SRB allocation cache
 */
#if defined(__VMKLNX__)
static kmem_cache_t *srb_cachep;
#else
static struct kmem_cache *srb_cachep;
#endif

/*
 * Ioctl related information.
 */
int num_hosts;
DEFINE_MUTEX(instance_lock);	/* Protects qla_hostlist and host_instance_map. */
unsigned long host_instance_map[(MAX_HBAS / 8)/ sizeof(unsigned long)];
int apiHBAInstance;

int ql2xlogintimeout = 20;
module_param(ql2xlogintimeout, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xlogintimeout,
		"Login timeout value in seconds.");

#if defined(__VMKLNX__)
int qlport_down_retry = 5;
#else
int qlport_down_retry = 30;
#endif
module_param(qlport_down_retry, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(qlport_down_retry,
		"Maximum number of command retries to a port that returns "
		"a PORT-DOWN status.");

int ql2xplogiabsentdevice;
module_param(ql2xplogiabsentdevice, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xplogiabsentdevice,
		"Option to enable PLOGI to devices that are not present after "
		"a Fabric scan.  This is needed for several broken switches. "
		"Default is 0 - no PLOGI. 1 - perfom PLOGI.");

int ql2xloginretrycount = 0;
module_param(ql2xloginretrycount, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xloginretrycount,
		"Specify an alternate value for the NVRAM login retry count.");

int ql2xallocfwdump = 1;
module_param(ql2xallocfwdump, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xallocfwdump,
		"Option to enable allocation of memory for a firmware dump "
		"during HBA initialization.  Memory allocation requirements "
		"vary by ISP type.  Default is 1 - allocate memory.");

int ql2xioctltimeout = QLA_PT_CMD_TOV;
module_param(ql2xioctltimeout, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xioctltimeout,
		"IOCTL timeout value in seconds for pass-thur commands. "
		"Default is 66 seconds.");

int ql2xextended_error_logging = 0;
module_param(ql2xextended_error_logging, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xextended_error_logging,
		"Option to enable extended error logging, "
		"Default is 0 - no logging. 1 - log errors.");

int ql2xdevdiscgoldfw = 0;
module_param(ql2xdevdiscgoldfw, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xdevdiscgoldfw,
		"Option to enable device discovery with golden firmware "
		"Default is 0 - no discovery. 1 - discover device.");

static void qla2x00_free_device(scsi_qla_host_t *);

int ql2xfdmienable = 1;
module_param(ql2xfdmienable, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xfdmienable,
		"Enables FDMI registratons "
		"Default is 0 - no FDMI. 1 - perfom FDMI.");

#define MAX_Q_DEPTH	64
static int ql2xmaxqdepth = MAX_Q_DEPTH;
module_param(ql2xmaxqdepth, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xmaxqdepth,
		"Maximum queue depth to report for target devices.");

int ql2xqfulltracking = 1;
module_param(ql2xqfulltracking, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xqfulltracking,
		"Controls whether the driver tracks queue full status "
		"returns and dynamically adjusts a scsi device's queue "
		"depth.  Default is 1, perform tracking.  Set to 0 to "
		"disable dynamic tracking and adjustment of queue depth.");

int ql2xqfullrampup = 120;
module_param(ql2xqfullrampup, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xqfullrampup,
		"Number of seconds to wait to begin to ramp-up the queue "
		"depth for a device after a queue-full condition has been "
		"detected.  Default is 120 seconds.");

int ql2xiidmaenable = 1;
module_param(ql2xiidmaenable, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xiidmaenable,
		"Enables iIDMA settings "
		"Default is 1 - perform iIDMA. 0 - no iIDMA.");

int ql2xusedefmaxrdreq = 0;
module_param(ql2xusedefmaxrdreq, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(usedefmaxrdreq,
		"Default is 0 - adjust PCIe Maximum Read Request Size. "
		"1 - use system default.");

int ql2xenablemsi = 1;
module_param(ql2xenablemsi, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xenablemsi,
		"Enables MSI-X/MSI interrupt scheme "
		"Default is 1 - enable MSI-X/MSI. 0 - disable MSI-X/MSI.");

int ql2xenablemsi24xx = 0;
module_param(ql2xenablemsi24xx, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xenablemsi24xx,
		"Enables MSIx/MSI interrupt scheme on 24xx cards"
		"Default is 0 - disable MSI-X/MSI. 1 - enable MSI-X/MSI.");

int ql2xenablemsi2422 = 0;
module_param(ql2xenablemsi2422, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xenablemsi2422,
		"Enables MSI interrupt scheme on 2422s"
		"Default is 0 - disable MSI-X/MSI. 1 - enable MSI-X/MSI.");

#if defined(__VMKLNX__)
int ql2xoperationmode = 1;
module_param(ql2xoperationmode, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xoperationmode,
		"Option to disable ZIO mode for ISP24XX:"
		"   Default is 1, set 0 to disable");

int ql2xintrdelaytimer = 1;
module_param(ql2xintrdelaytimer, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xintrdelaytimer,
		"ZIO: Waiting time for Firmware before it generates an "
		"interrupt to the host to notify completion of request.");

int ql2xusedrivernaming = 0;
module_param(ql2xusedrivernaming, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xusedrivernaming,
		"Enables Consistent Device Naming feature "
		"Default is 0, set 1 to enable.");

int ql2xcmdtimeout = 20;
module_param(ql2xcmdtimeout, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xcmdtimeout,
		 "Timeout value in seconds for scsi command, default is 20");

int ql2xexecution_throttle = 0;
module_param(ql2xexecution_throttle, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xexecution_throttle,
		 "IOCB exchange count for HBA."
		 "Default is 0, set intended value to override Firmware defaults.");

int ql2xmaxsgs = 0; 
module_param(ql2xmaxsgs, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xmaxsgs,
		"Maximum scatter/gather entries per request,"
		"Default is the Max the OS Supports.");
#endif

int ql2xmqqos = 0;
#if 0 /*disable for now*/
module_param(ql2xmqqos, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xmqqos,
		"Enables MQ settings "
		"Default is 0. Set it to number \
			of queues in MQ QoS mode.");
#endif

int ql2xmqcpuaffinity = 1;
module_param(ql2xmqcpuaffinity, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xmqcpuaffinity,
		"Enables CPU affinity settings for the driver "
		"Default is 0 for no affinity of request and response IO. "
		"Set it to 1 to turn on the cpu affinity.");

int ql2xfwloadbin = 0;
module_param(ql2xfwloadbin, int, S_IRUGO|S_IRUSR);
MODULE_PARM_DESC(ql2xfwloadbin,
		"Option to specify location from which to load ISP firmware:\n"
		" 2 -- load firmware via the request_firmware() (hotplug)\n"
		"      interface.\n"
		" 1 -- load firmware from flash.\n"
		" 0 -- use default semantics.\n");

/*
 * Proc structures and functions
 */
struct info_str {
        char    *buffer;
        int     length;
        off_t   offset;
        int     pos;
};

static void copy_mem_info(struct info_str *, char *, int);
static int copy_info(struct info_str *, char *, ...);

LIST_HEAD(qla_hostlist);
/*
 * SCSI host template entry points
 */
static int qla2xxx_slave_configure(struct scsi_device * device);
static int qla2xxx_slave_alloc(struct scsi_device *);
static int qla2xxx_scan_finished(struct Scsi_Host *, unsigned long time);
static void qla2xxx_scan_start(struct Scsi_Host *);
static void qla2xxx_slave_destroy(struct scsi_device *);
static int qla24xx_queuecommand(struct scsi_cmnd *cmd,
		void (*fn)(struct scsi_cmnd *));
static int qla2x00_queuecommand(struct scsi_cmnd *cmd,
		void (*fn)(struct scsi_cmnd *));
static int qla2xxx_eh_abort(struct scsi_cmnd *);
static int qla2xxx_eh_device_reset(struct scsi_cmnd *);
static int qla2xxx_eh_bus_reset(struct scsi_cmnd *);
static int qla2xxx_eh_host_reset(struct scsi_cmnd *);
static int qla2x00_device_reset(scsi_qla_host_t *, fc_port_t *, int);

static int qla2x00_change_queue_depth(struct scsi_device *, int);
static int qla2x00_change_queue_type(struct scsi_device *, int);

static int qla2x00_proc_info(struct Scsi_Host *, char *, char **,
	off_t, int, int);

struct scsi_host_template qla2x00_driver_template = {
	.module			= THIS_MODULE,
	.name			= QLA2XXX_DRIVER_NAME,
	.proc_name              = QLA2XXX_DRIVER_NAME,
	.proc_info		= qla2x00_proc_info,
	.queuecommand		= qla2x00_queuecommand,

	.eh_abort_handler	= qla2xxx_eh_abort,
	.eh_device_reset_handler = qla2xxx_eh_device_reset,
	.eh_bus_reset_handler	= qla2xxx_eh_bus_reset,
	.eh_host_reset_handler	= qla2xxx_eh_host_reset,

	.slave_configure	= qla2xxx_slave_configure,

	.slave_alloc		= qla2xxx_slave_alloc,
	.slave_destroy		= qla2xxx_slave_destroy,
	.scan_finished		= qla2xxx_scan_finished,
	.scan_start		= qla2xxx_scan_start,
	.change_queue_depth	= qla2x00_change_queue_depth,
	.change_queue_type	= qla2x00_change_queue_type,
	.this_id		= -1,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
	.sg_tablesize		= SG_ALL,

	/*
	 * The RISC allows for each command to transfer (2^32-1) bytes of data,
	 * which equates to 0x800000 sectors.
	 */
	.max_sectors		= 0xFFFF,
	.shost_attrs		= qla2x00_host_attrs,
};

struct scsi_host_template qla24xx_driver_template = {
	.module			= THIS_MODULE,
	.name			= QLA2XXX_DRIVER_NAME,
	.proc_name              = QLA2XXX_DRIVER_NAME,
	.proc_info		= qla2x00_proc_info,
	.queuecommand		= qla24xx_queuecommand,

	.eh_abort_handler	= qla2xxx_eh_abort,
	.eh_device_reset_handler = qla2xxx_eh_device_reset,
	.eh_bus_reset_handler	= qla2xxx_eh_bus_reset,
	.eh_host_reset_handler	= qla2xxx_eh_host_reset,

	.slave_configure	= qla2xxx_slave_configure,

	.slave_alloc		= qla2xxx_slave_alloc,
	.slave_destroy		= qla2xxx_slave_destroy,
	.scan_finished		= qla2xxx_scan_finished,
	.scan_start		= qla2xxx_scan_start,
	.change_queue_depth	= qla2x00_change_queue_depth,
	.change_queue_type	= qla2x00_change_queue_type,
	.this_id		= -1,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
	.sg_tablesize		= SG_ALL,

	.max_sectors		= 0xFFFF,
	.shost_attrs		= qla2x00_host_attrs,
};

struct scsi_transport_template *qla2xxx_transport_template = NULL;
struct scsi_transport_template *qla2xxx_transport_vport_template = NULL;

/* TODO Convert to inlines
 *
 * Timer routines
 */

void qla2x00_timer(scsi_qla_host_t *);

__inline__ void qla2x00_start_timer(scsi_qla_host_t *,
    void *, unsigned long);
static __inline__ void qla2x00_restart_timer(scsi_qla_host_t *, unsigned long);
__inline__ void qla2x00_stop_timer(scsi_qla_host_t *);

__inline__ void
qla2x00_start_timer(scsi_qla_host_t *vha, void *func, unsigned long interval)
{
	init_timer(&vha->timer);
	vha->timer.expires = jiffies + interval * HZ;
	vha->timer.data = (unsigned long)vha;
	vha->timer.function = (void (*)(unsigned long))func;
	add_timer(&vha->timer);
	vha->timer_active = 1;
}

static inline void
qla2x00_restart_timer(scsi_qla_host_t *vha, unsigned long interval)
{
	mod_timer(&vha->timer, jiffies + interval * HZ);
}

__inline__ void
qla2x00_stop_timer(scsi_qla_host_t *vha)
{
	del_timer_sync(&vha->timer);
	vha->timer_active = 0;
}

static int qla2x00_do_dpc(void *data);

static void qla2x00_rst_aen(scsi_qla_host_t *);

uint8_t qla2x00_mem_alloc(struct qla_hw_data *, uint16_t, uint16_t,
	struct req_que **, struct rsp_que **);
void qla2x00_mem_free(struct qla_hw_data *);
static int qla2x00_allocate_sp_pool(struct qla_hw_data *);
static void qla2x00_free_sp_pool(struct qla_hw_data *);
void qla2x00_sp_free_dma(struct qla_hw_data *ha, srb_t *);
void qla2x00_sp_compl(struct qla_hw_data *, srb_t *);

/* -------------------------------------------------------------------------- */
static int qla2x00_alloc_queues(struct qla_hw_data *ha)
{
 	ha->req_q_map = kzalloc(sizeof(struct req_que *) * ha->max_req_queues,
		GFP_KERNEL);
	if (!ha->req_q_map) {
		qla_printk(KERN_WARNING, ha,
		"Unable to allocate memory for request queue ptrs\n");
		goto fail_req_map;
	}

 	ha->rsp_q_map = kzalloc(sizeof(struct rsp_que *) * ha->max_rsp_queues,
		GFP_KERNEL);
	if (!ha->rsp_q_map) {
		qla_printk(KERN_WARNING, ha,
		"Unable to allocate memory for response queue ptrs\n");
		goto fail_rsp_map;
	}
	set_bit(0, ha->rsp_qid_map);
	set_bit(0, ha->req_qid_map);
	return 1;

fail_rsp_map:
	kfree(ha->req_q_map);
	ha->req_q_map = NULL;
fail_req_map:
	return -ENOMEM;
}

static void qla2x00_free_req_que(struct qla_hw_data *ha, struct req_que *req)
{
	if (req && req->ring)
		dma_free_coherent(&ha->pdev->dev,
		(req->length + 1) * sizeof(request_t),
		req->ring, req->dma);

	kfree(req);
	req = NULL;
}
 
static void qla2x00_free_rsp_que(struct qla_hw_data *ha, struct rsp_que *rsp)
{
 	if (rsp && rsp->ring)
 		dma_free_coherent(&ha->pdev->dev,
 		(rsp->length + 1) * sizeof(response_t),
 		rsp->ring, rsp->dma);
 
 	kfree(rsp);
 	rsp = NULL;
}
 
static void qla2x00_free_queues(struct qla_hw_data *ha)
{
	struct req_que *req;
	struct rsp_que *rsp;
	int cnt;

 	for (cnt = 0; cnt < ha->max_req_queues; cnt++) {
		req = ha->req_q_map[cnt];
 		qla2x00_free_req_que(ha, req);
 	}
 	kfree(ha->req_q_map);
 	ha->req_q_map = NULL;
 
 	for (cnt = 0; cnt < ha->max_rsp_queues; cnt++) {
 		rsp = ha->rsp_q_map[cnt];
 		qla2x00_free_rsp_que(ha, rsp);
	}
	kfree(ha->rsp_q_map);
	ha->rsp_q_map = NULL;
}
  
static int qla25xx_setup_cpuaffinity(struct scsi_qla_host *vha)
{
 	uint16_t options = 0;
 	int ques, req, ret;
 	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *vp;

	if (!(ha->fw_attributes & BIT_6)) {
		qla_printk(KERN_INFO, ha,
			"Firmware is not multi-queue capable\n");
		goto fail;
	}

 	/* create a request queue for IO */
 	options |= BIT_7;
 	req = qla25xx_create_req_que(ha, options, 0, 0, -1,
 		QLA_DEFAULT_QUE_QOS);
 	if (!req) {
 		qla_printk(KERN_WARNING, ha,
 			"Can't create request queue\n");
 		goto fail;
 	}
 	vha->req = ha->req_q_map[req];
	/* set boothosts to use proper request queue as well */
	list_for_each_entry(vp, &ha->vp_list, list) {
		if (vp->vp_idx) {
			vp->req = vha->req;
		}
	}
 	options |= BIT_1;
 	for (ques = 1; ques < ha->max_rsp_queues; ques++) {
 		ret = qla25xx_create_rsp_que(ha, options, 0, 0, req);
 		if (!ret) {
 			qla_printk(KERN_WARNING, ha,
 				"Response Queue create failed\n");
 			goto fail2;
 		}
 	}

 	return 0;
fail2:
 	qla25xx_delete_queues(vha);
fail:
 	ha->mqenable = 0;
	kfree(ha->req_q_map);
	kfree(ha->rsp_q_map);
	ha->max_req_queues = ha->max_rsp_queues = 1;
 	return 1;
}

static void qla25xx_register_ioqueues(struct scsi_qla_host *vha)
{
 	struct qla_hw_data *ha = vha->hw;
	int i, ret, num_io_queue = 0;
	struct qla_msix_entry *qentry;
	struct vmklnx_scsi_ioqueue_info *mq_info;
	uint64_t cnt;
	struct rsp_que *rsp = NULL;

	num_io_queue = vmklnx_scsi_get_num_ioqueue(vha->host, 
	    ha->max_rsp_queues - 1);

	DEBUG2(printk("num_io_queue returned = %d\n", num_io_queue));

	if ((num_io_queue <= 0) || (num_io_queue > ha->max_rsp_queues - 1)) {
 		qla_printk(KERN_WARNING, ha,
 			"num_io_queue too large! Using %d\n", 
			ha->max_rsp_queues - 1);
		num_io_queue = ha->max_rsp_queues - 1;
	}

	if (num_io_queue < ha->max_rsp_queues - 1) {
		/* Delete extra response queues */
		for (cnt = 1 + num_io_queue; cnt < ha->max_rsp_queues; cnt++) {
			rsp = ha->rsp_q_map[cnt];
			if (rsp) {
				ret = qla25xx_delete_rsp_que(vha, rsp);
				if (ret != QLA_SUCCESS) {
					qla_printk(KERN_WARNING, ha,
						"Couldn't delete rsp que %d\n",
						rsp->id);
				}
			}
		}
	}

	ha->max_rsp_queues = num_io_queue + 1;

 	DEBUG2(qla_printk(KERN_INFO, ha,
 		"CPU affinity mode enabled, no. of response"
 		" queues:%d, no. of request queues:%d\n",
 		ha->max_rsp_queues, ha->max_req_queues));

	mq_info = kzalloc(sizeof(struct vmklnx_scsi_ioqueue_info) * num_io_queue, GFP_KERNEL);
	memset(mq_info, 0, sizeof(struct vmklnx_scsi_ioqueue_info)* num_io_queue);

	for (cnt = 0, i = 2; cnt < num_io_queue; i++, cnt++) {
		qentry = &ha->msix_entries[i];
		mq_info[cnt].q_handle = (void*)cnt;
		mq_info[cnt].q_vector = qentry->vector;
		DEBUG2(printk("mq_info[%llx].q_handle = %llx\n", cnt, cnt));
		DEBUG2(printk("mq_info[%llx].q_vector = %x\n", cnt, qentry->vector));
	}

	vmklnx_scsi_register_ioqueue(vha->host, num_io_queue, mq_info);
	kfree(mq_info);

	ha->flags.cpu_affinity_enabled = 1;
}

int
qla2x00_set_info(char *buffer, int length, struct Scsi_Host *shost)
{
	char *tmp_str;
        scsi_qla_host_t *vha;

        if (length < 10 || strncmp("scsi-qla", buffer, 8))
                goto out;

        vha = shost_priv(shost);

        if (!strncmp("lip", buffer + 8, 3)) {
                qla_printk(KERN_INFO, vha->hw, "Scheduling LIP...\n");

                set_bit(LOOP_RESET_NEEDED, &vha->dpc_flags);
        } else if (!strncmp("enable-log", buffer + 8, 10)) {
                printk(KERN_INFO "scsi-qla%ld: Setting Extended Logging\n",
                    vha->host_no);
                ql2xextended_error_logging = 1;
        } else if (!strncmp("disable-log", buffer + 8, 11)) {
                printk(KERN_INFO "scsi-qla%ld: Clearing Extended Logging\n",
                    vha->host_no);
                ql2xextended_error_logging = 0;
        } else if (!strncmp("chip-reset", buffer + 8, 10)) {
                printk(KERN_INFO "scsi-qla%ld: User requested 0x8002\n",
                    vha->host_no);
		qla2x00_system_error(vha);
	}		

	if (ql2xextended_error_logging) {
		if ((tmp_str = strstr(qla2x00_version_str, "-debug")) == NULL)
			sprintf(qla2x00_version_str, "%s-debug", qla2x00_version_str);
	} else {
			if ((tmp_str = strstr(qla2x00_version_str, "-debug")) != NULL)
				*tmp_str = '\0';
	}

out:
        return (length);
}

static char *
qla2x00_pci_info_str(struct scsi_qla_host *vha, char *str)
{
	struct qla_hw_data *ha = vha->hw;
	static char *pci_bus_modes[] = {
		"33", "66", "100", "133",
	};
	uint16_t pci_bus;

	strcpy(str, "PCI");
	pci_bus = (ha->pci_attr & (BIT_9 | BIT_10)) >> 9;
	if (pci_bus) {
		strcat(str, "-X (");
		strcat(str, pci_bus_modes[pci_bus]);
	} else {
		pci_bus = (ha->pci_attr & BIT_8) >> 8;
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus]);
	}
	strcat(str, " MHz)");

	return (str);
}

static char *
qla24xx_pci_info_str(struct scsi_qla_host *vha, char *str)
{
	static char *pci_bus_modes[] = { "33", "66", "100", "133", };
	struct qla_hw_data *ha = vha->hw;
	uint32_t pci_bus;
	int pcie_reg;

	pcie_reg = pci_find_capability(ha->pdev, PCI_CAP_ID_EXP);
	if (pcie_reg) {
		char lwstr[6];
		uint16_t pcie_lstat, lspeed, lwidth;

		pcie_reg += 0x12;
		pci_read_config_word(ha->pdev, pcie_reg, &pcie_lstat);
		lspeed = pcie_lstat & (BIT_0 | BIT_1 | BIT_2 | BIT_3);
		lwidth = (pcie_lstat &
		    (BIT_4 | BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9)) >> 4;

		strcpy(str, "PCIe (");
		if (lspeed == 1)
			strcat(str, "2.5Gb/s ");
		else if (lspeed == 2)
			strcat(str, "5.0Gb/s ");
		else
			strcat(str, "<unknown> ");
		snprintf(lwstr, sizeof(lwstr), "x%d)", lwidth);
		strcat(str, lwstr);

		return str;
	}

	strcpy(str, "PCI");
	pci_bus = (ha->pci_attr & CSRX_PCIX_BUS_MODE_MASK) >> 8;
	if (pci_bus == 0 || pci_bus == 8) {
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus >> 3]);
	} else {
		strcat(str, "-X ");
		if (pci_bus & BIT_2)
			strcat(str, "Mode 2");
		else
			strcat(str, "Mode 1");
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus & ~BIT_2]);
	}
	strcat(str, " MHz)");

	return str;
}

static char *
qla2x00_fw_version_str(struct scsi_qla_host *vha, char *str)
{
	char un_str[10];
	struct qla_hw_data *ha = vha->hw;

	sprintf(str, "%d.%02d.%02d ", ha->fw_major_version,
	    ha->fw_minor_version,
	    ha->fw_subminor_version);

	if (ha->fw_attributes & BIT_9) {
		strcat(str, "FLX");
		return (str);
	}

	switch (ha->fw_attributes & 0xFF) {
	case 0x7:
		strcat(str, "EF");
		break;
	case 0x17:
		strcat(str, "TP");
		break;
	case 0x37:
		strcat(str, "IP");
		break;
	case 0x77:
		strcat(str, "VI");
		break;
	default:
		sprintf(un_str, "(%x)", ha->fw_attributes);
		strcat(str, un_str);
		break;
	}
	if (ha->fw_attributes & 0x100)
		strcat(str, "X");

	return (str);
}

static char *
qla24xx_fw_version_str(struct scsi_qla_host *vha, char *str)
{
	struct qla_hw_data *ha = vha->hw;
 	sprintf(str, "%d.%02d.%02d (%x)", ha->fw_major_version,
 	    ha->fw_minor_version, ha->fw_subminor_version, ha->fw_attributes);
	return str;
}

srb_t *
qla2x00_get_new_sp(scsi_qla_host_t *vha, fc_port_t *fcport,
    struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	srb_t *sp;
	struct qla_hw_data *ha = vha->hw;
	sp = mempool_alloc(ha->srb_mempool, GFP_ATOMIC);
	if (!sp)
		return sp;

	sp->fcport = fcport;
	sp->cmd = cmd;
	sp->flags = 0;
	CMD_SP(cmd) = (void *)sp;
	cmd->scsi_done = done;

	return sp;
}

static int
qla2x00_queuecommand(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmd->device));
	struct qla_hw_data *ha = vha->hw;
	srb_t *sp;
	int rval;

#if !defined(__VMKLNX__) /* pci_channel_offline not supported in vmklnx */
	if (unlikely(pci_channel_offline(ha->pdev))) {
		cmd->result = DID_REQUEUE << 16;
		goto qc_fail_command;
	}
#endif

	rval = fc_remote_port_chkready(rport);
	if (rval) {
		cmd->result = rval;
		goto qc_fail_command;
	}

	if (NULL == fcport) {
		DEBUG2(printk("scsi(%ld:%d:%d:%d): qcmd cmd %x\n",
			vha->host_no, cmd->device->channel, cmd->device->id, cmd->device->lun,
			cmd->cmnd[0]));
		cmd->result = DID_IMM_RETRY << 16;
		goto qc_fail_command;
	}

	/* Close window on fcport/rport state-transitioning. */
	if (fcport->drport) {
		cmd->result = DID_IMM_RETRY << 16;
		goto qc_fail_command;
	}

	if (atomic_read(&fcport->state) != FCS_ONLINE) {
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD ||
		    atomic_read(&vha->loop_state) == LOOP_DEAD) {
			cmd->result = DID_NO_CONNECT << 16;
			goto qc_fail_command;
		}
		goto qc_host_busy;
	}

	spin_unlock_irq(vha->host->host_lock);

	sp = qla2x00_get_new_sp(vha, fcport, cmd, done);
	if (!sp)
		goto qc_host_busy_lock;

	rval = qla2x00_start_scsi(sp);
	if (rval != QLA_SUCCESS)
		goto qc_host_busy_free_sp;

	spin_lock_irq(vha->host->host_lock);

	return 0;

qc_host_busy_free_sp:
	qla2x00_sp_free_dma(ha, sp);
	mempool_free(sp, ha->srb_mempool);

qc_host_busy_lock:
	spin_lock_irq(vha->host->host_lock);

qc_host_busy:
	return SCSI_MLQUEUE_HOST_BUSY;

qc_fail_command:
	done(cmd);

	return 0;
}


static int
qla24xx_queuecommand(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmd->device));
	struct qla_hw_data *ha = vha->hw;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);
	srb_t *sp;
	int rval;

#if !defined(__VMKLNX__)
	if (unlikely(pci_channel_offline(vha->pdev))) {
 		if (ha->pdev->error_state == pci_channel_io_frozen)
 			cmd->result = DID_REQUEUE << 16;
 		else
 			cmd->result = DID_NO_CONNECT << 16;
		goto qc24_fail_command;
	}
#endif

	rval = fc_remote_port_chkready(rport);
	if (rval) {
		cmd->result = rval;
		goto qc24_fail_command;
	}

	if (NULL == fcport) {
		DEBUG2(printk("scsi(%ld:%d:%d:%d): qcmd cmd %x\n",
			vha->host_no, cmd->device->channel, cmd->device->id, cmd->device->lun,
			cmd->cmnd[0]));
		cmd->result = DID_IMM_RETRY << 16;
		goto qc24_fail_command;
	}

	/* Close window on fcport/rport state-transitioning. */
	if (fcport->drport) {
		cmd->result = DID_IMM_RETRY << 16;
		goto qc24_fail_command;
	}

	if (atomic_read(&fcport->state) != FCS_ONLINE) {
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD ||
		    atomic_read(&base_vha->loop_state) == LOOP_DEAD) {
			cmd->result = DID_NO_CONNECT << 16;
			goto qc24_fail_command;
		}
		goto qc24_host_busy;
	}

	spin_unlock_irq(vha->host->host_lock);

	sp = qla2x00_get_new_sp(base_vha, fcport, cmd, done);
	if (!sp)
		goto qc24_host_busy_lock;

	rval = ha->isp_ops->start_scsi(sp);
	if (rval != QLA_SUCCESS)
		goto qc24_host_busy_free_sp;

	spin_lock_irq(vha->host->host_lock);

	return 0;

qc24_host_busy_free_sp:
	qla2x00_sp_free_dma(ha, sp);
	mempool_free(sp, ha->srb_mempool);

qc24_host_busy_lock:
	spin_lock_irq(vha->host->host_lock);

qc24_host_busy:
	return SCSI_MLQUEUE_HOST_BUSY;

qc24_fail_command:
	done(cmd);

	return 0;
}


/*
 * qla2x00_eh_wait_on_command
 *    Waits for the command to be returned by the Firmware for some
 *    max time.
 *
 * Input:
 *    ha = actual ha whose done queue will contain the command
 *	      returned by firmware.
 *    cmd = Scsi Command to wait on.
 *    flag = Abort/Reset(Bus or Device Reset)
 *
 * Return:
 *    Not Found : 0
 *    Found : 1
 */
int
qla2x00_eh_wait_on_command(struct scsi_cmnd *cmd)
{
#if defined(__VMKLNX__)
/*
 * The Maximum wait has been reduced to 2 sec to
 * ensure that the wait doesn't cause too much delay
 * during lun resets, which is extensively used in vmware
 * clustering environments.
 */
#define ABORT_POLLING_PERIOD	200
#define ABORT_WAIT_ITER			10
#else
#define ABORT_POLLING_PERIOD	1000
#define ABORT_WAIT_ITER		((10 * 1000) / (ABORT_POLLING_PERIOD))
#endif
	unsigned long wait_iter = ABORT_WAIT_ITER;
	int ret = QLA_SUCCESS;

	while (CMD_SP(cmd)) {
		msleep(ABORT_POLLING_PERIOD);

		if (!(--wait_iter))
			break;
	}
	if (CMD_SP(cmd))
		ret = QLA_FUNCTION_FAILED;

	return ret;
}

/*
 * qla2x00_wait_for_hba_online
 *    Wait till the HBA is online after going through
 *    <= MAX_RETRIES_OF_ISP_ABORT  or
 *    finally HBA is disabled ie marked offline
 *
 * Input:
 *     ha - pointer to host adapter structure
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 * Return:
 *    Success (Adapter is online) : 0
 *    Failed  (Adapter is offline/disabled) : 1
 */
int
qla2x00_wait_for_hba_online(scsi_qla_host_t *vha)
{
	int		return_status;
#if !defined(__VMKLNX__)
	unsigned long	wait_online;
	struct qla_hw_data *ha = vha->hw;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);

    /*   
	 * We can't use this code in ESX because it waits too long.
	 * This will block upper layer error handling call.
	 */
	wait_online = jiffies + (MAX_LOOP_TIMEOUT * HZ);
	while (((test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) ||
	    test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &base_vha->dpc_flags) ||
	    base_vha->dpc_active) && time_before(jiffies, wait_online)) {

		msleep(1000);
	}
	if (base_vha->flags.online)
		return_status = QLA_SUCCESS;
	else
		return_status = QLA_FUNCTION_FAILED;
#else
	/* Always return good status */
	return_status = QLA_SUCCESS;
#endif

	DEBUG2(printk("%s return_status=%d\n",__func__,return_status));

	return (return_status);
}

 /*
 * qla2x00_wait_for_chip_reset
 *    Wait till the HBA chip is reset.
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 * Return:
 *    Success (Chip reset is done) : 0
 *    Failed  (Chip reset not completed within max loop timout ) : 1
 */
int
qla2x00_wait_for_chip_reset(scsi_qla_host_t *vha)
{
	int status;
	unsigned long wait_reset;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	wait_reset = jiffies + (MAX_LOOP_TIMEOUT * HZ);
	while (((test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) ||
	    test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &base_vha->dpc_flags) ||
	    base_vha->dpc_active) && time_before(jiffies, wait_reset)) {

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ);

		if (!test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags) &&
		    ha->flags.chip_reset_done)
			break;
	}

	if (ha->flags.chip_reset_done)
		status = QLA_SUCCESS;
	else
		status = QLA_FUNCTION_FAILED;
	DEBUG2(printk("%s status=%d\n", __func__, status));

	return status;
}

/*
 * qla2x00_wait_for_loop_ready
 *    Wait for MAX_LOOP_TIMEOUT(5 min) value for loop
 *    to be in LOOP_READY state.
 * Input:
 *     ha - pointer to host adapter structure
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 *
 * Return:
 *    Success (LOOP_READY) : 0
 *    Failed  (LOOP_NOT_READY) : 1
 */
static inline int
qla2x00_wait_for_loop_ready(scsi_qla_host_t *vha)
{
	int 	 return_status = QLA_SUCCESS;
	unsigned long loop_timeout ;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);
	/* wait for 5 min at the max for loop to be ready */
	loop_timeout = jiffies + (MAX_LOOP_TIMEOUT * HZ);

	while ((!atomic_read(&base_vha->loop_down_timer) &&
	    atomic_read(&base_vha->loop_state) == LOOP_DOWN) ||
	    atomic_read(&base_vha->loop_state) != LOOP_READY) {
		if (atomic_read(&base_vha->loop_state) == LOOP_DEAD) {
			return_status = QLA_FUNCTION_FAILED;
			break;
		}
		msleep(1000);
		if (time_after_eq(jiffies, loop_timeout)) {
			return_status = QLA_FUNCTION_FAILED;
			break;
		}
	}
	return (return_status);
}

void
qla2x00_abort_fcport_cmds(fc_port_t *fcport)
{
	int cnt;
	unsigned long flags;
	srb_t *sp;
	scsi_qla_host_t *vha = fcport->vha;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	req = vha->req;
	for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		sp = req->outstanding_cmds[cnt];
		if (!sp)
			continue;
		if (sp->fcport != fcport)
			continue;

		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		if (ha->isp_ops->abort_command(sp)) {
			DEBUG2(qla_printk(KERN_WARNING, ha,
			"Abort failed --  %lx\n",
			sp->cmd->serial_number));
		} else {
			if (qla2x00_eh_wait_on_command(sp->cmd) !=
				QLA_SUCCESS)
				DEBUG2(qla_printk(KERN_WARNING, ha,
				"Abort failed while waiting --  %lx\n",
				sp->cmd->serial_number));
		}
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static void
qla2x00_block_error_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host *shost = cmnd->device->host;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmnd->device));
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	while (rport->port_state == FC_PORTSTATE_BLOCKED) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		msleep(1000);
		spin_lock_irqsave(shost->host_lock, flags);
	}
	spin_unlock_irqrestore(shost->host_lock, flags);
	return;
}

/**************************************************************************
* qla2xxx_eh_abort
*
* Description:
*    The abort function will abort the specified command.
*
* Input:
*    cmd = Linux SCSI command packet to be aborted.
*
* Returns:
*    Either SUCCESS or FAILED.
*
* Note:
*    Only return FAILED if command not returned by firmware.
**************************************************************************/
static int
qla2xxx_eh_abort(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	srb_t *sp;
	int ret, i;
	unsigned int id, lun;
	unsigned long serial;
	unsigned long flags;
	int wait = 0;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = vha->req;

	qla2x00_block_error_handler(cmd);

	if (!CMD_SP(cmd))
#if !defined(__VMKLNX__)
		return SUCCESS;
#else
		return FAILED;
#endif

	ret = SUCCESS;

	id = cmd->device->id;
	lun = cmd->device->lun;
	serial = cmd->serial_number;

	/* Check active list for command command. */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (i = 1; i < MAX_OUTSTANDING_COMMANDS; i++) {
		sp = req->outstanding_cmds[i];

		if (sp == NULL)
			continue;

		if (sp->cmd != cmd)
			continue;

		DEBUG2(printk("%s(%ld): aborting sp %p from RISC. pid=%ld.\n",
	   	 __func__, vha->host_no, sp, serial));
		DEBUG3(qla2x00_print_scsi_cmd(cmd));

		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		if (ha->isp_ops->abort_command(sp)) {
			DEBUG2(printk("%s(%ld): abort_command "
		    	"mbx failed.\n", __func__, vha->host_no));
			ret = FAILED;
		} else {
			DEBUG3(printk("%s(%ld): abort_command "
		    	"mbx success.\n", __func__, vha->host_no));
			wait = 1;
		}
		spin_lock_irqsave(&ha->hardware_lock, flags);
		break;
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	/* Wait for the command to be returned. */
	if (wait) {
		if (qla2x00_eh_wait_on_command(cmd) != QLA_SUCCESS) {
			qla_printk(KERN_ERR, ha,
			    "scsi(%ld:%d:%d): Abort handler timed out -- %ld "
			    "%x.\n", vha->host_no, id, lun, serial, ret);
			ret = FAILED;
		}
	}

	if(ret == SUCCESS)
		qla_printk(KERN_INFO, ha,
			"scsi(%ld:%d:%d): Abort command succeeded -- %d %ld.\n",
			vha->host_no, id, lun, wait, serial);

	return ret;
}

/**************************************************************************
* qla2x00_eh_wait_for_pending_device_commands
*
* Description:
*    Waits for all the commands to come back from the specified target.
*
* Input:
*    ha - pointer to scsi_qla_host structure.
*    is_target - is it waiting for commands from target or lun
*    id  - target id
     lun - lun number
*
* Returns:
*    Either SUCCESS or FAILED.
*
* Note:
**************************************************************************/
static int
qla2x00_eh_wait_for_pending_device_commands(scsi_qla_host_t *vha, int is_target,
	unsigned int id, unsigned int lun)
{
	int	cnt;
	int	status;
	srb_t		*sp;
	struct scsi_cmnd *cmd;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	status = 0;

	/*
	 * Waiting for all commands for the designated target in the active
	 * array
	 */
	req = vha->req;
	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		int match;
		sp = req->outstanding_cmds[cnt];

		if (!sp)
			continue;

		cmd = sp->cmd;
		DEBUG2(printk("vha->host_no=%ld sp=0x%p cmd=0x%p id=0x%x lun=0x%x "
				"serial_no=0x%x cdb=0x%02x%02x%02x%02x nport_handle=0x%x "
				"vp_idx=0x%x io-index=0x%x", sp->fcport->vha->host_no,
				sp, cmd, cmd->device->id, cmd->device->lun,
				(unsigned int)cmd->serial_number, cmd->cmnd[0], cmd->cmnd[1],
				cmd->cmnd[2], cmd->cmnd[3], sp->fcport->loop_id,
				sp->fcport->vp_idx, cnt));

		if (vha->vp_idx != ((scsi_qla_host_t *)
				shost_priv(cmd->device->host))->vp_idx)
			continue;

		match = is_target ? (cmd->device->id == id) :
			((cmd->device->id == id) && (cmd->device->lun == lun));
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		if (match) {
			if (qla2x00_eh_wait_on_command(cmd) != QLA_SUCCESS) {
				DEBUG2(printk("scsi(%ld:%d:%d:%d) dev cmd 0x%x timed out\n",
						vha->host_no, cmd->device->channel, cmd->device->id,
						cmd->device->lun, cmd->cmnd[0]));
				return (1);
			}
		}
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}

	spin_unlock_irqrestore(&ha->hardware_lock, flags);
	return (status);
}


/**************************************************************************
* qla2xxx_eh_device_reset
*
* Description:
*    The device reset function will reset the target and abort any
*    executing commands.
*
*    NOTE: The use of SP is undefined within this context.  Do *NOT*
*          attempt to use this value, even if you determine it is
*          non-null.
*
* Input:
*    cmd = Linux SCSI command packet of the command that cause the
*          bus device reset.
*
* Returns:
*    SUCCESS/FAILURE (defined as macro in scsi.h).
*
**************************************************************************/
static int
qla2xxx_eh_device_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	int ret = FAILED;
	unsigned int id, lun;
	unsigned long serial;

	qla2x00_block_error_handler(cmd);

	id = cmd->device->id;
	lun = cmd->device->lun;
	serial = cmd->serial_number;

	if (!fcport)
		return ret;

	qla_printk(KERN_INFO, ha,
	    "scsi(%ld:%d:%d): DEVICE RESET ISSUED.\n", vha->host_no, id, lun);
	DEBUG2(qla_printk(KERN_INFO, ha, "cmd=0x%p serial_no=0x%x "
			"cdb=0x%02x%02x%02x%02x nport_handle=0x%x vp_idx=0x%x "
			"fcport->ha=%ld", cmd, (unsigned int)cmd->serial_number,
			cmd->cmnd[0], cmd->cmnd[1], cmd->cmnd[2], cmd->cmnd[3],
			fcport->loop_id, fcport->vp_idx, vha->host_no));

	if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS)
		goto eh_dev_reset_done;

	if (qla2x00_wait_for_loop_ready(vha) == QLA_SUCCESS) {
#if defined(__VMKLNX__)
		/* try to reset just the LUN */
		if (cmd->vmkflags & VMK_FLAGS_USE_LUNRESET) {
			if (qla2x00_lun_reset(vha, fcport, lun,
			    0) == 0) {
				ret = SUCCESS;
			}
		} else
#endif
		if (qla2x00_device_reset(vha, fcport,
		    0) == 0) 
			ret = SUCCESS;

	} else {
		DEBUG2(printk(KERN_INFO
		    "%s failed: loop not ready\n",__func__));
	}

	if (ret == FAILED) {
		DEBUG3(printk("%s(%ld): device reset failed\n",
		    __func__, vha->host_no));
		qla_printk(KERN_INFO, ha, "%s: device reset failed\n",
		    __func__);

		goto eh_dev_reset_done;
	}

	/* Flush outstanding commands. */
#if defined(__VMKLNX__)
	if (cmd->vmkflags & VMK_FLAGS_USE_LUNRESET) {
		if (qla2x00_eh_wait_for_pending_device_commands(vha, 0, id, lun))
			ret = FAILED;
	} else
#endif
	if (qla2x00_eh_wait_for_pending_device_commands(vha, 1, id, 0))
		ret = FAILED;
	if (ret == FAILED) {
		DEBUG3(printk("%s(%ld): failed while waiting for commands\n",
		    __func__, vha->host_no));
		qla_printk(KERN_INFO, ha,
		    "%s: failed while waiting for commands\n", __func__);
	} else
		qla_printk(KERN_INFO, ha,
		    "scsi(%ld:%d:%d): DEVICE RESET SUCCEEDED.\n", vha->host_no,
		    id, lun);
 eh_dev_reset_done:
	return ret;
}

/**************************************************************************
* qla2x00_eh_wait_for_pending_commands
*
* Description:
*    Waits for all the commands to come back from the specified host.
*
* Input:
*    ha - pointer to scsi_qla_host structure.
*
* Returns:
*    1 : SUCCESS
*    0 : FAILED
*
* Note:
**************************************************************************/
int
qla2x00_eh_wait_for_pending_commands(scsi_qla_host_t *vha)
{
	int	cnt;
	int	status;
	struct scsi_cmnd *cmd;
	srb_t		*sp;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	status = QLA_SUCCESS;
	/*
	 * Waiting for all commands for the designated target in the active
	 * array
	 */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	req = vha->req;
	for (cnt = 1; status == QLA_SUCCESS && cnt < MAX_OUTSTANDING_COMMANDS;
    		cnt++) {
		sp = req->outstanding_cmds[cnt];

		if (!sp)
			continue;

		cmd = sp->cmd;
		if (vha->vp_idx != 
				((scsi_qla_host_t *)shost_priv(cmd->device->host))->vp_idx)
			continue;

		/*
		 * Even if we couldn't complete this cmd, go ahead
		 * and wait for other commands in the queue to complete.
		 */
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		status = qla2x00_eh_wait_on_command(cmd);
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	return status;
}


/**************************************************************************
* qla2xxx_eh_bus_reset
*
* Description:
*    The bus reset function will reset the bus and abort any executing
*    commands.
*
* Input:
*    cmd = Linux SCSI command packet of the command that cause the
*          bus reset.
*
* Returns:
*    SUCCESS/FAILURE (defined as macro in scsi.h).
*
**************************************************************************/
static int
qla2xxx_eh_bus_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	int ret = FAILED;
	unsigned int id, lun;
	unsigned long serial;

	qla2x00_block_error_handler(cmd);

	id = cmd->device->id;
	lun = cmd->device->lun;
	serial = cmd->serial_number;

	if (!fcport)
		return ret;

	qla_printk(KERN_INFO, ha,
	    "scsi(%ld:%d:%d): BUS RESET ISSUED.\n", vha->host_no, id, lun);

	if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS) {
		DEBUG2(printk("%s failed:board disabled\n",__func__));
		goto eh_bus_reset_done;
	}

	if (qla2x00_wait_for_loop_ready(vha) == QLA_SUCCESS) {
		if (qla2x00_loop_reset(vha) == QLA_SUCCESS)
			ret = SUCCESS;
	}
	if (ret == FAILED)
		goto eh_bus_reset_done;

	/* Flush outstanding commands. */
	if (qla2x00_eh_wait_for_pending_commands(vha))
		ret = FAILED;

eh_bus_reset_done:
	qla_printk(KERN_INFO, ha, "%s: reset %s\n", __func__,
	    (ret == FAILED) ? "failed" : "succeded");

	return ret;
}

/**************************************************************************
* qla2xxx_eh_host_reset
*
* Description:
*    The reset function will reset the Adapter.
*
* Input:
*      cmd = Linux SCSI command packet of the command that cause the
*            adapter reset.
*
* Returns:
*      Either SUCCESS or FAILED.
*
* Note:
**************************************************************************/
static int
qla2xxx_eh_host_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	int ret = FAILED;
	unsigned int id, lun;
	unsigned long serial;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	qla2x00_block_error_handler(cmd);

	id = cmd->device->id;
	lun = cmd->device->lun;
	serial = cmd->serial_number;

	if (!fcport)
		return ret;

	qla_printk(KERN_INFO, ha,
	    "scsi(%ld:%d:%d): ADAPTER RESET ISSUED.\n", vha->host_no, id, lun);

	if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS)
		goto eh_host_reset_lock;

	/*
	 * Fixme-may be dpc thread is active and processing
	 * loop_resync,so wait a while for it to
	 * be completed and then issue big hammer.Otherwise
	 * it may cause I/O failure as big hammer marks the
	 * devices as lost kicking of the port_down_timer
	 * while dpc is stuck for the mailbox to complete.
	 */
	qla2x00_wait_for_loop_ready(vha);
	if (vha != base_vha) {
		if (qla2x00_vp_abort_isp(vha))
			goto eh_host_reset_lock;
	} else {
		set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
		if (qla2x00_abort_isp(base_vha)) {
			clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
				/* failed. schedule dpc to try */
				set_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags);

				if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS)
					goto eh_host_reset_lock;
		}
		clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
	}

	/* Waiting for our command in done_queue to be returned to OS.*/
	if (!qla2x00_eh_wait_for_pending_commands(vha))
		ret = SUCCESS;

eh_host_reset_lock:
	qla_printk(KERN_INFO, ha, "%s: reset %s\n", __func__,
	    (ret == FAILED) ? "failed" : "succeded");

	return ret;
}

/*
* qla2x00_loop_reset
*      Issue loop reset.
*
* Input:
*      ha = adapter block pointer.
*
* Returns:
*      0 = success
*/
int
qla2x00_loop_reset(scsi_qla_host_t *vha)
{
	int ret;
	struct fc_port *fcport;
	struct qla_hw_data *ha = vha->hw;

	if (ha->flags.enable_lip_full_login && !vha->vp_idx &&
	    !IS_QLA81XX(ha)) {
		ret = qla2x00_full_login_lip(vha);
		if (ret != QLA_SUCCESS) {
			DEBUG2_3(printk("%s(%ld): failed: "
			    "full_login_lip=%d.\n", __func__, vha->host_no,
			    ret));
		} else
			qla2x00_wait_for_loop_ready(vha);
	}

	if (ha->flags.enable_lip_reset && !vha->vp_idx) {
		ret = qla2x00_lip_reset(vha);
		if (ret != QLA_SUCCESS) {
			DEBUG2_3(printk("%s(%ld): bus_reset failed: "
			    "lip_reset=%d.\n", __func__, vha->host_no, ret));
		} else
			qla2x00_wait_for_loop_ready(vha);
	}

	if (ha->flags.enable_target_reset) {
		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type != FCT_TARGET)
				continue;

			ret = qla2x00_device_reset(vha, fcport, 0);
			if (ret != QLA_SUCCESS) {
				DEBUG2_3(printk("%s(%ld): bus_reset failed: "
				    "target_reset=%d d_id=%x.\n", __func__,
				    vha->host_no, ret, fcport->d_id.b24));
			}
		}
	}

	return QLA_SUCCESS;
}

/*
 * qla2x00_device_reset
 *	Issue bus device reset message to the target.
 *
 * Input:
 *	ha = adapter block pointer.
 *	t = SCSI ID.
 *	TARGET_QUEUE_LOCK must be released.
 *	ADAPTER_STATE_LOCK must be released.
 *
 * Context:
 *	Kernel context.
 */
static int
qla2x00_device_reset(scsi_qla_host_t *vha, fc_port_t *reset_fcport, int tag)
{
	/* Abort Target command will clear Reservation */
	return vha->hw->isp_ops->abort_target(reset_fcport, tag);
}

void
qla2x00_abort_all_cmds(scsi_qla_host_t *vha, int res)
{
	int que, cnt;
	unsigned long flags;
	srb_t *sp;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (que = 0; que < ha->max_req_queues; que++) {
		req = ha->req_q_map[que];
		if (!req)
			continue;
		for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
			sp = req->outstanding_cmds[cnt];
			if (sp && sp->fcport->vha == vha) {
				req->outstanding_cmds[cnt] = NULL;
				sp->cmd->result = res;
				qla2x00_sp_compl(ha, sp);
			}
		}
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static int
qla2xxx_slave_alloc(struct scsi_device *sdev)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));

	if (!rport) {
		DEBUG2(printk("scsi(%d:%d:%d:%d): qla2xxx_slave_alloc null rport sdev:%p\n",
			sdev->host->host_no, sdev->channel, sdev->id, sdev->lun, sdev));
		return -ENXIO;
	}

	if (fc_remote_port_chkready(rport)) {
		DEBUG2(printk("scsi(%d:%d:%d:%d): qla2xxx_slave_alloc rport %x "
			"state %x roles %x flags %x sdev:%p\n",
			sdev->host->host_no, sdev->channel, sdev->id, sdev->lun,
			rport->port_id, rport->port_state, rport->roles, rport->flags, sdev));
		return -ENXIO;
	}
  
	sdev->hostdata = *(fc_port_t **)rport->dd_data;
	DEBUG2(printk("scsi(%d:%d:%d:%d): qla2xxx_slave_alloc rport %x hostdata %p sdev:%p\n",
		sdev->host->host_no, sdev->channel, sdev->id, sdev->lun,
		rport->port_id, sdev->hostdata, sdev));

	return 0;
}

static int
qla2xxx_slave_configure(struct scsi_device *sdev)
{
	scsi_qla_host_t *vha = shost_priv(sdev->host);
	struct qla_hw_data *ha = vha->hw;
	struct fc_rport *rport = starget_to_rport(sdev->sdev_target);
	struct req_que *req = vha->req;
	fc_port_t *fcport = *(fc_port_t **)rport->dd_data;

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, req->max_q_depth);
	else
		scsi_deactivate_tcq(sdev, req->max_q_depth);

	rport->dev_loss_tmo = ha->port_down_retry_count + 5;

	/* Check whether TAPE device */
	if (sdev->type == TYPE_TAPE)
		fcport->flags |= FCF_TAPE_PRESENT;

	return 0;
}

static void
qla2xxx_slave_destroy(struct scsi_device *sdev)
{
	struct fc_rport *rport = starget_to_rport(sdev->sdev_target);

	DEBUG2(printk("scsi(%d:%d:%d:%d): qla2xxx_slave_destroy: rport %x hostdata %p sdev %p\n",
		sdev->host->host_no, sdev->channel, sdev->id, sdev->lun,
		rport->port_id, sdev->hostdata, sdev));
	sdev->hostdata = NULL;
}

static int
qla2x00_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	return sdev->queue_depth;
}

static int
qla2x00_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}

/**
 * qla2x00_config_dma_addressing() - Configure OS DMA addressing method.
 * @ha: HA context
 *
 * At exit, the @ha's flags.enable_64bit_addressing set to indicated
 * supported addressing method.
 */
static void
qla2x00_config_dma_addressing(struct qla_hw_data *ha)
{
	/* Assume a 32bit DMA mask. */
	ha->flags.enable_64bit_addressing = 0;

	if (!dma_set_mask(&ha->pdev->dev, DMA_64BIT_MASK)) {
		/* Any upper-dword bits set? */
		if (MSD(dma_get_required_mask(&ha->pdev->dev)) &&
		    !pci_set_consistent_dma_mask(ha->pdev, DMA_64BIT_MASK)) {
			/* Ok, a 64bit DMA mask is applicable. */
			ha->flags.enable_64bit_addressing = 1;
			ha->isp_ops->calc_req_entries = qla2x00_calc_iocbs_64;
			ha->isp_ops->build_iocbs = qla2x00_build_scsi_iocbs_64;
			return;
		}
	}

	dma_set_mask(&ha->pdev->dev, DMA_32BIT_MASK);
	pci_set_consistent_dma_mask(ha->pdev, DMA_32BIT_MASK);
}

static void
qla2x00_enable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 1;
	/* enable risc and host interrupts */
	WRT_REG_WORD(&reg->ictrl, ICR_EN_INT | ICR_EN_RISC);
	RD_REG_WORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

}

static void
qla2x00_disable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 0;
	/* disable risc and host interrupts */
	WRT_REG_WORD(&reg->ictrl, 0);
	RD_REG_WORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static void
qla24xx_enable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 1;
	WRT_REG_DWORD(&reg->ictrl, ICRX_EN_RISC_INT);
	RD_REG_DWORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static void
qla24xx_disable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	if (IS_NOPOLLING_TYPE(ha))
		return;
	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 0;
	WRT_REG_DWORD(&reg->ictrl, 0);
	RD_REG_DWORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static struct isp_operations qla2100_isp_ops = {
	.pci_config		= qla2100_pci_config,
	.reset_chip		= qla2x00_reset_chip,
	.chip_diag		= qla2x00_chip_diag,
	.config_rings		= qla2x00_config_rings,
	.reset_adapter		= qla2x00_reset_adapter,
	.nvram_config		= qla2x00_nvram_config,
	.update_fw_options	= qla2x00_update_fw_options,
	.load_risc		= qla2x00_load_risc,
	.pci_info_str		= qla2x00_pci_info_str,
	.fw_version_str		= qla2x00_fw_version_str,
	.intr_handler		= qla2100_intr_handler,
	.enable_intrs		= qla2x00_enable_intrs,
	.disable_intrs		= qla2x00_disable_intrs,
	.start_scsi             = qla2x00_start_scsi,
	.abort_command		= qla2x00_abort_command,
	.abort_target		= qla2x00_abort_target,
	.fabric_login		= qla2x00_login_fabric,
	.fabric_logout		= qla2x00_fabric_logout,
	.calc_req_entries	= qla2x00_calc_iocbs_32,
	.build_iocbs		= qla2x00_build_scsi_iocbs_32,
	.prep_ms_iocb		= qla2x00_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla2x00_prep_ms_fdmi_iocb,
	.read_nvram		= qla2x00_read_nvram_data,
	.write_nvram		= qla2x00_write_nvram_data,
	.fw_dump		= qla2100_fw_dump,
	.beacon_on		= NULL,
	.beacon_off		= NULL,
	.beacon_blink		= NULL,
	.read_optrom		= qla2x00_read_optrom_data,
	.write_optrom		= qla2x00_write_optrom_data,
	.get_flash_version	= qla2x00_get_flash_version,
};

static struct isp_operations qla2300_isp_ops = {
	.pci_config		= qla2300_pci_config,
	.reset_chip		= qla2x00_reset_chip,
	.chip_diag		= qla2x00_chip_diag,
	.config_rings		= qla2x00_config_rings,
	.reset_adapter		= qla2x00_reset_adapter,
	.nvram_config		= qla2x00_nvram_config,
	.update_fw_options	= qla2x00_update_fw_options,
	.load_risc		= qla2x00_load_risc,
	.pci_info_str		= qla2x00_pci_info_str,
	.fw_version_str		= qla2x00_fw_version_str,
	.intr_handler		= qla2300_intr_handler,
	.enable_intrs		= qla2x00_enable_intrs,
	.disable_intrs		= qla2x00_disable_intrs,
	.start_scsi             = qla2x00_start_scsi,
	.abort_command		= qla2x00_abort_command,
	.abort_target		= qla2x00_abort_target,
	.fabric_login		= qla2x00_login_fabric,
	.fabric_logout		= qla2x00_fabric_logout,
	.calc_req_entries	= qla2x00_calc_iocbs_32,
	.build_iocbs		= qla2x00_build_scsi_iocbs_32,
	.prep_ms_iocb		= qla2x00_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla2x00_prep_ms_fdmi_iocb,
	.read_nvram		= qla2x00_read_nvram_data,
	.write_nvram		= qla2x00_write_nvram_data,
	.fw_dump		= qla2300_fw_dump,
	.beacon_on		= qla2x00_beacon_on,
	.beacon_off		= qla2x00_beacon_off,
	.beacon_blink		= qla2x00_beacon_blink,
	.read_optrom		= qla2x00_read_optrom_data,
	.write_optrom		= qla2x00_write_optrom_data,
	.get_flash_version	= qla2x00_get_flash_version,
};

static struct isp_operations qla24xx_isp_ops = {
	.pci_config		= qla24xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla24xx_nvram_config,
	.update_fw_options	= qla24xx_update_fw_options,
	.load_risc		= qla24xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.start_scsi             = qla24xx_start_scsi,
	.abort_command		= qla24xx_abort_command,
	.abort_target		= qla24xx_abort_target,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= qla24xx_read_nvram_data,
	.write_nvram		= qla24xx_write_nvram_data,
	.fw_dump		= qla24xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla24xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
};

static struct isp_operations qla25xx_isp_ops = {
	.pci_config		= qla25xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla24xx_nvram_config,
	.update_fw_options	= qla24xx_update_fw_options,
	.load_risc		= qla24xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.start_scsi             = qla24xx_start_scsi,
	.abort_command		= qla24xx_abort_command,
	.abort_target		= qla24xx_abort_target,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= qla25xx_read_nvram_data,
	.write_nvram		= qla25xx_write_nvram_data,
	.fw_dump		= qla25xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla25xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
};

static struct isp_operations qla81xx_isp_ops = {
	.pci_config		= qla25xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla81xx_nvram_config,
	.update_fw_options	= qla81xx_update_fw_options,
	.load_risc		= qla81xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.start_scsi		= qla24xx_start_scsi,
	.abort_command		= qla24xx_abort_command,
	.abort_target		= qla24xx_abort_target,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram			= NULL,
	.write_nvram		= NULL,
	.fw_dump		= qla81xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla25xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
};

static inline void
qla2x00_set_isp_flags(struct qla_hw_data *ha)
{
	ha->device_type = DT_EXTENDED_IDS;
	switch (ha->pdev->device) {
	case PCI_DEVICE_ID_QLOGIC_ISP2100:
		ha->device_type |= DT_ISP2100;
		ha->device_type &= ~DT_EXTENDED_IDS;
		ha->fw_srisc_address = RISC_START_ADDRESS_2100;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2200:
		ha->device_type |= DT_ISP2200;
		ha->device_type &= ~DT_EXTENDED_IDS;
		ha->fw_srisc_address = RISC_START_ADDRESS_2100;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2300:
		ha->device_type |= DT_ISP2300;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2312:
		ha->device_type |= DT_ISP2312;
		ha->device_type |= DT_ZIO_SUPPORTED;
		if ((ha->pdev->subsystem_vendor == 0x103C &&
		    (ha->pdev->subsystem_device == 0x12BA ||
		    ha->pdev->subsystem_device == 0x12C2 ||
		    ha->pdev->subsystem_device == 0x12C7 ||
		    ha->pdev->subsystem_device == 0x12C9)) ||
		    (ha->pdev->subsystem_device == 0x0131 &&
		    ha->pdev->subsystem_vendor == 0x1077))
			ha->device_type |= DT_OEM_002;				
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2322:
		ha->device_type |= DT_ISP2322;
		ha->device_type |= DT_ZIO_SUPPORTED;
		if (ha->pdev->subsystem_vendor == 0x1028 &&
		    ha->pdev->subsystem_device == 0x0170)
			ha->device_type |= DT_OEM_001;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP6312:
		ha->device_type |= DT_ISP6312;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP6322:
		ha->device_type |= DT_ISP6322;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2422:
		ha->device_type |= DT_ISP2422;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2432:
		ha->device_type |= DT_ISP2432;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP5422:
		ha->device_type |= DT_ISP5422;
		ha->device_type |= DT_FWI2;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP5432:
		ha->device_type |= DT_ISP5432;
		ha->device_type |= DT_FWI2;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2532:
		ha->device_type |= DT_ISP2532;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP8432:
		ha->device_type |= DT_ISP8432;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
 	case PCI_DEVICE_ID_QLOGIC_ISP8001:
 		ha->device_type |= DT_ISP8001;
 		ha->device_type |= DT_ZIO_SUPPORTED;
 		ha->device_type |= DT_FWI2;
 		ha->device_type |= DT_IIDMA;
 		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
 		break;
	}
 
 	/* Get adapter physical port no from interrupt pin register. */
 	pci_read_config_byte(ha->pdev, PCI_INTERRUPT_PIN, &ha->port_no);
 	if (ha->port_no & 1)
 		ha->flags.port0 = 1;
 	else
 		ha->flags.port0 = 0;
}

static int
qla2x00_iospace_config(struct qla_hw_data *ha)
{
	unsigned long	pio, pio_len, pio_flags;
	unsigned long	mmio, mmio_len, mmio_flags;
	uint16_t msix = 0;
 	int cpus;

	/* We only need PIO for Flash operations on ISP2312 v2 chips. */
	pio = pci_resource_start(ha->pdev, 0);
	pio_len = pci_resource_len(ha->pdev, 0);
	pio_flags = pci_resource_flags(ha->pdev, 0);
	if (pio_flags & IORESOURCE_IO) {
		if (pio_len < MIN_IOBASE_LEN) {
			qla_printk(KERN_WARNING, ha,
			    "Invalid PCI I/O region size (%s)...\n",
				pci_name(ha->pdev));
			pio = 0;
		}
	} else {
		qla_printk(KERN_WARNING, ha,
		    "region #0 not a PIO resource (%s)...\n",
		    pci_name(ha->pdev));
		pio = 0;
	}

	/* Use MMIO operations for all accesses. */
	mmio = pci_resource_start(ha->pdev, 1);
	mmio_len = pci_resource_len(ha->pdev, 1);
	mmio_flags = pci_resource_flags(ha->pdev, 1);

	if (!(mmio_flags & IORESOURCE_MEM)) {
		qla_printk(KERN_ERR, ha,
		    "region #0 not an MMIO resource (%s), aborting\n",
		    pci_name(ha->pdev));
		goto iospace_error_exit;
	}
	if (mmio_len < MIN_IOBASE_LEN) {
		qla_printk(KERN_ERR, ha,
		    "Invalid PCI mem region size (%s), aborting\n",
			pci_name(ha->pdev));
		goto iospace_error_exit;
	}

	if (pci_request_regions(ha->pdev, QLA2XXX_DRIVER_NAME)) {
		qla_printk(KERN_WARNING, ha,
		    "Failed to reserve PIO/MMIO regions (%s)\n",
		    pci_name(ha->pdev));

		goto iospace_error_exit;
	}

	ha->pio_address = pio;
	ha->iobase = ioremap(mmio, MIN_IOBASE_LEN);
	if (!ha->iobase) {
		qla_printk(KERN_ERR, ha,
		    "cannot remap MMIO (%s), aborting\n", pci_name(ha->pdev));

		goto iospace_error_exit;
	}

	/* Determine queue resources */
 	ha->max_req_queues = ha->max_rsp_queues = 1;
 	if ((!ql2xmqqos && !ql2xmqcpuaffinity) || 
 		(ql2xmqqos && ql2xmqcpuaffinity) || 
 		(!IS_QLA25XX(ha) && !IS_QLA81XX(ha)))
		goto mqiobase_exit;

#if defined(__VMKLNX__)
	mmio = pci_resource_start(ha->pdev, 3);
	mmio_len = pci_resource_len(ha->pdev, 3);
	if (mmio_len < MIN_IOBASE_LEN || mmio == 0) {
	    qla_printk(KERN_INFO, ha, "BAR 3 not enabled\n");
	    goto mqiobase_exit;
	}
#endif

	ha->mqiobase = ioremap(pci_resource_start(ha->pdev, 3),
			pci_resource_len(ha->pdev, 3));
	if (ha->mqiobase) {
        	/* Read MSIX vector size of the board */
		pci_read_config_word(ha->pdev, QLA_PCI_MSIX_CONTROL, &msix);
		ha->msix_count = msix + 1;
		if (!(ha->msix_count > 2)) 
			goto mqiobase_exit;

		qla_printk(KERN_INFO, ha,
			"MSI-X vector count from config space: %d\n", msix);
        	/* Max queues are bounded by available msix vectors */
        	/* queue 0 uses two msix vectors */
 		if (ql2xmqcpuaffinity) {
 			cpus = smp_num_cpus;
			DEBUG2(printk("CPU affinity mode," 
				"number of CPUs = %d, number of MSI-X vectors = %d\n", 
				cpus, msix));
 			ha->max_rsp_queues = (ha->msix_count - 1 > cpus) ?
 				(cpus + 1) : (ha->msix_count - 1);
 			ha->max_req_queues = 2;
 		} else if (ql2xmqqos > 1) {
 			ha->max_req_queues = ql2xmqqos > QLA_MQ_SIZE ?
 						QLA_MQ_SIZE : ql2xmqqos;
 			DEBUG2(qla_printk(KERN_INFO, ha, "QoS mode, max no"
 			" of request queues:%d\n", ha->max_req_queues));
 		}
 	} else
 		qla_printk(KERN_INFO, ha, "BAR 3 not enabled\n");
mqiobase_exit:
 	ha->msix_count = ha->max_rsp_queues + 1;
	qla_printk(KERN_INFO, ha,
	    "MSI-X vector count usable: %d\n", ha->msix_count);
	return (0);

iospace_error_exit:
	return (-ENOMEM);
}

static void
qla2xxx_scan_start(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	if (vha->hw->flags.running_gold_fw && !ql2xdevdiscgoldfw)
		return;

	set_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags);
	set_bit(LOCAL_LOOP_UPDATE, &vha->dpc_flags);
	set_bit(RSCN_UPDATE, &vha->dpc_flags);
}

static int
qla2xxx_scan_finished(struct Scsi_Host *shost, unsigned long time)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	if (!vha->host)
		return 1;
#if defined(__VMKLNX__)
	if (time > vha->hw->loop_reset_delay * 6 * HZ) /* timeout after 30s */
#else
	if (time > vha->hw->loop_reset_delay * HZ)
#endif
		return 1;

	return atomic_read(&vha->loop_state) == LOOP_READY;
}

/*
 * PCI driver interface
 */
static int __devinit
qla2x00_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int	ret = -ENODEV;
	device_reg_t __iomem *reg;
	struct Scsi_Host *host;
	scsi_qla_host_t *base_vha = NULL, *fvha = NULL;
	struct qla_hw_data *ha;
	char pci_info[30];
	char fw_str[QLA_FW_VERSION_STR_SIZE];
	struct scsi_host_template *sht;
	int max_id;
	uint16_t req_length = 0, rsp_length = 0;
	struct req_que *req = NULL;
	struct rsp_que *rsp = NULL;

	if (pci_enable_device(pdev))
		goto probe_out;

#if !defined(__VMKLNX__) /* no vmklnx support for aer for now */
	if (pci_find_aer_capability(pdev))
		if (pci_enable_pcie_error_reporting(pdev))
			goto probe_out;
#endif

	sht = &qla2x00_driver_template;
	if (pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2422 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2432 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP5422 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP5432 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2532 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8432 ||
 	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8001) {
		sht = &qla24xx_driver_template;
	}

	if(ql2xmaxsgs !=0 && sht->sg_tablesize != ql2xmaxsgs) {
		sht->sg_tablesize = ql2xmaxsgs;
		printk(KERN_INFO
		"qla2xxx: Changing sg_tablesize=%d\n", ql2xmaxsgs);
	}

	host = scsi_host_alloc(sht, sizeof(scsi_qla_host_t));
	if (host == NULL) {
		printk(KERN_WARNING
		    "qla2xxx: Couldn't allocate host from scsi layer!\n");
		goto probe_disable_device;
	}

	ha = kzalloc(sizeof(struct qla_hw_data), GFP_KERNEL);
	if (!ha) {
		DEBUG(printk("Unable to allocate memory for ha\n"));
		goto probe_failed;
	}

	/* Clear our data area */
	base_vha = shost_priv(host);
	memset(base_vha, 0, sizeof(scsi_qla_host_t));

	ha->pdev = pdev;
	base_vha->host = host;
	base_vha->host_no = host->host_no;
	base_vha->hw = ha;
	sprintf(base_vha->host_str, "%s_%ld", QLA2XXX_DRIVER_NAME, base_vha->host_no);
	spin_lock_init(&ha->hardware_lock);
	spin_lock_init(&ha->work_lock);
	spin_lock_init(&ha->vport_lock);
	init_completion(&ha->mbx_cmd_comp);
	complete(&ha->mbx_cmd_comp);
	init_completion(&ha->mbx_intr_comp);

	INIT_LIST_HEAD(&base_vha->list);
	INIT_LIST_HEAD(&base_vha->vp_fcports);
	INIT_LIST_HEAD(&ha->vp_list);
	INIT_LIST_HEAD(&ha->vp_del_list);
	INIT_LIST_HEAD(&base_vha->work_list);

	/* Set ISP-type information. */
	qla2x00_set_isp_flags(ha);

	/* Configure PCI I/O space */
	ret = qla2x00_iospace_config(ha);
	if (ret)
		goto probe_failed;

	qla_printk(KERN_INFO, ha,
	    "Found an ISP%04X, irq %d, iobase 0x%p\n", pdev->device, pdev->irq,
	    ha->iobase);

	ha->prev_topology = 0;
	ha->init_cb_size = sizeof(init_cb_t);
	base_vha->mgmt_svr_loop_id = MANAGEMENT_SERVER + base_vha->vp_idx;
	ha->link_data_rate = PORT_SPEED_UNKNOWN;
	ha->optrom_size = OPTROM_SIZE_2300;

	/* Assign ISP specific operations. */
	max_id = MAX_TARGETS;
	if (IS_QLA2100(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT_2100;
		req_length = REQUEST_ENTRY_CNT_2100;
		rsp_length = RESPONSE_ENTRY_CNT_2100;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2100;
		ha->gid_list_info_size = 4;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->isp_ops = &qla2100_isp_ops;
	} else if (IS_QLA2200(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_2200;
		rsp_length = RESPONSE_ENTRY_CNT_2100;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2100;
		ha->isp_ops = &qla2100_isp_ops;
	} else if (IS_QLA23XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_2200;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->gid_list_info_size = 6;
		if (IS_QLA2322(ha) || IS_QLA6322(ha))
			ha->optrom_size = OPTROM_SIZE_2322;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->isp_ops = &qla2300_isp_ops;
	} else if (IS_QLA24XX_TYPE(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_24xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_24XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA24XX;
		ha->isp_ops = &qla24xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA;
		ha->nvram_conf_off = FARX_ACCESS_NVRAM_CONF;
		ha->nvram_data_off = FARX_ACCESS_NVRAM_DATA;
	} else if (IS_QLA25XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_24xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_25XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA25XX;
		ha->isp_ops = &qla25xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA;
		ha->nvram_conf_off = FARX_ACCESS_NVRAM_CONF;
		ha->nvram_data_off = FARX_ACCESS_NVRAM_DATA;
		base_vha->flags.thermal_supported = 1;
	} else if (IS_QLA81XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_81xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_81XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA25XX;
		ha->isp_ops = &qla81xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF_81XX;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA_81XX;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
	}

	/* load the F/W, read paramaters, and init the H/W */

	set_bit(0, (unsigned long *) ha->vp_idx_map);

	pci_set_drvdata(pdev, base_vha);

	qla2x00_config_dma_addressing(ha);
	ret = qla2x00_mem_alloc(ha, req_length, rsp_length, &req, &rsp);
	if (ret) {
		qla_printk(KERN_WARNING, ha,
			"[ERROR] Failed to allocate memory for adapter\n");
		goto probe_failed;
	}

	req->max_q_depth = MAX_Q_DEPTH;
	if (ql2xmaxqdepth != 0 && ql2xmaxqdepth <= 0xffffU)
		req->max_q_depth = ql2xmaxqdepth;

 	base_vha->req = req;
	host->can_queue = req->length + 128;

	host->max_id = max_id;
	host->this_id = 255;
	host->cmd_per_lun = 3;
	host->unique_id = host->host_no;
	host->max_cmd_len = MAX_CMDSZ;
	host->max_channel = MAX_BUSES - 1;
	host->max_lun = MAX_LUNS;
	host->transportt = qla2xxx_transport_template;

	/* Set up the irqs */
	ret = qla2x00_request_irqs(ha, rsp);
	if (ret)
 		goto probe_failed;

que_init:
	/* Alloc arrays of request and response ring ptrs */
	if (!qla2x00_alloc_queues(ha)) {
		qla_printk(KERN_WARNING, ha,
		"[ERROR] Failed to allocate memory for queue"
		" pointers\n");
 		goto probe_failed;
	}
	ha->rsp_q_map[0] = rsp;
	ha->req_q_map[0] = req;
	rsp->req = req;
	req->rsp = rsp;
	set_bit(0, ha->req_qid_map);
	set_bit(0, ha->rsp_qid_map);
	/* FWI2-capable only. */
	req->req_q_in = &ha->iobase->isp24.req_q_in;
	req->req_q_out = &ha->iobase->isp24.req_q_out;
	rsp->rsp_q_in = &ha->iobase->isp24.rsp_q_in;
	rsp->rsp_q_out = &ha->iobase->isp24.rsp_q_out;
	if (ha->mqenable) {
		req->req_q_in = &ha->mqiobase->isp25mq.req_q_in;
		req->req_q_out = &ha->mqiobase->isp25mq.req_q_out;
		rsp->rsp_q_in = &ha->mqiobase->isp25mq.rsp_q_in;
		rsp->rsp_q_out =  &ha->mqiobase->isp25mq.rsp_q_out;
	}

	if (qla2x00_initialize_adapter(base_vha)) {
		qla_printk(KERN_WARNING, ha,
		    "Failed to initialize adapter\n");

		DEBUG2(printk("scsi(%ld): Failed to initialize adapter - "
		    "Adapter flags %x.\n",
		    base_vha->host_no, base_vha->device_flags));

		ret = -ENODEV;
		goto probe_failed;
	}

	if (ha->mqenable && ql2xmqcpuaffinity) {
		/* Attempt to create queues for cpu affinity */
		if (qla25xx_setup_cpuaffinity(base_vha)) {
			qla_printk(KERN_WARNING, ha,
				"Can't create queues, disable multi-queue\n");
			goto que_init;
		}
	}

	if (!ha->flags.running_gold_fw || ql2xdevdiscgoldfw) {
		/*
		 * Startup the kernel thread for this host adapter
		 */
		ha->dpc_thread = kthread_create(qla2x00_do_dpc, ha,
				"%s_dpc", base_vha->host_str);
		if (IS_ERR(ha->dpc_thread)) {
			qla_printk(KERN_WARNING, ha,
					"Unable to start DPC thread!\n");
			ret = PTR_ERR(ha->dpc_thread);
			goto probe_failed;
		}
	}

	/* Initialized the timer */
	qla2x00_start_timer(base_vha, qla2x00_timer, WATCH_INTERVAL);

	DEBUG2(printk("DEBUG: detect hba %ld at address = %p\n",
		base_vha->host_no, ha));

	ha->isp_ops->disable_intrs(ha);

	spin_lock_irq(&ha->hardware_lock);
	reg = ha->iobase;
	if (IS_FWI2_CAPABLE(ha)) {
		WRT_REG_DWORD(&reg->isp24.hccr, HCCRX_CLR_HOST_INT);
		WRT_REG_DWORD(&reg->isp24.hccr, HCCRX_CLR_RISC_INT);
	} else {
		WRT_REG_WORD(&reg->isp.semaphore, 0);
		WRT_REG_WORD(&reg->isp.hccr, HCCR_CLR_RISC_INT);
		WRT_REG_WORD(&reg->isp.hccr, HCCR_CLR_HOST_INT);

		/* Enable proper parity */
		if (!IS_QLA2100(ha) && !IS_QLA2200(ha)) {
			if (IS_QLA2300(ha))
				/* SRAM parity */
				WRT_REG_WORD(&reg->isp.hccr,
				    (HCCR_ENABLE_PARITY + 0x1));
			else
				/* SRAM, Instruction RAM and GP RAM parity */
				WRT_REG_WORD(&reg->isp.hccr,
				    (HCCR_ENABLE_PARITY + 0x7));
		}
	}
	spin_unlock_irq(&ha->hardware_lock);

	/* Insert new entry into the list of adapters */
	mutex_lock(&instance_lock);
	list_add_tail(&base_vha->hostlist, &qla_hostlist);
	base_vha->instance = find_first_zero_bit(host_instance_map, MAX_HBAS);
	if (base_vha->instance == MAX_HBAS) {
		DEBUG9_10(printk("Host instance exhausted\n"));
	}
	set_bit(base_vha->instance, host_instance_map);
	num_hosts++;
	mutex_unlock(&instance_lock);

	list_add_tail(&base_vha->list, &ha->vp_list);

	ha->isp_ops->enable_intrs(ha);

	base_vha->flags.init_done = 1;
	base_vha->flags.online = 1;

	if(ql2xusedrivernaming) {
		host->useDriverNamingDevice = 1;
		sprintf(host->name, "qlhba%04X-%x",pdev->device,
		    (unsigned int)base_vha->host_no);
	}
	ret = scsi_add_host(host, &pdev->dev);
	if (ret) {
		mutex_lock(&instance_lock);
		list_del(&base_vha->hostlist);
		clear_bit(base_vha->instance, host_instance_map);
		num_hosts--;
		mutex_unlock(&instance_lock);
		goto probe_failed;
	}

#if defined(__VMKLNX__)
        if (pdev->msix_enabled) {
		/*
		 * MSIX default handler is being used for coredump
		 */
		vmklnx_scsi_register_poll_handler(host, ha->msix_entries[0].vector,
						  qla24xx_msix_poll, rsp);
	} else {
		vmklnx_scsi_register_poll_handler(host, ha->pdev->irq,
						  ha->isp_ops->intr_handler, rsp);
	}
#endif /* defined(__VMKLNX__) */

	/* Register all the instantiated pre-boot hosts. */
	list_for_each_entry(fvha, &ha->vp_list, list) {
		if(fvha->vp_idx == 0) {
			continue;
		}	
		ret = qla2xxx_add_boot_host(fvha);
		if (ret) {
			/* Free up allocated resources. */
			qla2xxx_delete_boothost(fvha, 0);
		}
		fvha->flags.init_done = 1;
	}	

	/* this register call needs to be below scsi_add_host */
	/* due to vmkernel API requirements */
	if (ha->mqenable && ql2xmqcpuaffinity) 
		qla25xx_register_ioqueues(base_vha);

	scsi_scan_host(host);

	qla2x00_init_host_attr(base_vha);

#if defined(__VMKLNX__)
	ha->vmhba_name = vmklnx_get_vmhba_name(host);
#else
	ha->vmhba_name = "none";
#endif

	qla_printk(KERN_INFO, ha, "\n"
	    " QLogic Fibre Channel HBA Driver: %s: %s\n"
	    "  QLogic %s - %s\n"
	    "  ISP%04X: %s @ %s hdma%c, host#=%ld, fw=%s\n",
	    qla2x00_version_str, ha->vmhba_name, ha->model_number,
	    ha->model_desc ? ha->model_desc: "", pdev->device,
	    ha->isp_ops->pci_info_str(base_vha, pci_info), pci_name(pdev),
	    ha->flags.enable_64bit_addressing ? '+': '-', base_vha->host_no,
	    ha->isp_ops->fw_version_str(base_vha, fw_str));

	return 0;

probe_failed:
	qla2x00_free_device(base_vha);

	scsi_host_put(host);

probe_disable_device:
	pci_disable_device(pdev);

probe_out:
	return ret;
}

static void __devexit
qla2x00_remove_one(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha;
	struct qla_hw_data *ha;
	struct list_head *vhal, *tmp_vha;
	scsi_qla_host_t *search_vha = NULL;

	base_vha = pci_get_drvdata(pdev);
	ha = base_vha->hw;

	/* Remove ha from the global hostlist */
	mutex_lock(&instance_lock);
	list_for_each_safe(vhal, tmp_vha, &qla_hostlist) {
		search_vha = list_entry(vhal, scsi_qla_host_t, hostlist);

		if (search_vha->host_no == base_vha->host_no) {
			list_del(vhal);
			break;
		}
	}
	num_hosts--;
	mutex_unlock(&instance_lock);

	qla84xx_put_chip(base_vha);

	fc_remove_host(base_vha->host);

	scsi_remove_host(base_vha->host);

	qla2x00_free_device(base_vha);

	qla2xxx_npiv_cleanup(base_vha);
	printk(KERN_INFO "Completed NPIV cleanups\n");

	scsi_host_put(base_vha->host);

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static void
qla2x00_free_device(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;

	qla2x00_abort_all_cmds(vha, DID_NO_CONNECT << 16);

	/* Disable timer */
	if (vha->timer_active)
		qla2x00_stop_timer(vha);

	/* Kill the kernel thread for this host */
	if (ha->dpc_thread) {
		struct task_struct *t = ha->dpc_thread;

		/*
		 * qla2xxx_wake_dpc checks for ->dpc_thread
		 * so we need to zero it out.
		 */
		ha->dpc_thread = NULL;
		kthread_stop(t);
	}

	qla25xx_delete_queues(vha);

	if (ha->eft)
		qla2x00_disable_eft_trace(vha);

	/* Stop currently executing firmware. */
	qla2x00_try_to_stop_firmware(vha);

	/* turn-off interrupts on the card */
	if (ha->interrupts_on)
		ha->isp_ops->disable_intrs(ha);

	qla2x00_free_irqs(vha);

	qla2x00_mem_free(ha);

	if (vha->fcp_prio_cfg) {
		vfree(vha->fcp_prio_cfg);
		vha->fcp_prio_cfg = NULL;
	}

	qla2x00_free_queues(ha);

	if (ha->iobase)
		iounmap(ha->iobase);

	if (ha->mqiobase)
		iounmap(ha->mqiobase);

	pci_release_regions(ha->pdev);
	kfree(ha);
	ha = NULL;
}

static inline void
qla2x00_schedule_rport_del(struct scsi_qla_host *vha, fc_port_t *fcport,
    int defer)
{
	struct fc_rport *rport;
	struct scsi_qla_host *base_vha = pci_get_drvdata(vha->hw->pdev);
	if (!fcport->rport)
		return;

	rport = fcport->rport;
	if (defer) {
		unsigned long flags;
		DEBUG2(printk("scsi(%ld): defer rport_del rport %lx %x\n",
			vha->host_no, (unsigned long)rport->port_name, rport->port_id));
		spin_lock_irqsave(vha->host->host_lock, flags);
		fcport->drport = rport;
		spin_unlock_irqrestore(vha->host->host_lock, flags);
		set_bit(FCPORT_UPDATE_NEEDED, &vha->dpc_flags);
		qla2xxx_wake_dpc(base_vha);
	} else {
		DEBUG2(printk("scsi(%ld): rport_del rport %lx %x\n",
			vha->host_no, (unsigned long)rport->port_name, rport->port_id));
		fc_remote_port_delete(rport);
	}
}

/*
 * The following support functions are adopted to handle
 * the re-entrant qla2x00_proc_info correctly.
 */
static void
copy_mem_info(struct info_str *info, char *data, int len)
{
	if (info->pos + len > info->offset + info->length)
		len = info->offset + info->length - info->pos;

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

/**************************************************************************
 * qla23xx_blink_led
 *
 * Description:
 *   This function sets the colour of the LED for 23xx while preserving the
 *   unsued GPIO pins every sec.
 *
 * Input:
 *       ha - Host adapter structure
 *
 * Return:
 * 	None
 *
 * Context: qla2x00_timer() Interrupt
 ***************************************************************************/
inline void
qla23xx_flip_colors(scsi_qla_host_t *vha, uint16_t *pflags)
{
	struct qla_hw_data *ha = vha->hw;
	if (IS_QLA2322(ha)) {
		/* flip all colors */
		if (ha->beacon_color_state == QLA_LED_ALL_ON) {
			/* turn off */
			ha->beacon_color_state = 0;
			*pflags = GPIO_LED_ALL_OFF;
		} else {
			/* turn on */
			ha->beacon_color_state = QLA_LED_ALL_ON;
			*pflags = GPIO_LED_RGA_ON;
		}
	} else {
		/* flip green led only */
		if (ha->beacon_color_state == QLA_LED_GRN_ON) {
			/* turn off */
			ha->beacon_color_state = 0;
			*pflags = GPIO_LED_GREEN_OFF_AMBER_OFF;
		} else {
			/* turn on */
			ha->beacon_color_state = QLA_LED_GRN_ON;
			*pflags = GPIO_LED_GREEN_ON_AMBER_OFF;
		}
	}
}

void
qla23xx_blink_led(scsi_qla_host_t *vha)
{
	uint16_t	gpio_enable;
	uint16_t	gpio_data;
	uint16_t	led_color = 0;
	unsigned long	cpu_flags = 0;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	if (ha->pio_address)
		reg = (struct device_reg_2xxx *)ha->pio_address;

	spin_lock_irqsave(&ha->hardware_lock, cpu_flags);

	/* Save the Original GPIOE */
	if (ha->pio_address) {
		gpio_enable = RD_REG_WORD_PIO(&reg->gpioe);
		gpio_data   = RD_REG_WORD_PIO(&reg->gpiod);
	} else {
		gpio_enable = RD_REG_WORD(&reg->gpioe);
		gpio_data = RD_REG_WORD(&reg->gpiod);
	}

	DEBUG9(printk("%s Original data of gpio_enable_reg=0x%x"
	    " gpio_data_reg=0x%x\n",
	    __func__,gpio_enable,gpio_data));

	/* Set the modified gpio_enable values */
	gpio_enable |= GPIO_LED_MASK;

	DEBUG9(printk("%s Before writing enable : gpio_enable_reg=0x%x"
	    " gpio_data_reg=0x%x led_color=0x%x\n",
	    __func__, gpio_enable, gpio_data, led_color));

	if (ha->pio_address)
		WRT_REG_WORD_PIO(&reg->gpioe, gpio_enable);
	else {
		WRT_REG_WORD(&reg->gpioe, gpio_enable);
		RD_REG_WORD(&reg->gpioe);
	}

	qla23xx_flip_colors(vha, &led_color);

	/* Clear out any previously set LED color */
	gpio_data &= ~GPIO_LED_MASK;

	/* Set the new input LED color to GPIOD */
	gpio_data |= led_color;

	DEBUG9(printk("%s Before writing data: gpio_enable_reg=0x%x"
	    " gpio_data_reg=0x%x led_color=0x%x\n",
	    __func__,gpio_enable,gpio_data,led_color));

	/* Set the modified gpio_data values */
	if (ha->pio_address)
		WRT_REG_WORD_PIO(&reg->gpiod, gpio_data);
	else {
		WRT_REG_WORD(&reg->gpiod, gpio_data);
		RD_REG_WORD(&reg->gpiod);
	}

	spin_unlock_irqrestore(&ha->hardware_lock, cpu_flags);

	return;
}

/*************************************************************************
* qla2x00_proc_info
*
* Description:
*   Return information to handle /proc support for the driver.
*
* inout : decides the direction of the dataflow and the meaning of the
*         variables
* buffer: If inout==0 data is being written to it else read from it
*         (ptr to a page buffer)
* *start: If inout==0 start of the valid data in the buffer
* offset: If inout==0 starting offset from the beginning of all
*         possible data to return.
* length: If inout==0 max number of bytes to be written into the buffer
*         else number of bytes in "buffer"
* Returns:
*         < 0:  error. errno value.
*         >= 0: sizeof data returned.
*************************************************************************/
int
qla2x00_proc_info(struct Scsi_Host *shost, char *buffer,
    char **start, off_t offset, int length, int inout)
{
	struct info_str info;
	int             retval = -EINVAL;
	unsigned int    t;
	uint32_t        tmp_sn;
	uint32_t        *flags;
	char         *loop_state;
	char         *port_speed;
	char         *fcport_state;
	struct list_head *vhal;
	scsi_qla_host_t *vha, *vp, *tvp, *localvp = NULL;
	scsi_qla_host_t *search_vha = NULL;
	char fw_info[QLA_FW_VERSION_STR_SIZE];
	fc_port_t *fcport = NULL;
	unsigned long   lock_flags;
	struct qla_hw_data *ha;
	struct req_que *req;
	struct rsp_que *rsp;
#if defined(MSIX_CNTS)
	struct qla_msix_entry *qentry;
#endif
	int que, cnt;
	uint32_t count = 0;

	DEBUG3(printk(KERN_INFO
		"Entering proc_info buff_in=%p, offset=0x%lx, length=0x%x\n",
	   buffer, offset, length));

	vha = shost_priv(shost);
	ha = vha->hw;
	req = ha->req_q_map[0];
	rsp = ha->rsp_q_map[0];

	/* 
	 * Ensure that the ha is present in
	 * the vmkernel context.
	 */

	mutex_lock(&instance_lock);
	list_for_each(vhal, &qla_hostlist) {
		search_vha = list_entry(vhal, scsi_qla_host_t, hostlist);

		if (search_vha == vha)
			goto display_proc;
	}
	mutex_unlock(&instance_lock);

	return 0;

	/* Go ahead and print proc info. */
display_proc:	
	mutex_unlock(&instance_lock);

	if (inout) {
		/* Has data been written to the file? */
		DEBUG3(printk(
		   "%s: has data been written to the file. \n",
		   __func__));
		return qla2x00_set_info(buffer, length, shost);
	}

	if (start) {
	       *start = buffer;
	}

	info.buffer = buffer;
	info.length = length;
	info.offset = offset;
	info.pos    = 0;

	/* start building the print buffer */

	if (IS_QLA81XX(ha)) {
		char *chip_type = NULL;
		/* Type of chip : MPI capabilities 15-8 bits */
		uint8_t mpi_cap = (uint8_t)(ha->mpi_capabilities >> 8);

		if (mpi_cap == MPI_CAP_LIST_PROTOTYPE)
			chip_type = "Prototype";
		else if (mpi_cap == MPI_CAP_LIST_QLE8142)
			chip_type = "QLE814x";
		else if (mpi_cap == MPI_CAP_LIST_QLE8152)
			chip_type = "QLE815x";
		else if (mpi_cap == MPI_CAP_LIST_QMI8142)
			chip_type = "QMI814x";
		else
			chip_type = "Unknown";

		copy_info(&info,
				"QLogic PCI Express to FCoE Converged Network Adapter for %s:\n"
				"        FC Firmware version %s, Chip Type: %s\n",
				ha->model_number, ha->isp_ops->fw_version_str(vha, fw_info),
				chip_type);
		copy_info(&info, "        MPI Firmware Version: %2d.%02d.%02d (%d)\n",
				ha->mpi_version[0], ha->mpi_version[1], ha->mpi_version[2],
				ha->mpi_capabilities);

		if(strncmp(chip_type, "QLE814x", 8) && strncmp(chip_type, "QMI814x", 8))
		copy_info(&info, "        PHY Firmware Version: %2d.%02d.%02d \n",
				ha->phy_version[0], ha->phy_version[1], ha->phy_version[2]);
	} else {
		copy_info(&info,
				"QLogic PCI to Fibre Channel Host Adapter for %s:\n"
				"        FC Firmware version %s, ",
				ha->model_number, ha->isp_ops->fw_version_str(vha, fw_info));
	}

	copy_info(&info, "Driver version %s\n", qla2x00_version_str);

	if(ha->vmhba_name) {
		copy_info(&info, "\n");
		copy_info(&info, "Host Device Name %s\n", ha->vmhba_name);
		copy_info(&info, "\n");
	}

	copy_info(&info, "BIOS version %d.%02d\n", ha->bios_revision[1],
	   ha->bios_revision[0]);
	if (IS_FWI2_CAPABLE(ha))
	       copy_info(&info, "FCODE version %d.%02d\n",
		       ha->fcode_revision[1], ha->fcode_revision[0]);
	else
	       copy_info(&info, "FCODE version %s\n", ha->fcode_revision);
	copy_info(&info, "EFI version %d.%02d\n", ha->efi_revision[1],
	       ha->efi_revision[0]);
	copy_info(&info, "Flash FW version %d.%02d.%02d\n",
	       ha->fw_revision[0], ha->fw_revision[1], ha->fw_revision[2]);

	copy_info(&info, "ISP: ISP%04X", ha->pdev->device);
	if (IS_QLA24XX_TYPE(ha) || IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
	       copy_info(&info, "\n");
	} else {
	       tmp_sn = ((ha->serial0 & 0x1f) << 16) | (ha->serial2 << 8) |
		   ha->serial1;
	       copy_info(&info, ", Serial# %c%05d\n", 'A' + tmp_sn / 100000,
		   tmp_sn % 100000);
	}

	copy_info(&info,
	   "Request Queue = 0x%llx, Response Queue = 0x%llx\n",
	       (unsigned long long)req->dma,
	       (unsigned long long)rsp->dma);

	copy_info(&info,
	   "Request Queue count = %d, Response Queue count = %d\n",
	   req->length, rsp->length);

	copy_info(&info,
	    "Number of response queues for multi-queue operation: %d\n",
		ha->max_rsp_queues - 1);

	if (ha->flags.cpu_affinity_enabled) {
		copy_info(&info,
		    "\nCPU Affinity mode enabled\n");
#if defined(MSIX_CNTS)
		for (t=0; t<ha->max_rsp_queues+1; t++) {
		qentry = &ha->msix_entries[t];
		copy_info(&info,
	   	    "Total number of MSI-X interrupts on vector %d (%lx), entry %x = %ld\n",
	   	    t,qentry->vector, qentry->entry,(long)qentry->ints);
		}
#endif
		req = ha->req_q_map[1];
	}
	else {
		copy_info(&info,
		    "Total number of interrupts = %ld\n",
		    (long)ha->total_isr_cnt);
	}

	copy_info(&info,
	   "    Device queue depth = 0x%x\n",
	   (ql2xmaxqdepth == 0) ? 16 : ql2xmaxqdepth);

	copy_info(&info,
	   "Number of free request entries = %d\n", req->cnt);

	spin_lock_irqsave(&ha->hardware_lock, lock_flags);
	for (que = 0; que < ha->max_req_queues; que++) {
		req = ha->req_q_map[que];
		if (!req)
			continue;
		for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
			if (!req->outstanding_cmds[cnt])
				continue;
			count++;
		}
	}
	spin_unlock_irqrestore(&ha->hardware_lock, lock_flags);

	copy_info(&info,
	    "Total number of outstanding commands: %ld\n", count);

	copy_info(&info,
	   "Number of mailbox timeouts = %ld\n", ha->total_mbx_timeout);

	copy_info(&info,
	   "Number of ISP aborts = %ld\n", ha->qla_stats.total_isp_aborts);

	copy_info(&info,
	   "Number of loop resyncs = %ld\n", ha->total_loop_resync);

	flags = (uint32_t *) &ha->flags;

	if (atomic_read(&vha->loop_state) == LOOP_DOWN) {
	       loop_state = "DOWN";
	} else if (atomic_read(&vha->loop_state) == LOOP_UP) {
	       loop_state = "UP";
	} else if (atomic_read(&vha->loop_state) == LOOP_READY) {
	       loop_state = "READY";
	} else if (atomic_read(&vha->loop_state) == LOOP_TIMEOUT) {
	       loop_state = "TIMEOUT";
	} else if (atomic_read(&vha->loop_state) == LOOP_UPDATE) {
	       loop_state = "UPDATE";
	} else if (atomic_read(&vha->loop_state) == LOOP_DEAD) {
	       loop_state = "DEAD";
	} else {
	       loop_state = "UNKNOWN";
	}

	copy_info(&info,
	   "Host adapter:Loop State = <%s>, flags = 0x%lx\n",
	   loop_state , *flags);

	switch (ha->link_data_rate) {
	case 0:
		port_speed = "1 Gbps";
		break;
	case 1:
		port_speed = "2 Gbps";
		break;
	case 3:
		port_speed = "4 Gbps";
		break;
	case 4:
		port_speed = "8 Gbps";
		break;
	case 0x13:
		port_speed = "10 Gbps";
		break;
	default:
		/* unknown */
		port_speed = "Unknown";
		break;
	}
	copy_info(&info,
	   "Link speed = <%s>\n", port_speed);
	copy_info(&info, "\n");

	copy_info(&info, "Dpc flags = 0x%lx\n", vha->dpc_flags);

	copy_info(&info, "Link down Timeout = %3.3d\n",
	   ha->link_down_timeout);

	copy_info(&info, "Port down retry = %3.3d\n",
	   ha->port_down_retry_count);

	copy_info(&info, "Login retry count = %3.3d\n",
	   ha->login_retry_count);	

	if (IS_QLA24XX_TYPE(ha) || IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
		struct init_cb_24xx *icb = (struct init_cb_24xx *)ha->init_cb;
		copy_info(&info, "Execution throttle = %d\n",
	   		icb->execution_throttle);	
	}

	copy_info(&info, "ZIO mode = 0x%x, ZIO timer = %d\n",
	   ha->zio_mode, ha->zio_timer);	

	copy_info(&info,
	   "Commands retried with dropped frame(s) = %d\n",
	   ha->dropped_frame_error_cnt);

	copy_info(&info,
	   "Product ID = %04x %04x %04x %04x\n", ha->product_id[0],
	   ha->product_id[1], ha->product_id[2], ha->product_id[3]);

	copy_info(&info, "\n");

	if (ha->flags.npiv_supported && !vha->vp_idx) {
		copy_info(&info, "\n");
		copy_info(&info, "NPIV Supported : Yes \n");
		copy_info(&info,
		    "Max Virtual Ports = %d\n", ha->max_npiv_vports
			- (ha->num_vhosts - ha->cur_vport_count));
	} else {
		copy_info(&info, "NPIV Supported : No \n");
	}
	if (ha->cur_vport_count && !vha->vp_idx) {
		copy_info(&info,
		"Number of Virtual Ports in Use = %ld\n", ha->cur_vport_count);
	}

	/* Display the node name for adapter */
	copy_info(&info, "\nSCSI Device Information:\n");
	copy_info(&info,
	   "scsi-qla%ld-adapter-node=%02x%02x%02x%02x%02x%02x%02x%02x:"
		"%02x%02x%02x:%x;\n",
	   vha->instance, vha->node_name[0], vha->node_name[1], vha->node_name[2],
	   vha->node_name[3], vha->node_name[4], vha->node_name[5],
	   vha->node_name[6], vha->node_name[7],
		vha->d_id.b.domain, vha->d_id.b.area,
		vha->d_id.b.al_pa, vha->loop_id);

	/* display the port name for adapter */
	copy_info(&info,
	   "scsi-qla%ld-adapter-port=%02x%02x%02x%02x%02x%02x%02x%02x:"
		"%02x%02x%02x:%x;\n",
	   vha->instance, vha->port_name[0], vha->port_name[1], vha->port_name[2],
	   vha->port_name[3], vha->port_name[4], vha->port_name[5],
	   vha->port_name[6], vha->port_name[7],
		vha->d_id.b.domain, vha->d_id.b.area,
		vha->d_id.b.al_pa, vha->loop_id);

	/* Print out target port names */
	t = 0;
	fcport = NULL;
	copy_info(&info, "\nFC Target-Port List:\n");
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		switch (atomic_read(&fcport->state)) {
			case FCS_ONLINE:
				fcport_state = "Online";
				break;
			case FCS_DEVICE_LOST:
				fcport_state = "<Lost>";
				break;
			default:	
				fcport_state = "<Offline>";
		}

		copy_info(&info,
			"scsi-qla%ld-target-%d="
			"%02x%02x%02x%02x%02x%02x%02x%02x:"
			"%02x%02x%02x:%x:%s;\n",
			vha->instance, t,
			fcport->port_name[0], fcport->port_name[1],
			fcport->port_name[2], fcport->port_name[3],
			fcport->port_name[4], fcport->port_name[5],
			fcport->port_name[6], fcport->port_name[7],
			fcport->d_id.b.domain, fcport->d_id.b.area,
			fcport->d_id.b.al_pa, fcport->loop_id,
			fcport_state);

		t++;
	}

	/* Print out the port Information */
	t = 0;
	fcport = NULL;
 	if(ql2xextended_error_logging && !vha->vp_idx) {
		copy_info(&info, "\nNon-Target FC Port Information:\n");
 		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type == FCT_TARGET)
				continue;
 
			switch (atomic_read(&fcport->state)) {
				case FCS_ONLINE:
					fcport_state = "Online";
					break;
				case FCS_DEVICE_LOST:
					fcport_state = "<Lost>";
					break;
				default:	
					fcport_state = "<Offline>";
			}
 			copy_info(&info,
 				"scsi-qla%ld-port-%d="
 				"%02x%02x%02x%02x%02x%02x%02x%02x:"
 				"%02x%02x%02x%02x%02x%02x%02x%02x:"
				"%02x%02x%02x:%x:%s;\n",
 				vha->instance, t,
 				fcport->node_name[0], fcport->node_name[1],
 				fcport->node_name[2], fcport->node_name[3],
 				fcport->node_name[4], fcport->node_name[5],
 				fcport->node_name[6], fcport->node_name[7],
 				fcport->port_name[0], fcport->port_name[1],
 				fcport->port_name[2], fcport->port_name[3],
 				fcport->port_name[4], fcport->port_name[5],
 				fcport->port_name[6], fcport->port_name[7],
 				fcport->d_id.b.domain, fcport->d_id.b.area,
				fcport->d_id.b.al_pa, fcport->loop_id,
				fcport_state);
  
 			t++;
 		}
 	}	
  
 	/* Display this information for VIrtual Ports only. */
 	if (ha->cur_vport_count && !vha->vp_idx) {
		copy_info(&info, "\nVirtual Port Information:\n");

		spin_lock_irqsave(&ha->vport_lock, lock_flags);
		list_for_each_entry_safe(vp, tvp, &ha->vp_list, list) {
 			/* Skip pre-boot hosts. */
 			if(!vp->fc_vport)
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

			copy_info(&info,
			   "\n Virtual Port WWNN:WWPN:ID =%02x%02x%02x%02x%02x%02x%02x%02x:"
			   "%02x%02x%02x%02x%02x%02x%02x%02x:%02x%02x%02x;\n",
				vp->node_name[0], vp->node_name[1],
				vp->node_name[2], vp->node_name[3],
				vp->node_name[4], vp->node_name[5],
				vp->node_name[6], vp->node_name[7],
				vp->port_name[0], vp->port_name[1],
				vp->port_name[2], vp->port_name[3],
				vp->port_name[4], vp->port_name[5],
				vp->port_name[6], vp->port_name[7],
				vp->d_id.b.domain, vp->d_id.b.area,
				vp->d_id.b.al_pa);

			if (atomic_read(&vp->vp_state) == VP_OFFLINE) {                              
				loop_state = "OFFLINE";                                              
			} else if (atomic_read(&vp->vp_state) == VP_ACTIVE) {                        
				loop_state = "ACTIVE";                                               
			} else if (atomic_read(&vp->vp_state) == VP_FAILED) {                        
				loop_state = "FAILED";                                               
			} else {                                                                     
				loop_state = "UNKNOWN";                                              
			}                                                                            
		
			copy_info(&info,
			    "Virtual Port %d:VP State = <%s>, Vp Flags = 0x%lx\n",
			    vp->vp_idx - vp->hw->num_vhosts + vp->hw->cur_vport_count,
			    loop_state , vha->vp_flags);                                                  

			copy_info(&info, "\nFC Port Information for Virtual Port %d:\n",
				vp->vp_idx - vp->hw->num_vhosts + vp->hw->cur_vport_count);
				list_for_each_entry(fcport, &vp->vp_fcports, list) {
					if (fcport->port_type != FCT_TARGET)
						continue;

					copy_info(&info,
					"scsi-qla%d-port-%d="
					"%02x%02x%02x%02x%02x%02x%02x%02x:"
					"%02x%02x%02x%02x%02x%02x%02x%02x:"
					"%06x:%x;\n",
					(int)vha->instance, t,
					fcport->node_name[0], fcport->node_name[1],
					fcport->node_name[2], fcport->node_name[3],
					fcport->node_name[4], fcport->node_name[5],
					fcport->node_name[6], fcport->node_name[7],
					fcport->port_name[0], fcport->port_name[1],
					fcport->port_name[2], fcport->port_name[3],
					fcport->port_name[4], fcport->port_name[5],
					fcport->port_name[6], fcport->port_name[7],
					fcport->d_id.b24, fcport->loop_id);
					t++;
				}
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
		spin_unlock_irqrestore(&ha->vport_lock, lock_flags);
	}
	copy_info(&info, "\n");

       retval = info.pos > info.offset ? info.pos - info.offset : 0;

       DEBUG3(printk(KERN_INFO
           "Exiting proc_info: info.pos=%d, offset=0x%lx, "
           "length=0x%x\n", info.pos, offset, length));

       return (retval);
}

/*
 * qla2x00_mark_device_lost Updates fcport state when device goes offline.
 *
 * Input: ha = adapter block pointer.  fcport = port structure pointer.
 *
 * Return: None.
 *
 * Context:
 */
void qla2x00_mark_device_lost(scsi_qla_host_t *vha, fc_port_t *fcport,
    int do_login, int defer)
{
	if (atomic_read(&fcport->state) == FCS_ONLINE &&
	    vha->vp_idx == fcport->vp_idx) {
		atomic_set(&fcport->state, FCS_DEVICE_LOST);
		qla2x00_schedule_rport_del(vha, fcport, defer);
	}

	/*
	 * We may need to retry the login, so don't change the state of the
	 * port but do the retries.
	 */
	if (atomic_read(&fcport->state) != FCS_DEVICE_DEAD)
		atomic_set(&fcport->state, FCS_DEVICE_LOST);

	if (!do_login)
		return;

	if (fcport->login_retry == 0) {
		fcport->login_retry = vha->hw->login_retry_count;
		set_bit(RELOGIN_NEEDED, &vha->dpc_flags);

		DEBUG(printk("scsi(%ld): Port login retry: "
		    "%02x%02x%02x%02x%02x%02x%02x%02x, "
		    "id = 0x%04x retry cnt=%d\n",
		    vha->host_no,
		    fcport->port_name[0],
		    fcport->port_name[1],
		    fcport->port_name[2],
		    fcport->port_name[3],
		    fcport->port_name[4],
		    fcport->port_name[5],
		    fcport->port_name[6],
		    fcport->port_name[7],
		    fcport->loop_id,
		    fcport->login_retry));
	}
}

/*
 * qla2x00_mark_all_devices_lost
 *	Updates fcport state when device goes offline.
 *
 * Input:
 *	ha = adapter block pointer.
 *	fcport = port structure pointer.
 *
 * Return:
 *	None.
 *
 * Context:
 */
void
qla2x00_mark_all_devices_lost(scsi_qla_host_t *vha, int defer)
{
	fc_port_t *fcport;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (vha->vp_idx != fcport->vp_idx)
			continue;
		/*
		 * No point in marking the device as lost, if the device is
		 * already DEAD.
		 */
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD)
			continue;
		if (atomic_read(&fcport->state) == FCS_ONLINE) {
			atomic_set(&fcport->state, FCS_DEVICE_LOST);
			qla2x00_schedule_rport_del(vha, fcport, defer);
		} else
			atomic_set(&fcport->state, FCS_DEVICE_LOST);
	}
}

/*
* qla2x00_mem_alloc
*      Allocates adapter memory.
*
* Returns:
*      0  = success.
*      1  = failure.
*/
uint8_t
qla2x00_mem_alloc(struct qla_hw_data *ha, uint16_t req_len, uint16_t rsp_len,
	struct req_que **req, struct rsp_que **rsp)
{
	char	name[16];
	uint8_t   status = 1;
	int	retry= 10;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	do {
		/*
		 * This will loop only once if everything goes well, else some
		 * number of retries will be performed to get around a kernel
		 * bug where available mem is not allocated until after a
		 * little delay and a retry.
		 */

		ha->gid_list = dma_alloc_coherent(&ha->pdev->dev, GID_LIST_SIZE,
		    &ha->gid_list_dma, GFP_KERNEL);
		if (ha->gid_list == NULL) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - gid_list\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}

		ha->rlc_rsp = dma_alloc_coherent(&ha->pdev->dev, 
		    sizeof(rpt_lun_cmd_rsp_t), &ha->rlc_rsp_dma, GFP_KERNEL);
		if (ha->rlc_rsp == NULL) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - rlc_rsp\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}

		/* get consistent memory allocated for init control block */
		ha->init_cb = dma_alloc_coherent(&ha->pdev->dev,
			ha->init_cb_size, &ha->init_cb_dma, GFP_KERNEL);
		if (ha->init_cb == NULL) {
			qla_printk(KERN_WARNING, ha,
		 		"Memory Allocation failed - init_cb\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}
		memset(ha->init_cb, 0, ha->init_cb_size);

		snprintf(name, sizeof(name), "%s", QLA2XXX_DRIVER_NAME);
		ha->s_dma_pool = dma_pool_create(name, &ha->pdev->dev,
		    DMA_POOL_SIZE, 8, 0);
		if (ha->s_dma_pool == NULL) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - s_dma_pool\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}

		if (qla2x00_allocate_sp_pool(ha)) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - "
			    "qla2x00_allocate_sp_pool()\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}

		/* Allocate memory for SNS commands */
		if (IS_QLA2100(ha) || IS_QLA2200(ha)) {
			/* Get consistent memory allocated for SNS commands */
			ha->sns_cmd = dma_alloc_coherent(&ha->pdev->dev,
			    sizeof(struct sns_cmd_pkt), &ha->sns_cmd_dma,
			    GFP_KERNEL);
			if (ha->sns_cmd == NULL) {
				/* error */
				qla_printk(KERN_WARNING, ha,
				    "Memory Allocation failed - sns_cmd\n");

				qla2x00_mem_free(ha);
				msleep(100);

				continue;
			}
			memset(ha->sns_cmd, 0, sizeof(struct sns_cmd_pkt));
		} else {
			/* Get consistent memory allocated for MS IOCB */
			ha->ms_iocb = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL,
			    &ha->ms_iocb_dma);
			if (ha->ms_iocb == NULL) {
				/* error */
				qla_printk(KERN_WARNING, ha,
				    "Memory Allocation failed - ms_iocb\n");

				qla2x00_mem_free(ha);
				msleep(100);

				continue;
			}
			memset(ha->ms_iocb, 0, sizeof(ms_iocb_entry_t));

			/*
			 * Get consistent memory allocated for CT SNS
			 * commands
			 */
			ha->ct_sns = dma_alloc_coherent(&ha->pdev->dev,
			    sizeof(struct ct_sns_pkt), &ha->ct_sns_dma,
			    GFP_KERNEL);
			if (ha->ct_sns == NULL) {
				/* error */
				qla_printk(KERN_WARNING, ha,
				    "Memory Allocation failed - ct_sns\n");

				qla2x00_mem_free(ha);
				msleep(100);

				continue;
			}
			memset(ha->ct_sns, 0, sizeof(struct ct_sns_pkt));

			if (IS_FWI2_CAPABLE(ha)) {
				/*
				 * Get consistent memory allocated for SFP
				 * block.
				 */
				ha->sfp_data = dma_pool_alloc(ha->s_dma_pool,
				    GFP_KERNEL, &ha->sfp_data_dma);
				if (ha->sfp_data == NULL) {
					qla_printk(KERN_WARNING, ha,
					    "Memory Allocation failed - "
					    "sfp_data\n");

					qla2x00_mem_free(ha);
					msleep(100);

					continue;
				}
				memset(ha->sfp_data, 0, SFP_BLOCK_SIZE);
			}
		}

		/* Get memory for cached NVRAM */
		ha->nvram = kzalloc(MAX_NVRAM_SIZE, GFP_KERNEL);
		if (ha->nvram == NULL) {
			/* error */
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - nvram cache\n");

			qla2x00_mem_free(ha);
			msleep(100);

			continue;
		}

		/* Allocate memory for request ring */
		*req = kzalloc(sizeof(struct req_que), GFP_KERNEL);
		if (!*req) {
			DEBUG(printk("Unable to allocate memory for req\n"));
			qla2x00_mem_free(ha);
			msleep(100);
			continue;
		}
		printk("allocated req \n");
		(*req)->length = req_len;
		(*req)->ring = dma_alloc_coherent(&ha->pdev->dev,
				((*req)->length + 1) * sizeof(request_t),
				&(*req)->dma, GFP_KERNEL);
		if (!(*req)->ring) {
			DEBUG(printk("Unable to allocate memory for req_ring\n"));
			kfree(*req);
			qla2x00_mem_free(ha);
			msleep(100);
			continue;
		}
		/* Allocate memory for response ring */
		*rsp = kzalloc(sizeof(struct rsp_que), GFP_KERNEL);
		if (!rsp) {
			DEBUG(printk("Unable to allocate memory for rsp\n"));
			kfree(*req);
			dma_free_coherent(&ha->pdev->dev, 
				((*req)->length + 1) * sizeof(request_t),
				(*req)->ring, (*req)->dma);
			qla2x00_mem_free(ha);
			msleep(100);
			continue;
		}
		printk("allocated rsp \n");
		(*rsp)->hw = ha;
		(*rsp)->length = rsp_len;
		(*rsp)->ring = dma_alloc_coherent(&ha->pdev->dev,
				((*rsp)->length + 1) * sizeof(response_t),
				&(*rsp)->dma, GFP_KERNEL);
		if (!(*rsp)->ring) {
			DEBUG(printk("Unable to allocate memory for rsp_ring\n"));
			kfree(*req);
			dma_free_coherent(&ha->pdev->dev, 
				((*req)->length + 1) * sizeof(request_t),
				(*req)->ring, (*req)->dma);
			kfree(*rsp);
			qla2x00_mem_free(ha);
			msleep(100);
			continue;
		}
        	(*req)->rsp = *rsp;
		(*rsp)->req = *req;
		if (ha->nvram_npiv_size) {
			ha->npiv_info = kzalloc(sizeof(struct qla_npiv_entry) *
				ha->nvram_npiv_size, GFP_KERNEL);
			if (!ha->npiv_info) {
				qla_printk(KERN_WARNING, ha,
					"Unable to allocate memory for npiv info\n");
				kfree(*req);
				dma_free_coherent(&ha->pdev->dev, 
					((*req)->length + 1) * sizeof(request_t),
					(*req)->ring, (*req)->dma);
					
				kfree(*rsp);
				dma_free_coherent(&ha->pdev->dev,
					((*rsp)->length + 1) * sizeof(response_t),
					(*rsp)->ring, (*rsp)->dma);
				qla2x00_mem_free(ha);
				msleep(100);
			}
		} else
			ha->npiv_info = NULL;

 		/* Get consistent memory allocated for EX-INIT-CB. */
		if (IS_QLA81XX(ha)) {
 			ha->ex_init_cb = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL,
 		    	    &ha->ex_init_cb_dma);
 			if (!ha->ex_init_cb) {
				/* error */
				qla_printk(KERN_WARNING, ha,
				    "Memory Allocation failed - ex_init_cb\n");

				qla2x00_mem_free(ha);
				msleep(100);

				continue;
			}
 		}

		/* Allocate ioctl related memory. */
		if (qla2x00_alloc_ioctl_mem(base_vha)) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - ioctl_mem\n");

			qla2x00_mem_free(ha);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(HZ/10);

			continue;
		}
 
		/* Done all allocations without any error. */
		status = 0;

	} while (retry-- && status != 0);

	if (status) {
		printk(KERN_WARNING
			"%s(): **** FAILED ****\n", __func__);
	}

	return(status);
}

/*
* qla2x00_mem_free
*      Frees all adapter allocated memory.
*
* Input:
*      ha = adapter block pointer.
*/
void
qla2x00_mem_free(struct qla_hw_data *ha)
{
	if (ha == NULL) {
		/* error */
		DEBUG2(printk("%s(): ERROR invalid ha pointer.\n", __func__));
		return;
	}

	/* free sp pool */
	qla2x00_free_sp_pool(ha);

	if (ha->fw_dump) {
		if (ha->eft)
			dma_free_coherent(&ha->pdev->dev,
			    ntohl(ha->fw_dump->eft_size), ha->eft, ha->eft_dma);
		vfree(ha->fw_dump);
	}

	if (ha->dcbx_tlv)
		dma_free_coherent(&ha->pdev->dev, DCBX_TLV_DATA_SIZE,
		    ha->dcbx_tlv, ha->dcbx_tlv_dma);

	if (ha->xgmac_data)
		dma_free_coherent(&ha->pdev->dev, XGMAC_DATA_SIZE,
		    ha->xgmac_data, ha->xgmac_data_dma);

	if (ha->sns_cmd)
		dma_free_coherent(&ha->pdev->dev, sizeof(struct sns_cmd_pkt),
		    ha->sns_cmd, ha->sns_cmd_dma);

	if (ha->ct_sns)
		dma_free_coherent(&ha->pdev->dev, sizeof(struct ct_sns_pkt),
		    ha->ct_sns, ha->ct_sns_dma);

	if (ha->sfp_data)
		dma_pool_free(ha->s_dma_pool, ha->sfp_data, ha->sfp_data_dma);

 	if (ha->edc_data)
 		dma_pool_free(ha->s_dma_pool, ha->edc_data, ha->edc_data_dma);
 
	if (ha->ms_iocb)
		dma_pool_free(ha->s_dma_pool, ha->ms_iocb, ha->ms_iocb_dma);

 	if (ha->ex_init_cb)
 		dma_pool_free(ha->s_dma_pool, ha->ex_init_cb, ha->ex_init_cb_dma);

	if (ha->s_dma_pool)
		dma_pool_destroy(ha->s_dma_pool);

	if (ha->init_cb)
		dma_free_coherent(&ha->pdev->dev, ha->init_cb_size,
		    ha->init_cb, ha->init_cb_dma);

	if (ha->rlc_rsp)
		dma_free_coherent(&ha->pdev->dev, sizeof(rpt_lun_cmd_rsp_t),
		    ha->rlc_rsp, ha->rlc_rsp_dma);

	if (ha->gid_list)
		dma_free_coherent(&ha->pdev->dev, GID_LIST_SIZE, ha->gid_list,
		    ha->gid_list_dma);

	ha->eft = NULL;
	ha->eft_dma = 0;
	ha->sns_cmd = NULL;
	ha->sns_cmd_dma = 0;
	ha->ct_sns = NULL;
	ha->ct_sns_dma = 0;
	ha->ms_iocb = NULL;
	ha->ms_iocb_dma = 0;
	ha->init_cb = NULL;
	ha->init_cb_dma = 0;
	ha->ex_init_cb = NULL;
	ha->ex_init_cb_dma = 0;

	ha->s_dma_pool = NULL;

	ha->rlc_rsp = NULL;
	ha->rlc_rsp_dma = 0;

	ha->gid_list = NULL;
	ha->gid_list_dma = 0;

	ha->fw_dump = NULL;
	ha->fw_dumped = 0;
	ha->fw_dump_reading = 0;

	kfree(ha->nvram);
	kfree(ha->npiv_info);
}

/*
 * qla2x00_allocate_sp_pool
 * 	 This routine is called during initialization to allocate
 *  	 memory for local srb_t.
 *
 * Input:
 *	 ha   = adapter block pointer.
 *
 * Context:
 *      Kernel context.
 */
static int
qla2x00_allocate_sp_pool(struct qla_hw_data *ha)
{
	int      rval;

	rval = QLA_SUCCESS;
	ha->srb_mempool = mempool_create_slab_pool(SRB_MIN_REQ,
				(struct kmem_cache *)srb_cachep);
	if (ha->srb_mempool == NULL) {
		qla_printk(KERN_INFO, ha, "Unable to allocate SRB mempool.\n");
		rval = QLA_FUNCTION_FAILED;
	}
	return (rval);
}

/*
 *  This routine frees all adapter allocated memory.
 *
 */
static void
qla2x00_free_sp_pool(struct qla_hw_data *ha)
{
	if (ha->srb_mempool) {
		mempool_destroy(ha->srb_mempool);
		ha->srb_mempool = NULL;
	}
}

static struct qla_work_evt *
qla2x00_alloc_work(struct scsi_qla_host *vha, enum qla_work_type type)
{
	struct qla_work_evt *e;

	e = kzalloc(sizeof(struct qla_work_evt), GFP_ATOMIC);
	if (!e)
		return NULL;

	INIT_LIST_HEAD(&e->list);
	e->type = type;
	e->flags = QLA_EVT_FLAG_FREE;
	return e;
}

static int
qla2x00_post_work(struct scsi_qla_host *vha, struct qla_work_evt *e)
{
	unsigned long	flags = 0;
	struct qla_hw_data *ha = vha->hw;

	spin_lock_irqsave(&ha->work_lock, flags);
	list_add_tail(&e->list, &vha->work_list);
	qla2xxx_wake_dpc(vha);
	spin_unlock_irqrestore(&ha->work_lock, flags);
	return QLA_SUCCESS;
}

int
qla2x00_post_idc_ack_work(struct scsi_qla_host *vha, uint16_t *mb)
{
	struct qla_work_evt *e;

	e = qla2x00_alloc_work(vha, QLA_EVT_IDC_ACK);
	if (!e)
		return QLA_FUNCTION_FAILED;

	memcpy(e->u.idc_ack.mb, mb, QLA_IDC_ACK_REGS * sizeof(uint16_t));
	return qla2x00_post_work(vha, e);
}

static void
qla2x00_do_work(struct scsi_qla_host *vha)
{
	struct qla_work_evt *e = NULL, *tmp = NULL;
	struct qla_hw_data *ha = vha->hw;
	unsigned long flags;
	LIST_HEAD(work);

	spin_lock_irqsave(&ha->work_lock, flags);
	list_splice_init(&vha->work_list, &work);
	spin_unlock_irqrestore(&ha->work_lock, flags);

	list_for_each_entry_safe(e, tmp, &work, list) {
		list_del_init(&e->list);

		switch (e->type) {
		case QLA_EVT_AEN:
			fc_host_post_event(vha->host, fc_get_event_number(),
				e->u.aen.code, e->u.aen.data);
			break;
 		case QLA_EVT_IDC_ACK:
 			qla81xx_idc_ack(vha, e->u.idc_ack.mb);
			break;
		}
		if (e->flags & QLA_EVT_FLAG_FREE)
			kfree(e);
	}
}

/**************************************************************************
* qla2x00_do_dpc
*   This kernel thread is a task that is schedule by the interrupt handler
*   to perform the background processing for interrupts.
*
* Notes:
* This task always run in the context of a kernel thread.  It
* is kick-off by the driver's detect code and starts up
* up one per adapter. It immediately goes to sleep and waits for
* some fibre event.  When either the interrupt handler or
* the timer routine detects a event it will one of the task
* bits then wake us up.
**************************************************************************/
static int
qla2x00_do_dpc(void *data)
{
	int		rval;
	scsi_qla_host_t *vp = NULL, *tvha = NULL;
	fc_port_t *fcport = NULL, *tfcport = NULL;
	struct qla_hw_data *ha;
	scsi_qla_host_t *base_vha;
	unsigned long flags;
	LIST_HEAD(vp_del_list);

	ha = (struct qla_hw_data *)data;
	base_vha = pci_get_drvdata(ha->pdev);

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {
		DEBUG3(printk("qla2x00: DPC handler sleeping\n"));

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		__set_current_state(TASK_RUNNING);

		DEBUG3(printk("qla2x00: DPC handler waking up\n"));

		/* Initialization not yet finished. Don't do anything yet. */
		if (!base_vha->flags.init_done)
			continue;

		DEBUG3(printk("scsi(%ld): DPC handler\n", base_vha->host_no));

		base_vha->dpc_active = 1;

		if (ha->flags.mbox_busy) {
			base_vha->dpc_active = 0;
			continue;
		}

		qla2x00_do_work(base_vha);

		if (test_and_clear_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) {

			DEBUG(printk("scsi(%ld): dpc: sched "
			    "qla2x00_abort_isp ha = %p\n",
			    base_vha->host_no, ha));
			if (!(test_and_set_bit(ABORT_ISP_ACTIVE,
			    &base_vha->dpc_flags))) {

				if (qla2x00_abort_isp(base_vha)) {
					/* failed. retry later */
					set_bit(ISP_ABORT_NEEDED,
					    &base_vha->dpc_flags);
				}
				clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
			}

			DEBUG(printk("scsi(%ld): dpc: qla2x00_abort_isp end\n",
			    base_vha->host_no));
		}

		/* 
		 * When a vport delete call comes down, the last thread accessing
		 * the vport invokes the release callback tied with the vport
		 * referencing. This can happen from both the interrupt and the 
		 * process context. Some of the up calls that we need to make to
		 * cleanup the vport data structures cannot be invoked from the
		 * interrupt context. To avoid it, we defer part of the cleanup
		 * to complete in the process context i.e the dpc thread.
		 */
		if (test_and_clear_bit(VPORT_CLEANUP_NEEDED, &base_vha->dpc_flags)) {
		/* 
		 * Move all the deferred vports into the local delete list.
		 * We do this to minimize vport_lock contention.
		 */
			spin_lock_irqsave(&ha->vport_lock, flags);
			list_splice_init(&ha->vp_del_list, &vp_del_list);
			spin_unlock_irqrestore(&ha->vport_lock, flags);
        
			/* Cleanup the vport data structure. */
			list_for_each_entry_safe(vp, tvha, &vp_del_list, list) {
			/* 
			 * Freeup target data structures associated with the current 
			 * virtual port.
			 */
				list_for_each_entry_safe(fcport, tfcport, &vp->vp_fcports, list) {
					list_del_init(&fcport->list);
					kfree(fcport);
					fcport = NULL;
				}

				/* Free ioctl memory */
				qla2x00_free_ioctl_mem(vp);

				if (vp->fcp_prio_cfg) {
					vfree(vp->fcp_prio_cfg);
					vp->fcp_prio_cfg = NULL;
				}

				list_del_init(&vp->list);
				scsi_host_put(vp->host);
			}
		}

		if (test_and_clear_bit(FCPORT_UPDATE_NEEDED, &base_vha->dpc_flags))
			qla2x00_update_fcports(base_vha);

		if (test_and_clear_bit(LOOP_RESET_NEEDED, &base_vha->dpc_flags)) {
			DEBUG(printk("scsi(%ld): dpc: sched loop_reset()\n",
			    base_vha->host_no));
			qla2x00_loop_reset(base_vha);
		}

		if (test_and_clear_bit(RESET_MARKER_NEEDED, &base_vha->dpc_flags) &&
		    (!(test_and_set_bit(RESET_ACTIVE, &base_vha->dpc_flags)))) {

			DEBUG(printk("scsi(%ld): qla2x00_reset_marker()\n",
			    base_vha->host_no));

			qla2x00_rst_aen(base_vha);
			clear_bit(RESET_ACTIVE, &base_vha->dpc_flags);
		}

		/* Retry each device up to login retry count */
		if ((test_and_clear_bit(RELOGIN_NEEDED, &base_vha->dpc_flags)) &&
		    !test_bit(LOOP_RESYNC_NEEDED, &base_vha->dpc_flags) &&
		    atomic_read(&base_vha->loop_state) != LOOP_DOWN) {

			DEBUG(printk("scsi(%ld): qla2x00_port_login()\n",
			    base_vha->host_no));

			qla2x00_relogin(base_vha);
			DEBUG(printk("scsi(%ld): qla2x00_port_login - end\n",
			    base_vha->host_no));
		}

		if ((test_bit(LOGIN_RETRY_NEEDED, &base_vha->dpc_flags)) &&
		    atomic_read(&base_vha->loop_state) != LOOP_DOWN) {

			clear_bit(LOGIN_RETRY_NEEDED, &base_vha->dpc_flags);
			DEBUG(printk("scsi(%ld): qla2x00_login_retry()\n",
			    base_vha->host_no));

			set_bit(LOOP_RESYNC_NEEDED, &base_vha->dpc_flags);

			DEBUG(printk("scsi(%ld): qla2x00_login_retry - end\n",
			    base_vha->host_no));
		}

		if (test_and_clear_bit(LOOP_RESYNC_NEEDED, &base_vha->dpc_flags)) {

			DEBUG(printk("scsi(%ld): qla2x00_loop_resync()\n",
			    base_vha->host_no));

			if (!(test_and_set_bit(LOOP_RESYNC_ACTIVE,
			    &base_vha->dpc_flags))) {

				rval = qla2x00_loop_resync(base_vha);

				clear_bit(LOOP_RESYNC_ACTIVE, &base_vha->dpc_flags);
			}

			DEBUG(printk("scsi(%ld): qla2x00_loop_resync - end\n",
			    base_vha->host_no));
		}

		/* 
		 * Get ready to send out aen 
		 * notification to the application.
		 */
		qla2x00_aen_flush(base_vha);

		if (!ha->interrupts_on)
			ha->isp_ops->enable_intrs(ha);

		if (test_and_clear_bit(BEACON_BLINK_NEEDED, &base_vha->dpc_flags))
			ha->isp_ops->beacon_blink(base_vha);

		/* Physical port's dpc handling has completed. */
		base_vha->dpc_active = 0;

		qla2x00_do_dpc_all_vps(base_vha);

	} /* End of while(1) */

	DEBUG(printk("scsi(%ld): DPC handler exiting\n", base_vha->host_no));

	/*
	 * Make sure that nobody tries to wake us up again.
	 */
	base_vha->dpc_active = 0;

	return 0;
}

void qla2x00_relogin(struct scsi_qla_host *vha)
{
        fc_port_t       *fcport;
 	int 		status;
        uint16_t        next_loopid = 0;
        struct qla_hw_data *ha = vha->hw;

        list_for_each_entry(fcport, &vha->vp_fcports, list) {
        /*
         * If the port is not ONLINE then try to login
         * to it if we haven't run out of retries.
         */
                if (atomic_read(&fcport->state) !=
                        FCS_ONLINE && fcport->login_retry) {

                        if (fcport->flags & FCF_FABRIC_DEVICE) {
                                if (fcport->flags & FCF_TAPE_PRESENT)
                                        ha->isp_ops->fabric_logout(vha,
                                                        fcport->loop_id,
                                                        fcport->d_id.b.domain,
                                                        fcport->d_id.b.area,
                                                        fcport->d_id.b.al_pa);

                                status = qla2x00_fabric_login(vha, fcport,
                                                        &next_loopid);
                        } else
                                status = qla2x00_local_device_login(vha,
                                                                fcport);

                        fcport->login_retry--;
                        if (status == QLA_SUCCESS) {
                                fcport->old_loop_id = fcport->loop_id;

                                DEBUG(printk("scsi(%ld): port login OK: logged "
                                "in ID 0x%x\n", vha->host_no, fcport->loop_id));

                                qla2x00_update_fcport(vha, fcport);

                        } else if (status == 1) {
                                set_bit(RELOGIN_NEEDED, &vha->dpc_flags);
                                /* retry the login again */
                                DEBUG(printk("scsi(%ld): Retrying"
                                " %d login again loop_id 0x%x\n",
                                vha->host_no, fcport->login_retry,
                                                fcport->loop_id));
                        } else {
                                fcport->login_retry = 0;
                        }

                        if (fcport->login_retry == 0 && status != QLA_SUCCESS)
                                fcport->loop_id = FC_NO_LOOP_ID;
                }
                if (test_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags))
                        break;
        }
}

void
qla2xxx_wake_dpc(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	if (ha->dpc_thread)
		wake_up_process(ha->dpc_thread);
}

/*
*  qla2x00_rst_aen
*      Processes asynchronous reset.
*
* Input:
*      ha  = adapter block pointer.
*/
static void
qla2x00_rst_aen(scsi_qla_host_t *vha)
{
	if (vha->flags.online && !vha->flags.reset_active &&
	    !atomic_read(&vha->loop_down_timer) &&
	    !(test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags))) {
		do {
			clear_bit(RESET_MARKER_NEEDED, &vha->dpc_flags);

			/*
			 * Issue marker command only when we are going to start
			 * the I/O.
			 */
			vha->marker_needed = 1;
		} while (!atomic_read(&vha->loop_down_timer) &&
		    (test_bit(RESET_MARKER_NEEDED, &vha->dpc_flags)));
	}
}

void
qla2x00_sp_free_dma(struct qla_hw_data *ha, srb_t *sp)
{
	struct scsi_cmnd *cmd = sp->cmd;

	if (sp->flags & SRB_DMA_VALID) {
#if defined(__VMKLNX__)
        if (scsi_sg_count(cmd)) {
            dma_unmap_sg(&ha->pdev->dev, scsi_sglist(cmd), scsi_sg_count(cmd),
                cmd->sc_data_direction);
        }
#else
		scsi_dma_unmap(cmd);
#endif
		sp->flags &= ~SRB_DMA_VALID;
	}
	CMD_SP(cmd) = NULL;
}

void
qla2x00_sp_compl(struct qla_hw_data *ha, srb_t *sp)
{
	struct scsi_cmnd *cmd = sp->cmd;

	/* Deleting the timer for ioctl. */
	if ((sp->flags & SRB_IOCTL))
		del_timer(&sp->timer);
	/* Host Statistics */
	if (host_byte(cmd->result) == DID_ERROR)
		ha->total_dev_errs++;

	qla2x00_sp_free_dma(ha, sp);

	mempool_free(sp, ha->srb_mempool);

	cmd->scsi_done(cmd);
}

/**************************************************************************
*   qla2x00_timer
*
* Description:
*   One second timer
*
* Context: Interrupt
***************************************************************************/
void
qla2x00_timer(scsi_qla_host_t *vha)
{
	unsigned long	cpu_flags = 0;
	fc_port_t	*fcport;
	int		start_dpc = 0;
	int		index;
	srb_t		*sp;
	int		t;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	if(vha->vp_idx && test_bit(VP_DELETE_ACTIVE, &vha->dpc_flags)) {
		return;
	}
	/*
	 * Ports - Port down timer.
	 *
	 * Whenever, a port is in the LOST state we start decrementing its port
	 * down timer every second until it reaches zero. Once  it reaches zero
	 * the port it marked DEAD.
	 */
	t = 0;
	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (fcport->port_type != FCT_TARGET)
			continue;

		if (atomic_read(&fcport->state) == FCS_DEVICE_LOST) {

			if (atomic_read(&fcport->port_down_timer) == 0)
				continue;

			if (atomic_dec_and_test(&fcport->port_down_timer) != 0)
				atomic_set(&fcport->state, FCS_DEVICE_DEAD);

			DEBUG(printk("scsi(%ld): fcport-%d - port retry count: "
				"%d remaining\n",
				vha->host_no, t,
				atomic_read(&fcport->port_down_timer)));
		}
		t++;
	} /* End of for fcport  */


	/* Loop down handler. */
	if (atomic_read(&vha->loop_down_timer) > 0 &&
	    !(test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags)) && vha->flags.online) {

		if (atomic_read(&vha->loop_down_timer) ==
		    ha->loop_down_abort_time) {

			DEBUG(printk("scsi(%ld): Loop Down - aborting the "
			    "queues before time expire\n",
			    vha->host_no));

			if (!IS_QLA2100(ha) && ha->link_down_timeout)
				atomic_set(&vha->loop_state, LOOP_DEAD);

			/* Schedule an ISP abort to return any tape commands. */
			/* NPIV - scan physical port only */
			if (!vha->vp_idx) {
				spin_lock_irqsave(&ha->hardware_lock, cpu_flags);
				req = ha->req_q_map[0];
				for (index = 1;
				    index < MAX_OUTSTANDING_COMMANDS;
				    index++) {
					fc_port_t *sfcp;

					sp = req->outstanding_cmds[index];
					if (!sp)
						continue;
					sfcp = sp->fcport;
					if (!(sfcp->flags & FCF_TAPE_PRESENT))
						continue;

					set_bit(ISP_ABORT_NEEDED,
					    &vha->dpc_flags);
					break;
				}
				spin_unlock_irqrestore(&ha->hardware_lock,
				    cpu_flags);
			}
			start_dpc++;
		}

		/* if the loop has been down for 4 minutes, reinit adapter */
		if (atomic_dec_and_test(&vha->loop_down_timer) != 0) {

			if (!(vha->device_flags & DFLG_NO_CABLE) && !vha->vp_idx) {
				DEBUG(printk("scsi(%ld): Loop down - "
				    "aborting ISP.\n",
				    vha->host_no));
				qla_printk(KERN_WARNING, ha,
				    "Loop down - aborting ISP.\n");

				set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
			}
		}
		DEBUG3(printk("scsi(%ld): Loop Down - seconds remaining %d\n",
		    vha->host_no,
		    atomic_read(&vha->loop_down_timer)));
	}

	/* Check if beacon LED needs to be blinked */
	if (ha->beacon_blink_led == 1) {
		set_bit(BEACON_BLINK_NEEDED, &vha->dpc_flags);
		start_dpc++;
	}

	/* Schedule the DPC routine if needed */
	if ((test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
	    test_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags) ||
	    test_bit(LOOP_RESET_NEEDED, &vha->dpc_flags) ||
	    test_bit(FCPORT_UPDATE_NEEDED, &vha->dpc_flags) ||
	    start_dpc ||
	    test_bit(LOGIN_RETRY_NEEDED, &vha->dpc_flags) ||
	    test_bit(RESET_MARKER_NEEDED, &vha->dpc_flags) ||
	    test_bit(BEACON_BLINK_NEEDED, &vha->dpc_flags) ||
	    test_bit(VP_DPC_NEEDED, &vha->dpc_flags) ||
	    test_bit(RELOGIN_NEEDED, &vha->dpc_flags)))
		qla2xxx_wake_dpc(vha);

	if (!test_bit(VP_DELETE_ACTIVE, &vha->dpc_flags))
		qla2x00_restart_timer(vha, WATCH_INTERVAL);
}

/* XXX(hch): crude hack to emulate a down_timeout() */
int
qla2x00_down_timeout(struct semaphore *sema, unsigned long timeout)
{
	const unsigned int step = 100; /* msecs */
	unsigned int iterations = jiffies_to_msecs(timeout)/100;

	do {
		if (!down_trylock(sema))
			return 0;
		if (msleep_interruptible(step))
			break;
	} while (--iterations > 0);

	return -ETIMEDOUT;
}

/* Firmware interface routines. */

#define FW_BLOBS	6	
#define FW_ISP21XX	0
#define FW_ISP22XX	1
#define FW_ISP2300	2
#define FW_ISP2322	3
#define FW_ISP24XX	4
#define FW_ISP25XX	5
#define FW_ISP81XX	6

static DEFINE_MUTEX(qla_fw_lock);

/* Bind firmware with driver in VMKLNX */
extern struct firmware ql2100_fw, ql2200_fw, ql2300_fw, ql2322_fw, ql2400_fw, ql2500_fw;

static struct fw_blob qla_fw_blobs[FW_BLOBS] = {
    { .name = "ql2100_fw.bin", .segs = { 0x1000, 0 }, .fw = &ql2100_fw },
    { .name = "ql2200_fw.bin", .segs = { 0x1000, 0 }, .fw = &ql2200_fw },
    { .name = "ql2300_fw.bin", .segs = { 0x800, 0 }, .fw = &ql2300_fw },
    { .name = "ql2322_fw.bin", .segs = { 0x800, 0x1c000, 0x1e000, 0 }, .fw = &ql2322_fw },
    { .name = "ql2400_fw.bin", .fw = &ql2400_fw },
    { .name = "ql2500_fw.bin", .fw = &ql2500_fw },
};

struct fw_blob *
qla2x00_request_firmware(scsi_qla_host_t *vha)
{
	struct fw_blob *blob;
	struct  qla_hw_data *ha = vha->hw;
	blob = NULL;
	if (IS_QLA2100(ha)) {
		blob = &qla_fw_blobs[FW_ISP21XX];
	} else if (IS_QLA2200(ha)) {
		blob = &qla_fw_blobs[FW_ISP22XX];
	} else if (IS_QLA2300(ha) || IS_QLA2312(ha) || IS_QLA6312(ha)) {
		blob = &qla_fw_blobs[FW_ISP2300];
	} else if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
		blob = &qla_fw_blobs[FW_ISP2322];
	} else if (IS_QLA24XX_TYPE(ha)) {
		blob = &qla_fw_blobs[FW_ISP24XX];
	} else if (IS_QLA25XX(ha)) {
		blob = &qla_fw_blobs[FW_ISP25XX];
	}

	mutex_lock(&qla_fw_lock);
	if (blob->fw)
		goto out;

    printk("scsi(%ld): Failed to load firmware image "
        "(%s).\n", vha->host_no, blob->name);

out:
	mutex_unlock(&qla_fw_lock);
	return blob;
}

static void
qla2x00_release_firmware(void)
{
#if !defined(__VMKLNX__)
	int idx;

	mutex_lock(&qla_fw_lock);
	for (idx = 0; idx < FW_BLOBS; idx++)
		if (qla_fw_blobs[idx].fw)
			release_firmware(qla_fw_blobs[idx].fw);
	mutex_unlock(&qla_fw_lock);
#endif
}

static pci_ers_result_t
qla2xxx_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		qla2x00_abort_all_cmds(base_vha, DID_NO_CONNECT << 16);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t
qla2xxx_pci_mmio_enabled(struct pci_dev *pdev)
{
	int risc_paused = 0;
	uint32_t stat;
	unsigned long flags;
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	struct device_reg_24xx __iomem *reg24 = &ha->iobase->isp24;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	if (IS_QLA2100(ha) || IS_QLA2200(ha)){
		stat = RD_REG_DWORD(&reg->hccr);
		if (stat & HCCR_RISC_PAUSE)
			risc_paused = 1;
	} else if (IS_QLA23XX(ha)) {
		stat = RD_REG_DWORD(&reg->u.isp2300.host_status);
		if (stat & HSR_RISC_PAUSED)
			risc_paused = 1;
	} else if (IS_FWI2_CAPABLE(ha)) {
		stat = RD_REG_DWORD(&reg24->host_status);
		if (stat & HSRX_RISC_PAUSED)
			risc_paused = 1;
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	if (risc_paused) {
		qla_printk(KERN_INFO, ha, "RISC paused -- mmio_enabled, "
		    "Dumping firmware!\n");
		ha->isp_ops->fw_dump(base_vha, 0);

		return PCI_ERS_RESULT_NEED_RESET;
	} else
		return PCI_ERS_RESULT_RECOVERED;
}

static pci_ers_result_t
qla2xxx_pci_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t ret = PCI_ERS_RESULT_DISCONNECT;
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;

	if (pci_enable_device(pdev)) {
		qla_printk(KERN_WARNING, ha,
		    "Can't re-enable PCI device after reset.\n");

		return ret;
	}
	pci_set_master(pdev);

	if (ha->isp_ops->pci_config(base_vha))
		return ret;

	set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
	if (qla2x00_abort_isp(base_vha)== QLA_SUCCESS)
		ret =  PCI_ERS_RESULT_RECOVERED;
	clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);

	return ret;
}

static void
qla2xxx_pci_resume(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	int ret;

	ret = qla2x00_wait_for_hba_online(base_vha);
	if (ret != QLA_SUCCESS) {
		qla_printk(KERN_ERR, ha,
		    "the device failed to resume I/O "
		    "from slot/link_reset");
	}
#if !defined(__VMKLNX__)   /* no support of aer in vmklnx for now */
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif
}

static struct pci_error_handlers qla2xxx_err_handler = {
	.error_detected = qla2xxx_pci_error_detected,
	.mmio_enabled = qla2xxx_pci_mmio_enabled,
	.slot_reset = qla2xxx_pci_slot_reset,
	.resume = qla2xxx_pci_resume,
};

static struct pci_device_id qla2xxx_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2100) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2200) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2300) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2312) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2322) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP6312) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP6322) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2422) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP5422) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP5432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2532) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP8432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP8001) },
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, qla2xxx_pci_tbl);

static struct pci_driver qla2xxx_pci_driver = {
	.name		= QLA2XXX_DRIVER_NAME,
	.driver		= {
		.owner		= THIS_MODULE,
	},
	.id_table	= qla2xxx_pci_tbl,
	.probe		= qla2x00_probe_one,
	.remove		= __devexit_p(qla2x00_remove_one),
	.err_handler	= &qla2xxx_err_handler,
};

/**
 * qla2x00_module_init - Module initialization.
 **/
static int __init
qla2x00_module_init(void)
{
	int ret = 0;

	/* Allocate cache for SRBs. */
#if defined(__VMKLNX__)
	srb_cachep = kmem_cache_create("qla2xxx_srbs", sizeof(srb_t), 0,
	    SLAB_HWCACHE_ALIGN, NULL, NULL);
#else
	srb_cachep = kmem_cache_create("qla2xxx_srbs", sizeof(srb_t), 0,
	    SLAB_HWCACHE_ALIGN, NULL);
#endif

	if (srb_cachep == NULL) {
		printk(KERN_ERR
		    "qla2xxx: Unable to allocate SRB cache...Failing load!\n");
		return -ENOMEM;
	}

	/* Derive version string. */
	strcpy(qla2x00_version_str, QLA2XXX_VERSION);
	if (ql2xextended_error_logging)
		strcat(qla2x00_version_str, "-debug");

	qla2xxx_transport_template =
	    fc_attach_transport(&qla2xxx_transport_functions);
	if (!qla2xxx_transport_template) {
		kmem_cache_destroy(srb_cachep);
		return -ENODEV;
	}
	qla2xxx_transport_vport_template =
	    fc_attach_transport(&qla2xxx_transport_vport_functions);
	if (!qla2xxx_transport_vport_template) {
		kmem_cache_destroy(srb_cachep);
		fc_release_transport(qla2xxx_transport_template);
		return -ENODEV;
	}

	qla2x00_ioctl_init();

	printk(KERN_INFO "QLogic Fibre Channel HBA Driver: %s\n",
	    qla2x00_version_str);
	ret = pci_register_driver(&qla2xxx_pci_driver);
	if (ret) {
		kmem_cache_destroy(srb_cachep);
		fc_release_transport(qla2xxx_transport_template);
		fc_release_transport(qla2xxx_transport_vport_template);
	}
	return ret;
}

/**
 * qla2x00_module_exit - Module cleanup.
 **/
static void __exit
qla2x00_module_exit(void)
{
	pci_unregister_driver(&qla2xxx_pci_driver);
	qla2x00_ioctl_exit();
	qla2x00_release_firmware();
	kmem_cache_destroy(srb_cachep);
	fc_release_transport(qla2xxx_transport_template);
	fc_release_transport(qla2xxx_transport_vport_template);
}

module_init(qla2x00_module_init);
module_exit(qla2x00_module_exit);

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic Fibre Channel HBA Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(QLA2XXX_VERSION);
