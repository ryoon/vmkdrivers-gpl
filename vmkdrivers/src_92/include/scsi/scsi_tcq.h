/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef _SCSI_SCSI_TCQ_H
#define _SCSI_SCSI_TCQ_H

#include <linux/blkdev.h>
#if defined(__VMKLNX__)
#include <scsi/scsi.h>        // ORDERED_QUEUE_TAG
#endif
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>


#define MSG_SIMPLE_TAG	0x20
#define MSG_HEAD_TAG	0x21
#define MSG_ORDERED_TAG	0x22

#define SCSI_NO_TAG	(-1)    /* identify no tag in use */



/**
 * scsi_get_tag_type - get the type of tag the device supports
 * @sdev:	the scsi device
 *
 *	Get the type of tag the device supports
 *
 * 	RETURN VALUE:
 *	If the drive only supports simple tags, returns MSG_SIMPLE_TAG.
 *	If it supports ordered tag type, returns MSG_ORDERED_TAG,
 */
/* _VMKLNX_CODECHECK_: scsi_get_tag_type */
static inline int scsi_get_tag_type(struct scsi_device *sdev)
{
	if (!sdev->tagged_supported)
		return 0;
	if (sdev->ordered_tags)
		return MSG_ORDERED_TAG;
	if (sdev->simple_tags)
		return MSG_SIMPLE_TAG;
	return 0;
}

/**                                          
 *  scsi_set_tag_type - sets the tag type the device supports       
 *  @sdev: the scsi device    
 *  @tag: tag type   
 *                                           
 *  Sets tag type the device supports 
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_set_tag_type */
static inline void scsi_set_tag_type(struct scsi_device *sdev, int tag)
{
	switch (tag) {
        case MSG_HEAD_TAG:
                /* fall through */
	case MSG_ORDERED_TAG:
		sdev->ordered_tags = 1;
		/* fall through */
	case MSG_SIMPLE_TAG:
		sdev->simple_tags = 1;
		break;
	case 0:
		/* fall through */
	default:
		sdev->ordered_tags = 0;
		sdev->simple_tags = 0;
		break;
	}
}

/**
 * scsi_activate_tcq - turn on tag command queueing
 * @sdev:	device to turn on TCQ for
 * @depth:	queue depth
 *
 *  Turns on tag command queueing
 *
 *  ESX Deviation Notes:
 *  blk layer is not supported/affected.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: scsi_activate_tcq */
static inline void scsi_activate_tcq(struct scsi_device *sdev, int depth)
{
	if (!sdev->tagged_supported)
		return;

#if !defined(__VMKLNX__)
        /*
         * VMKLINUX SCSI does not deal with blk
         */
	if (!blk_queue_tagged(sdev->request_queue))
		blk_queue_init_tags(sdev->request_queue, depth,
				    sdev->host->bqt);
#endif

	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), depth);
}

/**
 * scsi_deactivate_tcq - turn off tag command queueing
 * @sdev:  SCSI Device to turn off TCQ for
 * @depth: number of commands the low level driver can queue up in non-tagged mode.
 *
 *  RETURN VALUE:
 *  None
 *
 *  ESX Deviation Notes:
 *  blk layer is not supported/affected.
 */
/* _VMKLNX_CODECHECK_: scsi_deactivate_tcq*/
static inline void scsi_deactivate_tcq(struct scsi_device *sdev, int depth)
{
#if !defined(__VMKLNX__)
	if (blk_queue_tagged(sdev->request_queue))
		blk_queue_free_tags(sdev->request_queue);
#endif
	scsi_adjust_queue_depth(sdev, 0, depth);
}

/**
 * scsi_populate_tag_msg - place a tag message in a buffer
 * @cmd:	pointer to the Scsi_Cmnd for the tag
 * @msg:	pointer to the area to place the tag
 *
 *	Create the correct type of tag message for the 
 *	particular request.  
 *
 * 	ESX Deviation Notes:
 * 	 No drivers use the second field, which is a tag value that
 * 	 is guaranteed to be unique for all commands outstanding on
 * 	 this device. All drivers that support tagging deal with
 * 	 this in hardware and do not use this value, so just leave
 * 	 it as 0.
 *
 *	RETURN VALUE:
 *	Returns the size of the tag message.
 *	May return 0 if TCQ is disabled for this device.
 **/
/* _VMKLNX_CODECHECK_: scsi_populate_tag_msg */
static inline int scsi_populate_tag_msg(struct scsi_cmnd *cmd, char *msg)
{
#if !defined(__VMKLNX__)
        /*
         * VMKLINUX does not deal with blk.
         */
        struct request *req = cmd->request;
	struct scsi_device *sdev = cmd->device;

        if (blk_rq_tagged(req)) {
		if (sdev->ordered_tags && req->flags & REQ_HARDBARRIER)
        	        *msg++ = MSG_ORDERED_TAG;
        	else
        	        *msg++ = MSG_SIMPLE_TAG;
        	*msg++ = req->tag;
        	return 2;
	}

	return 0;
#else
        /*
         * cmd->tag was set in SCSILinuxQueueCommand...
         */
        if (cmd->tag == ORDERED_QUEUE_TAG) { 
                *msg++ = MSG_ORDERED_TAG;
        } else if (cmd->tag == HEAD_OF_QUEUE_TAG) {  
                *msg++ = MSG_HEAD_TAG;
        } else {
                *msg++ = MSG_SIMPLE_TAG;
        }
        /*
         * No drivers use the second field, which is a tag value that
         * is guaranteed to be unique for all commands outstanding on
         * this device. All drivers that support tagging deal with
         * this in hardware and do not use this value, so just leave
         * it 0.
         */
        *msg++ = 0;
        return 2;
#endif
}

/*
 * scsi_find_tag - find a tagged command by device
 * @sdev:	pointer to the SCSI device
 * @tag:	the tag number
 *
 * Notes:
 *	Only works with tags allocated by the generic blk layer.
 */
static inline struct scsi_cmnd *scsi_find_tag(struct scsi_device *sdev, int tag)
{

        struct request *req;

        if (tag != SCSI_NO_TAG) {
        	req = blk_queue_find_tag(sdev->request_queue, tag);
	        return req ? (struct scsi_cmnd *)req->special : NULL;
	}

	/* single command, look in space */
	return sdev->current_cmnd;
}

/*
 * scsi_init_shared_tag_map - create a shared tag map
 * @shost:	the host to share the tag map among all devices
 * @depth:	the total depth of the map
 */
static inline int scsi_init_shared_tag_map(struct Scsi_Host *shost, int depth)
{
	shost->bqt = blk_init_tags(depth);
	return shost->bqt ? 0 : -ENOMEM;
}

#endif /* _SCSI_SCSI_TCQ_H */
