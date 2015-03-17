/*
 * Copyright (c) 2005-2008, 2011-2013 Mellanox Technologies.
 * All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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

#ifndef MLX4_ICM_H
#define MLX4_ICM_H

#include <linux/list.h>
#include <linux/pci.h>
#include <linux/mutex.h>

#define MLX4_ICM_CHUNK_LEN						\
	((256 - sizeof (struct list_head) - 2 * sizeof (int)) /		\
	 (sizeof (struct scatterlist)))

enum {
	MLX4_ICM_PAGE_SHIFT	= 12,
	MLX4_ICM_PAGE_SIZE	= 1 << MLX4_ICM_PAGE_SHIFT,
};

struct mlx4_icm_chunk {
	struct list_head	list;
	int			npages;
	int			nsg;
#ifdef __VMKERNEL_MODULE__
	struct scatterlist	mem;
	/*
	 * We need to save the virtual address for every page
	 * we are allocating because in vmkernel going back from
	 * physical address to virtual one can give different
	 * virtual address. Both virtual addresses point to same
	 * location but you can dma_free_coherent the memory only
	 * with the original virtual address you received from
	 * dma_alloc_coherent
	 */
	void 			*sg_virt_addr[MLX4_ICM_CHUNK_LEN];
#else
	struct scatterlist	mem[MLX4_ICM_CHUNK_LEN];
#endif	/* __VMKERNEL_MODULE__ */

};

struct mlx4_icm {
	struct list_head	chunk_list;
	int			refcount;
};

struct mlx4_icm_iter {
	struct mlx4_icm	       *icm;
	struct mlx4_icm_chunk  *chunk;
	int			page_idx;
};

struct mlx4_dev;

struct mlx4_icm *mlx4_alloc_icm(struct mlx4_dev *dev, int npages,
				gfp_t gfp_mask, int coherent);
void mlx4_free_icm(struct mlx4_dev *dev, struct mlx4_icm *icm, int coherent);

int mlx4_table_get(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj);
void mlx4_table_put(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj);
int mlx4_table_get_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			 int start, int end);
void mlx4_table_put_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			  int start, int end);
int mlx4_init_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			u64 virt, int obj_size,	int nobj, int reserved,
			int use_lowmem, int use_coherent);
void mlx4_cleanup_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table);
int mlx4_table_get(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj);
void mlx4_table_put(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj);
void *mlx4_table_find(struct mlx4_icm_table *table, int obj, dma_addr_t *dma_handle);
int mlx4_table_get_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			 int start, int end);
void mlx4_table_put_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			  int start, int end);

static inline void mlx4_icm_first(struct mlx4_icm *icm,
				  struct mlx4_icm_iter *iter)
{
	iter->icm      = icm;
	iter->chunk    = list_empty(&icm->chunk_list) ?
		NULL : list_entry(icm->chunk_list.next,
				  struct mlx4_icm_chunk, list);
	iter->page_idx = 0;
#ifdef __VMKERNEL_MODULE__
	/*
	 * We need to reset the current location inside the
	 * scatterlist (the pages list) in vmkernel.
	 * The scatterlist in vmkernel isn't an array but rather
	 * a data structure. We are going on all pages one by one
	 * with sg_next and we return to first one by sg_reset
	 */
	if (iter->chunk)
		sg_reset(&(iter->chunk->mem));
#endif	/* __VMKERNEL_MODULE__ */
}

static inline int mlx4_icm_last(struct mlx4_icm_iter *iter)
{
	return !iter->chunk;
}

static inline void mlx4_icm_next(struct mlx4_icm_iter *iter)
{
	if (++iter->page_idx >= iter->chunk->nsg) {
		if (iter->chunk->list.next == &iter->icm->chunk_list) {
			iter->chunk = NULL;
			return;
		}

		iter->chunk = list_entry(iter->chunk->list.next,
					 struct mlx4_icm_chunk, list);
		iter->page_idx = 0;
#ifdef __VMKERNEL_MODULE__
		sg_reset(&(iter->chunk->mem));
	} else {
		sg_next(&(iter->chunk->mem));
	}
#endif	/* __VMKERNEL_MODULE__ */
}

static inline dma_addr_t mlx4_icm_addr(struct mlx4_icm_iter *iter)
{
#ifdef __VMKERNEL_MODULE__
	return sg_dma_address(&iter->chunk->mem);
#else
	return sg_dma_address(&iter->chunk->mem[iter->page_idx]);
#endif	/* __VMKERNEL_MODULE__ */
}

static inline unsigned long mlx4_icm_size(struct mlx4_icm_iter *iter)
{
#ifdef __VMKERNEL_MODULE__
	return sg_dma_len(&iter->chunk->mem);
#else
	return sg_dma_len(&iter->chunk->mem[iter->page_idx]);
#endif	/* __VMKERNEL_MODULE__ */
}


#ifdef __VMKERNEL_MODULE__
#define MLNX_VMKSGEL(mem)		(mem)->vmksgel
#define MLNX_VMKIOSGEL(mem)		(mem)->vmkIOsgel
#define MLNX_CURSGEL(mem)		(mem)->cursgel
#define MLNX_CURIOSGEL(mem)		(mem)->curIOsgel
#define MLNX_PREMAPED(mem)		(mem)->premapped
#define MLNX_VMKSGA(mem)		(mem)->vmksga
#define MLNX_VMKIOSGA(mem)		(mem)->vmkIOsga

#define MLNX_NUMELEM(sga)		(sga)->numElems
#define MLNX_MAXELEM(sga)		(sga)->maxElems
#define MLNX_VMKSGA_NUMELEM(mem)	MLNX_NUMELEM(MLNX_VMKSGA(mem))
#define MLNX_VMKSGA_MAXELEM(mem)	MLNX_MAXELEM(MLNX_VMKSGA(mem))
#define MLNX_VMKIOSGA_NUMELEM(mem)	MLNX_NUMELEM(MLNX_VMKIOSGA(mem))
#define MLNX_VMKIOSGA_MAXELEM(mem)	MLNX_MAXELEM(MLNX_VMKIOSGA(mem))

#define MLNX_LENGTH(sgel)		(sgel)->length
#define MLNX_CURSGEL_LENGTH(mem)	MLNX_LENGTH(MLNX_CURSGEL(mem))
#define MLNX_CURIOSGEL_LENGTH(mem)	MLNX_LENGTH(MLNX_CURIOSGEL(mem))

#define MLNX_ADDR(sgel)			(sgel)->addr
#define MLNX_CURSGEL_ADDR(mem)		MLNX_ADDR(MLNX_CURSGEL(mem))
#define MLNX_IOADDR(sgel)		(sgel)->ioAddr
#define MLNX_CURIOSGEL_IOADDR(mem)	MLNX_IOADDR(MLNX_CURIOSGEL(mem))

static inline int mlx4_alloc_sga(vmk_SgArray** ptr, gfp_t gfp_mask)
{
	*ptr = kmalloc(sizeof(struct vmk_SgElem) * MLX4_ICM_CHUNK_LEN
			+ sizeof(struct vmk_SgArray), gfp_mask);
	if(!*ptr)
		return -ENOMEM;

	MLNX_MAXELEM(*ptr) = MLX4_ICM_CHUNK_LEN;
	MLNX_NUMELEM(*ptr) = 0;
	return 0;
}

static inline int mlx4_alloc_sgel(vmk_sgelem **ptr, gfp_t gfp_mask)
{
	*ptr = kmalloc(sizeof(vmk_sgelem) * MLX4_ICM_CHUNK_LEN , gfp_mask);
        if(!*ptr)
                return -ENOMEM;

        return 0;
}
#endif	/* __VMKERNEL_MODULE__ */

int mlx4_MAP_ICM_AUX(struct mlx4_dev *dev, struct mlx4_icm *icm);
int mlx4_UNMAP_ICM_AUX(struct mlx4_dev *dev);

#endif /* MLX4_ICM_H */
