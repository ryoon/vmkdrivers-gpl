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
/*
 * Provides access to the Network Interface Unit h/w block.
 */
#include <unm_inc.h>
#include <linux/kernel.h>
#include <asm/string.h> /* for memset */
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
#include <linux/hardirq.h>
#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#include <asm/hardirq.h>
#endif
#endif
#include <asm/processor.h>

#include "unm_nic.h"
#include <linux/ethtool.h>

#ifndef in_atomic
#define in_atomic()     (1)
#endif

static long phy_lock_timeout= 100000000;

int phy_lock(struct unm_adapter_s *adapter)
{
    int i;
    int done = 0, timeout = 0;

    while (!done) {
        /* acquire semaphore3 from PCI HW block */
        done = NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM3_LOCK));
        if (done == 1)
            break;
        if (timeout >= phy_lock_timeout) {
            return -1;
        }
        timeout++;
        /*
         * Yield CPU
         */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        if(!in_atomic())
                schedule();
        else {
#endif
                for(i = 0; i < 20; i++)
                        cpu_relax();    /*This a nop instr on i386*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        }
#endif
    }
    NXWR32(adapter, UNM_PHY_LOCK_ID, PHY_LOCK_DRIVER);
    return 0;
}

void phy_unlock(struct unm_adapter_s *adapter)
{
    /* release semaphore3 */
    NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM3_UNLOCK));
}

/*
 * unm_niu_gbe_phy_read - read a register from the GbE PHY via
 * mii management interface.
 *
 * Note: The MII management interface goes through port 0.
 *       Individual phys are addressed as follows:
 *       [15:8]  phy id
 *       [7:0]   register number
 *
 * Returns:  0 success
 *          -1 error
 *
 */
long unm_niu_gbe_phy_read (struct unm_adapter_s *adapter,
			   long reg, unm_crbword_t *readval) {
    long phy = adapter->physical_port;
    unm_niu_gb_mii_mgmt_address_t address;
    unm_niu_gb_mii_mgmt_command_t command;
    unm_niu_gb_mii_mgmt_indicators_t status;

    long timeout=0;
    long result = 0;
    long restore = 0;
    unm_niu_gb_mac_config_0_t mac_cfg0;

    if (phy_lock(adapter) != 0) {
        return -1;
    }

    /* MII mgmt all goes through port 0 MAC interface, so it cannot be in reset */
    *(u32 *)&mac_cfg0 = NXRD32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0));
    if (mac_cfg0.soft_reset) {
        unm_niu_gb_mac_config_0_t temp;
        *(unm_crbword_t *)&temp = 0;
        temp.tx_reset_pb = 1;
        temp.rx_reset_pb = 1;
        temp.tx_reset_mac = 1;
        temp.rx_reset_mac = 1;
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0), *(u32 *)&temp);
        restore = 1;
    }


    *(unm_crbword_t *)&address = 0;
    address.reg_addr = reg;
    address.phy_addr = phy;
    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_ADDR(0), *(u32 *)&address);

    *(unm_crbword_t *)&command = 0;    /* turn off any prior activity */
    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_COMMAND(0), *(u32 *)&command);

    /* send read command */
    command.read_cycle=1;
    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_COMMAND(0), *(u32 *)&command);

    *(unm_crbword_t *)&status = 0;
    do {
        *(u32 *)&status = NXRD32(adapter, UNM_NIU_GB_MII_MGMT_INDICATE(0));
        timeout++;
    } while ((status.busy || status.notvalid) && (timeout++ < UNM_NIU_PHY_WAITMAX));

    if (timeout < UNM_NIU_PHY_WAITMAX) {
        *readval = NXRD32(adapter, UNM_NIU_GB_MII_MGMT_STATUS(0));
        result = 0;
    }else {
        result = -1;
    }

    if (restore)
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0), *(u32 *)&mac_cfg0);

    phy_unlock(adapter);

    return (result);

}
/*
 * unm_niu_gbe_phy_write - write a register to the GbE PHY via
 * mii management interface.
 *
 * Note: The MII management interface goes through port 0.
 *       Individual phys are addressed as follows:
 *       [15:8]  phy id
 *       [7:0]   register number
 *
 * Returns:  0 success
 *          -1 error
 *
 */
long unm_niu_gbe_phy_write (struct unm_adapter_s *adapter,
			    long reg, unm_crbword_t val) {
    long phy = adapter->physical_port;
    unm_niu_gb_mii_mgmt_address_t address;
    unm_niu_gb_mii_mgmt_command_t command;
    unm_niu_gb_mii_mgmt_indicators_t status;
    long timeout=0;
    long result = 0;
    long restore = 0;
    unm_niu_gb_mac_config_0_t mac_cfg0;

    if (phy_lock(adapter) != 0) {
        return -1;
    }

    /* MII mgmt all goes through port 0 MAC interface, so it cannot be in reset */
    *(u32 *)&mac_cfg0 = NXRD32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0));
    if (mac_cfg0.soft_reset) {
        unm_niu_gb_mac_config_0_t temp;
        *(unm_crbword_t *)&temp = 0;
        temp.tx_reset_pb = 1;
        temp.rx_reset_pb = 1;
        temp.tx_reset_mac = 1;
        temp.rx_reset_mac = 1;
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0), *(u32 *)&temp);
        restore = 1;
    }

    *(unm_crbword_t *)&command = 0;    /* turn off any prior activity */
    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_COMMAND(0), *(u32 *)&command);

    *(unm_crbword_t *)&address = 0;
    address.reg_addr = reg;
    address.phy_addr = phy;
    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_ADDR(0), *(u32 *)&address);

    NXWR32(adapter, UNM_NIU_GB_MII_MGMT_CTRL(0), val);


    *(unm_crbword_t *)&status = 0;
    do {
        *(u32 *)&status = NXRD32(adapter, UNM_NIU_GB_MII_MGMT_INDICATE(0));
        timeout++;
    } while ((status.busy) && (timeout++ < UNM_NIU_PHY_WAITMAX));

    if (timeout < UNM_NIU_PHY_WAITMAX) {
        result = 0;
    }else {
        result = -1;
    }

    /* restore the state of port 0 MAC in case we tampered with it */
    if (restore)
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(0), *(u32 *)&mac_cfg0);

    phy_unlock(adapter);

    return (result);

}

/*
 * Return the current station MAC address.
 * Note that the passed-in value must already be in network byte order.
 */
int
unm_niu_gbe_macaddr_get(struct unm_adapter_s *adapter,
                    unsigned char *addr)
{
    __uint64_t result;
    unsigned long flags;
    int phy = adapter->physical_port;

    if (addr == NULL)
        return -1;
    if ((phy < 0) || (phy > 3))
        return -1;

    write_lock_irqsave(&adapter->adapter_lock, flags);
    if (adapter->curr_window != 0) {
	    adapter->unm_nic_pci_change_crbwindow(adapter, 0);
    }

    result = readl((void *)pci_base_offset(adapter, UNM_NIU_GB_STATION_ADDR_1(phy)))
                                                                  >> 16;
    result |= ((uint64_t)readl((void *)pci_base_offset(adapter,
                                       UNM_NIU_GB_STATION_ADDR_0(phy)))) << 16;

    memcpy(addr, &result, sizeof(unm_ethernet_macaddr_t));

    adapter->unm_nic_pci_change_crbwindow(adapter, 1);

    write_unlock_irqrestore(&adapter->adapter_lock, flags);

    return 0;
}

/*
 * Set the station MAC address.
 * Note that the passed-in value must already be in network byte order.
 */
int
unm_niu_gbe_macaddr_set(struct unm_adapter_s *adapter,
                    unm_ethernet_macaddr_t addr)
{
    unm_crbword_t temp = 0;
    int phy = adapter->physical_port;

    if ((phy < 0) || (phy > 3))
        return -1;

    memcpy(&temp,addr,2);
    temp <<= 16;
    NXWR32(adapter, UNM_NIU_GB_STATION_ADDR_1(phy), temp);

    temp = 0;

    memcpy(&temp,((__uint8_t *)addr)+2,sizeof(unm_crbword_t));
    NXWR32(adapter, UNM_NIU_GB_STATION_ADDR_0(phy), temp);

    return 0;
}


/* Enable a GbE interface */
native_t nx_p2_niu_enable_gbe_port(struct unm_adapter_s *adapter)
{
	unm_niu_gb_mac_config_0_t mac_cfg0;
	unm_niu_gb_mac_config_1_t mac_cfg1;
	unm_niu_gb_mii_mgmt_config_t mii_cfg;
	native_t port = adapter->physical_port;

	if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
		return -1;
	}

	if (adapter->link_speed != SPEED_10 &&
	    adapter->link_speed != SPEED_100 &&
	    adapter->link_speed != SPEED_1000) {

		return -1;
	}

        *(unm_crbword_t *)&mac_cfg0 = 0;
        mac_cfg0.soft_reset = 1;
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port), 
				*(u32 *)&mac_cfg0);

        *(unm_crbword_t *)&mac_cfg0 = 0;
        mac_cfg0.tx_enable = 1;
        mac_cfg0.rx_enable = 1;
        mac_cfg0.rx_flowctl = 0;
        mac_cfg0.tx_reset_pb = 1;
        mac_cfg0.rx_reset_pb = 1;
        mac_cfg0.tx_reset_mac = 1;
        mac_cfg0.rx_reset_mac = 1;

        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port), 
				*(u32 *)&mac_cfg0);

        *(unm_crbword_t *)&mac_cfg1 = 0;
        mac_cfg1.preamblelen = 0xf;
        mac_cfg1.duplex=1;
        mac_cfg1.crc_enable=1;
        mac_cfg1.padshort=1;
        mac_cfg1.checklength=1;
        mac_cfg1.hugeframes=1;

        switch(adapter->link_speed) {
        case SPEED_10:
        case SPEED_100: /* Fall Through */
                mac_cfg1.intfmode=1;
                NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_1(port), 
					*(u32 *)&mac_cfg1);

                /* set mii mode */
                NXWR32(adapter, UNM_NIU_GB0_GMII_MODE+(port<<3), 
					0);
                NXWR32(adapter, UNM_NIU_GB0_MII_MODE+(port<< 3), 
					1);
                break;

        case SPEED_1000:
                mac_cfg1.intfmode=2;
                NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_1(port), 
					*(u32 *)&mac_cfg1);

                /* set gmii mode */
                NXWR32(adapter, UNM_NIU_GB0_MII_MODE+(port<< 3), 
					0);
                NXWR32(adapter, UNM_NIU_GB0_GMII_MODE+(port << 3), 
					1);
                break;

        default:
                /* Will not happen */
                break;
        }

        *(unm_crbword_t *)&mii_cfg = 0;
        mii_cfg.clockselect = 7;
        NXWR32(adapter, UNM_NIU_GB_MII_MGMT_CONFIG(port),
				 *(u32 *)&mii_cfg);

        *(unm_crbword_t *)&mac_cfg0 = 0;
        mac_cfg0.tx_enable = 1;
        mac_cfg0.rx_enable = 1;
        mac_cfg0.tx_flowctl = 0;
        mac_cfg0.rx_flowctl = 0;
        NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port), 
				*(u32 *)&mac_cfg0);

	return 0;
}


/* Disable a GbE interface */
native_t nx_p2_niu_disable_gbe_port(struct unm_adapter_s *adapter)
{
	native_t port = adapter->physical_port;
	unm_niu_gb_mac_config_0_t mac_cfg0;

	if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
		return -1;
	}
	*(unm_crbword_t *)&mac_cfg0 = 0;
	mac_cfg0.soft_reset = 1;

	NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port),
				    *(u32 *)&mac_cfg0);
	return 0;
}

/* Disable an XG interface */
native_t unm_niu_disable_xg_port(struct unm_adapter_s *adapter)
{
	native_t port = adapter->physical_port;
	unm_niu_xg_mac_config_0_t mac_cfg;

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		if (port != 0) {
			return -1;
		}
		*(unm_crbword_t *)&mac_cfg = 0;
		mac_cfg.soft_reset = 1;
		NXWR32(adapter, UNM_NIU_XGE_CONFIG_0,
				   *(u32 *)&mac_cfg);
	} else {
		if ((port < 0) || (port >= UNM_NIU_MAX_XG_PORTS )){
			return -1;
		}
		*(unm_crbword_t *)&mac_cfg = 0;
		mac_cfg.soft_reset = 1;
		NXWR32(adapter,
				    UNM_NIU_XGE_CONFIG_0 + (port * 0x10000),
				   *(u32 *)&mac_cfg);
	}
	return 0;
}


/* Set promiscuous mode for a GbE interface */
native_t unm_niu_set_promiscuous_mode(struct unm_adapter_s *adapter,
				      unm_niu_prom_mode_t mode) {
    native_t port = adapter->physical_port;
    unm_niu_gb_drop_crc_t reg;
    unm_niu_gb_mac_config_0_t mac_cfg;
    native_t data;
    int cnt =0,ret = 0;
    ulong val;
    if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
        return -1;
    }
    /* Turn off mac */
    *(u32 *)&mac_cfg = NXRD32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port));
    mac_cfg.rx_enable = 0;
    NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port),*(u32 *)&mac_cfg);


    /* wait until mac is drained by sre */
    //Port 0 rx fifo bit 5
    val = (0x20 << port);
    NXWR32(adapter, UNM_NIU_FRAME_COUNT_SELECT , val );

    do{
	    mdelay(10);
	    val = NXRD32(adapter, UNM_NIU_FRAME_COUNT);
	    cnt++;
	    if(cnt > 20){
	        ret = -1;
	        break;
	    }
	
     }while(val);

    /* now set promiscuous mode */
    if(ret != -1){
	    if ( mode == UNM_NIU_PROMISCOUS_MODE)
		data = 0;
	    else
		data = 1;
	
	    *(u32 *) &reg = NXRD32(adapter, UNM_NIU_GB_DROP_WRONGADDR);
	    switch (port) {
		case 0:
		    reg.drop_gb0 = data;
		    break;
		case 1:
		    reg.drop_gb1 = data;
		    break;
		case 2:
		    reg.drop_gb2 = data;
		    break;
		case 3:
		    reg.drop_gb3 = data;
		    break;
		default:
            ret  = -1;
    	}
    NXWR32(adapter, UNM_NIU_GB_DROP_WRONGADDR,*(u32 *)&reg);
    }

    /* trun the mac on back */
    mac_cfg.rx_enable = 1;
    NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port),*(u32 *)&mac_cfg);

    return ret;
}

/*
 * Set the MAC address for an XG port
 * Note that the passed-in value must already be in network byte order.
 */
int
unm_niu_xg_macaddr_set(struct unm_adapter_s *adapter,
		       unm_ethernet_macaddr_t addr)
{
    int phy = adapter->physical_port;
    unm_crbword_t temp = 0;
    u32   port_mode = 0;

    if ((phy < 0) || (phy > 3))
        return -1;

    switch (phy) {
    case 0:
        memcpy(&temp, addr, 2);
        temp <<= 16;
        port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);
        if (port_mode == UNM_PORT_MODE_802_3_AP) {
            NXWR32(adapter, UNM_NIU_AP_STATION_ADDR_1(phy), temp);
            temp = 0;
            memcpy(&temp,((__uint8_t *)addr)+2,sizeof(unm_crbword_t));
            NXWR32(adapter, UNM_NIU_AP_STATION_ADDR_0(phy), temp);
        } else {
            NXWR32(adapter, UNM_NIU_XGE_STATION_ADDR_0_1, temp);
            temp = 0;
            memcpy(&temp,((__uint8_t *)addr)+2,sizeof(unm_crbword_t));
            NXWR32(adapter, UNM_NIU_XGE_STATION_ADDR_0_HI, temp);
        }
        break;

    case 1:
        memcpy(&temp, addr, 2);
        temp <<= 16;
        port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);
        if (port_mode == UNM_PORT_MODE_802_3_AP) {
            NXWR32(adapter, UNM_NIU_AP_STATION_ADDR_1(phy), temp);
            temp = 0;
            memcpy(&temp,((__uint8_t *)addr)+2,sizeof(unm_crbword_t));
            NXWR32(adapter, UNM_NIU_AP_STATION_ADDR_0(phy), temp);
        } else {
            NXWR32(adapter, UNM_NIU_XG1_STATION_ADDR_0_1, temp);
            temp = 0;
            memcpy(&temp,((__uint8_t *)addr)+2,sizeof(unm_crbword_t));
            NXWR32(adapter, UNM_NIU_XG1_STATION_ADDR_0_HI, temp);
        }
        break;

    default:
        printk(KERN_ERR "Unknown port %d\n", phy);
        break;
    }

    return 0;
}

/*
 * Return the current station MAC address.
 * Note that the passed-in value must already be in network byte order.
 */
int
unm_niu_xg_macaddr_get(struct unm_adapter_s *adapter,
		       unm_ethernet_macaddr_t *addr)
{
    int phy = adapter->physical_port;
    unm_crbword_t stationhigh;
    unm_crbword_t stationlow;
    __uint64_t result;

    if (addr == NULL)
        return -1;
    if (phy != 0)
        return -1;

    stationhigh = NXRD32(adapter, UNM_NIU_XGE_STATION_ADDR_0_HI);
    stationlow = NXRD32(adapter, UNM_NIU_XGE_STATION_ADDR_0_1);

    result = ((__uint64_t)stationlow) >> 16;
    result |= (__uint64_t)stationhigh << 16;
    memcpy(*addr,&result,sizeof(unm_ethernet_macaddr_t));

    return 0;
}

native_t
unm_niu_xg_set_promiscuous_mode(struct unm_adapter_s *adapter,
			unm_niu_prom_mode_t mode)
{
    long  reg;
    unm_niu_xg_mac_config_0_t mac_cfg;
    native_t port = adapter->physical_port;
    int cnt = 0;
    int result = 0;
    u32 port_mode = 0;

    if ((port < 0) || (port > UNM_NIU_MAX_XG_PORTS)) {
        return -1;
    }

    port_mode = NXRD32(adapter, UNM_PORT_MODE_ADDR);
    if (port_mode == UNM_PORT_MODE_802_3_AP) {
        reg = 0;
        NXWR32(adapter, UNM_NIU_GB_DROP_WRONGADDR, reg);
    } else {

        /* Turn off mac */
        *(u32 *)&mac_cfg = NXRD32(adapter, UNM_NIU_XGE_CONFIG_0+(0x10000*port));
        mac_cfg.rx_enable = 0;
        NXWR32(adapter, UNM_NIU_XGE_CONFIG_0+(0x10000*port), *(u32 *)&mac_cfg);


        /* wait until mac is drained by sre */
        if ((adapter->ahw.boardcfg.board_type != UNM_BRDTYPE_P2_SB31_10G_IMEZ) &&
	        (adapter->ahw.boardcfg.board_type != UNM_BRDTYPE_P2_SB31_10G_HMEZ)){
	/*     single port case bit 9*/
	reg     = 0x0200;
        	NXWR32(adapter, UNM_NIU_FRAME_COUNT_SELECT , reg );
         }else{

        	//Port 0 rx fifo bit 5
        	reg = (0x20 << port);
        	NXWR32(adapter, UNM_NIU_FRAME_COUNT_SELECT , reg );
        }
        do{
	mdelay(10);
            reg = NXRD32(adapter, UNM_NIU_FRAME_COUNT);
	cnt++;
	if(cnt     > 20){
	       result = -1;
	       break;
	}
	
         }while(reg);

        /* now set promiscuous mode */
        if(result != -1)
        {
	        reg = NXRD32(adapter, UNM_NIU_XGE_CONFIG_1+(0x10000*port));
	        if (mode == UNM_NIU_PROMISCOUS_MODE) {
		reg     = (reg | 0x2000UL);
	        } else { /* FIXME  use the correct mode value here*/
		reg     = (reg & ~0x2000UL);
	        }
	        NXWR32(adapter, UNM_NIU_XGE_CONFIG_1+(0x10000*port), reg);
        }

        /* trun the mac on back */
        mac_cfg.rx_enable = 1;
        NXWR32(adapter, UNM_NIU_XGE_CONFIG_0+(0x10000*port), *(u32 *)&mac_cfg);
    }

    return result;
}

int
unm_niu_xg_get_tx_flow_ctl(struct unm_adapter_s *adapter,
				int *val)
{
    int port = adapter->physical_port;
    unm_niu_xg_pause_ctl_t reg;
    if ((port < 0) || (port > UNM_NIU_MAX_XG_PORTS)) {
        return -1;
    }
    *(u32*)&reg = NXRD32(adapter, UNM_NIU_XG_PAUSE_CTL);
    if (port == 0) {
        *val = !reg.xg0_mask;
    } else {
        *val = !reg.xg1_mask;
    }
    return 0;
}

int
unm_niu_xg_set_tx_flow_ctl(struct unm_adapter_s *adapter,
				int enable)
{
    int port = adapter->physical_port;
    unm_niu_xg_pause_ctl_t reg;

    if ((port < 0) || (port > UNM_NIU_MAX_XG_PORTS)) {
        return -1;
    }
    *(u32*)&reg = NXRD32(adapter, UNM_NIU_XG_PAUSE_CTL);
    if (port == 0) {
        reg.xg0_mask = !enable;
    } else {
        reg.xg1_mask = !enable;
    }
    NXWR32(adapter, UNM_NIU_XG_PAUSE_CTL, *(u32 *)&reg);

    return 0;
}

int
unm_niu_gbe_get_tx_flow_ctl(struct unm_adapter_s *adapter,
				int *val)
{
    int port = adapter->physical_port;
    unm_niu_gb_pause_ctl_t reg;
    if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
        return -1;
    }
    *(u32*)&reg = NXRD32(adapter, UNM_NIU_GB_PAUSE_CTL);
    switch (port) {
        case(0):
            *val = !reg.gb0_mask;
            break;
        case(1):
            *val = !reg.gb1_mask;
            break;
        case(2):
            *val = !reg.gb2_mask;
            break;
        case(3):
        default:
            *val = !reg.gb3_mask;
            break;
    }
    return 0;
}
int
unm_niu_gbe_set_tx_flow_ctl(struct unm_adapter_s *adapter,
				int enable)
{
    int port = adapter->physical_port;
    unm_niu_gb_pause_ctl_t reg;

    if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
        return -1;
    }
    *(u32*)&reg = NXRD32(adapter, UNM_NIU_GB_PAUSE_CTL);
    switch (port) {
        case(0):
            reg.gb0_mask = !enable;
            break;
        case(1):
            reg.gb1_mask = !enable;
            break;
        case(2):
            reg.gb2_mask = !enable;
            break;
        case(3):
        default:
            reg.gb3_mask = !enable;
            break;
    }
    NXWR32(adapter, UNM_NIU_GB_PAUSE_CTL, *(u32 *)&reg);

    return 0;
}

int
unm_niu_gbe_get_rx_flow_ctl(struct unm_adapter_s *adapter,
				int *val)
{
    int port = adapter->physical_port;
    unm_niu_gb_mac_config_0_t reg;

    if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
        return -1;
    }
    *(u32*)&reg = NXRD32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port));
    *val = reg.rx_flowctl;

    return 0;
}
int
unm_niu_gbe_set_rx_flow_ctl(struct unm_adapter_s *adapter,
				int enable)
{
    int port = adapter->physical_port;
    unm_niu_gb_mac_config_0_t reg;

    if ((port < 0) || (port > UNM_NIU_MAX_GBE_PORTS)) {
        return -1;
    }

    *(u32*)&reg = NXRD32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port));
    reg.rx_flowctl = enable;
    NXWR32(adapter, UNM_NIU_GB_MAC_CONFIG_0(port), *(u32 *)&reg);

    return 0;
}
