/*
 * Copyright (c) 2012-2013 Mellanox Technologies. All rights reserved.
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

#ifdef __VMKERNEL_MODULE__
#include <vmknetddinetq.h>
#include <linux/log2.h>
#endif	/* __VMKERNEL_MODULE__ */

#include "mlx4_en.h"


#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)


#define en_netq_dbg(priv, format, arg...) do {	\
		if (priv)	\
			en_dbg(DRV, priv, "NETQ %s: " format, __func__, ## arg);	\
		else	\
			printk(KERN_DEBUG "NETQ %s: " format, __func__, ## arg);	\
	} while (0)

#define en_netq_err(priv, format, arg...) do {	\
		if (priv)	\
			en_err(priv, "NETQ %s: " format, __func__, ## arg);	\
		else	\
			printk(KERN_ERR "NETQ %s: " format, __func__, ## arg);	\
	} while (0)

#define en_netq_info(priv, format, arg...) do {	\
		if (priv)	\
			en_info(priv, "NETQ %s: " format, __func__, ## arg);	\
		else	\
			printk(KERN_INFO "NETQ %s: " format, __func__, ## arg);	\
	} while (0)


/********************************/
#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
	#define netq_for_each_tx_queue(priv, i)	\
		for (i = 0; i < priv->tx_ring_num; i++)

	#define netq_for_each_tx_standard_queue(priv, i)	\
		for (i = 1; i < priv->tx_ring_num; i++)
#else
	#define netq_for_each_tx_queue(priv, i)	\
		for (i = 0; i < priv->reported_tx_ring_num; i++)

	#define netq_for_each_tx_standard_queue(priv, i)	\
		for (i = 1; i < priv->reported_tx_ring_num; i++)
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */


/********************************/
#define netq_for_each_rx_queue(priv, i)	\
	for (i = 0; i < priv->netq_rx_queue_num; i++)

#define netq_for_each_rss_queue(priv, i)	\
	for (i = priv->netq_rx_queue_num - priv->netq_num_rss_queue; i < priv->netq_rx_queue_num; i++)

/* start from ring 1 because ring 0 is our default queue and we keep it */
#define netq_for_each_rx_standard_queue(priv, i)	\
	for (i = 1; i < priv->netq_rx_queue_num - priv->netq_num_rss_queue; i++)

/* start from ring 1 because ring 0 is our default queue and we keep it */
#define netq_for_each_rx_standard_and_rss_queue(priv, i)	\
	for (i = 1; i < priv->netq_rx_queue_num; i++)

#define netq_for_each_rx_default_and_standard_queue(priv, i)	\
	for (i = 0; i < priv->netq_rx_queue_num - priv->netq_num_rss_queue; i++)


/********************************/
static inline bool mlx4_en_is_zero_mac(u8 *mac)
{
	u8 zero_mac[ETH_ALEN] = {0};

	if (!memcmp(mac, zero_mac, ETH_ALEN))
		return true;
	return false;
}
/********************************/
static int mlx4_en_netqop_get_version(vmknetddi_queueop_get_version_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;
	int err = vmknetddi_queueops_version(args);

	if (err == VMKNETDDI_QUEUEOPS_OK)
		en_netq_info(priv, "version is: %u.%u\n",
				args->major, args->minor);
	else
		en_netq_err(priv, "failed to get version\n");
	return err;
}


/********************************/
static int mlx4_en_netqop_get_features(vmknetddi_queueop_get_features_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	en_netq_info(priv, "get features returned RX/TX QUEUES\n");
	return VMKNETDDI_QUEUEOPS_OK;
}


/********************************/
static int mlx4_en_netqop_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
		args->count = (priv->tx_ring_num > 0) ? (priv->tx_ring_num - 1) : 0;
#else
		args->count = (priv->reported_tx_ring_num > 0) ? (priv->reported_tx_ring_num - 1) : 0;
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */
		en_netq_info(priv, "num tx queues supported is %u\n", args->count);
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		/* without default and with rss ones */
		args->count = (priv->netq_rx_queue_num > 0) ? (priv->netq_rx_queue_num - 1) : 0;
		en_netq_info(priv, "num rx queues supported is %u\n", args->count);
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
static int mlx4_en_netqop_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		en_netq_err(priv, "failed to get tx filters - not supported\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = NETQ_NUM_RX_FILTERS_PER_Q;
		en_netq_info(priv, "num rx filters is %u\n", args->count);
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
static int mlx4_en_alloc_tx_queue(struct mlx4_en_priv *priv,
                vmknetddi_queueops_queueid_t *p_qid,
                u16 *queue_mapping)
{
	struct mlx4_en_netq_tx_map *netq_tx_map = &priv->netq_map.tx_map;
	u32 i;
	int err = VMKNETDDI_QUEUEOPS_ERR;

	mutex_lock(&priv->netq_lock);
#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
	if (netq_tx_map.num_used_queues >= (priv->tx_ring_num - 1)) {
#else
	if (netq_tx_map->num_used_queues >= (priv->reported_tx_ring_num - 1)) {
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */
		en_netq_err(priv, "failed to alloc tx queue - all tx queues are allocated\n");
		goto out;
	}

	netq_for_each_tx_standard_queue(priv, i) {
		if (!netq_tx_map->queue[i].active) {
			netq_tx_map->queue[i].active = true;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
			*queue_mapping = i;
			++netq_tx_map->num_used_queues;
			en_netq_info(priv, "allocated tx queue %u-[%u], queue mapping is %u\n",
					i, *p_qid, *queue_mapping);
			err = VMKNETDDI_QUEUEOPS_OK;
			goto out;
		}
	}

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}

static int mlx4_en_alloc_rx_queue(struct mlx4_en_priv *priv,
		vmknetddi_queueops_queueid_t *p_qid,
		struct napi_struct **pp_napi,
		vmknetddi_queueops_queue_features_t feat)
{
	struct mlx4_en_netq_rx_map *netq_rx_map = &priv->netq_map.rx_map;
	struct mlx4_en_cq *cq = NULL;
	u32 i;
	bool alloc_rss = false;
	int err = VMKNETDDI_QUEUEOPS_ERR;

	mutex_lock(&priv->netq_lock);
	if (netq_rx_map->num_used_queues >= (priv->netq_rx_queue_num - 1)) {
		en_netq_err(priv, "failed to alloc rx queue - all rx queues are allocated\n");
		goto out;
	}

#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
	if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS) {
		/* find free rss */
		netq_for_each_rss_queue(priv, i) {
			if (!netq_rx_map->queue[i].active) {
				en_netq_dbg(priv, "Found free rss queue=%u\n", i);
				alloc_rss = true;
				break;
			}
		}
	}
#endif	/* __VMKERNEL_RSS_NETQ_SUPPORT__ */

	/* if we failed to find free rss queue than we attempt to allocate regular one */
	netq_for_each_rx_standard_and_rss_queue(priv, i) {
		if (alloc_rss && !netq_rx_map->queue[i].is_rss)
			continue;
		if (!netq_rx_map->queue[i].active) {
			netq_rx_map->queue[i].active = true;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(i);
			if (!alloc_rss) {
				/* Not making sense to fill pp_napi for RSS */
				cq = &priv->rx_cq[i];
				*pp_napi = &cq->napi;
			}
			++netq_rx_map->num_used_queues;
			en_netq_info(priv, "allocated %s queue %u-[%u], napi ptr is %p\n",
					alloc_rss ? "rss" : "rx",
					i, *p_qid, *pp_napi);
			err = VMKNETDDI_QUEUEOPS_OK;
			goto out;
		}
	}

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}

static int mlx4_en_netqop_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return mlx4_en_alloc_tx_queue(priv,
		                &args->queueid,
		                &args->queue_mapping);
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return mlx4_en_alloc_rx_queue(priv,
				&args->queueid,
				&args->napi,
				0);
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
static int mlx4_en_netqop_alloc_queue_with_attr(vmknetddi_queueop_alloc_queue_with_attr_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);
	vmknetddi_queueops_queue_features_t feat;
	int i;

	en_netq_info(priv, "request for alloc queue with attribute\n");
	en_netq_dbg(priv, "attributes number=%d\n", args->nattr);
	if (!args->attr || !args->nattr) {
		en_netq_err(priv, "Attributes are invalid! attr(%p), nattr(%d).\n",
				args->attr, args->nattr);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < args->nattr; i++) {
		switch (args->attr[i].type) {
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR: {
			en_netq_err(priv, "attr=%u from type prior is not supported\n", i);
			break;
		}

		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT: {
			feat = args->attr[i].args.features;
			en_netq_dbg(priv, "attr=%u request feat=0x%x\n", i, feat);

			/* Unsupported features */
			if (feat & ~(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS)) {
				en_netq_err(priv, "unsupported feature 0x%x\n",
						feat & ~(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS));
				return VMKNETDDI_QUEUEOPS_ERR;
			}

			if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS) {
				if (args->type != VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
					en_netq_err(priv, "invalid queue type=0x%x, rss feature is only for rx\n",
							args->type);
					break;
				}
				en_netq_info(priv, "trying to allocate rss queue\n");
				return mlx4_en_alloc_rx_queue(priv,
							&args->queueid,
							&args->napi,
							feat);
			}

			/*
			 * Cheking also if we have not feat. This is done to be aligned with
			 * DDK porting guide although it doesn't make sense
			 */
			if (!feat) {
				return mlx4_en_alloc_rx_queue(priv,
							&args->queueid,
							&args->napi,
							0);
			}
			break;
		}

		default: {
			en_netq_err(priv, "invalid attr=%u type=0x%x\n", i, args->attr[i].type);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		}	/* end of switch */
	}

	en_netq_err(priv, "failed to allocate queue with attr\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}
#else
static int mlx4_en_netqop_alloc_queue_with_attr(vmknetddi_queueop_alloc_queue_with_attr_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to alloc queue with attr - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}
#endif	/* __VMKERNEL_RSS_NETQ_SUPPORT__ */


/********************************/
static int mlx4_en_free_tx_queue(struct mlx4_en_priv *priv,
                vmknetddi_queueops_queueid_t vmk_qid)
{
	struct mlx4_en_netq_tx_map *netq_tx_map = &priv->netq_map.tx_map;
	u32 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(vmk_qid);
	int err = VMKNETDDI_QUEUEOPS_ERR;

#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
	if (qid >= priv->tx_ring_num) {
#else
	if ((qid == 0) || (qid >= priv->reported_tx_ring_num)) {
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */
		en_netq_err(priv, "failed to free tx queue %u-[%u] - out of range\n",
				qid, vmk_qid);
		return err;
	}

	mutex_lock(&priv->netq_lock);
	if (!netq_tx_map->queue[qid].active) {
		en_netq_err(priv, "failed to free tx queue %u-[%u] - double free\n",
				qid, vmk_qid);
		goto out;
	}

	netq_tx_map->queue[qid].active = false;
	--netq_tx_map->num_used_queues;
	en_netq_info(priv, "freed tx queue %u-[%u]\n", qid, vmk_qid);
	err = VMKNETDDI_QUEUEOPS_OK;

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}

static int mlx4_en_free_rx_queue(struct mlx4_en_priv *priv,
		vmknetddi_queueops_queueid_t vmk_qid)
{
	struct mlx4_en_netq_rx_map *netq_rx_map = &priv->netq_map.rx_map;
	u32 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(vmk_qid);
	int err = VMKNETDDI_QUEUEOPS_ERR;

	if ((qid == 0) || (qid >= priv->netq_rx_queue_num)) {
		en_netq_err(priv, "failed to free rx queue %u-[%u] - out of range\n",
				qid, vmk_qid);
		return err;
	}

	mutex_lock(&priv->netq_lock);
	if (netq_rx_map->queue[qid].num_used_filters) {
		en_netq_err(priv, "failed to free rx queue %u-[%u] - we have active filters\n",
				qid, vmk_qid);
		goto out;
	}
	if (!netq_rx_map->queue[qid].active) {
		en_netq_err(priv, "failed to free rx queue %u-[%u] - double free\n",
				qid, vmk_qid);
		goto out;
	}

	netq_rx_map->queue[qid].active = false;
	--netq_rx_map->num_used_queues;
	en_netq_info(priv, "freed rx queue %u-[%u]\n", qid, vmk_qid);
	err = VMKNETDDI_QUEUEOPS_OK;

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}

static int mlx4_en_netqop_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return mlx4_en_free_tx_queue(priv,
				args->queueid);
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return mlx4_en_free_rx_queue(priv,
				args->queueid);
	} else {
		en_netq_err(priv, "invalid queue id=0x%x\n", VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid));
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
static int mlx4_en_netqop_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to get queue interrupt vector - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}


/********************************/
static int mlx4_en_netqop_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queue_mapping = 0;
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		en_netq_info(priv, "default tx queue mapping %u, queue id %u-[%u]\n",
				args->queue_mapping, 0, args->queueid);
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->napi = &priv->rx_cq[0].napi;
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		en_netq_info(priv, "default rx napi ptr %p, queue id %u-[%u]\n",
				args->napi, 0, args->queueid);
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
#define MAC_6_PRINT_FMT		"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x"
#define MAC_6_PRINT_ARG(mac)	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]

static int mlx4_en_netqop_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue;
	vmknetddi_queueops_filter_t *p_filter = &args->filter;
	vmknetddi_queueops_filter_class_t class = vmknetddi_queueops_get_filter_class(p_filter);
	u32 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u32 fid = 0;
	int i;
	int err = VMKNETDDI_QUEUEOPS_ERR;
	u64 mac64;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		en_netq_err(priv, "failed to apply rx filter - not rx queue id %u-[%u]\n",
				qid, args->queueid);
		return err;
	}
	if ((qid == 0) || (qid >= priv->netq_rx_queue_num)) {
		en_netq_err(priv, "failed to apply rx filter - queue id %u-[%u] out of range\n",
				qid, args->queueid);
		return err;
	}
	if (class != VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		en_netq_err(priv, "failed to apply rx filter - filter class 0x%x not supported\n",
				class);
		return err;
	}

	mutex_lock(&priv->netq_lock);
	netq_rx_queue = &priv->netq_map.rx_map.queue[qid];
	if (netq_rx_queue->num_used_filters >= NETQ_NUM_RX_FILTERS_PER_Q) {
		en_netq_err(priv, "failed to apply rx filter - all filters for queue id %u-[%u] are allocated\n",
				qid, args->queueid);
		goto out;
	}

	for (i = 0; i < NETQ_NUM_RX_FILTERS_PER_Q; i++) {
		if (mlx4_en_is_zero_mac(netq_rx_queue->filters[i].u.mac)) {
			memcpy(netq_rx_queue->filters[i].u.mac,
					p_filter->u.macaddr, ETH_ALEN * sizeof(u8));
			++netq_rx_queue->num_used_filters;
			fid = i;
			break;
		}
	}

	mutex_lock(&mdev->state_lock);
	if (!priv->port_up) {
		en_netq_err(priv, "failed to apply rx filter for queue id %u-[%u] - port is down\n",
				qid, args->queueid);
		goto err_flt;
	}

	mac64 = mlx4_en_mac_to_u64(netq_rx_queue->filters[fid].u.mac);
	if (mlx4_uc_steer_add(mdev->dev, priv->port,
			mac64,
			&netq_rx_queue->qp.qpn)) {
		en_netq_err(priv, "failed to apply rx filter by MAC addr " MAC_6_PRINT_FMT	\
				", rx queue is %u=[%u]\n",
				MAC_6_PRINT_ARG(((u8 *)netq_rx_queue->filters[fid].u.mac)),
				qid, args->queueid);
		goto err_flt;
	}
	mutex_unlock(&mdev->state_lock);

	netq_rx_queue->filters[fid].active = true;
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(fid);
	en_netq_info(priv, "applied rx filter %u-[%u] by MAC addr " MAC_6_PRINT_FMT	\
			", rx queue is %u-[%u]\n",
			fid, args->filterid,
			MAC_6_PRINT_ARG(((u8 *)netq_rx_queue->filters[fid].u.mac)),
			qid, args->queueid);
	err = VMKNETDDI_QUEUEOPS_OK;
	goto out;

err_flt:
	mutex_unlock(&mdev->state_lock);
	memset(netq_rx_queue->filters[fid].u.mac, 0, ETH_ALEN * sizeof(u8));
	--netq_rx_queue->num_used_filters;

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}


/********************************/
static int mlx4_en_netqop_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue;
	u32 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u32 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	int err = VMKNETDDI_QUEUEOPS_ERR;
	u64 mac64;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		en_netq_err(priv, "failed to remove rx filter - not rx queue id %u-[%u]\n",
				qid, args->queueid);
		return err;
	}
	if ((qid == 0) || (qid >= priv->netq_rx_queue_num)) {
		en_netq_err(priv, "failed to remove rx filter - queue id %u-[%u] out of range\n",
				qid, args->queueid);
		return err;
	}
	if (fid >= NETQ_NUM_RX_FILTERS_PER_Q) {
		en_netq_err(priv, "failed to remove rx filter - filter id %u-[%u] out of range\n",
				fid, args->filterid);
		return err;
	}

	mutex_lock(&priv->netq_lock);
	netq_rx_queue = &priv->netq_map.rx_map.queue[qid];
	if (netq_rx_queue->filters[fid].active) {
		mac64 = mlx4_en_mac_to_u64(netq_rx_queue->filters[fid].u.mac);
		mlx4_uc_steer_release(mdev->dev, priv->port,
				mac64,
				netq_rx_queue->qp.qpn);
		netq_rx_queue->filters[fid].active = false;
	}

	en_netq_info(priv, "removed rx filter %u-[%u] by MAC addr " MAC_6_PRINT_FMT	\
			", rx queue is %u-[%u]\n",
			fid, args->filterid,
			MAC_6_PRINT_ARG(((u8 *)netq_rx_queue->filters[fid].u.mac)),
			qid, args->queueid);
	memset(netq_rx_queue->filters[fid].u.mac, 0, ETH_ALEN * sizeof(u8));
	--netq_rx_queue->num_used_filters;

	err = VMKNETDDI_QUEUEOPS_OK;
	mutex_unlock(&priv->netq_lock);
	return err;
}


/********************************/
static int mlx4_en_netqop_get_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to get stats - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}


/********************************/
static int mlx4_en_netqop_set_tx_priority(vmknetddi_queueop_set_tx_priority_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to set tx priority - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}


/********************************/
static int mlx4_en_netqop_enable_feat(vmknetddi_queueop_enable_feat_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to enable queue feat - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}


/********************************/
static int mlx4_en_netqop_disable_feat(vmknetddi_queueop_disable_feat_args_t *args)
{
	struct mlx4_en_priv *priv = NULL;

	en_netq_err(priv, "failed to disable queue feat - not supported\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}


/********************************/
static int mlx4_en_netqop_get_supported_feat(vmknetddi_queueop_get_sup_feat_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->features = VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE;
		en_netq_info(priv, "tx queues support none queue features\n");
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->features = VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE;
#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__
		if (priv->netq_num_rss_queue) {
			args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS;
			en_netq_info(priv, "rx queues support rss queue features\n");
		} else {
			en_netq_info(priv, "rx queues support none queue features\n");
		}
#else
		en_netq_info(priv, "rx queues support none queue features\n");
#endif
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
static int mlx4_en_netqop_get_supported_filter_class(vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	struct mlx4_en_priv *priv = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->class = VMKNETDDI_QUEUEOPS_FILTER_NONE;
		en_netq_info(priv, "tx queues support none filter classes\n");
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
		en_netq_info(priv, "rx queues support filter class by MAC addr\n");
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		en_netq_err(priv, "invalid queue type=0x%x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}


/********************************/
static int mlx4_en_netq_ops(vmknetddi_queueops_op_t op, void *args)
{
	struct mlx4_en_priv *priv = NULL;
	int err = VMKNETDDI_QUEUEOPS_OK;

	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_VERSION: {
		err = mlx4_en_netqop_get_version(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES: {
		err = mlx4_en_netqop_get_features(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT: {
		err = mlx4_en_netqop_get_queue_count(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT: {
		err = mlx4_en_netqop_get_filter_count(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE: {
		err = mlx4_en_netqop_alloc_queue(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE: {
		err = mlx4_en_netqop_free_queue(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR: {
		err = mlx4_en_netqop_get_queue_vector(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE: {
		err = mlx4_en_netqop_get_default_queue(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER: {
		err = mlx4_en_netqop_apply_rx_filter(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER: {
		err = mlx4_en_netqop_remove_rx_filter(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_STATS: {
		err = mlx4_en_netqop_get_stats(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_SET_TX_PRIORITY: {
		err = mlx4_en_netqop_set_tx_priority(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR: {
		err = mlx4_en_netqop_alloc_queue_with_attr(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_ENABLE_FEAT: {
		err = mlx4_en_netqop_enable_feat(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_DISABLE_FEAT: {
		err = mlx4_en_netqop_disable_feat(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT: {
		err = mlx4_en_netqop_get_supported_feat(args);
		break;
	}
	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS: {
		err = mlx4_en_netqop_get_supported_filter_class(args);
		break;
	}
	default: {
		en_netq_err(priv, "invalid op=0x%x\n", op);
		err = VMKNETDDI_QUEUEOPS_ERR;
		break;
	}
	}
	return err;
}


/********************************/
static int mlx4_en_create_rx_standard_queues(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_map *netq_rx_map = &priv->netq_map.rx_map;
	int i, qpn;
	u32 good_qps = 0, non_default_qps;
	int err = 0;

	en_netq_info(priv, "Configuring net queue steering\n");

	/* Default queue */
	netq_rx_map->base_qpn_default = priv->base_qpn;
	err = mlx4_en_config_rss_qp(priv, netq_rx_map->base_qpn_default, &priv->rx_ring[0],
				    &netq_rx_map->queue[0].state,
				    &netq_rx_map->queue[0].qp);
	if (err)
		return err;
	++good_qps;

	/* Non default queues */
	non_default_qps = priv->netq_rx_queue_num - 1 - priv->netq_num_rss_queue;
	err = mlx4_qp_reserve_range(mdev->dev,
				    non_default_qps,
				    roundup_pow_of_two(non_default_qps),
				    &netq_rx_map->base_qpn_non_default);
	if (err) {
		en_netq_err(priv, "Failed reserving %d qps\n", non_default_qps);
		goto err_qps;
	}
	netq_for_each_rx_standard_queue(priv, i) {
		/* we start from idx 1 to skip default */
		qpn = netq_rx_map->base_qpn_non_default + (i - 1);
		err = mlx4_en_config_rss_qp(priv, qpn, &priv->rx_ring[i],
					    &netq_rx_map->queue[i].state,
					    &netq_rx_map->queue[i].qp);
		if (err)
			goto err_rsv_range;

		++good_qps;
	}

	return 0;

err_rsv_range:
	mlx4_qp_release_range(mdev->dev, netq_rx_map->base_qpn_non_default, non_default_qps);

err_qps:
	for (i = 0; i < good_qps; i++) {
		mlx4_qp_modify(mdev->dev, NULL, netq_rx_map->queue[i].state,
				MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_map->queue[i].qp);
		mlx4_qp_remove(mdev->dev, &netq_rx_map->queue[i].qp);
		mlx4_qp_free(mdev->dev, &netq_rx_map->queue[i].qp);
	}
	return err;
}

static void mlx4_en_destroy_rx_standard_queues(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_map *netq_rx_map = &priv->netq_map.rx_map;
	int i;

	en_netq_info(priv, "Destroying net queue steering\n");

	/* free all qps */
	netq_for_each_rx_default_and_standard_queue(priv, i) {
		mlx4_qp_modify(mdev->dev, NULL, netq_rx_map->queue[i].state,
				MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_map->queue[i].qp);
		mlx4_qp_remove(mdev->dev, &netq_rx_map->queue[i].qp);
		mlx4_qp_free(mdev->dev, &netq_rx_map->queue[i].qp);
	}
	mlx4_qp_release_range(mdev->dev, netq_rx_map->base_qpn_non_default,
			priv->netq_rx_queue_num - 1 - priv->netq_num_rss_queue);
}

#ifdef __VMKERNEL_RSS_NETQ_SUPPORT__


static int mlx4_en_create_rss_queue(struct mlx4_en_priv *priv, u32 index)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue = &priv->netq_map.rx_map.queue[index];
	struct mlx4_qp_context context;
	void *ptr;
	struct mlx4_rss_context *rss_context;
	u8 rss_mask = (MLX4_RSS_IPV4 | MLX4_RSS_TCP_IPV4 | MLX4_RSS_IPV6 |
			MLX4_RSS_TCP_IPV6);
	int i, qpn, ring_index;
	int err = 0;
	int good_qps = 0;
	static const u32 rsskey[10] = { 0xD181C62C, 0xF7F4DB5B, 0x1983A2FC,
			0x943E1ADB, 0xD9389E6B, 0xD1039C2C, 0xA74499AD,
			0x593D56D9, 0xF3253C06, 0x2ADC1FFC};

	err = mlx4_qp_reserve_range(mdev->dev, priv->netq_num_rings_per_rss,
				    priv->netq_num_rings_per_rss,
				    &netq_rx_queue->rss_context.base_rss_qpn);
	if (err) {
		en_netq_err(priv, "Failed reserving %d qps\n", priv->netq_num_rings_per_rss);
		return err;
	}

	ring_index = priv->rx_ring_num - (priv->netq_num_rss_queue * priv->netq_num_rings_per_rss);
	ring_index += (index - ring_index) * priv->netq_num_rings_per_rss;
	for (i = 0; i < priv->netq_num_rings_per_rss; i++) {
		qpn = netq_rx_queue->rss_context.base_rss_qpn + i;
		err = mlx4_en_config_rss_qp(priv, qpn, &priv->rx_ring[ring_index + i],
				&netq_rx_queue->rss_context.state[i],
				&netq_rx_queue->rss_context.qps[i]);
		if (err)
			goto rss_err;
		++good_qps;
	}

	/* Configure RSS indirection qp */
	err = mlx4_qp_reserve_range(mdev->dev, 1, 1, &netq_rx_queue->rss_context.base_indir_qpn);
	if (err) {
		en_netq_err(priv, "Failed reserving %d qps\n", 1);
		goto rss_err;
	}

	err = mlx4_qp_alloc(mdev->dev,
			    netq_rx_queue->rss_context.base_indir_qpn,
			    &netq_rx_queue->qp);
	if (err) {
		en_err(priv, "Failed to allocate RSS indirection QP\n");
		goto indir_range_err;
	}
	netq_rx_queue->qp.event = mlx4_en_sqp_event;
	mlx4_en_fill_qp_context(priv, 0, 0, 0, 1, netq_rx_queue->rss_context.base_indir_qpn,
				priv->rx_ring[i].cqn, &context);

	ptr = ((void *) &context) + offsetof(struct mlx4_qp_context, pri_path)
		+ MLX4_RSS_OFFSET_IN_QPC_PRI_PATH;
	rss_context = ptr;
	rss_context->base_qpn = cpu_to_be32(ilog2(priv->netq_num_rings_per_rss) << 24 |
			(netq_rx_queue->rss_context.base_rss_qpn));
	rss_context->default_qpn = cpu_to_be32(netq_rx_queue->rss_context.base_rss_qpn);
	if (mdev->profile.udp_rss) {
		rss_mask |=  MLX4_RSS_UDP_IPV4 | MLX4_RSS_UDP_IPV6;
		rss_context->base_qpn_udp = rss_context->default_qpn;
	}
	rss_context->flags = rss_mask;
	rss_context->hash_fn = MLX4_RSS_HASH_TOP;
	for (i = 0; i < 10; i++)
		rss_context->rss_key[i] = cpu_to_be32(rsskey[i]);

	err = mlx4_qp_to_ready(mdev->dev, &priv->res.mtt, &context,
			       &netq_rx_queue->qp, &netq_rx_queue->state);
	if (err)
		goto indir_qp_err;

	/* mark as rss */
	netq_rx_queue->is_rss = true;

	return 0;

indir_qp_err:
	mlx4_qp_modify(mdev->dev, NULL, netq_rx_queue->state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_queue->qp);
	mlx4_qp_remove(mdev->dev, &netq_rx_queue->qp);
	mlx4_qp_free(mdev->dev, &netq_rx_queue->qp);

indir_range_err:
	mlx4_qp_release_range(mdev->dev, netq_rx_queue->rss_context.base_indir_qpn, 1);

rss_err:
	for (i = 0; i < good_qps; i++) {
		mlx4_qp_modify(mdev->dev, NULL, netq_rx_queue->rss_context.state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_queue->rss_context.qps[i]);
		mlx4_qp_remove(mdev->dev, &netq_rx_queue->rss_context.qps[i]);
		mlx4_qp_free(mdev->dev, &netq_rx_queue->rss_context.qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, netq_rx_queue->rss_context.base_rss_qpn,
			      priv->netq_num_rings_per_rss);

	return err;
}

static void mlx4_en_destroy_rss_queue(struct mlx4_en_priv *priv, u32 index)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue = &priv->netq_map.rx_map.queue[index];
	int i;

	netq_rx_queue->is_rss = false;
	mlx4_qp_modify(mdev->dev, NULL, netq_rx_queue->state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_queue->qp);
	mlx4_qp_remove(mdev->dev, &netq_rx_queue->qp);
	mlx4_qp_free(mdev->dev, &netq_rx_queue->qp);
	mlx4_qp_release_range(mdev->dev, netq_rx_queue->rss_context.base_indir_qpn, 1);

	for (i = 0; i < priv->netq_num_rings_per_rss; i++) {
		mlx4_qp_modify(mdev->dev, NULL, netq_rx_queue->rss_context.state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &netq_rx_queue->rss_context.qps[i]);
		mlx4_qp_remove(mdev->dev, &netq_rx_queue->rss_context.qps[i]);
		mlx4_qp_free(mdev->dev, &netq_rx_queue->rss_context.qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, netq_rx_queue->rss_context.base_rss_qpn,
			      priv->netq_num_rings_per_rss);
}

static int mlx4_en_create_all_rss_queues(struct mlx4_en_priv *priv)
{
	int i, j, err;

	if (!priv->netq_num_rss_queue)
		return 0;

	en_netq_info(priv, "Configuring net queue rss steering\n");
	netq_for_each_rss_queue(priv, i) {
		err = mlx4_en_create_rss_queue(priv, i);
		if (err)
			goto err;
	}

	return 0;

err:
	netq_for_each_rss_queue(priv, j) {
		if (j >= i)
			break;
		mlx4_en_destroy_rss_queue(priv, j);
	}
	return err;
}

static void mlx4_en_destroy_all_rss_queues(struct mlx4_en_priv *priv)
{
	int i;

	if (!priv->netq_num_rss_queue)
		return;

	en_netq_info(priv, "Destroying net queue rss steering\n");
	netq_for_each_rss_queue(priv, i)
		mlx4_en_destroy_rss_queue(priv, i);
}


#else


static inline int mlx4_en_create_all_rss_queues(struct mlx4_en_priv *priv)
{
	/* vmkernel not support rss netqueue */
	return 0;
}

static inline void mlx4_en_destroy_all_rss_queues(struct mlx4_en_priv *priv)
{
	/* vmkernel not support rss netqueue */
	return;
}


#endif	/* __VMKERNEL_RSS_NETQ_SUPPORT__ */

static int mlx4_en_apply_all_filters(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue;
	int i, j;
	int err = 0;
	u64 mac64;

	/* apply all previous filters */
	en_netq_info(priv, "apply previous filters to all queues\n");
	netq_for_each_rx_standard_and_rss_queue(priv, i) {
		netq_rx_queue = &priv->netq_map.rx_map.queue[i];
		/* we start from idx 1 to skip default */
		en_netq_dbg(priv, "apply %u filters to queue %u\n",
				netq_rx_queue->num_used_filters, i);
		for (j = 0; j < NETQ_NUM_RX_FILTERS_PER_Q; j++) {
			if (!mlx4_en_is_zero_mac(netq_rx_queue->filters[j].u.mac)) {
				mac64 = mlx4_en_mac_to_u64(netq_rx_queue->filters[j].u.mac);
				err = mlx4_uc_steer_add(mdev->dev, priv->port,
						mac64,
						&netq_rx_queue->qp.qpn);
				if (err) {
					en_netq_err(priv, "failed to resume rx filter by MAC addr " MAC_6_PRINT_FMT	\
							", rx queue is %u\n",
							MAC_6_PRINT_ARG(((u8 *)netq_rx_queue->filters[j].u.mac)),
							i);
					goto err_filters;
				}
				netq_rx_queue->filters[j].active = true;
			}
		}
	}

	goto out;

err_filters:
	netq_for_each_rx_standard_and_rss_queue(priv, i) {
		for (j = 0; j < NETQ_NUM_RX_FILTERS_PER_Q; j++) {
			if (!mlx4_en_is_zero_mac(netq_rx_queue->filters[j].u.mac) &&
					netq_rx_queue->filters[j].active) {
				/* remove only the ones we active already */
				netq_rx_queue->filters[j].active = false;
				mac64 = mlx4_en_mac_to_u64(netq_rx_queue->filters[j].u.mac);
				mlx4_uc_steer_release(mdev->dev, priv->port,
						mac64,
						netq_rx_queue->qp.qpn);
			}
		}
	}

out:
	return err;
}

static void mlx4_en_remove_all_filters(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_netq_rx_queue *netq_rx_queue;
	int i, j;
	u64 mac64;

	/* remove all filters */
	en_netq_info(priv, "remove current filters from all queues\n");
	netq_for_each_rx_standard_and_rss_queue(priv, i) {
		netq_rx_queue = &priv->netq_map.rx_map.queue[i];
		/* we start from idx 1 to skip default */
		en_netq_dbg(priv, "removed %u filters from queue %u\n",
				netq_rx_queue->num_used_filters, i);
		for (j = 0; j < NETQ_NUM_RX_FILTERS_PER_Q; j++) {
			if (!mlx4_en_is_zero_mac(netq_rx_queue->filters[j].u.mac)) {
				netq_rx_queue->filters[j].active = false;
				mac64 = mlx4_en_mac_to_u64(netq_rx_queue->filters[j].u.mac);
				mlx4_uc_steer_release(mdev->dev, priv->port,
						mac64,
						netq_rx_queue->qp.qpn);
			}
		}
	}

}

int mlx4_en_config_netq_steer(struct mlx4_en_priv *priv)
{
	int err = -EINVAL;

	mutex_lock(&priv->netq_lock);

	err = mlx4_en_create_rx_standard_queues(priv);
	if (err) {
		en_netq_err(priv, "failed to create netq standard queues\n");
		goto out;
	}

	err = mlx4_en_create_all_rss_queues(priv);
	if (err) {
		en_netq_err(priv, "failed to create netq rss queues\n");
		goto err_standard_queues;
	}

	err = mlx4_en_apply_all_filters(priv);
	if (err) {
		en_netq_err(priv, "failed to apply all filters\n");
		goto err_rss_queues;
	}

	err = 0;
	goto out;

err_rss_queues:
	mlx4_en_destroy_all_rss_queues(priv);

err_standard_queues:
	mlx4_en_destroy_rx_standard_queues(priv);

out:
	mutex_unlock(&priv->netq_lock);
	return err;
}

void mlx4_en_release_netq_steer(struct mlx4_en_priv *priv)
{
	mutex_lock(&priv->netq_lock);
	mlx4_en_remove_all_filters(priv);
	mlx4_en_destroy_all_rss_queues(priv);
	mlx4_en_destroy_rx_standard_queues(priv);
	mutex_unlock(&priv->netq_lock);
}


/********************************/
void mlx4_en_netq_register(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_netq_tx_map *netq_tx_map = &priv->netq_map.tx_map;
	struct mlx4_en_netq_rx_map *netq_rx_map = &priv->netq_map.rx_map;
	u32 i, j;

	mutex_init(&priv->netq_lock);
	netq_tx_map->num_used_queues = 0;
	netq_for_each_tx_queue(priv, i)
		netq_tx_map->queue[i].active = false;
	netq_rx_map->num_used_queues = 0;
	netq_for_each_rx_queue(priv, i) {
		netq_rx_map->queue[i].active = false;
		netq_rx_map->queue[i].is_rss = false;
		netq_rx_map->queue[i].num_used_filters = 0;
		for (j = 0; j < NETQ_NUM_RX_FILTERS_PER_Q; j++) {
			netq_rx_map->queue[i].filters[j].active = false;
			memset(netq_rx_map->queue[i].filters[j].u.mac, 0, ETH_ALEN * sizeof(u8));
		}
	}

	VMKNETDDI_REGISTER_QUEUEOPS(dev, mlx4_en_netq_ops);
	en_netq_info(priv, "registered net queue done\n");
	return;
}


/********************************/

#endif	/* NET QUEUE */
