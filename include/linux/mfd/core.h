/*
 * drivers/mfd/mfd-core.h
 *
 * core MFD support
 * Copyright (c) 2006 Ian Molton
 * Copyright (c) 2007 Dmitry Baryshkov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef MFD_CORE_H
#define MFD_CORE_H

#include <linux/platform_device.h>

/*
 * This struct describes the MFD part ("cell").
 * After registration the copy of this structure will become the platform data
 * of the resulting platform_device
 */
struct mfd_cell {
	const char		*name;
	int			id;

	/* refcounting for multiple drivers to use a single cell */
	atomic_t		*usage_count;
	int			(*enable)(struct platform_device *dev);
	int			(*disable)(struct platform_device *dev);

	int			(*suspend)(struct platform_device *dev);
	int			(*resume)(struct platform_device *dev);

	/* mfd_data can be used to pass data to client drivers */
	void			*mfd_data;

	/*
	 * These resources can be specified relative to the parent device.
	 * For accessing hardware you should use resources from the platform dev
	 */
	int			num_resources;
	const struct resource	*resources;

	/* don't check for resource conflicts */
	bool			ignore_resource_conflicts;

	/*
	 * Disable runtime PM callbacks for this subdevice - see
	 * pm_runtime_no_callbacks().
	 */
	bool			pm_runtime_no_callbacks;
};

/*
 * Convenience functions for clients using shared cells.  Refcounting
 * happens automatically, with the cell's enable/disable callbacks
 * being called only when a device is first being enabled or no other
 * clients are making use of it.
 */
extern int mfd_cell_enable(struct platform_device *pdev);
extern int mfd_cell_disable(struct platform_device *pdev);

/*
 * "Clone" multiple platform devices for a single cell. This is to be used
 * for devices that have multiple users of a cell.  For example, if an mfd
 * driver wants the cell "foo" to be used by a GPIO driver, an MTD driver,
 * and a platform driver, the following bit of code would be use after first
 * calling mfd_add_devices():
 *
 * const char *fclones[] = { "foo-gpio", "foo-mtd" };
 * err = mfd_clone_cells("foo", fclones, ARRAY_SIZE(fclones));
 *
 * Each driver (MTD, GPIO, and platform driver) would then register
 * platform_drivers for "foo-mtd", "foo-gpio", and "foo", respectively.
 * The cell's .enable/.disable hooks should be used to deal with hardware
 * resource contention.
 */
extern int mfd_clone_cell(const char *cell, const char **clones,
		size_t n_clones);

/*
 * Given a platform device that's been created by mfd_add_devices(), fetch
 * the mfd_cell that created it.
 */
static inline const struct mfd_cell *mfd_get_cell(struct platform_device *pdev)
{
	return pdev->mfd_cell;
}

/*
 * Given a platform device that's been created by mfd_add_devices(), fetch
 * the .mfd_data entry from the mfd_cell that created it.
 * Otherwise just return the platform_data pointer.
 * This maintains compatibility with platform drivers whose devices aren't
 * created by the mfd layer, and expect platform_data to contain what would've
 * otherwise been in mfd_data.
 */
static inline void *mfd_get_data(struct platform_device *pdev)
{
	const struct mfd_cell *cell = mfd_get_cell(pdev);

	if (cell)
		return cell->mfd_data;
	else
		return pdev->dev.platform_data;
}

extern int mfd_add_devices(struct device *parent, int id,
			   struct mfd_cell *cells, int n_devs,
			   struct resource *mem_base,
			   int irq_base);

extern void mfd_remove_devices(struct device *parent);

#endif
