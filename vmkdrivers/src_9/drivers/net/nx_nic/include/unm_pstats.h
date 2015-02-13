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
#ifndef _UNM_PSTATS_H_
#define _UNM_PSTATS_H_

#include "unm_compiler_defs.h"

/*
 * We use all unsigned longs. Linux will soon be so reliable that even these
 * will rapidly get too small 8-). Seriously consider the IpInReceives count
 * on the 20Gb/s + networks people expect in a few years time!
 */

#define NETXEN_NUM_PORTS        4
#define NETXEN_NUM_PEGS         4
#define NX_NUM_CTX		64

typedef struct {
	__uint64_t ip_in_receives;
	__uint64_t ip_in_hdr_errors;
	__uint64_t ip_in_addr_errors;
	__uint64_t ip_in_no_routes;
	__uint64_t ip_in_discards;
	__uint64_t ip_in_delivers;
	__uint64_t ip_out_requests;
	__uint64_t ip_out_discards;
	__uint64_t ip_out_no_routes;
	__uint64_t ip_reasm_timeout;
	__uint64_t ip_reasm_reqds;
	__uint64_t ip_reasm_oks;
	__uint64_t ip_reasm_fails;
	__uint64_t ip_frag_oks;
	__uint64_t ip_frag_fails;
	__uint64_t ip_frag_creates;
} nx_ip_mib_t;

#define TCP_RSVD	3
typedef struct {
	__uint64_t tcp_active_opens;
	__uint64_t tcp_passive_opens;
	__uint64_t tcp_attempt_fails;
	__uint64_t tcp_estab_resets;
	__uint64_t tcp_curr_estab;
	__uint64_t tcp_in_segs;
	__uint64_t tcp_out_segs;
	__uint64_t tcp_slow_out_segs;
	__uint64_t tcp_retrans_segs;
	__uint64_t tcp_in_errs;
	__uint64_t tcp_out_rsts;
	__uint64_t tcp_out_collapsed;
	__uint64_t tcp_time_wait_conns;
	__uint64_t rsvd[TCP_RSVD];
} nx_tcp_mib_t;

#define L2_RSVD	14
typedef struct {
	__uint64_t rx_bytes;
	__uint64_t tx_bytes;
	__uint64_t rsvd[L2_RSVD];
} nx_l2_stats_t;

#define TCP_EXT_RSVD	1
typedef struct {
	__uint64_t	lro_segs;
	__uint64_t	pure_acks;
	__uint64_t	delayed_acks;
	__uint64_t	delayed_ack_lost;
	__uint64_t	listen_drops;
	__uint64_t	reno_recovery;
	__uint64_t	sack_recovery;
	__uint64_t	sack_reneging;
	__uint64_t	fack_reorder;
	__uint64_t	sack_reorder;
	__uint64_t	reno_reorder;
	__uint64_t	ts_reorder;
	__uint64_t	full_undo;
	__uint64_t	partial_undo;
	__uint64_t	dsack_undo;
	__uint64_t	loss_undo;
	__uint64_t	tcp_loss;
	__uint64_t	lost_retransmit;
	__uint64_t	reno_failures;
	__uint64_t	sack_failures;
	__uint64_t	loss_failures;
	__uint64_t	fast_retrans;
	__uint64_t	forward_retrans;
	__uint64_t	slow_start_retrans;
	__uint64_t	reno_recovery_fail;
	__uint64_t	sack_recovery_fail;
	__uint64_t	dsack_old_sent;
	__uint64_t	dsack_ofo_sent;
	__uint64_t	dsack_rcv;
	__uint64_t	dsack_ofo_rcv;
	__uint64_t	tcp_abort_failed;
	__uint64_t	memory_pressure;
	__uint64_t	tcp_hp_miss;
	__uint64_t	tcp_hp_miss_acks;
	__uint64_t	prune_called;
	__uint64_t	rcv_pruned;
	__uint64_t	ofo_pruned;
	__uint64_t	rcv_collapse;
	__uint64_t	paws_active_rejected;
	__uint64_t	paws_estab_rejected;
	__uint64_t	tcp_timeouts;
	__uint64_t	abort_on_syn;
	__uint64_t	abort_on_data;
	__uint64_t	abort_on_close;
	__uint64_t	abort_on_memory;
	__uint64_t	abort_on_timeout;
	__uint64_t	abort_on_linger;
	__uint64_t	rsvd[TCP_EXT_RSVD];
} nx_tcp_ext_stats_t;

typedef struct {
	__uint64_t	rsvd[16];
} nx_toe_debug_stats_t;

typedef struct {
	__uint64_t	rsvd[16];
} nx_nic_debug_stats_t;

typedef struct {
	__uint64_t	rsvd[128];
} nx_extended_stats_t;

typedef struct {
	nx_l2_stats_t		l2_stats;
	nx_ip_mib_t		ip_stats;
	nx_tcp_mib_t		tcp_stats;
	nx_tcp_ext_stats_t	tcp_ext_stats;	/* Slow path stats */
	nx_toe_debug_stats_t	toe_debug_stats;
	nx_nic_debug_stats_t	nic_debug_stats;
	nx_extended_stats_t	extended_stats;
} nx_peg_stats_t;  /* Has to be 512 bytes aligned */

typedef struct {
	nx_peg_stats_t		peg[NETXEN_NUM_PEGS];
} nx_ctx_stats_t;

#endif /* _UNM_PSTATS_H_ */

