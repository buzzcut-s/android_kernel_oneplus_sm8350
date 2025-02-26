// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator - buffer interface
 *
 * Copyright (c) 2019, Google, Inc.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-noncoherent.h>

#define CREATE_TRACE_POINTS
#include "ion_trace.h"
#include "ion_private.h"

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->iommu_data.map_list);
	mutex_init(&buffer->iommu_data.lock);
	buffer->heap = heap;
	buffer->flags = flags;
	buffer->size = len;

	ret = heap->ops->allocate(heap, buffer, len, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, flags);
		if (ret)
			goto err2;
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
#ifdef CONFIG_DEBUG_ION_TRACK_HEAP_MEM
	atomic_long_add(len, &heap->total_allocated);
#endif

	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

static int ion_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int ion_sglist_zero(struct scatterlist *sgl, unsigned int nents,
			   pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = ion_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = ion_clear_pages(pages, p, pgprot);

	return ret;
}

struct ion_buffer *ion_buffer_alloc(struct ion_device *dev, size_t len,
				    unsigned int heap_id_mask,
				    unsigned int flags)
{
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	char task_comm[TASK_COMM_LEN];

	if (!dev || !len) {
		return ERR_PTR(-EINVAL);
	}

	if (heap_id_mask & ION_HEAP_SYSTEM) {
		get_task_comm(task_comm, current->group_leader);
		pr_warn_ratelimited("%s: Detected allocation from generic sys heap for task %s-%d\n",
				    __func__, task_comm, current->tgid);
	}

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);
	if (!len)
		return ERR_PTR(-EINVAL);

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, flags);
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer)
		return ERR_PTR(-ENODEV);

	if (IS_ERR(buffer))
		return ERR_CAST(buffer);

	return buffer;
}

int ion_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table;
	pgprot_t pgprot;

	if (!buffer)
		return -EINVAL;

	table = buffer->sg_table;
	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	return ion_sglist_zero(table->sgl, table->nents, pgprot);
}
EXPORT_SYMBOL_GPL(ion_buffer_zero);

void ion_buffer_prep_noncached(struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	struct sg_table *table;
	int i;

	if (WARN_ONCE(!buffer || !buffer->sg_table,
		      "%s needs a buffer and a sg_table", __func__) ||
	    buffer->flags & ION_FLAG_CACHED)
		return;

	table = buffer->sg_table;

	for_each_sg(table->sgl, sg, table->orig_nents, i)
		arch_dma_prep_coherent(sg_page(sg), sg->length);
}
EXPORT_SYMBOL_GPL(ion_buffer_prep_noncached);

void ion_buffer_release(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		ion_heap_unmap_kernel(buffer->heap, buffer);
	}
	buffer->heap->ops->free(buffer);

	kfree(buffer);
}

int ion_buffer_destroy(struct ion_device *dev, struct ion_buffer *buffer)
{
	struct ion_heap *heap;

	if (!dev || !buffer) {
		pr_warn("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	heap = buffer->heap;
#ifdef CONFIG_DEBUG_ION_TRACK_HEAP_MEM
	atomic_long_sub(buffer->size, &heap->total_allocated);
#endif
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_release(buffer);

	return 0;
}

void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		if (buffer->kmap_cnt == INT_MAX)
			return ERR_PTR(-EOVERFLOW);

		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_heap_map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		ion_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}
