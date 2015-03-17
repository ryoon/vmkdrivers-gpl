/*
 * Portions Copyright 2008, 2010 VMware, Inc.
 */
/* Driver for USB Mass Storage compliant devices
 * Debugging Functions Header File
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
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

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <linux/kernel.h>
#if defined(__VMKLNX__)
#include <vmklinux_92/vmklinux_scsi.h>
#endif

#define USB_STORAGE "usb-storage: "

#ifdef CONFIG_USB_STORAGE_DEBUG
void usb_stor_show_command(struct scsi_cmnd *srb);
void usb_stor_show_sense( unsigned char key,
		unsigned char asc, unsigned char ascq );
#define US_DEBUGP(x...) printk( KERN_DEBUG USB_STORAGE x )
#define US_DEBUGPX(x...) printk( x )
#define US_DEBUG(x) x 
#else
#define US_DEBUGP(x...)
#define US_DEBUGPX(x...)
#define US_DEBUG(x)
#endif

#if defined(__VMKLNX__)
static inline int is_mmc2_command(u8 op)
{
	/* keep sorted */
	static u8 mmc2_ops[] = { /* mmc-2 commands currently supported */
		0x46, /* GET_CONFIGURATION */
		0xac, /* GET_PERFORMANCE */
		0xad, /* READ_DISC_STRUCTURE */
	};
	int i;

	for (i = 0; i < sizeof(mmc2_ops) && mmc2_ops[i] <= op; i++) {
		if (op == mmc2_ops[i])
			return 1;
	}
	return 0;
}

#define _VMKLNX_USB_STOR_ALLOW_OPS(srb, us) 			\
	((us->fflags & US_FL_VMKLNX_NO_FILTERING) ||		\
	strncmp(usb_stor_show_command_name(srb),		\
		USB_STOR_UNKNOWN_COMMAND, 			\
		strlen(USB_STOR_UNKNOWN_COMMAND)) ||		\
	((us)->devtype == 0x05 && is_mmc2_command((srb)->cmnd[0])))

void usb_stor_show_command_data(struct scsi_cmnd *srb, char *what);
char *usb_stor_show_command_name(struct scsi_cmnd *srb);
#define USB_STOR_UNKNOWN_COMMAND "(unknown command"

#define USB_STOR_MSG_LEN 256
#define USB_STOR_BUFF_SIZE 4

struct usb_stor_msg_buf {
	char buffer[USB_STOR_BUFF_SIZE][USB_STOR_MSG_LEN+1];
	int head;
	int tail;
};

void usb_stor_add_message_to_buffer(struct usb_stor_msg_buf *,
	char * format, ...);
void usb_stor_walk_and_print_warning_buffer(struct usb_stor_msg_buf *);

extern struct usb_stor_msg_buf usb_stor_default_msgbuf;

#define USB_STOR_MESSAGE_BUFFER(srb)				\
	((srb) ? &host_to_us((srb)->device->host)->msgbuf : 	\
	&usb_stor_default_msgbuf)
#define USB_STOR_DEVICE_SCSI_COMMAND(srb)				\
	(((srb) == NULL) ? "unknown" : usb_stor_show_command_name((srb)))
#define USB_STOR_DEVICE_NAME_FMT "on %s"
#define USB_STOR_DEVICE_NAME(srb)				\
	((srb) && (srb)->device->host->adapter ?		\
         vmklnx_get_vmhba_name(srb->device->host) : "unknown")
/*
 * _VMKLNX_USB_STOR_WARN are always shown.  We also dump the circular buffer
 * of recent _VMKLNX_USB_STOR_MSG.
 */
#define _VMKLNX_USB_STOR_WARN(message, srb, ...)		\
		_VMKLNX_PRINTK_DELAY_THROTTLED_PROLOGUE		\
		printk("usb storage warning (%d throttled) "	\
		    USB_STOR_DEVICE_NAME_FMT " (SCSI cmd %s): "	\
		    message, (_count-1),			\
		    USB_STOR_DEVICE_NAME((srb)) ,		\
		    USB_STOR_DEVICE_SCSI_COMMAND((srb)) ,	\
		    ##__VA_ARGS__);				\
		usb_stor_walk_and_print_warning_buffer(		\
		 USB_STOR_MESSAGE_BUFFER(srb));			\
		_VMKLNX_PRINTK_THROTTLED_EPILOGUE
/*
 * XXX printing _VMKLNX_USB_STOR_MESSAGES has caused timeouts on some usb
 * flash drives so we store them in a circular buffer.
 * 
 * ToDo: move this to a function with var args
 */
#define _VMKLNX_USB_STOR_MSG(message, srb, ...)				    \
	usb_stor_add_message_to_buffer(					    \
	    USB_STOR_MESSAGE_BUFFER(srb),				    \
	    "usb storage message " USB_STOR_DEVICE_NAME_FMT ": " message,   \
	    USB_STOR_DEVICE_NAME((srb)), ##__VA_ARGS__)
#else
#define _VMKLNX_USB_STOR_MSG(message, srb, ...)
#define _VMKLNX_USB_STOR_WARN(message, srb, ...)
#endif /* __VMKLNX__ */

#endif
