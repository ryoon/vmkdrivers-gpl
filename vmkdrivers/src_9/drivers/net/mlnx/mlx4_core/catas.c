/*
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2008, 2011-2013 Mellanox Technologies.
 * All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/module.h>

#include "mlx4.h"

enum {
	MLX4_CATAS_POLL_INTERVAL	= 5 * HZ,
};

static DEFINE_SPINLOCK(catas_lock);

static LIST_HEAD(catas_list);
static struct work_struct catas_work;

#ifndef __VMKERNEL_MODULE__
static int internal_err_reset = 1;
module_param(internal_err_reset, int, 0644);
MODULE_PARM_DESC(internal_err_reset,
		 "Reset device on internal errors if non-zero"
		 " (default 1, in SRIOV mode default is 0)");
#else /* __VMKERNEL_MODULE__ */
static int internal_err_reset;
#endif	/* __VMKERNEL_MODULE__ */

static void dump_err_buf(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	int i;

	mlx4_err(dev, "Internal error detected:\n");
	for (i = 0; i < priv->fw.catas_size; ++i)
		mlx4_err(dev, "  buf[%02x]: %08x\n",
			 i, swab32(readl(priv->catas_err.map + i)));
}

static void poll_catas(unsigned long dev_ptr)
{
	struct mlx4_dev *dev = (struct mlx4_dev *) dev_ptr;
	struct mlx4_priv *priv = mlx4_priv(dev);

	if (readl(priv->catas_err.map)) {
		dump_err_buf(dev);

		mlx4_dispatch_event(dev, MLX4_DEV_EVENT_CATASTROPHIC_ERROR, 0);

		if (internal_err_reset) {
			spin_lock(&catas_lock);
			list_add(&priv->catas_err.list, &catas_list);
			spin_unlock(&catas_lock);

			queue_work(mlx4_wq, &catas_work);
		}
	} else
		mod_timer(&priv->catas_err.timer,
			  round_jiffies(jiffies + MLX4_CATAS_POLL_INTERVAL));
}

static void catas_reset(struct work_struct *work)
{
	struct mlx4_priv *priv, *tmppriv;
	struct mlx4_dev *dev;

	LIST_HEAD(tlist);
	int ret;

	spin_lock_irq(&catas_lock);
	list_splice_init(&catas_list, &tlist);
	spin_unlock_irq(&catas_lock);

	list_for_each_entry_safe(priv, tmppriv, &tlist, catas_err.list) {
		struct pci_dev *pdev = priv->dev.pdev;

		ret = mlx4_restart_one(priv->dev.pdev);
		/* 'priv' now is not valid */
		if (ret)
			pr_err("mlx4 %s: Reset failed (%d)\n",
			       pci_name(pdev), ret);
		else {
			dev  = pci_get_drvdata(pdev);
			mlx4_dbg(dev, "Reset succeeded\n");
		}
	}
}

void mlx4_start_catas_poll(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	phys_addr_t addr;

	/*If we are in SRIOV the default of the module param must be 0*/
	if (mlx4_is_mfunc(dev))
		internal_err_reset = 0;

	INIT_LIST_HEAD(&priv->catas_err.list);
	init_timer(&priv->catas_err.timer);
	priv->catas_err.map = NULL;

	addr = pci_resource_start(dev->pdev, priv->fw.catas_bar) +
		priv->fw.catas_offset;

	priv->catas_err.map = ioremap(addr, priv->fw.catas_size * 4);
	if (!priv->catas_err.map) {
		mlx4_warn(dev, "Failed to map internal error buffer at 0x%llx\n",
			  (unsigned long long) addr);
		return;
	}

	priv->catas_err.timer.data     = (unsigned long) dev;
	priv->catas_err.timer.function = poll_catas;
	priv->catas_err.timer.expires  =
		round_jiffies(jiffies + MLX4_CATAS_POLL_INTERVAL);
	add_timer(&priv->catas_err.timer);
}

void mlx4_stop_catas_poll(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	del_timer_sync(&priv->catas_err.timer);

	if (priv->catas_err.map)
		iounmap(priv->catas_err.map);

	spin_lock_irq(&catas_lock);
	list_del(&priv->catas_err.list);
	spin_unlock_irq(&catas_lock);
}

void  __init mlx4_catas_init(void)
{
	INIT_WORK(&catas_work, catas_reset);
}
