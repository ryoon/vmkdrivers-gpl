/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)   2003-2006 QLogic Corporation
 * Portions Copyright 2009-2011 VMware, Inc.
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */
#include <linux/moduleparam.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>

#include <linux/klist.h>
#include "ql4_def.h"
#include "ql4_version.h"
#include "ql4_glbl.h"
#include "ql4_dbg.h"
#include "ql4_inline.h"
#include <scsi/iscsi_proto.h>
#include <scsi/scsi_eh.h>

#ifdef __VMKLNX__
#include <vmklinux_scsi.h>   // For Transport type
#include "ql4im_glbl.h"
#endif

/*
 * Driver version
 */
char qla4xxx_version_str[40];
EXPORT_SYMBOL_GPL(qla4xxx_version_str);

/*
 * List of host adapters
 */
struct klist qla4xxx_hostlist;

struct klist *qla4xxx_hostlist_ptr = &qla4xxx_hostlist;
EXPORT_SYMBOL_GPL(qla4xxx_hostlist_ptr);

static atomic_t qla4xxx_hba_count;

/*
 * SRB allocation cache
 */
static kmem_cache_t *srb_cachep;

/*
 * Module parameter information and variables
 */
int ql4xdiscoverywait = 60;
module_param(ql4xdiscoverywait, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(ql4xdiscoverywait, "Discovery wait time");
int ql4xdontresethba = 0;
module_param(ql4xdontresethba, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(ql4xdontresethba,
       "Dont reset the HBA when the driver gets 0x8002 AEN "
       " default it will reset hba :0"
       " set to 1 to avoid resetting HBA");

int extended_error_logging = 0; /* 0 = off, 1 = log errors */
module_param(extended_error_logging, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(extended_error_logging,
       "Option to enable extended error logging, "
       "Default is 0 - no logging, 1 - debug logging");

#ifdef __VMKLNX__
/* Command Timeout before ddb state goes to MISSING */
int cmd_timeout = 20;
module_param(cmd_timeout, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(cmd_timeout, "Command Timeout");

/* Timeout before ddb state MISSING goes DEAD */
int recovery_tmo = 16;
module_param(recovery_tmo, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(recovery_tmo, "Recovery Timeout");

/* Interval and Timeout used for NOOPs */
int ka_timeout = 14;
module_param(ka_timeout, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(ka_timeout, "Keep Alive Timeout");

/* Timeout used for IOCTLs */
int ioctl_timeout = 10;
module_param(ioctl_timeout, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(ioctl_timeout, "IOCTL Timeout");

/* Enable gratuitous ARP */
int gratuitous_arp = 1;
module_param(gratuitous_arp, int, S_IRUGO | S_IRUSR);
MODULE_PARM_DESC(gratuitous_arp, "Gratuitous ARP");

#define MAX_Q_DEPTH    128
static int ql4xmaxqdepth = MAX_Q_DEPTH;
module_param(ql4xmaxqdepth, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql4xmaxqdepth,
		"Maximum queue depth to report for target devices.");
#endif /* __VMKLNX__ */

int ql4_mod_unload = 0;
/*
 * SCSI host template entry points
 */

void qla4xxx_config_dma_addressing(struct scsi_qla_host *ha);

/*
 * iSCSI template entry points
 */
#ifndef __VMKLNX__
static int qla4xxx_tgt_dscvr(enum iscsi_tgt_dscvr type, uint32_t host_no,
              uint32_t enable, struct sockaddr *dst_addr);
#else
static int qla4xxx_host_get_param(struct Scsi_Host *,
         enum iscsi_host_param, char *);
static int qla4xxx_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
                    off_t offset, int length, int inout);

#endif
static int qla4xxx_conn_get_param(struct iscsi_cls_conn *conn,
              enum iscsi_param param, char *buf);
static int qla4xxx_sess_get_param(struct iscsi_cls_session *sess,
              enum iscsi_param param, char *buf);
static void qla4xxx_recovery_timedout(struct iscsi_cls_session *session);

/*
 * SCSI host template entry points
 */
static int qla4xxx_queuecommand(struct scsi_cmnd *cmd,
            void (*done) (struct scsi_cmnd *));
static int qla4xxx_eh_abort(struct scsi_cmnd *cmd);
static int qla4xxx_eh_device_reset(struct scsi_cmnd *cmd);
static int qla4xxx_eh_host_reset(struct scsi_cmnd *cmd);
static int qla4xxx_slave_alloc(struct scsi_device *device);
static int qla4xxx_slave_configure(struct scsi_device *device);
#ifdef __VMKLNX__
static void qla4xxx_target_destroy(struct scsi_target *starget);
#endif

static struct scsi_host_template qla4xxx_driver_template = {
   .module                  = THIS_MODULE,
   .name                    = DRIVER_NAME,
   .proc_name               = DRIVER_NAME,
#ifdef __VMKLNX__
   .proc_info               = qla4xxx_proc_info,
#endif
   .queuecommand            = qla4xxx_queuecommand,

   .eh_abort_handler        = qla4xxx_eh_abort,
   .eh_device_reset_handler = qla4xxx_eh_device_reset,
   .eh_host_reset_handler   = qla4xxx_eh_host_reset,

   .slave_configure         = qla4xxx_slave_configure,
   .slave_alloc             = qla4xxx_slave_alloc,
#ifdef __VMKLNX__
   .target_destroy          = qla4xxx_target_destroy,
#endif

   .this_id                 = -1,
   .cmd_per_lun             = 3,
   .use_clustering          = ENABLE_CLUSTERING,
   .sg_tablesize            = SG_ALL,

#ifdef __VMKLNX__
   .can_queue 		    = 128,
#else
   .can_queue 		    = REQUEST_QUEUE_DEPTH+128,
#endif

   .max_sectors             = 0xFFFF,
};

static struct iscsi_transport qla4xxx_iscsi_transport = {
   .owner         = THIS_MODULE,
   .name       = DRIVER_NAME,
#ifdef __VMKLNX__
   .description  = "QLogic iSCSI Adapter",
#endif
   .param_mask    = ISCSI_CONN_PORT |
              ISCSI_CONN_ADDRESS |
              ISCSI_TARGET_NAME |
              ISCSI_TPGT,
   .sessiondata_size = sizeof(struct ddb_entry),
   .host_template    = &qla4xxx_driver_template,

#ifndef __VMKLNX__
   .tgt_dscvr      = qla4xxx_tgt_dscvr,
#else
   .get_host_param      = qla4xxx_host_get_param,
#endif
   .get_conn_param      = qla4xxx_conn_get_param,
   .get_session_param   = qla4xxx_sess_get_param,
   .session_recovery_timedout = qla4xxx_recovery_timedout,
#ifdef __VMKLNX__
   .caps = (CAP_DATA_PATH_OFFLOAD | CAP_FW_DB | CAP_RECOVERY_L0 | 
            CAP_HDRDGST | CAP_DATADGST | CAP_SENDTARGETS_OFFLOAD |
            CAP_CONN_CLEANUP),
#endif
};

static struct scsi_transport_template *qla4xxx_scsi_transport;

#ifdef __VMKLNX__
typedef struct{
   const char *name;
   int *value;
}qla4xx_proc_param;

static qla4xx_proc_param proc_param[] = {
   { "extended_error_logging=", &extended_error_logging },
   { "recovery_tmo=", &recovery_tmo },
   { "cmd_timeout=", &cmd_timeout },
   { "ql4xdiscoverywait=", &ql4xdiscoverywait},
   { "ql4xdontresethba=", &ql4xdontresethba },
   { "ka_timeout=", &ka_timeout },
   { "ioctl_timeout=", &ioctl_timeout},
   { NULL, NULL },
};

struct info_str {
        char *buffer;
        int length;
        off_t offset;
        int pos;
};

/*
 * The following support functions are adopted to handle
 * the re-entrant qla4xxx_proc_info correctly.
 */
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

/**************************************************************************
 * qla4xxx_proc_dump_scanned_devices
 * This routine displays information for scanned devices in the proc
 * buffer.
 *
 * Input:
 * info - length of proc buffer prior to this function's execution.
 *
 * Remarks:
 * This routine is dependent on the DISPLAY_SRBS_IN_PROC #define being
 * set to 1.
 *
 * Returns:
 * info - length of proc buffer after this function's execution.
 *
 * Context:
 * Kernel context.
 **************************************************************************/
static inline void
qla4xxx_proc_dump_scanned_devices(struct scsi_qla_host *ha, struct info_str *info)
{
   struct list_head *ptr, *next; /* used for traversing lists */
#if 1 || defined(__VMKERNEL_MODULE__)
   unsigned long flags;
#endif

   copy_info(info, "SCSI Device Information:\n");

#if 1 || defined(__VMKERNEL_MODULE__)
   spin_lock_irqsave(&ha->list_lock, flags);
#endif
   list_for_each_safe(ptr, next, &ha->ddb_list) {
      struct ddb_entry *ddb_entry = list_entry(ptr, struct ddb_entry, list);

	copy_info(info, "ddb 0x%04x: Target=%d \"%s\"\n",
               ddb_entry->fw_ddb_index,
               ddb_entry->os_target_id,
               ddb_entry->iscsi_name);

      if ((ddb_entry->options & DDB_OPT_IPV6_DEVICE) == 0)
	      copy_info(info, "\tIP: "  NIPQUAD_FMT "\n",
		       NIPQUAD(ddb_entry->ip_addr));
      else {
	      copy_info(info, "\tRoutable  IP: "  NIP6_FMT "%s\n",
		       NIP6(ddb_entry->link_local_ipv6_addr),
		       (ddb_entry->options & DDB_OPT_IPV6_NULL_LINK_LOCAL)
			? " " : " (*)");
	      copy_info(info, "\tLinkLocal IP: "  NIP6_FMT "%s\n",
		       NIP6(ddb_entry->remote_ipv6_addr),
		       (ddb_entry->options & DDB_OPT_IPV6_NULL_LINK_LOCAL)
			? " (*)" : " ");
      }

      copy_info(info, "\tport=%d, tpgt=%d, isid=" ISID_FMT  "\n",
               ddb_entry->port, ddb_entry->tpgt, ISID(ddb_entry->isid));

      copy_info(info, "\tfw_state=0x%x, state=0x%x, flags=0x%x, options=0x%x\n",
               ddb_entry->fw_ddb_device_state,
               atomic_read(&ddb_entry->state),
               ddb_entry->flags,
               ddb_entry->options);

      copy_info(info, "\tha=%p sess=%p(%d) conn=%p(%d)\n",
               ddb_entry->ha,
               ddb_entry->sess,
               ddb_entry->target_session_id,
               ddb_entry->conn,
               ddb_entry->connection_id);
      copy_info(info, "\tpd_timer=%d rr_timer=%d(%d) rl_timer=%d(%d) "
               "rlr_count=%d ka_timeout=%d\n",
               atomic_read(&ddb_entry->port_down_timer),
               atomic_read(&ddb_entry->retry_relogin_timer), ddb_entry->default_time2wait,
               atomic_read(&ddb_entry->relogin_timer),ddb_entry->default_relogin_timeout,
               atomic_read(&ddb_entry->relogin_retry_count),
               ddb_entry->ka_timeout);
   }

#if 1 || defined(__VMKERNEL_MODULE__)
   spin_unlock_irqrestore(&ha->list_lock, flags);
#endif
}

/**************************************************************************
 * qla4xxx_proc_info
 * This routine return information to handle /proc support for the driver
 *
 * Input:
 * Output:
 * inout  - Decides on the direction of the dataflow and the meaning of
 *          the variables.
 * buffer - If inout==FALSE data is being written to it else read from
 *          it (ptrs to a page buffer).
 * *start - If inout==FALSE start of the valid data in the buffer.
 * offset - If inout==FALSE offset from the beginning of the imaginary
 *          file from which we start writing into the buffer.
 * length - If inout==FALSE max number of bytes to be written into the
 *          buffer else number of bytes in the buffer.
 * shost  - pointer to the scsi host for this proc node
 *
 * Remarks:
 * None
 *
 * Returns:
 * Size of proc buffer.
 *
 * Context:
 * Kernel context.
 **************************************************************************/
static int
qla4xxx_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
                    off_t offset, int length, int inout)
{
   int i;
   struct  info_str info;
   int retval = -EINVAL;
   struct scsi_qla_host *ha = to_qla_host(shost);

   /* Has data been written to the file?
    * Usage: echo "<cmd-line-param>=<value>" > /proc/scsi/qla4xxx/<adapter-id>,
    * where <adapter-id> = console host id#
    */
   if (inout == TRUE) {
      for (i=0; proc_param[i].name !=NULL; i++) {
         if (strncmp(buffer, proc_param[i].name, strlen(proc_param[i].name)) == 0) {
            sscanf(buffer+strlen(proc_param[i].name), "%d", proc_param[i].value);
            printk("qla4xxx: set parameter %s%d\n", proc_param[i].name, *(proc_param[i].value));
            retval = length;

	    /* Update version string, if applicable */
	    if (strcmp(proc_param[i].name, "extended_error_logging")) {
		    char *debug_str = strstr(qla4xxx_version_str, "-debug");

		    if (extended_error_logging == 0) {
                            if (debug_str != NULL) {
                                    *debug_str = '\0';
				    printk("qla4xxx: driver version string "
					   "changed to \"%s\"\n", qla4xxx_version_str);
			    }
		    } else {
                            if (debug_str == NULL) {
				    strcat(qla4xxx_version_str, "-debug");
				    printk("qla4xxx: driver version string "
					   "changed to \"%s\"\n", qla4xxx_version_str);
			    }
		    }
	    }
            break;
         }
      }
      goto done;
   }

   if (!ha) {
      goto done;
   }

   if (start) {
      *start = buffer;
   }

   info.buffer     = buffer;
   info.length     = length;
   info.offset     = offset;
   info.pos        = 0;

   copy_info(&info, "QLogic iSCSI Adapter\n");
   copy_info(&info, "Driver version %s\n", qla4xxx_version_str);

   copy_info(&info, "Firmware version %2d.%02d.%02d.%02d\n",
        ha->firmware_version[0], ha->firmware_version[1],
        ha->patch_number, ha->build_number);

   copy_info(&info, "SCSI HBA Information:\n", ha->host->io_port);
#ifdef __VMKLNX__
   copy_info(&info, "\t%s\n", vmklnx_get_vmhba_name(ha->host));
#endif
   copy_info(&info, "\tiSCSI Name = \"%s\"\n", ha->name_string);
   if (ha->acb_version == ACB_NOT_SUPPORTED || is_ipv4_enabled(ha)) {
	   copy_info(&info, "\tIPv4 Address = " NIPQUAD_FMT "  (%u)\n",
	      NIPQUAD(ha->ip_address), ha->ipv4_addr_state);
	   copy_info(&info, "\tMask = " NIPQUAD_FMT ", Gateway =  " NIPQUAD_FMT "\n",
              NIPQUAD(ha->subnet_mask), NIPQUAD(ha->gateway));
   }
   if (ha->acb_version == ACB_SUPPORTED && is_ipv6_enabled(ha))	{
	   copy_info(&info, "\tIPv6 LinkLocalAddr = " NIP6_FMT " (%u)\n",
	      NIP6(ha->ipv6_link_local_addr), ha->ipv6_link_local_state);
	   copy_info(&info, "\tIPv6 RoutableAddr0 = " NIP6_FMT " (%u)\n",
	      NIP6(ha->ipv6_addr0), ha->ipv6_addr0_state);
	   copy_info(&info, "\tIPv6 RoutableAddr1 = " NIP6_FMT " (%u)\n",
	      NIP6(ha->ipv6_addr1), ha->ipv6_addr1_state);
	   copy_info(&info, "\tIPv6 DfltRouterAddr= " NIP6_FMT " (%u)\n",
	      NIP6(ha->ipv6_default_router_addr), ha->ipv6_default_router_state);
   }
   copy_info(&info, "\nDriver Info:\n");
   copy_info(&info, "\tIPv4 options=0x%x tcp=0x%x\n",
        ha->ipv4_options, ha->tcp_options);
   if (ha->acb_version == ACB_SUPPORTED)
	   copy_info(&info, "\tIPv6 options=0x%x add'l=0x%x\n",
		ha->ipv6_options, ha->ipv6_addl_options);
   copy_info(&info, "\tNumber of free request entries  = %d of %d\n",
        ha->req_q_count, REQUEST_QUEUE_DEPTH);
   copy_info(&info, "\tNumber of free aen entries    = %d of %d\n",
        ha->aen_q_count, MAX_AEN_ENTRIES);
   copy_info(&info, "\tNumber of Mailbox Timeouts = %d\n",
        ha->mailbox_timeout_count);
   copy_info(&info, "\tTotal H/W int = 0x%lx\n",
        (unsigned long)ha->isr_count);
   copy_info(&info, "\tTotal RISC int = 0x%lx\n",
        (unsigned long)ha->total_io_count);
#if TRACK_SPURIOUS_INTRS
   copy_info(&info, "\tNumber of Spurious interrupts = %d\n",
        ha->spurious_int_count);
#endif
   DEBUG2(copy_info(&info, "\tSeconds since last Interrupt = %d\n",
        ha->seconds_since_last_intr);)
   copy_info(&info, "\tReqQptr=%p, ReqIn=%d, ReqOut=%d\n",
        ha->request_ptr, ha->request_in, ha->request_out);
   copy_info(&info, "\tAdapter flags = 0x%lx, DPC flags = 0x%lx ha=%p\n",
        ha->flags, ha->dpc_flags, ha);
   copy_info(&info, "\tFirmware state = 0x%x (0x%x)\n",
        ha->firmware_state,
        ha->addl_fw_state);
   copy_info(&info, "\tTotal number of IOCBs (used/max) "
        "= (%d/%d)\n", ha->iocb_cnt, REQUEST_QUEUE_DEPTH);
#if MEMORY_MAPPED_IO
   copy_info(&info, "\tMemory I/O = 0x%lx\n", ha->host->base);
#else
   copy_info(&info, "\tI/O Port = 0x%lx\n", ha->host->io_port);
#endif
   copy_info(&info, "\n");

   copy_info(&info, "Runtime Parameters:\n");
   for (i=0; proc_param[i].name !=NULL; i++) {
      copy_info(&info, "\t%s%d\n", proc_param[i].name, *(proc_param[i].value));
   }
   copy_info(&info, "\n");

   qla4xxx_proc_dump_scanned_devices(ha, &info);
   copy_info(&info, "\n\0");

   retval = info.pos > info.offset ? info.pos - info.offset : 0;
done:
   return retval;
}
#endif

static void qla4xxx_recovery_timedout(struct iscsi_cls_session *session)
{
   struct ddb_entry *ddb_entry = session->dd_data;
   struct scsi_qla_host *ha = ddb_entry->ha;

   atomic_set(&ddb_entry->state, DDB_STATE_DEAD);
   dev_info(&ha->pdev->dev,
      "scsi%ld: %s: ddb[%d] os[%d] marked DEAD - retry count of (%d)\n",
      ha->host_no, __func__, ddb_entry->fw_ddb_index,
      ddb_entry->os_target_id, ddb_entry->sess->recovery_tmo);

#ifdef __VMKLNX__
   set_bit(DF_OFFLINE, &ddb_entry->flags);
   set_bit(DPC_OFFLINE_DEVICE, &ha->dpc_flags);
#endif /* __VMKLNX__ */

   DEBUG2(printk("scsi%ld: %s: scheduling dpc routine - dpc flags = "
            "0x%lx\n", ha->host_no, __func__, ha->dpc_flags));
   queue_work(ha->dpc_thread, &ha->dpc_work);
}

int qla4xxx_conn_start(struct iscsi_cls_conn *conn)
{
   struct iscsi_cls_session *session;
   struct ddb_entry *ddb_entry;

#ifndef __VMKLNX__
   session = iscsi_dev_to_session(conn->dev.parent);
#else
   session = conn->session;
#endif
   ddb_entry = session->dd_data;

   DEBUG2(printk("scsi%ld: %s: index [%d] starting conn\n",
            ddb_entry->ha->host_no, __func__,
            ddb_entry->fw_ddb_index));
   iscsi_unblock_session(session);
   return 0;
}

static void qla4xxx_conn_stop(struct iscsi_cls_conn *conn, int flag)
{
   struct iscsi_cls_session *session;
   struct ddb_entry *ddb_entry;

#ifndef __VMKLNX__
   session = iscsi_dev_to_session(conn->dev.parent);
#else
   session = conn->session;
#endif
   ddb_entry = session->dd_data;

   DEBUG2(printk("scsi%ld: %s: index [%d] stopping conn\n",
            ddb_entry->ha->host_no, __func__,
            ddb_entry->fw_ddb_index));
   if (flag == STOP_CONN_RECOVER)
      iscsi_block_session(session);
   else
      printk(KERN_ERR "iscsi: invalid stop flag %d\n", flag);
}

static int qla4xxx_sess_get_param(struct iscsi_cls_session *sess,
              enum iscsi_param param, char *buf)
{
   struct ddb_entry *ddb_entry = sess->dd_data;
   int len;

   switch (param) {
   case ISCSI_PARAM_TARGET_NAME:
      len = snprintf(buf, PAGE_SIZE - 1, "%s",
                ddb_entry->iscsi_name);
      break;
   case ISCSI_PARAM_TPGT:
      len = sprintf(buf, "%u", ddb_entry->tpgt);
      break;

#ifdef __VMKLNX__
    case ISCSI_PARAM_ISID:
      len = sprintf(buf, ISID_FMT, ISID(ddb_entry->isid));
       break;
#endif

   default:
      return -ENOSYS;
   }

   return len;
}

static int qla4xxx_conn_get_param(struct iscsi_cls_conn *conn,
              enum iscsi_param param, char *buf)
{
   struct iscsi_cls_session *session;
   struct ddb_entry *ddb_entry;
   int len;

#ifndef __VMKLNX__
   session = iscsi_dev_to_session(conn->dev.parent);
#else
   session = conn->session;
#endif
   ddb_entry = session->dd_data;

   switch (param) {
   case ISCSI_PARAM_CONN_PORT:
      len = sprintf(buf, "%u", (uint32_t)ddb_entry->port);
      break;
   case ISCSI_PARAM_CONN_ADDRESS:
      if ((ddb_entry->options & DDB_OPT_IPV6_DEVICE) == 0)
         len = sprintf(buf, NIPQUAD_FMT,
               NIPQUAD(ddb_entry->ip_addr));
      else if (ddb_entry->options & DDB_OPT_IPV6_NULL_LINK_LOCAL)
         len = sprintf(buf, NIP6_FMT,
               NIP6(ddb_entry->remote_ipv6_addr));
      else
         len = sprintf(buf, NIP6_FMT,
               NIP6(ddb_entry->link_local_ipv6_addr));
      break;
   default:
      return -ENOSYS;
   }

   return len;
}

#ifndef __VMKLNX__
static int qla4xxx_tgt_dscvr(enum iscsi_tgt_dscvr type, uint32_t host_no,
              uint32_t enable, struct sockaddr *dst_addr)
{
   struct scsi_qla_host *ha;
   struct Scsi_Host *shost;
   struct sockaddr_in *addr;
   struct sockaddr_in6 *addr6;
   int ret = 0;

   shost = scsi_host_lookup(host_no);
   if (IS_ERR(shost)) {
      printk(KERN_ERR "Could not find host no %u\n", host_no);
      return -ENODEV;
   }

   ha = (struct scsi_qla_host *) shost->hostdata;

   switch (type) {
   case ISCSI_TGT_DSCVR_SEND_TARGETS:
      if (dst_addr->sa_family == AF_INET) {
         addr = (struct sockaddr_in *)dst_addr;
         if (qla4xxx_send_tgts(ha, (char *)&addr->sin_addr,
                     addr->sin_port) != QLA_SUCCESS)
            ret = -EIO;
      } else if (dst_addr->sa_family == AF_INET6) {
         /*
          * TODO: fix qla4xxx_send_tgts
          */
         addr6 = (struct sockaddr_in6 *)dst_addr;
         if (qla4xxx_send_tgts(ha, (char *)&addr6->sin6_addr,
                     addr6->sin6_port) != QLA_SUCCESS)
            ret = -EIO;
      } else
         ret = -ENOSYS;
      break;
   default:
      ret = -ENOSYS;
   }

   scsi_host_put(shost);
   return ret;
}
#else
static int qla4xxx_host_get_param(struct Scsi_Host *shost,
         enum iscsi_host_param param, char *buf)
{
   struct scsi_qla_host *ha = to_qla_host(shost);
   int len;

   switch (param) {
   case ISCSI_HOST_PARAM_IPADDRESS:
      if (ha->acb_version == ACB_NOT_SUPPORTED ||
	  (is_ipv4_enabled(ha) && ha->ipv4_addr_state == IP_ADDRSTATE_PREFERRED))
		len = sprintf(buf, NIPQUAD_FMT, NIPQUAD(ha->ip_address));
      else if (ha->ipv6_link_local_state == IP_ADDRSTATE_PREFERRED)
	      len = sprintf(buf, NIP6_FMT, NIP6(ha->ipv6_link_local_addr));
      else if (ha->ipv6_addr0_state == IP_ADDRSTATE_PREFERRED)
	      len = sprintf(buf, NIP6_FMT, NIP6(ha->ipv6_addr0));
      else if (ha->ipv6_addr1_state == IP_ADDRSTATE_PREFERRED)
	      len = sprintf(buf, NIP6_FMT, NIP6(ha->ipv6_addr1));
      break;
   case ISCSI_HOST_PARAM_INITIATOR_NAME:
      len = sprintf(buf, "%s", ha->name_string);
      break;
   default:
      return -ENOSYS;
   }

   return len;
}

/**
 * ql_alloc_osindex -
 * @ha: pointer to adapter structure
 **/
static int ql_alloc_osindex(struct scsi_qla_host *ha)
{
   int idx;

   for (idx = 0; idx < MAX_DDB_ENTRIES; idx++)
      if (test_and_set_bit((idx & 0x1F), &ha->os_map[(idx >> 5)]) == 0)
         return idx;
   return -1;
}

static void free_osindex(struct scsi_qla_host *ha, uint32_t idx)
{
   clear_bit((idx & 0x1F), &ha->os_map[idx >> 5]);
}

#endif

void qla4xxx_destroy_sess(struct ddb_entry *ddb_entry)
{
   if (!ddb_entry->sess)
      return;
#ifdef __VMKLNX__
   free_osindex(ddb_entry->ha, ddb_entry->os_target_id);
#endif

   if (ddb_entry->conn) {
      iscsi_if_destroy_session_done(ddb_entry->conn);
      iscsi_destroy_conn(ddb_entry->conn);
#ifndef __VMKLNX__
      iscsi_remove_session(ddb_entry->sess);
#else
      /* We already removed the session in the dpc. */
      if (atomic_read(&ddb_entry->state) != DDB_STATE_REMOVED) {
         iscsi_remove_session(ddb_entry->sess);
      }
#endif
   }
   iscsi_free_session(ddb_entry->sess);
}

int qla4xxx_add_sess(struct ddb_entry *ddb_entry, int scan)
{
   int err;

#ifndef __VMKLNX__
   err = iscsi_add_session(ddb_entry->sess, ddb_entry->fw_ddb_index);
#else
   err = iscsi_add_session(ddb_entry->sess, ddb_entry->os_target_id, 0);
#endif
   if (err) {
      DEBUG2(printk(KERN_ERR "Could not add session.\n"));
      return err;
   }

   ddb_entry->conn = iscsi_create_conn(ddb_entry->sess, 0);
   if (!ddb_entry->conn) {
      iscsi_remove_session(ddb_entry->sess);
      DEBUG2(printk(KERN_ERR "Could not add connection.\n"));
      return -ENOMEM;
   }

#ifndef __VMKLNX__
   ddb_entry->sess->recovery_tmo = ddb_entry->ha->port_down_retry_count;
   if (scan)
      scsi_scan_target(&ddb_entry->sess->dev, 0,
             ddb_entry->sess->target_id,
             SCAN_WILD_CARD, 0);
#else
   ddb_entry->sess->recovery_tmo = recovery_tmo;
   if (scan)
      scsi_scan_target(ddb_entry->sess->device, 0,
            ddb_entry->sess->targetID,
            SCAN_WILD_CARD, 0);
#endif
   iscsi_if_create_session_done(ddb_entry->conn);
   return 0;
}

struct ddb_entry *qla4xxx_alloc_sess(struct scsi_qla_host *ha)
{
   struct ddb_entry *ddb_entry;
   struct iscsi_cls_session *sess;

#ifdef __VMKLNX__
   int os_idx;

   if ((os_idx = ql_alloc_osindex(ha)) == -1)
      return NULL;
#endif

   sess = iscsi_alloc_session(ha->host, &qla4xxx_iscsi_transport);
   if (!sess) {
#ifdef __VMKLNX__
      free_osindex(ha, os_idx);
#endif
      return NULL;
   }

   ddb_entry = sess->dd_data;
   memset(ddb_entry, 0, sizeof(*ddb_entry));
#ifdef __VMKLNX__
   ddb_entry->os_target_id = os_idx;
#endif
   ddb_entry->ha = ha;
   ddb_entry->sess = sess;
   return ddb_entry;
}

/*
 * Timer routines
 */

static void qla4xxx_start_timer(struct scsi_qla_host *ha, void *func,
            unsigned long interval)
{
   DEBUG(printk("scsi: %s: Starting timer thread for adapter %d\n",
           __func__, ha->host->host_no));
   init_timer(&ha->timer);
   ha->timer.expires = jiffies + interval * HZ;
   ha->timer.data = (unsigned long)ha;
   ha->timer.function = (void (*)(unsigned long))func;
   add_timer(&ha->timer);
   ha->timer_active = 1;
}

static void qla4xxx_stop_timer(struct scsi_qla_host *ha)
{
   del_timer_sync(&ha->timer);
   ha->timer_active = 0;
}

/***
 * qla4xxx_mark_device_missing - mark a device as missing.
 * @ha: Pointer to host adapter structure.
 * @ddb_entry: Pointer to device database entry
 *
 * This routine marks a device missing and resets the relogin retry count.
 **/
void qla4xxx_mark_device_missing(struct scsi_qla_host *ha,
             struct ddb_entry *ddb_entry)
{
   atomic_set(&ddb_entry->state, DDB_STATE_MISSING);
   dev_info(&ha->pdev->dev,
      "scsi%ld: %s: ddb[%d] os[%d] marked MISSING\n",
       ha->host_no, __func__, ddb_entry->fw_ddb_index, ddb_entry->os_target_id);

   qla4xxx_conn_stop(ddb_entry->conn, STOP_CONN_RECOVER);
}


#ifdef __VMKLNX__
/***
 * qla4xxx_mark_device_lost - inform iSCSI transport a session has been lost
 * @ddb_entry: Pointer to device database entry
 *
 * This routine reports the device is lost to the iscsi transport.
 **/
void qla4xxx_mark_device_lost(struct ddb_entry *ddb_entry)
{
   struct iscsi_cls_conn *conn = ddb_entry->conn;

   dev_info(&ddb_entry->ha->pdev->dev,
      "scsi%ld: %s: index [%d] os [%d] LOST session\n",
      ddb_entry->ha->host_no, __func__, ddb_entry->fw_ddb_index,
      ddb_entry->os_target_id);

   iscsi_lost_session(conn->session);
}
#endif /* __VMKLNX__ */

/***
 * qla4xxx_get_new_srb - Allocate memory for a local srb.
 * @ha: Pointer to host adapter structure.
 * @ddb_entry: Pointer to device database entry
 * @cmd: Pointer to Linux's SCSI command structure
 * @done: Pointer to Linux's SCSI mid-layer done function
 *
 * NOTE: Sets te ref_count for non-NULL srb to one,
 *       and initializes some fields.
 **/
static struct srb* qla4xxx_get_new_srb(struct scsi_qla_host *ha,
                   struct ddb_entry *ddb_entry,
                   struct scsi_cmnd *cmd,
                   void (*done)(struct scsi_cmnd *))
{
   struct srb *srb;

   srb = mempool_alloc(ha->srb_mempool, GFP_ATOMIC);
   if (!srb)
      return srb;

   atomic_set(&srb->ref_count, 1);
   srb->ha = ha;
   srb->ddb = ddb_entry;
   srb->cmd = cmd;
   srb->flags = 0;
   cmd->SCp.ptr = (void *)srb;
   cmd->scsi_done = done;

#if defined(__VMKLNX__)
   srb->scsi_sec_lun_id = vmklnx_scsi_cmd_get_secondlevel_lun_id(cmd);
#endif

   return srb;
}

static void qla4xxx_srb_free_dma(struct scsi_qla_host *ha, struct srb *srb)
{
   struct scsi_cmnd *cmd = srb->cmd;

   if (srb->flags & SRB_DMA_VALID) {
      if (cmd->use_sg) {
         pci_unmap_sg(ha->pdev, cmd->request_buffer,
                 cmd->use_sg, cmd->sc_data_direction);
      } else if (cmd->request_bufflen) {
         pci_unmap_single(ha->pdev, srb->dma_handle,
                cmd->request_bufflen,
                cmd->sc_data_direction);
      }
      srb->flags &= ~SRB_DMA_VALID;
   }
   cmd->SCp.ptr = NULL;
}

void qla4xxx_srb_compl(struct scsi_qla_host *ha, struct srb *srb)
{
   struct scsi_cmnd *cmd = srb->cmd;

   if (!(srb->flags & SRB_SCSI_PASSTHRU)) {
      qla4xxx_srb_free_dma(ha, srb);
      mempool_free(srb, ha->srb_mempool);
   }
   cmd->scsi_done(cmd);
}

/**
 * sp_put - Decrement reference count and call callback.
 * @ha: Pointer to host adapter structure.
 * @sp: Pointer to srb structure
 **/
void sp_put(struct scsi_qla_host *ha, struct srb *sp)
{
	if (atomic_read(&sp->ref_count) == 0) {
		DEBUG2(printk("%s: SP->ref_count ZERO\n", __func__));
		DEBUG2(BUG());
		return;
	}
	if (!atomic_dec_and_test(&sp->ref_count)) {
		return;
	}
	qla4xxx_srb_compl(ha, sp);
}

/**
 * sp_get - Increment reference count of the specified sp.
 * @sp: Pointer to srb structure
 **/
void sp_get(struct srb *sp)
{
	atomic_inc(&sp->ref_count);
}

/**
 * qla4xxx_queuecommand - scsi layer issues scsi command to driver.
 * @cmd: Pointer to Linux's SCSI command structure
 * @done_fn: Function that the driver calls to notify the SCSI mid-layer
 * that the command has been processed.
 *
 * Remarks:
 * This routine is invoked by Linux to send a SCSI command to the driver.
 * The mid-level driver tries to ensure that queuecommand never gets
 * invoked concurrently with itself or the interrupt handler (although
 * the interrupt handler may call this routine as part of request-
 * completion handling).   Unfortunely, it sometimes calls the scheduler
 * in interrupt context which is a big NO! NO!.
 **/
static int qla4xxx_queuecommand(struct scsi_cmnd *cmd,
            void (*done)(struct scsi_cmnd *))
{
   struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
   struct ddb_entry *ddb_entry = cmd->device->hostdata;
   struct srb *srb;
   int rval;

   if (atomic_read(&ddb_entry->state) != DDB_STATE_ONLINE) {
#ifndef __VMKLNX__
      if (atomic_read(&ddb_entry->state) == DDB_STATE_DEAD) {
#else
      if ((atomic_read(&ddb_entry->state) == DDB_STATE_DEAD) ||
         (atomic_read(&ddb_entry->state) == DDB_STATE_REMOVED)) {
#endif
         cmd->result = DID_NO_CONNECT << 16;
         goto qc_fail_command;
      }
      goto qc_host_busy;
   }

   if (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
      test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
      test_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags))
      goto qc_host_busy;

   spin_unlock_irq(ha->host->host_lock);

   srb = qla4xxx_get_new_srb(ha, ddb_entry, cmd, done);
   if (!srb)
      goto qc_host_busy_lock;

   rval = qla4xxx_send_command_to_isp(ha, srb);
   if (rval != QLA_SUCCESS)
      goto qc_host_busy_free_sp;

   spin_lock_irq(ha->host->host_lock);
   return 0;

qc_host_busy_free_sp:
   qla4xxx_srb_free_dma(ha, srb);
   mempool_free(srb, ha->srb_mempool);

qc_host_busy_lock:
   spin_lock_irq(ha->host->host_lock);

qc_host_busy:
   return SCSI_MLQUEUE_HOST_BUSY;

qc_fail_command:
   done(cmd);

   return 0;
}

/**
 * qla4xxx_mem_free - frees memory allocated to adapter
 * @ha: Pointer to host adapter structure.
 *
 * Frees memory previously allocated by qla4xxx_mem_alloc
 **/
static void qla4xxx_mem_free(struct scsi_qla_host *ha)
{
#ifndef __VMKLNX__
	struct list_head *ptr;
	struct async_msg_pdu_iocb *apdu_iocb;
#endif /* __VMKLNX__ */
	
   if (ha->queues)
      dma_free_coherent(&ha->pdev->dev, ha->queues_len, ha->queues,
              ha->queues_dma);

#ifndef __VMKLNX__
	if (ha->gen_req_rsp_iocb)
		dma_free_coherent(&ha->pdev->dev, PAGE_SIZE,
			ha->gen_req_rsp_iocb, ha->gen_req_rsp_iocb_dma);

	while (!list_empty(&ha->async_iocb_list)) {
		ptr = ha->async_iocb_list.next;
		apdu_iocb = list_entry(ptr, struct async_msg_pdu_iocb, list);
		list_del_init(&apdu_iocb->list);
		kfree(apdu_iocb);
	}
#endif /* __VMKLNX__ */

   ha->queues_len = 0;
   ha->queues = NULL;
   ha->queues_dma = 0;
   ha->request_ring = NULL;
   ha->request_dma = 0;
   ha->response_ring = NULL;
   ha->response_dma = 0;
   ha->shadow_regs = NULL;
   ha->shadow_regs_dma = 0;

   /* Free srb pool. */
   if (ha->srb_mempool)
      mempool_destroy(ha->srb_mempool);

   ha->srb_mempool = NULL;

   /* release io space registers  */
   if (ha->reg)
      iounmap(ha->reg);
   pci_release_regions(ha->pdev);
}

/**
 * qla4xxx_mem_alloc - allocates memory for use by adapter.
 * @ha: Pointer to host adapter structure
 *
 * Allocates DMA memory for request and response queues. Also allocates memory
 * for srbs.
 **/
static int qla4xxx_mem_alloc(struct scsi_qla_host *ha)
{
   unsigned long align;

   /* Allocate contiguous block of DMA memory for queues. */
   ha->queues_len = ((REQUEST_QUEUE_DEPTH * QUEUE_SIZE) +
           (RESPONSE_QUEUE_DEPTH * QUEUE_SIZE) +
           sizeof(struct shadow_regs) +
           MEM_ALIGN_VALUE +
           (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
   ha->queues = dma_alloc_coherent(&ha->pdev->dev, ha->queues_len,
               &ha->queues_dma, GFP_KERNEL);
   if (ha->queues == NULL) {
      dev_warn(&ha->pdev->dev,
         "Memory Allocation failed - queues.\n");

      goto mem_alloc_error_exit;
   }
   memset(ha->queues, 0, ha->queues_len);

   /*
    * As per RISC alignment requirements -- the bus-address must be a
    * multiple of the request-ring size (in bytes).
    */
   align = 0;
   if ((unsigned long)ha->queues_dma & (MEM_ALIGN_VALUE - 1))
      align = MEM_ALIGN_VALUE - ((unsigned long)ha->queues_dma &
                  (MEM_ALIGN_VALUE - 1));

   /* Update request and response queue pointers. */
   ha->request_dma = ha->queues_dma + align;
   ha->request_ring = (struct queue_entry *) (ha->queues + align);
   ha->response_dma = ha->queues_dma + align +
      (REQUEST_QUEUE_DEPTH * QUEUE_SIZE);
   ha->response_ring = (struct queue_entry *) (ha->queues + align +
                      (REQUEST_QUEUE_DEPTH *
                       QUEUE_SIZE));
   ha->shadow_regs_dma = ha->queues_dma + align +
      (REQUEST_QUEUE_DEPTH * QUEUE_SIZE) +
      (RESPONSE_QUEUE_DEPTH * QUEUE_SIZE);
   ha->shadow_regs = (struct shadow_regs *) (ha->queues + align +
                    (REQUEST_QUEUE_DEPTH *
                     QUEUE_SIZE) +
                    (RESPONSE_QUEUE_DEPTH *
                     QUEUE_SIZE));

   /* Allocate memory for srb pool. */
   ha->srb_mempool = mempool_create(SRB_MIN_REQ, mempool_alloc_slab,
                mempool_free_slab, srb_cachep);
   if (ha->srb_mempool == NULL) {
      dev_warn(&ha->pdev->dev,
         "Memory Allocation failed - SRB Pool.\n");

      goto mem_alloc_error_exit;
   }

#ifndef __VMKLNX__
	/* Allocate memory for async pdus. */
	ha->gen_req_rsp_iocb = dma_alloc_coherent(&ha->pdev->dev, PAGE_SIZE,
						  &ha->gen_req_rsp_iocb_dma, GFP_KERNEL);
	if (ha->gen_req_rsp_iocb == NULL) {
		dev_warn(&ha->pdev->dev,
			 "Memory Allocation failed - gen_req_rsp_iocb.\n");

		goto mem_alloc_error_exit;
	}
#endif /* __VMKLNX__ */

   return QLA_SUCCESS;

mem_alloc_error_exit:
   qla4xxx_mem_free(ha);
   return QLA_ERROR;
}

/**
 * qla4xxx_timer - checks every second for work to do.
 * @ha: Pointer to host adapter structure.
 **/
static void qla4xxx_timer(struct scsi_qla_host *ha)
{
   struct ddb_entry *ddb_entry, *dtemp;
   int start_dpc = 0;

   /* Search for relogin's to time-out and port down retry. */
   list_for_each_entry_safe(ddb_entry, dtemp, &ha->ddb_list, list) {
      /* Count down time between sending relogins */
      if (adapter_up(ha) &&
          !test_bit(DF_RELOGIN, &ddb_entry->flags) &&
#ifdef __VMKLNX__
          !test_bit(DF_STOP_RELOGIN, &ddb_entry->flags) &&
          atomic_read(&ddb_entry->state) != DDB_STATE_REMOVED &&
#endif
          atomic_read(&ddb_entry->state) != DDB_STATE_ONLINE) {
         if (atomic_read(&ddb_entry->retry_relogin_timer) !=
             INVALID_ENTRY) {
            if (atomic_read(&ddb_entry->retry_relogin_timer)
                     == 0) {
               atomic_set(&ddb_entry->
                  retry_relogin_timer,
                  INVALID_ENTRY);
               set_bit(DPC_RELOGIN_DEVICE,
                  &ha->dpc_flags);
               set_bit(DF_RELOGIN, &ddb_entry->flags);
               DEBUG2(printk("scsi%ld: %s: index [%d]"
                        " relogin device\n",
                        ha->host_no, __func__,
                        ddb_entry->fw_ddb_index));
            } else
               atomic_dec(&ddb_entry->
                     retry_relogin_timer);
         }
      }

      /* Wait for relogin to timeout */
      if (atomic_read(&ddb_entry->relogin_timer) &&
          (atomic_dec_and_test(&ddb_entry->relogin_timer) != 0)) {
         /*
          * If the relogin times out and the device is
          * still NOT ONLINE then try and relogin again.
          */
         if (atomic_read(&ddb_entry->state) !=
             DDB_STATE_ONLINE &&
             ddb_entry->fw_ddb_device_state ==
             DDB_DS_SESSION_FAILED) {
            /* Reset retry relogin timer */
            atomic_inc(&ddb_entry->relogin_retry_count);
            DEBUG2(printk("scsi%ld: index[%d] relogin"
                     " timed out-retrying"
                     " relogin (%d)\n",
                     ha->host_no,
                     ddb_entry->fw_ddb_index,
                     atomic_read(&ddb_entry->
                       relogin_retry_count))
               );
            start_dpc++;
            DEBUG(printk("scsi%ld:%d:%d: index [%d] "
                    "initate relogin after"
                    " %d seconds\n",
                    ha->host_no, 0,
                    ddb_entry->os_target_id,
                    ddb_entry->fw_ddb_index,
                    ddb_entry->default_time2wait + 4)
               );

            atomic_set(&ddb_entry->retry_relogin_timer,
                  ddb_entry->default_time2wait + 4);
         }
      }
   }

   /* Check for heartbeat interval. */
   if (ha->firmware_options & FWOPT_HEARTBEAT_ENABLE &&
       ha->heartbeat_interval != 0) {
      ha->seconds_since_last_heartbeat++;
      if (ha->seconds_since_last_heartbeat >
          ha->heartbeat_interval + 2)
         set_bit(DPC_RESET_HA, &ha->dpc_flags);
   }

#ifdef __VMKLNX__
   /*
    * If we have have been !AF_ONLINE for an extended
    * period of time and a reset isn't scheduled.  Then
    * schedule a new reset to try and get the adapter
    * back online.
    */
   if ((!test_bit(AF_ONLINE, &ha->flags)) &&
       (!test_bit(DPC_RESET_HA, &ha->dpc_flags))) {
      if (ha->af_offline++ > AF_OFFLINE_RESET) {
         dev_info(&ha->pdev->dev,
            "scsi%ld: %s: detected adapter offline, attempt restart\n",
            ha->host_no, __func__);
         ha->af_offline = 0;
         set_bit(DPC_RESET_HA, &ha->dpc_flags);
      }
   }
#endif

   /* Wakeup the dpc routine for this adapter, if needed. */
   if ((start_dpc ||
        test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
        test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags) ||
        test_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags) ||
#ifdef __VMKLNX__
        test_bit(DPC_OFFLINE_DEVICE, &ha->dpc_flags) ||
        test_bit(DPC_REMOVE_DEVICE, &ha->dpc_flags) ||
        test_bit(DPC_FREE_DEVICE, &ha->dpc_flags) ||
        test_bit(DPC_LINK_CHANGED, &ha->dpc_flags) ||
#else /* ! __VMKLNX__ */
	test_bit(DPC_ASYNC_PDU, &ha->dpc_flags) ||
#endif /* __VMKLNX__ */
        test_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags) ||
        test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	test_bit(DPC_DYNAMIC_LUN_SCAN, &ha->dpc_flags) ||
        test_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags) ||
        test_bit(DPC_AEN, &ha->dpc_flags)) &&
        ha->dpc_thread) {
      DEBUG2(printk("scsi%ld: %s: scheduling dpc routine"
               " - dpc flags = 0x%lx\n",
               ha->host_no, __func__, ha->dpc_flags));
      queue_work(ha->dpc_thread, &ha->dpc_work);
   }

   /* Reschedule timer thread to call us back in one second */
   mod_timer(&ha->timer, jiffies + HZ);

   DEBUG2(ha->seconds_since_last_intr++);
}

/**
 * qla4xxx_cmd_wait - waits for all outstanding commands to complete
 * @ha: Pointer to host adapter structure.
 *
 * This routine stalls the driver until all outstanding commands are returned.
 * Caller must release the Hardware Lock prior to calling this routine.
 **/
static int qla4xxx_cmd_wait(struct scsi_qla_host *ha)
{
   uint32_t index = 0;
   int stat = QLA_SUCCESS;
   unsigned long flags;
   int wait_cnt = WAIT_CMD_TOV;   /*
                * Initialized for 30 seconds as we
                * expect all commands to retuned
                * ASAP.
                */

   while (wait_cnt) {
      spin_lock_irqsave(&ha->hardware_lock, flags);
      /* Find a command that hasn't completed. */
      for (index = 1; index < MAX_SRBS; index++) {
         if (ha->active_srb_array[index] != NULL)
            break;
      }
      spin_unlock_irqrestore(&ha->hardware_lock, flags);

      /* If No Commands are pending, wait is complete */
      if (index == MAX_SRBS) {
         break;
      }

      /* If we timed out on waiting for commands to come back
       * return ERROR.
       */
      wait_cnt--;
      if (wait_cnt == 0)
         stat = QLA_ERROR;
      else {
         msleep(1000);
      }
   }         /* End of While (wait_cnt) */

   return stat;
}

void qla4xxx_hw_reset(struct scsi_qla_host *ha)
{
   uint32_t ctrl_status;
   unsigned long flags = 0;

   DEBUG2(printk(KERN_ERR "scsi%ld: %s\n", ha->host_no, __func__));

   spin_lock_irqsave(&ha->hardware_lock, flags);
   /*
    * If the SCSI Reset Interrupt bit is set, clear it.
    * Otherwise, the Soft Reset won't work.
    */
   ctrl_status = readw(&ha->reg->ctrl_status);
   if ((ctrl_status & CSR_SCSI_RESET_INTR) != 0)
      writel(set_rmask(CSR_SCSI_RESET_INTR), &ha->reg->ctrl_status);

   /* Issue Soft Reset */
   writel(set_rmask(CSR_SOFT_RESET), &ha->reg->ctrl_status);
   readl(&ha->reg->ctrl_status);

   spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

/**
 * qla4xxx_soft_reset - performs soft reset.
 * @ha: Pointer to host adapter structure.
 **/
int qla4xxx_soft_reset(struct scsi_qla_host *ha)
{
   uint32_t max_wait_time;
   unsigned long flags = 0;
   int status = QLA_ERROR;
   uint32_t ctrl_status;

   qla4xxx_hw_reset(ha);

   /* Wait until the Network Reset Intr bit is cleared */
   max_wait_time = RESET_INTR_TOV;
   do {
      spin_lock_irqsave(&ha->hardware_lock, flags);
      ctrl_status = readw(&ha->reg->ctrl_status);
      spin_unlock_irqrestore(&ha->hardware_lock, flags);

      if ((ctrl_status & CSR_NET_RESET_INTR) == 0)
         break;

      msleep(1000);
   } while ((--max_wait_time));

   if ((ctrl_status & CSR_NET_RESET_INTR) != 0) {
      DEBUG2(printk(KERN_WARNING
               "scsi%ld: Network Reset Intr not cleared by "
               "Network function, clearing it now!\n",
               ha->host_no));
      spin_lock_irqsave(&ha->hardware_lock, flags);
      writel(set_rmask(CSR_NET_RESET_INTR), &ha->reg->ctrl_status);
      readl(&ha->reg->ctrl_status);
      spin_unlock_irqrestore(&ha->hardware_lock, flags);
   }

   /* Wait until the firmware tells us the Soft Reset is done */
   max_wait_time = SOFT_RESET_TOV;
   do {
      spin_lock_irqsave(&ha->hardware_lock, flags);
      ctrl_status = readw(&ha->reg->ctrl_status);
      spin_unlock_irqrestore(&ha->hardware_lock, flags);

      if ((ctrl_status & CSR_SOFT_RESET) == 0) {
         status = QLA_SUCCESS;
         break;
      }

      msleep(1000);
   } while ((--max_wait_time));

   /*
    * Also, make sure that the SCSI Reset Interrupt bit has been cleared
    * after the soft reset has taken place.
    */
   spin_lock_irqsave(&ha->hardware_lock, flags);
   ctrl_status = readw(&ha->reg->ctrl_status);
   if ((ctrl_status & CSR_SCSI_RESET_INTR) != 0) {
      writel(set_rmask(CSR_SCSI_RESET_INTR), &ha->reg->ctrl_status);
      readl(&ha->reg->ctrl_status);
   }
   spin_unlock_irqrestore(&ha->hardware_lock, flags);

   /* If soft reset fails then most probably the bios on other
    * function is also enabled.
    * Since the initialization is sequential the other fn
    * wont be able to acknowledge the soft reset.
    * Issue a force soft reset to workaround this scenario.
    */
   if (max_wait_time == 0) {
      /* Issue Force Soft Reset */
      spin_lock_irqsave(&ha->hardware_lock, flags);
      writel(set_rmask(CSR_FORCE_SOFT_RESET), &ha->reg->ctrl_status);
      readl(&ha->reg->ctrl_status);
      spin_unlock_irqrestore(&ha->hardware_lock, flags);
      /* Wait until the firmware tells us the Soft Reset is done */
      max_wait_time = SOFT_RESET_TOV;
      do {
         spin_lock_irqsave(&ha->hardware_lock, flags);
         ctrl_status = readw(&ha->reg->ctrl_status);
         spin_unlock_irqrestore(&ha->hardware_lock, flags);

         if ((ctrl_status & CSR_FORCE_SOFT_RESET) == 0) {
            status = QLA_SUCCESS;
            break;
         }

         msleep(1000);
      } while ((--max_wait_time));
   }

   return status;
}

/**
 * qla4xxx_flush_active_srbs - returns all outstanding i/o requests to O.S.
 * @ha: Pointer to host adapter structure.
 *
 * This routine is called just prior to a HARD RESET to return all
 * outstanding commands back to the Operating System.
 * Caller should make sure that the following locks are released
 * before this calling routine: Hardware lock, and io_request_lock.
 **/
static void qla4xxx_flush_active_srbs(struct scsi_qla_host *ha)
{
   struct srb *srb;
   int i;
   unsigned long flags;

   spin_lock_irqsave(&ha->hardware_lock, flags);
   for (i = 1; i < MAX_SRBS; i++) {
      if ((srb = ha->active_srb_array[i]) != NULL) {
         qla4xxx_del_from_active_array(ha, i);
         srb->cmd->result = DID_RESET << 16;
         sp_put(ha, srb);
      }
   }
   spin_unlock_irqrestore(&ha->hardware_lock, flags);

}

/**
 * qla4xxx_recover_adapter - recovers adapter after a fatal error
 * @ha: Pointer to host adapter structure.
 **/
static int qla4xxx_recover_adapter(struct scsi_qla_host *ha)
{
   int status;

   /* Stall incoming I/O until we are done */
   clear_bit(AF_ONLINE, &ha->flags);
#ifdef __VMKLNX__
   dev_info(&ha->pdev->dev,
      "scsi%ld: %s: adapter OFFLINE\n",
      ha->host_no, __func__);
#endif

   /* Wait for outstanding commands to complete.
    * Stalls the driver for max 30 secs
    */
   status = qla4xxx_cmd_wait(ha);

   qla4xxx_disable_intrs(ha);

   /* Flush any pending ddb changed AENs */
   qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);

   /* Reset the firmware.   If successful, function
    * returns with ISP interrupts enabled.
    */
   qla4xxx_flush_active_srbs(ha);

   DEBUG2(printk(KERN_ERR "scsi%ld: %s - Performing soft reset..\n",
            ha->host_no, __func__));
   if (ql4xxx_lock_drvr_wait(ha) == QLA_SUCCESS)
      status = qla4xxx_soft_reset(ha);
   else
      status = QLA_ERROR;

   /* Flush any pending ddb changed AENs */
   qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);

   /* Re-initialize firmware. If successful, function returns
    * with ISP interrupts enabled */
   if (status == QLA_SUCCESS) {
      DEBUG2(printk("scsi%ld: %s - Initializing adapter..\n",
               ha->host_no, __func__));

      /* If successful, AF_ONLINE flag set in
       * qla4xxx_initialize_adapter */
      status = qla4xxx_initialize_adapter(ha, PRESERVE_DDB_LIST);
   }

   /* Failed adapter initialization?
    * Retry reset_ha only if invoked via DPC (DPC_RESET_HA) */
   if ((test_bit(AF_ONLINE, &ha->flags) == 0) &&
       (test_bit(DPC_RESET_HA, &ha->dpc_flags))) {
      /* Adapter initialization failed, see if we can retry
       * resetting the ha */
      if (!test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags)) {
         ha->retry_reset_ha_cnt = MAX_RESET_HA_RETRIES;
         DEBUG2(printk("scsi%ld: recover adapter - retrying "
                  "(%d) more times\n", ha->host_no,
                  ha->retry_reset_ha_cnt));
         set_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
         status = QLA_ERROR;
      } else {
         if (ha->retry_reset_ha_cnt > 0) {
            /* Schedule another Reset HA--DPC will retry */
            ha->retry_reset_ha_cnt--;
            DEBUG2(printk("scsi%ld: recover adapter - "
                     "retry remaining %d\n",
                     ha->host_no,
                     ha->retry_reset_ha_cnt));
            status = QLA_ERROR;
         }

         if (ha->retry_reset_ha_cnt == 0) {
            /* Recover adapter retries have been exhausted.
             * Adapter DEAD */
            DEBUG2(printk("scsi%ld: recover adapter "
                     "failed - board disabled\n",
                     ha->host_no));
            qla4xxx_flush_active_srbs(ha);
            clear_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
            clear_bit(DPC_RESET_HA, &ha->dpc_flags);
            clear_bit(DPC_RESET_HA_DESTROY_DDB_LIST,
                 &ha->dpc_flags);
            status = QLA_ERROR;
         }
      }
   } else {
      clear_bit(DPC_RESET_HA, &ha->dpc_flags);
      clear_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags);
      clear_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
   }

   ha->adapter_error_count++;

   if (status == QLA_SUCCESS)
      qla4xxx_enable_intrs(ha);

   DEBUG2(printk("scsi%ld: recover adapter .. DONE\n", ha->host_no));
   return status;
}

#ifndef __VMKLNX__
/*
 * qla4xxx_process_async_pdu_iocb - processes ASYNC PDU IOCBS, if they are greater in
 * length than 48 bytes (i.e., more than just the iscsi header). Used for
 * unsolicited pdus received from target.
 */
static void qla4xxx_process_async_pdu_iocb(struct scsi_qla_host *ha,
			struct async_msg_pdu_iocb *amsg_pdu_iocb)
{
	struct iscsi_hdr *hdr;
	struct async_pdu_iocb *apdu;
	uint32_t len;
	void *buf_addr;
	dma_addr_t buf_addr_dma;
	uint32_t offset;
	struct passthru0 *pthru0_iocb;
	struct ddb_entry *ddb_entry = NULL;
	struct async_pdu_sense *pdu_sense;

	uint8_t using_prealloc = 1;
	uint8_t async_event_type;

	apdu = (struct async_pdu_iocb *)amsg_pdu_iocb->iocb;
	hdr = (struct iscsi_hdr *)apdu->iscsi_pdu_hdr;
	len = hdr->hlength + hdr->dlength[2] +
		(hdr->dlength[1]<<8) + (hdr->dlength[0]<<16);

	offset = sizeof(struct passthru0) + sizeof(struct passthru_status);
	if (len <= (PAGE_SIZE - offset)) {
		buf_addr_dma = ha->gen_req_rsp_iocb_dma + offset;
		buf_addr = (uint8_t *)ha->gen_req_rsp_iocb + offset;
	} else {
		using_prealloc = 0;
		buf_addr = dma_alloc_coherent(&ha->pdev->dev, len,
					&buf_addr_dma, GFP_KERNEL);
		if (!buf_addr) {
			dev_info(&ha->pdev->dev,
				"%s: dma_alloc_coherent failed\n", __func__);
			return;
		}
	}
	/* Create the pass-thru0 iocb */
	pthru0_iocb = ha->gen_req_rsp_iocb;
	memset(pthru0_iocb, 0, offset);

	pthru0_iocb->hdr.entryType = ET_PASSTHRU0;
	pthru0_iocb->hdr.entryCount = 1;
	pthru0_iocb->target = cpu_to_le16(apdu->target_id);
	pthru0_iocb->controlFlags =
		cpu_to_le16(PT_FLAG_ISCSI_PDU | PT_FLAG_WAIT_4_RESPONSE);
	pthru0_iocb->timeout = cpu_to_le16(PT_DEFAULT_TIMEOUT);
	pthru0_iocb->inDataSeg64.base.addrHigh =
		cpu_to_le32(MSDW(buf_addr_dma));
	pthru0_iocb->inDataSeg64.base.addrLow =
		cpu_to_le32(LSDW(buf_addr_dma));
	pthru0_iocb->inDataSeg64.count = cpu_to_le32(len);
	pthru0_iocb->async_pdu_handle = cpu_to_le32(apdu->async_pdu_handle);

	dev_info(&ha->pdev->dev,
			"%s: qla4xxx_issue_iocb\n", __func__);

	if (qla4xxx_issue_iocb(ha, sizeof(struct passthru0),
		ha->gen_req_rsp_iocb_dma) != QLA_SUCCESS) {
		dev_info(&ha->pdev->dev,
			"%s: qla4xxx_issue_iocb failed\n", __func__);
		goto exit_async_pdu_iocb;
	}

	async_event_type = ((struct iscsi_async *)hdr)->async_event;
	pdu_sense = (struct async_pdu_sense *)buf_addr;

	switch (async_event_type) {
	case ISCSI_ASYNC_MSG_SCSI_EVENT:
		dev_info(&ha->pdev->dev,
				"%s: async msg event 0x%x processed\n"
				, __func__, async_event_type);

		//qla4xxx_dump_buffer(buf_addr, len);

		if (pdu_sense->sense_data[12] == 0x3F) {
			if (pdu_sense->sense_data[13] == 0x0E) {
				/* reported luns data has changed */
				uint16_t fw_index = apdu->target_id;

				ddb_entry = qla4xxx_lookup_ddb_by_fw_index(ha, fw_index);
				if (ddb_entry == NULL) {
					dev_info(&ha->pdev->dev,
						 "%s: No DDB entry for index [%d]\n"
						 , __func__, fw_index);
					goto exit_async_pdu_iocb;
				}
				if (ddb_entry->fw_ddb_device_state != DDB_DS_SESSION_ACTIVE) {
					dev_info(&ha->pdev->dev,
						 "scsi%ld: %s: No Active Session for index [%d]\n",
						 ha->host_no, __func__, fw_index);
					goto exit_async_pdu_iocb;
				}

#ifndef __VMKLNX__
				/* report new lun to kernel */
				scsi_scan_target(&ddb_entry->sess->dev, 0,
				    ddb_entry->sess->target_id,
				    SCAN_WILD_CARD, 0);
#else
				scsi_scan_target(ddb_entry->sess->device, 0,
            	    ddb_entry->sess->targetID,
				    SCAN_WILD_CARD, 0);
#endif
			}
		}

		break;
	case ISCSI_ASYNC_MSG_REQUEST_LOGOUT:
	case ISCSI_ASYNC_MSG_DROPPING_CONNECTION:
	case ISCSI_ASYNC_MSG_DROPPING_ALL_CONNECTIONS:
	case ISCSI_ASYNC_MSG_PARAM_NEGOTIATION:
		dev_info(&ha->pdev->dev,
				"%s: async msg event 0x%x processed\n"
				, __func__, async_event_type);
		qla4xxx_conn_close_sess_logout(ha, apdu->target_id, 0, 0);
		break;
	default:
		dev_info(&ha->pdev->dev,
			"%s: async msg event 0x%x not processed\n",
			__func__, async_event_type);
		break;
	};

exit_async_pdu_iocb:
	if (!using_prealloc)
		dma_free_coherent(&ha->pdev->dev, len,
			buf_addr, buf_addr_dma);

	return;
}
#endif


/**
 * qla4xxx_do_dpc - dpc routine
 * @data: in our case pointer to adapter structure
 *
 * This routine is a task that is schedule by the interrupt handler
 * to perform the background processing for interrupts.   We put it
 * on a task queue that is consumed whenever the scheduler runs; that's
 * so you can do anything (i.e. put the process to sleep etc).  In fact,
 * the mid-level tries to sleep when it reaches the driver threshold
 * "host->can_queue". This can cause a panic if we were in our interrupt code.
 **/
#if defined(__VMKLNX__)
static void qla4xxx_do_dpc(struct work_struct *work)
{
   struct scsi_qla_host *ha = 
	container_of(work, struct scsi_qla_host, dpc_work);
#else /* !defined(__VMKLNX__) */
static void qla4xxx_do_dpc(void *data)
{
	struct scsi_qla_host *ha = (struct scsi_qla_host *) data;
#endif /* defined(__VMKLNX__) */
	struct ddb_entry *ddb_entry, *dtemp;
	struct async_msg_pdu_iocb *apdu_iocb, *apdu_iocb_tmp;
	int status = QLA_ERROR;

	DEBUG2(printk("scsi%ld: %s: DPC handler waking up."
	    "ha->flags=0x%08lx ha->dpc_flags=0x%08lx"
	    " ctrl_status=0x%08x\n",
	    ha->host_no, __func__, ha->flags, ha->dpc_flags,
	    readw(&ha->reg->ctrl_status)));

	/* Initialization not yet finished. Don't do anything yet. */
	if (!test_bit(AF_INIT_DONE, &ha->flags))
		return;

	if (adapter_up(ha) ||
	    test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags)) {
		if (test_bit(DPC_RESET_HA_DESTROY_DDB_LIST, &ha->dpc_flags) ||
		    test_bit(DPC_RESET_HA, &ha->dpc_flags))
			qla4xxx_recover_adapter(ha);

		if (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags)) {
			uint8_t wait_time = RESET_INTR_TOV;

			while ((readw(&ha->reg->ctrl_status) &
				(CSR_SOFT_RESET | CSR_FORCE_SOFT_RESET)) != 0) {
				if (--wait_time == 0)
					break;
				msleep(1000);
			}

			if (wait_time == 0)
				DEBUG2(printk("scsi%ld: %s: SR|FSR "
					"bit not cleared-- resetting\n",
					ha->host_no, __func__));
			qla4xxx_flush_active_srbs(ha);

			if (ql4xxx_lock_drvr_wait(ha) == QLA_SUCCESS) {
				qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);
				status = qla4xxx_initialize_adapter(ha, PRESERVE_DDB_LIST);
			}	

			clear_bit(DPC_RESET_HA_INTR, &ha->dpc_flags);
			if (status == QLA_SUCCESS)
				qla4xxx_enable_intrs(ha);
		}
	}

	/* ---- process AEN? --- */
	if (test_and_clear_bit(DPC_AEN, &ha->dpc_flags))
		qla4xxx_process_aen(ha, PROCESS_ALL_AENS);

	/* ---- Get DHCP IP Address? --- */
	if (test_and_clear_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags))
		qla4xxx_get_dhcp_ip_address(ha);

#ifdef __VMKLNX__
   if (test_and_clear_bit(DPC_LINK_CHANGED, &ha->dpc_flags)) {
      if (!test_bit(AF_LINK_UP, &ha->flags)) {
         /* ---- link down? --- */
         list_for_each_entry_safe(ddb_entry, dtemp, &ha->ddb_list, list) {
            if (atomic_read(&ddb_entry->state) == DDB_STATE_ONLINE)
               qla4xxx_mark_device_missing(ha, ddb_entry);
         }
      } else {
         /* ---- link up? ---
          * F/W will auto login to all devices ONLY ONCE after
          * link up during driver initialization and runtime
          * fatal error recovery.  Therefore, the driver must
          * manually relogin to devices when recovering from
          * connection failures, logouts, expired KATO, etc. */
		qla4xxx_relogin_all_devices(ha);
      }
   }

      /* ---- offline device? --- */
      if (test_and_clear_bit(DPC_OFFLINE_DEVICE, &ha->dpc_flags)) {
         list_for_each_entry_safe(ddb_entry, dtemp, &ha->ddb_list, list) {
            if (test_and_clear_bit(DF_OFFLINE, &ddb_entry->flags)) {
               if (atomic_read(&ddb_entry->state) == DDB_STATE_DEAD) {
                  iscsi_offline_session(ddb_entry->sess);
                  dev_info(&ha->pdev->dev, "scsi%ld: %s: ddb[%d] os[%d] - "
                     "offline\n", ha->host_no, __func__, ddb_entry->fw_ddb_index,
                     ddb_entry->os_target_id);
               } else {
                  dev_info(&ha->pdev->dev, "scsi%ld: %s: ddb[%d] os[%d] - "
                     "ddb state not dead but marked for offline\n",
                     ha->host_no, __func__, ddb_entry->fw_ddb_index,
                     ddb_entry->os_target_id);
               }
            }
         }
      }

      /* ---- remove device? --- */
      if (test_bit(DPC_REMOVE_DEVICE, &ha->dpc_flags)) {
         list_for_each_entry_safe(ddb_entry, dtemp, &ha->ddb_list, list) {
            if (atomic_read(&ddb_entry->state) == DDB_STATE_DEAD) {
               if (test_and_clear_bit(DF_REMOVE, &ddb_entry->flags)) {
                  /*
		   * This is used to clean up the ddb_entry.  We first
		   * remove the ddb from the firmware map since it no
		   * longer exists in the firmware.  Then we notify
		   * vmklinux to remove the ddb.  Once in this state
		   * we marked the ddb REMOVED.  It remains in this state
		   * until vmklinux is done with the target and notifies
		   * us via target_destroy().
		   */
                  clear_bit(DPC_REMOVE_DEVICE, &ha->dpc_flags);

                  ha->fw_ddb_index_map[ddb_entry->fw_ddb_index] = NULL;
                  ha->tot_ddbs--;

                  iscsi_remove_session(ddb_entry->sess);

                  atomic_set(&ddb_entry->state, DDB_STATE_REMOVED);
                  dev_info(&ha->pdev->dev, "scsi%ld: %s: ddb[%d] os[%d] marked REMOVED\n",
                     ha->host_no, __func__, ddb_entry->fw_ddb_index,
                     ddb_entry->os_target_id);
               }
            }
         }
      }

      /* ---- free device? --- */
      if (test_and_clear_bit(DPC_FREE_DEVICE, &ha->dpc_flags)) {
         list_for_each_entry_safe(ddb_entry, dtemp, &ha->ddb_list, list) {
            if (test_and_clear_bit(DF_FREE, &ddb_entry->flags)) {
               /*
                * vmklinux has called our target destroy and confirmed
                * this ddb_entry is no longer needed.  We can now
                * safely free the associated memory.
                *
                * (We don't want to touch the ddb after we call free
                * so save off the id[s] for reporting.)
                */
               int ddb_id, os_id;
               ddb_id = ddb_entry->fw_ddb_index;
               os_id  = ddb_entry->os_target_id;

               qla4xxx_free_ddb(ha, ddb_entry);

               dev_info(&ha->pdev->dev, "scsi%ld: %s: ddb[%d] os[%d] freed\n",
                  ha->host_no, __func__, ddb_id, os_id);
            }
         }
      }

#endif /* __VMKLNX__ */

   /* ---- relogin device? --- */
   if (adapter_up(ha) &&
       test_and_clear_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags)) {
      list_for_each_entry_safe(ddb_entry, dtemp,
                &ha->ddb_list, list) {
#ifndef __VMKLNX__
         if (test_and_clear_bit(DF_RELOGIN, &ddb_entry->flags) &&
             atomic_read(&ddb_entry->state) != DDB_STATE_ONLINE)
#else
         if (test_and_clear_bit(DF_RELOGIN, &ddb_entry->flags) &&
             atomic_read(&ddb_entry->state) != DDB_STATE_REMOVED &&
             atomic_read(&ddb_entry->state) != DDB_STATE_ONLINE)
#endif
            qla4xxx_relogin_device(ha, ddb_entry);

         /*
          * If mbx cmd times out there is no point
          * in continuing further.
          * With large no of targets this can hang
          * the system.
          */
         if (test_bit(DPC_RESET_HA, &ha->dpc_flags)) {
            printk(KERN_WARNING "scsi%ld: %s: "
                   "need to reset hba\n",
                   ha->host_no, __func__);
            break;
         }
      }
   }

	/* ---- perform dynamic lun scan? --- */
	if (adapter_up(ha) &&
	    test_and_clear_bit(DPC_DYNAMIC_LUN_SCAN, &ha->dpc_flags)) {
		list_for_each_entry_safe(ddb_entry, dtemp,
		    &ha->ddb_list, list) {
			if (test_and_clear_bit(DF_DYNAMIC_LUN_SCAN_NEEDED,
			    &ddb_entry->flags)) {
				dev_info(&ha->pdev->dev,"%s: ddb[%d] os[%d] "
					 "perform dynamic lun scan\n",
                                         __func__, ddb_entry->fw_ddb_index,
					 ddb_entry->os_target_id);
#ifndef __VMKLNX__
				/* report new lun to kernel */
				scsi_scan_target(&ddb_entry->sess->dev, 0,
				    ddb_entry->sess->target_id,
				    SCAN_WILD_CARD, 0);
#else
				scsi_scan_target(ddb_entry->sess->device, 0,
            	    ddb_entry->sess->targetID,
				    SCAN_WILD_CARD, 0);
#endif
				/* report new lun to GUI */
				qla4xxx_queue_lun_change_aen(ha,
					ddb_entry->fw_ddb_index);
			}
		}
	}

#ifndef __VMKLNX__
	/* Check for ASYNC PDU IOCBs */
	if (adapter_up(ha) &&
	    test_bit(DPC_ASYNC_PDU, &ha->dpc_flags)) {

		list_for_each_entry_safe(apdu_iocb, apdu_iocb_tmp,
					 &ha->async_iocb_list, list) {
			qla4xxx_process_async_pdu_iocb(ha, apdu_iocb);
			list_del_init(&apdu_iocb->list);
			kfree(apdu_iocb);
		}
		clear_bit(DPC_ASYNC_PDU, &ha->dpc_flags);
	}
#endif

}

/**
 * qla4xxx_free_adapter - release the adapter
 * @ha: pointer to adapter structure
 **/
static void qla4xxx_free_adapter(struct scsi_qla_host *ha)
{

   if (test_bit(AF_INTERRUPTS_ON, &ha->flags)) {
      /* Turn-off interrupts on the card. */
      qla4xxx_disable_intrs(ha);
   }

   /* Remove timer thread, if present */
   if (ha->timer_active)
      qla4xxx_stop_timer(ha);

   /* Kill the kernel thread for this host */
   if (ha->dpc_thread)
      destroy_workqueue(ha->dpc_thread);

   /* Issue Soft Reset to put firmware in unknown state */
   if (ql4xxx_lock_drvr_wait(ha) == QLA_SUCCESS)
      qla4xxx_hw_reset(ha);

   /* Detach interrupts */
   if (test_and_clear_bit(AF_IRQ_ATTACHED, &ha->flags))
      free_irq(ha->pdev->irq, ha);

   /* free extra memory */
   qla4xxx_mem_free(ha);

   pci_disable_device(ha->pdev);
}

/***
 * qla4xxx_iospace_config - maps registers
 * @ha: pointer to adapter structure
 *
 * This routines maps HBA's registers from the pci address space
 * into the kernel virtual address space for memory mapped i/o.
 **/
static int qla4xxx_iospace_config(struct scsi_qla_host *ha)
{
   unsigned long pio, pio_len, pio_flags;
   unsigned long mmio, mmio_len, mmio_flags;

   pio = pci_resource_start(ha->pdev, 0);
   pio_len = pci_resource_len(ha->pdev, 0);
   pio_flags = pci_resource_flags(ha->pdev, 0);
   if (pio_flags & IORESOURCE_IO) {
      if (pio_len < MIN_IOBASE_LEN) {
         dev_warn(&ha->pdev->dev,
            "Invalid PCI I/O region size\n");
         pio = 0;
      }
   } else {
      dev_warn(&ha->pdev->dev, "region #0 not a PIO resource\n");
      pio = 0;
   }

   /* Use MMIO operations for all accesses. */
   mmio = pci_resource_start(ha->pdev, 1);
   mmio_len = pci_resource_len(ha->pdev, 1);
   mmio_flags = pci_resource_flags(ha->pdev, 1);

   if (!(mmio_flags & IORESOURCE_MEM)) {
      dev_err(&ha->pdev->dev,
         "region #0 not an MMIO resource, aborting\n");

      goto iospace_error_exit;
   }
   if (mmio_len < MIN_IOBASE_LEN) {
      dev_err(&ha->pdev->dev,
         "Invalid PCI mem region size, aborting\n");
      goto iospace_error_exit;
   }

   if (pci_request_regions(ha->pdev, DRIVER_NAME)) {
      dev_warn(&ha->pdev->dev,
         "Failed to reserve PIO/MMIO regions\n");

      goto iospace_error_exit;
   }

   ha->pio_address = pio;
   ha->pio_length = pio_len;
   ha->reg = ioremap(mmio, MIN_IOBASE_LEN);
   if (!ha->reg) {
      dev_err(&ha->pdev->dev,
         "cannot remap MMIO, aborting\n");

      goto iospace_error_exit;
   }

   return 0;

iospace_error_exit:
   return -ENOMEM;
}

static void ql4_get_aen_log(struct scsi_qla_host *ha, struct ql4_aen_log *aenl)
{
   if (aenl) {
      memcpy(aenl, &ha->aen_log, sizeof (ha->aen_log));
      ha->aen_log.count = 0;
   }
}

/**
 * qla4xxx_probe_adapter - callback function to probe HBA
 * @pdev: pointer to pci_dev structure
 * @pci_device_id: pointer to pci_device entry
 *
 * This routine will probe for Qlogic 4xxx iSCSI host adapters.
 * It returns zero if successful. It also initializes all data necessary for
 * the driver.
 **/
static int __devinit qla4xxx_probe_adapter(struct pci_dev *pdev,
                  const struct pci_device_id *ent)
{
   int ret = -ENODEV, status;
   struct Scsi_Host *host;
   struct scsi_qla_host *ha;
   struct ddb_entry *ddb_entry, *ddbtemp;
   uint8_t init_retry_count = 0;
   char buf[34];

   if (pci_enable_device(pdev))
	   return -1;

   host = scsi_host_alloc(&qla4xxx_driver_template, sizeof(*ha));
   if (host == NULL) {
      printk(KERN_WARNING
             "qla4xxx: Couldn't allocate host from scsi layer!\n");
      goto probe_disable_device;
   }

   /* Clear our data area */
   ha = (struct scsi_qla_host *) host->hostdata;
   memset(ha, 0, sizeof(*ha));

   /* Save the information from PCI BIOS. */
   ha->pdev = pdev;
   ha->host = host;
   ha->host_no = host->host_no;

   ha->ql4mbx = qla4xxx_mailbox_command;
   ha->ql4cmd = qla4xxx_send_command_to_isp;
   ha->ql4getaenlog = ql4_get_aen_log;

   /* Configure PCI I/O space. */
   ret = qla4xxx_iospace_config(ha);
   if (ret)
      goto probe_failed;

   dev_info(&ha->pdev->dev, "Found an ISP%04x, irq %d, iobase 0x%p\n",
         pdev->device, pdev->irq, ha->reg);

   qla4xxx_config_dma_addressing(ha);

   /* Initialize lists and spinlocks. */
   INIT_LIST_HEAD(&ha->ddb_list);
#ifndef __VMKLNX__
   INIT_LIST_HEAD(&ha->async_iocb_list);
#endif

   mutex_init(&ha->mbox_sem);

   spin_lock_init(&ha->hardware_lock);
   spin_lock_init(&ha->list_lock);

   /* Allocate dma buffers */
   if (qla4xxx_mem_alloc(ha)) {
      dev_warn(&ha->pdev->dev,
            "[ERROR] Failed to allocate memory for adapter\n");

      ret = -ENOMEM;
      goto probe_failed;
   }

   /*
    * Initialize the Host adapter request/response queues and
    * firmware
    * NOTE: interrupts enabled upon successful completion
    */
   status = qla4xxx_initialize_adapter(ha, REBUILD_DDB_LIST);
   while (status == QLA_ERROR && init_retry_count++ < MAX_INIT_RETRIES) {
      DEBUG2(printk(KERN_ERR "scsi%ld: %s: retrying adapter initialization "
               "(%d)\n", ha->host_no, __func__, init_retry_count));
      qla4xxx_soft_reset(ha);
      status = qla4xxx_initialize_adapter(ha, REBUILD_DDB_LIST);
   }
   if (status == QLA_ERROR) {
      dev_warn(&ha->pdev->dev, "Failed to initialize adapter\n");

      ret = -ENODEV;
      goto probe_failed;
   }

   host->cmd_per_lun = 3;
   host->max_channel = 0;
   host->max_lun = MAX_LUNS - 1;
   host->max_id = MAX_TARGETS;
   host->max_cmd_len = IOCB_MAX_CDB_LEN;
   host->transportt = qla4xxx_scsi_transport;

#if defined(__VMKLNX__)
   /* Register second-level lun addressing capability */
   if (vmklnx_scsi_host_set_capabilities(host,
                                         SHOST_CAP_SECONDLEVEL_ADDRESSING)) {
      dev_warn(&ha->pdev->dev, "Failed to set capability: 0x%x for adapter.\n",
               SHOST_CAP_SECONDLEVEL_ADDRESSING);
   }
#endif

   ha->max_q_depth = MAX_Q_DEPTH;
   if (ql4xmaxqdepth != 0 && ql4xmaxqdepth <= 0xffffU)
      ha->max_q_depth = ql4xmaxqdepth;

   /* Startup the kernel thread for this host adapter. */
   DEBUG2(printk("scsi: %s: Starting kernel thread for "
            "qla4xxx_dpc\n", __func__));
   sprintf(buf, "qla4xxx_%lu_dpc", ha->host_no);
   ha->dpc_thread = create_singlethread_workqueue(buf);
   if (!ha->dpc_thread) {
      dev_warn(&ha->pdev->dev, "Unable to start DPC thread!\n");
      ret = -ENODEV;
      goto probe_failed;
   }
#if defined(__VMKLNX__)
   INIT_WORK(&ha->dpc_work, qla4xxx_do_dpc);
#else /* !defined(__VMKLNX__) */
   INIT_WORK(&ha->dpc_work, qla4xxx_do_dpc, ha);
#endif /* defined(__VMKLNX__) */

   ret = request_irq(pdev->irq, qla4xxx_intr_handler,
           SA_INTERRUPT|SA_SHIRQ, "qla4xxx", ha);
   if (ret) {
      dev_warn(&ha->pdev->dev, "Failed to reserve interrupt %d"
         " already in use.\n", pdev->irq);
      goto probe_failed;
   }
   set_bit(AF_IRQ_ATTACHED, &ha->flags);
   host->irq = pdev->irq;
   DEBUG(printk("scsi%ld: irq %d attached\n", ha->host_no, ha->pdev->irq));

   qla4xxx_enable_intrs(ha);

   /* Start timer thread. */
   qla4xxx_start_timer(ha, qla4xxx_timer, 1);

   pci_set_drvdata(pdev, ha);

   ret = scsi_add_host(host, &pdev->dev);
   if (ret)
      goto probe_failed;

#ifdef __VMKLNX__
   if ((ret = iscsi_register_host(host, &qla4xxx_iscsi_transport)) != 0) {
      goto remove_host;
   }

   vmklnx_scsi_register_poll_handler(host, pdev->irq,
                                     qla4xxx_intr_handler, ha);
#endif

   /* Update transport device information for all devices. */
   list_for_each_entry_safe(ddb_entry, ddbtemp, &ha->ddb_list, list) {

      if (ddb_entry->fw_ddb_device_state == DDB_DS_SESSION_ACTIVE)
         set_bit(DF_SCAN_ISSUED, &ddb_entry->flags);

      if (qla4xxx_add_sess(ddb_entry,
         test_bit(DF_SCAN_ISSUED, &ddb_entry->flags)))
#ifdef __VMKLNX__
         goto remove_adaptor;
#else
         goto remove_host;
#endif
      if (!test_bit(DF_SCAN_ISSUED, &ddb_entry->flags))
         qla4xxx_mark_device_missing(ha, ddb_entry);
   }

   printk(KERN_INFO
          " QLogic iSCSI HBA Driver version: %s\n"
          "  QLogic ISP%04x @ %s, pdev = %p host#=%ld, fw=%02d.%02d.%02d.%02d\n",
          qla4xxx_version_str, ha->pdev->device, pci_name(ha->pdev), pdev,
          ha->host_no, ha->firmware_version[0], ha->firmware_version[1],
          ha->patch_number, ha->build_number);

        /* Insert new entry into the list of adapters. */
   klist_add_tail(&ha->node, &qla4xxx_hostlist);
   ha->instance = atomic_inc_return(&qla4xxx_hba_count) - 1;
#ifdef __VMKLNX__
   if (ql4im_mem_alloc(ha->instance, ha)) {
      printk(KERN_INFO "ql4im_mem_alloc failed host %ld inst %d\n",
         ha->host_no, ha->instance);
      goto remove_adaptor;
   }
#endif

        DEBUG2(printk("qla4xxx: listhead=%p, done adding ha=%p i=%d\n",
            &qla4xxx_hostlist, &ha->node, ha->instance));
   set_bit(AF_INIT_DONE, &ha->flags);
   return 0;

#ifdef __VMKLNX__
remove_adaptor:
   (void)iscsi_unregister_host(host, &qla4xxx_iscsi_transport);
#endif

remove_host:
   qla4xxx_free_ddb_list(ha);
   scsi_remove_host(host);

probe_failed:
   qla4xxx_free_adapter(ha);
   scsi_host_put(ha->host);

probe_disable_device:
   pci_disable_device(pdev);
   return ret;
}

/**
 * qla4xxx_remove_adapter - calback function to remove adapter.
 * @pci_dev: PCI device pointer
 **/
static void __devexit qla4xxx_remove_adapter(struct pci_dev *pdev)
{
   struct scsi_qla_host *ha;

   ha = pci_get_drvdata(pdev);

   qla4xxx_disable_intrs(ha);

   while (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags))
      ssleep(1);

   klist_remove(&ha->node);
   atomic_dec(&qla4xxx_hba_count);

   /* remove devs from iscsi_sessions to scsi_devices */
   qla4xxx_free_ddb_list(ha);

#ifdef __VMKLNX__
   (void)iscsi_unregister_host(ha->host, &qla4xxx_iscsi_transport);
#endif

   scsi_remove_host(ha->host);

#ifdef __VMKLNX__
   ql4im_mem_free(ha->instance);
#endif
   qla4xxx_free_adapter(ha);

   scsi_host_put(ha->host);

   pci_set_drvdata(pdev, NULL);
}

/**
 * qla4xxx_config_dma_addressing() - Configure OS DMA addressing method.
 * @ha: HA context
 *
 * At exit, the @ha's flags.enable_64bit_addressing set to indicated
 * supported addressing method.
 */
void qla4xxx_config_dma_addressing(struct scsi_qla_host *ha)
{
   int retval;

   /* Update our PCI device dma_mask for full 64 bit mask */
   if (pci_set_dma_mask(ha->pdev, DMA_64BIT_MASK) == 0) {
      if (pci_set_consistent_dma_mask(ha->pdev, DMA_64BIT_MASK)) {
         dev_dbg(&ha->pdev->dev,
              "Failed to set 64 bit PCI consistent mask; "
               "using 32 bit.\n");
         retval = pci_set_consistent_dma_mask(ha->pdev,
                          DMA_32BIT_MASK);
      }
   } else
      retval = pci_set_dma_mask(ha->pdev, DMA_32BIT_MASK);
}

static int qla4xxx_slave_alloc(struct scsi_device *sdev)
{
#ifndef __VMKLNX__
   struct iscsi_cls_session *sess = starget_to_session(sdev->sdev_target);
#else
   struct iscsi_cls_session *sess = iscsi_sdevice_to_session(sdev);
#endif

   if (sess) {
      sdev->hostdata = sess->dd_data;
#ifdef __VMKLNX__
      sdev->sdev_target->hostdata = sess->dd_data;
#endif
      return 0;
   }
   return FAILED;
}

static int qla4xxx_slave_configure(struct scsi_device *sdev)
{
	struct scsi_qla_host *ha = shost_priv(sdev->host);

	if (sdev->tagged_supported)
           scsi_activate_tcq(sdev, ha->max_q_depth);
	else
		scsi_deactivate_tcq(sdev, ha->max_q_depth);

	return 0;
}

#ifdef __VMKLNX__
static void qla4xxx_target_destroy(struct scsi_target *starget)
{
   struct ddb_entry *ddb_entry = starget->hostdata;
   struct scsi_qla_host *ha;

   if (!ddb_entry || !ddb_entry->ha) {
      return;
   }
   ha = ddb_entry->ha;

   set_bit(DF_FREE, &ddb_entry->flags);
   set_bit(DPC_FREE_DEVICE, &ha->dpc_flags);

   DEBUG2(printk("scsi%ld: %s: scheduling dpc routine - dpc flags = "
            "0x%lx\n", ha->host_no, __func__, ha->dpc_flags));
   queue_work(ha->dpc_thread, &ha->dpc_work);
}
#endif

/**
 * qla4xxx_del_from_active_array - returns an active srb
 * @ha: Pointer to host adapter structure.
 * @index: index into to the active_array
 *
 * This routine removes and returns the srb at the specified index
 **/
struct srb * qla4xxx_del_from_active_array(struct scsi_qla_host *ha, uint32_t index)
{
   struct srb *srb = NULL;

   /* validate handle and remove from active array */
   if (index >= MAX_SRBS)
      return srb;

   srb = ha->active_srb_array[index];
   ha->active_srb_array[index] = NULL;
   if (!srb)
      return srb;

   /* update counters */
   if (srb->flags & SRB_DMA_VALID) {
      ha->req_q_count += srb->iocb_cnt;
      ha->iocb_cnt -= srb->iocb_cnt;
      if (srb->cmd)
         srb->cmd->host_scribble = NULL;
   }
   return srb;
}

/**
 * qla4xxx_eh_wait_on_command - waits for command to be returned by firmware
 * @ha: actual ha whose done queue will contain the comd returned by firmware.
 * @cmd: Scsi Command to wait on.
 * @got_ref: Additional reference retrieved by caller.
 *
 * This routine waits for the command to be returned by the Firmware
 * for some max time.
 **/
static int qla4xxx_eh_wait_on_command(struct scsi_qla_host *ha,
                  struct scsi_cmnd *cmd, int got_ref)
{
#define ABORT_POLLING_PERIOD	1000
#define ABORT_WAIT_ITER		1
   int done = 0;
   struct srb *rp;
   unsigned long wait_iter = ABORT_WAIT_ITER;

   do {
      /* Checking to see if its returned to OS */
      rp = (struct srb *) cmd->SCp.ptr;
      if (rp == NULL) {
         done++;
         break;
      }

      if (got_ref && (atomic_read(&rp->ref_count) == 1)) {
         done++;
         break;
      }

      msleep(ABORT_POLLING_PERIOD);
   } while (!(--wait_iter));

   return done;
}

/**
 * qla4xxx_wait_for_hba_online - waits for HBA to come online
 * @ha: Pointer to host adapter structure
 **/
static int qla4xxx_wait_for_hba_online(struct scsi_qla_host *ha)
{
   unsigned long wait_online = 60;

   while (wait_online--) {
      if (adapter_up(ha))
         return QLA_SUCCESS;
      ssleep(2);
   }
   return QLA_ERROR;
}

/**
 * qla4xxx_eh_wait_for_active_target_commands - wait for active cmds to finish.
 * @ha: pointer to to HBA
 * @t: target id
 * @l: lun id
 *
 * This function waits for all outstanding commands to a lun to complete. It
 * returns 0 if all pending commands are returned and 1 otherwise.
 **/
static int qla4xxx_eh_wait_for_active_target_commands(struct scsi_qla_host *ha,
                   int t, int l)
{
   int cnt;
   int status;
   struct srb *sp;
   struct scsi_cmnd *cmd;

   /*
    * Waiting for all commands for the designated target in the active
    * array
    */
   status = 0;
   for (cnt = 1; cnt < MAX_SRBS; cnt++) {
      spin_lock(&ha->hardware_lock);
      sp = ha->active_srb_array[cnt];
      if (sp) {
         cmd = sp->cmd;
         spin_unlock(&ha->hardware_lock);
         if (cmd->device->id == t && cmd->device->lun == l) {
            if (!qla4xxx_eh_wait_on_command(ha, cmd, 0)) {
               status++;
               break;
            }
         }
      } else {
         spin_unlock(&ha->hardware_lock);
      }
   }
   return status;
}

/**
 * qla4xxx_eh_abort - callback for abort task.
 * @cmd: Pointer to Linux's SCSI command structure
 *
 * This routine is called by the Linux OS to abort the specified
 * command.
 **/
static int qla4xxx_eh_abort(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
	struct srb *srb = NULL;
	int ret = FAILED;
	unsigned int channel = cmd->device->channel;
	unsigned int id = cmd->device->id;
	unsigned int lun = cmd->device->lun;
	unsigned long serial = cmd->serial_number;
	int i = 0;
	int got_ref = 0;
	unsigned long flags = 0;

	if (!cmd->SCp.ptr) {
		DEBUG2(printk("scsi%ld: ABORT - cmd already completed.\n",
			      ha->host_no));
#if !defined(__VMKLNX__)
		return SUCCESS;
#else
		return FAILED;
#endif
	}

	srb = (struct srb *) cmd->SCp.ptr;

	dev_info(&ha->pdev->dev, "scsi%ld:%d:%d:%d: ABORT ISSUED "
		 "cmd=%p, pid=%ld, ref=%d\n", ha->host_no, channel, id, lun,
		 cmd, serial, atomic_read(&srb->ref_count));

	/* Check active list for command */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (i=1; i < MAX_SRBS; i++) {
		srb =  ha->active_srb_array[i];

		if (srb == NULL)
			continue;

		if (srb->cmd != cmd)
			continue;

                DEBUG2(printk("scsi%ld:%d:%d:%d %s: aborting srb %p from RISC. "
			      "pid=%ld.\n", ha->host_no, channel, id, lun,
			      __func__, srb, serial));
		DEBUG3(qla4xxx_print_scsi_cmd(cmd));

		/* Get a reference to the sp and drop the lock.*/
		sp_get(srb);
		got_ref++;
		spin_unlock_irqrestore(&ha->hardware_lock, flags);

		if (qla4xxx_abort_task(ha, srb) != QLA_SUCCESS) {
			dev_info(&ha->pdev->dev,
				"scsi%ld:%d:%d:%d: ABORT TASK - FAILED.\n",
				ha->host_no, channel, id, lun);
		} else {
			dev_info(&ha->pdev->dev,
				"scsi%ld:%d:%d:%d: ABORT TASK - mbx success.\n",
				ha->host_no, channel, id, lun);
		}
		spin_lock_irqsave(&ha->hardware_lock, flags);
		break;
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	/* Wait for command to complete */
	if (qla4xxx_eh_wait_on_command(ha, cmd, got_ref)) {
		dev_info(&ha->pdev->dev,
			"scsi%ld:%d:%d:%d: ABORT SUCCEEDED - "
			 "cmd returned back to OS.\n",
			 ha->host_no, channel, id, lun);
		ret = SUCCESS;
	}

	if (got_ref)
		sp_put(ha, srb);

	DEBUG2(printk("scsi%ld:%d:%d:%d: ABORT cmd=%p, pid=%ld, ret=%x\n",
		      ha->host_no, channel, id, lun, cmd, serial, ret));
	return ret;
}

/**
 * qla4xxx_eh_device_reset - callback for target reset.
 * @cmd: Pointer to Linux's SCSI command structure
 *
 * This routine is called by the Linux OS to reset all luns on the
 * specified target.
 **/
static int qla4xxx_eh_device_reset(struct scsi_cmnd *cmd)
{
   struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
   struct ddb_entry *ddb_entry = cmd->device->hostdata;
   unsigned int channel = cmd->device->channel;
   unsigned int id = cmd->device->id;
   unsigned int lun = cmd->device->lun;
   int ret = FAILED, stat;
#if defined(__VMKLNX__)
   uint64_t sllid;
#endif

   if (!ddb_entry)
      return ret;

   dev_info(&ha->pdev->dev,
         "scsi%ld:%d:%d:%d: DEVICE RESET ISSUED.\n", ha->host_no,
         channel, id, lun);

   DEBUG2(printk(KERN_INFO
            "scsi%ld: DEVICE_RESET cmd=%p jiffies = 0x%lx, to=%x,"
            "dpc_flags=%lx, status=%x allowed=%d\n", ha->host_no,
            cmd, jiffies, cmd->timeout_per_command / HZ,
            ha->dpc_flags, cmd->result, cmd->allowed));

#ifndef __VMKLNX__
   if (qla4xxx_wait_for_hba_online(ha) != QLA_SUCCESS) {
      dev_info(&ha->pdev->dev, "%s: HBA OFFLINE: FAILED\n", __func__);
      return FAILED;
   }

   stat = qla4xxx_reset_lun(ha, ddb_entry, lun);
   if (stat != QLA_SUCCESS) {
      dev_info(&ha->pdev->dev, "DEVICE RESET FAILED. %d\n", stat);
      goto eh_dev_reset_done;
   }
#else
   /* NOTE: We do not want to wait for device (or HBA) to come online during
    * eh_xx_reset in a Hyper Visor Environment.  There is a timing window
    * in which a reset could be issued to clear a reservation and the
    * path fails at the same time, which could lead to a long timeout and
    * cause ESX operations like VMotion, clones, etc to fail. This
    * could also cause a root file system on a guest to go read-only.
    */
   /* If the adapter is not up fail the request. */
   if (!adapter_up(ha)) {
      dev_info(&ha->pdev->dev,
         "scsi%ld:%d:%d:%d: DEVICE RESET - ADAPTER DOWN\n",
         ha->host_no, channel, id, lun);
      goto eh_dev_reset_done;
   }

   /* If the ddb is not online fail the request. */
   if (atomic_read(&ddb_entry->state) != DDB_STATE_ONLINE) {
      dev_info(&ha->pdev->dev,
         "scsi%ld:%d:%d:%d: DEVICE RESET - DDB NOT-ONLINE\n",
         ha->host_no, channel, id, lun);
      goto eh_dev_reset_done;
   }

   stat = qla4xxx_reset_lun(ha, ddb_entry, lun);
   if (stat != QLA_SUCCESS) {
      /* The reset request failed. */
      dev_info(&ha->pdev->dev,
         "scsi%ld:%d:%d:%d: DEVICE RESET - FAILED(%d).\n",
         ha->host_no, channel, id, lun, stat);
      goto eh_dev_reset_done;
   }
#endif

   /*
    * If we are coming down the EH path, wait for all commands to complete
    * for the device.
    */
   if (cmd->device->host->shost_state == SHOST_RECOVERY) {
      if (qla4xxx_eh_wait_for_active_target_commands(ha, id, lun)) {
         dev_info(&ha->pdev->dev,
               "DEVICE RESET FAILED - waiting for "
               "commands.\n");
         goto eh_dev_reset_done;
      }
   }
#if defined(__VMKLNX__)
   sllid = vmklnx_scsi_cmd_get_secondlevel_lun_id(cmd);
   if (qla4xxx_send_marker_iocb(ha, ddb_entry, lun, sllid)
#else
   if (qla4xxx_send_marker_iocb(ha, ddb_entry, lun)
#endif
      != QLA_SUCCESS)
      goto eh_dev_reset_done;

   dev_info(&ha->pdev->dev,
         "scsi(%ld:%d:%d:%d): DEVICE RESET SUCCEEDED.\n",
         ha->host_no, channel, id, lun);

   ret = SUCCESS;

eh_dev_reset_done:

   return ret;
}

/**
 * qla4xxx_eh_host_reset - kernel callback
 * @cmd: Pointer to Linux's SCSI command structure
 *
 * This routine is invoked by the Linux kernel to perform fatal error
 * recovery on the specified adapter.
 **/
static int qla4xxx_eh_host_reset(struct scsi_cmnd *cmd)
{
   int return_status = FAILED;
   struct scsi_qla_host *ha;

   ha = (struct scsi_qla_host *) cmd->device->host->hostdata;

   dev_info(&ha->pdev->dev, "%s: HOST RESET ISSUED.\n", __func__);

   if (qla4xxx_wait_for_hba_online(ha) != QLA_SUCCESS) {
      dev_info(&ha->pdev->dev, "%s: HBA OFFLINE: FAILED\n", __func__);
      return FAILED;
   }

   if (qla4xxx_recover_adapter(ha) == QLA_SUCCESS) {
      return_status = SUCCESS;
   }

   dev_info(&ha->pdev->dev, "HOST RESET %s.\n",
         return_status == FAILED ? "FAILED" : "SUCCEDED");

   return return_status;
}


static struct pci_device_id qla4xxx_pci_tbl[] = {
   {
      .vendor      = PCI_VENDOR_ID_QLOGIC,
      .device      = PCI_DEVICE_ID_QLOGIC_ISP4010,
      .subvendor   = PCI_ANY_ID,
      .subdevice   = PCI_ANY_ID,
   },
   {
      .vendor      = PCI_VENDOR_ID_QLOGIC,
      .device      = PCI_DEVICE_ID_QLOGIC_ISP4022,
      .subvendor   = PCI_ANY_ID,
      .subdevice   = PCI_ANY_ID,
   },
   {
      .vendor      = PCI_VENDOR_ID_QLOGIC,
      .device      = PCI_DEVICE_ID_QLOGIC_ISP4032,
      .subvendor   = PCI_ANY_ID,
      .subdevice   = PCI_ANY_ID,
   },
   {0, 0},
};
MODULE_DEVICE_TABLE(pci, qla4xxx_pci_tbl);

struct pci_driver qla4xxx_pci_driver = {
   .name    = DRIVER_NAME,
   .id_table   = qla4xxx_pci_tbl,
   .probe      = qla4xxx_probe_adapter,
   .remove      = qla4xxx_remove_adapter,
};

static int __init qla4xxx_module_init(void)
{
   int ret;

   atomic_set(&qla4xxx_hba_count, 0);
   klist_init(&qla4xxx_hostlist, NULL, NULL);
   /* Allocate cache for SRBs. */
   srb_cachep = kmem_cache_create("qla4xxx_srbs", sizeof(struct srb), 0,
                   SLAB_HWCACHE_ALIGN, NULL, NULL);
   if (srb_cachep == NULL) {
      printk(KERN_ERR
             "%s: Unable to allocate SRB cache..."
             "Failing load!\n", DRIVER_NAME);
      ret = -ENOMEM;
      goto no_srp_cache;
   }

   /* Derive version string. */
   strcpy(qla4xxx_version_str, QLA4XXX_DRIVER_VERSION);
   if (extended_error_logging)
      strcat(qla4xxx_version_str, "-debug");

   qla4xxx_scsi_transport =
      iscsi_register_transport(&qla4xxx_iscsi_transport);
   if (!qla4xxx_scsi_transport){
      ret = -ENODEV;
      goto release_srb_cache;
   }

#ifdef __VMKLNX__
   ret = ql4im_init();
   if (ret) {
      printk(KERN_INFO "QLogic iSCSI HBA Driver ioctl init failed\n");
      goto unregister_transport;
   }
#endif
   printk(KERN_INFO "QLogic iSCSI HBA Driver\n");
   ret = pci_register_driver(&qla4xxx_pci_driver);
   if (ret)
      goto unregister_transport;

   printk(KERN_INFO "QLogic iSCSI HBA Driver\n");
   return 0;
unregister_transport:
   iscsi_unregister_transport(&qla4xxx_iscsi_transport);
release_srb_cache:
   kmem_cache_destroy(srb_cachep);
no_srp_cache:
   return ret;
}

static void __exit qla4xxx_module_exit(void)
{
   ql4_mod_unload = 1;
#ifdef __VMKLNX__
   ql4im_exit();
#endif
   pci_unregister_driver(&qla4xxx_pci_driver);
   iscsi_unregister_transport(&qla4xxx_iscsi_transport);
   kmem_cache_destroy(srb_cachep);
}

module_init(qla4xxx_module_init);
module_exit(qla4xxx_module_exit);

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic iSCSI HBA Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(QLA4XXX_DRIVER_VERSION);
