/*
 * QLogic iSCSI HBA Driver ioctl module
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef _QL4IM_DBG_H_ 
#define _QL4IM_DBG_H_ 

extern unsigned dbg_level;

/*
 * Driver debug definitions.
 */

#define QL_DBG_1	(1 << 0)
#define QL_DBG_2	(1 << 1)
#define QL_DBG_3	(1 << 2)
#define QL_DBG_4	(1 << 3)
#define QL_DBG_5	(1 << 4)
#define QL_DBG_6	(1 << 5)
#define QL_DBG_7	(1 << 6)
#define QL_DBG_8	(1 << 7)
#define QL_DBG_9	(1 << 8)
#define QL_DBG_10	(1 << 9)
#define QL_DBG_11	(1 << 10)
#define QL_DBG_12	(1 << 11)

#define QL_DEBUG

#ifdef QL_DEBUG

extern void dprt_hba_iscsi_portal(PEXT_HBA_ISCSI_PORTAL port);

extern void dprt_chip_info(PEXT_CHIP_INFO pcinfo);
extern void dprt_rw_flash(int rd, uint32_t offset, uint32_t len, uint32_t options);


#define DEBUG1(x)	if (dbg_level & QL_DBG_1) {x;}
#define DEBUG2(x)	if (dbg_level & QL_DBG_2) {x;}
#define DEBUG3(x)	if (dbg_level & QL_DBG_3) {x;}
#define DEBUG4(x)	if (dbg_level & QL_DBG_4) {x;}
#define DEBUG5(x)	if (dbg_level & QL_DBG_5) {x;}
#define DEBUG6(x)	if (dbg_level & QL_DBG_6) {x;}
#define DEBUG7(x)	if (dbg_level & QL_DBG_7) {x;}
#define DEBUG8(x)	if (dbg_level & QL_DBG_8) {x;}
#define DEBUG9(x)	if (dbg_level & QL_DBG_9) {x;}
#define DEBUG10(x)	if (dbg_level & QL_DBG_10) {x;}
#define DEBUG11(x)	if (dbg_level & QL_DBG_11) {x;}
#define DEBUG12(x)	if (dbg_level & QL_DBG_12) {x;}

#define ENTER(x) 	DEBUG12(printk("qisioctl: Entering %s()\n", x))
#define LEAVE(x) 	DEBUG12(printk("qisioctl: Leaving  %s()\n", x))

#define ENTER_IOCTL(x,n) 	\
	DEBUG12(printk("qisioctl(%d): Entering %s()\n", (int)n, x))

#define LEAVE_IOCTL(x,n) 	\
	DEBUG12(printk("qisioctl(%d): Leaving %s()\n", (int)n, x))

#define DUMP_HBA_ISCSI_PORTAL(port) if (dbg_level & QL_DBG_11) \
		dprt_hba_iscsi_portal(port);
#define DUMP_CHIP_INFO(pcinfo) if (dbg_level & QL_DBG_11) \
		dprt_chip_info(pcinfo);
#define DUMP_SET_FLASH(a,b,c) if (dbg_level & QL_DBG_11) \
		dprt_rw_flash(0, a, b, c);
#define DUMP_GET_FLASH(a,b) if (dbg_level & QL_DBG_11) \
		dprt_rw_flash(1, a, b, 0);

#else

#define DEBUG1(x)      
#define DEBUG2(x)      
#define DEBUG3(x)      
#define DEBUG4(x)      
#define DEBUG5(x)      
#define DEBUG6(x)      
#define DEBUG7(x)      
#define DEBUG8(x)      
#define DEBUG9(x)      
#define DEBUG10(x)      
#define DEBUG11(x)      
#define DEBUG12(x)      

#define ENTER(x)
#define LEAVE(x)

#define DUMP_HBA_ISCSI_PORTAL(port)
#define DUMP_CHIP_INFO(pcinfo)
#define DUMP_GET_FLASH(a,b,c)
#define DUMP_SET_FLASH(a,b) 

#endif

#endif /* #ifndef _QL4IM_DBG_H_ */
