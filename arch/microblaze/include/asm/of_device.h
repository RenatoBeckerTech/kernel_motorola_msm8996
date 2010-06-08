/*
 * Copyright (C) 2007-2008 Michal Simek <monstr@monstr.eu>
 *
 * based on PowerPC of_device.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_OF_DEVICE_H
#define _ASM_MICROBLAZE_OF_DEVICE_H
#ifdef __KERNEL__

#include <linux/device.h>
#include <linux/of.h>

extern ssize_t of_device_get_modalias(struct of_device *ofdev,
					char *str, ssize_t len);

extern struct of_device *of_device_alloc(struct device_node *np,
					 const char *bus_id,
					 struct device *parent);

extern void of_device_make_bus_id(struct of_device *dev);

/* This is just here during the transition */
#include <linux/of_device.h>

#endif /* __KERNEL__ */
#endif /* _ASM_MICROBLAZE_OF_DEVICE_H */
