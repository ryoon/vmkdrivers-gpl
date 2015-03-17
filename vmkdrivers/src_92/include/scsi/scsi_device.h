/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
#ifndef _SCSI_SCSI_DEVICE_H
#define _SCSI_SCSI_DEVICE_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

struct request_queue;
struct scsi_cmnd;
struct scsi_lun;
struct scsi_sense_hdr;

struct scsi_mode_data {
	__u32	length;
	__u16	block_descriptor_length;
	__u8	medium_type;
	__u8	device_specific;
	__u8	header_length;
	__u8	longlba:1;
};

/*
 * sdev state: If you alter this, you also need to alter scsi_sysfs.c
 * (for the ascii descriptions) and the state model enforcer:
 * scsi_lib:scsi_device_set_state().
 */
enum scsi_device_state {
	SDEV_CREATED = 1,	/* device created but not added to sysfs
				 * Only internal commands allowed (for inq) */
	SDEV_RUNNING,		/* device properly configured
				 * All commands allowed */
	SDEV_CANCEL,		/* beginning to delete device
				 * Only error handler commands allowed */
	SDEV_DEL,		/* device deleted 
				 * no commands allowed */
	SDEV_QUIESCE,		/* Device quiescent.  No block commands
				 * will be accepted, only specials (which
				 * originate in the mid-layer) */
	SDEV_OFFLINE,		/* Device offlined (by error handling or
				 * user request */
	SDEV_BLOCK,		/* Device blocked by scsi lld.  No scsi 
				 * commands from user or midlayer should be issued
				 * to the scsi lld. */
};

struct scsi_device {
	struct Scsi_Host *host;
	struct request_queue *request_queue;

	/* the next two are protected by the host->host_lock */
	struct list_head    siblings;   /* list of all devices on this host */
	struct list_head    same_target_siblings; /* just the devices sharing same target id */

	/* this is now protected by the request_queue->queue_lock */
	unsigned int device_busy;	/* commands actually active on
					 * low-level. protected by queue_lock. */
	spinlock_t list_lock;
	struct list_head cmd_list;	/* queue of in use SCSI Command structures */
	struct list_head starved_entry;
	struct scsi_cmnd *current_cmnd;	/* currently active command */
	unsigned short queue_depth;	/* How deep of a queue we want */
	unsigned short last_queue_full_depth; /* These two are used by */
	unsigned short last_queue_full_count; /* scsi_track_queue_full() */
	unsigned long last_queue_full_time;/* don't let QUEUE_FULLs on the same
					   jiffie count on our counter, they
					   could all be from the same event. */

	unsigned int id, lun, channel;

	unsigned int manufacturer;	/* Manufacturer of device, for using 
					 * vendor-specific cmd's */
	unsigned sector_size;	/* size in bytes */

	void *hostdata;		/* available to low-level driver */
	char type;
	char scsi_level;
	char inq_periph_qual;	/* PQ from INQUIRY data */	
	unsigned char inquiry_len;	/* valid bytes in 'inquiry' */
	unsigned char * inquiry;	/* INQUIRY response data */
	const char * vendor;		/* [back_compat] point into 'inquiry' ... */
	const char * model;		/* ... after scan; point to static string */
	const char * rev;		/* ... "nullnullnullnull" before scan */
	unsigned char current_tag;	/* current tag */
	struct scsi_target      *sdev_target;   /* vmklinux uses this cache info
   						 * just not used only for single_lun */

	unsigned int	sdev_bflags; /* black/white flags as also found in
				 * scsi_devinfo.[hc]. For now used only to
				 * pass settings from slave_alloc to scsi
				 * core. */
	unsigned writeable:1;
	unsigned removable:1;
	unsigned changed:1;	/* Data invalid due to media change */
	unsigned busy:1;	/* Used to prevent races */
	unsigned lockable:1;	/* Able to prevent media removal */
	unsigned locked:1;      /* Media removal disabled */
	unsigned borken:1;	/* Tell the Seagate driver to be 
				 * painfully slow on this device */
	unsigned disconnect:1;	/* can disconnect */
	unsigned soft_reset:1;	/* Uses soft reset option */
	unsigned sdtr:1;	/* Device supports SDTR messages */
	unsigned wdtr:1;	/* Device supports WDTR messages */
	unsigned ppr:1;		/* Device supports PPR messages */
	unsigned tagged_supported:1;	/* Supports SCSI-II tagged queuing */
	unsigned simple_tags:1;	/* simple queue tag messages are enabled */
	unsigned ordered_tags:1;/* ordered queue tag messages are enabled */
	unsigned single_lun:1;	/* Indicates we should only allow I/O to
				 * one of the luns for the device at a 
				 * time. */
	unsigned was_reset:1;	/* There was a bus reset on the bus for 
				 * this device */
	unsigned expecting_cc_ua:1; /* Expecting a CHECK_CONDITION/UNIT_ATTN
				     * because we did a bus reset. */
	unsigned use_10_for_rw:1; /* first try 10-byte read / write */
	unsigned use_10_for_ms:1; /* first try 10-byte mode sense/select */
	unsigned skip_ms_page_8:1;	/* do not use MODE SENSE page 0x08 */
	unsigned skip_ms_page_3f:1;	/* do not use MODE SENSE page 0x3f */
	unsigned use_192_bytes_for_3f:1; /* ask for 192 bytes from page 0x3f */
	unsigned no_start_on_add:1;	/* do not issue start on add */
	unsigned allow_restart:1; /* issue START_UNIT in error handler */
	unsigned no_uld_attach:1; /* disable connecting to upper level drivers */
	unsigned select_no_atn:1;
	unsigned fix_capacity:1;	/* READ_CAPACITY is too high by 1 */
	unsigned retry_hwerror:1;	/* Retry HARDWARE_ERROR */

	unsigned int device_blocked;	/* Device returned QUEUE_FULL. */

	unsigned int max_device_blocked; /* what device_blocked counts down from  */
#define SCSI_DEFAULT_DEVICE_BLOCKED	3

	atomic_t iorequest_cnt;
	atomic_t iodone_cnt;
	atomic_t ioerr_cnt;

	int timeout;

	struct device		sdev_gendev;
	struct class_device	sdev_classdev;

	struct execute_work	ew; /* used to get process context on put */

	enum scsi_device_state sdev_state;
#if defined(__VMKLNX__)

#define INQUIRY_RESULT_LENGTH 	256
    	char inquiryResult[INQUIRY_RESULT_LENGTH];
    	int dumpActive;        /* dumping to this device */

#define PSEUDO_DEVICE		0x00000001
#define DETACH_IN_PROGRESS	0x00000002
#define HAVE_CACHED_INQUIRY     0x00000004
#define	HOT_REMOVED		0x00000008 /* Device has been hot removed */
    	int vmkflags;          /* Protected by host_lock */
#endif

	unsigned long		sdev_data[0];
} __attribute__((aligned(sizeof(unsigned long))));

/**
 * to_scsi_device - Returns the scsi_device structure associated with the specified device.
 * @d: Pointer to the member sdev_gendev in struct scsi_device
 *
 * Returns the scsi_device structure associated with the specified device.
 * 
 * SYNOPSIS: 
 * #define to_scsi_device(d)
 *
 * RETURN VALUE:
 * Pointer to the parent scsi_device structure
 */
 /* _VMKLNX_CODECHECK_: to_scsi_device */
#define to_scsi_device(d)    \
        container_of(d, struct scsi_device, sdev_gendev)
#define	class_to_sdev(d)	\
	container_of(d, struct scsi_device, sdev_classdev)
#define transport_class_to_sdev(class_dev) \
	to_scsi_device(class_dev->dev)

#if defined(__VMKLNX__)
/**
 *  sdev_printk - a scsi device specific wrapper around printk
 *  @prefix: the prefix to be printed for the message
 *  @sdev: a pointer to struct scsi_device
 *  @fmt: the formatted string
 *  @a: the arguments to the formatted string
 *
 *  This macro, which is a wrapper around printk, is used for printing formatted
 *  messages to the log corresponding to @sdev.
 *
 *  ESX Deviation Notes:
 *  The priority for the messages might not be honored.
 *
 *  RETURN VALUE:
 *  Returns the number of characters written to the log.
 *
 */
/* _VMKLNX_CODECHECK_: sdev_printk */
#define sdev_printk(prefix, sdev, fmt, a...)	\
        printk(prefix "%s:%d:%d:%d:%d :: " fmt , sdev->host->hostt->name , sdev->host->host_no , sdev->channel, sdev->id, sdev->lun, ## a)

/**
 *  scmd_printk - a scsi command specific wrapper around printk
 *  @prefix: the prefix to be printed for the message
 *  @scmd: a pointer to struct scsi_cmnd
 *  @fmt: the formatted string
 *  @a: the arguments to the formatted string
 *
 *  This macro, which is a wrapper around printk, is used for printing formatted
 *  messages to the log corresponding to @scmd.
 *
 *  ESX Deviation Notes:
 *  The priority for the messages might not be honored.
 *
 *  RETURN VALUE:
 *  Returns the number of characters written to the log.
 *
 */
/* _VMKLNX_CODECHECK_: scmd_printk */
#define scmd_printk(prefix, scmd, fmt, a...)	\
        printk(prefix "%d :: " fmt , scmd->result, ## a)
#else
#define sdev_printk(prefix, sdev, fmt, a...)	\
	dev_printk(prefix, &(sdev)->sdev_gendev, fmt, ##a)
#define scmd_printk(prefix, scmd, fmt, a...)	\
	dev_printk(prefix, &(scmd)->device->sdev_gendev, fmt, ##a)
#endif

enum scsi_target_state {
	STARGET_RUNNING = 1,
	STARGET_DEL,
};

/*
 * scsi_target: representation of a scsi target, for now, this is only
 * used for single_lun devices. If no one has active IO to the target,
 * starget_sdev_user is NULL, else it points to the active sdev.
 */
struct scsi_target {
	struct scsi_device	*starget_sdev_user;
	struct list_head	siblings;
	struct list_head	devices;
	struct device		dev;
	unsigned int		reap_ref; /* protected by the host lock */
	unsigned int		channel;
	unsigned int		id; /* target id ... replace
				     * scsi_device.id eventually */
	unsigned int		create:1; /* signal that it needs to be added */
	unsigned int		pdt_1f_for_no_lun;	/* PDT = 0x1f */
						/* means no lun present */

	char			scsi_level;
	struct execute_work	ew;
	enum scsi_target_state	state;
	void 			*hostdata; /* available to low-level driver */
	unsigned long		starget_data[0]; /* for the transport */
	/* starget_data must be the last element!!!! */
} __attribute__((aligned(sizeof(unsigned long))));

/**
 * to_scsi_target - Convert pointer to member dev to its parent struct
 * @d: Pointer to struct device dev
 *
 * Convert the pointer to struct device dev to its parent struct scsi_target
 *
 * SYNOPSIS: 
 * #define to_scsi_target(d)
 *
 * RETURN VALUE:
 * Pointer to parent structure
 */
 /* _VMKLNX_CODECHECK_: to_scsi_target */
#define to_scsi_target(d) container_of(d, struct scsi_target, dev)

/**
 *  scsi_target - returns a pointer to the target for the scsi device
 *  @sdev: a pointer to struct scsi_device
 *
 *  Returns a pointer to the scsi target associated with @sdev.
 *
 *  RETURN VALUE:
 *  Returns a pointer to struct scsi_target corresponding to @sdev.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_target */
static inline struct scsi_target *scsi_target(struct scsi_device *sdev)
{
	return to_scsi_target(sdev->sdev_gendev.parent);
}
#define transport_class_to_starget(class_dev) \
	to_scsi_target(class_dev->dev)

#if defined(__VMKLNX__)
/**                                          
 *  starget_printk - Print a message regarding a specific SCSI target
 *  @prefix: Log-level prefix for the message
 *  @starget: SCSI target for the message
 *  @fmt: printf-style format string for output data message     
 *  @a: variable arguments to the format string, if any
 *  
 *  Prints a message to the kernel system log for a specific SCSI target.  
 *  Like printk, starget_printk takes an optional printk-style log-level
 *  prefix.  starget_printk prepends the provided debug string and arguments
 *  with the SCSI target's channel and ID information.
 *
 *  SYNOPSIS:
 *     #define starget_printk(prefix, starget, fmt, a...)
 *
 *  RETURN VALUE:
 *  The number of characters written to the kernel log, not including the
 *  terminating NULL of the string
 *
 *  SEE ALSO:
 *  printk
 *
 *  ESX Deviation Notes:
 *  starget_printk on ESX prepends the SCSI channel and ID information, whereas
 *  the Linux version prints driver-name and bus-id information.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: starget_printk */
#define starget_printk(prefix, starget, fmt, a...)	\
        printk(prefix "Channel :%d Id :%d :: " fmt , starget->channel, starget->id, ## a)
#else
#define starget_printk(prefix, starget, fmt, a...)	\
	dev_printk(prefix, &(starget)->dev, fmt, ##a)
#endif

extern struct scsi_device *__scsi_add_device(struct Scsi_Host *,
		uint, uint, uint, void *hostdata);
extern int scsi_add_device(struct Scsi_Host *host, uint channel,
			   uint target, uint lun);
extern void scsi_remove_device(struct scsi_device *);
extern int scsi_device_cancel(struct scsi_device *, int);

extern int scsi_device_get(struct scsi_device *);
extern void scsi_device_put(struct scsi_device *);
extern struct scsi_device *scsi_device_lookup(struct Scsi_Host *,
					      uint, uint, uint);
extern struct scsi_device *__scsi_device_lookup(struct Scsi_Host *,
						uint, uint, uint);
extern struct scsi_device *scsi_device_lookup_by_target(struct scsi_target *,
							uint);
extern struct scsi_device *__scsi_device_lookup_by_target(struct scsi_target *,
							  uint);
extern void starget_for_each_device(struct scsi_target *, void *,
		     void (*fn)(struct scsi_device *, void *));

/* only exposed to implement shost_for_each_device */
extern struct scsi_device *__scsi_iterate_devices(struct Scsi_Host *,
						  struct scsi_device *);

/**
 * shost_for_each_device  -  iterate over all devices of a host
 * @sdev: the &struct scsi_device to use as a iterator
 * @shost: the &struct Scsi_Host to iterate over
 *
 * Iterator that returns each device attached to @shost.  This loop
 * takes a reference on each device and releases it at the end.  If
 * you break out of the loop, you must call scsi_device_put.
 *
 * SYNOPSIS:
 *    #define shost_for_each_device(sdev, shost)
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: shost_for_each_device */
#define shost_for_each_device(sdev, shost) \
	for ((sdev) = __scsi_iterate_devices((shost), NULL); \
	     (sdev); \
	     (sdev) = __scsi_iterate_devices((shost), (sdev)))

/**
 * __shost_for_each_device  -  iterate over all devices of a host (UNLOCKED)
 * @sdev:	iterator
 * @shost:	host whose devices we want to iterate over
 *
 * This traverses over each devices of @shost.  It does _not_ take a
 * reference on the scsi_device, thus it the whole loop must be protected
 * by shost->host_lock.
 *
 * Note:  The only reason why drivers would want to use this is because
 * they're need to access the device list in irq context.  Otherwise you
 * really want to use shost_for_each_device instead.
 *
 * SYNOPSIS:
 *    #define __shost_for_each_device(sdev,shost)
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: __shost_for_each_device */
#define __shost_for_each_device(sdev, shost) \
	list_for_each_entry((sdev), &((shost)->__devices), siblings)

extern void scsi_adjust_queue_depth(struct scsi_device *, int, int);
extern int scsi_track_queue_full(struct scsi_device *, int);

extern int scsi_set_medium_removal(struct scsi_device *, char);

extern int scsi_mode_sense(struct scsi_device *sdev, int dbd, int modepage,
			   unsigned char *buffer, int len, int timeout,
			   int retries, struct scsi_mode_data *data,
			   struct scsi_sense_hdr *);
extern int scsi_mode_select(struct scsi_device *sdev, int pf, int sp,
			    int modepage, unsigned char *buffer, int len,
			    int timeout, int retries,
			    struct scsi_mode_data *data,
			    struct scsi_sense_hdr *);
extern int scsi_test_unit_ready(struct scsi_device *sdev, int timeout,
				int retries);
extern int scsi_device_set_state(struct scsi_device *sdev,
				 enum scsi_device_state state);
extern int scsi_device_quiesce(struct scsi_device *sdev);
extern void scsi_device_resume(struct scsi_device *sdev);
extern void scsi_target_quiesce(struct scsi_target *);
extern void scsi_target_resume(struct scsi_target *);
extern void scsi_scan_target(struct device *parent, unsigned int channel,
			     unsigned int id, unsigned int lun, int rescan);
extern void scsi_target_reap(struct scsi_target *);
extern void scsi_target_block(struct device *);
extern void scsi_target_unblock(struct device *);
extern void scsi_remove_target(struct device *);
extern void int_to_scsilun(unsigned int, struct scsi_lun *);
extern const char *scsi_device_state_name(enum scsi_device_state);
extern int scsi_is_sdev_device(const struct device *);
extern int scsi_is_target_device(const struct device *);
extern int scsi_execute(struct scsi_device *sdev, const unsigned char *cmd,
			int data_direction, void *buffer, unsigned bufflen,
			unsigned char *sense, int timeout, int retries,
			int flag);
extern int scsi_execute_req(struct scsi_device *sdev, const unsigned char *cmd,
			    int data_direction, void *buffer, unsigned bufflen,
			    struct scsi_sense_hdr *, int timeout, int retries);
extern int scsi_execute_async(struct scsi_device *sdev,
			      const unsigned char *cmd, int cmd_len, int data_direction,
			      void *buffer, unsigned bufflen, int use_sg,
			      int timeout, int retries, void *privdata,
			      void (*done)(void *, char *, int, int),
			      gfp_t gfp);

#if defined(__VMKLNX__)
extern void scsi_device_reprobe(struct scsi_device *sdev);
#else /* !defined(__VMKLNX__) */
static inline void scsi_device_reprobe(struct scsi_device *sdev)
{
	device_reprobe(&sdev->sdev_gendev);
}
#endif /* defined(__VMKLNX__) */

/**                                          
 *  sdev_channel - Returns scsi device channel
 *  @sdev: pointer to struct scsi_device 
 *                                           
 *  Returns scsi device channel 
 *                                           
 *  RETURN VALUE:
 *  scsi device channel number
 *
 */                                          
/* _VMKLNX_CODECHECK_: sdev_channel */
static inline unsigned int sdev_channel(struct scsi_device *sdev)
{
	return sdev->channel;
}

/**                                          
 *  sdev_id - Returns scsi device id
 *  @sdev: pointer to struct scsi_device 
 *                                           
 *  Returns scsi device id 
 *                                           
 *  RETURN VALUE:
 *  scsi device ID
 *
 */                                          
/* _VMKLNX_CODECHECK_: sdev_id */
static inline unsigned int sdev_id(struct scsi_device *sdev)
{
	return sdev->id;
}

/**
 *  scmd_id - returns the ID for the device associated with the scsi command
 *  @scmd: a pointer to struct scsi_cmnd 
 *
 *  Given a pointer to struct scsi_cmnd, this macro returns its corresponding
 *  id.
 *
 *  RETURN VALUE:
 *  Returns the id.
 *
 */
/* _VMKLNX_CODECHECK_: scmd_id */
#define scmd_id(scmd) sdev_id((scmd)->device)

/**
 *  scmd_channel - returns the channel for the device associated with the scsi command
 *  @scmd: a pointer to struct scsi_cmnd 
 *
 *  Given a pointer to struct scsi_cmnd, this macro returns its corresponding
 *  channel.
 *
 *  RETURN VALUE:
 *  Returns the channel.
 *
 */
/* _VMKLNX_CODECHECK_: scmd_channel */
#define scmd_channel(scmd) sdev_channel((scmd)->device)

/**                                          
 *  scsi_device_online - check if scsi device is online       
 *  @sdev: scsi device to be checked    
 *                                           
 *  Check if scsi device is NOT offline.
 *
 *  RETURN VALUE: TRUE if device is not offline, otherwise FALSE
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_online */
static inline int scsi_device_online(struct scsi_device *sdev)
{
	return sdev->sdev_state != SDEV_OFFLINE;
}

/* accessor functions for the SCSI parameters */
/**                                          
 *  scsi_device_sync - Check whether scsi device supports synchronous data transfer.       
 *  @sdev: scsi device    
 *                                           
 *  Check whether scsi device supports synchronous data transfer.                       
 *                                           
 *  RETURN VALUE:
 *	Non-zero if device supports synchronous data transfer, 0 if not
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_sync */
static inline int scsi_device_sync(struct scsi_device *sdev)
{
	return sdev->sdtr;
}
/**                                          
 *  scsi_device_wide - Check whether scsi device supports 16-bit wide data transfers.       
 *  @sdev: scsi device   
 *                                           
 *   Check whether scsi device supports 16-bit wide data transfers.                                         
 *                                           
 *   RETURN VALUE:
 *     Non-zero if device supports 16-bit wide data transfers, 0 if not
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_wide */
static inline int scsi_device_wide(struct scsi_device *sdev)
{
	return sdev->wdtr;
}

/**
 *  scsi_device_dt - Check whether device supports PPR messages
 *  @sdev: the device
 *
 *  Check whether device supports PPR messages
 *
 *  RETURN VALUE:
 *  1 if device supports PPR messages, 0 otherwise
 */
/* _VMKLNX_CODECHECK_: scsi_device_dt */
static inline int scsi_device_dt(struct scsi_device *sdev)
{
	return sdev->ppr;
}
static inline int scsi_device_dt_only(struct scsi_device *sdev)
{
	if (sdev->inquiry_len < 57)
		return 0;
	return (sdev->inquiry[56] & 0x0c) == 0x04;
}

/**
 *  scsi_device_ius - Check whether device supports Information Units
 *  @sdev: the device
 *
 *  Check whether device supports Information Units
 *
 *  RETURN VALUE:
 *  1 if device supports IUs, 0 if does not
 */
/* _VMKLNX_CODECHECK_: scsi_device_ius */
static inline int scsi_device_ius(struct scsi_device *sdev)
{
	if (sdev->inquiry_len < 57)
		return 0;
	return sdev->inquiry[56] & 0x01;
}
/**                                          
 *  scsi_device_qas - Check whether scsi device supports quick arbitration and selection (QAS)      
 *  @sdev: scsi device    
 *                                           
 *  Check whether scsi device supports quick arbitration and selection (QAS)
 *                                           
 *  RETURN VALUE:
 *   Non-zero if device supports QAS, 0 if not
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_qas */
static inline int scsi_device_qas(struct scsi_device *sdev)
{
	if (sdev->inquiry_len < 57)
		return 0;
	return sdev->inquiry[56] & 0x02;
}
#endif /* _SCSI_SCSI_DEVICE_H */
