/* exynos_drm_buf.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
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
#include "exynos_drm_buf.h"

static int lowlevel_buffer_allocate(struct drm_device *dev,
		unsigned int flags, struct exynos_drm_gem_buf *buf)
{
	int ret = 0;
	enum dma_attr attr = DMA_ATTR_FORCE_CONTIGUOUS;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (buf->dma_addr) {
		DRM_DEBUG_KMS("already allocated.\n");
		return 0;
	}

	init_dma_attrs(&buf->dma_attrs);

	if (flags & EXYNOS_BO_NONCONTIG)
		attr = DMA_ATTR_WRITE_COMBINE;

	dma_set_attr(attr, &buf->dma_attrs);

	buf->kvaddr = dma_alloc_attrs(dev->dev, buf->size,
			&buf->dma_addr, GFP_KERNEL, &buf->dma_attrs);
	if (!buf->kvaddr) {
		DRM_ERROR("failed to allocate buffer.\n");
		return -ENOMEM;
	}

	buf->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!buf->sgt) {
		DRM_ERROR("failed to allocate sg table.\n");
		ret = -ENOMEM;
		goto err_free_attrs;
	}

	ret = dma_get_sgtable(dev->dev, buf->sgt, buf->kvaddr, buf->dma_addr,
			buf->size);
	if (ret < 0) {
		DRM_ERROR("failed to get sgtable.\n");
		goto err_free_sgt;
	}

	DRM_DEBUG_KMS("vaddr(0x%lx), dma_addr(0x%lx), size(0x%lx)\n",
			(unsigned long)buf->kvaddr,
			(unsigned long)buf->dma_addr,
			buf->size);

	return ret;

err_free_sgt:
	kfree(buf->sgt);
	buf->sgt = NULL;
err_free_attrs:
	dma_free_attrs(dev->dev, buf->size, buf->kvaddr,
			(dma_addr_t)buf->dma_addr, &buf->dma_attrs);
	buf->dma_addr = (dma_addr_t)NULL;

	return ret;
}

static void lowlevel_buffer_deallocate(struct drm_device *dev,
		unsigned int flags, struct exynos_drm_gem_buf *buf)
{
	DRM_DEBUG_KMS("%s.\n", __FILE__);

	if (!buf->dma_addr) {
		DRM_DEBUG_KMS("dma_addr is invalid.\n");
		return;
	}

	DRM_DEBUG_KMS("vaddr(0x%lx), dma_addr(0x%lx), size(0x%lx)\n",
			(unsigned long)buf->kvaddr,
			(unsigned long)buf->dma_addr,
			buf->size);

	sg_free_table(buf->sgt);

	kfree(buf->sgt);
	buf->sgt = NULL;

	dma_free_attrs(dev->dev, buf->size, buf->kvaddr,
				(dma_addr_t)buf->dma_addr, &buf->dma_attrs);
	buf->dma_addr = (dma_addr_t)NULL;
}

struct exynos_drm_gem_buf *exynos_drm_init_buf(struct drm_device *dev,
						unsigned int size)
{
	struct exynos_drm_gem_buf *buffer;

	DRM_DEBUG_KMS("%s.\n", __FILE__);
	DRM_DEBUG_KMS("desired size = 0x%x\n", size);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate exynos_drm_gem_buf.\n");
		return NULL;
	}

	buffer->size = size;
	return buffer;
}

void exynos_drm_fini_buf(struct drm_device *dev,
				struct exynos_drm_gem_buf *buffer)
{
	DRM_DEBUG_KMS("%s.\n", __FILE__);

	if (!buffer) {
		DRM_DEBUG_KMS("buffer is null.\n");
		return;
	}

	kfree(buffer);
	buffer = NULL;
}

int exynos_drm_alloc_buf(struct drm_device *dev,
		struct exynos_drm_gem_buf *buf, unsigned int flags)
{

	/*
	 * allocate memory region and set the memory information
	 * to vaddr and dma_addr of a buffer object.
	 */
	if (lowlevel_buffer_allocate(dev, flags, buf) < 0)
		return -ENOMEM;

	return 0;
}

void exynos_drm_free_buf(struct drm_device *dev,
		unsigned int flags, struct exynos_drm_gem_buf *buffer)
{

	lowlevel_buffer_deallocate(dev, flags, buffer);
}
