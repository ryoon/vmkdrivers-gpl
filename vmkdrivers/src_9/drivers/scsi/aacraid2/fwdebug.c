/*
 *	Adaptec AAC series RAID controller driver
 *
 * Copyright (c) 2004-2007 Adaptec, Inc. (aacraid@adaptec.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdarg.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include "aacraid.h"
#include "fwdebug.h"

static int aac_firmware_debug=0;
module_param_named(firmware_debug, aac_firmware_debug, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(firmware_debug, "Enable Firmware print debugging.\n"
	"\t0=off (default)\n"
	"\t1=print to adapter diagnostic\n"
	"\t2=print to syslog\n"
	"\t3=adapter diagnostic to syslog (unsupported)");

/*
 * Debug flags to be put into the HBA flags field when initialized
 */
static const unsigned long aac_debug_flags = /* Variable to setup with above flags. */
/*			HBA_FLAGS_DBG_KERNEL_PRINT_B |		*/
			HBA_FLAGS_DBG_FW_PRINT_B |
			HBA_FLAGS_DBG_FUNCTION_ENTRY_B |
			HBA_FLAGS_DBG_FUNCTION_EXIT_B |
			HBA_FLAGS_DBG_ERROR_B |
/*			HBA_FLAGS_DBG_INIT_B |			*/
/*			HBA_FLAGS_DBG_OS_COMMANDS_B |		*/
/*			HBA_FLAGS_DBG_SCAN_B |			*/
/*			HBA_FLAGS_DBG_COALESCE_B |		*/
/*			HBA_FLAGS_DBG_IOCTL_COMMANDS_B |	*/
/*			HBA_FLAGS_DBG_SYNC_COMMANDS_B |		*/
/*			HBA_FLAGS_DBG_COMM_B |			*/
/*			HBA_FLAGS_DBG_AIF_B |			*/
/*			HBA_FLAGS_DBG_CSMI_COMMANDS_B | 	*/
/*			HBA_FLAGS_DBG_FLAGS_MASK | 		*/
0;

int aac_get_fw_debug_buffer(struct aac_dev * dev)
{
	if (nblank(fwprintf(x)) && (aac_firmware_debug == 1)) {
		u32 MonDriverBufferPhysAddrLow = 0;
		u32 MonDriverBufferPhysAddrHigh = 0;
		u32 MonDriverBufferSize = 0;
		u32 MonDriverHeaderSize = 0;
		u32 ReturnStatus = 0;

		/*
		 * Initialize the firmware print buffer fields
		 */
		/* Marked list and lock initialized */
		if (dev->FwDebugBuffer_P == (volatile u8 __iomem *)NULL) {
			spin_lock_init(&dev->PrintQueueLock);
			INIT_LIST_HEAD(&dev->PrintQueue);
			/*
			 * Mark list and lock initialized, but not print
			 * ability
			  */
			dev->FwDebugBuffer_P = (volatile u8 __iomem *)
				dev->base;
		}

		/*
		 * Get the firmware print buffer parameters from the firmware
		 * If the command was successful map in the address.
		 */
		if (!aac_adapter_sync_cmd(dev, GET_DRIVER_BUFFER_PROPERTIES,
		  0, 0, 0, 0, 0, 0,
		  &ReturnStatus,
		  &MonDriverBufferPhysAddrLow,
		  &MonDriverBufferPhysAddrHigh,
		  &MonDriverBufferSize,
		  &MonDriverHeaderSize) && MonDriverBufferSize) {
			unsigned long Offset = MonDriverBufferPhysAddrLow -
			     (dev->scsi_host_ptr->base & 0xffffffff);

			/*
			 * See if the address is already mapped in and if so
			 * set it up from the base address
			 */
			if (((u32)(((u64)dev->scsi_host_ptr->base) >> 32)
			  == MonDriverBufferPhysAddrHigh)
			 && ((Offset + MonDriverBufferSize) < dev->base_size))
				dev->FwDebugBuffer_P =
				  (volatile u8 __iomem *)dev->base + Offset;

			/*
			 * If mapping went well, Set up the debug buffer fields
			 * in the HBA structure from the data returned
			 */
			if (dev->FwDebugBuffer_P !=
			  (volatile u8 __iomem *)NULL) {
				dev->FwDebugFlags_P =
				  (volatile __le32 __iomem *)
				    (dev->FwDebugBuffer_P +
				      FW_DEBUG_FLAGS_OFFSET);
				dev->FwDebugStrLength_P =
				  (volatile __le32 __iomem *)
				    (dev->FwDebugBuffer_P +
				      FW_DEBUG_STR_LENGTH_OFFSET);
				dev->FwDebugBLEDvalue_P =
				  dev->FwDebugBuffer_P +
				  FW_DEBUG_BLED_OFFSET;
				dev->FwDebugBLEDflag_P =
				  dev->FwDebugBLEDvalue_P + 1;
				dev->FwDebugBufferSize = MonDriverBufferSize;
				dev->FwDebugBuffer_P += MonDriverHeaderSize;
				dev->FwDebugFlags = 0;
				dev->DebugFlags = aac_debug_flags;
				return 1;
			}
		}

		/*
		 * The GET_DRIVER_BUFFER_PROPERTIES command failed
		 */
	}
	return 0;
}

#define PRINT_TIMEOUT ((HZ+2)/4) /* 1/4 second */

static int aac_fw_send(struct aac_dev * dev, unsigned long PrintFlags,
	const char * PrintBuffer_P, int jafo)
{
	if (nblank(fwprintf(x)) && (aac_firmware_debug == 1)) {
		/*
		 * Make sure the HBA structure has been passed in for this
		 * section
		 */
		if (dev && dev->FwDebugBufferSize) {
			/*
			 * If we are set up for a Firmware print
			 */
			if ((dev->DebugFlags & HBA_FLAGS_DBG_FW_PRINT_B) &&
			  ((PrintFlags &
			    (HBA_FLAGS_DBG_KERNEL_PRINT_B |
			      HBA_FLAGS_DBG_FW_PRINT_B)) !=
			  HBA_FLAGS_DBG_KERNEL_PRINT_B)) {
				/*
				 * Wait for no more than PRINT_TIMEOUT for the
				 * previous message length to clear (the
				 * handshake).
				 */
				if (!jafo) {
					unsigned long counter = 400000000L /
						HZ * PRINT_TIMEOUT;
					unsigned long next_jiffies = jiffies +
								PRINT_TIMEOUT;
					while (readl(dev->FwDebugStrLength_P)
					 && !time_after(jiffies, next_jiffies)
					 && --counter)
						continue; /* schedule(); */
				}
	
				/*
				 * If the Length is clear, copy over the
				 * message, the flags, and the length. Make
				 * sure the length is the last because that is
				 * the signal for the Firmware to pick it up.
				 */
				if (readl(dev->FwDebugStrLength_P))
					return -1;
				{
					/*
					 * Make sure string size is within
					 * boundaries
					 */
					unsigned Length;
					volatile u8 __iomem * dst =
						dev->FwDebugBuffer_P;
					const u8 * src =
						(const u8 *)PrintBuffer_P;
					unsigned Count = strlen(src);
	
					if (Count > dev->FwDebugBufferSize)
						Count = dev->FwDebugBufferSize;
					if ((Length = Count)) do {
						writeb(*src, dst);
						++src;
						++dst;
					} while (--Length);
					writel(dev->FwDebugFlags
					  & HBA_FLAGS_DBG_FLAGS_MASK,
					  dev->FwDebugFlags_P);
					/* Enforce ordering, PCIe break */
					readl(dev->FwDebugFlags_P);
					writel(Count, dev->FwDebugStrLength_P);
				}
			}
	
			/*
			 * If the Kernel Debug Print flag is set, send it off
			 * to the Kernel debugger
			 */
			if (!(dev->DebugFlags & HBA_FLAGS_DBG_KERNEL_PRINT_B))
				return 0;
		}
	}

	if (nblank(fwprintf(x))) {
		if ((aac_firmware_debug == 2) ||
		   ((aac_firmware_debug == 1) && ((PrintFlags &
		    (HBA_FLAGS_DBG_KERNEL_PRINT_B|HBA_FLAGS_DBG_FW_PRINT_B)) !=
		     HBA_FLAGS_DBG_FW_PRINT_B))) {
			if (dev &&
			  (dev->FwDebugFlags & FW_DEBUG_FLAGS_NO_HEADERS_B))
				printk ("%s", PrintBuffer_P);
			else if (dev)
				printk (KERN_INFO "%s: %s\n",
				  dev->scsi_host_ptr->hostt->proc_name,
				  PrintBuffer_P);
			else
				printk (KERN_INFO "%s\n", PrintBuffer_P);
		}
	}

	if (nblank(fwprintf(x)) && (aac_firmware_debug == 1))
		return (jafo && dev && !dev->FwDebugBufferSize);
	else
		return 0;
}

void aac_fw_printf(struct aac_dev * dev, unsigned long PrintFlags, const char * fmt, ...)
{
	if (nblank(fwprintf(x)) &&
	  ((aac_firmware_debug == 1) || (aac_firmware_debug == 2))) {
		va_list args;
		char PrintBuffer_P[PRINT_BUFFER_SIZE+1];
		static struct PrintQueue {
			struct list_head entry;
			unsigned long PrintFlags;
			unsigned long DebugFlags;
			char PrintBuffer_P[1];
		}
		* Spare[512];
		unsigned long pflags = 0;
#ifdef irqs_disabled
#ifdef in_atomic
		int jafo = in_atomic() | in_interrupt() | irqs_disabled();
#else
		int jafo = in_interrupt() | irqs_disabled();
#endif
#elif defined(in_atomic)
		int jafo = in_atomic() | in_interrupt();
#else
		int jafo = in_interrupt();
#endif
	
		if (!jafo && dev) {
			struct Scsi_Host * host = dev->scsi_host_ptr;
	
			jafo = (
			  SHOST_RECOVERY == host->shost_state
			);
		}
		if (!jafo) {
			if (dev)
				jafo = spin_is_locked(
					dev->scsi_host_ptr->host_lock);
		}
		/* In case a print is issued *before* we are set up */
		if (dev && !dev->FwDebugBufferSize &&
		  (dev->FwDebugBuffer_P == (volatile u8 __iomem *)NULL)) {
			spin_lock_init(&dev->PrintQueueLock);
			INIT_LIST_HEAD(&dev->PrintQueue);
			/*
			 * Mark list and lock initialized, but not print
			 * ability
			 */
			dev->FwDebugBuffer_P = (volatile u8 __iomem *)dev->base;
		}
		/*
		 *	Print any queued items
		 */
		if (!jafo) {
			struct aac_dev * aac;
			extern struct list_head aac_devices; /* in linit.c */
			int count;
			struct PrintQueue * ooops = NULL;
	
			/* RePhil the 'lazy' pre-allocated Buckets */
			for (count = sizeof(Spare)/sizeof(Spare[0]); count;) {
				/*
				 * We should not need GFP_ATOMIC, but we also
				 * need to remain paranoid
				 */
				if (!ooops && !(ooops = kmalloc(
				  PRINT_BUFFER_SIZE + sizeof(*ooops),
				  GFP_KERNEL|GFP_ATOMIC)))
					break;
				--count;
				/*
				 * atomic_xchg works with ints, there is no
				 * current support for ptrdiff_t which would
				 * embody pointers as well. xchg in the intel
				 * architecture is atomic for pointers, so we
				 * will accept it's use as one means of
				 * lockless handling of the pre-allocated list.
				 */
				ooops = xchg(&Spare[count], ooops);
			}
			list_for_each_entry(aac, &aac_devices, entry) {
				int jafo1;
				struct Scsi_Host * host;
				unsigned long DebugFlags;
	
				if (list_empty(&aac->PrintQueue))
					continue;
				/* do not mull around too long */
				count = sizeof(Spare)/sizeof(Spare[0]) + 1;
				host = aac->scsi_host_ptr;
				jafo1 = (
				  (SHOST_RECOVERY == host->shost_state)
				  || spin_is_locked(host->host_lock)
				);
				if (!jafo1)
					spin_lock_irqsave (
						&aac->PrintQueueLock, pflags);
				else if (!spin_trylock_irqsave (
						&aac->PrintQueueLock, pflags))
					continue;
				DebugFlags = aac->FwDebugFlags;
				do {
					struct PrintQueue * item = list_entry(
					  aac->PrintQueue.next,
					  struct PrintQueue, entry);
					aac->FwDebugFlags = item->DebugFlags;
					if (aac_fw_send(aac, item->PrintFlags,
					  item->PrintBuffer_P, jafo1))
						break;
					list_del(&item->entry);
					kfree(item);
				} while (--count &&
				  !list_empty(&aac->PrintQueue));
				aac->FwDebugFlags = DebugFlags;
				spin_unlock_irqrestore (&aac->PrintQueueLock,
					pflags);
				if (!count)
					printk(KERN_INFO "aac_fw_printf: you "
					  "need a bigger buffer or a smaller "
					  "hammer");
				/* RePhil the lazy pre-allocated Buckets */
				for (count = sizeof(Spare)/sizeof(Spare[0]);
				  count;) {
					if (!ooops && !(ooops = kmalloc(
					  PRINT_BUFFER_SIZE + sizeof(*ooops),
					  GFP_KERNEL|GFP_ATOMIC)))
						break;
					--count;
					ooops = xchg(&Spare[count], ooops);
				}
			}
			kfree(ooops);
		}
		if ((((PrintFlags
		  & ~(HBA_FLAGS_DBG_KERNEL_PRINT_B|HBA_FLAGS_DBG_FW_PRINT_B)))
		  && dev && !(dev->DebugFlags & PrintFlags))
		 || (dev && !(dev->DebugFlags
		   & (HBA_FLAGS_DBG_KERNEL_PRINT_B|HBA_FLAGS_DBG_FW_PRINT_B))))
			return;
		/*
		 * Set up parameters and call sprintf function to format the
		 * data
		 */
		va_start(args, fmt);
		vsnprintf(PrintBuffer_P, PRINT_BUFFER_SIZE, fmt, args);
		PrintBuffer_P[PRINT_BUFFER_SIZE] = '\0';
		va_end(args);
	
		if (dev) {
			spin_lock_irqsave (&dev->PrintQueueLock, pflags);
			if (!list_empty(&dev->PrintQueue)
			 || aac_fw_send(dev, PrintFlags, PrintBuffer_P, jafo)) {
				struct PrintQueue * item = NULL;
	
				if (jafo) {
					/* Reuse jafo as Spare index */
					jafo = 0;
					do {
						if ((item = xchg(&Spare[jafo],
								NULL)))
							break;
					} while (++jafo <
					  (sizeof(Spare)/sizeof(Spare[0])));
				}
				if (!item) {
					/*
					 * Could cause a switch in interrupt
					 * context
					 */
					item = kmalloc(
					  strlen(PrintBuffer_P) + sizeof(*item),
					  GFP_KERNEL|GFP_ATOMIC);
				}
				if (item) {
					/*
					 * Should print a warning on the
					 * console if failed to buffer the
					 * print, but we are already in more
					 * trouble than we can handle if we
					 * failed to get here.
					 */
					item->PrintFlags = PrintFlags;
					item->DebugFlags = dev->FwDebugFlags;
					strcpy(item->PrintBuffer_P,
						PrintBuffer_P);
					list_add_tail(&item->entry,
						&dev->PrintQueue);
				}
			}
			spin_unlock_irqrestore (&dev->PrintQueueLock, pflags);
		} else
			(void)aac_fw_send(dev, PrintFlags, PrintBuffer_P, jafo);
	}
}

void aac_fw_print_mem(struct aac_dev * dev, unsigned long PrintFlags, u8 * Addr, int Count)
{
if (nblank(fwprintf(x)) || (aac_firmware_debug == 1)) {
	int Offset, i;
	u32 DebugFlags = 0;
	char Buffer[100];
	char * LineBuffer_P;

	/*
	 * If we have an HBA structure, save off the flags and set the no
	 * headers flag so we don't have garbage between our lines of data
	 */
	if (dev != NULL) {
		DebugFlags = dev->FwDebugFlags;
		dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
	}

	Offset = 0;

	/*
	 * Loop through all the data
	 */
	while (Offset < Count) {
		/*
		 * We will format each line into a buffer and then print out
		 * the entire line so set the pointer to the beginning of the
		 * buffer
		 */
		LineBuffer_P = Buffer;

		/*
		 * Set up the address in HEX
		 */
		sprintf(LineBuffer_P, "\n%04x  ", Offset);
		LineBuffer_P += 6;

		/*
		 * Set up 16 bytes in HEX format
		 */
		for (i = 0; i < 16; ++i) {
			/*
			 * If we are past the count of data bytes to output,
			 * pad with blanks
			 */
			sprintf (LineBuffer_P,
			  (((Offset + i) >= Count) ? "   " : "%02x "),
			  Addr[Offset + i]);
			LineBuffer_P += 3;

			/*
			 * At the mid point we will put in a divider
			 */
			if (i == 7) {
				sprintf (LineBuffer_P, "- ");
				LineBuffer_P += 2;
			}
		}
		/*
		 * Now do the same 16 bytes at the end of the line in ASCII
		 * format
		 */
		sprintf (LineBuffer_P, "  ");
		LineBuffer_P += 2;
		for (i = 0; i < 16; ++i) {
			/*
			 * If all data processed, OUT-O-HERE
			 */
			if ((Offset + i) >= Count)
				break;

			/*
			 * If this is a printable ASCII character, convert it
			 */
			sprintf (LineBuffer_P,
			  (((Addr[Offset + i] > 0x1F)
			   && (Addr[Offset + i] < 0x7F))
				? "%c"
				: "."), Addr[Offset + i]);

			++LineBuffer_P;
		}
		/*
		 * The line is now formatted, so print it out
		 */
		aac_fw_printf(dev, PrintFlags, "%s", Buffer);

		/*
		 * Bump the offset by 16 for the next line
		 */
		Offset += 16;

	}

	/*
	 * Restore the saved off flags
	 */
	if (dev != NULL)
		dev->FwDebugFlags = DebugFlags;
}
}
