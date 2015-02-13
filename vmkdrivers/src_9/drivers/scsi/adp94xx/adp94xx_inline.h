/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Written by : David Chaw  <david_chaw@adaptec.com>
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/razor/linux/src/adp94xx_inline.h#34 $
 * 
 */

#ifndef ADP94XX_INLINE_H
#define ADP94XX_INLINE_H


/******************************* Locking routines *****************************/

static inline void 	asd_lock_init(struct asd_softc *asd);
static inline void 	asd_lock(struct asd_softc *asd, u_long *flags);
static inline void 	asd_unlock(struct asd_softc *asd, u_long *flags);

/* Locking routines during midlayer entry points */
static inline void 	asd_sml_lock(struct asd_softc *asd);
static inline void 	asd_sml_unlock(struct asd_softc *asd);

/* Locking routine during list manipulation */
extern spinlock_t asd_list_spinlock;
static inline void 	asd_list_lockinit(void);
static inline void 	asd_list_lock(u_long *flags);
static inline void 	asd_list_unlock(u_long *flags);

static inline void
asd_lock_init(struct asd_softc  *asd)
{
	spin_lock_init(&asd->platform_data->spinlock);
}

static inline void
asd_lock(struct asd_softc *asd, u_long *flags)
{
#if 0
if(asd->debug_flag ==1)
{
	asd_log(ASD_DBG_INFO, "asd lock\n");
}
#endif
#if defined(__VMKLNX__)
	VMK_ASSERT(!vmklnx_spin_is_locked_by_my_cpu(&asd->platform_data->spinlock));
#endif

	spin_lock_irqsave(&asd->platform_data->spinlock, *flags);
}

static inline void
asd_unlock(struct asd_softc *asd, u_long *flags)
{
#if 0
if(asd->debug_flag ==1)
{
	asd_log(ASD_DBG_INFO, "asd Unlock\n");
}
#endif
	spin_unlock_irqrestore(&asd->platform_data->spinlock, *flags);
}

#ifdef CONFIG_SMP
#define ASD_LOCK_ASSERT(asd) \
	ASSERT(spin_is_locked(&asd->platform_data->spinlock))
#else
#define ASD_LOCK_ASSERT(asd)
#endif

static inline void 
asd_sml_lock(struct asd_softc *asd)
{
#if (SCSI_ML_HAS_HOST_LOCK == 0)
	spin_unlock(&io_request_lock);
	spin_lock(&asd->platform_data->spinlock);
#endif
}

static inline void 
asd_sml_unlock(struct asd_softc *asd)
{
#if (SCSI_ML_HAS_HOST_LOCK == 0)
	spin_unlock(&asd->platform_data->spinlock);
	spin_lock(&io_request_lock);
#endif
}

#if (SCSI_ML_HAS_HOST_LOCK == 0)
#define ASD_SML_LOCK_ASSERT(asd)  {\
	ASSERT(!spin_is_locked(&asd->platform_data->spinlock)); \
	ASSERT(spin_is_locked(&io_request_lock)) }
#else
#define ASD_SML_LOCK_ASSERT(asd)
#endif

static inline void 
asd_list_lockinit(void)
{
	spin_lock_init(&asd_list_spinlock);
}

static inline void 
asd_list_lock(u_long *flags)
{
	spin_lock_irqsave(&asd_list_spinlock, *flags);
}

static inline void 
asd_list_unlock(u_long *flags)
{
	spin_unlock_irqrestore(&asd_list_spinlock, *flags);
}


/* Delay routine for microseconds interval */
static inline void asd_delay(long usecs);

static inline void
asd_delay(long usecs)
{
	/* 
	 * Delay max up to 2 milliseconds at a time. 
	 */
	/*touch_nmi_watchdog();*/
	if (usecs > 1000)
		mdelay((usecs/1000));
	else
		udelay(usecs);
}

static inline char *asd_name(struct asd_softc  *asd);
	
static inline char *
asd_name(struct asd_softc  *asd)
{
	return (asd->profile.name);
}

/************************* Large Disk Handling ********************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static inline int asd_sector_div(u_long capacity, int heads, int sectors);

static inline int
asd_sector_div(u_long capacity, int heads, int sectors)
{
	return (capacity / (heads * sectors));
}
#else
static inline int asd_sector_div(sector_t capacity, int heads, int sectors);

static inline int
asd_sector_div(sector_t capacity, int heads, int sectors)
{
	sector_div(capacity, (heads * sectors));
	return (int)capacity;
}
#endif

/************************ Memory space  access routines ***********************/

static inline uint8_t  	asd_read_byte(struct asd_softc *asd, u_long offset);
static inline uint16_t 	asd_read_word(struct asd_softc *asd, u_long offset);
static inline uint32_t 	asd_read_dword(struct asd_softc *asd, u_long offset);
static inline void 	asd_write_byte(struct asd_softc *asd, u_long offset, 
				       uint8_t val);
static inline void 	asd_write_word(struct asd_softc *asd, u_long offset, 
				       uint16_t val);
static inline void 	asd_write_dword(struct asd_softc *asd, u_long offset, 
					uint32_t val);
static inline void 	asd_readstr_byte(struct asd_softc *asd, u_long offset,
	       				 uint8_t *buffer, u_long count);
static inline void 	asd_readstr_word(struct asd_softc *asd, u_long offset,
					 uint16_t *buffer, u_long count);
static inline void 	asd_readstr_dword(struct asd_softc *asd, u_long offset,
					  uint32_t *buffer, u_long count);
static inline void 	asd_writestr_byte(struct asd_softc *asd, u_long offset,
					  uint8_t *buffer, u_long count);
static inline void 	asd_writestr_word(struct asd_softc *asd, u_long offset,
					  uint16_t *buffer, u_long count);
static inline void 	asd_writestr_dword(struct asd_softc *asd, u_long offset,					   uint32_t *buffer, u_long count);

static inline uint8_t
asd_read_byte(struct asd_softc *asd, u_long offset)
{
	uint8_t		val;
	
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		val = inb(asd->io_handle[0]->baseaddr.iobase + 
			 (offset & 0xFF));
	else
		val = readb(asd->io_handle[0]->baseaddr.membase + offset);
	mb();
	return val;
}

static inline uint16_t
asd_read_word(struct asd_softc *asd, u_long offset)
{
	uint16_t	val;
	
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		val = inw(asd->io_handle[0]->baseaddr.iobase + 
			 (offset & 0xFF));
	else
		val = readw(asd->io_handle[0]->baseaddr.membase + offset);	
	mb();	
	return val;
}

static inline uint32_t
asd_read_dword(struct asd_softc *asd, u_long offset)
{
	uint32_t	val;
	
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		val = inl(asd->io_handle[0]->baseaddr.iobase + 
			 (offset & 0xFF));
	else
		val = readl(asd->io_handle[0]->baseaddr.membase + offset);
	mb();
	return val;
}

static inline void
asd_write_byte(struct asd_softc *asd, u_long offset, uint8_t val)
{
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		outb(val, asd->io_handle[0]->baseaddr.iobase + 
		    (offset & 0xFF));
	else
		writeb(val, asd->io_handle[0]->baseaddr.membase + offset);
	mb();
}
	
static inline void
asd_write_word(struct asd_softc *asd, u_long offset, uint16_t val)
{
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		outw(val, asd->io_handle[0]->baseaddr.iobase + 
		    (offset & 0xFF));
	else
		writew(val, asd->io_handle[0]->baseaddr.membase + offset);
	mb();
}

static inline void
asd_write_dword(struct asd_softc *asd, u_long offset, uint32_t val)
{
	if (asd->io_handle[0]->type == ASD_IO_SPACE)
		outl(val, asd->io_handle[0]->baseaddr.iobase + 
		    (offset & 0xFF));
	else
		writel(val, asd->io_handle[0]->baseaddr.membase + offset);
	mb();
}

static inline void
asd_readstr_byte(struct asd_softc *asd, u_long offset, uint8_t *buffer,
		 u_long	count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		*buffer++ = asd_read_byte(asd, offset);
}

static inline void
asd_readstr_word(struct asd_softc *asd, u_long offset, uint16_t *buffer,
		 u_long	count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		*buffer++ = asd_read_word(asd, offset);
}

static inline void
asd_readstr_dword(struct asd_softc *asd, u_long	offset, uint32_t *buffer,
		  u_long count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		*buffer++ = asd_read_dword(asd, offset);
}

static inline void
asd_writestr_byte(struct asd_softc *asd, u_long offset, uint8_t	*buffer,
		  u_long count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		asd_write_byte(asd, offset, *buffer++);
}

static inline void
asd_writestr_word(struct asd_softc *asd, u_long	offset, uint16_t *buffer,
		  u_long count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		asd_write_word(asd, offset, *buffer++);
}

static inline void
asd_writestr_dword(struct asd_softc *asd, u_long offset, uint32_t *buffer,
		   u_long count)
{ 	
	u_long	i;
	
	for (i = 0; i < count; i++)
		asd_write_dword(asd, offset, *buffer++);
}


/******************* PCI Config Space (PCIC) access routines ******************/

static inline uint8_t	asd_pcic_read_byte(struct asd_softc *asd, int reg);
static inline uint16_t 	asd_pcic_read_word(struct asd_softc *asd, int reg);   
static inline uint32_t 	asd_pcic_read_dword(struct asd_softc *asd, int reg);
static inline void 	asd_pcic_write_byte(struct asd_softc *asd, int reg, 
					    uint8_t val);
static inline void 	asd_pcic_write_word(struct asd_softc *asd, int reg, 
					    uint16_t val);
static inline void 	asd_pcic_write_dword(struct asd_softc *asd, int reg, 
					     uint32_t val);
static inline void 	asd_flush_device_writes(struct asd_softc *asd);
					
static inline uint8_t
asd_pcic_read_byte(struct asd_softc *asd, int reg)
{
	struct pci_dev	*dev;
	uint8_t		val;
	
	dev = asd_dev_to_pdev(asd->dev);
	pci_read_config_byte(dev, reg, &val);
	
	return (val);
}

static inline uint16_t
asd_pcic_read_word(struct asd_softc *asd, int reg)
{
	struct pci_dev	*dev;
	uint16_t	val;
	
	dev = asd_dev_to_pdev(asd->dev);	
	pci_read_config_word(dev, reg, &val);
	
	return (val);
}

static inline uint32_t
asd_pcic_read_dword(struct asd_softc *asd, int reg)
{
	struct pci_dev	*dev;
	uint32_t	val;
	
	dev = asd_dev_to_pdev(asd->dev);
	pci_read_config_dword(dev, reg, &val);
	
	return (val);
}

static inline void
asd_pcic_write_byte(struct asd_softc *asd, int reg, uint8_t val)
{
	struct pci_dev	*dev;
	
	dev = asd_dev_to_pdev(asd->dev);
	pci_write_config_byte(dev, reg, val);
}	
 
static inline void
asd_pcic_write_word(struct asd_softc *asd, int reg, uint16_t val)
{
	struct pci_dev	*dev;
	
	dev = asd_dev_to_pdev(asd->dev);
	pci_write_config_word(dev, reg, val);
}

static inline void
asd_pcic_write_dword(struct asd_softc *asd, int reg, uint32_t val)
{
	struct pci_dev	*dev;
	
	dev = asd_dev_to_pdev(asd->dev);
	pci_write_config_dword(dev, reg, val);
}

static inline void
asd_flush_device_writes(struct asd_softc *asd)
{
	/*
	 * Reading the interrupt status register
	 * is always safe, but is this enough to
	 * flush writes on *all* the architectures
	 * we may support?
	 */
	asd_read_dword(asd, CHIMINT);
}


/**************** Sliding Windows memory read/write utilities *****************/

static inline uint32_t	asd_hwi_adjust_sw_b(struct asd_softc *asd, 
					    uint32_t reg);
static inline uint32_t	asd_hwi_adjust_sw_c(struct asd_softc *asd, 
					    uint32_t reg);
static inline uint8_t	asd_hwi_swb_read_byte(struct asd_softc *asd, 
					      uint32_t reg);
static inline uint16_t	asd_hwi_swb_read_word(struct asd_softc *asd, 
					      uint32_t reg);
static inline uint32_t	asd_hwi_swb_read_dword(struct asd_softc *asd, 
					       uint32_t reg);
static inline void	asd_hwi_swb_write_byte(struct asd_softc *asd, 
					       uint32_t reg, uint8_t val);
static inline void	asd_hwi_swb_write_word(struct asd_softc *asd, 
					       uint32_t reg, uint16_t val);
static inline void	asd_hwi_swb_write_dword(struct asd_softc *asd, 
						uint32_t reg, uint32_t val);
static inline uint8_t	asd_hwi_swc_read_byte(struct asd_softc *asd, 
					      uint32_t reg);
static inline uint16_t	asd_hwi_swc_read_word(struct asd_softc *asd, 
					      uint32_t reg);
static inline uint32_t	asd_hwi_swc_read_dword(struct asd_softc *asd, 
					       uint32_t reg);
static inline void	asd_hwi_swc_write_byte(struct asd_softc *asd, 
					       uint32_t reg, uint8_t val);
static inline void	asd_hwi_swc_write_word(struct asd_softc *asd, 
					       uint32_t reg, uint16_t val);
static inline void	asd_hwi_swc_write_dword(struct asd_softc *asd, 
						uint32_t reg, uint32_t val);
static inline void 	asd_hwi_set_hw_addr(struct asd_softc *asd, 
					    uint32_t base_addr, 
					    dma_addr_t bus_addr);

/* 
 * Function:
 *	asd_hwi_adjust_sw_b()
 *
 * Description:
 *      Setup the Sliding Window B to proper base in order to be able to 
 *	access to the desired register.
 */
static inline uint32_t
asd_hwi_adjust_sw_b(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	new_swb_base;
	uint32_t	new_reg_offset;
	
	if (asd->io_handle[0]->type == ASD_IO_SPACE) {
		/* IO Mapped. */
		new_swb_base = (uint32_t) (reg & ~BASEB_IOMAP_MASK);
		new_reg_offset = (uint32_t) (reg & BASEB_IOMAP_MASK);
	} else {
		/* LT, 2005/10/24: This is the proper way to do it. */
		new_swb_base = reg & ~((asd->io_handle[0]->length - 0x80) - 1);
		new_reg_offset = reg - new_swb_base;
#if 0		
		/*To be reviewed...*/ 

		/* Memory Mapped. */
		uint32_t	mbar0_mask;
		uint32_t	offset;

		offset = (uint32_t) (reg - asd->io_handle[0]->swb_base);
		mbar0_mask = asd->io_handle[0]->length - 1;

		/* 
		* Check if the register to be accessed is already accessible 
		* with the current window B and avoid reprogramming the 
		* window. 
	        *
		* The boundaries of window B in a memory mapped space are:
		* lower bound: 
		*	io_handle[0]->swb_base (e.g. 0xBF80_0000h,current value)
		*	mbar0_mask (e.g., 0003_FFFFh)
		*	BASEB_START (e.g. 0000_0080h) or BF83_FF7Fh for example.
		*
		* The following check assumes that the largest access to
		* reg will be an 8 byte access.
		*/
         
		if ((reg >= (uint32_t) asd->io_handle[0]->swb_base) &&
			((reg + 7) <= (uint32_t) (asd->io_handle[0]->swb_base 
				+ mbar0_mask - BASEB_START))) {
			/* 
			* The window B is already positioned to allow the 
			* register to be accessed. The offset to the register 
			* is the difference between the register to be accessed 
			* and the current value of the base address register
			*/
			new_swb_base = (uint32_t) asd->io_handle[0]->swb_base;
			new_reg_offset = offset;
		} else {
			/*
			* The window B base address register must be changed to 
			* position window B to a location where the register 
			* can be accessed.
			*/
			new_swb_base = (uint32_t) (reg & ~BASEB_IOMAP_MASK);
			new_reg_offset = (uint32_t) (reg & BASEB_IOMAP_MASK);
		}
#endif		
#if 0
		/* 
		* The following code is not working correctly with a reg 
		* value of 0xBF83FF80. The resulting base of 0xBF80_0000 and 
		* the resulting offset of 0x003F_FF80, appear correct, but 
		* the calling routines always add BASEB_START to the offset. 
		* This results in an invalid write acess.
		*/
      
		uint32_t	mbar0_mask;
		uint32_t	offset;
		
		offset = (uint32_t) (reg - asd->io_handle[0]->swb_base);
		mbar0_mask = asd->io_handle[0]->length - 1;
		if (offset & ~mbar0_mask) {
			/*
			 * The register needs to be accessed is out of the 
			 * current sliding window B range.
			 */
			new_swb_base = (uint32_t) (reg & ~mbar0_mask);
			new_reg_offset = (uint32_t) (reg & mbar0_mask);
		} else {
			/*
			 * The register needs to be accessed is in the 
			 * current sliding window B range.
			 */
			 new_swb_base = asd->io_handle[0]->swb_base;
			 new_reg_offset = offset;
		}
#endif
	}
	/*
	 * Adjust the SW B Base if the the new base is different from the old
	 * one.
	 */
	if (new_swb_base != asd->io_handle[0]->swb_base) {
#ifdef ASD_DEBUG
//use debug_flag to check the first 10 setting (12 to 2)
#if 0
		if(asd->debug_flag>2)
		{
			asd_log(ASD_DBG_INFO, "asd->io_handle[0]->swb_base=0x%x, asd->io_handle[0]->length=0x%x \n",
				asd->io_handle[0]->swb_base, asd->io_handle[0]->length);
			asd_log(ASD_DBG_INFO, "reg=0x%x, new_swb_base=0x%x, new_reg_offset=0x%x \n",
				reg, new_swb_base, new_reg_offset);
			asd->debug_flag--;
		}
#endif
#endif
		asd_write_dword(asd, PCIC_BASEB, new_swb_base);
		asd->io_handle[0]->swb_base = new_swb_base;
	}
	
	return (new_reg_offset);
} 

/* 
 * Function:
 *	asd_hwi_adjust_sw_c()
 *
 * Description:
 *      Setup the Sliding Window C to proper base in order to be able to 
 *	access to the desired register.
 */
static inline uint32_t   
asd_hwi_adjust_sw_c(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	new_swc_base;
	uint32_t	new_reg_offset;

	new_swc_base = (uint32_t) (reg & ~BASEC_MASK);
	new_reg_offset = (uint32_t) (reg & BASEC_MASK);
	
	asd_write_dword(asd, PCIC_BASEC, new_swc_base);
	return (new_reg_offset);
}

static inline uint8_t
asd_hwi_swb_read_byte(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	return ((uint8_t) asd_read_byte(asd, (reg_offset + BASEB_START)));
}

static inline uint16_t
asd_hwi_swb_read_word(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	return ((uint16_t) asd_read_word(asd, (reg_offset + BASEB_START)));
}

static inline uint32_t
asd_hwi_swb_read_dword(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	return ((uint32_t) asd_read_dword(asd, (reg_offset + BASEB_START)));
}

static inline void
asd_hwi_swb_write_byte(struct asd_softc *asd, uint32_t reg, uint8_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	asd_write_byte(asd, (reg_offset + BASEB_START), val);
}

static inline void
asd_hwi_swb_write_word(struct asd_softc *asd, uint32_t reg, uint16_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	asd_write_word(asd, (reg_offset + BASEB_START), val);
}

static inline void
asd_hwi_swb_write_dword(struct asd_softc  *asd, uint32_t reg, uint32_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_b(asd, reg);
	asd_write_dword(asd, (reg_offset + BASEB_START), val);
}

static inline uint8_t
asd_hwi_swc_read_byte(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);
	return ((uint8_t) asd_read_byte(asd, (reg_offset + BASEC_START)));
} 

static inline uint16_t
asd_hwi_swc_read_word(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);
	return ((uint16_t) asd_read_word(asd, (reg_offset + BASEC_START)));
}

static inline uint32_t
asd_hwi_swc_read_dword(struct asd_softc *asd, uint32_t reg)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);
	return ((uint32_t) asd_read_dword(asd, (reg_offset + BASEC_START)));
}

static inline void
asd_hwi_swc_write_byte(struct asd_softc *asd, uint32_t reg, uint8_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);	
	asd_write_byte(asd, (reg_offset + BASEC_START), val);
}

static inline void
asd_hwi_swc_write_word(struct asd_softc *asd, uint32_t reg, uint16_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);	
	asd_write_word(asd, (reg_offset + BASEC_START), val);
}

static inline void
asd_hwi_swc_write_dword(struct asd_softc *asd, uint32_t reg, uint32_t val)
{
	uint32_t	reg_offset;
	
	reg_offset = asd_hwi_adjust_sw_c(asd, reg);	
	asd_write_dword(asd, (reg_offset + BASEC_START), val);
}

static inline void
asd_hwi_set_hw_addr(struct asd_softc *asd, uint32_t base_addr,
		    dma_addr_t bus_addr)
{
	asd_hwi_swb_write_dword(asd, base_addr,
				asd_htole32(ASD_GET_PADR(bus_addr)));
	asd_hwi_swb_write_dword(asd, base_addr + 4,
				asd_htole32(ASD_GET_PUADR(bus_addr)));
}


/************************** Scatter/Gather entry setup ************************/

static inline int	asd_sg_setup(struct sg_element *sg, dma_addr_t addr,
				     uint32_t len, int last);

static inline int
asd_sg_setup(struct sg_element *sg, dma_addr_t addr, uint32_t len, int last)
{
	sg->bus_address = asd_htole64(addr);
	sg->length = asd_htole32(len);
	memset(sg->res_1, 0, sizeof(*sg) - offsetof(struct sg_element, res_1));
	if (last)
		sg->flags |= SG_EOL;
	return (0);
}


/********************* Host Queue management routines ************************/ 

static inline struct asd_device *
			asd_next_device_to_run(struct asd_softc *asd);
static inline void	asd_schedule_runq(struct asd_softc *asd);
static inline void	asd_schedule_unblock(struct asd_softc *asd);
static inline void	asd_freeze_hostq(struct asd_softc *asd);
static inline void	asd_release_hostq(struct asd_softc *asd);
static inline void	asd_release_hostq_locked(struct asd_softc *asd);
static inline void	asd_freeze_targetq(struct asd_softc *asd,
					   struct asd_target *targ);
static inline void	asd_unfreeze_targetq(struct asd_softc *asd,
					     struct asd_target *targ);
static inline void	asd_setup_dev_timer(struct asd_device *dev,
					    u_long timeout,
					    void (*func)(u_long));
#if defined(__VMKLNX__)
static inline void	asd_setup_dev_dpc_task(struct asd_device *dev,
					       work_func_t func);
#else /* !defined(__VMKLNX__) */
static inline void	asd_setup_dev_dpc_task(struct asd_device *dev,
					       void (*func)(void *));
#endif /* defined(__VMKLNX__) */
					

static inline struct asd_device *
asd_next_device_to_run(struct asd_softc *asd)
{
	struct	asd_device	*dev;

	ASD_LOCK_ASSERT(asd);

	if (asd->platform_data->qfrozen != 0) {
		return (NULL);
	}

	list_for_each_entry(dev, &asd->platform_data->device_runq, links) {

		if ((dev->target->src_port->events & 
			ASD_DISCOVERY_PROCESS) != 0)
			continue;

		return dev;
	}

	return NULL;
}


/*
 * Must be called with our lock held.
 */
static inline void
asd_schedule_runq(struct asd_softc *asd)
{
	ASD_LOCK_ASSERT(asd);

	tasklet_schedule(&asd->platform_data->runq_tasklet);
}

static inline void
asd_schedule_unblock(struct asd_softc *asd)
{
	tasklet_schedule(&asd->platform_data->unblock_tasklet);
}

static inline void
asd_freeze_hostq(struct asd_softc *asd)
{
	asd->platform_data->qfrozen++;
	if (asd->platform_data->qfrozen == 1)
		scsi_block_requests(asd->platform_data->scsi_host);
}

static inline void
asd_release_hostq(struct asd_softc *asd)
{
	u_long flags;

	asd_lock(asd, &flags);
	asd_release_hostq_locked(asd);
	asd_unlock(asd, &flags);
}

static inline void
asd_release_hostq_locked(struct asd_softc *asd)
{
	if (asd->platform_data->qfrozen > 0)
		asd->platform_data->qfrozen--;

	if (asd->platform_data->qfrozen == 0) {
		asd_schedule_unblock(asd);
		asd_schedule_runq(asd);
	}
}

static inline void
asd_freeze_targetq(struct asd_softc *asd, struct asd_target *targ)
{
	if(targ->qfrozen==0)
	{
		targ->qfrozen++;
	}
}

static inline void
asd_unfreeze_targetq(struct asd_softc *asd, struct asd_target *targ)
{
	if (targ->qfrozen > 0)
		targ->qfrozen--;
}

static inline void
asd_setup_dev_timer(struct asd_device *dev, u_long timeout,
		    void (*func)(u_long))
{
	dev->flags |= ASD_DEV_TIMER_ACTIVE;
	init_timer(&dev->timer);
	dev->timer.expires = jiffies + timeout;
	dev->timer.data = (u_long) dev;
	dev->timer.function = func;
	add_timer(&dev->timer);
}

static inline void
#if defined(__VMKLNX__)
asd_setup_dev_dpc_task(struct asd_device *dev, work_func_t func)
#else /* !defined(__VMKLNX__) */
asd_setup_dev_dpc_task(struct asd_device *dev, void (*func)(void *))
#endif /* defined(__VMKLNX__) */
{
	dev->flags |= ASD_DEV_DPC_ACTIVE;
#if defined(__VMKLNX__)
	INIT_WORK(&dev->workq, func);
	schedule_work(&dev->workq);
#else /* !defined(__VMKLNX__) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,41)
	dev->taskq.routine = func;
	dev->taskq.data = (void *) dev;
	schedule_task(&dev->taskq);
#else
	INIT_WORK(&dev->workq, func, (void *) dev);
	schedule_work(&dev->workq);
#endif
#endif /* defined(__VMKLNX__) */
}

/**************************** Semaphores Handling *****************************/

static inline void	asd_sleep_sem(struct semaphore *sem);
static inline void	asd_wakeup_sem(struct semaphore *sem);

static inline void
asd_sleep_sem(struct semaphore *sem)
{
	down(sem);
}

static inline void
asd_wakeup_sem(struct semaphore *sem)
{
	up(sem);
}

/****************************** SCB/EDB Handling ******************************/

static inline void 	asd_platform_scb_free(struct asd_softc *asd, 
					      struct scb *scb);
static inline void 	asd_hwi_free_scb(struct asd_softc *asd,
					 struct scb *scb);
static inline void 	*asd_hwi_sdata_bus_to_virt(struct asd_softc *asd, 
						   uint64_t baddr);
static inline union edb *asd_hwi_get_edb_vaddr(struct asd_softc *asd, 
						uint64_t baddr);
static inline union edb *asd_hwi_get_edb_from_dl(struct asd_softc *asd,
						 struct scb *scb,
						 struct asd_done_list *dl,
						 struct scb **pescb,
						 u_int *pedb_index);
static inline void	asd_setup_scb_timer(struct scb *scb, u_long timeout,
					    void (*func)(u_long));
static inline void	asd_cmd_set_retry_status(Scsi_Cmnd *cmd);
#if !defined(__VMKLNX__) /* Specific to Linux 2.5.0 and earlier, not used in ESX */
static inline void	asd_cmd_set_offline_status(Scsi_Cmnd *cmd);
#endif /* !defined(__VMKLNX__) */

static inline void
asd_platform_scb_free(struct asd_softc *asd, struct scb *scb)
{
	if ((asd->flags & ASD_SCB_RESOURCE_SHORTAGE) != 0) {
		asd->flags &= ~ASD_SCB_RESOURCE_SHORTAGE;
		if ((asd->flags & ASD_WAITING_FOR_RESOURCE) != 0) {
			asd->flags &= ~ASD_WAITING_FOR_RESOURCE;
			wake_up(&asd->platform_data->waitq);
		}
		asd_release_hostq_locked(asd);
	}
}

static inline void
asd_hwi_free_scb(struct asd_softc *asd, struct scb *scb)
{
//JD
	scb->io_ctx=NULL;
	scb->post_stack_depth=0;
	if ((scb->flags & SCB_RESERVED) != 0) {
#ifdef ASD_DEBUG
		printk("Free reserved scb 0x%x flags 0x%x\n",scb, scb->flags);
#endif

        	list_add(&scb->hwi_links, &asd->rsvd_scbs);
	} else {
      	list_add(&scb->hwi_links, &asd->free_scbs);
		/* Notify the OSM that a resource is now available. */
		asd_platform_scb_free(asd, scb);
	}

	/* 
	 * Clean up the SCB's flags for the next user.
	 */
	scb->flags = SCB_FLAG_NONE;
}

static inline void *
asd_hwi_sdata_bus_to_virt(struct asd_softc *asd, uint64_t baddr)
{
	uint8_t		*vaddr;
	uint64_t	vaddr_offset;

	vaddr_offset = baddr - asd->shared_data_map.busaddr;
	vaddr = asd->shared_data_map.vaddr + vaddr_offset;
	return (vaddr);
}

static inline union edb *
asd_hwi_get_edb_vaddr(struct asd_softc *asd, uint64_t baddr)
{
	return ((union edb *)asd_hwi_sdata_bus_to_virt(asd, baddr));
}

static inline union edb * 
asd_hwi_get_edb_from_dl(struct asd_softc *asd, struct scb *scb, 
	   		struct asd_done_list *dl, struct scb **pescb, 
			u_int *pedb_index)
{
	union edb 		*edb;
	struct response_sb   	*rsp;
	struct scb 		*escb;
	u_int  			 escb_index;
	u_int			 edb_index;

	edb = NULL;
	rsp = &dl->stat_blk.response;
	escb_index = asd_le16toh(rsp->empty_scb_tc);
	edb_index = RSP_EDB_ELEM(rsp) - 1;

	edb = asd_hwi_indexes_to_edb(asd, &escb, escb_index, edb_index);

	*pescb = escb;
	*pedb_index = edb_index;

	return edb;
}

static inline void
asd_setup_scb_timer(struct scb *scb, u_long timeout, void (*func)(u_long))
{
	init_timer(&scb->platform_data->timeout);
	scb->platform_data->timeout.expires = jiffies + timeout;
	scb->platform_data->timeout.data = (u_long) scb;
	scb->platform_data->timeout.function = func;
	add_timer(&scb->platform_data->timeout);
}

static inline void
asd_cmd_set_retry_status(Scsi_Cmnd *cmd)
{
	/*
	 * If we want the request requeued, make sure there
	 * are sufficent retries.  In the old scsi error code,
	 * we used to be able to specify a result code that
	 * bypassed the retry count.  Now we must use this
	 * hack.  We also "fake" a check condition with
	 * a sense code of ABORTED COMMAND.  This seems to
	 * evoke a retry even if this command is being sent
	 * via the eh thread.  Ick!  Ick!  Ick!
	 */
	if (cmd->retries > 0)
		cmd->retries--;

	asd_cmd_set_scsi_status(cmd, SCSI_STATUS_CHECK_COND);
	asd_cmd_set_host_status(cmd, DID_OK);
	asd_cmd_set_driver_status(cmd, DRIVER_SENSE);
	memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
	cmd->sense_buffer[0] = SSD_ERRCODE_VALID
			     | SSD_CURRENT_ERROR;
	cmd->sense_buffer[2] = SSD_KEY_ABORTED_COMMAND;
} 

#if !defined(__VMKLNX__) /* Specific to Linux 2.5.0 and earliier, not used in ESX */
static inline void
asd_cmd_set_offline_status(Scsi_Cmnd *cmd)
{
	asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
	asd_cmd_set_driver_status(cmd, DRIVER_SENSE);
	memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
	cmd->sense_buffer[0] = SSD_ERRCODE_VALID
			     | SSD_CURRENT_ERROR;
	cmd->sense_buffer[2] = SSD_KEY_NOT_READY;
}
#endif /* !defined(__VMKLNX__) */

static inline void
asd_build_sas_header(
struct asd_target 		*targ,
struct asd_ssp_task_hscb	*ssp_hscb
)
{
	ssp_hscb->protocol_conn_rate = targ->ddb_profile.conn_rate;
	ssp_hscb->sas_header.frame_type = OPEN_ADDR_FRAME;
	memcpy(ssp_hscb->sas_header.hashed_dest_sasaddr,
	       targ->ddb_profile.hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	ssp_hscb->sas_header.res = 0;
	memcpy(ssp_hscb->sas_header.hashed_src_sasaddr,
	       targ->src_port->hashed_sas_addr, HASHED_SAS_ADDR_LEN);
	memset(ssp_hscb->sas_header.res1, 0,
	       offsetof(struct asd_sas_header, target_port_xfer_tag) - 
	       offsetof(struct asd_sas_header, res1));
	ssp_hscb->sas_header.target_port_xfer_tag = 0xFFFF;
	ssp_hscb->sas_header.data_offset = 0;
	ssp_hscb->conn_handle = targ->ddb_profile.conn_handle;
	ssp_hscb->sister_scb = 0xFFFF;
	ssp_hscb->retry_cnt = TASK_RETRY_CNT;

	/*
	 * SSP Command IU.
	 */
	memset(ssp_hscb->lun, 0,
	       offsetof(struct asd_ssp_task_hscb, cdb) -
	       offsetof(struct asd_ssp_task_hscb, lun));

	memset(&ssp_hscb->data_dir_flags, 0, 
	       offsetof(struct asd_ssp_task_hscb, sg_elements) - 
	       offsetof(struct asd_ssp_task_hscb, data_dir_flags));

	ssp_hscb->data_dir_flags = 0;
}

static inline void
asd_push_post_stack_timeout(
struct asd_softc	*asd,
struct scb		*scb,
asd_io_ctx_t		io_ctx,
asd_scb_post_t		*post,
void			(*timeout_func)(u_long)
)
{
	if (scb->post_stack_depth == SCB_POST_STACK_DEPTH) {
		panic("post_stack overflow\n");
	}

	scb->post_stack[scb->post_stack_depth].io_ctx = io_ctx;
	scb->post_stack[scb->post_stack_depth].post = post;
	scb->post_stack[scb->post_stack_depth].timeout_func = timeout_func;

	scb->io_ctx = io_ctx;
	scb->post = post;

	scb->post_stack_depth++;
}

static inline void
asd_push_post_stack(
struct asd_softc	*asd,
struct scb		*scb,
asd_io_ctx_t		io_ctx,
asd_scb_post_t		*post
)
{
	asd_push_post_stack_timeout(asd, scb, io_ctx, post, NULL);
}

static inline void
asd_pop_post_stack(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	if (scb->post_stack_depth == 0) {
//JD		panic("post_stack underflow - opcode 0x%x done_list=%p "
//			"scb = %p\n", done_listp->opcode, done_listp, scb);
			asd_log(ASD_DBG_ERROR, "Post overflow scb ptr=%p\n",scb);
		return;
	}

	scb->post_stack_depth--;

	scb->io_ctx = scb->post_stack[scb->post_stack_depth].io_ctx;
	scb->post = scb->post_stack[scb->post_stack_depth].post;

	scb->post(asd, scb, done_listp);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
#define SCSI_DATA_READ  DMA_FROM_DEVICE
#define SCSI_DATA_WRITE DMA_TO_DEVICE
#define SCSI_DATA_UNKNOWN DMA_BIDIRECTIONAL
#define SCSI_DATA_NONE  DMA_NONE
#define scsi_to_pci_dma_dir(_a) (_a)
#endif

#if defined(__VMKLNX__)
#define del_timer_sync(timer) \
{ \
    if (in_irq()) { \
	del_timer(timer); \
    } else { \
	del_timer_sync(timer); \
    } \
}
#endif /* #if defined(__VMKLNX__) */

#endif /* ADP94XX_INLINE_H */

