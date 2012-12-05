/* exynos_drm_dmabuf.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <drm/drmP.h>
#include <drm/exynos_drm.h>
#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"

#include <linux/dma-buf.h>

static struct sg_table *exynos_get_sgt(struct drm_device *drm_dev,
					struct exynos_drm_gem_buf *buf)
{
	struct sg_table *sgt = NULL;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		goto out;

	ret = dma_get_sgtable(drm_dev->dev, sgt, buf->kvaddr,
				buf->dma_addr, buf->size);
	if (ret < 0) {
		DRM_ERROR("failed to get sgtable.\n");
		goto err_free_sgt;
	}

	return sgt;

err_free_sgt:
	kfree(sgt);
	sgt = NULL;
out:
	return NULL;
}

static struct sg_table *
		exynos_gem_map_dma_buf(struct dma_buf_attachment *attach,
					enum dma_data_direction dir)
{
	struct exynos_drm_gem_obj *gem_obj = attach->dmabuf->priv;
	struct drm_device *dev = gem_obj->base.dev;
	struct exynos_drm_gem_buf *buf;
	struct sg_table *sgt = NULL;
	int nents;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	buf = gem_obj->buffer;
	if (!buf) {
		DRM_ERROR("buffer is null.\n");
		return sgt;
	}

	mutex_lock(&dev->struct_mutex);

	sgt = exynos_get_sgt(dev, buf);
	if (!sgt)
		goto err_unlock;

	nents = dma_map_sg(attach->dev, sgt->sgl, sgt->nents, dir);
	if (!nents) {
		DRM_ERROR("failed to map sgl with iommu.\n");
		sgt = NULL;
		goto err_unlock;
	}

	DRM_DEBUG_PRIME("buffer size = 0x%lx\n", buf->size);

err_unlock:
	mutex_unlock(&dev->struct_mutex);
	return sgt;
}

static void exynos_gem_unmap_dma_buf(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
	dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents, dir);

	sg_free_table(sgt);
	kfree(sgt);
	sgt = NULL;
}

static void exynos_dmabuf_release(struct dma_buf *dmabuf)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/*
	 * exynos_dmabuf_release() call means that file object's
	 * f_count is 0 and it calls drm_gem_object_handle_unreference()
	 * to drop the references that these values had been increased
	 * at drm_prime_handle_to_fd()
	 */
	if (exynos_gem_obj->base.export_dma_buf == dmabuf) {
		exynos_gem_obj->base.export_dma_buf = NULL;

		/*
		 * drop this gem object refcount to release allocated buffer
		 * and resources.
		 */
		drm_gem_object_unreference_unlocked(&exynos_gem_obj->base);
	}
}

static void *exynos_gem_dmabuf_kmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void exynos_gem_dmabuf_kunmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num,
						void *addr)
{
	/* TODO */
}

static void *exynos_gem_dmabuf_kmap(struct dma_buf *dma_buf,
					unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void exynos_gem_dmabuf_kunmap(struct dma_buf *dma_buf,
					unsigned long page_num, void *addr)
{
	/* TODO */
}

static int exynos_gem_dmabuf_mmap(struct dma_buf *dma_buf,
	struct vm_area_struct *vma)
{
	return -ENOTTY;
}

static struct dma_buf_ops exynos_dmabuf_ops = {
	.map_dma_buf		= exynos_gem_map_dma_buf,
	.unmap_dma_buf		= exynos_gem_unmap_dma_buf,
	.kmap			= exynos_gem_dmabuf_kmap,
	.kmap_atomic		= exynos_gem_dmabuf_kmap_atomic,
	.kunmap			= exynos_gem_dmabuf_kunmap,
	.kunmap_atomic		= exynos_gem_dmabuf_kunmap_atomic,
	.mmap			= exynos_gem_dmabuf_mmap,
	.release		= exynos_dmabuf_release,
};

struct dma_buf *exynos_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = to_exynos_gem_obj(obj);

	return dma_buf_export(exynos_gem_obj, &exynos_dmabuf_ops,
				exynos_gem_obj->base.size, 0600);
}

struct drm_gem_object *exynos_dmabuf_prime_import(struct drm_device *drm_dev,
				struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_gem_buf *buffer;
	int ret;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/* is this one of own objects? */
	if (dma_buf->ops == &exynos_dmabuf_ops) {
		struct drm_gem_object *obj;

		exynos_gem_obj = dma_buf->priv;
		obj = &exynos_gem_obj->base;

		/* is it from our device? */
		if (obj->dev == drm_dev) {
			drm_gem_object_reference(obj);
			return obj;
		}
	}

	attach = dma_buf_attach(dma_buf, drm_dev->dev);
	if (IS_ERR(attach))
		return ERR_PTR(-EINVAL);


	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_buf_detach;
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate exynos_drm_gem_buf.\n");
		ret = -ENOMEM;
		goto err_unmap_attach;
	}

	exynos_gem_obj = exynos_drm_gem_init(drm_dev, dma_buf->size);
	if (!exynos_gem_obj) {
		ret = -ENOMEM;
		goto err_free_buffer;
	}

	sgl = sgt->sgl;

	buffer->size = dma_buf->size;
	buffer->dma_addr = sg_dma_address(sgl);

	if (sgt->nents == 1) {
		/* always physically continuous memory if sgt->nents is 1. */
		exynos_gem_obj->flags |= EXYNOS_BO_CONTIG;
	} else {
		/*
		 * this case could be CONTIG or NONCONTIG type but for now
		 * sets NONCONTIG.
		 * TODO. we have to find a way that exporter can notify
		 * the type of its own buffer to importer.
		 */
		exynos_gem_obj->flags |= EXYNOS_BO_NONCONTIG;
	}

	exynos_gem_obj->buffer = buffer;
	buffer->sgt = sgt;
	exynos_gem_obj->base.import_attach = attach;

	DRM_DEBUG_PRIME("dma_addr = 0x%x, size = 0x%lx\n", buffer->dma_addr,
								buffer->size);

	return &exynos_gem_obj->base;

err_free_buffer:
	kfree(buffer);
	buffer = NULL;
err_unmap_attach:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
err_buf_detach:
	dma_buf_detach(dma_buf, attach);
	return ERR_PTR(ret);
}

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC DRM DMABUF Module");
MODULE_LICENSE("GPL");
