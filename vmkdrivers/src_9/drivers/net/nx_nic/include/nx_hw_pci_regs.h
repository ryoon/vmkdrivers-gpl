/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#ifndef __NX_HW_REGS_H
#define __NX_HW_REGS_H

/***************************************************************************
 *		PCI related defines.
 **************************************************************************/

/*
 * Interrupt related defines.
 */
#define PCIX_TARGET_STATUS	(0x10118)
#define PCIX_TARGET_STATUS_F1	(0x10160)
#define PCIX_TARGET_STATUS_F2	(0x10164)
#define PCIX_TARGET_STATUS_F3	(0x10168)
#define PCIX_TARGET_STATUS_F4	(0x10360)
#define PCIX_TARGET_STATUS_F5	(0x10364)
#define PCIX_TARGET_STATUS_F6	(0x10368)
#define PCIX_TARGET_STATUS_F7	(0x1036c)

#define PCIX_TARGET_MASK	(0x10128)
#define PCIX_TARGET_MASK_F1	(0x10170)
#define PCIX_TARGET_MASK_F2	(0x10174)
#define PCIX_TARGET_MASK_F3	(0x10178)
#define PCIX_TARGET_MASK_F4	(0x10370)
#define PCIX_TARGET_MASK_F5	(0x10374)
#define PCIX_TARGET_MASK_F6	(0x10378)
#define PCIX_TARGET_MASK_F7	(0x1037c)

/*
 * Message Signaled Interrupts
 */
#define PCIX_MSI_F0		(0x13000)
#define PCIX_MSI_F1		(0x13004)
#define PCIX_MSI_F2		(0x13008)
#define PCIX_MSI_F3		(0x1300c)
#define PCIX_MSI_F4		(0x13010)
#define PCIX_MSI_F5		(0x13014)
#define PCIX_MSI_F6		(0x13018)
#define PCIX_MSI_F7		(0x1301c)
#define PCIX_MSI_F(FUNC)	(0x13000 +((FUNC) * 4))

/*
 *
 */
#define PCIX_INT_VECTOR		(0x10100)
#define PCIX_INT_MASK		(0x10104)

/*
 * Interrupt state machine and other bits.
 */
#define PCIE_MISCCFG_RC		(0x1206c)


#define ISR_INT_TARGET_STATUS	  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS))
#define ISR_INT_TARGET_STATUS_F1  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F1))
#define ISR_INT_TARGET_STATUS_F2  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F2))
#define ISR_INT_TARGET_STATUS_F3  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F3))
#define ISR_INT_TARGET_STATUS_F4  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F4))
#define ISR_INT_TARGET_STATUS_F5  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F5))
#define ISR_INT_TARGET_STATUS_F6  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F6))
#define ISR_INT_TARGET_STATUS_F7  (UNM_PCIX_PS_REG(PCIX_TARGET_STATUS_F7))

#define ISR_INT_TARGET_MASK	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK))
#define ISR_INT_TARGET_MASK_F1	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F1))
#define ISR_INT_TARGET_MASK_F2	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F2))
#define ISR_INT_TARGET_MASK_F3	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F3))
#define ISR_INT_TARGET_MASK_F4	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F4))
#define ISR_INT_TARGET_MASK_F5	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F5))
#define ISR_INT_TARGET_MASK_F6	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F6))
#define ISR_INT_TARGET_MASK_F7	  (UNM_PCIX_PS_REG(PCIX_TARGET_MASK_F7))

#define ISR_INT_VECTOR		  (UNM_PCIX_PS_REG(PCIX_INT_VECTOR))
#define ISR_INT_MASK		  (UNM_PCIX_PS_REG(PCIX_INT_MASK))
#define ISR_INT_STATE_REG	  (UNM_PCIX_PS_REG(PCIE_MISCCFG_RC))

#define	ISR_MSI_INT_TRIGGER(FUNC) (UNM_PCIX_PS_REG(PCIX_MSI_F(FUNC)))


#define	ISR_IS_LEGACY_INTR_IDLE(VAL)		(((VAL) & 0x300) == 0)
#define	ISR_IS_LEGACY_INTR_TRIGGERED(VAL)	(((VAL) & 0x300) == 0x200)

/*
 * PCI Interrupt Vector Values.
 */
#define	PCIX_INT_VECTOR_BIT_F0	0x0080
#define	PCIX_INT_VECTOR_BIT_F1	0x0100
#define	PCIX_INT_VECTOR_BIT_F2	0x0200
#define	PCIX_INT_VECTOR_BIT_F3	0x0400
#define	PCIX_INT_VECTOR_BIT_F4	0x0800
#define	PCIX_INT_VECTOR_BIT_F5	0x1000
#define	PCIX_INT_VECTOR_BIT_F6	0x2000
#define	PCIX_INT_VECTOR_BIT_F7	0x4000

struct nx_legacy_intr_set {
	__uint32_t	int_vec_bit;
	__uint32_t	tgt_status_reg;
	__uint32_t	tgt_mask_reg;
	__uint32_t	pci_int_reg;
};

#define	NX_LEGACY_INTR_CONFIG					\
{								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F0,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS,		\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(0) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F1,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F1,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F1,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(1) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F2,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F2,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F2,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(2) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F3,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F3,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F3,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(3) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F4,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F4,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F4,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(4) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F5,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F5,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F5,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(5) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F6,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F6,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F6,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(6) },	\
								\
	{int_vec_bit:		PCIX_INT_VECTOR_BIT_F7,		\
	 tgt_status_reg:	ISR_INT_TARGET_STATUS_F7,	\
	 tgt_mask_reg:		ISR_INT_TARGET_MASK_F7,		\
	 pci_int_reg:		ISR_MSI_INT_TRIGGER(7) },	\
}

#endif /* __NX_HW_REGS_H */



























