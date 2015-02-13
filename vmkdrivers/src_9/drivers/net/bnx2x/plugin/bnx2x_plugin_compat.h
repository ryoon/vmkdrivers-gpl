/* bnx2x_plugin_compat.h: Broadcom bnx2x NPA plugin
 *
 * Copyright 2009-2011 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by: Shmulik Ravid
 *
 */
#ifndef _BNX2X_PLUGIN_COMPAT_H
#define _BNX2X_PLUGIN_COMPAT_H

/* logging */
#define ShellLog(_n, _fmt, ...)        \
    ps->shellApi.log((_n) + 1, "%s: " _fmt, __FUNCTION__, ##__VA_ARGS__)

#ifdef PLUGIN_DP
#define DP(_n, _fmt, ...)          \
    ps->shellApi.log((_n) + 1, "%s: " _fmt, __FUNCTION__, ##__VA_ARGS__)
#else
#define DP(_n, _fmt, ...)
#endif

/* HW write */
static inline void
hw_writel(Plugin_State *ps, uint32 val, uint32 offset)
{
    *(volatile uint32*)((uint8*)ps->memioAddr + (offset)) = (val);
}

/* DOORBELL */
#define DOORBELL(ps, id, val) \
    do { \
	hw_writel((ps), (u32)(val), VF_BAR_DOORBELL_OFFSET + \
	    (BAR_DOORBELL_STRIDE * (id)) + BAR_DOORBELL_TYPE_OFF); \
    } while (0)

/* RX PRODUCERS */
#define PROD_UPDATE(ps, qid, rx_prods) \
    do { \
	int i; \
	uint32 start = PXP_VF_ADDR_USDM_QUEUES_START + \
	    (qid) * sizeof(struct ustorm_queue_zone_data); \
	    for (i = 0; i < sizeof((rx_prods))/4; i++) \
		    hw_writel((ps), ((u32 *)&(rx_prods))[i], start + i*4); \
    } while (0)

/* INTERRUPT ACK */
#define IGU_ACK(ps, addr, ack_data) \
    do { \
	hw_writel((ps), (u32)(ack_data), BAR_IGU + (addr)); \
    } while (0)

#endif /* bnx2x_plugin_compat.h */
