/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
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
 */
/* ethtool support for unm nic */

#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "unm_nic_hw.h"
#include "unm_nic.h"
#include "nic_phan_reg.h"
#include "unm_nic_ioctl.h"
#include "nic_cmn.h"
#include "unm_version.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"
#include "kernel_compatibility.h"

extern char *unm_nic_driver_string;
extern void unm_change_ringparam(struct unm_adapter_s *adapter);
extern int rom_fast_read(struct unm_adapter_s *adapter, int addr, int *valp);
extern int nx_lro_send_cleanup(struct unm_adapter_s *adapter);
extern unsigned long crb_pci_to_internal(unsigned long addr);
int nx_nic_get_linkevent_cap(struct unm_adapter_s *adapter);

#define ADAPTER_UP_MAGIC     777

#define MAX_ROM_SIZE  (NUM_FLASH_SECTORS \
				* FLASH_SECTOR_SIZE)

#define UNM_ROUNDUP(i, size)    ((i) = (((i) + (size) - 1) & ~((size) - 1)))

struct unm_nic_stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

#define UNM_NIC_STAT(m) sizeof(((struct unm_adapter_s *)0)->m), \
                    offsetof(struct unm_adapter_s, m)

static const struct unm_nic_stats unm_nic_gstrings_stats[] = {
	{"rcvd bad skb", UNM_NIC_STAT(stats.rcvdbadskb)},
	{"xmit called", UNM_NIC_STAT(stats.xmitcalled)},
	{"xmited frames", UNM_NIC_STAT(stats.xmitedframes)},
	{"xmit finished", UNM_NIC_STAT(stats.xmitfinished)},
	{"bad skb len", UNM_NIC_STAT(stats.badskblen)},
	{"no cmd desc", UNM_NIC_STAT(stats.nocmddescriptor)},
	{"polled", UNM_NIC_STAT(stats.polled)},
	{"uphappy", UNM_NIC_STAT(stats.uphappy)},
	{"updropped", UNM_NIC_STAT(stats.updropped)},
	{"tx dropped", UNM_NIC_STAT(stats.txdropped)},
	{"txOutOfBounceBuf dropped", UNM_NIC_STAT(stats.txOutOfBounceBufDropped)},
	{"csummed", UNM_NIC_STAT(stats.csummed)},
	{"no rcv", UNM_NIC_STAT(stats.no_rcv)},
	{"rx bytes", UNM_NIC_STAT(stats.rxbytes)},
	{"tx bytes", UNM_NIC_STAT(stats.txbytes)},
};

#define UNM_NIC_STATS_LEN        \
        sizeof(unm_nic_gstrings_stats) / sizeof(struct unm_nic_stats)

static const char unm_nic_gstrings_test[][ETH_GSTRING_LEN] = {
	"Register test  (on/offline)",
	"Link test   (on/offline)",
#if 0
	"Eeprom test    (offline)",
#endif
	"Interrupt test (offline)",
	"Loopback test  (offline)",
	"Led Test       (offline)"
};

#define UNM_NIC_TEST_LEN sizeof(unm_nic_gstrings_test) / ETH_GSTRING_LEN

#define UNM_NIC_REGS_COUNT 42
#define UNM_NIC_REGS_LEN (UNM_NIC_REGS_COUNT * sizeof(unm_crbword_t))

#define TRUE    1
#define FALSE   0
#define DEFAULT_LED_BLINK_TIME   10000

static long sw_lock_timeout= 100000000;

int sw_lock(struct unm_adapter_s *adapter)
{
    int i;
    int done = 0, timeout = 0;

	while (!done) {
		/* acquire semaphore3 from PCI HW block */
		done = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM6_LOCK));
		if (done == 1)
			break;
		if (timeout >= sw_lock_timeout) {
			return -1;
		}
		timeout++;
		/*
		 * Yield CPU
		 */
		if(!in_atomic())
			schedule();
		else {
			for(i = 0; i < 20; i++)
				cpu_relax();    /*This a nop instr on i386*/
		}
	}
    return 0;
}

void sw_unlock(struct unm_adapter_s *adapter)
{
    int val;
    /* release semaphore3 */
    val = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM6_UNLOCK));
}
static int unm_nic_get_eeprom_len(struct net_device *netdev)
{

	return MAX_ROM_SIZE;
#if 0
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int n;
	if ((rom_fast_read(adapter, 0, &n) == 0) && (n & 0x80000000)) {	// verify the rom address
		n &= ~0x80000000;
		if (n < 1024)
			return (n);
	}
	return 0;
#endif
}
static uint32_t
unm_nic_get_tx_csum(struct net_device *netdev)
{
	return (netdev->features & NETIF_F_HW_CSUM) != 0;
}
static int
unm_nic_set_tx_csum(struct net_device *netdev, uint32_t data)
{
	if (data)
		netdev->features |= NETIF_F_HW_CSUM;
	else
		netdev->features &= ~NETIF_F_HW_CSUM;
	return 0;
}

#ifdef NETIF_F_TSO
static uint32_t unm_nic_get_tso(struct net_device *dev)
{
#ifdef NETIF_F_TSO6
	return (dev->features & (NETIF_F_TSO | NETIF_F_TSO6)) != 0;
#endif
}

static int unm_nic_set_tso(struct net_device *dev, u32 data)
{
	if (data) {
		dev->features |= NETIF_F_TSO;
#ifdef NETIF_F_TSO6
		dev->features |= NETIF_F_TSO6;
#endif
	} else {
		dev->features &= ~NETIF_F_TSO;
#ifdef NETIF_F_TSO6
		dev->features &= ~NETIF_F_TSO6;
#endif
	}

	return 0;
}
#endif //#ifdef NETIF_F_TSO

static void
unm_nic_get_drvinfo(struct net_device *netdev, struct ethtool_drvinfo *drvinfo)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint32_t fw_major = 0;
	uint32_t fw_minor = 0;
	uint32_t fw_build = 0;

	strncpy(drvinfo->driver, unm_nic_driver_name, 32);
	strncpy(drvinfo->version, UNM_NIC_LINUX_VERSIONID, 32);

	read_lock(&adapter->adapter_lock);
	fw_major = NXRD32(adapter, UNM_FW_VERSION_MAJOR);
	fw_minor = NXRD32(adapter, UNM_FW_VERSION_MINOR);
	fw_build = NXRD32(adapter, UNM_FW_VERSION_SUB);
	read_unlock(&adapter->adapter_lock);
	sprintf(drvinfo->fw_version, "%d.%d.%d", fw_major, fw_minor, fw_build);

	strncpy(drvinfo->bus_info, pci_name(adapter->pdev), 32);
	drvinfo->n_stats = UNM_NIC_STATS_LEN;
	drvinfo->testinfo_len = UNM_NIC_TEST_LEN;
    if (adapter->mdump.has_valid_dump) {
        drvinfo->regdump_len = adapter->mdump.md_dump_size;
    } else {
        drvinfo->regdump_len = UNM_NIC_REGS_LEN;
    }
	drvinfo->eedump_len = unm_nic_get_eeprom_len(netdev);
}

static int
unm_nic_get_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	unm_board_info_t *boardinfo;
	u32 port_mode = 0;
	int linkevent_cap;

	boardinfo = &adapter->ahw.boardcfg;

	linkevent_cap = nx_nic_get_linkevent_cap(adapter) &&
				netif_running(netdev);

	// read which mode
	if (adapter->ahw.board_type == UNM_NIC_GBE) {
		ecmd->supported = ( SUPPORTED_10baseT_Full |
				SUPPORTED_100baseT_Full |
				SUPPORTED_1000baseT_Full);

		ecmd->advertising = ( ADVERTISED_100baseT_Full |
				ADVERTISED_1000baseT_Full);

		ecmd->speed = adapter->link_speed;
		ecmd->duplex = adapter->link_duplex;
		ecmd->autoneg = adapter->link_autoneg;

	} else if (adapter->ahw.board_type == UNM_NIC_XGBE) {
		port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);
		if (port_mode == UNM_PORT_MODE_802_3_AP) {
			ecmd->supported = SUPPORTED_1000baseT_Full;
			ecmd->advertising = ADVERTISED_1000baseT_Full;
			ecmd->speed = SPEED_1000;
		} else {
			ecmd->supported = SUPPORTED_10000baseT_Full;
			ecmd->advertising = ADVERTISED_10000baseT_Full;
			ecmd->speed = SPEED_10000;
		}
		ecmd->duplex = DUPLEX_FULL;

		/* if f/w can send async notification,
		 * link_speed is the correct speed 
		 */
		if (linkevent_cap) {

			ecmd->speed = adapter->link_speed;

		} else if((adapter->ahw.revision_id >= NX_P3_B0)) {
			u32 val;

			/* Use the per-function link speed value */
			val = NXRD32(adapter,
			     PF_LINK_SPEED_REG(adapter->ahw.pci_func));

			/* we have per-function link speed */
			ecmd->speed = PF_LINK_SPEED_VAL(adapter->ahw.pci_func, val)
					* PF_LINK_SPEED_MHZ;
		}
	} else {
		printk(KERN_ERR "%s: ERROR: Unsupported board model %d\n",
		       unm_nic_driver_name,
		       (unm_brdtype_t) boardinfo->board_type);
		return -EIO;
	}

	ecmd->phy_address = adapter->portnum;
	ecmd->transceiver = XCVR_EXTERNAL;

	switch ((unm_brdtype_t) boardinfo->board_type) {
	case UNM_BRDTYPE_P3_REF_QG:
	case UNM_BRDTYPE_P3_4_GB:
	case UNM_BRDTYPE_P3_4_GB_MM:
		ecmd->supported |= SUPPORTED_Autoneg;
		ecmd->advertising |= ADVERTISED_Autoneg;
	case UNM_BRDTYPE_P3_10G_CX4:
	case UNM_BRDTYPE_P3_10G_CX4_LP:
	case UNM_BRDTYPE_P3_10000_BASE_T:
		ecmd->supported |= SUPPORTED_TP;
		ecmd->advertising |= ADVERTISED_TP;
		ecmd->port = PORT_TP;
		ecmd->autoneg = adapter->link_autoneg;
		break;
	case UNM_BRDTYPE_P3_XG_LOM:
	case UNM_BRDTYPE_P3_HMEZ:
		ecmd->supported = (SUPPORTED_1000baseT_Full  |
				SUPPORTED_10000baseT_Full |
				SUPPORTED_Autoneg | SUPPORTED_MII);
		ecmd->advertising = (ADVERTISED_1000baseT_Full  |
				ADVERTISED_10000baseT_Full |
				ADVERTISED_Autoneg | ADVERTISED_MII);
		ecmd->port = PORT_MII;
		ecmd->autoneg = AUTONEG_ENABLE;
		break;
	case UNM_BRDTYPE_P3_IMEZ:
		ecmd->supported |= SUPPORTED_MII;
		ecmd->advertising |= ADVERTISED_MII;
		ecmd->port = PORT_MII;
		ecmd->autoneg = AUTONEG_DISABLE;
		break;

	case UNM_BRDTYPE_P3_10G_SFP_PLUS:
	case UNM_BRDTYPE_P3_10G_XFP:
		ecmd->supported |= SUPPORTED_FIBRE;
		ecmd->advertising |= ADVERTISED_FIBRE;

		if (linkevent_cap && adapter->link_module_type == 
					LINKEVENT_MODULE_TWINAX) {
			ecmd->port = PORT_TP;
		} else {
			ecmd->port = PORT_FIBRE;
		}
		ecmd->autoneg = AUTONEG_DISABLE;
		break;

        case UNM_BRDTYPE_P3_10G_TP:
                if (adapter->ahw.board_type == UNM_NIC_XGBE) {
                  ecmd->autoneg = AUTONEG_DISABLE;
                  ecmd->supported |= SUPPORTED_FIBRE;
                  ecmd->advertising |= ADVERTISED_FIBRE;
                  ecmd->port = PORT_FIBRE;
                }else {
                  ecmd->autoneg = AUTONEG_ENABLE;
                  ecmd->supported |= (SUPPORTED_TP |SUPPORTED_Autoneg);
                  ecmd->advertising |= (ADVERTISED_TP | ADVERTISED_Autoneg);
                  ecmd->port = PORT_TP;
               }
                  break;

	default:
		printk(KERN_ERR "%s: ERROR: Unsupported board model %d\n",
		       unm_nic_driver_name,
		       (unm_brdtype_t) boardinfo->board_type);
		return -EIO;
		break;
	}

	return 0;
}

static int
unm_nic_set_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int rv;

	if (adapter->ahw.board_type != UNM_NIC_GBE ||
		!(adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_GBE_LINK_CFG))
	{
		nx_nic_print3(adapter, "Setting operations not permitted for this board\n");
        /*Currently Not setting parameters from ethtool */
		return 0;
	}

	rv = nx_fw_cmd_set_gbe_port(adapter, adapter->physical_port,
			ecmd->speed, ecmd->duplex, ecmd->autoneg);
	if (!rv) {
		adapter->link_speed = ecmd->speed;
		adapter->link_duplex = ecmd->duplex;
		adapter->link_autoneg = ecmd->autoneg;

		adapter->cfg_speed = ecmd->speed;
		adapter->cfg_duplex = ecmd->duplex;
		adapter->cfg_autoneg = ecmd->autoneg;
	} else {
		nx_nic_print3(adapter, "Error in setting speed/duplex/autoneg config\n");
	}
	return rv;
}

static int unm_nic_get_regs_len(struct net_device *netdev)
{
	u32 dump_size;
	struct unm_adapter_s *adapter = netdev_priv(netdev);

    if (adapter->mdump.has_valid_dump) {
        dump_size = adapter->mdump.md_dump_size;
    } else {
        dump_size = UNM_NIC_REGS_LEN;
    }
	return dump_size;
}

static void
unm_nic_get_regs(struct net_device *netdev, struct ethtool_regs *regs, void *p)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if (adapter->mdump.has_valid_dump) {

		u8 *capture_buff = adapter->mdump.md_capture_buff;
		memset(p, 0, adapter->mdump.md_dump_size);
		regs->version = (1 << 24) | (adapter->ahw.revision_id << 16) |
					adapter->ahw.device_id;
		memcpy(p, capture_buff, adapter->mdump.md_dump_size);

	} else {

		unm_crbword_t mode, *regs_buff = p;

		memset(p, 0, UNM_NIC_REGS_LEN);
		regs->version = (1 << 24) | (adapter->ahw.revision_id << 16) |
			adapter->ahw.device_id;


		/* P3 */
		// which mode
		regs_buff[0] = NXRD32(adapter, UNM_PORT_MODE_ADDR);
		mode = regs_buff[0];

		// Common registers to all the modes
		nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
				crb_pci_to_internal(UNM_NIU_STRAP_VALUE_SAVE_HIGHER),
				0, &regs_buff[2]);
		switch (mode) {

			case UNM_PORT_MODE_XG:
			case UNM_PORT_MODE_AUTO_NEG_XG:{
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_SINGLE_TERM),
													   0, &regs_buff[3]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_DRIVE_HI),
													   0, &regs_buff[4]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_DRIVE_LO),
													   0, &regs_buff[5]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_DTX),
													   0, &regs_buff[6]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_DEQ),
													   0, &regs_buff[7]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_WORD_ALIGN),
													   0, &regs_buff[8]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_RESET),
													   0, &regs_buff[9]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_POWER_DOWN),
													   0, &regs_buff[10]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_RESET_PLL),
													   0, &regs_buff[11]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_SERDES_LOOPBACK),
													   0, &regs_buff[12]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_DO_BYTE_ALIGN),
													   0, &regs_buff[13]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_TX_ENABLE),
													   0, &regs_buff[14]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_RX_ENABLE),
													   0, &regs_buff[15]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_STATUS),
													   0, &regs_buff[16]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XG_PAUSE_THRESHOLD),
													   0, &regs_buff[17]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_CONFIG_0),
													   0, &regs_buff[18]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_CONFIG_1),
													   0, &regs_buff[19]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_IPG),
													   0, &regs_buff[20]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_STATION_ADDR_0_HI),
													   0, &regs_buff[21]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_STATION_ADDR_0_1),
													   0, &regs_buff[22]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_STATION_ADDR_1_LO),
													   0, &regs_buff[23]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_STATUS),
													   0, &regs_buff[24]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_MAX_FRAME_SIZE),
													   0, &regs_buff[25]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_PAUSE_FRAME_VALUE),
													   0, &regs_buff[26]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_TX_BYTE_CNT),
													   0, &regs_buff[27]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_TX_FRAME_CNT),
													   0, &regs_buff[28]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_RX_BYTE_CNT),
													   0, &regs_buff[29]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_RX_FRAME_CNT),
													   0, &regs_buff[30]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_AGGR_ERROR_CNT),
													   0, &regs_buff[31]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_MULTICAST_FRAME_CNT),
													   0, &regs_buff[32]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_UNICAST_FRAME_CNT),
													   0, &regs_buff[33]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_CRC_ERROR_CNT),
													   0, &regs_buff[34]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_OVERSIZE_FRAME_ERR),
													   0, &regs_buff[35]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_UNDERSIZE_FRAME_ERR),
													   0, &regs_buff[36]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_LOCAL_ERROR_CNT),
													   0, &regs_buff[37]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_REMOTE_ERROR_CNT),
													   0, &regs_buff[38]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_CONTROL_CHAR_CNT),
													   0, &regs_buff[39]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_XGE_PAUSE_FRAME_CNT),
													   0, &regs_buff[40]);
											   break;
										   }

			case UNM_PORT_MODE_GB:
			case UNM_PORT_MODE_AUTO_NEG_1G:{
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_SERDES_RESET),
													   0, &regs_buff[3]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB0_MII_MODE),
													   0, &regs_buff[4]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB1_MII_MODE),
													   0, &regs_buff[5]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB2_MII_MODE),
													   0, &regs_buff[6]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB3_MII_MODE),
													   0, &regs_buff[7]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB0_GMII_MODE),
													   0, &regs_buff[8]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB1_GMII_MODE),
													   0, &regs_buff[9]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB2_GMII_MODE),
													   0, &regs_buff[10]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB3_GMII_MODE),
													   0, &regs_buff[11]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_REMOTE_LOOPBACK),
													   0, &regs_buff[12]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB0_HALF_DUPLEX),
													   0, &regs_buff[13]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB1_HALF_DUPLEX),
													   0, &regs_buff[14]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_RESET_SYS_FIFOS),
													   0, &regs_buff[15]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_CRC_DROP),
													   0, &regs_buff[16]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_DROP_WRONGADDR),
													   0, &regs_buff[17]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_TEST_MUX_CTL),
													   0, &regs_buff[18]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MAC_CONFIG_0(0)),
													   0x10000, &regs_buff[19]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MAC_CONFIG_1(0)),
													   0x10000, &regs_buff[20]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_HALF_DUPLEX_CTRL(0)),
													   0x10000, &regs_buff[21]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MAX_FRAME_SIZE(0)),
													   0x10000, &regs_buff[22]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_TEST_REG(0)),
													   0x10000, &regs_buff[23]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_CONFIG(0)),
													   0x10000, &regs_buff[24]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_COMMAND(0)),
													   0x10000, &regs_buff[25]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_ADDR(0)),
													   0x10000, &regs_buff[26]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_CTRL(0)),
													   0x10000, &regs_buff[27]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_STATUS(0)),
													   0x10000, &regs_buff[28]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_MII_MGMT_INDICATE(0)),
													   0x10000, &regs_buff[29]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_INTERFACE_CTRL(0)),
													   0x10000, &regs_buff[30]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_INTERFACE_STATUS(0)),
													   0x10000, &regs_buff[31]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_STATION_ADDR_0(0)),
													   0x10000, &regs_buff[32]);
											   nx_fw_cmd_query_hw_reg(adapter, adapter->portnum,
													   crb_pci_to_internal(UNM_NIU_GB_STATION_ADDR_1(0)),
													   0x10000, &regs_buff[33]);
											   break;
										   }

		}
	}
}

/*
 * Get the per adapter message level. Currently the global is not adjusted.
 */
static uint32_t unm_nic_get_msglevel(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	return adapter->msglvl;
}

/*
 * Set the per adapter message level. Currently the global is not adjusted.
 */
static void unm_nic_set_msglevel(struct net_device *netdev, uint32_t data)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	adapter->msglvl = data;
}

/* Restart Link Process */
static int unm_nic_nway_reset(struct net_device *netdev)
{
	if (netif_running(netdev)) {
		netdev->stop(netdev);	// verify
		netdev->open(netdev);
	}
	return 0;
}

static int
unm_nic_get_eeprom(struct net_device *netdev,
		   struct ethtool_eeprom *eeprom, uint8_t * bytes)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int i, b_offset, b_end;
	uint8_t b_data[4], b_align;
	uint32_t data, b_rem;

	if ((eeprom->len <= 0) || (eeprom->offset >= MAX_ROM_SIZE))
		return -EINVAL;

	eeprom->magic = adapter->ahw.vendor_id | (adapter->ahw.device_id << 16);

	if ((eeprom->offset + eeprom->len) > MAX_ROM_SIZE)
		eeprom->len = MAX_ROM_SIZE - eeprom->offset;

	b_offset = eeprom->offset;
	b_end = eeprom->len + eeprom->offset;
	b_align = 4;
	b_rem = eeprom->offset % 4;
	/*Check for offset which is not 4 byte aligned */
	if ((b_rem) & 0x03) {

		b_offset -= b_rem;
		if (rom_fast_read(adapter, b_offset, &data) == -1)
			return -1;
		memcpy(b_data, &data, b_align);
		if (eeprom->len < (b_align - b_rem)) {
			memcpy(bytes, (b_data + b_rem), eeprom->len);
			return 0;
		} else
			memcpy(bytes, (b_data + b_rem), (b_align - b_rem));
		b_offset += b_align;
		bytes += b_align - b_rem;
	}

	for (i = 0; i <= (b_end - b_offset - b_align); i += b_align) {

		if (rom_fast_read(adapter, (b_offset + i), &data) == -1)
			return -1;
		memcpy((bytes + i), &data, b_align);
	}

	if ((b_end % b_align) & 0x03) {

		if (rom_fast_read(adapter, (b_offset + i), &data) == -1)
			return -1;
		memcpy((bytes + i), &data, (b_end % b_align));
	}

	return 0;
}

static void unm_nic_get_ringparam(struct net_device *netdev,
				  struct ethtool_ringparam *ring)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	ring->rx_mini_pending = 0;
	ring->rx_mini_max_pending = 0;

	ring->rx_pending = adapter->MaxRxDescCount;
	ring->rx_max_pending = NX_MAX_SUPPORTED_RDS_SZ;

	ring->rx_jumbo_pending = adapter->MaxJumboRxDescCount;
	ring->rx_jumbo_max_pending = NX_MAX_JUMBO_RDS_SIZE;

	ring->tx_pending = adapter->MaxTxDescCount;
	ring->tx_max_pending = NX_MAX_CMD_DESCRIPTORS;
}

// NO CHANGE of ringparams allowed !
static int unm_nic_set_ringparam(struct net_device *netdev,
				 struct ethtool_ringparam *ring)
{
	return (-EIO);
}

static void
unm_nic_get_pauseparam(struct net_device *netdev,
		       struct ethtool_pauseparam *pause)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int temp;

	pause->autoneg = AUTONEG_DISABLE;

	if ((adapter->ahw.board_type != UNM_NIC_GBE) &&
			(adapter->ahw.board_type != UNM_NIC_XGBE)) {
		printk(KERN_ERR "%s: Unknown board type: %x\n",
				unm_nic_driver_name, adapter->ahw.board_type);
		return;
	}

	if (nx_fw_cmd_get_flow_ctl(adapter, adapter->portnum, 0,
				&temp) == 0)
		pause->rx_pause = temp;
	if (nx_fw_cmd_get_flow_ctl(adapter, adapter->portnum, 1,
				&temp) == 0)
		pause->tx_pause = temp;
	return;

}

static int
unm_nic_set_pauseparam(struct net_device *netdev,
		       struct ethtool_pauseparam *pause)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int ret = 0;

	if(pause->autoneg)
		return -EOPNOTSUPP;

	if ((adapter->ahw.board_type != UNM_NIC_GBE) &&
			(adapter->ahw.board_type != UNM_NIC_XGBE))
		return -EIO;

	if (nx_fw_cmd_set_flow_ctl(adapter, adapter->portnum, 1,
				pause->tx_pause) != 0)
		ret = -EIO;

	if ((adapter->ahw.board_type == UNM_NIC_XGBE) &&
			(!pause->rx_pause)) {
		/*
		 * Changing rx pause parameter is not
		 * supported for now
		 */
		ret = -EOPNOTSUPP;
	} else {

		if (nx_fw_cmd_set_flow_ctl(adapter, adapter->portnum, 0,
					pause->rx_pause) != 0)
			ret = -EIO;
	}
	return ret;

}

static uint32_t unm_nic_get_rx_csum(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	return adapter->rx_csum;
}

static int unm_nic_set_rx_csum(struct net_device *netdev, uint32_t data)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	adapter->rx_csum = data;
	if(data == 0)
		nx_lro_send_cleanup(adapter);
	return 0;
}

static int unm_nic_reg_test(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint32_t data_read, data_written;

	// Read test
	data_read = NXRD32(adapter, UNM_PCIX_PH_REG(0));
	if ((data_read & 0xffff) != PHAN_VENDOR_ID) {
		return 1;
	}
	// write test
	data_written = (uint32_t) 0xa5a5a5a5;

	NXWR32(adapter, CRB_SCRATCHPAD_TEST, data_written);
	data_read = NXRD32(adapter, CRB_SCRATCHPAD_TEST);
	if (data_written != data_read) {
		return 1;
	}

	return 0;
}
#ifndef	ESX
static int unm_nic_intr_test(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if (!unm_irq_test(adapter))
		return 0;
	else
		return 1;
}
#endif

static int unm_nic_diag_test_count(struct net_device *netdev)
{
	return UNM_NIC_TEST_LEN;
}

static void
unm_nic_diag_test(struct net_device *netdev,
		  struct ethtool_test *eth_test, uint64_t * data)
{
	struct unm_adapter_s *adapter;
#ifndef ESX
        int count;
#endif
	adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	memset(data, 0, sizeof(uint64_t) * UNM_NIC_TEST_LEN);
	// online tests
	// register tests
	if ((data[0] = unm_nic_reg_test(netdev)))
		eth_test->flags |= ETH_TEST_FL_FAILED;
	// link test
	if ((data[1] = (uint64_t) unm_link_test(adapter)))
		eth_test->flags |= ETH_TEST_FL_FAILED;
#ifndef ESX
	//LED test
	(adapter->ahw).LEDTestLast = 0;
	for (count = 0; count < 8; count++) {
		//Need to restore LED on last test
		if (count == 7)
			(adapter->ahw).LEDTestLast = 1;
		data[4] = (uint64_t) unm_led_test(adapter);
		mdelay(100);
	}
	if (data[4] == -LED_TEST_NOT_SUPPORTED || data[4] == 0)
		data[4] = 0;
	else
		eth_test->flags |= ETH_TEST_FL_FAILED;
	//End Led Test
	if ((eth_test->flags == ETH_TEST_FL_OFFLINE) && (adapter->is_up == ADAPTER_UP_MAGIC)) {	// offline tests
		if (netif_running(netdev))
			netif_stop_queue(netdev);

		// interrupt tests
		if ((data[2] = unm_nic_intr_test(netdev)))
			eth_test->flags |= ETH_TEST_FL_FAILED;
		if (netif_running(netdev))
			netif_wake_queue(netdev);
		// loopback tests
		data[3] = unm_loopback_test(netdev, 1, NULL, &adapter->testCtx);
		if (data[3] == -LB_NOT_SUPPORTED || data[3] == 0)
			data[3] = 0;
		else
			eth_test->flags |= ETH_TEST_FL_FAILED;
	} else {
		data[2] = 0;
		data[3] = 0;
	}
#else
		data[2] = 1;
		data[3] = -LB_NOT_SUPPORTED;
		data[4] = -LED_TEST_NOT_SUPPORTED;
		eth_test->flags |= ETH_TEST_FL_FAILED;
#endif
}

static void
unm_nic_get_strings(struct net_device *netdev, uint32_t stringset,
		    uint8_t * data)
{
	int i;

	switch (stringset) {
	case ETH_SS_TEST:
		memcpy(data, *unm_nic_gstrings_test,
		       UNM_NIC_TEST_LEN * ETH_GSTRING_LEN);
		break;
	case ETH_SS_STATS:
		for (i = 0; i < UNM_NIC_STATS_LEN; i++) {
			memcpy(data + i * ETH_GSTRING_LEN,
			       unm_nic_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
		}
		break;
	}
}

static int unm_nic_get_stats_count(struct net_device *netdev)
{
	return UNM_NIC_STATS_LEN;
}

/*
 * NOTE: I have displayed only port's stats
 * TBD: unm_nic_stats(struct unm_port * port) doesn't update stats
 * as of now !! So this function may produce unexpected results !!
 */
static void
unm_nic_get_ethtool_stats(struct net_device *netdev,
			  struct ethtool_stats *stats, uint64_t * data)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int i;

	for (i = 0; i < UNM_NIC_STATS_LEN; i++) {
		char *p =
		    (char *)adapter + unm_nic_gstrings_stats[i].stat_offset;
		data[i] =
		    (unm_nic_gstrings_stats[i].sizeof_stat ==
		     sizeof(uint64_t)) ? *(uint64_t *) p : *(uint32_t *) p;
	}

}

void unm_nic_get_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	wol->supported = 0;
	wol->wolopts = 0;

	return;
}

static int
unm_nic_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	if (wol->wolopts)
		return -EOPNOTSUPP;

	return 0;
}
/*
 * Send the interrupt coalescing parameter set by ethtool to the card.
 */
static int nx_nic_config_intr_coalesce(struct unm_adapter_s *adapter)
{
	nic_request_t req;
	int rv = 0;

	memcpy(&req.body, &adapter->coal, sizeof(req.body));
	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_INTR_COALESCE;
	req.body.cmn.req_hdr.comp_id = 0;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	req.body.cmn.req_hdr.need_completion = 0;
	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *) & req,
				   1);
	if (rv) {
		nx_nic_print3(adapter, "Setting Interrupt Coalescing "
			      "parameters failed\n");
	}
	return (rv);
}
/*
 * Set the coalescing parameters. Currently only normal is supported.
 * If rx_coalesce_usecs == 0 or rx_max_coalesced_frames == 0 then set the
 * firmware coalescing to default.
 */
static int nx_ethtool_set_intr_coalesce(struct net_device *netdev,
					struct ethtool_coalesce *ethcoal)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}

	/*
	 * Return Error if  unsupported values or unsupported parameters are
	 * set
	 */
	if (ethcoal->rx_coalesce_usecs > 0xffff ||
	    ethcoal->rx_max_coalesced_frames > 0xffff ||
	    ethcoal->tx_coalesce_usecs > 0xffff ||
	    ethcoal->tx_max_coalesced_frames > 0xffff ||
	    ethcoal->rx_coalesce_usecs_irq ||
	    ethcoal->rx_max_coalesced_frames_irq ||
	    ethcoal->tx_coalesce_usecs_irq ||
	    ethcoal->tx_max_coalesced_frames_irq ||
	    ethcoal->stats_block_coalesce_usecs ||
	    ethcoal->use_adaptive_rx_coalesce ||
	    ethcoal->use_adaptive_tx_coalesce ||
	    ethcoal->pkt_rate_low ||
	    ethcoal->rx_coalesce_usecs_low ||
	    ethcoal->rx_max_coalesced_frames_low ||
	    ethcoal->tx_coalesce_usecs_low ||
	    ethcoal->tx_max_coalesced_frames_low ||
	    ethcoal->pkt_rate_high ||
	    ethcoal->rx_coalesce_usecs_high ||
	    ethcoal->rx_max_coalesced_frames_high ||
	    ethcoal->tx_coalesce_usecs_high ||
	    ethcoal->tx_max_coalesced_frames_high) {
		return -EINVAL;
	}

	if (ethcoal->rx_coalesce_usecs == 0 ||
	    ethcoal->rx_max_coalesced_frames == 0) {
		adapter->coal.flags = NX_NIC_INTR_DEFAULT;
		adapter->coal.normal.data.rx_time_us =
			NX_DEFAULT_INTR_COALESCE_RX_TIME_US;
		adapter->coal.normal.data.rx_packets =
			NX_DEFAULT_INTR_COALESCE_RX_PACKETS;
	} else {
		adapter->coal.flags = 0;
		adapter->coal.normal.data.rx_time_us =
			ethcoal->rx_coalesce_usecs;
		adapter->coal.normal.data.rx_packets =
			ethcoal->rx_max_coalesced_frames;
	}
	adapter->coal.normal.data.tx_time_us = ethcoal->tx_coalesce_usecs;
	adapter->coal.normal.data.tx_packets =
		ethcoal->tx_max_coalesced_frames;

	nx_nic_config_intr_coalesce(adapter);

	return (0);
}

/*
 * Get the interrupt coalescing parameters.
 */
static int nx_ethtool_get_intr_coalesce(struct net_device *netdev,
					struct ethtool_coalesce *ethcoal)
{
	struct unm_adapter_s   *adapter = netdev_priv(netdev);

	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}
	ethcoal->rx_coalesce_usecs = adapter->coal.normal.data.rx_time_us;
	ethcoal->tx_coalesce_usecs =  adapter->coal.normal.data.tx_time_us;
	ethcoal->rx_max_coalesced_frames =
		adapter->coal.normal.data.rx_packets;
	ethcoal->tx_max_coalesced_frames =
		adapter->coal.normal.data.tx_packets;	
	return 0;
}

static int nx_ethtool_set_phys_id(struct net_device *netdev,u32 val)
{
	struct unm_adapter_s   *adapter = netdev_priv(netdev);
	int ret=0;
	u32 state=1;
	u32 rate=2;
	u32 msec=1000 * val;

	if (!netif_running(netdev))
        	return LED_TEST_ERR;

	if (val == 0)
		msec=DEFAULT_LED_BLINK_TIME; /* Default blinking time is 10 sec  */

	if(unm_led_blink_state_set(adapter, state)) {
		nx_nic_print4(adapter, "unm_led_blink_state_set failed\n");
		ret=LED_TEST_ERR;
		return ret;
	}
	if (unm_led_blink_rate_set(adapter, rate)) {
		nx_nic_print4(adapter, "unm_led_blink_rate_set failed\n");
		ret=LED_TEST_ERR;
		return ret;
	}
	MSLEEP_INTERRUPTIBLE(((HZ * msec + 999) / 1000));
	state=0;
	rate=2;
	if(unm_led_blink_state_set(adapter, state)) {
		nx_nic_print4(adapter, "unm_led_blink_state_set failed\n");
		ret=LED_TEST_ERR;
		return ret;
	}
	if (unm_led_blink_rate_set(adapter, rate)) {
		nx_nic_print4(adapter, "unm_led_blink_rate_set failed\n");
		ret=LED_TEST_ERR;
		return ret;
	}
	ret=LED_TEST_OK;
	return ret;
}

static struct ethtool_ops unm_nic_ethtool_ops = {
	.get_settings		= unm_nic_get_settings,
	.set_settings		= unm_nic_set_settings,
	.get_drvinfo		= unm_nic_get_drvinfo,
	.get_regs_len		= unm_nic_get_regs_len,
	.get_regs		= unm_nic_get_regs,
	.get_wol		= unm_nic_get_wol,
	.set_wol		= unm_nic_set_wol,
	.get_msglevel		= unm_nic_get_msglevel,
	.set_msglevel		= unm_nic_set_msglevel,
	.nway_reset		= unm_nic_nway_reset,
	.get_link		= ethtool_op_get_link,

#ifndef __VMKERNEL_MODULE__
	.get_eeprom_len		= unm_nic_get_eeprom_len,
#endif
	.get_eeprom		= unm_nic_get_eeprom,
	.get_ringparam		= unm_nic_get_ringparam,
	.set_ringparam		= unm_nic_set_ringparam,
	.get_pauseparam		= unm_nic_get_pauseparam,
	.set_pauseparam		= unm_nic_set_pauseparam,
	.get_rx_csum		= unm_nic_get_rx_csum,
	.set_rx_csum		= unm_nic_set_rx_csum,
	.get_tx_csum		= unm_nic_get_tx_csum,
	.set_tx_csum		= unm_nic_set_tx_csum,
	.get_sg			= ethtool_op_get_sg,
	.set_sg			= ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso                = unm_nic_get_tso,
	.set_tso                = unm_nic_set_tso,
#endif
	.self_test_count	= unm_nic_diag_test_count,
	.self_test		= unm_nic_diag_test,
	.get_strings		= unm_nic_get_strings,
	.get_stats_count	= unm_nic_get_stats_count,
	.get_ethtool_stats	= unm_nic_get_ethtool_stats,
	.get_coalesce		= nx_ethtool_get_intr_coalesce,
	.set_coalesce		= nx_ethtool_set_intr_coalesce,
	.phys_id		= nx_ethtool_set_phys_id,
};

void set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &unm_nic_ethtool_ops;
}
