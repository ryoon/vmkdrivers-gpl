/*
 * Portions Copyright 2008, 2010 VMware, Inc.
 */
#ifndef _SCSI_SCSI_CMND_H
#define _SCSI_SCSI_CMND_H

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/timer.h>

struct request;
struct scatterlist;
struct scsi_device;


/* embedded in scsi_cmnd */
struct scsi_pointer {
	char *ptr;		/* data pointer */
	int this_residual;	/* left in this buffer */
	struct scatterlist *buffer;	/* which buffer */
	int buffers_residual;	/* how many buffers left */

        dma_addr_t dma_handle;

	volatile int Status;
	volatile int Message;
	volatile int have_data_in;
	volatile int sent_command;
	volatile int phase;
};

struct scsi_cmnd {
	struct scsi_device *device;
	struct list_head list;  /* scsi_cmnd participates in queue lists */
	struct list_head eh_entry; /* entry for the host eh_cmd_q */
	int eh_eflags;		/* Used by error handlr */
	void (*done) (struct scsi_cmnd *);	/* Mid-level done function */

	/*
	 * A SCSI Command is assigned a nonzero serial_number before passed
	 * to the driver's queue command function.  The serial_number is
	 * cleared when scsi_done is entered indicating that the command
	 * has been completed.  It currently doesn't have much use other
	 * than printk's.  Some lldd's use this number for other purposes.
	 * It's almost certain that such usages are either incorrect or
	 * meaningless.  Please kill all usages other than printk's.  Also,
	 * as this number is always identical to ->pid, please convert
	 * printk's to use ->pid, so that we can kill this field.
	 */
	unsigned long serial_number;
	/*
	 * This is set to jiffies as it was when the command was first
	 * allocated.  It is used to time how long the command has
	 * been outstanding
	 */
	unsigned long jiffies_at_alloc;

	int retries;
	int allowed;
	int timeout_per_command;

	unsigned char cmd_len;
	enum dma_data_direction sc_data_direction;

	/* These elements define the operation we are about to perform */
#define MAX_COMMAND_SIZE	16
	unsigned char cmnd[MAX_COMMAND_SIZE];
	unsigned request_bufflen;	/* Actual request size */

	struct timer_list eh_timeout;	/* Used to time out the command. */
	void *request_buffer;		/* Actual requested buffer */

	/* These elements define the operation we ultimately want to perform */
	unsigned short use_sg;	/* Number of pieces of scatter-gather */
	unsigned short sglist_len;	/* size of malloc'd scatter-gather list */

	unsigned underflow;	/* Return error if less than
				   this amount is transferred */

	unsigned transfersize;	/* How much we are guaranteed to
				   transfer with each SCSI transfer
				   (ie, between disconnect / 
				   reconnects.   Probably == sector
				   size */

	int resid;		/* Number of bytes requested to be
				   transferred less actual number
				   transferred (0 if not supported) */

	struct request *request;	/* The command we are
				   	   working on */

#define SCSI_SENSE_BUFFERSIZE 	96
	unsigned char sense_buffer[SCSI_SENSE_BUFFERSIZE];
				/* obtained by REQUEST SENSE when
				 * CHECK CONDITION is received on original
				 * command (auto-sense) */

	/* Low-level done function - can be used by low-level driver to point
	 *        to completion function.  Not used by mid/upper level code. */
	void (*scsi_done) (struct scsi_cmnd *);

	/*
	 * The following fields can be written to by the host specific code. 
	 * Everything else should be left alone. 
	 */
	struct scsi_pointer SCp;	/* Scratchpad used by some host adapters */

	unsigned char *host_scribble;	/* The host adapter is allowed to
					 * call scsi_malloc and get some memory
					 * and hang it here.  The host adapter
					 * is also expected to call scsi_free
					 * to release this memory.  (The memory
					 * obtained by scsi_malloc is guaranteed
					 * to be at an address < 16Mb). */

	int result;		/* Status code from lower level driver */

	unsigned char tag;	/* SCSI-II queued command tag */
	unsigned long pid;	/* Process ID, starts at 0. Unique per host. */
#if defined(__VMKLNX__)
	struct list_head bhlist;  /* scsi_cmnd for vmklnx bh processing */
        spinlock_t vmklock;
   	int vmkflags;                  /* vmware flags protected by vmk_lock */
   	struct vmk_ScsiCommand	*vmkCmdPtr;

   	/*
         * When not using an SG list, request_buffer contains the address for
    	 * IO region, however that field only has 32bits, which can't handle
         * memory > 4GB, so we have new fields that contain the machine
         * address.
         */
   	unsigned long request_bufferMA;

        /* For drivers that support vmkSgArray, 1 struct scatterlist element is
         * required to hold vmksg list (i.e vmkSgElem's). Instead of dynamically
         * allocating that using scsi_alloc_sgtable(), we equip scsi_cmnd with
         * one to save cpu cycles. This will also avoid free'ing that element.
         * sgArray ptr below points to the address of vmksg (see linux_scsi.c) 
         * and no one else should touch this member.
         */
        struct scatterlist	vmksg[1];
#endif
};

/*
 * These are the values that scsi_cmd->state can take.
 */
#define SCSI_STATE_TIMEOUT         0x1000
#define SCSI_STATE_FINISHED        0x1001
#define SCSI_STATE_FAILED          0x1002
#define SCSI_STATE_QUEUED          0x1003
#define SCSI_STATE_UNUSED          0x1006
#define SCSI_STATE_DISCONNECTING   0x1008
#define SCSI_STATE_INITIALIZING    0x1009
#define SCSI_STATE_BHQUEUE         0x100a
#define SCSI_STATE_MLQUEUE         0x100b


extern struct scsi_cmnd *scsi_get_command(struct scsi_device *, gfp_t);
extern void scsi_put_command(struct scsi_cmnd *);
extern void scsi_io_completion(struct scsi_cmnd *, unsigned int);
extern void scsi_finish_command(struct scsi_cmnd *cmd);
extern void scsi_req_abort_cmd(struct scsi_cmnd *cmd);

extern void *scsi_kmap_atomic_sg(struct scatterlist *sg, int sg_count,
				 size_t *offset, size_t *len);
extern void scsi_kunmap_atomic_sg(void *virt);

/**
 *  scsi_sg_count - returns the number of scatter-gather pieces
 *  @cmd: a pointer to struct scsi_cmnd
 *
 *  Returns the number of scatter-gather pieces associated with @cmd
 *
 *  RETURN VALUE:
 *  Returns the number of scatter-gather pieces associated with @cmd
 *
 */
/* _VMKLNX_CODECHECK_: scsi_sg_count */
#define scsi_sg_count(cmd) ((cmd)->use_sg)

/**
 *  scsi_sglist - returns the request buffer
 *  @cmd: a pointer to struct scsi_cmnd
 *
 *  Returns the request buffer for @cmd.
 *
 *  RETURN VALUE:
 *  Returns a pointer to request buffer for @cmd masked as a pointer to struct
 *  scatterlist.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_sglist */
#define scsi_sglist(cmd) ((struct scatterlist *)(cmd)->request_buffer)

/**
 *  scsi_bufflen - returns the size of the request
 *  @cmd: a pointer to struct scsi_cmnd
 *
 *  Given a scsi command, this macro returns the size of the request.
 *
 *  RETURN VALUE:
 *  Returns the size of the request.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_bufflen */
#define scsi_bufflen(cmd) ((cmd)->request_bufflen)

/**
 *  scsi_set_resid - set the resid
 *  @cmd: a pointer the struct scsi_cmnd
 *  @resid: the resid
 *
 *  Sets the resid for the scsi command.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_set_resid */
static inline void scsi_set_resid(struct scsi_cmnd *cmd, int resid)
{
    cmd->resid = resid;
}

#if defined(__VMKLNX__)
/**
 *  scsi_get_resid - get the resid
 *  @cmd: a pointer the struct scsi_cmnd
 *
 *  Returns the residual for the scsi command.
 *
 *  RETURN VALUE:
 *  residual for the scsi command
 *
 */
/* _VMKLNX_CODECHECK_: scsi_get_resid */
static inline int scsi_get_resid(struct scsi_cmnd *cmd)
{
	return cmd->resid;
}

#include <linux/scatterlist.h>

/**
 *  scsi_for_each_sg - Loop over scsi_cmd's SG list
 *  @cmd: a pointer to struct scsi_cmnd
 *  @nseg: number of elements in the list
 *  @sg: scatter-gather list
 *  @__i: index of sg element
 *
 *  Loop over @cmd scsi_cmd's SG list.
 *
 *  RETURN VALUE:
 *  None.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_for_each_sg */
#define scsi_for_each_sg(cmd, sg, nseg, __i)			\
	for_each_sg(scsi_sglist(cmd), sg, nseg, __i)

/**
 *  scsi_sg_memset - Memset scsi_cmnd's buffer/SG list with constant byte
 *  @cmd: pointer to the struct scsi_cmnd
 *  @c: byte to set
 *  @count: number of bytes to set
 *
 *  Fills the first @count bytes of @cmd scsi_cmnd's request_buffer or SG list with
 *  constant byte @c.
 *
 *  RETURN VALUE:
 *  None.
 *
 */
static inline void scsi_sg_memset(struct scsi_cmnd *cmd, int c, size_t count)
{
	size_t len = min_t(size_t, scsi_bufflen(cmd), count);
	if (cmd->use_sg) {
		sg_memset(scsi_sglist(cmd), scsi_sg_count(cmd), c, len);
	} else {
		memset(cmd->request_buffer, c, len);
	}
}

/**
 *  scsi_sg_copy_from_buffer - Copy data from a buffer to scsi_cmnd's buffer/SG list
 *  @cmd: pointer to the struct scsi_cmnd
 *  @buf: buffer to copy from
 *  @buflen: transfer length
 *
 *  Copies data from a buffer to @cmd scsi_cmnd's request_buffer or SG list if @cmd
 *  uses scatter-gather list.
 *
 *  RETURN VALUE:
 *  Returns the number of bytes copied
 *
 */
/* _VMKLNX_CODECHECK_: scsi_sg_copy_from_buffer */
static inline int scsi_sg_copy_from_buffer(struct scsi_cmnd *cmd,
					   void *buf, int buflen)
{
	size_t transfer_len = min_t(size_t, scsi_bufflen(cmd), buflen);
	if (cmd->use_sg) {
		return sg_copy_from_buffer(scsi_sglist(cmd),
					   scsi_sg_count(cmd),
					   buf, transfer_len);
	} else {
		memcpy(cmd->request_buffer, buf, transfer_len);
		return transfer_len;
	}
}

/**
 *  scsi_sg_copy_to_buffer - Copy data to a buffer from scsi_cmnd's buffer/SG list
 *  @cmd: pointer to the struct scsi_cmnd
 *  @buf: buffer to copy
 *  @buflen: transfer length
 *
 *  Copies data to a buffer from @cmd scsi_cmnd's request_buffer or SG list if @cmd
 *  uses scatter-gather list.
 *
 *  RETURN VALUE:
 *  Returns the number of bytes copied
 *
 */
/* _VMKLNX_CODECHECK_: scsi_sg_copy_to_buffer */
static inline int scsi_sg_copy_to_buffer(struct scsi_cmnd *cmd,
					 void *buf, int buflen)
{
	size_t transfer_len = min_t(size_t, scsi_bufflen(cmd), buflen);
	if (cmd->use_sg) {
		return sg_copy_to_buffer(scsi_sglist(cmd),
					 scsi_sg_count(cmd),
					 buf, transfer_len);
	} else {
		memcpy(buf, cmd->request_buffer, transfer_len);
		return transfer_len;
	}
}

#endif /* defined(__VMKLNX__) */

#endif /* _SCSI_SCSI_CMND_H */
