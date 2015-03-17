/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2000-2012 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
/*
 * This file contains VMware-specific changes to the standard Linux hpsa driver.
 */

/*************
 * INCLUDES
 *************/
#include <scsi/scsi_transport_sas.h>
#include <linux/proc_fs.h>
#include "hpsa_cmd.h"
/**************
 * DEFINES 
 **************/
enum {
	SCSI_QDEPTH_DEFAULT,	/* default requested change, e.g. from sysfs */
	SCSI_QDEPTH_QFULL,	/* scsi-ml requested due to queue full */
	SCSI_QDEPTH_RAMP_UP,	/* scsi-ml requested due to threshhold event */
};

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#define CONTROLLER_DEVICE 7     /* Physical device type of controller */
#define MAX_CTLR        10       /* Only support 10 controllers in VMware */
#define HPSA_VMWARE_SGLIMIT 129	/* limit maxsgentries to conserve heap */
struct scsi_transport_template *hpsa_transport_template = NULL;

#define ENG_GIG 1000000000
#define ENG_GIG_FACTOR (ENG_GIG/512)

/**************
 * PROTOTYPES *
 **************/
const char * scsi_device_type(unsigned type);
int scsi_dma_map(struct scsi_cmnd *cmd);
void scsi_dma_unmap(struct scsi_cmnd *cmd);
static void hpsa_dbmsg(struct ctlr_info *h, int debuglvl, char *fmt, ...);
static void hpsa_db_dev(char * label, struct ctlr_info *h, 
	struct hpsa_scsi_dev_t *d, int dnum);
static void hpsa_db_sdev(char * label, struct ctlr_info *h, struct scsi_device *d);
static void hpsa_db_scmnd(char * label, struct ctlr_info *h, struct scsi_cmnd *c);
static int 
hpsa_set_sas_ids(struct ctlr_info *h,
        int reportlunsize,
        struct ReportExtendedLUNdata *physdev,
        struct SenseSubsystem_info *senseinfo,
        u32 *nphysicals);
static int 
hpsa_sense_subsystem_info(struct ctlr_info *h,
                struct SenseSubsystem_info *buf, int bufsize);
static int 
hpsa_get_linkerrors(struct sas_phy *phy);
static int 
hpsa_get_enclosure_identifier(struct sas_rphy *rphy, u64 *identifier);
static int
hpsa_get_bay_identifier(struct sas_rphy *rphy);
static int
hpsa_phy_reset(struct sas_phy *phy, int hard_reset);
static int
hpsa_get_initiator_sas_identifier(struct Scsi_Host *sh, u64 *sas_id);
static int
hpsa_get_target_sas_identifier(struct scsi_target *starget, u64 *sas_id);
static int 
hpsa_proc_get_info(char *buffer, char **start, off_t offset,
	int length, int *eof, void *data);
static void __devinit 
hpsa_procinit(struct ctlr_info *h);
static int 
hpsa_set_debug(struct ctlr_info *h, char *cmd);
static void 
hpsa_set_tmf_support(struct ctlr_info *h);
static int
hpsa_cmd_waiting(struct ctlr_info *h, struct scsi_cmnd *scsicmd);
static int 
decode_CC( struct ctlr_info *h, struct CommandList *cp,
        char *msg, int *ml, char *dmsg, int *dl);
#ifdef HPSA_DEBUG
static int is_abort_cmd(struct CommandList *c);
#endif
static int hpsa_create_rescan(struct ctlr_info *h);
static int hpsa_do_rescan(void *data);
static void hpsa_stop_rescan(struct ctlr_info *h);
static int hpsa_char_open (struct inode *inode, struct file *filep);
static int hpsa_char_ioctl(struct inode *inode, struct file *f, unsigned cmd, 
	unsigned long arg);
static long hpsa_char_compat_ioctl(struct file *f, unsigned cmd, unsigned long arg);
static int hpsa_register_controller(struct ctlr_info *h);
static int
is_keyword(char *ptr, int len, char *verb);
static int
hpsa_scsi_user_command(struct ctlr_info *h, char *buffer, int length);
static int
hpsa_scsi_proc_info(struct Scsi_Host *sh,
	char *buffer,	/* data buffer */
	char **start,	/* where data in buffer starts */
	off_t offset,	/* offset from start of imaginary file */
	int length,	/* length of data in buffer */
	int func);	/* 0 == read, 1 == write */
static inline void 
hpsa_limit_maxsgentries(struct ctlr_info *h);

/**************
 * STRUCTURES
 **************/
u64 target_sas_id[MAX_CTLR][MAX_EXT_TARGETS];
u64 cntl_sas_id[MAX_CTLR];
static struct sas_function_template hpsa_transport_functions = {
	.get_linkerrors			= hpsa_get_linkerrors,
	.get_enclosure_identifier	= hpsa_get_enclosure_identifier,
	.get_bay_identifier		=  hpsa_get_bay_identifier,
	.phy_reset			= hpsa_phy_reset,
	.get_initiator_sas_identifier	= hpsa_get_initiator_sas_identifier,
	.get_target_sas_identifier	= hpsa_get_target_sas_identifier,
};
static struct proc_dir_entry *proc_hpsa;

/* translate scsi cmd->result values into english */
static const char *host_status[] = 
{ 	"OK", 		/* 0x00 */
	"NO_CONNECT", 	/* 0x01 */
	"BUS_BUSY",	/* 0x02 */
	"TIME_OUT", 	/* 0x03 */
	"BAD_TARGET",	/* 0x04 */
	"ABORT",	/* 0x05 */
	"PARITY",	/* 0x06 */
	"ERROR",	/* 0x07 */
	"RESET",	/* 0x08 */
	"BAD_INTR",	/* 0x09 */
	"PASSTHROUGH",	/* 0x0a */
	"SOFT_ERROR",	/* 0x0b */
	"IMM_RETRY",	/* 0x0c */
	"REQUEUE"	/* 0x0d */
};

/* Character controller device operations */
static const struct file_operations hpsa_char_fops = {
	.owner		= THIS_MODULE,
	.open		= hpsa_char_open,
	.ioctl		= hpsa_char_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hpsa_char_compat_ioctl,
#endif
};

/* Keep a link between character devices and matching controllers */
struct ctlr_rep {
	int chrmajor;		/* major dev# of ctlr's character device */
	struct ctlr_info *h;	/* pointer to driver's matching ctlr struct */
};
static struct ctlr_rep cch_devs[MAX_CTLR]; /* ctlr char devices array */ 

