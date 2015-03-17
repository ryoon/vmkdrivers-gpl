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

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <linux/mlx4/cmd.h>

#include "mlx4.h"
#include "icm.h"
#include "fw.h"

/*
 * We allocate in as big chunks as we can, up to a maximum of 256 KB
 * per chunk.
 */
enum {
	MLX4_ICM_ALLOC_SIZE	= 1 << 18,
	MLX4_TABLE_CHUNK_SIZE	= 1 << 18
};

static void mlx4_free_icm_pages(struct mlx4_dev *dev, struct mlx4_icm_chunk *chunk)
{
	int i;
#ifdef __VMKERNEL_MODULE__
	struct scatterlist * sg;
	struct scatterlist * mem = &(chunk->mem);
#endif	/* __VMKERNEL_MODULE__ */

	if (chunk->nsg > 0)
#ifdef __VMKERNEL_MODULE__
		pci_unmap_sg(dev->pdev, mem, chunk->npages,
			     PCI_DMA_BIDIRECTIONAL);
#else
		pci_unmap_sg(dev->pdev, chunk->mem, chunk->npages,
			     PCI_DMA_BIDIRECTIONAL);
#endif	/* __VMKERNEL_MODULE__ */

#ifdef __VMKERNEL_MODULE__
	sg_reset(mem);
	for_each_sg(mem , sg , chunk->npages, i) {
		__free_pages(sg_page(mem),
		get_order(MLNX_CURSGEL_LENGTH(mem)));
	}
	kfree(MLNX_VMKSGEL(mem));
	VMKLNX_INIT_VMK_SG(mem, NULL);
#else
	for (i = 0; i < chunk->npages; ++i)
		__free_pages(sg_page(&chunk->mem[i]),
			     get_order(chunk->mem[i].length));
#endif	/* __VMKERNEL_MODULE__ */
}

static void mlx4_free_icm_coherent(struct mlx4_dev *dev, struct mlx4_icm_chunk *chunk)
{
	int i;
#ifdef __VMKERNEL_MODULE__
	struct scatterlist * sg;
	struct scatterlist * mem = &(chunk->mem);

	sg_reset(mem);
	for_each_sg(mem ,sg ,MLNX_VMKSGA_NUMELEM(mem), i) {
		dma_free_coherent(&dev->pdev->dev, MLNX_CURSGEL_LENGTH(mem),
				chunk->sg_virt_addr[i],
				sg_dma_address(mem));
	}

	kfree(MLNX_VMKSGA(mem));
	kfree(MLNX_VMKIOSGA(mem));
	VMKLNX_INIT_VMK_SG(mem, NULL);
#else
	for (i = 0; i < chunk->npages; ++i)
		dma_free_coherent(&dev->pdev->dev, chunk->mem[i].length,
				  lowmem_page_address(sg_page(&chunk->mem[i])),
				  sg_dma_address(&chunk->mem[i]));
#endif	/* __VMKERNEL_MODULE__ */
}

void mlx4_free_icm(struct mlx4_dev *dev, struct mlx4_icm *icm, int coherent)
{
	struct mlx4_icm_chunk *chunk, *tmp;

	if (!icm)
		return;

	list_for_each_entry_safe(chunk, tmp, &icm->chunk_list, list) {
		if (coherent)
			mlx4_free_icm_coherent(dev, chunk);
		else
			mlx4_free_icm_pages(dev, chunk);

		kfree(chunk);
	}

	kfree(icm);
}

static int mlx4_alloc_icm_pages(struct scatterlist *mem, int order, gfp_t gfp_mask)
{
	struct page *page;
#ifdef __VMKERNEL_MODULE__
	vmk_sgelem  *sgel;
#endif	/* __VMKERNEL_MODULE__ */

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return -ENOMEM;

#ifdef __VMKERNEL_MODULE__
	if (MLNX_CURSGEL(mem)) {
		sg_next(mem);
	} else {
		if (mlx4_alloc_sgel(&sgel, gfp_mask)) {
			__free_pages(page, order);
			return -ENOMEM;
		}
		VMKLNX_INIT_VMK_SG(mem, sgel);
	}
#endif	/* __VMKERNEL_MODULE__ */

	sg_set_page(mem, page, PAGE_SIZE << order, 0);
	return 0;
}

#ifdef __VMKERNEL_MODULE__
static int mlx4_alloc_icm_coherent(struct device *dev, void **sg_virt_addr, struct scatterlist *mem,
				    int order, gfp_t gfp_mask)
#else
static int mlx4_alloc_icm_coherent(struct device *dev, struct scatterlist *mem,
				    int order, gfp_t gfp_mask)
#endif	/* __VMKERNEL_MODULE__ */
{
#ifdef __VMKERNEL_MODULE__
	vmk_SgArray *sga = NULL, *IOsga = NULL;
	dma_addr_t  ioAddr;
	int rc = 0;

	if (MLNX_CURSGEL(mem)) {
		sg_next(mem);
	} else {
		rc =  mlx4_alloc_sga(&sga, gfp_mask);
		rc += mlx4_alloc_sga(&IOsga, gfp_mask);
		if (rc) {
			/* kfree accepts NULL ptrs, its O.K.for now */
			kfree(sga);
			kfree(IOsga);
			return -ENOMEM;
		}
		VMKLNX_INIT_VMK_SG_WITH_ARRAYS(mem, sga ,IOsga);
	}
#endif	/* __VMKERNEL_MODULE__ */

#ifdef __VMKERNEL_MODULE__
	void *buf = dma_alloc_coherent(dev, PAGE_SIZE << order,
				       &ioAddr, gfp_mask);
#else
	void *buf = dma_alloc_coherent(dev, PAGE_SIZE << order,
				       &sg_dma_address(mem), gfp_mask);
#endif	/* __VMKERNEL_MODULE__ */
	if (!buf)
		return -ENOMEM;

	sg_set_buf(mem, buf, PAGE_SIZE << order);
#ifdef __VMKERNEL_MODULE__
	MLNX_CURIOSGEL_IOADDR(mem) = ioAddr;
	MLNX_CURIOSGEL_LENGTH(mem) = PAGE_SIZE << order;
	MLNX_VMKSGA_NUMELEM(mem)++;
	MLNX_VMKIOSGA_NUMELEM(mem)++;
	/* Save the original virtual address */
	*sg_virt_addr = buf;
#else
	BUG_ON(mem->offset);
	sg_dma_len(mem) = PAGE_SIZE << order;
#endif	/* __VMKERNEL_MODULE__ */
	return 0;
}

#ifdef __VMKERNEL_MODULE__
struct mlx4_icm *mlx4_alloc_icm(struct mlx4_dev *dev, int npages,
				gfp_t gfp_mask, int coherent)
{
	struct mlx4_icm 	*icm;
	struct mlx4_icm_chunk 	*chunk = NULL;
	int 			cur_order;
	int 			ret;
	struct scatterlist      *mem = NULL;

	/* We use sg_set_buf for coherent allocs, which assumes low memory */
	BUG_ON(coherent && (gfp_mask & __GFP_HIGHMEM));

	icm = kmalloc(sizeof *icm, gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
	if (!icm)
		return NULL;

	icm->refcount = 0;
	INIT_LIST_HEAD(&icm->chunk_list);

	cur_order = get_order(MLX4_ICM_ALLOC_SIZE);

	while (npages > 0) {
		if (!chunk) {
			chunk = kmalloc(sizeof *chunk,
					gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
			if (!chunk)
				goto fail;

			chunk->npages = 0;
			chunk->nsg    = 0;
			mem           = &(chunk->mem);
                        VMKLNX_INIT_VMK_SG(mem , NULL);
			list_add_tail(&chunk->list, &icm->chunk_list);
		}

		while (1 << cur_order > npages)
			--cur_order;

		if (coherent)
			ret = mlx4_alloc_icm_coherent(&dev->pdev->dev,
							&(chunk->sg_virt_addr[chunk->npages]),
							 mem, cur_order, gfp_mask);
		else
			ret = mlx4_alloc_icm_pages(mem, cur_order, gfp_mask);
		if (!ret) {
			++chunk->npages;

			if (chunk->npages == MLX4_ICM_CHUNK_LEN) {
				/*This is also correct if coherent*/
				chunk->nsg = pci_map_sg(dev->pdev, mem,
							chunk->npages,
							PCI_DMA_BIDIRECTIONAL);

				if (chunk->nsg <= 0)
					goto fail;

				chunk = NULL;
			}

			npages -= 1 << cur_order;
		} else {
			--cur_order;
			if (cur_order < 0)
				goto fail;
		}
	}

	if (!coherent && chunk) {
		chunk->nsg = pci_map_sg(dev->pdev, mem,
					chunk->npages,
					PCI_DMA_BIDIRECTIONAL);
		if (chunk->nsg <= 0)
			goto fail;
	}

	return icm;

fail:
	mlx4_free_icm(dev, icm, coherent);
	return NULL;
}
#else
struct mlx4_icm *mlx4_alloc_icm(struct mlx4_dev *dev, int npages,
				gfp_t gfp_mask, int coherent)
{
	struct mlx4_icm *icm;
	struct mlx4_icm_chunk *chunk = NULL;
	int cur_order;
	int ret;

	/* We use sg_set_buf for coherent allocs, which assumes low memory */
	BUG_ON(coherent && (gfp_mask & __GFP_HIGHMEM));

	icm = kmalloc(sizeof *icm, gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
	if (!icm)
		return NULL;

	icm->refcount = 0;
	INIT_LIST_HEAD(&icm->chunk_list);

	cur_order = get_order(MLX4_ICM_ALLOC_SIZE);

	while (npages > 0) {
		if (!chunk) {
			chunk = kmalloc(sizeof *chunk,
					gfp_mask & ~(__GFP_HIGHMEM | __GFP_NOWARN));
			if (!chunk)
				goto fail;

			sg_init_table(chunk->mem, MLX4_ICM_CHUNK_LEN);
			chunk->npages = 0;
			chunk->nsg    = 0;
			list_add_tail(&chunk->list, &icm->chunk_list);
		}

		while (1 << cur_order > npages)
			--cur_order;

		if (coherent)
			ret = mlx4_alloc_icm_coherent(&dev->pdev->dev,
						      &chunk->mem[chunk->npages],
						      cur_order, gfp_mask);
		else
			ret = mlx4_alloc_icm_pages(&chunk->mem[chunk->npages],
						   cur_order, gfp_mask);

		if (ret) {
			if (--cur_order < 0)
				goto fail;
			else
				continue;
		}

		++chunk->npages;

		if (coherent)
			++chunk->nsg;
		else if (chunk->npages == MLX4_ICM_CHUNK_LEN) {
			chunk->nsg = pci_map_sg(dev->pdev, chunk->mem,
						chunk->npages,
						PCI_DMA_BIDIRECTIONAL);

			if (chunk->nsg <= 0)
				goto fail;
		}

		if (chunk->npages == MLX4_ICM_CHUNK_LEN)
			chunk = NULL;

		npages -= 1 << cur_order;
	}

	if (!coherent && chunk) {
		chunk->nsg = pci_map_sg(dev->pdev, chunk->mem,
					chunk->npages,
					PCI_DMA_BIDIRECTIONAL);

		if (chunk->nsg <= 0)
			goto fail;
	}

	return icm;

fail:
	mlx4_free_icm(dev, icm, coherent);
	return NULL;
}
#endif	/* __VMKERNEL_MODULE__ */

static int mlx4_MAP_ICM(struct mlx4_dev *dev, struct mlx4_icm *icm, u64 virt)
{
	return mlx4_map_cmd(dev, MLX4_CMD_MAP_ICM, icm, virt);
}

static int mlx4_UNMAP_ICM(struct mlx4_dev *dev, u64 virt, u32 page_count)
{
	return mlx4_cmd(dev, virt, page_count, 0, MLX4_CMD_UNMAP_ICM,
			MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
}

int mlx4_MAP_ICM_AUX(struct mlx4_dev *dev, struct mlx4_icm *icm)
{
	return mlx4_map_cmd(dev, MLX4_CMD_MAP_ICM_AUX, icm, -1);
}

int mlx4_UNMAP_ICM_AUX(struct mlx4_dev *dev)
{
	return mlx4_cmd(dev, 0, 0, 0, MLX4_CMD_UNMAP_ICM_AUX,
			MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
}

int mlx4_table_get(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj)
{
	int i = (obj & (table->num_obj - 1)) / (MLX4_TABLE_CHUNK_SIZE / table->obj_size);
	int ret = 0;

	mutex_lock(&table->mutex);

	if (table->icm[i]) {
		++table->icm[i]->refcount;
		goto out;
	}

	table->icm[i] = mlx4_alloc_icm(dev, MLX4_TABLE_CHUNK_SIZE >> PAGE_SHIFT,
				       (table->lowmem ? GFP_KERNEL : GFP_HIGHUSER) |
				       __GFP_NOWARN, table->coherent);
	if (!table->icm[i]) {
		ret = -ENOMEM;
		goto out;
	}

	if (mlx4_MAP_ICM(dev, table->icm[i], table->virt +
			 (u64) i * MLX4_TABLE_CHUNK_SIZE)) {
		mlx4_free_icm(dev, table->icm[i], table->coherent);
		table->icm[i] = NULL;
		ret = -ENOMEM;
		goto out;
	}

	++table->icm[i]->refcount;

out:
	mutex_unlock(&table->mutex);
	return ret;
}

void mlx4_table_put(struct mlx4_dev *dev, struct mlx4_icm_table *table, int obj)
{
	int i;

	i = (obj & (table->num_obj - 1)) / (MLX4_TABLE_CHUNK_SIZE / table->obj_size);

	mutex_lock(&table->mutex);

	if (--table->icm[i]->refcount == 0) {
		mlx4_UNMAP_ICM(dev, table->virt + i * MLX4_TABLE_CHUNK_SIZE,
			       MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE);
		mlx4_free_icm(dev, table->icm[i], table->coherent);
		table->icm[i] = NULL;
	}

	mutex_unlock(&table->mutex);
}

#ifdef __VMKERNEL_MODULE__
void *mlx4_table_find(struct mlx4_icm_table *table, int obj, dma_addr_t *dma_handle)
{
	int idx, offset, dma_offset, i;
	struct mlx4_icm_chunk 	*chunk;
	struct mlx4_icm 	*icm;
	void 			*address = NULL;
	struct scatterlist	*sg, *mem;

	if (!table->lowmem)
		return NULL;

	mutex_lock(&table->mutex);
	idx = (obj & (table->num_obj - 1)) * table->obj_size;
	icm = table->icm[idx / MLX4_TABLE_CHUNK_SIZE];
	dma_offset = offset = idx % MLX4_TABLE_CHUNK_SIZE;

	if (!icm)
		goto out;

	list_for_each_entry(chunk, &icm->chunk_list, list) {
		mem = &(chunk->mem);
                sg_reset(mem);
                for_each_sg(mem , sg , MLX4_ICM_CHUNK_LEN, i){
			if (dma_handle && dma_offset >= 0) {
				if (sg_dma_len(mem) > dma_offset)
					*dma_handle = sg_dma_address(mem) +
							dma_offset;
				dma_offset -= sg_dma_len(mem);
			}
			/*
			 * DMA mapping can merge pages but not split them,
			 * so if we found the page, dma_handle has already
			 * been assigned to.
			 */
			if (MLNX_CURSGEL_LENGTH(mem) > offset) {
				address = __va(MLNX_CURSGEL_ADDR(mem));
			        mutex_unlock(&table->mutex);
				return address + offset;
			}
			offset -= MLNX_CURSGEL_LENGTH(mem);
		}
	}

out:
	mutex_unlock(&table->mutex);
	return NULL;
}
#else
void *mlx4_table_find(struct mlx4_icm_table *table, int obj, dma_addr_t *dma_handle)
{
	int idx, offset, dma_offset, i;
	struct mlx4_icm_chunk *chunk;
	struct mlx4_icm *icm;
	struct page *page = NULL;

	if (!table->lowmem)
		return NULL;

	mutex_lock(&table->mutex);

	idx = (obj & (table->num_obj - 1)) * table->obj_size;
	icm = table->icm[idx / MLX4_TABLE_CHUNK_SIZE];
	dma_offset = offset = idx % MLX4_TABLE_CHUNK_SIZE;

	if (!icm)
		goto out;

	list_for_each_entry(chunk, &icm->chunk_list, list) {
		for (i = 0; i < chunk->npages; ++i) {
			if (dma_handle && dma_offset >= 0) {
				if (sg_dma_len(&chunk->mem[i]) > dma_offset)
					*dma_handle = sg_dma_address(&chunk->mem[i]) +
						dma_offset;
				dma_offset -= sg_dma_len(&chunk->mem[i]);
			}
			/*
			 * DMA mapping can merge pages but not split them,
			 * so if we found the page, dma_handle has already
			 * been assigned to.
			 */
			if (chunk->mem[i].length > offset) {
				page = sg_page(&chunk->mem[i]);
				goto out;
			}
			offset -= chunk->mem[i].length;
		}
	}

out:
	mutex_unlock(&table->mutex);
	return page ? lowmem_page_address(page) + offset : NULL;
}
#endif	/* __VMKERNEL_MODULE__ */

int mlx4_table_get_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			 int start, int end)
{
	int inc = MLX4_TABLE_CHUNK_SIZE / table->obj_size;
	int i, err;

	for (i = start; i <= end; i += inc) {
		err = mlx4_table_get(dev, table, i);
		if (err)
			goto fail;
	}

	return 0;

fail:
	while (i > start) {
		i -= inc;
		mlx4_table_put(dev, table, i);
	}

	return err;
}

void mlx4_table_put_range(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			  int start, int end)
{
	int i;

	for (i = start; i <= end; i += MLX4_TABLE_CHUNK_SIZE / table->obj_size)
		mlx4_table_put(dev, table, i);
}

int mlx4_init_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table,
			u64 virt, int obj_size,	int nobj, int reserved,
			int use_lowmem, int use_coherent)
{
	int obj_per_chunk;
	int num_icm;
	unsigned chunk_size;
	int i;

	obj_per_chunk = MLX4_TABLE_CHUNK_SIZE / obj_size;
	num_icm = (nobj + obj_per_chunk - 1) / obj_per_chunk;

	table->icm      = kcalloc(num_icm, sizeof *table->icm, GFP_KERNEL);
	if (!table->icm)
		return -ENOMEM;
	table->virt     = virt;
	table->num_icm  = num_icm;
	table->num_obj  = nobj;
	table->obj_size = obj_size;
	table->lowmem   = use_lowmem;
	table->coherent = use_coherent;
	mutex_init(&table->mutex);

	for (i = 0; i * MLX4_TABLE_CHUNK_SIZE < reserved * obj_size; ++i) {
		chunk_size = MLX4_TABLE_CHUNK_SIZE;
		if ((i + 1) * MLX4_TABLE_CHUNK_SIZE > nobj * obj_size)
			chunk_size = PAGE_ALIGN(nobj * obj_size - i * MLX4_TABLE_CHUNK_SIZE);

		table->icm[i] = mlx4_alloc_icm(dev, chunk_size >> PAGE_SHIFT,
					       (use_lowmem ? GFP_KERNEL : GFP_HIGHUSER) |
					       __GFP_NOWARN, use_coherent);
		if (!table->icm[i])
			goto err;
		if (mlx4_MAP_ICM(dev, table->icm[i], virt + i * MLX4_TABLE_CHUNK_SIZE)) {
			mlx4_free_icm(dev, table->icm[i], use_coherent);
			table->icm[i] = NULL;
			goto err;
		}

		/*
		 * Add a reference to this ICM chunk so that it never
		 * gets freed (since it contains reserved firmware objects).
		 */
		++table->icm[i]->refcount;
	}

	return 0;

err:
	for (i = 0; i < num_icm; ++i)
		if (table->icm[i]) {
			mlx4_UNMAP_ICM(dev, virt + i * MLX4_TABLE_CHUNK_SIZE,
				       MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE);
			mlx4_free_icm(dev, table->icm[i], use_coherent);
		}

	return -ENOMEM;
}

void mlx4_cleanup_icm_table(struct mlx4_dev *dev, struct mlx4_icm_table *table)
{
	int i;

	for (i = 0; i < table->num_icm; ++i)
		if (table->icm[i]) {
			mlx4_UNMAP_ICM(dev, table->virt + i * MLX4_TABLE_CHUNK_SIZE,
				       MLX4_TABLE_CHUNK_SIZE / MLX4_ICM_PAGE_SIZE);
			mlx4_free_icm(dev, table->icm[i], table->coherent);
		}

	kfree(table->icm);
}
