/*
 * arch/arm/mach-dove/include/mach/bridge-regs.h
 *
 * Mbus-L to Mbus Bridge Registers
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __ASM_ARCH_BRIDGE_REGS_H
#define __ASM_ARCH_BRIDGE_REGS_H

#include <mach/dove.h>

#define CPU_CONFIG		(BRIDGE_VIRT_BASE | 0x0000)

#define CPU_CONTROL		(BRIDGE_VIRT_BASE | 0x0104)
#define  CPU_CTRL_PCIE0_LINK	0x00000001
#define  CPU_RESET		0x00000002
#define  CPU_CTRL_PCIE1_LINK	0x00000008

#define RSTOUTn_MASK		(BRIDGE_VIRT_BASE | 0x0108)
#define  SOFT_RESET_OUT_EN	0x00000004

#define SYSTEM_SOFT_RESET	(BRIDGE_VIRT_BASE | 0x010c)
#define  SOFT_RESET		0x00000001

#define  BRIDGE_INT_TIMER1_CLR	(~0x0004)

#define IRQ_VIRT_BASE		(BRIDGE_VIRT_BASE | 0x0200)
#define IRQ_CAUSE_LOW_OFF	0x0000
#define IRQ_MASK_LOW_OFF	0x0004
#define FIQ_MASK_LOW_OFF	0x0008
#define ENDPOINT_MASK_LOW_OFF	0x000c
#define IRQ_CAUSE_HIGH_OFF	0x0010
#define IRQ_MASK_HIGH_OFF	0x0014
#define FIQ_MASK_HIGH_OFF	0x0018
#define ENDPOINT_MASK_HIGH_OFF	0x001c
#define PCIE_INTERRUPT_MASK_OFF	0x0020

#define IRQ_MASK_LOW		(IRQ_VIRT_BASE + IRQ_MASK_LOW_OFF)
#define FIQ_MASK_LOW		(IRQ_VIRT_BASE + FIQ_MASK_LOW_OFF)
#define ENDPOINT_MASK_LOW	(IRQ_VIRT_BASE + ENDPOINT_MASK_LOW_OFF)
#define IRQ_MASK_HIGH		(IRQ_VIRT_BASE + IRQ_MASK_HIGH_OFF)
#define FIQ_MASK_HIGH		(IRQ_VIRT_BASE + FIQ_MASK_HIGH_OFF)
#define ENDPOINT_MASK_HIGH	(IRQ_VIRT_BASE + ENDPOINT_MASK_HIGH_OFF)
#define PCIE_INTERRUPT_MASK	(IRQ_VIRT_BASE + PCIE_INTERRUPT_MASK_OFF)

#define POWER_MANAGEMENT	(BRIDGE_VIRT_BASE | 0x011c)

#define TIMER_VIRT_BASE		(BRIDGE_VIRT_BASE | 0x0300)

#endif
