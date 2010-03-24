/*
 * Copyright 2010 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Alex Deucher
 */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include "drmP.h"
#include "radeon.h"
#include "radeon_asic.h"
#include "radeon_drm.h"
#include "evergreend.h"
#include "atom.h"
#include "avivod.h"
#include "evergreen_reg.h"

#define EVERGREEN_PFP_UCODE_SIZE 1120
#define EVERGREEN_PM4_UCODE_SIZE 1376

static void evergreen_gpu_init(struct radeon_device *rdev);
void evergreen_fini(struct radeon_device *rdev);

bool evergreen_hpd_sense(struct radeon_device *rdev, enum radeon_hpd_id hpd)
{
	bool connected = false;
	/* XXX */
	return connected;
}

void evergreen_hpd_set_polarity(struct radeon_device *rdev,
				enum radeon_hpd_id hpd)
{
	/* XXX */
}

void evergreen_hpd_init(struct radeon_device *rdev)
{
	/* XXX */
}


void evergreen_bandwidth_update(struct radeon_device *rdev)
{
	/* XXX */
}

void evergreen_hpd_fini(struct radeon_device *rdev)
{
	/* XXX */
}

static int evergreen_mc_wait_for_idle(struct radeon_device *rdev)
{
	unsigned i;
	u32 tmp;

	for (i = 0; i < rdev->usec_timeout; i++) {
		/* read MC_STATUS */
		tmp = RREG32(SRBM_STATUS) & 0x1F00;
		if (!tmp)
			return 0;
		udelay(1);
	}
	return -1;
}

/*
 * GART
 */
void evergreen_pcie_gart_tlb_flush(struct radeon_device *rdev)
{
	unsigned i;
	u32 tmp;

	WREG32(VM_CONTEXT0_REQUEST_RESPONSE, REQUEST_TYPE(1));
	for (i = 0; i < rdev->usec_timeout; i++) {
		/* read MC_STATUS */
		tmp = RREG32(VM_CONTEXT0_REQUEST_RESPONSE);
		tmp = (tmp & RESPONSE_TYPE_MASK) >> RESPONSE_TYPE_SHIFT;
		if (tmp == 2) {
			printk(KERN_WARNING "[drm] r600 flush TLB failed\n");
			return;
		}
		if (tmp) {
			return;
		}
		udelay(1);
	}
}

int evergreen_pcie_gart_enable(struct radeon_device *rdev)
{
	u32 tmp;
	int r;

	if (rdev->gart.table.vram.robj == NULL) {
		dev_err(rdev->dev, "No VRAM object for PCIE GART.\n");
		return -EINVAL;
	}
	r = radeon_gart_table_vram_pin(rdev);
	if (r)
		return r;
	radeon_gart_restore(rdev);
	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	WREG32(VM_CONTEXT0_PAGE_TABLE_START_ADDR, rdev->mc.gtt_start >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_END_ADDR, rdev->mc.gtt_end >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, rdev->gart.table_addr >> 12);
	WREG32(VM_CONTEXT0_CNTL, ENABLE_CONTEXT | PAGE_TABLE_DEPTH(0) |
				RANGE_PROTECTION_FAULT_ENABLE_DEFAULT);
	WREG32(VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR,
			(u32)(rdev->dummy_page.addr >> 12));
	WREG32(VM_CONTEXT1_CNTL, 0);

	evergreen_pcie_gart_tlb_flush(rdev);
	rdev->gart.ready = true;
	return 0;
}

void evergreen_pcie_gart_disable(struct radeon_device *rdev)
{
	u32 tmp;
	int r;

	/* Disable all tables */
	WREG32(VM_CONTEXT0_CNTL, 0);
	WREG32(VM_CONTEXT1_CNTL, 0);

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_FRAGMENT_PROCESSING |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	if (rdev->gart.table.vram.robj) {
		r = radeon_bo_reserve(rdev->gart.table.vram.robj, false);
		if (likely(r == 0)) {
			radeon_bo_kunmap(rdev->gart.table.vram.robj);
			radeon_bo_unpin(rdev->gart.table.vram.robj);
			radeon_bo_unreserve(rdev->gart.table.vram.robj);
		}
	}
}

void evergreen_pcie_gart_fini(struct radeon_device *rdev)
{
	evergreen_pcie_gart_disable(rdev);
	radeon_gart_table_vram_free(rdev);
	radeon_gart_fini(rdev);
}


void evergreen_agp_enable(struct radeon_device *rdev)
{
	u32 tmp;

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	WREG32(VM_CONTEXT0_CNTL, 0);
	WREG32(VM_CONTEXT1_CNTL, 0);
}

static void evergreen_mc_stop(struct radeon_device *rdev, struct evergreen_mc_save *save)
{
	save->vga_control[0] = RREG32(D1VGA_CONTROL);
	save->vga_control[1] = RREG32(D2VGA_CONTROL);
	save->vga_control[2] = RREG32(EVERGREEN_D3VGA_CONTROL);
	save->vga_control[3] = RREG32(EVERGREEN_D4VGA_CONTROL);
	save->vga_control[4] = RREG32(EVERGREEN_D5VGA_CONTROL);
	save->vga_control[5] = RREG32(EVERGREEN_D6VGA_CONTROL);
	save->vga_render_control = RREG32(VGA_RENDER_CONTROL);
	save->vga_hdp_control = RREG32(VGA_HDP_CONTROL);
	save->crtc_control[0] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET);
	save->crtc_control[1] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET);
	save->crtc_control[2] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET);
	save->crtc_control[3] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET);
	save->crtc_control[4] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET);
	save->crtc_control[5] = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET);

	/* Stop all video */
	WREG32(VGA_RENDER_CONTROL, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC0_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC1_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC2_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC3_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC4_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC5_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC1_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC2_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC3_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC4_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC5_REGISTER_OFFSET, 0);

	WREG32(D1VGA_CONTROL, 0);
	WREG32(D2VGA_CONTROL, 0);
	WREG32(EVERGREEN_D3VGA_CONTROL, 0);
	WREG32(EVERGREEN_D4VGA_CONTROL, 0);
	WREG32(EVERGREEN_D5VGA_CONTROL, 0);
	WREG32(EVERGREEN_D6VGA_CONTROL, 0);
}

static void evergreen_mc_resume(struct radeon_device *rdev, struct evergreen_mc_save *save)
{
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC1_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC1_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC1_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC1_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC2_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC2_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC2_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC2_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC3_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC3_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC3_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC3_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC4_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC4_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC4_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC4_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC5_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC5_REGISTER_OFFSET,
	       upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC5_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC5_REGISTER_OFFSET,
	       (u32)rdev->mc.vram_start);

	WREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS_HIGH, upper_32_bits(rdev->mc.vram_start));
	WREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS, (u32)rdev->mc.vram_start);
	/* Unlock host access */
	WREG32(VGA_HDP_CONTROL, save->vga_hdp_control);
	mdelay(1);
	/* Restore video state */
	WREG32(D1VGA_CONTROL, save->vga_control[0]);
	WREG32(D2VGA_CONTROL, save->vga_control[1]);
	WREG32(EVERGREEN_D3VGA_CONTROL, save->vga_control[2]);
	WREG32(EVERGREEN_D4VGA_CONTROL, save->vga_control[3]);
	WREG32(EVERGREEN_D5VGA_CONTROL, save->vga_control[4]);
	WREG32(EVERGREEN_D6VGA_CONTROL, save->vga_control[5]);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC0_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC1_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC2_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC3_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC4_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC5_REGISTER_OFFSET, 1);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET, save->crtc_control[0]);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET, save->crtc_control[1]);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET, save->crtc_control[2]);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET, save->crtc_control[3]);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET, save->crtc_control[4]);
	WREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET, save->crtc_control[5]);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC1_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC2_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC3_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC4_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC5_REGISTER_OFFSET, 0);
	WREG32(VGA_RENDER_CONTROL, save->vga_render_control);
}

static void evergreen_mc_program(struct radeon_device *rdev)
{
	struct evergreen_mc_save save;
	u32 tmp;
	int i, j;

	/* Initialize HDP */
	for (i = 0, j = 0; i < 32; i++, j += 0x18) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
	}
	WREG32(HDP_REG_COHERENCY_FLUSH_CNTL, 0);

	evergreen_mc_stop(rdev, &save);
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	/* Lockout access through VGA aperture*/
	WREG32(VGA_HDP_CONTROL, VGA_MEMORY_DISABLE);
	/* Update configuration */
	if (rdev->flags & RADEON_IS_AGP) {
		if (rdev->mc.vram_start < rdev->mc.gtt_start) {
			/* VRAM before AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.vram_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.gtt_end >> 12);
		} else {
			/* VRAM after AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.gtt_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.vram_end >> 12);
		}
	} else {
		WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
			rdev->mc.vram_start >> 12);
		WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
			rdev->mc.vram_end >> 12);
	}
	WREG32(MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, 0);
	tmp = ((rdev->mc.vram_end >> 24) & 0xFFFF) << 16;
	tmp |= ((rdev->mc.vram_start >> 24) & 0xFFFF);
	WREG32(MC_VM_FB_LOCATION, tmp);
	WREG32(HDP_NONSURFACE_BASE, (rdev->mc.vram_start >> 8));
	WREG32(HDP_NONSURFACE_INFO, (2 << 7));
	WREG32(HDP_NONSURFACE_SIZE, (rdev->mc.mc_vram_size - 1) | 0x3FF);
	if (rdev->flags & RADEON_IS_AGP) {
		WREG32(MC_VM_AGP_TOP, rdev->mc.gtt_end >> 16);
		WREG32(MC_VM_AGP_BOT, rdev->mc.gtt_start >> 16);
		WREG32(MC_VM_AGP_BASE, rdev->mc.agp_base >> 22);
	} else {
		WREG32(MC_VM_AGP_BASE, 0);
		WREG32(MC_VM_AGP_TOP, 0x0FFFFFFF);
		WREG32(MC_VM_AGP_BOT, 0x0FFFFFFF);
	}
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	evergreen_mc_resume(rdev, &save);
	/* we need to own VRAM, so turn off the VGA renderer here
	 * to stop it overwriting our objects */
	rv515_vga_render_disable(rdev);
}

/*
 * CP.
 */

static int evergreen_cp_load_microcode(struct radeon_device *rdev)
{
	const __be32 *fw_data;
	int i;

	if (!rdev->me_fw || !rdev->pfp_fw)
		return -EINVAL;

	r700_cp_stop(rdev);
	WREG32(CP_RB_CNTL, RB_NO_UPDATE | (15 << 8) | (3 << 0));

	fw_data = (const __be32 *)rdev->pfp_fw->data;
	WREG32(CP_PFP_UCODE_ADDR, 0);
	for (i = 0; i < EVERGREEN_PFP_UCODE_SIZE; i++)
		WREG32(CP_PFP_UCODE_DATA, be32_to_cpup(fw_data++));
	WREG32(CP_PFP_UCODE_ADDR, 0);

	fw_data = (const __be32 *)rdev->me_fw->data;
	WREG32(CP_ME_RAM_WADDR, 0);
	for (i = 0; i < EVERGREEN_PM4_UCODE_SIZE; i++)
		WREG32(CP_ME_RAM_DATA, be32_to_cpup(fw_data++));

	WREG32(CP_PFP_UCODE_ADDR, 0);
	WREG32(CP_ME_RAM_WADDR, 0);
	WREG32(CP_ME_RAM_RADDR, 0);
	return 0;
}

int evergreen_cp_resume(struct radeon_device *rdev)
{
	u32 tmp;
	u32 rb_bufsz;
	int r;

	/* Reset cp; if cp is reset, then PA, SH, VGT also need to be reset */
	WREG32(GRBM_SOFT_RESET, (SOFT_RESET_CP |
				 SOFT_RESET_PA |
				 SOFT_RESET_SH |
				 SOFT_RESET_VGT |
				 SOFT_RESET_SX));
	RREG32(GRBM_SOFT_RESET);
	mdelay(15);
	WREG32(GRBM_SOFT_RESET, 0);
	RREG32(GRBM_SOFT_RESET);

	/* Set ring buffer size */
	rb_bufsz = drm_order(rdev->cp.ring_size / 8);
	tmp = RB_NO_UPDATE | (drm_order(RADEON_GPU_PAGE_SIZE/8) << 8) | rb_bufsz;
#ifdef __BIG_ENDIAN
	tmp |= BUF_SWAP_32BIT;
#endif
	WREG32(CP_RB_CNTL, tmp);
	WREG32(CP_SEM_WAIT_TIMER, 0x4);

	/* Set the write pointer delay */
	WREG32(CP_RB_WPTR_DELAY, 0);

	/* Initialize the ring buffer's read and write pointers */
	WREG32(CP_RB_CNTL, tmp | RB_RPTR_WR_ENA);
	WREG32(CP_RB_RPTR_WR, 0);
	WREG32(CP_RB_WPTR, 0);
	WREG32(CP_RB_RPTR_ADDR, rdev->cp.gpu_addr & 0xFFFFFFFF);
	WREG32(CP_RB_RPTR_ADDR_HI, upper_32_bits(rdev->cp.gpu_addr));
	mdelay(1);
	WREG32(CP_RB_CNTL, tmp);

	WREG32(CP_RB_BASE, rdev->cp.gpu_addr >> 8);
	WREG32(CP_DEBUG, (1 << 27) | (1 << 28));

	rdev->cp.rptr = RREG32(CP_RB_RPTR);
	rdev->cp.wptr = RREG32(CP_RB_WPTR);

	r600_cp_start(rdev);
	rdev->cp.ready = true;
	r = radeon_ring_test(rdev);
	if (r) {
		rdev->cp.ready = false;
		return r;
	}
	return 0;
}

/*
 * Core functions
 */
static u32 evergreen_get_tile_pipe_to_backend_map(struct radeon_device *rdev,
						  u32 num_tile_pipes,
						  u32 num_backends,
						  u32 backend_disable_mask)
{
	u32 backend_map = 0;
	u32 enabled_backends_mask = 0;
	u32 enabled_backends_count = 0;
	u32 cur_pipe;
	u32 swizzle_pipe[EVERGREEN_MAX_PIPES];
	u32 cur_backend = 0;
	u32 i;
	bool force_no_swizzle;

	if (num_tile_pipes > EVERGREEN_MAX_PIPES)
		num_tile_pipes = EVERGREEN_MAX_PIPES;
	if (num_tile_pipes < 1)
		num_tile_pipes = 1;
	if (num_backends > EVERGREEN_MAX_BACKENDS)
		num_backends = EVERGREEN_MAX_BACKENDS;
	if (num_backends < 1)
		num_backends = 1;

	for (i = 0; i < EVERGREEN_MAX_BACKENDS; ++i) {
		if (((backend_disable_mask >> i) & 1) == 0) {
			enabled_backends_mask |= (1 << i);
			++enabled_backends_count;
		}
		if (enabled_backends_count == num_backends)
			break;
	}

	if (enabled_backends_count == 0) {
		enabled_backends_mask = 1;
		enabled_backends_count = 1;
	}

	if (enabled_backends_count != num_backends)
		num_backends = enabled_backends_count;

	memset((uint8_t *)&swizzle_pipe[0], 0, sizeof(u32) * EVERGREEN_MAX_PIPES);
	switch (rdev->family) {
	case CHIP_CEDAR:
	case CHIP_REDWOOD:
		force_no_swizzle = false;
		break;
	case CHIP_CYPRESS:
	case CHIP_HEMLOCK:
	case CHIP_JUNIPER:
	default:
		force_no_swizzle = true;
		break;
	}
	if (force_no_swizzle) {
		bool last_backend_enabled = false;

		force_no_swizzle = false;
		for (i = 0; i < EVERGREEN_MAX_BACKENDS; ++i) {
			if (((enabled_backends_mask >> i) & 1) == 1) {
				if (last_backend_enabled)
					force_no_swizzle = true;
				last_backend_enabled = true;
			} else
				last_backend_enabled = false;
		}
	}

	switch (num_tile_pipes) {
	case 1:
	case 3:
	case 5:
	case 7:
		DRM_ERROR("odd number of pipes!\n");
		break;
	case 2:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		break;
	case 4:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 1;
			swizzle_pipe[3] = 3;
		}
		break;
	case 6:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
			swizzle_pipe[5] = 5;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 1;
			swizzle_pipe[4] = 3;
			swizzle_pipe[5] = 5;
		}
		break;
	case 8:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
			swizzle_pipe[5] = 5;
			swizzle_pipe[6] = 6;
			swizzle_pipe[7] = 7;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 6;
			swizzle_pipe[4] = 1;
			swizzle_pipe[5] = 3;
			swizzle_pipe[6] = 5;
			swizzle_pipe[7] = 7;
		}
		break;
	}

	for (cur_pipe = 0; cur_pipe < num_tile_pipes; ++cur_pipe) {
		while (((1 << cur_backend) & enabled_backends_mask) == 0)
			cur_backend = (cur_backend + 1) % EVERGREEN_MAX_BACKENDS;

		backend_map |= (((cur_backend & 0xf) << (swizzle_pipe[cur_pipe] * 4)));

		cur_backend = (cur_backend + 1) % EVERGREEN_MAX_BACKENDS;
	}

	return backend_map;
}

static void evergreen_gpu_init(struct radeon_device *rdev)
{
	u32 cc_rb_backend_disable = 0;
	u32 cc_gc_shader_pipe_config;
	u32 gb_addr_config = 0;
	u32 mc_shared_chmap, mc_arb_ramcfg;
	u32 gb_backend_map;
	u32 grbm_gfx_index;
	u32 sx_debug_1;
	u32 smx_dc_ctl0;
	u32 sq_config;
	u32 sq_lds_resource_mgmt;
	u32 sq_gpr_resource_mgmt_1;
	u32 sq_gpr_resource_mgmt_2;
	u32 sq_gpr_resource_mgmt_3;
	u32 sq_thread_resource_mgmt;
	u32 sq_thread_resource_mgmt_2;
	u32 sq_stack_resource_mgmt_1;
	u32 sq_stack_resource_mgmt_2;
	u32 sq_stack_resource_mgmt_3;
	u32 vgt_cache_invalidation;
	u32 hdp_host_path_cntl;
	int i, j, num_shader_engines, ps_thread_count;

	switch (rdev->family) {
	case CHIP_CYPRESS:
	case CHIP_HEMLOCK:
		rdev->config.evergreen.num_ses = 2;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 8;
		rdev->config.evergreen.max_simds = 10;
		rdev->config.evergreen.max_backends = 4 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		break;
	case CHIP_JUNIPER:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 10;
		rdev->config.evergreen.max_backends = 4 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 512;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		break;
	case CHIP_REDWOOD:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 4;
		rdev->config.evergreen.max_tile_pipes = 4;
		rdev->config.evergreen.max_simds = 5;
		rdev->config.evergreen.max_backends = 2 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 248;
		rdev->config.evergreen.max_gs_threads = 32;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 256;
		rdev->config.evergreen.sx_max_export_pos_size = 64;
		rdev->config.evergreen.sx_max_export_smx_size = 192;
		rdev->config.evergreen.max_hw_contexts = 8;
		rdev->config.evergreen.sq_num_cf_insts = 2;

		rdev->config.evergreen.sc_prim_fifo_size = 0x100;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		break;
	case CHIP_CEDAR:
	default:
		rdev->config.evergreen.num_ses = 1;
		rdev->config.evergreen.max_pipes = 2;
		rdev->config.evergreen.max_tile_pipes = 2;
		rdev->config.evergreen.max_simds = 2;
		rdev->config.evergreen.max_backends = 1 * rdev->config.evergreen.num_ses;
		rdev->config.evergreen.max_gprs = 256;
		rdev->config.evergreen.max_threads = 192;
		rdev->config.evergreen.max_gs_threads = 16;
		rdev->config.evergreen.max_stack_entries = 256;
		rdev->config.evergreen.sx_num_of_sets = 4;
		rdev->config.evergreen.sx_max_export_size = 128;
		rdev->config.evergreen.sx_max_export_pos_size = 32;
		rdev->config.evergreen.sx_max_export_smx_size = 96;
		rdev->config.evergreen.max_hw_contexts = 4;
		rdev->config.evergreen.sq_num_cf_insts = 1;

		rdev->config.evergreen.sc_prim_fifo_size = 0x40;
		rdev->config.evergreen.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.evergreen.sc_earlyz_tile_fifo_size = 0x130;
		break;
	}

	/* Initialize HDP */
	for (i = 0, j = 0; i < 32; i++, j += 0x18) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
	}

	WREG32(GRBM_CNTL, GRBM_READ_TIMEOUT(0xff));

	cc_gc_shader_pipe_config = RREG32(CC_GC_SHADER_PIPE_CONFIG) & ~2;

	cc_gc_shader_pipe_config |=
		INACTIVE_QD_PIPES((EVERGREEN_MAX_PIPES_MASK << rdev->config.evergreen.max_pipes)
				  & EVERGREEN_MAX_PIPES_MASK);
	cc_gc_shader_pipe_config |=
		INACTIVE_SIMDS((EVERGREEN_MAX_SIMDS_MASK << rdev->config.evergreen.max_simds)
			       & EVERGREEN_MAX_SIMDS_MASK);

	cc_rb_backend_disable =
		BACKEND_DISABLE((EVERGREEN_MAX_BACKENDS_MASK << rdev->config.evergreen.max_backends)
				& EVERGREEN_MAX_BACKENDS_MASK);


	mc_shared_chmap = RREG32(MC_SHARED_CHMAP);
	mc_arb_ramcfg = RREG32(MC_ARB_RAMCFG);

	switch (rdev->config.evergreen.max_tile_pipes) {
	case 1:
	default:
		gb_addr_config |= NUM_PIPES(0);
		break;
	case 2:
		gb_addr_config |= NUM_PIPES(1);
		break;
	case 4:
		gb_addr_config |= NUM_PIPES(2);
		break;
	case 8:
		gb_addr_config |= NUM_PIPES(3);
		break;
	}

	gb_addr_config |= PIPE_INTERLEAVE_SIZE((mc_arb_ramcfg & BURSTLENGTH_MASK) >> BURSTLENGTH_SHIFT);
	gb_addr_config |= BANK_INTERLEAVE_SIZE(0);
	gb_addr_config |= NUM_SHADER_ENGINES(rdev->config.evergreen.num_ses - 1);
	gb_addr_config |= SHADER_ENGINE_TILE_SIZE(1);
	gb_addr_config |= NUM_GPUS(0); /* Hemlock? */
	gb_addr_config |= MULTI_GPU_TILE_SIZE(2);

	if (((mc_arb_ramcfg & NOOFCOLS_MASK) >> NOOFCOLS_SHIFT) > 2)
		gb_addr_config |= ROW_SIZE(2);
	else
		gb_addr_config |= ROW_SIZE((mc_arb_ramcfg & NOOFCOLS_MASK) >> NOOFCOLS_SHIFT);

	if (rdev->ddev->pdev->device == 0x689e) {
		u32 efuse_straps_4;
		u32 efuse_straps_3;
		u8 efuse_box_bit_131_124;

		WREG32(RCU_IND_INDEX, 0x204);
		efuse_straps_4 = RREG32(RCU_IND_DATA);
		WREG32(RCU_IND_INDEX, 0x203);
		efuse_straps_3 = RREG32(RCU_IND_DATA);
		efuse_box_bit_131_124 = (u8)(((efuse_straps_4 & 0xf) << 4) | ((efuse_straps_3 & 0xf0000000) >> 28));

		switch(efuse_box_bit_131_124) {
		case 0x00:
			gb_backend_map = 0x76543210;
			break;
		case 0x55:
			gb_backend_map = 0x77553311;
			break;
		case 0x56:
			gb_backend_map = 0x77553300;
			break;
		case 0x59:
			gb_backend_map = 0x77552211;
			break;
		case 0x66:
			gb_backend_map = 0x77443300;
			break;
		case 0x99:
			gb_backend_map = 0x66552211;
			break;
		case 0x5a:
			gb_backend_map = 0x77552200;
			break;
		case 0xaa:
			gb_backend_map = 0x66442200;
			break;
		case 0x95:
			gb_backend_map = 0x66553311;
			break;
		default:
			DRM_ERROR("bad backend map, using default\n");
			gb_backend_map =
				evergreen_get_tile_pipe_to_backend_map(rdev,
								       rdev->config.evergreen.max_tile_pipes,
								       rdev->config.evergreen.max_backends,
								       ((EVERGREEN_MAX_BACKENDS_MASK <<
								   rdev->config.evergreen.max_backends) &
									EVERGREEN_MAX_BACKENDS_MASK));
			break;
		}
	} else if (rdev->ddev->pdev->device == 0x68b9) {
		u32 efuse_straps_3;
		u8 efuse_box_bit_127_124;

		WREG32(RCU_IND_INDEX, 0x203);
		efuse_straps_3 = RREG32(RCU_IND_DATA);
		efuse_box_bit_127_124 = (u8)(efuse_straps_3 & 0xF0000000) >> 28;

		switch(efuse_box_bit_127_124) {
		case 0x0:
			gb_backend_map = 0x00003210;
			break;
		case 0x5:
		case 0x6:
		case 0x9:
		case 0xa:
			gb_backend_map = 0x00003311;
			break;
		default:
			DRM_ERROR("bad backend map, using default\n");
			gb_backend_map =
				evergreen_get_tile_pipe_to_backend_map(rdev,
								       rdev->config.evergreen.max_tile_pipes,
								       rdev->config.evergreen.max_backends,
								       ((EVERGREEN_MAX_BACKENDS_MASK <<
								   rdev->config.evergreen.max_backends) &
									EVERGREEN_MAX_BACKENDS_MASK));
			break;
		}
	} else
		gb_backend_map =
			evergreen_get_tile_pipe_to_backend_map(rdev,
							       rdev->config.evergreen.max_tile_pipes,
							       rdev->config.evergreen.max_backends,
							       ((EVERGREEN_MAX_BACKENDS_MASK <<
								 rdev->config.evergreen.max_backends) &
								EVERGREEN_MAX_BACKENDS_MASK));

	WREG32(GB_BACKEND_MAP, gb_backend_map);
	WREG32(GB_ADDR_CONFIG, gb_addr_config);
	WREG32(DMIF_ADDR_CONFIG, gb_addr_config);
	WREG32(HDP_ADDR_CONFIG, gb_addr_config);

	num_shader_engines = ((RREG32(GB_ADDR_CONFIG) & NUM_SHADER_ENGINES(3)) >> 12) + 1;
	grbm_gfx_index = INSTANCE_BROADCAST_WRITES;

	for (i = 0; i < rdev->config.evergreen.num_ses; i++) {
		u32 rb = cc_rb_backend_disable | (0xf0 << 16);
		u32 sp = cc_gc_shader_pipe_config;
		u32 gfx = grbm_gfx_index | SE_INDEX(i);

		if (i == num_shader_engines) {
			rb |= BACKEND_DISABLE(EVERGREEN_MAX_BACKENDS_MASK);
			sp |= INACTIVE_SIMDS(EVERGREEN_MAX_SIMDS_MASK);
		}

		WREG32(GRBM_GFX_INDEX, gfx);
		WREG32(RLC_GFX_INDEX, gfx);

		WREG32(CC_RB_BACKEND_DISABLE, rb);
		WREG32(CC_SYS_RB_BACKEND_DISABLE, rb);
		WREG32(GC_USER_RB_BACKEND_DISABLE, rb);
		WREG32(CC_GC_SHADER_PIPE_CONFIG, sp);
        }

	grbm_gfx_index |= SE_BROADCAST_WRITES;
	WREG32(GRBM_GFX_INDEX, grbm_gfx_index);
	WREG32(RLC_GFX_INDEX, grbm_gfx_index);

	WREG32(CGTS_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_TCC_DISABLE, 0);

	/* set HW defaults for 3D engine */
	WREG32(CP_QUEUE_THRESHOLDS, (ROQ_IB1_START(0x16) |
				     ROQ_IB2_START(0x2b)));

	WREG32(CP_MEQ_THRESHOLDS, STQ_SPLIT(0x30));

	WREG32(TA_CNTL_AUX, (DISABLE_CUBE_ANISO |
			     SYNC_GRADIENT |
			     SYNC_WALKER |
			     SYNC_ALIGNER));

	sx_debug_1 = RREG32(SX_DEBUG_1);
	sx_debug_1 |= ENABLE_NEW_SMX_ADDRESS;
	WREG32(SX_DEBUG_1, sx_debug_1);


	smx_dc_ctl0 = RREG32(SMX_DC_CTL0);
	smx_dc_ctl0 &= ~NUMBER_OF_SETS(0x1ff);
	smx_dc_ctl0 |= NUMBER_OF_SETS(rdev->config.evergreen.sx_num_of_sets);
	WREG32(SMX_DC_CTL0, smx_dc_ctl0);

	WREG32(SX_EXPORT_BUFFER_SIZES, (COLOR_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_size / 4) - 1) |
					POSITION_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_pos_size / 4) - 1) |
					SMX_BUFFER_SIZE((rdev->config.evergreen.sx_max_export_smx_size / 4) - 1)));

	WREG32(PA_SC_FIFO_SIZE, (SC_PRIM_FIFO_SIZE(rdev->config.evergreen.sc_prim_fifo_size) |
				 SC_HIZ_TILE_FIFO_SIZE(rdev->config.evergreen.sc_hiz_tile_fifo_size) |
				 SC_EARLYZ_TILE_FIFO_SIZE(rdev->config.evergreen.sc_earlyz_tile_fifo_size)));

	WREG32(VGT_NUM_INSTANCES, 1);
	WREG32(SPI_CONFIG_CNTL, 0);
	WREG32(SPI_CONFIG_CNTL_1, VTX_DONE_DELAY(4));
	WREG32(CP_PERFMON_CNTL, 0);

	WREG32(SQ_MS_FIFO_SIZES, (CACHE_FIFO_SIZE(16 * rdev->config.evergreen.sq_num_cf_insts) |
				  FETCH_FIFO_HIWATER(0x4) |
				  DONE_FIFO_HIWATER(0xe0) |
				  ALU_UPDATE_FIFO_HIWATER(0x8)));

	sq_config = RREG32(SQ_CONFIG);
	sq_config &= ~(PS_PRIO(3) |
		       VS_PRIO(3) |
		       GS_PRIO(3) |
		       ES_PRIO(3));
	sq_config |= (VC_ENABLE |
		      EXPORT_SRC_C |
		      PS_PRIO(0) |
		      VS_PRIO(1) |
		      GS_PRIO(2) |
		      ES_PRIO(3));

	if (rdev->family == CHIP_CEDAR)
		/* no vertex cache */
		sq_config &= ~VC_ENABLE;

	sq_lds_resource_mgmt = RREG32(SQ_LDS_RESOURCE_MGMT);

	sq_gpr_resource_mgmt_1 = NUM_PS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2))* 12 / 32);
	sq_gpr_resource_mgmt_1 |= NUM_VS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 6 / 32);
	sq_gpr_resource_mgmt_1 |= NUM_CLAUSE_TEMP_GPRS(4);
	sq_gpr_resource_mgmt_2 = NUM_GS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 4 / 32);
	sq_gpr_resource_mgmt_2 |= NUM_ES_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 4 / 32);
	sq_gpr_resource_mgmt_3 = NUM_HS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 3 / 32);
	sq_gpr_resource_mgmt_3 |= NUM_LS_GPRS((rdev->config.evergreen.max_gprs - (4 * 2)) * 3 / 32);

	if (rdev->family == CHIP_CEDAR)
		ps_thread_count = 96;
	else
		ps_thread_count = 128;

	sq_thread_resource_mgmt = NUM_PS_THREADS(ps_thread_count);
	sq_thread_resource_mgmt |= NUM_VS_THREADS(((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8;
	sq_thread_resource_mgmt |= NUM_GS_THREADS(((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8;
	sq_thread_resource_mgmt |= NUM_ES_THREADS(((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8;
	sq_thread_resource_mgmt_2 = NUM_HS_THREADS(((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8;
	sq_thread_resource_mgmt_2 |= NUM_LS_THREADS(((rdev->config.evergreen.max_threads - ps_thread_count) / 6) / 8) * 8;

	sq_stack_resource_mgmt_1 = NUM_PS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_1 |= NUM_VS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_2 = NUM_GS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_2 |= NUM_ES_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_3 = NUM_HS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);
	sq_stack_resource_mgmt_3 |= NUM_LS_STACK_ENTRIES((rdev->config.evergreen.max_stack_entries * 1) / 6);

	WREG32(SQ_CONFIG, sq_config);
	WREG32(SQ_GPR_RESOURCE_MGMT_1, sq_gpr_resource_mgmt_1);
	WREG32(SQ_GPR_RESOURCE_MGMT_2, sq_gpr_resource_mgmt_2);
	WREG32(SQ_GPR_RESOURCE_MGMT_3, sq_gpr_resource_mgmt_3);
	WREG32(SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);
	WREG32(SQ_THREAD_RESOURCE_MGMT_2, sq_thread_resource_mgmt_2);
	WREG32(SQ_STACK_RESOURCE_MGMT_1, sq_stack_resource_mgmt_1);
	WREG32(SQ_STACK_RESOURCE_MGMT_2, sq_stack_resource_mgmt_2);
	WREG32(SQ_STACK_RESOURCE_MGMT_3, sq_stack_resource_mgmt_3);
	WREG32(SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, 0);
	WREG32(SQ_LDS_RESOURCE_MGMT, sq_lds_resource_mgmt);

	WREG32(PA_SC_FORCE_EOV_MAX_CNTS, (FORCE_EOV_MAX_CLK_CNT(4095) |
					  FORCE_EOV_MAX_REZ_CNT(255)));

	if (rdev->family == CHIP_CEDAR)
		vgt_cache_invalidation = CACHE_INVALIDATION(TC_ONLY);
	else
		vgt_cache_invalidation = CACHE_INVALIDATION(VC_AND_TC);
	vgt_cache_invalidation |= AUTO_INVLD_EN(ES_AND_GS_AUTO);
	WREG32(VGT_CACHE_INVALIDATION, vgt_cache_invalidation);

	WREG32(VGT_GS_VERTEX_REUSE, 16);
	WREG32(PA_SC_LINE_STIPPLE_STATE, 0);

	WREG32(CB_PERF_CTR0_SEL_0, 0);
	WREG32(CB_PERF_CTR0_SEL_1, 0);
	WREG32(CB_PERF_CTR1_SEL_0, 0);
	WREG32(CB_PERF_CTR1_SEL_1, 0);
	WREG32(CB_PERF_CTR2_SEL_0, 0);
	WREG32(CB_PERF_CTR2_SEL_1, 0);
	WREG32(CB_PERF_CTR3_SEL_0, 0);
	WREG32(CB_PERF_CTR3_SEL_1, 0);

	hdp_host_path_cntl = RREG32(HDP_HOST_PATH_CNTL);
	WREG32(HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	WREG32(PA_CL_ENHANCE, CLIP_VTX_REORDER_ENA | NUM_CLIP_SEQ(3));

	udelay(50);

}

int evergreen_mc_init(struct radeon_device *rdev)
{
	u32 tmp;
	int chansize, numchan;

	/* Get VRAM informations */
	rdev->mc.vram_is_ddr = true;
	tmp = RREG32(MC_ARB_RAMCFG);
	if (tmp & CHANSIZE_OVERRIDE) {
		chansize = 16;
	} else if (tmp & CHANSIZE_MASK) {
		chansize = 64;
	} else {
		chansize = 32;
	}
	tmp = RREG32(MC_SHARED_CHMAP);
	switch ((tmp & NOOFCHAN_MASK) >> NOOFCHAN_SHIFT) {
	case 0:
	default:
		numchan = 1;
		break;
	case 1:
		numchan = 2;
		break;
	case 2:
		numchan = 4;
		break;
	case 3:
		numchan = 8;
		break;
	}
	rdev->mc.vram_width = numchan * chansize;
	/* Could aper size report 0 ? */
	rdev->mc.aper_base = drm_get_resource_start(rdev->ddev, 0);
	rdev->mc.aper_size = drm_get_resource_len(rdev->ddev, 0);
	/* Setup GPU memory space */
	/* size in MB on evergreen */
	rdev->mc.mc_vram_size = RREG32(CONFIG_MEMSIZE) * 1024 * 1024;
	rdev->mc.real_vram_size = RREG32(CONFIG_MEMSIZE) * 1024 * 1024;
	rdev->mc.visible_vram_size = rdev->mc.aper_size;
	/* FIXME remove this once we support unmappable VRAM */
	if (rdev->mc.mc_vram_size > rdev->mc.aper_size) {
		rdev->mc.mc_vram_size = rdev->mc.aper_size;
		rdev->mc.real_vram_size = rdev->mc.aper_size;
	}
	r600_vram_gtt_location(rdev, &rdev->mc);
	radeon_update_bandwidth_info(rdev);

	return 0;
}

bool evergreen_gpu_is_lockup(struct radeon_device *rdev)
{
	/* FIXME: implement for evergreen */
	return false;
}

static int evergreen_gpu_soft_reset(struct radeon_device *rdev)
{
	struct evergreen_mc_save save;
	u32 srbm_reset = 0;
	u32 grbm_reset = 0;

	dev_info(rdev->dev, "GPU softreset \n");
	dev_info(rdev->dev, "  GRBM_STATUS=0x%08X\n",
		RREG32(GRBM_STATUS));
	dev_info(rdev->dev, "  GRBM_STATUS_SE0=0x%08X\n",
		RREG32(GRBM_STATUS_SE0));
	dev_info(rdev->dev, "  GRBM_STATUS_SE1=0x%08X\n",
		RREG32(GRBM_STATUS_SE1));
	dev_info(rdev->dev, "  SRBM_STATUS=0x%08X\n",
		RREG32(SRBM_STATUS));
	evergreen_mc_stop(rdev, &save);
	if (evergreen_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	/* Disable CP parsing/prefetching */
	WREG32(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);

	/* reset all the gfx blocks */
	grbm_reset = (SOFT_RESET_CP |
		      SOFT_RESET_CB |
		      SOFT_RESET_DB |
		      SOFT_RESET_PA |
		      SOFT_RESET_SC |
		      SOFT_RESET_SPI |
		      SOFT_RESET_SH |
		      SOFT_RESET_SX |
		      SOFT_RESET_TC |
		      SOFT_RESET_TA |
		      SOFT_RESET_VC |
		      SOFT_RESET_VGT);

	dev_info(rdev->dev, "  GRBM_SOFT_RESET=0x%08X\n", grbm_reset);
	WREG32(GRBM_SOFT_RESET, grbm_reset);
	(void)RREG32(GRBM_SOFT_RESET);
	udelay(50);
	WREG32(GRBM_SOFT_RESET, 0);
	(void)RREG32(GRBM_SOFT_RESET);

	/* reset all the system blocks */
	srbm_reset = SRBM_SOFT_RESET_ALL_MASK;

	dev_info(rdev->dev, "  SRBM_SOFT_RESET=0x%08X\n", srbm_reset);
	WREG32(SRBM_SOFT_RESET, srbm_reset);
	(void)RREG32(SRBM_SOFT_RESET);
	udelay(50);
	WREG32(SRBM_SOFT_RESET, 0);
	(void)RREG32(SRBM_SOFT_RESET);
	/* Wait a little for things to settle down */
	udelay(50);
	dev_info(rdev->dev, "  GRBM_STATUS=0x%08X\n",
		RREG32(GRBM_STATUS));
	dev_info(rdev->dev, "  GRBM_STATUS_SE0=0x%08X\n",
		RREG32(GRBM_STATUS_SE0));
	dev_info(rdev->dev, "  GRBM_STATUS_SE1=0x%08X\n",
		RREG32(GRBM_STATUS_SE1));
	dev_info(rdev->dev, "  SRBM_STATUS=0x%08X\n",
		RREG32(SRBM_STATUS));
	/* After reset we need to reinit the asic as GPU often endup in an
	 * incoherent state.
	 */
	atom_asic_init(rdev->mode_info.atom_context);
	evergreen_mc_resume(rdev, &save);
	return 0;
}

int evergreen_asic_reset(struct radeon_device *rdev)
{
	return evergreen_gpu_soft_reset(rdev);
}

static int evergreen_startup(struct radeon_device *rdev)
{
	int r;

	/* XXX until interrupts are supported */
	if (!rdev->me_fw || !rdev->pfp_fw /*|| !rdev->rlc_fw*/) {
		r = r600_init_microcode(rdev);
		if (r) {
			DRM_ERROR("Failed to load firmware!\n");
			return r;
		}
	}

	evergreen_mc_program(rdev);
	if (rdev->flags & RADEON_IS_AGP) {
		evergreen_agp_enable(rdev);
	} else {
		r = evergreen_pcie_gart_enable(rdev);
		if (r)
			return r;
	}
	evergreen_gpu_init(rdev);
#if 0
	if (!rdev->r600_blit.shader_obj) {
		r = r600_blit_init(rdev);
		if (r) {
			DRM_ERROR("radeon: failed blitter (%d).\n", r);
			return r;
		}
	}

	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (unlikely(r != 0))
		return r;
	r = radeon_bo_pin(rdev->r600_blit.shader_obj, RADEON_GEM_DOMAIN_VRAM,
			&rdev->r600_blit.shader_gpu_addr);
	radeon_bo_unreserve(rdev->r600_blit.shader_obj);
	if (r) {
		DRM_ERROR("failed to pin blit object %d\n", r);
		return r;
	}

	/* Enable IRQ */
	r = r600_irq_init(rdev);
	if (r) {
		DRM_ERROR("radeon: IH init failed (%d).\n", r);
		radeon_irq_kms_fini(rdev);
		return r;
	}
	r600_irq_set(rdev);
#endif

	r = radeon_ring_init(rdev, rdev->cp.ring_size);
	if (r)
		return r;
	r = evergreen_cp_load_microcode(rdev);
	if (r)
		return r;
	r = evergreen_cp_resume(rdev);
	if (r)
		return r;
	/* write back buffer are not vital so don't worry about failure */
	r600_wb_enable(rdev);

	return 0;
}

int evergreen_resume(struct radeon_device *rdev)
{
	int r;

	/* Do not reset GPU before posting, on rv770 hw unlike on r500 hw,
	 * posting will perform necessary task to bring back GPU into good
	 * shape.
	 */
	/* post card */
	atom_asic_init(rdev->mode_info.atom_context);
	/* Initialize clocks */
	r = radeon_clocks_init(rdev);
	if (r) {
		return r;
	}

	r = evergreen_startup(rdev);
	if (r) {
		DRM_ERROR("r600 startup failed on resume\n");
		return r;
	}

	r = r600_ib_test(rdev);
	if (r) {
		DRM_ERROR("radeon: failled testing IB (%d).\n", r);
		return r;
	}

	return r;

}

int evergreen_suspend(struct radeon_device *rdev)
{
#if 0
	int r;
#endif
	/* FIXME: we should wait for ring to be empty */
	r700_cp_stop(rdev);
	rdev->cp.ready = false;
	r600_wb_disable(rdev);

	evergreen_pcie_gart_disable(rdev);
#if 0
	/* unpin shaders bo */
	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (likely(r == 0)) {
		radeon_bo_unpin(rdev->r600_blit.shader_obj);
		radeon_bo_unreserve(rdev->r600_blit.shader_obj);
	}
#endif
	return 0;
}

static bool evergreen_card_posted(struct radeon_device *rdev)
{
	u32 reg;

	/* first check CRTCs */
	reg = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET) |
		RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET) |
		RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET) |
		RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET) |
		RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET) |
		RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET);
	if (reg & EVERGREEN_CRTC_MASTER_EN)
		return true;

	/* then check MEM_SIZE, in case the crtcs are off */
	if (RREG32(CONFIG_MEMSIZE))
		return true;

	return false;
}

/* Plan is to move initialization in that function and use
 * helper function so that radeon_device_init pretty much
 * do nothing more than calling asic specific function. This
 * should also allow to remove a bunch of callback function
 * like vram_info.
 */
int evergreen_init(struct radeon_device *rdev)
{
	int r;

	r = radeon_dummy_page_init(rdev);
	if (r)
		return r;
	/* This don't do much */
	r = radeon_gem_init(rdev);
	if (r)
		return r;
	/* Read BIOS */
	if (!radeon_get_bios(rdev)) {
		if (ASIC_IS_AVIVO(rdev))
			return -EINVAL;
	}
	/* Must be an ATOMBIOS */
	if (!rdev->is_atom_bios) {
		dev_err(rdev->dev, "Expecting atombios for R600 GPU\n");
		return -EINVAL;
	}
	r = radeon_atombios_init(rdev);
	if (r)
		return r;
	/* Post card if necessary */
	if (!evergreen_card_posted(rdev)) {
		if (!rdev->bios) {
			dev_err(rdev->dev, "Card not posted and no BIOS - ignoring\n");
			return -EINVAL;
		}
		DRM_INFO("GPU not posted. posting now...\n");
		atom_asic_init(rdev->mode_info.atom_context);
	}
	/* Initialize scratch registers */
	r600_scratch_init(rdev);
	/* Initialize surface registers */
	radeon_surface_init(rdev);
	/* Initialize clocks */
	radeon_get_clock_info(rdev->ddev);
	r = radeon_clocks_init(rdev);
	if (r)
		return r;
	/* Initialize power management */
	radeon_pm_init(rdev);
	/* Fence driver */
	r = radeon_fence_driver_init(rdev);
	if (r)
		return r;
	/* initialize AGP */
	if (rdev->flags & RADEON_IS_AGP) {
		r = radeon_agp_init(rdev);
		if (r)
			radeon_agp_disable(rdev);
	}
	/* initialize memory controller */
	r = evergreen_mc_init(rdev);
	if (r)
		return r;
	/* Memory manager */
	r = radeon_bo_init(rdev);
	if (r)
		return r;
#if 0
	r = radeon_irq_kms_init(rdev);
	if (r)
		return r;
#endif
	rdev->cp.ring_obj = NULL;
	r600_ring_init(rdev, 1024 * 1024);
#if 0
	rdev->ih.ring_obj = NULL;
	r600_ih_ring_init(rdev, 64 * 1024);
#endif
	r = r600_pcie_gart_init(rdev);
	if (r)
		return r;

	rdev->accel_working = false;
	r = evergreen_startup(rdev);
	if (r) {
		dev_err(rdev->dev, "disabling GPU acceleration\n");
		r700_cp_fini(rdev);
		r600_wb_fini(rdev);
#if 0
		r600_irq_fini(rdev);
		radeon_irq_kms_fini(rdev);
#endif
		evergreen_pcie_gart_fini(rdev);
		rdev->accel_working = false;
	}
	if (rdev->accel_working) {
		r = radeon_ib_pool_init(rdev);
		if (r) {
			DRM_ERROR("radeon: failed initializing IB pool (%d).\n", r);
			rdev->accel_working = false;
		}
		r = r600_ib_test(rdev);
		if (r) {
			DRM_ERROR("radeon: failed testing IB (%d).\n", r);
			rdev->accel_working = false;
		}
	}
	return 0;
}

void evergreen_fini(struct radeon_device *rdev)
{
	radeon_pm_fini(rdev);
	evergreen_suspend(rdev);
#if 0
	r600_blit_fini(rdev);
	r600_irq_fini(rdev);
	radeon_irq_kms_fini(rdev);
	radeon_ring_fini(rdev);
	r600_wb_fini(rdev);
#endif
	evergreen_pcie_gart_fini(rdev);
	radeon_gem_fini(rdev);
	radeon_fence_driver_fini(rdev);
	radeon_clocks_fini(rdev);
	radeon_agp_fini(rdev);
	radeon_bo_fini(rdev);
	radeon_atombios_fini(rdev);
	kfree(rdev->bios);
	rdev->bios = NULL;
	radeon_dummy_page_fini(rdev);
}
