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

#include <linux/mlx4/cq.h>
#include <linux/mlx4/qp.h>
#include <linux/mlx4/cmd.h>

#include "mlx4_en.h"

static void mlx4_en_cq_event(struct mlx4_cq *cq, enum mlx4_event event)
{
	return;
}


int mlx4_en_create_cq(struct mlx4_en_priv *priv,
		      struct mlx4_en_cq *cq,
		      int entries, int ring, enum cq_type mode)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	cq->size = entries;
	cq->buf_size = cq->size * mdev->dev->caps.cqe_size;

	cq->ring = ring;
	cq->is_tx = mode;

	err = mlx4_alloc_hwq_res(mdev->dev, &cq->wqres,
				cq->buf_size, 2 * PAGE_SIZE);
	if (err)
		return err;

	err = mlx4_en_map_buffer(&cq->wqres.buf);
	if (err)
		mlx4_free_hwq_res(mdev->dev, &cq->wqres, cq->buf_size);
	else
		cq->buf = (struct mlx4_cqe *) cq->wqres.buf.direct.buf;

	return err;
}

int mlx4_en_activate_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq,
			int cq_idx)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;
	char name[25];

	cq->dev = mdev->pndev[priv->port];
	cq->mcq.set_ci_db  = cq->wqres.db.db;
	cq->mcq.arm_db     = cq->wqres.db.db + 1;
	*cq->mcq.set_ci_db = 0;
	*cq->mcq.arm_db    = 0;
	memset(cq->buf, 0, cq->buf_size);

	if (cq->is_tx == RX) {
		if (mdev->dev->caps.comp_pool) {
			if (!cq->vector) {
				sprintf(name, "%s-%d", priv->dev->name,
					cq->ring);
				/* Set IRQ for specific name (per ring) */
				if (mlx4_assign_eq(mdev->dev, name, &cq->vector)) {
					cq->vector = (cq->ring + 1 + priv->port)
					    % mdev->dev->caps.num_comp_vectors;
					mlx4_warn(mdev, "Failed Assigning an EQ to "
						  "%s ,Falling back to legacy EQ's\n",
						  name);
				}
			}
		} else {
			cq->vector = (cq->ring + 1 + priv->port) %
				mdev->dev->caps.num_comp_vectors;
		}
	} else {
		/* For TX we use the same irq per
		ring we assigned for the RX    */
		struct mlx4_en_cq *rx_cq;

		cq_idx = cq_idx % priv->rx_ring_num;
		rx_cq = &priv->rx_cq[cq_idx];
		cq->vector = rx_cq->vector;
	}

	/* In ESXi we move adding napi to mlx4_en_init_netdev function */
#ifndef __VMKERNEL_MODULE__
	if (cq->is_tx)
		netif_napi_add(cq->dev, &cq->napi,
			       mlx4_en_poll_tx_cq, MLX4_EN_TX_BUDGET);
	else
		netif_napi_add(cq->dev, &cq->napi,
			       mlx4_en_poll_rx_cq, MLX4_EN_RX_BUDGET);
#endif  /* __VMKERNEL_MODULE__ */

	if (!cq->is_tx)
		cq->size = priv->rx_ring[cq->ring].actual_size;

	err = mlx4_cq_alloc(mdev->dev, cq->size, &cq->wqres.mtt, &mdev->priv_uar,
			    cq->wqres.db.dma, &cq->mcq, cq->vector, 0);
	if (err)
		return err;

	cq->mcq.comp  = cq->is_tx ? mlx4_en_tx_irq : mlx4_en_rx_irq;
	cq->mcq.event = mlx4_en_cq_event;

	return 0;
}

void mlx4_en_destroy_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq)
{
	struct mlx4_en_dev *mdev = priv->mdev;

	mlx4_en_unmap_buffer(&cq->wqres.buf);
	mlx4_free_hwq_res(mdev->dev, &cq->wqres, cq->buf_size);
	if (priv->mdev->dev->caps.comp_pool && cq->vector)
		mlx4_release_eq(priv->mdev->dev, cq->vector);
	cq->vector = 0;
	cq->buf_size = 0;
	cq->buf = NULL;
}

void mlx4_en_deactivate_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq)
{
	/* In ESXi we move deleting napi to mlx4_en_destroy_netdev function */
#ifndef __VMKERNEL_MODULE__
	netif_napi_del(&cq->napi);
#endif  /* __VMKERNEL_MODULE__ */

	mlx4_cq_free(priv->mdev->dev, &cq->mcq);
}

/* Set rx cq moderation parameters */
int mlx4_en_set_cq_moder(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq)
{
	return mlx4_cq_modify(priv->mdev->dev, &cq->mcq,
			      cq->moder_cnt, cq->moder_time);
}

int mlx4_en_arm_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq)
{
	mlx4_cq_arm(&cq->mcq, MLX4_CQ_DB_REQ_NOT, priv->mdev->uar_map,
		    &priv->mdev->uar_lock);

	return 0;
}

#ifdef __VMKERNEL_MODULE__
void mlx4_en_add_napi_to_all_cq(struct mlx4_en_priv *priv)
{
	int i;
	struct mlx4_en_cq *cq;

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = &priv->rx_cq[i];
		if (priv->use_rx_frags)
			netif_napi_add(priv->dev, &cq->napi,
					mlx4_en_poll_rx_cq, MLX4_EN_RX_BUDGET);
		else
			netif_napi_add(priv->dev, &cq->napi,
					mlx4_en_poll_rx_cq_skb, MLX4_EN_RX_BUDGET);
	}

	for (i = 0; i < priv->tx_ring_num; i++) {
		cq = &priv->tx_cq[i];
		netif_napi_add(priv->dev, &cq->napi,
				mlx4_en_poll_tx_cq, MLX4_EN_TX_BUDGET);
	}
}

void mlx4_en_delete_napi_from_all_cq(struct mlx4_en_priv *priv)
{
	int i;
	struct mlx4_en_cq *cq;

	for (i = 0; i < priv->tx_ring_num; i++) {
		cq = &priv->tx_cq[i];
		netif_napi_del(&cq->napi);
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = &priv->rx_cq[i];
		netif_napi_del(&cq->napi);
	}
}
#endif  /* __VMKERNEL_MODULE__ */
