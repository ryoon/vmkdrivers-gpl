/*
 * Portions Copyright 2008, 2012 VMware, Inc.
 */
/*
 * net/core/ethtool.c - Ethtool ioctl handler
 * Copyright (c) 2003 Matthew Wilcox <matthew@wil.cx>
 *
 * This file is where we call all the ethtool_ops commands to get
 * the information ethtool needs.  We fall back to calling do_ioctl()
 * for drivers which haven't been converted to ethtool_ops yet.
 *
 * It's GPL, stupid.
 */

/*
 * ioctl32.c: Conversion between 32bit and 64bit native ioctls.
 *
 * Copyright (C) 1997-2000  Jakub Jelinek  (jakub@redhat.com)
 * Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 * Copyright (C) 2001,2002  Andi Kleen, SuSE Labs 
 * Copyright (C) 2003       Pavel Machek (pavel@suse.cz)
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * ioctls.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>
#if defined( __VMKLNX__)
#include <linux/compat.h>
#endif

#if defined( __VMKLNX__)
struct ifmap32 {
	compat_ulong_t mem_start;
	compat_ulong_t mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

struct ifreq32 {
#define IFHWADDRLEN     6
#define IFNAMSIZ        16
        union {
                char    ifrn_name[IFNAMSIZ];            /* if name, e.g. "en0" */
        } ifr_ifrn;
        union {
                struct  sockaddr ifru_addr;
                struct  sockaddr ifru_dstaddr;
                struct  sockaddr ifru_broadaddr;
                struct  sockaddr ifru_netmask;
                struct  sockaddr ifru_hwaddr;
                short   ifru_flags;
                compat_int_t     ifru_ivalue;
                compat_int_t     ifru_mtu;
                struct  ifmap32 ifru_map;
                char    ifru_slave[IFNAMSIZ];   /* Just fits the size */
		char	ifru_newname[IFNAMSIZ];
                compat_caddr_t ifru_data;
	    /* XXXX? ifru_settings should be here */
        } ifr_ifru;
};
#endif /* defined( __VMKLNX__) */

/* 
 * Some useful ethtool_ops methods that're device independent.
 * If we find that all drivers want to do the same thing here,
 * we can turn these into dev_() function calls.
 */

/**                                          
 *  ethtool_op_get_link - returns the link status for the given net_device
 *  @dev: device whose link status to get
 *
 *  Returns the link status for the given net_device
 *                                           
 *  RETURN VALUES:
 *  1 if the link carrier is detected,
 *  0 otherwise
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_get_link */
u32 ethtool_op_get_link(struct net_device *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return netif_carrier_ok(dev) ? 1 : 0;
}

/**                                          
 *  ethtool_op_get_tx_csum - return whether transmit checksums are turned on
 *  @dev: net_device to check for transmit checksums
 *
 *  Returns whether transmit checksums are turned on for the given net_device
 *                                           
 *  RETURN VALUE:
 *  Non-zero if transmit checksums are turned on for the given net_device,
 *  0 otherwise
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_get_tx_csum */
u32 ethtool_op_get_tx_csum(struct net_device *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return (dev->features & NETIF_F_ALL_CSUM) != 0;
}

/**                                          
 *  ethtool_op_set_tx_csum - sets transmit checksums
 *  @dev: device for which to turn on or off transmit checksums
 *  @data: non-zero to turn on transmit checksums, 0 to turn off
 *
 *  Sets whether transmit checksums are enabled for the given net_device
 *                                           
 *  RETURN VALUE:
 *  Zero always
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_set_tx_csum */
int ethtool_op_set_tx_csum(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (data)
		dev->features |= NETIF_F_IP_CSUM;
	else
		dev->features &= ~NETIF_F_IP_CSUM;

	return 0;
}

/**                                          
 *  ethtool_op_set_tx_hw_csum - sets hardware transmit checksums
 *  @dev: device for which to turn on or off hardware transmit checksums
 *  @data: non-zero to turn on transmit checksums, 0 to turn off
 *                                           
 *  Sets whether hardware transmit checksums are enabled for the given
 *  net_device
 *
 *  RETURN VALUE:
 *  Zero always
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_set_tx_hw_csum */
int ethtool_op_set_tx_hw_csum(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (data)
		dev->features |= NETIF_F_HW_CSUM;
	else
		dev->features &= ~NETIF_F_HW_CSUM;

	return 0;
}

#if defined(__VMKLNX__)
/**                                          
 *  ethtool_op_set_tx_ipv6_csum - sets hardware ip and ipv6 transmit checksums
 *  @dev: device for which to turn on or off transmit checksums
 *  @data: non-zero to turn on transmit checksums, 0 to turn off
 *                                           
 *  Sets hardware transmit checksums for ip and ipv6 for the given
 *  net_device.
 *
 *  RETURN VALUE:
 *  Zero always
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_set_tx_ipv6_csum */
int ethtool_op_set_tx_ipv6_csum(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
        if (data)
                dev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
        else
                dev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
        
        return 0;
}
EXPORT_SYMBOL(ethtool_op_set_tx_ipv6_csum);
#endif /* defined(__VMKLNX__) */

/**                                          
 *  ethtool_op_get_sg - report whether scatter-gather is enabled
 *  @dev: net_device to check for scatter-gather
 *                                           
 *  Returns whether scatter-gather is enabled for the given net_device
 *                                           
 *  RETURN VALUES:
 *  Non-zero if scatter-gather is enabled,
 *  0 otherwise
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_get_sg */
u32 ethtool_op_get_sg(struct net_device *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return (dev->features & NETIF_F_SG) != 0;
}

/**                                          
 *  ethtool_op_set_sg - set scatter-gather
 *  @dev: device for which to enable or disable scatter-gather
 *  @data: non-zero to enable scatter-gather, or 0 to disable it
 *
 *  Sets whether scatter-gather is enabled for the given net_device
 *                                           
 *  RETURN VALUE:
 *  Zero always
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_set_sg */
int ethtool_op_set_sg(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (data)
		dev->features |= NETIF_F_SG;
	else
		dev->features &= ~NETIF_F_SG;

	return 0;
}

/**                                          
 *  ethtool_op_get_tso - report whether TCP segmentation offload is enabled
 *  @dev: net_device to check for TSO
 *                                           
 *  Returns whether TCP segmentation offload is enabled for the given
 *  net_device
 *                                           
 *  RETURN VALUES:
 *  Non-zero if TSO is enabled,
 *  0 otherwise
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_get_tso */
u32 ethtool_op_get_tso(struct net_device *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return (dev->features & NETIF_F_TSO) != 0;
}

/**                                          
 *  ethtool_op_set_tso - set TCP segmentation offload
 *  @dev: net_device for which to enable or disable TSO
 *  @data: non-zero to enable TSO, 0 to disable it
 *
 *  Sets whether TSO is enabled for the given net_device
 *                                           
 *  RETURN VALUE:
 *  Zero always
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_set_tso */
int ethtool_op_set_tso(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (data)
		dev->features |= NETIF_F_TSO;
	else
		dev->features &= ~NETIF_F_TSO;

	return 0;
}

/**                                          
 *  ethtool_op_get_perm_addr - returns the permanent hardware address
 *  @dev: device from which to obtain the hardware address
 *  @addr: used to check address length
 *  @data: output pointer
 *
 *  Returns the permanent hardware address for the given net_device
 *                                           
 *  RETURN VALUES:
 *  -ETOOSMALL if the given ethtool_perm_addr length is too small,
 *  0 otherwise
 */                                          
/* _VMKLNX_CODECHECK_: ethtool_op_get_perm_addr */
int ethtool_op_get_perm_addr(struct net_device *dev, struct ethtool_perm_addr *addr, u8 *data)
{
	unsigned char len = dev->addr_len;
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if ( addr->size < len )
		return -ETOOSMALL;
	
	addr->size = len;
	memcpy(data, dev->perm_addr, len);
	return 0;
}
 

u32 ethtool_op_get_ufo(struct net_device *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	return (dev->features & NETIF_F_UFO) != 0;
}

int ethtool_op_set_ufo(struct net_device *dev, u32 data)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (data)
		dev->features |= NETIF_F_UFO;
	else
		dev->features &= ~NETIF_F_UFO;
	return 0;
}

/* Handlers for each ethtool command */

static int ethtool_get_settings(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_cmd cmd = { ETHTOOL_GSET };
	int err;

	if (!dev->ethtool_ops->get_settings)
		return -EOPNOTSUPP;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->get_settings,
                           dev, &cmd);
#else
        err = dev->ethtool_ops->get_settings(dev, &cmd); 
#endif
	if (err < 0)
		return err;

	if (copy_to_user(useraddr, &cmd, sizeof(cmd)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_settings(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_cmd cmd;
#if defined(__VMKLNX__)
        int err;
#endif

	if (!dev->ethtool_ops->set_settings)
		return -EOPNOTSUPP;

	if (copy_from_user(&cmd, useraddr, sizeof(cmd)))
		return -EFAULT;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_settings,
                           dev, &cmd);
	return err;
#else
        return dev->ethtool_ops->set_settings(dev, &cmd);
#endif
}

static int ethtool_get_drvinfo(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_drvinfo info;
	struct ethtool_ops *ops = dev->ethtool_ops;

	if (!ops->get_drvinfo)
		return -EOPNOTSUPP;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_drvinfo, dev, &info);
#else
        ops->get_drvinfo(dev, &info);
#endif
	if (ops->get_sset_count) {
		int rc;

#if defined(__VMKLNX__)
      		VMKAPI_MODULE_CALL(dev->module_id, rc, ops->get_sset_count, dev, ETH_SS_TEST);
#else
		rc = ops->get_sset_count(dev, ETH_SS_TEST);
#endif
		if (rc >= 0)
			info.testinfo_len = rc;
#if defined(__VMKLNX__)
      		VMKAPI_MODULE_CALL(dev->module_id, rc, ops->get_sset_count, dev, ETH_SS_STATS);
#else
		rc = ops->get_sset_count(dev, ETH_SS_STATS);
#endif
		if (rc >= 0)
			info.n_stats = rc;
	} else {
		/* code path for obsolete hooks */
		if (ops->self_test_count) {
#if defined(__VMKLNX__)
			VMKAPI_MODULE_CALL(dev->module_id, info.testinfo_len,
					   ops->self_test_count, dev);  
#else
			info.testinfo_len = ops->self_test_count(dev);
#endif
		}
		if (ops->get_stats_count) {
#if defined(__VMKLNX__)
			VMKAPI_MODULE_CALL(dev->module_id, info.n_stats, 
					   ops->get_stats_count, dev);
#else
			info.n_stats = ops->get_stats_count(dev);
#endif
		}
	}

	if (ops->get_regs_len) {
#if defined(__VMKLNX__)
		VMKAPI_MODULE_CALL(dev->module_id, info.regdump_len,
                                   ops->get_regs_len, dev);
#else
                info.regdump_len = ops->get_regs_len(dev);
#endif
        }
	if (ops->get_eeprom_len) {
#if defined(__VMKLNX__)
		VMKAPI_MODULE_CALL(dev->module_id, info.eedump_len,
                                   ops->get_eeprom_len, dev);
#else
                info.eedump_len = ops->get_eeprom_len(dev);
#endif
        }
	if (copy_to_user(useraddr, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

static int ethtool_get_regs(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_regs regs;
	struct ethtool_ops *ops = dev->ethtool_ops;
	void *regbuf;
	int reglen, ret;

	if (!ops->get_regs || !ops->get_regs_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&regs, useraddr, sizeof(regs)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, reglen, ops->get_regs_len, dev); 
#else
        reglen = ops->get_regs_len(dev); 
#endif
	if (regs.len > reglen)
		regs.len = reglen;

	regbuf = kmalloc(reglen, GFP_USER);
	if (!regbuf)
		return -ENOMEM;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_regs, dev, &regs, 
                                regbuf);
#else
        ops->get_regs(dev, &regs, regbuf);
#endif

	ret = -EFAULT;
	if (copy_to_user(useraddr, &regs, sizeof(regs)))
		goto out;
	useraddr += offsetof(struct ethtool_regs, data);
	if (copy_to_user(useraddr, regbuf, regs.len))
		goto out;
	ret = 0;

 out:
	kfree(regbuf);
	return ret;
}

static int ethtool_get_wol(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

	if (!dev->ethtool_ops->get_wol)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->ethtool_ops->get_wol, 
                                dev, &wol);
#else
        dev->ethtool_ops->get_wol(dev, &wol);
#endif

	if (copy_to_user(useraddr, &wol, sizeof(wol)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_wol(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_wolinfo wol;
#if defined(__VMKLNX__)
        int err;
#endif

	if (!dev->ethtool_ops->set_wol)
		return -EOPNOTSUPP;

	if (copy_from_user(&wol, useraddr, sizeof(wol)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_wol,
                           dev, &wol);
	return err; 
#else
        return dev->ethtool_ops->set_wol(dev, &wol);
#endif
}

static int ethtool_get_msglevel(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GMSGLVL };

	if (!dev->ethtool_ops->get_msglevel)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
	VMKAPI_MODULE_CALL(dev->module_id, edata.data, dev->ethtool_ops->get_msglevel,
                           dev);
#else
        edata.data = dev->ethtool_ops->get_msglevel(dev);
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_msglevel(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;

	if (!dev->ethtool_ops->set_msglevel)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

#if defined(__VMKLNX__)
	VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->ethtool_ops->set_msglevel, dev, 
                                edata.data);
#else
        dev->ethtool_ops->set_msglevel(dev, edata.data);
#endif
	return 0;
}

static int ethtool_nway_reset(struct net_device *dev)
{
#if defined(__VMKLNX__)
	int err;
#endif
        if (!dev->ethtool_ops->nway_reset)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->nway_reset, dev);
	return err;
#else
        return dev->ethtool_ops->nway_reset(dev);
#endif
}

static int ethtool_get_link(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GLINK };

	if (!dev->ethtool_ops->get_link)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, 
                           dev->ethtool_ops->get_link, dev);
#else
        edata.data = dev->ethtool_ops->get_link(dev);
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_get_eeprom(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_eeprom eeprom;
	struct ethtool_ops *ops = dev->ethtool_ops;
	u8 *data;
	int ret;
#if defined(__VMKLNX__)
        int ops_eeprom_len;
#endif

	if (!ops->get_eeprom || !ops->get_eeprom_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
		return -EFAULT;

	/* Check for wrap and zero */
	if (eeprom.offset + eeprom.len <= eeprom.offset)
		return -EINVAL;

	/* Check for exceeding total eeprom len */
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ops_eeprom_len, ops->get_eeprom_len, 
                           dev);
	if (eeprom.offset + eeprom.len > ops_eeprom_len)
#else
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
#endif
		return -EINVAL;

	data = kmalloc(eeprom.len, GFP_USER);
	if (!data)
		return -ENOMEM;

	ret = -EFAULT;
	if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
		goto out;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, ops->get_eeprom, dev, &eeprom, 
                           data);
#else
        ret = ops->get_eeprom(dev, &eeprom, data);
#endif
	if (ret)
		goto out;

	ret = -EFAULT;
	if (copy_to_user(useraddr, &eeprom, sizeof(eeprom)))
		goto out;
	if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
		goto out;
	ret = 0;

 out:
	kfree(data);
	return ret;
}

static int ethtool_set_eeprom(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_eeprom eeprom;
	struct ethtool_ops *ops = dev->ethtool_ops;
	u8 *data;
	int ret;
#if defined(__VMKLNX__)
        int ops_eeprom_len;
#endif

	if (!ops->set_eeprom || !ops->get_eeprom_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
		return -EFAULT;

	/* Check for wrap and zero */
	if (eeprom.offset + eeprom.len <= eeprom.offset)
		return -EINVAL;

	/* Check for exceeding total eeprom len */
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ops_eeprom_len, ops->get_eeprom_len,
                           dev);
	if (eeprom.offset + eeprom.len > ops_eeprom_len)
#else
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
#endif
		return -EINVAL;

	data = kmalloc(eeprom.len, GFP_USER);
	if (!data)
		return -ENOMEM;

	ret = -EFAULT;
	if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
		goto out;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, ops->set_eeprom, dev, &eeprom,
                           data);
#else
        ret = ops->set_eeprom(dev, &eeprom, data);
#endif
	if (ret)
		goto out;

	if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
		ret = -EFAULT;

 out:
	kfree(data);
	return ret;
}

static int ethtool_get_coalesce(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_coalesce coalesce = { ETHTOOL_GCOALESCE };

	if (!dev->ethtool_ops->get_coalesce)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->ethtool_ops->get_coalesce, 
                                dev, &coalesce);
#else
        dev->ethtool_ops->get_coalesce(dev, &coalesce); 
#endif
	if (copy_to_user(useraddr, &coalesce, sizeof(coalesce)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_coalesce(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_coalesce coalesce;
#if defined(__VMKLNX__)
        int ret;
#endif

	if (!dev->ethtool_ops->set_coalesce)
		return -EOPNOTSUPP;

	if (copy_from_user(&coalesce, useraddr, sizeof(coalesce)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, dev->ethtool_ops->set_coalesce, 
                           dev, &coalesce);
	return ret; 
#else
        return dev->ethtool_ops->set_coalesce(dev, &coalesce);
#endif
}

static int ethtool_get_ringparam(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_ringparam ringparam = { ETHTOOL_GRINGPARAM };

	if (!dev->ethtool_ops->get_ringparam)
		return -EOPNOTSUPP;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->ethtool_ops->get_ringparam, 
                                dev, &ringparam);
#else
        dev->ethtool_ops->get_ringparam(dev, &ringparam);
#endif
	if (copy_to_user(useraddr, &ringparam, sizeof(ringparam)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_ringparam(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_ringparam ringparam;
#if defined(__VMKLNX__)
        int ret;
#endif

	if (!dev->ethtool_ops->set_ringparam)
		return -EOPNOTSUPP;

	if (copy_from_user(&ringparam, useraddr, sizeof(ringparam)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, dev->ethtool_ops->set_ringparam, 
                           dev, &ringparam);
	return ret;
#else
        return dev->ethtool_ops->set_ringparam(dev, &ringparam);
#endif
}

static int ethtool_get_pauseparam(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_pauseparam pauseparam = { ETHTOOL_GPAUSEPARAM };

	if (!dev->ethtool_ops->get_pauseparam)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, 
                                dev->ethtool_ops->get_pauseparam, 
                                dev, &pauseparam);
#else
        dev->ethtool_ops->get_pauseparam(dev, &pauseparam);
#endif

	if (copy_to_user(useraddr, &pauseparam, sizeof(pauseparam)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_pauseparam(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_pauseparam pauseparam;
#if defined(__VMKLNX__)
        int ret;
#endif

	if (!dev->ethtool_ops->set_pauseparam)
		return -EOPNOTSUPP;

	if (copy_from_user(&pauseparam, useraddr, sizeof(pauseparam)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, 
                           dev->ethtool_ops->set_pauseparam, dev, &pauseparam);
        
	return ret; 
#else
        return dev->ethtool_ops->set_pauseparam(dev, &pauseparam);
#endif
}

static int ethtool_get_rx_csum(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GRXCSUM };

	if (!dev->ethtool_ops->get_rx_csum)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, 
                           dev->ethtool_ops->get_rx_csum, dev);
#else
        edata.data = dev->ethtool_ops->get_rx_csum(dev);
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_rx_csum(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;

	if (!dev->ethtool_ops->set_rx_csum)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->ethtool_ops->set_rx_csum, 
                                dev, edata.data);
#else
        dev->ethtool_ops->set_rx_csum(dev, edata.data);
#endif
	return 0;
}

static int ethtool_get_tx_csum(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GTXCSUM };

	if (!dev->ethtool_ops->get_tx_csum)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, 
                           dev->ethtool_ops->get_tx_csum, dev);
#else
        edata.data = dev->ethtool_ops->get_tx_csum(dev);
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int __ethtool_set_sg(struct net_device *dev, u32 data)
{
	int err;

	if (!data && dev->ethtool_ops->set_tso) {
#if defined(__VMKLNX__)
                VMKAPI_MODULE_CALL(dev->module_id, err, 
                                   dev->ethtool_ops->set_tso, dev, 0);
#else
                err = dev->ethtool_ops->set_tso(dev, 0); 
#endif
		if (err)
			return err;
	}

	if (!data && dev->ethtool_ops->set_ufo) {
#if defined(__VMKLNX__)
                VMKAPI_MODULE_CALL(dev->module_id, err,
                                   dev->ethtool_ops->set_ufo, dev, 0);
#else
                err = dev->ethtool_ops->set_ufo(dev, 0);
#endif
		if (err)
			return err;
	}
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_sg, dev, 
                           data);
	return err;
#else
        return dev->ethtool_ops->set_sg(dev, data);
#endif
}

static int ethtool_set_tx_csum(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;
	int err;

	if (!dev->ethtool_ops->set_tx_csum)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	if (!edata.data && dev->ethtool_ops->set_sg) {
		err = __ethtool_set_sg(dev, 0);
		if (err)
			return err;
	}

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_tx_csum, 
                           dev, edata.data);
	return err;
#else
        return dev->ethtool_ops->set_tx_csum(dev, edata.data); 
#endif
}

static int ethtool_get_sg(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GSG };

	if (!dev->ethtool_ops->get_sg)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, dev->ethtool_ops->get_sg,
                           dev);
#else
        edata.data = dev->ethtool_ops->get_sg(dev);
#endif
	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_sg(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;

	if (!dev->ethtool_ops->set_sg)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	if (edata.data && 
	    !(dev->features & NETIF_F_ALL_CSUM))
		return -EINVAL;

	return __ethtool_set_sg(dev, edata.data);
}

static int ethtool_get_tso(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GTSO };

	if (!dev->ethtool_ops->get_tso)
		return -EOPNOTSUPP;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, 
                           dev->ethtool_ops->get_tso, dev);
#else
        edata.data = dev->ethtool_ops->get_tso(dev); 
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_tso(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;
#if defined(__VMKLNX__)
        int err;
#endif

	if (!dev->ethtool_ops->set_tso)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	if (edata.data && !(dev->features & NETIF_F_SG))
		return -EINVAL;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_tso, 
                           dev, edata.data);
	return err;
#else
        return dev->ethtool_ops->set_tso(dev, edata.data); 
#endif
}

static int ethtool_get_ufo(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GUFO };

	if (!dev->ethtool_ops->get_ufo)
		return -EOPNOTSUPP;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, edata.data, 
                           dev->ethtool_ops->get_ufo, dev);
#else
        edata.data = dev->ethtool_ops->get_ufo(dev);
#endif

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		 return -EFAULT;
	return 0;
}

static int ethtool_set_ufo(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;
#if defined(__VMKLNX__)
        int err;
#endif

	if (!dev->ethtool_ops->set_ufo)
		return -EOPNOTSUPP;
	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;
	if (edata.data && !(dev->features & NETIF_F_SG))
		return -EINVAL;
	if (edata.data && !(dev->features & NETIF_F_HW_CSUM))
		return -EINVAL;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->set_ufo, 
                           dev, edata.data);
	return err;
#else
        return dev->ethtool_ops->set_ufo(dev, edata.data);
#endif
}

static int ethtool_get_gso(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GGSO };

	edata.data = dev->features & NETIF_F_GSO;
	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		 return -EFAULT;
	return 0;
}

static int ethtool_set_gso(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_value edata;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;
	if (edata.data)
		dev->features |= NETIF_F_GSO;
	else
		dev->features &= ~NETIF_F_GSO;
	return 0;
}

static int ethtool_self_test(struct net_device *dev, char __user *useraddr)
{
	struct ethtool_test test;
	struct ethtool_ops *ops = dev->ethtool_ops;
	u64 *data;
	int ret;

	if (!ops->self_test || !ops->self_test_count)
		return -EOPNOTSUPP;

	if (copy_from_user(&test, useraddr, sizeof(test)))
		return -EFAULT;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, test.len, ops->self_test_count, dev);
#else
        test.len = ops->self_test_count(dev);
#endif
	data = kmalloc(test.len * sizeof(u64), GFP_USER);
	if (!data)
		return -ENOMEM;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->self_test, dev, &test, 
                                data);
#else
        ops->self_test(dev, &test, data); 
#endif

	ret = -EFAULT;
	if (copy_to_user(useraddr, &test, sizeof(test)))
		goto out;
	useraddr += sizeof(test);
	if (copy_to_user(useraddr, data, test.len * sizeof(u64)))
		goto out;
	ret = 0;

 out:
	kfree(data);
	return ret;
}

static int ethtool_get_strings(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_gstrings gstrings;
	struct ethtool_ops *ops = dev->ethtool_ops;
	u8 *data;
	int ret;

	if (!ops->get_strings)
		return -EOPNOTSUPP;

	if (copy_from_user(&gstrings, useraddr, sizeof(gstrings)))
		return -EFAULT;

	if (ops->get_sset_count) {
#if defined(__VMKLNX__)
      		VMKAPI_MODULE_CALL(dev->module_id, ret, ops->get_sset_count, dev, gstrings.string_set);
#else
		ret = ops->get_sset_count(dev, gstrings.string_set);
#endif
		if (ret < 0)
			return ret;

		gstrings.len = ret;
	} else {
		/* code path for obsolete hooks */

		switch (gstrings.string_set) {
		case ETH_SS_TEST:
			if (!ops->self_test_count)
				return -EOPNOTSUPP;
#if defined(__VMKLNX__)
			VMKAPI_MODULE_CALL(dev->module_id, gstrings.len, 
					   ops->self_test_count, dev);
#else
			gstrings.len = ops->self_test_count(dev);
#endif
			break;
		case ETH_SS_STATS:
			if (!ops->get_stats_count)
				return -EOPNOTSUPP;
#if defined(__VMKLNX__)
			VMKAPI_MODULE_CALL(dev->module_id, gstrings.len,
					   ops->get_stats_count, dev);
#else
			gstrings.len = ops->get_stats_count(dev);
#endif
			break;
		default:
			return -EINVAL;
		}
	}

	data = kmalloc(gstrings.len * ETH_GSTRING_LEN, GFP_USER);
	if (!data)
		return -ENOMEM;
#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_strings, dev, 
                                gstrings.string_set, data);
#else
        ops->get_strings(dev, gstrings.string_set, data);
#endif

	ret = -EFAULT;
	if (copy_to_user(useraddr, &gstrings, sizeof(gstrings)))
		goto out;
	useraddr += sizeof(gstrings);
	if (copy_to_user(useraddr, data, gstrings.len * ETH_GSTRING_LEN))
		goto out;
	ret = 0;

 out:
	kfree(data);
	return ret;
}

static int ethtool_phys_id(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_value id;
#if defined(__VMKLNX__)
        int err;
#endif

	if (!dev->ethtool_ops->phys_id)
		return -EOPNOTSUPP;

	if (copy_from_user(&id, useraddr, sizeof(id)))
		return -EFAULT;

#if defined(__VMKLNX__)
        /*
         * Fix for PR261564.
         *
         * When unspecified, limit the blinking time to 2 seconds
         * because ethtool cannot be killed on Visor.
         */
        if (id.data == 0) {
           id.data = 2; /* seconds */
        }
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, err, dev->ethtool_ops->phys_id, 
                           dev, id.data);
	return err;
#else
        return dev->ethtool_ops->phys_id(dev, id.data); 
#endif
}

static int ethtool_get_stats(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_stats stats;
	struct ethtool_ops *ops = dev->ethtool_ops;
	u64 *data;
	int ret, n_stats;

	if (!ops->get_ethtool_stats)
		return -EOPNOTSUPP;
	if (!ops->get_sset_count && !ops->get_stats_count)
		return -EOPNOTSUPP;

	if (ops->get_sset_count) {
#if defined(__VMKLNX__)
      		VMKAPI_MODULE_CALL(dev->module_id, n_stats, ops->get_sset_count, dev, ETH_SS_STATS);
#else
		n_stats = ops->get_sset_count(dev, ETH_SS_STATS);
#endif
	} else {
		/* code path for obsolete hook */
#if defined(__VMKLNX__)
		VMKAPI_MODULE_CALL(dev->module_id, n_stats, ops->get_stats_count,
				   dev);
#else
		n_stats = ops->get_stats_count(dev);
#endif
	}
	if (n_stats < 0)
		return n_stats;
	WARN_ON(n_stats == 0);

	if (copy_from_user(&stats, useraddr, sizeof(stats)))
		return -EFAULT;

	stats.n_stats = n_stats;
	data = kmalloc(n_stats * sizeof(u64), GFP_USER);
	if (!data)
		return -ENOMEM;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_ethtool_stats, dev, 
                                &stats, data);
#else
        ops->get_ethtool_stats(dev, &stats, data);
#endif

	ret = -EFAULT;
	if (copy_to_user(useraddr, &stats, sizeof(stats)))
		goto out;
	useraddr += sizeof(stats);
	if (copy_to_user(useraddr, data, stats.n_stats * sizeof(u64)))
		goto out;
	ret = 0;

 out:
	kfree(data);
	return ret;
}

static int ethtool_get_perm_addr(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_perm_addr epaddr;
	u8 *data;
	int ret;

	if (!dev->ethtool_ops->get_perm_addr)
		return -EOPNOTSUPP;

	if (copy_from_user(&epaddr,useraddr,sizeof(epaddr)))
		return -EFAULT;

	data = kmalloc(epaddr.size, GFP_USER);
	if (!data)
		return -ENOMEM;

#if defined(__VMKLNX__)
        VMKAPI_MODULE_CALL(dev->module_id, ret, dev->ethtool_ops->get_perm_addr, 
                           dev, &epaddr,data);
#else
        ret = dev->ethtool_ops->get_perm_addr(dev,&epaddr,data);
#endif
	if (ret)
		return ret;

	ret = -EFAULT;
	if (copy_to_user(useraddr, &epaddr, sizeof(epaddr)))
		goto out;
	useraddr += sizeof(epaddr);
	if (copy_to_user(useraddr, data, epaddr.size))
		goto out;
	ret = 0;

 out:
	kfree(data);
	return ret;
}

/* The main entry point in this file.  Called from net/core/dev.c */

#if defined( __VMKLNX__)
static int __ethtool_ioctl(struct ifreq *ifr, struct net_device *dev)
#else
int dev_ethtool(struct ifreq *ifr)
#endif
{
#if !defined( __VMKLNX__)
	struct net_device *dev = __dev_get_by_name(ifr->ifr_name);
#endif
	void __user *useraddr = ifr->ifr_data;
	u32 ethcmd;
	int rc;
	unsigned long old_features;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	/*
	 * XXX: This can be pushed down into the ethtool_* handlers that
	 * need it.  Keep existing behaviour for the moment.
	 */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!dev || !netif_device_present(dev))
		return -ENODEV;

	if (!dev->ethtool_ops)
		goto ioctl;

	if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd)))
		return -EFAULT;

	if(dev->ethtool_ops->begin) {
#if defined( __VMKLNX__)
                VMKAPI_MODULE_CALL(dev->module_id, rc, dev->ethtool_ops->begin, 
                                   dev);
		if (rc < 0) {
#else
                if ((rc = dev->ethtool_ops->begin(dev)) < 0) {
#endif
			return rc;
                }
        }

	old_features = dev->features;

	switch (ethcmd) {
	case ETHTOOL_GSET:
		rc = ethtool_get_settings(dev, useraddr);
		break;
	case ETHTOOL_SSET:
		rc = ethtool_set_settings(dev, useraddr);
		break;
	case ETHTOOL_GDRVINFO:
		rc = ethtool_get_drvinfo(dev, useraddr);
		break;
	case ETHTOOL_GREGS:
		rc = ethtool_get_regs(dev, useraddr);
		break;
	case ETHTOOL_GWOL:
		rc = ethtool_get_wol(dev, useraddr);
		break;
	case ETHTOOL_SWOL:
		rc = ethtool_set_wol(dev, useraddr);
		break;
	case ETHTOOL_GMSGLVL:
		rc = ethtool_get_msglevel(dev, useraddr);
		break;
	case ETHTOOL_SMSGLVL:
		rc = ethtool_set_msglevel(dev, useraddr);
		break;
	case ETHTOOL_NWAY_RST:
		rc = ethtool_nway_reset(dev);
		break;
	case ETHTOOL_GLINK:
		rc = ethtool_get_link(dev, useraddr);
		break;
	case ETHTOOL_GEEPROM:
		rc = ethtool_get_eeprom(dev, useraddr);
		break;
	case ETHTOOL_SEEPROM:
		rc = ethtool_set_eeprom(dev, useraddr);
		break;
	case ETHTOOL_GCOALESCE:
		rc = ethtool_get_coalesce(dev, useraddr);
		break;
	case ETHTOOL_SCOALESCE:
		rc = ethtool_set_coalesce(dev, useraddr);
		break;
	case ETHTOOL_GRINGPARAM:
		rc = ethtool_get_ringparam(dev, useraddr);
		break;
	case ETHTOOL_SRINGPARAM:
		rc = ethtool_set_ringparam(dev, useraddr);
		break;
	case ETHTOOL_GPAUSEPARAM:
		rc = ethtool_get_pauseparam(dev, useraddr);
		break;
	case ETHTOOL_SPAUSEPARAM:
		rc = ethtool_set_pauseparam(dev, useraddr);
		break;
	case ETHTOOL_GRXCSUM:
		rc = ethtool_get_rx_csum(dev, useraddr);
		break;
	case ETHTOOL_SRXCSUM:
		rc = ethtool_set_rx_csum(dev, useraddr);
		break;
	case ETHTOOL_GTXCSUM:
		rc = ethtool_get_tx_csum(dev, useraddr);
		break;
	case ETHTOOL_STXCSUM:
		rc = ethtool_set_tx_csum(dev, useraddr);
		break;
	case ETHTOOL_GSG:
		rc = ethtool_get_sg(dev, useraddr);
		break;
	case ETHTOOL_SSG:
		rc = ethtool_set_sg(dev, useraddr);
		break;
	case ETHTOOL_GTSO:
		rc = ethtool_get_tso(dev, useraddr);
		break;
	case ETHTOOL_STSO:
		rc = ethtool_set_tso(dev, useraddr);
		break;
	case ETHTOOL_TEST:
		rc = ethtool_self_test(dev, useraddr);
		break;
	case ETHTOOL_GSTRINGS:
		rc = ethtool_get_strings(dev, useraddr);
		break;
	case ETHTOOL_PHYS_ID:
		rc = ethtool_phys_id(dev, useraddr);
		break;
	case ETHTOOL_GSTATS:
		rc = ethtool_get_stats(dev, useraddr);
		break;
	case ETHTOOL_GPERMADDR:
		rc = ethtool_get_perm_addr(dev, useraddr);
		break;
	case ETHTOOL_GUFO:
		rc = ethtool_get_ufo(dev, useraddr);
		break;
	case ETHTOOL_SUFO:
		rc = ethtool_set_ufo(dev, useraddr);
		break;
	case ETHTOOL_GGSO:
		rc = ethtool_get_gso(dev, useraddr);
		break;
	case ETHTOOL_SGSO:
		rc = ethtool_set_gso(dev, useraddr);
		break;
	default:
		rc =  -EOPNOTSUPP;
	}
	
	if(dev->ethtool_ops->complete)
#if defined( __VMKLNX__)
                VMKAPI_MODULE_CALL_VOID(dev->module_id, 
                                        dev->ethtool_ops->complete, dev);
#else
                dev->ethtool_ops->complete(dev);
#endif /* defined(__VMKLNX__)  */

#if !defined( __VMKLNX__)
	if (old_features != dev->features)
		netdev_features_change(dev);
#endif /* defined(__VMKLNX__)  */

	return rc;

 ioctl:
	if (dev->do_ioctl) {
#if defined( __VMKLNX__)
                VMKAPI_MODULE_CALL(dev->module_id, rc, dev->do_ioctl, dev, ifr, 
                                   SIOCETHTOOL);
                return rc;
#else
                return dev->do_ioctl(dev, ifr, SIOCETHTOOL);
#endif /* defined(__VMKLNX__)  */
        }
	return -EOPNOTSUPP;
}

#if defined( __VMKLNX__)
int dev_ethtool(struct ifreq *ifr)
{
        int ret;
	struct net_device *dev = dev_get_by_name(ifr->ifr_name);

        if (!dev) {
           return -ENODEV;
        }

        ret = __ethtool_ioctl(ifr, dev);
        dev_put(dev);
        return ret;
}

int vmklnx_ethtool_ioctl(struct net_device *dev, struct ifreq *ifr, uint32_t *result,
                         vmk_IoctlCallerSize callerSize)
{
         void *useraddr;
         u32 ethcmd;

         /*
          * 32-64 bit Conversion lifted from fs/compat_ioctl.c of Linux.
          * As in in this original, this conversion avoids repacking the
          * ioctl data/result by taking advantage of the similarily in
          * format between "struct ifreq" and "struct ifreq32".
          */
         if (callerSize == VMK_IOCTL_CALLER_32) {
            struct ifreq32 *ifr32;
            
            ifr32 = (struct ifreq32 *)ifr;
            useraddr = compat_ptr(ifr32->ifr_ifru.ifru_data);
            ifr->ifr_data = useraddr;
         } else {
            useraddr = ifr->ifr_data;
         }
    
         if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd))) {
            return VMK_INVALID_ADDRESS;
         }
         
         switch (ethcmd) {
            /*
             * allow get options and only those set options which
             * send notification to vmkernel and are handled
             */
            case ETHTOOL_GSET:
            case ETHTOOL_SSET:
            case ETHTOOL_GDRVINFO:
            case ETHTOOL_GREGS:
            case ETHTOOL_GWOL:
            case ETHTOOL_SWOL:
            case ETHTOOL_GMSGLVL:
            case ETHTOOL_SMSGLVL:
            case ETHTOOL_NWAY_RST:
            case ETHTOOL_GLINK:
            case ETHTOOL_GEEPROM:
            case ETHTOOL_SEEPROM:
            case ETHTOOL_GCOALESCE:
            case ETHTOOL_SCOALESCE:
            case ETHTOOL_GRINGPARAM:
            case ETHTOOL_SRINGPARAM:
            case ETHTOOL_GPAUSEPARAM:
            case ETHTOOL_SPAUSEPARAM:
            case ETHTOOL_GRXCSUM:
            case ETHTOOL_SRXCSUM:
            case ETHTOOL_GTXCSUM:
            //case ETHTOOL_STXCSUM:
            case ETHTOOL_GSG:
            //case ETHTOOL_SSG:
            //case ETHTOOL_TEST:
            case ETHTOOL_GSTRINGS:
            case ETHTOOL_PHYS_ID:
            case ETHTOOL_GSTATS:
            case ETHTOOL_GTSO:
            //case ETHTOOL_STSO:
            case ETHTOOL_GPERMADDR:
            //case ETHTOOL_GUFO:
            //case ETHTOOL_SUFO:
            //case ETHTOOL_GGSO:
            //case ETHTOOL_SGSO:
            //case ETHTOOL_GFLAGS:
            //case ETHTOOL_SFLAGS:
            //case ETHTOOL_GPFLAGS:
            //case ETHTOOL_SPFLAGS:
               break;
            default:
               return VMK_NOT_SUPPORTED;
         } 

         *result = __ethtool_ioctl(ifr, dev);

         return vmklnx_errno_to_vmk_return_status(*result);
}
#endif

#if !defined( __VMKLNX__)
EXPORT_SYMBOL(dev_ethtool);
#endif
EXPORT_SYMBOL(ethtool_op_get_link);
EXPORT_SYMBOL_GPL(ethtool_op_get_perm_addr);
EXPORT_SYMBOL(ethtool_op_get_sg);
EXPORT_SYMBOL(ethtool_op_get_tso);
EXPORT_SYMBOL(ethtool_op_get_tx_csum);
EXPORT_SYMBOL(ethtool_op_set_sg);
EXPORT_SYMBOL(ethtool_op_set_tso);
EXPORT_SYMBOL(ethtool_op_set_tx_csum);
EXPORT_SYMBOL(ethtool_op_set_tx_hw_csum);
EXPORT_SYMBOL(ethtool_op_set_ufo);
EXPORT_SYMBOL(ethtool_op_get_ufo);
