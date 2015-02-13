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
#ifdef UNM_NIC_SNMP

#include <asm/types.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "unm_nic.h"
#include "nic_phan_reg.h"

#include "nxhal_nic_interface.h"
#include "nxhal.h"
#include "nxhal_v34.h"

#ifndef UNM_NIC_SNMP_ENUMS_H
#define UNM_NIC_SNMP_ENUMS_H

#define HEADER_STRING_LINE	"HEADER: SET/GET_REQUEST MIB_OID"

#define MAU_STRING_LINE		"\
MAU: TYPE STATUS MEDIA_AVAILABLE AVAILABLE_STATE_EXITS JABBER_STATE \
JABBER_STATE_ENTERED DEFAULT_TYPE AUTO_NEG_SUPP JACK_TYPE MAU_TYPE_LIST \
FALSE_CARRIER"

#define AUTO_NEG_STRING_LINE	"\
AUTO-NEGOTIATION: ADMIN_STATUS REMOTE_SIGNALING CURR_CONFIG RESTART \
CAPABILITY CAP_ADVERTISED CAP_RECEIVED FAULT_ADVERTISED FAULT_RECEIVED"

#define DOT3STATS_STRING_LINE	"\
ETHERNET: DUPLEX_STATUS RATE_CONTROL_ABILITY RATE_CONTROL_STATUS \
CONTROL_FUNCTION UNKNOWN_OPCODES ALIGNMENT_ERRORS FCS_ERRORS \
MAC_TRANSMIT_ERRORS FRAME_TOO_LONG MAC_RECEIVE_ERRORS SYMBOL_ERRORS"

#define PAUSE_TABLE_STRING_LINE	"\
PAUSE_TABLE: ADMIN_MODE OPERATIONAL_MODE FRAME_RECEIVED FRAME_TRANSMIT"

#define TEMP_STRING_LINE	"TEMP: TEMPERATURE_STATE"
	
#define HEADER_STATS_LINE	"HEADER: %u 0.0"
#define MAU_STATS_LINE		"MAU: %u %u %u %u %u %u %u %u %u %llu %llu"
#define AUTO_NEG_STATS_LINE	"\
AUTO-NEGOTIATION: %u %u %u %u %u %u %u %u %u"

#define DOT3STATS_STATS_LINE	"\
ETHERNET: %u %u %u %u %llu %llu %llu %llu %llu %llu %llu"

#define PAUSE_TABLE_STATS_LINE	"PAUSE_TABLE: %u %u %llu %llu"
#define TEMP_STATS_LINE		"TEMP: %u"

#define SHIFT_OCTECT_BIT(DIGIT) ((1L<<((DIGIT/8)*8))<<(7-(DIGIT<8?DIGIT:DIGIT%8)))
struct header_st{
	u32 set_get;
	char mib_oid[10];
};
struct dot3stats_st{
	u32 duplex_status;
	u32 rate_control_ability;
	u32 rate_control_status;
	u32 control_function;
	u64 unknow_opcodes;
	u64 alignment_errors;
	u64 fcs_errors;
	u64 mac_transmit_erros;
	u64 frame_too_long;
	u64 mac_receive_errors;
	u64 symbol_errors;
};
struct mau_st{
	u32 type;
	u32 status;
	u32 media_available;
	u32 available_state_exits;
	u32 jabber_state;
	u32 jabber_state_entered;
	u32 default_type;
	u32 auto_neg_supp;
	u32 jack_type;
	u64 mau_type_list;
	u64 false_carrier;
};
struct auto_neg_st{
	u32 admin_status;
	u32 remote_signaling;
	u32 curr_config;
	u32 restart;
	u32 capability;
	u32 cap_advertised;
	u32 cap_received;
	u32 fault_advertised;
	u32 fault_received;
};
struct pause_table_st{
	u32 admin_mode;
	u32 operational_mode;
	u64 frame_received;
	u64 frame_transmit;
};
struct temp_st{
	u32 temp_state;
};
/*
 * enums for column dot3StatsDuplexStatus
 */
#define DOT3STATSDUPLEXSTATUS_UNKNOWN		1
#define DOT3STATSDUPLEXSTATUS_HALFDUPLEX		2
#define DOT3STATSDUPLEXSTATUS_FULLDUPLEX		3

/*
 * enums for column dot3StatsRateControlAbility
 */
#define DOT3STATSRATECONTROLABILITY_TRUE		1
#define DOT3STATSRATECONTROLABILITY_FALSE		2

/*
 * enums for column dot3StatsRateControlStatus
 */
#define DOT3STATSRATECONTROLSTATUS_RATECONTROLOFF		1
#define DOT3STATSRATECONTROLSTATUS_RATECONTROLON		2
#define DOT3STATSRATECONTROLSTATUS_UNKNOWN		3

/*
 * enums for column dot3PauseAdminMode
 */
#define DOT3PAUSEMODE_DISABLED		1
#define DOT3PAUSEMODE_ENABLEDXMIT		2
#define DOT3PAUSEMODE_ENABLEDRCV		3
#define DOT3PAUSEMODE_ENABLEDXMITANDRCV		4

/*
 * enums for column dot3ControlFunctionsSupported
 */
#define DOT3CONTROLFUNCTIONSSUPPORTED_PAUSE		0

/*
 * enums for column ifJackType
 */
#define IFJACKTYPE_OTHER		1
#define IFJACKTYPE_RJ45		2
#define IFJACKTYPE_RJ45S		3
#define IFJACKTYPE_DB9		4
#define IFJACKTYPE_BNC		5
#define IFJACKTYPE_FAUI		6
#define IFJACKTYPE_MAUI		7
#define IFJACKTYPE_FIBERSC		8
#define IFJACKTYPE_FIBERMIC		9
#define IFJACKTYPE_FIBERST		10
#define IFJACKTYPE_TELCO		11
#define IFJACKTYPE_MTRJ		12
#define IFJACKTYPE_HSSDC		13
#define IFJACKTYPE_FIBERLC		14
#define IFJACKTYPE_CX4		15

/*
 * enums for column ifMauAutoNegAdminStatus
 */
#define IFMAUAUTONEGADMINSTATUS_ENABLED		1
#define IFMAUAUTONEGADMINSTATUS_DISABLED		2

/*
 * enums for column ifMauAutoNegRemoteSignaling
 */
#define IFMAUAUTONEGREMOTESIGNALING_DETECTED		1
#define IFMAUAUTONEGREMOTESIGNALING_NOTDETECTED		2

/*
 * enums for column ifMauAutoNegConfig
 */
#define IFMAUAUTONEGCONFIG_OTHER		1
#define IFMAUAUTONEGCONFIG_CONFIGURING		2
#define IFMAUAUTONEGCONFIG_COMPLETE		3
#define IFMAUAUTONEGCONFIG_DISABLED		4
#define IFMAUAUTONEGCONFIG_PARALLELDETECTFAIL		5

/*
 * enums for column ifMauAutoNegRestart
 */
#define IFMAUAUTONEGRESTART_RESTART		1
#define IFMAUAUTONEGRESTART_NORESTART		2

/*
 * enums for column ifMauAutoNegCapabilityBits
 */
#define IFMAUBITS_BOTHER		0
#define IFMAUBITS_B10BASET		1
#define IFMAUBITS_B10BASETFD		2
#define IFMAUBITS_B100BASET4		3
#define IFMAUBITS_B100BASETX		4
#define IFMAUBITS_B100BASETXFD		5
#define IFMAUBITS_B100BASET2		6
#define IFMAUBITS_B100BASET2FD		7
#define IFMAUBITS_BFDXPAUSE		8
#define IFMAUBITS_BFDXAPAUSE		9
#define IFMAUBITS_BFDXSPAUSE		10
#define IFMAUBITS_BFDXBPAUSE		11
#define IFMAUBITS_B1000BASEX		12
#define IFMAUBITS_B1000BASEXFD		13
#define IFMAUBITS_B1000BASET		14
#define IFMAUBITS_B1000BASETFD		15

/*
 * enums for column ifMauAutoNegRemoteFaultAdvertised
 */
#define IFMAUAUTONEGREMOTEFAULTADVERTISED_NOERROR		1
#define IFMAUAUTONEGREMOTEFAULTADVERTISED_OFFLINE		2
#define IFMAUAUTONEGREMOTEFAULTADVERTISED_LINKFAILURE		3
#define IFMAUAUTONEGREMOTEFAULTADVERTISED_AUTONEGERROR		4

/*
 * enums for column ifMauAutoNegRemoteFaultReceived
 */
#define IFMAUAUTONEGREMOTEFAULTRECEIVED_NOERROR		1
#define IFMAUAUTONEGREMOTEFAULTRECEIVED_OFFLINE		2
#define IFMAUAUTONEGREMOTEFAULTRECEIVED_LINKFAILURE		3
#define IFMAUAUTONEGREMOTEFAULTRECEIVED_AUTONEGERROR		4

/*
 * enums for column ifMauStatus
 */
#define IFMAUSTATUS_OTHER		1
#define IFMAUSTATUS_UNKNOWN		2
#define IFMAUSTATUS_OPERATIONAL		3
#define IFMAUSTATUS_STANDBY		4
#define IFMAUSTATUS_SHUTDOWN		5
#define IFMAUSTATUS_RESET		6

/*
 * enums for column ifMauMediaAvailable
 */
#define IFMAUMEDIAAVAILABLE_OTHER		1
#define IFMAUMEDIAAVAILABLE_UNKNOWN		2
#define IFMAUMEDIAAVAILABLE_AVAILABLE		3
#define IFMAUMEDIAAVAILABLE_NOTAVAILABLE		4
#define IFMAUMEDIAAVAILABLE_REMOTEFAULT		5
#define IFMAUMEDIAAVAILABLE_INVALIDSIGNAL		6
#define IFMAUMEDIAAVAILABLE_REMOTEJABBER		7
#define IFMAUMEDIAAVAILABLE_REMOTELINKLOSS		8
#define IFMAUMEDIAAVAILABLE_REMOTETEST		9
#define IFMAUMEDIAAVAILABLE_OFFLINE		10
#define IFMAUMEDIAAVAILABLE_AUTONEGERROR		11
#define IFMAUMEDIAAVAILABLE_PMDLINKFAULT		12
#define IFMAUMEDIAAVAILABLE_WISFRAMELOSS		13
#define IFMAUMEDIAAVAILABLE_WISSIGNALLOSS		14
#define IFMAUMEDIAAVAILABLE_PCSLINKFAULT		15
#define IFMAUMEDIAAVAILABLE_EXCESSIVEBER		16
#define IFMAUMEDIAAVAILABLE_DXSLINKFAULT		17
#define IFMAUMEDIAAVAILABLE_PXSLINKFAULT		18
#define IFMAUMEDIAAVAILABLE_AVAILABLEREDUCED		19
#define IFMAUMEDIAAVAILABLE_READY		20

/*
 * enums for column ifMauJabberState
 */
#define IFMAUJABBERSTATE_OTHER		1
#define IFMAUJABBERSTATE_UNKNOWN		2
#define IFMAUJABBERSTATE_NOJABBER		3
#define IFMAUJABBERSTATE_JABBERING		4

/*
 * enums for column ifMauAutoNegSupported
 */
#define IFMAUAUTONEGSUPPORTED_TRUE		1
#define IFMAUAUTONEGSUPPORTED_FALSE		2

/*
 * enums for column ifMauTypeListBits
 */
#define TYPELISTBITS_BOTHER		0
#define TYPELISTBITS_BAUI		1
#define TYPELISTBITS_B10BASE5		2
#define TYPELISTBITS_BFOIRL		3
#define TYPELISTBITS_B10BASE2		4
#define TYPELISTBITS_B10BASET		5
#define TYPELISTBITS_B10BASEFP		6
#define TYPELISTBITS_B10BASEFB		7
#define TYPELISTBITS_B10BASEFL		8
#define TYPELISTBITS_B10BROAD36		9
#define TYPELISTBITS_B10BASETHD		10
#define TYPELISTBITS_B10BASETFD		11
#define TYPELISTBITS_B10BASEFLHD		12
#define TYPELISTBITS_B10BASEFLFD		13
#define TYPELISTBITS_B100BASET4		14
#define TYPELISTBITS_B100BASETXHD		15
#define TYPELISTBITS_B100BASETXFD		16
#define TYPELISTBITS_B100BASEFXHD		17
#define TYPELISTBITS_B100BASEFXFD		18
#define TYPELISTBITS_B100BASET2HD		19
#define TYPELISTBITS_B100BASET2FD		20
#define TYPELISTBITS_B1000BASEXHD		21
#define TYPELISTBITS_B1000BASEXFD		22
#define TYPELISTBITS_B1000BASELXHD		23
#define TYPELISTBITS_B1000BASELXFD		24
#define TYPELISTBITS_B1000BASESXHD		25
#define TYPELISTBITS_B1000BASESXFD		26
#define TYPELISTBITS_B1000BASECXHD		27
#define TYPELISTBITS_B1000BASECXFD		28
#define TYPELISTBITS_B1000BASETHD		29
#define TYPELISTBITS_B1000BASETFD		30
#define TYPELISTBITS_B10GBASEX		31
#define TYPELISTBITS_B10GBASELX4		32
#define TYPELISTBITS_B10GBASER		33
#define TYPELISTBITS_B10GBASEER		34
#define TYPELISTBITS_B10GBASELR		35
#define TYPELISTBITS_B10GBASESR		36
#define TYPELISTBITS_B10GBASEW		37
#define TYPELISTBITS_B10GBASEEW		38
#define TYPELISTBITS_B10GBASELW		39
#define TYPELISTBITS_B10GBASESW		40
#define TYPELISTBITS_B10GBASECX4		41
#define TYPELISTBITS_B2BASETL		42
#define TYPELISTBITS_B10PASSTS		43
#define TYPELISTBITS_B100BASEBX10D		44
#define TYPELISTBITS_B100BASEBX10U		45
#define TYPELISTBITS_B100BASELX10		46
#define TYPELISTBITS_B1000BASEBX10D		47
#define TYPELISTBITS_B1000BASEBX10U		48
#define TYPELISTBITS_B1000BASELX10		49
#define TYPELISTBITS_B1000BASEPX10D		50
#define TYPELISTBITS_B1000BASEPX10U		51
#define TYPELISTBITS_B1000BASEPX20D		52
#define TYPELISTBITS_B1000BASEPX20U		53


#endif			  /* UNM_NIC_SNMP_ENUMS_H */

static void nx_get_snmp_ether_stats(struct unm_adapter_s *adapter,
				struct unm_nic_snmp_ether_stats *snmp_stats){
	
	nic_request_t   req;
	get_stats_request_t     *stats_req;
	nx_os_wait_event_t swait;
	int	     rv = 0;
	
	memset(&req, 0, sizeof(req));
	/* Copy old value */
	memcpy(snmp_stats, adapter->snmp_stats_dma.addr,
				sizeof(struct unm_nic_snmp_ether_stats));
	memset(adapter->snmp_stats_dma.addr, 0, 
				sizeof(struct unm_nic_snmp_ether_stats));

	req.opcode = NX_NIC_HOST_REQUEST;
	req.qmsg_type = UNM_MSGTYPE_NIC_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_GET_SNMP_STATS;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	stats_req = (get_stats_request_t *)&req.body;
	stats_req->ring_ctx = adapter->portnum;
	
	stats_req->dma_to_addr = ((__uint64_t)(adapter->snmp_stats_dma.phys_addr));
	stats_req->dma_size = ((__uint32_t)(sizeof(struct unm_nic_snmp_ether_stats)));

	/* Here req.body.cmn.req_hdr.comp_i will be set */
	if(nx_os_event_wait_setup(adapter, &req, NULL, &swait)) {
		nx_nic_print3(adapter, "%s: nx os event setup failed\n",__FUNCTION__);
		return ;
	}
	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);

	if(rv) {
		nx_nic_print3(adapter, "%s: Sending snmp stats request to FW failed: %d\n", 
				__FUNCTION__, rv);
		return ;
	}
	if(nx_os_event_wait(adapter, &swait, 500000)) {
		nx_nic_print3(adapter, "%s: nx os event wait failed\n",__FUNCTION__);
		return ;
	}
	memcpy(snmp_stats, adapter->snmp_stats_dma.addr,
					sizeof(struct unm_nic_snmp_ether_stats));
	return ;
}
/* These header is for agent. To send information to driver about type of
 * request(set/get) and oid which to set/get.
 */
static void read_header_stats(struct header_st *head){
	
	/* Read Proc is always get request.*/
	head->set_get = 0;
	memset(head->mib_oid, 0, sizeof(head->mib_oid));
}
static void read_mau_stats(struct unm_adapter_s *adapter, struct mau_st *mau_stats){
	
	native_t port = adapter->physical_port;
	u32 port_mode = 0;
	unm_board_info_t	*boardinfo = &adapter->ahw.boardcfg;
	unm_niu_xg_mac_config_0_t mac_cfg;
	unm_niu_gb_mac_config_0_t mac_gbe;

	if(adapter->ahw.board_type == UNM_NIC_XGBE){
		/* Status */
		*(u32 *)&mac_cfg = NXRD32(adapter,
				UNM_NIU_XGE_CONFIG_0 + (0x10000*(adapter->physical_port)));

		if(!mac_cfg.soft_reset)
			if(mac_cfg.tx_enable && mac_cfg.rx_enable)
				mau_stats->status = IFMAUSTATUS_OPERATIONAL;
			else
				mau_stats->status = IFMAUSTATUS_STANDBY;
		else
			mau_stats->status = IFMAUSTATUS_SHUTDOWN;

		/* Type List Bits */
		port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);

		if(port_mode == UNM_PORT_MODE_802_3_AP){
			mau_stats->mau_type_list =
				SHIFT_OCTECT_BIT(TYPELISTBITS_B1000BASEXFD);
			mau_stats->default_type = TYPELISTBITS_B1000BASEXFD;
			mau_stats->type = TYPELISTBITS_B1000BASEXFD;
		}
		else{
			mau_stats->mau_type_list =
				SHIFT_OCTECT_BIT(TYPELISTBITS_B10GBASEX);
			mau_stats->default_type = TYPELISTBITS_B10GBASEX;
			mau_stats->type = TYPELISTBITS_B10GBASEX;
		}

	}else if( (adapter->ahw.board_type == UNM_NIC_GBE) && 
			(port >= 0) && (port < UNM_NIU_MAX_GBE_PORTS) ){
		/* Status */
		*(u32 *)&mac_gbe = NXRD32(adapter,
					UNM_NIU_GB_MAC_CONFIG_0(port));

		if(!mac_gbe.soft_reset)
			if(mac_gbe.tx_enable && mac_gbe.rx_enable)
				mau_stats->status = IFMAUSTATUS_OPERATIONAL;
			else
				mau_stats->status = IFMAUSTATUS_STANDBY;
		else
			mau_stats->status = IFMAUSTATUS_SHUTDOWN;

		/*Type List Bits*/
		mau_stats->mau_type_list = (
				SHIFT_OCTECT_BIT(TYPELISTBITS_B10BASETHD) |
				SHIFT_OCTECT_BIT(TYPELISTBITS_B10BASETFD) |
				SHIFT_OCTECT_BIT(TYPELISTBITS_B100BASETXHD)|
				SHIFT_OCTECT_BIT(TYPELISTBITS_B100BASETXFD)|
				SHIFT_OCTECT_BIT(TYPELISTBITS_B1000BASEXHD)|
				SHIFT_OCTECT_BIT(TYPELISTBITS_B1000BASEXFD)
				);

		/* If Auto-Negotiation enable, these value can change*/
		mau_stats->default_type = TYPELISTBITS_B1000BASEXFD;
		mau_stats->type = TYPELISTBITS_B1000BASEXFD;
	}

	mau_stats->media_available = IFMAUMEDIAAVAILABLE_AVAILABLE;
	/* Jabber not suported */
	mau_stats->jabber_state = IFMAUJABBERSTATE_UNKNOWN;
	/* Default case */
	mau_stats->auto_neg_supp = IFMAUAUTONEGSUPPORTED_FALSE;
	mau_stats->jack_type = IFJACKTYPE_OTHER;

	switch ((unm_brdtype_t)boardinfo->board_type) {
		case UNM_BRDTYPE_P2_SB35_4G:
		case UNM_BRDTYPE_P2_SB31_2G:
		case UNM_BRDTYPE_P3_REF_QG:
		case UNM_BRDTYPE_P3_4_GB:
		case UNM_BRDTYPE_P3_4_GB_MM:
			mau_stats->jack_type = IFJACKTYPE_RJ45;
			mau_stats->auto_neg_supp = IFMAUAUTONEGSUPPORTED_TRUE;
			break;
		case UNM_BRDTYPE_P3_10000_BASE_T:
			mau_stats->auto_neg_supp = IFMAUAUTONEGSUPPORTED_TRUE;
			break;
	    	case UNM_BRDTYPE_P3_10G_CX4:
		case UNM_BRDTYPE_P3_10G_CX4_LP:
			mau_stats->auto_neg_supp = adapter->link_autoneg ? 
				IFMAUAUTONEGSUPPORTED_TRUE : IFMAUAUTONEGSUPPORTED_FALSE;
			mau_stats->jack_type = IFJACKTYPE_CX4;
			break;
		case UNM_BRDTYPE_P2_SB31_10G_CX4:
			mau_stats->jack_type = IFJACKTYPE_CX4;
			break;
		case UNM_BRDTYPE_P2_SB31_10G_HMEZ:
		case UNM_BRDTYPE_P2_SB31_10G_IMEZ:
		case UNM_BRDTYPE_P3_IMEZ:
		case UNM_BRDTYPE_P3_XG_LOM:
		case UNM_BRDTYPE_P3_HMEZ:
		case UNM_BRDTYPE_P2_SB31_10G:
			break;
		case UNM_BRDTYPE_P3_10G_SFP_PLUS:
		case UNM_BRDTYPE_P3_10G_XFP:
			mau_stats->auto_neg_supp = IFMAUAUTONEGSUPPORTED_FALSE;
			mau_stats->jack_type = IFJACKTYPE_FIBERLC;
			break;
		default:
			break;
	}
}
/* Auto negotiation not supported in XGB card*/
static void read_auto_neg_stats(struct unm_adapter_s *adapter,
				struct auto_neg_st * auto_neg_stats){

	u32 port_mode = 0;

	auto_neg_stats->admin_status = IFMAUAUTONEGADMINSTATUS_DISABLED;
	auto_neg_stats->remote_signaling =
				IFMAUAUTONEGREMOTESIGNALING_NOTDETECTED;
	auto_neg_stats->curr_config = IFMAUAUTONEGCONFIG_DISABLED;
	auto_neg_stats->restart = IFMAUAUTONEGRESTART_NORESTART;

	if(adapter->ahw.board_type == UNM_NIC_GBE) {
		auto_neg_stats->capability = (
			SHIFT_OCTECT_BIT(IFMAUBITS_B10BASET) |
			SHIFT_OCTECT_BIT(IFMAUBITS_B10BASETFD)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B100BASETX)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B100BASETXFD)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASET)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASETFD) );
		auto_neg_stats->cap_advertised = (
			SHIFT_OCTECT_BIT(IFMAUBITS_B100BASETX)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B100BASETXFD)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASET)|
			SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASETFD) );

	}else if(adapter->ahw.board_type == UNM_NIC_XGBE) {

		port_mode = NXRD32(adapter,
						UNM_PORT_MODE_ADDR);

		if(port_mode == UNM_PORT_MODE_802_3_AP){
			auto_neg_stats->capability =
				SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASETFD);
			auto_neg_stats->cap_advertised =
				SHIFT_OCTECT_BIT(IFMAUBITS_B1000BASETFD);
		}else{
			/* SUPPORTED 10000baseT_Full, It's not in the bit list */
			auto_neg_stats->capability =
				SHIFT_OCTECT_BIT(IFMAUBITS_BOTHER);
			auto_neg_stats->cap_advertised =
				SHIFT_OCTECT_BIT(IFMAUBITS_BOTHER);
		}
	}
	
	auto_neg_stats->cap_received = IFMAUBITS_BOTHER;
	auto_neg_stats->fault_advertised = IFMAUAUTONEGREMOTEFAULTADVERTISED_NOERROR;
	auto_neg_stats->fault_received = IFMAUAUTONEGREMOTEFAULTRECEIVED_NOERROR;
}
static void read_dot3stats(struct unm_adapter_s *adapter, struct dot3stats_st * dot3stats){

	/* XGB always Full duplex */
	dot3stats->duplex_status = DOT3STATSDUPLEXSTATUS_FULLDUPLEX;

	/* Rate control Ability Feature not supported */
	dot3stats->rate_control_ability = DOT3STATSRATECONTROLABILITY_FALSE;
	dot3stats->rate_control_status = DOT3STATSRATECONTROLSTATUS_UNKNOWN;
	/* default value is For Pause function is 0 */
	dot3stats->control_function = SHIFT_OCTECT_BIT(DOT3CONTROLFUNCTIONSSUPPORTED_PAUSE);
}
static void read_pause_stats(struct unm_adapter_s *adapter, struct pause_table_st * pause_stats){

	int temp = 0, pause_tx = 0, pause_rx = 0;

	if(adapter->ahw.board_type == UNM_NIC_XGBE) {
		/* Rcv mode is always enable for XGB card */	
		pause_rx = 1;
		if ( unm_niu_xg_get_tx_flow_ctl(adapter, &temp) == 0 ) 
			pause_tx = temp;
				
	}else if( adapter->ahw.board_type == UNM_NIC_GBE ) { 

		if (unm_niu_gbe_get_rx_flow_ctl(adapter, &temp) == 0)
			pause_rx = temp;
		if (unm_niu_gbe_get_tx_flow_ctl(adapter, &temp) == 0)
			pause_tx = temp;
	}
		
	pause_stats->operational_mode = DOT3PAUSEMODE_DISABLED; 

	if( pause_tx )
		pause_stats->operational_mode = DOT3PAUSEMODE_ENABLEDXMIT;
	if( pause_rx )
		pause_stats->operational_mode = DOT3PAUSEMODE_ENABLEDRCV;
	if( pause_tx && pause_rx )
		pause_stats->operational_mode = DOT3PAUSEMODE_ENABLEDXMITANDRCV; 

	/* 
	 * Fix ME. Support Configuration of pause parameter.
	 */
	pause_stats->admin_mode = pause_stats->operational_mode;
}
static void read_temp_stats(struct unm_adapter_s *adapter, struct temp_st *temp_stats){

	temp_stats->temp_state = adapter->temp;
}
/* Currently this read_proc will write 768 bytes into buffer, 
 * need to take care if it goes beyond 1024(count) 
 */
int
unm_nic_snmp_ether_read_proc(char *buf, char **start, off_t offset, int count,
				int *eof, void *data){
	struct net_device *netdev = (struct net_device *)data;
	int	     len = 0;
	struct header_st	header;
	struct dot3stats_st	dot3stats;
	struct mau_st		mau_stats;
	struct auto_neg_st	auto_neg_stats;
	struct pause_table_st	pause_stats;
	struct temp_st		temp_stats;
	struct unm_nic_snmp_ether_stats snmp_stats;
	struct unm_adapter_s    *adapter;

	if(offset > 0 ){
		*eof = 1;
		return len;
	}
	if(netdev == NULL || ((struct unm_adapter_s*)netdev_priv(netdev))->state != PORT_UP) {
		len = sprintf(buf, "No Statistics available now. Device is"
					"NULL\n");
		*eof = 1;
		return len;
	}
	adapter = (struct unm_adapter_s*)netdev_priv(netdev);
	
	memset(&snmp_stats, 0, sizeof(snmp_stats));
	memset(&header, 0, sizeof(struct header_st));
	memset(&dot3stats, 0, sizeof(struct dot3stats_st));
	memset(&mau_stats, 0, sizeof(struct mau_st));
	memset(&auto_neg_stats, 0, sizeof(struct auto_neg_st));
	memset(&pause_stats, 0, sizeof(struct pause_table_st));
	memset(&temp_stats, 0, sizeof(struct temp_st));
	/*This below #if is to test Trap,
	*/
#if 0
	unm_nic_send_snmp_trap(NX_TEMP_PANIC);
#endif
	
	if (!adapter->fw_v34) {
		nx_get_snmp_ether_stats(adapter, &snmp_stats);
	}

	len += sprintf(buf+len, HEADER_STRING_LINE"\n");
	read_header_stats(&header);
	len += sprintf(buf+len, HEADER_STATS_LINE"\n",
		header.set_get);
	//	header.mib_oid);

	read_mau_stats(adapter, &mau_stats);
	len += sprintf(buf+len, MAU_STRING_LINE"\n");
	len += sprintf(buf+len, MAU_STATS_LINE"\n",
		mau_stats.type,
		mau_stats.status,
		mau_stats.media_available,
		(u32)snmp_stats.available_mau_state_exits,
		mau_stats.jabber_state,
		(u32)snmp_stats.jabber_state_entered,
		mau_stats.default_type,
		mau_stats.auto_neg_supp,
		mau_stats.jack_type,
		mau_stats.mau_type_list,
		snmp_stats.false_carrier);

	read_auto_neg_stats(adapter, &auto_neg_stats);
	len += sprintf(buf+len, AUTO_NEG_STRING_LINE"\n");
	len += sprintf(buf+len, AUTO_NEG_STATS_LINE"\n",
		auto_neg_stats.admin_status,
		auto_neg_stats.remote_signaling,
		auto_neg_stats.curr_config,
		auto_neg_stats.restart,
		auto_neg_stats.capability,
		auto_neg_stats.cap_advertised,
		auto_neg_stats.cap_received,
		auto_neg_stats.fault_advertised,
		auto_neg_stats.fault_received);

	read_dot3stats(adapter, &dot3stats);
	len += sprintf(buf+len, DOT3STATS_STRING_LINE"\n");
	len += sprintf(buf+len, DOT3STATS_STATS_LINE"\n",
		dot3stats.duplex_status,
		dot3stats.rate_control_ability,
		dot3stats.rate_control_status,
		dot3stats.control_function,
		snmp_stats.unknow_opcodes,
		snmp_stats.alignment_errors,
		snmp_stats.fcs_errors,
		snmp_stats.mac_transmit_erros,
		snmp_stats.frame_too_long,
		snmp_stats.mac_receive_errors,
		snmp_stats.symbol_errors);

	read_pause_stats(adapter, &pause_stats);
	len += sprintf(buf+len, PAUSE_TABLE_STRING_LINE"\n");
	len += sprintf(buf+len, PAUSE_TABLE_STATS_LINE"\n",
		pause_stats.admin_mode,
		pause_stats.operational_mode,
		snmp_stats.pause_frame_received,
		snmp_stats.pause_frame_transmit);

	read_temp_stats(adapter, &temp_stats);
	len += sprintf(buf+len, TEMP_STRING_LINE"\n");
	len += sprintf(buf+len, TEMP_STATS_LINE"\n",
		temp_stats.temp_state);

	if(len > count)
		nx_nic_print3(adapter, "%s: Total buffer length exceeded %d"
				"page size\n", __FUNCTION__, count);
		
	*eof = 1;
	return len;
}
#ifdef UNM_NIC_SNMP_TRAP
/* NETLINK number should be less than 32 and greater than 16.
 * This is protocol number. 0, is unspecified protocol.
 */
#define NETLINK_TEST  17
#define MAX_PAYLOAD 1024

#define TRAP_HEADER_STR		"NETXEN:"
#define TRAP_HEADER_STR_LENGTH  7
#define TRAP_HIGH_TEMP_STR      "HIGH_TEMPERATURE"
#define TRAP_OVER_TEMP_STR      "OVER_TEMPERATURE"
#define TRAP_JABBER_STR		"JABBER"

unsigned int temperature_user_pid = 0;
void set_temperature_user_pid(unsigned int user_pid){
	temperature_user_pid = user_pid;
}
void unm_nic_send_snmp_trap(unsigned char temp){
	
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nl_hdr;
	u32 pid = temperature_user_pid;
	struct sock *nl_sk = NULL;

	if(!pid || !(temp == NX_TEMP_WARN || temp == NX_TEMP_PANIC))
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	netlink_kernel_create(&init_net, NETLINK_TEST, 1, NULL, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	netlink_kernel_create(NETLINK_TEST, 1, NULL, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	netlink_kernel_create(NETLINK_TEST, 1, NULL, THIS_MODULE);
#else
	nl_sk = netlink_kernel_create(NETLINK_TEST, NULL);
#endif
	skb = alloc_skb(MAX_PAYLOAD, GFP_KERNEL);
	skb_put(skb, MAX_PAYLOAD);
	
	nl_hdr = (struct nlmsghdr *)skb->data;
	nl_hdr->nlmsg_len = MAX_PAYLOAD;
	nl_hdr->nlmsg_pid = 0;
	nl_hdr->nlmsg_flags = 0;

	switch(temp){

		case NX_TEMP_WARN:
			strcpy(NLMSG_DATA(nl_hdr), "NETXEN:HIGH_TEMPERATURE");
			break;
		case NX_TEMP_PANIC:
			strcpy(NLMSG_DATA(nl_hdr), "NETXEN:OVER_TEMPERATURE");
			break;
	}
	
	NETLINK_CB(skb).pid = 0;      /* from kernel */

	netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);
	nx_nic_print5(NULL, "%s: Temp alert message sent to user space\n",__FUNCTION__);

	sock_release(nl_sk->sk_socket);
	sock_put(nl_sk);
}
#endif
#endif
