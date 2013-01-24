/*
 * Provides code common for host and device side USB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * If either host side (ie. CONFIG_USB=y) or device side USB stack
 * (ie. CONFIG_USB_GADGET=y) is compiled in the kernel, this module is
 * compiled-in as well.  Otherwise, if either of the two stacks is
 * compiled as module, this file is compiled as module as well.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/ch9.h>

const char *usb_speed_string(enum usb_device_speed speed)
{
	static const char *const names[] = {
		[USB_SPEED_UNKNOWN] = "UNKNOWN",
		[USB_SPEED_LOW] = "low-speed",
		[USB_SPEED_FULL] = "full-speed",
		[USB_SPEED_HIGH] = "high-speed",
		[USB_SPEED_WIRELESS] = "wireless",
		[USB_SPEED_SUPER] = "super-speed",
	};

	if (speed < 0 || speed >= ARRAY_SIZE(names))
		speed = USB_SPEED_UNKNOWN;
	return names[speed];
}
EXPORT_SYMBOL_GPL(usb_speed_string);

const char *usb_state_string(enum usb_device_state state)
{
	static const char *const names[] = {
		[USB_STATE_NOTATTACHED] = "not attached",
		[USB_STATE_ATTACHED] = "attached",
		[USB_STATE_POWERED] = "powered",
		[USB_STATE_RECONNECTING] = "reconnecting",
		[USB_STATE_UNAUTHENTICATED] = "unauthenticated",
		[USB_STATE_DEFAULT] = "default",
		[USB_STATE_ADDRESS] = "addresssed",
		[USB_STATE_CONFIGURED] = "configured",
		[USB_STATE_SUSPENDED] = "suspended",
	};

	if (state < 0 || state >= ARRAY_SIZE(names))
		return "UNKNOWN";

	return names[state];
}
EXPORT_SYMBOL_GPL(usb_state_string);

MODULE_LICENSE("GPL");
