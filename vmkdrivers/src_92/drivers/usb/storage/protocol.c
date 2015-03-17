/*
 * Portions Copyright 2008, 2010 VMware, Inc.
 */
/* Driver for USB Mass Storage compliant devices
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2002 Alan Stern (stern@rowland.org)
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/highmem.h>
#include <linux/module.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#if defined(__VMKLNX__)
#include <scsi/scsi_device.h>
#endif

#include "usb.h"
#include "protocol.h"
#include "debug.h"
#include "scsiglue.h"
#include "transport.h"

#if defined(__VMKLNX__)
extern int usb_stor_schedule_unclaim(struct us_data *us);

void fix_inquiry_data(struct scsi_cmnd *srb, struct us_data *us)
{
	unsigned char buf[6];
	const char *name = USB_STOR_DEVICE_NAME(srb);

	if (likely(srb->cmnd[0] != INQUIRY))
		return;

	scsi_sg_copy_to_buffer(srb, buf, sizeof(buf));

	printk(USB_STORAGE "detected SCSI revision number %d on %s\n",
		buf[2] & 7, name);
        if ((buf[2] & 7) != 0x2) {
		/* patch SCSI revision number */
		printk(USB_STORAGE "patching inquiry data to change SCSI "
			"revision number from %d to %d on %s\n", 
			buf[2] & 7, 0x2, name);
		buf[2] = (buf[2] & ~7) | 0x2;
 	}

	if (!(buf[1] & 0x80)) {
		/* force removable media bit on */
		printk(USB_STORAGE "patching inquiry data to change removable "
			"media bit from 'off' to 'on' on %s\n", name);
		buf[1] = buf[1] | 0x80;
	}

	if (buf[5] & 0x30) {
		/* clear TPGS */
		printk(USB_STORAGE "patching inquiry data to clear TPGS (%d) "
			"on %s\n", (buf[5] & 0x30) >> 4, name);
		buf[5] &= 0xcf;
 	}

	us->devtype = buf[0] & 0x1f;
	if (us->unclaim_rq) {
		if (us->devtype == 0x05) { 
			/* keep CD-ROM */
			us->unclaim_rq = 0;
		} else {
			printk(USB_STORAGE "setting peripheral qualifier to "
				"'not connected' on %s\n", name);
			buf[0] |= 0x20;
			usb_stor_schedule_unclaim(us);
		}
	}

	scsi_sg_copy_from_buffer(srb, buf, sizeof(buf));
}
#endif

/***********************************************************************
 * Protocol routines
 ***********************************************************************/

void usb_stor_pad12_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/*
	 * Pad the SCSI command with zeros out to 12 bytes.  If the
	 * command already is 12 bytes or longer, leave it alone.
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */
	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
#if defined(__VMKLNX__)
	if (srb->result == SAM_STAT_GOOD) {
		fix_inquiry_data(srb, us);
	}
#endif
}

void usb_stor_ufi_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/* fix some commands -- this is a form of mode translation
	 * UFI devices only accept 12 byte long commands 
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */

	/* Pad the ATAPI command with zeros */
	for (; srb->cmd_len<12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes (this affects the transport layer) */
	srb->cmd_len = 12;

	/* XXX We should be constantly re-evaluating the need for these */

	/* determine the correct data length for these commands */
	switch (srb->cmnd[0]) {

		/* for INQUIRY, UFI devices only ever return 36 bytes */
	case INQUIRY:
		srb->cmnd[4] = 36;
		break;

		/* again, for MODE_SENSE_10, we get the minimum (8) */
	case MODE_SENSE_10:
		srb->cmnd[7] = 0;
		srb->cmnd[8] = 8;
		break;

		/* for REQUEST_SENSE, UFI devices only ever return 18 bytes */
	case REQUEST_SENSE:
		srb->cmnd[4] = 18;
		break;
	} /* end switch on cmnd[0] */

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
#if defined(__VMKLNX__)
	if (srb->result == SAM_STAT_GOOD) {
		fix_inquiry_data(srb, us);
	}
#endif
}

void usb_stor_transparent_scsi_command(struct scsi_cmnd *srb,
				       struct us_data *us)
{
	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
#if defined(__VMKLNX__)
	if (srb->result == SAM_STAT_GOOD) {
		fix_inquiry_data(srb, us);
	}
#endif
}
EXPORT_SYMBOL_GPL(usb_stor_transparent_scsi_command);

#if !defined(__VMKLNX__)
/***********************************************************************
 * Scatter-gather transfer buffer access routines
 ***********************************************************************/

/* Copy a buffer of length buflen to/from the srb's transfer buffer.
 * Update the **sgptr and *offset variables so that the next copy will
 * pick up from where this one left off.
 */
unsigned int usb_stor_access_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb, struct scatterlist **sgptr,
	unsigned int *offset, enum xfer_buf_dir dir)
{
	unsigned int cnt;
	struct scatterlist *sg = *sgptr;

	/* We have to go through the list one entry
	 * at a time.  Each s-g entry contains some number of pages, and
	 * each page has to be kmap()'ed separately.  If the page is already
	 * in kernel-addressable memory then kmap() will return its address.
	 * If the page is not directly accessible -- such as a user buffer
	 * located in high memory -- then kmap() will map it to a temporary
	 * position in the kernel's virtual address space.
	 */

	if (!sg)
		sg = scsi_sglist(srb);

	/* This loop handles a single s-g list entry, which may
	 * include multiple pages.  Find the initial page structure
	 * and the starting offset within the page, and update
	 * the *offset and **sgptr values for the next loop.
	 */
	cnt = 0;
	while (cnt < buflen && sg) {
		struct page *page = sg_page(sg) +
				((sg->offset + *offset) >> PAGE_SHIFT);
		unsigned int poff = (sg->offset + *offset) & (PAGE_SIZE-1);
		unsigned int sglen = sg->length - *offset;

		if (sglen > buflen - cnt) {

			/* Transfer ends within this s-g entry */
			sglen = buflen - cnt;
			*offset += sglen;
		} else {

			/* Transfer continues to next s-g entry */
			*offset = 0;
			sg = sg_next(sg);
		}

		/* Transfer the data for all the pages in this
			* s-g entry.  For each page: call kmap(), do the
			* transfer, and call kunmap() immediately after. */
		while (sglen > 0) {
			unsigned int plen = min(sglen, (unsigned int)
					PAGE_SIZE - poff);
			unsigned char *ptr = kmap(page);

			if (dir == TO_XFER_BUF)
				memcpy(ptr + poff, buffer + cnt, plen);
			else
				memcpy(buffer + cnt, ptr + poff, plen);
			kunmap(page);

			/* Start at the beginning of the next page */
			poff = 0;
			++page;
			cnt += plen;
			sglen -= plen;
		}
	}
	*sgptr = sg;

	/* Return the amount actually transferred */
	return cnt;
}
EXPORT_SYMBOL_GPL(usb_stor_access_xfer_buf);
#endif

/* Store the contents of buffer into srb's transfer buffer and set the
 * SCSI residue.
 */
void usb_stor_set_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb)
{
#if !defined(__VMKLNX__)
	unsigned int offset = 0;
	struct scatterlist *sg = NULL;
#endif

	buflen = min(buflen, scsi_bufflen(srb));

#if defined(__VMKLNX__)
	scsi_sg_copy_from_buffer(srb, buffer, buflen);
#else
	buflen = usb_stor_access_xfer_buf(buffer, buflen, srb, &sg, &offset,
			TO_XFER_BUF);
#endif
	if (buflen < scsi_bufflen(srb))
		scsi_set_resid(srb, scsi_bufflen(srb) - buflen);
}
EXPORT_SYMBOL_GPL(usb_stor_set_xfer_buf);
