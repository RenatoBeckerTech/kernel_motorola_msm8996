/* linux/arch/arm/mach-s5pv210/pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5PV210 - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#include <mach/regs-irq.h>
#include <mach/regs-clock.h>

static struct sleep_save s5pv210_core_save[] = {
	/* Clock ETC */
	SAVE_ITEM(S5P_MDNIE_SEL),
};

static int s5pv210_cpu_suspend(unsigned long arg)
{
	unsigned long tmp;

	/* issue the standby signal into the pm unit. Note, we
	 * issue a write-buffer drain just in case */

	tmp = 0;

	asm("b 1f\n\t"
	    ".align 5\n\t"
	    "1:\n\t"
	    "mcr p15, 0, %0, c7, c10, 5\n\t"
	    "mcr p15, 0, %0, c7, c10, 4\n\t"
	    "wfi" : : "r" (tmp));

	pr_info("Failed to suspend the system\n");
	return 1; /* Aborting suspend */
}

static void s5pv210_pm_prepare(void)
{
	unsigned int tmp;

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), S5P_INFORM0);

	tmp = __raw_readl(S5P_SLEEP_CFG);
	tmp &= ~(S5P_SLEEP_CFG_OSC_EN | S5P_SLEEP_CFG_USBOSC_EN);
	__raw_writel(tmp, S5P_SLEEP_CFG);

	/* WFI for SLEEP mode configuration by SYSCON */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;
	tmp |= S5P_CFG_WFI_SLEEP;
	__raw_writel(tmp, S5P_PWR_CFG);

	/* SYSCON interrupt handling disable */
	tmp = __raw_readl(S5P_OTHERS);
	tmp |= S5P_OTHER_SYSC_INTOFF;
	__raw_writel(tmp, S5P_OTHERS);

	s3c_pm_do_save(s5pv210_core_save, ARRAY_SIZE(s5pv210_core_save));
}

static void s5pv210_pm_resume(void)
{
	u32 tmp;

	tmp = __raw_readl(S5P_OTHERS);
	tmp |= (S5P_OTHERS_RET_IO | S5P_OTHERS_RET_CF |\
		S5P_OTHERS_RET_MMC | S5P_OTHERS_RET_UART);
	__raw_writel(tmp , S5P_OTHERS);

	s3c_pm_do_restore_core(s5pv210_core_save, ARRAY_SIZE(s5pv210_core_save));
}

static struct syscore_ops s5pv210_pm_syscore_ops = {
	.resume		= s5pv210_pm_resume,
};

static __init int s5pv210_pm_syscore_init(void)
{
	register_syscore_ops(&s5pv210_pm_syscore_ops);

	pm_cpu_prep = s5pv210_pm_prepare;
	pm_cpu_sleep = s5pv210_cpu_suspend;

	return 0;
}
arch_initcall(s5pv210_pm_syscore_init);
