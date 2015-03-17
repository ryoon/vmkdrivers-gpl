/*
 * Copyright (c) 2007-2013 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/slab.h>

#include <linux/mlx4/driver.h>
#include <linux/mlx4/device.h>
#include <linux/mlx4/cmd.h>
#ifdef __VMKERNEL_MODULE__
#include <linux/log2.h>
#endif	/* __VMKERNEL_MODULE__ */

#include "mlx4_en.h"

MODULE_AUTHOR("Liran Liss, Yevgeny Petrilin");
MODULE_DESCRIPTION("Mellanox ConnectX HCA Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION " ("DRV_RELDATE")");

static const char mlx4_en_version[] =
	DRV_NAME ": Mellanox ConnectX HCA Ethernet driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

#define MLX4_EN_PARM_INT(X, def_val, desc) \
	static unsigned int X = def_val;\
	module_param(X , uint, 0444); \
	MODULE_PARM_DESC(X, desc);


/*
 * Device scope module parameters
 */

/* Enable RSS UDP traffic */
MLX4_EN_PARM_INT(udp_rss, 1,
		 "Enable RSS for incomming UDP traffic or disabled (0)");

/* Priority pausing */
MLX4_EN_PARM_INT(pfctx, 0, "Priority based Flow Control policy on TX[7:0]."
			   " Per priority bit mask");
MLX4_EN_PARM_INT(pfcrx, 0, "Priority based Flow Control policy on RX[7:0]."
			   " Per priority bit mask");

#ifdef __VMKERNEL_MODULE__
MLX4_EN_PARM_INT(use_rx_frags, 0, "Enable RX frags or disabled (0), default: 0");
#endif  /* __VMKERNEL_MODULE__ */

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
MLX4_EN_PARM_INT(netq, 1,
		 "Enable netqueue or disabled (0), default: 1");
#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
MLX4_EN_PARM_INT(netq_num_rings_per_rss, DEFAULT_NETQ_NUM_RINGS_PER_RSS,
		 "Number of rings per RSS netq\n"
		 "    valid values: [0, 2, 4]\n"
		 "    default: "__stringify(DEFAULT_NETQ_NUM_RINGS_PER_RSS));
#endif  /* __VMKERNEL_RSS_NETQ_SUPPORT__ */
#endif	/* NET QUEUE */

#ifndef __VMKERNEL_MODULE__
int en_print(const char *level, const struct mlx4_en_priv *priv,
	     const char *format, ...)
{
	va_list args;
	struct va_format vaf;
	int i;

	va_start(args, format);

	vaf.fmt = format;
	vaf.va = &args;
	if (priv->registered)
		i = printk("%s%s: %s: %pV",
			   level, DRV_NAME, priv->dev->name, &vaf);
	else
		i = printk("%s%s: %s: Port %d: %pV",
			   level, DRV_NAME, dev_name(&priv->mdev->pdev->dev),
			   priv->port, &vaf);
	va_end(args);

	return i;
}
#endif	/* NOT __VMKERNEL_MODULE__ */

static void mlx4_en_validate_params(struct mlx4_en_dev *mdev)
{
#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
	if (!netq) {
		/* not using netq so no reason for netq RSS */
		mlx4_warn(mdev, "netq is disabled, setting netq_num_rings_per_rss to 0\n");
		netq_num_rings_per_rss = 0;
	} else {
		if (netq_num_rings_per_rss > MAX_NETQ_NUM_RINGS_PER_RSS) {
			mlx4_warn(mdev, "Unable to set netq_num_rings_per_rss to = %u "
				  "since it is too high, Using %u instead\n",
				  netq_num_rings_per_rss, MAX_NETQ_NUM_RINGS_PER_RSS);
			netq_num_rings_per_rss = MAX_NETQ_NUM_RINGS_PER_RSS;
		} else if (netq_num_rings_per_rss < MIN_NETQ_NUM_RINGS_PER_RSS) {
			mlx4_warn(mdev, "Unable to set netq_num_rings_per_rss to = %u "
				  "since it is too low, Using %u instead\n",
				  netq_num_rings_per_rss, MIN_NETQ_NUM_RINGS_PER_RSS);
			netq_num_rings_per_rss = MIN_NETQ_NUM_RINGS_PER_RSS;
		}

		/* netq_num_rings_per_rss must be even */
		if ((netq_num_rings_per_rss % 2) != 0) {
			--netq_num_rings_per_rss;
			mlx4_warn(mdev, "netq_num_rings_per_rss must be of even value, "
				  "setting it to %u\n", netq_num_rings_per_rss);
		}

		/* netq_num_rings_per_rss must be power of 2 */
		if ((netq_num_rings_per_rss != 0) && (!is_power_of_2(netq_num_rings_per_rss))) {
			mlx4_warn(mdev, "netq_num_rings_per_rss must be power of 2 "
				"rounding down to %lu\n", rounddown_pow_of_two(netq_num_rings_per_rss));
			netq_num_rings_per_rss = rounddown_pow_of_two(netq_num_rings_per_rss);
		}
	}
#endif  /* __VMKERNEL_RSS_NETQ_SUPPORT__ */
#endif	/* NET QUEUE */
}

static u32 mlx4_en_calc_rx_ring_num(struct mlx4_en_dev *mdev)
{
	struct mlx4_dev *dev = mdev->dev;
	u32 rx_ring_num;

	if (!dev->caps.comp_pool)
		rx_ring_num = dev->caps.num_comp_vectors;
	else {
		rx_ring_num = dev->caps.comp_pool / dev->caps.num_ports;
		rx_ring_num = min_t(int, rx_ring_num, MAX_MSIX_P_PORT);
	}
	rx_ring_num = min_t(int, rx_ring_num, MAX_RX_RINGS);
	rx_ring_num = max_t(int, rx_ring_num, MIN_RX_RINGS);
	rx_ring_num = rounddown_pow_of_two(rx_ring_num);

	return rx_ring_num;
}

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
static u32 mlx4_en_calc_rings_per_rss(struct mlx4_en_dev *mdev,
					u32 total_rx_ring,
					u32 num_rss_queue,
					u32 requested)
{
	u32 granted = requested;

	if (!requested)
		goto out;

	if (!num_rss_queue) {
		granted = 0;
		goto out;
	}

	/* 1 default ring + 1 regular ring + requested RSS rings */
	if (total_rx_ring < (2 + num_rss_queue * requested)) {

		mlx4_warn(mdev, "not enough free EQs to open netq RSS with "
			  "%u rings per RSS\n", requested);

		/* best effort to open with as many RSS rings as possible */
		while (requested > 2) {
			requested = rounddown_pow_of_two(requested-1);

			/* 1 default ring + 1 regular ring + requested RSS rings */
			if (total_rx_ring >= (2 + num_rss_queue * requested)) {
				mlx4_warn(mdev, "Setting netq_num_rings_per_rss to %u\n",
					  requested);
				granted = requested;
				goto out;
			}
		}

		mlx4_warn(mdev, "disabling netq RSS\n");
		granted = 0;
	}

out:
	return granted;
}
#endif  /* __VMKERNEL_RSS_NETQ_SUPPORT__ */
#endif	/* NET QUEUE */

static int mlx4_en_get_profile(struct mlx4_en_dev *mdev)
{
	struct mlx4_en_profile *params = &mdev->profile;
	u32 rx_ring_num;
	int i;

	mlx4_en_validate_params(mdev);

	rx_ring_num = mlx4_en_calc_rx_ring_num(mdev);

	params->udp_rss = udp_rss;
	if (params->udp_rss && !(mdev->dev->caps.flags
					& MLX4_DEV_CAP_FLAG_UDP_RSS)) {
		mlx4_warn(mdev, "UDP RSS is not supported on this device.\n");
		params->udp_rss = 0;
	}
	for (i = 1; i <= MLX4_MAX_PORTS; i++) {
		params->prof[i].rx_pause = 1;
		params->prof[i].rx_ppp = pfcrx;
		params->prof[i].tx_pause = 1;
		params->prof[i].tx_ppp = pfctx;
		params->prof[i].tx_ring_size = MLX4_EN_DEF_TX_RING_SIZE;
		params->prof[i].rx_ring_size = MLX4_EN_DEF_RX_RING_SIZE;
		params->prof[i].tx_ring_num = MLX4_EN_NUM_TX_RINGS +
			(!!pfcrx) * MLX4_EN_NUM_PPP_RINGS;
		params->prof[i].rss_rings = 0;
		params->prof[i].rx_ring_num = rx_ring_num;

#ifdef __VMKERNEL_MODULE__
		params->prof[i].use_rx_frags = use_rx_frags;
#endif  /* __VMKERNEL_MODULE__ */

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
		params->prof[i].netq = netq;
		params->prof[i].netq_num_rss_queue = 0;
		params->prof[i].netq_num_rings_per_rss = 0;

		if (params->prof[i].netq & (!(mdev->dev->caps.flags &
				MLX4_DEV_CAP_FLAG_VEP_UC_STEER))) {
			mlx4_warn(mdev, "netq mode is not supported on this device\n");
			params->prof[i].netq = 0;
		}
#endif	/* NET QUEUE */
	}

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
	u32 rings_per_rss = mlx4_en_calc_rings_per_rss(mdev, rx_ring_num,
				DEFAULT_NETQ_NUM_RSS_Q, netq_num_rings_per_rss);

	if (rings_per_rss) {
		for (i = 1; i <= MLX4_MAX_PORTS; i++) {
			if (params->prof[i].netq) {
				params->prof[i].netq_num_rss_queue = DEFAULT_NETQ_NUM_RSS_Q;
				params->prof[i].netq_num_rings_per_rss = rings_per_rss;
			}
		}
	}
#endif  /* __VMKERNEL_RSS_NETQ_SUPPORT__ */
#endif	/* NET QUEUE */

	return 0;
}

static void *mlx4_en_get_netdev(struct mlx4_dev *dev, void *ctx, u8 port)
{
	struct mlx4_en_dev *endev = ctx;

	return endev->pndev[port];
}

static void mlx4_en_event(struct mlx4_dev *dev, void *endev_ptr,
			  enum mlx4_dev_event event, unsigned long port)
{
	struct mlx4_en_dev *mdev = (struct mlx4_en_dev *) endev_ptr;
	struct mlx4_en_priv *priv;

	/* check that port param is not a pointer */
	if (port != (port & (unsigned long)0x0FFFF))
		return;

	switch (event) {
	case MLX4_DEV_EVENT_PORT_UP:
		/* To prevent races, we poll the link state in a separate
		  task rather than changing it here */
		if (!mdev->pndev[port])
			return;

		priv = netdev_priv(mdev->pndev[port]);
		priv->link_state = 1;
		queue_work(mdev->workqueue, &priv->linkstate_task);
		break;

	case MLX4_DEV_EVENT_PORT_DOWN:
		/* To prevent races, we poll the link state in a separate
		  task rather than changing it here */
		if (!mdev->pndev[port])
			return;

		priv = netdev_priv(mdev->pndev[port]);
		priv->link_state = 0;
		queue_work(mdev->workqueue, &priv->linkstate_task);
		break;

	case MLX4_DEV_EVENT_CATASTROPHIC_ERROR:
#ifndef __VMKERNEL_MODULE__
		mlx4_err(mdev, "Internal error detected, restarting device\n");
#else /* __VMKERNEL_MODULE__ */
		mlx4_err(mdev, "Internal error detected, please reload the driver manually\n");
#endif /* __VMKERNEL_MODULE__ */
		break;

	default:
		mlx4_warn(mdev, "Unhandled event: %d\n", event);
	}
}

static void mlx4_en_remove(struct mlx4_dev *dev, void *endev_ptr)
{
	struct mlx4_en_dev *mdev = endev_ptr;
	int i;

	mutex_lock(&mdev->state_lock);
	mdev->device_up = false;
	mutex_unlock(&mdev->state_lock);

	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH)
		if (mdev->pndev[i])
			mlx4_en_destroy_netdev(mdev->pndev[i]);

	flush_workqueue(mdev->workqueue);
	destroy_workqueue(mdev->workqueue);
	mlx4_mr_free(dev, &mdev->mr);
	iounmap(mdev->uar_map);
	mlx4_uar_free(dev, &mdev->priv_uar);
	mlx4_pd_free(dev, mdev->priv_pdn);
	kfree(mdev);
}

static void *mlx4_en_add(struct mlx4_dev *dev)
{
	struct mlx4_en_dev *mdev;
	int i;
	int err;

	printk_once(KERN_INFO "%s", mlx4_en_version);

	mdev = kzalloc(sizeof *mdev, GFP_KERNEL);
	if (!mdev) {
		dev_err(&dev->pdev->dev, "Device struct alloc failed, "
			"aborting.\n");
		err = -ENOMEM;
		goto err_free_res;
	}

	if (mlx4_pd_alloc(dev, &mdev->priv_pdn))
		goto err_free_dev;

	if (mlx4_uar_alloc(dev, &mdev->priv_uar))
		goto err_pd;

	mdev->uar_map = ioremap((phys_addr_t) mdev->priv_uar.pfn << PAGE_SHIFT,
				PAGE_SIZE);
	if (!mdev->uar_map)
		goto err_uar;
	spin_lock_init(&mdev->uar_lock);

	mdev->dev = dev;
	mdev->dma_device = &(dev->pdev->dev);
	mdev->pdev = dev->pdev;
	mdev->device_up = false;

	mdev->LSO_support = !!(dev->caps.flags & (1 << 15));
	if (!mdev->LSO_support)
		mlx4_warn(mdev, "LSO not supported, please upgrade to later "
				"FW version to enable LSO\n");

	if (mlx4_mr_alloc(mdev->dev, mdev->priv_pdn, 0, ~0ull,
			 MLX4_PERM_LOCAL_WRITE |  MLX4_PERM_LOCAL_READ,
			 0, 0, &mdev->mr)) {
		mlx4_err(mdev, "Failed allocating memory region\n");
		goto err_map;
	}
	if (mlx4_mr_enable(mdev->dev, &mdev->mr)) {
		mlx4_err(mdev, "Failed enabling memory region\n");
		goto err_mr;
	}

	/* Build device profile according to supplied module parameters */
	err = mlx4_en_get_profile(mdev);
	if (err) {
		mlx4_err(mdev, "Bad module parameters, aborting.\n");
		goto err_mr;
	}

	/* Configure which ports to start according to module parameters */
	mdev->port_cnt = 0;
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH)
		mdev->port_cnt++;

	/* Create our own workqueue for reset/multicast tasks
	 * Note: we cannot use the shared workqueue because of deadlocks caused
	 *       by the rtnl lock */
	mdev->workqueue = create_singlethread_workqueue("mlx4_en");
	if (!mdev->workqueue) {
		err = -ENOMEM;
		goto err_mr;
	}

	/* At this stage all non-port specific tasks are complete:
	 * mark the card state as up */
	mutex_init(&mdev->state_lock);
	mdev->device_up = true;

	/* Setup ports */

	/* Create a netdev for each port */
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		mlx4_info(mdev, "Activating port:%d\n", i);
		if (mlx4_en_init_netdev(mdev, i, &mdev->profile.prof[i]))
			mdev->pndev[i] = NULL;
	}
	return mdev;

err_mr:
	mlx4_mr_free(dev, &mdev->mr);
err_map:
	if (!mdev->uar_map)
		iounmap(mdev->uar_map);
err_uar:
	mlx4_uar_free(dev, &mdev->priv_uar);
err_pd:
	mlx4_pd_free(dev, mdev->priv_pdn);
err_free_dev:
	kfree(mdev);
err_free_res:
	return NULL;
}

static struct mlx4_interface mlx4_en_interface = {
	.add		= mlx4_en_add,
	.remove		= mlx4_en_remove,
	.event		= mlx4_en_event,
	.get_dev	= mlx4_en_get_netdev,
	.protocol	= MLX4_PROT_ETH,
};

static int __init mlx4_en_init(void)
{
	return mlx4_register_interface(&mlx4_en_interface);
}

static void __exit mlx4_en_cleanup(void)
{
	mlx4_unregister_interface(&mlx4_en_interface);
}

module_init(mlx4_en_init);
module_exit(mlx4_en_cleanup);

