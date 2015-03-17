/*
 * Portions Copyright 2008 VMware, Inc.
 */
/*
 * memory buffer pool support
 */
#ifndef _LINUX_MEMPOOL_H
#define _LINUX_MEMPOOL_H

#include <linux/wait.h>

struct kmem_cache;

typedef void * (mempool_alloc_t)(gfp_t gfp_mask, void *pool_data);
typedef void (mempool_free_t)(void *element, void *pool_data);

typedef struct mempool_s {
	spinlock_t lock;
	int min_nr;		/* nr of elements at *elements */
	int curr_nr;		/* Current nr of elements at *elements */
	void **elements;

	void *pool_data;
	mempool_alloc_t *alloc;
	mempool_free_t *free;
	wait_queue_head_t wait;
#if defined(__VMKLNX__)
        vmk_ModuleID module_id;
#endif
} mempool_t;

extern mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data);
extern mempool_t *mempool_create_node(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data, int nid);

extern int mempool_resize(mempool_t *pool, int new_min_nr, gfp_t gfp_mask);
extern void mempool_destroy(mempool_t *pool);
extern void * mempool_alloc(mempool_t *pool, gfp_t gfp_mask);
extern void mempool_free(void *element, mempool_t *pool);

/*
 * A mempool_alloc_t and mempool_free_t that get the memory from
 * a slab that is passed in through pool_data.
 */
void *mempool_alloc_slab(gfp_t gfp_mask, void *pool_data);
void mempool_free_slab(void *element, void *pool_data);
/**                                          
 *  mempool_create_slab_pool - create a memory pool
 *  @min_nr: the minimum number of elements guaranteed to be
 *           allocated for this pool.
 *  @kc: a pointer to a struct kmem_cache
 *                                           
 *  Create a memory pool on top of the slab memory cache identified by
 *  @kc. The pool can be used from the mempool_alloc and mempool_free
 *  functions. All memory allocation and deallocation is done thru the
 *  the slab memory cache.
 *                                           
 *  RETURN VALUE:
 *  a pointer to the mempool_t.
 *
 *  SEE ALSO:
 *  mempool_create() and mempool_create_kmalloc_pool()
 */                                          
/* _VMKLNX_CODECHECK_: mempool_create_slab_pool */
static inline mempool_t *
mempool_create_slab_pool(int min_nr, struct kmem_cache *kc)
{
	return mempool_create(min_nr, mempool_alloc_slab, mempool_free_slab,
			      (void *) kc);
}

/*
 * 2 mempool_alloc_t's and a mempool_free_t to kmalloc/kzalloc and kfree
 * the amount of memory specified by pool_data
 */
void *mempool_kmalloc(gfp_t gfp_mask, void *pool_data);
void *mempool_kzalloc(gfp_t gfp_mask, void *pool_data);
void mempool_kfree(void *element, void *pool_data);
/**
 * mempool_create_kmalloc_pool - create a memory pool
 * @min_nr: the minimum number of elements guaranteed to be
 *          allocated for this pool.
 * @size: the size of an elment maintained in the memory pool.
 *
 * Create and allocate a guaranteed size, preallocated
 * memory pool. The pool can be used from the mempool_alloc and mempool_free
 * functions. The pool will use kmalloc() to create new elements
 * and uses kfree() to trim out elements in the pool.
 *
 * RETURN VALUE:
 * a pointer to a mempool descriptor on success; otherwise a NULL.
 *
 * SEE ALSO:
 * mempool_create() and mempool_create_slab_pool()
 */
/* _VMKLNX_CODECHECK_: mempool_create_kmalloc_pool */
static inline mempool_t *mempool_create_kmalloc_pool(int min_nr, size_t size)
{
	return mempool_create(min_nr, mempool_kmalloc, mempool_kfree,
			      (void *) size);
}
static inline mempool_t *mempool_create_kzalloc_pool(int min_nr, size_t size)
{
	return mempool_create(min_nr, mempool_kzalloc, mempool_kfree,
			      (void *) size);
}

/*
 * A mempool_alloc_t and mempool_free_t for a simple page allocator that
 * allocates pages of the order specified by pool_data
 */
void *mempool_alloc_pages(gfp_t gfp_mask, void *pool_data);
void mempool_free_pages(void *element, void *pool_data);
static inline mempool_t *mempool_create_page_pool(int min_nr, int order)
{
	return mempool_create(min_nr, mempool_alloc_pages, mempool_free_pages,
			      (void *)(long)order);
}

#endif /* _LINUX_MEMPOOL_H */
